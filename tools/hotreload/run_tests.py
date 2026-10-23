"""Build the hot reload tool with CMake, run its tests, and check every mutation twin.

    python -I tools/hotreload/run_tests.py [--build-dir DIR] [--arch x64|Win32]

Steps:
  1. Configure and build with CMake (one job at a time, RAM friendly): the loader DLL, the reload libraries, six example
     cores, the test host (a stand-in for the game) and the unit tests. Needs cmake and Visual Studio 2022 (Windows).
  2. Run the unit tests (state registry, threads, drain, JSON helpers, control pipe).
  3. Run the swap scenarios: each starts the test host, runs hotreload.py against it and checks the outcome.
  4. Check every mutation twin: a deliberately broken variant of one safety rule. The scenario or unit test that guards it
     must pass without the fault (steps 2 and 3) AND fail with it (this step).
Prints ALL OK as the last line only when all of that holds. Exit code 0 on success, 1 otherwise.
The build folder defaults to a folder under the system temp directory (override with --build-dir or the COOP_BUILD_DIR
environment variable), so nothing is written into the repository. Test hosts are started by this script and stopped by
their pid. Nothing else is touched, and no game is started.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "tests"))
import scenarios  # noqa: E402

UNIT_EXE = "hotreload_unit_tests.exe"
HOST_EXE = "hotreload_test_host.exe"
# Unit twins: number -> name (the test that guards each is found by the unit test program itself).
UNIT_TWINS = [(14, "crc_unchecked"), (15, "forward_shutdown"), (16, "thread_not_joined"), (17, "drain_ip_only"),
              (18, "drain_timeout_ok"), (19, "preflight_ignored")]
MIN_UNIT_TESTS = 30


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


def scrub(text, limit):
    """One line, no absolute paths (they would show the user name of whoever runs this), cut to `limit` characters."""
    text = re.sub(r"[A-Za-z]:(?:\\\\|\\|/)[^'\"\s]*", "<path>", str(text).replace("\n", " "))
    return text[:limit]


def tail(p, n=25):
    return "\n".join(((p.stdout or "") + (p.stderr or "")).splitlines()[-n:])


def main():
    if os.name != "nt":
        fail("the hot reload tool swaps DLLs in a running Windows process; its tests run on Windows only")
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.environ.get("COOP_BUILD_DIR"))
    ap.add_argument("--arch", default="", help="x64 or Win32 (default: the generator's default)")
    args = ap.parse_args()
    cmake = shutil.which("cmake")
    if not cmake:
        fail("cmake not found on PATH")
    if args.build_dir:
        build = Path(args.build_dir).resolve()
    else:
        build = Path(tempfile.gettempdir()) / ("coop-hotreload-build-" + hashlib.sha1(str(HERE).encode()).hexdigest()[:10])

    # ---- 1. build ----
    cfg_cmd = [cmake, "-S", str(HERE), "-B", str(build), "-DHOTRELOAD_MUTATION_TWINS=ON"]
    if args.arch:
        cfg_cmd += ["-A", args.arch]
    cfg = run(cfg_cmd)
    if cfg.returncode != 0:
        fail("cmake could not configure (is Visual Studio 2022 installed?)\n" + tail(cfg))
    b = run([cmake, "--build", str(build), "--config", "Release", "--parallel", "1"])
    if b.returncode != 0:
        fail("build failed\n" + tail(b))
    bin_dir = build / "bin"
    for need in (UNIT_EXE, HOST_EXE, "hotreload_loader.dll", "core_v1.dll", "core_v2.dll", "core_v3_nomigrate.dll",
                 "core_unregistered.dll", "core_failsave.dll", "core_badinit.dll"):
        if not (bin_dir / need).is_file():
            fail(f"{need} was not built (looked in {bin_dir})")

    # ---- 2. unit tests ----
    r = run([str(bin_dir / UNIT_EXE)])
    out = r.stdout or ""
    results = re.findall(r"^\[(PASS|FAIL)\] (\S+) \((\d+) checks\)", out, re.M)
    failed = [n for s, n, _ in results if s == "FAIL"]
    m = re.search(r"^== (\d+) tests, (\d+) passed, (\d+) failed, (\d+) checks ==", out, re.M)
    if r.returncode != 0 or failed or not m or int(m.group(3)) != 0:
        print(out[-4000:])
        fail("unit tests failed: " + (", ".join(failed) or "see output above"))
    unit_total = int(m.group(1))
    if unit_total < MIN_UNIT_TESTS or unit_total != len(results):
        fail("unexpected unit test count %d (parsed %d, want at least %d)" % (unit_total, len(results), MIN_UNIT_TESTS))
    print("unit tests: %d passed, 0 failed, %s checks" % (unit_total, m.group(4)), flush=True)

    # ---- 3. swap scenarios (with the test host) ----
    work = build / "work"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    passed = 0
    for name, fn in scenarios.SCENARIOS:
        t = time.time()
        try:
            fn(scenarios.Ctx(bin_dir, work))
        except Exception as e:
            print("[FAIL] %s: %s: %s" % (name, type(e).__name__, scrub(e, 1500)))
            fail("scenario %s failed" % name)
        passed += 1
        print("[PASS] %s (%.1f s)" % (name, time.time() - t), flush=True)
    print("swap scenarios: %d passed, 0 failed" % passed, flush=True)

    # ---- 4. mutation twins ----
    caught = 0
    total = len(UNIT_TWINS) + len(scenarios.TWINS)
    for num, name in UNIT_TWINS:
        p = run([str(bin_dir / UNIT_EXE), "--twin", str(num)])
        line = ((p.stdout or "").strip().splitlines() or [""])[-1]
        if p.returncode != 0:
            fail("mutation twin %s (%d) was not caught by the unit tests: %s" % (name, num, line))
        caught += 1
        print("[CAUGHT] %s: %s" % (name, line.replace("[CAUGHT] ", "")), flush=True)
    for name, mut, fault, scenario in scenarios.TWINS:
        fn = dict(scenarios.SCENARIOS)[scenario]
        env = {"HOTRELOAD_MUT": mut} if mut else {}
        t = time.time()
        try:
            fn(scenarios.Ctx(bin_dir, work, env=env, faults=(fault,) if fault else ()))
        except scenarios.SetupError as e:
            fail("twin %s: the test rig broke, so nothing was proven: %s" % (name, scrub(e, 500)))
        except Exception as e:
            caught += 1
            why = scrub(e, 400).split(": {")[0][:100]   # the sentence before a dumped report
            print("[CAUGHT] %s by %s (%.1f s): %s" % (name, scenario, time.time() - t, why), flush=True)
        else:
            fail("mutation twin %s was NOT caught: scenario %s still passes with it switched on" % (name, scenario))
    if caught != total:
        fail("only %d of %d twins caught" % (caught, total))

    print("mutation twins caught: %d of %d" % (caught, total))
    print("ALL OK")


if __name__ == "__main__":
    main()
