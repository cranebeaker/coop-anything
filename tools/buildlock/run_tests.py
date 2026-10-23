"""Tests for the build lock and the build driver, then a twin for each guard. Prints ALL OK as the last line only when every
check passes and every twin is caught.

    python -I tools/buildlock/run_tests.py

No real compiler runs. The build driver is tested against a fake cmake (tests/fake_cmake.py) that records what it was asked, and
the lock is tested with threads, with real short-lived stand-in processes (started and stopped by the tests themselves) and with
the command line. A check runs once against the real code, then again with one guard broken on purpose from outside (a function
of buildlock.py or build.py replaced). A check that still passes against the broken code proves nothing, and the run fails.
"""
import contextlib
import io
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import build as bld  # noqa: E402
import buildlock as bl  # noqa: E402
from buildlock import BuildLock, LockTimeout  # noqa: E402

PY = sys.executable
REAL_STAMP = bl.proc_stamp          # a twin may replace bl.proc_stamp; the helpers that start and stop stand-ins keep the real one
FAKE_CMAKE = HERE / "tests" / "fake_cmake.py"
FAKE_JOB = HERE / "tests" / "fake_job.py"
BUILDLOCK = HERE / "buildlock.py"
TMPS = []
SPAWNED = []
_UNDO = []


# ---------------------------------------------------------------------------------------------- helpers
def patch(obj, name, value):
    _UNDO.append((obj, name, getattr(obj, name)))
    setattr(obj, name, value)


def undo():
    while _UNDO:
        obj, name, old = _UNDO.pop()
        setattr(obj, name, old)


def hook(mut, **objs):
    if mut:
        mut(**objs)


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def tmpdir(prefix="bl-"):
    p = Path(tempfile.mkdtemp(prefix=prefix))
    TMPS.append(p)
    return p


@contextlib.contextmanager
def env(**kv):
    old = {k: os.environ.get(k) for k in kv}
    for k, v in kv.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = str(v)
    try:
        yield
    finally:
        for k, v in old.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def mk(path, owner="o", **kw):
    kw.setdefault("poll_s", 0.02)
    return BuildLock(path, owner, **kw)


def acq(lock, wait_s=0.0):
    """Take the lock; a timeout is a failed check, not a crash."""
    try:
        return lock.acquire(wait_s)
    except LockTimeout as e:
        raise AssertionError(f"could not take the lock: {e}")


def kill_pid(pid, stamp):
    """Stop a process a check started, only while it is still that process."""
    if REAL_STAMP(pid) != stamp:
        return
    if os.name == "nt":
        subprocess.run(["taskkill", "/F", "/PID", str(pid)], capture_output=True)
    else:
        os.kill(pid, 9)


def spawn_sleeper():
    """A process that only waits. Returns (Popen, pid, stamp)."""
    p = subprocess.Popen([PY, "-I", "-c", "import time; time.sleep(120)"], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    SPAWNED.append(p)
    return p, p.pid, REAL_STAMP(p.pid)


def dead_pid():
    """A pid that belonged to a process which has ended."""
    p, pid, stamp = spawn_sleeper()
    p.kill()
    p.wait()
    for _ in range(100):
        if REAL_STAMP(pid) is None:
            break
        time.sleep(0.05)
    return pid, stamp


def write_lock(path, **over):
    """A lock folder as another agent would have left it."""
    path = Path(path)
    path.mkdir(parents=True)
    info = {"owner": "ghost", "label": "", "pid": os.getpid(), "pid_stamp": bl.proc_stamp(os.getpid()), "token": "ghost-token",
            "acquired": time.time(), "heartbeat": time.time(), "lease_s": 3600, "max_hold_s": 7200}
    info.update(over)
    (path / bl.LOCK_FILE).write_text(json.dumps(info))
    return info


def run_cli(args, **envkw):
    e = dict(os.environ)
    e.update({k: str(v) for k, v in envkw.items()})
    cp = subprocess.run([PY, "-I", str(BUILDLOCK), *args], capture_output=True, text=True, env=e, timeout=120)
    return cp.returncode, cp.stdout, cp.stderr


# ---------------------------------------------------------------------------------------------- lock checks
def check_proc_stamp(mut):
    hook(mut)
    me = bl.proc_stamp(os.getpid())
    check(me is not None and me == bl.proc_stamp(os.getpid()), f"a running process has a stable stamp: {me}")
    pid, stamp = dead_pid()
    check(bl.proc_stamp(pid) is None, "a process that ended has no stamp")
    check(bl.proc_stamp(0) is None and bl.proc_stamp(-5) is None, "pid 0 and negative pids are never alive")


def check_exclusive(mut):
    t = tmpdir()
    state = {"cur": 0, "peak": 0, "done": 0}
    guard = threading.Lock()
    hook(mut)

    def worker(i):
        lk = mk(t / "x.buildlock", f"t{i}")
        lk.acquire(30)
        try:
            with guard:
                state["cur"] += 1
                state["peak"] = max(state["peak"], state["cur"])
            time.sleep(0.05)
            with guard:
                state["cur"] -= 1
                state["done"] += 1
        finally:
            lk.release()
    ths = [threading.Thread(target=worker, args=(i,)) for i in range(6)]
    for th in ths:
        th.start()
        time.sleep(0.01)
    for th in ths:
        th.join(60)
    check(state["done"] == 6, f"every waiter got its turn: {state}")
    check(state["peak"] == 1, f"never two holders at once: peak {state['peak']}")
    check(not (t / "x.buildlock").exists(), "the lock is free at the end")


def check_timeout_names_the_holder(mut):
    t = tmpdir()
    a = mk(t / "l", "alice", label="release build")
    acq(a)
    hook(mut)
    b = mk(t / "l", "bob")
    t0 = time.time()
    try:
        b.acquire(0.3)
        raise AssertionError("the second taker must not get a held lock")
    except LockTimeout as e:
        msg = str(e)
    check("alice" in msg and "release build" in msg and f"pid {os.getpid()}" in msg, f"the message names the holder: {msg}")
    check(time.time() - t0 < 5, "the wait ends at the timeout")
    try:
        b.acquire(0)
        raise AssertionError("try-once must fail while held")
    except LockTimeout:
        pass
    check(a.release() is True, "the holder releases")
    check(acq(b, 1.0) is b, "now the other one can take it")
    b.release()


def check_release_only_by_the_holder(mut):
    t = tmpdir()
    a = mk(t / "l", "a", lease_s=0.3)
    hook(mut)
    acq(a)
    info = json.loads((t / "l" / bl.LOCK_FILE).read_text())
    info.update(owner="bob", token="bobs-token")                  # the lock was broken and taken by bob
    (t / "l" / bl.LOCK_FILE).write_text(json.dumps(info))
    for _ in range(40):
        if a.lost:
            break
        time.sleep(0.1)
    check(a.lost, "the heartbeat notices that the lock was taken away")
    check(a.release() is False, "a lock that is no longer ours is not released")
    check((t / "l").exists() and bl.BuildLock(t / "l").read()["owner"] == "bob", "bob's lock is untouched")


def check_stale_holder_process_gone(mut):
    t = tmpdir()
    p, pid, stamp = spawn_sleeper()
    write_lock(t / "l", owner="ghost", pid=pid, pid_stamp=stamp)
    b = mk(t / "l", "b")
    check(b.status()["state"] == "held", f"a live holder is respected: {b.status()}")
    p.kill()
    p.wait()
    for _ in range(100):
        if REAL_STAMP(pid) is None:
            break
        time.sleep(0.05)
    hook(mut)
    check(b.status()["state"] == "stale", f"a dead holder makes the lock stale: {b.status()}")
    t0 = time.time()
    acq(b, 5)
    check(time.time() - t0 < 4 and b.recovered and "gone" in b.recovered, f"the stale lock is recovered fast: {b.recovered}")
    check(bl.BuildLock(t / "l").read()["owner"] == "b", "the new holder is recorded")
    b.release()


def check_pid_reuse(mut):
    t = tmpdir()
    write_lock(t / "l", pid_stamp="1")           # a live pid with another start time: the recorded process ended, the pid was reused
    hook(mut)
    b = mk(t / "l", "b")
    st = b.status()
    check(st["state"] == "stale" and "reused" in st["reason"], f"a reused pid is not the holder: {st}")
    acq(b, 2)
    b.release()


def check_lease_expiry(mut):
    t = tmpdir()
    write_lock(t / "l", heartbeat=time.time() - 100, lease_s=10)
    hook(mut)
    b = mk(t / "l", "b")
    st = b.status()
    check(st["state"] == "stale" and "heartbeat" in st["reason"], f"no heartbeat for longer than the lease: {st}")
    acq(b, 2)
    b.release()


def check_max_hold(mut):
    t = tmpdir()
    write_lock(t / "l", acquired=time.time() - 500, max_hold_s=100)
    hook(mut)
    b = mk(t / "l", "b")
    st = b.status()
    check(st["state"] == "stale" and "limit" in st["reason"], f"held longer than the limit: {st}")
    acq(b, 2)
    b.release()


def check_heartbeat_keeps_the_lock(mut):
    t = tmpdir()
    a = mk(t / "l", "a", lease_s=0.6)
    hook(mut)
    acq(a)
    b = mk(t / "l", "b")
    try:
        b.acquire(1.8)                                      # three lease periods: only the heartbeat keeps the lock alive
        raise AssertionError("the lock was taken from a live holder that was still renewing")
    except LockTimeout:
        pass
    check(not a.lost, "the holder still owns the lock")
    a.release()


def check_fifo_queue(mut):
    t = tmpdir()
    lock_path = t / "l"
    q = Path(str(lock_path) + ".queue")
    q.mkdir()
    (q / "00000000000000000001-aaaaaa.json").write_text(json.dumps({"owner": "first", "pid": os.getpid()}))
    hook(mut)
    b = mk(lock_path, "b")
    try:
        b.acquire(0)
        raise AssertionError("someone has waited longer: a free lock must not be taken past them")
    except LockTimeout:
        pass
    pid, _ = dead_pid()
    (q / "00000000000000000001-aaaaaa.json").write_text(json.dumps({"owner": "gone", "pid": pid}))
    acq(b, 0)
    check(not list(q.glob("*.json")), "the ticket of a dead process is removed")
    b.release()
    hung = q / "00000000000000000002-bbbbbb.json"
    hung.write_text(json.dumps({"owner": "hung", "pid": os.getpid()}))        # a live process whose waiter stopped waiting long ago
    old = time.time() - 100
    os.utime(hung, (old, old))
    acq(b, 0)
    check(not hung.exists(), "a ticket that nobody has touched for a long time is dropped")
    b.release()
    # real waiters in order
    order, lock = [], mk(lock_path, "holder")
    acq(lock)

    def waiter(name):
        w = mk(lock_path, name)
        w.acquire(30)
        order.append(name)
        w.release()
    ths = []
    for name in ("w1", "w2", "w3"):
        th = threading.Thread(target=waiter, args=(name,))
        th.start()
        ths.append(th)
        for _ in range(200):
            if len(list(q.glob("*.json"))) == len(ths):
                break
            time.sleep(0.01)
    lock.release()
    for th in ths:
        th.join(30)
    check(order == ["w1", "w2", "w3"], f"first come, first served: {order}")


def check_break_needs_the_judged_token(mut):
    t = tmpdir()
    a = mk(t / "l", "a")
    acq(a)
    b = mk(t / "l", "b")
    hook(mut)
    check(b.break_if_stale("an-old-token") is False and (t / "l").exists(), "a lock that is not the one judged stale is not broken")
    Path(str(t / "l") + ".breaking").mkdir()
    check(b.break_if_stale(a.token) is False and (t / "l").exists(), "a break guard held by someone else blocks the break")
    shutil.rmtree(str(t / "l") + ".breaking")
    check(b.break_if_stale(a.token) is True and not (t / "l").exists(), "the lock that was judged stale is removed")
    check(not Path(str(t / "l") + ".breaking").exists(), "the guard is dropped")
    a._stop.set()


def check_lock_folder_without_file(mut):
    t = tmpdir()
    (t / "l").mkdir()
    hook(mut)
    b = mk(t / "l", "b")
    check(b.status()["state"] == "held", f"a lock folder that is being created is respected: {b.status()}")
    old = time.time() - 60
    os.utime(t / "l", (old, old))
    check(b.status()["state"] == "stale", f"an old lock folder without lock.json is stale: {b.status()}")
    acq(b, 2)
    b.release()


def check_context_manager_releases(mut):
    t = tmpdir()
    hook(mut)
    try:
        with mk(t / "l", "a").acquire(1) as lk:
            check((t / "l").exists(), "held inside the block")
            raise RuntimeError("the build failed")
    except RuntimeError:
        pass
    check(not (t / "l").exists(), "released after an exception")
    check(lk._beat is None, "the heartbeat thread is stopped")


# ---------------------------------------------------------------------------------------------- command line checks
def check_cli_run_exit_codes(mut):
    t = tmpdir()
    tree = t / "tree"
    hook(mut)
    log = t / "job.log"
    rc, out, err = run_cli(["run", "--tree", str(tree), "--owner", "cli", "--", PY, "-I", str(FAKE_JOB), "j", str(log), "0.05", "7"])
    check(rc == 7, f"the command's exit code is passed on: {rc} {err}")
    check(not Path(str(tree) + ".buildlock").exists(), "the lock is released after a failing command")
    rc, out, err = run_cli(["run", "--tree", str(tree), "--", PY, "-I", str(FAKE_JOB), "j2", str(log), "0.05", "0"])
    check(rc == 0 and "lock taken" in err and "released" in err, f"a passing command: {rc} {err}")
    rc, out, err = run_cli(["run", "--tree", str(tree)])
    check(rc == 2 and "needs a command" in err, f"no command is a usage error: {rc} {err}")
    holder = mk(Path(str(tree) + ".buildlock"), "alice")
    acq(holder)
    try:
        rc, out, err = run_cli(["run", "--tree", str(tree), "--wait-s", "0.3", "--poll-s", "0.05", "--", PY, "-I", "-c", "pass"])
        check(rc == 3 and "alice" in err, f"a lock that stays busy is exit 3 and names the holder: {rc} {err}")
    finally:
        holder.release()


def check_cli_jobs_never_overlap(mut):
    t = tmpdir()
    tree, log = t / "tree", t / "job.log"
    hook(mut)
    procs = []
    for name in ("j1", "j2", "j3"):
        procs.append(subprocess.Popen([PY, "-I", str(BUILDLOCK), "run", "--tree", str(tree), "--owner", name, "--poll-s", "0.05", "--quiet",
                                       "--", PY, "-I", str(FAKE_JOB), name, str(log), "1.2"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        time.sleep(0.5)
    codes = [p.wait(60) for p in procs]
    check(codes == [0, 0, 0], f"all three jobs ran: {codes}")
    rows = [ln.split() for ln in log.read_text().splitlines()]
    spans = {}
    for what, name, when in rows:
        spans.setdefault(name, {})[what] = float(when)
    check(sorted(spans) == ["j1", "j2", "j3"], f"every job logged: {rows}")
    order = sorted(spans, key=lambda n: spans[n]["start"])
    check(order == ["j1", "j2", "j3"], f"the jobs ran in the order they asked: {order}")
    for a, b in (("j1", "j2"), ("j2", "j3")):
        check(spans[a]["end"] <= spans[b]["start"], f"{a} and {b} did not overlap: {spans}")


def check_cli_killed_holder_is_recovered(mut):
    t = tmpdir()
    tree, log, pidfile = t / "tree", t / "job.log", t / "job.pid"
    lock_path = Path(str(tree) + ".buildlock")
    e = dict(os.environ, FAKE_PIDFILE=str(pidfile))
    holder = subprocess.Popen([PY, "-I", str(BUILDLOCK), "run", "--tree", str(tree), "--owner", "doomed", "--quiet", "--", PY, "-I",
                               str(FAKE_JOB), "d", str(log), "120"], env=e, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    SPAWNED.append(holder)
    job_pid = job_stamp = None
    try:
        for _ in range(200):
            if pidfile.exists() and pidfile.read_text().strip() and lock_path.exists():
                job_pid = int(pidfile.read_text())
                job_stamp = REAL_STAMP(job_pid)
                break
            time.sleep(0.05)
        check(job_pid is not None, "the holder started its job")
        b = mk(lock_path, "rescuer")
        check(b.status()["state"] == "held" and b.status()["owner"] == "doomed", f"the live holder is respected: {b.status()}")
        holder.kill()                                           # the build tool dies without releasing anything
        holder.wait()
        hook(mut)
        for _ in range(100):
            if REAL_STAMP(holder.pid) is None:
                break
            time.sleep(0.05)
        acq(b, 10)
        check(b.recovered and "gone" in b.recovered, f"the lock of a dead holder is recovered: {b.recovered}")
        b.release()
    finally:
        if holder.poll() is None:
            holder.kill()
        if job_pid:
            kill_pid(job_pid, job_stamp)


def check_cli_status_and_break(mut):
    t = tmpdir()
    tree = t / "tree"
    lock_path = Path(str(tree) + ".buildlock")
    hook(mut)
    rc, out, err = run_cli(["status", "--tree", str(tree), "--json"])
    check(rc == 0 and json.loads(out)["state"] == "free", f"free: {out}")
    a = mk(lock_path, "alice", label="cache warmup")
    acq(a)
    rc, out, err = run_cli(["status", "--tree", str(tree), "--json"])
    st = json.loads(out)
    check(st["state"] == "held" and st["owner"] == "alice" and st["label"] == "cache warmup" and st["pid"] == os.getpid(), f"held: {out}")
    rc, out, err = run_cli(["break", "--tree", str(tree), "--only-if-stale"])
    check(rc == 1 and lock_path.exists(), f"a live lock is not broken with --only-if-stale: {rc} {out}")
    rc, out, err = run_cli(["break", "--tree", str(tree)])
    check(rc == 0 and not lock_path.exists(), f"break removes it by hand: {rc} {out}")
    a._stop.set()
    pid, stamp = dead_pid()
    write_lock(lock_path, pid=pid, pid_stamp=stamp)
    rc, out, err = run_cli(["status", "--tree", str(tree), "--json"])
    check(json.loads(out)["state"] == "stale", f"stale: {out}")
    rc, out, err = run_cli(["break", "--tree", str(tree), "--only-if-stale"])
    check(rc == 0 and not lock_path.exists() and "gone" in out, f"a stale lock is removed: {rc} {out}")


# ---------------------------------------------------------------------------------------------- build driver checks
def fake_tools(dirpath, names=("ninja", "sccache")):
    """Fake programs found on PATH. They do nothing, they only have to exist."""
    d = Path(dirpath)
    d.mkdir(exist_ok=True)
    for n in names:
        if os.name == "nt":
            (d / (n + ".cmd")).write_text("@echo off\r\nexit /b 0\r\n")
        else:
            (d / n).write_text("#!/bin/sh\nexit 0\n")
            (d / n).chmod(0o755)
    return d


def path_without(*programs):
    """The current PATH minus any folder that holds one of these programs, so a real install cannot leak into a check."""
    keep = [p for p in os.environ.get("PATH", "").split(os.pathsep) if p and not any(shutil.which(x, path=p) for x in programs)]
    return os.pathsep.join(keep)


class Build:
    """One scratch source and build folder, with a fake cmake, and a way to run build.main against them."""

    def __init__(self, tools=(), path_extra=()):
        self.t = tmpdir()
        self.src = self.t / "src"
        self.src.mkdir()
        self.tree = self.t / "tree"
        self.log = self.t / "cmake.log"
        self.lockdir = Path(str(self.tree) + ".buildlock")
        cmake = " ".join(shlex.quote(x.replace("\\", "/")) for x in (PY, "-I", str(FAKE_CMAKE)))
        self.cmake = cmake
        tooldir = fake_tools(self.t / "bin", tools) if tools else None
        parts = ([str(tooldir)] if tooldir else []) + list(path_extra) + [path_without("sccache", "ninja", "cl")]
        self.path = os.pathsep.join(parts)

    def run(self, *extra, **envkw):
        argv = ["--source", str(self.src), "--build", str(self.tree), "--cmake", self.cmake, "--msvc", "off", "--quiet", "--poll-s", "0.02",
                "--wait-s", "3"] + list(extra)
        o, e = io.StringIO(), io.StringIO()
        with env(PATH=self.path, FAKE_CMAKE_LOG=self.log, FAKE_LOCK_DIR=self.lockdir, **envkw), \
                contextlib.redirect_stdout(o), contextlib.redirect_stderr(e):
            rc = bld.main(argv)
        return rc, o.getvalue(), e.getvalue()

    def calls(self):
        if not self.log.exists():
            return []
        return [json.loads(ln) for ln in self.log.read_text().splitlines()]

    def kinds(self):
        out = []
        for c in self.calls():
            a = c["argv"]
            out.append("configure" if "-S" in a else ("dryrun" if a[-2:] == ["--", "-n"] else "build"))
        return out


def check_build_takes_the_lock_around_the_build(mut):
    b = Build()
    hook(mut)
    rc, out, err = b.run()
    check(rc == 0, f"{rc} {err}")
    calls = b.calls()
    check(b.kinds() == ["configure", "build"], f"a new tree is configured, then built: {b.kinds()}")
    check(all(c["lock_exists"] for c in calls), f"the lock is held while cmake runs: {calls}")
    check(not b.lockdir.exists(), "the lock is released afterwards")
    check("BUILD-TIME build" in out, f"the build time is printed: {out}")
    rc, out, err = b.run()
    check(b.kinds()[2:] == ["build"], f"the second run only builds: {b.kinds()}")


def check_failed_build_releases_the_lock(mut):
    b = Build()
    hook(mut)
    rc, out, err = b.run(FAKE_BUILD_RC=5)
    check(rc == 5 and "BUILD FAILED" in err, f"the build's exit code is passed on: {rc} {err}")
    check(not b.lockdir.exists(), "the lock is released after a failed build")
    b2 = Build()
    rc, out, err = b2.run(FAKE_CONFIGURE_RC=3)
    check(rc == 3 and "CONFIGURE FAILED" in err and b2.kinds() == ["configure"], f"a failed configure stops: {rc} {err} {b2.kinds()}")
    check(not b2.lockdir.exists(), "the lock is released after a failed configure")


def check_up_to_date_tree_does_not_wait_for_the_lock(mut):
    b = Build(tools=("ninja",))
    rc, out, err = b.run()
    check(rc == 0 and b.kinds() == ["configure", "build"], f"first build: {rc} {err} {b.kinds()}")
    (b.tree / "uptodate").write_text("x")
    other = mk(b.lockdir, "someone-else")
    acq(other)
    hook(mut)
    try:
        t0 = time.time()
        rc, out, err = b.run("--wait-s", "0.5")
        check(rc == 0 and "no work to do" in out, f"nothing to build means no waiting: {rc} {out} {err}")
        check(time.time() - t0 < 5 and b.kinds()[2:] == ["dryrun"], f"only the dry run happened: {b.kinds()}")
    finally:
        other.release()


def check_lock_timeout_is_exit_3(mut):
    b = Build()
    other = mk(b.lockdir, "alice")
    acq(other)
    hook(mut)
    try:
        rc, out, err = b.run("--wait-s", "0.3")
    finally:
        other.release()
    check(rc == 3 and "alice" in err, f"a busy lock is exit 3 and names the holder: {rc} {err}")
    check(b.calls() == [], "nothing was run without the lock")


def check_reconfigure_only_when_arguments_change(mut):
    b = Build()
    hook(mut)
    b.run()
    b.run()
    check(b.kinds() == ["configure", "build", "build"], f"same arguments: no second configure: {b.kinds()}")
    b.run("--cmake-arg=-DFEATURE=1")
    check(b.kinds()[3:] == ["configure", "build"], f"a new argument configures again: {b.kinds()}")
    b.run("--cmake-arg=-DFEATURE=1")
    check(b.kinds()[5:] == ["build"], f"and then not again: {b.kinds()}")
    cfg = [c for c in b.calls() if "-S" in c["argv"]][-1]["argv"]
    check("-DFEATURE=1" in cfg and any(a.startswith("-DCMAKE_BUILD_TYPE=") for a in cfg), f"configure arguments: {cfg}")


def check_sccache_option(mut):
    b = Build(tools=("ninja", "sccache"))
    hook(mut)
    rc, out, err = b.run()
    cfg = [c for c in b.calls() if "-S" in c["argv"]][0]["argv"]
    check(rc == 0 and "Ninja" in cfg, f"ninja is chosen when it is on PATH: {cfg}")
    check(any(a.startswith("-DCMAKE_CXX_COMPILER_LAUNCHER=") and "sccache" in a for a in cfg), f"auto: sccache on PATH is the launcher: {cfg}")
    check(any(a.startswith("-DCMAKE_C_COMPILER_LAUNCHER=") for a in cfg) and "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON" in cfg,
          f"the C launcher and no precompiled headers: {cfg}")
    b2 = Build(tools=("ninja", "sccache"))
    b2.run("--sccache", "off")
    cfg = [c for c in b2.calls() if "-S" in c["argv"]][0]["argv"]
    check(not any("LAUNCHER" in a for a in cfg), f"off: no launcher even though sccache is on PATH: {cfg}")
    b3 = Build(tools=("ninja",))
    rc, out, err = b3.run()
    cfg = [c for c in b3.calls() if "-S" in c["argv"]][0]["argv"]
    check(rc == 0 and not any("LAUNCHER" in a for a in cfg), f"auto without sccache builds without it: {cfg}")
    rc, out, err = b3.run("--sccache", "on", "--build", str(b3.t / "other"))
    check(rc == 2 and "not on PATH" in err, f"on without sccache is a setup error: {rc} {err}")
    b4 = Build(tools=("ninja", "sccache"))
    rc, out, err = b4.run("--generator", "Visual Studio 17 2022")
    cfg = [c for c in b4.calls() if "-S" in c["argv"]][0]["argv"]
    check(rc == 0 and not any("LAUNCHER" in a for a in cfg), f"a Visual Studio generator cannot use a launcher: {cfg}")
    b5 = Build(tools=("ninja", "sccache"))
    rc, out, err = b5.run("--generator", "Visual Studio 17 2022", "--sccache", "on")
    check(rc == 2 and "Ninja" in err and b5.calls() == [], f"on with a Visual Studio generator is refused: {rc} {err}")


def check_environment_is_cleaned(mut):
    b = Build()
    hook(mut)
    rc, out, err = b.run(CMAKE_TOOLCHAIN_FILE="stray-toolchain.cmake", CC="stray-cc")
    check(rc == 0, f"{rc} {err}")
    envs = [c["env"] for c in b.calls()]
    check(all(e["CMAKE_TOOLCHAIN_FILE"] is None and e["CC"] is None for e in envs), f"stray toolchain variables are removed: {envs}")
    b2 = Build()
    b2.run("--keep-env", CMAKE_TOOLCHAIN_FILE="stray-toolchain.cmake")
    check(b2.calls()[0]["env"]["CMAKE_TOOLCHAIN_FILE"] == "stray-toolchain.cmake", "--keep-env keeps them")
    b3 = Build()
    b3.run("--jobs", "1")
    check(all(c["env"]["CMAKE_BUILD_PARALLEL_LEVEL"] == "1" for c in b3.calls()), "--jobs sets the parallel level")


def check_msvc_environment(mut):
    b = Build(tools=("ninja", "sccache"))
    if os.name != "nt":
        check(bld.parse_set_output("A=1\nB=x=y\n=C:=C:\\\\\n") == {"A": "1", "B": "x=y"}, "parse_set_output")
        return
    bat = b.t / "fake_vcvars.bat"
    bat.write_text("@echo off\r\nset FAKEVAR=from_vcvars\r\n")
    hook(mut)
    check(bld.parse_set_output("A=1\nB=x=y\n=C:=C:\\\\\n") == {"A": "1", "B": "x=y"}, "parse_set_output")
    rc, out, err = b.run("--msvc", "on", "--vcvars", str(bat), CMAKE_TOOLCHAIN_FILE="stray")
    check(rc == 0, f"{rc} {err}")
    first = b.calls()[0]
    check(first["env"]["FAKEVAR"] == "from_vcvars", f"the Visual Studio environment reaches cmake: {first['env']}")
    check(first["env"]["CMAKE_TOOLCHAIN_FILE"] is None, "stray variables are removed after vcvars as well")
    cfg = first["argv"]
    check("-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded" in cfg and "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW" in cfg,
          f"sccache with MSVC gets embedded debug info: {cfg}")
    b2 = Build(tools=("ninja",))
    b2.run("--msvc", "off", "--vcvars", str(bat))
    check(b2.calls()[0]["env"]["FAKEVAR"] is None, "--msvc off does not load vcvars")
    rc, out, err = Build().run("--msvc", "on", "--vcvars", str(b.t / "missing.bat"))
    check(rc == 2 and "vcvars was not found" in err, f"a missing vcvars file is a setup error: {rc} {err}")


def check_dry_run_configure_only_clean_and_targets(mut):
    b = Build()
    rc, out, err = b.run("--dry-run", "--target", "a", "b")
    check(rc == 0 and "configure:" in out and "--target a b" in out and b.calls() == [], f"dry run prints and runs nothing: {out}")
    rc, out, err = b.run("--configure-only")
    check(rc == 0 and b.kinds() == ["configure"] and not b.calls()[0]["lock_exists"], f"configure only: no build, no lock: {b.kinds()}")
    rc, out, err = b.run("--target", "t1", "t2")
    build_call = [c for c in b.calls() if "--build" in c["argv"]][0]["argv"]
    check(build_call[build_call.index("--target") + 1:][:2] == ["t1", "t2"] and "--config" in build_call, f"targets reach cmake: {build_call}")
    (b.tree / "sentinel.txt").write_text("x")
    rc, out, err = b.run("--clean")
    check(rc == 0 and not (b.tree / "sentinel.txt").exists() and b.kinds()[-2:] == ["configure", "build"], f"--clean starts over: {b.kinds()}")
    hook(mut)


# ---------------------------------------------------------------------------------------------- twins
def tw_always_granted(**_):
    def create(self):
        self.token = "t-" + self.owner
        return True
    patch(BuildLock, "_try_create", create)


def tw_release_by_anyone(**_):
    def release(self):
        self._stop.set()
        shutil.rmtree(self.path, ignore_errors=True)
        return True
    patch(BuildLock, "release", release)


def tw_release_noop(**_):
    def release(self):
        self._stop.set()
        return False
    patch(BuildLock, "release", release)


def tw_process_always_alive(**_):
    patch(bl, "proc_stamp", lambda pid: "?")


def tw_pid_reuse_not_noticed(**_):
    patch(bl, "stamp_differs", lambda recorded, now: False)


def tw_lease_never_expires(**_):
    patch(bl, "lease_expired", lambda info, now: False)


def tw_held_forever(**_):
    patch(bl, "max_hold_exceeded", lambda info, now: False)


def tw_no_heartbeat(**_):
    patch(BuildLock, "_start_heartbeat", lambda self: None)


def tw_no_queue(**_):
    patch(BuildLock, "_first_in_line", lambda self, mine: True)


def tw_tickets_never_stale(**_):
    patch(bl, "TICKET_STALE_S", 1e12)


def tw_break_ignores_token(**_):
    def brk(self, judged_token):
        shutil.rmtree(self.path, ignore_errors=True)
        return True
    patch(BuildLock, "break_if_stale", brk)


def tw_no_grace_for_new_lock(**_):
    patch(bl, "NO_FILE_GRACE_S", -1e9)          # a negative grace: every lock folder without lock.json is stale at once


def tw_lock_not_taken_by_driver(**_):
    patch(bld.BuildLock, "acquire", lambda self, wait_s=0.0: self)


def tw_always_wait_for_lock(**_):
    patch(bld, "read_stamp", lambda build: None)


def tw_stamp_never_changes(**_):
    patch(bld, "configure_stamp", lambda args, source: "same")


def tw_no_sccache_flags(**_):
    patch(bld, "sccache_args", lambda exe, msvc: [])


def tw_sccache_ignores_off(**_):
    real = bld.choose_sccache
    patch(bld, "choose_sccache", lambda mode, env, generator, say=print: real("auto", env, generator, say))


def tw_env_not_cleaned(**_):
    patch(bld, "clean_env", lambda base, keep_env=False: dict(base))


def tw_vcvars_ignored(**_):
    patch(bld, "vcvars_env", lambda bat, base: dict(base))


REAL = [
    check_proc_stamp, check_exclusive, check_timeout_names_the_holder, check_release_only_by_the_holder,
    check_stale_holder_process_gone, check_pid_reuse, check_lease_expiry, check_max_hold, check_heartbeat_keeps_the_lock,
    check_fifo_queue, check_break_needs_the_judged_token, check_lock_folder_without_file, check_context_manager_releases,
    check_cli_run_exit_codes, check_cli_jobs_never_overlap, check_cli_killed_holder_is_recovered, check_cli_status_and_break,
    check_build_takes_the_lock_around_the_build, check_failed_build_releases_the_lock, check_up_to_date_tree_does_not_wait_for_the_lock,
    check_lock_timeout_is_exit_3, check_reconfigure_only_when_arguments_change, check_sccache_option, check_environment_is_cleaned,
    check_msvc_environment, check_dry_run_configure_only_clean_and_targets,
]

TWINS = [
    (check_exclusive, "lock_always_granted", tw_always_granted),
    (check_release_only_by_the_holder, "release_by_anyone", tw_release_by_anyone),
    (check_release_only_by_the_holder, "heartbeat_never_notices", tw_no_heartbeat),
    (check_stale_holder_process_gone, "dead_holder_still_alive", tw_process_always_alive),
    (check_cli_killed_holder_is_recovered, "dead_holder_still_alive", tw_process_always_alive),
    (check_pid_reuse, "pid_reuse_not_noticed", tw_pid_reuse_not_noticed),
    (check_lease_expiry, "lease_never_expires", tw_lease_never_expires),
    (check_max_hold, "held_forever", tw_held_forever),
    (check_heartbeat_keeps_the_lock, "no_heartbeat", tw_no_heartbeat),
    (check_fifo_queue, "no_queue", tw_no_queue),
    (check_fifo_queue, "tickets_never_stale", tw_tickets_never_stale),
    (check_break_needs_the_judged_token, "break_ignores_token", tw_break_ignores_token),
    (check_lock_folder_without_file, "no_grace_for_new_lock", tw_no_grace_for_new_lock),
    (check_context_manager_releases, "release_noop", tw_release_noop),
    (check_build_takes_the_lock_around_the_build, "lock_not_taken", tw_lock_not_taken_by_driver),
    (check_failed_build_releases_the_lock, "lock_never_released", tw_release_noop),
    (check_up_to_date_tree_does_not_wait_for_the_lock, "always_wait_for_lock", tw_always_wait_for_lock),
    (check_reconfigure_only_when_arguments_change, "stamp_never_changes", tw_stamp_never_changes),
    (check_sccache_option, "no_sccache_flags", tw_no_sccache_flags),
    (check_sccache_option, "sccache_ignores_off", tw_sccache_ignores_off),
    (check_environment_is_cleaned, "env_not_cleaned", tw_env_not_cleaned),
    (check_msvc_environment, "vcvars_ignored", tw_vcvars_ignored),
]


def final_sweep():
    for p in SPAWNED:
        try:
            if p.poll() is None:
                p.kill()
            p.wait(10)
        except (Exception, SystemExit):  # noqa: BLE001
            pass


def main():
    bad, n_real, n_twin = [], 0, 0
    try:
        for fn in REAL:
            n_real += 1
            t0 = time.time()
            try:
                fn(None)
                print("ok    ", fn.__name__, "(%.1fs)" % (time.time() - t0), flush=True)
            except (Exception, SystemExit):  # noqa: BLE001
                bad.append(fn.__name__)
                print("FAIL  ", fn.__name__, flush=True)
                traceback.print_exc()
            finally:
                undo()
        for fn, name, mut in TWINS:
            n_twin += 1
            try:
                fn(mut)
            except AssertionError:
                print("ok     twin %s[%s] caught" % (fn.__name__, name), flush=True)
            except (Exception, SystemExit):  # noqa: BLE001  (a crash is not a caught defect)
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] crashed instead of failing a check" % (fn.__name__, name), flush=True)
                traceback.print_exc()
            else:
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] NOT caught" % (fn.__name__, name), flush=True)
            finally:
                undo()
    finally:
        undo()
        final_sweep()
        for t in TMPS:
            for _ in range(20):
                shutil.rmtree(t, ignore_errors=True)
                if not t.exists():
                    break
                time.sleep(0.25)
    print(f"\n{n_real} checks, {n_twin} twins, {len(bad)} failed")
    if bad:
        print("FAILED:", ", ".join(bad))
        return 1
    print("ALL OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
