"""Saves or loads a PCSX2 savestate slot over PINE.

Usage: python -I tools/pine_state.py save|load <slot> [port]
"""
import os
import socket
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_LOAD_STATE, MSG_SAVE_STATE, call  # noqa: E402


def main():
    action, slot = sys.argv[1], int(sys.argv[2])
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 28011
    opcode = {"save": MSG_SAVE_STATE, "load": MSG_LOAD_STATE}[action]
    with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
        call(sock, opcode, bytes([slot]))
    print(f"{action} slot {slot}: ok")


if __name__ == "__main__":
    main()
