# OP-2.2 co-op — Phase 1 research notes

Date: 2026-10-23. Agent: OpenCode (GLM 5.3 Flash), following coop-anything AGENTS.md.

## Game identity
- Requested game: ОП-2.2 ("Объединённый Пак 2.2") — a large Russian story modpack for
  S.T.A.L.K.E.R.: Shadow of Chernobyl (built on Народная Солянка).
- Owner's install (observed 2026-10-23):
  - `D:\OP-2.2` — OP-2.2 standalone install. Contents: only `bin\unins000.*` and
    `gamedata.db_*` archives at the root (no xrEngine.exe visible at root; bin has no
    game exe — install looks partial or non-standard).
  - `D:\X-Ray Multiplayer Extension` — xrMPE install with launcher, `game\addons\`:
    `coop.addon` (4,248,229,988 bytes, SHA-256
    0F47F52944D8FF6EF0E9B523A0127356DCA2F23A13B1E049A601C1B9E0274744, dated 2025-10-28),
    `xrmpe_callofpripyat.addon`, `xrmpe_new_year_event.addon`.
    Game binary: `game\bin\xrEngine.exe` (3,430,400 bytes, SHA-256
    CDF28D1EB7BE83F1FB0E3F8C240ECD6FFCEC1B3D3D089619CDEC2059C87B4421).
  - Launcher config: last addon enabled = xrmpe_callofpripyat.addon, useSteam=true.
    Launcher log dated 2026-08-08 shows network errors (news/version.txt download
    failures) — possibly offline/locked ISP at that time.
- Scan of the first 50 MB chunks of coop.addon found NO `zzz_soc_coop_fixes` marker:
  the stalker-soc-coop-fixes patch is NOT applied (so any friends joining must match
  this exact unpatched coop.addon, or everyone installs the same patch).
- Store/version of the base game: unknown (xrMPE downloads its own game copy).

## Game identity — UPDATE after owner installed Patch 6 (2026-10-23, verified)

Owner installed "Патч 6 от 15.03.2023" (content patch from RPG Russia; patchlog:
шорохи valve fix, dialogs, Фраер quest). Verified on disk:
- New archive: gamedata.dbz_patch (13.03.2023, ~100 MB) — content-only.
- bin\ DLL dates unchanged (xrGame.dll 08.02.2022) — BUT hashes CHANGED vs my
  first record: xrGame.dll now 9AE0AD19A48AEAB09E9F78B96DAFE72CBBE4543BDA2F2221C3F7A30DBFC5FE4D
  (was 20897DE4...), XR_3DA.exe now 9F7F9424... (was 16349F7B...).
  Conclusion: the original install already had an engine of the same date but
  different bytes (repack vs patch-6 engine), OR patch 6 shipped engine files
  silently. Either way ALL RVAs in the prototype must be re-verified against
  the new xrGame.dll before the next run (Ghidra project re-import + script
  re-run; the structural layout is expected identical since same date/size).

- `D:\OP-2.2` full contents inspected (50 files, ~37.5 GB total):
  - `bin\XR_3DA.exe` (3,686,400 bytes, SHA-256 16349F7B0B426A2E2B3A339F7719B027975BD15C28473BF36E3464192CA65364, file date 2022), `xrGame.dll` (SHA-256 20897DE45FBAAF4846F8F4B367061D7CD7809D59AA3E6C64D31C297A9ECB0D8F), standard SoC engine DLL set (xrCore, xrGame, xrLua, xrNetServer, xrRender_R1/R2, BugTrap).
  - 24 `gamedata.db_*` archives at root (~37 GB), OP-2.2's own fsgame.ltx.
  - `userdata\` has empty saves/logs — game likely not played yet on this install.
- Engine is single-player SoC X-Ray: xrGame.dll without MP extensions; there is no
  netcode for co-op here beyond the engine's vestigial deathmatch support.
- Earlier "install looks partial" note was wrong: the launcher `XR_3DA.exe` was in
  `bin\`, not at the root. Corrected.

## Launch test 2026-10-23 (observed, ran)

- Launched `D:\OP-2.2\bin\XR_3DA.exe` (working dir `bin`). Game window
  "S.T.A.L.K.E.R.: Shadow Of Chernobyl" appeared, process ran ~95 s (menu + level load
  started), closed gracefully via CloseMainWindow. Engine build: xrGame Dec 31 2021.
  GPU: RTX 4060, OpenAL Soft. 0 crashes in userdata\crashes. Log:
  `D:\OP-2.2\userdata\logs\xray_tilgal.log` shows a normal startup sequence through
  "Loading textures... Synchronizing..." (a level was loading; intro/menu OK).
- VERIFIED: OP-2.2 installs and boots single-player on this PC. Not yet verified:
  an actual in-game playthrough session (only menu/level-load observed).

## Phase 3 first look: OP-2.2 engine vs xrMPE (2026-10-23, ran)

Method: RTTI class-name extraction + network-string scan of the installed binaries
(tools/rtti_extract.ps1; dumps in local/dumps/). No decompiler needed yet.

OP-2.2 `xrGame.dll` (14.5 MB, 1091 RTTI names):
- Has the full GSC SP netcode skeleton: `game_sv_Single`, `game_cl_Single`,
  `game_sv_GameState`, `IPureServer`/`IPureClient` (Send/SendBroadcast/SendTo/
  OnMessage/Connect all present as symbols), `xrServer`, `CServerEntityWrapper`,
  `CClientSpawnManager`.
- MP leftovers only at vestigial level: string `buy_menu`, `chat_team`,
  `GAME_EVENT_BUY_MENU_CLOSED`, classes `CUIGameCustom`/`CUIGameLog`/`CUIGameSP`,
  `CSpectator`/`CSE_Spectator`, `CMPPlayersBag`. No Deathmatch/ArtefactHunt game
  classes in this build (they may live only in xrGameSpy/multiplayer builds; OP-2.2
  ships no xrGameSpy.dll).
- Conclusion so far: the SP game state layer that coop mods hook into IS present in
  OP-2.2's engine; the missing parts are the MP game modes and any relay/lobby layer.

xrMPE `xrGame.dll` (7.9 MB, 1405 RTTI names) — the comparison target:
- Full MP stack: `game_sv_mp`, `game_cl_mp`, `game_sv_Cooperative`,
  `game_cl_Cooperative`, `game_sv_OpenWorld`, `game_sv_Defence`, BattleRoyale,
  ArtefactHunt, CaptureTheArtefact, Deathmatch, TeamDeathmatch, `game_sv_xrMPE`.
- Extra DLLs OP-2.2 does not have: xrAPI, xrPhysics, GameNetworkingSockets,
  libprotobuf, libcurl, steam_api, discord_game_sdk, vivoxsdk, xrVoice, xrGameSpy,
  LuaJIT, luabind (separate), crypto, dedicated\ server binary.
- Conclusion so far: xrMPE is a heavily reworked engine branch (different size, new
  network stack, its own game framework). A direct "drop xrMPE classes into OP-2.2's
  xrGame.dll" is not realistic; the transferable asset is the DESIGN (what
  game_sv_Cooperative overrides, what messages it adds), not the binaries.

OP-2.2 `xrNetServer.dll`: standard GSC transport (IPureServer/IPureClient/
MultipacketSender) with 67 exported-class methods — same family as vanilla SoC 1.0006.
The transport layer exists and is unchanged-looking; co-op could in principle ride
on it the way vanilla MP did, if a game_sv_Cooperative-like layer were built in
xrGame.

Next step: unpack OP-2.2 gamedata (scripts) and check how game_sv_Single is
scripted against, plus find where the engine picks the game type (level_game_type /
config), to see what a minimal "second player exists" patch would touch.

## Phase 3: gamedata script analysis (2026-10-23, ran)

Tooling installed with owner's permission: Python 3.12.8 (user install,
%LOCALAPPDATA%\Programs\Python\Python312), Git 2.47.1 (C:\Program Files\Git\cmd).
Cloned cranebeaker/stalker-soc-coop-fixes (tools/xdb_soc.py reads SoC .db archives:
LZHUF index + LZO1X payloads). Extraction script: local/tools/extract_op22.py
(read-only against D:\OP-2.2; output to local/dumps/op22_gamedata).

- Read all 26 gamedata.db* archives: 80,928 entries indexed.
- Extracted 574 Lua .script files + config\game_levels.ltx (the mod keeps its
  scripts inside db_bdata_0). Dumps in local/dumps/op22_gamedata/scripts/.
- Script style: classic SoC modding — 544 uses of db.storage, 264 alife():create,
  544 xr_logic references. Only 7 references to net/game_sv_* APIs, all in
  lua_help.script (API docs) and _g.script (alias helpers). OP-2.2 scripts
  themselves never touch the server game-state layer directly — they go through
  the same engine bindings every SoC mod uses.
- Implication (derived, not yet proven in-game): script-side OP-2.2 code is not
  the first obstacle. The first obstacle is the engine: nothing in the shipped
  xrGame.dll can create a second player client in game_sv_Single. The work is
  native code: either (a) add a game_sv_Cooperative-like class into xrGame (the
  xrMPE design shows which overrides matter), or (b) an external sync agent that
  drives the SP engine's public console/bindings on two instances.
- game type selection config not yet located (system.ltx not extracted from the
  db_cdata/db_adata set yet — only game_levels.ltx matched my filter). Next:
  extract config\* from all archives and find the game type bootstrap.

## Phase 3: game-type bootstrap found (2026-10-23, ran)

Extracted 382 config .ltx files from db_bdata_0 (local/dumps/op22_gamedata/config/).

- `config\script.ltx` is the bootstrap: it names the game-type class-ID factory
  `game_registrator.get_game_clsid` and lists game types: [single], [deathmatch],
  [teamdeathmatch], [artefacthunt], [lastingame].
- `scripts\game_registrator.script` maps option strings to class IDs:
  "single" -> "SV_SINGL"/"CL_SINGL", plus SV_DM/SV_TDM/SV_AHUNT/SV_LAST. Its
  register() is empty — the commented-out game_sv_LastStanding lines prove the
  engine's object_factory can register NEW server game classes from script-side
  registration points (that is exactly how GSC's MP and xrMPE inject game types).
- So the game-type plumbing is INTACT in OP-2.2's engine, and a "SV_COOP" class ID
  could be added via the same factory — but the class itself (game_sv_Cooperative
  C++ code) still does not exist in xrGame.dll.
- Meaning for path choice: the engine-side entry point for a co-op game type is
  identified (object_factory + game_registrator), which makes Path A (native mod)
  concretely scoped: implement game_sv_Cooperative in C++, register it here, and
  the launcher/server line picks it with `-stype`/server config as usual.

Remaining check for this phase: confirm how a server is started from command line
in SoC (dedicated/xr_3da.exe -server or -i -stype single) and whether XR_3DA.exe
keeps the -server/-client switches in this build. That decides the prototype shape.

## Phase 3: command-line / console survey (2026-10-23, ran)

String scan of XR_3DA.exe and xrGame.dll:

- XR_3DA.exe has NO -server / -stype / -port switches (unlike SoC 1.0006 MP-era
  builds or xrMPE). Only "-ltx", "-fsltx", "-load", "-start"(? lua), "-level" style
  switches remain, plus a hard error "Can't start game without client" and
  g_dedicated_server symbol (vestigial). The OP-2.2 engine is compiled as
  client-only: the engine literally refuses to run without a local client.
- xrGame.dll still registers the classic console surface: g_god, g_unlimitedammo,
  demo_record/demo_play, and class IDs CL_SINGL/SV_SINGL appear as strings.
  No sv_* console commands (sv_adminname etc.) remain, no "start_server" console
  command. net_Export/net_Import/net_spawn/net_save_relevant exist — but those are
  the per-object serialization methods, not a user-facing server.
- CONCLUSION (derived): the shipped OP-2.2 engine cannot host a network game at all
  as-is. The GSC server framework symbols survive inside xrGame.dll/xrNetServer.dll
  (IPureServer etc.), but the code path that lets a human host/join was stripped.
  A co-op prototype therefore needs, at minimum:
    1. re-enabling the server path in xrGame (game_sv_* factory call when acting as
       host), and
    2. a game_sv_Cooperative class that keeps the SP logic running while accepting
       a second client.
  Both are native-code work inside xrGame.dll — Path A is now confirmed as the only
  viable route; the "external sync agent" path (B) has no console/CLI surface to
  drive two clients into one session.

Phase 3 verdict: reverse engineering of xrGame.dll (14.5 MB) is the required next
step: find where game type CLSID is chosen at session start, where the host path
was cut, and what stubs remain (g_dedicated_server, IPureServer::Connect callers).
Tools ready: Python + xdb readers work; Ghidra is the next tool needed (needs
owner approval to install; heavy download ~400 MB + JDK).

## Tooling installed (2026-10-23, owner approved)

- JDK 25.0.4.1 (Temurin, user install, SHA-256 verified against Adoptium
  checksum). Ghidra 12.2 PUBLIC (SHA-256 verified against release metadata),
  both in %LOCALAPPDATA%\Programs.
- Ghidra headless analysis of D:\OP-2.2\bin\xrGame.dll completed (438 s).
  Note: Ghidra 12.2 needs Java 25 exactly (min=25 in application.properties);
  JDK 17/21 rejected. Scripts: local/tools/ghidra_scripts/.

(Next: function lists + decompilation of the game-type selection path.)

## Phase 3: decompilation of the game-type bootstrap (2026-10-23, ran)

Ghidra headless workflow settled (Java scripts, full log to local/ghidra_run*.log):
- list_funcs.java: only 2 demangled `game_` functions — RTTI vftable names exist
  in the binary but Ghidra did not propagate class names onto methods, so work is
  by string-xref instead.
- find_str_xrefs.java (find strings matching "SINGL", get xrefs, decompile
  referencing functions): 63 string hits, 44 functions dumped to
  local/dumps/ghidra_clsid_refs.c (222 KB).
- KEY FINDING — FUN_18098ca00 is `game_GameState::getCLASS_ID` (its decompile
  self-identifies via xrDebug::fail with file "game_base.cpp" and that name):
  - It reads `$game_config$\script.ltx` → `[common] game_type_clsid_factory`
    (exactly the OP-2.2 config we extracted) and calls the Lua functor
    `game_registrator.get_game_clsid` through the script engine.
  - If the script returns an unknown/empty class id it calls
    xrDebug::fatal "Unknown game type: %s".
  - `g_dedicated_server` branch still alive: when dedicated, it compares the
    requested type name against "single" (code continues beyond dump).
- Implication: the server game-type plumbing is INTACT down to this function; a
  co-op game type would flow through the very same Lua factory. What is missing
  is (a) a registered "SV_COOP"-style class in the C++ object factory and (b) the
  host/server start path that would instantiate it (engine exe has no -server).

Next: dump xrefs to IPureServer::Connect / where game_GameState is constructed,
and check xrGameSpy/net_start remnants. Then write up phase 3 and PR the findings.

## Phase 3 BREAKTHROUGH: the server connect path SURVIVES (2026-10-23, ran)

Decompiled the two callers of game_GameState::getCLASS_ID
(local/dumps/ghidra_clsid_callers.c):

1. FUN_18099cfa0 is **xrServer::Connect** (self-identifies via
   "xrServer_Connect.cpp", "xrServer::Connect" debug strings). Full logic intact:
   - parses session name "name/type/..."; takes the segment after the FIRST '/'
     as the game type and after the second '/' as options;
   - calls game_GameState::getCLASS_ID(type, is_server=1);
   - asks the engine's class factory (Engine+0x18 table) to create the object;
   - RTTI-casts to game_sv_GameState, stores it at server+0x10940;
   - calls the game state's init (vtable+0x20) and then IPureServer::Connect.
   So hosting a network session end-to-end IS implemented in xrGame.dll. It is
   only UNREACHABLE from the UI/command line we scanned earlier.

2. FUN_1807dd6d0 is the client-side twin ("Game configuring : Started"): same
   flow with is_server=0, cast to game_cl_GameState, stored at +0x8718. This is
   game_ClusterWrapper/game client state setup. Also intact.

Consequence: the mod-side work is now sharply defined:
- The engine can create any game class that is registered in the class factory
  under a CLSID. The class creation goes through Engine's DLL_TABLE (Engine+0x18),
  i.e. classes registered by xrGame at load time (class_registrator.script).
- The original SoC co-op path would be: register a "SV_COOP"/"CL_COOP" class pair
  (new C++ code) + call xrServer::Connect with "level/single/..." style name.
- Since xrServer::Connect itself is present, an injected launcher DLL could call
  it directly — no need to re-enable the MP UI. This is the concrete hook point
  for the prototype: host calls xrServer::Connect("localhost/ coop params"),
  friend's client uses the game_cl_ equivalent path.

Unknowns/next steps:
- Find the Engine DLL_TABLE registration (where xrGame registers game classes:
  search RTTI_Type_Descriptor references of game_sv_Single / vftable setup) to
  learn the exact registration API a new class must use.
- Check whether xrServer::Connect has live callers (menu was cut, but maybe a
  console command or script binding remains).
- Verify xrNetServer.dll exports the transport the host needs (it does export the
  class methods; the server binds via xrNetServer).

## Phase 3: class factory mapped (2026-10-23, ran)

Decompiled the registration function FUN_1809808d0 (local/dumps/ghidra_1809809.c):

- It is the xrGame class factory registration table (REGISTER_CLASS loop). Every
  game object class is registered as (TEXT2CLSID string, script name, creator fn):
  first entries are `SV_SINGL` "game_sv_single" (creator FUN_1803871e0),
  `CL_SINGL` "game_cl_single" (FUN_180387310), `UI_SINGL` "game_ui_single",
  then actor, AI classes, weapons, zones, etc.
- The creators for game_sv_single/game_cl_single are CObjectItemSingle templates;
  their "server_object" branch is a stub that xrDebug::fatal's
  "Cannot instantiate server object, because server class is not declared!"
  (object_item_single_inline.h:36) — i.e. the SP build genuinely never creates
  remote server objects; the real C++ game classes exist and are registered.
- A second registration branch exists under `g_dedicated_server` (artefact_s,
  device_torch_s, wpn_*_s...) — the dedicated-server object set survives.
- FUN_18098cf30 exports game_GameState to Lua via luabind (Phase/round/start_time
  props) — the Lua side can already see the game state.

## Phase 3 CONCLUSION (the complete picture)

The co-op path in OP-2.2's xrGame.dll requires exactly ONE new registration +
ONE new class pair, and a way to call xrServer::Connect:

1. Register `SV_COOP`/`CL_COOP` CLSIDs in the factory table (the registration
   function address is known: FUN_1809808d0 @ 1809808d0; the pattern of the
   call sequence is fully visible in the decompile).
2. Implement game_sv_Cooperative/game_cl_Cooperative (initially trivial
   subclasses of game_sv_Single/game_cl_Single that just allow a second client
   and skip MP-only logic).
3. Trigger xrServer::Connect("level_name/coop/options") — an injected DLL or
   engine patch; the function itself is present at FUN_18099cfa0.
4. game_registrator.script then maps "coop" -> "SV_COOP"/"CL_COOP" (Lua side,
   trivially editable in gamedata).

This is now a concrete, bounded engineering plan. Estimated skeleton (two
players standing in the same level): the smallest slice is (2) minimal subclass
+ (3) connect trigger; (1) is a table patch; (4) is config.

Not yet verified in-game: everything above is static analysis. Next phase would
be building the prototype DLL and testing on a copy of the install.

## Findings (each with source)

1. No co-op mod for OP-2.2 exists. The authoritative AP-PRO list of all released/beta
   S.T.A.L.K.E.R. co-op mods (updated 2025-01) lists 8 projects; none is OP-2.2 or any
   Народная Солянка-based mod.
   Source: https://ap-pro.ru/forums/topic/10147-kooperativnye-modifikacii-na-stalker-stalker-co-op-modifications/

2. All working S.T.A.L.K.E.R. co-op mods run on the Call of Pripyat platform
   (xrMPE / X-Ray Open Multiplayer / Anomaly-based Stalker Together). None runs on the
   original SoC X-Ray 1.0006 engine that OP-2.2 requires.
   Source: same AP-PRO list; https://www.xrmpe.online/

3. The closest thing to "SoC story in co-op" is Shadow of COOP (ARCANUM TEAM): a
   reworked SoC story ported onto xrMPE (CoP engine), 1–4 players, Steam relay.
   AP-PRO (2025-04) says playable to Yantar; coop-anything's fix patch
   (stalker-soc-coop-fixes, 2026-10) claims all 18 levels boot and true ending works.
   Sources: https://ap-pro.ru/forums/topic/4649-shadow-of-coop/
   https://github.com/cranebeaker/stalker-soc-coop-fixes

4. No evidence of any attempt to port xrMPE or any netcode onto OP-2.2 / Народная
   Солянка was found (DuckDuckGo searches in Russian; xrMPE wiki, fandom, forums).
   Unknown: whether an attempt exists behind forum registration walls
   (stalker-worlds.games, ap-pro.ru threads require login).

## Derived assessment (agent's conclusion, not a verified fact)

Porting OP-2.2 to co-op = Path 2 (build from zero), magnitude far above the existing
xrMPE projects: OP-2.2 has thousands of custom scripts on a single-player engine with
no netcode, while xrMPE needed a modified engine binary on CoP. Realistically this is
months of reverse-engineering work, not a session-scale task.

## Practical path for the owner

Play the SoC story in co-op via Shadow of COOP 8.0 + fixes patch (QA: automated).
Install steps are in the fixes README (xrMPE launcher 0.9.x → coop.addon → patch .db).

## Unknowns
- Whether OP-2.2 authors or community have any co-op plans (needs forum/VK reading
  behind login).
- Whether xrMPE's engine mods could ever accept SoC gamedata at all (probably no).
