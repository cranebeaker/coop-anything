"""Lists zero runs of 4 KiB or more in 0x00080000-0x000FFFFF of savestates 01/07/12 (code-cave search).

Usage: python -I cave_check.py <savestate prefix without .NN.p2s> <dir containing pine_check.py>
"""
import struct, sys, zipfile, socket, os
sys.path.insert(0, sys.argv[2])
from pine_check import call, MSG_READ32
lo, hi = 0x00080000, 0x00100000
def zero_runs(m):
    runs, start = [], None
    for a in range(lo, hi, 4):
        z = struct.unpack_from("<I", m, a)[0] == 0
        if z and start is None: start = a
        if not z and start is not None: runs.append((start, a)); start = None
    if start is not None: runs.append((start, hi))
    return [r for r in runs if r[1] - r[0] >= 0x1000]
for s in ("01", "07", "12"):
    with zipfile.ZipFile(f"{sys.argv[1]}.{s}.p2s") as z: m = z.read("eeMemory.bin")
    print(s, [(hex(a), hex(b - a)) for a, b in zero_runs(m)][:6])
