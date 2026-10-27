# Fallout: New Vegas (2010)

| | |
|---|---|
| Engine | Gamebryo (xNVSE plugins for native hooks) |
| Versions tested | Steam/GOG release 1.4.0.525; Tale of Two Wastelands (see below) |
| Projects | NV:MP (multiplayer: players, NPC motion, combat; closed netcode); [fnv-coop](https://github.com/cranebeaker/fnv-coop) (story sync layer) |
| Release | [fnv-coop](https://github.com/cranebeaker/fnv-coop) (developer preview, no player release yet) |
| QA level | automated |
| Last updated | 2026-10-26 |

## Where things stand

NV:MP lets people play in the same world, but quests are client-side and quest actors
are deliberately not synced. fnv-coop is a host-authoritative quest and story sync layer
(an xNVSE plugin plus a game-free sync core) meant to run next to NV:MP. It is a
developer preview with no player release or installer yet.

## Existing work

- NV:MP: players, NPC motion and combat. Closed netcode, no quest sync. See its entry in
  the directory.
- fnv-coop: the story layer described here.

## How the game works (for co-op)

- Quest logic runs as stage result scripts in the ESM. Sync captures the effects of a
  stage on the host as one transaction and applies it on clients without re-running the
  scripts.
- NV:MP's story server has no quest sync; its client marks quest actors as local.

## What we changed

- Sync core: engine-agnostic protocol and merge rules (stage = max, flags = OR,
  kills = union), covered by a simulator and soak tests.
- Effect capture: hooks record every world change a quest script makes on the host and a
  guest applies the transaction without re-running the script.
- Host-to-client quest-stage sync over our own network channel in the real game.
- Per-quest co-op rules for every story quest.
- Two game copies on one PC for tests, with the single-instance checks neutralized inside
  our own process.

## QA

- Game-free suites: the sync core with its simulator (2 to 4 clients on a lossy,
  reordering link), seeded chaos fuzzing and a 4-player soak.
- A JSON-RPC bridge plugin inside the game (`exec`, `waitFor`, `state`) lets tests drive
  real game copies; world snapshots split into shared and per-player state are diffed.
- Host capture and guest apply of quest-script effects in the real game, with matching
  snapshots.
- Host-to-client quest-stage sync over our own network channel in the real game.
- Run them with `python -I tools/test_all.py` in the fnv-coop repo; suites that need your
  own game install skip with a message.

## Tale of Two Wastelands (TTW)

Tested on Steam 1.4.0.525 (4GB-patched), xNVSE 6.4.9, TTW with YUP for TTW, MO2, the Best of Times guide.
Details and evidence: [NOTES.md](NOTES.md).

- **NV:MP is not an option for TTW:** TTW's own startup script shows an "NV:MP incompatible" message when NV:MP's
  `client` DLL is loaded.
- **fnv-coop works on TTW with small changes** (in the fnv-coop branch for TTW): its quest-data tools parse all
  18 TTW masters (917 quests); EditorIDs do not collide between the two games, so one `rules.json` holds both;
  `FNVCOOP_LOADORDER` / `FNVCOOP_QUESTDATA` switch the tools to a TTW load order and data set.
- **In game, two copies, all 17 TTW plugins:** host-to-guest stage sync on a Fallout 3 quest passes (0.1 s) and
  objective sync passes (0.3 s) once nvcoop's objective hook tolerates the call that ShowOff and Stewie's Tweaks
  (both required by TTW) re-point inside it. Without that fix the hook refuses to install and objectives do not sync.
  13 of 14 two-copy rounds ran without a crash.
- **Test copies need the guide's `FalloutCustom.ini` settings** merged into the instance INI: TTW checks
  `bBackgroundLoadLipFiles` and otherwise shows a warning; a save made after dismissing that warning crashed often.
- **Also fixed for TTW:** nvcoop's guest-intents and bots engine checks (another plugin jumps over or rewrites those
  functions in place; nvcoop now accepts exactly those forms).
- **TTW's train between the wastelands:** in a session the guest follows the host to the other wasteland (the host's
  player move replicates); a curated `ttw` rules lane exists for it. Videos (`PlayBink`) stall in a minimized window.
- **Fixed upstream gap:** rules never applied to a quest's own (GameMode) script, because the cause named the script and
  rules are keyed by quest. With the fix the train ride locks the guest's controls together with the host's; vanilla
  round, objective sync and rules still pass.
- **Quest rules:** a `ttw` rules lane with curated entries for the Fallout 3 prologue (CG01 / CG03 per player, CG04
  shared), Tranquility Lane, The American Dream and the train; the other main-quest entries were reviewed and kept.
- **Inventory stash (fixed):** when a quest strips the player into a container (The Pitt, Anchorage, Zeta, Tranquility
  Lane, the Enclave capture, TTW's train), only the host used to be stripped. A new `ContainerTransfer` effect now moves
  each player's own items through their own copy of the container and back; proven in game on The Pitt.
- **Videos for everyone:** `PlayBink` (the Fallout 3 ending, TTW's train) replicates as a story-capture event (needs
  `[hooks] story_capture=1`). Fallout 3 ending default: the player who enters the code dies, everyone sees the ending.
- **Open:** most other TTW story quests have only
  derived (unreviewed) rules or none; 68 curated New Vegas rules sit on quests TTW or YUP change; the Fallout 3 ending
  (purifier death + ending videos chosen by karma) needs a co-op design decision; one rare host crash (13 of 14 rounds
  clean) after a guest joins.

## How to play

There is no player release yet. Follow the fnv-coop repo for the installer.

## Known issues

- Developer preview only: no installer, no player documentation.
- Windows and New Vegas 1.4.0.525 only.

## Next steps

Package an installer and a player release, and pair it with NV:MP for avatars and combat.
