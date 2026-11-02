# TMNT (2007, PC)

> **Work in progress, not finished.** Two players can play on one PC, but large parts of the game have not been
> tried yet and there is no public release.

| | |
|---|---|
| Engine | Ubisoft Jade ("Engine v428-42" in the game's log; gameplay scripts compiled into the exe) |
| Versions tested | Retail PC, `TMNTGame.exe` 1.0.0.188 (UPX-packed, SecuROM traces), SHA-256 `701d9f00f8c1edf790466f2ecfeb72a64fe7ccdc491f7c370ae11c1942740589`; store/edition unknown |
| Projects | Local couch co-op by DLL injection (private, in progress; no public repo yet) |
| QA level | none |
| Last updated | 2026-11-02 |

## Where things stand

A second player controls a second turtle in the same game process, on one screen, with a second controller. Where a
level has the other brothers loaded (Map 5, *Ninja Tag Time*), player 2 plays the real Raphael, Michelangelo or
Donatello with his own weapon and combos; in levels that load only one turtle (Level 1, *Mystical Jungle*) player 2
is a copy of player 1's turtle in another bandana colour. Covered: Level 1 and Map 5. Rest of the campaign: not
covered yet. Not released.

## Existing work

- No co-op mod for the PC game was found (GitHub, Nucleus Co-op handler hub, web search; 2026-10-12). Only the PSP
  and DS versions had multiplayer.
- [xan1242/UbiTMNTHax](https://github.com/xan1242/UbiTMNTHax): ASI plugin for the same exe (windowed, skip launcher,
  multiple instances). Not co-op, but a reference for hooking this build.
- [hogsy/jaded](https://github.com/hogsy/jaded): community fork of the 2006 Jade engine source; its object flag
  constants (`OBJconst.h`) match this build and helped read the turtle objects. It does not load TMNT data.

## How the game works (for co-op)

Details and addresses (for the build above) are in [NOTES.md](NOTES.md).

- Normally one turtle is on screen. The turtle manager registers the brothers a level uses; a brother appears for
  team moves (Brother Throw, Super Family Attack) or, in tag levels, when the player switches.
- Turtle identities (AI `+0x6A4`): 1 Leonardo, 2 Raphael, 3 Michelangelo, 4 Donatello.
- Input goes through Ubisoft's Gear input library (DirectInput 8 and XInput 1.3). The game merges all pads into one
  input state, so a second physical pad simply also controls player 1. Per-turtle "IO ports" are on/off gates, not
  controller indexes.
- The active pad record (`0x160` bytes at `[0x01502198]`) and its wrapper (`0xE8` bytes) are updated each frame by
  `0x00639C80`.
- The game reads the keyboard and mouse through DirectInput even while its window is in the background.
- The launcher's mutex blocks a second instance, so multi-instance split screen is not an easy route.

## What we changed

All in an injected DLL (a host DLL loaded at start, plus a mod DLL that is reloaded while the game runs). No game
files are changed on disk.

- **Player 2 is a real brother where possible.** The registered brother joins through the game's own `Turtle_Spawn`
  and is shown through the engine's control-flag setter (hide, then show). Clearing the hide flag in memory alone left
  him culled: only his weapon and bandana were drawn. A brother that is still not drawn after three tries is skipped
  for that level.
- **Otherwise a copy of player 1.** In single-turtle levels the engine's object duplicate makes a second Leonardo, with
  its own skeleton unshared from player 1's. The chosen hero only changes the bandana colour. An earlier version also
  wrote the chosen brother's identity and weapon skeleton into the copy; that made it pull katanas and freeze on its
  first attack, so it was removed.
- **Independent input.** The pad record and wrapper are swapped around a second pass of the input pipeline and around
  player 2's AI tick, so player 2's buttons, sticks and combo history are his own.
- **Team moves with player 2 as partner**, started by either player. The game ends the partner's part with "absent"
  (state 0); that is redirected to playable for player 2 and, when player 2 started the move, for player 1 too (player
  1 used to vanish and lose control there).
- **Shared camera** on the midpoint of both turtles, a leash, enemies targeting the nearer turtle, a health bar for
  player 2, revive by standing next to a downed partner, and recovery after falling out of the level.
- **Foot rings** in each turtle's colour, drawn as soft alpha-blended bands. Screen positions use the game's own
  projection (focal factor `|proj._22| / 2`, 1.27 here); an earlier fixed guess of 1.05 put markers too high near the
  bottom of the screen.
- **Keyboard only while the game is in front.** Alt is the co-op key; held for Alt+Tab it started team moves in a loop.

## QA

QA level is `none`: there is no test suite with pass/fail output yet. What was checked, and how:

- A windowed copy of the game runs in the background with scripted controller input and records frames from the
  Direct3D back buffer. A frame-by-frame look at those recordings (kept on the developer's machine, not in this repo)
  showed, on 2026-11-01 and 2026-11-02:
  - Map 5: player 2 as Raphael, Michelangelo and Donatello, each with his own weapon, doing a 4-hit combo, an
    unarmed attack, walking and jumping afterwards.
  - Level 1: player 2 as a copy of Leonardo doing the same; a team move started by player 2 and one started by player
    1, both turtles staying in the level and playable afterwards; no team move starting by itself.
- The controller path was driven by a virtual pad in the test copy; real controllers (Xbox, Switch 2 Pro) were last
  played by hand on an earlier build.
- An older self-test script covers only the start of Level 1.

## How to play

Not released.

## Known issues

- Covered: Level 1 and Map 5. Rest of the campaign (later chapters, vehicle chases, bosses): not covered yet.
- In levels that load only one turtle, player 2 is a Leonardo copy: katanas and Leonardo's moves, whatever hero is
  picked.
- Cutscenes, checkpoint reloads and level changes may lose or misplace player 2; he can be brought back by hand.
- A crash at `TMNTGame.exe+27A2AE` (a call through a null pointer) was seen twice on 2026-10-29, the second time right
  after team moves had started over and over; that loop is fixed, but the crash itself was not reproduced or explained.
- Picking or switching heroes while a turtle is climbing, wall-running or in the air can snap positions.

## Next steps

- A test script with pass/fail output (states reached, positions, which object player 2 is) so the QA level can rise.
- Load the other brothers' data into single-turtle levels, so player 2 can be a real Raphael, Michelangelo or Donatello
  there too.
- Try the rest of the campaign, cutscenes and level changes.
- Track down the `27A2AE` crash.
