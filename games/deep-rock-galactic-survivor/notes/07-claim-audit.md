# Claim audit — can the current workflows prove each line of the sync contract?

2026-10-21. Rows follow [06-sync-contract.md](06-sync-contract.md). Verdict per row:
**P** = a test exists and has a negative control that fails; **W** = a test exists but is weak
(no control, or it checks the wrong thing); **M** = missing.

## What every current test shares (the blind spots)

1. **One PC, localhost, zero latency.** No test can see a race that needs a slow or late partner.
   The real test found the seed race this way.
2. **Both instances are started by the harness in a fixed, friendly order**, bots drive them,
   and the harness passes test flags (`-coopprofileadopt`, `-Auto…`) that a player never sets.
   So "works with the flags" is proven, "works as shipped" is not (host unlocks).
3. **Tests are written for what the code does**, then given a negative control. They do not start
   from a requirement. The real game found requirements nobody wrote a test for (downed player
   can move, button after a run).
4. **A run is one level, started once.** Nothing exercises run → menu → second run.
5. **Visual checks look at structure or a pixel oracle for the lobby only.** Puppet, camera and
   projectile "drawn" are proven by object counts, not by what is on screen.

## Row by row

| Contract | Verdict | Why |
|---|---|---|
| A1 config before map | **W** | fingerprint test exists, but the host is never late; the race (`haveConfig=False`) cannot occur. Needs the delayed-host twin. |
| A2 map and mission objects | **P** (census, with `-coopworlddiffhide` negative control) | measures, does not enforce; depends on A1 |
| A3 class/skin of both dwarves | **W** | `skin` test covers host→client only; the host seeing the client's class is not tested |
| A4 host unlocks | **W** | `profile-adopt` passes only with `-coopprofileadopt`; the default path is untested (and off) |
| B1 enemy state and motion | **W** | census/transport tests exist; smoothness has `enemysmooth` for interpolation, but no position-error measure at 10 Hz or under lag |
| B2 projectiles, turrets, beams both ways | **W** | host→client mirrors have tests with controls; client→host turret is not mirrored (G3), projectile count grows (G4): the census sees it, no test fails on it |
| B3 loot, no doubles | **W** | `loot` proves claims; G5 doubles are only visible in the census |
| B4 terrain | **P** | |
| B5 objectives, end, victory | **W** | `runend` forces the outcome (`-RunEndForce`); natural victory/boss never reached by bots |
| B6 barriers | **W** | `levelup`/`shop` check no timeout and card counts, **not hold time** (G1) |
| C1 position/animation | **W** | puppet exists/animates; no error-vs-real measure |
| C2 downed, revive | **W/M** | revive circle and timer have controls; "downed player cannot move/attack" and "revive completes with both present" are not tested |
| C3 damage | **P** | |
| C4 XP / cards | **P** | |
| C5 wallet / shop | **P** | |
| C6 meta separation | **P** (saves test) | single-PC saves backup/restore, verified |
| D1 start together | **W** | lobby-start works with the wizard; start-order swap and late client untested |
| D2 loading wait | **P** | `loadform`, `loadtimeout`, delay run |
| D3 second run after run end | **M** | nothing returns to the menu and starts again |
| D4 peer lost | **P** | `disconnect`, both directions, baseline FAIL |
| D5 reconnect | not required (decision 3) | |
| D6 lag profiles | **M** | no network shaper in the harness |
| E marker | **P** | `indicator`, `marker` |
| E camera | to be replaced | `camera` test asserts framing, which decision 5 retires; new assert: camera follows only the own dwarf |

## Tooling that is missing (needed to close the W/M rows)

1. **Lag/loss shaper** between the two instances, so D6, B1 and the seed race can be run. Options
   on Windows: a small UDP proxy of our own on 127.0.0.1 (delay, jitter, loss, duplicate,
   blackout) inside the mod's test mode or as a tool script. Preferred: own proxy (no driver
   install, no admin rights, reproducible); a driver such as Clumsy would need explicit approval.
2. **Late-start twin:** `-LoadDelayRole host` exists for loading; extend it to delay the host's
   run-config send, so the A1 race is reproduced with a failing baseline.
3. **Shipped-config mode:** run the harness with the release defaults and no test-only flags,
   plus a check that the log shows the same adoption/profile lines a player would get.
4. **Second-run test:** end the run, wait for the menu, assert the CO-OP button exists, start
   again, compare fingerprints.
5. **State-machine asserts:** downed → input and position frozen; revive completes within N s;
   hold-time asserted in `levelup`/`shop`.
6. **Position-error probe:** compare puppet/mirror positions to the real positions at
   fixed times (needs both logs on a common game clock; census already aligns by game time).
7. **Human-confirmed column:** a checklist a tester runs on two PCs, one line per contract row,
   so STATUS can show "automated" and "human" separately.

## Result

Only about a third of the contract rows (B4, C3–C6, D2, D4, E-marker) are provable with a
failing negative control today. Most "✅" rows measure that a mechanism exists, not that the
player-facing requirement holds. Recommended order: tooling 2 and 3 (cheap, close A1/A4), then 4
and 5, then 1 (lag shaper), then fix B1 at 10 Hz and measure under the lag profiles.

## Update 2026-10-21 (after the overnight run)

What changed since the audit above (run IDs in [../STATUS.md](../STATUS.md)):

| Contract | Before | Now |
|---|---|---|
| A1 config before map | W | **P** `lobby-start -MapSync -ConfigDelaySec 3`, twin `-coopnomapwait` |
| A3 class/skin both ways | W | **P** `-Test skin`, twin `-coopskinhostonly` |
| A4 host unlocks | W | **P** `lobby-start -ProfileScrub -AdoptCheck` without `-coopprofileadopt`, twin `-coopnoadopt` |
| B1 enemy motion | W | **P** `-Test enemyerr -EnemyErr -EnemyErrP95Max 0.8`, twin `-coopsnaphz=2` |
| B6 barriers | W | **P** `-Test barrier` (hold time, timeouts), twin `-coopbarrierold` |
| C2 downed | W/M | **P** `-Test downedlock` (movement + weapon activation), twin `-coopdownednolock` |
| D3 second run | M | **P** `-Test secondrun -MapSync -ExpectRuns 2`, twins `-coopmenubuttonsolo`, `-coopnolobbyreset` |
| D6 lag | M | **W** tool + runs exist (`-Lag`, `lagproxy.py`), no red twin possible for "no freeze" |
| E camera | to be replaced | **P** `-Test cameravanilla`, twin `-coopcameraon` |
| E start without visible clicks | - | **W** implemented, cannot be verified unattended (`OnGUI` is never called in the unattended lobby runs) |

Still **W/M** (not proven by a failing control): B2 (client turret G3, projectile growth G4), B3 (double loot G5), B5 (natural victory), C1 (puppet position error), E marker in the lobby path, ESC/pause with a partner, anything a human has to see. The "Mensch" column of `local/MANUAL-TEST.md` is empty on purpose.

**New blind spot found while doing this:** the unattended runs have no drawn IMGUI (`CoopTick.OnGUI` count 0), so every check that depends on the overlay (start cover, off-screen marker, notices) can only be run from a normal desktop session; the old `-Auto` splash path also needs a real click and does not work from a hidden shell. Use `-Ui -Lobby` for everything else.
