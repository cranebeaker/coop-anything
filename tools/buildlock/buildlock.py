"""A lease lock for a shared build tree, so several agents can build in one folder without clobbering each other.

    python -I tools/buildlock/buildlock.py run --tree build/all --owner agent1 -- cmake --build build/all
    python -I tools/buildlock/buildlock.py status --tree build/all
    python -I tools/buildlock/buildlock.py break --tree build/all --only-if-stale

In Python:

    from buildlock import BuildLock
    with BuildLock("build/all.buildlock", owner="agent1", lease_s=120).acquire(wait_s=3600):
        run_the_build()

How it works (each rule has a test):
  * The lock is a folder created with mkdir, which is atomic: one creator wins. Inside it, lock.json names the owner, pid,
    the process start time, a random token, and the last heartbeat.
  * A lease, not a forever lock: while the holder runs, a background thread renews the heartbeat every lease_s / 3 seconds.
    Nobody else may take the lock while the heartbeat is fresh and the holder process runs.
  * Stale lock recovery: another agent may take the lock when the holder process is gone (checked by pid AND process start
    time, so a reused pid is not taken for the holder), when the heartbeat is older than lease_s, when the lock was held longer
    than max_hold_s, or when lock.json never appeared. A break is done under a short guard and only if the lock still carries
    the token that was judged stale, so a fresh lock is never broken by a slow competitor.
  * Waiting is first come, first served: a waiter writes a ticket and may only take a free lock when it holds the oldest live
    ticket. Tickets of dead processes are removed.
  * Only the holder (by token) can release. Releasing a lock that was already broken and taken by someone else does nothing.

Exit codes of `run`: the command's own exit code, 3 when the lock could not be taken within --wait-s, 2 for a usage error.
Standard library only; works on Windows, Linux and macOS.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path
from typing import Callable, Dict, List, Optional

LOCK_FILE = "lock.json"
GUARD_STALE_S = 30.0          # a break guard older than this is a leftover of a crashed process
NO_FILE_GRACE_S = 5.0         # a lock folder without lock.json is "being created" for this long, then stale
TICKET_STALE_S = 30.0         # a waiting ticket that its waiter has not touched for this long belongs to a waiter that is gone
DEFAULT_LEASE_S = 120.0
DEFAULT_MAX_HOLD_S = 7200.0


class LockTimeout(Exception):
    """The lock was not free within the wait time. The message names the current holder."""


# ------------------------------------------------------------------------------------------------ processes
def proc_stamp(pid: int) -> Optional[str]:
    """A string that identifies the running process `pid` (its start time), or None when no such process runs.
    "?" means the process runs but its start time cannot be read."""
    pid = int(pid)
    if pid <= 0:
        return None
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.OpenProcess.restype = wintypes.HANDLE
        k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        h = k32.OpenProcess(0x1000, False, pid)        # PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            return "?" if ctypes.get_last_error() == 5 else None      # access denied: it exists; anything else: it does not
        try:
            code = wintypes.DWORD()
            if k32.GetExitCodeProcess(h, ctypes.byref(code)) and code.value != 259:     # 259 = STILL_ACTIVE
                return None
            ft = [wintypes.FILETIME() for _ in range(4)]
            if k32.GetProcessTimes(h, *[ctypes.byref(x) for x in ft]):
                return str((ft[0].dwHighDateTime << 32) | ft[0].dwLowDateTime)
            return "?"
        finally:
            k32.CloseHandle(h)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return None
    except PermissionError:
        return "?"
    try:
        stat = Path(f"/proc/{pid}/stat").read_text()
        return stat.rsplit(")", 1)[1].split()[19]       # field 22: start time in clock ticks
    except (OSError, IndexError):
        return "?"


def stamp_differs(recorded, now_stamp) -> bool:
    """True when pid `recorded` was started at another time than now: the pid was reused by a different process."""
    return bool(recorded) and recorded != "?" and now_stamp != "?" and now_stamp != recorded


def lease_expired(info: Dict, now: float) -> bool:
    return now - float(info.get("heartbeat", 0)) > float(info.get("lease_s", DEFAULT_LEASE_S))


def max_hold_exceeded(info: Dict, now: float) -> bool:
    return now - float(info.get("acquired", now)) > float(info.get("max_hold_s", DEFAULT_MAX_HOLD_S))


def holder_problem(info: Dict, now: float) -> Optional[str]:
    """Why the lock described by `info` may be taken away from its holder, or None when it must be respected."""
    pid = info.get("pid")
    if not isinstance(pid, int):
        return "the lock file has no pid"
    stamp = proc_stamp(pid)
    if stamp is None:
        return f"the holder process (pid {pid}) is gone"
    if stamp_differs(info.get("pid_stamp"), stamp):
        return f"pid {pid} now belongs to a different process (reused)"
    if lease_expired(info, now):
        return f"no heartbeat for {now - float(info.get('heartbeat', 0)):.0f} s (the lease is {float(info.get('lease_s', DEFAULT_LEASE_S)):.0f} s)"
    if max_hold_exceeded(info, now):
        return f"held for {now - float(info.get('acquired', now)):.0f} s, longer than the {float(info.get('max_hold_s', DEFAULT_MAX_HOLD_S)):.0f} s limit"
    return None


# ------------------------------------------------------------------------------------------------ the lock
def default_owner() -> str:
    return os.environ.get("BUILDLOCK_OWNER") or f"agent-{os.getpid()}"


class BuildLock:
    """One lock folder. Use as a context manager: `with BuildLock(path).acquire(wait_s=60): ...`."""

    def __init__(self, path, owner: Optional[str] = None, label: str = "", lease_s: float = DEFAULT_LEASE_S,
                 max_hold_s: float = DEFAULT_MAX_HOLD_S, poll_s: float = 1.0, log: Optional[Callable[[str], None]] = None):
        self.path = Path(path)
        self.guard = Path(str(path) + ".breaking")
        self.queue = Path(str(path) + ".queue")
        self.owner = owner or default_owner()
        self.label = label
        self.lease_s = float(lease_s)
        self.max_hold_s = float(max_hold_s)
        self.poll_s = float(poll_s)
        self.log = log or (lambda s: None)
        self.token = None
        self.recovered = None            # the reason, if the lock was taken from a stale holder
        self.lost = False                # set by the heartbeat when the lock was broken under us
        self._beat = None
        self._stop = threading.Event()

    # -------------------------------------------------------------- reading
    def read(self) -> Optional[Dict]:
        """The lock info, {} when the folder exists without a readable lock.json, None when the lock is free."""
        if not self.path.is_dir():
            return None
        try:
            return json.loads((self.path / LOCK_FILE).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}

    def problem(self, info: Dict) -> Optional[str]:
        """Why `info` (from read()) is stale, or None."""
        if not info:
            try:
                age = time.time() - self.path.stat().st_mtime
            except OSError:
                return None
            return None if age < NO_FILE_GRACE_S else "the lock folder has no readable lock.json (its creator crashed)"
        return holder_problem(info, time.time())

    def status(self) -> Dict:
        info = self.read()
        if info is None:
            return {"state": "free", "path": str(self.path)}
        reason = self.problem(info)
        out = {"state": "stale" if reason else "held", "path": str(self.path), "owner": info.get("owner"), "pid": info.get("pid"),
               "label": info.get("label"), "held_s": round(time.time() - info["acquired"], 1) if info.get("acquired") else None,
               "heartbeat_age_s": round(time.time() - info["heartbeat"], 1) if info.get("heartbeat") else None}
        if reason:
            out["reason"] = reason
        return out

    def describe(self, info: Optional[Dict]) -> str:
        if not info:
            return "an unknown holder"
        held = time.time() - float(info.get("acquired", time.time()))
        lbl = f" ({info['label']})" if info.get("label") else ""
        return f"{info.get('owner', '?')}{lbl}, pid {info.get('pid', '?')}, for {held:.0f} s"

    # -------------------------------------------------------------- writing
    def _write_info(self, token: str, acquired: float) -> None:
        info = {"owner": self.owner, "label": self.label, "pid": os.getpid(), "pid_stamp": proc_stamp(os.getpid()),
                "token": token, "acquired": acquired, "heartbeat": time.time(), "lease_s": self.lease_s,
                "max_hold_s": self.max_hold_s}
        tmp = self.path / (LOCK_FILE + "." + token[:8])
        tmp.write_text(json.dumps(info), encoding="utf-8")
        os.replace(tmp, self.path / LOCK_FILE)

    def _try_create(self) -> bool:
        """The atomic step: mkdir. True when this call created the lock."""
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.mkdir(self.path)
        except FileExistsError:
            return False
        token = uuid.uuid4().hex
        try:
            self._write_info(token, time.time())
        except OSError:
            shutil.rmtree(self.path, ignore_errors=True)
            return False
        self.token = token
        return True

    def break_if_stale(self, judged_token: Optional[str]) -> bool:
        """Remove a lock judged stale, but only while it still carries the token that was judged (a fresh lock that a
        faster competitor created in between is never removed). True when a lock was removed."""
        if not self._take_guard():
            return False
        try:
            info = self.read()
            if info is None:
                return False
            if (info.get("token") if info else None) != judged_token:
                return False
            dead = self.path.with_name(self.path.name + ".stale-" + uuid.uuid4().hex[:8])
            try:
                os.rename(self.path, dead)
            except OSError:
                return False
            shutil.rmtree(dead, ignore_errors=True)
            return True
        finally:
            self._drop_guard()

    def _take_guard(self) -> bool:
        try:
            os.mkdir(self.guard)
            return True
        except FileExistsError:
            try:
                if time.time() - self.guard.stat().st_mtime > GUARD_STALE_S:
                    shutil.rmtree(self.guard, ignore_errors=True)
            except OSError:
                pass
            return False
        except OSError:
            return False

    def _drop_guard(self) -> None:
        shutil.rmtree(self.guard, ignore_errors=True)

    # -------------------------------------------------------------- the queue
    def _ticket_files(self) -> List[Path]:
        try:
            return sorted(self.queue.glob("*.json"))
        except OSError:
            return []

    def _live_tickets(self) -> List[Path]:
        live = []
        for f in self._ticket_files():
            try:
                t = json.loads(f.read_text(encoding="utf-8"))
                idle = time.time() - f.stat().st_mtime
            except (OSError, ValueError):
                continue
            if proc_stamp(int(t.get("pid", 0))) is None or idle > TICKET_STALE_S:
                self._remove_ticket(f, tries=3)
                continue
            live.append(f)
        return live

    @staticmethod
    def _remove_ticket(f: Path, tries: int = 100) -> None:
        """Delete a ticket file. On Windows a file cannot be deleted while another thread or process is reading it, so try again."""
        for _ in range(tries):
            try:
                f.unlink()
                return
            except FileNotFoundError:
                return
            except OSError:
                time.sleep(0.02)

    def _first_in_line(self, mine: Optional[Path]) -> bool:
        live = self._live_tickets()
        if mine is None:
            return not live
        return bool(live) and live[0].name == mine.name

    def _make_ticket(self) -> Path:
        self.queue.mkdir(parents=True, exist_ok=True)
        name = f"{time.time_ns():020d}-{uuid.uuid4().hex[:6]}.json"
        f = self.queue / name
        f.write_text(json.dumps({"owner": self.owner, "pid": os.getpid()}), encoding="utf-8")
        return f

    # -------------------------------------------------------------- acquire / release
    def acquire(self, wait_s: float = 0.0) -> "BuildLock":
        """Take the lock, waiting up to wait_s seconds. Raises LockTimeout. Returns self."""
        deadline = time.monotonic() + max(0.0, wait_s)
        ticket = None
        last_note = 0.0
        try:
            while True:
                if self._first_in_line(ticket):
                    if self._try_create():
                        break
                    info = self.read()
                    if info is not None:
                        reason = self.problem(info)
                        if reason and self.break_if_stale(info.get("token") if info else None):
                            self.recovered = reason
                            self.log(f"took over a stale lock: {reason}")
                            continue
                if time.monotonic() >= deadline:
                    raise LockTimeout(f"build lock {self.path} is held by {self.describe(self.read())}; waited {wait_s:.0f} s")
                if ticket is None and wait_s > 0:
                    ticket = self._make_ticket()
                elif ticket is not None:
                    try:
                        os.utime(ticket, None)           # "still waiting": a ticket nobody touches for TICKET_STALE_S is dropped
                    except OSError:
                        pass
                if time.monotonic() - last_note > 60:
                    last_note = time.monotonic()
                    self.log(f"waiting for the build lock, held by {self.describe(self.read())}")
                time.sleep(min(self.poll_s, max(0.01, deadline - time.monotonic())))
        finally:
            if ticket is not None:
                self._remove_ticket(ticket)
        self.lost = False
        self._start_heartbeat()
        return self

    def _start_heartbeat(self) -> None:
        self._stop.clear()
        interval = max(0.05, self.lease_s / 3.0)

        def beat():
            while not self._stop.wait(interval):
                info = self.read()
                if not info or info.get("token") != self.token:
                    self.lost = True
                    self.log("the build lock was taken away while it was held (the heartbeat was too late)")
                    return
                try:
                    self._write_info(self.token, float(info.get("acquired", time.time())))
                except OSError:
                    pass
        self._beat = threading.Thread(target=beat, daemon=True)
        self._beat.start()

    def release(self) -> bool:
        """Free the lock if this object still holds it. Returns True when the lock was released."""
        self._stop.set()
        if self._beat is not None:
            self._beat.join(timeout=5)
            self._beat = None
        if not self.token:
            return False
        info = self.read()
        mine = bool(info) and info.get("token") == self.token
        self.token = None
        if not mine:
            return False
        self._remove_folder()
        return True

    def _remove_folder(self) -> None:
        """Take the lock folder away. On Windows a folder cannot be renamed or deleted while someone has lock.json open (a waiter
        reading it), so try for a while. Renaming first frees the lock at once even if the old files cannot be deleted yet."""
        for _ in range(100):
            dead = self.path.with_name(self.path.name + ".released-" + uuid.uuid4().hex[:8])
            try:
                os.rename(self.path, dead)
            except FileNotFoundError:
                return
            except OSError:
                time.sleep(0.02)
                continue
            shutil.rmtree(dead, ignore_errors=True)
            return
        shutil.rmtree(self.path, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.release()
        return False


def lock_path_for(tree: str, lock: Optional[str] = None) -> Path:
    """Default lock folder: next to the build tree, so wiping the tree does not wipe the lock."""
    if lock:
        return Path(lock)
    t = Path(tree)
    return t.with_name(t.name + ".buildlock")


# ------------------------------------------------------------------------------------------------ command line
def cmd_run(a, command: List[str]) -> int:
    lock = BuildLock(lock_path_for(a.tree, a.lock), a.owner, a.label, a.lease_s, a.max_hold_s, a.poll_s,
                     log=(lambda s: None) if a.quiet else (lambda s: print("[buildlock] " + s, file=sys.stderr, flush=True)))
    t0 = time.time()
    try:
        lock.acquire(a.wait_s)
    except LockTimeout as e:
        print(f"[buildlock] {e}", file=sys.stderr)
        return 3
    t1 = time.time()
    try:
        if not a.quiet:
            print(f"[buildlock] lock taken after {t1 - t0:.1f} s" + (f" (stale lock recovered: {lock.recovered})" if lock.recovered else ""),
                  file=sys.stderr, flush=True)
        rc = subprocess.run(command).returncode
    finally:
        lock.release()
        if not a.quiet:
            print(f"[buildlock] lock released after {time.time() - t1:.1f} s", file=sys.stderr, flush=True)
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--tree", required=True, help="the build tree; the lock folder is <tree>.buildlock")
        p.add_argument("--lock", help="use this lock folder instead")

    p = sub.add_parser("run", help="take the lock, run a command, release")
    common(p)
    p.add_argument("--owner", default=None, help="a name for the holder (default: BUILDLOCK_OWNER or agent-<pid>)")
    p.add_argument("--label", default="")
    p.add_argument("--wait-s", type=float, default=3600.0)
    p.add_argument("--lease-s", type=float, default=DEFAULT_LEASE_S)
    p.add_argument("--max-hold-s", type=float, default=DEFAULT_MAX_HOLD_S)
    p.add_argument("--poll-s", type=float, default=1.0)
    p.add_argument("--quiet", action="store_true")
    p.add_argument("command", nargs=argparse.REMAINDER)
    p = sub.add_parser("status", help="who holds the lock")
    common(p)
    p.add_argument("--json", action="store_true")
    p = sub.add_parser("break", help="remove the lock by hand")
    common(p)
    p.add_argument("--only-if-stale", action="store_true", help="remove it only when the holder is gone or the lease ran out")
    a = ap.parse_args(argv)
    if a.cmd == "run":
        command = [c for c in a.command if c != "--"] if a.command and a.command[0] == "--" else a.command
        if not command:
            print("run needs a command after --", file=sys.stderr)
            return 2
        return cmd_run(a, command)
    lock = BuildLock(lock_path_for(a.tree, a.lock))
    if a.cmd == "status":
        st = lock.status()
        print(json.dumps(st) if a.json else ", ".join(f"{k}={v}" for k, v in st.items()))
        return 0
    info = lock.read()
    if info is None:
        print("the lock is free")
        return 0
    reason = lock.problem(info)
    if a.only_if_stale and not reason:
        print(f"the lock is held by {lock.describe(info)} and is not stale; left alone")
        return 1
    ok = lock.break_if_stale(info.get("token") if info else None) if reason else (shutil.rmtree(lock.path, ignore_errors=True) or True)
    print("lock removed" + (f" ({reason})" if reason else "") if ok else "could not remove the lock")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
