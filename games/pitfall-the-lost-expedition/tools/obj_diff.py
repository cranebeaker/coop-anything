"""Prints player-object floats that change across savestates 08-11 (idle, left, right, right).

Usage: python -I obj_diff.py <savestate prefix without .NN.p2s>
"""
import struct, sys, zipfile, math
def mem(p):
    with zipfile.ZipFile(p) as z: return z.read("eeMemory.bin")
prefix = sys.argv[1]
obj = 0x00EB5300
slots = ("08", "09", "10", "11")
ms = [mem(f"{prefix}.{s}.p2s") for s in slots]
print("offset      idle      left     right    right2")
for off in range(0, 0x2000, 4):
    vals = [struct.unpack_from("<f", m, obj + off)[0] for m in ms]
    if not all(math.isfinite(v) and abs(v) < 1e4 for v in vals): continue
    if max(vals) - min(vals) < 0.2: continue
    if 0x340 <= off < 0x34C: continue
    print(f"+0x{off:04X} " + " ".join(f"{v:9.3f}" for v in vals))
