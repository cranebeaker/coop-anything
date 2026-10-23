"""Finds level CRC32 values (ptle-tools world_infos.json) in static RAM of two savestates.

Usage: python -I level_search.py <world_infos.json> <savestate prefix without .NN.p2s>
"""
import json, struct, sys, zipfile
infos = json.load(open(sys.argv[1], encoding="utf-8"))
crcs = {}
for region, areas in infos.items():
    if region == "//": continue
    for a in areas:
        crcs[int(a["asset_crc32"].replace(" ", ""), 16)] = f'{a["area_name"]} ({a["asset_name"]}, {region})'
print(len(crcs), "level CRCs")
def mem(p):
    with zipfile.ZipFile(p) as z: return z.read("eeMemory.bin")
found = {}
for slot in ("01", "07"):
    m = mem(f"{sys.argv[2]}.{slot}.p2s")
    w = struct.unpack(f"<{len(m)//4}I", m)
    for i, v in enumerate(w):
        if v in crcs and i * 4 < 0x00800000:
            found.setdefault(i * 4, {})[slot] = v
for addr, by in sorted(found.items()):
    a, b = by.get("01"), by.get("07")
    tag = "  <-- differs" if a and b and a != b else ""
    print(f"0x{addr:08X}: prologue={crcs.get(a, '-')}  jungle={crcs.get(b, '-')}{tag}")
