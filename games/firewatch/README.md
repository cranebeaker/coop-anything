# Firewatch (2016)

| | |
|---|---|
| Engine | Unity 2017.4.40f1, Mono, x64 (no IL2CPP) |
| Versions tested | Steam `v1.1.2`, buildid `7926499`, appid `383870` |
| Projects | [Shoshone Co-op](https://github.com/Zwtzyyy/Shoshone-Co-op-Firewatch), [Two-Forks-MP](https://github.com/Nanopaw-Studios/Two-Forks-MP) |
| QA level | automated (two real clients; suite in `harness/`), on a locally patched build |
| Last updated | 2026-11-14 |

## Where things stand

Two players can play Firewatch's story together over a LAN with the Shoshone 0.5.0 source
plus the fixes in `patch/`: the client receives the host's full save state, spawns beside
the host, sees their avatar, mirrors story facts and the current day, and world
interactions (doors, lamps, notes) take effect on both sides. **Upstream 0.5.0 as
published cannot sync world interactions at all** — see "What we changed".

The patched build passed a 9-check automated suite. It has not been through a full
playthrough, and it has no authentication: LAN only.

## Existing work

| Project | Last activity | State |
|---|---|---|
| [Zwtzyyy/Shoshone-Co-op-Firewatch](https://github.com/Zwtzyyy/Shoshone-Co-op-Firewatch) | 2026-10-25 (2 commits) | C# source + v0.3.0/v0.5.0 zips. No releases, no issues, no forks, **no license**. The build entry point does not build `StoryCoop_src/`. |
| [Nanopaw-Studios/Two-Forks-MP](https://github.com/Nanopaw-Studios/Two-Forks-MP) | 2025-01-02 | Earlier UNET `NetworkManager` prototype; Shoshone appears derived from it. |

Also relevant: [Raicuparta/two-forks-vr](https://github.com/Raicuparta/two-forks-vr) (VR
mod, unrelated, proves BepInEx works on this game).

## How the game works (for co-op)

- **Unity 2017.4.40f1, Mono.** `Firewatch_Data\Managed` holds a normal
  `Assembly-CSharp.dll`, so BepInEx 5 + HarmonyX patch it directly.
- **Game logic is C#** with PlayMaker FSMs for a lot of object behaviour. Useful entry
  points: `vgPlayerController`, `vgPlayerNavigationController`, `vgSaveManager`,
  `vgSaveable`, `vgEventManager` (blackboards/facts), `vgDialogTreeManager` /
  `vgDialogInstance`, `vgTargetInteract`, `vgEventTriggerVolume`, `vgPickup`, `vgPauseManager`.
- **Levels stream through SECTR.** `TeenLoop` is the main map; chunks are additively
  loaded scenes named `TeenLoop__SECTR <Chunk>` (and `TeenLoop_SECTR <Chunk>`), each with a
  single root GameObject whose name is an asset path such as
  `Assets/__Scenes/TeenLoop/Chunks/TeenLoop__SECTR General.unity`. **Any object-id scheme
  must escape slashes**: this is what broke Shoshone 0.5.0.
- **Saves** are `vgSaveData` (`nameSerializedDataMap`, `destroyedObjects`,
  `loadedChunkNames`, `loadedLevel`) written with `vgSaveManager.formatter`
  (`BinaryFormatter`) under `%USERPROFILE%\AppData\LocalLow\CampoSanto\Firewatch\saves`.
  `vgSaveManager.Load(vgSaveData)` reloads a level from that structure without touching
  disk — the basis of the story snapshot.
- **No netcode of its own.** The mod is a TCP client/server: 20 Hz player state, 5 Hz fact
  diff, snapshots on join/load/day change, plus an event channel for interactions.
- **Story day** is `vgEventManager.currentDayValue` (a field, not a blackboard fact); the
  snapshot does not carry it, so it has to be sent separately.

## What we changed

Patch: [`patch/shoshone-0.5.0-fixes.patch`](patch/shoshone-0.5.0-fixes.patch) (applies to
`Zwtzyyy/Shoshone-Co-op-Firewatch` `main`, 6 files).

1. **World sync was dead for every SECTR object.** `ObjectId.Of` joined transform names
   with `/` and `ObjectId.Find` split on `/`; SECTR roots contain `/`, so lookups resolved
   to nothing (`Interact in: object not loaded`). Saveable *state* only worked through its
   GUID fallback. Fix: percent-escape `%`, `/`, `:`, `#` per component before joining and
   unescape on lookup. Test: host uses a lamp, client's lamp FSM goes `LightOff -> LightOn`.
2. **Both players spawned on the same point.** Snapshot restores the host's position. Fix:
   on the first partner state after a snapshot load, step the client 1.6 m back and 0.6 m
   aside if it is within 0.75 m of the host. Test: distance 1.71 m.
3. **Facts ping-ponged at 5 Hz.** Both peers diffed whole blackboards and applied `Set`;
   `TimeSinceLastSpeech` (and anything else that changes every frame) fought forever. Fix:
   host-authoritative facts (`ClientSendsFacts=false` by default), `TimeSinceLastSpeech`
   excluded. Test: client emits zero fact packets; host-set fact still arrives.
4. **Client stayed on day -1.** Fix: new `Msg.Day` sent with each snapshot; client sets
   `currentDayValue` and the day fact after load. Test: both report day 0.
5. **Avatar looked broken.** The clone had no runtime animator controller (bind pose), the
   name tag was screen-filling and the fallback head sphere rendered black. Fix: copy the
   controller, scale the label, build the sphere from an in-build shader with a skin tone.
6. **Dev overlay on by default.** ESP markers/skeleton and the dev HUD are now off.

## QA

- `harness/run_smoke.ps1` starts a host and a client from two copies of the same install,
  drives them through an in-game plugin (`harness/CoopTestDriver.cs`) and writes
  `RESULT: PASS|FAIL`. It asserts: client synced, spawn distance > 1 m, equal day,
  interaction delivered with no unresolved ids, client-side FSM state changed, fact
  mirrored, and zero client fact broadcasts.
- Last suite: **9/9 PASS**, 2026-11-14, two real clients on one PC (Windows 11, Steam
  `v1.1.2`).
- Pinned build: `Firewatch.exe` SHA-256 `170AA585…92D7C0`; `Assembly-CSharp.dll` SHA-256
  `145B56AE…49D0522A`. The harness refuses other builds by default.

## How to play

1. Build `StoryCoop.dll` from the upstream source with the patch applied (Unity 2017 Mono
   references from `<game dir>\Firewatch_Data\Managed`, BepInEx 5.4.23.6).
2. Install BepInEx 5.4.23.6 (x64) and drop the DLL in `BepInEx\plugins`.
3. Host: game → `SHOSHONE CO-OP` (F8) → Host. Client: set `Ip` in
   `BepInEx\config\zwtzyy.firewatch.storycoop.cfg`, then Join.
4. Host starts a new game or loads a save; the client follows.

`install/install.ps1` does 2–4 for both players (finds the Steam install, verifies the
game hash, downloads BepInEx from the official release with a pinned SHA-256).

## Known issues

- **No authentication and `BinaryFormatter` snapshots.** Anything that can reach TCP 7778
  can join and send events. Trusted LAN only; do not port-forward.
- One partner only; the mod is built around a single peer.
- Dialog choices are mirrored but both players can answer the same prompt; untested with a
  live dialog.
- Scripted trigger volumes use the same fixed id scheme but were not exercised end to end.
- Cutscenes/camera choreography that assumes one player will fight each other.
- The client writes into a `StoryCoop_Client` save slot in the shared save folder.
- Only tested on one PC with two instances; a real two-machine LAN run is still needed.

## Next steps

1. Two-machine LAN run; check firewall/NAT behaviour and join while the host is mid-level.
2. A scripted dialog and a scripted trigger-volume test in the harness.
3. Day transition and save/load handoff in the suite.
4. Replace `BinaryFormatter` snapshots (custom serializer or a `SerializationBinder` +
   shared secret) before anyone considers internet play.
5. Ask the author about a license before publishing a redistribution; until then ship the
   patch, not a rebuilt DLL.
