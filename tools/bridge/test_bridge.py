"""Tests for the Python client (bridge_client.py) against the fake game (fake_game.py). No real game is started.

    python -I tools/bridge/test_bridge.py

COOPBRIDGE_CLIENT_DIR names a folder that holds a bridge_client.py to test instead of the real one. run_tests.py uses it
to run these same tests against broken copies of the client (the mutation twins), and every one of them must fail.
"""
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
CLIENT_DIR = os.environ.get("COOPBRIDGE_CLIENT_DIR") or str(HERE)
sys.path.insert(0, CLIENT_DIR)
sys.path.insert(1, str(HERE))
import bridge_client as bc  # noqa: E402
from fake_game import FakeGame  # noqa: E402

TMP = tempfile.mkdtemp(prefix="coopbridge-pytest-")


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def dead_pid():
    p = subprocess.Popen([sys.executable, "-I", "-c", "pass"])
    p.wait()
    return p.pid


class Base(unittest.TestCase):
    def game(self, **kw):
        g = FakeGame(**kw).start()
        self.addCleanup(g.stop)
        return g

    def client(self, g, **kw):
        b = bc.Bridge.connect(g.endpoint, wait_s=2, **kw)
        self.addCleanup(b.close)
        return b


class StandardMethodTests(Base):
    def test_a_game_with_a_newer_protocol_is_refused_with_a_clear_message(self):
        g = self.game()
        g.protocol = 2
        with self.assertRaises(bc.ProtocolError) as cm:
            bc.Bridge.connect(g.endpoint, wait_s=1)
        self.assertIn("protocol 2", str(cm.exception))

    def test_ping_version_methods(self):
        g = self.game(name="alpha")
        b = self.client(g)
        self.assertEqual(b.game_pid, g.pid)
        p = b.ping()
        self.assertEqual((p["pong"], p["protocol"], p["game"]), (True, 1, "alpha"))
        self.assertEqual(b.version()["protocol"], 1)
        self.assertEqual(b.methods(), ["ping", "version", "methods", "events", "exec", "eval", "waitFor", "state", "save", "load", "screenshot"])

    def test_exec_eval_state(self):
        g = self.game(vars={"health": 80}, state={"menu": {"kind": "none"}, "sync": {"lastSeq": 4}})
        b = self.client(g)
        self.assertEqual(b.exec("set health 55"), {"ok": True, "output": "ok"})
        self.assertEqual(b.eval("health"), 55)
        self.assertEqual(b.eval("health+5"), 60)
        r = b.exec("bogus")
        self.assertFalse(r["ok"])
        self.assertIn("not found", r["output"])
        self.assertEqual(b.state(["sync"]), {"sync": {"lastSeq": 4}})
        self.assertEqual(set(b.state()), {"menu", "sync"})

    def test_state_to_file_makes_the_path_absolute(self):
        g = self.game(state={"a": 1})
        b = self.client(g)
        old = os.getcwd()
        os.chdir(TMP)
        try:
            r = b.state_to_file("rel-state.json")
        finally:
            os.chdir(old)
        self.assertTrue(os.path.isabs(r["path"]), r["path"])
        self.assertTrue(os.path.exists(r["path"]))

    def test_save_load_screenshot_events(self):
        g = self.game()
        b = self.client(g)
        self.assertEqual(b.save("base")["saved"], True)
        self.assertEqual(b.load("base")["loaded"], True)
        self.assertTrue(b.screenshot(os.path.join(TMP, "s.png"))["ok"])
        page = b.events()
        self.assertEqual([e["type"] for e in page["events"]], ["game.saved", "game.loaded"])
        self.assertEqual(b.events(page["next"])["events"], [])

    def test_wait_for(self):
        g = self.game(vars={"door": 0})
        b = self.client(g)
        t = threading.Timer(0.3, lambda: g.vars.__setitem__("door", 3))
        t.start()
        r = b.wait_for(expr="door", op=">=", value=3, timeout_ms=4000, poll_ms=20)
        t.join()
        self.assertEqual(r["value"], 3)
        self.assertTrue(b.wait_for(cmd="say a b c", contains="b c", timeout_ms=1000)["satisfied"])
        with self.assertRaises(ValueError):
            b.wait_for()

    def test_wait_for_that_never_holds_raises(self):
        g = self.game(vars={"door": 0})
        b = self.client(g)
        t0 = time.monotonic()
        with self.assertRaises(bc.WaitTimeout) as cm:
            b.wait_for(expr="door", op=">", value=9, timeout_ms=300, poll_ms=20)
        self.assertEqual(cm.exception.last, 0)
        self.assertLess(time.monotonic() - t0, 3)

    def test_wait_event_respects_the_cursor(self):
        g = self.game()
        b = self.client(g)
        g.push_event("a", {"n": 1})
        g.push_event("a", {"n": 2})
        first = b.wait_event("a", timeout_s=1)
        self.assertEqual(first["data"]["n"], 1)
        second = b.wait_event("a", since=first["seq"], timeout_s=1)
        self.assertEqual(second["data"]["n"], 2)
        with self.assertRaises(bc.WaitTimeout):
            b.wait_event("a", since=second["seq"], timeout_s=0.3, poll_s=0.05)
        threading.Timer(0.2, lambda: g.push_event("b")).start()
        self.assertEqual(b.wait_event("b", since=second["seq"], timeout_s=2)["type"], "b")


class ErrorTests(Base):
    def test_game_errors_become_rpc_errors(self):
        g = self.game()
        b = self.client(g)
        with self.assertRaises(bc.RpcError) as cm:
            b.exec("boom")
        self.assertEqual((cm.exception.code, cm.exception.method), (-32000, "exec"))
        self.assertIn("the adapter threw", cm.exception.message)
        b.save("x")
        with self.assertRaises(bc.RpcError) as cm:
            b.save("x")
        self.assertEqual(cm.exception.code, -32012)
        with self.assertRaises(bc.RpcError) as cm:
            b.call("no.such")
        self.assertEqual(cm.exception.code, -32601)
        with self.assertRaises(bc.RpcError) as cm:
            b.eval("nothing_here")
        self.assertEqual(cm.exception.code, -32000)
        self.assertEqual(b.exec("say still fine")["output"], "still fine")     # the connection survives errors

    def test_unsupported_method(self):
        g = self.game(unsupported=["screenshot"])
        b = self.client(g)
        self.assertNotIn("screenshot", b.methods())
        with self.assertRaises(bc.RpcError) as cm:
            b.screenshot()
        self.assertEqual(cm.exception.code, -32601)
        with self.assertRaises(bc.BridgeError):
            b.require("exec", "screenshot")
        b.require("exec", "eval")

    def test_no_reply_in_time_is_a_timeout_and_the_late_reply_is_skipped(self):
        g = self.game()
        b = self.client(g)
        g.faults["exec"] = {"kind": "delay", "seconds": 1.2, "once": True}
        t0 = time.monotonic()
        with self.assertRaises(bc.CallTimeout) as cm:
            b.call("exec", {"cmd": "say first"}, timeout_s=0.3)
        self.assertLess(time.monotonic() - t0, 1.0)
        self.assertEqual(cm.exception.method, "exec")
        r = b.call("exec", {"cmd": "say second"}, timeout_s=5)         # the first reply arrives meanwhile and is skipped
        self.assertEqual(r["output"], "second")

    def test_a_reply_with_the_wrong_id_is_a_protocol_error(self):
        g = self.game()
        b = self.client(g)
        g.faults["exec"] = {"kind": "wrong_id", "once": True}
        with self.assertRaises(bc.ProtocolError):
            b.exec("say hi")

    def test_garbage_reply_is_a_protocol_error(self):
        g = self.game()
        b = self.client(g)
        g.faults["exec"] = {"kind": "garbage", "once": True}
        with self.assertRaises(bc.ProtocolError):
            b.exec("say hi")

    def test_a_reply_that_arrives_in_two_pieces(self):
        g = self.game()
        b = self.client(g)
        g.faults["exec"] = {"kind": "split", "seconds": 0.25, "once": True}
        self.assertEqual(b.exec("say whole")["output"], "whole")

    def test_big_reply(self):
        g = self.game(state={"blob": "x" * 2_000_000})
        b = self.client(g)
        self.assertEqual(len(b.state()["blob"]), 2_000_000)

    def test_dropped_connection_is_connection_lost(self):
        g = self.game()              # this process is alive, so it is not a crash
        b = self.client(g)
        g.faults["exec"] = {"kind": "close", "once": True}
        with self.assertRaises(bc.ConnectionLost) as cm:
            b.exec("say x")
        self.assertNotIsInstance(cm.exception, bc.GameCrashed)

    def test_dropped_connection_of_a_dead_game_is_a_crash(self):
        g = self.game(pid=dead_pid())
        b = self.client(g)
        self.assertFalse(bc.pid_alive(b.game_pid))
        g.faults["exec"] = {"kind": "close", "once": True}
        with self.assertRaises(bc.GameCrashed) as cm:
            b.exec("say x")
        self.assertIsInstance(cm.exception, bc.ConnectionLost)
        self.assertEqual(cm.exception.pid, g.pid)

    def test_pid_alive(self):
        self.assertTrue(bc.pid_alive(os.getpid()))
        self.assertFalse(bc.pid_alive(dead_pid()))
        self.assertFalse(bc.pid_alive(0))

    def test_calls_from_two_threads_do_not_mix_replies(self):
        g = self.game()
        b = self.client(g)
        g.faults["exec"] = {"kind": "delay", "seconds": 0.25, "once": True}
        out = {}

        def run(key, text):
            try:
                out[key] = b.call("exec", {"cmd": "say " + text}, timeout_s=3)["output"]
            except bc.BridgeError as e:
                out[key] = repr(e)
        a = threading.Thread(target=run, args=("a", "alpha"))
        a.start()
        time.sleep(0.08)
        c = threading.Thread(target=run, args=("b", "beta"))
        c.start()
        a.join(10)
        c.join(10)
        self.assertEqual(out, {"a": "alpha", "b": "beta"})


class ConnectTests(Base):
    def test_connect_waits_for_the_game_to_start(self):
        port = free_port()
        holder = {}

        def late():
            time.sleep(3.5)         # a refused connect to a closed port can take about 2 s on Windows
            holder["g"] = FakeGame(port=port).start()
        t = threading.Thread(target=late)
        t.start()
        try:
            b = bc.Bridge.connect(f"tcp://127.0.0.1:{port}", wait_s=15)
            self.assertEqual(b.ping()["pong"], True)
            b.close()
        finally:
            t.join()
            if "g" in holder:
                holder["g"].stop()

    def test_connect_without_waiting_fails_fast_with_a_clear_error(self):
        t0 = time.monotonic()
        with self.assertRaises(bc.ConnectionLost) as cm:
            bc.Bridge.connect(f"tcp://127.0.0.1:{free_port()}", wait_s=0)
        self.assertLess(time.monotonic() - t0, 3)
        self.assertIn("cannot connect", str(cm.exception))

    def test_a_server_that_never_answers_the_ping_is_an_error(self):
        srv = socket.socket()
        srv.bind(("127.0.0.1", 0))
        srv.listen(1)
        self.addCleanup(srv.close)
        with self.assertRaises(bc.CallTimeout):
            bc.Bridge.connect(f"tcp://127.0.0.1:{srv.getsockname()[1]}", ping_timeout_s=0.3)

    def test_endpoint_parsing(self):
        self.assertEqual(bc.parse_endpoint("tcp://127.0.0.1:5000"), ("tcp", "127.0.0.1", 5000))
        self.assertEqual(bc.parse_endpoint("localhost:81"), ("tcp", "localhost", 81))
        self.assertEqual(bc.parse_endpoint("pipe://mygame-0"), ("pipe", "mygame-0", None))
        for bad in ("", "tcp://nohost", "tcp://h:notaport", "pipe://", "5000"):
            with self.assertRaises(bc.BridgeError, msg=bad):
                bc.parse_endpoint(bad)

    def test_endpoint_file(self):
        path = os.path.join(TMP, "ep.json")
        if os.path.exists(path):
            os.remove(path)
        with self.assertRaises(bc.ConnectionLost):
            bc.endpoint_from_file(path)

        def write():
            time.sleep(0.3)
            with open(path, "w", encoding="utf-8") as f:
                json.dump({"endpoints": ["pipe://p", "tcp://127.0.0.1:9"]}, f)
        t = threading.Thread(target=write)
        t.start()
        self.assertEqual(bc.endpoint_from_file(path, wait_s=3), "tcp://127.0.0.1:9")
        self.assertEqual(bc.endpoint_from_file(path, prefer="pipe"), "pipe://p")
        t.join()

    def test_endpoint_from_the_environment(self):
        g = self.game()
        old = os.environ.get(bc.ENV_ENDPOINT)
        os.environ[bc.ENV_ENDPOINT] = g.endpoint
        try:
            b = bc.Bridge.connect()
            self.assertTrue(b.ping()["pong"])
            b.close()
            del os.environ[bc.ENV_ENDPOINT]
            with self.assertRaises(bc.BridgeError):
                bc.Bridge.connect()
        finally:
            if old is not None:
                os.environ[bc.ENV_ENDPOINT] = old
            else:
                os.environ.pop(bc.ENV_ENDPOINT, None)

    @unittest.skipIf(os.name == "nt", "pipe:// works on Windows")
    def test_pipe_endpoint_off_windows_says_so(self):
        with self.assertRaises(bc.BridgeError) as cm:
            bc.Bridge.connect("pipe://x")
        self.assertIn("Windows", str(cm.exception))


class CommandLineTests(Base):
    def cli(self, g, *args, endpoint=True):
        cmd = [sys.executable, "-I", bc.__file__]
        if endpoint:
            cmd += ["--endpoint", g.endpoint]
        p = subprocess.run(cmd + list(args), capture_output=True, text=True, timeout=30)
        return p.returncode, p.stdout, p.stderr

    def test_ok_and_exit_codes(self):
        g = self.game(vars={"x": 1})
        rc, out, _ = self.cli(g, "ping")
        self.assertEqual(rc, 0)
        self.assertTrue(json.loads(out)["pong"])
        rc, out, _ = self.cli(g, "exec", "say cli")
        self.assertEqual((rc, json.loads(out)["output"]), (0, "cli"))
        rc, out, _ = self.cli(g, "eval", "x")
        self.assertEqual((rc, json.loads(out)), (0, 1))
        rc, _, err = self.cli(g, "exec", "boom")
        self.assertEqual(rc, 2)
        self.assertEqual(json.loads(err)["error"]["code"], -32000)
        rc, _, err = self.cli(g, "waitfor", "x", "--op", ">", "--value", "5", "--timeout-ms", "300")
        self.assertEqual(rc, 3)
        self.assertIn("timed out", err)
        rc, out, _ = self.cli(g, "call", "events", '{"since": 0}')
        self.assertEqual(rc, 0)
        self.assertIn("events", json.loads(out))

    def test_connection_problem_is_exit_1_with_a_message(self):
        p = subprocess.run([sys.executable, "-I", bc.__file__, "--endpoint", f"tcp://127.0.0.1:{free_port()}", "ping"],
                           capture_output=True, text=True, timeout=30)
        self.assertEqual(p.returncode, 1)
        self.assertIn("cannot connect", p.stderr)
        p = subprocess.run([sys.executable, "-I", bc.__file__, "ping"], capture_output=True, text=True, timeout=30,
                           env={k: v for k, v in os.environ.items() if k != bc.ENV_ENDPOINT})
        self.assertEqual(p.returncode, 1)
        self.assertIn("no endpoint", p.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=1)
