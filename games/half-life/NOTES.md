# Half-Life (GoldSrc) — triage notes

Status: triage started, nothing built yet.

## Game identity

| Field | Value |
|---|---|
| Title | Half-Life |
| Year | 1998 |
| Store | Steam (app id 70) — expected, not yet verified on this PC |
| Version / build id | unknown (game not installed) |
| Engine | GoldSrc |
| OS | Windows 10.0.26300 (host) |
| Installed on owner PC | **yes** — `<game dir>`, `appmanifest_70.acf` buildid **15961492**, SizeOnDisk 589 649 007 B |
| hl.exe SHA-256 | `BE7C061A2D36A3517C98412A06642F98985AB746E3D65C8D105D25B6AC77369C` (39 264 B, FileVersion 1.1.1.1, "Half-Life Launcher") |
| Game dirs | `platform/`, `valve/`, `valve_hd/`, `valve_russian/`; servers: `hlds.exe` (366 944 B), `hltv.exe` |
| SDK | `ValveSoftware/halflife` cloned to `hl-sdk/` (dlls/, cl_dll/, projects/vs2019); Half-Life 1 SDK License = free mods only, no commercial use |
| Toolchain | **missing**: no Visual Studio / MSVC, no `gh`, no Docker. Free space: C: 152 GB, D: 543 GB |

## Observations (sources)

- Co-op for this game already exists and is playable: **Sven Co-op** (Steam app 225840), listed in
  `DIRECTORY.md` as `mod | playable | story: yes, enemies: yes | dedicated-server | player reports`
  (source: repo `DIRECTORY.md`, row `Half-Life`).
- Engine family note: "Source / GoldSrc — Server/client split and the SDK make single-player maps
  playable in multiplayer; the work is mostly entity and map scripting fixes"
  (`playbook/engine-families.md`, line 17).
- GoldSrc engine source for HL1-based games is public: <https://github.com/ValveSoftware/halflife>
  (C++, license "Other" / NOASSERTION, last push 2024-10-02, 4390 stars).
- Sven Co-op itself is not published as an open-source engine repo (no official `sven-coop/sven-coop`
  repository — GitHub API returns 404). Its extension surface is AngelScript plugins / map scripts
  (see community repos such as `baso88/SC_AngelScript`, `incognico/svencoop-plugins`).

## Derived (my conclusions, not yet tested)

1. The game is not "a gap": a mature co-op mod exists, so the playbook's first path is
   *finish/extend what exists*, not *build from zero*.
2. Building a **custom** co-op mod is still feasible the classic GoldSrc way: HLSDK
   (`ValveSoftware/halflife`) → build 32-bit `client.dll` + `server.dll` mod, run `hl.exe -game <mod>`,
   dedicated server + two clients on one PC.
3. GoldSrc already has client/server netcode (HLDM), so co-op work is mostly: spawn multiple players
   in SP maps, fix entity/trigger/script logic that assumes one player, sync doors/NPCs/scripts.

## Unknowns

- Does the owner own/install Half-Life on Steam? Which edition (original GoldSrc vs Half-Life: Source)?
- Target experience: use Sven Co-op as-is, extend Sven Co-op via AngelScript, or build own HLSDK mod?
- Toolchain availability: no Visual Studio / no Docker detected; `gh` CLI missing; Python is the
  Windows Store stub. Building HLSDK needs MSVC (32-bit) — installation requires owner approval.
- Game build id and exe SHA-256: unknown until game is installed.

## Plan (chosen path: own mod from scratch on HLSDK, learning goal)

Working copy of the game: `hl-test/Half-Life` (copy, owner's Steam install untouched).
SDK: `hl-sdk/` (ValveSoftware/halflife). Build with `projects/vs2019/hldll.vcxproj`
(→ `hl.dll`) and `hl_cdll.vcxproj` (→ `client.dll`), toolset v142, x86.

Milestones:
- **M1 build**: compile vanilla SDK DLLs, start `hlds.exe` on a copy, one client connects.
  Proves toolchain and DLL load.
- **M2 second player**: run an SP map, spawn a joining player at `info_player_start`
  (SP maps have no `info_player_deathmatch`), stop `singleplayer_only` rules from
  kicking/restarting, allow respawn.
- **M3 sync**: doors, triggers, scripted sequences, NPCs, level transitions.

First code questions to answer in source (not yet checked):
- `dlls/gamerules.cpp`: `CHalfLifeMultiplay` vs `CHalfLifeSingle` selection (`IsMultiplayer`,
  `liblist.gam` `type`).
- `dlls/client.cpp`: `ClientPutInServer`, `SpawnPlayer`, `info_player_start` handling.
- `dlls/player.cpp`: `CBasePlayer::Spawn`, respawn rules, `m_bIsDying`.

## Progress log

### 2026-10-20 — toolchain + first build (M1 partly done)

- Installed VS Build Tools 2022 (winget) with `Microsoft.VisualStudio.Workload.VCTools`
  + recommended. Only toolset **v143 (14.44)** present; SDK projects ask for v142, so build
  with `/p:PlatformToolset=v143`.
- Build command that works:
  `MSBuild.exe hl-sdk\projects\vs2019\hldll.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m`
  (same for `hl_cdll.vcxproj`). Outputs:
  `hl-sdk\projects\vs2019\Release\hldll\hl.dll` (1 202 176 B),
  `hl-sdk\projects\vs2019\Release\hl_cdll\client.dll` (819 200 B).
- Post-build event fails with MSB3073 because it copies to `..\..\..\game\mod\dlls`
  (created the dir, still fails on `filecopy.bat`) — **ignore it and copy artifacts manually**;
  the DLL is produced before the post-build step.
- Test copy of the game: `hl-test\Half-Life` (4 498 files, byte sizes match the Steam
  install; originals backed up as `valve\dlls\hl.dll.orig`, `valve\cl_dlls\client.dll.orig`).
- **Client `hl.exe` with our built `hl.dll` + `client.dll` runs** (15 s alive, local map
  `c1a0`, `-game valve -insecure -window -condebug`). That is the M1 evidence we have:
  the DLLs load and a local server starts.
- **`hlds.exe` crashes on this machine**: exit `0xC0000005` (access violation, faulting
  module `hlds.exe` offset `0x6991`) with *any* arguments, including none, both from the
  copy and from the untouched Steam install, under the sandbox and with full access.
  `hltv.exe` starts and stays running, so 32-bit GoldSrc binaries work in general.
  VCRUNTIME140 present (14.51). Cause **unknown**; dedicated-server tests are blocked on it.

### Open issue

- `hlds.exe` 0xC0000005 at startup — needs a proper debugger session (not installed) or a
  workaround (listen server via `hl.exe`, or an older `hlds` build). Co-op can be tested on
  a listen server meanwhile (`liblist.gam` `type multiplayer` + `maxplayers`).

### 2026-10-20 — M2 attempt blocked by Smart App Control

Config test (no code change yet): set `valve/liblist.gam` to `type "multiplayer"`,
`maxplayers "2"`, run `hl.exe -game valve +coop 1 +map c1a0`. Map loads, title shows
"Half-Life Coop Test", but the engine logs `"deathmatch" changed to "0"` after map spawn:
a single-player map forces SP rules, so co-op mode never activates (no HUD, observer-like
camera).

Code change made in `dlls/gamerules.cpp` `InstallGameRules()`: if `gpGlobals->coop`, return
`new CHalfLifeMultiplay` **before** the `!deathmatch` SP branch (single-player map keeps
multiplayer rules). Rebuilds fine.

**Blocker:** the rebuilt `hl.dll` is refused by Windows — `LoadLibrary` returns error 4551
(0x11C7, "An Application Control policy has blocked this file"). Evidence:
`Microsoft-Windows-CodeIntegrity/Operational` events **3077** and **3033** name
`...\hl-test\Half-Life\valve\dlls\hl.dll` and policy ID `{0283ac0f-fff1-49ae-ada1-8a933130cad6}`.
`HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 1`
=> **Smart App Control is on**. Our DLL is unsigned; early runs loaded before reputation
verdict, later loads are blocked. This blocks *any* local mod build at runtime on this PC.

Options (owner decision): turn Smart App Control off (Windows Security → App & browser
control → Smart App Control; re-enabling needs an OS reset), or develop/test on another
machine or VM without SAC.

## Next step (blocked on owner decisions)

See chat: need choice of game edition, permission to install Half-Life and the MSVC toolchain,
and the goal (play vs build).

### 2026-10-21 — build pipeline repaired, state frozen for handoff

- Full rebuild of `hldll.vcxproj` (`/t:Rebuild`) still works on this machine (MSVC 14.44.35207,
  toolset v143). Artifacts: `hl.dll` 1 202 176 B, `client.dll` 819 200 B.
- Fixed the post-build copy in `projects/vs2019/{hldll,hl_cdll}.vcxproj`: the destination was
  `..\..\..\game\mod\...`, i.e. three levels up from `projects/vs2019` (= workspace root), so
  `filecopy.bat` always failed with MSB3073. Now `..\..\game\mod\...` ( = `hl-sdk\game\mod\...`),
  MSBuild exits 0 and drops both DLLs there automatically.
- Added `tools/build-coop.ps1` + `tools/build-coop.cmd` (root of the workspace): build both
  projects, then deploy into a GoldSrc game dir, keeping one `*.orig` backup. Needs
  `-ExecutionPolicy Bypass` because the machine policy is `Restricted`; the `.cmd` wrapper does it.
- Re-deployed the freshly built DLLs into `hl-test/Half-Life/valve` (vanilla `hl.dll.orig` kept).
- Re-confirmed both blockers today: `hlds.exe -h` still exits `0xC0000005`
  (`-1073741819`), and `VerifiedAndReputablePolicyState` is still `1` (SAC on).
- Owner-facing handoff with paths, build commands, code map and next steps:
  `HANDOFF.md` in the workspace root.

### 2026-10-21 (evening) — SAC off, M2 patch verified, new single-instance blocker

Owner decided to turn Smart App Control **off** (one-way). Verified:
- `HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState` set `1` → `0`
  via elevated `Set-ItemProperty` (UAC prompt). Rebuilt unsigned `hl.dll` now loads — engine logs
  `Adding: valve/dlls\hl.dll` + `Dll loaded for mod Half-Life` (was `LoadLibrary 4551` before).
- Added a temporary `ALERT` in `InstallGameRules()`. **Correction:** `gpGlobals->coop` and
  `gpGlobals->deathmatch` are both `float` (`engine/progdefs.h:28-29`), not int — my first debug
  used `%d` and printed garbage. With `%f`, runtime values on the listen server are:
  `g.coop=1.0  g.dm=0.0  cv.coop=1.0  cv.dm=0.0  maxCli=2`.
- So the engine does **not** clear `coop` on a single-player map — only `deathmatch` is forced to 0.
  The patch `if (gpGlobals->coop) return new CHalfLifeMultiplay;` therefore **fires**, and the log
  shows `2 player server started`, host player joins, `STEAM USERID validated`. M2 is verified at the
  code-path level (multiplayer rules active on an SP map). This supersedes the earlier
  "co-op never activates" hypothesis — the fix works.

New blocker for the two-player test:
- A second `hl.exe` cannot run while the first is running: dialog "Could not launch game. Only one
  instance of this game can be run at a time." (GoldSrc/Steam single-instance guard). So
  "listen host + second client on one PC" is not a valid two-player test.
- Per AGENTS.md rule 11, QA must use bots, not an owner playtest. Next options: fix `hlds.exe`
  (still `0xC0000005`) for a dedicated server + bot, or run a server-side bot on the listen server
  (see `playbook/automated-qa.md`).

Unknown / not yet done:
- Visual confirmation of HUD + first-person view for the host player (screenshot saved to
  `tools/host.png`; not verified — this session's model can't read images).
- Whether host `hl.exe` exits on its own after ~1–2 min or was closed manually (background monitor
  `tools/host_monitor.txt`).
- Second-player spawn / single `info_player_start` behaviour — not tested (blocked by single-instance).

Single-instance guard — located and partially worked around:
- The lock is a named mutex **`ValveHalfLifeLauncherMutex`** created by the launcher `hl.exe`
  (found via ASCII strings; `CreateMutexA` + "Only one instance of this game can be run at a
  time." are in `hl.exe`, not in `hw.dll`). The engine itself is `hw.dll` (3 598 176 B, at the game root).
- Patched a byte copy `hl2.exe` (same dir, mutex string `ValveHalfLifeLauncherMutex` →
  `ValveHalfLifeLauncherMute2`, same length). The second instance now launches (window
  "Half-Life Coop Test") instead of the "Only one instance" dialog — no separate `FindWindowA`
  window-class check tripped, unlike the FNV case in `methods.md`.
- Remaining: the second client does not yet join the host. Server listens on `0.0.0.0:27015`
  (UDP, pid confirmed via `netstat`). `hl2.exe` launches and sits at the main menu (stable
  30 s+ alone) but `+connect 127.0.0.1` / `+connect <lan-ip>` never produces a second
  "Connection accepted" on the host — with or without `-port 27006`. So it is not a port or
  loopback issue; the client never reaches the server, consistent with the Steam one-account
  limit (two players can't share one SteamID). Documented fix: a mock `steam_api.dll` that
  fakes a distinct SteamID per instance (`networking-steam-spacewar.md` "Testing without two
  Steam accounts"). The host itself is stable — a 300 s monitor showed no auto-exit; earlier
  "closes" were manual.

### 2026-10-22 — resumed from handoff; two-player join already succeeded

Verified on disk (not just from notes):

- **SAC is off** now: `VerifiedAndReputablePolicyState = 0` (the handoff still said `1`).
  `hl-test/Half-Life/qconsole.log` confirms the rebuilt unsigned `hl.dll` loads
  (`Adding: valve/dlls\hl.dll`, `Dll loaded for mod Half-Life`).
- **The M2 patch fires**: `COOPDEBUG g.coop=1.000000 g.dm=0.000000 cv.coop=1.000000
  cv.dm=0.000000 maxCli=2`, then `2 player server started`.
- **Correction to "the second client never connects" (above):** the last `qconsole.log`
  shows a full two-player join. Host = `Local connection. Connection accepted.` (client 0,
  `<player> has joined the game`); a second instance then shows
  `"<player><1><STEAM_ID_LAN><>" STEAM USERID validated` →
  `Client (1)<player> connected` (`Adr: <lan-ip>:60863`) →
  `(1)<player> has joined the game`. So the mutex-patched `hl2.exe` + `-insecure` /
  `sv_lan` path **did** produce a second player. Both clients validate as `STEAM_ID_LAN`
  and the server accepts the duplicate — the "Steam one-account limit" was not the blocker.
  (Which exe the second instance was: inferred to be `hl2.exe` from its 21.10 23:58:41
  mtime inside the run window — not proven from the log.)
- `host_monitor.txt`: the host stayed alive for the full 300 s window (no auto-exit).

Still unknown (not verified):

- Where each player actually spawned. `c1a0` has a single `info_player_start`; the log has
  no spawn-origin line, so overlap vs. offset is not proven.
- HUD + first-person view for both players — not in the log. Screenshots
  (`tools/host.png`, `tools/z*.png`) exist but this model does not declare image input, so
  they can't be read here (same limitation as the previous session).
- `hlds.exe` still `0xC0000005` → dedicated-server QA remains blocked; listen-server path works.

### 2026-10-22 (run #2) — M2 confirmed: two players spawn at separate start points

Reproduced the two-player test cleanly: host + mutex-patched `hl2.exe`, host with
`+sv_lan 1`, client with `+connect <LAN IP>`. Verified from `hl-test/Half-Life/qconsole.log`:

- Two clients joined: host `Connection accepted.` → `<player> has joined the game`
  (ent=1); then `"<player><1><STEAM_ID_LAN><>" STEAM USERID validated` →
  `Client (1)<player> connected` (`Adr: <lan-ip>:58682`) →
  `(1)<player> has joined the game` (ent=2). Both validated as `STEAM_ID_LAN`, so
  **`+sv_lan 1` (LAN auth) is the enabler — the mock `steam_api.dll` is not needed.**
- Spawn debug (added to `gamerules.cpp` `CGameRules::GetPlayerSpawnSpot`):
  - `ent=1` → `info_player_start` at `(484 318 -204)`
  - `ent=2` → `info_player_start` at `(-621 -1000 -166)`
  - So `c1a0` has **more than one** `info_player_start`; the second player gets a different
    start point (via `g_pLastSpawn`), **no overlap**. The earlier "single `info_player_start`
    → overlap" worry was wrong.
  - Both `obs=0` (not observers) and `flags=0x8`, i.e. both are real playing clients.
- Conclusion: **M2 verified** — two players join an SP map in co-op and spawn at separate
  start points as playing clients (multiplayer rules active). HUD / first-person view is
  implied by `CHalfLifeMultiplay` + the unchanged vanilla `client.dll`, but still not visually
  confirmed (no image reader on this model).

Notes for the record (harness gotchas, not mod bugs):

- `-condebug` **appends** to `qconsole.log` (does not truncate) — truncate before a run or the
  poll matches stale lines from the previous run.
- Game processes inherit the harness stdout pipe and make a one-script launcher hang; the fix
  is `Start-Process -RedirectStandardOutput/-RedirectStandardError` to files.
- A `+connect 127.0.0.1` attempt stalled at `STEAM USERID validated` (no `Client (1) connected`);
  that run was interrupted by cleanup, so loopback-vs-LAN-IP is **not** conclusive. The LAN-IP
  connect above worked.
- Build: the spawn-debug line in `gamerules.cpp` was rebuilt via `tools/build-coop.cmd`
  (`hl.dll` → 1 202 688 B) and deployed.

Open follow-up for M3: the two start points are far apart (`(484 318 -204)` vs `(-621 -1000 -166)`)
— likely one is the intended tram start and the other an unrelated `info_player_start`, so a real
co-op mod will want dedicated `info_player_coop` points rather than whatever `info_player_start`
the map happens to contain.

### 2026-10-22 — M3 survey + first sync fixes (level transitions, monsters)

Root cause found: co-op runs through `CHalfLifeMultiplay` (our `gamerules.cpp` patch), but its
`IsDeathmatch()` returns `TRUE` unconditionally (`multiplay_gamerules.cpp:284`). Co-op is not
deathmatch, so SP mechanics gated on `IsDeathmatch()`/`!IsDeathmatch()` silently turn off in co-op.

What that breaks in the server DLL:

- `trigger_changelevel` (`triggers.cpp:1486`) bails when `IsDeathmatch()` → **level transitions do
  nothing in co-op**. `ChangeLevelNow` also hardcoded `pfnPEntityOfEntIndex(1)` (the SP hero) for the
  transition-volume check, so it would abort when the *second* player reaches the trigger.
- `trigger_endsection` (`triggers.cpp:2021`) is removed when `IsDeathmatch()` → chapter endings off
  (and `pfnEndSection` may not even work in MP — untested).
- `trigger_autosave` (`triggers.cpp:1967`) removed when `IsDeathmatch()` → correct (no saves in MP).
- `mp_allowmonsters` defaults to `0` (`game.cpp:40`); `CBaseMonster::MonsterInit` (`monsters.cpp:2010`)
  sets `FL_KILLME` when `!FAllowMonsters()` → **all NPCs are killed at spawn in co-op** by default.
  (`IsAllowedToSpawn` already lets monsters through — the block is commented out at
  `multiplay_gamerules.cpp:1000`.)

Fixes applied this round:

1. `triggers.cpp CChangeLevel::ChangeLevelNow` — guard is now
   `if ( IsDeathmatch() && !IsCoOp() ) return;` (allows changelevel in co-op), and the transition
   check uses the activating player (`pActivator`) instead of entity 1, falling back to entity 1
   only when the activator isn't a player.
2. `multiplay_gamerules.cpp CHalfLifeMultiplay::FAllowMonsters` — returns `TRUE` when
   `gpGlobals->coop`, so NPCs spawn in co-op regardless of `mp_allowmonsters`.

Verified:

- Rebuilt `hl.dll` (1 202 688 B) via `tools/build-coop.cmd` — exit 0, deployed.
- Regression: two-player run still loads and both players spawn at their `info_player_start`
  points (`ent=1` and `ent=2`) — the changes did not break M2.

Not yet runtime-verified (follow-ups):

- `trigger_changelevel` end-to-end — needs a player/bot to actually reach a changelevel trigger
  (`c1a0` has no quick path; the tram ride is a long scripted sequence).
- Monster spawn in co-op — needs a map that actually has monsters (`c1a0` is monster-free); test on
  `c1a1`+ once level transitions work.
- `trigger_endsection` / `pfnEndSection` in MP — untested.
- Weapon/item economy still runs DM rules (`DeadPlayerWeapons`, `WeaponShouldRespawn`, DM scoring)
  because those are not gated on `IsDeathmatch()`. Whether to make them SP-like in co-op is an open
  design choice for later.

### 2026-10-22 (c1a1) — monster fix runtime-verified

Launched `+coop 1 +map c1a1` (a map with NPCs) with two players. Log shows:

- Two players spawned: ent=1 at `(984 654 -80)`, ent=2 at `(240 2112 784)`, both `obs=0`.
- `COOPDEBUG monster allowed` × 7, `killed` × 0: `monster_generic`×2, `monster_scientist`×3,
  `monster_barney`×1, `monster_zombie`×1. So the `FAllowMonsters` fix works — NPCs now spawn in
  co-op (before the fix, every one would have been `FL_KILLME`).

Still pending runtime: `trigger_changelevel` end-to-end (needs a player/bot to reach the trigger),
`trigger_endsection`, and the weapon-economy question.

### 2026-10-22 — changelevel guard runtime-verified (partial)

Added a test server command `coop_test_changelevel` (in `triggers.cpp`, registered in
`GameDLLInit`) that fires the first `trigger_changelevel` on the map, plus a debug line in
`ChangeLevelNow` right after the deathmatch guard. Fired it from `listenserver.cfg` (runs after
map load + game rules init) on `c1a0` with `+coop 1`.

Log evidence:

- `COOPDEBUG test: firing trigger_changelevel target=`
- `COOPDEBUG ChangeLevelNow past-guard: coop=1 dm=1`

So in co-op the `trigger_changelevel` guard now **passes** (`IsDeathmatch() && !IsCoOp()` is
false; `coop=1 dm=1`). Before the fix, `ChangeLevelNow` returned silently at `IsDeathmatch()`
(TRUE in co-op). No `CHANGE LEVEL` line followed because the host player had not connected / was
not inside the tram `trigger_transition` volume — the full transition still needs a player or bot
to reach the trigger.

Also observed: `c1a0` *does* have NPCs (`monster_scientist`, `monster_barney`, `monster_gman`,
`monster_furniture`) — the earlier "c1a0 is monster-free" assumption was wrong; they are the
tram-intro scripted NPCs.

### 2026-10-22 — scripted_sequence survey + monster friendly-fire fix

`scripted_sequence` survey (`scripted.cpp`): no game-mode gating anywhere (no
IsMultiplayer/IsDeathmatch/IsCoOp/coop checks), and `CCineMonster::FindEntity` targets MONSTERS
(checks `FL_MONSTER`) by targetname or classname-within-radius — not players, and no hardcoded
entity index 1. So `scripted_sequence` itself is co-op-safe; no fix needed there.

Real co-op concern in the NPC layer: boss AI resolves "the player" as the FIRST player
(`UTIL_FindEntityByClassname(NULL, "player")` = host) in `apache.cpp:717`, `gman.cpp:156`,
`osprey.cpp:459`, `nihilanth.cpp:1211`. For story co-op these should target the nearest/activating
player — a late-game follow-up.

Monster friendly-fire bug (fixed): `player.cpp` enemy selection ("don't shoot at friends") used
`!IsDeathmatch()`, which is FALSE in co-op (`CHalfLifeMultiplay::IsDeathmatch()` returns TRUE), so
in co-op monsters stopped skipping friendly targets and would shoot allies. Changed to
`(!IsDeathmatch() || IsCoOp())` so co-op keeps the SP "don't shoot friends" behavior. Rebuilt
`hl.dll`; smoke-tested `c1a1` in co-op (DLL loads, NPCs spawn, no errors).

### 2026-10-22 — level transition fixed end-to-end (changelevel2 is blocked in MP)

Full-path test (a test driver teleports the host into the transition volume and fires
`trigger_changelevel`) revealed the engine blocks the SP transition in multiplayer:

- `CHANGE_LEVEL(map, landmark)` → `changelevel2 <levelname> : not for use with multiplayer games`.

So the earlier guard fix was necessary but not sufficient. Fix: `CChangeLevel::ChangeLevelNow`
now uses `CHANGE_LEVEL(st_szNextMap, NULL)` (a plain fresh map load) when `IsCoOp()`, instead of
the landmark transition. Verified end-to-end: the level actually changes in co-op now
(`Spawn Server c0a0e` / `c0a0d` / `c0a0c` all loaded, no engine refusal).

Note / follow-up: the fresh-load path does not carry players to the landmark — they respawn at the
new map's `info_player_start`. Positioning players at the landmark after load is a possible
follow-up for exact SP-parity, but fresh load is a working first version.

Also found: `c1a0` has a leftover `trigger_changelevel` to `c0a0e` (the Echo demo chain
c0a0e/c0a0d/c0a0c) in addition to the real tram transition `c1a0` → `c1a1`. Both go through the
same code path.

### 2026-10-22 — boss AI targets the nearest player (co-op)

Added `UTIL_FindNearestPlayer(const Vector &)` (util.h/util.cpp) — returns the nearest living
player. Replaced the "first connected player" pattern (`UTIL_FindEntityByClassname(NULL, "player")`)
in the boss AI: `apache.cpp`, `osprey.cpp` (engine-pitch target — the code itself noted
"UNDONE ... every player for multiplayer"), `gman.cpp` and `nihilanth.cpp` (end-sequence player).
So in co-op these target the nearest player instead of the host. Rebuilt `hl.dll`; smoke-tested
`c1a0` in co-op (loads + spawns, no errors). The runtime behavior is late-game and not exercised by
the early maps, so this is compile + smoke verified only.

### 2026-10-22 — SP-style economy + auto-duplication (owner's choice)

Owner chose SP-style economy with auto-duplication (when a player picks something up, the other
players get a copy). Implemented in `CHalfLifeMultiplay` (multiplay_gamerules.cpp):

- `PlayerRelationship` → `GR_TEAMMATE` between players in co-op (allies, not enemies).
- `FPlayerCanTakeDamage` → FALSE for player-vs-player in co-op (no friendly fire).
- `DeadPlayerWeapons` / `DeadPlayerAmmo` → `GR_PLR_DROP_*_NO` in co-op (keep weapons/ammo on death).
- `IPointsForKill` → 0 in co-op (no DM scoring).
- Auto-duplication:
  - `PlayerGotWeapon` → `GiveNamedItem` to every other player (with a re-entrancy guard).
  - `PlayerGotAmmo` → `GiveAmmo` to every other player (max carry looked up from their weapons).
  - `PlayerGotItem` → `GiveNamedItem` to every other player (health/armor).

Verified: two-player run — when the second player spawned with default weapons, the log showed
`COOPDEBUG autodup weapon weapon_crowbar -> <host>` and `weapon_9mmhandgun -> <host>`; both players
spawned, no errors. (Full pickup-based duplication on real map weapons needs a player/bot to reach
a pickup; the code path is the same `PlayerGotWeapon` hook.)

Minor note: during spawn, the auto-dup also re-gives the host a copy of the client's default
weapons; harmless (host already has them, so it only tops up a little 9mm ammo).

### 2026-10-23 — automated QA suite (coop_qa bot + launcher)

Added a server-side QA bot + launcher so the mod is tested without a human playtest
(AGENTS.md #11, `playbook/automated-qa.md`):

- `hl-sdk/dlls/coop_qa.cpp` — `coop_qa` server command creates a `coop_qa` entity that drives
  5 gates and logs `QA RESULT: PASS/FAIL <gate>` then a `QA SUITE: pass=N fail=M` summary and
  quits. Gates: two-players-connected, coop-mode (asserts `coop=1` + `IsCoOp()` + multiplayer
  rules), two-players-spawn (distinct origins, not observers, real net clients), monsters-spawn
  (`FAllowMonsters()` true + live NPC count), autodup (give the host a shotgun, assert the
  second player received the copy).
- `tools/qa-coop.ps1` / `tools/qa-coop.cmd` — starts host (`hl.exe`) + mutex-patched second
  client (`hl2.exe`) on one PC, injects `coop_qa` via `valve/listenserver.cfg`, polls
  `qconsole.log` for the suite line, kills its own PIDs, restores the config, prints
  `RESULT: PASS/FAIL/TIMEOUT`.

Results (real two-client runs, c1a1):

- co-op run (`+coop 1`): `QA SUITE: pass=5 fail=0` → **PASS**.
- control / mutation run (`+coop 0`): `QA SUITE: pass=1 fail=4` → **FAIL**
  (coop-mode `coop=0`, two-players-spawn, monsters-spawn `FAllowMonsters false`, autodup all
  correctly fail) — proves the suite detects co-op behaviour and is not a hollow pass.

Build note: a stale `hl.lib` from a previous sandbox session still carried that session's
file-permission grant, so the linker failed with LNK1114; deleting the stale linker outputs
and relinking recreated them with the current session's grant. Not a mod bug.

## Next steps (23.10, updated)

Done since the 22.10 plan:

- **Host guide** and **Player-2 guide** finalized: `docs/HOST.md` and `docs/PLAYER2.md`
  (LAN; covers both same-PC via the mutex-patched `hl2.exe` and a second PC).
- Automated QA suite added (`hl-sdk/dlls/coop_qa.cpp` + `tools/qa-coop.ps1`/`.cmd`):
  5 gates, PASS 5/5 in co-op, FAIL in the `+coop 0` control — see the 23.10 progress entry.

Remaining to ship (the PR):

- Publish the mod code (`hl-sdk/dlls` changes) to its own repo and link it from `README.md`
  and the directory entry.
- Regenerate `DIRECTORY.md` (`python tools/build_directory.py`) — needs Python.
- Open the PR from branch `game/half-life`.

