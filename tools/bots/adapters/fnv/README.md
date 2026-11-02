# Example adapter: Fallout: New Vegas (script console)

`fnv_adapter.py` is a small, complete `ActionAdapter` for a Gamebryo game that has a script console and a bridge into the
running game. It is an example of how short a first adapter can be, not a finished adapter for every playbook.

It implements: `goto_ref`, `activate`, `pickup`, `take`, `put`, `set_stage`, `unlock`, `set_timescale` and the queries `inventory_count`,
`target_alive`, `ref_enabled`, `is_locked`, `door_open`, `distance_to`, `quest_stage`, `stage_done`, plus `status`, `snapshot`
and `capture`. Everything else is left out, so playbooks that need it (fast travel, dialogue, spawning, ...) are reported
`SKIP`, which is the honest answer until someone writes those actions.

What to copy from it:

* One console line per action, an expression per query. The ids are editor ids or hex ids the console accepts.
* A postcondition after each action. `take` and `put` read both inventories before and after and fail unless both moved
  by the same count, so a transfer that duplicates or loses items is caught. `pickup` fails with "already taken" for the
  loser of a race and checks the object is really gone. `goto_ref` checks the distance after the move.
* `status()` maps the co-op mod's own counters onto the runner's names in one small table (`COUNTER_PATHS`).
* Say-why: `blockers()` reads the mod's `ui.status` (open menu, message boxes, weapon drawn) and the base class adds it to
  every failed action. `goto_ref` runs through `self.act(...)` with a postcondition and fails as `stuck: N units left`.
* `set_timescale` is the one game specific line of the `--timescale` feature: the console line `sgtm X`.

The bridge object needs `exec(line)`, `eval(expr)` and `call(method, params)` (for the mod's `coop.status`, `state` and
`events`). The tests (`tools/bots/tests/test_adapters.py`) run the adapter against an in-memory console, including a console
that silently ignores one command at a time, to show each postcondition firing. No game is started.

The matching scenario adapter is `tools/scenarios/examples/fnv/fnv_game.py`.
