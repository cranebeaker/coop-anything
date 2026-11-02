# Outlast (2013)

| | |
|---|---|
| Engine | Unreal Engine 3 (`CookedPCConsole`) |
| Versions tested | Steam build 576074, `Binaries\Win32\OLGame.exe` SHA-256 `fc06a9d562ea41757fa7c95abae67c89fb4f56a9c970122fdef2f15758f2bb0d`, Whistleblower DLC installed, Windows 11 |
| Projects | [OpenOL fork with our patches, play kit and tests](https://github.com/G3RKA/OpenOL) (own development, `tools/outlast-coop/`), [OpenOL](https://github.com/ShyKiss/OpenOL) with [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) (directory: alpha), [Outlast-Together](https://github.com/LucianLandIsBack/Outlast-Together) (abandoned) |
| QA level | automated (67 PASS, 4 XFAIL known bugs in the last full run; see [QA](#qa)) |
| Last updated | 2026-10-22 |

## Where things stand

OpenOL 2.1.0 has a working multiplayer beta if you host with the standalone OpenOL-Relay 2.1.0 and use the room
`PUBLIC`. Two clients on one PC stayed connected for runs of 5 to 13 minutes, saw each other, shared doors, and shared
player-spawned enemies with damage, death and respawn. Story sync is crude: a trigger touched by one player fires for
everyone wherever they are, checkpoint loads are not synced, and map enemies are duplicated (each client runs its own
copy and both attack the same player). The embedded relay *source copy* in the OpenOL repo uses the old packet format (the relay inside the release exe works). Nothing was
tested between two PCs.

## Existing work

| Project | What it is | Last activity | Notes |
|---|---|---|---|
| [OpenOL](https://github.com/ShyKiss/OpenOL) | Modified UE3 client (rebuilt `OLGame.exe` and packages) dropped over a licensed Steam install, multiplayer beta since 2.0.1, release 2.1.0 on 2026-08-29 | 2026-08 | Source is public, no license stated (GitHub API `license: null`). Needs the Whistleblower DLC (the multiplayer map is `DLC_Intro_Persistent`). Open issues #6 (missing map), #7 (connect does nothing), #8 (Start Server and Invite do nothing). |
| [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) | Standalone UDP relay (CLI and GUI) | 2026-08 | Creates one room, `PUBLIC`, when it has none in its database. Used for all tests here at commit `b9def512` ("Update to 2.1.0"). |
| [Outlast-Together](https://github.com/LucianLandIsBack/Outlast-Together) | MIT-licensed school project with a networking DLL and a client folder | 2025-02 | No releases. README says no updates after the school release. What it synced was not checked. |

Also looked at: the Steam Workshop item "Outlast multiplayer" (a Garry's Mod map, removed), ModDB and Nexus Outlast mods
(single-player only in the results). Not searched: Reddit, Discord, archive.org, Steam guides.

## How the game works (for co-op)

- UE3 with a native `OLGame` module. OpenOL ships a rebuilt `OLGame.exe` plus `.upk` files; its source tree has the
  game code (`Development/Src/OLGame`), the multiplayer layer (`Development/Src/Multiplayer`) and the relay
  (`Development/External/openol_relay`).
- Wire format (OpenOL 2.1.0, `FMpConnection::SendBinary`): `[channel][type][player_id LE4][payload]`. Channels: SRV 0, HERO 1,
  DOOR 2, ENEMY 3, PUSH 4, WORLD 5; relay-to-client notices start at 0xE0 (`READY`, `ONLINE_COUNT`, `HELLO_FAIL`).
  A client connects with the text line `HELLO,<room>,<password>`, gets `READY` and a player id, then sends a binary
  `SRV_HELLO` with its nick and a `SRV_PING` about every 5 s. Silence for 30 s drops the connection on both sides.
- The relay forwards packets inside a room and keeps snapshots for late joiners (doors, enemies, pushables, last
  hero state, matinee state).
- Hero state (`HERO_STATE`) carries position, rotation, health and the special-move id; enemies are sent as `SPAWN`,
  `LOC` (with mode and head-look target), `SMT` (special moves) and `DEL`; world events are `TRIGGER_ACT` (Kismet touch
  events by path name), `MATINEE_STATE`, pickups and `RemoteEvent`s.
- Each enemy a client spawns or owns is simulated locally and mirrored on the other clients as a dummy keyed by
  (name, owner id).
- With Steam on, the modded build reads the player's Steam Cloud profile; with `-nosteam` it uses
  `OLGame\SaveData\*.olprofile` and `OLGame\SaveData\OL_<date>.sav` inside the game folder. UE3 config for the modded
  build goes to `<game dir>\OLGame\Config\` (including `OLMultiplayer.ini`).
- A client can start straight on a map from the command line (not every map works: `adminblock_persistent` as the start map
  crashes with `Assertion failed: Sequence != RootSequence [UnWorld.cpp:1693]` and a Windows error box; start on `prison_persistent` and load
  a checkpoint instead)
  (`OLGame.exe prison_persistent?game=Multiplayer.MultiplayerGame -nosteam`). Starting on a chosen checkpoint is only
  possible through the F6 overlay or the console; the `#name` URL form loads the map at its default spawn.
- OpenOL's F6 overlay (Dear ImGui) has Checkpoints (with a search box), Spawns, Player and Scene tabs.

## What we changed

**Relay patches (OpenOL-Relay, optional, no game source needed).** `0001` [relay-patches/0001-openol-relay-trigger-radius.patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/relay-patches/0001-openol-relay-trigger-radius.patch) adds `--trigger-radius <units>`.
With it a trigger is forwarded only to players whose last known hero position is within that distance of the sender's; unknown positions fail open; 0 (default)
keeps upstream behaviour. `0002` [relay-patches/0002-openol-relay-enemy-packet-ids.patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/relay-patches/0002-openol-relay-enemy-packet-ids.patch) makes the relay use the `ENEMY_*` ids the 2.1.0 client sends (`LOC` 0x04, door ids 0x05 to 0x08), so a player who joins while an enemy is alive gets its position in the snapshot (the owner streams positions anyway, so in game the effect is small). `0003` [relay-patches/0003-openol-relay-drain-snapshots.patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/relay-patches/0003-openol-relay-drain-snapshots.patch) is a one-line fix: the standalone (CLI) relay fills the late-join snapshot queue but its main loop never calls `server_drain_snaps`, so a player who joins late never receives picked-up items, the other player's last matinee and hero state, or the automatic door, enemy and pushable replay (the game requests doors, enemies and pushables itself, so those still worked). All three apply to `OpenOL-Relay` at `b9def512` in this order (0003 also applies on its own). Trade-off: a player who is far away misses the trigger and the story
state of the two players can differ until they meet; pick a radius that covers the area you play in together (the suite uses 3000). Nothing in OpenOL itself was changed.

**Native patch for duplicated map enemies (OpenOL exe, optional):** [exe-patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/exe-patch/README.md) adds about 1 KB of code to `OLGame.exe` of OpenOL 2.1.0 and redirects two calls (`ApplySpawn`, `SendSpawn` in `TickSend`): when two copies of one map enemy meet, the copy of the lower player id stays real and the other client destroys its own copy, keeping only the puppet. Found with Ghidra from OpenOL's public multiplayer source, no Outlast source. Tests A4, A4b pass with it and fail on the original exe (twin).

**No-source workaround for duplicated map enemies (without the native patch):** set `SyncEnemies=False` in `OLMultiplayer.ini` on every client. Then no enemy packets are sent or applied: each player
has only their own, independent copy of every enemy (no ghost copy, no double damage, but the partner does not see your enemies and you do not see theirs). A relay-side
enemy filter was considered and rejected (see NOTES.md, test run 12).
Problems found, with the evidence kept in [NOTES.md](NOTES.md) (test runs 1 to 11):

| # | Problem | Root cause (source reading) | Test that shows it |
|---|---|---|---|
| 1 | Client reconnects every ~30 s with a new player id when the relay is built from the embedded relay *source copy* in the OpenOL repo (the relay inside the shipped `OLGame.exe` does not do this: verified with two clients, test run 16) | Embedded `server.c` parses `[type][player_id]`; 2.1.0 clients send `[channel][type][player_id]`, so every binary packet is dropped and the player times out | Test run 2; same clients stable with OpenOL-Relay `b9def512` (runs 3 to 8) |
| 2 | Direct connect to a "Start Server" host shows nothing | Default room `PUBLIC`, embedded relay only has `DEFAULT`; `HELLO_FAIL` is ignored by the client | Protocol probe (`hello_probe.py`): `DEFAULT` gives `READY`, `PUBLIC` gives `HELLO_FAIL`; not run with a real host |
| 3 | Relay and client use different `ENEMY_*` ids (`LOC` 0x08 vs 0x04, door ids shifted by one) | Hand-copied constants | Protocol test T6/T7: the upstream relay's late-join snapshot has `SPAWN` but no `LOC`; relay patch 0002 fixes it |
| 4 | Checkpoint loads are not synced and triggers fire for everyone out of context | Touch events are sent by path and force-activated on every client | Test run 7: scripted peer sent `SeqEvent_Touch_0..30`; 13 fired on both clients, story and a checkpoint save advanced for both. Trigger part mitigated by the relay patch (suite B3/B4, `relay_filter_test.py`); checkpoint loads still not synced |
| 5 | Map enemies are duplicated and both copies attack the same player (reproduced after checkpoint loads through the F6 overlay; **not yet checked in normal story progress**, see the open hypothesis in NOTES.md) | Every client sends every local enemy; no single owner for map enemies | Test run 8: two patients on screen, `HERO 0x07` from both copies, double damage. **Fixed by the native patch** (A4, A4b PASS; NOTES test run 21) |
| 6 | Smaller: startup stall when the window starts minimized, pause stops pings, unknown `OpenOLResources` package, Steam profile warning | see NOTES | Test runs 3 and 7 and source |
| 7 | The standalone relay never sends the late-join snapshot queue (picked-up items, matinee and hero state of the other players are lost for a late joiner) | `server_drain_snaps` is declared and documented as "called each tick" but the CLI loop in `main.cpp` does not call it (the loop in `server_run` does) | Protocol test S9 to S11 (XFAIL on the upstream relay, PASS with patch 0003); found with a debug build that logged the queue |

These problems have not been reported to the OpenOL authors; the fork is developed on its own.

## Licence

Checked on 2026-10-25 (GitHub API): [OpenOL](https://github.com/ShyKiss/OpenOL) and [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) state
no licence (`license: null`, no licence file), so all rights stay with their author by default. OpenOL's README says its files are
"provided only for modding convenience" and that the built `OLGame.exe` replaces the files of your licensed Steam copy; it says nothing
about forks or patches. The author has not been asked for permission. How the work here stays within that:

- No OpenOL or Outlast binary is redistributed: no `OLGame.exe`, no `.upk`, no release archive, no game assets. The exe patch is shipped as
  its own C source, the built code bytes and an applier that the player runs on the OpenOL 2.1.0 `OLGame.exe` they downloaded themselves
  from the OpenOL release. The applier checks the input SHA-256 and the original bytes at each of the 7 patched sites (5 to 8 bytes each)
  and refuses any other file. This is the "ship your work as a patch the player applies to their own copy" route of the hub's rule 7.
- The relay patches are unified diffs against OpenOL-Relay `b9def512` (only the changed lines plus the usual 3 lines of context); the
  player applies them to their own clone and builds the relay.
- [G3RKA/OpenOL](https://github.com/G3RKA/OpenOL) is a GitHub fork, which GitHub's terms of service allow for a public repository; the
  upstream files in it stay under the upstream terms. The fork's own work is in `tools/outlast-coop/` under the MIT licence (that folder
  only). One upstream file is changed: `Development/Src/Multiplayer/Src/EnemyChannel.cpp` (+66 lines), the one-owner rule written as
  source for reference; it is not built or shipped as a binary.
- Anyone who wants to ship a prebuilt patched exe or more of OpenOL's code should ask the OpenOL author first.

## QA

Command (Windows, two game windows open for about 10 minutes; set `OL_GAME_DIR`, `OL_LOG_DIR`, `OL_RELAY_EXE`, see
[harness/README.md](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/harness/README.md)):

```
python -I tools/outlast-coop/harness/suite.py       # in the fork https://github.com/G3RKA/OpenOL        # everything (P, A, B, C, D, E, F; about 25 minutes); or: suite.py P | A | B | C | D | E | F
```

It starts a relay and a logging proxy, launches two real OpenOL clients on a test copy of the game (`-nosteam`), loads
checkpoints, injects packets with a scripted peer, spawns enemies through the mod's F6 overlay, asserts on the decoded shared
state and the client logs, and prints `RESULT: PASS|FAIL|XFAIL|XPASS|SKIP|ERROR <test>` per test and one `SUMMARY` line. Exit code 0 only
without FAIL, ERROR or XPASS. A game crash dialog turns the run into ERROR.

| Test | Asserts | Last result |
|---|---|---|
| P (26 results, no game, `relay_filter_test.py` T1-T7 and `relay_state_test.py` S1-S13) | trigger radius (near, far, unknown position, off, doors unfiltered), late-join enemy snapshot with position, door lock exclusivity and first-write-wins snapshots, pushable ownership, release, `PUSH_INIT`, late-join replay of pickups, matinee and hero state; with twins on the upstream relay (T4, T7) and blind mirrors (S8, S13) | 23 PASS; S9 to S11 on the upstream relay are XFAIL (the missing drain call, patch 0003 fixes it) |
| S0 scorer self-test | the relay-timeout scorer sees good and bad input (it ignores the HELLO probe that `ol.py up` leaves in the relay) | PASS |
| A1 | both clients send fresh hero state | PASS |
| A2 | no reconnect in 45 s (exactly one `Connected!` per client) | PASS |
| A3 | the relay never times out a real client | PASS |
| A6 | a `DOOR_OPEN` for a real door, sent by a scripted peer, is applied by both clients (each echoes it once) | PASS |
| A7 | mutation twin of A6: a position with no door is applied by nobody | PASS |
| A4 | a map enemy is sent by one client only | PASS with the native patch; XFAIL on the original exe (both send their own copy) |
| A4b | the same when the higher player id loads the checkpoint first | PASS with the native patch; XFAIL on the original exe |
| A4c | both game processes alive after the enemy handover | PASS |
| N1 | a real map patient (checkpoint load on both clients) has one sender | PASS with the native patch (skipped on the original exe) |
| N2 | both game processes alive after the handover | PASS |
| N3 | the story advances: a second checkpoint of the group loads on both, both heroes arrive together and stream | PASS |
| N4 | both processes alive after the second load | PASS |
| G1 | both clients connect through the real menu (Multiplayer > Connect, values from the ini) | PASS |
| G2 | New Game on both: `StartGame`, then `Admin_Gates` saved by the story on both | PASS |
| G3 | the car-ride cutscene (matinee) crosses the relay in both directions for both clients | PASS |
| G4 | both heroes stream and share the start point after the intro | PASS |
| G5 | one client is killed and rejoins through the menu, both stream again | PASS (group G needs real mouse and keyboard, 312 s, run on its own) |
| A5 | both players spawn at the same point after a checkpoint load | PASS |
| B1 | an injected `WORLD_TRIGGER_ACT` fires on both clients | PASS |
| B2 | mutation twin of B1: a bogus trigger path fires on neither client | PASS |
| B3 | a trigger sent by a peer standing far away (50000 units) is not applied | PASS with the patched relay; on the upstream relay it fires on both (the same test, run with `OL_PATCHED_RELAY_EXE` pointing nowhere, reports XFAIL) |
| B4 | control: the same trigger sent by a peer next to the players is applied | PASS |
| F1 | a second client that joins (and loads the same checkpoint) while the first has a living, calm enemy gets that enemy relayed | PASS |
| F2 | ... and its position | PASS |
| F3 | the late joiner streams its state | PASS |
| F4 | the first client is not disturbed (no reconnect) | PASS |
| E1 | two clients join the relay built into the shipped `OLGame.exe`, started from the F6 overlay Relay tab (room `DEFAULT`; the suite switches the ini and restores it) | PASS |
| E2 | no reconnect in 65 s (one `Connected!` each) | PASS |
| E3 | both heroes stream state through it | PASS |
| E4 | a late joiner's enemy snapshot has the `SPAWN` | PASS |
| E6 | a late joiner gets the stored pickup state without asking (the relay's snapshot queue is sent; added after the last full run, group E alone: PASS) | PASS |
| E5 | ... and the enemy position (`LOC`) | XFAIL (the shipped relay has the `ENEMY_LOC` 0x08 vs 0x04 mismatch, patch 0002 fixes the source copy but not the exe) |
| D1 | `SyncEnemies=False` in `OLMultiplayer.ini` (both clients): no enemy packet crosses the relay (suite edits and restores the file) | PASS |
| D2 | each client still has its own map enemy (checkpoint with the patient applied) | PASS |
| D3 | an enemy spawned on one client is not mirrored to the other | PASS |
| D5 | precondition of D3/D4: the enemy on A's client really attacked (`SMT_AttackGrab` in its log) | PASS |
| D4 | the other player takes no damage from that enemy | PASS |
| H1 | two clients behind an impaired link (`tools/chaos` profile `bad_wifi`: about 35 ms +/- 25, 2% loss in bursts, reordering) stay connected for 90 s | PASS |
| H2 | both heroes still stream state | PASS |
| H3 | a burst of 10 door open/close packets is applied by both clients (at least 7 of 10 echoes) | PASS (9 of 10) |
| H4 | a 10 s blackout causes no reconnect and the streams resume | PASS |
| H5 | a 40 s blackout is recovered: each client reconnects once and streams again | PASS |
| C0 | control: with no enemy nobody loses health in 8 s | PASS |
| C1 | an enemy spawned by one player is relayed to the other client | PASS |
| C2 | the enemy damages a player | PASS |
| C3 | the death of a player is announced to the other client | PASS |
| C4 | the dead player respawns with health | PASS |
| C5 | the owner's death deletes the owner's enemy | PASS (SKIP if the owner never dies) |
| C6 | a non-owner's death does not delete the owner's enemy | PASS (SKIP if the non-owner never dies) |

Last full run on 2026-10-22, one command (`suite.py`, groups P, A, B, C, D, E, F, N, H) on the game exe with the native patch, 1822 s: `{'PASS': 67, 'XFAIL': 4}`, no FAIL (the self-test S0 counts once). An earlier run of the same day had one FAIL, H3: the scripted test peer gave up its handshake within a second on the impaired link (a harness bug, fixed by waiting for READY by time instead of counting packets; the native patch was not involved: twin runs). The four XFAIL are E5 (position missing in the late-join snapshot of the shipped relay) and S9 to S11 on the upstream relay (late-join snapshot queue never sent; only the standalone relay, the relay inside the shipped exe does send it, test E6). Earlier per-group runs gave the same results; the first runs of each group found the issues listed above.
protocol test without the game, [harness/relay_filter_test.py](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/harness/relay_filter_test.py) (7 of 7, with twins on the upstream relay). The first runs found a scorer bug (counting the HELLO
probe's timeout), a wrong start map (`adminblock_persistent` crashes the engine, see "How the game works"), a spawn aim that put the enemy out of
its attack range, and an idle enemy that only attacks a player that moves (about 100 to 190 units); each is handled in the suite.

Limits, plainly: the suite covers connection stability, spawn, doors and triggers (injected), map-enemy ownership, and player-spawned
enemies with damage, death and respawn. Not covered in game: pushables (applying a remote push could not be observed), cutscenes, pickups, story progress, other enemy types, two PCs. The
bots use desktop input and the F6 overlay (to load checkpoints, spawn enemies and set health), not an in-game bot, which the repo prefers.
Mutation twins exist for the door and trigger tests (A7, B2) and the control C0; there is no fix yet to switch off for the others. The one XFAIL (A4)
encodes today's bug; when a fix lands it reports XPASS and the expectation must be flipped. B3 only passes with the patched relay (see What we changed).

## How to play

Unverified over the internet. The setup that worked on one PC:

1. Copy your Outlast install (needs the Whistleblower DLC), install the OpenOL 2.1.0 release over the copy, keep the original untouched.
2. Host: run a relay with UDP 7777 reachable. Either the release `OpenOL_Relay` (room `PUBLIC`), the relay inside the game (F6 overlay, Relay tab, Start Relay; room `DEFAULT`), or one you build:
   ```
   git clone https://github.com/ShyKiss/OpenOL-Relay && cd OpenOL-Relay && git checkout b9def512
   patch -p1 -d src < <OpenOL fork>/tools/outlast-coop/relay-patches/0001-openol-relay-trigger-radius.patch    # optional
   patch -p1 -d src < <OpenOL fork>/tools/outlast-coop/relay-patches/0002-openol-relay-enemy-packet-ids.patch   # optional
   patch -p1 -d src < <OpenOL fork>/tools/outlast-coop/relay-patches/0003-openol-relay-drain-snapshots.patch     # optional, recommended
   cd src && gcc -O2 -c server.c cli.c db.c && g++ -O2 main.cpp server.o cli.o db.o -o OpenOL_Relay.exe -lws2_32   # MinGW
   OpenOL_Relay.exe --port 7777 --trigger-radius 3000
   ```
   Helper: `play\host.ps1 -Relay <OpenOL_Relay.exe>` starts it and prints this PC's addresses ([play/README.md](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/play/README.md)).
3. Everyone: in `OLGame\Config\OLMultiplayer.ini` set `IP`, `UdpPort` and `RoomCode` (`PUBLIC`, or `DEFAULT` for the relay inside the game), then use the in-game Multiplayer menu (Connect), or start the game
   with the multiplayer map on the command line. `play\join.ps1 -GameDir <copy> -HostIp <host>` writes the ini for you (with a backup). Add `-nosteam` to keep the Steam Cloud profile out of it.
4. Players should load the same checkpoint themselves (there is no checkpoint sync) and stay together (triggers fire for everyone unless the patched relay filters them).
5. Against the two copies of an enemy: apply the native patch to `OLGame.exe` of every player ([exe-patch](https://github.com/G3RKA/OpenOL/tree/main/tools/outlast-coop/exe-patch/README.md)), or set `SyncEnemies=False` on every client (each player then has their own, unshared enemies).

## Known issues

What a player will notice: a connection that silently does nothing when the room code is wrong; story events firing
while a player is elsewhere; a second copy of the same map enemy near the first, so enemies hit harder; checkpoint
loads that do not carry the partner along; a game error box (engine assertion in `UnWorld.cpp`) when a checkpoint in
another level group is loaded from the F6 overlay, seen with a single client too (no partner needed).
With "Start Server" (the relay inside the shipped exe) a late joiner gets no position of an enemy that is already on the
map (test E5); the standalone relay does not have that problem.

## Next steps

- OpenOL cannot be rebuilt without the full, non-public Outlast source tree (see NOTES.md, build check), so further native fixes
  go into the exe patch, the way the one-owner rule did. Still open there: trigger context (fire a trigger only for players near it)
  and the E5 enemy position in the relay inside the shipped exe.
- Not exercised in the game, only by protocol tests: pickups, cutscene (matinee) state, the effect of a remote push. The F6 overlay
  cannot spawn items (Spawns has only Enemies), so a check needs a real story pass.
- Check whether duplicates also appear in normal story progress, not only after overlay checkpoint loads (needs a New Game run).
- Test "Start Server" / Steam invites by hand, and two PCs over the internet (`lan_dryrun.py` covers one PC via its LAN address).
