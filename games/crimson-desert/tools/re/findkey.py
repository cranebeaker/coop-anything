# Usage: findkey.py <capture.bin> <hex u32> [opcode,...] : first times each opcode carries that little-endian u32 (S>C and C>S).
import struct, sys
REC = struct.Struct("<QBBHHHII"); data = open(sys.argv[1], "rb").read(); assert data[:4] == b"CDPK"
needle = struct.pack("<I", int(sys.argv[2], 16)); ops = {int(x) for x in sys.argv[3].split(",")} if len(sys.argv) > 3 else None
pos = 12; t0 = None; seen = {}
while pos + REC.size <= len(data):
    t, d, _, sz, c, _, s, th = REC.unpack_from(data, pos); pos += REC.size; pkt = data[pos:pos + c]; pos += c
    t0 = t if t0 is None else t0
    if len(pkt) < 5: continue
    op = struct.unpack_from("<H", pkt)[0]
    if (ops is None or op in ops) and needle in pkt[5:]:
        seen.setdefault(op, []).append(round((t - t0) / 1e6, 3))
for op, ts in sorted(seen.items(), key=lambda x: x[1][0]): print(f"  op {op} {'C>S' if False else ''}x{len(ts)} first t={ts[0]} last t={ts[-1]}")
