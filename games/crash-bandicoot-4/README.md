# Crash Bandicoot 4: It's About Time (2020)

| | |
|---|---|
| Engine | Unreal Engine 4.24.2 (UE project name "Lava"), Toys for Bob "Tfb" framework |
| Versions tested | Steam (app 1378990), exe `CrashBandicoot4.exe` SHA-256 `f78bebcc3cf183aac47cc7e60faaed0bd9f9db4bef41e407b2c16a75a696ddec`, 73,440,520 bytes, PE timestamp 2022-09-16, ProductVersion 1.0.0, Windows 10 x64 |
| Projects | none yet (directory entry: gap) |
| QA level | none |
| Last updated | 2026-10-30 |

## Where things stand

Nobody can play Crash 4 together yet. This folder is groundwork: UE4SS now runs on the Steam build,
the game's classes are mapped, and the game turns out to ship unused multiplayer classes and a
replicated combat framework. A listen server starts; the open blocker is that a second copy of the
game on the same PC crashes after about 56 seconds, so a host/join test has not happened yet.

## Existing work

- Shipped modes are local only, turn-based: Pass N. Play (up to 4 players take turns, hand-off on death or
  checkpoint) and Bandicoot Battle (Checkpoint Race, Crate Combo). No simultaneous co-op, no online
  ([TechRadar](https://www.techradar.com/how-to/crash-bandicoot-4-multiplayer-is-it-online-and-co-op)).
- No online or simultaneous co-op mod found (searches listed in the directory entry). The Nexus/GameBanana scene
  is cosmetic and level-data mods (Mario skin, N.Hanced).
- UE4SS issue [#720](https://github.com/UE4SS-RE/RE-UE4SS/issues/720): someone else ran UE4SS v3.0.1 on Crash 4
  and crashed at the main menu; a user there suggested `HookInitGameState = 0` and `HookLoadMap = 0`. Confirmed below.
- No Nucleus Co-op handler found.

## How the game works (for co-op)

- **Install layout:** standard UE4 packaged game: `Lava/Binaries/Win64/CrashBandicoot4.exe`, 646
  `Lava/Content/Paks/pakchunk*-WindowsNoEditor.pak` (~22 GB, no IoStore). No anti-cheat, no Denuvo string.
- **DRM:** SteamStub. Entry point in a `.bind` section (206,616 bytes); `.text` (47.7 MB) has entropy 8.00 on disk.
  It is decrypted at startup, so anything that scans code before the entry point runs sees garbage.
- **User folder:** `%LOCALAPPDATA%\CrashBandicoot4\Saved\` (Config, Crashes, Logs, SaveGames), shared between the
  Steam install and any copy. `-userdir=<path>` moves it per instance (verified). The shipping build writes no log
  with `-log`.
- **Netcode:** the exe contains `OnlineSubsystemSteam`, `SteamNetDriver`, `IpNetDriver`, `OnlineSubsystemNull`.
  The game's own modules are written for client/server replication (counts from the UE4SS header dump):
  Lava 9 OnRep / 18 Server / 12 Client / 2 Multicast; TfbCore 7/3/1/3; TfbCombat 3/6/4/3; Phasmid 21/1/4/2. Examples:
  `ServerSetReplicatedInputEvent`, `ServerTryActivateAbilityWithPayload`, `Server_InstigatorDamageClaim`,
  `Multicast_DamageResult`, `OnRep_Health`, `Server_ChangeControlledCharacter`, `Server_ReliablePlayerConnected`.
- **Unused multiplayer classes:** `ALavaMPGameMode` (PlayerCount, AreAllPlayersConnected, ...), `ALavaMPGameModeLobby`,
  `ALavaMPGameModeMatch`, `ALavaMPGameState` (OnRep_MatchState), `ALavaMPPlayerState`, `AMPLavaPlayerController`,
  `ABuddyPlayerStart`, `AHandoffPlayerStart`. Where they came from is unknown (possibly shared with a later
  Toys for Bob online game; unverified).
- **Player:** `ALavaCombatCharacter : ATfbCombatCharacter : ACharacter`, movement `UTfbCombatCharacterMovementComponent :
  UCharacterMovementComponent`, GAS `UTfbCombatAbilitySystemComponent`. In level L113 (`/L113/Maps/L113_MASTER`) the
  single `LavaPlayerController` possesses `BP_CP3701_C`; game mode `BP_GameMode_Crash_SinglePlayer_C`, game state
  `BP_GameState_Crash_C`. Enemies are `TfbCombatCharacter`s driven by `BP_Base_Enemy_AIController_C`.
- **UE4SS helper pitfall:** `UEHelpers.GetPlayerController()` errors with "No PlayerController found" in levels
  (its `Pawn:IsPlayerControlled()` filter); get the world from `FindFirstOf("GameStateBase"):GetWorld()` instead.

## What we changed

All in a copy of the install, never the Steam folder. Files are in [tools/](tools/).

1. **UE4SS finds nothing when loaded through `dwmapi.dll`.** Cause: the proxy loads before SteamStub decrypts `.text`;
   all 7 pattern scans fail ("Failed to find EngineVersion ... GUObjectArray ..."). Fix: no proxy; inject `UE4SS.dll`
   once the game shows its title screen ([tools/inject-ue4ss.ps1](tools/inject-ue4ss.ps1), plain LoadLibraryW, refuses
   to touch an exe outside the test folder, skips processes that already have UE4SS). Proof: next log finds
   EngineVersion 4.24 and all signatures on attempt 1. The DRM is not touched.
2. **Fatal error about 2 minutes in (every level load).** Minidump: access violation in VCRUNTIME140 memcpy, called from
   UE4SS.dll+0x37cab3 / +0x37c982 / +0x554aaa. Symbolized with the official `zDEV-UE4SS_v3.0.1.zip` PDB (its UE4SS.dll is
   byte-identical to the release, SHA-256 `8ac18fbf...2b26`): `FString::FString` <- `FURL::FURL` <- `RC::Unreal::HookedLoadMap`.
   Crash 4's `FURL` layout differs from stock 4.24, so the hook copies text as a pointer. Fix: `HookLoadMap = 0` under
   `[Hooks]` in `UE4SS-settings.ini` (supported by v3.0.1, `SettingsManager.cpp`, but missing from the shipped ini).
   Proof: no crash, header dump completed, level loads fine. Disabling InitGameState / CallFunctionByNameWithArguments /
   BeginPlay / LocalPlayerExec alone did not help.
3. **`mods.txt` first entry ignored.** The shipped file starts with a UTF-8 BOM and UE4SS v3.0.1 then skips the first line
   (stock `CheatManagerEnablerMod` never starts). Fix: save without BOM.
4. **Class map.** [tools/Mods/CoopAutoDump](tools/Mods/CoopAutoDump/Scripts/main.lua) runs `DumpAllObjects()` and
   `GenerateSDK()` 10 s after start: 57.8 MB object dump, 932 header files (taken at the menu).
5. **Live state report.** [tools/Mods/CoopTools](tools/Mods/CoopTools/Scripts/main.lua) prints world, game mode, game state,
   controllers, pawns on level change and on F7/F8.

Settings used: [tools/UE4SS-settings-changes.md](tools/UE4SS-settings-changes.md).

## QA

None yet. Manual runs only (tester launching, this session reading logs and minidumps):

- Listen server: `open /L113/Maps/L113_MASTER?listen` in copy A reloads the level as host (new GameMode instance), no crash.
- Second copy on the same PC (plain launch, and again with `-windowed -nosteam -userdir=<own folder>`): fatal error
  before injection, access violation reading 0x0 at `CrashBandicoot4.exe+0xc31cf1`, identical stack both times, about
  56 s after process start. Separate user folder and `-nosteam` do not change it.

## How to play

Not playable yet.

## Known issues

- A second copy of the game on one PC crashes after about 56 seconds (cause unknown).
- Run UE4SS with `HookLoadMap = 0` or the game crashes on the first level load.

## Next steps

1. Find what the second instance waits for at `CrashBandicoot4.exe+0xc31cf1` (debugger attached to the running copy;
   the on-disk code is encrypted). Suspects: per-user Steam state or the game's online login (both instances share
   one account). playbook/engine-families.md notes that on God of War (also SteamStub v3) a second instance needed the
   wrapper bypassed; check whether this is the same problem before assuming it. Per-instance Steam identity
   (playbook/split-screen.md) is the other route.
2. Or test with two PCs: host `open /L113/Maps/L113_MASTER?listen`, join `open <host LAN IP>` (with `-nosteam`, possibly
   `GameEngine.NetDriverDefinitions` pointing at `IpNetDriver` as on Genesis Alpha One).
3. Then see what replicates with `BP_GameMode_Crash_SinglePlayer_C` vs the shipped `LavaMPGameMode*` classes, and whether
   a joining player gets a `BP_CP3701_C` pawn.
