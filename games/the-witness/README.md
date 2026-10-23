# The Witness (2016)

| | |
|---|---|
| Engine | In-house engine (Thekla). Family not confirmed: `detect_engine` matched no signature |
| Versions tested | Steam build `11370412` (app `210970`), `witness64_d3d11.exe` SHA-256 `8d672d444df6a6df7130f25a517bdab1af92731fe2138dc5b324d519561d55a5`, Windows |
| Projects | [witness-coop](https://github.com/TommoDotCommo/witness-coop) (MIT, dev preview). Related tooling: [witness-randomizer](https://github.com/jbzdarkid/witness-randomizer) (MIT), [witness-trainer](https://github.com/jbzdarkid/witness-trainer) (MIT) |
| QA level | automated (narrow: three tutorial puzzles, localhost, see QA) |
| Last updated | 2026-10-22 |

## Where things stand

No other co-op for The Witness was found. [witness-coop](https://github.com/TommoDotCommo/witness-coop) is a dev-preview prototype that runs on one PC with two copies of the game:
each player sees the other as a puppet drawn through a Direct3D 11 overlay, and when one player solves a panel the other
game marks it solved and the door it gates opens, started by the game's own door function. The detector watches all 594
panels and 327 doors, but only three tutorial puzzles have automated natural solves and state comparisons: the first two door
puzzles and a black/white puzzle that powers a following panel. Panel power-on progression syncs separately from solved state,
and one cable (lit by a tutorial maze) is verified. A fresh peer (restart or first-time late join) and a same-process save reload both resync from the other game's state. Power-off switching, other cable types, lasers, plates and bridges remain unsynced.

## Existing work

No co-op or multiplayer project. Searched and not found: this repo, GitHub issues, PRs
and repository search, web search. Not checked: Nexus, ModDB, Reddit, archive.org, game
and speedrun Discords, non-English communities (see the directory entry's `searched:` list).

Tooling that reaches into the game:

| Project | License | Last push | What it does |
|---|---|---|---|
| [witness-randomizer](https://github.com/jbzdarkid/witness-randomizer) | MIT | 2026-01 | Reads and writes entity fields (panels, doors) from outside the process |
| [witness-trainer](https://github.com/jbzdarkid/witness-trainer) | MIT | 2026-09 | Camera position and angle, noclip, save control, panel state, a door-open function call, found by signature scan |
| Archipelago `worlds/witness` | see repo | n/a | Randomizer client runs beside the game; how it hooks the game was not read |
| [TheWitnessExplorer](https://github.com/ClementSparrow/TheWitnessExplorer) | none stated | 2021 | Python asset exploration |

## How the game works (for co-op)

Observed on build 11370412 (details and sources in [NOTES.md](NOTES.md)):

- Single-player, no netcode found. Assets are in `data-pc.zip`.
- A global entity table (`GLOBALS` is `0x62D0A0` from the module base) holds a pointer array indexed by entity id.
  Entity types have names reachable from each entity (`entity+0x08` to a descriptor, name at `+0x08`): 48 types,
  among them `Machine_Panel` (594), `Door` (327), `Power_Cable`, `Pressure_Plate`, `Multipanel`, `Human` (the player, id `0x1E465`).
- Camera position (3 floats) and angles (2 floats) are at static addresses and follow the player.
- Panel solved state: a float at entity `+0x294` (`1.0` when solved), an int at `+0x298` ("ever solved"), a state int at `+0x280`.
  A door is open when its power word at `+0x1D0` is `1.0`.
- A solve changes few things in memory (15 entities for the first panel): the panel's own fields, power words on the
  target door and its group node, and the transforms of the door parts. Writing those fields into another game did **not**
  open its door; the opening starts from a function call (`base + 0x8E4B0`, the one the trainer calls).
- Launching: the exe shows a graphics options dialog first. `-skip_config_dialog -windowed -new_game` start the world with no input.
- A proxy `AnselSDK64.dll` (the exe loads it from its own folder) loads our code into the game. It hooks `IDXGISwapChain::Present`
  and draws screen quads at world positions (camera model: z up, vertical field of view about 60 degrees, checked against
  screenshots at two headings and while walking). Moving a panel entity's position field does not move it on screen.

Not known: how to sync cables and powered panels, other entity effects, saves and new games, and whether other store builds
differ (the randomizer already knows two builds with different offsets).

## What we changed

Nothing in the game files; the real install was never modified. In disposable copies we added a proxy DLL with: pose exchange
and puppet overlay, a detector for solved panels and opened doors, a reliable event layer (numbered, acknowledged, resent,
applied once and in order, with epochs and a full resend when a peer restarts), and an applier on the render thread with an
echo guard. The source is in [witness-coop](https://github.com/TommoDotCommo/witness-coop).

## QA

A scripted suite (`tools/qa.sh` in witness-coop) restarts both games, has a bot play player A through the mod's in-process control
(no window focused, no desktop input: teleport to the first panel, aim, click to enter it, trace the line, click to finish), and compares the solved panels and open doors
read from both games' memory. Last run (2026-10-22, one machine, localhost, build 11370412); two agents also ran the baseline at the same time, beside an MDK2 agent, with no collision and no window ever taking the player's focus:

```
baseline                                 PASS   identical (1 panel, 10 doors); B sent nothing back; real saves and mouse untouched; game at 30 fps
lost first send of every event           PASS   identical; A resent all 11 events
MUTANT: no resend + lost first send      PASS   games diverged (panel 0x64 and door 0x34eda only on A), as required
MUTANT: frame cap off                    PASS   game A read 1123 fps (uncapped), as required
MUTANT: echo guard off                   PASS   B echoed its 2 events back, as required
ALL OK
```

Earlier checks: read-only memory probes before and after a solve; a scripted walk after which the other game's log showed the
first game's position (-199.82, -119.82, 3.73) against its real camera (-199.819, -119.816, 3.727), 31 ms old; screenshots of
the puppet and of overlay markers on known panels.

Additional targeted run (2026-10-22, witness-coop `df7b276`): peer restart catch-up and its disabled-resend mutant both PASS.
The original first-panel solve reaches both games and is acknowledged before B is restarted with a fresh world. Surviving A
retains its PID and state; fresh B recovers all progress (one panel, ten doors) without echo. With catch-up disabled, B is
missing exactly panel `0x64` and door `0x34eda`. Real saves, cursor and frame-cap checks pass. This exercises a full-state
resend after the original deltas were already acknowledged.

Targeted run (2026-10-22, witness-coop `cfb67a0`, command exit 0): clean and skip-zero-mutant both PASS. Save isolation, free
cursor and 30.0 fps checks passed. The second tutorial bot (`wbot2.py`) exposed a real adapter bug: the detector started at table index 1, skipping the door at
index 0 that Tutorial Bend (`0x182`) opens. Before the fix, both games had the two panels solved, but only A had the second door
open. Including index zero gives identical state (2 panels, 11 doors); the skip-zero mutant preserves both solved panels but
loses exactly that door on B. The bot traces the real three-node bend and checks solved state in memory. Its standing point
and cursor path are calibrated for a 1920x1080 back buffer.

The entity manager stores a base ID at `globals+8` (observed 1). The mod and its probes use table indices; game target fields
use engine IDs. Disassembly at `exe+0xB9F00` subtracts that base before looking up the target and opening its door. Thus the
second panel's raw target 1 means table index 0. This corrects the earlier interpretation of the first panel's target as an
Inanimate entity; it too names a door after normalization. `wpanel.py` reports the graph and both target namespaces.

Panel power-on progression now uses a separate reliable effect (kind 3). The applier calls the game's panel power-on
function at `exe+0xC2890`, type-checked and prologue-checked, on the render thread. Requested power is `+0x2A4`;
the game updates visible power at `+0x2A0`. Activating a target does not mark it solved. The core tracks power in startup
stability and snapshots and has a separate echo guard. This is power-on progression; it does not replicate power-off switching.

The natural black/white tutorial solve at table index `0x18AF` powers `0x1B`. Before the new event B learned the source solve
but its target stayed dark. A new bot traces the middle horizontal route and requires the target to power on while remaining
unsolved. The clean real-game scenario also restarts B after the original effects are acknowledged, then compares solved
panels, doors and powered panels in both processes. The disabled-power mutant must leave only `0x1B` unpowered on B.
Power-on T0 tests cover loss, refused calls, no echo, unsolved targets and restart snapshots, with four caught twins.
Protocol v2 uses `WTP2`/`WTE2`/`WTA2`; both peers need the updated DLL.
Targeted run (2026-10-22, witness-coop `9799af8`, command exit 0): clean and disabled-power mutant both PASS. The clean
case includes a fresh B restart after original ACKs and matches 2 solved panels, 10 open doors and 238 powered panels. The
mutant has 237 powered panels on B, missing only the target `0x1B`. Both keep the target unsolved. Save hashes, instance saves,
free cursor and 30.0 fps checks pass; no leased games remain. Checksummed v1 event/pose rejection also has T0 coverage with
v2 acceptance controls.

Verified so far: the first three tutorial puzzles (two door gates and one black/white power-on chain), one cable maze, 30% seeded random loss on localhost, peer restart, first-time late join and a same-process save reload, each with a mutant that must fail.

Not tested: a real network, more than two players, any other puzzle, other cable or gate types, WAN, fullscreen, non-16:9 windows.
The probes and test scripts are in witness-coop.

## How to play

Not playable together yet (prototype only, two copies on one PC). There is no installer or release; the dev setup is in the [witness-coop README](https://github.com/TommoDotCommo/witness-coop).

## Known issues

- Solved panels, opened doors, panel power-on progression and one verified cable are synced. Power-off switching, other cable types, plates, lasers and bridges remain unsynced. Reversible state still needs an authority/conflict policy.
- The puppet is a flat rectangle with a facing dot; it shows through walls (no depth test).
- Same-process save reload is detected by a heuristic (a known open door closing, or two or more known panels unsolved) and resynced; a game where doors close in normal play would trigger a harmless extra resync. Loading a different save slot is untested.
- Test hazard for the next agent: the game keeps saves in `%APPDATA%\The Witness` whatever
  folder the exe runs from. Starting a New Game from a copied install rewrote the player's
  latest existing save and settings file there. Back up and hash that folder first.

## Next steps

1. Extend sync to the other cable-driving panels, plates, lasers and bridges, power-off switching and more puzzles. Check each with a natural solve and a state diff; define authority for reversible state.
2. Real networking (Steam relay or direct IP), then lag runs with the chaos tool (random loss is already covered on localhost).
3. Occlusion and a real puppet model instead of a flat rectangle.
4. Widen the suite to more puzzles and gate types and keep the QA level honest; add an installer and a release; read the Archipelago client for another view of the same hooks; finish the unchecked searches.
