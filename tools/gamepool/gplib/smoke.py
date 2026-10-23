"""Multi-copy smoke run and launch probe.

    gamepool.py --config game.json smoke --n 2 --minutes 10 --every 30
        takes n game slots, starts the copies one after the other (minimized), waits until each is ready, then
        checks every --every seconds for --minutes: is the copy alive and does it still pass the readiness
        probe, how much RAM and CPU does it use, are its windows still minimized. Always kills the copies it
        started and frees the slots. Writes <state>/runs/<stamp>-smoke/smoke.json. Exit 0 only if every check of
        every copy passed.

    gamepool.py --config game.json probe --seconds 30
        starts one copy minimized and logs, every 50 ms, the state of its windows (visible, minimized,
        foreground), so you can tell whether a window ever shows up un-minimized in front of the owner.

Neither command touches the owner's settings or saves: the copies are private instance folders.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Dict, List, Optional

from . import launcher, winproc
from .adapter import Adapter, load_adapter
from .config import Config, state_dir


def window_states(pid: int) -> List[str]:
    return [("minimized" if ic else "NORMAL") for _, _, _, ic in winproc.windows_of({pid})]


def run_smoke(cfg: Config, n: int, minutes: float, every: float, *, owner: str = "smoke", slot_wait: int = 3600,
              allow_foreign: bool = False, stagger_s: float = 2.0, ready_each_s: float = 300.0,
              adapter: Optional[Adapter] = None, out_dir=None) -> Dict:
    cfg.require_dirs()
    adapter = adapter or load_adapter(cfg)
    out: Dict = {"n": n, "sys_before": winproc.system_memory(), "checks": [], "ok": False}
    try:
        ids = launcher.slot_acquire(owner, ttl=int(minutes * 60) + 900, wait=slot_wait, slots=n)
    except launcher.SlotBusy as e:
        out["error"] = f"slot busy: {e}"
        return out
    out["instances"] = ids
    try:
        foreign = launcher.foreign_pids(cfg)
        if foreign and not allow_foreign:
            out["error"] = f"{cfg.process_name} already running (pids {foreign}) and not ours; nothing was started"
            return out
        rc = launcher.start_instances(cfg, ids, owner, adapter, stagger_s, ready_each_s)
        out["up_rc"] = rc
        if rc != 0:
            return out
        pids = {i: rec["pid"] for i, rec in launcher.load_state(cfg, owner)["procs"].items()}
        out["pids"] = pids
        allok = True
        deadline = time.time() + minutes * 60
        while True:
            row = {"sys": winproc.system_memory()}
            for i, pid in pids.items():
                alive = winproc.pid_alive(pid)
                healthy = bool(alive and adapter.probe(int(i), pid))
                row[i] = {"alive": alive, "probe_ok": healthy, "windows": window_states(pid) if alive else [],
                          **winproc.proc_stats(pid)}
                allok = allok and healthy
            out["checks"].append(row)
            print(json.dumps(row), flush=True)
            if time.time() >= deadline:
                break
            time.sleep(every)
        out["ok"] = allok
    finally:
        launcher.stop_owner(cfg, owner)
        print(launcher.slot_release(owner))
        d = out_dir or state_dir() / "runs" / (time.strftime("%Y%m%dT%H%M%S") + "-smoke")
        d.mkdir(parents=True, exist_ok=True)
        (d / "smoke.json").write_text(json.dumps(out, indent=1))
    return out


def run_probe(cfg: Config, seconds: float, owner: str = "probe", slot_wait: int = 3600, allow_foreign: bool = False) -> List[str]:
    cfg.require_dirs()
    ids = launcher.slot_acquire(owner, ttl=int(seconds) + 300, wait=slot_wait, slots=1)
    events: List[str] = []
    try:
        if launcher.foreign_pids(cfg) and not allow_foreign:
            events.append("another copy of the game is running and is not ours; nothing was started")
            return events
        import ctypes
        fg_of = ctypes.WinDLL("user32").GetForegroundWindow
        fg_of.restype = ctypes.c_void_p
        pid, _ = launcher.launch_instance_ex(cfg, ids[0], owner)
        t0 = time.time()
        seen: Dict[int, str] = {}
        while time.time() - t0 < seconds:
            fg = fg_of()
            for h, p, title, iconic in winproc.windows_of({pid}):
                st = ("minimized" if iconic else "NORMAL") + (" FOREGROUND" if h == fg else "")
                if seen.get(h) != st:
                    seen[h] = st
                    events.append(f"+{time.time() - t0:5.2f}s pid {p} hwnd {h:#x} '{title}' -> {st}")
                    print(events[-1], flush=True)
            time.sleep(0.05)
    finally:
        launcher.stop_owner(cfg, owner)
        launcher.slot_release(owner)
    return events


def main(cfg: Config, argv: List[str]) -> int:
    ap = argparse.ArgumentParser(prog="gamepool smoke|probe")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("smoke")
    p.add_argument("--n", type=int, default=2)
    p.add_argument("--minutes", type=float, default=10)
    p.add_argument("--every", type=float, default=30)
    p.add_argument("--slot-wait", type=int, default=3600)
    p.add_argument("--allow-foreign", action="store_true")
    p = sub.add_parser("probe")
    p.add_argument("--seconds", type=float, default=30)
    p.add_argument("--slot-wait", type=int, default=3600)
    p.add_argument("--allow-foreign", action="store_true")
    a = ap.parse_args(argv)
    if a.cmd == "probe":
        run_probe(cfg, a.seconds, slot_wait=a.slot_wait, allow_foreign=a.allow_foreign)
        return 0
    out = run_smoke(cfg, a.n, a.minutes, a.every, slot_wait=a.slot_wait, allow_foreign=a.allow_foreign,
                    stagger_s=cfg["launch"]["stagger_s"], ready_each_s=float(cfg["ready"].get("timeout_s", 300)))
    print(json.dumps({k: v for k, v in out.items() if k != "checks"}, indent=1))
    return 0 if out["ok"] else 1
