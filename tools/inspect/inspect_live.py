#!/usr/bin/env python3
"""inspect_live: a read-only, side-by-side table of what each running game copy is doing right now.

    python -I tools/inspect/inspect_live.py --fields my_fields.json \\
        --endpoint host=tcp://127.0.0.1:27990 --endpoint client=tcp://127.0.0.1:27991
    python -I tools/inspect/inspect_live.py --fields my_fields.json --endpoint-file host=run/host.json --watch 1
    python -I tools/inspect/inspect_live.py --fields my_fields.json --endpoint a=pipe://mygame-0 --json

One column per game copy, one row per field. Which rows exist comes from a field list (JSON, see README.md and
examples/demo_fields.json): each field reads one value over the in-game bridge (tools/bridge) with a `ping`, `state`,
`eval`, `events` or a read-only custom method (`call`). After the table come the problems the field list's rules
spot: a menu that is open, a copy that is behind, a mismatch counter that moved.

Read-only by construction. Every request goes through ReadOnlyBridge, which allows a fixed list of query methods
(ping, version, methods, events, state, eval) plus the custom methods the field list names in `read_methods`, and refuses
everything else. Never allowed, even if a field list asks: exec, save, load, screenshot, waitFor, quit. `eval` is allowed only for expressions whose first word is in
the field list's `eval_allow`. So a message box that is up stays up and a quest stage stays where it is.

A method the game lacks shows as n/a in that row; a copy that cannot be reached is a column that says so. If your game's
bridge takes one client at a time, stop any runner that is attached to the same copy first.
Exit 0 when at least one copy answered, 2 when none did, 1 on bad arguments.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import textwrap
import time
from collections import deque
from pathlib import Path
from typing import Any, Deque, Dict, List, Optional, Sequence, Tuple

_BRIDGE_DIR = os.environ.get("COOPINSPECT_BRIDGE_DIR") or str(Path(__file__).resolve().parent.parent / "bridge")
if _BRIDGE_DIR not in sys.path:
    sys.path.insert(0, _BRIDGE_DIR)
import bridge_client as bc  # noqa: E402

READ_BASE = frozenset({"ping", "version", "methods", "events", "state", "eval"})
HARD_DENY = frozenset({"exec", "save", "load", "screenshot", "waitFor", "quit"})
SOURCES = ("ping", "version", "state", "eval", "events", "call")
OPS = ("==", "!=", "<", "<=", ">", ">=", "nonempty", "empty")
COL_W = 40            # characters per copy column; longer lines wrap
MAX_EVENTS = 200      # events kept per copy


class SpecError(Exception):
    """The field list is wrong."""


class ReadOnlyViolation(Exception):
    """The inspector tried to do something that could change the game."""


class _Absent(Exception):
    """The value's path is not in the answer."""


class SafeDict(dict):
    def __missing__(self, key: str) -> str:
        return "?"


# ---------------------------------------------------------------------------------------------- read-only wrapper
class ReadOnlyBridge:
    """Wraps a bridge and lets only reads through. Deny by default."""

    def __init__(self, bridge: Any, allowed: Sequence[str], eval_allow: Sequence[str]):
        self._b = bridge
        self.allowed = frozenset(allowed)
        self.eval_allow = frozenset(x.lower() for x in eval_allow)
        self.calls: List[str] = []       # every method (and expression) that went out, for the tests

    def call(self, method: str, params: Optional[Dict[str, Any]] = None, timeout_s: float = 10.0) -> Any:
        if method not in self.allowed:
            raise ReadOnlyViolation(f"{method} is not a read-only method")
        self.calls.append(method)
        return self._b.call(method, params or None, timeout_s=timeout_s)

    def eval(self, expr: str, ref: Optional[str] = None) -> Any:
        m = re.match(r"\s*([\w.]+)", expr)
        head = m.group(1).lower() if m else ""
        if head not in self.eval_allow:
            raise ReadOnlyViolation(f"expression {expr!r} is not on the eval_allow list")
        p: Dict[str, Any] = {"expr": expr}
        if ref is not None:
            p["ref"] = ref
        self.calls.append("eval " + expr)
        return self.call("eval", p)["value"]

    def __getattr__(self, name: str) -> Any:
        raise AttributeError(f"{name}: not available through the read-only bridge")


# ---------------------------------------------------------------------------------------------- field list
class Spec:
    def __init__(self, data: Dict[str, Any]):
        self.name = str(data.get("name", "game"))
        self.eval_allow = [str(x) for x in data.get("eval_allow", [])]
        self.read_methods = [str(x) for x in data.get("read_methods", [])]
        self.fields: List[Dict[str, Any]] = list(data.get("fields", []))
        self.rules: List[Dict[str, Any]] = list(data.get("problems", []))
        self.validate()

    def validate(self) -> None:
        for m in self.read_methods:
            if m in HARD_DENY:
                raise SpecError(f"read_methods may not contain {m!r}: the inspector never calls it")
        if not self.fields:
            raise SpecError("the field list has no fields")
        seen = set()
        for f in self.fields:
            label = f.get("label")
            if not isinstance(label, str) or not label:
                raise SpecError(f"every field needs a label: {f!r}")
            if label in seen:
                raise SpecError(f"two fields are labelled {label!r}")
            seen.add(label)
            src = f.get("source")
            if src not in SOURCES:
                raise SpecError(f"field {label!r}: source must be one of {', '.join(SOURCES)}")
            if src == "eval" and not isinstance(f.get("expr"), str):
                raise SpecError(f"field {label!r}: an eval field needs 'expr'")
            if src == "call":
                if f.get("method") not in self.read_methods:
                    raise SpecError(f"field {label!r}: method {f.get('method')!r} is not listed in read_methods")
            if src == "events" and not isinstance(f.get("types"), list):
                raise SpecError(f"field {label!r}: an events field needs a 'types' list")
        for r in self.rules:
            if "differs" in r:
                if r["differs"] not in seen:
                    raise SpecError(f"rule {r!r}: unknown field {r['differs']!r}")
            else:
                if r.get("field") not in seen:
                    raise SpecError(f"rule {r!r}: unknown field {r.get('field')!r}")
                if r.get("op") not in OPS:
                    raise SpecError(f"rule {r!r}: op must be one of {', '.join(OPS)}")
            if not isinstance(r.get("say"), str):
                raise SpecError(f"rule {r!r}: needs a 'say' text")

    @property
    def allowed_methods(self) -> List[str]:
        return sorted((READ_BASE | set(self.read_methods)) - HARD_DENY)


def load_spec(path: str) -> Spec:
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        raise SpecError(f"cannot read the field list {path}: {e}") from e
    if not isinstance(data, dict):
        raise SpecError("the field list must be a JSON object")
    return Spec(data)


# ---------------------------------------------------------------------------------------------- one game copy
class CopyState:
    """One column: the connection to a game copy and what was read from it so far."""

    def __init__(self, name: str, endpoint: Optional[str] = None, bridge: Any = None):
        self.name = name
        self.endpoint = endpoint
        self.bridge = bridge
        self.events: Deque[Dict[str, Any]] = deque(maxlen=MAX_EVENTS)
        self.cursor = 0

    def drop(self) -> None:
        if self.bridge is not None:
            try:
                self.bridge.close()
            except Exception:  # noqa: BLE001
                pass
        self.bridge = None
        self.events.clear()
        self.cursor = 0


def dig(obj: Any, path: str) -> Any:
    """obj['a']['b'][0] for 'a.b.0'. Raises _Absent when the path is not there."""
    cur = obj
    if path in ("", None):
        return cur
    for part in str(path).split("."):
        if isinstance(cur, dict) and part in cur:
            cur = cur[part]
        elif isinstance(cur, list) and part.isdigit() and int(part) < len(cur):
            cur = cur[int(part)]
        else:
            raise _Absent(path)
    return cur


def plain(v: Any) -> str:
    if v is None:
        return "null"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        return f"{v:g}"
    if isinstance(v, (dict, list)):
        return json.dumps(v, separators=(",", ":"))
    return str(v)


def format_value(f: Dict[str, Any], raw: Any) -> List[str]:
    """The lines shown in the cell. `format` formats one value (a list is spread over {0} {1} ..., a dict gives {name}),
    `template` formats each item of a list or each event."""
    fmt, tmpl = f.get("format"), f.get("template")
    if f["source"] == "events":
        items = raw[-int(f.get("last", 3)):]
        t = tmpl or "{type}"
        lines = [t.format_map(SafeDict(dict(e.get("data") or {}, seq=e.get("seq"), type=e.get("type")))) for e in items]
        return lines or [str(f.get("empty", "none"))]
    if isinstance(raw, list) and tmpl:
        lines = []
        for it in raw:
            try:
                lines.append(tmpl.format_map(SafeDict(it)) if isinstance(it, dict) else tmpl.format(it))
            except (IndexError, ValueError, KeyError):
                lines.append(plain(it))
        return lines or [str(f.get("empty", "none"))]
    if fmt:
        try:
            if isinstance(raw, dict):
                return [fmt.format_map(SafeDict(raw))]
            if isinstance(raw, (list, tuple)):
                return [fmt.format(*raw)]
            return [fmt.format(raw)]
        except (IndexError, ValueError, KeyError, TypeError):
            return [plain(raw)]
    return [plain(raw)]


# ---------------------------------------------------------------------------------------------- the inspector
class Inspector:
    def __init__(self, spec: Spec, copies: Sequence[CopyState], connect_wait_s: float = 2.0, col_w: int = COL_W):
        self.spec = spec
        self.copies = list(copies)
        self.connect_wait_s = connect_wait_s
        self.col_w = col_w
        self.last: Dict[str, Dict[str, Any]] = {}

    # ---- reading ----
    def _open(self, st: CopyState) -> ReadOnlyBridge:
        if st.bridge is None:
            if not st.endpoint:
                raise bc.ConnectionLost("no endpoint")
            st.bridge = bc.Bridge.connect(st.endpoint, wait_s=self.connect_wait_s)
        return ReadOnlyBridge(st.bridge, self.spec.allowed_methods, self.spec.eval_allow)

    def _events(self, st: CopyState, ro: ReadOnlyBridge) -> List[Dict[str, Any]]:
        for _ in range(20):
            page = ro.call("events", {"since": st.cursor, "limit": 500})
            st.events.extend(page["events"])
            if not page["events"]:
                break
            st.cursor = page["next"]
            if st.cursor >= page.get("last", st.cursor):
                break
        return list(st.events)

    def _value(self, f: Dict[str, Any], ro: ReadOnlyBridge, st: CopyState, shared: Dict[str, Any]) -> Any:
        src = f["source"]
        if src == "eval":
            return ro.eval(f["expr"], f.get("ref"))
        if src == "events":
            if "events" not in shared:
                shared["events"] = self._fetch(lambda: self._events(st, ro))
            return self._shared(shared["events"], None, f, only=f["types"])[-int(f.get("last", 3)):]
        if src == "state":
            sections = sorted({x["section"] for x in self.spec.fields if x["source"] == "state" and x.get("section")})
            whole = any(x["source"] == "state" and not x.get("section") for x in self.spec.fields)
            if "state" not in shared:
                shared["state"] = self._fetch(lambda: ro.call("state", {"sections": sections} if sections and not whole else None))
            return self._shared(shared["state"], f.get("path", ""), f)
        if src == "call":
            key = "call:" + f["method"] + json.dumps(f.get("params") or {}, sort_keys=True)
            if key not in shared:
                shared[key] = self._fetch(lambda: ro.call(f["method"], f.get("params") or None))
            return self._shared(shared[key], f.get("path", ""), f)
        if src not in shared:
            shared[src] = self._fetch(lambda: ro.call(src))
        return self._shared(shared[src], f.get("path", ""), f)

    @staticmethod
    def _fetch(fn: Any) -> Any:
        """The answer, or the RpcError that stands for it (one failing method must not hide the other rows)."""
        try:
            return fn()
        except bc.RpcError as e:
            return e

    @staticmethod
    def _shared(answer: Any, path: Optional[str], f: Dict[str, Any], only: Optional[List[str]] = None) -> Any:
        if isinstance(answer, bc.RpcError):
            raise answer
        if only is not None:
            return [e for e in answer if e.get("type") in only]
        return dig(answer, path or "")

    def read_copy(self, st: CopyState) -> Dict[str, Any]:
        rep: Dict[str, Any] = {"name": st.name, "reachable": True, "values": {}, "lines": {}, "errors": {}}
        try:
            ro = self._open(st)
            shared: Dict[str, Any] = {}
            for f in self.spec.fields:
                label = f["label"]
                try:
                    raw = self._value(f, ro, st, shared)
                    rep["values"][label] = raw
                    rep["lines"][label] = format_value(f, raw)
                except bc.RpcError as e:
                    if e.code == -32601:
                        rep["lines"][label] = ["n/a"]
                    else:
                        rep["errors"][label] = e.message
                        rep["lines"][label] = ["error: " + e.message]
                except ReadOnlyViolation as e:
                    rep["errors"][label] = str(e)
                    rep["lines"][label] = ["refused: " + str(e)]
                except _Absent:
                    rep["lines"][label] = ["n/a"]
        except bc.BridgeError as e:
            st.drop()
            rep = {"name": st.name, "reachable": False, "error": str(e)}
        return rep

    def refresh(self) -> List[Dict[str, Any]]:
        """Read every copy again. Nothing is reused from the previous refresh except open connections and the event cursor."""
        reports = [self.read_copy(st) for st in self.copies]
        self.last = {r["name"]: r for r in reports}
        return reports

    def close(self) -> None:
        for st in self.copies:
            st.drop()

    # ---- problems ----
    @staticmethod
    def _test(op: str, v: Any, rhs: Any) -> bool:
        if op == "nonempty":
            return bool(v)
        if op == "empty":
            return not v
        try:
            return {"==": v == rhs, "!=": v != rhs, "<": v < rhs, "<=": v <= rhs, ">": v > rhs, ">=": v >= rhs}[op]
        except TypeError:
            return False

    def problems(self, reports: List[Dict[str, Any]]) -> List[str]:
        out: List[str] = []
        for r in reports:
            if not r["reachable"]:
                out.append(f"{r['name']}: unreachable ({r.get('error', '')})")
                continue
            for label, msg in r["errors"].items():
                out.append(f"{r['name']}: {label}: {msg}")
        ok = [r for r in reports if r["reachable"]]
        for rule in self.spec.rules:
            if "differs" in rule:
                label = rule["differs"]
                vals = {r["name"]: r["values"][label] for r in ok if label in r["values"]}
                if len({json.dumps(v, sort_keys=True) for v in vals.values()}) > 1:
                    out.append(rule["say"].format_map(SafeDict(field=label, values=", ".join(f"{k} {plain(v)}" for k, v in vals.items()))))
                continue
            for r in ok:
                label = rule["field"]
                if label not in r["values"]:
                    continue
                v = r["values"][label]
                hit = self._test(rule["op"], v, rule.get("value"))
                if hit:
                    out.append(rule["say"].format_map(SafeDict(copy=r["name"], field=label, value=plain(v))))
        return out

    # ---- the table ----
    def render(self, reports: List[Dict[str, Any]]) -> str:
        col_w = self.col_w
        rows: List[Tuple[str, List[List[str]]]] = [("copy", [[r["name"]] for r in reports])]
        if not any(r["reachable"] for r in reports):
            rows.append(("state", [[r.get("error", "unreachable")] for r in reports]))
        else:
            for f in self.spec.fields:
                label = f["label"]
                cells = []
                for r in reports:
                    if not r["reachable"]:
                        cells.append(["unreachable"] if label == self.spec.fields[0]["label"] else [""])
                    else:
                        cells.append(r["lines"].get(label, [""]))
                rows.append((label, cells))
        lw = max(len(label) for label, _ in rows) + 1

        def line(label: str, cells: List[str]) -> str:
            return (label.ljust(lw) + "| " + " | ".join(c.ljust(col_w) for c in cells)).rstrip()
        out: List[str] = []
        for i, (label, cols) in enumerate(rows):
            wrapped = [[w for t in (c or [""]) for w in (textwrap.wrap(t, col_w) or [""])] for c in cols]
            h = max(len(w) for w in wrapped)
            for k in range(h):
                out.append(line(label if k == 0 else "", [w[k] if k < len(w) else "" for w in wrapped]))
            if i == 0:
                out.append("-" * lw + "+" + "+".join("-" * (col_w + 2) for _ in cols))
        probs = self.problems(reports)
        out.append("")
        out.append("problems: none" if not probs else "problems:")
        out.extend("  - " + p for p in probs)
        return "\n".join(out)


# ---------------------------------------------------------------------------------------------- command line
def parse_named(items: Sequence[str], kind: str) -> List[Tuple[str, str]]:
    """['host=tcp://h:1', 'tcp://h:2'] -> [('host', 'tcp://h:1'), ('2', 'tcp://h:2')]. A name is optional."""
    out: List[Tuple[str, str]] = []
    for i, it in enumerate(items):
        m = re.match(r"^([A-Za-z0-9_.-]+)=(.+)$", it)
        out.append((m.group(1), m.group(2)) if m else (str(i), it))
    names = [n for n, _ in out]
    if len(set(names)) != len(names):
        raise ValueError(f"two {kind}s have the same name: {names}")
    return out


def main(argv: Optional[Sequence[str]] = None, copies: Optional[Sequence[CopyState]] = None, out: Any = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fields", required=True, help="the field list (JSON)")
    ap.add_argument("--endpoint", action="append", default=[], metavar="[NAME=]ENDPOINT", help="tcp://HOST:PORT or pipe://NAME of one game copy (repeat)")
    ap.add_argument("--endpoint-file", action="append", default=[], metavar="[NAME=]PATH", help="the endpoint file a game writes at start (repeat)")
    ap.add_argument("--wait", type=float, default=2.0, help="seconds to wait for each copy to answer (default 2)")
    ap.add_argument("--watch", type=float, default=0.0, metavar="SECONDS", help="refresh every SECONDS until Ctrl-C")
    ap.add_argument("--count", type=int, default=0, help="with --watch: stop after this many refreshes")
    ap.add_argument("--col-width", type=int, default=COL_W)
    ap.add_argument("--json", action="store_true", help="print the raw data as JSON instead of the table")
    a = ap.parse_args(argv)
    out = out or sys.stdout
    try:
        spec = load_spec(a.fields)
        if copies is None:
            named = parse_named(a.endpoint, "endpoint")
            for name, path in parse_named(a.endpoint_file, "endpoint file"):
                try:
                    named.append((name, bc.endpoint_from_file(path, wait_s=a.wait)))
                except bc.BridgeError as e:
                    print(f"{name}: {e}", file=sys.stderr)
                    named.append((name, ""))
            if not named:
                raise ValueError("give at least one --endpoint or --endpoint-file")
            copies = [CopyState(n, ep or None) for n, ep in named]
    except (SpecError, ValueError) as e:
        print(str(e), file=sys.stderr)
        return 1
    insp = Inspector(spec, copies, connect_wait_s=a.wait, col_w=a.col_width)
    n = 0
    answered = False
    try:
        while True:
            reports = insp.refresh()
            answered = answered or any(r["reachable"] for r in reports)
            if a.json:
                print(json.dumps({"reports": reports, "problems": insp.problems(reports)}, indent=1, default=str), file=out)
            else:
                if a.watch:
                    print(f"--- refresh {n + 1} at {time.strftime('%H:%M:%S')} ---", file=out)
                print(insp.render(reports), file=out)
            n += 1
            if not a.watch or (a.count and n >= a.count):
                break
            out.flush()
            time.sleep(a.watch)
    except KeyboardInterrupt:
        pass
    finally:
        insp.close()
    return 0 if answered else 2


if __name__ == "__main__":
    sys.exit(main())
