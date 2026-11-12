# TMNT (2007, PC)

> **Work in progress, not finished.** Two players can play on one PC, and every story mission starts and passes a
> scripted test (join, movement, fights, revives, team moves, restarts). No mission has been played through to its end
> with two players, level changes are untested, real controllers were last checked by hand on an older build, and
> there is no public release.

| | |
|---|---|
| Engine | Ubisoft Jade ("Engine v428-42" in the game's log; gameplay scripts compiled into the exe) |
| Versions tested | Retail PC, `TMNTGame.exe` 1.0.0.188 (UPX-packed, SecuROM traces), SHA-256 `701d9f00f8c1edf790466f2ecfeb72a64fe7ccdc491f7c370ae11c1942740589`; store/edition unknown |
| Projects | Local couch co-op by DLL injection (private, in progress; no public repo yet) |
| QA level | automated (scripted tests per mission, see QA) |
| Last updated | 2026-11-06 |

## Where things stand

A second player controls a second turtle in the same game process, on one screen, with a second controller. Where a
mission has the other brothers loaded (7 of the 16 story missions), player 2 plays the real Raphael, Michelangelo or
Donatello with his own weapon and combos; in the other 9 he is a copy of player 1's turtle in another bandana colour.
All 16 story missions start with player 2 and pass the scripted checks in the QA section. Not covered: playing a
mission to its end, level changes, cutscenes, bosses, real controllers on the current build. Not released.

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
- **Otherwise a copy of player 1.** In single-turtle levels the engine's object duplicate makes a second copy of player
  1's turtle, with its own skeleton unshared from player 1's. The chosen hero only changes the bandana colour. An earlier version also
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
- **Player 2 appears on player 1's own spot** (joins, recoveries, team-move returns), with body pushing off until
  they move apart. A spot beside player 1 could be over a drop: in Level 1 the copy fell, was downed and freed on
  every join.
- **The copy is kept, not freed, when a level reloads.** A copy of player 1 still shares some of player 1's data
  (animation data among it); freeing it corrupted the heap and the next copy crashed the game. Before a restart or
  level change (`ChangeMap_Execute`, `0x005F4C70`), at player 1's death and when the copy is downed, the copy loses
  the Generated flag, so the reload only hides it, and it is used again. Restarts with a copy as player 2 crashed 5
  of 27 times before (Map 14 every time). The Leonardo copy's duplicated katana sheath is no longer attached: kept
  through a restart it was drawn after being freed.
- **Hazards.** Toxic water (turtle state 26) returns player 1 to safe ground after about 3 s, but player 2 stayed in
  it until he was downed (about 11 s). After 2.5 s in the water he now joins again beside player 1.
- **Six guard hooks from an earlier version removed.** They returned early for any "actor" value below
  `0x00400000`, but the game's scripts pass actor codes 1, 2 and 3 there (main actor 0, main actor 1, current object).
  Fighting enemies crashed the game within 3 to 5 combos with them, with player 1 alone too; found by removing the
  guard hooks in halves.
- **Main actor 0 is never swapped to null.** Enemies near player 2 target him by swapping main actor 0 around their
  AI. After player 2 left while player 1 was down, that swap wrote null and the next script crashed
  (`TMNTGame.exe+2B3E91`).

## QA

QA level is `automated`: scripted tests drive a real copy of the game. Scope: per-mission checks, stress runs and
crash hunts; no full playthrough.

How it runs: a windowed copy of the game (separate install and saves) runs in the background. A command channel into
the mod drives player 2 through the same input path as a real second controller, and player 1 through a virtual pad;
results are judged from the mod's log and the game process. Only a game that stopped counts as a crash. The scripts
are in the private project (`suite.py`, `depth.py`, `stress.py`, `p1_death.py`, `combo_crash.py`).

- **Per-mission suite, 16 missions x 11 checks** (boot, playable, join and drawn, walk, jump, input isolation both
  ways, attack, leave and rejoin, leave while player 1 is down, restart from the pause menu, no crash). Last run,
  2026-11-06 06:00 on the current build: **175 of 176 pass**. The one failure: in Map 8 player 1 was staggered by an
  enemy when the isolation check asked him to jump. Map n is story mission index n, 0-15.
- **Does the suite notice breakage?** Five deliberate faults, each on Map 0 (copy) and Map 5 (brother): shared input
  and a dead stick were caught in both; a skipped player-2 recovery and the old null swap (it reproduced the
  `2B3E91` crash) were caught where those paths run; skipping the mod's hide on leave was not, because the game hides
  a turtle that leaves by itself.
- **Restarts with a copy as player 2** since the restart fix: 65 of 66 in dedicated runs (10 each in Maps 14, 7, 1
  and 4, 10 more in Map 14, 15 in Map 0). The failure was in Map 0 and led to removing the Leonardo copy's katana
  sheath; Map 0 then passed 10 of 10. With a real brother: 10 of 10 (Map 5). Plus one restart per mission in every
  suite run.
- **Fights:** player 1 alone fighting the opening enemies of Map 8 crashed in 5 of 5 runs within 3 to 5 combos before
  the guard hooks were removed; 6 of 6 runs of 12 combos without a crash after (player 1 and player 2).
- **Co-op depth** (`depth.py`): player 1 revives player 2 (Map 5) and player 2 revives player 1 (Maps 5, 0, 8); a
  team move started by each player, both ending on the ground and in control (Maps 5 and 0; Map 8 has no team moves,
  player 1 is Nightwatcher there); both in
  frame with player 2 12-14 units away; player 2's hits kill enemies (Map 8: the opening enemies died in 2 of 2 runs,
  none in a control run with player 2 idle).
- **Deaths:** player 1 killed by enemies with a copy as player 2 (Map 8): 14 of 14 pass (game running, player 2 back
  after the checkpoint and able to walk); 8 of them were followed by a restart, which also passed.
- **Stress:** 30 leave/join rounds with hero switches, 30 of 30 in Maps 5 and 0; 20 minutes of random input for both
  players in Maps 0, 5, 8 and 14 without a crash (in Map 8 player 2 is away for a few seconds while player 1 dies and
  the checkpoint reloads, as designed).
- **Not covered:** level changes (the retail game never reads its debug actions such as `StartMenu_StartNextMap`, and
  forcing `ChangeMap_Execute` did not produce a working level change), finishing a mission, cutscenes, bosses,
  vehicle sections, real controllers on the current build (a 10-step hand checklist is ready for the owner).

## How to play

Not released.

## Known issues

- Level changes with player 2 have not been tested; whether the kept copy and player 2's return work across a level
  end is unknown.
- A copy of player 1 still shares some of player 1's data. Reloads are handled by keeping the copy instead of freeing
  it; anything else that frees it would hit the same problem.
- In missions that load only one turtle, player 2 is a copy of player 1's turtle: same weapons and moves whatever hero
  is picked; only the bandana colour changes. The Leonardo copy has no katana sheath on his back.
- Player 2 appears on top of player 1 until one of them moves.
- Cutscenes, bosses and vehicle sections: not checked.
- Picking or switching heroes while a turtle is climbing, wall-running or in the air can snap positions.

## Next steps

- A level-change test and a two-player playthrough of a mission.
- A copy of player 1 that shares none of his data.
- The owner's hand test with real controllers on the current build.
- Load the other brothers' data into single-turtle missions, so player 2 can be a real brother there too.
