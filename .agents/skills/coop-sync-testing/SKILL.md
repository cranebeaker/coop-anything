---
name: coop-sync-testing
description: Build and test co-op sync with the hub's ready-made tools in tools/: an in-game RPC bridge and live inspector, hot reload of the mod core inside running games, a UDP chaos box for lag and loss, a flight recorder that finds the first desync and shrinks it to a regression test, a seeded co-op fuzzer with invariant oracles, an actor-ownership library with epochs, a fake world model, a lock and warm pool for running several game copies, JSON scenarios, co-op bots with playbooks, and an animation sync checker.
---

# Co-op sync testing with the hub tools

Each tool lives in `tools/<name>/` with a README, its tests, mutation twins, and one command
that prints `ALL OK`. Run them all with `python -I tools/test_all.py`. They are generalized
from the fnv-coop project; game specifics go in a config file or an adapter you write.

| Need | Tool | Playbook |
|---|---|---|
| Lag, loss, reorder, duplicates, partitions between real clients | `tools/chaos` | `playbook/network-chaos-testing.md` |
| Find the first divergence between 2 to 4 machines, shrink it, emit a regression test | `tools/flight` | `playbook/desync-hunting.md` |
| Thousands of seeded N-player sessions checked by invariants | `tools/fuzz` | `playbook/fuzzing-coop-sync.md` |
| One owner per NPC, epochs, handoff without duplicates | `tools/ownership` | `playbook/actor-ownership.md` |
| Test sync without launching the game | `tools/fakeworld` | `playbook/fake-world-testing.md` |
| Run several game copies safely, reuse warm ones | `tools/gameslot`, `tools/gamepool` | `playbook/multi-instance-testing.md` |
| Known starting states, reset in place | `tools/scenarios` | `playbook/bots-and-playbooks.md` |
| Multi-bot co-op runs with a state diff after each step | `tools/bots` | `playbook/bots-and-playbooks.md` |
| Remote players animate and stand where they should | `tools/animcheck` | `playbook/animation-sync-checks.md` |
| Control the game from tests (console, state, save, load, screenshot) and watch every copy live | `tools/bridge`, `tools/inspect` | `playbook/in-game-bridge.md` |
| Pick up a code change without restarting the game | `tools/hotreload` | `playbook/hot-reloading-your-mod.md` |
| A twin for every fix; several agents sharing one build tree | `tools/mutants`, `tools/buildlock` | `playbook/fast-iteration.md` |
| Story sync: send effects, not script replays | design only | `playbook/effect-capture-story-sync.md` |

Order that works:
1. Model the world first (`fakeworld` pattern) and wrap your sync core in the fuzzer's target interface. Fix every oracle failure at the root, never by loosening the oracle.
2. Add the flight recorder to the mod on every machine. Every desync report becomes a shrunk trace and a regression test.
3. Use ownership with epochs for anything simulated (NPCs, physics objects).
4. Then real clients: `gameslot` plus `gamepool` to run copies, `scenarios` for start states, `bots` playbooks for the co-op actions, with `chaos` between the machines and `animcheck` on the probe streams.
5. Every fix ships with a test and a mutation twin that the test must catch.
6. Keep the in-game round short (`playbook/fast-iteration.md`): swap the core instead of restarting, prepared saves, event waits, `--from-step`, `--timescale`, parallel pairs, failure reasons that name the cause.

Never launch a game copy without the slot, never kill a process you did not start, and never touch the owner's own install or saves.
