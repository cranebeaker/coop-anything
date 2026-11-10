# Ultimate Trial (2024)

| | |
|---|---|
| Engine | Nintendo 64, Ocarina of Time engine (closed-source total-conversion ROM hack) |
| Versions tested | Ultimate Trial v1.2.3 (BPS patch SHA-256 `CD15868E2700F821A8F3342651D40B995F9A9C48B6DAD5F55025032CC86FB526`) with [ut-coop](https://github.com/TheeDeb/ut-coop/releases/latest) (v0.1.1) and the [gopher64 co-op fork](https://github.com/TheeDeb/gopher64/releases/tag/v1.2.0-utcoop1) |
| Projects | [ut-coop](https://github.com/TheeDeb/ut-coop) (client, GPL-3.0) and the [gopher64 fork](https://github.com/TheeDeb/gopher64) (GPL-3.0). Claimed in [issue 342](https://github.com/cranebeaker/coop-anything/issues/342); raw notes in [NOTES.md](NOTES.md) |
| QA level | automated (prototype, same-area co-op) |
| Last updated | 2026-11-09 |

## Where things stand

A prototype exists and is playable as same-area co-op: two emulator instances exchange
player snapshots over UDP, and each side renders the other player as a second Link that
moves and animates. Puppets are hidden when the two players are in different scenes, and
a joining side replays the host's entrance index to load the same scene. Scene following
was verified between the starting scenes 41 and 42; broader trial-to-trial travel is
lightly tested.

Not done yet: story, enemy, world-state and item sync; a verified trial-to-trial
playthrough. No ROMs or game data are distributed; each player applies the official BPS
patch to their own Ocarina of Time (USA 1.0) dump.

## Existing work

- **ut-coop** (this project): client plus a gopher64 fork. Two instances on one PC or
  over the internet (UDP, port forwarding or VPN). Snapshots carry position, rotation,
  appearance and the 24-limb joint table; a watchdog re-injects the puppet after actor
  lists are rebuilt. Releases: client `v0.1.0`, fork `v1.2.0-utcoop1`.
- **No prior multiplayer or co-op attempt existed for Ultimate Trial** (searched
  2026-11-09: the directory, GitHub issue search, a web search, and the hack's issue
  tracker).
- The OoT co-op ecosystem cannot be reused as-is:
  - [Zelda64 Online / ModLoader64](https://github.com/hylian-modding/Z64Online) (memory-sync, GPL-3.0)
    targets the vanilla OoT ROM (it ships per-hack BPS compatibility patches for the randomizer only)
    and hooks vanilla addresses.
  - [Anchor / Ship of Harkinian](https://github.com/garrettjoecox/anchor) and other decomp ports load
    the vanilla ROM, not a hacked one.
  - Zelda 64 recompilation projects likewise expect the vanilla ROM.
- Why they cannot be adapted cheaply: a BPS block analysis of the official patch shows **90.5% of the
  32 MiB ROM is brand-new bytes**, 9.3% is internal repetition, and only 0.2% (74,602 bytes) is
  byte-identical to vanilla (the IPL3 boot region and one 64-byte fragment). Addresses and data
  layouts differ throughout.
- The hack's source is not public (the team refuses to release it, per
  [issue #19](https://github.com/RichieUltimate/ultimate-trial/issues/19)), so all game knowledge has
  to come from the patched ROM.

## How the game works (for co-op)

- Ocarina of Time engine on N64 (MIPS R4300), single player, no netcode, no scripting layer to hook
  at runtime. Saves are the standard OoT save format.
- The hack targets emulators and real hardware: the author lists Project64 (interpreter core
  required), Simple64, N64 hardware with Expansion Pak, Dolphin (Wii VC patch), RetroArch and Ares as
  working, and BizHawk as crashing. That last point rules out the BizHawk Lua bridge.
- Engine reference for structure shapes: [zeldaret/oot](https://github.com/zeldaret/oot) (GlobalContext,
  actor overlay table, SaveContext, scene flags). UT's credits overlap the Hylian Modding scene, so
  layouts partly match the decomp; the divergences found so far are documented in NOTES.md.

## What we changed

Details, offsets and sources are in [NOTES.md](NOTES.md).

- Verified the game's identity and the base ROM (OoT USA 1.0, md5 `5bd1fe107bf8106b2ab6650abecd54d6`)
  and wrote a BPS applier that verifies source and target CRC32 both ways. The patched test ROM has
  CRC32 `627C0746` and boots.
- Parsed the official patch block by block and recorded what survives from vanilla (see above).
- Picked gopher64 (Rust, GPL-3.0, Simple64 lineage) as the host and added a local TCP bridge
  (`--coop-port`): batched READ/WRITE/INPUT/STAT against live RDRAM at VI frame boundaries, with
  per-connection response tags and input overrides released on disconnect. `GOPHER64_DATA_DIR` lets
  two instances on one PC keep separate data.
- Booted Ultimate Trial v1.2.3 on the fork and found the runtime structures: actor overlay table at
  `0x800fa7f0` (513 entries, entry 0 is `Player`), live player actor at `0x80289550` (world position
  `+0x24`), `PlayState` at `0x80276f80`, `sceneId` at `0x80277024`, `sceneSegment` at `0x80277030`.
- Built the puppet: copy the live `Player` struct into reserved RDRAM, rebase its skeleton tables,
  point update at a resident no-op and draw at `Player_Draw`, and append it to the player actor list
  tail. The real player stays the head, so `GET_PLAYER()` is unaffected.
- Streamed appearance and the 24-limb joint table in the snapshot, so the puppet moves and animates.
  A watchdog re-injects it after actor lists are rebuilt (scene loads), and scene gating hides it
  when the two sides report different scene ids.
- Found and verified forced scene transitions: `transitionTrigger` `0x80288DC1` (write 20) and
  `nextEntranceIndex` `0x80288DC6`. Writing an entrance + trigger fades the game and loads the scene.
  The peer follows the host by replaying the host's actual entrance index, so no entrance table is
  needed.
- Built and published the standalone client (`ut-coop`): launches the fork, boots through the menus,
  runs the sync node with host/join flags, and tags snapshots with a session id.

## QA

- Emulator protocol test: `cargo test --release --lib` in the fork -> 1 passed, 0 failed. It drives
  the real per-frame hook against a fake 8 MiB RDRAM: WRITE/READ round trip, input override, STAT.
- Boot smoke test: `local/smoke_ut.py` -> frame counter advanced, full 8 MiB RDRAM read succeeded.
- Boot-to-gameplay test: `local/boot_to_gameplay.py` -> scripted input reached gameplay and moved the
  player 144.7 units.
- Two-instance sync + puppet test: `local/sync_test.py` -> PASS. Each side's puppet read back exactly
  the snapshot it applied (0.0 position error), joint data matched and changed over time, both
  `isDrawn = 1`; 83/79 snapshots, 0 gaps, 0 errors. Forced unlink -> 0 -> 1 re-injection per side.
  Forced 42 vs 999 scene mismatch gated both puppets and clearing it restored them.
- Client interop: packaged `ut-coop.exe` on B plus the Python node on A, session `0x1234` -> PASS.
  281 snapshots, 0 gaps, exact puppets on both sides.
- Chaos: the repo's UDP relay (`bad_wifi`) on the A->B path dropped 16 of 286 packets (3 loss, 13
  burst) and duplicated 5; after teleporting A, B's puppet still matched exactly (0.0 error) and was
  drawn. PASS.
- Scene transition: a real 42 -> 41 load was observed; a forced entrance `0x0712` + trigger faded and
  respawned the player in ~2.9 s; follow-host replay of the entrance passed.
- True scene-to-scene follow: both booted in 41, B forced to 42 via entrance `0x0716`, then the
  joiner followed the host's `0x0712` back to 41 -> follow count 1, scenes 41/41, fresh puppet
  readbacks, 0.0 position error. PASS. The entrance table was located in RAM at `0x8010dc80`
  (2048 entries, 129 scenes), so scene travel does not need ROM reverse engineering.
- QA level is `automated` for this prototype: scripted two-client tests assert shared state. No human
  playthrough; same-area co-op only, no trial-to-trial run yet.

## How to play

1. Dump your own Ocarina of Time (USA 1.0) cartridge (md5 `5bd1fe107bf8106b2ab6650abecd54d6`) and
   apply the official Ultimate Trial v1.2.3 BPS patch. No game files are distributed here.
2. Download [ut-coop](https://github.com/TheeDeb/ut-coop/releases/latest) and the
   [gopher64 co-op fork](https://github.com/TheeDeb/gopher64/releases/tag/v1.2.0-utcoop1).
3. Create a save in File 1 once in the emulator.
4. Run `ut-coop.exe` with no arguments and answer the wizard (host/join, IP, session).
   Advanced users can pass `--emulator`, `--rom`, `--peer`, `--bind` and `--session` directly.

## Known issues

- Same-area co-op only; trial-to-trial travel is not yet validated end to end.
- Story, enemy, world-state and item state are not synced.
- Puppet animation steps at the snapshot rate (no interpolation).
- After a scene load the puppet can be missing for about a second before re-injection.
- Boot automation taps through the intro and assumes a File 1 save; it can take about a minute.
- No NAT traversal; port forwarding or a VPN is required.
- Python-packaged prototype; a Rust client is future work.

## Next steps

1. ~~Build the fork, bridge, boot automation and address discovery.~~ Done.
2. ~~Render and animate a remote Link; gate by scene and re-inject after loads.~~ Done.
3. ~~Verify forced scene transitions and follow-host scene loading.~~ Done.
4. ~~Verify a true scene-to-scene follow.~~ Done between scenes 41 and 42. Validate broader
   trial-to-trial travel next.
5. Add interpolation, close the scene-load gap, and run full two-player bot QA across scenes
   and saves.
6. Port the client to Rust, add a relay/NAT story, and test a real internet session.
