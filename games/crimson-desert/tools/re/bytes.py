# Usage: bytes.py <exe> <rva hex> [n] : raw bytes at an RVA as a C array.
import sys, pefile
pe = pefile.PE(sys.argv[1], fast_load=True); r = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 16
print(", ".join(f"0x{b:02X}" for b in pe.get_data(r, n)))
