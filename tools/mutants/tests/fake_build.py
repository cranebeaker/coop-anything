"""A fake build step for the twin runner tests.

    python fake_build.py

  BUILD_LOG       a file: every run appends the twin value it saw ("-" for none)
  BUILD_CRASH_ON  exit 2 when COOP_MUT has this value (a twin that does not compile)
"""
import os
import sys

twin = os.environ.get("COOP_MUT", "")
with open(os.environ["BUILD_LOG"], "a", encoding="utf-8") as f:
    f.write((twin or "-") + "\n")
if twin and twin == os.environ.get("BUILD_CRASH_ON"):
    print("error C2065: undeclared identifier")
    sys.exit(2)
sys.exit(0)
