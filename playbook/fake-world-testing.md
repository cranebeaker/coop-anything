# Fake world testing: test your sync without the game

Most co-op sync bugs are not engine bugs. They are logic bugs: a message applied twice, an old owner
still talking, two players taking the same item, a joiner missing half the state. You can find all of
those without starting the game. Model the part of the world you sync as plain data, run several
copies of it on a virtual clock over a fake lossy network, and check that the copies agree.

Example code: [tools/fakeworld](../tools/fakeworld/README.md). It uses
[tools/ownership](../tools/ownership/README.md) ([actor-ownership.md](actor-ownership.md)). Run it with
`python -I tools/fakeworld/run_tests.py`. Related: [fuzzing-coop-sync.md](fuzzing-coop-sync.md),
[network-chaos-testing.md](network-chaos-testing.md), [automated-qa.md](automated-qa.md).

## Why a fake world

- **Fast.** The whole example suite (63 tests, including four random sessions of 12 virtual seconds)
  runs in about a second. You can run it after every edit, and so can every agent working in parallel.
- **Deterministic.** Same seed, same world, same run. A failure is a seed and a test name, not a story.
- **No game needed.** Nothing to launch, no window, no save to corrupt, no owner's PC to keep free.
- **Faults on demand.** Loss, duplicates, reordering, corruption, players leaving and joining mid fight
  are a parameter, not luck.
- **Honest oracles.** The fake world knows the truth, so a check like "no item was created or lost" is
  one line.

It does not replace testing in the real game. It finds the logic bugs first and cheaply, so the
in-game bot tests ([automated-qa.md](automated-qa.md)) only have to find the engine surprises.

## What goes in the model

Only what you sync. For the example that is: cells and proximity loading, actors with an owner, health
and death, containers and items on the ground, doors and locks, spawn sites with respawn, players with
inventories, and quest stages. For your game it might be a tile map, a dungeon room list, a flag table.
Keep the data plain: ids, numbers, small maps. Anything you would put in a save file belongs in it.

## The ingredients

1. **A world built from a seed.** One call makes the same static world on every machine, like loading
   the same game data. Use a small seeded RNG you own, never the platform's.
2. **A virtual clock.** Machines never read a wall clock. The harness calls `tick(nowMs)`. Time is a
   number you can advance 10 ms at a time, so 12 seconds of play takes milliseconds.
3. **A lossy link.** Latency with jitter (jitter causes reordering), loss, duplicates, and a flipped
   bit. Seeded, so repeatable. Put your own reliable layer (sequence numbers, acks, retransmit, CRC)
   on top, exactly as your real transport needs it.
4. **A machine.** One peer: its own copy of the world, the protocol handlers, and a cheap NPC AI for
   the actors it owns. Local player actions go through one entry point (`doAction`) so a list of
   actions is a complete, replayable script.
5. **A cluster.** N machines, the network, join and leave, host save and load.
6. **Digests.** A hash of the world per category and per key, with floats quantized into buckets so
   rounding does not matter. Machines compare digests; a mismatch that stays is repaired by sending
   the host's copy of just those keys. This is your safety net in the game too.
7. **Oracles.** Checks that run during and after a scenario (below).
8. **Mutation twins.** Switches that re-create known bugs (below).
9. **A recorder.** Capture every input a machine received (ticks, actions, messages) and replay it into
   fresh machines with no network. A failing run becomes a regression test and shrinks easily.

## Oracles that earn their keep

Always valid (check after every step):

- one owner per actor per epoch, and one machine driving each actor's AI;
- nothing stale accepted: every accepted update came from the owner at the current epoch;
- nothing applied twice: ids that were already applied are counted, and the count must stay zero;
- no duplicate actors from one spawn site, no two items from one drop request.

Valid after the cluster is quiet (`quiesce`: AI frozen, nothing in flight, digests stable):

- every machine equals the host: digests and exact actor state;
- the same ownership table everywhere;
- item conservation: ground plus containers plus every inventory equals what the world started with;
- no digest repairs were needed. A repair is a symptom that something diverged and was patched. Count
  them, and on a very lossy link allow a small number.

## Scenarios

Each scenario is a function that builds a cluster, makes something happen, settles, and returns
true only if every invariant held. Write one for every bug class you can name:

- two players grab the same item at the same instant (one winner, the loser's copy rolled back);
- two players open the same container (one lock holder, items conserved through take, put, close);
- three players load the same cell (the encounter appears once);
- the host walks away mid fight (the actor changes owner without losing health or position);
- a story script kills an actor that combat must not kill;
- a message is delivered twice, or an old one arrives late (it must not apply again);
- a guest leaves and rejoins, the host loads a save;
- silent corruption on one machine (the digests must find it and repair it);
- a random session: random moves, attacks, pickups, container use, joins, leaves, saves, on a clean
  and a lossy link, with the always valid checks after every step.

## Mutation twins

A test you have never seen fail may be testing nothing. A **mutation twin** is a test that proves the
guard works: switch on one fault that re-creates a known bug, run the same scenario, and require it to
**fail**. The twin only passes if the scenario passes without the fault and fails with it. Our run
script prints `ALL OK` only when every twin is caught.

Cheap to build: each fault is one `if` in the code under test, behind a flag that is off in production
(`hitNoValidation`, `pickupNoArbitration`, `noDedupe`, `noEpochBump`...).

A real catch from the example port: the twin for "handoff commits without waiting for the new owner"
stayed green. The scenario asserted that the new owner had received at least one final state, but a
different, earlier handoff had already delivered one to the same machine. The scenario was weak, not
the code. The fix was to count only what arrives after the handoff under test begins, and the twin went
red as it should. Checking that the scenario passes first, then fails with the fault, is what exposes
that kind of hole.

## Build your own, in this order

1. List the state you sync and the messages that carry it. That is your world and your wire format.
2. Write the world as plain data with a seeded build, save and load, and a digest. Test those alone.
3. Write the lossy link and a tiny reliable layer. Test ordering and de-duplication alone.
4. Write the machine, host first: apply a message, publish a state. Add one feature at a time.
5. Add the cluster and the always valid oracles before the second feature.
6. For every feature add a scenario, then a fault flag, then the twin.
7. Add the random session last. It finds the interactions you did not think of.
8. When a real playtest or fuzzer finds a bug, reproduce it here first, then fix, then keep the test.

You can copy the example and change the types, or keep only the shape. The code is a starting point.
The message list and oracles will be your own.

## Limits

A model cannot tell you how the engine behaves: when a hook fires, what a reload does to a reference,
which frame an animation starts. Anything you learn about the engine from reverse engineering belongs
in the model as a rule, and anything the model cannot know needs a test in the real game. Use the fake
world for protocol and state logic, a fuzzer ([fuzzing-coop-sync.md](fuzzing-coop-sync.md)) and chaos
links ([network-chaos-testing.md](network-chaos-testing.md)) for breadth, and bots in real clients
([automated-qa.md](automated-qa.md)) for the truth.
