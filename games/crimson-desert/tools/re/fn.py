# Usage: fn.py <exe> <rva hex> : disassemble one whole function, bounded by its .pdata entry.
import sys, struct, bisect, pefile, capstone
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]; raw = pe.get_data(d.VirtualAddress, d.Size)
funcs = sorted(struct.unpack_from("<II", raw, i) for i in range(0, len(raw) - 11, 12))
rva = int(sys.argv[2], 16); i = bisect.bisect_right([f[0] for f in funcs], rva) - 1
b, e = funcs[i]; print(f"; function {b:#x}-{e:#x}")
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for ins in md.disasm(pe.get_data(b, e - b), base + b): print(f"{ins.address - base:#09x}  {ins.mnemonic:6} {ins.op_str}")
