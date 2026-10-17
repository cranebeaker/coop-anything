# Oracle of Seasons research notes

Entries preserve the observations and pending work at each stage. The later
native loopback-client and native Steam entries update earlier client, presence,
media and frontend/fault gaps; broader campaign and Internet gates remain open.

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
  Native game rendering was subsequently covered by the bounded 2026-10-17
  native presentation/media observations below.
- The authorized claim is [issue #1](https://github.com/cranebeaker/coop-anything/issues/1).
  The public fork is [Lux-Hue/coop-anything](https://github.com/Lux-Hue/coop-anything).

### Unknown / untested

- General co-op gameplay and campaign coverage remain unverified. The bounded
  same-room gate below passed; later room and loopback-client gates are recorded
  below.
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
  regression coverage was subsequently added; see the 2026-10-17 native
  presentation/media observations below.
- Disabling P2 dispatch and omitting private input history both made behavioral
  controls fail. Fiber entry/yield/reset, stack loads, handoffs and bad returns
  are rejected with caller context restored. Two real VBlank IRQs during a
  projected call preserved canonical sampled player identity.
- One measured workload produced 120 world ticks over 120 LCD frames, retaining
  original clock/IRQ accounting. This is observed performance for that fixture,
  not a general timing guarantee. The timing test then checked counter
  agreement; the later native-client gate adds a literal warm-workload assertion.
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
  nonlethal target/HP comparison against a single-owner control was still needed
  at this point; the later native-client gate adds that regression.

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
- The patch utility's narrow-character test refinement was subsequently reviewed;
  see the 2026-10-17 patch-test observations below. Every actual insertion/new
  file still needs separate origin review before publication.
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
  remained unverified in this helper-only work. See the later native Steam entry
  for subsequent integration and real-library initialization observations.

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
  were subsequently covered by the bounded native presentation/media entry below.
  Engine adaptations/tests are not
  yet packaged/exported in this playbook; the public marker, transport, Steam
  and patch helpers remain separate.
- Real two-client play, presence/rejoin, media, native Steam controls, live
  Internet and a campaign bot remain open. Co-op QA stays `none`; these bounded
  native observations do not establish a playable package or full campaign.

## 2026-10-17: bounded native presentation/media observations

### Observed

- The bounded native presentation/media slice passed fresh independent spec and
  quality review. Completed per-owner RGB555 160×144 views show actual different
  room images, own hearts/capacity, A/B names, private inventory and tracked
  P1/P2 labels. Re-equipping remains Satchel-only; unsupported entries display
  `UNAVAILABLE`. Broader tools and the Sword equipment registry remain later gates.
- Native blue P2/green P1 portraits, labels and outward chevrons cover actual
  adjacent outdoor C4/C5 east/west bearings. Unrelated interiors use readable
  neutral area captions. Markers move along the edge to avoid the owner's Link
  while retaining viewport/HUD clamps. Explicit presentation presence hides
  unknown/disconnected partners; neutral input does not imply disconnection.
- Same-room offscreen coverage uses controlled real native-renderer snapshot
  staging because admitted small-room gameplay clamps actors. It does not prove
  large-camera gameplay. The real P2 framebuffer assertion checks 49 exact pixels;
  disabling only owner-render selection changes 44 blue body pixels to zero.
  This closes the earlier packet-only owner-render regression gap.
- Completed audio is matched by room, tick, generation and epoch, with signed
  16-bit stereo at 48 kHz and the actual sample count. Each unique runtime is
  destructively read once, then its resident owners share the result. Prepared,
  unoccupied and first-attachment backlog is discarded; the first boundary
  honestly has zero samples. Views/media add no extra world tick. This is native
  data-boundary coverage, with no audio hardware, listening or networking claim.
- Two full builds passed. The 59-target full run had 50 passes, 7 skips and the
  2 known baseline failures (mods cut-off-text assertion and app_menus 120-second
  timeout), in 477.93 seconds. That run preceded the final compositor avoidance
  and UI edits. Final affected checks passed 38/38 in 33.49 seconds, covering
  7 top-level native-view scenarios, all 34 room scenarios, players, runtime and
  native binary. The standalone public marker's 254 checks also passed.
  The existing SDL libdecor CMake warning remains; the final incremental build
  produced no new compiler warnings.

### Still pending

- Equipment pixel-display tests decode initial letters rather than the full
  word or `UNAVAILABLE`; strengthening those assertions is an optional minor
  improvement. Whole-branch review and native-client QA remain open.
- Native client transport, actual gameplay presence/rejoin, media transport,
  native Steam Host/Invite/Join controls, live Internet reachability, campaign
  validation and packaging remain open. Explicit marker presentation presence
  does not establish a working multiplayer presence lifecycle.
- The room/campaign admission limits and exact-build save boundary above still
  apply. Engine adaptations remain private and are not exported in this playbook.
  Co-op QA remains `none`; these observations do not establish a playable co-op
  package or full campaign.

## 2026-10-17: patch-test observations

### Observed

- Independent spec, quality and behavioral review accepted a refinement to the
  patch utility's tests with no findings. Production helper code is unchanged;
  only `test_packager.py` changed. The reviewed tests passed all 28 cases and
  detected the five existing compiled mutation controls.
- New private controls show that complete changed-word replacement is accepted,
  while an exactly reconstructing whole-line/context copy is rejected by a
  normal assertion. This checks the test's edit-script flexibility and context
  exclusion; it does not establish source origin or game readiness.

### Still pending

- The byte-identical exported test copy also passed all 28 tests (exit 0,
  3.595 seconds). Every actual insertion/new file continues to require separate
  origin review before publication.

## 2026-10-17: bounded native loopback-client observations

### Observed

- The bounded client slice passed independent spec and quality review after
  disconnect and defeat-rollback fixes. Separate experimental SDL and headless
  frontends share an authoritative main-thread controller. Each guest consumes
  its own completed view, HUD, private inventory and audio and sends independent
  input; it runs no guest simulation and cannot save the campaign. This work
  targets the pinned [native source](https://github.com/kirby-letsgo/oracles-decomp/tree/533860de269e0e9b338e12f3484ce8497367e6ed).
- Two actual native OS processes over loopback demonstrated P2 sword damage and
  block pushing, split travel, the actual shared Satchel grant once, owner-only
  dialogue, stable-owner rejoin, inventory equip/use, one shared ammo spend and
  return to an occupied room. Fresh startup validates each player's raw ROM and
  uses the original button-only new-file flow from blank SRAM, preserving the
  first controllable room/troupe prologue attachment and actual P2 movement.
- P2 starts offline. EOF or silence detection cancels held input and owned
  attacks before the next native tick, including busy-detach cases. A single
  retained offline room/capsule stays frozen without media and has bounded reuse.
  Rejoin retains the owner slot, refuses an occupied landing safely, then succeeds
  when legal. The host can enter without resurrecting absent P2. Format-2 saves
  persist offline presence on the exact build; fresh-execution disk load passed.
  Simultaneous defeat preserves the current authenticated connection and restores
  new own media and control.
- The protocol checks raw-ROM, native/controller/build identity, connection,
  epochs, ticks and generation, expires old input and bounds media buffers.
  Malformed/stale inputs and views have bounded rejection coverage. A valid
  handshake with disabled P2 actions failed an ordinary enemy-HP assertion;
  omitting fresh-execution hook preparation also failed an ordinary assertion.
  The malformed-group mutation produced a memory-safety fault, not a normal
  assertion control. Stale-input negative tests still need stronger delivery
  synchronization before each behavioral assertion.
- Nonlethal native HP comparisons now cover the earlier simultaneous-sword test
  gap. Literal 120 logic ticks/120 LCD frames cover the warm fixture's cadence
  gap. Integration checks assert room/group on each of twelve movement frames;
  the original standalone runtime helper retains its narrower endpoint proof.
- Final affected checks passed 52/52 with no skips or failures in 24.86 seconds,
  spanning fresh startup, CLI, native/runtime, rooms, views and clients. The
  earlier full 66-target run had 56 passes, 7 skips and 3 failures in 168.94 seconds.
  Its old save-format assertion was corrected; the other two match the original
  mods/app_menus baseline. The full suite was not rerun after narrow fixes.
  Existing upstream app compiler and SDL libdecor warnings remain. Save proof
  remains exact non-PIE Linux x86-64 little-endian, without generalized portability.
- Actual Wayland SDL renderer-readback PNGs were manually inspected for both
  bodies/tags and own inventory. The automated runner checks process completion
  and capture production only; it is not automated visual-regression coverage.
  Audio hardware and listening were not tested.

### Still pending

- The first controllable fresh prologue is not a campaign proof. The mandatory
  later dance/abduction, broader tools, bosses/essences, stock HUD/cutscenes and
  the other campaign admission gates above remain open. Season changes affecting
  a retained offline room are deliberately safely refused; season-aware refresh
  remains a later gate.
- Native Steam frontend/fault integration was still in progress at this stage;
  the later native Steam entry records its accepted bounded gate. This entry
  provided no actual Steam library/account/lobby/invite/Internet evidence. The
  public mock Steam helper remains separate. Consensual two-player joining
  remains a campaign gate.
- Engine source and generated manifests remain private and unexported. There is
  no playable package. Packaging/export and final whole-branch review remain
  open; the original public helpers do not provide the experimental clients.
  Co-op QA remains `none`.


## 2026-10-17: bounded native Steam frontend and fault observations

### Observed

- The native Steam frontend/common game protocol and deterministic-fault slice
  passed independent review, followed by two scoped lifecycle fixes and source
  origin audits. Engine adaptations and generated manifests remain private;
  the original public Steam production helper copies are unchanged.
- Two actual native processes use the reviewed Steam abstraction/private mock,
  real authoritative controller and common game protocol. Clean, 50 ms RTT,
  150 ms RTT with ±30 ms jitter and 2% seeded attempt loss, and a 3-second
  blackout cover P2 combat/block pushing, split/occupied return, one shared
  Satchel grant, owner dialogue, equip/use/ammo and stable-slot rejoin.
  Reliable transmission attempts retain packets and retry; fixed 32-packet
  staging, finite deadlines and backpressure bound recovery. These are mock
  gameplay results, not actual Internet or Steam relay results.
- Admission checks each supplied own ROM and exact engine/controller/frontend/
  add-on identities in a separate game handshake beyond lobby metadata. A
  temporary native-dispatch control kept valid admission and own media but
  disabled later P2 actions: the ordinary enemy-HP assertion failed (2 remained
  2). Source was restored. A transport handshake alone cannot satisfy that test.
- Steam initialization precedes the renderer. The separate native frontend has
  visible Host/Invite/Join/Leave/failure controls and callback-driven joining.
  Cold-start registration is absent: launch the app first, then accept the
  invite. Invite API dispatch does not establish a visible overlay; the owner
  chooses and sends any actual invite. Reconnect needs a fresh lobby/invite
  with the same retained campaign and stable slot; there is no host migration.
- Production-main regressions cover explicit host Leave/repeated cleanup and
  callback/explicit P2 activation, continued connection after Host-key input,
  real independent movement, and retained-session rehosting. Both Leave checks
  report Valgrind zero errors/zero suppressed; leak and uninitialized-value
  analysis were disabled and native fiber stack-switch warnings remain visible.
  This is not whole-engine memory-clean evidence.
- Retained busy save-on-exit now settles the owner's live session with neutral
  input and safe P2 detachment. Real transport loss plus a pending season
  transaction, and healthy shutdown with that transaction, both produce saves
  verified in fresh processes: retained positions/health, one season revision,
  otherwise preserved campaign, P2 offline and resumed P1 media. The arranged
  native fixture is targeted QA, not input-only campaign progress. Save proof
  retains the exact Linux x86-64 little-endian non-PIE build limits above.
- Final affected room/media/client/Steam/frontend checks passed 72/72, with no
  failures or skips, in 37.14 seconds after all fixes. Earlier pre-fix counts
  are historical. The original broad baseline was not rerun; its two known
  failures and seven skips remain qualified. The known libdecor warning remains.
- Normal Wayland/OpenGL frontend/mock capture measured about 60 host ticks and
  58 guest completed views per second over a short 1.5-second window. Under
  software/dummy rendering with 150 ms RTT/jitter/loss, the guest delivered
  about 18 completed views per second, a material limitation. Manual captures
  showed readable controls and P2 inventory; capture production is not
  unattended visual-regression, audio-hardware or listening proof.
- Separately, after explicit owner approval, a frozen reviewed native build
  launched against the real Steam library and logged successful Steam API
  initialization. This narrow observation does not validate the preceding mock
  gameplay on actual Steam. Initially, the owner reported that the game opened
  under native Wayland but pressing Invite did not show the overlay. A narrow
  launch inspection found the real Steam client library and OpenGL/EGL loaded,
  but no overlay renderer module or preload setting.
- A subsequent per-process SDL X11/OpenGL launch with the installed 64-bit Steam
  overlay preload loaded the overlay, GLX and X11 modules. The owner explicitly
  confirmed that Invite now opened the invite overlay. The reviewed invite guard
  permits dispatch only while the host is in a lobby with no partner, supporting
  real host-lobby/invite-dialog setup. This manual owner confirmation is separate
  from all automated mock native gameplay results. Both temporary test sessions
  ended cleanly; no account configuration or system settings changed, and original
  saves were untouched. The agent chose or sent no invite. No second account or
  Internet gameplay was tested.

### Still pending

- Actual invite delivery, two-account Internet play and consensual partner
  joining remain gates. The owner-confirmed XWayland overlay and host-lobby setup
  do not prove relay use or friend reachability. Native-Wayland overlay support
  remains unverified after its observed failure; the successful launch used a
  per-process XWayland configuration. The launch-first limitation above remains.
- The supported fresh input-only endpoint remains the opening troupe attachment;
  later dance/abduction, broader tools, bosses/essences, stock HUD/cutscenes,
  companions/transformations and the other campaign gates remain open. An
  attempted longer route is not accepted campaign evidence.
- Packaging/source publication and final whole-branch review remain open. No
  native source, generated manifest, SDK/mock library, ROM, game-derived assets
  or friend-ready package is exported here. Co-op QA remains `none`; this gate
  does not establish playable or full-campaign co-op.
