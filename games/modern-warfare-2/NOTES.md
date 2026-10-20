# Modern Warfare 2 (2009) — session notes

Session date: 2026-10-19. Owner request: play the **full campaign, with its cutscenes**, in
co-op with friends. Spec Ops is explicitly not what the owner wants. The owner tried the
existing campaign co-op mod (COD-MOD, below) on 2026-07-04 and says it works badly.

## Game identity (AGENTS.md rule 5)

- Title: Call of Duty: Modern Warfare 2 (2009), Steam appid 10180 (SP: campaign + Spec Ops)
- Steam buildid **25374375** (Russian language set; depots 10181, 10182, 10189)
- `iw4sp.exe`: **x64** (PE machine 0x8664), PE link date 2026-09-16, 4,480,056 B,
  SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`
- Ships `steam_api64.dll`, `bink2w64.dll`, `SteamAPIUpdater.dll`, `UpdateDLLWrapper.dll`,
  `CoDCleaner.exe`; content in `main\iw_00..iw_09.iwd`, `zone\<language>\*.ff` (96 zones)
- Engine: IW 4.0 (closed binary). OS: Windows 11
- Exact local paths and the owner's logs: `local/` (private)

## Observations (checked this session)

- Repo had no MW2 (2009) coverage: `directory.yaml` and `games/` only mention MW3 (2011);
  `engine-families.md` has no IW-engine row (grep `IW ?4|IW5|Call of Duty|id Tech 3` → 0).
  `gh` is not installed on this PC, so open issues/PRs were not checked.
- **The 2026 64-bit update hit MW2 too.** `iw4sp.exe` is x64, linked 2026-09-16. The
  ultrawide fix README confirms "the September 2026 64-bit MW2 update"
  (https://github.com/ISASTO/mw2-ultrawide-fix).
- **The new zones are 64-bit.** All `.ff` keep the `IWffu100` magic and version `0x114`, but
  after inflating the first block the XAssetList holds 8-byte pointers:
  `common.ff` → `…0000020c 00000000 ffffffff ffffffff 00002448 00000000 ffffffff ffffffff`
  (string count, 64-bit ptr, asset count, 64-bit ptr). The old mod's `x_coop.ff` has 4-byte
  pointers (`00000139 ffffffff 00000025 ffffffff`). Same change as MW3, see
  `games/modern-warfare-3/NOTES.md` "STOP: the owner's install is the 64-bit build".
- **COD-MOD** is the only campaign co-op mod found. Pieces in the owner's install:
  - `iw4sp.hk`: x86 DLL, version string `COD-MOD: 3.223`, FileDescription "Call of Duty -
    Singleplayer Modification", built 2020-09-16; credits NTAuthority, kiwidog, archony.
  - `iw4sp_codmod.exe`: x86 game exe, PE link date 2009-11-07 (log prints
    `Build 159 … Fri Nov 06 2009`, i.e. game version 1.0.159).
  - `zone/custom/x_coop.ff`, `a_coop_models.ff`, `z_customanim.ff` (32-bit zones, 2020) and
    `spdata/x_core.iwd` (menus incl. `ui/coop_lobby.menu`, `maps/{trainer,boneyard,ending}.d3dbsp.ents`,
    ~20 `weapons/sp/*` files), all dated Sep–Oct 2020.
- **DLL source:** https://github.com/FreeTheTech101/cod-mod (momo5502, 223 commits =
  "3.223", last commit 2014-12-13 `db305c19`). No LICENSE file. `code/PatchMW2_Coop.cpp`
  only re-enables the engine's built-in **Spec Ops co-op netcode** over direct IP for the
  32-bit 1.0.159 exe: skips IWNet connect, forces port 28960, ignores protocol/XUID
  mismatch, adds `connect` → `connect_coop`, plus a master-server party search. It contains
  **no campaign logic**. Campaign co-op content lives in `x_coop.ff` + `x_core.iwd`, whose
  2020 author is unknown. The repo also commits the original `iw4sp_159.exe` and
  `iw4sp_184.exe` game binaries.
- Distribution: Steam guide "Tutorial - Modern Warfare 2 Coop Campaign Mod" (2021-10-11,
  mega.nz pack, Hamachi, host + 1 client, start via `iw4sp_codmod.exe`); a 2023 comment says
  "Mission big brother" freezes while loading
  (https://steamcommunity.com/app/10180/discussions/0/3087772148451672664).
- **Owner's own run (2026-07-04, logs in `local/`):** with one friend over the mod; maps
  trainer, roadkill, favela, invasion, favela_escape, arcadia. Seen in the logs:
  - crash `Fatal error (0xc0000005) at 0x00401772` while loading favela_escape (minidump);
  - lobby member timed out; repeated "host ended the game" disconnects;
  - mission end/fail goes through Spec Ops screens that are missing
    (`coop_eog_summary.menu`, `sp_eog_summary.menu`, `hud_show_timer`,
    soundaliases `so_defeat_ranger`, `so_defeat_woodland`);
  - favela_escape spawner warnings "dospawn called twice in frame";
  - many whole-level restarts (each prints a new `network coop random seed`).
  - Engine co-op dvars seen: `coop`, `coop_mapName`, `coop_maxFramesPerPacket 20`,
    `coop_minFramesClientCanBeBehind 2`, `coop_maxPacketUserCmds 32`, `coop_validateData`.
- **iw4x-sp** (AlterWare): singleplayer client that needs a 1.0.159 downgrade exe from the
  AlterWare CDN; no co-op mentioned (https://forum.alterware.dev/t/iw4x-singleplayer-install-guide/383).
  PCGamingWiki is reported (search snippet, page 403 to fetch) to say IW4x-SP does not work
  with the latest version.
- **IW4x** (https://github.com/iw4x/iw4x-client): open-source MP client, not campaign.

## Derived (reasoning, not tested)

- COD-MOD **cannot run on the owner's current install**: its 32-bit exe cannot load the
  64-bit zones, and its 32-bit `x_coop.ff` cannot load in the 64-bit exe. Any work on the
  existing mod needs a separate **32-bit test copy** (old depot manifests).
- Campaign co-op in MW2 does not need new netcode: the SP exe already carries a 2-player
  client/server (Spec Ops). The work is (a) making each campaign level's scripts handle a
  second player (`level.player` → players, cutscenes, vehicles, checkpoints, fail/restart),
  (b) a co-op checkpoint/restart flow instead of the Spec Ops end screens.
- The owner's complaints match (b) and per-mission script gaps: level restarts instead of
  checkpoints, missing end screens, spawner warnings, a load crash.
- Bink cutscenes between missions play locally on each PC; in-engine cutscenes are scripted
  around one player and need per-mission handling for the second.

## Guesses (unverified)

- Player cap: the Spec Ops engine path is probably hard-capped at 2 players.
- The 64-bit build may still have the same Spec Ops netcode, but there is no 64-bit tooling.

## Unknowns

- Author, license and source of `x_coop.ff` / `x_core.iwd`; how many missions they cover.
- Old (32-bit) manifest ids for depots 10181/10182/10189 and their sizes.
- Whether more than 2 players is possible in the SP engine.
- Whether official Spec Ops online still works on the 64-bit build.
- What the minidump's 0x00401772 is (needs the 1.0.159 exe in Ghidra).

## Owner decision (2026-10-19)

- **2 players**, **target the current 64-bit Steam build** (friends must not need to
  downgrade). No 32-bit depot download for now. Path 2 (build from zero) on the 64-bit build;
  COD-MOD is reference material only.

## Phase 3 recon of the 64-bit build (static, on a copy of the exe in `local/`)

Observations:
- `iw4sp.exe` PE32+, image base 0x140000000, 8 sections. Entry point is in a `.bind`
  section (raw 0x39248 B, entropy 7.96): a SteamStub DRM wrapper. `.text` entropy 6.55, so
  the code itself does not look encrypted. Debug dir PDB path ends in
  `iw4-pc\game\pc\iw4sp_steam.pdb` (internal project folder `outrun`).
- Imports from DLLs a proxy can stand in for: `d3d9.dll` (Direct3DCreate9), `WINMM.dll`
  (timeGetTime, timeBeginPeriod, timeEndPeriod), `X3DAudio1_7.dll`, `bink2w64.dll`.
  Networking: `WSOCK32`/`WS2_32` sockets, `steam_api64.dll` incl. SteamNetworking,
  SteamMatchmaking, SteamFriends. `SteamAPIUpdater.dll` and `UpdateDLLWrapper.dll` in the
  folder are 32-bit leftovers that the x64 exe cannot load.
- **The Spec Ops co-op netcode is all still there** (strings): `coop`, `coop_mapName`
  ("The name of the map to load in co-op mode", default `so_killspree_trainer`),
  `coop_autoLaunch`, `coopLaunch`, `coopConnect`/`coopAccept`/`coopReject COOP_FULL`,
  `coopLoaded`, `coopRestart`, `coop_maxFramesClientCanBeBehind`,
  `coop_fastForwardThreshold`, `coop_maxPacketUserCmds`, `connect %u.%u.%u.%u %u`,
  `connect_lobby`, `joinParty`, Steam lobby callbacks (`LobbyCreated_t`, `LobbyEnter_t`,
  `GameLobbyJoinRequested_t`), "Invalid co-op packet - references invalid clientNum %i".
- Console and mod hooks in the exe: `monkeytoy`, `cmdlist`, `devmap`, `spdevmap`,
  `sv_cheats`, `fs_game` ("Game data directory. Must be \"\" or a sub directory of
  'mods/'."), `fs_basegame`, `devraw`, `raw_shared`. Not present: `useFastFile`, `mod.ff`,
  the "Waited %d msec for asset" string.
- Owner's `players/config.cfg` binds `` ` `` and `~` to `toggleconsole`.
- The install now also holds MP zones (`mp_*.ff`, `common_mp.ff`, `patch_mp.ff`) next to
  the SP zones; 96 zones in `zone/russian`.
- **Scripts can be read from 64-bit zones**: rawfiles are inline `name\0` + zlib stream, so
  a small scanner (no asset parser) inflates the zone and pulls them out. Results:
  `common.ff` 455 rawfiles, `favela.ff` 89, `so_killspree_favela.ff` 2 (kept in `local/`,
  game data, never in a PR). The scanner is ~50 lines of Python; worth adding to `tools/`.
- A Spec Ops level is a campaign map with a different level script:
  `maps/so_killspree_favela.gsc` includes `maps\favela_code`, calls
  `maps\favela_precache::main()`, `favela_fx`, `favela_art`, `maps\_load::main()`, then
  runs its own objectives over `level.players`.
- Common scripts already know about a second player: `maps/_utility.gsc` `is_coop()` is
  `IsSplitScreen() || GetDvar("coop") == "1"` and asserts `level.player2` exists;
  `is_coop()`/`is_specialop()` calls sit in `_loadout`, `_load`, `_gameskill`, `_ac130`,
  `_specialops*`, `_snowmobile_drive`, `_spawner`, `_endmission`, `_autosave`, `_coop` and
  others. Counts: `common` 1624 `level.player` vs 270 `level.players`; `favela` level
  scripts 80 `level.player`, 0 `level.players`.
- Reported, not verified (search snippet; Nexus page 403): a Nexus gamepad mod says the
  64-bit build needs an x64 `winmm.dll` loader (https://www.nexusmods.com/codmw2/mods/83).

Derived:
- Campaign co-op on the 64-bit build = start a campaign map with `coop 1` through the
  existing co-op launch path, then patch the campaign level scripts (and some common
  scripts) to handle `level.player2`: spawning, cutscenes, vehicles, checkpoints, fail and
  restart, end of mission. No new netcode is needed.
- Delivering changed GSC needs either loose-file loading through `fs_game`/`mods/`
  (untested) or a proxy DLL (`winmm.dll` or `d3d9.dll`) that hooks the rawfile lookup.

Unknowns (next experiments, all need a test copy of the install):
- Does `monkeytoy 0` open the console on this build?
- Does setting `coop_mapName` to a campaign map and launching co-op load the map, and does
  `_load` then give both players a working spawn?
- Does a loose `mods/<x>/maps/*.gsc` override the zone copy?
- Can two game instances run on one PC and connect over `connect 127.0.0.1 <port>`, for
  bot-driven QA without a second Steam account?

## Phase 2/3 experiments on a test copy (2026-10-19)

Setup: full copy of the install (183 files, 11.3 GB, without the old COD-MOD files) in a
separate test folder; owner's `players\` backed up to `local/backup` first. Launcher script
watches every `iw4sp.exe` every 200 ms and kills any that is not the test copy.

Observations:
- **Incident:** run 1 worked with `steam_appid.txt` present, but the game **deleted
  `steam_appid.txt`** on that run. Run 2 (no file, no env vars): the stub exited with code
  53 and **Steam relaunched the owner's real install** with the same arguments. It rewrote
  the owner's `players\config.cfg` within ~6 s before it was killed. Restored from the
  backup, SHA-256 matches the original; no other file in the owner's install changed.
  **Fix that works:** start the test copy with env vars `SteamAppId=10180` and
  `SteamGameId=10180` (6 runs after that, all ran from the test copy, owner install
  untouched). Playbook lesson: back up the owner's config before the first run, and guard
  against Steam relaunching the real install.
- The game ran from the test copy, reached the main menu (SP menu now also offers
  "Multiplayer"). Nothing was written outside the test folder (checked LocalAppData,
  AppData, Documents, Saved Games, ProgramData).
- **Command-line `+set`/`+exec` are ignored** (window stayed fullscreen 1920x1080, `quit`
  never ran, no log).
- **`config.cfg` lines:** `seta …`, `set …` and `bind …` take effect (`set r_mode
  "1024x768"` resized the window; `seta cooptest_marker` was saved back). `quit`,
  `cmdlist`, `wait`, `exec` and `set logfile 2` from `config.cfg` had no visible effect: the
  game never quit and no `console.log` appeared anywhere.
- Console: posting `` ` `` (WM_KEYDOWN/WM_CHAR) to the game window, even after making it
  foreground, did not open a console (with `seta monkeytoy "0"` in the config). F10 bound to
  `exec` did nothing either. Unknown whether the console is gone or posted messages are
  ignored; synthetic global input (SendInput) was not tried, it could type into other apps.
- Screenshots: PrintWindow returns black for the D3D9 window; making the window TOPMOST
  for a moment and copying only its client rect works. (A first plain screen copy grabbed an
  overlapping app window; it was deleted. Never copy the screen without TOPMOST.)
  The Discord in-game overlay draws over the game; keep screenshots in `local/`.
- **Second instance:** launching the test exe twice, the second process quit at once;
  only one instance runs (cause unknown: mutex or Steam).

Derived:
- Without a native DLL there is no way to drive this build from scripts: no command line,
  no startup commands, no console, no log. A proxy DLL (`winmm.dll`, imported by the exe
  and not a KnownDLL) is the next step: log to a file, run console commands from code,
  later hook rawfile loading for our GSC.
- Two-client QA on one PC also needs the DLL (single instance), or a second PC/VM.

## Phase 3–5: native DLL on the 64-bit build (2026-10-19, owner OK'd tools)

Tools (pinned, SHA-256 checked, portable in `local/tools`): llvm-mingw 20261006 (ucrt x86_64,
`317492c4…d17a`), Ghidra 12.1.4 (`ddac49f9…d4db`), Temurin JDK 21.0.12.1+1 (`f9d6e191…8b4e`).
Mod source lives in `local/mw2coop` for now (to become its own repo).

Observations (each verified in a run on the test copies; RVAs are from image base, build
PE timestamp `0x6aaaa257`, `version` dvar "IW4 2.0 build 14 … 2025 win64"):
- **Loader:** a `winmm.dll` proxy (180 exports forwarded to System32) placed next to
  `iw4sp.exe` is loaded; image base is randomized (ASLR), so all addresses are RVAs.
- **Commands:** `Cbuf_AddText(int, const char*)` @ `0x1ee890` (locks critical section 0x24,
  safe from another thread). Commands queued this way run, incl. `map trainer` (campaign
  loads), `quit`, `set`. Console command list head @ `0x1315308` (`cmd_function_t` {next,
  name, dir, ext, fn}); 139 commands, incl. `listen`, `connect`, `coopLaunch`,
  `stopListening`, `connect_lobby`, `devmap`, `loadgame`. New commands can be added by
  linking a node into that list (done for `mw2coop_party_join`).
- **Dvars:** `Dvar_FindVar(name)` @ `0x26bdf0` (reader-locked). Layout: name +0, flags +8,
  type +0xC, current +0x10, latched +0x20, reset +0x30, hash next +0x58.
- **Logging:** `Com_Printf` and file logging look compiled out (no `console.log` string, most
  print strings gone). `logfile` dvar still registered. State is read from dvars/memory.
- **Scripts:** loose GSC is not read (tested `main/`, `raw/`; `fs_game` from config is
  ignored). `Scr_AddSourceBuffer` @ `0x217950` calls `DB_FindXAssetHeader(0x24 = rawfile)`
  @ `0xef5d0`, `DB_IsXAssetDefault` @ `0xefff0`, `DB_GetRawFileLen` @ `0xefdc0`,
  `Hunk_AllocateTempMemoryHigh` @ `0x2693f0`, `DB_GetRawBuffer` @ `0xefc70`. Replacing the
  function (14-byte absolute jmp, same logic + override lookup) makes
  `<game>\mw2coop\scripts\maps\trainer.gsc` win over the zone copy: the marker dvar it sets
  appeared in the running game.
- **Single instance:** the game writes its PID to `<game dir>\__iw4sp` (hidden) and quits
  silently if that PID is a live `iw4sp.exe` (@ `0x2900a0`). It also deletes
  `steam_appid.txt` at Steam init (@ `0x240c30`). A second folder (hardlinked data, own exe
  and `players/`) runs a second instance fine; it auto-binds UDP 28961.
- **Online gate:** `listen`/`connect` require Steam logged on and the DemonWare state
  (@ `0x115c07c`) == 4, reached ~7 s after start. Reported via `@PLATFORM_POPUP_CONNECTION`.
- **Direct-IP co-op handshake works natively.** `connect <ip> <port>` stores the port as-is
  into a network-order field, so pass `htons(port)`: `connect 127.0.0.1 8305` targets 28960.
  Packets (logged by wrapping WSOCK32 `sendto` #20 / `recvfrom` #17 in the IAT):
  `coopConnect` (protocol 0x1d, XUID, Steam name) → `coopAccept` (carries `coop_mapName`) →
  `coopAcceptAck` → `bw` bandwidth test → `bwres` → `coopLaunch <map>` → `coopLoaded`.
  Host handler @ `0x22d830` ignores a `coopConnect` from its own XUID; for QA two copies on
  one account need distinct XUIDs (XUID table @ `0x1bbaad8`; QA-only `!xuid_add`).
- **Party:** `coopLaunch` (@ `0xd25d0`) only sets a flag (@ `0x5dd953`); the party frame
  (@ `0xd74a0`) launches when the party is active (@ `0x5dd951`, lobby object @ `0x5dd958`
  +0x10), 2 members (+0x70, +0xa8), bandwidth test done (@ `0x5dd820` == 0). A member that is
  not heard from for `coop_lobbyTimeout` (12 s) is dropped. The normal client activates its
  party by joining the host's **Steam lobby** (@ `0x28f1b0` → `0x22dda0` →
  `PartyClient_Join` @ `0xd7cb0`); a bare `connect` skips that, so the client never sends
  keepalives, ignores `coopLaunch` and is dropped. `mw2coop_party_join` repeats
  `PartyClient_Join` without the Steam lobby lookup.
- **Result (run m14):** host `set coop_mapName trainer; listen; coopLaunch`, client
  `mw2coop_party_join; connect 127.0.0.1 8305` → both load `trainer` with `coop 1`; the
  level script reports `level.players.size == 2` and `level.player2` defined on both; the
  client sees the host's player model with its name tag; ~110 s in co-op without a drop
  until the test was stopped. The host plays the intro in-engine scene, the **client does
  not** (owner noticed it live): that scene is scripted on `level.player` only.
- **Lockstep:** the client also reports `sv_running 1` and compiles the level scripts
  itself; dvars `coop_maxFramesClientCanBeBehind`, `coop_fastForwardThreshold` and the old
  log's "network coop random seed" point the same way. Both PCs run the simulation, so
  every overridden script must be byte-identical on both (else desync).

Derived:
- The networking needed for campaign co-op exists and runs on this build. What is left is
  script work per mission (cutscenes, vehicles, checkpoints, fail/restart, mission end and
  next level) plus a host/join entry point.
- Two real Steam accounts should take the native path (Steam invite → lobby → party join),
  without `mw2coop_party_join` or `!xuid_add`. Untested: needs the owner and a friend.

Owner's install, follow-up to the incident: the relaunched real game had also left a hidden
`__iw4sp` lock file (PID 24472 = the killed relaunch). Moved to `local/backup` so the next
real launch does not show an "improper quit / safe mode" prompt. The guard now lists hidden
files too.

## Co-op campaign framework, S.S.D.D. (2026-10-19, in progress)

Built (mod source `local/mw2coop`, content in `content/`):
- **Script patches instead of shipped game code:** `<game>\mw2coop\patches\<script>.patch`
  (`@@@ replace / with / end`, `@@@ append`) applied by the DLL to the zone text at load;
  `tools/check_patches.py` applies them offline to extracted scripts (all 11 hunks OK).
  Own scripts in `<game>\mw2coop\scripts\` (`maps/_mw2coop.gsc`, `maps/_mw2coop_qa.gsc`).
  The log prints a hash per changed script; host and client hashes matched.
- **Mode:** campaign co-op = `coop 1` and a map not named `so_*`; `is_specialop()` patched
  to return false then (the lobby sets `specialops 1`, which sent `trainer` down its Spec
  Ops branch: no weapon strip, no ambient AI, Spec Ops loadout for player 2).
- **Framework (`_mw2coop.gsc`):** regroup player 2 beside player 1 after the start point and
  on any teleport of player 1; weapon sync (player 1's gains/losses to player 2, `both`
  mode for trainer); checkpoints = level start points (furthest flag sets the `start`
  dvar); no saves in co-op; both downed (`maps\_coop` revive system) or
  `missionFailedWrapper` → fade, then the host runs `fast_restart` (SV_MapRestart @0x22c970
  sends `coopRestart` to the client); `nextmission()` → request "nextmap <next level>"
  (transition still to do). Script→DLL requests via dvar `mw2coop_cmd`, host acts only.
- **trainer patch:** checkpoints timed_ads, bullet_penetration, frag, pit, course, end;
  endless range ammo for both; ending applies invulnerability, no sprint, slow walk and
  the fade to player 2 too.

Found on the way (observed in runs):
- **Co-op scripts cannot read most dvars.** GSC `getdvar` in a co-op session goes through
  `SV_GetDvarCoop` (@0x230fc0): only dvars with a flag in `0x20c4` return their value, all
  others return the default, and name+value go into the co-op validation checksum. So
  console-set dvars and even `start` read as "" (start points would never apply in co-op).
  The DLL ORs `0x80` into `start`, `mw2coop_qa`, `mw2coop_qa_phase` on both machines; after
  that the QA scenario was visible to scripts.
- **Script runtime errors are silent in retail:** `Scr_Error` (@0x2226a0) only longjmps.
  Replaced with a logger + the game's own longjmp (@0x2e485c). The client logs ~60 such
  longjmps from level load on, the host none, with stale `va()` text ("Event EV_PLAY_FX").
  Not caused by mod code; unknown whether vanilla co-op does the same (to check).
- **QA intro run:** PASS two players, campaign mode, player 2 at player 1 (distance 0: the
  free-spot trace found nothing, fallback shares the spot), same (empty) weapons. Then the
  QA's own `GiveWeapon("m4_grunt")` on player 1 ended the session: engine error "no
  viewmodel set for player 0 for 'm4_grunt' in 'right' hand" (sent as `coopDis`).
  `_loadout` for `trainer` sets the viewmodel only on `level.player` and only in its
  campaign branch; next: set the same viewhands on both players in co-op.

Owner asked (2026-10-19): vehicle boarding for both players wherever the campaign needs
it. Level scripts of all campaign maps are extracted in `local/mw2-64/gsc/<map>` for a
survey (not done yet).

## Session 2026-10-20: intro passes, restart in progress

Observed (two-copy QA runs):
- **Client-only script errors** came from `GetPlayerIntelIsFound` (builtin @0x18dea0): in
  co-op it requires a local player (co-op client slot state == 2), so on the client the
  host's player fails. `maps\_intelligence` ran it at level load → its thread died on the
  client only (lockstep drift). Patch `_intelligence.gsc`: remove intel in a co-op campaign
  session before `initialize_intel()`. After that: 0 script errors on both.
- **"no viewmodel set for player 0"** is raised by cgame (@0xc5b70) for the *local* player of
  the machine that draws: the client's own player (player 2). `maps\_loadout` sets hands on
  `level.player` only. Framework now copies player 1's `GetViewModel()` to player 2 at start
  and every 0.25 s.
- Regroup used `PlayerPhysicsTrace` from inside the partner's hull and always fell back to
  the same spot; now `BulletTracePassed` (ignoring the partner) + a floor trace: 48 units.
- **QA `intro`: 6/6 PASS on host and client**, no script errors, no disconnect (04:53).
- QA `restart`: checkpoint `timed_ads` set the `start` dvar on both; player 2 downed (not
  dead); both down → `coop_alldowned`. Then nothing, because (a) `watch_failure` called
  `mission_failed()` in its own thread, whose `level notify("mw2coop_level_end")` ended that
  thread (fixed: `level thread mission_failed()`), and (b) `maps\_coop`
  `level.revive_ent linkTo( self, "tag_origin" )` raises a script error on both machines
  (link builtin @0x18ff30 → parent has a model index but no DObj, @0x19004d): campaign
  players have no body model (Spec Ops sets one with SetModelFunc). Patch `_coop.gsc`: link
  without a tag in a co-op campaign session. Both fixes deployed, **not yet tested**.
- DLL: script errors are logged per calling site (return address RVA), so a builtin name
  can be looked up in the method table.

## Session 2026-10-20 (later): restart, mission end, progress saving

Observed (two-copy QA runs, `tools/qa_pair.sh`):
- **Script call stacks in the DLL log.** The VM keeps its live code position at rva
  0x1900b78 and the callers' saved positions in `function_frame_start[]` (rva 0x18f44c0,
  0x28 bytes per frame, position first; `function_count` at 0x18f44a0). Positions point into
  the program buffer (pointer at rva 0x15a03e0). `Scr_GetFunctionHandle` (rva 0x216920)
  returns a function's offset in that buffer, but only while the loaded-scripts table exists:
  `Scr_EndLoadScripts` (rva 0x2166d0) frees it. The DLL hooks that function, maps every
  function name found in the compiled source texts (5414 on S.S.D.D.) and prints
  `file::function +offset` for the failing position and each caller.
- Retail script errors do not always end the thread: one site logged 100 hits in 2.4 s.
- With the stacks, all remaining errors on downing were in `maps\_coop` (patched):
  - `flag( "special_op_terminated" )` on a flag only `maps\_specialops` creates (also read by
    `kill_wrapper()` in any co-op session). `_mw2coop` now creates it in a co-op campaign
    session and sets it when the level fails or ends, as Spec Ops does, so the revive HUD
    and downed-player threads stop.
  - `player_coop_kill()` → `kill()` on a player already made invulnerable. In the campaign it
    now fails the mission (restart at the checkpoint) instead of killing; a bleed-out does
    the same.
  - `player_coop_downed()` went on after `player_coop_kill()` (in Spec Ops the player dies
    there) and started the bleed-out HUD after its cleanup; it now returns once the level
    has ended.
- Patch parser bug (DLL and `check_patches.py` alike): a block right after `@@@ end` with no
  blank line in between was swallowed. Fixed in both; `_coop` now reports 4/4.
- **QA `restart`: all checks PASS on host and client, 0 script errors** (05:18): checkpoint,
  player 2 down, both down, `coopRestart` sent and acked, level back at checkpoint
  `timed_ads`, players up.
- **QA `end` (start point `end`): PASS on both, 0 errors**: mission end reached, next
  mission `roadkill` found. The DLL does not change map yet.
- On PC, S.S.D.D. skips the timed-ADS drill (`AA_timed_ADS_init`: `!level.console`) and
  sets its flag at once, so the `bullet_penetration` checkpoint follows `timed_ads` within a
  second. Expected, not a bug.

Progress saving (owner asked to check it):
- Before: nothing outlived the game. The checkpoint is the `start` dvar in memory; in a
  co-op session the game wrote no save (`players/save` untouched, only `config.cfg`), and
  `mission_complete()` replaces `_nextmission()`, so the profile records (`highestMission`,
  `missionHighestDifficulty`, `UpdateGamerProfile`) are not written either.
- Now: the host's DLL writes `<game dir>\mw2coop\progress.txt` (`map`, `start`) on each
  checkpoint and at mission end (next mission, no start point). `mw2coop_continue` on the
  host sets `coop_mapName` and `start` from it. On each `coopLaunch` the host also sends the
  client an out-of-band `mw2coop_start <start>` packet; the client sets its own `start` from
  it and drops the packet, so both lockstep machines pick the same start point.
- Script requests to the DLL are queued 0.3 s apart (the DLL reads the dvar every 250 ms).
- First continue run caught a desync: the host packet carried an empty start because the
  quoted dvar string failed the name check; host started at `bullet_penetration`, client at
  `default`. The QA check had passed on the client because, with no start point, the level
  writes its list of start points into `start`. Fixed both (raw dvar value; the check now
  reads `level.start_point`).
- **QA `resume @continue`: PASS on both, both at `bullet_penetration`, 0 errors** (05:32).
  After QA `end`, `progress.txt` holds `map roadkill`.
- Not done: profile records (mission list in the menu); the move to the next mission.

## Next-mission transition (2026-10-20, in progress)

- Spec Ops "play again" calls `ChangeLevel( level.script )` in a co-op session
  (`maps\_specialops_code::specialops_summary_player_choice`), so `mission_complete()` now
  waits 1 s (progress request) and calls `ChangeLevel( next, keepWeapons )` on both machines.
- Observed: QA `end` reached the mission end on both, saved `roadkill`, then both stayed on
  `trainer` until the run timed out; no script error.
- Builtin table entry at rva 0x3c2dc0: `changelevel` → 0x1937e0, `missionsuccess` →
  0x1938d0. Both only act when `g_reloading` (dvar pointer at rva 0xd9c020, registered in
  0x172830) is 0 and the int at rva 0xd9e1d4 is above 0; then they store the map name
  (0x174fe0 → buffer at 0xd96e30) and set the exit flag at rva 0xd97b80, which 0x170e60
  reads. Unknown yet: what 0xd9e1d4 is (guess: a player health or client count) and what
  0x170e60 does with the flag in a co-op session.

## Status

Next: next-mission transition (why `ChangeLevel` does nothing in co-op), then the vehicle
survey; consider body models for campaign players. Profile records for the mission menu are
open.
