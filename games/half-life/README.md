# Half-Life (1998)

| | |
|---|---|
| Engine | GoldSrc |
| Versions tested | Steam buildid 15961492 (Windows) |
| Projects | HLSDK co-op mod (own, learning project); Sven Co-op (existing) |
| QA level | automated |
| Last updated | 2026-10-23 |

## Where things stand

A from-scratch HLSDK co-op mod makes the single-player campaign playable by two players
over LAN, and it is covered by an in-DLL automated test bot that drives two real game
clients and asserts the shared state. Milestones M1 (build + load), M2 (second player on an
SP map) and M3 (level transitions, NPCs, economy sync) are done. This is a learning project,
not a replacement for the mature Sven Co-op.

## Existing work

- **Sven Co-op** — mature, playable, free standalone Steam mod (app 225840) with online/LAN
  co-op. See its directory entry.
- **HLSDK co-op mod (this)** — a minimal re-implementation of the classic GoldSrc approach:
  build `client.dll` + `hl.dll` from the public HLSDK and fix the SP mechanics that assume one
  player.

## How the game works (for co-op)

GoldSrc is a client/server engine. The server DLL (`hl.dll`) is open source
([ValveSoftware/halflife](https://github.com/ValveSoftware/halflife)), so co-op is mostly
entity/trigger/game-rules fixes rather than reverse engineering. Single-player maps force the
engine to clear `deathmatch`, so the key change is to make the server run multiplayer rules on
SP maps and then re-enable the SP story mechanics (level changes, monsters, item economy) that
are gated on deathmatch mode.

## What we changed

All changes are in the server DLL (`hl-sdk/dlls/`). Each fix is proven by the QA bot unless noted.

- **Multiplayer rules on SP maps** (`gamerules.cpp`): `InstallGameRules()` returns
  `CHalfLifeMultiplay` when `gpGlobals->coop` is set, before the SP branch. This is what lets a
  second player join a single-player map.
- **Level transitions** (`triggers.cpp`): `trigger_changelevel` was skipped in multiplayer
  (`IsDeathmatch()` is TRUE in co-op). The guard is now `IsDeathmatch() && !IsCoOp()`, it checks
  the activating player instead of hardcoded entity 1, and in co-op it uses a fresh
  `CHANGE_LEVEL(map, NULL)` because the engine rejects `changelevel2` in multiplayer.
- **Monsters spawn** (`multiplay_gamerules.cpp`): `FAllowMonsters()` returns TRUE in co-op so NPCs
  are not killed at spawn (the default `mp_allowmonsters 0` would otherwise remove them all).
- **SP-style economy + auto-duplication** (`multiplay_gamerules.cpp`): players are teammates, no
  friendly fire, no deathmatch scoring, weapons/ammo kept on death, and a pickup by one player is
  copied to the others (`PlayerGotWeapon`/`PlayerGotAmmo`/`PlayerGotItem`).
- **Monster friendly fire** (`player.cpp`): the "don't shoot allies" rule now also applies in
  co-op (`!IsDeathmatch() || IsCoOp()`).
- **Boss AI targets the nearest player** (`util.cpp`, `apache/osprey/gman/nihilanth.cpp`): replaced
  the "first connected player" pattern with `UTIL_FindNearestPlayer`.
- **Automated QA bot** (`coop_qa.cpp`): a server-side `coop_qa` entity that asserts co-op mode,
  two-player spawn, monster spawn, and item auto-duplication, then logs `QA SUITE: pass=N fail=M`.

## QA

Run the suite (starts a listen-server host + a second, mutex-patched client on one PC, then
collects `qconsole.log`):

```
tools\qa-coop.cmd
```

Last result (2026-10-23, `c1a1`, two real clients):

- co-op run: `QA SUITE: pass=5 fail=0` → **PASS**
- control run (`+coop 0`): `QA SUITE: pass=1 fail=4` → **FAIL** (proves the suite detects co-op
  behaviour and is not a hollow pass)

The five gates: two players connected, co-op mode active, two players spawn at separate points,
monsters survive spawn, item auto-duplication reaches the second player. Level transitions were
verified end-to-end once but are not yet a repeatable gate.

## How to play

- Host: `hl.exe -game valve -insecure +sv_lan 1 +coop 1 +maxplayers 2 +map c1a0`
  (`+sv_lan 1` is required so two players don't need two Steam accounts).
- Second player (same PC): run the mutex-patched `hl2.exe` copy:
  `hl2.exe -game valve -insecure +connect <host-ip>`.
- Second player (another PC on the LAN): `hl.exe -game valve -insecure +connect <host-ip>`.

## Known issues

- Level transitions use a fresh map load, so players respawn at the new map's `info_player_start`
  rather than at the landmark (SP parity not exact).
- HUD / first-person view is implied by multiplayer rules + vanilla `client.dll` but not visually
  confirmed.
- `hlds.exe` crashes on the test machine (access violation at startup), so a dedicated-server
  test is blocked; a listen server works.
- Boss-AI nearest-player targeting is compile + smoke tested only (late-game, not exercised by
  the early maps).

## Next steps

- Dedicated `info_player_coop` spawn points instead of reusing `info_player_start`.
- Carry players to the landmark after a level transition.
- Visual confirmation of HUD / first-person view for both players.
- Runtime test of boss AI against two players on a late map.
- Publish the mod code to its own repo and link it from the directory entry.
