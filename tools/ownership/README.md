# Actor ownership with epochs

A small C++17 library that decides **which machine simulates which actor** in a co-op mod, and
rejects updates from machines that are not the owner any more. Two header and source files, no
dependencies, no network code, no clock. You feed it who is near what, it tells you what to send.

It exists because the same bug keeps appearing in every co-op mod that shares NPCs: two machines
both think they own an actor, or an old owner's message arrives late and wins. Epochs fix that.
Read the [playbook page](../../playbook/actor-ownership.md) for the why, and
[DESIGN.md](DESIGN.md) for every rule and edge case.

Ported from fnv-coop commit f57aa78 (public repo
[github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)), where it drives
NPC ownership for Fallout: New Vegas co-op. The game specific parts were left out; this is the
generic core.

## What you get

| Piece | Runs on | Job |
|---|---|---|
| `coop::OwnershipService` | the host only | Picks an owner per actor (nearest player, host preferred, hysteresis, companions follow their recruiter), runs handoffs, emits `OwnMsg` |
| `coop::OwnershipView` | every machine, host included | Applies `OwnMsg`, answers "may I publish this actor?" and "should I accept this update?" |
| `coop::OwnMsg` + `encode` / `decode` | the wire | Five message kinds (Grant, Transfer, Ack, Commit, Release) and an optional compact byte form |
| `coop::OwnershipFaults` | tests only | Switches that re-create known bugs, used by the mutation twins |

## Embed it

Copy `include/coop/` and `src/ownership.cpp` into your project, or use CMake:

```cmake
add_subdirectory(path/to/tools/ownership)      # tests are off when it is not the top level project
target_link_libraries(my_mod PRIVATE coop_ownership)
```

Host loop, in short:

```cpp
#include "coop/ownership.h"
using namespace coop;

OwnershipService svc;                    // host only
OwnershipView view(kHostPlayer);         // every machine has one, with its own slot number
svc.playerJoined(kHostPlayer);

OwnershipService::Out out;
svc.track(actorId, presence, nowMs, out);        // presence: who is near this actor right now
svc.tick(nowMs, out);                            // once per host tick, drives handoff timeouts
for (const OwnMsg& m : out) { view.apply(m); broadcast(m); }
```

Every machine, when it sends or receives actor updates:

```cpp
OwnerEpoch epoch;
if (view.mayPublish(actorId, epoch)) send(update, epoch);        // only the owner may publish

switch (view.check(actorId, fromPlayer, updateEpoch)) {          // on receive
    case UpdateVerdict::Accept: applyUpdate(); break;
    case UpdateVerdict::FutureEpoch: askHostForCurrentOwner(); break;
    default: drop(); break;                                      // stale, not owner, echo, unknown
}
```

When `view.handoffIncoming(id, epoch)` is true you are the new owner of a handoff: apply the last
state you got from the old owner, then send an `Ack` to the host. The playbook page walks through
the whole handshake.

Limits: at most 8 player slots, slot 0 is the host. The library is not thread safe (use one
instance per thread). Ids and epochs are 32 bit.

## How it is tested

```
python -I tools/ownership/run_tests.py
```

Builds with CMake (one job at a time, build folder under the system temp directory unless you
pass `--build-dir`), runs every test, then checks every mutation twin. It prints `ALL OK` only when
all tests pass and every twin is caught. It needs cmake and a C++17 compiler (Visual Studio 2022
on Windows).

What the last run covered: 30 tests (134 checks) and 7 mutation twins, all passing.

- Rule tests for the owner choice (host preferred, nearest, ties, hysteresis, hold time).
- Handoff tests: epochs and commit, timeout forces a commit, leaving mid handoff, snapshots for joiners.
- View tests: messages are idempotent and order independent, every update verdict, wire form
  round trip, truncated and garbage input.
- Scenario tests: random churn with lost acks and reordered, duplicated messages (one owner per
  epoch, epochs never go backwards, every view ends equal to the host), a replayed old assignment
  (the stale echo), leave and rejoin, no flapping, companions, and "the old owner stays the
  authority until Ack and Commit".

**Mutation twins.** Each twin turns on one fault from `OwnershipFaults` and runs the scenario
that guards it. The twin passes only if the scenario passes without the fault and fails with it.

| Twin | Fault it switches on |
|---|---|
| `own_no_epoch` | changes keep the old epoch |
| `own_stale_epoch` | views accept older epochs and older messages |
| `own_adopt_echo` | a replayed older assignment overwrites a newer one |
| `own_transfer_no_ack` | handoff commits without waiting for the new owner |
| `own_no_hysteresis` | ownership flaps with distance |
| `own_follower_ignored` | companions are owned by whoever is nearest |
| `own_accept_non_owner` | views accept updates from any player |

## Changes from the original

- Renamed to the `coop` namespace and detached from the fnv-coop wire format, types and session
  code. The wire form is now `encode` / `decode` in this library.
- The three cluster-level scenarios (no flapping, follower, handoff keeps the last state) were
  rewritten to run on the service and views directly, so the library tests need no fake world.
  The original cluster versions live on in [../fakeworld](../fakeworld/README.md).
- `OwnershipView::check` and the service logic are unchanged.

License: MIT, like the rest of this repository.
