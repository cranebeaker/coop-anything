# Usage: dis.py <exe> <rva hex> [count]  -- linear disassembly from an RVA, stops at the first ret/int3.
import sys, pefile, capstone
pe = pefile.PE(sys.argv[1], fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
rva = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 200
data = pe.get_data(rva, n * 15)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for i, ins in enumerate(md.disasm(data, base + rva)):
    print(f"{ins.address - base:#09x}  {ins.mnemonic:6} {ins.op_str}")
    if i >= n or ins.mnemonic in ("int3",): break
