"""Read-only proof for the normal completed-quest seal, exact legitimate EXE."""
from pathlib import Path
import hashlib
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() == "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"
base = 0x140000000
pe = struct.unpack_from("<I", data, 0x3c)[0]
table = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
sections = [struct.unpack_from("<8sIIII", data, table + i * 40)
            for i in range(struct.unpack_from("<H", data, pe + 6)[0])]


def read(va, size):
    for _, _, rva, raw_size, raw in sections:
        if rva <= va - base and va - base + size <= rva + raw_size:
            at = raw + va - base - rva
            return data[at:at + size]
    raise AssertionError(hex(va))


def relative(va, prefix, size):
    code = read(va, size)
    assert code.startswith(bytes.fromhex(prefix))
    return va + size + struct.unpack_from("<i", code, size - 4)[0]


vtable = 0x140ecc4b8
col = struct.unpack("<Q", read(vtable - 8, 8))[0]
descriptor = base + struct.unpack("<I", read(col + 12, 4))[0]
name = read(descriptor + 16, 192).split(b"\0", 1)[0]
assert b"DrawQuestSeal@QuestInfo@Panel@RaidUI" in name
assert struct.unpack("<Q", read(vtable + 16, 8))[0] == 0x14074fdd0
# Copy/move constructs only the vptr: no captured display, panel, or sender object.
assert relative(0x14074fe10, "488d05", 7) == vtable
assert read(0x14074fe17, 7).hex() == "488902488bc2c3"
assert relative(0x14074f4a8, "488d05", 7) == vtable
assert read(0x14074f4af, 5).hex() == "4889442440"
# Only the native 'continue exploring' choice displays the completed-quest seal.
assert read(0x14074f301, 7).hex() == "83b94c01000002"
assert relative(0x14074f308, "0f85", 6) == 0x14074f58b
# Complete callback, no incoming RCX or captured data reads. Native state9 is
# scheduled, then the game's logger runs. The 4-byte prologue is followed by a
# RIP-relative load, so this callback must NOT use a blind copied trampoline.
assert read(0x14074fdd0, 4).hex() == "4883ec38"
assert relative(0x14074fdd4, "488b05", 7) == 0x140dd53f8
assert relative(0x14074fddb, "4c8d0d", 7) == 0x140e742d0
assert read(0x14074fde2, 11).hex() == "41b8d61000004889442420"
assert relative(0x14074fded, "488d15", 7) == 0x140e71770
assert read(0x14074fdf4, 2).hex() == "c705"
assert 0x14074fdfe + struct.unpack("<i", read(0x14074fdf6, 4))[0] == 0x142a6e664
assert struct.unpack("<I", read(0x14074fdfa, 4))[0] == 9
assert relative(0x14074fdfe, "488d0d", 7) == 0x140e74058
assert relative(0x14074fe05, "e8", 5) == 0x140372410
assert read(0x14074fe0a, 5).hex() == "4883c438c3"
print("PASS quest seal ABI: normal QuestInfo RTTI, exact vtable slot, stateless callback, continue-exploring guard, native results state9; no unsafe inline trampoline")
