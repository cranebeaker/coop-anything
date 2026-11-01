# Alien: Isolation (2014)

| | |
|---|---|
| Engine | Cathode (Creative Assembly proprietary), 32-bit x86 |
| Versions tested | Steam build 4940658, AI.exe SHA-256 `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90` |
| Projects | Dropper7894 Survivor Mode co-op (unreleased); Joshiloveyou presence prototype ([#85](https://github.com/cranebeaker/coop-anything/issues/85), PRs #156/#164); [alien-isolation-coop](https://github.com/phoenixd123/alien-isolation-coop) steamwrap foundation (this write-up) |
| QA level | none (for co-op); the foundation has an offline test plus a boot test |
| Last updated | 2026-11-01 |

## Where things stand

Nobody can play the story together today. Two from-zero efforts exist: a presence prototype
where players see a marker for each other but the alien, doors and story are separate, and the
foundation described here (safe test copies, two instances on one PC, local saves). Nothing
shares the world yet.

## Existing work

- **Dropper7894**, Survivor Mode co-op, announced 2026-06 with a demo video
  ([Bloody Disgusting](https://bloody-disgusting.com/video-games/3957869/fan-made-co-op-mod-for-alien-isolation-announced/)).
  No public build or source.
- **Joshiloveyou presence prototype**, claim [#85](https://github.com/cranebeaker/coop-anything/issues/85).
  The UDP position avatar is drawn by an overlay window. Code is not published; notes are on their fork.
- **steamwrap foundation (this write-up)**: an independent rebuild of the loading layer, which the
  owner chose instead of waiting for #85's code.
- Echo Protocol (2025): a post title only, with no activity since.

## How the game works (for co-op)

Observed on the build above:
- `AI.exe` is a PE32 x86 image (machine 0x14C), image base 0x400000, with ASLR and NX. Its sections
  are .text/.rdata/.data/.rsrc/.reloc, with no packer names.
- It imports **14** functions from steam_api.dll (Init, RestartAppIfNecessary, Shutdown,
  Register/Unregister Callback and CallResult, RunCallbacks, and the SteamUtils/Friends/UserStats/
  RemoteStorage/User/Apps accessors). The game's steam_api.dll provides RemoteStorage012,
  UserStats011, Friends014, User017, Utils006, Apps006 and Networking005.
- String counts: "CATHODE" 1075, plus leftover multiplayer names `IsMultiplayerMode`,
  `NET_AI_UPDATE`, `NETZ_DISPATCH` and `character_register_accurate_pos`. No working netcode
  (per #85's analysis).
- Saves go through `ISteamRemoteStorage` (`PROGRESSION.ais` at the menu; `CHSV00.ais` and
  `MSSV01.ais` written on exit for a new profile).
- Single-instance lock: named semaphore `Alien: Isolation`. Worker threads also create named
  semaphores (`NewLoadScr:*`, `CAStreamer:*`, `SteamCloudThread:*`, …).
- It relaunches through Steam unless `RestartAppIfNecessary` returns false.
  `steam_appid.txt` makes `SteamAPI_Init` work without a Steam launch.

The memory layout (player transform, camera buffer) is in #85's notes. It was not re-verified here.

## What we changed

steamwrap is a replacement `steam_api.dll` for a copy of the game:
- **Forward-by-default.** Exports and unchanged interface slots are `jmp` thunks to the real DLL
  (`mov ecx,[ecx+4]; mov eax,[ecx]; jmp [eax+4*i]`). Pass-through needs no argument counts. A full
  mock gets them wrong and skews the stack.
- **RemoteStorage012** slots 0–20 and 27–32 use a local folder per profile, and IsCloudEnabled* returns
  false. The owner's Steam Cloud is never read or written.
- **UserStats011** stat and achievement writes (slots 3,4,5,7,8,10,13) are no-ops.
- With `AIC_PROFILE` set, `CreateSemaphoreA/W` and `CreateMutexA/W` in AI.exe's import table
  append `_<profile>` to object names. Two instances run with **no exe patch**.

## QA

- Offline: `build.ps1` builds a fake `steam_api_real.dll` and a harness that calls the wrapper
  with MSVC thiscall and measures the bytes each callee pops. **40 passed, 0 failed** (2026-11-01).
- Boot test (2026-11-01, real Steam client): p1 reached the main menu; p2 started alongside it and
  reached the first-run calibration screen. Both responded and closed cleanly. All save files were in
  `coop_saves\p1` and `coop_saves\p2`.
- No co-op gameplay test exists yet, because there is no co-op gameplay.

## How to play

You can't yet. Developer instructions are in the mod repo's README.

## Known issues

- Fullscreen copies minimize when they lose focus. Use windowed mode for two instances.
- A new profile starts at the brightness calibration screen.

## Next steps

1. Windowed mode in the test copy, plus a menu bot that gets into a level.
2. `coopnet.dll` (loaded by steamwrap): read the player transform from #85's pointer chain,
   then send it over UDP between two instances.
3. Coordinate with #85 so the two efforts merge rather than duplicate.
4. The unsolved part: alien and NPC ownership (playbook/actor-ownership.md), doors, and story via
   `EntityManager::call_method`.
