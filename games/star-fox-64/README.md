# Star Fox 64 (1997)

| | |
|---|---|
| Engine | Starship (native PC port from the decompilation, libultraship) |
| Versions tested | N64 US 1.0 ROM, SHA-1 d8b1088520f7c5f81433292a9258c1184afa1457 (US 1.1 is accepted but not tested). Played by hand: a Windows (MSVC) host and a Linux (GCC) client. Scripted tests: Linux (GCC, Debug, some Release), some also on Windows (MSVC, Debug and Release). macOS not built. No store: the game data is built from your own ROM |
| Projects | [Starship Co-op](https://github.com/Clodmony/starship-coop), a fork of [Starship](https://github.com/HarbourMasters/Starship); directory entry "Starship Co-op" |
| QA level | automated (the project's own level, [QA.md](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/QA.md)); a campaign was also finished by hand on two PCs, on a build from before the latest merge (see QA) |
| Last updated | 2026-11-11 |

## Where things stand

Two people can play the campaign together online, each on their own PC, if both build the same commit from source;
there is no release and no download. On 2026-11-09 and 2026-11-10 two people, a Windows host and a Linux joining player
on a direct connection over a VPN, played one route from Corneria to the ending (Corneria, Meteo, Fortuna, Solar,
Macbeth, Area 6, Venom and Andross) in 66 minutes with no desync. Sessions on the evening of 2026-11-10, on newer builds,
took the other way through Sector Y into Aquas and found two more faults; both are fixed, and the Aquas fix has not been
played on two PCs yet. The code merged on 2026-11-11 (PR #265, `coop` `33bd5fe6`) has not been played by hand. On that
code, when the host's ship goes down it is refilled in place for a life from the shared pool (game over at 0 lives); a
desync or a lost player starts the level again for everyone from the host's state instead of ending the session, so far
tested only by scripts on one PC; and each human's name is drawn over their ship.

## Existing work

- **[Starship Co-op](https://github.com/Clodmony/starship-coop)** (this project): active, last commit 2026-11-11. Lockstep
  campaign co-op, played by two, up to four players in the code, with the co-op code in `src/port/coop/` and every
  change to upstream game code marked `// COOP:`
  ([decision 0001](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0001-fork-and-code-layout.md)).
- **[Starship](https://github.com/HarbourMasters/Starship)**: the port this builds on. It has the game's four-player
  versus mode on one screen, and upstream builds with networking off. No campaign co-op.
- A search of this hub on 2026-11-11 (`data/directory.yaml`, `games/`, issues and pull requests) found no Star Fox 64
  entry and no mention of Starship. A search on 2026-11-01 found no co-op or online fork in the Starship forks list on
  GitHub, sorted by stars. Forks seen there, none of them co-op:
  [izzy2lost/Starship](https://github.com/izzy2lost/Starship) (Android),
  [SgtBilko76/Starship-VR](https://github.com/SgtBilko76/Starship-VR) and
  [keraion/Starship](https://github.com/keraion/Starship) (Archipelago).

## How the game works (for co-op)

File references in this section are to `Clodmony/starship-coop` at commit `797c678b` (2026-11-06), the version it was written
against: line numbers have drifted since, function and file names still find the code. The project's lab notebook has the detail:
[NOTES.md](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/NOTES.md).

Observed, from the code:

- **Engine:** decompiled C. Shared code is in `src/engine/`, level code in the overlays in `src/overlays/`, and the
  game has 406 object ids (`include/sf64object.h:725`). There is no general scripting language, but actors can run
  event scripts held as data (`include/sf64event.h`, `ActorEvent_Update` in `src/engine/fox_enmy2.c:3026`).
- **Netcode:** none upstream: `USE_NETWORKING` is set off, and `src/` and `include/` on upstream `main` have no socket
  code. Versus mode is local: `gCamCount` is 1, and 4 only for `LEVEL_VERSUS`
  (`src/engine/fox_play.c:2929-2932`), and `Player_InitVersus` sets up the players (`src/engine/fox_play.c:2810`).
- **Players:** `gPlayer` is a pointer to an array of `Player` (`src/engine/fox_context.c:373`), and many per-player
  arrays have four slots, such as `gLifeCount[4]` (`include/sf64context.h:228`). `gCamCount` is the player count and
  also, in about 25 places, the versus flag, so co-op cannot just set it to 2.
- **Single-player assumptions:** the audit at that commit lists 1788 uses of `gPlayer[0]`, `gPlayer->`,
  `gMainController` and `gCamCount` in 407 functions
  ([single-player.md](https://github.com/Clodmony/starship-coop/blob/797c678b/docs/coop/audit/single-player.md)). Its
  verdicts are a starting list: a sample of 45 found 5 wrong.
- **World state:** fixed arrays such as `gActors[60]`, `gBosses[4]` and `gEffects[100]`
  (`include/sf64context.h:232-238`), one level memory buffer with a bump allocator (`src/sys/sys_memory.c:5-12`), and
  globals. The structs hold pointers, so a snapshot only fits the same build on the same platform.
- **Levels:** two modes, `LEVELMODE_ON_RAILS` and `LEVELMODE_ALL_RANGE` (`include/sf64player.h:62-63`). `Play_Init`
  frees level memory (`src/engine/fox_play.c:2847`). On rails, `Player_UpdatePath` copies the player's own `zPath` into
  the global `gPathProgress` (`src/engine/fox_play.c:3643`), and `Object_LoadLevelObjects` loads objects by that global
  (`src/engine/fox_enmy.c:630`).
- **Randomness:** `Rand_Init` seeds three values from `osGetTime()` (`src/sys/sys_math.c:15-18`) at boot
  (`src/engine/fox_game.c:58`). On Linux and Switch, `Rand_ZeroOne` seeds again from the clock when a seed is 0, and
  that check tests `sRandSeed2` twice and never `sRandSeed3` (`src/sys/sys_math.c:25`).
- **Audio and logic:** one game frame runs the whole game update first, and only then the audio thread
  (`push_frame`, `src/port/Game.cpp:24-31`). Game logic still reads audio state: the radio waits for the voice sequence
  (`src/engine/fox_radio.c:615-645`).
- **Saves:** one `gSaveFile` (`src/engine/fox_save.c:3`), written by `Save_Write` (`src/engine/fox_save.c:142`). At that
  commit no co-op code touched it; how a session handles it now is under "What we changed".
- **Hooks:** the port has an event system for reaching game code without editing it (`DEFINE_EVENT` and `CALL_EVENT`,
  `src/port/hooks/impl/EventSystem.h:35`, `:43`).

Derived, from test runs:

- On one PC and one build (Windows, MSVC, 2026-11-05), the hashed state of a full Corneria run depended only on inputs
  and seeds. Graphics backend, window and frame pacing, audio timing and turning off the C runtime's FMA3 path changed
  nothing, on all 13545 frames. That is what lockstep needs on one machine.
- Two players need one path. Player 0 owns it, and the others follow it: with both players writing, `gPathProgress`
  matched single player on only 26 of 4525 frames.

Found by real sessions between a Windows PC and a Linux PC (2026-11-08 to 2026-11-10, each PC on the same commit). The
one-PC tests had missed every one of them; the first four are differences between the two builds:

- The C library's `sinf` and `cosf` round some inputs differently on the two platforms, and the game calls them through
  `SIN_DEG` and `COS_DEG`: the first session parted at frame 22 of Corneria. With that fixed, the next session parted
  after 4556 frames on a sprite's angle one bit apart, which the game computes with an arctangent, most likely through
  `atanf` (that frame's input was not recovered). In a session the game now uses the N64's own sine and cosine, in
  double arithmetic, and the game's own arctangent ([decision 0011](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0011-deterministic-sine-in-sessions.md)).
- Two random draws in one expression: MSVC took the right one first, GCC the left one, and the games parted in Macbeth
  on the Landmaster's dust ([#199](https://github.com/Clodmony/starship-coop/issues/199)). In a session that expression and
  18 of the same kind in Titania now draw in their written order. Draws in two arguments of one call went right to left
  on both compilers where measured, and are left as they are.
- The state hash read past a four-byte buffer for a player who had not joined: asset names on Linux, pointers on
  Windows, so the games parted at frame 0 ([#240](https://github.com/Clodmony/starship-coop/pull/240)).
- `Actor.iwork` is `uintptr_t` (unsigned, 64 bits) in the port and an `s32` on the N64, so a negative index compared as
  a huge number and a dying eel in Aquas' boss fight read its smoke's place from outside an array, which the two
  compilers lay out differently ([#261](https://github.com/Clodmony/starship-coop/issues/261)). Two Linux games read the
  same memory and stayed equal, so a hash comparison on one PC could not see it.
- Not platform differences, but missed for the same reason (every one-PC test had used one save, one camera or fresh
  games): the map reads the save of the PC it runs on, so two saves parted 41 frames into the map
  ([#260](https://github.com/Clodmony/starship-coop/issues/260)); draw code wrote a hashed field only for a ship the PC
  draws, so two cameras parted ([#192](https://github.com/Clodmony/starship-coop/issues/192)); and a level played
  earlier in the same process left values the hash read at frame 0
  ([#187](https://github.com/Clodmony/starship-coop/issues/187)).

Unknown (2026-11-11):

- Levels not played by hand with two real players: Sector X, Sector Z, Katina, Bolse, Zoness, Titania, Venom 1. Scripted
  two-client runs now reach the end of all 17 campaign levels (the test pilot is invincible, and has unlimited bombs in
  Zoness and Venom/Andross), so those runs say the games stay equal, not that the bosses are fair to a human.
- A real connection that drops between two PCs. The recovery after a lost player is tested only by scripts that stop and
  freeze or remove a client on one PC, with two and three clients. Windows has not run it.
- More than two players in a real session. Three clients run in scripted tests; four are in the code.
- A Release build in a campaign between two PCs, other compilers (clang, macOS), and any network but one direct
  connection over a VPN.
- The Steam relay on real Steam: it is built and tested on a fake Steam only.

## What we changed

Each item is a bug or a missing piece, its cause, the fix, and the test that proves it. The tests are in `tools/coop/`; the
latest results are in the QA table below.

- **The player count was the camera count.** `gCamCount` is both, and it also means "this is versus". Fix:
  `CoopPlayers_Count()` (`src/port/coop/game/players.c`) counts players and `gCamCount` stays 1. Test: with co-op off, a
  Corneria recording made before the change still plays to PASS on all 13545 frames, and `hash_check.py base` shows that
  with co-op off a build hashes the same as the build before the change on every frame.
- **A second player on rails and in all-range.** Each player wrote the shared path. Fix: player 0 owns the path and the
  others follow it; extra players start parked and join as a copy of player 0. Test: `player_trace.py rails` with 2 and 4
  players: the path is player 0's, and the others stay on it, on every rail frame of Corneria, in a plain flight (4525
  frames) and in one with boosts (4253).
- **No netcode, and nothing kept mismatched clients apart.** Fix: UDP on SDL2_net with our own reliability layer
  ([decision 0004](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0004-transport.md)), and a session
  handshake that refuses a different build, ROM or simulation setting, warns about a different platform, and carries who
  flies which character ([decision 0005](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0005-players-and-characters.md)).
  Test: `net_test.py`, `session_test.py` and their mutation suites (each deliberate break of the code must fail a test).
- **Keeping two PCs equal.** Fix: lockstep (inputs go over the network with 3 frames, 100 ms, of input delay, and every
  frame waits for every player), a hash of the game state compared on every frame, and on a difference a report from
  each game plus a co-op log streamed to the others
  ([decision 0002](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0002-sync-model.md)).
  Test: `lockstep_test.py`, where a deliberate one-bit difference must show at exactly its frame and both games must
  write a report that names it.
- **What the real sessions found** (see "How the game works"). Fix: the N64 sine, cosine and arctangent in a session,
  draws in their written order, a hash buffer as large as the largest value, a signed comparison for the eel (in single
  player too: the old read left the array), the host's save as the session's save, the laser guns' position set by the
  simulation instead of the draw, and a clean level start. Tests: `math_test.py` and `rand_order_test.py` (bit for bit
  against a reference), `lockstep_test.py --only map_saves` (two different saves; on the build before it parts 41 frames
  into the map), `--only eels` (Aquas' boss; on the build before 34 of 171 eel puffs lie outside the fight) and
  `--only reset`, and `view_check.py`.
- **The save in a session.** The host sends its save once before the level starts, every game plays on that copy and
  keeps its own sound settings, and no game writes its save file during a session, the host's included
  ([decision 0009](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0009-vehicles-cutscenes-save.md),
  item 8). Test: `map_saves`, which also checks that both save files are unchanged byte for byte afterwards.
- **The session no longer ends when a player is lost or the games part.** Until 2026-11-11 a lost player, a player
  silent for 10 seconds or a desync ended the session for everyone; one lost run at Aquas' boss. Fix: the level starts
  again for everyone from the host's state (the level the team last entered, from its last checkpoint if the host had
  reached one, with no life taken). The host announces it, sends its save again and a new level start with a run number
  that every lockstep and desync message carries, so a late message of the old run is dropped. A player whose connection
  was lost has 15 seconds to come back by itself with the session id (a random 64-bit number the host makes outside the
  simulation); then the level starts with whoever is there. The check is not loosened: a desync still writes its report,
  and each game's log now names the first differing field, the same one `desync_report.py diff` names. Two desyncs within
  300 frames of a level start, no one left, or a different build still end the session
  ([decision 0014](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0014-the-session-survives.md)).
  Test: the `survive_*` scenarios of `lockstep_test.py` part two real games on purpose, freeze one client until it
  comes back with the session id, or take one of three away, and require equal hashes on the first 600 frames of the
  level that starts again. Not tried: a real connection drop, Windows, four players.
- **Names.** A "Your name" field in the co-op window (up to 8 characters of the game's own small font), sent to the host
  with `SESSION_NAME`, shared with everyone and drawn over each other human's Arwing on every client, or the character's
  name when none was typed. Display only, not in the state hash. Test: `lockstep_test.py --only player_names` (equal on
  2700 frames with names on one client and none on the other) and `view_check.py`.
- **Game rules for more than one human:** one shared pool of lives, with players 2 to 4 coming back after 150 frames and,
  since 2026-11-11, Fox refilled in place for a life instead of restarting the level, on 16 levels
  ([decision 0007](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0007-lives-and-respawn.md)),
  the team earning and scoring ([0008](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0008-routes-and-score.md)),
  a Landmaster or Blue Marine per player (0009, item 6), a human-flown wingman and the story scenes it would be in
  ([0006](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0006-human-flown-wingman.md),
  [0012](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0012-taken-wingman-scenes.md),
  [0013](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/decisions/0013-rescue-lines.md)), most aimed enemy
  shots and most bosses going after the nearest human, and items and level triggers that count every player. Test: a
  scripted two-client scenario per rule in `lockstep_test.py` (`return`, `lives`, `kill`, `laser`, `crate`, `wing`, `vehicles`,
  `scenes`, `guests`, `aim`, `bosses`, `other_bosses` and more).

## QA

Level: `automated`, the project's own level: scripts drive two or three real game clients in lockstep and check that
their state hashes match on every frame. A campaign was also finished by hand in co-op on two PCs (last row of the
table). It ran on a test build from 2026-11-09, before the merge of 2026-11-11 changed boss behaviour and the save
handling, and the project has not claimed `verified-playthrough` for it.

Unless a row says otherwise, the scripted runs below are on Linux, on one PC, with no window and the US 1.0 ROM; the
two-client ones connect over loopback.

| Command (in `tools/coop/`) | What it asserts | Last result |
|---|---|---|
| `test_all.py` | 53 steps: replays and state hashes, determinism variants, Debug against Release, co-op off against the build before, the transport, session and menu tests (also under AddressSanitizer), 48 lockstep scenarios (most with two real game clients) and the mutation suites | PASS 53/53 in 40 minutes, 2026-11-11, Linux, on `095ddc3d`, whose tree is the merged code (`coop` `33bd5fe6`) |
| `lockstep_test.py --only full` | Two clients with a pilot each fly Corneria to the map: equal on every frame; a one-bit change on one client shows at exactly its frame | 10601 frames equal, the change at frame 8601 seen at 8601; 2026-11-11, Linux (a step of `test_all.py`) |
| `lockstep_test.py --only map_saves` | Host with a blank save, client with every level cleared: one game through Corneria, the map and 300 frames of the next level, and both save files unchanged afterwards | 11689 frames equal, both files byte for byte as before; 2026-11-11, Linux |
| `lockstep_test.py --only eels` | Aquas' boss fight with both Blue Marines firing: equal, and every eel's smoke where the fight is | 9000 frames equal, 171 eel puffs, all in the fight; 2026-11-11, Linux |
| `hash_check.py base` | With co-op off, input by rule from boot through the menus into Corneria hashes the same as on the build before the change | Equal on all 5800 frames against `coop` before the merge; 2026-11-11, Linux (a step of `test_all.py`) |
| `session_test.py`, `session_mutants.py` | Handshake (a different build, ROM or setting refused, a different platform warned), character picks, level start, also on a lossy link; each of 434 deliberate breaks of the session code must fail a scenario | 17/17 scenarios, all 434 breaks caught; 2026-11-11, Linux (steps of `test_all.py`) |
| `lockstep_test.py --only survive_desync,survive_three,survive_checkpoint,survive_checkpoint_allrange,survive_next_level` | A pair of games parted on purpose, a frozen client and a client taken away: the level starts again from the host's state (from its checkpoint where it had one) | Equal hashes on the first 600 frames of the new level in each; 2026-11-11, Linux, one PC, two or three clients, on `095ddc3d` (the merged code's tree); the first four are steps of `test_all.py` |
| `campaign.py --route all` | Every campaign level as its own two-client session, a pilot on each client, from the level start to the map | All 17 levels reach level complete and stay equal on every frame, no desync report; the pilot is invincible except in Corneria and has unlimited bombs in Zoness and Venom/Andross. 2026-11-11, Linux, on the campaign-runs branch (code `303890d2`). Sector X, Titania and Sector Y rerun on the merged code: all three equal on every frame to the end. Before it, 12 of the 17 on 2026-11-10 |
| `chaos_run.py` | A pilot-flown Corneria session with the client's traffic through a relay that adds lag, loss and reordering: six profiles, three seeds each | 18/18 runs equal to frame 6000; a 12-second outage ends the session after 10 seconds without input, as designed. 2026-11-10, Linux |
| `determinism.py`; `hash_check.py builds` and `rates` | One build hashes the same whatever the graphics backend, pacing, audio timing or FMA3; Debug equals Release; 30 to 360 fps equal | PASS on Windows (MSVC) on 2026-11-05 and 2026-11-08, at older commits; on Linux in every `test_all.py` |
| Played by hand | Two people, two PCs (Windows host, Linux client), direct connection over a VPN | Corneria to the ending in 66 minutes, no desync, median ping 33 ms, 2026-11-09 and 2026-11-10 (build `89efaa8a`). Sessions on the evening of 2026-11-10 (builds `b98c3870`, `5b0e2204`) parted on the map and in Aquas' boss fight; both faults are fixed since |

## How to play

There is no release and no download. Each player needs their own Star Fox 64 ROM (US 1.0 or 1.1, the same version for
both) and the same build of this project, built from a clean checkout of the same commit:
`python tools/coop/build.py --config Release --assets`
([BUILDING.md](https://github.com/Clodmony/starship-coop/blob/coop/docs/BUILDING.md); only US 1.0 has been tested). In the
game press F1 and open **Co-op**; type a name in **Your name** if you want one shown over your ship; the host clicks **Host a game** (UDP port 46464 by default, which must be reachable: the
same network, a forwarded port, or both on a VPN); the other player types the host's address, clicks **Join a game**
and picks a wingman (the host flies Fox); then the host clicks **Start the level**. Both players must see the same
`Co-op <build id>` text in the menu bar. The host steers: route splits, the map and the pause menu's choices are the
host's. The README has the full steps and what each rule does:
[How to play](https://github.com/Clodmony/starship-coop#how-to-play).

## Known issues

What a player will notice, as of 2026-11-11:

- When the host's ship goes down it is refilled in place for a life from the shared pool, on 16 levels. Andross' lair,
  the warp zones and training are not covered, and with the pool at 0 the game's original rule runs (Fox goes down, the
  others park, game over). Nobody has played this by hand.
- A desync, a lost connection, a player silent for 10 seconds or a player who leaves a session of three or four starts the
  level again for everyone from the host's state (from the host's last checkpoint, if it had one). The level that parted
  is lost. A lost player has 15 seconds to come back by itself. With nobody else left, after two such restarts within 300
  frames of the level start, or with a different build, the session still ends. Tested by scripts only.
- What a session earns (clears, medals, rankings) is not kept on either PC.
- Macbeth's train end still goes after the host only (it is an actor; the sites are listed, not changed). The other
  bosses that aim or chase now go after the nearest human, Sector X's Spyborg and Titania's Goras included; scripts have
  tested each of those changes, Solar's Vulkain too, and nobody has played it by hand yet.
- While your own ship is out, you watch the host's view.
- A story scene of a wingman whom a human flies is flown by a free computer wingman, or by Katt or Bill when none is
  free, with that character's lines in place of the taken wingman's or none (decisions 0012 and 0013). Nobody has
  listened through those lines on a real run.
- Between a Windows PC and a Linux PC, most real sessions so far found a new difference (each is fixed); the campaign run
  found none. More are likely.
- Not played by hand with two players: Sector X, Sector Z, Katina, Bolse, Zoness, Titania and Venom 1; three or four
  players; a Release build in a campaign; the Steam relay.
- No split screen, no macOS build, no download.

## Next steps

The [roadmap](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/ROADMAP.md) lists every work package. The
main steps now:

1. Play the merged code through on two PCs again
   ([TWO_PC_TEST.md](https://github.com/Clodmony/starship-coop/blob/coop/docs/coop/TWO_PC_TEST.md)), with Aquas' boss on the
   route, the refill of Fox's ship and a real connection drop
   ([#267](https://github.com/Clodmony/starship-coop/issues/267)).
2. Two scripted scenarios outside `test_all.py` that fail on `coop`, a boss-aim check and a frame limit
   ([#266](https://github.com/Clodmony/starship-coop/issues/266)).
3. On Windows, on the current code: `hash_check.py builds` and `rates` again, and `rand_order_test.py` with MSVC.
4. A downloadable build, the Steam relay on real Steam, and more than two players in a real session.
