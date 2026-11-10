# Kill The Crows (2023)

| | |
|---|---|
| Engine | Unity 2022.1.5f1 (Mono, x64) |
| Versions tested | Steam, appid 2441270, buildid 16441009 (StreamingAssets build_info: built 2024-11-18) |
| Projects | **KTC Co-op** (built from zero in this hub; mod source in [games/kill-the-crows/Mod](Mod/)) |
| QA level | automated |
| Last updated | 2026-11-09 |

## Where things stand

No co-op existed for this game anywhere before this project. A first playable
two-player co-op mod now exists and was verified with two real game clients on one
machine (LAN loopback):

- Both clients connect over UDP 7777 (handshake → Welcome → modifier sync);
  hotkeys: F9 host LAN / F10 join LAN / F6 host Steam lobby / F7 join Steam lobby
  / F11 disconnect.
- The host's run (Scene_Ingame) pulls the client in via scene-sync; both pick their
  own loadout in Scene_Loadout independently (weapon/showdown/passives are per-player).
- Enemy spawning is host-authoritative; the client renders proxies built from the
  same prefabs (`WaveManager.enemyCharacterDictionary`), driven by 20 Hz state packets
  (position, rotation pivot, sprite flip, HP, animator shortNameHash + normalized time).
- Damage is decided by host physics: the client's shots become host-side bullets
  spawned at the client's position/direction (`Fire` events), the client's player is
  a hurtbox-carrying ghost the host's enemy bullets hit, and the resulting damage is
  routed to the client as `DamagePlayer` events (dodge invincibility is synced via
  state flags, so dodge-phase hits become OnDodgeSuccess on the client).
- Enemy deaths, shared kill counter (boss pacing uses the shared counter via a
  `WaveManager.OnKillCountIncrease` patch), intensity chunk level, phase (BGM),
  showdown time-scale ownership, pause and game-over are all synced.
- Automated two-instance test with input bots: 2.5–3 min runs with **zero exceptions
  and zero disconnects**, ~107 shared kills ↔ 107 client-side proxy deaths, client
  death → `ClientDied` → spectate → both dead → shared game over, all observed in logs.
- Revive cycle (client bot without god-mode): three full death → 5 s → revive rounds
  (pool 2 → 1 → 0), each returning the client to the fight, then the final death with
  an empty pool stopped wave spawning (`FinalizeRun`) and ended the run on both sides;
  zero exceptions across the whole 5.5 min run.

A friends-only **Steam lobby path** also exists now (host F6 creates the lobby,
client F7 searches and joins; version-filtered lobby data keeps mod versions from
mixing). Steam itself blocks a full local test on one machine — a user's own lobbies
are excluded from matchmaking search and self-P2P is not allowed — so the lobby code
is boot-tested only (create/join callbacks, handshake wiring verified in logs with
auto-retry); a real session needs two distinct Steam accounts. The LAN path below is
the fully tested one.

**Partner revive** is in (v0.3): a shared pool (default 3, config `RevivesPerRun`) of
revives per run. A downed player comes back after `ReviveDelaySec` (default 5 s) next
to the survivor (`ReviveAtPartner`, configurable) with a `ReviveInvulnSec`
invulnerability window; the run finalizes only when a death cannot be paid for
(empty pool) or the partner is already down. Remaining v2 candidates: reconnect
support, and the modifier set still belongs to the host's run (client's own equipped
modifiers are overridden for the shared simulation).

Fixed since v0.1: both players now see each other (bidirectional ghost sync + host
fire tracers), the dead player's camera spectates the surviving partner, the
client's loadout (bullet prefab, explosive params, showdown data, passives) is
applied to its ghost on the host, and after a shared game-over the client can
navigate menus freely again. The auto-test host also refuses to start a run before
the client has actually handshaked.

## How the game works (for co-op)

- Unity 2022.1.5 Mono: all game logic in `revolver_Data/Managed/Assembly-CSharp.dll`
  (356 decompilable classes), mods load with BepInEx 5.4.23.2 + HarmonyX. No anti-cheat.
- It is a single-arena wave roguelite: one gameplay scene (`Scene_Ingame`). Scenes:
  `Scene_Logo → Scene_Main → Scene_Loadout → Scene_Ingame (→ Scene_Ending)`.
  Co-op only needs to sync Scene_Ingame plus the transitions.
- `WaveManager` spawns enemies from `WaveData`/`SpawnData` at `EnemySpawnPoint`s;
  bosses spawn from `bossPrefabs` every N shared kills. Enemy AI (14+ types) is a
  per-frame state machine reading the `Enemy.player` field — retargeting that field
  to the nearest alive player per frame is enough for two-player targeting.
- Bullets are raycast-driven (`Bullet.FixedUpdate` + `CollisionDetection`), spawned by
  `PlayerAttack.ShootBullet` (player) and per-type enemy AI coroutines (enemies).
  One hit usually kills: HP is 1 (+1 with the OneCoinMore gear).
- `PlayerShowdown` (constellation bullet-time) changes the global
  `TimeManager.showdownTimeScale`; co-op syncs that scale with last-writer-wins per
  showdown episode.
- Damage entry points: `Hurtbox.OnHurt → Character.OnHurt(Bullet)` and
  `Character.OnHit(BulletOnHit, victim)` for melee/showdown hitboxes.
- Saves are PlayerPrefs under `AppData/LocalLow/5minlab/revolver` — shared by every
  copy on the machine; isolate before two-client tests if you care about the profile.
- Known vanilla edge: killing a boss with a corpse prefab leaves
  `CameraManager.camTarget` on the destroyed transform for a few seconds until
  `EndCutScene` fires (NRE spam in Player.log, cosmetic; vanilla single-player hides
  it behind cutscene invulnerability).

## Gotchas found while building the mod

- **The game destroys DontDestroyOnLoad root objects during startup scene
  transitions** (probe confirmed: arbitrary DDOL roots die before the first frame
  completes). Session state must live in statics and the pump MonoBehaviour must be
  recreated on `SceneManager.sceneLoaded`.
- **`System.ValueTuple` is not loadable** in this Unity Mono runtime — any mod type
  with a tuple field/signature throws `TypeLoadException` at first JIT. Use structs.
- Comparing `Environment.TickCount`-style values against millisecond constants is a
  classic bug — store `DateTime.UtcNow.Ticks` and convert constants with
  `TimeSpan.TicksPerMillisecond` (a tick/ms mixup made the handshake time out in
  0.6 ms).
- Disabling a stripped enemy's `Enemy` component fires `Character.OnDisable` and
  removes it from `Character.Characters`; proxies must be re-added for aim assist /
  showdown targeting to see them.
- `steam_appid.txt` ships with the game, so `SteamAPI.Init()` works for two
  simultaneous local instances; both were fully playable in tests.

## QA (automated, this hub)

Two game instances on one machine, launched with `-ktcHost` / `-ktcClient`
(plus `-ktcLog=` for per-instance logs). The mod's auto-test driver hosts, waits for
the client, starts the run, sets god-mode on both players and fires at the nearest
enemy every 0.6 s (reloading when dry), so both damage directions (client→host
bullet reconstruction and host→client damage events) run continuously.

Checks observed across runs: connection + welcome + modifier sync; scene transition
Main→Ingame on the client within a second of the host; proxy spawn tracking (client
proxy count tracks host enemy count through 100+ spawns); shared kill counter growth
matches client death events exactly; client death → ClientDied → host game-over
broadcast; zero managed exceptions in both Player logs; zero disconnects. Mutation
coverage: none yet (planned per `playbook/fuzzing-coop-sync.md`).
