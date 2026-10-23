# Usage: iat.py <exe> <rva hex> ... : import name for IAT slot RVAs.
import sys, pefile
pe = pefile.PE(sys.argv[1], fast_load=True); pe.parse_data_directories([pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
want = {int(a, 16) for a in sys.argv[2:]}
for e in pe.DIRECTORY_ENTRY_IMPORT:
    for i in e.imports:
        r = i.address - pe.OPTIONAL_HEADER.ImageBase
        if r in want: print(f"{r:#x} {e.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}")
