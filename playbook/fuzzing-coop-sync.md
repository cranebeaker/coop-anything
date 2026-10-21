# Fuzzing co-op sync: find the desync before your friends do

Most co-op sync bugs are not "the code is wrong". They are "two things happened at once". Two players loot the same
chest. A packet arrives twice. The owner of an enemy changes while the old owner is still sending. The host loads a
save while a request is on the wire. You will not hit these by playing, but a computer can try thousands of them a
minute against your sync logic, with no game running.

The tool is in [tools/fuzz/](../tools/fuzz/README.md). This page is the method. The step by step for wrapping your
own code is [tools/fuzz/GUIDE.md](../tools/fuzz/GUIDE.md), and the bug list is
[tools/fuzz/BUG-CLASSES.md](../tools/fuzz/BUG-CLASSES.md).

## 1. What a fuzzer for sync is

1. **A fake session.** 2 to 4 copies of your sync core in one process, a virtual clock, and a lossy network between
   them (delay, jitter, loss, duplicates, corruption).
2. **A random plan, from a seed.** A list of timed player actions: move, hit, pick up, drop, spawn an encounter, open
   a door, join, leave, host save and load, make the link worse, replay an old message. The same seed gives the same
   plan on every machine.
3. **Heal, wait, compare.** After the last action the link becomes perfect, everything freezes, and the harness waits
   until nothing is in flight. Then it compares what every machine believes.
4. **Oracles.** Rules that must hold, checked on the final state and on a log of everything each machine applied.
5. **A shrinker.** When a rule breaks, the harness removes actions until the smallest plan that still breaks it is
   left. A failure of 400 actions usually becomes 3 to 10. That is the bug report.

Because the whole thing is virtual time in one process, a session takes milliseconds, a failure replays exactly,
and an agent can run it in a loop after every change.

## 2. The rules worth checking

| Rule | Plain meaning |
|---|---|
| Converge | When quiet, all machines show the same world as the host |
| Nothing applied twice | A duplicated packet or a resend does not change anything a second time |
| One owner per actor per epoch | Two machines never both simulate the same actor in the same epoch, and epochs only go up |
| Nothing stale applied | Old epochs, old sequence numbers, non-owners and requests from before a save load are rejected |
| No echo | A machine does not adopt the echo of its own update |
| No duplicates | No two actors or items with the same identity (the same spawn site, the same original item) |
| Repairs settle | Once everything is quiet, resync and repair counters stay at zero |
| Quiet means quiet | After the link heals, traffic stops by itself |

These are the rules the fuzzer is built around. The catalogue, with how each bug looks in a game, is in
[BUG-CLASSES.md](../tools/fuzz/BUG-CLASSES.md). One of them is documented in public: a stale assignment echo that made
an owner bring a dead enemy back to life and then re-discover it as a new one, over and over, until hundreds of copies
crashed the game. It was found and fixed in Skyrim Together Reborn's code base,
[TiltedEvolution pull request 906](https://github.com/tiltedphoques/TiltedEvolution/pull/906).

## 3. How to use it on a new game

1. **Make the core runnable N times in one process.** No globals, injected time, injected transport, seeded
   randomness. If it is welded to the engine, split the engine calls behind a small interface and give the fuzzer a fake
   world to talk to. This is the biggest step, and it pays off for every other test too.
2. **Wrap it as a target.** Copy the 190-line skeleton, map the generic ops onto your game, report every message a
   machine applies or rejects, and answer `view`, `digest`, `quiescent` and `repairs`.
3. **Fix the adapter first.** Early failures are mostly the harness: a `quiescent()` that is too eager, a message id that
   is not stable, state in `view` that is allowed to differ. Only trust the oracles after 200 clean sessions.
4. **Prove the fuzzer can fail on your target.** Put a past bug back behind a switch (a mutation twin) and require the
   run to fail with the right oracle. A fuzzer that has never been seen failing has told you nothing.
5. **Guard coverage.** Count what the sessions caused: ownership transfers, loot races lost, spawn races lost, host
   loads, replayed stale messages, rejected duplicates. If a counter stays at zero, that defence was never tested.
6. **Run it often.** Two minutes after every change, a long run at night. Save each fixed bug as a shrunk
   regression scenario and replay it in your tests.

## 4. Lessons

- **Shrink before you read.** The minimized scenario is the bug report. Read the 4 ops, not the 400.
- **A convergence failure is usually a symptom.** The tool reports the most specific broken rule first (a double apply
  or a stale apply) and `converge` last.
- **Log the rejections too.** A correct reject is evidence that a defence ran. The coverage line of a clean run shows
  which defences the fuzzer reached.
- **Some twins need a coincidence.** A kill and an assignment crossing on the wire is rare, so that twin needs more
  seeds than the others. Give a rare twin its own seed count rather than loosening the oracle.
- **Do not fix the oracle to make a seed pass.** If a clean target fails, either the adapter lies or the core has a bug.
  Decide which before you touch anything.
- **It does not replace the game.** The fuzzer sees only what the target reports, and it does not test the engine hooks
  or the real network stack. Pair it with in-game bots ([automated-qa.md](automated-qa.md)) and, for real sockets, a
  bad-link relay ([network-chaos-testing.md](network-chaos-testing.md)).

## 5. Run it

```
python -I tools/fuzz/run_tests.py                          # build and test everything, prints ALL OK
python -I tools/fuzz/run.py --minutes 2 --players 4        # fuzz for two minutes with 4 players
python -I tools/fuzz/run.py --bug two_owners --count 100   # a switched-on bug must make this fail
python -I tools/fuzz/run.py --replay tools/fuzz/out/fail_seed6_dup_thing.min.scenario.json
```
