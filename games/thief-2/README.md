# Thief II: The Metal Age (2000)

| | |
|---|---|
| Engine | Dark Engine, patched by NewDark (closed source); multiplayer = NewDark's "MP beta" `Thief2MP.exe` |
| Versions tested | Steam (app 211740, buildid 1970) + NewDark 1.29 MP beta, `Thief2MP.exe` SHA-256 `e86b0645…1fe0` |
| Projects | NewDark multiplayer beta (since 1.27, 2019); [T2MPFix](https://github.com/D3c3453d/T2MPFix): four separate fixes (remote player movement, a stock steering bug, 4-player cap, lava damage) |
| QA level | automated |
| Last updated | 2026-10-28 |

## Where things stand

Co-op of the original missions works with NewDark's MP beta (2–4 players, everyone runs the same version).
Out of the box, other players' avatars are shown in the wrong place (lagging, stuck on geometry, floating, sinking,
warping, partly because the engine's steering reads uninitialized memory), a 4th player joining kicks two others,
and lava does no damage. T2MPFix, a `dinput.dll` proxy every player drops into the game folder, has four separate
fixes for these, each switched independently in `T2MPFix.ini`. Measured with two to four real game instances and a
movement bot (below).

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
stack floats the code never writes; in the plain game it doesn't, so avatars trail several units behind. The exe
imports no sendto/recvfrom/bind, so the game transport is likely DirectPlay (not verified). No config var controls any
of it.

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
   fails and avatars trail several units behind (path_dev max ~7.7); a hook in that call chain can flip it, which is
   how this was found. A 6-byte patch drops the test (always pull). It matters for players `recv_fix` doesn't
   interpolate (those without T2MPFix).
3. **4-player cap**, `max_players` (`cNetManager::SetMaxPlayers`, vtable slot 40): set to N+1 for N players, per Figgis.
4. **Lava damage**, `damage_fix` (`0x44bce0`): the MP damage filter zeroes all damage whose culprit is a Local Only
   object (lava is a `Water Flow Group`), on every machine. The hook lets it through on the machine that hosts the
   victim, so each player takes damage from their own copy of the lava and nothing is applied twice.

## QA

Paths below are in the T2MPFix repo (https://github.com/D3c3453d/T2MPFix).

- `mod/tests/`: host unit tests for the interpolation (6 cases incl. teleport, idle gap, heading wrap; mutation runs
  fail as expected), the lava damage rule, and which hooks each combination of fixes installs.
- `tools/qa/test_analyze.py` (metric self-test incl. stuck, floating, standing-still runs).
- `tools/qa/regress.sh`: real games on one PC (DLL-side QA channel drives the menus, bots drive the players,
  telemetry on both sides, network impairment in the DLL). "stock" is the real game: the steer function is not hooked
  (telemetry walks the ghost table). Last run 2026-10-28, build `dinput.dll` `04d806c9…`:

```
PASS stock localhost (control)    travelled  260.3  path_dev p95 6.481 max 7.652  off>1  3896  jumps 1  lag p50 3.04  hb/s 1.3  fixed 0/6519
PASS fixed localhost              travelled  260.1  path_dev p95 0.003 max 0.127  off>1     0  jumps 0  lag p50 0.30  hb/s 16.5  fixed 6209/6527
PASS fixed 60+-30ms 2% loss       travelled  259.8  path_dev p95 0.004 max 0.125  off>1     0  jumps 0  lag p50 0.62  hb/s 15.7  fixed 6182/6517
PASS fixed 150+-75ms 10% loss     travelled  257.8  path_dev p95 0.013 max 0.236  off>1     0  jumps 0  lag p50 0.98  hb/s 11.6  fixed 6093/6503
PASS send_fix only                travelled  259.8  path_dev p95 9.177 max 12.000  off>1  3482  jumps 2  lag p50 1.68  hb/s 16.5  fixed 0/6519
PASS recv_fix only                travelled  259.7  path_dev p95 2.401 max 3.060  off>1  2668  jumps 0  lag p50 1.02  hb/s 1.3  fixed 0/6529
PASS steer_fix only               travelled  257.1  path_dev p95 2.378 max 3.006  off>1  2702  jumps 1  lag p50 1.09  hb/s 1.4  fixed 0/6514
PASS mixed (mover without fix)    travelled  257.3  path_dev p95 2.438 max 3.124  off>1  2667  jumps 1  lag p50 1.08  hb/s 1.3  fixed 0/6396
ALL PASS
```

(path_dev: distance in Dark units of the shown avatar from the player's true path; "off>1": frames more than 1 unit
off it, out of ~6500.) "send_fix only" is a T2MPFix player seen by a player without it: worse than stock, so everyone
should install it. Earlier tables (README history, NOTES) called "stock" what was really stock behind our steer hook,
which happened to avoid the steering bug (max ~3.0).

Each fix alone, with every other fix off (same build):
- `max_players=4`: 4 instances join and play; control with `max_players=0`: the 4th joiner drops the others.
- `damage_fix=1` (`tools/qa/lava.sh`): the player dies in Soulforge's lava; `damage_fix=0`: unhurt (the stock bug).
- All fixes off and QA off: nothing hooked or patched.

Earlier builds with the same fix code: 4 players with 2 moving, in-mission path_dev max ≤ 0.64; 3 players, 5 minutes,
2 moving, max 0.23, no crash.

## How to play

1. Everyone: T2Fix 1.29 with "Thief 2 Multiplayer Beta" (or NewDark 1.29 + `mp.zip` by hand).
2. Everyone: copy `dinput.dll` and `T2MPFix.ini` into the game folder.
3. Host: forward port 5198 (NewDark MP release notes; 5199 too for the global server, per the video) or use a LAN VPN.

## Known issues

- A player without T2MPFix sees players who have it worse than stock (they lag and snap more), so everyone in the
  session should install it.
- AI and other networked objects still move with the stock ghost code.
- Alt-tabbing still pauses that player's game, which stalls things like saving for everyone (stock behaviour).

## Next steps

- Extend the walker bot to stairs, ladders and elevators in a known mission; add a save/load and mission-change test.
- Apply the same interpolation to AI ghosts.
- Optional "keep running in background" for players (the QA subclass already does it).
