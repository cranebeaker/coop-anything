"""A fake cmake for the build driver tests. It never compiles anything; it records what it was asked and behaves as told.

    python fake_cmake.py -S SRC -B TREE [configure args]      configure: creates TREE/CMakeCache.txt (and build.ninja with -G Ninja)
    python fake_cmake.py --build TREE [--config C] [--target T ...] [-- -n]    build, or a dry run with -- -n

Environment:
  FAKE_CMAKE_LOG     file: one JSON line per call (argv, a few environment values, whether FAKE_LOCK_DIR exists right now)
  FAKE_LOCK_DIR      the build lock folder, to prove the lock is held while this runs
  FAKE_CONFIGURE_RC  exit code of a configure (default 0)
  FAKE_BUILD_RC      exit code of a real build (default 0)
  FAKE_BUILD_SLEEP   seconds a real build takes (default 0)
A dry run (-- -n) prints "ninja: no work to do." when TREE/uptodate exists.
"""
import json
import os
import sys
import time
from pathlib import Path

argv = sys.argv[1:]
log = os.environ.get("FAKE_CMAKE_LOG")
lock = os.environ.get("FAKE_LOCK_DIR")
if log:
    rec = {"argv": argv, "lock_exists": bool(lock and Path(lock).exists()),
           "env": {k: os.environ.get(k) for k in ("CMAKE_TOOLCHAIN_FILE", "FAKEVAR", "CMAKE_BUILD_PARALLEL_LEVEL", "CC")}}
    with open(log, "a", encoding="utf-8") as f:
        f.write(json.dumps(rec) + "\n")
if "-S" in argv:
    tree = Path(argv[argv.index("-B") + 1])
    rc = int(os.environ.get("FAKE_CONFIGURE_RC", "0"))
    if rc == 0:
        tree.mkdir(parents=True, exist_ok=True)
        (tree / "CMakeCache.txt").write_text("fake")
        if "-G" in argv and argv[argv.index("-G") + 1] == "Ninja":
            (tree / "build.ninja").write_text("fake")
    sys.exit(rc)
if "--build" in argv:
    tree = Path(argv[argv.index("--build") + 1])
    if argv[-2:] == ["--", "-n"]:
        print("ninja: no work to do." if (tree / "uptodate").exists() else "ninja: work to do")
        sys.exit(0)
    time.sleep(float(os.environ.get("FAKE_BUILD_SLEEP", "0")))
    sys.exit(int(os.environ.get("FAKE_BUILD_RC", "0")))
sys.exit(0)
