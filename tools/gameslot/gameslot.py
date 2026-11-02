"""Game slots: a lease lock for running game copies on one PC.

Several agents (or scripts) may want to run copies of the same game on one machine. Each one takes a
slot first, so two of them never start the game at the same moment, never share an instance index, and
never push the PC out of memory.

    python -I tools/gameslot/gameslot.py acquire --owner agent1 [--slots 1] [--ttl 900] [--wait 600]
    python -I tools/gameslot/gameslot.py register-pid --owner agent1 --pid 1234
    python -I tools/gameslot/gameslot.py release --owner agent1
    python -I tools/gameslot/gameslot.py status

acquire     takes --slots slots (default 1; --exclusive takes all of them). The OK line ends with
            `SLOTS 0,1`: those indices are yours. Derive instance ids, ports and pipe names from them so
            concurrent owners never collide. Asking again with the same owner renews the lease.
register-pid  records a game process you started: its pid, image name and creation time.
release     frees every slot of the owner.
status      prints JSON: every slot, its lease, free or not.

Exit codes: 0 ok, 2 bad use, 3 busy (the wait ran out), 4 not the owner.

Rules
  * A lease expires after --ttl seconds. An expired lease may be taken over. When that happens, ONLY the
    processes the old owner registered are killed, and only if the pid still has the same image name and
    creation time (so a reused pid is never killed). Nothing here ever kills "every process of that name":
    the user may be playing the game on the same PC.
  * Waiters queue first in, first out. A waiter that polled in the last 60 seconds blocks newer
    acquirers, so a request for many slots is not starved by a stream of small ones.
  * RAM guard: a request that would start a second game while free RAM is below min_free_gb is refused
    (the first game and lease renewals are never refused).
  * All state changes happen under one short-lived lock file, so two processes never both win a slot.

Config (optional): <state dir>/gameslot_config.json  {"slots": 2, "min_free_gb": 3.0, "process_name": "Game.exe"}
process_name is only used by `status` to list running game pids.
State directory: env GAMESLOT_DIR, else <repo>/local/gameslot (git ignores local/).
Python standard library only. Windows is the supported platform.
"""
import argparse
import contextlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

STATE = Path(os.environ.get("GAMESLOT_DIR") or Path(__file__).resolve().parents[2] / "local" / "gameslot")
SLOTS_DIR = STATE / "slots"
WAIT_DIR = STATE / "waiters"
LOCK = STATE / "state.lock"
CONFIG = STATE / "gameslot_config.json"
DEFAULT_SLOTS = 2
DEFAULT_MIN_FREE_GB = 3.0
WAITER_STALE_S = 60
LOCK_TIMEOUT_S = 10.0
LOCK_STALE_S = 30.0


class LockTimeout(RuntimeError):
    pass


# ---------------------------------------------------------------------------------------------- config
def _config():
    try:
        d = json.loads(CONFIG.read_text())
        return d if isinstance(d, dict) else {}
    except (OSError, ValueError):
        return {}


def max_slots():
    try:
        return max(1, int(_config()["slots"]))
    except (KeyError, TypeError, ValueError):
        return DEFAULT_SLOTS


def min_free_gb():
    try:
        return float(_config()["min_free_gb"])
    except (KeyError, TypeError, ValueError):
        return DEFAULT_MIN_FREE_GB


def process_name():
    return str(_config().get("process_name") or "")


# ---------------------------------------------------------------------------------------------- OS layer
def avail_gb():
    """Free physical RAM in GB, or None when it cannot be read (then the RAM guard does not block)."""
    if os.name != "nt":
        try:
            return os.sysconf("SC_AVPHYS_PAGES") * os.sysconf("SC_PAGE_SIZE") / 2**30
        except (ValueError, OSError, AttributeError):
            return None
    import ctypes

    class MS(ctypes.Structure):
        _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong), ("ullTotalPhys", ctypes.c_ulonglong),
                    ("ullAvailPhys", ctypes.c_ulonglong), ("a", ctypes.c_ulonglong), ("b", ctypes.c_ulonglong),
                    ("c", ctypes.c_ulonglong), ("d", ctypes.c_ulonglong), ("e", ctypes.c_ulonglong)]
    m = MS()
    m.dwLength = ctypes.sizeof(m)
    ctypes.WinDLL("kernel32").GlobalMemoryStatusEx(ctypes.byref(m))
    return m.ullAvailPhys / 2**30


def process_info(pid):
    """{"name": lowercase image file name, "created": creation time} of a running pid, else None."""
    if os.name != "nt":
        return None
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    k32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    k32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    h = k32.OpenProcess(0x1000, False, int(pid))   # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return None
    try:
        code = wintypes.DWORD()
        k32.GetExitCodeProcess(h, ctypes.byref(code))
        if code.value != 259:                       # not STILL_ACTIVE
            return None
        buf = ctypes.create_unicode_buffer(1024)
        size = wintypes.DWORD(1024)
        name = ""
        if k32.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(size)):
            name = os.path.basename(buf.value).lower()
        ft = [wintypes.FILETIME() for _ in range(4)]
        k32.GetProcessTimes(h, *[ctypes.byref(f) for f in ft])
        return {"name": name, "created": (ft[0].dwHighDateTime << 32) | ft[0].dwLowDateTime}
    finally:
        k32.CloseHandle(h)


def pids_named(name):
    """Pids of running processes with this image name (used only by `status`)."""
    if not name or os.name != "nt":
        return []
    out = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    pids = []
    for ln in out.splitlines():
        parts = [p.strip('"') for p in ln.split('","')]
        if len(parts) > 1 and parts[0].strip('"').lower() == name.lower():
            try:
                pids.append(int(parts[1]))
            except ValueError:
                pass
    return pids


def kill_pid(pid):
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(int(pid)), "/F", "/T"], capture_output=True)
    else:
        import signal
        try:
            os.kill(int(pid), signal.SIGKILL)
        except OSError:
            pass


# ---------------------------------------------------------------------------------------------- state lock
def try_lock(timeout_s):
    """Create the lock file exclusively. Returns True on success. A lock file older than LOCK_STALE_S is broken."""
    STATE.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + timeout_s
    while True:
        try:
            fd = os.open(str(LOCK), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, str(os.getpid()).encode())
            os.close(fd)
            return True
        except FileExistsError:
            try:
                if time.time() - LOCK.stat().st_mtime > LOCK_STALE_S:
                    LOCK.unlink()
                    continue
            except OSError:
                continue
        except PermissionError:
            pass                       # another process is deleting it right now
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.02)


def unlock():
    try:
        LOCK.unlink()
    except OSError:
        pass


@contextlib.contextmanager
def state_lock(timeout_s=None):
    if not try_lock(LOCK_TIMEOUT_S if timeout_s is None else timeout_s):
        raise LockTimeout(f"could not take {LOCK} (is another gameslot call stuck?)")
    try:
        yield
    finally:
        unlock()


# ---------------------------------------------------------------------------------------------- leases
def _read(p):
    try:
        return json.loads(Path(p).read_text())
    except (OSError, ValueError):
        return None


def _write(p, data):
    p = Path(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(f".{os.getpid()}.tmp")
    tmp.write_text(json.dumps(data))
    os.replace(tmp, p)


def _slot_path(i):
    return SLOTS_DIR / f"slot-{i}.json"


def _live(lease, now):
    return bool(lease) and lease.get("expires", 0) > now


def kill_registered(procs):
    """Kill only recorded processes that are still the same process (same image name and creation time)."""
    for rec in procs:
        info = process_info(rec.get("pid", 0))
        if info and info["created"] == rec.get("created") and info["name"] == rec.get("name"):
            kill_pid(rec["pid"])
            print("killed stale pid %d of an expired lease" % rec["pid"])


def _try_slot(i, owner, ttl, now):
    """Take slot i if free, expired or already ours. Caller holds the state lock."""
    cur = _read(_slot_path(i))
    if _live(cur, now) and cur.get("owner") != owner:
        return False
    mine = bool(cur and cur.get("owner") == owner)
    if cur and not mine:                     # expired foreign lease: kill only what it registered
        kill_registered(cur.get("procs", []))
    _write(_slot_path(i), {"owner": owner, "slot": i, "since": cur["since"] if mine else now, "expires": now + ttl,
                           "procs": cur.get("procs", []) if mine else []})
    return True


def _give_back(indices):
    for i in indices:
        _slot_path(i).unlink(missing_ok=True)


def _waiters(now, exclude_owner):
    out = []
    if WAIT_DIR.exists():
        for f in WAIT_DIR.glob("*.json"):
            w = _read(f)
            if w and now - w.get("polled", 0) < WAITER_STALE_S and w.get("owner") != exclude_owner:
                out.append(w)
    return out


def _mine(owner, now):
    return [i for i in range(max_slots()) if _live(_read(_slot_path(i)), now) and _read(_slot_path(i))["owner"] == owner]


def try_acquire(owner, ttl, want, ticket, now=None):
    """-> (ok, slots, info). want=None means every slot (exclusive)."""
    now = now or time.time()
    n = max_slots()
    need = n if want is None else min(want, n)
    with state_lock():
        older = [w for w in _waiters(now, owner) if w["ticket"] < ticket]
        if older:
            return False, [], "queue: %s is ahead" % older[0]["owner"]
        have = _mine(owner, now)
        got = list(have)
        any_live = any(_live(_read(_slot_path(i)), now) for i in range(n))
        if len(have) < need and any_live:    # a second game needs RAM headroom; the first game and renewals never do
            free = avail_gb()
            if free is not None and free < min_free_gb():
                return False, [], "low memory: %.1f GB free < %.1f GB required for a new slot" % (free, min_free_gb())
        for i in range(n):
            if len(got) >= need:
                break
            if i not in got and _try_slot(i, owner, ttl, now):
                got.append(i)
        for i in have:                       # renew what we already hold
            _try_slot(i, owner, ttl, now)
        if len(got) >= need:
            return True, sorted(got)[:max(need, len(have))], ""
        _give_back(set(got) - set(have))     # all or nothing: give back what we just took
        holders = sorted({(_read(_slot_path(i)) or {}).get("owner", "?") for i in range(n)
                          if _live(_read(_slot_path(i)), now)})
        return False, [], "held by %s" % ",".join(holders)


def release(owner):
    freed = 0
    with state_lock():
        for i in range(max_slots() + 8):
            c = _read(_slot_path(i))
            if c and c.get("owner") == owner:
                _slot_path(i).unlink(missing_ok=True)
                freed += 1
    return freed


def register_pid(owner, pid):
    """Record pid (with image name and creation time) on every live lease of owner. Returns leases updated."""
    info = process_info(pid)
    rec = {"pid": int(pid), "created": info["created"] if info else 0, "name": info["name"] if info else ""}
    n = 0
    with state_lock():
        for i in range(max_slots() + 8):
            c = _read(_slot_path(i))
            if c and c.get("owner") == owner and _live(c, time.time()):
                c["procs"] = [p for p in c.get("procs", []) if p.get("pid") != rec["pid"]] + [rec]
                _write(_slot_path(i), c)
                n += 1
    return n


def status(now=None):
    now = now or time.time()
    slots = []
    for i in range(max_slots()):
        c = _read(_slot_path(i))
        slots.append({"slot": i, "lease": c, "free": not _live(c, now)})
    return {"max_slots": max_slots(), "min_free_gb": min_free_gb(), "slots": slots,
            "running": pids_named(process_name())}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["acquire", "release", "status", "register-pid"])
    ap.add_argument("--owner")
    ap.add_argument("--pid", type=int)
    ap.add_argument("--ttl", type=int, default=900)
    ap.add_argument("--wait", type=int, default=0)
    ap.add_argument("--slots", type=int, default=1)
    ap.add_argument("--exclusive", action="store_true", help="take every slot")
    a = ap.parse_args(argv)
    if a.cmd == "status":
        print(json.dumps(status()))
        return 0
    if not a.owner:
        ap.error("--owner required")
    if a.cmd == "register-pid":
        if not a.pid:
            ap.error("--pid required")
        n = register_pid(a.owner, a.pid)
        print("REGISTERED %d on %d lease(s)" % (a.pid, n))
        return 0 if n else 4
    if a.cmd == "release":
        n = release(a.owner)
        WAIT_DIR.mkdir(parents=True, exist_ok=True)
        (WAIT_DIR / f"{a.owner}.json").unlink(missing_ok=True)
        if n:
            print("RELEASED")
            return 0
        print("NOT OWNER: %s" % json.dumps([s["lease"] for s in status()["slots"] if not s["free"]]))
        return 4
    want = None if a.exclusive else a.slots
    WAIT_DIR.mkdir(parents=True, exist_ok=True)
    wf = WAIT_DIR / f"{a.owner}.json"
    old = _read(wf)
    ticket = old["ticket"] if old else time.time()
    deadline = time.time() + a.wait
    try:
        while True:
            now = time.time()
            _write(wf, {"owner": a.owner, "ticket": ticket, "polled": now, "slots": want})
            ok, slots, info = try_acquire(a.owner, a.ttl, want, ticket, now)
            if ok:
                print("OK lease until %s; SLOTS %s" % (time.ctime(now + a.ttl), ",".join(map(str, slots))))
                return 0
            if time.time() >= deadline:
                print("BUSY: %s" % info)
                return 3
            time.sleep(min(10, max(0.05, float(os.environ.get("GAMESLOT_POLL", 10)))))
    finally:
        wf.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(main())
