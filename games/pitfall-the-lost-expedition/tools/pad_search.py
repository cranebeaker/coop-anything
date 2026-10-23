"""Finds controller button data: halfwords equal in two idle states, different for two buttons.

Usage: python -I tools/pad_search.py <idle1.p2s> <buttonA.p2s> <buttonB.p2s> <idle2.p2s>
"""
import struct
import sys
import zipfile


def ee(path):
    with zipfile.ZipFile(path) as z:
        return z.read("eeMemory.bin")


def main():
    i1, a, b, i2 = (ee(p) for p in sys.argv[1:5])
    n = len(i1) // 2
    h = [struct.unpack(f"<{n}H", m) for m in (i1, a, b, i2)]
    hits = []
    for k in range(0x00100000 // 2, n):
        v0, va, vb, v1 = h[0][k], h[1][k], h[2][k], h[3][k]
        if (v0 == v1 and bin(va ^ v0).count("1") == 1 and bin(vb ^ v0).count("1") == 1
                and (va ^ v0) != (vb ^ v0)):
            hits.append((k * 2, v0, va, vb))
    print(f"{len(hits)} halfwords: idle == idle, L1 and R1 each flip one different bit")
    for addr, v0, va, vb in hits[:60]:
        print(f"  0x{addr:08X}: idle {v0:04X}  L1 {va:04X} (xor {v0 ^ va:04X})  R1 {vb:04X} (xor {v0 ^ vb:04X})")


if __name__ == "__main__":
    main()

