# Scenarios: jump to a starting state, then test

Co-op tests need a starting state: "both players stand at the raider camp, three raiders are alive". Reloading a save for
every test costs a minute or more. This tool describes the starting state and the test as one JSON file, checks the file
without a game, and then gets every game instance into that state by resetting in place, reloading the save only when
it has to.

It is game independent. The game side is one small class (the adapter) that you write for your game.

* [SCHEMA.md](SCHEMA.md): the file format (objects, steps, checks, jump rules).
* `scenlib.py`: loading, validation, jump planning, comparison. Pure functions, standard library only.
* `runner.py`: runs a scenario over adapters, keeps what is loaded on each instance, reports `pass`, `fail`, `error`, `partial`, `invalid`. Also the event-driven sync waits and the popup clearing.
* `gameadapter.py`: the interface you implement, with the optional hooks.
* `prepare_save.py`: command line that makes a prepared save (load popups already cleared) and shows what is prepared.
* `fakegame.py`: an in-memory fake of N instances sharing one world, with switchable bugs and switchable hooks. For tests and dry runs.
* `scen.py`: command line (`list`, `validate`, `plan`, `run`).
* `examples/fnv/`: four Fallout: New Vegas scenarios and a small console adapter for it, including the optional hooks.

## Quick start

```
python -I tools/scenarios/scen.py list
python -I tools/scenarios/scen.py validate
python -I tools/scenarios/scen.py plan ownership_handoff --prev fight     # would this jump reload the save?
python -I tools/scenarios/scen.py run fight loot_race quest_stage_sync --fake    # dry run, no game
```

Then write your own file next to the examples (copy one), and run it on your game:

```
python -I tools/scenarios/scen.py --dir my_scenarios run my_scenario --adapter my_game:make_adapter --players 2
```

`make_adapter(i)` returns the `GameAdapter` for game instance `i`. Instance 0 is the host. The tool never starts a game;
you start the instances and join them into one session first.

## How to write an adapter for your game

Subclass `GameAdapter` (see `gameadapter.py`) and fill in five things. The optional hooks come after that.

```python
from gameadapter import GameAdapter, ScenarioError

class MyGame(GameAdapter):
    OPS = {"op:kill", "op:move", "op:enable", "check:dead", "check:state"}   # what you can do; leave it out to claim everything

    def __init__(self, conn):                  # conn: however you reach ONE running game instance
        self.conn = conn

    def do(self, op, args):                    # op "kill", args {"ref": "<id from the scenario>"}
        if op == "kill":
            self.conn.run_script("kill %s" % args["ref"])
        elif op == "move":
            self.conn.run_script("teleport %s %s" % (args["who"], args["to"]))   # who may be "player"
        else:
            raise ScenarioError("op %s is not implemented" % op)

    def read(self, kind, args):                # a number. 1 and 0 for true and false
        if kind == "dead":
            return float(self.conn.query("is_dead %s" % args["ref"]))
        raise ScenarioError("check %s is not implemented" % kind)

    def load_save(self, name):  self.conn.load(name)
    def save_game(self, name):  self.conn.save(name)
    def reset_state(self, ids): ...            # put these quests or flags back to their start value, no reload
```

Rules that keep the results honest:

1. Raise `ScenarioError` when the game refuses something (a console parse error, a missing object). The step becomes `error`,
   never a silent pass.
2. Leave out what you cannot do. The step is then `blocked`, and the scenario ends `partial`. A scenario that cannot be run is
   not a scenario that passed.
3. `read` must ask the instance it belongs to. Every check runs on each player it targets, so a bug that only shows on the
   guest is found.
4. `reset_state` must really reset. If it cannot reset something, leave that object out of the scenario or set `start.reload`.
5. Add a validation hook if you have game data: `scenlib.validate(sc, hooks=[check_ids_exist])`. The hook gets the scenario and
   returns error strings, for example "id GSPG09Ref is not in the dump". For the common case (does this id exist, and as
   what kind) return a data index from `data_index()` instead, see "Check ids against the game data" below.

`examples/fnv/fnv_game.py` is a complete, small adapter for a game with a script console: each op becomes one console line,
each check one expression. Read it first.

## Optional hooks

Each hook has a default that means "this game cannot tell", and the runner then keeps its older behaviour. Implement the ones
your game can answer. The names and shapes are the contract; other tools may call them too.

| hook | returns | used for |
|---|---|---|
| `sync_status()` | `dict` or `None` | event-driven sync waits (next section) |
| `popup_count()` | `int` or `None` | number of blocking popups open now; `None` = cannot tell |
| `dismiss_popup()` | `bool` | press away one open popup; `True` when one was pressed |
| `prepared_names(save)` | `[str]` | the names a prepared copy of `save` is tried under, best first (default `["coop_<save>_ready"]`) |
| `save_exists(name)` | `bool` or `None` | is this save file there |
| `save_stamp(name)` | JSON value or `None` | size and time of a save, to notice that a source save changed |
| `ROLLING_SAVES` (attribute) | tuple of names | saves the game rewrites itself (autosave, quicksave); their stamp is not checked |
| `data_index()` | `dict`, `DataIndex` or `None` | ids the game data knows, for checking scenarios without a game |

A hook that raises `NotImplementedError` or `ScenarioError` counts as "cannot tell".

### Event-driven sync waits

After a reload every client rejoins the host's session. Waiting a fixed time is either too long or too short, so the runner
reads `sync_status()` of every instance and returns the moment the condition holds: every client joined, applied what the host
committed, and the host judged one more state digest ok (a digest is a cheap checksum of the shared state that clients send). The
adapter answers for its own instance, every key optional:

```
{"role": "host" | "client" | None,
 "head": 120,                  # host: how far the committed stream is (a number that only grows)
 "applied": 120,               # client: how much of it was applied (same unit)
 "joined": true, "syncing": false, "connected": true, "rejected": null,    # client
 "digests": {"received": 4, "ok": 3, "mismatch": 1, "stale": 0, "rateLimited": 0, "repairs": 1},   # host
 "desyncs": [{"id": 7, "key": "state:q1", "host": "25", "client": "0", "repaired": true}]}         # host, id = report number
```

It also fails fast, with a plain reason, when sync stops making progress:

* a client that rejected the join: at once, with its text;
* client progress (joined, applied, baseline, link) that stood still for `stall_s` (default 30 s): the reason names each client
  ("client 0: applied 50 of 100", "not joined", "link not connected");
* no digest for `digest_stall_s` (default 20 s), `stale_limit` stale digests (default 3), or the same mismatch `repeat_limit` times
  (default 3, the same keys and values: the repair is not working). The result lists the mismatching `desyncEntries` and the
  `digestDelta` counters;
* `timeout_s` (180 s) and `digest_timeout_s` (60 s) are the outer limits.

All limits are parameters: `Runner(..., sync={"stall_s": 10, "repeat_limit": 2})`, or `runner.wait_synced(stall_s=10)`. `stall_s` 0
turns the stall exit off. The result is in `jump.coopSync` of a run log: `synced`, `seconds`, `reason`, `digestOk`, `settleReason`,
`settleSeconds`, `settleRepairs`, `digestDelta`, `desyncEntries`. A setup step that moves, spawns or sets state also waits (up to
10 s, stall 5 s) until the clients applied it, and a step record gets `syncWait` when that did not work out. A game whose adapter
returns `None` keeps the old behaviour: a fixed pause (`load_wait_s`, `settle_s`) and no `coopSync`.

### Prepared saves

Some games raise many message boxes after a load. An open box can keep the game in menu mode, so test input is lost, and clearing
them takes a long quiet window. `prepare_save.py` clears them once and saves the result under a ready name on every instance:

```
python -I tools/scenarios/prepare_save.py fight --adapter my_game:make_adapter                 # prepare the start save of a scenario
python -I tools/scenarios/prepare_save.py --save autosave --adapter my_game:make_adapter --instances 2,3
python -I tools/scenarios/prepare_save.py --status fight [--adapter my_game:make_adapter]      # what is prepared; no game needed
```

It uses the same `--adapter pkg.mod:factory` as `scen.py run`. `--instances` lists the ids handed to the factory (default `0,1`),
the equivalent of pointing the tool at copies by pipe or port number: a second pair of copies is `2,3`, and its prepared saves
are kept under those ids. It loads the save, presses popups away through `popup_count()` and `dismiss_popup()` until none has shown
for 20 s, saves under `prepared_names(save)` (a later name is tried when the engine refuses the first), then loads the ready save
and checks that no popup comes up. It prints one JSON line per save with `ok`, and exits 1 when the popups never go quiet, a copy
refused the save, or the reload check still shows a popup. Exit 2 is a usage problem (no adapter, an adapter without popup hooks).

The facts are kept in one JSON file: `./scen_prepared.json`, or `$SCEN_PREPARED_FILE`, or `--prepared FILE` (`scen.py run` takes
the same option). They are keyed by instance id and start save. A prepared save is used by a jump only when the adapter says its
file is there (`save_exists`), the reload check passed, and the source save has not changed since (`save_stamp`; saves in
`ROLLING_SAVES` are exempt). Otherwise the plain save loads, and the run log says why in `jump.loadedSaves[].why`. When every
copy loaded a prepared save the popup quiet window shrinks from 20 s to 4 s, as a safety net. The jump plan still sees the plain
start save, so a prepared copy never looks like a different save.

Opt out with `SCEN_NO_PREPARED=1` in the environment, or `"start": {"prepared": false}` in a scenario file.

### Check ids against the game data

`data_index()` returns the ids your game data knows, as `{"<id>": "<kind>"}` (kind is an object kind, a list of kinds, or `None` for
any), or a `scenlib.DataIndex(entries, kinds=None, case_sensitive=True, label="the game data")`. `Runner.run` then rejects a scenario
before any game call when an object's id is unknown or is there under another kind. Every object of the glossary is checked, and the
error says where it is used, including setup steps (move, give, enable, spawn and the rest):

```
objects.raider2: id 'GSPG02Ref' is not in the game data (used by setup step enable_raider2 (enable), step all_alive_2 (check));
nearest known id: 'GSPG02Rex' (also close: 'GSPG03Ref', 'GSPG01Ref')
```

A `base` on an object is checked as an item or spawnable type. `kinds=` limits the index to the kinds it can speak for. A game
without an index skips the check, and the run result says so in `notes`. Without a game:

```
python -I tools/scenarios/scen.py validate fight --index ids.json                 # ids.json is {"id": "kind"}
python -I tools/scenarios/scen.py validate fight --adapter my_game:make_adapter   # uses the adapter's data_index()
```

Game specific console lines and ids stay in your adapter. `examples/fnv/fnv_game.py` shows an index built from record types
(`records={"GSPG01Ref": "ACHR"}`), and the other hooks.

## How it is tested

```
python -I tools/scenarios/run_tests.py
```

Prints `ALL OK` only when every test passes and every twin is caught. No game is started.

* 120 unit tests: the validator (every kind of broken file), id resolution, op and check translation, the comparison table,
  the jump rules, the runner against `fakegame.py` (including a guest that never sees changes, duplicated loot, double
  spawn, a missing ownership handoff, blocked steps, console errors, invalid files never reaching the game, in-place versus
  reload jumps, and that an in-place jump costs fewer than 20 adapter calls), the example console adapter against a
  recording bridge, and the command lines. 61 of them cover the three newer features, all on the fake game (virtual time, no
  real waiting): the sync waits (a healthy wait ends at the first ok digest, a stuck or silent client, a rejected join, a dead
  link, stale and repeated mismatches, a mismatch that heals, limits as parameters, no hooks keeps the fixed pause), prepared
  saves (prepare, use, stale source, a failed reload check, a refused or missing file, opt-outs, instance ids, exit codes) and the
  data index (unknown id with the nearest match, wrong kind, setup ops, base ids, covered kinds, case, the runner, the command line).
* 20 mutation twins. The suite is run again with one behaviour broken (`SCEN_MUTANT`). The first five: no alias checking, jump
  never reloading, scenarios forgetting what they changed, every check passing, every step running only on the host. Five more
  for the sync waits (a stuck client never given up on, a digest wait that never ends early, a success noticed only at the time
  limit, the first mismatch ending the wait, a failure without the mismatch entries), seven for prepared saves (never loaded,
  long quiet window, stale source used, unverified save used, no reload check, saved before the popups were pressed, instances
  recorded by position) and three for the index (never consulted, no nearest match, wrong kind accepted). Each must make a
  test fail, and the run prints `caught` for each.

Updated from fnv-coop commit e85460f.

## Ported from fnv-coop commit f57aa78

Ported from the Fallout: New Vegas co-op project ([cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop), commit f57aa78).
The format, validator, jump planner, runner and fake were made game independent: form ids and plugin load orders became
free-form ids, the data dump checks became validation hooks, and the Fallout console lines moved into the example adapter.
