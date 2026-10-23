"""Runs the C++ server template (include/coopbridge/server.h, embedded in cpp/test_server.cpp) against the Python client.

    COOPBRIDGE_TEST_EXE=<path to coopbridge_test[.exe]> python -I tools/bridge/test_cpp_server.py

run_tests.py builds the program and runs this file once for the real header (must pass) and once for each broken header
copy, a mutation twin (must fail). Nothing here starts a game.
"""
import json
import os
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import bridge_client as bc  # noqa: E402

EXE = os.environ.get("COOPBRIDGE_TEST_EXE", "")
WINDOWS = os.name == "nt"
TMP = tempfile.mkdtemp(prefix="coopbridge-test-")


class Proc:
    """The pretend game as a child process. Only processes started here are ever stopped here."""

    def __init__(self, *args):
        self.p = subprocess.Popen([EXE, *args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.lines = queue.Queue()
        threading.Thread(target=self._drain, daemon=True).start()
        self.ready = None
        self.failed = None
        try:
            first = self.lines.get(timeout=15)
        except queue.Empty:
            self.p.kill()
            raise RuntimeError("the test game printed nothing")
        if first.startswith("READY "):
            self.ready = json.loads(first[6:])
        else:
            self.failed = first.strip()

    def _drain(self):
        for line in self.p.stdout:
            self.lines.put(line)

    @property
    def port(self):
        return self.ready["port"]

    @property
    def tcp(self):
        return f"tcp://127.0.0.1:{self.port}"

    def connect(self, **kw):
        return bc.Bridge.connect(self.tcp, wait_s=5, **kw)

    def stop(self):
        if self.p.poll() is None:
            if self.ready:
                try:
                    with self.connect() as g:
                        g.call("test.quit", timeout_s=3)
                except bc.BridgeError:
                    pass
            try:
                self.p.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.p.kill()
        self.p.wait()
        self.p.stdout.close()


class Raw:
    """A bare socket, to send lines the client would never send."""

    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.buf = b""

    def send(self, data):
        self.s.sendall(data)

    def line(self, timeout=5):
        self.s.settimeout(timeout)
        while b"\n" not in self.buf:
            chunk = self.s.recv(65536)
            if not chunk:
                return None
            self.buf += chunk
        raw, self.buf = self.buf.split(b"\n", 1)
        return raw

    def msg(self, timeout=5):
        raw = self.line(timeout)
        return None if raw is None else json.loads(raw.decode("utf-8"))

    def close(self):
        self.s.close()


MAIN = None
VARIANT = None
PIPE = "coopbridge-test-" + uuid.uuid4().hex[:8]
ENDPOINT_FILE = os.path.join(TMP, "endpoint.json")


def setUpModule():
    global MAIN, VARIANT
    if not EXE or not os.path.exists(EXE):
        raise RuntimeError("set COOPBRIDGE_TEST_EXE to the built coopbridge_test program")
    args = ["--endpoint-file", ENDPOINT_FILE, "--budget", "8"]
    if WINDOWS:
        args += ["--pipe", PIPE]
    MAIN = Proc(*args)
    VARIANT = Proc("--event-capacity", "8", "--max-pending", "5", "--max-line", "1024", "--no-screenshot")
    assert MAIN.ready, MAIN.failed
    assert VARIANT.ready, VARIANT.failed


def tearDownModule():
    for p in (MAIN, VARIANT):
        if p:
            p.stop()


def burst(proc, n, line="inc n"):
    """Pause the game's main thread, then make n clients call exec at once. Returns the list of outcomes (ok or an error code)."""
    clients = [proc.connect() for _ in range(n)]
    ctl = proc.connect()
    out = []
    lock = threading.Lock()

    def one(g):
        try:
            g.exec(line)
            r = "ok"
        except bc.RpcError as e:
            r = e.code
        with lock:
            out.append(r)

    ctl.call("test.pause_pump", {"ms": 1200})
    time.sleep(0.15)
    threads = [threading.Thread(target=one, args=(g,)) for g in clients]
    for t in threads:
        t.start()
    for t in threads:
        t.join(20)
    for g in clients + [ctl]:
        g.close()
    return out


class ServerTests(unittest.TestCase):
    def setUp(self):
        self.g = MAIN.connect()

    def tearDown(self):
        self.g.close()

    def test_ready_endpoint_file_and_endpoints(self):
        eps = MAIN.ready["endpoints"]
        self.assertTrue(eps[0].startswith("tcp://127.0.0.1:"))
        if WINDOWS:
            self.assertIn("pipe://" + PIPE, eps)
        self.assertEqual(bc.endpoint_from_file(ENDPOINT_FILE), eps[0])
        with open(ENDPOINT_FILE, encoding="utf-8") as f:
            data = json.load(f)
        self.assertEqual(data["protocol"], 1)
        self.assertEqual(data["game"], "cppfake")

    def test_ping_version_methods(self):
        p = self.g.ping()
        self.assertEqual((p["protocol"], p["game"], p["ready"]), (1, "cppfake", True))
        self.assertGreater(p["pid"], 0)
        self.assertEqual(self.g.game_pid, p["pid"])
        v = self.g.version()
        self.assertEqual((v["protocol"], v["game"], v["build"]), (1, "cppfake", "test-1"))
        m = self.g.methods()
        for name in ("ping", "version", "methods", "events", "exec", "eval", "waitFor", "state", "save", "load", "screenshot", "test.echo"):
            self.assertIn(name, m)

    def test_calls_run_on_the_main_thread(self):
        self.assertEqual(self.g.exec("whoami")["output"], "main")
        self.assertTrue(self.g.call("test.on_main")["onPumpThread"])

    def test_exec_eval_state_screenshot(self):
        self.assertEqual(self.g.exec("set score 41"), {"ok": True, "output": "ok"})
        self.assertEqual(self.g.exec("inc score")["output"], "42")
        self.assertEqual(self.g.exec("say hello  world")["output"], "hello  world")
        r = self.g.exec("nonsense")
        self.assertFalse(r["ok"])
        self.assertIn("not found", r["output"])
        self.assertEqual(self.g.eval("score"), 42)
        self.assertEqual(self.g.eval("2.5"), 2.5)
        self.assertEqual(self.g.state(["vars"])["vars"]["score"], 42)
        self.assertNotIn("menu", self.g.state(["vars"]))
        path = os.path.join(TMP, "state.json")
        r = self.g.state_to_file(path)
        self.assertGreater(r["bytes"], 0)
        with open(path, encoding="utf-8") as f:
            self.assertEqual(json.load(f)["vars"]["score"], 42)
        self.assertTrue(self.g.screenshot(os.path.join(TMP, "a.png"))["ok"])

    def test_wait_for(self):
        self.g.exec("set door 0")

        def later():
            time.sleep(0.3)
            with MAIN.connect() as g2:
                g2.exec("set door 5")
        t = threading.Thread(target=later)
        t.start()
        r = self.g.wait_for(expr="door", op=">=", value=5, timeout_ms=5000, poll_ms=20)
        t.join()
        self.assertEqual(r["value"], 5)
        self.assertGreaterEqual(r["elapsedMs"], 200)
        r = self.g.wait_for(cmd="say ready now", contains="now", timeout_ms=2000)
        self.assertTrue(r["satisfied"])
        r = self.g.wait_for(cmd="get door", op="==", value=5, timeout_ms=2000)
        self.assertEqual(r["value"], 5)

    def test_wait_for_timeout_is_a_result_not_a_hang(self):
        self.g.exec("set never 0")
        t0 = time.monotonic()
        with self.assertRaises(bc.WaitTimeout):
            self.g.wait_for(expr="never", timeout_ms=400, poll_ms=20)
        self.assertLess(time.monotonic() - t0, 3)
        raw = self.g.call("waitFor", {"expr": "never", "timeoutMs": 300, "pollMs": 20}, timeout_s=5)
        self.assertEqual((raw["satisfied"], raw["timedOut"]), (False, True))
        self.assertGreaterEqual(raw["elapsedMs"], 250)

    def test_save_and_load(self):
        name = "s" + uuid.uuid4().hex[:6]
        r = self.g.save(name, timeout_ms=5000)
        self.assertEqual(r, {"saved": True, "path": name + ".sav"})
        with self.assertRaises(bc.RpcError) as cm:
            self.g.save(name, timeout_ms=5000)
        self.assertEqual(cm.exception.code, -32012)
        self.assertTrue(self.g.save(name, timeout_ms=5000, overwrite=True)["saved"])
        r = self.g.load(name, timeout_ms=5000)
        self.assertEqual((r["loaded"], r["name"]), (True, name))
        ev = self.g.wait_event("game.loaded", timeout_s=3)
        self.assertEqual(ev["type"], "game.loaded")
        with self.assertRaises(bc.RpcError) as cm:
            self.g.load("missing", timeout_ms=2000)
        self.assertEqual(cm.exception.code, -32000)
        self.assertIn("no such save", cm.exception.message)

    def test_save_that_never_finishes_times_out_with_its_own_code(self):
        t0 = time.monotonic()
        with self.assertRaises(bc.RpcError) as cm:
            self.g.save("slowsave", timeout_ms=400)
        self.assertEqual(cm.exception.code, -32002)
        self.assertLess(time.monotonic() - t0, 3)

    def test_errors_keep_their_code_and_message(self):
        with self.assertRaises(bc.RpcError) as cm:
            self.g.exec("boom")
        self.assertEqual(cm.exception.code, -32000)
        self.assertIn("the adapter threw", cm.exception.message)
        with self.assertRaises(bc.RpcError) as cm:
            self.g.exec("throw")
        self.assertEqual((cm.exception.code, cm.exception.message), (-32050, "custom adapter error"))
        with self.assertRaises(bc.RpcError) as cm:
            self.g.eval("no_such_name")
        self.assertEqual(cm.exception.code, -32000)
        with self.assertRaises(bc.RpcError) as cm:
            self.g.call("exec", {})
        self.assertEqual(cm.exception.code, -32602)
        with self.assertRaises(bc.RpcError) as cm:
            self.g.call("no.such.method")
        self.assertEqual(cm.exception.code, -32601)
        self.assertIn("no.such.method", cm.exception.message)
        # the connection still works after errors
        self.assertEqual(self.g.exec("say fine")["output"], "fine")

    def test_unsupported_method_is_not_found_and_not_listed(self):
        with VARIANT.connect() as v:
            self.assertNotIn("screenshot", v.methods())
            with self.assertRaises(bc.RpcError) as cm:
                v.screenshot()
            self.assertEqual(cm.exception.code, -32601)
            with self.assertRaises(bc.BridgeError):
                v.require("screenshot")
            v.require("exec", "eval")

    def test_framing_and_bad_requests(self):
        r = Raw(MAIN.port)
        try:
            r.send(b"not json\n")
            m = r.msg()
            self.assertIsNone(m["id"])
            self.assertEqual(m["error"]["code"], -32700)
            r.send(b"[1]\n")
            self.assertEqual(r.msg()["error"]["code"], -32600)
            r.send(b'{"id": 7}\n')
            m = r.msg()
            self.assertEqual((m["id"], m["error"]["code"]), (7, -32600))
            r.send(b'{"id": 8, "method": "ping", "params": [1]}\n')
            m = r.msg()
            self.assertEqual((m["id"], m["error"]["code"]), (8, -32602))
            r.send(b'{"id": {"a": 1}, "method": "ping"}\n')
            self.assertEqual(r.msg()["error"]["code"], -32600)
            r.send(b'{"id": "abc", "method": "ping"}\r\n')
            m = r.msg()
            self.assertEqual(m["id"], "abc")
            self.assertTrue(m["result"]["pong"])
            r.send(b'{"id": 1, "method": "ping"}\n{"id": 2, "method": "version"}\n{"id": 3, "method": "exec", "params": {"cmd": "say x"}}\n')
            ids = [r.msg()["id"] for _ in range(3)]
            self.assertEqual(ids, [1, 2, 3])
            part = b'{"id": 9, "method": "ping"}\n'
            r.send(part[:10])
            time.sleep(0.3)
            r.send(part[10:])
            self.assertEqual(r.msg()["id"], 9)
        finally:
            r.close()

    def test_invalid_utf8_from_the_game_is_replaced(self):
        r = Raw(MAIN.port)
        try:
            r.send(b'{"id": 1, "method": "test.bytes"}\n')
            raw = r.line()
            text = raw.decode("utf-8")           # strict: must be valid UTF-8
            self.assertEqual(json.loads(text)["result"]["text"], "ab\ufffdcd")
        finally:
            r.close()
        self.assertEqual(self.g.call("test.echo", {"s": "caf\u00e9 \u4e2d \U0001F600"})["s"], "caf\u00e9 \u4e2d \U0001F600")

    def test_main_thread_timeout_and_abandoned_request(self):
        self.g.call("test.pause_pump", {"ms": 2500})
        time.sleep(0.15)
        t0 = time.monotonic()
        with self.assertRaises(bc.RpcError) as cm:
            self.g.exec("set late 1", timeout_ms=300)
        self.assertEqual(cm.exception.code, -32001)
        self.assertLess(time.monotonic() - t0, 1.8)
        self.assertGreaterEqual(self.g.ping()["mainLoopAgeMs"], 50)     # ping is answered while the main thread is stuck
        time.sleep(max(0.0, 2.7 - (time.monotonic() - t0)))
        with self.assertRaises(bc.RpcError) as cm:                       # the abandoned request never ran
            self.g.eval("late")
        self.assertEqual(cm.exception.code, -32000)

    def test_requests_per_pump_are_capped(self):
        self.g.exec("set n 0")
        out = burst(MAIN, 40)
        self.assertEqual(out, ["ok"] * 40)
        self.assertEqual(self.g.eval("n"), 40)
        st = self.g.call("test.pump_stats")
        self.assertLessEqual(st["maxDrained"], 8)
        self.assertGreaterEqual(st["served"], 40)

    def test_queue_limit_answers_busy(self):
        out = burst(VARIANT, 12)
        self.assertEqual(len(out), 12)
        self.assertGreaterEqual(out.count(-32003), 1)
        self.assertGreaterEqual(out.count("ok"), 1)
        self.assertEqual(out.count("ok") + out.count(-32003), 12)
        with VARIANT.connect() as v:
            self.assertGreaterEqual(v.call("test.pump_stats")["rejectedBusy"], 1)

    def test_many_clients_do_not_lose_updates(self):
        self.g.exec("set c 0")

        def work():
            with MAIN.connect() as g2:
                for _ in range(25):
                    g2.exec("inc c")
        ts = [threading.Thread(target=work) for _ in range(8)]
        for t in ts:
            t.start()
        for t in ts:
            t.join(30)
        self.assertEqual(self.g.eval("c"), 200)

    def test_events_cursor_and_overflow(self):
        e0 = self.g.events()
        for i in range(3):
            self.g.call("test.event", {"type": "t.one", "data": {"i": i}})
        page = self.g.events(e0["next"])
        self.assertEqual([e["data"]["i"] for e in page["events"] if e["type"] == "t.one"], [0, 1, 2])
        self.assertEqual(page["dropped"], 0)
        again = self.g.events(page["next"])
        self.assertEqual(again["events"], [])
        self.assertEqual(again["next"], page["next"])
        self.assertEqual(page["last"], page["next"])
        with self.assertRaises(bc.WaitTimeout):
            self.g.wait_event("t.two", since=page["next"], timeout_s=0.4)
        with VARIANT.connect() as v:                       # capacity 8
            base = v.events()["last"]
            for i in range(20):
                v.call("test.event", {"type": "t.many", "data": {"i": i}})
            p = v.events(base)
            self.assertEqual(len(p["events"]), 8)
            self.assertEqual(p["dropped"], 12)
            self.assertEqual(p["events"][-1]["data"]["i"], 19)
            self.assertEqual(p["next"], base + 20)
            lim = v.events(base + 12, limit=3)
            self.assertEqual(len(lim["events"]), 3)
            self.assertEqual(lim["next"], base + 15)

    def test_line_longer_than_the_limit_is_refused(self):
        r = Raw(VARIANT.port)
        try:
            r.send(b"x" * 3000)
            m = r.msg()
            self.assertEqual(m["error"]["code"], -32600)
            self.assertIn("too long", m["error"]["message"])
        finally:
            r.close()

    @unittest.skipUnless(WINDOWS, "named pipes need Windows")
    def test_named_pipe_transport(self):
        with bc.Bridge.connect("pipe://" + PIPE, wait_s=5) as a, bc.Bridge.connect("pipe://" + PIPE, wait_s=5) as b:
            self.assertEqual(a.ping()["game"], "cppfake")
            self.assertEqual(a.exec("set pv 7")["output"], "ok")
            self.assertEqual(b.eval("pv"), 7)
            self.assertEqual(b.exec("whoami")["output"], "main")
            for _ in range(50):
                a.exec("say x")
        with bc.Bridge.connect("pipe://" + PIPE, wait_s=5) as again:       # a client that left does not block the next
            self.assertEqual(again.eval("pv"), 7)
        with self.assertRaises(bc.ConnectionLost):
            bc.Bridge.connect("pipe://coopbridge-no-such-pipe", wait_s=0)


class StartStopTests(unittest.TestCase):
    def test_non_loopback_host_is_refused(self):
        p = Proc("--host", "0.0.0.0")
        try:
            self.assertIsNone(p.ready, "the server listened on 0.0.0.0")
            self.assertIn("loopback", p.failed)
            self.assertEqual(p.p.wait(timeout=5), 3)
        finally:
            p.stop()

    def test_a_port_in_use_is_reported(self):
        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        s.listen(1)
        try:
            p = Proc("--port", str(s.getsockname()[1]))
            try:
                self.assertIsNone(p.ready)
                self.assertIn("cannot bind", p.failed)
            finally:
                p.stop()
        finally:
            s.close()

    def test_stop_closes_everything_and_removes_the_endpoint_file(self):
        f = os.path.join(TMP, "stop-endpoint.json")
        args = ["--endpoint-file", f]
        pipe = "coopbridge-stop-" + uuid.uuid4().hex[:8]
        if WINDOWS:
            args += ["--pipe", pipe]
        p = Proc(*args)
        try:
            self.assertTrue(os.path.exists(f))
            keep = [p.connect()]
            if WINDOWS:
                keep.append(bc.Bridge.connect("pipe://" + pipe, wait_s=5))
            keep[0].call("test.quit", timeout_s=3)
            self.assertEqual(p.p.wait(timeout=8), 0)
            self.assertFalse(os.path.exists(f))
            for k in keep:
                k.close()
        finally:
            p.stop()


if __name__ == "__main__":
    unittest.main(verbosity=1)
