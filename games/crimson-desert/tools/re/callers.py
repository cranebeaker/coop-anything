# Usage: callers.py <exe> <target rva hex> [target2 ...] : direct E8 calls / E9 jumps to the targets, in executable sections.
import sys, struct, pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
targets = {int(t, 16) for t in sys.argv[2:]}
for s in pe.sections:
    if not (s.Characteristics & 0x20000000): continue
    d = s.get_data(); va = s.VirtualAddress
    for op in (0xE8, 0xE9):
        i = d.find(bytes([op]))
        while i != -1 and i + 5 <= len(d):
            t = va + i + 5 + struct.unpack_from("<i", d, i + 1)[0]
            if t in targets: print(f"{va + i:#09x} {'call' if op == 0xE8 else 'jmp '} -> {t:#x}")
            i = d.find(bytes([op]), i + 1)
