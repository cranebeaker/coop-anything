# Clive Barker's Jericho: working notes

## Identity of the tested build
- Store: retail disc (owner's old disc, reinstalled)
- `bin/Jericho.exe` FileVersion 1.0.0.1, PE timestamp 2007-09-21, SHA-256
  `7fb5b759823283a83b685adc4848289a149f520ddbab6dbc7f9e6131b15d2a62`
- 32-bit, D3D9 (d3dx9_31), PhysX (PhysXLoader, NxCooking), FMOD Ex, Bink, XInput 1.3, DInput8
- SecuROM: exe has a `.securom` section and SecuROM activation strings.

## Session 2026-10-18: triage (phase 0) and first research (phase 1)

### Observations (checked)
- Hub: no entry for Jericho in `DIRECTORY.md`, `data/directory.yaml`, `games/`, playbook.
  GitHub issues and PRs searched for "jericho": 0 results.
- Web: no existing co-op or multiplayer mod found. Players asking for one:
  - https://www.moddb.com/forum/thread/clive-barkers-jericho-multiplayer
  - https://steamcommunity.com/app/11420/discussions/0/846963710897727939/
  - https://gamefaqs.gamespot.com/boards/934444-clive-barkers-jericho/41226885
- Existing modding toolkit: SMOD (https://www.nexusmods.com/clivebarkersjericho/mods/2,
  page 403 to WebFetch; known only from search snippets).
- Engine is MercurySteam's ScrapEngine, the Scrapland engine: exe strings
  `.\ScrapEngMain.cpp`, `ScrapEngWorld.cpp`, Lua key binds for a "Racer" action set
  (Laser, Vulcan, Devastator, Swarm) in `scripts/quickconsole.lua`.
- **The exe contains a full client/server network layer** (Scrapland multiplayer):
  - source paths `Net\GameServer.cpp`, `Net\ManServer.cpp`, `Net\ManClient.cpp`,
    `Net\msgman.cpp`, `Net\NetData.cpp`; classes `cServerNetManager`, `cClientNetManager`
  - imports WSOCK32 UDP (`socket`, `bind`, `sendto`, `recvfrom`, `select`)
  - game vars: `DefaultServerPort`, `DefaultServerAddress`, `DefaultMaxPlayersOnServer`,
    `JoinServerOnStartup`, `ServerType`, `ServerMapList`, `ServerName`, `ServerPassword`,
    `ClientPPS`, `NetTimeOut`, `LogNetData`, `MasterServerAddress/Port`
  - command line handling strings: `server`, `dedicated`, `Init Server at %s`,
    `Using default server map` with `Levels/FZ` (a Scrapland map, not in Jericho data)
  - script callbacks: `OnClientJoin(...)`, `OnClientExit(...)`, `OnClientModify(...)`,
    `OnChatString`, `OnUsrString`; their signatures describe Scrapland ships
    (`PilotName, Motor0..3, WeaponBay`)
  - Lua `Net` library: `InitServer, CloseServer, IsServer, ServerChangeLevel, AddResource,
    ModifyUsrData, GetServerData, GetBotName, GetObjName, InitClient, CloseClient,
    IsClient, GetClientData, GetMyClientShip, InitBrowser, DoneBrowser, SendUsrString,
    SendChatString, SendCtrlString, SendMasterString, IsMaster, PingInetSvrs, GetPing`
- Game scripts barely use it: only `scripts/quickconsole.lua` (`QuickConsole.reload`)
  calls `Net.IsServer/IsClient/CloseServer/CloseClient`.
- Lua 5.1.1 (`$Lua: Lua 5.1.1 ...` in exe). Lua bindings: `LuaInterface\*.cpp`
  (Game, Act, AI, AIBehav, AITeam, FX, Input, Physics libs).
- Data archives `*.packed` are BFPK: `"BFPK"`, u32 version (1 for Data0x, 0 for Voices),
  u32 count, entries `u32 name_len, name, u32 size, u32 abs_offset`. Tool: `tools/bfpk.py` in the mod repo.
- Scripts are `*.lc`: u32 header (not the raw size; meaning unknown) + zlib stream of
  stock Lua 5.1 bytecode (`\x1bLuaQ`, int 4, size_t 4, instr 4, number 8) with debug info.
  1533 files; all decompile with unluac 2025-12-23 to readable Lua with local names.
- Game config: GVars written to `game.cfg` under `%APPDATA%\Codemasters\Clive Barker's Jericho`
  (format `name = value ### comment`); vars `ModFileName`, `ModPathName` ("Modification
  file/path name") exist; pack loader also knows a `.2BPACK` extension.
- Input layer supports pads 0..3 (`pad>=0 && pad<4`) and has `ResetToSplit`.
- 34 levels under `levels/` (alk_*, cru_*, rom_*, sum_*, wwii_*, menu), each with its own
  `scripts/` folder (map, triggers, enemies, scenes, physics).

### Derived
- Co-op is not "build netcode from zero": a working UDP client/server with entity creation,
  user descriptors, ping, chat and level change probably still runs inside the exe. The
  work is making Jericho's characters, AI, triggers and scenes run on it, plus fixing what
  was cut. This matches the triage row "engine has MP plus script source".
- Jericho is a squad game where the player switches between squad members, so a natural
  co-op design is "each player controls one squad member, AI keeps the rest".

### Guesses (unverified)
- The net layer may be partly stubbed out in Jericho; replication may only know Scrapland
  entity types (ships).
- `server` / `dedicated` on the command line may start a host directly.
- `ModPathName` may let the game load loose or extra packed files (mod loading without
  repacking Data0x).

### Unknowns
- Store; whether SecuROM in this build needs online activation.
- Whether the game runs on Windows 11 from the test copy.
- Whether the net layer actually starts, connects, and what it replicates.
- Meaning of the `.lc` u32 header; whether the engine loads plain `.lua` source.

## Session 2026-10-18: first launch (test copy, Windows 11)

### Observations (checked)
- Launch 1 crashed at start, log.html: `FATAL ERROR: Check failed! physicsSDK && "Wrong PhysX
  SDK DLL version?"` (`World\WorldPhysics.cpp` line 1587). Game ships `NxCooking.dll` 2.7.0.3,
  so it needs PhysX core 2.7.0. Installed cores at that point: 2.7.1 to 2.8.4 only.
- Owner then installed NVIDIA PhysX System Software 9.12.0613 from the game disc. After that
  `%ProgramFiles(x86)%\NVIDIA Corporation\PhysX\Engine\v2.7.0\PhysXCore.dll` (2.7.0.9) exists.
- Launch 2: window "Clive Barker's Jericho" still running after 40 s; owner confirms it
  started. log.html stays empty while the game runs (flushed on exit?).
- Community fix for the same crash: PhysX legacy 9.13.0604 or 9.12.x
  (https://steamcommunity.com/app/11420/discussions/0/846945955298387706/).

## Session 2026-10-18: first network experiments (test copy)

### Observations (checked)
- Command line syntax (disassembled tokenizer at 0x402ae0): tokens `-Name:value`, separated by
  spaces, `"` or `'` quote values. Each pair goes to handler 0x402ce0: if `Name` is a GVar it is
  set (so any `game.cfg` var can be overridden at launch), else `build`, `dedicated`, `server`.
  `-server` / `-dedicated` call InitServer during command line parsing and crash with
  `Check failed: LangTabl` (`Global\Language.cpp:29`): too early, language table not loaded.
- Useful GVars: `FullScreen` (0 = windowed), `VideoWidth/Height`, `SelectedLanguage`,
  `debug` (0..3, log verbosity), `OutDebugFile` (console log to a text file), `InitLuaLine`
  (Lua run after init.lua, then cleared), `iMap` (first map, default `Levels/Menu`),
  `LogNetData`, `ShowNetStatus`, `SoundMainVolume/MusicMainVolume/VoiceMainVolume`.
- `-InitLuaLine:"print('COOP_INIT_OK '..tostring(Net.IsServer()))"` printed `COOP_INIT_OK 0`:
  arbitrary Lua runs at startup without touching game files.
- Pack loader also tries `<ModFileName>.packed`, `English.packed`, `Voices.packed`,
  `Pack1.packed` (log lines `Pack: X not found`): extra archives can be added next to the
  originals.
- `SelectedLanguage` written by the game on this PC is `Русский` (cp1251), but only
  english/french/german/italian/spanish text exists, so the game shows the language picker.
  Tests pass `-SelectedLanguage:English`.
- Missing `Video/*.bik` are skipped with `ERROR: video file not found` (videos moved to
  `Video.disabled` in the test copy only). Without that the Prologue video blocks the window.
- Lua `Net` bindings (luaL_Reg table around VA 0x72e4f8), args via format strings:
  - `Net.InitServer(map:string, maxPlayers:int, port:int) -> 1/0` (0x53b3e0, fmt `"sii"`,
    calls 0x482030; first test with `(map, 28086, 4)` opened UDP port 4)
  - `Net.InitClient(address:string, port:int) -> 1/0` (0x53b790, fmt `"si"`, calls 0x481a30)
- `Net.InitServer('Levels/Alk_Desert',4,28086)` from InitLuaLine: log `Starting server at ...`,
  UDP 0.0.0.0:28086 open, then Alk_Desert loads and its scripts and intro scene run as server.
- Second instance shows "Session detected / Clive Barker's Jericho is already running."
  Check is `FindWindowA("MercuryGameClass", NULL)` inside SecuROM-obfuscated code (0x5820b0).
  - Separate hidden desktop (CreateDesktop) avoids the check but D3D fails there:
    `Could not find any compatible Direct3D devices.`
  - Works: test-only `dinput8.dll` proxy in `bin/` (`native/` in the mod repo) that, only when env
    `JERICHO_MULTI=1`, makes FindWindowA return NULL for that class. Exe untouched.
- Client `Net.InitClient('127.0.0.1',28086)` from the menu: `Try to server`, `COOP_CLI 1`,
  `'Ready to build' message send to server`.
  - Run 1 (client started 45 s after server): server never logged a client.
  - Run 2 (client started 80 s after server, LogNetData=1): server logged
    `Creating new client!` and then crashed: `Check failed: newpos<=size`
    (`FileSys\FileMem.cpp:46`). Client stayed in the menu.

### Derived
- Transport and handshake still work in the retail exe. The first blocker is the server
  building the initial state for a new client: a fixed-size memory file overflows, probably
  sized for Scrapland levels (`NetMaxResourcesSend`, `NetMaxPlayersInitSend` vars and
  `Global/StaticBuffer.h` are candidates).

### Unknowns
- What the server serializes for a new client and how big the buffer is.
- Whether `NetMaxResourcesSend` / `NetMaxPlayersInitSend` change that size.

## Session 2026-10-18 (evening): own transport, Lua bridge, puppets

### Observations (checked)
- Engine net layer abandoned: the server crash `newpos<=size` (FileSys\FileMem.cpp:46) is the
  server parsing the client's join packet (Net\ManServer.cpp around 0x4839ac): it expects
  NET_GAME_ID 0xCEBA, NET_GAME_VERSION 0x101 and a Scrapland ship descriptor (strings at
  +0x8c..+0x40c, u16 +0x490, u32 +0x48c). The client builder lives in the SecuROM section.
  Found with a stack dump taken from a GetLastError hook (the Check failed handler at
  0x404f00 calls GetLastError first).
- Lua 5.1 C API inside Jericho.exe (cdecl, stock signatures): lua_pushnil 0x662770,
  lua_pushnumber 0x662790, lua_pushlstring 0x6627d0, lua_pushstring 0x662810,
  lua_pushboolean 0x662940, lua_tolstring 0x663e90, lua_tonumber 0x663f30,
  lua_toboolean 0x663f80, lua_tointeger 0x663ff0; lua_State top at +8, base at +0xc;
  global lua_State* at 0x7cd390; luaL_dostring-like 0x663cb0. Engine arg helpers:
  getargs 0x661fe0 (fmt b d f i s v |), pushret 0x52df90 (fmt b d f i s v).
- `Net` luaL_Reg table at 0x72e4f8 (24 entries). The mod's dinput8.dll rewrites 8 unused
  entries to Net.CoopHost/CoopJoin/CoopSend/CoopRecv/CoopStatus/CoopClose/CoopTime/CoopLog
  (ENet 1.3.18 on a background thread). Two instances exchanged ping/pong, RTT 2-5 ms.
- InitLuaLine runs once per Lua init via 0x663cb0 and is cleared right after (code in
  relocated section around 0x481548c). Re-arming it from a scheduled Lua function makes
  the mod run on every level. `dofile` works with absolute paths to loose files; `io`/`os`
  are absent; `debug`, `coroutine`, `loadstring` exist; Game.AddScheduledFunc drives a 20 Hz tick.
- Characters: `UsrControl` 0 = user 0's input, -1 = AI side; `oAI.bEnable` toggles AI.
  With AI off and UsrControl -1, writing `InCtrl_Walk`/`InCtrl_Str` (and `Ang`) makes the
  body run with its own animation (Action=Run). `UsrControl=1` is accepted
  (`Game.UsrEntity(1)` returned the body) but device input then overwrites InCtrl.
  For the local user body, `IgnoreUsrControl=1` lets a bot drive it through InCtrl.
- Heading convention: Ang.y = yaw (degrees); forward = (-sin yaw, -cos yaw) in x/z
  (Black at yaw 113 ran along (-0.92, +0.39)).
- Two-instance test in rom_tepidarium (host Rawlings, client got Black): client followed
  the host to the level, switched to Black, its bot ran in circles; on the host the Black
  puppet ran with err 23-140 units and periodic snaps; on the client the host-driven squad
  puppets stayed within 0-45 units.
- INCIDENT: one later two-instance run froze the owner's desktop (both logs stop at the
  client's `Engine 3D initializing`, no TDR in the event log, kernel alive); hard reboot.
  New rule: one game instance + headless bot (tools/bot/coop_bot.py) by default; two
  instances only with the owner's OK. Owner then enabled the integrated GPU and put
  the second monitor on it.

### Unknowns
- Whether D3D9 here can be told to render on adapter 1 (strings `AdapterInfo`,
  `Render Last Device name` exist), or whether Windows GPU preference applies to it.
  -> answered below: an IAT hook on Direct3DCreate9 + vtable hook on CreateDevice does it.

## Session 2026-10-18 (night): safe two-instance testing, background mode

### Observations (checked)
- `OnRenderDebug` GVar names a Lua function the engine calls every rendered frame, also
  when game time is slowed (10 s real time: 297 calls at normal speed, 301 at
  SetTimeSpeed(0.05)); `Game.AddScheduledFunc` follows game time (150 vs 9 ticks). The mod
  runs from OnRenderDebug.
- Proxy DLL display hooks (exe import table + COM vtables, env driven):
  `JERICHO_WINPOS=x,y` creates the window there (log `window 0,0 -> 1940,20`);
  `JERICHO_ADAPTER=window` creates the D3D9 device on the monitor's GPU
  (log `CreateDevice asked=0 used=1 (<integrated GPU> ) hr=0`), the game runs fine.
- The game pauses when its window is inactive (owner saw frozen unfocused windows; host
  Lua stopped after the client took focus). `JERICHO_BACKGROUND=1` subclasses the window
  to swallow deactivation and fakes GetForegroundWindow/GetActiveWindow. Because the game
  polls GetAsyncKeyState and warps the cursor with SetCursorPos, background mode also
  zeroes GetAsyncKeyState, ignores SetCursorPos/SetForegroundWindow/SetFocus/
  BringWindowToTop and empties DirectInput GetDeviceState/GetDeviceData while the window
  is not really in front.
- Headless bot replaying a path recorded in-game (dev 'wander' bot, collision-respecting):
  host puppet err_avg 11-24 units, 1 snap in 70 s (vs 31 snaps, err_avg ~55 for a bot
  running through walls).
- Two real instances (host on the dedicated GPU, client on the integrated GPU, both windows on the
  secondary monitor, background mode, BelowNormal, R_LimitFPS 30): 60 s without problems.
  Host puppet of the client's Black: err_avg 13-23, max 55-127, 0 snaps. Client puppets of
  the host-driven squad: err 0-10.

### Derived
- Stage 2 of the playbook build order ("make a second player visible") passes in
  rom_tepidarium. Enemies still run independently on each machine (next: host authority).

## Cutscene POV camera (2026-10-18)
- `FCam.Ang` is set in RADIANS, while `camAng` (read-only result of any camera) is in degrees
  (sum_end's credits camera: `cam.Ang = V3D(-0.3, 3.4, 0)`, `Fov = 1.35`). Setting degrees
  makes the camera spin wildly; that was the "chaotic POV" in earlier recordings.
- Hide an entity's model with `e.bObjectVisible = false`; per-node `bShowNode_<node>` also
  exists. `bRender` does not exist on entities (only inside model data). Camera has clipNear.
- Engine property names are listed as strings in the exe next to `TimeSpeed`, `EntityLink`.
- Result: other players watch first-person scenes from their own actor's head (own body hidden),
  looking at the hit point of the scene camera's view ray (smoothed). Checked on TheTrap by video.
- Testing with video: native/capture.c (Present hook, MSAA resolve), tools/record.ps1, sheet.ps1.
- More downloads: ffmpeg (BtbN master-latest win64 gpl) SHA-256
  `4c967363c4f369172cfec1336d8364814c181c749cb34f654d522af1542fd29e`.

## Tools and downloads
- unluac 2025-12-23, https://sourceforge.net/projects/unluac/files/Unstable/unluac_2025_12_23.jar/download
  SHA-256 `98be0fa84ac73ca66dce2842a2e4512226f4c611b6500dc96415571fc5538fcc`
- pefile (pip) for PE inspection.
- Zig 0.16.0 (C compiler for the 32-bit proxy DLL),
  https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip
  SHA-256 `68659eb5f1e4eb1437a722f1dd889c5a322c9954607f5edcf337bc3684a75a7e` (matches ziglang.org index.json)
- capstone 5.0.3 (pip) for disassembly.
- ENet 1.3.18 source, https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz
  SHA-256 `28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd` (MIT)
- Ghidra 12.1.4 (SHA-256 ddac49f9...d4db, matches release notes), Temurin JDK 21.0.12.1+1
  (SHA-256 f9d6e191...8b4e, matches Adoptium API)
