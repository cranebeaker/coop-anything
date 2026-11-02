# Modern Warfare 2 (2009) — session notes

Session date: 2026-10-19. Goal: the **full campaign, with its cutscenes**, in co-op with
friends (Spec Ops is not the goal). The existing campaign co-op mod (COD-MOD, below) was tried
on 2026-07-04 and works badly.

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

## Decisions (2026-10-19)

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

## Phase 3–5: native DLL on the 64-bit build (2026-10-19)

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
  not** (seen live): that scene is scripted on `level.player` only.
- **Lockstep:** the client also reports `sv_running 1` and compiles the level scripts
  itself; dvars `coop_maxFramesClientCanBeBehind`, `coop_fastForwardThreshold` and the old
  log's "network coop random seed" point the same way. Both PCs run the simulation, so
  every overridden script must be byte-identical on both (else desync).

Derived:
- The networking needed for campaign co-op exists and runs on this build. What is left is
  script work per mission (cutscenes, vehicles, checkpoints, fail/restart, mission end and
  next level) plus a host/join entry point.
- Two real Steam accounts should take the native path (Steam invite → lobby → party join),
  without `mw2coop_party_join` or `!xuid_add`. Untested: needs two people with their own Steam accounts.

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

Next step (2026-10-19): vehicle boarding for both players wherever the campaign needs
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

Progress saving (checked):
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

## Next-mission transition (2026-10-20)

- Spec Ops "play again" calls `ChangeLevel( level.script )`, so the first try called
  `ChangeLevel( next, keepWeapons )` on both machines. Observed: both stayed on `trainer`,
  no script error.
- Builtin table entry at rva 0x3c2dc0: `changelevel` → 0x1937e0, `missionsuccess` →
  0x1938d0. Both act only when `g_reloading` (dvar pointer at rva 0xd9c020) is 0 and the int
  at rva 0xd9e1d4 is above 0; then they store the map (buffer at rva 0xd96e30) and set the exit
  flag at rva 0xd97b80. The exit handler (rva 0x170e60) fades out and runs `spmap <map>`, but
  **returns at once when `gameMode` (dvar pointer at rva 0x192da20; values none/sp/so/mp) is
  2 ("so")**, which a co-op session uses (watched: `gameMode` 0 → 2 when the lobby forms).
- Lobby launch: the `coopLaunch` command sets the launch flag (rva 0x5dd953); the party
  frame (rva 0xd74a0) needs the party active (rva 0x5dd951), the lobby with two members
  (lobby +0x70 and +0xa8), rva 0x1905300 == 0 and rva 0x5dd820 <= 0; then the host sends the
  `coopLaunch` message (rva 0x22e610) and both machines run rva 0x22e1a0: co-op slot state
  (rva 0x533cb8, per local client, stride 16) = 3, net rate dvars, `map <coop_mapName>`.
- Observed (watches): the party goes inactive (0x5dd951 → 0) on both machines when the level
  starts, so a `coopLaunch` given in a level only sets the flag; and the client's
  `coopLaunch` handler (rva 0x22d2e0, from the message table at rva 0x350ce0) ignores the
  message unless its slot state is 0 (it is 4, then 6 after `coopLoaded`).
- Fix: on the level-end request both DLLs set `coop_mapName` and call rva 0x22e1a0 themselves
  (console command `mw2coop_launch_map`). **QA `end`: both machines load `roadkill`,
  exchange `coopLoaded`, two players, campaign mode, PASS on both** (06:01). On `roadkill`
  player 2 starts 4668 units from player 1 (player 1 starts in a convoy vehicle) and
  `roadkill_code::player_shepherd_fov` fails on both (`LerpViewAngleClamp` on
  `level.player`): level work for that mission.

## Test god mode (2026-10-20)

- `_mw2coop::god_mode()`: every frame each living player gets `EnableInvulnerability()` and
  full health. On unless the host sets `mw2coop_god 0`. Must match on both lockstep
  machines (damage applied on one only would desync), so the host's launch packet now
  carries `start=… mw2coop_god=…` and the client sets both; `mw2coop_god` also gets the
  co-op readable flag. The `god` console command would act on one machine only.
- QA `god`: heavy damage on both players → both alive, neither downed, health 100/100,
  PASS on both. QA `restart` with `mw2coop_god 0` on the host only: the client got `0` from
  the packet; all PASS, 0 errors.

## Vehicles for two players (2026-10-20, in progress)

Requirements: in a vehicle both players sit on different seats; on a vehicle turret only one
player, whoever gets in first, and the players can swap. Places: Team Player
(`roadkill`, the only vehicle turret), Wolverines! (`invasion`: Humvee ride at the start, getting
into a parked Humvee at the very end after defending Burger Town).

Survey (grep of each mission's own scripts for `UseBy( level.player`, `level.player
PlayerLinkTo` and vehicle names; a starting list, not a full audit):

| Mission | Player and vehicles | Status |
|---|---|---|
| roadkill (Team Player) | Humvee: board through a usable seat, then the 50 cal turret (`player_ride_vehicle`, `player_gets_on_turret`); ride ends in a crash slide | done, QA `rkintro`, `board`, `rideend` pass |
| invasion (Wolverines!) | opening: passenger seat via `_utility::player_rides_shotgun_in_humvee`; end: parked Humvee seat `tag_guy1` | done, QA `ride` passes; player 2 on the rear-left seat |
| cliffhanger | player drives a snowmobile (380 mentions). Next step: a second snowmobile for player 2; the mission-end trigger only when player 2 has arrived too | not looked at |
| af_chase (Endgame, the chase after Shepherd) | player drives the boat (`friendly_boat UseBy( level.player )`) | not looked at |
| oilrig (underwater start) | SDV / underwater set piece (`PlayerLinkTo`) | not looked at |
| gulag, oilrig, af_caves, dcemp, estate, boneyard, favela_escape, airport, contingency | `PlayerLinkTo` scenes (helicopters, boats, set pieces) | not looked at |
| dcburning | helicopters / Humvees, no player link found by the grep | not looked at |

Seats: `level.vehicle_aianims[ vehicletype ][ pos ].sittag` (Humvee: 0 tag_driver,
1 tag_passenger, 2 tag_guy0, 3 tag_guy1; `humvee50cal` adds 4 tag_guy_turret); a vehicle's
`usedPositions[ pos ]` marks seats AI riders hold. `_mw2coop::seat_partner_in_vehicle()` puts
player 2 on a free non-driver, non-turret seat (marking it used), linked like player 1
(`PlayerLinkTo` with view arcs, weapons off), else at `tag_body` offset (-50, 0, 10).

- Patched `_utility::player_rides_shotgun_in_humvee` / `player_leaves_humvee` (player 2 rides
  and leaves with player 1) and `invasion::player_enters_convoy_humvee` (seat for player 2).
- QA `ride` on invasion: player 1 seated, player 2 seated 72 units away, still together while
  driving, both out, player 2 next to player 1: PASS on both, 0 errors (06:18 and 06:22).
- Observed: in the opening Humvee all rear seats hold AI (`humvee: 0=tag_driver* 1=tag_passenger
  2=tag_guy0* 3=tag_guy1*`), so player 2 rides at the fallback spot: a screenshot from the
  client shows the view from the roof hatch (turret ring ahead).
- A real seat was chosen over the roof hatch: Worm (`script_noteworthy` "worm", a squadmate
  the mission lets die after the yards; every later use checks `isalive( level.worm )`) gives
  up his rear-left seat. `invasion` sets `level.mw2coop_spare_rider = "worm"`;
  `seat_partner_in_vehicle()` removes that rider (`evict_rider()`) when no seat is free. At
  seating time the rider's spawn function has not run yet, so he is matched by animname or
  else script_noteworthy. The opening's three unchecked `level.worm` uses got `isalive()`.
  QA `ride`: `player 2 seated on tag_guy0 spare worm: removed`, all PASS on both, 0 errors
  (06:37); the client screenshot shows the view from the rear seat behind the driver.
- `follow_leader_teleports` now leaves a linked player 2 alone.
- Seen on roadkill: player 1 is blended (1 s) into the Shepherd intro scene, too slow for the
  512-units-per-frame teleport check, so player 2 stays 4668 units away; needs a distance
  leash.

Roadkill design (not built yet): the usable seat (and its 80-unit auto trigger) answers either
player; the first is the gunner (`level.mw2coop_gunner`) and gets the get-in animation and the
turret; the convoy (`player_gets_in`) waits until the second boards a recreated seat (or a
timeout seats him); the passenger holds Use to swap; ride end unlinks both. Unknown: how to
take a player off a turret by script (`UseBy` again?).

## Team Player for two players (2026-10-20)

Method: before solving a mission, look at how COD-MOD did it (a reference, not a
base to build on). COD-MOD's `x_coop.ff` scripts (`local/`, read only, no license, nothing
copied): in roadkill it links both players to
the same rig in Shepherd's scene, links player 2 lying on `tag_seat_rb_attach` once player 1
is on the turret, unlinks both at the ride end and links both to the crash slide. No choice
of gunner, no swap, start points not handled.

mw2_coop (`_mw2coop_roadkill.gsc`, patches to `roadkill_code`, `roadkill`,
`_minigun_viewmodel`; outside co-op every patched spot does what it did):
- The glowing seat (and its 80-unit auto trigger) answers either player; the first is the
  gunner (`level.mw2coop_gunner`, `_mw2coop::vehicle_gunner()`), gets the get-in animation and
  the turret; the turret hands follow the gunner's buttons.
- The convoy flag `player_gets_in` waits until the second player reaches the seat again (or 30
  s); he takes a free seat with weapons (`tag_passenger` was free in this Humvee).
- Swap: the passenger holds Use for 1 s. Taking a player off a turret: `EnableTurretDismount()`
  then a second `turret UseBy( player )` (it also unlinks him; QA `turret`).
- Observed: doing the swap in one server frame crashed both machines on the second swap
  (access violation writing 0x1c at rva 0x15f515, `TeleportPlayer` with `ent->client` NULL,
  stack 0x175ca5 / 0x15b73f / 0x234331). Spreading it over three frames fixed it (QA `board`:
  swap and swap back pass).
- Ride end and crash: both get off where they are; player 2 lands next to player 1.
- Start points `ride`, `ambush`, `ride_later`: observed player 1 linked in the first frame,
  left behind when the start point moved the Humvee to its path (13375 units from the
  turret he was using) and unlinked about 2.25 s later; he is mounted again after 1 s.
- Shepherd's scene: player 2 is linked to the same view rig (COD-MOD's idea) and set next to
  player 1 afterwards.
- **Script-set dvars:** a method trace (DLL `!trace <rva of a method table entry>`, unlink at
  rva 0x342c88 → 0x187e20) showed `_introscreen::flying_intro` unlinking player 1 at 2.25 s.
  The slam-zoom intro runs only when `getdvarint( "newintro" )` is 0; roadkill.gsc sets it to 1,
  but in co-op getdvar reads "" for any dvar without the 0x20c4 flags, so the SP-only intro ran
  and broke the scene (`player_shepherd_fov` LerpViewAngleClamp error). The DLL now wraps the
  GSC `setdvar` builtin (function table entry rva 0x3c1568 → 0x185500; name via
  `Scr_GetString` rva 0x223440) and gives the dvar 0x80 after the set: both machines set the
  same value, so reading it back is lockstep-safe. After that: no slam zoom, player 1 stays in
  the scene, 0 script errors.
- DLL crash logger (vectored exception handler): faulting RVA, registers, return addresses.
- QA (two copies, 0 script errors on both): `rkintro` (both in the scene, 48 units apart
  after), `board` from `convoy` (player 2 first → gunner, convoy waits, player 1 seated, swap
  and swap back, both in the moving Humvee 47 units apart), `rideend` from `ride_later` (player
  1 at the turret 51 units, crash at 21.5 s, both off, 48 units apart), `turret`.

## Cliffhanger, spawn audit, Gulag and Takedown seats (2026-10-20, later)

Cliffhanger snowmobiles (COD-MOD has no cliffhanger script, nothing to compare):
- `maps\_snowmobile_drive::drive_vehicle()` already handles a second player (Spec Ops). Player
  2 gets his own snowmobile from the map's `playermobile_spawner` (the player's spawner spawns
  once); either player may mount either snowmobile, the one player 1 rides becomes
  `level.player_snowmobile`; start point `snowmobile` mounts both; after `end_begins` the
  helicopter ending waits until player 2 is off his snowmobile within 400 units of player 1.
- `_blizzard::blizzard_start_proc` plays the co-op blizzard on the player's `tag_origin`;
  campaign players have no body model (the `coop_body_*` models exist only in the so_* zones:
  grep of inflated zones), so the call failed; it now uses a tag_origin model linked to the
  player.
- QA `snow` (start snowmobile → favela on both), `snowpick` (start icepick, player 2 takes
  player 1's snowmobile first): PASS, 0 errors.
- No Russian is always played (this build's `_nextmission()` skips airport after
  cliffhanger unconditionally, "RUSSIAN PC BUILD" comment).

Ice climb (in progress; goal: fully for two players, no workarounds, quality as if
Activision shipped it): `cave_main` already runs `_climb::climb_wall` for every player at its
own `player_climb` start (two exist: wrong, there is one, see the climb section below), but `_climb.gsc` uses `level.player` 158 times in 35
functions, global flags (`player_in_position_to_climb`, `finished_climbing`, `final_climb`,
`climb_pullup`, `climb_big_jump`, ...), `level.arm_ent_globals`, `level.stabs_completed`, and
reads the swing buttons with `ButtonPressed( "mouse1"/"mouse2"/"j"/"k" )` - local keys, which
desync lockstep for the remote player. Plan: `@@@ subst` level.player -> the climbing player
per function, per-player ent_flags fed by the same trigger volumes, usercmd buttons
(`AttackButtonPressed`/`AdsButtonPressed`), per-player arm globals, big jump with both players
slipping and Soap MacTavish (`level.price` in the scripts) pulling both up, DLL input injection
for QA. The snowmobile escape is done.

Spawn audit (QA `spawn`, every mission's default start, 12 s): player 2 next to player 1 on
ground in trainer, roadkill, cliffhanger, favela_escape, arcadia, dcburning, contingency,
dc_whitehouse, estate, boneyard, af_caves, af_chase; seated in invasion. Problems where player 1
starts linked: gulag (player 2 fell 67000 units from the map's player start), favela (car),
oilrig (SDV underwater: player 2 on the sea floor 3530 below), airport (elevator: both on one
spot), dcemp (crash scene), ending (credits: 8 `_credits::playcredits` errors).
- Teleports now refuse while player 1 is linked or in the air and retry until he stands
  (`is_standing`: `IsOnGround()` reads 0 for player 2 in co-op, so ground is traced).
- Gulag: player 2 gets his own `get_player_view_controller` on a free seat of the fly-in
  helicopter (COD-MOD had a second controller too), looks at the same target, gets off with
  player 1. Takedown: player 2 rides the opening car (its `luxurysedan` aianims only have a
  driver seat, so he is at the fallback spot for now) and gets out with player 1. Audit PASS
  for both (53 and 66 units, linked), 0 errors.
- Patch op `@@@ subst <function>` (DLL + check_patches.py): whole-word replace inside one
  function, so reworks do not put game code into patch files.

## Cliffhanger ice climb for two players (2026-10-20)

Both players climb the ice wall, run and make the big jump, Soap MacTavish (`level.price` in
the scripts) catches each of them, and both climb the last wall; the level goes on once both
are up. Mod files: `scripts/maps/_mw2coop_climb.gsc` (logic), `_mw2coop_climb_qa.gsc` (QA bot),
`patches/maps/_climb.gsc.patch` (110 hunks), `_climb_anim.gsc.patch` (8), `cliffhanger.gsc.patch`
(2), all generated by `tools/gen_climb_patches.py` from the extracted scripts.

Observations (QA logs, screenshots of both copies):
- The map has **one** `player_climb` start, not two (correction of the section above):
  `cave_main` indexed `climb_starts[ 1 ]` for player 2 → two script errors and no start for
  him. Player 2's lane is now computed: beside the start along the wall, on the side away from
  Soap's climb node (`cliffhanger_cliff`), checked with 12 bullet traces up the wall for a
  surface the picks take (ice/rock/snow/plaster). Here: 72 units, wall lane offset
  (-18.6, -69.5, 0).
- `climb_startdir` reads "" in this build when the climb starts (`create_dvar` =
  `SetDvarIfUninitialized` did not take), so `get_anims_for_climbing_direction` returned
  undefined and the arms had no animations until the first stab: script errors in
  `player_finishes_climbing`/`player_relinks_to_tag` (rva 0x21f1b6: index on a non-array;
  0x21ee98/0x2263d6: compare with undefined) and no getting-on-the-wall animation. Co-op uses
  "up" for an unknown value (single player left as it is).
- Across the gap the wall lane offset points almost along the jump direction, so the far side
  (big jump node, `climb_catch`, the getup struct) uses its own offset along `climb_catch`'s
  right, side picked by the same wall test (here 70 units, score 6 vs 3).
- `if ( !flag( "can_save" ) ) return;` at the end of `player_big_jump` stopped both players
  from reaching the top: `can_save` reads 0 in co-op by then (cleared somewhere, writer not
  found). In co-op the check is the player's own fall (`fade_to_death`).
- Trigger geometry (origins): `climb_jump_prep` (455, -29245, 855), `climb_big_jump`
  (497, -29084, 948, touching), `climb_icepick_slide` (531, -28878, 682, touching),
  `ramp_block_notsolid` set (447, -29132, 810) and **clear** (461, -29264, 860) triggers; the
  per-player monitor treats `*_flag_clear` triggers as clearing.
- Loading: a script loaded on every map must not name `maps\_climb` (only in Cliffhanger's
  zone): the first version had the climb QA in `_mw2coop_qa.gsc`, and every other map failed
  with "Could not find script 'maps/_climb_anim'" (caught by the `snow` and `leash` runs). The
  climb QA lives in `_mw2coop_climb_qa.gsc`, reached through `level.mw2coop_qa_climb`.

Design (as built):
- `level.player` → the climbing player in every climb function (`self`, `ent.player`,
  `self.player`, `self.mw2coop_player`); single player is unchanged (the climber is
  level.player there).
- 20 per-player flags (8 trigger flags fed by an `IsTouching` monitor on the same volumes,
  12 climb state flags). A per-player set also sets the shared flag; `reached_top` is set when
  both have it.
- Swings: usercmd (`AttackButtonPressed` left, `AdsButtonPressed` right, as the developers'
  comments next to the mouse1/mouse2 calls say) instead of local `ButtonPressed`.
- Big jump: Soap jumps once; his catch is queued (`start_gaz` notetrack): while he catches
  one player, the other's rig holds (rate 0) where he hangs on, then Soap catches him.
- `level.mw2coop_no_follow` during the climb: no follow-teleport and no leash, so nobody is
  moved up the wall.
- Known, not done: `cg_fov` 78 while climbing and the climbing depth of field are global /
  level.player only; the catch slow motion (0.2 s) is global.

QA `climb` (default start, bot: swings via `mw2coop_qa_hold`, walking and running as velocity
pushes; it cannot find its way along the cave ledge, so it sets `price_begins_climbing` after
6 s), run 2026-10-20 on both copies, identical script hashes, 0 script errors:
```
PASS two players (2)
PASS player 1 reached the top
PASS player 2 reached the top
PASS Soap caught player 1
PASS Soap caught player 2
PASS the level goes on (reached_top)
PASS both players together at the top (60 units)
```
Log order: Soap catches player 1 → "player 2 hangs on, Soap is catching the other player" →
player 1 at the top, waiting → Soap catches player 2 → both at the top. Regression the same
day: `leash` (trainer), `snow` (now on to airport), `snowpick` PASS. Real swing input from the
mouse (usercmd path) is not exercised by the bot: untested.

## Takedown opening car for two players (2026-10-20)

COD-MOD only put player 2 at a fixed spot in the street (`level.players[1] SetOrigin` in its
favela.gsc), outside the car. Observations: the sedan's back seat is empty during the ride
(Soap is spawned outside when he runs off: `anim_teleport_solo` on `soap_start_node`); the
level rides level.player on a rig linked to `tag_passenger`, ducks on `notifyOnCommand`
(player 1's commands only; Spec Ops uses `player NotifyOnPlayerCommand`), and Makarov's shots
10-15 kill level.player unless the global `player_is_ducking` is set.

Built (`scripts/maps/_mw2coop_favela.gsc`, `patches/maps/favela.gsc.patch` 17 hunks generated by
`tools/gen_favela_patches.py`; generators share `tools/gsc_patch.py`):
- `player_rides_vehicle` and `player_exits_vehicle` run for every player (`level.player` →
  self); player 2's rig is on the back seat, 40 units behind `tag_passenger`, with the same duck
  and death animations and his own crouch commands.
- `player_is_ducking` = "a player ducks" (the hotel doors); each deadly shot hits every player
  who is not ducking (`kill_player( player )`), and a hit fails the mission 1.5 s later, as in
  single player (no being downed in the car).
- Getting out: player 2 waits for player 1, slides to the front seat and gets out through the
  same open door to a floor spot beside the exit point.

QA 2026-10-20, both copies identical, 0 script errors: `takedown` (both in the car, eyes 39
units apart, both duck, nobody shot, both out 47 units apart, alive), `takedown2` (player 2
stays up: "Makarov hits player 2", mission failed and restarted on both 1.5 s later), `spawn`
on favela PASS. Screenshots: player 2 sees the front seat's headrest and the van ahead; gets out
through the open front door. Ducking was sent as the `go_crouch` notify: the real crouch key
through `NotifyOnPlayerCommand` is untested.

## The Only Easy Day... (oilrig) opening for two players (2026-10-20)

COD-MOD linked both players to player 1's rig on SDV A and hid them (two cameras on one rig),
for the grate kill too: not used. Observations: SDV A carries pilot, copilot, swimmer1 and the
player (`oilrig_sub_A_disembark_player` = place 3); SDV B (from the second submarine, ~16900
units away at the start) carries four SEALs, place 3 = `sdv02_swimmer1`. The grate scene is
one synchronized kill: the player's first-person kill of guard 2 and the SEAL `sdv02_pilot`'s
kill of guard 1 (no first-person animation exists for guard 1); then two SEALs
(`water_helper_01/02`, `surface_helpout`) pull the player out, timed by the "help" notetrack of
the player's kill animation. `killtrigger_ocean` is switched on once the player is out and
kills any player touching the water.

Built (`scripts/maps/_mw2coop_oilrig.gsc`, `patches/maps/oilrig.gsc.patch` 24 hunks from
`tools/gen_oilrig_patches.py`):
- Player 2 rides SDV B in sdv02_swimmer1's place (that SEAL is left out in co-op) with the
  player's ride-and-swim animation; only player 1's rig drives the swim flags (same length).
- Every player bobs and swims by the grate (`water_bob` per player); player 2's swim ends 35
  units higher (over the walkway), so he bobs at player 1's height.
- Stealth kill: the first player to strike (in the trigger, looking at the guard, melee) does
  it; the hint shows when any player looks. The other player swims until the killer is out,
  then his own rig runs the kill animation unseen (rate 8) up to "help" and he is pulled out
  by the two SEALs (their help-out animation again), beside the killer.
- The ocean kill trigger spares the player still waiting to be pulled out.
- Known: the water vision (above/below) and HUD follow the killer on both screens.

QA 2026-10-20 (`oilrig`: player 1 kills; `oilrig2`: player 2 kills), both copies, identical
results, 0 script errors after the ocean fix: both on an SDV, both surfaced (235 units
apart, same height), killer out, the other pulled out 5.75 s later, both on the platform 24
units apart, alive. The strike is forced by the bot (`mw2coop_qa_melee`): looking at the
guard and the melee key are untested. Screenshots: player 2 in sub B's lock tube, behind a
SEAL's fins underwater, in the water under the grate during the kill, on the platform after.

## No Russian elevator, crash site and Second Sun opening for two players (2026-10-20)

No Russian (`airport`): the level holds player 1's view on a script_origin at the back of the
elevator for ~20 s (zoom, gas mask overlay, `airport_vision_elevator`); player 2 shared that
spot. He now stands at the back wall beside player 1 (free spot: floor, clear line, no team
member within 30 units; here 36 units to the right), held the same way and let go with him
(`_mw2coop_airport.gsc`, `airport_code.gsc.patch` 2 hunks). QA `spawn`: d 36, both linked;
screenshot: player 2 in the right back corner, Makarov's "Remember, no Russian".

Crash site (`dc_crashsite.gsc`, byte-identical in the dcburning and dcemp zones, so one patch
serves the end of Of Their Own Accord and the start of Second Sun): player 2 gets his own rig
with the wake-up animation beside player 1 (side picked by 5 view traces out of the wreck: the
hull blocks one side), his own movement in the cabin grid (`player_crash_movement` per
player), his rifle and Foley's magazine (`player_crash_gun_behavior` per player; the autosave
in it only for player 1: twice at once gave `_autosave_game_now` script errors). The crash site
logic is in `_mw2coop_crashsite.gsc`, which names no dcemp/dcburning script (loaded in both
zones). Second Sun (`_mw2coop_dcemp.gsc`): at the ISS player 2 floats 64 units beside player 1
on a tag_origin linked to player 1's animated camera; his view and stance are kept for after
the EMP; `emp_empaftermath` and `emp_free_player` run per player (player 2 out 0.6 s later,
40 units beside).
QA `dcemp` (2026-10-20, both copies, 0 script errors): both in the wreck (eyes 35 units
apart), both got a rifle, both at the ISS (64), both back in the wreck (35), both out (40),
alive. QA `crashsite` (dcburning, start crash): both in the wreck, rifles, both still in it at
the end; the level moves on to contingency.
Open: `dcemp_code::street_heli_player_kill` (the street helicopter kills a player who stays in
the open) raised a script error with the bot idle after the QA, and is written for level.player
only: player 2 is not threatened there yet.

## Second Sun street and the ending map (2026-10-20)

Second Sun street (`dcemp_code::street_heli_player_kill`, `street_hide_moment`): a falling
little bird chases level.player until the team and "street_player_hide" (any player in the
hiding place) set `street_safe`. Co-op: one helicopter per player (`street_helis`), each stops
when its player has hidden (per-player latch from the same trigger) or the street is safe;
`street_safe` waits for every living player; "street_insta_death" is per player. QA `street`
(start street, bot teleports player 1 into the hiding place, follow-teleport off): street not
safe while player 2 is out (his helicopter reached him: `kill()` raises a script error under
test god mode), safe once both hid, both alive.

Ending map (`ending`): `ending.gsc` sets `level.level_mode` from `ui_char_museum_mode`, which
af_chase's end sets to "credits_1" (both lockstep machines run that script, so both have it).
"credits_1" is the knife fight with Shepherd (`af_chase_knife_fight`, starts wakeup ...
walkoff) followed by the museum credits. Launched directly in the spawn audit, the start list
held only the museum starts (" ** no_game arcadia ... af_caves "), the level started at
"af_caves", and `_credits::playcredits` failed on `level.linelist.size` (8 script errors) with
player 1 on the museum camera and player 2 standing 1500 units away. Unknown: whether the
knife-fight starts are registered when the map is reached from af_chase; needs a run from the
end of af_chase. Not changed yet.

## Ending map mode, No Russian ending, QA runner (2026-10-20)

- Ending map: in a co-op session `ending.gsc` read `ui_char_museum_mode` empty (the dvar watch
  showed "credits_1"; cause unknown), so the map went to the free-roam museum with broken
  credits. Co-op now treats an unknown or empty value as "credits_1" (`_mw2coop_ending.gsc`,
  `ending.gsc.patch`): the knife fight starts (start "wakeup"; starts wakeup ... walkoff).
  Player 2 is still 43000 units from player 1 there: the knife fight for two is next.
- Design (2026-10-20): knife fight = both wounded, in turn (the first to reach
  Shepherd plays the stab/monologue/revolver scene, the other crawls and watches; both pull
  out the knife, the first throws); No Russian ending = Makarov kills both.
- No Russian ending (`airport.gsc.patch` 8, `airport_code.gsc.patch` 11 hunks): either player
  can be the one at the van; Makarov shoots that player first (close ending) or the nearer one
  (far ending), then unlinks, turns (ForceTeleport to face him) and shoots the other with his
  far-ending shooting animation; the other goes down where he stands with the far-ending player
  animation. Each white flash on the victim's own screen; the death vision with the second shot.
  QA `nrend` (far: player 2 first, then player 1) and `nrend2` (player 1 at the van door: close
  ending, then player 2): both shot, both down. Screenshots: player 2 down at the ambulance with
  Makarov over him, player 1 down by the wheel, then both white out. Open: in `nrend` (far
  ending) one script error in `_anim::anim_loop_packet` from `escape_end_sequence` (the van
  mate's loop) after the second shot; not in `nrend2`. Not yet known whether single player has it.
- QA runner: `run_test.ps1 -DoneLogs` closes the test copies once every log has "qa: N: done"
  (the game windows stayed open until the timeout). `leash` closed after 71 s instead
  of 170.

## Endgame knife fight for two players, mod localized strings (2026-10-20)

COD-MOD linked both players to player 1's rig and teleported them to one fixed spot; it also
forced `level_mode = "credits_1"` in co-op (the same empty-dvar problem as ours).

Observations: `af_chase_knife_fight.gsc` and `_code.gsc` are byte-identical in the af_chase and
ending zones (192 `level.player` uses). The scene has first-person arm animations only for the
player; a third-person `player_body` animation exists only for Price waking up. Players have no
body model in the campaign, and the engine has no per-player entity visibility (only
`target_showtoplayer`/`target_hidefromplayer` for HUD targets), so anybody looking at the lead
sees floating arms (seen on the client: only the arms were left). The crawl reads
`ButtonPressed("mouse1"/"mouse2")` (local keys); the hints say +attack (right), +speed_throw
(left). `getdvar` read `ui_char_museum_mode` empty in co-op (dvars set by another map's script).
A player linked with `AllowStand(false)` and `AllowCrouch(false)` gets the session dropped
("All stances (standing & crouching) disallowed for linked player"). This build's localized
strings are Windows-1251 (`!loc COOP_WAITINGFORPLAYER`: ce e6 e8 e4 ...); a plain script string
with Cyrillic, UTF-8 or 1251, is drawn as dots.

Built (design: both wounded, the first to strike plays the scene; then, after
seeing the floating arms: the other player drifts in and out of consciousness):
- `tools/gen_knife_patches.py`: every `level.player` of both files -> `level.mw2coop_lead`
  (set to level.player in `_mw2coop::main()` for single player too); nearer player is the lead
  candidate; a melee by any player counts; crawl buttons from the lead's usercmd.
- Player 2 wakes up 64 units beside player 1 with his own rig; wounded walk afterwards.
- The other player (`_mw2coop_knife.gsc`): own black overlay with breathing/heartbeat and the
  text `MW2COOP_STUNNED` from the strike on; he comes to when the knife is out
  (`knife_pulled_out`) and watches the throw, Shepherd's death and Price from a ground camera
  120-240 units from the fight, turned so the lead is at least 60-81 degrees off his view (his
  view arcs 25/25/15/10); he blacks out at once if Shepherd/Price jump (>80 units per frame:
  the scene's cuts) and at the walk to the helicopter (floating arms again).
- `src/localize.c`: `DB_FindXAssetHeader` trampoline (15 prologue bytes) answers
  `MW2COOP_*` localize entries in Russian (1251) when `MENU_YES` reads "Да", else English.
- `tools/gsc_patch.py` finds functions by brace matching (a function ending in "}		" made the
  old `
}
` search run into the next function).

QA `knife` (2026-10-20, ending, start wakeup; bot: striker teleported in front of Shepherd,
alternating crawl buttons, quick Use taps, aim and "throw"): both wake up 64 units apart; player
1 strikes and plays the whole scene (crawl 84 s, knife 23 s, throw 6 s); the other is out until
the knife is out, awake 114 units from Shepherd with a clear line as Shepherd dies, 3 s and 30 s
later; 0 script errors. `knife2` (player 2 strikes): player 2 plays the scene, player 1 is the
witness, awake 121 units from Shepherd as he dies; 0 script errors. Screenshots: the Russian
stunned text; Shepherd with the knife in his
eye; his body from the ground. Lip sync: voice and lips seen out of sync on both
screens; `timescale` stayed 1 the whole scene except the throw's slow motion (back to 1), so not
the cause; open.

## QA bots press real buttons (2026-10-21)

Console `+activate` did nothing because the 64-bit build has no such console command (`!cmds`:
168 commands; of the `+` ones only `+moveup`, `+speed`, `+melee_breath`, `+throw`,
`+nightvision`, `+incAnimWeight`, `+decAnimWeight`, registered at rva 0xcf5d0 and 0xd3240).
How a key works (Ghidra, iw4sp.exe build 25374375):
- Bind name table at rva 0x3be770, 0x4c `char *` entries: index 0 empty, `+attack` 1, `-attack`
  2, `+melee` 3, ... `+forward` 0x1b, `+back` 0x1d, `+gostand` 0x19, `+activate` 0x31,
  `-activate` 0x32, `+reload` 0x33, ... then `togglemenu`, `weapnext`, ... `leaveads` 0x4b.
- rva 0xd1810: name -> index (0 if none). Per-key bind index at 0x5300f0 (stride 0x18,
  256 keys); rva 0xd17f0 reads it; `bind` (rva 0xd13d0) writes it.
- rva 0xce450 `(localClient, index, key)`: the switch over the index. Odd index: kbutton down
  (rva 0xd0620), even: up (rva 0xd06f0); a few run actions (actionslots, stance). It returns
  early unless the client is in a level (bytes 0x5dac53 and 0x40cac8). kbutton (0x14 bytes):
  `int down[2]; int downtime; int msec; char active; char wasPressed`; the array starts at
  0x52fbc0 (`+left` 0x52fbc0, `+forward` 0x52fbe8, `+attack` 0x52fcd8, `+activate` 0x52fd3c).
- rva 0xd03a0 builds the usercmd: with key catcher 0x10 (0x533cb0) set it only does stance;
  otherwise buttons (rva 0xce200: attack 0x1, melee 0x4, Use 0x8, reload 0x10, usereload 0x20,
  prone 0x100, ...), key moves (rva 0xcf900, forward/back/strafe from kbutton state, rva
  0xcfbc0) and mouse. Key moves are skipped while the player state's flag 0x800 (0x535d44,
  frozen controls) is set.
- mw2_coop: console command `mw2coop_bind +activate` calls 0xce450 with key 0x1ff (no keyboard
  sends it); QA scripts set `mw2coop_qa_press "<player> <button> <ms> <n>"` and only the DLL of
  that player's machine (host = player 1) presses and lets go after ms.

Results: `buttons` (S.S.D.D., Whiskey Hotel) and `pickups` (rifle, pistol, frag box) all PASS on
both machines with real presses. Found with real input:
- S.S.D.D.: player 1's range place (48 units left of `player_start_range`) was 10 units further
  back than player 2's and inside the bay's back wall: traces from him were blocked to the
  sides and back, Forward moved him 0 units in five tries. Now both stand where COD-MOD puts
  its players: player 1 at -2515.2 5190.2 (67 units left
  of the start, 36 forward: bags 20 ahead, wall 23 behind), player 2 on the start (bags 42
  ahead). The bay (player traces): from 101 units left to 28 right of the start; the back wall
  slants, 7 to 25 units behind the start's line. QA `bay` checks both inside, not stuck.
- `maps\_introscreen` froze `level.player` only (S.S.D.D.: 12 s): player 2 could walk and take
  the pistol while player 1 could not. Patched (tools/gen_intro_patches.py): FreezeControls,
  DisableWeapons and EnableWeapons on `level.player` in the 12 intro functions go to every
  player. Each of them lifts its own freeze (no early return in between). QA `introfreeze`.

## Loose Ends: Shepherd's betrayal for two players (2026-10-22)

Scene (estate.gsc `ending_moments()`): at the LZ a mortar drops Roach (`birchfield_knockout`,
flag `play_ending_sequence`); Ghost drags him on rig `playerview` (anim `estate_ending_drag`,
`PlayerLinkToDelta` arcs 30) while he fights on his back with an AK; blackout; at the Pave Low
(`estate_ending_part1`) Ghost pulls him up, Shepherd draws (`pistol_pullout`), fires at him
(`shepherd_fire2`: `DoDamage(99 / damagemultiplier)` with the death shield on) and at Ghost
(`shepherd_fire1`); blackout; `estate_ending_part2`: two men throw Roach's body
(`body_ending`), then Ghost's, into a pit; gasoline, fire, `end_the_mission`. Everything is on
`level.player`. Before: player 2 stayed where he was (12,000 units away at start `ending`) and
saw nothing. COD-MOD: only freeze and death shield for both at the knockout; its attempts to link
both players to the rig are commented out.
Design: as in COD-MOD, player 2 invisible in the
cutscenes and thrown with Roach in the one throw; Shepherd's shot hits player 2 as well (he
sees from Roach's eyes, the gun points at him), then a black screen without text.
Done (tools/gen_estate_patches.py, `maps\_mw2coop_estate`): every `level.player` statement of
`ending_moments`, `ending_gasoline_sequence`, `ending_passing_out`, `ending_slowmo`,
`ending_mortarhit_ghost_sequence`, `startEnding`, `birchfield_knockout` runs for each player,
except Shepherd's hit and its rumbles, the damage watch, the breathing sound and the blur cycle's
pacing; both lose their model function (`SetModelFunc( undefined )`: Hide does not hide a co-op
player) and names; player 2 takes the same hit after Roach's, then a client black overlay
(sort -1, under the subtitles) fades in over 0.3 s and out with the toss's `restoreVision`.
QA `betrayal`: with test god mode on, the hit does nothing (health stays 100, for player 1 too):
run it with `MW2COOP_GOD=0`. The ending's death shield sends no `damage` notify; the test reads
the lowest health instead.

## UPnP on a PC with several network adapters (2026-10-22)

On a PC with several network adapters the search got no answer ("no router answered"). A plain
SSDP M-SEARCH (Python, no interface chosen) got no answer either, even for `ssdp:all`; the same
search sent from the LAN address (bound to it, `IP_MULTICAST_IF`) or straight to the gateway got
the router's answer at once. Windows sent the multicast out of another adapter. Fix in
`src/upnp.c`: the address of the route to the internet (UDP `connect` to a public address,
`getsockname`; it sends nothing) and the gateway of that route (`GetBestRoute`); the search goes
out from that address and to the gateway as well. The router then answered and took the forward
(UDP 28960). Also, starting a level dropped "hosting" for a moment and the forward was removed and
added again; removal now waits for a 10 s stop.

## No Russian: Makarov's crew look on the Spec Ops body (2026-10-22)

Player bodies must be Spec Ops co-op bodies (`coop_body_*`): an AI body such as the crew's
`body_airport_com_c` on a player gives a broken pose (the player animations need the co-op
skeleton). To dress both players as Makarov's crew, the DLL (`src/transplant.c`) puts the crew's
mesh on `coop_body_us_army` in memory: the target keeps its bones, hit locations, collision and
bounds and takes the donor's LODs (surface copies) and material list. Every vertex's bones are
renumbered by bone name; a donor bone the target lacks takes its nearest ancestor. Heads stay
separate models (the crew's own heads attach to the co-op body as the co-op heads do).

Observations (dumps with the new `!surfs <xmodel>` QA command, then frames):
- 64-bit layouts. XModel: boneNames +0x28 (script string indices, interned), parentList +0x30
  (a byte per non-root bone: its index minus its parent's), materialHandles +0x58, lodInfo[4]
  +0x60, stride 0x38 (dist +0, numsurfs +4, surfIndex +6, modelSurfs +8, partBits[6] +0x10, surfs
  +0x28). XModelSurfs: name +0, surfs +8, numsurfs +0x10, partBits +0x14. XSurface, 0x58 bytes:
  deformed +1, vertCount +2, vertInfo.vertCount[4] +0x18, vertsBlend +0x20, vertListCount +0x30,
  vertList +0x38 (0x10 each: boneOffset, vertCount, triOffset, triCount, collision tree +8),
  partBits[6] +0x40. Checked on every surface of three models: the vertex groups add up to
  vertCount.
- vertsBlend: vertCount[0] vertices with 1 bone (1 value), [1] with 2 (bone, bone, weight), [2]
  with 3 (5 values), [3] with 4 (7 values); a bone is stored as its index * 64.
- partBits: one bit per bone used, bone 0 = the top bit of word 0, without ancestors (17 of 17
  crew surfaces match that rule, none match "with ancestors").
- Crew (com_c) 81 bones, 17 surfaces in 4 LODs; ranger 90 bones, 33 surfaces. Six crew bones have
  no ranger bone of the same name (mapped to ancestors).
- The swap runs on the main thread (a console command the control thread queues), only while the
  requested map runs: asset lookups from another thread, or during the next level's zone load,
  hung the game in earlier work. A level restart finds it done (the target's material list is the
  donor's); the next level's zone load brings the original model back.
- Frames (QA `bodyview`, airport start `stairs`): both players in the crew's jacket and trousers,
  standing, stepping sideways and backing off with the player animations; no visible stretching
  at the hips, where the two skeletons' bind poses differ by up to 5 units.

Unknown: how it looks in the other LODs at distance (not looked at). Both players share one crew
body (com_c) with different crew heads; whether the loaded zones hold a second co-op body that
could take com_d is not checked.

## No Russian level tour and the squad's line of fire (2026-10-22)

QA `tour` on airport (9 start points, each with frames on both screens): 109 PASS, 0 FAIL. The
script errors came from the developer start `grigs` and from one `Kill()` under the test god mode.
That kill is the squad's line of fire: in the lobby (`intro_civilians`) and the massacre
(`massacre_killers_shotgun`) the level waits on a final trigger with `add_wait(
::trigger_wait_targetname )`, aborted by the squad's move-on flag, and calls `level.player
add_call( ::Kill )`; before that, `player_line_of_fire` fires warning MagicBullets at
`level.player`'s feet while he stands within the line's radius. The trigger fires for either
player, so player 2 walking in killed player 1, and player 2 got no warning shots. Fix (generated
by `tools/gen_airport_patches.py`): the warning goes to the player in the line, and the player who
walks into the final trigger dies (player 1 outside co-op). QA `lineoffire` (start `intro`,
`MW2COOP_GOD=0`): player 2 walks in, he dies, player 1 lives, the mission fails for both.

Frames: the `intro` start is the elevator with the mission's intro screen, black for its first
10 s, as in single player.

## God mode, downs and difficulty for two (2026-10-22, v0.4.0-beta)

- Test god mode: `god_mode_on()` was `GetDvar("mw2coop_god") != "0"`, and only the removed
  `.bat` launchers set "0": every lobby session of v0.3.0-alpha and v0.3.1-alpha ran with both
  players invulnerable. Now on only for "1" (`tools/qa_pair.sh` sets it). QA `god` both ways.
- Recruit: `maps\_gameskill` has `player_downed_buffer_time` for normal, hardened and veteran
  only. On Recruit (`easy`) `maps\_coop::downed_player_manager()` multiplied undefined on the
  first down (script error) and its thread ended: nobody went down after it and both down never
  failed the mission. Patched to Regular's 2 s. QA `downs` (forces Recruit with
  `level.forcedgameskill`): before 4 FAIL plus the error on both machines, after all PASS.
- `resetSkill()` runs on every down and revive (Spec Ops' buff for the player still up) and set
  both players' `threatbias` and `attackeraccuracy` back to the difficulty's values, undoing a
  scene's own. Both kept around the call. QA `sceneskill`: before 150 and 1 instead of 777 and
  0.123, after all PASS.
- Difficulty: the popup's buttons run `profile_difficultySave <n>` and `setdvar` on
  `g_gameskill` and `g_gameskill_player_1/2`. All three are latched (flag 2; `g_gameskill` has
  0x2000 too, and console `set`, `setdvar`, `seta` change nothing): `devmap` applies the latch,
  a co-op session starts with `coopLaunch`, so the pick never applied. The level start (rva
  `0x22e1a0`) sets `g_gameskill_player_<n>` from party member n's difficulty (rva `0x5dd988` +
  n * 0x38, each member's own profile), `GetPlayerSetting("gameskill")` reads that dvar (rva
  `0x18de0a`), and `updateGameSkill()` plays the higher of the two players'. So the host played
  what his profile loaded at startup and the client his own. Fix: the DLL hooks
  `profile_difficultySave` for the host's pick (else the difficulty it plays), sends it as
  `mw2coop_skill` with `coopLaunch`, and `updateGameSkill()` takes each player's from it in a
  co-op campaign. QA `skill` (host picks through `profile_difficultySave`, client profile on
  Regular): picks 3, 0 and 2 give the level and both players 3, 0 and 2 on both machines; the
  release DLL checked with 3.

## Level tour results (2026-10-22)

QA `tour` on two copies (host + client), god mode on, the mod's main of the day (each level run
once between 07:22 and 13:20; fixes found earlier in the day were in, later ones not). Per level:
start points (sections), checks passed and failed on the host, runtime script errors host/client,
frames host/client. dc_whitehouse (Whiskey Hotel) and ending (Museum) were toured later that day
(16:55 and 16:58).

| Level | Sections | PASS | FAIL | Script errors | Frames | Notes |
|---|---|---|---|---|---|---|
| trainer | 7 | 91 | 0 | 0/0 | 28/28 | |
| roadkill | 9 | 88 | 1 | 0/0 | 30/30 | walk check at `dismount`: 26 of 100 units (the level's slow walk) |
| cliffhanger | 15 | 162 | 21 | 4/4 | 56/56 | the walk check stepped off the cave ledge and player 2 fell forever: fixed (ledge death per player, walk only where there is floor); first two sections rerun after the fix PASS; errors from the dev-only `jump` start |
| airport | 9 | 109 | 0 | 6/6 | 34/34 | errors in `grigs_test` / `_anim::get_anim_position` (not looked into) |
| favela | 12 | 123 | 0 | 0/0 | 37/37 | |
| invasion | 17 | 201 | 0 | 0/0 | 62/62 | run after the Burger Town spot fix |
| favela_escape | 13 | 169 | 0 | 0/0 | 52/52 | |
| arcadia | 5 | 65 | 0 | 0/0 | 20/20 | |
| oilrig | 9 | 93 | 0 | 0/0 | 30/30 | surfacing start fixed afterwards |
| gulag | 13 | 139 | 4 | 1/1 | 46/46 | player 1 sent to triggers inside walls (test artifact); error: scripted kill in `player_dies_to_cavein` under god mode |
| dcburning | 10 | 116 | 0 | 0/0 | 36/36 | Blackhawk ride fixed afterwards |
| contingency | 8 | 104 | 0 | 0/0 | 32/32 | |
| dcemp | 12 | 126 | 0 | 7/7 | 40/40 | errors in the level's `emp_empaftermath` / ambient start code; partner rigs fixed afterwards |
| estate | 10 | 121 | 1 | 4/4 | 38/38 | walk check at `forest_fight` (the start's shellshock); errors in the level's start code |
| boneyard | 7 | 83 | 2 | 5/5 | 26/26 | `ride_end`: player 2 left behind, fixed (UAZ back seat); errors in the level's vehicle start code |
| af_caves | 10 | 108 | 1 | 0/0 | 33/33 | walk check at the default start: 25 of 99 units (not looked into) |
| af_chase | 13 | 39 | 0 | 0/0 | 14/14 | knife dev starts skipped |
| dc_whitehouse | 4 | 52 | 0 | 0/0 | 16/16 | |
| ending | 9 | 45 | 9 | 361714/2562846 | 16/16 | player 2 cannot walk (0 of 100 units) or is 1500-1800 units from player 1 at the exhibit starts: not fixed; the errors are the civilians' `_anim::anim_weight` storm, also in single player |

## Playtest 2026-10-23 (two PCs, Cliffhanger and No Russian)

Two people played over the internet, each on his own PC and Steam account, with a
session recorder in the scripts (`maps\_mw2coop_rec.gsc`: positions every second, buttons,
weapons, links, damage, downs and the allies, written to `mw2coop.log` through a `setdvar`
the DLL catches). Both machines' logs matched line for line (no desync). Builds were sent
during the session (v0.4.6-beta-rec1 to rec10), each fix from the last log:

| Seen in the logs | Cause | Fix |
|---|---|---|
| player 2 lost his ice picks, then fell off the wall when player 1 reached the top | the weapon sync took the picks with player 1's and gave them back mid-climb | the picks are each climber's own (`level.mw2coop_own_weapons`) |
| player 2 hung inside the rock after the big jump; later Soap caught a player who had already fallen | player 2's far-lane spot under the edge was in the rock; his hidden rig still hit `start_gaz` | player 2 uses player 1's spots across the gap; no catch without `player_hangs_on` |
| a fall put player 2 off the ground in the slope, then 40 units under the floor | the lane-shifted `player_recover`, then his lane's start | both players recover at `player_recover`; moved beside the other if on top of him |
| falls above the safe point killed | vanilla rule | in co-op any fall from the first wall goes back to the bottom; the climb uses the easy rules |
| two MacTavishes at the top | `friendly_init_cliffhanger` spawns a Soap per catch | the second catch reuses him |
| a restart at checkpoint `c4` put both at the cave | `start_c4()` places nobody (teleport commented out) | checkpoints keep where they were reached; restore at the start |
| the helicopter ending never started, the mission failed later | it waited for player 2 to get off his snowmobile | the ending starts when either player is there (as in the original) |
| No Russian never ended after a restart at `tarmac` | `escape_main()` calls `nextmission()` from the default start only | `maps\_mw2coop::ending_goes_on()` in five level endings |

Cliffhanger was finished; No Russian's ending fix was not played yet. The 12 s lobby drop seen
earlier in QA was a two-copies-on-one-PC artifact (joining `127.0.0.1` makes member addresses
loopback, so the lobby heartbeat never reaches the socket); players on two PCs are not affected.

## v0.5.0-beta (2026-10-25)

Release `v0.5.0-beta` of mw2_coop (tag on main 8e4f31c, docs 83c6cbd): zip SHA-256
`661359ebba9e8ee4a9f4d62f2a9a533a00afa341178a72dca848d1897051c4e6`, release `winmm.dll` SHA-256
`12176eeea7bf517c1901812e2f996a3dbc4175ac794c1806bc918a671d4c13fd`, built from a fresh clone of GitHub `main`.
Release DLL checked on two test copies: `spawn` on the range and the Gulag rope (`rappel`): PASS on both, the fonts load. The experimental
32-bit archive `mw2coop-v0.5.0-beta-x86.zip` (SHA-256 `146d14513c1944b21a4ac3e4c473de0801c191d87d50f6209ad37e4a69b073bf`, its DLL
`836f09112cf33694...`): `spawn` PASS on two 32-bit copies.

Observations (what ran; all by bots on two copies on one PC, nobody played these with two people):
- Scenes for both: QA `rappel` (Gulag cellblock rope: both on the rope, both 479 units down, let go, weapons in
  hand), `rappel` on af_caves, `gulagevac`, `chopperjump`, `bigjump`, `oilbird`, `invconvoy`: PASS.
- Cliffhanger: `snow`, `snowview` (frames of the rider, the partner's view of the snowmobile), `climbview` (frames of
  the climber and the ice picks), `climb` (both reach the top; Soap catches both; four runs failed at the bot's
  run-up to the big jump while one passed with the new picks: a flaky QA step, not proven).
- S.S.D.D. `course` (gates close after both are through), Team Player `seatview` (the passenger looks ahead, no
  friendly damage on the ride): PASS.

Things we learned (worth knowing for other games on this engine):
- `GetEntArray( value, "script_flag" )` returns undefined in this build: pick triggers by class and compare the field.
- A player body (multiplayer skeleton) cannot play the campaign AI animations (a snowmobile driver twists across the
  seat): make a model with an AI skeleton wearing the player's mesh. The DLL copies the AI model into the XModel
  pool and puts the co-op body's surfaces on it (the same transplant as No Russian's crew), and answers the lookup
  of a made-up model name; `DB_IsXAssetDefault` must say "not default" for that name or a reader count leaks and
  the next `playsound` hangs.
- Attach takes no angles: an upright ice pick in a fist needs a model whose bone is turned in memory.
- Debugging a hang: a thread of the DLL that writes every other thread's return addresses to a file on request
  (a file `dumpstacks` in the mod folder gives `stacks.txt`), taking no lock the game may hold.

Unverified: the click fix for the ice climb (a guess; its own commit, revert with `git revert`), the second
player's stealth in Cliffhanger (no frame), slide_sync in Escape from Favela, the other player's view of every
rope scene.

## Status

Done in v0.5.0-beta: the rope and rappel scenes for two, S.S.D.D.'s gates, Team Player's swap, Cliffhanger's
snowmobile and climb views, Russian fonts, an experimental 32-bit archive. Next: a real two-person playthrough of the new
scenes, lip sync (off on both screens), the nrend anim_loop error, the Museum for player 2, split screen.
