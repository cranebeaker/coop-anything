# Usage: disp2.py <exe> <disp hex> <lo rva hex> <hi rva hex> : any [reg + disp] memory operand in executable sections within [lo, hi).
import sys, re, pefile, capstone
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
disp = int(sys.argv[2], 16); lo = int(sys.argv[3], 16); hi = int(sys.argv[4], 16)
needle = disp.to_bytes(4, "little"); md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
data = pe.get_data(lo, hi - lo)
for m in re.finditer(re.escape(needle), data):
    for back in range(2, 6):
        st = m.start() - back
        ins = next(md.disasm(data[st:st + 16], base + lo + st), None)
        if ins and ins.size >= back + 4 and f"+ {disp:#x}]" in ins.op_str:
            print(f"{lo + st:#09x}  {ins.mnemonic} {ins.op_str}"); break
