"""Tests for the live inspector (inspect_live.py) against fake game copies (tools/bridge/fake_game.py). No game is started.

    python -I tools/inspect/test_inspect.py

COOPINSPECT_DIR names a folder that holds an inspect_live.py to test instead of the real one. run_tests.py uses it to run these
same tests against broken copies (the mutation twins), and every one of them must fail.
"""
import io
import json
import os
import socket
import sys
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
BRIDGE = HERE.parent / "bridge"
os.environ.setdefault("COOPINSPECT_BRIDGE_DIR", str(BRIDGE))
sys.path.insert(0, os.environ.get("COOPINSPECT_DIR") or str(HERE))
sys.path.insert(1, str(BRIDGE))
sys.path.insert(2, str(HERE))
import inspect_live as il  # noqa: E402
import bridge_client as bc  # noqa: E402
from fake_game import FakeError, FakeGame  # noqa: E402

TMP = tempfile.mkdtemp(prefix="coopinspect-test-")

SPEC = {
    "name": "t",
    "eval_allow": ["health"],
    "read_methods": ["sync.status"],
    "fields": [
        {"label": "role", "source": "ping", "path": "game"},
        {"label": "menu", "source": "state", "section": "menu", "path": "menu.kind"},
        {"label": "health", "source": "eval", "expr": "health", "format": "{:g}"},
        {"label": "position", "source": "state", "section": "player", "path": "player.pos", "format": "{0:.0f} {1:.0f} {2:.0f}"},
        {"label": "lastSeq", "source": "call", "method": "sync.status", "path": "lastSeq"},
        {"label": "acks", "source": "call", "method": "sync.status", "path": "peers", "template": "slot {slot}: acked {acked}", "empty": "no clients"},
        {"label": "digest", "source": "call", "method": "sync.status", "path": "digests", "format": "ok {ok} mismatch {mismatch}"},
        {"label": "desync", "source": "events", "types": ["sync.desync"], "last": 2, "template": "{key}: {host} vs {client}"},
        {"label": "boxes", "source": "events", "types": ["ui.messagebox"], "last": 3, "template": "#{seq} {text}"},
    ],
    "problems": [
        {"field": "menu", "op": "!=", "value": "none", "say": "{copy}: the {value} menu is open"},
        {"field": "health", "op": "<", "value": 25, "say": "{copy}: health is low ({value})"},
        {"field": "desync", "op": "nonempty", "say": "{copy}: a desync was reported"},
        {"differs": "lastSeq", "say": "lastSeq differs: {values}"},
    ],
}


def spec(**over):
    d = json.loads(json.dumps(SPEC))
    d.update(over)
    return il.Spec(d)


def make(name="a", health=80.0, seq=10, port=0, **kw):
    g = FakeGame(name=name, vars={"health": health}, port=port, state={
        "menu": {"kind": "none"}, "player": {"pos": [1.0, 2.0, 3.0], "cell": 7}}, **kw)
    status = {"lastSeq": seq, "peers": [{"slot": 1, "acked": seq}], "digests": {"ok": 3, "mismatch": 0}}
    g.custom["sync.status"] = lambda p: status
    g.status = status
    return g.start()


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Base(unittest.TestCase):
    def game(self, **kw):
        g = make(**kw)
        self.addCleanup(g.stop)
        return g

    def inspector(self, games, sp=None, wait=1.0):
        copies = [il.CopyState(g.name, g.endpoint) for g in games]
        insp = il.Inspector(sp or spec(), copies, connect_wait_s=wait)
        self.addCleanup(insp.close)
        return insp


class ReadingTests(Base):
    def test_every_source_is_read_and_formatted(self):
        a = self.game(name="a")
        a.push_event("sync.desync", {"key": "q1", "host": 2, "client": 1})
        a.push_event("ui.messagebox", {"text": "hello"})
        rep = self.inspector([a]).refresh()[0]
        self.assertTrue(rep["reachable"])
        L = rep["lines"]
        self.assertEqual(L["role"], ["a"])
        self.assertEqual(L["menu"], ["none"])
        self.assertEqual(L["health"], ["80"])
        self.assertEqual(L["position"], ["1 2 3"])
        self.assertEqual(L["lastSeq"], ["10"])
        self.assertEqual(L["acks"], ["slot 1: acked 10"])
        self.assertEqual(L["digest"], ["ok 3 mismatch 0"])
        self.assertEqual(L["desync"], ["q1: 2 vs 1"])
        self.assertEqual(L["boxes"], ["#2 hello"])
        self.assertEqual(rep["values"]["health"], 80)

    def test_each_refresh_reads_the_game_again(self):
        a = self.game(name="a")
        insp = self.inspector([a])
        self.assertEqual(insp.refresh()[0]["lines"]["health"], ["80"])
        a.vars["health"] = 12.0
        a.state["menu"]["kind"] = "pipboy"
        a.status["lastSeq"] = 11
        r = insp.refresh()[0]
        self.assertEqual(r["lines"]["health"], ["12"])
        self.assertEqual(r["lines"]["menu"], ["pipboy"])
        self.assertEqual(r["lines"]["lastSeq"], ["11"])

    def test_events_are_read_from_a_cursor_and_never_listed_twice(self):
        a = self.game(name="a")
        insp = self.inspector([a])
        a.push_event("ui.messagebox", {"text": "one"})
        self.assertEqual(insp.refresh()[0]["lines"]["boxes"], ["#1 one"])
        self.assertEqual(insp.refresh()[0]["lines"]["boxes"], ["#1 one"])
        a.push_event("ui.messagebox", {"text": "two"})
        self.assertEqual(insp.refresh()[0]["lines"]["boxes"], ["#1 one", "#2 two"])
        self.assertEqual(len(insp.copies[0].events), 2)

    def test_a_method_the_game_lacks_is_n_a_not_an_error(self):
        a = self.game(name="a", unsupported=["eval"])
        insp = self.inspector([a])
        rep = insp.refresh()[0]
        self.assertEqual(rep["lines"]["health"], ["n/a"])
        self.assertEqual(rep["errors"], {})
        self.assertEqual(insp.problems([rep]), [])

    def test_a_missing_path_is_n_a(self):
        a = self.game(name="a")
        a.state["player"] = {}
        self.assertEqual(self.inspector([a]).refresh()[0]["lines"]["position"], ["n/a"])

    def test_a_failing_method_shows_its_message_and_does_not_hide_other_rows(self):
        a = self.game(name="a")

        def bad(p):
            raise FakeError(-32010, "status module is off")
        a.custom["sync.status"] = bad
        insp = self.inspector([a])
        rep = insp.refresh()[0]
        self.assertEqual(rep["lines"]["lastSeq"], ["error: status module is off"])
        self.assertEqual(rep["lines"]["health"], ["80"])
        self.assertTrue(any("status module is off" in p for p in insp.problems([rep])))

    def test_unreachable_copy_is_a_column_and_the_others_still_show(self):
        a = self.game(name="a")
        copies = [il.CopyState("a", a.endpoint), il.CopyState("b", f"tcp://127.0.0.1:{free_port()}")]
        insp = il.Inspector(spec(), copies, connect_wait_s=0.2)
        self.addCleanup(insp.close)
        reports = insp.refresh()
        self.assertTrue(reports[0]["reachable"])
        self.assertFalse(reports[1]["reachable"])
        text = insp.render(reports)
        self.assertIn("unreachable", text)
        self.assertIn("80", text)
        self.assertTrue(any(p.startswith("b: unreachable") for p in insp.problems(reports)))

    def test_a_copy_that_goes_away_and_comes_back_is_read_again(self):
        port = free_port()
        g = make(name="a", port=port)
        insp = il.Inspector(spec(), [il.CopyState("a", g.endpoint)], connect_wait_s=0.3)
        self.addCleanup(insp.close)
        self.assertTrue(insp.refresh()[0]["reachable"])
        g.stop()
        self.assertFalse(insp.refresh()[0]["reachable"])
        g2 = make(name="a", port=port, health=33.0)
        self.addCleanup(g2.stop)
        rep = insp.refresh()[0]
        self.assertTrue(rep["reachable"])
        self.assertEqual(rep["lines"]["health"], ["33"])


class ReadOnlyTests(Base):
    def test_only_queries_pass_the_wrapper(self):
        a = self.game(name="a")
        b = bc.Bridge.connect(a.endpoint, wait_s=1)
        self.addCleanup(b.close)
        ro = il.ReadOnlyBridge(b, spec().allowed_methods, ["health"])
        for m in ("exec", "save", "load", "screenshot", "waitFor", "quit", "other.method"):
            with self.assertRaises(il.ReadOnlyViolation, msg=m):
                ro.call(m, {"cmd": "set hacked 1", "name": "x"})
        self.assertNotIn("hacked", a.vars)
        self.assertEqual(a.calls, [])
        self.assertEqual(ro.call("ping")["pong"], True)
        self.assertEqual(ro.eval("health"), 80)
        with self.assertRaises(il.ReadOnlyViolation):
            ro.eval("set hacked 1")
        with self.assertRaises(il.ReadOnlyViolation):
            ro.eval("   ")
        self.assertNotIn("hacked", a.vars)

    def test_an_expression_off_the_allow_list_is_refused_and_never_sent(self):
        a = self.game(name="a")
        sp = spec(fields=[{"label": "bad", "source": "eval", "expr": "set hacked 1"}, {"label": "ok", "source": "eval", "expr": "health"}], problems=[])
        rep = self.inspector([a], sp).refresh()[0]
        self.assertTrue(rep["lines"]["bad"][0].startswith("refused:"))
        self.assertEqual(rep["lines"]["ok"], ["80"])
        self.assertNotIn("hacked", a.vars)
        self.assertEqual(a.calls, ["eval"])

    def test_a_whole_refresh_only_calls_read_methods(self):
        a = self.game(name="a")
        b = self.game(name="b")
        insp = self.inspector([a, b])
        insp.refresh()
        insp.refresh()
        for g in (a, b):
            self.assertTrue(set(g.calls) <= {"eval", "state"}, g.calls)
            self.assertEqual(g.saves, {})

    def test_a_field_list_cannot_ask_for_a_write_method(self):
        for m in ("exec", "save", "load", "screenshot", "waitFor", "quit"):
            with self.assertRaises(il.SpecError, msg=m):
                spec(read_methods=[m], fields=[{"label": "x", "source": "call", "method": m}], problems=[])

    def test_a_call_field_needs_its_method_listed(self):
        with self.assertRaises(il.SpecError):
            spec(read_methods=[], fields=[{"label": "x", "source": "call", "method": "sync.status"}], problems=[])


class SpecTests(unittest.TestCase):
    def test_bad_field_lists_are_refused_with_a_reason(self):
        bad = [
            ({"fields": []}, "no fields"),
            ({"fields": [{"source": "ping"}]}, "label"),
            ({"fields": [{"label": "a", "source": "nope"}]}, "source"),
            ({"fields": [{"label": "a", "source": "ping"}, {"label": "a", "source": "ping"}]}, "two fields"),
            ({"fields": [{"label": "a", "source": "eval"}]}, "expr"),
            ({"fields": [{"label": "a", "source": "events"}]}, "types"),
            ({"fields": [{"label": "a", "source": "ping"}], "problems": [{"field": "zz", "op": "==", "say": "x"}]}, "unknown field"),
            ({"fields": [{"label": "a", "source": "ping"}], "problems": [{"field": "a", "op": "~", "say": "x"}]}, "op"),
            ({"fields": [{"label": "a", "source": "ping"}], "problems": [{"field": "a", "op": "==", "value": 1}]}, "say"),
        ]
        for data, word in bad:
            with self.assertRaises(il.SpecError, msg=str(data)) as cm:
                il.Spec(data)
            self.assertIn(word, str(cm.exception))

    def test_the_example_field_lists_load(self):
        il.load_spec(str(HERE / "examples" / "demo_fields.json"))
        fnv = HERE / "examples" / "fnv" / "fnv_fields.json"
        if fnv.exists():
            il.load_spec(str(fnv))
        with self.assertRaises(il.SpecError):
            il.load_spec(os.path.join(TMP, "missing.json"))


class ProblemAndTableTests(Base):
    def test_no_problems_when_all_is_well(self):
        a, b = self.game(name="a"), self.game(name="b")
        insp = self.inspector([a, b])
        text = insp.render(insp.refresh())
        self.assertTrue(text.rstrip().endswith("problems: none"), text)

    def test_each_rule_fires_for_its_own_copy_only(self):
        a, b = self.game(name="a"), self.game(name="b", health=10.0, seq=9)
        b.state["menu"]["kind"] = "message"
        b.push_event("sync.desync", {"key": "k", "host": 1, "client": 2})
        insp = self.inspector([a, b])
        reports = insp.refresh()
        probs = insp.problems(reports)
        self.assertIn("b: the message menu is open", probs)
        self.assertIn("b: health is low (10)", probs)
        self.assertIn("b: a desync was reported", probs)
        self.assertIn("lastSeq differs: a 10, b 9", probs)
        self.assertFalse(any(p.startswith("a:") for p in probs), probs)
        self.assertEqual(len(probs), 4)

    def test_table_columns_line_up_and_long_cells_wrap(self):
        a, b = self.game(name="a"), self.game(name="b")
        b.push_event("ui.messagebox", {"text": "word " * 30})
        insp = il.Inspector(spec(), [il.CopyState(g.name, g.endpoint) for g in (a, b)], connect_wait_s=1.0, col_w=24)
        self.addCleanup(insp.close)
        text = insp.render(insp.refresh())
        table = text.split("\n\nproblems")[0].splitlines()
        bars = [[i for i, c in enumerate(l) if c == "|"] for l in table if "|" in l]
        self.assertTrue(all(x == bars[0] for x in bars), "columns are not aligned:\n" + "\n".join(table))
        self.assertGreater(len(table), 14)                     # the long box text took several lines
        self.assertTrue(all(len(l) <= len(table[1]) for l in table), "a line is wider than the table")
        self.assertTrue(table[1].startswith("-") and "+" in table[1])
        self.assertEqual(table[0].split("|")[0].strip(), "copy")
        self.assertIn(" a ", table[0])

    def test_when_no_copy_answers_the_table_says_so(self):
        insp = il.Inspector(spec(), [il.CopyState("x", f"tcp://127.0.0.1:{free_port()}")], connect_wait_s=0.1)
        self.addCleanup(insp.close)
        text = insp.render(insp.refresh())
        self.assertIn("cannot connect", text)


class CommandLineTests(Base):
    def run_main(self, *args):
        out = io.StringIO()
        old_err = sys.stderr
        sys.stderr = io.StringIO()
        try:
            rc = il.main(list(args), out=out)
            err = sys.stderr.getvalue()
        finally:
            sys.stderr = old_err
        return rc, out.getvalue(), err

    def fields_file(self):
        p = os.path.join(TMP, "fields.json")
        with open(p, "w", encoding="utf-8") as f:
            json.dump(SPEC, f)
        return p

    def test_table_json_and_exit_codes(self):
        a, b = self.game(name="a"), self.game(name="b", seq=7)
        f = self.fields_file()
        rc, out, _ = self.run_main("--fields", f, "--endpoint", "a=" + a.endpoint, "--endpoint", "b=" + b.endpoint)
        self.assertEqual(rc, 0)
        self.assertIn("lastSeq differs", out)
        rc, out, _ = self.run_main("--fields", f, "--endpoint", a.endpoint, "--json")
        self.assertEqual(rc, 0)
        data = json.loads(out)
        self.assertEqual(data["reports"][0]["name"], "0")
        self.assertEqual(data["reports"][0]["values"]["health"], 80)
        rc, _, _ = self.run_main("--fields", f, "--endpoint", f"tcp://127.0.0.1:{free_port()}", "--wait", "0.1")
        self.assertEqual(rc, 2)

    def test_endpoint_file_and_watch(self):
        a = self.game(name="a")
        ep = os.path.join(TMP, "a-endpoint.json")
        with open(ep, "w", encoding="utf-8") as fh:
            json.dump({"endpoints": [a.endpoint]}, fh)
        rc, out, _ = self.run_main("--fields", self.fields_file(), "--endpoint-file", "a=" + ep, "--watch", "0.05", "--count", "3")
        self.assertEqual(rc, 0)
        self.assertEqual(out.count("--- refresh"), 3)
        self.assertEqual(out.count("problems: none"), 3)

    def test_bad_arguments_exit_1(self):
        f = self.fields_file()
        self.assertEqual(self.run_main("--fields", f)[0], 1)
        self.assertEqual(self.run_main("--fields", os.path.join(TMP, "none.json"), "--endpoint", "tcp://127.0.0.1:1")[0], 1)
        rc, _, err = self.run_main("--fields", f, "--endpoint", "a=tcp://h:1", "--endpoint", "a=tcp://h:2")
        self.assertEqual(rc, 1)
        self.assertIn("same name", err)


class DemoTests(unittest.TestCase):
    def test_the_demo_shows_a_table_and_the_problems(self):
        import demo
        out = io.StringIO()
        self.assertEqual(demo.run(refreshes=3, interval=0.05, out=out), 0)
        text = out.getvalue()
        self.assertEqual(text.count("--- refresh"), 3)
        self.assertIn("client2: unreachable", text)
        self.assertIn("client: the message menu is open", text)
        self.assertIn("lastSeq differs between copies", text)
        first = text.split("--- refresh 2")[0]
        self.assertNotIn("menu is open", first)


if __name__ == "__main__":
    unittest.main(verbosity=1)
