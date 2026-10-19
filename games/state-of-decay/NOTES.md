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

- The renderer is healthy: the log shows `Renderer initialization`, D3D9 on the RTX 5080, shader cache writable. The window draws the main menu at 68 FPS (farm background, three editions, `SPC Play State of Decay`). So D3D and the front end both render.
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


