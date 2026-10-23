"""Local co-op bridge for Pitfall: The Lost Expedition (PS2 NTSC-U SLUS-20408) on two PCSX2 instances.

Usage: python -I tools/ptle_bridge.py --a-pid <pid> --b-pid <pid> [--a 28011] [--b 28012] [--hz 30]
       [--seconds 0] [--csv out.csv] [--mode steer|position]

Each instance has a real player (pad 1) and a puppet: the second player spawned by spawn_patch.py
(address in the mailbox at 0x000EFF04), reading input slot 1, which is fed by controller port 2.
Port 2 must be a keyboard-bound DualShock2 in PCSX2 (LUp/LDown/LLeft/LRight = U/N/B/M,
Cross/Square/Circle/Triangle = Z/X/C/V, L1/R1 = Y/R).

steer (default): each tick, hold port-2 direction keys in instance D that walk D's puppet toward
the other instance's real player (the puppet's stick is world-fixed: right = +x, left = -x,
up = -y, down = +y), so the game plays its own run/idle animations and facing. Copies the other
player's Cross/Square/Circle/Triangle/L1/R1. Snaps the puppet's position if it is more than
SNAP units off. position: only writes the position (no animation), the first bridge version.

Syncs only while both instances report the same level and the puppet object still has the player
class. --seconds 0 runs until Ctrl+C. Keys are released on exit.
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
from pcsx2_keys import WM_KEYDOWN, WM_KEYUP, lparam, user32, vk_of, windows_of  # noqa: E402
from pine_check import MSG_ID, MSG_READ32, MSG_UUID, call, read_string  # noqa: E402

MSG_WRITE32 = 0x06
EXPECTED = ("SLUS-20408", "394f402e")
PLAYER_PTR = 0x0052F198
LEVEL_CRC = 0x0052F200
PUPPET_PTR = 0x000EFF04
PLAYER_CLASS = 0x004AB738
POS = 0x340
PAD1_RAW = 0x00576780  # libpad buffer for port 1; buttons are the halfword at +2, active low
EE_RAM = (0x00100000, 0x02000000)

START_MOVING, STOP_MOVING, SNAP, STALL_AFTER = 1.2, 0.6, 6.0, 1.0
DIR_KEYS = {"right": "m", "left": "b", "down": "n", "up": "u"}
BUTTON_KEYS = {0x4000: "z", 0x8000: "x", 0x2000: "c", 0x1000: "v", 0x0400: "y", 0x0800: "r"}


class Game:
    def __init__(self, name, port, pid):
        self.name, self.port = name, port
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        ident = (read_string(call(self.sock, MSG_ID)), read_string(call(self.sock, MSG_UUID)).lower())
        if ident != EXPECTED:
            sys.exit(f"{name} (port {port}) runs {ident}, bridge is for {EXPECTED}")
        self.hwnds = windows_of(pid) if pid else []
        self.held = set()
        self.moving = False
        self.best_gap, self.best_at = float("inf"), 0.0

    def u32(self, addr):
        return struct.unpack("<I", call(self.sock, MSG_READ32, struct.pack("<I", addr)))[0]

    def vec(self, addr):
        return tuple(struct.unpack("<f", call(self.sock, MSG_READ32, struct.pack("<I", addr + 4 * i)))[0]
                     for i in range(3))

    def write_vec(self, addr, v):
        for i, c in enumerate(v):
            call(self.sock, MSG_WRITE32, struct.pack("<If", addr + 4 * i, c))

    def snapshot(self):
        """(level, real player position or None, puppet address or None, pressed pad-1 buttons)."""
        level, player, puppet = self.u32(LEVEL_CRC), self.u32(PLAYER_PTR), self.u32(PUPPET_PTR)
        pos = self.vec(player + POS) if EE_RAM[0] <= player < EE_RAM[1] else None
        if pos is not None and not all(math.isfinite(c) for c in pos):
            pos = None
        if not (EE_RAM[0] <= puppet < EE_RAM[1] and self.u32(puppet) == PLAYER_CLASS):
            puppet = None
        buttons = ~(self.u32(PAD1_RAW) >> 16) & 0xFFFF
        return level, pos, puppet, buttons

    def set_keys(self, wanted):
        for key in self.held - wanted:
            self._post(key, WM_KEYUP)
        for key in wanted - self.held:
            self._post(key, WM_KEYDOWN)
        self.held = set(wanted)

    def _post(self, key, msg):
        vk = vk_of(key)
        for hwnd in self.hwnds:
            user32.PostMessageW(hwnd, msg, vk, lparam(vk, msg == WM_KEYUP))


def steer(game, puppet, target, buttons):
    """Hold port-2 keys in `game` that move its puppet toward `target`.

    Snaps the position if the puppet is more than SNAP off, or if it has been trying to move for
    STALL_AFTER seconds without getting 0.1 units closer (blocked by a wall or the local player).
    """
    here = game.vec(puppet + POS)
    dx, dy = target[0] - here[0], target[1] - here[1]
    dist = math.hypot(dx, dy)
    wanted = {key for bit, key in BUTTON_KEYS.items() if buttons & bit}
    now = time.perf_counter()
    if not game.moving or dist < game.best_gap - 0.1:
        game.best_gap, game.best_at = dist, now
    stalled = game.moving and now - game.best_at > STALL_AFTER
    if dist > SNAP or stalled:
        game.write_vec(puppet + POS, target)
        game.moving = False
        game.snaps = getattr(game, "snaps", 0) + 1
    else:
        game.moving = dist > (STOP_MOVING if game.moving else START_MOVING)
        if game.moving:
            if dx > 0.38 * dist:
                wanted.add(DIR_KEYS["right"])
            if dx < -0.38 * dist:
                wanted.add(DIR_KEYS["left"])
            if dy > 0.38 * dist:
                wanted.add(DIR_KEYS["down"])
            if dy < -0.38 * dist:
                wanted.add(DIR_KEYS["up"])
    game.set_keys(wanted)
    return dist


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--a", type=int, default=28011)
    parser.add_argument("--b", type=int, default=28012)
    parser.add_argument("--a-pid", type=int)
    parser.add_argument("--b-pid", type=int)
    parser.add_argument("--mode", choices=("steer", "position"), default="steer")
    parser.add_argument("--hz", type=float, default=30)
    parser.add_argument("--seconds", type=float, default=0)
    parser.add_argument("--csv")
    args = parser.parse_args()
    if args.mode == "steer" and not (args.a_pid and args.b_pid):
        sys.exit("steer mode needs --a-pid and --b-pid (to press port-2 keys in each PCSX2)")

    a, b = Game("A", args.a, args.a_pid), Game("B", args.b, args.b_pid)
    writer = None
    if args.csv:
        out = open(args.csv, "w", newline="", encoding="utf-8")
        writer = csv.writer(out)
        writer.writerow(["t", "a_x", "a_y", "a_z", "b_x", "b_y", "b_z", "gap_in_a", "gap_in_b",
                         "synced", "tick_ms"])

    period, start, ticks, synced_ticks, worst = 1.0 / args.hz, time.perf_counter(), 0, 0, 0.0
    next_report = start + 1.0
    try:
        while not args.seconds or time.perf_counter() - start < args.seconds:
            t0 = time.perf_counter()
            (la, pa, qa, ba), (lb, pb, qb, bb) = a.snapshot(), b.snapshot()
            synced = la == lb and None not in (pa, pb, qa, qb)
            gap_a = gap_b = float("nan")
            if synced and args.mode == "position":
                b.write_vec(qb + POS, pa)
                a.write_vec(qa + POS, pb)
            elif synced:
                gap_b = steer(b, qb, pa, ba)   # A's player drives the puppet in B
                gap_a = steer(a, qa, pb, bb)   # B's player drives the puppet in A
            else:
                a.set_keys(set())
                b.set_keys(set())
            synced_ticks += synced
            tick_ms = (time.perf_counter() - t0) * 1000
            worst = max(worst, tick_ms)
            ticks += 1
            if writer:
                writer.writerow([f"{t0 - start:.3f}", *(f"{c:.3f}" for c in (pa or (0, 0, 0))),
                                 *(f"{c:.3f}" for c in (pb or (0, 0, 0))), f"{gap_a:.2f}", f"{gap_b:.2f}",
                                 int(synced), f"{tick_ms:.1f}"])
            if t0 >= next_report:
                state = "synced" if synced else "waiting (level or puppet mismatch)"
                print(f"{t0 - start:6.1f}s {state}  A {fmt(pa)}  B {fmt(pb)}  "
                      f"puppet gap A {gap_a:5.2f} B {gap_b:5.2f}  tick {tick_ms:.1f} ms", flush=True)
                next_report += 1.0
            time.sleep(max(0.0, start + ticks * period - time.perf_counter()))
    except KeyboardInterrupt:
        pass
    finally:
        a.set_keys(set())
        b.set_keys(set())
    elapsed = time.perf_counter() - start
    print(f"{ticks} ticks in {elapsed:.2f}s ({ticks / elapsed:.1f} Hz), {synced_ticks} synced, "
          f"worst tick {worst:.1f} ms", file=sys.stderr)


def fmt(v):
    return "-" if v is None else "(" + ", ".join(f"{c:7.2f}" for c in v) + ")"


if __name__ == "__main__":
    main()
