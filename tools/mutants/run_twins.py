"""Run a mutation twin table: every twin must make the test named for it fail.

    python -I tools/mutants/run_twins.py TABLE.json [--jobs N] [--only NAME ...] [--verbose] [--json FILE] [--list]

A twin is a deliberately broken variant of your code, switched on at run time with the environment variable COOP_MUT (see
coop_mut.h and coop_mut.py). The table says which test must fail for which twin. The runner:

  1. builds once (optional "build" command),
  2. runs every test WITHOUT a twin: each must pass,
  3. runs the named test(s) WITH each twin: at least one must fail ("caught"),
  4. prints one line per twin and ALL OK as the last line only when every twin was caught and nothing else is wrong.

Table (JSON, paths relative to the table file unless absolute):

    {
      "label": "my suite",
      "cmd": ["build/my_tests.exe", "{test}"],      the command; {test} {twin} {dir} are replaced in every argument
      "env_var": "COOP_MUT",                          default COOP_MUT
      "build": ["python", "build.py"],                optional, run once before everything
      "twin_build": ["python", "build.py"],           optional, run for each twin (compile time twins). {twin} is replaced
      "real_regex": ", 0 failed",                     optional: a real run must also print this
      "caught_regex": ", [1-9][0-9]* failed",         optional: a caught twin must also print this (so a crash is not "caught")
      "timeout_s": 300, "jobs": 2, "cwd": ".",
      "scan": ["src/**/*.cpp", "py/*.py"],            optional: check the sources against the table (see below)
      "tests": ["test_add", "test_even"],             optional: all tests; default is every test named by a twin
      "twins": {"add_wrong_operator": "test_add",     twin name -> the test that must fail
                "skip_even_check": ["test_even", "test_range"]}    a list: at least one of them must fail
    }

"twins" may also be a list of objects {"name", "test" (string or list), "mut" (the COOP_MUT value, default the name), "cmd", "build"}.
Without {test} in "cmd" the whole program runs once per twin and the test names are only labels.

What counts as caught: a non-zero exit code that is not a loader error (missing DLL, bad image), and "caught_regex" matches if
given. A hang past timeout_s counts as caught (the real run does not hang).

Failures, each printed and each making the exit code 1:
  MISSED        the named test passed with the twin on: the test proves nothing about that code, or the twin is not wired in
  WRONG TEST    the named test passed but another test failed: the twin breaks something else, fix the table or the test
  BUILD CRASH   the build of a compile time twin failed: a twin must compile and then fail its test
  UNLISTED / STALE (with "scan")  a twin name used in the sources is not in the table, or a table row has no call site

Exit codes: 0 all caught, 1 a twin was missed or the real run or build failed, 2 the table is wrong.
"""
import argparse
import glob
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LOADER_ERRORS = {127, 0xC0000135, 0xC0000139, 0xC000007B, 0xC0000142}  # not found, entry point, bad image, DLL init failed
TAIL = 6

CPP_SITE = re.compile(r"COOP_MUT_IF\(\s*(?:\"([^\"]+)\"|(\d+))\s*\)")
PY_SITE = re.compile(r"\bmut\(\s*(?:([\"'])([^\"']+)\1|(\d+))\s*\)")


class TableError(Exception):
    pass


# ------------------------------------------------------------------------------------------------ table
def _as_cmd(v, what):
    if v is None:
        return None
    if isinstance(v, str):
        return shlex.split(v, posix=(os.name != "nt"))
    if isinstance(v, list) and v and all(isinstance(x, str) for x in v):
        return list(v)
    raise TableError(f"{what} must be a non-empty list of strings (or one string)")


def _as_tests(v, what):
    if isinstance(v, str):
        return [v]
    if isinstance(v, list) and v and all(isinstance(x, str) and x for x in v):
        return list(v)
    raise TableError(f"{what}: the test must be a name or a non-empty list of names")


def load_table(src, base=None):
    """Read and check a table (a path or a dict). Returns a normalised dict. Raises TableError."""
    if isinstance(src, (str, Path)):
        p = Path(src)
        try:
            raw = json.loads(p.read_text(encoding="utf-8"))
        except OSError as e:
            raise TableError(f"cannot read {p}: {e}")
        except ValueError as e:
            raise TableError(f"{p} is not valid JSON: {e}")
        base = p.resolve().parent
    else:
        raw = dict(src)
        base = Path(base or os.getcwd()).resolve()
    if not isinstance(raw, dict):
        raise TableError("the table must be a JSON object")
    known = {"label", "cmd", "env_var", "build", "twin_build", "real_regex", "caught_regex", "timeout_s", "jobs", "cwd",
             "scan", "tests", "twins"}
    unknown = [k for k in raw if k not in known and not k.startswith("_")]
    if unknown:
        raise TableError(f"unknown table keys: {unknown}")
    t = {"label": str(raw.get("label") or "twins"), "env_var": str(raw.get("env_var") or "COOP_MUT"),
         "cmd": _as_cmd(raw.get("cmd"), "cmd"), "build": _as_cmd(raw.get("build"), "build"),
         "twin_build": _as_cmd(raw.get("twin_build"), "twin_build"), "real_regex": raw.get("real_regex"),
         "caught_regex": raw.get("caught_regex"), "timeout_s": float(raw.get("timeout_s", 300)),
         "jobs": int(raw.get("jobs", 2)), "base": base, "cwd": str((base / raw.get("cwd", ".")).resolve()),
         "scan": list(raw.get("scan") or [])}
    if not t["cmd"]:
        raise TableError("the table needs a 'cmd'")
    for rx in ("real_regex", "caught_regex"):
        if t[rx]:
            try:
                re.compile(t[rx])
            except re.error as e:
                raise TableError(f"{rx}: {e}")
    twins_raw = raw.get("twins")
    items = []
    if isinstance(twins_raw, dict):
        items = [{"name": k, "test": v} for k, v in twins_raw.items()]
    elif isinstance(twins_raw, list):
        items = list(twins_raw)
    if not items:
        raise TableError("the table needs at least one twin")
    twins, seen = [], set()
    for it in items:
        if not isinstance(it, dict) or "name" not in it:
            raise TableError(f"a twin entry needs a name: {it!r}")
        name = str(it["name"])
        if not name or any(c.isspace() for c in name):
            raise TableError(f"twin name {name!r} must be non-empty text without spaces")
        if name in seen:
            raise TableError(f"twin {name!r} is listed twice")
        seen.add(name)
        tests = _as_tests(it.get("test", "*"), f"twin {name}") if "test" in it else ["*"]
        twins.append({"name": name, "mut": str(it.get("mut", name)), "tests": tests, "cmd": _as_cmd(it.get("cmd"), f"twin {name} cmd"),
                      "build": _as_cmd(it.get("build"), f"twin {name} build")})
    t["twins"] = twins
    uses_test = any("{test}" in a for a in t["cmd"]) or any(tw["cmd"] and any("{test}" in a for a in tw["cmd"]) for tw in twins)
    if uses_test:
        if any("*" in tw["tests"] for tw in twins):
            raise TableError("the command uses {test}: every twin needs a 'test'")
        named = []
        for tw in twins:
            for x in tw["tests"]:
                if x not in named:
                    named.append(x)
        tests = list(raw["tests"]) if raw.get("tests") else named
        missing = [x for x in named if x not in tests]
        if missing:
            raise TableError(f"tests named by twins but missing from 'tests': {missing}")
        t["tests"] = tests
    else:
        t["tests"] = ["*"]
        for tw in twins:                   # no {test} in the command: the names are only labels and the program runs once
            tw["labels"] = tw["tests"]
            tw["tests"] = ["*"]
    for tw in twins:
        tw.setdefault("labels", tw["tests"])
    return t


# ------------------------------------------------------------------------------------------------ running
def clean_env(var):
    env = dict(os.environ)
    env.pop(var, None)
    return env


def expand(argv, test, twin, table):
    out = []
    for a in argv:
        out.append(a.replace("{test}", test).replace("{twin}", twin).replace("{dir}", str(table["base"])))
    return out


def _resolve_exe(argv, cwd):
    """CreateProcess does not resolve a relative program path against our cwd reliably: make a path that exists absolute."""
    if argv and ("/" in argv[0] or "\\" in argv[0]):
        cand = Path(argv[0])
        if not cand.is_absolute():
            cand = Path(cwd) / cand
        if cand.exists():
            argv = [str(cand.resolve())] + argv[1:]
    return argv


def run_proc(argv, env, timeout, cwd):
    """(returncode, output, timed_out)."""
    argv = _resolve_exe(argv, cwd)
    try:
        p = subprocess.run(argv, capture_output=True, text=True, errors="replace", env=env, timeout=timeout, cwd=cwd)
    except FileNotFoundError:
        return 127, "program not found: " + argv[0], False
    except subprocess.TimeoutExpired:
        return 1, "timeout", True
    return p.returncode, (p.stdout or "") + (p.stderr or ""), False


def tail(text, n=TAIL):
    return "\n    ".join(text.rstrip().splitlines()[-n:])


def run_build(table, argv, twin=None, mut=None):
    """Run a build command. With a twin the environment variable names it, so a build can compile that twin in."""
    env = clean_env(table["env_var"])
    if twin is not None:
        env[table["env_var"]] = mut
    argv = expand(argv, "*", twin or "", table)
    rc, out, _ = run_proc(argv, env, table["timeout_s"] * 4, table["cwd"])
    return rc, out


def is_caught(rc, out, timed_out, caught_regex):
    """A failing run of the test counts as a caught twin; a loader error or a crash without the expected text does not."""
    if timed_out:
        return True
    if rc == 0 or (rc & 0xFFFFFFFF) in LOADER_ERRORS:
        return False
    if caught_regex and not re.search(caught_regex, out):
        return False
    return True


def run_test(table, twin_cmd, test, mut):
    """Run one test, with twin `mut` on, or off when mut is None."""
    env = clean_env(table["env_var"])
    if mut is not None:
        env[table["env_var"]] = mut
    argv = expand(twin_cmd or table["cmd"], test, mut or "", table)
    return run_proc(argv, env, table["timeout_s"], table["cwd"])


def run_real(table):
    """Every test without a twin. Returns (ok, lines)."""
    lines = []
    for test in table["tests"]:
        rc, out, to = run_test(table, None, test, None)
        if rc != 0 or to:
            lines.append(f"REAL RUN FAILED: test {test!r} exit {rc}{' (timeout)' if to else ''}\n    {tail(out)}")
        elif table["real_regex"] and not re.search(table["real_regex"], out):
            lines.append(f"REAL RUN: test {test!r} exited 0 but its output does not match {table['real_regex']!r}\n    {tail(out)}")
    return not lines, lines


def find_broken_tests(table, tw, skip):
    """With the twin on, which of the other tests fail? Used to say that a twin broke the wrong test."""
    broken = []
    for test in table["tests"]:
        if test in skip or test == "*":
            continue
        rc, out, to = run_test(table, tw["cmd"], test, tw["mut"])
        if is_caught(rc, out, to, table["caught_regex"]):
            broken.append(test)
    return broken


def run_twin(table, tw):
    """-> dict(name, status, detail). status: caught, missed, wrong_test, build_crash."""
    res = {"name": tw["name"], "status": "missed", "detail": "", "tests": tw["labels"]}
    names = ", ".join(tw["labels"])
    build = tw["build"] or table["twin_build"]
    if build:
        rc, out = run_build(table, build, tw["name"], tw["mut"])
        if rc != 0:
            res.update(status="build_crash", detail=f"build exited {rc}\n    {tail(out)}")
            return res
    last = ""
    for test in tw["tests"]:
        rc, out, to = run_test(table, tw["cmd"], test, tw["mut"])
        last = out
        if is_caught(rc, out, to, table["caught_regex"]):
            res.update(status="caught", detail=f"test {names}: " + ("timeout" if to else f"exit {rc}"))
            return res
    other = find_broken_tests(table, tw, set(tw["tests"]))
    if other:
        res.update(status="wrong_test", detail=f"{names} passed, but {', '.join(other)} failed")
    else:
        res.update(status="missed", detail=f"{', '.join(tw['tests'])} passed with {table['env_var']}={tw['mut']}\n    {tail(last)}")
    return res


# ------------------------------------------------------------------------------------------------ scan
def scan_sites(table):
    """Twin names used in the sources named by 'scan'. Returns (names, files_seen)."""
    names, seen = set(), 0
    for pat in table["scan"]:
        full = pat if os.path.isabs(pat) else str(Path(table["base"]) / pat)
        for f in glob.glob(full, recursive=True):
            fp = Path(f)
            if not fp.is_file():
                continue
            seen += 1
            text = fp.read_text(encoding="utf-8", errors="replace")
            for m in CPP_SITE.finditer(text):
                names.add(m.group(1) or m.group(2))
            if fp.suffix == ".py":
                for m in PY_SITE.finditer(text):
                    names.add(m.group(2) or m.group(3))
    return names, seen


def scan_problems(table):
    if not table["scan"]:
        return []
    names, seen = scan_sites(table)
    if not seen:
        return [f"SCAN: no source file matches {table['scan']}"]
    listed = {tw["mut"] for tw in table["twins"]}
    probs = [f"UNLISTED: twin {n!r} is used in the sources but is not in the table (it is never run)" for n in sorted(names - listed)]
    probs += [f"STALE: twin {n!r} is in the table but no call site uses it" for n in sorted(listed - names)]
    return probs


def restore_build(table, say):
    """After compile time twins: build again without a twin so the tree is left with the real code. True when that worked."""
    if not table["build"]:
        return True
    rc, bout = run_build(table, table["build"])
    if rc != 0:
        say(f"{table['label']}: the rebuild without a twin failed (exit {rc})\n    {tail(bout)}")
        return False
    return True


def all_ok(results, real_ok, build_ok, problems):
    return real_ok and build_ok and not problems and all(r["status"] == "caught" for r in results)


# ------------------------------------------------------------------------------------------------ driver
def run_table(table, jobs=None, only=None, verbose=False, json_path=None, out=None):
    """Run everything. Returns the exit code (0 or 1). Prints to `out` (default stdout)."""
    out = out or sys.stdout

    def say(s=""):
        print(s, file=out, flush=True)

    label = table["label"]
    twins = [tw for tw in table["twins"] if not only or tw["name"] in only]
    if only and len(twins) != len(set(only)):
        missing = sorted(set(only) - {tw["name"] for tw in table["twins"]})
        say(f"{label}: no such twin: {missing}")
        return 2
    problems = scan_problems(table)
    for p in problems:
        say(p)
    build_ok = True
    if table["build"]:
        rc, bout = run_build(table, table["build"])
        if rc != 0:
            say(f"{label}: BUILD FAILED (exit {rc})\n    {tail(bout)}")
            build_ok = False
    report = {"label": label, "real": None, "twins": [], "scan_problems": problems}
    if not build_ok:
        report["ok"] = False
        _write_json(json_path, report)
        return 1
    real_ok, real_lines = run_real(table)
    for ln in real_lines:
        say(ln)
    report["real"] = real_ok
    if not real_ok:
        say(f"{label}: the real run fails, so no twin result can be trusted")
        report["ok"] = False
        _write_json(json_path, report)
        return 1
    serial = bool(table["twin_build"] or any(tw["build"] for tw in twins))
    workers = 1 if serial else max(1, jobs or table["jobs"])
    results = []
    try:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            for r in ex.map(lambda tw: run_twin(table, tw), twins):
                results.append(r)
                _print_result(say, r, verbose)
    finally:
        if serial and not restore_build(table, say):
            build_ok = False
    report["twins"] = results
    ok = all_ok(results, real_ok, build_ok, problems)
    report["ok"] = ok
    _write_json(json_path, report)
    bad = [r for r in results if r["status"] != "caught"]
    if ok:
        say(f"{label}: real tests pass, all {len(results)} twins caught - ALL OK")
        return 0
    parts = []
    if bad:
        parts.append(f"{len(bad)} of {len(results)} twin(s) not caught: {[r['name'] for r in bad]}")
    if problems:
        parts.append(f"{len(problems)} table/source problem(s)")
    if not build_ok:
        parts.append("the final rebuild failed")
    say(f"{label}: " + "; ".join(parts or ["failed"]))
    return 1


def _print_result(say, r, verbose):
    s = r["status"]
    if s == "caught":
        if verbose:
            say(f"  twin {r['name']}: caught ({r['detail']})")
    elif s == "missed":
        say(f"MISSED: twin {r['name']}: {r['detail']}")
    elif s == "wrong_test":
        say(f"WRONG TEST: twin {r['name']}: {r['detail']}")
    else:
        say(f"BUILD CRASH: twin {r['name']}: {r['detail']}")


def _write_json(path, report):
    if path:
        Path(path).write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("table")
    ap.add_argument("--jobs", type=int, default=None)
    ap.add_argument("--only", nargs="+")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--json", dest="json_path")
    ap.add_argument("--list", action="store_true", help="print the twins and their tests, run nothing")
    a = ap.parse_args(argv)
    try:
        table = load_table(a.table)
    except TableError as e:
        print(f"table error: {e}", file=sys.stderr)
        return 2
    if a.list:
        for tw in table["twins"]:
            print(f"{tw['name']}\t{','.join(tw['tests'])}")
        return 0
    return run_table(table, a.jobs, set(a.only) if a.only else None, a.verbose, a.json_path)


if __name__ == "__main__":
    sys.exit(main())
