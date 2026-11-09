"""Read-only ABI evidence from the exact legitimate build. Never runs game code."""
from pathlib import Path
import hashlib
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() == "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"
pe = struct.unpack_from("<I", data, 0x3c)[0]
opt = pe + 24
table = opt + struct.unpack_from("<H", data, pe + 20)[0]
sections = [struct.unpack_from("<8sIIII", data, table + i * 40)
            for i in range(struct.unpack_from("<H", data, pe + 6)[0])]


def read(va, size):
    rva = va - 0x140000000
    for _, vsize, address, raw_size, raw in sections:
        if address <= rva and rva + size <= address + raw_size:
            at = raw + rva - address
            return data[at:at + size]
    raise AssertionError(hex(va))


def relative(va, prefix, size):
    code = read(va, size)
    assert code.startswith(bytes.fromhex(prefix))
    return va + size + struct.unpack_from("<i", code, size - 4)[0]


# Complete, non-RIP-relative prologue instructions, identical lengths to the hooks.
assert read(0x140712080, 14).hex() == "488bc44889581855565741564157"
assert read(0x140713bf0, 13).hex() == "48895c2420574881ecc0000000"
assert read(0x140712800, 15).hex() == "488bc4555356574154415541564157"
# RTTI-derived std::_Func vtables: Do_call at +0x10.
assert struct.unpack("<Q", read(0x140ec8890 + 0x10, 8))[0] == 0x140713bf0
assert struct.unpack("<Q", read(0x140ec8498 + 0x10, 8))[0] == 0x140713b80
assert read(0x140713b80, 4).hex() == "4883c108"  # skip std::_Func vptr
assert relative(0x140713b84, "e9", 5) == 0x140712800
# Both lambdas capture an overlay pointer followed by food as float, NOT a
# RaidDisplay pointer. The refusal thunk skips its std::_Func vptr first.
assert read(0x1407123bd, 5).hex() == "48897c2458"
assert read(0x1407123c2, 6).hex() == "f30f117c2460"
assert read(0x1407125e2, 5).hex() == "48897c2458"
assert read(0x1407125e7, 6).hex() == "f30f117c2460"
# Eating uses the full std::_Func: overlay +8, float food cost +0x10.
assert read(0x140713c00, 4).hex() == "488b5108"
assert read(0x140713cd1, 5).hex() == "f30f2c5710"
assert relative(0x140713cd6, "8b05", 6) == 0x142a87b08  # native food type
assert relative(0x140713d11, "e8", 5) == 0x1405e15d0  # native inventory removal
# ShowHunger sums each hero's modifier, permits food >= cost, state2 is open.
show = read(0x140712080, 0x77c)
assert bytes.fromhex("f30f1088c40b0000") in show
assert bytes.fromhex("4881c2a8050000") in show  # native inventory Item stride
assert read(0x1407120bb, 6).hex() == "41bf02000000"
# Hunger dispatch only takes the no-prop branch, distinct from a curio in this overlay.
assert read(0x14070de43, 7).hex() == "488b97700d0000"
assert relative(0x14070e050, "e8", 5) == 0x140712080
print("PASS hunger ABI: native callback vtables/captures, complete hook prologues, food modifier/type/inventory and normal ShowHunger dispatch")
