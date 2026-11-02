"""Collect evidence after a game crash: the game's logs plus the Windows crash record, into one folder.

    from gplib.crashlog import collect_crash_report
    summary = collect_crash_report(cfg, "runs/t1/crash", instance=0)

    gamepool.py --config game.json crashlog DEST [--instance 0] [--minutes 30]

What is read comes from the config (crash block): `logs` are paths relative to the instance folder (or the
game folder when no instance is given), `markers` are texts that mark a crash line in those logs,
`wer_match` is a regular expression for the Windows Application log (event id 1000, application crash)
and defaults to the game's image name without ".exe".

A game that crashes at exit or shows its own "critical error" box often never flushes its log, so "no error in
the log" proves nothing. The Windows record and the exit code are the second source.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path
from typing import Callable, Dict, List, Optional

from .config import Config

_PS = ("Get-WinEvent -FilterHashtable @{LogName='Application';Id=1000;StartTime=(Get-Date).AddMinutes(-$env:GP_MINUTES)} "
       "-ErrorAction SilentlyContinue | ? { $_.Message -match $env:GP_MATCH } | select -First 3 | "
       "% { $_.TimeCreated.ToString('s') + ' ' + ($_.Message -replace \"`r?`n\", ' | ') }")


def _wer_events(minutes: int, match: str) -> List[str]:
    env = dict(os.environ, GP_MINUTES=str(int(minutes)), GP_MATCH=match)
    try:
        r = subprocess.run(["powershell", "-NoProfile", "-Command", _PS], capture_output=True, text=True, timeout=60, env=env)
        return [l for l in r.stdout.splitlines() if l.strip()]
    except Exception as e:  # noqa: BLE001
        return [f"(could not query the event log: {e})"]


def collect_crash_report(cfg: Config, dest, instance: Optional[int] = None, event_minutes: int = 30,
                         event_query: Optional[Callable[[int], List[str]]] = None) -> Dict:
    """Copy the logs into `dest`, write summary.json (last log lines, marker lines, Windows crash records).
    event_query(minutes) -> list of text can be injected (tests do) instead of reading the real event log."""
    base = cfg.instance_dir(instance) if instance is not None else cfg.game_dir
    dest = Path(dest)
    dest.mkdir(parents=True, exist_ok=True)
    crash = cfg["crash"]
    copied, tails, marked = [], {}, []
    for rel in crash["logs"]:
        src = base / rel
        if src.is_file():
            shutil.copy2(src, dest / src.name)
            copied.append(src.name)
            lines = src.read_text(errors="replace").splitlines()
            tails[src.name] = lines[-8:]
            marked += [f"{src.name}: {l}" for l in lines if any(m in l for m in crash["markers"])][-5:]
    match = crash["wer_match"] or re.escape(re.sub(r"\.exe$", "", cfg.process_name or cfg.name))
    events = (event_query or (lambda m: _wer_events(m, match)))(event_minutes)
    summary = {"collected_at": time.strftime("%Y-%m-%dT%H:%M:%S"), "base_dir": str(base), "files": copied,
               "marker_lines": marked, "windows_crash_records": events, "log_tails": tails}
    (dest / "summary.json").write_text(json.dumps(summary, indent=2))
    return summary


def main(cfg: Config, argv: List[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(prog="gamepool crashlog")
    ap.add_argument("dest")
    ap.add_argument("--instance", type=int)
    ap.add_argument("--minutes", type=int, default=30)
    a = ap.parse_args(argv)
    print(json.dumps(collect_crash_report(cfg, a.dest, a.instance, a.minutes), indent=2)[:4000])
    return 0
