"""Isolated game installs that cost almost no disk.

Each instance lives in <instances_root>/<i>/ and mirrors the read-only install with hard links (fallback:
symlinks, then copies). Only the files named in the config (link.per_instance_files, saves and plugin
folders, ...) are real, private, writable files. The original install is only ever READ.

Why hard links and not a plain copy: a 50 GB game costs about nothing per copy and a copy appears in a second.
Why private files matter: a hard link is the same file. Writing an ini through a link would change the
owner's own install, so everything the game or a tool writes must be a real file in the instance.

    python -I tools/gamepool/gamepool.py --config game.json provision --n 2
    python -I tools/gamepool/gamepool.py --config game.json show --n 2
    python -I tools/gamepool/gamepool.py --config game.json destroy --n 2
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

from . import pairs as pairs_mod
from .config import Config, ConfigError, _inside, check_roots, expand, has_placeholder, norm_rel
from .inifile import ini_set

MIN_FREE_GB = 2.0


class UnsafeTarget(Exception):
    """A write was refused because it could reach a file outside the private instances."""


def sha256_file(p: Path, chunk: int = 1 << 20) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


# ------------------------------------------------------------------------------------------ safety
def guard_write(cfg: Config, path: Path) -> None:
    """Refuse to edit a file unless it is a private, single-link, real file under the instances root.
    Also refuses anything under cfg.protected_paths (for example the owner's real settings and save folders)
    and anything directly in the game folder."""
    p = Path(path)
    if not _inside(p, cfg.instances_root):
        raise UnsafeTarget(f"{p}: not inside the instances folder {cfg.instances_root}")
    for prot in cfg.protected_paths:
        if _inside(p, prot):
            raise UnsafeTarget(f"{p}: inside the protected folder {prot}")
    if _inside(p, cfg.game_dir):
        raise UnsafeTarget(f"{p}: inside the game folder")
    if p.exists() and os.stat(p).st_nlink > 1:
        raise UnsafeTarget(f"{p}: has {os.stat(p).st_nlink} hard links (writing it would change another folder's file)")
    if p.is_symlink():
        raise UnsafeTarget(f"{p}: is a link")


# ------------------------------------------------------------------------------------------ linking
def _link(src: Path, dst: Path, modes: Sequence[str]) -> str:
    last = None
    for m in modes:
        try:
            if m == "hardlink":
                os.link(src, dst)
            elif m == "symlink":
                os.symlink(src, dst)
            else:
                shutil.copy2(src, dst)
            return m
        except OSError as e:
            last = e
    raise OSError(f"cannot link {src} -> {dst}: {last}")


def _tree_size(root: Path) -> int:
    total = 0
    for dp, _, fns in os.walk(root):
        for fn in fns:
            try:
                total += os.stat(Path(dp) / fn).st_size
            except OSError:
                pass
    return total


def probe_link_mode(src_root: Path, dst_root: Path, modes: Sequence[str]) -> str:
    """Which link mode really works between the two folders (tries one small file)."""
    dst_root.mkdir(parents=True, exist_ok=True)
    sample = None
    for dp, _, fns in os.walk(src_root):
        if fns:
            sample = Path(dp) / fns[0]
            break
    if sample is None:
        return modes[0]
    probe = dst_root / ".linkprobe"
    probe.unlink(missing_ok=True)
    try:
        return _link(sample, probe, modes)
    finally:
        try:
            probe.unlink()
        except OSError:
            pass


def disk_check(cfg: Config, modes: Sequence[str]) -> Tuple[str, float]:
    """Refuse to start when there is not enough free disk. Returns (mode that works, free GiB)."""
    root = cfg.instances_root
    root.mkdir(parents=True, exist_ok=True)
    mode = probe_link_mode(cfg.game_dir, root, modes)
    free = shutil.disk_usage(root).free
    need = MIN_FREE_GB * 2**30
    if mode == "copy":
        need += _tree_size(cfg.game_dir) * 1.05      # hard links and symlinks do not work here: every instance is a full copy
    if free < need:
        raise ConfigError(f"not enough free disk: {free / 2**30:.1f} GiB free, need {need / 2**30:.1f} GiB "
                          f"(link mode '{mode}'; hard links need the install and the instances on the same NTFS volume)")
    return mode, free / 2**30


def mirror_tree(cfg: Config, dst_root: Path, modes: Sequence[str]) -> Dict[str, int]:
    """Link every file of the install into dst_root, as real folders. Idempotent. Returns counts per mode."""
    src_root = cfg.game_dir
    link = cfg["link"]
    per_files = {norm_rel(x) for x in link["per_instance_files"]}
    empty_dirs = {norm_rel(x) for x in link["empty_dirs"]}
    per_dirs = [norm_rel(x) for x in link["per_instance_dirs"]]
    skip_suffixes = tuple(s.lower() for s in link["root_skip_suffixes"])
    counts: Dict[str, int] = {m: 0 for m in ("hardlink", "symlink", "copy")}
    counts["existing"] = 0
    counts["skipped_dir_links"] = 0
    for dirpath, dirnames, filenames in os.walk(src_root):
        rel = Path(dirpath).relative_to(src_root)
        relp = norm_rel(str(rel)) if str(rel) != "." else ""
        keep = []
        for d in dirnames:
            drel = (relp + "/" + d.lower()).lstrip("/")
            if drel in empty_dirs or drel in per_dirs:
                (dst_root / rel / d).mkdir(parents=True, exist_ok=True)        # created empty, contents not linked
            elif (Path(dirpath) / d).is_symlink():
                counts["skipped_dir_links"] += 1                                # never follow a link out of the install
            else:
                keep.append(d)
        dirnames[:] = keep
        out = dst_root / rel
        out.mkdir(parents=True, exist_ok=True)
        for fn in filenames:
            frel = (relp + "/" + fn.lower()).lstrip("/")
            if frel in per_files:
                continue
            if relp == "" and fn.lower().endswith(skip_suffixes):
                continue                       # runtime output of the original install: never share it
            d = out / fn
            if d.exists() or d.is_symlink():
                counts["existing"] += 1
                continue
            counts[_link(Path(dirpath) / fn, d, modes)] += 1
    return counts


def _replace_text(path: Path, text: str, encoding: str) -> None:
    """Write a real, private file. An existing link is removed first, so the write never goes through it."""
    if path.exists() or path.is_symlink():
        path.unlink()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding=encoding, newline="")


def _replace_file(src: Path, dst: Path) -> None:
    if dst.exists() or dst.is_symlink():
        dst.unlink()
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def base_text(cfg: Config, name: str) -> str:
    """Text a config file starts from: the base file named in the config (read only), else the fallback, else empty."""
    spec = cfg["config_files"][name]
    for key in ("base", "fallback_base"):
        v = spec.get(key)
        if not v or has_placeholder(v):
            continue
        p = Path(expand(cfg.fmt(v, 0)))
        if p.exists():
            return p.read_text(encoding=spec.get("encoding", "latin-1"))
    return ""


def render_config_files(cfg: Config, i: int, role: str = "", ctx: Optional[Dict] = None) -> Dict[str, str]:
    out: Dict[str, str] = {}
    extra = dict(ctx or {})
    extra["role"] = role or extra.get("role", "")
    for name, spec in cfg["config_files"].items():
        text = base_text(cfg, name)
        for section, key, value in spec.get("edits", []):
            text = ini_set(text, section, key, cfg.fmt(value, i, **extra))
        out[name] = text
    return out


def apply_pair_edits(cfg: Config, i: int, ctx: Dict) -> List[str]:
    """Write the pairs.edits of the config into instance i's private files, with the pair's port and role filled in.
    Returns the files changed. Every write goes through guard_write, so a file that is a link into the install is refused."""
    changed = []
    d = cfg.instance_dir(i)
    for name, edits in cfg.pairs["edits"].items():
        p = d / name
        guard_write(cfg, p)
        enc = cfg["config_files"].get(name, {}).get("encoding", "latin-1")
        text = p.read_text(encoding=enc) if p.exists() else ""
        for section, key, value in edits:
            text = ini_set(text, section, key, cfg.fmt(value, i, **ctx))
        _replace_text(p, text, enc)
        changed.append(name)
    return changed


def provision(cfg: Config, i: int, *, role: str = "", modes: Optional[Sequence[str]] = None,
              copies: Optional[Dict[str, Path]] = None, pair: Optional[Dict] = None) -> Dict:
    """Build instance i. copies maps a path relative to the instance to a file to copy in (a mod, a plugin).
    pair is the placeholder dict of pairs.context_for() when the instance is part of a host/client pair."""
    cfg.require_dirs()
    g = cfg.game_dir
    d = cfg.instance_dir(i)
    d.mkdir(parents=True, exist_ok=True)
    counts = mirror_tree(cfg, d, tuple(modes or cfg["link"]["modes"]))
    for name in cfg["link"]["empty_dirs"] + cfg["link"]["per_instance_dirs"]:
        p = d / name
        if p.is_symlink():
            raise UnsafeTarget(f"{p} is a symlink; refusing to write into the game install")
        p.mkdir(parents=True, exist_ok=True)
    for name, text in render_config_files(cfg, i, role, pair).items():
        _replace_text(d / name, text, cfg["config_files"][name].get("encoding", "latin-1"))
    for name, text in cfg["write_files"].items():
        _replace_text(d / name, cfg.fmt(text, i, **dict(pair or {}, role=role or (pair or {}).get("role", ""))), "utf-8")
    if pair:
        apply_pair_edits(cfg, i, pair)
    installed: List[str] = []
    for rel, src in (copies or {}).items():
        _replace_file(Path(src), d / rel)
        installed.append(rel)
    meta = {"instance": i, "role": role or (pair or {}).get("role", ""), "pair": (pair or {}).get("pair"), "port": (pair or {}).get("port"),
            "dir": str(d), "link_counts": counts, "installed": installed,
            "game_dir": str(g),
            "pinned": {p: sha256_file(d / p) for p in cfg["link"]["pinned_files"] if (d / p).exists()}}
    (d / "instance.json").write_text(json.dumps(meta, indent=1))
    return meta


def load_meta(cfg: Config, i: int) -> Dict:
    return json.loads((cfg.instance_dir(i) / "instance.json").read_text())


def provisioned(cfg: Config, i: int) -> bool:
    return (cfg.instance_dir(i) / "instance.json").exists()


def destroy(cfg: Config, i: int) -> None:
    """Remove an instance folder. Removes link entries only; the install's files are not touched."""
    check_roots(cfg)
    d = cfg.instance_dir(i)
    if not d.exists():
        return
    if not _inside(d, cfg.instances_root) or d.resolve() == cfg.instances_root.resolve():
        raise UnsafeTarget(f"{d} is not a numbered folder inside {cfg.instances_root}")

    def onerr(fn, path, exc):   # linked files can carry the read-only attribute
        os.chmod(path, 0o666)
        fn(path)
    shutil.rmtree(d, onerror=onerr)


def verify_pins(cfg: Config, i: int, extra: Optional[List[str]] = None) -> List[str]:
    """Problems: a pinned file is missing or its hash differs from the hash recorded at provisioning."""
    d = cfg.instance_dir(i)
    meta = load_meta(cfg, i)
    bad = []
    for rel, h in meta["pinned"].items():
        p = d / rel
        if not p.exists():
            bad.append(f"{rel}: missing")
        elif sha256_file(p) != h:
            bad.append(f"{rel}: hash changed")
    for rel in extra or []:
        if not (d / rel).exists():
            bad.append(f"{rel}: missing")
    return bad


def main(cfg: Config, argv: List[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(prog="gamepool provision|show|destroy")
    ap.add_argument("cmd", choices=["provision", "show", "destroy"])
    ap.add_argument("--n", type=int, default=None, help="number of instances (default 2)")
    ap.add_argument("--pairs", type=int, default=0, help="N host/client pairs: instances 2k (host) and 2k+1 (client), each pair on its own port")
    ap.add_argument("--first-pair", type=int, default=0, help="with --pairs: start at this pair number (add more pairs to an existing set)")
    ap.add_argument("--modes", default="", help="comma list of hardlink,symlink,copy (default: the config)")
    ap.add_argument("--copy", action="append", default=[], metavar="SRC=REL",
                    help="copy file SRC into every instance at REL (a mod or plugin); repeatable")
    a = ap.parse_args(argv)
    plan = None
    if a.pairs:
        if a.n is not None and a.n != 2 * a.pairs:
            ap.error(f"--n {a.n} does not match --pairs {a.pairs} (that is {2 * a.pairs} instances); give only one")
        try:
            plan = pairs_mod.aligned_plan(a.pairs, int(cfg.pairs["port_base"]), a.first_pair)
        except ValueError as e:
            ap.error(str(e))
        ids = pairs_mod.aligned_ids(a.pairs, a.first_pair)
    else:
        if a.first_pair:
            ap.error("--first-pair needs --pairs")
        ids = list(range(2 if a.n is None else a.n))
    if a.cmd == "provision":
        cfg.require_dirs()
        modes = tuple(a.modes.split(",")) if a.modes else tuple(cfg["link"]["modes"])
        mode, free = disk_check(cfg, modes)
        print(f"link mode that works here: {mode}; free disk: {free:.1f} GiB")
        copies = {}
        for c in a.copy:
            src, _, rel = c.partition("=")
            if not rel:
                ap.error("--copy needs SRC=REL")
            copies[rel] = Path(src)
        for i in ids:
            ctx = pairs_mod.context_for(plan, i) if plan else None
            m = provision(cfg, i, role=(ctx["role"] if ctx else ("host" if i == 0 else "client")), modes=modes, copies=copies, pair=ctx)
            extra = f" pair={ctx['pair']} {ctx['role']} port={ctx['port']}" if ctx else ""
            print(f"instance {i}: {m['dir']} links={m['link_counts']}{extra}")
    elif a.cmd == "show":
        for i in ids:
            print(json.dumps(load_meta(cfg, i), indent=1))
    else:
        cfg.require_dirs()
        for i in ids:
            destroy(cfg, i)
            print("destroyed", i)
    return 0
