"""Tests for the mutation twin tools (coop_mut.h, coop_mut.py, run_twins.py), then a twin for each guard.
Prints ALL OK as the last line only when every check passes and every twin is caught.

    python -I tools/mutants/run_tests.py [--build-dir DIR] [--no-cpp]

Needs cmake and a C++17 compiler for the C++ part (Visual Studio 2022 on Windows, gcc or clang elsewhere). --no-cpp skips that
part and says so; the last line is then still ALL OK only for the Python part. The build folder defaults to a folder under the
system temp directory (override with --build-dir or the COOP_BUILD_DIR environment variable).

How twins work here: a check runs once against the real code, then again with one guard broken on purpose from outside
(a function of run_twins.py replaced, or a copy of coop_mut.h / coop_mut.py with one line changed). A check that still passes
against the broken code proves nothing, and the run fails.

The runner checks use fake programs (tests/fake_suite.py, tests/fake_build.py). No game is involved.
"""
import argparse
import contextlib
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import run_twins as rt  # noqa: E402

PY = sys.executable
FAKE = HERE / "tests" / "fake_suite.py"
FAKE_BUILD = HERE / "tests" / "fake_build.py"
TMPS = []
_UNDO = []
OPTS = {"build_dir": None, "no_cpp": False}


# ---------------------------------------------------------------------------------------------- helpers
def patch(obj, name, value):
    _UNDO.append((obj, name, getattr(obj, name)))
    setattr(obj, name, value)


def undo():
    while _UNDO:
        obj, name, old = _UNDO.pop()
        setattr(obj, name, old)


def hook(mut, **objs):
    if mut:
        mut(**objs)


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def raises(exc, fn, *a, **k):
    try:
        fn(*a, **k)
    except exc:
        return True
    return False


def tmpdir(prefix="mut-"):
    p = Path(tempfile.mkdtemp(prefix=prefix))
    TMPS.append(p)
    return p


@contextlib.contextmanager
def env(**kv):
    old = {k: os.environ.get(k) for k in kv}
    for k, v in kv.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = str(v)
    try:
        yield
    finally:
        for k, v in old.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def fake_table(spec, twins, **kw):
    """A table that runs tests/fake_suite.py. spec: what the fake does (see fake_suite.py). twins: name -> test."""
    t = {"label": "fake", "cmd": [PY, "-I", str(FAKE), "{test}"], "timeout_s": 30, "jobs": 2, "twins": twins}
    t.update(kw)
    return t, spec


def cli(args, **envkw):
    """run_twins.main(args) in this process. Returns (exit code, stdout, stderr)."""
    o, e = io.StringIO(), io.StringIO()
    with env(**envkw), contextlib.redirect_stdout(o), contextlib.redirect_stderr(e):
        rc = rt.main(args)
    return rc, o.getvalue(), e.getvalue()


def run_fake(table, spec, jobs=None, only=None, verbose=False, json_path=None, **envkw):
    """Run a table in this process. Returns (exit code, printed text)."""
    buf = io.StringIO()
    with env(FAKE_SPEC=json.dumps(spec), **envkw):
        rc = rt.run_table(rt.load_table(table, HERE), jobs=jobs, only=only, verbose=verbose, json_path=json_path, out=buf)
    return rc, buf.getvalue()


# ---------------------------------------------------------------------------------------------- run_twins checks
def check_all_caught(mut):
    hook(mut)
    spec = {"t_add": {"test_add": 1}, "t_even": {"test_even": 1}, "t_label": {"test_label": 3}}
    table, spec = fake_table(spec, {"t_add": "test_add", "t_even": "test_even", "t_label": "test_label"})
    rc, out = run_fake(table, spec, verbose=True)
    check(rc == 0, f"all three twins are caught, exit {rc}:\n{out}")
    check(out.rstrip().splitlines()[-1].endswith("ALL OK") and "all 3 twins caught" in out, f"ALL OK is the last line: {out}")
    check("twin t_label: caught" in out, f"a crash exit code counts as caught without a caught_regex: {out}")


def check_missed_twin(mut):
    hook(mut)
    spec = {"good": {"test_a": 1}}                              # twin "dud" changes nothing
    table, spec = fake_table(spec, {"good": "test_a", "dud": "test_b"})
    rc, out = run_fake(table, spec)
    check(rc == 1, f"a missed twin fails the run, exit {rc}: {out}")
    check("MISSED: twin dud" in out, f"the missed twin is named: {out}")
    check("ALL OK" not in out, f"no ALL OK with a missed twin: {out}")
    check("MISSED: twin good" not in out, f"the caught twin is not reported as missed: {out}")


def check_wrong_test(mut):
    hook(mut)
    spec = {"misplaced": {"test_b": 1}}                         # the table says test_a must fail, test_b fails instead
    table, spec = fake_table(spec, {"misplaced": "test_a", "other": "test_b"})
    rc, out = run_fake(table, spec)
    check(rc == 1, f"a twin that breaks the wrong test fails the run, exit {rc}: {out}")
    check("WRONG TEST: twin misplaced" in out and "test_b failed" in out, f"the wrong test is named: {out}")
    check("ALL OK" not in out, out)


def check_build_crash(mut):
    """A compile time twin that does not build is reported and the tree is rebuilt without a twin."""
    t = tmpdir()
    log = t / "build.log"
    spec = {"ok_twin": {"test_a": 1}, "broken_twin": {"test_b": 1}}
    table, spec = fake_table(spec, {"ok_twin": "test_a", "broken_twin": "test_b"},
                             build=[PY, "-I", str(FAKE_BUILD)], twin_build=[PY, "-I", str(FAKE_BUILD)])
    hook(mut)
    rc, out = run_fake(table, spec, BUILD_LOG=str(log), BUILD_CRASH_ON="broken_twin")
    check(rc == 1, f"a twin that crashes the build fails the run, exit {rc}: {out}")
    check("BUILD CRASH: twin broken_twin" in out and "C2065" in out, f"the build crash is reported with its output: {out}")
    check("MISSED: twin broken_twin" not in out, f"a build crash is not called a missed twin: {out}")
    lines = log.read_text().split()
    check(lines == ["-", "ok_twin", "broken_twin", "-"], f"build once, once per twin, then again without a twin: {lines}")
    rc, out = run_fake(table, spec, BUILD_LOG=str(log), BUILD_CRASH_ON="nothing")
    check(rc == 0 and "ALL OK" in out, f"the same table passes when every twin builds: {out}")


def check_real_run_failure(mut):
    hook(mut)
    spec = {"_real_fail": ["test_b"], "t": {"test_a": 1}}
    table, spec = fake_table(spec, {"t": "test_a", "u": "test_b"})
    rc, out = run_fake(table, spec)
    check(rc == 1 and "REAL RUN FAILED" in out and "test_b" in out, f"a failing real run stops everything: {rc} {out}")
    check("ALL OK" not in out and "twins caught" not in out, f"no twin result is claimed: {out}")
    table, spec = fake_table({"t": {"test_a": 1}}, {"t": "test_a"}, real_regex=", 0 failed")
    rc, out = run_fake(table, spec)
    check(rc == 0, f"real_regex matches the real output: {rc} {out}")
    table, spec = fake_table({"t": {"test_a": 1}}, {"t": "test_a"}, real_regex="never printed")
    rc, out = run_fake(table, spec)
    check(rc == 1 and "does not match" in out, f"a real run without the expected text fails: {rc} {out}")


def check_loader_error_is_not_caught(mut):
    hook(mut)
    for code in ((127, 0xC0000135) if os.name == "nt" else (127,)):
        table, spec = fake_table({"t": {"test_a": code}}, {"t": "test_a"})
        rc, out = run_fake(table, spec)
        check(rc == 1 and "MISSED: twin t" in out, f"exit {code:#x} is a loader error, not a caught twin: {rc} {out}")
    table, spec = fake_table({"t": {"test_a": 1}}, {"t": "no_such_test"}, cmd=[str(HERE / "no_such_program.exe"), "{test}"])
    rc, out = run_fake(table, spec)
    check(rc == 1 and "REAL RUN FAILED" in out, f"a missing test program fails the real run: {rc} {out}")


def check_caught_regex(mut):
    hook(mut)
    table, spec = fake_table({"t": {"test_a": 3}}, {"t": "test_a"}, caught_regex=", [1-9][0-9]* failed")
    rc, out = run_fake(table, spec)
    check(rc == 1 and "MISSED: twin t" in out, f"a crash without the failed line is not a caught twin: {rc} {out}")
    table, spec = fake_table({"t": {"test_a": 1}}, {"t": "test_a"}, caught_regex="1 failed")
    rc, out = run_fake(table, spec)
    check(rc == 0, f"a failure that prints the expected text is caught: {rc} {out}")


def check_timeout_is_caught(mut):
    hook(mut)
    table, spec = fake_table({"t": {"test_a": "hang"}}, {"t": "test_a"}, timeout_s=1)
    t0 = time.time()
    rc, out = run_fake(table, spec, verbose=True)
    check(rc == 0 and "timeout" in out, f"a hang is a caught twin: {rc} {out}")
    check(time.time() - t0 < 25, "the timeout ends the run")


def check_any_of_tests(mut):
    hook(mut)
    table, spec = fake_table({"t": {"test_b": 1}}, {"t": ["test_a", "test_b"]})
    rc, out = run_fake(table, spec)
    check(rc == 0, f"one of the listed tests failing is enough: {rc} {out}")
    table, spec = fake_table({"t": {"test_c": 1}}, {"t": ["test_a", "test_b"], "u": "test_c"})
    rc, out = run_fake(table, spec)
    check(rc == 1 and "WRONG TEST: twin t" in out and "test_c" in out, f"none of the listed tests failed: {rc} {out}")


def check_env_isolation(mut):
    t = tmpdir()
    log = t / "calls.log"
    table, spec = fake_table({"t": {"test_a": 1}}, {"t": "test_a"})
    hook(mut)
    with env(COOP_MUT="leaked", FAKE_LOG=str(log)):
        rc, out = run_fake(table, spec)
    rows = [ln.split() for ln in log.read_text().splitlines()]
    check(rc == 0, f"{rc} {out}")
    check(rows[0] == ["test_a"], f"the real run does not see a twin from the parent environment: {rows}")
    check(rows[1] == ["test_a", "t"], f"the twin run sees its own twin: {rows}")
    log.unlink()
    table, spec = fake_table({"MY_TWIN_VALUE": {"test_a": 1}}, {"MY_TWIN_VALUE": "test_a"}, env_var="MY_TWIN")
    with env(FAKE_LOG=str(log), FAKE_VAR="MY_TWIN", MY_TWIN="parent"):
        rc, out = run_fake(table, spec)
    rows = [ln.split() for ln in log.read_text().splitlines()]
    check(rc == 0 and rows[0] == ["test_a"] and rows[1] == ["test_a", "MY_TWIN_VALUE"], f"a custom env_var is used and cleaned: {rc} {rows} {out}")


def check_scan(mut):
    t = tmpdir()
    (t / "code.cpp").write_text('int f() { if (COOP_MUT_IF("cpp_twin")) return 1; if (COOP_MUT_IF(3)) return 2; return 0; }\n')
    (t / "code.py").write_text('import coop_mut\n\ndef g():\n    return coop_mut.mut("py_twin") or coop_mut.mut(\'py_single\') or coop_mut.mut(9)\n')
    spec = {"cpp_twin": {"a": 1}, "3": {"b": 1}, "py_twin": {"c": 1}, "py_single": {"d": 1}, "9": {"e": 1}, "ghost": {"f": 1}}
    names = {"cpp_twin": "a", "3": "b", "py_twin": "c", "py_single": "d", "9": "e"}
    hook(mut)
    table, spec2 = fake_table(spec, names, scan=[str(t / "*.cpp"), str(t / "*.py")])
    rc, out = run_fake(table, spec2)
    check(rc == 0 and "ALL OK" in out, f"every call site is in the table: {rc} {out}")
    bigger = dict(names, ghost="f")
    table, spec2 = fake_table(spec, bigger, scan=[str(t / "*.cpp"), str(t / "*.py")])
    rc, out = run_fake(table, spec2)
    check(rc == 1 and "STALE: twin 'ghost'" in out and "ALL OK" not in out, f"a table row without a call site is reported: {rc} {out}")
    smaller = {k: v for k, v in names.items() if k != "py_single"}
    table, spec2 = fake_table(spec, smaller, scan=[str(t / "*.cpp"), str(t / "*.py")])
    rc, out = run_fake(table, spec2)
    check(rc == 1 and "UNLISTED: twin 'py_single'" in out, f"a call site that is not in the table is reported: {rc} {out}")
    table, spec2 = fake_table(spec, names, scan=[str(t / "*.nothing")])
    rc, out = run_fake(table, spec2)
    check(rc == 1 and "no source file matches" in out, f"a scan pattern that finds nothing is reported: {rc} {out}")


def check_table_validation(mut):
    hook(mut)
    base = {"cmd": [PY, "-c", "pass", "{test}"], "twins": {"a": "t"}}
    bad = {
        "no cmd": {"twins": {"a": "t"}},
        "no twins": {"cmd": ["x"]},
        "unknown key": dict(base, bogus=1),
        "duplicate": dict(base, twins=[{"name": "a", "test": "t"}, {"name": "a", "test": "u"}]),
        "{test} without a test": dict(base, twins=[{"name": "a"}]),
        "test not in tests": dict(base, tests=["other"]),
        "bad regex": dict(base, caught_regex="("),
        "space in name": dict(base, twins={"a b": "t"}),
        "empty test list": dict(base, twins={"a": []}),
        "cmd of numbers": dict(base, cmd=[1, 2]),
    }
    for name, tbl in bad.items():
        check(raises(rt.TableError, rt.load_table, tbl, HERE), f"table error expected: {name}")
    ok = rt.load_table(dict(base, twins={"a": "t", "b": ["t", "u"]}), HERE)
    check(ok["tests"] == ["t", "u"] and ok["twins"][1]["tests"] == ["t", "u"], f"tests are collected from the twins: {ok['tests']}")
    t = tmpdir()
    (t / "bad.json").write_text("{not json")
    rc, out, err = cli([str(t / "bad.json")])
    check(rc == 2 and "table error" in err, f"a broken table file exits 2: {rc} {err}")
    rc, out, err = cli([str(t / "missing.json")])
    check(rc == 2, f"a missing table exits 2: {rc}")
    table, spec = fake_table({}, {"a": "t"})
    rc, out = run_fake(table, spec, only={"nope"})
    check(rc == 2 and "no such twin" in out, f"--only with an unknown twin exits 2: {rc} {out}")


def check_parallel_and_only(mut):
    names = {f"t{i}": f"test_{i}" for i in range(6)}
    spec = {f"t{i}": {f"test_{i}": 1} for i in range(6)}
    table, spec = fake_table(spec, names, jobs=3)
    hook(mut)
    rc, out = run_fake(table, spec, jobs=3)
    check(rc == 0 and "all 6 twins caught" in out, f"six twins on three workers: {rc} {out}")
    rc, out = run_fake(table, spec, only={"t1", "t4"})
    check(rc == 0 and "all 2 twins caught" in out, f"--only runs just those: {rc} {out}")


def check_json_report_and_cli(mut):
    t = tmpdir()
    spec = {"t1": {"test_a": 1}}
    table = {"label": "cli", "cmd": [PY, "-I", str(FAKE), "{test}"], "twins": {"t1": "test_a", "dud": "test_b"}}
    (t / "table.json").write_text(json.dumps(table))
    hook(mut)
    rc, out, err = cli([str(t / "table.json"), "--json", str(t / "r.json")], FAKE_SPEC=json.dumps(spec))
    check(rc == 1 and "MISSED: twin dud" in out, f"cli: {rc} {out} {err}")
    check((t / "r.json").exists(), "the json report file is written")
    rep = json.loads((t / "r.json").read_text())
    st = {r["name"]: r["status"] for r in rep["twins"]}
    check(st == {"t1": "caught", "dud": "missed"} and rep["ok"] is False and rep["real"] is True, f"json report: {rep}")
    rc, out, err = cli([str(t / "table.json"), "--list"])
    check(rc == 0 and out.split() == ["t1", "test_a", "dud", "test_b"], f"--list: {out!r}")
    rc, out, err = cli([str(t / "table.json"), "--only", "t1"], FAKE_SPEC=json.dumps(spec))
    check(rc == 0 and out.rstrip().splitlines()[-1].endswith("ALL OK"), f"cli --only: {out}")


def check_example_table_runs(mut):
    hook(mut)
    ex = HERE / "twins.example.json"
    table = rt.load_table(ex)
    check([tw["name"] for tw in table["twins"]] == ["py_add_wrong_operator", "py_first_word_whole_text", "7"], "example twins")
    check(rt.scan_problems(table) == [], f"the example table matches its sources: {rt.scan_problems(table)}")
    table["cmd"][0] = PY                                  # the example says "python"; use this interpreter
    buf = io.StringIO()
    rc = rt.run_table(table, out=buf)
    check(rc == 0 and "all 3 twins caught" in buf.getvalue(), f"the example runs and every twin is caught: {rc} {buf.getvalue()}")
    cp = subprocess.run([PY, "-I", str(HERE / "run_twins.py"), str(ex), "--list"], capture_output=True, text=True)
    check(cp.returncode == 0 and "7	test_clamp" in cp.stdout, f"the command line lists the twins: {cp.returncode} {cp.stdout!r}")


# ---------------------------------------------------------------------------------------------- coop_mut.py and its twins
class PyCtx:
    """A copy of coop_mut.py that a twin may change."""

    def __init__(self):
        self.dir = tmpdir("mut-py-")
        shutil.copy2(HERE / "coop_mut.py", self.dir / "coop_mut.py")

    def replace(self, old, new):
        p = self.dir / "coop_mut.py"
        text = p.read_text(encoding="utf-8")
        if old not in text:
            raise RuntimeError(f"twin does not apply: {old!r} not found in coop_mut.py")
        p.write_text(text.replace(old, new, 1), encoding="utf-8")


def check_python_module(mut):
    ctx = PyCtx()
    hook(mut, py=ctx)
    cp = subprocess.run([PY, "-I", str(HERE / "tests" / "py_mut_checks.py"), str(ctx.dir)], capture_output=True, text=True)
    check(cp.returncode == 0, f"coop_mut.py checks failed:\n{cp.stdout}{cp.stderr}")


def check_python_twin_table(mut):
    ctx = PyCtx()
    (ctx.dir / "tests").mkdir()
    shutil.copy2(HERE / "tests" / "py_under_test.py", ctx.dir / "tests" / "py_under_test.py")
    hook(mut, py=ctx)
    table = {"label": "py", "cmd": [PY, "-I", str(ctx.dir / "tests" / "py_under_test.py"), "{test}"], "real_regex": ", 0 failed",
             "caught_regex": ", [1-9][0-9]* failed", "twins": {"py_add_wrong_operator": "test_add", "py_first_word_whole_text": "test_first_word",
                                                                 "7": "test_clamp"}}
    buf = io.StringIO()
    rc = rt.run_table(rt.load_table(table, HERE), out=buf)
    check(rc == 0 and "all 3 twins caught" in buf.getvalue(), f"the python twins are all caught: {rc} {buf.getvalue()}")


# ---------------------------------------------------------------------------------------------- coop_mut.h and its twins
class CppCtx:
    """Builds tests/CMakeLists.txt against a copy of coop_mut.h that a twin may change."""

    count = 0

    def __init__(self):
        CppCtx.count += 1
        self.n = CppCtx.count
        self.hdr = tmpdir("mut-hdr-")
        shutil.copy2(HERE / "coop_mut.h", self.hdr / "coop_mut.h")
        base = OPTS["build_dir"] or Path(tempfile.gettempdir()) / ("coop-mutants-build-" + hashlib.sha1(str(HERE).encode()).hexdigest()[:10])
        self.build = Path(base) / f"v{self.n}"
        self.tests = self.prod = None

    def replace(self, old, new):
        p = self.hdr / "coop_mut.h"
        text = p.read_text(encoding="utf-8")
        if old not in text:
            raise RuntimeError(f"twin does not apply: {old!r} not found in coop_mut.h")
        p.write_text(text.replace(old, new, 1), encoding="utf-8")

    def do_build(self):
        if self.tests:
            return
        cmake = shutil.which("cmake")
        if not cmake:
            raise RuntimeError("cmake not found on PATH (use --no-cpp to skip the C++ part)")
        e = dict(os.environ)
        for k in ("CMAKE_TOOLCHAIN_FILE", "CMAKE_GENERATOR", "CMAKE_GENERATOR_PLATFORM", "CMAKE_GENERATOR_TOOLSET"):
            e.pop(k, None)
        e.pop("COOP_MUT", None)
        shutil.rmtree(self.build, ignore_errors=True)
        cfg = subprocess.run([cmake, "-S", str(HERE / "tests"), "-B", str(self.build), "-DCMAKE_BUILD_TYPE=Release",
                              f"-DCOOP_MUT_HEADER_DIR={self.hdr.as_posix()}"], capture_output=True, text=True, errors="replace", env=e)
        if cfg.returncode != 0:
            raise RuntimeError("cmake could not configure (is a C++17 compiler installed?)\n" + (cfg.stdout + cfg.stderr)[-1500:])
        b = subprocess.run([cmake, "--build", str(self.build), "--config", "Release", "--parallel", "1"], capture_output=True, text=True,
                           errors="replace", env=e)
        if b.returncode != 0:
            raise RuntimeError("build failed\n" + (b.stdout + b.stderr)[-2500:])
        for sub in ("bin/Release", "bin"):
            for ext in (".exe", ""):
                t, p = self.build / sub / ("coop_mut_cpp_tests" + ext), self.build / sub / ("coop_mut_cpp_prod" + ext)
                if t.is_file() and p.is_file():
                    self.tests, self.prod = t, p
                    return
        raise RuntimeError("test programs not found under " + str(self.build))


def run_exe(exe, *args, **envkw):
    e = dict(os.environ)
    e.pop("COOP_MUT", None)
    for k, v in envkw.items():
        e[k] = v
    cp = subprocess.run([str(exe), *args], capture_output=True, text=True, errors="replace", env=e, timeout=60)
    return cp.returncode, cp.stdout + cp.stderr


_REAL_CTX = []


def cpp_ctx(mut):
    """The real build, built once, or a build against a header that the twin changed."""
    if mut is None:
        if not _REAL_CTX:
            _REAL_CTX.append(CppCtx())
        ctx = _REAL_CTX[0]
    else:
        ctx = CppCtx()
        hook(mut, cpp=ctx)
    ctx.do_build()
    return ctx


def check_cpp_twin_table(mut):
    ctx = cpp_ctx(mut)
    table = {"label": "cpp", "cmd": [str(ctx.tests), "{test}"], "real_regex": ", 0 failed", "caught_regex": ", [1-9][0-9]* failed",
             "twins": {"add_wrong_operator": "test_add", "even_check_removed": "test_even", "label_always_off": "test_label", "4": "test_clamp"}}
    buf = io.StringIO()
    rc = rt.run_table(rt.load_table(table, HERE), out=buf)
    check(rc == 0 and "all 4 twins caught" in buf.getvalue(), f"the C++ twins are all caught by the tests named for them: {rc} {buf.getvalue()}")


def check_cpp_command_line_flag(mut):
    ctx = cpp_ctx(mut)
    rc, out = run_exe(ctx.tests, "--mut", "add_wrong_operator", "test_add")
    check(rc == 1 and "twin = 'add_wrong_operator'" in out, f"--mut NAME switches the twin: {rc} {out}")
    rc, out = run_exe(ctx.tests, "--mut=even_check_removed", "test_even")
    check(rc == 1, f"--mut=NAME switches the twin: {rc} {out}")
    rc, out = run_exe(ctx.tests, "--mut", "4", "test_clamp")
    check(rc == 1, f"--mut 4 switches twin number 4: {rc} {out}")
    rc, out = run_exe(ctx.tests, "test_add")
    check(rc == 0 and "twin = ''" in out, f"no twin by default: {rc} {out}")
    rc, out = run_exe(ctx.tests, "test_add", COOP_MUT="0")
    check(rc == 0, f"COOP_MUT=0 is no twin: {rc} {out}")
    rc, out = run_exe(ctx.tests, "test_add", COOP_MUT="add_wrong_operato")
    check(rc == 0, f"a twin name must match exactly: {rc} {out}")
    rc, out = run_exe(ctx.tests, "test_add", COOP_MUT="label_always_off")
    check(rc == 0, f"a twin only affects its own code: {rc} {out}")


def check_cpp_shipped_build_ignores_twins(mut):
    ctx = cpp_ctx(mut)
    for value in ("add_wrong_operator", "4", "even_check_removed"):
        rc, out = run_exe(ctx.prod, COOP_MUT=value)
        check(rc == 0 and "compiled out" in out and ", 0 failed" in out, f"the shipped shape ignores COOP_MUT={value}: {rc} {out}")
    rc, out = run_exe(ctx.prod, "--mut", "add_wrong_operator", "test_add")
    check(rc == 0, f"the shipped shape does not even know --mut: {rc} {out}")
    check(b"add_wrong_operator" in ctx.tests.read_bytes(), "the test build contains the twin name")
    check(b"add_wrong_operator" not in ctx.prod.read_bytes() and b"COOP_MUT" not in ctx.prod.read_bytes(),
          "the shipped build contains no twin name and never reads COOP_MUT")


# ---------------------------------------------------------------------------------------------- twins
def tw_caught_always(**_):
    patch(rt, "is_caught", lambda rc, out, timed_out, rx: True)


def tw_caught_never(**_):
    patch(rt, "is_caught", lambda rc, out, timed_out, rx: False)


def tw_loader_errors_counted(**_):
    patch(rt, "LOADER_ERRORS", set())


def tw_real_run_skipped(**_):
    patch(rt, "run_real", lambda table: (True, []))


def tw_env_leaks(**_):
    def leaky(var):
        e = dict(os.environ)
        e[var] = "leaked"
        return e
    patch(rt, "clean_env", leaky)


def tw_wrong_test_not_found(**_):
    patch(rt, "find_broken_tests", lambda table, tw, skip: [])


def tw_build_crash_ignored(**_):
    real = rt.run_build
    patch(rt, "run_build", lambda table, argv, twin=None, mut=None: (0, "") if twin is not None else real(table, argv, twin, mut))


def tw_no_restore(**_):
    patch(rt, "restore_build", lambda table, say: True)


def tw_scan_ignored(**_):
    patch(rt, "scan_problems", lambda table: [])


def tw_all_ok_always(**_):
    patch(rt, "all_ok", lambda results, real_ok, build_ok, problems: True)


def tw_timeout_not_caught(**_):
    real = rt.is_caught
    patch(rt, "is_caught", lambda rc, out, timed_out, rx: False if timed_out else real(rc, out, timed_out, rx))


def tw_json_not_written(**_):
    patch(rt, "_write_json", lambda path, report: None)


def tw_no_table_checks(**_):
    patch(rt, "_as_tests", lambda v, what: [v] if isinstance(v, str) else list(v))


def tw_only_ignored(**_):
    real = rt.run_table
    patch(rt, "run_table", lambda table, jobs=None, only=None, verbose=False, json_path=None, out=None: real(table, jobs, None, verbose, json_path, out))


def tw_scan_ignores_numbers(**_):
    patch(rt, "PY_SITE", rt.re.compile(r"\bmut\(\s*([\"'])([^\"']+)\1()\s*\)"))
    patch(rt, "CPP_SITE", rt.re.compile(r"COOP_MUT_IF\(\s*(?:\"([^\"]+)\")()\s*\)"))


def hdr(old, new):
    def apply(cpp=None, **_):
        cpp.replace(old, new)
    return apply


def pyt(old, new):
    def apply(py=None, **_):
        py.replace(old, new)
    return apply


REAL = [
    check_all_caught, check_missed_twin, check_wrong_test, check_build_crash, check_real_run_failure,
    check_loader_error_is_not_caught, check_caught_regex, check_timeout_is_caught, check_any_of_tests, check_env_isolation,
    check_scan, check_table_validation, check_parallel_and_only, check_json_report_and_cli, check_example_table_runs,
    check_python_module, check_python_twin_table,
]
CPP_REAL = [check_cpp_twin_table, check_cpp_command_line_flag, check_cpp_shipped_build_ignores_twins]

TWINS = [
    (check_missed_twin, "caught_always", tw_caught_always),
    (check_loader_error_is_not_caught, "caught_always", tw_caught_always),
    (check_caught_regex, "caught_always", tw_caught_always),
    (check_all_caught, "caught_never", tw_caught_never),
    (check_loader_error_is_not_caught, "loader_errors_counted", tw_loader_errors_counted),
    (check_real_run_failure, "real_run_skipped", tw_real_run_skipped),
    (check_env_isolation, "env_leaks", tw_env_leaks),
    (check_wrong_test, "wrong_test_not_found", tw_wrong_test_not_found),
    (check_build_crash, "build_crash_ignored", tw_build_crash_ignored),
    (check_build_crash, "no_restore_build", tw_no_restore),
    (check_scan, "scan_ignored", tw_scan_ignored),
    (check_scan, "scan_ignores_numbers", tw_scan_ignores_numbers),
    (check_missed_twin, "all_ok_always", tw_all_ok_always),
    (check_timeout_is_caught, "timeout_not_caught", tw_timeout_not_caught),
    (check_json_report_and_cli, "json_not_written", tw_json_not_written),
    (check_table_validation, "no_table_checks", tw_no_table_checks),
    (check_parallel_and_only, "only_ignored", tw_only_ignored),
    (check_python_module, "py_env_ignored", pyt("_current = _clean(os.environ.get(ENV_VAR, \"\"))", "_current = \"\"")),
    (check_python_module, "py_prefix_match", pyt("return bool(_current) and _current == str(name)", "return bool(_current) and str(name).startswith(_current)")),
    (check_python_module, "py_active_not_restored", pyt("    finally:\n        set(old)", "    finally:\n        pass")),
    (check_python_module, "py_zero_is_a_twin", pyt("return \"\" if value == \"0\" else value", "return value")),
    (check_python_module, "py_args_not_stripped", pyt("        out.append(a)\n        i += 1", "        out.append(a)\n        out.append('--mut')\n        i += 1")),
    (check_python_twin_table, "py_env_ignored", pyt("_current = _clean(os.environ.get(ENV_VAR, \"\"))", "_current = \"\"")),
]
CPP_TWINS = [
    (check_cpp_twin_table, "h_name_never_matches", hdr("inline bool is(const char* name) { return any() && slot() == name; }",
                                                        "inline bool is(const char* name) { return false; }")),
    (check_cpp_twin_table, "h_number_never_matches", hdr("inline bool is(int number) { return any() && slot() == std::to_string(number); }",
                                                          "inline bool is(int number) { return false; }")),
    (check_cpp_command_line_flag, "h_flag_ignored", hdr("set(argv[i + 1]);", "(void)argv;")),
    (check_cpp_command_line_flag, "h_prefix_match", hdr("return any() && slot() == name;", "return any() && std::strncmp(slot().c_str(), name, slot().size()) == 0;")),
    (check_cpp_shipped_build_ignores_twins, "h_shipped_build_switchable", hdr("#ifdef COOP_RUNTIME_MUT", "#if 1")),
]


# ---------------------------------------------------------------------------------------------- driver
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.environ.get("COOP_BUILD_DIR"))
    ap.add_argument("--no-cpp", action="store_true")
    a = ap.parse_args()
    OPTS["build_dir"], OPTS["no_cpp"] = a.build_dir, a.no_cpp
    real = list(REAL) + ([] if a.no_cpp else CPP_REAL)
    twins = list(TWINS) + ([] if a.no_cpp else CPP_TWINS)
    if a.no_cpp:
        print("NOTE: the C++ part (coop_mut.h) was skipped by --no-cpp")
    bad, n_real, n_twin = [], 0, 0
    try:
        for fn in real:
            n_real += 1
            t0 = time.time()
            try:
                fn(None)
                print("ok    ", fn.__name__, "(%.1fs)" % (time.time() - t0), flush=True)
            except (Exception, SystemExit):  # noqa: BLE001
                bad.append(fn.__name__)
                print("FAIL  ", fn.__name__, flush=True)
                traceback.print_exc()
            finally:
                undo()
        for fn, name, mut in twins:
            n_twin += 1
            try:
                fn(mut)
            except AssertionError:
                print("ok     twin %s[%s] caught" % (fn.__name__, name), flush=True)
            except (Exception, SystemExit):  # noqa: BLE001  (a crash is not a caught defect)
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] crashed instead of failing a check" % (fn.__name__, name), flush=True)
                traceback.print_exc()
            else:
                bad.append(f"{fn.__name__}[{name}]")
                print("FAIL   twin %s[%s] NOT caught" % (fn.__name__, name), flush=True)
            finally:
                undo()
    finally:
        undo()
        for t in TMPS:
            shutil.rmtree(t, ignore_errors=True)
    print(f"\n{n_real} checks, {n_twin} twins, {len(bad)} failed")
    if bad:
        print("FAILED:", ", ".join(bad))
        return 1
    print("ALL OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
