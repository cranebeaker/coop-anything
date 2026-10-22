# Shadow of War co-op: handoff (2026-10-22)

Read this first, then the "Status summary 2026-10-21" and session ag at the end of
[NOTES.md](NOTES.md).

**Where we are:** phase 5 (build), first co-op step working one way on one PC. Copy A
streams its player over loopback UDP every frame: position, rotation, health and the 25
inputs of the player's animation graph (stick, speed, turning, camera). Copy B shows a
spawned Talion stand-in, AI off, moved by A's inputs through its own animation
(`coop_puppet_move=anim`): it walks, turns and stops with A's stride, feet on the ground,
and is put back only when it drifts past 150 units or once A stands still. Free-run (A
button) shows the run but falls behind (put back about every 150 units). Not carried yet:
jumps and attacks (unchecked), anything from B to A, and world state (enemies, Nemesis,
quests, time).

**Build and tools:** game build 3095001 only (Steam app 356190). Our guard DLL
(`bink2w64.dll` proxy) and probe live in the private `local/sow-coop` repo. Offline tests
41/41, negative control 18/18. The hub branch `game/shadow-of-war` is ahead of PR #99 and
not pushed.

**Run it:**
- `local/launch-sow.ps1 -Copy a -TimeoutSec 7200 -AllowOthers`, then the same with
  `-Copy b`. Each copy loads unattended and reaches the world in about 48 s; B spawns the
  stand-in about 20 s later.
- Probe: `python -I local/sow-coop/tools/probe.py local/test-installs/sow-b-profile remote`
  (or `puppet`, `inputs`, `player`, `teleport x y z`, `pad ...`, `puppet follow anim|teleport|none`).
- Watch it: `local/sow-coop/tools/view-standin.ps1 -Out <prefix> -Turn` once (B faces A's
  start), then `view-steer.ps1 -Out <prefix> -Dx 1 -Dz 0`; measure with `root_match.py`
  and `drift.py` (move B's own player off A's path first).

**Rules that bit us:**
- Never touch the owner's install or saves; work only in `local/test-installs/`.
- Ask before pushing anything.
- Stop stray background `sh.exe` input loops before restarting a copy.
- A test that runs sender and receiver in one process can hide receive-side bugs.

**Next steps:**
1. Free-run speed: the stand-in runs with A's stride but slower (put back every ~150 units).
   The graph gets no extra message during free-run (probe `trace`), so look at the
   player's movement code for where free-run speed comes from, and drive the stand-in's
   speed the same way.
2. Steadier drift correction, tested on a repeatable path: `DeltaYaw` turns the stand-in;
   writing its position each frame made drift worse.
3. Make it two-way (B to A).
4. Then world state: host authority for enemies and Nemesis events, and a real network
   transport.
5. Show the owner the hub diff and ask before pushing.
