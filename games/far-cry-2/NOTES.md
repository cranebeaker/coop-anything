# Far Cry 2: working notes

Started 2026-10-19. Phase: 0 triage, starting 1 research and 2 workspace.

## Identity

- Title: Far Cry 2 (2008), Ubisoft Montreal. Store: Steam, app id 19900.
- Version: 1.03 (from the shipped `PatchNotes.txt`). Steam build id: 52164.
- Engine: Dunia (a CryEngine 1 derivative). Architecture: x86, 32-bit.
- Main files (SHA-256):
  - `bin\FarCry2.exe`, 28296 bytes (launcher stub): `7d018d049974972fe71ec38ec446a141e9011a846b71dad6fe4242bf9353fd1d`
  - `bin\Dunia.dll`, 20183176 bytes (engine): `6353936a54aa841350bb30ff005727859cdef1aa10c209209b220b399e862765`
  - `bin\FC2.dll`, 389768 bytes (game code): `94486f716554c23f9673409b0a4b36121a1ad64d11029fed1aebb1fe71494a7b`
- OS: Windows 10 Pro 10.0.19045.

## Observations (checked on the install or a source)

- `detect_engine` returned no match for the install folder (187 files scanned).
- The install ships `FC2ServerLauncher.exe` (dedicated server) and a readme for it, so
  the engine has multiplayer netcode. Source: `bin\FC2ServerLauncher_ReadMe.txt`.
- A PunkBuster installer is shipped in `installers\PunkBuster`.
- Data lives in `Data_Win32\*.dat/.fat` archives (common, patch, shadersobj, sound).
- The directory has no Far Cry 2 entry. It has two Far Cry 1 (CryEngine 1) co-op mods.
- A 2013 Steam thread says the game is single player only, with no co-op outside
  multiplayer: https://steamcommunity.com/app/19900/discussions/0/864973123475897875
  (a user answer, not an official source).
- FCSE (Far Cry 2 Engine Extension) exists on Nexus Mods and patches `Dunia.dll`
  behaviour: https://www.nexusmods.com/farcry2/mods/368 . The page returned HTTP 403 to
  WebFetch, so the details come from a search summary only.

- FC2MPPatcher (GPL-2.0, 463 commits, no dates seen) patches game files to restore online
  multiplayer after Ubisoft shut its services down on 2021-06-01. Needs game version 1.03;
  Retail, Steam, Uplay and GOG tested. Accounts go through longweep.net. No co-op or
  campaign mention. https://github.com/halvors/FC2MPPatcher (README via WebFetch only).
- FCSE per search summary: a launcher by Nexus author "bajuh" that loads DLL plugins from
  `bin\plugins\` at start; a Vortex extension treats such DLLs as "Far Cry Script Extender"
  plugins. No source repository found by search.
- Searches that found no Far Cry 2 co-op campaign project: "Far Cry 2 campaign co-op mod",
  "... Dunia ... moddb", an extended co-op/Nexus/Reddit query, a Russian query, and a
  dedicated-server query. ModDB lists overhaul mods (New Dunia, Realism+Redux, True
  Misery Redux) and a 16-player deathmatch map, none co-op.

### From strings and Ghidra auto-analysis (on the test copy, nothing run yet)

Ghidra 12.1.4 headless analysis of `FC2.dll` and `Dunia.dll` succeeded. Keyword scan of
printable strings (script: local scratchpad, output: `local/dumps/strings-*.txt`):

- `Dunia.dll` holds the netcode. Class names: `NetworkEngine`, `CNetObject*`,
  `CNetDescriptor`, `CNetworkComponent` with Pawn, Vehicle, MountedWeapon, Projectile,
  Pickup, Weapon, Camera, Gadget, Ladder variants. NetObject types come in
  `Host` / `Controller` / `ThirdParty` roles (e.g. `NetObject.PawnHost`,
  `NetObject.PawnController`, `NetObject.PawnThirdParty`).
- Sessions and lobby: `CSessionInfo_Agora`, `CLobbyService`, `CFCXLobbyService`, strings for
  Ubisoft rendezvous / matchmaking errors, `MULTI_ERROR_NETWORK_BIND_MAIN_SOCKET`. A
  deathmatch lobby string exists (`Deathmatch Lobby (%d)`).
- Single player structure: `CFCXMissionManager`, `CGameMissionMgr`, `CBaseMission`,
  `MissionManager`, console command `get_mission_manager_status`, `SCRIPTS\MissionTools.lua`,
  `CLuaResource` and a `Lua 4.1 (alpha)` runtime string (Lua is the script language, to be
  confirmed in the archives).
- Buddy system in the strings: `get_current_primary_buddy`, `set_current_primary_buddy`,
  `CSpawnPointBuddy`, `BuddyRescueEnable`, `Selects a buddy to use as main avatar.`
- Saves: `SaveGame`, `CSavePointSaveGamePage`, `IsSaveGame`. Only one string with
  "coop": a DirectSound `SetCooperativeLevel` call (not a game mode).
- `FC2.dll` is small (1097 strings) and has none of these: networking sits in the engine DLL.
- PunkBuster is referenced by 15 strings in `Dunia.dll`.

### Archive unpacking (Data_Win32, on the test copy)

- Archive format: `FAT2` header, file version 5, platform Windows, compression version 3
  (read from the first bytes of `patch.fat`). Entries use the v5 layout: 2-bit compression
  id (0 none, 1 LZO1x).
- Tool: Gibbed.Dunia (zlib licence) https://github.com/gibbed/Gibbed.Dunia at commit
  `a43405cab3ea206ed0e9fcc6222fc2e36d545eea`, project `Gibbed.FarCry2.Unpack`, with the
  `FarCry2-File-Lists` submodule at `22f9499e837f9f524ac7a857d3e0af9e3f8ee72c`. No release
  builds exist, so it is built with `dotnet build` (SDK 9). It needed two local fixes at
  this commit (marked in the source as locally modified):
  1. `BigFileV2.ToCompressionScheme` only mapped file versions 10 and 11; added v5
     (0 = none, 1 = LZO1x, as in older upstream commits such as `da6fb1d`).
  2. `EntryDecompression.DecompressLZO1x` ignored `maxSize`, so the 64-byte type-guess
     buffer overflowed (`MemoryStream not expandable`); it now writes at most `maxSize`.
  The `configs` folder must sit next to the tool's folder.
  This is a playbook-worthy finding (a stock build of the tool does not unpack Far Cry 2).
- Result: `patch.fat` gives 216 files; `common.fat` gives 3428 files, 102 MB
  (`ui` 1977, `domino` 1071, `scripts` 218, `config` 26, `languages` 20, `engine` 27,
  `soundbinary` 24, `databases` 10, `graphics` 8, `ingameeditor` 14, `__UNKNOWN` 29).
- Scripts are Lua. `scripts\missiontools.lua` defines a `Mission` table with events such as
  `Event__MissionSetState`, `Event__SaveGame`, `Event__Game_PlaySequence`
  (calls `Game:MissionSetState`, `Game:SaveGame`).
- `domino\` holds 1071 Lua files, each one a node of a visual scripting graph system
  ("Domino"): a header comment declares `ControlIn`, `ControlOut`, `DataIn`, `DataOut`
  (e.g. `domino\system\assignbuddy.lua` calls `GetBuddiesManager():AssignBuddy(...)` and
  `GetFCXMissionManager():ActivateBuddyMission(...)`). `domino\system`: 233 nodes,
  `domino\user`: 838 nodes.
- The mission graphs themselves are not in `common`; `missions\missions.xml` is empty (0
  bytes) there. `worlds\worlds.dat` is 2.58 GB and likely holds maps and mission data.
  Unpacking it was started with unknown-type files skipped; result not read yet.
- Input action maps exist for single player and multiplayer separately
  (`config\inputactionmapsingle.xml`, `inputactionmapmulti.xml`).

- `worlds.fat` (v5, 142369 entries): 77776 files with known names extracted (2.0 GB):
  `levels\` (60219 files), `graphics` (13055), `domino` (821 mission-specific Lua nodes),
  `scripts` (126), `worlds\` (224). Top extensions: `.fcb` 26015 (binary object
  format), `.xbt` 14127 (textures), `.sdat`/`.zsr`/`.srl` 6000+, `.nvm` 4275 (navmesh),
  `.hkx` 2546 (Havok), `.lua` 821.
- Levels: `mp_*` (15 multiplayer maps, e.g. `mp_02_s_shanty`, `mp_25_prison`), `w1_*` and
  `w2_*` (campaign, two regions, sectors a to e, steps 1 to 5, e.g. `w1_b_2`), `tmpla`,
  `ige_map` (in-game editor). `scripts\engine\objects\pawn\statemachine\*.gosm.xml`
  (`main_avatar`, `vehicles`, `weapons`) are plain XML state machines.
- Files named `*.game.xml` in the MP levels are binary, not XML. Campaign level folders
  hold per-sector `.fcb` files and no `generated\*.managers.fcb` among the named files; the
  remaining 64593 entries have no known name and were extracted separately (not read yet).

- The 64593 unnamed entries were extracted separately (0.59 GB): 43403 without extension,
  12973 `.fcb`, 7282 `.xbt`, 862 `.hMvN`, 51 `.xml`, 22 `.xbg`.
- FCB to XML works with `Gibbed.FarCry2.ConvertBinaryObject` (same repo and commit,
  built unmodified). `binary_classes.xml` in the FC2 config names many classes
  (`CBaseMission`, `CConvoyMission`, `CBrainBuddyBase`, `CEnableBuddyDown`, ...) but many
  fields are still shown as raw hashes. Example: the multiplayer level
  `mp_02_s_shanty.managers.fcb` converts to XML (5.7 MB) with a readable entity
  `DominoManager` of class `CDominoManager` at the start, so the Domino graph runner is a
  per-level manager entity.

### Game modes and network config (data files, read from the unpacked archives)

- `engine\gamemodes\gamemodesconfig.xml` (common and a slightly larger patched copy in
  `patch.fat`, 491463 bytes) defines each game mode as a `<GameMode>` with a
  `<Desc Name=... GameModeEntityArchetype=...>` and a list of `<Service ClassName=...>`.
  Modes seen: `FCXConsole`, `FCXMainMenu`, `Single`, `Editor`, `FCXEditor` (the in-game
  editor, a networked mode), `FCXBenchmark`, `FCXSingle`, plus the multiplayer modes.
- Campaign (`FCXSingle`) services have no `NetPresence` attribute: `CSpawnPointService`,
  `CFCXPlayerService`, `CFCXMissionManager`, `CBuddiesManager`, `CFCXGameplayManager`,
  `CRescueManager`, `CFCXWeaponsService`, `CFCXAIBehaviorService`, `CFCXHudService`,
  `CPersistenceMgr`, `CDominoService`, and others. The networked `FCXEditor` mode lists
  `CFCXPlayerService`, `CFCXMissionManager`, `CFCXWeaponsService`, `CFCXHudService` with
  `NetPresence="AllPeer"`, and `CFCXDMSpawnPointService` with `NetPresence="HostOnly"`.
  So a service can be marked as replicated to all peers or host-only in this file.
- `engine\settings\defaultnetworkconfig.xml`: `UseAgora="0"`, `ConnectionSpeed="lan"`,
  ports 9000 to 9004 (online engine, online service, file transfer host and client, LAN
  broadcast). Net object types are defined in
  `engine\online\onlineengine\{engine,fcx,session,onlineengine}.netobject.xml`, groups and
  channels in `default.netgroup.xml` and `default.netchannel.xml`.
- `Dunia.dll` strings: `CFCXGameModeSingle`, `CFCXGameModeDeathMatch`, `...VIP`, `...CTF`,
  `...TeamDeathMatch`, `CFCXGameModeInitNetworkOperation`,
  `CFCXGameModeShutdownNetworkOperation`, `GameModeOnNetReady`, `-dedicated`,
  `IsDedicated`, `net_GetCurrentGameMode`.
- Ghidra (decompiled, `Dunia.dll`): `FUN_10750140` registers `CFCXMissionManager` as a Lua
  class (`gettable_event`), confirming the Lua binding. `FUN_104ebf90` is
  `RegisterNetworkComponents`; it branches on a global flag at `DAT_11645a74 + 4`.
  `FUN_10218010` builds a table of network log categories (General, Transmission,
  MainSocket, NetworkEngine), not engine start-up. `MULTI_ERROR_NETWORK_BIND_MAIN_SOCKET`
  has no direct code reference (probably looked up by table). Function names are Ghidra
  auto names. Scripts: `local/scripts/XrefStrings.java`, `strs.py`, `grepstr.py`.

### Command line and first run (test copy)

- Command-line switches found as strings in `Dunia.dll` (not yet tried, except
  `-dedicated` through the launcher): `-host`, `-join`, `-client`, `-dedicated`, `-noagora`,
  `-world`, `-load`, `-spawn`, `-spawnpos`, `-spawnangle`, `-cmdfile`, `-exec`, `-logFile`,
  `-norender`, `-nosound`, `-borderless`, `-online`, `-ranked`, `-login`, `-nobf`,
  `-noarchivecache`, `-usearchivecache`, `-benchmark*`.
- Archive names in the engine's file list (strings): `common.dat`, `patch.dat` (listed last),
  `shadersobj.dat`, `sound.dat`, `sound_%lang%.dat`, `worlds/<name>/<name>.dat`. Whether the
  engine also reads loose files from disk instead of the archives is still unknown.
- 2026-10-19, run 1: `FC2ServerLauncher.exe` from the test copy, default options, 40 s, then
  stopped by its PID. Result: it started without any Ubisoft service. Log
  (`bin\dedicated_log.txt` in the copy): login `EAccountFlags_Offline`, session
  `E_SESSION_TYPE_OFFLINE` on `<LAN address>:9001`, 16 public slots,
  `match_pb_enabled = 0`, `network_type = 1`, scanning ports 9000 to 9999, LAN broadcast
  port 9004. Setting `gamemode` fails with "can only do it in the game lobby". No crash. This
  only shows the server boots and opens an offline LAN session; no client joined and no map
  was loaded.
- Side effect outside the workspace: the run created
  `Documents\My Games\Far Cry 2\Server\dedicated_server.cfg` (3217 bytes). That folder did
  not exist before; no save files exist there. Real saves were not touched (not located yet).

### Start-up code paths (Ghidra, `Dunia.dll`, auto names)

- `FUN_10663d40` is the command-line dispatcher (`-exec` first; then `-editorpc`,
  `-benchmark`, `-host`/`-join`, `-load`, `-wait`, otherwise the normal start `FUN_106622f0`).
- `-world <name>` and `-map <name>` are only used by the benchmark launcher `FUN_10662f90`
  (its error text: `-benchmark {playback|record|sectors|spawnpoints|path} -world worldname
  -map mapname`, with `-spawn`, `-spawnpos`, `-spawnangle`). Without `-benchmark` they do nothing.
- `-load <savegame>` (`FUN_10661f50`) loads a save file and reads `PlayerPos`. Saves are
  `*.sav` under `Documents\My Games\Far Cry 2\` (string `My Games\Far Cry 2\`). No save exists
  on this PC, so this path cannot be tried without creating one first.
- `-host` (`FUN_10662440`): requires `-login <name>` ("Invalid parameters for -host, missing
  (-login)"); optional `-password`, `-keyonline`, `-bfname`, `-dedicated`, `-sessionuid`,
  `-ranked`; mode keys `-team`, `-ctf`, `-vip` (default is deathmatch); network type `-online`
  or `-lan`; `-online` with `-noagora` is refused ("Cannot launch an online game (-online)
  without using Agora").
- `-join` / `-client` (`FUN_10662b40`) and `-wait` go through `FUN_10663af0`.

### Client runs on the test copy (2026-10-19)

All runs: `bin\FarCry2.exe` of the test copy, working directory `bin`, stopped by PID.

| Arguments | Result |
|---|---|
| `-noagora -nosound -logFile <absolute path>` | exited with code 0 at once (not reproduced; maybe a first-run step) |
| `-nobf`, `-borderless -logFile fc2.log`, `-noagora`, `-nosound`, `-logFile fc2b.log` (each alone) | window titled "Far Cry 2", process alive for the whole 15 to 20 s |
| `-noagora -world w1_b_2 -logFile fc2c.log` | working set about 40 MB, exited with code 0 at about 30 s |
| `-host -login coopbot1 -lan -noagora` | working set 38 to 69 MB, no UDP or TCP port opened, exited with code 0 at about 30 s |

- `-logFile` did not produce a log file in the copy or in `Documents`.
- Nothing was written to `Documents` by the client runs, and the original install shows no
  changed file.
- Unknown: why the client never grows past about 70 MB and exits by itself after about 30 s.
  Candidates (not checked): a failed graphics start on this PC, an intro or menu that waits
  for input, a Steam check. The dedicated server, by contrast, started and opened a session.

### Why the client stalls (diagnosis in progress, 2026-10-19)

- Window-only captures (`local/scripts/shot-window.ps1`, `PrintWindow`, not desktop capture):
  `FarCry2.exe` with no arguments shows the Far Cry 2 splash artwork in a 572x380 window and
  does not change between 10 s and 22 s.
- Process state at about 14 s (`local/scripts/list-modules.ps1`, 32-bit PowerShell needed to read
  modules of a 32-bit game): about 2 s of CPU in a 6 s window (busy, not frozen), 15 threads, all
  waiting. Loaded: `Dunia.dll`, `systemdetection.dll`, `dxdiagn.dll`, WMI (`wbem*`, `fastprox`),
  `d3d9`, `d3d10`, `d3d11`, `d3d12`, `dxgi`, AMD user mode drivers, `binkw32`, `dsound`, `EAX`,
  `ws2_32`, `AcLayers.DLL`. `FC2.dll` is **not** loaded yet, so the game library never starts.
- `FC2Launcher.exe` from the copy shows no window, lives a few seconds and exits with code 0.
- `-norender -nosound` alone: exits with code 0 after 23 s (74 MB peak); with
  `-host -login coopbot1 -lan -noagora`: after 7 s (41 MB); with `-dedicated`: after 3 s (65 MB).
- `systemdetection.dll` strings: DxDiag and WMI queries (`Win32_VideoController`,
  `Win32_PhysicalMemory`, `Win32_ComputerSystem`, `Win32_NetworkAdapter`) and error texts such as
  `Could not detect information for adapter %i`, `Could not retreive current resolution width`.
- This PC reports two video controllers: `AMD Radeon(TM) Graphics` and a `Virtual Display Device`
  with no resolution and no RAM value. WMI answers all the queries above in under 0.4 s.
- ~~Guess: the system detection step fails on the virtual adapter.~~ **Refuted for the display
  checks** (see below). Not tested: Steam launch of the original install, `FC2Launcher.exe` with a
  profile.

Static reading of the start-up (Ghidra, `Dunia.dll`, auto names; scripts `local/scripts/DecompAt.java`,
`display-check.ps1`):

- Export `RunGame(HINSTANCE, const char*)` (`10006510`): if the command line has `-openautomate` it
  calls `FUN_10005fa0` and returns true. Otherwise it loops: `FUN_10006130` (creates the splash
  window), `InitDuniaEngine` (`10004900`), `DestroyWindow(splash)`; if `InitDuniaEngine` returns 0
  it returns false (the stub exits with code 0). Otherwise it runs the game (`DAT_10fd42c8`).
- `InitDuniaEngine` returns 0 on these paths seen in the decompilation: the named mutex
  `FarCry2Instance` already exists (`OpenMutexA`); no display device with the flags
  ATTACHED_TO_DESKTOP and PRIMARY_DEVICE (`EnumDisplayDevicesA` loop); `EnumDisplaySettingsA` of
  the current mode fails; the current mode is not 32 bits per pixel (unless a flag is set);
  other branches not read yet. A `MessageBoxA(..., "Error", 0)` is shown if the command-line
  dispatcher `FUN_10663d40` returns 0.
- The same display calls made read-only on this PC (`display-check.ps1`, C# P/Invoke, 32 and 64
  bit): `\\.\DISPLAY1` AMD Radeon, flags `0x80005` (attached, primary), 32 bpp, 1920x1080@60;
  `DISPLAY2/3` not attached; `DISPLAY4` "Virtual Display Device" flags 0 (not attached). So these
  checks would pass. (A first PowerShell version reported "no device" because of a marshalling
  mistake of mine, error 87; the C# version is the one to trust.)
- Not yet known which branch exits. A debugger (breakpoints on `ExitProcess`, `MessageBoxA`,
  `OpenMutexA` results) or a Process Monitor trace would answer it; neither is installed.

- Bulk conversion of the 12973 unnamed `.fcb` files to XML finished: 12973 converted, 0 failed
  (`local/dumps/xml-unknown`, not analysed yet).

### World sectors and mission layers (converted XML, nothing run)

- World data is split in `WorldSector` objects (header fields `Id`, `X`, `Y`), each a list of
  `Entity` objects with components. 2023 of the 12973 unnamed `.fcb` files are sectors; one
  campaign level folder (`levels\w1_b_2`) converted to 1105 XML files of 1246 (141 failed, not
  looked into), 605 of them sectors.
- Every entity carries a `CMissionComponent` (string field hash `7AF1FD74` plus
  `hidMissionLayerPath`). Empty string and path `FFFFFFFF` mean "always loaded".
  Script: `local/scripts/mission_layers.py`, results `local/dumps/mission-layers*.txt`.
- Unnamed sectors (multiplayer maps): only four non-empty layers, named after the game modes:
  `fcxdeathmatch`, `fcxteamdeathmatch`, `fcxvip`, `fcxctf`. So the multiplayer modes select their
  content through layers, as the game-mode name.
- Campaign sectors of `w1_b_2`: 61 distinct layers, mostly mission paths such as
  `missions\librarymissions\a1lm02\wager_trainwreck` (290 entities), `...\wager_enemies`,
  `missions\convoymissions\convoy_01_disable`, `missions\grinmissions\mission_01\operative`,
  `missions\carvertapemissions\carvertapes`, `missions\safehouse\weaponcrates\magiccrate_01`,
  `missions\safehouse\upgrades\w1\level_01` to `level_06`, `missions\_disableformission\...`,
  plus `benchmark`. 528 entities have no layer.
- Reading: the campaign world is one persistent open world per region whose mission content is
  switched on and off per layer, so the set of active layers is the mission state a second player
  would need to share. This is derived from the data, not tested in the game.

### Client starts: baseline, profile and the `-noagora` finding (2026-10-19, later)

- Owner ran the original install from Steam: it works and reaches the main menu (owner's report,
  not checked by me). That run created `Documents\My Games\Far Cry 2\GamerProfile.xml` (2205
  bytes; `Platform="d3d9"`, 1760x990, `Fullscreen="1"`, network ports 9000 to 9004, no account).
  Before that file existed the client of the test copy never got past start-up.
- With the profile present, the test copy runs: no arguments, 90 s alive, working set 177 to 189
  MB (the first 20 to 25 s are a quiet start-up phase at about 40 MB). Window captures of the
  fullscreen Direct3D window are all black (`PrintWindow` cannot read it), so they prove
  nothing about what is on screen.
- Argument results on the test copy, 45 s each: `-host -login coopbot1` alive, 97 MB;
  `-host -login coopbot1 -lan` alive, 97 MB; `-nobf` exited code 0 after 22 s; `-noagora`
  exited code 0 after 22 s; earlier `-host -login coopbot1 -lan -noagora` exited after 31 s.
  So **`-noagora` makes the client quit** here (cause not read in the code). `-nobf` alone also
  exited this time although an earlier 20 s run of it stayed alive: not explained.
- `-host -login coopbot1 -lan` (75 s run): UDP 9000 and 9001 opened after about 26 s, 34 to 40
  threads, 96 MB, no TCP listener and no outgoing TCP connection seen. That is a LAN host session
  from the unmodified game files of the copy. Nobody joined, no map was seen.
- The game takes over the screen (fullscreen) while a run lasts. `GamerProfile.xml` is the
  owner's file and was not changed.
- Two clients on one PC will collide on the named mutex `FarCry2Instance` (`OpenMutexA` in
  `InitDuniaEngine`): the second instance exits. Not tried yet.

### Two copies, a dedicated LAN server and a join attempt (2026-10-19, evening)

- Second test copy `local/test-installs/FarCry2-B` (copied from the first test copy, never from the
  real install; real `Dunia.dll` and copy A hashes re-checked unchanged). Patches in its
  `Dunia.dll` only (`local/scripts/patch_mutex.py`, `patch_bytes.py`, backups kept next to it):
  1. file offset `0xe08950`: mutex name `FarCry2Instance` to `FarCry2Instanc2` (1 byte differs),
     so two game instances can run at the same time. Verified: the copy B client ran next to the
     copy A server and next to nothing else; two clients at once not yet tried.
  2. file offset `0x662032` (address `10662c32`, the immediate of `PUSH 0x2` before
     `CALL 10aa1580` in the `-join` handler `FUN_10662b40`): `02` to `03`, trying LAN instead of
     online as the network type. The host handler uses 3 for `-lan` and 2 for `-online`.
- `-join` takes an IPv4 address (`inet_addr`); messages: "Trying to launch an online client, but ip
  is missing (-join xxx.xx.xx.xx)" and "...with an invalid ip".
- The dedicated server's default config (`Documents\My Games\Far Cry 2\Server\dedicated_server.cfg`,
  made by the game, owner's folder, not edited) has `network_type 1` ("1 = Online, 2 = LAN"). With it
  the server tries to log in to the shut-down online service and logs
  `E_SESSION_ERROR_CORE_TASK_TIMEOUT` and "The Far Cry 2 server is not available at this time".
- With an own config (`local/test-installs/server-lan.cfg`, passed as
  `FC2ServerLauncher.exe -dedicated "<path>"`, `network_type 2`, `match_pb_enabled 0`): the server
  logs a login with `EAccountFlags_LAN`, then creates a session `E_SESSION_TYPE_LAN`, and prints
  `Game message: Entering lobby!`. Its sockets: UDP 9000 on all addresses, 9001 and 9002 on the LAN
  address, 9004 (LAN broadcast) on all. The session record advertises the address of a VPN adapter
  of this PC instead of the LAN address (not written here).
- The server's own log file `dedicated_log.txt` is cumulative and was not updated by runs that were
  killed; the console output (stdout redirected to a file) is live and complete.
- Join attempts, copy B client with `-join <ip>`, three addresses (loopback, LAN address, the
  advertised address): in every case the client reached about 100 MB after 25 s, opened UDP 9003 and
  9004, and **quit by itself with code 0 after about 46 s**; the server never logged a second peer
  (only its own host entry with `PeerId: 0`). So no join was achieved. The exit looks like a
  failed join (not confirmed). The same exit happened before and after the `02` to `03` patch.
- Untried: the in-game LAN or direct-connect screens (strings `SearchSessionByJoinIp`, `JoinMatch`,
  "Try to join using direct connect but no ip was set."), `-wait`, a client host (`-host -login x
  -lan`) joined from the second copy, and reading what happens after `FUN_10aa1580` in Ghidra.

### Host client plus join, and the static join trace (2026-10-19, night)

- Test: copy A full client `-host -login coopbot1 -lan` (UDP 9000, 9001 after about 30 s), then
  copy B client `-join <LAN address>` (`local/scripts/host-join-test.ps1`, 75 s). Results: the
  client stayed alive for the whole 75 s (it quit after 46 s when the host was the dedicated
  server); it opened UDP 9002, 9003, 9004; the system counters `UDPv4 Datagrams Sent/Received` showed
  a single short burst (3 sent, 2 received per second) at the moment the client network came up, and
  0 afterwards. No log exists on either client, and window captures are black, so whether the client
  actually joined is **unknown**. The burst is the only hint of a handshake attempt.
- Static trace of the join: `SearchSessionByJoinIp`, `SearchSessionId`, `JoinSessionId` are field
  names registered in a reflection table of the search-session parameter class (`FUN_100f2980`),
  not the join logic. The message "Try to join using direct connect but no ip was set." is
  referenced from a lazily initialised string-hash helper at `106cb980` (not inside a recognised
  function), not from the logic either. Nothing in this pass explains why the client quits.
- Observability is the actual blocker: clients have no log (`-logFile` writes nothing), the
  fullscreen window cannot be captured, and the profile folder is shared with the owner. Ideas, in
  order of effort: (1) per-copy profile folder by patching the string `My Games\Far Cry 2\`
  (`Dunia.dll` offset `0xe089a0`, 19 bytes) so each test copy has its own `GamerProfile.xml` with
  `Fullscreen="0"` (needs new folders under `Documents`, owner to approve); (2) an in-process DLL
  (proxy or plugin loader, as FCSE does) that logs engine calls and later hosts the co-op code.

## Derived

- Nothing to revive: no co-op project for Far Cry 2 turned up in the first three searches.
  The path is "build from zero" (playbook/how-coop-mods-work.md).

- Derived from the strings, not from running the game: the engine already replicates pawns,
  vehicles, projectiles and pickups with host/controller/third-party roles, so a second
  player in the campaign is plausibly a matter of running the single-player mission code
  on that netcode, as with other engines that ship a deathmatch mode (see
  playbook/triage.md, step 5). Mission logic is likely Lua.

- Derived from the config files (not run): game modes are data-driven, and the engine has
  a per-service replication setting. A co-op mode may be expressible as a new `<GameMode>`
  that takes the `FCXSingle` service list and adds `NetPresence` to the services that must
  be shared, with the player pawn spawned as a networked `PawnHost` / `PawnThirdParty`.
  Whether the single-player level data (sectors, Domino graphs) loads under a networked
  mode is untested.

## Guesses

- FCSE could be the hook point for a native plugin. Not checked.
- The multiplayer netcode in `Dunia.dll` may be reusable for campaign co-op. Not checked.
- The single-player build may not instantiate the NetworkEngine at all, so a campaign
  session might need to be started through the multiplayer game-mode path. Not checked.

## Unknown

- Whether a Far Cry 2 co-op project exists on Discord, GitHub forks/branches or Archive.org
  (not searched yet; "not found by web search" is all that can be said so far).
- FCSE licence, supported versions, source availability (Nexus page blocks WebFetch).
- How the campaign scripts and mission state are stored.
- (answered 2026-10-19) `gh issue list` / `gh pr list --state all` searches for "Far Cry 2",
  "farcry", "Dunia": no issue, and no PR about this game (PR #2 matched by accident, it is
  about Celeste). Claim issue opened: https://github.com/cranebeaker/coop-anything/issues/29
  (the `in-progress` label from the template does not exist in the repo, so it has no label).

## Log

- 2026-10-19: identity recorded in `local/identity.md` (private). Copied the install to
  `local/test-installs/FarCry2` (187 files, 3.44 GB). Downloaded Ghidra 12.1.4 from the
  official GitHub release, SHA-256 `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`
  matched the release notes. Started headless import and analysis of `FC2.dll` and
  `Dunia.dll`.

## Per-copy profile folders and window capture (2026-10-19, morning)

Owner approved creating test profile folders under `%USERPROFILE%\Documents\My Games\`.

Observations (all ran):
- `Dunia.dll` has exactly one ASCII occurrence of `My Games\Far Cry 2\` at file offset 0xe089a0 (19 bytes). The UTF-16 hits are 12 other places, not touched.
- Copy A now uses `My Games\FC2test_A\`, copy B `My Games\FC2test_B\` (same-length patch; backups `Dunia.dll.before-profile`). Each folder holds a copy of the owner's `GamerProfile.xml` with `ResolutionX=800 ResolutionY=600 Fullscreen=0`. The owner's own profile hash was unchanged afterwards.
- The game did not write anything into the new folders (no file changed, no new file).
- Plain start of B stays alive 45 s both with and without its profile file. So that test does not show whether the new folder is read.
- Window size at start is 1408x792 (80 % of the owner's 1760x990), not 800x600. This hints the engine still takes the size from elsewhere, or overrides it. Unknown which.
- Capture: `PrintWindow` and a screen-region copy (`shot-screen.ps1`, window brought to front) both give a black image of the D3D window, also with `-host`.

Derived: window capture is a dead end in this environment, whatever the profile says.

Unknown: whether the new profile path is read at all; why capture is black (session without a real desktop, or the D3D swap chain).

Next: an in-process logging DLL in the test copies (needs a 32-bit compiler; ask the owner before installing anything).

## In-process probe (dinput8.dll proxy), first results (2026-10-19)

Tool: `dinput8.dll` proxy built with the installed 32-bit MinGW (gcc 9.2), placed in the bin folder of both test copies. It forwards `DirectInput8Create` to the system DLL, waits for `Dunia.dll`, patches its import table and writes `coop_probe.log` next to itself. Hooked: `bind`, `connect`, `sendto`, `recvfrom` (ws2_32), `PostQuitMessage` (user32). `ExitProcess` is not imported by `Dunia.dll` (no hook installed). The probe source is not in this repository (game code lives in the game's own repo, see the README for its status).

Observations (host A `-host -login coopbot1 -lan`, client B `-join <LAN IP>`, ran once, 60 s):
- The proxy loads and all 5 hooks apply in both processes. Logging works with no desktop capture.
- Host A: binds UDP 9000 and 9001 on the LAN address (9000 retried with an already-used result, then ok). It also tries `connect` to a public address on TCP port 80, which fails (-1).
- Client B: binds 9000, 9001 (fail, port taken by A), then 9002 and 9003, 9004. Then `connect` to a public address on TCP 80 (fail) and TCP 3074 (fail), where 3074 is the port used by the old online services.
- Neither process logged any `sendto` or `recvfrom` towards the other peer. System UDP counters showed a few datagrams, so another send path exists (probably `WSASendTo` or another module). Unknown.
- `PostQuitMessage` was not called during the 60 s; the client stayed alive.

Derived (not proven): `-join` goes through the dead online services first (TCP 80 and 3074); the LAN peer is never contacted over the hooked calls. This would explain why joining shows nothing.

Next: hook `WSASendTo`/`WSARecvFrom`/`WSASend`/`getaddrinfo`/`gethostbyname` and the first-chance exception and exit paths; log which module calls `connect` (caller address); then try to make the engine use a LAN-only path.

## Probe round 2: name resolution and callers (2026-10-19)

Probe now also hooks `WSASendTo`, `WSARecvFrom`, `getaddrinfo`, `gethostbyname` (9 import entries patched) and prints the calling module+offset. Same setup (host A `-host -login coopbot1 -lan`, client B `-join <LAN IP>`).

Observations (ran once, 50 s):
- Both processes resolve the PC's own host name many times at start (`Dunia.dll+0x14200`).
- Host A: resolves `onlineconfigservice.ubi.com` (`Dunia.dll+0xd928b4`), `connect` to the resulting address on TCP 80 fails (`+0xd921d7`). Later `getaddrinfo("locate.madserver.net")` fails with 11001 (host not found; `+0xc86a27`).
- Client B: the same, plus it resolves `gconnect.ubi.com` (`+0xc16339`) and tries `connect` to TCP 3074, which fails.
- Still no `sendto`, `WSASendTo`, `recvfrom` or `WSARecvFrom` calls in either process, so the engine sends its UDP some other way (unknown; maybe a different import, or sockets in another module).
- Neither process quit in 50 s.

Derived: the client's `-join` path wants the old Ubisoft connect service (`gconnect.ubi.com:3074`) before it does anything on the LAN. Those servers are gone. So the stock join cannot work as is, and the `02`→`03` network-type patch is not enough.

Options (not decided): (a) make the engine skip that service step by patching, (b) emulate the service locally (unknown protocol, large), (c) leave the stock multiplayer netcode alone and run our own co-op transport inside a loaded DLL, as other co-op mods did. Recommendation: (c), using the probe DLL as the base.

## Lua channel into the running game (2026-10-19)

The probe source (`probe.c`, `probe.def`; build: `gcc -O1 -shared -o dinput8.dll probe.c probe.def -lws2_32 -static-libgcc`, 32-bit MinGW) is not in this repository (game code lives in the game's own repo, see the README for its status). It is a `dinput8.dll` proxy for the test copies only. RVAs below are for the Steam v1.03 `Dunia.dll` (SHA-256 in the private identity record) and need re-checking for any other build.

Observations (ran, copy A at the main menu):
- The scripting language is **Lua 4.0** (statically linked, no symbols): base library has `getglobal`, `tag` methods, `foreach`, `globals`, `copytagmethods`, `_ALERT`. Found from the `dostring` registration table at VA 0x10e3b150 (name, function pairs).
- Lua API addresses (cdecl, VA = 0x10000000 + RVA): `lua_gettop` 0x102aa9c0, `lua_settop` 0x102aa9e0, `lua_getglobal(L, name)` 0x102aaf90, `lua_dobuffer(L, buf, size, name)` 0x102ab600, `luaL_check_lstr(L, narg, &len)` 0x102d24b0. Lua values take 5 bytes on the stack.
- The main Lua state pointer is stored at `*(void **)0x11606728` (the helper `FUN_102aa850(0, name, cfunc)` at 0x102aa850 reads it and registers a C function as a global; this is how the engine registers its Lua API in `FUN_109fc830` and `FUN_1070f5e0`).
- Running Lua from outside works: the probe drops a file `coop_cmd.lua` in the game folder, a `PeekMessage` hook on the main thread runs it with `lua_dobuffer`, then reads the Lua global `__out` and logs it. Test: `__out = "alive " .. (1+2)` returned `alive 3`. `_ALERT`/`print`/`writeto` give no visible output in this build (the `_ALERT` hook never fired), so `__out` is the output channel.
- 573 globals; 21 are tables (`Game`, `System`, `Script`, `Sound`, `CGameMissionMgr`, `CBuddiesManager`, `CFCXRegionManager`, `CDominoConsoleCommandManager`, ...); 246 functions. `Game` has 98 methods, among them `GetLocalPlayerId`, `IsServer`, `IsLocalPlayer`, `GetEntityTeam`, `SetPlayerId`, `SaveGame`, `LoadNextMap`, `LoadScript`, `Exec`, `MissionSetState`, `SetHealth`, `SelectMainAvatar`, `GetCurrentGameType`, `SendGlobalChatMessage`.
- Entity-related globals: `TeleportEntity(entity, destEntity)`, `SpawnEntityFromArchetype`, `RemoveEntity`, `IsEntityValid`, `IsEntityLoaded`, `GetEntityName`, `GetInvalidEntityId`, `KillPawnAgent`, `SetVisibility`, `GameChangeWorld`. No script function returns an entity position.
- `net_log_enable` / `net_log_disable` and `debug_envNetwork` exist as console variables, possibly a built-in net log (not tried).
- The native behind `TeleportEntity` (`FUN_109fb530`) does not move anything itself: it builds a network event with fields `entityId` and `destId` and sends it through the event system. Many script operations seem to be network events (see also `CFCXTeleportEntityOperation`).

Derived (not proven): co-op could be built on this event system (script operations are events with a host/controller role) rather than on raw object replication; a position getter needs native code (entity lookup by id) because the script API has none.

Unknown: the native entity table and how an id maps to an entity object; where a pawn's position is stored; which events are already replicated in a network game mode.

Next: (1) decompile the entity lookup by id and the pawn position (start from `IsEntityValid` and `GetLocalPlayerId` natives); (2) try `net_log_enable` to see if the engine logs its own network events; (3) test whether a campaign world loads when the game type is switched via `Game:LoadLevel`/`GameChangeWorld` while the probe runs.

## Entity lookup, and why a loaded world is needed (2026-10-19)

Observations (static, Ghidra on the Steam v1.03 `Dunia.dll`; VA = 0x10000000 + RVA):
- Entity ids are **64-bit**. Lua gets them as decimal strings (parsed with `sscanf "%llu"` in the shared wrapper `FUN_1058f250`). The invalid id is 0xFFFFFFFFFFFFFFFF; in the main menu `GetLocalPlayerId()` returns `18446744073709551615` and `IsEntityValid` is false (run).
- Entity lookup by id is `FUN_10055820` (thiscall; `ECX = *(void **)0x11644e80`, the entity manager; stack args: out pointer, id low, id high). It returns a ref-counted holder; `holder+8` is the refcount and `holder+0xC` the entity object (0 if not found). `IsEntityValid` then tests bit 4 of the object's flags dword at `+0x70` (invalid when set).
- `GetLocalPlayerId` native is `FUN_1058fa00` (calls `FUN_1058f8c0`, which reads a list from `DAT_11645578`).
- Lua natives register through `FUN_102aa850(0, name, cfunc)`, in batches in `FUN_10591120` (entities), `FUN_109fc830` (gameplay) and `FUN_1070f5e0` (console).

Tools: the probe got native commands (`coop_native.txt`: `peek <hexaddr> <len>`, `ent <id> <len>` with a float column). Built and deployed, not yet run against a real entity.

Tried to get a campaign world loaded without the menu (all ran, none loaded a world; memory stayed flat, local player id stayed invalid):
- `Game:LoadLevel("w1_b_2")` from Lua: run error (status 1); `LoadLevel` is a console template, not a method of the `Game` table.
- `GameChangeWorldDefaultSpawnPoint("w1_a_1")`: runs without error, nothing loads in the menu.
- `-benchmark -world w1_a_1`: nothing visible loaded after 75 s (world name may be wrong).

Blocker: no campaign save exists in the owner's profile, so `-load` has nothing to load, and the menu cannot be clicked without a visible window.

## Getting a world loaded: `-load` and menu keys (2026-10-19, later)

Owner played to the first save; the save is `Saved Games\<number>.sav` (1.1 MB) in the profile. A read-only copy is in the private workspace and in the two test profile folders (`Saved Games` subfolder). The owner's profile file changed (their play), test profiles were refreshed from it with `Fullscreen=0`, 800x600.

Observations (ran):
- The probe now hooks `CreateFileA/W` (paths containing `av`, `Saved`, `Documents`, `Profile`). `-load <arg>` opens `<profile>\Saved Games\<arg>` literally, with no extension added: `-load 1624175031227` fails to open, `-load 1624175031227.sav` opens fine.
- After a successful open the process still exits within ~3 s with code 0, no `PostQuitMessage`, no error. So `-load` does not start the campaign from a normal `.sav` in this build (it may be a developer path for another save kind; the handler reads a `PlayerPos` key). Unknown why.
- Sending Enter keys to the game window with `SendKeys` moved the working set from 144 MB (splash) to about 180 MB (menu) but never produced a valid local player id. This approach is blind and can type into whichever window has focus, so it was dropped and the script deleted.
- A whole-desktop capture works in this session (it showed the owner's real desktop, so it was deleted at once and must not be used again). The game window itself is still not capturable.

Unknown: how the menu's Continue starts a campaign; whether a Lua or native entry point does it directly.

## First live entity dump: the player (2026-10-19)

Setup: the owner started test copy A by hand (own profile `FC2test_A`, windowed), pressed Continue, stood in the world. The probe's Lua channel and `ent` command ran on that process.

Observations (ran once, one dump, the player standing still):
- `GetLocalPlayerId()` = `13072524568650856960`, `IsEntityValid` = true, `GetEntityName` = `player.MainCharacter.PawnPlayer.<character name>`, `Game:IsServer()` = true and `Game:GetCurrentGameType()` = 0 in the single-player campaign.
- The entity object (64-bit id -> holder -> object at `holder+0xC`) starts with the vtable pointer at `+0x00`. At `+0x30` there is a 4x4 float matrix, row-major, 16 bytes per row: rows 0-2 look like a rotation (row0 `-0.664 0.748 0 0`, row1 `-0.748 -0.664 0 0`, row2 `0 0 1 0`), row 3 at `+0x60` is `(2801.552, 2884.597, 17.0, 1.0)`.
- Entity flags dword at `+0x70` was not isolated in this dump; the earlier static reading of `IsEntityValid` used the flags in the object returned by the lookup (bit 4).

Derived (not yet proven): `obj+0x60..0x68` is the world position (x, y, z with z up) and rows 0-1 give the yaw. Needs a second dump after the player moves to confirm.

Next: second dump after moving; then write a position reader and a position setter (a setter may need to go through the engine's teleport event rather than writing the matrix, because physics and the network layer track the entity).

## Second dump: position confirmed (2026-10-19)

Observation (ran): after the owner walked forward and turned the camera, the same entity (same id, same object address, `refs` 76 -> 77) showed row 3 of the matrix at `+0x30` changing from `(2801.552, 2884.597, 17.000)` to `(2781.723, 2869.039, 17.002)` (about 25 m), and rows 0-1 changed from `(-0.664, 0.748)/(-0.748, -0.664)` to `(0.763, 0.646)/(-0.646, 0.763)`; the rows stay orthonormal.

Derived (supported by the two dumps): the player's world transform is a row-major 4x4 float matrix at `entity+0x30`; position = `+0x60`, `+0x64`, `+0x68` (z up), yaw from `atan2(row0.y, row0.x)`. The entity address stays the same while the entity lives.

Probe update: new native commands `pos <id>` (logs position and direction), `setpos <id> x y z` (writes the three floats; effect on physics unknown, may snap back) and `poke <hexaddr> <hex32>`. Not yet run.

## setpos test: writing the matrix does not stick (2026-10-19)

Observation (ran, owner in the world, player standing): `pos` returned `(2806.237, 2886.508, 17.262)`. `setpos` wrote `(2814.0, 2886.5, 17.3)` into `entity+0x60..0x68`. Four reads over the next ~3 s returned the original values each time; the first read came ~2.7 s after the write because of the command pump interval, so the engine restored the value within that time (or before the next frame).

Derived (not proven): the entity's matrix is recomputed every frame from another source (physics body or the pawn controller), so writing it directly does not move the pawn. Unknown: what the owner saw on screen.

Next options: (a) find the source object (a physics or controller pointer inside the entity, for example the fields near `+0x9c` and `+0x1c0`) and write there, (b) use the engine's teleport event (`TeleportEntity(entity, destEntity)`), which needs a destination entity placed where we want, (c) write every frame from a hook right after the engine's update.

## Per-frame hold test (2026-10-19)

Observation (ran, owner in the world, standing): `hold` wrote the matrix position (+8 m in x) on every `PeekMessage` call of the main thread for 8 s (1168 ticks, about 146 calls per second). At the start of each tick the probe logged what the engine had left: in 5 of the first 8 ticks the original value was back; the written value survived only in ticks where `PeekMessage` ran several times between two engine updates (ticks 9, 10, 840, 960). `pos` after `unhold` was the original again.

Derived: the engine recomputes the entity matrix from another source on every update, and a write from the main loop does not persist. Owner's observation of what the screen showed is pending.

Next: look for the source (physics body or controller object referenced from the entity, such as the pointers near `+0x9c`, `+0x1c0` and `+0x1d4`; the 3-4 pointers seen in the dump) and write there; or send the engine's own teleport event.

## Where the player position really lives (2026-10-19)

Method (ran): memory scans for the float pair (x, y) of the player at two different standing positions about 14 m apart, 4 and 16-byte window, tolerance 0.01; the addresses present in both scans (value changed from old to new at the same address) are the live candidates. Then each candidate got `x + 6` written, one at a time, and the entity position (`pos`) was read after 1.2 s.

Observations:
- Scan 1: 66 hits; scan 2: 54 hits; 45 addresses in both. 17 of those follow the player's z. The other 28 have z of 11-13 or -1000005 (bounding-box style initial values) and were not tested.
- 16 of the 17 follow-the-z addresses are passive copies: writing to them leaves `pos` unchanged, including the copy inside the entity at `entity+0x140`.
- Writing `x + 6` to the heap address `3C22A050` (first float of a struct: x, y, z, then a small float; the next 12-byte groups hold other positions) moved the entity position by exactly +6, the value stayed, and writing the old x back restored it. So that struct is the source that the entity matrix at `+0x60` is derived from. A second test (+8 m for 6 s and back) also took effect in `pos`. What the owner saw on screen is pending.
- The address is a heap address and changes between sessions; it is not a fixed offset.

Derived: the player can be moved by writing x, y, z into this source struct once (no per-frame hold needed). To use it in a mod the struct must be found from the entity by a pointer chain, not by scanning.

Next: find who points to `3C22A050` or to the start of its struct (pointer scan for dwords in `3C22A000..3C22A050`), then find the chain from the entity object to it.

## Pointer chain from the entity to the position matrix (2026-10-19)

Method (ran, in the live process, read-only): reverse scan for pointers into `3C22A000..3C22A050`, then a breadth-first search over pointer-looking dwords (aligned, heap range, not in the `Dunia.dll` image) starting at the player entity object. Tools (private): `local/scripts/memscan.ps1`, `local/scripts/ptrchain.ps1`.

Observations:
- The struct at `3C22A020` is a 4x4 transform matrix: identity rotation in the first three rows and the position in row 3 (`+0x30` from the matrix start). Its neighbours at `3C22A060/70` hold other positions (probably previous and target values).
- Another object holds the same position as three points (position, +0.45 m, +1.35 m) and a pointer to the matrix; this looks like the character controller's capsule.
- BFS found chains at depth 5-6. The shortest: `entity+0x9C -> A; A+0x140 -> B; B+0x140 -> C; C+0x34 -> D; D+0x18 -> matrix`. Other chains reach the same matrix, so it is shared by several owners.
- The owner confirmed that the character really moved on screen when writing the position there (as a teleport, not as a walk).

Derived (not yet verified in a second session): the offsets `9C, 140, 140, 34, 18` may be stable between sessions, since they come from object layout, not from addresses.

Probe update: `follow <id> <hex offsets...>` follows the chain and logs the matrix position next to the entity position; `tp <id> x y z <hex offsets...>` writes the position at the chain end. Built and deployed, not yet run.

## The pointer chain was not stable; the physics body is (2026-10-19)

Observations (ran, second and third session, the same save):
- `entity+0x9C` points to an array of pointers, not to a component. Following `+0x140` links gave different object kinds in the new session (one step landed on a record that holds a table name string), so the chain `9C,140,140,34,18` from the earlier session is **not** stable. `follow` reported "chain broken at step 3".
- `Dunia.dll` contains RTTI names and the Havok physics classes (`hkpRigidBody`, `hkpCharacterRigidBody`, `hkpCharacterProxy`, `hkpCapsuleShape`). The object found earlier (`3C229F80` in session 2) had vtable `10E67658` (RVA `0xE67658`, static). Its layout matches `hkpRigidBody`: `+0` vtable, `+4` ref count/size, `+8` world pointer, `+0xC` user data, `+0x10` shape, `+0x18` the collidable's pointer to the motion transform.
- In the third session, a memory scan for dwords equal to `10E67658` found exactly **one** instance (`3B183DC0`). Its `+0x18` points to `3B183E60`, an identity rotation followed (at `+0x30`) by the position `(2806.967, 2886.745, 17.258)`, equal to the entity position read by `pos`. The next row holds another position (earlier value or target).
- So the rule "find the instance of the class with vtable RVA `0xE67658` whose motion matrix is near the entity position" worked in two sessions.

Derived: the player's rigid body can be found by class and position; writing its motion matrix translation moves the player (confirmed earlier at the heap address; the owner saw teleports).

Unknown: whether NPCs use the same class (here only one instance existed, so probably not); how to give a second pawn its own physics body; what the world pointer at body+8 offers (iterating all bodies).

Probe: `findbody <id>` scans for the class and caches the matching body, `posb` reads it, `tpb x y z` writes it. Built, not yet run in a session.

## findbody / tpb verified in a fresh session (2026-10-19)

Observation (ran, owner in the world): `findbody` found exactly one body (`3A115AC0`, matrix `3A115B60`) near the entity position; `tpb` to +5 m in x moved the entity to that position (`pos` and `posb` agreed, z settled from 17.3 to 17.27), and `tpb` back restored it (z 17.259). This is the third session in a row where the class `RVA 0xE67658` identified the player's body; the heap addresses were different each time.

## Own UDP transport between two copies: position exchange works (2026-10-19)

Setup: the probe opens its own non-blocking UDP socket (loopback, ports 9101 and 9102, not the engine ports 9000-9004; the engine's imports are hooked but the probe's own calls are not). Each copy sends a 28-byte packet (magic, sequence, position x/y/z from the player's rigid-body matrix, direction from the entity matrix row 0) about every 50 ms when its main loop runs (about 16 packets/s were observed) and polls the socket in the same main-loop hook. Command: `netstart <localport> <peerip> <peerport> <entity id>`; the entity id of the main character is the same in both copies because they load the same save.

Observations (ran, owner had both copies in the world, windowed, both running at once):
- After `netstart` on both, `rx` grew in both logs (about 16 packets/s each way, ages 0-63 ms) and each copy's `remote` equalled the other copy's `local` (A saw `2806.26 2886.47`, B's own position, and the other way round).
- A `tpb` teleport of the player in copy A by +6 m in x showed up in copy B's `remote` within the next log line (`2812.10 2886.62`), and teleporting back showed `2806.11` again.
- Two copies run in the world at the same time without a crash for the whole test (about 2 minutes).

Derived: for a position-only prototype, the transport and the read side are done. What is missing is the remote avatar in the receiving copy: a second pawn that can be moved without the engine's AI or physics moving it back, plus animation, weapons and health.

## A second pawn can be spawned, and moved, without the AI interfering (2026-10-19)

Observations (ran, copy A only, owner in the world, standing):
- `SpawnEntityFromArchetype(name, x, y, z, rx, ry, rz)` (seven arguments: a string and six numbers; found by reading the native argument checks, `FUN_10590da0`) returns the new entity id; an unknown name returns `18446744073709551615` and `IsEntityValid` is false.
- Passing the player's own entity name (`GetEntityName(GetLocalPlayerId())`, here `player.MainCharacter.PawnPlayer.<character>`) as the archetype name created a second, valid entity at the given position (`2810, 2886`, z 17.275 after settling). `GetLocalPlayerId()` stayed the same.
- The new pawn has a rigid body of the same class as the player's (vtable RVA `0xE67658`); `findbody` found exactly one match near it.
- The pawn stood still for 4 s (no AI drift) and `tpb` moved it to four corners of a 4 m square (positions read back matched within 0.4 m, z within 0.3 m). Whether the owner saw it move on screen is pending.
- The buddy system (`GetBuddiesManager():SpawnBuddy`) was not usable at this point of the campaign: `GetBuddyList()` has two empty entries and no buddy is unlocked.

Probe update (built, not yet run): `findbody` takes the nearest match instead of the first; `lua_eval` runs Lua from C and returns `__out`; once the first packet from the peer arrives, the receiving copy spawns an avatar at the peer's position, binds its body and moves it each frame towards the received position (exponential smoothing 25 % per frame, snap above 6 m); `netstop` removes the avatar with `RemoveEntity`.

## Owner's view of the spawned avatar (2026-10-19)

Observation (owner, on screen, copy A, avatar moved by `tpb` about 4 times per second along a circle of 2-2.5 m radius): the second character was visible, it moved around the circle by jumps (teleports), and each time it appeared slightly above the ground and then fell, over and over.

Derived: the avatar is a real, rendered pawn. The jumps come from the low update rate (the test wrote four positions per second) and the falling from the physics world applying gravity between two writes (I had put the avatar 5 cm above the player's z). Writing the position on every frame of the main loop (about 140 per second) should hide both; to be checked with the per-frame smoothing built into the probe.

## Avatar over the network: first look (2026-10-19)

Observation (ran, both copies in the world at once, owner looking at copy B only; copy A's player moved by `tpb` along a 4 m circle, about 4 steps per second):
- Both copies spawned an avatar of the peer on their own after the first packet, then bound its body after a few retries (the body appears a few seconds after the spawn): `avatar spawned`, `findbody` 0 matches several times, then 1 match, `avatar body bound`.
- Owner: the avatar was visible in B and followed A's circle; it moved by teleports, did not fall or bounce as before, and stood in an idle pose (no walking animation).

Derived: the per-frame write fixed the falling. The teleport-like look may come from the test itself (the source player only changed position 4 times per second), not from the receiver. The idle pose is expected, since only the position is written; locomotion animation needs another input (velocity or controller).

Probe update (built, not yet run): `walk <radius> <seconds> <speed>` moves the local body on a circle every frame (to test smoothness with a continuous source); `avvel x y z <hex offset>` and `bodyvel x y z <hex offset>` write three floats into the avatar's or the cached body at an offset (to test whether a velocity field drives movement and animation).

## Copy B crashed after the avatar spawn (2026-10-19)

Observation (ran): in one of the two-copy runs copy B quit right after `avatar spawned` (the log of that process ends there; no `avatar body bound`, no further `net:` lines). The owner confirmed the game crashed and restarted it. The Windows application log had no entry. The earlier run with the same probe version (the avatar spawn path) did not crash in either copy.

Derived (hypothesis, not proven): `findbody` walked every committed read-write region with direct reads. If another engine thread frees or decommits a page between `VirtualQuery` and the read, that read faults and kills the process; the avatar's body only appears a few seconds after the spawn, so the scan runs several times right when the engine is busy creating it.

Probe update: `findbody` now copies memory in 64 KiB chunks with `ReadProcessMemory` (a failed read skips the chunk instead of faulting), and an exception logger writes any access violation (code, module and offset, address) and a final crash record to `coop_probe.log`. Built, not yet run in both copies.

## "Game over" after the continuous walk (2026-10-19)

Observation (ran, both copies in the world with their avatars, no crash this time): the exchange worked (about 16 packets/s both ways) and both avatars were created and bound. Then `walk 4 40 2.5` made copy A's player move on a 4 m circle by per-frame writes of its rigid-body matrix at fixed z. The owner then saw the "Game over" screen in **both** copies. Entity validity checks (`IsEntityValid`, `IsEntityLoaded`) were still true for both players and both avatars, so the entities were not removed.

Derived (hypotheses, not proven): the fixed z does not follow the terrain, so the pinned body is pushed into or held above the ground and takes fall or penetration damage; in A that is the local player, in B it is the avatar of A (which receives the same z). The game seems to treat the avatar, created from the main character's archetype, as a main character, so its death ends the mission for both.

Probe update (built, not yet run): after the avatar spawn, the probe sends `CFCXCountersComponent_SetInvincible` to it (the event the scripts use for `PawnInvincibleState`, via `CDominoManager_GetInstance():SendRegisteredEventToEntity`).

## Smooth avatar, no Game over with invincibility (2026-10-19)

Observation (ran, both copies in the world; local player of copy A made invincible by hand, both avatars set invincible by the probe right after the spawn; copy A's player moved by per-frame writes on a 3 m circle at 2 m/s for 30 s): the owner watched copy B: the avatar of A rode the circle smoothly, no falling or bouncing, and no Game over in either copy.

Derived: the earlier Game over was a death (damage) of a pinned pawn, not an effect of the avatar or of the network layer. Smooth motion needs per-frame writes plus the smoothing (about 140 updates/s from the main-loop hook, 16 packets/s from the peer).

## Layout of the pinned avatar's body (2026-10-19)

Observation (ran, copy B, avatar of A moving on a circle by per-frame writes; 400 bytes from body+0x80): the rigid body at `3911C5C0` has the motion transform (rotation rows, then position) at `body+0xA0`; the two 16-byte rows right after it (`+0xE0`, `+0xF0`; value at `+0xD0` is the current position) still hold the position where the avatar was **spawned** (`2799.274 2877.071 17.031`) while the transform holds the current one, so the physics world's "swept" copy of the motion is stale. The bytes after `body+0x180` look like packed or encoded data, not floats; no obvious velocity vector was found in this block.

Unknown: where the character's velocity, the animation input and the controller input live; the engine's own remote-player path (`NetObject.PawnThirdParty`, `PawnHost`, `PawnController`) probably feeds them. The avatar currently stands in an idle pose while sliding.

## Walking through a tree, other archetypes, and where heading and animation might come from (2026-10-19)

Observations:
- Owner: during the `walk` test the moving character passed through a tree. Expected and by design of the test: the position is written into the rigid body every frame, so collision is bypassed. In normal play the peer's real position already includes its collisions, so a puppet avatar needs none.
- `gamemodesconfig.xml` (`PlayerService`) lists thirteen playable pawn archetypes: `player.MainCharacter.PawnPlayer` and `player.MainCharacter.PawnPlayer.<Andre_Hyppolite|Flora_Guillen|Frank_Bilders|Hakim_Echebbi|Josip_Idromeno|Marty_Alencar|Michele_Dachss|Nasreen_Davar|Paul_Ferenc|Quarbani_Singh|Warren_Clyde|Xianyong_Bai>`, and the multiplayer ones (`player.MainCharacter.PawnPlayerNetworkKit...`).
- Spawning `player.MainCharacter.PawnPlayerNetworkKit` in the single-player game returned the invalid id (not available in this game mode). Spawning `...PawnPlayer.Frank_Bilders` worked (valid id) and, per the owner, looked like the same character as Marty ("stands next to me and looks the same").
- `fcx.netobject.xml` / `engine.netobject.xml` define the engine's own pawn replication: `NetObject.PawnController` (a `CPawnControllerMemento` state snapshot, plus messages: reload, shoot, fire_bullet, enter vehicle, use equipment, process landing ...), `NetObject.PawnHost` (`CPawnHostMemento`, inventory ammo and equipment mementos) and `NetObject.PawnThirdParty` (`CPawnThirdPartyMemento`), driven by `CPawnNetworkComponent`.
- Heading: the entity's direction (row 0 of the matrix at entity+0x30) changed only from `(0.986, -0.164)` to `(0.967, -0.253)` when the owner turned the camera by about 90 degrees in place, so the pawn's body heading does not follow the camera while standing. A two-scan search on the heading pair gave 53 addresses common to both scans; a brute-force write test over them was run with a scripting bug (a function named like the `Dir` alias), so it measured nothing and, for 244 candidates, left the test value (0, 1) in memory until the engine overwrote it; copy A kept running normally.

Derived: position can be pushed from outside (done), but heading and the animation state live in the pawn's own components, and the cleanest source of truth is the layout of `CPawnControllerMemento` and `CPawnThirdPartyMemento`. Reading their serialization code in Ghidra is the next concrete step. Collision of the puppet is not needed.

## Pawn memento classes in Ghidra (2026-10-19)

Method (static, Ghidra on the Steam v1.03 `Dunia.dll`): cross-references of the class names `CPawnControllerMemento`, `CPawnThirdPartyMemento`, `CPawnNetworkComponent`, then the class registration function `FUN_10044de0` (name + factory pairs), the factories and the vtable of the controller memento.

Observations:
- The names are registered with factories: `CPawnControllerMemento` -> `FUN_10040090` (allocates `0x130` bytes, constructor `FUN_100b1fe0`, vtable `10E176C8`); `CPawnThirdPartyMemento` -> `FUN_10040110` (allocates `0x278` bytes, constructor `FUN_100b3ef0`). Class descriptor getters are `FUN_100b2080` and `FUN_100b3a70`.
- The controller memento is built from about eleven typed sub-objects at offsets `0x70, 0x84, 0x98, 0xac, 0xc0, 0xd4, 0xe8, 0xfc, 0x108, 0x114, 0x120`; its serialize method (`vtable[2]`, `100b2000`) calls the same virtual function (slot 1) on each of them. The sub-object types decide what the fields are (not decoded yet).

Derived: the pawn's synced state (heading, locomotion, stance) is stored in these typed net fields and fed by `CPawnNetworkComponent`; using it from outside means either running the engine's own network layer in single-player (the `NetworkEngine` session path, which also needs the dead Ubisoft services or a local replacement) or finding which fields in the pawn's components carry heading and speed and writing them every frame. Both are larger efforts than the position puppet; no cheap shortcut was found.

State at the end of this step: the position puppet works (smooth, invincible, auto-spawned, both directions) but has no walking animation, no heading and no weapons.

## Walking animation: where the locomotion data lives (2026-10-19)

Method (ran, read-only, in the live test copy): snapshots of the pawn's pointer graph (breadth-first from the player's entity object, depth 4, first 0x200 bytes of each object, about 35,000 to 39,000 objects, 18 to 20 MB each) taken twice while standing and twice while the owner held W; then the fields that are equal in both standing snapshots, equal in both walking snapshots, and different between the two states. Tool (private): `local/scripts/snapgraph.ps1`.

Observations:
- 113,098 fields met the rule (the heap churns); the readable ones cluster in a few objects. The one reached by the shortest chain is `entity+0x9C` (depth 1). At its `+0x110..0x118` there is a 3-float vector that reads about `(0.003, -0.032, 0.066)` standing and `(2.72, 0.59, 1.76)` walking; at `+0x120..0x134` a box that reads `(99999, 99999, 99999, -99999, -99999, -99999)` standing (an empty box) and real numbers walking; at `+0x1E0..` a block that changes type between the states (text-like standing, floats `-0.11, -0.01, 1.40, -0.18` walking). Other objects (reached at depth 3 to 5) show 0/1 flag sets that flip between the states.
- The same chain on a script-spawned avatar leads to a different kind of object (different values and layout; coordinates of other things). So `entity+0x9C` is not a fixed "movement component" slot for every pawn; the avatar made from the player archetype has none of the controller-side data the real player has.
- Multiplayer pawn archetypes found in the config (`player.MainCharacter.PawnPlayerNetwork.RedVIP`, `...PawnPlayerNetworkKit`, `.Agent`, `.Assault.Level1`, `.CQB.Level1`) all returned the invalid id when spawned in the campaign. Only the `PawnPlayer.*` archetypes spawn. No AI pawn archetype name appears in the text of the unpacked config or Lua (level entities are named `Pawn.Enemy.C` and so on, which are instance names, not archetypes).
- Writing the walking vector into the avatar was not tried: its object does not have that layout.

Derived: locomotion data belongs to the controller side of a player-controlled pawn, which a script-spawned `PawnPlayer.*` pawn does not get. Options left: (a) find an AI pawn archetype and let the engine's own `MoveTo` command walk it to a moving target (natural animation and collision, but lag and AI behaviour); (b) decode `CPawnControllerMemento` and feed the engine's third-party path; (c) accept an idle-pose puppet. None was attempted.

## Second PC: profile patch correction, two copies, avatar body fix (2026-10-24)

Identity (second PC): Steam, app 19900, build id 52164, `PatchNotes.txt` "v1.03"; `Dunia.dll`
SHA-256 `6353936a...2765`, 20,183,176 bytes (identical to the file above); Windows 11 23H2, 64-bit.
Two test copies of the install; the owner's install and profile were only read. Probe built from
fc2-coop-probe `583772f` with winlibs i686 gcc 9.5.0 (no hashed 9.2 build available; the 9.5.0
archive matched its published SHA-256 and SHA-512). Before every launch a file snapshot of the
owner's profile folder and both test profile folders was taken, and a kill switch watched the
probe log for the owner's profile path.

Observations (all ran):
- With the documented patches only (0xE08950 mutex, 0xE089A0 `My Games\Far Cry 2\` →
  `My Games\FC2test_A\`), the probe log showed `CreateFileW <Documents>\My Games\Far Cry 2\GamerProfile.xml`
  (read, open-existing): the game opened the owner's real profile. Copy killed after ~45 s; no file in any
  profile folder changed.
- `Dunia.dll` has separate strings next to `CGamerProfileNative`: 0xE0E080 `\GamerProfile.xml`,
  0xE0E094 `\Far Cry 2` (10 bytes + NUL), 0xE0E0A0 `\My Games`. The ASCII `My Games\Far Cry 2\` at
  0xE089A0 is the only other ASCII occurrence; the 12 UTF-16 ones are in `.rsrc`, inside XML with
  `KnownFolderID=... path="My Games\Far Cry 2\Saved Games"` (Game Explorer metadata, by the look).
- After also patching 0xE0E094 to `\FC2test_A` / `\FC2test_B` (17 bytes differ from the original per
  copy): copy A opened `FC2test_A\GamerProfile.xml`, then on Continue `FC2test_A\saved games\<save>.sav`;
  copy B the `FC2test_B` ones. Window 800x600 as set in the test profiles. Both copies ran at once.
- This explains the 2026-10-19 observations above (window 1408x792 instead of 800x600; "whether the
  new profile path is read at all" unknown): the 0xE089A0 patch never moved the profile.
- `GetLocalPlayerId()` returned the same id in both copies after loading the same save.
- `netstart` on both: about 16 packets/s each way (log every second, e.g. `tx=694 rx=697` after ~44 s),
  each copy's `remote` equal to the other's `local`.
- Avatar binding bug: both copies logged `avatar body bound` with the avatar body equal to the local
  player's body (A `3bd8e940` both, B `3bd48a40` both). Both players stood on the same spot after
  loading, so the nearest body to the freshly spawned avatar was the local player's. Stopped there
  (no movement test with the bug).
- With the fix (avatar search skips the local body, waits until it is known; `av_valid` rejects
  equality): A avatar `3babb100` vs local `3bb54580`, B avatar `3ba96980` vs local `3bd22900`. The first
  avatar search found 0 matches and succeeded about 1 s later. `walk 3 20 2` in A: B's `pos <avatar id>`
  every 2 s followed A's circle 0.5-1 m behind (e.g. remote `2589.02 2456.56` → avatar `2589.62 2456.96`);
  B's `pos <player id>` stayed within 0.1 m (the same drift was there before the walk).
- After the walk A's body rose from z 18.49 to 19.29 and settled.
- The avatar's `dir` stayed `(1,0)` the whole time.
- No CRASH or ACCESS VIOLATION lines in any of the runs; after every run all profile folders matched
  the snapshot taken before it.
- The first start of the second copy raised a Windows Firewall prompt for its `FarCry2.exe`
  (the engine binds UDP 9000/9001); the owner answered it.
- Code reading: the `scan` command still dereferences memory directly (only `findbody` uses chunked
  `ReadProcessMemory`); `setpos` exists but is not in the probe README; `ent`, `pos`, `setpos` never drop
  the entity holder's reference count.

Derived:
- The profile folder and the save folder both come from 0xE0E094.
- The avatar bug only shows when both players start within 1 m of each other; the 2026-10-19 runs had
  moved one player first (teleports, `tpb`) and did not hit it.

Unknown:
- What the 0xE089A0 string is used for.
- Whether new saves (autosave) also go to the test folder: no save was written in these runs.
- Why the walked body rose 0.8 m after `walk` ended.
## Automated two-copy walk test (2026-10-24)

Script `tests/two_copy_walk.ps1`, added to the probe in https://github.com/MortyRei/fc2-coop-probe/pull/1.
What it asserts is listed in the README's QA section.

Observations (ran, same PC and copies as the section above):
- Probe with the avatar-body fix (`dinput8.dll` SHA-256 `87cb733b...431c`): 16 checks, 0 failed, about 2 minutes
  from start to kill. Copy A needed 8 Enter presses to reach its save, copy B 3. Avatar bodies differed from the local
  ones in both copies; 16.0 packets/s each way; 17 avatar samples during the 20 s walk, all paired with a `remote`
  position; avatar span 5.92 m; distance to A mean 0.43 m, max 1.07 m; B's own player drift 0.12 m; no crash lines;
  real and test profile folders unchanged.
- Negative control with the probe before the fix (`22ac2fea...8001`), same script: 3 checks failed. `B player stays
  put`: max drift 16.46 m. `avatar body A/B`: the old log line has no pointers, so it cannot be compared.
  `avatar moved` (span 16.01 m) and `avatar follows A` (mean 0.19 m) passed.

Derived: with the bug both players are written to each other's position, so they move together (hence the 16 m
drift and an "avatar" that seems to follow); only the check that the receiving copy's own player stays put tells the
two cases apart.
## Heading, network mode, Steam relay (2026-10-24, later)

Observations (ran; one PC unless stated):
- The player's physics body matrix is always identity (upright capsule) while the entity direction changes; mouse
  look (relative `mouse_event`) turned the player: entity dir (-0.640, 0.768) -> (-0.913, -0.409), body unchanged.
  Player entity rows: (d0, d1, 0), (-d1, d0, 0), (0, 0, 1).
- On a script-spawned pawn: writing its body rotation sticks in the body but the entity dir stays (1,0);
  `TeleportEntity(id, x, y, z, a, b, c)` returns and changes nothing (neither rotation nor position);
  `SpawnEntityFromArchetype(..., 0, 0, 90)` gives dir (0,1) (last argument = yaw in degrees; radians or other axes
  do nothing); writing the pawn's entity rotation rows sticks (still (0,1) 3 s later).
- Probe change: the avatar's entity rotation rows get the received direction every tick (25 % smoothing).
  Two-copy test with a mouse-turn phase: 18/18, A and B's avatar at 128.1/128.1, -145.4/-145.4, 149.8/149.8 deg;
  without the change: avatar stayed at 0 deg (1 check failed).
- Owner's view (both copies side by side): B's avatar turned with A, also when the owner turned A by hand.
  My own screenshots never caught the avatar: the opening save's dialogue camera pans by itself; a pawn spawned
  outdoors with a later save was not visible either, and an unpinned pawn moved by `tpb` fell through the
  terrain (z 108.9 -> 63.0).
- Network mode: peer not 127.x -> bind 0.0.0.0, only packets from the peer's IP count (`drop` counter). Test with
  the PC's own LAN address: 19/19; 30 forged packets from 127.0.0.1 dropped.
- Relay (`relay/fc2_relay.py`): udp stand-in and Steam (Spacewar, 64-bit ctypes over the flat API of
  `steam_api64.dll` from Steamworks.NET Standalone 2025.164.1, SHA-256 eb17909a...fb75). Test through two udp
  relays: 22/22. Steam host side on one account: init, friends-only lobby created and left.

## Two PCs over Steam (2026-10-25)

Setup: the first PC (host) and a second PC on a different network (Wi-Fi vs Ethernet, no ping between them), each
with its own Steam account, the accounts friends; the second PC had never run the game. Same `Dunia.dll`
(6353936a...2765) on both. The second PC was prepared from a kit (probe build, relay, scripts) plus the shared
save and a test profile passed by hand.

Observations (ran):
- First attempt: the second PC's copy was patched at 0xE08950 and 0xE0E094 only. It read its own
  `FC2test_B\GamerProfile.xml`, but Story Mode offered only New Story; the auto-Enter run never opened a save.
  The first PC's copies also had 0xE089A0 patched (`My Games\FC2test_A\`). After patching 0xE089A0 on the second
  PC its `Dunia.dll` matched the first PC's copy with the same three patches (SHA-256 e0fef7c2...) and the save
  was listed and loaded (`CreateFileW ...\FC2test_B\saved games\222368278550.sav`).
- Second attempt: host in the world and lobby created 13:05:54; the joining relay found no lobby on its first try,
  found the friend in a Spacewar lobby on the second (13:05:56), entered it, the session was accepted, and the
  first message decoded with the sender identity at offset 16. Avatars spawned on both sides (bound bodies differ
  from the local ones).
- Packets: about 16/s each way for ~7 minutes, dropped=0 in every relay stats line on both sides, peer never
  down while both ran (joining side final tx 6711 / rx 6624).
- `walk 3 20 2` on the host at about 13:06:40. Joining side, `pos <avatar>` every 2 s: about two circles between
  ~13:06:58 and ~13:07:19, centre about (2587.3, 2459.2), radius about 3 m; the walk's centre is the start point
  minus the radius in x, (2587.36, 2459.24), and 2 m/s x 20 s = 40 m = 2.1 circles of 18.85 m. The joining PC's own
  player stayed within about 0.1 m. The avatar's dir stayed (-0.989, 0.151), equal to the host player's: `walk`
  does not turn the player.
- Owner (by eye): the avatar moved on the first PC's screen, and on the second PC it went round in a circle.
- After the joining side stopped (13:13:00), the host's relay showed peer=down after its 20 s timeout.
- No CRASH / ACCESS VIOLATION lines on either side; neither probe log named a real profile folder; the test
  profiles were unchanged (the game saved nothing).

Derived:
- 0xE0E094 decides the profile and the save file that is opened; 0xE089A0 decides the folder the save list is
  built from. The 2026-10-24 statement that 0xE089A0 is not needed was wrong.
- The Steam relay carries the probe's traffic between two PCs without IP addresses or port forwarding.

Unknown: heading across PCs (not exercised), invites and Join Game, behaviour over a longer session or with loss.
## Two PCs: heading, Steam invite, Join Game (2026-10-25, afternoon)

Same two PCs and test copies as above; host with `scripts/start-coop.ps1 -Role host`, joining side with the same
script (its relay patched with `--join wait`, byte-identical to the probe repository's).

Observations (ran):
- Heading: the host turned its player with relative mouse input, each direction held ~8 s: 171.3 (start), 81.3,
  -171.3, 77.6 degrees (clocks of the two PCs agreed to ~1 s: "peer is talking" at 14:38:05 on both). The joining
  PC's avatar, sampled every 2 s: 171.3 until 14:38:29, 95.7 at 14:38:31 (mid-turn), 81.3 from 14:38:33, -171.3 from
  14:38:42, 77.6 from 14:38:52; position steady within a few mm.
- Steam invite: the host relay logged "invite to <name> sent: True" when its lobby was created; the joining relay
  waited ("waiting for a Steam invite or Join Game ..."); after the owner accepted the invite on the second PC
  (about 3 minutes later) the relay logged callback 333 ("invite or Join Game accepted (lobby)"), entered the lobby,
  and the first packets and the avatar followed within 3 s.
- Join Game: a new host run without any invite created a new lobby; the joining relay waited; after the owner chose
  Join Game on the host in the Steam friends list, it joined that new lobby, again through callback 333; no 337 was
  logged. The earlier invite had pointed to the previous (closed) lobby, so this join came from Join Game.
- All runs: dropped=0 in every relay stats line, no CRASH / ACCESS VIOLATION lines, no real profile folder in
  either probe log; after a host stopped, the joining relay logged "peer silent for 20 s, dropped".
- No "accepted session" line in these runs: both sides sent their first message at about the same time, so each
  side's own send opened the session.

Derived: for a friend who is in a joinable lobby, Steam delivers Join Game as GameLobbyJoinRequested (333), the same
callback as an accepted invite; GameRichPresenceJoinRequested (337) would need a connect string without a lobby.