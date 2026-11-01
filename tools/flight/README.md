# Co-op flight recorder and desync analyzer

A black box for co-op mods, for any game. Each machine records what it sent, received, decided and applied, plus digests of
its state, into a small `.cofr` file ("co-op flight record"). Put the files of 2 to 4 machines side by side and the analyzer
tells you **where their worlds first disagreed, down to the bucket, the key and the value, and which messages led there**.
Then it can shrink the failing run to the few steps that matter and write a regression test from them.

Two halves, one file format:

| Part | Where | What |
|---|---|---|
| Recorder | `cpp/` | Standalone C++17 (standard library only). Ring buffer in memory, written to a file on a desync, crash or request. About a microsecond per recorded event. |
| Analyzer | `*.py` | Python 3, no dependencies, knows no game. Reader and pretty-printer, align 2 to 4 machines, first divergence, delta-debugging shrinker, regression test generator, HTML report. |
| Format | [FORMAT.md](FORMAT.md) | The byte layout, the schema, the digest algorithm, the replay hook contract. Write files from any language. |

The playbook page [Desync hunting with traces and shrinking](../../playbook/desync-hunting.md) shows the whole workflow.

### Host portability

On Windows, the test runner tries Visual Studio 2026, Visual Studio 2022, then
Ninja when creating a fresh CMake build. Ninja requires a configured C++17 compiler
on PATH. An existing CMake cache keeps its generator; compiler installation paths
containing "Visual Studio" do not make a Ninja build an MSBuild build.

Replay commands may be argument lists (preferred for API callers) or strings.
Windows strings use CRT-style argument quoting, including paths with spaces,
backslashes before quotes and doubled quotes inside quoted arguments. POSIX
strings retain POSIX `shlex` parsing. Neither path enables shell execution.
The `PortabilityTests` group checks both parsers and generator/cache selection
without needing a game or all supported Visual Studio versions installed.

## Try it in one minute

The repo ships two pairs of sample traces made by the C++ recorder ([golden/](golden)). In `desync_*` a script on the client
quietly overwrote one value:

```
python -I tools/flight/flight.py diverge tools/flight/golden/desync_host.cofr tools/flight/golden/desync_client.cofr
```

```
FIRST DIVERGENCE (digest) at 680.000ms: world digest (epoch 1, seq 5) differs: ... differing 0xA21A963AA6B76AAB: slot 0 'stage.q1 = 10' vs slot 1 'stage.q1 = 99'
  key: world:0xA21A963AA6B76AAB   suspect slot(s): [0, 1]   (2 candidate(s) in total)
  narrowing: bucket(s) 171 of 256
    key 0xA21A963AA6B76AAB (bucket 171)  [from this digest]
      slot 0: value hash 10  stage.q1 = 10
      slot 1: value hash 99  stage.q1 = 99
  causal chain, window 290.000ms .. 680.000ms:
        290.000ms slot 0 TxnCaptured  commit seq=3 ...
        ...
        325.000ms slot 1 TxnApplied   seq=4[0] origin=0 epoch=1 tag=1 hash=8d0f343480ecf481
        325.000ms slot 1 ScriptWrite  seq=4 key=stage.q1 value=99
```

The chain starts at the last digest both machines agreed on and ends at the one that differs, so the bug is in between.
The `ScriptWrite` line is a game-defined record (see the schema below).

## Add the recorder to your mod

[cpp/example_usage.cpp](cpp/example_usage.cpp) is a complete, built and run example. The steps:

1. **Add the files.** Put `tools/flight/cpp` on your include path and add `cofr.cpp` to your build, or `add_subdirectory(tools/flight/cpp)` and link the `cofr` target.
   Nothing else is needed: `cofr.h` and `cofr.cpp` use the standard library only. Your game hooks need only `observer.h`.
2. **Create one `cofr::Recorder` per machine.** Give it a slot, a role, a label, a folder, and optionally a schema and a decoder:

   ```cpp
   cofr::Config cfg;
   cfg.slot = 1; cfg.role = cofr::kRoleClient; cfg.label = "client-1";
   cfg.dir = "flight";                  // dumps go here
   cfg.ringBytes = 16u << 20;           // the newest 16 MB of records are kept
   cfg.schemaJson = R"({"game":"mygame","digest_categories":{"0":"quests","1":"npcs"}})";
   cfg.decoder = decodeMyProtocol;      // optional, see below
   cofr::Recorder recorder(cfg);
   recorder.mapPeer(0, 0);              // transport peer 0 is the host (slot 0), if your peer ids are not slots
   ```
3. **Call it from your hooks.** Wrap your transport's send and poll, and your "apply this change to the world" function:

   ```cpp
   obs->setFrame(frame);                          // once per frame
   obs->netOut(peer, bytes, size);                // every datagram sent
   obs->netIn(peer, bytes, size);                 // every datagram received
   obs->captured(commit);                         // the host decided a change set (cofr::CapturedEvent)
   obs->applied(effect);                          // one effect applied to the local world (cofr::AppliedEvent, with a hash of the effect)
   obs->digest(world.digest(0, epoch, seq));      // every few seconds at a quiet point
   obs->ownership(event);                         // an actor changed owner
   obs->marker(cofr::kMarkNote, "player joined"); // anything you want to see in the chain
   ```
   `obs` is a `cofr::IObserver*`. It is the recorder in a test build and can be null in a release build.
4. **Dump when something is wrong.** `recorder.onDesync("what you saw")` writes a marker and a file (at most 8 per run and one per 30 s by default);
   `recorder.dump(path)` writes one on request; `recorder.dumpCrash()` is for a crash filter and does not wait for the lock.
   On a desync, first write the detail so the analyzer can reach the key: `recorder.digestDetail(category, epoch, seq, entries)` and one
   `recorder.entryText(key, valueHash, "readable text")` per entry. `cofr::BucketedState` keeps a digest up to date one entry at a time.
5. **Make your effect hashes stable.** The analyzer compares the hash of the same effect on different machines. Hash the effect's content, not
   pointers, addresses or anything that differs between runs (`cofr::hash64` is FNV-1a 64; any stable hash will do).

### The decoder and the schema

* **Decoder** (optional): `bool(const uint8_t* data, size_t size, NetMeta& out)`. It runs when the recorder dumps, never when a datagram is recorded. Fill the message
  type, session, epoch and the range of change numbers (`seqLo`, `seqHi`) the datagram carries. Without a decoder the recorder still keeps every datagram
  as raw bytes and the analyzer still pairs sends with receives. With one, the analyzer can link a datagram to the txn it carried. A decoder can also say
  that a datagram carries a digest of the sender's state (`NetMeta::hasDigest`); the recorder then adds a "derived" Digest record.
* **Schema** (optional JSON, stored in every file): names for your digest categories, marker kinds, ownership ops and message types, and the layout of your
  own record types (32 to 255, written with `recorder.custom(type, bytes)`). Without one you get the example set (story, actors, containers, objects, other).
  The full list of keys is in [FORMAT.md](FORMAT.md) section 5; [schemas/example_game.json](schemas/example_game.json) is a small custom one.

### From another language

The format is plain little-endian records, so any language can write it. `cofr.py` has a writer (`cofr.dump(path, header, records)`) and
the exact layout is in [FORMAT.md](FORMAT.md). A mod in C#, Lua or Rust can write the file itself, or send events to a small helper.

## Analyzer commands

All commands run as `python -I tools/flight/flight.py <command> ...`. Add `--schema FILE` to print, align, diverge, report or fixture to override the schema stored in the files.

| Command | What it does |
|---|---|
| `print FILE [--type NetIn,Digest] [--limit N]` | Pretty-print one trace. |
| `align FILE FILE [FILE FILE]` | Line up 2 to 4 machines: clock offsets, messages matched, lost, delivered twice. |
| `diverge FILE FILE [...] [--json] [--check]` | The first divergence, narrowed to bucket, key and value, with the causal chain. `--check` exits 1 when one is found. |
| `report FILE FILE [...] -o report.html [--shrink shrunk.json]` | A self-contained HTML report: machines, timeline, first divergence, chain, minimal reproduction. |
| `fixture FILE FILE [...] -o fixture.json` | Build a replay fixture from the traces: one step per datagram a machine received. |
| `shrink fixture.json --replay-cmd "prog {fixture}" -o shrunk.json` | Delta-debug the steps down to a minimal list that still fails the same way. `--replay-py module:function` plugs in a Python function instead. |
| `gentest shrunk.json --name stale_echo [--out DIR]` | Write the regression test: the shrunk fixture plus a CMake `add_test` line. |
| `regress DIR --replay-cmd "prog {fixture}"` | Run every fixture in a folder through the hook; prints ALL OK when each matches its expectation. |
| `schema [FILE]` | Print the default schema, or check a schema file and print it back. |
| `from-failure [PATH ...]` | A failed bot run to an offline repro and a regression case, see the next section. Same as `from_failure.py`. |

### The replay hook

The shrinker does not know your game. You give it a **replay hook**: a program (or Python function) that takes a fixture, replays it in your
test rig and says whether the bug still shows. The contract is in [FORMAT.md](FORMAT.md) section 7, and [toy_replay.py](toy_replay.py) is a complete
example of both forms, which the tests use:

```
python -I tools/flight/flight.py shrink fixture.json --replay-cmd "python -I tools/flight/toy_replay.py {fixture}" -o shrunk.json
python -I tools/flight/flight.py shrink fixture.json --replay-py toy_replay:replay --replay-path tools/flight -o shrunk.json
```

Steps must be independent (any subset is a valid fixture), and a step marked `"pin": true` is never removed. The reduction is the ddmin
algorithm of Zeller and Hildebrandt; a smaller list only counts if the failure keeps the same signature, so a shrink that turns your bug into a different one is rejected.

## From a failed game run to a regression case

When a bot run (`tools/bots`) fails a step, the runner saves a `failure.json` and each copy's evidence in a `FAIL-*` folder. With both
copies still up, this reads files only and takes a few seconds:

```
python -I tools/flight/from_failure.py                         # the newest failure under ./coop-bots-out
python -I tools/flight/from_failure.py coop-bots-out/RUN       # every failure.json below that run folder
python -I tools/flight/from_failure.py host.cofr client.cofr   # two recordings given directly
python -I tools/flight/from_failure.py FAIL-DIR --instances DIR --report failure.html
```

It finds the host and client recordings the failure names, replays them through the analyzer, prints **REPRODUCED** or **NOT REPRODUCED**,
cross-checks that with what the bots saw in the live status (the `counters`, `seq` and `synced` of each machine's status, see
[tools/bots](../bots/README.md)), shrinks the recordings to the few records that still show the same failure, and writes a regression case
`tools/flight/regress/<name>.json`. `--no-write` gives only the verdict, `--report FILE.html` the full flight report (with the minimal
reproduction), `--json` the raw result. Exit 0 means every failure reproduced, 1 at least one did not, 2 recordings were missing or unreadable.

* **Where the recordings are looked for.** A machine entry of `failure.json` names its recording in `flight_dump` (a path, or `{"path": ...}`),
  or in `flight`, `recording`, `cofr` or `trace`, at the top of the entry or in the `capture` dict that the adapter's `capture()` returned
  (any value there whose file name matches `--pattern`, default `*.cofr`, counts too). A relative path is tried under `<root>/<instance>/`,
  `<root>/`, the failure folder's `<machine name>/` and the failure folder, then as written. `<root>` is `--instances DIR` or the environment variable
  `COOP_INSTANCES_ROOT`: the folder that holds one sub folder per game copy. With no recorded path, the newest matching file written in the
  10 minutes before the failure in the machine's failure sub folder or game copy is used.
* **The case.** The case holds the shrunk records as a `cofr-trace-1` fixture ([FORMAT.md](FORMAT.md) section 7), so
  `python -I tools/flight/regress.py tools/flight/regress` replays it in a second with no game and no replay program. It pins the recorded evidence:
  it passes while the recording still shows the failure. A recording cannot change when you fix the mod, so the proof of a fix is a fresh run:
  run the fight again and run `from_failure.py` on the new failure folder, and NOT REPRODUCED (exit 1) is the sign it worked.
  `--expect-converges` writes the case the other way round (it fails until the analyzer stops seeing the failure). `--schema FILE` reads the
  recordings with another schema, and the case keeps it. Machine paths are removed from the case.
* **Your own recording format.** The generic part only reads `.cofr`. If your game writes something else, give `--converter module:function`
  (the module is found on `--converter-path DIR`, or write `path/to/file.py:function`) and, if the files are not named `*.cofr`, `--pattern GLOB`.
  The hook is one function:

  ```python
  def convert(path: str, info: dict) -> bytes:
      """path: the recording as found. info: {"name": ..., "role": ..., "instance": ...} of the machine (a value may be None).
      Return the bytes of a version 1 .cofr file (FORMAT.md). Raise any exception to report that the file cannot be converted."""
  ```

  The default converter returns the file unchanged. The written case keeps the converted records, so it replays without your converter.

## How it is tested

One command from the repo root runs everything and prints `ALL OK`:

```
python -I tools/flight/run_tests.py
```

| Part | What runs |
|---|---|
| C++ (cmake, Visual Studio 2022 on Windows, `--parallel 1`) | `cofr_tests`: 40 tests (recorder behaviour, ring wrap, truncation, threads, bucketed digests, the schema record, custom records, golden files, overhead benchmark), 12 of them mutation twins; plus the example program. Built in a folder in the system temp dir (`COFR_BUILD_DIR` changes it), so nothing is written into the repo. |
| Python | 115 tests: format round trips, schema rules, alignment of 2 to 4 machines, every divergence kind, narrowing, the shrinker with a command hook and a Python hook, regression generation, the HTML report, the CLI end to end, and the failure-to-case tools (48 of them: finding recordings, the converter hook, status cross-check, shrink stages, cases and their replay, exit codes), all on fake recordings built with the `cofr.py` writer and fake failure folders. |
| C++ writes, Python reads | The C++ tests write the golden traces and a trace that uses every record type and option; Python must read them field by field, recompute the bucketed digest from the detail records and get the C++ bytes, and write the exact same bytes back. The other way round, Python writes a file and a C++ test parses it. A drift test fails if the writer's bytes differ from `golden/`. |
| Mutation twins | A **twin** breaks one thing on purpose and the suite must fail. 12 C++ twins re-introduce recorder bugs (a full ring drops the newest record, received datagrams not recorded, a cut datagram not flagged, a changed value invisible in the buckets, ...). 140 Python twins edit the sources (latest instead of earliest divergence, repairs counted as divergence, pinned steps removed, report not escaped, the converter ignored, the oldest recording guessed, ...) and each must make the suite fail. |

It needs Python 3, cmake 3.20 or newer (for `ctest --test-dir`) and a C++17 compiler. It was run with Python 3.11, cmake 4.1 and Visual Studio 2022. The full run takes a few minutes, most of it the Python mutation twin.
Options: `--quick` runs only the fast Python tests (a few seconds, prints `QUICK OK`), `--no-cpp` skips the C++ phase, `--no-twin` skips the Python
mutation twin, `--only A,B` runs just the Python test classes A and B (the twins of the failure-to-case tools use it). Only the full run prints `ALL OK`. Regenerating the golden files after a deliberate format change: run the C++ `cofr_tests` with `COFR_WRITE_GOLDEN=1` and `COFR_GOLDEN_DIR=tools/flight/golden`, then rerun the suite.

### Recording overhead

The benchmark in `cofr_tests` asserts a limit of 100 microseconds per frame. On the PC it was written on (x64 Release, repeated runs):
0.01 to 0.2 microseconds per idle frame (an ack pair every half second, a digest every two seconds) and 7 to 22 microseconds per busy frame (20 datagrams out, 20 in, 10 applied effects).
A bucketed digest of 20000 entries costs about 0.1 ms. After one changed entry in a 200000-entry state, `BucketedState` produces the new digest in about 3 microseconds (median) where a full pass takes about 1 ms.

## What it does not do

* It records what your hooks report. A change your hooks never see is not in the trace.
* A trace holds hashes of host-side decisions, not the decisions themselves, so a fixture built from traces replays what each machine received, not what the host decided.
* Pairing a send with its receive relies on the transport peer id meaning the other machine's slot (or a `mapPeer` entry). When identical bytes are retransmitted, which copy was lost is a guess; the count is right.
* The ring keeps the newest records. A desync that began before the ring's oldest record shows up as an alignment note ("lost N old records"). Use a bigger ring or dump earlier.

## Credits and origin

Ported from fnv-coop commit f57aa78 ([github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)), where it was built for a Fallout: New Vegas
co-op mod. This version is game-agnostic: a neutral `.cofr` format with a configurable schema, a recorder with its own tiny observer interface and no
dependency on that project's sync core, and an analyzer that reads everything game-specific from the schema. What stayed in the fnv-coop repository: the decoder for its own wire protocol, the wrappers around its transport and effect sink, the adapter for its engine model, the plugin service (crash filter, bridge commands) and its game-state digest cache.

Updated from fnv-coop commit e85460f

Ideas from open work: record and replay as in [rr](https://rr-project.org/); deterministic simulation, snapshot and desync-detection thinking from
[Gaffer on Games](https://gafferongames.com/); delta debugging (ddmin) by Andreas Zeller and Ralf Hildebrandt, "Simplifying and Isolating Failure-Inducing Input" (2002).
FNV-1a is by Glenn Fowler, Landon Curt Noll and Phong Vo; SplitMix64 is by Sebastiano Vigna (public domain).

License: MIT, like the rest of this repo.
