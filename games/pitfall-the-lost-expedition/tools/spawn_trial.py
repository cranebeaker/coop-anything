"""One spawn trial on a mid-load savestate: patch, finish loading, skip cutscene, check health.

Usage: python -I tools/spawn_trial.py <pcsx2_pid> <steps> [slot] [port]

steps 0 = no patch (baseline), 1-5 = run that many spawn stages (see spawn_patch.STAGE_NAMES).
The slot must hold a savestate taken while the level loader is mid-load (catch_load.py).
Healthy = loader idle (state 0) for every sample and the real player within 30 units of the
Crash Site spawn point.
"""
import math
import os
import socket
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pine_check import MSG_LOAD_STATE, MSG_READ32, call  # noqa: E402
import spawn_patch  # noqa: E402

LOADER_STATE = 0x0052F1F4
PLAYER_PTR = 0x0052F198
SPAWN = (-1.587, 747.350, -11.028)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--pos=")]
    pos_arg = [a for a in sys.argv[1:] if a.startswith("--pos=")]
    pos = tuple(float(c) for c in pos_arg[0][6:].split(",")) if pos_arg else None
    pid, steps = int(args[0]), int(args[1])
    slot = int(args[2]) if len(args) > 2 else 20
    port = int(args[3]) if len(args) > 3 else 28011
    sock = socket.create_connection(("127.0.0.1", port), timeout=10)
    u = lambda a: struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    f = lambda a: struct.unpack("<f", call(sock, MSG_READ32, struct.pack("<I", a)))[0]
    vec = lambda a: tuple(f(a + 4 * i) for i in range(3))

    call(sock, MSG_LOAD_STATE, bytes([slot]))
    time.sleep(0.5)
    if steps:
        sock.close()
        words = spawn_patch.patch_words(steps, pos)
        spawn_patch.pine_write(port, words[:-1]
                               + [(spawn_patch.MAILBOX, 1), (spawn_patch.MAILBOX + 4, 0)]
                               + words[-1:])
        sock = socket.create_connection(("127.0.0.1", port), timeout=10)

    seen, start = [], time.time()
    while time.time() - start < 120:
        st = u(LOADER_STATE)
        if not seen or seen[-1] != st:
            seen.append(st)
        if st == 0 and len(seen) > 1:
            break
        time.sleep(0.05)
    print(f"steps={steps} ({', '.join(spawn_patch.STAGE_NAMES[:steps]) or 'no patch'}): "
          f"loader states {seen} in {time.time() - start:.1f}s")

    time.sleep(3)
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "pcsx2_keys.py"), str(pid),
                    "return:400", "wait:2000", "k:400", "wait:2000", "return:400"], check=True)
    time.sleep(8)

    healthy, last = True, None
    for _ in range(20):
        st, p = u(LOADER_STATE), u(PLAYER_PTR)
        pos = vec(p + 0x340) if 0x100000 <= p < 0x2000000 else None
        ok = st == 0 and pos is not None and math.dist(pos, SPAWN) < 30
        healthy &= ok
        last = (st, p, pos)
        time.sleep(0.5)
    extra = u(spawn_patch.MAILBOX + 4)
    extra_info = "none"
    if 0x100000 <= extra < 0x2000000:
        extra_info = (f"0x{extra:08X} class 0x{u(extra):08X} pos "
                      + " ".join(f"{c:.2f}" for c in vec(extra + 0x340)))
    st, p, pos = last
    print(f"  last: loader {st} player 0x{p:08X} pos {pos}; second object {extra_info}")
    print(f"  verdict: {'HEALTHY' if healthy else 'BROKEN'}")
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "pcsx2_keys.py"), str(pid), "f8"], check=True)


if __name__ == "__main__":
    main()
