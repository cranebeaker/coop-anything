"""Tests for gameslot.py, then one mutation twin per guard. Prints ALL OK only when every check passes
and every twin is caught.

    python -I tools/gameslot/run_tests.py

No game is started. Most checks use a fake process table (nothing is really killed). Two checks start
harmless stand-in processes of their own (a Python interpreter that sleeps) and kill only those.

How twins work: the same check runs against a copy of the module that has been broken on purpose
(for example "every slot is always granted"). A check that still passes against the broken copy is
not testing anything, and the run fails.
"""
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback
from contextlib import redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "gameslot.py"
TMPS = []


class FakeProcs:
    """An in-memory process table. kill() removes a process; nothing real is touched."""

    def __init__(self):
        self.table = {}          # pid -> {"name", "created"}
        self.killed = []

    def add(self, pid, name="game.exe", created=10):
        self.table[pid] = {"name": name, "created": created}

    def info(self, pid):
        return dict(self.table[pid]) if pid in self.table else None

    def kill(self, pid):
        self.killed.append(pid)
        self.table.pop(pid, None)

    def named(self, name):
        return [p for p, v in self.table.items() if v["name"] == name]


class Env:
    def __init__(self, slots=3, mutate=None, min_free=3.0, config=True):
        self.tmp = Path(tempfile.mkdtemp(prefix="gameslot-test-"))
        TMPS.append(self.tmp)
        os.environ["GAMESLOT_DIR"] = str(self.tmp)
        os.environ["GAMESLOT_POLL"] = "0.05"
        if config:
            (self.tmp / "gameslot_config.json").write_text(json.dumps({"slots": slots, "min_free_gb": min_free}))
        spec = importlib.util.spec_from_file_location("gameslot_%d" % time.monotonic_ns(), SRC)
        self.m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.m)
        self.procs = FakeProcs()
        self.m.process_info, self.m.kill_pid = self.procs.info, self.procs.kill
        self.m.pids_named = lambda name: self.procs.named(name)
        self.m.avail_gb = lambda: 99.0           # a check must not depend on this machine's RAM
        if mutate:
            mutate(self.m, self)

    def call(self, *argv):
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = self.m.main(list(argv))
        return rc, buf.getvalue()

    def slot_file(self, i):
        return self.tmp / "slots" / f"slot-{i}.json"

    def expire(self, i):
        d = json.loads(self.slot_file(i).read_text())
        d["expires"] = time.time() - 1
        self.slot_file(i).write_text(json.dumps(d))

    def free_count(self):
        return [s["free"] for s in self.m.status()["slots"]].count(True)


def slots_of(out):
    return [int(x) for x in out.strip().rsplit("SLOTS ", 1)[1].split(",")]


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


# ---------------------------------------------------------------------------------------------- checks
def check_distinct_slots_then_busy(mut):
    e = Env(3, mut)
    got = []
    for o in ("A", "B", "C"):
        rc, out = e.call("acquire", "--owner", o)
        check(rc == 0, out)
        got += slots_of(out)
    check(sorted(got) == [0, 1, 2], f"owners must get distinct slots, got {got}")
    rc, out = e.call("acquire", "--owner", "D", "--wait", "0")
    check(rc == 3 and "BUSY" in out, f"a fourth owner must be told BUSY: {rc} {out}")


def check_exclusive_takes_every_slot(mut):
    e = Env(3, mut)
    rc, out = e.call("acquire", "--owner", "ALL", "--exclusive")
    check(rc == 0 and slots_of(out) == [0, 1, 2], out)
    check(e.call("acquire", "--owner", "X")[0] == 3, "nobody else may start while an exclusive lease is held")
    check(e.call("release", "--owner", "ALL")[0] == 0, "release")
    check(e.call("acquire", "--owner", "X")[0] == 0, "free again after release")
    check(e.call("acquire", "--owner", "Y", "--exclusive")[0] == 3, "an exclusive request needs every slot free")


def check_all_or_nothing(mut):
    e = Env(3, mut)
    e.call("acquire", "--owner", "A", "--slots", "2")
    rc, _ = e.call("acquire", "--owner", "B", "--slots", "2")
    check(rc == 3, "B asked for 2 but only 1 is free")
    check(e.free_count() == 1, "B must not keep a partial grab")
    check(e.call("acquire", "--owner", "B", "--slots", "1")[0] == 0, "one slot is still available to B")


def check_reacquire_renews(mut):
    e = Env(3, mut)
    _, o1 = e.call("acquire", "--owner", "A", "--slots", "2", "--ttl", "5")
    _, o2 = e.call("acquire", "--owner", "A", "--slots", "2", "--ttl", "500")
    check(slots_of(o1) == slots_of(o2), "same owner keeps the same slots")
    lease = e.m.status()["slots"][slots_of(o1)[0]]["lease"]
    check(lease["expires"] > time.time() + 400, "the ttl must be renewed")


def check_expired_lease_is_taken_over(mut):
    e = Env(1, mut)
    e.call("acquire", "--owner", "A", "--ttl", "1")
    e.expire(0)
    rc, _ = e.call("acquire", "--owner", "B")
    check(rc == 0, "an expired lease can be taken over")
    check(json.loads(e.slot_file(0).read_text())["owner"] == "B", "B now owns the slot")


def check_live_lease_is_never_taken_over_or_killed(mut):
    e = Env(1, mut)
    e.procs.add(111)
    e.call("acquire", "--owner", "A", "--ttl", "1500")
    e.call("register-pid", "--owner", "A", "--pid", "111")
    check(e.call("acquire", "--owner", "X")[0] == 3, "live lease: busy")
    check(e.procs.killed == [], f"a live lease's process must never be killed: {e.procs.killed}")
    check(json.loads(e.slot_file(0).read_text())["owner"] == "A", "owner unchanged")


def check_fifo_waiter_not_starved(mut):
    e = Env(3, mut)
    e.call("acquire", "--owner", "A")
    wd = e.tmp / "waiters"
    wd.mkdir(parents=True, exist_ok=True)
    (wd / "BIG.json").write_text(json.dumps({"owner": "BIG", "ticket": time.time() - 100, "polled": time.time(), "slots": None}))
    rc, out = e.call("acquire", "--owner", "B")
    check(rc == 3 and "BIG" in out, f"free slots exist but an older waiter is queued: {rc} {out}")
    (wd / "BIG.json").write_text(json.dumps({"owner": "BIG", "ticket": time.time() - 100, "polled": time.time() - 500, "slots": None}))
    check(e.call("acquire", "--owner", "B")[0] == 0, "a waiter that stopped polling must not block anyone")


def check_wait_blocks_until_released(mut):
    e = Env(1, mut)
    e.call("acquire", "--owner", "A")
    threading.Timer(0.5, lambda: e.m.release("A")).start()   # not through main(): no stdout redirect from two threads
    t0 = time.time()
    rc, _ = e.call("acquire", "--owner", "B", "--wait", "10")
    check(rc == 0, "B gets the slot once A releases")
    check(time.time() - t0 >= 0.4, "B really waited")


def check_ram_guard(mut):
    e = Env(3, mut)
    check(e.call("acquire", "--owner", "A")[0] == 0, "first game")
    e.m.avail_gb = lambda: 1.5
    rc, out = e.call("acquire", "--owner", "B")
    check(rc == 3 and "low memory" in out, f"a second game with 1.5 GB free must be refused: {rc} {out}")
    check(e.call("acquire", "--owner", "A")[0] == 0, "renewing a held slot is not a new slot")
    check(e.call("acquire", "--owner", "A", "--slots", "2")[0] == 3, "growing needs headroom too")
    e.m.avail_gb = lambda: 3.5
    check(e.call("acquire", "--owner", "B")[0] == 0, "enough RAM: granted")


def check_ram_guard_edges(mut):
    e = Env(3, mut)
    e.m.avail_gb = lambda: 0.5
    check(e.call("acquire", "--owner", "A")[0] == 0, "the first game is never blocked by low memory")
    e.m.avail_gb = lambda: None
    check(e.call("acquire", "--owner", "B")[0] == 0, "unreadable RAM does not block")
    (e.tmp / "gameslot_config.json").write_text(json.dumps({"slots": 3, "min_free_gb": 6}))
    e.m.avail_gb = lambda: 5.0
    check(e.call("acquire", "--owner", "C")[0] == 3, "min_free_gb comes from the config file")


def check_default_slots_is_two(mut):
    e = Env(0, mut, config=False)
    check(e.m.max_slots() == 2, "default is 2 slots without a config file")
    check(e.m.min_free_gb() == 3.0, "default RAM guard is 3 GB")


def check_register_and_release(mut):
    e = Env(2, mut)
    e.procs.add(111, "game.exe", 42)
    check(e.call("register-pid", "--owner", "NOBODY", "--pid", "111")[0] == 4, "register needs a live lease")
    e.call("acquire", "--owner", "A")
    check(e.call("register-pid", "--owner", "A", "--pid", "111")[0] == 0, "register")
    e.call("register-pid", "--owner", "A", "--pid", "111")
    rec = [json.loads(p.read_text())["procs"] for p in (e.tmp / "slots").glob("slot-*.json")]
    check(rec == [[{"pid": 111, "created": 42, "name": "game.exe"}]], f"pid recorded once with name and creation time: {rec}")
    check(e.call("release", "--owner", "B")[0] == 4, "release by a non-owner is refused")
    check(e.call("release", "--owner", "A")[0] == 0, "release by the owner")
    check(e.free_count() == 2, "slots are free again")


def check_takeover_kills_only_registered(mut):
    e = Env(3, mut)
    e.procs.add(111)
    e.procs.add(222)
    e.procs.add(333)
    e.call("acquire", "--owner", "OLD", "--ttl", "1")
    e.call("register-pid", "--owner", "OLD", "--pid", "111")
    e.call("acquire", "--owner", "LIVE")
    e.call("register-pid", "--owner", "LIVE", "--pid", "222")
    old_slot = [i for i in range(3) if e.slot_file(i).exists() and json.loads(e.slot_file(i).read_text())["owner"] == "OLD"][0]
    e.expire(old_slot)
    check(e.call("acquire", "--owner", "NEW")[0] == 0, "NEW takes the expired slot")
    check(e.procs.killed == [111], f"only the expired lease's registered pid may die; killed {e.procs.killed} "
          "(222 belongs to a live lease, 333 to nobody)")


def check_pid_reuse_is_not_killed(mut):
    e = Env(1, mut)
    e.procs.add(111, "game.exe", 10)
    e.call("acquire", "--owner", "OLD", "--ttl", "1")
    e.call("register-pid", "--owner", "OLD", "--pid", "111")
    e.procs.table[111] = {"name": "game.exe", "created": 99}     # the old process ended; the pid now belongs to a new one
    e.expire(0)
    check(e.call("acquire", "--owner", "NEW")[0] == 0, "takeover works")
    check(e.procs.killed == [], "a reused pid is a different process and must not be killed")
    e.procs.add(112, "game.exe", 10)
    e.call("release", "--owner", "NEW")
    e.call("acquire", "--owner", "OLD2", "--ttl", "1")
    e.call("register-pid", "--owner", "OLD2", "--pid", "112")
    e.procs.table[112] = {"name": "other.exe", "created": 10}    # same creation time, other image
    e.expire(0)
    e.call("acquire", "--owner", "NEW2")
    check(e.procs.killed == [], "a pid whose image name changed must not be killed")


def check_state_lock_is_exclusive(mut):
    e = Env(1, mut)
    check(e.m.try_lock(0.2), "first lock")
    try:
        check(not e.m.try_lock(0.2), "a second lock while the first is held must fail")
    finally:
        e.m.unlock()
    check(e.m.try_lock(0.2), "free after unlock")
    e.m.unlock()


def check_stale_lock_is_broken(mut):
    e = Env(1, mut)
    (e.tmp).mkdir(exist_ok=True)
    lock = e.tmp / "state.lock"
    lock.write_text("99999")
    old = time.time() - 3600
    os.utime(lock, (old, old))
    check(e.m.try_lock(1.0), "a lock left behind by a dead process must not block forever")
    e.m.unlock()


def check_concurrent_cli_only_one_winner(mut):
    """Real processes: six callers race for one slot through the command line."""
    tmp = Path(tempfile.mkdtemp(prefix="gameslot-race-"))
    TMPS.append(tmp)
    (tmp / "gameslot_config.json").write_text(json.dumps({"slots": 1, "min_free_gb": 0}))
    env = dict(os.environ, GAMESLOT_DIR=str(tmp), GAMESLOT_POLL="0.05")
    procs = [subprocess.Popen([sys.executable, "-I", str(SRC), "acquire", "--owner", f"p{i}", "--wait", "0"],
                              env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True) for i in range(6)]
    rcs = []
    for p in procs:
        p.communicate(timeout=60)
        rcs.append(p.returncode)
    check(sorted(rcs) == [0, 3, 3, 3, 3, 3], f"exactly one racer may win the single slot: {rcs}")
    st = subprocess.run([sys.executable, "-I", str(SRC), "status"], env=env, capture_output=True, text=True, timeout=60)
    data = json.loads(st.stdout)
    check(data["max_slots"] == 1 and [s["free"] for s in data["slots"]] == [False], "status shows the held slot")


def check_real_standins_pid_scoped_kill(mut):
    """Two harmless stand-in processes. Taking over an expired lease kills only the registered one."""
    if os.name != "nt":
        print("     (skipped: stand-in kill check needs Windows)")
        return
    e = Env(2, None, min_free=0)
    import importlib
    real = importlib.util.module_from_spec(importlib.util.spec_from_file_location("gs_real", SRC))
    real.__spec__.loader.exec_module(real)
    real.avail_gb = lambda: 99.0
    code = "import time; time.sleep(120)"
    a = subprocess.Popen([sys.executable, "-I", "-c", code])
    b = subprocess.Popen([sys.executable, "-I", "-c", code])
    try:
        buf = io.StringIO()
        with redirect_stdout(buf):
            check(real.main(["acquire", "--owner", "OLD", "--ttl", "1"]) == 0, "acquire")
            check(real.main(["register-pid", "--owner", "OLD", "--pid", str(a.pid)]) == 0, "register the stand-in")
        slot = Path(os.environ["GAMESLOT_DIR"]) / "slots" / "slot-0.json"
        d = json.loads(slot.read_text())
        check(d["procs"] and d["procs"][0]["created"] > 0 and d["procs"][0]["name"], f"identity recorded: {d['procs']}")
        d["expires"] = time.time() - 1
        slot.write_text(json.dumps(d))
        with redirect_stdout(buf):
            check(real.main(["acquire", "--owner", "NEW"]) == 0, "takeover")
        for _ in range(100):
            if a.poll() is not None:
                break
            time.sleep(0.1)
        check(a.poll() is not None, "the registered stand-in must be gone after the takeover")
        check(b.poll() is None, "the unregistered stand-in (same image name) must still be running")
    finally:
        for p in (a, b):
            if p.poll() is None:
                p.kill()
            p.wait()


# ---------------------------------------------------------------------------------------------- twins
def m_no_exclusion(m, e):
    m._try_slot = lambda i, owner, ttl, now: True


def m_no_rollback(m, e):
    m._give_back = lambda idx: None


def m_never_expires(m, e):
    m._live = lambda lease, now: bool(lease)


def m_always_expired(m, e):
    m._live = lambda lease, now: False


def m_no_queue(m, e):
    m._waiters = lambda now, exclude: []


def m_no_ram_guard(m, e):
    m.min_free_gb = lambda: 0.0


def m_blanket_kill(m, e):
    m.kill_registered = lambda procs: [m.kill_pid(p) for p in e.procs.named("game.exe")]


def m_kill_without_identity(m, e):
    def kill_any_recorded(procs):
        for rec in procs:
            if m.process_info(rec["pid"]):
                m.kill_pid(rec["pid"])
    m.kill_registered = kill_any_recorded


def m_no_name_check(m, e):
    def kill_if_same_time(procs):
        for rec in procs:
            info = m.process_info(rec["pid"])
            if info and info["created"] == rec["created"]:
                m.kill_pid(rec["pid"])
    m.kill_registered = kill_if_same_time


def m_lock_always_granted(m, e):
    m.try_lock = lambda timeout_s: True


def m_no_stale_break(m, e):
    m.LOCK_STALE_S = 10 ** 9


def m_no_pid_record(m, e):
    m.register_pid = lambda owner, pid: 1


def m_release_any(m, e):
    real = m.release

    def release_all(owner):
        n = real(owner)
        return n or 1
    m.release = release_all


REAL = [check_distinct_slots_then_busy, check_exclusive_takes_every_slot, check_all_or_nothing,
        check_reacquire_renews, check_expired_lease_is_taken_over, check_live_lease_is_never_taken_over_or_killed,
        check_fifo_waiter_not_starved, check_wait_blocks_until_released, check_ram_guard, check_ram_guard_edges,
        check_default_slots_is_two, check_register_and_release, check_takeover_kills_only_registered,
        check_pid_reuse_is_not_killed, check_state_lock_is_exclusive, check_stale_lock_is_broken,
        check_concurrent_cli_only_one_winner, check_real_standins_pid_scoped_kill]

TWINS = [
    (check_distinct_slots_then_busy, "no_exclusion", m_no_exclusion),
    (check_all_or_nothing, "no_rollback", m_no_rollback),
    (check_expired_lease_is_taken_over, "never_expires", m_never_expires),
    (check_live_lease_is_never_taken_over_or_killed, "always_expired", m_always_expired),
    (check_fifo_waiter_not_starved, "no_queue", m_no_queue),
    (check_ram_guard, "no_ram_guard", m_no_ram_guard),
    (check_takeover_kills_only_registered, "blanket_kill", m_blanket_kill),
    (check_pid_reuse_is_not_killed, "kill_without_identity", m_kill_without_identity),
    (check_pid_reuse_is_not_killed, "no_name_check", m_no_name_check),
    (check_state_lock_is_exclusive, "lock_always_granted", m_lock_always_granted),
    (check_stale_lock_is_broken, "no_stale_break", m_no_stale_break),
    (check_register_and_release, "release_by_anyone", m_release_any),
    (check_register_and_release, "no_pid_record", m_no_pid_record),
]


def main():
    bad, n = [], 0
    try:
        for fn in REAL:
            n += 1
            try:
                fn(None)
                print("ok    ", fn.__name__)
            except Exception:  # noqa: BLE001
                bad.append(fn.__name__)
                print("FAIL  ", fn.__name__)
                traceback.print_exc()
        for fn, name, mut in TWINS:
            n += 1
            try:
                fn(mut)
            except AssertionError:
                print("ok     twin %s[%s] caught" % (fn.__name__, name))
            except Exception:  # noqa: BLE001  (a crash is not a caught defect)
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] crashed instead of failing a check" % (fn.__name__, name))
                traceback.print_exc()
            else:
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] NOT caught" % (fn.__name__, name))
    finally:
        os.environ.pop("GAMESLOT_DIR", None)
        os.environ.pop("GAMESLOT_POLL", None)
        for t in TMPS:
            shutil.rmtree(t, ignore_errors=True)
    print(f"\n{len(REAL)} checks, {len(TWINS)} twins, {len(bad)} failed")
    if bad:
        print("FAILED:", ", ".join(bad))
        return 1
    print("ALL OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
