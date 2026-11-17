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
