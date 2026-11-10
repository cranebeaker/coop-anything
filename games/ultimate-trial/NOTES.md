# Ultimate Trial — working notes

Session 1: 2026-11-09. Agent working from a Windows 11 machine, workspace
`<workspace>` (git not installed; repo fetched as a zip from codeload).

## Game identity (phase 0)

| Field | Value | Source |
|---|---|---|
| Title | Ultimate Trial | [repo](https://github.com/RichieUltimate/ultimate-trial) |
| Kind | Total-conversion ROM hack of The Legend of Zelda: Ocarina of Time (1998), N64 | [romhacking.net](https://www.romhacking.net/hacks/8640) |
| Current version | 1.2.3 (2026-03-15) | [romhackplaza](https://romhackplaza.org/romhacks/ultimate-trial-nintendo-64) |
| Base ROM | OoT (USA) 1.0 ("U 1.0, NTSC 1.0, Rev 0"), md5 `5bd1fe107bf8106b2ab6650abecd54d6`, CRC32 `cd16c529`, SHA-1 `ad69c91157f6705e8ab06c79fe08aad47bb57ba7` | [repo README](https://github.com/RichieUltimate/ultimate-trial), [romhackplaza](https://romhackplaza.org/romhacks/ultimate-trial-nintendo-64) |
| Patch | `ultimate_trial_v1.2.3_n64.bps`, 30,917,075 bytes, SHA-256 `CD15868E2700F821A8F3342651D40B995F9A9C48B6DAD5F55025032CC86FB526` (downloaded from the official repo, checked) | local |
| Engine | OoT engine (N64, MIPS R4300). No netcode, single player | — |
| Source code | NOT available; a team member refuses to release it | [issue #19](https://github.com/RichieUltimate/ultimate-trial/issues/19) |
| Emulator compatibility (team-tested) | Project64 (interpreter core required), Simple64, N64 hardware (Expansion Pak required), Dolphin (Wii VC patch), RetroArch, Ares | [repo README](https://github.com/RichieUltimate/ultimate-trial) |
| Known incompatible | BizHawk (freezes/crashes) — rules out the BizHawk Lua bridge route | [repo README](https://github.com/RichieUltimate/ultimate-trial) |

## Observations (checks that ran)

1. **BPS patch analysis** (own parser, `local/bps_scan.cs`, format verified against
   [RomPatcher.js bps.js](https://github.com/marcrobledo/RomPatcher.js/blob/master/rom-patcher-js/modules/RomPatcher.format.bps.js)):
   - source size = target size = 32 MiB (no ROM expansion),
   - source CRC32 `CD16C529` matches the documented base ROM CRC32 → parser cross-checked,
   - target bytes: **90.5% TargetRead (brand new), 9.3% TargetCopy (repeats), 0.22% SourceRead
     (identical to vanilla), 0% SourceCopy** → the hack rewrote essentially the entire ROM:
     file table, code, scenes, objects.
   - Full block list in `local/ut_bps_blocks.csv`.
2. **Directory / prior work**: no Ultimate Trial entry in `data/directory.yaml`; GitHub
   issue search over cranebeaker/coop-anything for "ultimate trial" → 0 results. Web search
   found no multiplayer or co-op attempt for this hack anywhere.
3. **Same-engine prior work** (OoT): Anchor/Shipwright-Online/ocarina-coop (PC port route,
   vanilla ROM only — `games/ocarina-of-time`), Z64Online/ModLoader64 (emulator memory-sync
   route, vanilla OoT + OoT randomizer compat patches), Zelda64 Online supports OoTR ROMs
   via BPS compat patches ([repo](https://github.com/hylian-modding/Z64Online)).
4. **Tooling on this machine**: no git, no python, no gh CLI. PowerShell available; C#
   compiler available via `Add-Type`.

## Derived (from the above)

- Vanilla-address tools (Z64Online payloads, ML64-Z64Lib offsets, SoH/Anchor) cannot run
  UT: it is 90.5% new bytes, and SoH/Zelda64Recomp only accept the exact vanilla ROM anyway.
- UT is built on the OoT engine, so engine structures (GlobalContext, Actor, SaveContext,
  actor profile table, scene/room system) should keep their shapes at new addresses. The
  zeldaret/oot decomp is the reference for those shapes.
- Since UT ships patch-only (BPS applied to the owner's own dump), a co-op mod can follow the
  same distribution model: our own BPS delta on top of the UT-patched ROM. No game files
  shipped. Matches playbook legal rules (console-emulators.md §7).
- The puppet-actor payload pattern is proven on N64 by Z64Lib's CommandBuffer overlay
  (GPL-3.0): code cave + actor spawn hooks + function hooks, injected by the loader.

## Route options considered

| Route | What | Verdict |
|---|---|---|
| A. Emulator netplay (input sync, e.g. gopher64) | Friends share the one Link (take turns) | Cheap but not real co-op; fallback only |
| B. Memory-sync co-op, two emulators + RAM bridge + puppet actor (the ss-coop pattern from `games/skyward-sword`) | Real co-op: two Links, shared progress | **Recommended.** Big build-from-zero project |
| C. Revive Z64Online/ModLoader64 for UT | Closest existing framework, but vanilla addresses + ancient mupen64plus fork; would need full address port + possibly emulator updates | Possible later; not first |
| D. Ask the UT team (they are in the same Hylian Modding scene as Z64Online) | Could unlock upstream interest or their address map | Needs owner approval to contact anyone |
| E. SoH / Zelda64Recomp port | They reject non-vanilla ROMs; UT source closed | Rejected |

Route B details (if chosen):
- Emulator candidates: Simple64 (UT-tested, mupen64plus family, archived) or gopher64
  (active, Rust, mupen64plus core); both need a fork or plugin to expose RDRAM over a
  socket (ModLoader64 did exactly this via N-API). PJ64 works only in interpreter mode
  (team-documented recompiler crash); Dolphin/Wii-VC route possible (memory access solved
  via Dolphin Memory Engine) but requires WAD repacking.
- N64-side payload: own code cave (MIPS) spawning a puppet actor, hooks at UT's addresses;
  addresses found via decomp-informed signature scanning of UT RAM.
- Bridge: reuse the ss-coop bridge design (UDP, flags as events, save/load detection).
- QA: two emulator instances on one PC, bots drive via memory writes, per
  `playbook/automated-qa.md`.

## Guesses (not verified)

- UT was likely built with the Hylian Modding / decomp-derived toolchain (credits overlap
  with that scene), so its engine structs probably match the decomp's. Needs verification in
  phase 3.
- The PJ64 recompiler crash hints at self-modifying or DMA-unusual code; the mupen64plus
  family (Simple64) is officially tested so it should be the safer bridge host.

## Unknowns

- Whether the UT team would cooperate or accept a co-op patch (needs owner decision to ask).
- Whether UT's randologic/achievements systems complicate determinism or state sync (probably
  fine — flags are flags — but unverified).
- Whether the Wii VC patch of UT behaves under Dolphin for a two-instance test rig.
- Emulator choice for the bridge host: needs hands-on testing on the owner's machine.

## Route chosen (owner decision, 2026-11-09)

**Route B: memory-sync co-op, two emulators + RAM bridge + puppet actor.** 2 players,
online. Tools install approved (git, python, gh done; more later). gh login in progress;
claim issue to be opened on the owner's account.

Architecture (ss-coop pattern adapted to N64/OoT):

```
player A                                            player B
gopher64 (fork: RDRAM socket + bot input API)       same
   ^ local socket                                       ^ local socket
   |                                                    |
ut-coop bridge A  <--------- UDP (host forwards 1 port) -------->  bridge B
   reads own Link (GlobalContext/player actor),          mirrors of A
   writes remote Link into puppet mailbox
ROM: UT-patched ROM + our coop BPS patch (puppet actor in an unused
     overlay slot, hooks, mailbox). Patch-only distribution, no game
     files shipped. License: our N64 payload may study/port GPL-3.0
     Z64Online/ML64-Z64Lib code -> our mod repo must be GPL-3.0.
```

Key pieces to build:
1. `local/patch_rom.ps1` — done. Applies UT BPS to owner dump, CRC-verified both ways.
2. gopher64 fork with: RDRAM read/write over TCP + per-frame callback + scripted input
   (QA bots). Keep as our own GPL-3.0 fork; do NOT PR to upstream (they ban AI PRs).
3. Address discovery for UT ROM (phase 3): locate GlobalContext, gActorOverlayTable,
   player actor, SaveContext/scene flags, using decomp-derived signatures
   (oot/ clone at `<workspace>/oot`, `gActorOverlayTable` in `src/code/z_actor.c`,
   overlay entry struct in `include/z_actor_dlftbls.h`).
4. Puppet overlay in MIPS (n64 toolchain needed): adapt Z64Online's CommandBuffer
   puppet approach (GPL-3.0) to UT addresses.
5. Python bridge: UDP host/join, position/anim sync, flag sync as events, save/load
   detection (mass-clear brake), interpolation.
6. QA harness: two gopher64 instances, loopback, bot via scripted input + RDRAM asserts.

## gopher64 fork plan (verified against source, commit of clone 2026-11-09)

Verified architecture facts (all read from `<workspace>/gopher64`):

| What | Where | Notes |
|---|---|---|
| RDRAM | `device.rdram.mem: Vec<u8>` (`src/device/rdram.rs`), masked `& RDRAM_MASK (0xFFFFFF)` | default size `0x800000` = 8 MiB (`Device::new`, device.rs:458); `--disable-expansion-pak` drops to 4 MiB. UT requires the Pak → run with default. `rdram::init` hands the raw pointer to RA the same way our bridge can |
| Byte order | `device.byte_swap` = 3 on little-endian hosts (`device.rs:314-320`); cheats do `addr ^ byte_swap` per byte (`src/cheats.rs:93-132`) | RDRAM bytes are stored host-endian; XOR each game-address byte with 3 to get the big-endian game view. Our fork's TCP server will apply this once so the wire protocol is plain big-endian game-order bytes |
| Main loop | `cpu::run` (`src/device/cpu.rs:261`) — interpreter, one opcode per iteration | gopher64 is interpreter-only (no recompiler), which also matches UT's PJ64-recompiler-crash context |
| Per-frame hook | `vertical_interrupt_event` (`src/device/vi.rs:164`) | cheats execute + `retroachievements::do_frame()` run here — same place for our bridge frame callback |
| Input injection | `controller.rs:57-63`: `JCMD_CONTROLLER_READ` → `ui::input::get(&mut device.ui, channel)` unless netplay overrides with `netplay.inputs[channel]` | PIF channel model; netplay's override pattern is exactly what our bot input + remote-player input override will copy |
| Threading | tokio runtime created in `main` via `create_runtime()` + `handle.enter()` (`src/lib.rs:109`, `src/main.rs`) | tokio already a dependency (RA, netplay use it) → bridge socket adds no new top-level deps; emulator thread runs with runtime context entered, so `tokio::spawn` from emulation works |
| Savestates | `device.rs:296` `self.rdram.clone_from(&device.rdram)` etc. | full-device serde serialize; useful for QA bot determinism checks |
| Cheats | `cheats.rs` — RDRAM read/write helpers + conditionals keyed by ROM CRC (`ui::storage::get_game_crc`) | gopher64 keeps per-game cheat DB; our fork can reuse the same CRC gating for UT-specific hooks |
| No existing socket server | only WebRTC netplay (matchbox/ggrs, `src/netplay.rs`) | our TCP RDRAM server is new code, no conflict |

Fork changes planned (all additive, behind a `coop` feature/flag so upstream merge stays trivial):

1. `src/coop.rs` — new module: TCP server (`127.0.0.1:port`) with a small binary protocol:
   - `read {addr, len}` / `write {addr, bytes}` RDRAM (XOR `byte_swap` handled server-side),
   - `frame` notification per VI (pushed from `vertical_interrupt_event`),
   - `input set {channel, u32}` override (checked in `controller.rs` before `ui::input::get`),
   - `state save/load` passthrough for QA.
2. CLI flag `--coop-port N` in `Args` (`src/lib.rs` `run()`), plus `--coop-input` to force keyboard-free headless input.
3. Keep GPL-3.0, keep our fork separate, **no PRs upstream** (gopher64 bans AI-authored PRs).

QA bot plan (playbook/automated-qa.md): a Python harness opens both coop sockets, drives both
instances' input, and asserts on RDRAM (positions, flags) per frame.

### Fork changes implemented (2026-11-09)

All wiring is additive and lives behind the new `--coop-port` flag:

- `src/coop.rs` (new): TCP server on `127.0.0.1:PORT`, one client at a time, batching
  protocol (READ/WRITE/INPUT/STAT) executed at a frame boundary; disconnect clears all
  input overrides so a closed bridge never leaves a stuck button.
- `src/lib.rs`: `pub mod coop;`, `--coop-port PORT` argument, passed through `run()`.
- `src/device.rs`: `#[serde(skip)] coop: Option<coop::Coop>` field, started right after
  `rdram::init` (so the 8 MiB buffer exists).
- `src/device/vi.rs`: `coop::on_frame(device)` is the first line of
  `vertical_interrupt_event`.
- `src/device/controller.rs`: override order is netplay, then coop bot input, then
  physical input.
- Unit test `bridge_serves_read_write_input_and_stat` in `src/coop.rs` drives the real
  `on_frame` entry point against a fake 8 MiB buffer. It needs no ROM, so the wire
  protocol can be validated before the owner's dump is available.
  **Ran 2026-11-09**: `cargo test --release --lib` -> 1 passed, 0 failed. It checks a
  WRITE then READ round trip, the input override, and STAT (frame counter and RDRAM
  size). Protocol only; no game involved.
- `local/coop_bridge.py` (owner workspace, never committed): Python client and CLI for
  the same protocol, for the QA harness and the eventual game bridge.

Controller word (from `ui/input.rs`, verified): button bits 0-13 using the
`input_profile` indices (0 R dpad, 1 L dpad, 2 D dpad, 3 U dpad, 4 Start, 5 Z,
6 B, 7 A, 8 R C, 9 L C, 10 D C, 11 U C, 12 R trigger, 13 L trigger); signed X
axis in bits 16-23, signed Y axis in bits 24-31. The INPUT opcode takes this word
directly, so bots can drive a player without SDL or a keyboard.

Build environment on this Windows machine (installed with owner approval):

- Rust 1.99.1 via rustup; VS 2022 Build Tools 17.14.44 with the VCTools workload
  (cl/link/rc and the Windows SDK).
- LLVM 23.1.1 portable, matching rustc's own LLVM, from the official llvm-project
  release. SHA-256 `c54ac814...9264c3` verified against the release's signed SLSA
  provenance, extracted under `<workspace>/local/tools`. Needed because
  `gopher64/.cargo/config.toml` sets `CC=clang` globally and `build.rs` passes
  clang-style flags.
- `git` must be on PATH at build time: `build.rs` runs `git describe` and panics when
  it cannot execute it (the clone is shallow with no tags, so the `--always` fallback
  reports the commit hash).

### Verified end to end on the real ROM (2026-11-09)

- Owner's dump was found in Downloads and copied to `local/rom/base_ot.z64`; md5
  `5bd1fe107bf8106b2ab6650abecd54d6` matches OoT US 1.0. `local/patch_rom.ps1`
  produced `local/rom/ultimate_trial.z64`: CRC32 `627C0746` (matches the patch target),
  SHA-256 `7802EB8F...F3E142`, header magic `NZL`.
- `gopher64.exe local\rom\ultimate_trial.z64 --coop-port 47903` boots, renders UT's
  fan-made-games warning screen (window capture `local/logs/ut_window4.png`), and
  answers STAT: frame counter 358 after about 10 s, RDRAM size 8 MiB.
- `local/smoke_ut.py` automates that check. Last run: `frames: 18 -> 140; rdram 0x800000`.
- `local/scan_rdram.py` read all 8 MiB live through the socket (72% nonzero) into
  `local/dumps/rdram_boot.bin`.
- Real protocol bug found while testing and fixed: responses from a disconnected batch
  leaked into the next connection (a stale INPUT ack was read as a STAT reply). Every
  command and response now carries a connection id and stale responses are discarded;
  disconnect clears use id 0. Unit test still passes after the change.

## Address discovery, first pass (2026-11-09, verified in live RDRAM)

The gopher64 socket makes runtime structure scanning practical. Findings from the
title-screen dump:

- **Actor overlay table**: base `0x800fa7f0`, 513 entries of 0x20 bytes, entry 0 is
  `Player` (initInfo `0x801113d0`), entry 1 is `En_Zol`, and so on. The table is in the
  resident code segment, so it is present before gameplay. The name strings live at
  `0x8011d700` (`Player`), `0x8011d708` (`En_Zol`), ...
- **Actor struct offsets** (from the local zeldaret/oot decomp, `include/actor.h`):
  id +0x00, category +0x02, room +0x03, flags +0x04, world pos +0x24, world rot +0x30,
  focus +0x38, velocity +0x5C, speed +0x68, prevPos +0x100, next +0x124, init +0x128,
  destroy +0x12C, update +0x130, draw +0x134, overlayEntry +0x138. Size 0x14C.
- **Player instance**: scanning for a u32 pointing at `0x800fa7f0` (entry 0) found
  `0x80289550` with id 0, category 2, room -1, update `0x800a1040`, draw `0x800a1070`,
  world pos `(389.0, -211.0, 1029.3)`. This is the title-screen Link; it does not move.
  The same scan also returned live door actors (table index 35) and a few others, so the
  `overlayEntry` pointer is the reliable anchor.
- **Do not hardcode the actor address.** Allocation varies with boot path. The co-op host
  should scan for `overlayEntry == 0x800fa7f0 && id == 0` (cheap if limited to the heap
  region, or cached and re-validated per frame).

Menu/input facts learned while getting there:

- The title screen requires Start/A; the file select and name entry respond to A.
- The D-pad bits are ignored by the name-entry grid; the analog stick works. On the
  stick, +X is right and +Y is up; holding repeats, so frame-exact single-frame pulses
  (toggle, wait one VI, release) move exactly one cell.
- UT has a custom Options menu: Randologic (Disabled/Thematic/Chaotic), Interface
  Version (GameCube/N64), Aspect Ratio (4:3/16:9).

### Boot-to-gameplay flow, verified end to end (2026-11-09)

`local/boot_to_gameplay.py` now drives a fresh instance into controllable gameplay:

1. Wait for the title screen (~12 s), then tap A+Start on all four channels.
   The title advances to Select File.
2. A opens File 1. On a brand-new save this enters name entry; a file only becomes
   real after the name is confirmed. Name entry is navigated with the analog stick
   (frame-exact pulses); END, then the custom Options screen; **Start** leaves Options
   and returns to Select File with the file created.
3. A on the file, then A on "Yes" in the Open File dialog, then eight A taps with
   ~0.8 s gaps to clear the intro.

Verified run (cold boot, port 47921): player actor at `0x80289550`, position
`(408.97, 0.0, -2864.51)`, then holding the stick forward moved it 144.7 units. So
scripted input reaches gameplay and the player actor is writable/readable.

Save file location: `%APPDATA%\gopher64\saves\<rom-name>-<sha256>.sra` (32 KiB for
this game). Back it up before bot runs that save.

### Player state helper

`local/player_state.py` reads and validates the player actor. Validation is
`id == 0 && category == 2 && overlayEntry` points at table entry 0. `find_player`
rescans RDRAM if the cached address stops validating. The address has been
`0x80289550` on every boot so far, but do not rely on that alone.

## Two-instance sync prototype (2026-11-09, PASS on loopback)

Fork change: `GOPHER64_DATA_DIR` overrides the data/config/cache roots, so two
instances on one PC keep separate saves, states and config. Normal players are
unaffected (the variable is unset).

`local/sync_proto.py`
- Each side reads its player actor with one batched READ+STAT round trip per tick
  (the earlier six-read version could not hold 30 Hz).
- Sends a 48-byte UDP snapshot: magic, version, flags, seq, VI frame, position,
  rotation, speed, scene.
- Tracks receive count, sequence gaps and errors; reconnects the emulator socket
  after three consecutive failures.
- Optional `--remote-buf` writes the last received snapshot into a reserved RDRAM
  buffer (default candidate `0x807FF000`), which is the handoff the future puppet
  actor will read. Off by default until the buffer is proven unused.
- Input is queued through the node so only one thread ever uses the emulator socket.

`local/sync_test.py`
- Launches two gopher64 instances with separate data dirs, copies the same save into
  each, boots both to controllable gameplay (control-checked, retries intro taps),
  runs a node per side over UDP loopback, then moves A and B in turn.
- Pass criteria: each player moves, the peer receives the matching position, error
  under 5 units, more than 20 packets each.

Result 2026-11-09: **PASS**. A moved 418.2 units and B received its position with
0.0 error; B moved 153.9 units and A received with 0.0 error; 129 packets each,
0 gaps, 0 socket errors. This is loopback on one machine with no packet loss
injection yet, and it does not render a second player.

Next: puppet actor. Reserve and verify the RDRAM handoff buffer, then design the
patch-only injection that spawns a visible second Player actor driven by the remote
snapshot.

### Overlay table layout and the puppet decision (verified 2026-11-09)

The runtime `ActorOverlay` entry layout is confirmed against live entries:

- +0x00 vromStart, +0x04 vromEnd
- +0x08 vramStart, +0x0C vramEnd (fake VRAM >= `0x80800000`)
- +0x10 loadedRamAddr: 0 until the overlay is loaded, then the real RAM address
  (e.g. entry 24 -> `0x80290290`, entry 151 -> `0x80295470`)
- +0x14 initInfo (VRAM), +0x18 name, +0x1C numLoaded (u16), +0x1E allocType

Overlays are compiled at fake VRAM and relocated to their allocated address at load
(see `oot/src/libu64/loadfragment2_n64.c`). So the table's vram values must not be
treated as real RAM addresses; use `loadedRamAddr`.

Two puppet routes:

1. **Emulator-side actor injection.** The player is not an overlay: its update/draw
   pointers are resident at `0x800a1040` / `0x800a1070`. In principle the bridge can
   allocate a second Actor plus a copy of the `Player` struct in RDRAM, link it into
   an actor list and point it at the resident player code. No ROM change at all, and
   it keeps distribution to our own code. Risk: engine systems assume one player
   (camera, attention, save context), and the Player update expects a fully valid
   `Player` struct.
2. **Patch-only ROM overlay.** Add a new relocated overlay and actor-table entry plus
   a spawn hook. Cleaner for a shipped patch, but the runtime table is not
   byte-identical in ROM (the code segment is likely compressed/relocated), so this
   needs the loader's own structures reverse engineered first.

Recommendation: try route 1 for the first visible puppet; keep route 2 as the
shipping-clean fallback. Either way the next concrete step is to prove a reserved
RDRAM handoff buffer that the game side can read every frame.



## Phase 3 approach — revised after block-level analysis (2026-11-09)

Re-checked the BPS block list with Python over `local/ut_bps_blocks.csv` (337,478 blocks):
only **74,602 vanilla bytes** survive, in two ranges ≥ 64 bytes:

- `0x40–0x1003` (IPL3 boot code) — expected; hacks keep IPL3
- `0x8124CF–0x81250F` (64 bytes, in the dmadata region)

**Consequence:** signature-scanning UT's ROM against *vanilla* byte patterns is dead — there
is nothing vanilla left to anchor on. Address discovery must instead be:

1. **Runtime structure scanning in RDRAM** (via the gopher64 fork socket, once a UT ROM is
   running): identify the actor overlay table by shape (OoT 1.0 has ~472 entries, terminated;
   entries are 32-byte structs with vram/rom addresses and a load flag — see
   `oot/include/z_actor_dlftbls.h`), then the GlobalContext pointer and `gSaveContext` by
   their persistent-region layouts.
2. **Parse UT's own dmadata table from its ROM** (segment table → code segment address →
   overlay table location). UT is 90.5% new bytes but it is still an OoT-engine ROM with an
   OoT-style file table.
3. **Cross-check with the decomp**: UT's credits overlap the Hylian Modding/decomp scene, so
   struct layouts likely match `zeldaret/oot` — treat as a guess until verified in RAM.

## Owner state (private, `local/`)

- `local/ultimate_trial_v1.2.3_n64.bps` — official patch, hash above.
- `local/bps_scan.cs`, `local/run_bps_scan.ps1`, `local/ut_bps_blocks.csv` — patch analysis.
- `local/patch_rom.ps1` — applies UT to the owner's dump (CRC-verified both ways).
- Toolchain installed (approved): git 2.56.0, Python 3.13.17, gh 2.103.3 (logged in as
  TheeDeb), Rust 1.99.1 via rustup; VS 2022 Build Tools with C++ workload installing.
- Workspace: `<workspace>/gopher64` (clone with submodules), `<workspace>/oot` (decomp).
- Claim issue: cranebeaker/coop-anything#342. Fork: TheeDeb/coop-anything, branch
  `game/ultimate-trial`.

## Next steps

1. ~~Owner decisions, toolchain~~ — done (route B, 2 players online, installs approved).
2. ~~Claim issue~~ — done, cranebeaker/coop-anything#342.
3. Owner places their OoT US 1.0 dump at `local/rom/base_ot.z64`, then run
   `local/patch_rom.ps1` → `local/rom/ultimate_trial.z64`.
4. Build stock gopher64 once VS Build Tools finishes; verify UT runs on it headless (CLI).
5. Fork changes: `src/coop.rs` TCP server + `--coop-port` + input override.
6. Phase 3 address discovery on the live UT ROM (runtime scanning per above).
7. Phase 8 give-back (directory entry + README) at the next milestone.
