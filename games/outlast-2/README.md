# Outlast 2 (2017)

| | |
|---|---|
| Engine | Unreal Engine 3 (custom UE3.5, 64-bit DirectX 11) |
| Versions tested | Steam buildid 2601375 (12 Mar 2018 update), SHA-256 `F7993204F4A865AAE46473A7B5AF59D6D0F1F8DD134158157CD95AA726554107` |
| Projects | (none found for Outlast 2; documented gap). Sibling project: [OpenOL](https://github.com/ShyKiss/OpenOL) for Outlast 1 |
| QA level | none |
| Last updated | 2026-10-21 |

## Where things stand

Outlast 2 does not currently have a playable co-op mod; it is documented here as an active gap. While the game shipped strictly as single-player, its binary retains Unreal Engine 3 replication structures and native network driver classes (`UNetDriver`, `TcpNetDriver`).

A proxy hook layer now exists in `games/outlast-2/src/` (`dinput8.dll` + manual trampoline hook on `UObject::ProcessEvent` at RVA `0x4DA80`), deployed to the isolated test install (`local/installs/instance1`). Hook installation and target validation are **verified working** (see QA); live interception of the first engine event is the current open question.

## Existing work

- **Outlast 2 Co-op Mods:** None found. Exhaustive searches across Nexus Mods, GitHub, ModDB, and Reddit show no community multiplayer projects were previously created for Outlast 2.
- **O2menu (Pyraciste):** A single-player trainer / cheat menu on Nexus Mods using `O2Inject.exe` to inject a DLL into `Outlast2.exe` at runtime, proving in-process hooking and memory manipulation feasible.
- **OpenOL & OpenOL-Relay (ShyKiss):** For the original *Outlast* (UE3 32/64-bit), ShyKiss developed a modified client and UDP relay server architecture that synchronized player puppets, doors, enemy attacks, and items, and maintained room snapshot saves.
- **The Outlast Trials (Red Barrels):** Official standalone 4-player co-op horror title released in 2023–2024 built on Unreal Engine 4.

## How the game works (for co-op)

- **Engine & Scripts:** Outlast 2 runs on a heavily customized Unreal Engine 3 (build 17517) in 64-bit. Game logic is compiled into UnrealScript bytecode inside `.upk` packages located in `OLGame\CookedPCConsole` (`OLGame.upk`, `Engine.upk`, `GameFramework.upk`, `IpDrv.upk`).
- **Netcode:** The binary `Outlast2.exe` compiles UE3's `UNetDriver`, `UNetConnection`, `UPackageMap`, `UChannel`, and `IpDrv.TcpNetDriver`, but game maps and Kismet sequence triggers were cooked without network replication flags.
- **Characters & Mechanics:**
  - Blake Langermann (`OLHero`) possesses unique mechanics: directional microphone sound meter, camcorder night vision and zoom, stamina, dynamic sliding, crawling, swimming/underwater diving, and bandages/bleeding.
  - Companions and enemies run custom AI behavior trees (`OLBot`, `OLEnemyPawn`).
- **Level Transitions & Saves:**
  - Levels are loaded via streaming sublevels (`PersistentMaps` such as `School_Persistent`, `Crash_Persistent`, `VSlice1_Persistent`).
  - Checkpoints are managed by `X_Checkpoints.upk` and local profile saves (`SaveData/*.olprofile`).

## What we changed

The proxy DLL (`dinput8.dll`) and hook (`UObject::ProcessEvent` at RVA `0x4DA80`) are built and deployed to the isolated test install (`local/installs/instance1`) via `games/outlast-2/src/build.bat`. No changes have been made to the owner's game install or saves.

The networking stack is also in place in `games/outlast-2/src/`, compiled into the DLL but not yet driven by the game: sync protocol (`coop_protocol.h`), UDP transport (`coop_net.c`), relay session table (`coop_session.c`), the host/client relay pump (`coop_relay.c`) and remote-puppet interpolation (`coop_interp.c`). Host-authoritative: clients send their pawn state, the host relays it, owns slot assignment and answers late joins with a snapshot; remote puppets are played back through a 100 ms jitter buffer.

## QA

- Proxy hook install verified in live isolated runs (3/3 clean: prologue OK, jump applied, no crash).
- PE static validation: RVA `0x4DA80` confirmed as genuine function start (`.pdata` RUNTIME_FUNCTION, size 812).
- Live `ProcessEvent` interception: not yet observed (see `games/outlast-2/NOTES.md` §6).
- Offline protocol test: `games/outlast-2/tests/run.bat` — packed layout, round-trip, flag invariants — **PASS** (2026-10-24, 5 groups).
- UDP loopback test: same command — self round-trip, two-socket delivery, recv timeout — **PASS** (2026-10-24, 3 groups).
- Relay session test: same command — join/find, capacity/slot reuse, touch/expire, relay fan-out, late-join snapshot — **PASS** (2026-10-24, 5 groups).
- Relay end-to-end test: same command — real host + two clients over UDP loopback (join, state relay, late-join snapshot), host-owned slot assignment, unknown-sender rejection, malformed-packet rejection, silent-player expiry — **PASS** (2026-10-24, 5 groups). 18 check groups total.
- Interpolation test: same command — playback delay/lerp, stale+reordered packet rejection, loss gaps, extrapolation cap, teleport snapping, shortest-arc angles, scripted lossy/reordered fake network — **PASS** (2026-10-24, 6 groups). 24 check groups total.
- These are offline/loopback tests of the sync stack; no automated test runs two game clients yet.
- Directory and sync testing tools verified via `tools/build_directory.py`.

## How to play

Not yet playable. Official co-op gameplay in the Outlast universe is available in *The Outlast Trials*.

## Known issues

- Single-player campaign design: Scripted sequences and jump scares assume exactly one protagonist and camera perspective.
- Level streaming triggers and Kismet sequences require a host-authoritative sync layer to prevent sequence breaking.

## Next steps

1. Build a proxy DLL (hooking `xinput1_3.dll` or `d3dcompiler_46.dll`) that loads MinHook into `Outlast2.exe`.
2. Hook `UObject::ProcessEvent` to log events and intercept `OLPlayerController` / `OLHero` ticks.
   - Status: hook installed and target validated; first live interception still pending (NOTES §6).
3. Locate `GObjects` and `GNames` to dynamically query world actors and pawn instances.
4. Implement a lightweight local UDP transport and spawn a puppet `OLHero` mesh for player 2.
