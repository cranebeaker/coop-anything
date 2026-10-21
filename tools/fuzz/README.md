# Co-op fuzzer: random sessions and invariant oracles for any sync core

A small C++17 framework that tests the **sync logic** of a co-op mod without the game. It runs 2 to 4 copies of your
sync core in one process, joins them with a lossy fake network (delay, jitter, loss, duplicates, corruption), plays a
random session at them (moves, hits, loot races, spawns, doors, joins, leaves, host save and load, bad links, replayed old
messages), heals the network, waits for quiet, and checks the result with **oracles**:

| Oracle | What must hold |
|---|---|
| `converge` | After quiet, every machine has the same things with the same values as the host |
| `double_apply` | Nothing is applied twice |
| `multi_owner` | One owner per actor per ownership epoch, and epochs only grow |
| `stale_applied` | Nothing out of date is applied (old epoch, old sequence, non-owner, request from before a load) |
| `echo_adopted` | A machine never adopts the echo of its own update |
| `dup_thing` | No duplicate actors or items (same net id, or same identity twice) |
| `repairs_after_quiescence` | Repairs per minute are 0 once everything is quiet |
| `not_quiescent` | Traffic really stops after the link heals |

When one breaks, the fuzzer prints the failing **seed**, shrinks the session to a few ops, and writes a replayable
scenario file and a JSON report.

It is a **template plus a guide**. You implement one interface (`ITarget`) that wraps your own sync core. Two complete
targets are included, and neither needs a game or a sync core:

- `example/skeleton_target.cpp` (191 lines): the smallest target. Start here.
- `example/example_target.cpp`: a full host-authoritative world (actors with ownership epochs, items, doors, encounters,
  host save and load, reliable streams, digests, repair) with **12 mutation twins**, one per known bug class.

Documents: [GUIDE.md](GUIDE.md) (wrap your own core, step by step), [BUG-CLASSES.md](BUG-CLASSES.md) (the bug classes,
what they look like in a game, which oracle catches them), and the playbook page
[fuzzing-coop-sync.md](../../playbook/fuzzing-coop-sync.md).

## Quick start

You need `cmake` and a C++17 compiler (Visual Studio 2022 on Windows) and Python 3.

```
python -I tools/fuzz/run_tests.py                      # build, run all tests, prints ALL OK
python -I tools/fuzz/run.py --minutes 2 --players 4    # fuzz the example target for two minutes with 4 players
python -I tools/fuzz/run.py --bug two_owners --count 100   # switch a bug on: the run must FAIL, shrink and write a report
python -I tools/fuzz/run.py --target skeleton --count 200
```

The first call builds into `tools/fuzz/build` (git ignores it). A clean run ends with `RESULT: ALL OK`. A broken run
exits 1 and prints, for example:

```
FAIL seed 6 (2 machines): dup_thing: m0 has two copies of identity 2 (...)
  shrunk 377 -> 4 ops, 2 machines, 5799 ms in 94 runs; still fails: dup_thing
  minimized scenario: tools/fuzz/out/fail_seed6_dup_thing.min.scenario.json  (hash 680756bd)
  report:             tools/fuzz/out/fail_seed6_dup_thing.min.report.json
  replay: coopfuzz --replay tools/fuzz/out/fail_seed6_dup_thing.min.scenario.json
```

(Through `run.py` the paths are absolute. Failures go to `tools/fuzz/out`, which git ignores; change it with `--out`.)

Options of `run.py` (passed on to the executable): `--minutes M` or `--count K`, `--seed S`, `--players 2..4` (0 or
absent varies it per seed), `--duration MS`, `--profile lan|good_wifi|bad_wifi|mobile|hell`, `--target NAME`, `--bug NAME`
(repeatable), `--replay FILE`, `--fixture BUG`, `--out DIR`, `--no-shrink`, `--max-fail N`, `--list-bugs`,
`--list-targets`, `--quiet`; and for the driver `--no-build`, `--rebuild`. Exit codes: 0 all invariants held, 1 an
invariant broke, 2 usage or build error.

## Use it on your own game

Read [GUIDE.md](GUIDE.md). In short: copy `example/skeleton_target.cpp`, replace the stand-in core with calls into yours,
log what each machine applies, answer the oracle questions (`view`, `digest`, `quiescent`, `repairs`), register the
target, build with `-DCOOPFUZZ_TARGET_SOURCES="my_target.cpp"`, run it clean, then switch past bugs back on as twins and
check that the oracles catch them.

## Layout

| Path | What |
|---|---|
| `include/coopfuzz/fuzz.h` | the whole public API: ops, scenario, `ITarget`, `TargetBase`, registry, oracles, run, shrink, report |
| `include/coopfuzz/net.h`, `src/net.cpp` | the built-in lossy link and the named link presets |
| `include/coopfuzz/rng.h` | the seeded random number generator (same seed, same session on every platform) |
| `src/scenario.cpp` | the session generator and the scenario JSON |
| `src/oracles.cpp` | the oracles |
| `src/runner.cpp` | run, quiescence wait, shrinker, JSON report |
| `src/main.cpp` | the command line |
| `example/` | the two targets |
| `tests/` | the self tests (own tiny harness, no dependencies) |
| `run.py`, `run_tests.py` | driver and test runner |

## How it is tested

`python -I tools/fuzz/run_tests.py` builds with cmake (one compile job at a time) and prints `ALL OK` only when all of
this passed:

- the oracles on hand-made logs: each oracle accepts a good log and flags a bad one
- the runner flags a target that never goes quiet and one that keeps repairing
- the link drops, duplicates, delays and drops traffic of a machine that goes down
- the generator and the scenario JSON are deterministic and round-trip, and a run with the same seed gives the same result
- 300 clean sessions with 2 to 4 players hold every oracle, and a coverage guard checks that those sessions really cause
  ownership grants and transfers, deaths, loot races, spawn races, host loads, joins, leaves, replayed stale messages,
  lost and duplicated packets
- **every mutation twin is caught by the oracles**, by the C++ tests and again by the command line (12 twins on the
  example target, 1 on the skeleton), each with the kind of violation it should produce
- clean sessions hold under all five link presets, and injected bugs are still caught on the worst one
- a failing run shrinks (for example 203 ops to 3), the shrunk scenario still fails, and the same file passes once the
  bug is removed
- the JSON report is valid and has the seed, the violations, the shrink info and the logs
- the command line: exit codes, `--replay`, `--fixture`, bad input

It was run on Windows 11 with Visual Studio 2022 (MSVC 19.40) and cmake 4.1. The code is plain C++17 with standard
cmake. Anyone who builds it elsewhere should run the same command.

## Ported from

Ported from fnv-coop commit f57aa78 ([github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)), where it
fuzzes a Fallout: New Vegas sync core. The oracle logic, the scenario generator, the shrinker and the example world come
from there. This version is game-agnostic: the sync core, its transport and its chaos model are replaced by the
`ITarget` interface and a small built-in lossy link, and the example world gained one bug class (a stale assignment echo,
see BUG-CLASSES.md) and a skeleton target. The Fallout-specific adapter is not part of this repo.

## License

MIT, like the rest of this repo.
