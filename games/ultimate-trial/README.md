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
  and checks that each receives the other's moved position. Loopback test passes (129 packets
  each, 0 gaps, 0 errors, 0.0 position error). No second player is rendered yet.

## QA

- Emulator protocol test: `cargo test --release --lib` in the fork, 2026-11-09 ->
  `1 passed; 0 failed`. It drives the real per-frame hook against a fake 8 MiB RDRAM:
  WRITE then READ round trip, input override state, and STAT (frame counter, RDRAM size).
- Game boot smoke test: `python local/smoke_ut.py` (2026-11-09) -> `frames: 18 -> 140;
  rdram 0x800000`; a full 8 MiB RDRAM dump through the socket also succeeded.
- Boot-to-gameplay test: `python local/boot_to_gameplay.py --port 47921` (cold boot,
  2026-11-09) -> player position `(408.97, 0.0, -2864.51)`, then holding the stick
  forward moved it 144.7 units. Scripted input reaches real gameplay.
- Two-instance sync test: `python local/sync_test.py` (2026-11-09) -> PASS. Two
  instances booted to gameplay, exchanged 129 UDP snapshots each with 0 gaps and 0
  socket errors; each side received the other's moved position with 0.0 error
  (A moved 418.2 units, B moved 153.9). Loopback only; no second player is rendered yet.
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
2. ~~Find GlobalContext, the player actor, and the overlay table.~~ Player actor and
   actor overlay table found and validated; camera, scene id and save flags still to map.
3. Prove a reserved RDRAM handoff buffer that survives the game loop, then build the
   puppet actor (emulator-side injection first, patch-only overlay as fallback).
4. Harden the UDP bridge (loss, latency, jitter), then port the prototype to a
   shippable client.
5. Bot QA for two players: movement, scene transitions, save/load, and desync checks.
