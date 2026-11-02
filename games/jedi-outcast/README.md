# Star Wars Jedi Knight II: Jedi Outcast (2002)

| | |
|---|---|
| Engine | id Tech 3 (Raven single-player branch), run through [OpenJK](https://github.com/JACoders/OpenJK) at `2ba5021`, Windows x64 (MSVC, Ninja) |
| Versions tested | Steam (app 6030), `GameData\version.inf` SinglePlayerVersion 1.03. The retail `jk2sp.exe` (SHA-256 `5d220aefad2262ec49497b25cabe6a4a28ef87ea5949bb07bd5ae0801197ae42`) is not used: OpenJK reads only the retail `base\assets*.pk3` |
| Projects | [jedi-outcast-coop](https://github.com/Benehiko/jedi-outcast-coop) (original), [Jedi-Outcast-Coop](https://github.com/masonan777/Jedi-Outcast-Coop) (fork that continues it); both in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated (level loading with a joiner, all 26 campaign maps); gameplay fixes hand-tested in two-player split screen on one PC |
| Last updated | 2026-10-25 |

## Where things stand

Two players can play the whole campaign together: one hosts, the other joins over the network or in a second window on the same PC with their own controller. Level scripts, enemies, Force powers and the lightsaber work for the joiner, and a revive system replaces death while a partner is up. Objectives text for joiners, remote views (security cameras, droids) and the AT-ST are still host-only. Two separate PCs and 3–4 players have not been tested yet.

## Existing work

- **[Benehiko/jedi-outcast-coop](https://github.com/Benehiko/jedi-outcast-coop)** (GPL-2.0, last commit 2026-07). The base for everything here. 31 patches on a pinned OpenJK commit widen single-player to 4 clients, add a UDP transport, LAN discovery, a `coop_host` command, a Co-op menu page, and a **dual-load** remote client. A Go tool (`jk2coop`) applies the patches, builds, installs and launches. Its README lists joiner objectives/cutscenes (Track F) as in progress.
- **[masonan777/Jedi-Outcast-Coop](https://github.com/masonan777/Jedi-Outcast-Coop)** (GPL-2.0, 2026-10). Keeps the original history and adds one cumulative patch, `0032`, plus Windows tools. This write-up covers that patch.
- **[JACoop](https://github.com/Parricidium/JACoop)** (Jedi Academy) uses the same dual-load design and was a useful reference per call site; its code isn't portable as is (JA's single-player game code differs).

## How the game works (for co-op)

- **Source, not reverse engineering.** Raven released the single-player source; OpenJK builds it. Game and cgame are one DLL (`jospgamex86_64.dll`).
- **Dual-load design (Benehiko's):** the host runs the real campaign. A joiner has no local server; it runs cgame against *placeholder* game entities filled from network snapshots. Any field only the host's game code writes (gentity data, Ghoul2 bone state, `renderInfo`) is zero or stale on the joiner unless something networks it.
- **What goes on the wire** is the field tables in `code/qcommon/msg.cpp` (`NETF` for entities, `PSF` for the player state). Adding a field there is the cheapest way to get host data to joiners.
- **"The player" is entity 0 everywhere.** Single-player code says `s.number == 0`, `!ent->s.number`, `g_entities[0]`, `cg_entities[0]` or `client == 0` for the player. On the host that skips joiners (treated as NPCs); on the joiner `cg_entities[0]` is the *host*. Most joiner bugs came from one of these. The fix is a `G_IsCoopPlayer(ent)` check on the host and "local client = `cg.snap->ps.clientNum`" in cgame.
- **Level scripts (ICARUS)** address the player as `kyle`. Names resolve through `ICARUS_EntList` (last registered wins), and an `affect("kyle")` block is parsed into that entity's sequencer when the outer script starts. Joiners registered as `kyle` after the host, and only entity 0's task manager ran, so scripted events (doors, troop spawns) silently never ran.
- **Renderer callbacks into the server:** the renderer calls `SV_PointContents`/`SV_Trace` (fog volumes, every frame on Bespin). A joiner has no server world, so those crashed; they must fall back to the local collision map.
- **Single-player shortcuts that touch the whole game:** Force Speed changes the engine `timescale` (slows everyone's game), and the hardware gamma ramp brightens the whole desktop.

## What we changed

All in patch `0032`. "Confirmed" means the author checked it in two-player split screen on one PC after the fix; "not confirmed separately" means it was built and played over but nobody checked that specific behaviour.

| Bug | Root cause | Fix | Test |
|---|---|---|---|
| Scripted events (Jan opening the kejim_post blast door, troop spawns) never ran with a joiner present | Joiners also registered script name `kyle`, last one wins; only entity 0's task manager updated | Script names for player slots always resolve to the host; joiner task managers run; `g_coopScriptLog` traces every script command | Confirmed |
| Joiner shots came from below the eyes | Muzzle point cached by the host's cgame for its third-person view of the joiner | Joiner fires from eyes plus a per-weapon offset | Confirmed |
| Enemies ignored or missed the joiner | NPC/joiner eye points only written when the host draws them; dozens of `s.number == 0` checks | Server refreshes eye/head/torso points and joiner light level each frame; player checks converted | Not confirmed separately |
| Enemy animations froze or slid on the joiner's screen | Joiner anim driver differed from `PM_SetAnimFinal` (backwards anims, bone mapping, blend); reused entity slots kept old bodies | Driver mirrors the host's; slot reuse detected by a gap in snapshot time | Confirmed, plus an anim log with 0 problems |
| Joiner dropped on Bespin levels ("Cannot use streamed MP3 files here") | Movers sent the host's raw sound handles; in the joiner's process the same number is another (streamed) sound | Send the sound set's *name*, resolve it locally | Confirmed |
| Joiner crashed on maps with fog | Renderer called `SV_PointContents` with no local server | Fall back to `CM_PointContents`/`CM_BoxTrace` | Confirmed; found from a WER crash dump symbolized with the PDB |
| Joiner had no Force bar, couldn't select or use Force powers | Force fields not in `PSF`; selection read from the host; use only checked for entity 0 | Fields networked; joiner sends its selection (`coopforce`); host runs the use check for joiners | Confirmed |
| Thrown saber blade detached from the hilt, Force aura missing on the joiner | cgame looked up the saber's owner by `s.number`, which is 0 on placeholders; Force effects read host-only player state | Look up by entity pointer; Force effect bits sent in spare `entityState.powerups` bits (widened to 32 on the wire) | Confirmed |
| Force Speed made the joiner's view stutter | Host's `timescale` slowed the whole server | With joiners present, Force Speed is a movement boost instead | Not confirmed separately |
| Breakable explosions, debris, glass, turret aim, mover sounds, dialogue, center prints missing for the joiner | Host-only direct calls into cgame | New events/fields carry them | Turret aim confirmed; the rest not confirmed separately |
| A death ends the run for both | Single-player death flow | Revive: downed instead of dead while a partner is up, hold USE 3 s to revive at 25 % health, all down = mission failed | Confirmed |
| Co-op only reachable from the console / PowerShell | The Co-op menu page existed but no button opened it | Engine adds a CO-OP button to the stock main and pause menus at load (parses `ui/coop_buttons.txt`, then `Menu_PostParse` so items get screen coordinates); menu hosts, joins by LAN list or IP, or starts split screen | Confirmed |
| Split screen needed a launcher script | — | `coop_splitscreen` puts the host on the left half, waits for the level, starts player 2's game itself in the right half, assigns controllers by count | Confirmed |

## QA

- **Level crawl bot** (in the host's game code, `coop_crawl*` cvars): for each map it waits for a joiner whose `commandTime` keeps advancing, moves both players to item/NPC/door spots in god mode, blocks level exits, logs `MAP_START/JOINED/TOUR/RESULT`, then loads the next map. The joiner's cgame logs its own view (`P2 MAP/INGAME/ALIVE`). Videos and intro cutscenes are skipped only in the crawl; a cutscene stuck for 20 s real time is ended.
- **Launcher:** `powershell -ExecutionPolicy Bypass -File tools\windows\jk2coop-levelcrawl.ps1 [-Maps a,b] [-Dwell 30] [-Mute]`. Fresh home folders per run, restarts crashed games, hang watchdogs, new crash dumps reported, prints `RESULT: PASS/FAIL`. `-SelfTest` feeds known bad logs to the scorer (must FAIL); `-TwinKillJoiner` kills the joiner mid-map and must report FAIL ("PASS (twin caught)").
- **Last full run (2026-10-25):** `RESULT: PASS (26/26 maps)`, one host and one joiner process for the whole campaign, no crashes, drops or restarts, no new crash dumps. Three intro cutscenes (artus_topside, valley, bespin_undercity) were ended by the stuck-cutscene breaker; `pit` passed with no tour points found, so it only proves loading.
- **Not automated:** gameplay sync (Force, saber, revive) was checked by hand in split screen. Two PCs and 3–4 players: untested.

## How to play

- Build from source (the repo's `jk2coop` tool, or OpenJK's CMake with the patches applied). The fork also has `tools/windows/jk2coop-package.ps1`, which zips a build with an installer (`Install.cmd`) that finds the player's own Jedi Outcast install (Steam library folders, GOG, disc) and adds a desktop shortcut; game data is read in place through `fs_cdpath`, never copied.
- In game: main menu > **CO-OP** > HOST NEW GAME / join from the LAN list or by IP (UDP 29070) / START SPLIT SCREEN.

## Known issues

- Joiners don't get the objectives screen or mission text (Track F), and remote views (security cameras, droid control) and the AT-ST are host-only.
- Joiners are frozen during cutscenes.
- Dropped items float slightly on the joiner's screen.
- Not yet tested on two separate PCs, or with more than 2 players.

## Next steps

- Two-PC network test, then a 3-player crawl.
- Track F: objectives and mission text for joiners.
- Find out whether the three stalled intro cutscenes also stall in normal co-op play or only under the crawl's fast-forward.
- Steam invites (lobby + relay) next to direct IP, see [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md).
- `wp_saber.cpp` still has about 60 `s.number` checks; JACoop converted the equivalent JA ones.
