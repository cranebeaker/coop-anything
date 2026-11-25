# Assassin's Creed Odyssey (2018)

| | |
|---|---|
| Engine | AnvilNext 2.0 (Ubisoft, proprietary) |
| Versions tested | Steam app 812140, build 17083392; `ACOdyssey.exe` x64, 286,453,072 bytes, SHA-256 `ac327dad2cbbdd72a3fda8e99cbeab9d12af328363e4f09bc5674bdd36b8c483` |
| Projects | [odyssey-ghost-coop](https://github.com/xXCroissantXx/odyssey-ghost-coop) (abandoned, MIT) |
| QA level | automated (bot tests); no co-op playthrough with a second real player yet |
| Last updated | 2026-11-09 |

## Where things stand

**Abandoned 2026-11-09.** The owner's goal was co-op where friends appear as real player
heroes in each other's worlds. On this build that would need code injection into the
protected exe, so the project stopped at its read-only ceiling, described below.

The story can't be played together today. Static reverse engineering is ruled out by the
exe's DRM. A read-only runtime route does work, and it is enough for "ghost" co-op: each
player runs their own game, and positions are shared onto a minimap. That is what
[odyssey-ghost-coop](https://github.com/xXCroissantXx/odyssey-ghost-coop) does.

## Existing work

Before this project, no co-op mod or fork was found. The directory search on 2026-10-16
covered GitHub, Nexus, ModDB and Steam. The nearest series option is Assassin's Creed Unity,
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
- **Loading screens and menus:** while the player is not in the world, the pointer
  paths still resolve, but to fake positions. Every loading screen (fast travel or
  loading a save) reports exactly (-94.7, 33.8, -4.3). The main menu reports its
  animated background character near (-47, -15, 0), at exactly z = 0. A mod must
  treat these as "loading", or friends will see the player teleport about 5 km away.
  Checked across 3 loads and a restart; the filter flagged all 164 such samples and
  none of the 916 real ones.
- **Restarts:** the exe can start twice in quick succession at launch, so a mod should
  keep checking for a new process instead of attaching once.
- **Anti-cheat:** none seen. Opening the process with `PROCESS_VM_READ` works, and
  nothing is written or injected.

## What we changed

Nothing in the game. [odyssey-ghost-coop](https://github.com/xXCroissantXx/odyssey-ghost-coop) reads the position, syncs
it over UDP (one host relays at 20 Hz, up to 16 players), and draws other players on an
always-on-top minimap. It also reads the game camera and draws each friend as an animated,
see-through original figure in the game view. That needs borderless-window mode.

- **Camera (build 17083392):** a block holds the camera position, then a row-vector view
  matrix at +32 and a reverse-Z projection at +96 (vertical FOV about 50 degrees, 16:9). It is
  found by scanning for float triples that orbit a still player while the camera swings.
  603 pointer paths to it were found.
- **Limits of an overlay:** figures cannot be hidden behind walls, because the depth
  buffer is only reachable by hooking the renderer. They also cannot use the game's own
  character models or animations. Synthetic test bots that move away from the player's
  path float, because there is no terrain data; real players' own positions do not.
- **Overlay speed:** Tk timers follow the 15.6 ms Windows tick, which capped the overlay
  at about 24 fps. `timeBeginPeriod(1)` lifts it to about 31 fps.

## QA

- Automated bot tests (`python tests/test_bots.py`, `python tests/test_pointers.py`) use dummy processes and bot players: the float
  scan, the pointer scan on a 3-level chain, and 3-player UDP sync with timeouts.
- In-game checks by the owner on build 17083392: position tracking, axis directions,
  pointer paths across a restart, and a minimap with a bot friend.
- In-game bot-friend session (9.5 min) on build 17083392: the host ran from the real game,
  with two bot peers over UDP. One followed the real path 3 s behind; the other circled at
  25 m. Both were fresh in every sample, through two fast travels and a game restart
  mid-session (the mod re-attached by itself).
- Bad-network tests: 20% loss, 80±60 ms jitter and 10% duplicates. There were no backwards
  jumps, and players were fresh in 120 of 120 samples. Removing the ordering check makes the
  test fail. The tests also cover outages, host restart, junk packets, duplicate names and a
  version mismatch, plus a 10-minute soak with 5 peers (no dropouts, no memory growth).
- No co-op session with a second real player yet.

## How to play

See the project README. In short: `pip install -r requirements.txt`, then run `scan` and
`pointers` once per game build (or copy `local/paths.json` from a friend on the same build).
After that, one player runs `python -m odyssey_coop host --name A`, and the others run
`python -m odyssey_coop join <host-ip> --name B`. Over the internet, forward UDP 27015 or
use Tailscale/ZeroTier. Players who want a shared session instead can stream one game
with Parsec or Steam Remote Play.

## Known issues

- Visible remote players are not possible on this route. They would need runtime
  hooks to spawn and drive an NPC.
- Pointer paths are tied to the build. After a game update, the scan has to be redone.

## Next steps

Nobody is working on this. Showing real heroes would need an injection route that this
project rules out. Within read-only limits, the remaining ideas are: gear colours, combat
and quest status, and opt-in data writes for teleport-to-friend and time sync (not
started). Origins and Valhalla are probably blocked the same way, but nobody has checked yet.
