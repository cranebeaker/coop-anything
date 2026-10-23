"""Finds the player object in two PCSX2 savestates by testing the GameCube field layout.

Usage: python -I tools/ram_diff.py <state_a.p2s> <state_b.p2s>

Savestates must be uncompressed (SavestateCompressionType = 0). Layout hypothesis from
Avasam/ptle-tools (GameCube US): player +0x338 position xyz, +0x1A0 velocity xyz,
+0x658 health capacity, +0x65C health amount, +0x680 in-cutscene, +0x195C idols.
"""
import math
import struct
import sys
import zipfile

EE_BASE = 0x00000000
POS, VEL, HP_CAP, HP_AMT = 0x338, 0x1A0, 0x658, 0x65C


def ee_memory(path):
    with zipfile.ZipFile(path) as z:
        return z.read("eeMemory.bin")


def f32(mem, off):
    return struct.unpack_from("<f", mem, off)[0]


def u32(mem, off):
    return struct.unpack_from("<I", mem, off)[0]


def vec(mem, off):
    return tuple(f32(mem, off + 4 * i) for i in range(3))


def plausible(v, limit=1e5):
    # World positions: finite, at least one coordinate clearly away from the 0..2 range colours use.
    return all(math.isfinite(c) and abs(c) < limit for c in v) and any(abs(c) > 2.5 for c in v)


def main():
    a, b = ee_memory(sys.argv[1]), ee_memory(sys.argv[2])
    size = len(a)
    hits = []
    for base in range(0x00100000, size - 0x2000, 4):
        pa, pb = vec(a, base + POS), vec(b, base + POS)
        if not (plausible(pa) and plausible(pb)):
            continue
        moved = math.dist(pa, pb)
        if not 0.05 < moved < 500:
            continue
        for mem in (a, b):
            cap_i, amt_i = u32(mem, base + HP_CAP), u32(mem, base + HP_AMT)
            cap_f, amt_f = f32(mem, base + HP_CAP), f32(mem, base + HP_AMT)
            ints_ok = 0 < cap_i <= 1000 and amt_i <= cap_i
            floats_ok = math.isfinite(cap_f) and 0 < cap_f <= 1000 and 0 <= amt_f <= cap_f
            if not (ints_ok or floats_ok):
                break
        else:
            hits.append((base, pa, pb, moved))
    print(f"{len(hits)} layout matches")
    hit_set = {h[0] for h in hits}
    pointers = {}
    for off in range(0, size, 4):
        word = u32(b, off)
        if word in hit_set:
            pointers.setdefault(word, []).append(off)
    hits = [h for h in hits if h[0] in pointers]
    print(f"{len(hits)} of them are pointed to from somewhere")
    for base, pa, pb, moved in hits[:40]:
        refs = pointers.get(base, [])
        print(f"obj 0x{base:08X} moved {moved:8.3f}  pos A {fmt(pa)}  pos B {fmt(pb)}")
        print(f"    vel B {fmt(vec(b, base + VEL))}  hp A {hp(a, base)}  hp B {hp(b, base)}")
        print(f"    referenced from {len(refs)} words: " + " ".join(f"0x{r:08X}" for r in refs[:8]))


def fmt(v):
    return "(" + ", ".join(f"{c:9.2f}" for c in v) + ")"


def hp(mem, base):
    return (f"int {u32(mem, base + HP_AMT)}/{u32(mem, base + HP_CAP)} "
            f"float {f32(mem, base + HP_AMT):.2f}/{f32(mem, base + HP_CAP):.2f}")


if __name__ == "__main__":
    main()
