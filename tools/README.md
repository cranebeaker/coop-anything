# Tools

Two kinds of tools live here:

- **Hub tools:** `build_directory.py` (validates `data/directory.yaml` and renders
  `DIRECTORY.md`) and `mcp/` (the [MCP server](../playbook/mcp.md)).
- **Sync test tools:** game-agnostic tools for building and testing co-op sync, listed below.
  They come from the [fnv-coop](https://github.com/cranebeaker/fnv-coop) project, with the
  game specifics moved into config files, adapters and `examples/fnv/` folders.

## Sync test tools

| Tool | What it does | Language | Playbook |
|---|---|---|---|
| [chaos](chaos/README.md) | UDP relay between a host and clients of any game: delay, jitter, loss, bursts, reorder, duplicates, bandwidth caps, one-way cuts. Profiles `lan` to `hell`, changed live. A header-only C++ mirror gives the same decisions for in-process tests. | Python, C++ header | [network-chaos-testing](../playbook/network-chaos-testing.md) |
| [flight](flight/README.md) | Flight recorder for your mod (`.cofr` traces of messages, txns, ownership and state digests) plus an analyzer that lines up 2 to 4 machines, finds the first divergence down to bucket, key and value, shrinks the run and writes a regression test and an HTML report. | C++, Python | [desync-hunting](../playbook/desync-hunting.md) |
| [fuzz](fuzz/README.md) | Seeded 2 to 4 player sessions over a lossy link, checked by invariant oracles (converge, no double apply, one owner per epoch, no dupes, nothing stale, repairs stop). Shrinks failing seeds. Wrap your sync core in its target interface. | C++, Python CLI | [fuzzing-coop-sync](../playbook/fuzzing-coop-sync.md) |
| [ownership](ownership/README.md) | Which machine simulates which actor: epochs, grant, transfer, release, hysteresis, the follower exception, stale and echo rejection. No dependencies. | C++ library | [actor-ownership](../playbook/actor-ownership.md) |
| [fakeworld](fakeworld/README.md) | Example in-memory game world (actors, cells, containers, doors, spawns, combat, quest stages) on a virtual clock, to test sync without the game. | C++ | [fake-world-testing](../playbook/fake-world-testing.md) |
| [gameslot](gameslot/README.md) | Lease lock so only one lane runs game copies at a time, with a RAM guard and kills scoped to the processes you started. | Python | [multi-instance-testing](../playbook/multi-instance-testing.md) |
| [gamepool](gamepool/README.md) | Isolated hardlinked installs, a warm pool of reused copies, low-cost minimized launches, readiness probes and crash log reading, from one JSON config per game. | Python | [multi-instance-testing](../playbook/multi-instance-testing.md) |
| [scenarios](scenarios/README.md) | JSON starting states plus checks; resets in place and reloads a save only when it has to. | Python | [bots-and-playbooks](../playbook/bots-and-playbooks.md) |
| [bots](bots/README.md) | Multi-bot co-op playbooks (fight, loot race, container, door, dialogue, consent vote, quest sync, fast travel, ownership handoff, save and load rejoin) with a state diff after every step, over an action adapter you write for your game. | Python | [bots-and-playbooks](../playbook/bots-and-playbooks.md) |
| [bridge](bridge/README.md) | One RPC server inside the game process: a header-only C++ template with a main-thread request queue and a method registry (exec, eval, waitFor, state, save, load, screenshot, events), plus a Python client over a named pipe or TCP. | C++ header, Python | [in-game-bridge](../playbook/in-game-bridge.md) |
| [inspect](inspect/README.md) | Read-only live table of every game copy side by side (menu, weapon, health, position, owners, sync counters, recent desyncs), with columns from a per-game field list. | Python | [in-game-bridge](../playbook/in-game-bridge.md) |
| [hotreload](hotreload/README.md) | A permanent loader DLL plus a reloadable core: versioned state across the swap, a drain that waits for core threads, build ids, clean refusal with exit 2. | C++, Python | [hot-reloading-your-mod](../playbook/hot-reloading-your-mod.md) |
| [mutants](mutants/README.md) | Mutation twin harness: a zero-cost runtime switch (`COOP_MUT`) for C++ and Python and a runner that reports twins missed, catching the wrong test, or breaking the build. | C++ header, Python | [fast-iteration](../playbook/fast-iteration.md) |
| [buildlock](buildlock/README.md) | Lease lock for a shared build tree (stale lock recovery by pid) and a CMake driver with optional sccache. | Python | [fast-iteration](../playbook/fast-iteration.md) |
| [animcheck](animcheck/README.md) | Compares animation, pose, weapon and facing across machines and flags sliding, T-pose, floating, sinking, jitter, rubber-banding, stuck and death-pose glitches. Reads a JSON probe stream. | Python | [animation-sync-checks](../playbook/animation-sync-checks.md) |

The design behind story sync (capture effects on the host instead of replaying scripts) is in
[effect-capture-story-sync](../playbook/effect-capture-story-sync.md).

## Running the tests

Every tool has one command that runs its tests and its mutation twins (deliberately broken
variants that the tests must catch) and prints `ALL OK`:

```
python -I tools/chaos/run_tests.py
python -I tools/test_all.py            # every tool
python -I tools/test_all.py fuzz bots  # some of them
```

The C++ tools build with CMake and need a C++17 compiler (they were built with Visual Studio
2022 on Windows). The Python tools use only the standard library. No test starts a real game:
they use fakes and stand-in processes.

Use the `coop-sync-testing` skill to have your agent pick the right tool.
