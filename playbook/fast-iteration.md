# Fast iteration: one in-game fix round in a minute or two

A fix round for a co-op mod is: change the code, get it into two running game copies, put the
world in the state that failed, run the test, read why it failed. Done by hand this easily takes
15 minutes. Almost none of that is CPU time: it is booting, reloading saves, clicking away
popups, fixed waits and working out what went wrong. A faster language does not help. Removing
each wait does.

A typical round from the fnv-coop project, and what replaces each part:

| Part of the round | By hand | With the hub tools |
|---|---|---|
| Rebuild the mod | 1.5 min | Build only the reloadable core, with a build cache ([buildlock](../tools/buildlock/README.md)) |
| Restart both game copies | 4 min | Swap the core inside the running games ([hotreload](../tools/hotreload/README.md)) |
| Load the save, dismiss popups | 1 min | A prepared save with the popups already cleared ([scenarios](../tools/scenarios/README.md)) |
| Fixed sync waits | 2 min | Event-driven waits that stop early when sync stops making progress ([bots](../tools/bots/README.md)) |
| Walk there and set up | 1 min | `--from-step N`: every step can arrange its own world state |
| The test itself | 2 min | `--timescale` through the game's time multiplier |
| Working out why it failed | 3+ min | Failure reasons that name the cause, a live side-by-side view ([inspect](../tools/inspect/README.md)), and an offline repro of the failure ([flight](../tools/flight/README.md)) |

The target is a minute or two per round.

## The pieces

1. **A bridge into the game.** One small RPC server inside the game process, a request queue
   drained on the game's main thread, and a method registry (run a console line, evaluate, wait
   for a condition, dump state, save, load, screenshot). Every other tool talks to the game through
   it. See [in-game-bridge.md](in-game-bridge.md) and [tools/bridge](../tools/bridge/README.md).
2. **Split the mod into a loader and a core.** The loader stays in the game for good; the core is
   mapped from a cache copy and can be swapped while the game runs. Modules save and restore
   versioned state across the swap. A drain waits until no thread runs core code. Every thread the
   core owns must be registered, or the drain refuses forever. See
   [hot-reloading-your-mod.md](hot-reloading-your-mod.md).
3. **Several copies from one install.** Hardlinked copies, one INI and port set per copy, host and
   client pairs with their own ports, and no more pairs than free RAM allows
   ([gamepool](../tools/gamepool/README.md) `--pairs N`).
4. **Bots whose actions explain themselves.** Every action returns `ok`, a `reason` and data, with
   pre- and postconditions: "menu open", "weapon holstered", "stuck, 4.2 m left", "target too
   steep". A failed run should tell you what to fix without opening the game.
5. **Resumable playbooks.** Each step has an idempotent arrange part, so `--from-step` and
   `--only-step` jump straight to the failing step. Settle waits fail fast with a reason.
6. **Parallel pairs.** One runner per pair as subprocesses, prefixed logs, one PASS/FAIL table
   (`tools/bots/run_parallel.py`).
7. **Offline repro.** `tools/flight/from_failure.py` turns a failed in-game run into an offline
   regression case: shrink it, fix it without the game, keep the test.
8. **Twins for everything.** Each behaviour has a test and a mutation twin, a runtime switch that
   puts the bug back, and the test must fail with it on ([mutants](../tools/mutants/README.md)).

## Rules that keep the loop fast

- Never restart a game copy to pick up a core change. If a copy cannot swap, the swap tool says why
  and exits with code 2; fix that once, then keep swapping.
- Never add a fixed sleep. Wait for an event, and give up early when nothing moves.
- A failure without a reason is a bug in the test. Add the reason before fixing the mod.
- Never touch game copies another agent or session is using. Take the
  [gameslot](../tools/gameslot/README.md) lease first.
