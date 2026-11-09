"""Check that a proxy DLL is x64 and forwards every export: check_forwarders.py <dll> [expected count]"""
import struct
import sys

d = open(sys.argv[1], "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
machine = struct.unpack_from("<H", d, pe + 4)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
osz = struct.unpack_from("<H", d, pe + 20)[0]
opt = pe + 24
erva, esz = struct.unpack_from("<II", d, opt + 112)
secs = [struct.unpack_from("<8sIIII", d, opt + osz + i * 40) for i in range(nsec)]


def off(r):
    for _, vs, va, rs, ro in secs:
        if va <= r < va + max(vs, rs):
            return r - va + ro
    raise ValueError(hex(r))


e = off(erva)
base, nfun, nnames, funcs, names, ords = struct.unpack_from("<IIIIII", d, e + 16)
fwd = sum(1 for i in range(nfun) if erva <= struct.unpack_from("<I", d, off(funcs) + 4 * i)[0] < erva + esz)
expected = int(sys.argv[2]) if len(sys.argv) > 2 else nfun
local=set(sys.argv[3:]);actual=set()
for i in range(nnames):
    at=off(struct.unpack_from('<I',d,off(names)+4*i)[0]);name=d[at:d.index(b'\0',at)].decode()
    ordinal=struct.unpack_from('<H',d,off(ords)+2*i)[0]
    target=struct.unpack_from('<I',d,off(funcs)+4*ordinal)[0]
    if not erva<=target<erva+esz:actual.add(name)
ok = machine == 0x8664 and nfun == expected and fwd==expected-len(local) and actual==local
print(f"machine={hex(machine)} exports={nfun} names={nnames} forwarders={fwd} local={sorted(actual)} expected={expected} -> {'OK' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
