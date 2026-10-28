# Crime Scene Cleaner (2024)

| | |
|---|---|
| Engine | Unity 2021.3.45f2, IL2CPP, URP |
| Versions tested | Steam build 26046462 |
| Projects | [Crime Scene Cleaner Co-op](https://github.com/gogoigor51-tech/crime-scene-cleaner-coop) (directory entry "Crime Scene Cleaner Co-op") |
| QA level | automated (plus two players through chapters 1-6) |
| Last updated | 2026-10-27 |

## Where things stand

Two players can play the story together over Steam: the client follows the host into each job and nearly all of the
cleaning, evidence, bodies, furniture and level machinery is shared, with each player keeping their own save,
report, money and skills. Chapters 1-6 were played through by two players on 0.4.1; 0.4.2 fixes were checked on one
job and by the bot suites. 3-4 players, Nightmare mode and the last chapters are untested.

## Existing work

- [Crime Scene Cleaner Co-op](https://github.com/gogoigor51-tech/crime-scene-cleaner-coop), 0.4.2, 2026-10:
  this write-up. Built from scratch.
- [coop-anything#176](https://github.com/cranebeaker/coop-anything/pull/176) by KolesovD (open PR, 2026-10): a
  prototype write-up and directory entry for the same game. It touches this same file; the two write-ups should
  be merged into one when either lands.

## How the game works (for co-op)

- Single-player only, no netcode. Steamworks is present (`steam_api64`), so the mod drives Steam's flat API
  (ISteamFriends v018, ISteamNetworkingMessages v002) itself: rich presence as a directory of hosts, no lobbies.
- BepInEx 6 **be.791** (pre.2 fails on metadata v31). Cpp2IL ISIL dumps decided most questions: many calls the
  game makes go through UnityEvents, so "who calls this" often has no direct caller.
- Objects: `GrabableItemController` (items), `BodyRagdollController` (bodies), `Taint`/`CleanableDecal` (stains),
  `GarbageItem` (van), plus dozens of level machinery types. Objectives are PixelCrushers Dialogue System quests;
  counters (`IncrementEvent`, `BodyQuest`) keep their numbers in Lua variables and add to what they read.
- Saves: `SaveSystem.StartGame(slot)` is a *new game* (it clears the slot); loading is `SaveSystem.LoadSlot`.
- Jobs are scenes; the hub is the apartment. A census bot that loads every job lists 510 game component types in
  17 scenes (`tools/run-census.ps1`), used to find what still needed sharing.

## What we changed

The mod repo's `NOTES.md` is the full log. The decisions that mattered most:

- **Identity:** every shared object is found by a hierarchy path (and start position for items), sorted into
  tables. The scene key is the items + bodies table only; every other table has its own fingerprint, exchanged
  between players. A table that differs (two saves that differ a little, e.g. the Villa job) is switched off alone
  instead of the whole job. Before 0.4.2 one differing table cut all sync while the corner label still said
  "2 players".
- **Ownership:** items and bodies are owned by whoever holds them (epochs, the host breaks ties) and go back to
  the host at rest. Bodies needed "rest" defined as asleep *and* not moving, and bones kept awake for a few seconds
  after a throw: bones put to sleep while kinematic reported sleeping in mid-air, and a body handed over then froze
  in the air on both screens.
- **World events:** state events through the host (newest per key, replayed to late joiners) for doors, drawers,
  lifts, stains, evidence, trash and so on; a whitelist of `Type.Method` calls with simple arguments replayed on
  the same object for level machinery (58 entries, from the census). IL2CPP folds identical method bodies, so a
  patch on one type's method also fires for others: each replay casts to the expected type first.
- **Objectives:** only flags, text, objectives added during a job, story objects and restarts cross between
  players; numbers never do. Merging counts made one body count 2/2 (BodyQuest adds 1 to the Lua value it reads)
  and a merged blood percent made the client's game wipe the rest of its blood itself.
- **Double counts:** after a replayed van collect the game calls `GarbageItem.CollectAndDestroy` again; the same
  van item is now collected once.
- **The other player:** a procedurally built skinned figure with the equipped tool copied from the game's prefab
  (falls back to a tool of the same kind when the other machine lacks an upgraded model) and a carried body on
  the shoulder.

## QA

- `tools/run-pair.ps1 -Test <name>` starts two game copies on one PC (small, muted, on the second monitor), bots
  connect over localhost UDP, step in lockstep and dump state; `tools/<name>_report.py` compares both sides.
  Suites: items, stains, bodies, doors, trash, world2 (machinery, furniture, story objects, objective flags, F9
  reports), jobs, laptop, cctv, figure. A mutant run (`-Mutant table-differ`) proves a differing table no longer
  cuts the job. `GATES.md` in the mod repo binds each to a command.
- Last result (2026-10-27, 0.4.2): world2 20/20, bodies 13/13, stains 14/14, jobs 15/15, laptop 9/9, cctv 3/3,
  world2 with the mutant 20/20; core tests pass.
- Two players over Steam: chapters 1-6 on 0.4.1; the Villa job on 0.4.2.

## How to play

Download the release zip from the mod repo, extract it into the game folder on both PCs (same mod version), back
up `CrimeCleaner_Data\Saves`. Main menu → КООПЕРАТИВ (Co-op): host picks Host and a slot, the friend picks Join, a
slot and the host. F8 shows status and ping, F9 saves a bug report for both players.

## Known issues

- Deleting the CCTV recordings has to be done by each player; the hack itself is shared.
- Objective maximums can differ between players (the game raises the furniture maximum when a piece that already
  stood in place is moved and put back).
- An upgraded tool may show as the basic one on the other player's screen.
- The Spa walkie-talkie and the job restart are shared in code but not checked by players.
- After the sped-up intro cutscene the pizza box can hang next to the player in the apartment.

## Next steps

- CCTV recordings: the recordings exist only once the laptop's folder opens in the UI; share the deletion at the
  presenter level (`HackableComputerPresenter.HandleFileDelete`) and test it with a UI-driving bot.
- 3-4 players, Nightmare mode, the last chapters.
- Re-dirtying (power washer splashes on cleaned stains) is not shared; stains only sync towards clean.
