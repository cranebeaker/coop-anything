"""Run every generated regression fixture in a directory through the replay hook.

Usage: python -I tools/flight/regress.py DIR [--replay-cmd "build/replay_tool {fixture}" | --replay-py module:function]
Prints one line per fixture and ALL OK when every fixture converges (exit 0). A fixture that diverges is a
reproduced bug. A fixture whose "expect" says converges=false is expected to diverge (a known open bug).
Fixtures of format "cofr-trace-1" (written by from_failure.py) replay in process, so no replay hook is needed for them.
"""
import argparse
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shrink as S  # noqa: E402
import trace_replay as TR  # noqa: E402


def _replay(fx, oracle):
    if fx.get("format") == TR.FORMAT:
        try:
            return TR.oracle(fx)
        except TR.BadFixture as e:
            raise S.OracleError(str(e))
    if oracle is None:
        raise S.OracleError("no replay hook given (--replay-cmd or --replay-py) for a fixture that is not a %s case" % TR.FORMAT)
    return oracle(fx)


def run_dir(d, oracle):
    bad = 0
    n = 0
    for p in sorted(glob.glob(os.path.join(d, "*.json"))):
        with open(p) as f:
            fx = json.load(f)
        want = (fx.get("expect") or {}).get("converges", True)
        try:
            r = _replay(fx, oracle)
        except S.OracleError as e:
            print("ERROR %s: %s" % (os.path.basename(p), e))
            bad += 1
            n += 1
            continue
        got = not r["diverged"]
        ok = got == want
        n += 1
        bad += 0 if ok else 1
        print("%s %s (%s)" % ("PASS" if ok else "FAIL", os.path.basename(p), "converged" if got else "diverged: " + str(S.signature(r))))
    return n, bad


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--replay-cmd")
    ap.add_argument("--replay-py")
    ap.add_argument("--replay-path")
    ap.add_argument("--tmpdir")
    args = ap.parse_args(argv)
    oracle = None
    try:
        if args.replay_py:
            oracle = S.PyOracle(args.replay_py, args.replay_path)
        elif args.replay_cmd:
            oracle = S.CommandOracle(args.replay_cmd, tmpdir=args.tmpdir)
    except S.OracleError as e:
        print("cannot set up the replay hook: %s" % e)
        return 2
    n, bad = run_dir(args.dir, oracle)
    if n == 0:
        print("no fixtures found in " + args.dir)
        return 2
    print("%d fixture(s), %d failed" % (n, bad))
    if bad == 0:
        print("ALL OK")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
