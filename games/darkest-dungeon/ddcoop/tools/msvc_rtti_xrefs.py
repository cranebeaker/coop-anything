#!/usr/bin/env python3
"""List MSVC RTTI vtables and RIP-relative code references in a PE32+ executable."""
import re
import struct
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: msvc_rtti_xrefs.py Darkest.exe")
    data = open(sys.argv[1], "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE file")
    sections_n = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x20B:
        raise SystemExit("not PE32+")
    image_base = struct.unpack_from("<Q", data, opt + 24)[0]
    sec_off = opt + opt_size
    sections = {}
    for i in range(sections_n):
        p = sec_off + 40 * i
        name = data[p:p + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        vsize, va, raw_size, raw = struct.unpack_from("<IIII", data, p + 8)
        sections[name] = (va, max(vsize, raw_size), raw, raw_size)

    def rva_to_off(rva):
        for va, size, raw, raw_size in sections.values():
            if va <= rva < va + size and rva - va < raw_size:
                return raw + rva - va
        return None

    if ".text" not in sections or ".rdata" not in sections:
        raise SystemExit("missing .text or .rdata")
    text_va, text_size, text_raw, text_raw_size = sections[".text"]
    text = data[text_raw:text_raw + text_raw_size]
    data_sections = []
    for name, (va, _size, raw, raw_size) in sections.items():
        if name != ".text":
            data_sections.append((va, data[raw:raw + raw_size]))

    names = {}
    for section_va, blob in data_sections:
        for m in re.finditer(rb"\.\?AV[^\0]{2,240}@@", blob):
            name = m.group(0).decode("ascii", "replace")
            td_rva = section_va + m.start() - 16
            names[td_rva] = name

    cols = {}
    for section_va, blob in data_sections:
        for off in range(0, len(blob) - 24, 4):
            sig, _offset, _cd, td_rva, _chd, self_rva = struct.unpack_from("<IIIIII", blob, off)
            if sig == 1 and td_rva in names and self_rva == section_va + off:
                cols[section_va + off] = names[td_rva]

    print(f"RTTI type descriptors={len(names)}, complete-object-locators={len(cols)}", file=sys.stderr)

    vtables = {}
    for section_va, blob in data_sections:
        for off in range(8, len(blob) - 8, 8):
            col_va = struct.unpack_from("<Q", blob, off - 8)[0]
            name = cols.get(col_va - image_base)
            if not name:
                continue
            vt_va = image_base + section_va + off
            first = struct.unpack_from("<Q", blob, off)[0]
            if not (image_base + text_va <= first < image_base + text_va + text_size):
                continue
            vtables[vt_va] = (name, first)

    xrefs = {va: [] for va in vtables}
    for i in range(len(text) - 7):
        rex, op, modrm = text[i], text[i + 1], text[i + 2]
        if 0x48 <= rex <= 0x4F and op == 0x8D and (modrm & 0xC7) == 0x05:
            disp = struct.unpack_from("<i", text, i + 3)[0]
            target = image_base + text_va + i + 7 + disp
            if target in xrefs:
                xrefs[target].append(image_base + text_va + i)

    for vt_va, addresses in xrefs.items():
        if addresses:
            name, first = vtables[vt_va]
            print(f"{name} vftable=0x{vt_va:x} first=0x{first:x} xrefs=" + ",".join(f"0x{x:x}" for x in addresses))


if __name__ == "__main__":
    main()
