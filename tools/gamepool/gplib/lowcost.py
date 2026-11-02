"""Low-cost profile: private per-instance settings that make a kept-running game copy cheap.

The profile is data in the game's config file (lowcost.rows), not code. Each row is
{file, section, key, value, why, status, opt_in?}. Typical rows: small window, no vsync, frame cap, no
shadows, no anti-aliasing, skip the intro movies.

It edits ONLY files of an instance folder (see instances.guard_write): never the owner's own settings, never
the install, never a file with more than one hard link, never a link.

    python -I tools/gamepool/gamepool.py --config game.json lowcost apply --instance 0 [--opt grids=3]
    python -I tools/gamepool/gamepool.py --config game.json lowcost doc        # markdown table of every row
    python -I tools/gamepool/gamepool.py --config game.json lowcost verify     # which keys exist in the base files

status of a row (free text, shown in the table): required (instance-local operation needs it), standard (a key
the game is expected to read), guess (a name taken from a sibling game; unknown keys are ignored by most
engines, so a wrong guess costs nothing, but the saving is not known until measured), opt-in.

An opt-in row carries "opt_in": "<option name>" and is applied only when that option is passed with --opt
NAME=VALUE (or options={"NAME": "VALUE"} in code). "value_from_option": true uses the option value as the
row value. "option_regex" rejects values that do not match (for example only odd numbers).
"""
from __future__ import annotations

import re
from pathlib import Path
from typing import Dict, List, Optional

from .config import Config
from .inifile import ini_get, ini_set
from .instances import UnsafeTarget, base_text, guard_write


def rows(cfg: Config, options: Optional[Dict[str, str]] = None) -> List[Dict]:
    opts = dict(options or {})
    out = []
    for r in cfg["lowcost"]["rows"]:
        r = dict(r)
        name = r.get("opt_in")
        if name:
            if name not in opts:
                continue
            if r.get("option_regex") and not re.fullmatch(r["option_regex"], str(opts[name])):
                raise ValueError(f"option {name}={opts[name]!r} does not match {r['option_regex']}")
            if r.get("value_from_option"):
                r["value"] = str(opts[name])
        out.append(r)
    unknown = set(opts) - {r.get("opt_in") for r in cfg["lowcost"]["rows"]}
    if unknown:
        raise ValueError(f"unknown option(s) {sorted(unknown)}; the config defines {sorted(x for x in {r.get('opt_in') for r in cfg['lowcost']['rows']} if x)}")
    return out


def _enc(cfg: Config, name: str) -> str:
    return cfg["config_files"].get(name, {}).get("encoding", "latin-1")


def apply(cfg: Config, instance_dir: Path, options: Optional[Dict[str, str]] = None, check: bool = True) -> Dict[str, int]:
    """Edit the instance's files in place. Returns {file: rows written}. Idempotent."""
    d = Path(instance_dir)
    selected = rows(cfg, options)
    texts: Dict[str, str] = {}
    for name in sorted({r["file"] for r in selected}):
        f = d / name
        if check:
            guard_write(cfg, f)
        texts[name] = f.read_text(encoding=_enc(cfg, name)) if f.exists() else ""
    n = {name: 0 for name in texts}
    for r in selected:
        texts[r["file"]] = ini_set(texts[r["file"]], r["section"], r["key"], str(r["value"]))
        n[r["file"]] += 1
    for name, t in texts.items():
        (d / name).write_text(t, encoding=_enc(cfg, name), newline="")
    for fname, section, key, want in cfg["lowcost"]["post_checks"]:
        f = d / fname
        got = ini_get(f.read_text(encoding=_enc(cfg, fname)) if f.exists() else "", section, key)
        if got != want:
            raise UnsafeTarget(f"{fname} [{section}] {key} is {got!r} after apply, expected {want!r}")
    return n


def verify(cfg: Config) -> Dict[str, List[str]]:
    """Read only: which profile keys exist in the base files the instances start from."""
    found: Dict[str, List[str]] = {"present": [], "absent": []}
    base = {name: base_text(cfg, name) for name in cfg["config_files"]}
    for r in cfg["lowcost"]["rows"]:
        has = r["file"] in base and ini_get(base[r["file"]], r["section"], r["key"]) is not None
        found["present" if has else "absent"].append(f"{r['file']}:{r['section']}:{r['key']}")
    return found


def render_doc(cfg: Config) -> str:
    lines = ["| File | Section | Key | Value | Why | Status |", "|---|---|---|---|---|---|"]
    for r in cfg["lowcost"]["rows"]:
        lines.append(f"| {r['file']} | {r['section']} | `{r['key']}` | `{r['value']}` | {r.get('why', '')} | {r.get('status', '')} |")
    return "\n".join(lines)


def main(cfg: Config, argv: List[str]) -> int:
    import argparse
    import json
    ap = argparse.ArgumentParser(prog="gamepool lowcost")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("apply")
    p.add_argument("--instance", type=int, required=True)
    p.add_argument("--opt", action="append", default=[], metavar="NAME=VALUE")
    sub.add_parser("doc")
    sub.add_parser("verify")
    a = ap.parse_args(argv)
    if a.cmd == "doc":
        print(render_doc(cfg))
    elif a.cmd == "verify":
        cfg.require_dirs()
        print(json.dumps(verify(cfg), indent=1))
    else:
        cfg.require_dirs()
        opts = dict(o.partition("=")[::2] for o in a.opt)
        print(apply(cfg, cfg.instance_dir(a.instance), opts))
    return 0
