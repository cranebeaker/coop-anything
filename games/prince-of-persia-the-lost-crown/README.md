# Prince of Persia: The Lost Crown (2024)

| | |
|---|---|
| Engine | Unity 2021.3.29f1, IL2CPP x64 |
| Versions tested | `1.4.3+TU16.421692.3046900` (repack with Goldberg/GSE saves; store build untested) |
| Projects | PopCoop (source not published yet) |
| QA level | automated |
| Last updated | 2026-11-16 |

## Where things stand

Two players can share one world today: both bodies exist and animate, interactions and chests sync,
levels follow the host, enemies share HP/positions/deaths, remote attacks are visible, and a client
reconnects after a drop. It is an early co-op shell, not campaign co-op: story, quests, dialogue,
cutscenes and ability unlocks are not synchronised, and the game must be entered through an
automation bridge or the normal menus.

## Existing work

- No co-op or multiplayer mod for this game was found (GitHub code/repo search, web search,
  Nexus/ModDB/Steam discussions) as of 2026-11-13.
- BepInEx 6 IL2CPP loads the game. `6.0.0-pre.2` works with `UnityLogListening = false`;
  `6.0.0-be.793` aborts in Il2CppInterop (`GenericMethod_GetMethod_Hook ... Sequence contains more
  than one element`). ClassInjector is also broken, so custom MonoBehaviours cannot be injected:
  the frame pump rides Harmony postfixes on `LoadSaveManager.AlkawaUpdate` and `Camera.get_main`.
- p1xel8ted/UltrawideFixes has a working plugin for this game (Harmony patches on scene load,
  video, canvas scaler), proving plugin-based camera/UI work.

## How the game works (for co-op)

- IL2CPP x64, in-house `Alkawa.*` assemblies, Addressables (`StreamingAssets/aa`), Wwise,
  Unity Input System, Animancer. **No gameplay netcode**: `Alkawa.OnlineServices` is Ubisoft
  Connect, not replication.
- Levels are Addressable scenes driven by world streaming, not build-settings scenes. The safe load
  path is `WorldStreamingStartSettings{WorldData, StartingLevel, PerformPlayerTeleport}` +
  `GameFlow.PushState(Loading)`; only real `LevelData` scenes count as transitions.
- The hero has a `Rigidbody` on the root, an `Animator` and `CapsuleCollider` on children, no
  `CharacterController` and no `PlayerInput`. Movement is velocity; animation is Animancer +
  input-driven (`PlayerAnimationSubComponent.Play(EPlayerAnims, ...)`).
- `AlkawaBehaviour.transform` throws "transform access is forbidden"; go through the managed
  `MonoBehaviour.gameObject.transform`.
- Interactions funnel through `InteractiveElementLogicBase.TriggerLogic(GameObject)`. Direct GPE
  (doors/mechanisms) activation is `GameplayElementBaseComponent.ToggleGPE_External`; Harmony-
  patching that method crashes around streaming, so the mod polls activation state instead.
- Saves use the Goldberg/GSE layout: `%APPDATA%\GSE Saves\<appid>\remote\win64_save\data001Slot.bin`
  (3,317,100 bytes, `DSSS` header). `%APPDATA%` redirection does not isolate it; the game resolves
  the known folder through Windows. Saves are per machine and per Windows profile.
- Single-instance guard: `Alkawa.Engine.UbiconnectPCManager.PreInitialize` holds a `Global\` mutex
  and shows "Game already running!". It runs before BepInEx can patch it reliably, so local QA
  applies a two-byte NOP to the test install only; two-machine play needs no patch.
- `FindObjectsOfType` on `UIScript` throws (plain managed class). UI scripts live on
  `UIScreensBehavior.m_UIScreenList[].m_UIScriptList`.

## What we changed

PopCoop, a BepInEx 6 plugin plus a localhost RPC bridge used by the tests.

- **Second body**: `Object.Instantiate` of the live player, renamed and tinted. Physics-driven,
  follows peer snapshots.
- **Locomotion and facing**: snapshots carry the stick X; the receiver injects it into the clone's
  locomotion state and plays Idle/Walk/Run through the game's own animation component. Facing is a
  skeletal mirror, not a root yaw; `ForceFacing` crashed a probe.
- **Player animations**: snapshot is 26 bytes with the local `EPlayerAnims` byte; the clone plays
  non-locomotion values (jumps, dodges, attacks) once per change.
- **Interactions**: a Harmony postfix on `TriggerLogic` forwards player-action interactions to the
  peer, which replays them with a real instigator. System logics (checkpoint, portal, cutscene,
  conversation) are deliberately not replayed.
- **Chest authority**: a client does not run chest/pickup logic; it asks the host (action 15). The
  host completes and broadcasts state only (action 13), so loot is not rolled twice.
- **Level sync**: action 12 (host broadcast) / 14 (client request) plus a 10 s heartbeat. WorldPortal
  activations are resolved through `PortalComponent.LinkedLevelData` and routed as action 16.
- **Tick stability**: every game-object access sits behind a 12 s scene settle and 15 stable frames;
  gameplay interactions are deferred to that window. Streamed sub-scenes are not treated as level
  changes. This fixed silent hangs during connected transitions.
- **Clone can no longer fire world logics**: the spawned body's collider sat in level-entry portal
  volumes and triggered them; a `TriggerLogic` whose instigator is `PopCoop Player*` is now blocked.
- **Enemy state**: both peers stream local AI state (type 7). The peer matches by position (4 m),
  takes the lower HP, hides enemies reported dead, and the client lerps its AI to the host position.
  This is host-authoritative enough for co-op but trusts reported HP.
- **Player death visibility**: the local HP crossing zero sends action 18/19; the peer hides/shows
  the clone. Health pools stay per player.
- **Reconnect**: a client remembers its join target and retries after 10 s of silence or a 5 s
  handshake timeout (2/5/10 s backoff).
- **Installer**: creates `BepInEx.cfg` with `UnityLogListening = false` on a fresh folder, backs up
  existing configs, and uninstalls only the mod.
- **Hit events (opt-in)**: type 8 carries actor position, attacker role id, damage, resulting HP,
  alive and epoch/seq. The host applies client-reported hits to the position-matched AI (4 m) and
  the client applies the host's; own echo, non-owner, stale epoch and duplicates are rejected. A
  128-record decision trace is readable via `net combat log`.
- **Story effect capture (one slice)**: an ordered, epoch-stamped effect transaction (flags OR,
  stages max) plus the Vision ability flag. The host publishes `m_isVisionUnlocked`, the client
  applies it in the stable window with an echo guard; no script is replayed.
- **Staged save import**: a manifest-verified, baseline-checked, timestamped-backup replacement at
  plugin load; the bridge can export the live slot read-only. No in-session write path.
- **QA suite**: tracked `qa/` with a scorer self-test, seven live gates, control, invariants,
  mutation twins, chaos and animcheck modes.

## QA

Automated, two live clients on one machine (the second install carries a local guard NOP; the owner
install is never touched). The bridge drives menus, levels, interactions and probes.

```powershell
.\tools\dotnet\dotnet.exe build src\PopCoop.Loader -c Release
.\tools\dotnet\dotnet.exe test PopCoop.sln -c Release
powershell -NoProfile -ExecutionPolicy Bypass -File qa\run-live-suite.ps1 `
    -RepoRoot <repo> -GameA D:\popcoop-game -GameB D:\popcoop-game2
```

`qa/` is the tracked suite (scorer self-test first; regression, chest, reconnect, enemy-damage,
enemy-pos, anim, player-death; `-Control`, `-Invariants`, `-Chaos`, `-Animcheck`, `-Only`,
`-Mutant`). Last suite (2026-11-16): build 0 warnings, **132/132 unit tests**; the seven live gates
PASS across runs (locomotion/facing, interaction both ways, chest state-only apply, reconnect after
an 8 s host outage, enemy HP/death/position, remote animation, player death visibility). Control
PASS (host alone: HP 1.00 -> 0.25 read back); invariants PASS (4/4 actors, alive, HP and positions
within 0.20 m at t0/t+3/t+6 s).

Tools from this hub were used and their own suite passed with `python -I tools/test_all.py`
(ALL OK, 15 tools; cmake on PATH, Python 3.13):
- **chaos** (`lan` seed 1): movement, interaction, 4/4 actors, 0.30 m, 0 drops; `good_wifi` lost the
  interaction with 3 drops (movement/positions/scene OK); `bad_wifi` and `hell` ended when both
  games exited `0x3` before comparison. No convergence failure was measured; a 0.5 % loss profile
  does show that one-shot interactions have no retry.
- **animcheck**: 400 samples, 99 avatar comparisons, `group_mismatch_pct=39.39`, one sliding
  episode, no T-pose/jitter/rubber-band/stuck/death-pose findings. Pose and facing are not
  comparable: the bridge has no bone or yaw command (the report names those missing fields).
- **fuzz**: `qa/fuzz` wraps the lower-HP, 4 m position match, one-shot death, ownership epoch and
  deferred-activation rules in the hub target. 200 seeds on 2 players, 200 on 3 and a 2-minute
  4-player `bad_wifi` run held every oracle; five twins (`hp_overwrite`, `death_all`,
  `stale_accept`, `double_apply`, `no_position_match`) all failed with the named oracle and were
  shrunk (1-3 ops).
- **mutants**: the scorer self-test rejects 15 bad fixtures; the lower-HP twin is unit-proven
  (`HpMergeTests`). Live `no-actors`/`death-all` twins were interrupted by the host `0x3` exit at
  the kill step and are reported as attempted, not passes.
- **flight / gameslot / inspect**: flight ran its own tests but no `.cofr` emitter is wired into the
  mod yet; gameslot serialised the live lane; inspect was not adapted (optional item).

Known flake: on one machine, connected sessions can exit with code `0x3` after tens of seconds
(host first), often around an enemy death. It reproduces on older builds, with the GPE poll,
snapshot send and managed quits disabled, and with the actor apply or the `SetActive(false)` hide
disabled, so the actor path is not the trigger; no WER/Application Error event is written. A lone
client is stable. It looks like the emulator/two-copy environment, not the mod's tick work. Long
local sessions are not a reliable test bed.

Evidence logs live in the owner's private QA folder, referenced in [NOTES.md](NOTES.md).

Known flake: on one machine, connected sessions can exit with code `0x3` after tens of seconds
(host first). It reproduces on older builds, with the GPE poll/snapshot disabled and with managed
quits blocked; a lone client is stable. It looks like the emulator/two-copy environment, not the
mod's tick work. Long local sessions are not a reliable test bed.

## How to play

Both players need the exact build. Copy the install, then:

```powershell
.\install.ps1 -GameDir "<game dir>"
```

Host: `net host 27015`. Other player: `net join <host LAN IP> 27015`. The handshake compares game
version, IL2CPP metadata and Addressables catalog hashes. Saves are per machine; use
`save-transfer.ps1` with the game closed to copy progress.

## Known issues

- Story, quests, dialogue, cutscenes and ability unlocks are not synchronised.
- Saves are per machine; there is no live shared progression (manual copy only).
- Connected WorldPortal transitions can exit the game with code `0x3` after both clients arrive.
- Two-machine play has not been tested (no second PC); the LAN-address two-client run is the
  closest validation.
- The second player is a cloned body: no inventory, abilities or camera. Remote attacks animate but
  do not deal damage; damage authority is the HP-stream model.
- Player health is per player; only death/respawn visibility is shared.

## Next steps

1. **Story sync by effect capture** (see `playbook/effect-capture-story-sync.md`): host runs story
   scripts, records primitive changes, clients apply them without re-running the script. Start with
   one quest flag or ability unlock, not the whole story.
2. **Hit events** instead of inferred HP: send actor position + damage + resulting HP, apply through
   the matched actor; keep the stream as fallback.
3. **Safe shared save**: staged import at plugin load (hash, backup), never live writes; then a
   transfer path.
4. **Two-machine or VM test**: settle the code-3 exit with a separate OS/profile and measure real
   latency.
5. Network-chaos and desync passes from the playbook once a second machine exists.
