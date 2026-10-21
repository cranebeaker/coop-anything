# Scenario format, schema 1

A scenario is one JSON file. It describes a starting state, the steps that happen next, and the checks that must hold on
every machine. The runner puts all game instances into the starting state, runs the steps and reports `pass`, `fail`,
`error`, `partial` (a step needs something the adapter does not have) or `invalid` (the file is wrong; no game was touched).

The file name is the scenario name: `fight.json` holds `"name": "fight"`.

## Top level

| key | meaning |
|---|---|
| `schema` | always `1` |
| `name` | `lower_snake_case`, equal to the file name without `.json` |
| `title`, `description` | words for a human; both required |
| `players` | 1 to 4 game instances sharing one world. Player 0 is the host |
| `tags` | free list of strings |
| `start.save` | name of the save the scenario starts from. `start.reload: true` forces a reload even when the loaded save is the same |
| `start.prepared` | optional boolean. `false` always loads the plain `start.save`, never a prepared copy of it (see README, "Prepared saves"). Default `true`: a prepared copy is used when the adapter has one |
| `reset.keys` | `$alias` list of state objects (quests, flags, script variables) that the jump resets in place. Every `set_state` key is added automatically |
| `objects` | the glossary, see below |
| `refs` | optional names for the bots tool (`tools/bots`): ref name to `$alias`, list, or nested object. Aliases become ids |
| `setup` | steps run during the jump, after the reset |
| `steps` | the test: steps and checks, in order. At least one check is required |

## The `objects` glossary

Everything the game must find goes in here once: `alias -> {"id": ..., "kind": ...}`. Steps then say `"$alias"`.

```json
"objects": {
  "raider1": {"id": "GSPG01Ref", "kind": "actor"},
  "teddyRef": {"id": "VMS13TeddyBearRef", "kind": "thing", "base": "VMS13TeddyBear"},
  "quest": {"id": "VCG02", "kind": "state"}
}
```

`id` is any string your adapter understands: an editor id, a form id, a path in a scene, a name. The runner never looks
inside it. `kind` is one of:

| kind | what it is | used by |
|---|---|---|
| `actor` | a character or creature in the world | kill, revive, owner, dead, stat |
| `thing` | a placed object: door, container, switch, marker, a world item | activate, lock, move target, locked |
| `item` | an item type that can sit in an inventory | give, take, item_count |
| `base` | a type that can be spawned | spawn, count |
| `place` | an area or cell | in_area, move target |
| `state` | a quest, story flag or script variable | set_state, state, state_reached |

`base` (optional, on a `thing`): the id of the item type you get by picking this object up. The jump planner uses it to
know that a pickup changes player inventories.

The word `player` is built in. It means "the player of the instance the step runs on" and is allowed where a step lists it.

## Steps

Every step has an `id` (`lower_snake_case`, unique) and an `op`. Optional keys on any step:

| key | meaning |
|---|---|
| `on` | `host` (default for actions), `guest` (every player but the host), `all` (default for checks), or a player index |
| `parallel` | `true` runs an `all` step on every player at the same moment (threads). Use it for races |
| `note` | free text |
| `needs` | extra capability names the step needs from the adapter |
| `dirties` | list of `$alias` the step changes in the world. See "Jump to a scenario" |

Ops and their fields (`?` means optional):

| op | fields |
|---|---|
| `set_state` | `key` (state), `value` |
| `move` | `who` (actor, thing, player), `to` (thing, place, actor) |
| `spawn` | `base`, `at` (thing, place, actor, player), `count?` |
| `give`, `take` | `who`, `item`, `count` (at least 1) |
| `kill`, `revive` | `ref` (actor) |
| `enable`, `disable`, `activate` | `ref` (actor or thing) |
| `lock` | `ref` (thing), `level` |
| `unlock` | `ref` (thing) |
| `wait` | `seconds` (0 to 120) |
| `save`, `load` | `name` |
| `leave`, `join` | none: this player drops out of or rejoins the co-op session |
| `action` | `name`, `args?` (an object; `$alias` values are resolved). Anything game specific: fast travel, a dialogue pick, a consent answer |
| `check` | `kind` plus the fields of that kind, see below |

## Checks

`{"op": "check", "kind": ..., <fields>, "value": N, "cmp": "==", "agg": null, "tol": 0, "timeout": 5}`

| kind | fields | reads |
|---|---|---|
| `state` | `key` | the current value of the state |
| `state_reached` | `key`, `at` | 1 if the state was ever at `at` |
| `item_count` | `who`, `item` | how many the holder has |
| `dead`, `disabled`, `locked` | `ref` | 1 or 0 |
| `in_area` | `ref`, `place` | 1 or 0 |
| `distance` | `a`, `b` | units between two things |
| `stat` | `ref`, `stat` | an actor value such as health |
| `follower` | `ref` | 1 if the actor follows the player |
| `count` | `base` | live objects of that base |
| `owner` | `ref` | which player owns (simulates) the object |
| `value` | `query`, `args?` | a number your adapter can read by name |

* `cmp` is `==`, `!=`, `<`, `<=`, `>`, `>=` or `same`. `same` means every player reports the same number, without knowing it.
* `value` is a number. Use 1 and 0 for true and false.
* `agg` is `sum`, `min` or `max` over the players the check ran on. `"agg": "sum", "value": 1` on `item_count` means
  exactly one copy exists in total, which catches both duplication and loss.
* `tol` is the allowed difference for `==` and `same`.
* `timeout` (seconds, default 5, at most 120): the runner polls until the check holds or the time is up. Write the wait into the
  check instead of adding a `wait` step.

## Jump to a scenario, not through the game

`Runner.jump()` brings every instance into the starting state. It reloads `start.save` only when it must:

* nothing is loaded on that instance yet (a restarted game counts as empty),
* a different save is loaded,
* the previous scenario failed or errored,
* this scenario sets `start.reload`,
* the previous scenario changed something this one uses, or changed something it could not name.

Otherwise the jump is in place: the adapter resets the scenario's state keys (plus everything the previous scenario set),
then the `setup` steps run.

What a scenario "dirties" is computed from its steps: spawns, moves of non-player objects, kills, enables, locks,
activates (and the item types they give), `give` and `take` (on the player: that item in the player inventory). Three things cannot be
named and force a reload for the next scenario, written as `*`: a `load`, an `action`, and a `set_state` with no `dirties`
list. When a state change cannot touch the world, write `"dirties": []` and the next jump stays in place. When it changes
named objects, list them: `"dirties": ["$gate", "$raider1"]`.

`python -I tools/scenarios/scen.py plan NAME --prev OTHER` prints the decision without a game.

## Capabilities

Every step needs one capability from the adapter: `op:<op>`, `check:<kind>`, or `action:<name>` (plus its own `needs`
list). An adapter that returns a set from `capabilities()` is held to it. A step whose capability is missing is reported
`blocked`, never `pass`, and every later step is `blocked` too, so the scenario ends `partial`. An adapter that returns
`None` claims everything.

## Validation

`scenlib.validate(scenario, name=None, hooks=())` is game-free. It rejects: a wrong `schema`, a bad name, a missing `title`,
`description` or `start.save`, an object without an `id` or with an unknown `kind`, an unknown alias anywhere (steps,
`reset`, `dirties`, `refs`, `action` args), an object of the wrong kind for a field, `player` where it is not allowed,
unknown ops and check kinds, missing or unexpected fields, wrong types, duplicate step ids, a bad `on`, a check without a
`value` (or `cmp: same`), a bad `cmp` or `agg`, a timeout out of range, and a scenario with no check.

`hooks` are functions `f(scenario) -> [error strings]`. Use one to check ids against your game's data, for example "this
editor id exists in the dump and has the right record type". The shipped example scenarios need none.

For the common check "does every id exist, as the kind the scenario says", use `scenlib.index_hook(index)`, where `index` is
`{"<id>": "<kind>"}` (or a `scenlib.DataIndex`). `Runner.run` adds it by itself when the adapter's `data_index()` returns an index.
It checks the `id` and the `base` of every object in `objects`, names the steps that use it (setup steps included), and
names the nearest known id. `scen.py validate --index FILE` and `--adapter pkg.mod:factory` run it without a game. Also
rejected: a `start.prepared` that is not a boolean.
