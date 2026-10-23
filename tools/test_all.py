"""Run every sync test tool's suite (tests plus mutation twins) and print ALL OK when all pass.

    python -I tools/test_all.py            run every suite
    python -I tools/test_all.py chaos bots run only these

A suite counts as passed only if it exits 0 AND its last output line is ALL OK.
The C++ suites build with CMake and need a C++17 compiler (Visual Studio 2022 on Windows).
"""
import os
import subprocess
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
SUITES = ["chaos", "flight", "ownership", "fakeworld", "fuzz",
          "gameslot", "gamepool", "scenarios", "bots", "animcheck",
          "bridge", "inspect", "hotreload", "mutants", "buildlock"]


def run(name):
    script = TOOLS / name / "run_tests.py"
    if not script.exists():
        return False, "missing run_tests.py"
    env = dict(os.environ, PYTHONIOENCODING="utf-8")
    t = time.time()
    p = subprocess.run([sys.executable, "-I", str(script)], cwd=TOOLS.parent, env=env,
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    lines = [l.strip() for l in (p.stdout or "").splitlines() if l.strip()]
    ok = p.returncode == 0 and bool(lines) and lines[-1] == "ALL OK"
    note = f"{time.time() - t:.0f}s"
    if not ok:
        tail = "\n".join(((p.stdout or "") + (p.stderr or "")).splitlines()[-15:])
        note += f", exit {p.returncode}\n{tail}"
    return ok, note


def main():
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
