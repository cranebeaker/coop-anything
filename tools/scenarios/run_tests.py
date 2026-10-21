#!/usr/bin/env python3
"""Game-free tests for the scenario tool, plus the mutation twins.

    python -I tools/scenarios/run_tests.py        (prints ALL OK only when everything passes and every twin is caught)

Part 1: unit tests (validator, resolution, comparison, jump planning, runner against fakegame.py, the example FNV adapter
        against a recording bridge, the command line).
Part 2: twins. The same tests are run again under each SCEN_MUTANT value; every mutant must make at least one test fail.
No game is ever started.
"""
import contextlib
import copy
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / "examples" / "fnv"))
import scenlib  # noqa: E402
import runner  # noqa: E402
import fakegame  # noqa: E402
import fnv_game  # noqa: E402
import prepare_save  # noqa: E402
from gameadapter import GameAdapter, ScenarioError  # noqa: E402

EX = HERE / "examples" / "fnv"
EXAMPLES = ["fight", "loot_race", "quest_stage_sync", "ownership_handoff"]


def load(name):
    return scenlib.load_scenario(EX / (name + ".json"))


def run_all(names, lacks=(), scenarios=None, **flags):
    """Run scenarios in order on one fake 2-player world. Returns (world, results, runner)."""
    w, clock, games = fakegame.make(2, lacks=lacks, **flags)
    rn = runner.Runner(games, runner.StateStore(), sleep=clock.sleep, now=clock.now)
    scs = scenarios or [load(n) for n in names]
    for s in scs:
        w.load_scenario(s)
    return w, [rn.run(s) for s in scs], rn


def inline(**over):
    """A tiny valid scenario for tests that need their own steps."""
    sc = {"schema": 1, "name": "t", "title": "t", "description": "d", "players": 2, "start": {"save": "s"},
          "objects": {"npc": {"id": "NPC1", "kind": "actor"}, "q": {"id": "Q1", "kind": "state"},
                      "pad": {"id": "PAD1", "kind": "place"}, "gun": {"id": "GUN1", "kind": "item"},
                      "box": {"id": "BOX1", "kind": "thing"}, "mob": {"id": "MOB1", "kind": "base"}},
          "steps": [{"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}]}
    sc.update(over)
    return sc


POPUP_OFFSETS = [2, 3, 4, 5, 18, 19, 32, 33, 34, 35, 48, 49, 50, 51]   # 14 popups in waves over about 50 s after a load


def hooked(n=2, sim=None, popups=False, index=None, **flags):
    """A fake world with the optional hooks switched on: sync readings (`sim`), load popups, a data index."""
    w, clock, games = fakegame.make(n, **flags)
    w.sim = sim
    w.popup_offsets = list(POPUP_OFFSETS) if popups else None
    w.index = index
    return w, clock, games


def runner_on(games, clock, **kw):
    return runner.Runner(games, runner.StateStore(), sleep=clock.sleep, now=clock.now, **kw)


def full_index(sc):
    """A data index that knows every object of the scenario under its own kind."""
    return {o["id"]: o["kind"] for o in sc["objects"].values()}


class Hygiene(unittest.TestCase):
    def test_no_em_dashes_or_personal_paths_in_this_folder(self):
        for p in HERE.rglob("*"):
            if p.suffix in (".py", ".md", ".json") and "__pycache__" not in p.parts:
                text = p.read_text(encoding="utf-8")
                self.assertNotIn(chr(0x2014), text, p.name)
                self.assertNotIn("\\" + "Us" + "ers\\", text, p.name)
                self.assertNotIn("/" + "Us" + "ers/", text, p.name)


class Shipped(unittest.TestCase):
    def test_examples_exist_and_are_valid(self):
        names = {p.stem for p in scenlib.list_scenarios()}
        self.assertTrue(set(EXAMPLES) <= names, names)
        for p in scenlib.list_scenarios():
            self.assertEqual(scenlib.validate_file(p), [], p.name)

    def test_examples_have_no_numeric_form_ids_or_personal_data(self):
        for p in scenlib.list_scenarios():
            text = p.read_text(encoding="utf-8")
            self.assertNotRegex(text, r"\b[0-9A-F]{8}\b", p.name)
            self.assertNotIn(chr(0x2014), text)
            self.assertNotIn("Users", text)

    def test_playbook_refs_resolve_aliases(self):
        refs = scenlib.playbook_refs(load("fight"))
        self.assertEqual(refs["raiders"], ["GSPG01Ref", "GSPG02Ref", "GSPG03Ref"])
        self.assertEqual(scenlib.playbook_refs(load("quest_stage_sync"))["stages"], [10, 20, 100])


class ValidatorRejects(unittest.TestCase):
    def errs(self, sc):
        return scenlib.validate(sc, name=sc.get("name"))

    def expect(self, mut, text, base=None):
        sc = copy.deepcopy(base or load("quest_stage_sync"))
        mut(sc)
        errs = self.errs(sc)
        self.assertTrue(any(text in e for e in errs), "wanted %r in %s" % (text, errs))

    def test_base_is_clean(self):
        self.assertEqual(self.errs(load("quest_stage_sync")), [])
        self.assertEqual(self.errs(inline(name="t")), [])

    def test_unknown_alias_in_step(self):
        self.expect(lambda s: s["steps"][1].update(key="$nothere"), "unknown alias")

    def test_unknown_alias_in_reset_dirties_and_refs(self):
        self.expect(lambda s: s["reset"].update(keys=["$nothere"]), "unknown alias")
        self.expect(lambda s: s["steps"][1].update(dirties=["$nothere"]), "unknown alias")
        self.expect(lambda s: s.update(refs={"x": "$nothere"}), "unknown alias")

    def test_unknown_alias_inside_nested_refs(self):
        self.expect(lambda s: s.update(refs={"spec": {"ref": "$nothere", "dist": 3}, "many": ["$quest", "$nothere"]}), "unknown alias")

    def test_wrong_object_kind_for_the_field(self):
        def m(s):
            s["objects"]["quest"]["kind"] = "item"
        self.expect(m, "expected one of")

    def test_player_only_where_allowed(self):
        self.expect(lambda s: s["steps"][1].update(key="player"), "'player' is not allowed")

    def test_bad_alias_object_and_kind(self):
        self.expect(lambda s: s["objects"].update({"1bad": {"id": "x", "kind": "state"}}), "bad alias")
        self.expect(lambda s: s["objects"]["quest"].pop("id"), "non-empty string id")
        self.expect(lambda s: s["objects"]["quest"].update(kind="widget"), "kind must be")

    def test_unknown_op_and_key(self):
        self.expect(lambda s: s["steps"][1].update(op="teleport"), "unknown op")
        self.expect(lambda s: s["steps"][1].update(color="red"), "unexpected key")

    def test_missing_required_field_and_wrong_type(self):
        self.expect(lambda s: s["steps"][1].pop("key"), "needs 'key'")
        sc = inline(steps=[{"id": "g", "op": "give", "who": "player", "item": "$gun", "count": 0},
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        self.assertTrue(any("wrong type" in e for e in self.errs(sc)))

    def test_duplicate_and_odd_ids(self):
        self.expect(lambda s: s["steps"][1].update(id=s["steps"][0]["id"]), "duplicate id")
        self.expect(lambda s: s["steps"][1].update(id="Bad Id"), "lower_snake_case")

    def test_no_checks_and_no_steps(self):
        self.expect(lambda s: s.update(steps=[{"id": "w", "op": "wait", "seconds": 1}]), "no check")
        self.expect(lambda s: s.update(steps=[]), "steps is empty")

    def test_bad_on_players_and_parallel(self):
        self.expect(lambda s: s["steps"][1].update(on=5), "bad 'on'")
        self.expect(lambda s: s["steps"][1].update(parallel="yes"), "parallel")

    def test_start_schema_name_players(self):
        self.expect(lambda s: s["start"].pop("save"), "start.save")
        self.expect(lambda s: s.update(schema=2), "schema")
        self.expect(lambda s: s.update(name="Other Name"), "lower_snake_case")
        self.expect(lambda s: s.update(players=9), "players")
        sc = load("quest_stage_sync")
        sc["name"] = "other"
        self.assertTrue(any("does not match file name" in e for e in scenlib.validate(sc, name="quest_stage_sync")))

    def test_check_rules(self):
        self.expect(lambda s: s["steps"][0].pop("value"), "needs a 'value'")
        self.expect(lambda s: s["steps"][0].update(cmp="~"), "cmp")
        self.expect(lambda s: s["steps"][0].update(agg="avg"), "agg")
        self.expect(lambda s: s["steps"][0].update(kind="mood"), "unknown check kind")
        self.expect(lambda s: s["steps"][0].update(timeout=500), "timeout")
        self.expect(lambda s: s["steps"][0].update(value=True), "must be a number")
        self.expect(lambda s: s["steps"][0].update(agg="sum", cmp="same"), "cannot be combined")

    def test_action_args_aliases_are_checked(self):
        sc = inline(steps=[{"id": "a", "op": "action", "name": "fast_travel", "args": {"marker": "$nope"}},
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        self.assertTrue(any("unknown alias" in e for e in self.errs(sc)))

    def test_hooks_add_game_specific_errors(self):
        sc = load("fight")
        hook = lambda s: ["id %s is not in the game data" % o["id"] for o in s["objects"].values() if o["id"].endswith("03Ref")]  # noqa: E731
        errs = scenlib.validate(sc, name="fight", hooks=[hook])
        self.assertEqual(errs, ["id GSPG03Ref is not in the game data"])

    def test_every_shipped_scenario_rejects_a_broken_alias(self):
        for p in scenlib.list_scenarios():
            sc = scenlib.load_scenario(p)
            first = next(s for s in sc["steps"] if s["op"] == "check")
            field = next(k for k in first if k in scenlib.OBJECT_FIELDS)
            first[field] = "$missing_object"
            self.assertTrue(any("unknown alias" in e for e in scenlib.validate(sc, name=sc["name"])), p.name)


class Resolution(unittest.TestCase):
    def test_resolve_args(self):
        res = scenlib.Resolver(load("fight"))
        st = {"id": "x", "op": "move", "who": "player", "to": "$raider1"}
        self.assertEqual(scenlib.resolve_args(st, res), {"who": "player", "to": "GSPG01Ref"})
        sc = inline()
        res = scenlib.Resolver(sc)
        st = {"id": "x", "op": "action", "name": "go", "args": {"marker": "$pad", "n": 3}}
        self.assertEqual(scenlib.resolve_args(st, res), {"name": "go", "args": {"marker": "PAD1", "n": 3}})
        st = {"id": "x", "op": "check", "kind": "state_reached", "key": "$q", "at": 10, "value": 1}
        self.assertEqual(scenlib.resolve_args(st, res), {"key": "Q1", "at": 10})

    def test_needs_of(self):
        self.assertEqual(scenlib.needs_of({"op": "kill"}), {"op:kill"})
        self.assertEqual(scenlib.needs_of({"op": "check", "kind": "dead"}), {"check:dead"})
        self.assertEqual(scenlib.needs_of({"op": "action", "name": "say", "needs": ["x"]}), {"action:say", "x"})

    def test_targets(self):
        sc = {"players": 3}
        self.assertEqual(scenlib.targets(sc, "host"), [0])
        self.assertEqual(scenlib.targets(sc, "guest"), [1, 2])
        self.assertEqual(scenlib.targets(sc, "all"), [0, 1, 2])
        self.assertEqual(scenlib.targets(sc, 2), [2])
        with self.assertRaises(ValueError):
            scenlib.targets(sc, 3)

    def test_compare_table(self):
        c = scenlib.compare
        self.assertTrue(c([1, 1], "==", 1, None, 0))
        self.assertFalse(c([1, 0], "==", 1, None, 0))
        self.assertTrue(c([1, 0], "==", 1, "sum", 0))
        self.assertFalse(c([1, 1], "==", 1, "sum", 0))      # duplicated loot
        self.assertFalse(c([0, 0], "==", 1, "sum", 0))      # vanished loot
        self.assertTrue(c([3, 3.5], "same", None, None, 1))
        self.assertFalse(c([3, 9], "same", None, None, 1))
        self.assertTrue(c([5, 6], "<=", 6, "max", 0))
        self.assertTrue(c([50], "<", 100, None, 0))
        self.assertTrue(c([99.5], "==", 100, None, 1))
        self.assertTrue(c([2, 3], "!=", 9, None, 0))


class JumpPlanning(unittest.TestCase):
    def test_rules(self):
        fight, quest = load("fight"), load("quest_stage_sync")
        st = {"loaded": True, "save": "autosave", "dirty": [], "failed": False}
        self.assertFalse(scenlib.plan_jump(st, quest)["reload"])
        self.assertTrue(scenlib.plan_jump({"loaded": False}, quest)["reload"])
        self.assertTrue(scenlib.plan_jump(dict(st, save="other"), quest)["reload"])
        self.assertTrue(scenlib.plan_jump(dict(st, failed=True), quest)["reload"])
        self.assertTrue(scenlib.plan_jump(dict(st, dirty=["*"]), quest)["reload"])
        d = sorted(scenlib.footprint(fight)["dirty"])
        self.assertTrue(scenlib.plan_jump(dict(st, dirty=d), fight)["reload"])           # same raiders again
        self.assertFalse(scenlib.plan_jump(dict(st, dirty=d), quest)["reload"])          # unrelated things
        self.assertTrue(scenlib.plan_jump(st, quest, force_reload=True)["reload"])
        sc = dict(quest, start={"save": "autosave", "reload": True})
        self.assertTrue(scenlib.plan_jump(st, sc)["reload"])

    def test_load_and_unnamed_state_changes_are_unnameable(self):
        sc = inline(steps=[{"id": "l", "op": "load", "name": "x"}, {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        self.assertIn("*", scenlib.footprint(sc)["dirty"])
        sc = inline(steps=[{"id": "s", "op": "set_state", "key": "$q", "value": 1},
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        self.assertIn("*", scenlib.footprint(sc)["dirty"])
        sc["steps"][0]["dirties"] = []
        self.assertNotIn("*", scenlib.footprint(sc)["dirty"])
        sc["steps"][0]["dirties"] = ["$box"]
        self.assertEqual(scenlib.footprint(sc)["dirty"], {"BOX1"})

    def test_player_inventory_is_tracked(self):
        fp = scenlib.footprint(load("loot_race"))
        self.assertIn("inv:VMS13TeddyBear", fp["dirty"])
        self.assertIn("VMS13TeddyBearRef", fp["dirty"])
        sc = inline(steps=[{"id": "g", "op": "give", "who": "player", "item": "$gun", "count": 2},
                           {"id": "c1", "op": "check", "kind": "item_count", "who": "player", "item": "$gun", "value": 2}])
        self.assertEqual(scenlib.footprint(sc)["dirty"], {"inv:GUN1"})

    def test_reset_ids(self):
        self.assertEqual(scenlib.reset_ids(load("quest_stage_sync")), ["VCG02"])
        self.assertEqual(scenlib.reset_ids(load("fight")), [])


class RunnerOnFake(unittest.TestCase):
    def test_synced_world_passes_every_example(self):
        _, results, _ = run_all(EXAMPLES)
        for r in results:
            self.assertEqual(r["status"], "pass", (r["scenario"], [s for s in r["steps"] if s["status"] != "pass"]))

    def test_each_example_passes_on_a_fresh_world(self):
        for n in EXAMPLES:
            _, results, _ = run_all([n])
            self.assertEqual(results[0]["status"], "pass", n)

    def test_missing_capabilities_are_blocked_not_passed(self):
        sc = inline(steps=[{"id": "leave", "op": "leave", "on": "guest"}, {"id": "join", "op": "join", "on": "guest"},
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        _, results, _ = run_all([], lacks={"op:leave", "op:join"}, scenarios=[sc])
        r = results[0]
        self.assertEqual(r["status"], "partial")
        self.assertEqual([s["status"] for s in r["steps"]], ["blocked", "blocked", "blocked"])
        self.assertIn("op:leave", r["steps"][0]["detail"])
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "pass")

    def test_guest_blind_world_fails_the_sync_checks(self):
        _, results, _ = run_all(["quest_stage_sync", "fight"], desync_guest=True)
        for r in results:
            self.assertEqual(r["status"], "fail", r["scenario"])

    def test_loot_duplication_is_caught(self):
        _, results, _ = run_all(["loot_race"], dup_loot=True)
        r = results[0]
        self.assertEqual(r["status"], "fail")
        self.assertTrue(any(s["id"] == "exactly_one_copy" and s["status"] == "fail" for s in r["steps"]))

    def test_double_spawn_is_caught(self):
        sc = inline(steps=[{"id": "sp", "op": "spawn", "base": "$mob", "at": "$box"},
                           {"id": "one", "op": "check", "kind": "count", "base": "$mob", "value": 1}])
        _, results, _ = run_all([], scenarios=[sc], double_spawn=True)
        self.assertEqual(results[0]["status"], "fail")
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "pass")

    def test_missing_ownership_handoff_is_caught(self):
        _, results, _ = run_all(["ownership_handoff"], no_handoff=True)
        self.assertEqual(results[0]["status"], "fail")

    def test_steps_run_on_the_named_player_only(self):
        w, _, _ = run_all(["fight"])
        kills = [(i, t) for i, k, t in w.calls if k == "do" and t.startswith("kill")]
        self.assertTrue(any(i == 1 and "GSPG02Ref" in t for i, t in kills))
        self.assertFalse(any(i == 0 and "GSPG02Ref" in t for i, t in kills))
        self.assertTrue(any(i == 0 and "GSPG01Ref" in t for i, t in kills))
        self.assertFalse(any(i == 1 and "GSPG01Ref" in t for i, t in kills))

    def test_parallel_step_runs_on_every_player(self):
        w, _, _ = run_all(["loot_race"])
        acts = [i for i, k, t in w.calls if k == "do" and t.startswith("activate")]
        self.assertEqual(sorted(acts), [0, 1])

    def test_unmet_check_times_out_with_a_detail(self):
        sc = inline(steps=[{"id": "never", "op": "check", "kind": "state", "key": "$q", "value": 55, "timeout": 2}])
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "fail")
        self.assertIn("not met after", results[0]["steps"][-1]["detail"])

    def test_unknown_action_is_blocked_and_a_failing_one_is_an_error(self):
        sc = inline(steps=[{"id": "bad", "op": "action", "name": "no_such_action"},
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "partial")        # the adapter never said it can do this
        sc = inline(steps=[{"id": "bad", "op": "action", "name": "say", "args": {}},   # missing argument: the adapter blows up
                           {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "error")
        self.assertIn("KeyError", results[0]["steps"][0]["detail"])

    def test_invalid_scenario_is_not_run(self):
        sc = inline(steps=[{"id": "c1", "op": "check", "kind": "dead", "ref": "$ghost", "value": 0}])
        w, _, games = fakegame.make(2)
        r = runner.Runner(games, runner.StateStore()).run(sc)
        self.assertEqual(r["status"], "invalid")
        self.assertEqual(w.calls, [])

    def test_save_and_load_steps_round_trip_state(self):
        sc = inline(steps=[{"id": "s1", "op": "set_state", "key": "$q", "value": 10, "dirties": []},
                           {"id": "sv", "op": "save", "name": "mark"},
                           {"id": "s2", "op": "set_state", "key": "$q", "value": 20, "dirties": []},
                           {"id": "ld", "op": "load", "name": "mark"},
                           {"id": "back", "op": "check", "kind": "state", "key": "$q", "value": 10}])
        _, results, _ = run_all([], scenarios=[sc])
        self.assertEqual(results[0]["status"], "pass", results[0]["steps"])


class InstantJump(unittest.TestCase):
    def test_second_unrelated_scenario_jumps_in_place(self):
        w, results, _ = run_all(["quest_stage_sync", "fight", "ownership_handoff"])
        # fight and ownership_handoff share raiders, so only the first two are unrelated to each other
        self.assertEqual([r["jump"]["reload"] for r in results[:2]], [True, False])
        self.assertEqual(len([x for x in w.loads if x[1] == "autosave"]), 4)   # first jump, then ownership_handoff: 2 players each
        self.assertTrue(all(r["status"] == "pass" for r in results))

    def test_conflicting_scenario_reloads(self):
        _, results, _ = run_all(["fight", "ownership_handoff"])
        self.assertTrue(results[1]["jump"]["reload"])
        self.assertTrue(any("changed things" in x for x in results[1]["jump"]["reasons"]))

    def test_repeating_a_scenario_that_killed_actors_reloads(self):
        _, results, _ = run_all(["fight", "fight"])
        self.assertTrue(results[1]["jump"]["reload"])
        self.assertEqual(results[1]["status"], "pass")     # and it passes because the world was restored

    def test_failed_scenario_forces_a_reload_next(self):
        _, results, _ = run_all(["quest_stage_sync", "quest_stage_sync"], desync_guest=True)
        self.assertEqual(results[0]["status"], "fail")
        self.assertTrue(results[1]["jump"]["reload"])

    def test_in_place_jump_resets_state_keys(self):
        w, _, _ = run_all(["quest_stage_sync", "fight"])
        self.assertEqual(w.state.get("VCG02", 0), 0)       # reset by the in-place jump

    def test_in_place_jump_is_cheap_in_adapter_calls(self):
        w, clock, games = fakegame.make(2)
        rn = runner.Runner(games, runner.StateStore(), sleep=clock.sleep, now=clock.now)
        rn.run(load("quest_stage_sync"))
        n0 = len(w.calls)
        j = rn.jump(load("fight"))
        self.assertFalse(j["reload"])
        self.assertLess(len(w.calls) - n0, 20)
        self.assertLess(j["seconds"], 10)

    def test_state_survives_in_a_store_file(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "s.json"
            s = runner.StateStore(p)
            s.put("0@1", {"loaded": True, "save": "autosave", "dirty": [], "failed": False})
            self.assertEqual(runner.StateStore(p).get("0@1")["save"], "autosave")
            self.assertIsNone(runner.StateStore(p).get("0@2"))      # new generation = nothing loaded


class RecordingBridge:
    def __init__(self):
        self.lines, self.evals, self.calls, self.loads, self.saves = [], [], [], [], []
        self.out, self.value = "", 0.0

    def exec(self, line):
        self.lines.append(line)
        return {"ok": True, "output": self.out}

    def eval(self, expr):
        self.evals.append(expr)
        return self.value

    def call(self, method, params=None):
        self.calls.append((method, params))
        return {"owner": 1, "count": 2}

    def load_save(self, name):
        self.loads.append(name)

    def save_game(self, name):
        self.saves.append(name)


class FnvExampleAdapter(unittest.TestCase):
    def test_ops_become_console_lines(self):
        b = RecordingBridge()
        g = fnv_game.FnvGame(b)
        g.do("set_state", {"key": "VCG02", "value": 10})
        g.do("move", {"who": "player", "to": "GSPG01Ref"})
        g.do("spawn", {"base": "Gecko", "at": "player", "count": 2})
        g.do("give", {"who": "player", "item": "Stimpak", "count": 3})
        g.do("take", {"who": "Chest1", "item": "Stimpak", "count": 1})
        g.do("kill", {"ref": "GSPG01Ref"})
        g.do("revive", {"ref": "GSPG01Ref"})
        g.do("lock", {"ref": "Door1", "level": 50})
        g.do("activate", {"ref": "Door1"})
        self.assertEqual(b.lines, ["setstage VCG02 10", "player.moveto GSPG01Ref", "player.placeatme Gecko 2",
                                   "player.additem Stimpak 3", "Chest1.removeitem Stimpak 1", "GSPG01Ref.kill",
                                   "GSPG01Ref.resurrect", "Door1.lock 50", "Door1.activate player 1"])

    def test_checks_become_expressions(self):
        b = RecordingBridge()
        b.value = 1.0
        g = fnv_game.FnvGame(b)
        self.assertEqual(g.read("dead", {"ref": "X"}), 1.0)
        g.read("state", {"key": "VCG02"})
        g.read("state_reached", {"key": "VCG02", "at": 10})
        g.read("item_count", {"who": "player", "item": "Stimpak"})
        g.read("distance", {"a": "A", "b": "B"})
        self.assertEqual(b.evals, ["X.GetDead", "GetStage VCG02", "GetStageDone VCG02 10", "player.GetItemCount Stimpak", "A.GetDistance B"])

    def test_console_parse_error_is_an_error(self):
        b = RecordingBridge()
        b.out = "Could not parse the command"
        with self.assertRaises(ScenarioError):
            fnv_game.FnvGame(b).do("kill", {"ref": "X"})

    def test_capabilities_depend_on_project_methods(self):
        plain = fnv_game.FnvGame(RecordingBridge()).capabilities()
        rich = fnv_game.FnvGame(RecordingBridge(), with_project_methods=True).capabilities()
        self.assertNotIn("check:owner", plain)
        self.assertIn("check:owner", rich)
        b = RecordingBridge()
        g = fnv_game.FnvGame(b, with_project_methods=True)
        self.assertEqual(g.read("owner", {"ref": "X"}), 1.0)
        g.reset_state(["VCG02"])
        self.assertEqual(b.calls[-1], ("test.quest.reset", {"quests": ["VCG02"]}))

    def test_runner_blocks_owner_checks_without_project_methods(self):
        b0, b1 = RecordingBridge(), RecordingBridge()
        games = [fnv_game.FnvGame(b0), fnv_game.FnvGame(b1)]
        rn = runner.Runner(games, runner.StateStore(), sleep=lambda s: None)
        r = rn.run(load("ownership_handoff"))
        self.assertEqual(r["status"], "partial")
        self.assertTrue(any(s["status"] == "blocked" and "check:owner" in s["detail"] for s in r["steps"]))
        self.assertIn("GSPG01Ref.enable", b0.lines)            # setup steps default to the host
        self.assertIn("player.moveto GSPG01Ref", b1.lines)     # `on: all` reaches the guest too
        self.assertNotIn("GSPG01Ref.enable", b1.lines)

    def test_console_parse_error_surfaces_as_an_error_step(self):
        b0, b1 = RecordingBridge(), RecordingBridge()
        b0.out = "Could not parse the command"
        games = [fnv_game.FnvGame(b0), fnv_game.FnvGame(b1)]
        sc = inline(objects={"npc": {"id": "NPC1", "kind": "actor"}},
                    steps=[{"id": "k", "op": "kill", "ref": "$npc"}, {"id": "c1", "op": "check", "kind": "dead", "ref": "$npc", "value": 0}])
        r = runner.Runner(games, runner.StateStore(), sleep=lambda s: None).run(sc)
        self.assertEqual(r["status"], "error")
        self.assertIn("NPC1.kill", r["steps"][0]["detail"])

    def test_base_adapter_methods_are_abstract(self):
        g = GameAdapter()
        for call in (lambda: g.do("kill", {}), lambda: g.read("dead", {}), lambda: g.load_save("x"),
                     lambda: g.save_game("x"), lambda: g.reset_state([])):
            with self.assertRaises(NotImplementedError):
                call()


class EventDrivenWaits(unittest.TestCase):
    """After a reload the runner waits on sync_status() readings and returns the moment the condition holds."""

    def jump(self, sim, runner_kw=None, **flags):
        w, clock, games = hooked(sim=sim, **flags)
        j = runner_on(games, clock, prepared=False, **(runner_kw or {})).jump(load("quest_stage_sync"))
        return j, j["coopSync"]

    def test_synced_world_returns_the_moment_the_digest_is_ok(self):
        j, cs = self.jump(fakegame.SyncSim())
        self.assertTrue(cs["synced"] and cs["digestOk"], cs)
        self.assertEqual(cs["settleReason"], "ok")
        self.assertLess(cs["settleSeconds"], 10)               # the first digest comes 6 s after the load; a fixed wait would take 60 s
        self.assertLess(cs["seconds"], 25)
        self.assertLess(j["seconds"], 20)

    def test_slow_but_moving_client_is_not_given_up_on(self):
        _, cs = self.jump(fakegame.SyncSim(catch_up_s=100.0, head=400))   # 100 s to catch up, applied changes at every poll
        self.assertTrue(cs["synced"], cs)

    def test_client_stuck_half_way_fails_fast_and_names_the_numbers(self):
        _, cs = self.jump(fakegame.SyncSim(stuck_at=50))
        self.assertFalse(cs["synced"])
        self.assertIn("no client progress", cs["reason"])
        self.assertIn("applied 50 of 100", cs["reason"])
        self.assertLess(cs["seconds"], 45)                      # not the 180 s limit

    def test_client_that_never_joins_fails_fast(self):
        _, cs = self.jump(fakegame.SyncSim(joined=False))
        self.assertFalse(cs["synced"])
        self.assertIn("not joined", cs["reason"])
        self.assertLess(cs["seconds"], 45)

    def test_client_with_a_dead_link_fails_fast(self):
        _, cs = self.jump(fakegame.SyncSim(disconnected=True))
        self.assertFalse(cs["synced"])
        self.assertIn("link not connected", cs["reason"])
        self.assertLess(cs["seconds"], 45)

    def test_rejected_join_fails_at_once_with_the_text(self):
        _, cs = self.jump(fakegame.SyncSim(rejected=True, joined=False))
        self.assertFalse(cs["synced"])
        self.assertIn("version mismatch", cs["reason"])
        self.assertLess(cs["seconds"], 3)

    def test_same_mismatch_again_fails_fast_and_lists_the_desync_entries(self):
        _, cs = self.jump(fakegame.SyncSim(digests="mismatch_same"))
        self.assertTrue(cs["synced"])
        self.assertFalse(cs["digestOk"])
        self.assertIn("same mismatch", cs["settleReason"])
        self.assertLess(cs["settleSeconds"], 15)                # 3 digests, not the 60 s limit
        e = cs["desyncEntries"][0]
        self.assertEqual((e["key"], e["host"], e["client"]), ("actor:raider1.dead", "1", "0"))
        self.assertEqual(cs["digestDelta"]["mismatch"], 3)

    def test_one_mismatch_that_the_repair_fixes_is_a_success(self):
        _, cs = self.jump(fakegame.SyncSim(digests="mismatch_once", digest_from=10.0))
        self.assertTrue(cs["digestOk"], cs)
        self.assertEqual(cs["settleRepairs"], 1)

    def test_mismatches_on_changing_keys_are_not_called_the_same(self):
        _, cs = self.jump(fakegame.SyncSim(digests="mismatch_moving"))
        self.assertFalse(cs["digestOk"])
        self.assertIn("no ok digest within", cs["settleReason"])      # healing still moves: only the outer limit ends it
        self.assertGreater(len(cs["desyncEntries"]), 1)

    def test_stale_digests_fail_fast(self):
        _, cs = self.jump(fakegame.SyncSim(digests="stale"))
        self.assertFalse(cs["digestOk"])
        self.assertIn("stale", cs["settleReason"])
        self.assertLess(cs["settleSeconds"], 15)

    def test_silent_client_fails_after_the_digest_stall_window(self):
        _, cs = self.jump(fakegame.SyncSim(digests="silent"))
        self.assertFalse(cs["digestOk"])
        self.assertIn("no digest arrived", cs["settleReason"])
        self.assertTrue(20 <= cs["settleSeconds"] < 30, cs)

    def test_limits_are_parameters(self):
        _, cs = self.jump(fakegame.SyncSim(stuck_at=50), runner_kw={"sync": {"stall_s": 5}})
        self.assertIn("no client progress for", cs["reason"])
        self.assertLess(cs["seconds"], 15)                      # a 5 s window, not the default 30 s
        _, cs = self.jump(fakegame.SyncSim(stuck_at=50), runner_kw={"sync": {"stall_s": 0, "timeout_s": 40}})
        self.assertEqual(cs["reason"], "timeout after 40 s")    # stall exit switched off: only the outer limit ends it
        _, cs = self.jump(fakegame.SyncSim(digests="mismatch_same"), runner_kw={"sync": {"repeat_limit": 2}})
        self.assertIn("same mismatch 2 times", cs["settleReason"])

    def test_a_game_without_the_hooks_keeps_the_fixed_pause(self):
        w, clock, games = fakegame.make(2)               # no sync_status, no popup_count
        rn = runner_on(games, clock, prepared=False)
        j = rn.jump(inline(start={"save": "autosave"}))
        self.assertTrue(j["reload"])
        self.assertIsNone(j["coopSync"])
        self.assertIsNone(j["popups"])
        self.assertEqual(j["seconds"], rn.load_wait_s)

    def test_a_single_instance_has_nothing_to_wait_for(self):
        w, clock, games = hooked(n=1, sim=fakegame.SyncSim())
        self.assertIsNone(runner_on(games, clock, prepared=False).jump(load("quest_stage_sync"))["coopSync"])

    def test_run_result_carries_the_reasons(self):
        w, clock, games = hooked(sim=fakegame.SyncSim(digests="mismatch_same"))
        r = runner_on(games, clock, prepared=False).run(load("quest_stage_sync"))
        cs = r["jump"]["coopSync"]
        self.assertIn("same mismatch", cs["settleReason"])
        self.assertIn("loadedSaves", r["jump"])
        self.assertEqual(r["status"], "pass")                    # a digest that never matches does not block the steps

    def op_scenario(self):
        return inline(start={"save": "autosave"}, setup=[{"id": "mv", "op": "move", "who": "player", "to": "$pad"}], objects={
            "npc": {"id": "NPC1", "kind": "actor"}, "pad": {"id": "PAD1", "kind": "place"}})

    def settle_jump(self, sim, runner_kw=None):
        """Load once, then jump in place into a scenario whose setup moves the player. -> (virtual seconds, setup step record)."""
        w, clock, games = hooked(sim=sim)
        rn = runner_on(games, clock, prepared=False, **(runner_kw or {}))
        rn.run(load("quest_stage_sync"))
        t = clock.now()
        j = rn.jump(self.op_scenario())
        self.assertFalse(j["reload"])
        return clock.now() - t, j["setup"][0]

    def test_a_setup_step_waits_until_the_clients_applied_it(self):
        secs, rec = self.settle_jump(fakegame.SyncSim(op_lag_s=3.0))
        self.assertEqual(rec["status"], "pass")
        self.assertNotIn("syncWait", rec)
        self.assertTrue(3.0 <= secs < 5.0, secs)                  # about the lag, not a fixed pause

    def test_a_setup_step_that_the_clients_never_apply_is_reported_but_does_not_hang(self):
        secs, rec = self.settle_jump(fakegame.SyncSim(op_lag_s=1000.0))
        self.assertEqual(rec["status"], "pass")
        self.assertFalse(rec["syncWait"]["synced"])
        self.assertIn("applied 99 of 100", rec["syncWait"]["reason"])
        self.assertLess(secs, 8.0)                                # the 5 s settle stall window, not the 10 s limit


class PreparedSave(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.manifest = Path(self.tmp.name) / "prep.json"

    def tearDown(self):
        self.tmp.cleanup()

    def store(self):
        return prepare_save.PreparedSaves(self.manifest)

    def prepare(self, clock, games, save="autosave", **kw):
        return prepare_save.prepare(games, save, self.store(), log=lambda *a: None, sleep=clock.sleep, now=clock.now, **kw)

    def test_prepare_presses_every_popup_before_it_saves_and_the_manifest_says_so(self):
        w, clock, games = hooked(popups=True)
        r = self.prepare(clock, games)
        self.assertTrue(r["ok"], r)
        self.assertEqual(r["popupsPressed"], 28)                      # 14 popups on each of the two copies
        self.assertEqual(sorted(w.save_log), [(0, "coop_autosave_ready"), (1, "coop_autosave_ready")])
        self.assertIn("coop_autosave_ready", w.clean_saves)           # saved after the last popup was pressed
        self.assertEqual(self.store().pick(1, "autosave", games[1])[0], "coop_autosave_ready")

    def test_prepare_records_the_instance_ids_it_was_given(self):
        # a second pair of copies is instances 2 and 3: prepared saves are recorded and found by those ids
        w, clock, games = hooked(popups=True)
        opened = []

        def factory(i):
            opened.append(i)
            return games[i - 2]
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc = prepare_save.main(["--save", "autosave", "--instances", "2,3"], factory=factory, store=self.store(),
                                   sleep=clock.sleep, now=clock.now, log=lambda *a: None)
        self.assertEqual(rc, 0, out.getvalue())
        self.assertEqual(opened, [2, 3])
        line = json.loads(out.getvalue().strip().splitlines()[-1])
        self.assertTrue(line["ok"])
        self.assertEqual([x["instance"] for x in line["perInstance"]], [2, 3])
        self.assertEqual(self.store().pick(2, "autosave", games[0])[0], "coop_autosave_ready")
        self.assertEqual(self.store().pick(3, "autosave", games[1])[0], "coop_autosave_ready")
        self.assertIsNone(self.store().pick(0, "autosave", games[0])[0])

    def test_jump_loads_the_prepared_save_and_skips_the_long_quiet_window(self):
        w, clock, games = hooked(popups=True)
        plain = runner_on(games, clock, prepared=False).jump(load("quest_stage_sync"))
        self.assertGreater(plain["seconds"], 60)                      # the waves plus the 20 s quiet window
        self.assertEqual(plain["popups"]["pressed"], 28)
        self.assertEqual([x["prepared"] for x in plain["loadedSaves"]], [False, False])
        self.prepare(clock, games)
        w.loads.clear()
        j = runner_on(games, clock, prepared=self.store()).jump(load("quest_stage_sync"), force_reload=True)
        self.assertEqual(sorted(w.loads), [(0, "coop_autosave_ready"), (1, "coop_autosave_ready")])
        self.assertEqual([x["prepared"] for x in j["loadedSaves"]], [True, True])
        self.assertIn("coop_autosave_ready", j["loadedSaves"][0]["why"])
        self.assertEqual(j["popups"]["pressed"], 0)
        self.assertLess(j["seconds"], 12)                             # a short load pause and a 4 s quiet window, not 70 s

    def test_state_still_names_the_plain_start_save(self):
        # the jump plan compares the loaded save with start.save: loading the ready copy must not look like a different save
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        rn = runner_on(games, clock, prepared=self.store())
        rn.jump(load("quest_stage_sync"), force_reload=True)
        self.assertEqual(rn.store.get(rn.keys[0])["save"], "autosave")
        self.assertFalse(rn.jump(load("fight"))["reload"])

    def test_without_a_prepared_save_the_plain_one_loads_with_the_full_quiet_window(self):
        w, clock, games = hooked(popups=True)
        j = runner_on(games, clock, prepared=self.store()).jump(load("quest_stage_sync"))
        self.assertEqual(sorted(w.loads), [(0, "autosave"), (1, "autosave")])
        self.assertIn("no prepared save", j["loadedSaves"][0]["why"])
        self.assertEqual(j["popups"]["pressed"], 28)

    def test_one_copy_without_a_prepared_save_keeps_the_long_window_for_both(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        del w.save_files[(1, "coop_autosave_ready")]                  # the file of copy 1 is gone
        w.loads.clear()
        j = runner_on(games, clock, prepared=self.store()).jump(load("quest_stage_sync"), force_reload=True)
        self.assertEqual(sorted(w.loads), [(0, "coop_autosave_ready"), (1, "autosave")])
        self.assertIn("gone", j["loadedSaves"][1]["why"])
        self.assertEqual(j["popups"]["pressed"], 14)
        self.assertGreater(j["seconds"], 60)

    def test_a_prepared_save_the_game_refuses_falls_back_to_the_plain_save(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        w.refuse_loads.add("coop_autosave_ready")
        w.loads.clear()
        j = runner_on(games, clock, prepared=self.store()).jump(load("quest_stage_sync"), force_reload=True)
        self.assertEqual(sorted(w.loads), [(0, "autosave"), (1, "autosave")])
        self.assertIn("refused", j["loadedSaves"][0]["why"])
        self.assertEqual([x["prepared"] for x in j["loadedSaves"]], [False, False])

    def test_scenario_can_opt_out(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        w.loads.clear()
        sc = copy.deepcopy(load("quest_stage_sync"))
        sc["start"]["prepared"] = False
        self.assertEqual(scenlib.validate(sc), [])
        j = runner_on(games, clock, prepared=self.store()).jump(sc, force_reload=True)
        self.assertEqual(sorted(w.loads), [(0, "autosave"), (1, "autosave")])
        self.assertIn("start.prepared", j["loadedSaves"][0]["why"])
        sc["start"]["prepared"] = "no"
        self.assertTrue(any("start.prepared" in e for e in scenlib.validate(sc)))

    def test_environment_variable_opts_out(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        self.assertIsNotNone(runner_on(games, clock, prepared=self.store()).prepared)
        with unittest.mock.patch.dict(os.environ, {prepare_save.NO_PREPARED_ENV: "1"}):
            rn = runner_on(games, clock, prepared=self.store())
            self.assertIsNone(rn.prepared)
            w.loads.clear()
            rn.jump(load("quest_stage_sync"), force_reload=True)
            self.assertEqual(sorted(w.loads), [(0, "autosave"), (1, "autosave")])
            self.assertIsNone(prepare_save.PreparedSaves.auto())

    def test_default_manifest_is_found_through_the_environment(self):
        w, clock, games = hooked(popups=True)
        with unittest.mock.patch.dict(os.environ, {prepare_save.MANIFEST_ENV: str(self.manifest)}):
            self.assertIsNone(prepare_save.PreparedSaves.auto())      # no file yet
            self.prepare(clock, games)
            auto = prepare_save.PreparedSaves.auto()
            self.assertEqual(auto.pick(0, "autosave", games[0])[0], "coop_autosave_ready")
            self.assertIsNotNone(runner_on(games, clock).prepared)    # a Runner picks it up with no argument

    def test_a_source_save_that_changed_makes_the_prepared_one_stale(self):
        w, clock, games = hooked(popups=True)
        for i in (0, 1):
            w.touch_save(i, "mysave")
        self.prepare(clock, games, save="mysave")
        store = self.store()
        self.assertEqual(store.pick(0, "mysave", games[0])[0], "coop_mysave_ready")
        w.touch_save(0, "mysave")                                     # the source save of copy 0 is replaced
        name, why = store.pick(0, "mysave", games[0])
        self.assertIsNone(name)
        self.assertIn("changed", why)
        self.assertEqual(store.pick(1, "mysave", games[1])[0], "coop_mysave_ready")   # the other copy is untouched

    def test_a_rolling_save_is_exempt_from_the_stale_check(self):
        # the game rewrites "autosave" itself, so its stamp proves nothing about the prepared copy
        w, clock, games = hooked(popups=True)
        w.touch_save(0, "autosave")
        self.prepare(clock, games)
        w.touch_save(0, "autosave")
        self.assertEqual(self.store().pick(0, "autosave", games[0])[0], "coop_autosave_ready")

    def test_a_save_whose_reload_still_raised_popups_is_not_used(self):
        w, clock, games = hooked(popups=True, popups_persist=True)   # the prepared save raises the popups again
        r = self.prepare(clock, games)
        self.assertFalse(r["ok"])
        self.assertTrue(all(not x["verified"] and "still raised" in x["error"] for x in r["perInstance"]), r)
        name, why = self.store().pick(0, "autosave", games[0])
        self.assertIsNone(name)
        self.assertIn("reload check", why)

    def test_a_missing_file_is_not_used(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        del w.save_files[(0, "coop_autosave_ready")]
        name, why = self.store().pick(0, "autosave", games[0])
        self.assertIsNone(name)
        self.assertIn("gone", why)

    def test_without_popup_hooks_nothing_is_saved(self):
        w, clock, games = fakegame.make(2)                            # popup_count() gives None
        with self.assertRaises(prepare_save.NoPopupHooks):
            self.prepare(clock, games)
        self.assertEqual(w.save_log, [])
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc = prepare_save.main(["--save", "autosave"], factory=lambda i: games[i], store=self.store(),
                                   sleep=clock.sleep, now=clock.now)
        self.assertEqual(rc, 2, out.getvalue())
        self.assertIn("cannot report popups", out.getvalue())

    def test_popups_that_never_go_quiet_save_nothing_and_exit_1(self):
        w, clock, games = hooked(popups=True)
        w.popup_offsets = list(range(1, 500))                         # a new popup every second, for ever
        with self.assertRaises(RuntimeError) as cm:
            self.prepare(clock, games, max_s=30.0)
        self.assertIn("still coming", str(cm.exception))
        self.assertEqual(w.save_log, [])
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc = prepare_save.main(["--save", "autosave"], factory=lambda i: games[i], store=self.store(),
                                   sleep=clock.sleep, now=clock.now, max_s=30.0, log=lambda *a: None)
        self.assertEqual(rc, 1, out.getvalue())

    def test_exit_code_is_1_when_the_reload_check_fails(self):
        w, clock, games = hooked(popups=True, popups_persist=True)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc = prepare_save.main(["--save", "autosave"], factory=lambda i: games[i], store=self.store(),
                                   sleep=clock.sleep, now=clock.now, log=lambda *a: None)
        self.assertEqual(rc, 1)
        self.assertFalse(json.loads(out.getvalue().strip().splitlines()[-1])["ok"])

    def test_engine_with_a_naming_rule_gets_the_second_name(self):
        w, clock, games = hooked(popups=True, name_prefix="autosave")
        r = self.prepare(clock, games)
        self.assertTrue(r["ok"], r)
        self.assertEqual(self.store().pick(0, "autosave", games[0])[0], "autosave_coop_autosave_ready")

    def test_status_shows_what_is_prepared_without_changing_anything(self):
        w, clock, games = hooked(popups=True)
        self.prepare(clock, games)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            rc = prepare_save.main(["--status", "--save", "autosave", "--save", "other"], factory=lambda i: games[i], store=self.store())
        self.assertEqual(rc, 0)
        rows = [json.loads(x) for x in out.getvalue().strip().splitlines()]
        self.assertEqual([(r["save"], r["instance"], bool(r["prepared"])) for r in rows],
                         [("autosave", 0, True), ("autosave", 1, True), ("other", 0, False), ("other", 1, False)])
        self.assertTrue(all(r["checked"] for r in rows))
        self.assertEqual(len(w.save_log), 2)                          # only the two saves of the prepare itself: --status wrote nothing

    def test_runner_takes_instance_ids_from_the_adapters(self):
        w, clock, games = hooked()
        self.assertEqual(runner_on(games, clock).instance_ids, [0, 1])
        for g, n in zip(games, (2, 3)):
            g.instance = n
        self.assertEqual(runner_on(games, clock).instance_ids, [2, 3])
        self.assertEqual(runner_on(games, clock, instances=[4, 5]).instance_ids, [4, 5])

    def test_prepared_saves_are_found_by_the_instance_id_of_a_second_pair(self):
        w, clock, games = hooked(popups=True)
        for g, n in zip(games, (2, 3)):
            g.instance = n
        prepare_save.prepare(games, "autosave", self.store(), instances=[2, 3], log=lambda *a: None, sleep=clock.sleep, now=clock.now)
        j = runner_on(games, clock, prepared=self.store()).jump(load("quest_stage_sync"), force_reload=True)
        self.assertEqual([x["prepared"] for x in j["loadedSaves"]], [True, True])
        self.assertEqual([x["instance"] for x in j["loadedSaves"]], [2, 3])


class DataIndexValidation(unittest.TestCase):
    OK = {"NPC1": "actor", "Q1": "state", "PAD1": "place", "GUN1": "item", "BOX1": "thing", "MOB1": "base"}

    def errs(self, sc, index):
        return scenlib.validate(sc, hooks=[scenlib.index_hook(index)])

    def test_a_scenario_whose_ids_all_exist_is_clean(self):
        self.assertEqual(self.errs(inline(), self.OK), [])
        for n in EXAMPLES:
            sc = load(n)
            self.assertEqual(self.errs(sc, full_index(sc)), [], n)

    def test_an_unknown_id_names_the_nearest_match_and_where_it_is_used(self):
        index = dict(self.OK, NPC10="actor", NPC2="actor")
        del index["NPC1"]
        errs = self.errs(inline(), index)
        self.assertEqual(len(errs), 1, errs)
        self.assertIn("objects.npc", errs[0])
        self.assertIn("'NPC1' is not in the game data", errs[0])
        self.assertIn("nearest known id: 'NPC10' (also close: 'NPC2')", errs[0])
        self.assertIn("used by step c1 (check)", errs[0])

    def test_an_id_with_nothing_like_it_says_so_without_a_guess(self):
        index = dict(self.OK, ZZZZZZZZ="actor")
        del index["NPC1"]
        errs = self.errs(inline(), index)
        self.assertEqual(len(errs), 1)
        self.assertNotIn("nearest", errs[0])

    def test_setup_ops_are_checked_and_named(self):
        sc = inline(setup=[{"id": "mv", "op": "move", "who": "player", "to": "$pad"},
                           {"id": "gv", "op": "give", "who": "player", "item": "$gun", "count": 1},
                           {"id": "en", "op": "enable", "ref": "$box"}])
        index = {"NPC1": "actor", "Q1": "state", "PADD1": "place", "GUN_1": "item", "BOX1": "item", "MOB1": "base"}
        errs = self.errs(sc, index)
        text = "\n".join(errs)
        self.assertIn("setup step mv (move)", text)
        self.assertIn("nearest known id: 'PADD1'", text)
        self.assertIn("setup step gv (give)", text)
        self.assertIn("nearest known id: 'GUN_1'", text)
        self.assertIn("setup step en (enable)", text)
        self.assertIn("is a item in the game data but the scenario uses it as thing", text)

    def test_a_wrong_kind_is_an_error_and_an_open_kind_is_not(self):
        errs = self.errs(inline(), dict(self.OK, NPC1="item"))
        self.assertEqual(len(errs), 1)
        self.assertIn("is a item", errs[0])
        self.assertEqual(self.errs(inline(), dict(self.OK, NPC1=None)), [])                 # None: any kind
        self.assertEqual(self.errs(inline(), dict(self.OK, GUN1=["item", "base"])), [])     # several kinds

    def test_a_base_id_is_checked_as_an_item_or_spawnable_type(self):
        sc = inline()
        sc["objects"]["box"]["base"] = "GUN9"
        errs = self.errs(sc, self.OK)
        self.assertEqual(len(errs), 1, errs)
        self.assertIn("base 'GUN9'", errs[0])
        self.assertIn("nearest known id: 'GUN1'", errs[0])

    def test_kinds_the_index_does_not_cover_are_skipped(self):
        idx = scenlib.DataIndex({"GUN1": "item"}, kinds={"item"})
        self.assertEqual(self.errs(inline(), idx), [])                                      # actors, places ... are not in this index
        sc = inline()
        sc["objects"]["gun"]["id"] = "GUN7"
        self.assertEqual(len(self.errs(sc, idx)), 1)

    def test_an_index_that_ignores_case_matches_that_way_and_keeps_the_data_spelling(self):
        idx = scenlib.DataIndex({k.lower(): v for k, v in self.OK.items()}, case_sensitive=False)
        self.assertEqual(self.errs(inline(), idx), [])
        sc = inline()
        sc["objects"]["npc"]["id"] = "Npc9"
        self.assertIn("nearest known id: 'npc1'", self.errs(sc, idx)[0])

    def test_a_dict_a_list_and_a_data_index_are_all_accepted(self):
        self.assertEqual(self.errs(inline(), list(self.OK)), [])
        self.assertEqual(self.errs(inline(), scenlib.DataIndex(self.OK)), [])
        self.assertEqual(len(scenlib.as_index(self.OK)), 6)
        self.assertIsNone(scenlib.as_index(None))

    def test_the_runner_rejects_unknown_ids_before_any_game_call(self):
        w, clock, games = hooked(index=dict({k: v for k, v in self.OK.items() if k != "NPC1"}, NPC2="actor"))
        r = runner_on(games, clock).run(inline())
        self.assertEqual(r["status"], "invalid")
        self.assertIn("nearest known id: 'NPC2'", r["errors"][0])
        self.assertEqual(w.calls, [])                                # nothing reached the game

    def test_the_runner_runs_a_scenario_that_the_index_knows(self):
        w, clock, games = hooked(index=self.OK)
        r = runner_on(games, clock).run(inline())
        self.assertEqual(r["status"], "pass", r)
        self.assertNotIn("notes", r)

    def test_a_game_without_an_index_skips_the_check_with_a_note(self):
        w, clock, games = hooked()
        r = runner_on(games, clock).run(inline())
        self.assertEqual(r["status"], "pass")
        self.assertEqual(len(r["notes"]), 1)
        self.assertIn("data_index()", r["notes"][0])
        self.assertIn("note:", runner.summarize([r]))
        self.assertEqual(runner.summarize([r, r]).count("note:"), 1)   # said once, not once per scenario

    def test_start_prepared_must_be_a_boolean(self):
        sc = inline()
        sc["start"]["prepared"] = 1
        self.assertTrue(any("start.prepared" in e for e in scenlib.validate(sc)))

    def test_fnv_example_builds_its_index_from_record_types(self):
        g = fnv_game.FnvGame(RecordingBridge(), records={"GSPG01Ref": "ACHR", "GSPG01": "NPC_", "Stimpak": "ALCH", "VCG02": "QUST"})
        sc = load("fight")
        errs = scenlib.validate(sc, hooks=[scenlib.index_hook(g.data_index())])
        self.assertTrue(any("'GSPG02Ref' is not in the Fallout data" in e and "nearest known id: 'GSPG01Ref'" in e for e in errs), errs)
        self.assertIsNone(fnv_game.FnvGame(RecordingBridge()).data_index())
        idx = g.data_index()
        self.assertEqual(idx.lookup("gspg01ref")[0], "GSPG01Ref")                          # editor ids ignore case
        self.assertEqual(idx.lookup("Stimpak")[1], frozenset(["item", "base"]))


class FnvExampleHooks(unittest.TestCase):
    def bridge(self, **replies):
        b = RecordingBridge()
        b.replies = replies
        b.call = lambda method, params=None: (b.calls.append((method, params)) or b.replies.get(method, {}))
        return b

    def test_popup_hooks_follow_bot_menu(self):
        b = self.bridge(**{"bot.menu": {"kind": "message"}})
        g = fnv_game.FnvGame(b, with_project_methods=True)
        self.assertEqual(g.popup_count(), 1)
        self.assertTrue(g.dismiss_popup())
        self.assertEqual(b.calls[-1], ("bot.menu.press", {"button": 0}))
        b.replies["bot.menu"] = {"kind": "none"}
        self.assertEqual(g.popup_count(), 0)
        self.assertIsNone(fnv_game.FnvGame(b).popup_count())              # no project methods: cannot tell

    def test_sync_status_maps_the_host_and_client_readings(self):
        b = self.bridge(**{"coop.status": {"role": "host", "host": {"lastSeq": 7}, "desyncs": {
            "digests": {"ok": 2}, "desyncs": [{"id": 3, "repaired": True, "entries": [{"key": "k", "host": "1", "client": "0"}]}]}}})
        g = fnv_game.FnvGame(b, with_project_methods=True)
        self.assertEqual(g.sync_status(), {"role": "host", "head": 7, "digests": {"ok": 2},
                                           "desyncs": [{"id": 3, "key": "k", "host": "1", "client": "0", "repaired": True}]})
        b.replies["coop.status"] = {"role": "client", "transport": {"connected": False},
                                    "client": {"joined": True, "syncing": True, "applied": 5, "rejected": True, "rejectText": "no room"}}
        self.assertEqual(g.sync_status(), {"role": "client", "joined": True, "syncing": True, "applied": 5, "connected": False,
                                           "rejected": "no room"})
        b.replies["coop.status"] = {"role": "off"}
        self.assertEqual(g.sync_status(), {"role": None})
        self.assertIsNone(fnv_game.FnvGame(b).sync_status())

    def test_the_runner_waits_on_the_example_adapter_readings(self):
        # a client that never applies anything: the wait ends with the numbers, driven by the example adapter alone
        host = {"role": "host", "host": {"lastSeq": 9}, "desyncs": {}}
        client = {"role": "client", "client": {"joined": True, "applied": 2}}
        clock = fakegame.Clock()
        games = [fnv_game.FnvGame(self.bridge(**{"coop.status": host}), with_project_methods=True),
                 fnv_game.FnvGame(self.bridge(**{"coop.status": client}), with_project_methods=True)]
        cs = runner_on(games, clock, prepared=False).wait_synced()
        self.assertFalse(cs["synced"])
        self.assertIn("client 0: applied 2 of 9", cs["reason"])

    def test_prepared_names_and_save_stamps(self):
        with tempfile.TemporaryDirectory() as d:
            g = fnv_game.FnvGame(RecordingBridge(), saves_dir=d)
            self.assertEqual(g.prepared_names("goodsprings"), ["nvcoop_goodsprings_ready", "autosave_nvcoop_goodsprings_ready"])
            self.assertFalse(g.save_exists("goodsprings"))
            self.assertIsNone(g.save_stamp("goodsprings"))
            (Path(d) / "goodsprings.fos").write_bytes(b"one")
            self.assertTrue(g.save_exists("goodsprings"))
            first = g.save_stamp("goodsprings")
            (Path(d) / "goodsprings.fos").write_bytes(b"another world")
            self.assertNotEqual(g.save_stamp("goodsprings"), first)
            self.assertIsNone(fnv_game.FnvGame(RecordingBridge()).save_exists("goodsprings"))   # no saves folder: cannot tell


class AdapterHooksDefault(unittest.TestCase):
    def test_a_plain_adapter_says_it_cannot_tell(self):
        g = GameAdapter()
        self.assertIsNone(g.sync_status())
        self.assertIsNone(g.popup_count())
        self.assertIsNone(g.save_exists("x"))
        self.assertIsNone(g.save_stamp("x"))
        self.assertIsNone(g.data_index())
        self.assertEqual(g.prepared_names("s"), ["coop_s_ready"])
        self.assertEqual(g.ROLLING_SAVES, ())
        with self.assertRaises(NotImplementedError):
            g.dismiss_popup()

    def test_an_adapter_that_raises_not_implemented_in_a_hook_counts_as_not_having_it(self):
        class Odd(fakegame.FakeGame):
            def sync_status(self):
                raise NotImplementedError("sync_status")

            def popup_count(self):
                raise ScenarioError("not now")
        w, clock, games = fakegame.make(2)
        odd = [Odd(w, i, clock) for i in range(2)]
        rn = runner_on(odd, clock, prepared=False)
        j = rn.jump(load("quest_stage_sync"))
        self.assertIsNone(j["coopSync"])
        self.assertIsNone(j["popups"])


class Cli(unittest.TestCase):
    def run_cli(self, *args):
        p = subprocess.run([sys.executable, "-I", "-X", "utf8", str(HERE / "scen.py"), *args], capture_output=True, text=True)
        return p.returncode, p.stdout + p.stderr

    def test_validate_list_plan(self):
        code, out = self.run_cli("validate")
        self.assertEqual(code, 0, out)
        self.assertIn("0 invalid", out)
        code, out = self.run_cli("list")
        self.assertEqual(code, 0)
        self.assertEqual(len([l for l in out.splitlines() if l.strip()]), len(EXAMPLES))
        code, out = self.run_cli("plan", "ownership_handoff", "--prev", "fight")
        self.assertEqual(code, 0)
        self.assertIn('"reload": true', out)

    def test_validate_exit_code_on_a_broken_file(self):
        with tempfile.TemporaryDirectory() as d:
            bad = load("fight")
            bad["steps"][0]["ref"] = "$ghost"
            p = Path(d) / "fight.json"
            p.write_text(json.dumps(bad), encoding="utf-8")
            code, out = self.run_cli("validate", str(p))
            self.assertEqual(code, 1, out)
            self.assertIn("unknown alias", out)

    def test_fake_run(self):
        code, out = self.run_cli("run", "fight", "loot_race", "quest_stage_sync", "ownership_handoff", "--fake")
        self.assertEqual(code, 0, out)
        self.assertEqual(out.count("PASS"), 4, out)
        code, out = self.run_cli("run", "fight")
        self.assertEqual(code, 2, out)

    def adapter_module(self, d, index=None):
        """A one-file adapter module in folder d: make_adapter(i) returns a fake game that knows `index`."""
        lines = ["import sys", "sys.path.insert(0, %r)" % str(HERE), "import fakegame",
                 "def make_adapter(i):", "    w, clock, games = fakegame.make(2)", "    w.index = %r" % (index,), "    return games[i % 2]"]
        (Path(d) / "tmp_game.py").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def run_in(self, d, script, *args):
        p = subprocess.run([sys.executable, "-I", "-X", "utf8", str(HERE / script), *args], capture_output=True, text=True, cwd=d)
        return p.returncode, p.stdout + p.stderr

    def test_validate_against_an_index_file(self):
        with tempfile.TemporaryDirectory() as d:
            sc = load("fight")
            idx = Path(d) / "ids.json"
            idx.write_text(json.dumps(full_index(sc)), encoding="utf-8")
            code, out = self.run_cli("validate", "fight", "--index", str(idx))
            self.assertEqual(code, 0, out)
            bad = full_index(sc)
            bad["GSPG02Rex"] = bad.pop("GSPG02Ref")
            idx.write_text(json.dumps(bad), encoding="utf-8")
            code, out = self.run_cli("validate", "fight", "--index", str(idx))
            self.assertEqual(code, 1, out)
            self.assertIn("nearest known id: 'GSPG02Rex'", out)     # the best match comes first

    def test_validate_with_an_adapter_uses_its_data_index_or_says_there_is_none(self):
        with tempfile.TemporaryDirectory() as d:
            sc = load("fight")
            self.adapter_module(d, index={k: v for k, v in full_index(sc).items() if k != "GSPG03Ref"})
            code, out = self.run_in(d, "scen.py", "validate", "fight", "--adapter", "tmp_game:make_adapter")
            self.assertEqual(code, 1, out)
            self.assertIn("'GSPG03Ref' is not in the game data", out)
            self.adapter_module(d, index=None)
            code, out = self.run_in(d, "scen.py", "validate", "fight", "--adapter", "tmp_game:make_adapter")
            self.assertEqual(code, 0, out)
            self.assertIn("provides no data_index()", out)

    def test_prepare_save_command_line_usage_and_status(self):
        with tempfile.TemporaryDirectory() as d:
            code, out = self.run_in(d, "prepare_save.py", "--save", "autosave")
            self.assertEqual(code, 2, out)
            self.assertIn("--adapter", out)
            code, out = self.run_in(d, "prepare_save.py", "--status", "fight")                # no game, no adapter needed
            self.assertEqual(code, 0, out)
            row = json.loads(out.strip().splitlines()[0])
            self.assertEqual((row["save"], row["instance"], row["prepared"], row["checked"]), ("autosave", 0, None, False))
            self.adapter_module(d)
            code, out = self.run_in(d, "prepare_save.py", "--status", "fight", "--adapter", "tmp_game:make_adapter", "--instances", "2,3")
            self.assertEqual(code, 0, out)
            self.assertEqual([json.loads(x)["instance"] for x in out.strip().splitlines()], [2, 3])
            code, out = self.run_in(d, "prepare_save.py", "--save", "autosave", "--adapter", "tmp_game:make_adapter", "--instances", "x")
            self.assertEqual(code, 2, out)
            code, out = self.run_in(d, "prepare_save.py", "nosuchscenario")
            self.assertEqual(code, 2, out)


MUTANTS = {
    "noalias": "validator stops checking aliases",
    "noreload": "jump never reloads the save",
    "nodirty": "scenarios never record what they changed",
    "passall": "every check passes",
    "firsthost": "every step runs only on the host",
    "nostall": "the sync wait never gives up on a client that stopped",
    "fullwait": "the digest wait never gives up early",
    "latesuccess": "the digest wait notices an ok digest only at its time limit",
    "hairtrigger": "the first mismatch already ends the digest wait",
    "nodesyncs": "a failed digest wait leaves out the desync entries",
    "noprepared": "jump never loads the prepared save",
    "longquiet": "a prepared save still waits the long quiet window of a plain save",
    "nostale": "a prepared save is used although its source save changed",
    "unverified": "a prepared save that failed its reload check is used",
    "noverify": "prepare_save skips the reload check",
    "earlysave": "prepare_save saves before the popups are pressed away",
    "posids": "prepare_save records instances by position, not by the ids given",
    "noindex": "scenarios are never checked against the game data index",
    "nonearest": "an unknown id error does not name the nearest match",
    "anykind": "an id of the wrong kind passes the index check",
}


def main():
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    if os.environ.get("SCEN_MUTANT"):
        res = unittest.TextTestRunner(stream=open(os.devnull, "w"), verbosity=0).run(suite)
        return 0 if res.wasSuccessful() else 1
    res = unittest.TextTestRunner(verbosity=1).run(suite)
    print("tests run: %d" % res.testsRun)
    if not res.wasSuccessful():
        print("FAILED")
        return 1
    bad = []
    for m, what in MUTANTS.items():
        env = dict(os.environ, SCEN_MUTANT=m)
        p = subprocess.run([sys.executable, "-I", str(Path(__file__).resolve())], env=env, capture_output=True, text=True)
        caught = p.returncode != 0
        print("twin %-11s %-60s %s" % (m, what, "caught" if caught else "NOT CAUGHT"))
        if not caught:
            bad.append(m)
    if bad:
        print("FAILED: twins not caught: %s" % bad)
        return 1
    print("twins caught: %d of %d" % (len(MUTANTS), len(MUTANTS)))
    print("ALL OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
