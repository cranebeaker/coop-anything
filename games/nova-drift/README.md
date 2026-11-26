# Nova Drift (2024)

| | |
|---|---|
| Engine | GameMaker, 64-bit YYC (GML compiled into the exe) |
| Versions tested | Steam, app 858210, build 25890040 (`NovaDrift.exe` 1.2.38.4) |
| Projects | [Nova Drift online co-op](https://github.com/firefliesalco/nova-drift-coop) (directory entry: "Nova Drift online co-op") |
| QA level | automated |
| Last updated | 2026-11-18 |

## Where things stand

2 to 4 players can play full runs together online: each player has their own ship, build and upgrades,
experience is shared, downed players respawn, and the run ends when everyone is down. It works in
automated multi-copy tests and in hand tests on one PC; a real Steam session between two PCs has not
been tested yet.

## Existing work

- No co-op mod or attempt found before this one (repo directory, issues and PRs checked 2026-11-16).
- [Issue #401](https://github.com/cranebeaker/coop-anything/issues/401) (opened 2026-11-17 by another
  contributor) claims Nova Drift with a similar plan; no code linked as of 2026-11-18.
- Prior art for the approach: the Forager mod in this repo (also a GameMaker YYC game).

## How the game works (for co-op)

- No netcode of its own. Single player only; the game already loads Steamworks (`steam_api64.dll`).
- YYC build: no bytecode in `data.win`; game scripts are native functions in the exe. Function names and
  addresses come from the runtime's script table, so the mod hooks game scripts and object events by
  address (MinHook) and is tied to one game build (it checks the exe's timestamp and turns off otherwise).
- One `oPlayer` ship and one `oPowers` "brain" (hull, shield, upgrades, offer pools) per run, and many
  globals. Extra players are a copy of player 1's ship plus their own `oPowers`, and an ownership layer
  routes every instance and variable access to the right player (each instance is tagged with an owner).
- Enemies set `hull` / `hullMax` in their Create event; player helpers are always owned, which separates
  them from enemies.
- Saves go to `%LOCALAPPDATA%\NovaDrift\save.ini` (ini writes, buffer saves, Steam stats/achievements/cloud).

## What we changed

Details, dead ends and how each problem was found are in [NOTES.md](NOTES.md).

- **Lockstep over Steam P2P**: every PC runs the full game; only input travels (aim, thrust/fire as
  actions, ability keys, upgrade-screen actions) with a 3-frame delay; seeds and launch state are
  normalized at run start. A world hash every 120 frames detects desyncs. Up to 4 players; every PC waits
  for all others' input.
- **Lobby**: a Multiplayer entry on the main menu (seats, ship colour sliders, ready/start); the host's
  START screen drives the guests' (they mirror it and cannot change it); the host sets upgrade pool,
  respawn time and an enemy-health multiplier. Works at any window size (layout and mouse mapping are
  learned from the menu itself).
- **Per-player runs**: own build, offer pools, rerolls and banishes per player; shared XP gives everyone
  the same points; the game pauses while one player chooses. Downed players respawn after the host's
  time next to a living player; the run ends when all are down.
- **Controls**: each player's thrust button option is respected; fire/thrust travel as actions, and the
  host's control options are used for the shared simulation (different options broke lockstep).
- **No saving after a multiplayer run starts** (ini, buffers, files, Steam stats, achievements, cloud).

## QA

- `tools/run_pair.ps1` (in the mod repo) starts 2-4 copies of a separate test install on one PC, connected
  over local UDP, with saves redirected to a test folder. Test runs are started through the in-game lobby.
  It compares a per-frame world hash between all copies and fails if the player's real save changed.
- Scripted bots steer, thrust, fire, use abilities and take upgrades; test-only switches end the run or
  drop a ship to 1 hull to exercise respawn and game over.
- Last results (2026-11-18):
  - 2 players, lobby start: `IDENTICAL for 1561 frames`
  - 4 players, lobby start, bots taking upgrades: `IDENTICAL for 2461 frames`; every player received a
    point at each level-up and spent it
  - 4 players to game over: `IDENTICAL for 2281 frames`, one game over, no respawns after it
  - 2 players with opposite thrust-button options: `IDENTICAL for 1501 frames`
- Hand tests on one PC: lobby, colours, respawn, end of run, controls, single-player play unchanged.

## How to play

See the mod's [README](https://github.com/firefliesalco/nova-drift-coop#readme): copy `version.dll` next
to `NovaDrift.exe` (every player), then Main menu, Multiplayer, invite friends through Steam, Ready,
Start Game, START.

## Known issues

- Not yet tested between two PCs over Steam.
- Gamepads are not networked (mouse and keyboard only).
- Works only on Steam build 25890040; the mod turns itself off on other builds.

## Next steps

- A real Steam session between two or more PCs.
- An installer and releases (one download, update and uninstall).
- Gamepad input over the network.
