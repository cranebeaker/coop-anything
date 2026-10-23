"""Looks for a "reads the controller" switch in the second player object.

Usage: python -I tools/input_flag_scan.py <pcsx2_pid> <slot> <state.p2s for the same slot> [port]

For every u32 field of the second player (address in mailbox +4) that is 1 in the savestate:
load the slot, write 0 to the field, hold left for 1 s, and compare how far each player moved.
A hit: the real player walks (> 5 units) while the second player stays within 0.3 units and
keeps a sane height.
"""
import math
import os
import socket
import struct
import subprocess
import sys
import time
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pine_check import MSG_LOAD_STATE, MSG_READ32, call  # noqa: E402

MSG_WRITE32 = 0x06
PLAYER_PTR = 0x0052F198
MAILBOX = 0x000EFF00
PLAYER_SIZE = 0x1B30


def main():
    pid, slot, state_path = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    port = int(sys.argv[4]) if len(sys.argv) > 4 else 28011
    with zipfile.ZipFile(state_path) as z:
        mem = z.read("eeMemory.bin")
    u = lambda a: struct.unpack_from("<I", mem, a)[0]
    second = u(MAILBOX + 4)
    fields = [off for off in range(4, PLAYER_SIZE, 4) if u(second + off) == 1]
    print(f"second player 0x{second:08X}: {len(fields)} fields equal 1")

    sock = socket.create_connection(("127.0.0.1", port), timeout=10)
    r32 = lambda a: struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    rf = lambda a: struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    vec = lambda obj: tuple(rf(obj + 0x340 + 4 * i) for i in range(3))

    def trial(off):
        call(sock, MSG_LOAD_STATE, bytes([slot]))
        time.sleep(1.0)
        first = r32(PLAYER_PTR)
        if off is not None:
            call(sock, MSG_WRITE32, struct.pack("<II", second + off, 0))
        time.sleep(0.3)
        p1, p2 = vec(first), vec(second)
        subprocess.run([sys.executable, "-I", os.path.join(HERE, "pcsx2_keys.py"), str(pid),
                        "a:1000", "wait:300"], check=True)
        q1, q2 = vec(first), vec(second)
        return math.dist(p1, q1), math.dist(p2, q2), q2

    d1, d2, _ = trial(None)
    print(f"baseline: real moved {d1:.2f}, second moved {d2:.2f}")
    for off in fields:
        d1, d2, q2 = trial(off)
        hit = d1 > 5 and d2 < 0.3 and all(math.isfinite(c) for c in q2) and q2[2] > -50
        print(f"+0x{off:04X}=0: real {d1:6.2f}  second {d2:6.2f}{'   <-- HIT' if hit else ''}", flush=True)


if __name__ == "__main__":
    main()
