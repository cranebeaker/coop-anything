# FTL: Faster Than Light (2012)

| | |
|---|---|
| Engine | Subset Games' own C++ engine (SDL, OpenGL); Linux builds ship with DWARF debug info |
| Versions tested | Steam, Linux native, 1.6.13 (`FTL.amd64` SHA-256 `23897b7e…57d80`, Steam build id 4710954) |
| Projects | [FTL Co-op](https://github.com/SCM69/FTL-Hyperspace/tree/coop) (Hyperspace fork, branch `coop`); no earlier co-op mod exists |
| QA level | automated (bots in 2 to 8 real game copies on one PC; see QA) |
| Last updated | 2026-11-06 |

## Where things stand

A lockstep co-op mod for up to 8 players runs in the real game: every copy simulates the same ship, players' commands
are scheduled by the host and applied on the same frame everywhere, and state hashes agree across 2, 4 and 8 copies
through jumps, events and fights. It is not released yet: there is no in-game lobby, only direct TCP connections
(port forwarding or a VPN for internet play), and only Linux 1.6.13 has been tested.

## Existing work

- [tomahawk812/FTL---Faster-than-light-Multiplayer](https://github.com/tomahawk812/FTL---Faster-than-light-Multiplayer):
  README-only stub, last push 2013-09-06.
- [Tachyon / TachyonsCE](https://github.com/Dav-Edward/TachyonsCE): a separate multiplayer game in FTL's style, not a mod.
- [Final Frontier](https://www.pcgamer.com/ftl-garrys-mod/): a Garry's Mod gamemode, a separate first-person game.
- Subset Games: no plans for multiplayer ([quoted here](https://playco-opgame.com/ftl-faster-than-light/)).
- [FTL-Hyperspace](https://github.com/FTL-Hyperspace/FTL-Hyperspace) (GPL-3.0-or-later): active binary hook framework,
  not multiplayer. The co-op mod is built on it, so Hyperspace content mods (Multiverse) are possible later.

## How the game works (for co-op)

- No netcode. One player ship, at most one enemy ship per beacon, a sector map, event dialogs, stores.
- Hyperspace loads as `LD_PRELOAD` on Linux and hooks functions by byte signature (`libzhlgen/.../1.6.13/*.zhl`).
- Time: `CFPS::OnLoop` sets `SpeedFactor = seconds since last frame × 16` (max 0.5); `RunningTime` adds it up only
  while unpaused. A fixed 16/60 per frame makes simulated time identical on every copy.
- Randomness: `random32` (an LCG, state `_sil_random_state` at 0xa35ff0 in the non-PIE binary) does almost all of it;
  libc `rand()` is interposed but was not called in the tested runs. Several cosmetic systems draw `random32` a varying
  number of times depending on rendering (Animation::Update, ParticleEmitter::OnUpdate, SpaceManager::OnLoopNebulas);
  co-op saves and restores the state around them.
- Pause: `CommandGui::IsPaused() = bPaused || menu_pause`, and `menu_pause` is set while any window (store, ship
  screen, map, menu) is open. Co-op patches that out and pauses only for shared things (event dialogs, game over,
  the explicit pause command).
- Player actions and AI share the same functions (`ShipManager::CommandCrewMoveRoom` is called by the crew AI too),
  so actions are taken over only while an input event is being handled.
- Saves: `WorldManager::SaveGame()` writes the continue save, `WorldManager::LoadGame(file)` reads it (the main menu's
  Continue also calls `TutorialManager::Stop()` and closes the menu).
Details, addresses and sources: [NOTES.md](NOTES.md).

## What we changed

The mod is a Hyperspace fork; the co-op code is in `src/features/coop/`.
- Lockstep core (game-independent C++11): the host collects commands, filters them by the ship-sharing mode, schedules
  them for frame N + delay and sends one bundle per frame; copies run frame N only when they hold its bundle; hashes
  every 30 frames; the host leads by at most 12 frames; restarts carry an epoch so stale messages are dropped.
- Ship sharing is a host setting, switchable in a run (F11): free-for-all, or own crew (players command their crew and
  the systems those crew staff; unstaffed systems belong to the captain). Jumps, event choices and the mode are the
  captain's (host's).
- Commands: crew move, system power, weapon and drone power, fire at target, clear target, autofire, doors, pause,
  event choice, jump, ship-sharing mode, crew owner.
- Resync: the host saves, sends the save, and every copy (the host too) reloads it and restarts lockstep. Used after a
  detected desync, for a late joiner, and to continue a saved run (tested: forced resync mid-combat, late join;
  continuing a saved run is untested).
- Fixes found by tests: listed with their evidence in [NOTES.md](NOTES.md) (blocking dialogs in headless runs, first
  launch language chooser, hook return types, catch-up desyncs from cosmetic randomness, a crash when jumping with the
  store open, a frozen game while a store window was open).

## QA

Test tools (mod repo, `tests/coop/`):
- `fakeworld_test.cpp`: the lockstep core over real loopback TCP with a toy simulation: 2/4/8 players, both modes,
  30 seeds, restart, late join, and mutation checks (double apply, missing epoch check) that must fail.
- `run_copies.sh`: starts N hard-linked copies of the game under headless gamescope, each with its own save folder,
  runs an in-game bot, and compares the copies' hash logs with `compare_logs.py` (self-test: `test_compare_logs.sh`).
- Bots: `probe` (captain answers dialogs and jumps), `chaos` (every copy issues random moves, power, doors, weapon power,
  pauses), `fight` (chaos that keeps weapons powered and fires at enemy rooms), `keys` (real hotkeys through the game's
  input path). Test switches: `FTL_COOP_TEST_HITCH` (a guest falls behind and catches up), `FTL_COOP_RNGTRACE`
  (random32 callers per hash frame, diffed with `trace_diff.py`).

Last results (2026-11-06, mod commit 262a0a7, Linux Steam 1.6.13, one PC; full table in NOTES.md):
- Fight bot (chaos plus weapons kept powered and fired at enemy rooms), 2, 4 and 8 copies, both sharing modes, up to
  10800 frames (3 minutes of game time) through jumps, events and fights where ships were destroyed: 6 of 6 runs PASS
  after the last fix (all hash checks equal, every copy reached the end frame); 8 copies: 6306 commands, 240 hash checks.
- Chaos bot before the last fix (start beacon, 3600 frames): 2 copies 10/10 seeds, 4 copies own-crew, 8 copies: PASS.
- Forced resync mid-combat (2 copies) and a late joiner (3rd copy at frame 706): PASS, state kept across the reload.
- Keys bot through the real input path: 93 commands from hotkeys, PASS.
- Fake world: 14/14 checks, including two mutation checks that must fail and do.

## How to play

Not released yet. Developers can build the branch and run it as described in its
[COOP.md](https://github.com/SCM69/FTL-Hyperspace/blob/coop/COOP.md).

## Known issues

- Direct TCP only (default port 27890): internet play needs port forwarding or a VPN. No lobby or invites yet.
- Configuration is by environment variables or `coop.cfg` next to `FTL.amd64`; no menu yet.
- Store purchases, upgrades, crew abilities, hacking, mind control, teleporting and sector-exit choices are not yet
  commands: when a guest uses them only their copy changes, the host detects a desync and resyncs everyone.
- Only tested on Linux 1.6.13 (Steam).

## Next steps

- Remaining actions as commands (store, upgrades, teleporter, hacking, mind control, cloaking, crew abilities, sector map).
- Steam lobbies and relayed P2P instead of direct TCP.
- An in-game host/join screen; per-player crew markers in own-crew mode.
- Windows 1.6.9 (Hyperspace supports it; the random32 and menu_pause addresses differ).
