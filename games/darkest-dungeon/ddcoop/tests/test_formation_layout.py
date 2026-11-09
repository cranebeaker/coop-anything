"""Read-only ABI proof for the normal dungeon formation reset, exact Steam EXE."""
from pathlib import Path
import hashlib
import struct
import sys

b = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest() == 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
base = 0x140000000
pe = struct.unpack_from('<I', b, 60)[0]
q = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
sections = [struct.unpack_from('<8sIIII', b, q + 40*i) for i in range(struct.unpack_from('<H', b, pe+6)[0])]
def read(va, size):
    for _, _, rva, n, raw in sections:
        if rva <= va-base and va-base+size <= rva+n:
            return b[raw+va-base-rva:raw+va-base-rva+size]
    raise AssertionError(hex(va))
def rel(va, prefix, size):
    code=read(va,size);assert code.startswith(bytes.fromhex(prefix))
    return va+size+struct.unpack_from('<i',code,size-4)[0]
vt=0x140ed3090
col=struct.unpack('<Q',read(vt-8,8))[0]
td=base+struct.unpack('<I',read(col+12,4))[0]
assert b'RenderPartyReorderButton@DDTabPanel@Screen@RaidUI' in read(td+16,200)
assert struct.unpack('<Q',read(vt+16,8))[0] == 0x1407812d0
assert rel(0x1407812d0,'488b0d',7) == 0x14118e570
assert rel(0x1407812d7,'e9',5) == 0x14076f060
assert rel(0x14042bc65,'e8',5) == 0x14076f060 # Keyboard input converges on the same function.
assert read(0x14042bc54,5).hex() == 'b912000000'
# Whole instructions, no RIP-relative relocation in the copied11-byte prefix.
assert read(0x14076f060,11).hex() == '488bc45355565741564157'
assert rel(0x14076f0ad,'e8',5) == 0x140614cb0
assert read(0x14076f0ba,7).hex() == 'c6851941000001' # display+4119: animation busy
assert read(0x14076f1f2,7).hex() == '488b4608488938' # raid+8 party vector, first pointer
for va,mask in [(0x14076f192,1),(0x14076f242,2),(0x14076f2f7,4),(0x14076f3ad,8)]:
    assert read(va,5) == b'\xba'+struct.pack('<I',mask)
assert read(0x140614cc3,6).hex() == '4c8bf2488be9' # RDX output array, RCX raid
assert read(0x140614d49,7).hex() == '4439827c130000' # saved hero matched by persistent ID
print('PASS formation ABI: normal button RTTI and keyboard converge; exact relocatable11-byte prologue; native raid/default array, rank masks, vector writes and animation flag proven; no game execution')
