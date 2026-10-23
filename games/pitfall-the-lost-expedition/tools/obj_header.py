"""Dumps the first 0x60 bytes and the +0x160 world matrix of the player object.

Usage: python -I obj_header.py <state.p2s>
"""
import struct, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z: m = z.read("eeMemory.bin")
obj = 0x00EB5300
u = lambda a: struct.unpack_from("<I", m, a)[0]
f = lambda a: struct.unpack_from("<f", m, a)[0]
print("player object header words:")
for off in range(0, 0x60, 4):
    w = u(obj + off)
    print(f"  +0x{off:03X}: 0x{w:08X}  {f(obj+off):12.4f}")
print("matrix +0x160..+0x19C:")
for r in range(4):
    print("  " + " ".join(f"{f(obj + 0x160 + 16*r + 4*c):9.3f}" for c in range(4)))
