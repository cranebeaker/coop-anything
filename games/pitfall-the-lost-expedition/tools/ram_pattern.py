"""Finds floats that follow a movement pattern across savestates (start, left, right, right).

Usage: python -I tools/ram_pattern.py <start.p2s> <after_left.p2s> <after_right1.p2s> <after_right2.p2s>

Keeps 32-bit floats whose change is one sign for the left step and the opposite sign for both
right steps, each step at least MIN_STEP units, then groups hits that sit within 0x40 bytes of
each other and lists the words that point near each group.
"""
import math
import struct
import sys
import zipfile

MIN_STEP = 0.2
MAX_STEP = 200.0


def ee_memory(path):
    with zipfile.ZipFile(path) as z:
        return z.read("eeMemory.bin")


def main():
    mems = [ee_memory(p) for p in sys.argv[1:5]]
    n = len(mems[0]) // 4
    floats = [struct.unpack(f"<{n}f", m) for m in mems]
    hits = []
    for i in range(0x00100000 // 4, n):
        s, l, r1, r2 = (f[i] for f in floats)
        if not all(math.isfinite(v) and abs(v) < 1e5 for v in (s, l, r1, r2)):
            continue
        d1, d2, d3 = l - s, r1 - l, r2 - r1
        if not all(MIN_STEP < abs(d) < MAX_STEP for d in (d1, d2, d3)):
            continue
        if d1 * d2 < 0 and d2 * d3 > 0:
            hits.append((i * 4, s, l, r1, r2))
    print(f"{len(hits)} floats follow the pattern")
    groups = []
    for hit in hits:
        if groups and hit[0] - groups[-1][-1][0] <= 0x40:
            groups[-1].append(hit)
        else:
            groups.append([hit])
    words = struct.unpack(f"<{n}I", mems[3])
    for group in groups[:30]:
        lo = group[0][0]
        refs = [j * 4 for j, w in enumerate(words) if lo - 0x2000 <= w <= lo and w % 16 == 0]
        print(f"group at 0x{lo:08X} ({len(group)} floats), {len(refs)} pointers within 0x2000 below")
        for off, s, l, r1, r2 in group:
            print(f"    0x{off:08X}: {s:10.3f} -> {l:10.3f} -> {r1:10.3f} -> {r2:10.3f}")
        targets = sorted({words[r // 4] for r in refs})[:6]
        if targets:
            print("    pointed-to bases: " + " ".join(f"0x{t:08X} (+0x{lo - t:X})" for t in targets))


if __name__ == "__main__":
    main()
