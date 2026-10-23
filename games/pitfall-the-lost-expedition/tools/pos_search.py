"""Finds the player's position vector from five savestates.

Usage: python -I tools/pos_search.py <base> <idle> <left> <right1> <right2>

base/idle: no input between them, so the position must not change.
left, right1, right2: savestates after walking left, right, right.
Keeps xyz float triples that are idle-stable, move 0.3-50 units each step, and come back
toward the start after left-then-right. Prints candidates and words that point below them.
"""
import math
import struct
import sys
import zipfile


def ee_floats(path):
    with zipfile.ZipFile(path) as z:
        data = z.read("eeMemory.bin")
    return data, struct.unpack(f"<{len(data) // 4}f", data)


def main():
    states = [ee_floats(p) for p in sys.argv[1:6]]
    floats = [f for _, f in states]
    n = len(floats[0])
    hits = []
    for i in range(0x00100000 // 4, n - 3):
        vecs = [f[i:i + 3] for f in floats]
        if not all(all(math.isfinite(c) and abs(c) < 1e5 for c in v) for v in vecs):
            continue
        base, idle, left, right1, right2 = vecs
        if math.dist(base, idle) > 1e-4 or max(abs(c) for c in idle) < 5.0:
            continue
        steps = [math.dist(idle, left), math.dist(left, right1), math.dist(right1, right2)]
        if not all(0.3 < s < 50 for s in steps):
            continue
        if math.dist(idle, right1) > 0.6 * steps[0]:
            continue
        hits.append((i * 4, vecs, steps))
    print(f"{len(hits)} candidate position vectors")
    words = struct.unpack(f"<{n}I", states[4][0])
    for off, vecs, steps in hits[:40]:
        print(f"0x{off:08X} steps " + ", ".join(f"{s:.2f}" for s in steps))
        for name, v in zip(("base", "idle", "left", "right1", "right2"), vecs):
            print(f"    {name:6} " + " ".join(f"{c:10.3f}" for c in v))
        refs = sorted({w for w in words if off - 0x2000 <= w <= off and w % 16 == 0})
        print("    objects pointed to within 0x2000 below: "
              + " ".join(f"0x{r:08X}(+0x{off - r:X})" for r in refs[-6:]))


if __name__ == "__main__":
    main()

