"""Code under test for the Python side: three functions with twins and three named tests.

    python py_under_test.py [test_name] [--mut NAME]

The twins are switched on with COOP_MUT=NAME in the environment. The last line says "<n> checks, <k> failed"; exit 1 on a failure.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import coop_mut  # noqa: E402


def add(a, b):
    if coop_mut.mut("py_add_wrong_operator"):
        return a - b
    return a + b


def first_word(text):
    if coop_mut.mut("py_first_word_whole_text"):
        return text
    return text.split()[0] if text.split() else ""


def clamp(x, lo, hi):
    if coop_mut.mut(7):
        return x
    return max(lo, min(hi, x))


CHECKS = {"n": 0, "bad": 0}


def check(cond, msg):
    CHECKS["n"] += 1
    if not cond:
        CHECKS["bad"] += 1
        print("[FAIL] " + msg)


def test_add():
    check(add(2, 3) == 5, "2+3")
    check(add(-1, 1) == 0, "-1+1")


def test_first_word():
    check(first_word("hello world") == "hello", "first word")
    check(first_word("") == "", "empty")


def test_clamp():
    check(clamp(50, 0, 10) == 10, "clamp high")
    check(clamp(-5, 0, 10) == 0, "clamp low")


TESTS = {"test_add": test_add, "test_first_word": test_first_word, "test_clamp": test_clamp}


def main():
    args = coop_mut.init_from_args(sys.argv[1:])
    only = args[0] if args else None
    if only and only not in TESTS:
        print("unknown test " + only)
        return 2
    for name, fn in TESTS.items():
        if only in (None, name):
            fn()
    print("%d checks, %d failed" % (CHECKS["n"], CHECKS["bad"]))
    return 1 if CHECKS["bad"] else 0


if __name__ == "__main__":
    sys.exit(main())
