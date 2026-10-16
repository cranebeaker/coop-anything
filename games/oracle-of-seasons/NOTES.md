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

### Proposed, not implemented

- Two players, each with a camera and independent room travel. Shared permanent
  upgrades and campaign progress; independently chosen equipment and health.
- One authoritative host maintains campaign progress and room ownership. A room's
  enemies and puzzles have one owner even when both players occupy it.
- First feasibility gate is two genuine interactive players in one room. Subsequent
  gates cover splitting rooms, shared progression and reuniting without resets.
- The detailed proposed design is in the dated design document under
  `docs/superpowers/specs/`.

### Unknown / untested

- All co-op gameplay, campaign coverage, same-room player contexts, room runtime
  isolation, migration, and networking remain unimplemented and untested.
- Every singleton consumer and runtime-global state has not yet been audited.
- SDK/dependency build and network reachability between two actual PCs are untested.
- Seasons/boss/script policies require implementation audits against the original
  game's affected-room semantics; shared progression cannot mean copying all RAM.
- No owner account was used and nothing was posted. A proposed claim issue and
  contribution diff must be shown to the owner before upstream publication.
