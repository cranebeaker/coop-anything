"""Read-only live logger for Pitfall: The Lost Expedition (PS2 NTSC-U SLUS-20408) over PINE.

Usage: python -I ptle_logger.py [--port 28011 [--port 28012 ...]] [--hz 20] [--seconds 0] [--csv out.csv]
       [--levels world_infos.json]

Polls the player pointer of each PCSX2 instance and prints level, position, facing and health,
plus the distance between the first two players. Never writes guest memory. --seconds 0 runs
until Ctrl+C. Addresses are from games/pitfall-the-lost-expedition/NOTES.md.
"""
import argparse
import csv
import math
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine_check import MSG_ID, MSG_READ32, MSG_UUID, call, read_string  # noqa: E402

EXPECTED_SERIAL = "SLUS-20408"
EXPECTED_CRC = "394f402e"

PLAYER_PTR = 0x0052F198
LEVEL_CRC = 0x0052F200
POS = 0x340
YAW = 0x35C  # radians, offset by pi from the drawn rotation matrix at +0x160
HEALTH_MAX = 0x684  # derived, not proven
HEALTH = 0x688

EE_RAM_END = 0x02000000


class Pine:
    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=5)

    def u32(self, addr):
        return struct.unpack("<I", call(self.sock, MSG_READ32, struct.pack("<I", addr)))[0]

    def f32(self, addr):
        return struct.unpack("<f", call(self.sock, MSG_READ32, struct.pack("<I", addr)))[0]

    def string(self, opcode):
        return read_string(call(self.sock, opcode))


def load_level_names(path):
    if not path:
        return {}
    try:
        import json
        with open(path, encoding="utf-8") as f:
            infos = json.load(f)
    except OSError:
        return {}
    names = {}
    for region, areas in infos.items():
        if region == "//":
            continue
        for area in areas:
            names[int(area["asset_crc32"].replace(" ", ""), 16)] = area["area_name"]
    return names


def sample(pine):
    level = pine.u32(LEVEL_CRC)
    player = pine.u32(PLAYER_PTR)
    if not 0x00100000 <= player < EE_RAM_END:
        return level, None
    x, y, z = (pine.f32(player + POS + 4 * i) for i in range(3))
    yaw = pine.f32(player + YAW)
    facing = math.remainder(yaw - math.pi, math.tau)
    return level, (player, x, y, z, facing, pine.u32(player + HEALTH), pine.u32(player + HEALTH_MAX))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, action="append")
    parser.add_argument("--hz", type=float, default=20)
    parser.add_argument("--seconds", type=float, default=0)
    parser.add_argument("--csv")
    parser.add_argument("--levels", help="ptle-tools world_infos.json, for level names")
    args = parser.parse_args()
    ports = args.port or [28011]

    pines = []
    for port in ports:
        pine = Pine(port)
        serial, crc = pine.string(MSG_ID), pine.string(MSG_UUID).lower()
        if (serial, crc) != (EXPECTED_SERIAL, EXPECTED_CRC):
            sys.exit(f"port {port}: wrong game {serial} {crc}, addresses are for "
                     f"{EXPECTED_SERIAL} {EXPECTED_CRC}")
        pines.append(pine)
    names = load_level_names(args.levels)
    writer = None
    if args.csv:
        out = open(args.csv, "w", newline="", encoding="utf-8")
        writer = csv.writer(out)
        writer.writerow(["t", "port", "level_crc", "level", "player", "x", "y", "z", "facing",
                         "health", "health_max"])

    period = 1.0 / args.hz
    start = time.perf_counter()
    count = 0
    try:
        while not args.seconds or time.perf_counter() - start < args.seconds:
            t = time.perf_counter() - start
            parts, positions = [], []
            for port, pine in zip(ports, pines):
                level, player = sample(pine)
                level_name = names.get(level, f"0x{level:08X}")
                if player is None:
                    parts.append(f"[{port}] {level_name:12.12} no player")
                    row = [f"{t:.3f}", port, f"{level:08X}", level_name] + [""] * 7
                    positions.append(None)
                else:
                    obj, x, y, z, facing, hp, hp_max = player
                    parts.append(f"[{port}] {level_name:12.12} {x:8.2f} {y:8.2f} {z:7.2f} "
                                 f"{math.degrees(facing):6.0f}deg hp {hp}/{hp_max}")
                    row = [f"{t:.3f}", port, f"{level:08X}", level_name, f"{obj:08X}", f"{x:.4f}",
                           f"{y:.4f}", f"{z:.4f}", f"{facing:.4f}", hp, hp_max]
                    positions.append((x, y, z))
                if writer:
                    writer.writerow(row)
            line = f"{t:7.2f}s  " + "  ".join(parts)
            if len(positions) >= 2 and None not in positions[:2]:
                line += f"  dist {math.dist(positions[0], positions[1]):7.2f}"
            print(line, flush=True)
            count += 1
            time.sleep(max(0.0, start + count * period - time.perf_counter()))
    except KeyboardInterrupt:
        pass
    elapsed = time.perf_counter() - start
    print(f"{count} samples in {elapsed:.2f}s ({count / elapsed:.1f} Hz)", file=sys.stderr)


if __name__ == "__main__":
    main()
