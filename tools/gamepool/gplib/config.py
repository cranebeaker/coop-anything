"""Per-game configuration for the game pool tools. One JSON file describes one game.

Every game specific value (install folder, exe names, which files must be private per copy, ini keys, how
to tell the game is ready) lives in that file, never in the code. See examples/fnv.json and the README.

Paths in the file may use %VARS% (expanded) and the placeholders <game dir> / <instances dir>, which the
tools refuse to run with, so a copied example cannot act on a folder by accident.
"""
from __future__ import annotations

import json
import os
import re
from pathlib import Path
from typing import Any, Dict, List, Optional

HUB = Path(__file__).resolve().parents[3]
PLACEHOLDER = re.compile(r"<[^<>]+>")

DEFAULTS: Dict[str, Any] = {
    "name": "game",
    "game_dir": "",
    "instances_root": "",
    "process_name": "",
    "launch": {"exe": "", "args": [], "env": {}, "window": "minimized", "spawns_game": False,
               "stagger_s": 2.0, "child_wait_s": 30.0, "watch_s": 3.0},
    "link": {"modes": ["hardlink", "symlink", "copy"], "per_instance_files": [], "empty_dirs": [],
             "per_instance_dirs": [], "root_skip_suffixes": [".log", ".dmp", ".mdmp"], "pinned_files": []},
    "config_files": {},
    "write_files": {},
    "protected_paths": [],
    "ready": {"type": "process", "timeout_s": 300},
    "crash": {"logs": [], "markers": ["CRASH"], "wer_match": ""},
    "collect_logs": [],
    "lowcost": {"rows": [], "post_checks": []},
    "adapter": "",
    "pool": {"recycle_after": 25, "ws_growth_mb": 1200, "ws_max_mb": 3300, "slot_ttl_s": 3600, "tick_s": 5.0},
    "pairs": {"port_base": 27960, "copy_gb": 2.0, "reserve_gb": 2.0, "proto": "both", "edits": {}},
}
PAIRS_KEYS = ("port_base", "copy_gb", "reserve_gb", "proto", "edits")
PAIRS_PROTOS = ("udp", "tcp", "both")
WINDOW_MODES = ("minimized", "normal", "hidden")
LINK_MODES = ("hardlink", "symlink", "copy")
READY_TYPES = ("process", "window", "file", "log_line", "tcp_port", "named_pipe")


class ConfigError(Exception):
    pass


def state_dir() -> Path:
    """Where this tool keeps its private state (pids, locks, pool info, runs). Git ignores <repo>/local/."""
    return Path(os.environ.get("GAMEPOOL_DIR") or HUB / "local" / "gamepool")


def has_placeholder(s: str) -> bool:
    return bool(PLACEHOLDER.search(str(s)))


def expand(s: str) -> str:
    return os.path.expanduser(os.path.expandvars(str(s)))


def _merge(base: Dict[str, Any], over: Dict[str, Any]) -> Dict[str, Any]:
    out = dict(base)
    for k, v in over.items():
        if isinstance(v, dict) and isinstance(out.get(k), dict) and k not in ("config_files", "write_files"):
            out[k] = _merge(out[k], v)
        else:
            out[k] = v
    return out


def norm_rel(p: str) -> str:
    """Relative path with forward slashes, lowercase: the form used to compare file names."""
    return str(p).replace("\\", "/").strip("/").lower()


class Config:
    def __init__(self, raw: Dict[str, Any], base_dir: Optional[Path] = None):
        unknown = [k for k in raw if k not in DEFAULTS and not k.startswith("_")]
        if unknown:
            raise ConfigError(f"unknown config keys: {unknown}")
        self.raw = _merge(DEFAULTS, raw)
        self.base_dir = Path(base_dir) if base_dir else Path.cwd()
        self.validate()

    # ------------------------------------------------------------------ loading
    @classmethod
    def load(cls, path: Optional[str] = None) -> "Config":
        p = path or os.environ.get("GAMEPOOL_CONFIG")
        if not p:
            raise ConfigError("no config file: pass --config FILE or set GAMEPOOL_CONFIG (see tools/gamepool/examples/fnv.json)")
        f = Path(expand(p))
        try:
            raw = json.loads(f.read_text(encoding="utf-8"))
        except OSError as e:
            raise ConfigError(f"cannot read {f}: {e}") from e
        except ValueError as e:
            raise ConfigError(f"{f} is not valid JSON: {e}") from e
        return cls(raw, f.resolve().parent)

    # ------------------------------------------------------------------ validation
    def validate(self) -> None:
        r = self.raw
        if r["launch"]["window"] not in WINDOW_MODES:
            raise ConfigError(f"launch.window must be one of {WINDOW_MODES}")
        bad = [m for m in r["link"]["modes"] if m not in LINK_MODES]
        if bad or not r["link"]["modes"]:
            raise ConfigError(f"link.modes must be a non-empty list of {LINK_MODES}, got {r['link']['modes']}")
        if r["ready"].get("type") not in READY_TYPES:
            raise ConfigError(f"ready.type must be one of {READY_TYPES}")
        per = {norm_rel(x) for x in r["link"]["per_instance_files"]}
        for name in r["config_files"]:
            if norm_rel(name) not in per:
                # A config file that is not private per copy would be a hard link into the original install:
                # writing it would change the owner's file.
                raise ConfigError(f"config_files entry {name!r} must also be listed in link.per_instance_files")
        for name in r["write_files"]:
            if norm_rel(name) not in per and not any(norm_rel(name).startswith(norm_rel(d) + "/") for d in r["link"]["per_instance_dirs"]):
                raise ConfigError(f"write_files entry {name!r} must be in link.per_instance_files or inside link.per_instance_dirs")
        for row in r["lowcost"]["rows"]:
            for k in ("file", "section", "key", "value"):
                if k not in row:
                    raise ConfigError(f"lowcost row {row} lacks {k!r}")
        for name in {row["file"] for row in r["lowcost"]["rows"]}:
            if norm_rel(name) not in per:
                raise ConfigError(f"lowcost file {name!r} must be listed in link.per_instance_files")
        self._validate_pairs(per)

    def _validate_pairs(self, per) -> None:
        p = self.raw["pairs"]
        unknown = [k for k in p if k not in PAIRS_KEYS and not k.startswith("_")]
        if unknown:
            raise ConfigError(f"unknown keys in pairs: {unknown}")
        try:
            base, copy_gb, reserve = int(p["port_base"]), float(p["copy_gb"]), float(p["reserve_gb"])
        except (TypeError, ValueError) as e:
            raise ConfigError(f"pairs.port_base, copy_gb and reserve_gb must be numbers: {e}") from e
        if not 1024 <= base <= 65535:
            raise ConfigError(f"pairs.port_base must be within 1024..65535, got {base}")
        if copy_gb <= 0 or reserve < 0:
            raise ConfigError("pairs.copy_gb must be above 0 and pairs.reserve_gb must not be negative")
        if p["proto"] not in PAIRS_PROTOS:
            raise ConfigError(f"pairs.proto must be one of {PAIRS_PROTOS}")
        for name, edits in p["edits"].items():
            if norm_rel(name) not in per:
                # an edit of a file that is not private per copy would be written through a hard link into the original install
                raise ConfigError(f"pairs.edits file {name!r} must also be listed in link.per_instance_files")
            if not all(isinstance(e, (list, tuple)) and len(e) == 3 for e in edits):
                raise ConfigError(f"pairs.edits[{name!r}] must be a list of [section, key, value]")

    def require_dirs(self) -> None:
        """Commands that touch disk call this: refuse placeholders and missing folders."""
        for key in ("game_dir",):
            v = self.raw[key]
            if not v or has_placeholder(v):
                raise ConfigError(f"{key} is not set (still {v!r}); edit your copy of the config")
        if not self.game_dir.is_dir():
            raise ConfigError(f"game_dir does not exist: {self.game_dir}")
        if self.raw["instances_root"] and has_placeholder(self.raw["instances_root"]):
            raise ConfigError(f"instances_root is still {self.raw['instances_root']!r}; set it or remove the key")
        check_roots(self)

    def require_process_name(self) -> str:
        if not self.process_name:
            raise ConfigError("process_name is not set (the image name of the game process, e.g. Game.exe)")
        return self.process_name

    # ------------------------------------------------------------------ accessors
    def __getitem__(self, k: str) -> Any:
        return self.raw[k]

    @property
    def name(self) -> str:
        return self.raw["name"]

    @property
    def slug(self) -> str:
        return re.sub(r"[^a-z0-9]+", "-", self.name.lower()).strip("-") or "game"

    @property
    def game_dir(self) -> Path:
        return Path(expand(self.raw["game_dir"]))

    @property
    def instances_root(self) -> Path:
        v = self.raw["instances_root"]
        return Path(expand(v)) if v and not has_placeholder(v) else state_dir() / "instances" / self.slug

    @property
    def process_name(self) -> str:
        return self.raw["process_name"].lower()

    @property
    def pairs(self) -> Dict[str, Any]:
        return self.raw["pairs"]

    @property
    def protected_paths(self) -> List[Path]:
        return [Path(expand(p)) for p in self.raw["protected_paths"]]

    def instance_dir(self, i: int) -> Path:
        return self.instances_root / str(int(i))

    def fmt(self, text: str, i: int, **extra: Any) -> str:
        """Fill {i}, {instance_dir}, {game_dir}, {role} in a config string."""
        vars_ = {"i": int(i), "instance_dir": str(self.instance_dir(i)), "game_dir": str(self.game_dir), "role": "", **extra}
        out = str(text)
        for k, v in vars_.items():
            out = out.replace("{" + k + "}", str(v))
        return out


def _inside(child: Path, parent: Path) -> bool:
    try:
        child.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def check_roots(cfg: Config) -> None:
    """The instances folder and the game folder must not contain each other (copying a folder into itself
    never ends, and removing an instance must never be able to reach the install)."""
    g, root = cfg.game_dir, cfg.instances_root
    if _inside(root, g) or _inside(g, root):
        raise ConfigError(f"instances_root ({root}) and game_dir ({g}) must be separate folders, neither inside the other")
