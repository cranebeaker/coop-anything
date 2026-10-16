# Oracle of Seasons research notes

## 2026-10-16: triage and research

### Observed

- Target is the original Oracle of Seasons (2001), USA/Australia revision 0, through
  the Linux x86-64 native port. The ROM matches the port's expected SHA-1
  `ba1268290fb2b1b70505d2d7b5825fc8a4816a4b`. Private install paths and exact local
  identity records are kept under ignored `local/`.
- Native-port source and official nightly are pinned to
  [`533860de269e0e9b338e12f3484ce8497367e6ed`](https://github.com/kirby-letsgo/oracles-decomp/tree/533860de269e0e9b338e12f3484ce8497367e6ed).
  The release AppImage SHA-256 is
  `95337867a999ec6069679e4dda501d8ab3168a2f09696dd1d627e664911f010b`;
  its embedded native executable SHA-256 is
  `d4e9d266a23cecc8aaff1217082ba5ed4ec058edd439e7a5ee3aafa552cf6344`.
- A prior single-player smoke test on this exact build covered ROM validation, a
  new game, opening overworld movement, and a save-state round trip. It was not a
  co-op test or a campaign playthrough.
- The pinned [modding API](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/MODDING.md)
  permits observation and drawing; it cannot change gameplay or access the network.
- Native source retains one Link at bank-1 RAM `$d000-$d03f`, global action/input
  scratch, one current room and room-layout/collision buffers. Adjacent RAM is
  already occupied. See
  [ram.h](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/ram.h).
- The [launcher input callback](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/platform/app.c#L151-L153)
  merges all devices into one input byte.
- [Pushblock triggers](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/common/interactions/pushblockTrigger.c)
  mutate room layout and counters; the world cannot be updated once per player.
  [Door controllers](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/common/interactions/doorController.c)
  directly target the existing Link.
- Supplemental frame-order audit found `updateAllObjects_hook` in
  [bank0.c](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/bank0.c#L9973-L10021):
  special objects, items, enemy targeting, enemies, parts, interactions, room/scroll
  work, held-object/animation work, item post-pass, following object, camera and
  changed tiles/animations. Native same-room co-op must preserve one such world pass.
- Ordinary transitions in
  [bank1.c](https://github.com/kirby-letsgo/oracles-decomp/blob/533860de269e0e9b338e12f3484ce8497367e6ed/src/game/bank1.c#L5403-L5426)
  reload room layout, collisions and objects. Warp loading clears bank-1 object
  RAM. Running this over a room still occupied by the partner would erase that
  room's active state.
- The design received a fresh builder review: no important unanswered planning
  questions were identified. This is a document review, not gameplay validation.
- Existing Oracle [randomizer multiworld](https://jangle.dev/oracles-randomizer/manual.html#multiworld)
  distributes items between separate games. The
  [ArchipelagoOoS documentation](https://github.com/Dinopony/ArchipelagoOoS/blob/oos/worlds/tloz_oos/docs/en_The%20Legend%20of%20Zelda%20-%20Oracle%20of%20Seasons.md)
  and [MultiworldGG integration](https://github.com/MultiworldGG/MultiworldGG/tree/main/worlds/tloz_oos)
  describe the same distinction. These were researched, not installed or tested.

### Searched

- Refreshed this playbook checkout, checked its directory and game write-ups, and
  searched public issues and PRs for Oracle/Seasons. No matching work was returned.
  Public unauthenticated APIs were used instead of accessing an owner's account.
- Web searches for Oracle of Seasons co-op, multiplayer, online mods and native
  port co-op; indexed ModDB, Nexus, Archive.org, Steam-community and Japanese-title
  queries. Relevant findings were multiworld; other results were unrelated hacks.
  Private Discord material was not accessed.
- The native port's public issues, open/closed PR records, branches and both forks.
  Both forks exposed only `main`; no co-op implementation was identified.
- All 60 public forks returned by the Stewmath disassembly fork API were surveyed
  using public branch refs. No branch name indicated co-op/netplay/multiplayer.
  This was not a line-by-line audit of all fork code.
- Upstream `hack/crossitems` and `poc/crossworld1` concern item/content crossover,
  not observed simultaneous multiplayer. See
  [crossworld TODO](https://github.com/Stewmath/oracles-disasm/blob/poc/crossworld1/crossworld_todo.md).

### Derived

- No ready same-world two-Link implementation was found by the searches above.
  This is a bounded search result, not a proof that none exists anywhere.
- A native C fork is the viable hook point identified so far. Lua or controller
  multiplexing is insufficient for independent interactive players.
- Independent exploration requires separately owned occupied-room simulations,
  plus a solution for two interactive players sharing one room when reunited.
  Per-player action state, owned weapon objects, shared interactions and explicit
  campaign transactions must be designed before networking can be called co-op.

### Approved design; gameplay implementation in progress

- Two players, each with a camera and independent room travel. Shared permanent
  upgrades and campaign progress; independently chosen equipment and health.
- One authoritative host maintains campaign progress and room ownership. A room's
  enemies and puzzles have one owner even when both players occupy it.
- First feasibility gate is two genuine interactive players in one room. Subsequent
  gates cover splitting rooms, shared progression and reuniting without resets.
- The detailed approved design is in the dated design document under
  `docs/superpowers/specs/`.
- Offscreen partners get a small portrait, player label and outward chevron at
  the viewport edge. P2 is blue and P1 is green. Different coordinate spaces use
  an area label without a fabricated direction; visible or disconnected partners
  do not get an edge marker.

### Observed: source build and bounded runtime gate

- Built the pinned native source with GCC 16.2.1 and a project-local SDL3 3.4.16
  source archive. SDL archive SHA-256:
  `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`.
- Original full CTest baseline: 22 tests, 13 passed, 7 skipped, 2 failed.
  `mods` failed an assertion about which timeout message occurred;
  `app_menus` exceeded its 120-second timeout. Neither came from co-op edits.
  Ages-dependent targets and external-service scenarios did not all run.
- An original native runtime harness compares two alternately stepped Seasons
  instances against two separately stepped references. All 9,530 frames per
  stream matched sampled memory/state, rendered pixels and renderer metadata.
  The actual gameplay checkpoint is group `00`, room `c4`, Link `(72,79)`.
  Thirty frames of Right reached `(102,79)`; Left reached `(61,83)`.
- Disabling the second candidate's input while preserving its reference stream
  made the comparison fail on the first independent-input frame. Exporting and
  reloading the checkpoint reproduced both movement outcomes and state hashes.
- This establishes bounded sequential isolation for same-title native runtimes,
  with defaults and complete-frame scheduling. It does not establish shared-world
  co-op, thread safety, mixed-game runtimes or a full campaign.
- The standalone marker layout passed 149 assertions, including all compass
  edges, full badge clipping, HUD clearance, player labels, visibility suppression
  and area mode. Its original RGB555 portrait/label/chevron/caption renderer
  passed another 105 assertions and two targeted review-fix mutation controls.
  Native game rendering remains to be integrated and tested.
- The authorized claim is [issue #1](https://github.com/cranebeaker/coop-anything/issues/1).
  The public fork is [Lux-Hue/coop-anything](https://github.com/Lux-Hue/coop-anything).

### Unknown / untested

- General co-op gameplay and campaign coverage remain unverified. The bounded
  same-room gate below passed; migration and networking are still pending.
- The runtime gate above is intentionally narrower than arbitrary runtime
  isolation. Shared engine globals and renderer-slot lifetime need further work
  if their usage expands beyond the tested boundary.
- Every singleton consumer and runtime-global state has not yet been audited.
- Network reachability between two actual PCs is untested.
- Seasons/boss/script policies require implementation audits against the original
  game's affected-room semantics; shared progression cannot mean copying all RAM.
- The owner authorized the GitHub claim and fork. Contribution diffs must still
  be shown before opening the authorized draft PR; owner review stays unchecked.

## 2026-10-16: bounded native same-room gate

### Observed

- The experimental native changes passed independent source/test review for an
  ordinary-Link/sword fixture. Original Link state machines and owned weapons run
  for both players while shared enemies, parts, interactions and tile work run
  once. Discrete player/item fields are projected into canonical addresses;
  adjacent shared object slots are preserved.
- In-engine bot cases passed independent movement/idle controls, solid walls,
  P2 initiating and settling an original pushblock, P2 sword damage, P2-only
  incoming contact damage, and a real enemy choosing nearer P2. The controlled
  combat/puzzle fixture places original enemies/tiles before input; it does not
  establish campaign spawning or an authored route through the game.
- A native framebuffer image showed both actual actors with different poses and
  green/blue identification. Owner graphics are committed with matching OAM DMA.
  The current automated render assertion checks OAM/packets; framebuffer pixel
  regression coverage is still pending.
- Disabling P2 dispatch and omitting private input history both made behavioral
  controls fail. Fiber entry/yield/reset, stack loads, handoffs and bad returns
  are rejected with caller context restored. Two real VBlank IRQs during a
  projected call preserved canonical sampled player identity.
- One measured workload produced 120 world ticks over 120 LCD frames, retaining
  original clock/IRQ accounting. This is observed performance for that fixture,
  not a general timing guarantee. The timing test currently checks counter
  agreement; a literal cadence regression assertion remains pending.
- Final post-guard gameplay and focused native checks passed: `coop_players`,
  `coop_runtime`, `native_binary` (3/3). The broader 24-target run produced
  15 passes, 7 skips and the same 2 baseline failures. It spanned the final guard
  rebuild; the report distinguishes pre/post-change binaries. It is not a fresh
  complete-suite run of one final source state.
- The standalone exported runtime harness was separately compiled and run using
  a clean pinned native checkout with SDL off. Positive and expected-failing
  disabled-input runs reproduced the recorded movement/state results.
- Research and original helpers were published as
  [draft PR #3](https://github.com/cranebeaker/coop-anything/pull/3), after showing
  the contribution diff. The draft and unchecked owner-review status were verified.

### Still pending

- The native gate admits ordinary state-1 Link with no item or sword, no companion,
  text, menu, death or transition. Unsupported modes stop clearly. This is an
  early feasibility slice, not a playable mod or a full player implementation.
- Occupancy, portable player contexts, split/rejoin without resetting an occupied
  room, typed campaign transactions, saves, owner dialogue/inventory and seasons
  are the next implementation work. Network clients and packaging follow them.
- Simultaneous swords dispatch one collision effect in the tested case. A
  nonlethal target/HP comparison against a single-owner control is still needed
  to make that exact damage outcome an independent regression test.

## 2026-10-16: original transport helper gate

### Observed

- The original [loopback transport helper](coop/transport/README.md) passed
  independent implementation and behavioral-test review. Two real-TCP test
  executables passed compatibility, malformed-peer, framing, replay, slot
  reservation, reconnect sequence, backpressure, EOF and short-I/O cases.
- The exported public copy also passed both test executables. Its preserved
  buildable baseline and six compile-success mutations each failed through
  normal behavioral assertions, including guest-direction validation and
  unsent-tail loss. Compile errors and signal deaths were not counted.
- Listeners and connectors accept only literal `127.0.0.1`; test listeners use
  ephemeral ports and verify cleanup. Payloads are opaque and bounded. This
  gate neither supplies nor tests a shared-world game adapter.

### Still pending

- Real native game clients, authoritative view/event encoding, reconnect
  resynchronization, security, fault-condition gameplay tests and friend
  reachability remain open. The helper does not make co-op playable.

## 2026-10-16: Steam target and original packaging helpers

### Observed

- The selected Internet path is Steam friend invites. The original Linux
  [Steam adapter](coop/steam/README.md) passed independent spec, code-quality and
  behavioral review after fixing a rejected-message silence-timeout bypass.
- The exported copy passed its mock suite: friends-only two-member lobby, joined
  peer checks, compatibility metadata, bounded messages, deadlines and cleanup.
  Two mock processes exchanged 4096-byte payloads over loopback. A buildable
  baseline and eight compile-success mutations failed normally. No real Steam
  library, account, lobby, invite, overlay or relay was exercised.
- Linux ABI declarations were checked against pinned SDK1.64 primary source.
  Linux callback packing differs from the Windows example in the playbook.
- The original [source-patch utility](coop/patch-tools/README.md) passed
  independent review and 28 toy-repository tests on its exported copy, with five
  detected behavioral mutations. It reconstructs a new private source tree from
  a pinned Git base plus reviewed insertions/removal positions; removed source
  bytes and surrounding decompilation context are not serialized.
- A separate private mechanical check reconstructed all 1,377 blobs/modes of
  the bounded native prototype, including 20 changed files. This verified bytes,
  not source authorship, buildability or gameplay. Unchanged baseline binaries
  remain only in the private reconstructed tree; the reference gitlink stays
  uninitialized and no submodule is fetched. No generated manifest or native
  source snapshot is distributed here.

### Still pending

- The three Steam-helper test-oracle improvements were subsequently addressed;
  see the 2026-10-17 entry below.
- The patch utility's narrow-character test prescribes one valid edit script;
  loosening that assertion is a minor maintenance improvement. Every actual
  insertion/new file still needs separate origin review before publication.
- Native game clients, common game handshake, presence/rejoin behavior, views,
  media transport, Host/Invite/Join UI, faults and live two-account Internet
  reachability remain open. None of these helpers makes co-op playable.

## 2026-10-17: Steam regression-test observations

### Observed

- Independent review accepted three strengthened mock-helper tests: actual
  nondefault published metadata, the intended recipient receiving the original
  copied binary payload once after backpressure, and send timeout while a
  recently active peer keeps receive silence below its deadline.
- The full mock suite passed. The buildable baseline and thirteen deliberate
  mutations compiled successfully and failed normal assertions; five new
  mutations independently exercised the added expectations. Compiler errors,
  signals and timeouts do not count as detection.
- Original production adapter code is unchanged. Only original tests, mock
  observations and mutation-runner controls changed; prior frozen evidence was
  retained. The exported test sources match the reviewed snapshot.

### Still pending

- These are mock transport-boundary results. Native clients, actual Steam
  authentication, a visible invite flow and two-account Internet reachability
  remain unverified. No real Steam account or API was exercised by this work.

## 2026-10-17: bounded native room/campaign observations

### Observed

- The bounded native slice passed independent spec and quality review after
  fixing death during pending cold travel and checking restored queued dialogue,
  pickups and chests through completion, including a fresh process execution.
- Native P2 split rooms while P1 fought/pushed; the original Satchel unlock
  reached absent P1, who equipped it and spent an Ember seed. Dialogue accepted
  only its owner's input. Return preserved occupied enemies, blocks, scripts,
  fibers and clocks. Cases covered sixteen mixed door transitions, concurrent
  final-key/seed and chest claims, individual/both-player death and potion use,
  original room-pack atomic season changes, unsafe water and malformed saves.
- Shared progress uses typed claims, events and resources with physical room
  identity. A parked pre-gameplay loader is bound to its actual stable owner;
  shared world work steps once. Progress is not merged by whole-RAM OR.
  Measured cold graphics took two logic ticks/three LCD frames; this does not
  establish general one-to-one cold-travel timing.
- Final affected checks passed 37/37: 34 scenarios plus native players, runtime
  and binary checks. A later test-only stronger death assertion passed 1/1.
  The last full baseline-containing run was 47 passes, 7 skips and 3 failures.
  Its diagnostic-wording failure was corrected; the other two match the original
  mods/app_menus baseline. The full suite was not rerun after the narrow fix.
  Existing SDL libdecor CMake and upstream compiler warnings remain disclosed.
- Save proof applies to the exact Linux x86-64 little-endian non-PIE build.
  Arbitrary PIE/ASLR and cross-build save portability are unproven.

### Still pending

- Admission covers enabled normal Link and the tested death/hazard/item/entry
  adapters, sword/Satchel with Ember seeds, native Satchel/small-key treasures
  and limited ordinary edges/entry variants. There is no complete tool, treasure,
  script or campaign registry. Boss completion/essences, capacities/maps/compasses,
  Rod/stump use, companions/transformations and fresh Din/troupe global scenes
  remain integration gates.
- Per-player presentation, own HUD/inventory and offscreen-marker integration
  are being implemented and remain unproven. Engine adaptations/tests are not
  yet packaged/exported in this playbook; the public marker, transport, Steam
  and patch helpers remain separate.
- Real two-client play, presence/rejoin, media, native Steam controls, live
  Internet and a campaign bot remain open. Co-op QA stays `none`; these bounded
  native observations do not establish a playable package or full campaign.
