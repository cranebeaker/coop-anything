# Assassin's Creed Odyssey (2018)

| | |
|---|---|
| Engine | AnvilNext 2.0 (Ubisoft, proprietary) |
| Versions tested | Steam app 812140, build 17083392; `ACOdyssey.exe` x64, 286,453,072 bytes, SHA-256 `ac327dad2cbbdd72a3fda8e99cbeab9d12af328363e4f09bc5674bdd36b8c483` |
| Projects | none published; directory entry "Assassin's Creed Origins, Odyssey and Valhalla" (gap) |
| QA level | none (no co-op playthrough yet) |
| Last updated | 2026-11-09 |

## Where things stand

The story can't be played together today. Static reverse engineering is ruled out by the
exe's DRM. A read-only runtime route does work, and it is enough for "ghost" co-op: each
player runs their own game, and positions are shared onto a minimap. An unpublished
prototype does this.

## Existing work

No published co-op mod or fork was found. The directory search on 2026-10-16 covered
GitHub, Nexus, ModDB and Steam. The nearest series option is Assassin's Creed Unity,
which has official co-op side missions.

## How the game works (for co-op)

- **Exe protection:** `game_identity` shows `.vmp0` / `.vmp1` sections (VMProtect), and
  the game also ships with Denuvo. Static analysis would need DRM removal, which this
  project does not do or publish.
- **Netcode:** none. The game is single-player only.
- **Player position:** three float32 values, **x east, y north, z up, in metres**. This
  was checked by walking 35 m north (+y) and 69 m east (+x). World coordinates near
  the area tested are around (-3400, 3950, 300).
- **Many copies:** the position appears in hundreds of places in private heap memory.
  Some are temporary and get zeroed or reused as the player moves. In one test, 252
  copies moved together, and reading several copies and taking the value most agree on
  is reliable.
- **Finding it once:** an unknown-value float scan works well. Filter for values that
  went up after climbing, unchanged while standing still, and down after dropping, and
  repeat. Then keep the candidates whose two preceding floats changed after a walk on
  flat ground. Skipping exact-integer values removes most of the flags and counters.
  A block of identical (h, h, h) values that copies the player's height is a decoy;
  the walk check removes it.
- **Pointer paths survive restarts:** a backwards pointer scan from the position copies
  (offset 0x1000 at the first level, 0x800 above it, up to depth 5) found 1,986 paths
  that start at the exe's static data. After a full game restart, 249 still agreed on
  the player's position, including all 27 one-jump paths, and they followed a 30 m
  walk exactly.
- **Anti-cheat:** none seen. Opening the process with `PROCESS_VM_READ` works, and
  nothing is written or injected.

## What we changed

Nothing in the game. A separate prototype tool (unpublished) reads the position, syncs
it over UDP (one host relays at 20 Hz), and draws other players on an always-on-top
minimap. That needs borderless-window mode.

## QA

- The prototype's automated bot tests use dummy processes and bot players: the float
  scan, the pointer scan on a 3-level chain, and 3-player UDP sync with timeouts.
- In-game checks by the owner on build 17083392: position tracking, axis directions,
  pointer paths across a restart, and a minimap with a bot friend.
- No co-op session with a second real player yet.

## How to play

There is no published co-op build. The streaming fallback is Parsec or Steam Remote
Play, which shares one game session.

## Known issues

- Visible remote players are not possible on this route. They would need runtime
  hooks to spawn and drive an NPC.
- Pointer paths are tied to the build. After a game update, the scan has to be redone.

## Next steps

Publish the prototype as its own repo and link it here, run a real two-player session,
then look for camera data to draw markers in the game view. Origins and Valhalla are
probably blocked the same way, but nobody has checked yet.
