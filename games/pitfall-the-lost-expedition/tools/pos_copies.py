"""Lists every copy of an object's drawn position (+0x190) inside the object.

Usage: python -I pos_copies.py <state.p2s> <hex object address>
"""
import struct, sys, zipfile, math
with zipfile.ZipFile(sys.argv[1]) as z: m = z.read("eeMemory.bin")
obj = int(sys.argv[2], 16)
f = lambda a: struct.unpack_from("<f", m, a)[0]
pos = [f(obj + 0x190 + 4*i) for i in range(3)]
print("translation", pos)
for off in range(0, 0x1000, 4):
    v = [f(obj + off + 4*i) for i in range(3)]
    if all(math.isfinite(c) for c in v) and math.dist(v, pos) < 0.01:
        print(f"  position copy at +0x{off:X}")
