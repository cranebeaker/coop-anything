# What is still not in sync (measured 2026-10-20, night)

Question: after all the single-feature syncs, what differs between host and client in a running level?
Answered with a census instead of reasoning. Nothing was fixed in this pass; this is the list and an order.

Tested build: Steam buildid **23854500**, Unity 6000.4.7f1 (see [01-core.md](01-core.md)). Roles in every run:
host Driller / Interrogator, client Engineer / Maintenance, bots on, god mode on.

## Method

- **Census (`-Test worlddiff`).** After the load barrier releases, both instances log one line per
  `(category, object name)` at +30 s, +60 s, +120 s, then every 60 s, 20 s after every new stage and when the
  end screen appears: count and the first instance's position rounded to 2 m. Categories: `type:*` (enemies by
  `id`, pickups, activation zones, level pickups, cargo/drop pods, loot crates, projectiles, grenades,
  spawners) and `obj` (every active non-UI GameObject by name, `(Clone)` and numbers stripped; this is the net
  for classes nobody thought of). Time is **game time** (scaled), so a level-up pause stops the clock on that
  side. Read-only: nothing is created, moved or sent. Cost: about 0.3 s per sample per instance (a visible
  hitch, logged as `wd-end ms=`), 3-9 samples per run.
- **Report (`worlddiff-report.ps1`).** Per sample label both logs are put side by side. A label where the
  two roles were in **different stages** is reported as *not comparable* (run `20261020-222849`, `t120`: host
  already in stage 2, client in stage 1, which showed 2268 vs 0 terrain blocks). A difference smaller than
  max(2, 10 % of the larger count) is counted as noise. A mismatch counts as **persistent** only when it keeps
  the same sign in at least 3 samples. Exit code 1 means at least one persistent mismatch.
- **Ignore list** (three rules, each reports how many rows it hid): pooled audio emitters
  (`AudioSourceObject`), unnamed temporaries (`New Game Object`), transient effects (`VFX_*`,
  `BasicDeathFX*`). Everything else stays in the report.
- **Matrix runs** reach the second stage through the existing `-StageEnd win -StageEndDelay 90` aid, except the
  natural run. Selection by the host through the real lobby buttons (`mission_`, `sector_`, `node_`,
  `hazard_`); a locked button is logged as `NOT CLICKABLE` instead of being skipped.

## Negative control (the census can fail)

| Run | What was changed | Result |
|---|---|---|
| `20261020-223657-worlddiff` | none (baseline, 140 s) | no `GRUNT` row |
| `20261020-223954-worlddiff` | client `-coopworlddiffhide=Grunt` (leaves the name out of the count) | `GRUNT` host 93.3 vs client 0, persistent in 3 of 3 samples |

Two switches that look like controls and are not: `-NoMirror` (`20261020-222217`) only disables combat
visuals, and `-coopboscooff` on the client (`20261020-223311`) changes nothing the census can see (the Bosco
mirror is not an additional named object). Neither was used as a control.

## Matrix

Every run: both roles, bots, RESULT `worlddiff PASS` means "the measurement completed" (samples taken, no
exception), not "in sync". Duration = game seconds after the release, about 4 minutes of wall time per run
including load.

| # | Mission | Biome | Sector/Node | Hazard | Run | Duration | Result |
|---|---|---|---|---|---|---|---|
| m1 | Elimination | Crystalline Cavern | S1 / N1 | H1 | `20261020-222849` | 200 s, stage 2 sampled | PASS (`t120` not comparable). First attempt `20261020-221809`: no RESULT, see G9 |
| m3 | Elimination | Salt Pits | S1 / N4 | H3 | `20261020-225527` | 200 s, stage 2 sampled | PASS |
| m4 | Escort | Crystalline Cavern | S1 / N1 | H1 | `20261020-225926` | 200 s, stage 2 sampled | PASS |
| m5 | Escort | Crystalline Cavern | S1 / N1 (S2 and N3 not clickable) | H3 | `20261020-230326` | 200 s, stage 2 sampled | PASS |
| m6 | Escort | Crystalline Cavern | S1 / N1 (N4 not clickable) | H1 | `20261020-231931` | 200 s, stage 2 sampled | PASS |
| m2 | Elimination | - | **S2** / N1 | H1 | `20261020-225053`, repeat `20261020-231532` | - | **not startable**, both times (G2) |
| m7 | Elimination, natural | Crystalline Cavern | S1 / N1 | H1 | `20261020-232755` | ended at 440 s (host) | PASS, ended by `MISSED_DROPPOD` |
| m7b | Elimination, natural | Crystalline Cavern | S1 / N1 | H1 | `20261020-233928` | ~10 min | no RESULT: stopped by the launcher; 4 host and 1 client level-up timeouts (G1) |

Coverage that could **not** be reached, with the reason: Escort in Salt Pits (Escort nodes beyond N1 are
locked on this profile), Escort in sector 2 (locked), Elimination in sector 2 (selectable but not startable,
G2), a natural run to a victory or a boss (the bots never use the drop pod, so the run ended by timeout).

## Open points of the four tasks, each with its reason

| Point asked for | State | Reason it is open | Where it would be closed |
|---|---|---|---|
| Escort in a second biome | not measured | Escort nodes beyond N1 and Escort sector 2 are **locked on this profile** (`select ... node=3(NOT CLICKABLE)` in `20261020-231931`, `sector=1(NOT CLICKABLE) node=2(NOT CLICKABLE)` in `20261020-230326`). Unlocking them would mean changing the player's save, which is not allowed here | a test profile with more of the road unlocked |
| Elimination in a second sector | not startable | Sector 2 is clickable but START hangs on the hazard page (`20261020-225053`, repeat `20261020-231532`); recorded as gap G2, cause unknown | fix G2, then rerun `-Wd 0,1,0,0` |
| Two biomes and two hazard levels | covered | Biomes: Crystalline Cavern and Salt Pits (Elimination, `225527`); hazards H1 and H3 (`222849`, `225527`, `225926`, `230326`). Salt Pits only for Elimination, see above | - |
| Natural run to the mission end | partly | `20261020-232755` ran without `-StageEnd` to the end (440 s, `MISSED_DROPPOD`). The repeat `20261020-233928` has no RESULT: the launcher stops a run after ~10 min wall and this one lost ~3.7 min to level-up freezes (G1). Not a result of the tool: the run was in progress when it was stopped, saves restored | fix G1, then rerun |
| Boss and victory on both sides | not reached | The bots never board the drop pod, so a natural run ends by timeout (`MISSED_DROPPOD`), not by victory; no boss spawns before that. The census therefore cannot show them | a bot behaviour that plays the objective and the pod |
| Whether the client shows its own end screen after the host's run end | unproven | The host quit 0.6 s after its end screen, so the client's own end screen could not be told from the one the new peer-loss path forced (G8) | rerun with both roles alive (the tool now waits 35 s after the end) |
| Negative control | covered | one persistent mismatch is the intended outcome of hiding one name; the baseline `223657` has none for that name | - |
| `t120` of `20261020-222849` in the report | reported, not hidden | listed as *not comparable* (host stage 2, client stage 1) with the numbers, so it is neither compared nor silently dropped; it is a rule of the report, not an ignore-list entry | - |

## Gap list

Classes: **blocks the run** / **visibly wrong** / **cosmetic** / **expected**. Frequency = runs of the 5
comparable census runs (m1, m3-m6) unless stated.

### Observed (evidence in the logs)

| # | Gap | Class | Frequency | Evidence |
|---|---|---|---|---|
| G1 | **Level-up barrier freezes the run for 45 s.** The partner's "done" arrives before this side's own level-up starts, `ResetBarrier` (on `LevelUpManager.StartFlow`) then clears it, and this side waits the full timeout. | blocks the run | in the one run that was watched, 5 timeouts in 10 min (host 4, client 1); also 1 each in `20261020-220651` and `20261020-222849`; reported live in play-testing as a ~1 min hang | `20261020-233928` host: `23:34:39.012 xp: remote picks done (localDone=True holding=False)`, `23:34:39.901 xp: barrier reset (StartFlow)`, `23:34:41.934 local picks done, holding until remote picks`, `23:35:26.948 xp: hold timed out after 45s`. The client had already released at `23:34:41.956`. Code: `ResetBarrier` sets `_remoteDone = false` (`CoopXp.cs`, line 508 at the time of the run) |
| G2 | **Sector 2 cannot be started from the lobby.** The sector button is clickable, START runs the wizard, and the hazard page then has no hazard button for level 1. | blocks the run | 2 of 2 attempts | `20261020-225053`, `20261020-231532`: `lobby: start no button for hazard`, `page=HAZ`, step 394 and counting. Same flow works for sector 1 |
| G3 | **The client's turret is not shown on the host.** The turret mirror is host to client only (`CoopTurret.OnSpawn` returns unless host). The host receives the turret's fire events, so shots appear from nowhere. | visibly wrong | 5/5 (needs a client class with a turret, here Engineer) | `Turret_LMG` host 0 / client 2.4-4.7 and `Turret_BASE` host 0 / client 2.2-2.3 in every sample of every run; `Head` and `FirePoint` (turret parts) follow. Host log shows `skill=Sub_ProjectileWeaponSkill_LMG_Turret` fire events |
| G4 | **Active projectile objects grow without bound on the host** (and slowly on the client). | visibly wrong / performance | 5/5 | host `type:Projectile` at t30/t60/t120/t180: 85/362/506/702 (`222849`), 128/252/515/806 (`225527`), 45/133/342/515 (`225926`); client 16/28/30/52, 23/41/66/101, 7/7/9/13. Host `combat=... fireRecv=614 hookMirror=614` in `225926`: each received client shot creates a mirror projectile |
| G5 | **Twice as many loot bugs on the client.** | visibly wrong | 5/5 | `type:Enemy LOOTBUG` host/client means: 8/16 (`222849`), 5.8/11.6 (`225527`), 7.4/14.8 (`225926`), 6.2/12.4 and 6/12 in the other two; the ratio is 2.0 every time. `BlobProjector` and `Projector` follow (decals under them) |
| G6 | **Resource veins are not identical.** A whole vein type exists on one side only. | visibly wrong | 4 of 5 runs show a difference at +30 s | `MorkiteVein` host 16 / client 0 (`225926`); `EnorVein` host 1 / client 0 (`230326`); `NitraVein` 20 vs 16 (`231931`), `GoldVein` 26 vs 21 (`222849`) (the last two could be mining) |
| G7 | **Escort objects exist on both sides, identical** (Bobby, 4 detonators, TNT zone, fuel pickups, beacon pod). | expected (good) | 3 of 3 Escort runs | `20261020-225926`: `Bobby=1`, `TNTDetonator=4`, `BobbyTNTZone=1`, `MaterialBlock_BobbyFuel` 8/8, `PickupBobbyFuel` 1/1 at t30 on both |
| G8 | **A run end reaches the client only as a stage end.** After the host's `MISSED_DROPPOD` the client logs `stage: peer ended its stage (condition=2) -> raising it locally`, and no end screen within the next ~10 s; it appeared only through the new peer-loss path (ABORT) after the host had quit. | visibly wrong (unconfirmed) | 1 run | `20261020-232755` client log 23:27:30.6 (relay) vs `watch: active=CoreOutro` 23:27:48.6 after `peerloss ending the run` 23:27:46.8. The repeat meant to settle it (`233928`) was stopped by the launcher (G1) |
| G9 | **Sporadic total freeze.** Both instances' game time stopped for 7+ minutes after three chained level-ups, no barrier holding. | blocks the run | 1 of ~17 runs | `20261020-221809`: last sample `t120` at 22:10:23, no `combat fire` after 22:11:06, ticks continue with identical position and enemy count until the launcher gave up. Not reproduced in the repeat `222849`. Cause unknown; G1 is a candidate for the chained case but was not proven here |

### Derived (reasoned from the observations, not tested)

- G5: the client's loot bug spawner is probably not silenced like the normal enemy spawns, so it spawns its
  own bugs next to the mirrored host bugs (the ratio is exactly 2).
- G4: the mirror projectiles from `Fire` packets are not recycled when they hit; the host's own projectiles
  do not grow like this on the client side, which only receives 11 shots in the same time.
- G1: the fix is to keep a "done" that belongs to the current level-up instead of clearing both flags on
  `StartFlow`; the existing `levelup` test does not measure the hold duration and passes anyway.

### Not covered, and why (unknown)

- Escort dynamics: Bobby stayed at the same position on both sides for 180 s (`225926`, position 30,32) and the
  ESCORT objective stayed at 0, because the bots never escort. Movement, detonator phases and the TNT phase
  are unmeasured.
- Boss and victory: never reached (bots do not use the drop pod; the run ended by timeout at ~440 s).
- Terrain block counts differ by 10-15 % between roles in every run (`CC_Block_*`, `Top_*Damage`) and some
  pickups flip sign between samples (`LevelPickup_Apoca`, `LevelPickup_Boolo`). Mined terrain is mirrored with
  a delay and the roles mine different amounts, so this is probably timing; not shown to be a gap.
- Why the hazard button is missing in sector 2 (hazard level locked on that profile, or a different page).
- A partner that stays connected but stops answering (barriers).
- Pause / ESC and reconnect (out of scope for this pass).
- Whether the puppet shows the partner's class model: the census only shows the local dwarf's model parts on
  each side; the puppet itself is one object named `CoopPuppet(remote)`.

## Suggested order of fixing

1. **G1** first: it hits every run within minutes, costs 45 s each time and looks like a crash to a player.
   Small change, and add a hold-duration check (no hold longer than a few seconds) to the `levelup` test so
   the existing PASS can no longer hide it.
2. **G2**: a lobby that offers a sector the game cannot start is a dead end. At minimum disable the button.
3. **G4**: recycle mirror projectiles; it grows for as long as a run lasts.
4. **G5**: silence the client's loot bug spawner (visible and shifts loot).
5. **G3**: send the client's turret to the host the way the host's goes to the client.
6. Measure before fixing: **G8** (a run end that never shows an end screen on the client), **G6** and
   **G9** (needs a reproduction), and give the bots an Escort behaviour to measure the Escort dynamics.

## Known log lines that are not new

`profile: read 'animator' failed` (host, also in earlier runs) and `loot: wallet credit failed` (client).

## Update 2026-10-21 (overnight fixes)

| Gap | State |
|---|---|
| G1 level-up / shop barrier freezes (45 s) | **Fixed for the level-up barrier** by counting picks instead of a flag that `StartFlow` cleared (`20261021-043130-barrier`, twin `20261021-042819-barrier` holdMax 45 s). The shop barrier was not changed (same code shape, not reproduced). Only 150 s of bots measured, not 10 minutes (see STATUS for the 600 s run if present). |
| Map seed race | Fixed (client waits for the host's config). |
| Host unlocks not adopted | Fixed (default on; host re-sends in the menu; lobby applies; buttons redrawn). |
| CO-OP button / second run | Fixed (`CoopLobby.ResetForNextRun`, button built whatever the role). |
| Downed dwarf could move / shoot | Fixed for the game's movement step and weapon activation. |
| Class/skin only host→client; host puppet without skin slot | Fixed both. |
| Enemy motion 2 Hz | 10 Hz (time-based). Bandwidth rises ~2.3× with ~30 enemies. |
| Camera | Back to the base game's own follow (the two-dwarf framing is off by default, `-coopcameraon` brings it back). |
| G2 sector 2 not startable, G3 client turret on the host, G4 projectile growth, G5 double loot, G6 veins | **Open**, not touched. |

### Mission-sync findings (2026-10-21, goal "mission sync"; details in [08-mission-sync.md](08-mission-sync.md))

| ID | Finding | Status |
|---|---|---|
| G10 | Bobby is simulated per instance; on the client he never moved (`20261021-110753-worlddiff`). | fixed: `CoopEscort`, green `20261021-123846-worlddiff` |
| G11 | The partner did not count for Bobby's range or fuel. | fixed (host lends the partner's position to `Bobby.Update`), green `20261021-130744-worlddiff` |
| G12 | Objective mirror wrote the field: completion events never ran on the client, ARM_TNT never existed there, index shift risk. | fixed (type byte + `AddProgress`), green `20261021-123846-worlddiff` |
| G13 | Bobby fuel is a per-player wallet value; fuel the client collects does not reach the host's Bobby. | open |
| G14 | Secondary objectives of stage 2+ differed (client used the previous stage's seed). | fixed (client waits for the fresh seed), green `20261021-123846-worlddiff`, red `20261021-124304-worlddiff` |
| G9 note | The "sporadic total freeze after chained level-ups" is very likely the test bot answering a chained level-up form only once (reproduced and fixed in `CoopAuto`, see STATUS). It was not seen again in ~20 runs after the fix. | probable cause, not proven for the original run |

### New open point (2026-10-21, 10 min barrier run `20261021-062813-barrier`)

**XP totals drift apart in a long run:** at the same moment the client showed `total=2491.5` and the host `total=2453.0` (level 19 vs 18). The shared-XP mirror (`XpGain`, orb claims) loses or double-counts about 38 XP in 10 minutes of bot play (client side was also running with a 3 s artificial XP lag). Effect: one side levels up first and waits (18.1 s once) for the other to catch up. Not investigated; a census of `totalXp` on both sides every 30 s would show where it diverges. Also: map seed `-1192758820` is a fixture where the bots never kill an enemy (`20261021-061635-barrier`).

