# Fallout Tactics: working notes

Goal: play the single-player campaign co-op with friends.

## Identity
GOG, v1.27, BOS.exe SHA-256 C5B91FE56EF8DE118CE22CCD5E6512F217CA16C8B8758EA14C626229EB5A4E40, Windows.
Engine: proprietary (Micro Forte / 14 Degrees East). Ships with the official editor (FT Tools.exe, v1.26).

## Observations (2026-10-20)
- Directory and games/ had no entry for Fallout Tactics. `gh` not installed, so issues/PRs not checked yet.
- Combat modes (core/locale/gui/gui_text.txt 306-313): Continuous Turn-Based (CTB, default; AP refill
  over time, everyone acts at once), Individual turn-based, Squad turn-based.
- Built-in multiplayer: Skirmish, Assault, Capture the Flag, Scavengers (readme.txt). CTB and the
  turn-based modes both exist in MP (Patch Readme.txt mentions "turnbased multiplayer" fixes).
- The tester plays CTB. Co-op target: CTB first; Individual/Squad turn-based later (needs turn order across humans). Saving is disabled in
  multiplayer (Patch Readme.txt 1.25 notes: "Disabled save in multiplayer").
- MP maps live as loose files in `core/missions/<mode>/*.mis`. Campaign maps exist as loose files too in
  `core/campaigns/missions/core/` (bunker01.mis, mission01.mis ... ). Others may be packed in .bos archives (unknown).
- FTSE (Fallout Tactics Scripting Engine): https://github.com/melindil/FTSE, MIT, v0.60a (2026-03-27).
  Patches BOS.exe to load a DLL with Lua, triggers at game events, configurable hex edits. Installs by
  replacing binkw32.dll. Multiplayer support not mentioned. Supported exe versions: unknown.

## Prior co-op attempts (none found with a download)
- TBC clan co-op mod, about 2003: AI critters run by a "battlemaster (bughandler)" player. Source: TTLG thread
  https://www.ttlg.com/forums/showthread.php?t=49006 (403 to fetch; seen via search summary only).
- "The COOP Mod" on NMA: original SP maps, up to 6 players, author said 40% done. https://www.nma-fallout.com/threads/the-coop-mod.165469/
  (403 to fetch; search summary only). Status: unknown, assume abandoned.
- NMA co-op map that copies a campaign mission: 2 players vs AI mutants; solo start ends the game at once
  because there is no opposing human. Source: search summary of NMA "Playing multiplayer alone?" thread.
- GameSAS how-to: copy SP maps into the MP folder, ally player slots; author never finished. Search summary only.
- Steam threads (2014, 2022): no campaign co-op exists. https://steamcommunity.com/app/38420/discussions/0/45350791308582171

## Setup (2026-10-20)
- The author chose: full campaign co-op, 3-4 players.
- Test copy at `local/test-install/` (robocopy, 364 files, 1.572 GB; copied BOS.exe hash matches).
- FTSE 0.60a: release zip SHA-256 8731b3fdd962ffc53d8191c4c41950489ba932b0eef612e367a735e82613bc9d
  (matches the GitHub release digest). Since 0.60a it loads through a binkw32.dll wrapper, so BOS.exe
  is untouched. Its realBinkw32.dll is byte-identical to the GOG binkw32.dll.
- Smoke test: test copy launched, still running after 12 s, FTSE.log created (empty). Main menu
  FTSE version text not checked visually yet.
- Hub repo search (GitHub API, "tactics"): 0 issues/PRs. Nobody has claimed the game.
- FTSE Lua hooks (from bundled docs): OnStart, OnLocaleLoad, OnLongTick, OnRadiated, OnVariableChanged
  (mission + campaign vars), OnChanceToHitCalc, OnChanceToCritical1/2, OnCriticalEffect1/2, OnDamageCalc,
  On{Straight,Area,Radial,Spray,Cone,Burst,Projectile}Attack, OnInventoryAdd/Remove, OnCheckItemAllowed,
  OnCheckEquip/OnEquip/OnCheckUnequip/OnUnequip. Plus Entity vtable call/hook, `world` class,
  entity classes incl. Actor, Inventory, Door, Switch. No multiplayer or networking hooks.

## Reverse engineering, session 1 (2026-10-20)
Build: GOG 1.27 BOS.exe (SHA-256 above). Tools: REA (Ghidra 12.1.4), Ghidra headless decompile-all,
own stdlib Python PE readers. Dumps in `local/re/` (not published). Image base 0x400000,
.text 0x401000-0x7fc000, .rdata 0x7fc000, .data 0x894000. Embedded PDB path "BOS Milestone.pdb" (no PDB shipped).

Observed (static reading of decompiled code, not yet run):
- Imports have no Winsock/DirectPlay. Networking is DirectPlay through COM (ole32): strings
  `net_initCOM`, `net_initDPlay`, `net_initDPLobby`, `DPNetwork`.
- RTTI classes and vtables: Network 0x8336c0, DPNetwork 0x833714, DPTCPNetwork 0x832aa4,
  NullNetwork 0x83366c, XNetwork 0x825298 (exception type), Server 0x825260, NetworkServer 0x82527c,
  Client 0x824ec0, NetworkClient 0x825230, Player 0x80942c, CampaignMission 0x82cb6c,
  WCampaignMain 0x82cde0, WMultiSetup 0x805228, WSingleMain 0x807868, WMainMenu 0x82e060, WGameMain 0x801ac8.
- 0x7570d0 = NullNetwork ctor (0x30 bytes). 0x724400 = allocate NullNetwork and register it.
- 0x7288d0 = allocate DPTCPNetwork (0xac bytes).
- 0x724550 = network manager: add network to list (manager+8 list head, +0xc count). Throws XNetwork if null.
  If the session is already started (+0x10), calls the new network's vtable+0x2c (start) right away.
- 0x7244c0 = start session(name, int param_2, string param_3): sets started flag (+0x10), stores param_2
  at +0x19, calls vtable+0x2c on every registered network.
- 0x636bd0 = **single-player start**: creates 0xfb-byte object via 0x63eb70 (stored at this+0x248),
  copies 0x58 bytes config from 0x8bd9c0 into it at +0x31, registers NullNetwork only, starts the
  session named "SinglePlayer" with param_2 = 1.
- 0x4a9a20 = **multiplayer host** (after dialog returns 4): same 0xfb object and config copy, registers
  NullNetwork **and** DPTCPNetwork, starts session with param_2 from 0x6d0630.
- 0x637cf0 = another path that registers NullNetwork plus a passed-in network (join/lobby?), unknown.
- FTSE notes: 0x4e8280 loads a mission or .sav (campaign start, area reload). End of level serializes
  the player group entities first, then the level.

- DirectPlay version: **DirectPlay 4** (exe contains CLSID_DirectPlay, CLSID_DirectPlayLobby, IID_IDirectPlay4/4A,
  IID_IDirectPlayLobby3A, DPSPGUID_TCPIP, DPSPGUID_IPX; no DirectPlay 8 GUIDs). The test PC has dplayx.dll,
  dpwsockx.dll, dpnet.dll in SysWOW64 (dated 2025-06-11); runtime use not tested yet.
- Network vtable: 20 slots (0x00-0x4c). Base Network slots 6-19 are `0x7a2eae` (likely _purecall), so
  6-19 are the transport interface. DPTCPNetwork overrides only slots 2 (dtor) and 9 over DPNetwork.
  NullNetwork impls: 6 0x757e00, 7 0x757e10, 8 0x757e20, 9 0x757140, 10 0x757560, 11 0x757150, 12 0x757270,
  13 0x7574b0, 14 0x757570, 15 0x757820, 16 0x757900, 17 0x757850, 18 0x757e50, 19 0x757b80.
  - slot 9 (0x24) calls slot 10 then sets open flag (+0x18)=1; slot 10 (0x28) calls slot 13 and clears it.
    (Naming of 9/10/13 still unsure: start/stop pair.)
  - slot 17 (0x44) 0x757850 = poll: pop one queued message (queue +0x20, count +0x24); type short at +8:
    0 = data -> listener(+0x28)->vtable[0x10](buf, len, sender), else system message -> 0x757b40.
  - slot 15 (0x3c) 0x757820 takes the same (name, int, string) as session start (manager calls +0x2c... check).
  NullNetwork is a loopback message queue: the Network classes only move byte buffers between Server
  and Client listeners.
- Derived: a custom Network subclass (own vtable, modern transport) could carry the game's own messages.

### Session 1b: protocol (static reading)
Tooling note: Ghidra auto-analysis missed 3,037 functions only reachable through vtables (incl. every
Server/Client/NullNetwork method). Fixed by scanning all RTTI vtables (662 classes, 4,685 slot targets)
and creating functions at missing targets with a headless script (3,027 created). Lesson for the playbook.
Also: awk compares hex addresses like `0063e640` as numbers (scientific notation); force string compare.

- Server vtable 0x825260 (6 slots); slot 4 0x63dbb0 = receive dispatch on msg type (dword at +0).
  Client vtable 0x824ec0; slot 4 0x6321a0 = receive dispatch. Client has extra vtables at +9, +25.
- Server receives: 2 = player commands (0x63dce0, validates ownership, see below), 4 = 0x63c3b0,
  100 = player setup record 0x37 bytes (0x63e120), 0xcc = name/string, sets client+10 (0x63df00),
  0xcd = sets client+0xb (0x63e010), 0xcf = client reports out of sync, sets client+0xe (0x63e050),
  300/301/302 = chat relay (0x63e1a0; 301 and others choose recipients, team flag).
- Client receives: 1 (0x633bd0), 3 (0x633ab0, record with id +0x10 and array: checksums?), 200 = wrong version,
  0xc9 = too violent (gore setting), 0xca = kicked, 0xcb = 0x6327f0 (sets +0x24c "in game"?),
  0xcc = command batch per tick (0x632840; frames at +0x1d7..+0x1db, 16-byte entries),
  0xce = 0x632c50, 0xcf = "out_of_sync" / "my_out_of_sync" text (0x632c90), 300-302 = chat (formats `<+p>`).
- Server 0x63e640 packs queued per-client command blobs into one 0xcc and broadcasts via 0x724840.
- Client 0x6363b0 compares two record lists by id (+0x10) element-wise; on mismatch calls 0x634770,
  which sends {0xcf, client id (+0x130)} to the server.
- **Ownership check in msg 2**: World global `DAT_008bc564`; entity table at World+0x743 (8-byte entries,
  generation short at +6); entity+0x163 = owning player index; Player array at World+0x733, stride 0x15d;
  Player+0x2d = network client id. Command accepted only if Player+0x2d == sender.

Derived (high confidence, still static): the netcode is **deterministic lockstep**: the server relays
player commands, every machine simulates the full world (AI, scripts), and checksums detect desync.
Single-player runs through the same Server/Client path over NullNetwork.

Implications for campaign co-op (derived):
1. Every machine must start a mission from identical state (mission, campaign vars, squad, inventory,
   RNG). Natural plan: host sends its save to clients, all load it, then lockstep.
2. Anything that changes state without going through the command stream desyncs. Suspects: dialogue,
   barter, recruiting, bunker quartermaster, world map travel (none exist in MP). Must check whether SP
   UI issues these as commands.
3. Joining mid-mission needs a full state transfer; join at mission start / via save is far simpler.
4. Built-in desync detection is a gift for automated QA.
5. Squad control = set entity+0x163 to the joining player's Player record.

- Lockstep command = FTSE's ControllerCommandStruct {u8 type, u16 action_entity_halfid, EntityID e1,
  Vector3 loc, EntityID e2, i32 param}, applied by entity vtable slot 510 ActOnControllerCommand.
  Actor (vtable 0x80c1d0) slot 510 = 0x5747e0, ~36 cases (0x00-0x25). Cases with telling strings:
  0x01 RobotScurry, 0x06 equip, 0x07 unEquip, 0x18/0x19 steal, 0x20 overwatch, 0x25 disembark/sneak.
  Controller (0x80a778) slot 510 = 0x50fe60. Case meanings for the rest: unknown.
- No talk/barter/recruit command found by string. Speech buttons (FTSE notes: barter 0x4d0000,
  gamble 0x4d0090, recruit 0x4d0110) are only reached by function pointer; 0x4d0090 sets globals and
  opens UI window 7. Whether these mutate state outside the command stream: **unknown**.
- Toolchain on the test PC: VS 2022 Professional/Community + Build Tools 2022 (x86 builds possible).

Next experiment (runtime): DLL in the test copy that logs every NullNetwork send (message type, and
command type for msg 2) during single-player. Actions that change state without a message = desync risk.

### Runtime run 1 (2026-10-20, the tester played the test copy, first campaign mission)
Logger: `netlog.dll` (own code, local/src/ftcoop/logger), loaded by FTSE user script core/FTSE/ftse.lua via
`package.loadlib("netlog.dll","*")`. Patches Server vtable slot 4 (0x825270) and Client vtable slot 4
(0x824ed0) after checking the exe prologue bytes. Marker key = scan code 0x29 (left of "1"), works on any
layout; MessageBeep is inaudible while the game runs. Raw log: local/re/netlog_run1.txt.
FTSE note: a fresh FTSE 0.60a install logs "Error loading LUA scripts ... module 'FTSE.ftse' not found"
until core/FTSE/ftse.lua exists; the error aborts the rest of init.lua (hook installs after line 176).

Observed:
- **Single-player runs the full lockstep session over NullNetwork** (confirmed at runtime).
  Start handshake: C->S 0x64 (59 bytes), S->C 0xc9 (358/413 bytes, settings), 0xc8 (16), 0xcb (8),
  C->S 0xcc (12), S->C 0xcc (20), C->S 0xcd, S->C 0xce. Then S->C type 1 tick every ~280 ms with a
  sequence counter (+4 per tick); a tick carrying a command is 32 + 27*n bytes.
- C->S type 2 = `{u32 type=2, u16 count, count x 27-byte ControllerCommandStruct}`.
- Command types seen (meaning from timing against the tester markers; "?" = not confirmed):
  0x00 move (loc = target point; squad move = one record per actor), 0x02 act on/attack target e1,
  0x04 use/open target e1 (corpse, also self+item form), 0x06 equip (e2 = item), 0x0b ? (param 8/0, stance?),
  0x0c ? (e2 entity, param 782), 0x12 ? (e2 entity, param 6), 0x14 take item from target inventory
  (e1 = container/corpse, e2 = item, param = quantity), 0x17 talk? (e1 NPC, param 6), 0x21 close
  inventory/transfer? (param 5).
- Sender id in this run: 58282272 (single client).
- Game crashed while the tester changed to a very high resolution. No Windows crash event, netlog shows
  nothing unusual before it. Probably the known FoT resolution fragility, not the logger: unknown.

Not covered in this run: barter, bunker (quartermaster, recruit), world map travel, mission end/transition, save/load.

### Runtime run 2 (2026-10-20, the tester played; raw log local/re/netlog_run2.txt)
- Tester: marks 6 and 7 = **buying and selling** (barter). The log shows **no message at all** at
  marks 6 and 7. Observed: barter changes inventories without any session message or command.
  Derived: in lockstep co-op this desyncs; barter must be turned into a synced message.
- Mark 4: a **second full session start** (0xc9, 0xcb with 0x0210, 0xcc, 0xcd, 0xce) right after a squad
  move at mark 3. Tester action: not yet confirmed (mission exit / bunker / load?).
- New command 0x15 seen with 0x04 -> 0x15 -> 0x21 (open, transfer, close; container?). Meaning: unknown.
- Tester: mark 9 = clicking on a new mission from the quartermaster (bunker NPC). Log: exactly one
  command 0x17 right after mark 9. With 0x17 after marks 2, 5, 8 too: **0x17 = talk to / interact
  with an NPC** (likely). Starting a conversation is a synced command.
- Tester: marks 10-13 = **saving the game, then exiting**. Exiting sent no message either (SP session). No messages: expected, saving writes a file and does not
  change world state. Derived: in lockstep every machine holds the same state, so saving can be
  host-only (or everyone) without sync work.
- Mission selection after the quartermaster conversation: no separate message seen between marks 9 and
  10, but the tester did not travel or start the mission in this run. Synced or not: unknown.

### Runtime run 3 (2026-10-20, the tester played; raw log local/re/netlog_run3.txt)
- Mark 1 = **loading a save**: full session start handshake (0x64 ... 0xce), same as a mission start.
- Marks 2-3 = **recruiting** 3 characters (squad 3 -> 6) in the bunker: **no message**. Unsynced.
- Mark 5 = **entering the world map**: full session restart (0xc9 at 92 s ... 0xce at 101 s).
- Mark 6 = pressing start to accept the mission on the world map: no message. Mark 7: not stated.
- Run 2 mark 4 (session restart after a squad move) fits leaving the mission for the bunker.

Derived: every map change (mission, bunker, world map) and every save load starts a **fresh session**.
These are natural sync points: the host can hand all clients identical state (its save) before each
session starts. Between them, the unsynced actions found so far are barter and recruiting.

Summary. Synced: move, attack, use/open, equip, loot/transfer, talk (start). Unsynced, must become
co-op messages: barter, recruiting, world map mission accept (probably fine if host-only plus the
next session sync). No sync needed: saving, quitting. Not yet seen: dialogue choices beyond the
first click, quartermaster gear requisition (may be barter-like), vehicles, turn-based modes.

### Single instance
- 0x72ee90 (WinMain path): `CreateMutexA(NULL, TRUE, "Fallout Tactics")`; if GetLastError() == 0xb7
  (ERROR_ALREADY_EXISTS) it closes the handle and returns 0 (silent exit). Only check found: no
  FindWindow import. Fix per playbook lesson 21: neutralize in-process before WinMain (binkw32 wrapper
  loads at import time, so an FTSE fork can patch the IAT entry 0x7fc180 or the compare).

### Two copies on one PC (done 2026-10-20)
- FTSE custom config `core/FTSE/FTSE_config.json` patch: offset 0x72eeb4 `75` -> `eb` (jnz -> jmp after
  the ERROR_ALREADY_EXISTS compare). FTSE applies patches in its WinMain detour before the real WinMain.
  Verified: FTSE.log "Applying patch Allow multiple instances", both copies stay running.
- Second copy needed anyway: both instances would share core/user/$$current$$ (scratch save) and logs.
- Fullscreen-only game; alt-tab between two fullscreen copies is unreliable (the tester saw it close).
  Fix: cnc-ddraw v7.1.0.0 (https://github.com/FunkyFr3sh/cnc-ddraw/releases/tag/v7.1.0.0, MIT),
  cnc-ddraw.zip 2,688,079 bytes, SHA-256 0B13AB89A64C9918189B1DADD449EF6ED3CB3B7B19CABD96D8ADBD95505BB908
  (GitHub publishes no digest; hash recorded at download). ddraw.ini has a built-in `[BOS/2]` profile
  (hook_peekmessage=true). Set windowed=true, savesettings=0, posX per copy. Verified: two windows
  side by side (window rects (-7,2)-(647,520) and (649,2)-(1303,520)).

### Live experiment 1: a second copy joins a single-player session (2026-10-20)
Setup: host = test copy 1 with a save loaded; netlog hotkey Ctrl+marker runs on the main thread (from the
Client receive hook): Server = Client+0x248 (vtable checked 0x825260), Server+0x19 max players 1 -> 5,
`net = 0x7288d0()` (DPTCPNetwork factory, no args, ret), `0x724550(server, net)` (AddNetwork, thiscall, ret 4).
Joiner = test copy 2, Multiplayer -> TCP/IP -> join 127.0.0.1.
Observed:
- `host: AddNetwork returned, TCP network attached`. Joiner **sees the session** and connects.
- Joiner sends 0x64 player setup records (name "Unnamed"); host server answers 0xc9 roster, which grows
  413 -> 468 bytes (+0x37 = one more player record). Ready toggles resend 0x64; server acknowledges.
- Joiner stays in the MP lobby: the server never sends it 0xcb (start).
- Static, same session: Server+0x19 is DPSESSIONDESC2.dwMaxPlayers (0x758bc0 builds the desc, offset 0x28,
  then IDirectPlay4A::Open (+0x60) with DPOPEN_CREATE). 0xc9 roster builder 0x63e3a0: server settings
  (0x58 bytes from Server+0x31), a 0x103-char string, game-in-progress flag (Server+0xa5), N x 0x37 records.
- **0xcb = start + RNG seed**: 0x63e5c0 seed = LCG of time(0) (0x7a2f3f), sent as {0xcb, seed} to every
  player record with present(+8), connected(+0xf), ready(+0xd). Caller 0x63f0b0 = StartGame(force):
  without force throws net_playersReady if a connected player is not ready (+9); sets +0xa5, marks ready
  players in-game (+0xd), rebroadcasts roster, sends seed. Callers: 0x484850 (MP lobby Start button),
  0x63aab0 (StartGame(1); no direct caller, reached by pointer; likely SP auto-start).
- Bug fixed after the run: GetAsyncKeyState is global, so the hotkey fired in both copies. Now focus-only.

Derived: a late joiner cannot just be sent 0xcb: a new seed (and the host's advanced world/RNG state)
means instant desync. Joining must happen **at a session start**: host holds StartGame until joiners are
ready, every machine starts from the same save and the same seed.

### Live experiment 2: hold the single-player start, joiner via MP browser (2026-10-20)
- SP Load flow 0x637ab0(Client*, FOTString file): 0x636bd0 (session), Client+0x13a = file, Server+0x2d = file,
  then modal WDummyGameSetup (vtable 0x825074; +0xfa Client, +0xfe Server); result 4 -> 0x638260.
  WDummyGameSetup slot 48 (0x825134 -> 0x63aab0) is the per-frame Update (called by window tick 0x6c5130,
  slot 70) = `mov ecx,[ecx+0xfe]; StartGame(1)` in a try/catch. Hooked: host attaches TCP on first frame
  and holds the start until Ctrl+marker.
- Client 0xc9 handler 0x6324d0 assigns Client+0x13a from the roster string (msg+0x5c) (`lea ecx,[ebp+0x13a]`),
  copies 0x58 settings to Client+0x14a; 0xcb handler writes the seed into Client+0x14a.
- Client level load 0x636de0: loader 0x4e8280(Client+0x13a, ...), then 0x4f4d90(Client+0x14a settings/seed).
- Server player records (node+8): byte0 present, byte1 ready (joiner 0 -> 1 on Ready), byte7 connected,
  name at +0xc ("Unnamed", joiner "Unnamed(1)").
Observed: host held, joiner joined and readied, Ctrl+marker sent 0xcb seed 0x7d16 to both. Host's client
sent 0xcc (loaded) but the server sent no 0xcc/0xcd/0xce: it waits for every client to load. Joiner
went into MP squad selection ("select operators") and crashed after picking (no Windows crash event).
Derived: the MP client flow is the wrong path for a save. Next: joiner uses the SP Load flow too.

Client network API (NetworkClient base at Client+0: +4 network, +8 connected): SetNetwork 0x728640
(clears old via 0x7286e0: net[0x28] close, net[4] release; opens new via net[0x24](address)), Connect
0x728790 -> net[0x30](FOTString* password, session record*, client), GetSessions 0x728710 -> net[0x38](vector*).
DPNetwork: EnumSessions slot 14 0x7599d0 (synchronous; vector of 0x85-byte records, guidInstance at +0x25),
Join slot 12 0x758580 -> 0x7591d0 (DPSESSIONDESC2 from record GUID, IDirectPlay4A::Open DPOPEN_JOIN).
SendPlayerInfo 0x6347a0 sends 0x64 from Client+0x103. FOTString(char*, codepage [0x82ea44]) 0x6c8bf0.

### Live experiment 4: both sides use the SP Load flow (2026-10-20)
Joiner mode (ftcoop.ini mode=join): on its WDummyGameSetup first frame, SetNetwork(DPTCP, "127.0.0.1"),
GetSessions (0 found at first try, 1 found two seconds later), Connect(password "", sessions[0]),
SendPlayerInfo. Never calls the original Update.
Observed: joiner connected, got 0xcb (same seed 0x437f as host), **loaded the same map**. Both clients
sent C->S 0xcc ({0xcc, player id, 0}: host 1, joiner 2); server sent S->C 0xcc with 2 entries
{len 8: {1,0}}, {len 8: {2,0}}. Host crashed while handling it; joiner showed the map with no units and
got nothing further (no 0xcd/0xce, no ticks).
Static: S->C 0xcc handler 0x632840 -> mission spawn 0x637050 ("mission_spawn"). For every player record
(Client+0x142 list, id at record+0x2d) it calls World::SpawnPlayer 0x4f4fb0 (thiscall ECX=World, (rec, spawn,
a, b), ret 0x10). Pass 1 call at 0x637202: spawn data = *0x8bdd2c (campaign) + 0x5d when 0x6a2270() is false
(SP), else the player's own 0xcc data. So the **campaign squad was spawned for player 2 too**.
Passes 2/3 (0x6373ad, 0x6373fd, ...) retry failed players with their own data; "Player %d %s was refused
spawning" -> mission_spawnpl.
Fix: call site 0x637202 redirected: players other than id 1 get an empty spawn list
({flag 0, std::list sentinel (12-byte node, next=prev=self), count 0}, as built at 0x63722e).

### Live experiments 5-10: into the same mission, in sync (2026-10-20)
- Empty spawn for player 2 returns 0: World::SpawnPlayer 0x4f4fb0 assigns a team. Record byte +8 == 1 and
  SP (World+0x7c9 == 0) -> team 1, else 0x4f6fc0 finds a team (fails in an SP save). World Player slot
  index == team (World+0x733 + team*0x15d); slot already used (+4) -> throw. SP branch then finds the first
  entity with +0x163 == 1, sets the slot camera, returns 1. So two humans cannot share slot 1 natively.
  Fix: spawn hook returns 1 for player != 1 without spawning (joiner = spectator of team 1).
- Joiner then crashed at game start: EXCEPTION c0000005 eip 0x4df6e4 (group "get Nth alive member",
  reads [this+0x4f], this = 0). Caller WGameMain 0x476dc0 got the group from UI "local player" 0x65af20:
  cached at ui+0xfe, else slot with Player+0x31 == Client(ui+0xfa)+0x130 (own id). Joiner id 2 has no slot.
  Fix: 0x65af20 reimplemented (jmp at entry): no match + co-op joiner -> slot 1.
- Joiner then loaded with correct positions but its world never moved. Client tick processor 0x635e00 runs
  ticks only while gate 0x635ce0 is true; past frame Client+0x251 the gate requires
  GetPlayerById(own id) 0x50b880 (thiscall, ret 4: slot with Player+0x31 == id) != 0. Fix: only the call
  at 0x635dbe redirected (joiner -> slot 1). Not the function: "player left" 0x4f4e10 removes the slot it
  gets (0x4e0bc0, clears +0x2d), so a fallback there would delete the host's squad.
- **Observed (exp 10): host and joiner in the same mission, the joiner's units follow the host's moves.
  Both processed the same 16 command ticks, 0 out-of-sync (0xcf) messages, no exceptions.**
- Joiner commands (6, from its net id) reached the server and were dropped (ownership: Player[slot]+0x2d
  != sender). Fix (exp 11): host ServerRecvHook passes co-op msg-2 commands on with sender = slot 1's
  owner net id (Player[1]+0x2d) -> shared squad.
- Crash logging: vectored exception handler in netlog logs code, EIP, registers, and BOS.exe return
  addresses on the stack (stale ones too; the first entry is the most reliable).

### Milestone (exp 11, 2026-10-20): two-player campaign co-op, shared squad
Setup: two copies on one PC (loopback TCP, DirectPlay 4 via the game's own DPTCPNetwork), both load the
same SP save ("New Save Game", byte-identical in both copies); host holds start, Ctrl+marker starts.
Observed (the tester played both windows): **both players control all squad members from either window**.
Logs: 5 commands from each sender, joiner's passed on as squad owner; both copies processed the same 10
command ticks, last tick sequence identical on both, 0 out-of-sync (0xcf), 0 exceptions.
QA level of this: manual playtest, one mission, a few minutes. Not automated yet.

### Automated two-copy test (2026-10-20)
Bot in netlog.dll, on with ftcoop.ini [test] enabled=1; launcher local/src/ftcoop/test/run_test.py.
- Skip startup movies: DAT_008bddbf ("intro shown this session", set by intro playlist 0x6c1890) = 1 before the
  main menu ctor 0x6bff50, which otherwise schedules a 1 ms timer -> 0x6c0970 -> 0x6c1890 (intro.bik + ss00.bik).
- Load without menus: main menu Ctrl+L (WMainMenu slot 55 0x6c0ba0) sets DAT_008bdda9 = 1 and calls 0x6c0970:
  path "user/save/" + locale mission_quickSaveFile + ".sav" -> 0x6c2540(*0x8bddb8, FOTString*, 0, 0) (cdecl).
  Calling 0x6c2540 from WMainMenu's Update (slot 48, vtable 0x82e060, shared no-op 0x401d20) **hung**: the load
  runs a whole game inside the window tick. The game's pump 0x784920 peeks its own window and passes each message
  to 0x7857f0 (thiscall(app, hwnd, msg, wp, lp, time), ret 0x14) with **no DispatchMessage** (a WndProc subclass
  never sees posted messages). Fix: hook that call (0x7849cc), Update posts WM_APP+0x51, the load runs from the pump.
- Exact-frame bot steps: world frame loop 0x4fbf50 calls Client tick processor 0x635e00 at 0x4fc0db once per frame,
  then world step 0x4fc230, then World+0x2eb++. Hooked call = per-frame bot step.
- Squad = World Player[1] member list at +0x4b (std::list {next, prev, EntityID{u16 seq, u16 id}}), entity table
  World+0x743 ({ptr, i16, i16 seq}), position float[3] at entity+0x9a, Actor HP at +0x281 (FTSE layouts).
- Move command sent as msg 2 (33 bytes, cmd 0, actor id, target xyz, param 4) via Client send 0x728840
  (thiscall(client, msg, len, reliable), ret 0xc). Target = position of the farthest squad member.
- Background window: game menu did not advance while unfocused; cnc-ddraw noactivateapp=true did not fix it
  (the tester's focus may have helped in the passing run). Open item.
Result: host_move 16/16 PASS (logs local/re/test_runs/20261020-165823_host_move): same first frame 39 on both,
squad of 6 snapshot A and B identical, moved unit 922 travelled 2.29 on both, 0 out-of-sync, clean exit.
Not yet run: join_move, mutation (share_squad=0) for join_move.

Races the bot found (a human start is slower and mostly misses them):
1. Stale UI local-player cache: both copies crashed at 0x49f61b/0x49f62f (0x49f600, centre camera on squad) reading
   the member list of freed memory. 0x65af20 caches the slot at ui+0xfe; the World player array is rebuilt during
   level load. Fix: the reimplemented 0x65af20 always rescans and refreshes the cache.
2. Split "everyone loaded": the joiner resends player setup (0x64) right after StartGame; handler 0x63e120 copies
   the 0x37-byte record over node+8.., clearing the in-game byte node+0xd. If that 0x64 lands before both 0xcc
   reports, all-loaded check 0x63e090 (waits for present+connected+in-game players with node+10 loaded == 0)
   ignores the joiner, and the server sends two one-entry S->C 0xcc. Each client then runs mission spawn twice;
   the second time player 1 has no entry, goes to the no-data path (call 0x637256, not hooked) and
   SpawnPlayer throws XRuntime (0x4f51ab); the game then crashes in its error handling (title redraws forever).
   Evidence: run 20261020-170452 (0x64 at 10.360 before both 0xcc at 10.641) vs passing 20261020-170144 (0x64 at
   10.546 after the host's 0xcc at 10.515). Fix: host keeps node+0xd across a 0x64 for a player already in game.
Suite (2026-10-20, after both race fixes, bot schedule shortened to snapshot B at +200 frames, ~22 s per test):
`python -I local/src/ftcoop/test/run_suite.py`
```
ok   host_move: expected PASS, got PASS  (host_move, 16/16 checks)
ok   join_move: expected PASS, got PASS  (join_move, 16/16 checks)
ok   join_move without shared-squad fix (mutation): expected FAIL, got FAIL  (join_move-mutated, 15/16 checks)
SUITE: PASS
```
Mutation fails only on "moved unit travelled > 1.0" (0.00 on both copies), so join_move tests the fix.
host_move also passed 3/3 consecutive runs before the schedule change; the 0x64 race fix triggered every run.
Scope of this QA: one save, one mission, two copies on one PC over loopback, shared squad, CTB.
Tester: the first walk (to the farthest squad mate, 3 units, moved 2.29) was invisible. Now the target is an
Actor outside the squad, 10-30 units away, same floor height (|dy| < 1); threshold 8.0; snapshot B at +400 frames.
Suite rerun: host_move PASS (moved 23.86 on both), join_move PASS, mutation FAIL as expected -> SUITE: PASS.
The tester watched the run and confirmed the unit visibly walked.

Scenario bot (bot_scenarios.inc, [test] scenario=move|simul|attack|soak), whole-map oracle: every Actor's
position and HP every 50 frames, all shared frames compared.
- host_move PASS (8 frames, 440 actor states identical, moved 23.81), simul PASS (host unit 22.40, joiner unit
  20.85, same frame), attack PASS (cmd 0x02, e1 = {seq, id}; target 70 hp 10 -> -5, 550 states identical).
  Test copies still run the god mode / sure-hit Lua (identical on both copies).
- Soak v1 was mostly idle (the tester noticed): walk targets were live characters 10-30 units away; after a few walks
  none qualified and most rounds logged bot: FAIL. Frames that were compared (80 to frame ~4000) all matched.
  Fix (built, NOT yet run): waypoints = every Actor's position on the first frame, band 10-40 units, plus a soak
  check that the squad walked a minimum total distance.
- Soak v2 PASS: 1,200 frames (10 moves, 259.5 units, 1,320 states identical) and 4,000 frames (34 moves, 921.9
  units walked, 80 frames x 55 actors = 4,400 states identical, 0 out-of-sync, clean exits).

### Save transfer (2026-10-20)
- A save is not loaded by name at session start. Client+0x13a stayed 0x8af7e4 (the game's shared empty FOTString)
  through the 0xc9 roster and up to the level load, the roster's file field (msg+0x5c) is empty, and Server+0x2d
  is empty in the lobby. Loading a save fills user/$$current$$ and the level loads from there. So the transfer
  must happen before the joiner loads anything.
- 0x6c2540 (direct load) calls the save check 0x4c1290(file) (thiscall, ret 4; opens the file 0x6d4b50 and
  checks its header) at 0x6c260e before the session starts. Hooked on both sides: the host records the file.
- Host serves the recorded save on TCP port 47701 ([coop] file_port): request "GET\n", reply
  "FTCOOP1 <size> <name>\n" + bytes. Joiner fetches at the main menu, writes core\user\save\ftcoop_received.sav,
  then loads that file through the same quick-load path. [coop] fetch_save=0 turns it off.
- Test fetch_save: joiner's bot is configured with QuickSave (precondition: differs from the host's New Save Game)
  and must still end identical. PASS 19/19: 700,953 bytes sent, received file byte-identical, joiner loaded it,
  both first frame 39, 440 states identical, moved 25.36 on both.
  Mutation (--mutate-fetch): FAIL as required: joiner started at frame 10117 (QuickSave's timeline), 0 shared
  frames, joiner timed out.
- Limits: only the bot's main-menu path fetches (a human joiner needs a menu key or button); blocking socket calls
  with 10 s timeouts on the main thread; no NAT or internet testing.
- Harness bug found: interrupting the launcher left test ftcoop.ini files in the copies, and the next run backed
  them up as the "originals". Launcher now restores plain play settings when a backup contains [test].

Suite after the transfer: 7 tests (host_move, join_move, join mutation, simul, attack, fetch_save, fetch mutation)
-> SUITE: PASS.
Perf bug found: since the UI local-player lookup rescans on every call, its "sharing slot 1" log line was written
on every call (28,000 flushed lines in seconds, joiner crawled). Logged once per id now.

### Map transitions (2026-10-20, in progress)
- Mission end is a trigger action: ActionEndMission (vtable 0x82b2e0) slot 15 = execute 0x680b60 (fastcall,
  reads its target FOTString at +0xb; "World Map" sets WGameMain(DAT_008bc384)+0x19d/0x19e exit timer).
  ActionMissionComplete (0x82bc14) slot 15 = 0x68a460: marks the mission complete (0x6a84e0) and calls entity
  vtable+0x358(1) for each member of the UI local player's squad (0x65af20) **inside the simulation**: the joiner's
  slot-1 fallback is what keeps this identical on both copies. Slots 4/6 are the editor description and name.
- Test scenario=transition: both bots call 0x680b60("World Map") on the same frame. Observed: both copies open the
  world map locally (window classes WCampaignWindow 0x82d2a8, WWorldWindow 0x82db04, WMissionItem, WListBox), no
  new session: the world map is local UI, not a session. (Window classes logged by a reimplemented window tick
  0x6c5130 that notes each RTTI class the first time it ticks.)
- World map: WCampaignWindow ctor 0x6b4cc0 (+0xfa client); key handler slot 55 0x6b6650: Esc 0x6b5cd0, Ctrl+S
  quick-save 0x6b5ee0, Ctrl+L quick-load 0x6b6360, Alt+S/Alt+L dialogs. Quick-load assigns the global
  "file to load" FOTString DAT_008bddb4 (mov ecx,0x8bddb4; call 0x6c8e80) and closes the window with
  vtable+0x34(0x10). WWorldWindow slot 51 (mouse down) 0x6ba2a0: on a location -> 0x6ba580 (mission), else
  0x6ba410 (travel destination; travel passes time, random encounters -> not lockstep).
- Derived design (proposal): re-sync by save at every mission start. Host loads the chosen mission alone, then on
  its first frame quick-saves and quick-loads it (the first-join path: lobby waits, file served, joiner fetches and
  loads); the joiner, idle on its world map, sees a new save generation from the host, fetches it, sets
  DAT_008bddb4 and closes its world map with 0x10. The same flow gives mid-match join, and makes barter and
  recruit sync unnecessary (they only happen where the host is in charge; the joiner is re-aligned at each start).

### Re-sync by save (built 2026-10-20; the author chose this design)
- Loads all end in the top-level loop 0x6bf6a0: after a session it reads the "file to load" FOTString 0x8bddb4 and
  calls 0x6c2540(file), so every quick-load passes the save check and LoadCheckHook (new generation).
- In-mission keys (keys.cfg): Alt+S {quickSave}, Alt+L {quickLoad}, Ctrl+S/L {save}/{load}; command dispatch
  0x4734f0: quickSave -> 0x478340, quickLoad -> 0x478860, reload -> 0x478150 (all WGameMain methods; WGameMain*
  in DAT_008bc384). quickLoad assigns 0x8bddb4 and closes the screen with vtable+0x38(0x10); it does nothing while
  WGameMain+0x15c != 0, a countdown that is 100 right after a quick-save (quickSave refuses then too).
- Host: a lobby is a co-op lobby (TCP, hold, serve) only when a load is pending; any other session starts alone and
  re-syncs after 0xce: quick-save, wait for QuickSave.sav to be written and +0x15c == 0, quick-load (new generation).
  Side channel answers "GEN" with the generation; the joiner polls it every 2 s on a background thread.
- Joiner: Client vtable slot 3 (lost session 0x632080 -> clear network 0x7286e0, show net_lostSession) replaced in
  co-op join mode: clear the network only (the error dialog stopped the mission screen ticking, so it never
  followed). From the window tick, on a newer generation: fetch, assign 0x8bddb4, close the world map
  (WCampaignWindow vtable+0x34) or mission (WGameMain vtable+0x38) with 0x10.
- Bugs found on the way: (1) the host's second lobby reused the first Server's address, so "new lobby" (pointer
  changed) was missed: no TCP, joiner found 0 sessions. Fix: forget the lobby at every session start. (2) the
  re-sync "retry if still in the mission 100 frames after quick-load" fired in every new session: 4 re-syncs in a row
  (each followed correctly). Fix: a session start ends the re-sync.
- Test resync: host bot requests a re-sync on frame 60 of session 1; both play 300 frames into session 2.
  PASS 18/18: generation 2 served (702,246 bytes), joiner followed from the mission, 2 connects, 7 snapshot frames
  (1 + 6) with 385 actor states identical, clean exits. Mutation: [coop] follow=0 (--mutate-follow).
- QuickSave.sav on the host is overwritten by every re-sync (the game's quick-save slot). Open item: own slot.
- World map -> mission (the real map change) not tested yet: needs the bot to start a mission from WWorldWindow
  (0x6ba580 on a location) and the joiner to follow from WCampaignWindow.

Next (old note): run soak v2 and the 5-test suite, then save transfer (design: host file server on a side TCP port serves
Server+0x2d's file; joiner downloads before connecting, rewrites the 0xc9 roster file name to its local copy).

Diagnostics added: C++ throw type names (ThrowInfo -> CatchableTypeArray -> TypeDescriptor), main-thread EIP and
stack on watchdog timeout, text of this process's windows (error dialogs) on timeout.
Harness lessons: kill the launcher loop too, not just the games (a PowerShell foreach started the next run and
reset the logs); the joiner did not need focus after all (an early log read was mistaken for a stall).

### Co-op design that follows from the evidence (proposal, not built)
1. Missions in lockstep; each player owns some squad members (entity+0x163 -> their Player record).
2. Host alone runs the campaign layer (bunker, recruit, quartermaster, world map); others wait/watch.
3. At every session start (map change, load) the host sends its save so every machine starts identical.
4. In-mission barter becomes a co-op message (or host-only plus resync at next transition).
5. Transport: own Network subclass in an FTSE fork (modern transport, no DirectPlay dependency).

Hypothesis (to test, not a fact): single-player runs as server + local client over NullNetwork, so
campaign co-op may be "register a DPTCPNetwork in the SinglePlayer session and let clients join",
plus giving joined clients control of squad members.

## Derived
- The engine has netcode for squads, AI and both combat modes, so replication exists. What MP lacks is
  the campaign layer: bunker, persistent squad/inventory/XP between missions, recruits, briefings, saves.

## Unknown
- Whether campaign mission scripts/triggers run correctly in an MP session.
- Whether FTSE works with the GOG 1.27 exe and in MP.
- Where the campaign state (squad roster, inventory, mission progress) lives in memory/saves.

## World map -> mission transition (2026-10-20)

Flow that now works (test `transition`, 19/19): both copies end mission 1 (ActionEndMission "World Map"), the host
picks a location on the world map, starts that mission alone, re-syncs by quick-save/quick-load (generation 2), and
the joiner, still on its world map, fetches generation 2, closes the world map with 0x10, loads it and joins the
host's lobby. Generation-2 states are identical on both copies.

Bugs found and fixed on the way (observed in test logs):

1. **Joiner joined a stale lobby.** An old lobby window ticked again when the joiner's world map opened, and the
   joiner joined the host's next lobby without loading the new save. Fix: a joiner lobby may only join after the
   joiner itself started loading the host's save (`g_join_pending`).
2. **Host's solo mission start hung.** World map -> mission restarts the game on the same Server, whose records
   still list the joiner as connected. The forced StartGame `0x63f0b0` (`__thiscall(Server*, char force)`, `ret 4`)
   then waited for the joiner's 0xcc forever. An unforced start threw `XNetwork` (net_playersReady) every frame,
   because the ready flags are reset after a mission. Fix: mark every other player not connected (`node+0xf = 0`)
   and run the original forced start.
3. **Joiner's world froze in the next session.** On the world map the joiner still received the host's solo-start
   messages for the old session (a phantom session start), which left its client's go flag `+0x24d = 0`. Its tick
   gate `0x635ce0` never opened (`started=1/0`, frame 0). Fix: the joiner ignores S->C messages from the old session
   while on the world map, until it follows.
4. **Joiner was sent back to the world map about 1 s into the mission.** The mission screen was closed (result 16)
   from `0x47a680`, the "whole squad is on an exit grid" check. It walks the units of the UI local player
   (`0x65af20`) and returns early on the first unit not on an exit. The host's save carried a player record with the
   joiner's id and **no units**, so the joiner resolved to that empty slot, and an empty squad counted as "everyone
   at the exit". Fix: a co-op joiner with a shared squad always uses slot 1 in `0x65af20`.

RE notes: `0x825074` (vtable) is the loading-screen window (its run, slot 11, returns 4 when loaded), not the
mission. The mission screen is WGameMain (vtable `0x801ac8`, close slot 14 at `0x6c6260`). Ghidra missed
`0x47a2e0` and `0x47a680`; create them by hand. The campaign loop is `0x638fc0` (world map when the next-map string
at `*(0x8bdd2c)+0x10` is empty, otherwise the mission).

Tests run after the last change: transition 19/19, join_move 15/15, resync 18/18. Full suite not rerun yet.

## Connecting: the Multiplayer menu, and drop-in join (2026-10-21)

The author's choice: use the game's own Multiplayer menu and skip the deathmatch lobby in co-op; players can join a
game already in progress.

How it works now:

- **Multiplayer menu.** WMultiSetup (ctor `0x4a84a0`, vtable `0x805228`) is shown by the multiplayer loop
  `0x637720`, which the main menu's Multiplayer button runs (`0x6c1ba0` -> `0x6c0c10(1)`: new Client, then the
  top-level loop nested). With the mod, the **Host** button (`0x4a9a20`) and the **Join** button (`0x4a9820`) are
  replaced by jmp hooks (`[coop] mp_menu=0` keeps the originals). They set the role and close the screen with result 2;
  anything other than 4 returns from `0x637720` to the main menu. Join reads the screen's IP field (FOTString,
  wide, at +0x135; a host name is resolved to IPv4) and the name field (+0x139).
- **Host:** after Host, load a save or start a campaign as usual. The host plays at once; there is no lobby to wait in.
- **Joiner:** after Join, the main menu stays up until the host holds a co-op lobby, then it fetches the host's
  save and loads it.
- **Drop-in.** Every 2 s each joiner polls `GEN <id> <name>`. The reply is `GEN <gen> <lobby> <save>`. A joiner
  that registers after the host's session started makes the host re-sync (quick-save, quick-load, new generation,
  co-op lobby). The lobby starts once every live joiner (polled in the last 6 s) is ready, or after 30 s.
- **Hooks.** All hooks are installed at start and act only once a role is set; `[coop] mode=host|join` still sets the
  role at start for the tests.

Original handlers, for reference: Host opens WCreateServerBox (`0x4aa890`: name, password, max clients; OK returns 4),
then builds a DirectPlay network and Server (`0x63eb70`), copies the game settings from `0x8bd9c0`, and closes the
screen with 4. The loop then runs the deathmatch lobby WGameSetup (`0x482540`) and the game (`0x638260`).

Tests (both copies muted: `core/bos.cfg` `sound.masterVolume` set to 0 in the test copies only):
- `menu_join`: 17/17. Both copies press Multiplayer, then Host or Join through the real handlers, from co-op mode off.
- `dropin`: 20/20. The host plays alone; the joiner starts 10 s later, joins through the menu; the host re-syncs
  as generation 2; both move units in generation 2; all states identical.

Not yet checked by a test: whether the real Join button is clickable with no session selected in the list (the bot
calls the handler directly), and what the joiner sees while it waits (nothing shown yet, log only).

## Drop-in hardening, QuickSave guard, notices (2026-10-21)

Found by the full suite (the standalone drop-in run had passed): under load the host's single-threaded file server
stalled the joiner's 2-second polls for 20 s and more. The host took the joiner for gone, started its lobby without
it, then counted it as newly arrived on every return and re-synced in a loop (generations 2, 3, 4). The joiner also
crashed once during those repeated rejoins, an access violation in a system DLL after a C++ throw. It may be a knock-on
of the loop; it was not investigated further.

Changes:

- File server: one thread per connection.
- Polls report the joiner's state, `GEN <id> <W|L|P> <gen> <name>`: W is waiting at the menu, L is loaded or in its
  lobby, P is playing. The host re-syncs only when a live joiner reports W in a poll made after the current session
  started, or reports L 15 s into a session that started without it. At most one re-sync per joiner every 20 s;
  joiners count as live for 15 s after their last poll.
- QuickSave guard: before a re-sync quick-save, the host's `QuickSave.sav` is copied to
  `ftcoop_quicksave_backup.sav`. The re-sync save is served as `ftcoop_resync.sav`. At the next session start, the
  original is restored, or the re-sync's quick-save is deleted if the host had none. A leftover backup is restored at
  game start.
- Notices in the game's chat line, through Client::ShowChat `0x6330a0` (`__thiscall(Client*, int player, FOTString*
  text, char team)`, `ret 0xc`, used by the player-left handler `0x6361f0`): "<name> joined mid-session - saving and
  reloading to bring them in", "<name> left", "Co-op: N player(s) in this game", and "Co-op: in the host's game" on
  joiners. `0x6602b0` is a modal OK box (wmisc_gui.cpp), so it is not used for in-game notices.
- Join button: WMultiSetup enables Join only while a listed session is selected (`0x4a9cd0`: list at +0x125, button
  at +0x131, `0x6d1350(widget, enabled)`, flag at widget+0x102). In co-op the mod keeps it enabled, because a co-op
  host lists no session until it holds a lobby.

Full suite before these changes: 11 of 12 as expected (every PASS test passed, every mutation failed); dropin failed
as described above.

## Four players (2026-10-21)

There are four test copies (`local/test-install`, `-2`, `-3`, `-4`; copies 3 and 4 are cloned from copy 2), with
windows in a 2x2 grid set by `ddraw.ini` posX/posY. The launcher option `--joiners 1..3` starts the extra copies, and
each joiner gets `[test] index=N`, so in `simul` joiner N orders squad member N.

- Run 1 (`simul --joiners 3`): 28/29. All four connected and the four simultaneous orders synced. **One divergence:**
  raider 101 had 33 HP on the host and on joiners 2 and 3 at frame 439, but 21 on joiner 1. The game's own desync
  check (0xcf) did not fire. Whether the tester touched joiner 1's window during the run: unknown.
- Added in test mode: an `HPCHG s= f= id= old->new` line for every actor HP change, every frame.
- Run 2 (same test, no input): 29/29. All four copies logged the same 8 HP changes on the same frames: raiders 66,
  70 and 100 die, and 101 is hit at frames 338 and 440.

The tester confirmed afterwards that nobody touched joiner 1 during run 1, so the divergence is a real nondeterminism,
not input.

- Run 3 (`soak --joiners 3 --soak-frames 2000`): the bots timed out, because four copies on one PC run at about 5 frames
  per second (code 3, timeout 300 s). Up to the timeout, the World's random state (World+0x729, logged every frame as
  `RNG s= f= v=`) was identical on all four copies for 1477 frames, and all 29 snapshots matched.
- RE: `0x505b00` returns the World's random state: `&World+0x729`, or, while the flag `0x8bdd8b` is set, a seed reset
  to the frame number (World+0x2eb). The seed is written into saves (`0x4eaed0` / `0x4eb470`). The game draws from it
  inline with the MS LCG (`x * 0x343fd + 0x269ec3`); 88 callers. A hook logging draws outside the client tick
  processor found about 50 callers, but they are the World's own update (around `0x4fc2xx`), which runs next to the
  hooked call rather than inside it. Not a bug; the hook was removed.
- Open: the run-1 divergence has not reappeared. The per-frame RNG and HPCHG lines stay on in test mode, and the
  launcher compares RNG frame by frame for every joiner, so the next occurrence will show its first frame.

## Menu flow as built, waiting message, bunker screens (2026-10-21)

The author's target flow is built and was checked by the tester with real clicks:

- Host: Multiplayer, then **Start Co-op**, then the Load Game screen.
  - Start Co-op is a new button at (280, 390) on WMultiSetup, added by wrapping the screen's constructor call
    (`0x637767` -> `0x4a84a0`).
  - Button anatomy: allocate 0x15b with `0x6c4dd0`. Make a callback with `0x4ac1d0(owner, &tmp, handler, 0)`.
    Construct with `0x65bcb0(button, parent, int pos[2], callback, FOTString* text, style)`, `ret 0x14`. Then
    unregister the temporary callback with `0x6c7d40(owner, &tmp)`.
  - **Handlers are `__thiscall(owner, int arg)` with `ret 4`.** The dispatcher at `0x7053ea` pushes one argument.
    A handler that returns with a plain `ret` crashes on a real click; the bot missed this because it called
    handlers directly. The bot now calls through a `__thiscall(owner, arg)` pointer.
  - The Load Game screen comes from the Single Player Load handler, `0x4ca7b0(parent)`.
- Joiner: Multiplayer, then select "[Co-op] <name>" in the normal session list, then **Join**. Normal sessions go to
  the original Join through a 5-byte trampoline.
  - The host names its DirectPlay session "[Co-op] <name>": `Server+0x11` is set before `AddNetwork`.
  - Session records are 0x85 bytes, between `client+4 -> net+0x90` and `+0x94`: +0 vtable, +4 type name, **+8 name**
    (FOTString), +0xc password flag, +0xd players, +0x11 max players, +0x15 dwUser1.
  - DirectPlay does not give the joiner the host's IP. The joiner asks over UDP on port 47701 ("FTCOOP?" sent to the
    IP field's address, loopback and the broadcast address); the host replies "FTCOOP! <session>" and the reply's
    source address is the host.
- Waiting message: `0x6602b0(parent, FOTString* text)` (cdecl) shows a modal WBOSMessageBox (vtables 0x827ce0 /
  0x827ce8). The box's own tick fetches the host's save once the lobby is open and ends the box (+0x34); then the
  pump loads the save. Clicking OK first stops waiting.
- Bunker screens: buying and selling (WBarter 0x7ff758), recruiting (WRecruit 0x80884c) and squad management
  (WSquadWindow 0x8081f8, WSquadSelectionWindow 0x80838c) send no network messages. In co-op they are host-only. When
  one closes on the host while joiners are connected, the host re-syncs; on a joiner the screen is closed at once
  with a notice. WBarter is opened by `0x471ad0` event types 6 and 7 (with a merchant entity).
- Tests:
  - `menu_join`: 23/23, through Start Co-op, the session list, UDP discovery and the waiting message.
  - `bunker` (`bunker.sav`, made by the tester): 17/17. Co-op in the bunker, a move, then a simulated host-only
    screen close: the host re-syncs and the joiner follows with identical states.
  - The random-state check now fails only on a lasting divergence. A one-frame blip on the host's re-sync frame
    (quick-save while the joiner still runs the old session) is reported but not failed.
- Not verified yet: detection of the real WBarter and WRecruit windows (needs a merchant; the vtables come from RTTI)
  and closing them on a joiner.

### Live bunker session, and conversations that belong to one player (2026-10-21)

Observed in the tester's live co-op session on `bunker.sav` (host and joiner on one PC):
- Talking to the recruiting NPC sent one command 0x17 from the host's client: squad member 0x03bc, NPC EntityID
  0x0051000a. **WSpeech opened on both copies**, because the talk is a lockstep command.
- The host then recruited. The windows that ticked were WSingleRecruit, WPortraitWindow, WListBox, WRecruitItem and
  WBackground. **WRecruit never ticked through the shared window tick 0x6c5130**, so the host-only check (RTTI vtables)
  missed it and no re-sync ran. The next squad move carried 5 members instead of 6. Tester: "can confirm it gets out
  of sync once party members change".

Built (derived from the above):
- Conversations and what follows them are private to the player who started the conversation:
  - The client-send function 0x728840 (`char __thiscall(Client*, msg, len, reliable)`, `ret 0xc`; the first 5 bytes
    go to a trampoline) notes this copy's own talk commands.
  - The host also notes joiners' talks in its server receive.
  - When WSpeech first ticks, the copy that sent the talk in the last 10 s keeps it. Every other copy closes it with
    vtable +0x34(0). A conversation nobody sent stays on the host (scripted ones).
- Screens are matched by RTTI name (WSpeech; anything containing Barter, Recruit, SquadWindow, SquadSelection). When
  none has ticked for 1 s they count as closed, and the talker's game becomes everyone's:
  - Host talker: the normal re-sync (quick-save, quick-load, joiners follow).
  - Joiner talker: it quick-saves (its own QuickSave.sav kept aside and put back) and uploads the file to the host
    on the side port: `PUT <id> <size> <name>\n` + bytes, answered `OK\n`, stored as
    `user/save/ftcoop_upload.sav`. The host loads it the way a quick-load does (assign 0x8bddb4, close WGameMain
    +0x38 with 0x10), serves it as the next generation, and every joiner follows.
- The host-only rule and its "only the host can trade" notice are gone.
- Every conversation now ends with a reload of a few seconds, even one with no trade. Dialogue choices can change
  quest state, so this is on purpose.
- Two players trading at the same moment: the later save wins, and the other player's trades are lost. Not handled.
- New tests `talk_host` and `talk_join` (the old `bunker` test is gone):
  - The bot sends the tester's exact talk command, then changes one HP on its own copy (standing in for a trade) and
    ends the conversation.
  - Checks: the other copy closed the conversation, the save went the right way, and session 2 carries the change on
    both copies.

Found while making the tests pass (observed in test logs unless marked):
- The talk command does nothing from across the room. No direct move towards the NPC moved anyone either: not the
  the tester's last spot, not a nearby waypoint, for either squad member tried. Following the tester's own route
  (6 legs, decoded from their squad moves) works.
- While a conversation is open, the game runs no frames. The bot acts from the conversation window's own tick.
- **The conversation is a mode of the mission screen.** `WGameMain::SetMode` 0x471ad0 (`__thiscall(WGameMain*,
  int)`, mode at +0x156) builds each mode's windows and closes the previous mode's. 12 = conversation (0x4cf4b0),
  6/7 = barter (0x453e00 with 0/1); other modes' windows: unknown. Closing only the window (+0x34) left mode 12
  set, and quickSave 0x478340 refuses outside modes 0, 1 and 10, so the save was never written. Conversations
  are now ended with `SetMode(0)` from the mission screen's own tick.
- quickSave also refuses while +0x15a (saving) or +0x15c (countdown) is set, or when 0x4782a0 fails (World+0x7c9
  set, or no squad member passes vtable+0x480).
- **Every copy runs its own Server, a joiner too** (its single-player lobby's, idle once it joined). A save stores
  that Server's tick numbers: +0x95 last sent and +0x99 next, frame+1 and frame+5 while running.
  - A joiner's local Server stayed at its load (1954/1957, world at frame 2546). The host, loading the joiner's
    save, sent ticks from 1961, and both copies waited about 42 s for them to reach the world's frame.
  - Setting the host's Server after the load stopped the ticks entirely.
  - Fix: before a joiner saves for the host, it sets its local Server to frame+1 / frame+5.
- Results:
  - `talk_join`: **25/25** (log: `join: local Server ticks 1954/1957 -> 2547/2551`).
  - `talk_host`: **22/22**. It was run before the final joiner-only change, which does not touch the host's path.
- Both tests were added to `run_suite.py`. The full suite has not been run since the menu, popup and conversation
  changes.
- Not verified with real clicks yet: barter and recruit windows reached from a real conversation, a real conversation
  ended from its own dialogue options, and squad management screens opened without a conversation.

### Bunker trading by bot, and inventories in every test (2026-10-21)

The tester's live session sat idle for 95 minutes (no squad moved), so trading was tested by bot instead.

Found (decompiled, BOS.exe sha256 c5b91fe5...):
- `WGameMain::SetMode` 0x471ad0 modes: 1, 4, 5 (squad-member screens, built only for a valid entity), 2 (0x4817b0),
  3 (0x458b40), 6 barter (WBarter 0x453e00 flag 0), 7 the same with flag 1, 8 (0x4d6060), 9 (0x4b3c00),
  10 (0x4cea20), 11 (0x4b71c0), 12 conversation, 13 pause ("paused", "pressAnyKey"), 14 (0x63fc50), 15 quit prompt.
  Meanings other than 6, 7, 12 and 13: unknown.
- Conversation buttons are WSpeech methods (`__thiscall(WSpeech*, button)`, `ret 4`):
  - 0x4cffd0 ends the conversation (SetMode(0) if it is WGameMain+0x136, then closes itself);
  - 0x4d0000 Barter: copies the speakers' EntityIDs (WSpeech +0x100..+0x106) to 0x8bc218 (buyer) and 0x8bc208
    (merchant), then calls World+0x2b9's vtable +0x28; the barter screen (mode 6) is up when it returns;
  - 0x4d0090 sets the same pair and calls SetMode(7) (FTSE notes: gamble).
- WBarter's constructor raises the world's pause counter (0x4fc800(1)) and its destructor lowers it, so no game
  frames run while trading, and the barter screen's windows tick many times per game frame.
- Inventories (FTSE `Actor.h` / `Inventory.h` / `Collectable.h`; their offset comments count from the entity's start):
  Actor +0x1006 inventory EntityID; Inventory entity (vtable 0x818af8) +0x1a9 circular list (node: next, prev,
  EntityID); item count at +0x1ad. Actor +0x1055 isquartermaster, +0x1056 canbarter.

Observed in the trade tests:
- The bunker's merchant is NPC 96 (the tester's run 2 also talked to entity 0x60 before buying and selling). Squad
  member 0 reaches it by the tester's first two route legs plus a step to (359.5, 32, 320.5).
- The merchant holds 2 stacks of its own. With the barter screen up it holds the shop's stock (30 stacks). After the
  screen closes and the game reloads, it has its 2 stacks again: the stock exists only while barter is open, and an
  item sold to it goes with it. Both copies agree, so this is the game's behaviour, not a sync fault (derived).
- While a private screen is open, a mission-screen mode other than 0, 1 and 10 now also counts as "still open"
  (windows like WRecruit tick on their own, not through the shared hook).

New bot tests (`trade_host`, `trade_join`, bunker.sav):
- Before the talk, the non-trading side moves squad member 1 and the trader moves member 2. Session 1 must match on
  both copies up to the talk's frame: positions, HP, random state and inventories.
- The trader's member 0 walks to the merchant and talks. The bot presses the conversation's real Barter button
  (0x4d0000); the barter screen opens on the trader's copy only. After 2 s it leaves the screen with SetMode(0).
- Then it moves one item each way with the inventory functions the game exposes to scripts (Entity vtable +0x508
  RemoveInventory, +0x504 AddToInventory, as FTSE calls them). **Not covered:** the barter screen's own
  drag-and-offer and money. Which functions it calls is unknown.
- Checks: session 2 (the trader's game) has the squad member's new inventory on both copies, and every inventory
  matches the trader's at the moment it brought the others up to date.
- Every test now also logs each Actor's inventory in its snapshots (`INV s= f= id= items=id:count,...`). The runner
  checks that every inventory is identical on both copies at every shared frame.
- Results: `trade_host` **30/30**, `trade_join` **33/33**.
- Full suite before these changes (old build, 14 tests): all as expected except `dropin`, 29/30. Its check looked for
  the pump-load wording, but the joiner now loads through the waiting message ("loading the host's save ..."). The
  check accepts both now.
- Full suite on the final build (16 tests, all with the inventory check): **SUITE: PASS**. Mutations failed as they
  should; dropin 31/31, trade_host 30/30, trade_join 33/33.

The author's decisions (2026-10-21):
- Squad control stays shared: every player can order every squad member, and the players sort it out among
  themselves.
- The rare divergence gets hunted with a 4-player stress test: many actions from all four players, checked for any
  desync.
- Next: recruiting and squad changes by bot.

### Recruiting by bot (2026-10-21)

Found (observed in a test log's window tree, then decompiled):
- A button (WBOSButton / WImageButton) keeps its handler at +0x112 and the handler's owner at +0x10e. Handlers are
  `__fastcall(owner, edx, arg)`.
- The conversation's four buttons (WSpeech children): Done 0x4cffd0, Gamble 0x4d0090, Barter 0x4d0000, Recruit
  0x4d0110. This matches FTSE's `WFTSESpeech.cpp`.
- Recruit opens mission-screen mode 8: WSingleRecruit (vtable 0x808bc0), with a list of WRecruitItem and a
  WPortraitWindow of the squad. Its buttons:
  - 0x4d4fe0 (image button): add the selected recruit (vtable +0x1a8 the selection, +0x1b4 add);
  - 0x4d5010 (image button): dismiss the squad member whose entity flags +0xb2 have bit 0x40. That bit is the
    selection: the talker had 0x52, the others 0x12;
  - 0x4d4e70: character sheet (a 0x195e window, 0x458b40);
  - 0x4d4e60: Done (observed: mode 0 afterwards, the squad kept).
- After a dismissal the recruit list's selection is the member just dismissed.

New bot tests `recruit_host`, `recruit_join` (bunker.sav):
- The recruiting player's member 0 walks the tester's route to the recruiter (NPC 81) and talks. The bot presses the
  conversation's Recruit button.
- On the recruit screen it selects and dismisses the last two squad members, adds one recruit, and presses Done.
  These are the screen's own button handlers, called the way a click calls them. Observed squad on the recruiter's
  copy: 6 (1055, 997, 957, 969, 956, 308) -> 4 -> 5 (the last dismissed, 956, recruited back).
- Checks: the recruit screen opened on the recruiter's copy only, and the squad changed. Session 2 has the
  recruiter's 5-member squad on both copies, in the same order.
- Every test now also logs the campaign squad in its snapshots (`SQUAD s= f= n= ids=`). The runner checks the same
  squad on both copies at every shared frame: the bug the tester reported.
- Results: `recruit_host` **28/28**, `recruit_join` **31/31**.
- Full suite afterwards (18 tests, all with the inventory and squad checks): **SUITE: PASS**.
### Real barter deals, with money (2026-10-21)

Found (window tree in a test log, raw bytes decoded by hand, decompile of the deal):
- The barter screen (mode 6), as SetMode keeps it:
  - WGameMain+0x10e: WSquadEquip, the buyer's inventory;
  - +0x11a: WBarter, the merchant's stock;
  - +0x11e: WBarterTable, the two offer tables (WInvActor at table +0x104 and +0x108, each with its total value at
    +0x147). It has two buttons:
    - Offer 0x455830 (thiscall): `if (value(+0x104) >= value(+0x108)) deal()`;
    - Done 0x455880: SetMode(0).
- **Every Actor has a barter offer:** a second Inventory entity whose EntityID is at +0x100a (vtable slot 406, FTSE's
  GetUnk22EntityID; slot 402 GetInventoryList). The bunker run: buyer 1055 offer 320, merchant 96 offer 265.
- The deal, 0x4558a0: for each item in the buyer's offer and in the merchant's offer, RemoveInventory (+0x508) from the
  offer and AddToInventory (+0x504) to the other side. The pair comes from globals 0x8bc208 / 0x8bc20c (merchant)
  and 0x8bc210 / 0x8bc218 (buyer).
- The table values update by themselves when items land in the offers: 20 and 2 after the bot's moves.
- **Money is ringpulls:** `sprites/items/Money/Ringpulls.spr`, value 1 each (the buyer had 250). An entity's sprite
  name pointer is at +0x105. A Collectable's value is at +0x1b6. Inventory entities (vtable 0x818af8) have real
  add/remove functions (0x5b4030 / 0x5b44e0). A Container (vtable 0x817a2c) keeps its inventory EntityID at +0x1aa.

The trade tests now make a real deal:
- The bot puts the merchant's cheapest item into the merchant's offer and 3x its value in ringpulls (at least 20)
  into the buyer's offer, then presses the barter screen's Offer button. Observed: 20 ringpulls for one 9mm round;
  the buyer went from 250 to 230, and both offers emptied.
- The direct item move from before is gone. Not driven: the drag-and-drop itself (the bot fills the offers with the
  same inventory functions), and the merchant's price rules (the table only checks offered value >= asked value).
- Results: `trade_host` **32/32**, `trade_join` **35/35**.

- Planned (the author asked for bunker sync without a reload): replay each private-screen action as a co-op lockstep
  command, so every copy runs the same game function on the same frame (new item and recruit entities then get the
  same ids everywhere). Then a checksum of squad, inventories, money and quest variables on every copy. A match means
  no reload; a mismatch, or a change nothing replays, falls back to the save and reload. A conversation that changed
  nothing would skip the reload. Order: the barter screen's own deal function first (with money), then the
  checksum, then recruit replay, then barter replay.
- Not covered yet: the multi-recruit screen (WMultiRecruit, "gui/back/recruitMulti"), recruit costs or limits, the
  squad management screens opened without a conversation.

### Trades and recruits without a reload (2026-10-21)

Tester: "once that's built i no longer want to save/reload when making these changes its annoying"; the other players
should keep playing; "if it doesn't have it, write it and sync it"; "create a lock on the trader so nobody else can use
them while a player is". Code: `local/src/ftcoop/logger/private_sync.inc`.

Found (observed in test logs unless marked):
- Lockstep and pauses. While the host is on a private screen, every copy stalls: the ticks come from the host's world.
  While a joiner is, the host played on (trade_join: host frames 2277 -> 2343 while the joiner sat at 2305).
- Entity table entries are `{ptr, i16 next_free, i16 seq}` (FTSE `World.h`), with the free-list head u16 at
  World+0x74f. Opening barter creates the shop's stock and closing it destroys it again. That alone moves 61 entries
  and the head (320 -> 265) on the trader's copy only.
- The game serializes an entity to memory the way the level editor's undo does (`world_undo.cpp`, 0x42ec00):
  - a memory streambuf: 0x14 bytes, base 0x426830, vtable 0x7fde04, an FOTString buffer at +0xc;
  - an archive on it: 0x704060 `thiscall(archive, 1, streambuf, 1)`;
  - the saver 0x5eabd0 `cdecl(archive*, EntityID)`. The game's save-entity-to-file 0x5ea630 calls it the same way.
  - Output: `<entity>`, the class name, then the properties. Bug in the Shell was 6301 bytes.
- MakeEntity 0x5ea400 (FTSE CreateEntity) reads such a file and creates the entity in the slot at the free-list head.
  It reads the whole file into memory, wraps it in streambuf vtable 0x80948c, and loads it with 0x5ea6d0, which also
  sets entity +0x5d from the counter 0x8be148.
- A bought item can merge into an existing stack: the 9mm round went to the buyer's 9mm stack, 135 -> 136.
- The recruit screen's dismiss path (0x4d6980) removes the actor from the map grid on World+4 (0x6e5ab0, 0x6e9bc0).
  The re-add places the recruit next to the squad: 956 jumped to (333, 32, 339). The dismissed actor's entity is
  destroyed.
- DestructEntity (vtable slot 10) finishes during the next frame, so a receiver's free list changes once more after
  the delta.
- Walking characters creep a fraction of a unit between frames even while the world is paused.
- The world pause counter: 0x4fc800 `thiscall(World*, bool)` (+1 / -1 on World+0x2db), `ret 4`.
- An Actor's command handler 0x5747e0 calls the actor's vtable +0x660 for every command before its switch, so a
  custom command type must not reach it.

Built:
- **NPC lock.** A talk command (0x17) locks its NPC on every copy, on the frame it runs. The lock is lifted by the
  talker's `kSyncDone` co-op command.
  - A copy refuses its own player's talk to a locked NPC (client send) with a notice.
  - The host also drops a second player's talk to a busy NPC from the message (server receive), for two players
    clicking at the same moment.
- **Hold.** Every copy but the talker's holds its game (pause counter +1) from the frame the other player's
  conversation opens; that is the same frame everywhere, and the talker's world is stopped then too. It lets go when
  the talker's `kSyncDone` arrives. A hold that waits over 10 minutes, or whose players have left, is released.
  - **This differs from what the author asked for ("keep playing"):** a first build without the hold drifted
    (recruit_join: an NPC walked round the moved squad member on one copy only).
- **Delta instead of a reload.** When the screens close, which is the first frame the talker's world runs again, or
  1 s without a screen tick, the talker compares against its snapshot from the opening. It sends a delta as co-op
  commands (type 0x30, 18 bytes per command):
  - destroyed entities;
  - new entities as the game's own serialization, which the receiver creates in the same slot (free-list head pointed
    at it);
  - stack counts;
  - Actor inventory lists, in order;
  - the squad list;
  - squad members' HP, and their position when it jumped more than 2 units (map grid updated with 0x6e9bc0);
  - the table's bookkeeping and the serial counter.
- The other copies apply the delta when its network message arrives: the host from server receive, joiners from the
  tick before the client handles it. The ticks run a few frames ahead, so waiting for the lockstep frame would let a
  held copy fall behind.
  - The bookkeeping is written again two frames later, because destroys finish late.
  - Changed quest variables (a dialogue choice) still use the old save and reload.
- Every snapshot now also logs an entity-table hash (`ETAB`), and the runner compares it.

Results (each test covers the frames before the talk and every frame after the delta, on both copies: positions, HP,
random state, inventories, squad, entity table):
- `browse_host` **25/25**: barter opened, nothing bought, 61 entries synced, no reload. The joiner's talk to the
  busy merchant was refused.
- `browse_join` 24/25 on its run, failing only a check that looked for the old log wording; that check is fixed but
  has not been rerun.
- `trade_host` **34/34** and `trade_join` **35/35**: a real deal with ringpulls, a 555-byte delta, no reload, the
  other copy held for about 2 s.
- `recruit_host` **30/30** and `recruit_join` **31/31**: two dismissed, one recruited back, a 632-byte delta, no
  reload.
- The old `talk_host` and `talk_join` tests relied on the reload; `browse_*` replaces them in the suite. The full
  suite has not been run with these changes (the author asked for feature work first).

Leaving the bunker, and the world map (the tester: "on the map lets just have hte host allowed to move"):
- The bunker's exit grid: the tester left from about (370, 32, 343) in runtime run 3 (same bunker layout: same
  recruiter id, same route). A whole-squad move to (369.3, 32, 345.5) is enough: the mission's own exit trigger
  ("Players at Exit Grid") ended it on both copies.
- On a joiner, GoToMap 0x6b6580 (where a click on a world map location ends) is refused with a notice: only the host
  chooses. The host's choice starts its mission alone; it re-syncs and the joiner follows (existing path).
- `exit_host`: 23/24 on its only run, failing just a check that wrongly expected a second squad move. That check is
  now relaxed; exit_host has not been rerun with it. `exit_join` **24/24** with the relaxed check.

Not covered yet:
- a bought item that becomes a new stack (the 'C' path) and a recruit that is a new entity;
- quest dialogue (it still reloads);
- three or four players;
- the squad screens opened without a conversation.

## Cheats in the test copies: remove once we're done

For testing only, each test copy's FTSE user script (`<game dir>/core/FTSE/ftse.lua`) has squad god mode
(`GODMODE_SQUAD`, `OnDamageCalc`) and squad sure-hit (`SUREHIT_SQUAD`, `OnChanceToHitCalc`) under the netlog.dll
loader. They are not part of the mod. **TODO before shipping:** remove them so `ftse.lua` holds only the loader.

## Mod repo (2026-10-22)

The mod's code is public at https://github.com/thennak3/fallout-tactics-coop (MIT; source, test bot and runner,
FTSE loader without the cheats; no game files, saves or built DLLs). Release v0.1.0 (pre-release, 2026-10-22): `netlog.dll` SHA-256
383c3c2c687f418d467dcf15cb311871b6b4787c9ade4acc71b1d57ee92efa3c, built from commit cd073c1's source; `exit_host` 24/24 on that
build. Version check (observed): the tester's GOG install and all four test copies have the same `BOS.exe` (hash in
Identity); file by file, the test copies differ from the install only by FTSE, cnc-ddraw, the mod's files, saves and
`bos.cfg` (sound muted, last save). So the mod targets the unmodified GOG 1.27 game plus FTSE 0.60a.
The playbook write-up and draft PR wait until the mod is finished (the author's choice).

## Four players in the bunker, new stacks, quest variables, the merchant race (2026-10-22)

Observed (test logs in `local/re/test_runs/`, 2026-10-22):
- `trade_host --joiners 3`: 61/62. The host bought a kind of item it did not carry (a bullhorn trap, `.?AVTrap@@`), so
  the merchant's own entity, made when the barter screen opened on the host only, stayed in the buyer's inventory. The
  delta carried it ('C', 2208 bytes); all three joiners created it in the same slot (1057) with 0 conflicts, and all
  four copies matched at every shared frame (positions, HP, inventories, squad, entity table, quest variables). The one
  failure: the bot found no quest variable holding a number.
- Bunker variables (logged): 2 mission variables, `BUNKER_HAPPY="TRUE"`, `BUNKER_END="FALSE"`; 97 campaign variables,
  e.g. `CVAR_M01_COMPLETE="TRUE"`, `CVAR_M01_NPC_HAWKEYE="FALSE"`. Values are text, not numbers.
- Quest variables now go in the delta ('V': mission/campaign, index, name, value) instead of forcing a reload. The
  receiver sets them as a trigger does: FTSE's World::SetVariable, a command object with vtable `0x837b24` run by
  `0x67ee30` (Ghidra does not mark `0x67ee30` as a function). `trade_join --joiners 3`: every copy logged
  `mission variable "BUNKER_HAPPY" = "1"` from the delta.
- **Bug found and fixed: the merchant race.** In `trade_join --joiners 3` the harness made every joiner the trader. The
  host's lock dropped two of the three talks, but all three joiners took the conversation as theirs: every player's
  talk is the same command (the shared squad's member, the same NPC), and a copy only knew "I sent a talk in the last
  10 s". Fix: each player sends a claim (co-op command 0x30, kind 4, a random number) just before its talk; the host
  relays only the first claim and talk for a free NPC (`HostFilterTalks`), so every copy learns the winner from the
  relayed claim. A claim can arrive just before a loser's own talk goes out (seen: 16 ms), so a talk to an NPC someone
  else's claim won is refused on the sender's copy, and the relayed claim, not the 10-second window, decides whose
  conversation opens. Claim numbers first came from `rand()`, which runs the same sequence on every copy: two claims
  differed in two hex digits. Now QueryPerformanceCounter and the process id, hashed.
- `trade_race --joiners 3` (every joiner talks to the merchant at once), second run: exactly one winner, the host
  dropped the other talk that was sent, the third joiner was refused on its own copy; the trade synced to all four
  (66/68; the two failures were checks that expected every loser to have sent its talk).
- Harness fix: with `actor=join` only joiner 1 acts; `actor=all` (the race) makes every joiner act.

Read from the recruit screen's buttons (assembly; Ghidra marks no function at `0x4d68f0`, its bytes decoded by hand):
- Add `0x4d4fe0`: `pick = vt+0x1a8()`, `vt+0x1b4(pick)`, `0x4d5d60(pick)`, all `__thiscall`. `vt+0x1a8` (`0x4d68f0`)
  asks the object at +0x14f (`0x65af20`) for a record in World+0x733 (stride 0x15d, the players) and walks that
  player's members list (+0x4b).
- Remove `0x4d5010`: finds the squad member flagged +0xb2 bit 0x40 (count `vt+0x1b0`, member i `vt+0x1ac`), removes it
  (`vt+0x1b8(i)`), then selects that member's entry in the list box at +0xfa (`0x6cde50` gives the entries; entry
  +0x134 -> EntityID at +0x153/+0x155; `0x6cded0(listbox, index, 0)` selects). That is why add re-adds the member just
  dismissed.

Guess, not verified: the recruit pool is one of the World's players. The first search (a player other than the squad
holding the dismissed member, right after the dismissals) found nothing; the dismissed member 308 was destroyed (52
actors -> 51 on every copy), so whether recruits are entities before they are recruited is **unknown**.

- `recruit_join --joiners 3`: 54/56. The bot re-added the member it dismissed (pool not found). Divergence: two
  wandering NPCs (256, 246, 30 HP) took different paths on the recruiter's copy (joiner 1) from frame 2554-2604 on,
  while the host and joiners 2 and 3 agreed. All four paused at frame 2537 and resumed at 2538; the random state
  matched on all four throughout. Cause: unknown. The earlier unexplained 4-player divergence (2026-10-21, `simul`)
  was also joiner 1 alone; the copies' `bos.cfg` and settings are identical apart from line endings.

### Recruits, deferred destroys, big deltas (2026-10-22, later)

Observed (bot runs, logs in `local/re/test_runs/`):
- **Map placement.** After a recruit, wandering NPCs took different paths on the recruiter's copy. Memory dumps of
  every actor on three copies (two receivers agree, so differing pointers between them are noise) showed the moved
  member's bounds (+0x08..+0x1c ints, about position x 4, and +0x50..+0x6c) still at the old spot on the receivers.
  The recruit screen places a character with `0x4dfc60`, then `0x6e5ab0(World+4, entity)` when World+0x1a5 is set,
  then `0x6e9bc0(World+4, entity)` (`0x4d6980`, all `__thiscall`). The position op only did the last call; now both.
- **Per-tick command limit.** `0x63bc10` (commands.cpp line 22) stops the game with "Invalid number of commands
  passed" when a tick holds more than **1000** commands. A new recruit's delta (Actor 12.7 KB serialized, 23 KB with
  AI, inventory, armour and two weapons) took 1288. Now: LZNT1 compression (ntdll `RtlCompressBuffer`, 23 KB -> 6 KB),
  22 payload bytes per command, and deltas over `[coop] lockstep_chunks` (default 300) go through the host's file
  server (`DPUT` / `DGET`, save_transfer.inc); lockstep carries only the done record with size and hash.
- **Deferred destroys.** `DestructEntity` (vtable slot 10) sets **+0x141 = 1**; the entity leaves its slot during the
  next frame, keeping next_free and seq. The diff counts a marked entity as gone and puts its slot in the bookkeeping;
  the talker also rewrites its own bookkeeping after that frame, as the receivers do.
- **New characters cannot be copied slot for slot.** Made from its serialized bytes (`0x5ea400`), an Actor brings its
  children along, in a different order and even as different entities (a BaseAI, two inventories) than the recruit
  screen made. Chaining the free list in the recruiter's order did not help.
- Tried: every copy, the recruiter too, makes the new character from its bytes after the next frame (the recruiter
  destroys the screen's one). The character then matched slot for slot on both copies (slot 308) and the entity table
  matched, but NPC 46 later walked another way on the recruiter's copy. (First read as an AI flag at +0x117; wrong:
  +0x117 differs on every actor between copies even before the screens, its low bits always 0 or 8, part of a
  pointer. The likelier cause is the extra frame below.) Behind `[coop] recreate_recruits=1`, off by default.
- Tried and removed: copying every byte an Actor's memory changed during the screens. Nearly every actor changed
  +0x171..+0x172 to the same new value on the recruiter: part of a pointer to an object it had reallocated. A packed
  struct gives no safe way to tell such pointers from data, so raw copies are not used.
- So: re-adding a dismissed member and dismissing are delta-synced; **recruiting someone new is a save and reload**.

Derived: the recruit list box entries hold EntityIDs (+0x134 -> +0x153 seq / +0x155 id); `0x6cded0(listbox, index,
1)` selects one, and the add button then adds it. Dismissed members are destroyed (308: 52 actors -> 51).

- **The talker ran one frame more.** Exact position bits logged at the snapshot and at the diff: the talker's world went
  from frame 2533 to 2534 between them (the screens closed, the world resumed, and the diff ran on that first frame),
  while every other copy stayed at 2533 and ran 2534 only after applying the delta. NPCs walking in that frame (93,
  239, 246, 256, 419, 432, 441) are the ones that drifted in the failing recruit runs. Now the talker holds its own
  world too (the pause counter `0x4fc800`, World+0x2db) from the conversation's opening until the diff is made, and
  the screens count as closed when nothing private has ticked for 300 ms. Snapshot and diff are now on the same frame
  (2533). Full suite before this change: 24 of 26 as expected (simul: a move of 5.07 instead of 8 units, the same on
  both copies, then 21/21 on a rerun; recruit_host 3 players: the NPC drift, 42/44).

### Recruiter candidates; v0.2.0 (2026-10-22)

- Observed: after the talker-side hold, positions (bit for bit), the random state and the entity table matched on both
  copies right after a recruit, yet NPC 46 walked another way about 200 frames later (full suite run 2: recruit_host
  31/32). While the recruit screen is open the recruiter's world holds 8 more actors, at (0, 0, 0): the list's
  candidates (the list entries' EntityIDs), made on that copy only and destroyed at close. Guess, not verified: making
  and destroying them reorders something NPC updates walk through.
- So every recruiter visit is a save and reload for now (issue #1 on the mod's repo). The planned fix is the tester's
  design for the shop: run a screen's allocation on every copy at the same lockstep frame, the window on one copy only.
- Full suite run 2 (before that change): 25 of 26 as expected. After it: recruit_host 26/26, recruit_join 27/27,
  recruit_host with 3 players 37/37, trade_host with 4 players through the host's file server 64/64.
- Mod repo: commit 0d43e0f, release v0.2.0 (pre-release), `netlog.dll` SHA-256
  1d2dfb4b46e583de7b3d555eecb6ffddeb4852a0ad725326b4e8b9c0da6d792e.

The author's decisions (2026-10-22): shopping must not pause the other players; recruiting may (it changes the characters
everyone plays). Next: barter opened through lockstep, the shop's stock made on every copy at the same frame (window
only on the shopper), the shopper's character locked against orders while it shops, no hold for barter, the delta
applied at its lockstep frame; the recruiter refused with a notice while anyone shops.

### The quartermaster's shop, for a shop that pauses nobody (2026-10-22)

Observed (hardware write watch on the free list's head World+0x74f while the bot pressed Barter; Ghidra):
- Every entity allocation writes the head in `0x4e525c`. Opening the bunker merchant's barter allocates only from the
  quartermaster barter screen's constructor `0x454330` (vtable `0x7ff8ec`), called by SetMode (`0x4720e5`).
- `0x454330`: sets the pair globals (0x8bc20c merchant, 0x8bc210 buyer, copies of 0x8bc208 / 0x8bc218); `0x5b3a60()`
  makes a new Inventory entity (each call: watch hits 1-3); buyer and merchant each get one as their offer inventory
  (`vt+0x65c`, FTSE slot 407 ResetUnk22Entity, EntityID by value). If the merchant `vt+0x3f4` (253 IsQuartermaster):
  `vt+0x64c` (403 GetInventoryID) keeps its own inventory's ID, a third `0x5b3a60()` makes the stock inventory, and for
  each template in the campaign's stock list (`*0x8bdd2c` +0x151 begin, +0x155 end: FOTString* templates; +0x161:
  counts) with a count above 0: `MakeEntity 0x5ea400(&item, &template, -1)`, `item vt+0x1a4` (105 SetCount)(count),
  `stock vt+0x504` (321 AddToInventory)(&result (InventoryActionResult, 32 bytes), item, &id, count, 1), the item
  destroyed (`vt+0x28`) if the add fails. Then `merchant vt+0x650` (404 SetInventoryID)(stock).
- Its destructor `0x454f40`: for a quartermaster, every item left in the stock goes (`vt+0x188`, `0x6a6550`), the
  stock inventory is destroyed, the merchant's own inventory is set back; then both offer inventories are destroyed
  (`vt+0x658` each, `vt+0x28`) and the pair globals cleared.
- The other barter screen's constructor `0x453e00` raises the pause counter (`0x4fc800(1)`); the quartermaster's does
  not. During the bunker trade the counter is 1 from the conversation itself (before the mod's own hold), 2 with it.

Plan (the author's design): the Barter press becomes a lockstep command; at its frame the shopper's copy opens the screen
(which makes the stock) and every other copy makes the same stock with the calls above, no window; the shopper's world
keeps running (the conversation's pause cancelled on its copy); nobody holds; the shopper's character is locked; at
close the delta goes as lockstep commands, and at the close command's frame the others apply it and run the
destructor's steps.

### The shop that pauses nobody: built (2026-10-22)

`[coop] shop_no_pause` (default 1). For a conversation with a quartermaster (`vt+0x3f4`) who is not a recruiter
(`vt+0x3f0`, slot 252), nobody holds and the talker's own world keeps running: the pause counter `0x4fc800` is hooked
(6-byte trampoline) and its changes are swallowed on the talker's copy while that conversation is up (balance checked
back to 0). The Barter button `0x4d0000` (7-byte trampoline) sends co-op command kShopOpen instead of opening; at its
frame the shopper calls the real handler (the screen opens synchronously, its constructor makes the stock) and every
other copy repeats the constructor's allocations (`HeadlessShopOpen`). The quartermaster screen's destructor `0x454f40`
(7-byte trampoline) first runs `0x4569a0` itself (hands back the offer tables), measures the deal (diff mode 2: the
buyer's and merchant's inventories, new stacks, destroys, quest variables, bookkeeping) and sends it with kShopClose;
at that frame the others apply it and repeat the destructor's steps (`HeadlessShopClose`, campaign stock list rebuilt
from what is left). Orders for the shopper's character and talks to a recruiter are taken out of other players'
messages while a shop is open, with a notice. The conversation's own close sends only variable changes (diff mode 1),
applied at their frame.

Found on the way (each a real desync, each fixed and covered by a test):
- An entity made from serialized bytes keeps its own EntityID copy (a GenericItem at +0x146): given the talker's seq in
  the table but not there, the game refused to destroy it (`FixSelfId`).
- An entity made from bytes takes its template name (+0x199) from the temporary file (`ftcoop_entity.tmp`), and the
  quartermaster's stock list, rebuilt from those names, differed; 'C' now carries the name. This affected every
  entity the mod made from bytes, so saves made after such a sync could carry the wrong name before.
- Destroying the stock inventory takes its items with it, but not one only written into its list: its items are
  destroyed first, in list order.
- A slot freed and reused within one deal (spending the last ringpulls): destroys go before creates, a create whose
  slot is still being destroyed waits a frame, and when a delta destroys anything its bookkeeping and the shop's close
  wait for the next frame, so the destroys' completion does not change the entries again.
- A forced slot is unlinked from the free list first (`UnlinkFree`), so it is never handed out twice.

Tests (bot runs, 2026-10-22): trade_host 46/46 (shop open at frame 2305 on both, stock of 29 made headless, the
joiner ran frames 2305..2337 meanwhile, locks refused with notices), trade_join 45/45, trade_race with 4 players
73/73, trade2_host 30/30 (two visits, a sale, the last ringpulls spent; the campaign stock list, inventories and
entity tables identical throughout).

### Selection dots (2026-10-22)

A coloured dot on a squad portrait for every player who has that character selected (host red, then blue, yellow,
green by join order; a dropped joiner's colour is freed). Side channel, not lockstep: "SEL <joiner id> <slots>" on
the file port about 5 times a second, answered with every live player's colour and selection.
- Squad bar portrait: WPortrait, vtable 0x806d44. It overrides only slot 0 (destructor 0x4bcdf0, ret 4), slot 4
  (draw 0x4bc7d0, __thiscall(this, renderer), ret 4) and slot 6 (0x4bcde0, returns 1). Character EntityID at +0x153
  (slot in the high half). Window size 57x75.
- Renderer vt+0x24 draws a rectangle outline {x0, y0, x1, y1} as ints in the window's coordinates, with a pointer
  to an ARGB colour (the portrait's selection frame). Floats there draw nothing visible.
- Portraits draw only when invalidated (six draws in a whole session): window base slot 3 (0x6c7260,
  __thiscall(this), plain ret) invalidates the whole window, slot 2 (0x6c7390) a rectangle.
- Selection: character +0xb2 bit 0x40. The game's select-all 0x49f6b0 and a screen's deselect 0x4cfc10 also stamp
  +0x13c with world +0x2ef plus 1. 0x49f990 makes the first selected character the lead (0x49f8d0); it is not a UI
  refresh, and calling it from the client tick crashed. Setting the bit directly (test only) leaves the deselected
  portraits drawing black on that copy; a real click goes through 0x509bd0 / 0x50a1a0 (not followed). Posted
  WM_LBUTTON messages do not reach the game's input.
- Test captures: PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT) on the game window works (800x600).

Tested: host_move 25/25 (the joiner is blue; its narrowed selection, 959,984, shows on both copies; screenshots
show red on all six portraits and blue on the joiner's two). Tester check (2026-10-22, by hand, in the running test copies): clicking characters as a player updates every copy's dots. Not tested: colour reuse after a drop.

### A merchant freed a frame early on the shopper's copy (4 players, 2026-10-22)

trade_host with 3 joiners drifted: actors 300 frames after the shop stood 2 units apart, the shopper's copy the odd
one. The end of a light conversation freed its NPC when the co-op record arrived, and the shopper's own record comes
back a frame before the others get it (merchant freed at 2347 on the host, 2348 on the joiners). Fix: a record
applied at its frame (flag 4) frees its NPC in CoopCommandAtFrame, on every copy at the same frame. After it, 19 of
20 runs of the 4-player test passed; 8 of them logged a hash of every actor's position every frame (2200-2700) and
matched on all four copies at every frame. The one failure (run 164447, right after the tester had been clicking in
the test windows): NPC 46 stood still on every copy, then at about frame 2600 walked east on the host and north on
the three joiners; same random state. Cause unknown; not tied to the shop (250 frames after it closed). Possibly
the same as the intermittent simul failure. Worth knowing: actor byte +0x117 differs between copies all the time
(per-copy state, maybe visibility), so it is not game state to compare.

### Selection dots moved above the name tabs (2026-10-22)

The tester pointed out the squad bar shows portraits only for the selected characters, so the deselected portraits
going black in the test is the game's normal look, not an artifact (correcting the note above), and dots on
portraits vanished when they were most useful. The dots now sit above the name tabs, which always show.
- Bottom panel: vtable 0x80299c, rect 0,472,800,600 (a portrait's +0x10e). Name tabs (vtable 0x80805c, 80x30, panel
  coordinates 236..721) at +0xfe, +0x102, ... +0x112; portraits in the same order at +0x147 ... +0x15b (+0x143 is
  another portrait window, 68x72).
- Above the tabs is the game view (vtable 0x804570, rect 0,0,800,472, slot 4 draw 0x4a0100, slot 0 destructor
  0x4a34b0, both ret 4). The view repaints only its changed parts: dots drawn everywhere showed only where the view
  happened to repaint (one tab). A change of dots now invalidates the whole view (base slot 3) once.
Tested: host_move 25/25, screenshots on both copies: red above all six tabs, blue above the joiner's two.

### Drift check: automatic re-sync (2026-10-22)

Every copy hashes every actor's position every 25 frames at the client tick (the same point of the frame on every
copy: per-frame hashes matched on four copies in eight 4-player shop runs). Joiners send their last 8 hashes with
the selection poll (" H<gen>:<frame>:<hash>,"); the host compares. Two checked frames in a row that differ make the
host re-sync (quick-save and quick-load, as for a drop-in), once no conversation or shop is open, at most once a
minute, with a notice to everyone. Actor byte +0x117 is left out of the hash: it differs between copies all the time.
Tested: drift_host 25/25 (the joiner moves an NPC 3 units on its copy only at frame 139; the host sees it at frames
150 and 175, re-syncs 5 s after the nudge, everyone plays again 8 s later, session 2 identical); host_move and a
4-player trade_host show no false alarm.

### Recruiter visits without a reload (issue #1, 2026-10-22)

Found (exploration hooks in recruit_sync.inc, Ghidra):
- SetMode makes the recruit screen at `0x4722eb`: `new 0x6c4dd0(0x153)`, ctor `0x4d6060 __thiscall(this, WGameMain*,
  [WGameMain+0x102])` (the second argument is the game view, WGameWorld), stored at WGameMain+0x122. Deleting
  destructor: vtable 0x808bc0 slot 0, `0x4d9280` (runs `0x4d6830`, ret 4).
- The constructor walks the campaign's recruit pool (`*0x8bdd2c +0x181`) and makes 88 entities in the bunker: the
  candidates with their AI, inventories, weapons and armour. It destroys the ineligible ones (`vt+0xdc` false ->
  `vt+0x28`) at once (40 of the 88). It also clears this player's selection. `0x505b00` gives it the world's random
  state (World+0x729), or a throwaway copy of World+0x2eb when 0x8bdd8b is set.
- The constructor attaches the window to its parent: made on another copy, the recruit screen covered that copy's
  game (screenshot). Unlinked from the parent's child list (+0x14 parent, +0x18/+0x1c first and last child,
  +0x20/+0x24 previous and next sibling) it is neither drawn nor ticked.
- On the recruiter's copy all candidates not recruited are marked for destruction by the time the screen is gone (the
  Done path); a screen that was never shown destroys only the 40 the constructor did.

The sync: a recruiter visit holds every copy (it changes the squad everyone plays), so all copies sit on the same frame
while the screen is up. After the recruiter's constructor ran, its copy takes its snapshot again (variables kept) and
sends co-op record 7 (kRecruitOpen); every other copy, on arrival, makes the same screen object unlinked and restores
its player's selection: the same 88 entities in the same slots (logged on both). At close the recruiter's diff (made
after its screen is gone) carries the squad change, positions and 'D' for every candidate; the others destroy their
hidden screen first, then apply it ('D' finishes off the 48 the hidden screen left). A new recruit already exists on
every copy, so nothing is made from bytes any more.
Tests (no reload in any): recruit_host 28/28, recruitnew_host 28/28, recruit_join 28/28, recruitnew_join 28/28,
recruit_host with 4 players 56/56, recruitnew_host with 4 players 56/56.

### Quest givers, level-ups, a scrip deal (QuestBarter.sav, 2026-10-22)

The tester made QuestBarter.sav: squad member 0 (1055) next to General Barnaky (NPC 266, B01_GenBarnaky.ent), member
1 (997) next to Quartermaster Octavius (NPC 96, B01_QMOctavius.ent). A test "survey" lists the NPCs next to the
first two squad members of any save.

Found (observed in test logs; decompiled where noted):
- **A quest giver opens the Pipboy** (WPipboy, mission-screen mode 11), not a conversation window, on every copy (the
  talk runs everywhere). A Pipboy opened within the talk-claim window counts as that conversation: kept on the
  talker's copy, closed elsewhere with a notice (the tester: only the talker sees it).
- **Squad bar, one character selected:** the bar shows one large portrait and hides the six small ones, whose
  character field (+0x153) is then 0. The dots now take tab k = squad member k (the bar fills its tabs in squad
  order; a WSquadMember tab holds only the name +0x10e and HP it shows) and find the bottom panel (WGameBar, vtable
  0x80299c) among the mission screen's children. The view repaints its dots when it is new and every 2 s.
- **Level-ups.** The Level effect button (WEffectButton, handler 0x479e60: by effect name, "objectiveComplete" opens
  the Pipboy, else SetMode(3)) opens the character screen WCharacter (vtable 0x7ffa84, mode 3; its character's
  EntityID at +0x123). With a perk to choose, its setup (0x45ab80) makes WPickPerk (ctor 0x4b5d60, vtable 0x8063a8)
  and runs its modal loop, vtable slot 11 (+0x2c, the base window's 0x6c6190): window ticks, the message-pump hook,
  and the dialog's own update and draw slots are not reached from it. Perk Done 0x4b64b0 (highlighted perk to
  vt+0x188, EndModal(4)); Cancel 0x6c62c0 (EndModal(0x10)). The character screen's Done is 0x4678b0 (Cancel
  0x467e10); its changes reach the character only on Done (a direct skill poke was written over by it).
- **A character's numbers:** four attribute tables (Stats, Traits, Skills, OTraits, Perks, vtables 0x809434,
  0x824be4, 0x824bc4, 0x824bf4, 0x824c04) from Actor +0x2a7, 0x339 apart; the byte at +0xf8a is not theirs (it
  differed between copies after a level-up). Delta op 'A' carries the changed byte runs of squad members' tables;
  tests compare a hash of every squad member's tables (ATTR) on every copy.
- **Deferred destroys, in full:** World::DestroyEntity 0x4e4a30 puts the entity on a pending list (World+0x759,
  count +0x75d), sets +0x141 and the slot's seq + 1; 0x4e4bb0, once a frame, frees them all in order (0x4e5270: slot
  onto the free list, seq + 1 again).
- **A scrip deal (the whole stack spent):** the stacks the barter screen split off for the offer tables exist on
  the shopper's copy only; handing them back marks them; measured before they were freed, the deal gave the others
  their slots unfreed, and four slots stayed one seq apart. The shopper now frees them (0x4e4bb0) before measuring,
  when nothing else was pending.
- **Two more desyncs fixed on the way:** a conversation's delta carried the free-list head from a snapshot taken
  while another player's shop stock existed (now: no head or serial in a delta that changes no entries); a create
  that waited for its slot was destroyed again by the "destroy what is still there" safety net (now struck off it).

Rules added (the tester's):
- While a player shops, the others cannot start a conversation (it would hold the shopper; the shop's close then ran
  its destroys in another order). Notice: "a player is trading - talk to them when they are done".
- While a player levels up a character, the others cannot open that character's screen or use the recruiter
  (records 8/9, kLevelOpen/kLevelClose with a claim). Two opens of the same character at once: the first through the
  host's relay wins everywhere; the screen's Done waits until this copy knows it won (with the host levelling up, no
  record flows until it is done, so a joiner learned of losing only after its own Done).
- A level-up holds only the levelling player's game: its changes land elsewhere up to a second later.

Tests (2026-10-22): quest_host 25/25, quest_join 25/25, levelup_host 23/23, levelup_join 25/25, levelrace 23/23,
shoptalk 25/25; on the same code: trade_host 48/48, trade_join 49/49, trade2_host 31/31, browse_host 29/29,
recruitnew_host 29/29, recruit_join 29/29, talk_host 30/30 (these last seven before the level-up lock).

### What the game's multiplayer layer already syncs, and what it doesn't (2026-10-22)

Method: the lockstep command handler (Actor vtable slot 510, `0x5747e0`) switches on the 27-byte command's first byte;
each case's calls were named with FTSE's Entity vtable names (`docs/vtable functions/Entity_Vtable.html`, 536 slots)
and the strings its helpers use. Window classes (259 `W*`), trigger actions (61) and conditions (20) come from the
exe's RTTI. "Observed" = seen in a test run; "derived" = from names and decompiles, not run.

**Commands the lockstep layer carries** (every copy runs them on the same frame; derived unless marked):
| type | what (FTSE names of what the case calls) |
|---|---|
| 0x00 | move (stance and AP checks; observed: the bot's moves) |
| 0x01 | burrow / gun-turret activation / driver (special units) |
| 0x02 | attack an entity (BeginAttackAnEntity; observed: attack test) |
| 0x03 | act on an entity in reach |
| 0x04 | use / activate an object or item on a target (CanBeActivated, CanUseItem) |
| 0x05 | drop an item (RemoveItemFromEquippedThenInventory) |
| 0x06 | equip, called shot, ammo |
| 0x07 | unequip ("unEquip") |
| 0x08, 0x09 | change stance / animation set |
| 0x0a | stop |
| 0x0b | set active weapon hand |
| 0x0c, 0x1d | vehicle controller command |
| 0x0d, 0x22 | evade (SetBonusACForHTHEvade) |
| 0x0e-0x10 | sentry mode, accuracy trigger, location |
| 0x11 | attack a location (grenades, bursts at ground) |
| 0x12 | reload |
| 0x13, 0x14 | take / give an item in reach (pick up, from containers) |
| 0x15 | combine inventory items |
| 0x16 | act on a target in reach (skills, probably) |
| 0x17 | talk (observed) |
| 0x18, 0x19 | steal ("steal") |
| 0x1a, 0x1b | traps: activate, set frequency |
| 0x1e | end combat ("combatEnd" sound) |
| 0x1f | vehicle related |
| 0x20 | overwatch |
| 0x21 | set a target / leader id |
| 0x23 | unknown |
| 0x25 | disembark, use an entity, sneak |

So movement, combat (real-time; turn-based has its commands too), equipment, picking up and dropping, stealing,
traps, skills on targets, vehicles, sneaking and overwatch go through lockstep already.

**Single-player screens: who syncs them**
| screen (window class) | state it changes | status |
|---|---|---|
| conversation (WSpeech) | variables, items, HP | mod: delta at close; everyone held (observed: talk_*) |
| quest giver briefing (WPipboy from a talk) | quest variables | mod: talker only, delta (observed: quest_*) |
| barter with a quartermaster (WBarter, WBarterTable) | inventories, stock list | mod: no pause, stock on every copy (observed: trade_*) |
| recruit screen (WSingleRecruit) | squad, new characters | mod: candidates on every copy (observed: recruit*) |
| character screen, perks (WCharacter, WPickPerk) | stats, skills, perks | mod: 'A' op, lock, race (observed: levelup*) |
| squad screens (WSquadWindow, WSquadSelectionWindow) | squad | counted private (full diff); no test |
| **multi-recruit (WMultiRecruit)** | squad | not handled: falls to the private-screen diff at best; no test |
| **Gamble button in a conversation (0x4d0090)** | money, items | not handled specifically; no test |
| **barter with a non-quartermaster trader** | inventories | goes the old held path (not light); no test |
| inventory screen (WInventory, WSquadEquip, WMemberEquip) | items moved between squad members, equip | lockstep commands (5 drop, 6 equip, 7 unequip, 0x0b hand, 0x13 give); held while a shop is open (observed: chaos with equips, trade_* --inv-chaos, 2026-10-28) |
| looting a container (WLoot) | container and squad inventories | lockstep 0x14/0x15; dead list entries pruned on every copy (observed: loot, 2026-10-27); corpses: no test; WMultiEquipLoot changes inventories directly (never seen in play) |
| **stack split (WMoveStackInput, WQuantityTypeDialog)** | stack counts | not checked |
| skills screen (WSkillDex) | none by itself; the use is a command | probably fine |
| Pipboy from its button (WPipboy) | none (reading) | fine |
| world map, travel (WCampaign*, WWorldWindow) | where the squad goes | mod: host only; the host's party shown on every map (observed: exit_*, worldmap, 2026-10-28) |
| random encounters on the map (WCampaignRandomWindow, WRandomDialog) | which mission loads | host's copy rolls, the others follow its save (observed: encounter, v0.8.0) |
| **save and load (WSaveLoad)** | everything | host load re-syncs (observed: resync); **a joiner loading its own save** not handled |
| mission briefing and debrief (WLoadMissionScreen, WEndScreen), cutscenes (WMovieWindow, ActionPlayBink) | none, but they pause | run on every copy from the same trigger; each player closes their own |

**Trigger script risks** (scripts run in lockstep on every copy, so most actions are fine; these read local state):
- `ConditionGUIOpen`: a trigger that tests whether a screen is open sees each copy's own screens.
- `ConditionRightClickOnTab`: local mouse input as a trigger condition (tutorial-style triggers).
- `ActionForceSpeech`: a script opens a conversation on every copy; the mod's rule "nobody's talk: the host's"
  keeps it on the host; not tested.
- `ActionStartCombatMode`: switches to turn-based; turn-based play is not covered by tests.

**Next, by risk** (updated 2026-10-28; inventory moves, looting containers, random encounters and turn-based are
done, see the 2026-10-23 to 10-28 section): (1) corpse looting, (2) a joiner's own load, (3) forced speech,
(4) gambling and other traders, (5) multi-recruit, (6) a story character joining the squad (still a reload).

## 2026-10-23 to 2026-10-28: desync root cause, turn-based, two PCs, v0.7.0 to v0.9.3

Mod repo: thennak3/fallout-tactics-coop (pre-releases v0.7.0 ... v0.9.3). Every "observed" below is a bot test run
with `test/run_test.py <name>` on one PC (2 to 4 copies), or the tester's two-PC session log where it says so.

### Observations
- **Burst desync, root cause.** FTSE's Lua `OnBurstAttack` rolled with `math.random`, seeded from `os.time`, so each
  copy's NPC bursts landed differently. A deterministic Park-Miller override in `core/FTSE/ftse.lua` fixed it (v0.7.0):
  chaos runs with every copy acting at once stopped drifting (chaos 3p 38/38).
- **Random encounters** (v0.8.0): WWorldWindow 0x6bad60 rolls on a private LCG at ww+0x112; the queue at
  campaign+4/+8/+0xc holds FOTString names (a refcounted copy must be queued, raw entry pointers crashed 0x6bc980);
  dispatcher 0x6bcaa0(ww, 0). Test `encounter` 26/26.
- **Turn-based** (v0.9.0): World+0x427 mode (0 real time, 1 Individual, 2 Squad), groups at World+0x430, current
  index World+0x43c; 0x507bc0 = whose turn; End Turn 0x477a80 checks 0x6368a0 (client+0x130 vs player+0x31), hooked
  so a joiner owns slot 1. The host's settings copy (World+0x749, fields as bos.cfg) goes to every copy as a co-op
  record (kind 11) and is rewritten every frame. Tests `turnbased` 2-4 players (69/69 with 4), `tbauto` 29/29.
- **Two PCs** (the tester): discovery on UDP 47701 failed with WSAECONNRESET after an ICMP port unreachable; fixed by
  SIO_UDP_CONNRESET off (v0.9.1). The tester's session then played PC to PC.
- **Looting** (v0.9.2): taking from a container left dead list entries (node seq at +8, slot at +10, valid only if the
  table entry's seq at +6 matches) on the non-looter's copy: endless inventory drift. Pruned on every copy 2 frames
  after 0x14/0x15. Test `loot` 24/24.
- **Conversations** (v0.9.2): an NPC lock left by a lost talk blocked every later re-sync. Locks now carry the talker
  and are freed on conversation open / a non-talk command. Tests `talk2` 28/28, `talk2same` 27/27.
- **Crash leaving a session to the main menu** (the tester, 3 times; WER 0xc0000409 in netlog.dll, `__report_gsfailure`).
  Crash dump: the guard slot of PumpHandlerHook held a return address one slot up. 0x4ca7b0 (Single Player > Load)
  is `__thiscall(menu, int unused)`, `ret 4`; it was called as `__fastcall(menu)`. Test `menu_join --menu-cancel`:
  the old call exits 0xC0000409, fixed 36/36. A Sonnet agent checked ~85 other direct game calls: all match.
- **Drift check silent after one correction** (the tester's log): the correction frame was kept across loads, and loads
  restart the frame count, so nothing was compared for the rest of the night. Now per generation, with a
  once-a-minute "matches ours" line.
- **Shop + inventory moves split the copies** (the tester's bunker night; reproduced by `trade_host --inv-chaos` twice).
  A shop pauses nobody; the shopper's copy makes and destroys entities with each offer (seen: made 702, destroyed 702
  and 1095 at frame 2310 on the host only), so a give at frame 2313 took different free-list slots on each copy.
  Commands 04/05/06/07/13/14/15 now wait from the shop's open frame until the frame after it closed everywhere.
  `trade_host --inv-chaos` 48/48, `trade_join --inv-chaos` 49/49.
- **Equipment**: Actor+0x100e holds three handles {seq, slot} (hands 0 and 1, armour 2; getter vfunc +0x54c, 0x5581a0),
  active hand byte +0x1029. UI equips are lockstep commands (WSquadEquip 0x46f940 / 0x46f520 / 0x470290 -> wrappers
  0x631320 cmd 6, 0x631300 cmd 7, 0x631a80 cmd 0x0b). Now in the snapshots, the drift hash and the correction ('G').
  Test `drift_equip` 25/25 (corrected, no reload).
- **Entity table growth** (0x4e50c0): with the free list empty the table grows to max(1000, 2n) via 0x4e7ed0, new
  slots chained N..2N-1 -> 0. During a private screen this used to force a reload (the tester saw 2000 -> 4000 during a
  character screen); now the delta says so ('T') and every copy grows the same way. `talk_host --grow-table` 30/30,
  `talk_join --grow-table` 31/31.
- **Joining delays**: fixed 1-2 s polls (generation poll, session retry, waiting message) now 250 ms. The Multiplayer
  screen's DirectPlay Release (0x759976 in 0x759960) blocks about 3 s (dpwsockx+0x7eca WaitForSingleObject INFINITE on
  its own thread); moved to a thread. Drop-in join 7.9 s -> 3.5 s; a re-sync from the host's load to playing
  2.7 s -> 0.9 s.
- **World map**: the party moves in 0x6ba780 (real time, dt capped 0.1 s) towards ww+0x10a/+0x10e while ww+0x101 is
  set, position at campaign+0xa5/+0xa9; clicks go to 0x6ba410 (thiscall(ww, float dest[2]), ret 4). The host's
  party now rides on the joiners' generation poll; a joiner's travel and encounter roll are off. Test `worldmap` 31/31.
- **World map pings** (v0.9.4): WWorldWindow mouse-down is vtable slot 51 (0x82dbd0 -> 0x6ba2a0, thiscall(ww, int pt[2]
  window-local, int button, int), ret 0xc; button 1 travels, others do nothing); draw is slot 4 (0x82db14 -> 0x6bbb00);
  world point = pt + campaign+0x9d/+0xa1 (scroll). The renderer's rect (+0x24) draws a 1-px outline, not a fill.
  Background test windows take no posted mouse input, and edge-scroll while the cursor sits in a corner. Test
  `worldmap` 33/33 (pings at the same map point on both copies, seen in a screenshot); right = button 2 unverified.

### Derived
- The game's multiplayer never had NPC-vs-squad combat or private single-player screens; every desync found so far
  came from something running on one copy only (a Lua RNG, a private screen's entity churn, a lost lock), not from
  the lockstep core.
- Anything a private screen does to the entity table must reach the others as bookkeeping, and lockstep commands that
  allocate must not interleave with it.

### Guesses
- The tester's table grew 2000 -> 16000 in one evening; growth needs an empty free list, so free slots may be lost
  somewhere (perhaps our own bookkeeping writes).

### Unknown
- Why the joiner's copy uploaded its save at 1031 s on 2026-10-27 (no joiner log); most likely the table-growth check.
- Corpse looting, a joiner loading its own save, forced speech, gambling, other traders, multi-recruit.
- (Later, 2026-10-28) Fog of war was listed here as not shared; it is: campaign +0x1ad holds it (columns of 0x38-byte
  cells, first int 0 unseen to 2 seen), joiners load it with the host's save and their party follows the host's,
  so both uncover the same cells (`worldmap`: 45 -> 46 cells, same hash on both copies, 35/35).
- (Later, 2026-10-28) The table growth guess was right: a delta's bookkeeping, written again after its deferred
  destroys, cut the freed slots off the free list (94 per recruit visit, 3 per level-up, the same on every copy).
  Fixed by putting every unreached unused slot back on it in slot order (`recruit_host` lost 94 -> 0).
