"""Tests for the adapter interface and the example FNV adapter (against an in-memory console, never a game)."""
import os
import re
import sys
import tempfile
import unittest

import support
from support import HERE, core, fakecoop, registry
from coopbots.adapter import ACTIONS, QUERIES, ActionAdapter, MissingAction, Result

sys.path.insert(0, os.path.join(HERE, "adapters", "fnv"))
import fnv_adapter  # noqa: E402


class Interface(unittest.TestCase):
    def test_result_is_truthy_only_when_ok(self):
        self.assertTrue(Result(True))
        self.assertFalse(Result(False, "no"))
        self.assertEqual(Result(True, data={"a": 1}).data["a"], 1)

    def test_every_documented_method_is_a_stub_that_says_what_is_missing(self):
        a = ActionAdapter()
        for name in list(ACTIONS) + list(QUERIES) + ["status", "snapshot"]:
            with self.assertRaises(MissingAction, msg=name) as cm:
                getattr(a, name)()
            self.assertIn(name, str(cm.exception))
            self.assertFalse(a.supports(name), name)

    def test_the_module_docs_list_every_method(self):
        doc = __import__("coopbots.adapter", fromlist=["x"]).__doc__
        for name in list(ACTIONS) + list(QUERIES):
            self.assertIn(name, doc, "%s is missing from the adapter module docs" % name)

    def test_every_method_a_playbook_calls_is_in_the_interface(self):
        known = set(ACTIONS) | set(QUERIES) | {"status", "snapshot", "capture", "supports", "name"}
        pdir = os.path.join(HERE, "coopbots", "playbooks")
        called = set()
        for fn in os.listdir(pdir):
            if fn.endswith(".py"):
                with open(os.path.join(pdir, fn), encoding="utf-8") as f:
                    called |= set(re.findall(r"\.bot\.([a-z_]+)\(", f.read()))
        self.assertEqual(called - known, set(), "playbooks call methods that are not part of the adapter interface")

    def test_capture_default_is_an_empty_dict(self):
        self.assertEqual(ActionAdapter().capture("x"), {})

    def test_fake_capture_writes_a_file(self):
        w, _ = registry.REGISTRY["loot_race"].fake(2)
        w.bots[0].goto_ref("world_item", 100)
        d = tempfile.mkdtemp()
        info = w.bots[0].capture(os.path.join(d, "host"))
        self.assertTrue(os.path.isfile(info["actions"]))


class Console:
    """A tiny in-memory game that understands exactly the console lines the example adapter writes."""

    def __init__(self, noop=()):
        self.noop = set(noop)               # verbs that silently do nothing: each one is a bug the adapter must notice
        self.lines = []
        self.dist = {"chest": 40.0, "gold_pile": 60.0}
        self.disabled, self.locked, self.dead = set(), {"gate"}, set()
        self.items = {("player", "stimpak"): 0, ("chest", "stimpak"): 5}
        self.stage = {"quest1": 0}
        self.out = ""
        self.ui = {"menu": "", "messageBoxes": 0, "weaponDrawn": True}

    def exec(self, line):
        self.lines.append(line)
        if self.out:
            return {"ok": True, "output": self.out}
        m = re.match(r"^(\S+)\.(\w+)\s*(.*)$", line)
        if line.startswith("sgtm"):
            self.speed = float(line.split()[1])
            return {"ok": True, "output": ""}
        if line.startswith("setstage"):
            _, q, n = line.split()
            if "setstage" not in self.noop:
                self.stage[q] = int(n)
            return {"ok": True, "output": ""}
        who, verb, rest = m.group(1), m.group(2).lower(), m.group(3).split()
        if verb in self.noop:
            return {"ok": True, "output": ""}
        if verb == "moveto":
            self.dist[rest[0]] = 80.0
        elif verb == "activate":
            self.disabled.add(who)
        elif verb == "removeitemtarget":
            item, dst, n = rest[0], rest[1], int(rest[2])
            self.items[(who, item)] = self.items.get((who, item), 0) - n
            self.items[(dst, item)] = self.items.get((dst, item), 0) + n
        elif verb == "unlock":
            self.locked.discard(who)
        return {"ok": True, "output": ""}

    def eval(self, expr):
        m = re.match(r"^(\S+)\.(\w+)\s*(.*)$", expr)
        if expr.startswith("GetStage "):
            return float(self.stage[expr.split()[1]])
        if expr.startswith("GetStageDone "):
            _, q, n = expr.split()
            return float(self.stage[q] >= int(n))
        who, fn, rest = m.group(1), m.group(2).lower(), m.group(3).split()
        if fn == "getdistance":
            return self.dist.get(rest[0], 9999.0)
        if fn == "getdisabled":
            return float(who in self.disabled)
        if fn == "getdead":
            return float(who in self.dead)
        if fn == "getlocked":
            return float(who in self.locked)
        if fn == "getitemcount":
            return float(self.items.get((who, rest[0]), 0))
        if fn == "getopenstate":
            return 1.0
        raise AssertionError("console fake cannot eval " + expr)

    def call(self, method, params=None):
        if method == "coop.status":
            return {"lastSeq": 7, "host": {"allSynced": False, "badMessages": 2}, "desyncs": {"digests": {"mismatch": 3, "repairs": 1}},
                    "client": {"rebases": 4}, "counters": {"applyFailed": 5}}
        if method == "ui.status":
            return dict(self.ui)
        if method == "state":
            return {"shared": {"stage": dict(self.stage)}, "per_player": {}}
        if method == "events":
            return {"events": [], "next": 0}
        raise AssertionError(method)


class FnvAdapter(unittest.TestCase):
    def bot(self, **kw):
        c = Console(**kw)
        return fnv_adapter.FnvBot(c), c

    def test_actions_write_the_expected_console_lines(self):
        b, c = self.bot()
        self.assertTrue(b.goto_ref("chest", 150))
        self.assertTrue(b.take("stimpak", 2, "chest"))
        self.assertTrue(b.put("stimpak", 1, "chest"))
        self.assertTrue(b.set_stage("quest1", 10))
        self.assertTrue(b.unlock("gate"))
        self.assertEqual(c.lines, ["player.moveto chest", "chest.RemoveItemTarget stimpak player 2 1",
                                   "player.RemoveItemTarget stimpak chest 1 1", "setstage quest1 10", "gate.unlock"])

    def test_queries_read_the_console(self):
        b, c = self.bot()
        b.take("stimpak", 2, "chest")
        self.assertEqual(b.inventory_count("stimpak"), 2)
        self.assertEqual(b.inventory_count("stimpak", "chest"), 3)
        self.assertEqual(b.quest_stage("quest1"), 0)
        b.set_stage("quest1", 10)
        self.assertTrue(b.stage_done("quest1", 10))
        self.assertFalse(b.stage_done("quest1", 20))
        self.assertTrue(b.target_alive("raider"))
        self.assertTrue(b.is_locked("gate"))
        self.assertTrue(b.door_open("gate"))
        self.assertEqual(b.distance_to("chest"), 40.0)

    def test_pickup_race_loser_gets_already_taken(self):
        b, c = self.bot()
        self.assertTrue(b.pickup("gold_pile"))
        r = b.pickup("gold_pile")
        self.assertFalse(r)
        self.assertEqual(r.reason, "already taken")

    def test_console_parse_error_is_a_failed_result_not_an_exception(self):
        b, c = self.bot()
        c.out = "Could not parse the command"
        r = b.activate("gold_pile")
        self.assertFalse(r)
        self.assertIn("gold_pile.activate", r.reason)

    def test_status_maps_the_mod_counters_onto_the_runner_names(self):
        b, _ = self.bot()
        st = b.status()
        self.assertEqual(st["seq"], 7)
        self.assertFalse(st["synced"])
        self.assertEqual(st["counters"], {"mismatches": 3, "repairs": 1, "repaired_effects": 0, "bad_messages": 2,
                                          "apply_failures": 5, "rebases": 4})
        self.assertEqual(core.read_counters(st)["rebases"], 4)
        self.assertEqual(b.snapshot()["shared"]["stage"], {"quest1": 0})

    def test_capture_writes_events(self):
        b, _ = self.bot()
        d = tempfile.mkdtemp()
        info = b.capture(os.path.join(d, "host"))
        self.assertTrue(os.path.isfile(info["events"]))

    def test_unimplemented_actions_are_reported_as_missing(self):
        b, _ = self.bot()
        self.assertFalse(b.supports("fast_travel"))
        self.assertTrue(b.supports("take"))
        w, refs = registry.REGISTRY["fast_travel_tether"].fake(2)
        w.bots[0] = b
        res = support.run_pb(registry.make("fast_travel_tether"), w, refs)
        self.assertEqual(res.status, "skip")
        self.assertIn("fast_travel", res.reason)

    def test_an_open_menu_stops_a_walk_before_any_console_line_and_says_so(self):
        b, c = self.bot()
        c.ui["menu"] = "inventory"
        r = b.goto_ref("chest", 150)
        self.assertEqual((r.ok, r.code, r.data["phase"]), (False, "menu", "pre"))
        self.assertIn("inventory menu is open", r.reason)
        self.assertEqual(c.lines, [])

    def test_a_message_box_and_a_holstered_weapon_are_named_in_failures(self):
        b, c = self.bot()
        c.ui.update(messageBoxes=2, weaponDrawn=False)
        c.disabled.add("gold_pile")
        r = b.pickup("gold_pile")
        self.assertEqual(r.code, "already taken")
        self.assertIn("2 message box(es) are up", r.reason)
        self.assertIn("weapon is holstered", r.reason)

    def test_a_holstered_weapon_does_not_stop_a_walk(self):
        b, c = self.bot()
        c.ui["weaponDrawn"] = False
        self.assertTrue(b.goto_ref("chest", 150))

    def test_a_bridge_without_ui_status_changes_nothing(self):
        b, c = self.bot()
        c.call = lambda method, params=None: (_ for _ in ()).throw(AssertionError(method))
        c.disabled.add("gold_pile")
        self.assertEqual(b.pickup("gold_pile").reason, "already taken")

    def test_set_timescale_is_one_console_line(self):
        b, c = self.bot()
        self.assertTrue(b.supports("set_timescale"))
        self.assertTrue(b.set_timescale(3))
        self.assertEqual(c.lines, ["sgtm 3"])
        self.assertEqual(c.speed, 3.0)
        self.assertFalse(b.supports("timescale"))

    # ---- twins: a console that lies must be noticed by the postconditions ----
    def test_twin_transfer_that_moves_nothing_is_caught(self):
        b, _ = self.bot(noop={"removeitemtarget"})
        r = b.take("stimpak", 2, "chest")
        self.assertFalse(r)
        self.assertIn("moved 0 out and 0 in", r.reason)
        self.assertFalse(b.put("stimpak", 1, "chest"))

    def test_twin_activate_that_does_nothing_is_caught(self):
        b, _ = self.bot(noop={"activate"})
        r = b.pickup("gold_pile")
        self.assertFalse(r)
        self.assertIn("still in the world", r.reason)

    def test_twin_moveto_that_does_not_move_is_caught(self):
        b, c = self.bot(noop={"moveto"})
        r = b.goto_ref("far_thing", 100)
        self.assertFalse(r)
        self.assertEqual(r.code, "stuck")
        self.assertIn("units left", r.reason)
        self.assertIn("far_thing", r.reason)

    def test_twin_unlock_that_does_nothing_is_caught(self):
        b, _ = self.bot(noop={"unlock"})
        r = b.unlock("gate")
        self.assertFalse(r)
        self.assertIn("still locked", r.reason)


if __name__ == "__main__":
    unittest.main()
