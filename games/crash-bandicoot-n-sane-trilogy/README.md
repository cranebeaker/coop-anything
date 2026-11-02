# Crash Bandicoot N. Sane Trilogy (2017)

| | |
|---|---|
| Engine | Alchemy (Vicarious Visions), Havok behavior graphs, NovaScript / visual scripts |
| Versions tested | Steam build 2964270, exe SHA-256 `170c61d37fb5277dd94b3e41f7a918f227b76bc67aa338f5c97f63ac317b6af3` |
| Projects | [Crash N. Sane Coop](https://github.com/MaegIins/CrashNSaneCoop) (directory entry "Crash N. Sane Coop") |
| QA level | none (author's manual tests only, see QA) |
| Last updated | 2026-11-02 |

## Where things stand

Two players can play Crash 1 levels "together" today: each runs their own game, sees the other as a second Crash
(or Coco) that runs, jumps and spins, and crates and wumpa fruit stay in step between the two games. Not handled yet:
deaths and checkpoints, Crash 2's animation events, Coco's own animations, and characters slide a little.

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
- crate sync by position, replayed with the game's damage function so fruit and counters behave normally;
- shared wumpa counter (each side sends what it picked up; anti-echo window after replayed crates);
- optional Coco model for the friend;
- a WinForms launcher (French/English) that starts the game and enables the mod.

## QA

No automated suite yet. Manual checks that ran (2026-11-01..02):
- Real two-player sessions over Steam P2P (owner and a friend): presence, animations, spins breaking crates.
- Lab: two copies of the game on one PC over UDP. Crate break on A is replayed on B (log plus same-spot
  screenshots). Fruit is added both ways (6 -> 16, 10 -> 18). The Coco puppet runs and jumps, with no crash in
  about 2 minutes.
Not tested yet: crates, fruit and Coco over Steam; Crash 2 and Crash 3 for those features; deaths and checkpoints.

## How to play

Download the latest release from <https://github.com/MaegIins/CrashNSaneCoop/releases>. Unzip it into a normal folder
and run `Lancer Crash Coop.cmd`. Paste the friend's SteamID64 and start the game. Both players need the same version.

## Known issues

- Characters slide a little while running.
- Coco has no animations of her own yet (Crash's event ids made her puppet shatter, so events are off for her).
- Dying or going back to a checkpoint is not synced.
- Crash 2: the friend appears, but some animation events do not match.
- Only the Steam build above works (fixed addresses).

## Next steps

- Translate behavior events by name between Crash's and Coco's graphs.
- Sync deaths and checkpoint reloads (and the crates they restore).
- Reduce sliding (velocity or root motion on the puppet).
- Build an in-game bot and a two-client test to reach the `automated` QA level.
