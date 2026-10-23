"""Minimal PINE client: checks that a PCSX2 instance answers on its PINE port.

Usage: python -I tools/pine_check.py [port]

Wire format (PCSX2 PINE): request = u32 total size (LE, includes itself) + u8 opcode + args.
Reply = u32 total size + u8 result (0 = OK) + payload. Strings are u32 length + bytes.
"""
import socket
import struct
import sys

MSG_READ32 = 0x02
MSG_VERSION = 0x08
MSG_SAVE_STATE = 0x09
MSG_LOAD_STATE = 0x0A
MSG_TITLE = 0x0B
MSG_ID = 0x0C
MSG_UUID = 0x0D
MSG_GAME_VERSION = 0x0E
MSG_STATUS = 0x0F

STATUS_NAMES = {0: "running", 1: "paused", 2: "shutdown"}


def call(sock, opcode, args=b""):
    body = bytes([opcode]) + args
    sock.sendall(struct.pack("<I", len(body) + 4) + body)
    header = recv_exact(sock, 4)
    size = struct.unpack("<I", header)[0]
    payload = recv_exact(sock, size - 4)
    if payload[0] != 0:
        raise RuntimeError(f"opcode 0x{opcode:02X} failed (result {payload[0]})")
    return payload[1:]


def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("PINE connection closed")
        data += chunk
    return data


def read_string(payload):
    length = struct.unpack("<I", payload[:4])[0]
    return payload[4:4 + length].rstrip(b"\0").decode("utf-8", "replace")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 28011
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        print("version:", read_string(call(sock, MSG_VERSION)))
        status = struct.unpack("<I", call(sock, MSG_STATUS))[0]
        print("status: ", STATUS_NAMES.get(status, status))
        if status == 2:
            return
        print("title:  ", read_string(call(sock, MSG_TITLE)))
        print("serial: ", read_string(call(sock, MSG_ID)))
        print("crc:    ", read_string(call(sock, MSG_UUID)))
        print("gamever:", read_string(call(sock, MSG_GAME_VERSION)))
        # EE RAM starts at 0x00100000 for game code; read a word to prove memory access.
        word = struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", 0x00100000)))[0]
        print(f"read32 @0x00100000: 0x{word:08X}")


if __name__ == "__main__":
    main()
