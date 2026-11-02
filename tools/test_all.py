"""Run every sync test tool's suite (tests plus mutation twins) and print ALL OK when all pass.

    python -I tools/test_all.py            run every suite
    python -I tools/test_all.py chaos bots run only these

A suite counts as passed only if it exits 0 AND its last output line is ALL OK, which a
suite may follow with a parenthesised note when it skipped a stage, for example
"ALL OK (C++ cross-check skipped: cmake not found on PATH)".
The C++ suites build with CMake and need a C++17 compiler (Visual Studio 2022 on Windows).
"""
import re
import subprocess
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
SUITES = ["chaos", "flight", "ownership", "fakeworld", "fuzz",
          "gameslot", "gamepool", "scenarios", "bots", "animcheck",
          "bridge", "inspect", "hotreload", "mutants", "buildlock"]

# The last line of a suite that passed: ALL OK, or ALL OK followed by a note in brackets
# saying which stage it had to skip. A suite that failed prints something else and exits
# non-zero, and the exit code is checked as well.
PASS_LINE = re.compile(r"ALL OK(?: \(.*\))?")


def run(name):
    script = TOOLS / name / "run_tests.py"
    if not script.exists():
        return False, "missing run_tests.py"
    t = time.time()
    # -X utf8=1, not PYTHONIOENCODING: -I ignores the environment, so the variable this
    # used to set never reached the suite and its output came back in the console's code
    # page instead (on Windows an OS error message in it then fails to decode as UTF-8).
    p = subprocess.run([sys.executable, "-I", "-X", "utf8=1", str(script)], cwd=TOOLS.parent,
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = [l.strip() for l in (p.stdout or "").splitlines() if l.strip()]
    ok = p.returncode == 0 and bool(lines) and PASS_LINE.fullmatch(lines[-1]) is not None
    note = f"{time.time() - t:.0f}s"
    if not ok:
        tail = "\n".join(((p.stdout or "") + (p.stderr or "")).splitlines()[-15:])
        note += f", exit {p.returncode}\n{tail}"
    return ok, note


def main():
    # A suite can still hand us a character this console cannot encode: errors="replace"
    # above turns undecodable bytes from a tool the suite ran itself (cmake, a compiler)
    # into U+FFFD. Print it rather than die on it halfway through the run.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, OSError, ValueError):
            pass
    names = sys.argv[1:] or SUITES
    failed = []
    for name in names:
        ok, note = run(name)
        print(f"{'PASS' if ok else 'FAIL'}  {name}  ({note})", flush=True)
        if not ok:
            failed.append(name)
    if failed:
        print("FAILED: " + ", ".join(failed))
        sys.exit(1)
    print("ALL OK")


if __name__ == "__main__":
    main()
