"""Builds the "spawn a second Harry" patch for Pitfall: The Lost Expedition (PS2 NTSC-U SLUS-20408).

Usage: python -I tools/spawn_patch.py pnach <out.pnach>     write a PCSX2 patch file
       python -I tools/spawn_patch.py pine [port] [steps]   write the same words over PINE now
                                                            (steps 1-5: alloc, ctor, init, placement, scene add)
       python -I tools/spawn_patch.py arm [port]            set the mailbox switch to 1

How it works: the level loader (FUN_0026d250, state 5) calls add-to-world FUN_001be9b0(world,
player, x) at 0x0026D4F4. The hook turns that jal into a jal to the stub. The stub makes the
original call, then, if the mailbox switch at 0x000EFF00 is non-zero, clears it and builds a
second player the same way state 2 and FUN_001be9b0 do:
  alloc FUN_002f9e90(0x1B30, 0x10) -> ctor FUN_00173910 -> init FUN_001f29b8(obj, [level+0x8C])
  -> placement class+0x2A4(obj, &spawn_pos + (2,0,0), f12 = spawn_angle_deg * pi/180)
  -> scene add FUN_002d3cd8(world[0], obj, 0, 0)
The new object's address is left in mailbox +4. Nothing here touches the player pointer at
0x0052F198, which belongs to the level loader.
"""
import os
import socket
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mips import NOP, R, addiu, beq, jal, jr, li32, lui, lw, ori, sw  # noqa: E402

CAVE = 0x000F0000
MAILBOX = 0x000EFF00
HOOK = 0x0026D4F4
ORIG_HOOK_WORD = 0x0C06FA6C  # jal 0x001BE9B0

ADD_TO_WORLD = 0x001BE9B0
ALLOC = 0x002F9E90
CTOR = 0x00173910
INIT = 0x001F29B8
SCENE_ADD = 0x002D3CD8
PLAYER_SIZE = 0x1B30
DEG_TO_RAD = 0x3C8EFA36  # 0.0174533f, same constant FUN_001be9b0 loads
PLAYER_INPUTS = 0x0055EC80  # per-player-slot input objects (FUN_00319878 returns [slot] + 0xC)
PUPPET_INPUT_SLOT = 1


def sd(rt, off, rs): return (0x3F << 26) | (R[rs] << 21) | (R[rt] << 16) | (off & 0xFFFF)
def ld(rt, off, rs): return (0x37 << 26) | (R[rs] << 21) | (R[rt] << 16) | (off & 0xFFFF)
def lh(rt, off, rs): return (0x21 << 26) | (R[rs] << 21) | (R[rt] << 16) | (off & 0xFFFF)
def lwc1(ft, off, rs): return (0x31 << 26) | (R[rs] << 21) | (ft << 16) | (off & 0xFFFF)
def swc1(ft, off, rs): return (0x39 << 26) | (R[rs] << 21) | (ft << 16) | (off & 0xFFFF)
def mtc1(rt, fs): return 0x44800000 | (R[rt] << 16) | (fs << 11)
def add_s(fd, fs, ft): return 0x46000000 | (ft << 16) | (fs << 11) | (fd << 6) | 0x00
def mul_s(fd, fs, ft): return 0x46000000 | (ft << 16) | (fs << 11) | (fd << 6) | 0x02
def daddu(rd, rs, rt): return (R[rs] << 21) | (R[rt] << 16) | (R[rd] << 11) | 0x2D
def addu(rd, rs, rt): return (R[rs] << 21) | (R[rt] << 16) | (R[rd] << 11) | 0x21
def jalr(rs): return (R[rs] << 21) | (R["ra"] << 11) | 0x09


STAGE_NAMES = ["alloc", "ctor", "init", "placement", "scene_add"]


def f32_bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def build_stub(steps=len(STAGE_NAMES), pos=None):
    """Stub that runs the first `steps` spawn stages (1 = alloc only ... 5 = everything).

    pos: fixed (x, y, z) for the second player. None = level spawn data + (2, 0, 0), which is
    (2, 0, 0) under the map at this point of the load in Crash Site: he falls and the level
    restarts (see NOTES.md).
    """
    if pos is None:
        place_xyz = [
            lw("t0", 4, "s2"),
            lwc1(0, 0x50, "t0"),
            lui("at", 0x4000), mtc1("at", 1), add_s(0, 0, 1),
            swc1(0, 0, "sp"),
            lw("t1", 0x54, "t0"), sw("t1", 4, "sp"),
            lw("t1", 0x58, "t0"), sw("t1", 8, "sp"),
            lw("t1", 0x5C, "t0"), sw("t1", 12, "sp"),
        ]
    else:
        place_xyz = [lw("t0", 4, "s2")]
        for i, c in enumerate(pos):
            place_xyz += [*li32("t1", f32_bits(c)), sw("t1", 4 * i, "sp")]
        place_xyz += [sw("zero", 12, "sp")]
    head = [
        addiu("sp", "sp", -0x40),
        sd("ra", 0x38, "sp"), sd("s0", 0x10, "sp"), sd("s1", 0x18, "sp"), sd("s2", 0x20, "sp"),
        daddu("s2", "a0", "zero"),            # s2 = world
        jal(ADD_TO_WORLD), NOP,                # original call, a0..a2 untouched
        *li32("t0", MAILBOX),
        lw("t1", 0, "t0"),
        None,                                  # beq t1, zero, END (patched below)
        NOP,
        sw("zero", 0, "t0"),                   # one shot per arm
    ]
    stages = [
        [   # alloc; the address goes to mailbox +4
            addiu("a0", "zero", PLAYER_SIZE),
            jal(ALLOC),
            addiu("a1", "zero", 0x10),
            daddu("s0", "v0", "zero"),
            *li32("t0", MAILBOX),
            sw("s0", 4, "t0"),
        ],
        [   # constructor
            jal(CTOR),
            daddu("a0", "s0", "zero"),
        ],
        [   # init = set the player's input source (+0x7A0). Player slot N's input object is
            # *(PLAYER_INPUTS[N]) + 0xC; the level passes slot 0's. The puppet gets slot 1's,
            # which is fed by controller port 2 (nothing plugged in), so pad 1 does not move it.
            *li32("t0", PLAYER_INPUTS + 4 * PUPPET_INPUT_SLOT),
            lw("t0", 0, "t0"),
            lw("a1", 0xC, "t0"),
            jal(INIT),
            daddu("a0", "s0", "zero"),
        ],
        [   # placement, angle from spawn data
            *place_xyz,
            lwc1(0, 0x6C, "t0"),
            *li32("at", DEG_TO_RAD), mtc1("at", 12), mul_s(12, 0, 12),
            lw("v1", 0, "s0"),
            lh("a0", 0x2A0, "v1"),
            lw("v0", 0x2A4, "v1"),
            daddu("a1", "sp", "zero"),
            jalr("v0"),
            addu("a0", "s0", "a0"),
        ],
        [   # scene add
            lw("a0", 0, "s2"),
            daddu("a1", "s0", "zero"),
            daddu("a3", "zero", "zero"),
            jal(SCENE_ADD),
            daddu("a2", "zero", "zero"),
        ],
    ]
    code = head + [w for stage in stages[:steps] for w in stage]
    end = len(code)
    code += [
        ld("ra", 0x38, "sp"), ld("s0", 0x10, "sp"), ld("s1", 0x18, "sp"), ld("s2", 0x20, "sp"),
        jr("ra"),
        addiu("sp", "sp", 0x40),
    ]
    branch = code.index(None)
    code[branch] = beq("t1", "zero", end - branch - 1)
    return code


def patch_words(steps=len(STAGE_NAMES), pos=None):
    words = [(CAVE + 4 * i, w) for i, w in enumerate(build_stub(steps, pos))]
    words.append((HOOK, jal(CAVE)))
    return words


CRASH_SITE_PUPPET_POS = (0.9, 744.35, -10.5)  # on the ground next to the Crash Site start


def write_pnach(path):
    """The tested patch file: unlabeled lines, so PCSX2 applies it on boot without opting in.

    PCSX2 then disables its own bundled patches for this game (it logs that). Every level load
    spawns a puppet at CRASH_SITE_PUPPET_POS with input slot 1, so only Crash Site is supported.
    """
    lines = [
        "gametitle=Pitfall - The Lost Expedition (NTSC-U) [SLUS-20408] 394F402E",
        "comment=coop-anything puppet spawn (experimental, Crash Site spawn point)",
        f"patch=1,EE,{MAILBOX:08X},word,00000001",
    ]
    lines += [f"patch=1,EE,{a:08X},word,{w:08X}" for a, w in patch_words(len(STAGE_NAMES), CRASH_SITE_PUPPET_POS)]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {len(lines)} lines to {path}")


def pine_write(port, words):
    from pine_check import MSG_READ32, call
    with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
        hook_now = struct.unpack("<I", call(sock, MSG_READ32, struct.pack("<I", HOOK)))[0]
        if hook_now not in (ORIG_HOOK_WORD, jal(CAVE)):
            sys.exit(f"unexpected word 0x{hook_now:08X} at hook 0x{HOOK:08X}; not patching")
        for addr, word in words:
            call(sock, 0x06, struct.pack("<II", addr, word))
    print(f"wrote {len(words)} words over PINE port {port}")


def main():
    mode = sys.argv[1]
    if mode == "pnach":
        write_pnach(sys.argv[2])
    elif mode == "pine":
        port = int(sys.argv[2]) if len(sys.argv) > 2 else 28011
        steps = int(sys.argv[3]) if len(sys.argv) > 3 else len(STAGE_NAMES)
        stub = patch_words(steps)
        pine_write(port, stub[:-1] + [(MAILBOX, 1), stub[-1]])  # hook last
    elif mode == "arm":
        port = int(sys.argv[2]) if len(sys.argv) > 2 else 28011
        pine_write(port, [(MAILBOX, 1), (MAILBOX + 4, 0)])


if __name__ == "__main__":
    main()
