# The Witness (2016)

| | |
|---|---|
| Engine | In-house engine (Thekla). Family not confirmed: `detect_engine` matched no signature |
| Versions tested | Steam build `11370412` (app `210970`), `witness64_d3d11.exe` SHA-256 `8d672d444df6a6df7130f25a517bdab1af92731fe2138dc5b324d519561d55a5`, Windows. The mod refuses any other exe |
| Projects | [witness-coop](https://github.com/TommoDotCommo/witness-coop) (MIT, public beta [v0.3.0-beta.1](https://github.com/TommoDotCommo/witness-coop/releases/tag/v0.3.0-beta.1), directory entry "The Witness"). Related tooling: [witness-randomizer](https://github.com/jbzdarkid/witness-randomizer) (MIT), [witness-trainer](https://github.com/jbzdarkid/witness-trainer) (MIT) |
| QA level | automated (see QA for what ran when, and what has never been tested) |
| Last updated | 2026-11-11 |

## Where things stand

People can play The Witness together today. [witness-coop](https://github.com/TommoDotCommo/witness-coop) is in public
beta (v0.3.0-beta.1, 2026-11-10): each player runs their own copy, up to 16 in a lobby, and everyone walks the same
island as an animated character. A panel one player solves is solved for everyone, with the line in the solver's colour,
and what it sets going (doors, cables, lasers, bridges, elevators, ramps, water levels) moves in every game. Friends
join through a Steam friends-only lobby or directly, and can join a game already under way. It installs with one Setup
file and starts from a launcher.

What is not established: nobody has recorded a full co-op playthrough to the credits, a session between two PCs with
two Steam accounts has no recorded result, and Steam Deck and Linux support is untested on real hardware.

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
- A proxy DLL in the game folder (first `AnselSDK64.dll`, now `dsound.dll`) loads our code into the game. It hooks `IDXGISwapChain::Present`
  and draws screen quads at world positions (camera model: z up, vertical field of view about 60 degrees, checked against
  screenshots at two headings and while walking). Moving a panel entity's position field does not move it on screen.

- The exe imports `DSOUND.dll` and the game folder does not ship it, so a `dsound.dll` there is loaded at start and can
  forward to the system copy. Under Wine the built-in `dsound` wins unless overridden for the exe.
- After the solved fields, the solve handler runs name-keyed behaviours (`exe+0xB7F80`) that read the drawn line, and
  tells the panel's `Bridge` entity; see NOTES.md, "from prototype to public beta".
- Saves go to `%APPDATA%\The Witness` whatever folder the exe runs from; video options are a text file there.

Not known: whether other store builds differ (the randomizer already knows two builds with different offsets).

## What we changed

Nothing in the game's files. The mod is a proxy `dsound.dll` (a system DLL the exe imports and the game does not ship)
plus a launcher and a models folder; the source and the evidence for each item are in
[witness-coop](https://github.com/TommoDotCommo/witness-coop) and [NOTES.md](NOTES.md).

- **Other players as characters.** A hook on Direct3D 11 `Present` draws each remote player as a skinned, animated
  model, hidden behind walls with the game's own depth buffer. First it was a flat rectangle that showed through walls.
- **Shared progress as reliable events.** Solved panels, doors, panel power, cables and lasers are numbered,
  acknowledged and resent events with epochs, an echo guard and a full-state resync for a peer that restarts, joins
  late or reloads a save. Doors and power are started by the game's own functions; writing the fields did not open a
  door.
- **A remote solve runs what the solver's solve ran.** The game's solve handler runs about 70 behaviours chosen by the
  panel's name, and several read the drawn line. Marking a panel solved left bridges, ramps and water levels behind.
  The fix writes the solver's line into the panel's trace, runs the reactions, and guards what they change from
  being echoed (`tools/qa.sh react`, `bridge`, with twins that must fail).
- **Doors are reported when told to move, not on arrival.** A sliding bridge used to reach the other game 40 s late.
- **Groups.** One remote peer became a roster of up to 16, with per-recipient acknowledgements and versioned
  events for reversible state (cables). Found by an eight-world test: one peer's ACK must not retire an event for
  everyone, and a time-only echo guard swallows a genuine quick reversal.
- **Loading.** The proxy moved from replacing the game's `AnselSDK64.dll` (Steam restores it on every update, mod
  managers cannot install it) to `dsound.dll`, so installing is a file copy and no game file changes.
- **Playing with friends.** A launcher with a lobby (chat, voice, ready, co-op or race, new game with parts of the
  island done), Steam friends-only lobbies over the game's own Steam connection, direct UDP with LAN and Tailscale
  discovery and invite codes, a version check and an updater that verifies a SHA-256.
- **Social layer.** Text chat, pings, proximity voice with channels, an emote wheel, drawing on the world, watching
  another player's view, a hold-Tab player panel, and controller support.
- **New puzzles.** Community randomizers (MIT) built into the launcher, and an editor for the player's own puzzles;
  in a lobby the host's choice, seed and puzzle set reach everyone before the game starts.
- **Shipping.** One `Witness-Coop-Setup.exe` that finds the game, checks the build hash and writes a receipt per
  file; a zip for manual installs, a Vortex extension, a Mod Organizer 2 plug-in and a Linux/Proton install script.

## QA

Three tiers, all in witness-coop. Each protection has a mutant (a deliberately broken build or switch) that the test
must fail.

- **Game-free** (`tools/t0.sh`): the sync core in the hub's fuzzer over a lossy link with bug twins, the group and
  Steam transports, lobby, launcher, updater, randomizers, voice mixer, playthrough planner and installer logic.
- **Two real games** (`tools/qa.sh [scenario]`): a bot inside the game (no window focus, no desktop input) plays, and
  solved panels, doors, power and cables are compared from both games' memory. Scenarios cover the baseline solve, lost
  and random packet loss, peer restart, late join, save reload, power chains, cables, bridges and solve reactions,
  shared lines, lobby and launcher flows, race mode, voice, randomizers and the puzzle editor.
- **N real games** (`tools/playthrough.sh N`): bots play the island in the order of Archipelago's logic and every solve
  must appear in every other world.

Last game-free run (2026-11-10, at tag `v0.3.0-beta.1`, commit `be974a7`):

```
tools/t0.sh
51 tests, 0 failed, 288 s wall
```

Last two-game run (2026-11-11, same tag, one PC, four parallel shards, 28 minutes):

```
tools/qa.sh
40 scenarios: 39 PASS, 1 FAIL
  17 clean scenarios, 16 PASS: baseline, lost first send, 30% random loss, second tutorial door, panel power chain,
     cable chain, peer restart catch-up, late join, same-process reload, save point restore, preset apply (twice),
     bot-free effects, partner view, dev menu, your puzzles sent by the lobby, your puzzles alone continued
  23 MUTANT scenarios, 23 PASS (each broken build was caught)
  FAIL: your puzzles in the game
```

The failure was the test bot, on one of that scenario's ten puzzles: it could not steer the editor's solution to the
end of the line on panel `0x295` ("steering: the end stub was not reached"); the other nine were judged alike by the
game and the editor's rules and reached the second game. Re-run alone straight after, the scenario passed, with that
puzzle accepted. So the bot's drawing on that panel is intermittent; why is not diagnosed.

Never tested: two PCs with two Steam accounts (the steps are written in the mod's `docs/STEAM.md`; no result is
recorded), a real Steam Deck or Linux PC, a person speaking into a real microphone, sixteen real game clients at once,
a bot run that reaches the credits, stores other than Steam.

<details>
<summary>The prototype's suite results of 2026-10-22 (first three tutorial puzzles, localhost)</summary>

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

</details>

## How to play

Every player needs their own copy of The Witness on Steam (build 11370412) and the same version of the mod.

1. Download `Witness-Coop-Setup.exe` from the [latest release](https://github.com/TommoDotCommo/witness-coop/releases)
   and run it. It finds the game, checks its version and installs; it changes none of the game's files.
2. Press Play on The Witness in Steam. The co-op launcher opens first.
3. Host: **Host a game**, **Through Steam**, **Open lobby**, then invite friends through Steam. Friends accept the
   invite, or paste the lobby number into **Join a friend**. Direct play on one network or over Tailscale is the
   **Direct** choice.

Details, keys, controller setup and Linux: the [mod's README](https://github.com/TommoDotCommo/witness-coop#readme).

## Known issues

- The files are not code-signed, so Windows SmartScreen warns before the first run.
- Each player calls and rides their own boat and plays the theatre's videos for themselves. The ending and final
  elevator, the timed challenge and the sound-room birds are not run by a remote solve.
- Only the current Steam build works. After a game update co-op switches itself off until the mod is updated; GOG and
  Epic copies are refused.
- Everyone in a lobby needs the same mod version; the launcher says so and offers the update.
- A mod that replaces DirectSound completely cannot run beside it (one that is a `dsound.dll` proxy can be chained).
- Steam Deck and Linux through Proton are untested on real hardware.

## Next steps

1. Run and record the two-PC, two-account Steam session from the mod's `docs/STEAM.md`, then a lag run with the chaos
   tool over that path.
2. A recorded co-op playthrough to the credits (the bot route is 238 panels), which is what `verified-playthrough`
   needs.
3. A real Steam Deck or Linux run of `install-linux.sh` and the launcher in Game Mode.
4. Sync what a remote solve still leaves out (boat, theatre, challenge); `tools/react_sweep.py` lists every entity
   field that differs after a solve.
5. Finish the unchecked searches for other co-op work (Nexus, ModDB, Reddit, archive.org, Discords).

Test hazard for the next agent: the game keeps saves in `%APPDATA%\The Witness` whatever folder the exe runs from.
Starting a New Game from a copied install rewrote the player's latest save and settings file there. Back that folder
up and hash it first, or use the mod's `save_dir=instance` redirect.
