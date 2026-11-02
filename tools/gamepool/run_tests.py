"""Tests for the game pool tools, then a mutation twin for each guard. Prints ALL OK only when every
check passes and every twin is caught.

    python -I tools/gamepool/run_tests.py

No real game is ever started. The "game" is tests/fake_game.py: a small Python script that can hold a named
mutex, look for a window class, write a ready file, listen on a port or crash, like the games these tools
must cope with. It shows no visible window. Every process a check starts is either killed by the code under
test (only its own pids) or by the check's own cleanup, and a decoy process with the same image name is kept
alive next to them to prove that nothing else is ever killed.

How twins work: a check runs once against the real code, then again with one guard broken on purpose (for
example "kill every process with the game's name"). A check that still passes against the broken code is not
testing anything, and the run fails.

Windows only (the tools drive Win32 processes, mutexes and windows).
"""
import contextlib
import ctypes
import hashlib
import io
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import uuid
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
if os.name != "nt":
    print("SKIPPED: these tools and tests need Windows")
    sys.exit(2)

from gplib import adapter as adapter_mod, client, config, crashlog, daemon, fakes, inifile, instances, jobqueue  # noqa: E402
from gplib import launcher, lowcost, pairs as pairs_mod, poolcore as core, smoke, winproc  # noqa: E402
from gplib.backend import RealBackend, RealSlots  # noqa: E402
from gplib.config import Config, ConfigError  # noqa: E402

FAKE_GAME = HERE / "tests" / "fake_game.py"
EXAMPLE = HERE / "examples" / "fnv.json"
GAMESLOT = HERE.parent / "gameslot" / "gameslot.py"
PROC = winproc.process_info(os.getpid())["name"]        # image name of this Python: the stand-ins are the same image
TMPS = []
SPAWNED = []
RUN_TAG = "gptest" + uuid.uuid4().hex          # unique per run: marks every stand-in this run starts, so cleanup never needs a name match
REAL_STOP_OWNER = launcher.stop_owner
_UNDO = []


# ---------------------------------------------------------------------------------------------- harness helpers
def patch(obj, name, value):
    """Break something on purpose for a twin. Undone after the twin ran."""
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


def raises(exc, fn, *a, **k):
    try:
        fn(*a, **k)
    except exc:
        return True
    return False


def tmpdir(prefix="gp-"):
    p = Path(tempfile.mkdtemp(prefix=prefix))
    TMPS.append(p)
    return p


@contextlib.contextmanager
def env(**kv):
    old = {k: os.environ.get(k) for k in kv}
    os.environ.update({k: str(v) for k, v in kv.items()})
    try:
        yield
    finally:
        for k, v in old.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def tree_hashes(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted(Path(root).rglob("*")) if p.is_file()}


def spawn_decoy():
    """A process with the same image name as the 'game' that does not belong to the tool under test."""
    p = subprocess.Popen([sys.executable, "-I", "-c", "import time; time.sleep(90)"])
    SPAWNED.append(p)
    return p


def kill_proc(p):
    if p.poll() is None:
        p.kill()
    p.wait()


def wait_for(fn, timeout=10.0, step=0.05):
    end = time.time() + timeout
    while time.time() < end:
        if fn():
            return True
        time.sleep(step)
    return fn()


def make_world(game_args=(), ready=None, spawns=False, extra=None):
    """A fake install, an 'owner' settings folder and a config for them. Returns (cfg, tmp)."""
    tmp = tmpdir("gp-world-")
    g = tmp / "game"
    (g / "data" / "mods").mkdir(parents=True)
    (g / "data" / "asset.bin").write_bytes(b"A" * 200_000)
    (g / "data" / "mods" / "readme.txt").write_text("hello")
    (g / "main.dat").write_bytes(b"MAIN" * 1000)
    (g / "Game.ini").write_text("[General]\r\nbPortable=0\r\n", encoding="latin-1")
    (g / "Prefs.ini").write_text("[Display]\r\nfFps=60\r\n", encoding="latin-1")
    (g / "saves").mkdir()
    (g / "saves" / "old.sav").write_bytes(b"SAVE")
    (g / "plugins").mkdir()
    (g / "plugins" / "old.dll").write_bytes(b"DLL")
    (g / "game.log").write_text("log of the original install")
    shutil.copy2(FAKE_GAME, g / "fake_game.py")
    owner = tmp / "owner"
    owner.mkdir()
    (owner / "Game.ini").write_text("[General]\r\nbPortable=0\r\nsOwnerSetting=keep\r\n[Display]\r\niSize W=1920\r\n", encoding="latin-1")
    (owner / "owner.sav").write_bytes(b"OWNER SAVE")
    args = ["-I", "fake_game.py", "--tag", RUN_TAG, "--i", "{i}", "--log", "game.log", "--ready-file", "ready.flag"] + list(game_args)
    raw = {
        "name": "Fake Game", "game_dir": str(g), "instances_root": str(tmp / "inst"), "process_name": PROC,
        "launch": {"exe": sys.executable, "args": args, "window": "minimized", "spawns_game": spawns,
                   "stagger_s": 0.0, "child_wait_s": 20, "watch_s": 1.0},
        "link": {"per_instance_files": ["Game.ini", "Prefs.ini"], "empty_dirs": ["saves"], "per_instance_dirs": ["plugins"],
                 "pinned_files": ["main.dat"]},
        "config_files": {"Game.ini": {"base": str(owner / "Game.ini"), "edits": [["General", "bPortable", "1"], ["General", "iInstance", "{i}"]]},
                         "Prefs.ini": {"edits": []}},
        "protected_paths": [str(owner)],
        "ready": ready or {"type": "file", "path": "ready.flag", "timeout_s": 30, "poll_s": 0.05},
        "crash": {"logs": ["game.log"], "markers": ["CRASH"], "wer_match": "no-such-application-xyz"},
        "collect_logs": ["*.log"],
        "lowcost": {"post_checks": [["Game.ini", "General", "bPortable", "1"]], "rows": [
            {"file": "Game.ini", "section": "General", "key": "bPortable", "value": "1", "why": "private", "status": "required"},
            {"file": "Game.ini", "section": "Display", "key": "iSize W", "value": "640", "why": "small", "status": "standard"},
            {"file": "Prefs.ini", "section": "Display", "key": "fFps", "value": "30", "why": "cap", "status": "standard"},
            {"file": "Prefs.ini", "section": "Display", "key": "iGrid", "value": "3", "why": "fewer cells", "status": "opt-in",
             "opt_in": "grids", "value_from_option": True, "option_regex": "3|5|7"}]},
        "pool": {"tick_s": 0.5},
    }
    raw.update(extra or {})
    return Config(raw, tmp), tmp


def slot_env(tmp, slots=3):
    d = tmp / "slots-state"
    d.mkdir(exist_ok=True)
    (d / "gameslot_config.json").write_text(json.dumps({"slots": slots, "min_free_gb": 0}))
    return env(GAMESLOT_DIR=d, GAMESLOT_POLL="0.05", GAMEPOOL_DIR=tmp / "gp-state")


def slot_status():
    cp = subprocess.run([sys.executable, "-I", str(GAMESLOT), "status"], capture_output=True, text=True)
    return json.loads(cp.stdout)


def sweep(cfg, owners=("pool", "gamepool", "smoke", "probe", "t")):
    """Cleanup: kill whatever the tool recorded that is still running (real stop code, never name based)."""
    for o in owners:
        REAL_STOP_OWNER(cfg, o)


# ---------------------------------------------------------------------------------------------- config checks
def check_example_config_loads_and_is_clean(mut):
    hook(mut)
    cfg = Config.load(str(EXAMPLE))
    check(cfg.process_name == "falloutnv.exe" and cfg["launch"]["spawns_game"], "the New Vegas example loads")
    check(raises(ConfigError, cfg.require_dirs), "the example still has <game dir> and must refuse to run")
    text = EXAMPLE.read_text(encoding="utf-8")
    check("<game dir>" in text, "the example uses a placeholder, not a real path")
    check("C:\\\\Users" not in text and "C:/Users" not in text and "\u2014" not in text, "no personal path, no em dash")
    check(len(cfg["lowcost"]["rows"]) > 30, "the low-cost profile rows are there")


def check_config_refuses_bad_input(mut):
    hook(mut)
    raw = {"name": "x", "link": {"per_instance_files": ["a.ini"]}, "config_files": {"b.ini": {}}}
    check(raises(ConfigError, Config, raw), "a config file that is not private per copy must be refused")
    check(raises(ConfigError, Config, {"name": "x", "bogus": 1}), "unknown key refused")
    check(raises(ConfigError, Config, {"launch": {"window": "huge"}}), "bad window mode refused")
    check(raises(ConfigError, Config, {"ready": {"type": "telepathy"}}), "bad ready type refused")
    check(raises(ConfigError, Config, {"lowcost": {"rows": [{"file": "z.ini", "section": "s", "key": "k", "value": "1"}]}}),
          "a low-cost row for a file that is not private is refused")
    check(raises(ConfigError, Config.load, str(tmpdir() / "missing.json")), "missing file")


def check_roots_must_be_separate(mut):
    cfg, tmp = make_world()
    hook(mut)
    check(raises(ConfigError, config.check_roots, Config(dict(cfg.raw, instances_root=str(Path(cfg["game_dir"]) / "inside")), tmp)),
          "instances inside the install must be refused")
    parent = Config(dict(cfg.raw, instances_root=str(tmp)), tmp)         # the install is inside the instances folder
    check(raises(ConfigError, instances.provision, parent, 0), "an install inside the instances folder must be refused")
    check(not (tmp / "0").exists(), "nothing was created")


def check_ini_editing(mut):
    hook(mut)
    t = "[General]\r\nbA=1\r\n\r\n[Display]\r\nIsize W=10\r\n"
    t2 = inifile.ini_set(t, "general", "ba", "2")
    check(inifile.ini_get(t2, "General", "bA") == "2" and "\r\n" in t2 and "\n" not in t2.replace("\r\n", ""), "case-insensitive set keeps CRLF")
    t3 = inifile.ini_set(t2, "New", "k", "v")
    check(inifile.ini_get(t3, "New", "k") == "v" and inifile.ini_get(t3, "Display", "Isize W") == "10", "new section appended")
    t4 = inifile.ini_set(t3, "General", "bB", "x")
    check(t4.index("bB=x") < t4.index("[Display]"), "a new key lands inside its section")
    check(inifile.ini_get("", "A", "b") is None, "missing key")


# ---------------------------------------------------------------------------------------------- instance checks
def check_provision_private_files_and_untouched_install(mut):
    cfg, tmp = make_world()
    hook(mut)
    before_game, before_owner = tree_hashes(cfg.game_dir), tree_hashes(tmp / "owner")
    m = instances.provision(cfg, 0, role="host")
    d = cfg.instance_dir(0)
    check(os.stat(d / "data" / "asset.bin").st_nlink >= 2 and sha(d / "data" / "asset.bin") == sha(cfg.game_dir / "data" / "asset.bin"),
          "big files are hard links (no extra disk)")
    check(os.stat(d / "Game.ini").st_nlink == 1 and os.stat(d / "Prefs.ini").st_nlink == 1, "per-instance files are real private files")
    ini = (d / "Game.ini").read_text(encoding="latin-1")
    check(inifile.ini_get(ini, "General", "iInstance") == "0" and inifile.ini_get(ini, "General", "sOwnerSetting") == "keep",
          "ini starts from the owner's file and gets the edits")
    check(list((d / "saves").iterdir()) == [] and list((d / "plugins").iterdir()) == [], "saves and plugin folders start empty")
    check(not (d / "game.log").exists(), "the install's runtime log is not shared")
    check(m["pinned"]["main.dat"] == sha(cfg.game_dir / "main.dat"), "pinned hash recorded")
    check(tree_hashes(cfg.game_dir) == before_game, "the original install must not change")
    check(tree_hashes(tmp / "owner") == before_owner, "the owner's own settings and saves must not change")


def check_reprovision_never_writes_through_old_links(mut):
    cfg, tmp = make_world()
    d = cfg.instance_dir(0)
    d.mkdir(parents=True)
    os.link(cfg.game_dir / "Game.ini", d / "Game.ini")        # an old copy of the instance had this file hard-linked
    before = tree_hashes(cfg.game_dir)
    hook(mut)
    instances.provision(cfg, 0)
    check(tree_hashes(cfg.game_dir) == before, "re-provisioning must not write through a link into the install")
    check(os.stat(d / "Game.ini").st_nlink == 1, "the file is private now")


def check_provision_is_idempotent_and_destroy_is_safe(mut):
    cfg, tmp = make_world()
    hook(mut)
    before = tree_hashes(cfg.game_dir)
    instances.provision(cfg, 0)
    m2 = instances.provision(cfg, 0)
    check(m2["link_counts"]["existing"] > 0 and m2["link_counts"]["hardlink"] == 0, f"second run links nothing new: {m2['link_counts']}")
    check(instances.provisioned(cfg, 0), "provisioned")
    instances.destroy(cfg, 0)
    check(not cfg.instance_dir(0).exists(), "instance removed")
    check(tree_hashes(cfg.game_dir) == before, "destroying an instance leaves the install intact")
    instances.destroy(cfg, 5)      # nothing to remove: no error


def check_copy_mode(mut):
    cfg, tmp = make_world()
    hook(mut)
    instances.provision(cfg, 0, modes=("copy",))
    d = cfg.instance_dir(0)
    check(os.stat(d / "data" / "asset.bin").st_nlink == 1 and sha(d / "data" / "asset.bin") == sha(cfg.game_dir / "data" / "asset.bin"),
          "copy mode makes independent copies")
    mode, free = instances.disk_check(cfg, ("hardlink", "symlink", "copy"))
    check(mode in ("hardlink", "symlink", "copy") and free > 0, "link probe")


def check_disk_check_refuses(mut):
    cfg, tmp = make_world()
    hook(mut)
    import collections
    usage = collections.namedtuple("usage", "total used free")
    real = shutil.disk_usage
    shutil.disk_usage = lambda p: usage(10 * 2**30, 9 * 2**30 + 500 * 2**20, 500 * 2**20)
    try:
        check(raises(ConfigError, instances.disk_check, cfg, ("hardlink",)), "500 MiB free is not enough")
    finally:
        shutil.disk_usage = real


def check_pins_detect_changes(mut):
    cfg, tmp = make_world()
    instances.provision(cfg, 0)
    hook(mut)
    check(instances.verify_pins(cfg, 0) == [], "clean instance")
    p = cfg.instance_dir(0) / "main.dat"
    p.unlink()
    check(instances.verify_pins(cfg, 0) == ["main.dat: missing"], "missing pinned file reported")
    p.write_bytes(b"different")                  # a new private file, not a write through the link
    check(instances.verify_pins(cfg, 0) == ["main.dat: hash changed"], "changed pinned file reported")
    check(instances.verify_pins(cfg, 0, ["nope.txt"])[-1] == "nope.txt: missing", "required extra file")


def check_guard_write(mut):
    cfg, tmp = make_world()
    instances.provision(cfg, 0)
    hook(mut)
    d = cfg.instance_dir(0)
    instances.guard_write(cfg, d / "Game.ini")                          # private file: allowed
    for bad, why in ((tmp / "owner" / "Game.ini", "the owner's folder (outside the instances)"),
                     (cfg.game_dir / "Game.ini", "the install"),
                     (d / "data" / "asset.bin", "a hard-linked file")):
        check(raises(instances.UnsafeTarget, instances.guard_write, cfg, bad), f"must refuse {why}")
    prot = Config(dict(cfg.raw, protected_paths=[str(d / "saves")]), tmp)
    check(raises(instances.UnsafeTarget, instances.guard_write, prot, d / "saves" / "x.ini"), "must refuse a protected folder")
    link = d / "sym.ini"
    try:
        os.symlink(d / "Game.ini", link)
    except OSError:
        return                                                            # no symlink right on this PC
    check(raises(instances.UnsafeTarget, instances.guard_write, cfg, link), "must refuse a symlink")


def check_lowcost(mut):
    cfg, tmp = make_world()
    instances.provision(cfg, 0)
    hook(mut)
    d = cfg.instance_dir(0)
    owner_before = tree_hashes(tmp / "owner")
    lowcost.apply(cfg, d)
    gi, pr = (d / "Game.ini").read_text(encoding="latin-1"), (d / "Prefs.ini").read_text(encoding="latin-1")
    check(inifile.ini_get(gi, "Display", "iSize W") == "640" and inifile.ini_get(pr, "Display", "fFps") == "30", "profile applied")
    check(inifile.ini_get(pr, "Display", "iGrid") is None, "an opt-in row stays off unless asked for")
    before = (gi, pr)
    lowcost.apply(cfg, d)
    check(before == ((d / "Game.ini").read_text(encoding="latin-1"), (d / "Prefs.ini").read_text(encoding="latin-1")), "idempotent")
    lowcost.apply(cfg, d, {"grids": "5"})
    check(inifile.ini_get((d / "Prefs.ini").read_text(encoding="latin-1"), "Display", "iGrid") == "5", "opt-in applied with its value")
    check(raises(ValueError, lowcost.rows, cfg, {"grids": "4"}), "an option value that fails the regex is refused")
    check(raises(ValueError, lowcost.rows, cfg, {"nonsense": "1"}), "an unknown option is refused")
    check("| Game.ini |" in lowcost.render_doc(cfg), "doc table")
    ver = lowcost.verify(cfg)
    check("Game.ini:Display:iSize W" in ver["present"] and "Prefs.ini:Display:fFps" in ver["absent"], f"verify reads the base files: {ver}")
    # guards
    os.link(d / "Prefs.ini", d / "Prefs.link")
    check(raises(instances.UnsafeTarget, lowcost.apply, cfg, d), "a hard-linked settings file must be refused")
    (d / "Prefs.link").unlink()
    other = tmp / "elsewhere"
    other.mkdir()
    (other / "Game.ini").write_text("[General]\r\n")
    (other / "Prefs.ini").write_text("[Display]\r\n")
    check(raises(instances.UnsafeTarget, lowcost.apply, cfg, other), "a folder outside the instances must be refused")
    check(tree_hashes(tmp / "owner") == owner_before, "the owner's files are untouched")


# ---------------------------------------------------------------------------------------------- readiness probes
def make_pipe(name):
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CreateNamedPipeW.restype = ctypes.c_void_p
    k32.CreateNamedPipeW.argtypes = [ctypes.c_wchar_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong,
                                     ctypes.c_ulong, ctypes.c_ulong, ctypes.c_void_p]
    k32.CloseHandle.argtypes = [ctypes.c_void_p]
    h = k32.CreateNamedPipeW(name, 3, 0, 1, 512, 512, 0, None)
    check(h not in (None, ctypes.c_void_p(-1).value), "could not create the test pipe")
    return lambda: k32.CloseHandle(h)


def check_readiness_probes(mut):
    cfg, tmp = make_world()
    instances.provision(cfg, 0)
    hook(mut)
    d = cfg.instance_dir(0)
    probe = adapter_mod.probe

    def with_ready(r):
        return Config(dict(cfg.raw, ready=r), tmp)
    f = with_ready({"type": "file", "path": "flag-{i}.txt"})
    check(not probe(f, 0, os.getpid()), "file probe: not ready before the file exists")
    (d / "flag-0.txt").write_text("x")
    check(probe(f, 0, os.getpid()), "file probe: ready when it exists")
    lg = with_ready({"type": "log_line", "path": "x.log", "text": "READY NOW"})
    (d / "x.log").write_text("starting\n")
    check(not probe(lg, 0, 1), "log_line probe: not ready without the text")
    (d / "x.log").write_text("starting\nREADY NOW\n")
    check(probe(lg, 0, 1), "log_line probe: ready with the text")
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    tp = with_ready({"type": "tcp_port", "base_port": port - 1})
    check(not probe(tp, 1, 1), "tcp probe: closed port is not ready")
    srv = socket.socket()
    srv.bind(("127.0.0.1", port))
    srv.listen(1)
    try:
        check(probe(tp, 1, 1), "tcp probe: open port is ready (base port + instance)")
    finally:
        srv.close()
    name = r"\\.\pipe\gptest-" + uuid.uuid4().hex
    pp = with_ready({"type": "named_pipe", "name": name})
    check(not probe(pp, 0, 1), "pipe probe: missing pipe is not ready")
    close = make_pipe(name)
    try:
        check(probe(pp, 0, 1), "pipe probe: existing pipe is ready")
    finally:
        close()
    pr = with_ready({"type": "process"})
    check(probe(pr, 0, os.getpid()), "process probe: a live pid is ready")
    dead = subprocess.Popen([sys.executable, "-I", "-c", "pass"])
    dead.wait()
    check(not probe(pr, 0, dead.pid), "process probe: an exited pid is not ready")


def check_adapter_wait_ready_and_custom_adapter(mut):
    cfg, tmp = make_world()
    ad = adapter_mod.Adapter(Config(dict(cfg.raw, ready={"type": "process", "timeout_s": 5, "poll_s": 0.05}), tmp))
    calls = []
    ad.after_ready = lambda i, pid: calls.append((i, pid))
    hook(mut, adapter_cls=adapter_mod.Adapter)
    check(ad.wait_ready(0, os.getpid(), lambda p: True, 2.0) and calls == [(0, os.getpid())], "ready once, after_ready runs once")
    ad.probe = lambda i, pid: False
    t0 = time.time()
    check(not ad.wait_ready(0, 123, lambda p: True, 0.4) and time.time() - t0 < 3, "times out when never ready")
    check(not ad.wait_ready(0, 123, lambda p: False, 5.0), "a dead process is never ready")
    src = tmp / "my_adapter.py"
    src.write_text("from gplib.adapter import Adapter\n\nclass Mine(Adapter):\n    def probe(self, i, pid):\n        return i == 7\n\n"
                   "class NotAnAdapter:\n    def __init__(self, cfg):\n        pass\n")
    c2 = Config(dict(cfg.raw, adapter="my_adapter.py:Mine"), tmp)
    a2 = adapter_mod.load_adapter(c2)
    check(isinstance(a2, adapter_mod.Adapter) and a2.probe(7, 1) and not a2.probe(1, 1), "a custom adapter is loaded from the config")
    check(raises(ConfigError, adapter_mod.load_adapter, Config(dict(cfg.raw, adapter="my_adapter.py:NotAnAdapter"), tmp)),
          "a class that is not an Adapter is refused")
    check(raises(ConfigError, adapter_mod.load_adapter, Config(dict(cfg.raw, adapter="nope.py:X"), tmp)), "missing adapter file refused")


# ---------------------------------------------------------------------------------------------- real stand-in processes
def check_launch_and_pid_scoped_kill(mut):
    cfg, tmp = make_world()
    with slot_env(tmp):
        for i in (0, 1):
            instances.provision(cfg, i)
        hook(mut)
        decoy = spawn_decoy()
        try:
            ids = launcher.slot_acquire("t", ttl=60, wait=5, slots=2)
            check(ids == [0, 1], f"slots {ids}")
            pids = {}
            for i in ids:
                pid, popen = launcher.launch_instance_ex(cfg, i, "t")
                pids[i] = pid
                check(wait_for(lambda: (cfg.instance_dir(i) / "ready.flag").exists()), f"stand-in {i} became ready")
            check(all(winproc.pid_alive(p) for p in pids.values()), "both copies run side by side")
            check(all(launcher.is_recorded_process(cfg, p, "t") for p in pids.values()), "recorded with identity")
            reg = [rec for s in slot_status()["slots"] if s["lease"] for rec in s["lease"]["procs"]]
            check(sorted({r["pid"] for r in reg}) == sorted(pids.values()), f"pids registered with the game slot: {reg}")
            # a tampered record (pid reuse) is not killed
            st = launcher.load_state(cfg, "t")
            real_created = st["procs"]["0"]["created"]
            st["procs"]["0"]["created"] = real_created + 1
            launcher.save_state(cfg, st, "t")
            killed = REAL_STOP_OWNER(cfg, "t")
            check(killed == [pids[1]], f"only the matching record is killed; killed {killed}")
            check(winproc.pid_alive(pids[0]), "a record that no longer matches (pid reuse) must not be killed")
            check(wait_for(lambda: not winproc.pid_alive(pids[1])), "the recorded copy is gone")
            check(decoy.poll() is None, "the decoy (same image name, not ours) must still run")
            winproc.kill_pid(pids[0])                                  # test cleanup: we started it
            check(launcher.slot_release("t") == "RELEASED", "slots released")
        finally:
            sweep(cfg)
            launcher.slot_release("t")
            kill_proc(decoy)


def check_loader_child_found_by_process_tree(mut):
    cfg, tmp = make_world(game_args=["--spawn-child"], spawns=True)
    with slot_env(tmp):
        instances.provision(cfg, 0)
        hook(mut)
        decoy = spawn_decoy()
        try:
            launcher.slot_acquire("t", ttl=60, wait=5, slots=1)
            pid, popen = launcher.launch_instance_ex(cfg, 0, "t")
            late_decoy = spawn_decoy()          # another process of the same name appears right after the loader
            check(pid != popen.pid and pid not in (decoy.pid, late_decoy.pid), "the real game process is found, not the loader or a stranger")
            check(pid in winproc.descendants(popen.pid, PROC), "it is a descendant of the loader")
            check(wait_for(lambda: (cfg.instance_dir(0) / "ready.flag").exists()), "the child is the running game")
            check(REAL_STOP_OWNER(cfg, "t") == [pid], "stop kills the child that was recorded")
            check(decoy.poll() is None and late_decoy.poll() is None, "strangers are untouched")
        finally:
            sweep(cfg)
            launcher.slot_release("t")
            kill_proc(decoy)
            if "late_decoy" in locals():
                kill_proc(late_decoy)


def run_up(cfg, ids, owner="t"):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        rc = launcher.start_instances(cfg, ids, owner, adapter_mod.Adapter(cfg), 0.0, 20.0)
    return rc, out.getvalue(), err.getvalue()


def check_single_instance_games(mut):
    """A fixed-name mutex or window class stops the second copy; a per-process name (what a fix inside the game does) lets both run."""
    token = uuid.uuid4().hex[:10]
    for flavour, plain, fixed in (("mutex", ["--mutex", "GPMX" + token], ["--mutex", "GPMX" + token, "--rename-mutex"]),
                                  ("window", ["--window-class", "GPWC" + token], ["--window-class", "GPWC" + token, "--unique-window-class"])):
        cfg, tmp = make_world(game_args=plain)
        cfg_fixed, tmp2 = make_world(game_args=fixed)
        with slot_env(tmp):
            for c in (cfg, cfg_fixed):
                for i in (0, 1):
                    instances.provision(c, i)
            hook(mut)
            try:
                rc, out, err = run_up(cfg, [0, 1], "t")
                check(rc == 3, f"{flavour}: the second copy must be reported as failed, rc={rc}")
                check("exited within" in err and "exit code" in err, f"{flavour}: the early exit is reported with its exit code: {err!r}")
                log = (cfg.instance_dir(1) / "game.log").read_text()
                check("already running" in log, f"{flavour}: the copy itself says why it stopped: {log!r}")
                check(winproc.pid_alive(launcher.load_state(cfg, "t")["procs"]["0"]["pid"]), f"{flavour}: the first copy keeps running")
            finally:
                REAL_STOP_OWNER(cfg, "t")
            try:
                rc, out, err = run_up(cfg_fixed, [0, 1], "t")
                check(rc == 0, f"{flavour}: with a per-process name both copies run: rc={rc} {err}")
                pids = [r["pid"] for r in launcher.load_state(cfg_fixed, "t")["procs"].values()]
                check(len(pids) == 2 and all(winproc.pid_alive(p) for p in pids), f"{flavour}: two copies alive at once")
                if flavour == "mutex":
                    check(winproc.mutex_exists("GPMX" + token + "-" + str(pids[0])), "the renamed mutex exists")
                else:
                    check(winproc.find_window("GPWC" + token + "-" + str(pids[0])) != 0, "the unique window class exists")
            finally:
                REAL_STOP_OWNER(cfg_fixed, "t")
    check(winproc.find_window("GPWC" + token) == 0 and not winproc.mutex_exists("GPMX" + token), "nothing left behind")


def check_crash_is_visible_in_crashlog(mut):
    cfg, tmp = make_world(game_args=["--crash-after", "0.2"])
    with slot_env(tmp):
        instances.provision(cfg, 0)
        hook(mut)
        d = cfg.instance_dir(0)
        argv, cwd, envv = launcher.launch_command(cfg, 0)
        p = subprocess.Popen(argv, cwd=cwd, env=envv, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        SPAWNED.append(p)
        check(p.wait(timeout=20) == 1, "the stand-in exits with code 1")
        s = crashlog.collect_crash_report(cfg, tmp / "crash", instance=0, event_query=lambda m: ["stub event"])
        check(s["files"] == ["game.log"] and any("CRASH" in l for l in s["marker_lines"]), f"marker lines found: {s}")
        check(s["windows_crash_records"] == ["stub event"] and (tmp / "crash" / "summary.json").exists(), "summary written")
        empty = crashlog.collect_crash_report(cfg, tmp / "crash2", instance=1, event_query=lambda m: [])
        check(empty["files"] == [], "missing logs are fine")
        got = crashlog._wer_events(1, "no'; Write-Output pwned; '")
        check(got == [], f"a hostile pattern must not run as code: {got}")


def check_jobs(mut):
    cfg, tmp = make_world()
    with slot_env(tmp):
        for i in (0, 1):
            instances.provision(cfg, i)
        hook(mut)
        runs = tmp / "runs"
        code = "import os,sys; print('INST=' + os.environ['GAMEPOOL_INSTANCES']); open(os.path.join(sys.argv[1], 'x.log'), 'w').write('hi')"
        # the command writes a log into instance 0 so the job can collect it
        r = launcher.run_job(cfg, "j1", 1, [sys.executable, "-I", "-c", code, str(cfg.instance_dir(0))], owner="job", runs_dir=runs, timeout_s=30)
        check(r["status"] == "pass" and (runs / "j1" / "stdout.txt").read_text().strip() == "INST=0", f"job passes with its instance in the env: {r}")
        check(r["logs"]["0"] == ["x.log"] and (runs / "j1" / "inst0" / "x.log").exists(), "the log written during the job is collected")
        check(all(s["free"] for s in slot_status()["slots"]), "slots are free after a passing job")
        r = launcher.run_job(cfg, "j2", 1, [sys.executable, "-I", "-c", "raise SystemExit(5)"], owner="job", runs_dir=runs, timeout_s=30)
        check(r["status"] == "fail" and r["returncode"] == 5, "failing job")
        check(all(s["free"] for s in slot_status()["slots"]), "slots are free after a failing job")
        r = launcher.run_job(cfg, "j3", 1, [sys.executable, "-I", "-c", "import time; time.sleep(30)"], owner="job", runs_dir=runs, timeout_s=1)
        check(r["status"] == "timeout", "timeout")
        check(all(s["free"] for s in slot_status()["slots"]), "slots are free after a timeout")
        (cfg.instance_dir(1) / "main.dat").unlink()
        r = launcher.run_job(cfg, "j4", 2, [sys.executable, "-I", "-c", "print(1)"], owner="job", runs_dir=runs)
        check(r["status"] == "pin_mismatch" and any("main.dat" in p for p in r["problems"]), f"pinned file check: {r}")
        check(all(s["free"] for s in slot_status()["slots"]), "slots are free after a pin mismatch")
        holder = launcher.slot_acquire("other", ttl=60, wait=2, slots=3)
        try:
            r = launcher.run_job(cfg, "j5", 1, [sys.executable, "-I", "-c", "print(1)"], owner="job", runs_dir=runs, slot_wait_s=0)
            check(r["status"] == "slot_busy", f"all slots held by someone else: {r}")
        finally:
            launcher.slot_release("other")
        r = launcher.run_job(cfg, "j6", 2, [sys.executable, "-I", "-c", "import os; print(os.environ['GAMEPOOL_INSTANCES'])"], owner="job",
                             runs_dir=runs, use_slot=False, pool=2, check_pins=False)
        check(r["status"] == "pass" and (runs / "j6" / "stdout.txt").read_text().strip() == "0,1", f"no-slot job leases instances by file lock: {r}")


def check_jobqueue_locks(mut):
    root = tmpdir()
    hook(mut)
    locks = jobqueue.acquire(root, 2, pool=2, job="a")
    try:
        h = jobqueue.holder(root, 0)
        check(jobqueue.instance_ids(locks) == [0, 1] and h and h["job"] == "a", f"a held instance is described: {h}")
        check(raises(jobqueue.LeaseTimeout, jobqueue.acquire, root, 1, pool=2, job="b", timeout_s=0.3), "a held instance cannot be leased twice")
    finally:
        for l in locks:
            l.release()
    check(jobqueue.holder(root, 0) is None, "free after release")
    with jobqueue.lease(root, 1, pool=2, job="c") as ids:
        check(ids == [0], "lowest free instance first")
    check(raises(ValueError, jobqueue.acquire, root, 3, pool=2), "more than the pool is an error")


# ---------------------------------------------------------------------------------------------- pool policy (in-memory fakes)
class Clock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def mk(size=2, mut=None, **kw):
    b, s, c = fakes.FakeBackend(), fakes.FakeSlots(), Clock()
    tmp = tmpdir("pooltest-")
    p = core.Pool(b, s, size=size, sync=True, clock=c, state_path=tmp / "state.json", **kw)
    hook(mut, pool=p, backend=b, slots=s)
    return p, b, s, c, tmp


def check_pool_exclusive(mut):
    p, b, *_ = mk(2, mut)
    p.up()
    l1, l2 = p.lease("a"), p.lease("b")
    check(l1["instances"] == [0] and l2["instances"] == [1], (l1, l2))
    check(raises(core.PoolBusy, p.lease, "c", 1, 900, 0.2), "a third lease must wait and fail while both are leased")
    p.release(l1["id"])
    check(p.lease("c")["instances"] == [0], "a released instance can be leased again")
    p.down()


def check_pool_pair_lease_order(mut):
    p, *_ = mk(2, mut)
    p.up()
    check(p.lease("x", n=2)["instances"] == [0, 1], "host (lowest id) first")
    check(raises(core.PoolError, p.lease, "y", 3), "more than the pool size is an error")
    p.down()


def check_pool_size_and_slots(mut):
    check(mk(0, mut)[0].size == 1, "a pool has at least one instance")
    p, b, s, *_ = mk(3, mut)
    check(raises(core.PoolError, p.up), "asking for more slots than exist fails loudly")
    check(not b.procs, "nothing was started")


def check_pool_recycle_after_k(mut):
    p, b, *_ = mk(1, mut, recycle_after=3)
    p.up()
    for _ in range(3):
        p.release(p.lease("t")["id"])
    check(b.starts == [0, 0], f"one recycle after 3 tests, starts={b.starts}")
    check(p.insts[0].gen == 2 and p.insts[0].tests == 0, "fresh instance")
    p.down()


def check_pool_dirty_recycles(mut):
    p, b, *_ = mk(1, mut)
    p.up()
    L = p.lease("t")
    p.mark_dirty(L["id"], 0, "loaded a foreign save")
    p.release(L["id"])
    check(b.starts == [0, 0], "a dirty instance must be restarted")
    L = p.lease("t")
    p.release(L["id"])
    check(b.starts == [0, 0], "a clean release must NOT restart")
    p.down()


def check_pool_expiry(mut):
    p, b, s, c, _ = mk(1, mut)
    p.up()
    p.lease("t", ttl_s=10)
    c.t += 5
    p.tick()
    check(p.leases, "a lease survives before its ttl")
    c.t += 6
    p.tick()
    check(not p.leases and p.stats["expired"] == 1, "an expired lease is reaped")
    check(b.starts == [0, 0], "an expired lease's instance is in an unknown state: recycle")
    check(b.unlocked, "the game side lock is released")
    p.down()


def check_pool_renew(mut):
    p, b, s, c, _ = mk(1, mut)
    p.up()
    L = p.lease("t", ttl_s=10)
    c.t += 8
    check(p.renew(L["id"]), "renew")
    c.t += 8
    p.tick()
    check(p.leases, "a renewed lease lives")
    p.down()


def check_pool_kills_only_own(mut):
    p, b, *_ = mk(2, mut)
    p.up()
    p.down()
    check(b.foreign == {900001, 900002}, f"foreign game processes were killed: {b.foreign}")
    check(not b.procs, "own processes are gone")


def check_pool_reap_previous_only_recorded(mut):
    p, b, s, c, tmp = mk(1, mut)
    p.up()
    pid = p.insts[0].pid
    p2 = core.Pool(b, fakes.FakeSlots(), size=1, sync=True, clock=c, state_path=tmp / "state.json")      # a daemon crash: a new pool, same machine
    hook(mut, pool=p2, backend=b, slots=None)
    check(pid in p2.reap_previous() and 900001 in b.foreign, "the recorded pid is reaped, the foreign process is not")
    check(pid not in b.procs, "reaped")


def check_pool_crash_restart(mut):
    p, b, *_ = mk(1, mut)
    p.up()
    b.crash(0)
    p.tick()
    check(p.insts[0].state == "warm" and p.insts[0].gen == 2, p.insts[0].view())
    L = p.lease("t")
    b.crash(0)
    p.tick()
    g = p.leases[L["id"]]["gens"]["0"]
    check(p.insts[0].gen != g and p.insts[0].state == "leased", "the lessee can see the restart")
    p.down()


def check_pool_dead_instance_never_leased(mut):
    p, b, *_ = mk(1, mut)
    p.up()
    b.crash(0)
    L = p.lease("t")
    check(b.alive(p.insts[0].pid) and L["gens"]["0"] == 2, "a dead warm instance is restarted before it is handed out")
    p.down()


def check_pool_unresponsive_restart(mut):
    p, b, *_ = mk(1, mut)
    p.up()
    b.unresponsive.add(0)
    old = p.insts[0].pid
    orig = b.start
    b.start = lambda i: (b.unresponsive.discard(i), orig(i))[1]      # the restart brings a working game
    p.tick()
    check(p.insts[0].pid != old and p.insts[0].gen == 2, "an unresponsive game is restarted")
    p.down()


def check_pool_memory_growth(mut):
    p, b, *_ = mk(1, mut, ws_growth_mb=500)
    p.up()
    b.grow(0, 1200 + 400)
    p.tick()
    check(b.starts == [0], "400 MB growth is under the 500 MB limit")
    b.grow(0, 1200 + 600)
    p.tick()
    check(b.starts == [0, 0], "600 MB growth recycles")
    p.down()


def check_pool_start_failure(mut):
    p, b, *_ = mk(1, mut)
    b.fail_start.add(0)
    check(raises(core.PoolError, p.up), "up fails loudly when an instance will not start")
    check(not b.procs, "a failed start leaves no process behind")


def check_pool_failed_start_stops_lease(mut):
    p, b, *_ = mk(1, mut, max_start_fails=2)
    p.up()
    b.fail_start.add(0)
    b.crash(0)
    p.tick()
    p.tick()
    try:
        p.lease("t", 1, 900, 0.3)
    except core.PoolBusy:
        raise AssertionError("lease waited for the timeout instead of failing at once")
    except core.PoolError:
        return
    raise AssertionError("lease must fail when nothing can start")


def check_pool_slot_renew_and_pids(mut):
    p, b, s, c, _ = mk(2, mut, slot_ttl_s=300)
    p.up()
    check(sorted(s.pids) == sorted(i.pid for i in p.insts.values()), "every launched pid is registered with the game slot")
    c.t += 150
    p.tick()
    check(s.renewed == 1, "the game slot lease is renewed before it expires")
    p.down()
    check(s.held == [], "slots freed on down")


# ---------------------------------------------------------------------------------------------- daemon, client, real slots
def check_daemon_and_client(mut):
    import secrets
    tmp = tmpdir("poold-")
    with env(GAMEPOOL_DIR=tmp):
        b, s = fakes.FakeBackend(), fakes.FakeSlots(available=(0, 1))
        pool = core.Pool(b, s, size=2, sync=True, state_path=tmp / "state.json")
        hook(mut, pool=pool, backend=b, slots=s)
        pool.up()
        srv = daemon.Server(pool, secrets.token_hex(4))
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        (tmp / "pool").mkdir()
        (tmp / "pool" / "pool.json").write_text(json.dumps({"port": srv.server_address[1], "token": srv.token}))
        try:
            with client.lease("t1", n=1, ttl_s=60) as L:
                check(L.instances == [0], "first lease gets instance 0")
                check(L.check()["state"] == "leased", "check passes while the instance is unchanged")
                check([x["state"] for x in client.status()["instances"]] == ["leased", "warm"], "status over the socket")
                L.dirty(0, "test")
            check(client.status()["leases"] == [], "released at the end of the block")
            check(b.starts.count(0) == 2, "a dirty instance is recycled through the daemon path")
            try:
                with client.lease("t2", n=1):
                    raise RuntimeError("boom")
            except RuntimeError:
                pass
            check(b.starts.count(0) + b.starts.count(1) >= 3, "an exception in the lessee marks the instance dirty")
            check(raises(client.PoolError, client.acquire, "x", 3, 5, 1), "n above the pool size is an error")
            L2 = client.acquire("t3", n=2)
            check(L2.instances == [0, 1], "a pair lease gets both, host first")
            b.crash(0)
            pool.tick()
            check(raises(client.PoolError, L2.check, 0), "a restart under a lease is reported to the lessee")
            L2.release()
            (tmp / "pool" / "pool.json").write_text(json.dumps({"port": srv.server_address[1], "token": "wrong"}))
            check(raises(client.PoolError, client.request, "status"), "a wrong token is rejected")
        finally:
            srv.shutdown()
            pool.down()


def check_client_when_pool_is_not_running(mut):
    hook(mut)
    with env(GAMEPOOL_DIR=tmpdir("poolnone-")):
        check(raises(client.PoolNotRunning, client.status), "no daemon: a clear error")


def check_real_slots_through_gameslot(mut):
    tmp = tmpdir("gslots-")
    hook(mut)
    with env(GAMESLOT_DIR=tmp, GAMESLOT_POLL="0.05"):
        (tmp / "gameslot_config.json").write_text(json.dumps({"slots": 2, "min_free_gb": 0}))
        s = RealSlots(owner="pooltest", ttl_s=60, wait_s=2)
        check(sorted(s.acquire(2)) == [0, 1], "the pool takes both slots")
        s.renew(2)
        check(raises(RuntimeError, RealSlots(owner="other", ttl_s=60, wait_s=0).acquire, 1), "nobody else gets a slot while the pool holds both")
        s.release()
        check(RealSlots(owner="other", ttl_s=60, wait_s=0).acquire(1) == [0], "free after release")


def check_real_pool_with_stand_in_processes(mut):
    """The whole stack with real processes: provision, slots, launch, ready, crash, restart, dirty, down."""
    cfg, tmp = make_world()
    with slot_env(tmp):
        for i in (0, 1):
            instances.provision(cfg, i)
        decoy = spawn_decoy()
        slots = RealSlots(owner="pool", ttl_s=120, wait_s=5)
        backend = RealBackend(cfg, boot_timeout_s=30, options={})
        pool = core.Pool(backend, slots, size=2, sync=True, state_path=tmp / "pool-state.json")
        hook(mut, pool=pool, backend=backend, slots=slots)
        try:
            check(pool.up() == [0, 1], "two instances up")
            pids = {i: pool.insts[i].pid for i in (0, 1)}
            check(all(pool.insts[i].state == "warm" and winproc.pid_alive(pids[i]) for i in (0, 1)), "both warm and alive")
            gi = (cfg.instance_dir(0) / "Game.ini").read_text(encoding="latin-1")
            check(inifile.ini_get(gi, "Display", "iSize W") == "640", "the low-cost profile was applied before launch")
            L = pool.lease("t", n=2, timeout_s=5)
            winproc.kill_pid(pids[0])                       # the game dies under the lease (we started it, so we may end it)
            check(wait_for(lambda: not winproc.pid_alive(pids[0])), "stand-in died")
            pool.tick()
            check(pool.insts[0].gen == 2 and pool.insts[0].pid != pids[0] and winproc.pid_alive(pool.insts[0].pid), "the dead copy was restarted")
            check(pool.leases[L["id"]]["gens"]["0"] != pool.insts[0].gen, "the lessee can see that it was restarted")
            pool.mark_dirty(L["id"], 1, "test")
            pool.release(L["id"])
            check(pool.insts[1].gen == 2 and pool.insts[1].pid != pids[1], "a dirty copy is recycled")
            check(decoy.poll() is None, "the decoy is alive")
            mine = [pool.insts[0].pid, pool.insts[1].pid]
        finally:
            pool.down()
        check(wait_for(lambda: not any(winproc.pid_alive(p) for p in mine + list(pids.values()))), "down killed every copy the pool started")
        check(decoy.poll() is None, "the decoy with the same image name still runs after down")
        check(launcher.load_state(cfg, "pool")["procs"] == {}, "no pids left recorded")
        check(all(s["free"] for s in slot_status()["slots"]), "game slots are free after down")
        kill_proc(decoy)
        sweep(cfg)


def check_smoke_run_cleans_up(mut):
    cfg, tmp = make_world()
    with slot_env(tmp):
        for i in (0, 1):
            instances.provision(cfg, i)
        hook(mut)
        out_dir = tmp / "smoke-out"
        try:
            with redirect_stdout(io.StringIO()):
                out = smoke.run_smoke(cfg, 2, 0.05, 0.4, owner="smoke", slot_wait=5, allow_foreign=True, stagger_s=0.0, ready_each_s=20, out_dir=out_dir)
            check(out["ok"] and out["up_rc"] == 0 and len(out["checks"]) >= 2, f"two copies stay healthy: {out}")
            check(wait_for(lambda: not any(winproc.pid_alive(p) for p in out["pids"].values())), "every copy the run started is gone")
            check((out_dir / "smoke.json").exists(), "report written")
            check(launcher.load_state(cfg, "smoke")["procs"] == {}, "copies were stopped")
            check(all(s["free"] for s in slot_status()["slots"]), "slots freed")
            # a copy that never becomes ready: the run fails, and still stops what it started
            bad, tmp2 = make_world(game_args=["--ready-delay", "30"], ready={"type": "file", "path": "ready.flag", "timeout_s": 1, "poll_s": 0.05})
            for i in (0, 1):
                instances.provision(bad, i)
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                out = smoke.run_smoke(bad, 1, 0.05, 0.2, owner="smoke", slot_wait=5, allow_foreign=True, stagger_s=0.0, ready_each_s=1, out_dir=tmp2 / "o")
            check(not out["ok"] and out["up_rc"] == 3, "a copy that is not ready fails the run")
            check(launcher.load_state(bad, "smoke")["procs"] == {}, "the stuck copy was stopped")
            check(all(s["free"] for s in slot_status()["slots"]), "slots freed after a failed run")
        finally:
            sweep(cfg)
            launcher.slot_release("smoke")


def check_probe_starts_one_copy_and_stops_it(mut):
    cfg, tmp = make_world()
    with slot_env(tmp):
        for i in (0, 1):
            instances.provision(cfg, i)
        hook(mut)
        try:
            with redirect_stdout(io.StringIO()):
                events = smoke.run_probe(cfg, 1.5, owner="probe", slot_wait=5, allow_foreign=True)
            check(events == [], f"the stand-in shows no window, so there is nothing to log: {events}")
            check(launcher.load_state(cfg, "probe")["procs"] == {}, "the probed copy was stopped")
            check(all(s["free"] for s in slot_status()["slots"]), "slots freed")
            check(any((cfg.instance_dir(i) / "game.log").exists() for i in (0, 1)), "the copy really started")
            check(smoke.run_probe(cfg, 0.1, owner="probe", slot_wait=5, allow_foreign=False)[0].startswith("another copy"),
                  "without --allow-foreign the probe refuses to start next to a stranger")
        finally:
            sweep(cfg)


def run_cli(args, envv, cwd=None):
    cp = subprocess.run([sys.executable, "-I", str(HERE / "gamepool.py"), *args], capture_output=True, text=True, env=envv, cwd=cwd, timeout=180)
    return cp.returncode, cp.stdout, cp.stderr


def check_command_line(mut):
    cfg, tmp = make_world()
    cfgfile = tmp / "game.json"
    cfgfile.write_text(json.dumps(cfg.raw))
    state = tmp / "cli-state"
    slots = tmp / "cli-slots"
    slots.mkdir()
    (slots / "gameslot_config.json").write_text(json.dumps({"slots": 2, "min_free_gb": 0}))
    e = dict(os.environ, GAMEPOOL_DIR=str(state), GAMESLOT_DIR=str(slots), GAMESLOT_POLL="0.05", GAMEPOOL_OWNER="clitest")
    e.pop("GAMEPOOL_CONFIG", None)
    hook(mut)
    rc, out, err = run_cli(["--config", str(EXAMPLE), "provision", "--n", "1"], e)
    check(rc == 2 and "config error" in err, f"the unedited example refuses to run: {rc} {err}")
    rc, out, err = run_cli(["provision", "--n", "2"], e)
    check(rc == 2 and "no config file" in err, f"no config given: {rc} {err}")
    try:
        rc, out, err = run_cli(["--config", str(cfgfile), "provision", "--n", "2"], e)
        check(rc == 0 and "instance 1:" in out, f"provision: {rc} {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "status"], e)
        rows = json.loads(out)
        check(rc == 0 and [r["provisioned"] for r in rows] == [True, True], f"status lists both instances: {out}")
        rc, out, err = run_cli(["--config", str(cfgfile), "lowcost", "doc"], e)
        check(rc == 0 and "| Game.ini |" in out, "lowcost doc")
        rc, out, err = run_cli(["--config", str(cfgfile), "up", "--n", "1", "--allow-foreign", "--stagger", "0"], e)
        check(rc == 0 and "ready" in out, f"up: {rc} {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "status"], e)
        check(any(r["alive"] for r in json.loads(out)), "status shows the running copy")
        rc, out, err = run_cli(["--config", str(cfgfile), "up", "--n", "1"], e)
        check(rc == 2 and "not ours" in err, f"without --allow-foreign the tool refuses to start next to another copy of the game: {rc} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "down"], e)
        check(rc == 0 and "killed instance" in out and "RELEASED" in out, f"down: {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "crashlog", str(tmp / "cl"), "--instance", "0", "--minutes", "1"], e)
        check(rc == 0 and (tmp / "cl" / "summary.json").exists(), f"crashlog: {rc} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "job", "--id", "cj", "--n", "1", "--", sys.executable, "-I", "-c", "print(5)"], e)
        check(rc == 0 and '"status": "pass"' in out, f"job: {out} {err}")
        # the daemon: a fake pool through the real command line (detached process, loopback socket)
        rc, out, err = run_cli(["--config", str(cfgfile), "pool", "up", "--fake", "--size", "2", "--tick-s", "0.5"], e)
        check(rc == 0, f"pool up --fake: {rc} {out} {err}")
        info = json.loads((state / "pool" / "pool.json").read_text())
        try:
            rc, out, err = run_cli(["pool", "lease", "--owner", "cli", "--n", "2"], e)
            lease = json.loads(out)
            check(rc == 0 and lease["instances"] == [0, 1], f"pool lease: {out} {err}")
            rc, out, err = run_cli(["pool", "release", "--id", lease["id"]], e)
            check(rc == 0 and '"released": true' in out, f"pool release: {out}")
            rc, out, err = run_cli(["pool", "status"], e)
            check(rc == 0 and json.loads(out)["stats"]["leases"] == 1, "pool status")
        finally:
            rc, out, err = run_cli(["pool", "down"], e)
        check(rc == 0 and not (state / "pool" / "pool.json").exists(), f"pool down: {out} {err}")
        check(wait_for(lambda: not winproc.pid_alive(info["pid"]), 15), "the daemon process is gone")
        rc, out, err = run_cli(["pool", "status"], e)
        check(rc == 2, "no pool running: exit code 2")
    finally:
        with env(GAMEPOOL_DIR=state, GAMESLOT_DIR=slots):
            sweep(cfg, ("clitest", "pool"))


# ---------------------------------------------------------------------------------------------- pairs and swap
def free_port_base(span=10):
    """A port base such that base .. base+span are free for TCP and UDP right now. Chosen below the range Windows hands out for
    outgoing connections (from 49152), so another program does not grab one of the ports a moment later."""
    import random
    for _ in range(300):
        base = random.randint(20000, 45000)
        if all(pairs_mod.port_free(base + k) for k in range(span)):
            return base
    raise RuntimeError("no free port range found")


def pair_world(copy_gb=0.5, reserve_gb=0.0, base=None, adapter=None, edits=True):
    """A fake world whose copies are one half of a host/client pair: the host listens on {port}, the client connects to it."""
    base = base or free_port_base()
    pc = {"port_base": base, "copy_gb": copy_gb, "reserve_gb": reserve_gb}
    if edits:
        pc["edits"] = {"Game.ini": [["Net", "iPort", "{port}"], ["Net", "sRole", "{role}"], ["Net", "iPair", "{pair}"]]}
    extra = {"pairs": pc}
    if adapter:
        extra["adapter"] = adapter
    cfg, tmp = make_world(game_args=["--role", "{role}", "--bind-port", "{port}"], extra=extra)
    return cfg, tmp, base


def lmain(cfg, args):
    """launcher.main(cfg, args) in this process. Returns (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        try:
            rc = launcher.main(cfg, args)
        except SystemExit as e:
            rc = e.code
    return rc, out.getvalue(), err.getvalue()


def imain(cfg, args):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        try:
            rc = instances.main(cfg, args)
        except SystemExit as e:
            rc = e.code
    return rc, out.getvalue(), err.getvalue()


def ini_value(cfg, i, section, key):
    return inifile.ini_get((cfg.instance_dir(i) / "Game.ini").read_text(encoding="latin-1"), section, key)


def game_log(cfg, i):
    p = cfg.instance_dir(i) / "game.log"
    return p.read_text() if p.exists() else ""


def check_pair_layout_math(mut):
    hook(mut)
    check(pairs_mod.aligned_ids(2, 1) == [2, 3, 4, 5], "pairs 1 and 2 are instances 2..5")
    plan = pairs_mod.plan_pairs([0, 1, 2, 3], 30000)
    check([(p["host"], p["client"], p["port"]) for p in plan] == [(0, 1, 30000), (2, 3, 30002)], f"two pairs, two ports: {plan}")
    plan = pairs_mod.plan_pairs([1, 2, 3, 4], 30000)
    check([(p["host"], p["client"], p["port"]) for p in plan] == [(1, 2, 30001), (3, 4, 30003)],
          f"instance numbers that do not start at 0 still give each pair its own port: {plan}")
    check(len({p["port"] for p in pairs_mod.plan_pairs(list(range(12)), 30000)}) == 6, "six pairs, six ports")
    check(raises(ValueError, pairs_mod.plan_pairs, [0, 1, 2]), "an odd number of copies is refused")
    check(raises(ValueError, pairs_mod.plan_pairs, [0, 1, 1, 2]), "a repeated instance is refused")
    check(raises(ValueError, pairs_mod.plan_pairs, [2, 3], 65535), "a port above 65535 is refused")
    check(raises(ValueError, pairs_mod.aligned_ids, 0), "zero pairs is refused")
    ctx = pairs_mod.context_for(plan, 3)
    check(ctx["role"] == "host" and ctx["pair"] == 1 and ctx["port"] == 30003 and ctx["client"] == 4, f"context of a host: {ctx}")
    check(pairs_mod.context_for(plan, 4)["role"] == "client" and pairs_mod.context_for(plan, 9) is None, "context of a client, of a stranger")
    check([pairs_mod.pair_state(a, b) for a, b in ((True, True), (True, False), (False, True), (False, False))] ==
          ["up", "partial", "partial", "down"], "pair states")


def check_ram_guard_math(mut):
    hook(mut)
    g = 2 ** 30
    check(pairs_mod.max_pairs_by_ram(3 * g, 0, 0.7, 2.0) == 0, "1 GiB spare holds one copy, which is no pair")
    check(pairs_mod.max_pairs_by_ram(int(5.8 * g), 0, 0.7, 2.0) == 2, "3.8 GiB spare holds five copies: two pairs")
    check(pairs_mod.max_pairs_by_ram(int(3.1 * g), 2, 0.7, 2.0) == 1, "two copies already run: one more copy makes the second pair")
    check(pairs_mod.max_pairs_by_ram(0, 0, 0.7, 2.0) == 0 and pairs_mod.max_pairs_by_ram(g, 0, 0.7, 2.0) == 0, "no spare RAM, no pairs")
    check(pairs_mod.max_pairs_by_ram(12 * g, 0, 10.0, 2.0) == 0 and pairs_mod.max_pairs_by_ram(12 * g, 0, 0.5, 2.0) == 10,
          "the memory of one copy decides")
    check(pairs_mod.ram_problem(1, 0, 8 * g, 0.7, 2.0) is None, "one pair fits")
    msg = pairs_mod.ram_problem(3, 0, 5 * g, 0.7, 2.0)
    check(msg and "refusing 3 pair(s)" in msg and "4.2 GB" in msg and "at most 2 pair(s) fit" in msg and "--no-ram-check" in msg, f"the refusal explains itself: {msg}")
    check(pairs_mod.ram_problem(2, 4, 2 * g, 0.7, 2.0) is None, "two pairs already up need no more memory")
    check(isinstance(pairs_mod.free_ram_bytes(), int) and pairs_mod.free_ram_bytes() >= 0, "free RAM can be read")


def check_pairs_config_validation(mut):
    hook(mut)
    ok = {"link": {"per_instance_files": ["a.ini"]}, "pairs": {"edits": {"a.ini": [["s", "k", "{port}"]]}}}
    check(Config(ok)["pairs"]["port_base"] == 27960, "a good pairs block loads with its defaults")
    for name, raw in (("edit of a file that is not private", {"pairs": {"edits": {"b.ini": [["s", "k", "v"]]}}}),
                      ("low port", {"pairs": {"port_base": 80}}), ("zero memory per copy", {"pairs": {"copy_gb": 0}}),
                      ("bad protocol", {"pairs": {"proto": "icmp"}}), ("unknown key", {"pairs": {"ports": 3}}),
                      ("bad edit row", {"link": {"per_instance_files": ["a.ini"]}, "pairs": {"edits": {"a.ini": [["s", "k"]]}}})):
        check(raises(ConfigError, Config, raw), f"must refuse: {name}")


def check_provision_pairs(mut):
    cfg, tmp, base = pair_world()
    hook(mut)
    rc, out, err = imain(cfg, ["provision", "--pairs", "2"])
    check(rc == 0 and "pair=1 host port=%d" % (base + 2) in out, f"provision --pairs: {rc} {out} {err}")
    for i, (role, port, pair) in enumerate([("host", base, 0), ("client", base, 0), ("host", base + 2, 1), ("client", base + 2, 1)]):
        check(ini_value(cfg, i, "Net", "iPort") == str(port) and ini_value(cfg, i, "Net", "sRole") == role and ini_value(cfg, i, "Net", "iPair") == str(pair),
              f"instance {i} is the {role} of pair {pair} on port {port}")
    meta = instances.load_meta(cfg, 2)
    check(meta["role"] == "host" and meta["port"] == base + 2 and meta["pair"] == 1, f"the pair is recorded in instance.json: {meta}")
    rc, out, err = imain(cfg, ["provision", "--pairs", "1", "--first-pair", "2"])
    check(rc == 0 and ini_value(cfg, 4, "Net", "iPort") == str(base + 4) and ini_value(cfg, 5, "Net", "sRole") == "client", "--first-pair adds pairs")
    check(imain(cfg, ["provision", "--pairs", "2", "--n", "3"])[0] == 2, "--n that does not match --pairs is refused")
    check(imain(cfg, ["provision", "--first-pair", "1"])[0] == 2, "--first-pair needs --pairs")
    rc, out, err = imain(cfg, ["destroy", "--pairs", "1", "--first-pair", "2"])
    check(rc == 0 and not cfg.instance_dir(4).exists() and cfg.instance_dir(0).exists(), "destroy --pairs removes just those instances")
    # the edit goes into a private file only: a file that is a hard link into the install is refused
    d = cfg.instance_dir(1)
    (d / "Game.ini").unlink()
    os.link(cfg.game_dir / "Game.ini", d / "Game.ini")
    before = sha(cfg.game_dir / "Game.ini")
    check(raises(instances.UnsafeTarget, instances.apply_pair_edits, cfg, 1, pairs_mod.context_for(pairs_mod.plan_pairs([0, 1]), 1)),
          "a pair edit never writes through a link into the install")
    check(sha(cfg.game_dir / "Game.ini") == before, "the install is untouched")


def check_up_pairs_refuses_without_ram(mut):
    cfg, tmp, base = pair_world(copy_gb=0.7, reserve_gb=2.0)
    big, _, _ = pair_world(copy_gb=10.0, reserve_gb=2.0)
    small, _, _ = pair_world(copy_gb=0.5, reserve_gb=2.0)
    hook(mut)
    with slot_env(tmp, 6):
        rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--dry-run", "--free-ram-gb", "3", "--owner", "pt"])
        check(rc == 4 and "refusing 2 pair(s)" in err and "at most 0 pair(s) fit" in err, f"3 GB free is not enough for two pairs: {rc} {err}")
        rep = json.loads(out)
        check(rep["ram"]["ok"] is False and rep["ram"]["max_pairs"] == 0 and rep["ram"]["copy_gb"] == 0.7, f"the numbers are in the plan: {rep['ram']}")
        rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--free-ram-gb", "3", "--owner", "pt", "--allow-foreign"])
        check(rc == 4 and "refusing" in err, f"a real start is refused the same way: {rc} {err}")
        check(all(s["free"] for s in slot_status()["slots"]), "no game slot was taken")
        check(not any(game_log(cfg, i) for i in range(4)) and launcher.load_state(cfg, "pt")["procs"] == {}, "no copy was started")
        rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--dry-run", "--free-ram-gb", "3", "--no-ram-check", "--owner", "pt"])
        check(rc == 0 and json.loads(out)["ram"]["checked"] is False, f"--no-ram-check overrides: {rc} {err}")
        rc, out, err = lmain(big, ["up", "--pairs", "2", "--dry-run", "--free-ram-gb", "12", "--owner", "pt"])
        check(rc == 4 and "40.0 GB" in err, f"a copy that needs 10 GB: two pairs do not fit in 12 GB: {rc} {err}")
        rc, out, err = lmain(small, ["up", "--pairs", "2", "--dry-run", "--free-ram-gb", "12", "--owner", "pt"])
        check(rc == 0, f"a copy that needs 0.5 GB: two pairs fit in 12 GB: {rc} {err}")
        rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--n", "3", "--dry-run", "--owner", "pt"])
        check(rc == 2 and "does not match" in err, f"--n and --pairs must agree: {rc} {err}")


def check_up_pairs_dry_run_plan(mut):
    cfg, tmp, base = pair_world()
    hook(mut)
    with slot_env(tmp, 6):
        rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--dry-run", "--free-ram-gb", "50", "--owner", "pt"])
        rep = json.loads(out)
        check(rc == 0 and rep["assumed_instances"] == [0, 1, 2, 3], f"dry run: {rc} {out} {err}")
        check([(p["host"], p["client"], p["port"]) for p in rep["pairs"]] == [(0, 1, base), (2, 3, base + 2)], f"ports of the plan: {rep['pairs']}")
        check(rep["port_problems"] == [] and not any(game_log(cfg, i) for i in range(4)), "a dry run starts nothing")


def check_up_pairs_needs_free_ports(mut):
    cfg, tmp, base = pair_world()
    srv = socket.socket()
    srv.bind(("127.0.0.1", base))
    srv.listen(1)
    hook(mut)
    try:
        with slot_env(tmp, 6):
            instances.provision(cfg, 0)
            instances.provision(cfg, 1)
            rc, out, err = lmain(cfg, ["up", "--pairs", "1", "--free-ram-gb", "50", "--allow-foreign", "--owner", "pt", "--slot-wait", "5"])
            check(rc == 4 and f"port {base} is already in use" in err, f"a busy port stops the start: {rc} {err}")
            check(launcher.load_state(cfg, "pt")["procs"] == {} and all(s["free"] for s in slot_status()["slots"]),
                  "no copy was started and the slots were given back")
            rc, out, err = lmain(cfg, ["up", "--pairs", "1", "--dry-run", "--free-ram-gb", "50", "--owner", "pt"])
            check(rc == 4 and json.loads(out)["port_problems"], f"the dry run reports the busy port: {rc} {out}")
    finally:
        srv.close()
        with slot_env(tmp, 6):
            sweep(cfg, ("pt",))


def up_two_pairs(cfg, tmp, owner="pt", extra=()):
    rc, out, err = lmain(cfg, ["up", "--pairs", "2", "--free-ram-gb", "50", "--allow-foreign", "--owner", owner, "--slot-wait", "5", *extra])
    return rc, out, err


def check_pairs_up_status_down(mut):
    cfg, tmp, base = pair_world()
    with slot_env(tmp, 6):
        hook(mut)
        try:
            check(imain(cfg, ["provision", "--pairs", "2"])[0] == 0, "provision")
            rc, out, err = up_two_pairs(cfg, tmp)
            check(rc == 0, f"two pairs start: {rc} {out} {err}")
            check(f"pair 0: host instance 0, client instance 1, port {base}" in out and f"pair 1: host instance 2, client instance 3, port {base + 2}" in out,
                  f"the plan is printed: {out}")
            for i, (role, port) in enumerate([("host", base), ("client", base), ("host", base + 2), ("client", base + 2)]):
                check(f"role={role} port={port}" in game_log(cfg, i), f"instance {i} runs as the {role} on port {port}: {game_log(cfg, i)!r}")
            for port in (base, base + 2):
                socket.create_connection(("127.0.0.1", port), timeout=2).close()      # each host listens on its own port
            check(ini_value(cfg, 2, "Net", "iPort") == str(base + 2), "the pair settings are in the private files")
            rc, out, err = lmain(cfg, ["status", "--pairs", "--owner", "pt"])
            rows = json.loads(out)
            check(rc == 0 and [(r["pair"], r["port"], r["state"]) for r in rows] == [(0, base, "up"), (1, base + 2, "up")], f"status per pair: {out}")
            check(all(r["host"]["alive"] and r["client"]["alive"] and r["host"]["ready"] and r["client"]["ready"] for r in rows), "both copies of both pairs are alive and ready")
            rc, out, err = lmain(cfg, ["status", "--owner", "pt"])
            inst = {r["instance"]: r for r in json.loads(out)}
            check(inst[2]["role"] == "host" and inst[3]["role"] == "client" and inst[3]["port"] == base + 2 and inst[3]["pair"] == 1,
                  f"the instance rows carry pair, role and port: {inst[3]}")
            rc, out, err = up_two_pairs(cfg, tmp)
            check(rc == 0 and out.count("already up") == 4, f"a second `up` finds its own pairs running and does not refuse their ports: {rc} {out} {err}")
            rec = launcher.load_state(cfg, "pt")["procs"]["3"]
            winproc.kill_pid(rec["pid"])                                                # the client of pair 1 dies
            check(wait_for(lambda: not winproc.pid_alive(rec["pid"])), "the client is gone")
            rows = json.loads(lmain(cfg, ["status", "--pairs", "--owner", "pt"])[1])
            check([(r["pair"], r["state"]) for r in rows] == [(0, "up"), (1, "partial")], f"one pair lost a copy: {[(r['pair'], r['state']) for r in rows]}")
            check(rows[1]["client"]["alive"] is False and rows[1]["host"]["alive"] is True, "the dead copy is the client")
            rc, out, err = lmain(cfg, ["down", "--owner", "pt"])
            check(rc == 0 and json.loads(lmain(cfg, ["status", "--pairs", "--owner", "pt"])[1]) == [], "after down no pair is listed")
            check(wait_for(lambda: pairs_mod.port_free(base) and pairs_mod.port_free(base + 2), 10), "the ports are free again")
            check(all(s["free"] for s in slot_status()["slots"]), "the slots are free")
        finally:
            sweep(cfg, ("pt",))


def check_pairs_with_slots_that_do_not_start_at_zero(mut):
    """Another agent holds slot 0: this one gets 1..4. The pairs are (1,2) and (3,4), and the ports and roles written into the
    private files at `up` follow the real instance numbers, not the numbers provisioning assumed."""
    cfg, tmp, base = pair_world()
    with slot_env(tmp, 6):
        try:
            check(imain(cfg, ["provision", "--pairs", "3"])[0] == 0, "provision")
            check(launcher.slot_acquire("other", ttl=300, wait=5, slots=1) == [0], "another agent holds slot 0")
            check(ini_value(cfg, 1, "Net", "sRole") == "client" and ini_value(cfg, 1, "Net", "iPort") == str(base), "provisioned for the aligned layout")
            hook(mut)
            rc, out, err = up_two_pairs(cfg, tmp)
            check(rc == 0, f"{rc} {out} {err}")
            check(f"pair 0: host instance 1, client instance 2, port {base + 1}" in out and f"pair 1: host instance 3, client instance 4, port {base + 3}" in out,
                  f"pairs follow the slots: {out}")
            check(ini_value(cfg, 1, "Net", "sRole") == "host" and ini_value(cfg, 1, "Net", "iPort") == str(base + 1), "instance 1 was rewritten as a host")
            check(ini_value(cfg, 4, "Net", "sRole") == "client" and ini_value(cfg, 4, "Net", "iPort") == str(base + 3), "instance 4 was rewritten as a client")
            check(f"role=host port={base + 1}" in game_log(cfg, 1) and f"role=client port={base + 3}" in game_log(cfg, 4), "the copies got the same values on their command lines")
            check(not game_log(cfg, 0), "the slot of the other agent was not used")
        finally:
            lmain(cfg, ["down", "--owner", "pt"])
            launcher.slot_release("other")
            sweep(cfg, ("pt",))


def swap_adapter_source():
    return ("import os\nfrom gplib import winproc\nfrom gplib.adapter import Adapter\n\n\n"
            "class SwapAdapter(Adapter):\n"
            "    def can_swap(self, i, pid, payload):\n"
            "        return 'no reload pipe' if str(i) == os.environ.get('TEST_CANT_SWAP') else None\n\n"
            "    def swap(self, i, pid, payload):\n"
            "        with open(self.cfg.instance_dir(i) / 'swapped.log', 'a') as f:\n"
            "            f.write(repr(sorted(payload.items())) + '\\n')\n"
            "        if str(i) == os.environ.get('TEST_RAISE_ON'):\n"
            "            raise RuntimeError('reload failed')\n"
            "        if str(i) == os.environ.get('TEST_REPORT_FAIL'):\n"
            "            return {'ok': False, 'detail': 'dll rejected'}\n"
            "        if str(i) == os.environ.get('TEST_KILL'):\n"
            "            winproc.kill_pid(pid)\n"
            "            for _ in range(100):\n"
            "                if not winproc.pid_alive(pid):\n"
            "                    break\n"
            "                __import__('time').sleep(0.05)\n"
            "        return {'detail': 'reloaded ' + payload.get('dll', '')}\n\n"
            "    def probe(self, i, pid):\n"
            "        if str(i) == os.environ.get('TEST_UNHEALTHY') and (self.cfg.instance_dir(i) / 'swapped.log').exists():\n"
            "            return False\n"
            "        return super().probe(i, pid)\n")


def swap_lines(cfg, i):
    p = cfg.instance_dir(i) / "swapped.log"
    return p.read_text().splitlines() if p.exists() else []


def swap_run(cfg, extra, **envkw):
    with env(**envkw):
        rc, out, err = lmain(cfg, ["swap", "--owner", "pt", "--settle-s", "0.5", *extra])
    try:
        rep = json.loads(out)
    except ValueError:
        rep = None
    return rc, rep, err


def check_swap_hook(mut):
    cfg, tmp, base = pair_world(adapter="my_swap.py:SwapAdapter", edits=False)
    (tmp / "my_swap.py").write_text(swap_adapter_source())
    plain = Config(dict(cfg.raw, adapter=""), tmp)                      # the same copies, an adapter without a swap hook
    with slot_env(tmp, 6):
        try:
            check(imain(cfg, ["provision", "--pairs", "2"])[0] == 0, "provision")
            rc, out, err = up_two_pairs(cfg, tmp)
            check(rc == 0, f"{rc} {out} {err}")
            pids = {i: launcher.load_state(cfg, "pt")["procs"][str(i)]["pid"] for i in range(4)}
            hook(mut)
            rc, rep, err = swap_run(cfg, ["--dry-run"])
            check(rc == 0 and rep["dry_run"] and [r["status"] for r in rep["results"]] == ["can_swap"] * 4 and not any(swap_lines(cfg, i) for i in range(4)),
                  f"dry run asks and swaps nothing: {rc} {rep} {err}")
            rc, rep, err = swap_run(cfg, ["--arg", "dll=new.dll", "--arg", "mode=fast"])
            check(rc == 0 and rep["ok"] and rep["swapped"] == 4 and [r["instance"] for r in rep["results"]] == [0, 1, 2, 3], f"swap all: {rc} {rep} {err}")
            check(all(swap_lines(cfg, i) == ["[('dll', 'new.dll'), ('mode', 'fast')]"] for i in range(4)), "the adapter got the payload for every copy")
            check([(r["pair"], r["role"]) for r in rep["results"]] == [(0, "host"), (0, "client"), (1, "host"), (1, "client")], "the report names pair and role")
            check(rep["results"][0]["detail"] == "reloaded new.dll" and rep["payload"] == {"dll": "new.dll", "mode": "fast"}, "the adapter's detail and the payload are in the report")
            check(all(winproc.pid_alive(p) for p in pids.values()) and {i: launcher.load_state(cfg, "pt")["procs"][str(i)]["pid"] for i in range(4)} == pids,
                  "the copies are the same processes after the swap")
            # refused before any copy is touched
            rc, rep, err = swap_run(cfg, [], TEST_CANT_SWAP="2")
            check(rc == 2 and rep["refused"] == [{"instance": 2, "reason": "no reload pipe"}] and "instance 2 cannot be swapped: no reload pipe" in err, f"refusal: {rc} {rep} {err}")
            check(all(len(swap_lines(cfg, i)) == 1 for i in range(4)), "a refused swap touched no copy")
            # the first failure stops the rest
            rc, rep, err = swap_run(cfg, [], TEST_RAISE_ON="1")
            check(rc == 1 and [r["status"] for r in rep["results"]] == ["swapped", "failed", "skipped", "skipped"] and "reload failed" in rep["results"][1]["detail"],
                  f"a failing copy stops the swap: {rc} {rep}")
            check([len(swap_lines(cfg, i)) for i in range(4)] == [2, 2, 1, 1], "copies after the failure were not swapped")
            rc, rep, err = swap_run(cfg, [], TEST_REPORT_FAIL="0")
            check(rc == 1 and rep["results"][0]["status"] == "failed" and rep["results"][0]["detail"] == "dll rejected" and rep["results"][1]["status"] == "skipped",
                  f"a returned failure is a failure: {rc} {rep}")
            rc, rep, err = swap_run(cfg, [], TEST_UNHEALTHY="0")
            check(rc == 1 and "not healthy" in rep["results"][0]["detail"], f"a copy that is not healthy after its swap fails it: {rc} {rep}")
            # which copies
            before = [len(swap_lines(cfg, i)) for i in range(4)]
            rc, rep, err = swap_run(cfg, ["--pair", "1"])
            check(rc == 0 and [r["instance"] for r in rep["results"]] == [2, 3] and [len(swap_lines(cfg, i)) for i in range(4)] == [before[0], before[1], before[2] + 1, before[3] + 1],
                  f"--pair 1 swaps only that pair: {rc} {rep}")
            rc, rep, err = swap_run(cfg, ["--instances", "3,0"])
            check(rc == 0 and [r["instance"] for r in rep["results"]] == [3, 0], f"--instances keeps the order given: {rc} {rep}")
            for bad, why in ((["--instances", "9"], "not running"), (["--instances", "x"], "not numbers"), (["--pair", "7"], "no such pair"), (["--arg", "novalue"], "bad --arg")):
                check(swap_run(cfg, bad)[0] == 2, f"must refuse: {why}")
            # an adapter without a swap hook
            n = [len(swap_lines(cfg, i)) for i in range(4)]
            rc, out, err = lmain(plain, ["swap", "--owner", "pt"])
            check(rc == 2 and "no swap hook" in err and [len(swap_lines(cfg, i)) for i in range(4)] == n, f"no hook, no swap: {rc} {err}")
            # a swap that kills the copy
            rc, rep, err = swap_run(cfg, ["--instances", "0"], TEST_KILL="0")
            check(rc == 1 and "gone or was replaced" in rep["results"][0]["detail"], f"a copy that died during its swap is a failure: {rc} {rep}")
            rc, rep, err = swap_run(cfg, ["--instances", "0"])
            check(rc == 2 and "instance 0 is not running" in err, f"a dead copy is not swapped: {rc} {err}")
        finally:
            lmain(cfg, ["down", "--owner", "pt"])
            sweep(cfg, ("pt",))


def check_swap_and_pairs_command_line(mut):
    cfg, tmp, base = pair_world(adapter="my_swap.py:SwapAdapter", copy_gb=0.01, reserve_gb=0.0)
    (tmp / "my_swap.py").write_text(swap_adapter_source())
    cfgfile = tmp / "game.json"
    cfgfile.write_text(json.dumps(cfg.raw))
    state, slots = tmp / "cli-state", tmp / "cli-slots"
    slots.mkdir()
    (slots / "gameslot_config.json").write_text(json.dumps({"slots": 4, "min_free_gb": 0}))
    e = dict(os.environ, GAMEPOOL_DIR=str(state), GAMESLOT_DIR=str(slots), GAMESLOT_POLL="0.05", GAMEPOOL_OWNER="clipairs",
             PYTHONPATH=str(HERE))
    e.pop("GAMEPOOL_CONFIG", None)
    hook(mut)
    try:
        rc, out, err = run_cli(["--config", str(cfgfile), "provision", "--pairs", "2"], e)
        check(rc == 0 and "pair=1 client port=%d" % (base + 2) in out, f"provision --pairs: {rc} {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "up", "--pairs", "2", "--allow-foreign", "--stagger", "0"], e)
        check(rc == 0 and out.count("ready") == 4, f"up --pairs: {rc} {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "status", "--pairs"], e)
        rows = json.loads(out)
        check(rc == 0 and [r["state"] for r in rows] == ["up", "up"] and [r["port"] for r in rows] == [base, base + 2], f"status --pairs: {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "swap", "--arg", "dll=x.dll", "--settle-s", "1", "--out", str(tmp / "swap.json")], e)
        check(rc == 0 and json.loads(out)["swapped"] == 4 and json.loads((tmp / "swap.json").read_text())["ok"], f"swap: {rc} {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "up", "--pairs", "1", "--n", "3"], e)
        check(rc == 2, f"--pairs and --n disagree: {rc} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "down"], e)
        check(rc == 0 and "RELEASED" in out, f"down: {out} {err}")
        rc, out, err = run_cli(["--config", str(cfgfile), "swap"], e)
        check(rc == 2 and "no running copies" in err, f"swap with nothing running: {rc} {err}")
    finally:
        with env(GAMEPOOL_DIR=state, GAMESLOT_DIR=slots):
            sweep(cfg, ("clipairs",))


# ---------------------------------------------------------------------------------------------- twins: pairs and swap
def tw_pairs_share_a_port(**_):
    patch(pairs_mod, "port_of", lambda host, base=27960: int(base))


def tw_roles_swapped(**_):
    real = pairs_mod.context_for

    def swapped(plan, i):
        ctx = real(plan, i)
        if ctx:
            ctx = dict(ctx, role="client" if ctx["role"] == "host" else "host")
        return ctx
    patch(pairs_mod, "context_for", swapped)


def tw_ram_always_fits(**_):
    patch(pairs_mod, "ram_problem", lambda *a, **k: None)


def tw_ram_ignores_running_copies(**_):
    real = pairs_mod.max_pairs_by_ram
    patch(pairs_mod, "max_pairs_by_ram", lambda free, up=0, copy_gb=2.0, reserve_gb=2.0: real(free, 0, copy_gb, reserve_gb))


def tw_ram_ignores_config_copy_gb(**_):
    real = pairs_mod.ram_problem
    patch(pairs_mod, "ram_problem", lambda pairs, up, free, copy_gb=0.7, reserve_gb=2.0: real(pairs, up, free, 0.7, reserve_gb))


def tw_no_port_check(**_):
    patch(pairs_mod, "port_problems", lambda plan, proto="both", skip_pairs=(), is_free=None: [])


def tw_own_ports_checked(**_):
    patch(launcher, "running_pairs", lambda cfg, plan, owner: [])


def tw_pair_state_always_up(**_):
    patch(pairs_mod, "pair_state", lambda host_alive, client_alive: "up")


def tw_pair_edits_not_applied_at_up(**_):
    patch(instances, "apply_pair_edits", lambda cfg, i, ctx: [])


def tw_no_pairs_validation(**_):
    patch(Config, "_validate_pairs", lambda self, per: None)


def tw_swap_without_precheck(**_):
    patch(launcher, "swap_refusals", lambda adapter, targets, payload: [])


def tw_swap_continues_after_failure(**_):
    real = launcher.run_swap
    patch(launcher, "run_swap", lambda cfg, adapter, targets, payload, settle_s, say=None, stop_on_failure=True:
          real(cfg, adapter, targets, payload, settle_s, say, False))


def tw_swap_not_verified(**_):
    patch(launcher, "swap_verify", lambda cfg, adapter, t, settle_s: None)


def tw_swap_drops_payload(**_):
    real = launcher.run_swap
    patch(launcher, "run_swap", lambda cfg, adapter, targets, payload, settle_s, say=None, stop_on_failure=True:
          real(cfg, adapter, targets, {}, settle_s, say, stop_on_failure))


def tw_swap_ignores_returned_failure(**_):
    real_run = launcher.run_swap

    class Wrap:
        """The adapter, except that a returned {"ok": False} loses its failure flag."""
        def __init__(self, inner):
            self.inner = inner

        def swap(self, i, pid, payload):
            res = self.inner.swap(i, pid, payload)
            return {"detail": res.get("detail", "")} if isinstance(res, dict) else res

        def __getattr__(self, name):
            return getattr(self.inner, name)

    patch(launcher, "run_swap", lambda cfg, adapter, targets, payload, settle_s, say=None, stop_on_failure=True:
          real_run(cfg, Wrap(adapter), targets, payload, settle_s, say, stop_on_failure))


# ---------------------------------------------------------------------------------------------- twins (one broken guard each)
def tw_no_config_validation(**_):
    patch(Config, "validate", lambda self: None)


def tw_no_root_check(**_):
    patch(config, "check_roots", lambda cfg: None)
    patch(instances, "check_roots", lambda cfg: None)


def tw_no_private_files(**_):
    real = instances.mirror_tree

    def link_everything(cfg, dst_root, modes):
        counts = real(cfg, dst_root, modes)
        for name in cfg["link"]["per_instance_files"]:          # the broken version hard-links the files that must be private
            if (cfg.game_dir / name).exists() and not (dst_root / name).exists():
                os.link(cfg.game_dir / name, dst_root / name)
        return counts

    def write_in_place(path, text, encoding):                  # ...and writes through whatever is there
        Path(path).write_text(text, encoding=encoding, newline="")
    patch(instances, "mirror_tree", link_everything)
    patch(instances, "_replace_text", write_in_place)


def tw_write_without_unlink(**_):
    patch(instances, "_replace_text", lambda path, text, encoding: Path(path).write_text(text, encoding=encoding, newline=""))


def tw_no_disk_check(**_):
    patch(instances, "MIN_FREE_GB", 0.0)


def tw_pins_never_differ(**_):
    patch(instances, "verify_pins", lambda cfg, i, extra=None: [])


def tw_no_write_guard(**_):
    patch(instances, "guard_write", lambda cfg, path: None)
    patch(lowcost, "guard_write", lambda cfg, path: None)


def tw_opt_in_always_on(**_):
    real = lowcost.rows

    def all_rows(cfg, options=None):
        return [dict(r) for r in cfg["lowcost"]["rows"]]
    patch(lowcost, "rows", all_rows)


def tw_probe_always_ready(**_):
    patch(adapter_mod, "probe", lambda cfg, i, pid: True)


def tw_after_ready_skipped(adapter_cls=None, **_):
    def wait_ready(self, i, pid, alive, timeout_s=0.0):
        return alive(pid) and self.probe(i, pid)
    patch(adapter_cls, "wait_ready", wait_ready)


def tw_no_early_exit_report(**_):
    patch(launcher, "wait_alive", lambda pid, seconds: True)


def tw_child_by_name(**_):
    """The broken loader lookup takes a process with the game's image name that is not recorded yet. To stay
    harmless it only looks at decoys this test run started itself, never at other processes of the PC."""
    def by_name(cfg, loader_pid, skip):
        owned = {p.pid for p in SPAWNED}
        deadline = time.time() + 20
        while time.time() < deadline:
            cands = [p for p in winproc.list_pids(cfg.process_name) if p in owned and p not in skip]
            if cands:
                return max(cands)
            time.sleep(0.1)
        raise launcher.LaunchError("none")
    patch(launcher, "find_game_child", by_name)


def tw_slot_not_released(**_):
    patch(launcher, "slot_release", lambda owner=launcher.DEFAULT_OWNER: "")


def tw_lock_always_granted(**_):
    patch(jobqueue._Lock, "try_acquire", lambda self, info: True)


def tw_smoke_leaves_copies(**_):
    patch(launcher, "stop_owner", lambda cfg, owner: [])


def tw_double_lease(pool=None, **_):
    patch(pool, "_pick_free", lambda n: sorted((i for i in pool.insts.values() if i.state in ("warm", "leased")), key=lambda i: i.id)[:n])


def tw_no_recycle_k(pool=None, **_):
    patch(pool, "recycle_after", 10 ** 9)


def tw_no_dirty(pool=None, **_):
    patch(pool, "mark_dirty", lambda lid, inst_id, reason: True)


def tw_no_expire(pool=None, **_):
    patch(pool, "_reap_expired", lambda now: None)


def tw_kill_foreign(backend=None, **_):
    orig = backend.kill
    patch(backend, "kill", lambda pid: (orig(pid), [backend.kill_any(p) for p in list(backend.foreign)]))


def tw_dead_handed_out(backend=None, **_):
    patch(backend, "alive", lambda pid: True)
    patch(backend, "ping", lambda i: True)


def tw_crash_not_noticed(backend=None, **_):
    patch(backend, "alive", lambda pid: True)


def tw_ping_always_ok(backend=None, **_):
    patch(backend, "ping", lambda i: True)


def tw_no_memory_recycle(pool=None, **_):
    patch(pool, "ws_growth_mb", 10 ** 9)
    patch(pool, "ws_max_mb", 10 ** 9)


def tw_no_kill_on_failed_start(backend=None, **_):
    patch(backend, "kill", lambda pid: None)


def tw_endless_start_retries(pool=None, **_):
    patch(pool, "max_start_fails", 10 ** 9)


def tw_slot_never_renewed(pool=None, **_):
    patch(pool, "slot_ttl_s", 10 ** 9)


def tw_pids_not_registered(slots=None, **_):
    patch(slots, "register_pid", lambda pid: None)


REAL = [
    check_example_config_loads_and_is_clean, check_config_refuses_bad_input, check_roots_must_be_separate, check_ini_editing,
    check_provision_private_files_and_untouched_install, check_reprovision_never_writes_through_old_links,
    check_provision_is_idempotent_and_destroy_is_safe, check_copy_mode, check_disk_check_refuses, check_pins_detect_changes,
    check_guard_write, check_lowcost, check_readiness_probes, check_adapter_wait_ready_and_custom_adapter,
    check_launch_and_pid_scoped_kill, check_loader_child_found_by_process_tree, check_single_instance_games,
    check_crash_is_visible_in_crashlog, check_jobs, check_jobqueue_locks,
    check_pool_exclusive, check_pool_pair_lease_order, check_pool_size_and_slots, check_pool_recycle_after_k,
    check_pool_dirty_recycles, check_pool_expiry, check_pool_renew, check_pool_kills_only_own,
    check_pool_reap_previous_only_recorded, check_pool_crash_restart, check_pool_dead_instance_never_leased,
    check_pool_unresponsive_restart, check_pool_memory_growth, check_pool_start_failure,
    check_pool_failed_start_stops_lease, check_pool_slot_renew_and_pids,
    check_daemon_and_client, check_client_when_pool_is_not_running, check_real_slots_through_gameslot,
    check_real_pool_with_stand_in_processes, check_smoke_run_cleans_up, check_probe_starts_one_copy_and_stops_it,
    check_command_line,
    check_pair_layout_math, check_ram_guard_math, check_pairs_config_validation, check_provision_pairs,
    check_up_pairs_refuses_without_ram, check_up_pairs_dry_run_plan, check_up_pairs_needs_free_ports,
    check_pairs_up_status_down, check_pairs_with_slots_that_do_not_start_at_zero, check_swap_hook,
    check_swap_and_pairs_command_line,
]

TWINS = [
    (check_config_refuses_bad_input, "no_config_validation", tw_no_config_validation),
    (check_roots_must_be_separate, "no_root_check", tw_no_root_check),
    (check_provision_private_files_and_untouched_install, "private_files_linked", tw_no_private_files),
    (check_reprovision_never_writes_through_old_links, "write_without_unlink", tw_write_without_unlink),
    (check_disk_check_refuses, "no_disk_check", tw_no_disk_check),
    (check_pins_detect_changes, "pins_never_differ", tw_pins_never_differ),
    (check_guard_write, "no_write_guard", tw_no_write_guard),
    (check_lowcost, "no_write_guard", tw_no_write_guard),
    (check_lowcost, "opt_in_always_on", tw_opt_in_always_on),
    (check_readiness_probes, "probe_always_ready", tw_probe_always_ready),
    (check_adapter_wait_ready_and_custom_adapter, "after_ready_skipped", tw_after_ready_skipped),
    (check_single_instance_games, "no_early_exit_report", tw_no_early_exit_report),
    (check_loader_child_found_by_process_tree, "child_by_name", tw_child_by_name),
    (check_jobs, "slot_not_released", tw_slot_not_released),
    (check_jobqueue_locks, "lock_always_granted", tw_lock_always_granted),
    (check_smoke_run_cleans_up, "copies_left_running", tw_smoke_leaves_copies),
    (check_pool_exclusive, "double_lease", tw_double_lease),
    (check_pool_recycle_after_k, "no_recycle_after_k", tw_no_recycle_k),
    (check_pool_dirty_recycles, "no_dirty_recycle", tw_no_dirty),
    (check_pool_expiry, "no_expiry", tw_no_expire),
    (check_pool_kills_only_own, "kills_foreign", tw_kill_foreign),
    (check_pool_reap_previous_only_recorded, "reap_kills_foreign", tw_kill_foreign),
    (check_pool_crash_restart, "crash_not_noticed", tw_crash_not_noticed),
    (check_pool_dead_instance_never_leased, "dead_handed_out", tw_dead_handed_out),
    (check_pool_unresponsive_restart, "ping_always_ok", tw_ping_always_ok),
    (check_pool_memory_growth, "no_memory_recycle", tw_no_memory_recycle),
    (check_pool_start_failure, "failed_start_left_running", tw_no_kill_on_failed_start),
    (check_pool_failed_start_stops_lease, "endless_start_retries", tw_endless_start_retries),
    (check_pool_slot_renew_and_pids, "slot_never_renewed", tw_slot_never_renewed),
    (check_pool_slot_renew_and_pids, "pids_not_registered", tw_pids_not_registered),
    (check_daemon_and_client, "no_dirty_recycle", tw_no_dirty),
    (check_pair_layout_math, "pairs_share_a_port", tw_pairs_share_a_port),
    (check_provision_pairs, "pairs_share_a_port", tw_pairs_share_a_port),
    (check_pairs_up_status_down, "pairs_share_a_port", tw_pairs_share_a_port),
    (check_provision_pairs, "roles_swapped", tw_roles_swapped),
    (check_pairs_up_status_down, "roles_swapped", tw_roles_swapped),
    (check_provision_pairs, "no_write_guard", tw_no_write_guard),
    (check_pairs_config_validation, "no_pairs_validation", tw_no_pairs_validation),
    (check_ram_guard_math, "ram_ignores_running_copies", tw_ram_ignores_running_copies),
    (check_up_pairs_refuses_without_ram, "ram_always_fits", tw_ram_always_fits),
    (check_up_pairs_refuses_without_ram, "ram_ignores_config_copy_gb", tw_ram_ignores_config_copy_gb),
    (check_up_pairs_needs_free_ports, "no_port_check", tw_no_port_check),
    (check_pairs_up_status_down, "own_ports_checked", tw_own_ports_checked),
    (check_pairs_up_status_down, "pair_state_always_up", tw_pair_state_always_up),
    (check_pairs_with_slots_that_do_not_start_at_zero, "pair_edits_not_applied_at_up", tw_pair_edits_not_applied_at_up),
    (check_swap_hook, "swap_without_precheck", tw_swap_without_precheck),
    (check_swap_hook, "swap_continues_after_failure", tw_swap_continues_after_failure),
    (check_swap_hook, "swap_result_unchecked", tw_swap_not_verified),
    (check_swap_hook, "swap_drops_payload", tw_swap_drops_payload),
    (check_swap_hook, "swap_ignores_returned_failure", tw_swap_ignores_returned_failure),
]


def kill_tagged():
    """Stop stand-ins of THIS run that nobody recorded (for example the child of a loader in a broken twin).
    They are found by the unique tag on their command line, never by image name."""
    ps = ("Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'python*' -and $_.CommandLine -like '*--tag " + RUN_TAG
          + "*' } | ForEach-Object { $_.ProcessId }")
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=60).stdout
    except Exception:  # noqa: BLE001
        return
    for ln in out.split():
        if ln.isdigit():
            winproc.kill_pid(int(ln))


def final_sweep():
    """Last line of cleanup: stop anything a check recorded and left behind, and any decoy still running."""
    for p in SPAWNED:
        try:
            kill_proc(p)
        except Exception:  # noqa: BLE001
            pass
    kill_tagged()
    for t in TMPS:
        for state in t.rglob("state-*.json"):
            try:
                recs = json.loads(state.read_text()).get("procs", {})
            except (OSError, ValueError):
                continue
            for rec in recs.values():
                if launcher.is_recorded_process_rec(rec):
                    winproc.kill_pid(rec["pid"])


def main():
    bad, n_real, n_twin = [], 0, 0
    try:
        for fn in REAL:
            n_real += 1
            t0 = time.time()
            try:
                fn(None)
                print("ok    ", fn.__name__, "(%.1fs)" % (time.time() - t0), flush=True)
            except Exception:  # noqa: BLE001
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
            except Exception:  # noqa: BLE001  (a crash is not a caught defect)
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
            for _ in range(20):             # a just-killed process may hold its folder for a moment
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
