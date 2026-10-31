# BioShock Infinite (2013)

| | |
|---|---|
| Engine | Unreal Engine 3, licensee build (package 727 / licensee 75), D3D11, Morpheme animation |
| Versions tested | Steam, build 10404041 (exe SHA-256 `C20A4252…0E77`, full hash in [NOTES.md](NOTES.md)) |
| Projects | BioShock Infinite Coop, built with this playbook (v0.12, not public yet, no directory entry until it is) |
| QA level | automated (4 suites on two real game clients, see QA) |
| Last updated | 2026-10-31 |

## Where things stand

Two players can play the campaign side by side, each in their own game, and see each other: the
partner's Booker walks, runs, jumps, crouches, aims and fires, and their vigor projectiles and traps
show up. Enemies are shared for damage, shields and deaths, and loosely for position (2-10 m apart in
combat). Elizabeth, doors, loot, story progress and cutscenes are not shared, and enemies only target
their own local player. Everything is tested on two copies on one PC; it has not been tested between
two PCs yet.

## Existing work

No co-op mod or multiplayer attempt for Infinite was found (triage 2026-10-30). Two official
multiplayer modes, Spec Ops (four-player co-op) and Border Control, were cut in 2012
([Shacknews](https://shacknews.com/article/83435/bioshock-infinites-scrapped-multiplayer-detailed),
[Co-Optimus](https://www.co-optimus.com/article/8743/bioshock-infinite-loses-intended-co-op-modes-in-mid-development.html)).
Their netcode config and 112 `Server*`/`Client*` RPCs still ship in `XCore`.

## How the game works (for co-op)

- **Netcode transport survives, replication does not.** A listen server starts (`IpNetDriverSteamworks`,
  UDP bound), but `UActorChannel` is compiled out of the exe and no cooked property keeps `CPF_Net`.
  Script RPCs cannot cross the wire. So the mod is its own sync layer: each player runs their own game,
  and a DLL exchanges state and drives a puppet of the partner.
- **In-game bridge.** A `d3d11.dll` proxy reads GNames/GObjects, calls any UFunction through
  `ProcessEvent` and hooks native functions by address. The co-op module lives in the same DLL and
  runs every frame on the game thread.
- **Positions are floating sections**: `Location` is not a property in this build. Write
  `XPrivateLocalLocation`/`Rotation` and call `ForceUpdateComponents(bTransformOnly=True)`.
- **A second `XHuman` without a controller crashes the game**, so the partner is a
  `XReactiveSkeletalMeshActor` carrying `Booker.Booker_MESH` (stored only in the lighthouse levels,
  preloaded with `ClientPrepareMapChange` and rooted). It borrows the soldiers' Morpheme tree and
  locomotion sets.
- **Saves go through Steam Cloud** (`ISteamRemoteStorage`), not files: test copies wrap that interface
  so they never touch the owner's cloud.
- **Effects** are played from script only through `XPlayerController.ClientScriptedPlayEffect` with a
  borrowed `XSeqAct_PlayEffect`. Third-person vigor models carry sound only; projectile archetypes
  (`FlyingEffect`, `StuckEffect`, `ExplodedEffect`) are visible from outside.
- **Pause menu** is filled in C++ (`0x00b43480` and three helpers); hooking them adds a MULTIPLAYER page.

Details, addresses and dead ends: [NOTES.md](NOTES.md).

## What we changed

| Feature | How | Test |
|---|---|---|
| Two copies on one PC, owner files guarded | `-AllowMultipleInstances`, per-copy pipe, `dinput8.dll` file guard, Steam Cloud wrapped into the copy | owner file hashes before/after every run |
| Transports | UDP host/client and Steam P2P (flat API), 30 Hz pose packets, jitter buffer | `essai_coop` in 3 modes |
| Partner puppet | Booker mesh, walk/trot/run/sprint, jump, crouch, strafe, blends, hidden when overlapping the camera | `essai_synchro`: 60-70 ms delay, 1-5 units mean error, 0 freezes |
| Partner weapon | third-person model on `R_Grip`, guard stance, aim pitch, muzzle flash, recoil | `essai_arme` |
| Partner vigors | cast sound; projectiles and traps mirrored with their archetype effects | captures (Possession, Devil's Kiss) |
| Shared enemies | identity = spawner + rank; damage, shields, deaths exchanged; host-authoritative position snaps | `essai_ennemis` on a station with 6 enemies |
| Options | MULTIPLAYER pause-menu page: collisions, friendly fire, alt-tab pause, test cheat, join partner | menu and collision checks on captures |

## QA

- `python -I outils/essai_coop.py [--mode-joueur | --mode-steam]`: puppet follows a circle and a walk,
  F8 join; asserts position error and owner files unchanged.
- `python -I outils/essai_ennemis.py TWN3`: damage, shield and death in both directions, cleanup of
  enemies killed ahead; asserts both copies agree.
- `python -I outils/essai_synchro.py`: 18 scripted moves (walk, strafe, sprint, jump, crouch, turn);
  measures delay, error, freezes, jumps and the animation played.
- `python -I outils/essai_arme.py`: two weapons, aim and shots seen from the other copy.
- Last results: `essai_ennemis` OK (2026-10-31), `essai_synchro` 0 freezes, mean error 1.2-4.9 units
  (2026-10-31), `essai_arme` OK (2026-10-30), `essai_coop` OK (2026-10-30). About 100 recorded runs in
  total during development.
- Not covered yet: two PCs, two Steam accounts, real internet latency and loss, vigors other than
  Possession and Devil's Kiss, a full level played start to finish.

## How to play

Not released yet. The player package (installer, uninstaller, French readme) is built from the mod
repo; both players need the same Steam build and the same mod version.

## Known issues

- Each player has their own Elizabeth, doors, loot, money and story progress.
- Enemies only see and shoot their own player, and can be a few metres apart between screens.
- No bullet tracers or world impact effects; arms stay in the guard pose when aiming up or down.
- The partner's Booker always has the bandaged right hand.

## Next steps

1. Test between two PCs and two Steam accounts, with network loss and latency.
2. Tight enemy sync: disable the AI on the non-host copy and drive its pawns like the puppet.
3. Share story state: doors, scripted events, level changes, then Elizabeth.
4. Publish the mod repo (it depends on the owner's private bridge sources) and add the directory entry.
