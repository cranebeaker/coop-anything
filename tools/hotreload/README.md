# Hot reload: swap your mod's DLL inside a running game

A small C++17 library, a loader DLL and a Python client that put a freshly built mod DLL into a game
that is already running, keeping the mod's state. The edit, build, swap, test round drops from "restart
the game and load a save" to a few seconds.

It works for any Windows game whose mod is a DLL loaded into the game process (a script extender
plugin, an ASI plugin, an injected library). Nothing in it knows about a particular game. Read the
[playbook page](../../playbook/hot-reloading-your-mod.md) for the method and the mistakes, and
[examples/fnv](examples/fnv/README.md) for how one real mod wired it in.

**Fair use:** this is a development tool for your own mod in your own copy of a game. Never use it
against an online game that has anti-cheat.

## How it works

You split your mod in two:

| Half | Lives | What it holds |
|---|---|---|
| **Loader** (`hotreload_loader.dll`, or the static library `hotreload_host` inside your own plugin) | in the game for the whole session | the control pipe, the cache copy of the core, load / unload / reload, a blob store that survives reloads |
| **Core** (your DLL, exports `HotCore_Init`) | mapped from a private cache copy, replaced on every swap | everything else: your mod's logic, threads, hooks, sockets |

Why a cache copy: Windows will not let a build overwrite a DLL that is loaded. The loader maps
`cache\core.<pid>.<n>.dll` and leaves your build output unlocked, so the build and the swap tool can
replace it at any time.

A swap, from `hotreload.py`:

1. Read `dev.status` from the running copy: where its core file is, which build runs, which generation.
2. Copy the new DLL beside the core file under a temporary name and rename it over the core (the
   loader never sees a half-written file).
3. Call `dev.reload`. In the running game, between two frames: the old core **saves** every
   participant's versioned state and **drains** (waits until no thread runs core code), then it
   **shuts down** (stops threads, removes hooks) and the loader unmaps it. The loader maps the new
   copy and the new core **restores** the state.
4. Check the result: the running build id equals the CRC-32 and size of the file that was copied,
   the generation went up, nothing rolled back, the registry reports no failure and no leak.

If anything is wrong before the unmap, the old core keeps running with its state. If the new core
fails to start, the loader maps the old one again.

## What you get

| Piece | Where | Job |
|---|---|---|
| `hotreload_loader.dll` | built by CMake | the permanent loader with a plain C API (`HotReload_Start`, `HotReload_OnFrame`, ...) |
| `hotreload_host` (static lib) | `src/loader_host.cpp`, `include/hotreload/loader_host.h` | the same loader as a class, to link into your own plugin |
| `hotreload_core` (static lib) | `src/reload_state.cpp`, `reload_drain.cpp`, `core_glue.cpp` | for the core: state registry, `ManagedThread`, `adoptThread`, the drain, `BeforeUnload` / `AfterLoad` |
| `hotreload_control` (static lib) | `src/control_pipe.cpp` | a JSON-lines named pipe server. No other dependency |
| `hotreload.py` | this folder | the client: `swap` (with `--dry-run`, `--force`, `--root`, several `--pipe`) and `status`. Standard library only |
| example cores, test host | `examples/` | six builds of one example core with versioned state and two owned threads, and a stand-in game |
| unit tests, scenarios, twins | `tests/`, `run_tests.py` | see "How it is tested" |

## Split your mod: loader and core

1. **Pick the permanent part.** Your plugin entry point (the function the game's mod loader calls)
   stays in the loader. It does three things: call `HotReload_Start` once, call `HotReload_OnFrame`
   once per frame on the main thread, and forward the messages you want the core to see with
   `HotReload_OnMessage`. Everything else moves to the core.

   ```cpp
   HotReloadConfig cfg{};
   cfg.size = sizeof cfg;
   cfg.core_path = "C:\\games\\mygame\\mods\\mymod_core.dll";   // the file your build output gets copied over
   cfg.pipe_name = "mymod-0";                                   // the control pipe
   cfg.user_context = engineInterface;                          // handed to the core as api->user_context
   HotReload_Start(&cfg);                                       // in your entry point
   ...
   HotReload_OnFrame();                                         // every frame, main thread
   ```

2. **Make the core export one function and fill in four callbacks.** See
   [examples/core/example_core.cpp](examples/core/example_core.cpp), about 240 lines.

   ```cpp
   extern "C" __declspec(dllexport) int HotCore_Init(const HrLoaderApi* api, HrCoreApi* out) {
       g_api = api;
       out->abi_version = HR_ABI_VERSION; out->size = sizeof(HrCoreApi);
       out->init = &Init;                      // start threads, install hooks, then hotreload::AfterLoad(g_api)
       out->on_frame = &OnFrame;               // optional
       out->on_message = &OnMessage;           // optional
       out->before_unload = &BeforeUnload;     // return hotreload::BeforeUnload(g_api) ? 1 : 0
       return 1;
   }
   ```

3. **Register every piece of state that must survive** with a version number (below).
4. **Register every thread the core owns** (next section). This is the rule that bites.
5. **Keep the boundary plain C.** Between loader and core only the types in
   [include/hotreload/abi.h](include/hotreload/abi.h) cross: no `std::string`, no exceptions, no
   objects from either side's runtime. Nothing in the core may point into the loader, and nothing
   the loader keeps may point into the core (the loader removes the core's control methods before
   every unload).
6. **Build the core into a folder the game does not read from**, and let `hotreload.py` copy it
   over the live core file. The tool refuses `--dll` when it is the live file.

### State: versioned, saved before, restored after

```cpp
static bool save(hotreload::Writer& w)                    { w.u64(g_counter); w.str(g_label); return true; }
static bool restore(uint32_t, hotreload::Reader& r)       { g_counter = r.u64(); g_label = r.str(); return true; }
static void fresh()                                       { g_counter = 0; g_label.clear(); }
COOP_RELOAD_STATE(my_module, "my_module", /*version*/ 1, /*order*/ 50, nullptr, save, restore, nullptr, shutdown, fresh);
```

- Bump `version` whenever the saved layout changes. A new build that cannot read an older layout
  (no `migrate` function) starts that participant **fresh**, and `hotreload.py` prints a warning
  that names it. A `migrate(savedVersion, reader)` function converts the old layout instead.
- `save` can run and then be thrown away (a later step may refuse the reload), so it must not change
  anything. Returning false from `save` refuses the reload and the old core keeps running.
- The blob carries a CRC per participant and one for the whole blob. A damaged one never reaches
  `restore`: that participant gets `fresh()`.
- Order: save and restore run in ascending `order`, shutdown in descending order. Put hooks first
  (low number) and sessions after them, so shutdown stops sessions before it removes hooks.
- Process-wide handles (a `SOCKET`, a `HANDLE`) can be saved as integers and stay valid across the
  unmap if `shutdown` did not close them.

### Threads: register every thread the core owns

> **Every thread the core owns must be registered, or the drain refuses forever.**

The drain proves it is safe to unmap the core by looking at every other thread in the process: a
thread counts as inside the core when its instruction pointer is in the core image, **or any return
address on its stack points into it**. A worker thread that is parked waiting for jobs has its
instruction pointer in the kernel, but it always has core frames on its stack, so it is "inside".

The owner of such a thread stops and joins it in a participant's `shutdown`, which runs after the
drain. So the drain must be told to ignore it:

- Start the thread with `hotreload::ManagedThread` and it is registered for you.
- A thread you cannot start that way (a worker a library you link in created, a `std::thread` in
  code you do not control) needs `hotreload::adoptThread(tid)` when it starts and
  `releaseThread(tid)` after you join it.
- A thread that is neither makes the drain time out every time. The swap is refused cleanly (exit 1,
  the report says `drain timed out ... thread 1234 is still inside the code that would be unloaded`),
  the old core keeps running, and nothing is lost. The example core built as `core_unregistered`
  does exactly this, and the `unregistered_thread_refuses` scenario checks the refusal.
- A thread that is not yours (the game's own threads) is fine as long as no core code is on its
  stack. If the engine calls into your core from its own thread (a hook, a callback), that thread
  counts as inside while the call runs, and the drain waits for it to leave.

### Mods that patch code in the game

The glue cannot know your hooks. Give it three callbacks once, with `hotreload::SetDrainHooks`:
`suspend` puts the original code back so no thread can enter a detour any more, `resume` puts the
hooks back when the drain refuses, and `extraRanges` adds the address ranges of your trampolines, so
a thread inside generated code counts as busy. Register a leak probe for installed hooks
(`hotreload::global().addLeakProbe`), so an unremoved hook makes the unload refuse.

## Quick start (no game needed)

Build, start the test host, swap to the second example build:

```
cmake -S tools/hotreload -B build/hotreload
cmake --build build/hotreload --config Release --parallel 1
mkdir build\hotreload\demo
copy build\hotreload\bin\core_v1.dll build\hotreload\demo\core.dll
build\hotreload\bin\hotreload_test_host.exe --loader build\hotreload\bin\hotreload_loader.dll --core build\hotreload\demo\core.dll --pipe demo
```

In a second terminal:

```
python -I tools/hotreload/hotreload.py status --pipe demo
python -I tools/hotreload/hotreload.py swap --pipe demo --dll build/hotreload/bin/core_v2.dll --dry-run
python -I tools/hotreload/hotreload.py swap --pipe demo --dll build/hotreload/bin/core_v2.dll
```

The JSON report is on stdout, progress on stderr. Stop the test host with
`python -I -c "import sys; sys.path.insert(0,'tools/hotreload'); import hotreload; c=hotreload.PipeClient.connect('demo'); c.call('host.quit')"`
or close its window.

For your own game, replace the test host with your loader and the example core with yours. Start
the game once, then after every build:

```
python -I tools/hotreload/hotreload.py swap --pipe mymod-0 --dll build/mymod_core.dll
```

Several running copies (host and client of a co-op test): repeat `--pipe`. Every copy is checked
first, nothing is touched unless all of them can reload, then they are swapped one after the other,
stopping at the first failure.

### Options

| Option | Meaning |
|---|---|
| `--pipe NAME` | control pipe of a running copy (a short name or a full `\\.\pipe\...` path). Repeat for several copies |
| `--dll PATH` | the freshly built core |
| `--dry-run` | check every copy and report `would swap` or `unchanged`. Changes nothing |
| `--force` | reload even when the copy already runs these bytes |
| `--root DIR` | refuse unless every copy's core file is inside `DIR` (a guard against overwriting another install's file) |
| `--reload-timeout-s N` | how long to wait for `dev.reload` (default 90) |
| `--replace-wait-s N` | how long to retry when the core file is locked (default 5) |
| `--connect-wait-s N` | how long to wait for a pipe to appear (default 3) |
| `--out FILE` | also write the JSON report to a file |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | every copy runs the new build (or already did), or `--dry-run` found nothing wrong |
| 1 | a copy failed. The report names the check. The old core may still run, or have been rolled back to |
| 2 | **refused before anything was changed**, with the reason on stderr and in the report |

What produces which:

| Situation | Result |
|---|---|
| no control pipe (game not running, or no loader in it) | exit 2, "no control pipe" |
| monolith build: the pipe answers but has no `dev.status` / `dev.reload` | exit 2, "monolith build" |
| old loader: `dev.status` has no `buildId`, so a swap cannot be proven | exit 2, "old loader" |
| `--dll` is not a core (no `HotCore_Init` export: the loader, a text file ...) | exit 2 |
| `--dll` is the copy's own live core file | exit 2 |
| core file outside `--root` | exit 2 |
| one of several copies cannot reload | exit 2, the others are not touched either |
| the drain refuses (a thread is still inside, usually an unregistered one) | exit 1, the old core keeps running |
| a participant's `save` or `canUnload` refuses | exit 1, the old core keeps running |
| the new core's init fails | exit 1, the old core is mapped again with its state |
| the loader maps the original file, so the build is locked | exit 1, "cannot replace" |
| a change to the loader itself | not possible: the loader is never unloaded, restart once |

## Build id

The build id is `<crc32 hex>-<size in bytes>` of the exact bytes that were mapped (the cache copy).
The loader reports it in `dev.status`. `hotreload.py` computes the same text for the file it copies
and compares: "the running core is the build I just copied" is a checked fact, not a hope. A reload
that answers `ok` but leaves the old build running fails the check.

## The control channel

A Windows named pipe, one JSON object per line, one client at a time:

```
{"id":1,"method":"dev.status"}                    ->  {"id":1,"result":{"loaded":true,"generation":2,"buildId":"a1b2c3d4-95744",...}}
{"id":2,"method":"dev.reload","params":{"reason":"hotswap","timeoutMs":90000}}
{"id":3,"method":"nope"}                          ->  {"id":3,"error":{"code":-32601,"message":"unknown method: nope"}}
```

- `ping` is answered on the pipe thread (`{"pid":...}`), so a client can tell "game up, main loop
  stuck" from "no game".
- Every other method runs on **your main thread**: `HotReload_OnFrame` runs the queued requests. A
  method can touch game state without locks, and a reload happens between frames. If the main loop
  does not run the request in time the client gets error `-32001`.
- Built in: `dev.status` (loaded, generation, reloadCount, corePath, cachePath, buildId, last reload
  result), `dev.reload`, and from the core glue `reload.state` (participants, versions, live threads,
  what the last save, drain, shutdown or restore reported).
- Register your own with `api->register_method(name, fn, user)` in the core (removed before every
  unload) or `HotReload_RegisterMethod` in the host (kept). The reply text must be JSON.
- Only one client at a time can hold the pipe. A test runner and the swap tool must not hold it
  together.

The pipe is local only (remote clients are rejected). It carries no authentication: do not point it
at anything you would not let another program on your PC trigger.

## Files the loader writes

Beside the core file (or in `cache_dir`): `hr_cache\<core stem>.<pid>.<n>.dll` (the mapped copy,
deleted on unload, stale ones of dead processes removed at start) and `hr_cache\hotreload.log` (every
load, reload, refusal and core log line). The core finds files that sit beside your build output
through `api->core_dir`, because its own module file is the cache copy.

## How it is tested

```
python -I tools/hotreload/run_tests.py
```

Builds with CMake (one job at a time, build folder under the system temp directory unless you pass
`--build-dir`; `--arch Win32` builds 32-bit), runs the unit tests and the swap scenarios, then checks
every mutation twin. It prints `ALL OK` only when all tests pass and every twin is caught. It needs
cmake and Visual Studio 2022. No game is started: the stand-in is `hotreload_test_host.exe`, a
program that loads the loader, runs a main loop and serves the control pipe. The scenarios start it
themselves and stop it by pid.

What the last run covered: 37 unit tests (131 checks), 16 swap scenarios and 20 mutation twins, all
passing, in a 64-bit build and again in a 32-bit build.

- **Unit tests:** byte reader and writer, CRC-32, the registry (save, restore, migrate, version
  mismatch, corrupt payload, corrupt blob, preflight, shutdown order, leak probes), threads
  (`ManagedThread`, `adoptThread`), the drain (a thread parked inside the code is seen through a
  return address, listed threads are ignored, a timeout is a failure), the JSON helpers, and the
  control pipe (ping without the main loop, methods on the pumping thread, errors, one client after
  another, timeout).
- **Swap scenarios** (each starts the host, runs `python -I hotreload.py`, checks the outcome):
  state carried over and build id changed with host pid and threads kept running; two copies;
  unchanged and forced; dry run; `status`; the six refusals (no pipe, monolith, old loader, not a
  core, own core file, outside `--root`); all-or-nothing; a version bump without migrate (fresh
  start, warning); an unregistered thread (drain refuses, old core keeps running); a failing save
  (reload vetoed); a core whose init fails (rollback, then a good swap works).

**Mutation twins.** Each twin is a deliberately broken variant of one rule, compiled in only for the
test build and switched on with `HOTRELOAD_MUT=<n>` (or, for the client, a fault name). The guarding
test must pass without the fault and fail with it.

| Twin | Fault it switches on | Caught by |
|---|---|---|
| `drain_off` | the drain says "nobody is inside" without looking | `unregistered_thread_refuses` |
| `adopt_ignored` | `adoptThread` does nothing | `swap_carries_state` |
| `managed_ids_ignored` | the drain does not ignore `ManagedThread`s | `swap_carries_state` |
| `no_cache_copy` | the loader maps the original file, the build is locked | `swap_carries_state` |
| `stale_build_id` | the loader keeps the old build id after a reload | `swap_carries_state` |
| `version_ignored` | an older state layout is read as the current one | `swap_carries_state` |
| `no_state_blob` | the old core never hands its state over | `swap_carries_state` |
| `save_failure_ignored` | a failing `save` no longer vetoes the reload | `save_failure_vetoes` |
| `no_rollback` | the loader stays dead when the new core fails to start | `bad_core_rolls_back` |
| `veto_ignored` | the loader unloads although the core refused | `save_failure_vetoes` |
| `exit1_on_refusal` | the client exits 1 instead of 2 on a refusal | `refuse_no_control_channel` |
| `no_buildid_check` | the client does not refuse an old loader | `refuse_old_loader` |
| `dry_run_writes` | `--dry-run` copies the DLL | `dry_run_changes_nothing` |
| `root_ignored` | `--root` is not enforced | `refuse_outside_root` |
| `crc_unchecked` | a corrupt payload reaches `restore` | unit test |
| `forward_shutdown` | shutdown runs in registration order | unit test |
| `thread_not_joined` | `ManagedThread` detaches instead of joining | unit test |
| `drain_ip_only` | the drain checks only the instruction pointer | unit test |
| `drain_timeout_ok` | a drain timeout counts as success | unit test |
| `preflight_ignored` | `canUnload` refusals do not veto | unit test |

## Limits

- Windows only. The code has no pointer-size assumptions (it is tested as 64-bit and 32-bit).
- The loader is never unloaded. A change to the loader, to `abi.h`, or to anything a loader-side
  object holds needs one normal restart of the game.
- One control client at a time.
- The tests use a stand-in host, not a real game. A real engine adds threads, hooks and callbacks
  of its own, so for a new mod run `--dry-run` first, then a swap on a throwaway save, and read
  `hr_cache\hotreload.log` and `reload.state` if the drain refuses.
- Raw threads, hooks and sockets that the core owns and does not register are the mod's job: the
  library finds the thread problem for you (the drain refuses), but it cannot see a hook you
  installed unless you give it a leak probe.

## Changes from the original

Generalised from fnv-coop commit e85460f ([github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)).

- Renamed to the neutral `hotreload` namespace, `HotCore_Init` export, `COOP_RELOAD_STATE`, and the
  state blob key `coop.reload.state`. The wire protocol, build id, reload sequence and drain are the same.
- The loader no longer depends on the game's plugin interface or the original bridge. The control pipe is
  a small self-contained server (`hotreload_control`). Methods are all main-thread, so the in-flight
  gate of the original is gone.
- The config mirror (copying the mod's ini and data files into the cache folder) and the file
  watcher were dropped. `api->core_dir` tells the core where the build output sits.
- Hook patching and the thread-freezing write guard are not included. `SetDrainHooks` is the generic
  interface for them.
- The client takes a pipe name instead of an instance number, can swap several copies, and has a
  `--root` guard instead of the instance folder check.
- New: the example core, the test host, the unit tests, the scenarios and the twins.

License: MIT, like the rest of this repository.
