"""Lists objects in a savestate that share the player's base layout.

Usage: python -I tools/actor_scan.py <state.p2s> [player_ptr_addr]

Base layout seen on the player (NTSC-U SLUS-20408): +0x000 class table pointer into the ELF,
+0x03C/+0x048 bounding box min/max (xyz floats), +0x160 4x4 world matrix whose last row
(+0x190) is the world position. An object matches when its class pointer lies in the ELF range,
the box is ordered, the 3x3 rotation rows have length ~1 and the matrix w column is 0,0,0,1.
"""
import collections
import math
import struct
import sys
import zipfile

ELF_LO, ELF_HI = 0x00100000, 0x00540000
PLAYER_PTR = 0x0052F198


def main():
    with zipfile.ZipFile(sys.argv[1]) as z:
        m = z.read("eeMemory.bin")
    ptr_addr = int(sys.argv[2], 0) if len(sys.argv) > 2 else PLAYER_PTR
    u = lambda a: struct.unpack_from("<I", m, a)[0]
    fs = lambda a, n: struct.unpack_from(f"<{n}f", m, a)
    player = u(ptr_addr)
    ppos = fs(player + 0x190, 3)
    found = []
    for obj in range(0x00540000, len(m) - 0x200, 16):
        cls = u(obj)
        if not ELF_LO <= cls < ELF_HI:
            continue
        mat = fs(obj + 0x160, 16)
        if not all(math.isfinite(v) for v in mat):
            continue
        if any(abs(mat[i]) > 1e-3 for i in (3, 7, 11)) or abs(mat[15] - 1.0) > 1e-3:
            continue
        rows = [mat[0:3], mat[4:7], mat[8:11]]
        if not all(abs(math.hypot(*r) - 1.0) < 0.02 for r in rows):
            continue
        lo, hi = fs(obj + 0x3C, 3), fs(obj + 0x48, 3)
        if not all(math.isfinite(v) for v in lo + hi) or not all(a <= b for a, b in zip(lo, hi)):
            continue
        pos = mat[12:15]
        if max(abs(v) for v in pos) > 1e5:
            continue
        found.append((obj, cls, pos, math.dist(pos, ppos)))
    by_class = collections.Counter(c for _, c, _, _ in found)
    print(f"player 0x{player:08X} class 0x{u(player):08X} at "
          + " ".join(f"{v:.2f}" for v in ppos))
    print(f"{len(found)} objects match the base layout, {len(by_class)} classes")
    for cls, n in by_class.most_common():
        print(f"  class 0x{cls:08X}: {n}")
    print("nearest 25 to the player:")
    for obj, cls, pos, d in sorted(found, key=lambda t: t[3])[:25]:
        print(f"  0x{obj:08X} class 0x{cls:08X} dist {d:8.2f} pos "
              + " ".join(f"{v:9.2f}" for v in pos))


if __name__ == "__main__":
    main()
