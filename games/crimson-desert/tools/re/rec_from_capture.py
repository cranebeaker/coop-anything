# Usage: rec_from_capture.py <capture.bin> <from s> <to s> : prints cdmp_stage_record.txt lines ("D op ms hex" for
# quest dialog messages 2342/2621 the game sent, "M key kind state row" for mission rows new/changed, "N runtime static" per NPC in them, "S key kind_before kind_after" per row whose kind changed, "KKKKKKKK branch flags" per stage branch the game chose) between the two
# capture times. The mission list before <from> is the baseline. Same formats as cdmp stage_replay.
import struct, sys
REC = struct.Struct("<QBBHHHII")
data = open(sys.argv[1], "rb").read(); assert data[:4] == b"CDPK"
lo, hi = float(sys.argv[2]), float(sys.argv[3])
pos = 12; t0 = None; rows = {}; first = None; actors = {}; npcs = []; inst_key = {}
while pos + REC.size <= len(data):
    t, d, _, sz, copied, _, sess, th = REC.unpack_from(data, pos); pos += REC.size
    pkt = data[pos:pos + copied]; pos += copied
    t0 = t if t0 is None else t0; ts = (t - t0) / 1e6
    if len(pkt) < 5 or ts > hi: continue
    op, slot, plen = struct.unpack_from("<HbH", pkt, 0); p = pkt[5:]
    assert len(p) >= plen or op not in (2342, 2621, 3009), "truncated"
    if op == 2661 and len(p) >= 23: inst_key[struct.unpack_from("<Q", p, 9)[0]] = struct.unpack_from("<I", p, 19)[0]
    if op == 2436 and d == 1 and ts >= lo and len(p) >= 14:
        k = inst_key.get(struct.unpack_from("<Q", p)[0], 0)
        if k: print(f"{k:08X} {struct.unpack_from('<I', p, 8)[0]} {struct.unpack_from('<H', p, 12)[0]}")
    if op == 2005 and len(p) >= 26: actors[struct.unpack_from("<I", p, 3)[0]] = struct.unpack_from("<I", p, 22)[0]
    if op == 3009 and len(p) >= 7 and (len(p) - 7) % 55 == 0:
        for i in range(7, len(p), 55):
            r = p[i:i + 55]; k = struct.unpack_from("<I", r)[0]
            if rows.get(k) != r and ts >= lo:
                print(f"M {k:08X} {r[4]} {r[5]} {r.hex().upper()}")
                before = rows[k][4] if k in rows else 0
                if before != r[4]: print(f"S {k:08X} {before} {r[4]}")
            rows[k] = r
    elif op in (2342, 2621) and d == 1 and ts >= lo:  # 1 = C>S
        first = ts if first is None else first
        print(f"D {op} {round((ts - first) * 1000)} {p.hex().upper()}")
        rid = struct.unpack_from("<I", p)[0]
        if rid >> 24 == 0xB0 and rid not in npcs:
            npcs.append(rid); print(f"N {rid:08X} {actors.get(rid, 0):08X}")
