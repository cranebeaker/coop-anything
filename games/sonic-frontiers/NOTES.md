# Sonic Frontiers — co-op agent notes

Started 2026-11-24. Owner goal: **Seamless-Co-op-style co-op** — shared enemies,
items and story — with friends on Steam PC, not just visible player puppets.

## Identity (observed — source: local read-only inspection, 2026-11-24)

- Title: Sonic Frontiers (PC/Steam).
- Install: `<game dir>` (Steam library; path withheld from shared notes).
- Main exe: `SonicFrontiers.exe`, 365,848,592 bytes.
- SHA-256: `7E58F45C810A2A8352A451424895B3D543998F3968E7571853F7A080E8E344B6`.
- `steam_appid.txt` in the game root contains `1237320`. This is **not** assumed to
  be the game's Steam app id; reason unknown.
- Store build/version id: **unknown** (no appmanifest found in the inspected Steam
  library).
- Engine: Hedgehog Engine 2 (from the HE2MP project name and prior local notes;
  needs an independent source before being treated as confirmed).
- OS: Windows.

## Triage (observed)

- `DIRECTORY.md` / `games/`: no Sonic Frontiers entry. Sonic Adventure 2, Sonic
  Adventure DX, Sonic Riders and Sonic Robo Blast 2 are listed.
- GitHub issues/PRs in `cranebeaker/coop-anything`, searched via the web UI for
  "sonic frontiers": 0 results for both (checked 2026-11-24). `git`/`gh` were not
  installed at session start; checks were unauthenticated web reads.
- Upstream project: `https://github.com/angryzor/sonic-frontiers-multiplayer`
  (GPL-3.0, C++, pushed 2026-03-21, 1 star, 0 forks). Its README is still a
  `rangers-sdk` starter-project README and does not document features.
- `he2mp` v0.0.35 ("Hedgehog Engine 2 Multiplayer", author angryzor) is installed
  in `<game dir>/mods/he2mp` and is the only mod active in `ModsDB.ini`.
  Its changelog mentions players visible, name tags, effects, sound, text chat,
  experimental PvP and support for several game modes; it does **not** claim
  enemy, item or story sync.
- A prepared copy for a friend exists at
  `%USERPROFILE%/Desktop/SonicFrontiers-MP-DEV/handoff/he2mp-PLAY-NOW/`. Its own
  README says: two-PC operation was never tested, distribution is through the
  author's Discord playtest, and both players need version 0.0.35 and the same
  realm. `he2mp.dll` SHA-256 in that handoff:
  `FF457469855393B97D762684A2F1DF98177AB3F325D5B4621B22B5D263AA5C9F`.

## Prior local project (observed, not independently verified)

`%USERPROFILE%/Desktop/SonicFrontiers-MP-DEV` is a large staged reverse-engineering
project aimed at full co-op. Its newest safety record (`mpdev047-safety-status.txt`,
2026-11-24) states, among other things:

- `real_stable_game_identity=NOT_CONFIRMED`
- `real_enemy_resolver=NOT_APPROVED`
- `real_in_game_enemy_sync=NOT_ATTEMPTED`
- `MP-022=STOP`, `MP-025=OUTCOME_B`
- `GATE_B=CLOSED`, `GATE_C=CLOSED`, `GATE_C_OPENED=0`
- `IN_PRINCIPLE_MODIFICATION_ACCEPTED=1`, `ACTUAL_MODIFICATION_AUTHORIZED=0`
- candidate identity field `GOCEnemy+0x0fc` is a candidate only; no promotions.

This means the existing custom DLLs are **not** a playable full co-op mod and no
in-game write/hook capability has been authorized. Do not claim otherwise.

## Plan / decisions from owner (2026-11-24)

- Route: full co-op with enemy, item and story sync (Elden Ring Seamless Co-op style).
- Notes location approved: this local playbook fork.
- Git + GitHub CLI installation and use of the owner's GitHub account approved;
  claim-a-game issue and later draft PR are in scope.
- Friends: Steam PC, same build.

## Searched (so far, 2026-11-24)

- This repo's `DIRECTORY.md`, `games/`, issues and PRs.
- Web search: "Sonic Frontiers co-op multiplayer mod PC 2026",
  "Sonic Frontiers multiplayer mod github coop online".
- GitHub repository search: "sonic frontiers multiplayer" (1 result: angryzor).
- Local Steam library, installed mods, `ModsDB.ini`, prior work folders.

## Gate status, deep read (observed 2026-11-24)

From `%USERPROFILE%/Desktop/SonicFrontiers-MP-DEV/RESULTS/mpdev047-c-file-hashes.txt`
(the newest authoritative artifact; the root README is stale):

- `MP047_C_STATUS=PACKAGE_COMPLETE_NO_RUNTIME`
- `READONLY_TWO_PC_TEST_AUTHORIZED=1`, `H_EXT2_RUNTIME_TEST_EXECUTED=0`
- `MP036_REQ6_STATUS=UNMET`
- `CANDIDATE_FIELD=GOCEnemy+0x0fc`, `CANDIDATE_KEY_FIELD_ONLY=1`
- `IDENTITY_PROMOTIONS=0`, `RESOLVER_APPROVALS=0`, `GATE_C_CHANGES=0`
- `real_stable_game_identity=NOT_CONFIRMED`, `real_enemy_resolver=NOT_APPROVED`,
  `real_in_game_enemy_sync=NOT_ATTEMPTED`
- `MP-022=STOP (narrow read-only override per scan)`, `MP-025=OUTCOME_B`
- `GATE_B=CLOSED`, `GATE_C=CLOSED`, `GATE_C_OPENED=0`
- `IN_PRINCIPLE_MODIFICATION_ACCEPTED=1`, `ACTUAL_MODIFICATION_AUTHORIZED=0`

`mpdev047-c-safety-status.txt`: `PROCESS_COUNT=0`, `READY_PC_A_RECEIVED=0`,
`READY_PC_B_RECEIVED=0`, `PC_B_TRACE_RECEIVED=0`, no runtime scan by the assistant.

The next planned, already-authorized step is a **strictly read-only two-PC
correlation** of `GOCEnemy+0x0fc`:

1. Friend runs `mpdev047-c-RUN_ON_PC_B.bat` from
   `RESULTS/mpdev047-c-pcB-one-click-package.zip` (sha256
   `02055AE54FA362B92CDB26E9DF5D54BF76DD7FCB479E0F2A3EC2D4FB5AA0F81D`)
   with Sonic Frontiers open in a comparable enemy area, and sends back the
   raw trace plus console text.
2. With fresh `READY_PC_A` and `READY_PC_B` tokens, run the pinned PC A scanner
   (`tools/mpdev047-a-pcB-readonly-scan.ps1` pattern) to
   `RESULTS/mpdev047-a-pcA-raw-trace.txt`.
3. Compare offline using `RESULTS/mpdev047-comparison-schema.txt` and
   `mpdev047-two-pc-correlation-plan.txt`. Even `CROSS_PEER_SUPPORT_WEAK`
   would promote nothing and open no gate.

Why this matters for the owner's goal: read-only enemy discovery found real
`GOCEnemy`/`GOCHealth` fields (position `GOCEnemy+0x090`, HP `GOCHealth+0x080`),
but there is no stable cross-peer enemy identity, no fail-closed resolver, no
authorized write path, no item-ID/inventory work, and no story-flag/trigger work.
MP-044 records `QUALIFIED_WRITE_TARGET_EXISTS=NO` and
`GATE_C_CURRENT_STATUS = CLOSED / UNJUSTIFIED`. Item and story sync have no
dedicated investigation in this corpus; treat them as unmapped, not as failures.
MP-040 says `FEASIBLE_WITH_CURRENT_EVIDENCE=NO` and
`MORE_READ_ONLY_SCANS_CAN_REACH_ONLINE_SYNC=NO`.

## Existing projects, web research (observed 2026-11-24)

- **HE2MP (angryzor)**, newest public build **v0.0.35, 2026-04-01**, for both
  Sonic Frontiers and Shadow/SXSG. Distribution:
  `https://github.com/angryzor/hmm-update-server/tree/main/sonic-frontiers/he2mp`.
  Ships GPL-3.0 text but no public client/server source was found. Uses
  GameNetworkingSockets + protobuf, central server run by the author with
  "realms"; exact protocol is unknown.
  - Confirmed: remote player transforms/rotations/animations, effects (disabled
    in v0.0.30 to reduce crashes), sounds, nametags, join/leave messages, text
    chat (T), experimental PvP (F10), several game modes (Battle Rush, Boss
    Rush, CyberStage Challenge, Fishing, Hacking, Master Trial).
  - Not found: enemy combat sync, shared item pickups, story/progression sync.
    An indexed video transcript says enemies are different for each player and
    fighting them is not visible to others (medium confidence). Boss sync is
    listed as a future goal in the Shadow WIP, not confirmed complete.
  - Bugs from its changelog: T-posing remote Sonics, tutorial-room crash,
    Boss Rush/Master King crash, nametags lost after death/retry, effects not
    removed on destruction, Steam Deck crash with older MSVC redist, VFX
    overloading the server. Feedback lives in the author's playtest Discord,
    not a public issue tracker.
- **`https://github.com/angryzor/sonic-frontiers-multiplayer`** is NOT the online
  mod: it is a GPL-3.0 `rangers-sdk` example with local second player/split
  viewports, `maxPlayers = 2`, no sockets, no transport, no server. HEAD
  `a0bea36` "Fixes for 1.42" (2026-03-21).
- **GameBanana WIP 86943** (2024-08-06, modified 2025-03-21, no files) is the
  planning page: local multiplayer first, networked/co-op/Titan fights later.
  **WIP 92852** is the Shadow equivalent and lists boss sync as future.
- **`HE2-SDK` GitHub org** (`rangers-sdk`, `rangers-api`, `he2-devtools`,
  `miller-sdk`): engine types/addresses for Frontiers and Shadow, pushed
  2026-11-08. No networking/co-op logic found. Several repos have no detected
  license; do not assume reuse rights. `he2-devtools` is GPL-3.0.
- Negative searches: GitHub repo search `he2mp` (0 results), Nexus (~35 mods,
  no multiplayer), archive.org, Japanese and Russian queries, Reddit (403 to
  fetcher), GameBanana; all circled back to HE2MP. `sonicfrontiersguide.com`
  is a low-quality SEO page repeating the GitHub template.
- No separate JP/RU co-op project found. No public HE2MP source/server found;
  server software said to be released only at public release.

## Code inventory (observed 2026-11-24)

- MPD2 protocol `2.0/build3` has player, entity (spawn/state/health/death),
  boss, world state/events and lobby messages, plus reliable acks. Two-process
  loopback tests pass offline; no game is involved.
- Game adapter hard-codes the pinned EXE hash and read-only offsets: player
  `+0x080`, enemy `+0x090`, current HP `+0x080`; max HP `+0x084` is candidate
  only. Reads go through `IMemoryReader`; shipped `mpdev-coop.dll` has live
  reads, transport, hooks, writes, remote spawn and position sync compiled OFF.
- `mpdev-live-read.dll` (reads ON, everything else OFF) and
  `mpdev-pos-sync.dll` (controlled position-write experiment, hooks OFF) are
  isolated engineering builds, not installed and not real co-op.
- No `Item`/`Inventory`/`Pickup` messages exist. Story support is only generic
  `WorldState2.progressFlags` / `WorldEvent2.eventId`. Item and story sync need
  new typed messages, schemas, adapters, authority rules and tests.
- Future work surfaces: `src/coop/mpdev_protocol2.h/.cpp`,
  `mpdev_entity_replication.*`, `mpdev_world_replication.*`,
  `mpdev_game_adapter.*`, `mpdev_enemy_integration.*`, plus new
  `mpdev_item_replication.*` / `mpdev_story_replication.*`; tests in
  `tools/run-tests.ps1` and `tools/run-coop-test.ps1`.

## Unknowns / not yet checked

- Exact Steam build id and whether all players have the same one.
- Whether Hedgehog Engine 2 is confirmed from the exe/install.
- Full feature list, current build and bug reports for HE2MP.
- Other projects in Japanese/Russian communities, GameBanana, Nexus, Discord,
  archive.org (research in progress).
- Which parts of prior MP-DEV evidence are sound and which are obsolete.
