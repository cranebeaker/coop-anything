# Usage: classify2.py <exe> <csv> <name regex> : like classify.py, but each handler is bounded by its .pdata
# RUNTIME_FUNCTION entry (exact begin/end), so a scan never runs into the next function.
import sys, re, csv, struct, bisect, pefile, capstone
COMMON = {0x141f270, 0x448962c0, 0x40392380, 0x447f03bc, 0x447f02bc, 0x4143b450, 0x4143b670, 0x44898e24}
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]  # exception directory (.pdata)
raw = pe.get_data(d.VirtualAddress, d.Size)
funcs = sorted(struct.unpack_from("<III", raw, i)[:2] for i in range(0, len(raw) - 11, 12))
starts = [f[0] for f in funcs]
def bounds(rva):
    i = bisect.bisect_right(starts, rva) - 1
    return funcs[i] if i >= 0 and funcs[i][0] <= rva < funcs[i][1] else None
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
print(f"; {len(funcs)} RUNTIME_FUNCTION entries")
for row in csv.DictReader(open(sys.argv[2])):
    if "C->S" not in row["dir"] or not re.search(sys.argv[3], row["name"]): continue
    rva = int(row["handler_slot2"], 16); b = bounds(rva)
    if not b or b[0] != rva:
        print(f"{row['name']:45} {row['id']:>5} {rva:#x}  no exact .pdata entry ({b})"); continue
    # chained unwind info can split one function into several entries: follow consecutive ones
    end = b[1]
    extra = []; n = 0
    for ins in md.disasm(pe.get_data(rva, end - rva), base + rva):
        n += 1
        if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
            t = int(ins.op_str, 16) - base
            if ins.mnemonic == "jmp" and rva <= t < end: continue
            if (t & 0xffffffff) not in COMMON and t not in COMMON and t + 0x40000000 not in COMMON: extra.append(hex(t))
    print(f"{row['name']:45} {row['id']:>5} {rva:#x}-{end:#x} {n:4} insns  {'STUB' if not extra else 'live: ' + ' '.join(sorted(set(extra))[:6])}")
