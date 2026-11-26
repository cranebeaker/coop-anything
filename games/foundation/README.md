# Foundation (2025, Polymorph Games)

| | |
|---|---|
| Engine | Polymorph's own C++ engine, Lua (LuaJIT 2.0.5) for mods |
| Versions tested | Steam 1.12.0.16, build 26138506, `foundation.exe` SHA-256 576845171FF3F6E44F1F8C5F231691D732768D3398C0A0DCA8734B42E5252B49, Windows 11 |
| Projects | [CallmeL3o/foundation-coop](https://github.com/CallmeL3o/foundation-coop) (directory entry "Foundation co-op") |
| QA level | automated (two real game clients on one PC, field-by-field save comparison) |
| Last updated | 2026-11-05 |

## Where things stand

A prototype lets two players share one city: the host simulates, the friend's game replays the host's frames
(deterministic lockstep). Both can build, paint zones and change the speed. Save comparisons of both games stay
identical (0 of ~1.9 million simulation fields differ) in every scenario tested so far. It has not been tried on
two real PCs yet.

## Existing work

The game is single-player only. GitHub repository search for "foundation polymorph multiplayer / coop / co-op mod"
returned nothing (2026-11-05). mod.io, Nexus, Steam forums and the Polymorph Discord: not checked yet (unknown).

## How the game works (for co-op)

- Native C++ engine; mods are Lua (mod.io / local `mods` folder, sandboxed; a `moddev` folder runs unsandboxed Lua).
  Lua cannot place buildings, so co-op works natively (DLL injected by a launcher).
- The engine has a **command + replay system**: player actions are `PlaCPlayerCommand*` objects (28 classes) pushed
  with `GamCGame::pushPlayerCommand`, executed inside `updateFrame`; a replay records commands per frame in the
  engine's binary serializer and can play them back (mode 2), checking the random seed per frame. Lockstep builds on
  this: the host records, the client plays back the host's frames.
- Object GUIDs come from a boost UUID generator (BCryptGenRandom); the host streams the GUIDs created inside each
  frame, the client uses them for creations of the same call path in the same frame.
- Non-determinism found and fixed (details and addresses in [NOTES.md](NOTES.md)): CRT `rand()`, a second global
  64-bit LCG used by house-lot seeds and shared with other threads, thread-pool jobs submitted by the simulation
  (navmesh, residential lots, desirability, resource containers), a timer-driven pooled spawner, and build-tool UI
  code that changes simulation state outside the commands (preview creation, edit mode, part placement, preview
  moves, cancelling).
- Save files: PNG thumbnail + zlib world in the engine's binary tree format (key table, nodes
  `{key, next, X, Y}`, objects with an element list and an attribute list). `savediff.py` in the project parses it.

## What we changed

All in [foundation-coop](https://github.com/CallmeL3o/foundation-coop):

- Lockstep transport: host streams frames (commands, seeds, GUIDs, events) over one authenticated TCP link;
  client commands are serialized and sent to the host.
- UI-state replication as events with captured GUIDs; in-frame build-tool calls deferred to a shared frame boundary.
- Lockstep copies of the CRT and engine random generators; simulation thread-pool jobs run inline.
- Drift detector: both games quicksave at the same frame and every field of the two saves is compared.

## QA

- `bot/holdtest.ps1 -Step <scenario>` (two game instances on one PC, UI bot, pause, Ctrl+S on both, `savediff.py`).
- Last results (2026-11-05), all IDENTICAL (0 differing simulation fields, 0 seed/random desyncs):
  - 8 checks over 27 in-game days at x3, host residential zone + client logging zone;
  - host builds a woodcutter's camp, then the client builds one (3 checks over 11 days);
  - build-tool previews opened and cancelled on both sides (2 checks);
  - client camera moved far away (3 checks);
  - player kit end to end without the test mod (zones by both).
- Known drift: a woodcutter's camp preview held in hand by the client for minutes made the client's resources age
  faster (cause not found).

## How to play

Prototype only: see the player README in the project (`play/README.md`). Both players need the game on Steam, a copy
of the game folder, the same save; the host shares a session key.

## Known issues

- Not tested on two real PCs, nor: deleting buildings, roads, job changes, monuments, trade, quests, saving in co-op.
- No reconnect and no automatic resync. Holding a woodcutter preview for minutes can drift.

## Next steps

- First two-PC test; collect `coop.log` and quicksaves from both.
- Find the held-woodcutter-preview drift (resource container "DaysSinceReplenish" ahead on the client).
- Test the remaining actions with the save-diff harness; add reconnect and host-save resync.
