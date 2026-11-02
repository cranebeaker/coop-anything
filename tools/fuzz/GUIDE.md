# Guide: wrap your sync core in a fuzz target

This guide takes you from "I have a sync core" to "the fuzzer runs my core, and I know it catches the bugs I care
about". Read `example/skeleton_target.cpp` (about 190 lines, compiled and tested) next to it. It is the smallest
complete target. `example/example_target.cpp` is the full-featured one.

You wrap **the sync logic**, not the game. The fuzzer never starts the game. It runs several copies of your core in
one process, connects them with a lossy fake network, throws a random session at them, and checks the result. That
makes it fast (thousands of sessions a minute) and repeatable (a seed always replays the same session).

## What your core must allow

A target can only wrap code that can run N times in one process under a fake clock. Check four things first:

1. **No global state.** Two machines in one process must not share singletons. If your core uses globals, put them in
   one struct that you create per machine, or add a reset hook and give each machine its own copy.
2. **Injectable time.** The core takes the time from the caller (`tick(nowMs)` or a clock callback). No wall clock,
   no `sleep`, no timer threads. Fixed 10 ms steps are what the example uses.
3. **Injectable transport.** The core sends bytes through a callback and receives bytes through a function call. The
   target moves the bytes through `LossyLink`.
4. **Seedable randomness.** Any randomness comes from a seed the target passes in. Then a failing seed replays exactly.

If your core is tied to the game engine, split the engine calls out behind a small interface and give the target a fake
implementation (a map of things standing in for the game world). That fake world is the part the oracles look at.

## Step by step

### 1. Copy the skeleton

```
copy tools\fuzz\example\skeleton_target.cpp tools\fuzz\my_target.cpp
```

Rename the target (`kRegisterSkeleton`, the `TargetInfo` name). Delete part 1 (the stand-in core) and keep part 2,
the adapter.

### 2. Build the machines

In the constructor, create `s.machines` copies of your core (2 to 4, machine 0 is the host). Give each one:

- a send callback that calls `net_.send(from, to, bytes)` on the built-in `LossyLink`
- a report callback (step 5)
- the seed: derive it from `s.seed` and the machine number

Set the starting link: `net_.setLink(s.link)`. If `s.profile` is not empty, use `linkProfile(s.profile, link)`
instead (named presets: `lan`, `good_wifi`, `bad_wifi`, `mobile`, `hell`). If you bring your own transport, ignore
`LossyLink` and `s.link`, but then you own loss, duplication, delay and reordering.

### 3. Drive the clock: `advance(toMs)`

Step a virtual clock in small slices (10 ms is fine). In each slice: `net_.advance(now_)`, deliver every received
packet (`net_.poll(m, from, bytes)` then your core's receive function), then run the core's periodic work on its own
schedule (the example ticks its streams every 50 ms, ownership every 200 ms, digests every 2 s).

### 4. Map ops onto your game: `apply(op)`

The fuzzer's ops are generic: `Move`, `Hit`, `Pickup`, `Drop`, `Spawn`, `Activate`, `Join`, `Leave`, `Save`, `Load`,
`SetLink`, `Stale`. The table is in `include/coopfuzz/fuzz.h`. Do what your core can do and ignore the rest
(the skeleton handles only `Move`). Rules:

- An op names a machine (`op.m`) and small numbers. References are slots: pick "the (a modulo count)-th live actor",
  never a raw id. That keeps every op valid when the shrinker deletes other ops.
- Do the thing the way the game would, through the core's normal entry point (a local change, a request to the host),
  not by writing into another machine's state.
- `Stale` asks you to inject an old message: replay a recorded packet (`c=1`) or send old content in a fresh packet
  (`c=0`). It is how "stale echo" and "double apply" bugs get provoked. Ignore it if your protocol has nothing to
  replay, but then those bug classes are only reachable by the network's own duplication and reordering.

### 5. Report what each machine applies

This is the step that makes the oracles work. Every time a machine applies or rejects an incoming message, call
`logApply(...)` from `TargetBase` (the skeleton does it from one callback):

| Argument | What to put there |
|---|---|
| `machine` | the machine that applied it |
| `idem` | a stable id of the **message**, the same every time that message is delivered: for example (sender, sequence) packed into 64 bits. Use 0 if a message has no id (it is then not checked for double apply) |
| `ref` | the net id of the thing, or -1 |
| `epoch`, `seq` | the ownership epoch and the update sequence the message carried, or 0 |
| `origin` | the machine that produced the content (not the one that forwarded it) |
| `kind` | `Pos`, `PlayerPos`, `Hp`, `Owner`, `Holder`, `Door`, `Spawn`, `Remove`, `Reset`, `Intent` |
| `accepted` | true when the message changed state, false when it was correctly dropped |
| `worldEpoch` | the save/load generation the message was made under, or 0 |
| `why` | a short reason for a rejection ("stale epoch", "duplicate"). Rejections are counted in `stats()` as `reject_<why>` |

Log **rejections too**. They prove the defences ran, and the coverage line tells you which defences the fuzzer never
reached.

If your core has an ownership service (who simulates which actor), call `logOwn(Grant|Transfer|Release, ref, owner,
prevOwner, epoch)` at each decision, and `logOwn(WorldEpoch, ...)` when the host replaces the world. If your game has no
ownership, skip it: the ownership oracles then see no events and check nothing. Say so in your own docs.

### 6. Answer the oracle questions

| Method | Meaning |
|---|---|
| `view(m)` | everything machine m has, as `Thing`s sorted by net id. Fill `ident` for things that have an identity beyond the net id (the spawn site, the original item), so duplicates can be found |
| `digest(m, cat)` | a hash of machine m's state, one hash per category (0 to 3). Equal on all machines at rest. A target without categories can return the same hash for all four |
| `repairs(m)` | how many resync or heal actions machine m has taken. A counter that never goes down. Return 0 if your core has no repair path |
| `heal()` | switch the link to perfect, reconnect everyone, and stop all new activity (AI, heartbeats that change state). The fuzzer calls it once, after the last op |
| `quiescent()` | true when nothing is in flight, every machine has caught up with the host, and activity is frozen. Be strict: this is the gate that decides when to compare states |
| `connected(m)` | false while machine m is out of the session (it is skipped by the convergence check) |

### 7. Register and build

At the end of your file:

```cpp
const TargetRegistrar kRegisterMine(TargetInfo{"mine", "my sync core", {"bug_one", "bug_two"}, makeMine});
```

`makeMine(const Scenario& s, std::string* err)` builds your target. It must set `*err` and return null if
`s.bugs` has a name it does not know. Then:

```
cmake -S tools/fuzz -B build/fuzz -DCOOPFUZZ_TARGET_SOURCES="my_target.cpp;example/example_target.cpp"
cmake --build build/fuzz --config Release --parallel 1
```

or set `COOPFUZZ_CMAKE_ARGS=-DCOOPFUZZ_TARGET_SOURCES=...` and use `python -I tools/fuzz/run.py --target mine`.
Pass `-DCOOPFUZZ_TESTS=OFF` if you do not want the self tests of the framework in your build.

### 8. Run clean, and fix your adapter first

```
python -I tools/fuzz/run.py --target mine --count 200 --players 3
```

The first failures are usually in the adapter, not in your core. Open the `*.min.report.json` file. The minimized
scenario is often 3 to 10 ops. Ask three questions:

- Did my `quiescent()` say "quiet" while packets were still in flight? Then `converge` fires too early.
- Did my `idem` repeat for different messages, or differ for the same message? That gives false `double_apply`.
- Does `view` include state that is legitimately different on each machine (local-only things)? Leave those out.

Only after 200 clean sessions on every player count means something. Then run longer:
`python -I tools/fuzz/run.py --target mine --minutes 2 --players 4`.

### 9. Check that the fuzzer can fail on your target

Clean runs prove nothing until you have seen red. Add the twins (see [BUG-CLASSES.md](BUG-CLASSES.md), "Add your own
twin"): switch a past bug back on and confirm the run fails with the oracle you expect. Keep a test that sweeps seeds
for each twin (copy `tests/tests.cpp`, `twin(...)`). Also keep one that sweeps clean seeds and one that counts the
events the sweep reached (`stats()`): a fuzzer that never causes a transfer, a loot race or a save/load passes
for the wrong reason.

### 10. Use it

- Every failure prints `FAIL seed N ...` and writes, under `--out`:
  `fail_seedN_<oracle>.min.scenario.json` (the shrunk session), `.min.report.json` (violations, stats, ownership and
  apply logs) and the `.full.*` versions of the original session.
- Replay: `coopfuzz --replay <file>.min.scenario.json` (or `python -I tools/fuzz/run.py --replay <file>`). Exit 1 and
  `VIOLATION <oracle> <detail>` lines while the bug is there, exit 0 once it is fixed.
- Keep a fixed bug as a regression: `python -I tools/fuzz/run.py --fixture <bug_name>` writes a shrunk repro
  (`regress_<bug>.json`) that you commit and replay in your tests.
- Run `--minutes 2` after every change to the sync core, and a long run (`--minutes 30`) overnight.

## Cost and limits

- A session is virtual time. On the machine this was built on, the example target ran about 130 sessions per second
  (3000 sessions of 15 virtual seconds in 23 seconds). A real core with a heavier world will be slower.
- The fuzzer can only see what the target reports. A bug in a field the view does not contain, or in a message that is
  never logged, is invisible. Grow the view and the log when you find a bug by hand that the fuzzer missed, then add
  a twin for it.
- It tests the sync logic against network faults and racing players. It does not test the engine hooks, the game's
  own rules or the real network stack. Those need the in-game bots described in
  [automated-qa.md](../../playbook/automated-qa.md).
