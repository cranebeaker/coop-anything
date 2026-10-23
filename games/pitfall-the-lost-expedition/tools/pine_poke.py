"""Reads or writes one 32-bit word in a running PCSX2 over PINE.

Usage: python -I tools/pine_poke.py read <addr> [port]
       python -I tools/pine_poke.py write <addr> <value> [port]
       python -I tools/pine_poke.py readf <addr> [count] [port]   (count floats from addr)
Addresses and values accept 0x prefixes.
"""
import os
import socket
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_READ32, call  # noqa: E402

MSG_WRITE32 = 0x06


def main():
    action, addr = sys.argv[1], int(sys.argv[2], 0)
    with socket.create_connection(("127.0.0.1", 28011), timeout=5) as sock:
        if action == "write":
            value = int(sys.argv[3], 0)
            call(sock, MSG_WRITE32, struct.pack("<II", addr, value))
            print(f"wrote 0x{value:08X} to 0x{addr:08X}")
        elif action == "read":
            word = struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", addr)))[0]
            print(f"0x{addr:08X}: 0x{word:08X} ({word})")
        elif action == "readf":
            count = int(sys.argv[3]) if len(sys.argv) > 3 else 3
            values = []
            for i in range(count):
                raw = call(sock, MSG_READ32, struct.pack("<I", addr + 4 * i))
                values.append(struct.unpack("<f", raw)[0])
            print(f"0x{addr:08X}: " + " ".join(f"{v:.3f}" for v in values))


if __name__ == "__main__":
    main()
