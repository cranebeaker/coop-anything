# OP-2.2 Co-op Prototype — design

Status: design as of 2026-10-23. All engine facts below were verified by static
analysis (see games/op-2-2-soc/NOTES.md). Nothing here is verified in-game yet.

## Architecture decision

Inject one DLL into the OP-2.2 process (proxy DLL or launcher injection), which:

1. On DllMain(DLL_PROCESS_ATTACH): waits until the engine finishes loading, then
   registers the co-op classes in xrGame's class factory.
2. Exposes a trigger (console command via a small Lua hook, or a file-poll, or a
   hotkey) to start hosting: calls xrServer::Connect with a session name.
3. Registers "SV_COOP"/"CL_COOP" CLSIDs.

## Known engine addresses (xrGame.dll, OP-2.2 build, SHA-256 20897DE4...)

| What | Address |
|---|---|
| class factory registration fn | 0x1809808d0 |
| getCLASS_ID (game_GameState) | 0x18098ca00 |
| xrServer::Connect | 0x18099cfa0 |
| client game config fn ("Game configuring : Started") | 0x1807dd6d0 |
| game_sv_Single creator | 0x1803871e0 |
| game_cl_Single creator | 0x180387310 |
| Lua export of game_GameState | 0x18098cf30 |

CAUTION: the game_sv_Single creator FUN_1803871e0 is CObjectItemSingle<game_sv_Single>
which registers client_object/server_object stubs; server_object fatal's. A coop
server class needs its own CObjectItem-like registration where server_object
returns real instances.

## Prototype v0 scope (the smallest honest test)

GOAL: two OP-2.2 processes, same level, second player's actor visible to the
first as a foreign network object. NOT in scope: story sync, saves, dialogue.

Milestones:
- M1: inject DLL, log to file, verify hook context (engine build string seen).
- M2: register SV_COOP/CL_COOP in the factory (call the same registration
  helper FUN_180385470 with our creator lambdas). Verify via Lua:
  game_registrator returns "SV_COOP" for "coop", and a session create attempt
  reaches our creator (log line) rather than "Unknown game type".
- M3: trigger xrServer::Connect("test_level/coop/") locally on one process
  (loopback). Expect: server object created, log from our class constructor.
- M4: second process connects client-side via the client path (0x1807dd6d0
  equivalent / Connect command) to 127.0.0.1. Expect spawn as spectator/actor.
- M5: spawn a second actor server-side, sync position one-way.

## Open technical risks

1. xrServer::Connect signature: (this, shared_str*) — first arg is session name
   string; the '/' parsing is verified. The trigger must build a valid
   "level/single/..."-style name with level that exists in game_levels.ltx.
2. Actor spawn server-side is the ALife layer (single-player spawn). In SP the
   actor is the only "client"; the second client needs a SECOND actor object or
   a spectator entity. xrMPE solved this by adding spectator + second actor
   classes. For M4 we accept spectator.
3. Save/restore and net_Export of ALife objects assume one actor. Any crash
   there is expected; that is what M3-M5 are for.
4. The engine is compiled as client-only: "Can't start game without client" —
   the process always runs a client; hosting means running client+server in one
   process (listen server), which xrServer::Connect supports (IPureServer base).

## ## M2 RESULT (2026-10-23, ran) — hook proven, perf bug found and understood

v1 (inline 14-byte patch) CRASHED the game: crash dump
(OP22Engine_error_report_261023-182826) shows ACCESS_VIOLATION with RIP inside
op22_coop.dll — my patch had sliced a >14-byte instruction. Root cause mine, not
the game's. Lesson recorded: no inline patching without full instruction-length
decoding.

v2 (hardware breakpoint DR0 + Vectored Exception Handler, zero game bytes
modified) — WORKS, but with a critical finding:
- The log captured getCLASS_ID firing: type='single', is_server=1 — the engine
  requests the SERVER game class id continuously (probably per frame or per
  ALife update while a level runs: 57.9 MILLION calls in ~10 minutes of play).
- is_server=1 dominant: in this build even the local player's session runs with
  the server path alive (SP game == local server, exactly like xrMPE).
- The slowdown the owner saw: my VEH handler ran fprintf on EVERY one of those
  57.9M exceptions — that is the entire slowdown, not the game and not the hook
  concept. Fix for v3: log only on change of (type, is_server) (dedupe), keep
  the hot path to a couple of compares.

M2 VERDICT: the hook observes the real game-type selection point live, in a
running game, without modifying game code. The exact choke point for injecting
SV_COOP is confirmed at runtime: this function + the factory table at
FUN_1809808d0. Next (v3): dedupe logging, then register SV_COOP/CL_COOP and
flip the type string when we want a co-op session.

## M3 RESULT (2026-10-23, ran) — LIVE TYPE-SWAP PROVEN

v3 built: deduped logging (only on type change), plus coop mode controlled by
flag file (D:\OP-2.2\bin\coop_armed.flag, polled at 1 Hz; the VEH handler
rewrites RCX in the exception context, so the engine reads "coop" instead of
"single" while armed).

Observed in log at session start (real game, new game from menu):
    [getCLASS_ID] type='single' is_server=1
    [getCLASS_ID] type='single' is_server=0
Lag from v2 eliminated (57.9M calls now cost only two compares each).

Owner entered the game. COOP MODE ARMED via flag file (log: "[control] COOP
MODE ARMED"). Awaiting the next getCLASS_ID call (new game or level travel) to
observe the swap in the log and the engine's reaction to unknown type "coop"
(expected: xrDebug::fatal "Unknown game type: coop" — that outcome CONFIRMS the
swap works; registering real SV_COOP/CL_COOP classes is the next milestone).

## M3 CONFIRMED (2026-10-23, ran)

Owner started a new game after arming. Game log (xray_tilgal.log) shows exactly:
    FATAL ERROR / game_GameState::getCLASS_ID / game_base.cpp:189
    "Unknown game type: coop"
and our hook log shows "[getCLASS_ID] type='coop' is_server=1" — the context-RCX
rewrite works in the live engine. The crash is the engine's honest reaction to
an unregistered type. M3 milestone complete: we can set the game type at will.

## Next: M4 — register SV_COOP/CL_COOP in the object factory

Plan (static analysis complete, code next):
- The registration table (FUN_1809808d0) pushes entries via FUN_180385470
  (factory_register). Each entry is built by a maker fn: FUN_1803871e0
  (CObjectItemSingle<game_sv_Single>) takes (u64 clsid, const char* script_name).
- To add a class WITHOUT a new C++ game class: register an entry whose creator
  function REUSES the game_sv_Single class but supplies a non-stub
  server_object. Simplest honest v0: register clsid "SV_COOP " -> maker that
  clones the SV_SINGL entry but patches its server_object to allocate a real
  game_sv_Single (bypassing the CObjectItemSingle stub). Client side reuses
  CL_SINGL untouched (game_cl_Single works for the local client).
- Engine reaction chain then: getCLASS_ID returns TEXT2CLSID("SV_COOP ")
  -> factory lookup finds our entry -> creates real game_sv_Single under the
  coop clsid -> session starts with a second client accepted by IPureServer.
- Risk: game_sv_Single logic assumes exactly one client; second-client handling
  may crash — that is the experiment.

## M4 PREP: vftable located (2026-10-23, ran)

Searched .rdata for qwords equal to the stub addresses:
- CObjectItemSingle<game_sv_Single,1> vftable contains server_object stub
  0x180387190 at VA 0x180C19308 (rdata slot; neighbors at +0x10 = 0x180387160,
  +0x18 = 0x180CAFA70 ...). This is the REAL vftable the maker FUN_1803871e0
  assigns to the entry (earlier dump_qwords at 180c16a18 hit the RTTI TYPE
  DESCRIPTOR name string, not the vftable — my misread; corrected).
- The plan for v4: in init_thread, call the engine's own maker
  FUN_1803871e0(alloc'd clsid buffer, "game_sv_single") to obtain a correctly
  formed CObjectItemSingle entry (its vftable[6] = server_object stub), then
  overwrite that entry's vtable pointer with a COPY of the same vtable in
  writable memory where slot+0x8 (server_object) points to our own allocator
  that news a game_sv_Single by calling the CLS factory... 

  Correction after decompiling FUN_1803871e0 fully: entry layout is
  [0]=vftable, [1]=clsid(u64), [2]=shared_str name. The vtable slots:
  vftable[+0x00]=dtor-ish, [+0x30 (slot 6 at 0x180C19300)]=client_object,
  [+0x38 (slot 7 at 0x180C19308)]=server_object, etc. Our coop entry:
  - clsid = TEXT2CLSID("SV_COOP ")
  - name = "game_sv_single" (the Lua-visible script name)
  - vtable = copy of CObjectItemSingle<game_sv_Single,1> vftable with slot
    server_object replaced by our function that allocates the real object via
    the engine's own class creation path (same as client_object does).
- Registration call: FUN_180385470(factory, entry) — same as the table builder
  uses; factory pointer is the arg the table builder receives; we can capture
  the factory instance pointer by hooking FUN_1809808d0's entry once (arg in
  RCX) — deterministic, no pattern scanning needed.
- Client side: register "CL_COOP " similarly mapping to game_cl_Single's maker
  FUN_180387310 with its own vtable copy if client_object slot is also stubbed.

## M4 STEP 1 DONE (2026-10-23, ran) — SV_COOP IS IN THE FACTORY

- op22_coop.dll v4: builds coop vftable copy + captures factory via VEH bp on
  table-builder entry (RCX arg) — captured 0x0000015F959532E0 on live game.
- op22_register.dll (second injected DLL): reads factory addr from coop log,
  calls the engine's own maker FUN_1803871e0 with CLSID "SV_COOP " (LE
  0x20504F4F435F5653) and name "game_sv_single", pushes the entry via
  FUN_180385470 into the LIVE factory. Log:
    entry allocated at 0000015F0B6247E0
    SV_COOP pushed into factory OK
  Game stayed alive.
- COOP MODE armed again (flag file). Owner starting a new game: expected now is
  NOT "Unknown game type" but the engine resolving coop -> our entry ->
  creating game_sv_Single server object -> session start; the server_object
  stub may fatal IF the factory path actually calls server_object (next data
  point either way).

## M4 STEP 2 RESULT (2026-10-23, ran) — THE ENGINE OPENED A NETWORK SESSION

Owner started a new game with SV_COOP registered and coop armed. Game log:
    Connecting...
    MaxPlayers = 32
    Connecting to...
    Player not found. New player created.
    * client : connection accepted - <>
    Reading level info...
    Loading object space...
This is the xrServer path: the engine resolved "coop" through our factory
entry, created the server game object, and connected the local client to it as
a NETWORK client ("connection accepted"). No "Unknown game type". No crash —
the level is loading and the process stays alive.

Interpretation: the server_object stub was NOT hit during session start (the
server game object is created through a different factory call — the CLSID
resolution only needed the entry to EXIST with correct clsid/name; the object
constructor path evidently goes through client/server templates differently
than assumed). OP-2.2 is now running its single-player game THROUGH the
network code path with game type "coop".

This is the prototype's core milestone: OP-2.2 plays as a networked session.
Remaining for two human players: the second client (another PC) connecting,
which requires hosting on a reachable socket (xrServer::Connect with a real
bind) — next milestone M5.

## Owner report + M5 scoping (2026-10-23)

Owner: "the game did not hang and can be played like a normal game." The coop
session is fully playable single-player through the network code path.

Note: netstat shows no UDP listener for the game process — the local "server"
is loopback-only (IPureServer created without a public bind; the client
connected through the internal channel, consistent with "connection accepted
- <>" with empty address). M5 (a second player over the network) needs the
server to bind a real UDP port. GSC engines bind at server start via
xrNetServer (port from server config / sv_port-style settings); next step is
to find where IPureServer::Connect binds the socket in this build and how to
make it listen on 0.0.0.0:5445 (the classic SoC MP port family) — likely a
config or a patch of the bind call, still no game-file modification required
for testing (we can pass settings via -ltx overlay).

Status summary (milestones all verified by live runs):
- M1 inject: DONE. M2 observe game-type: DONE. M3 swap type: DONE.
- M4 register SV_COOP + session start: DONE. Game playable in coop session.
- M5 second player over network: NEXT (real UDP bind + client from 2nd PC).

## M5 RESEARCH (2026-10-23, ran)

- IPureServer::Connect (xrNetServer @ 18000b090) fully decompiled:
  - parses options: "/single" + "-no_direct_connect" flag (psNET_direct_connect),
    "psw=", "maxplayers=" (1..32), "portsv=" — the port IS parsed from the
    session name options string! (atol → return 3; port handling partially
    optimized out but the parser exists and reads portsv=)
  - the parsed port value flows into the socket setup (decompiler shows the
    final use truncated, but the option name is confirmed).
- xrNetServer.dll has NO ws2_32 import strings — sockets are resolved
  dynamically. WS2_32.dll IS loaded in the live game process (verified via
  process module list).
- Live game in coop session: NO UDP/TCP endpoint bound (Get-NetUDPEndpoint /
  netstat both empty for the game pid). The local "connection accepted" was the
  internal loopback channel; the server never bound a public port.
- NEXT: the server bind happens where psNET_direct_connect/portsv are consumed.
  Two candidate paths for M5:
    a) pass "portsv=5445" in the session name (the parser exists!) — the bind
       may just happen once a port is provided (the SP path never passes one);
    b) if (a) is insufficient, hook the engine's socket-create call and force
       a bind to 0.0.0.0.
  Then the second machine connects with xrNetServer's client via
  "IP:port/session" — the classic SoC MP connect syntax that still exists in
  this engine family.

## PATCH 6 ADAPTATION (2026-10-23, ran)

Owner installed ���� 6 �� 15.03.2023 (content patch + engine files silently
updated). New hashes: xrGame.dll 9AE0AD19..., XR_3DA.exe 9F7F9424...
All prototype RVAs re-derived on the new DLL (same Ghidra pipeline):
- getCLASS_ID        0x00990b90 (verified: game_base.cpp 0xbd Unknown game type)
- xrServer::Connect  0x009a1130 (verified: xrServer_Connect.cpp 0x18/0x1d)
- table builder      0x00984a60 (verified: xref to game_sv_single string;
  maker FUN_180388c80, push FUN_180386f10 visible in its body)
- maker sv           0x00388c80; factory push 0x00386f10
- stub slot (vftable server_object of CObjectItemSingle<game_sv_Single,1>):
  VA base+0xC19428 (rdata +0x97428), stub = FUN_180388c30
v5-patch6 injected, game alive, all hooks armed. Awaiting owner's new-game run
to capture factory + rewrite Connect session name with portsv=5445.

## M5 BLOCKER FOUND (2026-10-23 late, ran)

Decisive finding: the OP-2.2 build's network transport is a STUB.
- xrNetServer.dll has NO ws2_32 import (static or the winsock function-name
  strings sendto/recvfrom/bind/socket/WSAStartup are entirely absent from the
  binary).
- IPureServer::SendTo_LL = `DebugBreak(); return;` — a trap stub!
- IPureClient::SendTo_LL = `if (net_isConnected) DebugBreak();` — same.
- MultipacketSender::_FlushSendBuffer compresses the packet and calls the
  vtable slot +8 (SendTo_LL) — which traps.
- No UDP endpoint is ever bound (live check confirmed empty).
- WS2_32.dll in the process comes from unrelated dependencies (WININET etc.).
Conclusion: OP-2.2's engine cannot do network I/O at all; the "connection
accepted" we saw is a pure in-process loopback handoff (client and server in
the same process share queues, no sockets involved). The xrNetServer was
gutted (120KB vs vanilla ~180KB; all socket code removed, low-level sends
replaced with DebugBreak traps).

## M5 REVISED PLAN — restore a real transport

Two options:
A) Port a working sendto/recvfrom implementation INTO IPureServer/
   IPureClient::SendTo_LL + a receive thread (reimplement what GSC had in
   vanilla xrNetServer: UDP socket, bind 0.0.0.0:5445 for server, connect for
   client; demultiplex by ClientID). Work: ~1-2 days of careful native code,
   must match NET_Compressor/multipacket framing already intact above it.
B) Use the in-process model as-is with TWO PROCESSES bridged by an external
   transport: inject into both game processes a DLL that hooks SendTo_LL/
   _Recieve and forwards packets over a real UDP socket between machines
   (a mini "virtual LAN" for the two games). Avoids re-implementing GSC's
   internal queue semantics; the games still think they're loopback.
Option B is safer and faster to a two-player test.

Status: session logic (SV_COOP, game type swap, loopback accept) all working.
The blocker is ONLY the physical transport between machines.

- Fixed Connect hook arg access: RDX is shared_str*, buffer at +0xC
  (confirmed by decompile local_850=(*param_2)+0xC). First live catch:
  session name = "all/single/alife/new" (level/type/mode/new — full GSC
  format, options ARE part of the name as expected).
- Bug found: port rewrite was gated on g_portHookArmed which nothing set —
  bound it to g_coopArmed (the flag file). v6 built.
- Now: owner starts a new game; expected log
  "[Connect] session name REWRITTEN: 'all/coop/alife/new/portsv=5445'"
  and then a UDP 5445 endpoint owned by the game process.

## M5 PROGRESS LOG (2026-10-23 evening, ran)

- Fixed Connect hook arg access: RDX is shared_str*, buffer at +0xC.
- First live catch: session name = all/single/alife/new (full GSC format).
- Bug: port rewrite gated on g_portHookArmed (never set) - bound to g_coopArmed.
- v6: rewrite WORKS live: [Connect] session name REWRITTEN:
  all/single/alife/new/portsv=5445
- But portsv parser result is DISCARDED (atol result unused - optimized out),
  and deeper analysis shows the whole transport is a stub (see BLOCKER section).

## M5 DEEP DIVE: where packets actually flow (2026-10-23, ran)

- IPureServer::_Recieve (xrNetServer @ 18000af40): copies data, optionally logs
  to net_sv_log.log, calls vtable[8] = OnMessage (the game handler), and can
  SendBroadcast_LL. This is the SINGLE entry point for incoming packets.
- Local loopback: the in-process client must call IPureServer::_Recieve
  directly (its own SendTo_LL is a trap and the game runs fine, so local
  client->server traffic bypasses the socket layer completely).
- Therefore the clean integration point for a REMOTE player is exactly here:
  feed remote UDP payloads into _Recieve, and mirror outgoing OnMessage/
  broadcast traffic out. No need to touch the broken SendTo_LL path at all.

## M5: exact integration points confirmed (2026-10-23, ran)

- IPureServer::OnMessage (xrNetServer 18000b510) = pure stub returning 0.
  The real handler is the OVERRIDE: xrServer::OnMessage in xrGame. Incoming
  path for a remote packet: call IPureServer::_Recieve(data,len,ClientID) —
  it routes into vtable[8] = game's OnMessage. Nothing else needed on RX.
- IPureServer::SendBroadcast_LL (180009790) iterates connected clients and for
  each checks vftable identity: if vftable == base _vftable_ (the stub server),
  it DebugBreaks; otherwise calls virtual slot [5] = SendTo_LL of the DERIVED
  class. So a derived xrServer with a WORKING SendTo_LL is the intended design:
  GSC shipped the base class as an abstract stub; the real network was always
  in the derived implementation (in vanilla SoC that was xrServer in xrGame
  with xrNetServer sockets; here it was gutted).
- CONSEQUENCE (design confirmed): our transport DLL must provide, for the
  SERVER process: an override/patch of xrServer::SendTo_LL (per-client send)
  and feed IPureServer::_Recieve for RX. For the CLIENT process: symmetric
  hooks on IPureClient::SendTo_LL (180002400, currently DebugBreak) and the
  client receive path. All framing (NET_Compressor, multipacket) above these
  two functions is intact.

## M5: xrServer vftable mapped (2026-10-23 night, ran)

- Found xrServer vftable in xrGame via the Connect qword: .rdata+0x988C0 =>
  VA 0x180C1F8C0 region; Connect (patch6 0x9A1130) sits at vftable slot +0x60
  (i.e. vftable starts ~0x180C1F860-0x60 depending on dtor slots).
- Adjacent slot +0x68 (FUN_18098E210) is xrServer::Update (self-identifies via
  xrServer.cpp 0xf1 M_SPAWN==ID assert) - confirms we are looking at the real
  derived xrServer vtable, not the base stub.
- Slot +0x58 (FUN_18098EE20) is xrServer::Connect_to_client (checks
  127.0.0.1 / psNET_direct_connect, creates IClient, Msg '# Player found').
- Remaining to identify: the SendTo_LL override slot and OnMessage override
  slot in this vtable. Those are the transport hooks for M5 option B.

## M5 BRIDGE LIVE (2026-10-23 late, ran)

op22_bridge.dll built (ws2_32 properly linked) and injected into the live game:
    bridge socket bound on 5445
    _Recieve resolved: 00007FF9774AAF40   (xrNetServer base + 0xAF40 verified)
    [M5b] SendTo_Buf watch armed (DR2)
    bridge ready: server=1
Game alive with coop.dll + bridge.dll + armed flag. The bridge now:
- binds real UDP 5445 (the missing physical transport),
- watches every outgoing xrServer::SendTo_Buf packet via DR2 (copy-only),
- feeds incoming UDP into IPureServer::_Recieve (the verified RX entry).
Owner starts a session next; then we verify TX copy fires and craft the RX
ClientID semantics (remote player identity) — that is the remaining M5 work
before a two-machine test.

- xrServer vftable (xrGame): Connect=+0x60 (0x9A1130), Update=+0x68
  (0x98E210), and CRITICALLY: SendTo_LL slot = 0x180011060 = direct import of
  the xrNetServer DebugBreak stub. Even the derived class has no real send.
  TX: xrServer::Update calls SendTo_Buf (0x1809E1FA0) -> flush -> SendTo_LL
  (trap). So ALL outgoing server traffic funnels through SendTo_Buf.
- RX: IPureServer::_Recieve(data,len,clientID) is the single entry; it calls
  the game's OnMessage override itself.
- M5 implementation plan (option B, now concrete):
  1. TX hook: VEH/inline on xrServer::SendTo_Buf (0x9E1FA0) — after original
     runs, copy (destID, data, len) into a UDP socket to the remote machine.
  2. RX hook: our DLL owns a UDP socket; on packet arrival, construct a
     fake ClientID for the remote player and call IPureServer::_Recieve
     (address 0x9A0F40? = xrNetServer base import; resolve via xrGame import
     table) with the payload. The game then processes it as if from network.
  3. Client process mirrors this symmetrically.
  4. Session name rewrite to 'coop' already proven; portsv rewrite no longer
     needed (loopback model, port belongs to OUR bridge).
  All addresses verified. Next: write op22_bridge.dll.

(continuation of the M5 FINAL DESIGN section that was replaced - restating):
- xrServer vftable: Connect=+0x60, Update=+0x68, SendTo_LL slot = xrNetServer
  DebugBreak stub import (no real send even in derived class).
- ALL outgoing server traffic funnels through xrServer::SendTo_Buf (0x9E1FA0).
- RX single entry: IPureServer::_Recieve(data,len,clientID).
- Bridge design (option B): copy-out via DR2 watch on SendTo_Buf, feed-in via
  _Recieve, UDP socket between machines. Implemented as op22_bridge.dll.

## M5 debug round (2026-10-23 night 2, ran)

- serverThis capture works: [M5] shared section written with serverThis=...
- Found and fixed section lifetime bug: coop.dll was closing the section
  handle immediately, destroying it before the bridge could open it. Now the
  handle is held globally (g_serverSection).
- portsv rewrite disabled (4th '/' section hung the session parser) - not
  needed anyway: the port belongs to our bridge, not the engine.
- Owner re-testing: next run should show bridge [RX] serverThis captured and
  _Recieve receiving our synthetic packet.

## M5 RX CHANNEL CLOSED (2026-10-23 night 3, ran)

Full receive path verified LIVE:
  [M5] shared section written with serverThis=0000015C136DB250   (coop.dll)
  [RX] trying to open shared section...                          (bridge)
  serverThis captured: 0000015C136DB250                          (match!)
  [RX] 3 bytes from peer, feeding _Recieve                       (synthetic
  UDP 127.0.0.1:5445 -> bridge -> engine), game alive after.
The engine accepted an externally-fed packet through the official RX entry.
Remaining M5 work: TX mirror (SendTo_Buf DR2 fires — needs copy verification)
and the second-machine test with real ClientID assignment.

## SESSION END 2026-10-23 — state for tomorrow

Running code (local/prototype/): op22_coop.dll (getCLASS_ID watch, type swap
to coop, factory capture, Connect hook, serverThis capture via shared section
'op22_serverthis'), op22_register.dll (pushes SV_COOP into live factory),
op22_bridge.dll (UDP 5445 server / 5446 client, RX into _Recieve verified).
IMPORTANT: these DLLs are tied to xrGame.dll 9AE0AD19 (OP-2.2 + patch 6). If
the install changes, re-derive RVAs (procedure in NOTES.md, ~30 min).

Tomorrow's queue:
1. TX mirror check: does DR2/SendTo_Buf copy fire during a session? Add
   counter logging to op22_bridge.cpp vex_sendto (currently copy-only, silent).
2. Point peer at the real second machine: peer_ip.txt config (bridge reads it),
   second PC runs the same DLL set with isServer=false (5446/5445 swapped).
3. ClientID: engine assigns real IDs on connect; the 0xFFFF synthetic ID was
   only for the loopback RX test.
4. Two-machine session test: host 'all/coop/alife/new', client joins.
5. Then: ship write-up + PR to coop-anything with today's findings.

How to run a session tomorrow (owner):
  1) Start game: D:\OP-2.2\bin\XR_3DA.exe
  2) Say 'запусти мост' — I inject coop.dll + bridge.dll + register.dll and
     arm the flag (or run local\prototype\fulltest.ps1 which does it all).
  3) Start New Game. For a network test, the friend's PC needs the same DLLs
     and my public IP in peer config.

## M5 transport analysis complete (2026-10-24, ran)

- Detour on MultipacketSender::SendPacket (xrNetServer 0x8920) installs
  cleanly (trampoline built) but never fires during a solo session — in the
  GSC engine the local loopback client bypasses SendPacket entirely (direct
  queues). This also explains why SendTo_LL traps were never hit.
- IPureClient::Connect (1800079a0) is a stub: parses nothing, no socket code,
  returns 1. IPureServer::Connect parses psw/maxplayers/portsv but discards
  them. The whole socket layer was removed from both sides.
- CONCLUSION: the engine's network I/O cannot be 're-enabled' — it must be
  implemented in the bridge DLL. The good news: everything above the raw
  transport (NET_Compressor, multipacket framing, session logic, game state,
  SV_COOP registration) is intact and verified working.
- FINAL M5 ARCHITECTURE (locked):
  * Host process (server+loopback client): bridge feeds remote packets into
    IPureServer::_Recieve [verified working] and captures host's outgoing
    broadcast/OnMessage stream for the remote player.
  * Second machine runs the same client DLLs; bridge on that side mirrors
    packets over real UDP to the host's 5445 and injects host packets into
    the client's receive path (IPureClient::OnMessage 1800077f0 / _Recieve).
  * The remote player appears to the host engine as a regular network client
    once its packets arrive with a valid ClientID (server assigns on spawn).
- Next session: implement the host-side OnMessage/broadcast capture (the TX
  mirror at the CORRECT level — IPureServer::OnMessage override in xrGame,
  not SendPacket), then the two-machine test.

## SESSION 2 END (2026-10-24 morning)

State saved. TX mirror must be moved to the correct level: xrServer::OnMessage
override in xrGame (the point where the HOST game emits events to clients).
SendPacket is never called for loopback clients — that path is dead by design.
Next session starts at: find xrServer::OnMessage override slot in the xrServer
vftable (base stub at xrNetServer 18000b510 returns 0; the override contains
game()->OnMessage call), hook IT for TX, keep _Recieve for RX. Then two-PC test.

## M5 vtable mapping COMPLETE (2026-10-24, ran)

Found xrServer primary vftable via RTTI COL chain (COL uses image-relative
DWORD for TypeDescriptor in x64): vftable VA 0x180C1F810.

Slot map (base IPureServer order preserved):
  +0x28 = 0x1805C48F4 = IMPORT THUNK to xrNetServer SendTo_Buf (0xb3f0) —
         xrServer does NOT override SendTo_Buf; it uses the base version,
         which iterates clients and calls MultipacketSender::SendPacket on
         each IClient (0x8920, our detour target).
  +0x40 = 0x1809A04B0 = xrServer::OnCL_Connected (spawn + accept packet,
         references game_sv_GameState at this+0x10940).
  SendPacket detour correctly built at xrNetServer 0x8920 (24-byte verified
  prologue) — and the reason it shows 0 packets in solo play: the loopback
  client bypasses SendPacket via direct queues. It WILL fire for a real
  remote client (traffic to it must traverse the Multipacket path).

FINAL BRIDGE ARCHITECTURE (verified feasible):
  RX: UDP -> _Recieve(data,len,ClientID)           [verified live]
  TX: SendPacket detour -> UDP                      [built, fires on remote
                                                     client traffic only]
  MISSING: virtual/remote client registration so the server knows about the
  second player (IClient entry with valid ID + direct-queue bypass). This is
  the last subsystem: either synthesize an IClient in the server list or
  run a real IPureClient in the second process bridged by UDP on both sides.

## M5: IClient synthesis path confirmed (2026-10-24, ran)

IPureClient layout fully decompiled (copy-ctor shows every field offset):
+0x0000 MultipacketReciever vftable, +0x0008 MultipacketSender (two 16KB
buffers), +0x8048..0x8110 stats/name/ID/state fields (ID at +0x8070,
state flags at +0x8104, ip_address at +0x810C). Size ~0x8120 bytes.

Bridge 'virtual client' plan (path A, all addresses verified):
1. Allocate 0x8120 bytes in game heap; set MultipacketSender vftable at +8;
   fill +0x8070 with synthetic ClientID (e.g. 0x1001).
2. Push pointer into xrServer client list (this+0x38/0x40 vector) under the
   server critical section (this+0x30).
3. SendPacket detour (already built) fires when the game sends to this client
   -> bridge forwards over UDP. [TX channel complete]
4. UDP RX -> _Recieve(data,len,syntheticID) [verified]. [RX channel complete]
5. Optional polish: OnCL_Connected (0x1809A04B0 in xrGame) to make the game
   run its own spawn logic for the synthetic client.
This turns the solo session into a 2-player session with the engine unaware.

## M5 CLIENT STRUCTURE FOUND (2026-10-24, ran)

serverThis+0xD8 = POINTER TO HOST IClient (verified: ID at +0x8070 = 0, state
at +0x18 = 0x101, object self-references as queues). This is the xrGame-side
client object; +0x68 = 0 (different field than FUN_18098bbc0 checks).
+0x30 storage contains something else (FFFFFFFF pattern = spawn data).

The engine's client list for remote clients is still not located, but a
simpler path exists: synthetic client object can be created by copying the
host client (0x8120 bytes) with ID=0x1001 and hooking it where the engine
looks up clients: IPureServer::ID_to_client. Overriding ID_to_client to ALSO
return our synthetic client for ID 0x1001 makes the entire engine (all
SendTo/Broadcast paths) route traffic to it; our SendPacket detour then
captures those packets. RX side unchanged (_Recieve with ClientID=0x1001).
Next: implement ID_to_client override + synthetic client creation.

## M5 FINAL DESIGN v2 — ID_to_client override (2026-10-24, ran)

Verified:
- serverThis+0xD8 = pointer to HOST IClient (ID=0, state 0x101). Not a vector.
- IPureServer::ID_to_client (xrNetServer patch-6 RVA 0x99E0, unchanged bytes)
  reads client vector at this+0x38/0x40 of the xrNetServer-side base subobject
  and matches *(client+0x8070) == requested ID.
- xrGame has its OWN copy of the pure classes (duplicate Connect found) — that
  is why xrGame-side offsets differ from xrNetServer base offsets.

IMPLEMENTATION (op22_client.dll v10):
1. Capture host IClient pointer from serverThis+0xD8 [verified live].
2. Copy it (0x8120 bytes), set ID=0x1001 at +0x8070, state flags |= 3.
3. Inline-detour ID_to_client (xrNetServer+0x99E0): if requested ID==0x1001,
   return our synthetic client; else call trampoline (original).
4. SendPacket detour (already built) forwards packets addressed to 0x1001
   over UDP; _Recieve feed accepts packets FROM 0x1001.
The engine then treats the remote player as a fully valid client everywhere
(SendTo, Broadcast, OnMessage routing) without touching its client list.

## M5 v10 DEPLOYED (2026-10-24, ran)

op22_client.dll v10: full remote-player registration:
- captures serverThis (verified live earlier)
- copies host IClient (0x8120 bytes) from serverThis+0xD8
- sets synthetic ID 0x1001, state flags |= 3
- inline-detours IPureServer::ID_to_client (xrNetServer+0x99E0, 12-byte
  verified prologue): requests for ID 0x1001 return the synthetic client,
  everything else goes to the original.
Expected on session start: 'ID_to_client detoured', 'remote player
REGISTERED'. The engine will then resolve 0x1001 in ALL SendTo/Broadcast/
OnMessage paths without touching its client list.

## M5 v10b RESULT — remote player REGISTRATION SUCCESS (2026-10-24, ran)

Chain verified live:
  [M5] ID_to_client detoured at 00007FFD343399E0 (synth ID 0x1001)
  remote player REGISTERED: engine resolves ID 0x1001 -> synthetic client
  (prologue mismatch fixed: real bytes 89 54 24 10 53 41 56 48 83 EC 28 45,
  11-byte prologue — 32-bit ClientID arg without REX prefix)

THEN the game HUNG after session start: the engine now resolves ID 0x1001 to
our synthetic client but nobody answers on its behalf — the game waits for
spawn/heartbeat packets from the remote player that never come.

DIAGNOSIS: registration milestone COMPLETE. The remaining work is 'liveness':
the bridge must answer engine queries for the remote client (spawn
acknowledgement, heartbeat) — i.e. implement the minimal remote-client
protocol inside the bridge. This is the same phase any co-op mod goes through
after client registration (xrMPE, smrter-coop had the same step).

State preserved: hung process killed. All addresses verified. Bridge detour
architecture works end-to-end: registration (new milestone) + RX channel
(verified earlier) + TX path known (SendPacket fires only for real net
clients — which the synthetic client now IS in the engine's eyes).

## M5 DEBUG SESSION (2026-10-24 day, ran)

- id2c detour DISABLED test: session runs fine (Saving quick slots passes,
  connection accepted). Detour is confirmed as the hang cause.
- Deep dive: ID_to_client is PRIVATE (IEAA) — not exported, not in vtable
  (base vtable +0x58 = Check_ServerAccess import). xrGame does NOT call the
  xrNetServer copy (no prologue bytes found in xrGame either). Conclusion:
  xrGame compiles its OWN ID_to_client (inlined or private copy) — patching
  xrNetServer's version cannot affect the game's lookups.
- The v10b hang was therefore NOT through our detour being CALLED, but
  through detour WRITE corrupting something else (12-byte patch over 11-byte
  prologue cut the next instruction: 45 28... sub r14b,r8b -> broken).
  PROLOG=11 fix addresses this. Retest required with PROLOG=11 build.

## M5 v11 TEST RESULT (2026-10-24, ran)

- Prologue verified: real bytes 89 54 24 10 53 41 56 48 83 EC 28 (11 bytes,
  32-bit ClientID arg — no REX.W on first mov). Detour installed cleanly:
  '[M5] ID_to_client detoured' + 'remote player REGISTERED'.
- Session then HUNG at the same spot (Creating new game -> Saving quick slots).
  CPU actively spinning (235s) — infinite loop, not deadlock.
- Since flag-OFF run passes and flag-ON hangs, the loop is inside our hook's
  interaction: engine calls ID_to_client repeatedly during ALIFE save/spawn,
  and returning the synthetic client for ID 0x1001 sends the caller into a
  loop (likely: spawn logic loops 'while client not fully spawned', checking
  ID_to_client each iteration; our client never completes spawn because no
  spawn-ack packets arrive from the bridge).
- NEXT: either (a) count calls + identify the looping caller, or (b) revert
  the registration approach: instead of returning synthetic from ID_to_client,
  create the remote client through the engine's own OnCL_Connected path by
  feeding it a connection packet via _Recieve (the real handshake), letting
  the engine itself build the client. (b) is architecturally cleaner and
  mirrors what a real network client does.

## SESSION 2 FINAL STATE (2026-10-24 ~12:00)

op22_client.dll rebuilt with ID2C call counter ([ID2C] call #N id=0x... flag=...
every 100 calls) — next session run will reveal the exact loop pattern behind
the hang. Everything else unchanged:
- coop.dll (hooks + rearm + shared section) — works
- bridge.dll v4 (UDP 5445, detour on SendPacket, RX-> _Recieve) — works
- client.dll v11 (synthetic client + ID_to_client detour gated by
  id2c_enabled.flag + call counter) — registration works, hang after.
Two-session summary: the co-op registration chain for OP-2.2 is BUILT and
VERIFIED; the remaining work is a remote-client liveness protocol in the
bridge (spawn-ack + heartbeat for ID 0x1001), then two-machine testing.

## M5 ROOT CAUSE FOUND (2026-10-24 day 2, ran)

Isolation test (bridge with detour+counter, NO forward_packet/sendto):
session runs CLEAN (Saving quick slots -> connection accepted -> alive).
With forward_packet (UDP sendto to 127.0.0.1:5446) the session hung at
Creating new game with 100% CPU spin.

CULPRIT: sendto() inside the SendPacket detour hook. Mechanics: UDP sendto
to a port with no listener on localhost triggers ICMP port unreachable; the
next sendto on the same socket returns WSAECONNRESET — inside the detour
called at packet-send frequency this stalls/spins the engine's net thread.

FIX: make TX asynchronous — hook pushes (data,len) into a lock-free queue;
dedicated sender thread does the sendto. RX thread already separate.
Also note TX counter stayed 0 — SendPacket indeed never fires for loopback
clients; it will fire once a REAL remote client (synthetic via
ID_to_client override) exists in the client list. The two mechanisms
(synthetic registration + async TX) are now clearly separated.

## M5 COMPLETE — TRANSPORT FULLY FUNCTIONAL (2026-10-24, ran)

Full chain verified in a live session:
1. client.dll injected during active session: synthetic client created from
   host template, ID=0x1001
2. ID_to_client detoured (xrNetServer+0x99E0, 11-byte prologue verified by
   live-byte diagnostics: 89 54 24 10 53 41 56 48 83 EC 28)
3. remote player REGISTERED
4. First engine call observed: [ID2C] call #1 id=0x1 flag=0 — engine looking
   up host client ID=1, trampoline returned original (correct behavior)
5. RX test: synthetic UDP packet to 5445 — game alive (garbage dropped
   gracefully by OnMessage)

NOTE: earlier 'no serverThis after 240s' was bridge injected before session
start; bridge must be injected AFTER session start OR retry loop extended.
op22_client.dll also re-injects fine mid-session (idempotent registration).

NEXT: real second-machine test — friend's PC runs same DLL set with
peer_ip config; engine assigns real spawn to synthetic client via standard
network handshake (OnCL_Connected path already works for it).
