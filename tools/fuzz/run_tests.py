"""Tests for coopfuzz. Prints ALL OK only when everything passed.

    python -I tools/fuzz/run_tests.py [--rebuild]

What it does:
  1. builds the fuzzer and the C++ self tests with cmake (one compile job at a time)
  2. runs the C++ suite: oracle unit tests, clean sessions with 2 to 4 players, every mutation twin, shrinking, replay, reports
  3. checks the command line: a clean run passes, every bug class listed by --list-bugs is caught by the real CLI,
     a failing run shrinks and writes a replayable scenario and a JSON report, replay reproduces, bad input exits 2
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import run as R  # noqa: E402

fails = []
sessions = {"cli": 0}


def check(name, cond, extra=""):
    print(("ok   " if cond else "FAIL ") + name)
    if not cond:
        fails.append(name)
        if extra:
            print("     " + extra.strip().replace("\n", "\n     ")[-1500:])


def cli(exe, args, env=None):
    p = subprocess.run([exe] + args, capture_output=True, text=True, errors="replace", env=env)
    m = re.search(r"coopfuzz: (\d+) sessions", p.stdout)
    if m:
        sessions["cli"] += int(m.group(1))
    return p


def main():
    rebuild = "--rebuild" in sys.argv
    exe = R.ensure_built(rebuild)
    check("build (cmake, C++17)", exe is not None and os.path.exists(exe))
    tests_exe = R.exe_path("coopfuzz_tests")
    check("test executable exists", os.path.exists(tests_exe))
    if fails:
        print("FAILED: " + ", ".join(fails))
        return 1

    tmp = tempfile.mkdtemp(prefix="coopfuzz_")
    try:
        # ---- 2. C++ suite
        env = dict(os.environ)
        env["COOPFUZZ_TEST_OUT"] = os.path.join(tmp, "cpp")
        p = subprocess.run([tests_exe], capture_output=True, text=True, errors="replace", env=env)
        print(p.stdout.rstrip())
        m = re.search(r"== (\d+) tests, (\d+) passed, (\d+) failed, (\d+) checks, (\d+) sessions, (\d+) twins caught ==", p.stdout)
        check("C++ suite passes", p.returncode == 0 and m is not None and m.group(3) == "0", p.stdout[-1500:] + p.stderr)
        cpp_sessions = int(m.group(5)) if m else 0
        cpp_twins = int(m.group(6)) if m else 0

        # ---- 3. command line
        bugs = cli(exe, ["--list-bugs"]).stdout.split()
        check("--list-bugs lists the bug classes (at least 12)", len(bugs) >= 12, str(bugs))
        check("every bug class was caught by the C++ twin tests", m is not None and cpp_twins == len(bugs), "caught %d of %d" % (cpp_twins, len(bugs)))
        tg = cli(exe, ["--list-targets"])
        check("--list-targets names the example target", tg.returncode == 0 and tg.stdout.startswith("example"), tg.stdout)

        for players in (2, 3, 4):
            p = cli(exe, ["--count", "40", "--players", str(players), "--seed", "500", "--quiet", "--out", tmp])
            check("clean CLI run with %d players: exit 0 and ALL OK" % players, p.returncode == 0 and "RESULT: ALL OK" in p.stdout, p.stdout + p.stderr)

        caught_cli = 0
        for b in bugs:
            p = cli(exe, ["--count", "400", "--bug", b, "--max-fail", "1", "--no-shrink", "--quiet", "--out", os.path.join(tmp, "twin")])
            ok = p.returncode == 1 and "FAIL seed" in p.stdout and "RESULT: ALL OK" not in p.stdout
            caught_cli += ok
            check("CLI catches mutation twin %s" % b, ok, p.stdout + p.stderr)
        check("all %d mutation twins caught by the CLI" % len(bugs), caught_cli == len(bugs))

        # the skeleton target (the template users copy): clean run passes, its twin is caught
        p = cli(exe, ["--target", "skeleton", "--count", "60", "--quiet", "--out", tmp])
        check("skeleton target: clean CLI run passes", p.returncode == 0 and "RESULT: ALL OK" in p.stdout, p.stdout + p.stderr)
        p = cli(exe, ["--target", "skeleton", "--bug", "no_dedupe", "--count", "60", "--max-fail", "1", "--no-shrink", "--quiet", "--out", os.path.join(tmp, "sk")])
        check("skeleton target: twin no_dedupe is caught", p.returncode == 1 and "FAIL seed" in p.stdout, p.stdout + p.stderr)

        # a failing run shrinks, writes a replayable scenario and a JSON report
        out = os.path.join(tmp, "fail")
        p = cli(exe, ["--count", "40", "--bug", "two_owners", "--max-fail", "1", "--out", out])
        check("CLI fails with an injected bug and shrinks", p.returncode == 1 and "FAIL seed" in p.stdout and "shrunk" in p.stdout, p.stdout)
        mins = [f for f in os.listdir(out) if f.endswith(".min.scenario.json")] if os.path.isdir(out) else []
        reps = [f for f in os.listdir(out) if f.endswith(".min.report.json")] if os.path.isdir(out) else []
        check("minimized scenario and JSON report written", len(mins) == 1 and len(reps) == 1)
        if reps:
            try:
                rep = json.load(open(os.path.join(out, reps[0]), encoding="utf-8"))
                good = (rep.get("format") == "coopfuzz-report-1" and rep["violations"][0]["invariant"] == "multi_owner" and rep["mutations"] == ["two_owners"]
                        and isinstance(rep["seed"], int) and rep["ops"] < rep["shrunk_from"]["ops"] and "apply_log" in rep and "ownership_log" in rep)
            except Exception as e:  # noqa: BLE001
                good = False
                print("     " + repr(e))
            check("report is valid JSON with seed, violations, shrink info and logs", good)
        if mins:
            sc = os.path.join(out, mins[0])
            p = cli(exe, ["--replay", sc])
            check("replay of the minimized scenario reproduces (exit 1, VIOLATION line)", p.returncode == 1 and "VIOLATION multi_owner" in p.stdout, p.stdout)
            data = json.load(open(sc, encoding="utf-8"))
            data["mutations"] = []
            clean_sc = os.path.join(tmp, "clean.scenario.json")
            json.dump(data, open(clean_sc, "w", encoding="utf-8"))
            p = cli(exe, ["--replay", clean_sc])
            check("same scenario without the bug passes (the file is the bug, not the environment)", p.returncode == 0, p.stdout)
        p = cli(exe, ["--replay", os.path.join(tmp, "nope.json")])
        check("replay of a missing file exits 2", p.returncode == 2)
        p = cli(exe, ["--count", "1", "--bug", "not_a_bug", "--out", tmp])
        check("unknown bug name exits 2", p.returncode == 2, p.stdout + p.stderr)
        p = cli(exe, ["--count", "1", "--profile", "no_such_profile"])
        check("unknown link profile exits 2", p.returncode == 2)
        p = cli(exe, ["--count", "5", "--profile", "hell", "--quiet", "--out", tmp])
        check("link profile 'hell' accepted and clean", p.returncode == 0 and "RESULT: ALL OK" in p.stdout, p.stdout + p.stderr)

        # fixture: a saved shrunk repro that breaks with the bug and passes without it
        p = cli(exe, ["--fixture", "no_idempotency", "--out", os.path.join(tmp, "fx")])
        fx = os.path.join(tmp, "fx", "regress_no_idempotency.json")
        check("--fixture writes a shrunk repro", p.returncode == 0 and os.path.exists(fx), p.stdout + p.stderr)
        if os.path.exists(fx):
            p = cli(exe, ["--replay", fx])
            check("fixture breaks while the bug is injected", p.returncode == 1, p.stdout)
            data = json.load(open(fx, encoding="utf-8"))
            data["mutations"] = []
            json.dump(data, open(fx, "w", encoding="utf-8"))
            p = cli(exe, ["--replay", fx])
            check("fixture passes without the bug", p.returncode == 0, p.stdout)

        # the python wrapper
        p = subprocess.run([sys.executable, "-I", os.path.join(HERE, "run.py"), "--no-build", "--count", "5", "--players", "4", "--quiet", "--out", tmp],
                           capture_output=True, text=True, errors="replace")
        check("run.py wrapper (-I, --players 4)", p.returncode == 0 and "RESULT: ALL OK" in p.stdout, p.stdout + p.stderr)
        m2 = re.search(r"coopfuzz: (\d+) sessions", p.stdout)
        sessions["cli"] += int(m2.group(1)) if m2 else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("sessions run: %d in the C++ suite, %d through the CLI" % (cpp_sessions, sessions["cli"]))
    print("mutation twins caught: %d of %d" % (min(caught_cli, cpp_twins), len(bugs)))
    if fails:
        print("FAILED: " + ", ".join(fails))
        return 1
    print("ALL OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
