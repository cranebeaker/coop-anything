# Crash Bandicoot N. Sane Trilogy (2017)

| | |
|---|---|
| Engine | Alchemy (Vicarious Visions), Havok behavior graphs, NovaScript / visual scripts |
| Versions tested | Steam build 2964270, exe SHA-256 `170c61d37fb5277dd94b3e41f7a918f227b76bc67aa338f5c97f63ac317b6af3` |
| Projects | [Crash N. Sane Coop](https://github.com/MaegIins/CrashNSaneCoop) (directory entry "Crash N. Sane Coop") |
| QA level | automated (two-client suite, see QA) |
| Last updated | 2026-11-02 |

## Where things stand

Two players can play Crash 1, 2 and 3 levels "together" today: each runs their own game, sees the other as a second Crash
(or Coco, when both play Coco) that runs, jumps and spins, and crates and wumpa fruit stay in step between the two
games, including checkpoints and multi-hit crates. A friend who falls into a pit makes the puppet hide until they
respawn. Not handled yet: a friend on another hero than yours (they appear as your hero), and characters slide a little.

## Existing work

- Before this project: no online co-op mod found (GitHub, Nexus, web search, 2026-10). The game only has local
  hot-seat modes.
- [Crash N. Sane Coop](https://github.com/MaegIins/CrashNSaneCoop): this project, built with this playbook.

## How the game works (for co-op)

Full notes with addresses: [NOTES.md](NOTES.md). In short:
- The exe is SteamStub-packed; Ghidra runs on a memory dump. No netcode for the campaign.
- Player actor: `CPlayerEntityManager` and an actor list; the "Crash" CActor prototype (no components) can be cloned
  with the engine's SpawnEntity to make a puppet.
- Animation is a Havok behavior graph driven by events that the game fires only for the controlled character
  (FireEventById `exe+0x33FEA0`); they are captured on the sender and re-fired on the puppet from the game thread.
- Crates are `CDestructibleComponent`s. A broken crate's component is freed. The engine's damage entry
  `exe+0x4D63A0(CDamageData, attacker, target)` with a "bounce" def breaks a crate exactly like a jump.
- The wumpa counter is a `CGameVariableValue` (int at +0x10). The HUD script keeps a display copy.
- Hero packs load into a per-chunk memory pool. Only the local hero's pool has memory, so Coco is loaded into that
  pool next to Crash to get her prototype.

## What we changed

The mod is a DLL injected into the running game (no game files changed):
- puppet of the friend (position, rotation, buttons every frame, behavior events replayed);
- crate sync by position, replayed with the game's damage function so fruit and counters behave normally; every hit is
  sent (damage stage at component +0x30), so checkpoint crates and bounce crates follow too;
- shared wumpa counter (each side sends what it picked up; anti-echo window after replayed crates);
- the puppet uses the hero the friend plays when it is also ours (Coco/Coco gets all animations); loading another
  hero's pack crashed the game on the next level change and is disabled;
- falls: the puppet hides while the friend falls into a pit, instead of hitting the kill plane;
- a WinForms launcher (French/English) that starts the game and enables the mod.

## QA

Automated two-client suite: [tests/suite.ps1](https://github.com/MaegIins/CrashNSaneCoop/tree/main/tests) in the mod
repository. It starts two copies of the game on one PC (separate APPDATA folders), enters the chosen level in both
with the game's own `loadmap` console command, connects the two mods over UDP, drives player A and asserts on B's game and both mod logs. A scorer self-test
runs first; a check that never ran fails.

Last full run (2026-11-02, mod v18 code):

```
scorer self-test ok
PASS  setup: both copies in N. Sanity Beach
PASS  1 presence: puppets spawned, friend visible on both
PASS  1 presence: crate lists built on both (precondition)
PASS  3 fruit: B found its own counter (precondition)
PASS  2 crate: A detected its own break and sent it
PASS  2 crate: B found its copy of the crate and broke it
PASS  2 crate: B did not report it missing
PASS  2 crate: B did not send the crate back as its own
PASS  3 fruit: B added the fruit A picked up  wumpa: +1 from the friend (12 -> 13)
PASS  4 fall: B parked the puppet while A fell (precondition friend visible: True)
PASS  4 fall: puppet back once A respawned
PASS  4 fall: puppet never destroyed by the kill plane
PASS  5 level: A went to the map (precondition)
PASS  5 level: A back in the level, puppet spawned again
PASS  invariants: both games alive, no FAULT, no error dialog
15/15 checks passed
RESULT: PASS
```

The suite found one real bug (a crate broken for the friend was sent back as ours, fixed in v18). Earlier runs
failed for reasons in the test itself (a crate with no fruit, a menu click during a respawn), which were fixed. The
copy of the suite published in the mod repository (relative paths) was then run against the v18 release build:
15/15, RESULT: PASS.
Also played over Steam by two people: presence, animations, spins breaking crates.
The same suite, entering levels with `loadmap`, then passed in all three games with v18: Crash 1 N. Sanity Beach 15/15,
Crash 2 Turtle Woods 15/15, Crash 3 Toad Village 15/15.
With v19 a check 6 was added (A opens a checkpoint crate; B's opens too; B dies and respawns at the checkpoint, which
without the sync it did not: B came back at the level start). Results with v19: Crash 1 19/19, Crash 2 15/15, Crash 3 15/15. The suite then caught an intermittent freeze (crate hits run from a
hook that also runs on worker threads); v20 moved them to the main thread: Crash 1 19/19 twice, Crash 2 and 3 15/15.
Not covered: checkpoints in Crash 2 and 3, a friend on another hero, play over Steam for crates and fruit.

## How to play

Download the latest release from <https://github.com/MaegIins/CrashNSaneCoop/releases>. Unzip it into a normal folder
and run `Lancer Crash Coop.cmd`. Paste the friend's SteamID64 and start the game. Both players need the same version.

## Known issues

- Characters slide a little while running.
- A friend who plays another hero than you appears as your hero, moved by their buttons only.
- Dying or going back to a checkpoint is not synced.
- Only the Steam build above works (fixed addresses).

## Next steps

- Sync deaths and checkpoint reloads (and the crates they restore).
- Reduce sliding (velocity or root motion on the puppet).
- Extend the suite: checkpoints in Crash 2 and 3, more levels per game, a two-PC run over Steam.
- Load a second hero without breaking the next level load (see the notes: removing the loader cache entry was not enough).
