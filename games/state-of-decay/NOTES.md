# State of Decay (2013) — notes

Game slug: `state-of-decay`. Session: 2026-10-19.

Private install path, exe hash and Steam build id live in `local/identity.md` (gitignored).

## Phase 0 — Triage (2026-10-19)

### Identity (rule 5)

| Field | Value | Source |
|---|---|---|
| Title | State of Decay (2013). Original DX9 Steam release, not Year-One Survival Edition (separate app 329430 / 61628) | `appmanifest_241540.acf` name "State of Decay"; Steam DLC pages say Breakdown and Lifeline keep working on 241540 and do not work with YOSE Day One |
| Store | Steam appid 241540 | appmanifest |
| Version / build id | Steam buildid **305948** | appmanifest `buildid` and `TargetBuildID` |
| Main exe | `<game dir>\StateOfDecay.exe`, 23449088 bytes, SHA-256 in `local/identity.md` | Get-FileHash, 2026-10-19 |
| Exe version resource | FileDescription "ULLaunch Application", FileVersion 14.6.23.5340, ProductVersion 1.0.0.1, Copyright (C) 2010 | exe VersionInfo |
| Engine | CryEngine 3 | ModDB game page "CryENGINE 3"; Wikipedia "Engine: CryEngine 3"; install layout `Engine/` + `Game/*.pak`; exe strings `CryEngine`, `CryAction`, `CryNetwork`, `CrySystem`, Lua 5.1.1, PDB-style path `c:\ul\class3\main\code\cryengine\...` |
| DLC installed | Breakdown (depot/app 259040, `Game\sandbox\BreakdownC.pak`) and Lifeline (depot/app 259041, `Game\beltway\BeltwayC.pak` and `BeltwayU.pak`) | appmanifest InstalledDepots; dir listing |
| OS | Windows 10 | this PC |

### Answers to the triage questions

1. Which exact game? State of Decay (2013), Steam app 241540, buildid 305948, with Breakdown and Lifeline. Not State of Decay 2, not YOSE.
2. Already done? **No usable co-op project.** No `DIRECTORY.md` entry and no `games/` write-up before this session. GitHub search API `repo:cranebeaker/coop-anything State of Decay` returned `total_count: 0` (2026-10-19). `gh` is not installed, so this was the public API, not `gh issue list` / `gh pr list`.
3. Engine? CryEngine 3. Sibling co-op work in this repo is Far Cry (CryEngine 1), Crysis (CryEngine 2), Prey (2017) and Kingdom Come: Deliverance 1 and 2. None of those is this game's code.
4. Source available? No engine source and no decomp found. The paks do ship game data: `Game\gamedata.pak` lists **324 `.lua` files** (AI behaviors under `scripts/ai/...`, plus `scripts/gamerules/singleplayer.lua`) and the DLC paks list XML under `libs/class3.1` and `libs/class3.2`. Native code is inside `StateOfDecay.exe`. The only binaries on disk are that exe, `steam_api.dll`, `fmodex.dll`, `fmod_event.dll` and `fmod_event_net.dll`. There is no `CrySystem.dll` / `CryAction.dll` / `CryNetwork.dll` even though the exe contains those names as strings.
5. Engine multiplayer? The shipped game is single-player (Steam feature tag on the base game's DLC pages; Wikipedia "Mode: Single-player"). The exe contains CryEngine netcode strings (`EngineModule_CryNetwork`, `CNetChannel`, `CUIMultiPlayer`, `IsMultiplayer`, `g_multiplayerEnableVehicles`). `gamedata.pak` lists stock-looking multiplayer entity scripts (`scripts/entities/multiplayer/flag.lua`, `spawnpoint.lua`, `spectatorpoint.lua`, and others) and `scripts/gamerules/singleplayer.lua`. The only game-rules Lua found by name is `singleplayer.lua` plus `hitdeathreactions.lua`. Pak entries use a compression method Python's zipfile and .NET ZipFile cannot inflate ("unsupported compression method"), so the script bodies were not read.
6. Shipping check. A mod can ship as its own package. No EasyAntiCheat or BattlEye DLL is in the install. `steam_api.dll` is present. Breakdown's Steam text says players "compete with survivors around the world" while the feature tag is still Single-player. That online score feature was not tested. Do not touch it.

### Observations

- Install root listing, 2026-10-19: `Engine\config.pak`, `Engine\shaders.pak`, `Game\` paks, `StateOfDecay.exe`, three FMOD DLLs, `steam_api.dll`.
- `Engine\config.pak` central directory includes `config/multiplayer.cfg`, `config/singleplayer.cfg`, `config/levelrotation.xml`. Bodies not inflated.
- Exe string scan found Lua 5.1.1 and the path `c:\ul\class3\main\code\cryengine\cryscriptsystem\luadebugger\...`. Class3 is the game's codename (ModDB page: "formally codenamed as Class3"). Level folder on disk: `Game\levels\Class3\`.
- `LastPlayed` in the appmanifest is 0.

### Derived

- Path is **build from zero** (`coop-build-from-zero`). There is no abandoned co-op mod to revive.
- Hook point is an injected DLL or a pak overlay. CryEngine Lua and XML are the game-logic surface. Campaign simulation lives in the exe.
- CryEngine 3's own multiplayer module is compiled in. Whether Undead Labs left it callable for this campaign is unknown until the scripts and the exe are read.

### Guesses

- `scripts/entities/multiplayer/*.lua` and `config/multiplayer.cfg` are CryEngine SDK leftovers, not a hidden State of Decay mode. Not confirmed: those files were not decompressed.
- `fmod_event_net.dll` is the FMOD SDK's event-net library shipped beside `fmod_event.dll`, not evidence of a game netcode mode.

### Unknowns

- Text of `singleplayer.lua`, the multiplayer entity scripts, and `multiplayer.cfg`.
- Whether CryNetwork is initialized in a normal single-player launch.
- Whether any leaderboard or Xbox service is still live. Not tested.
- Saves location on this PC. Not searched, and the game has not been launched (`LastPlayed` 0).

## Phase 1 — Research (2026-10-19)

Searches that ran:

- This repo: `DIRECTORY.md` / `games/` grep for "state of decay" (no hits before this note). GitHub issues+PRs search API, `total_count: 0`.
- Web search: "State of Decay 2013 co-op mod multiplayer CryEngine", "State of Decay Nucleus Co-op split screen handler", GitHub "state of decay coop OR multiplayer".
- GitHub repository search API `state of decay coop OR multiplayer` in name/description: `total_count` 123. The first page is State of Decay 2 templates, Proton issues and unrelated "together" mods. No original-game co-op repo was identified in that page. Forks and non-default branches of every hit were **not** walked.
- ModDB game mods page, fetched 2026-10-19: 3 mods. KryoTronic's Skin Mod, "State Of Decay Lifeline" (2016, describes itself as containing the full game; not downloaded), SOD Raf Arcade Mod (gameplay numbers). None is multiplayer. https://www.moddb.com/games/state-of-decay/mods
- Nexus: search `site:nexusmods.com/stateofdecay` for multiplayer/coop returned single-player mods (followers, ally shooting, Lonewolf mode). The full Nexus catalog was not paged.
- Nucleus / splitscreen.me results that came back are **State of Decay 2** handlers. Nucleus's own FAQ says it does not add multiplayer to a single-player game. No State of Decay (2013) handler was found.
- Not checked: archive.org, the game's Discord, Reddit beyond search snippets, playground.ru file contents.

### Findings

1. **Official co-op was cancelled before the PC release.** GamesRadar, 3 July 2013, quoting Undead Labs executive producer Jeff Strain: co-op was dropped because it meant redesigning core systems and new content, not only adding technology. https://www.gamesradar.com/state-decay-scraps-plans-co-op/
2. **Year-One Survival Edition also has no co-op.** Eurogamer, quoting a Microsoft producer (Grinke): Undead Labs would have had to rewrite a large percentage of the code. https://www.eurogamer.net/heres-whats-new-in-state-of-decay-year-one-survival-edition
3. **A Steam thread is an idea, not a mod.** "(closed) Co-OP/Multiplayer Mod Idea" by Mr. Indubitably on app 241540. The post describes a LAN design and asks for help. No release is linked. https://steamcommunity.com/app/241540/discussions/0/810939351105827815
4. **A 2013 "split screen" upload on playground.ru** (ixca, 28 Nov 2013) tells the player to drop two launchers and edit `config.pak`, then run two windows (keyboard and gamepad). The file was not downloaded. Shared-world co-op is not established by that description. https://www.playground.ru/state_of_decay/file/state_of_decay_split_screen_mod_igra_na_razdelennom_ekrane-921436
5. **State of Decay 2 is a different game** (Steam 495420) and already has official co-op. It is not this install. Nexus "multiplayer" mods in search results are loot and client fixes for that sequel.
6. **Community modding of this game is XML/Lua data**, extracted from paks (Steam group "State of Decay Modding Crew", 2013 CryEngine SDK skin guide; Nexus follower mods that drop files into `Game\`). https://steamcommunity.com/groups/stateofdecaymodding/discussions/0/864980277909273594/

### Decision

Build co-op from zero. Nothing found is playable shared-world co-op for build 305948.

Owner agreed on 2026-10-19 to the test copy, installing PyYAML, and opening the claim issue.

- Test copy: `local/test-install` (gitignored). 25 files, 2390985941 bytes, same as the Steam install. `StateOfDecay.exe` SHA-256 matches. Steam install not modified.
- `python -m pip install pyyaml` installed PyYAML 6.0.3. `python tools/build_directory.py` wrote `DIRECTORY.md` (256 entries). The gap row is in the Open gaps table.
- Claim issue: https://github.com/cranebeaker/coop-anything/issues/44 (account that Git is already logged in as). The `in-progress` label was rejected with 403 "Must have admin rights to Repository."

Next: decompress `gamedata.pak` from the test copy and read `singleplayer.lua`. Do not edit `<game dir>`.

Checked 2026-10-19 on the test copy: all 1388 entries in `Game\gamedata.pak` use zip compression method 13. Python's zipfile and .NET ZipFile both refuse that method.

That method is Xbox `XMemDecompress` (LZX), the mapping in QuickBMS `zip2.bms`. A reader in `local/lzx/` (gitignored; MonoGame's LZX decoder, LGPL 2.1 or MS-PL) decodes it with LZX window bits **17**. Short blocks start with `0xFF`, then big-endian destination size and source size, then the LZX bytes, then a 5-byte tail. Verified 2026-10-19: `config/multiplayer.cfg` came out as readable text (2977/2977 printable bytes). The same settings extracted the game-rules and player scripts below. Extracted text is in `local/extract/` and stays out of git.

## Phase 3 — Scripts (2026-10-19)

Read from the test copy only. The Steam install was not modified and the game was not launched.

### Observations

- `config/singleplayer.cfg` sets `g_multiplayerDefault = "0"`, `sv_gamerules="SinglePlayer"`, `g_telemetryConfig = "SP"`, and turns on the third-person camera (`g_tpview_enable = 1`).
- `config/multiplayer.cfg` is a CryEngine multiplayer preset. It sets `g_multiplayerDefault = "1"`, `g_telemetryConfig = "MP"`, `sv_bandwidth`, `net_packetsendrate`, spectator cvars, and a comment that points at `GameCrysis2\Difficulty\*.cfg`. It is not referenced by `singleplayer.cfg`.
- `config/levelrotation.xml` lists empty `InstantAction` and `TeamInstantAction` slots. `scripts/gamerules/levelnames.xml` names Crysis levels (Recovery, Relic, Assault, Paradise Lost, Exodus), not Trumbull Valley.
- `scripts/gamerules/` contains `singleplayer.lua` and `hitdeathreactions.lua` only. There is no multiplayer rules script.
- `singleplayer.lua` header is Crytek, 2001-2004, "GameRules implementation for Death Match". Undead Labs added zombie and juggernaut hit materials (`UL Begin` around spawn). `SinglePlayer:IsMultiplayer()` returns false. The same file still has `Client` and `Server` tables. `Server:OnClientConnect(channelId)` calls `Actor.CreateActor` with class `"Player"` and name `"Dude"`, then `GetFirstSpawnLocation` (the UL edit). `Server:OnClientEnteredGame` is empty.
- Several rules functions return immediately when `srcId ~= g_localActorId` (`OnNewUsable`, `OnUsableMessage`).
- `scripts/entities/actor/player.lua` is the `Player` class, animation graphs `UL_FullBody.xml` and `UL_UpperBody.xml`. It has `Server:OnInit`, `Server:OnInitClient`, `Client:Revive`, `Client:MoveTo`, `OnSave` / `OnLoad`. `scripts/main.lua` only reloads `common.lua`, `physics.lua`, and `Debug.lua`.
- `scripts/entities/ul/` is the game's own entities (searchable containers, homesite, infestationsite, facility socket, stockpile, zombie spawn editors). `UL_Searchable` has `NotSearched` and `Searched` states and separate `Server:OnInit` / `Client:OnInit`.
- `scripts/entities/multiplayer/*.lua` (flag, spawn point, spectator point, and the rest) are Crytek 2004 entity scripts. `SpawnPoint.Server:OnInit` assigns a team.
- The exe contains the strings `sv_port`, `sv_maxplayers`, `%USER%/SaveGames`, `%USER%/Profiles/`, and `%USER%/game.cfg`. It does not contain a literal Documents path. Where `%USER%` points on this PC was not checked, because the game was not launched.

### Derived

- The campaign already runs on CryEngine's listen-server rules. A second connection, if the native side allows one, is spawned by `OnClientConnect` as another `Player`. Co-op work starts by making that second actor a survivor and by fixing the `g_localActorId` early-outs, not by inventing a netcode.
- `multiplayer.cfg`, `levelrotation.xml`, `levelnames.xml`, and `scripts/entities/multiplayer/` are SDK leftovers. They are not a hidden State of Decay match.
- Community, missions, and the character-switch simulation are not in `singleplayer.lua`. They live in the exe and in the XML under the paks (`libs/class3.*`, `mission_mission0.xml`). Syncing the story means reading that native state, not only the rules script.

### Unknowns

- Whether a second client is accepted. `sv_maxplayers` is a string in the exe. No launch yet.
- Whether a loose file next to the pak overrides it. A 2013 Japanese mod note says it does (http://jpmod.blogspot.com/2013/09/state-of-decay.html). Not tested here.
- The real folder behind `%USER%`. Do not launch until saves are forced into the test copy.

## Phase 3b — Loose files and a test launch (2026-10-19)

The Steam install was not modified. Launches used `local/test-install/StateOfDecay.exe` only, windowed, then the process was closed.

### Observations

- `sys_PakPriority` help text in the exe: "If set to 1, tells CryPak to try to open the file in pak first, then go to file system."
- `system.cfg` and `systemoverride.cfg` in the test-copy root are loaded. The log says `Loading Config file system.cfg` and `Loading Config file systemoverride.cfg`. The engine then runs `exec user.cfg`. A file named `UserConfig.cfg` is not that file. After `user.cfg` was added, the log says `user.cfg found in ./`.
- `sys_user_folder = USER` does not mean a folder next to the exe. The first launch created `%USERPROFILE%\Saved Games\USER` and wrote an empty default profile (`Profiles/default/profile.xml`, `LastPlayed` set, no campaign save). That directory was then replaced with a junction to `local/test-install/USER`, so later profile writes land in the test copy.
- With `sys_PakPriority = 0`, loose files are read. A log line and a marker file were added to `Game/scripts/main.lua` and `Game/scripts/gamerules/singleplayer.lua` in the test copy. `class3.log` contains `<Lua> COOP_PROBE loose singleplayer.lua loaded` and `<Lua> COOP_PROBE loose main.lua OnInit`. Marker files `COOP_PROBE_MAIN.txt` and `COOP_PROBE_RULES.txt` were created in the test-copy root. This was during startup, before any level.
- The window came up as `StateOfDecay`, Direct3D9, 800x600 windowed, then the log records a change to 1024x768 windowed. Three `[Error] Runaway thread` lines follow shader flush. The process still closed with exit code 0 when the window was closed. Cause of those errors is unknown.
- `+map class3` was on a later command line. In a 60 second run the log never said a level loaded, and `OnClientConnect` did not run (no `COOP_PROBE_CONNECT.txt`). `singleplayer.lua` still loaded at startup, which is the top of the file, not the connect function.

### Derived

- A co-op patch can ship as loose Lua under `Game/scripts/` when `sys_PakPriority` is 0. The packs do not have to be rebuilt for script changes.
- Saves for `sys_user_folder = USER` go to `%USERPROFILE%\Saved Games\USER`. The junction keeps those bytes in the test copy. Do not point that name at a real profile.

### Unknowns

- Whether `+map` on the command line is applied too early. A later run answered the level name and the local connect. See the next section.

## Phase 3c — Level load and a second process (2026-10-19)

Steam install not modified. Both processes were test copies. The host was `local/test-install`. The joining process was a second folder with its own exe, DLLs, and cfg files, and with `Engine` and `Game` junctioned back to the host copy. Its profile name was `USER2`, junctioned the same way as `USER`.

### Observations

- `Game/levels/Class3/level.pak` stores `levelinfo.xml` uncompressed. The name inside it is `game\levels\class3`, mission `Mission0`. Other folders in `levels.pak` are test maps (`playground`, `forest`, `class3worldmap`, and others).
- `map class3` at the end of `user.cfg` is executed (`Executing console command 'map class3'`). The log then says `Loading level class3`, loads `singleplayer.cfg`, and finishes in about 4 seconds. `+map class3` on the command line still does not do this. `user.cfg` runs later, after Lua init.
- The loose `OnClientConnect` marker ran for that load: log line `COOP_PROBE OnClientConnect channel=1`, and `COOP_PROBE_CONNECT.txt` contained `channel=1`. The same run logs `BlockingConnect` reaching `have player` in about 6 seconds, then a local profile join. `SinglePlayer.Server:OnStartLevel` only sends `eGE_GameStarted`.
- `map class3` with `sv_port 64087`, `sv_maxplayers 4`, `sv_lanonly 1`, and `sv_bind 127.0.0.1` did not open a socket. `netstat` showed no UDP row for that process.
- `map class3 dedicated` did open `127.0.0.1:64087`. It loads `multiplayer.cfg` instead of `singleplayer.cfg`. That process never logged `OnClientConnect`, and its log stopped at `Can't open file (levels/Class3/LevelInfo.xml)` plus the same three `Runaway thread` errors. No local player.
- `map class3 server` does both. It loads `multiplayer.cfg`, logs `PrepareLevel class3`, calls `OnClientConnect` for channel 1, and binds UDP `127.0.0.1:64087`. The client log of the second process still says `Game rules class: SinglePlayer`.
- The second process ran `connect 127.0.0.1 64087` from its own `user.cfg`. Against the `server` host it logged `PrepareLevel class3` and `RTS.OnLevelLoadEnd`. The host log shows that peer receiving fragmented packets (on the order of 160 fragments and several thousand unfragmented packets), then resending one packet, then `Disconnect` with cause 12 and message `Unreachable address`. The host marker file stayed `channel=1` only. The second process never logged `have player`.
- After `OnLevelLoadEnd` the joining process logs `No network scheduling parameters set` for `ULBarricade` and `ULDoor`.
- A second process is allowed to start. It is not blocked by a single-instance check in the window of these runs. `user.cfg` is loaded from the exe directory (`found in ./`), not from a different working directory.

### Derived

- The campaign map command is `map class3`, issued from `user.cfg`.
- The local player is already the CryEngine connect path: channel 1, `OnClientConnect`, class `Player`.
- A listen port and a local player happen together only with `map class3 server` in this build. Plain `map class3` has the player and no socket. `dedicated` has the socket and no player.
- A second game process can be told the level and can load it. It was disconnected before `OnClientConnect` ran for it. Cause 12 was not investigated past the log line.

### Unknowns

- Why the listen server marks the second process unreachable after the level has already streamed. A UDP exchange on the Ethernet address completed 300 packets; closing one socket produced Windows error 10054. During a game join the second process kept its UDP socket open, and the host resent to that same port, then logged `32769` from it and disconnected with cause 12. `sys_SleepIfInactive 0` stretched the resend from about 7 seconds to about 18 seconds and still ended in `32769`. Silencing the client log did not change it. `Scripts/Network/Scheduler.xml` and `EntityScheduler.xml` are in `gamedata.pak` and include a `Default` policy; the scheduling lines are a per-class warning, not a missing file. `map class3 dedicated` binds the port but does not finish the level: a loose `LevelInfo.xml` gets past the first missing file, then `LevelData.xml` and `LevelDataAction.xml` fail to open and `OnLevelLoadEnd` never comes. Private route notes are in `local/vpn-route.md`.
- Whether `OnClientConnect` will run for that peer if the disconnect is avoided.
- Whether `ULBarricade` and the other UL classes can replicate once a second channel exists. They currently have no network scheduling parameters.

## Phase 3d — Second channel reaches the host (2026-10-19)

Same two test copies as Phase 3c. Host: `map class3 server`, `sv_bind` on the LAN address. No file of the exe was changed; both fixes below are a loose Lua edit and a runtime memory patch written by `WriteProcessMemory` into the running test copy. The exe is SteamStub-wrapped, so `.text` is only readable from a live process.

### Observations

- A logging UDP relay between client and host showed the client stop sending about 10 seconds into the join while the host kept resending. Windows error 10054 and the host's `32769` appeared only after the client process was closed. Cause 12 `Unreachable address` was a consequence of the client dying, not a route problem.
- The client's own minidump (written to `USER/` only when no debugger is attached) shows an access violation at `StateOfDecay+0x1213f9c`, `mov ecx,[eax+0Ch]` with `eax = 0`: the result of a virtual call at vtable `+0x114` is NULL on the joining client.
- Runtime patch on the client at that address: if `eax` is NULL, use `ecx = 0` and continue. With it the client stays alive, and the host logs `OnClientConnect channel=2`.
- The host then quit with exit code 1 and no exception. A hook on `msvcr100!_exit` caught it: `_exit(1)` called from `StateOfDecay+0x18df40`, the engine's fatal-assert routine. The assert, read from `.rdata`, is `countPlusUseless < capacity`, `Buffer overflow in entity db: %d:%d`, file `UL\EntityDb\EntityDb.cpp`, function `EntityDbAdd`. The caller adds the entity to a separate database when `strncmp(name, "Dude", 4) == 0`. The Lua stack strings show `Actor.CreateActor(channelId, actorParams)` with class `Player`.
- In the loose `singleplayer.lua`, naming channel > 1 actors `CoopPeer<channel>` instead of `Dude` avoids the assert. The host logs `AllocateObject ... (CoopPeer2) ... [owned dynamic]` and `spawned channel=2 ... name=CoopPeer2 pos=1345.1,3442.8,116.9` (2 m from channel 1). Both processes were still running 60 seconds later.
- The client logs `CoopPeer2 renamed to Dude(1)`, `[CUIMultiPlayer] PlayerRenamed`, `InvokeRMI: RMI via server (to client) requested but we are not a server`, and `Unable to bind object to network` for `Dude` and `CoopPeer2`.
- Screens at that moment: the host window was black; the client window showed the title menu (`Play State of Decay`).

### Derived

- The single-player code keeps the player in a "Dude" entity database with room for one entry. Any second actor must not be named `Dude`.
- The joining client reaches the stage where it receives the second actor, but it does not take it over as its own player yet.

### Unknowns

- What the vtable `+0x114` call that returns NULL on the client is (likely the client actor or a game-rules lookup). The NULL guard may hide later problems.
- Why the host renders black. It is black with no client too (checked 15 seconds after `spawned channel=1`), so it comes from loading the level with the console `map` command instead of the menu, not from the join. Whether the host world is playable from this state.

## Phase 3e — Why `map` is black; the real entry is the front-end (2026-10-19)

Host test copy only. No file of the exe changed. All loads via `user.cfg`.

### Observations

- The renderer is healthy: the log shows `Renderer initialization`, D3D9, shader cache writable. The window draws the main menu at 68 FPS (farm background, three editions, `SPC Play State of Decay`). So D3D and the front end both render.
- `map class3` (plain, singleplayer) is black. `map class3 server` is black. Both are black with no client connected. So the black is the `map` path itself, not the join and not the `server` flag.
- Forcing daytime did not change it: `sv_timeofdaystart 12` and `e_TimeOfDay 12` with `map class3` were still black. So it is not a night/no-sun time-of-day issue.
- The exe has a front-end level/flow named `Class3FrontEnd`, and CryAction commands `StartGame` / `LoadGame`, plus `loadLastSave` and `LoadLastCheckpoint`. The menu `Play` goes through this front end, not through a bare `map`.
- Sending the menu's `Play` key (VK_SPACE) with `PostMessage(WM_KEYDOWN/UP)` did nothing; the menu stayed put. The game reads raw input (DirectInput/XInput), so synthetic window messages are ignored. Driving the menu needs `SendInput` with the window focused, or a console command.
- Relevant cvars seen in the exe: `g_startFirstTime`, `g_quickGame` / `g_quickGameStop` (no companion level/rules strings, looks like an MP quick-match, not a campaign start), `g_enableloadingscreen`, `sv_timeofdaystart`, `g_EnableLoadSave`, `g_playerRespawns`.

### Derived

- There are two half-working paths. The menu `Play` path runs the full game flow (community/survivor/HUD/view) and renders, but starts a local single-player context with no listen socket. The `map class3 server` path opens the listen socket and runs `OnClientConnect`, but does not run the game flow, so it renders black. Co-op needs one path that does both: a networked server context that also runs the front-end game flow.
- This is the core remaining task and it is in native code (the CGame front end / `StartGameContext`), not a loose-Lua or small-patch fix.

### Unknowns

- How the front end calls into `StartGame` / `StartGameContext`, and whether it can be told to start as a server (so the menu `Play` opens a socket), or whether the `map ... server` path can be told to run the front-end game flow.
- What `g_startFirstTime` does on a fresh profile, and whether a console command exists to start a campaign directly (so the host can be rendered and playable without driving the menu by hand).
- The `Play State of Decay` button in the front-end flash (`libs/ui/flashassets/scripts/class3frontend/class3_frontend.as`) fires `fscommand("DLCSelected", "SOD")` (and `BREAKDOWN` / `LIFELINE` for the other two). A native handler for `DLCSelected` runs the campaign start. There is no console command for it, so a campaign is started either by driving the menu with real input or by calling that native handler.
- Whether the client's title menu is drawn on top of a loaded world, and what "we are not a server" RMI path needs.

## Phase 3f — Menu Play automation; full rendered campaign (2026-10-20)

Test copy only. No exe file changed. Automation scripts in `local/`.

### Observations

- SendInput / PostMessage still cannot drive the menu (DirectInput). Mouse movement on screen earlier was the owner, not the agent.
- Flash `SelectSoD` only highlights the DLC tile. Confirm/Play is MouseRemap `"A"` → action code **6** stored at `0x01970f80` (idle value `0x19` = 25). Writing that global alone is not consumed on the DLC-select screen; real key path calls `0x01615CB0(action)` which dispatches to the current menu state's input handler at entry `+0x8`.
- DLC-select input handler: `0x01619B10`. Action 6 / 10 jumps to Play body `0x01619C5A` (start sound + `MenuStackPush` state `0x11`).
- `CreateRemoteThread` calling `0x01619B10(6)` advances DLC-select → main menu (`Start a New Game` highlighted). Depth 1→2.
- Calling the next menu's input handler (`0x016153B0` at depth 2) via CreateRemoteThread crashes: `CryEngine Error: sounds/music:menu:main_menu_02 Called RemoveSoundObject from other than Main thread`.
- CryEngine main thread id is at `[*(0x01DBFAB4) + 0x43EC]` (IsMainThread: `GetCurrentThreadId` vs that dword). Hijacking **that** thread to call the menu input handler(6) starts a new campaign without the sound assert.
- Result: loading screen, then full rendered gameplay (lakeside with Ed, HUD, minimap, objective `Survive!`, ~120 FPS). Log shows `Loading level Class3`, `singleplayer.cfg`, full terrain/brush load, then `COOP_PROBE OnClientConnect channel=1` and Dude spawn. Menu depth returns to 0.
- No UDP listen socket while in this session (`netstat` empty for the pid). `sv_*` cvars in `user.cfg` did not open a port on the menu path.
- Reusable script: `local/boot_to_gameplay.py <pid>`.

### Derived

- The two half-paths can be sequenced: **menu Play gives a rendered game that still uses `OnClientConnect` for the local player**; it simply never binds a listen port. Co-op needs that same start plus a listen/server context (or opening a listen socket after load) — not `map class3 server` alone (black, no front-end flow).
- Menu automation must run sound/Flash-touching calls on the CryEngine main thread.

### Unknowns

- How to open a listen UDP port (or flip the game context to server) on an already-running menu-started session without remapping into the black `map ... server` path.
- Whether `singleplayer.cfg` vs `multiplayer.cfg` blocks hosting even if a socket is opened.



## Phase 3g — Rendered listen host via `map %s server` (2026-10-20)

Test copies only. Runtime patch; exe file not modified on disk.

### Observations

- Mid-session `CNetwork::CreateNub` from a main-thread hijack either returned NULL or crashed the process. Host-migration `OnPromoteToServer` exists but needs a migration session; not usable as a simple "open port now" call on a LocalOnly SP context.
- Menu New Game does **not** use the CryAction `map %s` site at `0x006E4357`. Stack dump of `IConsole::ExecuteString` during New Game showed return address `0x0144D7E2`.
- The real formatter is at `0x0144D7AF`: `snprintf(buf, 0x100, "map %s", level)` then `ExecuteString(buf)`. Level name comes from `0x01E80CE0` (Class3).
- Runtime patch: replace the `push "map %s"` immediate with a remote string `map %s server`. Script: `local/force_map_server.py <pid>` (before `boot_to_gameplay.py`).
- Result after `boot_to_gameplay.py`: log shows `Executing console command 'map Class3 server'`, loads **`multiplayer.cfg`**, `OnClientConnect channel=1`, UDP listen on `192.168.88.37:64087` (from `sv_bind` / `sv_port` in host `user.cfg`).
- Host screenshot: full rendered campaign (Marcus/Ed, HUD, minimap, objective, ~117 FPS) — **not** the black `map` path. So front-end New Game + `server` flag gives both render and listen.
- Client (`local/client-install`, `connect 192.168.88.37 64087`, `patch_client.py`): host logs `OnClientConnect channel=2`, `AllocateObject ... (CoopPeer2)`, `spawned channel=2 ... name=CoopPeer2`, `PlayerJoined`. Both processes stayed up.
- Client window remained on the DLC title menu (`[SPC] Play State of Decay`) after join — same as earlier black-host joins. Peer exists on the host; client has not taken over CoopPeer2 / left the front-end yet.

### Derived

- Co-op host recipe for this build: patch New Game's `map %s` → `map %s server`, then drive the menu with `boot_to_gameplay.py`. That is the combined path that was missing.
- `force_map_server.py` + `boot_to_gameplay.py` + existing `sv_*` in host `user.cfg` + client `patch_client.py` + CoopPeer naming in loose `singleplayer.lua` is enough to get channel 2 spawned on a rendered host.

### Unknowns

- How the joining client leaves the title menu and possesses CoopPeer2 (front-end still drawn over the net session).
- Whether UL entity replication is healthy beyond the initial spawn (scheduling warnings may still apply).
- Whether the `map %s` patch site is stable across SteamStub page decrypt / ASLR (address is in the live image; re-find if the build moves).


### Client follow-up (same session)

- After channel 2 spawned, the client window stayed on the DLC title menu. Client `class3.log` stopped early (~247 lines) and never logged `PrepareLevel` even though the host accepted the peer.
- Re-running `connect` on the client main thread after the menu was up caused a CryEngine Error and `Disconnect ... User left the game`. Do not re-issue connect on a live joined client.
- Next: find how to dismiss `Class3FrontEnd` on the joining client (or possess CoopPeer2) without starting a second local New Game.

### Boot reliability

- With an existing profile, the post-DLC main menu highlights **Continue**, not Start a New Game. `boot_to_gameplay.py` confirms the highlighted item (action 6), so it can land on Continue and never hit the patched `map %s server` path. Workaround: call the menu handler again once `Start a New Game` is highlighted, or clear the test profile. Confirmed: a second confirm on the New Game entry runs `map Class3 server` and opens UDP.
- Client must be started **after** the host UDP listen is up. `connect` in client `user.cfg` during early boot often fails silently (no client UDP socket).
- Writing `menu depth` to 0 on the client crashes the process. Need a real FrontEnd unload / possess path for CoopPeer2.

## Phase 3h — This PC handoff; second join crash; channel 2 again (2026-10-20)

Test copies under `D:\projects\coop-anything\coop-anything\local\`. Steam install `D:\SteamLibrary\steamapps\common\State of Decay` not modified. Exe SHA matches prior build.

### Observations

- AmneziaVPN was stealing `192.168.88.0/24` until excluded. After exclude, `Find-NetRoute` for `192.168.88.25` selects Ethernet 4. Same-PC tests then used `sv_bind 127.0.0.1` / `connect 127.0.0.1 64087` to avoid LAN/VPN entirely.
- `%USERPROFILE%\Saved Games\USER` and `USER2` junctioned to host/client profile folders.
- Probe Lua paths updated off `D:/react-projects/...` to this project tree.
- First join with only the old NULL guard (`0x01613F9C`) still dumped after `OnLevelLoadEnd`. Dump `…21-33-14.dmp`: AV at `0x013E7410`, `mov edx,[edi]` with `edi==0` (vtable+0x184 call). Second runtime patch in `local/patch_client.py` skips that call and sets `eax=0`.
- With both patches + host `host_boot_coop` + client `connect` in `user.cfg` after UDP listen: host logs `OnClientConnect channel=2`, `AllocateObject … (CoopPeer2)`, spawn at Dude+2 m. Both processes stay up. Client log shows `PrepareLevel` / `OnLevelLoadEnd`, `Unable to bind … Dude/CoopPeer2`, then `PlayerRenamed … Dude(1)`.
- Joined client has a live `CActionGame*` (`0x019F1004` non-NULL) and menu depth 1 / FE flag `0x01EE8807=1`. Calling `0x016171F0` alone did not clear the flag. Wrapper `0x01449810` (unload + follow-up) raised menu depth 1→2 and left the flag set. Do not treat that wrapper as the dismiss path.
- `deferred_connect.py` first cut used `add esp,0x14` after thiscall `ExecuteString` and crashed main thread at `eip=0` (`…21-40-43.dmp`). Stack cleanup fixed to `add esp,8` only; prefer `user.cfg` connect after listen for the known-good path.
- Use `C:\Users\user\AppData\Local\Python\bin\python.exe` on this PC (WindowsApps `python` stub is broken).

### Derived

- This build needs **two** client NULL guards for a stable join, not one.
- Channel 2 + CoopPeer2 on a rendered listen host reproduces on this PC over loopback.
- Client already has an ActionGame after join; the remaining wall is dismissing Class3FrontEnd / possessing CoopPeer2, not creating the game object.

### Unknowns

- Correct FrontEnd unload / possess entry that clears FE and gives control of CoopPeer2 without pushing another menu state.
- Whether PrintWindow black captures are D3D-only (likely) or the host really went black after long join.

## Phase 3i — FrontEnd dismiss mapping (2026-10-20, this PC)

### Observations

- Host after successful New Game: `depth=0`, `fe_flag(0x01EE8807)=0`, live `CActionGame*`. Client after join before dismiss: `depth=1`, `fe=1`, live ActionGame, still draws the DLC title logo (BitBlt with window foregrounded; PrintWindow is useless on D3D9).
- Real dismiss routine is **`0x01617130`** (not `0x016171F0`). It finds `Class3FrontEnd`, calls vtable+0x64(0), then `mov byte ptr [0x01EE8807],0`. Confirmed: fe 1→0, process stays up. Script: `local/fe_dismiss.py`.
- `0x016171F0` is the *next* function (destroy via vtable+0x3C(1)). Calling it after dismiss **re-sets fe to 1**. Do not use it for leave-FE.
- Wrapper `0x01449810` (destroy + `0x01616CF0` state machine) pushes menu depth (1→2). Avoid.
- `MenuStackPop` at `0x01614300` after dismiss: when depth is the DLC layer (depth=1), pop **re-enables fe_flag**. When depth was artificially 2 from the bad wrapper, two pops reached depth 0 with fe staying 0 once — not a reliable recipe.
- Poking `0x01EE8890=0` after dismiss still AVs (`…21-48-26.dmp` at `0x0120F96B`). Do not poke depth.
- Thirteen inlined `fe_flag=0` sites exist under `0x01617xxx`; no direct `E8` callers of `0x01617130` (likely reached via other control flow). `local/client_post_join.py` now does dismiss-only.
- Client ActionGame often has `+0x20`/`+0x28` (server/game nub) null after join; host has both set. Client log still `Unable to bind object to network (Dude|CoopPeer2)` and `CPlayerInput::SetState called: should never happen`. Possession/view not solved.
- Foreground BitBlt: host shows outdoor world (~47 FPS overlay); client still shows State of Decay title logo (~37 FPS) even after fe_flag clear — flag clear alone does not remove the Flash title draw.

### Derived

- Leaving the title is at least two steps: (1) FE object/flag dismiss at `0x01617130`, (2) something else that tears down the Flash/title presentation and binds the local view to CoopPeer2 / Dude(1). Step 2 is not MenuStackPop and not depth poke.
- Stable join recipe on this PC: `sv_bind`/`connect` **`192.168.88.25`** (not `127.0.0.1` — Docker Desktop hosts maps `host.docker.internal`→`192.168.88.16`, and a loopback connect logged `host.docker.internal:64087` then failed) + dual client NULL patches + `connect` after UDP; then dismiss-only (`0x01617130`) for FE flag experiments. Confirmed again: channel 2 + fe 1→0, depth left at 1, both processes up (host 5016 / client 1184 this run).

### Unknowns

- What New Game does after `0x01617130`-equivalent to also drive `depth→0` and replace the title render with the world view on a *client* session.
- Where the client nub / view entity should be installed so `Unable to bind` stops and `CPlayerInput::SetState` is valid.

## Phase 3j — Menu stack vs FE flag; why title sticks (2026-10-20)

### Observations

- Joined client sits at **menu depth 1** with DLC entry handler `0x01619B10`. Entry+4 (leave) is **`0x01617130`** (same as FE dismiss). So popping the DLC menu *calls dismiss as leave*.
- `MenuStackPop` (`0x01614300`) takes a **bool arg at [esp+4]**. Nonzero arg re-enters a menu after leave. Callers use `push 0`. Our earlier stub only did `sub esp,8` without `push 0`, so garbage arg re-opened UI.
- When pop drives depth to 0, code at `0x01614360` (`test eax; jnz`) falls into **`push 0; push 0x0B; call …`** which **reopens FrontEnd**. That is why a normal pop turns `fe` back on. Patch site: `0x01614362` (`75→EB` skips reopen). Script: `local/menu_pop_noreopen.py`.
- Even with reopen skipped + leave call nopped, a pop from the live joined client still ended at `depth=1 fe=1` (needs more work — possible FE tick re-sync, or pop not actually decrementing under that patch set).
- **Poking `depth=0` crashes** the client even if all threads are suspended for the write and then resumed (`ag` goes NULL). Same class of failure as earlier `0x0120F96B` dump. Do not poke depth.
- ActionGame layout clarification: **`+0x1c` and `+0x20` share NetNub vtable `0x1836EA0`**. Host has both; client has **`+0x1c` only** (likely `m_pClientNub`). Host-only `+0x20` server nub and `+0x28` game nub. Client is not missing a client nub at `+0x1c`.
- `Unable to bind object to network` site: push at `0x7908C9` (string `0x17D5E3C`).
- `fe_flag=0` alone does not remove the title logo while depth stays 1 — Flash/DLC menu still draws.

### Derived

- Title→world on the client is a **menu-stack** problem first, not “missing client nub at +0x20”.
- Safe leave needs either: (1) a depth→0 path that does not crash and does not reopen FE, or (2) hide/destroy the Flash movie without changing depth.
- Next suspects: crash site `0x0120F96B` when depth is zeroed; FE tick `0x01615C30`; GFx/Flash hide; host’s level-load path that reaches depth 0 cleanly.

### Unknowns

- Exact reason pop-with-noreopen still left depth 1 / fe 1 on the clean join test.
- How to hide Class3FrontEnd Flash while depth remains 1 (or how host zeroes depth without AV).

## Phase 3k — Depth clears; Flash carousel remains (2026-10-20, this PC)

### Observations

- **Working leave-stack recipe** on a joined client (channel 2 / CoopPeer2, ag live):
  1. `fe_dismiss.py` → `0x01617130` (fe 1→0, depth stays 1)
  2. `menu_pop_noreopen.py` → `MenuStackPop(0)` with `0x01614362` jnz→jmp so FE reopen is skipped
  3. Result: **depth=0, fe=0, ag live, process up**. No depth poke. No crash guard required for this path.
- Earlier pop failures were from wrong pop args / calling pop before dismiss / testing on a dying process. With dismiss-first + `push 0` + noreopen patch, depth reaches 0 cleanly.
- **BitBlt after that recipe still shows the DLC carousel** (SoD / Breakdown tiles, ~32 FPS). Clearing the menu stack does **not** unload the orphan Flash movie.
- Host in world vs client after leave-stack (same globals region):
  - Host: `0x01EE8894=1`, `0x01EE8898=0`, `0x01EE88A0` nonzero float
  - Client: `0x01EE8894=0`, `0x01EE8898=0xFC0` (not a usable pointer — RPM fails), `0x01EE88A0=0`
- Poking `8894=1` / clearing `8898` does not remove the carousel.
- `0x01614720` sets `8894=1` but is the **pause main-menu** entry (`sounds/ui:ui_pause:main_menu`) — calling it pushed depth 2 and re-set fe=1. Do not use for leave-title.
- Class3FrontEnd `vtable+0x64(1)` via patched dismiss killed the client window; `+64(0)` alone (dismiss) does not hide the carousel.
- `OnShutdown` invoke (`vtable+0x3C("OnShutdown")` then `+0x44`) and destroy `0x016171F0` (`vtable+0x3C(1)`) left the carousel drawing.
- FlashUI notify helper `0x01475A90` (used by dismiss for `Class3HUDPause` / `Class3FrontEnd`) with (0,0) did not hide the carousel.
- Null-guard at crash site `0x0120F960` (`local/guard_depth_crash.py`) is available but **dismiss after installing it once killed the client**; prefer the dismiss+pop recipe over depth poking.
- Scripts added: `fe_hide.py`, `fe_vt64.py`, `fe_shutdown.py`, `fe_enter_game.py`, `flash_hide.py`, `guard_depth_crash.py`, `poke_depth.py`, `bitblt_pid.py`.

### Derived

- Menu depth / fe_flag / reopen skip are **solved** for “leave the DLC menu stack without AV”.
- Remaining wall is **orphan Scaleform/Flash presentation** (DLC select movie still Advances after depth 0), then possession/view bind (`Unable to bind` / `CPlayerInput::SetState`).
- Next: find the live GFx movie instance for the DLC carousel and unload/stop Advance; or drive the same UI transition host uses New Game→world (not pause-menu `0x01614720`). Compare host/client FlashUI element lists after leave-stack.

### FlashUI element probe (same session)

- After leave-stack, **both** host and client still resolve FlashUI name `Class3FrontEnd` (same object as `Class3Frontend`) with shared vtable `0x017C6DF8`.
- Structural diff that matched “host in world / client on carousel”: element **`+0x44`** is **NULL on host**, was **non-NULL on client** (`0x2eb77000`, looks like a GFx display/movie object: vt `0x1846020`).
- Nulling client `Class3FrontEnd+0x44` (all threads suspended) did **not** stop the carousel BitBlt; pointer stayed 0 afterward. So either another root still Advances that movie, or draw is keyed off other fields (`+0x28`/`+0x2c`/… differ by heap identity on both sides).
- Helper: `local/client_leave_title.py` runs dismiss + pop-noreopen.

### Unknowns

- Which remaining GFx/FlashUI root still draws the DLC tiles after `+0x44` is cleared.
- Whether `0x01EE8898=0xFC0` is a sentinel/state code or a corrupted handle from a client-only path.
- Correct possess/view bind once the world is visible.

## Phase 3l — World view after destroying FE movie (2026-10-21, this PC)

### Observations

- Steam SoD must not run during tests: it shares `%USERPROFILE%\Saved Games\USER` via the junction into `local/test-install\USER` and caused early host AVs (`0x012FAA8A`). Stop the Steam process only; do not touch Steam files.
- BitBlt of overlapping maximized windows lies (both “captures” show the topmost client). Always tile host/client apart before judging FE vs world.
- Class3FrontEnd `vtable+0x64` is SetVisible: compares arg to element `+0x4C`, may Invoke `cry_onShow` / `cry_onHide`. After leave-stack, client already has `+0x4C=0` but **`+0x44` still holds a GFx movie** — SetVisible(0) no-ops and the carousel keeps drawing.
- Calling **element `vtable+0x3C(1)`** (same as destroy helper `0x016171F0`'s core call) clears `+0x44` to NULL. Separated BitBlt then shows the **3D world** on the client (trees/terrain; no DLC tiles). Host still in world.
- Full leave recipe is now dismiss → pop-noreopen → destroy-movie (`local/client_leave_title.py` + `local/fe_destroy_movie.py`).
- After world reveal, client log still has `Unable to bind … Dude|CoopPeer2` and `CPlayerInput::SetState called: should never happen`. ActionGame still `+0x1c` client nub only (`+0x20`/`+0x28` null). Possession not solved.

### Derived

- Title→world on the joined client is **three** steps: (1) FE flag dismiss, (2) menu stack pop without reopen, (3) destroy orphan Class3FrontEnd movie via `vtable+0x3C(1)`.
- Next wall is **possess / local view / input** for CoopPeer2 / Dude(1), not Flash.

### Unknowns

- Correct path to bind the local view/actor so input works (fix `Unable to bind` / `SetState`).
- Whether `0x016171F0` as a whole (vs only `vtable+0x3C(1)`) is safe post leave-stack (earlier it re-set fe when mis-ordered).

### Live PIDs (this session)

- Host `6696` / client `18048` after full leave recipe: both render world when windows are tiled apart. Client still unbound for input.

## Phase 3m — Join crash fix + local actor id (2026-10-21, this PC)

### Observations

- Join dump `08:35:48`: AV at `0x013E7425` (`mov edx,[edi]`) with `edi=0`, `eax=0`. The prior null-edi cave returned `eax=0` into `test eax; jz` which lands on that second vcall with `edi` still null. Fixed in `patch_client.py`: null `edi` now `jmp 0x013E743F` (past both vcalls) instead of resume at `0x013E741C`.
- Host menu: **Start a New Game is idx 2** on this profile (idx 1’s handler is DLC `0x01619B10`). `boot_to_gameplay.py` now forces idx 2.
- Stale host after a crashed client keeps `Resend last packet` to the dead UDP port and can make the next `connect` immediately `disconnect`. Prefer kill both and clean dual boot.
- After a clean join (host `4932` / client `17660`): channel 2 / CoopPeer2 / rename Dude(1) as before. Leave-title cleared FE movie `+0x44`; depth sometimes stays 1 / fe flaps to 1.
- Game framework object at `gEnv+4` (`0x01EE2F18`): **`+0x78` is local entity id** — host `0x7777` (Dude), client `0xFE5F` (=65119, matches `PlayerRenamed 65119 Dude(1)`). So the client already has a local actor id for Dude(1); input still broken (`Unable to bind`, `CPlayerInput::SetState` warn stub at vtable slot `0x0191C9E8 → 0x0164DBC0`).
- Do **not** probe EntitySystem via CreateRemoteThread into arbitrary vtable slots — that hung/killed both processes earlier.

### Derived

- Join stability requires the skip-both-vcalls null-edi patch.
- Possession wall is not “missing local entity id”; it is view/input/network bind (`Unable to bind` at `0x7908C9`) while FE leave is otherwise good enough to see the world.

### Unknowns

- How to finish network bind / client view for entity `0xFE5F` so `CPlayerInput` gets real local OnAction instead of SetState warn.
- Reliable menu-depth 0 after leave-stack when pop no-ops at depth 1.

### Leave-stack crash / safer recipe

- Full dismiss → pop-noreopen → destroy crashed client at **`0x0056FFF8`** (dump `08:51:53`, `ecx` garbage after `mov ecx,[edx]`).
- **Dismiss → destroy only** (skip pop) keeps the client alive: `fe=0`, movie `+0x44=0`, depth may stay 1, BitBlt shows world. `client_leave_title.py` now defaults to that; pass `--pop` for the old path.
- Live PIDs after safe leave (this pass): host `20792` / client `22240`. `gEnv+4+0x78` still host `0x7777` / client `0xFE5F`. Client later died again after console `g_detachCamera`/`r_DisplayInfo` (new dump — check EIP before retrying those cvars).
- `Unable to bind` path: `0x7908A0` looks up a word key at game-object `+0x50` in a map (`0x72E410`); null → log. Client still hits this for Dude/CoopPeer2 after join. Next possess lead: identify that game-object on the client and why the map miss happens (vs host Dude).
- Host entity-slot-like records exist for Dude (`vt 0x183D1B8`, eid at +8). Client had no matching records for `0xFE5F` / `0x1FF5D` in the same shape; raw `0xFE5F` heap hits look like non-object data.
- **Unsafe on joined client (crash):** `g_gameRules:GetPlayerByChannelId(2)` via `#System.Log`, and earlier `g_detachCamera` / `r_DisplayInfo`. Prefer pure RPM for possess dig. Orchestrator: `local/session_join_leave.py` (host boot + patch + connect + dismiss/destroy leave).

## Phase 3n — CGameObject layout + bind map root (2026-10-21, this PC)

### Observations

- `CGameObject` is MI: ctor writes **`[this]=0x017D5F48`** (primary) and **`[this+4]=0x017D5E68`** (IGameObject; `BindToNetwork` at slot 4 = `0x016071C0`…/`0x7907C0`). Slot 0 of secondary is an adjustor (`sub ecx,4`); **BindToNetwork itself is called with `this = obj+4`**.
- Layout (verified host Dude / client Dude(1)):
  - `+0x8` = entity id (`0x7777` / `0xFE5F`)
  - `+0x54` = **net channel** (host Dude `1`, CoopPeer2 / client Dude(1) `2`)
  - `+0x10` / aspect obj holds world pos — client Dude(1) at ~1345,3442,117 (spawn)
- `BindToNetwork` map provider: `[*(0x019EF9AC)+0x28]+0x28` → **`ActionGame+0x28` (game nub)**. Host has it; **client `AG+0x28=0`**, so the map getter returns null on a joined client. `Unable to bind` is the server-nub map path; client missing game nub matches earlier ActionGame notes.
- `BecomeLocalPlayer` is GameObject extension **event id 8** (registered with PauseGame/OnCollision/… at `0x674Axx`). Not yet invoked.
- Helper: `local/find_gameobjects.py`. Live after safe leave: host `15916` / client `8692` (BitBlt world avgB≈71).

### Derived

- Local eid + channel + world pos are present on the client GO. Remaining possess work is **view/action-map / BecomeLocalPlayer (or equivalent)**, not finding the entity.
- Do not expect client `AG+0x28` to match host; fixing input likely means driving the local-player event path, not cloning the server game nub.

### Unknowns

- How to fire extension event 8 (`BecomeLocalPlayer`) on the client GO safely on the main thread.
- Which object owns `CPlayerInput` / action maps for eid `0xFE5F`.

### Attempt

- IGameObject method at `0x78EF20` (secondary vt ~slot 17) special-cases `ebx==8` as BecomeLocalPlayer. Naive main-thread call `SendEvent(igo, 0, 8, 0, 0)` **crashed** the client (dump after call). Need correct `SGameObjectEvent` / arg layout before retrying.

## Phase 3o — BecomeLocalPlayer args + CPlayerInput (2026-10-21, this PC)

### Observations

- `SendEvent` at `0x78EF20` is **stdcall 4 args** (`ret 0x10`). `ebx` = stack arg1. Extension loop calls `ext->vt+0x2c` (HandleEvent) with those 4 args; any extension returning 0 fails the whole call. After all extensions return true, `arg1==8` hits a BecomeLocalPlayer tail that requires **`IGO+0x18 == 0`**, then `entity = [IGO+0x0c]`, `proxy = entity->vt+0xe8(1)`, then `proxy->(arg0->vt+0x28)(arg0)`.
- When set, **`IGO+0x18` points at `CPlayerExt+0x14`** (MI adjustor inside the player extension). Host local Dude and client Dude(1) both have it set after spawn — the tail is a “not yet local” path.
- CPlayer HandleEvent `0x1574A30` first calls helper `0x13EA531`. Helper **`cmp [esp+0x0C], 8`** (SendEvent **arg2**, not arg1). If arg2==8 it takes an ActorMass/`skip` profile path; bad stack object → AV at `0x13EA5C4` (`mov eax,[ecx]`). That matches the naive `(0,8,0,0)` crash (eip ~`0x13EA5D8`). **Do not pass arg2=8 without a real profile object on stack.**
- With `IGO+0x18` cleared and `(arg0=igo, 8, 8, 4)` SendEvent returned **0** safely (no CPI change). Direct BLP tail with `arg0=igo` completed (`entity->e8(1)` non-null) but **still left CPI inert**.
- **`CPlayerInput` already exists** on both: `player+0xF34` → MI object (`vt 0x0191C9D4 / +4 0x0191C9C4 / +8 0x0191C98C`). `GO+0x14` = that object +4. Host local has view floats at CPI+0x0C..+0x14 and **`CPI+0x20 = 0x1C`**; client Dude(1) has zeros / `+0x20=0`.
- CPlayerInput ctor `0x1650070` is called from CPlayer code at `0x1572270` (inside large method starting `0x1571FD0` = CPlayer **vt+0x28**, stdcall 1 arg; strings `PlayerInputExists` / `mountedWeapon`). Not yet invoked with a proven arg.
- Flag byte **`player+0x57`**: host local `0x0D`, client Dude(1) `0x00` (both have `+0x56=1`).
- Host Dude GO (`eid=0x7777`, ch=1) is easy to miss with `find_gameobjects.py`’s `+0x0C` pointer filter; locate via `player+0xF34` / CPI owner instead.
- **Unsafe:** BLP-style call with `arg0 = CPlayerInput*`; killed client.

### Derived

- Possession wall is **activating** the existing CPlayerInput / local-view path, not allocating CPI or setting `gEnv+4+0x78` (already `0xFE5F`).
- Safe SendEvent for event 8 should keep **arg2≠8** (avoid helper ActorMass) or supply a real stack profile; prefer driving **CPlayer vt+0x28** / CPI activate once the arg is known.
- Next: decode `0x1571FD0`’s single argument (what host passes), or find what sets `CPI+0x20=0x1C` / `player+0x57=0x0D` on host New Game.

### Unknowns

- Exact arg to CPlayer `vt+0x28` (`0x1571FD0`) for local-player init (looks like script/property bag: `viewQuat`, `PlayerInputExists`, …).
- What writes host CPI `+0x0C..+0x14` (stable ~3255/3240/216 floats — not per-frame mouse deltas).
- How to get the case-8 body in `0x13EA6C0` to finish view/input bind on the joined client.

### Follow-up (same day)

- `CActor`-ish vt `0x018CEB98` slot 16 = `0x13EA6C0`: `HandleEvent(SGameObjectEvent*)` with `cmp eax,8` → sets `player+0x56=1`, calls `IGO→vt+0x60(1)`, then entity flag tweaks. No live objects start with that vt (MI-only?). Calling it with `this=CPlayer*` + stack `{8,0,0,0}` returned safely but left CPI floats still zero.
- `IGO→vt+0x60` (`0x78F520`) only stores arg into `IGO+0x7C` and forwards — not full possess.
- Client `player+0x57` differs from host (`0x00`/`0x30` vs `0x0D`); poking `0x0D` alone does not wake CPI.
- **CPlayerInput MI:** `player+0xF34` → base vt `0x0191C9D4`. Big update `0x164D580` is **base slot 6** (`vt+0x18`), not third-vt slot 24. Callers pass a **pointer to a stack struct** (`lea edx,[esp+…]; push edx`). Sites: `0x1574C89` (inside CPlayer HandleEvent after building the struct) and `0x156C582`.
- **Unsafe:** naive `SendEvent` with event `0x10` / `0x20` / `0x4000` (killed client). CPI slot24/slot6 with wrong `this`/args also unstable.
- Helpers: `local/probe_local_player.py`.

### Follow-up — CPI source + proxy0 (same day, continued)

- **What writes CPI `+0x0C..+0x14`:** `CPlayerInput::Update` (`0x164D580`) copies the first 3 dwords of **`view+0x304`** (via `player+0x9CC` → view → `vt+8` memcpy of `0x3D` dwords). Host local matches world pos there; client CoopPeer2’s `view+0x304` stays zero.
- **Why client `view+0x304` stays 0:** view pose updater `vt+0x30` (`0x1682520`) needs `entity->GetCharacter(0)` (`vt+0x144` / `0xBC66F0` → proxy0 → slot `+0x6C`). Client **CoopPeer2 / Dude(1) have `entity` proxy[0] == NULL** (host remote CoopPeer2 likewise). NPCs on the client often have proxy0 but still `GetChar`/`+0x6C` empty — only the host local Dude reliably returns a live character instance.
- **`CreateProxy(0)`** (`entity vt+0xF0` / `0xBCB520`) on client CoopPeer2 allocates a proxy (`vt 0x180F6F8`) safely but leaves the character vector empty — not enough for pose.
- **Workaround that sticks:** write aspect/world pos into `view+0x304`; then `Update` fills CPI and **keeps** it (earlier “CPI cleared” was Update copying zeros from empty `view+0x304`). Patching out `Update` also made manual CPI pokes stick; `SetState` patch did not.
- **Event `0x4000` gate:** HandleEvent uses **`arg0->vt+0x20()`** (not arg2); need return **0** to call CPI Update. A minimal fake vtable (`xor eax,eax; ret` only) **killed the client** — other slots / later `0x1565E80` paths touch `arg0`. Need a real message/object or skip SendEvent and drive `0x13E7000` / camera bind after seeding CPI.
- Injection caveat: check `ctx.Esp`; cool down after splash / rapid restarts.

### Follow-up — pose seed live (same day)

- Helper: `local/seed_cpi_pose.py <client_pid> [--patch]`. `--patch` trampolines `CPlayerInput::Update` (`0x164D580`) to copy `IGO+0x0C` entity `+0x24` xyz into `view+0x304` each call. Confirmed on live join: CPI `+0x0C..+0x14` stay at CoopPeer2 world pos; client stays alive.
- **`0x4000` arg0 is a property-bag**, not IGO: gate is `arg0->vt+0x20()` (no stack args) must return 0; later `0x1565E80` reads named props via `arg0->vt+0x4C/0x68/0x74`. Minimal fake vtable killed the client. **Do not** call IGO `vt+0x20` (`0x792190`) with no args (it expects stack args — also fatal).
- `player vt+0x134` = `return byte [this+0x56]`; `vt+0x130` = nonzero if `IGO+0x50` (channel word) nonzero. Host ch=1 / client CoopPeer2 ch=2 both pass.
- Client `player+0x57` still `0x00` vs host `0x0D` (poke alone unproven for input).

### Follow-up — view system empty on client

- Object at **`gEnv+0x24`** (`vt 0x178F470`): host has populated pointer fields at `+0x08` / `+0x14`; **joined client is mostly zeros there** (same vt/`+0x04=2`). Likely I3DEngine (not CViewSystem); `+0x08/+0x14` were freelist/string noise, not the render cam.
- Live PIDs after rejoin+seed: see `local/last_pids.txt`. Re-apply `seed_cpi_pose.py --patch` after each client boot.

### Follow-up — render camera already on CoopPeer2

- **Rendered camera** lives in static `gEnv+0x10` object (`0x1DD7320`) at **`+0x1394`** (also mirrored at `+0x13C8`, `+0x1690`, …). Host ≈ Dude; joined client ≈ CoopPeer2 (`~1345/3439/118` vs peer `~1345/3442/118`) once world is up — **no separate SetActiveEntity needed for position**.
- Player view (`player+0x9CC`, vt `0x191FF6C`): host fills **`view+0x18`** with cam xyz; client peer left it 0. Seeding `view+0x18` from aspect sticks. `view+0x10` host local `0x80001008` vs client `0x80201068` (poke to host value sticks).
- CViewSystem is created into CryAction `+0x4C4` (ctor `0x75C970`, multi-vt `0x17D1FF0/FD4/FB8`); `SetActiveCamera` = iface `vt+4` → `0x75B710`. Framework at `0x01EE2F18` is **not** that owner (`+0x4C4=0`).
- Console **`EnableActionMap` is unregistered** on client (`Unknown command`). Call **`IActionMapManager` `vt+0x24`** (`0x1098570`, `ret 8`) with `(name, enable)` via main-thread inject. Helper: `local/enable_actionmap.py <pid> [player]`.
- Input still short vs host local: client peer often has **`player+0x57=0`** (host `0x0D`) and **`CPI+0x28=0`** (host `0x1C`). Both stick when poked; `seed_cpi_pose.py` now sets them + seeds `view+0x18` / `view+0x10`. Patch trampoline also copies aspect → `view+0x18`.

### Follow-up — leave-title recipe (depth 0 safe)

- After dismiss+destroy, **`menu_pop_noreopen.py` can reach `depth=0` with `fe=0`, `ag` live, `localEid=0xFE5F`** without AV (confirmed host 11496 / client 8172). Do dismiss → destroy → pop (not pop alone; not depth poke).
- **But BitBlt still shows DLC carousel** (`[SPC] Play State of Decay`) even with `elem+0x44=0` and depth 0. Gameplay `renderCam` (`gEnv+10+0x1394`) sits at CoopPeer2 while the window composites the FE farm scene + logos — need a different Flash/UI teardown (or FE level unload), not another `+0x44` clear.
- Console `EnableActionMap` = unknown; use `local/enable_actionmap.py`. `seed_cpi_pose.py` now also seeds `view+0x18`, `view+0x10`, `player+0x57=0x0D`, `CPI+0x28=0x1C`. Do **not** `movups` into `view+0x18` (clobbers `+0x24+` and breaks Update→CPI); use 3× `movss`.

### Follow-up — logos survive every FlashUI movie clear (2026-10-21)

- FlashUI manager (`gEnv` query `"FlashUI"`, vt `0x17BD070`) keeps a **vector at `+0x40`** of ~10 elements (all vt `0x17C6DF8`). Index 0 is `Class3FrontEnd`; **index 2 is often the only other `+0x4C=1` (visible) element** with a live `+0x44` movie after leave-title.
- Helper: `local/flash_destroy_all.py` — walks that vector and calls `vt+0x3C(1)` on every element with `+0x44!=0` or `+0x4C!=0`. Confirmed all `+0x44`/`+0x4C` stay 0 afterward; client stays up.
- **BitBlt still shows the three DLC logos + `[SPC] Play`** after that full clear, with `LoadMovie` stubbed (`0x6E0A30` → `xor eax,eax; ret 4`) so movies cannot reload, and after nulling element `+0x48` (GFx player). Ret-patching several FlashUI vtable methods also did not remove logos.
- Assets still in heap: `FrontEnd.gfx`, `Libs/UI/Class3_FrontEnd_I1.dds`, AS names `LogoToDisplay` / `PressStart`, meshes `logo.cgf` / `logo_sign.cgf` (also world props). Logos are **not** only `Class3FrontEnd+0x44`.
- Gameplay `renderCam` at CoopPeer2 is live while the window shows the FE farm; forcing other cam slots in the same object to peer xyz did not change BitBlt. **Do not poke `depth=0` by hand** — still kills the client (`ag→0`).
- `bitblt_pid.py` now prefers the hwnd titled `StateOfDecay` (crash-dialog / `DIEmWin` otherwise steal the capture).
- Leave-title can leave `depth=1`/`fe=1` if pop does not stick; a clean session still reaches `depth=0`/`fe=0` via `client_leave_title.py`. Live PIDs: `local/last_pids.txt`.
- `client_leave_title.py` now runs `flash_destroy_all.py` instead of only `fe_destroy_movie.py`. Element `[2]` sometimes survives one pass (movie reloads); a second destroy with `LoadMovie` stubbed clears it.
- Console `gfx_draw 0` / `sys_flash 0` via working `ExecuteString` (callee cleans args — do **not** `add esp,0x14`) left the client alive but **logos still BitBlt**. So the remaining composite is not gated by those cvars (or draws as FE level meshes / `FrontEnd.gfx` outside FlashUI). Wrong stack cleanup on `ExecuteString` kills the client.
- **BitBlt is a weak oracle:** writing `renderCam` (`+0x1394` / mirrors) to a totally different xyz did **not** change BitBlt pixels while the memory values stuck — GDI BitBlt is likely not the live D3D swap chain (DWM cache / last GDI frame). Prefer engine capture or host-side observation of CoopPeer2.
- **`e_ScreenShot` is server-controlled** on the joining client (host resets it; log shows `Server controlled CVar: e_ScreenShot`). Run screenshots on the host, or find a client-local capture path.
- FE UIElement XML for `Class3FrontEnd` lists `Class3_FrontEnd.gfx` on **layer 2**, functions `Setup3DElements` / `HideMainMenu` / `OpenFrontend` / `ShowDLCSelect`, and `cry_onSetup`. Logos may be **Setup3DElements** world billboards that outlive `+0x44` destroy.
- Game code at **`0x0161BA00`** (menu entry idx 3 slot0) contains `push HideMainMenu` + call through **`[caller_ebp+0xF4]`** (Invoke) on the Class3FrontEnd element, then touches Journal. Calling `0x161BA00` alone from inject **sets `fe=1` again** and can reload Flash movies — treat as menu-enter, not a safe hide. Related toggle helper **`0x0161B6B0`** flips byte **`0x0197327C`** and plays `sounds/ui:ui_main_menu:*`.
- `local/console_exec.py` for safe main-thread console commands.

### Follow-up — HideMainMenu Invoke clears logo composite (2026-10-21)

- Element **Invoke = vt+0xF4 (`0x6E37D0`)**, stdcall 3 args `(name, SUIArguments*, 0)`. Backend vt+0xF0 auto-`LoadMovie(1)` when `+0x44==0`. Helper: `local/hide_main_menu.py` (default `HideMainMenu` then `FadeOutPressStart`). SUIArguments ctor/dtor `0x5CD0A0` / `0x5C7590`.
- Do **not** call `0x161BA00` (menu-enter). Safe path: leave-title → Invoke → `flash_destroy_all` again. Wired into `client_leave_title.py` steps 4–5.
- **Oracle:** `PrintWindow(..., PW_RENDERFULLCONTENT)` — count 40×40 near-white blocks. Logo FE (`c-world.bmp`) ≈28/228; after HideMainMenu+FadeOut+destroy ≈**0/228**. BitBlt alone remains unreliable. No FE-named entities near cam; logos were Setup3D/AS-driven, not leftover `+0x44` movies.
- EntitySystem is **`gEnv+0x38`** (query-by-name returns null). Entity map vector at `ES+0x68..+0x6c`; `GetEntity` = vt[10] (`0xBA0FB0`). Entity world pos ≈`+0xDC`; name often inline/`+0xB0`. **Do not** probe random ES vtable slots with a lone EntityId — stack-corrupt / kill client (lost prior session; recovered via `session_join_leave.py`).
- Live session after recover: host/client in `local/last_pids.txt`. `seed_cpi_pose.py --patch` + `enable_actionmap.py player` re-applied. `GO+0x57=0x0D`, `CPI+0x28=0x1C` stick; **keybd_event / PostMessage WASD still do not move** eid `0xFE5F` (Dude / CoopPeer2).
- `CPlayerInput` MI vts `0x191C9D4` / `+4 0x191C9C4` / `+8 0x191C98C`. **Primary vt[5]=`0x164DBC0` is the static “SetState called: should never happen” stub** (push log + `ret 4`) — not a corrupted slot. Keyboard path is ActionMap→OnAction, not SetState; the log fires when something wrongly calls SetState on this CPI. `seed_cpi_pose` hooks **vt[6] Update** (`0x164D580` → cave).
- **Two ActionMap managers:** `gEnv+0x20` (vt `0x185CA20`, used by `enable_actionmap.py` / `EnableActionMap` at `0x1098570`) is **not** the one game code uses for filters. Real manager: `gEnv+4` → `vt+0x38()` → `vt+0x70()` → AMM vt `0x17D5500`; **`EnableFilter` = vt+0x68 (`0x78A480`, ret 8)**. Game toggles `no_move`+`no_mouse` together at `0x013D2BC0`. Helper: `local/enable_filter.py`. Wired into `client_leave_title` after HideMainMenu.
- After filter clear + player map enable + CPI flags, **keybd/SendInput still do not move** eid `0xFE5F`. Host gameAMM also has filter-index-ish `+0x7C=-1`. Possession / local-actor binding (not just filters) is still the blocker.

### Follow-up — possess dig (2026-10-21, continued)

- **BecomeLocalPlayer already applied:** both host Dude and client CoopPeer2 have `IGO+0x18 = CPlayerExt+0x14`. Event-8 BLP tail is a “not yet local” path — it will not re-run.
- **gameAMM `vt+0x30(name, listener)`** = SetActionListener (ret 8). With `listener=CPI` it refcounts `CPI+4` and returns `CPI+4`. Helper: `local/set_action_listener.py` (aborts if main-thread EIP is outside the game module — see crash dialog below).
- **Only live `CActionMap` (vt `0x192248C`) in either process is `muscle_car_variant_shiny`.** No `player`-named map object. `enable_actionmap.py` hits `gEnv+0x20` AMM; movement bindings are not that singleton.
- **`CPlayerInput::OnAction` body `0x164F890`** (ret `0xC`; mid vt[1] adjustor). Needs a real ActionId (CCryName), not a raw `"moveforward"` string — naive calls return safely with no pose delta.
- **`CPlayer::Update` = vt+0x3C (`0x1575E80`).** Host: ~300+ calls/s. Client CoopPeer2: **0** while hung.
- **Player list:** host `LIST[0]` = local Dude; client `LIST[0]` = ch=1 remote, CoopPeer2 often last. CPI `Update` is not invoked for the peer unless something calls it (manual inject fills xyz from seeded `view+0x304`).
- **Leave-title:** first `menu_pop` can leave `depth=1`; second destroy+dismiss+pop reaches `depth=0` with `ag` live. After `HideMainMenu`, FlashUI query can fail — `client_leave_title.py` now continues.
- **Critical hang:** client `#32770` title **`Crash Dump Error!`** leaves main thread in `win32u` (`Responding=False`). Game Update never runs → WASD impossible until restart. Avoid injects while EIP is outside `0x00400000–0x02000000`. Dump: AV in `ntdll` (`0x77091432`); dialog close kills the process.
- **Leave-title crash source:** `flash_destroy_all` / pop path often raises Crash Dump Error. **`fe_dismiss` alone is safe:** `depth=1` `fe=0` `ag` live, **no dialog**, `CPlayer::Update` ~6k calls/s, `seed_cpi_pose --patch` fills CPI xyz, `set_action_listener` returns `CPI+4` and client stays `Responding=True`. Pop alone does not drop depth (stays 1, may set `fe=1`). Prefer dismiss-only for input work until destroy is fixed.
- Live PIDs after dismiss-only session: see `local/last_pids.txt`. WASD still no pose delta after listener+filters+seed at depth 1.
- Hooked `OnAction` (`0x164F890`): **0 calls** during WASD `keybd_event` while listener is bound and Update is live. Same on **host** (pos unchanged, OnAction cnt 0) — game ignores synthetic `keybd_event` (DirectInput/raw path). Not a useful WASD oracle.
- `client_leave_title.py --dismiss-only` for the safe path (dismiss + filters + env EnableActionMap).
- Helpers: `local/set_action_listener.py`.
- **IInput** ≈ `gEnv+0x14` (vt `0x182DCC8`, near DirectInput strings). `+0x8/+0x10` look like paired buffers but are not a plain DIK[256] map (writing `DIK_W=0x80` stuck, no move / no OnAction).

### Follow-up — ActionId OnAction + FE input gate (2026-10-21)

- **Join AV** at `0x79140C` (`test [esi+0x5C],2` with `esi==0` from list node `+0x0C`). Dump `StateOfDecay_*_15-34-02.dmp`. Patched in `local/patch_client.py` as **join null list payload esi** (skip to `0x79147A`).
- **Host New Game** often sticks at `depth=3` idx=5 (handler `0x161DA20` list UI) and never issues `map`. Workaround: after `force_map_server`, `console_exec.py <pid> "map class3 server"` → listen + Dude + UDP. Prefer fixing the depth-3 confirm path later.
- **Join wait must require fresh `CoopPeer2`** (not stale `OnClientConnect channel=1` / old log lines). Use a log marker or `after_size` + `CoopPeer2` only.
- **ActionId**: `gEnv+0x58` → `vt+4(const char*)` returns NameEntry*; **ActionId value = entry+0xC** (string). `moveforward` lives at GF ActionId table `+0x8` (same handle). Helper: `local/inject_onaction.py`.
- **`CPlayerInput::OnAction` (`0x164F890`)** is gated by **`0x146EF10`**: if it returns 1, OnAction zeroes move floats and returns without dispatch. Gate reads **`[0x1EA7FFC]`** (`test eax, 0xC01A00` — client/host often have bit **`0x800`** set while FE depth≥1) plus bytes `0x1EA70D9` / `0x1EA710A`. Patch gate → `xor eax,eax; ret` (or clear `0x800`) and OnAction(`moveforward`, mode=4 hold, value=1.0) sets **`CPI+0x30 = 1.0`**.
- With gate clear + `CPI+0x30=1.0` sticky, **entity `+0x24` xyz still does not change** on client CoopPeer2 or host’s ch=2 peer (**console-map session**). Console `map class3 server` leaves FE up, Dude falls through world (Z→−20k), and **`CPlayer::Update` (`0x1575E80`) never runs** — OnAction floats are inert there.
- **`CPlayerInput::Update` (`0x164D580`, vt+0x18) is never called for CoopPeer2** (private-vt call counter = 0 over 4s). Callers: `0x1574C89` (HandleEvent path) and player-list loop `0x156C582` inside `0x156C4A0`. **Do not** call Update with a zeroed ctx (hung/killed client).
- Live helpers: `inject_onaction.py <pid> moveforward --hold --patch-gate`. `local/call_player_list_update.py` forces `0x156C4A0`.

### Follow-up — New Game host: OnAction moves local Dude (2026-10-21)

- **Do not use console `map class3 server` for input work.** Use `host_boot_coop.py` (`force_map_server` + `boot_to_gameplay`). New Game briefly hits depth=3 then settles at **depth=0**, UDP listen, Dude grounded, **`CPlayer::Update` ~150–270/0.5s**.
- On that host, gate-patched `OnAction(moveforward, hold, 1.0)` sets `CPI+0x30=1.0` / `+0xd4=1.0` and **Dude entity `+0x24` moves** (~10 m over 3 s). First confirmed pose delta from injected ActionId.
- **`session_join_leave.py`** now boots via `host_boot_coop` (not console map). Leave defaults to `fe_dismiss` only.
- **Join without `fe_dismiss` stays healthy** (both titles `StateOfDecay`). **`fe_dismiss` after join often raises client `Crash Dump Error!`** and parks main thread in win32u — revisit; prior “dismiss-only is safe” note is **not** reliable on this boot path.
- **`OnAction` on host CoopPeer2 CPI** (ch=2) hung/failed (`mark` stayed `DEAD`) and raised host Crash Dump — do not call OnAction on remote peer CPI until gated. Peer often spawns at default `(1345,3442,117)` while Dude is elsewhere after New Game.
- Trampolines: steal **full** instructions (e.g. 6-byte `sub esp,imm32` via `FF 25` abs jmp). A 5-byte `E9` over a 6-byte insn corrupts `CPlayer::Update`.

### Follow-up — client peer Update live; authority gap (2026-10-21)

- After clean `host_boot_coop` + join (**no** `fe_dismiss`), client often already has `fe=0` at depth=1, stays `StateOfDecay`, and **`CPlayer::Update` ~6k/s total / ~300+/s with `ecx==CoopPeer2`**.
- Client CoopPeer2 CPI vt = **`0x191C9D4`** (real `CPlayerInput`). Host’s ch=2 CPI vt = **`0x1920D08`** (remote/net input). Remote slot `+0x10` at `0x14D0470` is **`ret 0xC`** stub — never call Dude `OnAction` (`0x164F890`) on host peer CPI (Crash Dump).
- Client `OnAction(moveforward)` + sticky `+0x30/+0xd4` with Update live: **no entity `+0x24` delta** (host peer also unchanged). Local host Dude *does* move from the same inject when the session is healthy — client peer is not movement authority.
- Raw `entity+0x24` writes on host peer snap back within 1s (net/physics). `wait_join` must use **fresh log bytes after `after_size`** and confirm **client `ag != 0`** (stale `CoopPeer2` lines false-positive).

### Follow-up — host remote CPI pose drive (2026-10-21)

- Host CoopPeer2 movement authority is the **remote CPI** (`vt 0x1920D08`): writing world xyz into **`CPI+0x3C` and `CPI+0x58`** (both triplets) updates `entity+0x24` and sticks. Helpers: `local/drive_remote_peer.py`, `local/bridge_peer_move.py`.
- Confirmed: `--to-dude` / `--nudge` teleports host peer next to Dude; bridging client `CPI+0x30` (from `inject_onaction`) → host remote pose steps moved host peer **~15 m** while both stayed `StateOfDecay`.
- Client `entity+0x24` is overwritten by net each tick (snaps to spawn). **High-rate rewrite of `+0x24` + cam holds** for the duration of the sync loop; `+0xDC` sticks even after. Helper: `local/sync_peer_view.py` (now writes both). Same in `wasd_bridge.py` / `bridge_peer_move.py`.
- Entity vt+0x74 (`0xBC72F0`) is **not** a usable SetPos for peers (returns ok, no `+0x24` change) and **raised Crash Dump** once — do not call.
- GetPos-like stubs: vt+0x60 / +0x70 → `lea eax,[ecx+0x24]; ret`.
- Working control loop: `drive_remote_peer.py --to-dude` then `wasd_bridge.py <host> <client> --cam [--demo]`. Keep a sync loop running or cam/`+0x24` revert. Physical WASD needs real keys (`GetAsyncKeyState`); `--demo` for unattended tests.
- After client Crash Dump, host often stops accepting new joins — **full `session_join_leave` reboot** rather than reconnect-only.

### Follow-up — net sync asymmetry (2026-10-21)

- **Host Dude → client Dude(1) position sync works** (same xyz). Proves the listen/join pipe replicates the local player.
- **Host CoopPeer2 pose pokes do not reach client CoopPeer2** (`+0x24` stays spawn). Extra host mirrors at remote CPI `+0x94` and `player+0xBB8` also fail to replicate.
- ActionGame nub slots (live join-only): host `AG+0x1C`/`+0x20` both vt `0x1836EA0`, **`AG+0x28` vt `0x17CDB50` (game nub)**; client has **`+0x1C` only**, `+0x20`/`+0x28` = 0. Matches `Unable to bind` map root (`[*(0x019EF9AC)+0x28]+0x28` → `AG+0x28`). Do **not** copy host nub pointers into the client.
- Client CoopPeer2 `+0x24` force-reset every tick (Dude flag copy does not help). **Continuous client overwrite** of `+0x24`/`+0xDC`/cam remains the view fix; host motion stays on remote CPI (`drive_remote_peer` / `wasd_bridge` / `sync_peer_view`).
- Live: see `local/last_pids.txt`. Avoid entity vt+0x74 inject (Crash Dump).

### Follow-up — channel authority + AG+0x28 create (2026-10-21)

- **Authority is flipped per channel (expected CryEngine):**
  - ch=1 Dude: host CPI vt `0x191C9D4` (local), client vt `0x1920D08` (remote) → host→client sync works.
  - ch=2 CoopPeer2: host CPI vt `0x1920D08` (remote), client vt `0x191C9D4` (local) → **client owns motion**; host remote CPI pokes move **host only** and will not appear on the owning client.
- Confirmed live: host `--to-dude` moves host peer; client ch=2 stays at spawn. Client entity/`CPI+0x3C` pokes snap back; `inject_onaction` sets client `CPI+0x30=1.0` but entity does not walk. Forced `CPlayerInput::Update` (`0x156C4A0`) spam did not move either (and once killed the client — avoid).
- `GO+0x14` is set on **local** players only (host Dude, client ch=2); null on remotes. `GO+0x58=0xff` on both sides after join (bind flag looks set despite log spam).
- **How host creates `AG+0x28`:** AG method table slot `0x017C8BC8` → `0x6EE330` (string near path: `[Host Migration]: CActionGame::OnPromote`). Flow: alloc `0x58` via `0x404100`, ctor `0x72FB00` writes vt `0x017CDB50`, `mov [AG+0x28], eax`, then `0x72E2A0`. Gated by `g=*(0x019EF9AC)` → `vt+0x1F0` (`0x666980`). Clearer at `0x6EA560` (`mov [AG+0x28],0`).
- Bind map getter `0x668380`: `return AG ? AG+0x28 : 0`. Null → Bind takes alt path straight into `Unable to bind` (`0x7908C9`).
- **Do not** raw-alloc + ctor the game nub on the client the same way — crashed the client process. Need a client-safe registration/bind path, or fix local ch=2 locomotion so outbound aspects can flow client→host.
- Visual stopgap unchanged: `drive_remote_peer` (host) + `sync_peer_view` / `wasd_bridge --cam` (client `+0x24`/`+0xDC`/cam).
- Join-only client stays **depth=1**; host is depth=0. `client_leave_title --dismiss-only` on a freshly joined client still ended **ag=0 / process dead** this session (after prior seed/OnAction patches) — keep skipping dismiss in `session_join_leave`.
- Host peer pose from remote CPI is **temporary**: without continuous writes it returns to spawn (client authority re-asserts spawn). Client `+0xDC` can be held at a target but `+0x24` and host peer do not follow.
- Clearing `no_move` / `no_mouse` / cutscene filters + `EnableActionMap player` + OnAction still leaves ch=2 at spawn (`CPI+0x30=1.0`). Filters alone are not the locomotion gate.
- Do **not** fire the current `inject_onaction` stub at host Dude (ch=1): one attempt zeroed Dude and killed the host process.
- Local-player memory gaps on client ch=2 vs host Dude: `pl+0x57` is `0` vs `0x0D`; `CPI+0x28` is `0` vs `0x1C`; `CPI+0x0C` pose is `0,0,0` vs live xyz. Seeding those + calling `CPlayer` vt+0x54 (`0x156E1C0`, calls `CPlayerInput::Update`) still does not move the entity.
- `CPlayer` “Update” body at `0x156FDF0` early-outs if `player+0x0C` (entity) is null; both sides have an entity. CPI Update call site is from vt+0x54, not from that Update body directly.
- Game-nub bind table (used by `0x72E410` → `0x66BA80`): not a std::map — list/tree under `AG+0x28` with `gn+8` head, node key at `+0x0C`, value at `+0x10`. Host has `gn+0xC=2` entries. Client has no table (`AG+0x28=0`) → `Unable to bind` for Dude and CoopPeer2. Receive path for Dude(1) still works without client bind.
- After `drive_remote_peer --to-dude`, host peer can **stay** near Dude for a while (client not actively correcting); `sync_peer_view` still needed for client `+0xDC`/cam. Live: `local/last_pids.txt`.

### Next

- Make **client-owned** ch=2 actually walk (OnAction → physics) **while still depth=1**, or find a non-dismiss depth→0 path that does not Crash Dump — then confirm host remote peer follows via net.
- Client-safe game-nub: alloc + minimal list + insert local ch=2 key/value, then `BindToNetwork` — **without** running full `0x72FB00` ctor (that crashed once). A bare `VirtualAlloc` shell (`vt=0x17CDB50`, empty sentinel at `gn+8`) can be installed at `AG+0x28` without an immediate crash; Bind was not completed because the client main thread was parked in ntdll (`EIP>=0x70000000`) and hijacking that state killed the process — only inject when `EIP` is in the game image.
- Keep avoiding `fe_dismiss` on join-only clients.

