"""Game profile: everything the analyzer needs to know about one game, loaded from a small JSON file.

A profile says:
  * which bone name plays each role the analyzer uses (root bone, arm bones),
  * the animation group table: group id (or name) -> name,
  * which coarse category each group belongs to (idle, walk, run, ... see CATEGORIES),
  * the angle unit, the world scale, and optional threshold overrides.

Without a profile the analyzer uses GENERIC, a tiny neutral table that the synthetic tests use. For a real game, write
a profile (see examples/fnv/profile.json for a full one) and pass it with `--profile`.

Profile JSON (every key is optional):

    {
      "name": "mygame",
      "bones": {"root": "Hips", "left_upper_arm": "UpperArm_L", "left_hand": "Hand_L",
                "right_upper_arm": "UpperArm_R", "right_hand": "Hand_R"},
      "groups": ["Idle", "Walk", "Run"],                 // index = group id. Or {"0": "Idle", "5": "Walk"}
      "categories": {"idle": ["Idle"], "walk": ["Walk"], "run": ["Run", 12]},   // exact names or ids
      "category_prefixes": {"attack": ["Attack"], "reload": ["Reload"]},
      "default_category": "misc",                        // known group that matches no rule
      "angle_unit": "rad",                               // or "deg"
      "units_per_meter": 70.0,                           // thresholds are written for 70; they are scaled to this
      "move_flag_bits": {"sneak": 9, "swim": 11},        // optional: bit numbers in a `move_flags` integer
      "thresholds": {"slide_min_speed": 50.0}            // any field of config.Thresholds
    }
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Union

Group = Union[int, str]

# The categories the detectors understand. Profiles map groups onto these.
CATEGORIES = ("idle", "walk", "run", "dodge", "turn", "aim", "attack", "reload", "jam", "equip", "hit", "death",
              "jump", "block", "misc")
# Categories that mean "the body is travelling under animation control"
LOCOMOTION = {"walk", "run", "dodge"}
# Categories only a living, standing actor plays
ALIVE_ONLY = {"idle", "walk", "run", "dodge", "turn", "aim", "attack", "reload", "jam", "equip", "block", "misc"}
AIRBORNE = {"jump"}

# Bone roles. The analyzer needs "root" and the four arm roles; the others are only used by synth.py.
ANALYZER_ROLES = ("root", "left_upper_arm", "left_hand", "right_upper_arm", "right_hand")
SYNTH_ROLES = ("root", "spine1", "spine2", "neck", "head", "left_upper_arm", "left_forearm", "left_hand",
               "right_upper_arm", "right_forearm", "right_hand", "left_calf", "left_foot", "right_calf", "right_foot",
               "weapon")

_KNOWN_KEYS = {"name", "bones", "groups", "categories", "category_prefixes", "default_category", "angle_unit",
               "units_per_meter", "move_flag_bits", "thresholds"}


@dataclass(frozen=True)
class Profile:
    name: str = "generic"
    bones: Dict[str, str] = field(default_factory=dict)
    groups: Tuple[str, ...] = ()
    categories: Dict[str, Tuple[Group, ...]] = field(default_factory=dict)
    category_prefixes: Dict[str, Tuple[str, ...]] = field(default_factory=dict)
    default_category: str = "misc"
    angle_unit: str = "rad"
    units_per_meter: float = 70.0
    move_flag_bits: Dict[str, int] = field(default_factory=dict)
    threshold_overrides: Dict[str, object] = field(default_factory=dict)
    # group id -> name for tables given as a sparse dict; filled by from_dict
    sparse_groups: Dict[int, str] = field(default_factory=dict)

    def __post_init__(self) -> None:
        exact: Dict[Group, str] = {}
        for cat, items in self.categories.items():
            for it in items:
                exact.setdefault(it, cat)
        prefixes: List[Tuple[str, str]] = []
        for cat, items in self.category_prefixes.items():
            for pre in items:
                prefixes.append((pre, cat))
        object.__setattr__(self, "_exact", exact)
        object.__setattr__(self, "_prefixes", prefixes)
        object.__setattr__(self, "_cache", {})

    # ---- bones ----
    @property
    def root_bone(self) -> str:
        return self.bone("root")

    def bone(self, role: str) -> str:
        """Bone name for a role; a role the profile does not name is used as its own bone name."""
        return self.bones.get(role, role)

    # ---- groups ----
    def known(self, group: Group) -> bool:
        if isinstance(group, int):
            return (0 <= group < len(self.groups)) or group in self.sparse_groups
        return group in self.groups or group in self._exact

    def group_name(self, group: Group) -> str:
        if isinstance(group, str):
            return group
        if group in self.sparse_groups:
            return self.sparse_groups[group]
        if 0 <= group < len(self.groups):
            return self.groups[group]
        return f"group{group}"

    def category(self, group: Group) -> str:
        """One of CATEGORIES, or 'unknown' for a group the profile does not know at all."""
        c = self._cache.get(group)
        if c is not None:
            return c
        c = self._category(group)
        self._cache[group] = c
        return c

    def _category(self, group: Group) -> str:
        if group in self._exact:
            return self._exact[group]
        if isinstance(group, int) and group < 0:
            return "unknown"
        n = self.group_name(group)
        if n in self._exact:
            return self._exact[n]
        for pre, cat in self._prefixes:
            if n.startswith(pre):
                return cat
        if self.known(group):
            return self.default_category
        return "unknown"

    def first_group(self, category: str) -> Group:
        """The first group id (or name) of a category; used by the synthetic streams."""
        for i in range(len(self.groups)):
            if self.category(i) == category:
                return i
        for gid, n in sorted(self.sparse_groups.items()):
            if self.category(gid) == category:
                return gid
        for it in self.categories.get(category, ()):
            return it
        raise KeyError(f"profile {self.name!r} has no group in category {category!r}")

    # ---- thresholds ----
    def thresholds(self, base=None):
        """Default thresholds scaled to this game's world units, then the profile's own overrides applied."""
        from .config import DEFAULT, Thresholds
        th = (base or DEFAULT).scaled(self.units_per_meter / 70.0)
        return th.with_(**Thresholds.coerce(self.threshold_overrides)) if self.threshold_overrides else th

    # ---- loading ----
    @classmethod
    def from_dict(cls, d: dict) -> "Profile":
        if not isinstance(d, dict):
            raise ValueError("profile must be a JSON object")
        unknown = sorted(k for k in d if k not in _KNOWN_KEYS and not k.startswith("_") and not k.startswith("//"))
        if unknown:
            raise ValueError(f"unknown profile keys: {', '.join(unknown)}")
        for key in ("bones", "categories", "category_prefixes", "move_flag_bits", "thresholds"):
            if d.get(key) is not None and not isinstance(d[key], dict):
                raise ValueError(f"{key} must be an object")
        groups: Tuple[str, ...] = ()
        sparse: Dict[int, str] = {}
        g = d.get("groups", [])
        if isinstance(g, list):
            groups = tuple(str(x) for x in g)
        elif isinstance(g, dict):
            sparse = {int(k): str(v) for k, v in g.items()}
        else:
            raise ValueError("groups must be a list or an object")
        cats = {}
        for cat, items in (d.get("categories") or {}).items():
            if cat not in CATEGORIES:
                raise ValueError(f"unknown category {cat!r} (use one of {', '.join(CATEGORIES)})")
            if not isinstance(items, list):
                raise ValueError(f"categories.{cat} must be a list")
            cats[cat] = tuple(items)
        pres = {}
        for cat, items in (d.get("category_prefixes") or {}).items():
            if cat not in CATEGORIES:
                raise ValueError(f"unknown category {cat!r} (use one of {', '.join(CATEGORIES)})")
            if not isinstance(items, list):
                raise ValueError(f"category_prefixes.{cat} must be a list")
            pres[cat] = tuple(str(x) for x in items)
        dc = d.get("default_category", "misc")
        if dc not in CATEGORIES:
            raise ValueError(f"unknown default_category {dc!r}")
        unit = d.get("angle_unit", "rad")
        if unit not in ("rad", "deg"):
            raise ValueError("angle_unit must be 'rad' or 'deg'")
        bones = {str(k): str(v) for k, v in (d.get("bones") or {}).items()}
        bits = {str(k): int(v) for k, v in (d.get("move_flag_bits") or {}).items()}
        if set(bits) - {"sneak", "swim"}:
            raise ValueError("move_flag_bits keys must be 'sneak' and/or 'swim'")
        upm = float(d.get("units_per_meter", 70.0))
        if upm <= 0:
            raise ValueError("units_per_meter must be positive")
        ov = dict(d.get("thresholds") or {})
        from .config import Thresholds
        Thresholds.coerce(ov)   # validates the keys now rather than at analysis time
        return cls(name=str(d.get("name", "unnamed")), bones=bones, groups=groups, categories=cats,
                   category_prefixes=pres, default_category=dc, angle_unit=unit, units_per_meter=upm,
                   move_flag_bits=bits, threshold_overrides=ov, sparse_groups=sparse)

    @classmethod
    def load(cls, path: Union[str, Path]) -> "Profile":
        with open(path, "r", encoding="utf-8") as f:
            return cls.from_dict(json.load(f))

    def summary(self) -> str:
        cats: Dict[str, int] = {}
        n = len(self.groups) + len(self.sparse_groups)
        for i in list(range(len(self.groups))) + sorted(self.sparse_groups):
            c = self.category(i)
            cats[c] = cats.get(c, 0) + 1
        parts = ", ".join(f"{k}={v}" for k, v in sorted(cats.items()))
        return (f"profile {self.name!r}: {n} groups ({parts}); root bone {self.root_bone!r}; "
                f"angle unit {self.angle_unit}; {self.units_per_meter:g} units per meter")


# A tiny neutral profile: group id = position in this list, group name = its category.
GENERIC = Profile(
    name="generic",
    bones={"root": "pelvis", "spine1": "spine_1", "spine2": "spine_2", "neck": "neck", "head": "head",
           "left_upper_arm": "upper_arm_l", "left_forearm": "forearm_l", "left_hand": "hand_l",
           "right_upper_arm": "upper_arm_r", "right_forearm": "forearm_r", "right_hand": "hand_r",
           "left_calf": "calf_l", "left_foot": "foot_l", "right_calf": "calf_r", "right_foot": "foot_r",
           "weapon": "weapon"},
    groups=("idle", "walk", "run", "dodge", "turn", "aim", "attack", "reload", "equip", "hit", "death", "jump",
            "block", "talk"),
    categories={"idle": ("idle",), "walk": ("walk",), "run": ("run",), "dodge": ("dodge",), "turn": ("turn",),
                "aim": ("aim",), "attack": ("attack",), "reload": ("reload",), "equip": ("equip",), "hit": ("hit",),
                "death": ("death",), "jump": ("jump",), "block": ("block",), "misc": ("talk",)},
)

DEFAULT_PROFILE = GENERIC
