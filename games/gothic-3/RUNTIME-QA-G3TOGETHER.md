# G3 Together runtime QA (2026-10-17 to 2026-10-20)

Evidence behind the QA level of G3 Together in [README.md](README.md). Run IDs and series
names refer to the owner's private run folders and logs; they are not published. The
mod's source and its suites: [GGM1k3y23/g3-together](https://github.com/GGM1k3y23/g3-together). The earlier Myrtana Accord test is summarised in
[OTHER-PROJECTS.md](OTHER-PROJECTS.md).

## Tested stack

- Steam build 1037023, `Gothic3.exe` 1.75.20108.14 (Community Patch 1.75.14), x86,
  Parallel Universe Patch files present, alternative balancing on.
- Myrtana Accord and the old QA helper moved to quarantine in the test copies.
- Two pairs of full install copies (host and client each), every copy with its own
  redirected profile. Pair 1 uses UDP port 34567, pair 2 port 34568, each with its own
  QA lock and run folder.
- Both games of a pair run on one PC and connect through 127.0.0.1.

## How a suite runs

1. The runner checks the pair's isolation receipts and hashes the owner's real save
   folder.
2. It starts the host copy, waits until the in-game bot answers, sends `releaselock`
   (the game closes its own `GenomeMutex` handle), then starts the client the same way.
   A game that exits early (another game held the mutex) is started again after a random
   pause of 3 to 12 s.
3. The bot starts a new game through the menu (focus plus Enter queued to its own
   window), and the runner sends one Escape once the intro would block the loop.
4. The suite sends numbered commands (teleport, hit, take, save, dialog, ...) and polls
   the mod logs of both games for the asserted state.
5. At the end of a two-player suite the invariant monitors run (below), both games close
   through their normal path, and the runner checks clean shutdowns and the save hashes.
6. A stage needs 3 passes in a row and a failing negative control (the feature switched
   off by a QA command), with the earlier suites still passing.

## Invariant monitors (report only)

With both games running, the runner reads the NPCs near both heroes three times, 3 s
apart. A finding counts only if it holds in all three samples.

| Rule | Meaning |
|---|---|
| `dead_mismatch` | dead on one machine, alive on the other |
| `double_sim` | seen on both, mirrored on neither: both machines simulate it |
| `foreign_ai` | a mirror runs a task of its own |
| `drift` | alive on both, more than 5 m apart |
| `hp_mismatch` | alive on both, different hit points |
| `no_motion` | alive, no motion at all (T-pose) |

Self-test `invariantself`: the client stops taking the host's NPC snapshots, so both
machines simulate the same NPCs; the monitors must report `double_sim`. Earlier
development runs of the self-test failed (no violation found before the snapshot stream
was cut). With the final version it passed in `20261019-175827-together` and
`20261019-194507-together`; its negative control `20261019-180006-together` (nothing
cut) reported nothing and failed as required. Run `20261019-194507-together` reported
`double_sim`, `drift` and `hp_mismatch` on raiders and the Ardea companions.

## Runtime trace

Off by default; QA command `trace on|all|off` or `trace = on` in the INI. It hooks the
engine's motion start and the game's task setters, and writes `g3trace.log` in the game
folder: time, body (mirror or puppet), motion or task, and the caller as module+RVA.
Suite `tracediag` passed 3 of 3, the INI path 1 of 1, and its negative control (no file
written) behaved as required. The first `tracediag` run failed in the harness: the
runner could not delete a `g3trace.log` the game still held.

## Results, 2026-10-19

| Series | Runs | Result |
|---|---|---|
| Full regression (37 suites incl. V1 to V5) | series `20261019a` | 36 of 37 passed. `stage2` failed once: the client's puppet ran `ZS_HangAround` for about 2 s on the host. `stage2` then passed 4 times. Cause found and fixed on 2026-10-20 (below) |
| Stage T and neighbours | series `20261019b` | `stageT` 3 of 3; 14 more suites passed; `stageE3` failed once (Lester did not follow after a teleport; game AI), passed on 2 repeats |
| Parallel probe, both pairs at once (`stage2`, `stage3b`, `stageA` on each) | `parallel-probe-p1`, `-p2` | 5 of 6 passed; the red one was the known rare `stage2` puppet turn. One start hit the mutex and was retried automatically |
| Full regression with invariant monitors (38 suites) | `20261019-180209-together` to `20261019-191307-together` | 37 of 38 passed. Monitors found no lasting violation in any suite. `stage3c` failed ("Client earned 500 XP for kills it had no part in"): since stage E1 a client may own raiders and complete the raid quest itself, whose shared reward is allowed. Test adjusted; `20261019-192246-together` passed, negative control `20261019-192435-together` failed as required |
| P3a (companion and summon kills) | series `20261019-p3a`, `-p3a2` | 3 of 3, negative control `partyxpoff` failed as required; `stageE3`, `stageE4`, `invariantself` passed |
| Stage L (jump to partner) | `20261019-194701-together` to `-195110-together` | 3 of 3, negative control failed as required |
| P1 meteor (pair 2) | series `p2-stageP1` | 3 of 3, negative control `summoncopyhero` failed as required (the copy killed the target a second time) |
| Pre-package regression (9 suites) | series `20261019-pre-package` | 8 of 9 passed. `stage3b` failed in `20261019-195533-together`: the orc parried the test hit (`ZS_ParadeStumble`, 0 damage, nothing to report). The test now swings up to 3 times when the target parries. Rerun `20261019-201518-together` passed, negative control `20261019-201712-together` failed as required |
| After the runtime trace (7 suites) | series `20261019-trace-reg` | 7 of 7 passed (`stage3b`, `stageA2`, `stageE2`, `stageE3`, `stageV4`, `stageL`, `stageP3a`) |
| Merged `master` `dff5cfa` | `20261019-205329-together-p2`, `-205555-together-p2`, `-205901-together-p2` | `stageP1`, `stageP3a`, `stageL` passed. A full regression of `master` on pair 2 (41 suites) had started; result unknown here |

All runs listed above ended with the owner's real saves unchanged.

## Results, 2026-10-20

From the agents' notes ([NOTES.md](NOTES.md)); this write-up re-ran none of them.

| Series | Runs | Result |
|---|---|---|
| P2 arrows, first version (pair 2, `ce6c5c7`) | `stageP2`, `20261020-083728` to `-091232-together-p2` | 1 of 3 passed. Every arrow the scan found was announced and copied (6 of 6 in the green run), copies 0 to 16 degrees off the original. Red runs: the bot's arrow stopped in its own body from the idle stance; one run's scan found no arrow (`spawned=0`). Negative control `shotmirroroff` failed as required. This commit predates the dead-mirror fix, so the client still showed standing dead orcs |
| Experiment 0, arrow touches | `20261020-122311-together-p2` | Diagnostic only. The host's arrows hit a chest and a house before the target; the client's arrows hit the shooter's own sword and spine and stuck there |
| Puppet turn fix (`a4da3aa`) | series `series-20261020-cont20`, DLL `b0ec5c94…` | `stage2` 19 of 20; `ZS_HangAround` on puppets 0 of 20 runs (before: at least once in 63 of 63 traced runs). The red run is a different, open case (puppet 1 to 2 m behind a client running under orc hits). Regression on that DLL 15 of 16; the red `stageP1` was a test error, 2 of 2 after the fix |
| Arrow hit counted once (`875d676`) | `stageP2hit`, target Hamlar | Every arrow that arrived was applied exactly once on the host; 2 of 4 runs passed (one run without a host arrow arriving in 8 tries, one aborted after the host hung), a later rerun passed: 3 of 5, not three in a row. Negative control `shotcopyhero` failed as required (double hit). No crossbow bolt reached the target |
| P2f fireball (`ba72045`) | `stageP2fire` | 3 of 3; host and client fireballs reached Hamlar and were applied once (client's as a claim). Negative control `shotcopyhero` failed as required |
| Summoned creature lifetime (`760af83`) | `stageCreatureLife`, `20261020-185021`, `-185346`, `-185644-together-p2` | 3 of 3 passed and the negative control `20261020-185959-together-p2` failed, but static review found that all three accepted six living mirrored creatures frozen on the host after the client moved 50 m away: the suite rejects only `free`, not `mirrored`. The suite does not prove the creature's end |

## Results, 2026-10-17 and 2026-10-18 (summary)

- Stages 1 to 5c, 3c, P: each 3 of 3 plus a failing negative control, on 2026-10-17.
- Stages A, A2, B, C, D1 to D3b, G, G2, H, H3, E1 to E5, Q, V1 to V4: each 3 of 3 plus a
  failing negative control, on 2026-10-18, with a full regression after each block (25,
  then 32 suites, all green at the end).
- Soak: 22 min with 320 seeded random actions, then 55 min with 738 actions (75 loads,
  59 saves, 55 falls with help, 53 partings): no failed action, no crash, clean
  shutdowns. Host working set levelled off at about 1.25 GB, client about 1.0 GB. This
  ran on an older build (before stages V and later).
- First human look on 2026-10-18 (owner, two windows on one PC): enemies ignored the
  remote player and the client saw no fights. Both became stages V1 and V2 and were
  fixed and bot-tested; a second look found T-posing orcs, fixed by replaying attacks as
  the game's combat moves.

## Not covered by any suite

Real internet play, more than two players, a full story playthrough, the real trade
screen and bed dialog, real key presses for gameplay (bots set the same tasks and states
a key press sets), monsters hitting a downed hero, fights and saves while transformed,
the end of a summoned creature when the players are far apart, exactly-once for
crossbow bolts.
