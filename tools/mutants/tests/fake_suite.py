"""A fake test program for the twin runner tests. Its behaviour comes from the environment, so one script plays every case.

    python fake_suite.py TEST

  FAKE_SPEC   JSON: {"<twin>": {"<test>": <exit code or "hang">}, "_real_fail": ["<test>", ...]}
              With twin <twin> on, test <test> exits with that code (and prints "1 checks, 1 failed" for code 1, "crashed" otherwise).
              A test in _real_fail fails even without a twin.
  FAKE_VAR    name of the environment variable that carries the twin (default COOP_MUT)
  FAKE_LOG    a file: every run appends "<test> <twin value>"
"""
import json
import os
import sys
import time

spec = json.loads(os.environ.get("FAKE_SPEC", "{}"))
var = os.environ.get("FAKE_VAR", "COOP_MUT")
test = sys.argv[1] if len(sys.argv) > 1 else "*"
twin = os.environ.get(var, "")
log = os.environ.get("FAKE_LOG")
if log:
    with open(log, "a", encoding="utf-8") as f:
        f.write(f"{test} {twin}\n")
if test in spec.get("_real_fail", []):
    print("1 checks, 1 failed")
    sys.exit(1)
effect = spec.get(twin, {}).get(test) if twin else None
if effect is None:
    print("1 checks, 0 failed")
    sys.exit(0)
if effect == "hang":
    time.sleep(60)
    sys.exit(0)
print("1 checks, 1 failed" if effect == 1 else "crashed")
code = int(effect)
sys.exit(code - (1 << 32) if code >= (1 << 31) else code)  # an exit status such as 0xC0000135 does not fit a C int
