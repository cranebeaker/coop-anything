"""coopfuzz driver. Builds the fuzzer with cmake when needed, then runs it.

Usage:  python -I tools/fuzz/run.py --minutes 2 --players 4 [--profile bad_wifi] [--bug NAME] [--seed N]

Options are passed to the coopfuzz executable: --minutes, --count, --seed, --players (2 to 4), --duration, --profile,
--target, --bug NAME (inject a bug class, the run is then expected to fail), --replay FILE, --fixture BUG, --out DIR,
--no-shrink, --max-fail N, --list-bugs, --list-targets, --quiet. Extra driver options: --no-build, --rebuild.

Exit code 0 means every invariant held (and the line "RESULT: ALL OK" is printed). 1 means an invariant broke: the failing
seed, a shrunk scenario and a JSON report are written under --out (default tools/fuzz/out). 2 means a usage or build error.

Environment: COOPFUZZ_BUILD_DIR changes the cmake build folder (default tools/fuzz/build),
COOPFUZZ_CMAKE_ARGS adds cmake configure arguments (for example -DCOOPFUZZ_TARGET_SOURCES=my_target.cpp).
"""
import os
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.environ.get("COOPFUZZ_BUILD_DIR") or os.path.join(HERE, "build")
DEFAULT_OUT = os.path.join(HERE, "out")
PARALLEL = "1"   # one compile job at a time: friendly to small machines


def clean_env():
    """Environment for cmake: drop variables that point it at some other toolchain (for example an Android one)."""
    env = dict(os.environ)
    for k in ("CMAKE_TOOLCHAIN_FILE", "CC", "CXX", "ANDROID_NDK", "ANDROID_NDK_HOME", "CMAKE_GENERATOR", "CMAKE_BUILD_TYPE"):
        env.pop(k, None)
    return env


def exe_path(name="coopfuzz"):
    suffix = ".exe" if os.name == "nt" else ""
    for sub in ("Release", "RelWithDebInfo", "MinSizeRel", "Debug", ""):
        p = os.path.join(BUILD, sub, name + suffix)
        if os.path.exists(p):
            return p
    return os.path.join(BUILD, "Release" if os.name == "nt" else "", name + suffix)


def newest_source():
    t = 0.0
    for base, _, files in os.walk(HERE):
        if os.path.abspath(base).startswith(os.path.abspath(BUILD)) or os.path.basename(base) in ("out", "__pycache__"):
            continue
        for f in files:
            if f.endswith((".cpp", ".h", ".txt")):
                t = max(t, os.path.getmtime(os.path.join(base, f)))
    return t


def build(force=False):
    """Configure and build. Returns True on success."""
    env = clean_env()
    extra = shlex.split(os.environ.get("COOPFUZZ_CMAKE_ARGS", ""))
    r = subprocess.run(["cmake", "-S", HERE, "-B", BUILD] + extra, env=env)
    if r.returncode != 0:
        return False
    r = subprocess.run(["cmake", "--build", BUILD, "--config", "Release", "--parallel", PARALLEL], env=env)
    return r.returncode == 0


def ensure_built(rebuild=False):
    exe = exe_path()
    stale = not os.path.exists(exe) or os.path.getmtime(exe) < newest_source()
    if rebuild or stale:
        if not build():
            print("coopfuzz build failed (needs cmake and a C++17 compiler)")
            return None
    return exe_path()


def main(argv):
    rebuild = "--rebuild" in argv
    no_build = "--no-build" in argv
    args = [a for a in argv if a not in ("--rebuild", "--no-build")]
    if args and args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    exe = exe_path() if no_build else ensure_built(rebuild)
    if exe is None or not os.path.exists(exe):
        print("coopfuzz executable not found; run without --no-build")
        return 2
    if "--out" not in args:
        args += ["--out", DEFAULT_OUT]
    return subprocess.run([exe] + args).returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
