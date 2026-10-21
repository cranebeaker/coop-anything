# buildlock: share one build tree between agents, and cache the compiles

Several agents working on one project want one build tree: a second tree costs disk and a full rebuild. But two builds in one
tree at the same time corrupt each other. This tool puts a lock around the tree and drives CMake so that the lock is always
released.

| Part | What it does |
|---|---|
| [`buildlock.py`](buildlock.py) | The lock: a lease with a timeout, an owner name, first come first served waiting, and recovery of a lock whose holder died. A Python class and a command line (`run`, `status`, `break`). |
| [`build.py`](build.py) | A CMake build driver that takes the lock, cleans the environment, loads the Visual Studio command line, configures only when needed, builds, and optionally uses sccache as a compiler cache. |

Standard library only; Python 3.8 or newer. The tests ran on Windows; the process check has a separate branch for Linux and macOS.

## Quick start

Build a project in a shared tree:

```
python -I tools/buildlock/build.py --source . --build build/all --owner agent1 --target my_tests
```

Run any command under the lock (a test run that reads the build output, a second tool that writes into the tree):

```
python -I tools/buildlock/buildlock.py run --tree build/all --owner agent1 -- ctest --test-dir build/all
python -I tools/buildlock/buildlock.py status --tree build/all
```

From Python:

```python
from buildlock import BuildLock
with BuildLock("build/all.buildlock", owner="agent1").acquire(wait_s=3600):
    run_the_build()
```

The lock is the folder `<tree>.buildlock`, next to the tree, so wiping the tree does not wipe the lock. Pick a name with
`--owner NAME` (or the environment variable `BUILDLOCK_OWNER`); the default is `agent-<pid>`. Use a name that tells the next
agent who to ask, and never a personal name.

## How the lock behaves

- **One holder.** The lock is a folder made with `mkdir`, which is atomic. `lock.json` inside says who holds it: owner, label, pid, the
  start time of the process, a random token and the last heartbeat.
- **A lease.** A background thread renews the heartbeat every `lease_s / 3` seconds (`--lease-s`, default 120). While the heartbeat is
  fresh and the holder process runs, nobody else can take the lock, however long the build takes.
- **Stale lock recovery.** A waiter takes the lock over when the holder process is gone (checked by pid **and** process start time, so a
  reused pid is not taken for the holder), when the heartbeat is older than the lease, when the lock was held longer than
  `--max-hold-s` (default 7200, for a hung holder that still heartbeats), or when the lock folder never got a `lock.json`. It says why on
  stderr. The break is done under a short guard and only if the lock still carries the token that was judged stale, so a fresh lock
  is never broken by a slow competitor.
- **First come, first served.** A waiter writes a ticket and may take a free lock only when it has the oldest live ticket. The tickets
  of dead processes, and tickets that their waiter has not touched for 30 seconds, are removed.
- **Only the holder releases.** A lock that was broken and taken by someone else is left alone, and the old holder is told by its
  heartbeat (`[buildlock] the build lock was taken away`).
- **Exit codes of `run`.** The command's own exit code; 3 when the lock was not free within `--wait-s` (default 3600), with the holder
  named in the message; 2 for a usage error.
- **By hand.** `buildlock.py break --tree T --only-if-stale` removes a lock only when it is stale; without `--only-if-stale` it
  removes any lock. Do that only when you know the holder is gone.

## The build driver

`build.py` does these steps in order:

1. Removes `CMAKE_TOOLCHAIN_FILE`, `CMAKE_GENERATOR`, `CC`, `CXX` and `ANDROID_NDK*` from the environment, because a stray Android
   toolchain file in the shell cross-compiles everything. `--keep-env` keeps them.
2. On Windows with a Ninja or Makefile generator it loads the Visual Studio command line for `--arch` (default x64), found with
   `vswhere`, or named with `--vcvars FILE` or `BUILDLOCK_VCVARS`. `--msvc auto` (the default) does this only when `cl` is not on PATH;
   `on` forces it and `off` never does it. A Visual Studio generator does not need it.
3. Picks the generator: Ninja when `ninja` is on PATH, else CMake's default (`--generator NAME` overrides).
4. If the tree is a Ninja tree that was configured with the same arguments and `ninja -n` says there is no work, it returns at once
   **without taking the lock**, so a build that has nothing to do never waits behind someone else's compile.
5. Takes the lock, then configures when the tree is new or the configure arguments changed since the last configure (it remembers them
   in `.buildlock-configure.json` in the tree), then builds. The lock is released on success, on failure and on Ctrl-C.
6. Prints `BUILD-TIME <label> <seconds>`.

Useful flags: `--target T [T ...]`, `--config Release`, `--jobs N` (default: `CMAKE_BUILD_PARALLEL_LEVEL` or 2), `--cmake-arg=-DNAME=VALUE`
(write the equals sign, because the value starts with a dash), `--clean`, `--configure-only`, `--keep-going`, `--dry-run` (print the commands),
`--no-lock`, `--wait-s`, `--cmake "CMD ..."` (another cmake program, which is also how the tests run a fake one). `--help` lists everything.

## sccache: a compile cache for MSVC and CMake

[sccache](https://github.com/mozilla/sccache) wraps the compiler, hashes each compile, and answers from a cache when it has seen the same
compile before. A rebuild after a branch switch, or a second agent building the same sources, then costs a lookup instead of a
compile. **This tool never installs anything.** Install sccache yourself the way you install other tools (ask your owner first), make
sure `sccache --version` works in the shell that runs `build.py`, and then:

```
python -I tools/buildlock/build.py --source . --build build/all --sccache on --sccache-stats
```

`--sccache auto` (the default) uses it whenever it is on PATH, `on` stops with exit code 2 when it is missing, `off` never uses it.

What `build.py` adds to the configure step when sccache is used:

| Argument | Why |
|---|---|
| `-DCMAKE_C_COMPILER_LAUNCHER=<sccache>` and `-DCMAKE_CXX_COMPILER_LAUNCHER=<sccache>` | CMake runs every compile through sccache. |
| `-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` | sccache cannot cache a compile that uses a precompiled header (`/Yu`, `/Yc`). |
| `-DCMAKE_POLICY_DEFAULT_CMP0141=NEW` and `-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` (MSVC only) | `/Zi` writes a shared PDB, which a cache cannot reuse. `Embedded` is `/Z7`: the debug info goes into each object file. Needs CMake 3.25 or newer. |

Things to know:

- **Generator.** A compiler launcher works with the Ninja and Makefile generators, not with a Visual Studio generator. With a Visual
  Studio generator `auto` builds without sccache (and says so) and `on` stops with exit code 2.
- **CMake older than 3.25.** Set the equivalent yourself: remove `/Zi` and `/ZI` from the compile flags and add `/Z7`.
- **One cache for all agents.** Agents that run as the same user on the same PC share one sccache server and one cache folder. To
  move the folder or change its size set `SCCACHE_DIR` and `SCCACHE_CACHE_SIZE` in the environment before the first sccache command.
- **Hits need the same compile.** The key covers the compiler, its command line and the preprocessed source, which contains file
  paths. Two trees at different paths, or builds with different flags, do not share hits. Keep one checkout path per project.
- **See what it does.** `--sccache-stats` prints `sccache --show-stats` after the build; `sccache --zero-stats` resets the counters.
- **After changing the sccache setting** the next run configures again, because the configure arguments changed.

The tests use a fake `sccache` program on PATH and a fake cmake, so they check which arguments reach CMake. How many compiles a
real cache saves depends on your project; read the stats.

## How it is tested

```
python -I tools/buildlock/run_tests.py       # prints ALL OK only if every check passes and every twin is caught
```

26 checks and 22 mutation twins; it takes about 45 seconds. No compiler runs. The build driver runs against a fake cmake
(`tests/fake_cmake.py`) that records what it was asked, and the lock runs with threads, with short-lived stand-in processes that the tests
start and stop themselves (only by pid, and only while the pid is still the same process), and through the command line.

- **Lock:** six threads never hold it together; a timeout names the holder; only the holder releases; the heartbeat notices a lock taken
  away; a dead holder, a reused pid, an expired lease and a lock held too long are recovered; a live heartbeat keeps the lock past
  three lease periods; a waiter does not jump the queue and tickets of dead processes and of waiters that stopped waiting are dropped; waiters get the lock in the order
  they came; a break needs the token that was judged stale and the break guard; a new lock folder without `lock.json` is respected for a
  moment and then stale; the context manager releases after an exception.
- **Command line:** exit codes pass through, a busy lock is exit 3, three real jobs started half a second apart run one after the
  other in that order, the lock of a killed holder is recovered, `status` and `break`.
- **Build driver:** the lock is held while cmake runs and released after a failed configure and a failed build; a tree with nothing to do
  returns without waiting for a held lock; the tree is configured again only when the arguments change; sccache is used, forced and
  refused as described above, including the Visual Studio generator; stray toolchain variables are removed; the Visual Studio environment
  (a fake `vcvars` batch file) reaches cmake; `--dry-run`, `--configure-only`, `--clean`, `--target`.
- **Twins**, each applied from outside: a lock that is always granted, release by anyone, a heartbeat that never notices or never runs,
  a process that is always alive, a reused pid not noticed, a lease that never expires, no maximum hold time, no queue, tickets that never go stale, a break that
  ignores the token, no grace for a new lock, a release that does nothing, a driver that never takes the lock, one that always
  waits, a configure that never repeats, missing sccache arguments, sccache used when off, a dirty environment, a Visual Studio environment
  that is ignored.

## Files

`buildlock.py`, `build.py`, `run_tests.py`, `tests/` (`fake_cmake.py`, `fake_job.py`).

Generalised from fnv-coop commit e85460f (github.com/cranebeaker/fnv-coop): `tools/build/build.py`. What changed: the lock is a lease with a heartbeat,
an owner, a queue and recovery by pid and start time instead of a bare `mkdir` that retries every 15 seconds and never recovers; the Visual
Studio path is found with `vswhere` instead of being written into the script; sccache is an option with the MSVC settings it needs; the
configure step is repeated when its arguments change; nothing is specific to one project (the superbuild path, the line-break repair step
and the precompiled header switch of the original are gone).
