# Crash Bandicoot 4: It's About Time — co-op triage notes

Started 2026-10-30. Phase 0 (triage), following AGENTS.md and playbook/triage.md.

## Game identity (rule 5)

| Field | Value |
|---|---|
| Title | Crash Bandicoot 4: It's About Time (Toys for Bob / Activision, 2020; PC 2021) |
| Store | Steam (app 1378990), confirmed by tester 2026-10-30 |
| Version / build id | Steam build; exe PE timestamp 1663342796 (2022-09-16 UTC); ProductVersion 1.0.0; Steam build id unknown |
| SHA-256 of main exe | f78bebcc3cf183aac47cc7e60faaed0bd9f9db4bef41e407b2c16a75a696ddec (Lava/Binaries/Win64/CrashBandicoot4.exe, 73,440,520 bytes) |
| Engine | Unreal Engine 4.24 (exe string ++UE4+Release-4.24); UE project name "Lava" |
| OS | Windows x64 |

## Observations (sourced)

- No co-op or online project for Crash 4 in DIRECTORY.md or games/ of coop-anything (checked 2026-10-30). No hub issue mentions it.
- Shipped multiplayer is local only, hotseat: Pass N. Play (up to 4 players take turns through levels, hand-off on death or checkpoint) and Bandicoot Battle (Checkpoint Race, Crate Combo). No simultaneous co-op, no online. — https://www.techradar.com/how-to/crash-bandicoot-4-multiplayer-is-it-online-and-co-op
- PC release: Battle.net exclusive on 2021-03-26 with always-online DRM; Steam release 2022-10-18. — https://en.wikipedia.org/wiki/Crash_Bandicoot_4:_It%27s_About_Time
- Wikipedia lists the engine as Unreal Engine 4. — same page.
- Active modding scene on Nexus Mods and GameBanana: cosmetic swaps (Mario mod) and gameplay/level-data changes (N.Hanced mod: added checkpoint and Aku Aku crates, changed relic times). — https://www.nexusmods.com/crashbandicoot4itsabouttime/mods/194, https://steamcommunity.com/app/1378990/discussions/0/4408543929284881139
- Alchemy-engine modding tools (Alchemist, IgzModelConverter) list N. Sane Trilogy, CTR Nitro-Fueled and Skylanders Imaginators; neither lists Crash 4. — https://thewarproom.com/printthread.php?tid=97, https://github.com/AdventureT/IgzModelConverter
- No existing Crash 4 online or simultaneous co-op mod found in web searches.

## Derived

- Path is 2 (build from zero): nothing usable exists to revive.
- Engine decides the route. If the install is UE4 (Engine/ folder, *.pak + *.utoc, a Win64-Shipping exe), the playbook's UE4SS + Dumper-7 route applies and is the realistic path. If it is an in-house/Alchemy-derived engine (*.igz / *.pak archives, no UE layout), it is a closed binary with no scripting: the hardest category.
- The hotseat mode means the game already supports several player profiles and controller hand-off; whether it can hold two Crash actors at once is unknown.

## Guesses (unverified)

- The Wikipedia engine claim may be wrong, since the fan tooling around this series is Alchemy/IGZ based. Treat as unconfirmed until the install is inspected.
- Battle.net always-online DRM may complicate running two copies on one PC for bot testing.

## Unknowns / next checks (need the tester's PC)

1. Steam build id (tester: Steam > Crash 4 > Properties > Updates).
2. Whether UE4SS loads on this build (in a copy of the install).
3. SDK dump of game classes (Dumper-7) to map the player pawn, camera, checkpoints, saves.

## Stopgap that needs no mod

- Steam Remote Play Together streams a local-multiplayer game to friends online; it would let friends join Pass N. Play turns. Whether Crash 4 is listed for it is unknown — check the Steam store page features list.

## Tester's goal (2026-10-30)

Couch co-op (split screen or two monitors) and online with friends.

## Plan derived from playbook/split-screen.md

- Split screen is a layer on top of multiplayer: two copies on one PC talking over the local network. So online co-op comes first; the couch version then reuses it via Couchlink (one copy per player, per-copy controller).
- Alternative couch route: a second Crash inside one running copy (the Dave the Diver approach). Cheaper than networking, but Crash 4's camera follows one player, so a shared screen needs camera work. Decide after the engine is known.
- Next session needs the tester's PC (desktop app link) for the install listing, exe hash and engine check.

## Install inspection (2026-10-30, read-only, from the tester's PC)

Observations:
- Layout: `Engine/`, `Lava/Binaries/Win64/CrashBandicoot4.exe`, `Lava/Content/Paks/` (646 `pakchunk*-WindowsNoEditor.pak`, about 22 GB), `Lava/Content/Movies/`. Standard UE4 packaged-game layout.
- Engine string in the exe: `++UE4+Release-4.24`. No `.utoc`/`.ucas` (no IoStore), no `.sig` files.
- No anti-cheat: no EasyAntiCheat or BattlEye folders; no `EasyAntiCheat` or `Denuvo` strings in the exe.
- The exe contains `OnlineSubsystemSteam`, `SteamNetDriver`, `IpNetDriver` and `OnlineSubsystemNull`, and references `steam_api64.dll`. So UE4's replication netcode and Steam networking are compiled in, even though the game ships no online mode.
- `steam_api64.dll` is not in the install folder listing: where it loads from is unknown.

Derived:
- Wikipedia's "Unreal Engine 4" is correct; the Alchemy guess was wrong.
- Route: UE4SS (Lua/C++ mods) plus a Dumper-7 SDK of the game's classes, per README "What you can co-op" (Unreal Engine 4/5). Because UE4 replication and SteamNetDriver are already in the binary, co-op may be able to use the engine's own netcode instead of a hand-made sync layer: unverified, needs the SDK dump to see whether Crash 4's game classes replicate anything.

## Workspace (2026-10-30)

- Test copy made by the tester: `<test copy>` (outside Steam). Verified: exe SHA-256 identical to the Steam install (f78bebcc…96ddec); all 810 files listed at depth ≤5 match name and size; Movies/134 spot check 399 files in both.
- UE4SS chosen: v3.0.1 (latest stable tag, release 2024-02-14), official asset `https://github.com/UE4SS-RE/RE-UE4SS/releases/download/v3.0.1/UE4SS_v3.0.1.zip`, SHA-256 `4b47d4bceddd2f561a4e395bfa00924ccfc945af576a2d0c613e6537846c57ec`, 5,523,402 bytes. GitHub shows no published digest for this 2024 release, so the hash is ours, recorded for pinning. README at the tag says it targets UE 4.12 to 5.3 (covers 4.24).
- Install method: proxy `dwmapi.dll` + `UE4SS.dll` + `UE4SS-settings.ini` + `Mods/` next to the copy's `Lava/Binaries/Win64/CrashBandicoot4.exe`.
- Pitfall to handle: a Steam game started outside Steam may call SteamAPI_RestartAppIfNecessary and relaunch the Steam install instead of the copy. Mitigation: `steam_appid.txt` containing `1378990` next to the copy's exe (unverified for this game).
- UE4SS ships a `SplitScreenMod` sample: UE4 has engine-level local split screen (multiple local players per viewport). Worth testing on Crash 4 as the cheap couch route.
- 2026-10-30 17:5x: UE4SS v3.0.1 installed into the test copy's `Lava/Binaries/Win64` with tester approval (23 files: dwmapi.dll, UE4SS.dll, UE4SS-settings.ini, Changelog.md, README.md, Mods/, plus steam_appid.txt = 1378990). One settings change from the release: `ConsoleEnabled = 1`. Steam install untouched. Load test pending (tester launches the copy's exe).

## UE4SS load test 1 (2026-10-30 22:05 UTC) — FAILED to find engine internals

Observations:
- Game launched from the test copy (steam_appid.txt worked: copy started, not the Steam install) and ran normally. UE4SS console opened and wrote UE4SS.log.
- Log: UE4SS v3.0.1 (git d935b5b) loaded; 6 PS scan attempts within ~1 s, each failing to find EngineVersion, GUObjectArray, GMalloc, FName::ToString, FName ctor, StaticConstructObject_Internal, FText ctor. "Scan failed". Nothing logged after.
- Exe section table: `.text` (47.7 MB) has entropy 8.00 (encrypted/compressed; normal code is ~6); entry point is in an extra section `.bind` (206,616 bytes, entropy 7.96). `.bind` as the entry section is the signature of Valve's SteamStub DRM wrapper, which decrypts `.text` at startup.
- UE4SS issue #720 (https://github.com/UE4SS-RE/RE-UE4SS/issues/720): someone else ran v3.0.1 on Crash 4 (UE 4.24.2) and got past the scan, but the game crashed at the main menu; a user-reported workaround is `HookInitGameState = 0` and `HookLoadMap = 0`. Which exe that reporter had is unknown.

Derived:
- UE4SS, loaded through the dwmapi.dll proxy before the game's entry point, scans `.text` while it is still encrypted by SteamStub, so every signature misses.

Options (not chosen yet; tester decides):
1. Manual injection (documented in UE4SS docs/installation-guide.md "Manual Injection"): remove dwmapi.dll, start the game, inject UE4SS.dll once the game is at the title screen (code decrypted by then). Needs an injector program; no DRM change.
2. Remove SteamStub from the test copy's exe with Steamless (open-source tool). Simplest for tooling, but it is DRM removal: legal grey area, tester's call; not done.
3. Set the issue #720 hook workaround in either case.

## Option 1 chosen: manual injection (2026-10-30 22:18 UTC, tester approved)

- Written for this project (no third-party injector): `inject-ue4ss.ps1` + `inject-ue4ss.bat` in the test copy's Win64 folder. Plain LoadLibraryW + CreateRemoteThread from PowerShell Add-Type; refuses to inject unless the running exe path contains `Crash4-CoopTest`; waits for the tester to press Enter at the title screen.
- Settings: `HookInitGameState = 0` (v3.0.1 has no `HookLoadMap` key; issue #720 workaround partly applied).
- Tester must delete `dwmapi.dll` from the test copy first (the bridge cannot delete files), or UE4SS loads twice.
- Testing limit: this session cannot launch the game; the tester launches and presses Enter, this session reads UE4SS.log.

## UE4SS load test 2 (2026-10-30 22:20 UTC): injection works, then fatal error

Observations:
- Injected at the title screen with inject-ue4ss.ps1 (dwmapi.dll deleted by tester). PS scan succeeded on attempt 1: EngineVersion 4.24, GUObjectArray, GMalloc, FName::ToString, FName ctor, StaticConstructObject_Internal, FText ctor all found. Member offsets resolved, mods started, "Event loop start".
- Then the game hit a fatal error ~118 s after launch. Crash report (`%LOCALAPPDATA%\CrashBandicoot4\Saved\Crashes\UE4CC-Windows-0A2D769D…`): EXCEPTION_ACCESS_VIOLATION reading 0xffffffff, engine 4.24.2. Minidump (parsed by hand): faulting RIP in VCRUNTIME140.dll+0x1cca7 (memcpy); stack: UE4SS.dll+0x37cab3, +0x37c982, +0x554aaa, then CrashBandicoot4.exe frames. RDX/RSI = 0x006e006f00690067 = UTF-16 text "gion" used as a pointer.
- Game config/saves live in `%LOCALAPPDATA%\CrashBandicoot4\Saved\` (Config, Crashes, Logs, SaveGames). The test copy shares this folder with the Steam install.

Derived:
- A UE4SS hook called from game code copied a string through a field that holds text, not a pointer: a struct layout UE4SS gets wrong for this build. Which hook is unknown (no symbols). String-taking hooks are CallFunctionByNameWithArguments and LocalPlayerExec.

Test 3 config (written 22:3x UTC):
- Hooks off: InitGameState, CallFunctionByNameWithArguments, BeginPlay, LocalPlayerExec. On: ProcessInternal, ProcessLocalScriptFunction.
- Mods: only Keybinds + new `CoopAutoDump` (10 s after start: DumpAllObjects, then GenerateSDK). Console/cheat/BP-loader mods off.
- Risk: saves are shared with the real game; tester asked to back up SaveGames first (rule 2).

## Test 3 (22:30 UTC): same crash; root cause found

Observations:
- With hooks InitGameState/CallFunctionByName/BeginPlay/LocalPlayerExec off, the game crashed with an identical stack (VCRUNTIME140 memcpy <- UE4SS+0x37cab3 <- +0x37c982 <- +0x554aaa <- CrashBandicoot4.exe+0x1fe7b97). RSI = 0x006f006f00420000 (UTF-16 text again).
- CoopAutoDump did not start: `mods.txt` starts with a UTF-8 BOM and UE4SS v3.0.1 then ignores the first entry (the stock file loses CheatManagerEnablerMod the same way).
- Symbolized with the official `zDEV-UE4SS_v3.0.1.zip` (SHA-256 af777d2af33fce4d520de35a8665e417376bcc530fec647778012ce6cdf51b2b; its UE4SS.dll is byte-identical to the shipped one, SHA-256 8ac18fbf…2b26) and a small hand-written PDB reader: +0x554aaa = `RC::Unreal::HookedLoadMap`+0xea, +0x37c982 = `FURL::FURL`+0x32, +0x37cab3 = `FString::FString`+0x53.

Derived:
- UE4SS's LoadMap hook copies the FURL argument by value; Crash 4's FURL layout differs from stock 4.24, so an FString inside it is read from the wrong offset. Matches the issue #720 workaround.
- v3.0.1 does support `HookLoadMap` (SettingsManager.cpp line 112) though the shipped ini omits the key (default on).

Test 4 config: `HookLoadMap = 0` added; mods.txt rewritten without BOM.
- Playbook note for the PR: UE4SS v3.0.1 on Crash 4 Steam needs (a) late injection because of SteamStub, (b) HookLoadMap = 0.

## Test 4 (22:36 UTC): UE4SS stable on Crash 4 Steam; first class map

Observations:
- No crash with `HookLoadMap = 0`. CoopAutoDump ran: DumpAllObjects 22:36:23–24, GenerateSDK done 22:36:42. Outputs: `UE4SS_ObjectDump.txt` (57.8 MB) and `CXXHeaderDump/` (932 files, 5.3 MB). Dump taken at the title/menu, so only menu worlds (`/UI005_SPMenu/Maps/...`) are loaded; level map names still unknown.
- Player character chain: `ALavaCombatCharacter : ATfbCombatCharacter : ACharacter`, with `UTfbCombatCharacterMovementComponent : UCharacterMovementComponent` and a GAS `UTfbCombatAbilitySystemComponent` (Toys for Bob "Tfb" framework modules: TfbCore, TfbCombat, TfbCamera, TfbStateMachine, Phasmid).
- Unused multiplayer classes ship in the game: `ALavaMPGameMode` (PlayerCount, AreAllPlayersConnected, ...), `ALavaMPGameModeLobby`, `ALavaMPGameModeMatch`, `ALavaMPGameState` (OnRep_MatchState), `ALavaMPPlayerState`, `AMPLavaPlayerController`, plus `ABuddyPlayerStart` and `AHandoffPlayerStart` (Pass N. Play).
- Networking functions in the game's own modules (counts from headers): Lava 9 OnRep / 18 Server / 12 Client / 2 Multicast; TfbCore 7/3/1/3; TfbCombat 3/6/4/3; Phasmid 21/1/4/2. Examples: `ServerSetReplicatedInputEvent`, `ServerTryActivateAbilityWithPayload`, `Server_InstigatorDamageClaim`, `Multicast_DamageResult`, `OnRep_Health`, `Server_ChangeControlledCharacter`, `Server_ReliablePlayerConnected`.
- Network drivers present as classes: IpNetDriver, SteamNetDriver, DemoNetDriver.

Derived:
- The character/combat framework is built for client-server replication (input sent to server, predicted abilities, server damage claims, replicated health). Likely shared with Toys for Bob's later online game; unverified.
- Best first experiment for online co-op: UE's own listen server. Copy A `open <level>?listen`, copy B `open 127.0.0.1`, and see what replicates (second Crash visible? moves? crates/enemies?). This is the "engine has MP" row of the triage table, not "closed binary".

Setup for next session: CoopAutoDump off; ConsoleEnablerMod on (F10 opens the UE console). Need: level map names (dump again while in a level), whether two copies run at once on one PC.

## In-level report (23:11 UTC, CoopTools v3)

- World `/L113/Maps/L113_MASTER` (sublevels L113_Art03, L113_design, ...). GameMode `BP_GameMode_Crash_SinglePlayer_C`, GameState `BP_GameState_Crash_C`.
- One `LavaPlayerController`, Pawn `BP_CP3701_C` (the playable character), LocalPlayer `LavaLocalPlayer`. Enemies are TfbCombatCharacters with `BP_Base_Enemy_AIController_C`.
- UEHelpers.GetPlayerController fails here (its `Pawn:IsPlayerControlled()` check), so mods should not rely on it.
- Bridge pitfall: committing a file in the same parallel batch as the write that creates it sends the old content. Write first, commit after.
- Next: listen-server test. Copy A `open /L113/Maps/L113_MASTER?listen`, copy B `open 127.0.0.1`.

## Listen-server test 1 (23:13 UTC)

Observations:
- Host: `open /L113/Maps/L113_MASTER?listen` in copy A reloaded the level (World none at 23:13:03, back at 23:13:41 with a new GameMode instance, still BP_GameMode_Crash_SinglePlayer_C). No crash.
- Copy B (second process, plain launch, before injection): fatal error. Crash report UE4CC-Windows-6DCC6AD1…: ACCESS_VIOLATION reading 0x0 at CrashBandicoot4.exe+0xc31cf1; UE4SS not loaded in that process; process created 23:13:38, crashed ~23:14:34 (about 56 s, during startup/title). steam_api64/steamclient64/gameoverlayrenderer64 loaded.
- No published Nucleus Co-op handler for Crash 4 found (web search; tester's local Nucleus handlers folder has none).

Derived / guesses:
- The game does not tolerate a second instance as-is. Suspects: shared `%LOCALAPPDATA%\CrashBandicoot4\Saved` (config/save locks) or the same Steam user in two instances. Unverified.

Next: `launch-player2.bat` = `-windowed -ResX=1280 -ResY=720 -nosteam -userdir="<test copy root>\Player2" -log`.

## Second-instance test 2 (23:18 UTC): launch-player2.bat

Observations:
- `-userdir` works: the second copy created its own `<test copy root>\Player2\Saved\Config\WindowsNoEditor\*.ini` and wrote its crash report there. `-log` produced no log file (shipping build).
- Same fatal error: ACCESS_VIOLATION reading 0x0 at CrashBandicoot4.exe+0xc31cf1, identical stack to test 1. Started 23:17:44, crashed 23:18:40 (about 56 s, same as test 1). Command line confirmed in the crash report.

Derived / guesses:
- Not caused by the shared Saved folder (ruled out) and not by UE's Steam online subsystem alone (`-nosteam` did not help).
- Both crashes at about 56 s suggests a timeout: something per-user (Steam user session, or the game's online login; the crash report carries a LoginId) fails for the second instance and returns null. Unverified.
- Identifying the function at exe+0xc31cf1 needs the decrypted code (the on-disk .text is SteamStub-encrypted): a debugger attached to the running game (x64dbg), or a memory dump. That is phase-3 reverse engineering, best done from Claude Code on the PC.

Options:
1. Test with two PCs on the same network (host on one, `open <host LAN IP>` on the other). Avoids the second-instance problem and matches the real online use case.
2. Debug the second-instance crash (x64dbg on the PC, or per-instance Steam identity as in playbook/split-screen.md). Needed later for couch co-op on one PC.
