"""Finds which position copy of an object moves it, by writing a target xyz into each copy.

Usage: python -I tools/move_test.py <slot> <obj> <dx> <dy> <dz> <off> [<off> ...] [--keep]

For each offset: load <slot>, write the player's position plus (dx, dy, dz) into obj+off,
wait 1.5 s, then report whether the write stuck and whether the drawn translation (+0x190)
followed. With --keep the last offset is written and the state is left as is (for a screenshot).
"""
import math
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_LOAD_STATE, MSG_READ32, call  # noqa: E402

MSG_WRITE32 = 0x06
PLAYER_PTR = 0x0052F198


def u32(sock, addr):
    return struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", addr)))[0]


def vec(sock, addr):
    return [struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", addr + 4 * i)))[0]
            for i in range(3)]


def write_vec(sock, addr, v):
    for i, c in enumerate(v):
        call(sock, MSG_WRITE32, struct.pack("<If", addr + 4 * i, c))


def main():
    args = [a for a in sys.argv[1:] if a != "--keep"]
    keep = "--keep" in sys.argv
    slot, obj = int(args[0]), int(args[1], 16)
    delta = [float(a) for a in args[2:5]]
    offsets = [int(a, 16) for a in args[5:]]
    with socket.create_connection(("127.0.0.1", 28011), timeout=10) as sock:
        for off in offsets:
            call(sock, MSG_LOAD_STATE, bytes([slot]))
            time.sleep(2)
            player = u32(sock, PLAYER_PTR)
            target = [p + d for p, d in zip(vec(sock, player + 0x340), delta)]
            write_vec(sock, obj + off, target)
            time.sleep(1.5)
            stuck = math.dist(vec(sock, obj + off), target) < 0.5
            drawn = vec(sock, obj + 0x190)
            follows = math.dist(drawn, target) < 0.5
            print(f"+0x{off:X}: stuck={stuck} drawn_follows={follows} drawn="
                  + " ".join(f"{c:.2f}" for c in drawn))
            if keep and off == offsets[-1]:
                return


if __name__ == "__main__":
    main()
