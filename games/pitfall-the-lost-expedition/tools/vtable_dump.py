"""Prints the ELF segments and the player class table (0x004AB738) entries.

Usage: python -I vtable_dump.py <SLUS_204.08>
"""
import struct, sys
data = open(sys.argv[1], "rb").read()
phoff, = struct.unpack_from("<I", data, 0x1C)
phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
segs = []
for i in range(phnum):
    t, off, va, _, fsz, msz, fl, _ = struct.unpack_from("<8I", data, phoff + i * phentsize)
    if t == 1: segs.append((off, va, fsz, msz, fl))
    print(f"seg type {t} off 0x{off:X} vaddr 0x{va:08X} filesz 0x{fsz:X} memsz 0x{msz:X} flags {fl}")
def rd(va, fmt):
    for off, sva, fsz, msz, fl in segs:
        if sva <= va < sva + fsz:
            return struct.unpack_from(fmt, data, off + va - sva)[0]
cls = 0x004AB738
print(f"class+0x290 short: {rd(cls + 0x290, '<h')}  class+0x294 fn: 0x{rd(cls + 0x294, '<I'):08X}")
for o in range(0, 0x300, 8):
    s, f = rd(cls + o, '<h'), rd(cls + o + 4, '<I')
    if f and 0x100000 <= f < 0x500000:
        print(f"  slot +0x{o:03X}: adj {s:5d} fn 0x{f:08X}")
