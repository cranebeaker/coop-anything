# Core: identity, engine, and early findings

> Part of the Deep Rock Galactic: Survivor working notes. Extracted verbatim from `NOTES.md` on 2026-10-20 (lines 1-261); content unchanged.
> Current state and Ist/Soll: [../STATUS.md](../STATUS.md). Index: [../NOTES.md](../NOTES.md).

# Deep Rock Galactic: Survivor — working notes

Slug: `deep-rock-galactic-survivor`
Goal: online co-op (2+ players) for a game that ships single-player only.

> Private, machine-specific state (real install paths, logs, dumps, saves) lives in
> `local/`, which is git-ignored. This file is meant to be public-safe.

## Identity

- **Game:** Deep Rock Galactic: Survivor. Developer **Funday Games**, publisher Ghost Ship
  Publishing. Steam-only, Windows x64.
- **Store / appid:** Steam `2321470` (depot `2321471`).
- **Engine:** Unity, **IL2CPP x64**. `GameAssembly.dll` + `DRG Survivor_Data/il2cpp_data/
  Metadata/global-metadata.dat` (IL2CPP metadata **v39**).
- **Tested build (current):** Steam buildid `23854500`, **Unity 6000.4.7f1**
  (`UnityPlayer.dll` FileVersion `6000.4.7.15975364`),
  `build-guid=86f484871c2549f690a3fdbf26aebc8c`.
  - `DRG Survivor.exe` SHA-256 `5abca214deecb763d0cec4d6194c519ac9bd215052987a601c96e75d67e8679a`
  - `GameAssembly.dll` SHA-256 `250bbb4bb0491dee3c9d7f312276d9ecfce04cd98fda14cca1ab67cb289829ba`
  - `global-metadata.dat` SHA-256 `49202e38cb7e400c6895ce6188d81bbf2676af29a90e71493bd03513178a10d8`
- **Earlier build (superseded):** Steam buildid `20237659`, Unity 2022.3.59 IL2CPP,
  `build-guid=43d5554851e8403c9db7df2bc6cc9e34`.
  - `DRG Survivor.exe` SHA-256 `b788e1755d229679140a62e14c8bc175391c064cabce60ef217aac468b47a1db`
  - `GameAssembly.dll` SHA-256 `fcbac8f75fb8e508136b240e2eac2db5fff5d9271eac43df400996672dee06a3`
- **App strings:** `app.info` = `Funday Games` / `DRG Survivor`.

## Modding stack (verified on build 23854500)

- **BepInEx 6 IL2CPP** bleeding-edge `6.0.0-be.789+6cea286` loads and runs
  (`Chainloader startup complete`) **only after a workaround** — see below. BepInEx runs
  its own bundled .NET 6 runtime and generates `BepInEx/interop/*.dll` (132 assemblies on
  this build, incl. `Assembly-CSharp.dll` and `UnityEngine.CoreModule.dll`) via Cpp2IL.
- **Unity 6 / metadata v39 workaround (reproduced, then fixed):** the default config does
  IL *unstripping* using Unity base libraries; on this game that produces a broken interop
  and the chainloader dies with
  `FileNotFoundException: Could not load ... 'UnityEngine.CoreModule'`. Fix:
  1. In `BepInEx/config/BepInEx.cfg`, blank the value:
     `UnityBaseLibrariesSource =` (keep the key, remove the URL).
  2. Delete `BepInEx/interop` and `BepInEx/unity-libs`.
  3. Relaunch; interop regenerates without unstripping
     (`Not performing unstripping as unity libs are not specified`) and the chainloader
     completes. Cost: no unstripped Unity base libs. Reported upstream in
     [BepInEx#1350](https://github.com/BepInEx/BepInEx/issues/1350).
- **Third-party tooling note:** `Il2CppDumper` v6.7.46 **cannot** read metadata v39
  ("ERROR: Metadata file supplied is not a supported version[39]"); use Cpp2IL (bundled
  with BepInEx) or BepInEx's interop output instead.
- **Game's own assemblies** (from interop): `Assembly-CSharp.dll` (main game code),
  `FundayFactory.UI.dll`, `FundayGames.Audio`, `FundayGames.Attributes`,
  `FundayGames.LocalPrefs`, `FundayGames.PlatformService` (+`.Steam`).
- **Libraries of note:** **Zenject** (dependency injection — code is wired from
  installers/singletons, not `MonoBehaviour` globals), **KDTree** (spatial queries —
  likely enemy/target lookup, relevant for interest management), **UniTask** (async),
  PrimTween/DOTween (tweening), Cinemachine (camera), Steamworks.NET.
- **Namespace:** game code sits under `DRS.*` (e.g. `DRS.UI`).

## Observations (facts from a source)

- The game is **single-player only**. The devs answered a co-op question on the official
  Discord (Feb 2024): the focus is a single-player experience; co-op is "not being worked
  on right now" and may never happen. Reported by gamepressure.
- **Modding is established and open:** BepInEx **6** (Unity IL2CPP, bleeding-edge) is the
  community loader; there is a Thunderstore community `deep-rock-galactic-survivor` with a
  preconfigured `BepInExPack_IL2CPP`, and Nexus lists a BepInEx pack plus ~15 mods.
- **Reference mod source proves the hook pattern** (BlueRaja's mods, MIT): a BepInEx
  `BasePlugin` with `Harmony.CreateAndPatchAll`, patching IL2CPP game types. It exposes the
  game's namespace and types: namespace **`DRS.*`** (e.g. `DRS.UI`), types `RarityWeight`,
  `PickupSpawner`, `Economy`, enum `ECurrency` (GOLD, NITRA, RED_SUGAR, BISMOR, ...).
  Interop assembly referenced is `BepInEx/interop/Assembly-CSharp.dll`.
- **Steamworks.NET** ships in the build (`Plugins/x86_64/steam_api64.dll`), so a Steam
  relay path (Spacewar AppID 480) is available later for host/join without port forwarding.

## Derived (my reasoning, not yet verified in-game)

- **Path 2 — build from zero.** There is no co-op to revive. Unity IL2CPP + BepInEx is the
  "easiest modern target" per the playbook's engine table.
- **Reusable models (same engine):** Nitrox (Subnautica: BepInEx + Harmony + LiteNetLib,
  server-authoritative), HKMP (Hollow Knight), Quantum Space Buddies (Outer Wilds),
  CelesteNet (Celeste). Nitrox is the closest structural match.
- **Sync model:** host-authoritative state sync (cannot make a closed binary deterministic,
  so lockstep is out).
- **Sync surface to map:** the dwarf player(s), the enemy swarm and projectiles, damage and
  deaths, XP orbs, material pickups, the run clock/objectives, boss phases, and two
  single-player-only flows that have no co-op analogue: the **level-up card choice**
  (game pauses) and **mission end / extraction**.

## Guesses (believed but untested)

- Puppet dwarves via the game's own player/NPC models are feasible; syncing hundreds of
  swarm enemies is the hard part (bandwidth and authority, not visibility).
- The level-up pause will need a policy (freeze both, or first-to-pick decides).

## Unknowns

- Can two instances run on one PC? (single-instance mutex / window check not yet tested.)
- Entity model, tick/fixed-step behaviour, and how enemies are spawned/pooled — not dumped.
- Save/run state format; where between-run progression lives.
- Version drift: community reports show a later game version on **Unity 6**
  (`1.1.73p`, Unity `6000.4.7f1`). Co-op code must pin to one build; signatures will differ.

## Sources

- https://www.gamepressure.com/newsroom/deep-rock-galactic-survivor-drg-coop-and-multiplayer-explained/za6929
- https://steamcommunity.com/app/2321470/discussions/0/4371376043390476657/
- https://github.com/BlueRaja/Deep-Rock-Galactic-Survivor-Mods
- https://www.nexusmods.com/games/deeprockgalacticsurvivor
- https://thunderstore.io/c/deep-rock-galactic-survivor/p/BepInEx/BepInExPack_IL2CPP/
- https://github.com/BepInEx/BepInEx/issues/1350

## How the game works (from the interop dump, build 23854500)

- **No netcode at all.** A tree-wide search for Photon/Mirror/Netcode/NetworkManager/
  sockets/`TcpClient`/`UdpClient` found **nothing**. The sync layer must be written from
  scratch; there is no engine multiplayer to piggyback on.
- **`Entity` (`Entity : MonoBehaviour`) is the sync anchor.** Fields: `currentHp` (long),
  `IsPlayer` (bool), `pierceStopValue`; `position` (Vector3); `isAlive`. Methods:
  `TakeDamage(long dmg, bool isCrit, EDamageType, bool doFx, EDamageSource, Entity source,
  bool ignoreReduction)`, `Heal(long, EHealType)`, `Die(EDamageType, long overkill, bool)`,
  `ApplyStatusEffect(s)`, `OnGamePaused`/`OnGameUnpaused`. Both `Player : Entity` and
  `Enemy : Entity`.
- **`Player`** adds `EMoveControlType { DISABLED, FORCED, FULL_CONTROL }` plus a companion
  `PlayerControlled : MonoBehaviour` that does movement/animation (`_moveSpeed`,
  `_origTarget`, Idle/Forward/Backward/Left/Right). A **`DummyPlayer : Player`** subclass
  already exists — a ready-made puppet base.
- **Enemies are pooled:** `EnemySpawner.poolDictionary` → `ObjectPool<Enemy>` per `EEnemy`
  type; `Enemy.OnSpawn(...)`, `Enemy.ownerPool`. Pool reuse means instance ids are not
  stable, so the sync layer needs its **own network ids**.
- **AI is centralised in `AI_Manager`** (singleton `_instance`, `_units`, `Add`/`Remove`/
  `Update`) — the lever for authority: run it on the host, suppress on clients.
- **Zenject DI:** managers are wired with `Inject(...)` (e.g. `LevelUpManager`,
  `RunStateManager`, `SaveDataManager`). That is how a mod obtains live references.
- **Level-up flow:** `LevelUpManager` (`flowActive`, `StartFlow`, `HandleLevelUps`,
  `OnPrePlayerLevelGain`/`OnPlayerLevelGain`). This is the pause-and-pick-a-card flow that
  has no single-player analogue and needs a co-op policy.
- **Run/stages:** `RunStateManager` (stage index, primary/secondary objectives,
  `SaveToMidRunData`). **Save:** `DRS.SaveSystem` (`SaveDataManager`, `PCSaveFileManager`,
  `PCMidRunSaveManager`, `MidRunSaveBuilder`, `MidRunSaveData`) with PC and GDK variants.
- **Watch out:** `Assets.Scripts.PhysicsOptimisation` uses Unity Jobs/**Burst**
  (`lib_burst_generated.dll`), and the game ships **KDTree** + `TextmeshDOTS` — enemy
  targeting/movement may be partly Burst-compiled, which is harder to hook than managed code.

### Co-op implications (design sketch — Phase 4)

- **Required co-op behaviours (design decisions):**
  - **Both dwarves gain experience together** — XP/progression is shared, not per-player.
  - **Level-up choices synchronise:** when the reward/upgrade selection appears, the game must
    pause on **both** clients until **both** players have picked. (Today `LevelUpManager`
    pauses only the local instance.)
- Model: **host-authoritative state sync** (a closed binary cannot be made bit-deterministic).
- **Level generation must be forced identical** — see the transport milestone below: the game
  has no run seed, so the mod seeds `UnityEngine.Random` to a shared value before
  `MapGenerator.CreateMap`. Verified: identical map fingerprints on both instances.
- Puppets: spawn clones from the game's own player prefab / `DummyPlayer`; drive transform +
  animation from the network; disable their input and AI.
- Combat: replicate `Entity.TakeDamage`/`Die`/`Heal` results from the host; guests apply
  with a re-entrancy guard so an applied hit is not re-broadcast.
- Enemies: host runs `AI_Manager`, guests mirror position/HP. Milestone order: players
  visible → shared combat → swarm sync → objectives.
- Level-up: freeze both players during the card flow, or first-to-pick decides.

## Build log

- **Probe plugin v0.0.2 builds and loads** (`BepInEx` IL2CPP plugin, `net6.0`, Harmony).
  Build recipe: reference `BepInEx/core/{BepInEx.Core,BepInEx.Unity.IL2CPP,0Harmony,
  Il2CppInterop.Runtime}.dll` and the interop assemblies needed
  (`Assembly-CSharp`, `UnityEngine`, `UnityEngine.CoreModule`, `Il2Cppmscorlib`,
  `Il2CppSystem`, `Il2CppSystem.Core`), all with `Private=false`.
  Note: a `%(Item.Filename)`-batched `<Reference>` wildcard over the interop folder does
  **not** resolve; list the assemblies explicitly.
- Live in-game result: `Loading [DRG Survivor co-op probe 0.0.2]` → `probe: Load() called`
  → `probe: Harmony patches applied` → `Chainloader startup complete`. Custom managed code
  runs inside the game.
- Probe hooks: `GameController.StartGame` / `OnCoreRunStarted` (captures the run's
  `player`), `Player.Awake`, `AI_Manager.Update` (samples every 120 frames), and
  `Entity.TakeDamage`. It writes `probe.log` next to the exe plus the BepInEx log.
  Signals captured: player position, `CurrentHP`, `isAlive`, live `AI_Manager._units`
  count (peak), and player damage events.
- `GameController.stageIndex` is **not** exposed to interop (no generated property), so
  stage reporting was dropped from the probe.

## Next steps

- Build the puppeteer: spawn a remote-player puppet from the game's own player prefab and
  drive it from the received positions (the transport already carries them), then start the
  host-authoritative combat/enemy layer sized for the measured swarm.

## Transport milestone (two instances, verified)

- **Two instances run side by side on one PC** — no single-instance mutex. Both stayed alive
  (~1.34 GB each) at 1280x720/900x600 windowed.
- **Transport works:** BepInEx plugin + **LiteNetLib 2.1.4** (`lib/netstandard2.1` build —
  the `net8.0` build will not load on BepInEx's .NET 6 runtime). Host listens on UDP 9050,
  client connects to loopback; role is chosen with `-coophost` / `-coopclient`
  (plus `-coopaddr=`, `-coopport=`, `-cooplatency=`). Latency simulation is wired up for
  later loss/latency tests.
- **Bidirectional player-state exchange sustained in both directions** over ~70 s at the main
  menu: host `sent=1427 recv=1376`, client `sent=1359 recv=1409` (every 3rd frame ≈ 20 Hz,
  unreliable channel). Evidence: `local/dumps/twoclient-host.log`, `twoclient-client.log`.
  (Positions read `(0,0,0)` because both instances sat at the menu; real movement needs both
  in a run.)
- Each instance writes its own `coop-<role>.log`, because **BepInEx's `LogOutput.log` is
  shared by both instances** in the same install folder and gets clobbered.

### Environment gotchas found (needed for any two-instance work)

- **The game profile/save folder is per-user, not per-install:**
  `%USERPROFILE%\AppData\LocalLow\Funday Games\DRG Survivor\` (Steam-Cloud synced).
  Two instances, and any test copy, all read and write the player's saves
  (`drg_save_slotN_*.dat`, `drg_current_run_slotN.dat`, `localprefs.dat`). Rule: back the
  folder up before testing and restore after; a proper fix (per-instance redirection) is
  still needed before real co-op play. Backup taken at
  `local/backup/localLow-DRGS-*`.
- A second instance occasionally comes up but stalls (no `Update`), so a two-instance run
  must assert both sides are ticking before trusting the result.

## Design decisions (design decisions)

Requirements set by the project author (not derived from the game).

- **Player count: 2.** Do not design around 4.
- **Upgrades are per player** — both dwarves level together, but each picks their own card. (An
  exception for some shared upgrades may be discussed later; not now.)
- **Difficulty stays as-is** for now — no scaling for the second dwarf.
- **Objectives and loot are fully shared**, with one important exception: **drops are consumed**.
  If one dwarf picks up an XP orb or any other drop, it must disappear for the other. So pickups
  need ownership and removal sync, not just position sync.
- **Objectives are the hard part.** Examples:
  - "Stand in a circle for a few seconds" objectives should **progress faster when both dwarves are
    in the circle**.
  - **The drill** (appears after a set time to advance to the next stage) requires **both dwarves
    to be present** to advance.
  - Newer reward/objective screens have not been tested at all yet.
- **Death and downed state:**
  - An **off-screen indicator** — a floating icon that always shows where the other dwarf is when
    they are outside the local camera view.
  - A **revive mechanic**: the surviving dwarf stands within a circle around the dead dwarf for
    roughly **5 seconds** to revive them.
- **Meta progression uses the HOST profile for both dwarves** (design decision, supersedes the earlier
  "shared real profile"). Unlocks come from the host only: if the host has the Driller, the partner may
  play it, otherwise not. Progress is credited to the host's profile for **both** characters, so a host
  on Scout with a partner on Driller advances both the Scout and the Driller in the host's save. The
  partner's own save is neither read for the run nor written. How the client's run results are fed into
  the host's save path is **unknown** and must be measured before it is built.
- **Team economy:** Gold and Nitra are one shared pot; spending (shop, rerolls, artifact rerolls) is
  arbitrated by the host. **Bosco:** one per player, host simulates both. **Supply Pod:** one global
  pod, each player picks their own artifact. **Damage model:** client weapons run locally, the client
  reports hits and the host applies them (cheat exposure accepted for 2 friends).
- **The host picks the mission**, and the client must **drop into the same mission**, with the same
  drill and same level state. This is the "settings adoption" problem, and it should be fully
  automatic rather than matched by hand.
- **Scope for now: main missions only** (Elimination, Escort Duty). Other run types do not have to work
  yet, and **Egg Hunt is explicitly out of scope** (it needs the Heavy Duty DLC).
- **Intended join flow (later):** the host picks a mission, the co-op partner **joins over Steam**,
  the partner presses **Ready**, and only then can the host start. So "ready" is a real gate, not a
  cosmetic button.

  This matches the automation findings nicely: on this profile the **only unlocked run type is the
  campaign Mission Road** (`UIChallengeSetButton_MissionRoad`), with Mastery, Endless, Anomaly and
  both fixed runs reported `[unavailable]`. So main missions are not just the wanted scope, they
  are the only reachable one.

