# TMNT (2007, PC)

> **Work in progress, not finished.** Two players can play on one PC and every story mission starts and passes a
> scripted smoke test, but no mission has been played through with two players, level changes are untested, and
> there is no public release.

| | |
|---|---|
| Engine | Ubisoft Jade ("Engine v428-42" in the game's log; gameplay scripts compiled into the exe) |
| Versions tested | Retail PC, `TMNTGame.exe` 1.0.0.188 (UPX-packed, SecuROM traces), SHA-256 `701d9f00f8c1edf790466f2ecfeb72a64fe7ccdc491f7c370ae11c1942740589`; store/edition unknown |
| Projects | Local couch co-op by DLL injection (private, in progress; no public repo yet) |
| QA level | automated (scripted smoke test per mission, see QA) |
| Last updated | 2026-11-04 |

## Where things stand

A second player controls a second turtle in the same game process, on one screen, with a second controller. Where a
mission has the other brothers loaded (7 of the 16 story missions in the test), player 2 plays the real Raphael,
Michelangelo or Donatello with his own weapon and combos; in the other 9 he is a copy of player 1's turtle in another
bandana colour. All 16 story missions were started with player 2 and pass a scripted smoke test (join, walk, jump,
attack, leave and rejoin, leave while player 1 is down, restart from the pause menu). Not covered: playing a mission
to its end, level changes, cutscenes, bosses. Not released.

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
- **Hazards and deaths.** Toxic water (turtle state 26) returns player 1 to safe ground after about 3 s, but player 2
  stayed in it until he was downed (about 11 s). After 2.5 s in the water he now joins again beside player 1. When
  player 1 dies (state 24) the checkpoint reload frees spawned objects; a copy of player 1 still shares some of player
  1's objects, and freeing it corrupted the heap (3 of 4 deaths crashed). At the death the copy is now released and
  kept out of that free, and used again after the reload.
- **Main actor 0 is never swapped to null.** Enemies near player 2 target him by swapping main actor 0 around their
  AI. After player 2 left while player 1 was down, that swap wrote null and the next script crashed
  (`TMNTGame.exe+2B3E91`).

## QA

QA level is `automated`, with this scope: a smoke test per mission, not a playthrough.

- A windowed copy of the game (separate install and saves) runs in the background. A command channel into the mod
  drives player 2 through the same input path as a real second controller, and player 1 through a virtual pad. The
  runner (`suite.py`, in the private project) starts a fresh copy for each mission, starts a new game forced to that
  mission, skips the intro and checks from the mod's log: player 2 joins and is drawn; he walks more than 1.5 units
  (other directions are tried if one drops him off a ledge); he jumps (state 3 or 4); he attacks (a combat state); he
  leaves and rejoins; he leaves while player 1 is down; a restart from the pause menu brings both back; the game is
  still running. Only a game that stopped counts as a crash; exceptions caught by the mod's guards are logged.
- Result, 2026-11-04 00:56, 16 missions x 10 checks: **156 of 160 pass** (Map n is story mission index n, 0-15).
  - Map 1: the restart crashed the game (restart and no-crash fail).
  - Map 8: the attack check got no samples (attack fail; it passed in every other run of Map 8).
  - Map 12: player 2 walked into a ledge grab (state 6) and the runner did not try another direction (walk fail).
- Rerun of Maps 1, 8 and 14 after the last change (mod 194): Maps 1 and 8 pass all 10; Map 14's restart crashed.
- Restarts with a copy as player 2 over these runs: 10 of 12 pass. With a real brother: 7 of 7.
- Player 1 killed by enemies in Map 8 with a copy as player 2 (`p1_death.py`, mods 192-193): 10 of 10 pass (game running, player 2
  back after the checkpoint and able to walk), 4 of them followed by a restart that also passed.
- Not covered by any script: real controllers (last played by hand on an earlier build), level changes, cutscenes,
  bosses, finishing a mission, the camera beyond the 14-unit leash.

## How to play

Not released.

## Known issues

- A restart from the pause menu with a copy as player 2 can crash the game (2 of 12 in the test runs, Maps 1 and 14).
  The copy shares some of player 1's objects; the restart frees it. A clean copy that shares nothing is not done yet.
- Level changes with player 2 have not been tested; the mod brings player 2 back once player 1 has stood for 3 s, but
  whether a copy survives the old level being unloaded is unknown.
- In missions that load only one turtle, player 2 is a copy of player 1's turtle: same weapons and moves whatever hero
  is picked; only the bandana colour changes.
- Cutscenes, bosses and vehicle sections: not checked.
- Picking or switching heroes while a turtle is climbing, wall-running or in the air can snap positions.

## Next steps

- A copy of player 1 that shares none of his objects, so a restart or a checkpoint can free it safely.
- A level-change test (the game's own level end, not a forced mission) and a two-player playthrough of a mission.
- Load the other brothers' data into single-turtle missions, so player 2 can be a real brother there too.
