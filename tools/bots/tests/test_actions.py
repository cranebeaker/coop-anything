"""The action result model: ActionResult, pre and postconditions, and "say why" failure reasons (fake worlds only)."""
import json
import os
import tempfile
import unittest

import support
from support import registry, run_pb
from coopbots import fakecoop
from coopbots.adapter import ActionAdapter, ActionFailed, ActionResult, Fail, Result, Why, ensure


class Bank(ActionAdapter):
    """A tiny adapter whose take() has a precondition and a postcondition. `lie` makes the game do nothing."""

    def __init__(self, lie=False, strict=False, blocked=()):
        self.chest, self.me, self.lie, self.strict, self.blocked = 5, 0, lie, strict, list(blocked)
        self.did = []

    def blockers(self):
        return list(self.blocked)

    def take(self, item, count, from_ref):
        def have_enough():
            ensure(self.chest >= count, "precondition", "only %d left in %s" % (self.chest, from_ref), left=self.chest)

        def do():
            self.did.append("take")
            if not self.lie:
                self.chest -= count
                self.me += count
            return {"moved": count}

        def moved(data):
            ensure(self.me == count, "postcondition", "the inventory did not change: %d x %s held" % (self.me, item))
        return self.act(do, pre=[have_enough], post=[moved])

    def drop(self, item, count=1):
        raise Why.holstered("nothing")            # a Fail raised in the body of an action

    def pickup(self, ref):
        return True                               # a bare bool is accepted

    def talk(self, npc):
        return None                               # a bare None is a bug of the adapter, not a pass

    def use_device(self, ref, entry):
        return Result(False, "terminal is locked")


class ResultModel(unittest.TestCase):
    def test_action_result_is_a_result_with_action_and_time(self):
        r = ActionResult(True, "", {"a": 1}, "take", 0.5)
        self.assertIsInstance(r, Result)
        self.assertTrue(r)
        self.assertEqual((r.ok, r.reason, r.data, r.action, r.elapsed_s), (True, "", {"a": 1}, "take", 0.5))
        self.assertFalse(ActionResult(False, "stuck: 5 units left"))
        self.assertIn("FAILED", str(ActionResult(False, "no", {}, "take")))

    def test_code_is_the_part_before_the_colon(self):
        self.assertEqual(Result(False, "stuck: 312 units left (wanted within 150)").code, "stuck")
        self.assertEqual(Result(False, "already taken").code, "already taken")
        self.assertEqual(ActionResult(False, "menu: the map menu is open").to_json()["code"], "menu")
        self.assertEqual(ActionResult(True).to_json()["code"], "")

    def test_every_fake_action_returns_an_action_result(self):
        w, refs = registry.REGISTRY["fight"].fake(2)
        a = w.bots[0]
        for r in (a.goto_ref("raider_1", 300), a.equip("WEAPON"), a.aim_at("raider_1"), a.fire(1), a.set_timescale(2),
                  a.open_door("raider_1")):
            self.assertIsInstance(r, ActionResult)
            self.assertTrue(r.action)
            self.assertGreaterEqual(r.elapsed_s, 0.0)
        self.assertEqual(a.goto_ref("raider_1", 300).action, "goto_ref")

    def test_plain_results_of_any_adapter_are_wrapped(self):
        b = Bank()
        r = b.use_device("t", 1)
        self.assertIsInstance(r, ActionResult)
        self.assertEqual((r.ok, r.reason, r.action), (False, "terminal is locked", "use_device"))
        self.assertTrue(b.pickup("x"))
        self.assertIsInstance(b.pickup("x"), ActionResult)

    def test_a_bare_none_is_rejected_as_a_bug_of_the_adapter(self):
        with self.assertRaises(TypeError):
            Bank().talk("npc")

    def test_a_fail_raised_inside_an_action_becomes_a_failed_result(self):
        r = Bank().drop("x")
        self.assertFalse(r)
        self.assertEqual(r.code, "holstered")
        self.assertIn("holstered", r.reason)

    def test_strict_mode_raises_with_the_result(self):
        with self.assertRaises(ActionFailed) as cm:
            Bank(lie=True, strict=True).take("gold", 2, "chest")
        self.assertEqual(cm.exception.result.code, "postcondition")
        self.assertTrue(Bank(strict=True).take("gold", 2, "chest"))


class PreAndPostconditions(unittest.TestCase):
    def test_a_good_action_passes_both_and_returns_its_data(self):
        r = Bank().take("gold", 2, "chest")
        self.assertTrue(r)
        self.assertEqual(r.data, {"moved": 2})

    def test_a_failed_precondition_stops_before_the_action_runs(self):
        b = Bank()
        r = b.take("gold", 9, "chest")
        self.assertFalse(r)
        self.assertEqual((r.code, r.data["phase"], r.data["left"]), ("precondition", "pre", 5))
        self.assertIn("only 5 left in chest", r.reason)
        self.assertEqual(b.did, [])

    def test_twin_an_action_that_lies_is_caught_by_its_postcondition(self):
        b = Bank(lie=True)
        r = b.take("gold", 2, "chest")
        self.assertFalse(r)
        self.assertEqual((r.code, r.data["phase"]), ("postcondition", "post"))
        self.assertEqual(b.did, ["take"])            # it ran; the world check is what failed it

    def test_a_failed_result_from_the_action_is_passed_on_without_the_postcondition(self):
        class Locked(ActionAdapter):
            ran_post = False

            def unlock(self, ref, key=None):
                def post(data):
                    Locked.ran_post = True
                return self.act(lambda: Result(False, "need a key", {"key": "k1"}), post=[post])
        r = Locked().unlock("door")
        self.assertEqual((r.ok, r.reason, r.data), (False, "need a key", {"key": "k1"}))
        self.assertFalse(Locked.ran_post)


class SayWhy(unittest.TestCase):
    def test_blockers_are_added_to_every_failure(self):
        b = Bank(lie=True, blocked=[("menu", "the inventory menu is open")])
        r = b.take("gold", 2, "chest")
        self.assertEqual(r.code, "postcondition")
        self.assertIn("the inventory menu is open", r.reason)
        r = b.use_device("t", 1)
        self.assertIn("terminal is locked", r.reason)
        self.assertIn("the inventory menu is open", r.reason)

    def test_twin_without_the_hook_the_failure_does_not_say_why(self):
        # the engine twin COOPBOTS_MUTANT=nowhy removes the added text: this test must then fail
        r = Bank(blocked=[("message_box", "a message box is up")]).use_device("t", 1)
        self.assertIn("a message box is up", r.reason)

    def test_free_refuses_to_start_while_something_blocks(self):
        class Walker(ActionAdapter):
            def __init__(self):
                self.box = True

            def blockers(self):
                return [("message_box", "a message box is up")] if self.box else []

            def goto_ref(self, ref, dist=150):
                return self.act(lambda: {"arrived": True}, free=True)
        w = Walker()
        r = w.goto_ref("x")
        self.assertEqual((r.ok, r.code, r.data["phase"]), (False, "message_box", "pre"))
        w.box = False
        self.assertTrue(w.goto_ref("x"))

    def test_the_cause_is_not_written_twice(self):
        class Walker(ActionAdapter):
            def blockers(self):
                return [("menu", "the map menu is open")]

            def goto_ref(self, ref, dist=150):
                return self.act(lambda: {}, free=True)
        r = Walker().goto_ref("x")
        self.assertEqual(r.reason.count("the map menu is open"), 1, r.reason)

    def test_a_broken_explain_hook_does_not_hide_the_failure(self):
        class Odd(Bank):
            def explain(self, action, result):
                raise RuntimeError("boom")
        r = Odd().use_device("t", 1)
        self.assertEqual(r.reason, "terminal is locked")

    def test_the_helpers_word_the_shared_failures(self):
        self.assertEqual(Why.stuck(312, 150).reason, "stuck: 312 units left (wanted within 150)")
        self.assertEqual(Why.stuck(312, 150, "menu open").data["distance"], 312)
        self.assertEqual(Why.too_close(40, 100).code, "too_close")
        self.assertEqual(Why.too_steep(70, 60, 300).code, "too_steep")
        self.assertIn("inventory", Why.menu_open("the inventory menu").reason)
        self.assertEqual(Why.message_box("Save?").data["text"], "Save?")
        self.assertEqual(Why.holstered("rifle").code, "holstered")
        self.assertEqual(Why.postcondition("x").code, "postcondition")
        self.assertEqual(Why.precondition("x").code, "precondition")
        with self.assertRaises(Fail):
            ensure(False, "precondition", "no")
        ensure(True, "precondition", "no")


class FakeWorldReasons(unittest.TestCase):
    """The fake world can be put in each situation; the reason must name it."""

    def world(self):
        w, refs = registry.REGISTRY["fight"].fake(2)
        return w, w.bots[0], refs

    def test_stuck_walk_says_how_far_is_left(self):
        w, a, _ = self.world()
        w.walk_limit = 100.0
        r = a.goto_ref("raider_1", 150)
        self.assertEqual(r.code, "stuck")
        self.assertRegex(r.reason, r"stuck: \d+ units left \(wanted within 150\)")
        self.assertGreater(r.data["distance"], 3000)

    def test_menu_open_blocks_a_walk(self):
        w, a, _ = self.world()
        w.players[0]["menu"] = "pip-boy"
        r = a.goto_ref("raider_1", 150)
        self.assertEqual(r.code, "menu")
        self.assertIn("pip-boy menu is open", r.reason)

    def test_message_box_blocks_aiming(self):
        w, a, _ = self.world()
        w.players[0]["box"] = "Level up!"
        r = a.aim_at("raider_1")
        self.assertEqual(r.code, "message_box")
        self.assertIn("Level up!", r.reason)

    def test_holstered_weapon_fails_a_shot_and_says_so(self):
        w, a, _ = self.world()
        a.equip("WEAPON")
        a.aim_at("raider_1")
        w.players[0]["drawn"] = False
        r = a.fire(1)
        self.assertEqual(r.code, "holstered")
        self.assertIn("WEAPON is holstered", r.reason)
        w.players[0]["drawn"] = True
        self.assertTrue(a.fire(1))

    def test_target_too_close_and_too_steep(self):
        w, a, _ = self.world()
        a.goto_ref("raider_1", 100)
        w.aim_min_dist = 5000
        r = a.aim_at("raider_1")
        self.assertEqual(r.code, "too_close")
        self.assertRegex(r.reason, r"\d+ units from the target \(minimum 5000\)")
        w.aim_min_dist = None
        w.max_pitch = 10.0
        w.t("raider_1")["pos"][2] = 3000.0
        for v in w.views:
            v["refs"]["raider_1"]["pos"][2] = 3000.0
        r = a.aim_at("raider_1")
        self.assertEqual(r.code, "too_steep")
        self.assertIn("degrees of pitch (limit 10)", r.reason)

    def test_a_failed_step_names_the_cause_and_the_evidence_keeps_it(self):
        w, refs = registry.REGISTRY["fight"].fake(2)
        w.walk_limit = 100.0
        d = tempfile.mkdtemp(prefix="coopbots-")
        res = run_pb(registry.make("fight"), w, refs, outdir=d)
        self.assertEqual(res.status, "fail")
        self.assertRegex(res.reason, r"walks to raider_1: stuck: \d+ units left \(wanted within 600\)")
        fail_dir = res.steps[-1].fail_dir
        with open(os.path.join(fail_dir, "failure.json"), encoding="utf-8") as f:
            man = json.load(f)
        recent = man["machines"][0]["recent_actions"]
        self.assertEqual(recent[-1]["code"], "stuck")
        self.assertEqual(recent[-1]["action"], "goto_ref")
        self.assertFalse(recent[-1]["ok"])

    def test_recent_actions_keep_only_the_tail(self):
        w, a, _ = self.world()
        for _ in range(20):
            a.cell()
            a.goto_pos(1, 2, 3)
        self.assertEqual(len(a.recent_actions(5)), 5)
        self.assertTrue(all(x["ok"] for x in a.recent_actions(5)))


class SubclassingStillWorks(unittest.TestCase):
    def test_a_subclass_that_blanks_an_action_keeps_it_missing(self):
        class NoSpawn(fakecoop.FakeAdapter):
            spawn = ActionAdapter.spawn
        w, _ = registry.REGISTRY["spawn_once"].fake(2)
        a = NoSpawn(w, 0)
        self.assertFalse(a.supports("spawn"))
        self.assertTrue(a.supports("goto_ref"))


if __name__ == "__main__":
    unittest.main()
