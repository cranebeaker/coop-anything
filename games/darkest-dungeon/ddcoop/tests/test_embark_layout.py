"""Normal expedition completion callback, read-only exact-EXE ABI proof."""
from pathlib import Path
import runpy
import struct

abi = runpy.run_path(str(Path(__file__).with_name('test_quest_return_layout.py')))
read, relative, base = abi['read'], abi['relative'], abi['base']
vtable = 0x140ec3df0
col = struct.unpack('<Q', read(vtable - 8, 8))[0]
descriptor = base + struct.unpack('<I', read(col + 12, 4))[0]
name = read(descriptor + 16, 192).split(b'\0', 1)[0]
assert b'ProgressForwardInternal@TownDisplay@Screen@TownUI' in name
assert struct.unpack('<Q', read(vtable + 16, 8))[0] == 0x1406e7c70
# Stateless normal-mode callback; no sender-owned capture is dereferenced.
assert read(0x1406e7c70, 4).hex() == '4883ec38'
assert relative(0x1406e7c74, '488b05', 7) == 0x140dd5450
assert relative(0x1406e7c7b, '4c8d0d', 7) == 0x140e742d0
assert read(0x1406e7c82, 11).hex() == '41b8d61000004889442420'
assert relative(0x1406e7c8d, '488d15', 7) == 0x140e71770
assert read(0x1406e7c94, 2).hex() == 'c705'
assert 0x1406e7c9e + struct.unpack('<i', read(0x1406e7c96, 4))[0] == 0x142a6e664
assert struct.unpack('<I', read(0x1406e7c9a, 4))[0] == 0x14
assert relative(0x1406e7c9e, '488d0d', 7) == 0x140e74058
assert relative(0x1406e7ca5, 'e8', 5) == 0x140372410
assert read(0x1406e7caa, 5).hex() == '4883c438c3'
print('PASS embark ABI: normal TownDisplay completion RTTI/Do_call slot, stateless native state14 callback; no copied RIP-relative prologue')
