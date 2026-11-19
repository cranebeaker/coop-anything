# The Outer Worlds 2 (2025): co-op notes

Triage (phase 0) and a first research pass (phase 1) were done from public sources on 2026-11-15 without
the game installed. The same day, triage step 1 (game identity) and an install inspection were done on
the game PC, read-only: nothing in the install or the save folder was changed and the game has not been
started yet.

## Game identity (triage step 1, recorded on the game PC, 2026-11-15)

All read from the install with the hub MCP (`game_identity`, `detect_engine`) and small read-only scripts.

| Field | Value |
|---|---|
| Store | Steam, app 1449110, build id 24225441 (`appmanifest_1449110.acf`, state 4, fully downloaded) |
| Depots | 1449111 (main, 82.5 GB), 3729850 (DLC depot, 7.9 GB, name not found: the store API returned nothing for that id) |
| Game version | 1.2.0.1, changelist 2994920 (bootstrap exe `ProductVersion` `++Arkansas+release-1.2.0.1-CL-2994920`; `MicrosoftGame.config` `Version="1.2.0.1"`) |
| Main exe | `<game dir>\Arkansas\Binaries\Win64\TheOuterWorlds2-Win64-Shipping.exe`, 181,109,400 bytes, SHA-256 `55ac756281828d00d93194ac80f99d13233415aff69360554d982a899f17c444` |
| Bootstrap exe | `<game dir>\TheOuterWorlds2.exe` (Epic `BootstrapPackagedGame`), 364,112 bytes, SHA-256 `e92d39a01ee6ae98b80acf9acf3da524b23ef9604243065032cee29b785a23c5` |
| Engine | Unreal Engine 5.4.4 (shipping exe `FileVersion` 5.4.4.0), project `Arkansas`, Microsoft GDK build (`MicrosoftGame.config`, `.xbld` section) |
| OS | Windows only; inspected on Windows 11 Pro build 26300 |
| Protection | SteamStub: entry point in the `.bind` section (entropy 7.96). No anti-cheat files found (`detect_engine`, file list) |

## Observations from the install (sourced)

- **The code is readable on disk despite SteamStub.** `.text` is not encrypted: 487,327 `.pdata` entries all
  point inside `.text`, sorted; 186,371 runs of `cc cc cc cc` padding and 241,058 `mov [rsp+x],rbx`
  prologues (random data would give about zero); entropy of the first MB 6.76. Only `.bind` (the stub) is
  opaque. Same situation as God of War 2018 in [engine-families.md](../../playbook/engine-families.md), and
  unlike Crash Bandicoot 4, where UE4SS found nothing in an encrypted `.text`. Static disassembly of the file
  needs no unpacking.
- **Import table intact**, 43 DLLs, including `dwmapi.dll` (UE4SS's default proxy name), `DSOUND.dll`,
  `WINMM.dll`; `SHELL32!SHGetKnownFolderPath` and `SHGetFolderPathW`; `KERNEL32!CreateMutexA/W`; `WS2_32` (47
  functions). `steam_api64.dll` is not imported; the engine loads it at run time.
- **Paks are not encrypted.** `Arkansas-Windows.pak`: pak version 11, index not encrypted, key GUID zero,
  compression Oodle, 4,614 files (1,615 Oodle-compressed). `Arkansas-Windows.utoc` (IoStore TOC version 6,
  469,907 chunks, 77.9 GB `.ucas`): flags Compressed and Indexed, not Encrypted, key GUID zero. No `.sig`
  files. Reading compressed entries needs the Oodle runtime, which the game does not ship as a DLL.
- **Config in the pak** includes `Arkansas/Config/Windows/Custom/Steam/WindowsEngine.ini` (a Steam-only custom
  config), `WinGDK/WinGDKEngine.ini`, `DefaultEngine.ini`, `DefaultGame.ini`.
- **Game plugins:** `ArkansasDualSenseFeatures`, `D11AdaptiveTriggers`, `D11DualSenseLightBar`,
  `OEIDeviceProfileSelector`, `OEIDynamicStructs`, `OEIGamingDevice`, `OEIPlatformService`, `UDSTools`. No
  AngelScript: gameplay is C++ (module `Arkansas`) and Blueprints, unlike Gothic 1 Remake.
- **Engine netcode is compiled in.** Strings in `.rdata`: `UIpNetDriver`, `USteamNetDriver`, `GameNetDriver`,
  `NetDriverDefinitions`, `?listen`, `DemoNetDriver`, `BeaconNetDriver`, `IrisNetDriverConfig`; also the
  GDK and PlayFab drivers (`UGDKNetDriver`, `UPlayFabPartyNetDriver`). Engine plugins in the pak:
  `OnlineSubsystemSteam`, `SteamShared`, `OnlineSubsystemUtils`, `OnlineSubsystemNull`, `OnlineSubsystemGDK`,
  `GDKNetDriver`, `PlayFabParty`, `OodleNetwork`. Steamworks SDK v157 in
  `Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64\steam_api64.dll`.
- **Microsoft GDK libraries ship next to the exe:** `Party.dll`, `PartyXboxLive.dll`, `GameChat2.dll`,
  `XCurl.dll`, `libHttpClient.GDK.dll`. `MicrosoftGame.config`: identity `Microsoft.OE-Arkansas`,
  `RequiresXboxLive=false`. Steam runs `GamingRepair.exe scenarioExternal` (GDK runtime repair) as the
  install script on the first Steam launch (`gamingrepair.vdf`). Gaming Services 39.118, `xgameruntime.dll`
  and GameInput were already present on the test PC.
- **Saves.** Obsidian's own `USaveGameManager` (`Arkansas\Source\Arkansas\Persistence\SaveGameManager.cpp`
  in the exe's strings) writes one folder per save with `Metadata.dat`, `SaveGame.dat` and
  `SaveGameScreenshot.png` under `%USERPROFILE%\Saved Games\TheOuterWorlds2\` (folder names are 32 hex
  digits, plus `Autosave00` to `Autosave02`), with `steam_autocloud.vdf` (Steam Auto-Cloud). The exe holds
  the `FOLDERID_SavedGames` GUID next to `FOLDERID_Documents`, `FOLDERID_LocalAppData` and
  `FOLDERID_LocalAppDataLow`. Other names in the same string block: `Travel.dat`, `Factions.dat`,
  `CompanionManager.dat`, `LevelRefs.dat`, `USaveGameManager::TransferSaveGames`, and a cvar
  `SaveGameManager.DebugFrequentAutosaves`.
- **Steam start-up check.** The platform layer (`OEIPlatformService`) prints "Game not started from Steam
  and failed to relaunch in Steam, please delete the steam_appid.txt file if it exists in game's binary
  directory" and knows the services `Steam`, `XSAPI`, `BattleNet`, `Development`.
- **UE command-line switches present as strings:** `UserDir=`, `SaveToUserDir`, `-saveddirsuffix=`.
- **Reflection property names** `bSingleInstance`, `bOnlyAllowSingleInstance`, `bAllowMultipleInstances`
  exist; which class owns them is not known yet.
- **Content map** from the IoStore directory index (455,685 paths, read without Oodle by a 70-line script):
  gameplay Blueprints under `/Game/Blueprints` (15,214 entries: `World`, `Item`, `WEAP`, `NPC`, `AI`, `Spells`,
  `CHAR`, `Logic`, `Conversations`, `Player`, `Online`). The framework reuses the first game's codename:
  `Game/IndianaGameInstance_BP`, `Game/OwGameModeStandard`, `Game/OwGameModeFrontEnd`, `Game/OwGameState_BP`,
  `Player/IndianaPlayerCharacter_BP`, `Player/IndianaPlayerController_BP`, `Player/OwPlayerState_BP`,
  `Player/OwMainMenuPawn_BP`. A third-person animation set exists (`Player/TPV/.../ARK_P3P_PlayerCharacter_*`,
  `CHAR/ARK_PlayerCharacterTPV_AnimBP`), which a puppet player can use. `Blueprints/Online` holds only
  achievements (49 assets), hero stats and rich presence. Map groups: `01_JP`, `02_ParadiseIsland`,
  `03_GoldenRidge`, `05_Space`, `06_Cloister`, `08_Praetor`, `09_HP`, `CharacterCreation_Lite`,
  `ConversationalMainMenu`.

## Existing co-op work

None found for this game, 2026-11-15, by: this repo's DIRECTORY.md, `games/`, issues and PRs ("outer
worlds"); web search for "The Outer Worlds 2" co-op or multiplayer mod (again on 2026-11-15, nothing
new); Nexus via web search; GitHub repo search ("outer worlds coop", "outer worlds multiplayer",
"outer worlds 2 mod", "outerworlds2", "outer worlds ue4ss"). Not checked: Reddit, Discord servers, Nexus
browsed directly (Nexus pages answer automated fetches with HTTP 403), GitHub code search, forks and
branches.

Closest same-engine projects in DIRECTORY.md:

- **Gothic 1 Remake Coop** (UE 5.4). The directory URL `github.com/CreativeNa/gothic1-remake-coop`
  answers 404 since at least 2026-11-15. The project now lives at
  [CreativeNa/G1R-coop](https://github.com/CreativeNa/G1R-coop) ("release repository", MIT, created
  2026-11-09, release 0.5.4.5 on 2026-11-15, README status "Beta. Working: connection, movement, items,
  chests, death, F9, time of day, quests, trading, sleeping"). That repo has never contained the mod's C
  source (no `.c`/`.cpp`/`.h`/`.lua` file in its history; commit `b8ca6df` "Keep only user-facing docs and
  the engine research in the public repository"). It keeps the engine research (`docs/RE/`), the
  PowerShell and Python test harness (`tools/`), and the installer. The directory entry needs a new URL and
  status (separate PR, one game per PR).
- [NOLA-Stalker2-Coop](https://github.com/Joaovitor404silva/NOLA-Stalker2-Coop): UE5, claims story, enemy
  and world sync, partial.

What Gothic 1 Remake Coop's docs say that applies here (read at commit `5f630c7`):

- Architecture: an injected native C DLL ("coop core") walks UE reflection itself (objects from
  `GUObjectArray`, properties through `FField::Next`), calls game functions through `ProcessEvent`, and
  hooks `ProcessEvent` (a vtable slot) for a game-thread tick. Each player runs their own world; a custom
  UDP protocol syncs player puppets (full skeleton pose, 422 bones), story words and quest states,
  containers, and NPC health through an ownership claim. No UE4SS, no UE replication.
- Pipeline: a reflection dump from the live game gives the SDK, a `.usmap` and per-function parameter
  layouts (`docs/RE/02_PIPELINE.md`); `retoc` plus the Oodle runtime (`oo2core_9_win64.dll` from
  WorkingRobot/OodleUE) unpack IoStore content; UAssetAPI turns assets into JSON.
- Rules (`docs/RE/05_COOP_BRIDGE.md`): call the function the game calls instead of writing state;
  `FGameplayEffectSpec` cannot cross processes, so send "who hit whom" plus the resulting state; RVAs change
  every patch, class layouts mostly do not.

## Derived (from the observations above)

- Path 2, build from zero: there is no co-op mod to revive.
- A proxy DLL in the test copy loads before the game's code (`dwmapi.dll`, `DSOUND.dll` or `WINMM.dll`;
  none is a KnownDLL). Because `.text` is plain, UE4SS loaded early should find its signatures, unlike
  Crash Bandicoot 4.
- A copy at another path still writes saves to `%USERPROFILE%\Saved Games\TheOuterWorlds2\` (known folder,
  not the install folder). Test copies need an in-process redirect of `SHGetKnownFolderPath`
  (`FOLDERID_SavedGames`) through the exe's import table (The Witness method in methods.md), so native
  code, so a compiler, before the first test launch. Config and logs can probably move with `-UserDir=`.
- A `steam_appid.txt` in the copy's binary folder plus `SteamAppId`/`SteamGameId` in the environment should
  keep `SteamAPI_RestartAppIfNecessary` from relaunching the store install. Steam must be running.
- Two networking routes exist: the engine's own listen server (Genesis Alpha One, Pacific Drive) or two
  worlds with an own sync layer (Gothic 1 Remake, Gothic 3, The Witness). For an RPG whose quests, dialogue
  and inventory assume one local player, the second fits better; the first is cheap to probe once code runs.

## Guesses

- The DLC depot 3729850 is the premium-edition content or the art book (the `Artbook` folder holds a Unity
  app, `TOW2Artbook.exe`).
- Two copies at once for bot QA will be tight on this PC: 32 GB RAM, RTX 4060 Laptop with 8 GB VRAM, game
  minimum 16 GB RAM.
- Test runs launched with the real app id show as playtime and could unlock achievements on the owner's
  Steam account unless the mod blocks stat writes in test copies.

## Unknowns

- Why UE4SS start-up stalls with the generated `VTableLayout.ini` (run 3), and whether UFunction calls work
  once it gets past that point. Other engine classes may also have extra virtuals.
- Whether the Saved Games redirect holds when the game lists, writes and loads saves (not reached yet).
- How to pass the gamma screen without a real mouse click (its accept handler is `OnGammaAccepted`).
- Whether two copies run at once (GDK single-instance flags, a named mutex).
- Whether the configured `GameNetDriver` is Steam or IP (`Custom/Steam/WindowsEngine.ini` is Oodle-compressed).
- Whether the game writes anywhere else at start-up (settings, telemetry, `TransferSaveGames`).
- Which class owns `bOnlyAllowSingleInstance`.

## Workspace (phase 2), 2026-11-15

Owner approvals: llvm-mingw 20261020 (portable, in the git-ignored `local/tools`), UE4SS v4.0.0 in test copies
only, starting the Steam client for test runs. Both downloads were checked against the release SHA-256
(`fd5462b7…74cd`, `251ad0d1…0c81e45`). The real saves were backed up and hashed before anything ran.

**Test copies.** `tools/gamepool` with a hard-link-only config: 714 hard links, the 78 GB `.ucas` shared, no
extra disk. Private per copy: `steam_appid.txt` (1449110), the proxy DLL, `ue4ss/`, and a `sandbox/` folder.

**Save and settings isolation** (needed because the game resolves its folders through Windows, not its install
path). A `dsound.dll` proxy (the exe imports `DSOUND.dll` by ordinals 1, 3, 6, 8, 11, 12; not a KnownDLL) with
the System32 export table (12 ordinals, checked against `llvm-readobj` output at build time). With
`TOW2_SANDBOX_DIR` set it patches the exe's own import slots for `SHELL32!SHGetKnownFolderPath` and
`SHGetFolderPathW` in `DllMain` and answers Saved Games, LocalAppData, RoamingAppData, Documents and
LocalAppDataLow with folders under the sandbox; every other folder passes through. It terminates the process
(exit `0x5AFE000x`) if a slot is missing or the path is bad, rather than letting the game use the real folders.
Tests (all PASS, 2026-11-15): pass-through without the variable, redirect with it, DirectSound still forwarded
(2 devices), ordinals reach the named functions, a twin build without the patch is caught (4 failures), a
relative path stops the process with `0x5AFE0003`.

**First launch of a test copy** (2026-11-15, Steam running, no UE4SS yet):

- `TheOuterWorlds2-Win64-Shipping.exe` started from the copy with `SteamAppId`/`SteamGameId` set and
  `steam_appid.txt` present. No relaunch through Steam, no process from the store install, no `GamingRepair`.
- SteamStub did not object to the import patch: the patched slots are at RVA `0x7d00170` and `0x7d00190`.
- Folder requests in the first second: `FOLDERID_ProgramData` (passed through, nothing written there),
  `FOLDERID_LocalAppData` and `FOLDERID_Documents` (both redirected). UE then wrote
  `Arkansas\Saved\Config\Windows\GameUserSettings.ini` and `UnrealEngine\5.4\Saved\Config` into the sandbox.
  `Arkansas\Saved\Logs` stayed empty.
- Window title `The Outer Worlds 2  ` after 46 s. The first launch shows "The game is optimizing shaders for
  your system…", a precompile screen. Minimized, the process used about 4 cores; restored behind every other
  window (`SW_SHOWNOACTIVATE`, then `SetWindowPos(HWND_BOTTOM, SWP_NOACTIVATE)`) it used about 10 cores, so a
  minimized copy compiles shaders much more slowly. The NVIDIA driver cache (`%LOCALAPPDATA%\NVIDIA\DXCache`,
  shared by all games, outside the sandbox) grew 768 MB in the first 10 minutes.
- `PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT)` returns the real D3D12 frame of the game window, also
  when other windows cover it; the capturing process must be DPI-aware or the picture is cropped.

**First-run screens** (run 1 by window pictures, run 2 by the live widget list through UE4SS), in order:
`ShaderPreCache_BP_C` ("optimizing shaders"; ~45 min on the first launch with the window mostly minimized,
32 s on the second launch with the driver cache warm), `DialogBox_Photosensitivity_BP_C`,
`DialogBox_Autosave_BP_C`, `GammaSelection_BP_C`. The first two accept a Space key posted to the game window
(`PostMessage` `WM_KEYDOWN`/`WM_KEYUP`, no focus change, `qa/postkey.ps1`). The gamma screen ignored posted
Space, Enter and Down. The world behind the menus is `/Game/Maps/ConversationalMainMenu/ConversationalMainMenu`
with `OwMainMenuPlayerController_BP_C` and `IndianaGameInstance_BP_C`. The game never reached the save list
in these runs, so the Saved Games redirect itself has not been exercised yet (only LocalAppData and
Documents were requested and redirected).

**Owner state after each run** (`qa/owner_guard.py`: real save folder hashes, `%LOCALAPPDATA%\Arkansas`
absent, the game's Steam userdata folder, top-level names of LocalAppData, Documents, My Games and Saved
Games): UNCHANGED after run 1 (proxy only), run 2 (UE4SS) and run 3 (UE4SS with a vtable layout file).

## UE4SS on this build (2026-11-15)

- **Loads early through `dwmapi.dll`** (v4.0.0 layout: proxy in `Win64`, everything else in `Win64\ue4ss`).
  Its scanner found `GUObjectArray`, `GMalloc`, `FName::ToString`, `FName::FName`,
  `StaticConstructObject_Internal`, `GNatives` and `GameEngine::Tick` (engine version 5.4 on the second scan
  pass). Not found: `FUObjectHashTables::Get` (it falls back to iterating objects) and
  `ConsoleManagerSingleton` (3 candidates). It warned "VTable and scan addresses differ for UGameEngine::Tick.
  Potentially customized vtables", and the hooks for `CallFunctionByNameWithArguments` and
  `ProcessConsoleExec` failed to install.
- **Property reads work, UFunction calls silently do nothing** with the stock 5.4 layout:
  `PlayerController:IsLocalController()` returned false on the menu's own controller, every bool came back
  false and every enum 0, and calling a widget's accept handler changed nothing. UE4SS's "ProcessEvent address"
  was RVA `0xfbaa10`, which is the compiler's shared empty stub (it fills 64 of the first 200 slots of
  `UObject`'s vtable).
- **The real `UObject::ProcessEvent` is vtable slot 79** (offset `0x278`, RVA `0x1027aec`): the only function in
  the first 200 slots with Dumper-7's fingerprint `test dword ptr [reg+0xB0], 0x400` (FUNC_Native), at +0x52
  (`tools/find_processevent.py`, reading the vtable of `/Script/CoreUObject.Default__Object` from the test
  copy). UE4SS's stock 5.4 template has it at 77. UE4SS's own S.T.A.L.K.E.R. 2 config (also a Microsoft GDK
  UE5 build) differs from stock 5.4 by exactly two virtuals before `ProcessEvent`,
  `UObjectBaseUtility::GetVersePath` and `UObject::CollectSaveOverrides` (after `PreSave(const
  ITargetPlatform*)`), which gives 79; Obsidian's Grounded (UE 4.27) config adds none, so the extras come from
  the engine branch, not from Obsidian's code. Gothic 1 Remake's docs say "slot 95" for their build; slot 95
  here is a different function (no FUNC_Native test).
- **Run 3 with a generated `VTableLayout.ini`** (stock 5.4 template plus those two entries,
  `tools/make_vtable_layout.py`, asserts ProcessEvent at 79): UE4SS read it (it logged
  `UObject::ProcessConsoleExec = 0x290`, slot 82 as generated), but its start-up log stopped after
  `Already constructed: ["/Script/CoreUObject", "Interface"]` and the bridge mod gave no heartbeat within 90 s
  (in run 2 the next line was `Object CDO: ...` and the bridge answered at 23 s).
  **Correction (2026-11-16): run 3 crashed, it did not hang.** The copy's own crash report
  (`Saved/Crashes/.../CrashContext.runtime-xml`, `EXCEPTION_ACCESS_VIOLATION`) and UE4SS's dump written 3 s after
  its last log line show UE4SS's init thread faulting inside a game function it called (return address
  exe+0x14795ff, next to `StaticConstructObject_Internal`), during the "Object CDO" step; the process then sat
  in the crash handler, which looked like a hang. Not pursued: UE4SS was replaced by our own core (below).
- **The +2 slot shift is confirmed independently** by UE4SS's own run-2 log: it printed the `UGameEngine`
  vtable around the stock `Tick` slot, and the entry two slots later equals the address its signature scan found
  for `UGameEngine::Tick`.
- **Native module is `/Script/Arkansas`** (classes keep the first game's names: `IndianaUserWidget`,
  `DialogBoxWidget` with `ExecuteConfirm`/`OnInputConfirm`/`OnConfirmedDialogBox`, `GammaSelectionWidget`
  with `OnGammaAccepted`/`OnGammaSelectFinished`/`AcceptButton` (`GlowButtonWithBacking_BP_C`)).
- **Agent bridge:** a 70-line Lua mod (`AgentBridge`, only active when `TOW2_SANDBOX_DIR` is set) runs Lua
  dropped into `<instance>\sandbox\bridge\cmd.lua` on the game thread and writes `out.txt`; `qa/bridge.py`
  sends code and waits. It answered every call in run 2 (Lua 5.4, object queries, class reflection).

## Reaching gameplay in an isolated copy (2026-11-16)

- **The gamma screen was the first-run flow, not a hang.** The game writes `bIsFirstTimeFrontEndFlow=True` under
  `[/Script/Arkansas.IndianaGameUserSettings]` in `GameUserSettings.ini`. Set to `False` in the copy's own (sandboxed)
  file, the copy starts at "PRESS ANY KEY" in 20 s with the redirect DLL only, no UE4SS (run 4).
- **Save isolation seen from the game:** the Load screen of the copy shows "MAX SAVES: 0 / 100" (the owner has 5
  saves), and `sandbox.log` shows `SHGetKnownFolderPath(SavedGames, 0x8000) -> <instance>\sandbox\SavedGames`.
- **Steam writes are blocked in test copies.** The test copy runs under the owner's account with the real app id,
  so the proxy now also answers `ISteamUserStats` writes (SetAchievement, IndicateAchievementProgress, StoreStats,
  SetStat*, ResetAllStats, ClearAchievement) and `ISteamRemoteStorage` writes (FileWrite*, FileDelete,
  FileForget, FileShare, SetSyncPlatforms, SetCloudEnabledForApp, write batches) locally; cloud reads are logged
  and passed on. The vtable slot of each method is read from the shipped `steam_api64.dll`'s own flat wrappers
  (`mov rax,[rcx]; rex.w jmp [rax+disp]`); the exe requests only `STEAMUSERSTATS_INTERFACE_VERSION012`,
  `STEAMREMOTESTORAGE_INTERFACE_VERSION016` and `STEAMAPPS_INTERFACE_VERSION008`. The guard is on 0.4 s after start
  (24 slots); an unreadable wrapper stops the process with `0x5AFE0010` (tested with a fake `steam_api64.dll`).
  Steam still records play time and "last played" for the app (the appmanifest's `LastPlayed` changed).
- **Copied saves load into gameplay.** Copies of the owner's backed-up saves (game version 1.0.4.1, without
  `steam_autocloud.vdf`) placed in `<instance>\sandbox\SavedGames\TheOuterWorlds2\` load with
  `SaveGameManager.LoadGame(Filename=<save folder name>)`: world `/Game/Maps/06_Cloister/0601_Overlands/0601_CL_P`,
  pawn `IndianaPlayerCharacter_BP_C`, full HUD. `SaveGame(Filename, bIgnoreSuperNova, SaveGameType)`,
  `RequestAutosave` and `Quicksave` each wrote a folder (`CoopTest1`, `Autosave00`, `Quicksave00`) under the
  sandbox only; `Quickload` returned `Success`; `KismetSystemLibrary.QuitGame` exited cleanly (no crash report).
  Owner state UNCHANGED after every run (runs 4 to 10); `%LOCALAPPDATA%\Arkansas` still absent.
- Posted mouse clicks are unreliable here (Slate takes hover from the real cursor), posted keys work. Menus are
  driven through the widgets' own functions instead: the game's `ButtonBase.StartSimulateHoldClick` presses a button.

## Our native core instead of UE4SS (2026-11-16)

`tow2coop.dll` (C++17, llvm-mingw, static runtime, imports only system DLLs), loaded by the `dsound.dll` proxy from
a thread. It checks the build (PE timestamp `0x89B6FE2C`, SizeOfImage `0x0B137000`, CheckSum `0x0ACBC2F0`, and the
bytes at every address it uses), waits for the `GameEngine` instance, checks the reflection layouts against live
objects, then detours `UGameEngine::Tick` (RVA `0x14165E4`, frame pump on the game thread) and `UObject::ProcessEvent`
(RVA `0x1027AEC`, event trace). On any mismatch it stays off. In test copies it serves the hub's bridge protocol
(`tools/bridge`, pipe `tow2-<i>`). Addresses on this build: `GUObjectArray` RVA `0xA3F65C0`, `FNamePool` RVA
`0xA5DD780` (ready flag `0xA436C40`), `FName::FName(const TCHAR*, EFindName)` RVA `0x109EC84`.

Layouts measured on the live process (differences from stock 5.4 in bold):
- UObject, UStruct (`ChildProperties` 0x50, `PropertiesSize` 0x58), UFunction (`FunctionFlags` 0xB0,
  `ParmsSize` 0xB6), FUObjectItem 0x18 bytes, FName 8 bytes, UEnum names at 0x40: stock.
- **FField uses the 8-byte tagged `FFieldVariant`** (owner pointer with bit 0 = UObject): `Next` 0x18, name 0x20,
  so FProperty `ArrayDim` 0x30, `ElementSize` 0x34, `PropertyFlags` 0x38, `Offset_Internal` 0x44, subclass data 0x70.
- **`FArrayProperty` keeps `ArrayFlags` before `Inner`** (inner at +8). Reading it at +0 crashed the game once
  (run 8); the core now detects it from `AActor::Tags`, and an access violation inside the core during a bridge
  request now fails that request instead of the game (vectored handler turned into a C++ exception; tested).
- `UClass::ClassDefaultObject` is found at run time (the slot of `Object`'s class that points at
  `Default__Object`); ProcessEvent = vtable slot 79 (the entry of `Default__Object` equals the detour target).
- **Proof of calls:** `PlayerController.IsLocalController()` returns `true` (UE4SS returned false),
  `K2_GetActorLocation()` returns the pawn position, the frame pump runs every frame (5 to 16 ms).

## Two copies on one PC (2026-11-16)

- A second copy stops at "There is already an instance of the game running." The lock is a named mutex
  `TheOuterWorlds2` (`CreateMutexW` in the exe's imports). In test copies the proxy appends a per-sandbox suffix to
  every named mutex the exe creates (`TheOuterWorlds2-sbx<hash>`); tested (a mutex created through the patched import
  is not visible under its plain name, pass-through keeps the name). Players on two PCs do not need it.
- Low settings (`qa/prep_instance.py`: scalability 0, resolution scale 50, 60 fps cap) give 6.1 to 6.4 GB working
  set per copy in gameplay; two copies plus the desktop fit in 32 GB.

## Choosing the co-op route (2026-11-16)

- **Native replication probe: failed.** `GameNetDriver` is `OnlineSubsystemSteam.SteamNetDriver` (fallback
  `IpNetDriver`); with the definition switched to `IpNetDriver` in memory, `open <0601 map>?listen` through
  `KismetSystemLibrary.ExecuteConsoleCommand` crashed the host in the engine's `LoadMap` (access violation at
  exe+0x13a3e96, no mod code on the stack). A raw engine map open bypasses the game's own travel and save flow; the
  game's systems (quests, dialogue, inventory, AI) are single-player code anyway.
- **Chosen: two worlds plus our own sync** (as Gothic 1 Remake Coop): each player runs the game, the host owns the
  campaign, a UDP protocol in `tow2coop.dll` carries the shared state.
- **Stable entity ids:** level-placed actors come from World Partition cells and have the same path in both worlds
  (`..._C_UAID_<hex>_<n>`; 37 of 37 enemies matched). Runtime-spawned actors (companions, spawner output, puppets)
  have per-process names and need another scheme.

## Co-op slices that work on one PC (2026-11-16, same save loaded in both copies)

Host and client found each other through the hub's gamepool pair variables (`up --pairs 1`, UDP port 27960).
- **Remote player:** each copy sends its pawn's location, rotation and velocity at 30 Hz; the other copy shows an
  NPC body (`Townie_DockWorker_Male_C`, spawned with `BeginDeferredActorSpawnFromClass` with `AutoPossessAI=Disabled`)
  that follows with 100 ms interpolation. Positions matched exactly in both directions. Without a controller the
  movement component runs no physics, so writing its `Velocity` drives the NPC locomotion animation: with
  `MovementMode=MOVE_Walking` the puppet walks when the remote player walks (anim `SmoothedSpeed` 400 = the player's
  `MaxWalkSpeed`) and stands when they stop; `MOVE_None` left it in an odd pose. Pictures: client sees the host's puppet.
- **Real input:** posted `W` (held) walks the player; posted left clicks swing the equipped weapon.
- **Combat, host authority over NPC health and death:** the host streams health/position of level-placed NPCs within
  80 m of a player at 10 Hz; the client sets its copy's health (`HealthComponent.SetHealth`), kills it when the host's
  dies (`Suicide`), snaps it when more than 3 m off, and sends the damage its own player deals as a delta that the host
  applies. `qa/test_npc_sync.py`: host damage reaches the client, client damage reaches the host and comes back,
  host kill kills the client's copy (all PASS). `qa/test_melee.py` with real clicks: host hit 253 -> 223 on both;
  client hits killed an NPC on both (75 -> 0).
- **Disconnect:** a killed client is dropped by the host after 5 s (puppet removed, port free) and a restarted
  client is accepted again (new id, puppet back). Peers also drop and rejoin across their own blocking save loads.
- **Loading a save too early crashes the game:** `SaveGameManager.LoadGame` called about 12 s after start (the front
  end still initializing) crashed in the engine's `LoadMap` (exe+0x1429567, reading 0xffffffffffffffff, no mod code
  on the stack) 5 times out of 5; after 60 s at the menu the same load works (fresh starts and the rejoin test).
  Wrong guesses checked on the way: co-op traffic (none flows before gameplay), inherited std handles (a detached
  launch crashed too), leftover sandbox state (a reset sandbox crashed too). The test helper now waits 60 s.

- **Join (run 16):** the host saves (`SaveGame` "CoopHost") and serves that folder over TCP (co-op port + 1); the
  client downloads it into its own `CoopHost_<host id>` save folder and loads it once its front end has run 60 s.
  Result on one PC: 2.13 MB in 14 s, same map, the client spawns at the host's spot, both see each other, the
  client's own saves untouched. Save folders come from the exe's own `SHGetKnownFolderPath` import slot, so a test
  copy uses its sandbox and a player's install its real Saved Games folder.
- **30-minute soak (run 15):** both players moved and attacked with posted input; 60 checks every 30 s found both
  copies answering, packets flowing, the puppet at the real player's position (median 0, max 1.69 m while moving) and
  synced NPC health equal (max difference 0). The client crashed at the very end (game code exe+0x161af38, reading
  0x610; cause unknown). The soak's liveness check cannot see a process sitting in the crash handler; next runs
  must watch the crash folder too.

## Story, travel and interactions (2026-11-17, same PC, two copies)

- **Quest data is reflected:** every quest is in the `QuestBundleSet` asset
  `/Game/Exported/BaseGame/Design/Quests/quests` (158 quests: name, GUID, node arrays with objective addendum ids and
  the global variables objectives test). Scripts are GUID-based: `QuestScripts.StartQuest/MentionQuest/
  TriggerQuestAddendum/DebugAdvanceQuest`, `QuestConditionals.IsQuestAddendumTriggered/IsQuestStateActive/...`,
  `GlobalVariableScripts.SetGlobalValue`, `GlobalVariableConditionals.IsGlobalValue(Guid, EComparisonOperator, Int)`;
  `OwQuestManager.GetQuestState_BP(Guid)` gives `EQuestState` (Invalid, InProgress, Succeeded, Failed, Mentioned).
  `OwQuestManager.QuestComplete/QuestFail(Name)` do nothing in this shipping build, and no script completes a quest.
- **Story sync:** the host polls quest states, addendums and the 441 objective globals (1151 probes, 80 per frame)
  and sends every change through an ordered, acknowledged channel; the client applies them with the scripts above.
  Quest end states, which the client cannot apply, reach it by a **save resync**: the client asks, the host saves and
  hands the campaign over (the join path), the client reloads and asks for an idempotent replay of the story log.
  Tested: quest start, addendum and global reached the client live; a quest driven to Succeeded on the host reached
  Succeeded on the client through a resync.
- **Following the host across maps:** `TravelManager.BeginTravelToBP(destination data asset)` is the game's own
  travel (e.g. `0605_OAHQHardLoad`, the hard load into the OA headquarters). When the host is in another map than the
  client for 5 s, it hands the campaign over; tested 0601 -> 0605: the client followed and spawned 0.5 m from the host.
- **Doors and containers:** door state lives in each door's `OCLComponent` (`AttemptOpen/AttemptClose/LockOCL`,
  `GetCurrentState`); its change delegate is native, so doors are polled (16 per frame). Container removals are
  caught on the game's `Container.HandleItemRemoved` (a ProcessEvent call, matched by FName index so Blueprint
  overrides count) and applied with `InventoryComponent.RemoveItemFromActor`. Rules: door state shared, container
  contents shared, the taker keeps the item. Tested both ways for doors, host to client for a chest item.
- **Puppet hygiene:** a puppet NPC without an AI controller is risky (game code expects one), so the puppet keeps its
  controller with the behaviour tree stopped (`BrainComponent.StopLogic`, re-checked every 2 s), god mode,
  `SetInteractionsEnabled(false)`, `SetTargetable(false)`, barks off. Walking into it and hitting it: no crash.
- **Two copies on one 8 GB GPU:** without a budget the host stopped with "Out of video memory trying to allocate a
  rendering resource" after moving into new areas. A per-copy `Engine.ini` (`r.Streaming.PoolSize=600`, fixed pool,
  Nanite streaming pool 256 MB, skin cache 128 MB) and 960x540 keep both copies at about 5.3 GB together.

## Saves, test-copy pitfalls and a player install (2026-11-16, same PC, two copies)

Observed (runs 30-33):

- **A spawned stand-in ends up in the host's saves.** A save made while the client's stand-in NPC existed, loaded by
  the host alone with co-op off, still held a `Townie_DockWorker_Male_C_<number>` where the stand-in stood. This
  engine branch has `AActor.bShouldSaveGameState` (not in stock UE 5.4; true on every NPC checked). Set to false on a
  live actor it is ignored (saved anyway); set between `BeginDeferredActorSpawnFromClass` and `FinishSpawningActor`
  the NPC is left out of the save while a default one next to it is kept. With that, test_resume finds no leftover.
- **The game deletes `steam_appid.txt` when it closes normally.** The next start then asked Steam to launch the store
  install (Steam's "Select launch option" dialog) and the copy quit with exit code 53. Rewriting the file read-only
  before every start fixed it.
- **A minimized copy cannot finish a save.** `SaveGame` returns `AsyncSaveTask_Begun`, but the save waits for its
  screenshot; until the window is restored, `LoadGame` returns `Restricted`, and a co-op hand-over (which saves first)
  fails.
- **A minimized copy in the 0601 overland grows fast after a load:** about 4 GB per minute at ~140 frames/s, up to
  ~18 GB before the game frees ~6 GB; the same curve with the mod's co-op off (control run), so it is the game's own
  behaviour. Restored behind other windows it ran at ~7 frames/s and stayed flat.
- **Memory:** starts after a forced stop reached 13-19 GB resident (working set = private) about 20 s after launch,
  with and without the mod, while the NVIDIA shader cache sat at its size cap. After the owner raised the cap, each copy
  started alone once peaked at 7.8 / 4.1 GB; both copies wrote the same driver cache file (keyed by the exe, not the
  path). With the client started 60 s after the host, the full suite peaked at 9.9 GB per copy. Unverified that the
  cap was the cause.
- **The burst came back after an evening of other use (2026-11-16, run 49):** the host copy sat at 7 GB at the menu
  (runs 42-47: 3-5 GB), the map-travel load took free RAM under the 2 GB guard, and the next start after that forced
  stop reached 17 GB. One copy started alone and closed normally (`qa/warmup.ps1`) peaked at 17.2 GB and wrote a 1 GB
  driver cache file on exit; the next start of the same copy peaked at 4.5 GB, the other copy at 4.3 GB. So: before a
  two-copy session, warm one copy up alone and close it normally (a forced stop does not write the cache).
- **Real-input melee:** posted left clicks hit hostile NPCs, but swings at a friendly one (the game offers "Talk")
  do no damage; tests must pick targets with `CharacterBlueprintFunctionLibrary.GetTeamAttitudeTowards` = Hostile.
- **Suite (run 33, two copies on one PC):** auto-join from the main menu, NPC authority, story, doors and containers,
  quest-outcome resync, map-travel resync, co-op resume after restarting both games: PASS. Run 37: real-input melee
  from both players on a hostile NPC, and a join with a shared key: PASS; different keys: the host ignores the client.
- **30-minute soak (run 38):** both players moving and attacking with posted input, 60 checks: no problems, stand-in
  to player median 0.2 units, synced NPC health difference 0, no crash, 8.6 GB peak per copy.

Built (installer checked on a fake game folder only, never on a real install): `install.ps1` finds the game through
Steam, places the loader, the mod and `tow2coop.ini` (`role`, `peer`, `port`, `key`); the core reads the ini when no
environment variable is set (seen in a test copy). An optional shared key changes the packet marker so strangers who
reach the open port are ignored, and the campaign hand-over accepts only the joined player's address.

- **Stand-in look (run 39):** copying the `AppearanceComponent.Customization` struct, `SetDefaultHead/SetDefaultBody`
  and `EquipmentComponent.SetArmorVisualOverride(ArmorSet, Helmet)` with the player's equipped items, then
  `RefreshFullAppearance`, makes gender, head, customization and `CurrentBody` match the player in both worlds.

- **Armor changes after the join (run 50):** each player sends the class paths of its worn armor set and helmet every
  2 s; the other side applies them with `SetArmorVisualOverride`, using the class default object when it doesn't hold
  the same piece (checked first: a `/Game/` class whose default object is an `ArmorSet` / `Helmet`). The client took
  its helmet off, put another armor set on and both back; the override in the host's world followed each change within
  10 s (read back with `GetArmorVisualOverride`). Probe 53 (pictures from the host's idle camera, which orbits the
  player in third person after a while without input): the stand-in wore the player's suit and helmet; with the helmet
  override cleared it wore its own dock worker's hard hat (`HeadGear_DockWorker_01_Var1`, merged into the head mesh, no
  separate component); `AppearanceComponent.bHelmetVisible=false` + `RefreshFullAppearance` showed a bare head. So the
  look message also carries "helmet shown" (worn and the player's own `bHelmetVisible`), mirrored on the stand-in;
  run 53: helmet off -> no override and `bHelmetVisible` false, back on -> both restored.
- **Faults after a hand-over load (run 50):** the client's per-frame sync hit 23 caught access violations within
  1.4 s, 5-7 s after a resync `LoadGame`, in `ue::alive` reading a freed actor of the old world (the guard now logs the
  offset in the core: `+0x1f6c5`, mapped with `llvm-nm`). The old world is torn down over several frames before the
  world pointer changes, later than the 5 s pause then used. Fix: after a `LoadGame` the core started and the game
  accepted, wait until the world pointer changes (60 s at most), and read network data only after that world check.

Not done yet (see the acceptance matrix in the mod repo): enemies attacking the remote player, loot and inventory
rules beyond containers, dialogue sync, two-machine test.

## First two-PC test (2026-11-17, beta.2: the owner's PC and a friend's PC, both started by Steam)

Observed:

- Both `tow2coop.log` files: build check OK, "GameEngine instance up after 22125 ms" (host) and "15718 ms" (friend),
  then "co-op: Steam is not initialised in this game", and nothing after it. beta.2 checked `SteamAPI_GetHSteamUser()`
  once, at the mod's start, and left co-op off for the whole session.
- Test copies (started directly, `steam_appid.txt` plus `SteamAppId`/`SteamGameId` in the environment): the proxy's
  Steam guard saw `SteamAPI_Init` about 0.4 s after the start. The core found the Steam session up 14 s after it
  loaded (copy 0, "Steam link ready (app 1449110)"). So no one-PC test met the late case.
- `SteamAPI_Init` is a delay import of `steam_api64.dll`. Only UE's `FSteamSharedModule::ObtainSteamClientInstanceHandle`
  (exe+0x1b3cc10) calls it, and that function has two callers:
  - `FOnlineSubsystemSteam::InitSteamworksClient` (exe+0x1b3c9a0), the only caller of `SteamAPI_RestartAppIfNecessary`.
    It is called from `FOnlineSubsystemSteam::Init` (exe+0x245b2ac: `OnlineSubsystemSteam`, `SteamSockets`,
    `OnlineAsyncTaskThreadSteam %s`).
  - A Steam Deck check (exe+0x3f94790: `Valve`, `Jupiter`, `Galileo`, `SteamDeck`) that drops its handle when it returns.
- Obsidian's platform layer checks the default platform service in `OEIPlatformServiceSubsystem::Initialize`
  (exe+0x1e701c4, a vtable entry; source path `Plugins/OEIPlatformService/.../OEIPlatformServiceSubsystem.cpp`). Its
  messages: "Game not started from Steam and failed to relaunch in Steam, please delete the steam_appid.txt file if it
  exists in game's binary directory"; "Failed to initialize default platform service %s which is necessary, game
  exiting...".

Derived:

- A game started by Steam sets up its Steam session after the point where the mod starts. A direct start (test copies)
  sets it up before that point.
- The Steam Deck check's early init does not last. `SteamShared` keeps only a weak pointer (the stored handle bumps the
  weak count), so releasing the handle shuts Steam down again. Read from the code, not observed.

Unknown:

- When exactly a Steam launch sets up Steam. beta.3 logs it: "Steam link ready (app ...), N s after the mod started".
- Whether the title screen ("PRESS ANY KEY") plays a part. The platform check runs in a subsystem `Initialize`, so it is
  probably part of the engine start, before the first map. Not checked on a real launch.

Fix (beta.3): the Steam link retries every 2 s on the game thread until the session is up. Checked by test 54
(`qa/test_steam_late.py`: one copy as Steam client at the menu, the mod told to ignore Steam for 40 s): the log reads
"waiting for the game's Steam session", then "Steam link ready (app 1449110), 41 s after the mod started", with no
caught faults. Not yet retested across two PCs.

## Second two-PC test (2026-11-17, beta.3)

Observed:

- Both logs: "Steam link ready (app 1449110)" at the first frame, 0 s and 2 s after the mod started (GameEngine up after
  14.3 s and 15.0 s).
- Friend (client, no host name set): "steam: joining the host <host>" 8 s after its link was ready, so it found the
  host by rich presence. Its session state then cycled every ~10 s for 3 minutes: 1 (connecting), then 5 (problem
  detected locally). It sent 97 messages and received 0.
- Host: nothing after "Steam link ready", no "a friend joins". It never matched the friend's `join:<host>` presence
  among the friends playing the game. The session-request callback (set with
  `SetGlobalCallback_MessagesSessionRequest`, accepts Steam friends) never accepted either. beta.3 logged neither path,
  so which one failed is unknown.

Guesses (not checked):

- The host can't see the friend's game. Steam privacy hides it, so the friend is missing from the host's list of
  friends playing.
- The game doesn't deliver the global session-request callback to the mod, or the callback doesn't accept.

Fix (beta.4): while it has no client, the host calls `AcceptSessionWithUser` for every immediate friend every 2 s. The
call does nothing when that friend has no pending session. The host now logs:
- each session request that reaches the callback;
- each session accepted this way;
- the friend it adopts when the first message arrives;
- every 30 s, the friends playing, with their `tow2coop` presence and the relay status.

Test 55 (`qa/test_steam_late.py` on one copy on real Steam): the accept pass ran over the account's 10 friends, with no
caught faults. It could not be tested with a second account on one PC.

## Third two-PC test (2026-11-17, beta.4 on the host, beta.3 on the friend)

Observed in both logs:

- **Link and join:**
  - The host logged "steam: no client yet; friends playing: none" every 30 s. The friend's game details were not visible
    to it.
  - Then "accepted a session from <friend>" and "a friend joins", with the session connected.
  - The campaign went over Steam: 2.1 MB in 7 s. The friend loaded it, and its saves went off.
- **During play:** both stand-ins were shown, and packets flowed both ways for 15 minutes. Map changes repeated the
  hand-over four times.
- **Callback:** no "session request" line appeared, so the session-request callback never reached the mod. Polling
  `AcceptSessionWithUser` did the job.

Reported by the players:

- **Worked:** damage with firearms reaches the other world, both ways. Armor and helmet changes follow.
- **Stand-in:** it moves and jumps, but in a frozen pose. Without a helmet, it is bald.
- **Enemies:** an enemy attacked by one player only takes the damage in the other world; it doesn't move.
- **Map changes:** the friend follows, but only after a wait.
- **Loot:** items looted from a dead enemy are still there in the other world.
- **Ammo:** after a map change, the friend's magazine has the host's ammo count. Every hand-over replaces the friend's
  character with a copy of the host's.

Probe 56 (one copy, bridge and posted keys):

- **Hair:** an NPC body ignores `AppearanceComponent.Customization` (hair, beard, colours) unless
  `bEnableCustomization` is set. The dock-worker NPCs have it off. With it on, the copied hair shows (pictures
  hair-1/hair-2).
- **Animation:** an NPC that keeps its AI controller runs its movement component (mode `MOVE_NavWalking`), which zeroes
  a written `CharacterMovement.Velocity` every frame:
  - with the component ticking, the anim instance's `SmoothedSpeed` stays at 0;
  - with its tick off, `SmoothedSpeed` reaches 400 within 0.5 s;
  - `MOVE_Falling` sets the anim's `bMovementStateIsInAir`.

  The one-PC walk check (anim speed 400) predates the change that kept the controller.
- **Dead NPCs:** a killed NPC stays the same character, with health 0 and its own `CharacterInventoryComponent`. After a
  save and reload it keeps the same level name and `IDComponent.ID` (ObsidianIDComponent, a GUID). `Corpse` actors did
  not appear for a killed sprat or soldier; the class has its own inventory and `HandleItemRemoved`, probably for
  bodies placed in the level.
- **Looting through the UI** (E, then R): the trace shows `<item>:OnInventoryItemRemoved`,
  `EquipmentComponent:OnItemRemovedFromInventory`, `<NPC class>:ItemRemoved` (`IndianaCharacter.ItemRemoved`, with an
  `ItemStack` parameter: Item, Count, bLootable) and `ItemContainer_BP_C:OnItemRemoved`. Then the player's
  `ItemAdded` calls follow.
- **Inventory API:** `GiveItemToActor`, `RemoveItemFromActor`, `GetItemCountOnActor`, `AddCurrency` and `RemoveCurrency`
  on `InventoryComponent`. The player has a `PlayerInventoryComponent`.

Runs 57-58, probe 59 and test 60 (beta.5, two copies on one PC):

- The fixes for the stand-in hold:
  - with its movement tick off, the animation speed reaches 400 while the client walks and drops to 0 when it stops;
  - it goes into the in-air state during a jump;
  - with customization enabled, the stand-in wears the player's hair asset.
- **Loot at death:** the game rebuilds a dead NPC's inventory from its loot table. Items given to the NPC before its
  death were gone after it, and its own NPC gear stays, marked not lootable. On a joined pair, the generated loot of
  the same OA soldier was identical in both worlds: a Zenith drug and a lucky sprat foot. Taking one item in either
  world removed it in the other. A death sends no take.
- **Travel:** the host hands the campaign over 8.4 s after its world changed, without waiting 10 s to settle and 5 s
  of "client elsewhere". The client was in the new map 21 s after the host.
- **Steam send rate:** `GetConfigValueInfo` names ids 9, 10 and 11 `SendBufferSize`, `SendRateMin` and `SendRateMax`
  in this `steam_api64.dll`. The mod sets them to 8 MB, 1 MB/s and 1 MB/s.

## Own characters, looks, trading and XP (2026-11-17 and 18, two copies on one PC)

Goal from the owner: play the whole game together, each player with their own character; the friend joins with
nothing, items are picked up separately and can be traded.

Probe 61 (save files, read from copies of the test saves):

- `SaveGame.dat` is one zlib stream. Inside is an "SGDF" container: a 23-byte header (u32 5, "SGDF", a version
  block), then named entries `[u32 name length][name\0][u32 size][data]`: one per level, plus `Player.dat`,
  `Quests.dat` and others.
- `Player.dat` is the character: inventory with each item's own state (magazine, mods, durability), equipment, money
  (the `Bit_Cartridge_C` item), level, perks and appearance. It starts with 01, version 5, "PSHF", the version block,
  two section sizes, 01 05 01 00 00 00, then the player's position (3 doubles at offset 46) and rotation (3 doubles at
  70), matched against the pawn's location.
- `Metadata.dat` ("GMHF") holds the container's unpacked size (a u32 7 bytes after the end of the "Release\0"
  string) and the character's name. The loader checks the size: a wrong one fails with
  `CompressedGameStateUnpackFailure`.
- Splice: one save's `Player.dat` put into another save (with the target save's position and rotation, and the size in
  `Metadata.dat` updated) loads that world with this character. Level 17 came back after a splice into a level-20 save.

What the mod does with it (runs 62, 65, 67):

- Before loading a campaign the host hands over, the client's mod makes a private save of its own world ("CoopSelf"),
  keeps its `Player.dat` per host, and splices it into the received campaign. First join: the character of the
  friend's own newest save, emptied (no items, no money).
- Result: the client joined with 0 bits and none of the host's items, gained 123 bits, 50 ammo and 2 levels, and after
  the host reloaded and handed over again it still had them (the host kept its 25314 bits and level 17), standing next
  to the host.
- Looks: each player sends its `AppearanceComponent` fields as text (numbers, asset paths, head, body, gender tag), and
  the stand-in applies them. When the client changed its hair, the host's stand-in followed in 2 s.

Party XP (2026-11-18, from the disassembly of the build in "Game identity"; run in game in runs 70-74):

- No reflected function reads XP: `PlayerInfoComponent` only has `GetLevel`, `SetLevel` and the exec command
  `AddXPDebug(XP)`.
- The native table entry of `AddXPDebug` leads to its exec thunk, which inlines the gain logic:
  - XP is an int32 at `PlayerInfoComponent + 0x93C`, and the level is at `+0x330`;
  - no gain at the maximum level;
  - the amount is multiplied by an XP multiplier stat;
  - the XP is capped at the next level's threshold, with a level-up loop through a virtual call;
  - a native delegate at `+0x560` is broadcast with (XP, reason, a TArray, leveled up). It is not visible to
    ProcessEvent.
- The same code exists out of line as `bool AddXP(int32 XP, EXPGainReason Reason, TArray* Details)` at RVA 0x139E03C.
  It has 9 direct callers, which pass reasons 3 (point of interest), 13 (bits to XP), 0 (enemy killed), 1 (quest
  completed), 10 (interaction) and variable ones. Each caller passes a pointer to an empty TArray.
- The mod hooks it (20-byte prologue) to share XP by reason:
  - kills, lock picks, hacks, conversations, interactions, automechs and taming go both ways;
  - quest XP goes from the host to the client, and the client's own quest XP is dropped (its quest graph follows the
    host's);
  - discoveries, collectibles, bits-to-XP and debug XP are not shared.
- Runs 70-74 (`qa/test_xp.py`, gains made through the hooked function): every case behaved as above. A gain of 20 XP
  became 23 in both copies (the multiplier stat). No listener filled the TArray.

Trading (runs 70-74):

- Run 70: the client's store worked, the host's did not. The host already had shotgun shells, and taking part of a
  stack from the player fires `OnItemCountChangedInInventory` (on the `EquipmentComponent` and `InventoryAmmoPool`),
  not `IndianaCharacter.ItemRemoved`.
- Run 71: the client's store failed. The client's container already held the host's shells, and items added to a stack
  the container already has fire no `Container.HandleItemAdded`.
- The mod now notes, every 0.25 s, what the containers within 5 m of the player hold (`InventoryComponent.Items`,
  `FItemStack` of 0x18 bytes). A container that gained items of a class the player lost in the last second got them
  from the player. Runs 73-74: both ways passed, including the merge into an existing stack.
- Run 73: the client was placed above the container by the test. At the next hand-over, `LoadGame` returned
  "Restricted", and the client stayed in its old world. It now tries again every 2 s for up to 2 minutes. In run 74,
  3 forced refusals were followed by a load 18 s later.

Probes 77-80 and run 81 (2026-11-18, before publishing):

- **Kill XP:** a death applied the way the mod applies one (`HealthComponent.Suicide`) gives the local player no XP, so
  a kill is not counted twice (once by the game, once by the party XP).
- **Saves:** `SaveGame` returns "Restricted" while another save is still running. Two calls back to back gave
  `AsyncSaveTask_Begun`, then `Restricted`; the call 3 s later was accepted again.
  - A new save folder appearing in the save directory does not block saves.
  - The "refused" private save seen at every hand-over (runs 70-76) came from the mod itself: after the first save
    finished, it started a second one, then retried for 6 s. Each hand-over waited 6 s, but the character was kept.
  - Fixed: one save per hand-over. In run 81 the save was not refused, and the client was in the new map 22 s after
    the host (25 s before).
- **Container screens:** the quick-loot popup of an Order of the Ascendant bag offers only "Steal" and "Hold to steal
  all".
  - The inventory screen has no drop; only the cheat `OwCheatManager.DropAllItemsDebug` exists.
  - `IndianaUI` has an `ItemTransferClass` (`ItemTransfer_BP`, two item lists), opened from native code; not reached.
  - So players can't yet put an item where the other player can take it, and trading through containers is not usable
    in play.
  - Ship landing pads are travel destinations (`/Game/Maps/TravelDestinationData/<map>/SHIP_<map>_Landing_Pad`). An
    early save in `0101_JP` had no ship storage.

Memory: with a cold NVIDIA shader cache (5.8 GB cap, shared with the owner's game), a test copy's start reached
13-17 GB and the memory guard stopped it (runs 67-69). The owner raised the cache size to unlimited on 2026-11-18.

## Map changes together, giving items through the stand-in (2026-11-18, two copies on one PC)

The owner found the 22 s between the host's and the client's arrival in a new map (run 81) far too long, and chose
giving items "from the other player's stand-in".

Observed (probes 82-84, runs 85-86):

- **Travel:** the game has one `TravelManager` (a transient object). Its `ActiveTravelDestination` (+0x60) is set when
  a travel starts and stays set through the load until `IsTravelingBP()` turns false, about 7 s after the new world
  object appears; then it is cleared (`LastTravelDestination` keeps it). A `LoadGame` sets nothing there.
  - `BeginTravelToBP(destination, false, false, false)` returns at once; the game thread then blocks for the load
    (about 9 s from 0601 into 0605 on this PC).
  - Probe 82: a second copy starting the same travel 0.5 s after the first was in the new world 0.6 s after it.
  - A travel started on the joined client with `bTriggerAutosave` true wrote no autosave (saves stay off across it).
- **Transfer screen:** the two-list screen (`ItemTransfer_BP`, class in `IndianaUI.ItemTransferClass`, +0x540) is
  opened by `UIndianaUI::OpenItemTransfer(AActor*)`, RVA `0x6BC7764` in CL-2994920.
  - The function gets the actor's inventory through a virtual call, creates the widget (RVA `0x6BB0FDC`) and binds it
    to that inventory (RVA `0x6C63734`).
  - It has no direct caller (a virtual or a delegate target, so the standard calling convention).
  - It was found from the `&UItemTransferWidget::OnInventoryItemAdded` name strings that `AddDynamic` leaves in the
    binding function.
  - Called on the stand-in NPC, the screen crashed the game while listing the NPC's own clothes (probe 83: a read of
    `0xFFFFFFFFFFFFFFFF` at RVA `0x6C7DBF2`, an item stat function).
  - Called on a freshly spawned, empty `BP_Container_Chest_Small` (hidden, no collision, `bLootManagerLootApplied`
    true, `bShouldSaveGameState` false), it opened: "PLAYER" and "CONTAINER" lists and "ESC Exit". Escape destroys
    the widget.
  - Posted keys (arrows, E, Enter, Space) and `ButtonBase.StartSimulateHoldClick` on its item buttons moved nothing:
    the screen follows the real mouse cursor.
  - The quick-loot popup of an unowned world chest offers "Take", "Hold E Take All", "F Equip" and "C Compare"; it
    has no store.
- **Runs 85 and 86** (`qa/run_quick.ps1` with `qa/test_give.py` and `qa/test_resync.py B2C`): all steps PASS, owner
  state unchanged.
  - Giving: E in front of the other player's stand-in opened the screen; 3 shells moved into the chest reached the
    other player both ways (sent, received, confirmed); with the receiver refusing gifts, they came back to the giver
    after 20 s.
  - Travel: the host travelled into 0605 and the client was in the new map 1.1 s after it, with no save sent (run 81:
    22 s); they saw each other 8 s after arriving. With following off on the client, the hand-over brought it 20.5 s
    after the host. When the client travelled first, the host followed and arrived at the same time.

What the mod does with it (beta.6):

- Each player's own travel start (the destination set) goes to the other player, who starts the same travel at once,
  unless the host is in combat. The host waits up to 90 s for a following client instead of handing over its
  campaign. The hand-over stays for loads that are no travel (a save loaded, a death) and for a refused travel.
- After a travel, the sync starts 1 s after the game's own travel ends, instead of 10 s after the world change.
- Giving: E (seen in the game window's messages, so posted keys work in tests) while looking at the stand-in from
  3.5 m or less opens the transfer screen on a hidden, empty chest spawned at the stand-in. What lands in the chest is
  removed and sent to the other player's inventory (`GiveItemToPlayer`, with the game's notification). It is resent
  every 0.5 s until confirmed, and comes back to the giver after 20 s without an answer. A weapon or armor arrives as
  a new copy with its level and rarity, without its mods.

Unknown: ship travel and fast travel with following; a mouse transfer in the screen (expected to move the real item
into the chest, not exercised by the bots); a gamepad's interact button.

## Gifts with their mods, death and reload (2026-11-18, two copies on one PC)

The owner asked for given items to keep their mods, and for faster deaths and reloads.

Observed (probes 85-88, runs 87-89):

- **Mods on an item:** a weapon keeps them in `WeaponModContainer.ModSlots` (+0x40 of the container at `Weapon`
  +0x1EC8): a `TMap<EModSlot, FWeaponModSlot>` whose values hold `DefaultMod` (the slot's stock mod class) and
  `CurrentMod` (the installed mod object). The mod reads the map's sparse array directly: elements of 0xD0 bytes
  (the key byte padded to 8, the 0xC0 value, two hash ints), allocation bits inline at +0x10.
  - An armor keeps them in `ArmorModSlots`, a fixed array of 7 `FArmorSetModSlot` (`CurrentModClass` at +0x8), plus
    `BonusArmoringSlot`. A unique armor's resistance mod sits in its class defaults: the mod skips mods equal to the
    defaults.
  - This build reads properties with the 8-byte `FFieldVariant` layout: `ArrayDim` is at +0x30 of an `FProperty`,
    not at the stock +0x38. Reading the stock offset gave 1 instead of 7.
- **Installing a mod on an item in the bag** (not equipped):
  - `UWeapon::EquipModByClass(const TSubclassOf<UWeaponMod>&, bool bIgnoreSlotModdability)`, RVA 0x6D3ED8C. It
    reads the item's `WeaponModContainer` at +0x1EC8 and has 13 callers in the exe, among them the debug command
    `EquipmentComponent.EquipWeaponModByClassDebug`.
  - The armor install that `EquipArmorModByClassDebug` calls, RVA 0x179A0F0, with (armor, &class, false, true, true,
    true).
  - Both worked on items in the bag: a silencer on a pistol, defensive plates on an armor (read back from the item).
- **Loading:** `SaveGameManager.LoadGame` and `Quickload` are virtual (slots 0x2F8 and 0x310). The `LoadGame`
  implementation (RVA 0x6B30B90, no direct caller) takes (this, `FString*` name, bool) and returns `ELoadGameResult`.
  - Two of the `OnQueryLatestSaveFinished` implementations (menus' Continue) call it through slot 0x2F8.
- **Death screen** (a client with god mode off, `HealthComponent.Suicide`): Dying after 1 s, Dead after 4 s, then
  "GAME OVER / YOU DIED" with "You don't have enough trauma kits to be revived", then Continue, Load Game, Main Menu
  and Quit. Posted keys don't drive it (real mouse).
  - Continue loads the newest save, which in a joined game is a local save (the friend's own campaign, or the mod's
    private "CoopSelf"): a stale or foreign world under the host's game.
  - `HealthComponent.Resurrect(false, 1.0)` brings a dead player back to Healthy.
- **Results:**
  - Gifts: a pistol with a silencer and an armor with defensive plates went into the giving chest; the client's
    copies had the same mod.
  - Death and reload (run 89): the mod sends the save before the host's `LoadGame` (which returns only once the new
    world is up, probe 88: about 16 s, ticking the engine inside); the client was in its new world 2.7 s after the
    host (before: about 20 s). The client died: the host's current game came, the client's own `LoadGame` of a local
    save was refused (`LoadCanceledByUser`), and the host's game loaded 7.3 s later, the client alive next to the
    host. A revive after the death dropped the fetched game.
  - Run 87 failed two checks (the save went out after the host's load; the auto-join test expected the client at the
    menu while the host's first load had already brought it in). Run 89, after both fixes: 11 steps PASS, owner
    state unchanged.

What the mod does with it (beta.6):

- A gift carries `"<item class>|<mod class>|..."`; the receiver installs each mod on its new copy with the game's
  install functions.
- The host's `LoadGame` (hooked) hands that very save folder over at once, without a new save and without a second
  hand-over once its world is up.
- In a joined game, the friend's own loads of local saves are refused. When the friend dies, the host's current game
  is fetched and held; the friend's Continue releases it, with the last kept character (a dead character is never
  kept).

Unknown: Continue clicked for real; Quickload (its implementation is expected to call `LoadGame`); the host's death
screen with a real click.

## Quests live both ways, one clock, menus (2026-11-18, two copies on one PC)

The owner asked for everything testable to be fixed or built, so that the whole campaign plays well with two.

Observed (probe 89, runs 90-93):

- **Quest state:** `GetQuestState_BP` calls the quest manager's lookup by GUID (RVA 0x14BEE2C, 15 callers; the quests
  are a set at +0x1C0 of the manager, elements of 0x1C0 bytes). The returned instance holds the `EQuestState` byte at
  +0x1A4.
  - Writing the host's outcome there gave the client the same end state live: the 5 s check passed and nothing
    reloaded.
- **Menus pause the game:** with the host's inventory open, `GameplayStatics.IsGamePaused` was true and the host's
  calendar hour stood still. The client's NPCs, corrected towards the host's frozen ones, snapped 45 times in 15 s.
- **Calendar:** one `GameCalendar` object. `K2_GetRawHour` gives the hour (float, 0-24); `K2_AdvanceTime(days, hours,
  minutes, seconds)` moves it; `RequestPauseTime` and `RequestUnpauseTime` (a named request) stop and start it.
  - Game time runs about 25 game seconds per real second.
  - The host's 3-hour jump did not reach the client before the clock sync.
- **Fast travel** (`BeginTravelToBP` with a `POI_Data/06_Cloister/*_FastTravel` point in the same map) sets the same
  `ActiveTravelDestination`: the client followed and landed 0-49 units from the host.
- **Container takes:** `Container.HandleItemRemoved` fires only when a whole stack leaves; part of a stack fires
  nothing on the container.
- **Test tool fix:** `qa/gamewin.py` sent F10 for "F9" (VK 0x79); VK_F9 is 0x78.

What the mod does with it:

- **Story both ways.** The client also polls quests, addendums and the objective globals and sends its own changes;
  the host applies them to its campaign, checking each quest, addendum and global against the quest data.
  - Each side sets its baseline to what it applies, so nothing echoes.
  - After a reload the client drops its unsent changes; each packet carries the first sequence still valid.
  - Outcomes go through the quest lookup above when the receiving quest graph has not got there by itself.
- **One clock.** Every 2 s the host sends its calendar hour. A client more than 0.1 h behind advances to it; a client
  ahead pauses its calendar until the host's catches up.
- **Menus.** No NPC corrections while the host's game is paused.
- **Container takes** come from the container watch (containers within 5 m, every 0.25 s), whole or part of a stack.
  A take applied from the other player lowers the watch's snapshot first, so it is not sent back.

Results:

- Run 91: the client started a quest, drove it to Succeeded, and the host's campaign had the same outcome, without a
  reload. The client's clock followed a 3-hour jump within 1 s. With the host's inventory open, 1 snap instead of 45.
- Run 92: the quick suite, 13 steps, all PASS, owner state unchanged; part of a stack taken out of a chest was
  mirrored both ways.

- Run 93: F9 posted to the host went through the game's own `Quickload` (RVA 0x1B2478C, vtable slot 0x310), not
  `LoadGame`: the client followed only after the host's world was up (23 s). Hooked too: the newest `Quicksave*`
  folder goes to the client before the load.
- Run 95: the client waited about 15 s for its own private save before loading the host's reload. Reloads now take
  the client's last kept character (the world goes back in time too).
- Run 96: a 30 s quiet window after one fetch made the client ignore the first notice of the next hand-over, which
  then came only after the host's load (9 s late). Each notice now carries its hand-over's number.
- Run 97 (the quick suite, 13 steps): all PASS, owner state unchanged.
  - The host's reload: the client 0.8 s behind. F9: 0.1 s behind.
  - The client back 6.4 s after Continue.
  - The clock followed a 3-hour jump within 1 s; 0 NPC snaps during the host's menu (looked at before each update).

Unknown: ship travel; quest XP for outcomes set directly (the game's completion logic doesn't run for them).

## Co-op combat (2026-11-19, two copies on one PC)

The owner asked for real co-op fights: the enemies split over both players, a detection of either starts the fight for
both, and the enemies fight in both games.

Observed (probes 90-91, runs 98-102), all in the 0601 Cloister overland, near three Ursopods (`Bear_T3_*`, melee
creatures in one encounter, ~200 m from the test save's start):

- **`AIDetectionComponent.ForceFullDetection(DetectedActor, ExternalStimulusLocation, bPermitArkansasAI)`** (the
  component is `IndianaAiController.AIDetectionComponent`):
  - the NPC goes Alerted (awareness ~96), its current target (`K2_GetCurrentTarget`) is that actor, and
    `OwBaseCharacter.IsInCombat` is true;
  - this worked on the local player and on the stand-in, with the local player 8 m away;
  - with the local player 200 m away, or dead (a teleport over a cliff killed the host twice), nothing happened: far
    from a game's player, the NPCs' AI sleeps in that game.
- **`AITargetInfoPriorityFunctionLibrary.ForceOverridePrimaryTarget(Controller, Priority, ForcedTargetActor)`** returns
  an opaque `PriorityNodeHandle` (0x18 bytes).
  - It does nothing until `PriorityNodeFunctionLibrary.Activate(Handle&, Owner, FromWhere)`; `Deactivate(Handle&)`
    removes it.
  - The handle holds a reference: `Deactivate`'s native thunk (RVA 0x12afaf4) releases a reference controller at +8.
    Exactly one copy may stay alive. In run 99 a stale copy was released twice and crashed the game (the mod's
    incremental registry rebuild had copied it).
- **Natural detection.** The client, in the open 7 m in front of an Ursopod, was seen in 1-3 s in its own game.
  - The Ursopods switched targets between the player and both companions (Marisol, Azahelia); each game has its own
    companions.
  - With the host near, the host's game saw the client's stand-in on its own too: on the stand-in's default team
    (`T_Generic_Friendly`), and also on `T_Generic_NeutralToAll` (run 100).
- **Untargetable.** `IndianaCharacter.CastSpell(/Game/Blueprints/Spells/AI/Spell_MakeNpcUntargetable)` (target Self,
  cooldown 0) adds `UntargetableByAIStatusEffect`; `RemoveSpell` takes it off. Cast after remove returned Success every
  time (probe 91).
  - `StatusEffectManagerComponent.BPAddEffect` with the effect class itself did not stick: the class default is
    `UseDurationTime` with a duration of 0.
  - An Ursopod already on an actor kept it after the effect, and after `K2_RequestNewTarget(Untargetable)`.
- **Test helpers on `HealthComponent`:** `SetPseudoGod` (a player takes the hits, never dies), `bImmuneToFallDamage`
  and `GetLastHitInstigator` (who hit last).
- **Two mod bugs found on the way:**
  - An NPC packet with more than 44 records (33 bytes each) was larger than the 1500-byte receive buffer, and
    `recvfrom` then failed for that frame's packets.
  - `ue::alive` read the header of an NPC whose memory had been released, after the host walked 200 m away and the
    area streamed out (an access violation the frame guard caught).

What the mod does with it:

- **One game decides each NPC's fight.** That is the host's game, or the client's when the host is more than 50 m
  away and the client is 20 m nearer (back to the host under 40 m).
  - The deciding game says, 10 times a second, whom the NPC fights: the host, the client, nobody, or someone else
    (another NPC by its registry id, or a companion by its class).
  - The other game's copy is made to detect that player (or their stand-in, or the same companion) and pinned on them
    with an activated `ForceOverridePrimaryTarget`; it is looked at again every second.
- **Hints.** When the client's copy fights the client while the host's copy fights nobody, the client sends a hint
  and the host's copy detects the client's stand-in: a detection by either starts the fight for both.
- **The stand-ins.**
  - Out of a fight, each carries `Spell_MakeNpcUntargetable` and the `T_Generic_NeutralToAll` team, so each player is
    only seen in their own game.
  - While the party fights (either player in combat, or an NPC on either), each is on the player's team and
    targetable: the deciding game's AI may pick it, and the fight spreads over both players.
  - A dead player's stand-in leaves the other game until they live again.
- **Positions and pins.** In a fight, a copy that drifted more than 1.5 m is eased back (15 % of the gap per update)
  and snapped only past 6 m. A pin moves with its NPC when the registry is rebuilt, and is never copied.
- **Liveness.** `ue::alive` first checks the object's memory (`VirtualQuery`, a per-frame cache of committed
  regions).
- **Packets.** At most 32 NPC records per packet, in as many packets as needed; protocol version 3.

Results:

- Run 98 (first version): the host's game saw the client's stand-in before the client's own game saw the client (the
  stand-in was on the player's team out of a fight); 49 snaps of 3 m and more in about 35 s.
- Run 99: 2 snaps (eased instead), 99 % agreement. Then the client's game crashed when the host walked away
  (`Deactivate` on a stale pin copy), and the host's core faulted (a streamed-out NPC's memory).
- Runs 100-108: the stand-in neutral out of a fight was not enough (creatures went for it); the untargetable spell
  fixed that. A cached spell class pointer led, a minute later, to a `StaticMeshActor`: the class had been unloaded
  and its memory and object slot reused. A dead player's stand-in now leaves the other game. The host's game paused
  2 s after the test killed the Ursopods; the host then sends no NPC updates, as designed for menus.
- Run 110 (the quick suite): the screen was the game's flaw offer "Reckless Leadership" (companions had gone down
  often in the test fights): "Hold E Accept / Hold Esc Reject", and the game stays paused until then. A tapped Escape
  does not close it; the test now holds Escape 2 s whenever the host's game is paused.
- Run 109 (`qa/test_combat.py`, 27 checks): all PASS.
  - The client was seen in its own game after 3.6 s; the host's game joined 0.4 s later, through 3 hints.
  - Over 20 s, the client's copies fought whom the host's game said in 73 of 73 samples. The words seen were the host,
    the client and the companions; both players took hits from Ursopods in their own game.
  - The copies were 1.4 m apart (median), with 2 snaps of fighting NPCs and 278 eased corrections.
  - With the host 150 m away, the client's game decided within 0.6 s; it handed back 0.04 s after the host returned.
  - The client "dead" (test switch): its stand-in left the host's game in 0.5 s and the enemies let go at once.
  - After the kills, the stand-ins were out of the fight and untargetable again within 0.5 s.
- Run 111 (the quick suite with the combat step, 14 steps): all PASS, owner state unchanged.
  - Combat: 64 of 67 samples agreed; both players took hits; the copies were 1.3 m apart (median), with 2 snaps.
  - After E, the host's game was paused by a flaw offer; the test held Escape and the fight went on.

Unknown: fights against many armed humans; NPCs the game spawns during play (not level-placed: not shared yet).

## The other player's weapon, spawned enemies, host and join from the menu (2026-11-19, two copies on one PC)

Observed (probes 92-98, runs 112-118):
- **A weapon on an NPC:** `EquipmentComponent.EquipWeaponBP(WeaponClass)` gives the stand-in a new weapon of that class;
  the NPC then carries that weapon's NPC visuals (`NPC_Visuals_*`, `NPC_Weapon_Animations_*`, the 3P sound object).
  `Weapon.BeginPrimaryTrigger` / `EndPrimaryTrigger` on it fire for real: ammo goes down, the NPC fire montage plays
  (`AS_NPC_RepRifle_Fire_01_Montage`). Calling the visuals' `TriggerEventBP(Fire)` alone plays no montage.
  `bIgnoresAmmoConsumption` keeps the magazine full. `AISequencerBehaviors` (the cinematic controls for NPCs) has
  `HolsterWeapon`, `EquipWeaponByInstance`, `SetOrientation`, `PlayAnimMontage`, `FireWeaponStart/Stop`.
- **Aim:** an NPC with a stopped brain fires along its body (pitch 0); `AIController.K2_SetFocalPoint` changes nothing.
  `AISequencerBehaviors.SetOrientation(Character, TR_WeaponAim, {FaceLocation})` turns the weapon aim exactly there
  (asked 125.7 / 16.7 degrees, got 125.2 / 16.7), but only within the upper body's reach: a point behind the stand-in
  is not aimed at.
- **The player's weapon events:** every visuals object of the player's weapon (4-7 of them: 1P, 3P, NPC, shared, sound)
  gets `TriggerEventBP(EWeaponEvent)` through ProcessEvent, so a ProcessEvent hook sees each shot (Fire 7 and
  FireHipfire 8 in the same frame), melee swing (23/24/26/28...), reload (19) and aim (42/43). The visuals' outer chain
  leads to the weapon. A real shot by the player at a 10-round rifle: 58 TriggerPull, 7, 8; empty: 19 Reload.
- **The damage function:** `IndianaGameplayStatics::execK2_ApplyDamage` builds its arguments and calls one function
  (RVA 0x10AB944 in build 24225441; 17 direct callers, weapons and melee among them):
  `bool(causer, info, FIndianaDamageEvent*, damage event, instigator, bool, ?)`; it takes `instigator ? instigator :
  causer` as the source pawn. Returning false there drops the hit: the stand-in's shots at the other player in its
  world did 9 health of damage without the hook (3 shots), none with it.
- **NPC melee animations:** `/Game/Art/ANIM/Character/NPC/{MeleeLt,MeleeHvy,Unarmed}/AS_NPC_*_ATK_*_Montage`, played on
  the stand-in with `Character.PlayAnimMontage`; the weapon kind comes from `IndianaAiCharacter.K2_GetCurrentWeaponAnimations`
  (`ARK_MeleeLt_NPC_WeaponAnimations_C`...). `TriggerQuickMelee` and `K2_TriggerMeleeAction` did nothing on the stand-in.
- **Crouch:** `K2_SetWantCrouch`, `Crouch()` and setting `bIsCrouched` don't crouch the stand-in: the animation's crouch
  progress comes from the movement component, which the stand-in doesn't tick (it zeroes the velocity that drives the
  walk animation, 8a). What does crouch it: probe 103, in the next section.
- **Spawned enemies:** the 12 encounters near the test save start reference 41 level-placed characters (41 of 41 with
  `_UAID_`): encounters animate placed, dormant NPCs (`AICharacterToSpawnerPair`). `CharacterSpawner` blueprints
  exist (`Spawner_Mantiwasp_Outdoor_Hive`, `Spawner_HoverBot_Hatch*`, `Spawner_Human_Door_Reusable`); none was loaded
  there. tow2-coop shares any hostile AI character without `_UAID_` by class and place (15 m, 1.5 s grace, else a copy).
- **The main menu is a conversation** with the Moon Man (`ConversationalMainMenu`): its choices are
  `ConversationResponse_BP_C` widgets in `ConversationWidget_BP_C.ResponsesContainer` (a VerticalBox; the class is in
  `ResponseWidgetClass`, the live ones in `SelectableResponseWidgets`). A widget made with
  `WidgetBlueprintLibrary.Create(ResponseWidgetClass)` and `AddChildToVerticalBox` shows as one more choice; its text
  is `MessageTextBlock.SetText(FText)` (rich text) and `ResponseNumberTextBlock.SetText(FString)`. The game binds its own
  responses' clicks natively, so a click on an added one does nothing in the game. Number keys reach the game as
  input actions that ProcessEvent doesn't see, and the game picks a choice on the key's **release**: swallowing the
  key-down only (probe 95) let the game run "Continue" on the release.
- **Clicks and display scaling:** posted mouse messages don't reach UMG (it reads the real cursor); a window procedure
  sees them. A script that is not DPI aware sees the 960 x 540 window as 640 x 360 at 150 % and its posted coordinates
  are scaled up by Windows (485, 507 arrives as 727, 760). The UI's coordinates: `SlateBlueprintLibrary.LocalToViewport`
  (PixelPosition), the same space as the game's client pixels at 1:1 (`WidgetLayoutLibrary.GetViewportSize` 960 x 540).
- **FText from a script:** `KismetTextLibrary.Conv_StringToText` gives the FText bytes to pass as a TextProperty argument.
- **Who hit an NPC:** `HealthComponent.GetLastHitInstigator` gives the last hitter (probe 101: the client's pawn after
  its blade swings, `Marisol_C` after the companion's). A health-delta damage report (the client's world tells the host
  what its NPCs lost) counted the client's companions' hits too: 106 reports in 20 s of a fight the client stayed out
  of (run 118); with the last hitter checked, 0 (runs 119-121). `IndianaGameplayStatics.K2_ApplyDamage` with the
  player as causer did no damage; with a companion it did.

- **After a fight:** the stand-in's own AI may put its weapon away (its `GetCurrentWeapon` becomes None); the game's
  flaw offer ("Hold Esc Reject") can pause the client's game as well as the host's (run 124), and a paused world fires
  no shot and swings no blade.

Results:
- Run 117: the co-op menu (host by keys, client by clicks and keys, the client joined and both saw each other), the
  stand-in's weapon (16/16: 3 rounds fired, 3 counted, 3 fired by the stand-in, its last shot 2.1 m from the host's at
  40 m, its 3 shots at the client dropped, swings, holster, reload) and spawned enemies (15/15). Owner state unchanged.

## Weapon mods, a gun's bash, crouch on the stand-in (2026-11-19, two copies on one PC)

Observed (probes 102-103, runs 127-128):
- **Weapon mods:** the game's own mod install (`weapon_equip_mod`, RVA 0x6D3ED8C, already used for given weapons) puts
  a mod class on the stand-in's weapon. `WeaponMod_Barrel_Silencer_MR_Tactical_C` installed on the host's
  `Weapon_MR_Tactical_Unique_DeathSentence_C` showed on its stand-in's rifle in the client's world (run 127).
- **A gun's quick melee (bash):** the player's weapon visuals get `TriggerEventBP(QuickMelee)` (35). The NPC bashes are
  `/Game/Art/ANIM/Character/NPC/LngGn/AS_NPC_LngGn_QuickMelee_Cross_Montage`, `Hndgn1h/AS_NPC_Hndgn1h_QuickMelee01_Montage`,
  `Hndgn2h/AS_NPC_Hndgn2h_QuickMelee_GunButt01_Montage` and `HvyWpn/AS_NPC_HvyWpn_QuickMelee_Montage`; tow2-coop picks
  one from the stand-in's weapon animation set (`NPC_Weapon_Animations_<code>`). The rifle's bash played
  `AS_NPC_LngGn_QuickMelee_Cross_Montage` on the stand-in (run 127). The quick melee key is not in the InputSettings
  action mappings, and posted V, F and C did not bash: the test sends the visuals event.
- **Crouch:** an NPC crouches when its AI controller asks (`IndianaAiController.K2_SetWantCrouch(bCrouch)`) and its
  movement component ticks on the ground (probe 103):
  - In the mode the stand-in's placement leaves it in (MOVE_Falling; MOVE_Walking turned into Falling within 0.3 s of
    ticking), neither `bWantsToCrouch`, `bForceCrouch` nor the controller's request crouched it.
  - In MOVE_NavWalking with the controller's request: `bIsCrouched` true, the animation's `CrouchedProgressNormalized`
    0.54 after 0.25 s and 0.78 after 0.5 s, then 1.0 with `bIsFullyCrouched` and the capsule at 55 (standing 96). With
    the request off it stood within 0.3 s.
  - While the controller asks, `bWantsToCrouch` false doesn't stand it up; without the request, `bWantsToCrouch` alone
    doesn't crouch it.
  - The animation instance's fields set directly are overwritten at its next update. `bIsCrouched` set by hand, then a
    tick: the animation's `bIsCrouching` turns true, its progress stays 0 and the capsule full.
  - A companion (`Azahelia_C`, NavWalking) crouched when its player did: capsule 92 to 52.8, progress 0.32 after 1.2 s
    and 1.0 after 2.5 s.
  - tow2-coop: on each change of the other player's crouch, the controller's request, NavWalking, the movement
    component ticking 1 s, then Walking again.

Results:
- Run 127: mods and bash PASS; crouch FAIL (`bWantsToCrouch` with a tick in Falling).
- Run 128: the stand-in's weapon and gestures 25/25 PASS: mods, bash, crouch (animation fully crouched, capsule 55)
  and stand-up (capsule 96), its movement component off and in Walking after. Owner's files unchanged.
- Run 129 (the quick suite through the menu): 15 of 16 steps. After the combat test the stand-in stayed crouched when
  the host stood up (capsule 55; cause not found). Since then the controller's request is repeated every 0.25 s until
  the stand-in matches (3 s at most), then every 5 s, and the stand-in's feet are placed on the player's: the center
  of the player's capsule is sent, so a stand-in of the other height (mid-change, or not stood up) sank or floated
  41 cm, and floating, its standing capsule would sit 41 cm above the player's.
- Run 130 (combat, then gestures): PASS. Run 131 (the quick suite): the stand-in crouched and stood up with its feet on
  the host's (+0.0 and -0.3 cm); 15 of 16 steps: in the spawned-enemies step the client's own bear was not alive within
  25 m of its spawn point and the host's copy was 10.7 m from it (the same step passed in runs 129 and 133). The test
  now spawns on the navigation mesh when it finds some (`NavigationSystemV1.K2_ProjectPointToNavigation` works from a
  script, probe 104; 20 m ahead of the host past the ledge it finds none).
- Run 132: the memory guard stopped both copies during the combat test (free RAM 1.9 GB; the copies at 10.3 and
  10.7 GB private, about 1.5 GB more than in runs 129 and 131). Run 133 (combat, gestures, spawned): PASS except the
  stand-in's last shot 4.29 m from the host's at 6.1 m. The test now aims the host's shots away from the other player
  (guess, not checked: the client's own body stopped the stand-in's shot where the host's shot passed the client's
  stand-in). Probe 104 (a held pair, combat then gestures then spawned): gestures PASS; combat and spawned enemies
  were disturbed by soldiers of another faction fighting nearby (the host down, the spawned bear killed).
- Run 134 (the quick suite through the menu, the final 0.3.0 build): 16 of 16 steps PASS (227 checks): the stand-in's
  last shot 0.26 m from the host's, crouched and standing feet +0.0 / -0.1 cm. Owner's files unchanged.

## Tools on the test PC (2026-11-15)

Present: Python 3.13, git, gh, Java 21 (Temurin), .NET SDK. Missing: any C/C++ compiler (the Visual Studio
2019 and 2022 folders are empty), CMake, Docker, Ghidra, 7-Zip. 19.7 GB free on the only drive, so a full
90 GB copy of the install does not fit; test copies must be hard links (multi-instance-testing.md).

## Playbook gaps

- The README quick start doesn't say the agent must run on the PC that runs the game. An agent on a
  machine that can't run the game can only do research. Proposed: one line in the quick start.
- engine-families.md (UE4/5 row) presents SteamStub as hiding `.text` from UE4SS (Crash Bandicoot 4). On
  The Outer Worlds 2, as on God of War 2018, SteamStub leaves `.text` plain. Proposed: "check `.text`
  entropy and `.pdata` first; inject late only when the code really is encrypted".
- engine-families.md (UE4/5 row): UE4SS can pick an empty stub as `ProcessEvent` on engine branches with
  extra `UObject` virtuals; property reads still work, so the failure is silent (every call returns
  zeros). Check one call with a known answer (`IsLocalController` on the local controller) before trusting
  UE4SS calls, and find the slot by the FUNC_Native fingerprint.
- multi-instance-testing.md: a minimized UE game compiles its shader pipeline cache at a fraction of the
  speed; restoring the window behind every other window without activating it keeps the owner's screen
  free and the compile fast.
- DIRECTORY entry for Gothic 1 Remake Coop: dead URL and stale status (separate PR).
- multi-instance-testing.md: test copies that run under the owner's Steam account with the real app id write
  achievements, stats and Steam Cloud files to that account. Guard them in the proxy (vtable slots read from the
  game's own `steam_api64.dll` flat wrappers), and rename the game's single-instance mutex per sandbox.
- automated-qa.md / in-game-bridge.md: on Slate (UE4/5), posted mouse clicks hit-test against the real cursor's
  hover, so they land on the wrong widget; posted keys work. Drive menus through the widgets' own functions (here
  `ButtonBase.StartSimulateHoldClick`) and use a first-run settings flag to skip first-run screens.
- engine-families.md (UE5): check `FFieldVariant` size (8-byte tagged on this 5.4 branch) and whether
  `FArrayProperty` keeps `ArrayFlags` before `Inner` before trusting stock 5.4 FProperty offsets.
- multi-instance-testing.md: (1) a game may delete `steam_appid.txt` on a clean exit, after which the next copy asks
  Steam to launch the store install; restore it before every start. (2) A minimized UE copy may never finish an async
  save (it waits for the save screenshot). (3) Start-up memory bursts can come from a full graphics-driver shader cache;
  start copies one at a time and check the driver's cache size before blaming the mod.
- how-coop-mods-work.md (puppets): a spawned stand-in can be written into the host's save. Test "save with the
  stand-in present, load alone, look for it"; on this branch `bShouldSaveGameState=false` before `FinishSpawning`
  keeps it out.
