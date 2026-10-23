"""Finds MIPS code that builds a 32-bit constant (lui + addiu/ori) in a PS2 ELF.

Usage: python -I tools/elf_xref.py <elf> <value> [<value> ...]

For each hit prints the instruction address, the enclosing function start (nearest
"addiu sp, sp, -N" above) and whether the built register is stored with sw to offset 0 of
another register within the next 8 instructions (what a C++ constructor does with its class
table pointer).
"""
import struct
import sys


def segments(data):
    phoff, = struct.unpack_from("<I", data, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
    for i in range(phnum):
        p_type, p_offset, p_vaddr, _, p_filesz, _, p_flags, _ = struct.unpack_from(
            "<8I", data, phoff + i * phentsize)
        if p_type == 1 and p_flags & 1:
            yield p_offset, p_vaddr, p_filesz


def main():
    data = open(sys.argv[1], "rb").read()
    targets = [int(v, 0) for v in sys.argv[2:]]
    for off, vaddr, size in segments(data):
        words = struct.unpack_from(f"<{size // 4}I", data, off)
        for i, w in enumerate(words):
            if w >> 26 != 0x0F:  # lui
                continue
            rt, hi = (w >> 16) & 31, w & 0xFFFF
            for j in range(i + 1, min(i + 9, len(words))):
                w2 = words[j]
                op, rs, rt2, imm = w2 >> 26, (w2 >> 21) & 31, (w2 >> 16) & 31, w2 & 0xFFFF
                if rs != rt or op not in (0x09, 0x0D):
                    continue
                low = imm - 0x10000 if (op == 0x09 and imm & 0x8000) else imm
                value = ((hi << 16) + low) & 0xFFFFFFFF
                if value not in targets:
                    break
                addr = vaddr + j * 4
                func = None
                for k in range(j, max(j - 2000, -1), -1):
                    wk = words[k]
                    if wk >> 16 == 0x27BD and wk & 0x8000:  # addiu sp, sp, -N
                        func = vaddr + k * 4
                        break
                store = None
                for k in range(j + 1, min(j + 9, len(words))):
                    wk = words[k]
                    if wk >> 26 == 0x2B and (wk >> 16) & 31 == rt2 and wk & 0xFFFF == 0:
                        store = f"sw r{rt2}, 0(r{(wk >> 21) & 31}) at 0x{vaddr + k * 4:08X}"
                        break
                print(f"0x{value:08X} built at 0x{addr:08X} in function ~0x{func or 0:08X}"
                      + (f"; {store}" if store else ""))
                break


if __name__ == "__main__":
    main()
