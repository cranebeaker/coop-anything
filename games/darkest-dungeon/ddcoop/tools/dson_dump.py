#!/usr/bin/env python3
"""Dump a Darkest Dungeon binary save file (persist.*.json, magic 01 B1 00 00) as readable JSON.

The format has no value types; they are inferred from each value's size, as other save tools do.
Embedded save files (e.g. a hero's raw_data) are decoded recursively. Read-only.

    python tools/dson_dump.py <persist.roster.json> [-o out.json]
"""
import json
import struct
import sys

MAGIC = b"\x01\xb1\x00\x00"


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def _i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def decode(buf):
    if buf[:4] != MAGIC:
        raise ValueError("not a DSON file")
    n_meta1, meta1_off = _u32(buf, 0x14), _u32(buf, 0x18)
    n_meta2, meta2_off = _u32(buf, 0x2C), _u32(buf, 0x30)
    data_len, data_off = _u32(buf, 0x38), _u32(buf, 0x3C)
    data = buf[data_off:data_off + data_len]

    meta1 = [struct.unpack_from("<iiii", buf, meta1_off + 16 * i) for i in range(n_meta1)]
    fields = []
    for i in range(n_meta2):
        _hash, off, info = struct.unpack_from("<III", buf, meta2_off + 12 * i)
        name_len = (info >> 2) & 0x1FF
        fields.append({
            "off": off,
            "is_obj": bool(info & 1),
            "obj": (info >> 11) & 0xFFFFF,
            "name": data[off:off + name_len - 1].decode("utf-8", "replace"),
            "val": off + name_len,
        })
    # Value size = up to the next field in data order (fields are stored in data order).
    for i, f in enumerate(fields):
        end = fields[i + 1]["off"] if i + 1 < n_meta2 else data_len
        f["size"] = end - f["val"]

    def value(f):
        start, size = f["val"], f["size"]
        if size <= 0:
            return None
        if size == 1:
            return bool(data[start]) if data[start] in (0, 1) else chr(data[start])
        # Values of 4+ bytes are 4-aligned relative to the data block.
        pad = (-start) % 4
        if size > pad and size - pad >= 4:
            start, size = start + pad, size - pad
        raw = data[start:start + size]
        if size == 4:
            i = _i32(raw, 0)
            fl = struct.unpack_from("<f", raw, 0)[0]
            # Treat as float when it reads like an ordinary float and not like a small/hash int.
            if abs(i) > 0x00800000 and 1e-6 < abs(fl) < 1e7:
                return round(fl, 6)
            return i
        if size == 8 and _u32(raw, 0) not in (1,):
            a, b = _i32(raw, 0), _i32(raw, 4)
            return [a, b]
        n = _u32(raw, 0)
        if n + 4 == size or (n + 4 <= size and size - (n + 4) < 4):
            body = raw[4:4 + n]
            if body[:4] == MAGIC:
                return decode(body)
            if body.endswith(b"\x00"):
                try:
                    return body[:-1].decode("utf-8")
                except UnicodeDecodeError:
                    pass
        if 4 + 4 * n == size:
            return [_i32(raw, 4 + 4 * k) for k in range(n)]
        if 4 + 8 * n == size:
            return [[_i32(raw, 4 + 8 * k), _i32(raw, 8 + 8 * k)] for k in range(n)]
        return {"_raw": raw.hex()}

    def build(obj_index, field_index):
        _parent, _f, n_direct, n_all = meta1[obj_index]
        out = {}
        i = field_index + 1
        for _ in range(n_direct):
            f = fields[i]
            if f["is_obj"]:
                out[f["name"]] = build(f["obj"], i)
                i += 1 + meta1[f["obj"]][3]
            else:
                out[f["name"]] = value(f)
                i += 1
        return out

    root = fields[0]
    return {root["name"]: build(root["obj"], 0)}


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    with open(argv[1], "rb") as fh:
        tree = decode(fh.read())
    text = json.dumps(tree, indent=1, ensure_ascii=False)
    if "-o" in argv:
        with open(argv[argv.index("-o") + 1], "w", encoding="utf-8") as fh:
            fh.write(text)
    else:
        sys.stdout.write(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
