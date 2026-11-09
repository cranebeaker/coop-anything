"""Compare two coop-rng.log traces (probe v5) to test whether Darkest Dungeon draws the same numbers.

Each log line: <seq> <kind r|w|s> <generator address> <state> <caller> [name]
Generator addresses differ between runs (heap, ASLR), so draws are grouped by the calling function
(caller offset in Darkest.exe). Per caller the sequences of generator states are compared.

Usage: python3 -I compare_rng.py <coop-rng-A.log> <coop-rng-B.log> [--show N]
"""
import collections
import sys


def load(path):
    draws = collections.defaultdict(list)   # caller -> [(seq, kind, state)]
    names = {}                               # caller -> generator name (named generators only)
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 5 or not parts[0].isdigit():
                continue
            seq, kind, state, caller = int(parts[0]), parts[1], int(parts[3]), parts[4]
            if len(parts) > 5:
                names[caller] = " ".join(parts[5:])
            draws[caller].append((seq, kind, state))
    return draws, names


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    show = int(sys.argv[sys.argv.index("--show") + 1]) if "--show" in sys.argv else 15
    a, names_a = load(sys.argv[1])
    b, names_b = load(sys.argv[2])
    names = {**names_a, **names_b}
    rows = []
    for caller in sorted(set(a) | set(b)):
        sa = [s for _, k, s in a.get(caller, []) if k != "s"]
        sb = [s for _, k, s in b.get(caller, []) if k != "s"]
        n = min(len(sa), len(sb))
        first = next((i for i in range(n) if sa[i] != sb[i]), None)
        prefix = n if first is None else first
        if first is None:
            status = "IDENTICAL" if len(sa) == len(sb) else "SAME-PREFIX"
        else:
            status = "DIVERGES"
        rows.append((status, caller, len(sa), len(sb), prefix, first, names.get(caller, "")))

    print(f"A: {sum(len(v) for v in a.values())} draws, B: {sum(len(v) for v in b.values())} draws, "
          f"{len(rows)} calling functions")
    for status in ("IDENTICAL", "SAME-PREFIX", "DIVERGES"):
        group = [r for r in rows if r[0] == status]
        print(f"\n{status}: {len(group)} callers")
        for _, caller, la, lb, prefix, first, name in sorted(group, key=lambda r: -max(r[2], r[3]))[:show]:
            extra = f" first difference at draw {first}" if first is not None else ""
            print(f"  {caller:>14}  A={la:<7} B={lb:<7} matching={prefix:<7}{extra}  {name}")

    print("\nReseeds (first 20 per run):")
    for log, tag in ((a, "A"), (b, "B")):
        seeds = [(seq, caller, st) for caller, lst in log.items() for seq, k, st in lst if k == "s"]
        for seq, caller, st in sorted(seeds)[:20]:
            print(f"  {tag} #{seq} seed {st} from {caller}")


if __name__ == "__main__":
    main()
