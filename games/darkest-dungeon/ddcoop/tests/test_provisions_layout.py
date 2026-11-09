"""Static constructor-chain check against the legitimate EXACT executable. Never executes game code.

The isolated model must reproduce both this-adjustments, rather than just copying an offset from
our reader. This catches the test14 error: +1fc8 was relative to TownDisplay's +17a8 subobject.
Usage: python tests/test_provisions_layout.py <path to supported Darkest.exe>
"""
from pathlib import Path
import hashlib
import struct
import sys

EXPECTED = "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"


def verify(path):
    data = Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == EXPECTED, "Unsupported executable"
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    table = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
    sections = [struct.unpack_from("<8sIIII", data, table + 40 * i) for i in range(count)]

    def read(rva, length):
        for _, _, va, size, raw in sections:
            if va <= rva and rva + length <= va + size:
                return data[raw + rva - va:raw + rva - va + length]
        raise AssertionError("RVA outside file sections")

    def i32(rva):
        return struct.unpack("<i", read(rva, 4))[0]

    # TownDisplay ctor: mov r14,rcx; sets its primary vtable.
    assert read(0x6da6f9, 3) == bytes.fromhex("4c8bf1")
    assert read(0x6da702, 3) == bytes.fromhex("488d05")
    assert 0x6da709 + i32(0x6da705) == 0xec37a8
    assert read(0x6da709, 3) == bytes.fromhex("498906")
    # lea rcx,[r14+17a8]; call 6dad90; helper preserves adjusted this in rsi.
    assert read(0x6da730, 3) == bytes.fromhex("498d8e")
    outer = i32(0x6da733)
    assert read(0x6da737, 1) == b"\xe8"
    assert 0x6da73c + i32(0x6da738) == 0x6dad90
    assert read(0x6dadab, 3) == bytes.fromhex("488bf1")
    # lea rdx,[rsi+1fc8]; set its ProvisionSelect vtable at [rdx].
    assert read(0x6daf04, 3) == bytes.fromhex("488d96")
    inner = i32(0x6daf07)
    assert read(0x6daf54, 3) == bytes.fromhex("488d05")
    assert 0x6daf5b + i32(0x6daf57) == 0xebdde8
    assert read(0x6daf5b, 3) == bytes.fromhex("488902")
    assert (outer, inner, outer + inner) == (0x17a8, 0x1fc8, 0x3770)
    print("PASS native constructor chain: TownDisplay+17a8, subobject+1fc8 => ProvisionSelect+3770")
    # Normal supply purchase compares the full pair, not a nonzero-ID sentinel.
    assert read(0x6961cd, 7) == bytes.fromhex("448b8b88000000")  # item.id -> r9d
    assert read(0x6961d4, 4) == bytes.fromhex("448b5344")        # item.type -> r10d
    assert read(0x6961f3, 4) == bytes.fromhex("44395040")        # definition.type == r10d
    assert read(0x6961f9, 7) == bytes.fromhex("44398884000000")  # definition.id == r9d (zero included)
    print("PASS native provision item identity: type+44/id+88 compared with definition type+40/id+84")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tests/test_provisions_layout.py <Darkest.exe>")
    verify(sys.argv[1])
