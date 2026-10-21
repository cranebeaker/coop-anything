"""Pool client (standard library only). This is the lease API test scripts use.

    sys.path.insert(0, "tools/gamepool"); from gplib.client import lease
    with lease("my-test", n=2, ttl_s=900, timeout_s=600) as L:
        L.instances            # [0, 1]  lowest id first (host first)
        L.check(0)             # raises PoolError if instance 0 was restarted or lost under this lease
        L.dirty(0, "loaded a foreign save")   # this instance is recycled on release
        L.renew()              # extend the ttl during a long test
    # leaving the block releases the lease (dirty if the block raised)

An instance may be restarted under you (crash, failed health check). L.check() raises PoolError when that
happened, so a test never talks to a fresh game that has lost its state. How your test talks to the game
(a bridge, a socket, files in the instance folder) is up to the test; the lease only says which instances
are yours.
"""
from __future__ import annotations

import contextlib
import json
import os
import socket
from typing import Any, Dict, List, Optional

from .config import state_dir


class PoolError(Exception):
    pass


class PoolBusy(PoolError):
    pass


class PoolNotRunning(PoolError):
    pass


def pool_dir():
    return state_dir() / "pool"


def _info() -> Dict[str, Any]:
    try:
        return json.loads((pool_dir() / "pool.json").read_text())
    except (OSError, ValueError) as e:
        raise PoolNotRunning("pool is not running (no pool.json); start it with `pool up`") from e


def request(op: str, sock_timeout_s: float = 30.0, **kw: Any) -> Dict[str, Any]:
    info = _info()
    try:
        s = socket.create_connection(("127.0.0.1", info["port"]), timeout=5)
    except OSError as e:
        raise PoolNotRunning(f"pool daemon not answering on port {info['port']}: {e}") from e
    with s:
        s.settimeout(sock_timeout_s)
        s.sendall((json.dumps({"op": op, "token": info["token"], **kw}) + "\n").encode())
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(65536)
            if not chunk:
                raise PoolError("pool closed the connection")
            buf += chunk
    rep = json.loads(buf.split(b"\n", 1)[0])
    if not rep.get("ok"):
        raise (PoolBusy if rep.get("kind") == "busy" else PoolError)(rep.get("error", "unknown error"))
    return rep


def status() -> Dict[str, Any]:
    return request("status")


class Lease:
    def __init__(self, data: Dict[str, Any]):
        self.id: str = data["id"]
        self.owner: str = data["owner"]
        self.instances: List[int] = list(data["instances"])
        self.ttl_s: float = data["ttl_s"]
        self._gens = {int(k): v for k, v in data["gens"].items()}
        self.released = False

    def check(self, i: Optional[int] = None) -> Dict[str, Any]:
        """The instance's current status row; raises PoolError if it was restarted or lost under this lease."""
        i = self.instances[0] if i is None else i
        if i not in self.instances:
            raise PoolError(f"instance {i} is not part of lease {self.id} ({self.instances})")
        cur = {x["id"]: x for x in status()["instances"]}[i]
        if cur["gen"] != self._gens[i] or cur["state"] != "leased":
            raise PoolError(f"instance {i} was restarted or lost under lease {self.id} "
                            f"(gen {self._gens[i]} -> {cur['gen']}, {cur['state']})")
        return cur

    @property
    def instance(self) -> int:
        return self.instances[0]

    @property
    def generation(self) -> int:
        return self._gens[self.instances[0]]

    def dirty(self, i: int, reason: str) -> None:
        request("dirty", id=self.id, instance=i, reason=reason)

    def renew(self, ttl_s: Optional[float] = None) -> bool:
        return request("renew", id=self.id, ttl_s=ttl_s)["renewed"]

    def release(self, dirty: bool = False, reason: str = "") -> None:
        if self.released:
            return
        self.released = True
        request("release", id=self.id, dirty=dirty, reason=reason)


def acquire(owner: str, n: int = 1, ttl_s: float = 900, timeout_s: float = 600) -> Lease:
    return Lease(request("lease", sock_timeout_s=timeout_s + 60, owner=owner, n=n, ttl_s=ttl_s, timeout_s=timeout_s)["lease"])


@contextlib.contextmanager
def lease(owner: str, n: int = 1, ttl_s: float = 900, timeout_s: float = 600):
    L = acquire(owner, n, ttl_s, timeout_s)
    try:
        yield L
    except BaseException:
        L.release(dirty=True, reason="exception in lessee")
        raise
    else:
        L.release()
