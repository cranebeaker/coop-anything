# Usage: rows.py <payload.bin> <element size> [max rows] : 7-byte header {u8,u32,u8,u8} then fixed-size elements as hex rows.
import struct, sys
b = open(sys.argv[1], "rb").read(); n = int(sys.argv[2]); mx = int(sys.argv[3]) if len(sys.argv) > 3 else 8
s, p, a, c = struct.unpack_from("<BIBB", b, 0); body = b[7:]
print(f"scope={s} player={p:#x} a={a} b={c} len={len(b)} rem={len(body) % n} elements={len(body)//n}")
for i in range(0, min(len(body), n * mx), n): print("  " + body[i:i+n].hex(" "))
