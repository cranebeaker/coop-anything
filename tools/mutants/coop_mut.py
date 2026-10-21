"""Runtime-switchable mutation twins for Python (tools/mutants/README.md). The same idea as coop_mut.h.

A "twin" is a deliberately broken variant of one piece of your code. The test that guards that code must fail when the twin is
on. Pick the twin when the test program starts, with the environment variable COOP_MUT (a name or a number):

    set COOP_MUT=add_wrong_operator && python my_tests.py

In the code under test:

    import coop_mut

    def add(a, b):
        if coop_mut.mut("add_wrong_operator"):
            return a - b                       # the broken variant
        return a + b

Cost when off: the environment is read once at import. `mut()` then does one global load and one comparison. Hot loops can test
`coop_mut.ENABLED` (False when no twin is chosen) first. Ship your product without the call sites, or keep them: with no
COOP_MUT in the environment every call returns False.

One twin is active at a time. "0" or an empty value means no twin. set() and active() are for in-process tests.
"""
import contextlib
import os

ENV_VAR = "COOP_MUT"


def _clean(value):
    value = "" if value is None else str(value)
    return "" if value == "0" else value


_current = _clean(os.environ.get(ENV_VAR, ""))
ENABLED = bool(_current)


def mut(name):
    """True when twin `name` (a string or an int) is the active one."""
    return bool(_current) and _current == str(name)


def any_active():
    return bool(_current)


def current():
    return _current


def set(name):  # noqa: A001  (the name mirrors the C++ header)
    """Choose a twin from code ("" or 0 turns it off). Not thread safe: call it during test setup."""
    global _current, ENABLED
    _current = _clean(name)
    ENABLED = bool(_current)


@contextlib.contextmanager
def active(name):
    """`with coop_mut.active("add_wrong_operator"): ...` turns one twin on for the block."""
    old = _current
    set(name)
    try:
        yield
    finally:
        set(old)


def init_from_args(argv):
    """Accept `--mut NAME` or `--mut=NAME` in an argument list. Returns the list without those arguments."""
    out, i = [], 0
    while i < len(argv):
        a = argv[i]
        if a == "--mut" and i + 1 < len(argv):
            set(argv[i + 1])
            i += 2
            continue
        if a.startswith("--mut="):
            set(a[len("--mut="):])
            i += 1
            continue
        out.append(a)
        i += 1
    return out
