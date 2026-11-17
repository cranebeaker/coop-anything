# BioShock Infinite (2013)

| | |
|---|---|
| Engine | Unreal Engine 3, licensee build (package 727 / licensee 75), D3D11, Morpheme animation |
| Versions tested | Steam, build 10404041 (exe SHA-256 `C20A4252…0E77`, full hash in [NOTES.md](NOTES.md)) |
| Projects | BioShock Infinite Coop, built with this playbook (packet format v26; last player package v0.12; not public yet, no directory entry until it is) |
| QA level | automated (12 tests on two real game clients, see QA) |
| Last updated | 2026-11-08 |

## Where things stand

Two players can play the campaign side by side, each in their own game, and see each other: the partner's Booker
walks, runs, jumps, crouches, aims and fires, and their vigors, bullet impacts and bullet holes show up. Enemies are
run by the host: on the client they have no AI of their own and copy the host's position, gait, crouch, actions and
shots (their real weapons fire, so the client's game decides hits), and AI on both sides sees and attacks both
players. Elizabeth, doors, loot, story progress and cutscenes are not shared. Everything is tested on two copies on
one PC, at a few save points; it has not been tested between two PCs, and the player package predates this work.

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
  and a DLL exchanges state, drives a puppet of the partner and steers the client's enemies.
- **In-game bridge.** A `d3d11.dll` proxy reads GNames/GObjects, calls any UFunction through
  `ProcessEvent` and hooks native functions by address (the exe has no ASLR). The co-op module lives in the same
  DLL and runs every frame on the game thread.
- **Positions are floating sections**: `Location` is not a property in this build. Write
  `XPrivateLocalLocation`/`Rotation` and call `ForceUpdateComponents(bTransformOnly=True)`.
- **A second `XHuman` built from the player archetype crashes the game** (its `XPawn.ActionModel` is never
  created). The partner is an `XProtectionObjectiveActor` (a skeletal-mesh actor with a faction) carrying
  `Booker.Booker_MESH` (stored only in the lighthouse levels, preloaded with `ClientPrepareMapChange` and rooted).
  With `FactionID` 0 and `bTargetableForAI` set, enemies target it like a player. It borrows the extras'
  Morpheme tree and the soldiers' locomotion and aim sets.
- **Enemies move by animation, not velocity.** Their Morpheme action model reads a movement model the AI keeps up to
  date, and an AI weapon fires along the animation aim tracker (`ActionModel.AimTracker` and its child trackers),
  not toward the target it is given. Driving an enemy means writing those, not `Velocity`.
- **Saves go through Steam Cloud** (`ISteamRemoteStorage`), not files: test copies wrap that interface
  so they never touch the owner's cloud.
- **Effects** are played from script through `XPlayerController.ClientScriptedPlayEffect` with a borrowed
  `XSeqAct_PlayEffect`, or by watching the shooter's `XEmitterPool` and `DecalManager.ActiveDecals` and recreating
  what appears there. Beam endpoints (`SetBeamTargetPoint` is native and not exposed) are written straight into the
  emitter instance at +0x210.
- **Pause menu** is filled in C++ (`0x00b43480`, plus the label, list, grey-out and choice functions it uses); hooking
  them adds a MULTIPLAYER page.

Details, addresses and dead ends: [NOTES.md](NOTES.md).

## What we changed

| Feature | How | Test |
|---|---|---|
| Two copies on one PC, owner files guarded | `-AllowMultipleInstances`, per-copy pipe, `dinput8.dll` file guard, Steam Cloud wrapped into the copy | owner file hashes before/after every run |
| Transports | UDP host/client and Steam P2P (flat API), 30 Hz pose packets, jitter buffer; per-launch session id so a restarted partner is picked up again; received values validated; mod version checked on all six packet types | `essai_relance` (one copy restarts alone); `essai_coop` in 3 modes (last run in player modes 2026-10-30) |
| Partner puppet | Booker mesh, walk/trot/run/sprint, jump, crouch, strafe, blends; waits for the extras' archetype instead of a T-pose; hidden when overlapping the camera; hangs from the skyline hook when on a rail | `essai_synchro`: 60-70 ms delay, 0.8-4.0 units mean error, 0 freezes, 0 teleports |
| Partner weapon | third-person model on the `R_Grip` bone, unarmed / pistol / two-handed / shotgun stances; at rest the body turns so the barrel points at the aim and NPC aim poses follow pitch; muzzle flash, recoil | `essai_arme`, `essai_tirs`: barrel within 0.2-1.5° of the aim |
| Shot impacts and bullet holes | impact particles relayed from the shooter's `XEmitterPool`; decals diffed from `DecalManager.ActiveDecals` and recreated | `essai_tirs`: shotgun 5/5 holes, machine gun 20/20 on both copies |
| Partner vigors | cast sound; projectiles and traps mirrored on timestamped trajectories, with bounces and looping trap effects; Shock Jockey crystals, arcs and short-cast bolt; area effects (Bucking Bronco, Undertow, Return to Sender); Murder of Crows flock | captures only (no automated test) |
| Shared enemies | identity = spawner + deaths per spawner; damage, shields, deaths exchanged | `essai_ennemis` on a station with 6 enemies |
| Host-authoritative enemies | AI targets the partner puppet; on the client, enemies are suspended (`bIsSuspended`, `Physics` 0) and copy the host 15 times a second: position, crouch, gait, stops and turns; host shots replayed through the enemy's real weapon; host actions (reloads, dodges, side steps, baton attacks) replayed in place | `essai_cible`, `essai_pilote`, `essai_actions`, `essai_marche` |
| Turrets | destroyed turrets switched off on the other side; a possessed turret's faction is copied and its mirrored bullets do no damage | `essai_tourelle`, `essai_possession` |
| Options | MULTIPLAYER pause-menu page: collisions, friendly fire, alt-tab pause, test cheat, join partner (with the reason when joining is not possible) | menu and collision checks on captures (2026-10-31) |

## QA

```
python -I outils/essai_tout.py [--complet]
```

Two test copies on one PC, both driven through the bridge's pipes. It reloads one save per group (`BW2` Battleship
Bay, `TWN2` Columbia without enemies, `TWN3` station with 6 enemies and a turret), runs each test, and prints one
line per step. Each test writes a `resultat.json` and fails if any owner file hash changed. `--complet` adds the
puppet circle test; a full run takes about 24 minutes.

| Test | Asserts | Last result (2026-11-07) |
|---|---|---|
| `essai_tirs` | puppet barrel within 6° of the aim at 0/+30/-30°; shotgun shot played, impacts replayed, same bullet-hole count; machine-gun burst within 2 holes | OK: 1.1/0.9/1.5°, 5/5 and 20/20 holes |
| `essai_relance` | one copy restarts alone, picked up again with 0 rejected packets, its shots still arrive | OK |
| `essai_synchro` | 18 scripted moves through real input: mean error <= 25 units, <= 10% stutter, no teleport, expected animation | OK: 60-70 ms delay, 0.8-4.0 units, 0 stutter, 0 teleports |
| `essai_arme` | weapon in hand, barrel within 10°, first partner shot played, one weapon visible, two weapons | OK: 0.2° (pistol), 0.7° (machine gun) |
| `essai_ennemis` | damage, shields and deaths in both directions, clean-up of enemies killed ahead | OK |
| `essai_cible` | enemies target the puppet, drop it when the partner dies, take it back | OK: 6/6 within 8 s |
| `essai_pilote` | client enemies suspended; bullets 0.6-1.2x the host's; median position gap < 60; crouch agreement >= 70%; local AI back within 3 s when the host goes silent | OK: ratio 0.81, gap 0, 12/12 crouched, 197 real shots, 0 refused |
| `essai_tourelle` | turret destroyed on one side is off on the other within 3 s and stops firing | OK: 0.1 s |
| `essai_possession` | possessed turret mirrored, does not target players, no double damage, faction restored | OK: mirrored in 0.4 s, 78 hits cancelled |
| `essai_actions` | host actions sent and played, <= 20% refused, played in place (95th percentile <= 120 units) | OK: 61 sent, 55 played, 95% under 81 |
| `essai_marche` | client starts/stops <= 1.5x host + 10, in-place turns <= host + 4, backwards walking | OK: starts 25/19, stops 19/17, turns 6/1 (host/client) |
| `essai_coop` (`--complet`) | puppet follows a circle, survives 60 s | OK: 5.7 units mean, 0.0 after stopping |

- Last full run: 2026-11-07, all 12 tests OK in their saved results (the suite's 14-step summary, which also counts
  the enemy-position measurement and one relaunch, is printed only, not saved). Nothing has run since. Of 9 full runs recorded since 2026-11-04, 2 were fully green, the latest
  included. Most failing runs failed in `essai_synchro`; the failures examined (2026-11-05 to 11-07) came from the
  test's own route, fixed on 2026-11-07 (14 recorded passes in a row since). The other failures were two
  `essai_possession` runs (unexplained damage to the placed enemy) and one `essai_coop` run.
- 409 recorded runs in total during development.
- Not covered by any automated test: vigors and projectiles, friendly fire (tested once by hand), skyline, the
  MULTIPLAYER menu, collisions, Steam P2P and IP modes (last run 2026-10-30), two PCs, two Steam accounts, real
  latency and loss, the player package, a full level played start to finish. Everything enemy-related is measured at
  one station fight.

## How to play

Not released yet. The player package (installer, uninstaller, French readme) is built from the mod repo; both
players need the same Steam build and the same mod version. The last package was built at v0.12 (2026-10-31): it
cannot talk to the current build and its readme describes the old enemy model, so it needs a rebuild first.

## Known issues

- Each player has their own Elizabeth, doors, loot, money and story progress.
- The partner's puppet never takes damage from enemies, and bullets pass through it.
- The client takes less enemy damage than the host estimates (about 0.35x in tests), because hits are decided by the
  client's own game; not yet checked in real play.
- Live turrets fire on their own in each game; a turret destroyed by the partner just switches off (no explosion), and
  a possessed turret has no green veil on the other side.
- On the client, some enemy turns on the spot and backward walks are missed, and 9 to 22 % of the enemy actions the
  host sends are not played (`essai_actions`, 2026-11-06/07).
- No bullet tracers; no arm gesture for shots or vigors; the aim pose only plays when the partner stands still.
- Possession of human enemies is not shared and Charge has not been looked at; Undertow and Bucking Bronco only show
  their effects.
- The partner appears about 35 s after loading; at Battleship Bay only once the player walks toward the promenade
  (83 s in the test).
- The partner's Booker always has the bandaged right hand.

## Next steps

1. Rebuild the player package for the current build, then test between two PCs and two Steam accounts, with network
   loss and latency.
2. Test in other levels and in a hand-played session: enemy damage felt by the client, skyline thresholds on a real
   ride, collisions and the menu again.
3. Automated tests for vigors and friendly fire; re-run the turret and possession tests client to host (last
   2026-11-03).
4. Share story state: doors, scripted events, level changes, then Elizabeth.
5. Publish the mod repo (it depends on the owner's private bridge sources) and add the directory entry.
