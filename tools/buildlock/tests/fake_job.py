"""A fake build job for the lock tests: records when it started and ended, so a test can prove two jobs never overlapped.

    python fake_job.py NAME LOG [SECONDS] [EXIT_CODE]

Appends "start NAME <time>" and "end NAME <time>" to LOG (time = time.time()). With the environment variable FAKE_PIDFILE it first
writes its own pid there. SECONDS is how long it works (default 0.3).
"""
import os
import sys
import time

name, log = sys.argv[1], sys.argv[2]
seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 0.3
code = int(sys.argv[4]) if len(sys.argv) > 4 else 0
pf = os.environ.get("FAKE_PIDFILE")
if pf:
    with open(pf, "w") as f:
        f.write(str(os.getpid()))
with open(log, "a") as f:
    f.write(f"start {name} {time.time():.4f}\n")
time.sleep(seconds)
with open(log, "a") as f:
    f.write(f"end {name} {time.time():.4f}\n")
sys.exit(code)
