"""Read-only proof of optional checkpoint restore vs new normal raid, exact EXE."""
from pathlib import Path
import hashlib,struct,sys
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=='f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
base=0x140000000;pe=struct.unpack_from('<I',b,60)[0];q=pe+24+struct.unpack_from('<H',b,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',b,q+40*i) for i in range(struct.unpack_from('<H',b,pe+6)[0])]
def read(va,n):
    for _,_,rva,size,raw in sections:
        if rva<=va-base and va-base+n<=rva+size:return b[raw+va-base-rva:raw+va-base-rva+n]
    raise AssertionError(hex(va))
def rel(va,prefix,n):
    code=read(va,n);assert code.startswith(bytes.fromhex(prefix))
    return va+n+struct.unpack_from('<i',code,n-4)[0]
assert read(0x1403d7f8e,7).hex()=='4538a70d120000' # app+120d optional resume flag
assert rel(0x1403d7fbe,'e8',5)==0x14060eee0
assert read(0x1403d7fc3,7).hex()=='4188878b180000' # restored result written from AL
assert rel(0x1403d7fd0,'0f85',6)==0x1403d8232 # true skips new start parameters
assert read(0x1403d7ffc,7).hex()=='498b8720170000' # false uses app+1720 new parameters
assert read(0x14060eee0,13).hex()=='40555356574154415541564157'
assert rel(0x14060ef18,'488b35',7)==0x14118e2c8 # global normal Raid
assert b'persist.raid.json' in read(rel(0x14060ef43,'4c8d05',7),100)
assert b'persist.map.json' in read(rel(0x14060efa2,'4c8d05',7),100)
print('PASS fresh raid ABI: optional checkpoint restore boolean, false branch uses new start parameters, exact relocatable13-byte restore prefix, normal Raid and persist.raid/map reads; no game/save changes')
