# Usage: cap.py <capture.bin> <opcode,...> <outdir> : writes each matching packet payload as <outdir>/<op>_<n>_<t>.bin and prints an index.
import struct, sys, os
REC = struct.Struct("<QBBHHHII")
data = open(sys.argv[1], "rb").read(); assert data[:4] == b"CDPK"
ops = {int(x) for x in sys.argv[2].split(",")}; out = sys.argv[3]; os.makedirs(out, exist_ok=True)
pos = 12; t0 = None; n = {}
while pos + REC.size <= len(data):
    t, d, _, size, copied, _, sess, th = REC.unpack_from(data, pos); pos += REC.size
    pkt = data[pos:pos + copied]; pos += copied
    t0 = t if t0 is None else t0
    if len(pkt) < 5: continue
    op, slot, plen = struct.unpack_from("<HbH", pkt, 0)
    if op in ops:
        n[op] = n.get(op, 0) + 1; ts = (t - t0) / 1e6
        fn = f"{out}/{op}_{n[op]:03}_{ts:.3f}.bin"; open(fn, "wb").write(pkt[5:])
        print(f"{op} #{n[op]} t={ts:.3f} plen={plen} copied={len(pkt)-5}{' TRUNC' if len(pkt)-5 < plen else ''}")
