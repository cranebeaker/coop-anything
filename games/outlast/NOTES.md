# Outlast (2013): working notes

> Where the scripts and patches are: the folders `harness/`, `patches/` and `play/` named below now live in [`tools/outlast-coop/` of the own fork https://github.com/G3RKA/OpenOL](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop) (as `harness/`, `relay-patches/`, `exe-patch/`, `play/`), not in this repository.

Terms: "owner" always means the client that owns (simulates) an enemy, OpenOL's `OwnerID`; the other clients only show a puppet of it. The scripts mentioned (`harness/...`) are in [harness/](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/harness/). Screenshots and raw
logs from the runs are kept privately (desktop captures; the repo rules forbid publishing them); the findings are written out here.

## Phase 0: triage (2026-10-20)

### Game identity (observed via MCP `game_identity` / `detect_engine`)

| | |
|---|---|
| Store | Steam, appid 238320 |
| Build id | 576074 |
| Main exe | `Binaries\Win32\OLGame.exe`, PE32 x86, 26000896 bytes |
| SHA-256 | `fc06a9d562ea41757fa7c95abae67c89fb4f56a9c970122fdef2f15758f2bb0d` |
| File version | 1.0.12046.0 (string table says 1, 0, 8767, 0) |
| PE timestamp | 1398795250 (2014-04-29) |
| Engine | Unreal Engine 3 (evidence: `CookedPCConsole`) |
| DLC | Whistleblower installed (`OLGame\CookedPCConsoleDLC`) |
| OS | Windows 11 |

- Not in DIRECTORY.md or `games/` as of this clone. Issue search "outlast" in this repo: 0 results.
  PR search: not done (`gh` not installed).

## Phase 1: research (2026-10-20)

### Observations

1. **OpenOL** https://github.com/ShyKiss/OpenOL (author ShyKiss)
   - Modified UE3 client (rebuilt `OLGame.exe` + `.upk`), drop-in over a licensed Steam install.
   - License: none stated (GitHub API `license: null`).
   - Last push 2026-08-29. Latest release 2.1.0 (2026-08-29): `OpenOL.7z` sha256
     `f15d724124a07cad8a58b0465795d66610ba431608ee65bdf4844667e682dc62`, `OpenOL.zip` sha256
     `b301b267d4c2e26815b6b7f11267ecae493c65db2ad523ab2d23512d9db2d67d` (GitHub asset digests).
   - Multiplayer beta since 2.0.1 (2026-08-07). README claims sync of doors, triggers, enemies,
     cutscenes. 2.1.0 notes: channel-based packets; door, enemy, push, world, CSA channels.
   - Hosting: Multiplayer -> Start Server (embedded relay, Steam P2P, room `DEFAULT`), or
     standalone `OpenOL_Relay` UDP 7777, room `PUBLIC` (repo ShyKiss/OpenOL-Relay).
   - Author (issue #6): client requires Whistleblower DLC (`CookedPCConsoleDLC/DLC_Intro_Persistent.upk`).
   - Open bug reports: #7 "Connect does nothing" (2026-09-14), #8 "Invite in Steam and Start
     Server do nothing; F6 relay enables invite but still doesn't join" (2026-09-21). No author reply.
2. **Outlast-Together** https://github.com/LucianLandIsBack/Outlast-Together
   - MIT, 5 commits, no releases; school project, README says no further updates.
   - Folders `Outlast Networking DLL`, `OutlastNetworkingClient`; what it syncs: unknown.
3. Steam Workshop "Outlast multiplayer" (316004291) is a Garry's Mod map, removed. Not a mod.
4. ModDB / Nexus Outlast mods: single-player only in search results.

### Not yet searched
Reddit, Discord, archive.org, Steam guides, OpenOL forks/branches, OpenOL-Relay repo.

### Unknowns
- (Answered by later test runs below: OpenOL MP does work on build 576074 with the standalone relay; synced items are listed in the README.)
- Saves and inventory/batteries: not checked.
- Max player count.

## Phase 2–3: workspace and source reading (2026-10-20)

- Test copy of the Steam install made with robocopy (1345 files, 5.868 GB, 0 failed).
- OpenOL 2.1.0 `OpenOL.zip` downloaded, SHA-256 matched the GitHub asset digest, installed
  over the test copy only. Overwrites 15 stock files (incl. `Binaries\Win64\OLGame.exe`,
  `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultUI.ini`, `OLFrontEnd.upk`, localization).
  Modded `OLGame.exe` (Win64) SHA-256 `2e58afbb1203d04476581f34216428d1dfc96228e5f3df8c6881323c56c93346`.
- Source: https://github.com/ShyKiss/OpenOL commit `2ab9ecaa17325c32260aba039e5398d5720f0acc` (2026-08-29).
- UE3 writes config to `%USERPROFILE%\Documents\My Games\Outlast\OLGame\Config` even for a copied
  install; the MP settings file is `OLMultiplayer.ini` there (`FMpConnection::LoadConfig`).

### Observations (source)
- Host button (`OLUIFrontEnd_Multiplayer.uc` `Press_CreateServer`): `NativeStartRelay(port)` then
  `NativeConnectLocal(port)`, which hardcodes room `DEFAULT` (`MultiplayerLink.cpp` `GMpConn_ConnectLocal`).
  Then `open DLC_Intro_Persistent?game=Multiplayer.MultiplayerGame` (Whistleblower map, so DLC is required).
- Embedded relay creates only room `DEFAULT` (`openol_relay/src/relay_api.c`).
- Client "Connect" reads room from `OLMultiplayer.ini`; release `DefaultMultiplayer.ini` ships `RoomCode=PUBLIC`.
- Relay replies `SRV_HELLO_FAIL "unknown room"` for an unknown room (`server.c` `handle_hello`).
- Client ignores HELLO_FAIL: `MultiplayerController.cpp` `case SRV_HELLO_FAIL: break;`. No message to the player.
- `DefaultEngine.ini` adds `+SeekFreePackage=OpenOLResources`, no such package in the release archive. Effect: unknown.

### Test run: relay protocol without the game (2026-10-20)
- Built the embedded relay sources unmodified with MinGW gcc plus a 15-line `main` (`relay_host.c`),
  probed with `hello_probe.py` sending the client's exact HELLO line.
- Result: `DEFAULT` -> `READY player_id=1000`; `PUBLIC` -> `HELLO_FAIL unknown room`. 2/2 as expected.

### Derived
- Direct-IP join to a player hosting with "Start Server" fails silently unless the joiner changes
  the room code from `PUBLIC` to `DEFAULT`. Matches OpenOL issue #7 ("Connect does nothing"). Not yet
  confirmed in-game.

### Test run: modded game as client, launched from the command line (2026-10-20)

Launch: `Binaries\Win64\OLGame.exe DLC_Intro_Persistent?game=Multiplayer.MultiplayerGame -nostartupmovies -log=mp_a.log`
(log lands in `<game dir>\OLGame\Logs`).

Observed:
1. With the release config (room `PUBLIC`, port 7777, no relay running), the client loads the MP map,
   logs `MpConn: Resolving '127.0.0.1':7777 room='PUBLIC'`, then `HELLO #1..#46` every 2 s forever.
   Nothing is shown to the player. (Single instance, Steam on, it had focus.)
2. The modded build writes its config to `<game dir>\OLGame\Config\` (OLEngine.ini, OLMultiplayer.ini, ...),
   NOT to Documents. `OLMultiplayer.ini` is generated there from `DefaultMultiplayer.ini`.
3. Saves and the profile live in Steam Cloud: `<steam>\userdata\<id>\238320\remote\` (`*.sav`, `profile.bin`).
   With Steam on, the mod reads that profile (`Profile version mismatch (19 != 17)`, then
   `Setting to defaults`). Risk: the mod may write a v19 `profile.bin` over the player's own. Not observed
   (no file changed after several force-killed runs), but untested on a clean exit.
4. `-nosteam` works: `Steam Client API Disabled!`, `UOLProfileSettings::SetToDefaults()`, username `Player`.
   Cloud profile is not used. UDP relay connect does not need Steam.
5. Resolution and fullscreen come from the profile (`OLEngine.cpp` around line 1883 `### Applying resolution`),
   so `-windowed -ResX -ResY` and `Fullscreen=False` in OLSystemSettings.ini are overridden. Default profile is fullscreen.
6. `<game dir>\OLGame\Config\OLEngine.ini` has `bPauseOnLossOfFocus=TRUE`.
7. **Blocker for bot testing:** with port 17777 / room `DEFAULT` pointed at the running test relay, every launch
   that did not get the foreground (2 instances, or 1 instance launched from a background shell, with or without
   `-nosteam`, with `bPauseOnLossOfFocus=FALSE`) stops logging right after `Resizing to WxH (fullscreen)`:
   process responding, CPU busy, no `HELLO` sent, relay saw nothing.
8. `-EXEC=<file>` resolves to `<game dir>\Binaries\<file>` (`..\..\Binaries\` from Win64). It runs at ~14.1 s,
   BEFORE the profile resize at ~14.9 s, so `setres 960x540w` in it gets overridden. Last run ended here.

Guesses (unverified):
- Item 7 is exclusive fullscreen D3D9 being minimized when the window has no focus, so the
  network tick never runs. If true, a host who alt-tabs also stops syncing.
- The fix for bots is to get windowed mode before the profile is applied: a profile with fullscreen off
  in a `-nosteam` local profile, or a delayed `setres` (OpenOL ImGui F6 or console after load).

### Test run 2: one client, -nosteam, gamma screen, protocol (2026-10-20, later)

Launch as before plus `-nosteam`, room `DEFAULT`, port 17777, `harness\relay_host.exe` (embedded relay source).
Input driven with `harness\olwin.py` (focus window + real keyboard input via SendInput; the game ignores
PostMessage key events).

Observed:
1. **Item 7 above was a misdiagnosis.** The log file is written in big delayed chunks, and `relay_host` prints
   nothing per packet, so "no HELLO, relay saw nothing" was wrong. Once the log flushed: `HELLO #1` at 16.65 s,
   `Connected! PlayerID=1000` at 16.88 s, while the window was fullscreen and showing the gamma screen.
2. A fresh profile (`-nosteam` defaults) opens the **gamma calibration screen** over the main menu
   (`OLUIFrontEnd.uc` ~341, `PSI_GammaInitialized`). Keyboard: Down, Enter accepts it.
3. The command-line MP map lands in the **main menu** (`DLC_Intro_Persistent` streams `dlc_mainmenugame`), MP HUD
   "OpenOL: 1 player" in the corner.
4. Options -> Graphics -> FullScreen off, Resolution 800x600, Apply: log `Applying resolution 800 x 600
   (Fullscreen: no)`, window 816x639, and the profile is saved to `<game>\OLGame\SaveData\Player1.olprofile`
   (379 bytes, `[OnlineSubsystemPC] ProfileDataDirectory=../../OLGame/SaveData`). Next `-nosteam` launches should
   start windowed (not yet re-run).
5. Console (Tilde) did not open in the main menu.
6. **Reconnect loop:** `Connected!` then a new `HELLO` every ~30-35 s, PlayerID 1000, 1001, ... 1009.
7. Cause (source reading, both sides):
   - Client 2.1.0 wire format (`MultiplayerLink.cpp` `FMpConnection::SendBinary`): `[channel][type][player_id LE4][payload]`;
     PING = `[0x00][0x02][id][ms]`. Client drops the link if nothing is received for 30 s (`Tick`, heartbeat).
   - Embedded relay in the OpenOL repo (`Development/External/openol_relay/src/server.c` `handle_packet`) parses the
     OLD format `[type][player_id LE4]`: it reads the player id from the wrong bytes, finds no player, drops every
     binary packet (no PING echo, `last_seen` never updated) and times the player out after 30 s (`TIMEOUT_SEC`).
   - The standalone relay https://github.com/ShyKiss/OpenOL-Relay at `b9def512` ("Update to 2.1.0") already has the
     channel header (`/* Client->server 2-byte header: [channel][type][player_id LE4] */`).
   - Note: even there, `ENEMY_LOC` is 0x08 in the relay but 0x04 in the client's `PacketChannels.h`
     (and ENEMY_DOOR_* differ). Not checked further.
8. Not yet known: whether the relay compiled INTO the shipped `OLGame.exe` (Start Server) is the old source
   (strings check inconclusive). If it is, hosting via "Start Server" cannot sync anything, which would explain
   OpenOL issue #8. Test: two instances, one host via the menu "Start Server".

Side observation: `FMpConnectionTicker::IsTickableWhenPaused()` returns FALSE (`Multiplayer.h` ~172), so a paused
game (pause menu, `bPauseOnLossOfFocus`) stops HELLO/PING/heartbeat. Not observed as a problem yet.

### Test run 3: two clients on the standalone OpenOL-Relay 2.1.0 (2026-10-20, 23:30-23:47)

Setup: OpenOL-Relay `b9def512` (https://github.com/ShyKiss/OpenOL-Relay), `src/{main.cpp,server.c,cli.c,db.c}`
built with MinGW (`gcc -c` the C files, `g++` main.cpp, `-lws2_32`), run as
`OpenOL_Relay.exe --port 17777 --name CoopTest`. It creates only room `PUBLIC` (main.cpp, when its db has no
rooms), so the test `OLMultiplayer.ini` was set to `RoomCode=PUBLIC`. `hello_probe.py`: PUBLIC -> READY,
DEFAULT -> HELLO_FAIL (2/2).
Two `-nosteam` instances of the same test copy (shared windowed profile from test run 2), driven with `olwin.py`.

Observed:
1. **New launch blocker and fix:** started from a background shell, the window comes up minimized and engine init
   stops after `Wwise(R) SDK Version ...` (CPU flat, log empty, waited >2 min). Minimize + restore of the window
   (`olwin.py wake <pid>`) unblocks it at once (log gap 13.5 s -> 88.1 s = time until the wake).
2. Relay: `ID=1001 joined room 'PUBLIC' (1 players)`, `ID=1002 joined ... (2 players)`, both `identified as 'Player'`;
   no `timed out` for the rest of the run (~13 min). Client logs: exactly 1 `HELLO` and 1 `Connected!` each.
   **The reconnect loop from test run 2 is gone.**
3. Both HUDs: `CoopTest: 2 players`, ping 15-17 ms, player list `Player (You)`, `Player`.
4. Both: New Game -> Main -> Normal -> Continue -> intro letter -> asylum gate (main campaign start). Both logs:
   `KISMET: Remote event Car_Ride activated for 2 recipients`.
5. **Hero sync works:** B held W for 3 s; A's window shows B's character model walking ahead at the gate
   (screenshot kept locally).
6. Menu quirks for bots: on the intro text screen the Continue button sometimes has no focus; a mouse move/click
   over it gives focus, then Enter works.

Not tested yet: doors, enemies, pushables, cutscene sync, story progress, saves, death/respawn, two PCs over the
internet, in-menu "Start Server" (embedded relay).

### Test run 4: door and enemy sync with bots (2026-10-21 00:04-00:14)

Setup as test run 3, plus a logging proxy between clients and relay (`harness/udp_tap.py`, clients on UDP 17778,
relay on 17777). The proxy decodes the 2.1.0 wire format and writes `state.json` (hero x/y/z/yaw per client, doors
by position, enemies by name). Bots: `goto.py` (walk to x,y: aims with relative mouse moves, calibrated from the
yaw in `state.json`; walks in <=0.6 s bursts; if it moved <25 units it backs off and sidesteps, alternating sides;
gives up with `BLOCKED` after 5 tries; aborts with `STALE` if the state is >3 s old), `turn.py` (closed-loop turn),
`click.py`, `look.py`, `hold.py`. Both clients loaded checkpoint `Admin_MainHall` from the OpenOL F6 overlay
(Checkpoints tab).

Observed:
1. **Checkpoint loads are not synced.** Loading `Admin_MainHall` on A left B at the gates; B had to load it too.
2. On level load each client sends `DOOR INIT` for every door (47-52 here, with x/y/z and angle). The relay answers
   with `DOOR ANGLE` snapshots.
3. **Door A->B works:** A closed the open door at (-8748,-788,0) (the game prompted "Press LEFT MOUSE BUTTON to close").
   Proxy: `C>R A DOOR CLOSE` -> `R>C B DOOR CLOSE`. B's view: the mesh door closed across the doorway, A behind it
   (before/after screenshots kept locally).
4. **Door B->A works:** B closed it again later; on A's screen the door was closed and the prompt changed to "open".
5. After each received CLOSE/OPEN the receiver sends the same packet back once (echo). No loop was seen (1 echo each).
6. The door opened once with no click. A's client sent `OPEN` while B walked past it (00:09:36). Guess: B's remote
   model on A's client pushed the door. Not confirmed.
7. **Enemy A->B works for spawn and position:** a Soldier spawned on A (overlay Spawns -> Enemies -> + -> Spawn,
   "Should Attack" off, then left click in game to place, right click to leave placement mode). Proxy: 1 `ENEMY SPAWN
   OLEnemySoldier_0` A->B, then `ENEMY LOC` 5263 sent by A / 5263 delivered to B in ~1.5 min. Both screens show the
   same enemy at the same place near A (dark, screenshot kept locally).
8. Restarting the proxy changes the client's address at the relay: the relay times out the old address and the
   clients re-HELLO and get new IDs (1001/1002 -> 1003/1004 -> 1005/1006). They came back by themselves in <30 s.
9. Harness problems found and fixed: the proxy crashed when a bot read `state.json` during `os.replace`
   (Windows PermissionError), and a bot then walked on stale data. A bot walking straight got stuck on furniture and
   on the other player.

Not tested yet: enemy chasing/attacking a player, enemy AI owned by B, enemy killed/despawn, door lock/unlock,
pushables, cutscenes, story triggers, death/respawn, two PCs.

### Test run 5: attacking enemy, damage, death, respawn (2026-10-21 00:17-00:35)

Setup as test run 4. The proxy now also decodes hero health (i16 @38) and special move (u8 @30) from `HERO_STATE`,
and enemy mode (u8 after yaw/velocity) from `ENEMY LOC`, and logs every change. `harness/watch.py` writes a 1 s
timeline (hp, special move, positions, enemy->hero distances, STALE flags) plus a screenshot every 2 s.
A = enemy owner (spawned the Soldier from the F6 overlay, "Should Attack" on, no weapon), B = the other client.
Both on checkpoint `Admin_MainHall`. To get deaths, Max Health was set to 40 in the overlay (Player tab).

Observed:
1. **The owner's enemy attacks both players.** The enemy AI runs on A's client only; on B it is a copy.
   - B attacked (00:23:15): enemy mode 0->2, `ENEMY SMT` A->B, `HERO 0x07` (player event) A->B, B hp 100->49 and
     B pushed ~250 units. B's own special move stayed 0 (no grab animation on that hit).
   - A attacked (00:28:05, again 00:28:56): A special move 0->62 (grab) -> 67 (throw, z=41) -> landed, hp 100->49.
   - After A's death the remaining enemy left the dead/respawned B and walked toward A
     ((-8649,166) -> (-8732,132)), grabbed A, hp 100->49. So it chases.
   - One Soldier hit = 51 damage. Regeneration about 2 hp/s. After a hit the enemy goes back to mode 0 within ~4-12 s.
2. **What the other player sees:** the enemy copy moves and plays its attack/grab/kill animations on the other
   screen; the victim's name turns red in the other player's HUD while hurt, the victim's own screen turns red.
   B saw the Soldier lift A during the kill; A saw the Soldier over B's body.
3. **Death of the owner (A, 40 max hp):** 00:32:03 grab 62 -> kill move 68 -> hp 40->5->0 (00:32:07).
   00:32:15 A sends `ENEMY DEL` for its enemies (both Soldiers removed on B too), `LIFECYCLE DIED` and
   `LIFECYCLE RESPAWNED`, both delivered to B. A respawned with 100 hp (the max-health 40 setting was reset).
4. **Death of the non-owner (B, 40 max hp):** 00:34:31 two `HERO 0x07` A->B, B special move 62 -> 68, hp 40->5->0
   (00:34:35). 00:34:43 B sends `DIED` + `RESPAWNED`, delivered to A. B respawned at the checkpoint start
   (-9272,2) with 100 hp. A's enemy was NOT removed and kept fighting (attacked A next).
5. Asymmetry: owner death wipes the owner's enemies for everyone; non-owner death does not touch them.

Harness changes (the bots now check state more often and handle obstacles):
- `goto.py` decides "stuck" by progress toward the target (no new best distance for 2 bursts), not by distance moved.
  Bouncing between two pieces of furniture used to reset the counter forever. Detours grow (back off, sidestep
  right/left 0.4 s * n, step forward); after 6 detours it reports `BLOCKED` with position and best distance.
  Long routes go through waypoints.
- `goto.py` stops with `HIT` when the hero's hp drops or a special move starts. Before, it treated a grab and throw
  as an obstacle and walked off.
- `setslider.py` sets ImGui slider values (ctrl+click, type).

Not tested yet: enemy owned by B, two enemies owned by different clients, weapons (Soldier with weapon, other types),
hiding (lockers/beds), story triggers/cutscenes, checkpoint progress after a death, two PCs.

### Test run 6: enemy owned by B, enemies of two owners at once (2026-10-21 00:36-00:45)

Setup as test run 5 (relay, proxy, `watch.py`). Both on `Admin_MainHall`. Soldiers spawned from the F6 overlay
with "Should Attack" on.

Observed:
1. **B-owned enemy works like an A-owned one.** B spawned `OLEnemySoldier_0` near A: `SPAWN` B->A, it attacked A
   at once (A hp 100->49), and A's screen showed it. Later it also hit its owner B (hp 100->49).
2. **Two owners at once:** A spawned `OLEnemySoldier_1` while B's `_0` was alive, then B spawned `_2`.
   The proxy got position updates for all three from their owners. All four cases worked with both owners'
   enemies alive: B's enemy hit A (00:42:27) and B, A's enemy hit B (00:43:3x) and later A.
3. Freshly spawned enemies attack at once if a player is near the spawn point. Otherwise they stand in mode 0
   until a player comes within ~100 units (not tuned/measured further).
4. **B (non-owner of `_1`) killed by A's enemy:** B's death sent `DEL` for B's own enemies `_0` and `_2` only.
   A's `_1` stayed, walked to A and killed A too. A's death sent `DEL _1` only. So owner death removes exactly that
   owner's enemies, for everyone. This matches test run 5 and explains its "asymmetry".
5. **Names never collided.** Each client numbers enemies with the engine's per-class counter, and the remote copy
   ("dummy") of the other player's enemy uses up a number in the receiver's world. So the next local spawn gets a
   new number: A got `_1` after B's `_0`, B got `_2`. Even confirming both spawns 0.27 s apart gave
   A `_3` and B `_5`.
6. Source reading: the client looks up remote enemies by (name, owner id) (`EnemyChannel.cpp` `ApplySpawn`,
   `ApplyDel`, `ApplySmt`), so equal names from different owners would be safe on clients. The relay's snapshot
   table for late joiners keys by (room, name) only (`server.c` `enemy_find/enemy_alloc/enemy_remove`). Two owners
   with the same name would overwrite each other there. Not observed (see 5). The relay also never stores
   `ENEMY_LOC` snapshots because its `ENEMY_LOC` is 0x08 and the client sends 0x04 (test run 2 item 7), so a late
   joiner would get spawn + SMT but no current position.

Not tested: late join while enemies are alive (the relay snapshot path), story/level enemies (spawned by the map
on both clients at once), weapons, other enemy types, two PCs.

### Test run 7: story sync with a scripted fake peer (2026-10-21 00:47-01:03)

Method change (bot walking to find triggers was too slow):
`harness/fake_peer.py` joins the relay room as a third player (text HELLO, binary `SRV_HELLO` nick, `SRV_PING`
every 5 s) and injects `WORLD_TRIGGER_ACT` packets. Its `sweep` mode sends
`<level>.TheWorld:PersistentLevel.Main_Sequence.SeqEvent_Touch_N` for N in a range, one every 3 s, then maps
the receiving client's log (game time + "Log file open" time) to each send. The two real clients are receivers.
The receiver logs `[TA] ForceActivateTouchEvent: <name> ... MaxTriggerCount=..` (OLPlayerController.cpp ~1773),
`KISMET: Remote event <name>`, `--- SAVING CHECKPOINT "<name>" ---`, `GS - Applying checkpoint <name>`.
Level names come from trigger paths the proxy saw and from `CookedPCConsole` file names (lower-cased).
The proxy now decodes WORLD packets (trigger path, matinee path/pos/rate).

**Needed `-FORCELOGFLUSH`:** without it the client log reached the disk ~3+ min late, so the first sweep matched
nothing.

Observed:
1. At game start each client touches `asylum_exterior-ld...SeqEvent_Touch_4`; each sends `TRIGGER_ACT` and the
   other receives it. Matinee state (`SeqAct_Interp_0`, rate 1.0) is sent both ways.
2. **Checkpoint loads are not synced, and triggers are applied out of context.** When A loaded `Admin_Explosion`
   (F6 overlay), A's touches of `center_block_explosionroom-art`/`floors_snd` triggers reached B, who was still at
   the gates. B's log: `[TA] ForceActivateTouchEvent: SeqEvent_Touch_0/3/7 ...`. The objects exist on B (same
   persistent map `adminblock_persistent`), so B ran another room's story events.
3. **Sweep of `center_block_sript` Touch_0..30 with both clients in `Admin_Explosion`:** 13 triggers existed and were
   force-activated on BOTH clients, same order. Story events seen on both: `Patient_Running_After_AirVent`
   (Touch_2), `SOLDIER_MEZZANINE_GRAB` (Touch_8, plus warning "SeqAct_Interp_10 has no groups that reference
   connected Actor 'MultiplayerController_0'"), `SwapWindowsEntrance` (Touch_9), then `WheelchairReady`,
   `PriestDone` and **`SAVING CHECKPOINT "Admin_MainHall"` on both** (around Touch_23..25).
   So one player's trigger moves BOTH players' story and checkpoint forward. Both players stayed where they were
   (no teleport), i.e. the story skipped ahead without them. Saves went to the test copy's
   `OLGame\SaveData\OL_<date>.sav` (-nosteam), Steam Cloud unchanged.
4. Sweep of `center_block_basement-ld` Touch_0..30 in `Admin_Electricity`: 8 triggers activated on the receiver,
   no Kismet remote events, no enemies.
5. **Map enemies are duplicated.** Both loaded `Prison_IsolationCells02_Soldier`. A loaded first and sent
   `SPAWN OLEnemyGenericPatient_0`. B loaded 14 s later and sent `SPAWN OLEnemyGenericPatient_1` (its own AI copy
   of the same map patient; the number moved because A's dummy took `_0` in B's world). Both kept sending LOC for
   the same spot (2734,1166,1351), mode 0. Derived from the source (`EnemyChannel.cpp` TickSend sends every
   local `AOLEnemyPawn`; `ApplySpawn` makes a dummy per (name, owner)): each client has its own AI patient plus a
   dummy of the other's in the same place. Not checked on screen (the patient was ~3000 units away).
   The relay also re-sent A's `SPAWN _0` to B on B's level load (relay snapshot).
6. No Soldier (despite the checkpoint name) was in the world at load; it must appear later via script.

Not tested: cutscene (matinee) start/stop sync while both watch, the same trigger touched by both at once,
MaxTriggerCount handling (count sent as 0 by the fake peer), killing/despawning a duplicated map enemy, two PCs.

### Test run 8: duplicated map enemy on screen, who it attacks (2026-10-21 01:06-01:26)

To cut manual walking and menu use, a new one-command driver `harness/ol.py` (details in its docstring):
`up` (relay + proxy, detached, HELLO probe), `launch N <map>` (clients started straight on a map by URL, no menus;
waits for the Wwise log line, then wakes the window, retries; tiles windows; maps each client to its proxy port),
`cp <idx> <name>` / `cpall <name>` (loads a checkpoint through the F6 overlay: Checkpoints tab, search box, Ctrl+A+Delete,
type the full name, click the single result), `survey <idx> <cp...>` (hero spawn position and enemies per checkpoint),
`status`, `down`. A map URL alone does not pick a checkpoint (`prison_persistent#<cp>` loads the map at the default
spawn; no URL option for checkpoints exists in the source).

Observed:
1. Survey of the 13 Prison checkpoints (`survey_prison_checkpoints.log`): hero spawns are fixed per checkpoint. Only
   `Prison_OldCells_PreStruggle2` spawns on the patient's floor close to it: hero (2902,225,1348), patient
   `OLEnemyGenericPatient` at (2734,1166,1351), 956 units away. Both players spawn at exactly the same point.
2. Fresh start, both clients `cpall Prison_OldCells_PreStruggle2`: proxy shows TWO patients at the same spot,
   `_0` sent by A and `_1` sent by B, both mode 0.
3. A walked to the patient (`goto.py`, no menu). On screen (brightened frame 01:24:14, kept in `evidence`): **A sees two
   patients on top of him at once; B, still at the spawn, sees two silhouettes next to each other in the corridor.**
4. **Who attacks whom:** each patient attacks the nearby player, and both copies attack the same victim.
   A took hits from `_1` (B's copy: `HERO 0x07` sent by B's client to A, 01:24:18 and :22) and from `_0` (A's own
   copy, 01:24:20 and :23): hp 100->70->...->10, then grab (special move 69) and death 01:24:28. So the damage was
   about double compared to one enemy.
5. After A's death the B-owned copy walked to B (2903,233) and killed B at 01:24:47 the same way.
6. On A's death: `ENEMY DEL _0` (A's own copy removed), `DIED`+`RESPAWNED`; the relay replayed B's `SPAWN _1` to A twice and
   A's reloaded checkpoint spawned a fresh `_2`. So after a respawn the map has again one local copy plus the other
   player's copy.
7. The head-tracking target in `ENEMY LOC` differs between the copies (each copy looks at the player it considers
   closest), which matches two AIs choosing targets independently.

Not tested: what happens when only one client has the enemy loaded, whether killing one copy removes the other,
other enemy types, two PCs.

### Test run 9: first assert-based suite (2026-10-21 16:40-17:10)

`harness/suite.py` (README table). First runs found: (1) scorer bug: the relay log shows the first id (the HELLO probe from `ol.py up`)
timing out after 30 s because it never pings; counted as a real timeout, fixed and covered by a scorer self-test (S0).
(2) **Crash:** starting a client on `adminblock_persistent` and then loading `Admin_Explosion` ended in a Windows error box
`Assertion failed: Sequence != RootSequence [UnWorld.cpp:1693] SubLevel is streaming in its parent`; the log goes silent. The previous manual
runs reached the admin block only through other maps. The suite now starts on `prison_persistent` and jumps by checkpoint, and every
assert checks for crash dialogs. Result of two full runs: 7 PASS, 2 XFAIL (A4 map enemy sent by both clients, B3 trigger of another
level fires on both), 365 s each, stable.

### Test run 10: suite extended to doors and player-spawned enemies (2026-10-21 17:00-18:10)

Added: door injection (`fake_peer.py door`), tests A6/A7, group C (C0..C6), `ol.py maxhp/spawn/nudge/level`, hero pitch in the proxy state.
What the first group C runs showed (all handled in the suite, none an OpenOL bug unless noted):
1. The overlay's console has no visible input line in an 800x600 window and blind typing did nothing; `KillPlayer` could not be used. Death
   comes from the enemy plus Max Health 40 on both players (one hit of 51 kills).
2. Spawn placement follows the crosshair: with the view level the ghost lands 1100 units away behind a wall; looking down to the limit puts it at
   the feet. A restored view overshoots, so the camera is leveled closed loop on the pitch the proxy decodes (HERO_STATE bytes 18-19).
3. A Soldier spawned inside the padded start cell of `Prison_Start` never attacked (no path?); in `Admin_MainHall` it does.
4. An idle enemy 70 to 270 units from a motionless player does not attack; a short step makes it switch to mode 2 (about 77 to 190 units).
   After one attack it goes back to mode 0 within seconds and needs the players to move again.
5. A death of the owner deletes the owner's enemy (DEL, relayed); a death of the non-owner leaves it until the owner dies (C5, C6 both seen).
Two full runs: 16 PASS, 2 XFAIL, 562 s and 601 s.

### Build check of OpenOL (2026-10-21)

Read `docs/BUILD.md`, the CI workflow and the container recipe of OpenOL (commit 2ab9ecaa) and the local toolchain.
1. The repo holds only the files OpenOL changed: `Development` 25 MB, `OLGame` 8 MB, `Binaries` 3.6 MB. The documented build needs the full Outlast source
   tree (about 36 GB: `Binaries` 5 GB, `Development/External` 30 GB, `Development/Src` 8.6 GB, `Engine`, `OLGame` 22.6 GB), which
   the docs say they do not provide ("We won't be providing any links to the Outlast source here"), plus Visual Studio 2012 and the DirectX SDK June 2010
   (Windows, `DXSDK_DIR`), or Wine 11 + the same on Linux. None of this is on the test PC (no VS 2012, no DXSDK, no `cl`/`msbuild`).
2. Consequence: the native code that causes the open XFAILs (`EnemyChannel.cpp`, `WorldChannel.cpp`) cannot be rebuilt here, and we will not look for the
   proprietary source. What can be built without it: the standalone relay (done, MinGW) and UnrealScript-free tools.
3. Config switches in `OLMultiplayer.ini` exist without a rebuild: `SyncEnemies`, `SyncMatinees`, `SyncInteractable`, `SyncPickups` (all `True`
   by default; read in `FMpConnection::LoadConfig`). `SyncMatinees=False` stops a client from sending its trigger events; `SyncEnemies=False` stops sending and
   receiving enemy packets. They are on/off switches, not ownership, so they do not fix duplicates or out-of-context triggers; not tested.
4. Fix path for A4 (one owner per map enemy) and B3 (area context for triggers): a source change by someone with the full source; a patch of the shipped exe (done later, test run 21);
   or a relay-side filter (limited: the relay cannot tell a map enemy from an overlay-spawned one, it only sees names and positions).

### Test run 11: relay-side trigger filter (2026-10-21 19:20-20:15)

Because the native fixes cannot be built (see build check), the trigger problem was attacked in the open-source relay. Patch
`patches/0001-openol-relay-trigger-radius.patch` (against OpenOL-Relay `b9def512`, dry-run applied to pristine sources): `--trigger-radius <units>`; the relay
keeps each player's last `HERO_STATE` and forwards `WORLD_TRIGGER_ACT`/`WORLD_TRIGGER_FIRE` only to players within the radius of the sender (unknown position: forward).
1. `relay_filter_test.py` (no game): T1 near and unknown-position peers get the trigger, the far one does not; T2 off = everybody; T3 door packets are not
   filtered; T4 the upstream relay violates T1 (twin); T5 a sender without a position reaches everybody. 5 of 5.
2. In game (`suite.py B`, radius 3000, one scripted peer): a trigger sent by a peer 50000 units away is ignored by both real clients (B3 PASS); the same trigger from a peer
   at the players' position fires on both (B4 PASS). On the upstream relay B3 fires on both (XFAIL), so the test can fail.
3. **Test bug found by that twin:** the first version of B3 used a one-shot trigger (`MaxTriggerCount=1`) after B4 had used it up, so it "passed" on the upstream relay too. B3 now uses a
   repeatable trigger (`center_block_sript` Touch_3), runs first, and waits for the previous Kismet chain.
4. Full suite: 18 PASS, 1 XFAIL (A4), 618 s.
Limits: distance is a heuristic for "same area" (vertical floors, big rooms), a far player misses story triggers until they meet, checkpoint loads are still not synced, enemies are not filtered.

### Test run 12: relay-side enemy dedupe rejected, SyncEnemies=False measured (2026-10-21 20:00-20:40)

1. **A relay filter for duplicated map enemies would make things worse.** The damage of an enemy copy is sent by the client that runs that copy: in the duplicated-patient run
   (test run 8) `HERO 0x07` from client B follows the mode change 0->2 of B's own copy at once (`HeroChannel.cpp` `SendPlayerHit`). Hiding the copy from the
   other player at the relay would leave an invisible attacker that still hurts him, because the AI of the copy keeps running on its client.
2. **`SyncEnemies=False` (config, no rebuild) on both clients, suite group D:** no `ENEMY` packet crosses the relay (D1), a Soldier spawned on one client is not mirrored (D3), the
   other player takes no damage from it (D4), and the enemy does attack on its own client (D5: `SMT_AttackGrab`/`SMT_KillHero` in its log).
3. Surprises: with `SyncEnemies=False` the local enemy's attack did not change A's own hero state (no hp or special-move change in the proxy, no death) although
   the log shows grab and kill animations, probably because it targets the remote-player dummy of B standing at the same spot; `HERO 0x07` events (grab/kill visuals) were
   still sent by A and relayed to B (4 in 90 s) but B ignored them (hp unchanged). `SendPlayerHit` is gated by `SyncEnemies`, the other player events are not.
   Not checked: who is targeted when the players stand apart.
4. Test notes: D3 to D5 run in `Admin_MainHall` because a Soldier does not attack in the prison corridors (as in `Prison_Start`); D5 exists so D3/D4 cannot pass vacuously.
   Full run of A, B, C: 18 PASS, 1 XFAIL (A4); D: 7 PASS.

### Test run 13: relay enemy packet ids (2026-10-21 20:45-21:10)

Game-free test with fake peers (`relay_filter_test.py` T6/T7): an owner peer sends `ENEMY SPAWN` (1) and `LOC` (4, as the 2.1.0 client does); a late joiner sends
`WORLD_REQUEST_ENEMIES` (0x0A). Upstream relay: the joiner gets only `SPAWN` (T7 shows the missing `LOC`), because the relay stores `ENEMY_LOC` = 0x08. Patched
(`0002`): `SPAWN` and `LOC` (T6). The relay uses `ENEMY_LOC` nowhere else, and the shifted door ids not at all, so the change is local. Patches split into
`0001` (trigger radius) and `0002` (ids), applied in order to pristine `b9def512` sources they reproduce the patched tree byte for byte.
In game the effect is small: the enemy's owner sends `ENEMY_LOC` continuously, so a late joiner gets positions within a moment anyway; no in-game test added.
Tooling note: on Windows, Python text-mode writes turned `server.c` into CRLF and the first patch was 2900 lines; patches are now generated from LF files.

### Test run 14: relay shared-state tests without the game (2026-10-22)

`relay_state_test.py` (fake peers): S1 a second `DOOR_LOCK` on a locked door gets the server packet `SRV_DOOR_DENY` (0xE4), the lock is not relayed (the first version of the
test looked for a door-channel type 8 and failed: the test, not the relay, was wrong); S2 lock allowed after the owner unlocks; S3 `DOOR_INIT` first write wins, never relayed, replayed
to a late joiner as `DOOR_ANGLE` with the first angle; S4 `DOOR_OPEN` live and in the late-join snapshot; S5 a second pusher gets `PUSH_DENIED`, the owner is not disturbed; S6 push allowed after release; S7 `PUSH_INIT`
never relayed, snapshot keeps the first position with `bPushing` cleared; S8 twin: a blind mirror relay fails S1. 8 of 8 on both the patched and the upstream relay (upstream behaviour). Added to the suite as group P with T1-T7
(64 s, 15 checks + self-test). In game the pushable and door-lock paths are still untested (no pushable was moved by any client in the runs).

### Test run 15: pushable objects in game (2026-10-22)

`pushsurvey.py` (one client, load each checkpoint, count `PUSH_INIT` it sends) over 25 checkpoints: pushables announced only in the male ward group (`Male_Start`: 2 objects at
(-3626,-7763,1000) and (-3799,-11516,1000), later checkpoints 3 to 4, `Male_Cafeteria` 1); none in Admin, Prison, Courtyard, Sewer_start, Revisit, Lab. Names with `_Doc_`/`_RM_` are notes/recordings, not checkpoints
(`cp` times out after 90 s); real names come from the overlay's Checkpoints tab (`Male_Start`, `Male_Chase`, `Male_Torture`, `Male_Elevator`, `Male_Priest`, `Male_Cafeteria`, `Sewer_ToMaleWard`, `Courtyard_FemaleWard`...).
The proxy now decodes `PUSH` packets (key, displacement, pushing). A scripted peer (`fake_peer.py push`) sent a rising `PUSH_STATE` and a stop packet for the object 764 units from the heroes: the relay forwarded it to both real clients
(`R>C` to both ports) but no visible displacement showed in the facing client's window (before/after frames equal except light flicker), so whether the client applies a remote push is **unverified** (the receive code clamps
or ignores it? unknown; no log line). Second crash of the same engine assertion (`UnWorld.cpp:1693`, "Sublevel is streaming in its parent") when jumping by checkpoint from `Lab_Soldierdead` to `Prison_Start`.
Pushables are only announced after some checkpoint loads inside the male map, not on the first load from another map (not understood).

### Test run 16: the embedded relay of the shipped OLGame.exe ("Start Server") (2026-10-22)

The overlay's Relay tab (F6) starts the same embedded relay the Multiplayer menu uses (`GRelayThread.StartRelay`): fields port (default 7777), name, DB path, button Start Relay; its log is shown in the tab.
Setup: only the logging proxy (UDP 17778 -> 17780), client A on `prison_persistent` with `RoomCode=DEFAULT` (the embedded relay only has room `DEFAULT`), Start Relay on port 17780, then client B.
1. **The shipped embedded relay speaks the 2.1.0 format.** A joined (`ID=1000`, identified), stayed connected for more than 70 s with exactly one `Connected!`, no timeout; B joined, both heroes streamed fresh
   state for another 40 s, no reconnects. So the reconnect loop of test run 2 is a property of the *embedded relay source copy in the repo*, not of the relay in the release exe (the exe was
   built from a newer copy). Problem 1 of the README is limited to the source copy accordingly.
2. **The shipped embedded relay has the `ENEMY_LOC` id problem too** (game-free fake peers, port 17780, room `DEFAULT`): a late joiner's enemy snapshot contains `SPAWN` (1) but no `LOC` (4).
   So relay patch 0002 matters for "Start Server" hosts, but they cannot apply it without a rebuild; a standalone relay can.
3. The overlay start needed no Steam. The Multiplayer menu button (`Press_CreateServer` = StartRelay + ConnectLocal + open map) and the Steam invite were not clicked; upstream issues #7/#8 may be about those or about Steam.
   Test setup left `relay_db.json`-style files only in the test copy.

Group E automates test run 16: `suite.py E` (196 s): E1 to E4 PASS, E5 (enemy position in the late-join snapshot of the shipped relay) XFAIL. Menu note: the main menu ignores mouse clicks and its
keyboard focus is ambiguous after dialogs (a Down+Enter sequence landed on Options, then New Game, then Difficulty), so menu-driven tests were not attempted; the overlay is the reliable way to start things.

Crash repeat (2026-10-22): loading `Lab_Soldierdead` and then `Prison_Start` through the overlay on one client crashed with `Assertion failed: Sequence != RootSequence [UnWorld.cpp:1693]` ("Sublevel is streaming in its parent") on the first
attempt, again in the second session: deterministic for this jump (the other cross-map jumps tried, Prison <-> Admin <-> Male, did not crash). The start map `adminblock_persistent` crashes with the same assertion. It is a stock engine
assertion in sublevel streaming, reached by the overlay's checkpoint jumps (`StartNewGameAtCheckpoint`); not shown to be multiplayer related (all clients run `MultiplayerGame`; a single-player run was not tried).

Group F (late join with real clients, `suite.py F`, 169 s): A loads `Admin_MainHall` and spawns a calm Soldier (Should Attack off, otherwise the enemy kills the owner and
deletes itself), then client B is launched after that and loads the same checkpoint: B receives the `SPAWN` (F1), a position (F2, from the owner's continuous `LOC` stream, so the relay's `ENEMY_LOC` id problem does not show here), streams state (F3), A keeps its first connection (F4). All PASS.

Cutscene sync, attempted and dropped (2026-10-22): a suite check "an injected trigger starts the cutscene on both clients" failed (0 of 2 clients reported a new matinee) because the premise was wrong.
`MATINEE_STATE` packets list matinees with position and play rate and are sent by each client when it loads level content (the proxy's first report of each matinee path coincides with checkpoint/level
loads: `asylum_exterior-art Interp_0`, `center_block_explosionroom-art Interp_1`, `prison_01-ld Interp_27`...), not when a trigger starts one; the `Touch_4` trigger itself fired (`ForceActivateTouchEvent`) but
started nothing visible in that context. Receivers apply a reported position only if the matinee is not playing locally (`WorldChannel.cpp`). Testing cutscene sync needs a real cutscene
and a visual or state oracle; not done. The failing check was removed from `suite.py` rather than kept as a vacuous XFAIL.

README build check (2026-10-22): a fresh copy of `OpenOL-Relay` `b9def512/src`, both patches applied in order (`patch -p1 -d src`), compiled exactly as in the README
(`gcc -O2 -c server.c cli.c db.c`, `g++ ... -lws2_32`), reproduces the patched tree byte for byte; `relay_filter_test.py` 7/7 and `relay_state_test.py` 8/8 on that binary.

### Open hypothesis (2026-10-22, unverified)

The duplicated map enemies (A4, test run 8) may occur only after a checkpoint is loaded through the F6 overlay (`StartNewGameAtCheckpoint`: `ClearAllProgress`,
`SaveCheckpoint`, `RestartPlayer` on every client), and not during normal story progress. All duplicate evidence so far comes from overlay loads. Not
refuted: story triggers are synced, so both clients would run the same Kismet and each spawn its own copy; also not confirmed. A trigger sweep in test run 7 did not spawn any enemy,
so it cannot decide. Reasoning (derived, not tested): a plain map start (`prison_persistent` by URL) spawned no patient until a checkpoint was applied, so map enemies come from sublevels that
stream in per client; players who walk together would load the same sublevel on both clients and get one copy each, while a player who stays far away may never load it (then no duplicate). So the guess is plausible only for
players who are apart. Test: play both clients from New Game to the first map enemy without the overlay and look for two `ENEMY SPAWN` of one class at one place.

### Derived
- Story sync in OpenOL 2.1.0 = "any trigger anyone touches fires for everyone, wherever they are". Fine when the
  players stay together; breaks when one is in another area or loads another checkpoint. Map enemies are not
  owned by one client, so every client simulates and broadcasts its own copy.
- The standalone relay 2.1.0 is the working server for OpenOL 2.1.0 clients; the embedded relay sources in the OpenOL
  repo are not (protocol mismatch, test run 2).

### Next steps
1. Test in-menu "Start Server" (embedded relay in the shipped exe) the same way. If it loops, fix = rebuild the
   embedded relay from OpenOL-Relay `server.c`, or host with the standalone relay.
2. Bot checks still open: story triggers/cutscenes, checkpoint/level change sync, enemy owned by B,
   hiding from an enemy, what happens to level state (doors, enemies) after a player's death reload.
3. Two-PC test with two people only after the bot checks: the host runs OpenOL_Relay with UDP 7777 forwarded,
   the other player sets IP/port/room `PUBLIC` in Multiplayer settings.
4. Findings an upstream report would cover (none filed): room `PUBLIC` vs `DEFAULT`, silent HELLO_FAIL, embedded relay protocol
   mismatch, ENEMY_* id mismatch, minimized-window init stall.
5. Not done at this point: `gh` (not installed), claim-a-game issue, fork/PR (done later: claim issue #133, the fork, this write-up's PR).

### Tentative path
Revive/extend OpenOL (playbook/reviving-abandoned-mods.md): it is recent, has a working
networking layer and a release; first test with two local instances, not with people.

### Test run 17 (2026-10-22): guide dry run over the LAN address
Relay started as the player guide says (`OpenOL_Relay.exe --port 7777`, default bind `0.0.0.0:7777`, room `PUBLIC`); one client with `IP=<this PC's LAN address>` and `UdpPort=7777` (ini restored afterwards) logged `Connected!` and the relay logged `joined room 'PUBLIC'`. Script: `harness/lan_dryrun.py`. No firewall prompt appeared on this PC. Not a two-PC or internet test: traffic never left the machine.

### Test run 18 (2026-10-22): late-join snapshots of the standalone relay
New game-free tests `relay_state_test.py` S9 to S13 (pickup state and Kismet path, matinee and hero state, a player who left, blind-mirror twin). On both the upstream relay and the one built with patches 0001/0002 a late joiner received nothing but the online count. A debug build (a `fprintf` in `send_snapshots_to_newcomer` and in `server_drain_snaps`) showed the snapshot queued and the drain never running: the CLI loop in `main.cpp` does not call `server_drain_snaps`. Patch 0003 adds the call; the tests then pass (15 of 15) and fail on the upstream relay (3 expected failures, XFAIL). Unverified in game: effect on collected pickups. The relay inside the shipped exe sends the queue (suite E6: a late joiner gets the pickup state unasked), so the bug is only in the standalone CLI relay.

### Observation (2026-10-22): the F6 overlay cannot spawn items
The Spawns tab lists one category, Enemies. There is no way to create a pickup from the overlay, so in-game pickup sync stays covered by the protocol tests only (`relay_state_test.py` S9 to S13, E6).
The Actions tab only has a DummyHead tuning form; the Scene tab is a UObject browser (88144 objects, search box), with no item-grant control seen.

### Play kit (2026-10-22)
`play/host.ps1` and `play/join.ps1` (see `play/README.md`) automate the host and join steps of the guide. Tested on one PC against a throwaway folder: ini rewrite with backup (existing and missing ini), the `steamapps` guard, and `host.ps1` starting the patched relay (UDP port bound, address list printed). Two PCs not tested.

### Test run 19 (2026-10-22): the real player path, no overlay and no map URL
Setup: relay (patch 0001 to 0003, `--trigger-radius 3000`) and the logging proxy; the ini written by `play/join.ps1`; two game clients started at the main menu (`harness/ol_menu.py`).
Observations:
1. `join.ps1` found `OLGame.exe` equal to the known OpenOL 2.1.0 hash (no warning). The in-game Multiplayer dialog showed exactly the values it had written (name, IP, port, room, the four Sync flags).
2. Menu > Multiplayer > Connect connected both clients (`MpConn: Connected! ... room='PUBLIC'`; the HUD shows `CoopTest: 2 players` and the ping). The menu list takes keyboard input for the main menu and dialogs, but the Multiplayer dialog buttons need the (virtual) mouse cursor: relative mouse moves only.
3. New Game > Main > Normal on both clients: both started at checkpoint `StartGame` in the car. The car-ride matinee (`SeqAct_Interp_3`) crossed the relay in both directions (proxy), the log shows `KISMET: Remote event Car_Ride activated for 2 recipients`, then `CarRideDone` and the checkpoint `Admin_Gates` saved on its own, and both heroes got control at the same place. An email note is shown first; Enter closes it.
4. The partner is visible in the world (the other client's Miles model, seen on screen from the first client).
5. Moving with real input works (about 130 units per second walking; the yaw decoded by the proxy lets a script turn to an absolute heading).
Not reached: any map enemy. The way from the gate into the building needs several steps found by looking at the screen; a blind script did not find them in about twenty tries, so the natural-progress check for the duplicated enemies (open hypothesis above) is still open.

### Test run 20 (2026-10-22): F6 overlay as a test aid, and the VPN-adapter dry run
Overlay tabs seen on screen (window-only pictures): Player has toggles Fully Ghost (no collision, the hero flies along the view direction), Bunny Hop, Unlimited Batteries, Free Camera, God and Max Health;
Visuals has show flags and FreeCam Inspector; Checkpoints lists the story groups (StartGame, Admin, Prison, Sewer, Male, Courtyard, Female, Revisit, Lab) and the DLC; the Console strip at the bottom of the Player/Visuals tab is only
visible when the window is larger than 800x600 (the window can be resized with the mouse or by a script and the viewport follows): `ToggleUnlit` typed there switched the view to unlit.
Fully Ghost took a client through the gate in a few seconds; flying blind inside the building shell gave black frames (back faces), so no natural route to a map enemy was recorded from it. Not a result about the story.
Dry run on the VPN adapter: `lan_dryrun.py` with the address of a VPN adapter (Radmin VPN, network category Public in Windows) connected (`Connected!`, the relay saw the client from that address). The relay executables that had been started before already had inbound allow rules for Public networks (read-only look at the firewall rules), so a friend who reaches the host through that VPN would not meet a new prompt for those executables; another copy of the relay at another path will ask again.

### Test run 21 (2026-10-22): native patch for the duplicated map enemies
Decision: patch the exe directly (Ghidra 12.1.4 from the official NSA GitHub release, SHA-256 matched the release notes; JDK 21 was already installed; everything in the workspace).
Observations (Ghidra on the OpenOL 2.1.0 exe, anchored on strings of OpenOL's public `EnemyChannel.cpp`):
- `ApplySpawn` = `FUN_14171eb00` (only direct caller at `0x14172b0b4` in `OnBinaryPacket`); its body matches the source line by line: `RemoteEnemies` at controller `+0xeb8` (count `+0xec0`, element `0x44`: name, `DummyEnemy +0x10`, `OwnerID +0x18`), `SpawnActor`, `Tag = MultiplayerDummyEnemy` at `+0x130`, `DestroyActor(controller)` = `FUN_1408d7550`.
- `TickSend` = `FUN_141736ae0`: `IndexEnemies` through the controller vtable `+0xbb0`, `CachedEnemies` at `+0xe78`, health `+0x37c`, `SendSpawn` through the channel vtable `+0x268` at `0x141736cab`.
- `GMpConn.LocalPlayerID` at `0x1436b9a70` (a function compares a player id with it to return the local hero), `SyncEnemies` at `0x1436b9aec`, `GWorld` at `0x1430d1e58`; `UObject::Index +0x38`, `Name +0x48` (from `GetName()`), so `Class +0x50`.
Patch: `patches/native-one-owner` (C source compiled with MinGW into 1104 bytes of position independent code, appended as section `.olfix`, two call sites redirected). Rule: the lower player id keeps the real enemy.
Results on the test copy, suite group A:
- patched exe: 10 of 10 PASS, including A4 (one owner), new A4b (the higher id loads first) and A4c (both processes alive afterwards). No crash.
- twin, the original exe in the same run setup: A4 and A4b XFAIL (one spot with patients sent by both clients), as in every earlier run.
Not checked yet: a story script that waits for the destroyed copy, two enemies of one class within 150 units, three players, two PCs.

### Test run 22 (2026-10-22): full suite on the patched exe
One command, groups P, A, B, C, D, E, F, N, H, 1822 s: `{'PASS': 67, 'XFAIL': 4}`. The four XFAIL are the known relay bugs (S9 to S11 on the standalone relay, E5 on the relay inside the shipped exe). A4 and A4b pass; new group N (a real map enemy stays playable, the story advances to the next checkpoint on both clients, both processes stay alive) 4 of 4.
An earlier full run of the same day had one FAIL, H3 (a scripted door burst under the bad-wifi profile, echoes [0,0] of 10). It looked like a regression of the patch: the same group passed on the original exe ([9,9]). The proxy log showed the real cause: `fake_peer.py` counted received packets instead of time in its handshake retry, so an ONLINE_COUNT packet reordered before READY made it give up within a second and send no doors. After the fix H3 passes on the patched exe too ([10,10] and [9,9]). Not a patch bug; the twin run would have been misleading without reading the packets.
Still unverified: a scripted story enemy that the story waits for (the destroyed copy on the higher-id client), two enemies of one class closer than 150 units, play through the real menu path with the patch, two PCs.

### Test run 23 (2026-10-22): the real player path with the native patch (group G)
`suite.py G`, 312 s, 6 of 6 PASS on the patched exe: both clients started at the main menu, connected through Multiplayer > Connect (values from the ini), New Game > Main > Normal; both logged `StartGame`, then the story saved `Admin_Gates` on both; the car-ride matinee crossed the relay in both directions for both ports; both heroes streamed at one place; one client was killed, started again at the menu and rejoined, both streaming. This group uses real mouse and keyboard input (`harness/ol_menu.py`) and is run on its own, not in the default run.

### Test run 24 (2026-10-22): near enemies and story scenes with the native patch, read from the game's memory
New read-only tool `harness/census.py`: reads from a running client's memory its own enemies (`CachedEnemies`) and the puppets of other clients' enemies (`RemoteEnemies`) using the offsets found for the patch. Checked first on one patient: original exe, each client has 1 own + 1 puppet in the same spot (2 enemies); patched exe, client 0 has 1 own, client 1 has 1 puppet (1 enemy), as designed.
1. Two enemies of one class closer than 150 units (`fake_peer.py enemy-near`, a scripted peer announces a second patient next to a real one): at 86 units from the real one, the lower-id client destroyed the peer's puppet (a false merge: client 0 saw 1 enemy, client 1 saw 2); at 151 units both clients agreed (client 0: 1 own + 1 puppet, client 1: 2 puppets). So inside the radius the rule cannot tell a duplicate from a different enemy. A smaller radius would lose merges of enemies that moved between their spawn announcement and the other client's load (one replayed spawn position was 172 units from the enemy's position after the checkpoint applied); not changed.
2. Story scenes with an enemy, original vs patched exe, 25 s after the load and a step: Prison_OldCells_PreStruggle2 (one patient): original 1 own + 1 puppet on each client, patched 1 own on client 0 and 1 puppet on client 1, warnings unchanged. Admin_MainHall: no enemies in either variant. **Prison_PrisonFloor02_SecurityRoom2 (a scripted soldier scene): original 2 own + 2 puppets on each client (two distinct enemies and their copies); patched only 1 enemy in total per client (client 0 1 own, client 1 1 puppet), and one Kismet remote event fewer on client 0 (4 against 5). One of the two enemies is missing on both clients on the patched exe.** Not explained yet: a false merge of two close same-class enemies, an enemy killed in the scene, or the destroyed copy stopping a script. Next step: print class, position and health per enemy for this scene on both exes.
The `Accessed None` script warnings exist on the original exe too (baseline noise), and the patched runs show no more of them.
Follow-up to run 24, from the saved client logs only (no new run): the missing enemy in `Prison_PrisonFloor02_SecurityRoom2` was a timing artifact of the test. The step that wakes enemies started the scripted fight: in both variants the soldier grabbed and killed the hero and the level reloaded, so the census landed on different phases. The logs show the same Kismet events in the same order on both exes (`SoldierSAS`, `SoldierEnteredCloseSasDoors`, `AirlockParticlesFire`, then a second `SoldierSAS` after the respawn; the original log is just longer). Enemy names seen on client 0: original `Patient_0`, `Patient_2`, `Soldier_0`, `Soldier_1`, `Soldier_2` (real ones and copies), patched only `Patient_0` and `Soldier_0`. The original also logs `Accessed None 'Bot'` from a puppet soldier damaging the local hero; the patched run has none, the real soldier kills the hero with its own bot (`SMT_KillHero`, `SMT_HeroDecapitate`). So there is no evidence that the patch breaks this scene. Still to do for a clean confirmation: a census at a fixed time after the load without waking the enemies.

### Research without the game (2026-10-27/28): why 4 checkpoints do not load, and where scripted enemy animations come from
Sources: OpenOL's public `OLImGui_Checkpoints.cpp`, `OLBot.uc`, `OLEnemyPawn.uc`, `OLBehaviorTree.cpp`, `OLActorFactory.cpp`, `OLUtilities.cpp`, and Ghidra on the 2.1.0 exe.
- **Checkpoint loader.** The overlay's loader looks for the checkpoint actor in the current world; if it is not there it sets `AOLGame::PendingCheckpointName` and changes the map to the target's persistent level, **also when that is the map already loaded**.
  The first controller tick afterwards finds the actor with `Utils::GetCheckpointFromName` (which falls back to every loaded `AOLCheckpoint` object, so it always succeeds), runs the script event, clears the name, and the native `StartNewGameAtCheckpoint` walks only the **world's** actors and returns silently when the real actor has not streamed in yet.
  So the load only takes effect when the target's sublevel is already in the world; otherwise the hero starts at the game's own last saved checkpoint. Calling `UOLEngine::SaveCheckpoint(target)` first would be wrong: it stores the current world state under that name.
  Checkpoints saved by walking through the story (`SaveCheckpoint`) do not pass `StartNewGameAtCheckpoint`; only the menu, the overlay, the pending-name tick and the Insane death restart do.
- **Enemy animations.** Idle poses of AI enemies come from waypoint data (`OLWaypoint.AnimToPlay`, `bLoopAnimation`) and behaviour-tree play-animation tasks: `OLBot.StartAnimating` puts the bot in state `Animating`, which sets `bAnimating`, turns and aligns, then calls the **script** function `OLEnemyPawn.PlayFullBodyAnim`.
  The wire carries only the flag (bit 4 of the `LOC` flags). `AOLBot::CurrentAnimation` (`FAnimationData`) is at `Bot+0x618` (confirmed at two fields of the same struct, `ActiveDoor` +0x560 and `PatrolToPlayerDistance` +0x660, and at runtime). The pawn's `FullBodyAnimSlot` is at +0x794, `bIsAnimatingFullBody` is bit 0x10 of the dword at +0x2bf8.
  A puppet lacks the anim sets that the level's actor factory gives the real enemy (`AdditionalAnimSets`, appended to `SpawnerAnimSets` at pawn +0x2d7c), which is where the `SE_AI_*` animations live.

### Test run 25 (2026-10-28): enemy animations on a puppet (patch v7)
New world packet 0x12 carries the animation name; the puppet runs the script function `OLEnemyPawn.PlayFullBodyAnim` through `ProcessEvent` from the per-frame hook. Read-only probe `probe_anim.py` on two real clients, prison checkpoints with waypoint patients:
1. First run: owner counters `cnt_anim_sent 14`, puppet client `cnt_anim_recv 14`, `cnt_anim_played 14`, `cnt_anim_dropped 0`, `Bot+0x618` filled during the animation (name index 53969, 0 outside). But the puppet's `bIsAnimatingFullBody` stayed 0 and its log said
   `PlayCustomAnim SE_AI_SectionA_CageShaker, CustomChildIndex: 1, Animation Not Found!!` after every `### PlayFullBodyAnim AnimName=SE_AI_SectionA_CageShaker` line: the script ran with the right name but the puppet lacks the anim set.
2. Fix: when the merge drops our own copy of an enemy in favour of the puppet, the copy hands its `SpawnerAnimSets` to the puppet (`AddAnimSets`, pawn vtable +0x738). Second run: owner and puppet animating together 12 of 12 seconds (`Prison_OldCells_PreStruggle2`), 8 of 12 and 12 of 12 (`Prison_Showers_2ndFloor`, two runs), 3 (`Prison_ToPrisonFloor`, a short animation);
   `Animation Not Found` still 3 times for two enemies in the first run with the fix (not explained), 0 in the next run.
3. Twin: the same checkpoints with kill-switch bit 3 set on both clients: 0 seconds with an animating puppet, every `cnt_anim_*` 0.
Not covered: an enemy that exists on one client only (nothing to take the anim sets from), root motion of animations that move the enemy, three players, two PCs.

### Test run 26 (2026-10-28): sweep of 95 checkpoints and the hands-free tests on patch v7
Two real clients, checkpoints loaded through the mailbox one after the other on both clients, no mouse and no keyboard. Groups Admin, Revisit, Prison, Male, Female, Courtyard, Lab; the sewer group with the clients started on `sewer_persistent`.
Result: 95 checkpoints, no hang (hero stream silent for 45 s), no crash box, no enemy simulated twice, no difference in the enemy count between the clients, 21 checkpoints with enemies (one owner, the other client has puppets); **4 not applied**:
`Courtyard_Soldier2`, `Female_2ndFloorChute`, `Female_3rdFloorPostHole`, `Lab_PremierAirLock` (the loader limit above). The earlier sweep on patch v3 had 7 not applied of 90, one enemy simulated twice and one hang in the sewer.
`probe_positions.py Revisit_To3rdfloor Revisit_Soldier3 6` (one enemy walks away between the two loads): 2 enemies, one owner, none doubled. `mailbox_live.py` 3 of 3, `shared_cp_live.py` SC1 to SC5 PASS. The desktop suite (`suite.py`, mouse and keyboard) was run on v7 later, see test run 30.

### Test run 27 (2026-10-28): animation sets by path, more checkpoints (patch v7)
Probe `probe_anim.py` on six more checkpoints with enemies (a soldier, the security rooms, Revisit): owner and puppet animate together 12 of 12 seconds on `Prison_IsolationCells02_Soldier`, `Prison_PrisonFloor02_SecurityRoom2` and `Revisit_RH`, 4 on `Prison_PrisonFloor_3rdFloor` (a short scene);
`Admin_SecurityRoom` and `Revisit_PriestDead` have no enemy in a pose, so 0 is right there. Revisit uses other animations (`SE_DupontAndDupont_Waiting_Dupont1`, `SE_Patient_StandsAtRailing`); they play too.
One puppet in `SecurityRoom2` stood without a pose: its enemy had no twin on the other client, so there was nothing to hand the animation sets over from (`SpawnerAnimSets` count 0 on the puppet, 1 on the owner).
Change: the owner's packet also carries the full paths of its animation sets (at most four, `GetFullName` without the class word); a puppet without sets finds them with `StaticFindObject` (class `Engine.AnimSet`) and installs them like the handover does.
With the handover switched off (`--disable 16`) on `Prison_OldCells_PreStruggle2`, `Prison_Showers_2ndFloor` and `Revisit_RH`: 12 of 12 seconds each, `cnt_anim_resolved 4`, no `Animation Not Found` in the log. With both paths on, the last run had no `Animation Not Found` and 1 of 43 owner-animating puppet-seconds without the pose (the first second after a load).
`hosttest.py` now has 35 checks (packet with and without sets, the refresh without sets, the lookup, no lookup when the puppet already has sets).

### Test run 28 (2026-10-28): story triggers from a far-away player, dropped by the exe patch (patch v7)
Research: `WORLD_TRIGGER_ACT` (world type 1) is forced on every client by OpenOL. The relay patch 0001 filters it at the relay; the exe patch now does it on the receiving client, in the world-packet hook, so it also works with the relay built into the game and with an unpatched relay. The sender's position is `URemotePlayer.LastReceivedLoc` (+0x94, `PlayerID` +0x68, `bHasReceivedData` bit 0 of +0xb8) in `GMultiplayerController.RemotePlayers`; the local hero is `HeroPawn` of the controller. Rule: the sender's position farther than 3000 units (squared, float bits `0x4b095440`, override `trigger_r2` in the data area) drops the packet; unknown position, unknown sender and a dead hero let it through. Kill switch bit 5; counters `cnt_trig_seen`, `cnt_trig_dropped`. A dropped trigger leaves `TriggerCount` alone, so the local player can still fire a one-shot trigger by touching it.
The code outgrew 8 KB, the data area moved from 0x2000 to 0x3000 (patched exe 1392b18c...).
`harness/trigger_live.py` (two real clients, scripted peer, the **upstream** relay, no mouse and no keyboard, checkpoint `Admin_Explosion` loaded through the mailbox): T1 a trigger from a peer with no known position fires on both clients; T2 from a peer 50000 units away fires on none (both count the drop: dropped [7, 1], seen [8, 8] in the run); T3 from the players' position fires on both; T4 gate off (bit 5) the far trigger fires on both again; T5 both alive. 5 of 5.
`hosttest.py` has 43 checks now (far, near, unknown position, unknown sender, dead hero, another packet type, custom radius, kill switch); `selftest.py` 21 PASS. Regression on this build: `mailbox_live.py` 3 of 3, `shared_cp_live.py` SC1 to SC5 PASS, `probe_anim.py Prison_OldCells_PreStruggle2` 10 animating seconds on both sides.
Observed side effect, by design: while the players are apart (for instance a checkpoint load that one client has not applied yet) triggers from the other are dropped. Not tested: a trigger in a real walk-through, two PCs.

### Test run 29 (2026-10-28): ten minute soak with enemies (patch v7)
`harness/soak_enemy.py Prison_OldCells_PreStruggle2 10`: two real clients, the checkpoint loaded through the mailbox, no mouse and no keyboard, upstream relay. 20 samples at 30 s: both processes alive, both hero streams fresh, no `Animation Not Found` or fatal line in either log, 0 anomalies. Memory 1547 and 1545 MB after the load, 1419 and 1413 MB at the end, flat after the first minute. One enemy with one owner (own copy destroyed on one client, puppet destroyed on the other, both counters 1 and constant). The owner sent 75 animation packets (the long idle pose is repeated now and then), the other client received 75 and ran the animation once.
Not covered: heroes that fight or die, a longer run, two PCs.

### Test run 30 (2026-10-28): desktop suite on patch v7 (with the trigger gate), groups A, B, C
Real clients, mouse and keyboard (the F6 overlay), upstream relay except B3 (patched relay), exe sha 1392b18c...: group A 10 of 10 PASS in 239 s (A4, A4b, A4c: one owner, both processes alive; doors A6, A7); group B 5 of 5 PASS in 182 s (B1 to B4, the far trigger is dropped by the patched relay; the exe gate stays out of the way because the peer position is unknown or near); group C 7 PASS and 1 SKIP in 189 s (C0 to C5: spawn, damage, death announced, respawn with health, owner death deletes its enemy; C6 skipped as before, the non-owner did not die). This repeats the v5 results on v7.
Tried and dropped: setting the hero's Health from outside (`pawn+0x37c`): the write succeeds and reads back 100 on the next read, the game keeps its own value elsewhere, so a death test stays on the enemy damage of group C.
Group G (the real player path through the main menu, New Game, car ride, rejoin) on the same build: 6 of 6 PASS in 286 s (G1 to G5 and the scorer check). With A, B and C that is 28 PASS and 1 SKIP on patch v7.

### Test run 31 (2026-10-28): the remaining suite groups on patch v7
Same build and setup as run 30. E (relay inside the exe) 6 PASS, 1 XFAIL (E5, the known ENEMY_LOC id of the shipped relay); H (bad network, 90 s of `bad_wifi`, 10 s and 40 s blackouts) 6 of 6; N (real map enemy, story advances after the dedupe) 5 of 5; P (no game) 26 PASS, 3 XFAIL (S9 to S11 on the upstream relay).
Two failures on the first pass, both caused by the shared checkpoint load and fixed in the tests, not in the patch:
- F (late join) failed F1 (no SPAWN of the enemy reached the late joiner). The tap log shows why: the late joiner loads the checkpoint, announces it (world 0x11), client A follows it and its soldier, spawned from the overlay, is deleted (`ENEMY 0x02 DEL`). That is the feature working: a checkpoint load on one client reloads it on the other and drops enemies spawned by hand. The test now switches the following off on A (`disable 4`); with the relay patch 0003 (late-join replay) F is 5 of 5 (F1 needs the replay, the upstream relay does not send the SPAWN to a late joiner unasked).
- D (`SyncEnemies=False`) failed the precondition D5 (the enemy never attacked within 90 s): the clients reloaded each other between the steps. With the following switched off on both D is 7 of 7.
Practical note for players: a checkpoint load by one player is followed by the other; a soldier spawned from the F6 overlay does not survive it.

### Test run 32 (2026-10-28): a crash found by a soak, an independent code review (patch v7)
A soak on `Prison_IsolationCells02_Soldier` ended with client 1 dead 1.2 s after it had connected: `Assertion failed: Sequence != RootSequence [UnWorld.cpp 1693]`, right after `ExecCP: cross-map travel`. The test had loaded the checkpoint on client 0 while client 1 was still starting; the 0x11 packet reached client 1, its follow ran at once and took the loader's cross-map path while the levels were streaming in. In real play that is one player loading a checkpoint while the other is still starting the game.
Fix (exe sha 17cda407...): the follow waits until the controller the enemy hooks know has ticked 600 frames and the hero exists (`cnt_follow_deferred`, override `follow_min_ticks`). `harness/follow_race.py` (mailbox only): old build R1 and R2 FAIL (client 1 crashed), new build 3 of 3 with 755 deferred frames and one executed follow. The soak rerun on the new build: 5 minutes, 0 anomalies, both clients alive, 38 of 38 animation packets received.
Review: an independent read of `olfix.c` (no game) found no access past a packet or buffer; four input problems fixed (a NaN place was "the same place" in the merge, a puppet marked for deletion could be merged with, an empty checkpoint name in a 0x11 packet was run, NaN or infinite animation floats reached the script call). `hosttest.py` 54 checks, five of them new and failing on the old code. Regression on the new build: `shared_cp_live.py` SC1 to SC5, `mailbox_live.py` 3 of 3, `trigger_live.py` T1 to T5, `probe_anim.py` 25 animating seconds, selftest 21.
Left as they are (reviewed, low risk): a stale actor pointer in the first-seen table, a full table of 64 entries, the animation name added to the global name table by a peer that owns a puppet.
The Windows session was locked during this run (lock screen in the foreground), so only the hands-free tests ran; the desktop suite had passed earlier the same day on the previous build and was not repeated on this one.
Sweep of all 95 checkpoints on the new build (17cda407..., the follow wait and the input checks), hands-free: 95 of 95 ok (no hang, no crash box, no enemy simulated twice, equal enemy counts on both clients); 4 not applied, the same four as before (`Courtyard_Soldier2`, `Female_2ndFloorChute`, `Female_3rdFloorPostHole`, `Lab_PremierAirLock`). The wait before a partner's load did not change what the checkpoints load: for example `Prison_ToPrisonFloor` 3 enemies on one owner and 3 puppets on the other.

### Test run 33 (2026-11-04): OpenOL 2.2.0 on one PC, two clients
Release 2.2.0 (`OpenOL.zip`, SHA-256 `307b3b3834f6efbba940440133d1cf19ed68ff2c32e585a6ce95cc407a5c8da9`, `OLGame.exe` SHA-256 `5d3fb292baeec5cb03afb4765d361a24f2e07ea6ed6f353d71792cfd6cb4f7d1`) unpacked over a copy of the test game folder; standalone relay built from the commit the release pins (`OpenOL-Relay` `69998e7`), loopback, `-nosteam`. The traffic is encrypted, so the checks read the game logs and the process state (scripts in the fork's `harness/`: `v22.py`, `v22_soak.py`, `v22_scenarios.py`, `v22_churn.py`, `v22_cp.py`, `v22_sweep.py`).
- Handshake and session: both clients connect, one AI owner (`Enemy AI owner`), both `In world`, the HUD lists the players and the ping.
- Ten minutes with two clients: no error line, memory flat (1503 and 1525 MB after the load, 1376 and 1421 MB at the end).
- Owner leaves (process killed): the other client becomes the owner and stays alive; the returning player rejoins as a new id and is not the owner; the other client keeps its ownership; after a relay restart both clients reconnect. 5 of 5.
- A player leaves and joins again 5 times while the other stays: 0 failures, the staying client 1506 to 1481 MB.
- Checkpoint loads through the overlay (the key is `~`, F6 opens the engine console): at 17 checkpoints with enemies (Prison 11, `Admin_SecurityRoom`, `Lab_BigTower`, `Lab_BigTowerStairs`, `Lab_SwarmIntro2`, `Male_Chase`, `Male_GetTheKey2`) the load on one client was followed by the other (`[MP] Travel: cp=... epoch=N`, then `Applying checkpoint`); exactly one client logged `Enemy factory ...: spawn` and the other `not the owner`; `Dummy ... plays` on the other client; no `Enemy anim not found`.
- Design read from the source of release 2.2.0: every way an enemy enters the world goes through one factory gate that only the AI owner passes, so a second copy is not created, scripted enemies included; limited-use triggers, CSAs and pickups are decided by the server.
Not tested: two PCs, Steam P2P (all runs used the standalone relay), three players, the Revisit, Female, Courtyard, Sewer groups, triggers, doors, pickups and pushables in real play, the scene where the enemy breaks a door.
