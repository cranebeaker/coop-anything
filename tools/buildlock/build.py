"""Build a CMake project in a tree that several agents share: take the build lock, configure, build, release.

    python -I tools/buildlock/build.py --source . --build build/all [--arch x64] [--config Release] [--target T ...]
        [--cmake-arg ARG ...] [--sccache auto|on|off] [--sccache-stats] [--owner NAME] [--wait-s 3600] [--no-lock]
        [--clean] [--configure-only] [--keep-going] [--jobs N] [--generator auto|Ninja|...] [--msvc auto|on|off]
        [--vcvars FILE] [--keep-env] [--dry-run] [--label NAME]

What it does, in order:
  1. Cleans the environment: CMAKE_TOOLCHAIN_FILE, CMAKE_GENERATOR, CC, CXX, ANDROID_NDK* are removed (a stray Android toolchain
     file in the shell would cross-compile everything). --keep-env keeps them.
  2. On Windows with a Ninja or Makefile generator, loads the Visual Studio command line environment (vcvars) for --arch, found
     with vswhere or given with --vcvars / BUILDLOCK_VCVARS. --msvc auto (the default) does it only when `cl` is not already on PATH.
  3. Compiler cache: --sccache auto uses sccache as the compiler launcher when it is on PATH, on forces it (exit 2 when it is
     missing), off never. With it on, the configure step also sets what sccache needs for MSVC: no precompiled headers and
     embedded debug info (/Z7). See README.md. Nothing here installs sccache.
  4. If the tree is a Ninja tree and `ninja -n` says there is no work, returns at once without taking the lock.
  5. Takes the build lock (buildlock.py): waits, recovers a stale lock, keeps a heartbeat while it builds.
  6. Configures when the tree is new or the configure arguments changed since the last configure, then builds.
  7. Prints BUILD-TIME <label> <seconds>.

--cmake "CMD ..." (or BUILDLOCK_CMAKE) replaces the cmake program; the tests use a fake one. Exit codes: the build's exit code,
2 usage or setup problem, 3 the lock could not be taken in time.
"""
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from buildlock import BuildLock, LockTimeout, lock_path_for  # noqa: E402

DROP_ENV = ("CXX", "CC", "CMAKE_TOOLCHAIN_FILE", "ANDROID_NDK_HOME", "ANDROID_NDK", "CMAKE_ANDROID_NDK", "CMAKE_GENERATOR")
STAMP_FILE = ".buildlock-configure.json"


class SetupError(Exception):
    pass


# ------------------------------------------------------------------------------------------------ environment
def clean_env(base: Dict[str, str], keep_env: bool = False) -> Dict[str, str]:
    env = dict(base)
    if not keep_env:
        for k in DROP_ENV:
            env.pop(k, None)
    return env


def parse_set_output(text: str) -> Dict[str, str]:
    """Environment from the output of the `set` command."""
    out = {}
    for line in text.splitlines():
        if "=" in line and not line.startswith("="):
            k, v = line.split("=", 1)
            out[k] = v
    return out


def find_vcvars(arch: str, override: Optional[str] = None) -> Optional[str]:
    """Path of vcvars32.bat / vcvars64.bat, or None. override (or env BUILDLOCK_VCVARS) wins."""
    given = override or os.environ.get("BUILDLOCK_VCVARS")
    if given:
        return given if Path(given).is_file() else None
    pf = os.environ.get("ProgramFiles(x86)") or os.environ.get("ProgramFiles")
    if not pf:
        return None
    vswhere = Path(pf) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    p = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property", "installationPath"], capture_output=True, text=True, errors="replace")
    root = (p.stdout or "").strip().splitlines()
    if p.returncode != 0 or not root:
        return None
    bat = Path(root[0].strip()) / "VC" / "Auxiliary" / "Build" / ("vcvars32.bat" if arch == "x86" else "vcvars64.bat")
    return str(bat) if bat.is_file() else None


def vcvars_env(bat: str, base: Dict[str, str]) -> Dict[str, str]:
    """The environment after running a vcvars batch file."""
    p = subprocess.run(f'cmd /c ""{bat}" >nul && set"', shell=True, capture_output=True, text=True, env=base, errors="replace")
    if p.returncode != 0:
        raise SetupError("vcvars failed: " + (p.stderr or p.stdout)[-300:])
    env = parse_set_output(p.stdout)
    if not env:
        raise SetupError("vcvars printed no environment")
    return env


def has_program(name: str, env: Dict[str, str]) -> bool:
    return shutil.which(name, path=env.get("PATH") or env.get("Path")) is not None


# ------------------------------------------------------------------------------------------------ plan
def sccache_args(sccache_exe: str, msvc: bool) -> List[str]:
    """CMake arguments that make the compiler cache work. The launcher needs a Ninja or Makefile generator."""
    args = [f"-DCMAKE_C_COMPILER_LAUNCHER={sccache_exe}", f"-DCMAKE_CXX_COMPILER_LAUNCHER={sccache_exe}",
            "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON"]
    if msvc:
        # /Zi writes a shared PDB, which a compiler cache cannot reuse; /Z7 puts the debug info into each object file.
        args += ["-DCMAKE_POLICY_DEFAULT_CMP0141=NEW", "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"]
    return args


def choose_sccache(mode: str, env: Dict[str, str], generator: Optional[str], say=print) -> Optional[str]:
    """The sccache program to use as the compiler launcher, or None. Raises SetupError for mode "on" when it cannot be used."""
    if mode == "off":
        return None
    found = shutil.which("sccache", path=env.get("PATH") or env.get("Path"))
    if mode == "on" and not found:
        raise SetupError("--sccache on, but sccache is not on PATH (see README.md; this tool does not install it)")
    # A compiler launcher works with Ninja and Makefile generators only. The default generator on Windows is Visual Studio.
    usable = (generator is not None and "Visual Studio" not in generator and "Xcode" not in generator) or              (generator is None and os.name != "nt")
    if found and not usable:
        if mode == "on":
            raise SetupError("sccache can only be the compiler launcher with the Ninja or Makefile generators "
                             "(ninja is not on PATH, or --generator names another one)")
        say("[build] sccache found, but this generator cannot use it; building without it")
        return None
    return found


def configure_args(a, generator: Optional[str], sccache_exe: Optional[str], msvc: bool) -> List[str]:
    args: List[str] = []
    if generator:
        args += ["-G", generator]
    single = generator is None or "Visual Studio" not in generator
    if single:
        args.append(f"-DCMAKE_BUILD_TYPE={a.config}")
    if sccache_exe:
        args += sccache_args(sccache_exe, msvc)
    return args + list(a.cmake_arg)


def configure_stamp(args: List[str], source: str) -> str:
    return hashlib.sha256(json.dumps([source, args]).encode()).hexdigest()


def read_stamp(build: Path) -> Optional[str]:
    try:
        return json.loads((build / STAMP_FILE).read_text(encoding="utf-8")).get("stamp")
    except (OSError, ValueError):
        return None


def write_stamp(build: Path, stamp: str) -> None:
    try:
        (build / STAMP_FILE).write_text(json.dumps({"stamp": stamp}), encoding="utf-8")
    except OSError:
        pass


def cmake_command(a) -> List[str]:
    spec = a.cmake or os.environ.get("BUILDLOCK_CMAKE") or "cmake"
    cmd = shlex.split(spec, posix=True)
    if len(cmd) == 1 and shutil.which(cmd[0]) is None and not Path(cmd[0]).exists():
        raise SetupError(f"{cmd[0]} not found on PATH (use --cmake to name it)")
    return cmd


# ------------------------------------------------------------------------------------------------ main
def build_main(a, say=print) -> int:
    source, build = Path(a.source).resolve(), Path(a.build).resolve()
    try:
        cmake = cmake_command(a)
        env = clean_env(dict(os.environ), a.keep_env)
        env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(a.jobs)
        gen = a.generator
        loaded_vcvars = False
        wants_msvc = os.name == "nt" and a.msvc != "off" and (gen in ("auto", "Ninja") or "Makefiles" in gen)
        if wants_msvc and (a.msvc == "on" or not has_program("cl", env)):
            bat = find_vcvars(a.arch, a.vcvars)
            if bat:
                env = clean_env(vcvars_env(bat, env), a.keep_env)
                env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(a.jobs)
                loaded_vcvars = True
            elif a.msvc == "on":
                raise SetupError("--msvc on, but vcvars was not found (use --vcvars FILE or set BUILDLOCK_VCVARS)")
        generator = ("Ninja" if has_program("ninja", env) else None) if gen == "auto" else gen
        sccache_exe = choose_sccache(a.sccache, env, generator, say)
        msvc_cl = os.name == "nt" and (loaded_vcvars or has_program("cl", env))
        cfg = configure_args(a, generator, sccache_exe, msvc_cl)
    except SetupError as e:
        print(f"build: {e}", file=sys.stderr)
        return 2

    stamp = configure_stamp(cfg, str(source))
    ninja_tree = (build / "build.ninja").exists()
    configure_cmd = cmake + ["-S", str(source), "-B", str(build)] + cfg
    build_cmd = cmake + ["--build", str(build), "--config", a.config]
    if a.target:
        build_cmd += ["--target"] + a.target
    if a.keep_going:
        build_cmd += ["--", "-k", "0"]
    if a.dry_run:
        say("configure: " + " ".join(configure_cmd))
        say("build:     " + " ".join(build_cmd))
        say(f"sccache: {sccache_exe or 'off'}; generator: {generator or 'default'}")
        return 0
    if a.configure_only:
        a.no_lock = True

    if not a.no_lock and not a.clean and ninja_tree and read_stamp(build) == stamp:
        dry = cmake + ["--build", str(build), "--config", a.config] + (["--target"] + a.target if a.target else []) + ["--", "-n"]
        p = subprocess.run(dry, env=env, capture_output=True, text=True, errors="replace")
        if p.returncode == 0 and "no work to do" in (p.stdout + p.stderr):
            say("ninja: no work to do. (checked without taking the build lock)")
            say(f"BUILD-TIME {a.label} 0.0s (tree={build}, up to date)")
            return 0

    lock = None
    if not a.no_lock:
        lock = BuildLock(lock_path_for(str(build), a.lock), a.owner, a.label, a.lease_s, a.max_hold_s, a.poll_s,
                         log=(lambda s: None) if a.quiet else (lambda s: print("[build] " + s, file=sys.stderr, flush=True)))
        try:
            lock.acquire(a.wait_s)
        except LockTimeout as e:
            print(f"[build] {e}", file=sys.stderr)
            return 3
    t0 = time.time()
    rc = 1
    try:
        if a.clean and build.exists():
            shutil.rmtree(build, ignore_errors=True)
        if not (build / "CMakeCache.txt").exists() or read_stamp(build) != stamp:
            p = subprocess.run(configure_cmd, env=env)
            if p.returncode != 0:
                print("[build] CONFIGURE FAILED", file=sys.stderr)
                return p.returncode
            write_stamp(build, stamp)
        if a.configure_only:
            return 0
        p = subprocess.run(build_cmd, env=env)
        rc = p.returncode
        if rc != 0:
            print("[build] BUILD FAILED", file=sys.stderr)
        return rc
    finally:
        if lock is not None:
            lock.release()
        say(f"BUILD-TIME {a.label} {time.time() - t0:.1f}s (tree={build}, sccache={'yes' if sccache_exe else 'no'})")
        if a.sccache_stats and sccache_exe:
            subprocess.run([sccache_exe, "--show-stats"], env=env)


def make_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", default=".", help="CMake source folder")
    ap.add_argument("--build", required=True, help="build tree folder; the lock is <build>.buildlock")
    ap.add_argument("--arch", default="x64", choices=("x86", "x64"), help="Visual Studio command line architecture")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--target", nargs="*", default=[])
    ap.add_argument("--cmake-arg", action="append", default=[], help="an extra argument for the configure step; repeatable")
    ap.add_argument("--generator", default="auto", help="auto (Ninja when ninja is on PATH, else cmake's default), or a generator name")
    ap.add_argument("--sccache", default="auto", choices=("auto", "on", "off"))
    ap.add_argument("--sccache-stats", action="store_true", help="print sccache --show-stats after the build")
    ap.add_argument("--msvc", default="auto", choices=("auto", "on", "off"))
    ap.add_argument("--vcvars", default=None, help="path of vcvars32.bat / vcvars64.bat")
    ap.add_argument("--keep-env", action="store_true", help="do not remove CMAKE_TOOLCHAIN_FILE and friends from the environment")
    ap.add_argument("--jobs", type=int, default=None, help="parallel build jobs (default: CMAKE_BUILD_PARALLEL_LEVEL if set, else 2)")
    ap.add_argument("--clean", action="store_true", help="delete the build tree first")
    ap.add_argument("--configure-only", action="store_true")
    ap.add_argument("--keep-going", "-k", action="store_true", help="Ninja: report every compile error, not just the first")
    ap.add_argument("--cmake", default=None, help="the cmake program (default: cmake on PATH, or BUILDLOCK_CMAKE)")
    ap.add_argument("--dry-run", action="store_true", help="print the commands, run nothing")
    ap.add_argument("--no-lock", action="store_true")
    ap.add_argument("--lock", default=None)
    ap.add_argument("--owner", default=None)
    ap.add_argument("--label", default="build")
    ap.add_argument("--wait-s", type=float, default=3600.0)
    ap.add_argument("--lease-s", type=float, default=120.0)
    ap.add_argument("--max-hold-s", type=float, default=7200.0)
    ap.add_argument("--poll-s", type=float, default=1.0)
    ap.add_argument("--quiet", action="store_true")
    return ap


def main(argv=None) -> int:
    a = make_parser().parse_args(argv)
    if a.jobs is None:
        a.jobs = int(os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or 2)
    return build_main(a)


if __name__ == "__main__":
    sys.exit(main())
