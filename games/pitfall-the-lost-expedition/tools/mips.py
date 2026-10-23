"""Tiny R5900 helper: decode a few common instructions and assemble the ones the spawn stub needs.

Usage: python -I tools/mips.py dis <elf> <addr> <count>
"""
import struct
import sys

REGS = ["zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"]
R = {name: i for i, name in enumerate(REGS)}

I_OPS = {0x09: "addiu", 0x0D: "ori", 0x0F: "lui", 0x23: "lw", 0x2B: "sw", 0x37: "ld", 0x3F: "sd",
         0x1E: "lq", 0x1F: "sq", 0x31: "lwc1", 0x39: "swc1", 0x04: "beq", 0x05: "bne"}


def decode(word, pc):
    op = word >> 26
    rs, rt, rd = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    imm = word & 0xFFFF
    simm = imm - 0x10000 if imm & 0x8000 else imm
    if word == 0:
        return "nop"
    if op in (0x02, 0x03):
        return f"{'j' if op == 2 else 'jal'} 0x{(pc & 0xF0000000) | ((word & 0x3FFFFFF) << 2):08X}"
    if op == 0 and word & 0x3F == 0x08:
        return f"jr {REGS[rs]}"
    if op == 0 and word & 0x3F == 0x09:
        return f"jalr {REGS[rd]}, {REGS[rs]}"
    if op == 0 and word & 0x3F in (0x21, 0x2D, 0x25):
        name = {0x21: "addu", 0x2D: "daddu", 0x25: "or"}[word & 0x3F]
        return f"{name} {REGS[rd]}, {REGS[rs]}, {REGS[rt]}"
    if op in I_OPS:
        name = I_OPS[op]
        if name == "lui":
            return f"lui {REGS[rt]}, 0x{imm:04X}"
        if name in ("addiu", "ori"):
            return f"{name} {REGS[rt]}, {REGS[rs]}, {simm if name == 'addiu' else hex(imm)}"
        if name in ("beq", "bne"):
            return f"{name} {REGS[rs]}, {REGS[rt]}, 0x{pc + 4 + simm * 4:08X}"
        reg = f"f{rt}" if name in ("lwc1", "swc1") else REGS[rt]
        return f"{name} {reg}, {simm}({REGS[rs]})"
    return f".word 0x{word:08X}"


# --- assembler for the stub -------------------------------------------------------------------

def _i(op, rs, rt, imm):
    return (op << 26) | (R[rs] << 21) | (R[rt] << 16) | (imm & 0xFFFF)


def addiu(rt, rs, imm): return _i(0x09, rs, rt, imm)
def ori(rt, rs, imm): return _i(0x0D, rs, rt, imm)
def lui(rt, imm): return _i(0x0F, "zero", rt, imm)
def lw(rt, off, rs): return _i(0x23, rs, rt, off)
def sw(rt, off, rs): return _i(0x2B, rs, rt, off)
def lq(rt, off, rs): return _i(0x1E, rs, rt, off)
def sq(rt, off, rs): return _i(0x1F, rs, rt, off)
def lwc1(ft, off, rs): return (0x31 << 26) | (R[rs] << 21) | (ft << 16) | (off & 0xFFFF)
def beq(rs, rt, off_words): return _i(0x04, rs, rt, off_words)
def jal(target): return (0x03 << 26) | ((target >> 2) & 0x3FFFFFF)
def jr(rs): return (R[rs] << 21) | 0x08
def move(rd, rs): return (R[rs] << 21) | (R[rd] << 11) | 0x2D  # daddu rd, rs, zero
NOP = 0


def li32(reg, value):
    hi, lo = (value >> 16) & 0xFFFF, value & 0xFFFF
    return [lui(reg, hi), ori(reg, reg, lo)]


def elf_reader(path):
    data = open(path, "rb").read()
    phoff, = struct.unpack_from("<I", data, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
    segs = []
    for i in range(phnum):
        t, off, va, _, fsz, _, _, _ = struct.unpack_from("<8I", data, phoff + i * phentsize)
        if t == 1:
            segs.append((off, va, fsz))

    def word(addr):
        for off, va, fsz in segs:
            if va <= addr < va + fsz:
                return struct.unpack_from("<I", data, off + addr - va)[0]
        raise ValueError(hex(addr))
    return word


def main():
    if sys.argv[1] == "dis":
        word = elf_reader(sys.argv[2])
        start, count = int(sys.argv[3], 0), int(sys.argv[4])
        for pc in range(start, start + 4 * count, 4):
            w = word(pc)
            print(f"0x{pc:08X}: {w:08X}  {decode(w, pc)}")


if __name__ == "__main__":
    main()
