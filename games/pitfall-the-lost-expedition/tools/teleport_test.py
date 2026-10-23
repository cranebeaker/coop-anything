"""Tests which candidate position address is authoritative by nudging X and watching copies.

Usage: python -I tools/teleport_test.py <slot> <addr> [<addr> ...]

For each candidate: load <slot>, add 20 to the float at addr, wait 1.5 s, then read every
candidate. If the nudge spread to the other copies and stuck at addr, addr is the source.
"""
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_LOAD_STATE, MSG_READ32, call  # noqa: E402

MSG_WRITE32 = 0x06


def read_f(sock, addr):
    return struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", addr)))[0]


def write_f(sock, addr, value):
    call(sock, MSG_WRITE32, struct.pack("<If", addr, value))


def main():
    slot = int(sys.argv[1])
    addrs = [int(a, 0) for a in sys.argv[2:]]
    with socket.create_connection(("127.0.0.1", 28011), timeout=10) as sock:
        for target in addrs:
            call(sock, MSG_LOAD_STATE, bytes([slot]))
            time.sleep(2)
            before = {a: read_f(sock, a) for a in addrs}
            write_f(sock, target, before[target] + 20.0)
            time.sleep(1.5)
            after = {a: read_f(sock, a) for a in addrs}
            moved = [a for a in addrs if abs(after[a] - before[a]) > 10]
            print(f"nudge 0x{target:08X}: {before[target]:.3f} -> now {after[target]:.3f}; "
                  f"{len(moved)}/{len(addrs)} candidates shifted >10: "
                  + " ".join(f"0x{a:08X}" for a in moved))


if __name__ == "__main__":
    main()
