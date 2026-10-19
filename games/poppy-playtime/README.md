# Poppy Playtime (2021)

| | |
|---|---|
| Engine | Unreal Engine 4.27.2 |
| Versions tested | Steam, buildid 21905565 (UE 4.27.2, Windows) |
| Projects | [PoppyCoop](https://github.com/romannazarov057-spec/PoppyCoop) (build-from-zero, work in progress) |
| QA level | automated (movement sync only; no playthrough) |
| Last updated | 2026-10-19 |

## Where things stand

You cannot play the story together. What exists is a foundation: two real clients, each
seeing the other as a player-shaped puppet standing in the right place, moving both ways
over a relay. Chapter 1's actual co-op — doors, the GrabPack, objectives, Huggy — is not
synced, so a pair cannot progress past the first room as a pair.

## Existing work

- **Nothing playable.** A fan multiplayer mod existed and was deleted by its author (the
  dev confirmed it on Reddit); no mirror found. [Reddit thread](https://www.reddit.com/r/PoppyPlaytime/comments/uut6zj/anybody_have_a_poppy_playtime_multiplayer_mod/).
- **PoppyCoop** — this project. UE4SS Lua mod + Python relay, MIT.
- Two leads that say multiplayer was in the house once: the Steam build ships the VoxeX
  voice SDK (`vivoxsdk_x64.dll`, `ortp_x64.dll`) in `Binaries\Win64`, and Chapter 2 builds
  contain an unused multiplayer minigame ([TCRF](https://tcrf.net/Poppy_Playtime:_Chapter_2/Multiplayer)).
- Steam lists the game as single-player only (no Remote Play Together, no LAN, no co-op
  category), so nothing is available out of the box.

## How the game works (for co-op)

- **Engine / modding:** UE 4.27.2, shipped without editor content but fully scripted.
  [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) v3.0.1 loads Lua mods in-process, exposes
  the game's objects and the console, and needs no compiler.
- **Netcode:** the game has no networking of its own in Chapter 1. Everything is client-side
  and single-player, so co-op has to be built on top: a host-authoritative state exchange
  between two mods.
- **Levels:** the whole chapter is one map, `/Game/Levels/PP_FinalLevel`. A test can jump into
  it from the menu with the console `open` command, which avoids scripting menu clicks.
- **Spawning:** UE4SS's own `UWorld::SpawnActor` re-implementation crashes on this engine
  (upstream issue UE4SS-RE/RE-UE4SS#527, still open in v3.0.1). The console `summon` command
  spawns reliably and is the game's own code path, so that is how the puppet is created.
- **The player rig** is a 7-bone mesh (`/Game/Player/Rig/Plegs_rig`) with a *procedural*
  animation blueprint (`Plegs_rig_AnimBP`) that casts to the owning pawn. It cannot be put on
  a standalone actor. The same skeleton ships a plain `legs_Walk_anim` sequence, which can.
- **Save system:** standard UE `SaveGame` files under
  `%LOCALAPPDATA%\Poppy_Playtime\Saved\SaveGames` (`SG_Audio.sav`, `SG_Graphics.sav`) — the
  chapter's own progress lives in the level/GameInstance, not in a save the mod needs to touch
  yet.

## What we changed

1. **Puppet is invisible** → it was summoned at scale 1.0 while the player's mesh component
   carries 14.37, so it rendered a few pixels wide. Fix: copy `K2_GetComponentScale()` from
   the player's mesh. Test: `diag` prints player and puppet mesh, slot-0 material, scale and
   reference bone side by side and they match.
2. **Puppet T-posed / no animation** → the player's AnimBP cannot apply (see above). Fix:
   `SetAnimationMode(AnimationSingleNode)` + `SetAnimation(legs_Walk_anim)` + `Play(true)`.
   Test: `animstat` reports `playing=true` and a position that advances inside the 2.0 s loop.
3. **Game dies minutes into a run, or the mod's loop stops silently** → calls into engine
   objects were made from the mod's own thread. Fix: every mutation goes through
   `ExecuteInGameThread`. Test: 4 minutes of continuous puppet placement, 0 Lua errors, no
   crash report (off-thread, the same run died twice with nothing logged).
4. **No transport** → the mod only has file I/O, so it writes its own state and reads the
   peer's; a Python relay bridges that to another machine. Test: `tests/test-relay.ps1`,
   two real clients and two relays, both directions, distinct positions in both logs.

## QA

- `tests/test-relay.ps1` launches two copies of the game with the mod deployed, clears the
  logs, runs two relays over UDP, waits for both to enter Chapter 1, then checks each game log
  for the peer's position and for the absence of crash reports. Last result: **pass**,
  2026-10-18 — both logs show a puppet at the peer's position, 0 crashes, saves and config
  restored byte-identical afterwards.
- Not covered: any playthrough, any gameplay interaction, the Steam message leg.

## How to play

Not ready for players yet. When it is: install UE4SS 3.0.1, deploy `PoppyCoop` into
`Mods\`, add `PoppyCoop : 1` to `Mods\mods.txt`, run `relay/cooprelay.py` on both machines
(Steam transport, one `--role host`, one `--role client --lobby <id>`), and play. Full steps
are in the [PoppyCoop README](https://github.com/romannazarov057-spec/PoppyCoop#install).

## Known issues

- Only movement is synced. Doors, objectives, interaction and enemies are not, so the chapter
  cannot be finished in co-op.
- The puppet plays a fixed walk cycle, not one driven by the peer's real speed, and its facing
  is a rough yaw.
- The Steam transport has only been tested up to "lobby created and entered". Nobody has
  exchanged a message over Steam yet.
- The mod spawns a local puppet even in a single-player session.

## Next steps

- Sync Chapter 1 state: the objective/GameInstance class and the door and interactable
  classes, so a pair can actually progress.
- Drive the puppet's animation from synced velocity, and its facing from movement direction.
- A one-click host/join wrapper so players never touch a command line.
- Test the Steam message leg once a second Steam account is available.

Deeper notes, with the measurements behind every claim and the dead ends (no usable bone
read on this build; screen-diff tests are too noisy in this game), are in
[NOTES.md](NOTES.md).
