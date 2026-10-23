"""Checks the player pointer in savestates 01/07/11 and dumps player fields +0x640..+0x690.

Usage: python -I obj_check.py <savestate prefix without .NN.p2s>
"""
import struct, sys, zipfile
def mem(p):
    with zipfile.ZipFile(p) as z: return z.read("eeMemory.bin")
u32 = lambda m, a: struct.unpack_from("<I", m, a)[0]
f32 = lambda m, a: struct.unpack_from("<f", m, a)[0]
prefix = sys.argv[1]
for slot in ("01", "07", "11"):
    m = mem(f"{prefix}.{slot}.p2s")
    print(f"--- state {slot}")
    for ptr in (0x0052F198, 0x0052F218, 0x00582FF4):
        obj = u32(m, ptr)
        if 0x100000 <= obj < len(m) - 0x2000:
            pos = [f32(m, obj + 0x340 + 4 * k) for k in range(3)]
            print(f"  [0x{ptr:08X}] = 0x{obj:08X}  pos(+0x340) = " + " ".join(f"{c:.3f}" for c in pos))
        else:
            print(f"  [0x{ptr:08X}] = 0x{obj:08X}  (not a heap pointer)")
m = mem(f"{prefix}.11.p2s")
obj = 0x00EB5300
print("--- Harry object fields +0x640..+0x690 in state 11 (int / float)")
for off in range(0x640, 0x690, 4):
    print(f"  +0x{off:X}: {u32(m, obj + off):>11}  {f32(m, obj + off):12.4f}")
