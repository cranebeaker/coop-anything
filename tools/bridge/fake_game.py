#!/usr/bin/env python3
"""fake_game: a pretend game with an in-game RPC server (PROTOCOL.md), for tests and demos. Standard library only.

It behaves like the C++ server in include/coopbridge/server.h: requests that touch the game run on one "main
thread" that ticks every few milliseconds, `ping`, `version`, `methods` and `events` are answered at once, and
replies come back in request order. The game is a tiny console:

    set NAME VALUE      store a number or text          get NAME        print it
    inc NAME [N]        add N (default 1)               say TEXT        print TEXT
    msgbox TEXT         raise a message box (menu "message", event ui.messagebox)
    dismiss             close the message box           desync KEY A B  record a sync.desync event
    boom                the adapter throws (error -32000)
    anything else       {"ok": false, "output": "Script command not found: ..."}

`eval` reads a name (or `a+b`, `a-b`, a number). `state` returns FakeGame.state, cut to the sections asked for.

Faults for client tests: fake.faults[method] = {"kind": ..., ...} with kind
    delay   {"seconds": s}     answer after s seconds (the client may time out first, then sees a late reply)
    wrong_id                   answer with the wrong id
    garbage                    answer with a line that is not JSON
    close                      drop the connection instead of answering
    split   {"seconds": s}     send the reply in two halves, s seconds apart
Add "once": true to apply a fault one time only.

Run it alone to try the CLI or the inspector:  python fake_game.py --name a --port 0
"""
from __future__ import annotations

import argparse
import json
import os
import queue
import socket
import socketserver
import sys
import threading
import time
from typing import Any, Callable, Dict, List, Optional

PROTOCOL_VERSION = 1


class _Job:
    def __init__(self, fn: Callable[[], Any]):
        self.fn = fn
        self.done = threading.Event()
        self.result: Any = None
        self.error: Optional[Dict[str, Any]] = None


class FakeError(Exception):
    def __init__(self, code: int, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


class FakeGame:
    def __init__(self, name: str = "fake", vars: Optional[Dict[str, Any]] = None, state: Optional[Dict[str, Any]] = None,
                 host: str = "127.0.0.1", port: int = 0, pid: Optional[int] = None, unsupported: Optional[List[str]] = None,
                 tick_ms: int = 5, max_calls_per_tick: int = 16):
        self.name = name
        self.protocol = PROTOCOL_VERSION                # tests set a higher number to play a newer game
        self.vars: Dict[str, Any] = dict(vars or {})
        self.state: Dict[str, Any] = state if state is not None else {}
        self.menu = "none"
        self.host = host
        self.port = port
        self.pid = pid if pid is not None else os.getpid()
        self.unsupported = set(unsupported or [])      # standard methods this game does not offer
        self.faults: Dict[str, Dict[str, Any]] = {}
        self.saves: Dict[str, str] = {}
        self.events: List[Dict[str, Any]] = []
        self.calls: List[str] = []                     # every method the server ran, in order (for tests)
        self.on_tick: Optional[Callable[["FakeGame"], None]] = None
        self.tick_ms = tick_ms
        self.max_calls_per_tick = max_calls_per_tick
        self.main_thread_id = 0
        self.frames = 0
        self.paused_until = 0.0
        self._q: "queue.Queue[_Job]" = queue.Queue()
        self._running = False
        self._server: Optional[socketserver.ThreadingTCPServer] = None
        self._threads: List[threading.Thread] = []
        self._conns: List[socket.socket] = []
        self._lock = threading.Lock()
        self._seq = 0
        self._t0 = time.monotonic()
        self.custom: Dict[str, Callable[[Dict[str, Any]], Any]] = {}

    # ------------------------------------------------------------------------------------------ lifecycle
    @property
    def endpoint(self) -> str:
        return f"tcp://{self.host}:{self.port}"

    def start(self) -> "FakeGame":
        game = self

        class Handler(socketserver.StreamRequestHandler):
            def handle(self) -> None:
                game._serve(self.connection, self.rfile)

        class Server(socketserver.ThreadingTCPServer):
            allow_reuse_address = False
            daemon_threads = True

        self._server = Server((self.host, self.port), Handler)
        self.port = self._server.server_address[1]
        self._running = True
        for target in (self._server.serve_forever, self._main_loop):
            t = threading.Thread(target=target, daemon=True)
            t.start()
            self._threads.append(t)
        return self

    def stop(self) -> None:
        self._running = False
        self.kill_connections()
        if self._server is not None:
            self._server.shutdown()
            self._server.server_close()
            self._server = None
        for t in self._threads:
            t.join(timeout=2)
        self._threads = []

    def kill_connections(self) -> None:
        """Drop every client at once, like a game that crashed."""
        with self._lock:
            conns, self._conns = self._conns, []
        for c in conns:
            try:
                c.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                c.close()
            except OSError:
                pass

    def __enter__(self) -> "FakeGame":
        return self.start()

    def __exit__(self, *a: Any) -> None:
        self.stop()

    # ------------------------------------------------------------------------------------------ the main thread
    def pause_main(self, seconds: float) -> None:
        self.paused_until = time.monotonic() + seconds

    def _main_loop(self) -> None:
        self.main_thread_id = threading.get_ident()
        while self._running:
            if time.monotonic() >= self.paused_until:
                self.frames += 1
                if self.on_tick:
                    self.on_tick(self)
                for _ in range(self.max_calls_per_tick):
                    try:
                        job = self._q.get_nowait()
                    except queue.Empty:
                        break
                    try:
                        job.result = job.fn()
                    except FakeError as e:
                        job.error = {"code": e.code, "message": e.message}
                    except Exception as e:  # noqa: BLE001  (the adapter threw)
                        job.error = {"code": -32000, "message": str(e)}
                    job.done.set()
            time.sleep(self.tick_ms / 1000.0)

    def _on_main(self, fn: Callable[[], Any], timeout_s: float) -> Any:
        job = _Job(fn)
        self._q.put(job)
        if not job.done.wait(timeout_s):
            raise FakeError(-32001, "the main thread did not run the request in time (is the game loading?)")
        if job.error:
            raise FakeError(job.error["code"], job.error["message"])
        return job.result

    # ------------------------------------------------------------------------------------------ the game
    def push_event(self, type_: str, data: Any = None) -> None:
        with self._lock:
            self._seq += 1
            self.events.append({"seq": self._seq, "tMs": int((time.monotonic() - self._t0) * 1000), "type": type_,
                                "data": data if data is not None else {}})
            if len(self.events) > 1024:
                self.events.pop(0)

    def _value(self, tok: str) -> Any:
        tok = tok.strip()
        try:
            return float(tok)
        except ValueError:
            pass
        if tok in self.vars:
            return self.vars[tok]
        if tok == "menu":
            return self.menu
        raise FakeError(-32000, f"unknown name {tok}")

    def _eval(self, expr: str) -> Any:
        for op in ("+", "-"):
            if op in expr[1:]:
                i = expr.index(op, 1)
                a, b = self._value(expr[:i]), self._value(expr[i + 1:])
                return a + b if op == "+" else a - b
        return self._value(expr)

    def _exec(self, line: str) -> Dict[str, Any]:
        parts = line.split()
        if not parts:
            return {"ok": False, "output": ""}
        cmd, args = parts[0].lower(), parts[1:]
        if cmd == "set" and len(args) >= 2:
            raw = " ".join(args[1:])
            try:
                self.vars[args[0]] = float(raw)
            except ValueError:
                self.vars[args[0]] = raw
            return {"ok": True, "output": "ok"}
        if cmd == "get" and args:
            return {"ok": True, "output": f"{args[0]} = {self._value(args[0])}"}
        if cmd == "inc" and args:
            self.vars[args[0]] = float(self.vars.get(args[0], 0)) + (float(args[1]) if len(args) > 1 else 1.0)
            return {"ok": True, "output": f"{self.vars[args[0]]:g}"}
        if cmd == "say":
            return {"ok": True, "output": " ".join(args)}
        if cmd == "msgbox":
            self.menu = "message"
            self.push_event("ui.messagebox", {"text": " ".join(args)})
            return {"ok": True, "output": "ok"}
        if cmd == "dismiss":
            self.menu = "none"
            return {"ok": True, "output": "ok"}
        if cmd == "desync" and len(args) >= 3:
            self.push_event("sync.desync", {"key": args[0], "host": args[1], "client": args[2]})
            return {"ok": True, "output": "ok"}
        if cmd == "boom":
            raise RuntimeError("the adapter threw")
        if cmd == "whoami":
            return {"ok": True, "output": "main" if threading.get_ident() == self.main_thread_id else "other"}
        return {"ok": False, "output": f"Script command not found: {parts[0]}"}

    # ------------------------------------------------------------------------------------------ the server
    def _methods(self) -> List[str]:
        std = [m for m in ("exec", "eval", "waitFor", "state", "save", "load", "screenshot") if m not in self.unsupported]
        return ["ping", "version", "methods", "events"] + std + sorted(self.custom)

    def _events(self, p: Dict[str, Any]) -> Dict[str, Any]:
        since = int(p.get("since", 0) or 0)
        limit = int(p.get("limit", 0) or 0) or 1000
        with self._lock:
            evs = [e for e in self.events if e["seq"] > since][:limit]
            last = self._seq
            dropped = max(0, self.events[0]["seq"] - since - 1) if self.events else 0
        return {"events": evs, "next": evs[-1]["seq"] if evs else since, "dropped": dropped, "last": last}

    def _wait_for(self, p: Dict[str, Any]) -> Dict[str, Any]:
        timeout = float(p.get("timeoutMs", 30000)) / 1000.0
        poll = max(0.001, float(p.get("pollMs", 100)) / 1000.0)
        op = p.get("op", "truthy")
        rhs = float(p.get("value", 0))
        t0 = time.monotonic()
        last: Any = None
        while True:
            if "expr" in p:
                v = self._on_main(lambda: self._eval(p["expr"]), timeout + 2)
            else:
                v = self._on_main(lambda: self._exec(p["cmd"]), timeout + 2)["output"]
            last = v
            if "contains" in p:
                ok = p["contains"] in str(v)
            else:
                num = v if isinstance(v, (int, float)) else None
                if num is None:
                    try:
                        num = float("".join(c for c in str(v).split()[0] if c in "0123456789.-"))
                    except (ValueError, IndexError):
                        num = None
                ok = num is not None and {
                    "truthy": num != 0, "==": num == rhs, "!=": num != rhs, "<": num < rhs, "<=": num <= rhs,
                    ">": num > rhs, ">=": num >= rhs}[op]
            if ok:
                return {"satisfied": True, "timedOut": False, "value": last, "elapsedMs": int((time.monotonic() - t0) * 1000)}
            if time.monotonic() - t0 >= timeout:
                return {"satisfied": False, "timedOut": True, "value": last, "elapsedMs": int((time.monotonic() - t0) * 1000)}
            time.sleep(poll)

    def handle(self, method: str, p: Dict[str, Any]) -> Any:
        """Run one request and return the result, or raise FakeError."""
        if method == "ping":
            return {"pong": True, "protocol": self.protocol, "game": self.name, "pid": self.pid, "frames": self.frames,
                    "mainLoopAgeMs": 0, "ready": True, "loadedCount": 0}
        if method == "version":
            return {"protocol": self.protocol, "game": self.name, "build": "fake-1", "server": "fake_game.py", "pid": self.pid}
        if method == "methods":
            return {"methods": self._methods()}
        if method == "events":
            return self._events(p)
        if method in self.unsupported:
            raise FakeError(-32601, f"this game does not support {method}")
        timeout = float(p.get("timeoutMs", 30000)) / 1000.0 + 2.0
        if method in self.custom:
            fn = self.custom[method]
            return self._on_main(lambda: fn(p), timeout)
        if method == "exec":
            if not isinstance(p.get("cmd"), str):
                raise FakeError(-32602, "exec requires string param 'cmd'")
            self.calls.append("exec")
            return self._on_main(lambda: self._exec(p["cmd"]), timeout)
        if method == "eval":
            if not isinstance(p.get("expr"), str):
                raise FakeError(-32602, "eval requires string param 'expr'")
            self.calls.append("eval")
            return {"value": self._on_main(lambda: self._eval(p["expr"]), timeout)}
        if method == "waitFor":
            if not isinstance(p.get("expr"), str) and not isinstance(p.get("cmd"), str):
                raise FakeError(-32602, "waitFor requires string param 'expr' or 'cmd'")
            self.calls.append("waitFor")
            return self._wait_for(p)
        if method == "state":
            self.calls.append("state")
            secs = p.get("sections") or []
            snap = self._on_main(lambda: {k: v for k, v in self.state.items() if not secs or k in secs}, timeout)
            if isinstance(p.get("path"), str):
                with open(p["path"], "w", encoding="utf-8") as f:
                    json.dump(snap, f)
                return {"path": p["path"], "bytes": os.path.getsize(p["path"])}
            return snap
        if method == "load":
            name = p.get("name")
            if not isinstance(name, str) or not name:
                raise FakeError(-32602, "load requires string param 'name'")
            if name not in self.saves:
                raise FakeError(-32000, f"no such save: {name}")
            self.calls.append("load")
            time.sleep(0.05)
            self.push_event("game.loaded")
            return {"loaded": True, "name": name}
        if method == "save":
            name = p.get("name")
            if not isinstance(name, str) or not name:
                raise FakeError(-32602, "save requires string param 'name'")
            if name in self.saves and not p.get("overwrite"):
                raise FakeError(-32012, f"save '{name}' already exists (pass overwrite:true to replace it)")
            self.calls.append("save")
            self.saves[name] = f"{name}.fos"
            self.push_event("game.saved", {"path": self.saves[name]})
            return {"saved": True, "path": self.saves[name]}
        if method == "screenshot":
            path = p.get("path") or os.path.join(os.environ.get("TEMP", "."), f"{self.name}-shot.png")
            with open(path, "wb") as f:
                f.write(b"\x89PNG\r\n\x1a\n")
            self.calls.append("screenshot")
            return {"ok": True, "path": path}
        raise FakeError(-32601, f"unknown method: {method}")

    def _serve(self, conn: socket.socket, rfile: Any) -> None:
        with self._lock:
            self._conns.append(conn)
        try:
            for raw in rfile:
                line = raw.strip()
                if not line:
                    continue
                if not self._one(conn, line):
                    return
        except OSError:
            return

    def _one(self, conn: socket.socket, line: bytes) -> bool:
        try:
            req = json.loads(line.decode("utf-8"))
        except ValueError:
            conn.sendall(json.dumps({"id": None, "error": {"code": -32700, "message": "parse error"}}).encode() + b"\n")
            return True
        if not isinstance(req, dict) or not isinstance(req.get("method"), str):
            conn.sendall(json.dumps({"id": None, "error": {"code": -32600, "message": "missing method"}}).encode() + b"\n")
            return True
        rid, method = req.get("id"), req["method"]
        params = req.get("params") or {}
        fault = self.faults.get(method)
        if fault and fault.get("once"):
            self.faults.pop(method, None)
        kind = fault.get("kind") if fault else None
        if kind == "close":
            conn.shutdown(socket.SHUT_RDWR)
            return False
        if kind == "delay":
            time.sleep(float(fault.get("seconds", 1.0)))
        try:
            resp: Dict[str, Any] = {"id": rid, "result": self.handle(method, params)}
        except FakeError as e:
            resp = {"id": rid, "error": {"code": e.code, "message": e.message}}
        if kind == "wrong_id":
            resp["id"] = (rid if isinstance(rid, int) else 0) + 1
        data = json.dumps(resp).encode("utf-8") + b"\n"
        if kind == "garbage":
            data = b"this is not json\n"
        if kind == "split":
            half = len(data) // 2
            conn.sendall(data[:half])
            time.sleep(float(fault.get("seconds", 0.2)))
            conn.sendall(data[half:])
        else:
            conn.sendall(data)
        return True


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="A pretend game with an in-game RPC server")
    ap.add_argument("--name", default="fake")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--set", action="append", default=[], metavar="NAME=VALUE", help="start with a variable set")
    a = ap.parse_args(argv)
    vars_: Dict[str, Any] = {}
    for kv in a.set:
        k, _, v = kv.partition("=")
        try:
            vars_[k] = float(v)
        except ValueError:
            vars_[k] = v
    g = FakeGame(a.name, vars=vars_, port=a.port).start()
    print(json.dumps({"endpoint": g.endpoint, "pid": g.pid}), flush=True)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        g.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
