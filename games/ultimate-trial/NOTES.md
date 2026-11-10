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

## Owner state (private, `local/`)

- `local/ultimate_trial_v1.2.3_n64.bps` — official patch, hash above.
- `local/bps_scan.cs`, `local/run_bps_scan.ps1`, `local/ut_bps_blocks.csv` — patch analysis.
- Machine: Windows 11, PowerShell, no git/python/gh yet.

## Next steps

1. Owner decision: route (B recommended), plus permission to install toolchain
   (git, python, VS Build Tools or Docker, an N64 emulator to fork).
2. Owner provides their own OoT US 1.0 dump for local testing (never committed anywhere).
3. Open a "claim a game" issue on cranebeaker/coop-anything (needs owner's GitHub).
4. Phase 2: workspace setup, then RE of UT ROM (address discovery) with the decomp as
   reference.
