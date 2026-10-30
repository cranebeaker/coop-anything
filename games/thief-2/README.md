# Thief II: The Metal Age (2000)

| | |
|---|---|
| Engine | Dark Engine, patched by NewDark (closed source); multiplayer = NewDark's "MP beta" `Thief2MP.exe` |
| Versions tested | Steam (app 211740, buildid 1970) + NewDark 1.29 MP beta, `Thief2MP.exe` SHA-256 `e86b0645…1fe0` |
| Projects | NewDark multiplayer beta (since 1.27, 2019); [T2MPFix](https://github.com/D3c3453d/T2MPFix): four separate fixes (remote player movement, a stock steering bug, player limit, environmental damage). Directory: "NewDark multiplayer beta (Thief2MP.exe) + T2MPFix" in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated |
| Last updated | 2026-10-29 |

## Where things stand

NewDark's MP beta plays the original missions in co-op (2–3 players stock, up to 8 with T2MPFix; everyone runs the same
version). Its own release notes (`mp_release_notes.txt` in `mp.zip`) call it a beta: "do not expect a fully working
game", and some missions have completion objectives only the host can do.
Out of the box, other players' avatars are shown in the wrong place (lagging, stuck on geometry, floating, sinking,
warping, partly because the engine's steering reads uninitialized memory), the last player to join drops others
(e.g. a 4th player kicks two), and hazards each game runs locally do no damage (e.g. lava). T2MPFix, a `dinput.dll`
proxy every player drops into the game folder, has four separate fixes for these, each switched independently in
`T2MPFix.ini`. Measured with two to nine real game instances and a movement bot (below), in missions 1 and 15 only.

## Existing work

- NewDark 1.27/1.28/1.29 ship `mp.zip` (`Thief2MP.exe`, multiplayer-safe OSMs, `dark_net.cfg`): TTLG t=146448, t=153456.
  T2Fix bundles it as the "Thief 2 Multiplayer Beta" component (github.com/Xanfre/T2Fix).
- The older "Thief Multiplayer" fan mod (2007–2010, OldDark 1.18) is a different project (see the directory entry).
- Figgis (systemshock.org topic 12190, 2023): player cap analysis for SS2 and T2 and a patched exe (Google Drive link
  from the 2024 video guide now returns 404). His T2 byte offset (0x2F440) does not match the 1.27 exe from
  `t2_v127.zip`; the constant is at file offset 0x2F4FF there and 0x3813F in 1.29 (`mov [edi+0x136], 4`).
- Video guide: WungusDasPungus, "How to Make Thief 2 Multiplayer Actually Work" (youtube.com/watch?v=aRiVJqx9cnQ).
- Global server for the in-game browser: github.com/sungaila/NewDarkGlobalServer.

## How the game works (for co-op)

The engine's original Looking Glass "ghost" system moves networked objects (details and addresses in `NOTES.md`):
the owner sends a heartbeat only when the receiver's dead-reckoned guess is more than 4 units off on an axis
(minimum 100 ms apart for players); the receiver predicts with gravity and a raycast that stops at walls, then steers
the avatar's physics body toward the guess with a velocity, colliding with the world on the way, and only teleports it
when the error passes 12 units. Whether that steering pulls a moving avatar toward the guess at all depends on three
stack floats the code never writes; in the plain game it doesn't, so avatars trail several units behind. The game
transport is DirectPlay (Windows offers to install it on the first launch; the exe's error strings name it). No config
var controls any of it.

## What we changed

Four independent fixes, one source file and one `T2MPFix.ini` switch each. A fix that is switched off (or whose
engine bytes don't match) hooks nothing, so if NewDark fixes one of these itself, that fix alone can be turned off.
`T2MPFix.log` lists which fixes are on.

1. **Smooth remote players**, two halves:
   - `send_fix` (`0x554fe0`): while a player moves or turns, send a heartbeat every 50 ms.
   - `recv_fix` (`0x553dc0`): buffer received states and show the player on the interpolated path a small adaptive
     delay behind (60–300 ms), placed each frame by the engine's own warp (`ObjPosUpdate` fallback if the warp
     refuses). Players whose stream is at stock rate (no `send_fix`) keep the stock behaviour.
2. **Stock steering bug**, `steer_fix` (`0x553f07`): for a moving remote player the stock steer code decides whether
   to pull the avatar toward its prediction by testing three stack floats it never wrote. In the plain game the test
   fails and avatars trail several units behind (path_dev p95 ~7); a hook in that call chain can flip it, which is
   how this was found. A 6-byte patch drops the test (always pull). It matters for players `recv_fix` doesn't
   interpolate (those without T2MPFix).
3. **Player limit**, `max_players` (`cNetManager::SetMaxPlayers`, vtable slot 40): set to N+1 for N players, per
   Figgis. At the stock cap the last player to join gets others kicked (stock's cap is 4, so 3 fit and a 4th joiner
   kicks two). The shipped ini says 8, the most there can be: the host's DirectPlay8 session has `dwMaxPlayers = 8`
   (`0x684254`), so larger values are clamped. A 9th player waits at "Joining Game..." and drops no one. A smaller
   N only moves the bug: with `max_players=3` a 4th joiner still kicked two players, so the ini says to keep 8.
4. **Environmental damage**, `damage_fix` (`0x44bce0`): the MP damage filter zeroes all damage whose culprit is a
   Local Only object, on every machine (e.g. lava, a `Water Flow Group`). The hook lets it through on the machine
   that hosts the victim, so each player takes damage from their own copy of the hazard and nothing is applied twice.

## QA

Paths below are in the T2MPFix repo (https://github.com/D3c3453d/T2MPFix).

- `mod/tests/`: host unit tests for the interpolation (6 cases incl. teleport, idle gap, heading wrap; mutation runs
  fail as expected), the local-only damage rule, and which hooks each combination of fixes installs.
- `tools/qa/test_analyze.py` (metric self-test incl. stuck, floating, standing-still runs).
- `tools/qa/regress.sh`: real games on one PC (DLL-side QA channel drives the menus, bots drive the players,
  telemetry on both sides, network impairment in the DLL). "stock" is the real game: the steer function is not hooked
  (telemetry walks the ghost table). Last run 2026-10-29, build `dinput.dll` `817cbee6…` (version 0.3):

```
PASS stock localhost (control)    travelled  285.8  path_dev p95 7.052 max 14.870  off>1  4346  jumps 2  lag p50 3.03  hb/s 1.3  fixed 0/7119
PASS fixed localhost              travelled  286.1  path_dev p95 0.003 max 0.134  off>1     0  jumps 0  lag p50 0.30  hb/s 16.4  fixed 6802/7120
PASS fixed 60+-30ms 2% loss       travelled  285.2  path_dev p95 0.005 max 0.209  off>1     0  jumps 0  lag p50 0.61  hb/s 15.6  fixed 6780/7112
PASS fixed 150+-75ms 10% loss     travelled  285.2  path_dev p95 0.006 max 0.245  off>1     0  jumps 0  lag p50 0.98  hb/s 11.4  fixed 6710/7100
PASS send_fix only                travelled  287.0  path_dev p95 9.764 max 11.877  off>1  4154  jumps 2  lag p50 2.13  hb/s 16.4  fixed 0/7119
PASS recv_fix only                travelled  285.9  path_dev p95 2.676 max 3.658  off>1  2996  jumps 1  lag p50 1.13  hb/s 1.3  fixed 0/7122
PASS steer_fix only               travelled  286.7  path_dev p95 2.626 max 3.509  off>1  2981  jumps 1  lag p50 1.12  hb/s 1.3  fixed 0/7126
PASS mixed (mover without fix)    travelled  284.3  path_dev p95 2.578 max 3.424  off>1  2816  jumps 1  lag p50 1.09  hb/s 1.3  fixed 0/6891
ALL PASS
```

(path_dev: distance in Dark units of the shown avatar from the player's true path; "off>1": frames more than 1 unit
off it, out of ~7100.) Stock's max depends on a few snaps and varies between runs (7.7, 17.7, 14.3, 14.9 in the last four);
its p95 stays at 6.5–7.3. "send_fix only" is a T2MPFix player seen by a player without it: no better than stock,
usually a bit worse (p95 9.8 vs 7.1 here), so everyone should install it. Earlier tables (README history, NOTES)
called "stock" what was really stock behind our steer hook, which happened to avoid the steering bug (max ~3.0).

Each fix alone, with every other fix off:
- `max_players=4` (earlier build): 4 instances join and play; control with `max_players=0`: the 4th joiner drops
  the others.
- `tools/qa/players.sh` (version 0.3): `MAXP=8 players.sh 4` and `players.sh 3` PASS (each instance sees all the
  others every second for 60 s). With `max_players=3` a 4th joiner kicked two players (see fix 3).
- `tools/qa/players.sh` (build `070f9688…`, same fix code as 0.3): 5 and 8 players PASS. A 9th instance stays at
  "Joining Game..." and drops no one, also with the clamp removed (the cap was 10; DirectPlay is the limit). The
  8+1 rerun on 0.3 failed with 189 MB of RAM free and wasn't repeated (details in NOTES).
- `damage_fix=1` (`tools/qa/lava.sh`, version 0.3): the player dies in Soulforge's lava; `damage_fix=0`: unhurt
  (the stock bug). A QA command places the player in the lava; telemetry confirms the position before scoring.
- All fixes off and QA off: nothing hooked or patched.

Earlier builds with the same fix code: 4 players with 2 moving, in-mission path_dev max ≤ 0.64; 3 players, 5 minutes,
2 moving, max 0.23, no crash.

## How to play

1. Everyone: T2Fix 1.29 with "Thief 2 Multiplayer Beta" (or NewDark 1.29 + `mp.zip` by hand).
2. Everyone: copy `dinput.dll` and `T2MPFix.ini` into the game folder.
   On the first launch Windows offers to install DirectPlay; accept it or multiplayer won't start (if it was
   dismissed, `INSTALL.txt` says how to turn it on in Windows features).
3. Host: forward port 5198 (NewDark MP release notes; 5199 too for the global server, per the video) or use a LAN VPN.

In the game, a dead player is revived when another player frobs them; it's game over only when everyone is dead
(NewDark's MP release notes).

## Known issues

- A player without T2MPFix sees players who have it no better than stock, usually a bit worse, so everyone in the
  session should install it.
- Some missions have completion objectives that only the host can do (NewDark's MP release notes).
- AI and other networked objects still move with the stock ghost code.
- Alt-tabbing still pauses that player's game, which stalls things like saving for everyone (stock behaviour).
- Not covered by tests: other players on elevators and moving platforms (these fall back to the stock
  movement), ladders, rope arrows, mantling, swimming, saving and loading, mission changes, fan missions, and a
  session over the Internet (lag and packet loss were only simulated).

## Next steps

- Extend the walker bot to stairs, ladders and elevators in a known mission; add a save/load and mission-change test.
- Apply the same interpolation to AI ghosts.
- Optional "keep running in background" for players (the QA subclass already does it).
- `max_players` below 8: refuse joiners beyond N instead of letting them kick others (today only 8 is safe).
