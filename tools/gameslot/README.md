# gameslot: a lease lock for running game copies

Several agents, or several scripts of one agent, want to run copies of the same game on one PC. Without
a lock they start the game at the same moment, pick the same instance folder or port, run the PC out of
RAM, or kill each other's games. `gameslot` is a small lease lock that prevents all of that.

- **Slots.** The machine has N slots (default 2). An owner takes one or more slots before it starts a game,
  and frees them after it killed its games. The slot numbers are yours: derive your instance folder, port
  and pipe names from them, and two owners can never collide.
- **Leases expire.** A lease lasts `--ttl` seconds (default 900) and is renewed by asking again with the same
  owner. A crashed owner does not block the PC for ever.
- **First in, first out.** Waiters queue, so a request for many slots is not starved by a stream of small ones.
- **RAM guard.** A request that would start a second game while free RAM is below `min_free_gb` (default 3) is
  refused. The first game and renewals are never refused.
- **Pid-scoped kills.** An owner registers every game process it starts (`register-pid`). If the lease later
  expires and someone takes it over, only those registered processes are killed, and only if the pid still
  has the same image name and creation time. A reused pid is a different process and is left alone. Nothing
  here ever kills "every process with the game's name", because the owner of the PC may be playing the game.
- **One lock for state changes.** Every change happens under a short-lived lock file, so two processes can
  never both win the same slot.

Python standard library only. Windows is the supported platform (process identity uses the Win32 API).

## Quick start

```
python -I tools/gameslot/gameslot.py acquire --owner agent1 --slots 2 --ttl 1800 --wait 600
#  OK lease until ...; SLOTS 0,1          <- instances 0 and 1 are yours

python -I tools/gameslot/gameslot.py register-pid --owner agent1 --pid 1234     # for every game process you start
python -I tools/gameslot/gameslot.py status                                      # JSON: every slot and its lease
python -I tools/gameslot/gameslot.py release --owner agent1                      # after you killed your games
```

Options of `acquire`: `--slots K` (default 1), `--exclusive` (all slots), `--ttl SECONDS`, `--wait SECONDS`
(how long to queue; 0 means fail at once). Exit codes: 0 ok, 2 bad use, 3 busy, 4 not the owner.

Python callers can import the module: `acquire`, `release`, `register_pid` and `status` are plain functions,
and `main(argv)` is the command line.

## Config

State lives in `GAMESLOT_DIR` if set, else `<repo>/local/gameslot` (git ignores `local/`). An optional
`gameslot_config.json` in that folder:

```json
{"slots": 2, "min_free_gb": 3.0, "process_name": "Game.exe"}
```

| Key | Default | Meaning |
|---|---|---|
| `slots` | 2 | How many game copies may run at the same time on this PC |
| `min_free_gb` | 3.0 | A new slot is refused while less RAM than this is free (only when another slot is live) |
| `process_name` | none | Only used by `status`, to list the pids of running game processes. Kills never use it |

Set `GAMESLOT_POLL` (seconds, default 10) to change how often a waiting `acquire` retries.

## How it is tested

```
python -I tools/gameslot/run_tests.py       # prints ALL OK only if every check passes and every twin is caught
```

18 checks and 13 mutation twins at the time of writing (last line of the output: `18 checks, 13 twins, 0 failed`).

- Most checks use an in-memory process table, so nothing real is killed. They cover distinct slots, busy
  answers, all-or-nothing multi-slot requests, renewal, takeover of expired leases, FIFO fairness, the RAM guard,
  recorded processes, pid reuse and the state lock (including a stale lock left by a dead process).
- One check races six real `gameslot.py acquire` processes for a single slot and requires exactly one winner.
- One check starts two harmless stand-in processes (a Python interpreter that sleeps), registers one, lets the
  lease expire, takes it over, and requires that only the registered stand-in died.
- Each **twin** breaks one guard on purpose (every slot always granted, no rollback, leases that never expire,
  no queue, no RAM guard, a blanket kill of every process with the game's name, a kill that ignores pid reuse, a
  lock that is always granted, ...) and runs the same check again. The run fails unless the check notices.

The tests never start a game and never touch processes they did not start.

## Ported from fnv-coop commit f57aa78

Ported from `tools/gameslot.py` and its tests in [fnv-coop](https://github.com/cranebeaker/fnv-coop) (commit
f57aa78). Changes: the game name, executable name and folders were removed (no per-game values are left in
the code); `--slots` now defaults to 1 and `--exclusive` replaces "no flag means all slots"; the compatibility
mirror for an older single-slot tool was dropped; registered processes now carry their image name and creation
time, which makes the pid-reuse guard independent of any game name; the state lock and the twin-based test
runner are new.
