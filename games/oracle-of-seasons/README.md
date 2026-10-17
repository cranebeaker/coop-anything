# The Legend of Zelda: Oracle of Seasons (2001)

| | |
|---|---|
| Engine | Original Game Boy Color game, through the native C port |
| Versions tested | USA/Australia revision 0; native source `533860de269e0e9b338e12f3484ce8497367e6ed`; Linux x86-64 |
| Projects | [Native port](https://github.com/kirby-letsgo/oracles-decomp), [Oracles randomizer multiworld](https://jangle.dev/oracles-randomizer/manual.html#multiworld) |
| Co-op QA level | none; bounded native room/campaign and helper results below |
| Last updated | 2026-10-17 |

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
The [original runtime harness and marker helpers](coop/README.md) are included
here. They have been tested but do not add a playable multiplayer mode. Separate
experimental native changes passed bounded same-room, room/campaign, presentation/media
and loopback client gates. Separate experimental SDL and headless frontends now
exercise native host/join, independent guest input and authoritative guest media.
The engine adaptations and tests remain private and are not packaged/exported here.

## QA

Results from 2026-10-16 and 2026-10-17:

- Single-player smoke test on the exact native build: ROM validation, new game,
  opening overworld movement, and save-state round trip passed.
- Source build with project-local SDL3 3.4.16 completed. Original baseline command
  `ctest --test-dir <native build> --output-on-failure -j2` produced 13 passes,
  7 skips and 2 failures out of 22 tests. The `mods` timeout-message assertion
  and `app_menus` timeout failed before co-op implementation. Missing Ages ROM
  and external-service dependencies limit the baseline.
- Native isolation command `ctest --test-dir <native build> -R '^coop_runtime$' -V`
  passed in the test workspace. The exported harness has
  [standalone build instructions](coop/native-tests/README.md).
  Two alternately stepped real instances matched isolated references
  over 9,530 frames per stream, then ended with different real Link positions.
  A disabled-second-input negative control failed as expected. Checkpoint reload
  reproduced the results. This proves only bounded same-title, sequential,
  complete-frame runtime isolation; it does not test shared room gameplay.
- Experimental native same-room fixture: independent movement, walls, P2 block
  pushing, original sword/contact damage, enemy targeting and distinct owner
  graphics passed. Final gameplay and focused native checks passed 3/3. A broader
  24-target run had 15 passes, 7 skips and the same 2 baseline failures, spanning
  the final guard rebuild. This ordinary-Link/sword slice does not cover travel,
  general items, dialogue or a campaign. See [NOTES.md](NOTES.md) for boundaries
  and remaining regression-test gaps.
- A later bounded native room/campaign slice passed independent spec and quality
  review. It covers split/return while the partner fights or pushes, shared
  Satchel/Ember use, owner-only dialogue, mixed door transitions, contested
  resources/chests, death/potion cases, atomic seasons and malformed saves.
  Occupied enemies, blocks, scripts, fibers and clocks survive return. Final
  affected checks passed 37/37 (34 scenarios plus players/runtime/binary); a
  later stronger death assertion passed 1/1. The last full run was 47 passes,
  7 skips and 3 failures: its diagnostic-wording failure was corrected; the two
  others match the original baseline. The full suite was not rerun after the
  narrow fix. Existing SDL libdecor CMake and upstream compiler warnings remain.
  Save restoration is proven only for the exact Linux x86-64 little-endian
  non-PIE build, without cross-build or arbitrary PIE/ASLR portability. See
  [NOTES.md](NOTES.md) for admission limits and remaining campaign gates.
- A subsequent bounded native presentation/media slice passed independent spec
  and quality review. Each owner has a completed 160×144 RGB555 room view, own
  hearts/capacity, A/B names, private inventory and tracked P1/P2 labels. Native
  portrait/label/chevron markers cover adjacent outdoor bearings and neutral
  captions for unrelated interiors, avoid the owner's Link, and suppress unknown
  or disconnected partners through explicit presentation presence. Same-room
  offscreen coverage uses controlled native-renderer snapshot staging, not large
  camera gameplay. Exact framebuffer pixels now regress P2's blue owner graphics.
  Matching room/tick/generation/epoch audio is read once per unique runtime and
  shared among resident owners; first-attachment backlog is discarded. No audio
  hardware, listening or network playback was tested. Two full builds passed;
  the full 59-target run had 50 passes, 7 skips and the 2 known baseline failures
  before final compositor edits. Final affected checks then passed 38/38,
  including all 34 room scenarios and 7 top-level view scenarios. See
  [NOTES.md](NOTES.md) for equipment admission and remaining review limits.
- A bounded native client slice passed independent spec and quality review after
  lifecycle fixes. Two actual native processes over loopback exercised P2 sword
  damage/block pushing, split/occupied return, one shared Satchel grant,
  owner-only dialogue, stable-owner rejoin, equip/use and shared ammo. The guest
  receives its own view/HUD/private inventory/audio from the authoritative
  main-thread host; it runs no guest simulation and cannot save the campaign.
  Fresh startup validates each supplied raw ROM and follows the original
  button-only new-file flow from blank SRAM through first controllable prologue
  attachment and P2 movement. It does not cover the later mandatory dance or
  abduction. EOF/silence cancellation occurs before the next native tick, even
  when safe detachment is busy. Offline presence, frozen retained rooms, safe
  rejoin refusals and exact-build save/load have bounded coverage. Final affected
  checks passed 52/52 in 24.86 seconds; the earlier full 66-target run had
  56 passes, 7 skips and 3 failures. Its old save-format assertion was corrected;
  the other failures match the original mods/app_menus baseline. The full suite
  was not rerun after narrow fixes. Manually inspected Wayland SDL readbacks
  showed both bodies/tags and own inventory; the automated runner checks capture
  production only. There is no automated visual-regression, audio-hardware or
  listening proof. See [NOTES.md](NOTES.md) for presence and campaign limits.
- Original [loopback transport helper](coop/transport/README.md): two real-TCP
  test executables passed, with a failing baseline and six detected mutation
  controls. This covers bounded transport plumbing, not game-client networking.
- Original [Steam adapter](coop/steam/README.md): mock-only two-process tests
  passed, with thirteen detected mutations and a failing baseline after
  independent review. Steam invites are the selected Internet path; live
  integration remains unverified.
- Original [source-patch utility](coop/patch-tools/README.md): 28 toy tests and
  five detected mutations passed. No generated native-source manifest is included.
- Standalone edge-marker layout: 149 checks passed; original pixel renderer:
  105 checks passed. Run `make test` in `games/oracle-of-seasons/coop`.
  These 254 standalone checks also passed after the native presentation work.
  Two native clients now have bounded loopback coverage; Internet play and a
  co-op campaign have not passed their gates.

## How to play

There is no co-op package yet. The upstream native port can be used for
single-player with a separately supplied matching ROM. No ROMs, extracted assets,
game saves or decompiled game source are distributed in this playbook.

## Known issues

There is no host/join package. Native coverage remains a bounded bot-driven
slice: enabled normal Link plus tested death/hazard/item/entry adapters,
sword/Satchel with Ember seeds, native Satchel/small-key treasures, and limited
ordinary edges/entries. General tools, treasures, scripts and campaign coverage
remain open. Equipment re-equipping admits only the Satchel; unsupported
inventory entries display `UNAVAILABLE`. Native views and completed-frame audio
have bounded loopback client and gameplay presence/rejoin coverage. An affected
season change involving an offline room is safely refused pending season-aware
refresh. Live Steam, Internet play, whole-branch review and broader client QA remain
open; equipment display tests check initial letters rather than full words.

## Next steps

Complete native Steam frontend and fault integration, then validate actual
Steam accounts, friend invites and two-PC Internet play. The bounded loopback
client gate does not establish those outcomes.
Campaign gates still include bosses/essences, capacities/maps/compasses,
Rod/stumps, companions/transformations and the mandatory Din/troupe dance and
abduction beyond the tested fresh prologue.
Package and export the reviewed engine adaptations separately from the public
marker/transport/Steam/patch helpers; complete campaign bot coverage before
claiming campaign playability. Track progress in
[claim issue #1](https://github.com/cranebeaker/coop-anything/issues/1).
