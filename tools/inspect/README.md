# inspect: a read-only live table of your game copies

When a co-op test fails, the quickest question is "what is each copy doing right now?". This tool answers it: one column per running game copy, one
row per field (menu, health, position, who owns the target, sync counters, recent desyncs, recent message boxes, whatever your game reports), refreshing
if you ask. Under the table it lists the problems it can name in plain words: a message box that is up, a copy that is behind the host, a mismatch counter that moved.

It only looks. It reads the copies over the in-game bridge ([tools/bridge](../bridge/README.md)), through a wrapper that refuses every method that could change the game,
so a message box that is up stays up and a quest stage stays where it is.

Python 3.8 or newer, standard library only. It needs a game that runs the bridge server (or the fake copies in the demo).

## Quick start: see it on fake copies

```
python -I tools/inspect/demo.py
```

It starts a fake host, a fake client and a third copy that is not running, refreshes the table five times and, after the second refresh, makes the client raise a message box, lose health and miss a sync update.
The table is made by the same code as the real tool, from [examples/demo_fields.json](examples/demo_fields.json).

## Quick start: your game

1. Run the bridge server in your game ([tools/bridge](../bridge/README.md)) and have it write its endpoint file, or note its port.
2. Write a field list (below). Start from `examples/demo_fields.json`.
3. Look:

```
python -I tools/inspect/inspect_live.py --fields local/mygame_fields.json \
    --endpoint host=tcp://127.0.0.1:27990 --endpoint client=tcp://127.0.0.1:27991
python -I tools/inspect/inspect_live.py --fields local/mygame_fields.json --endpoint-file a=run/a.json --endpoint-file b=run/b.json --watch 1
python -I tools/inspect/inspect_live.py --fields local/mygame_fields.json --endpoint a=pipe://mygame-0 --json
```

`--watch SECONDS` refreshes until Ctrl-C (`--count N` stops after N). `--json` prints the raw values and the problem list for a script. Exit code 0 when at least one copy answered, 2 when none did, 1 for bad arguments.
A copy that cannot be reached is a column that says so; the other columns still show. A copy that comes back is picked up again on the next refresh.

## The field list

One JSON file per game. Keys starting with `_` are ignored.

```json
{
  "name": "mygame",
  "eval_allow": ["health"],
  "read_methods": ["sync.status"],
  "fields": [
    {"label": "menu",   "source": "state", "section": "menu", "path": "menu.kind"},
    {"label": "health", "source": "eval",  "expr": "health", "format": "{:g}"},
    {"label": "acks",   "source": "call",  "method": "sync.status", "path": "peers", "template": "slot {slot}: acked {acked}"},
    {"label": "boxes",  "source": "events", "types": ["ui.messagebox"], "last": 3, "template": "#{seq} {text}"}
  ],
  "problems": [
    {"field": "menu", "op": "!=", "value": "none", "say": "{copy}: the {value} menu is open"},
    {"differs": "lastSeq", "say": "lastSeq differs between copies: {values}"}
  ]
}
```

| Key | Meaning |
|---|---|
| `eval_allow` | The first words of the expressions the inspector may evaluate (case does not matter). An `eval` field whose expression starts with anything else is shown as `refused` and is never sent. |
| `read_methods` | Custom bridge methods the inspector may call. They must only read. `exec`, `save`, `load`, `screenshot`, `waitFor` and `quit` are refused here even if listed. |
| `fields[].label` | The row name. Unique. Rows appear in the order of the list. |
| `fields[].source` | `ping` (the ping answer), `version`, `state` (the bridge `state` method, with `section` to read only that part; all `state` fields share one request), `eval` (`expr`, optional `ref`), `events` (`types`: recent events of those types, `last` of them), `call` (`method` from `read_methods`, optional `params`). |
| `fields[].path` | Where to read inside the answer: `a.b.0` walks keys and list positions. Empty means the whole answer. A path that is not there shows `n/a`. |
| `fields[].format` | Python format text for one value. A list fills `{0} {1}`, a dict fills `{name}`, a number can use `{:.0f}` or `{:08X}`. |
| `fields[].template` | For a list (or events): one line per item, filled from the item's keys. Events also have `{seq}` and `{type}`. `empty` is the text for an empty list (default `none`). |
| `problems[]` | Rules. `{"field", "op", "value", "say"}` fires for every copy whose value passes the test (`op`: `==`, `!=`, `<`, `<=`, `>`, `>=`, `nonempty`, `empty`); `say` can use `{copy}`, `{field}`, `{value}`. `{"differs": "field", "say"}` fires when the copies disagree; `say` can use `{values}`. |

A method the game does not have (`-32601`) shows `n/a` in that row and is not a problem. Any other error from a method shows `error: ...` and is listed under the problems.

## What it never does

- Send anything that can change the game. Every request goes through `ReadOnlyBridge`: a fixed set of query methods (`ping`, `version`, `methods`, `events`, `state`, `eval`) plus the field list's `read_methods`, and nothing else. `eval` only for expressions on `eval_allow`.
- Press a message box away. Use your runner's own dismiss step for that.
- Hide a copy that is down. It shows as unreachable and as a problem.

## How it is tested

```
python -I tools/inspect/run_tests.py       # prints ALL OK only if every check passes and every twin is caught
```

23 checks and 11 mutation twins at the time of writing; about half a minute. The "games" are fake copies from `tools/bridge/fake_game.py` on loopback ports. No real game is started.

- **Reading:** every source (ping, state, eval, a custom method, events) is read and formatted; each refresh reads the game again and shows changed values; events are read from a cursor and listed once; a method the game lacks is `n/a`; a missing path is `n/a`; a method that fails shows its message and the other rows still show;
  an unreachable copy is a column while the others show; a copy that goes away and comes back is read again.
- **Read-only:** the wrapper refuses `exec`, `save`, `load`, `screenshot`, `waitFor`, `quit` and unknown methods and the fake's state stays untouched; an expression off the allow list is refused and never sent; a whole refresh only ever calls `eval` and `state` on the fake; a field list cannot name a write method.
- **Field lists:** nine kinds of broken list are refused with a reason; the shipped example lists load.
- **Problems and table:** a healthy pair gives `problems: none`; each rule fires for its own copy only; columns line up and long cells wrap; the table says so when nothing answers.
- **Command line and demo:** table and JSON output, exit codes, endpoint files, `--watch --count`, bad arguments, and the demo's output.
- **Twins:** stale data returned on a refresh, write methods passing the wrapper, an ignored eval allow list, a field list that may name a write method, one dead copy breaking the table, a missing method shown as an error, an event cursor that is not kept, a dead connection that is never reopened, rules that never fire, a "differs" rule that is off, and columns that are not padded.

## Files

`inspect_live.py`, `demo.py`, `examples/demo_fields.json`, `examples/fnv/` (`README.md`, `fnv_fields.json`), `test_inspect.py`, `run_tests.py`.

## Generalised from fnv-coop commit e85460f (github.com/cranebeaker/fnv-coop)

Generalised from `tools/bots/inspect_live.py` in [fnv-coop](https://github.com/cranebeaker/fnv-coop), commit e85460f. What changed:

- The rows are no longer built into the code. The original had fixed rows for menu, weapon, health, position, a reference block and the co-op state; they are now a field list per game, read over the bridge's `ping`, `state`, `eval`, `events` and read-only custom methods.
- The read-only guard became a rule of the tool: a fixed set of query methods plus the field list's `read_methods`, an `eval_allow` list of expression names, and a hard refusal of the write methods.
- The problem rules moved from code to the field list, with a rule for "the copies differ".
- Copies are found by endpoint (TCP or pipe) or by endpoint file instead of by instance number, `--watch` refreshes, and a copy that returns is reconnected.
- The tests use fake copies on loopback and twins made from outside the code.

The Fallout: New Vegas field list is in [examples/fnv](examples/fnv/README.md).
