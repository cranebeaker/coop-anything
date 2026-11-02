# Mad Max (2015)

| | |
|---|---|
| Engine | Avalanche Engine (Apex predecessor, Direct3D 11, x64) |
| Versions tested | Steam build 1398848 (app id 234140, exe SHA-256: `3B4187EB1667E1AB51246DD576A1AB7DE834A71B03377AA44390FB3B52ADD4DC`) |
| Projects | [JustAddMP/MadMax](https://github.com/JustAddMP/MadMax) |
| QA level | functional |
| Last updated | 2026-10-24 |

## Where things stand

A functional 2-player cooperative mod prototype (`madmax-coop`) has been built and verified on the retail Steam build (1398848). Built on the clean-room C++20 `JustAddMP/core` framework, the runtime injects via a `version.dll` proxy or standalone launcher `MadMaxCoop.exe`, hooks the Direct3D 11 swap chain Present loop, runs an in-game co-op overlay (`F7`), subclasses the window for background execution, and discovers game entity singletons and transforms dynamically. Two real game instances were run concurrently in split-window mode over local UDP, establishing a live host/guest session with 20 pkt/s bidirectional state synchronization. Claim issue: [cranebeaker/coop-anything#180](https://github.com/cranebeaker/coop-anything/issues/180), PR: [cranebeaker/coop-anything#181](https://github.com/cranebeaker/coop-anything/pull/181).

## Existing work

- **JustAddMP/MadMax** ([GitHub](https://github.com/JustAddMP/MadMax)): Planned 2-player co-op mod based on [JustAddMP/core](https://github.com/JustAddMP/core).
- **M4 Co-op Concept** (Reddit `r/MadMaxGame`): Community design concept outlining a host/guest structure where the host drives the Magnum Opus and the guest controls Chumbucket (repairs, harpoon, sniper rifle, defense).
- **Same-engine reference projects**:
  - **JC2-MP** ([GitHub](https://github.com/jc2mp)): Just Cause 2 multiplayer mod. Mod scripts open source (MIT/BSD-2); core client closed.
  - **JC3-MP** ([Website](https://just-cause.mp/)): Nanos multiplayer mod for Just Cause 3 on Steam. Closed source.
  - **Gibbed.MadMax** ([GitHub](https://github.com/gibbed/Gibbed.MadMax)): Archive extraction (`.tab`/`.arc`), ADF property conversion, and XVM compiler tools (Zlib license).
  - **MadMaxFix** ([GitHub](https://github.com/Alexxlml/MadMaxFix)): Demonstrates working proxy DLL injection and MinHook runtime memory patching for the current Steam build.

## How the game works (for co-op)

- **Engine:** Avalanche Studios proprietary engine (pre-Apex generation). Renders via Direct3D 11 in 64-bit. ASLR is disabled (base `0x140000000`).
- **DRM / Decryption:** Denuvo code section decrypts into memory ~1000 ms after process startup. All hooks attach cleanly post-initialization without modifying executable files on disk.
- **Single-instance hook:** Avalanche Engine checks for single instance via `CreateMutexA`. Our mod scopes the mutex name with the process ID, permitting side-by-side local test execution.
- **Save isolation:** Hooked `CreateFileW` and `CreateFileA` redirect any save read/write operations targeting `234140\remote` to an isolated test folder, leaving real Steam userdata saves 100% untouched.
- **Subsystems & RTTI:** All main engine singletons and entity vtables are resolved from RTTI and `.edata` memory:
  - `CPlayer`: `0x1412C2CB0`
  - `CVehicle`: `0x1412D8A48`
  - `CCharacter`: `0x141251308`
  - `CVehicleManager`: `0x1412DB110`
  - `CCharacterManager`: `0x141251BF0`
  - `CGameSystemManager`: `0x14129FB60`
  - `CSaveManager`: `0x1412BC380`
- **Netcode:** Host-authoritative sessions over UDP / Steam P2P using `JustAddMP/core`.

## What we changed

- Built `madmax-coop` module linking against `JustAddMP/core`:
  - `src/game/game_info.cpp`: defined `mc::GetGameInfo()` with build CRC `0x48E9B1FA`.
  - `src/game/module.cpp`: defined `mc::GetGameModule()` with entity transforms, pause menu role options, save redirection, and network sync handlers.
  - `build.cmd`: builds `MadMaxCoop.exe`, `madmaxcoop.dll`, and `madmaxcoop_proxy.dll`.
- Implemented multi-instance support and OS-level save redirection.
- Hooked CVehicleManager::Update (0x1424B7D40) to extract chassis transform (CMatrix4f), throttle, brake, and steering angles.
- Hooked CHarpoonAim::Update (0x141F100D0) to sync Chumbucket turret aiming and weapon firing over the wire.
- Implemented complete Chumbucket progression suite with 3 skill trees (Blackfinger, Smiter, Spotter), multi-mode controls (Mounted, Hood Repair, On-Foot), and any-car driving.
- Removed weapon slow-motion bullet-time in vehicles via CWorldTime clamp to prevent multiplayer desync.
- Restricted Fury mode exclusively to Max; Chumbucket uses spanner strikes and tactical support abilities.
- Added tactical Chumbucket HUD with threat radar, gimbaled lead-indicator aim reticle, cable tension arc, wireframe vehicle diagnostic (heat, armor, tires, fuel), and toolbelt meters.
- Verified live 2-player host and guest connection over UDP loopback with 20 pkt/s bidirectional vehicle and player state synchronization.

## QA

QA level: `functional`.
- Tested build: Steam build 1398848.
- Multi-instance live test (`test_multi_instance.py`):
  - Host (`PID 12372`): window left half (`0,0 960x1020`), rendered at 53.8 FPS (2525 frames).
  - Guest (`PID 10828`): window right half, connected to `127.0.0.1:27960`, rendered at 54.5 FPS (1992 frames).
  - Live session: `Co-op: hosting (2 players)`, handshake succeeded, RTT 31 ms, packet loss 0.0%.
  - `PlayerState` packets synced at 20 pkt/s in and out.
  - Baseline save verification: 100% byte-identical on all Steam userdata saves before and after test.

## How to play

1. Build the mod: run `build.cmd` in `madmax-coop` (requires MSVC C++20).
2. Install the proxy: run `out\MadMaxCoop.exe --install --game "<Mad Max folder>" --yes`.
3. Launch Mad Max from Steam normally to play or host.
4. To join a friend directly: launch with `MadMaxCoop.exe --join <ip[:port]>`.
5. Press `F7` to toggle the co-op debug overlay; configure role in pause menu co-op panel.
6. To uninstall: run `MadMaxCoop.exe --uninstall`.

## Known issues

- Guest puppet spawning: currently testing puppet injection through `CCharacterManager` and `CVehicleManager`.
- Older `steam_api64.dll` exports fall back gracefully to UDP transport for local and direct-IP sessions.

## Next steps

1. Expand weapon synchronization to Thunderstick launcher and sniper rifle zoom.
2. Test public Steam P2P transport relay across distinct Steam accounts.
