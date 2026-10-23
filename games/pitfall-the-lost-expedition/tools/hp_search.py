"""Finds values that went from 10 to 4 between two savestates (used to find the prologue boss health).

Usage: python -I hp_search.py <before.p2s> <after.p2s>
"""
import struct, sys, zipfile
def mem(p):
    with zipfile.ZipFile(p) as z: return z.read("eeMemory.bin")
a, b = mem(sys.argv[1]), mem(sys.argv[2])
n = len(a)
for fmt, size, pairs in (("<B",1,[(10,4)]),("<H",2,[(10,4)]),("<I",4,[(10,4)]),("<f",4,[(10.0,4.0),(100.0,40.0),(1.0,0.4)])):
    for off in range(0x100000, n - size, size):
        va = struct.unpack_from(fmt, a, off)[0]; vb = struct.unpack_from(fmt, b, off)[0]
        for x, y in pairs:
            if (abs(va-x) < 1e-4 and abs(vb-y) < 1e-4):
                print(f"{fmt} 0x{off:08X}: {va} -> {vb}")
