# A Place for Every Potion — co-op notes

Workspace: `L:/agent test space/apfep-coop` (this fork). Test install copied to
`apfep-coop/test-install` (owner's real install untouched).

## Identity (observed, recorded before following any write-up)
- Store: Steam. appid `5189500`, depot 5189502, buildid `26252470` (appmanifest_5189500.acf).
- Version `1.2.6`, development=false, target StandaloneWindows64
  (`A Place for Every Potion_Data/build_stamp.txt`).
- Engine: Unity **6000.3.23f1**, IL2CPP (GameAssembly.dll + UnityPlayer.dll, managed
  Assembly-CSharp.dll present in ScriptingAssemblies.json).
- Build commit `dd98831a55a480af49c1e498799eb9bd7c2bcb21`, branch master, recorded 2026-10-28.
- SHA-256:
  - `A Place for Every Potion.exe` BC7DDB918A534152BD44D06EAA5EE3F2EA5BC823893902ACC33DF98590EAEBF3
  - `GameAssembly.dll` 44189523D1CAB702670397EB1B93F68A38DBF859B2972F1BAF4794A8091DAF65
  - `UnityPlayer.dll` EEA205BECA44A252E1751E57EC5A1F866EEE2A9B4066263E5089BDE7D70C7350
- OS: Windows x64. Exe and steam_api64.dll machine 0x8664 (PE32+).
- Steam running at session start (`SteamAPI_IsSteamRunning()` = 1 on the game's own DLL).
- Prior co-op work: none. DIRECTORY.md / directory.yaml have no entry; `games/` has no folder.

## Engine facts (observed)
- Managed assemblies shipped: `Assembly-CSharp.dll`, `CodeBase.dll`, `Common.dll`,
  `com.rlabrecque.steamworks.net.dll` (Steamworks.NET), UniTask, R3, DOTween, VContainer,
  Newtonsoft.Json, Unity.InputSystem, Unity.Addressables, Cinemachine, NiceVibrations,
  `Unity.Multiplayer.Center.Common.dll` (only Unity's MP *editor* tool, no runtime netcode).
- Build defines include `STEAMWORKS_NET`.
- Native plugin shipped: `A Place for Every Potion_Data/Plugins/x86_64/steam_api64.dll`.
  Checked with ctypes: exports `SteamAPI_InitFlat`, `SteamAPI_IsSteamRunning`,
  `SteamAPI_GetHSteamPipe`, `SteamAPI_ManualDispatch_Init`,
  `SteamAPI_SteamNetworkingMessages_SteamAPI_v002`, `SteamAPI_SteamMatchmaking_v009`,
  `SteamAPI_SteamUser_v023`, `SteamAPI_SteamFriends_v018`,
  `SteamAPI_SteamNetworkingUtils_SteamAPI_v004`. => full flat API present; nothing to
  download or redistribute (playbook "check the game's own copy first" passes).

## Decisions
- Hook point: Unity mod loader + Harmony (playbook stage 1, order 3). Candidate:
  MelonLoader (proxy DLL `version.dll`) or BepInEx Unity-IL2CPP. Game is Unity 6 (6000.3)
  — verify loader support against the game in the test install before committing.
- Networking: Steam lobbies + ISteamNetworkingMessages relay through the game's own
  `steam_api64.dll`, real appid 5189500 (game ships Steamworks.NET and initializes Steam).
  UDP localhost fallback transport for two-client QA on one PC (playbook networking page).
- Model: host-authoritative state sync (retrofit onto closed game, playbook 1.6).
- QA: Unity Steamworks.NET mock via Harmony prefix patch plugin (playbook "Unity Mono
  games: mock Steamworks.NET"), test flag `-coopqa`, two instances on one PC.

## Log
- 2026-10-30: clone coop-anything; triage done (above); copied install to test-install
  (599 MB). No gcc on machine; dotnet SDK 9.0.318 present, Python 3.13.
- 2026-10-30: dump via Cpp2IL (tools/DecompTool) -> `decomp/`; system map in
  `apfep-coop/ARCHITECTURE.md` (state machine, buckets, hero, saves, Steam context).
- 2026-10-31: ApfepCoop built (MelonLoader 0.7.3 + Harmony, net6 plugin). Two test lanes
  `install/` (host) and `install-b/` (client), bots drive menus headlessly
  (`APFEP_SMOKE_AUTO=1`).
- Patch findings: generic il2cpp methods cannot be Harmony-patched under il2cpp interop
  ("IL Compile Error"): `SaveLoadService.SaveImpl`, `PersistentDataService.Save(bool)`,
  `PersistentDataService.Save(int,bool)`, `Enter(eGameState, TPayload)`. Working hooks:
  `Enter(eGameState)`, `AssignNewState` (postfix backstop), `BackgroundSaveWriter.Write`,
  `UndoService.Record`, `HeroMover.GetMoveInput/Update`, `LoadLevelState.PopulateLevel/
  LoadCurrentLevel`, `PotionsPersistence.ClaimDeal/AdoptLab/Pin/Flush`,
  `InputService.Update` frame pump.
- Desync trail: puppet showed host avatar but host saw none. Cause 1: puppet entered `Playing`
  without loading the level scene (no hero -> broadcast nothing); fixed by remap rule
  host `Playing` + puppet lobby -> local `LoadLevel`. Cause 2: puppet's own transitions were
  overwriting the mirrored authoritative state; split `localStateId` from `HostGameStateId`,
  snapshots set the authoritative value. `ReceivedMoves`/`SentMoves` counters in the tick log
  made both directions visible.
- Installer bug: after `uninstall.bat`, `install.bat` restored loader + dll but the client lane
  hung at startup with no ticks; Steam needed an app id outside Steam. Installer now writes
  `steam_appid.txt` (5189500); uninstall removes it. QA passes on the reinstalled lane.
- QA: `tools/qa.ps1` 11 checks; last run 2026-10-31: 11/11 PASS (host recv/sent ~220, client
  recv/sent ~230, both `Playing`, progress mirror equal).
- Outstanding: real two-account Steam lobby playtest; puppet level id from host snapshot;
  puppet-side potion actions.
