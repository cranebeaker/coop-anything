# Lobby, UI, and late visual mirrors

> Part of the Deep Rock Galactic: Survivor working notes. Extracted verbatim from `NOTES.md` on 2026-10-20 (lines 3399-3840); content unchanged.
> Current state and Ist/Soll: [../STATUS.md](../STATUS.md). Index: [../NOTES.md](../NOTES.md).

## Turret visual mirror (2026-10-20)

The turret is not a projectile and is not a `WeaponSpawnTurret.OnSpawn` call on the tested asset:
`SpawnWeaponSkill_TurretThumper` creates `WeaponSpawnThumperTurret`, which overrides `OnSpawn` and
`DeSpawn`. Patching the base virtual and the override together is the documented 0xc0000005 crash, so
the hook sits on `SpawnWeaponHandler.Spawn`/`Despawn` instead.

- **Host:** `Spawn(Vector3)` postfix registers the handler + spawn skill and watches the public
  `activeSpawns` list. Once the turret instance appears, `TurretSpawn` (33, id/skill/position/life)
  is sent; `TickHostState` sends `TurretState` (34, quantized position + packed yaw, 4 Hz); `Despawn`
  sends `TurretDespawn` (35). The sweep calls `SpawnWeaponHandler.Spawn(player position)` directly
  because a freshly equipped test weapon does not get the game's own deploy tick.
- **Client:** resolves the spawn skill, clones the turret hierarchy and disables every
  `WeaponHandler`/`Collider` on the clone. The spawn skill's `_turretTop` is **null** on the client
  (the reference is resolved on the spawned instance), so the visual source falls back through the
  derived `animatingTop`, `TurretDeploymentPrefab`, and a live local turret. It never calls
  `OnSpawn`/`DoUpdate`, so it cannot shoot or damage.
- **Positive** (`20261020-022442-turret`): host `sent=1 stateSent=56`; client `spawned=1 states=42
  miss=14 prefab='Turret_THUMPER'`, `RESULT: PASS` both roles.
- **Negative** (`20261020-022622-turret`, `-TurretOff`): both `sent=0 spawned=0`, `RESULT: FAIL`.
- **Limits:** the clone is a static visual. Firing VFX/animation, per-part aim and the exact prefab
  source still need work; the client state misses before the visual source is available are counted
  as `miss` (14 in the run), not as failures.

### Turret fire VFX — client side added, still blocked on the host firing (2026-10-20)

The client mirror now has the real light flash, not only the scale pulse:

- `CoopCombat.FlashAt(from, settings, skill)` is the shared light path (the remote-shot `Flash(e)`
  now calls it too); `CoopCombat.MuzzleAt(WeaponHandler, fallbackFrom)` resolves the turret clone's
  own `WeaponHandler.data.FlashSettings` and `firePoint` and calls the same `ImpactLightsManager`.
- `CoopTurret` stores a `WeaponHandler` from the cloned hierarchy (preferring a
  `ProjectileWeaponHandler`), calls `MuzzleAt` on `TurretFire`, and counts `FireFlashes` /
  `FireFlashMiss`. The `turretfire` client verdict now requires `FireFlashes > 0`, so the scale pulse
  alone cannot pass it any more.
- `OnTurretShot` also searches the turret's child `ProjectileWeaponHandler`s; the spawn's
  `weaponHandler`/`spawnWeaponHandler` are not always the firing handler. Only turret-like unmatched
  handlers are logged now, so Bosco/drone shots no longer spam the hook.

Measured (`20261020-125033-turretfire`, direct `SpawnWeaponSkill_TurretThumper` spawn, reliable
auto-start):

```
host:   turret: spawn sent id=1 skill='SpawnWeaponSkill_TurretThumper' native='WeaponSpawnThumperTurret'
host:   turret: fire hook unmatched handler='Bosco_ProjectileWeaponSkill_Base' (x5)
host:   check: turretfire sent=0 recv=0 applied=0 flashes=0
client: turret: spawned id=1 ... prefab='Turret_THUMPER'
client: check: turretfire sent=0 recv=0 applied=0 flashes=0
RESULT: FAIL (both)
```

So the visual mirror and the spawning are still correct, but the direct dev-spawned Thumper turret
never fires: the only `ProjectileWeaponHandler.ShootProjectileInDirection` events are Bosco's. The
client muzzle path is ready and unexercised; it only runs once the host sends `TurretFire`.

**Field probe (`20261020-125843-turretfire`): the handler is initialized, it is just a plain
`WeaponHandler`.** The spawn logs its actual fields:

```
turret: fields wh='Sub_ThumperWeaponSkill' swh='SpawnWeaponSkill_TurretThumper'
        wsd='Sub_ThumperWeaponSkill' owner=1 ext='SpawnWeaponSkill_TurretThumper' fire='FirePoint'
turret: child[0] type='WeaponHandler' data='Sub_ThumperWeaponSkill' dType='WeaponSkillData'
        proj=0 beam=0 lightning=0 expl=0 melee=0 ground=0 auto=0 enabled=1 owner=1 fire='FirePoint'
```

So the direct spawn is fully wired; the Thumper simply does not use a projectile/beam/explosion
handler, and its `OnShoot` action is not invoked by the plain handler either. The actual thump is
the **`OnReloadUpdate(t)` animation callback**, normally driven by the spawn handler's reload. The
direct dev spawn has no live spawn-handler reload, so the callback never ran.

**Fix and verification (`20261020-131050-turretfire`, `20261020-131227-turretfire`).** The turret
state tick (`CoopTurret.OnUpdate`, already the 4 Hz host tick) now advances a 0..1 `ThumperClock` for
a `WeaponSpawnThumperTurret`, calls `OnReloadUpdate`, and `OnReloadFire` mirrors the thump as a
`TurretFire` (0.5 s rate limit). The client `ApplyFire` uses `CoopCombat.MuzzleAt`, so the clone
shows a real `ImpactLightsManager` flash at its `FirePoint`; the `turretfire` verdict requires
`flashes > 0`.

```
positive 20261020-131050:
host:   turret: thump fire id=1 t=0.25 / 0.50 / ...
host:   check: turretfire sent=5 recv=0 applied=0 flashes=0
client: check: turretfire sent=0 recv=5 applied=5 flashes=5
RESULT: PASS both

negative 20261020-131227 (-TurretFireOff):
both:   check: turretfire sent=0 recv=0 applied=0 flashes=0
RESULT: FAIL both
```

The client tick also shows `muzzle=5/5/...` next to the player muzzle, so the turret flash is
counted separately from the scale pulse. The `OnShoot` listener remains a harmless fallback: it
preserves the original game callback and the 0.5 s rate limit prevents a double send if the real
reload path is also active.

Limitations: the 4 Hz `OnReloadUpdate` drive is a dev-spawn approximation of the spawn handler's
real reload cadence; the mirror is **visual**, the host turret's damage path is unchanged; the
natural deploy path is still unverified. Do not claim the exact thump interval until the natural
deploy is tested.

### Reliable test auto-start (2026-10-20)

The desktop click into a background window is not reliable for automation. `-LobbyAutoPlay` reuses
the lobby's tested auto-join + `UISplashForm.AdvanceFlow` + save-slot + `OnPlayButton` path with
`-LobbyUIOff`, so no lobby screen is built and `CoopAuto` drives the wizard. Command shape:

```
coop-run.ps1 -Ui -Auto -Test <name> -LobbyAutoPlay -LobbyUIOff ...
```

`20261020-125033-turretfire` reached the run, the turret spawn and the test verdict in 1m18s; the
earlier desktop-click attempts timed out after 3m48s without reaching the run. This is a harness
change only; normal play still uses the real Play button.

## Co-op lobby — L0 measurement (2026-10-20)

Design goals recorded for milestone G: a real in-game lobby (own screen), the host sets mission and
difficulty there and the client sees the changes live, the client picks only **class + class mod**,
and the host may start only after the client is ready. Keeping the loading screen up with
"Fertig geladen - warte auf Spieler 2" is explicitly lower priority than the lobby.

Probe: `-cooplobbyprobe` / `-cooptest=lobbyflow` (`CoopLobby.cs`), one host instance:

```
coop-run.ps1 -Deploy -Auto -Test lobbyflow -LobbyProbe -TestQuit both -Only host
```

Archive: `local/work/runs/20261020-053637-lobbyflow/`.

```
RESULT: lobbyflow PASS reason=sources=1 sectors=5 nodes=6 hazards=0 classes=5 mods=15
  hide=ok handlers=8 errors=4 fallbacks=1 driveErrors=0 launched=1 role=host
```

Observations (from the log):

- **The menu-time source is `UIPlaySelectForm.runSettingsManager` + `form.missionRoadManager`.**
  `rs.MissionRoadState` is null in the Play menu; it is assigned only after a main mission is chosen.
  `missionRoadManager.GetMissionRoadState(Elimination)` / `.EliminationState` returns the live road:
  **5 sectors x 6 nodes**, each with `Data.Biome`, `IsLocked`, `MinHazIndex` and `IsCompleted()`.
  This is what the lobby lists.
- **`rs.GetHazardLevels()` NREs in the menu** before a biome is selected
  (`lobbyflow: step=rs.GetHazardLevels failed ... NullReferenceException`). Hazard index 0 still
  works through `OnHazLevelNext(0)` once the hazard page is up. The lobby must read the hazard list
  after the node step (or from the hazard page's own buttons), not at Play time.
- **`form.SetVisibility(false, true)` does not deactivate the form** (`hide=ok hidden=0`), and a
  handler still runs while it is "hidden". The lobby cannot hide the wizard with that call; it must
  **cover** it with its own full-screen surface.
- **Handler route measured:** `StartMainMissionsFlow` -> `OnMainMissionSelect` -> sector -> class ->
  class artifact -> `OnHazLevelNext` -> `UILoadingForm`. `OnMainMissionSelect`,
  `OnClassPageNext`, `OnClassArtifactPageNext` and `OnHazLevelNext` all worked.
- **`OnMissionSectorClick` NREs** with the `MissionSectorState` from the road manager *and* with the
  button's own `sectorState` (4 errors in the run). The game's own `UIMissionSectorButton.OnButtonClick`
  is the working route: the probe pressed `MissionSectorButton_1` and the page advanced to CLASS
  (index 4). The sector page is a **combined page**: it keeps the sector tabs and adds
  `MissionNodeButton_01..06` plus display-only `UIMutatorSetEntry` rows. The first node is selected
  by default, so the node page was skipped and the node handler was not exercised.
- The wizard hands over to `UILoadingForm` **before `GameController` exists**; the probe counts that
  as `launched=1 why=loadingform`. In this fixture the loading form then waits for input (it stayed
  up for 80 s in an earlier run). The load-screen milestone must also **continue** the form, not only
  keep it visible.
- Client class/mod lists are the live `ClassData` / `ClassArtifactData` assets filtered through
  `SaveDataManager.IsClassUnlocked` / `IsUnlocked` (the adopted host profile, I1). Measured:
  **5 class assets, 15 class-mod assets**; the first unlocked class was DRILLER and its first
  unlocked mod `ClassArtifactData_Driller_Interrogator`.

Derived (design consequences):

- The lobby uses the **button path** for mission/sector/node (the game's own `OnButtonClick`), and
  may use the handler path for class, class mod and hazard. The button path is the one proven for
  every selection.
- The lobby screen **covers** `UIPlaySelectForm`; it does not hide it.
- Hazard levels are read after the node selection; at Play time only mission/sector/node data is
  guaranteed.
- The "client picks class + class mod" is exactly what the game exposes; mission, sector,
  node, biome, hazard and mutators are host-side, and skin/starting weapon follow the class mod.

Unknowns (honest):

- Whether `OnMissionSectorClick` can be made to work with another state object (e.g. a state taken
  from the form's current page). Not needed while the button path works, but it is the cleaner API
  if it is ever fixed.
- Why the loading form waits for input in this fixture (auto-continue setting vs. a required input).
  The load-screen milestone starts with a probe of `UILoadingForm.Show`/`SetVisibility`/`OnInput`.

### L1 — protocol + ready/start gate (2026-10-20)

Packets **38 `LobbyState`, 39 `LobbyChoice`, 40 `LobbyStart`** in `CoopNet.cs`; the state machine and
the gate live in `CoopLobby.cs` (`-cooplobbystate`, `-cooptest=lobby-state`). The test runs both
instances in the main menu through the join panel, with no wizard and no run.

**Positive** (`20261020-054247-lobby-state`, `-Ui -Lobby -Test lobby-state`):

```
host:   RESULT: lobby-state PASS reason=host rev=2 blockedPeer=1 blockedStale=1 allowed=1
        choiceRecv=2
client: RESULT: lobby-state PASS reason=client state=2 choice=2 start=1 rev=2
        stateRecv=2 choiceSent=2
```

Sequence: host `rev=1` -> `TryStart` blocked `peer-not-ready` -> client choice on `rev=1`
(ENGINEER / `ClassArtifactData_Engineer_Maintenance`) -> host bumps `rev=2`, `hazard=2` ->
`TryStart` blocked `stale-rev` (the peer's ready still belongs to rev 1) -> client choice on `rev=2`
-> `TryStart` allowed -> `LobbyStart` rev=2 seed=20261017 -> client receives it.

**Negative** (`20261020-054515-lobby-state`, `-LobbyNoReadyGate`):

```
host:   RESULT: lobby-state FAIL reason=ready-gate-off start allowed before peer ready, rev=1
client: RESULT: lobby-state FAIL reason=timeout after 120s
```

The gate is therefore not vacuous: with it disabled the host starts before the client is ready and
the test fails.

Derived: the revision echo is sufficient to invalidate an old ready on a host-side change, and the
start is host-authoritative - the client never starts from its own choice, it waits for
`LobbyStart`. Still open for L2/L3: host-side validation of the client's class/mod against the host
profile, the actual lobby screen, and the handoff of the selected values into the wizard.

### L2 — the real lobby screen (2026-10-20)

The screen is a **clone of the game's root UI canvas** with all children removed, plus cloned
`Image`/`UIButton`/`TextMeshProUGUI` objects for panels, buttons and labels
(`CoopLobbyUI.cs`). It is a ScreenSpaceOverlay canvas with `sortingOrder = 1000`, covers
`UIPlaySelectForm` and blocks its raycasts. The wizard is not driven while it is active
(`CoopAuto.DriveWizard` returns early).

**Positive** (`20261020-063928-lobby-ui`, `-Ui -Lobby -Test lobby-ui`):

```
host:   RESULT: lobby-ui PASS reason=host active=1 sectors=5 classes=5 mods=3 hazard=1
        start=1 rev=2
client: RESULT: lobby-ui PASS reason=client active=1 stateRecv=2 choiceSent=6 start=1 rev=2
```

Sequence: host opens at `rev=1`; host clicks hazard H2 -> `rev=2`; client receives rev 2, selects
ENGINEER / `ClassArtifactData_Engineer_Maintenance`, presses READY; host sees `peerRev=2` and START
is allowed; `LobbyStart` rev=2 reaches the client. The screen exposes 5 sectors, 6 nodes, 5 class
assets, 3 class mods for the local class and 5 hazard levels; the data comes from
`form.missionRoadManager` and is read after the asynchronous load.

**Negative** (`20261020-063812-lobby-ui`, `-LobbyUIOff`): both instances stay `role=solo` and time
out, `RESULT: ... FAIL`. The screen is the thing under test, not a no-op.

Measured implementation facts (these cost several runs, keep them):

- **`AddComponent<T>` is not usable for the UI types.** The Il2CppInterop generic method store
  (`MethodInfoStoreGeneric_AddComponent_Public_T_0`) throws in its type initializer for
  `RectTransform`/`Canvas`/`Image`/`TMP`. `Instantiate` of the game's own root canvas, Images,
  UIButtons and TMP texts works and is the route the screen uses. The root clone keeps the game's
  `CanvasScaler`, `GraphicRaycaster` and the scene's EventSystem; only Canvas/CanvasScaler/
  GraphicRaycaster survive the component filter.
- `CanvasGroup.blocksRaycasts` is **read-only** in the interop; the root CanvasGroup is removed by
  the filter instead of being configured.
- **The splash flow needs the save-slot selection.** `UISplashForm.AdvanceFlow()` alone reached
  `SAVE_SLOT` but never selected a slot, so `SaveDataManager` stayed empty and the menu's Play
  button threw a `NullReferenceException` inside its click. `OnSaveSlotSelected(ESaveSlot.Slot1)`
  is the working path; the test profile's main save is Slot1.
- **The menu Play press needs the managers and a settle.** Wait for
  `UIMenuSplashForm.saveDataManager.SaveData != null` and `metaManager != null`, then 2 s, then call
  `UIMenuSplashForm.OnPlayButton()` directly.
- **The Play form's mission road loads asynchronously.** A single `ReadMenuData` immediately after
  opening saw `sectors=0`. `CoopLobby.EnsureMenuData` retries on the main thread until the road has
  sectors; only then does it send the first `LobbyState`. The panel rebuilds when `UiDataReady`
  flips.
- **Button actions:** `UnityEngine.UI.Button.onClick` cannot take a managed `Action` directly.
  `DelegateSupport.ConvertDelegate<UnityAction>(new Action(...))` + `onClick.AddListener` works; the
  L2 test invokes the exact `Button.onClick` a pointer click reaches. The cloned button's original
  `UIButton` component is destroyed and its event replaced, so a cloned Back button cannot fire the
  game's Back.

Unknowns (honest):

- The screen is verified structurally and by programmatic clicks. A physical mouse click on the
  lobby buttons has not been separately exercised; it goes through the same `Button.onClick`, but
  the EventSystem/pointer path is not independently proven.
- `ScreenCapture.CaptureScreenshot` did not produce a file in this environment (no exception was
  logged either). A visual screenshot/layout review is still open; the screen is built from the
  game's own assets and the test proves it is active and interactive.
- The host's default class came from `runSettingsManager.selectedClass` (Engineer in this run), not
  a fixed class. Both players happened to choose Engineer/Maintenance in the positive run; a
  different-class end-to-end run belongs to L3.

### L3 — start through the real wizard (2026-10-20)

L2's START now drives the game's own `UIPlaySelectForm` with the lobby's selection and creates a
real run. Test `-cooplobbystart` / `-cooptest=lobby-start`, run
`20261020-064751-lobby-start`, `-Ui -Lobby -Test lobby-start`:

```
RESULT: lobby-start PASS host   active=1 sectors=5 classes=5 mods=3 hazard=1 start=1 rev=2
  run=1 dwarf=0 mod='ClassArtifactData_Driller_Interrogator'
RESULT: lobby-start PASS client active=1 stateRecv=2 choiceSent=6 start=1 rev=2
  run=1 dwarf=1 mod='ClassArtifactData_Engineer_Maintenance'
saves: restored and verified (14 files, canary removed)
```

- **Both run settings are the host's lobby choice:** `main=0 biome=0 sector=0 node=0 hazard=1`
  on both sides; host class Driller/Interrogator, client Engineer/Maintenance. Same level,
  different classes.
- **The driver is page-driven, not step-indexed.** The observed order was
  `CAMPAIGN -> CLASS -> NODE -> CLASS -> ARTIFACT -> HAZ`: the game reused the previous
  mission/sector and revisited the class page. Each tick reads the active buttons and presses the
  one carrying the chosen object: sector/node by name, class by dwarf name, class mod by
  `UIClassArtifactButton.data.name`, hazard by `UIHazLevelButton.hazLevel`. The L0/L2 button path
  is the only reliable route, so the driver never calls the handler API directly.
- **Loading form:** the wizard hands over to `UILoadingForm`; the automation continues it with
  `UILoadingForm.OnLoadingComplete()` while `waitingForInput` is true. That is what lets the run
  reach `GameController.StartGame` and the `CoopLoad` barrier. In normal play a key press does
  this; the P2 load-screen work will hold the form with "Fertig geladen - warte auf Spieler 2"
  instead of continuing immediately.
- **Host validation:** `HostStartClicked` checks the client's `PeerDwarf`/`PeerClassMod` against
  `SaveDataManager.IsClassUnlocked`/`IsUnlocked` before sending `LobbyStart`. The client's screen
  already filters the choices, so this is a wire-level safety net.
- **Test timing:** `lobby-start` waits for `ProbeState.Gc != null` plus 5 s before finishing, so
  `run=1` means a real level, not just the loading form or a start packet.
- **Still open:** a visual/layout review of the lobby screen. The screen is functionally verified
  (data, live updates, ready gate, start, programmatic `Button.onClick`); a physical pointer click
  and screenshot are not independently proven. The next visual polish list (turret firing VFX,
  beam visual, held weapon prop, Bosco/turret entity+animation sync, grenade visual-only) is
  unchanged from the last handoff.

### P2 — loading-screen hold (2026-10-20)

The load-screen requirement is implemented. `CoopLoad` caches the active `UILoadingForm` while
the run is armed, pauses at `StartGame`, re-shows the same form over the paused game, sets
`tipText = "Fertig geladen - warte auf Spieler 2"`, clears the press-any-key prompt, and hides the
form only when the barrier releases. `-cooploadformoff` / `-LoadFormOff` is the negative control.

Positive run `20261020-123303-loadform` PASS both roles:

```
check: load started role=host delay=0.0s timeout=30s
check: load form shown=1 role=host text='Fertig geladen - warte auf Spieler 2' paused=1
check: load held role=host paused=1 form=1 delay=0.0s
check: load released role=host via=both held=1.4s peer=1 timeout=0 paused=1 wait=1
RESULT: loadform PASS reason=host ... run=1 wait=1
```

Negative run `20261020-123406-loadform` (`-LoadFormOff`): both `FAIL reason=loadform wait=0`,
`check: load held ... form=0`. So the test is not vacuous.

Measured/derived:

- The real form survives the game's completion; `SetVisibility(true, true)` re-shows it and the
  `tipText` assignment takes effect. No custom overlay was needed.
- The form's `locPressAnyKey` is a Unity Localization `LocalizedString`, not a TMP; setting
  `pressAnyKeyText.text = ""` is the working prompt hide.
- The hold lasts until the host's `PeerReady`, i.e. exactly the peer-load delta (1.4 s in this
  run). The game is paused behind the form, so the level is never visible while waiting.
- The `loadform` test waits for `ProbeState.Gc` + 5 s and requires `CoopLoad.WaitShownEver`, so a
  PASS means the real run started **and** the wait screen was actually shown.
- A visual check of the form's exact layout (title/logo, tip placement) is still open; the log
  proves it was visible and the text was set.

## Bosco mirror — done (2026-10-20)

Both players have their own live `Bosco`; the client uses its own as the visual source for the
host's companion.

- Packet 41 `BoscoState` at 10 Hz: position x/y/z, yaw (ushort), `state`, `speedFrac`, slowing/
  active flags. The host finds its live `Bosco` (`player.Pointer == Player.Pointer`, enabled) and
  sends position/yaw/state/speed/flags.
- The client drains the queue, clones its **own** live Bosco once, disables the `Bosco` component,
  the `WeaponHandler`s and the `Collider`s, keeps the `Animator`, applies position/yaw and sets
  `ID_SPEED`/`ID_BRAKE`; `ID_SALUTE` fires on a state transition. The clone is visual-only and
  named `CoopPuppetBosco`; the client's own Bosco stays visible.
- Positive `20261020-134658-bosco`: host `RESULT: bosco PASS sent=164`; client `recv=173 spawned=1
  statesApplied=194 renderers=3 miss=0`; launcher `RESULT: PASS`.
- Negative `-BoscoOff` `20261020-134912-bosco`: both FAIL `sent=0 recv=0 spawned=0 statesApplied=0
  renderers=0`; launcher `RESULT: FAIL`. `CoopBosco.Tick` runs `MaybeTest` even when disabled, so
  the negative produces the Bosco verdict instead of a timeout.
- Limits: the clone animation is approximate (speed/brake/salute; no full lean/vertical
  parameters); the clone carries no host Bosco damage/behaviour. The client's own Bosco remains
  authoritative for its own actions. A visual check of the clone is still open.

## Co-op UI pass — BACK fixed, marker rebuilt (VERIFIED, 2026-10-20)

Design decisions for this pass: scope is LAN/direct IP plus the in-run marker; **no partner
panel**; visual proof via external window captures. The full plan is
`~/.commandcode/plans/coop-ui-plan.md` (private).

**Built:**

- **BACK in the mission lobby is no longer a no-op.** `CoopLobby.LeaveLobby()` sets `LeftLobby`, and
  `CoopLobbyUI.Tick` refuses to rebuild while it is set. The packet/ready/start path keeps running
  (`TickUiWithoutScreen`), so a client that pressed BACK still receives `LobbyStart`; only the screen
  is closed. Reset on `OnLobbyOpen` / menu entry / explicit Host/Join.
- **Marker (`CoopIndicator`):**
  - explicit states `off | no-remote | no-camera | stale | suppressed | onscreen | offscreen | downed`;
  - direction glyph (`<<`, `>>`, `^^`, `vv`) + distance in metres;
  - downed partner: red marker with `DOWN`;
  - stale peer: `CoopNet.RemoteStateSeq` is stamped on the main thread; after 2 s without a
    `PlayerState` the marker disappears instead of leaving a phantom;
  - suppressed while the lobby, the load hold, START or a paused (modal) game screen is up;
  - bounds come from `CoopScreen.ClientSize()` (Win32 `GetClientRect`), not `Screen.width/height`;
  - only `CameraController.instance.cam` is accepted - the old `Camera.main` fallback is the UI
    camera in this build and made every remote dwarf look off-screen.
  - `-coopindicatoroff` is the negative control.
- **New files:** `CoopScreen.cs` (visible client size) and `MarkerMath.cs` (pure marker geometry:
  on-screen test, behind-camera flip, edge intersection). Geometry is separable from drawing so it can
  be tested without a frame.

**Interop limits found by compiling (do not re-try):**

- `Mathf.Rad2Deg`, `GUIUtility.RotateAroundPivot`, `GUIStyle(…)` (only the `IntPtr` constructor),
  `TextAnchor` (needs `UnityEngine.TextRenderingModule`) and `GUI.DrawTexture` (neither 2- nor
  4-argument overload) are **absent** from this build's interop. The marker therefore draws a text
  glyph in a default `GUI.Box`; no texture and no rotation.

**Build:** `dotnet build -c Release` succeeds (0 errors, the 7 pre-existing warnings).

**Verified on two instances (2026-10-20):**

- **Marker, positive** (`20261020-172040-indicator`, `-Test indicator -CameraOff`), PASS both roles:
  ```
  host   RESULT: indicator PASS state=offscreen onscreen=2 offscreen=1303 selfOnScreen=1
         downed=0 noCamera=0 errors=0 maxDist=23,1 enabled=1
  client RESULT: indicator PASS state=offscreen onscreen=0 offscreen=1324 selfOnScreen=1
         downed=0 noCamera=0 errors=0 maxDist=22,3 enabled=1
  ```
  The "on-screen" half is asserted against the **local** dwarf (`selfOnScreen=1`), which the camera
  follows; the remote half is the real off-screen sample. Requiring the remote to be visible as well
  would have measured the fixture: the first attempt failed with `onscreen=0` for exactly that reason.
- **Marker, negative control** (`20261020-172202-indicator`, `-IndicatorOff`), FAIL both roles:
  `state=off … enabled=0 maxDist=0,0` - the marker is the thing under test.
- **BACK** (`20261020-172235-lobby-back`), PASS both roles:
  `RESULT: lobby-back PASS back=1 active=0 left=1 peer=1`. Before the fix the screen was rebuilt on
  the next tick, which is now the failing signature (`active=1`).
- **Regression - lobby UI** (`20261020-172313-lobby-ui`), PASS both:
  `host active=1 sectors=5 classes=5 mods=3 hazard=1 start=1 rev=2`.
- **Regression - start through the wizard** (`20261020-172401-lobby-start`), PASS both:
  `run=1`, host Driller/Interrogator, client Engineer/Maintenance, same `hazard=1`.
- Every run restored and verified the saves; the launcher stopped both processes.

Still open in this pass: the stale/interrupt and downed marker states were not exercised (the test
window had god mode on and a connected peer throughout); a dedicated run with `-NoGod` and a forced
peer silence is the next measurement. The external screenshot oracle (U6) is also still unwritten.

## Lobby Images, marker states and connect screen — closed (2026-10-20, evening)

### Why the cloned Images were invisible

Measured with a post-activation log (`check: lobby postactivate crComp ...`): after the deferred
`Destroy` every clone that went through `CloneImage`/`StripExtras` had **no Image** left
(`img0/cr1`), while the unstripped control kept it (`img1/cr1`). The CanvasRenderer survived, so the
earlier hypothesis (renderer destroyed) was wrong. Type checks (`c is T`, `IsInstanceOfType`) do not
work on components that `GetComponents<Component>()` returns through the base `Component` wrapper in
this Il2CppInterop build. The fix compares native `.Pointer` values and resolves keep-types with
`GetComponentsInChildren(Il2CppType.From(t))`. A second, independent bug: a root canvas stretched with
anchors 0/1 resolves to 0x0 and now gets the client size.

- Probe run `20261020-200837-lobbyhold`: swatches drawn (plain tinted quad, template material,
  sprite null). A clone that keeps the sprite `Btn_Campaign_Background` renders almost black, so
  production keeps `sprite = null`.
- Production capture `20261020-202207-lobbyhold`: opaque panel, text and buttons, both roles.

### Pixel oracle

`coop-shot.ps1 -VerifyPanel <png,...>` samples an empty zone of the panel (x 55-80 %, y 56-64 % of the
window) and requires mean luminance 8-22 and std <= 3. Calibration: good capture mean 13.2 / std 0.00;
known-bad captures (mission art shining through) mean 56 / std 25-27, both fail. The same script now
tiles the host window left and the client right before capturing: with overlapping windows both
"roles" were the same (front) window.

### Regressions after the Image/root-size fix (all PASS on both roles, saves restored and verified)

| Test | Run |
|---|---|
| `menu` | `20261020-201108-menu` |
| `lobby-ui` | `20261020-201132-lobby-ui` |
| `lobby-start` | `20261020-201214-lobby-start` |
| `lobby-ui -LobbyUIOff` (negative) | `20261020-201926-lobby-ui`: FAIL, `timeout after 420s` |

The host log of `lobby-start` contains `profile: read 'animator' failed`, which already appears in
older runs (e.g. `20261020-064751-lobby-start`); not related to this change.

### Marker `stale` / `downed` (`-Test marker`)

Flags: `-coopstatesilent=<s>` (client stops sending `PlayerState` for `<s>` seconds, starting 6 s after
the load barrier releases), `-coopdownself` (client calls the game's real `Player.Die` at 16 s, god
mode off), launcher `-StateSilent <s> -DownSelf`, always with `-CameraOff` so the partner is off-screen.
Host verdict needs `stale>0`, `downed>=5` samples and `recovered>=1` (the marker left `stale` for a
fresh state once packets resumed) within 45 s; the client verdict only proves it did what it was told.

- Positive `20261020-202736-marker`: host `state=downed stale=352 downed=5 recovered=1 enabled=1`,
  client `silenceSkipped=95 selfDowned=1 isDownedAtDeath=1`.
- Negative `20261020-202927-marker` (`-IndicatorOff`): host FAIL `state=off stale=0 downed=0
  recovered=0 enabled=0`.
- A first run (`20261020-202527-marker`) had a host PASS but a client FAIL: the test read
  `CoopDowned.IsDowned` 16 s after death, when the dwarf had stood up again. The test was fixed to
  capture the flag at the moment of death, not relaxed.
- Not covered here: a revive during the same run (covered by the earlier downed/revive row).

### Connect screen (`-Test connecthold`)

The connect screen normally disappears within about a second once the peer is up. The test keeps it
open for 20 s with the peer connected. Run `20261020-203105-connecthold`: `connect=1 hold=20s peer=1`
on both roles; the captures show `PEER CONNECTED (0 MS) - OPENING THE MISSION MENU...`. Note: the
screen prints the full path of `coop-join.txt`, which contains the Windows user name, so these
captures must not be published.

## Partner lost mid-run (2026-10-20, night)

### Measured first (`-coopnopeerloss` = the old behaviour, kept as baseline and negative control)

The disconnect handler only cleared `PeerConnected`; `HaveRemote` was never reset. With the client
leaving 20 s into the run (`-Test disconnect -QuitAfter 20`), the surviving host showed:
`haveRemote=1`, puppet still standing at its last position, marker `stale`, no message
(`20261020-205716`, FAIL). With the leaver silent in a level-up, the host stayed frozen:
`xpHold=1`, `timeAdvanced=0,0` for the whole 16 s window (`20261020-210213`, FAIL); the only way out was
the 45 s timeout. Detection is LiteNetLib's disconnect timeout (`reason=Timeout`, roughly 5-10 s after
the process is gone, since a killed game sends no disconnect).

### Fix (`CoopPeerLoss.cs`, hooks in `CoopNet`, `CoopXp`, `CoopShop`, `CoopLoad`)

On the main thread, at detection: `HaveRemote=false` (so later barriers pass and "no partner" counts as
solo for the downed rule), a holding level-up/shop/load barrier is released immediately, the puppet is
removed, the marker falls back to `no-remote`, and a notice is drawn ("Partner getrennt - du spielst solo
weiter" / "Host getrennt - der Run wird beendet"). The client cannot continue without the host, so it
ends the run through `CoreGameEvents.TriggerCoreRunOutro(ABORT)` after 6 s, unless an end screen is
already up. Nothing here reconnects; a later `PlayerState` would set `HaveRemote` again, but no
reconnect flow exists.

Test aids: `-coopquitafter=<s>`, `-coopquitonlevelup` (never confirms the level-up, quits 3 s later),
`-coopnopeerloss`; launcher `-QuitAfter`, `-QuitOnLevelUp`, `-QuitterRole`, `-NoPeerLoss`.
`-Test disconnect`: the leaver writes PASS and exits, the survivor judges (`haveRemote=0`, no barrier
holding, puppet gone, marker `no-remote`, notice shown; host also alive and time advancing; client
`runEnded=1`; a barrier that was holding must release within 2 s of detection).

| Case | Run | Result |
|---|---|---|
| client leaves, host survives | `20261020-212227-disconnect` | PASS |
| client leaves in a level-up, host holding | `20261020-212435-disconnect` | PASS, `holdReleasedAfter=0,0` |
| host leaves, client survives | `20261020-214714-disconnect` | PASS, `runEnded=1` |
| host forces run end, then leaves | `20261020-214936-disconnect` | PASS (end screen already up, no second outro) |
| negative, host survivor, `-NoPeerLoss` | `20261020-205716`, `20261020-210213` | FAIL |
| negative, client survivor, `-NoPeerLoss` | `20261020-211301` | FAIL |

The notice is readable on an external capture (`CopyFromScreen`, host window right after the loss).
IMGUI limit worth knowing: a default `GUI.Box` is translucent and its text dim over the game's HUD, so
the notice uses an opaque backdrop, a larger label font (restored after drawing) and three overdraws.

### Regression after the change (all PASS on both roles, saves restored and verified)

`xpsync` `20261020-213321`, `stage -StageEnd win` `20261020-213458`, `shop -StageEnd win`
`20261020-213635`, `loadform` `20261020-213716`, `loadtimeout -Only host` `20261020-213833`,
`lobby-start` `20261020-213914`, `lobby-back` `20261020-213942`, `marker` `20261020-214119`,
`indicator` `20261020-214235`, `levelup -TestMinSec 120` `20261020-214453`.

One `levelup` run failed first (`20261020-213147`, `timeout after 420s`): the bot dwarf stood still from
minute 4 inside the swarm at level 2, so the second pick never happened; no exception and no peer loss in
the run. The immediate re-run passed, so this is recorded as a flaky bot position, not as a fix.

Log lines seen in these runs that are **not** new: `profile: read 'animator' failed ... NullReference`
(host, also in runs from the 20th before this change) and `loot: wallet credit failed` (client, also in
runs from 2026-10-19).

### Not covered

Reconnect mid-run; Pause/ESC while a partner is connected (not measured, first item of the manual test);
a partner that stays connected but stops answering (only the marker going `stale` is measured, see the marker test; how the barriers behave then is unmeasured).
