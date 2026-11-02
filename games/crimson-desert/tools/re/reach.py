# Usage: reach.py <exe> <root rva hex> <depth> <target rva hex | @file of lo-hi ranges> : BFS over DIRECT calls/jumps (.pdata-bounded functions)
# from root; prints every path that reaches a function calling <target> (direct only; virtual calls are not followed).
import sys, struct, bisect, pefile, capstone
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
d = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]; raw = pe.get_data(d.VirtualAddress, d.Size)
fns = sorted(struct.unpack_from("<II", raw, i)[:2] for i in range(0, len(raw) - 11, 12)); starts = [f[0] for f in fns]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
def bounds(r):
    j = bisect.bisect_right(starts, r) - 1
    return fns[j] if j >= 0 and fns[j][0] <= r < fns[j][1] else (r, r + 0x400)
cache = {}
def callees(f):
    if f in cache: return cache[f]
    b, e = bounds(f); out = set()
    try: code = pe.get_data(b, e - b)
    except pefile.PEFormatError: cache[f] = out; return out  # outside the file (e.g. a jump into unpacked code)
    for ins in md.disasm(code, base + b):
        if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
            t = int(ins.op_str, 16) - base
            if not (b <= t < e): out.add(t)
    cache[f] = out; return out
root, depth = int(sys.argv[2], 16), int(sys.argv[3])
# <target>: one rva, or @file of lines "lo-hi" (function ranges): a hit = calling into any of them.
if sys.argv[4].startswith("@"):
    rng = [tuple(int(x, 16) for x in l.split("-")) for l in open(sys.argv[4][1:]) if "-" in l]
    targets = {lo for lo, hi in rng}
else:
    targets = {int(sys.argv[4], 16)}
target = None
prev = {root: None}; frontier = [root]; hits = []
for lvl in range(depth):
    nxt = []
    for f in frontier:
        cs = callees(f)
        hit = cs & targets
        if hit: hits.append((f, sorted(hit)))
        for c in cs:
            if c not in prev and c not in targets: prev[c] = f; nxt.append(c)
    frontier = nxt
print(f"; {len(prev)} functions reached in {depth} levels")
for h, t in hits:
    p = []; x = h
    while x is not None: p.append(f"{x:#x}"); x = prev[x]
    print("calls " + ",".join(f"{v:#x}" for v in t) + ": " + " <- ".join(p))
