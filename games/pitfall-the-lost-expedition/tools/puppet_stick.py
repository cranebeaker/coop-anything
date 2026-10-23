"""Pushes the puppet's left stick (player slot 1, channel 1) over PINE for a while.

Usage: python -I tools/puppet_stick.py <x> <y> <seconds> [port] [--once]

The puppet's +0x7A0 points at slot 1 object + 0x10, whose buffer is 0x00579E88 (the real
player's points at slot 0 object + 0x20, buffer 0x00578C60). Stick floats at +0x228 / +0x22C
and a copy at +0x238 / +0x23C; per-axis "centred" flags at +0x268 / +0x26C (1 = at rest).
Writes at 30 Hz unless --once; prints what the buffer holds afterwards.
"""
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_READ32, call  # noqa: E402

MSG_WRITE32 = 0x06
SLOT1_STICK = 0x00579E88  # buffer behind the puppet's +0x7A0 entry (slot 1 object + 0x10)


def stick_words(x, y):
    return [(SLOT1_STICK + 0x228, struct.pack("<f", x)), (SLOT1_STICK + 0x22C, struct.pack("<f", y)),
            (SLOT1_STICK + 0x238, struct.pack("<f", x)), (SLOT1_STICK + 0x23C, struct.pack("<f", y)),
            (SLOT1_STICK + 0x268, struct.pack("<I", int(abs(x) < 0.2))),
            (SLOT1_STICK + 0x26C, struct.pack("<I", int(abs(y) < 0.2)))]


def write_stick(sock, x, y):
    for addr, data in stick_words(x, y):
        call(sock, MSG_WRITE32, struct.pack("<I", addr) + data)


def main():
    args = [a for a in sys.argv[1:] if a != "--once"]
    x, y, seconds = float(args[0]), float(args[1]), float(args[2])
    port = int(args[3]) if len(args) > 3 else 28011
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        end = time.time() + seconds
        write_stick(sock, x, y)
        while "--once" not in sys.argv and time.time() < end:
            time.sleep(1 / 30)
            write_stick(sock, x, y)
        if "--once" in sys.argv:
            time.sleep(seconds)
        held = [struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", SLOT1_STICK + o)))[0]
                for o in (0x228, 0x22C)]
        print(f"after: stick x {held[0]:.3f} y {held[1]:.3f}")


if __name__ == "__main__":
    main()

