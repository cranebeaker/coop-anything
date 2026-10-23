"""Instance leasing by OS file lock, for runs that do not use a game slot.

One lock file per instance: <lock root>/locks/inst-<i>.lock. The lock (msvcrt.locking on byte 0) is released
by Windows when the holder dies, so there are no stale locks. The holder writes a JSON description after
byte 16, readable by others (only byte 0 is locked).

    with lease(root, n=2, pool=2, job="smoke") as inst:    # inst == [0, 1]; waits until two are free
        ...
"""
from __future__ import annotations

import contextlib
import json
import os
import random
import time
from pathlib import Path
from typing import Iterator, List, Optional

import msvcrt


class LeaseTimeout(Exception):
    pass


def lock_dir(root: Path) -> Path:
    d = Path(root) / "locks"
    d.mkdir(parents=True, exist_ok=True)
    return d


class _Lock:
    def __init__(self, path: Path):
        self.path = path
        self.f = None

    def try_acquire(self, info: dict) -> bool:
        f = open(self.path, "a+b")
        try:
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            f.close()
            return False
        self.f = f
        f.seek(16)
        f.truncate(16)
        f.write(json.dumps(info).encode())
        f.flush()
        return True

    def release(self) -> None:
        if self.f is None:
            return
        try:
            self.f.seek(0)
            msvcrt.locking(self.f.fileno(), msvcrt.LK_UNLCK, 1)
        finally:
            self.f.close()
            self.f = None


def holder(root: Path, i: int) -> Optional[dict]:
    """Describe the current holder of instance i, or None if it is free."""
    p = lock_dir(root) / f"inst-{i}.lock"
    if not p.exists():
        return None
    lk = _Lock(p)
    if lk.try_acquire({"probe": True}):
        lk.release()
        return None
    try:
        with open(p, "rb") as f:
            f.seek(16)          # byte 0 is locked by the holder; read past it
            data = f.read()
        return json.loads(data.decode()) if data else {"unknown": True}
    except (OSError, ValueError):
        return {"unknown": True}


def acquire(root: Path, n: int = 1, *, pool: int = 2, job: str = "", timeout_s: float = 600.0,
            poll_s: float = 0.25) -> List[_Lock]:
    """All or nothing: n instances out of range(pool). Backs off with jitter when contended."""
    if n > pool:
        raise ValueError(f"asked for {n} instances but pool is {pool}")
    deadline = time.monotonic() + timeout_s
    ld = lock_dir(root)
    while True:
        got: List[_Lock] = []
        for i in range(pool):
            lk = _Lock(ld / f"inst-{i}.lock")
            if lk.try_acquire({"job": job, "pid": os.getpid(), "instance": i, "since": time.time()}):
                got.append(lk)
                if len(got) == n:
                    return got
        for lk in got:          # not enough free: let go of the partial set
            lk.release()
        if time.monotonic() >= deadline:
            raise LeaseTimeout(f"could not lease {n} of {pool} instances within {timeout_s}s")
        time.sleep(poll_s + random.random() * poll_s)


def instance_ids(locks: List[_Lock]) -> List[int]:
    return [int(l.path.stem.split("-")[1]) for l in locks]


@contextlib.contextmanager
def lease(root: Path, n: int = 1, *, pool: int = 2, job: str = "", timeout_s: float = 600.0) -> Iterator[List[int]]:
    locks = acquire(root, n, pool=pool, job=job, timeout_s=timeout_s)
    try:
        yield instance_ids(locks)
    finally:
        for lk in locks:
            lk.release()
