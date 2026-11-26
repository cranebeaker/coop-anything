# Sonic Frontiers (2022)

| | |
|---|---|
| Engine | Hedgehog Engine 2 (from project names/SDK repos; not yet confirmed from the exe) |
| Versions tested | Steam PC, `<game dir>/SonicFrontiers.exe`, SHA-256 `7E58F45C810A2A8352A451424895B3D543998F3968E7571853F7A080E8E344B6`; Steam build id unknown |
| Projects | [HE2MP](https://github.com/angryzor/hmm-update-server/tree/main/sonic-frontiers/he2mp) (binary only); local, unpublished MP-DEV research project |
| QA level | none (no two-PC or in-game co-op test was run for this write-up) |
| Last updated | 2026-11-24 |

## Where things stand

There is no full story co-op mod for Sonic Frontiers. The only widely distributed
multiplayer mod, HE2MP, shows other players and supports chat/PvP experiments but
has no verified enemy, item or story sync. A separate local research project has a
well-tested offline/synthetic co-op foundation and read-only game probes, but no
stable cross-peer enemy identity, no authorized write path, and no item or story
implementation. Do not describe any of this as a working Seamless Co-op style mod.

## Existing work

- **HE2MP v0.0.35 (2026-04-01)**, angryzor. Binary distribution via
  [hmm-update-server](https://github.com/angryzor/hmm-update-server/tree/main/sonic-frontiers/he2mp).
  Ships GPL-3.0 text; no public client/server source found. Syncs remote player
  transforms, rotations, animations, effects, sounds, nametags, join/leave, text
  chat and experimental PvP. Effects were disabled in v0.0.30 to reduce crashes.
  No confirmed enemy/item/story sync. Feedback is in the author's playtest
  Discord, not a public issue tracker.
- [angryzor/sonic-frontiers-multiplayer](https://github.com/angryzor/sonic-frontiers-multiplayer):
  GPL-3.0 `rangers-sdk` example for local split viewports (`maxPlayers = 2`), no
  networking. Last push 2026-03-21.
- [GameBanana WIP 86943](https://gamebanana.com/wips/86943): angryzor's planning
  page for local multiplayer, later networked co-op and Titan fights; no files.
- [HE2-SDK](https://github.com/HE2-SDK): engine headers/addresses for Frontiers
  (`rangers-sdk`, `rangers-api`) and Shadow (`miller-sdk`); no networking code,
  varied license status.
- Searched without finding another project: GitHub repos/code/issues/PRs,
  GameBanana, Nexus Mods, archive.org, Reddit, Steam guides, YouTube and JP/RU
  web queries. See `NOTES.md` for the search list.

## How the game works (for co-op)

Sonic Frontiers is a closed-source Hedgehog Engine 2 game. Mods load through
HE2ModLoader / HedgeModManager; the installed HE2MP package uses
GameNetworkingSockets and protobuf with a central server run by the mod author.
No shipped game netcode was found. The local MP-DEV project reads specific game
objects from a running process with a pinned exe hash; confirmed read-only fields
include player position (`+0x080`), enemy position (`GOCEnemy+0x090`) and current
HP (`GOCHealth+0x080`). Its only candidate enemy key field, `GOCEnemy+0x0fc`, is
not a confirmed identity. Item IDs, inventory, story flags, triggers and the save
system have not been mapped.

## What we changed

Research only: this write-up, `NOTES.md` and directory entries. No game files,
saves, mod installs or live game state were changed by this session.

## QA

No test was run for this write-up. The local MP-DEV workspace contains offline
scripts (`tools/run-tests.ps1`, `tools/run-coop-test.ps1`,
`tools/run-enemy-loopback-test.ps1`) whose saved results cover synthetic
two-process loopback, not real game clients. Its own latest record says
`real_in_game_enemy_sync=NOT_ATTEMPTED` and `PC_B_TRACE_RECEIVED=0`.

## How to play

- **Visible players only:** HE2MP v0.0.35 requires the same version on both PCs,
  a mod loader, and the author's playtest Discord/realm. Prepare a friend-ready
  copy from the existing handoff if the owner approves; two-PC operation is not
  verified.
- **Full co-op:** not available. There is no installable build to distribute.

## Known issues

HE2MP's changelog records T-posing remote players, crashes in the tutorial room
and Boss Rush/Master King, lost nametags after death/retry, stale effects, an
older-MSVC crash on Steam Deck, and server trouble with heavy VFX.

For the custom project, the blockers are a missing stable cross-peer enemy
identity, no qualified write path, closed write gates, and unmapped item/story
state. Even a positive two-PC read-only correlation would not authorize any of
those changes.

## Next steps

1. Run the already-prepared, strictly read-only two-PC correlation of
   `GOCEnemy+0x0fc`: friend runs the packaged PC B scan, then the owner runs the
   pinned PC A scan and the traces are compared offline. This needs fresh
   `READY_PC_A`/`READY_PC_B` tokens and explicit per-run approval.
2. In parallel, design and test **typed item and story replication** in the
   offline MPD2 core (new message kinds, authority/epoch rules, effect capture,
   late join/reconnect, fuzz tests). This touches no game files.
3. Only after evidence and itemized approval: qualify a minimal reversible write
   path for enemy state, then attempt the first real two-PC enemy sync.

See `NOTES.md` for exact file paths, quotes and sources.
