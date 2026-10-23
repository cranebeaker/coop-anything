"""Tries cumulative "make input slot 1 look connected" writes until the puppet walks on a fake stick.

Usage: python -I tools/puppet_enable_scan.py <slot> [port]

Each step: load the savestate slot, apply all writes up to and including this step (writes are
repeated at 30 Hz while the stick is pushed), push slot 1's stick left for 1.5 s, then report how
far the puppet moved.
"""
import math
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_LOAD_STATE, MSG_READ32, call  # noqa: E402
from puppet_stick import SLOT1_STICK, stick_words  # noqa: E402

MSG_WRITE32 = 0x06
PUPPET_PTR = 0x000EFF04
SLOT1_OBJ_TABLE = 0x0055EC84
PORT2_CTRL = 0x005776C8

STEPS = [
    ("slot1 ch1 +0x10/+0x14 = FFFFFFFF", lambda r: [(SLOT1_STICK + 0x10, 0xFFFFFFFF), (SLOT1_STICK + 0x14, 0xFFFFFFFF)]),
    ("slot1 object +0x00 = 0, +0x08 = 1", lambda r: [(r(SLOT1_OBJ_TABLE) + 0x00, 0), (r(SLOT1_OBJ_TABLE) + 0x08, 1)]),
    ("port2 controller +0x00 = 0, +0x04 = 7", lambda r: [(PORT2_CTRL + 0x00, 0), (PORT2_CTRL + 0x04, 7)]),
]


def main():
    slot = int(sys.argv[1])
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 28011
    sock = socket.create_connection(("127.0.0.1", port), timeout=5)
    r = lambda a: struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    rf = lambda a: struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    w = lambda a, v: call(sock, MSG_WRITE32, struct.pack("<II", a, v))
    for n in range(1, len(STEPS) + 1):
        call(sock, MSG_LOAD_STATE, bytes([slot]))
        time.sleep(1.0)
        puppet = r(PUPPET_PTR)
        before = [rf(puppet + 0x340 + 4 * i) for i in range(3)]
        writes = [wv for _, fn in STEPS[:n] for wv in fn(r)]
        end = time.time() + 1.5
        while time.time() < end:
            for a, v in writes:
                w(a, v)
            for a, data in stick_words(-1.0, 0.0):
                call(sock, MSG_WRITE32, struct.pack("<I", a) + data)
            time.sleep(1 / 30)
        after = [rf(puppet + 0x340 + 4 * i) for i in range(3)]
        print(f"steps 1-{n} (+ {STEPS[n - 1][0]}): puppet moved {math.dist(before, after):.2f}", flush=True)


if __name__ == "__main__":
    main()
