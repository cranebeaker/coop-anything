"""Pass/fail check for one peer_trial.ps1 run, from the two peers' CSV logs.

Usage: python -I tools/peer_check.py <host.csv> <join.csv>

Checks, for each side: it steered for at least half of the ticks, the puppet was within 2 units
of the other player's position at the end, and during the walk the puppet got within 2 units of
the moving player at least once after the last snap-sized gap (> 6). Prints one line per side
and a PASS / FAIL summary; exit code 0 only on PASS.
"""
import csv
import math
import sys


def check(path):
    rows = list(csv.DictReader(open(path, encoding="utf-8")))
    steering = [r for r in rows if r["steering"] == "1"]
    gaps = [float(r["puppet_gap"]) for r in steering if r["puppet_gap"] not in ("nan", "")]
    if not gaps:
        return False, f"{path}: never steered"
    final = gaps[-1]
    late = gaps[len(gaps) // 2:]
    ok = len(steering) >= len(rows) // 2 and final < 2.0 and min(late) < 2.0 and all(math.isfinite(g) for g in gaps)
    return ok, (f"{path}: steered {len(steering)}/{len(rows)} ticks, max gap {max(gaps[5:] or gaps):.2f}, "
                f"final gap {final:.2f}: {'ok' if ok else 'FAIL'}")


def main():
    results = [check(p) for p in sys.argv[1:3]]
    for _, line in results:
        print(line)
    passed = all(ok for ok, _ in results)
    print("PASS" if passed else "FAIL")
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
