# Usage: questlist.py <UpdateQuestListAck payload.bin> [key hex] : header + 34-byte elements {u32,u32,u64,u64,u64,u8,u8}.
import struct, sys, collections
b = open(sys.argv[1], "rb").read()
scope, player, a, c = struct.unpack_from("<BIBB", b, 0); body = b[7:]
assert len(body) % 34 == 0, len(body)
els = [struct.unpack_from("<IIQQQBB", body, i) for i in range(0, len(body), 34)]
print(f"scope={scope} player={player:#x} a={a} b={c} elements={len(els)}")
print("state histogram (u8 #6):", dict(collections.Counter(e[5] for e in els)), " flag (u8 #7):", dict(collections.Counter(e[6] for e in els)))
print("nonzero field counts:", [sum(1 for e in els if e[i]) for i in range(7)])
want = int(sys.argv[2], 16) if len(sys.argv) > 2 else None
for e in els[:6] if want is None else [e for e in els if e[0] == want]:
    print(f"  key={e[0]:#x} f1={e[1]:#x} f2={e[2]:#x} f3={e[3]:#x} f4={e[4]:#x} state={e[5]} flag={e[6]}")
