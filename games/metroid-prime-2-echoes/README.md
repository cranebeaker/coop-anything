# Metroid Prime 2: Echoes (2004), in Metroid Prime Trilogy (2009)

| | |
|---|---|
| Engine | Retro Studios in-house engine, Wii (PowerPC), run in Dolphin; Echoes enemies are RSO modules loaded at runtime |
| Versions tested | Metroid Prime Trilogy NTSC-U `R3ME01` rev 0 (extracted disc); `rs5mp2_p.dol` SHA-256 `5b68271fd75ff1bd48986874d6f9112fc8299a1c93c6ce459fa1a83b8ebeb2b2` (SHA-1 `2375606f4e9429a699cfa02728b0bb1176421226`); Dolphin 2609a, Windows 10 |
| Projects | [Echoes co-op](https://github.com/FarrenB939/metroid-prime-2-echoes-coop) (Gecko + Dolphin patch codes, split screen, 2-4 players), directory entry "Echoes co-op" |
| QA level | community-reports (playtests with two Xbox controllers, opening hours up to the item loss; scripted checks against the running game for each feature; no automated suite) |
| Last updated | 2026-10-31 |

## Where things stand

Two to four players can play the Echoes campaign together on one PC, split screen, starting
from Echoes' own versus mode. Doors, elevators, portals, saving, scanning, cutscenes, the map,
pickups, story item changes and bosses all work for every player in testing. Only the opening
hours have been played by people. For later areas the test harness teleported into 282 of the
284 rooms of the five main worlds to list their enemies, and checked that the bosses present in a
fresh game go after player 2 too. Only the NTSC-U Trilogy is supported; the GameCube Echoes is not.

## Existing work

- No Metroid Prime entry in this directory and no issues or PRs here (2026-10-21 and 2026-10-24).
- SubsonicBeatz154, "Metroid Prime 2 (Trilogy) Co-op Mod", 2019-04 to 2019-07, same NTSC-U Trilogy
  build, asset edits only (Prime World Editor PAKs), abandoned, no public download found
  ([forum thread](https://forum.metroidconstruction.com/index.php?topic=4942.0), read through an
  archive.org capture). It listed the blockers that need code changes: death ends the match, save
  stations off, rooms follow player 1, world teleports overwrite inventories, Scan Visor dead,
  separate inventories, friendly fire, enemies target player 1, cutscene cameras break for player 2.
  This project fixes all of them with code.
- xjonx, GameCube Echoes and the E3 demo, around 2018, cancelled: hit the GameCube multiplayer
  memory limit ([video](https://youtu.be/PXVwoBMD6k0)).
- [Randovania](https://github.com/randovania/randovania) multiworld sends items between separate
  games (GameCube discs, not the Trilogy); not a shared world.
- PrimeHack talks to player 1 only; Wii Remotes 2-4 don't work under it in Echoes multiplayer
  ([PrimeHack issue #113](https://github.com/shiiion/dolphin/issues/113)). Use mainline Dolphin.

## How the game works (for co-op)

- The Trilogy front end (`main.dol`) loads `rs5mp2_p.dol` when Echoes starts, so every patch is
  gated on three words of that executable being resident. Enemies live in RSO modules.
- Echoes' versus mode already runs 2-4 Samus actors, split-screen cameras and HUDs in one world
  (`CStateManager` 0x804e72e8: player count +0x14f0, players +0x14f4, player states +0x1504,
  camera managers +0x1514). Loading a campaign world in that mode works; the rest is code fixes.
- The current room follows `players[0]` only (door think), and a lot of game code reads
  `players[0]`: enemy targeting, script message senders, boss target ids, the Scan Visor's target buffer.
- Story loadout changes are script messages to spawn points (RSET applies a loadout to every
  player). Multiplayer death respawns rebuild the player state from scratch.
- Difficulty: the getter `FUN_800bc668` returns the save's difficulty only in single-player mode
  ('SNGL'); versus mode ('DTHM') always got Veteran. 0 Normal, 1 Veteran, 2 Hypermode.
- Full map of addresses, hooks, what was tried and every live test: [NOTES.md](NOTES.md).

## What we changed

One Gecko code (gate, single-word patches, and a stub per hook) plus a Dolphin `[OnFrame]` patch
that writes the hook bodies to unused MEM2 (0x93700000), so the code list stays under Dolphin's
limit. Every hook's original word is checked against the executable before the codes are built.
The Gecko part is gated on 23 words of `rs5mp2_p.dol`, one every 256 KB of its code: with only three,
leaving Echoes let the code handler patch the incoming front end for a frame, which then crashed into a
hook body ("Invalid read from 0x000014f0").
Codes, as named in the ini:

- **A, keep inventory:** multiplayer respawns keep items; the spawn point's start items are given
  only on a session's first spawn.
- **B, campaign session:** no match timer or kill limit; choosing Sidehopper Station starts the
  campaign (or continues the co-op save file) with every joined player; a respawned player
  appears beside a teammate (placed as in code C) with the screen faded back in.
- **C, shared rooms:** any player crossing a door switches the room (crossing = past the door's plane and
  within 6 units of its opening, whatever the door's size). The others can walk through while the door is
  open; when it closes, anyone still in the room behind it is placed beside the leader: right, else left,
  else behind, else in front, else away from the door, then the same at 1.6 units. Each spot needs a clear
  collision ray, floor below it, and must not be back through the closed door. A Morph Ball leader gets an
  upright frame (the ball's own transform rolls with it) and ball-height checks, so two balls fit a tunnel.
- **D, cutscenes:** play full screen from player 1's camera, every player held as in single
  player; message boxes and the Data Network take the whole screen too. Start (Wii 2) skips a cutscene
  the game marks skippable on a replay, on the first viewing too (the game refuses skips in multiplayer).
- **E, Scan Visor and no friendly fire:** scanning works for every player (fallback target picker
  for players 2-4), scan text shows and any player can page it; players can't hurt each other.
- **F, enemies and bosses:** each enemy's think and messages see the nearest player as player 1;
  bosses that store a target player id (9 classes) are pointed at the nearest player.
- **G, save stations** under the multiplayer rules. **H, pickups** go to every player. **I, scan progress** shared. **J, script upgrades**
  go to every player. **K, map screen** for any controller, and the pause button (Wii 2) opens the
  single-player Data Network (scan and item percentages, Inventory, Logbook) full screen for any
  controller (the player who opened it controls it), instead of the per-player multiplayer "Quit Game?" menu. **L, shared lives:** a pool set by
  the Deathmatch Kill Limit, refilled by saving; downed players spectate a teammate.
- **Difficulty:** three small codes (Normal / Veteran / Hypermode, Hypermode on by default).
- Story RSET is vanilla again (2026-10-24): an earlier "raise only" change blocked the item loss.

## QA

- No automated suite. Each feature was checked against the running game with Python scripts that
  read and write Dolphin's memory, call game functions through a small hook, and press keys (the
  scripts aren't published; the project repo has the code generator); results are logged per session in [NOTES.md](NOTES.md).
- 2026-10-30, after a playtest past the item loss (fresh boots of the test build):
  - Hive Chamber C's door to Hive Transport Area is 15 x 10; its old crossing "centre" sat 6.7 units above
    the floor, so walking through never switched the room and the room unloaded when the door closed. With
    the fix, a crossing at foot level switched it at once, and the lift console scanned ("Elevator unit
    online"). Playtest confirmed.
  - Morph Ball tunnel between Hive Chamber B and C: reproduced two balls in one spot (no placement passed),
    then after the fix the follower landed 1.6 units beside the leader (C to B) and 2.25 units (B to C), no
    overlap. Playtest confirmed both ways. Respawn placement: 2.25 units away, on the floor.
  - Cutscene skip: the opening of a new game skipped part-way through; both players kept the start items.
    Playtest confirmed.
- 2026-10-24, second round (fresh boots with the release codes):
  - Exit crash reproduced (quit multiplayer to the main menu), then gone after the gate change: main menu,
    no warning, and the front end's code at the old hook sites untouched.
  - Data Network: player 2's Menu opens it full screen; player 2 browses and closes it. A playtest
    found player 1 couldn't control it (player 2's idle input replaced player 1's each frame); it now takes only
    the opener's input, and a follow-up playtest confirmed Menu works for both.
- Earlier checks, 2026-10-24, fresh boots with the release codes:
  - Hypermode: getter returns 2; a 10-damage enemy hit took player 1 from 99 to 83.7 (x1.53);
    player 1's 10 against an enemy became 5.0.
  - Item loss (Hive Chamber B): both players lost Missiles, Space Jump, Bombs, Boost and Spider Ball.
  - Morph-ball follower through a Landing Site door: moved 57.5 units, ball camera 6.4 units away
    on the next sample, no launch.
- Earlier (2026-10-23): smoke test of the release file on one run: doors, scans, text box, map
  from both players, lives with save Yes/No, boss targeting from each player, frame rate 89-91
  (harness calls per second) in a 16-enemy room.
- Enemy survey: the harness teleported into 282 of the 284 rooms of the five main worlds and
  recorded every enemy class; bosses that remember a target player (Blogg, Space Pirate and
  others) were checked with player 2 near and player 1 far, before and after the fix.

## How to play

See the project's README: copy `echoes-coop-R3ME01.ini` to Dolphin's `GameSettings\R3ME01.ini`,
enable cheats, optionally use the two-Xbox-controller `WiimoteNew.ini`, then Echoes >
Multiplayer > 2-4 players > Deathmatch (Kill Limit sets lives) > Sidehopper Station. Co-op uses
the Trilogy's third Echoes save file.

## Known issues

- Only the opening hours were played by people; later areas are untested in real play.
- NTSC-U Trilogy only (`R3ME01` rev 0). PAL, Japanese and GameCube releases won't work.
- Local split screen only. Dolphin NetPlay is untested.
- Cutscenes and message boxes use player 1's view.
- Menu opens the Data Network instead of the multiplayer "Quit Game?" menu; quit from the Data
  Network's Options.
- Players have to jump off save stations to stop them asking to save again.
- On Xbox controllers Wii A (confirm, fire) is RT; the Xbox A button is Wii B.

## Next steps

- A full playthrough in co-op, with a list of story spots that still assume one player.
- Story-gated enemies and devices past the opening (no later save was available).
- NetPlay check; PAL build support (addresses differ).
