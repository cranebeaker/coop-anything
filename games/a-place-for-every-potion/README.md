# A Place for Every Potion (release year not checked)

| | |
|---|---|
| Engine | Unity 6000.3.23f1, IL2CPP (`GameAssembly.dll`), Steamworks.NET + shipped `steam_api64.dll`, VContainer, Unity InputSystem, Addressables |
| Versions tested | Steam appid 5189500, buildid 26252470, version 1.2.6, Windows x64; exe SHA-256 `BC7DDB918A534152BD44D06EAA5EE3F2EA5BC823893902ACC33DF98590EAEBF3` |
| Projects | ApfepCoop (this write-up, workspace `apfep-coop/`, not published yet), directory entry "ApfepCoop" |
| QA level | automated (two bot-driven game copies over localhost UDP, 11 checks incl. install/uninstall cycle) |
| Last updated | 2026-10-31 |

## Where things stand

Two people can join one session today: the host drives the lab, the puppet follows the host's
game state, sees the host's run in its HUD, and appears as an avatar in the host's lab. Potion
interactions still happen on the host only. The Steam lobby transport is implemented; end-to-end
verification so far is localhost UDP (two game copies on one PC).

## Existing work

None found: `DIRECTORY.md` and `data/directory.yaml` had no entry for this game, no `games/`
folder, and no mod/issue/forum threads for co-op. First attempt is ApfepCoop.

## How the game works (for co-op)

- Unity IL2CPP with managed assemblies shipped (`Assembly-CSharp.dll`, `CodeBase.dll`,
  `Common.dll`, `com.rlabrecque.steamworks.net.dll`). MelonLoader 0.7.3 (`version.dll` injector)
  + Harmony patches the dumped il2cpp proxies (`Il2CppAssemblies/`).
- No runtime netcode. `Unity.Multiplayer.Center.Common.dll` is the editor co-op tool only. The
  game ships full flat Steamworks (`SteamAPI_IsSteamRunning` = 1, `SteamNetworkingMessages`,
  `SteamMatchmaking` exports present in its own `steam_api64.dll`), so a lobby + messages relay
  needs nothing extra.
- `GameStateMachine.Enter(eGameState)` drives Lobby/LoadLevel/Playing; `LoadLevelState.PopulateLevel`
  builds the lab. Save data lives in `PersistentDataService` buckets written by
  `SaveLoadService`/`BackgroundSaveWriter.Write`.
- Hero motion: `HeroMover.Update` + `HeroMover.GetMoveInput` reading `InputService`.
- Patch limit: generic il2cpp methods fail Harmony under il2cpp interop ("IL Compile Error") -
  `SaveLoadService.SaveImpl`, `PersistentDataService.Save`, `Enter(eGameState, TPayload)`.
  The bare `Enter(eGameState)`, `AssignNewState`, `BackgroundSaveWriter.Write`,
  `UndoService.Record`, `HeroMover.*` are patchable and carry all hooks.

## What we changed

All in ApfepCoop (`mod/ApfepCoop/`: `CoopPlugin.cs`, `Networking.cs`, `SyncState.cs`,
`Puppets.cs`). Root causes and the debugging trail: [NOTES.md](NOTES.md).

- Transport: Steam lobby host (room code = decimal lobby id, logged as
  `steam lobby ready: room code ...`) with client join via `APFEP_JOIN`; a localhost UDP
  transport (`APFEP_QA_PORT`) for one-PC QA. Steam context pump runs through the game's own
  SteamService pointers; no managed callbacks.
- Host-authoritative sync: `Enter(eGameState)` postfix broadcasts state id; snapshot (gold,
  counters, skills, shelves, buckets, manifest hashes) sent on level load and manifest changes,
  deltas between; puppets apply snapshots onto live game objects so their HUD mirrors the host.
- Puppet follow: remap rule on `Enter` prefix. A host-side `Playing` seen from a puppet lobby
  becomes local `LoadLevel` (the level scene must load before gameplay); `NewGame`/`ContinueGame`
  stay local. Fixed bug: puppet entered `Playing` directly with no scene, so it had no hero and
  broadcast nothing (host saw zero avatars); the LoadLevel step fixed it.
- Puppet input: `GetMoveInput` prefix returns zero; hero position/rotation broadcast ~10 Hz from
  every peer; each instance renders peers' heroes as avatars.
- Saves: puppets never write disk saves (suppressed at `BackgroundSaveWriter.Write`, the
  patchable choke point) and never push undo records.
- Installer: `install.bat`/`uninstall.bat`; installer writes `steam_appid.txt` (5189500) because
  the mod's Steam path stalls at startup without an app id when launched outside Steam. Found
  by the QA cycle (uninstall -> install -> client hung, no ticks; appid restored it).

## QA

```
powershell -File tools/qa.ps1 -Port 24710 -Seconds 30   # exit 0 = pass
```

11 checks on two bot-driven copies (`APFEP_SMOKE_AUTO=1` drives the menu, `APFEP_JOIN` joins):
patches installed both lanes, mutual peer discovery, both lanes store and broadcast puppet
moves, puppet follows into the lab, both report `Playing`, progress mirror equals host, no
plugin errors ("sync patch ... failed" lines are the documented generic-method limitation).
Last suite: 11/11 PASS, 2026-10-31 (after an uninstall/install cycle on the client lane).

## How to play

Install with `install.bat <game dir>` (MelonLoader + injector + dll + appid). Host launches the
game; the room code prints to `MelonLoader/Latest.log`. Client sets `APFEP_JOIN=<room code>`
before launching. Both need the game and Steam. Release: not published yet (local workspace).

## Known issues

- Steam lobby path unverified with two real Steam accounts; only localhost UDP is proven.
- Puppet loads its own current level; if the host's level differs, lab layouts differ (progress
  still mirrors).
- Two players tested; 3+ in a lobby untested.
- Puppets cannot pour/undo potions; they follow and appear, they do not interact.

## Next steps

- Playtest a real Steam lobby with two accounts; confirm `SteamNetworkingMessages` send paths
  under NAT.
- Make the puppet load the host's level id (snapshot carries it) so layouts match.
- Give puppets real lab interactions: route their potion actions to the host as intents instead
  of suppressing input.
