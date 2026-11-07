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
through jumps, events and fights. Players connect through a friends-only Steam lobby (no addresses or port
forwarding; direct TCP is the fallback). It is not released yet, and it has not been played between two real Steam
accounts or on real Windows.

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
  cloaking, battery, hacking (pulse, target), mind control, teleporter, event choice, jump, ship-sharing mode, crew owner.
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

Last results (2026-11-06, mod commit b36cb9e, Linux Steam 1.6.13, one PC under headless gamescope; full table and the
failures that led to fixes in NOTES.md). Final regression batch, 9 of 9 PASS (all hash checks equal, every copy at the
end frame):

| Bot | Copies | Mode | Frames | Commands |
|---|---|---|---|---|
| fight | 2 | free-for-all | 10800 | 4586 |
| fight | 4 | own crew | 7200 | 1316 |
| fight | 8 | free-for-all | 7200 | 6557 |
| specials (cloak, battery, hacking, mind control) | 2 / 4 | own crew / free-for-all | 7200 | 1287 / 4704 |
| keys (real hotkeys through the input path) | 2 | free-for-all | 3600 | 61 |
| chaos | 2 | free-for-all | 3600 | 798 |
| fight + forced resync at frame 1200 | 4 | free-for-all | 3600 | 2198 |
| captain (host upgrades in the ship screen, guest blocked) | 2 | free-for-all | 1800 | resync, last epoch equal |

Soak: 2 copies for 10 minutes of game time (8232 commands, 1200 hash checks) and 4 copies own-crew for 6 minutes
(3776 commands): PASS.

Latency (test switch delaying every message): 2 copies at 0 / 200 / 300 ms round trip run at full speed (35-36 s for
1800 frames) with an adaptive input delay of 3 / 9 / 12 frames; 4 copies at 160 ms, resync at 120 ms and late join at
100 ms round trip PASS.

Steam transport (mod commits 8495111, 5fd7f2e): `steamnet_test.cpp` with a fake Steam, 12/12 (8 players found
through the friends list, a 3 MB resync in 128 KB packets with 20% of sends failing and retried, members leaving,
host leaving, picking the right lobby among a wrong build and a non-co-op lobby, late member-list updates, joining
by id, full lobby). Real game copies over the Steam transport with `tests/coop/fakesteam_hub.py` standing in for Steam,
all PASS: Linux 2 fight and 4 chaos own-crew; Windows under Proton 2 fight, 8 chaos own-crew, and a late join with
80 ms hub lag (resync, rtt 185 ms). Real Steam, one account: lobby created and read back on Linux and on Windows
under Proton; the friends list reads.

Also PASS on the same day: late join (3rd copy at frame 706), a guest disconnecting (host and the other guest go on,
80 hash checks equal), continuing a saved run (mid-combat save), the Linux installer (install, launch through the
game's start script, uninstall restores it). Fake world: 15/15 checks including three mutation checks (double apply,
missing epoch check, restart from inside the desync callback) that fail as they must. Scorer self-test: 12/12.

## How to play

Not released yet (no prebuilt download). Linux players can build the branch and install it with the included
`coop-install.sh` (checks the game version, needs no Hyperspace.ftl), see
[COOP.md](https://github.com/SCM69/FTL-Hyperspace/blob/coop/COOP.md). Host: `role=host`, `players=N` in `coop.cfg`;
guests: `role=guest`; the host starts first, guests start the game and join the Steam friend who hosts
(`friend=<name>` picks one). `net=tcp` with `address=<host>:27890` connects directly instead. Host keys: F9 start,
F10 resync, F11 sharing mode, F7 give crew.

## Known issues

- Steam lobby (default) or direct TCP. No overlay invites or "Join Game" from the friends list yet: guests start the
  game themselves and join a hosting friend automatically. Steam has only been checked with one account (lobby
  create, data, friends list); two-account play is untested. With direct TCP, internet play needs port forwarding or a
  VPN. Latency is handled by an input delay the host adapts to the measured round trip (tested up to 300 ms round trip at
  full speed); at high latency clicks take effect a fraction of a second later.
- Configuration is by environment variables or `coop.cfg` next to `FTL.amd64`; no menu yet.
- Store, upgrades, equipment and the crew screen are the captain's: guests' clicks there are ignored, and when the host
  closes them after a change everyone reloads the host's save (a short pause). Sector-exit choices, crew abilities of
  Hyperspace races and a few rarer actions are not commands yet; using them makes the host resync everyone.
- A resync waits for a quiet moment (no jump, no event dialog), so it can take a few seconds.
- Only tested on Linux 1.6.13 (Steam). The Windows 1.6.9 build (mod commit cd89dfc) passed the bot tests under
  Proton Experimental on Linux (2 to 8 copies, see NOTES); not yet run on real Windows. Windows and Linux players cannot
  share a game (different programs, so the simulations differ); a Linux player can use the Windows game under Proton.

## Next steps

- Remaining actions as commands (store, upgrades, teleporter, hacking, mind control, cloaking, crew abilities, sector map).
- Play over Steam between two real accounts (two PCs); add overlay invites and "Join Game".
- An in-game host/join screen; per-player crew markers in own-crew mode.
- Run the Windows 1.6.9 build on real Windows with players.
