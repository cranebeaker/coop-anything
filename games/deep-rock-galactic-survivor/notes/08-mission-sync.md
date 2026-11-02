# Mission sync — exploration (goal "mission sync", step 1)

2026-10-21. Game: Deep Rock Galactic: Survivor, Steam build used for all runs in this repo (see NOTES.md for the identity).
Everything below was measured with `coop-run.ps1 -Ui -Lobby -Test worlddiff -Wd "1,0,0,1"` (Escort, sector 1, node 1,
hazard 2) and the observation-only sampler `CoopMission` (`-coopmissiondump`, `-coopmissionsec=N`). Names are the
game's own type and member names as seen through the generated interop; no game code is reproduced here.

## Escort mission: who does what

| Piece | Type / member | Measured behaviour |
|---|---|---|
| Bobby | `Bobby` (MonoBehaviour); `State` (`INTRO, ESCORT, MINING_HEART, BROKEN, OUTRO`), `animState`, `distMoved`, `totalDistance`, `fuel`, `playerActivationRange`, `moveSpeed`, `HandleEscort(bool inRange)`, `Move(dt)` | Start (30,32), end (160,32), `totalDistance=130`, `moveSpeed=4`, `playerActivationRange=6`. Stands still until a player is within 6 m **and** the wallet holds Bobby fuel. With fuel and a dwarf next to him he walks the 130 m in ~30 s (host `20261021-110753-worlddiff`: `distMoved` 41 -> 129, State `RUNNING` -> `BROKEN` at the end). |
| Fuel | `Wallet.BobbyFuel` (`ECurrency.BOBBY_FUEL = 22`), pickups `PickupBobbyFuel` from `MaterialBlockBobbyFuel` | A **per-player wallet** value. Bobby reads the local wallet, so fuel a partner collects does not reach the host's Bobby. |
| Escort objective | `LevelObjectiveEscort`, type `ESCORT` | Progress = % of the path (0..100), driven by Bobby's movement. |
| Follow-up objective | `LevelObjectiveArmTNT`, type `ARM_TNT` | **Added to the tracker's primary list when ESCORT completes**; `EscortMissionHandler.state` goes `ESCORT -> PREPARE_TNT`. Host primary list `[ESCORT=100, ARM_TNT=0..4]`. |
| Detonators | `TNTDetonator : MineableBlock` x4 (`numDetonatorsToSpawn`), `TakeDamage` / `Die` / `SetArmed(bool)` | Armed by damage. Host bot damaging all four gives `ARM_TNT=4` (`20261021-111555-worlddiff`). |
| TNT zone | `BobbyTNTZone`, at (160,0,32) | Exists on both sides from the start. |
| Handler | `EscortMissionHandler` (zenject-injected, **not a component**: `FindObjectsOfType` cannot find it) | Owns `state`, `tntProgress`, `detonators`, `dropPod`. Subscribes to the objective events, so it only runs the next phase if the objective completion event fires on that instance. |
| Drop pod | `DropPod`; `state` `INACTIVE, ANIMATING_IN, ARRIVING_COUNTDOWN, WAITING_FOR_PLAYER, WAITING_FOR_PLAYER_EXIT, ANIMATING_OUT`; `timeoutSeconds=30`, `playerPoint`, `rampDetector`, `IsPlayerOnRamp()`, `IsPlayerInside()`, `hasPlayerBeenOnRamp` | Lands ~2 s after the TNT phase, waits 30 s, then leaves (`MISSED_DROPPOD`). **Boarding needs the ramp first:** teleporting a dwarf straight to the pod's `playerPoint` is not enough; going to `rampDetector` first, then inside, ends the stage in ~7 s (`20261021-114845-worlddiff`). |
| Supply pod | `SupplyPod` (pooled, `SummonAt(pos)`), field `ActivationZone.supplyPod`, `UIManager.ShowSupplyPod` | Not triggered in any run so far; trigger source still to be measured (activation zones). |
| Boss | `EliminationMissionHandler.SpawnBoss` / `didSpawnBoss`, `EggHuntMissionHandler.SpawnBoss`, `EnemySpawner.liveBosses`, `MissionMapConfig.bossMissions` | Boss stages are an Elimination feature; no escort run spawned one. Not reached by any bot yet. |

## Measured gaps (client vs host)

| ID | Finding | Evidence |
|---|---|---|
| G10 | **Bobby is simulated per instance.** Host Bobby walks 30 -> 160; the client's Bobby stays at (30,32) with `distMoved=0 fuel=0 State=NONE` for the whole run. | `20261021-110753-worlddiff` (host `pos=159,4,32 State=BROKEN`, client `pos=30,0,32 State=NONE`) |
| G11 | **The partner never counts for Bobby's range.** A dwarf that is only the client cannot make the host's Bobby walk. | by construction (range test uses the host's own player); to be shown by a client-only escort run |
| G12 | **Objective mirror writes the field, so completion events never run on the client.** The client's ESCORT reads 100 but `ARM_TNT` is never added to its tracker, and the index-based mirror would write the host's `ARM_TNT` into the client's secondary slot. | same run: host `primary[ESCORT=100,ARM_TNT=0]`, client `primary[ESCORT=100]`; later host `ARM_TNT=4`, client has no such objective |
| G13 | Fuel is per wallet (see above); a client that picks up fuel gives the host's Bobby nothing. | wallet type; not yet shown by a run |

## Test-harness findings (not sync gaps)

- A chained level-up (two levels at once, e.g. the reward for finishing the escort) left `UILevelUpForm` open for minutes with
  `timeScale 0`: the automation answered each form once (`_levelUpAnswered`). The game and the barrier were fine. Fixed in
  `CoopAuto` (answers again if the form is still open after 4 s); the pre-fix freeze is `20261021-111555-worlddiff` /
  `20261021-114003-worlddiff` (`mission: frozen form=UILevelUpForm`), the post-fix run is `20261021-114352-worlddiff`.
- Bots added as test switches: `-coopescortbot=host|client|both` (dwarf next to Bobby), `-coopescortfuel=N` (host wallet
  shortcut for the fuel mining), `-coopescortdet` (host damages the detonators once Bobby is broken), `-coopdropbot=...` (ramp, then pod).
  With them an Escort stage ends **naturally** on the host: `20261021-114845-worlddiff` (pod `WAITING_FOR_PLAYER` -> `ANIMATING_OUT`,
  the client followed with `stage: peer ended its stage`). This is the evidence for goal step 2 on the host side.
- `worlddiff` was missing from the lobby-path list in `coop-run.ps1` (the run sat at the splash); added.

## Startup crash found and fixed (2026-10-21 afternoon)

Symptom: both game instances died 5 s after launch (`coreclr.dll`, access violation, always the same offset `+0x1d1fdd`; WER
`APPCRASH`, minidumps in `%LOCALAPPDATA%\CrashDumps`). It looked like an environment problem because the first cluster (11:51-11:54) and the
second (14:01-14:02) happened while an interactive desktop session was active, so it was measured:

| Check | Result |
|---|---|
| Game without BepInEx (`--doorstop-enabled false`), interactive session active | runs, no crash |
| BepInEx without the plugin, interactive session active | runs, no crash |
| BepInEx + plugin, interactive session active | crash at 5 s (also with no co-op flags) |
| plugin with every Harmony patch skipped (`-coopskip=all`) | runs |
| bisect over the patch list (`-coopskip=<names>`, 7 runs) | one patch alone crashes: `CoopDownedLockWeaponPatch` (`WeaponHandler.ActivateWeapon`) |
| BepInEx log before the crash | `During invoking native->managed trampoline ... NullReferenceException at DMD<WeaponHandler::ActivateWeapon>` (and later `DMD<TNTDetonator::Die>` for the escort hook) |
| hook installed late (first `StartGame`) | escort hooks fine; the weapon hook still crashed |

Cause: the game calls `WeaponHandler.ActivateWeapon` already while the title scene starts (it does so only when the window is really
rendered, which is why runs from a disconnected session passed). A Harmony hook on that call ended the process. The hook was added in the
overnight goal and had never run in an interactive session, so a normal player start would have crashed too. Fix: the hook is gone; the attack
lock now pauses the dwarf's weapon handlers (`WeaponHandler.paused`) while downed (`CoopDownedLock.PauseTick`), and the escort hooks
install at the first `StartGame` (`ProbePlugin.InstallLate`). Rule for future hooks: never hook a method the game calls during title-scene
startup; install late, keep the hook body in try/catch, and prefer state changes over hooks. Bisect aids kept in the plugin: `-coopskip=`,
`-coopbreadcrumbs`, `-coopguimask=`, `-coopnolate`.
