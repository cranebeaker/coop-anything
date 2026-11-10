# Ultimate Trial (2024)

| | |
|---|---|
| Engine | Nintendo 64, Ocarina of Time engine (closed-source total-conversion ROM hack) |
| Versions tested | Ultimate Trial v1.2.3 (BPS patch SHA-256 `CD15868E2700F821A8F3342651D40B995F9A9C48B6DAD5F55025032CC86FB526`) on a gopher64 fork: boots and runs, memory bridge live. No co-op yet |
| Projects | none yet. Claimed in [issue 342](https://github.com/cranebeaker/coop-anything/issues/342); working notes in [NOTES.md](NOTES.md) |
| QA level | none |
| Last updated | 2026-11-09 |

## Where things stand

Nothing is playable yet: this entry is a claimed gap with triage and research done. Ultimate Trial is a
closed-source total-conversion hack of Ocarina of Time (USA 1.0), distributed as a BPS patch, and it
has no co-op or multiplayer of any kind. The planned route is the memory-sync pattern used for
[Skyward Sword](../skyward-sword/README.md): two emulator instances, a bridge that moves player state
over UDP, and a puppet second player injected by a patch-only BPS delta applied on top of each
player's own UT-patched ROM.

## Existing work

- **No multiplayer or co-op attempt exists for Ultimate Trial.** Searched 2026-11-09: the directory,
  GitHub issue search over this repo, a web search for the obvious terms, and the hack's own issue
  tracker. Nothing found.
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
  layouts probably match the decomp, but that is an assumption to verify in RAM, not a fact.

## What we changed

No co-op code ships yet. Done so far (details and sources in [NOTES.md](NOTES.md)):

- Verified the game's identity and the base ROM (OoT USA 1.0, md5 `5bd1fe107bf8106b2ab6650abecd54d6`)
  and wrote a BPS applier that verifies source and target CRC32 both ways. The patched
  test ROM has CRC32 `627C0746` and boots.
- Parsed the official patch block by block and recorded what survives from vanilla (see above).
- Picked the emulator host: gopher64 (Rust, GPL-3.0, active, Simple64 lineage). Verified in its source
  where the per-frame hook (`vertical_interrupt_event`), the guest RAM (`device.rdram.mem`) and the
  controller-input injection point (`JCMD_CONTROLLER_READ`) live. The fork stays a separate GPL-3.0
  derivative; gopher64 does not accept AI-authored upstream PRs.
- Built the fork and added a local TCP memory bridge (`--coop-port`): batched READ/WRITE/INPUT/STAT
  against live RDRAM, executed at VI frame boundaries, with per-connection response tags and input
  overrides that are released on disconnect.
- Booted Ultimate Trial v1.2.3 on the fork and confirmed it renders and runs (UT's fan-made-games
  warning screen captured, VI frame counter advancing, 8 MiB RDRAM served over the socket).
- Found the runtime structures co-op needs: actor overlay table at `0x800fa7f0` (513 entries,
  entry 0 is `Player`), and the live player actor at `0x80289550` with world position at `+0x24`.
  The actor is validated through its `overlayEntry` pointer. A scripted bot now boots from power-on
  to controllable gameplay, reads the player, and moves it with the bridge (verified: 144.7 units
  of movement).
- Built a two-instance sync prototype: 48-byte UDP snapshots of player position/rotation/speed at
  up to 30 Hz, sequence-gap and error tracking, and a QA harness that boots two emulator instances
  and checks that each receives the other's moved position. Loopback test passes (24/21 packets,
  0 gaps, 0 errors, 0.0 position error).
- Emulator-side puppet prototype: the bridge copies the live Player struct into reserved RDRAM,
  links it into the player actor list tail and points it at the resident Player draw code. A second
  Link renders on each instance at the other player's position, driven entirely by the received
  snapshots. Snapshots now carry appearance and the 24-limb joint table, so the puppet moves and
  animates; a watchdog re-injects it after a scene transition. Two-instance test passes with 0.0
  position error, matching joint data that changes over time, and re-injection 0 -> 1 per side.
- Located the engine scene state: `PlayState` base `0x80276f80`, `sceneId` at `0x80277024`,
  `sceneSegment` at `0x80277030`. Confirmed on a real transition: the fountain is scene 42
  (segment `0x806AD070`) and the canyon/start area is scene 41 (segment `0x806A9000`).
- Found and verified forced scene transitions: `transitionTrigger` at `0x80288DC1` (write 20)
  and `nextEntranceIndex` at `0x80288DC6`. Writing entrance `0x0712` + trigger in scene 41
  faded the game and respawned Link at that entrance ~2.9 s later.
- Implemented follow-host scene sync: snapshots carry the scene id and the host's actual entrance
  index; when the peer's scene differs it writes that entrance + trigger and reloads. Test passes
  (fake mismatch -> both reload -> scenes match, puppets restored and drawn). No entrance table is
  needed because the peer replays the host's own entrance index.
- Added a standalone client: `ut-coop` launches the fork, optionally boots the game, and runs the
  sync node with host/join flags. Packaged as a single Windows exe with PyInstaller, with a
  32-bit session id in the snapshot to drop foreign traffic, and tested against the Python node
  (281 snapshots, 0 gaps, exact puppets, both drawn).
- Added scene gating: the puppet is hidden when the two players report different scene ids and
  restored when they match (tested with a forced 42 vs 999 mismatch).
  Known limits: polled re-injection (about a second gap after a scene load), no interpolation,
  Python prototype only.

## QA

- Emulator protocol test: `cargo test --release --lib` in the fork, 2026-11-09 ->
  `1 passed; 0 failed`. It drives the real per-frame hook against a fake 8 MiB RDRAM:
  WRITE then READ round trip, input override state, and STAT (frame counter, RDRAM size).
- Game boot smoke test: `python local/smoke_ut.py` (2026-11-09) -> `frames: 18 -> 140;
  rdram 0x800000`; a full 8 MiB RDRAM dump through the socket also succeeded.
- Boot-to-gameplay test: `python local/boot_to_gameplay.py --port 47921` (cold boot,
  2026-11-09) -> player position `(408.97, 0.0, -2864.51)`, then holding the stick
  forward moved it 144.7 units. Scripted input reaches real gameplay.
- Two-instance sync + puppet test: `python local/sync_test.py` (2026-11-09) -> PASS.
  The harness boots two instances into gameplay, injects a puppet actor on each side,
  teleports each real player and checks delivery. Each side's puppet read back exactly
  the snapshot it last applied (0.0 position error), the streamed joint table matched
  the written bytes and changed over time, and `isDrawn = 1` on both sides; 83/79 UDP
  snapshots, 0 gaps, 0 socket errors. Detaching both puppets made the watchdog restore
  them (0 -> 1 re-injection per side, position exact). Scene gating: both read scene 42;
  forcing A to report scene 999 hid both puppets (gate counters 0 -> 1 each) and clearing
  it restored them with exact positions and `isDrawn = 1`.
- Client interop test: packaged `ut-coop.exe` on B plus the Python sync node on A,
  both with session `0x1234` (2026-11-09) -> PASS. 281 snapshots with 0 gaps and exact
  positions; A's puppet matched B and B's puppet matched A, both `isDrawn = 1`.
- Co-op QA level stays `none`: this is a loopback prototype (no internet, no packet
  loss testing). Scene follow is verified with a faked mismatch (both sides replay entrance
  `0x712`, reload, puppets restored), and a forced local transition (entrance `0x0712` +
  trigger) faded and respawned the player in ~2.9 s. A true trial-to-trial follow, network
  hardening and a Rust client are still missing.
- Co-op QA level stays `none`: no second player exists yet.
- Plan (from [automated-qa.md](../../playbook/automated-qa.md)): two gopher64 instances on one PC over
  loopback, driven through the fork's RAM socket and scripted input, asserting on positions and flags
  per frame.

## How to play

Not possible yet. UT itself can be played solo: apply the official BPS patch to your own OoT USA 1.0
dump in the emulator of your choice.

## Known issues

- No co-op exists yet; everything below is risk, not a promise.
- UT's rewritten ROM means every address has to be found by runtime structure scanning, not by
  vanilla signatures. This is the slowest part of the project.
- The hack requires the Expansion Pak (8 MiB RDRAM).
- UT ships no source; if the team later publishes one, that changes the plan for the better.

## Next steps

1. ~~Build a stock gopher64 and confirm UT v1.2.3 boots headless.~~ Done; the fork adds
   `--coop-port` and boots to controllable gameplay with scripted input.
2. ~~Find the player actor and overlay table.~~ Done, plus `PlayState`, `sceneId` and
   `sceneSegment`; camera and save flags still to map.
3. ~~Prove a reserved RDRAM handoff buffer and a visible animated puppet.~~ Done, with
   scene gating and re-injection after actor-list rebuilds.
4. Reach a trial scene and run a true scene-to-scene follow (different scene ids).
5. Harden and ship the client: packet-loss/jitter tests (chaos relay), package the
   gopher64 fork with it, publish the client repo (owner approval), and port to Rust.
5. Harden the UDP bridge with chaos testing (delay, loss, jitter), add interpolation and
   close the scene-transition gap; port to a shippable client and run full two-player bot
   QA across scenes and saves.
