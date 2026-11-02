# Remote-control your game from tests: one RPC server in the game, a main-thread queue, a method registry

Every automated co-op test needs a way to reach into a running game: put it in a known state, run a console line, ask "what is the quest stage?",
wait until something happens, read the state to compare two copies. Screen scraping and input faking cannot do that reliably. A small RPC server inside the game process can.
This page is the pattern. The tools are in [`tools/bridge`](../tools/bridge/README.md) (server template, Python client, wire format, fake game) and
[`tools/inspect`](../tools/inspect/README.md) (a read-only live table of your copies). Both were generalised from the Fallout: New Vegas co-op project and need no particular game.

Read [automated-qa.md](automated-qa.md) first for what a test must prove, and [bots-and-playbooks.md](bots-and-playbooks.md) for what sits on top of a bridge (bots, scenarios).
[multi-instance-testing.md](multi-instance-testing.md) covers starting the copies.

## 1. The shape

```
 test / bot / inspector (any language)            the game process
 -----------------------------------            -------------------------------------------
  bridge_client.py                                transport thread     main thread (game loop)
    call("exec", {...})  --- one JSON line --->    reads a line   ---> request queue
                                                   waits (timeout)       |  Pump(), once per frame hook
    <--- one JSON line ---  reply  <-------------  replies       <---   adapter -> the game's own calls
```

Four parts:

1. **A transport thread** that accepts connections and reads newline-delimited JSON. TCP on loopback works on every OS. A named pipe is the Windows alternative.
2. **A request queue drained on the game's main thread.** Game code (console, scripts, forms, save and load) is almost never thread safe. The transport thread only queues the request and waits; one call in your frame hook runs it.
3. **A method registry.** The server offers a small standard set (`ping`, `version`, `exec`, `eval`, `waitFor`, `state`, `save`, `load`, `screenshot`, `events`) and your mod registers its own (`sync.status`, `bot.menu`) beside them.
4. **An adapter**, the only game-specific code: "run this console line", "evaluate this expression", "start this load".

## 2. Rules that save you weeks

- **Run game calls on the main thread only.** A call from the transport thread works in testing and crashes the game once in a hundred runs. Test the rule: make `exec` report the thread it ran on and fail the build when it is not the frame hook's.
- **Answer `ping` from the transport thread.** While the game loads or hangs, the main thread does not pump. A ping that still answers (with the time since the last frame) tells "loading" from "dead".
- **A timeout on every call, on both sides.** The client times out, and the server times out requests that no frame picked up (the caller gets a clear error, and the abandoned request never runs later). Never block a test forever on a game that stopped pumping.
- **Late replies must not confuse the next call.** Number the requests and skip any reply with an older number.
- **Errors keep their code and text.** The game's own error ("script command not found", "save exists") reaches the test unchanged. An adapter that swallows errors makes every test pass for the wrong reason.
- **Cap the work per frame.** Start a few queued requests per frame hook, not all of them, so a burst of callers cannot freeze a frame. Refuse new requests when the queue is full.
- **Poll waits on the game's side.** `waitFor` evaluates a condition every few milliseconds on the main thread and answers when it holds or when the time runs out. It is faster and more reliable than a client loop with sleeps, and a failed poll during a load does not end the wait.
- **A missing method is normal.** An older build or a module that is switched off lacks methods. Answer "method not found" and let tools show "n/a" instead of failing.
- **Absolute paths.** The game process resolves a relative path against its own folder. Have the client turn paths into absolute ones before it sends them.
- **Valid UTF-8 only.** Game text is often in a legacy code page. Replace bad bytes before they go into JSON, or one odd character hides a reply.
- **Loopback only, and only in test builds.** The server runs console lines for whoever can connect and has no password. Bind to 127.0.0.1 and do not ship it switched on.

## 3. What to expose, in order

1. `ping` and `version` (record the build in every run's metadata).
2. `exec`: one console line, with the output text. If your console cannot address a reference in a line, give `exec` a `ref` parameter that runs the line "on" that reference.
3. `eval`: one expression to a number. Quest stage, item count, health.
4. `waitFor`: the condition poll above.
5. `state`: a JSON snapshot of the shared state you sync, to compare machines path by path ([bots-and-playbooks.md](bots-and-playbooks.md)).
6. `events`: a ring of things that happened (`game.loaded`, `game.saved`, a message box shown, a desync report). Tests wait for an event instead of sleeping, and read the recent ones after a failure.
7. `load` and `save`: start the engine call, return, and finish when the game's own "loaded" or "saved" message arrives. Refuse the owner's save names.
8. `screenshot`, and your own methods for whatever the bots need (read a menu, press a button, read the sync status).

Keep the methods that only read apart from the ones that change something. An inspector then allows the first group and refuses the second.

## 4. Build order that finds problems early

1. **Write the client and a fake game first.** `tools/bridge/fake_game.py` is a pretend game with the same behaviour as the server, including faults (a late reply, a wrong id, garbage, a dropped connection). Your test scripts and bots can run against it before the real game exists.
2. **Embed the server in the smallest thing that has a main loop.** `tools/bridge/cpp/test_server.cpp` is a pretend game of about 200 lines. Run the same client tests against it.
3. **Write the adapter for the real game.** Start with `exec` only, then add methods one by one. Each one gets a client test.
4. **Add custom methods** as the bots need them. Register them under a dotted name and say which ones only read.
5. **Put the server only in test builds** and read its endpoint from a file the game writes at start, so every copy can pick a free port.

## 5. Look at your copies: the live inspector

When a test fails with two copies running, the first question is what each one is doing. [`tools/inspect`](../tools/inspect/README.md) prints one table with a column per copy:
menu, weapon, health, position, who owns the target, sync counters, recent desyncs and message boxes. The rows come from a small JSON field list for your game, read over the bridge.
It is read-only on purpose: a wrapper allows a fixed set of query methods and refuses everything else, so looking never changes what you are looking at. Under the table it lists the problems your rules spot (a message box is up, a copy is behind, a counter moved).
A copy that is down is a column that says so. Run it with `--watch 1` while a test runs, or once after a failure.

## 6. Test the bridge itself

A harness that cannot fail is worth nothing. The bridge tools run their own tests against a fake game and against the C++ server template, and every test has a **mutation twin**: a copy of the code
with one guard broken (timeouts ignored, errors swallowed, calls run off the main thread, no per-frame budget, a read-only wrapper that lets writes through, stale data returned on a refresh) that the guarding test must catch. Do the same for your adapter: the
test that asserts "this runs on the main thread" must fail when you call the game from the wrong thread.

```
python -I tools/bridge/run_tests.py
python -I tools/inspect/run_tests.py
```

## Checklist for an agent

- [ ] The server runs game code only inside the per-frame call on the main thread, and a test proves it.
- [ ] `ping` answers while the main thread is stuck, and says how long it has been stuck.
- [ ] Every client call and every queued request has a timeout, and a late reply is skipped.
- [ ] Game errors reach the test with their code and text. No method swallows an error.
- [ ] Per-frame work and queue length are capped.
- [ ] Listening on loopback only, in test builds only.
- [ ] Paths sent to the game are absolute; text sent back is valid UTF-8.
- [ ] A fake game stands in for the real one, so tests run before the game is wired up.
- [ ] Read-only methods are separate from the ones that change the game, and the inspector uses only the first group.
- [ ] The bridge's tests have twins, and every twin is caught.
