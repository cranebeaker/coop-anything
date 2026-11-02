# Tony Hawk's Underground 2 (PC): story co-op notes

Goal: play the Story mode campaign together online (two players, internet).
Phases done: 0-7 (2026-10-24 to 2026-10-26). The mod: https://github.com/LeftGamer1/thug2-coop; write-up in
[README.md](README.md). Early sections below are kept as written at the time.

## Game identity (phase 0)

| | |
|---|---|
| Game | Tony Hawk's Underground 2, PC, 2004, Neversoft / Beenox port, English |
| Store | unknown (disc install, no-CD exe; no Steam/GOG release exists) |
| Engine | Neversoft engine (THPS4/THUG lineage), QB (QScript) bytecode scripts, `.prx` archives |
| OS | Windows (Windows) |
| `Game\THUG2.exe` | 2,695,168 bytes, SHA-256 `52fc88849654b34839ec2f96bff3a8c0b7a855df9a207aab9f2fca2e6bd440f3`, CRC32 `88af0474`, PE timestamp 1095202195 (2004-09-14) |
| `Game\THEMOD 1.3.exe` | same size and section layout, 123 bytes differ from THUG2.exe; SHA-256 `2b1ac8a9282c7817d6999967d0d47bfabaf8b29ac220bbff9f634e107f1095c3`. Not THEMOD's original: see "THEMOD's installer" at the end |
| `THEMOD 1.3.exe` (install root) | 2,224,553 bytes, SHA-256 `d3de1fa1a51fdd6b9487321f7e0a7ea99992692e4e738cbc0b875604188fa292`; THEMOD's NSIS installer (listed with 7-Zip 2026-10-26, not run) |
| Scripts | shipped as loose compiled `.qb` under `Data\scripts` (223 files, 2004 dates) plus per-level `*_scripts.prx` in `Data\pre` |

### Observations

- Both exes are unwrapped (plain `.text/.rdata/.data/.tls/.rsrc`, no SafeDisc sections). Source: PE header parse.
- Neither exe is a build PartyMod's patcher accepts: it wants CRC32 `a8142d3a`
  (https://github.com/PARTYMANX/partymod-thug2/blob/84908a7/patcher/main.c#L18).
- Neither exe's SHA-256 is in thug2-decomp's known-exe list
  (https://github.com/CanuckAussieKev/thug2-decomp/blob/07bd387/tools/game/install_manifest.ts#L28).
- The game ships online modes (GameSpy, now dead). Net game modes in `gamemodes/gamemode.qb` include
  `mode_netgoalattack` ("Goal Attack").
- `game/goalattack.qb` has team-aware events `goal_attack_completed_goal_other_same_team`,
  `goal_attack_started_goal_other_same_team`, texts "%n Completed %t! %i to go!", "Complete all selected goals",
  and goal-set options `classic_only` / `created_only` (net options menu: "Classic Goals"). Source: own QB name/string
  scan (`local/tools/qbnames.py`, read-only).

### Derived

- THUG2 already has an online mode where several players do level goals in the same session with teams. It is
  limited to Classic-mode goals and created goals; Story-mode goals are not offered.
- Path per triage table: "Engine has MP plus script source" -> make Story logic network-aware, largely in QB, with a
  native DLL for what QB can't reach.

### Guesses (unverified)

- Team Goal Attack may already credit a goal to the whole team (the "other_same_team" events suggest it).
- THEMOD's exe changes are small (MOTD URL and a few code bytes); its real content is likely in `Data` (scripts/pre).
- The tester's no-CD exe may have the same code layout as THUG2PM.exe (as the 2024 repack's does), which would let
  thug2-decomp's DLL run on it.

### Unknown

- What THEMOD 1.3 changes in Data (no list of its files; no source found).
- Whether OpenSpy lobbies work with this exe (stock exe points at GameSpy hosts).
- How Story mode progression (cutscenes, level unlocks, chapter flags, saves) is driven; Goals & career native
  code is ~1% decompiled in thug2-decomp.

## Prior work (phase 1)

Searches run 2026-10-24: web ("THUG2 story mode co-op mod online", "THUG Pro story mode multiplayer goals co-op",
"Tony Hawk Underground 2 story mode multiplayer mod ... career online", Russian "тони хоук андеграунд 2 кооператив
сюжет мод", "THUG2 THEMOD 1.3 thmods"), GitHub repo search (thug2, thug2 qb, neversoft qb, qb thps, partymod,
thug2 coop, thug2 multiplayer story), this repo's issues/PRs (thug, tony hawk, neversoft: 0 hits).

**No story co-op project found** by these searches. Not yet checked: THPSX forum, THUG Pro / PartyMod Discords,
ModDB file pages, archive.org.

| Project | Link | License | Last activity | What it does for us |
|---|---|---|---|---|
| thug2-decomp (CanuckAussieKev) | https://github.com/CanuckAussieKev/thug2-decomp | none stated (no LICENSE file) | commit 07bd387, 2026-10-24 | C++ reimplementation as a `dinput8.dll` hook, 73.2% of game code by size. Net library 100%, online modes 99%, script runtime 100%, Goals & career 1%. Has a test-automation channel (`run_game.ps1 -Commands`, `[features] automation=1`), level boot switches, OpenSpy host names. Requires THUG2PM.exe or a known no-CD exe. Needs VS 2026, Node 24, Ghidra 12.1.4. |
| PARTYMOD for THUG2 (PARTYMANX) | https://github.com/PARTYMANX/partymod-thug2 | MIT | commit 84908a7, 2026-06-06 | Fixes, SDL2 input, OpenSpy online. Patcher accepts one exe (CRC32 a8142d3a). |
| THUG Pro (Morten1337, Quazz, Gone) | https://en.wikipedia.org/wiki/THUG_Pro | closed | last release Oct 2023 (Wikipedia) | Online total conversion on THUG2; no campaign, so no story co-op. |
| NeverScript (byxor) | https://github.com/byxor/NeverScript | none checked | 2023-05-01 | QB compiler/decompiler (Go). A newer copy lives in thug2-decomp/tools/neverscript. |
| QBReader (kennythps) | https://github.com/kennythps/QBReader | none checked | 2015-11-06 | QB to text. |
| thug-pro-disassembly (byxor) | https://github.com/byxor/thug-pro-disassembly | none checked | 2020-06-02 | Annotated THUG Pro disassembly. |
| qcc (donnaken15) | https://github.com/donnaken15/qcc | none checked | 2026-01-05 | QB compiler, in progress. |

Official game: the Story mode's "team" (your skater plus a pro partner) is single-player character switching,
not co-op (ModDB game page wording; not a mod).

## Next (phase 2-3 proposal)

1. Decompile all QB (needs a QB decompiler built: NeverScript in Docker; agent build was blocked pending approval).
2. Diff THEMOD's Data against stock data to learn what THEMOD changes (needs a stock data reference).
3. Check whether the tester's exe matches THUG2PM.exe's layout (PartyMod's bps against a matching input, or byte
   compare of known function addresses from thug2-decomp).
4. Map Story progression in QB: `gameflow.qb`, `goal_utilities.qb`, `cutscene.qb`, level `*_scripts.prx`.
5. Prototype: unlock Story goals in net Goal Attack (team), two clients on one PC over loopback.

## Phase 2-3 (2026-10-24): scripts and the goal filter

Tools (all read-only on the install): NeverScript (thug2-decomp's copy, violetvandal fork at 73fe0f9) built in
Docker `golang:1.22.5`; own Python helpers in `local/tools` (PRE v3 extractor with the LZSS decoder from
thug2-decomp `src/game/file/pre.cpp`, QB name-table scanner, checksum->name filler, capstone 5.0.3 exe search).

### Observations

- The stock exe loads `scripts\qdir.txt` from `qb_scripts.pre`; THEMOD's exe loads `thethug2mod.pre` instead
  (exe string). `Data\pre\thethug2mod.prx` (2007-08-09, 862,422 bytes) holds 209 `.qb` + `qdir.txt`: a full
  replacement of `qb_scripts.prx`. The loose `Data\scripts\*.qb` are the 2004 originals.
- THEMOD's `.qb` have their checksum-name tables stripped; NeverScript decompiles 206 of 209 with `-lenient`
  (fails: `game.qb`, `menu/themod.qb`, `skater/walltricks.qb`). 104 checksums stay unresolved after filling names
  from stock scripts + exe strings. Note: NeverScript prints `#xxxxxxxx` in file byte order (little-endian
  reversed).
- THEMOD's script changes (lines differing, case-insensitive): CAS (`cas_skater_*`, `cas_logos`, `decks`),
  `player_controlled_vehicle`, `skater/physics`, `bails`, menus, `chapter_info`, `Levels`, `cutscene` (27),
  `gamemode` (37). `goalattack.qb` only differs by a restored printf. Goal and net scripts are close to stock.
- Story goals are gated in each level's script, e.g. `levels/BO/BO_scripts.qb` `script BO_Goals`:
  `if ((GameModeEquals is_classic) or (InNetGame))` -> classic goals, else `if ! InNetGame` -> story goals
  (`BO_AddGoal_FindJesse`, ...).
- `GoalManager_AddGoalChoices` (THEMOD exe 0x0055D590) reads flags `selected_only`/`classic_only`/`created_only`
  and calls 0x0055C2F0, which for each goal skips goals with the `net` param (0x00554540) and, with
  `classic_only`, skips goals with `edited_goal` (0x00554300); `created_only` keeps only `edited_goal` ones;
  `selected_only` keeps goals with bit 4 of byte +0x6C. No check for `classic_mode_goal`.
- The exe has direct-IP join (`TryJoinServerIP`, `join_ip`, `join_private_ip`). `CreateMutexA` is imported;
  thug2-decomp uses it as a frame mutex, and its docs describe testing net games with a second instance.

### Derived

- "Classic Goals" in net Goal Attack means "every non-created goal the level added". If the level script adds
  story goals in a net game, Goal Attack should offer them with no exe change.
- First prototype is pure QB: change the `InNetGame` gate in each level's `<LV>_Goals` script (shipped as a
  repacked `<LV>_scripts.prx`) and keep THEMOD's `thethug2mod.prx`.
- Internet play without GameSpy: host + direct IP join (port forward or a virtual LAN such as ZeroTier/Radmin).

### Unknown (needs a game run)

- Whether story goals run in a net game: peds, cutscenes, goal-start NPC dialogs, pro-skater swaps, timers.
- Whether team Goal Attack credits a goal to all team members.
- Story progression (chapter unlocks, level unlocks, save) outside career mode: not available in a net game; would
  need a host-side career save update.
- Whether two instances run on one PC with this exe.

## Phase 5 prototype run 1 (2026-10-24)

Test exe `Game\THUG2_coop_test.exe` = THEMOD exe + 4 patches (file offset == VA - 0x400000 for .text):
- 0x004D8720: `89 1D` -> `89 05` (D3D present params Windowed = 1; same byte PartyMod writes)
- 0x005F58D3: `jne` (6 bytes) -> NOPs (window creation takes the bordered-window path instead of the
  full-screen WS_POPUP|WS_EX_TOPMOST path)
- 0x004E270B: `je` -> `jmp` (skip the FindWindowExA "THUG2" already-running check)
- 0x004E1705: WM_ACTIVATE handler -> `mov byte [0x7CCBE4],1; xor eax,eax; ret 10h` (stay active, never minimise)

Mod: `Data\pre\BO_scripts.prx` repacked with `levels\BO\BO_scripts.qb` changed so `BO_Goals` adds the story
goals (not the classic ones) whenever `InNetGame`.

Driving: `local/tools/gamectl.ps1` (SendInput/keybd_event, window capture) + `scenario_goalattack.sh`.
Command line: host `-HOST -NAME P1 -HOSTNAME coop`, client `-CONNECT 127.0.0.1 -NAME P2` (parsed by the exe at
0x004E1220; both boot straight to skater select, host then gets HOST OPTIONS with Level: Boston).

### Observations (this run)
- Two instances ran side by side windowed (640x480 client) at 60 fps; the client joined the host over loopback.
- Host pause > START GAME > Game Type: Goal Attack > Choose Goals listed Boston's story goals: Find the Special
  Guest, Find Ben Franklin, Mark Your Territory, Hospital Duty, Arcade Machine High Score, Dance Sucka,
  Decapitate the Statues, Combo on the RV Ramp, Combo the 4 Benches, Ye Olde Cannon Grind, ... (screenshot
  `r2_choose_goals.png`, local).
- With the first patch (story goals only when `GameModeEquals is_goal_attack`) the list showed classic goals:
  goals are added once at level load, and the level loads in the net lobby mode.
- Goal Attack started for both players (timer shown, both on the player list).
- Not yet tested: starting or completing a story goal in the net game, team credit, peds, cutscenes.

## Pivot: story-mode co-op DLL (2026-10-24)

Goal: the real Story mode for both players (NPC quests, cutscenes). Approach: own `dinput8.dll` proxy
(repo `thug2-coop`, MinGW i686 in Docker) + `scripts\coop\coop.qb` appended to THEMOD's `thethug2mod.prx` (qdir.txt).

### Observations (runs on one PC, two windowed THEMOD instances)
- Single player is already a local server + client (`setservermode on; StartServer; JoinServer`). `SetNetworkMode
  LAN_MODE` before StartServer makes the server non-local (GameNet::Manager::InNetGame 0x0044E4F0 true).
- Script-visible `InNetGame` (0x005A0D80) / `InMultiplayerGame` (0x005A11B0) are 2-instruction thunks; hooked to
  return false in co-op, so level scripts take the single-player paths.
- Native content gates also need it: PreloadPedestrians (call 0x005AB321), Skate::SkipNode (call 0x00565BC8).
- Joining mid-game: OkToJoin's GameInProgress (call 0x0052F672) and HandleJoinRequest's (call 0x0052FB4B, else the
  joiner becomes a pending observer: "spectator with a weird camera") answer "netlobby" on the co-op host.
- Career mode has max_players=1: no second skater heap -> host crash in Calloc when the client's skater is made.
  Fixed by `cleanup preserve_skaters; chosen_leave_server; FlushDeadObjects; InitSkaterHeaps` (the two-player start
  order; cleanup after LeaveServer crashes in CSkater::SetUpNetwork) with host option players=2 (0x0067EFD0), and the
  level cleanup's DeleteExtraSkaterHeaps (call 0x005A0017) skipped in co-op.
- A network skater has no StatsManager component: Script::CallMemberFunction 0x00472980 crashed on a null component
  (guard added at 0x004729BA).
- The client ran its own story (intro cutscenes) until it got the host's state: CSkaterCareer (skate module +0x20)
  WriteToStructure 0x00585550 / ReadFromStructure 0x00584D00 plus CSkater::WriteChapterInfo 0x005801B0 /
  ReadChapterInfo 0x00580210 (CHAPTER_STATUS / _INTROS_VIEWED / _OUTTROS_VIEWED), 3398 bytes via Script::WriteToBuffer.
  With that the client starts chapter 1 without the intro, moves its own skater and earns stat points.
- Side channel: own TCP link port+1 (5154). Goal wins mirrored through a trampoline on CGoalManager::WinGoal 0x00558030;
  host PlayCutscene (cfunc 0x005AE010) mirrored to the client. Not tested in-game yet.
- Goal mirror verified: host F9 (debug) won TR_Goal_Gaps27 -> client got "goal D0E4352B won by peer"; both showed
  Tony's chapter-advance dialog. Host "Yes" -> host sends the career again at LaunchLevel (cfunc 0x005654B0, table
  0x0067FD90) -> both loaded Boston as chapter 2 (host: Boston goal intro cameras, client: Select Pro Teammate).
- One earlier identical run crashed both on the BO load (host in the disc-error screen after File::Open failed;
  client later in a jump sound). Not reproduced on the next run. Unknown cause; LoadPre is now logged.

## Lobby (2026-10-24)
- Host F10 at the main menu: role host, link listen, server restarted as LAN (cleanup/leave/heaps/StartServer/
  JoinServer, then change_level Load_MainMenu). SetNetworkMode script cfunc (0x005A0A00, table 0x00680150) keeps
  LAN in co-op (story start / chosen_leave_server set 0).
- Client F11: link + hello -> career; waits. Host Story > Continue -> LaunchLevel hook (level=<checksum>, not
  Load_MainMenu 0xE92ECAFE) sends career + "join now" -> client joins as the host's level loads; both run
  chapter 1 and play TR_1A.. intro cutscenes in step (screenshots match frame for frame).
- Crashes fixed on the way: joining the host's main menu (Allocate null) -> join only into a level; cutscene
  mirror + local cutscene at once (DeleteHeap) -> mirror off by default ([coop] mirror_cutscenes); stand-in objects
  missing on the client (FindObject null in CCutsceneData::UpdateStandIn 0x00465AFF / DestroyObjects 0x00463CBD) ->
  guarded; "lost connection to the host" during cutscenes (ConnectionTask 0x0053EB70 silence timeouts) -> skipped
  while the co-op link is up.
- Cutscene end/skip mirrored: CMovieManager::Update's EndMovies(false) call 0x0057AF21 hooked; the playing name
  (GetPlayingName 0x0057B030, manager at skate module +0x368) sent; the peer ends it only if it plays the same one.
  Host Esc -> client "cutscene F5012F52 ended by peer" x3, both reached gameplay.
- Guest/pro swaps: SwapObjectModels (table 0x00681950, cfunc 0x005B34A0) mirrored as (local skater id, other id).
  Not verified with a real swappable ped yet; a debug swap with TRG_EricSkate_01 crashed in 0x0048E4D4 (also an
  arbitrary ped, not a swap target).
- NPC check (TR, chapter 1, after intro): single player has TRG_EricSkate_01, TRG_BamSkate_01, TRG_G_COUNTER_PRO;
  co-op host has Tony_Advance, Eric, Bam; client Eric, Bam. Peds are created in co-op; story peds come and go
  with goal state/timing.
- Test automation moved off OS input: [debug] automation=1 wraps DirectInput (CreateDevice -> keyboard
  GetDeviceState/GetDeviceData) and reads coop_cmd_<pid>.txt ("key", "wait", "spawn"); screenshots via PrintWindow.
- Client story save/load (SaveToMemoryCard 0x005A97E0 / LoadFromMemoryCard 0x005A8730, type=OptionsAndPros)
  refused in co-op.
- F12 teleport to friend: positions are synced by the net game (objects 0/1 m_pos +0x4C agree on both). Script
  Obj_SetPosition snapped back to a restart point; done natively like CSkater::MoveToNode (m_pos, m_oldPos,
  core physics +0x280 -> +0x98, SetTeleported 0x0045E4C0). Verified: both skaters side by side on both screens.
- Friend's name: the game's floating name (pos3D text) lands at the screen top in career mode. Replaced in co-op:
  own label removed, friend's drawn by coop_friend_label at a 2D pos projected with skatercam0's Gfx::Camera
  (pos +0x5C, matrix rows right/up/back +0x6C, hfov +0xB0 deg at 4:3, focal 320/tan(hfov/2)), clamped to the
  screen edge, text "<name>  <m>m", rgba [90 170 255 100].
- Injected "key up" did not move the skater (menus/Esc work): skater controls read input another way; unknown.
- Bot: automation "goto <object|friend> [radius]" steers with W/A/D (skater forward = matrix row 2 at +0xC0+32);
  reached the host from 2413 units in 9 s. Controls: WASD move, numpad tricks (ollie kp2, grind kp8,
  kickflip kp4); Esc skips cutscenes, kp2 skips goal cutscenes; dialogs (Tony yes/no) use arrows + Enter.
- Open (2026-10-24, paused): Boston goal points differed once (host 0/1000, client 825/1000); points =
  sum of won goals' rewards (GoalManager_GetActiveRewardPoints 0x0055B7C0) + total_cheat_points. Not reproduced yet.
  Request: goal cutscenes (goal intro / win cameras) for the client too.

## Night run 2026-10-25 (Boston)
- Two instances loading the same archives: one hit the disc-error screen (File::Open, exclusive open). Test-only:
  CreateFileA import (0x006450D0) wrapped to add FILE_SHARE_READ when multi_instance=1.
- Pro teammate: client's create_select_pro_menu waits; host's choice (select_pro_menu_choose) sent with its params;
  client runs it (scripts spawned for the peer are tracked by CScript unique id +0xB0 so hooks don't echo).
- Client's view of the host was NaN after a level change: the host's skater object stayed COMPOSITE_FLAG_PAUSED
  (+0x104 bit 0) on the client after the chapter intro, so its non-local playback never restarted. The DLL clears
  the flag on the friend's skater whenever no movie plays; playback also restarted after each LaunchLevel.
- Script commands' "skater 0" (Skate::GetSkater 0x005665C0 called from 0x0059F610-0x005B6560) = the local skater
  in co-op (the client's goal cameras: PlaySkaterCamAnim on a non-local skater did nothing).
- Goal cameras: PlaySkaterCamAnim mirrored (skaterId stripped), sent when it starts playing on the sender (a tour
  queues all at once); KillSkaterCamAnim / EndMovies end mirrored by name; remote end runs KillSkaterCamAnim
  Current (restores the camera). The client skips its own chapter goal tour (PlayIntroCameras from_chapter_intro:
  setup, KillChapterMenu, cleanup, callback) and shows the host's.
- beat_first_stat_goal paused the client's game and its dialog got closed by KillChapterMenu -> stuck pause. In
  co-op the message is a panel message, no pause.
- Pro swap verified: client swap_skaters with TRG_Ped_TeammatePro -> host "swap from peer ... done"; host sees the
  client as the pro.
- Goal/career native code checks GameNet InNetGame in 19 places (goal manager 0x00551AE0-0x00560D5C, CSkater::GetStat
  0x005804E5): in a network game won goals weren't kept in the career (lost on level reload). Retargeted to the
  co-op-aware check. Goal manager params (+0x14 via 0x004A2BA0, GoalManager 0x005591B0 - what a story save keeps as
  GoalManagerParams) now sent with the career, so a rejoining friend gets the won goals.
- Rejoin into a level a friend left: the old skater's leftovers crashed a new one (anim controller) -> host reloads
  the level and the friend joins as it loads.
- Model swap mirrored on the host's copy of the friend's skater must be undone before that skater dies or the level
  changes (a ped's model in a skater that lives across levels): undone at change_level (coop.qb wrapper ->
  CoopBeforeLevelChange), Cleanup (0x0067F5C8) and on link loss; kept out of the game's swap table (0x007CEDE0).
- Client losing the host in Barcelona crashed ~20 s later in TRG_BA_Ped_Skater02/03 (anim) during the game's own
  drop/lost-connection handling (goal teardown mid-level). Now the client leaves at once: PauseObjects,
  quit_network_game, UnpauseObjects, then a message (coop_client_host_gone). Host side: the dropped friend's skater
  is hidden + paused and removed at the level's Cleanup (Skate::RemoveSkater 0x00565790 detour), except while this
  game leaves the server (LeaveServer 0x005A08D0 hook).
- Host quitting to the main menu sends kHostLeft; the friend leaves too. Friend quitting (pause > Quit) allowed
  through the level_select_change_level wrapper (Load_MainMenu).
- Client stat tutorial dialog (beat_first_stat_goal) paused its game -> panel message in co-op.
- Demo (local/tools/demo.sh, 17 steps, TR -> Boston -> Barcelona, swaps, goals both ways, rejoin, host loss): passes
  with no crashes (2026-10-25 09:11). Single-player regression through Barcelona: OK.

## Shipping (2026-10-25 / 26)

### THEMOD's installer
- 7-Zip lists it as NSIS 2.46 with `Game/THEMOD 1.3.exe` (2008-11-08, SHA-256
  `25874001be0c1f12df47bc8f0dad61869cae0fe90964c12245165732a4cbb5ea`), `Game/data/pre/thethug2mod.prx` (2007-08-09,
  same SHA-256 as the installed one) and NSIS plugins (`HostsManager.dll`, `NSISFilePatch.dll`, ...).
- Installer exe vs `THUG2.exe`: 122 bytes in 7 runs. 0x1A8A01 `75 9C` -> `90 90` (save checksum jnz, the same patch
  PartyMod makes), 0x1F95BB `75` -> `EB` (GameSpy CD-key SDK, near gcd_init 0x005F9540), MOTD URLs ->
  thmods.com, `ReportStats` -> `RaportStats`, `qb_scripts.pre` -> `thethug2mod.pre`, peerchat -> peerchat1.thmods.com,
  HTTP user agent `GameSpyHTTP/1.0` -> `THEMOD13`.
- The tester's installed THEMOD exe = the installer's + 3 changes by an unknown third party: 0x0048C170
  (CFunc_SetScreenBlur) -> `mov al,1; ret` (blur off), the 800x600 mode's immediates (0x4D876E / 0x4D878A) ->
  1920x1080, peerchat host back to `peerchat.gamespy.com`.
- thmods.com does not answer (2026-10-26). The only trace found: a 2008 blog post listing THEMOD 1.0-1.3 with
  credit to "TH Mods" (https://dks-site.blogspot.com/2008/12/tony-hawk-underground-2-mod.html).
- The installer holds a full copy of the game's exe, so the mod ships a byte patcher (`patcher/patch_themod.ps1`):
  size + SHA-256 + every original byte checked, writes `THEMOD 1.3.exe`, never overwrites. Its table applied in
  Python to the tester's `THUG2.exe` gives the installer's exe (SHA-256 equal). Not yet run on Windows.

### Features added
- Shared goals: `goal_accept_trigger` wrapper -> `CoopSharedGoal mode=start` -> the peer starts it
  (`coop_remote_start_goal`); `WinGoal` trampoline holds the first finisher (`coop_goal_done_wait`) until the peer's
  `kGoalDone`; staged goals (training) shared per stage through `coop_goal_won_chain`.
- Player collision: `GameNet::Manager::PlayerCollisionEnabled` 0x00534340 detoured (6 bytes `56 8B F1 8A 46 3A`);
  `[coop] player_collision=` 1 / 0 / empty (the game's net option).
- Chat: `Sio::KeyInput` 0x005BDE70 detoured (7 bytes `A0 6E E4 7C 00 84 C0`); CP1251 bytes from a Cyrillic layout
  are transliterated (the fonts have no Cyrillic). The game takes at most 15 characters per frame.
- Keys: `[keys] host/join/teleport` (F1-F24, letters, digits, NumN, Insert..., Ctrl+/Shift+/Alt+).
- Lobby message on the host was wiped by the main menu reload: shown by `coop_lobby_open_message` after
  `is_changing_levels = 0` plus 20 frames.
- Resolution: device setup 0x004D870B picks one of 4 modes by Launcher.exe's setting (`mov esi,3; sub esi,edx`);
  `[display] resolution=WxH` NOPs the `sub` (always the last slot) and writes W/H into the 640x480 immediates
  (0x4D8776, 0x4D8792). Windowed: WinMain's window size pushes (0x4E274C, `push 480; push 640; push 100; push 100`)
  -> the resolution, centred (x/y -1), scaled to fit the work area; the post-device `SetWindowPos(0,0,w,h)`
  (0x4D8891, 14 bytes) NOPed in windowed mode (PartyMod does the same). Tested 2026-10-26: windowed 1280x720
  (client 1280x720, centred), windowed 1920x1080 on a 1080p screen (1797x1011), full screen 1920x1080, `launcher`.
- Splash-screen crash reported on a friend's Windows 11 PC at 2K: per a tester it happens with plain THEMOD 1.3
  too; running windowed avoids it. Not reproduced here.

