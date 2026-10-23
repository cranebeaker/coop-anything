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
| OS | Windows 10 | test machine |

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
- Saves location. Not searched in this phase (no launch yet); Phase 3b found where the first launch of the test copy writes its profile.

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


- Test copy: `local/test-install` (gitignored). 25 files, 2390985941 bytes, same as the Steam install. `StateOfDecay.exe` SHA-256 matches. Steam install not modified.
- `python -m pip install pyyaml` installed PyYAML 6.0.3. `python tools/build_directory.py` wrote `DIRECTORY.md` (256 entries). The gap row is in the Open gaps table.
- Claim issue: https://github.com/cranebeaker/coop-anything/issues/44 (account that Git is already logged in as). The `in-progress` label was rejected with 403 "Must have admin rights to Repository."

Next: decompress `gamedata.pak` from the test copy and read `singleplayer.lua`. Do not edit `<game dir>`.

Checked 2026-10-19 on the test copy: all 1388 entries in `Game\gamedata.pak` use zip compression method 13. Python's zipfile and .NET ZipFile both refuse that method.

That method is Xbox `XMemDecompress` (LZX), the mapping in QuickBMS `zip2.bms`. A reader in `local/lzx/` (gitignored; MonoGame's LZX decoder, LGPL 2.1 or MS-PL) decodes it with LZX window bits **17**. Short blocks start with `0xFF`, then big-endian destination size and source size, then the LZX bytes, then a 5-byte tail. Verified 2026-10-19: `config/multiplayer.cfg` came out as readable text (2977/2977 printable bytes). The same settings extracted the game-rules and player scripts below. Extracted text is in `local/extract/` and stays out of git.

## Phase 3 — Scripts (2026-10-19)

Read from the test copy only, before its first launch (test copies were launched from Phase 3b on, as the README says). The Steam install was not modified.

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
- The exe contains the strings `sv_port`, `sv_maxplayers`, `%USER%/SaveGames`, `%USER%/Profiles/`, and `%USER%/game.cfg`. It does not contain a literal Documents path. Where `%USER%` points was not checked in this phase, before the first test launch (Phase 3b answers it).

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

- Why the listen server marks the second process unreachable after the level has already streamed. A UDP exchange on the LAN address completed 300 packets; closing one socket produced Windows error 10054. During a game join the second process kept its UDP socket open, and the host resent to that same port, then logged `32769` from it and disconnected with cause 12. `sys_SleepIfInactive 0` stretched the resend from about 7 seconds to about 18 seconds and still ended in `32769`. Silencing the client log did not change it. `Scripts/Network/Scheduler.xml` and `EntityScheduler.xml` are in `gamedata.pak` and include a `Default` policy; the scheduling lines are a per-class warning, not a missing file. `map class3 dedicated` binds the port but does not finish the level: a loose `LevelInfo.xml` gets past the first missing file, then `LevelData.xml` and `LevelDataAction.xml` fail to open and `OnLevelLoadEnd` never comes.
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

- SendInput / PostMessage still cannot drive the menu (DirectInput).
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
- Result after `boot_to_gameplay.py`: log shows `Executing console command 'map Class3 server'`, loads **`multiplayer.cfg`**, `OnClientConnect channel=1`, UDP listen on the host address, port `64087` (from `sv_bind` / `sv_port` in host `user.cfg`).
- Host screenshot: full rendered campaign (Marcus/Ed, HUD, minimap, objective, ~117 FPS) — **not** the black `map` path. So front-end New Game + `server` flag gives both render and listen.
- Client (`local/client-install`, `connect <host address> 64087`, `patch_client.py`): host logs `OnClientConnect channel=2`, `AllocateObject ... (CoopPeer2)`, `spawned channel=2 ... name=CoopPeer2`, `PlayerJoined`. Both processes stayed up.
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

## Phase 3h — New test machine; second join crash; channel 2 again (2026-10-20)

Test copies under the gitignored `local/` folder; the Steam install was not modified. Exe SHA matches prior build.

### Observations

- A local network route on the test machine hid the LAN address at first. Same-machine tests then used loopback (`sv_bind 127.0.0.1` / `connect 127.0.0.1 64087`).
- `%USERPROFILE%\Saved Games\USER` and `USER2` junctioned to host/client profile folders.
- Probe Lua paths now point into this project tree.
- First join with only the old NULL guard (`0x01613F9C`) still dumped after `OnLevelLoadEnd`. Dump `…21-33-14.dmp`: AV at `0x013E7410`, `mov edx,[edi]` with `edi==0` (vtable+0x184 call). Second runtime patch in `local/patch_client.py` skips that call and sets `eax=0`.
- With both patches + host `host_boot_coop` + client `connect` in `user.cfg` after UDP listen: host logs `OnClientConnect channel=2`, `AllocateObject … (CoopPeer2)`, spawn at Dude+2 m. Both processes stay up. Client log shows `PrepareLevel` / `OnLevelLoadEnd`, `Unable to bind … Dude/CoopPeer2`, then `PlayerRenamed … Dude(1)`.
- Joined client has a live `CActionGame*` (`0x019F1004` non-NULL) and menu depth 1 / FE flag `0x01EE8807=1`. Calling `0x016171F0` alone did not clear the flag. Wrapper `0x01449810` (unload + follow-up) raised menu depth 1→2 and left the flag set. Do not treat that wrapper as the dismiss path.
- `deferred_connect.py` first cut used `add esp,0x14` after thiscall `ExecuteString` and crashed main thread at `eip=0` (`…21-40-43.dmp`). Stack cleanup fixed to `add esp,8` only; prefer `user.cfg` connect after listen for the known-good path.
- Use a real Python 3 install (the Windows Store `python` stub does not work).

### Derived

- This build needs **two** client NULL guards for a stable join, not one.
- Channel 2 + CoopPeer2 on a rendered listen host reproduces over loopback on the test copies.
- Client already has an ActionGame after join; the remaining wall is dismissing Class3FrontEnd / possessing CoopPeer2, not creating the game object.

### Unknowns

- Correct FrontEnd unload / possess entry that clears FE and gives control of CoopPeer2 without pushing another menu state.
- Whether PrintWindow black captures are D3D-only (likely) or the host really went black after long join.

## Phase 3i — FrontEnd dismiss mapping (2026-10-20)

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
- Stable join recipe: `sv_bind`/`connect` on the host address (on the test machine loopback first resolved to another local host name and failed) + dual client NULL patches + `connect` after UDP; then dismiss-only (`0x01617130`) for FE flag experiments. Confirmed again: channel 2 + fe 1→0, depth left at 1, both processes up.

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

## Phase 3k — Depth clears; Flash carousel remains (2026-10-20)

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

## Phase 3l — World view after destroying FE movie (2026-10-21)

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

## Phase 3m — Join crash fix + local actor id (2026-10-21)

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

## Phase 3n — CGameObject layout + bind map root (2026-10-21)

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

## Phase 3o — BecomeLocalPlayer args + CPlayerInput (2026-10-21)

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


## Phase 3p — Joining player walks, input replicates client→host (2026-10-21, Claude Code)

New tooling in `local/sodlib.py` (gitignored, described here so it can be rebuilt):

- **Safe main-thread calls.** Every older injector (`inject_onaction.py`, `fe_dismiss.py`, `console_exec.py`, …) hijacked the main thread at an arbitrary EIP and jumped back without saving registers or flags. That alone explains many of the "Crash Dump Error!" dialogs blamed on the called functions. `sodlib` instead patches the exe's `PeekMessageW` IAT slot (`0x0178B7F4`) with a stub that runs one queued call per pass on the main thread (thread id check), saves `pushfd/pushad` + `fnsave`, and restores `esp` after the call (works for cdecl/stdcall/thiscall). A vectored exception handler turns an access violation inside a queued call into a reported fault; the game keeps running.
- Counting / argument-logging detours (`hook_count`, `hook_log`), hardware write watch on the main thread (`watch_writes`, DR0 + VEH), exit trap (`trap_exits`: parks `ExitProcess`/`NtTerminateProcess` in a spin loop so a silent self-exit can be inspected), and `lua()` (below).

### Observations

- `"Unable to bind object to network (CoopPeer2)"` on the client comes from `CGameObject::BindToNetwork` (`0x7907C0`): if `GO+0x50` (channel id) is set it asks for the game **server** nub (`AG+0x28`) and warns when there is none. A client never has one, and it only binds when `gEnv->bServer` (`gEnv+0x12C`). Benign on a client; the earlier hunt for a client game nub was a dead end.
- Authority is already delegated correctly. `CGameContext::SynchObject` (`0x76E790`) on the host serializes only objects the host owns (host Dude, 4 survivors, rules); the client serializes only its ch=2 player, input aspect `0x4000`, ~10 Hz (change polling). Built-in `test_delegate <entityName> <channel>` exists (`0x668890`). The ch=2 entity is named `Dude(1)` on both sides (log: `CoopPeer2$o renamed to Dude(1)`), so `test_delegate CoopPeer2 2` finds nothing.
- `CPlayer::NetSerialize` = `0x1574A30`; input aspect branch calls `CPlayerInput::GetState` (vt[6] `0x164D580`) when writing and `SetState` (vt[5]) when reading. Host `CNetPlayerInput::SetState` = `0x169E740`. `GetState` serializes the *physics velocity* of the player when `[0x1E75B74]+0x260 != 0` (it is 1), so nothing changes upstream unless the client player actually moves locally.
- UL world state `[0x1E81F20]`: host **2** (playing), client **3** (front-end idle). Switch at `0x145ED60` (jump table `0x145EE48`): 2 → `0x1449720` then UL world tick `0x145E040` (community sim, UI, all state machines); 3 → `0x145B080`; 4/5 → loading→playing transition `0x145B100` (calls FE teardown `0x16171F0`, sets state 2). `0x1449AD0(teleport, fade)` enters loading (state 4/5). `NSRTSWorld::OnStartGame` = `0x145F730`.
- Calling `0x1449AD0(0,0)` on the joined client takes it 3→4→2 within ~1 s: depth 0, fe 0, UI flags 0, no exit. The world tick then runs on the client, but ~1 min later the client crashed (AV `0x161226E`, UI queue arrays `0x1EE7F5C/60`) — `OnStartGame` never initialised that state. **Not used** in the working recipe below.
- UI gate `0x146EF10` (`[0x1EA7FFC]` bit `0x800` = "a menu is open") has only two callers: `CPlayerInput::OnAction` (`0x164FA82`) and `0x16B4B05`. Clearing bit `0x800` by hand makes the client push the pause menu (depth 1→2); a few seconds later a Flash menu event reached `FrontEndEventListener` (`0x16189A0`) and the client called `ExitProcess` (caught by the exit trap; stack through `NSUIMFrontend::MenuStackPush`). Patching the gate globally had the same effect. Patch only the OnAction call site.
- `CPlayerInput::PreUpdate` (vt[1] `0x164E900`) copies `+0x2C..` → `+0x44..` unconditionally; with the OnAction call-site bypass the client's `+0x48`/`+0x94` follow the injected action.
- Root cause of "client player does not walk": **the client's player entity has no character model.** `IEntity` vt+0x144 (`0xBC66F0`, `GetCharacter(slot)`) returns null for `Dude(1)` on both machines and for the remote `Dude` on the client; only the host's own `Dude` has `characters/male/human/story_characters/marcus_campbell.cdf`. Locomotion is animation-driven (`CAnimatedCharacter::UpdatePhysicalEntityMovement` `0x885E50`), so no character → no movement.
- Console Lua (`#...`) does nothing in this build even with `con_restricted 0` (server-controlled on the client; set it on the host). Lua runs fine through `gEnv+0x20` (IScriptSystem) vt[6] `ExecuteBuffer(buf, len, desc, env)`.
- After `e:LoadCharacter(0, "<that .cdf>")` on the client's `Dude(1)` via `ExecuteBuffer`, holding `OnAction(moveforward)` walked the client player **16.45 m in 4 s (4.0 m/s)** and the host's copy of `Dude(1)` followed **14.95 m**, with host `CNetPlayerInput::SetState` firing ~10/s. Run `walk_test.py`, 2026-10-21 22:0x, session host/client pids in `local/last_pids.txt`.

Other client-side state set during that run (not yet known which are required, see Unknowns):

- `CAnimatedCharacter` pre-physics gate `0x875A30` asks the entity render proxy whether the character is visible (vt+0x64); forced true for the local player's AC only.
- `CAnimatedCharacter` request check at `0x8814C3` drops requests whose renderer-frame stamp (`AC+0x144`) is older than `GetFrameID(1)` (`AC+0x134`); in world-state 2 the client was always one frame behind. Patched to accept one frame old. (In state 3 the stamps matched.)
- `CGameObject::ShouldUpdateAI` (`0x78D8F0`) reads AI activation mode in bits 4–6 of `GO+0x60`; both players were mode 2 (visible-or-in-range). Set the client player to mode 1 (always) so `CGameObject::Update` (`0x7919D0`) stops skipping extensions flagged disable-with-AI (the AC's `Update`, `0x825820`).

### Derived

- The local-player setup the single-player "Play" flow does on the host (survivor model via UL's character switch, AI activation, being on screen) never happens for a joined client. The network path for client-owned input already works once the client player physically moves.
- `BitBlt`/`PrintWindow` captures of the client still show the title art; capture is of the screen region and is not evidence either way.

### Verified on a fresh session (2026-10-21, `local/restart_session.sh` then `local/client_enable_move.py`)

Only two client-side changes are needed; the AI-activation, AC visibility-gate and AC frame-stamp tweaks above are **not** required while the client stays in world state 3:

1. Bypass the UI gate call inside `CPlayerInput::OnAction` (`0x164FA82`: `call 0x146EF10` → `xor eax,eax` + 3 nops).
2. Load a character on the client's `Dude(1)` through `ExecuteBuffer`: `System.GetEntityByName("Dude(1)"):LoadCharacter(0, "<.cdf>")`.

Results (same session):

| Test | Client player | Host copy of `Dude(1)` |
|---|---|---|
| `OnAction(moveforward)` held 4 s (`walk_test.py`) | 16.25 m at 4.0 m/s | 13.27 m at the last sample, host `CNetPlayerInput::SetState` 40 calls |
| `moveback` / `moveright` / `moveleft` / `moveforward`, 1.5 s each | −Y 5.84, +X 5.82, −X 5.82, +Y 5.80 m | gap to client 2.5 s after release: 0.00 / 0.00 / 0.00 / 0.01 m |
| Real input path: client window focused, `SendInput` scancode W (0x11) held 1.5 s (`key_test.py`) | 9 `OnAction` calls, moved (0, +5.72, +2.90) | moved (0, +5.72, +2.90) |

The hard-coded action-name addresses in the old `inject_onaction.py` were wrong: `0x01909ADC` is `"moveright"` and `0x01909AF4` is `"freezetime"`. `move_test.action_id()` now writes the name into the process and looks it up in the name table (`gEnv+0x58` vt+4); ActionId = returned entry + 0xC. One test pressed and released `freezetime` by mistake before this was found; no effect was seen.

### Unknowns

- Turning (`rotateyaw`/`rotatepitch` / mouse), sprint, crouch, interaction, and whether the host's copy animates visibly (it has no model; only its position was checked).
- The client window still shows the title art, so a person on the client cannot see the world they are walking in. World rendering on the client is the next wall.
- Which model the joining player should get and how UL assigns survivors (`0x1543360`: `SwitchChar_Away`, `FROM_PLAYER`). `marcus_campbell.cdf` was used because the host's `Dude` had it.
- Which survivor model the joining player should get, and how UL assigns it (`0x1543360`: `SwitchChar_Away`, `FROM_PLAYER`).

### Mistakes this session (so the next agent does not repeat them)

- Called physics-entity vtable slots 0 and 2 blind, assuming CryPhysics order (`GetType/AddRef/Release`). Slot 0 behaved like a deleting destructor; the host showed "CryEngine Error" and had to be restarted. Disassemble a slot before calling it.
- `last_pids.txt` is written with CRLF; strip `\r` before passing pids to `taskkill`.

## Phase 3q — Joining copy sees the world; joining player visible on the host (2026-10-21, Claude Code)

### Observations

- `BitBlt` and `PrintWindow(PW_RENDERFULLCONTENT)` captures of these D3D9 windows are unreliable (stale or black frames). `local/capture.py` now brings the window to the foreground, grabs that screen area, and restores the previous foreground window. All screenshots below use it.
- The client's system view camera (`gEnv+0x88` → `+0x3DEC`, Matrix34) was already ~4 m behind its own player, same as the host's. The 3D world was being rendered (`I3DEngine::RenderWorld` `0x4CB320` ran every frame on the client).
- What the client showed instead was the renderer's **load-time Flash playback**: the render thread (`gRenDev` `[0x1E05888]` → `+0xBE0`) had video mode `+0x54 = 2` and a load-time callback `+0x8` that renders a second `Libs/UI/Class3_FrontEnd.gfx` player (`0x2F402000` that session) instead of game frames. The host has mode 0, callback 0. Render-thread commands `0x59`/`0x5A` start/stop it (dispatch `0x1227050`, jump table `0x1228878`, index = cmd−1).
- `CD3D9Renderer::StopLoadtimeFlashPlayback` = `0x12FAFE0` (thiscall on `gRenDev`, no args; queues `0x5A` via `0x12239B0` and waits for mode 0). Calling it on the client makes the game frame appear: third-person view of the client's own player.
- Hiding the `Class3FrontEnd` FlashUI element (`vt+0x64 SetVisible(0)`, `vt+0x68 IsVisible` = `+0x4C`) does not remove that picture; it is not drawn by that element.
- `gfx_draw 0` on either machine blanks the whole frame in this game (not just Flash). `gfx_draw` is server-controlled on the client; the "Server controlled CVar" check is `0x76B850` (return 1 to allow).
- The client paused itself after an experiment: `NSUIMFrontend::MenuStackPush` (`0x16141E0`) on the first menu sets UI bit 11 (`0x800`) and calls `IGameFramework::PauseGame(true, …)` (`gEnv+4` vt+0x38 → vt+0x3C). `MenuStackPop` (`0x1614300`) at depth 0 clears the bit and unpauses. Menu stack ids at `0x1EE8168`, table `0x19732A0` (stride 0x14; id 1 = title menu, exit handler `0x1617130`).
- The re-push came from the front-end update `0x16189B0` (deferred reopen: flag `0x1EE8186`, timer `0x1EE8188`), called from the world-state-3 handler (`0x145B080` → `call 0x1618C00` at `0x145B0EC`). With that call NOPed on the client, popping the menu stays popped (depth 0, UI flags 0, not paused) through focus changes.
- `e:LoadCharacter(0, ".../maya_torres.cdf")` on the host's copy of `Dude(1)` makes the joining player visible on the host. The client's copy of the host's `Dude` also had no model; loading `marcus_campbell.cdf` there makes the host player visible on the client. Story-character `.cdf` paths found in memory include `maya_torres`, `marcus_campbell`, `ed_jones`, `lilly_ritter`, `alan_gunderson`, and others.
- Mouse look works on the client after the fix: `SendInput` relative mouse moves with the client window focused rotated the camera (sky and hillside visible).

### Verified on a fresh session (2026-10-21 22:34, `restart_session.sh` → `coop_setup.py` → `walk_test.py 3` → `show_peer.py` → `capture.py`)

- `coop_setup.py` result: client `depth 0, ui 0x0, video_mode 0, peer_model True, host_model True`; host `peer_model True`.
- Walk: client 11.28 m, host copy 11.28 m in 3 s.
- Host screenshot: Marcus (host player), Maya (joining player, placed 4.5 m in front of the host camera from the client side with `SetWorldPos`) and Ed, all visible.
- Client screenshot: third-person game view from Maya by the boat shed, with Marcus (host player) beside her; FPS counter, no title art.
- ~2 min later both processes responding, client exit trap not hit, still walking (1.50 m / 1.33 m in 1 s).

### Unknowns / next

- The 17 NPC actors on the client (`ULNPC*`, community survivors such as Ed) have no character model either, so the joining player sees no survivors. Same fix (load their models) or find what UL's survivor setup does on the host.
- The client has no HUD (front-end update skipped; UL world tick not running), and the single-player systems (missions, community, inventory) are host-only.
- Whether a real mouse/keyboard player on the client can do everything: sprint, crouch, attack, interact were not tested.

## Phase 3r — Control feel, survivors and HUD on the joining copy (2026-10-21 late, Claude Code)

Owner report after Phase 3q: turning was "tanky" (character followed the camera late, in steps), strafing played the walk-forward animation, and the joining player played random animations on the host.

### Observations

- `turn_probe.py` (mouse-x via `SendInput`, sample camera yaw from `gEnv+0x88`+`0x3DEC` and entity yaw from the entity world matrix at `ent+0x4C`): host player follows the camera and settles at 0° offset; client player did **not rotate at all** while idle (0 changes, 102° off after settling).
- Host entity rotation is applied by `CAnimatedCharacter::UpdatePhysicalEntityMovement` (`0x885E50`) → entity `0xBC72F0`, from the movement request built by `CPlayerInput::PreUpdate`. Host and client call `CPlayerMovementController::RequestMovement` (mc vt[1] `0x1689680`, mc = `pl+0x9CC`) from **different** sites: host `0x164F36C`, client `0x164F5BF`.
- The split is `CPlayer` vt+0x384 (`0x14618F0`), true when `pl+0x1114` is set. `pl+0x1114` is UL's "StateMachine On" flag: `0x1463510` turns it on (script property `StateMachine`=`On`), `0x1463550` turns it off. `Dude(1)` is spawned with it **on** on both machines; the host's own `Dude` has it off. So the joining player's input went down the AI path, and the host played state-machine animations on its copy.
- Calling `0x1463550` (thiscall on the CPlayer) on `Dude(1)` on both machines: client player now rotates smoothly (102 changes, max step 0.1°; idle offset still settles about 45° short of the camera, host settles at 0°); right/left input turns the character into the run direction with the camera swinging behind, same as the host; host shows the joining player in a normal running pose.
- Ticking the client player's own StateMachine2 (`0x1635440`, `0x1468FE0`, `0x1636870` from a native per-frame hook) made her play combat-like animations (a knee kick while strafing) and was dropped.
- Survivors: every host actor has a model; on the client none did (17 `ULNPC*` + zombie). Names match across machines, so `sync_models.py` mirrors the host's model path (`ICharacterInstance` → `+0xB8`) onto the same-named client actor via `LoadCharacter`. After that Ed and Marcus are visible from the client and positions of nearby survivors match the host. Distant survivors on the client slowly sink in Z (terrain far away not loaded there?).
- HUD: `0x14705D0(bool)` shows/hides `Class3HUDRadar` + `Class3HUDStats` (loads the movies). The UL per-frame HUD update is `0x1482DF0(float dt)` (called from the world tick at `0x145E2DA`); 120 fault-protected calls on the client were clean. `client_tick.py` now patches `0x145B0EC` (former front-end update call in the world-state-3 handler) to call `0x1482DF0(dt)` each frame. Result: minimap with player arrow on the client. The health/stamina ring and the mission box do not appear; invoking `SetCharacterUiVisible(true)` on `Class3HUDStats` (SUIArguments ctor `0x5CD0A0`, add-bool `0x67A820`, Invoke vt+0xF4) did not show them either. Derived: those come from UL's single-player world (survivor records, missions), which only exists on the host.
- Map updater `0x141E1C0` already ran on the client; the client minimap texture shows a striped "no data" fill instead of terrain.

### Boot regression (unresolved, 23:00–23:20)

- From 23:00 every `session_join_leave.py` / `host_boot_coop.py` run failed: after "Start New Game" (idx 2, same as before) the host process **exits cleanly** about 20 s later (exit trap hit `ExitProcess`, shallow stack in teardown code `0x13E3510` `Singleplayer.SaveRunningID`; log stops after "Allocate render buffer for particles" + `[Error] Runaway thread`; no dump).
- Not the cause (checked): network/bind (`map class3 server` from the console on a fresh host loads in 5 s and binds UDP 64087), host address (unchanged), carousel selection (fresh host shows "Play State of Decay"), window focus, saves (none on disk), profile files (unchanged), disk space, Steam (running).
- The same scripts booted fine at 22:34.
- Because of this, the Phase 3r changes were verified on the live 22:34 session one by one, **not** together on a fresh boot. `coop_setup.py` now applies all of them (not yet run end-to-end).

## Phase 3s — Client HUD renders; boot made reliable; combined re-test (2026-10-22, Claude Code)

### Observations

- Phase 3r's `coop_setup.py` never installed the HUD tick: a scripted edit had not applied, so `0x145B0EC` was left as NOPs. Fixed; it now installs `client_tick.py`.
- `CPlayer`-side gate for `SetHealth`/`SetStamina` in the HUD update `0x1482DF0`: UI-state bit 10 (`0x400`, "gameplay HUD active") in `[0x1EA7FFC]`. Host has it, client did not (set by `0x14742A0`). The client tick now ORs it in each frame.
- Even then **no Flash movie rendered on the client** (host renders all HUD movies each frame on the render thread via `0x1228A10`). Cause: `CFlashUI` stayed in load-time mode (`FlashUI+0x91 = 1`; host 0) because Phase 3q stopped only the renderer's playback. `CFlashUI`'s own stop is `0x654560` (thiscall on the FlashUI interface: renderer vt+0x46C, release load-time elements, `gEnv+0x40` vt+0x64(0), clear `+0x91`). After calling it the client renders `Class3HUDRadar`, `Class3HUDStats`, `Class3HUDNotifications`, `Class3HUDCenterPrompts` every frame, and the health/stamina arcs show around the minimap.
- Mission banner ("Go to the Ranger Station"): `Class3HUDBanners` `MissionBannerStart/Update` (`0x1481890` / `0x1481FA0`), called from `0x15020E0` with localized ids from the mission objective spec (`+0x34`, `+0x38`, resolved by `0x14F1C50`); event-driven only. Missions exist only on the host. Not mirrored: reading host memory works only on one PC, so this needs a network message (same for `sync_models.py`, which today reads the host process).
- Client minimap shows a striped fill instead of terrain. Texture names are in a table at `0x197095C` (`Textures/Gui/Map/map.dds`, `map_large_danforth.dds`, getter `0x1416E70`, no direct callers). Guess, not verified: the stripes are fog of war, whose exploration state is host-side.
- **Boot regression explained and fixed.** Twice now the first boot after a PC start worked and the next ones quit ~20 s after New Game ("[Error] Runaway thread" in the log right after step 1). Step 1 of `boot_to_gameplay.py` ran the DLC input handler on a `CreateRemoteThread` thread and step 2 hijacked the main thread's EIP. `host_boot_safe.py` runs both steps through the sodlib mailbox on the main thread; `session_join_leave.py` now uses it and connects the client with `Proc.console("connect …")` instead of `deferred_connect.py`. Three boots in a row then succeeded (including the "second boot" case that had failed).
- Your Steam copy running blocks test copies from starting (seen 2026-10-22 08:58: test host never got a main thread). Close it before testing.

### Combined re-test, fresh boot (2026-10-22 ~09:35, `restart_session.sh` → `coop_setup.py`)

| Check | Result |
|---|---|
| `coop_setup.py` | client `sm_off True, depth 0, ui 0x400, video_mode 0, peer_model True, host_model True`; host `peer_model True, sm_off True`; 17 survivor/zombie models loaded on client |
| `walk_test.py 3` | client 11.35 m, host copy 11.68 m |
| `turn_probe.py client` | entity yaw changes 89/275 samples, max step 0.2° (smooth); idle offset to camera −102° (host idle offset −90° in the same kind of run, so not client-specific) |
| Client HUD | Radar/Stats/Notifications/CenterPrompts rendered 85/s; `SetHealth` path 85/s; screenshot shows minimap + health/stamina arcs |
| Screenshots | client: Maya with Ed and Marcus visible; host: Maya beside Ed in front of Marcus |
| Second consecutive boot | setup identical; walk client 7.35 m / host 7.07 m |

### Still open

- Mission banner, item slot and minimap terrain on the client (host-side game state; needs networking).
- `sync_models.py` and any mirroring must become network messages before two PCs can play.

## Phase 3t — Held items visible; movement/animation sync (2026-10-22, Claude Code)

Owner report: movement animations felt out of sync with WASD/camera on both machines, the joining player played random animations on the host, and held items (Marcus's weapon, the journal on Tab) were invisible.

### Observations

- **Held items.** Single-player host (`host_boot_safe.py --sp`): Tab puts the journal in Marcus's right hand (`right_item_attachment` holds a `CEntityAttachment`, vt `0x1811FD8`, bound to the journal entity; flags `+0xC4 = 0`). Listen-server host: the same attachment holds a static `notebook.cgf` (`CCGFAttachment`, vt `0x18072E0`) with flags `0x50000` = hidden in main pass, shadow only. The item code (`0x1554560` → `HideAttachment(1)` + `HideInShadow(0)`) does that when the owner is first-person.
- `CPlayer` vt+0x128 `IsThirdPerson` (`0x1564400`): non-client actors → 1; the local player → byte `pl+0xC85`. Single-player: 1. Multiplayer: 0 for the host's own player **and** the client's own player (`gEnv+0x12D` `bMultiplayer` = 1 in both; `gEnv+0x12C` = `bServer`).
- `CPlayer::SetThirdPerson(bool)` = `0x15717F0` (thiscall; notifies listeners at `pl+0xA88`). Calling it with 1 on both local players before items are drawn makes the host's journal bind as an entity attachment with flags 0 — visible, same as single-player. Clearing the flags by hand also showed it.
- Side effect, wanted: idle turning now matches single-player (the body no longer chases the camera while standing; 0 yaw changes in `turn_probe.py`, both machines and SP).
- `CItem::AttachToBack` (`0x154F030`) returns early when `bMultiplayer`; patching that out alone did not help (journal is a hand item).
- **Remote copies snapping.** `move_probe.py` / `follow_probe.py`: the host's copy of the joining player periodically flipped up to ~170° for 3–4 frames during strafes (client body smooth). Source: `CNetPlayerInput` position interpolation (`0x169E9C0`, `+0x58` interpolated point, `+0x94` net target, flags `+0x90/+0x91`); each update can put the target beside the copy and SoD turns the body to its movement → `CAnimatedCharacter::UpdatePhysicalEntityMovement` → `CEntity::SetRotation` (`0xBC72F0`, quat at `entity+0x30`). `turn_clamp.py` hooks `SetRotation` and limits yaw change to N° per call for remote copies only.
- **Host's own player snapping to the camera** (exact 90° steps) in a long session: `CPlayer::ProcessEvent` XFORM (`0x156C730`) copies an entity rotation set with flags `0x4` into the player's quats; those came from UL camera "behavior begin" (`0x1526CD0` → `0x165D610`, aligns the client actor to the camera when spec `+0x2D` is set) because the camera behavior kept switching 1→9→11→24 via the secondary-spec flag `0x1EE1269` (set by `CamOverride` `0x1527210`, cleared by `0x1527320`, both reached from state-machine code). Single-player: behavior stays 1. On two later fresh sessions the switching did not happen at all (with or without the host ticking `Dude(1)`'s state machine), so it was probably left over from that session's many journal/menu tests. Not changed.
- Client's own Tab opens an empty journal ("STRANGER") and plays no journal pose; the remote copy does not show the other player's journal pose either. UL actions and inventory are host-side and not networked.
- One join crashed the client (AV `0x13BE164`, dump `client-install/USER/..._10-27-22.dmp`); the retry joined normally.

### Verified on a fresh session (2026-10-22 ~10:30, `restart_session.sh` → `coop_setup.py`)

`coop_setup.py` now also: `SetThirdPerson(1)` on both local players, `SetActorModel` (Lua) instead of bare `LoadCharacter` for every model (inits animation events, IK, item attachments), and `turn_clamp` 10°/call on remote copies (host: its `Dude(1)`; client: host player + all survivors/zombies).

| Check | Result | Single-player reference |
|---|---|---|
| Host own player, strafe, max yaw step | 5.7° | 3.3–8.5° |
| Client own player, strafe | 5.6° | — |
| Host player seen on client | 11.9° | — |
| Client player seen on host | 20.0° (2 clamped frames between samples; was 135–171°) | — |
| Host copy vs client body, strafe | median 3.9° | — |
| Walk 2 s | client 6.91 m, host copy 6.55 m | 7.4 m |
| Host Tab → hand attachment | `CEntityAttachment`, flags 0 (journal visible) | same |

### Still open

- Inventory/journal/UL actions for the joining player, and their poses on the other machine (needs networking of UL state).
- Not checked: whether the host player's weapon is drawn on the client when equipped.

## Phase 3u — Client HUD over the game's own network (2026-10-22, Claude Code)

First use of real game network messages instead of reading the other process's memory directly. The loose
`Game/scripts/gamerules/singleplayer.lua` (same file on host and client) now declares script RMIs on the
game rules: `Net.Expose{Class=SinglePlayer, ClientMethods={ClCoopBanner={RELIABLE_ORDERED, NO_ATTACH,
STRING, STRING, STRING, BOOL}, ClCoopMapIcons={RELIABLE_ORDERED, NO_ATTACH, STRING}}}`. The host calls
them with `g_gameRules.otherClients:<Method>(hostChannel, ...)` (`local/hud_relay.py`, which runs on the host PC).

### Mission banner
- `MissionBannerStart` `0x1481890` / `MissionBannerUpdate` `0x1481FA0` (cdecl, three localized strings) are only
  wrappers around `Class3HUDBanners.Invoke("MissionBannerStart"|"MissionBannerUpdate", a, b, c)`. The text comes from
  the objective's banner method `0x15020E0` (`this` = objective, spec `+4`, keys `+0x34`/`+0x38`, localizer
  `0x14F1C50`). Active objective getter: `CContentManager::GetActiveObjective` `0x13BDBF0`.
- Client: `Class3HUDBanners` is loaded but hidden (element `+0x4C` = 0; host 1). Lua has the CryEngine
  `UIAction` bind, so the RMI handler does `UIAction.ShowElement("Class3HUDBanners", 0)` +
  `UIAction.CallFunction("Class3HUDBanners", 0, "MissionBannerStart", a, b, c)` with no native code.
- Host: `hud_relay.install()` detours both functions and copies the strings into a buffer plus a sequence number;
  `host_boot_safe.py` installs it before New Game. The relay loop sends each new banner and resends the last one
  when a peer joins.
- Verified: a banner fired on the host ("Talk to Ed" / "Ranger Station") appeared on the client through the RMI.
  The story's own first banner did not fire within this session (it comes after the intro), so it hasn't been seen
  travelling through the relay yet.

### Minimap icons and "terrain"
- The renderer draws `ULHUD_Map_IconData` from 500 × 0x14-byte records at `0x1E78A90`
  (x, y, scale, packed type, color; empty record = 12 zero bytes + `0x00FFFFFF`, 0). Only host code fills them;
  the client had just its own arrow (slot 0).
- "No minimap terrain" was not missing data. The client stood at the level's first spawn point (1345, 3442), an
  empty area, while the host's intro had moved Marcus to the ranger station (~3083, 3346). Fixed in the rules
  script: peers now spawn 2 m beside the host's `Dude` instead of the first spawn point.
- Lua → native bridge: `IConsole::RegisterString` is vtable `+0x08` (name, value, flags, help, cb); the value
  `char*` is at cvar `+0x18`. Clients must use `VF_NOT_NET_SYNCED` (`0x80`); otherwise `SetCVar` logs
  "Server controlled CVar" and does nothing.
- `hud_relay.install_client()` (called by `coop_setup.py`) registers `coop_map_icons` and detours the HUD update
  `0x1482DF0` to hex-decode the cvar into slots 1.. every frame, emptying slots left over from the last frame.
  The relay sends the host's active records whenever they change, and every 5 s regardless.
- Verified: the client minimap shows the host's icons (?, flag, home, survivors) and follows them as they change;
  clearing the cvar removes them; the periodic resend restores them.

### Item slot — not fixed, and not a HUD problem
- The slot is `class3_stats` `ConsumableSet(count, icon)` (HUD ActionScript is extractable:
  `libs/ui/flashassets/scripts/class3hud/*.as` in `gamedata.pak`, LZX via `lzx/PakRead.exe --blob`).
  It is driven by widget `0x1977A80` (`+8` item, `+0x1C` count; refresh `0x163D900`), which reads UL's
  character table `[0x1EA83B8]` (100 × 0x178 records, active player = bit 0 of `+0x15C`).
- On the client that table is entirely empty (UL community simulation runs only on the host), and the joining
  player is not a community member even on the host. The engine inventory replicates fine (both sides list
  `Prop_Meds`, `Prop_Pistol`, ...), but UL's backpack does not exist for the peer. A real fix means making the
  peer a UL character on the host and networking its backpack.

### Also seen, not fixed
- Inside the ranger station the client renders a large translucent green wedge (some area volume) that the host
  does not show. It is not from the icon mirror; the cause is unknown.
- Host-only HUD still missing on the client: the radar's "Home Status" panel (`SetBaseStatus`) and the
  building-interior minimap.

## Phase 3v — Joining player's actions (2026-10-22, owner report: "client can only walk")

- Inputs arrive: `act_probe.py client lmb|<scancode>` logs `CPlayerInput::OnAction` (`0x164F890`). A left click gives
  `attack1`, `ul_melee_attack`, `ul_shoot`; Space gives `jump`/`jump_hold`. They are the same on the host.
- UL actions run in the per-actor UL state machine. Manager `0x1636A40` (host world tick) walks the pool `0x1F35518`
  (0x92C-byte records, actor at `+8`) plus an extra list `0x1F354CC` (count `0x1F354C4`). Per record:
  `0x1635440(rec, actor)`, `actor->0x1468FE0()`, `0x1636870(rec)`. The local joining player is in the extra list on
  the client (the host also ticks its own copy of `Dude(1)` through its extra list). A client never runs the
  manager.
- `player_sm.py` (now in `coop_setup.py`) detours `CPlayer::Update` (`0x1575E80`) and runs the three passes for the
  client's own player only. Result on the client: left click plays the unarmed combo (punch, kick); Space while
  moving dodges and uses stamina; plain strafing stays a normal run (the "knee kick while strafing" from Phase 3r
  did not come back now that StateMachine Off, SetActorModel and third person are in place).
- Not networked: the host's copy of the joining player stays idle during her attacks, and hits are client-side only,
  so they don't damage host-simulated zombies. Next step: forward the UL action to the host (client → server RMI) and
  feed it into the host's state machine for `Dude(1)`.

## Phase 3w — Owner's four reports: journal mouse, actions on the host, prompts, journal data (2026-10-22 afternoon)

### Boot fix: title carousel
- Three joins in a row failed: the client got game-rules class "TeamRandomSoundVolume" and disconnected. The host had
  loaded **C3_beltway** (a DLC level) because the title carousel remembered a DLC. Selection = `0x1416C60()` (decoded
  from `[0x197094C]`), setter `0x1416CB0(value)`: 0 = State of Decay, 1/2 = DLCs. `host_boot_safe.py` now sets 0
  before pressing Play. (A game-rules `Net.Expose` ServerMethods entry was blamed first and removed; it was not the
  cause, and it hasn't been retried.)

### 3) Mouse "locked" in the journal — fixed
- The client's OnAction UI-gate bypass (`0x164FA82`) made the player camera eat mouse input while the journal was open.
  Gate `0x146EF10`: blocks on `[0x1EA7FFC] & 0xD`, `& 0xC01A00`, byte `0x1EA70D9` (set while the journal is open, on
  both machines) or byte `0x1EA710A`. Once the title menu is popped the client's gate inputs are clear, so
  `coop_setup.py` restores the original call. Verified: walking still works (6.3 m in 4 s), journal mouse moves the
  cursor and a click switches pages, camera stays put (2.3° during open/close).

### 1) Joining player's actions on the host — transport works, animation does not yet
- Client → host transport: chat message `"coop:<hex>"` via `g_gameRules.game:SendChatMessage(2, g_localActorId,
  NULL_ENTITY, ...)` (native RMI `SvRequestChatMessage`). Host hook on `CGameRules::SendChatMessage` (`0x15D10D0`,
  thiscall, `ret 0x10`) decodes and swallows them (`peer_input.py`).
- Payload: the bytes UL pass 1 reads from `IPlayerInput` (`+0xD8..+0x117`: sticks `+0xD8..+0xE4`, buttons
  `+0xF5..+0x117`). They are one-frame flags, so the client sends them *before* pass 1 (`player_sm.py`).
- Host: pass 1 for the joining player runs with `actor+0xF34` pointed at a stand-in buffer. Verified: host input block
  `+0x1250` shows the same button bits as the client (`0x1200000` press, `0x2400000` hold).
- But the host's state machine for that player sits in state `0xC1` (client idles in `0x9C`) and ignores attacks.
  Writing `0x009C009C` to record `+0xC` makes it follow (`0x9C → 0xA1 → 0xA7`, same as the client), but the host then
  starts different animations (`ISkeletonAnim` vt+0x28 `StartAnimationById` from `0x820C20`, ids `0x85`/`0x95`)
  while the client starts `0x11A` by name (vt+0x24 via `0x820BC0`). On screen the host's copy does not punch. Open.
- Damage to host zombies from the client's local attack: untested (no zombies near the start; `zm_spawn N` spawned
  none in view).

### 2) and 4) Journal data and interaction prompts — same root cause, not fixed
- Host prompt: `Class3HUDCenterPrompts.ShowUseButton` every frame via `0x1478720`, which checks UL item/character
  handles (`0x14A5A20`, `0x14A5AE0`) and the backpack (`0x14B0420`).
- The journal character page, the item slot and the prompts all read UL's character table `[0x1EA83B8]`
  (100 × 0x178), which is empty on a client. The records hold heap pointers (`+0xE0`, `+0xE4`, e.g. into a definitions
  array), so copying the table to the client needs pointer translation, plus the item/backpack pools behind it.
- Design question for the owner: SoD has no "player 2"; the natural co-op model is that the joining player takes over
  a community survivor (switching survivors is how SoD works). Then the host's data for that survivor is what the
  client's journal, item slot and prompts should show, and interactions (greet, loot) must run on the host.

### Toward "player 2 = a community survivor" (owner's choice, 2026-10-22)
- Character definitions: `[0x1EE25E4]`, 146 × 0x34, `+8` = model path (0 = Marcus, 142 = Ed Jones, 145 = Maya Torres).
- Character records `[0x1EA83B8]` (100 × 0x178): `+0xF0` word = definition index, `+0x15C` flags (bit 0 = known,
  `0x8` = community member, `0x20000` seen on Ed = following, `0xF` on the player's record), `+0xE0` → link object for
  the survivor's NPC body (vt `0x17D36A8`, `+8` entity id, `+0xC` entity), `+0xE4` → per-character data.
- NPC body entity `ULNPCnn` belongs to record `nn`. The current player's record is `[0x1EE2638]` (record 0, Marcus);
  its NPC body `ULNPC00` is parked at (0,0,0). Community at the start: records 1 (Ed, following), 14, 15, 16.
- `fm_nextMember`/`fm_prevMember` (`0x16555A0`/`0x16555E0`) only re-skin the current record to another definition
  (`0x15472D0`); the real survivor switch is elsewhere (`SwitchChar_*` strings).
- UL's notion of "the player" is that global record plus the engine's single client actor
  (`GetIGameFramework()->GetClientActor()`, vt+0x114), used by the HUD, journal, backpack (`0x14AF5E0`) and prompts.
  `IConsole::AddCommand` is vt+0x7C.
- Plan: (1) host binds a community record to `Dude(1)` (model from the definition, park its `ULNPCnn` body);
  (2) mirror the community records (with `+0xE0`/`+0xE4` pointer translation) and the backpack/item pools to the
  client, and point the client's `[0x1EE2638]` at player 2's record, so the client's own journal, item slot and
  prompt code show that survivor; (3) send player 2's interactions (greet, loot, use item) to the host to run there.

## Phase 3x - Joining player's attacks animate on the host (2026-10-22 evening)

Fixed and verified on two fresh boots (`restart_session.sh` -> `coop_setup.py`): a left click on the client plays the
push kick on the host's copy of the joining player (same animation id 0x11A as on the client; a second click gives
the combo, PushKick -> PushKick_Left, 0x11A/0x112); Space while moving plays the jump/land states there too.

### Why it didn't animate
- State names: state-machine type table at record `+4` (0x68-byte entries: "Player", "Civilian", "Zombie", ...);
  `+0x58` offsets, `+0x5C` string pool, `+0x40` count. 0x17 `GraphDriven.Observer` (normal idle), 0x9C
  `MachineDriven.Combat.Idle.Unarmed.Normal`, 0xA1 `...Utility.Logic.Evaluate`, 0xA7 `...Stand.PushKick`,
  0xA8 `...PushKick_Left`, 0xC1 `MachineDriven.Dodge.Roll.Normal`.
- The host's copy sat in `Dodge.Roll.Normal`, reached from garbage input before forwarding existed. MachineDriven
  states end when their animation finishes, and the animation graph only follows the UL state machine while the
  graph input "StateMachine" = "On" (set by `0x1463510`, the same function as the script property; actor
  `+0x1114`). The state machine sets it itself when it enters MachineDriven states from `GraphDriven.Observer`
  (wrapper SetInput-by-name, `CAnimationGraphStates` vt27 `0x88BB90`; seen: "StateMachine", "AimTurn", "TurnDir",
  "CarryWeapon"). Stuck in a MachineDriven state with it off, the copy never animated and never left.
- Fix: `peer_input.install` resets the copy to `GraphDriven.Observer` (`peer_input.py --reset` does it by hand).
- Input edges: the client's "pressed" flags last one frame and several chat messages can arrive in one host frame,
  so the host queues them (16 entries) and applies one per pass-1 tick.
- Not needed in the end: forcing graph input 0x1D (0 for each machine's local player, 1 for the other player) to 0.

### Stamina side effect - fixed
- The joining player's moves drained the host survivor's stamina: `CPlayer` stamina setter (vt+0x78) stores
  `actor+0x1678` and, if vt+0x130 ("is a player", true for both players), pushes it into the *current player's* UL
  record via `0x1535490` (call at `0x146209B`). `peer_input` retargets that call so only the host's own player
  pushes. Verified: the host survivor's stamina stays at 115 through the joining player's attacks; hers is on her
  actor.

### Tools
- `ag_probe.py` (AnimationGraph SetInput on both machines), `act_probe.py` (actions + `lmb`/`rmb`), `tab_probe.py`,
  `journal_shot.py`. CryEngine bits: `CAnimationGraphState` vt `0x17DADE0` (SetInput overloads vt1..vt3);
  `ISkeletonAnim` = character vt+0x10, StartAnimation by name vt+0x24 / by id vt+0x28 (`0xAEFF10`).

### Still open
- Damage to host-simulated zombies from the joining player's hits (no zombies near the start to test).
- Health may have the same "current player record" coupling as stamina (not tested).
- Items 2 and 4 (journal data, interaction prompts): see Phase 3w plan.

### Both directions (2026-10-22 late)
- Owner: the host saw the client's attacks but not the other way round. The client runs no state machine for its
  copy of the host's player and never received the host's input.
- `player_sm.py host|client`: on both machines the local player's input bytes go out as chat (client tag `coop:`,
  host tag `cooh:`; the host's catcher only takes `coop:`, so its own messages go out to clients as normal
  `ClChatMessage`). On the client it also ticks its copy of the host's player (record in the main pool, so
  `record_for` now searches the pool too).
- `peer_input.py host|client`: the client catches `cooh:` in the `ClChatMessage` handler (game-rules vt83
  `0x15C2A30`, args type/source/target/msg/?, `ret 0x14`) and feeds the same queue and pass-1 swap into its copy of
  the host's player, reset to `GraphDriven.Observer` at install. The stamina patch stays host-only.
- Verified on a fresh boot: client attack -> host copy PushKick, 0x11A. Host attack -> the client's copy of
  Marcus goes Observer -> Evaluate -> PushKick and starts the same animations as the host's own Marcus
  (0x10D, 0x105 for a two-hit combo).
- Test-harness note: `SendInput` mouse clicks go to the window under the cursor; move the cursor onto the target
  game window first (keyboard input follows focus).

## Phase 3y - Zombie damage and health (2026-10-22 late evening)

### Health: the joining player's damage hurt the host survivor - fixed
- Test: on the host, `Dude(1).actor:SetHealth(55)` also set Marcus to 55 (UL player record `+0xA4`). Same coupling
  as stamina: the CPlayer health setter (`0x1461E40`) pushes into the *current player record* through `0x1544950`
  (call at `0x1461F09`), gated only by "is a player". `peer_input` now patches both pushes (`RECORD_PUSHES`) so only
  the host's own player pushes. Verified: Maya 55 -> Marcus stays 85 (record 85); Marcus 70 -> record 70; Maya's
  health still replicates to the client.

### Zombies in a co-op session
- No zombies spawn in the home safe zone. `zm_spawn N` (spawn around the player) works only away from home and
  only while the zombie pool (~6 entities) has free slots. Corpses aren't recycled; `zm_killAll 1` clears them.
  `zm_spawnPathZombies N` reliably spawns "FreshGrunt"s about 70 m away that walk in. Class `Grunt` spawned from Lua
  has no model or hit volume (useless as a target).
- Entity ids replicate with the same index but a different salt (high 16 bits); pooled zombies get renamed and the
  name isn't replicated. Match zombies across machines by `id & 0xFFFF`.
- The zombie manager keeps zombies active only around the host's player: walking the host away pools them
  (hidden on the host) while the client still shows them standing (hidden state isn't replicated).
- `IsHidden` also flickers with visibility culling, so it isn't an "inactive" test; use the zombie's UL state
  (`zstate.py`: records whose type name is Zombie / Freak_*, `0xFFFF` = no active state).
- Teleporting a zombie with `SetWorldPos` resets its state machine; move the players to zombies instead.
- Teleporting the joining player into an area the client hasn't streamed drops her through the ground (z = -14876);
  `zombie_site.py` re-places her until the client's terrain height matches.

### Melee results
- Host player: hits register (zombies knocked down and killed). Ed (NPC) also kills zombies.
- Zombies never target the joining player; they attack only the host's player (Marcus was downed repeatedly while
  Maya stood next to them untouched).
- The joining player's hits do not register. On the host her copy plays the same push kick (state PushKick,
  input forwarded) straight through a zombie that is busy striking Marcus, and the zombie shows no hit reaction
  (its state keeps cycling `Combat.Fast.Strike.TargetDown.*`). Her combat hit list (`blackboard +0x1C20`) is only
  cleared each frame (`0x1649BB5`), never filled. `0x164A120` adds hits to a *victim's* list from the got-hit
  handlers `0x1466160` / `0x1467720`. So no hit event is generated for her attacks at all.
- Both symptoms (not a target, hits not generated) point to her not being a UL combatant: she has no UL character
  record (the same gap as the journal and interaction prompts). Next: find where combat collision decides which
  actors' attack volumes are live (`CombatCollisionMode` action, parser `0x170D4D0`; runtime still to find), or
  give her a UL character record (Phase 3w plan).
- Tools: `health_probe.py`, `zombie_site.py`, `zombie_duel.py`/`zombie_duel2.py` (by id), `zstate.py`, `zhit*.py`,
  `dump_exc.py` (minidump exception + stack).

### Crashes seen
- 16:02 host crash on a worker thread (`0x1128072`, stack through `0x6544DF`/`0x654622` FlashUI) during zombie
  console experiments (`zm_density 1` debug display was on). Game time froze while rendering continued.
- 16:11 host crash during `coop_setup` in the host's own state-machine actions for the joining player (null + 0xE8
  write at `0x1706C5F`), before `peer_input` was installed: her copy was running on garbage input. `coop_setup.py`
  now installs `peer_input` on the host first.

### Melee hit generation (2026-10-22, continued)
- `0x1640430` is the UL combat-collision physics callback (dispatched from the physics event queue
  `0x160A8E0`/`0x160A9D0`). Event `+0xC` = volume object (owner actor at `+0x44`), `+8` = the other physical entity,
  `+0x18` = active/strength (callback returns early when <= 0). It resolves the victim actor (`0x1462330`) and calls
  the got-hit handler `0x1466160`, which adds the hit to the victim's list (`0x164A120`).
- `combat_events.py` logs these per owner. With three zombies striking a downed Marcus and Maya swinging within ~1 m
  (host copy in PushKick), only the zombies' volumes report a real "other" (Marcus's physics entity); Maya's
  volumes only ever report the static-world sentinel `0x1A3E398`. Her always-on body volumes sit at her body
  position (head/feet); no attack-window volume of hers ever touches a zombie.
- Current explanation: the joining player isn't a UL combatant (no UL character record, no faction), so zombies
  neither target her nor register her attacks. The fix is the "player 2 = community survivor" work (Phase 3w), not
  a hook in the hit path.

### Melee, continued (2026-10-22 night) - player hit path still not found
- The physics "combat collision" volumes (`0x1640430` callback) are body capsules; neither player's swing creates
  new volumes, and Marcus's volumes also report only the static sentinel while he lands punches, so they are not
  the player melee path.
- `0x1707730(ctx, params)` is a state-machine "apply hit to blackboard target" action: target = `[bb + 0x48 +
  slot*4]`, queued on the attacker (`0x1468C40`: actor `+0x1324` count, `+0x12B4`/`+0x132C` 0x74-byte entries) and
  delivered in pass 2 (`0x1469EF6`) to the victim's got-hit handler. Captured live: every call came from zombies
  (slot 1 = the host player's entity `0x7777`), i.e. this is the zombies' attack, not the players'.
- Blackboard slot 2 of the host player gets the id of the zombie engaging him (e.g. `0xFE4D`); the joining player's
  slots stay 0 on both machines (no zombie engages her). Forcing that id into her host slot 2 did not make her swings
  land.
- `0x140B430` (grid query that delivers hits) is only called from the zombie manager (`0x13B0860` <- `0x13BB3A0`),
  so it is the zombies' melee as well.
- Testing obstacles: each fresh boot gives one short round of zombies (zm_spawn near the players after
  `zombie_site.py`); corpses are not recycled, `zm_playerInvisible 1` and `zm_killAll` leave zombies in state
  `0xFFFF`; Ed must be moved away and hidden or he kills them; zombies keep downing the host player. One host crash
  (19:33, empty dump) happened during these tests.
- Next: capture a clean host-player punch on a free-standing zombie (fresh boot, zombie approaching, punch before it
  grabs) with stacks on the zombie's got-hit entry (`0x1466160` with this = zombie actor) to find the player melee
  query, then see why it doesn't run or find targets for the joining player.

## Phase 3z - co-op fixes: client melee, zombie models, remote-player effects, shared pause (2026-10-23)

### Joining player's hits damage zombies (peer_melee.py, host)
- Player melee hits come from the UL action CombatDamage (action type 0x1d, `0x1707730(ctx{bb, rec}, params)`):
  target = blackboard slot 1, hit queued on the attacker (`[[ctx+4]+8]`), delivered in pass 2. Its condition tree
  (composite type 17, evaluated into flags `0x1F36D60`) needs a target in slot 1 (type 40), a time window on
  blackboard `+0x1340` (= seconds in state; push kick 0.433-0.5) and more. Slot 1 is copied from slots 2/3/4,
  which the player's perception queries fill - never for the joining player on the host, so the action never fired.
- Action handler table `0x1F378A8` (0x24 bytes/type: +0 begin, +4 end, +8 fire, +0xC update, +0x20 name).
  Dispatcher `0x17068A5`: ecx = entry (+0 condition id, +2 type); after pushad `[esp]` = params, `[esp+0x5C]` = bb,
  `[esp+0x60]` = record.
- Fix: a dispatcher hook remembers the params of her current state's CombatDamage entry; a tick after her pass 1
  (peer_input `data+0x14`) fires CombatDamage once per attack state at 0.43 s on the nearest live zombie within
  1.6 m. A kneeling/downed target ("...Down...") gets the unarmed DownKill params (found by tag string).
  Damage/kills need a UL character: the hit is credited to the host's player (fake record, bb `+0xC` owner id
  swapped during the call). Verified: reactions, knee-downs, and kills (hp 0 on host and client).
- Without the credit swap her hits staggered zombies but never killed (hp stayed 100).

### Zombies invisible on the client
- Host zombies are `characters/male|female/zombie/skeleton/skeleton.chr` + attachments built by the zombie manager
  (not run on clients); client zombies had no character at all. hud_relay.py now sends `index=model;...`
  (RMI `ClCoopZombieModels`, both singleplayer.lua copies), skeleton mapped to `zombie/skin/skin.cdf` (naked but
  visible; clothing attachments not replicated). hud_relay.py must run on the host.

### Host's downed/low-health tint on the client
- The client ticks a copy of the host player's state machine (player_sm); its PlayerDowned/Vulnerable/Camera/
  ScreenShake/TimeScale/MusicCue/ForceFeedback actions applied to the client's own screen (tint + "press SPC to
  get up"). remote_fx.py wraps those handlers to do nothing for the remote player's record (both machines).
  Verified: host tinted with tip, client normal.
- Also: two more health setters push into the current player record (`0x1461F55`, `0x1461FA0`); RECORD_PUSHES now
  patches all three on both sides (client allows only its own player).

### Shared pause (pause_sync.py)
- MenuStackPush/Pop call IGameFramework::PauseGame (`0x1614261`/`0x1614388`). Stubs record local wish, send it as
  chat `pauc:`/`pauh:` (caught by peer_input's chat hook, handler `data+0x18`) and apply PauseGame(local && remote).
  Verified with game time: one menu -> both worlds run; both menus -> both stop; either closes -> both run.

### Crash guard
- Client crashes 20:02/20:15/20:29 at `0x171676C` (null from game->vt+0x38->vt+0x114 in a UL action during player_sm
  ticks): crash_guards.py takes the not-equal branch on null. Calling the slot setter `0x1648C90` through the
  mailbox killed the host twice (no dump) - don't.

### Testing notes
- Ed kills spawned zombies: maya_hit_test.park_ed (move + Hide + Activate(0)). `zm_spawn` after zombie_site may
  need repeating; Lua-spawned Grunts have no model/hit volume.
- New tools: peer_melee.py, pause_sync.py, remote_fx.py, crash_guards.py, hit_action.py, hit_cond*.py,
  cond_tree.py, bb_sets.py, disp_probe.py, path_hit.py, maya_hit_test.py, punt_params.py, pause_test.py.

## Phase 4a - zombies vs. both players (2026-10-23)

### Zombies target the joining player (zombie_targets.py, host)
- UL entity queries: 0x200 slots x 0x108 at 0x1EC00B8 (owner +0x24, flags +0x2C, shape +0x90, origin matrix
  translation +0x44/+0x54/+0x64, candidates +0xDC (8), count +0x104, valid masks +0xFC/+0x100, pick +0x28).
  Category lists: bit 1 players 0x1EC0060 (capacity 1: host player only), bit 2 NPCs 0x1EC0080 (cap 64), bit 4
  zombie-manager grid (zombies only). List struct: ids +0, positions +4, actors +0xC, valid +0x14, inputs +0x18,
  capacity +0x1C; rebuilt each frame by 0x1525610. Zombie target queries use bits 1|2; picker 0x1525A00 takes
  the lowest valid candidate (players before NPCs); bit 0x80 = line-of-sight post-filter (0x15235B0).
- Fix: she is kept in the NPC list inputs, and the picker swaps her ahead of the host's player when she is
  closer to the querying zombie. Verified: zombies near her pick her (slot 1 = her id).
- Zombie strikes on her never passed the zombies' CombatDamage conditions (a "Hero" contact event in the zombie's
  blackboard list +0x1538 - 0xB0-byte events, name +0xA0, arc +0xA8 - plus counter/grab conditions). A pass-3 hook
  (0x1636870) for zombies targeting her adds the "Hero" contact and, once per ".Strike." state (>= 0.35 s, within
  1.8 m), runs the zombie's CombatDamage on her with the zombies' strike params ("Zombie, DamageLight"/torso).
  Verified: her health 100 -> 57 in 20 s on host and client, host copy plays hit reactions, Marcus unaffected.
- Open: the zombie manager only keeps zombies live around the host's player (moving Marcus 40 m away despawned
  the zombies near her).

### Kill counts (kill_counts.py)
- Kills only update globals for "the player": zombie manager kill handler 0x13A6300(this, slot, counted) adds 1 to
  Zombies.Killed (0x12000020; AddVar 0x13F6970(id, delta), GetVar 0x13F47B0). No per-attacker counts exist.
- Host: a counted kill of the zombie she last hit (peer_melee records target/tick; slot position 0x1E54D00 within
  2 m, within 300 ticks) is taken off the host's Zombies.Killed and sent as chat "kilh:NNNN"; the client handler
  (peer_input chat hook, data+0x1C) sets the client's own Zombies.Killed. Verified: her kill -> client 1, host
  unchanged.

### Weapons
- CryEngine ItemSystem.GiveItem/SelectItemByName gives a Hammer item, but UL combat states stay "Unarmed": the
  SelectMeleeWeapon action (0x1703980) reads the melee weapon of a UL character: actor vt+0x130 (player) ->
  current player character 0x14B0420, else the character by entity id 0x14AF690; weapon item 0x14A52C0, type 5 =
  melee. On the host her copy therefore uses the host survivor's weapon; on the client she has no UL character.
  Normal weapon use for her needs her own UL character (community survivor), not done.

### Crash guard 2
- 0x170AD70: collision-volume action passed null+0xE8 when the actor isn't in its per-actor table (host crashes
  10-22 16:11, 10-23 09:15 after forcing Marcus's state): crash_guards.collision_guard skips the call.

### Weapons for both players (weapon_relay.py) - 2026-10-23 afternoon
- UL combat picks weapon moves from blackboard +0x1454 = melee weapon name hash (0x59EBF0, thiscall ret 4; e.g.
  Hammer 0xB964FFCF), set in pass 1 from a UL character (+0x1450 = ranged). Conditions 0x16FE3C0 compare it with
  every melee name. UL's select-item actions (0x7A/0x7B/0x7C) re-select the UL character's item (Fists for her).
- Fix: her weapon = the melee item she holds (engine inventory; `weapon_relay.py give Hammer`): client Lua timer
  -> cvar coop_my_weapon (client post-pass-1 override via player_sm) and chat "wpnc:<class>" (host: peer_melee
  override). Host player's hash -> chat "wpnh:<hex>" -> client's copy of him. Select actions are no-ops for her
  (host) and for both players on the client. Verified: BluntLight swings on both machines, hammer hits with
  "BluntLight, Mid, DownHead, RandomKill" params, 2 kills counted for her.
- Zombie activation: Zombies.ActivationDist/DeactivationDist (0x12000001/0x12000010, SetVar 0x13F4A10) raised to
  60/120 did not keep zombies alive near her with the host's player 40 m away; deactivation comes from behavior
  exits (0x13AF520 <- 0x138C921). Still open.
- coop_setup.py now starts hud_relay.py itself.

### Zombies around both players, kill banners, clothed zombies (2026-10-23 late afternoon)
- zm_focus.py: the zombie manager update (0x13BB3A0) keeps zombies live around one focus (client actor position:
  edi/ebx/[esp+0x214] at 0x13BB784). Focus = midpoint of the two players while they are within 60 m. Verified:
  host player 40 m away, 6-7 zombies stay live around her and keep attacking. At 80 m apart (midpoint) nothing
  stays live; alternating the focus actor per frame deactivates everything.
- kill_counts.py now shows each player's count on their own HUD after each kill ("Zombies killed: N", the
  mission-banner element). The game itself shows kill counts nowhere (journal People/Skills pages have none).
- hud_relay.py sends clothed zombie bodies (zombie/body/*.cdf, picked by entity index per gender); the host's
  exact outfit per zombie isn't read yet.
- coop_setup gives the joining player a starter melee weapon (weapon_relay.STARTER = Hammer) and starts
  hud_relay.py; sodlib Proc.call is serialized across processes (named mutex coop_mailbox_<pid>).
- The host's downed tint + "press SPC" tip seen with Marcus standing was Marcus's own state machine stuck in
  Stunned.Down after test teleports (Space gets him up) - not a co-op leak.

### Zombie focus list, normal weapon equip (2026-10-23 evening)
- The zombie manager keeps a list of focus points (count 0x1E54534, max 20; positions 0x1E6FF90, forwards
  0x1E70080, flags 0x1E51F64/0x1E51F50/0x1E51F28): entry 0 = client actor, then friendly NPCs near the camera.
  zm_focus.py (replaces the midpoint version) appends the joining player at the end of that list (hook on the
  call 0x13BBBAA -> 0x14C72A0). Verified on a fresh boot: players 100 m apart, zombies around her stay live, 4 of 5
  target her, she takes damage and kills with her hammer (counted for her). zm_spawn still spawns only around the
  host's player; natural spawns didn't occur in a minute around either player at fresh sites.
- Host player's weapon through normal UI: journal People page -> click the backpack weapon -> Equip (Q) sets the
  UL character's melee slot (character +0xE0 equipment: +4 container (+4 count, +0x10 item handles), +0xD selected
  melee slot; item handle 0x5xxxxxxx = melee def index, defs at [0x1EA8310] 0x70 each). Verified: Wooden Branch
  -> BluntLight swings, hit params "BluntLight, High, RandomKill", kill counted; client copy gets the hash.
- weapon_relay host message is now "wpnh:<hash><0|1 downed>"; the client resets its copy of the host's player to
  idle when he is up but the copy still lies down (it missed the get-up input).

### Weapon cycling for the joining player, kill attribution, zombie range (2026-10-23 night)
- Community storage records (key 0x5F019A59, type 0x88, item handle, count, -1, durability) hold the community's
  items; melee defs: [0x1EA8310] + 0x70*i (+0 name hash, +0x54 item class, class +8 = class name).
- weapon_relay: her middle mouse button (client native GetAsyncKeyState while the game window is focused) sends
  chat "wpnc:next"; the host counts it, hud_relay.py calls weapon_relay.cycle(): next melee class from community
  storage (non-destructive, plus the starter Hammer), ItemSystem.GiveItem on the host and RMI ClCoopSelectItem to
  her machine, which selects it; the existing relay then puts the weapon in both state machines. Verified: Hammer
  <-> 2x4_Bracket on both machines, BluntLight swings with the held weapon.
- (R is the radio key - it opened Radio Assistance on the client during key probing.)
- kill_counts KILL_DIST 6 m (blunt hits knock zombies back several metres before the kill registers).
- Zombie movement range: zombies walk to her with the host's player 20/45/70 m away (approach partly at 70 m);
  at ~100 m they stay live and attack if adjacent but don't move - movement/animation LOD follows the host's
  camera. Still open beyond ~70 m.
- Per-zombie outfits: host zombie characters have no attachments (attachment manager at character +0x710 is
  empty) - parts are merged natively at activation; the user accepted per-zombie bodies (no exact match).

### Zombies around her at any distance: cull and population (2026-10-23, after the night session)
- The ~100 m "zombies don't move" result above came from long test sessions (many teleports, earlier stubs). On
  fresh boots zombies chase her with the host's player 100-600 m away (`local/zm_freeze_test.py`).
- Cull: zombie slots farther than 175 m (squared 30625 at 0x18C7998) from the camera are freed at 0x13AD9C6 (slot
  position at [esp+0x34]). `zm_focus.install_cull` compares the nearer of camera and her instead. Before: the
  host's player 250 m away deleted every zombie around her (52 slots -> 9). After: they stay (and chase her).
- Spawning: the population pass 0x13B8C10 (thiscall, float dt, ret 4, called at 0x13BBF73) fills 200 m map cells
  within 150 m of the camera (system view camera [gEnv] vt+0x18C and 3D engine camera 0x195E090, translation at
  +0xC/+0x1C/+0x2C; sweep every ~12 s via timer manager +0x1C). The spawn routine 0x13B3A70 centres its box on the
  client actor (0x13B3BD3: camera only when there is none). `zm_focus.install_population` runs the pass a second
  time with both cameras moved to her, the sweep timer restored and the actor check skipped, when she is 80-300 m
  from the camera. A/B at 300 m apart (`local/zm_pop_ab.py`): her pass off -> 0 zombies around her in 40 s; on ->
  9-12 within 150 m. Beyond 300 m it is off: at 550 m the host died silently right after her pass spawned there
  (the level only streams around the host's camera).
- Spawn density is per area: x ~2500-2850 (home safe zone) and ~3700, y ~3230-3350 have none; ~3150,3350 fills
  within seconds. zm_spawn is a cvar consumed by the update (0x13BD34D); it fails in empty areas.
- Debugging traps: a 5-byte hook inside the spawn routine (0x13B4921) killed the host because 0x13B4923 is also a
  branch target - use `Proc.count_exec` (hardware execute breakpoints, nothing patched) inside functions.
  `Proc.hook_stack(..., this=)` filters by ecx. A "CryEngine Error: Invalid parameter detected in CRT function"
  dialog (radar HUD frame on the stack) froze one long session; not seen on fresh boots.
- The host log fills with "Entity 130907 is not part of the community" (her entity id, every frame); harmless
  so far, not fixed.
- Final check (fresh boot + coop_setup only, then a ~1 h session on the same boot): kill_verify her kills +2 /
  client Zombies.Killed +2 / host +0; both machines list the same zombies near her (client also keeps some
  stale records); client screenshot shows her knocked down by a zombie, health 1 on both machines; the host's
  player swings his own Branch (BluntLight) and takes hits, 3 kills counted for him (host Zombies.Killed
  12 -> 15, hers unchanged). Chasing her with him far away: reliable 100-150 m in the long session; at 300 m it
  worked on fresh boots (also 400/600 m) but chasers stood still (0.0 m) late in the long session. Silent host
  deaths (no dump, exit trap not hit, no Windows error record) still happen occasionally; cause unknown.
- Both games crashed at 18:03:48 in the same render-thread instruction (0x11F9775, shader/render-target code) the
  moment Windows left Modern Standby (System log Kernel-Power 507, "Input Mouse"). Earlier freezes/deaths line up
  with standby periods too (15:10-15:18, 16:21-16:32, 17:44-18:03): unattended tests let the PC sleep, which
  throttles the games and then kills the renderers on wake. Long tests now keep the PC awake
  (zm_soak.keep_awake: SetThreadExecutionState CONTINUOUS|SYSTEM|DISPLAY while the script runs).
- coop_setup sets es_MaxPhysDist / es_MaxPhysDistInvisible to 2000 on the host (default 300 / 500), so entities
  around her keep physics (and walk) when she is far from his camera.
- Host hang during a 30-minute soak (PC kept awake): the main thread spun in the game's own lookup of
  `Class3HUDRadar` (UL HUD update 0x1652BFF -> 0x1475100 GetUIElement -> hash-map find 0x1127100): the bucket
  node for that name pointed to itself. An earlier long session stopped on "Invalid parameter detected in CRT
  function" in the same lookup. The only co-op code that touched the UI from outside the game's own flow was the
  kill-count banner (UIAction.ShowElement/CallFunction run from inside the native kill hook on the host and the
  chat hook on the client). kill_counts.SHOW_BANNERS is now False: counts are still kept and synced, not shown.
  Re-verification soak pending.
