"""Playbook refs from a scenario file.

A playbook needs named things ("raiders", "item", "quest"). The scenario tool (tools/scenarios) lets a scenario carry a
`refs` block that names them with `$alias` strings; this turns that block into plain ids. It reads the JSON itself, so
the bots tool does not import the scenario tool.
"""
import json
import re
from pathlib import Path
from typing import Any, Dict

_ALIAS = re.compile(r"^\$([A-Za-z][A-Za-z0-9_]*)$")


def refs_from_scenario(scenario: Any) -> Dict[str, Any]:
    """`scenario` is a dict or a path to a scenario JSON file. Returns {ref name: id, list of ids, or literal value}."""
    sc = json.loads(Path(scenario).read_text(encoding="utf-8")) if not isinstance(scenario, dict) else scenario
    objects = sc.get("objects") or {}

    def one(v):
        if isinstance(v, str):
            m = _ALIAS.match(v)
            if m:
                if m.group(1) not in objects:
                    raise KeyError("refs use unknown alias %s" % v)
                return str(objects[m.group(1)]["id"])
            return v
        if isinstance(v, list):
            return [one(x) for x in v]
        if isinstance(v, dict):
            return {k: one(x) for k, x in v.items()}
        return v
    return {k: one(v) for k, v in (sc.get("refs") or {}).items()}
