# Combat, progression, saves, and run mechanics

> Part of the Deep Rock Galactic: Survivor working notes. Extracted verbatim from `NOTES.md` on 2026-10-20 (lines 1195-3398); content unchanged.
> Current state and Ist/Soll: [../STATUS.md](../STATUS.md). Index: [../NOTES.md](../NOTES.md).

## Roadmap (current)

A master plan with the full remaining-work list and PASS criteria lives at
`local/mod/DrgSurvivorCoop/PLAN.md` (private). Order: A character visuals → B combat visuals →
C objectives → D shop/upgrade → E loot value fix → F downed/revive → G UI + Steam lobby → H give back.

- **Milestone A (character class sync) — VERIFIED.** New `CharClass` packet (12): host sends `EDwarf`
  as byte; client stores `HostDwarf`. Two-instance run: `check: char self=3 role=host` ==
  `check: char host=3 self=3 role=client`. **Skin visual VERIFIED (2026-10-20).** The earlier block was
  the **pre-run** puppet: it has no `PlayerSkin` component. After the run starts the puppet is respawned
  and does have one, so the typed re-wrap plus `ActivateSkin(playerSkin, skinData, false)` succeeds.
  Run `20261020-030131-skin` (`-SkinTest`, host class 0 / local 1): client `ActivateSkin ok dwarf=0
  meshes=4`, `check: skin ... applied=1 meshes=4`, PASS both. Negative `-SkinOff`
  (`20261020-030403-skin`): client FAIL `applied=0 meshes=0`. The blank-`AddComponent` path is not
  used. Full detail in `PLAN.md`.
- **Milestone C (objective sync) — VERIFIED.** `Objective` packet (13): host reads
  `LevelObjectiveTracker.Primary/SecondaryObjectives → ObjectiveProgress.Progress`, client mirrors.
  Fixed two bugs: scene-walk tracker discovery (FindObjectsOfType returned 0) and secondary-objective
  determinism (seed `UnityEngine.Random` before `SetupSecondaryObjectives`). Log:
  `check: objective recv=26 applied=2 miss=0`, types match (both `COLLECT_POINTS` + `LEVEL_PICKUP`).
- **Off-screen indicator (F part 1) — VERIFIED.** `CoopIndicator.cs` draws a "REMOTE" marker at the
  screen edge when the remote dwarf is off-camera (`check: indicator offscreen remote=… edge=…`).
- **Downed-state (F) — code written, VERIFY BLOCKED by wizard-flow change.**
  - Implemented: `CoopDowned.cs` (downed flag + remote-downed detection via synced hp) and a Harmony
    prefix on `GameController.OnPlayerDeath()` that skips the game-over and marks downed
    (`check: downed role=…`). Not yet verified because the run no longer starts.
  - **New blocker:** the save game progressed (previous runs unlocked content), so the wizard flow
    changed: the landing page now shows `UIMutatorSetEntry` ×6 (mutator selection) and
    `UISetProgressButton_Mastery/Endless/Anomaly` are now available. `UIPlaySelectForm.StartMainMissionsFlow()`
    throws (`Exception thrown by target of invocation`), and pressing `UIMutatorSetEntry` does not
    advance. The `CoopAuto` wizard Plan is now out of date and needs the mutator step reverse-engineered,
    or the save game reset. Splash dismissal is now done in-mod (`UISplashForm.AdvanceFlow()`) instead of
    the flaky external mouse click.
  - `MutatorSetEntry` lives on `UIMissionRoadPage`/`UIMissionNodePage`/`UIChallengeSetSelectPage`
    (`mutatorEntries` field), so mutators are a mission-road selection step, not a landing-page tab.
- **Test harness: different classes + god-mode toggle — VERIFIED.**
  - Class page comes *before* the class-artifact page (old `CoopAuto` Plan had them swapped; fixed).
  - Save game 3 only has **Scout** unlocked (Gunner/Engineer/Driller `[unavailable]`). Forcing a locked
    class in the wizard stalls the artifact page (all artifacts become `[unavailable]`).
  - Fix: host keeps Scout, client overrides to **Engineer** via `RunSettingsManager.SetClass(EDwarf)`
    *after* the artifact step (plan index 6), not during the wizard. Verified:
    host `check: char self=3`, client `check: char host=3 self=1` (Scout=3, Engineer=1), both `alive=True`.
  - `-coopnogod` flag disables the bot's `currentHp=100000` pin → bot takes damage and dies
    (`hpDrops=6, alive=False`), the prerequisite for the downed test. Launcher: `coop-run.ps1 -NoGod`.
  - Class override needs `UIClassSelectPage.runSettingsManager`; `RunSettingsManager` is a plain
    Il2Cpp object (not a MonoBehaviour), reachable via the page field, not `FindObjectsOfType`.
- **Weapon / augment / status-effect system — MAPPED (Milestone B groundwork).**
  - `WeaponHandler` (MonoBehaviour) is the base; subclasses: `ProjectileWeaponHandler` (AutoCannon,
    AlternatingProjectile, Boomerang, ImpactAxe, Lok1, StubbySMG), `BeamWeaponHandler` (LocalSpaceBeam,
    RotatingBeam→SludgePump, ShardDiffractor), `GrenadeWeaponHandler` (Groundzone, LeadBurster,
    PlasmaBurster, Trap), `OrbitingDroneWeaponHandler` (CarrierDrone), plus CoilGun, LinkProjectile,
    RocketSwarm, Trail, Spawn, StunExplosion, WaveCooker, AttachedGroundzone. AI/turret path is
    `AIWeaponHandler` (MonoBehaviour, separate from `WeaponHandler`).
  - Augments are data (`WeaponSkillData`/`ASkillData` hierarchy; e.g. `BeamWeaponSkillData`,
    `ProjectileWeaponSkillData`→`AlternatingProjectileSkillData`, `ArtifactSkillData`). Weapon skill data
    exposes `GetStatusEffectData()` → `List<StatusEffectData>`.
  - `EDamageType` = KINETIC, CRYO, FIRE, ACID, PLASMA, ELECTRICAL. `EStatusEffect` = NONE, SLOW, STUN,
    BURN, ELECTROCUTE, CORRODE, FROZEN. So "weapon + upgrades → fire damage" is exactly
    `EDamageType.FIRE` + `EStatusEffect.BURN`, applied on the host and carried on the projectile
    (`Projectile.Shoot(... statusEffects, EDamageType damageType, ...)`).
  - **Sync conclusion:** effects are host-computed (damage + status applied on the host); the client
    already mirrors the *outcome* (enemy hp/death via enemy sync). What is NOT synced is the *visual*
    (projectile/muzzle/grenade/turret/status particles) — that is Milestone B's spawn-event work.
- **Auto-wizard FIXED (root cause found).** The wizard wedged on the Hauptmissionen screen: pressing the
  campaign card `UIChallengeSetButton_MissionRoad` did nothing, and the page never advanced (the mod then
  pressed it every 2 s via a fallback, which made it look worse).
  - Root cause: the mod cleared the publisher splash itself with `UISplashForm.AdvanceFlow()`. The splash's
    flow includes `ShowSaveSlotSelectorIfNeeded` (selecting/loading the save), so skipping it left the
    campaign half-initialized; `StartMainMissionsFlow()` then threw a NullReferenceException inside
    `UIMissionSelectPage.Show(...)` and the page could not change. (The mod's own log named the exception
    only after `Cause()` unwrapped `TargetInvocationException`.)
  - Fix: **never advance the splash from the mod.** Clear it with a real mouse click instead. Then the API
    driver works again untouched. Verified end-to-end: host `IN RUN after 20 steps`, client
    `IN RUN after 19 steps`, both `peer connected`, `alive=True`, `check: char self=0` / `host=0 self=1`.
  - Evidence that it is the splash, not the card: with a real click on the splash the campaign card
    advanced normally and the wizard walked `UIMissionRoadButton_` → `MissionSectorButton_` →
    `MissionNodeButton_` → `ClassSelectButton_` → `ClassArtifact` → `HazLevelButton` → IN RUN. A recorded
    real-mouse replay (`tools\coop-wizard.ps1`) reached IN RUN the same way, while `AdvanceFlow` did not.
  - The wizard's page flow is now logged by button signature (`auto: wizard page: …`) so the sequence is
    visible in the log instead of guessed.
  - `UIMutatorSetEntry` is display-only (`Show`/`Hide`, hover via `OnSelect`); it has no click action, so it
    must never be pressed as a fallback.
  - Level-up rewards are auto-served by the mod's `ServeLevelUp` (inside the auto driver's in-run branch);
    enabling `-coopwatch` now also clears the splash, so rewards can be automated without the wizard driver.
- **Downed / revive (F) — code done, revive not yet observed completing.** `CoopDowned` marks downed and a
  `GameController.OnPlayerDeath` prefix blocks game-over; the survivor bot rescues (walks into a 3 m circle),
  and the downed dwarf revives itself after 5 s. Two harness bugs found: relative-position rescue appeared
  frozen (the game rewrites the position between ticks; now an absolute target is written), and a false
  "remote downed" fired the moment one side started a run (now gated on having seen the remote's hp > 0).
  In an earlier full test **both** went down before the 5 s elapsed, so nobody could revive; the rescuer's
  hp is now pinned only while rescuing so it survives the rescue window.
  - **VERIFIED end-to-end, fully automated** (`coop-run.ps1 -Auto -NoGod`):
    `check: downed role=client (game-over blocked)` → host `check: downed remote role=host (remote hp=0)` →
    `check: rescue started role=host dist=11,8` → `check: revive started role=client dist=2,8` →
    `check: revived role=client hp=100 alive=True`, and the run kept going. No game-over screen appeared.
- **Combat visuals (B) — stage B1 done: the fire event is synced.** New `PacketType.Fire = 14` carrying the
  muzzle position (0.1-unit fixed point) and aim direction (1/10000), sent unreliable.
  - Hook: a Harmony *prefix* on `ProjectileWeaponHandler.ShootProjectileInDirection(pData, direction, …)`.
    It carries the firing handler, so `handler.owner` identifies the shooter; only the local player's own
    weapon forwards (`ProjectileShootPatch`). Rate-limited to ~25 events/s.
  - Verified (`coop-run.ps1 -Auto -NoGod`): host `shots 85 fireSent=85 fireRecv=115`,
    client `shots 117 fireSent=117 fireRecv=91` - each side's shots reach the peer, both directions.
  - **Rendering a visual is blocked by the build itself.** `GameObject.CreatePrimitive` returns objects
    whose material is `Hidden/InternalErrorShader` (this build ships no Unity default resources), so no
    procedural primitive can ever be visible. `Shader.Find("Sprites/Default")` / `Unlit/Color` / `Standard`
    return shaders with no compiled variants (`Shader.Find` succeeds, the draw produces nothing - the
    "applied but invisible" symptom), and a shader borrowed from a scene renderer was `FX/RT_Blobs`, a VFX
    shader that also draws nothing for a plain mesh. See the tracer attempt in `CoopCombat`.
  - Therefore the tracer is **off by default** (`-cooptrace` re-enables it) and B2 must reuse the game's
    own assets: either clone an existing visual the way `CoopPuppet` clones the player subtree, or - better
    - resolve the firing skill's real projectile/VFX prefab and spawn that. `Projectile.Shoot(WeaponHandler,
    ProjectileSettings, Vector3 origin, Vector3 direction, List<StatusEffectData>, EDamageType, float)` and
    `Projectile.SetInitialState(Vector3 origin, Vector3 direction, int pierce, ProjectileSettings, …)` are
    the spawn points; `ProjectileSettings` is small and serialisable (LifeTime, Speed,
    VFXThrottleFraction, OverrideDamageScalar, Ricochet, RicochetOnEnemy, SurvivesEnvironmentCollision).
  - `-cooptracerdebug` (and `coop-run.ps1 -TracerDebug`) still exist for re-testing a drawing approach;
    they print the chosen shader and the tracer's screen position.
- **Combat visuals (B) — stage B2: real projectile resolution works, spawning needs the game's pool.**
  The Fire packet now also carries the firing skill's asset **name**. This is the right key: both
  instances run the same content, so the host's skill asset is the client's skill asset.
  - Client indexes every `ProjectileWeaponSkillData` via `Resources.FindObjectsOfTypeAll` - **33 assets**
    in a run, and every shot resolved (`noSkill=0 noPrefab=0`). Host logs confirm the shot really carries
    the asset: `shot skill='ProjectileWeaponSkill_Subata' prefab=True`.
  - `ProjectileWeaponSkillData.ProjectilePrefab` is a `Projectile` component reference, so the real
    prefab is reachable: `Instantiate` produced `DamageProjectile_Kinetic_Medium(Clone)` with a live
    `Projectile` component and `ProjectileSettings` non-null.
  - **Blocker:** `Projectile.SetInitialState(...)` throws `Il2CppException` (masked NRE) on a bare
    `Instantiate`. The pooled projectile the game hands out has been through
    `Init(ObjectPool<Projectile>, LayerMaskLibrary)` first; a fresh clone has those fields null. Next step:
    obtain the game's projectile pool + layer-mask library (or reuse an already-`Init`ed pooled instance)
    before `SetInitialState`/`Shoot`.
  - Second finding: `ProjectileSettings.Speed` read **1000** for `Bosco_ProjectileWeaponSkill_Base`, so it
    is not a units/second muzzle velocity - the real velocity comes from the skill's `FireConfigs`
    (`FirePointOffset`, `worldDirStartOffset`, `RotateDirPerShot`, `BurstShots`, `BurstFireRate`).
  - Safety kept regardless: the mirror is spawned with `damageScalar` 0 and no status effects, so a client
    can never damage a host-owned enemy and desync the swarm.
  - All of this is behind `-cooptrace` (still off by default), so a normal run is unaffected.
- **Combat visuals (B) — stage B3: the spawn plumbing is located, one piece still missing.**
  A bare `Instantiate` cannot work, but the game has the right door: `Assets.Scripts.SkillSystem.
  ProjectileProviderStandard(WeaponSkillData data, LayerMaskLibrary layerMaskLibrary, BulletTrailVFXSystem
  bulletTrailVFXSystem)` builds a pool whose `Get()` hands out a projectile already through
  `Projectile.Init(pool, layerMaskLibrary)`. `UpdateProjectilePrefab(Projectile)` re-points one provider at
  any skill's prefab, so a single instance can serve every shot the peer fires.
  - Plumbing is borrowed from the local player's own weapon handlers (the provider is a field on
    `LinkProjectileWeaponHandler`; `LayerMaskLibrary` is a **property** on `WeaponHandler`, which is why a
    fields-only scan found nothing - the cheap mistake that cost a run).
  - Verified progress: `combat: masks=True trails=False` then
    `combat: projectile provider built for 'Bosco_ProjectileWeaponSkill_Base'`. The provider constructs.
  - The missing piece was `Player.BulletTrailVFXSystem` - a public property the player is injected with
    (vfx, layer masks and most managers all arrive on `Player.Inject(...)`). Borrowing it from the player
    instead of hunting for a projectile-carrying weapon closed the gap.
  - **Verified (client, this run):** `combat: masks=True trails=True` ->
    `combat: projectile provider built for 'ProjectileWeaponSkill_Subata'` ->
    `combat: mirror projectile #1/#2/#3 skill='ProjectileWeaponSkill_Subata'
    prefab=DamageProjectile_Kinetic_Medium go=DamageProjectile_Kinetic_Medium(Clone)`.
    Counters `proj=109/0` - 109 spawned, 0 failed (was `proj=0/98`), with `live=0`, so the game also
    reclaims them. The client now spawns real, pooled, damage-neutral game projectiles.
  - A burst screenshot of the client during combat shows a fully rendered scene with projectiles, muzzle
    flashes and impact VFX. One frame cannot attribute an individual projectile to the mirror stream, so
    the *spawn* is proven and *visibility* is strong but not yet isolated.
  - Render probe added (`-cooptrace` only): 0.4 s after spawn it reports the object's survival, distance
    travelled, every renderer's enabled/isVisible state, its shader, its layer and the camera's mask.
    Deferred on purpose - `Renderer.isVisible` is written during culling, so asking on the spawn frame
    always answers false.
  - **Probe result (client):** `active=True travelled=9,2 rends=1 drawn=0 layer=9 camHas=True on-screen
    [r0 en=True vis=False URP/URP_Matcap_Unlit_Simple]`. So the mirrored projectile is alive, has moved
    ~9 units, has exactly one renderer that is **enabled**, carries a **real game shader** (not
    `Hidden/InternalErrorShader` as the procedural primitives did), sits on layer 9 which the main camera
    **does** render, and is inside the viewport - yet Unity's culling still reports it undrawn.
  - That combination (enabled, right layer, on-screen, real shader, still `vis=False`) points at bounds or
    mesh setup that the game performs inside its own `Shoot(...)` path, which this bypasses by calling
    `SetInitialState` directly. **Control experiment before chasing it:** run the same probe against the
    host's own projectiles. If those also report `vis=False` at +0.4 s, the signal is simply unusable at
    that sampling point and the mirror visual is fine - which is the cheaper thing to rule out first.
  - Everything stays behind `-cooptrace`; a normal run is unaffected, and the Fire event itself (B1) is
    verified and always on.

- **Test stimulus for weapons: any of the 75 weapon skills can be equipped on demand (build 14:34).**
  The blocker below ("nothing throws, and one cannot be attached") turned out to be the *wrong entry point*,
  not a missing fixture. The game has a named, public way to hand a weapon to a player, and it works:

  ```text
  check: dev weapons n=75 role=host [0]AlternatingProjectileSkillData_Boltshark [1]BeamWeaponSkill_Electrical_TH0R
    [2]BeamWeaponSkill_RotatingBeam_CryoCannon … [25]Driver_ProjectileWeaponSkill_ChimeraFragcannon
    … [43]GrenadeWeaponSkillData_Leadburster [44]GrenadeWeaponSkill_Cryo [45]GrenadeWeaponSkill_HEGrenade
    … [48]ImpactAxeSkillData … [61]ProjectileWeaponSkill_AutoCannon …
  check: dev give 'GrenadeWeaponSkill_HEGrenade' via=EquipNewWeapon handler=yes data='…' owner=self
    weapons=1->2 registered=1 role=host
  grenade: throw #1 handler skill='GrenadeWeaponSkill_HEGrenade' own=1 dir=-0,15/0,99
  grenade: throw #1/1 base skill='GrenadeWeaponSkill_HEGrenade' prefab='Grenade_HE' from=(9.8,1.0,7.0)
  RESULT: grenade PASS reason=handlerThrows=7 baseThrows=7        (host)
  RESULT: grenade PASS reason=handlerThrows=12 baseThrows=12      (client)
  ```

  - **The path:** `Player.EquipNewWeapon(WeaponSkillData, bool)`, plus `EquipNewWeaponAtActionbarIndex`,
    `EquipBoscoWeapon`, `RemoveEquippedWeapon`, `HasWeapon`, `GetWeaponCount`, and the list the game
    actually drives: `Player.weapons` (`List<WeaponHandler>`) with `WeaponAdded`.
  - **The flag aids:** `-coopdev-weapons` lists every `WeaponSkillData` asset once; `-coopdev-give=<name[,name]>`
    equips them at run start (`coop-run.ps1 -DevWeapons -DevGive <names>`). Every give logs the weapon count
    before/after, so "the game accepted it" is a number and not an assumption.
  - **Why the first attempt failed, precisely:** `GrenadeWeaponSkillData.AddWeaponHandler(go)` +
    `Init(data, owner, firePoint)` produces a handler that looks complete (right data, right owner) but is
    never inserted into `Player.weapons` - so the game never drives it. Correct initialisation is necessary
    and not sufficient; *registration* is the missing half.
  - **`weapons=1` at run start**, even though the dwarf visibly carries the Subata and Bosco: the drawn
    starting weapon is not in that list either. Worth knowing before reading a count as "number of guns".
  - **Weapon catalogue facts this produced:** 75 weapon skill assets on both instances, including the
    `Driver_*` line (a whole dwarf's set), `BeamWeaponSkill_*`, `ImpactAxeSkillData`, `CoilGun`,
    `DefenseDrone*`, `Boomerang`, `AlternatingProjectile*`, `LinkProjectile*` and eight grenade skills;
    the list also contains the **enemy** weapons (`EnemyProjectile*`, `ExploderWeaponData*`), so a name
    filter has to exclude those.
  - **Turrets are cards too:** `SpawnWeaponSkill_Turret_Flamethrower` was offered at a level-up, so the
    "turret" part of milestone B is a `SpawnWeaponSkill` weapon, not a `Turret` type.

- **An instrument trap now measured twice: on interop objects, `GetType()` and `is` describe the wrapper.**
  A grenade equipped through the game threw seven times in one run while the probe reported
  `grenade handlers=0`, because `h is GrenadeWeaponHandler` was false for it. `GetType().Name` is no better -
  every level-up card printed the declared base type `ASkillData`. Both are trustworthy only for objects this
  mod created itself through the managed type (a runtime `AddComponent<GrenadeWeaponHandler>()` did satisfy
  `is`). **The reliable discriminator is the asset name** from the game (`data.name`), which is what the
  grenade checks use now. The same mistake had been built into the card-offer log and into the fixture
  preference; both are name-based after this.

- **Launcher: the "stuck on splash" console was a real defect, and it also clicked into live runs.**
  `coop-click.ps1` ran synchronously for up to 240 s and clicked **every** game window each round, with the
  role mapped only through log markers. So (a) the console kept showing its last `host=on-splash` line while
  `coop-run.ps1` waited silently, and (b) an instance already in the menu or in a run kept receiving centre
  clicks - which presses UI buttons. Two runs today "hung on the splash" and silently doomed themselves.
  Now: role→PID from the process command line (`-coophost`/`-coopclient`), clicks only for instances that are
  still before the splash, a named `GAVE UP after Ns` warning, and a live per-role progress line during the
  result wait (plus an early "no instance has reached a run after 90s" warning). Verified in the next run:
  `phase: run-started t=+31,5s` / `auto: IN RUN after 14 steps`.

- **Combat visuals (B) — grenades: the fixture has no grenade weapon, and one cannot be attached (build 13:42).**
  The plan said "hook `Grenade.Throw`/`ShootGrenade`, broadcast, spawn on the client". Measured first, in three
  steps, each of which answered a different question:

  1. **Does anything throw a grenade?** No. `check: grenade dump weapons=2
     [WeaponHandler:'ProjectileWeaponSkill_Subata':self, WeaponHandler:'Bosco_ProjectileWeaponSkill_Base':self]
     grenadeHandlers=0 inScene=13 active=0 skillAssets=10` on **both** roles - the dwarf has its primary and
     Bosco, no grenade handler, and the 13 pooled `Grenade` objects were never active. So the fixture has
     nothing to measure: this profile has **nothing equipped** (`EquippedGear = []` in the save, the `eq=0`
     gap already noted for I1).
  2. **Can the game's own factory attach one?** Not this way.
     `GrenadeWeaponSkillData.AddWeaponHandler(player.gameObject)` returned a plain **`WeaponHandler`** (not a
     `GrenadeWeaponHandler`), owner set by a follow-up `Init(...)`; `grenadeHandlers=0` afterwards and zero
     throws in 25 s. The factory adds what the *base* class adds - the wrong door for a grenade.
  3. **Can the grenade component itself be attached?** It can be created, but it is not driven.
     `AddComponent<GrenadeWeaponHandler>()` + `Init(data, player, player.FirePoint, null)` with
     `GrenadeWeaponSkill_HEGrenade` gives a real handler -
     `check: grenade give 'GrenadeWeaponSkill_HEGrenade' via=AddComponent+Init handler=GrenadeWeaponHandler
     isGrenade=1 data='GrenadeWeaponSkill_HEGrenade' owner=self` - and still **zero throws in 25 s**. A
     correctly initialised handler (data, owner, fire point) is necessary but not sufficient: the weapon list
     is built by the game from class and loadout at run start, and a handler attached afterwards is not part
     of it.

  Two further runs of step 3 (`20261019-135025`, `20261019-135619`) produced **no data**: the automation sat
  on `auto: waiting on splash (leave it to a real click)` - the launcher's click did not land - and the test
  timed out. That is harness flake, recorded so it is not read as a result.

  **What was gained anyway, and it is not nothing:**
  - The grenade asset inventory is identical on both instances (10 assets), so a mirror can resolve a skill
    by name exactly as the projectile mirror does: `Bosco_GrenadeGroundzone_Fire:GRENADE`,
    `Bosco_GrenadeWeaponSkill_Cryo:GRENADE`, `Driver_GrenadeWeaponSkill_ProximityMines:TRAP`,
    `GrenadeWeaponSkillData_Groundzone_Acid|Fire:GRENADE`, `GrenadeWeaponSkillData_Leadburster:GRENADE`,
    `GrenadeWeaponSkill_Cryo|HEGrenade|PlasmaBurster|ShredderSwarm:GRENADE`.
  - `Grenade.Throw` has overrides (`GrenadeShredderSwarm`, `LeadBursterGrenade`) that do not call base, so it
    is **not** the single entry; `GrenadeWeaponHandler.ThrowGrenadeInDirection` is, and only `TrapWeaponHandler`
    overrides that. Both hooks are installed and counted side by side, so a throw seen in one and not the
    other names a subclass prefab instead of hiding it.
  - The saves live in a **Steam-ID subfolder**: `%USERPROFILE%\AppData\LocalLow\Funday Games\DRG Survivor\
    steam\<SteamID>\drg_save_slot{1..3}_{0..3}.dat` (the earlier notes said "the same folder" - it is one
    level deeper than that).
  - A named outcome now separates the two zeros: `NO_GRENADE_THROWN` (no weapon at all) from
    `HANDLER_NOT_DRIVEN` (handler present, game never fires it). The wording is in the build; the two runs
    that would have shown it flaked, so it is **unobserved** until the next successful run.

  **Superseded by the build above:** the blocker was not a missing fixture but the wrong entry point. A
  grenade *can* be equipped on demand through `Player.EquipNewWeapon` and the game throws it (7/12 throws,
  PASS both roles). The gear/loadout hypothesis was wrong on its own terms as well: `EGearSlotType` is
  `TOOL, ARMOR, GRINDER, WEAPON_MOD, TANK, COMPANION` - there is no weapon or grenade slot in the loadout, and
  `GearData` carries stats, no `WeaponSkillData`. A grenade mirror is now a code task with a working stimulus.

- **Combat visuals (B) — the draw oracle: five in-process paths closed, all with evidence (build 03:30).**
  The open gap after B3 was narrow and honest: the client spawns real pooled projectiles (`proj=109/0`) and a
  screenshot of a fight looks right, but "the mirrored projectile is actually drawn" was never *proven*.
  Chasing it produced a result of its own kind - **every in-process reading this build offers is closed**, and
  each one with the exact evidence, so nobody has to walk them again:

  1. `Renderer.isVisible` - no information (B3's A/B control: the game's own projectiles read `vis=False`
     exactly like the mirror's, at every offset).
  2. A managed `MonoBehaviour` added at runtime to count `OnWillRenderObject` -
     `TypeInitializationException -> NullReferenceException`, i.e. the il2cpp class for a runtime-injected
     managed component does not come up. (`AddComponent<CoopTick>()` at plugin load does work, so it is the
     *injected* type that fails, not the mechanism.)
  3. `Camera.Render()` - **not in the interop assembly at all**; the game never calls it and IL2CPP only
     ships what the build uses.
  4. `Camera.CopyFrom()` - same.
  5. `Texture2D.ReadPixels` from a RenderTexture - **returns black**. That one is decisive: the probe tried
     all five cameras in the run, one frame each, and every single read was `mean=0`
     (`Camera(en=1) RT_Camera(en=1,rt) Camera(en=1) Main Camera(en=0) UICamera(en=1)`), *including* reading
     `RT_Camera`'s own 256x256 texture, which the game certainly renders every frame. So it is the read path,
     not the camera choice.

  ```text
  03:26:32.280 check: b3draw cameras Camera(en=1) RT_Camera(en=1,rt) Camera(en=1) Main Camera(en=0) UICamera(en=1) -> trying 'Camera'
  03:26:32.397 check: b3draw camera 'Camera' rendered nothing, trying 'Camera'
  03:26:34.108 check: b3draw camera 'Camera' rendered nothing, trying 'RT_Camera'
  03:26:34.165 check: b3draw camera 'RT_Camera' rendered nothing, trying 'Main Camera'
  03:26:34.207 check: b3draw camera 'Main Camera' rendered nothing, trying 'UICamera'
  03:26:34.252 RESULT: b3draw FAIL reason=verdict=NO_RENDERING_CAMERA
  ```

  **The control earned its place here.** `NO_RENDERING_CAMERA` is the outcome the positive control is for:
  without it this run would have reported `MIRROR_NOT_DRAWN` and sent the next agent hunting a visual bug
  that the measurement never looked at. Hiding the dwarf - which is certainly on screen - moved zero pixels
  too, and that is what said "the instrument is blind", not "the mirror is missing".

  Two facts about the build that fell out and are worth keeping:
  - **`Camera.main` is `UICamera`**, and `Camera.allCameras` lists *only enabled* cameras - one in-run, the
    UI camera. The world is drawn by cameras that are not in that list, which is why the search had to go
    through `Resources.FindObjectsOfTypeAll<Camera>()`.
  - A parked projectile can be produced on demand: `CoopCombat.SpawnParkedMirror` spawns through the normal
    mirror path and zeroes its velocity, which is what any future oracle needs (a flying projectile answers
    "was it drawn" only sometimes).

  **What is unchanged:** the mirror spawns, is pooled, damage-neutral, carries a real game shader, sits on a
  rendered layer - and no available reading contradicts that it is drawn. What is still not proven is that it
  is drawn. The remaining oracle is **outside the process** (capture the game window while a parked mirror is
  held in front of a camera and diff two frames); every in-process path is now known to be closed.

- **Combat visuals (B) — stage B3 control experiment: the `isVisible` signal is unusable, not the visual.**
  The open question was whether the mirror is really drawn, because the probe reported `vis=False` for an
  object that was alive, moving, enabled, on the right layer and on screen. The fix was to stop measuring
  the mirror against nothing: `-cooptraceab` puts **both kinds in one instance**. A Harmony prefix on
  `Projectile.SetInitialState` (the single private method every projectile goes through) registers the
  game's own projectiles as `kind=local`, `CoopCombat.InMirrorSpawn` marks the mod's own call so its
  projectile registers as `kind=mirror`, and the *same* probe samples both at +0.1/0.25/0.4/0.8 s. The flag
  also loops the local shot back into the mirror path, so real and mirror are spawned from the same muzzle
  in the same frame.
  - **Observation** (`coop-run.ps1 -Deploy -Auto -Test b3 -TracerAB -Only host`, archived log):
    `check: B3 result=PASS verdict=SIGNAL_UNUSABLE local_vis=0/33(n=36 done=6) other_vis=0/0 mirror_vis=0/8(n=8 done=2)`
    - All 33 alive observations of the game's **own** projectiles read `vis=False`, at every offset, with
      `rends=1 en=True layer=9 camHas=True on-screen` and a real shader
      (`URP/URP_Matcap_Unlit_Simple`) - byte-for-byte the same reading as the mirror's 8 observations.
    - `combat: probe kind=local … travelled=2,4 / 5,2 / 8,0 / 16,0` is the same ~22 u/s curve as the
      mirror's `3,1 / 6,6 / 9,7 / 18,0`, so both kinds did travel and were sampled mid-flight.
    - Recycled projectiles are visibly different (`active=False`), so the probe does distinguish states.
    - Hook counters prove the plumbing: `hookReal=11 hookMirror=3`.
  - **Derived:** at these offsets `Renderer.isVisible` carries **no information** about whether a
    game-drawn projectile is on screen in this build. The earlier `vis=False` on the mirror is therefore
    **not evidence** that the mirror visual is broken, and B3's "chase the game's `Shoot(...)` path" branch
    is closed. The instrument was the broken part, not the visual.
  - **Unchanged by this run:** `Renderer.bounds.extents` came back `bounds=n/a` for every renderer, so the
    bounds hypothesis was neither confirmed nor refuted - the property is not reachable the way the probe
    asks for it (same reflection gap that hides `isVisible`).
  - **Unknown (honest gap):** "the mirrored projectile is actually drawn" is still **not proven**. What is
    proven is that no available reading contradicts it, and that the one reading which seemed to had no
    signal. Both oracles named here were tried later and are **closed**: the parked-object pixel diff cannot
    read a frame (see the draw-oracle section further down - `Texture2D.ReadPixels` returns black, and
    `Camera.Render`/`Camera.CopyFrom` do not exist in the interop assembly), and the injected
    `MonoBehaviour` for `OnWillRenderObject` fails to construct. **The only oracle left is outside the
    process:** hold a parked mirror in front of a camera (the parking helper exists and works) and diff two
    captures of the game window taken from outside.
  - **Side effect of the run that is worth keeping:** the automation reached the mission in **~54 s** from
    plugin load (phases: menu at +19 s, wizard 7 steps at 2 s each, `run-started` at +35,6 s, first shot at
    +48 s, verdict at +49 s). The old "3-4 minutes to mission start plus a fixed 80 s wait" was mostly the
    guessed wait, not the boot.
- **Test harness (repo-relevant technique, not just this game).** Three things made the run above possible
  and are reusable for every later milestone:
  - The mod ends its own test: `-cooptest=<name>` makes it decide when the question is answered, write
    `RESULT: <name> PASS|FAIL reason=… role=…` into the per-role log and quit. The launcher polls that line
    instead of sleeping a guessed number of seconds, and an unanswered test reports FAIL after a timeout
    (`-cooptesttimeout`, default 420 s) rather than producing no line at all. Verdicts name their
    inconclusive outcomes (`MIRROR_MISSED`, `NO_REAL_ALIVE`) instead of hiding them.
  - Every step from plugin load to in-run logs `phase: <name> t=+Ns dt=Ms`, so boot cost is attributed
    instead of guessed. (Caveat: markers logged before the first frame report a different time base -
    `plugin-load t=+2,0s` precedes `first-tick t=+0,3s` - so only `dt` between frame-time markers is
    trustworthy.)
  - Deploy is verified: the DLL copy is checked by SHA-256 against the build output, because a stale binary
    looks exactly like a fix that does nothing. Test runs archive their logs and screenshots under
    `local/work/runs/<stamp>-<test>/`, back the save profile up first, and always stop the processes they
    started.

## Stage 1 damage — client hits now reach the host (build of 18:22)

The client's weapons used to hit only its own frozen mirrors; the host never learned about the damage, so
the client contributed nothing to a kill except its own local miskill (`localDie`). The accepted model is
now in place: **the client runs its weapons locally and reports the damage, the host applies it to the
enemy it owns**.

- Observed: `check: hit sent=20 dmgTotal=532 flushes=15` (client) and
  `check: hit recv=20 applied=20 missed=0 unmatched=0 dmgTotal=532` (host) in the same run. Both damage
  totals are **identical**, and every one of the 20 hits was applied - so the two sides agree on the
  number, not just on the count.
- Wire: `EnemyHit` (0x0F) = `count(u16) × (hostId u16, dmg i32)`, ReliableOrdered. Reported damage is the
  **hp delta the client's own `TakeDamage` produced**, not the incoming number: the mirror carries the
  same armor as the original, so the delta is already final and the host applies it with
  `ignoreReduction: true`. A second armor pass would have made the two sides disagree by a fixed factor.
- Hits are summed **per enemy** and flushed at ~16 Hz, so a burst of splash damage costs one entry, not one
  packet (20 hits over 15 flushes). Applied in `CoopEnemy.DrainHits` on the main thread.
- Host applies through the game's own path, `Enemy.TakeDamage(dmg, false, KINETIC, doFx:false,
  PROJECTILE, hostPlayer, ignoreReduction:true)`, so death, kill credit and loot follow from the real code
  instead of being reproduced by hand. `entitySource` is the host's dwarf (the real remote player does not
  exist on this side), so kills are currently attributed to the host dwarf - a known gap for the
  per-dwarf stats work (PLAN.md I2).

### Cost: patching a virtual method *and* its override kills the game

The first attempt patched **both** `Entity.TakeDamage` (virtual) and `Enemy.TakeDamage` (its override).
Both instances died ~1 s after the plugin loaded, deterministically, with
`0xc0000005` (access violation) in `coreclr.dll` at the same offset; the mod log simply stopped after
`test: begin` and BepInEx showed `Il2CppInterop ... During invoking native->managed trampoline`.
No Player.log, no dump - and the launcher reported `verdict: FAIL` with no RESULT line, which is the
intended shape for "the processes died".

Patching only the override (`Enemy.TakeDamage`) works, and that is the rule: **patch the override, never the
virtual+override pair.** The existing hooks already followed it (`Enemy.Die`/`DeSpawn`/`Recall` are
overrides and are patched that way); `TakeDamage` was the first place it was violated. Confirmed from the
Cpp2IL dummies: `Entity.TakeDamage` is `public virtual bool`, `Enemy.TakeDamage` is `public override bool`,
and both were being detoured.

Also learned from the same dummies, and worth keeping: the signature and its defaults are
`Entity.TakeDamage(long dmg, bool isCrit=false, EDamageType=KINETIC, bool doFx=false,
EDamageSource source=PROJECTILE, Entity entitySource=null, bool ignoreReduction=false)`. Knowing that
replaced the reflection call with a direct one, which is both simpler and safer on IL2CPP.

## Incoming damage — the client's dwarf is already in danger (build of 18:39)

Before writing anything for "enemies must be able to hurt the client", we measured whether they already
do. They do, and the reason is structural rather than lucky:

- A mirror is a **real `Enemy` created by the client's own spawner**, so the game handed it the client's
  player at spawn. The client's mirror swarm regards the client's dwarf as its target and attacks it.
- Measured with `-coopnogod -coopswarm -cooptest=damage -cooptestmin=120` (god mode off, client glued to
  the nearest mirror so the dwarf is actually in reach): the client lost **8×10 hp in ~34 s**, every hit
  logged with the distance that makes it attributable - `nearestMirror=0,7–1,2`,
  `type=GRUNT0/BASE`/`FAST_GRUNT/BASE`, `enemiesWithin5=2–7`. The host took **8×10 hp in the same
  window**. Both sides are hurt at the same rate while both are in the swarm.

So no damage packet is needed in this direction either. Each side is authoritative for its own dwarf's hp:
the client computes its own hits, the host reads the result as `remoteHp` in `PlayerState`, and
downed/revive already keys off that number. `EnemyHit` (client -> host) and this (client-local) are the
same rule, "whoever runs the simulation of an entity owns its hp", applied to the two entity kinds.

### Still open: aggro, and it is not a cheap fix

`Enemy.Player` is a **getter-only** property backed by a private field that
`OnCreated(..., Player player, ...)` sets once at spawn, and an instance has exactly one `Player`. On the
host every enemy therefore aims at the host's dwarf, and nothing on the host can be aimed at the client's
dwarf. "Host aggro on the puppet" is not a target reassignment; it needs a second *real* `Player` on the
host (DI init, HUD, camera). Consequence today: the swarm follows the host, and the client is hit because
it stands in the same space - not because the enemies chose it.

**Design decision (18:40):** the partner *should* ideally have its own aggro, but this is **recorded as a
later goal and deliberately not prioritised now**. Parked in PLAN.md B2 and in the plan file under
"Später (bewusst NICHT priorisiert)". The cheap thing to try first when it is picked up is dropping
`SetAgentEnabled(false)` to see whether an unfrozen mirror chooses the client by itself; the proof has to
be a *closing distance* over several seconds, not an accidental hit.

### Two measurement traps this cost

- **God mode hides all damage.** The test bot pins hp to 100000, so every earlier run reported
  `hpDrops=0` — which says nothing about enemies and was briefly read as "the client is safe". Damage
  questions require `-coopnogod`.
- **One instance quitting invalidated the other's window.** The host reached its threshold after 12 s and
  exited; the client then spent 85 s alone with no partner, a pending level-up barrier (`barrier=both`)
  and a frozen mirror set, and its "1 hit, then nothing" was drawn from that dead window. The first
  conclusion ("frozen mirrors do not attack") was wrong for exactly this reason. `CoopTest.MinSec`
  (`-cooptestmin=<s>`) now holds a verdict until both sides have had the same observation window, and the
  re-run reversed the finding. A two-instance test has to treat the other instance's lifetime as part of
  its own measurement.

## Level-up cards: the reward screen is drivable, and every earlier run took no reward (build of 18:52)

The automation answered level-ups by calling `LevelUpManager.OnLevelUpFinished`. That moves the flow on
and keeps a run alive, which is why it was chosen - but it also finishes the screen **without committing
a card**. The consequence had never been noticed: *no* automated run had ever applied an upgrade, so the
whole upgrade system was untested and "the run passed" meant "the run passed with no upgrades".

The screen is drivable; the earlier attempt to press a card failed for a documented reason worth keeping:

- `UISkillButton`'s press callback lands in `UILevelUpForm.OnSkillSelected`, which only **previews** a
  card. That is why pressing it left the run frozen.
- The commit is the public **`UILevelUpForm.OnSkillChosen(UISkillButton, SkillRaritySet)`** - the call the
  game makes when the player confirms.
- The card's data is reachable without naming private fields: `UISkillButton.SkillRarity` (public) gives
  the `SkillRaritySet`, and `SkillRaritySet.SkillData.name` is the readable card name.

`CoopUpgrade.cs` (new) does the pick; `CoopAuto.ServeLevelUp` now tries a real card **first** and keeps the
manager bypass only as the last resort that stops a screen from wedging the run - and counts it
(`Skipped`), so "the run continued" can no longer hide "the dwarf gained nothing".

Measured (`-Test levelup -TestMinSec 120`), PASS on both roles with `skipped=0`:

```
host   picked=3   SupportSkill_MoveSpeed / SupportSkill_MiningSpeed / SupportSkill_MoveSpeed   (index 0)
client picked=3   WeaponUpgradeSkill_FireRate ×3                                               (index 1)
```

- The two sides take **different cards**, which is the point: the pick is per player, not one shared
  choice echoed twice. Each instance is offered its own list (observed order differs: host
  `Damage/FireRate/ReloadSpeed`, client `FireRate/Damage/ReloadSpeed`), and the mod deliberately prefers a
  different index per role so a run cannot pass by both sides happening to agree.
- The XP barrier is unaffected: `local picks done, holding until remote picks` → `remote picks done`
  around every pick.
- The test needs **two** picks, not one: the interesting failure is the second time the form is opened
  after the flow has already been through the barrier once.

This removed the last "automation-only shortcut" in the level-up path: a run now applies real upgrades,
so later damage/progression measurements are measuring a real game.

## Run settings adoption: the levels matched by coincidence, and now they match on purpose (build of 19:09)

Both instances generated an identical map long before any of this existed. The reason was not
synchronisation - it was that the test bot walks both wizards through the same fixed page order on
saves that look alike, so both sides clicked the same biome, the same sector and the same node. Only the
hazard was ever forced across the wire (and the seed). Anything that broke the resemblance - a different
save, a different unlock state, a human choosing on one side - would have put the two dwarves in
different caves, and nothing would have caught it.

`RunConfig` now carries the whole run identity: hazard, seed, biome, sector, node, mission type, main
mission type. The host reads it out of its own `RunSettingsManager` and broadcasts it on a 1 s heartbeat
from `CoopTick`; the client applies it in the `MapGenerator.CreateMap` prefix.

- **The heartbeat is not a nicety.** The client reaches its own run preparation *before* the host starts
  its run: the client presses Play when the bot's wizard finishes, and if the config only went out at
  host run start, the client could be past its preparation already. Resending is idempotent.
- **Node identity is (biome, sector index, node index)**, not an object. Both instances read the same
  `MissionSectorData` assets, so the same three indices name the same node on both sides. The client
  resolves it through its own mission road and applies it with the game's own
  `SetupMissionRoadNode(nodeState)` - the call the game makes when a player clicks that node. Setting the
  derived fields by hand would have meant guessing which ones matter.
- **The test needs a deliberate disagreement.** `-coopdrift` (client only) makes the client press the
  *highest* available `MissionNodeButton_` where the host presses the lowest. Without that, both sides
  agree by construction and "settings matched" proves nothing.

Measured (`-Test settings -Drift -TestMinSec 60`), PASS both roles:

```
host   check: settings role=host gathered  settings biome=Crystalline Cavern(0) hazard=0 sector=0 node=0 mission=0/0 mutators=off
client auto: DRIFT - client takes 'MissionNodeButton_05' instead of the lowest node
client check: settings role=client applied=1 drift=1 (CreateMap) settings biome=Crystalline Cavern(0) hazard=0 sector=0 node=0 mission=0/0 mutators=off
```

`drift=1` is the whole point: the client really did choose node 5, the adoption really did pull it back
to the host's node 0, and the game's own state then reads the same on both sides. The other four fields
agreed already (both wizards picked the same hazard/mission/sector pages), so the node is the field this
run actually proves - which is also the field that decides which cave you get.

**Two things measured the hard way:**

- **`PrepNewRun` is not the hook.** It reads like the right place - it builds the level configs and run
  mods from the settings, so adopting before its body runs would beat adopting before terrain
  generation. It never fired. Every run reported the adoption happening at `CreateMap` instead, and the
  method has no caller anywhere in the decompiled `Assembly-CSharp` (it is `internal`, so the UI assembly
  may call it, or it may serve the challenge/endless/retry paths rather than a run started from the
  mission wizard). The patch was removed rather than kept as unexercised code. The honest consequence:
  the adoption lands before terrain generation but not provably before the level configs - everything
  read later reads the corrected manager, and the map fingerprint is identical.
- **A run-start reset wiped the evidence and made a working feature look broken.** The client applies the
  settings in the menu, i.e. before the run exists; clearing the counters at run start left a log
  showing `applied=1 drift=1` next to a test that timed out with `drift=0 none`. Reset only per-run
  bookkeeping, never the facts that describe what happened before the run.

## Readiness handshake: the client may not choose before it holds the host's data (build of 19:26)

"Both sides generated the same map" was, until now, a matter of timing. The host broadcast its settings
once per second and the client's wizard takes ~40 s, so the data always won the race - but nothing
*guaranteed* it, and the guarantee is what a co-op session actually needs: the client must not create its
run from whatever its own menu happened to have selected. So the client now waits.

**The exchange**, all of it headless and testable without any UI:

1. The client sends `Hello` (16) as soon as a peer socket exists - and keeps re-asking every 2 s while it
   is waiting (see the trap below).
2. The host answers with the full `RunConfig`.
3. The client confirms with `ReadyEcho` (17), reporting the values it holds.
4. The host compares that against what it sent and reports `echo=1`.

**The gate** is `CoopAuto.HoldForHandshake`: the client's wizard does not press anything until step 3 has
happened, with a 30 s escape that logs and proceeds, so a lost peer cannot wedge the wizard - the same
shape as the 45 s level-up barrier timeout.

Measured, PASS on both roles, twice:

```
normal                 client: check: handshake role=client ready=1 held=10,1s helloTx=7
                       host:   check: handshake role=host peer-hello=1 peer-ready=1 echo=1 cfgTx=4
gate forced (-ConfigDelaySec 15)
                       client: held=20,3s        host: echo=1
```

**Two checks, not one, on purpose.** The handshake answers "did the data arrive" and the `settings` test
answers "does the game run with it". Collapsing them into one verdict would leave a failure unable to say
which half broke - and they really can fail independently: a value can arrive intact and still be
overwritten by the wizard afterwards.

**The trap that made the handshake look done while it carried nothing.** The `Hello` is sent once at mod
load (~3 s in), and the host has no `RunSettingsManager` until ~25 s in, so the reply was "nothing to send
yet" and the 1 Hz broadcast quietly did all the work. The round trip was implemented, logged, and
useless. It is now re-asked every 2 s while the gate waits, which is what makes it a real
request/response - and the `helloTx=7` in the evidence above is exactly what proves it was exercised
rather than merely present.

**What is left of the plan's G milestone** is only packaging: the in-game host/join screen, the Steam
lobby and invites. The protocol underneath it (settings, handshake, ready gate) now exists and is
verified - which is also why the flags can stay the test path: the harness needs a headless join.

## Where the unlocks live, and whether the game reads them live (build of 20:01)

Host-profile adoption (I1) starts with a measurement, because the plan says so and because guessing here
would have been expensive: the answer decides whether adoption is a memory swap or a save-file reload.

`SaveData` is the object that holds everything worth adopting:

```
Version Timestamp
Challenges
WeaponSaveData          ClassModSaveData        BiomeSaveData      MutatorSaveData
MetaStatUpgrades        MileStoneStates
UnlockedChallengeSetIds UnlockedWeaponIds       UnlockedArtifactIds
UnlockedClassArtifactIds OverclockUnlockedWeaponIds UnlockedClassIds
DailyScore WeeklyScore BiomeScore ...
```

The probe mutated one of those lists in memory and asked the game's own query again:

```
check: profile role=client profile cls=4 wpn=29 art=13 mod=9 oc=0 meta=18 wpnSave=0 dwarf=11110000
check: profile live=yes cleared=4 before=11110000 afterClear=00000000 restored=11110000
```

`live=yes` is the result: clearing `UnlockedClassIds` made `SaveDataManager.IsClassUnlocked` return
false for every dwarf **in the same frame**, and restoring the list brought them all back. So the unlock
state is read live out of `SaveData` - no cache built at load, no reload needed. Adoption can therefore
happen in the menu, before the client picks a class, which is exactly the window the readiness gate from
the previous section already provides. Gear quirks and meta upgrade levels read from the same object, so
"adoption is a save-data swap" stops being the plan's guess and becomes a measured property.

**Three attempts to reach the save, and the reason each failed is the useful part:**

- `GameController` does not exist in the menu. It appears when a run starts - so the natural path
  (`gc.saveDataManager`) is unreachable during the one window adoption needs, and the probe sat in the
  menu reporting "no GameController yet" while the wizard ran without it.
- `SaveDataManager.SaveData` and `PCSaveFileManager.Data` are both **get-only**. Neither can simply be
  assigned, so adoption is either a getter patch or an in-place mutation of the existing object.
- The actual blocker: **`MetaManager.saveDataManager` is not a field on the interop type.** Fields and
  properties both look plausible from a decompile, and only dumping the members settled it:

```
type DRS.Meta.MetaManager fields={ isWrapped pooledPtr}
type DRS.Meta.MetaManager props={ OldPriceData statUpgradeCollection ... saveDataManager wallet
                                  runSettingsManager ... ArtifactRerolls ... transform gameObject ...}
```

  `GetFields()` returns only Il2CppInterop's own bookkeeping, while every real member - including the
  private ones - is a *property*. Reading a private field by name finds nothing and the code fails
  silently, at the most misleading moment. This is worth knowing for every future manager lookup in this
  game, so it is in the playbook gotchas too.

**One more finding with teeth:** both test instances read the *same* save profile
(`%USERPROFILE%\AppData\LocalLow\Funday Games\DRG Survivor\steam`). So at the time of this measurement
"the host's save" and "the client's save" were the same file, and a profile-adoption test would have had
nothing to adopt. Both halves of that are fixed in the next section: the launcher can now put the two
instances on different save slots, and it gives the saves back afterwards.

**Not measured yet:** I2 - how a run end turns stats into rank, mastery and milestones. That is the
plan's second unknown and the prerequisite for crediting the client's progress to the host.

## Two separate save profiles - and giving the player's saves back (build 20:24)

I1 needs the host and the client to have *different* profiles, otherwise there is nothing to adopt. Two
things had to be fixed before that could even be tested.

**Separate slots.** The game keeps three save slots side by side in one folder
(`drg_save_slot{N}_{rotation}.dat`), so the instances can be pointed at different ones. The persisted
choice lives in `PlayerPrefs`, which both instances share - forcing it there would move both - so the mod
forces the slot at `PCSaveFileManager.Load` instead, and only when the launcher asks (`-coopsave=N`).
Measured:

```
host   save: forced slot0 (read back Slot1) before load 1
client save: forced slot1 (read back Slot2) before load 1
host   check: saves peerSlot=1 mySlot=0 separateFiles=yes sameProfile=yes
```

`separateFiles=yes` is the harness property that now has a test (`-Test saves`, PASS on both sides).

**But the slots hold the same progression.** `sameProfile=yes`: Slot1 and Slot2 both report
`cls=4 wpn=29 art=13 mod=9 oc=0 meta=18`. So pointing the two instances at different slots separates the
*files* but not the *content* - a tester has played all three slots to the same point. That matters for
the test design, and it is why the `saves` verdict is "different files", not "different numbers": the
latter would fail for a reason that has nothing to do with the code under test. I1 will instead *scrub*
the client's unlock lists in memory before the wizard, so the host has something real to give it.

The same measurement answered a second question: the client's profile travels over the wire
(`ProfileReport = 18`, plain ints like the rest of the handshake), so the host compares against numbers
the client actually holds rather than against what the host hoped it sent.

**The harness was writing the player's saves and never restoring them.** `Backup-Saves` copied the profile
before a test and nothing ever put it back - so every test run left its progress in the real save. That
is fixed: `Restore-Saves` runs in the `finally` after the game processes are gone, and it verifies itself
against the backup. Two things came out of doing that honestly:

```
saves: restored and verified (14 files, canary removed)
```

- **First attempt broke the save layout**, and the hash check is what caught it. The backup root is the
  *contents* of `steam`, not the folder; restoring one directory level too high left the game with no
  `steam` folder at all. It was repaired from the backup and verified byte-identical (14 files,
  `differences=0`) before anything else ran. Both functions now share one `$script:saveDir` so the two
  copies cannot disagree again.
- **Timestamps cannot prove a restore ran.** `Copy-Item` preserves `LastWriteTime`, so an unchanged mtime
  means "faithful copy" *or* "nothing happened" - and a run that never reached gameplay writes nothing at
  all, which is exactly the case `-Test saves` produces. The restore therefore creates a canary file
  *after* taking the backup and requires it to be gone afterwards; the archive line above is the proof.

**Status:** `saves` PASS (both roles), restore verified, saves byte-identical to the pre-run state.
Next: I1 itself - the host's unlock lists to the client, applied in the menu before the class pick, with
the client's own profile scrubbed so the adoption is visible.

## The host's unlocks now reach the client before it picks a class (build 20:38)

I1's first real increment, and the measurement from the two sections above is what decided its shape: the
game reads `SaveData` live, so adoption is an in-memory swap that can happen in the menu - and the menu is
exactly where the readiness gate from the handshake step already holds the client.

**The six unlock lists travel verbatim.** `UnlockedClassIds`, `UnlockedClassArtifactIds`,
`UnlockedWeaponIds`, `UnlockedArtifactIds`, `OverclockUnlockedWeaponIds`, `UnlockedChallengeSetIds` are
read out of the host's `SaveData` as the strings the game itself stores, and arrive as strings
(`ProfileAdopt = 19`). Nothing in the protocol needs to know what an id *means*: `IsUnlocked` and
`IsClassUnlocked` compare against exactly these strings, so a format guessed wrong cannot half-work. The
counts are bounded on receipt (16 lists, 8192 items per list) because they come off the wire.

**Ordering is what makes it correct rather than lucky.** The client applies the lists inside the same
`TryConfirmConfig` gate that holds its wizard, and the gate now also waits for the payload when adoption
is on. So the class page - built after the gate opens - can only ever show the host's pool.

**And the client stops writing its profile.** `PCSaveFileManager.RequestSave` and `ForceSave` are skipped
while the client runs with borrowed unlocks; otherwise a save request would persist them into the
client's own file, turning "borrowed for this run" into "permanently mine". That is the core of I3.

`-Test profile-adopt` passes both sides, with the client's `SaveData` emptied first so there is something
to adopt (the three save slots all hold identical progression, measured earlier):

```
client  check: profile-scrub role=client cleared before={4 9 29 13 0 1} after={0 0 0 0 0 0} dwarf=00000000
client  check: profile-adopt role=client received {cls=4 mod=9 wpn=29 art=13 oc=0 sets=1}
client  check: profile-adopt role=client applied  {cls=4 mod=9 wpn=29 art=13 oc=0 sets=1}
client  auto: wizard page: UIBackButton ClassSelectButton_Scout ClassSelectButton_Gunner
                     ClassSelectButton_Engineer ClassSelectButton_Driller UIPlayCrumb ...
client  auto: step 22 - pressing (OnButtonClick) plan[4] ClassSelectButton_ 'ClassSelectButton_Driller'
client  RESULT: profile-adopt PASS reason=applied={...} class=ENGINEER classPageButtons=4 received=2
host    RESULT: profile-adopt PASS reason=peerClasses=0 deficit=1 adoptSent=2 mine={... dwarf=15}
```

The client's own save offered **no** classes (`dwarf=00000000`), the host handed over 56 ids, and the
class page then offered **four** - Driller among them, a class that client's save never unlocked.

**Why the assertion is on the class page and not on the final class:** this harness overrides the
client's class afterwards by design (`OverrideClientClass`, from the character-visuals work), so
`class=ENGINEER` is the harness talking. "The page offered four classes to a client whose own save offered
none" cannot be true for the wrong reason; "the client plays Driller" could be.

**Two harness bugs this run exposed**, both of which had been silently shaping results:

- **The game skips wizard steps on its own.** The client stood on `plan[3] MissionNodeButton_` while the
  *class* page was up, because that save remembers its last node and the node page's buttons are all
  inactive - so a rigid plan waits there forever, and the symptom is indistinguishable from a broken
  adoption. The plan now advances to a later step whose button is actually on the page and logs
  `auto: skipped plan[3]` when it does.
- **`classPageButtons` was the *last* page's count, not the class page's.** Every page change refilled it,
  so the hazard page left it at zero and a *passing* adoption reported failure. It is a maximum now: a
  fact about a run must survive the pages that come after it.

**Not in this increment, and worth saying rather than implying:** meta upgrade levels, gear
inventory/loadout and player rank. The six lists decide *what the client may pick*; those decide *how
strong it is*. I2 (crediting the client's progress back to the host at run end) is untouched.

## The corpse that keeps walking: `Die` runs, the animator never comes back - FIXED (build of 23:06)

Reported in play-testing: "the other dwarf sometimes keeps running around as a corpse after a revive." Measured, and
the cause is not where it looks:

```
host  check: corpse die       dmg=KINETIC hp=0 alive=0 anim=Idle#1432961145 t=12.96
host  check: downed role=host (game-over blocked)
host  check: corpse anim-end  Walk condition=DIE hp=0 alive=0 anim=Idle#1432961145 t=12.96
host  check: revived          hp=100 alive=True | after={hp=100 alive=1 anim=Dying#-1546996312}
host  check: corpse after-revive hp=100 alive=1 anim=Dying#-1546996312 t=2.64
client  downed: stood up on its own (hp=13) hp=13 alive=1 anim=Dying#-1546996312 t=0.74
```

- **`Player.Die(EDamageType, long, bool)` runs.** The mod only skips `GameController.OnPlayerDeath`, so the
  death itself - and with it the death animation - still happens.
- **`Player.IsAlive` is a get-only property (derived), but the animation is a separate state.**
  `WalkPlayerAnimator.HandleEndCondition(DIE)` switches the animator to `Dying` (hash `-1546996312`; normal
  `Idle` is `1432961145`). Writing `currentHp` back does not touch it.
- **So the pose stays dead while the dwarf moves**, and it is the same for the game's own recovery: hp 0 to
  13 leaves the animator in `Dying` too. "Sometimes" is about which path came first, not about the pose
  sometimes recovering - neither path recovers it.

`BasePlayerAnimator` declares no counterpart to `HandleEndCondition` in the stubs (only `UpdateAnimator`,
`SetIdle`, `SetMiningWeight`), so the way back had to be found by measurement, and the measurement is what
settled it (`20261018-225649-corpse`, both roles byte-identical):

```
corpse params   n=8 {isIdle:Bool isDead:Trigger isAngry:Trigger moveSpeed:Float
                     isMining:Trigger isPickUp:Bool flying:Bool flyType:Int}
corpse baseline hp=145 alive=1 anim=Idle#1432961145 t=0,00 trueBools=[isIdle]
```

**The death pose is put on by a Trigger (`isDead`), not by a boolean** - so there is no latch to clear:
`SetBool` cannot clear a trigger, and the only boolean ever set is `isIdle`, the normal idle state that
the game re-writes every frame. A "clear every boolean" step was written first, ran, and was removed
again: it changed nothing and mutated unrelated state. The pose has to be reset at the **state** level,
and `Animator.Rebind()` + `Update(0f)` is exactly that:

```
corpse reset where=revive before={hp=100 alive=1 anim=Dying#-1546996312 trueBools=[isIdle]}
             step1(rebind)={hp=100 alive=1 anim=Idle#1432961145 t=0,00} fixedBy=rebind
corpse stable t=+1,0s hp=100 alive=1 anim=Idle#1432961145 dying=0 restores=1/1
```

The reset runs in **both** paths that bring a dwarf back - the mod's own revive (`CoopDowned.ReviveTick`)
and the game's self-heal (`downed: stood up on its own`), which produce the corpse equally - and
`BasePlayerAnimator.SetIdle(false)` is kept behind it as the handoff named it.

**Verified end to end, twice**, `-Test corpse -NoGod -TestQuit none` (`20261018-230341-corpse` and
`20261018-230838-corpse`), PASS on both roles both times:

```
host   RESULT: corpse PASS reason=restores=1 clean=1 stable=1/1 last='revive fixedBy=rebind' anim=Idle#1432961145
client RESULT: corpse PASS reason=restores=1 clean=1 stable=1/1 last='self-stand fixedBy=rebind' anim=Idle#1432961145
```

The second run had eight further revives and every one was reset the same way
(`corpse stable ... dying=0 restores=8/8`, `fixedBy=rebind` each time). `-TestQuit none` matters: the
launcher otherwise lets the first instance that finishes quit, and the peer then has no partner left to
drive the revive it is still waiting for.

**Two defects in the check itself, both found by the check failing for the wrong reason.** The first
version sampled the pose once at +1 s regardless of state and failed the client
(`20261018-225649-corpse`) - but that client had **re-died 0.7 s after a reset that had demonstrably
worked**, so `hp=0` with `Dying` was the game being right, not the bug. The sample now only counts while
the dwarf is **alive**, and an alive dwarf in `Dying` still fails immediately. The second is the one the
playbook warns about: a check that cannot fail proves nothing, so `-CorpseNoReset` runs the same test with
the reset deliberately left out. It reports the bug and FAILs (`20261018-231400-corpse`):

```
corpse reset  where=revive skip=flag(nreset) fixedBy=none
corpse stable alive-but-dying hp=100 alive=1 anim=Dying#-1546996312 t=2,15 restores=1/0
RESULT: corpse FAIL reason=restores=1 clean=0 stable=0/1 last='revive fixedBy=none(flag)' anim=Dying#-1546996312
```

**Honest gap:** neither passing run had a re-death land inside the 1 s window, so the alive-gate was not
exercised again after the run that motivated it; what the passing runs prove is that the gate cannot
produce a false PASS. And this fixed the *local* dwarf's pose - the puppet's animation is still not synced
at all (`CoopPuppet` drives only position), which is a separate item.

## Both down ends the run - and two ways the first attempts were wrong

Design decision: when both dwarves are down, the run ends the way it does in solo. A tester also supplied
the mechanism behind the game's *own* natural end, which is worth writing down because it explains every
unforced end in this session: **the first elite dies, the transport-capsule phase starts with a short
countdown (21 s in the observed case), and missing the capsule ends the run immediately** -
`MISSED_DROPPOD`. That is why runs ended on their own after ~4-5 minutes and why a run that never got
there never ended.

**Attempt 1 - only in the death patch.** The patch can ask "is my partner down right now", but the partner's
hp arrives a round trip later: measured, both dwarves died 40 ms apart and each still saw a live partner, so
both took the normal downed path and the run hung for minutes.

**Attempt 2 - in the tick, on the flags.** `IsDowned && RemoteDowned` fired while the dwarf was *up*: the
client's `IsDowned` stayed set while its own hp was 13 (`downed=1` in the tick log, hp=13 in the same line),
so the run was declared both-down off a stale flag. That also exposed the stale flag itself - the bot's
`Tick` returns early while `IsDowned` is set, so the dwarf had been standing still while alive.

**Attempt 3 - what is in place now.** One shared test, `CoopDowned.CannotRevive()`, reads the hp both sides
actually have (with the `everSawRemoteAlive` gate so the menu's hp=0 dummy is not a downed partner), and
*both* callers use it - the death patch (may the game's own game-over run?) and the tick (end the run) - so
the two cannot disagree about what "down" means. The tick also clears a flag that outlives its down, keyed
on hp, because `isAlive` is derived from hp. And the rule is gated on `ProbeState.Gc != null`: measured, an
ungated version fired in the menu (`selfHp=0 remoteHp=0 haveRemote=0`) and asked to end a run that does not
exist.

**Verified:** the menu false positive is gone with the gate, and the stale-flag clear fired repeatedly in
one run (`stood up on its own (hp=13), flag cleared`). The game's own game-over *does* run when the patch
lets it through - an earlier run ended for the client with `end screen setup condition=DIE` while its own
log read `both down ... remoteHp=0`.

**Not verified end to end, and a new finding while trying:** the game **stands a downed dwarf back up on its
own** - hp 0 to 13 within about a second, three times in one run, without the mod's revive path (its
`check: revived` line never appeared). So "both down" is a window of about one second rather than a stuck
state, and it is only terminal when both are at hp <= 0 long enough for the other side's hp to arrive. The
rule therefore stays conservative: it fires only when the run genuinely cannot continue, which is the safe
direction to be wrong in - but the run that verifies it end to end still has to be produced. What the game
does with a downed dwarf (self-recovery, and how that interacts with the mod's downed state) is the open
question here.

## The loan ends with the run, and the client keeps what it earned (build 21:56)

**Design decision** (asked, because the two rules contradicted each other): the client keeps its own earned
progress, and only the borrowed data stays borrowed. Nothing in I1's transfer and nothing in I3's block was
weakened to get there - instead the loan is now *given back* at the moment the run ends, so the game's own
save then writes a profile that is the client's plus everything it earned while playing.

- **Captured before the loan is applied**: `CaptureOwnBorrowed()` records the six lists, the meta levels and
  the gear as they stand, because after `ApplyPeerAdopt` they are no longer the client's.
- **Given back at the run end**: `EndLoan()` runs from the postfix of `GainRunEndRankXp` (after the credit,
  so the credit is part of what gets written) and from `UIEndScreen.Setup` as a backstop, because a run can
  end through `DIE`, `WIN`, `MISSED_DROPPOD` or `ABORT`. It is idempotent.
- **Per field, and "earned" is a subtraction, not a wildcard**: an id is dropped when the host lent it and
  the client did not have it (`host \ own`), so anything the client gained during the run - which is in
  neither - survives. Meta levels keep the client's own plus its rise above the borrowed level; gear items
  are matched by `HC` (the per-item hash, not `ID`, which is only the type) so a drop in the run is not
  mistaken for a lent instance.
- **Then** `BlockWrites` is released and the game's own `SaveDataManager.Save(true)` writes. I3 still blocks
  everything *during* the run, which is what keeps the loan out of the file mid-run.

**Lifetime bug hit on the first attempt, worth recording because it was invisible:** the snapshot was
cleared by `Reset("StartGame")`, but the loan is taken *in the menu*, before the run - so the run-end
log said `nothing to give back (own=False peer=True)` and the whole feature was a no-op that still *looked*
wired up. The loan's life now hangs off the adoption, not the run start.

**Verified at the file level, not from the mod's own log lines.** The save files are **JSON**
(`drg_save_slot{N}_{rotation}.dat`, `{"Version":9,...}`), so the runner now copies the used slots into the
run archive before restoring the player's saves, and the end state can simply be read:

```
client  check: loan ended why=rankXp wpn=29->0 cls=0 metaSum=90 gear=0

client slot after the run     UnlockedClassIds  weapons  meta levels  gear  classXp  credits
  drg_save_slot1_1/2.dat (*)  []                0        18 x 5       0     5958     141836
  drg_save_slot1_3.dat        []                0        18 x 5       0     5935     141818
  drg_save_slot1_0.dat        []                0        18 x 5       0     5956     141834
```
(*) the newest write (`Timestamp` 214523795, matching the run).

- **The borrowed half is absent**: the host lent 4 classes, 29 weapons, 18 meta entries at level 0 and 6
  gear items; the client's file has none of them.
- **The earned half is present**: the file's class XP total is 5958, and the client's own log measured
  `classXp 5956 -> 5958` for `perfXp=2`. The credits moved with it (141834 -> 141836). That is the thing
  that used to be computed and then thrown away.

**Not yet exercised by the fixture, and labelled as such:** the "earned beyond the loan" rules need
something earned during a co-op run - a weapon or gear item dropped on the client's side - and the client's
world has its spawner suppressed, so neither the set difference nor the `HC` subtraction has been seen to
fire in a run yet.

## I2, first measurement: what a run end does - and two harness defects found on the way

The question for I2 is what a run end turns into permanent progress, because that is where I1's borrowed
profile and the client's own profile would collide. The entry point is declared rather than guessed, and
the credit is direct:

```
host  runend: GainRunEndRankXp(161) BEFORE classXp=5956 playerRank=19 classLevels=58 runStartRank=DRILLER:14/3155
host  runend: GainRunEndRankXp AFTER  classXp=6117 playerRank=19 classLevels=58
host  runend: end screen setup condition=MISSED_DROPPOD
```

`PlayerRankManager.GainRunEndRankXp(int performanceXp)` is public, and the class XP total moved by exactly
its argument (5956 + 161 = 6117). The end screen then reports the condition. Around it, from the
*declarations* (bodies are stubs, so this is what exists, not what was called):

- `ScoreManager` turns the run into a score with explicit weights - `GOLD=5`, `NITRA=7`,
  `CRAFTING_MATERIAL=15`, `RED_SUGAR=3`, `SECONDARY_OBJECTIVE=500`, `PLAYER_LEVEL=100`, `ARTIFACT=1000`,
  `WIN_MOD=1.25`, plus a hazard table - and raises `OnFinalScore`.
- `UIEndScreen.ShowProgression(int rankXpEarned, int creditsEarned)` is the display side of the two numbers.
- Mastery has four writers: `SaveDataManager.TrySetWeaponMastery`, `TrySetClassModMastery`,
  `TrySetBiomeMastery`, `TrySetMutatorMastery`.
- Milestones go through `MilestoneTracker.GiveMilestoneReward(MilestoneData, doSave)` and
  `MilestoneSaveUpdate.NewlyCompletedMilestones`.

**Defect 1 - the rescue aid was permanent.** The bot's rescue branch lifts its own hp to 100000 so the
rescuer survives the 5 s revive window. The game does *not* clamp `currentHp` to the dwarf's max hp, so
that 100000 simply stayed: measured, a client whose rescue branch fired once sat at ~99900 for the rest of
the run taking 10 damage per hit, and **its run could no longer end by death at all** while the host's did.
That is a harness aid silently deleting a whole outcome from every test that touches it. Fixed: the pre-pin
hp is captured and given back when the rescue branch ends (`bot: rescue pin released, hp back to 63`).

**Defect 2 - both dwarves down at once wedges the run.** With `-NoGod`, both went down within seconds of
each other and stayed down: `hp=0 alive=False` for 139 host ticks and 119 client ticks (~3.5 minutes) with
no `UIEndScreen.Setup` and no credit. Nothing can revive anyone - there is no live partner - and the mod's
downed state holds the game away from its game over. A solo run would have ended. This belongs to the
downed/revive item (F), not to I2, but it is recorded here because it silently removes the run end from any
test that dies twice, and because "both down" needs a decided behaviour anyway.

**The reliable way to reach a run end in this harness** is to leave god mode on and let the mission run to
its own outro, which the autopilot already watches:

```
auto: watch: active=CoreOutro      <- the mission is ending (drop pod phase)
runend: GainRunEndRankXp(161) ...
auto: watch: active=UIEndScreen
```

**Seen once and worth knowing (F, not I2):** the downed/revive path demonstrably ran in the same session -
`check: downed remote role=client (remote hp=0)`, `check: rescue started role=client dist=13,3`,
`check: revived role=host hp=100 alive=True`. Noted as an observation; F's own test is still open.

**Still open:** mastery and milestone writes need a run that actually plays (the trigger below ends one on
demand, but the run it ends has nothing to complete).

**A run end can be forced, and that is what made the rest measurable.** Waiting for a mission to end by
itself is not a test: of four attempts, one reached the outro, one wedged with both dwarves down, and one
had not ended after 9 minutes. `CoreGameEvents.TriggerCoreRunOutro(ECoreRunEndCondition)` is public and
static, and it is the same door the mission's own end goes through, so `-cooprunendforce=<win|die|abort>`
runs the real chain on demand. Whole measurement: ~90 seconds instead of ~9 minutes, both sides, twice.

```
host    runend: forcing condition=DIE via CoreGameEvents.TriggerCoreRunOutro
host    GainRunEndRankXp(2)  BEFORE classXp=5956  AFTER classXp=5958   end screen condition=DIE    writesBlocked=0
client  GainRunEndRankXp(3)  BEFORE classXp=5956  AFTER classXp=5959   end screen condition=DIE    writesBlocked=0

host    GainRunEndRankXp(17) classXp 5956->5973   condition=WIN   stages=1
client  GainRunEndRankXp(18) classXp 5956->5974   condition=WIN   stages=1
```

**Each side credits its own profile with its own performance** (`runStartRank` names the class each one
actually played: host DRILLER, client ENGINEER). The `WIN` path behaves the same as `DIE` in this respect,
so the credit is not conditioned on winning.

Milestones stood still at 78/281 in every *forced* run, including the wins - but a forced win 20 seconds
into a run has no stage progression, no hazard and no milestones to complete, so that is a limit of the
fixture and **not** evidence that milestones are unaffected. A *real* run end (god mode off, 592 kills) did
move them: `milestones 78/281 -> 79/282`, so the milestone write path is live and writes at the run end.

> **Corrected later** ("A run that actually plays", below). Counting only *completions* hid the thing that
> moves constantly: the summed `Progress` of the same list advances by half a million in a three-minute run,
> per instance and in proportion to that instance's own kills. And a forced run end *does* reach the
> milestone completion path - what it does not reach unattended is the end screen, because a won run puts up
> a gear-found popup that nothing dismissed. So the fixture limit above was real but localised, and the
> "writes at the run end" sentence was right for the wrong reason.

**The collision I2 exists to find, measured.** With adoption on and the same forced end:

```
client  check: profile-scrub ... after={0 0 0 0 0 0} metaSum=90
client  check: profile-adopt ... applied {cls=4 mod=9 wpn=29 art=13 oc=0 sets=1}
client  save: blocking the client's writes (RequestSave) - adopted unlocks are in memory only
client  runend: GainRunEndRankXp(3) BEFORE classXp=5956 AFTER classXp=5959  writesBlocked=2
client  runend: end screen setup condition=DIE
```

The client plays the run, the game computes and applies its rank XP, and then I3's write block throws the
whole thing away: the client's own earned progress never reaches its file, because the block is on the save
request rather than on the borrowed data inside it. That is a design decision, not a bug in either half -
I3 is right that borrowed unlocks must not become permanent, and I2 is right that the game credits the run -
but the two rules now contradict each other at exactly this point.

## Gear transport works, and the cache needed rebuilding (build 21:03)

The third piece of I1: the client's *inventory*, not just its unlocks and its strength. Gear is the first
thing in this mod that is **structured** rather than a list of strings, and that changed the shape of the
work in two places.

**1. Gear cannot travel as live objects.** `GearSaveData` has twelve members, three of them arrays
(`S`, `SG`, `ST`); `GearEquipSaveData` has six arrays. The runtime dump settled what they actually are:

```
GearSaveData      props={ HC:Int32 ID:String R:Int32 L:Int32 U:Int32 N:Boolean
                          S:Il2CppStructArray`1 SG:Il2CppStructArray`1 ST:Il2CppStructArray`1
                          RQ:Int32 LQ:Int32 F:Int32 ... }
GearEquipSaveData props={ ID:String A,C,G,Ta,To,W:Il2CppStructArray`1 ... }
```

So everything is a *property* (the interop rule again) and every `int[]` is an `Il2CppStructArray<int>`.
Putting a `GearSaveData` on the wire would mean both sides agreeing on a runtime detail; instead the mod
carries wire-shaped items (`GearItem`/`EquipItem`, plain ints and arrays, arrays as count + values), and
the client rebuilds game objects from them - arrays built by indexed fill, properties written through the
same generic accessor used for reading.

**2. The save is not what the game shows.** `GearManager` holds `AllGear` (`List<GearView>`), and measured
at the handshake it *mirrors the save exactly* - host `allGear=6 saveGear=6`, client `allGear=0 saveGear=0`
- which is what a load-time cache looks like. Replacing `SaveData.GearSaveData` and stopping there would
have left the inventory screen showing the client's own items while the save said otherwise.

The rebuild goes through the game's own loader, and the log was arranged to say *which* half of that call
does the work, because the decompiled bodies are stubs and neither half can be read off them:

```
client  RESULT: profile-adopt PASS ... {gear 0->6 hostGear=6 collected=6 glvl=18 load=3
                                        manager=0->6 loaderFilled=0} hostGear=6 loadedGear=6
```

**`loaderFilled=0` is the finding**: `SaveDataManager.LoadGear(GearManager)` *returns* the rebuilt list
(6 items) but does **not** store it in the `AllGear` field the UI reads. The mod assigns it. Had I assumed
"the loader loads", the inventory would have stayed empty and the save would have looked perfectly
adopted - the same shape of bug as the meta wallet, in a different subsystem.

The counters were written and read back (`collected=6 glvl=18 load=3`), so those are not "no exception, so
presumably fine" but a value that is demonstrably in the save afterwards.

**Gap, stated rather than implied: the loadout half is unverified.** `EquippedGear` is transported and
rebuilt, but *both* profiles have nothing equipped (`saveEquipped=0` on both, and the host's own
`equippedList=15` with an empty save list is consistent with "15 slot records, none of them filled"). A
fixture with an equipped loadout would be needed to show that the client's equip slots follow - which means
driving the gear UI to equip something on the host first.

## Player rank: measured, and deliberately not adopted

The last item of I1 turned out not to be worth building, and the evidence is in the settings rather than in
a run:

- `PlayerRankSettings` has one member: `ClassRanksPerPlayerRank`. `ClassRankSettings` has one: `RankXP`.
- So `Rank` is derived per class from `Xp` via XP thresholds, and `PlayerRank` is an *aggregate* of the
  class ranks. Neither adds a stat mod, a currency or an item.

Rank drives milestones, the progression screens and `UIEndScreen`'s "XP gained" display - not the run's
power. A low-rank client is therefore not a weaker player, which is the only reason I1 exists. And since
rank *is* progress, adopting it would actively work against I3 (the client's own progress must stay its
own) once the run-end credit (I2) lands.

There is a second, mechanical reason not to touch it: `PlayerRankManager` caches the ranks it read at load
(`ranks`, `dataLoaded`, `PlayerRank`, `TotalClassLevels`) - the same cache-versus-save split as gear, but
with nothing to gain from bridging it.

## Meta levels, and the cached values they feed (build 20:45)

The second half of I1: not just what the client may *pick*, but how strong it is. Meta upgrade levels live
in `SaveData.MetaStatUpgrades` as `{Id, Level}` pairs, so they travel as pairs and are rebuilt with the
game's own constructor - the same verbatim rule as the id lists.

**The part that would have quietly not worked.** `MetaManager` does not read the levels when the run needs
them: it computes `StartingGold`, `StartingNitra` and `ArtifactRerolls` into cached fields, and the only
call to `UpdateWalletUpgrades()` is inside `MetaManager` itself. Replacing the levels and stopping there
would leave the list looking perfectly adopted and the run using the old numbers - a change that appears
to work and does not. So the mod calls `UpdateWalletUpgrades()` after applying, the game's own refresh
rather than a reimplementation of it.

**Measured, including the derived values:**

```
client  check: profile-scrub role=client ... dwarf=00000000 metaSum=90
client  check: profile-adopt role=client received {cls=4 mod=9 wpn=29 art=13 oc=0 sets=1}
                                        meta=18 metaSum=0 gold=0 rerolls=0
client  RESULT: profile-adopt PASS ... meta 90->0 hostMetaSum=0 gold 10->0 hostGold=0 goldMoved=1
host    RESULT: profile-adopt PASS reason=peerClasses=0 deficit=1 adoptSent=2
```

`gold 10->0`: with the scrubbed level 5 the game's own wallet computation produced 10 starting gold, and
after adopting the host's level 0 levels and refreshing it produced the host's 0. The adoption reaches the
game's derived state, not only its storage.

**The fixture had nothing to measure - the third time this pattern bit.** The host's 18 meta entries are
*all at level 0*, so `0 -> 0` reads identically whether the adoption happened or never touched anything.
The scrub therefore sets the client's levels to a distinctive 5 instead of 0: its job is to make the
client's profile *different* from the host's, not poorer. The verdict compares sums, so an untouched
profile cannot pass - and `goldMoved` is reported explicitly, so a wallet check that was skipped for lack
of a difference is visible rather than silently green.

**Still open in I1:** gear inventory/loadout and player rank. Then I2, crediting the client's progress back
to the host at run end.

## A run that actually plays: milestones complete at the end screen, and mastery is empty (build of 23:45)

I2's last open item was the one fixture that could not answer it: the forced end ran the run-end chain 20
seconds into a mission, so there were no stages, no hazard and nothing to complete. The fixture now exists
- `-RunEndDelay 180` plays for three minutes and *then* ends through `CoreGameEvents.TriggerCoreRunOutro`,
so the run has kills, levels and a stage behind it. Extending the probe to the number that had been
invisible turned three separate "nothing happened" readings into results.

**Milestones: the completed count was the wrong thing to watch.** `MileStoneStates` entries are
`{Guid, Progress, IsCompleted}` - read out of the save file, which is JSON - and the probe only counted
completions. So `78/281 -> 78/281` in every earlier run read as "the write path is dead" when it only meant
"nothing crossed its threshold yet". Summed over the list, a three-minute run moves it by half a million:

```
host    milestonesStart=78/281 p=2452142  -> 78/281 p=3022974  -> end screen: 82/284 p=3022983   kills=456
client  milestonesStart=78/281 p=2452142  -> 78/281 p=2719156  -> end screen: 78/281 p=2719156   kills=221
```

Three facts in one line, none of them visible before:

- **Progress is written during the run**, per instance, and it is proportional to that instance's *own*
  play: +570 832 on the host against +267 014 on the client, with 456 against 221 kills (ratio 2.14 against
  2.06). The same shape in the run before it (+543 036 / +266 941 against 457 / 210 kills). Each side
  advances its own copy of the profile, which is exactly what the later "the client keeps what it earned"
  decision assumes.
- **The credit itself moves nothing** (`p=3022974 -> p=3022974`), so the run-end credit is not where
  milestone progress comes from.
- **The completion path runs at the end screen, and only there**: the host went `78/281 -> 82/284`, i.e. four
  milestones completed *and* three new entries appeared in the list. That is `PostEvaluateMilestones` /
  `GiveMilestoneReward` doing their work - the path that every earlier run reported as inactive because the
  end screen never ran.

**Mastery is empty on this profile, and the save file says so.** After a three-minute run with 445 kills:

```
"WeaponSaveData":[],"BiomeSaveData":[],"MutatorSaveData":[],"ClassModSaveData":[]
```

That is a played slot read straight out of a backup, not a runtime misread - all four mastery
lists are empty and a played run creates no entry in any of them. It matches the profile's run-type page,
where only the campaign Mission Road is unlocked and **Mastery is `[unavailable]`**. So the plan's "mastery
write path" item is **blocked on a fixture, not open**: it needs the Mastery mode unlocked (or a profile
that has played it), and no amount of code on this profile will produce a number to read.

**The blocker between the credit and the end screen is a popup nobody dismissed.** A won run puts up
`UIGearFoundForm` before the end screen, and the autopilot - which already learned to serve `UILevelUpForm`
- had no case for it, so the host sat on the gear popup while the client (which won no gear) sailed through:

```
host  runend: GainRunEndRankXp AFTER  ... 23:44:44.660
host  phase: gearfound-open                23:44:46.024
host  runend: end screen setup condition=WIN milestones=82/284   23:44:46.058   <- 1.4 s after the credit
```

`UIGearFoundForm.OnContinueButton()` is the button's own handler, so the autopilot now calls it, the same
way it calls `UILevelUpForm.OnSkillChosen`. With that, both roles report `endScreens=1 condition=WIN`.

**And a verdict that described a chain that had not finished.** The `runend` verdict is written from the
credit postfix, which is *before* the end screen - so it printed `endScreens=0 condition=none` and empty
end-screen reads (`->?`) about a chain that was still running. That was read as "the forced end never
reaches the end screen" and was simply wrong: in the run where the host's end screen did not appear, the
gear popup above is why, not the force. The verdict is now deferred a tick or two (15 s grace, so a run
that genuinely never reaches an end screen still gets a verdict) and carries the real values.

**Correction to the earlier section:** "milestones 78/281 -> 79/282 in a real run" is consistent with this,
and the sentence next to it - that a forced end "has no stage progression, no hazard and no milestones to
complete" - was only half right. A forced end *does* run the completion path; what it cannot do is reach
the end screen unattended, because of the gear popup.

**What is still not exercised:** the client completed no milestones in these runs (its own progress is
lower), so "both sides complete their own" is inferred from the host's result plus the per-instance
progress, not measured on the client. And mastery remains unmeasurable here.

## The partner's dwarf: the puppet now plays the pose the remote is actually in (build 00:01)

The other half of the "corpse" report, and the half the earlier fix did not touch: the local dwarf's
pose is repaired, but the *partner* sees a `CoopPuppet` that was driven from position and rotation only.
A downed dwarf stops moving, so the puppet came out as `Idle` - a dwarf standing there where the partner's
screen should show a corpse.

**Measured first, and the measurement changed the plan.** The obvious fix is to drive the puppet's
`Animator`, so the first question was whether the pose lives there. Reading the player tree off a real run
(20261018-234503) settled it: **not one `SkinnedMeshRenderer` anywhere** (`[m/0]` on every node, 8 renderers
in `GFXContainer`), and the mesh sets do not change between alive and down:

```
puppet: view local=alive ... localMesh=[6:,TankBody_Model,Cube,Cube,Quad_1x1m,GraplingHook]
puppet: view local=down  ... localMesh=[6:,TankBody_Model,Cube,Cube,Quad_1x1m,GraplingHook]
```

So two candidate layers were wrong before writing anything: it is neither a skinned rig nor a mesh swap of
the body. What *does* differ is the animator state (`Idle#1432961145` vs `Dying#-1546996312`), and the puppet
already followed that for locomotion (`Idle`/`Running`), so the pose is carried by **transform animation on
the body-part hierarchy** - which is exactly what the `Animator` drives. That made the animator the right
layer, but it also ruled out the cheap version of the fix: *deriving* down/dead from hp means guessing a
parameter name, and the earlier measurement (20261018-235525) showed the guess landing on `Idle` for a downed
remote.

**So the state travels instead of being guessed.** New packet `AnimState` (its own type, not a field on
`PlayerState`: a peer on an older build would read `PlayerState` short and desync every packet after it,
while an unknown type is simply ignored). It carries the local player's `GetCurrentAnimatorStateInfo(0)
.fullPathHash`, sent when it changes plus once a second, because the channel is unreliable and a lost
"I am dying" would leave the partner's puppet standing on a corpse until the next state change. The puppet
plays it:

```
puppet: played remote anim Idle#1432961145            <- and Running#256545655 while walking
puppet: mirror remote=down puppet={Idle#1432961145}   <- the remote is a corpse, the puppet is standing
puppet: played remote anim Dying#-1546996312          <- 100 ms later the pose is on it
puppet: sample down +1,0s dying=1 puppet={Dying#-1546996312} remoteAnim=-1546996312
puppet: played remote anim Idle#1432961145
puppet: sample up +1,0s dying=0
RESULT: puppet PASS reason=down=1/1 up=1/1            <- both roles
```

`-Test puppet` (`-NoGod -TestQuit none`, the same fixture as `corpse`) samples **1 s after each flip of the
remote's life state** and counts clean or not - the shape the `corpse` check already uses, so the verdict is a
ratio and not an opinion. It needs **both** halves: a puppet stuck in `Dying` passes the first and fails the
second, which is the same bug from the other end.

**Negative control is a real run, not a flag.** Before the puppet played the remote's state
(20261018-235525) the same sample would have read `dying=0` with the remote down - the log line is in the run
above (`puppet: view local=alive remote=down ... puppetAnim=Idle#1432961145`). So the check is not blind; it
was failing an hour before it passed.

The parameter-derived drive in `CoopPuppet.UpdateAnimation` stays, but only as the **fallback** for a peer
that has never sent a state: it is a guess (it maps a derived speed onto whichever names look like movement)
and it would otherwise fight the real answer by transitioning the state machine away from it.

**What this does not claim:** the verdict is about the *pose state*, not pixels. The puppet now plays the
same animator state the partner's dwarf plays, on a clone with the same meshes and the same controller -
that the death pose then reads as a corpse on screen is inferred from that, and was not verified with a
screenshot. The samples are also thin: one down/up pair per instance per run, same convention as `corpse`.

### The walk cycle now follows the same phase (build 22:31)

`AnimState` carried only `fullPathHash`, so the puppet played the right state from its **own** clip
time: the pose was right, the two walk cycles were not in step. `AnimPhase` (packet 28, unreliable,
~10 Hz) adds `normalizedTime`; it repeats the state hash so a lost `AnimState` is also recovered
from a phase packet. The puppet uses `Animator.Play(hash, 0, phase)` when the state changes and,
for the same state, only corrects when the phase error exceeds 0.1 - playing the state every packet
would reset the pose ten times a second.

- `-Test animphase` on the walking bot: **PASS both roles**, `good=15/15 moved=1`
  (`20261019-223142-animphase`). Host `lastErr=0.08 corrections=0`, client `lastErr=0.00
  corrections=12`. `moved=1` means the remote phase changed between samples, so this is not the
  constant-Idle trivial pass.
- Negative `-AnimPhaseOff` (`-coopanimphaseoff`, `20261019-223338-animphase`): both sides FAIL on
  state (`phase=off (negative control)`), not by test timeout.
- Still open: the correction is discrete (10 Hz + 0.1 threshold), so a very fast state change can
  show a short mismatch before the next packet. Weapon/equipment animations, Bosco/turret
  animations and enemy animator state remain unsynced.

## The loot value was never wrong - the plan was (build 00:37)

The plan carried an open item since the stage-2 loot work: *"the broadcast value reads 1, because
`Pickup.OnSpawn` fires before the game sets the value; hook `set_value` or read one frame later -
cosmetic"*. It has read one frame later for a long time (the `SpawnedQueue` drain), so the honest question
was whether that deferral buys anything. Measuring it turned the item inside out.

**The deferral buys nothing, and there was nothing to buy.** Both fields, at spawn *and* one tick later, over
130 samples:

```
loot: send read=1 atSpawn=1 differs=0 type=CURRENCY Value=1 value=1
check: loot value= send={sent n=50 min=1 max=1 sum=50 differs=0} ...
```

`Value` is a getter *method* on this build and `value` the raw field, so both were read. Neither ever
changes, on the fixture's own drops or on the natural mining drops mixed in.

**Why 1 is the truth, two independent ways:**

- **The API says so.** `SpawnMaterial(ECurrency material, int count, Vector3 position, float forceMod)` -
  the parameter is a **`count`**, not an amount. `SpawnMaterial(NITRA, 5, …)` makes *five one-unit pickups*,
  and the host broadcast exactly five per drop. There is no "real value applied afterwards" for a material
  pickup; one pickup is one unit.
- **The game's own wallet says so.** The host claims its own drops, so its wallet is the *game's* accounting,
  and it rises 1:1 with claims:

```
check: loot value= send={sent n=10 ...} wallet={Nitra=10} pickups=10 claims=10
check: loot value= send={sent n=50 ...} wallet={Nitra=50} pickups=50 claims=50
check: loot value= send={sent n=80 ...} wallet={Nitra=80} pickups=80 claims=80
```

**So the plan's PASS criterion was unsatisfiable.** It asked for `value != 1`, which is the one thing this
build can never produce - the item would have sent the next agent to "fix" correct behaviour, and any check
written to it could only ever fail. The `loot` check now asserts the invariant that actually has to hold -
one pickup carries one unit and the sum matches the count (`min == max == 1`, `sum == n`), on both halves of
the pipe - and PASSes on both roles (`20261019-003708-loot`).

**A flag that controlled nothing, removed.** A `-cooplootvaloff` switch was written to put the old
spawn-time read back for a negative control, and then deleted: it selects between two numbers that are
*always* equal (`differs=0`), so it cannot make the check fail, and a control that cannot fail is not a
control. The evidence for that is the `differs=0` column, not an argument.

**Two harness notes from the three runs it took:** the check fired after five broadcasts and the host quit
(the default `-TestQuit both`), so the client - which had mirrored nothing yet - lost its partner and timed
out; this check needs `-TestQuit none` like `corpse` does. And the wallet figures in the *verdict* are read
at that instant (`hostWallet={Nitra=0} hostClaims=0` in the PASS reason), so the 1:1 series above is the
thing to read, from the periodic log line, not the verdict.

## The shop between stages is reachable, and the currency now runs both ways (build 01:52)

Milestone D had one half left: the level-up cards are done, the post-stage shop was not. It is now
reachable on purpose and its economy is measured - and the design question was settled in the middle
of it.

**A stage end can be forced, and that is what opens the shop.** Waiting for a stage to end by itself is the
same kind of maybe the run end was (`~9 minutes, or never`), and the game has the same kind of door:
`CoreGameEvents.TriggerStageEnd(CoreLevelEndArgs)` is public and static, and the args carry the same
`ECoreRunEndCondition` the run end uses. New test aid `-StageEnd win`, and the shop appears **~20 ms later on
both instances**:

```
shop: forcing stage end condition=WIN via CoreGameEvents.TriggerStageEnd role=host
shop: show #1 baseReroll=5 reroll=0/0 heal=30/0 wallet={GOLD=0 ... NITRA=35 ...} role=host
shop: caller of Show = ... < CoreGameEvents.TriggerStageEnd < CoopShop.Tick < CoopTick.Update < ...
```

Both sides show the **same prices** (`baseReroll=5 reroll=0/0 heal=30/0`), and a purchase is local and
personal: the game's own chain runs `Show -> SelectBestButton -> OnButtonSelect -> OnPurchaseComplete`, the
two instances bought *different* items (`WeaponUpgradeSkill_Damage` vs `SupportSkill_Armor`), each paid from
its own wallet. **The mod presses nothing in the shop** - all six shop hooks are listeners.

**The design decision, because it is not what the code did:** each player has their **own shop** (the
rewards are personal - a weapon upgrade improves *that* player's weapon), but the **currency is shared**:
claims run in both directions and both sides always credit, so the two wallets stay identical. Plus a shop
barrier like the level-up one - **built and measured further down**.

**What the code actually did, in three places, all now fixed:**

- `ClaimPostfix` was **host-only**, so a client that collected a mirrored pickup told nobody. The host kept
  the original (the same loot, collectable twice) and its wallet never moved for it.
- The claim queue was drained **inside the client-only block** of `CoopLoot.Tick`, so `ApplyClaim` never ran
  on the host at all. Measured: the client sent 3 claims and the host counted 0 received.
- `CreditWallet` **skipped the currencies the game saves** (`Wallet.ShouldSaveValue`), which left the claimer
  the only one paid for persistent currency - drift in exactly the currencies a shop spends.

Now: both roles broadcast claims, both roles drain them, the receiving side removes its *own* copy (the
host's original, the client's mirror) and credits it, and the gate is gone rather than inverted. The fixture
drops nitra **and** gold on the *client's* dwarf (`-TestLoot`, with `-Still` so neither dwarf wanders off
it), which exercises the direction that used to be silent:

```
host:   claimsSent=7 claimsRecv=2 miss=0 credited=2 walletDelta={GOLD=+3 NITRA=+6}
client: claimsSent=2 claimsRecv=7 miss=0 credited=7 walletDelta={GOLD=+3 NITRA=+6}
```

The directions mirror each other exactly (7/2 against 2/7), every received claim found its local copy
(`miss=0`), every one credited once (`credited == claimsRecv`) - and **the two wallets rose by the same
amount**, which is the thing requested. PASS on both roles, twice.

**Two defects found on the way, both of the silent kind this project keeps paying for:**

- The claim broadcast sat in a bare `catch { }`. A run showed the client counting 7 claims with
  `claimsSent=0`: the broadcast was failing and nothing said so. It now logs once - a swallowed exception in
  a code path whose whole job is to send is indistinguishable from "nothing happened".
- The client's `MirrorList` kept entries for mirrors the *client itself* had claimed (the game pools the
  object, the list did not care). That is the symmetric counterpart of the host's `ForgetOrigin`, and with
  every pickup worth 1 in this fixture the resulting wrong FIFO match would be invisible - in the real game
  it would credit a value that belongs to loot nobody collected. Both sides now forget what they claimed.

## A stage end reached nobody - the client played stage 1 forever (build 02:28)

The shop barrier left a question behind that turned out to be the bigger one. In its fixture the two shops
opened ~14 s apart because **each** instance forced its own stage end - which only works because a test aid
did it. A real run has no such aid, so: when one player's stage ends, does the other's?

**Measured first, with the client unable to force anything** (`-StageEndHostOnly`, so the host's stage end is
the only one there is):

```
host   02:18:06  check: stage starts=1 endsGame=0 endsAid=1 forced=1
host   02:18:51  check: stage starts=2 endsGame=0 endsAid=1 forced=1     <- host is in stage 2
client 02:21:54  check: stage starts=1 endsGame=0 endsAid=0 forced=0     <- 300 s later, still stage 1
host   RESULT: stage PASS reason=starts=2 …        client   RESULT: stage FAIL reason=timeout after 300s
```

**A stage end does not propagate at all.** The client sat in stage 1 for the whole run. The objectives are
mirrored (`CoopObjective`, milestone C) and that was never enough: mirroring the *progress* does not end the
other instance's stage. The host's 45 s shop-barrier timeout is the only reason it did not wedge there - a
single-run fixture hiding a two-player break.

**Finding the signal took one wrong turn worth recording:** `GameController.stageIndex` looks like the obvious
read-out and does not compile - in the dump it belongs to a compiler-generated nested iterator class
(`__4__this`, `__2__current` right above it), not to `GameController`. What works is the door this file
already used: a postfix on `CoreGameEvents.TriggerStageEnd`, which fires on whichever instance's stage ends,
with the source counted separately (`aid` / `game` / `remote`). That distinction is the entire measurement -
the forced end and a real one are the same call.

**The fix** is a `StageEnd` packet (22) carrying `CoreLevelEndArgs.EndCondition`, host -> client. The client
raises the *same* door locally, with `ApplyingRemote` set so it never echoes the packet back. Reusing
`TriggerStageEnd` means the client runs its own real stage-end chain - shop, next stage - rather than
anything invented. Measured after the relay:

```
client 02:25:29.234  stage: peer ended its stage (condition=0, recv #1) -> raising it locally
client 02:25:29.251  shop: show #1 …                     <- its shop, 17 ms after the host's stage end (was 14 s)
client 02:25:30.348  shop: released after holding 0,5s    <- the barrier held 0.5 s, not the 45 s timeout
client 02:25:31.145  GameController.StartGame             <- stage 2
host   RESULT: stage PASS reason=starts=2 endsAid=1 peerShopDone=1 sent=1
client RESULT: stage PASS reason=starts=2 endsRemote=1 recv=1
```

Two things fall out of that: the shops now open together, and the barrier's hold dropped from the 45 s
emergency exit to 0.5 s of real handshake. The barrier was working all along; the split stages were hiding it.

**The negative control** (`-StageEndNoRelay`, the host ends its stage and says nothing) is red on *both*
sides, and each side's reason is different on purpose:

```
host   FAIL reason=starts=2 endsAid=1 sent=0 peerShopDone=0                                <- "I got there, alone"
client FAIL reason=no new stage 120s after the end: starts=1 ends=0/0/0 recv=0             <- the diagnosis
```

That shape comes from one rule the check follows: **each side must show evidence from the peer, and a
timeout is not evidence.** The host reaching stage 2 by itself is exactly what the broken run did, so its
criterion is the peer's `ShopDone` - which only exists if the peer had a shop - rather than "I advanced". The
client's criterion is `endsRemote`, since in this fixture it can only get a stage end from the other side.
And because the side that hears nothing has no end to measure from, the client's clock also starts at its own
stage start; without that anchor the red side could only have reported a bare "timeout after 300s", which is
the unhelpful verdict this project keeps running into.

**Two transitions, because one cannot fail.** Both halves of the chain carry re-arm logic - the barrier
resets when a shop opens and the peer's `done` flag is *consumed* when this side leaves - so a single
transition measures only the case that works even if neither re-arms. The aid now repeats
(`-StageEndRepeat`, the clock restarting after each fire so the next end lands after the shop and the new
stage):

```
02:44:43.232 client shop: show #1 …                    -> released after holding 0,8s
02:44:44.981 client GameController.StartGame            <- stage 2
02:45:03.718 client shop: show #2 … (reroll=5/0)        <- the second shop, re-armed
02:45:03.727 client stage: end #2 (from the peer)
02:45:06.258 client GameController.StartGame            <- stage 3

host   PASS reason=starts=3 want=3 endsAid=2 peerShopDone=2 sent=2 forced=2
client PASS reason=starts=3 want=3 endsRemote=2 recv=2
```

Both re-arm paths hold, and the relay fires twice with no echo (`endsRemote=2` on the client, `sent=0`).
The first run of this fixture was a **false green on the client**: I had put `-coopstageendrepeat` inside the
launcher's host-only guard, so the client kept the default of 1, checked for *one* transition and wrote its
verdict after the first one - the second transition was in the log and in nobody's verdict. The count is an
expectation, not a force, and it now sits outside that guard. Worth generalising: **a parameter that decides
what a check expects does not belong in a block that decides what one role is allowed to do.**

**Limits, stated rather than implied:** the relay is host -> client only, because the host is the authority
for the stage (the client's objectives are mirrors). A stage end the client raises by itself is logged as
`stage: end #N (game's own)` and *not* relayed - if that turns out to be reachable, that is the place to
extend. Two transitions are measured, both forced; a stage that ends *by itself* (objective complete) was
never observed on either side, and how the chain behaves when both sides end a stage within the same second
is untested.

## The shop barrier: built, and the two bugs it took (build 02:09)

It was requested ("Ja, Barriere wie beim Level-up"), and it is in: `UIShopScreen.OnPlayButton` is held
with a `false`-returning prefix until the peer reports done, and released by re-invoking that same method -
the shape `CoopXp` already uses on `LevelUpManager.OnLevelUpFinished`. The message is a new packet
(`ShopDone = 21`), its own type for the same reason `AnimState` has one: an older build ignores an unknown
type instead of desyncing on a longer packet.

```
green  host:   show=1 play=2 holds=1 heldFor=15,6 releases=1 timedOut=0 remoteDone=1 sent=1 left=stage-start screenActive=0   PASS
       client: show=1 play=1 holds=0 heldFor=0,0  releases=0 timedOut=0 remoteDone=1 sent=1 left=stage-start screenActive=0   PASS
red    both:   show=1 play=1 holds=0 timedOut=0 remoteDone=0 sent=0                                                 (barrier off)
```

The red run is the negative control (`-ShopNoBarrier`): same fixture, every continue goes straight through,
`remoteDone=0` - so the check distinguishes a barrier that talked from one that did not.

**The autopilot had to learn to press it first.** Nothing has ever closed a shop in an automated run: the
first room that reached one left it open, and a barrier with no way out of the shop could not be measured at
all. `CoopAuto` now presses the screen's own `OnPlayButton`, which is also exactly the call the barrier holds,
so an automated press and a player's press go through the same logic.

**Bug 1, and the reason the first check could not have worked: `UIShopScreen.Hide` is never called.** The
first barrier run held and released *correctly* - host pressed at `02:05:45.886`, held, got the client's done
at `02:06:01.126`, released after 15,2 s - and the run carried on (`GameController.StartGame` 0.6 s later),
while `hide` stayed 0. The check waited on `hide` and timed out on both sides. The observed way out of a shop
is the **next stage starting**, which now triggers the verdict, and the screen's own active state is recorded
at that moment (`left=stage-start screenActive=0`) so that claim is evidence rather than belief.

**Bug 2, mine: `OnRemoteShopDone` never set `_remoteDone`.** The message was counted, logged, and acted on -
but only in the branch for "I am already holding". So it worked on whichever side happened to press first and
was invisible on the other. Measured signature: the client held for exactly `heldFor=45,0` (the safety
timeout) while the host looked perfect, and the peer's `ShopDone` had arrived *13 s before the client's shop
even opened*:

```
02:05:45.895 client: shop: remote done (localDone=False holding=False)   <- arrives with no shop open yet
02:06:00.238 client: shop: show #1 ...
02:06:01.105 client: shop: local done, holding until the peer leaves the shop
02:06:46.117 client: shop: hold timed out after 45s -> releasing
```

That is not a corrupt corner case, it is the normal shape of a stage-transition gap: the two shops do **not**
open together. This fixture forces the stage end independently on each instance (each has its own 30 s timer
after its own run start, and the run starts were ~14 s apart), so the host had shopped, left, and told the
world before the client's shop existed. **A `ShopDone` that arrives before this side's shop opens means "my
partner is already through"** - the exact thing the barrier exists to establish - so the flag is now kept
across `Show` and *consumed* when this side leaves (`Release`, or the immediate pass-through). Wiping it on
`Show` threw the message away; never clearing it would disarm the barrier after one use.

**And the check did not catch bug 2.** It asserted `!holding` at the end, and a *timeout* releases the run
exactly like the peer's message does - so a 45 s wedge and a clean handshake looked identical. `timedOut` is
now counted and the check fails on it. Worth keeping in mind for the other checks in this repo: when a
safety net has the same visible effect as success, "it recovered" is not "it worked".

**The stagger that this section explained away turned out to be a real bug.** I wrote here that the two
shops going up ~14 s apart was a property of the fixture (each instance forcing its own stage end), and that
whether a real stage end propagates was "a stage-progress question, not a shop question". The first half was
right, the second was wrong: **a stage end did not reach the other instance at all** - measured, fixed and
written up in the next section. The barrier itself behaves correctly in both orders, which is what this
fixture could measure.

## Play-test observations, recorded for later (2026-10-19)

Four points raised after a play session. Recorded with what the code does *today*, so the next
session starts from facts instead of re-reading everything. None of these is built unless stated.

### 1. Revive amount: flat or percentage? (DECIDED — 50% of max hp, `-coopreviveflat` keeps 100)

Today it is **flat and hard-coded**: `CoopDowned.ReviveHp = 100` (with `ReviveRadius = 3.0`,
`ReviveSeconds = 5.0`). The downed dwarf therefore stands up with exactly 100 hp whatever its maximum is: at
a 400 hp pool that is a 25% revive, at 150 hp it is 67%. Which one is wanted is a real decision, because the
two produce very different difficulty curves in a long run. Changing it is a constant plus a measurement, not
a design overhaul. Worth deciding together with point 2.

**Decided (2026-10-20):** 50% of max hp is the default; `-coopreviveflat` restores the old flat 100.
`CoopDowned.ReviveHpFor(maxHp)` is the single rule used by the revive path. A live check in
`-Test revivecircle` logged host `maxHp=145 hp=72` and client `maxHp=130 hp=65`, both `mode=50%`.
The downed/revive flow itself was verified earlier (`check: revived ... hp=100`); this run verifies the
new rule on a live `Player.maxHp` (`Stat.Value`), not a hand-computed constant.

### 2. A visible fill circle like the mission objectives (IMPLEMENTED)

There is **no visual** today: the revive is a 3-unit radius and a 5 s timer, and the only trace is a log line
(`revive: started` / `revived`). The game already draws a filling circle for mission objectives, so the cheap
route is to reuse that: a ground-zone decal anchored on the downed dwarf, filling while the survivor stays
inside. Constraint learned from the HUD work: procedural meshes and built-in shaders do **not** render in this
build, so it has to reuse the game's own ground-zone visual rather than to draw a circle.

**Implemented (2026-10-20):** `CoopReviveVisual` clones `ActivationZone.areaProjector` (the real mission
circle, shader `FX/FX_Decal_Zone`) into a local `DecalProjector`, scales it with progress, and writes the
shader's `_Fill_Factor`. The downed player sees the circle around itself; the survivor runs the same 3 m /
5 s timer against `CoopNet.RemotePos` and sees it around the partner. No packet is needed because only the
visual is computed. `-Test revivecircle` positive `20261020-011703-revivecircle`: `prefab=Projector`,
`hasFillFactor=1`, 329/332 updates, PASS both; negative `-ReviveVisualOff`
`20261020-011516-revivecircle`: `created=0`, FAIL both. The GroundZone fallback remains for runs with no
`ActivationZone` in the scene; the current test mission has two.

### 3. Monsters stutter and never turn on the client (FIXED — see "Enemy mirror smoothing + facing" below)

- **Stutter:** enemy positions travel host -> client as `EnemySnapshot` at **2 Hz**
  (`CoopEnemy.SendSnapshots`, `Time.frameCount % 30 == 0`) and the client **sets the position directly**, with
  no interpolation. Two position updates per second, snapped - that is the judder that is being seen.
- **No rotation:** rotation is **not on the wire at all**. Packet 8 carries `(id u16, q-pos 3s16)` only, and
  the mirrors are additionally frozen (`SetAgentEnabled(false)`), so their own AI cannot turn them either.
- This is not a regression, it is the visible edge of the current design. The fix has two independent halves:
  (a) smooth the existing 2 Hz stream (move towards the last snapshot instead of snapping, or raise the rate /
  send deltas), and (b) put facing on the wire so a mirror can turn - a packed u8 angle costs one byte per
  enemy per snapshot. The swarm peaks near 260 live entities, so per-enemy per-tick work has to stay cheap.
- Already measured and worth not confusing with this: `check: snapshot applied=… missed=…` shows the
  *positions* arrive and land. The complaint is about smoothness and orientation, not about missing data.
- **Resolved (2026-10-20):** facing is now a packed byte in the snapshot and the client moves each
  mirror towards the newest position with a speed and per-frame cap instead of snapping. Positive and
  both negative controls are recorded in the section at the end of this file.

### 4. XP on the ground is not synced (FIXED — see "XP orb sync" below)

- What **is** synced is the **XP total**: every `Player.GainXp` is mirrored as `XpGain` (packet 3) and applied
  with `scaleByXpGain: false`, so both dwarves level together (verified: identical totals on both sides).
- What is **not** synced are the **orbs on the floor**. Unlike currencies, which travel as
  `LootSpawn`/`LootClaim`, XP pickups have no packet. Each instance only sees the orbs its *own* kills
  dropped: the host kills, the host sees orbs, the client does not - and the XP still arrives as a number,
  which is exactly why the totals match while the floor looks empty.
- If the orbs should be visible to both (a fair ask - they are the main "the fight pays off" feedback), the
  shape is the same as loot: broadcast the orb spawn, remove it on both sides when either player collects it,
  and have the collecting side send the claim. Care is needed to avoid double XP: the total is already
  mirrored, so a mirrored orb must not also grant XP locally.
- **Resolved (2026-10-20):** orbs travel as packet 30 (`XPManager.Spawn`), claims as packet 31
  (`XPManager.Claim`/`FinishPickup`). The receiver finds the orb in the static
  `XPManager.xpPositions` buffer and removes it under `ApplyingRemoteXp`; `Player.GainXp` is
  suppressed for that copy, so only the collector's `XpGain` lands. `-Test xpsync`
  `20261020-005304-xpsync` PASS both (`spawnSent=16`, `claimSent=15`, client `applied=16`,
  `suppressed=8`); `-XpOff` `20261020-005512-xpsync` FAIL. Details at the end of this file.

## Arena test fixture (2026-10-19)

The walking bot is a poor fixture for static checks: the dwarves circle, terrain and wave timing change
the camera, and a projectile or dummy cannot be parked at a known distance. An opt-in arena was added to
the local prototype and measured.

- Flags: `-Arena` (host + client), `-ArenaProbe` (measure only), `-ArenaNoClear` and `-ArenaWaves`
  (negative controls), plus radius/ separation/ dummy-distance/ dummy-type overrides. Defaults are
  `radius=10`, `separation=2`, `dummy distance=2`, `dummy=GRUNT0`. The old walking bot remains the
  default for swarm/objective tests.
- The arena anchors in the cave around the player start (not the map centre; the centre was a wall and
  the camera stayed at the drop pod), clears that radius through the game's own `MineableBlock.Die`,
  suppresses host waves, spawns one `LEVEL_START` dummy, and stands both dwarves at fixed spots.
- **Measured:** `check: arena-clear role=host cleared=57 open=0 rocks=57 veins=0 ms=2
  anchor=(17.2,0.0,39.9)` and the same on the client; `check: arena dummy role=host type=GRUNT0 id=1
  pos=(17.2,0.0,41.9) hp=1000000000 pinned=1 enemies=1`; client `check: arena info ...
  dummyPos=(17.2,0.0,41.9) ... near=1 mirrors=1`; both sides `RESULT: arena PASS ...`.
- Negative controls are named: `-ArenaWaves` → host `RESULT: arena FAIL reason=WAVES_LEAKED ...`, and
  `-ArenaNoClear` → both `RESULT: arena FAIL reason=NOT_CLEARED ...`.
- Two bugs the fixture exposed and fixed: the clear test compared a distance against a **squared**
  radius (`d <= r2`), so a radius-10 arena cleared 1872 blocks (almost the whole map) and left both
  cameras framing unrelated cave parts; and the host splash detector read only a 60-line log tail,
  so it kept clicking the centre of already-running games. The client's remote indicator also used
  `Camera.main` (the UI camera in this build) instead of `CameraController.cam` and reported every
  remote dwarf as off-screen. The capture helper now places the two game windows side by side.
- **Not claimed:** the arena `hit` test (`-Test hit -Arena`) still reports `HitsSent=0`; the client's
  fire events are forwarded but the dummy mirror is not damaged. The normal `hit` fixture already
  verifies client damage reaching the host (`sent=20 applied=20`), so this is arena targeting, not a
  newly found co-op defect. The game camera still has its look-ahead and puts the local dwarf near the
  bottom edge; that is cosmetic and was not pursued.

**Verdict:** keep the arena as an opt-in static fixture (fixed positions, one dummy, wave suppression,
block clearing) and stop investing in its camera/hit sub-cases. It has already paid for itself by
finding the over-clear, splash and indicator bugs. Use the walking fixture for swarm/objective tests and
move on to the next co-op milestone; revisit the arena only when a static combat/visual check is needed.

## Objective state at run start (the Boolo-Kappe 8/12)

Observed: the secondary objective showed `SAMMLE BOOLO-KAPPE 8/12` from the start of every mission,
even though nothing had been collected. Two separate facts were behind that:

1. **The 8/12 is the objective's target, not its progress.** The `ObjectiveProgress.progress` is 0 at
   the start (`check: objective-start role=host primary[COLLECT_POINTS=0] secondary[LEVEL_PICKUP=0]`).
   The HUD text `8/12` is the objective's target/definition (its required Boolo-Cap amount), not the
   runtime progress we mirror, so "nothing collected yet" is correct and the label does not move until
   the objective itself changes. The actual collected value is the `ObjectiveProgress.progress` field,
   which is 0 at the start.
2. **Every mission picked the same secondary because the seed was fixed.** `CoopNet.SharedSeed` was
   the constant `20261017`; `CoopObjective.SeedPrefix` seeded `UnityEngine.Random` with it before
   `SetupSecondaryObjectives`. Worse, the first attempt generated the new seed in
   `OnCoreRunStarted`, which runs *after* `SetupSecondaryObjectives` on the host: the host chose its
   secondary with the old constant while the client used the new seed (observed host
   `objective: seeded Random=20261017`, client `objective: seeded Random=901174632`).

Fix: the host now generates a fresh random seed in the `SetupSecondaryObjectives` prefix, immediately
before `UnityEngine.Random.InitState`, and the client receives that seed through the existing
`RunConfig` heartbeat; `MapGenerator.CreateMap` uses the same value. A fixed seed is available only
for reproducible tests (`-coopseed=<int>` / `coop-run.ps1 -Seed <int>`). Measured with a random-seed
arena run: host `run: new shared seed=-802946283`, both peers
`objective: seeded Random=-802946283` before `SetupSecondaryObjectives`, both
`map: seeded Random=-802946283`, and the arena test PASS (`cleared=35`). The cave and the secondary
objective are now random per mission and identical on both peers.

`CoopObjective.Reset` on `StartGame` and `OnCoreRunStarted` only clears the cached tracker, the
`Latest` map and queued packets so the next stage/run cannot keep writing into the previous tracker.
It deliberately does **not** zero the host's objective progress: that would alter the game's real
state and did not update the HUD. If the 8/12 target still appears, it is the Boolo-Cap secondary
being selected for that seed; the actual progress is the separate `ObjectiveProgress` value.

## Milestone G: in-game host/join UI (2026-10-19)

- `CoopJoinUI.cs` draws a direct-IP/LAN panel (address, port, Host, Join, F8 hide) before the run,
  including the publisher splash, and calls the same runtime `CoopNet.Init` the `-coophost` /
  `-coopclient` flags use. It sets the client's bot phase; the existing run-config/map/objective gates
  are unchanged, so the host still starts the run and the client follows.
- The launcher's `-Ui` mode starts both instances **without** role flags, passes `-coopui` and
  `-cooplog=host|client` (separate logs for the two concurrent processes), and the click helper
  presses Host on the host window and Join on the client window at the fixed panel coordinates.
- **Verified:** `RESULT: ui PASS reason=ui role=host peer=1 peers=1` and the same for the client, with
  no role flags. The end-to-end `-Ui -Arena -Test arena` run also PASSed (`cleared=26`, `peer=1`,
  `peerMirrors=1`) with UI-selected roles. The `ui` test fails at 45 s if either role or the peer is
  missing.
- **Not included:** Steam lobby/relay/invite. That needs a Steam account and explicit
  approval; the current UI is LAN/direct-IP only (UDP 9050 by default, same game build on both
  sides). F8 hides the panel; the command-line role path remains for every other test.

The panel's text fields are a small click-to-focus button plus IMGUI characters because the interop
assembly exposes no `GUI.TextField`; the default address/port (`127.0.0.1:9050`) need no typing for
two instances on one PC.

## Grenade mirror with different loadouts (2026-10-19)

The throw itself was already mirrored (`GrenadeThrow`, packet 25): skill name, origin and direction,
replayed with `Grenade.Throw(..., isOriginal:false)` and a damage-suppressed explosion. It passed
only when **both** dwarves had the same grenade. The realistic case - the receiver does not have the
host's grenade - failed with `NO_EXPLOSION`: the receiver fell back to a bare
`Object.Instantiate` of the prefab, which has no initialized grenade pool/handler, so the grenade flew
but never exploded.

What did not work, both measured:

- Creating the handler at throw time (`AddWeaponHandler` + `Init`, with and without adding it to
  `Player.weapons`) throws a masked NRE or leaves a handler whose `GetGrenadeFromPool()` is null.
  A handler created after the run has started is not driven.
- Treating "a `WeaponHandler` with this skill name exists" as "I already have it" was a false signal:
  the half-created handler matched by name while its pool was still null, so adoption skipped the one
  call that would have fixed it. **`Player.HasWeapon(data)` is the only reliable predicate.**

The fix has two halves:

1. `GrenadeLoadout` (packet 26), host -> client: the grenade skills the host's `Player.weapons`
   actually holds, sent at run start and again whenever the set changes (a card can add one).
2. `CoopGrenade.TryAdoptPending` equips each one through the game's own
   `Player.EquipNewWeapon(data, false)` in the window between `OnCoreRunStarted` and `StartGame`
   (the same window the `-DevGive` stimulus uses), retrying for 20 s because `EquipNewWeapon` throws
   until the run player is really up. `Adopted` is cumulative, not zeroed at `StartGame`: the
   run-start reset had wiped the very fact the verdict reports.

Measured (`coop-run.ps1 -Deploy -Auto -Arena -Test grenade-mirror-damage -DevGiveHostOnly
-DevGive GrenadeWeaponSkill_HEGrenade`, run `20261019-201723`):

```
client: check: grenade host-loadout recv=2 n=1 [GrenadeWeaponSkill_HEGrenade]
client: check: grenade adopt 'GrenadeWeaponSkill_HEGrenade' via=EquipNewWeapon handler=yes weapons=1->2 registered=1
client: check: grenade mirror spawned skill='GrenadeWeaponSkill_HEGrenade' … via=pool
client: grenade: mirror exploding (damage suppressed)
client: RESULT: grenade-mirror-damage PASS reason=sent=1 spawned=1 exploded=1 adopted=1
host:   RESULT: grenade-mirror-damage PASS reason=sent=1
```

Three things this run changed beyond the feature:

- A throw that arrives while the receiver is still in the **menu** is dropped (`Gc == null`) instead
  of spawning against the menu dwarf. The old mid-run handler creation did exactly that in the first
  attempt and poisoned the later adoption.
- The test is role-aware: the host may pass as sender, but the **client must show the received mirror
  spawn and explode**. With adoption, the client also throws its own grenade, and the old
  `sender || receiver` rule greened the client before the received mirror had exploded
  (`sent=1 spawned=1 exploded=0`).
- **Known side effect, not yet changed:** adoption gives the receiver the weapon, so it fires that
  grenade too. A visual-only variant - create the handler/pool at run start, then remove it from
  `Player.weapons` so the game does not drive it - would keep loadouts distinct and still needs its
  own measurement.

## Run-start load barrier (2026-10-19)

Design requirement: the host starts the run; the client adopts every setting and only picks its own
character; and the two runs must not begin until **both** sides have finished loading. Settings
adoption already did the second part. The timing problem was real: in the runs above the host's
`StartGame` and the client's were **14-16 s apart**, so one dwarf was already in the cave while the
other was still loading.

`CoopLoad.cs` implements the barrier with the game's own pause door:

- `OnCoreRunStarted` arms it (a new run, not a stage transition).
- The `GameController.StartGame` postfix marks "loaded", and the next tick calls the public
  `GameController.PauseCore()` and sends `RunLoaded(ready)`.
- The host waits for its own ready **and** the client's, then sends `RunLoaded(go)` and calls
  `UnpauseCore()`; the client unpauses when `go` arrives. A 30 s timeout releases a lost peer, so the
  barrier cannot soft-lock a session (the same shape as the level-up, shop and stage barriers).

Two findings the test produced:

- **The faster side's `ready` can arrive before the slower side has even armed.** Measured: client
  `load peer-ready` at 20:09:52, its own `OnCoreRunStarted` at 20:10:05. The first version reset
  `PeerReady` while arming and the client then reported `peer=0` despite `readyRx=1`. The packet now
  carries the **shared seed as a run epoch**; a ready for the current epoch survives arming, a stale
  one cannot release the new run.
- **`PauseCore` really pauses the game.** During the host's 23.7 s hold its position and enemy count
  were constant while its bot's `steps` counter kept climbing (the mod's `Update` runs, game time
  does not). `IsPaused()` read 1 on both sides.

Measured with a deliberately delayed client (`-Test load -LoadDelay 8`, which passes
`-cooploaddelay=8` to the client only; run `20261019-202123-load`, and the same run reports
`saves: restored and verified (14 files, canary removed)`):

```
host:   check: load held role=host paused=1
host:   check: load go sent role=host wait=23,5s
host:   RESULT: load PASS reason=local=1 peer=1 go=1 paused=1 held=23,5s timeout=0
client: check: load held role=client paused=1 delay=8,0s
client: check: load local-ready role=client after=8,0s
client: check: load released role=client via=go held=8,0s peer=1 go=1 paused=1
```

The negative control is a single instance (`-Only host`): no peer ever reports ready, so the run must
release on the timeout and the verdict must be `FAIL timeout=1`.

**Honest scope:** "loaded" is `GameController.StartGame`, the point the level is playable, not a hook
on the literal loading-screen form. The visible result is the level on screen, frozen until the partner
is ready, then both unpause.

## Camera frames both dwarves (2026-10-19)

The camera follows one `Player`, so on the test bot's circle the partner is simply not on screen:
the two bots run the same circle in opposite phase, i.e. **20–23 units apart** for the whole run.
The off-screen marker (`CoopIndicator`) was compensating, but a marker is not the partner.

The projectile origin was **not** the bug: the grenade run logs
`mirror spawned … from=(97.7,1.0,68.7)` against `remotePos=(97.7,0.0,68.7)` (run
`20261019-201723`) — the mirror starts at the remote dwarf's fire point, one unit above the feet.
What made it look like "from nowhere" was that the fire point could be outside the local view.

Measured camera (probe `-coopcameraprobe`, run `20261019-203618-camera`):

```
vcam=CinemachineVirtualCamera follow=Player lookAt=none
pipeline=CinemachineTransposer/CinemachineComposer/CinemachineBasicMultiChannelPerlin
lensFov=25,0 lensOrtho=0 aspect=1,78 (host) / 1,50 (client)
transposer followOffset=(-9.6,33.9,-26.0) binding=LockToTargetWithWorldUp
```

So the game **does not use a FramingTransposer**, and native `CinemachineTargetGroup` group-framing
(the clean solution, its enums and fields are exposed by the interop) would require replacing the
game's body component. That was not done. Instead:

- A `CoopCameraFocus` transform sits at the **midpoint** of the two dwarves and is assigned as
  `vCam.Follow`; the existing `CinemachineComposer` aims there, so both dwarves are symmetric.
- The transposer's own `m_FollowOffset` is scaled by `1 + max(0, sep-10)/40`, capped at 1.6. This
  dollies out along the game's isometric direction: the view widens without changing FOV/perspective
  and without moving the camera into the cave.
- `Follow`, the offset and every field the module touches are restored on run start and whenever no
  remote exists, so solo/menu are untouched. `-coopcameraoff` keeps vanilla follow for the control.

Measured on the final build (run `20261019-204847-camera`, no arena):

```
host:   RESULT: camera PASS reason=mode=focus+dolly samples=20 far=20 farVisible=19 both=19 maxSep=21,8
client: RESULT: camera PASS reason=mode=focus+dolly samples=20 far=20 farVisible=20 both=20 maxSep=20,6
```

The immediately preceding build (run `20261019-204538-camera`) measured **20/20 both** on both sides;
the one missing final-build sample is a camera-damping transient, and the off-screen marker covers
that instant. Viewport z of the dwarves rose from ~40–48 (vanilla) to ~49–63 with the dolly; every
sampled frame but that one had both inside the viewport. The negative control is the same measurement
with `-coopcameraoff`:

```
host:   RESULT: camera FAIL reason=mode=none … far=20 farVisible=5
client: RESULT: camera FAIL reason=mode=none … far=20 farVisible=1
```

The `camera` test only counts samples at separation ≥ 12 and requires ≥ 90% of them to contain the
remote in the viewport; it is state-based, not a timeout. The off-screen marker remains the fallback
for the extreme case (a player who runs far away).

## Enemy hp sync; the client keeps its own damage numbers (2026-10-19)

Design clarification: the client must not see the host's damage numbers - its own are enough. I had
built a host->client `EnemyDamage` channel first (client `events=10 total=400 text=10/0` through
`DamageTextAnimationSystem.SpawnText`, which needs `PreWarm()` on the `DamageTextSystem(Clone)`),
measured it, and then removed it. What was actually missing was the health value.

- `EnemySnapshot` now carries `hp(i32)` per entry (100-entry chunks, 12 B each instead of 120 x 8 B).
  The client applies it damage-only: `currentHp = min(currentHp, snapshotHp)`. A snapshot that
  predates the client's own reported hit can therefore never heal the mirror; the next host update
  catches up.
- The client's own weapons already call `TakeDamage` on the mirror locally, so its numbers are
  immediate. No host number event exists, and the host still applies client hits with `doFx=false`.
- PASS (`20261019-210726-hpsync`, arena): host `hits=10 total=351 dummyHp=999999856 snapSent=34`;
  client `snapSeen=11 equal=6 drops=1/82 above=4`. The client's own hits never landed in this
  fixture (`hit sent=0/0`), so every drop it saw was host-originated - direct evidence of the
  host->client path.
- Control (`20261019-211019-hpsync`, `-HpSyncOff`): client FAILs on state
  (`snapEntries=31 hpSeen=0 hpSync=off`), not on the generic test timeout; the host still PASSes.
- Known limit: damage-only, so enemy healing is not mirrored. No healing enemy appears in the
  tested content. The arena dummy sits at ~1e9 hp, so the absolute numbers are small relative to
  its pool; the drop counters, not the pool, are the evidence.

## Mirrored muzzle flash (2026-10-19)

The game's muzzle flash is a light, not a prefab: `ProjectileWeaponSkillData.FlashSettings`
(`ImpactFlashSettings`: color/range/duration/delay/Enabled) is handed to
`Assets.Scripts.VFXSystem.ImpactLightsManager.AddLight(position, settings)`. `CoopCombat.Drain` now
calls `Flash(e)` for every received Fire, before and independent of the projectile mirror (with
`-cooptrace` off there is no projectile, but the flash still happens). `Lights()` finds the scene
manager through `Resources.FindObjectsOfTypeAll`, caches it, and logs found/MISSING on first use.

- Verified (`20261019-213116-muzzle`): client `flashes=8 disabled=2 failures=0` (host's
  `ProjectileWeaponSkill_Subata`), host `flashes=0 disabled=10` (client's
  `Bosco_ProjectileWeaponSkill_Base`, whose `FlashSettings.Enabled=false` in the game's own data).
  The host's `disabled=10` with `flashes=0` initially looked like "the code never ran" because the
  tick line omitted the disabled counter; the `coop flags: muzzle=True` startup line and the
  `disabled` field in the verdict settled it. Counters are displayed as
  `muzzle=flashes/noSettings/failures/disabled`.
- Negative (`20261019-213321-muzzle`, `-MuzzleOff`): both FAIL on state
  `fireRecv=10 flashes=0 muzzle=off`, not on the generic timeout.
- The automation's client carries Bosco, which the game does not flash; that direction is
  "respected disabled", not "no flash code". A client with a gun produces the same AddLight calls on
  the host. No flash is invented for a weapon that explicitly disables it; only a **missing**
  settings asset falls back to a small warm default.

## Weapon coverage, per-handler vs per-weapon, and the muzzle origin (2026-10-19)

Open question: does every weapon need its own code? **No for projectile weapons.** The Fire hook sits
on `ProjectileWeaponHandler.ShootProjectileInDirection`; the peer resolves
`ProjectileWeaponSkillData` by asset name and uses that asset's `ProjectilePrefab` and
`ProjectileSettings`. One code path serves all of them. Per-weapon **testing** is still needed because
the assets carry different prefabs/settings, and per-**category** code is needed for handlers that are
not `ProjectileWeaponHandler` at all.

Measured with `-Test weapons -Mirror -DevGive "…"` (run `20261019-215130-weapons`, 10 skills):

- mirrored on both sides: `Subata`, `Bosco_ProjectileWeaponSkill_Base`, `M1000`, `Boomstick`,
  `ImpactAxeSkillData` - `noAsset=0 noPrefab=0 failed=0`.
- never fired by the plain `-DevGive` path: `AutoCannon`, `Leadstorm_Minigun`,
  `GrenadeLauncher_PGL`, `AlternatingProjectileSkillData_Boltshark`,
  `LinkProjectileSkillData_BreachCutter`, `BeamWeaponSkill_Electrical_TH0R`. The plain path adds all
  weapons, but the automation only fires the weapon that was active at run start plus Bosco.

The sweep (`-DevSweep <sec>`) makes the matrix real: it clears `Player.weapons`, equips one skill at a
time into action-bar slot 0, and fires it through `ProjectileWeaponHandler.ShootProjectileInDirection`
(the same path the hook observes). Runs:

- `20261019-220450-weapons`: `AutoCannon` 9/10 and `Leadstorm_Minigun` 9/9 applied on both sides;
  `Boltshark` reached the mirror but `noPrefab`; `PGL` returned from the manual call without a Fire;
  `BreachCutter` was `not-projectile`; the last weapon (beam) was skipped by an ordering bug.
- `20261019-220839-weapons` with the alternating resolver: `Boltshark` mirrors both prefabs,
  `DamageProjectile_Electric_Bolt` and `DamageProjectile_Acid_Bolt` (27/28 host, 32/33 client,
  `applied`), and the last weapon now gets its window.
- `AlternatingProjectileSkillData` inherits `ProjectileWeaponSkillData` but has `Projectiles[]`, not a
  single `ProjectilePrefab`; `SpawnMirrorProjectile` now indexes that array round-robin.

What is left is genuinely per handler category, not per weapon:

- `LinkProjectileSkillData_BreachCutter` — **done.** The handler is `LinkProjectileWeaponHandler`
  (not `ProjectileWeaponHandler`); a Harmony postfix on its public `Get()` sends the same Fire packet
  with `pData.name`, `firePoint` and the base `externalAimDirection`. Verified
  (`20261019-222018-weapons`): `LinkProjectileSkillData_BreachCutter` applied 9/10 on both sides,
  `noAsset=0 noPrefab=0 failed=0`.
- `ProjectileWeaponSkill_GrenadeLauncher_PGL` — **done.** The sweep now holds the trigger
  (`CoopDev.HoldTrigger`, dev-only; `Player.ShouldManualTriggerWeapon` returns true and
  `primaryTriggerDown` is re-asserted after `Player.Update`) instead of writing the field once, so the
  game's own activation logic fires it. The shot is observed on
  `Grenade.ShootGrenade(WeaponHandler, Vector2, GrenadeLauncherSkillData, Vector3, bool)`, a base
  method with no override. The base `ProjectileWeaponHandler.ShootProjectileInDirection` hook cannot
  see the PGL's override, and patching the virtual plus its override is the documented `0xc0000005`
  crash, so the grenade launch entry is the safe hook. Runs `20261019-233339-weapons` (6 s window)
  and `20261019-235051-weapons` (15 s window, final build): `expected=1 received=1 notFired=0`,
  `RESULT: PASS` on both roles; the host sees the client's and the client sees the host's
  `ProjectileWeaponSkill_GrenadeLauncher_PGL` Fire event. The 15 s window matters: the PGL can need
  ~4 s to become ready, and a 6 s rerun failed (`20261019-234848-weapons`, `notFired=1`). The
  receiver still goes through the normal Fire/mirror path.
- `BeamWeaponSkill_Electrical_TH0R` — **done, but it is not a `BeamWeaponHandler`.** The asset is a
  `LightningBeamSkillData` (subclass of `BeamWeaponSkillData`) and its `AddWeaponHandler` creates
  `LightningBeamHandler : WeaponHandler`, with its own `ActivateWeapon`, `ActivationLogic` and an
  `IEActivate` coroutine. It has no `isBeaming` and never calls `StartBeaming`; the earlier re-wrap as
  `BeamWeaponHandler` read the wrong offsets, which is why the old run saw `beaming=0`. The sweep now
  calls `LightningBeamHandler.ActivateWeapon()`; a postfix on that method sends one `BeamState` (29)
  per burst and the remote line lives 0.2 s. Run `20261019-234302-weapons`: `RESULT: PASS` on both
  roles, `beam=3/2` host and `3/3` client, `skills=1/1`, `notFired=0`; the first host call reported
  `fired=1`. Negative control `-BeamOff` (`20261019-234645-weapons`): both roles FAIL
  `notFired=1`, `beam=0/0`, so the pass is the mirror path, not the activation log.
- `BeamWeaponSkill_RotatingBeam_CryoCannon` — **done, continuous path.** This is a real
  `BeamWeaponHandler` (`StartBeaming`/`StopBeaming`). Sweep `20261020-012001-weapons`
  (`-DevSweep 15`): host `beam=98/91`, client `100/98`, `skills=1/1`, `RESULT: PASS` both;
  `beam: local start/send/applied` lines present. The first two sweep logs still read
  `beaming=0` because the beam starts ~3 s after equip; the 15 s window covers it. ShardDiffractor,
  SludgePump and LocalSpaceBeam share this handler path but were not individually swept.
  **Game BeamVFX - DONE (20261020-133051-beamgame).** `CoopBeam` resolves
  `BeamWeaponSkillData.BeamVFX` (`VFXAssetGeneric`), keeps one live prefab per beam, sets
  `Hovl_Laser.MaxLength`, and falls back to the debug cube. `-Test beamgame` uses a direct
  `CoopDev` stimulus (`isBeaming=true` + `OnLocalStart`, never `StartBeaming`): positive PASS both,
  host `sent=56`, client `applied=68`, `game=1/0/0`, `noAsset=0`, `fallback=0`, and
  `beam: game vfx ... prefab='CryoBeam_VFX' range=5.0`. Negative `-BeamOff`
  (`20261020-133207-beamgame`) FAIL both `sent=0 applied=0 game=0`; negative `-GameBeamOff`
  (`20261020-133320-beamgame`) host PASS/client FAIL `game=0 fallback=68`. The earlier `notFired`
  runs used the short give name `RotatingBeam_CryoCannon`; the asset is
  `BeamWeaponSkill_RotatingBeam_CryoCannon`, so `CoopDev` reported `NOT_FOUND` and never equipped
  it. The direct stimulus is a test fixture; the game's own `ActivationLogic` did not start the
  beam in the auto-start runs. Asset path verified; exact length/alignment not pixel-checked.

The `weapons` test is per-role and excludes a skill this side fired from `missingRecv` (the peer
mirrors it; it is not echoed back to the sender). PGL, TH0R and the continuous rotating beam are
green; the untested variants of the continuous path are named above rather than implied done.

**Origin bug (tester: "the client's projectiles come from the wrong direction").** `OnLocalShot` used
`Player.FirePoint` for every handler. Bosco/drone handlers fire from their own
`WeaponHandler.firePoint`, so the mirror started at the dwarf's gun. Measured (`20261019-214847-hpsync`,
client): `from=(51.6,1,84.5) handlerFP=(51.6,1,84.5) playerFP=(53.6,1,82.4)`,
`dir=(0.2,-1,-0.1)`. The direction is world-space and points at the dummy; only the origin was wrong.
`handler.firePoint` is now preferred, with `Player.FirePoint` as fallback.

**HUD probe (`-coophudprobe`).** On both sides slot #1 is `ProjectileWeaponSkill_Subata`,
`handler=Subata`, `ownerSelf=1`, `inPlayerWeapons=1`; slots 2-4 are empty. The client's bot fires
Bosco, which is a drone weapon and does not sit in the action bar, so the bar's Subata slot is idle
and shows no fire/reload status. The binding is correct; this is a stimulus/loadout difference, not a
missing handler. In normal play the bar weapon is the one the player fires.

## The partner's projectiles were gated off by default (2026-10-19)

Play-test observation: both players fire their own weapons, but nothing from the partner is visible.
Cause: `CoopCombat.Enabled` defaulted to **false** and was only switched on by `-cooptrace`/`-Mirror`.
The Fire packet was received and `Flash(e)` played the muzzle light, but `SpawnMirrorProjectile`
returned immediately, so the peer never spawned a visual. This was the B3 experiment gate, not a
rendering bug.

`Enabled` is now true by default; `-coopmirroroff` is the negative control. The per-projectile
`isVisible` probe stays opt-in (`CoopCombat.Probe`, set by `-cooptrace`/`-coopb3draw`) because that
signal read false even for the game's own projectiles - it is not a gameplay gate and not proof of
invisibility.

Verified in a normal co-op run with no `-Mirror` (`20261019-221411-hpsync`): the host mirrors the
client's `Bosco_ProjectileWeaponSkill_Base` and the client mirrors the host's
`ProjectileWeaponSkill_Subata`, both as the real prefab `DamageProjectile_Kinetic_Medium`, with
`applied=N dropped=0` and no `combat: probe` lines. `hpsync` PASS on both sides.

## Harness: the restore canary had become permanent (2026-10-19)

Every run from 19:25 onwards ended with `saves: VERIFY FAILED - hashes match but the canary survived`.
The cause was a previous partial restore leaving `coop-restore-canary.txt` in the save folder; the next
backup copied it, so the restore faithfully restored a tree containing the marker and then reported a
failure forever. `Backup-Saves` now removes the marker from the backup before recording it, and the
stray file was deleted. Save data itself was verified intact (`drg_save_slot*.dat`, `localprefs.dat`);
only the harness marker was wrong.

## Enemy VAT animation sync (2026-10-19)

The snapshot's `Anim` field now carries the host enemy's VAT state: clip index + 1 in the high 16
bits, normalized phase in the low 16. The host reads
`EnemyAnimator.meshAnim.currentAnimIndex/currentAnimTime/currentAnimation.length`; the client
resolves the mirror's `meshAnim` clip by index (same prefab/order), calls `meshAnim.Play(idx)` on a
clip change and sets `currentAnimTime = phase * length` when the phase is off by more than 0.2.
Playing the mesh directly avoids routing through `EnemyAnimator.PlayVATClip` and its animation-event
path on a mirror.

- The first attempt used a standard `Animator` state hash and timed out (`20261019-230123`): the
  swarm animates through `Assets.Scripts.EnemyAnimator` / `ShaderMeshAnimator` (VAT), not an
  `Animator` state. That wrong channel was removed.
- PASS (`20261019-231004-enemyanim`, normal swarm): host `sent=196`; client `seen=42 applied=7
  sampled=21 match=21` - every sampled mirror held the last applied clip.
- Negative (`20261019-231303-enemyanim`, `-EnemyAnimOff`): client FAILs on state
  `seen=0 enemyanim=off`; the host has no `SnapAnimSent` and times out. That is the control.
- Limits: clip index/phase only, no blend/parameter state; the index order is assumed identical for
  the same prefab (held on the tested swarm); phase is corrected at the 2 Hz snapshot rate; mirrors
  were not checked for animation-event gameplay (test runs use god mode). The mirror's own
  `EnemyAnimator` did not override the mesh during the test.

## Enemy mirror smoothing + facing (2026-10-20)

Play-test observation #3 is fixed: the client no longer teleports mirrors at the 2 Hz snapshot rate, and
the host's facing now reaches the mirror.

- **Wire:** `SnapEntry` carries a packed yaw byte (0..255 = 0..360 degrees). The entry is 17 bytes;
  56-entry chunks stay under the MTU (60 entries were rejected by SteamNetworkingSockets at 1020
  bytes once the yaw byte was added). The snapshot rate is still 2 Hz — the frame loop supplies the
  missing frames.
- **Position:** `CoopEnemy.TickMirrors` runs every frame on the client and moves each mirror towards
  the newest snapshot with `Vector3.MoveTowards`, capped at 12 m/s and 0.5 m per frame. A large gap
  (spawn, pool reuse, a hitch) closes over several frames instead of one jump.
- **Facing:** the packed yaw is applied with `Quaternion.Slerp` (same 10 Hz convergence constant);
  a frozen mirror now turns with its original.
- **Positive** (`20261020-001101-enemysmooth`, normal swarm): host `sentChunks=66 facing=846`;
  client `applied=731 maxStep=0.412 facingApplied=142`, `RESULT: PASS` both roles.
- **Negative smoothing** (`20261020-000933-enemysmooth`, `-SnapSmoothOff`): client FAIL
  `maxStep=2.002`; the old snap path is the control.
- **Negative facing** (`20261020-001231-enemysmooth`, `-EnemyFaceOff`): host FAIL `facing=0`,
  client FAIL `facingApplied=0`.
- The test (`-Test enemysmooth`) requires the client to have moved in many small steps
  (`maxStep <= 0.75`) and to have applied at least 10 host turns; the host must have sent both
  chunks and facing. Both negative controls are state-based, not timeouts.
- **Final-build rerun** (`20261020-005713-enemysmooth`, build `2C4B9760140340BE`, 56-entry chunks):
  host `sentChunks=65 facing=953`; client `applied=770 maxStep=0.500 facingApplied=364`,
  `RESULT: PASS` both roles. No "packet size exceeded" line in this run.

## Soak 2026-10-20 (600 s, two instances)

- `20261020-040552-soak`: PASS both after 600 s post-release. Host 2900 live enemies, client
  2915; 618,215 snapshots applied / 3671 missed; 928 projectile events each way; no matches for
  `Exception`, `NullReference`, `0xc0000005`, snapshot-size, disconnect or spawn-failure patterns.
  Saves restored and verified (14 files, canary removed).
- **Fixed during the soak:** `SpawnMirror` called `SpawnAt`, then looked the returned `Enemy` up in
  `ByPointer`; the registration postfix had not run yet, so 29/50 mirrors were dropped as
  `MirrorSpawnFailed`. It now calls the idempotent `Register(e, "mirror-spawn", scale)` before the
  lookup. After: `fail=0 null=0 noEntry=0 noPtr=0` at `spawn=3921`. Also fixed the
  `SpawnWeaponHandler.Despawn` Harmony parameter (`ws`, not `spawn`).
- **Fixed (2026-10-20):** `localDie=0`, `miss=0` in `20261020-043000-soak` (600 s, 624 spawns,
  580 deaths on each side; `blocked local mirror death id=...` in the client log). The client's local
  `Enemy.Die` for mirrors is blocked (`OnDiePrefix`), the killing hit is still reported, and
  `OnDamageEnd` clamps the mirror to 1 hp; the host's `DeSpawn` removes it. The previous
  `localDie=249`/`miss=249` is gone.
- The `hp hurt ... nearestMirror` drops are expected: each instance's mirrored swarm attacks its own
  dwarf (see `CoopDamage`).

## XP orb sync (2026-10-20)

Play-test observation #4 is fixed: both instances now see the same XP orbs and each orb grants once.

- **Spawn:** a postfix on `XPManager.Spawn(Vector3, float)` sends packet 30 with the quantized
  position and the force modifier. The host is the only sender; the client applies it inside
  `ApplyingRemoteXp` and records the new orb as a mirror.
- **Claim:** prefixes on `XPManager.Claim(int)` and `XPManager.FinishPickup(int)` send packet 31
  with the orb's position. A quantized-position set stops the same local orb from sending twice.
- **Removal without double XP:** the receiver resolves the orb through the static
  `XPManager.xpPositions` buffer (`NativeArray<float3>`, read by reflection because the interop
  does not expose the element type), calls `Claim` + `FinishPickup` under `ApplyingRemoteXp`, and
  the `Player.GainXp` prefix returns false for that copy. The collector's normal `GainXp` is the
  only grant and is mirrored as packet 3.
- **Positive** (`20261020-005304-xpsync`, normal swarm): host `spawnSent=16 claimSent=15
  finishCalls=8`; client `spawnRecv=16 applied=16 mirrorSeen=16 claimRecv=15 claimApplied=8
  suppressed=8`, `RESULT: PASS` both roles.
- **Negative** (`20261020-005512-xpsync`, `-XpOff`): host `spawnSent=0`, client
  `spawnRecv=0`, `RESULT: FAIL` both roles. That is the control.
- **Limits:** claims are matched by nearest position (6 m), so a mirror that drifted further is a
  miss; the run sent 15 claims for 8 finishes because `Claim` and `FinishPickup` can both fire on
  the same orb at slightly different positions. The 8 host finishes matched the client's 8 remote
  gains, so no double XP was observed, but a longer two-player soak is still needed for the
  simultaneous-collection race.

## Trail ground zones (2026-10-20)

Trail weapons are not projectiles and do not call `GroundZone.Init`; they drop zones through
`GroundZoneDispenser.TryPlaceGroundZone`, so neither the projectile nor the old GroundZone.Init hook
saw them.

- **Stimulus:** `CoopDev` detects `TrailWeaponSkillData`, re-wraps the handler as
  `TrailWeaponHandler`, calls `ActivateWeapon()`, then `EnableGroundZones()` if the dispenser is
  missing and `BaseLogicTick(0.5f)`. A freshly equipped test weapon does not get the game's own
  handler tick; driving it is what makes the dispenser place zones.
- **Wire:** postfix on `GroundZoneDispenser.TryPlaceGroundZone` sends packet 32 with the skill name,
  quantized position, radius and lifetime. Only trail skills are sent (ground-zone grenades already
  create their zone from the mirrored explosion).
- **Client:** resolves the trail asset by name, takes
  `TrailWeaponSkillData.GroundZoneParams.GroundZonePrefab`, instantiates it, calls
  `InitVisualsOnly(life, radius, null)` and positions it. The visual expires with its lifetime.
- **Positive** (`20261020-015745-trail`): host `sent=34`; client `recv=38 applied=38 miss=0`,
  `RESULT: PASS` both roles.
- **Negative** (`20261020-015907-trail`, `-ZoneOff`): host `sent=0`; client `applied=0`,
  `RESULT: FAIL` both roles.
- **Limits:** the client zone is visual-only; it does not apply the host's damage/slow. Zone count
  differs (34 sent vs 38 applied) because the client's own trail also drops local zones that are not
  counted as remote; the test only counts the remote path. The first `GroundZone.Init` patch was
  ambiguous/fired for nothing and is retained only as a no-op guard; the dispenser hook is the
  reliable path.

## Remote weapon visual — not applicable (no held model) (2026-10-20)

The `WeaponState` packet (36) and a visual-only prop path are in place, but the tested Driller
weapons have **no held model** to clone:

- The `WeaponHandler` GameObject is the shared `Player/WeaponHandlers` object
  (`DropPod/DropPodModel/PlayerPoint/Player/WeaponHandlers`) with **0 renderers**. The only
  weapon-ish renderers in the player tree are the hand bones (`mixamorigHand.L/R`, 1 each); there is
  no `PGL`/`Subata` model node.
- Cloning that handler therefore produces 0 renderers; `-Test weaponvisual`
  (`20261020-025641-weaponvisual`) ends client `applied=0 renderers=0 miss=255`. The host sends the
  last-equipped skill (PGL); the client finds the handler but no model.
- The current class-skin/mod swap works (the remote GFX takes the host's class mod), so the old
  "skin blocker" is gone. The game has **no separate held weapon model**: `WeaponHandlers` is the
  shared empty container, `WeaponSkillData` carries no model prefab, and no `WeaponSkinData` /
  `WeaponModel` type exists in the assembly. The class-mod skin is the character visual; the
  weapon is represented by the skill/projectile/beam VFX. A held prop is not applicable here.
- Do not claim a held-prop visual. The packet is harmless and the prop path is guarded (no empty
  object is left in the scene); retire `-Test weaponvisual` from the matrix. `-WeaponVisualOff`
  remains the negative control.

