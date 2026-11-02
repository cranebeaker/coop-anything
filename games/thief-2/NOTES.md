# Thief II: The Metal Age: co-op notes

Working notes, written as I go. Observations, derived facts, guesses and unknowns are kept apart.

## Game identity (rule 5)

| Field | Value |
|---|---|
| Store | Steam, app 211740, buildid 1970 |
| Stock exe | `thief2.exe` (OldDark 1.18 + ddfix), SHA-256 `f21cfc6d5fa73404637621e06cb20cade31a733508692fa8f7296a078dc74386` |
| Engine | Dark Engine (Looking Glass), patched by NewDark (unofficial, closed source) |
| MP exe targeted | NewDark 1.29 `Thief2MP.exe`, SHA-256 `e86b0645295e24f85747607c106beb9e94e4c21314f09156d4813c931d521fe0` (5,615,616 bytes) |
| MP exe 1.27 (reference) | SHA-256 `a22b3d0e6a254fe5247019bbd5a4d93bcfec234b51e67adf898825a442d45927`, PE timestamp 2019-06-15 |
| OS | Windows (32-bit PE, i386); tested from WSL2 on Windows |

## Downloads (pinned)

| File | SHA-256 | Source |
|---|---|---|
| `t2_v127.zip` (NewDark 1.27) | `33caca92a14acdb366211b464fbaa73113e028e9d024c9636e5a74890157ce26` | TTLG t=146448 mirror (MediaFire, via bit.ly/2KV5oDU) |
| `t2_v129.zip` (NewDark 1.29) | `c2de1addf3e61b81ec55de6b4df0154d42a174b0c2e9fce4cc3c768855183952` | Google Drive id pinned in T2Fix `fetchresources.sh`; MD5 `991b0ad2…` matches the TTLG t=153456 post |
| `T2Fix_1.29-2026-09-16.exe` | `4982266e7777683285dcf88311377257fe6be8880010458b3c6255bf2c78730f` | github.com/Xanfre/T2Fix releases (not needed in the end: Inno 6.4.3, innoextract master can't parse it) |

## Sources

- Video guide (WungusDasPungus, "How to Make Thief 2 Multiplayer Actually Work"): https://www.youtube.com/watch?v=aRiVJqx9cnQ
- NewDark 1.27: https://www.ttlg.com/forums/showthread.php?t=146448 ("added multiplayer beta (separate executable)")
- NewDark 1.29: https://www.ttlg.com/forums/showthread.php?t=153456
- T2Fix: https://www.ttlg.com/forums/showthread.php?t=149669, https://github.com/Xanfre/T2Fix
- Figgis, SS2/T2 MP improvements: https://www.systemshock.org/index.php?topic=12190.0
- Global server: https://github.com/sungaila/NewDarkGlobalServer
- Costume add-on: https://github.com/vfig/t2mpfun

## Observations

- The "Thief 2 Multiplayer" in the hub directory (2007 fan mod for OldDark 1.18) is a different project. Since NewDark 1.27
  (2019-06-15) the NewDark team ships its own MP beta as `mp.zip` → `Thief2MP.exe`. The video and T2Fix use that one.
- 1.28 and 1.29 release notes list MP fixes for server-list and readables crashes and centered name labels. Nothing about
  avatar/ghost movement (release_notes.txt in t2_v129.zip).
- `mp_release_notes.txt` in 1.29's `mp.zip`: a beta, "do not expect a fully working game"; some missions have
  completion conditions only the host player can perform; an alive player revives a dead one by frobbing them, and it's
  game over only when all players are dead; no built-in chat.
- The MP exe uses the original Looking Glass "ghost" system: RTTI `cGhostLocalHash`, `cGhostRemoteHash`; net message names
  `GhostFullHB`, `GhostNormHB`, `GhostMiniHB`, `GhostHBTerse`, `GhostRotHB`, `GhostObjHB`, `GhostAIHB`. The only config
  vars are `ghost_local_frame_rate`, `ghost_local_process_ms`, `ghost_spew`, so no knob for remote smoothing.
- Figgis (2023) lists as unsolved: "Sometimes a remote player can get stuck on geometry and then warp to the proper
  position after a while." This matches the owner's bug report.
- Figgis's player-cap byte "offset 0x2F440 in Thief2MP.exe": in the 1.27 exe from t2_v127.zip that file offset is a function
  prologue (`6a ff 68 …`), not `04`. Resolved: the constant is at file offset 0x2F4FF in 1.27 and 0x3813F in 1.29
  (`mov [edi+0x136], 4`, see "Player cap" below); the offset is wrong, the finding (cap N+1) is right. The Google
  Drive patched exe from the video description returns 404 (checked 2026-10-26).
- YouTube transcript points: 4th player joining kicks two others (Figgis's patch fixes it); server browser crash in T2Fix
  1.27 (fixed in 1.29 per notes); teleports in FMs break sessions; items picked up by a client can vanish.

## Derived (from the Ghidra 12.2 decompile of the 1.29 MP exe; addresses are preferred VAs, base 0x400000)

Ghost (network object movement) pipeline:

| Address | Role |
|---|---|
| `0x5516e0` | per-frame ghost tick (dt ms): each local ghost `0x5556a0`, then each remote ghost `0x554980` |
| `0x5556a0` | local ghost: run the receiver's predictor on our own state, ask `0x554fe0` whether to send, send with `0x554d40` |
| `0x554fe0` | send decision. Builds the heartbeat (pos from phys model +0x34), then sends only if the receiver's prediction is off by more than 4 units on an axis (`0x81a108` = 4.0 × per-ghost scale), velocity off by 20, heading off, mode change, or 30 s keep-alive. Minimum interval by ghost type table `0x889f1c` = {0, 500, 250, 100} ms; type 3 = player |
| `0x5539d0` | receive heartbeat (all message handlers `0x552620..0x552800` funnel here). Copies into the remote ghost's info block (+0x0c); adds the `AvatarZOffset` property to z |
| `0x553bd0` → `0x553520` | receiver prediction: gravity if not grounded, blend velocity, pos += vel·dt, then a raycast (`0x54a880`) that stops the prediction at walls |
| `0x553dc0` | steer the avatar toward the prediction: error d = pred − physics position. If \|d\|² > 144 (`0x81a9a0`) or flag 0x200000: warp via `0x5534c0` (which fails if the physics spheres don't fit there, `0x547220`). Else set a physics velocity d·2.0 (`0x819aa0`) or predicted velocity + d, so the avatar is moved by local physics, with collisions |
| `0x516c90` | object → physics model (`+0x34` position) |
| `0x41a940` | engine command line (EAX = string) |

Conclusion: the remote avatar is a locally simulated physics body chasing a coarse guess. It may be up to 12 units away
before the engine snaps it, collides with door frames and stairs that the real player passed, follows a gravity-and-raycast
prediction (sinks or floats until the next update), and the sender only corrects when the guess is 4+ units off. That
matches all reported symptoms. There is no config var for any of these thresholds.

UI and input facts (for test automation):
- The window procedure (`0x6999d0`) handles `WM_ACTIVATEAPP(FALSE)` by calling a pause method (vtable +0x5c), so an
  inactive window freezes the whole game loop (also what Figgis saw: saves stall until everyone alt-tabs back).
- Mouse events go to the Looking Glass mouse queue `0x689480` ({x, y, type, time, buttons}).
- `screen_dump` captures the 3D back buffer only, which is empty in menus.
- With the Windows session locked, D3D can't present and nothing can be tested live.

## Fix: T2MPFix (mod/, dinput.dll proxy)

Four independent fixes, one module each, each switched by its own `T2MPFix.ini` key (`mod/src/fixes.h`). A fix
that is off or whose signatures don't match hooks nothing; `T2MPFix.log` has one status line per fix and an
`active:` line. The core (`main.c`) owns the ghost hooks (`0x5516e0`, `0x554fe0`, `0x554d40`, `0x553dc0`) because QA
needs some of them with every fix off; the avatar fix plugs in through `ghost_send_force` and
`ghost_steer_pre`/`ghost_steer_post`, and each hook is installed only when a fix or QA needs it
(`ghost_hooks_needed`, unit tested). With all fixes and QA off nothing is hooked. The steer function `0x553dc0` is
hooked only for `recv_fix`: hooking it changes stock steering (fix 2), so telemetry reads the remote players by
walking the remote ghost hash `0x8c5a80` around the ghost frame instead.

1. Smooth remote players, two halves:
   - `send_fix` (`fix_send.c`): for the local player ghost, `0x554fe0` is forced to "send" every 50 ms while
     moving/turning (plus one "stopped" heartbeat). Measured 16.3 heartbeats/s while moving vs 1.3/s stock.
   - `recv_fix` (`fix_recv.c`), only for senders proven to run `send_fix` (≥ 12 heartbeats/s while moving, which
     stock can't reach): per remote player, buffer received states (receive time, pos, vel, heading) and show the
     state at now − delay (delay = 1.5 × mean gap + 2 × jitter, clamped 60..300 ms; no gravity in extrapolation,
     capped 100 ms). The interpolated state is written into the ghost's prediction block and flag 0x200000 makes the
     stock steer code warp there. If the warp refuses (body doesn't fit), `0x559c30` (ObjPosUpdate) places it
     anyway. Never needed in tests so far.
   - Alone: `send_fix` gives receivers ~16 heartbeats/s, but their stock steering still sticks on the same geometry
     (path_dev no better than stock); `recv_fix` does nothing until a sender runs `send_fix`. Both pinned by
     `regress.sh` (send_only, recv_only).
2. `steer_fix` (`fix_steer.c`): a 6-byte patch in the stock steering, see "Stock steering reads uninitialized
   stack" below.
3. `max_players` (`fix_players.c`): `SetMaxPlayers(N+1)` once the net manager exists (below).
4. `damage_fix` (`fix_damage.c`): the damage filter hook, see "Lava does no damage" below.

## Test results (2026-10-29, version 0.3, build `dinput.dll` `817cbee6…`)

Build: reproducible (two clean builds byte-identical). Against `070f9688…` (the 8-player tests below) the DLL source
differs only in the version string, comments and the QA-only `place` command (unused unless QA is on). Host tests: `make -C mod test` and `tools/qa/test_analyze.py` pass.

`tools/qa/regress.sh`, ALL PASS, exit 0 (mission 1, 70 s per scenario):

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

- Stock's max depends on a few snaps and varies between runs: 7.65 (2026-10-28), 17.66, 14.34 and 14.87 (2026-10-29); p95
  6.5–7.3. send_fix only stays no better than stock, usually a bit worse (p95 9.8 vs 7.1 here).
- Scorer self-check: the same scorer with an impossible bar prints FAIL and exits 1; a crash dump written during the
  run also fails it; with the real bars it exits 0.
- `MAXP=8 players.sh 4` PASS (twice): cap 4 -> 9, each lane sees exactly 3 remote players in every second of 60 s.
  `players.sh 3` PASS (cap 4 -> 4, each sees 2).
- **`max_players=N` does not keep an extra player out unless N = 8.** `players.sh 3 client3` FAIL, then by hand: with
  3 in the lobby (Host, Client1, Client2) and `max_players=3` (cap 4), Client3's join was accepted; the lobby then
  showed "Host, Client3" and Client1 got "Network Connection Lost!". The stock bug, one player later. At 8 the 9th is
  kept out by DirectPlay (below), which is why the shipped value is 8. Docs now say "keep 8".
- `players.sh 8 client8` FAIL on 2026-10-29 at 04:00 with 189 MB of RAM free after launching 9 instances (the passing
  run below had 1.2 GB): the host was back at the main menu when the lobby was checked. The host log was overwritten,
  so "never got into the lobby" vs "was dropped" is unknown. A rerun needs about 5 GB of free RAM.
- `lava.sh`: the first `lava.sh 1` FAILED because the timed walk stopped on the trough's rim (screenshot: player in
  front of the lava), not in it; reruns PASS. The walk missed in 4 of 10 attempts. The script now checks the
  player's last telemetry position (in the trough around (80, 29, -4.8), rim positions have z 1.5–3.0), retries up
  to 3 times and fails as a setup error. Without that check a `damage_fix=0` run on the rim would have been a hollow
  PASS. Then the walk was replaced by a QA command, `place x y z` (`obj_pos_update` on the local player, keeping its
  facing): `lava.sh` places the player at (80, 29, -4) and still checks the position. Build `817cbee6…`: `lava.sh 1`
  PASS (at 80.0 29.0 -5.6, damage applied 3 times, dies), `lava.sh 0` PASS (at 80.0 29.0 -3.9, standing in the lava
  at full health), first try each.
- Harness: `lanes.py kill` now stops only lane processes (no Thief2MP left after any run); suites refuse a locked
  session (`check_unlocked`, also checked with a process name that is running: exit 1 with the reason).

## Test results (2026-10-28, modules, build `dinput.dll` `04d806c9…`, mission 1)

`tools/qa/regress.sh`, ALL PASS (stock = steer function not hooked, i.e. the real game):

```
PASS stock localhost (control)    travelled  260.3  path_dev p95 6.481 max 7.652  off>1  3896  jumps 1  lag p50 3.04  hb/s 1.3  fixed 0/6519
PASS fixed localhost              travelled  260.1  path_dev p95 0.003 max 0.127  off>1     0  jumps 0  lag p50 0.30  hb/s 16.5  fixed 6209/6527
PASS fixed 60+-30ms 2% loss       travelled  259.8  path_dev p95 0.004 max 0.125  off>1     0  jumps 0  lag p50 0.62  hb/s 15.7  fixed 6182/6517
PASS fixed 150+-75ms 10% loss     travelled  257.8  path_dev p95 0.013 max 0.236  off>1     0  jumps 0  lag p50 0.98  hb/s 11.6  fixed 6093/6503
PASS send_fix only                travelled  259.8  path_dev p95 9.177 max 12.000  off>1  3482  jumps 2  lag p50 1.68  hb/s 16.5  fixed 0/6519
PASS recv_fix only                travelled  259.7  path_dev p95 2.401 max 3.060  off>1  2668  jumps 0  lag p50 1.02  hb/s 1.3  fixed 0/6529
PASS steer_fix only               travelled  257.1  path_dev p95 2.378 max 3.006  off>1  2702  jumps 1  lag p50 1.09  hb/s 1.4  fixed 0/6514
PASS mixed (mover without fix)    travelled  257.3  path_dev p95 2.438 max 3.124  off>1  2667  jumps 1  lag p50 1.08  hb/s 1.3  fixed 0/6396
```

- send_fix only = a T2MPFix player seen by a player without it: worse than stock (the stock steering's no-correction
  branch with more frequent heartbeats; 2 snaps at 12 units).
- recv_fix only: steer_fix off but the result matches it, because the uninitialized read happened to go the good way
  behind this build's `hk_steer`. Not something to rely on.
- Each fix alone, every other fix off:
  - `max_players=4`: 4 instances in mission 1 for 60+ s, the host sees 3 remote players every frame. Control with
    `max_players=0`: client1 "Network Connection Lost!" in the lobby, host and client3 stuck at "Waiting...".
  - `tools/qa/players.sh` (2026-10-29, `070f9688…`, every other fix off): `players.sh 5` and `players.sh 8 client8`
    PASS. The lobby lists 8; in mission 1 every lane sees exactly N-1 remote players in each second of 60 s; host log
    `cap field 4 -> 9`. The 9th (client8) stays on "Joining Game..." (no error shown), never gets in, nobody drops.
    About 480 MB of RAM per instance in the mission; 9 instances left 1.2 GB free of 15 GB.
  - Without the clamp (= the v0.2 DLL `148633d5…`), `max_players=9`, 9 instances: cap field 4 -> 10, yet the lobby
    stops at 8. The 9th sits on "Joining Game..." for 2+ minutes with no error (its loop still runs) while the 8 start
    the mission and each sees exactly 7 remote players for 60 s. DirectPlay's 8 is the real limit; the clamp only
    makes the log say so.
  - Shipped default `max_players=8` with a usual group: `MAXP=8 players.sh 4` PASS (cap 4 -> 9, each lane sees 3).
  - `damage_fix=1` (`tools/qa/lava.sh 1`): host alone on Soulforge walks into the lava; log shows damage from obj 1739
    (`Water Flow Group`) to the player, "Host has died". `damage_fix=0`: standing in the lava at full health.
  - Startup log for all on, each one off, and all off: each off fix logs "off in T2MPFix.ini" and installs nothing;
    with all off and QA off there is no hook and no patch.

## Test results (2026-10-26, NewDark 1.29 MP, Steam build 1970, 2 instances on one PC, mission 1)

The "stock" rows here were measured with the steer function hooked for telemetry, which changed stock steering (see
"Stock steering reads uninitialized stack"): they are closer to `steer_fix` than to the real game.

Harness: `tools/qa/run_ab.sh` (A/B: identical session and bot, only the ini differs), metric `tools/qa/analyze.py`.
One player runs `tools/qa/bots/walker.txt` (walk, run, jump, strafe, turn, crouch, back, circle; 30 s loop), the
other stands still; 65 s measured. path_dev = distance of the shown avatar from the true path (lag excluded).

| Run | path_dev p95 | path_dev max | frames >1 unit off path | lag p50 | heartbeats/s |
|---|---|---|---|---|---|
| stock, client moving, localhost | 2.371 | 3.031 | 2338 / 5926 | 0.982 | 1.3 |
| T2MPFix, client moving, localhost | 0.002 | 0.077 | 0 / 5926 | 0.314 | 16.3 |
| stock, host moving, localhost | 2.367 | 3.041 (lag max 14.3, 1 warp) | 2360 | 1.053 | 1.3 |
| T2MPFix, host moving, localhost | 0.002 | 0.066 | 0 | 0.307 | 16.3 |
| stock, 60±30 ms, 2% loss | 2.428 | 3.38 | 2171 | 1.054 | 1.3 |
| T2MPFix, 60±30 ms, 2% loss | 0.004 | 0.094 | 0 | 0.648 | 16.3 |
| stock, 150±75 ms, 10% loss | 2.651 | 7.13 | 2328 | 1.291 | 1.3 |
| T2MPFix, 150±75 ms, 10% loss | 0.011 | 0.249 | 0 | 1.059 | 16.3 |

Network impairment is applied in the DLL in front of the heartbeat receiver (QA only; doing it there is simpler than putting
the hub's UDP chaos relay in front of DirectPlay's ports). Visual check: screenshot of the host's view
shows the client's avatar on the ground at the telemetry position (tools/qa shots, not committed).

The game transport is DirectPlay: Windows offers to install DirectPlay on the game's first launch, and the exe has
`Could not initialize DirectPlay` (0x683f60), `DPlay initialization error: %s (0x%X)`, `DPlayException` and
`cCDPlayer` RTTI. The exe imports no sendto/recvfrom/bind; DirectPlay's own DLLs do the socket work.

- Player cap: `cNetManager` field +0x136 (default 4 written at `0x438d39`), `SetMaxPlayers` = vtable slot 40
  (`0x43a230`, stdcall). Stock lobby with 4 instances: Client3 joining kicked Client1 and Client2 (reproduced).
  With the cap set to 5: all 4 stay and play.
- Upper bound: the host's DirectPlay8 `DPN_APPLICATION_DESC` (built at `0x68424d`: dwSize 0x48, flags 0x40 NODPNSVR)
  has `dwMaxPlayers = 8` (`mov [ebp+0x28],8` at `0x684254`, the only such write; no SetApplicationDesc later), so
  DirectPlay refuses a 9th peer whatever the game cap is. `max_players` is clamped to 8. The join handler
  (`0x437c20`) sends the joiner the other players' ids in a fixed `int[8]`, fine up to 10. 5 and 8 tested live (below).

## Stock steering reads uninitialized stack (fixed by `steer_fix`)

Found when the module split moved code around in `hk_steer` and the *stock* control got worse (path_dev max 3.0 ->
7.5, lag p50 1.0 -> 3.0) with the same game. A/B with old and new DLLs split by side: only the receiver's DLL mattered.

`0x553dc0` (steer), moving player (`|pred vel|^2 > 1`), at `0x553f07`: `fld [esp+0x14]; fld [esp+0x10]; fld [esp+0x18]`,
`|.|^2` compared with 1.0, `jp 0x553fbd`. Those three floats are never written on this path (Ghidra: `local_24/20/1c`).
- `|garbage|^2 <= 1` -> `0x553fbd`: physics velocity = predicted velocity only. The avatar never closes the gap and
  trails until the 12-unit warp.
- otherwise: velocity = (pred - pos) + predicted velocity (pulls toward the prediction, time constant 1 s).
- The slots hold whatever the previous call at that stack depth left, so any hook in the call chain (our old
  `hk_steer`) changes the result. The old DLL happened to leave large values; the plain game doesn't.

Measured (host observes, client1 walks with stock sending, 70 s, mission 1):

| Receiver | path_dev p95 | max | lag p50 |
|---|---|---|---|
| real stock (steer not hooked, telemetry from the ghost hash) | 6.63 | 7.75 | 3.03 |
| stock behind the new `hk_steer` (no fix) | 6.49 | 7.52 | 3.06 |
| stock behind the old `hk_steer` (no fix), the "stock" of the earlier tables | 2.39 | 3.00 | 1.09 |
| patch: read `d = pred - pos` instead ("correct only when > 1 unit off") | 5.63 | 8.11 | 1.15 |
| patch: drop the branch (always correct), shipped as `steer_fix` | 2.37 | 3.04 | 1.09 |

So the earlier "stock" rows (path_dev max ~3) were stock plus our hook's lucky stack; real stock is ~7.7 here (later
runs: p95 6.5–7.3, max up to 17.7, set by a few snaps). The
`d` variant is the code's likely intent but is worse here, so `steer_fix` NOPs the `jp` at `0x553f35`
(`0f 8a 82 00 00 00` -> `66 0f 1f 44 00 00`), after checking 52 bytes from `0x553f07`.

## Lava does no damage in multiplayer (fixed by `damage_fix`)

Observed: Soulforge (mission 15, `miss16`): the lava trough next to the start does nothing in `Thief2MP.exe`, kills in
`thief2.exe`; the ladle (a physical object) hurts in both. Reproduced with all T2MPFix hooks off and `max_players=0`.

Traced with a throwaway logging build, step by step (each step logged on the live game):
- The lava is a water room whose cells carry flow group 2. Object 1739 (`Water Flow Group`, `FlowGroup` = 2) has
  stim sources FireStim and WaterStim. On a medium change the physics media callback (`0x486d00`) calls the flow
  propagator (`0xab9a70`, a `cPeriodicPropagator`), which links 1739 -> player and sends FireStim (intensity 14) now
  and every 1000 ms. This all works in MP.
- The player's receptrons: `M-GarrettDiffNormal` Amplify 0.8 (-> 11.2), then `FireDamaged` `damage` (effect 21,
  handler `0x596fc0`), which calls `IDamageModel::DamageObject` (`0x5a4da0`) with victim = player, culprit = 1739,
  amount 11-12.
- `DamageObject` runs the damage filters (`0x5963f0`, array at model+0x20). Filter `0x44bce0` (MP only, `__stdcall`
  (victim, culprit, msg, data)) zeroes the amount and returns 1, so `DamageObject` returns 1 ("status quo").
- Its logic: allow if not networking or if the victim is local-only; if the culprit is hosted here and the victim
  is a proxy, send the damage to the owner (`0x44c080`) and zero it locally; if the culprit is neither hosted here
  nor a proxy, zero it unless it has `LocalCopy` (the out value of that property query reuses an argument slot, so
  a missing property also blocks). Measured: player f1c 0 / f20 1 / f24 0, lava 1739 f1c 0 / f20 0 / f24 1
  (`IObjectNetworking` at `0xab83ac`, slots +0x1c proxy, +0x20 hosted here, +0x24 local only).
- Result: any damage whose culprit is a Local Only object is dropped on every machine. `dark_net.dml` makes
  `physical` objects Hosted, which is why the ladle works.

Fix: hook the filter; if the culprit is local-only and the victim is hosted here (not a proxy), return 0 (no
opinion) and let the damage through. Each machine applies it to the objects it owns (its own player; AI on the
host); proxies still go through the stock filter and stay blocked, so nothing is applied twice.
Verified: host and client each die within ~2 s in the Soulforge lava; the other player keeps full health.

## Lessons (candidates for playbook/lessons.md)

Sent to coop-anything `playbook/lessons.md`: `ret N` and `-mstackrealign` = 60, the hook that changes stock steering =
62 (61, the path-deviation metric, comes from the QA section), log each hop = 64, locked session = 65. The focus
one is already covered by lesson 23 and `playbook/methods.md`.

- `0x516c90` is `__stdcall` (`ret 4`) although Ghidra shows a plain one-argument function. Calling it as cdecl drifted
  ESP by 4 per call and crashed both games on the first in-mission frame. Check `ret N` of every engine function you
  call, not only the decompiler's signature.
- Hooks called from MSVC code get a 4-byte-aligned stack; clang/gcc for i686 assume 16 and emit `movapd` to the stack.
  Build hook DLLs with `-mstackrealign`. The fault shows up as an access violation at address 0xffffffff.
- Thief2MP pauses its whole loop on `WM_ACTIVATEAPP(FALSE)`, and only accepts injected mouse events while its window
  is *not* the foreground window. For two instances on one desktop: subclass the window, swallow deactivation, send a
  synthetic activate, and keep both windows in the background. Mouse coordinates are in a space scaled 1.28 from the
  800x600 client (menus are 640x480-based).
- When a stimulus "does nothing", log each hop (propagator -> sensor -> receptron -> effect -> damage model ->
  filters) on the live game before reading more decompile: here the decompile of the stimulator had "unreachable"
  blocks removed, and the real cause was a damage filter three layers further down.
- A hook in front of an engine function can change that function's behaviour without touching its data: the
  stock steer code reads stack slots it never writes, so what our hook left there decided whether avatars drift.
  Measure the stock control with nothing hooked in that call chain, and A/B a refactor's *stock* mode too.
- A locked Windows session makes every visual or input test fail silently (D3D can't present); check `LogonUI` first.

## Unknowns

- Remote players on elevators and moving platforms (heartbeats relative to an object fall back to stock steering),
  ladders, rope arrows, mantling, swimming, saves/loads, mission changes, fan missions, a session over the
  Internet (network impairment was only simulated in the DLL): not tested.

Resolved: player heartbeat rate and warp threshold (see "Derived"); several `Thief2MP.exe` instances run on one PC
(tested with 4).
