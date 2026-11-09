"""Write a .def that forwards every export of a DLL to a renamed copy of it.

Usage: python3 -I gen_forward_def.py <original.dll> <forward-module-name> <out.def> [local exports...]
Add --msvc to emit a C translation unit with linker export directives instead of a .def.
"""
import struct
import sys


def exports(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsize = struct.unpack_from("<H", d, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", d, opt)[0]
    ddir = opt + (112 if magic == 0x20B else 96)
    exp_rva = struct.unpack_from("<I", d, ddir)[0]
    secs = [struct.unpack_from("<8sIIII", d, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vs, va, rs, ro in secs:
            if va <= rva < va + max(vs, rs):
                return rva - va + ro
        raise ValueError(hex(rva))

    e = off(exp_rva)
    base, nfun, nnames, _funcs, names, ords = struct.unpack_from("<IIIIII", d, e + 16)
    out = []
    for i in range(nnames):
        n = off(struct.unpack_from("<I", d, off(names) + 4 * i)[0])
        o = struct.unpack_from("<H", d, off(ords) + 2 * i)[0] + base
        out.append((d[n:d.index(b"\0", n)].decode(), o))
    return out


def main():
    src, module, dst = sys.argv[1:4]
    local={arg for arg in sys.argv[4:] if arg!='--msvc'}
    msvc='--msvc' in sys.argv[4:]
    ex = exports(src)
    with open(dst, "w", newline="\n") as f:
        f.write('/* Generated SDL exports: keep every original name and ordinal. */\ntypedef int dd_bootstrap_export_unit;\n' if msvc else "LIBRARY SDL2.dll\nEXPORTS\n")
        for name, ordinal in ex:
            target='' if name in local else '=' + module + '.' + name
            f.write(f'#pragma comment(linker, "/export:{name}{target},@{ordinal}")\n' if msvc else f"    {name}{target} @{ordinal}\n")
    print(f"{len(ex)-len(local)} exports forwarded to {module}; {len(local)} local exports")


if __name__ == "__main__":
    main()
