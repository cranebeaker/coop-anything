# The Legend of Zelda: Oracle of Seasons (2001)

| | |
|---|---|
| Engine | Original Game Boy Color game, through the native C port |
| Versions tested | USA/Australia revision 0; native source `533860de269e0e9b338e12f3484ce8497367e6ed`; Linux x86-64 |
| Projects | [Native port](https://github.com/kirby-letsgo/oracles-decomp), [Oracles randomizer multiworld](https://jangle.dev/oracles-randomizer/manual.html#multiworld) |
| Co-op QA level | none; native baseline and feasibility harness results below |
| Last updated | 2026-10-16 |

## Where things stand

No ready same-world, two-Link co-op build was found in the bounded research
described in [NOTES.md](NOTES.md). A source-based implementation is in progress;
it is not yet playable with a friend. Existing randomizer multiworld sends items
between separate games.

## Existing work

- The [native port](https://github.com/kirby-letsgo/oracles-decomp/tree/533860de269e0e9b338e12f3484ce8497367e6ed)
  provides source access and a Linux application. Its tested single-player build
  loads the matching original ROM; two controllers currently feed one Link.
- [Oracles randomizer multiworld](https://jangle.dev/oracles-randomizer/manual.html#multiworld)
  exchanges items between worlds. [ArchipelagoOoS](https://github.com/Dinopony/ArchipelagoOoS/tree/oos/worlds/tloz_oos)
  and [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/tree/main/worlds/tloz_oos)
  also document multiworld integration. These projects were researched, not
  installed or tested here.
- Native-port issues, PR records, branches, both public forks, and branch refs in
  60 disassembly forks were surveyed on 2026-10-16. No obvious same-world co-op
  implementation was identified. This is not an exhaustive code audit.

## How the game works for co-op

The native source retains original registers, RAM addresses and game-thread
conventions. Link occupies one 64-byte object, but input, actions, health,
equipment, weapons, enemy targeting and scripts also read singleton state.
Duplicating the object is insufficient. Adjacent object RAM is already occupied.
See the pinned [RAM definitions](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/ram.h)
and [mod API limits](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/MODDING.md).

Room transitions reload shared layout, collision and object data. Independent
travel therefore needs explicit occupied-room ownership: entering the partner's
room must join its current enemies and puzzles. Updating a whole room once per
player would advance shared objects twice.

The [approved design](../../docs/superpowers/specs/2026-10-16-oracle-of-seasons-coop-design.md)
uses two independent Links and cameras, shared permanent abilities and campaign
progress, separate health/equipment, and an offscreen partner marker. The intended
host owns up to two occupied room simulations and resolves shared actions once.

## What changed

Research, the design, and the implementation plan are recorded in this repository.
The native source and game-derived fixtures stay in ignored local workspaces.
The first runtime harness and standalone marker layout have been tested; they
do not add a playable multiplayer mode.

## QA

Results from 2026-10-16:

- Single-player smoke test on the exact native build: ROM validation, new game,
  opening overworld movement, and save-state round trip passed.
- Source build with project-local SDL3 3.4.16 completed. Original baseline command
  `ctest --test-dir <native build> --output-on-failure -j2` produced 13 passes,
  7 skips and 2 failures out of 22 tests. The `mods` timeout-message assertion
  and `app_menus` timeout failed before co-op implementation. Missing Ages ROM
  and external-service dependencies limit the baseline.
- Native isolation command `ctest --test-dir <native build> -R '^coop_runtime$' -V`
  passed. Two alternately stepped real instances matched isolated references
  over 9,530 frames per stream, then ended with different real Link positions.
  A disabled-second-input negative control failed as expected. Checkpoint reload
  reproduced the results. This proves only bounded same-title, sequential,
  complete-frame runtime isolation; it does not test shared room gameplay.
- Standalone edge-marker layout: 149 checks passed. Marker rendering inside the
  native game, shared-world gameplay, two-client networking and a co-op campaign
  have not yet passed their gates.

## How to play

There is no co-op package yet. The upstream native port can be used for
single-player with a separately supplied matching ROM. No ROMs, extracted assets,
game saves or decompiled game source are distributed in this playbook.

## Known issues

Friends cannot yet control two interactive Links in one shared room. Independent
travel, shared campaign transactions, save compatibility, dialogue, seasons,
bosses, reconnects and Internet reachability remain unverified.

## Next steps

Prove two actual players in one room with original movement, walls, pushblocks,
weapons, enemy targeting and independent incoming damage, while advancing the
world once. Then test split/rejoin room ownership and shared abilities before
building host/join transport and the playable package. Track progress in
[claim issue #1](https://github.com/cranebeaker/coop-anything/issues/1).
