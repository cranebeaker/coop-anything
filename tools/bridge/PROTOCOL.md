# Bridge protocol, version 1

The wire format between a test client and the RPC server that lives inside a game process. The C++ server
(`include/coopbridge/server.h`) and the Python client (`bridge_client.py`) speak it, and so does the fake game
(`fake_game.py`). A server or client in any other language only needs this page.

## Transports

One request stream per connection. Either transport carries the same bytes.

| Endpoint | Meaning |
|---|---|
| `tcp://127.0.0.1:PORT` | TCP on a loopback address. Works on every OS. |
| `pipe://NAME` | Windows named pipe `\\.\pipe\NAME`, byte mode. Remote pipe clients are refused. |

A server may offer both. It may write its endpoints to a small JSON file at start (`Options::endpointFile`):

```json
{"protocol": 1, "game": "mygame", "pid": 1234, "endpoints": ["tcp://127.0.0.1:41873", "pipe://mygame-0"]}
```

A test harness that starts the game reads that file, so the server can pick a free port.

## Framing

- UTF-8 text. One JSON object per line, ended by `\n` (a `\r` before it is ignored). Empty lines are ignored.
- Requests and replies are JSON objects. A line longer than 4 MiB closes the connection after an error reply.
- A server writes only valid UTF-8. Bytes in a game's text that are not valid UTF-8 are replaced by U+FFFD.
  A client decodes with replacement so one odd byte never hides a reply.

## Request and reply

```
-> {"id": 7, "method": "exec", "params": {"cmd": "set quest 10"}}
<- {"id": 7, "result": {"ok": true, "output": "ok"}}
<- {"id": 7, "error": {"code": -32602, "message": "exec requires string param 'cmd'"}}
```

- `id`: a number or a string, chosen by the client and echoed in the reply. `null` is allowed. A client that sends
  increasing integers can recognise a late reply (see Timeouts).
- `method`: a string. `params`: an object, or absent.
- A reply has `result` or `error`, never both. `error` is `{"code": int, "message": string}` and may add `data`.
- Replies on one connection come back in request order. A server runs one request per connection at a time. Several
  connections may be open at once; their requests are queued together for the main thread.
- A line that is not JSON gets `{"id": null, "error": {"code": -32700, ...}}`.

## Timeouts

- Any request may carry `params.timeoutMs`. The server's default is 30000 ms for requests that do not.
- A request that needs the game's main thread waits in a queue. If no frame ran it within `timeoutMs` plus a small slack
  (2 s by default), the server answers error `-32001` and drops the request: it will not run later.
- `load` and `save` answer error `-32002` when the game does not finish in time. `waitFor` does not use an error: it answers
  `satisfied: false, timedOut: true`.
- A client has its own timeout for every call. When it gives up, the connection stays usable, and the late reply of the
  abandoned call is skipped by the next call because its `id` is smaller than the one expected.

## Which thread runs a request

| Served on the transport thread (works while the game loads or hangs) | Run on the game's main thread, one frame hook at a time |
|---|---|
| `ping`, `version`, `methods`, `events`, custom methods registered as "direct" | `exec`, `eval`, `waitFor`, `state`, `save`, `load`, `screenshot`, custom methods |

The server starts at most `maxCallsPerPump` (default 16) queued requests per frame hook, so a burst of calls cannot freeze a frame.
When more than `maxPending` (default 256) wait, new ones are refused with `-32003`.

## Standard methods

A game declares which of these it supports. A request for one it does not support, or for an unknown method, gets `-32601`.
`methods` lists what a game answers.

### `ping` (always)

Result: `{"pong": true, "protocol": 1, "game": "name", "pid": 1234, "frames": 5210, "mainLoopAgeMs": 12, "ready": true, "loadedCount": 1}`

`mainLoopAgeMs` is the time since the last frame hook ran, or `-1` if none ran yet (the game is still starting). A client keeps
`pid` so it can tell a crash (pid gone) from a closed connection.

### `version` (always)

Result: `{"protocol": 1, "game": "name", "build": "abc123", "server": "coopbridge-cpp 1", "pid": 1234}`. Record it in the metadata of every test run.

### `methods` (always)

Result: `{"methods": ["ping", "version", "methods", "events", "exec", ...]}`

### `exec`

Params: `{"cmd": "console line", "ref": "optional reference", "timeoutMs": n}`. Result: `{"ok": bool, "output": "text"}`.
`ok` says whether the game accepted the line; `output` is what it printed, or `""` when the game cannot capture output. `ref` names a
reference the line runs on, in whatever form your adapter documents.

### `eval`

Params: `{"expr": "expression", "ref": "optional", "timeoutMs": n}`. Result: `{"value": number | bool | string}`.

### `waitFor`

Params: `{"expr" | "cmd": "...", "op": "truthy", "value": 0, "contains": "text", "timeoutMs": 10000, "pollMs": 100, "ref": "optional"}`.

The server evaluates `expr` (with the adapter's eval) or runs `cmd` (with exec) on the main thread every `pollMs` until the condition holds.

- With `expr`: a number or bool is compared with `op` (`truthy`, `==`, `!=`, `<`, `<=`, `>`, `>=`) and `value`. A string value
  satisfies only a `contains` test.
- With `cmd`: the first number in the output is compared; with `contains` the output must contain that text.

Result: `{"satisfied": bool, "timedOut": bool, "value": last value, "elapsedMs": n}` and, on a timeout where polls failed, `"lastError"`.
A poll that fails (the game is loading) does not end the wait.

### `state`

Params: `{"sections": ["name", ...], "path": "file"}`. Result: the adapter's JSON snapshot, cut to the sections asked for (all when absent).
With `path` the server writes the snapshot to that file and answers `{"path": "...", "bytes": n}`. The game process writes the file, and its
working directory is the game's own folder, so pass an absolute path.

### `save`

Params: `{"name": "x", "overwrite": false, "timeoutMs": 30000}`. Starts a save and waits for the game to report it. Result: `{"saved": true, "path": "..."}`.
The adapter decides which names are allowed and may refuse to replace a save: a refusal is an adapter error (the example uses `-32012`).

### `load`

Params: `{"name": "x", "timeoutMs": 180000}`. Starts a load and waits until the game says it is in the world. Result: `{"loaded": true, "name": "x"}`.

### `screenshot`

Params: `{"path": "optional file"}`. Result: `{"ok": true, "path": "file written"}`.

### `events`

Params: `{"since": 0, "limit": 0}`. Result: `{"events": [{"seq": 5, "tMs": 1200, "type": "game.loaded", "data": {...}}], "next": 5, "dropped": 0, "last": 9}`.

The server keeps the last 1024 events (a ring). It returns events with `seq > since`, oldest first, up to `limit` (0 means up to 1000). `next` is the `seq` of the last
event returned, or `since` when there is none: pass it as `since` in the next call. `dropped` counts events after `since` that the ring no longer holds. `last` is the newest `seq`.
The server adds `game.loaded` and `game.saved`; a game adds its own with `PushEvent(type, data)`.

## Custom methods

A game registers its own methods under a dotted name (`sync.status`, `bot.menu`). Standard names cannot be replaced. A custom method
takes the params object and returns any JSON value, or throws. It runs on the main thread unless registered as direct.
Inspectors and dashboards should only call custom methods that read.

## Error codes

| Code | Name | When |
|---|---|---|
| -32700 | parse error | the line is not JSON |
| -32600 | invalid request | not an object, `method` missing, `id` of a bad type, line too long |
| -32601 | method not found | unknown method, or the game does not support a standard method |
| -32602 | invalid params | a parameter is missing or has the wrong type |
| -32000 | host error | the adapter or a handler threw; `message` is its text |
| -32001 | main thread timeout | no frame ran the request in time |
| -32002 | operation timeout | `load` or `save` did not finish in time |
| -32003 | busy | too many requests are waiting for the main thread |
| -32004 | stopping | the server is shutting down |
| -32010 to -32099 | game specific | adapters choose these (a save that exists, a lease held, ...) |

Clients map a reply with `error` to an exception that keeps `code` and `message`. A missing method (`-32601`) is the normal way for a tool to
learn that a game lacks a feature, so tools treat it as "not available" and carry on.

## Versioning

`ping` and `version` carry `protocol`, an integer. Version 1 is this page. Rules:

- Adding a method, a result field or an event type does not change the number. Clients ignore fields they do not know.
- Changing the meaning of a field, removing one or changing framing raises the number. A client checks `protocol` after it connects and stops with a
  clear message when it is higher than it knows.

## Security

The server can run console lines in your game. It listens on loopback only unless a mod opts in with `allowRemote`, and it has no
password. Treat the port like a debugger: use it on test machines and test copies of a game, never on a machine where strangers can
reach the port, and never ship a build with the server on in a release.
