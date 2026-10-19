"""Pure Python, no dependencies. Find code references to strings in a PE32 (no ASLR) by scanning .text for the string VA as imm32.
Usage: python tools/pe_string_xrefs.py <exe> <needle> [<needle> ...]
"""
import re, struct, sys

def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    optsz = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    out = []
    for i in range(nsec):
        o = pe + 24 + optsz + i * 40
        name = data[o:o+8].rstrip(b'\0').decode()
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, o + 8)
        out.append((name, va, vsz, raw, rsz))
    return base, out

def off2va(base, secs, off):
    for n, va, vsz, raw, rsz in secs:
        if raw <= off < raw + rsz:
            return base + va + off - raw, n
    return None, None

def va2off(base, secs, v):
    for n, va, vsz, raw, rsz in secs:
        if base + va <= v < base + va + rsz:
            return raw + v - base - va
    return None

def find_func_start(data, off, text_raw):
    # walk back to a likely prologue (CC/C3 padding followed by code), max 0x2000 bytes
    for k in range(off, max(text_raw, off - 0x2000), -1):
        if data[k-1] in (0xCC, 0xC3) and data[k:k+3] in (b'\x55\x8b\xec',) or (data[k-1] == 0xCC and data[k] != 0xCC):
            return k
    return None

def main():
    data = open(sys.argv[1], 'rb').read()
    base, secs = sections(data)
    text = next(s for s in secs if s[0] == '.text')
    t0, t1 = text[3], text[3] + text[4]
    for needle in sys.argv[2:]:
        nb = needle.encode()
        print(f'=== {needle}')
        for m in re.finditer(re.escape(nb), data):
            # expand to the full NUL-terminated string
            s = data.rfind(b'\0', 0, m.start()) + 1
            e = data.find(b'\0', m.end())
            sva, sec = off2va(base, secs, s)
            if sva is None:
                continue
            full = data[s:e].decode('latin-1')
            imm = struct.pack('<I', sva)
            refs = [r.start() for r in re.finditer(re.escape(imm), data[t0:t1])]
            print(f'  str {sec}:{sva:#x} "{full[:110]}"  refs={len(refs)}')
            for r in refs[:12]:
                ro = t0 + r
                rva, _ = off2va(base, secs, ro)
                op = data[ro-1]
                op2 = data[ro-2:ro]
                f = find_func_start(data, ro, t0)
                fva = off2va(base, secs, f)[0] if f else 0
                print(f'    ref @ {rva:#x} (opcode byte before: {op:02x}, {op2.hex()}) func~{fva:#x}')

if __name__ == '__main__':
    main()
