"""Network peer for Pitfall: The Lost Expedition co-op: one per PC, next to that PC's PCSX2.

Usage (host):  python -I tools/ptle_peer.py --code <shared word> --listen 47100
Usage (join):  python -I tools/ptle_peer.py --code <shared word> --connect <host ip>:47100
Options: --pid <pcsx2 pid> (default: the only running pcsx2-qt.exe)  --pine 28011  --bind 0.0.0.0
         --hz 30  --seconds 0  --csv out.csv

Each tick the peer reads its own game's real player (level, position, pad-1 buttons) over PINE
and sends it to the other peer in one small UDP packet. When a packet from the other peer is
fresh (under 1 s old) and both games are in the same level, it steers its own puppet toward the
other player with controller-port-2 keys, like ptle_bridge.py steer mode. Packets carry a token
derived from --code; anything else is ignored. The host answers whichever address last sent a
valid packet, so it works through NAT on the joiner's side, a Tailscale/ZeroTier overlay, a
forwarded UDP port, or the coop-anything chaos relay. Packets older than the newest one seen
are dropped. Keys are released on exit and while the other peer is silent.

Packet (little endian, 32 bytes): magic b"PTLE", version u8 = 1, pad u8 x3, token u32, seq u32,
level u32, x f32, y f32, z f32, buttons u16, flags u16 (bit 0 = position valid).
"""
import argparse
import csv
import hashlib
import math
import os
import select
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ptle_bridge import Game, steer  # noqa: E402

PACKET = struct.Struct("<4sB3xIIIfffHH")
MAGIC, VERSION = b"PTLE", 1
STALE_AFTER = 1.0


def find_pcsx2_pid():
    """The pid of the only running pcsx2-qt.exe, or exit with a message."""
    import subprocess
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq pcsx2-qt.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    pids = [int(line.split('","')[1]) for line in out.splitlines() if line.startswith('"pcsx2-qt.exe"')]
    if len(pids) != 1:
        sys.exit(f"found {len(pids)} pcsx2-qt.exe processes; pass --pid")
    return pids[0]


def token_of(code):
    return struct.unpack("<I", hashlib.sha256(code.encode("utf-8")).digest()[:4])[0]


def parse_addr(text):
    host, _, port = text.rpartition(":")
    return socket.gethostbyname(host), int(port)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, help="this PC's PCSX2 process id (default: the only pcsx2-qt.exe)")
    parser.add_argument("--code", required=True, help="shared session word; both peers must match")
    parser.add_argument("--pine", type=int, default=28011)
    parser.add_argument("--listen", type=int, default=0, help="UDP port to listen on (host)")
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--connect", help="host address ip:port (join)")
    parser.add_argument("--hz", type=float, default=30)
    parser.add_argument("--seconds", type=float, default=0)
    parser.add_argument("--csv")
    args = parser.parse_args()
    if not args.connect and not args.listen:
        sys.exit("give --listen <port> to host or --connect <ip:port> to join")

    game = Game("local", args.pine, args.pid or find_pcsx2_pid())
    token = token_of(args.code)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.listen))
    sock.setblocking(False)
    peer = parse_addr(args.connect) if args.connect else None
    print(f"peer up: udp {sock.getsockname()[1]}, {'joining ' + str(peer) if peer else 'hosting'}", flush=True)

    writer = None
    if args.csv:
        out = open(args.csv, "w", newline="", encoding="utf-8")
        writer = csv.writer(out)
        writer.writerow(["t", "x", "y", "z", "remote_x", "remote_y", "remote_z", "remote_age_ms",
                         "puppet_gap", "steering"])

    seq, remote, remote_seq, remote_at = 0, None, -1, 0.0
    sent = received = dropped_old = 0
    period, start, ticks = 1.0 / args.hz, time.perf_counter(), 0
    next_report = start + 1.0
    try:
        while not args.seconds or time.perf_counter() - start < args.seconds:
            now = time.perf_counter()
            level, pos, puppet, buttons = game.snapshot()

            while select.select([sock], [], [], 0)[0]:
                try:
                    data, addr = sock.recvfrom(64)
                except (BlockingIOError, ConnectionResetError):
                    break
                if len(data) != PACKET.size:
                    continue
                magic, version, tok, rseq, rlevel, x, y, z, rbuttons, flags = PACKET.unpack(data)
                if magic != MAGIC or version != VERSION or tok != token:
                    continue
                if not args.connect:
                    peer = addr
                if rseq <= remote_seq:
                    dropped_old += 1
                    continue
                remote_seq, remote_at, received = rseq, now, received + 1
                remote = (rlevel, (x, y, z) if flags & 1 else None, rbuttons)

            if peer is not None:
                seq += 1
                x, y, z = pos if pos else (0.0, 0.0, 0.0)
                packet = PACKET.pack(MAGIC, VERSION, token, seq, level, x, y, z, buttons, int(pos is not None))
                try:
                    sock.sendto(packet, peer)
                    sent += 1
                except OSError:
                    pass

            fresh = remote is not None and now - remote_at < STALE_AFTER
            steering = fresh and remote[0] == level and remote[1] is not None and puppet is not None
            gap = float("nan")
            if steering:
                gap = steer(game, puppet, remote[1], remote[2])
            else:
                game.set_keys(set())

            ticks += 1
            if writer:
                rp = remote[1] if fresh and remote[1] else (0.0, 0.0, 0.0)
                writer.writerow([f"{now - start:.3f}", *(f"{c:.3f}" for c in (pos or (0, 0, 0))),
                                 *(f"{c:.3f}" for c in rp),
                                 f"{(now - remote_at) * 1000:.0f}" if remote else "", f"{gap:.2f}", int(steering)])
            if now >= next_report:
                state = "steering" if steering else ("waiting for peer" if not fresh else "peer in another level")
                print(f"{now - start:6.1f}s {state}; sent {sent} recv {received} old {dropped_old}; "
                      f"puppet gap {gap:5.2f}", flush=True)
                next_report += 1.0
            time.sleep(max(0.0, start + ticks * period - time.perf_counter()))
    except KeyboardInterrupt:
        pass
    finally:
        game.set_keys(set())
    elapsed = time.perf_counter() - start
    print(f"{ticks} ticks in {elapsed:.2f}s; sent {sent}, received {received}, dropped as old {dropped_old}, "
          f"position snaps {getattr(game, 'snaps', 0)}", file=sys.stderr)


if __name__ == "__main__":
    main()
