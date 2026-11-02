# Alien: Isolation (2014)

| | |
|---|---|
| Engine | Cathode (Creative Assembly proprietary), 32-bit x86 |
| Versions tested | Steam build 4940658, AI.exe SHA-256 `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90` |
| Projects | Dropper7894 Survivor Mode co-op (unreleased); Joshiloveyou presence prototype ([#85](https://github.com/cranebeaker/coop-anything/issues/85), PRs #156/#164); [alien-isolation-coop](https://github.com/phoenixd123/alien-isolation-coop) presence prototype (this write-up) |
| QA level | automated, for presence only (bot drives two real clients and checks marker placement) |
| Last updated | 2026-11-01 |

## Where things stand

Nobody can play the story together today. Two from-zero efforts reach **presence**: each player
runs their own campaign and sees the partner as an on-screen marker. This one (alien-isolation-coop)
adds host/join over UDP and an installer. Nothing shares the world yet: no 3D partner
character, and the alien, doors, items and story are separate.

## Existing work

- **Dropper7894**, Survivor Mode co-op, announced 2026-06 with a demo video
  ([Bloody Disgusting](https://bloody-disgusting.com/video-games/3957869/fan-made-co-op-mod-for-alien-isolation-announced/)).
  No public build or source.
- **Joshiloveyou presence prototype**, claim [#85](https://github.com/cranebeaker/coop-anything/issues/85).
  The UDP position avatar is drawn by an overlay window. Code is not published; notes are on their fork.
- **alien-isolation-coop (this write-up)**: an independent rebuild guided by #85's notes, which the
  owner chose instead of waiting for #85's code. steamwrap loading layer plus the coopnet presence layer.
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

Camera record (pointer chain from #85, re-verified here): `mgr=[exe+0x12F0C88]`, `blk=[mgr+0x58]`,
record at `blk+0x78C`, signature `rec[7..9] = 47 / 0.05 / 1000`. Measured in a level:
- `rec[0..3]` is the view quaternion (x,y,z,w). Identity looks along world **+Z**, **+Y** is up and
  **+X** is right. A yaw of -72 deg (from the quaternion) matched a walk direction of -72.2 deg.
  Looking up adds x/z components. #85's "facing is about 90 degrees off" came from reading one component.
- `rec[4..6]` is the camera position. `rec[10..15]` repeats `rec[0..5]`; why is unknown.

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

- In game (2026-11-01, two copies on one PC, owner away). A bot (focus, scancode keys, mouse) drove both
  copies from the title to Continue Game, into the level. One player walked 5.7 m down a corridor. The
  other's overlay placed the marker at camera (-1.26, -0.01, 5.60), matching the geometry. The reverse
  direction gave an edge arrow when behind and a marker after turning around. Host/join: the host
  learned the joiner's address from the first packet; tx 1874 / rx 1872.
  The overlay was checked by an offscreen render of it composited on a PrintWindow capture. **Never
  screen-copy for QA**: an earlier screen copy caught the owner's other windows.
- Offline: protocol over real UDP 12/12, projection 10/10 (cases from in-game measurements), installer
  round trip on a fake folder 6/6.
- Offline: `build.ps1` builds a fake `steam_api_real.dll` and a harness that calls the wrapper
  with MSVC thiscall and measures the bytes each callee pops. **40 passed, 0 failed** (2026-11-01).
- Boot test (2026-11-01, real Steam client): p1 reached the main menu; p2 started alongside it and
  reached the first-run calibration screen. Both responded and closed cleanly. All save files were in
  `coop_saves\p1` and `coop_saves\p2`.
- Untested: two PCs over the internet.

## How to play

Only presence works: you see your partner's marker, and each of you plays your own campaign.
Install and host/join steps are in the mod repo's README (`install.ps1`, `play-coop.bat`).

## Known issues

- The marker is an overlay, so it shows through walls.
- Fullscreen copies minimize when they lose focus. Use windowed mode for two instances.
- A new profile starts at the brightness calibration screen.

## Next steps

1. Test across two PCs. Then Steam networking (ISteamNetworking005), so no port forwarding is needed.
2. A 3D partner character: find how Cathode spawns or moves a human NPC, and drive one from the network.
3. Coordinate with #85 so the two efforts merge rather than duplicate.
4. The unsolved part: alien and NPC ownership (playbook/actor-ownership.md), doors, and story via
   `EntityManager::call_method`.
