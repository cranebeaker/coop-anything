# bridge: remote-control a running game from tests

One small RPC server lives inside the game process. Test scripts connect to it and run console lines, read values, wait for a condition,
dump state, load and save, take screenshots and read an event log, without a person at the keyboard. It is what lets a bot or a test runner
drive two or more real game copies. Nothing in it is specific to one game: everything that touches the game sits behind one adapter class you write.

| Part | What it is |
|---|---|
| [`include/coopbridge/server.h`](include/coopbridge/server.h) | Header-only C++17 server template a mod embeds. A transport thread, a request queue that is drained on the game's main thread (one `Pump()` call per frame hook), a method registry, and the standard methods behind an adapter. TCP on loopback everywhere, plus a named pipe on Windows. |
| [`include/coopbridge/json.h`](include/coopbridge/json.h) | The small JSON value, parser and writer the server uses. Your adapter can use it too. |
| [`bridge_client.py`](bridge_client.py) | Python client, standard library only. TCP or Windows named pipe, a timeout on every call, distinct errors (`RpcError`, `CallTimeout`, `WaitTimeout`, `ConnectionLost`, `GameCrashed`, `ProtocolError`), and a command line. |
| [`PROTOCOL.md`](PROTOCOL.md) | The wire format, the methods, the error codes and the versioning rules. |
| [`fake_game.py`](fake_game.py) | A pretend game with the same server behaviour, for tests and demos. It can inject faults (late replies, a wrong id, garbage, a dropped connection). |
| [`cpp/test_server.cpp`](cpp/test_server.cpp) | A tiny pretend game that embeds the C++ server. The tests drive it with the Python client. It is also the shortest example of an embedding. |

The pattern and the pitfalls are in [playbook/in-game-bridge.md](../../playbook/in-game-bridge.md). The live table that reads a bridge is
[tools/inspect](../inspect/README.md).

Python 3.8 or newer for the client. The server needs a C++17 compiler (built with Visual Studio 2022 and CMake).

## Quick start: try the client without a game

```
python -I tools/bridge/fake_game.py --name demo --set health=80      # prints {"endpoint": "tcp://127.0.0.1:PORT", ...}
python -I tools/bridge/bridge_client.py --endpoint tcp://127.0.0.1:PORT ping
python -I tools/bridge/bridge_client.py --endpoint tcp://127.0.0.1:PORT exec "inc health 5"
python -I tools/bridge/bridge_client.py --endpoint tcp://127.0.0.1:PORT waitfor health --op ">=" --value 85 --timeout-ms 2000
```

In Python:

```python
sys.path.insert(0, "tools/bridge"); from bridge_client import Bridge, WaitTimeout, RpcError
with Bridge.connect("tcp://127.0.0.1:27990", wait_s=60) as g:    # waits for the game to open the port
    print(g.version())                       # record this in every test run's metadata
    g.require("exec", "waitFor")             # a clear error if the game lacks a method
    g.load("base_save")
    g.exec("set quest 10")
    g.wait_for(expr="quest", op=">=", value=10, timeout_ms=5000)     # raises WaitTimeout, never hangs
    page = g.events(); g.wait_event("game.saved", since=page["next"], timeout_s=30)
```

The endpoint is `tcp://127.0.0.1:PORT` or `pipe://NAME` (Windows). `COOPBRIDGE_ENDPOINT` supplies a default, and
`endpoint_from_file(path)` reads the file a game writes at start, so the game can pick a free port.

## Quick start: put the server in your mod

Copy `include/coopbridge/` into your mod (or add the folder to the include path). Write the adapter, start the server once, call `Pump()` once per frame.

```cpp
#include <coopbridge/server.h>

class MyAdapter : public coopbridge::Adapter {
public:
    coopbridge::Capabilities capabilities() const override {       // only what your game can do
        coopbridge::Capabilities c; c.exec = c.eval = c.state = true; return c;
    }
    coopbridge::ExecResult Exec(const std::string& line, const std::string& ref) override {
        return { /*ok*/ RunConsoleLine(line, ref, &out), out };   // your game's call; throw coopbridge::Error on failure
    }
    coopbridge::json::Value Eval(const std::string& expr, const std::string& ref) override { return EvalExpression(expr, ref); }
    coopbridge::json::Value State(const std::vector<std::string>& sections) override { return BuildSnapshot(sections); }
};

MyAdapter adapter;
coopbridge::Server server;

void OnPluginStart() {
    coopbridge::Options opt;
    opt.game = "mygame";  opt.build = GIT_HASH;
    opt.tcpPort = 27990;                       // or 0 for a free port, plus opt.endpointFile = "bridge-endpoint.json"
    opt.pipeName = "mygame-0";                 // Windows only, optional
    if (!server.Start(opt, adapter)) Log("bridge: %s", server.lastError().c_str());
    server.Register("sync.status", [](const coopbridge::json::Value&) { return BuildSyncStatus(); });   // your own methods
}
void OnFrameHook() { server.Pump(); }          // the game's main thread, once per frame
void OnShutdown()  { server.Stop(); }
```

What to know:

- **Every adapter method runs inside `Pump()`, on your main thread.** Call into the game freely there. The transport thread never touches the game.
- **Failures are exceptions.** Throw `coopbridge::Error(code, message)` for a code of your own (use -32010 to -32099), or any `std::exception` (reported as -32000). The client gets an `RpcError` with that code and text.
- **`load` and `save` start the work and return.** Call `server.NotifyLoaded()` / `server.NotifySaved(path)` from your game's load and save hooks. The request waits for it and answers `-32002` if it never comes.
- **Custom methods:** `server.Register("name", fn)` runs `fn` on the main thread. Pass `false` as the third argument for a method that only reads thread-safe data and must answer while the game loads. `PushEvent(type, data)` (any thread) feeds the `events` log.
- **Limits:** at most `maxCallsPerPump` requests are started per frame (16), at most `maxPending` wait (256), `slackMs` after a request's own timeout the caller gets `-32001`. All are in `coopbridge::Options`.
- **It listens on loopback only.** `tcpHost` must be a loopback address unless you set `allowRemote`. There is no password: see the security note in PROTOCOL.md. Build the server into test builds, not into what you ship.
- **Exceptions must be enabled** in the build of the code that includes the header.
- `Stop()` is for the main thread (at shutdown); it fails requests that are still waiting and joins every thread.

## Methods

`ping`, `version`, `methods` and `events` always exist. `exec`, `eval`, `waitFor`, `state`, `save`, `load` and `screenshot` exist when the adapter says so in `capabilities()`. A
method a game does not have answers `-32601`, which tools treat as "not available". Parameters, results and error codes are in [PROTOCOL.md](PROTOCOL.md).

## How it is tested

```
python -I tools/bridge/run_tests.py       # prints ALL OK only if every check passes and every twin is caught
python -I tools/bridge/run_tests.py --no-cpp     # the Python part only; the last line then says the C++ stage was skipped
```

28 client tests, 22 C++ server tests, 13 client twins and 12 C++ twins at the time of writing. A run takes about three minutes, most of it the compiles and
the process starts. No real game is started. The build goes to `tools/bridge/build/` (git ignores it) or to `COOPBRIDGE_BUILD_DIR`, one compile job at a time, and
the script removes `CMAKE_TOOLCHAIN_FILE` from the environment because a leftover toolchain from another target would pick the wrong compiler. If cmake or a C++ compiler is missing, the C++
stage prints SKIP and the last line is not a plain `ALL OK`.

- **Client against the fake game (`test_bridge.py`):** the standard methods and their results, state dumps to a file with a relative path, `wait_for` and `wait_event` with their cursors, an error from the game keeps its code and message, an unsupported method, a
  call that times out while the late reply is still on its way and is skipped by the next call, a reply with the wrong id, garbage, a reply split in two pieces, a 2 MB reply, a dropped connection (a closed
  connection while the game is alive, and a crash when the game's pid is gone), two threads on one connection, connecting while the game is still starting, a server that never answers the ping, a newer protocol, endpoint parsing, the endpoint file, the
  environment default, and the command line with its exit codes.
- **C++ server against the Python client (`test_cpp_server.py`):** the program from `cpp/test_server.cpp` is started and driven over TCP and over a named pipe. Checks: every standard method end to end; the adapter always runs on the thread that calls `Pump()`; adapter exceptions
  keep their code and message; unknown and unsupported methods; bad lines, bad ids, requests sent together in one write or in two halves; invalid UTF-8 from the game is replaced; a stuck main thread gives `-32001` while `ping` still answers and the abandoned request never runs; a burst of 40 callers
  is served at no more than the per-frame budget; a full queue answers `-32003`; 200 updates from 8 clients are not lost; the event cursor and the overflow count; an over-long line; a `load` or `save` that never finishes; a non-loopback host and a port in use are refused; `Stop()` closes open TCP and pipe clients, ends the process and removes the endpoint file.
- **Twins:** each one breaks a guard in a copy of `bridge_client.py` or `server.h` and the test that guards it must fail. Client twins: timeouts ignored, errors swallowed, reply ids not checked, a late reply not skipped, a split reply not buffered, no connect retry,
  a crash not told from a closed connection, every drop reported as a crash, `wait_for` that never raises, event cursor ignored, a relative path sent as is, no lock between threads, no protocol check. Server twins: calls run off the main thread, no per-frame budget, no queue limit,
  a main-thread timeout that never fires, errors swallowed, replies with the wrong id, a stuck event cursor, listening on every address, one request handled per read, a wait without a deadline, an unknown method that succeeds, a save without a timeout.

Not part of the suite: the non-Windows socket branch of the header (the suite builds with Visual Studio 2022), and any real game.

## Files

`bridge_client.py`, `fake_game.py`, `PROTOCOL.md`, `include/coopbridge/` (`server.h`, `json.h`), `cpp/` (`CMakeLists.txt`, `test_server.cpp`), `test_bridge.py`, `test_cpp_server.py`, `run_tests.py`,
`examples/fnv/README.md`.

## Generalised from fnv-coop commit e85460f (github.com/cranebeaker/fnv-coop)

Generalised from `tools/runner/nvbridge.py` (the Python client) and `plugin/bridge/` (the in-game server) in [fnv-coop](https://github.com/cranebeaker/fnv-coop), commit e85460f. What changed:

- The game's console, expression parser, form lookup, save hooks and screenshot code moved behind the `Adapter` class. Standard methods are declared with `capabilities()`.
- The Python client gained a TCP transport next to the Windows pipe, a `CallTimeout` that is separate from a lost connection, skipping of late replies, a protocol version check, an endpoint file and an environment default. It no longer depends on a game-specific helper module.
- Method names were made game neutral: `bridge.version` is `version`, `loadSave` is `load`, `saveGame` is `save`. The original's lease methods, `plugins` and `quit` are not part of the standard set.
- The server got a per-frame request budget, a queue limit, a loopback-only check and an endpoint file, and the tests drive the real server with the real client instead of checking each half alone.
- The original's tests used build switches inside the code for its mutants. Here the test runner makes the broken copies from outside, so the shipped headers carry no test switches.

The Fallout: New Vegas method names and console lines are in [examples/fnv/README.md](examples/fnv/README.md).
