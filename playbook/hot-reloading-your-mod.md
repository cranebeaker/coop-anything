# Hot reloading your mod: swap the DLL without restarting the game

A fix round for a co-op mod that needs a game restart costs minutes, and almost none of that is
your code: it is booting the game, loading a save and getting back to the spot that failed. If the
mod is a DLL inside the game process, you can skip all of it. Keep a tiny loader in the game, put the
rest of the mod in a second DLL, and swap that second DLL while the game runs.

Code and tests: [tools/hotreload](../tools/hotreload/README.md). Related:
[fast-iteration.md](fast-iteration.md) (the whole fix round this is one part of),
[multi-instance-testing.md](multi-instance-testing.md), [automated-qa.md](automated-qa.md),
[desync-hunting.md](desync-hunting.md).

**Only for your own mod in your own copy of a game.** Never use this against an online game that has
anti-cheat. Test on a copy of the install, and do not swap into a game your owner is playing.

## When it works

- The mod is a native DLL loaded into the game process (a script extender plugin, an ASI plugin, an
  injected library). Mods that are only scripts or data files already reload without any of this.
- You can put a call in your plugin entry point that runs once per frame on the game's main thread.
- You are willing to keep a clean boundary: what the game or a library keeps pointing at must not
  point into the code you swap.

If your mod patches game code, it still works, but you must be able to put the original bytes back
and take your trampolines out of use (see "Mods that patch code" below).

## The pattern

```
 game process
 +------------------------------------------------------------+
 |  loader (stays)            core DLL (swapped)              |
 |  - plugin entry point      - your mod's logic              |
 |  - control pipe            - your threads, hooks, sockets  |
 |  - cache copy of the core  - saves and restores its state  |
 |  - state blob store        - drains before it unloads      |
 +------------------------------------------------------------+
        ^                                   ^
   hotreload.py swap  ---  copies the new DLL over the core file, asks the loader to reload
```

1. **A permanent loader.** Small, almost never changes, and is never unloaded. A change to the
   loader needs a normal restart.
2. **A reloadable core.** The loader copies the core to a private cache file and maps the copy. That
   is what lets your build overwrite the original: Windows does not allow it for a loaded DLL.
3. **Versioned state.** Everything that must survive is written by "participants" into a blob the
   loader holds in memory, and read back by the next build. Each participant has a layout version.
   An older layout is converted by a `migrate` step or the participant starts fresh and says so.
4. **A drain.** Before the unmap, wait until no thread runs core code. A thread counts as inside
   when its instruction pointer is in the core image or any return address on its stack points
   into it. The threads the core owns are excluded, because their owner joins them after the
   drain.
5. **A build id.** CRC-32 and size of the mapped copy. The swap tool computes the same for the file
   it copied, so "the build I just made is the one running" is checked.
6. **A refusal that is clean.** A swap that cannot be done safely stops before anything is unloaded.
   The old core keeps running with all its state, and the tool says why.

## The rule that bites: every thread the core owns must be registered

The first time you try this on a real mod, the drain will refuse, and the cause is almost always a
thread. A worker that sits parked waiting for jobs has its instruction pointer in the kernel, but its
stack holds your code's return addresses, so it counts as inside the core. The drain waits for it to
leave, it never does, and every reload times out.

The fix is not to weaken the drain. Tell it which threads you own and will stop yourself:

- Start your own threads through `hotreload::ManagedThread`. They are registered, stopped and joined
  for you.
- For a thread you cannot start that way (a library you link in started a worker), call
  `hotreload::adoptThread(tid)` when it starts and `releaseThread(tid)` after you joined it in the
  participant's `shutdown`.
- Anything else that runs your code, a callback on an engine thread for example, is inside while the
  call runs, and the drain correctly waits for it.

The refusal is clean. The report says `drain timed out ... thread N is still inside the code that
would be unloaded: return address ... points into the core image`, and the mod keeps working. The
test host in the tool has a core built with exactly this mistake, and a test that checks the refusal.

## The other mistakes, in the order they show up

| Mistake | What you see | Fix |
|---|---|---|
| State kept in a static that is not registered | the counter, the session, the loaded quest list resets after a swap | add a participant that saves and restores it |
| Changed the saved layout without bumping the version | garbage after the swap, or a `restore` that reads past its payload | bump `version`, add `migrate`, or accept a fresh start |
| `save` that changes state | a refused reload leaves the old core half modified | `save` must only read |
| A hook that is not removed in `shutdown` | the unload refuses (leak probe), or the game crashes after it | remove it, and add a leak probe for installed hooks |
| A socket closed in `shutdown` that the new core wants | peers disconnect on every swap | store the handle as an integer in the blob and leave it open |
| Another mod chained over your hook | "hook was chained over by another mod" in the refusal | return that reason from `canUnload`; do not unload |
| A pointer from the loader or the game into the core | crash right after the unmap | register through the loader API only, so the loader removes it before the unload |
| `std::string` or an exception across the loader and core boundary | crashes that depend on the build | plain C types only at that boundary |
| Static destructors that join threads | the game hangs on exit | allocate long-lived thread objects on the heap and let the OS end them |
| Building straight over the live core file | the swap tool refuses (`--dll` is the live core) | build to another folder, let the tool copy it |

## Mods that patch code

The reload glue cannot know your hooks. Three callbacks cover them (`hotreload::SetDrainHooks`):

- `suspend`: put the original bytes back so nothing can enter a detour any more. Return how many hooks
  could not be taken off.
- `extraRanges`: your trampolines and generated code, so a thread inside them counts as busy.
- `resume`: put the hooks back when the drain refuses, so the core keeps working.

Order matters. Hooks come off first, then the drain runs, then the participants shut down in reverse
order, then the leak probes are read. Register a leak probe for installed hooks so that an unremoved
one makes the unload refuse instead of crash.

## Steps for an agent adding this to a mod

1. Read [tools/hotreload/README.md](../tools/hotreload/README.md) and
   [examples/core/example_core.cpp](../tools/hotreload/examples/core/example_core.cpp).
2. Build the tool and run its tests: `python -I tools/hotreload/run_tests.py`. They need no game.
3. Find the plugin entry point. Decide what stays in the loader: the entry point, the per-frame call,
   the message forwarding. Everything else is the core. Keep a short note of every global, thread,
   hook and socket you find while moving code; each one becomes a participant, a `ManagedThread`
   (or `adoptThread`), a hook with a leak probe, or a handle in the blob.
4. Build the split mod (a loader and a core). Start a **copy** of the game once with the loader.
5. Run `python -I tools/hotreload/hotreload.py swap --pipe <name> --dll <core> --dry-run`. Exit 2 says
   what is missing.
6. Swap for real, then read the report. Check `reload.state` for failures and fresh starts, and the
   log in the cache folder if the drain refused.
7. Write a bot or script that checks the state that matters across a swap (a counter, a session, an
   ownership table), and keep it. See [automated-qa.md](automated-qa.md).
8. Write the loader/core split, the participants and the thread list in your game's write-up, so the
   next agent does not rediscover them.

## The round

```
build the core only                 (seconds, not a full rebuild)
hotreload.py swap --pipe ...        (the game keeps running; a state blob goes across)
run the test or the bot             (no save load, no menu)
read the failure reason, fix, repeat
```

Run the whole test, not a partial one, before you call a fix done. A change to the loader, to the
boundary header, or to anything the loader keeps still needs one restart.

## Proving it

```
python -I tools/hotreload/run_tests.py
```

Builds the loader, the libraries, six example cores and a test host that stands in for the game, runs
the unit tests, runs the swap scenarios (state carried over, refusals, rollback, an unregistered
thread) and checks every mutation twin. It prints `ALL OK` only when all tests pass and every twin is
caught. The tests use the stand-in host, so for your own mod add the check that matters to you: the
state you care about, across a swap, in your game.
