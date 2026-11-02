"""Assertions on a report's flat counters, for playbooks, scripts and dashboards.

    from animcheck.counters import check
    failures = check(report, {"pose_err_p95": "<=12", "glitches": "==0", "glitch.sliding": 0})
    assert not failures, failures

A spec value is a number (meaning ==) or a string with an operator: <=, >=, <, >, ==, !=. An unknown counter name is a
failure (a typo must not pass silently).
"""
from __future__ import annotations

import operator
import re
from typing import Dict, List, Union

from .config import mutant

_OPS = {"<=": operator.le, ">=": operator.ge, "<": operator.lt, ">": operator.gt, "==": operator.eq, "!=": operator.ne}
_RE = re.compile(r"^\s*(<=|>=|==|!=|<|>)\s*(-?\d+(?:\.\d+)?)\s*$")


def check(report: dict, spec: Dict[str, Union[str, int, float]]) -> List[str]:
    counters = report.get("counters", report)
    out: List[str] = []
    for name, want in spec.items():
        if name not in counters:
            out.append(f"{name}: no such counter (have: {', '.join(sorted(counters)[:12])} ...)")
            continue
        got = counters[name]
        if isinstance(want, (int, float)):
            op, val = "==", float(want)
        else:
            m = _RE.match(str(want))
            if not m:
                out.append(f"{name}: bad spec {want!r}")
                continue
            op, val = m.group(1), float(m.group(2))
        if not _OPS[op](got, val) and mutant() != "counters":
            out.append(f"{name}: {got} fails {op} {val:g}")
    return out
