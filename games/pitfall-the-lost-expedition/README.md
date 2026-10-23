# Pitfall: The Lost Expedition (2004)

| | |
|---|---|
| Engine | unknown (Edge of Reality in-house; disc ships `EORPS2IO.IRX`) |
| Versions tested | PS2 NTSC-U `SLUS-20408`, PCSX2 ELF CRC `394F402E`, disc VER 1.00, on PCSX2 v2.8.2 (Windows) |
| Projects | coop-anything ghost co-op prototype (this folder: patch, PINE peer, tools). Directory entry: "Pitfall: The Lost Expedition", kind `mod`, status `prototype` |
| QA level | automated (two real PCSX2 clients on one PC, scripted; see QA) |
| Last updated | 2026-10-23 |

## Where things stand

Two players can see each other in Crash Site, the first jungle level: each runs their own PCSX2,
and the other player's Harry appears in it as a second Harry who runs after their real position
with the game's own animations. Nothing else is shared yet (enemies, items, puzzles, progress),
other levels are not supported, and two real PCs over the internet have not been tested; the
network peer was tested on one PC through a lag and loss simulator.

## Existing work

- No co-op or multiplayer mod found (web search for co-op mod, multiplayer, decompilation, two
  player; 2026-10-23). No other issues or PRs for the game in this repo.
- Native PC port by Beenox, published by Aspyr in October 2004
  ([Wikipedia](https://en.wikipedia.org/wiki/Pitfall:_The_Lost_Expedition)). Not used here because
  the owner plays the PS2 version; per the playbook it may be the easier target.
- [Avasam/ptle-tools](https://github.com/Avasam/ptle-tools) (LGPL-3.0): GameCube US memory watches,
  an entrance randomizer for Dolphin, and `world_infos.json` with the level CRC32s that the PS2
  build also uses.
- GOG's listing names "split screen" as a mode
  ([GOG](https://www.gog.com/dreamlist/game/pitfall-the-lost-expedition-2004)); no other source
  confirms a two-player mode.

## How the game works (for co-op)

- No netcode. Single player, but the input system has per-player slots: table `0x0055EC80` holds
  one input object per slot, each fed by a controller port (slot 0 = port 1, slot 1 = port 2).
  A player object reads its input through the pointer at +0x7A0.
- A per-level load state machine (`FUN_0026d250`, state object at `0x0052F190`) allocates and
  constructs the player (`FUN_00173910`, 0x1B30 bytes), gives it slot 0's input, places it and
  adds it to the world (`FUN_001be9b0`). Spirit-temple levels build a different player class.
- The player pointer `0x0052F198` is a field of that state object, read in 430 places.
- Player fields: position +0x340 (Z up), facing radians +0x35C (read only), health +0x688.
  Current level CRC at `0x0052F200`.
- Save only stores progress at level entry (in-game warning). Level ids are CRC32s of asset names.

Full memory map, code addresses and experiment logs: [NOTES.md](NOTES.md).

## What we changed

- **Second player (puppet).** `tools/spawn_patch.py pnach` writes a PCSX2 patch file. It hooks
  the level loader's add-to-world call (0x0026D4F4) with a small MIPS routine at 0x000F0000 that
  builds a second player the same way the loader builds the first, with two differences found
  the hard way: it is placed at a fixed ground point in Crash Site (the spawn data still reads
  0,0,0 at that point of the load, so a puppet placed there falls under the map and the level
  restarts in a loop), and it gets input slot 1 instead of slot 0 (otherwise it copies pad 1).
- **Puppet control.** PCSX2 controller port 2 is a keyboard-bound DualShock 2; a disconnected
  port is reset by the game every frame, so writing input into memory does not work. The puppet's
  stick is world-fixed (no camera), so steering it toward a target position is simple.
- **Bridge and network peer.** `tools/ptle_peer.py` runs next to each PCSX2: 30 Hz UDP packets
  with level, position and pad-1 buttons; it steers its own puppet toward the other player with
  port-2 key presses (run animation and facing come from the game), copies Cross/Square/Circle/
  Triangle/L1/R1, and snaps the position when the puppet is more than 6 units off or blocked for
  1 s. `tools/ptle_bridge.py` does the same for two PCSX2 copies on one PC.
- **Tools** for finding all of this: PINE helpers, a no-focus key sender, savestate RAM diffing,
  Ghidra headless scripts, a small R5900 assembler. See [tools/README.md](tools/README.md).

## QA

`tools/peer_trial.ps1` drives two real PCSX2 v2.8.2 clients on one Windows PC: both load the
same Crash Site savestate with the puppet patch active, a host peer and a joining peer start
(optionally through `tools/chaos/chaos.py relay`), one Harry walks 1.5 s, then the other, and
`tools/peer_check.py` asserts for each side that the peer steered for at least half the ticks and
the puppet ended within 2 units of the other player. 18 s runs, 2026-10-23:

```
negative control (joiner sends to a dead port):
local\qa-dead-host.csv: never steered
local\qa-dead-join.csv: never steered
FAIL
clean loopback:
local\qa-clean-host.csv: steered 539/540 ticks, max gap 6.07, final gap 1.13: ok
local\qa-clean-join.csv: steered 539/540 ticks, max gap 6.16, final gap 0.27: ok
PASS
chaos relay, profile bad_wifi, seed 7:
local\qa-badwifi-host.csv: steered 538/540 ticks, max gap 6.34, final gap 1.06: ok
local\qa-badwifi-join.csv: steered 537/540 ticks, max gap 4.93, final gap 0.18: ok
PASS
```

An earlier run through the `hell` profile (8% loss, 150 ms jitter, 10% reordering) also passes
the same check (final gaps 1.20 and 0.93). The patch file was checked on a cold boot: PCSX2 loads
it, LOAD GAME into Crash Site spawns the puppet with input slot 1, and a screenshot shows both
Harrys. Not covered: two physical PCs, jumps and attacks on screen, other levels.

## How to play

See [PLAY.md](PLAY.md): PCSX2 PINE on, port-2 key bindings, the patch file, a Crash Site save,
Tailscale (or a forwarded UDP port), then `ptle_peer.py --listen` on one PC and
`ptle_peer.py --connect` on the other.

## Known issues

- Only Crash Site. In other levels the puppet spawns at the Crash Site point and may fall, which
  restarts the level.
- Worlds are separate: enemies, items, doors and progress are not shared.
- The other player's Harry trails a few units behind while running and stops about one unit away.
- The patch file switches off PCSX2's bundled patches for this game.
- Do not press U/N/B/M/Z/X/C/V/Y/R in PCSX2 while playing; they drive the puppet.

## Next steps

1. Test on two real PCs over Tailscale.
2. Spawn the puppet next to the real player after the load finishes instead of at a fixed point,
   so every level works.
3. Check jumps and attacks; send facing and animation state if steering is not enough.
4. Share world state: start with level transitions, then pickups, then enemies (host authority).
