# Co-op bots and playbooks

Co-op breaks in ways one player never sees: two players grab the same item, one player's kill never reaches the other
machine, a quest step fires twice, a rejoining player gets every reward again. These tests run 2 to 4 real game
instances, drive each with a bot, and after every step check that all machines agree.

It is game independent. The runner and the twelve playbooks know nothing about any game. Everything game specific is one
class, the **action adapter**, that you write for your game.

```
playbook (what to do, what must hold)  ->  runner (settle, compare, counters, evidence)  ->  ActionAdapter (your game)
```

* `coopbots/adapter.py`: the adapter interface (`ActionAdapter`, `Result`, `MissingAction`). Read the module docstring.
* `coopbots/core.py`: the runner: steps, settle, quiet windows, repair budgets, snapshot compare, desync counters, failure evidence.
* `coopbots/playbooks/`: the playbooks, written against the adapter only.
* `coopbots/fakecoop.py`: a fake co-op session (a host and 1 to 3 lagging clients) with 30 switchable bugs. It is the test adapter.
* `coopbots/links.py`: options for impaired links. `coopbots/refs.py`: playbook refs from a scenario file.
* `adapters/fnv/`: a small example adapter for a Gamebryo game with a script console.
* `coopbots/results.py`: the action result model (`ActionResult`, `Fail`, `Why`, `ensure`).
* `run_coop.py`: the command line. `run_parallel.py`: one runner per host/client pair. `run_tests.py`: the tests.

## Quick start

No game needed for the first run:

```
python -I tools/bots/run_coop.py --list
python -I tools/bots/run_coop.py --fake --playbook all
python -I tools/bots/run_coop.py --fake --playbook loot_race --bug dup_pickup      # must FAIL: the bug is caught
python -I tools/bots/run_coop.py --fake --playbook quest_sync --link bad_wifi      # lossy link with quiet windows
```

On your game, once you have an adapter (you start the instances and join them into one session; the tool never launches a game):

```
python -I tools/bots/run_coop.py --adapter my_game:make_adapter --players 2 \
    --scenario my_scenarios/fight.json --playbook fight
```

`make_adapter(i)` returns the adapter for instance `i`; instance 0 is the host. `--scenario` is a scenario file from
[tools/scenarios](../scenarios/README.md) whose `refs` block names what the playbook needs (`--refs file.json` does the
same with plain JSON). Exit code 0 means at least one playbook passed and none failed. A `SKIP` (the adapter lacks an
action the playbook needs) is never a pass, so a run where everything skipped exits 1.

## How to write an adapter for your game

One adapter object drives one running game instance. Subclass `ActionAdapter`, implement the engine surface first, then
the actions the playbooks you want need. Anything you leave out raises `MissingAction`, `supports("take")` says it is
missing, and every playbook that needs it is reported `SKIP` with the name of the missing action.

### 1. The engine surface (the runner uses these after every step)

```python
from coopbots.adapter import ActionAdapter, Result

class MyGame(ActionAdapter):
    def __init__(self, conn, name="bot"):
        self.conn, self.name = conn, name

    def status(self):
        s = self.conn.sync_status()                  # whatever your netcode can tell you
        return {"seq": s.last_applied,               # number of the last host update this machine applied
                "synced": s.all_clients_caught_up,   # host only; leave out on clients
                "counters": {"mismatches": s.digest_mismatches, "repairs": s.repairs,
                             "bad_messages": s.bad_messages, "apply_failures": s.apply_failures, "rebases": s.rebases}}

    def snapshot(self):
        return {"shared": self.conn.dump_world(),    # must be EQUAL on every machine once synced
                "per_player": self.conn.dump_me()}   # inventory, position: allowed to differ

    def capture(self, directory):                    # optional: logs for a failed step
        ...
```

* `seq` is how the runner knows the machines converged: it waits until every client reports the host's number and keeps
  it for two polls. If your netcode has no sequence number, use any counter that equals on all machines when nothing is in
  flight (a state hash works).
* `counters` are optional. Use the names `mismatches`, `repairs`, `repaired_effects` (a desync was found and healed) and
  `bad_messages`, `apply_failures`, `rebases` (never acceptable). A counter that rises during a step fails the step.
  Names you add are kept in the records but never fail anything.
* `shared` is compared path by path between the host and each client. Keep per-player or cosmetic things out of it, or list
  them in `snapdiff_ignore` (fnmatch patterns such as `shared/refs/*/in_use*`). An empty `shared` is refused.

### 2. Actions: return a `Result`, check the postcondition

```python
    def take(self, item, count, from_ref):
        before = (self.inventory_count(item), self.inventory_count(item, from_ref))
        self.conn.run("transfer %s %s %d" % (from_ref, item, count))
        after = (self.inventory_count(item), self.inventory_count(item, from_ref))
        if after[0] - before[0] != count or before[1] - after[1] != count:
            return Result(False, "transfer moved %d in and %d out" % (after[0] - before[0], before[1] - after[1]))
        return Result(True)
```

* An ordinary refusal (a locked door, a lost race) is `Result(False, "reason")`. Raise only when the game or the link is
  dead; the runner records a crash as a failed step with a trace.
* Check that the thing really happened, not that the call returned. A bot that reports success because the call went
  through is the main source of hollow passes. The example adapter shows the pattern for `take`, `pickup`, `unlock`, `goto_ref`.
* Every action result is an `ActionResult(ok, reason, data, action, elapsed_s)`; the base class wraps whatever your action
  returns, so you never build one by hand. See "Action results" below for preconditions, postconditions and failure reasons
  that say why.
* Ids (`ref`, `item`, `quest`) are opaque strings. The playbooks only hand back what the scenario gave them.
* The full list of actions and queries, with what each must return, is in the `coopbots/adapter.py` docstring. The playbook
  table below says which ones each playbook needs.

### 3. Run it

Run one playbook with `--adapter`. Fix what `SKIP` and `FAIL` tell you, one action at a time. Then keep the adapter next to
your game's tests. `adapters/fnv/fnv_adapter.py` (under 150 lines) is a complete small example: console lines for the
actions, expressions for the queries, and a status mapping.

## Action results: pre and postconditions, and "say why"

Every action returns an `ActionResult` (a `Result` that also knows its action name and how long it took). A failed one reads
`code: what the bot saw`, for example `stuck: 312 units left (wanted within 150)`. The code is short and stable
(`result.code`), the rest is for the person reading a failed step. `coopbots/results.py` has the model and `Why`, ready
made failures for the cases most games share: `stuck`, `too_close`, `too_steep`, `menu_open`, `message_box`, `holstered`.

```python
from coopbots.adapter import ActionAdapter, Why, ensure

class MyGame(ActionAdapter):
    def blockers(self):                              # what stands in the way of input right now
        out = []
        if self.ui.menu_open():
            out.append(("menu", "the %s menu is open" % self.ui.menu_name()))
        if self.ui.message_box():
            out.append(("message_box", "a message box is up"))
        return out

    def goto_ref(self, ref, dist=150):
        def arrived(data):                           # postcondition: the world really changed
            left = self.game.distance(ref)
            if left > dist:
                raise Why.stuck(left, dist)          # "stuck: 312 units left (wanted within 150)"
        return self.act(lambda: self.game.walk_to(ref), free=True, post=[arrived])
```

* `self.act(do, pre=[...], post=[...], free=False)` runs the checks around the action. `free=True` refuses to start while
  `blockers()` lists anything (use `ignore=("holstered",)` for actions that do not need a drawn weapon). A failed check raises
  `Fail(code, say)` or uses `ensure(cond, code, say)`; the result's data says which `phase` failed: `pre`, `do` or `post`.
* The base class adds the text of the current `blockers()` to every failed action, so a plain `Result(False, "door is locked")`
  becomes `door is locked: a message box is up`. Override `explain(action, result)` for a different note.
* `adapter.strict = True` raises `ActionFailed` instead of returning a failed result (handy in a one-off script).
* `adapter.recent_actions(n)` is the log of the last actions. The runner keeps it in `failure.json` as `recent_actions` per machine.
* An action that returns `None` raises a `TypeError`: return a `Result`. A bare `True` or `False` is accepted.
* A postcondition that is wrong in a deliberate way is how this tool tests itself: `tests/test_actions.py` has an adapter
  whose `take` does nothing, and the postcondition must catch it.

## Fast rounds: resume, fail fast, game speed

A failing step 4 should not cost a rerun of steps 1 to 3.

```
python -I tools/bots/run_coop.py --adapter my_game:make_adapter --playbook fight --from-step 4 --timescale 3
python -I tools/bots/run_coop.py --adapter my_game:make_adapter --playbook fight --only-step 4
python -I tools/bots/run_coop.py --fake --playbook quest_sync --from-step 3        # game free
```

* **Resumable steps.** A resumable playbook runs each step through `ctx.run_step(name, body, arrange=...)`. `arrange` is an
  idempotent function that puts the world into the state that step expects (players at the enemies and armed, earlier
  quest stages set, earlier enemies killed). In a partial run the first step that runs gets its `arrange` once; a full
  run never arranges. If a step cannot be arranged (a target that must be alive is already dead) it fails as kind
  `arrange` and tells you to run the whole playbook or reload the scenario. `fight`, `loot_race` and `quest_sync` are
  resumable (`--list` marks them with `R`); the others refuse `--from-step` with exit 2 and list the ones that work.
  Run the whole playbook once before you trust a pass from a partial run. A failed run prints the rerun line.
* **Fail-fast settle.** `ctx.settle()` stops early when no machine's update number and no host ack has moved for the stall
  window (`--settle-stall S`, option `settle_stall_s`, default `max(6, budget / 4)` seconds, 0 turns it off). The reason says how
  long nothing moved, the update number of every machine and, when the adapter's `status()` gives an `acks` dict, what the host
  knows each client has received. A mismatch counter that keeps rising while nothing moves is named too.
* **Game speed.** `--timescale X` calls `set_timescale(X)` on every machine before the first step and `set_timescale(1)` at the
  end, also after a failure. A machine that refuses stops the run before any step. If the adapter also has `timescale()`, a
  copy that accepted the call but runs at another speed is caught, and so is a copy that stays fast afterwards. Fixed
  `ctx.pause()` waits shrink by X. The game specific part is one adapter method: the example adapter sends one console line.
  Checks that depend on real time may need a run at 1x.

## Several pairs at once

```
python -I tools/bots/run_parallel.py --pairs 3 -- --adapter my_game:make_adapter --scenario fight.json --playbook fight
python -I tools/bots/run_parallel.py --only 1 -- --adapter my_game:make_adapter --playbook quest_sync --from-step 3
python -I tools/bots/run_parallel.py --pairs 3 --fake -- --playbook all          # game free smoke test
python -I tools/bots/run_parallel.py --pairs 3 --dry-run -- --playbook fight     # checks and commands only
```

`run_parallel.py` starts one `run_coop.py` per host/client pair as a subprocess (`--runner FILE` for your own runner). Pair k is
the instances `2k` and `2k+1` (`--players-per-pair N` for more); each runner gets `--instances A,B --outdir <out>/pair<k>`
(your adapter factory receives those instance numbers, the first is the host) and finds its pair in `COOP_PAIR`,
`COOP_INSTANCES` and, with `--port-base P`, `COOP_PORT` (P + k). Arguments after `--` go to every runner. The log of each pair
streams with a `[pair<k>]` prefix, and at the end one table shows PASS, FAIL, SETUP, BLOCKED, TIMEOUT or CRASH per pair with the
reason of each failure; `parallel_summary.json` has it too. Exit codes: 0 all pass, 1 any FAIL, TIMEOUT or CRASH, 3 a pair was
BLOCKED (its runner exited 3), 2 a setup problem. It refuses to start when two pairs would share an instance, port or output
folder, or when `--instances`, `--outdir` or `--players` is passed after `--`. `--preflight module:function` adds your own
checks (free RAM, a lease on an instance, a port in use): the function gets the list of pair plans and returns problem
strings. `--timeout S` stops a hung runner, and only that process. Parallel runs add throughput; they do not shorten one round.

## The playbooks

Each playbook is a short script of steps. After every step the runner settles, compares snapshots and checks counters.
`refs` are the names the scenario (or `--refs`) must provide. `requires` are the adapter methods every machine needs; without
them the playbook is skipped.

| playbook | players | refs | requires | what each step proves |
|---|---|---|---|---|
| `fight` | 2-4 | `raiders` (list), `raider_base?`, `weapon?` | goto_ref, aim_at, fire, target_alive | everyone shoots the same enemies; dead on every machine, each dies exactly once, enemy count constant (needs `ref_count`, skipped with a note otherwise) |
| `ownership_handoff` | 2-4 | `raiders`, `far_pos`, `near_pos` | goto_ref, goto_pos, aim_at, fire, target_alive, owner_of | the host walks away: control moves once to the client, no flapping when the host returns, the fight continues |
| `loot_race` | 2-4 | `item`, `item_base` | goto_ref, pickup, inventory_count, ref_enabled, drop | exactly one pickup wins and the party holds exactly 1; two more rounds by dropping and racing again |
| `container_share` | 2-4 | `container`, `item_a`, `item_b` | goto_ref, open_container, close_container, take, put, inventory_count | one "in use" lock; take and put replicate; nothing duplicated or lost |
| `door_lock` | 2-4 | `door`, `locked_door`, `device`, `device_entry`, `locked_box`, `key` | doors, unlock, use_device, is_locked, containers | door open and close from host and client, a locked door refuses, unlock with a key, a device entry unlocks a box |
| `fast_travel_tether` | 2-4 | `marker`, `anchor`, `tether_radius`, `far_pos`, `hostile?`, `weapon?` | goto_ref, fast_travel, consent, distance_to, cell | refused without consent and in combat; all arrive together; a wanderer is held by the tether |
| `dialogue_spectator` | 2-4 | `npc`, `topics`, `speakers?`, `done_quest?`, `done_stage?` | talk, pick_topic, end_dialogue, in_dialogue, spectating | the speaker talks, the others spectate and are released, no second speaker, roles swap |
| `consent_vote` | 2-4 | `barrier_quest`, `barrier_stage` and/or `confirm_ref`, `confirm_quest`, `confirm_from`, `confirm_to` | set_stage, barrier_pass, activate, answer_prompt, consent | a barrier holds until the last player passes and applies once; a ready check: decline, a guest's "not yet" vetoes, all ready applies once |
| `consent_vote_guest_declines` | 2-4 | same | same | the ready check stops after the veto: the stage holds everywhere |
| `quest_sync` | 2-4 | `quest`, `stages` | set_stage, start_quest, stage_done, objective flags | stages and objective flags match on every machine; repeating a stage is a no-op |
| `save_load_rejoin` | 2-4 | `quest`, `stage_a`, `stage_b`, `reward_item?`, `save_name?` | save_game, load_game, session_leave, session_join, rebases | a client leaves, the host moves on, the client rejoins and catches up with each reward once; the host loads the older save and every client rebases; a second rejoin adds nothing |
| `spawn_once` | 2-4 | `marker`, `base` | goto_ref, spawn, ref_count | the host spawns one; exactly one exists on every machine and it stays one |

`far_pos` and `near_pos` are `{"ref": id, "dist": units}` or `{"x": .., "y": .., "z": .., "cell": ..}`. `stages` is
`[[stage, [[objective, "displayed" or "completed", 0 or 1], ...]], ...]`.

A playbook is a class: set `name`, `needs_refs`, `requires`, write `run(ctx)` with `with ctx.step("..."):` blocks, and add
it to `coopbots/registry.py`. The helpers on `ctx` are `must`, `expect`, `on_all` (every machine at the same moment, one
thread each), `same_everywhere`, `wait_until`, `pause`, `settle`, `optional` (run a check that may need an action the adapter
lacks), `equip_all` and `run_step(name, body, arrange=...)` (a step that `--from-step` and `--only-step` can skip or start
from; set `resumable = True` on the playbook when every step uses it).

## What the runner checks after every step

1. **Settle:** every machine reports the host's last update number, stable for two polls, or the step fails and names the
   machines and numbers. It stops early, with the reason, when nothing moves (see "Fast rounds").
2. **Quiet window** (option `quiesce_s`, for impaired links): nothing is sent for that long, every machine must be converged,
   and then no repair may happen during a second quiet window.
3. **Snapshot compare:** the `shared` part of each client against the host.
4. **Counters:** repairs and mismatches must not rise (unless `strict_counters` is off, see below); bad messages, failed
   applies and rebases must never rise (a step that expects a rebase passes `allow_rebase=True`).
5. **Repair budget** (option `repair_budget_per_min`): repairs per minute over the whole run. Default is none set; with an
   impaired link the default is 0. A budget above 0 in the profiles file needs a `repair_budget_reason` next to it.

On any failure the runner writes `FAIL-<n>-<step>/` with `failure.json` (reason, per machine status, capture, steps so far),
each machine's snapshot, and whatever your `capture()` wrote, plus `result.json` and `snap/` for the whole run.

### Impaired links

`--link bad_wifi` (or `links.link_options("bad_wifi")`) turns strict counters off, adds the quiet window and a budget. It
does not impair the network itself: start the chaos relay in `tools/chaos` between the instances. Profiles: `lan`,
`good_wifi`, `bad_wifi`, `mobile_tether`, `transatlantic`, `hell`. Repairs are never ignored: they are recorded per step, must
stop once the world is quiet, and are budgeted.

## How it is tested

```
python -I tools/bots/run_tests.py
```

Prints `ALL OK` only when every test passes and every twin is caught. Fakes only: no game is started.

* 138 unit tests (59 before the newer features). Every playbook passes on the fake world with 2, 3 and 4 players and with a laggy link, and on a lossy link.
  A playbook whose adapter lacks an action is skipped, never passed. A playbook with no steps is not a pass. A wrong
  player count or a missing ref is a clear failure. A failed step saves its evidence. The example adapter runs against an
  in-memory console and each of its postconditions is checked with a console that lies. The command line is tested for exit
  codes, `--link`, a run through `--adapter` with refs read from a scenario file.
* 27 playbook twins: a playbook plus one deliberately wrong behaviour of the fake world (a duplicated pickup, a barrier that
  releases early, a tether that is not enforced, a client that never rebases, a stale health echo, a client that stops
  applying updates, ...). Each must make that playbook FAIL, and a test checks that every one of the 30 fake bugs has a twin. `counter_noise` (counters that rise) must
  fail all 12 playbooks. A test proves the snapshot comparison alone catches a double spawn that no query asks about.
* Impaired-link tests: every playbook passes on a lossy link; a message lost for good, a repair in the quiet window and repairs over budget each fail; a budget above 0 needs a written reason.
* The newer features have their own tests, all on the fake world: the action result model (`tests/test_actions.py`: result
  type, pre and postconditions, a lying adapter, strict mode, every say-why reason: stuck walk, menu open, message box,
  holstered weapon, target too close or too steep, evidence in `failure.json`); resumable steps, fail-fast settle and game
  speed (`tests/test_resume.py`: `--from-step`, `--only-step`, arrange, a frozen client, a copy that ignores or refuses the
  speed); the parallel runner (`tests/test_parallel.py`: the plan, the checks, every exit code, real subprocess runs with
  prefixed logs and one table).
* 15 engine twins: the suite is run again with one check of the runner itself broken (`COOPBOTS_MUTANT`: no settle, no snapshot
  compare, no counter check, skips counted as passes, failures that stop saying why, postconditions that are not checked,
  resume that runs every step, resume without arrange, settle that never stops early, a speed that never reaches the game,
  a parallel runner with unprefixed logs, always exit 0, one instance set for every pair, a hung runner never stopped, and
  forbidden options let through). Each must make a test fail. The twins of the newer features run only the test files that
  cover them.

## Ported from fnv-coop commit f57aa78

Updated from fnv-coop commit e85460f: the action result model, resumable steps, fail-fast settle, `--timescale` and the parallel
pair runner were generalised from the Fallout project's newer work. The game specific parts stayed there: the console lines
for the game speed, the menu and weapon state reads and the walk and aim details live only in the example adapter in
`adapters/fnv/`.

Ported from the Fallout: New Vegas co-op project ([cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop), commit f57aa78),
where these playbooks and the runner were first written against its game bridge. The runner, the playbooks and the fake co-op
world were made game independent (ids are opaque strings, the story and netcode names are generic, the per-game action
layer became the adapter interface). Left in the Fallout project: the questline plan for its opening quests, the
animation and pose checks, and the code that drives the game's own input and engine functions. The Fallout console
adapter in `adapters/fnv/` shows the shape of a first adapter without any of that.
