"""Auto-shrink (delta debugging) of a failing replay fixture, plus the fixture builder and the pluggable replay hooks.

The replay hook is the only game-specific part. It is a function "fixture in, did it fail out", and there are two ways to plug one in:
  * a command (CommandOracle): `<program> ... {fixture}`; the token {fixture} becomes the path of a temp JSON file;
  * a Python function (PyOracle): "module:function", called with the fixture dict.
toy_replay.py in this folder is a complete example of both.

Fixture JSON v1: {"version":1, "config":{...}, "steps":[{"op":"deliver"|"inject"|"quiesce"|..., ...}]}
Every step must be independent: deleting any subset leaves a valid fixture. Steps with "pin": true are never deleted
(use it for a final check step such as "quiesce"). The list may also be called "ops" (or any name you pass as list_key).

Replay result contract (command): exit 0 = converged, exit 1 = diverged, exit 2 = bad fixture. stdout may end in one JSON line
{"diverged": bool, "violations": [...], "first": {...}}; or print one line per broken rule: `VIOLATION <rule-name> <detail>`.
The failure signature is the first rule name (or the first violation with numbers and hashes removed), so a shrink that
turns the failure into a different one is rejected.

The reduction is the ddmin algorithm of Zeller and Hildebrandt, "Simplifying and Isolating Failure-Inducing Input" (2002).
"""
import importlib
import json
import os
import re
import subprocess
import sys
import tempfile
import time

import cofr as C
import analyze as A


class OracleError(Exception):
    pass


# ---------------------------------------------------------------- signatures

_NORM = re.compile(r"0x[0-9a-fA-F]+|[0-9a-fA-F]{8,}|\d+")


def signature(result):
    """A short text that names the kind of failure, with numbers and hashes removed, so a shrink that
    changes the failure into a different one is rejected."""
    if not result.get("diverged"):
        return None
    if result.get("invariant"):
        return str(result["invariant"])
    v = result.get("violations") or []
    if v:
        return _NORM.sub("#", str(v[0]))[:160]
    first = result.get("first") or {}
    return "diverged:" + str(first.get("category", "?"))


# ---------------------------------------------------------------- oracles

def _msvcrt_split(s):
    """Parse Windows argument quoting, including list2cmdline output and doubled
    quotes inside a quoted argument. This is not shell parsing: metacharacters
    are passed literally. shlex(posix=False) incorrectly retains path quotes."""
    args, i, n = [], 0, len(s)
    while True:
        while i < n and s[i] in " \t":
            i += 1
        if i >= n:
            return args
        cur, inq = [], False
        while True:
            if i >= n or (not inq and s[i] in " \t"):
                args.append("".join(cur))
                if i >= n:
                    return args
                i += 1
                break
            c = s[i]
            if c == "\\":
                bs = 0
                while i < n and s[i] == "\\":
                    bs += 1
                    i += 1
                if i < n and s[i] == '"':
                    cur.append("\\" * (bs // 2))
                    if bs % 2:
                        cur.append('"')
                        i += 1
                    else:
                        if inq and i + 1 < n and s[i + 1] == '"':
                            cur.append('"')
                            i += 2
                        else:
                            inq = not inq
                            i += 1
                else:
                    cur.append("\\" * bs)
            elif c == '"':
                if inq and i + 1 < n and s[i + 1] == '"':
                    cur.append('"')
                    i += 2
                else:
                    inq = not inq
                    i += 1
            else:
                cur.append(c)
                i += 1


class CommandOracle:
    """Runs an external replay program. cmd is a list of args (or a string) with the token {fixture} replaced by a temp file."""

    def __init__(self, cmd, tmpdir=None, timeout=60):
        if isinstance(cmd, str):
            if os.name == "nt":
                cmd = _msvcrt_split(cmd)
            else:
                import shlex
                cmd = shlex.split(cmd, posix=True)
        if not any("{fixture}" in c for c in cmd):
            raise OracleError("replay command must contain {fixture}")
        self.cmd = cmd
        self.tmpdir = tmpdir
        self.timeout = timeout
        self.runs = 0

    def __call__(self, fixture):
        self.runs += 1
        fd, path = tempfile.mkstemp(suffix=".json", prefix="shrink_", dir=self.tmpdir)
        try:
            with os.fdopen(fd, "w") as f:
                json.dump(fixture, f)
            args = [c.replace("{fixture}", path) for c in self.cmd]
            try:
                p = subprocess.run(args, capture_output=True, text=True, timeout=self.timeout, errors="replace")
            except subprocess.TimeoutExpired:
                raise OracleError("replay timed out after %ss" % self.timeout)
            except OSError as e:
                raise OracleError("cannot start replay: %s" % e)
        finally:
            try:
                os.remove(path)
            except OSError:
                pass
        if p.returncode not in (0, 1):
            raise OracleError("replay exited with %d: %s" % (p.returncode, (p.stdout + p.stderr).strip()[-300:]))
        result = {}
        viol = []
        for line in p.stdout.splitlines():
            line = line.strip()
            if line.startswith("VIOLATION "):           # VIOLATION <rule-name> <detail>
                parts = line.split(None, 2)
                viol.append(parts[1] + (" " + parts[2] if len(parts) > 2 else ""))
                result.setdefault("invariant", parts[1])
        for line in reversed(p.stdout.splitlines()):
            line = line.strip()
            if line.startswith("{"):
                try:
                    result = dict(json.loads(line), **{k: v for k, v in result.items() if k == "invariant"})
                except ValueError:
                    pass
                break
        if viol and not result.get("violations"):
            result["violations"] = viol
        if p.returncode == 1:
            result["diverged"] = True
        else:
            result["diverged"] = False
        return result


class PyOracle:
    """Calls a Python function "module:function" with the fixture dict. It returns {"diverged": bool, ...} like the command contract.
    `path` is added to the import path first (python -I does not search the current folder)."""

    def __init__(self, spec, path=None):
        if ":" not in spec:
            raise OracleError("a replay function is written module:function")
        mod, fn = spec.split(":", 1)
        if path:
            sys.path.insert(0, os.path.abspath(path))
        try:
            self.fn = getattr(importlib.import_module(mod), fn)
        except (ImportError, AttributeError) as e:
            raise OracleError("cannot load %s: %s" % (spec, e))
        self.runs = 0

    def __call__(self, fixture):
        self.runs += 1
        try:
            result = self.fn(json.loads(json.dumps(fixture)))
        except Exception as e:   # a hook that crashes on a bad fixture is a bad fixture
            raise OracleError("the replay function raised %s: %s" % (type(e).__name__, e))
        if not isinstance(result, dict) or "diverged" not in result:
            raise OracleError("the replay function must return a dict with a 'diverged' key")
        return result


# ---------------------------------------------------------------- fixture from traces

def fixture_from_traces(traces, config=None):
    """Turn recorded traces into a replay fixture: every inbound datagram of every machine becomes a
    deliver step, in aligned-time order, followed by a pinned quiesce step."""
    a = A.align(traces)
    items = []
    skipped = 0
    for s in a.slots:
        for r in a.traces[s]["records"]:
            if r.get("malformed"):
                continue
            t = a.t(s, r)
            if r["type"] == C.T_NET_IN:
                if r["truncated"] or not r["raw"]:
                    skipped += 1
                    continue
                items.append((t, s, r["idx"], {"op": "deliver", "to": s, "from": r["peer"], "hex": r["raw"].hex()}))
    items.sort(key=lambda x: (x[0], x[1], x[2]))
    t0 = items[0][0] if items else 0
    steps = []
    for t, _, _, st in items:
        st["t"] = int((t - t0) / 1000)
        steps.append(st)
    steps.append({"op": "quiesce", "pin": True})
    cfg = {"seed": 1, "clients": max(1, len(a.slots) - 1)}
    cfg.update(config or {})
    return {"version": 1, "config": cfg, "steps": steps, "source": {"slots": a.slots, "skipped_steps": skipped}}


# ---------------------------------------------------------------- ddmin

def ddmin(count, fails, max_runs=5000, deadline=None):
    """ddmin over item indices 0..count-1. fails(list_of_indices) -> True if the failure persists.
    Returns (kept_indices, runs, complete) where complete is False if a budget stopped it early."""
    runs = [0]
    cache = {}

    def test(idx):
        key = tuple(idx)
        if key in cache:
            return cache[key]
        if runs[0] >= max_runs or (deadline is not None and time.time() > deadline):
            raise _Budget()
        runs[0] += 1
        cache[key] = bool(fails(list(idx)))
        return cache[key]

    cur = list(range(count))
    n = 2
    complete = True
    try:
        if count and test([]):
            return [], runs[0], True
        while len(cur) >= 2:
            size = max(1, len(cur) // n)
            chunks = [cur[i:i + size] for i in range(0, len(cur), size)]
            reduced = False
            for ch in chunks:
                if len(ch) < len(cur) and test(ch):
                    cur, n, reduced = ch, 2, True
                    break
            if not reduced:
                for ch in chunks:
                    comp = [x for x in cur if x not in set(ch)]
                    if comp and len(comp) < len(cur) and test(comp):
                        cur, n, reduced = comp, max(n - 1, 2), True
                        break
            if not reduced:
                if n >= len(cur):
                    break
                n = min(len(cur), n * 2)
    except _Budget:
        complete = False
    return cur, runs[0], complete


class _Budget(Exception):
    pass


def list_key_of(fixture, list_key=None):
    if list_key:
        if list_key not in fixture:
            raise OracleError("the fixture has no list called %r" % list_key)
        return list_key
    return "ops" if "ops" in fixture and "steps" not in fixture else "steps"


def shrink_fixture(fixture, oracle, max_runs=2000, max_seconds=None, list_key=None):
    """Reduce fixture[list_key] to a (1-)minimal list that still fails with the same signature."""
    key = list_key_of(fixture, list_key)
    steps = fixture[key]
    pinned = [i for i, s in enumerate(steps) if s.get("pin")]
    free = [i for i in range(len(steps)) if i not in set(pinned)]
    base = oracle(fixture)
    sig = signature(base)
    if sig is None:
        raise OracleError("the original fixture does not diverge; nothing to shrink")

    def build(sel):
        keep = sorted(set(pinned) | {free[j] for j in sel})
        f = dict(fixture)
        f[key] = [steps[i] for i in keep]
        return f

    def fails(sel):
        r = oracle(build(sel))
        return r.get("diverged") and signature(r) == sig

    deadline = time.time() + max_seconds if max_seconds else None
    kept, runs, complete = ddmin(len(free), fails, max_runs=max_runs, deadline=deadline)
    out = build(kept)
    final = oracle(out)
    out["shrunk"] = {"signature": sig, "original_steps": len(steps), "reduced_steps": len(out[key]), "list_key": key,
                     "oracle_runs": runs + 2, "complete": complete, "first": final.get("first"),
                     "violations": final.get("violations", [])[:5]}
    return out
