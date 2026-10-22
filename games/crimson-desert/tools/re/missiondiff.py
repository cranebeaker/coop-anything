# Usage: missiondiff.py <capture.bin> : changes in UpdateMissionListAck (3009, 55-byte rows keyed by u32) and
# UpdateQuestListAck (2808, 34-byte rows) over a capture, plus StageBranchStateReq/Ack (2436/2981) times.
import struct, sys
REC = struct.Struct("<QBBHHHII")
data = open(sys.argv[1], "rb").read(); assert data[:4] == b"CDPK"
pos = 12; t0 = None; rows = {3009: {}, 2808: {}, 2724: {}}; size = {3009: 55, 2808: 34, 2724: 56}
while pos + REC.size <= len(data):
    t, d, _, sz, copied, _, sess, th = REC.unpack_from(data, pos); pos += REC.size
    pkt = data[pos:pos + copied]; pos += copied
    t0 = t if t0 is None else t0; ts = (t - t0) / 1e6
    if len(pkt) < 5: continue
    op, slot, plen = struct.unpack_from("<HbH", pkt, 0); p = pkt[5:]
    if op == 2436 and len(p) >= 14: print(f"{ts:9.3f} C>S StageBranchStateReq inst={struct.unpack_from('<Q', p)[0]:#x} branch={struct.unpack_from('<I', p, 8)[0]}")
    if op in rows and len(p) >= 7:
        s, pl, a, b = struct.unpack_from("<BIBB", p); body = p[7:]; n = size[op]
        if len(body) % n: print(f"{ts:9.3f} {op} odd len {len(p)} a={a}"); continue
        full = a == 1
        cur = {struct.unpack_from("<I", body, i)[0]: body[i:i+n] for i in range(0, len(body), n)}
        old = rows[op]
        if old:
            for k, v in cur.items():
                if old.get(k) != v:
                    o = old.get(k)
                    diff = "new" if o is None else " ".join(f"+{j}:{o[j]:02x}>{v[j]:02x}" for j in range(n) if o[j] != v[j])
                    print(f"{ts:9.3f} {op} {'full' if full else 'delta'} key={k:#x} {diff}")
            if full:
                for k in old.keys() - cur.keys(): print(f"{ts:9.3f} {op} full key={k:#x} GONE")
        elif not full: print(f"{ts:9.3f} {op} delta before any full list: {len(cur)} rows")
        if full: rows[op] = cur
        else: rows[op] = {**old, **cur}
