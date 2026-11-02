"""Checks of coop_mut.py itself. Run as a subprocess by run_tests.py, once against the real module and once per broken copy.

    python -I py_mut_checks.py MODULE_DIR

MODULE_DIR holds the coop_mut.py to test. Prints one line per failed check and exits 1 if any failed.
"""
import importlib
import os
import subprocess
import sys

moddir = os.path.abspath(sys.argv[1])
fails = []


def check(cond, name):
    if not cond:
        fails.append(name)
        print("FAIL " + name)


def fresh(env_value):
    """Import coop_mut anew with COOP_MUT set to env_value (None removes it)."""
    if env_value is None:
        os.environ.pop("COOP_MUT", None)
    else:
        os.environ["COOP_MUT"] = env_value
    sys.modules.pop("coop_mut", None)
    sys.path.insert(0, moddir)
    try:
        return importlib.import_module("coop_mut")
    finally:
        sys.path.remove(moddir)


m = fresh(None)
check(not m.mut("a") and not m.mut(1) and not m.any_active() and not m.ENABLED, "off by default")

m = fresh("")
check(not m.mut("") and not m.any_active(), "an empty value is off")

m = fresh("0")
check(not m.any_active() and not m.mut(0) and not m.mut("0"), "0 is off")

m = fresh("alpha")
check(m.mut("alpha") and not m.mut("beta") and m.any_active() and m.ENABLED and m.current() == "alpha", "a name from the environment")
check(not m.mut("alph") and not m.mut("alphabet"), "only an exact match counts")

m = fresh("7")
check(m.mut(7) and m.mut("7") and not m.mut(8), "a number from the environment")

m = fresh(None)
m.set("beta")
check(m.mut("beta") and m.ENABLED, "set() turns a twin on")
m.set(0)
check(not m.any_active() and not m.ENABLED, "set(0) turns it off")
with m.active("gamma"):
    check(m.mut("gamma"), "active() turns a twin on inside the block")
check(not m.mut("gamma") and not m.any_active(), "active() restores the old state")
m.set("outer")
with m.active("inner"):
    check(m.mut("inner") and not m.mut("outer"), "one twin at a time")
check(m.mut("outer"), "active() restores the outer twin")
try:
    with m.active("boom"):
        raise RuntimeError("x")
except RuntimeError:
    pass
check(m.mut("outer"), "active() restores after an exception")

m = fresh(None)
rest = m.init_from_args(["a", "--mut", "delta", "b"])
check(rest == ["a", "b"] and m.mut("delta"), "--mut NAME is read and removed")
m = fresh(None)
rest = m.init_from_args(["--mut=eps", "c"])
check(rest == ["c"] and m.mut("eps"), "--mut=NAME is read and removed")
m = fresh(None)
check(m.init_from_args(["x", "y"]) == ["x", "y"] and not m.any_active(), "no flag, no change")

# a different process sees its own environment: the switch is read at import, not shared
code = "import sys; sys.path.insert(0, sys.argv[1]); import coop_mut; print(coop_mut.mut('zeta'))"
env = dict(os.environ, COOP_MUT="zeta")
out = subprocess.run([sys.executable, "-I", "-c", code, moddir], capture_output=True, text=True, env=env).stdout.strip()
check(out == "True", "a child process reads COOP_MUT")
env.pop("COOP_MUT")
out = subprocess.run([sys.executable, "-I", "-c", code, moddir], capture_output=True, text=True, env=env).stdout.strip()
check(out == "False", "a child process without COOP_MUT has no twin")

print("%d failed" % len(fails))
sys.exit(1 if fails else 0)
