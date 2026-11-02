# TMNT (2007, PC)

> **Work in progress, not stable.** This is an experimental prototype under active development. It is not
> released and not ready to play; expect missing moves, disabled features and occasional crashes.

| | |
|---|---|
| Engine | Ubisoft Jade ("Engine v428-42" in the game's log; gameplay scripts compiled into the exe) |
| Versions tested | Retail PC, `TMNTGame.exe` 1.0.0.188 (UPX-packed, SecuROM traces), SHA-256 `701d9f00f8c1edf790466f2ecfeb72a64fe7ccdc491f7c370ae11c1942740589`; store/edition unknown |
| Projects | Local couch co-op by DLL injection (private, in progress; no public repo yet) |
| QA level | none (no structured or automated testing yet) |
| Last updated | 2026-10-25 |

## Where things stand

A second player can play a second turtle on screen at the same time as player 1, in one game process on one PC with
two controllers. Movement, jump, both attacks, health, enemy targeting, respawn and a shared midpoint camera were seen
working in manual runs during development, not in a structured test. Player 2 does not yet have full move parity (double jump, combos, dodge, moves that need a run-up),
and team moves are switched off while player 2 is present. Not released.

## Existing work

- No co-op mod for the PC game was found (GitHub, Nucleus Co-op handler hub, web search; 2026-10-12). Only the PSP and
  DS versions had multiplayer.
- [xan1242/UbiTMNTHax](https://github.com/xan1242/UbiTMNTHax): ASI plugin for the same exe (windowed, skip launcher,
  multiple instances). Not co-op, but a reference for hooking this build.
- [hogsy/jaded](https://github.com/hogsy/jaded): community fork of the 2006 Jade engine source; useful for structures,
  does not load TMNT data.

## How the game works (for co-op)

Details and addresses (for the build above) are in [NOTES.md](NOTES.md).

- Normally only one turtle is on screen; a brother appears briefly for team moves (Brother Throw, Super Family
  Attack). Turtle Swap only changes which turtle object is "main actor 0"; the other turtle objects stay alive.
- The engine has two main-actor slots (the second is empty in solo play) and per-turtle "IO ports", but a port is only
  an on/off gate: every present turtle reads the same shared input object.
- Input goes through Ubisoft's Gear input library (DirectInput 8 and XInput 1.3). The game merges all pads into one
  input state, so a second physical pad simply also controls player 1.
- The launcher's mutex blocks a second instance, so multi-instance split screen is not an easy route.

## What we changed

All in an injected DLL (a host DLL injected once, plus a hot-reloadable mod DLL swapped while the game runs). No game
files are modified.

- **A second turtle:** the game's own turtle spawn function places an idle brother next to player 1 with its port open.
- **Its own input:** around player 2's turtle update, the shared stick and action reads are answered from player 2's
  pad; camera-relative movement. Player 2's DualSense is read over HID directly and hidden from the game's
  DirectInput list (otherwise the game picks it as pad 1 or merges it into player 1).
- **Enemies target player 2:** in the enemy AI tick, main actor 0 is swapped to player 2 when that enemy is closer to
  it, and restored after.
- **Health bar** for player 2 (D3D9 EndScene hook on the game's real device), bound to the turtle's 0-100 health.
- **Camera:** looks at the horizontal midpoint of both turtles; player 2 is leashed to player 1 and brought back if it
  falls into a pit; auto-respawn after death.
- **Team moves off** while player 2 is present: after a team move the game turns the partner into a scripted helper
  (inside the obfuscated code) that flies it away and marks it absent.

## QA

No automated suite yet. The results above come from manual runs during development, not a test. Next step is a pad-2 input script and a log that compares
player 1's and player 2's move states for the same input (see the playbook's automated QA page and the Two Towers
write-up).

## How to play

Not released.

## Known issues

- Player 2 lacks some moves that depend on input history (double jump, combos, dodge, run-up moves).
- Team moves are disabled while player 2 is in the level.
- Brothers still unlock mid-level as in the base game.
- Level loads and checkpoint reloads with player 2 are untested; player 2 may not survive them.
- The game crashed once while a new mod build was hot-loaded during play; mod builds change often.

## Next steps

- Input parity the way [The Two Towers](../lotr-two-towers/NOTES.md) did it: give player 2 its own copy of the
  game's input object (a virtual second pad record), let the game's own poll fill it, and run player 2's control step
  with that copy swapped in.
- A pad-2 replay test that asserts player 2 reaches the same move states as player 1.
- Team moves with player 2 as the partner.
