# Deep Rock Galactic: Survivor (2024)

| | |
|---|---|
| Engine | Unity 6 (IL2CPP, metadata v39) |
| Versions tested | Steam build `23854500` (Windows) |
| Projects | [drg-survivor-local-coop](https://github.com/Reudo/drg-survivor-local-coop) (local co-op, playable, MIT); [drg-survivor-coop](https://github.com/Reudo/drg-survivor-coop) (online prototype, archived) |
| QA level | local co-op plugin: automated on one PC (results below) plus the author's own play by hand; online prototype: automated on one PC only, not human-confirmed (first two-PC test contradicted parts, see [STATUS.md](STATUS.md)) |
| Last updated | 2026-10-25 |

> **Current Ist/Soll:** [STATUS.md](STATUS.md) — verified state, known defects, design
> requirements and the gap table. Detailed working notes: [NOTES.md](NOTES.md) and
> [notes/](notes/).

## Local co-op plugin (playable, version 1.0.0)

[drg-survivor-local-coop](https://github.com/Reudo/drg-survivor-local-coop) puts two dwarves into one game on one PC, each on its own controller (BepInEx 6 IL2CPP plugin with
Harmony patches; the second dwarf is a second player object in the same process, so there is no network code and no desync). Elimination and Escort missions: own co-op menu, two HUD
panels, level-ups and shop one player after the other, pause menu per player, revive, shared XP, the drop pod needs both dwarves. Online with a friend works through Steam
Remote Play Together. A release ZIP with an installer is on the repo's releases page; screenshots are in its README.

- **QA:** a scripted harness (`tools/batch_*.ps1` in the repo) starts a test copy of the game with virtual gamepads and judges log lines and screenshots; every check has a twin flag that must turn it red. Results, 2026-10-21 to 2026-10-23 (run ids in the repo's `PLAN.md`): main menu 11/11, real mouse clicks 5/5, real start from the menu (class and mission read back from the live second dwarf), menu locks 10/10 (twin 6 ok, 4 fail), bonus mirror 5/5 (twin 0/5), pause menu per player 5/5 (twins red), retry run 7/7, revive and loot-crate tests green (twins red). The author has also played several full runs by hand with two controllers: no game-breaking bugs, at most small visual ones such as a wrong UI display. No independent report yet; one PC only.
- **Known gaps:** end-screen damage numbers are probably inaccurate; DLCs are not supported and the game was not tested with DLCs active; other mission types are not offered. The repo README lists the rest.
- This is the path that works today. The online prototype below was stopped and archived; its notes stay as research.

## Where things stand

The game itself has **no co-op**. The rest of this page records the earlier from-zero **online** prototype, written when there was no published co-op mod at all. The game is a
single-player survivors-like: one dwarf, one cave, and no networking code of any kind in the shipped
assemblies. Playing together therefore means building co-op from scratch as a mod — there is no
"finish the existing mod" path, which is why this page records a from-zero effort.

The prototype ([Reudo/drg-survivor-coop](https://github.com/Reudo/drg-survivor-coop), two instances on one PC) **aims to cover** the whole run loop. **Caveat:**
the list below is what the one-PC automated runs measured; the first real two-PC test (2026-10-21)
showed that some of it does not hold yet (different maps through a seed race, the client's dwarf
shown wrongly on the host, a downed player able to move, no host unlocks by default; see
[STATUS.md](STATUS.md), section "Contradicted by the first real two-PC test"). Measured in the
harness: same cave, both dwarves visible and animated, host-authoritative enemies,
shared loot and mined terrain, shared XP with a per-player card choice, mirrored objectives, a shop
after each stage that waits for the partner, stage ends that actually reach the other instance, a
downed/revive state, and a client that plays the run on the **host's unlocks** while keeping the
progress it earns. Every claim below was measured on two real game clients on one PC through a scripted harness,
not by a human on two PCs.

Both dwarves now also **wait for each other at the start of a run**: each side reports its level as
loaded, the faster one stays paused, and the host releases both together. The camera **frames both
dwarves**, centring between them and dollying out as they separate.

The start of a co-op run is now a real **lobby**: after the host/join panel both players land on an
in-game screen, the host sets mission, sector/node and hazard live, the client sees every change and
picks only its class and class mod, and the host can start once the client is ready. The faster
player also stays on the **loading screen** ("Fertig geladen - warte auf Spieler 2") until the
partner has loaded.

**Caveat on the lobby's look (2026-10-20):** the screen is built from cloned game UI, and on this
build its panel/background/button **images are not drawn** — the mission art shows through and the
text is faint. The flow, buttons and ready gate work, and `-LobbyIMGUI` draws a readable fallback.
The *logic* is verified; the *look* is not. See [STATUS.md](STATUS.md).

What is still missing is the last mile to a player: **no Steam invite or relay** (the in-game
host/join panel plus the lobby handle direct IP/LAN), and enemies that only ever chase the host's
dwarf. Enemy HP bars follow the host; the client shows **only its own damage numbers**, on purpose.
Muzzle flashes are mirrored for weapons whose game data enables them. The partner's pose **and
walk-cycle phase** are synced, the enemies' VAT animation state/phase is mirrored, and the Bosco and
the Thumper turret (including its firing flash) are mirrored as visual entities. The game has no
separate held-weapon model, so a weapon prop is not applicable; the class-mod skin is the character
visual.

## Existing work

Before this project, nothing was published. Searches across GitHub, Nexus, ModDB and Steam found no co-op or multiplayer
project, and a full search of the game's own assemblies found no transport, socket or Steam networking
layer. The gap entry in `data/directory.yaml` records what was searched and when.

The prototype is published as source at [Reudo/drg-survivor-coop](https://github.com/Reudo/drg-survivor-coop): no release, no installer, no DLL. It is written up so the
next agent does not start at zero.

## How the game works (for co-op)

Read by decompiling the shipped IL2CPP assemblies. Note **Il2CppDumper cannot read this game**
(metadata v39); Cpp2IL, which ships with BepInEx, can. Modding is BepInEx 6 (IL2CPP) plus Harmony, and
any transport you bring.

- **No netcode at all.** Nothing in the game is authoritative over anything, so every sync
  decision is ours. The prototype is host-authoritative and 2-player.
- **Level generation has no run seed for normal runs.** Nothing calls `Random.InitState`; only
  `FixedRunInstance` (daily/weekly) carries a seed. `MapGenerator.CreateMap` builds the cave from
  noise settings. This is why two instances generate *different* caves by default — and it is
  fixable: seeding `UnityEngine.Random` to a shared value immediately before `CreateMap` makes the
  two caves identical (proven, see below).
- **`Entity : MonoBehaviour` is the natural sync anchor** — both `Player` and `Enemy` derive from
  it, carrying `currentHp`, `position`, `isAlive`, `TakeDamage`, `Heal`, `Die`.
- **XP has a single entry point:** `Player.GainXp(float amount, bool scaleByXpGain = true)`.
  `CoreGameEvents.OnPlayerXpGain` / `TriggerPlayerXpGain(PlayerXpGainArgs)` is the matching global
  event if an observer is preferred over patching.
- **The level-up flow** lives in `LevelUpManager`: a queue (`levelUps`) drained by
  `StartFlow` → `HandleLevelUps` → `OnLevelUpFinished`, with a `flowActive` flag and a static
  `OnFlowActivity` event. A card is *previewed* by `UILevelUpForm.OnSkillSelected` and only
  *committed* by `OnSkillChosen` — pressing a button is not choosing a card.
- **Enemies** are owned by `EnemySpawner` (`enemies` registry, `enemyPrefabs`, `poolDictionary`,
  `liveElites`, `liveBosses`) and come from an `ObjectPool`, never `Instantiate`. `Enemy` already
  has an `id` field and exposes `OnDeath`, `OnTookDamage`, `OnHealed`, `OnKnockback`. An enemy holds
  exactly **one** `Player`, fixed in `OnCreated(..., Player player, ...)` through a getter-only
  property — which is why aggro cannot simply be retargeted.
- **Stage ends and shops** hang off `CoreGameEvents.TriggerStageEnd(CoreLevelEndArgs)`; the shop is
  `UIShopScreen.Show`, its continue button is `OnPlayButton`, and a shop closes through the next
  `GameController.StartGame` — `UIShopScreen.Hide` is never called when it closes.
- **The game is Zenject-wired** (dependency injection), uses `FSG.MeshAnimator` for meshes, and
  ships a KDTree (`PhysicsSimulation`) that could serve interest management for the swarm.
- **Saves are per user, not per instance.** All instances on one machine share the same profile
  folder and it is Steam Cloud-synced, so two local instances write the same saves unless something
  redirects them. The game keeps three numbered slots (`drg_save_slot{N}_{rotation}.dat`).
- **The game hard-requires Steam to be running.** Without it, it hangs on a black screen with
  `SteamAPI_Init() failed` and never reaches a menu — worth knowing before blaming a mod.

- **The partner's shots are visible by default.** The mirror used to be behind the B3 experiment
  flag `-cooptrace`; in a normal session the Fire packets arrived but the peer spawned no visual.
  It now spawns the game's real projectile prefab by default (`-coopmirroroff` disables it).
  Verified without `-Mirror`: host mirrors the client's Bosco, client mirrors the host's Subata,
  `applied=N dropped=0`.

## What we changed

The prototype is a BepInEx IL2CPP plugin using Harmony for hooks and LiteNetLib (the
transport Nitrox uses) over UDP. Roles are selected in an in-game host/join panel; the automated
harness can still pin them with command-line flags. A claim counts only when **both** sides produced
evidence; a timeout is not evidence.

- **In-game host/join (LAN/direct IP).** An IMGUI panel offers address/port, **Host** and **Join**
  (F8 hides it) and calls the same `CoopNet.Init` the role flags use, so every module sees the role
  before the run. *Test:* both instances launched **without** role flags; the harness pressed Host
  and Join; both sides reported `RESULT: ui PASS role=host/client peer=1`, and an end-to-end arena
  run then passed with `peer=1` on both. Steam invite/relay is not included.
- **Identical caves.** Prefix on `MapGenerator.CreateMap` that seeds `UnityEngine.Random` to a
  shared value on both instances. *Test:* sampled 81 rock positions through the game's own
  `MapGenerator.IsRockBlock` and compared the two 81-bit fingerprints — **identical, 0/81 samples
  differ**, and confirmed visually (same terrain in both windows). Before the fix the same
  comparison differed.
- **The other dwarf is visible, moves, turns and plays its state.** Each instance clones the local
  player's visual subtree, rebuilds it at run start, positions it from the network and drives its
  animator from a synced state. *Root causes of the early failures:* the clone was taken from the
  **menu** player (4 renderers, against 9 in-run), and speed derived per frame from a position that
  only changes ~20 times a second came out ~3x too high, so the animator never returned to idle. The
  player tree has **no `SkinnedMeshRenderer`** at all, so the pose is transform animation — the
  animator state is the difference between alive and downed, which is why an `AnimState` hash is
  synced instead of deriving a pose from hp.
- **The other dwarf's walk cycle is phase-aligned.** The state hash alone left the puppet's loop at
  its own clip time. `AnimPhase` (type 28, unreliable, ~10 Hz) carries `normalizedTime` plus the
  hash; the puppet plays the state at that phase and only corrects when the loop error exceeds 0.1,
  so it does not reset the pose every packet. *Test:* `-Test animphase` PASS on both roles,
  `good=15/15 moved=1` (host `lastErr=0.08`, client `0.00`); `-AnimPhaseOff` fails on
  `phase=off`, not on a timeout.
- **Enemy VAT animations are state- and phase-synced.** The swarm animates through
  `EnemyAnimator`/`ShaderMeshAnimator`, not a standard `Animator`; the snapshot now carries the
  host's clip index and normalized phase, and the mirror plays the same clip at the same phase.
  *Test:* `-Test enemyanim` PASS both roles (`sent=196`; client `seen=42 applied=7 sampled=21
  match=21`); with `-EnemyAnimOff` the client fails on `seen=0 enemyanim=off`.
- **Host-authoritative enemies.** Spawn, death and position snapshots travel host → client; the
  client builds and drives real `Enemy` mirrors from its own pool. *Test:*
  `check: enemy transport host=… mirrors=… spawn=… death=… fail=0` and
  `check: snapshot applied=… missed=…`, both sides.
- **Client damage reaches the host.** The client runs its weapons locally and reports the hp delta
  its own `TakeDamage` produced; the host applies it to the enemy it owns. *Test:*
  `sent=20 dmgTotal=532` equals `applied=20 missed=0 unmatched=0 dmgTotal=532`.
- **The client's dwarf takes damage** from the mirrored swarm, because a mirror is a real `Enemy`
  created by the client's own spawner and `OnCreated` hands it the client's player. No packet was
  needed: each side is authoritative for its own dwarf's hp. *Test:* `-NoGod`, 8 × 10 hp lost on both
  sides in the same window, every hit with `nearestMirror=0.7–1.2`.
- **Loot is shared and the wallets stay identical.** Every pickup is broadcast; the receiver removes
  its own copy (host: the original, client: the mirror) and credits **every** currency. *Test:*
  identical `walletDelta={GOLD=+3 NITRA=+6}` on both sides, mirrored claim counts (7/2 and 2/7),
  `miss=0`, `credited == claimsRecv`. Three defects had to go: a host-only claim broadcast, a claim
  queue drained only on the client, and crediting that skipped persistent currencies.
- **Mined terrain matches.** `TerrainDestroy` mirrors `MaterialBlock` ore veins into the other
  instance's block list. *Test:* `check: terrain recv=3 mirrored=3 miss=0`.
- **Shared XP, individual cards.** XP gains are mirrored and applied with `scaleByXpGain: false`; the
  level-up flow is held until **both** players have picked, with a timeout so a lost peer cannot
  soft-lock the run. *Test:* identical `lvl/xp/total` on both sides, barrier held ~4.1 s, and the two
  sides took **different** cards (`SupportSkill_*` vs `WeaponUpgradeSkill_FireRate`).
- **Objectives mirror.** The host reads `LevelObjectiveTracker.PrimaryObjectives` /
  `SecondaryObjectives` at 1 Hz and broadcasts `(index, progress)`. Secondaries are random, so
  `UnityEngine.Random` is seeded before `SetupSecondaryObjectives` (the same trick as the cave).
  *Test:* `check: objective recv=26 applied=2 miss=0`, same objective types on both sides.
- **The client adopts the host's run.** The host sends hazard, seed, biome, sector, node, mission and
  main mission; the client confirms what it holds, and its wizard **waits** for that config before
  the class page. A 1 Hz heartbeat is the fallback that actually delivered. *Test:*
  `held=10.1s helloTx=7` → host `peer-ready=1 echo=1`; with the config withheld on purpose,
  `held=20.3s`. With the client forced onto a different node (`-coopdrift`), both sides still read the
  host's `biome=Crystalline Cavern(0) … node=0`.
- **Shops after a stage — one per player, currency shared.** A purchase is local and personal (the
  two instances bought different items); the wallet is not. `UIShopScreen.OnPlayButton` is held until
  the peer reports done, and released by re-invoking that same method. *Test:* `heldFor=15,6s
  releases=1 timedOut=0` with the peer's own message as evidence; the negative control (barrier off)
  fails with `remoteDone=0`.
- **Stage ends reach the partner.** Mirroring objectives is not enough — measured, the host reached
  stage 2 while the client stayed in stage 1 for the full 300 s. The fix is a packet raised through
  `CoreGameEvents.TriggerStageEnd` with `ApplyingRemote` set so it never echoes. *Test:* the client's
  shop opened **17 ms** after the host's stage end (was ~14 s), the barrier's hold fell from the 45 s
  timeout to 0.5 s, and both sides reached `starts=2`; two transitions give `starts=3` with
  `endsRemote=2`. Negative control (`-StageEndNoRelay`) is red on both sides.
- **Downed, revive and the corpse.** `Player.Die` still runs, so the animator enters `Dying`; after a
  revive, writing `currentHp` back never touched the pose because `isDead` is an **Animator Trigger**,
  not a Bool. `Animator.Rebind()` + `Update(0f)` resets it (with `SetIdle(false)` as fallback), and the
  remote's view is driven by the synced animator state rather than inferred from hp. *Test:*
  `anim != Dying#-1546996312` after the revive and still not 1 s later, twice; the negative control
  fails with `alive-but-dying`.
- **Grenades are mirrored, including to a partner without that grenade.** A throw travels as skill
  name + origin + direction; the host also sends the grenade skills it actually carries, and the
  receiver equips them through the game's own `Player.EquipNewWeapon` **before the run plays**, so its
  pool exists and the replayed grenade explodes. *Test:* client
  `adopt … via=EquipNewWeapon weapons=1->2 registered=1` → mirror `via=pool` →
  `RESULT PASS spawned=1 exploded=1`; the receiver's own throw no longer counts as proof of the
  received mirror. Known side effect: the receiver can now throw that grenade too.
- **Both sides finish loading before either run plays.** `CoopLoad` pauses the local run at
  `StartGame` through the game's own `GameController.PauseCore()`, exchanges `RunLoaded` messages, and
  the host releases both with `UnpauseCore()` once both are ready (30 s timeout for a lost peer).
  *Test:* with the client's readiness delayed by 8 s on purpose, host `held=23.5s` and client
  `held=8.0s`, both `paused=1 peer=1`; during the hold the host's position and enemy count were frozen
  while its bot kept ticking.
- **The camera frames both dwarves.** The game camera follows one player; a focus point at the
  midpoint of the two dwarves is assigned as the virtual camera's follow target, and the transposer's
  own follow offset is scaled with their separation (up to 1.6×), so the view dollies out instead of
  leaving the partner off-screen. Original follow/offset are restored outside co-op. *Test:* at
  20–23 units apart, `camera PASS farVisible=19–20/20` on both sides; with
  `-coopcameraoff` (vanilla follow) `5/20` and `1/20` → **FAIL**. The off-screen marker remains the
  fallback when a player runs beyond the zoom.
- **Enemy HP follows the host; damage numbers stay personal.** The `EnemySnapshot` now carries each
  enemy's host hp, and the client applies it **damage-only** (`hp = min(local, host)`), so a host hit
  lowers the mirror's bar and a stale snapshot can never heal it. The client's own weapons already
  damage the mirror locally, so its numbers appear immediately; by the design decision it does not
  display the host's numbers, and no host→client number event exists. *Test:* client
  `snapSeen=11 equal=6 drops=1/82 above=4`; with `-coophpsoff` the client fails on state
  (`snapEntries=31 hpSeen=0`) instead of timing out.
- **Muzzle flashes are mirrored.** Every received shot calls the weapon's own
  `ProjectileWeaponSkillData.FlashSettings` through `ImpactLightsManager.AddLight` at the fire point,
  independent of whether a projectile is mirrored. A weapon whose data disables its flash stays dark
  (Bosco's base skill does); a missing settings asset falls back to a small warm default. *Test:*
  client `flashes=8 disabled=2 failures=0` (host's Subata), host `flashes=0 disabled=10` (client's
  Bosco, disabled by the game); `-MuzzleOff` → **FAIL** `fireRecv=10 flashes=0`.
- **The client plays on the host's profile and keeps what it earns.** The host's unlock lists, meta
  levels and gear inventory travel as the strings and structures the game itself stores; the client
  applies them **in memory, in the menu**, before the class page is built. Its own save writes are
  blocked while it plays on borrowed data, and the loan is given back at run end — an id is dropped
  only if the host lent it and the client did not have it, so earnings survive. *Test:*
  `client dwarf=00000000 → received {cls=4 mod=9 wpn=29 art=13 oc=0 sets=1} → classPageButtons=4`;
  meta `90 → 0` with `gold 10 → 0 goldMoved=1`; gear `0 → 6 collected=6`. Verified in the **save
  file**, not only in the log: the client's file holds none of the lent data and does hold its
  earnings (`classXp 5956 → 5958` for `perfXp=2`).
- **A run ends like solo.** When both dwarves are down, the game's own game-over runs; a lone survivor
  keeps playing and can revive. The policy lives in one patch, and the downed *state* decides — each
  side still saw a live partner 40 ms after both died, because hp arrives a round trip later.
- **The harness itself.** Verdicts are written by the mod (`RESULT: <test> PASS|FAIL reason=…`), so a
  run never waits a guessed number of seconds; every run backs up and **restores** the saves (verified
  by hashing plus a canary file), archives its logs, and stops the processes it started.

## QA

**How it is run.** Two instances of the game on one PC, bots driving both dwarves, and one log per
instance. Roles come from the in-game host/join panel (`-Ui`, pressed by the click helper) or from
command-line flags for the other tests. The mod decides when a question is answered,
writes a machine-readable verdict and can end the run itself, so an unanswered test reports
`FAIL reason=timeout` instead of producing nothing. Assertions compare the two logs: every claim needs
evidence from the peer, and no exception may appear.

**Last suite result (2026-10-20).** All of the following PASS on both roles in their most recent runs:

| Question | Evidence |
|---|---|
| Same cave | 0/81 fingerprint samples differ |
| Enemies | `spawn/death/… fail=0`, `snapshot applied=… missed=…` |
| Client damage | `sent=20 applied=20 dmgTotal=532` on both sides |
| Loot | identical wallet deltas, mirrored claim counts, `miss=0` |
| XP and cards | identical totals, different cards per side, barrier released |
| Objectives | `recv=26 applied=2 miss=0`, same types |
| Settings adoption | client forced to another node, both read the host's |
| Shop barrier | held until the peer's `ShopDone`, `timedOut=0` |
| Stage relay | `starts=3` over two transitions, `endsRemote=2` |
| Downed / revive | pose leaves `Dying` and stays out, twice |
| Profile loan | `classPageButtons=4`, save file checked before and after |
| Host/join UI | `RESULT: ui PASS role=host/client peer=1`; end-to-end `arena PASS` with the UI roles |
| Grenade mirror (receiver had no grenade) | `adopt … weapons=1->2 registered=1` → `via=pool` → `exploded=1` |
| Run-start load barrier | `-LoadDelay 8`: host `held=23.5s`, client `held=8.0s`, both `paused=1` |
| Camera frames both dwarves | `camera PASS farVisible=19–20/20`; `-coopcameraoff` `5/20` / `1/20` → FAIL |
| Enemy HP follows the host | client `snapSeen=11 equal=6 drops=1/82 above=4`; `-coophpsoff` → FAIL `hpSeen=0` |
| Muzzle flash mirrors | client `flashes=8 disabled=2`; host `disabled=10` (Bosco); `-MuzzleOff` → FAIL |
| PGL fires and mirrors | `weapons PASS expected=1 received=1 notFired=0`; Fire events both ways (`20261019-233339-weapons`, 15 s window rerun `20261019-235051-weapons`) |
| TH0R lightning beam mirrors | `beam=3/2` host, `3/3` client, `skills=1/1`; PASS both (`20261019-234302-weapons`); `-BeamOff` → FAIL `beam=0/0 notFired=1` (`20261019-234645-weapons`) |
| Continuous rotating beam mirrors | host `beam=98/91`, client `100/98`, `skills=1/1`; PASS both (`20261020-012001-weapons`, `RotatingBeam_CryoCannon`) |
| Ground-zone grenade mirrors | host `sent=1`; client `sent=4 spawned=6 exploded=1 adopted=1`; PASS both (`20261020-013904-grenade-mirror`, `Groundzone_Acid`); the zone is created by the mirrored explosion |
| Enemy mirrors move smoothly and turn | client `maxStep=0.500 facingApplied=364`; PASS both (`20261020-005713-enemysmooth`, final build); `-SnapSmoothOff` → FAIL `maxStep=2.002`, `-EnemyFaceOff` → FAIL `facingApplied=0` |
| XP orbs visible on both sides, one grant | host `spawnSent=16 claimSent=15 finishCalls=8`; client `spawnRecv=16 applied=16 suppressed=8`; PASS both (`20261020-005304-xpsync`); `-XpOff` → FAIL `spawnSent=0 spawnRecv=0` (`20261020-005512-xpsync`) |
| Revive circle fills | `prefab=Projector hasFillFactor=1`, 329/332 updates; PASS both (`20261020-011703-revivecircle`); `-ReviveVisualOff` → FAIL `created=0` (`20261020-011516-revivecircle`) |
| Revive amount scales | 50% of max by default: host `maxHp=145 → hp=72`, client `130 → 65`; `-coopreviveflat` keeps the old flat 100 |
| Trail ground zones | host `sent=34`, client `recv=38 applied=38`, PASS both (`20261020-015745-trail`); `-ZoneOff` → FAIL `sent=0 applied=0` (`20261020-015907-trail`) |
| Ground-zone grenade | `GrenadeWeaponSkillData_Groundzone_Acid`: client `spawned=6 exploded=1 adopted=1`, PASS (`20261020-013734-grenade-mirror`) |
| Turret visual mirror | host `sent=1 stateSent=56`; client `spawned=1 states=42 prefab='Turret_THUMPER'`, PASS both (`20261020-022442-turret`); `-TurretOff` → FAIL `sent=0 spawned=0` (`20261020-022622-turret`) |
| Turret firing VFX | host `sent=5`, client `recv=5 applied=5 flashes=5`; PASS both (`20261020-131050-turretfire`); `-TurretFireOff` → FAIL `sent=0 recv=0` (`20261020-131227-turretfire`) |
| Remote weapon prop | Not applicable: the game has no separate held-weapon model (`WeaponHandlers` is the shared empty container, 0 renderers; no `WeaponSkinData`/`WeaponModel` type). The class-mod skin is the character visual; the `weaponvisual` fail is expected and retired |
| Co-op lobby state / ready gate | host rev=2 `blockedPeer=1 blockedStale=1 allowed=1`; client `state=2 choice=2 start=1`; PASS both (`20261020-054247-lobby-state`); `-LobbyNoReadyGate` → FAIL (`20261020-054515-lobby-state`) |
| Co-op lobby screen (structure/clicks, **not pixels**) | host `sectors=5 nodes=6 classes=5 mods=3 hazards=5`; host changes hazard → client sees rev=2, picks Engineer/Maintenance, ready; host start allowed; PASS both (`20261020-063928-lobby-ui`, `20261020-172313-lobby-ui`); `-LobbyUIOff` → FAIL (`20261020-063812-lobby-ui`). **Visual: ❌ the cloned images do not draw — see [STATUS.md](STATUS.md)** |
| Lobby start through the wizard | host Driller/Interrogator, client Engineer/Maintenance, same `main=0 biome=0 sector=0 node=0 hazard=1`; `run=1`; PASS both (`20261020-064751-lobby-start`) |
| Loading screen waits | real `UILoadingForm` shown over the paused game with `Fertig geladen - warte auf Spieler 2`, hidden on release; host `shown=1 paused=1`, `held=1.4s`; PASS both (`20261020-123303-loadform`); `-LoadFormOff` → FAIL (`20261020-123406-loadform`) |
| Game beam VFX | `BeamWeaponSkillData.BeamVFX` = `CryoBeam_VFX`, one live prefab; host `sent=56`, client `applied=68`, `game=1/0/0`, `fallback=0`; PASS both (`20261020-133051-beamgame`); `-BeamOff` → FAIL both (`20261020-133207`), `-GameBeamOff` → host PASS/client FAIL `fallback=68` (`20261020-133320`) |
| Bosco mirror | host `sent=164`; client `recv=173 spawned=1 statesApplied=194 renderers=3 miss=0`; PASS both (`20261020-134658-bosco`); `-BoscoOff` → FAIL both (`20261020-134912-bosco`) |
| Host class skin | client `ActivateSkin ok dwarf=0 meshes=4`, `applied=1` with host class 0 / local 1; PASS both (`20261020-030131-skin`); `-SkinOff` → FAIL `applied=0 meshes=0` (`20261020-030403-skin`) |
| 10-minute soak | PASS both after 600 s post-release; 2900/2915 enemies, 618k snapshots (3671 missed), 928 projectiles; no exception/snapshot-size/disconnect matches; saves restored (`20261020-040552-soak`). Mirror spawn fix: `fail=0` at 3921 spawns |
| Mirror spawn parity | Before: `fail=29/50`; after idempotent `Register` in `SpawnMirror`: `fail=0 null=0 noEntry=0 noPtr=0`. Local mirror deaths blocked (`OnDiePrefix`, hp clamp): `20261020-043000-soak` client `localDie=0 miss=0` at 624 spawns / 580 deaths; `blocked local mirror death` logged |

**Caveats, stated plainly.** The harness **forces** stage ends and run ends
(`TriggerStageEnd`, `TriggerCoreRunOutro`) so a measurement takes ~90 s instead of many minutes, so a
stage that ends *by itself* has never been observed. Shop purchases and card picks are exercised by
fixtures that choose specific buttons, not by a human. The host/join panel test presses the IMGUI
buttons at fixed client coordinates; a human has not clicked it in this run. There is no one-command
suite: the launcher is local tooling, not part of a repo.

**Visual caveat (2026-10-20).** The lobby test above asserts structure and clicks, not pixels. On the
canonical menu path the cloned uGUI **images** do not draw (mission art shows through, text faint), so
the lobby's look is currently broken even though its logic is verified. Use the menu-button path
(`coop-run.ps1 -Menu -Test menu`) for UI QA; the `-cooplobby` shortcut is invalid for visuals because
it builds the screen before the menu entry and produces a canvas that is active but not drawn. The
current Ist/Soll and the evidence are in [STATUS.md](STATUS.md).

## How to play

Not released. There is no installer and no Steam invite/relay. Use a copy of the game (never the
Steam install), with Steam running and the same game build on both sides. Launch both instances;
press **Host** on one and **Join** on the other, with the host's address (127.0.0.1 for two instances
on one PC, the LAN IP otherwise; UDP port 9050 by default). F8 hides the panel. After joining, both
players land in the co-op lobby: the host sets mission type, sector/node and hazard (the client sees
the changes live), the client picks only its class and class mod, and the host starts once the client
is ready. Each side reports when its level is loaded; the faster one stays on the loading screen
("Fertig geladen - warte auf Spieler 2") until both are ready, then the run starts. The automated
harness keeps `-coophost` / `-coopclient` for the other tests and has a separate `-Ui` panel test.

## Known issues

Plain words, what a player would notice today:

- **The co-op lobby is semi-transparent and hard to read.** Its panel, background and button images
  are not drawn on this build, so the mission screen shows through; the text is faint. The buttons
  still work. `-LobbyIMGUI` draws a readable fallback. This is a rendering defect in the cloned UI,
  not missing logic.
- **You cannot invite a friend through Steam yet.** The in-game panel handles LAN/direct IP, but there
  is no Steam relay/invite flow; for a WAN connection the host's UDP port (9050 by default) must be
  reachable.
- **Your partner's bullets may not be visible.** They are spawned as real, pooled game projectiles
  with a real shader, but "it is actually drawn" has not been *proven*: every in-process way to check
  (visibility flags, render callbacks, reading a frame back) turned out to be unusable in this build.
  **Grenades are mirrored** (including to a partner without that grenade, who equips it at run start)
  and muzzle flashes follow the weapon's own `FlashSettings`. Enemy HP now follows the host; the
  client shows only its own damage numbers (design decision), so those numbers are immediate.
- **Enemies only ever chase the host's dwarf.** The client is hit when it walks into the swarm, but it
  does not pull aggro. Changing that needs a second real `Player` on the host, not a retarget.
- **The held weapon is not shown.** The class-mod skin is applied on the partner; the game has no
  separate held-weapon model, so weapons are represented by their projectiles, beams and turrets.
  This is the game's design, not a missing sync.
- **"Stand in the circle" objectives do not count your partner.** Their progress mirrors, but the
  host's game only ever sees one player standing there.
- **Mastery and the gear loadout are untested**, because the test profile has neither the Mastery mode
  unlocked nor anything equipped.
- **What the client borrowed does not stick.** During the run it plays on the host's unlocks with its
  own save writes blocked; at run end the loan is given back and the game writes the client's own
  profile plus the progress it earned. That is a design decision, not a bug — but it does mean the
  client and the host each keep their own earnings, while the evening's unlocks came from the host.
- **Run the mod from a copy of the game**, never the Steam install, and keep Steam running.

## Next steps

First, **UI stability**: fix (or replace) the cloned lobby images so the panel is opaque and
readable, then capture the connect screen and the lobby on the canonical menu path. The logical flow
and the tests already pass; the open work is the draw path. See [STATUS.md](STATUS.md).

In the order that makes sense:

1. **Remaining combat visuals:** only the projectile draw oracle, which is out-of-process (park a
   mirror in front of a camera and diff two window captures). The turret firing flash, the game
   `BeamVFX` for the continuous beam and the Bosco mirror are done; the held-weapon prop is not
   applicable (no separate model). The grenade loadout stays host-owned: the receiver equipping the
   adopted grenade is intended, so no distinct-loadout variant.
2. **Aggro for the second player** — ideally by giving the host a second real `Player`, or by first
   proving that an unfrozen mirror chooses the client on its own.
3. **The objective rules requested:** standing in a circle faster with both dwarves, a drill
   that needs both, one supply pod with a personal artifact choice, one Bosco per player (the Bosco
   mirror is done; the objective rule is not).
4. **Cosmetics:** the class-mod skin is verified; the explicit held-weapon prop is not applicable.
5. **Steam lobby and invite, last** (Spacewar AppID 480 is the usual route to a free relay). The
   in-game host/join panel for LAN/direct IP is done; the Steam piece needs explicit approval and a
   second Steam account/session, so it is the final step rather than the next one.

Two habits save time here: measure before designing (more than one "obvious" bug turned out to be the
fixture), and give every change a negative control that has to go red — otherwise a passing check
proves only that the check can pass.
