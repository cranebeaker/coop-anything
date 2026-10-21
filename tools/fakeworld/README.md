# Fake world: test your sync without the game

An example of a pattern, not a drop-in library: **model the parts of your game world that you
sync as plain in-memory data, run several copies of it on a virtual clock over a lossy fake
network, and check that they stay equal.** No game, no window, no real network. A full run of
the 63 tests below takes about a second once it is built.

This folder is one finished example. It models actors, cells, containers, items, doors, spawn sites,
simple combat and quest stages, uses the [ownership library](../ownership/README.md) to decide who
simulates what, and has a test for every classic co-op bug. Copy it, rename the types, and reshape
the world to match your game. Read the [playbook page](../../playbook/fake-world-testing.md) for
the method.

Ported from fnv-coop commit f57aa78 (public repo
[github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)), where it is the test
bed for a Fallout: New Vegas co-op sync layer. This copy was made standalone: the original's
serializer, transport, RNG and session code were replaced by small local ones.

## What is in it

| File | What it is |
|---|---|
| `include/fakeworld/types.h`, `world.h`, `src/world.cpp` | The world one machine holds: actors, containers, items, doors, spawn sites, players, quests. Deterministic build from a seed, effects, digests per category and per key, save and load, repair import and export, duplicate and conservation helpers |
| `effect.h`, `src/effect.cpp` | `Ref` (a name in a content pack) and a small set of story effects (quest stage, kill, move, enable, door state, lock, spawn, companion owner) |
| `messages.h`, `src/messages.cpp` | The wire messages of the live layer and the envelope codec |
| `machine.h`, `src/machine_*.cpp` | One simulated peer: world replica, ownership view (the host also runs the service), reliable link over a lossy datagram transport, NPC AI for the actors it owns, combat, loot, containers, spawn arbitration, digests and repair, full state for joiners, host save and load |
| `cluster.h`, `src/cluster.cpp` | N machines on one fake network with a virtual clock, join and leave, host save and load, the invariant oracles, and a replay of a recorded run |
| `net.h`, `rng.h`, `wire.h` | A tiny lossy link simulator (latency, jitter, loss, duplicates, one bit flip), a seeded RNG, and a bounds checked serializer. Swap these for your own |
| `faults.h` | Test only switches that re-create classic bugs (the mutation twins use them) |
| `tests/` | Scenario tests, world and codec tests, and the twins |

## Embed it

It is meant to be read and copied. To build it as is:

```cmake
add_subdirectory(path/to/tools/fakeworld)            # also pulls in ../ownership
target_link_libraries(my_tests PRIVATE coop_fakeworld)
```

Smallest useful run:

```cpp
#include "fakeworld/cluster.h"
using namespace coop::fakeworld;

ClusterConfig cfg;
cfg.seed = 1; cfg.players = 3;                       // slot 0 is the host
cfg.link.minLatencyMs = 20; cfg.link.maxLatencyMs = 70; cfg.link.loss = 0.08;
cfg.machine.spec.seed = 1;                           // same seed, same world on every machine
Cluster c(cfg);
c.run(2000);                                         // two virtual seconds
c.machine(1).move(someCell, somePos);                // a local player action on machine 1
c.machine(2).attackActor(actorId, 45);
bool quiet = c.quiesce();                            // stop the AI, wait until nothing changes
auto always = c.checkAlways();                       // invariants that hold at any time
auto settled = c.checkQuiescent();                   // equality with the host after quiescence
```

To adapt it to your game, follow the steps in the playbook page: replace the world types with
yours, keep the machine structure (ownership, link, digests, repair), and add one oracle and one
twin for every bug you have met.

## How it is tested

```
python -I tools/fakeworld/run_tests.py
```

Builds with CMake (one job at a time, build folder under the system temp directory unless you pass
`--build-dir`), runs every test, then checks every mutation twin. It prints `ALL OK` only when all
tests pass and every twin is caught. It needs cmake and a C++17 compiler (Visual Studio 2022 on
Windows).

What the last run covered: 63 tests (483 checks) and 18 mutation twins, all passing.

**Scenario tests** (34): join and sync with 2, 3 and 4 players (clean and on a lossy, duplicating,
corrupting link); a fight; ownership handoff when the host walks away; guest to guest handoff that
keeps the old owner's last state; no flapping between equally near players; a companion that
follows its recruiter; a loot race (one winner, losers roll back); an encounter that spawns once;
container lock, take, put and close with item conservation; doors, locks and teleports; a story NPC
that combat cannot kill but a story effect can; quest stage merge and re-delivered effects; host
save and load with a guest rejoin; silent corruption that the digests find and repair; a repeated
drop request that makes one item; forged and misrouted hits; a late joiner that gets the ownership
table; four random sessions of 12 virtual seconds with joins, leaves, save and load and every kind
of action; same seed gives the same world; a recorded run replays to identical digests; garbage
frames; reliable link ordering and de-duplication; guest rejoin.

**World and codec tests** (11): deterministic build, cells and proximity loading, save and load, repair
of a single key, effects (stage is a maximum, corpses, idempotent spawn, sticky object flags), effect
wire round trip, duplicate detection, the codec for every message type, garbage and bit flips, and
the action codec.

**The oracles** the cluster runs after each scenario: one owner per epoch, one AI driver per actor, no
stale update accepted, nothing applied twice, no duplicate actors or items, every machine equal to the
host after quiescence (digests and exact actor state), the same ownership table everywhere, items
conserved, and no repairs after quiescence.

**Mutation twins.** Each twin turns on one fault and runs the scenario that guards it. The twin
passes only if the scenario passes without the fault and fails with it.

| Twin | Fault it switches on |
|---|---|
| `own_transfer_no_ack`, `own_no_hysteresis`, `own_follower_ignored` | the ownership library's handoff, flapping and follower faults, seen through the whole cluster |
| `no_dedupe` | links and ids stop dropping duplicates, re-delivered messages apply twice |
| `hit_no_validation` | the victim's owner applies any hit, forged or not |
| `hit_by_non_owner` | a machine that does not own the actor applies a hit aimed at it |
| `pickup_no_arbitration` | the host grants every pickup claim |
| `pickup_no_rollback` | a pickup loser keeps the item |
| `spawn_no_arbitration` | the host spawns an encounter once per roller |
| `loot_per_machine` | every machine rolls corpse loot itself |
| `pick_per_machine` | every machine makes its own leveled pick |
| `container_no_lock` | a container can be open for two players at once |
| `named_killable` | combat can kill a story NPC |
| `no_session_epoch` | messages from before a host load apply after it |
| `heal_disabled` | digest mismatches are found but never repaired |
| `drop_no_idempotency` | a re-sent drop request makes a second item |
| `join_no_ownership` | the full state for a joiner leaves out the ownership table |
| `owner_keeps_publishing` | a former owner keeps publishing (together with a view that accepts old epochs) |

## Changes from the original

- Detached from the fnv-coop core: the serializer (`wire.h`, without the shared string table), the
  link simulator (`net.h`, without bandwidth limits and the pluggable shaper), the RNG and the
  effect type are local, and the ownership code comes from `../ownership`.
- Names are neutral: namespace `coop::fakeworld`, `Ref` instead of a form reference, made up content
  pack and ids. Nothing in it is game data.
- The effect list is the small subset the model can show. Story sessions, quest policy, effect
  capture from a game and the real transport are not part of this example.
- Two scenarios needed fixing to pass in this port:
  - `handoffKeepsLastState` counted final states at the second guest from the start, so an earlier
    handoff (host to guest) satisfied it and the `own_transfer_no_ack` twin stayed green. It now counts
    only what arrives after the second handoff begins, and the twin catches the fault.
  - `fightScenario` on a lossy link now allows a few digest repairs (at most 8), like the random
    sessions do. A moving actor can sit across a position bucket edge for two reports in a row on a
    lossy link, and the repair path then does its job.

License: MIT, like the rest of this repository.
