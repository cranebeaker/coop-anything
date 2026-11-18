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
enemy-pos, anim, player-death, portal, story-combat, character, save-transfer; `-Control`,
`-Invariants`, `-Chaos`, `-Animcheck`, `-Only`, `-Mutant`). Last suite (2026-11-18,
`qa-20261118-180618`): build 0 warnings, **174/174 unit tests**; **13/13 live gates PASS**
(regression, chest, reconnect, enemy-damage, enemy-pos, anim, player-death, portal, story-combat,
character, save-transfer, animcheck plus the scorer self-test), and all five mutation twins are
caught.
Control PASS (host alone: HP 1.00 -> 0.25 read back); invariants PASS (4/4 actors, alive, HP and
positions within 0.20 m at t0/t+3/t+6 s).

Tools from this hub were used and their own suite passed with `python -I tools/test_all.py`
(ALL OK, 15 tools; cmake on PATH, Python 3.13):
- **chaos**: all four profiles (`lan`, `good_wifi`, `bad_wifi`, `hell`) and a 4 s partition heal
  PASS after the `0x3` fix: movement, interaction, actor positions and scene converge. One-shot
  interactions still have no retry beyond the 3x resend.
- **animcheck**: the `pose` command exposes root yaw and rig bones. The walk command was fixed to
  stay inside the walk band: `group_mismatch_pct=1.01`, `pose_err_p95=17.0`, facing error 0.0, no
  threshold breach (`qa-20261118-150037`).
- **fuzz**: `qa/fuzz` wraps the lower-HP, 4 m position match, one-shot death, ownership epoch and
  deferred-activation rules in the hub target. 200 seeds on 2 players, 200 on 3 and a 2-minute
  4-player `bad_wifi` run held every oracle; five twins (`hp_overwrite`, `death_all`,
  `stale_accept`, `double_apply`, `no_position_match`) all failed with the named oracle and were
  shrunk (1-3 ops).
- **mutants**: the scorer self-test rejects 15 bad fixtures; the lower-HP twin is unit-proven
  (`HpMergeTests`). Live `mutant-clone-triggers` (clone instigator fires a WorldPortal),
  `mutant-interact-early` (client does not apply the chest completion),
  `mutant-no-character` (abilities/inventory are not published) and `mutant-no-save-transfer`
  (the save server refuses) are caught: each breaks its gate as required
  (`qa-20261118-155411`, `qa-20261118-160730`, and the Round 7 evidence).
- **character** (2026-11-18): `DoubleJump` unlocked on the host appears on the client; an
  inventory grant stays local by default (per-player character sheet) and mirrors only after both
  peers enable shared inventory; both peers sit in conversation `id=8` at line `2645`
  (`qa-20261118-171422`).
- **save-transfer** (2026-11-18): the host serves the live slot, the client pulls 3,317,100 bytes,
  the SHA-256 matches the served slot and the file is staged for the next load; staging is cleaned
  (`qa-20261118-164616`).
- **choice** (2026-11-18): host-authoritative conversation choice. The `character` gate picks
  option 0 with a synthetic selector: host `decided=0 sent=1`, client `decided=0 applied=1`, both
  in conversation `id=8` at line `4450` (`qa-20261118-174929`); `mutant-no-choice` is caught
  (`qa-20261118-175416`).
- **loss retry**: player-action, chest, death-visibility and story packets are sent three times;
  the receiver's 10 s key dedupe keeps them idempotent. The `good_wifi` profile now passes.
- **flight / gameslot / inspect**: flight ran its own tests but no `.cofr` emitter is wired into the
  mod yet; gameslot serialised the live lane; inspect was not adapted (optional item).

Former flake, now fixed: connected sessions used to exit with code `0x3` after tens of seconds. The
finalizer/pooling root cause is described in NOTES Round 6; regression, chest, enemy-damage, all
four chaos profiles, portal and story-combat are stable after the fix.

Evidence logs live in the owner's private QA folder, referenced in [NOTES.md](NOTES.md).

## How to play

Both players need the exact build. Copy the install, then:

```powershell
.\install.ps1 -GameDir "<game dir>"
```

Host: `net host 27015`. Other player: `net join <host LAN IP> 27015`. The handshake compares game
version, IL2CPP metadata and Addressables catalog hashes. Saves are per machine; `net host` also
serves the live slot on port+1, and the peer can run `save pull <host> 27016` to stage a
hash-verified copy for the next start (or use `save-transfer.ps1` with the game closed).

## Launcher

`src/PopCoop.Launcher` (in the PopCoop repo) is a WPF launcher that detects the install, starts the
game, hosts or joins through the mod bridge, shows live status and can pull the host save. It
offers LAN, Tailscale, ZeroTier and public-IP helpers plus an across-town guide.

The theme samples the game's key-art palette (deep teal-navy, cyan energy, gold, sand) and uses an
original Persian girih/sun/dune composition. **Use artwork** loads the player's own key
art/screenshot locally; the launcher ships and redistributes no game art.

```powershell
dotnet build src\PopCoop.Launcher -c Release
.\src\PopCoop.Launcherin\Release
et8.0-windows\PopCoop.Launcher.exe
```

### Packaging

`build-release.ps1` builds a folder/zip (~96 MB) with `PopCoop.Launcher.exe`, `payloadepinex`
(BepInEx 6.0.0-pre.2), `payload\plugin`, a manifest with the tested game hash and a quickstart.
The launcher **is** the installer: first run asks for the game folder, installs BepInEx and the mod
in-process (no terminal, no `install.cmd`, no extra downloads), then shows Host/Join and one
**Play** button. No game files or artwork are redistributed.

## Known issues

- Story sync is opt-in (`POPCOOP_STORY_SYNC=1`). The host owns story progression: six ability
  flags, every unlockable ability and quest/element stages. Each machine owns its character sheet:
  inventory is per player by default (`POPCOOP_SHARED_INVENTORY=1` opts into a shared, merge-max
  pool). Dialogue and cutscene playback is shared and line-synced; conversation **choices** are
  host-authoritative (a client pick becomes a request), so the branch cannot diverge.
- Saves are per machine by design: the host slot is the canonical story copy, and each machine's
  slot also carries that player's character. A read-only sidecar (`save status|backup|inspect`), a
  hash-verified `save pull` and the manual copy tool exist. A pull applies at the next start and
  restores the local character sheet after load; there is no live in-session or semantic merge.
- The `0x3` exit was root-caused and fixed; connected WorldPortal transitions now pass a clean
  `CIT_LOW_02` -> `CIT_LOW_11` re-test with both peers alive for 10 s.
- Two-machine play is verified between a Win11 host and a Hyper-V Win10 LTSC guest (movement,
  mirror polarity, attack, death/revive, levers, enemy HP, screenshots on both machines).
- The second player is a cloned body: no inventory, abilities or camera. Remote attacks animate but
  do not deal damage; damage authority is the HP-stream model.
- Player health is per player; only death/respawn visibility is shared.

## Next steps

1. **Real scene choice coverage**: the choice sync is proven with a synthetic selector; exercise it
   on an authored `ConversationChoice` element when one is reachable (none in the current QA
   levels).
2. **A real second player on one machine**: not recommended. The engine assumes one local player
   (camera, UI, input, save); the clone is the remote body. Two machines or a streamed second
   instance are the supported paths.
3. **Loss hardening**: one-shot interactions and story batches have 3x resend and 10 s dedupe but
   no acknowledgement/repair pass yet.
4. **In-session save merge**: `save pull` stages a whole slot for the next start; a semantic
   progression merge while both games run is still open.
