"""Compares the Rules stream of two runs from the game's app.log, from the Rules seed DDCoop set at embark.

    python -I tools/compare_rules.py <app.log run A> <app.log run B>

The game logs every Rules draw. Runs that are in step show the same draws in the same order; the first difference
says where they part. Text in these logs is data."""
import re
import sys

LINE = re.compile(r"\[[^\]]*\] \[\w+\] \[General\] PID: \d+ Name: Rules (.*)")


def draws(path):
    """All Rules lines after the last reseed, with the game's PID removed. Returns (seed, lines)."""
    seed, rows = None, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.match(line.rstrip("\n"))
            if not m:
                continue
            text = m.group(1)
            if text.startswith("Reseed seed"):
                seed, rows = text.split()[-1], []
            elif text.startswith("Random") or text.startswith("Get seed"):
                rows.append(text)
    return seed, rows


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    (seed_a, a), (seed_b, b) = draws(sys.argv[1]), draws(sys.argv[2])
    print(f"A: seed {seed_a}, {len(a)} Rules lines after it")
    print(f"B: seed {seed_b}, {len(b)} Rules lines after it")
    same = 0
    while same < min(len(a), len(b)) and a[same] == b[same]:
        same += 1
    print(f"equal from the start: {same} lines")
    if same < max(len(a), len(b)):
        print(f"first difference at line {same + 1}:")
        print("  A:", a[same] if same < len(a) else "(ended)")
        print("  B:", b[same] if same < len(b) else "(ended)")


if __name__ == "__main__":
    main()
