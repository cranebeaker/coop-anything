# Usage: immscan.py <exe> <lo rva hex> <hi rva hex> : RVAs in [lo,hi) of code bytes holding well-known RNG constants
# (as 32-bit little-endian immediates), with the containing function from .pdata.
import sys, struct, bisect, pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]; raw = pe.get_data(d.VirtualAddress, d.Size)
fns = sorted(struct.unpack_from("<II", raw, i) for i in range(0, len(raw) - 11, 12)); starts = [f[0] for f in fns]
K = {0x6C078965: "mt19937 init", 0x9908B0DF: "mt19937 matrix", 0x9D2C5680: "mt19937 temper", 0x000343FD: "msvc rand mul",
     0x00269EC3: "msvc rand add", 0x41C64E6D: "lcg 1103515245", 0x5851F42D: "pcg/knuth mul lo", 0x2545F491: "xorshift* lo",
     0x9E3779B9: "golden ratio", 0x85EBCA6B: "murmur fmix", 0xBF58476D: "splitmix", 0x94D049BB: "splitmix2", 0x4C957F2D: "pcg64 mul lo"}
lo, hi = int(sys.argv[2], 16), int(sys.argv[3], 16)
for s in pe.sections:
    if not s.Characteristics & 0x20000000: continue
    a, b = max(lo, s.VirtualAddress), min(hi, s.VirtualAddress + s.Misc_VirtualSize)
    if a >= b: continue
    data = s.get_data()[a - s.VirtualAddress:b - s.VirtualAddress]
    for k, name in K.items():
        pat = struct.pack("<I", k); i = data.find(pat)
        while i >= 0:
            r = a + i; j = bisect.bisect_right(starts, r) - 1
            fn = f"{fns[j][0]:#x}-{fns[j][1]:#x}" if j >= 0 and fns[j][0] <= r < fns[j][1] else "?"
            print(f"{r:#x} {name:18} fn {fn}"); i = data.find(pat, i + 1)
