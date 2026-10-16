# Oracle of Seasons Co-op Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify two interactive Links with independent exploration, shared progression, online host/join, and an offscreen partner marker.

**Architecture:** A session host owns the campaign and each occupied room. Native player contexts separate input/action/health/equipment, while each room advances its world exactly once. Presentation and transport consume authoritative state and never substitute a ghost or a second independent world for real same-room gameplay.

**Tech Stack:** C11/C++17 native port, SDL3 3.4.16, CMake/Ninja, Python orchestration, Linux x86-64.

**Spec:** `docs/superpowers/specs/2026-10-16-oracle-of-seasons-coop-design.md`

## Global Constraints

- Pin native base `533860de269e0e9b338e12f3484ce8497367e6ed` and USA/Australia revision-0 ROM.
- Two Linux players; independent room travel and cameras; shared permanent progression.
- Work only in isolated source copies and new co-op profiles, never original installs/saves.
- Original game source, ROMs, derived state, screenshots and personal paths stay in ignored `local/`.
- Track original add-on code, source-patching scripts and clean documentation in the playbook; do not publish a copy of the decompilation.
- Fresh game-client evidence is required for every gameplay claim; bots live inside native code.
- The approved edge marker uses a blue P2 or green P1 Link portrait, outward chevron and P1/P2 label; viewport clamping excludes the HUD.
- User approved the design, project-local source/dependency builds, claim issue and repository workflow. Continue without another plan approval prompt.
- Repository instructions select subsystem workers. Only one worker may mutate the native engine checkout at a time; independent original add-on modules may be built in separate directories.

## Review Focus

- Idle player: another player's input/action/hit must not move or injure the idle player.
- Simultaneous item claims/spends: one authoritative transaction, no duplication or negative stock.
- Reunification: entering an occupied room preserves its enemy health, blocks and active scripts.
- Unknown/different coordinate spaces: marker hides or labels area instead of inventing a direction.
- Disconnect/load/version mismatch: do not overwrite a save or leave a stale attacking player behind.

## File and artifact ownership

Public original add-on files belong under `games/oracle-of-seasons/coop/`; `NOTES.md` and the game write-up report actual evidence. Private native edits live under the isolated native worktree and are exported only as original injection/patching code with no copied upstream function bodies. Build logs, fixtures, source diffs and per-task reports live under `local/`.

### Task 1: Establish native baseline and occupied-room isolation seam

**Files:** private native `CMakeLists.txt`, new `tests/test_coop_runtime.c`, and original harness/build scripts under `games/oracle-of-seasons/coop/` as needed.

**Interfaces:** consume the upstream native ROM/TAS/bootstrap APIs; produce a fixture manifest with ROM/build/state hashes and an explicit PASS/FAIL result for two independently stepped runtime instances. A fixture used by later tasks identifies its room and player position rather than assuming boot reached gameplay.

- [ ] Configure/build the pinned source using locally verified SDL archive; record the complete baseline suite, including every skipped test and missing ROM.
- [ ] Write a native test that loads two separate real Seasons instances from the same supported boot state, gives them different input streams, alternates complete frame steps, and compares each to a separately stepped reference instance. Expected equality per stream, with different player outcomes between streams.
- [ ] Run it before adding runtime isolation changes; if it fails, identify the exact shared state, write the narrow correction, then rerun. Do not declare a co-op feature from this test.
- [ ] Record the native fixture room/coordinates and test output; commit only the task's original code and private native changes in their respective repositories.

### Task 2: Two interactive native players in one room

**Files:** new private `src/coop/player.h`, `src/coop/player.c`, `tests/test_coop_players.c`; controlled changes to `src/core/gb.h`, memory access/player aliases, special-object update/draw, item ownership, enemy targeting/collision, and CMake wiring. Worker must name exact touched owners in its report before broadening edits.

**Interfaces:** `bool coop_enable(GB *gb)`; `void coop_set_input(GB *gb, unsigned player, uint8_t joy)`; `bool coop_player_state(const GB *gb, unsigned player, CoopPlayerState *out)`. `CoopPlayerState` exposes room key, position, health, equipment and action state. All original single-player entry points remain behaviorally compatible when co-op is disabled.

- [ ] Write and run failing native tests for independent opposite-direction movement and an explicitly idle player, using original gameplay movement/collision code.
- [ ] Implement full player contexts including action/input scratch, health/equipment, and owned weapon identity. Do not use occupied RAM beside the existing Link as free storage.
- [ ] Extend world consumers to resolve both players: update world objects once, target/collide enemies with the correct player, and draw both interactive players.
- [ ] Test player 2 pushing a real room block and damaging a real enemy once, player-1 idle negative control, shared-wall collisions, independent damage, and unchanged world update count. Disable player-2 dispatch once and prove the test fails.
- [ ] Run baseline regressions and generate native screenshots. If the engine seam is invalid, preserve the failing test and report the exact contradiction; a drawn puppet cannot satisfy this task.
- [ ] Self-review and commit the coherent native change; export original integration code without copying upstream implementation bodies into the public playbook.

### Task 3: Shared campaign and independent occupied rooms

**Files:** new private/native add-on `src/coop/session.h`, `session.c`, `campaign.h`, `campaign.c`, `tests/test_coop_rooms.c`; room transition, save/treasure/item and script integration owners identified by Task 2.

**Interfaces:** session owns two stable player IDs and up to two occupied room runtimes. `coop_session_input(session, player, joy)` routes input; `coop_session_step(session)` advances each occupied room once; `coop_session_player(session, player, out)` supplies views. Campaign events carry stable event/room/object IDs and are ordered/idempotent. No raw-RAM merge API is permitted.

- [ ] Write a failing real-game split/rejoin test: player 2 leaves, player 1 continues pushing/fighting, player 2 obtains a permanent ability, both acquire the unlock, and player 2 returns to unchanged occupied-room enemy/block state.
- [ ] Implement authoritative room ownership and safe player migration, preserving occupied source and destination rooms. Last-player departure uses documented original transient reset semantics.
- [ ] Implement typed claims/unlocks/resource spends/story events and a versioned co-op save. Test concurrent chest/key claims, nonnegative shared resource spending and no guest single-player save writes.
- [ ] Test separate equipment/health, ordinary dialogue/menu isolation, season terrain changes, locked encounter travel and per-player death. Test at least 15 mixed transitions followed by canonical-state checks.
- [ ] Verify reunification and ability events with negative controls, run baseline/native player tests, then self-review and commit.

### Task 4: Partner edge marker

**Files:** original add-on `games/oracle-of-seasons/coop/marker.h`, `marker.c`, `tests/test_marker.c`; integration into private native view rendering after session/view APIs exist.

**Interfaces:** `bool coop_marker_layout(const CoopMarkerView *view, const CoopPartner *partner, CoopMarker *out)` is pure layout in native viewport coordinates; renderer consumes its portrait/chevron/label placement. Inputs distinguish same coordinate space, different area, visibility, connection and known location.

- [ ] Write failing tests with literal expected positions: partner east/west/north/south/diagonal; marker fully inside 160x144 viewport with top 16-pixel HUD and 2-pixel padding; both P1/P2 viewpoints; hide when visible, disconnected or unknown.
- [ ] Implement ray/rectangle clamping accounting for the full marker dimensions. Different coordinate spaces produce an area-labelled marker with no fabricated bearing. Offscreen in the same room still yields a marker; room equality alone must not hide it.
- [ ] Render original small pixel art blue-Link portrait plus label and outward chevron into a native game-generated image; compare visually against the approved mockup held under `local/`.
- [ ] Integrate small P1/P2 labels for visible actors, as required by the approved design, alongside the offscreen marker.
- [ ] Run focused tests and add-on suite; commit original module and integrate only after room identity and view transforms are reliable.

### Task 5: Two real clients, saves and playable package

**Files:** add-on network/session protocol files and platform Host/Join controls, native bot scenarios, `tests/run_coop.py`, launcher/build scripts and a clean game README.

**Interfaces:** versioned handshake verifies engine/add-on protocol and ROM identity. Client inputs are sequenced by stable player ID; host supplies authoritative snapshots/events and player views. Rendering transport choice is builder-owned after Tasks 2/3 prove game semantics. Host owns saves; late join/rejoin restores the correct slot.

- [ ] Write a failing two-process loopback test for a genuine shared-world action; reject protocol mismatch and absent second-player mode.
- [ ] Implement host/join and authoritative transport, starting on loopback/LAN. Keep all test listeners on loopback, with explicit binding only when the user chooses Host for an actual friend session.
- [ ] Run two native bot clients through walk/fight/puzzle/split-room/ability/rejoin/save/load/disconnect sequences. Add in-process delay/jitter/loss/blackout injection without changing host networking.
- [ ] Test 50ms RTT clean, 150ms RTT plus 30ms jitter and 2 percent packet loss, and a 3-second interruption. Check no duplicate claims, correct room authority, recovery or a clear safe disconnect, and graceful process exit.
- [ ] Package a separately named Linux app and one-command launchers requiring each user's own ROM. Include no test saves, ROMs, extracted assets or private data. Select approved friend reachability path (existing overlay or relay) and verify it before saying online play works.
- [ ] Review whole branch, run complete applicable suites and a campaign bot test before claiming full-campaign coverage. Report the exact reached level if any campaign gate fails; keep unresolved gates open.
- [ ] Update the directory/write-up with actual QA evidence, show the contribution diff, and create a draft PR only under the already-authorized repository workflow after its contents are reviewable. Never mark ready or merge on the owner's behalf.
