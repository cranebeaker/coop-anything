"""Samples EE RAM over PINE to show reads return real game memory.

Usage: python -I tools/pine_sample.py [port]
"""
import os
import socket
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_READ32, call  # noqa: E402


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 28011
    nonzero = 0
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        for addr in range(0x00100000, 0x00500000, 0x10000):
            word = struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", addr)))[0]
            nonzero += bool(word)
            if addr % 0x80000 == 0:
                print(f"0x{addr:08X}: 0x{word:08X}")
    print("non-zero words out of 64 sampled:", nonzero)


if __name__ == "__main__":
    main()
