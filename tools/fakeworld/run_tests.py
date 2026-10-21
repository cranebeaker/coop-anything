"""Build the fake world and its tests with CMake, run them, and check every mutation twin.

    python -I tools/fakeworld/run_tests.py [--build-dir DIR]

Steps:
  1. Configure and build with CMake (one job at a time, RAM friendly). Needs cmake and a C++17 compiler
     (Visual Studio 2022 on Windows, gcc or clang elsewhere).
  2. Run the test program. Every test must pass.
  3. Check that every mutation twin in TWINS ran and passed. A twin passes only when its scenario passes
     without the fault AND reports a violation with the fault switched on.
Prints ALL OK as the last line only when all of that holds. Exit code 0 on success, 1 otherwise.
The build folder defaults to a folder under the system temp directory (override with --build-dir or the
COOP_BUILD_DIR environment variable), so nothing is written into the repository.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXE = "coop_fakeworld_tests"
TWINS = [
    # ownership library faults, seen through the whole cluster
    "own_transfer_no_ack", "own_no_hysteresis", "own_follower_ignored",
    # fake world faults
    "no_dedupe", "hit_no_validation", "hit_by_non_owner", "pickup_no_arbitration", "pickup_no_rollback",
    "spawn_no_arbitration", "loot_per_machine", "pick_per_machine", "container_no_lock", "named_killable",
    "no_session_epoch", "heal_disabled", "drop_no_idempotency", "join_no_ownership", "owner_keeps_publishing",
]
MIN_TESTS = 40   # guards against a build that silently registers almost nothing


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def clean_env():
    """A toolchain file left in the environment (for example an Android one) would cross-compile the tests."""
    env = dict(os.environ)
    env.pop("CMAKE_TOOLCHAIN_FILE", None)
    return env


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", env=clean_env(), **kw)


def tail(p, n=25):
    return "\n".join(((p.stdout or "") + (p.stderr or "")).splitlines()[-n:])


def find_exe(build):
    for name in (EXE + ".exe", EXE):
        p = build / "bin" / name
        if p.is_file():
            return p
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.environ.get("COOP_BUILD_DIR"))
    args = ap.parse_args()
    cmake = shutil.which("cmake")
    if not cmake:
        fail("cmake not found on PATH")
    if args.build_dir:
        build = Path(args.build_dir).resolve()
    else:
        build = Path(tempfile.gettempdir()) / ("coop-fakeworld-build-" + hashlib.sha1(str(HERE).encode()).hexdigest()[:10])
    cfg = run([cmake, "-S", str(HERE), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"])
    if cfg.returncode != 0:
        fail("cmake could not configure (is a C++17 compiler installed?)\n" + tail(cfg))
    b = run([cmake, "--build", str(build), "--config", "Release", "--parallel", "1"])
    if b.returncode != 0:
        fail("build failed\n" + tail(b))
    exe = find_exe(build)
    if not exe:
        fail("test program not found under " + str(build / "bin"))
    r = run([str(exe)])
    out = r.stdout or ""
    results = re.findall(r"^\[(PASS|FAIL)\] (\S+) \((\d+) checks\)", out, re.M)
    passed = [n for s, n, _ in results if s == "PASS"]
    failed = [n for s, n, _ in results if s == "FAIL"]
    m = re.search(r"^== (\d+) tests, (\d+) passed, (\d+) failed, (\d+) checks ==", out, re.M)
    if r.returncode != 0 or failed or not m or int(m.group(3)) != 0:
        print(out[-4000:])
        fail("tests failed: " + (", ".join(failed) or "see output above"))
    total = int(m.group(1))
    if total < MIN_TESTS or total != len(results):
        fail("unexpected test count %d (parsed %d, want at least %d)" % (total, len(results), MIN_TESTS))
    missing = [t for t in TWINS if "twin." + t not in passed]
    if missing:
        fail("mutation twins not caught or not run: " + ", ".join(missing))
    extra = [n for n in passed if n.startswith("twin.") and n[5:] not in TWINS]
    if extra:
        fail("twins missing from TWINS in run_tests.py: " + ", ".join(extra))
    print("tests: %d passed, 0 failed, %s checks" % (total, m.group(4)))
    print("mutation twins caught: %d of %d" % (len(TWINS), len(TWINS)))
    print("ALL OK")


if __name__ == "__main__":
    main()
