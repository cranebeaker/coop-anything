"""Waits for the level loader to reach a state, then saves a savestate slot over PINE.

Usage: python -I tools/catch_load.py <state> <slot> [timeout_s] [port]

Polls the loader state word (0x0052F1F4 = level state object 0x0052F190 + 0x64) every 20 ms.
"""
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_READ32, MSG_SAVE_STATE, call  # noqa: E402

LOADER_STATE = 0x0052F1F4


def main():
    want, slot = int(sys.argv[1]), int(sys.argv[2])
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 180
    port = int(sys.argv[4]) if len(sys.argv) > 4 else 28011
    start = time.time()
    seen = []
    with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
        while time.time() - start < timeout:
            state = struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", LOADER_STATE)))[0]
            if not seen or seen[-1] != state:
                seen.append(state)
            if state == want:
                call(sock, MSG_SAVE_STATE, bytes([slot]))
                print(f"saved slot {slot} at state {state} after {time.time() - start:.1f}s; states seen {seen}")
                return
            time.sleep(0.02)
    print(f"timeout; states seen {seen}")


if __name__ == "__main__":
    main()
