# Prince of Persia: The Lost Crown — working notes

Owner session start: 2026-11-13. Game: Ubisoft Montpellier, 2024, metroidvania (2.5D).

## Observations (checked on this machine)

- Install folder: `D:\Games\Prince of Persia - The Lost Crown` (26.7 GB), *not* in a Steam library.
  Steam libraries present: `C:\Program Files (x86)\Steam`, `D:\SteamLibrary` (Lords of the Fallen,
  Steam Linux Runtime 4.0). Game is not a Steam install.
- Root files: `TheLostCrown.exe`, `UnityPlayer.dll` (29 MB), `GameAssembly.dll` (250 MB),
  `TheLostCrown_Data\`, `_Redist\`, `unins000.exe`, `voices38.dll`.
- Engine: **Unity 2021.3.29f1, IL2CPP (x64)**. `UnityPlayer.dll` FileVersion 2021.3.29.0;
  `TheLostCrown.exe` ProductVersion "2021.3.29f1 (204d6dc9ae1c)".
- `TheLostCrown.exe` SHA-256: `447BC0A6FB6B36E70F23DE9B6E9E4D8DDE128C2EA1427E93F69AC6B4997B0ED8`
- `TheLostCrown_Data/app.info`: "Ubisoft" / "Prince Of Persia The Lost Crown". No store build id found there.
- `ScriptingAssemblies.json` shows a large in-house Ubisoft layer on top of Unity:
  `Alkawa.Engine`, `Alkawa.Gameplay`, `Alkawa.Core`, `Alkawa.Gameplay.DLC1`, `Alkawa.Gameplay.CU3`,
  `Alkawa.OnlineServices`, `Alkawa.ObjectRegister`, `Ubisoft.Bloomberg.dll`, `com.ubisoft.uplay.dll`,
  plus Cinemachine, Animancer, Unity Addressables (`StreamingAssets/aa`), Wwise, InputSystem.
- Player machine: Ryzen 5 5600, RX 9060 XT, 32 GB RAM. Disk free: C 21.8 GB, D 49.3 GB.
  No .NET SDK installed (`dotnet --list-sdks` empty).

## Derived

- IL2CPP + no gameplay netcode (`Alkawa.OnlineServices` = Ubisoft Connect services, not replication)
  → mod path is BepInEx 6 (IL2CPP) + HarmonyX + Il2CppInterop, i.e. Path 2 "build from zero".
- The install looks like a repack (`voices38.dll`, Inno Setup uninstaller), not a store build.
  Build identity therefore unverified against Steam/Ubisoft Connect → mod compatibility with the
  official store build is unknown until tested on one.

## Prior art (external)

- BepInEx 6 IL2CPP does load this game: Nexus "cheat mod" (BepInEx 6.0.0-pre.2, IL2CPP win-x64)
  and Thai localization mod (6.0.0-be.785).
- p1xel8ted/UltrawideFixes has a working PoP:TLC plugin (`src/PrinceOfPersiaTheLostCrown`) that
  Harmony-patches `SceneManager.Internal_SceneLoaded`, `VideoController`, `CanvasScaler` and
  enumerates `Camera`s per scene → camera control from a plugin is proven feasible.
  https://github.com/p1xel8ted/UltrawideFixes
- No co-op/multiplayer mod for this game found (GitHub code+repo search, web search).
- Same-genre precedent in DIRECTORY.md: Hollow Knight HKMP (Unity, 2D metroidvania) ships
  "players visible, no enemy/story/world sync". CelesteNet / Silksong SilklessCoop similar scope.

## Directory status

- No entry for this game in `data/directory.yaml`, no `games/` write-up (folder was empty before
  this session). No issues or PRs mentioning it (`gh issue list` / `gh pr list --search`).

## Progress (M0, 2026-11-13)
Design: `docs/superpowers/specs/2026-11-13-pop-lost-crown-coop-design.md` (approved).
Plan: `docs/superpowers/plans/2026-11-13-popcoop-m0-shell-and-discovery.md`.
Mod repo: `D:\New folder (2)\popcoop`, branch `feat/m0`.

Done, headless (no game launch, owner's install untouched):

- Workspace-local toolchain pinned with SHA-256 in `local/TOOLCHAIN.md` of the mod repo:
  .NET SDK 8.0.428, BepInEx 6.0.0-be.793 (IL2CPP, win-x64), Il2CppDumper 6.7.46.
- Test install `D:\popcoop-game`: root files copied (285 MB), `TheLostCrown_Data` junctioned
  to the owner's install, BepInEx core in place.
- `ContentFingerprint` — `v1-<16 hex>` over game version + `global-metadata.dat` hash +
  `catalog.json` hash. Golden vector for this build: `v1-1184cca478c23412`.
- `EntityKey` — FNV-1a 64 static object ids from level key + normalised scene path, with a
  100k-path collision test.
- Bridge server in C# speaking the repo's protocol v1 on loopback, driven successfully by
  `tools/bridge/bridge_client.py` (`ping`, `version`, `exec`). 19 tests green.

Parked because the owner was using the PC (no game launch, no heavy CPU): the IL2CPP type
dump, offline Il2CppInterop generation, and the first in-game plugin boot.

### In-game milestone (2026-11-13, after owner freed the PC)

- Type dump done: metadata v29, 129 assemblies, `dump.cs` 30 MB, `script.json` 92 MB
  (addresses for hooking). Findings in `TYPES.md`.
- **BepInEx 6.0.0-be.793 fails on this build**: `Il2CppInterop ... GenericMethod_GetMethod_Hook
  .FindTargetMethod()` throws `Sequence contains more than one element`, chainloader aborts
  before any plugin. Same failure with 6.0.0-pre.2.
- **Workaround that works**: BepInEx 6.0.0-pre.2 with `[Logging] UnityLogListening = false`
  in `BepInEx/config/BepInEx.cfg`. With that, `Chainloader startup complete`, the plugin
  loads, and the bridge answers from inside the running game (`ping`, `version`, `exec scene`
  → `Bootstrap`).
- **ClassInjector is also broken** on this build (same `Single()` failure inside
  `RegisterTypeInIl2Cpp`), so custom `MonoBehaviour`s cannot be injected: no `AddComponent`
  frame pump, no injection-based spawning. The pump must hang off an existing per-frame game
  method via Harmony instead. This is the first task of M1.
- `findplayer` returned `count=0` in the `Bootstrap` scene; player discovery has not yet been
  observed in a loaded level.

### Harness milestone (same session)

- Game build id, read from the title screen: **`1.4.3+TU16.421692.3046900`**. Record it with the
  content fingerprint.
- **Main-thread pump works.** `Time.get_deltaTime` is not managed here, so the pump hangs off
  Harmony postfixes on `InputSystem.Update(InputUpdateType)`, `LoadSaveManager.AlkawaUpdate`
  and `Camera.get_main`. Verified: `frames` 101 → 326 over 2 s, and `exec` commands no longer
  time out. (Exact winning hook not isolated yet; all three are cheap.)
- Bridge now speaks: `scene`, `findplayer`, `ticks`, `slots`, `newgame`, `load`,
  `focus`, `key`, `click`, `shot`, `anykey`.
- **Save slots are readable** through `LoadSaveManager.BuildSaveGameDisplayInformation`:
  `Game1..3` all answer with the blank-slot title, so no save exists yet. The run created no
  save files, and the screenshots it wrote were deleted afterwards.
- **Blocker: automated menu entry.** `SetForegroundWindow` genuinely takes the foreground
  (foreground handle == game handle) but `SendInput` keystrokes do not advance the
  "Press Any Button" screen, and calling `UIScriptPressAnyButton.OnInputPressed(...)` throws
  `NullReferenceException` inside game code (with a real `KeyControl` and with `null`).
  `UIScriptSaveSlot.QuickNewGame()` needs that screen to exist first.
  Route not yet tried: Harmony-inject the flow transition, or drive the `GameFlowState`
  timeline event that the title screen listens to.

### M1 gate: two bodies exist (same session, solved)

- The menu blocker was **not** a game problem: the game ran fullscreen, so nothing could be
  clicked. Launched windowed (`-screen-fullscreen 0 -screen-width 1600 -screen-height 900`) and
  driven with real desktop clicks (Windows MCP), a new game reaches gameplay normally.
- Player object: GameObject `New Player`, component `Alkawa.Gameplay.Player.Components.PlayerComponent`,
  in scene `KIN_BAT_02` at `28.82, 6.49, 0.00`.
- `AlkawaBehaviour.transform` throws `"transform access is forbidden"` by design; go through
  `component.Cast<MonoBehaviour>().gameObject.transform`.
- `cloneplayer` = `Object.Instantiate` of the live body, renamed `PopCoop Player 2` → `count=2`,
  both active, same scene. **The second body can exist.** Still unverified: whether it renders,
  what drives its animation, and how input reaches it (it was spawned during a cutscene).

### M1 confirmed visually

- With a save loaded in `KIN_BAT_02`, `cloneplayer` + a screenshot shows **two Sargons standing
  side by side, both animated**.
- `move2 3` moved only the clone (30.82 → 33.82); the original stayed at 29.49 and the clone was
  not snapped back. So the second body renders and holds its own transform.
- Open for M1 completion: input routing to the second body, collision, camera behaviour, and
  whether its animator is driven by the original's state.

### M1 complete (automated)

- Player rig, read at runtime: **no `CharacterController`, no `PlayerInput`**; `Rigidbody` on the
  root, `Animator` and `CapsuleCollider` on children. Movement is therefore driven by writing
  `Rigidbody.velocity`, not by `CharacterController.Move`.
- `walk2 3 2` moved the clone 30.82 → 36.82 (exactly 3 m/s × 2 s) with physics stepping and
  collision, while the original stayed at 28.83. Physics-driven player 2 works.
- **Camera follows the local player only**; player 2 walks out of frame. Online co-op is
  unaffected (each PC has its own camera); split-screen would need real work.
- `auto on` spawns and tints player 2 on the frame tick; tint is re-applied every 30 frames for
  ~5 s after spawn because the skin system swaps materials when a shot starts.
- `tools/smoke-test.ps1` runs the whole chain and passes: launch → title → save slot → gameplay →
  `auto on` → `count=2` → tint verified numerically (`_BaseColorPremult=(0.15,0.75,1.00)`,
  `emission=1`) → screenshot.
- **Crash fixed:** the pump originally hooked `InputSystem.Update`, which can run on the input
  thread; draining the command queue there touched Unity objects off the main thread and killed
  the game during scene loads. Removed; the pump now rides `LoadSaveManager.AlkawaUpdate` and
  `Camera.get_main` only.
- **Save location is still unknown.** No save files appear under `LocalLow`, `Documents`,
  `Saved Games`, `%LOCALAPPDATA%` or the game folder, yet the slot shows "Battleground 0.33%".
  Finding the actual container is the first task for the co-op-profile work.

### M2 started: transport

- `PopCoop.Net` (new project): UDP peer, hello handshake carrying the content fingerprint,
  ack/reject, 21-byte snapshot packets with sequence numbers. **24 tests green**, including a real
  loopback handshake, rejection of a mismatched fingerprint, and garbage-packet tolerance.
- In-game commands: `net host <port>`, `net join <ip> <port>`, `net status`, `net stop`. The local
  hero's transform is streamed each frame; the received snapshot drives the cloned body with a lerp.
- `tools/netprobe.py` acts as a fake peer for testing the receive path with one install.
- Second install `D:\popcoop-game2` (junction to the same data folder) exists for a two-client test.
- **Not yet demonstrated: two live game clients syncing.** Attempts were blocked by Windows Firewall
  prompts, window-focus fights and one game exit. Firewall rules for `TheLostCrown.exe` are now in
  place, so the next attempt starts clean.
- Fresh installs need `UnityLogListening = false` in `BepInEx.cfg`, otherwise the chainloader dies in
  Il2CppInterop; disk logging must be on or failures are silent.

### M3 first slice: enemy replication

- `MsgEnemies` packet (type 5): sequence, count, then count × (x,y,z).
- The host streams AI transforms every 10 ticks after `net enemies on`; `net aiscan` reports the
  scene (`KIN_BAT_02` has zero AI — it is the intro battlefield).
- The wire is proven with a synthetic source (`net fakeenemies on`): a listening peer received
  70 frames in 6 s carrying 3 entities with sane positions. Real AI uses the same path and needs a
  combat scene to verify.
- **Two live clients on one Windows session are impossible with this loader**: whichever game
  process starts first gets BepInEx; the second never loads it (no bridge, no log) — confirmed by
  starting each install alone and then both. A second session/VM or a loader fix is required.

## Session 2 (2026-11-13): save store solved, two-client root cause corrected

### Save store (observation)

- The game's save is `%APPDATA%\GSE Saves\2638890\remote\win64_save\data001Slot.bin`
  (3,317,100 bytes; header `DSSS`; mtime 2026-11-13 14:18; SHA-256
  `B34D9065C1893168560A0D600BF426578A160AEA39803717A858F305D445A4AD`).
  Backed up to `popcoop/local/save-backup/data001Slot.bin`.
- `%APPDATA%\GSE Saves\settings\configs.user.ini` holds the Goldberg emulator account
  (`account_steamid=76561198745311075`). `GSE Saves` is the Goldberg Steam-emulator save
  layout, so this repack routes saves through the emulator rather than Ubisoft Connect.
- `%USERPROFILE%\AppData\LocalLow\Ubisoft\Prince Of Persia The Lost Crown` holds only
  `Player.log`, `Player-prev.log` and `RecordedInputs` (the game's input recorder), no saves.
- `slots` shows Game1 and Game3 both as `Battleground / Persia Under Attack`, Game2 blank:
  that is the stray slot 3 the owner wants removed.

### Two live clients on one machine (observation + derived)

- Both installs load BepInEx: the second client writes its bridge ~5 s after launch. The
  earlier note "the second never loads the preloader" is wrong.
- The game then shows a native modal titled **`Game already running!`**. The first client
  holds the named mutant `\BaseNamedObjects\85928750e68698ea4c99b38be5f9950d` (seen with
  Sysinternals `handle64`). Closing that handle with a `DuplicateHandle(...,
  DUPLICATE_CLOSE_SOURCE)` tool lets the second client get past the dialog and load the
  plugin, but the second game still exits cleanly a few seconds later.
- Renaming the second `TheLostCrown.exe` makes the crack show an `Error` dialog (derived:
  the crack checks the executable name), so renaming is not a workaround.
- Closing only the mutant is not sufficient; a second guard (window/process check or the
  shared GSE save) still ends the second process. Local two-client needs a separate
  Windows session/VM or a game patch; two machines remain the intended path.
- Negative result for save profiles: setting `%APPDATA%` per process (the `-Profile` flag in
  `run-game.ps1`) does **not** redirect the GSE save store. With the profile's save hidden,
  `slots` still showed the owner's `Battleground` entries, i.e. the game resolves APPDATA
  through the Windows known-folder API. The flag is appdata-only, not save isolation.
- The mod itself still works in a profiled launch: `auto on` -> `count=2`, tint
  `_BaseColorPremult=(0.15,0.75,1.00)`, screenshot written.

## Unknowns
- Official store build number / whether owner's copy matches Steam or Ubisoft Connect.
- Whether the game has any latent second-player support (not checked inside the binary).
- Whether a separate Windows session isolates the single-instance mutant; not tested.
- Scene/level structure inside `StreamingAssets/aa` addressables.

## Session 4 (2026-11-13): two live clients, guard solved, remote locomotion animation

Repo: `D:\New folder (2)\popcoop`, branch `feat/m0`, commit `b25a251` (handover:
`HANDOVER-2026-11-13-session4.md`). Build 0 warnings, 28/28 tests.

### Single-instance guard solved (observation + static RE)

- The `Game already running!` dialog is the game's own managed code, not the crack:
  `Alkawa.Engine.UbiconnectPCManager` (C# namespace `Alkawa.Engine`, interop image
  `Alkawa.Engine.Windows.dll`), `public static class UbiconnectPCManager`:
  - constant `ANTI_CONCURRENCY_MUTEX_NAME = "Global\7b2c96eb-fc18-48de-947e-9b67534a8f03"`,
    field `antiGameConcurrencyMutex`;
  - `PreInitialize()` RVA `0x51DA60` creates that mutex with `WindowsInvoke.Mutex..ctor`; when
    CreateMutex reports `ERROR_ALREADY_EXISTS` it shows the message through
    `WindowsInvoke.DisplayOSError("Game already running!", "It looks like the game \"<exe>\" ...")`
    and skips the normal Ubisoft Connect init (second client then exits/blocks);
  - the decisive branch is RVA `0x51DC03` (`74 05` = je error), followed by `jmp normal`;
    NOP 2 bytes to always take the normal path; a first instance already behaves that way.
- The check runs very early, sometimes before the BepInEx plugin `Load()`; a Harmony patch from
  the plugin therefore races (`POPCOOP_MULTI_INSTANCE=1`, best effort). For local QA the test
  install's `GameAssembly.dll` is patched on disk (`local\tools\guard-patch.py apply|revert`,
  original hash recorded); the owner install is never touched. Two-machine co-op needs no patch.
- The dialog blocks the second client's main thread (bridge accepts no commands) and is visible
  even when the window is minimised; a `#32770` window titled `Game already running!`.

### Two live clients on one Windows session now work

- `tools\two-client-test.ps1` (PASS): A = `D:\popcoop-game`, B = `D:\popcoop-game2`; both
  minimised/off-screen 640x360; user foreground window handed back (`givefocus <hwnd>`); both
  enter `KIN_BAT_02` with no clicks; host/join on 27015; both spawn the remote body; A's movement
  reaches B's clone; cursor and owner save unchanged (`B34D9065...`).
- Background menu entry (game-side, no input): enumerate `UIScreensBehavior.m_UIScreenList[].
  m_UIScriptList` (UIScript is a plain class; `FindObjectsOfType` throws), set
  `UIScriptPressAnyButton.m_hasInputTriggered`, then set static
  `UIScriptMainMenu.LatestSaveLoadRequested` so the game continues the newest save itself
  (`UIScriptMainMenu.Update_Internal` calls `TriggerLatestSaveLoadingOrNot` RVA `0xC32590`).
- BepInEx plugin `Load()` can take ~30 s on the second instance when both run; wait for a bridge
  command reply, not just `bridge.json`.

### Remote body no longer slides

- Player snapshots now carry `MoveX` (25-byte packet; 21-byte legacy parses as idle), read from
  `PlayerInputSubComponent.StateInfo.m_movementInput.x` with a velocity fallback for test walks.
- Receiver caches the clone's `PlayerAnimation` / `PlayerLocomotion` and calls
  `PlayerAnimationSubComponent.Play(EPlayerAnims.Run/Walk/Idle, null, 0, -1)` on state change;
  verified: B reports `anim=Run` while A walks, `anim=Idle` after A stops.
- Animation in this game is Animancer + input-driven (`PlayerLocomotionSubComponent.
  GetLocomActionFromInput` -> `Idle_Old.HandleStart` -> `Play`), not a Unity AnimatorController
  and not velocity-driven; `PlayerAnimationSubComponent.Play` RVA `0xA383A0`.
- Facing is a discrete `Side` + skeletal mirror, not root yaw. `ForceFacing` on the clone crashed
  the client in a receiver probe, so facing sync is gated off; next path is the P1 input
  injection in `local\animation-recon.md`.
- Two full RE reports for the next sessions (in popcoop `local/`, gitignored):
  `interaction-recon.md` (door/GPE/lever/pickup detect+apply RVAs and identity scheme) and
  `animation-recon.md` (locomotion chain, entry points, in-game test plan).

## Session 5 (2026-11-13): clone animations/facing + in-game interaction apply

Repo: `D:\New folder (2)\popcoop`, branch `feat/m0`, commit `aef174b` (handover:
`HANDOVER-2026-11-13-session5.md`). Build 0 warnings, 28/28 tests, `tools\two-client-test.ps1` PASS.

### Remote body animation + facing (works)

- The cloned body is not a registered player: its active ability is `Puppet`. Pure P1 input injection
  (`PlayerLocomotionSubComponent.OnFixedUpdate` postfix writing `ProcessedInputNotLocked`,
  `m_movementInput`, `m_isInputLocked=false`) reaches the clone's locomotion state but the Puppet
  ability ignores it. Hybrid that works: keep the injection; on main-thread ticks play the matching
  clip with `PlayerAnimationSubComponent.Play(EPlayerAnims.Idle/Walk/Run, null, 0, -1)` (RVA
  0xA383A0) when the input bucket changes; flip facing by setting only
  `LocomotionStateInfo.m_MirrorSystem.Mirror` (set_Mirror RVA 0xDC4640). Do not write
  `CharacterFacing` or call `ForceFacing` on the clone.
- **Crash trap found:** never call `Camera.main` from the frame pump. The pump is a Harmony postfix
  on `Camera.get_main`, so `Camera.main` recurses and the client dies silently (native stack
  overflow, no WER/managed exception) within ~1-2 s of the first non-zero remote movement. Fix:
  cache the camera in `CameraMainHook.Postfix(Camera __result)` (`MainThreadPump.MainCamera`).
- Verify with `net animstate` on the receiver: `currentAnim=Run_OutOfCombat`, `mirror=True/False`
  flips with the sender's stick direction, `currentAnim=Idle` after stop.
- Do not re-assert `ForceSetAbility` every N physics frames (scripted-override semantics); it is off
  by default. The intro scene locks hero translation, so verify animation/facing numerically rather
  than by position delta.

### Interaction sync (works in-game)

- Detect: Harmony postfix on `InteractiveElementLogicBase.TriggerLogic(GameObject)` (single funnel
  for all IE logics). Map logic -> owning `InteractiveElementBaseComponent` via the scene scan,
  entity key = FNV-1a 64 over scene name + transform path.
- Apply on the peer (main thread, queued from the net thread): `logic.TriggerLogic(localPlayerGO)`
  (RVA 0x961DF0). `SimulatePlayerInteraction` (0x954150) did not fire for a distant ToggleGPE in
  the intro scene, so direct TriggerLogic with a real instigator is the applied path. `set_Mirror`
  and `TriggerLogic(null)` did not crash once the camera recursion was fixed; keep a real
  instigator for fidelity.
- Filter: do not forward system/flow logics (Checkpoint, Save, Conversation, Notification, Spawn,
  Portal, Trigger, Teleport, DiscoverMinimap, StartQuest, Cutscene, PrepareVideo); each client runs
  them locally, and replaying can re-enter save/cutscene flow. Keep echo suppression (2 s after
  apply, 10 s receive dedupe, 400 ms send throttle).
- Debug commands: `ie scan`, `ie trigger <i> [logic]`, `ie triggerlogic <key>`, `ie triggerkey <key>`,
  `ie applykey <key> [action] [value]`, `ie applied`, `ie gpe`, `ie gpeapply <key> [0|1]`,
  `net intsync on|off`. `KIN_BAT_02` has 19 IEs (3 ToggleGPE levers, 1 WorldPortal, checkpoints,
  quest/cutscene triggers) and 16 GPEs (spawners/breakables/world-streaming); no chest/pickup yet.
- Verified two-client: A triggers ToggleGPE `0x396CD50BFD3EB4EC`, B applies it (`applied=1`,
  same key), reverse direction also applies; state/save/cursor unchanged.

### New questions for the next session

- Chest/pickup apply needs a real level (level transition not solved).
- Direct door/GPE activation path (without a lever IE) is not hooked; `ToggleGPE_External` exists as
  a debug apply.
- Late-join interaction/state replay (event-only sync today).

## Session 6 (2026-11-13): level loading, chest apply in a real level, GPE activation sync, late-join replay

Repo: `D:\New folder (2)\popcoop`, branch `feat/m0` (handover: `HANDOVER-2026-11-13-session6.md`).
Build 0 warnings; 28/28 tests; full two-client test + new `two-client-level-chest.ps1` PASS.

### Level transitions / loading (works for host-initiated loads)

- `UnityEngine.SceneManager.LoadScene` is a dead end: only `Bootstrap.unity` is in build settings;
  all real levels are Addressables scenes driven by world streaming.
- New `LevelTravel.cs`: `level list [filter]` enumerates the 336 `LevelData` entries under
  `GameManager.Instance.MainGameWorld.Biomes[].Areas[].Levels` (with `SceneRef.ScenePath` + GUID);
  `level goto <substring>` mirrors the continue-save path: build
  `WorldStreamingStartSettings{WorldData, StartingLevel, PerformPlayerTeleport=true}`, set
  `GameFlow.WorldStreamingStartSettings`, `GameFlow.PushState(EGameFlowStateType.Loading)`.
  Verified loading `CIT_LOW_02_MysteryChest` (336 levels listed; 271 root levels per the RE report).
- The intro scene's WorldPortal IE (`Root/_PORTALS/Portal_KIN_BAT_02_1`, key
  `0xCC228A01DFD6D25F`) transitions `KIN_BAT_02 -> KIN_BAT_04` simply by running its TriggerLogic.
- Host level-change sync: host polls the active scene and sends interaction action 12
  (`key = FNV-1a(sceneName)`); peer finds the LevelData by scene name and loads it. Verified B
  following A from `KIN_BAT_02` to `CIT_LOW_02_MysteryChest`. Client-initiated portals do not sync
  yet (host authority only).
- Safety trap: while a level transition is in flight, drop remote interaction/state packets and
  skip the clone drive (`Interactables.InSceneTransition` + `CloneDrive.InvalidateCache`), otherwise
  the host dies silently touching objects from the unloading scene.

### Chest/pickup apply in a real level

- `CIT_LOW_02_MysteryChest` contains `InteractiveElementLogic_Chest` key
  `0x277F418013854B94` (puzzle-gated, state NonInteractive until the chakram puzzle is solved).
- Added interaction action 13 (`ie complete <key>`): calls `InteractiveElementLogicBase.JumpToComplete()`
  on every logic of the IE; used for gated chests and state restoration. Two-client test: A completes
  the chest, B applies it (`interact: completed key=0x277F...` in B's log), same scene, both alive.
- Loot/inventory is still per-client local (no shared loot authority yet).

### Direct GPE activation sync (works)

- Harmony-patching `GameplayElementBaseComponent.ToggleGPE_External` crashes the client during scene
  streaming (even an IntPtr-only postfix) - do not. Replaced with main-thread polling:
  `Interactables.PollGpeStates()` compares `m_ActivationStateInfo.m_activationState` every 15 ticks
  and forwards changes as interaction action 11; remote apply uses `ToggleGPE_External(active)`.
  Verified A `ie gpeapply` -> B `gpeApplied=1`, same key/state.

### Late-join / reconnect replay (works)

- `NetPeer.ConnectionEpoch` increments on each completed handshake; the host then sends world state:
  action 10 for non-system IE `InitFromSave(state)`, action 11 for GPE activation states
  (20-21 entries in KIN_BAT_02). Verified: B reconnects and its `stateReplayApplied` grows; lever and
  GPE states match A's.

### Two-machine playtest: closest possible only

- No second PC with the game: instead the full two-client test also passes over the LAN interface
  (`-HostAddress 192.168.8.100`), covering UDP host/join, fingerprint, snapshots, interactions, GPE
  poll/replay and level loads. Firewall inbound rules for `thelostcrown` already exist.
- Two-machine procedure for a later session: same build on PC B (`install.ps1`), host on A
  (`net host 27015`), join `net join <A LAN IP> 27015`; saves are per-machine (GSE emulator); shared
  progression authority still missing; latency/interpolation untuned.

### New bridge commands

`level list [filter]`, `level goto <name>`, `ie complete <key>`, `ie applied` (now includes
`gpeApplied`, `stateReplaySent/Applied`, `levelChangesApplied`), `net statereplay`, `net intstate`,
`flow`. Local tests: `local\tools\two-client-level-chest.ps1` (level load + chest apply),
`level-goto-test.ps1`, `chest-test.ps1`, `portal-chain-explore.ps1`.

### Still open (session 6 end)

- True two-machine playtest; shared save/progression + loot authority.
- Client-initiated portal/level transitions must be routed through host sync; story flow transitions too.
- Chest normal TriggerLogic path needs puzzle completion; only complete/state path verified.
- Combat/AI streaming, story flags, dialogue/cutscenes, ability unlocks, combat authority, slot-3
  removal, installer validation with a second player.

## Session 7 (2026-11-14): client-initiated level sync, loot authority, actor streaming

Repo: `D:\New folder (2)\popcoop`, branch `feat/m0` (handover: `HANDOVER-2026-11-14-session7.md`).
Build 0 warnings; 28/28 tests. Level-sync and actor-wire two-client tests PASS; portal transition
and real-AI verification remain blocked (details below).

### Client-initiated level transitions through the host (verified for level loads; portal blocked)

- Interaction action 14 = client level-change request; action 12 = host level broadcast/heartbeat.
  Client scene change -> send 14 -> host loads requested level -> host broadcasts 12 -> peer loads.
  Host also re-broadcasts its current level every ~10 s (heartbeat), repairing missed packets and
  rapid double transitions; same-scene notifications are ignored and local/remote loads suppress
  their own outgoing notification for a window to avoid ping-pong.
- `LevelTravel.Go/LoadBySceneHash` now refuse to push `Loading` unless `GameFlow` is `InGame`
  (`busy: flow=...`); pushing Loading during a cutscene hangs the client.
- Verified with `local\tools\two-client-level-sync.ps1`: phase 1 client `level goto
  mysterychest` -> host follows (`levelChangesRequested=1`, both in `CIT_LOW_02_MysteryChest`);
  phase 2 host `level goto KIN_BAT_02` -> client follows.
- Portal/story transitions use the same scene-change path, but a real portal transition is still
  blocked: `KIN_BAT_02` WorldPortal -> `KIN_BAT_04` hangs (single-player too), and direct
  `level goto KIN_BAT_04` ends in `flow=Respawn` and stops answering the bridge. Story auto-
  transitions (save now resumes in `KIN_BAT_07` and auto-advances to `KIN_THO_01`) are followed
  naturally by each client and kept aligned by the heartbeat.

### Shared loot/save authority (verified host -> client)

- Client prefix blocks local chest/pickup logic (`Chest`, `CollectibleItem`, `InventoryItem`,
  `Loot`) when connected as a client: it sends action 15 (completion request) to the host instead.
- Host runs the chest logic authoritatively (loot roll only on host), and if complete broadcasts
  action 13; action 13 applies `InitFromSave(Completed)` state only on the peer (no second loot
  roll). Host-local chest triggers also send action 13 (not action 1, which would re-roll loot on
  the client).
- Save/progression authority is still per-machine (GSE save file); only late-join world-state
  replay exists. Two-machine pre-flight should compare save hashes.
- Full `tools/two-client-test.ps1` regression PASS after flow-aware normalisation (cutscene wait +
  normalise to KIN_BAT_02): animation/facing, interactions, direct GPE and late-join replay all
  verified again.
- Verified in-game: `local\tools\two-client-level-chest.ps1` PASS, receiver log
  `interact: state-only chest completion key=0x277F418013854B94` (no local loot roll), host did the
  completion, both clients alive in `CIT_LOW_02_MysteryChest`, owner save unchanged. Client-press ->
  action 15 request path is implemented but not separately exercised (chest is puzzle-gated).

### Real AI / combat actor streaming (wire verified with synthetic actors; real AI blocked)

- New message type 7: actor frame = seq, count, per actor `key (u64), xyz, yaw, hpRatio,
  flags(alive)` (29 B each, up to 12/packet). Host samples real `AIComponent` within 70 m every
  6 ticks (stable scene+path entity keys, `AIHealthStateInfo.m_hpRatio`); receiver tracks
  key -> ActorState; `net actors [on|off]` reports counters/samples; `net fakeactors on` streams
  three synthetic actors for wire tests.
- Verified end-to-end with two clients: host sent 76 frames, peer received 77 frames / 3 actors
  with correct keys, positions, hp and alive flags.
- Real-AI verification blocked: all probed levels report `ai=0` at load (`KIN_THO_01/02/03/04`,
  `KIN_BAT_06/07/08_Boss`, `CIT_LOW_01/11`), `KIN_BAT_02` is input-locked with no AI, and
  `KIN_BAT_04` (the arena) hangs. Once a real combat level with live AI is reachable, the wire is
  ready without further code changes.

### Environment notes

- Save resume now starts in `KIN_BAT_07` (auto-advances to `KIN_THO_01` cutscene, ~60-70 s to
  InGame). `tools\two-client-test.ps1` waits for `flow=InGame` and normalises both clients to
  `KIN_BAT_02` (retrying while `level goto` says busy).
- `KIN_BAT_04` direct loads end in `flow=Respawn`/hang; treat it as broken-for-QA until fixed.
- Owner save hash unchanged (`B34D9065...A4AD`).

### Still open

- Real portal/story transition end-to-end (KIN_BAT_04 hang).
- In-game verification of host-authoritative chest loot (client request + state-only apply).
- Real AI/combat streaming and combat authority (hit/damage/death), enemy rendering on peer,
  late-join actor snapshots.
- Real two-machine playtest; shared save/progression authority; latency tuning.
- Story flags/quests/dialogue/cutscene sync, ability unlock sync, reconnect UX.

### Session 7 addendum: real AI streaming now verified

- KIN_BAT_02's tutorial spawner GPEs (`Design/BUBBLE_ATTACK/NME_Spawner`, keys
  `0x572D38C0FD0D94D7` and `0x32A5B7D6C236B3C2`) spawn four real `KushanwarriorPrefab(Clone)`
  AIComponents (positions ~158-176 m from spawn).
- Host streamed 406 real actor frames; a second client kept in the Bootstrap menu received 408
  frames / **4 real actors** with unique keys, positions, hp=1.00, alive=True
  (`local\tools\two-client-ai-real2.ps1`, PASS).
- Actor keys for spawned clones include `GetInstanceID()` because hierarchy paths collide; the
  streaming range was widened 70 m -> 250 m to cover the tutorial arena.
- Level loads with live AI (boss arenas `KIN_BAT_04`, `CIT_DEP02_05`) still hang, so the tutorial
  spawner is the verified real-AI route today. Native WorldPortal transitions remain blocked by
  the same engine/level hang (client-initiated level changes sync correctly through the host).

### Session 7 round 2: transition-hang diagnosis, detour-free clone drive, WorldPortal routing

- **Diagnosis:** with `POPCOOP_SAFE_MODE=1` (frame pump + bridge only) a real
  `CIT_LOW_02_MysteryChest -> CIT_LOW_11` WorldPortal transition succeeds; with gameplay hooks it
  hangs. Bisected to the Harmony detours on `PlayerLocomotionSubComponent.OnFixedUpdate` /
  `PlayerAbilitiesSubComponent.OnLateUpdate`: removed them. Clone stick input is now injected from
  the main-thread pump (`CloneDrive.PumpInjection` in `NetSession.Tick`); main two-client test still
  passes (remote clone animation/mirror unaffected; local-hero injected walking is weaker).
- **WorldPortal routing (action 16):** on a connected port al the TriggerLogic prefix resolves the
  destination via `PortalComponent.LinkedLevelData`, blocks the native transition, and routes it
  through the host: host loads destination (LevelTravel) + broadcasts action 12; client sends
  action 16 and waits. Verified messages in a two-client run (B `portal sync: client portal ...
  request sent=True`, A `portal sync: host loaded requested destination: ... CIT_LOW_11`), but the
  clients then hung during the connected transition - end-to-end portal sync still unverified.
  Next bisect: plain connected `level goto CIT_LOW_11` (no portal) vs blocked-portal state.
- **Env gates for diagnosis:** `POPCOOP_SAFE_MODE`, `POPCOOP_NO_HOOKS`, `POPCOOP_NO_TICK`,
  `POPCOOP_NO_COOP`, `POPCOOP_NO_INTERACT`, `POPCOOP_NO_CLONEDRIVE`.
- Build 0 warnings, 28/28 tests; main two-client regression PASS.

### Session 7 round 3: connected-transition hang isolated to NetSession.Tick

- Diagnostic `local\tools\portal-bisect.ps1`: two connected clients in CIT_LOW_02_MysteryChest with
  `POPCOOP_NO_TICK=1 POPCOOP_NO_INTERACT=1`; client B ran the native WorldPortal transition and BOTH
  clients stayed alive (B in CIT_LOW_11). With Tick on, the same transition hangs both clients.
  => remaining crash is in NetSession.Tick's per-frame game work (detours already removed).
- Attempted hardening (committed): scene watchdog at Tick start, cached/guarded LocalPlayer,
  try/catch around snapshot transform/send. Routed portal re-test still hangs after the host
  completes the LevelTravel load, so hardening alone is insufficient.
- Next: env-gate individual Tick sections (snapshot send / PollGpeStates / level poll+heartbeat /
  actor+enemy send) to isolate the exact statement, then fix. Portal routing implemented and
  message-verified but not end-to-end verified yet.
- Build 0 warnings, 28/28 tests.

### Session 7 round 4: client-initiated portal routing verified with two live clients

- With `POPCOOP_NO_INTERACT + POPCOOP_NO_SNAPSHOT + POPCOOP_NO_GPE_POLL`: client B ran a real
  CIT_LOW_02_MysteryChest -> CIT_LOW_11 WorldPortal transition, sent the level-change request, host
  loaded CIT_LOW_11 via LevelTravel and broadcast it, and BOTH clients ended in CIT_LOW_11 alive.
  Logs: B `level sync: client changed to CIT_LOW_11, request sent=True`; A `level sync: applied
  client request: remote level load Level_CIT_LOW_11` + `host changed to CIT_LOW_11, notify sent`.
- Connected transitions crash when the Tick snapshot send and/or GPE poll run during the
  transition; frame-time guard + scene watchdog help but do not fix it. Next fix: make those two
  Tick sections transition-safe (skip game-object access through the world-streaming window),
  then re-run `local\tools\two-client-portal-sync.ps1` without gates.
- New per-section env gates: POPCOOP_NO_SNAPSHOT / NO_GPE_POLL / NO_LEVELSYNC / NO_ACTORS /
  NO_ENEMIES. Diagnostic: `local\tools\portal-bisect.ps1`.
- Build 0 warnings, 28/28 tests.

### Session 7 round 5 (final): Tick transition-safe, default-config portal sync + regression PASS

- `NetSession.Tick` reworked so no game-object access happens during world streaming: cheap scene
  read, 12 s scene settle on real level changes, 6 s flow pause, long-frame guard, 15 stable frames
  before touching objects; level notifications (host heartbeat/broadcast, client request) run
  before the busy gate since they are pure network; world-state replay deferred until stable.
- Follow-ups: streamed sub-scenes are no longer mistaken for level transitions
  (`LevelTravel.IsKnownLevelScene`), and the flow gate no longer extends the settle window or
  invalidates clone caches.
- Default configuration (no env gates), two live clients:
  - `local\tools\two-client-portal-sync.ps1` PASS (twice): client WorldPortal CIT_LOW_02 ->
    CIT_LOW_11, host follows via request+broadcast, both stable/alive, `portalLoads=1`,
    `levelChangesApplied=2`.
  - `tools\two-client-test.ps1` full regression PASS: clone animation/mirror facing, interaction
    wire both ways, direct GPE activation, late-join replay. Save hash unchanged.
- Build 0 warnings, 28/28 tests. Two-machine playtest still needs a second PC with the same build.

## Session 8 (2026-11-15, round 2): parallel feature lanes, loader fixes, portal limit

Repo `D:\New folder (2)\popcoop`, HEAD `1abb977`, branch `feat/m0`. Build 0 warnings,
**64/64 tests**. The session was split into parallel Orca lanes and merged back on top of
`0cdac38`: `4e4e45f` save-authority design + `SaveAuthority` model/tests,
`199f8dd` combat wire type 8 + `CombatAuthority` model/tests, `3a9d6a3` remote-enemy proxy
plumbing. Merge commits `435e09f`/`44141e3`.

### Loader fixes (all verified in game)

- **Clone cache across long frames.** The long-frame guard called `CloneDrive.InvalidateCache()`,
  and invalidation did not reset the cached clone GameObject, so one >150 ms frame permanently
  stopped the peer clone's animation. Now only the scene-change watchdog invalidates, it also
  clears `_cloneGo`/`_cloneBody`, and Tick re-caches when `CloneDrive.HasCache` is false.
  Symptom before the fix: client received `remoteMoveX=1.00` yet `currentAnim=Idle_OutOfCombat`
  (evidence `local\evidence\two-client-regression-2026-11-15.log`).
- **Gameplay interactions wait for the stable window.** The drain used to run before the busy
  gate, so `ApplyRemote` could touch scene objects while the world streamed. Actions 12/14/16
  still run immediately (they route transitions); 1/2/10/11/13/15 are deferred, cleared on a
  level change, capped at 64.
- **The remote clone no longer triggers world logics.** In `CIT_LOW_02_MysteryChest` the spawned
  player-2 body overlapped the level-entry portal volume and fired the exit WorldPortal ~5 s
  after connect, dragging both clients to `CIT_LOW_11`. `InteractionHook` blocks a
  `TriggerLogic` whose instigator is `PopCoop Player*` in the prefix and skips it in the
  postfix. This bug is why the host-authoritative chest test kept leaving the level.

### Verified on 1abb977

- Host-authoritative chest: **PASS** (`local\evidence\chest-1abb977-2026-11-15.log`). Host
  completed `0x277F418013854B94`, peer applied action 13 state-only, screenshot 329,966 bytes.
- Full two-client regression: **PASS** (`local\evidence\regression-1abb977-b-2026-11-15.log`):
  locomotion, mirror facing, interaction wire both ways, direct GPE, late-join replay.
  An earlier run of the same build failed at late join because the host stopped ticking at client
  disconnect (host log ends 20:27:30.754, before the new epoch); rerun passed. Record as a
  host-freeze flake.
- Client-initiated WorldPortal: **not passing today** on `1abb977`, `0cdac38`, or a clean
  `e8d6b97` baseline (`local\evidence\portal-bisect-e8d6b97.log`). Host freezes ~1 s after its
  own `LevelTravel` load of `CIT_LOW_11`; the client reaches `CIT_LOW_11` and stays up.
  Treat as an engine/environment limit, not a merge regression.
- Owner save unchanged (`B34D9065...A4AD`), guard patch reverted (`74 05`), no game
  processes after every run.

### Still open

In-game remote-enemy visuals (prefab TODO; `net proxies` reports counters only), combat
authority applied to real actors, save transfer between machines, portal host-freeze diagnosis,
story/quest sync, two-machine playtest.

### Session 8 addendum: portal routing fixed, connected-pair exit code 3

- `351f5df` fixes the routing path itself: `TrySyncWorldPortal` identifies a portal by logic
  type before scanning, so every non-portal `TriggerLogic` no longer runs a full
  `FindObjectsOfType`; a WorldPortal pressed inside the settle window is blocked in both
  directions (no native transition, no request) instead of falling through; the Tick watchdog
  records the new level in `Interactables` (`NoteLevelChanged`), so the first later scan does
  not treat it as a fresh transition; `BeginSceneTransition` now only extends an open window.
- After that fix the client request (action 16), host `LevelTravel` load and both arrivals in
  `CIT_LOW_11` are verified in the log (`local\evidence\portal-levelnote-2026-11-15.log`).
- The remaining portal failure is a **spontaneous exit**, not a freeze: both processes exit with
  code `0x3` (requested quit; no managed `Application.Quit` log, no crash dump, no WER event).
  Host exits first, client 9-27 s later. A lone client in the same level survives 42 s
  (`local\evidence\single-citlow11-long-2026-11-15.log`). The same exit appeared at the end of a
  green regression run (`regression-351f5df-2026-11-15.log`, host `alive=False`), so it is not
  level-specific. Prime suspect: both local installs share one GSE save; per-machine saves on a
  second PC may not reproduce it.
- Regression on `351f5df`: PASS (locomotion, mirror, interactions both ways, GPE, late join).
  State clean after every run: no game processes, guard `74 05`, owner save unchanged.

Bisect update: the connected-pair exit code `0x3` also happens with `POPCOOP_NO_GPE_POLL=1`
(host at +39 s), `POPCOOP_NO_SNAPSHOT=1` (host at +30 s) and `POPCOOP_BLOCK_QUIT=1` (host at
+24 s, no managed `Application.Quit` log). A lone client is stable for 42 s and two unconnected
copies are stable, so the GPE poll, snapshot send and managed quit are all exonerated. Working
theory: Goldberg emulator / crack reacting to a second copy (same account or save root), or a
native `exit(3)` in the game. Local long connected sessions are not a reliable test bed;
two-machine play or a separate Windows profile is needed. Diagnostic evidence:
`local\evidence\connected-citlow11-{nogpe60,nosnap,blockquit,gpedebug}-2026-11-15.log`.

### Session 8 addendum 2: profile isolation attempt, machine stability, actor matcher

- Tried isolating the second client by profile: a local user with its own %APPDATA% and a copied
  save (same and different emulator account id). The game stayed at FTUE (fresh profile does not
  recognise the copied save), the DLC Mask of Darkness modal ignored synthetic confirm, and the
  fresh profile exited at the title screen on its own. Removed the user/profile and restored the
  owner install ACL with `icacls /remove:g`. Profile isolation is not a quick fix; a second PC is
  the honest test for the connected-pair exit.
- Machine stability: Kernel-Power 41 at 02:46 and 07:06 (unexpected shutdowns) and a Windows
  Update restart at 07:38 (Event 1074, winlogon). Long dual-client runs may stress GPU/power here.
  Prefer short 640x360 runs; check GPU driver/PSU before long playtests.
- Ground work `6e00415`: `ActorMatcher` (PopCoop.Core) pairs remote actor samples with local
  actors by nearest position, one-to-one, radius-limited, order-independent, six tests (71/71
  total). Spawned-enemy actor keys contain GetInstanceID and cannot match across peers, so
  position is the stable signal for future enemy health/combat sync. New bridge commands:
  `quicknewgame`, `saveui`.

### Session 8 addendum 3: auto-reconnect, installer fix, fresh-install validation

- Auto-reconnect (`c00d264`): the client remembers its join target and retries after 10 s of peer
  silence or a 5 s handshake timeout, with 2/5/10 s backoff; `net stop` clears it. Verified
  two-client: host stop -> client `connecting` -> host return -> reconnected in ~2 s with the
  late-join replay (36 entries). Evidence `local\evidence\two-client-reconnect-2026-11-16.log`.
  Full regression PASS afterwards (`regression-reconnect-2026-11-16.log`).
- Installer fix: on a fresh folder `BepInEx.cfg` does not exist until the game runs once, so the
  required `UnityLogListening = false` was silently skipped and the chainloader would die.
  `install.ps1` now creates the cfg (or appends the key) and says so. Validated end-to-end on a
  fresh `D:\popcoop-install-test` built from `tools\make-test-install.ps1` with the pre.2
  BepInEx zip: plugin booted, bridge answered, fingerprint `v1-ca349433644ad8c5`, uninstall
  removed only the mod and left the owner data intact
  (`local\evidence\installer-boot2-2026-11-16.log`). `make-test-install.ps1` now defaults to
  the pre.2 zip (be.793 fails on this build).

### Session 8 addendum 4: host-authoritative enemy deaths

- `55b4e34`: the host's actor wire (type 7) carries `hpRatio`/`alive`. On the non-host,
  `ApplyRemoteActorDeaths` matches each dead remote actor to the nearest local AI (4 m, using
  `ActorMatcher`), sets its HP to 0 and hides it. One shot per remote key, cleared on level
  change/disconnect, stable window only. Verified two-client with the KIN_BAT_02 tutorial spawner:
  4 AI on both peers, host sets one to HP 0, peer removes exactly one
  (`local\evidence\two-client-enemy-death2-2026-11-16.log`). The first attempt removed two
  because the same remote re-matched on the next tick; the applied-key set fixed it.
- QA commands: `net ailist`, `net aisethp <key> <hp>`, `net aikill <key> [ClassicDeath|OnDeath]`,
  `net aihide <key>`. `ClassicDeath`/`OnDeath` through IL2CPP runtime invoke did nothing to an
  idle AI, hence hp=0 + SetActive(false) on the peer; the host keeps its own death animation.
- Still open: client damage against host-authoritative enemies is not sent to the host, so client
  kills do not stick. Next step is a combat message from the client on local AI HP loss.
- Docs: README known-issues and handover section 10 updated; suite 71/71, build 0 warnings.

### Session 8 addendum 5: two-way enemy health, death and position

- `322c67f`: both peers stream local AI state (actor wire, every 6 ticks, `net actors` on by
  default). Each applies the peer's state to the nearest local AI within 4 m: lower HP wins (so
  either player's damage sticks), a dead remote hides the local AI once, a local HP-zero sweep
  hides the killer's own copy after 2 s, and the client lerps its AI to the host's position/yaw
  (0.35, threshold 0.25 m). `3379c8e` adds the position convergence.
- Verified: `two-client-enemy-damage2-2026-11-16.log` (client HP 40->10 on host, client kill
  4->3 both sides) and `two-client-enemy-pos-2026-11-16.log` (max nearest 0.2 m). Regression
  PASS `regression-322c67f-2026-11-16.log`; build 0 warnings, 71/71 tests.
- Limit: health is inferred from streams, not hit events. No attack-animation or damage-number
  sync; peer deaths are a hide; AI targeting/patrol still runs independently, pinned by position.

### Session 8 addendum 6: remote player animations

- `df89df4`: snapshot 25 -> 26 bytes with a local `EPlayerAnims` byte (255 = unknown). The
  receiver plays non-locomotion values on the clone (jumps/dodges <=17, grabs/attacks >=57), once
  per change; locomotion stays on the moveX buckets. `net animstate` shows `remoteAnim=`; new QA
  command `net playanim <EPlayerAnims>`. Snapshot round-trip test added (72/72).
- Verified: A plays `AttackCombo01`; B reports `remoteAnim=AttackCombo01`, clone logs
  `clone remote anim -> AttackCombo01` and `currentAnim=AttackCombo01`
  (`two-client-anim-2026-11-16.log`). Regression PASS after the protocol change
  (`regression-anim2-2026-11-16.log`; first attempt hit the known host-freeze flake).
- Display only: remote attacks animate but do not deal damage through the clone; damage remains the
  HP-stream model. Some clips may look approximate because the clone is a Puppet body.

### Session 8 addendum 7: player death visibility and save transfer

- `ab58e29`: the local player's HP crossing zero sends interaction action 18/19 (network-only in
  the Tick drain); the peer hides/shows the clone through the cached `_cloneGo` (FindClone only
  sees active objects). Verified two-client: peer `findplayer` 2 -> 1 -> 2
  (`two-client-player-death2-2026-11-16.log`). Health pools remain per player; visibility only.
  Regression PASS `regression-player-death-2026-11-16.log`; the host still hits the code-3 exit
  after the checks.
- Manual save transfer: `save-transfer.ps1` (compare/export/import, SHA-256 manifest, backup on
  import, refuses while the game runs). Tested on a scratch copy; live save untouched. Documented
  in README.
- Not implemented and not safely reachable here: automatic shared save/progression, story/quest/
  dialogue/cutscene sync, ability unlocks. The connected WorldPortal exit `0x3` remains an
  emulator/two-copy issue; two-machine play is untested.

## Handover for the final push (2026-11-16)

`D:\New folder (2)\popcoop\HANDOVER-2026-11-16.md` is the read-me-first for the next agent: current
verified state (build 0 warnings, 72/72 tests, live suite PASS), what round 3 changed, the remaining
work (shared save, story/quest/ability sync, hit events, portal exit 0x3, two-machine), updated
traps, commands, QA scripts/bridge commands, and a ready-to-paste Orca prompt with three worker
lanes and a single live-test lane.

### Round 4 (2026-11-16, coordinator + five Orca lanes): story slice, hit events, save staging, tool-backed QA

Final tree: **0 warnings, 132/132 tests**. New opt-in features, all live-tested on one machine:
- Story effect capture + one slice (Vision ability): host publishes `m_isVisionUnlocked`, client
  applies action 20 in the stable window with an echo guard. A=1/B=1. Two live bugs fixed: host
  `story set` did not publish, and the attacker id was an entity key that collided across peers.
- Hit events (type 8, 45 bytes, actor XYZ): position match 4 m, lower HP wins, one-shot death,
  own-echo/non-owner/stale/dup rejected, `net combat log` trace. Verified both directions.
- Staged save import at plugin load (hash + baseline + timestamped backup + atomic replace) and a
  read-only `save export`; no live in-session save writes. Not verified against the owner save.
- `qa/` suite: scorer self-test (15 cases), 7 gates PASS across runs, control PASS, invariants PASS
  (4/4 actors, 0.20 m), chaos lan PASS, good_wifi lost one interaction (3 drops), animcheck 39.39 %
  group mismatch + one sliding episode (no bones/yaw). `qa/fuzz` hub target: 200/200 seeds and a
  2-minute bad_wifi run clean; five twins caught. Hub `tools/test_all.py`: ALL OK (15 tools).
- Code-3 at the kill step: reproduced; still exits with the actor hide disabled and with the actor
  apply skipped, and no WER/Application Error event. Actor path ruled out; environment limit stands.
- Two-machine play still untested. Hub write-up PR prepared locally, not published (owner approval
  required); `games/.../README.md` was previously untracked and is now part of the change.

### Round 5 (2026-11-16 evening): loss retry, pose probe, hardware limits

- Loss retry: player-action, chest-request, death-visibility and story packets go out three times;
  the receiver's key+10 s dedupe keeps them idempotent. The `good_wifi` proof was interrupted by
  the code-3 exit; delivery under loss is implemented but unproven.
- Pose probe: `pose [all|local|clone|bones]` returns root yaw and rig bones; the animcheck probe
  now emits `rot` and `bones`. Animcheck compares pose (`pose_err_p95=37.9`, no missing fields) and
  still finds 39.8 % group mismatch plus one sliding episode (`qa-20261116-180917`).
- Two-machine play is not possible here: no second PC answers on 192.168.8.x/192.168.1.x and no
  usable VM/GPU path exists (hypervisor present, no Hyper-V management/VM). A second Windows
  profile was not retried. Quests/cutscenes/other unlocks remain design-only; no client-side quest
  script suppression point was found, so no raw quest write was risked.
- Code-3 isolation on a second OS/VM remains untested. Same-machine sessions degraded to exits
  within ~30 s in the last runs; stop live work and let the machine cool.
- State: 0 warnings, 132/132 tests, save B34D9065...A4AD, guard 74 05, no games. PR #397 remains a
  draft with the earlier evidence; this round's text is added to the same branch.

### Round 6 (2026-11-18): code-3 fixed, two-machine verified, story/save/proxy lanes

- **Exit code 0x3 root cause fixed.** Frida + IL analysis: BepInEx `6.0.0-pre.2` never ran
  `InjectorHelpers.Setup()`, so the GC finalizer patch was never installed while
  `Il2CppObjectPool.DisableCaching` stayed `false`; pooled non-finalizable IL2CPP objects were
  re-registered for finalization and the Boehm finalizer thread crashed with a null `MethodInfo`.
  `Plugin.Load()` now disables pooling of those objects and hooks `RunFinalizer`. Regression,
  chest, enemy-damage and all four chaos profiles (`lan`, `good_wifi`, `bad_wifi`, `hell` + a 4 s
  partition) pass afterwards (`qa-20261118-114007`).
- **Two-machine play verified** (Win11 host + Hyper-V Win10 LTSC `PopCoop-W10L`): movement,
  fixed `MirrorAnimationV2` polarity, `AttackCombo01`, death/revive, `ToggleGPE` both ways and
  enemy HP lower-wins/kill `4->3`, with screenshots on both machines.
- **WorldPortal clean re-test passes after the fix**: the client triggers `CIT_LOW_02` ->
  `CIT_LOW_11`, both peers arrive, stay connected and stay alive for 10 s
  (`qa-20261118-153308`).
- **animcheck lands in the walk band**: `group_mismatch_pct=1.01`, `pose_err_p95=17.0`,
  facing error 0.0, no threshold breach (`qa-20261118-150037`).
- **Story sync beyond one flag**: the host captures six ability flags and `QuestManager` quest and
  element stages; the client applies them and its `QuestManager.m_canUpdate` is suppressed so its
  quest scripts do not advance independently. Story epoch and sequence are host-authoritative; a
  client-side rebase was resetting the sequence after the baseline and stalling replication, now
  fixed. `story-combat` PASS: vision host->client, combat hits both ways, host=1 client=1
  (`qa-20261118-154901`).
- **Save authority**: read-only sidecar (live-slot hashes, host/peer baselines, pre-session
  backups under `%LOCALAPPDATA%\PopCoop\save-authority`), `MsgSaveMeta` exchange and
  `save status|backup|inspect`. No save-content transfer and no in-session write to the live save.
- **Remote enemies render**: visual-only scene clone, procedural capsule fallback, pooled and
  dropped on level change.
- **Mutation twins caught live**: `mutant-clone-triggers` (clone instigator fires a WorldPortal)
  and `mutant-interact-early` (client does not apply the chest completion) both fail the chest gate
  as required (`qa-20261118-155411`, `qa-20261118-160730`).
- State: 0 warnings, 159/159 unit tests, scorer self-test 15/15, owner save unchanged
  (`69174FE0...FDBE`), guard patch reverted after every run.
