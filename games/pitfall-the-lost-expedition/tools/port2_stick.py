"""Writes a left-stick state into controller port 2's object (the puppet's input source) over PINE.

Usage: python -I tools/port2_stick.py <x> <y> <seconds> [port] [--no-connect]

Offsets are where port 1's controller object (0x005763A8) holds the left stick (found by
savestate diff with the stick held): float x/y pairs at +0x5C, +0x70, +0x84, +0xE8, +0x33C,
+0x34C; per-axis "centred" flags at +0x37C / +0x380; raw analog bytes [rx, ry, lx, ly] at +0x3DC
(0x7F = centre). Port 2's object is 0x005776C8. Unless --no-connect, also writes +0x00 = 0 and
+0x04 = 7 (port 1's values; port 2 holds 4 / 0 with no pad). Repeats at 30 Hz.
"""
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import call  # noqa: E402

MSG_WRITE32 = 0x06
PORT2 = 0x005776C8
STICK_PAIRS = (0x5C, 0x70, 0x84, 0xE8, 0x33C, 0x34C)


def port2_words(x, y, connect=True):
    words = []
    for off in STICK_PAIRS:
        words += [(PORT2 + off, struct.pack("<f", x)), (PORT2 + off + 4, struct.pack("<f", y))]
    words += [(PORT2 + 0x37C, struct.pack("<I", int(abs(x) < 0.2))),
              (PORT2 + 0x380, struct.pack("<I", int(abs(y) < 0.2)))]
    lx = max(0, min(255, round(0x7F + x * 0x7F)))
    ly = max(0, min(255, round(0x7F - y * 0x7F)))
    words.append((PORT2 + 0x3DC, bytes([0x7F, 0x7F, lx, ly])))
    if connect:
        words += [(PORT2 + 0x00, struct.pack("<I", 0)), (PORT2 + 0x04, struct.pack("<I", 7))]
    return words


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    x, y, seconds = float(args[0]), float(args[1]), float(args[2])
    port = int(args[3]) if len(args) > 3 else 28011
    words = port2_words(x, y, "--no-connect" not in sys.argv)
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        end = time.time() + seconds
        while True:
            for addr, data in words:
                call(sock, MSG_WRITE32, struct.pack("<I", addr) + data)
            if time.time() >= end:
                break
            time.sleep(1 / 30)


if __name__ == "__main__":
    main()
