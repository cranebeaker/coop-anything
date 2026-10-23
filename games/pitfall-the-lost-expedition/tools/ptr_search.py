"""Lists words below 0x00800000 that point at most 0x3000 bytes below a target address.

Usage: python -I ptr_search.py <state.p2s> <hex target>
"""
import struct, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z: m = z.read("eeMemory.bin")
target = int(sys.argv[2], 16)
n = len(m)//4
w = struct.unpack(f"<{n}I", m)
for i, v in enumerate(w):
    if target - 0x3000 <= v <= target and i*4 < 0x00800000:
        print(f"0x{i*4:08X} -> 0x{v:08X} (pos at +0x{target - v:X})")
