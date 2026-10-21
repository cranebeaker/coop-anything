"""Resumable steps (--from-step, --only-step), fail-fast settle and --timescale, on the fake co-op world."""
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest

import support
from support import HERE, OPTS, core, fakecoop, registry, run_pb
from coopbots.adapter import ActionAdapter, Result

CLI = os.path.join(HERE, "run_coop.py")


def run_cli(*args):
    p = subprocess.run([sys.executable, "-I", CLI, *args], capture_output=True, text=True, errors="replace")
    return p.returncode, p.stdout + p.stderr


def partial(name, bugs=(), **opts):
    """Run one playbook on a fresh fake world with from_step / only_step; returns (result, world, log lines)."""
    e = registry.REGISTRY[name]
    w, refs = e.fake(e.players, set(bugs))
    lines = []
    o = dict(OPTS)
    o.update(opts)
    res = core.run_playbook(registry.make(name), fakecoop.make_machines(w), fakecoop.FakeComparer(), tempfile.mkdtemp(prefix="coopbots-"),
                            refs, o, lines.append)
    return res, w, lines


class ResumableSteps(unittest.TestCase):
    def test_a_full_run_numbers_its_steps_and_skips_nothing(self):
        res, _, lines = partial("fight")
        self.assertTrue(res.ok, res.reason)
        self.assertEqual([s.number for s in res.steps], [1, 2, 3, 4, 5, 6])
        self.assertEqual(res.skipped, [])
        self.assertFalse(any("arranging" in l for l in lines), "a full run never arranges")

    def test_from_step_skips_the_earlier_steps_and_arranges_once(self):
        res, w, lines = partial("fight", from_step=4)
        self.assertTrue(res.ok, res.reason)
        self.assertEqual([s.number for s in res.steps], [4, 5, 6])
        self.assertEqual(len(res.skipped), 3)
        self.assertEqual(sum("arranging" in l for l in lines), 1)
        self.assertEqual(res.to_json()["from_step"], 4)
        self.assertTrue(all(w.t(r)["dead"] for r in ("raider_1", "raider_2", "raider_3")))

    def test_only_step_runs_exactly_one_step(self):
        res, _, _ = partial("fight", only_step=5)
        self.assertTrue(res.ok, res.reason)
        self.assertEqual([s.number for s in res.steps], [5])
        self.assertEqual(len(res.skipped), 5)

    def test_the_arrange_part_brings_the_world_to_the_state_the_step_expects(self):
        # step 3 kills enemy 1 and expects all enemies alive; step 6 expects them all dead
        res, w, _ = partial("fight", only_step=3)
        self.assertTrue(res.ok, res.reason)
        self.assertTrue(w.t("raider_1")["dead"])
        self.assertFalse(w.t("raider_2")["dead"])
        res, w, _ = partial("fight", only_step=6)
        self.assertTrue(res.ok, res.reason)

    def test_arrange_for_a_quest_sets_the_earlier_stages(self):
        res, w, _ = partial("quest_sync", from_step=4)
        self.assertTrue(res.ok, res.reason)
        names = [s.name for s in res.steps]
        self.assertEqual(names[0], "stage 20")
        for bot in w.bots:
            self.assertTrue(bot.stage_done("town_quest", 5))
            self.assertTrue(bot.stage_done("town_quest", 10))

    def test_loot_race_can_start_at_a_drop_round(self):
        res, w, _ = partial("loot_race", only_step=3)
        self.assertTrue(res.ok, res.reason)
        self.assertEqual([s.number for s in res.steps], [3])

    def test_twin_a_resumed_run_still_catches_the_bug(self):
        res, _, _ = partial("fight", bugs={"stale_echo"}, from_step=4)
        self.assertFalse(res.ok)
        res, _, _ = partial("quest_sync", bugs={"no_stage_sync"}, from_step=3)
        self.assertFalse(res.ok)

    def test_an_arrange_that_cannot_work_fails_as_kind_arrange(self):
        e = registry.REGISTRY["fight"]
        w, refs = e.fake(2)
        full = run_pb(registry.make("fight"), w, refs)
        self.assertTrue(full.ok, full.reason)
        again = run_pb(registry.make("fight"), w, refs, opts={"only_step": 3})      # the enemies are already dead
        self.assertFalse(again.ok)
        self.assertEqual(again.steps[-1].kind, "arrange")
        self.assertIn("already dead", again.reason)
        self.assertIn("run the whole playbook", again.reason)

    def test_a_playbook_without_arrange_parts_refuses_a_partial_run(self):
        res, _, _ = partial("door_lock", from_step=2)
        self.assertEqual(res.status, "fail")
        self.assertIn("cannot run from a later step", res.reason)
        self.assertEqual(res.steps, [])

    def test_a_step_number_beyond_the_last_one_is_an_error_not_a_pass(self):
        res, _, _ = partial("fight", from_step=99)
        self.assertFalse(res.ok)
        self.assertIn("no step was selected", res.reason)

    def test_the_resumable_list_matches_the_playbook_flags(self):
        flagged = {n for n in registry.REGISTRY if registry.make(n).resumable}
        self.assertEqual(flagged, set(registry.RESUMABLE))
        self.assertGreaterEqual(len(flagged), 3)

    def test_cli_from_step_and_only_step(self):
        d = tempfile.mkdtemp()
        code, out = run_cli("--fake", "--playbook", "fight", "--from-step", "4", "--outdir", d)
        self.assertEqual(code, 0, out)
        self.assertIn("arranging the world for step 04", out)
        code, out = run_cli("--fake", "--playbook", "fight", "--only-step", "6", "--outdir", d)
        self.assertEqual(code, 0, out)
        code, out = run_cli("--fake", "--playbook", "door_lock", "--from-step", "2", "--outdir", d)
        self.assertEqual(code, 2)
        self.assertIn("Resumable: fight, loot_race, quest_sync", out)
        self.assertEqual(run_cli("--fake", "--playbook", "fight", "--from-step", "2", "--only-step", "3")[0], 2)

    def test_cli_names_the_rerun_line_of_a_failed_step(self):
        code, out = run_cli("--fake", "--playbook", "fight", "--bug", "stale_echo", "--outdir", tempfile.mkdtemp())
        self.assertEqual(code, 1)
        self.assertRegex(out, r"rerun that step: --playbook fight --from-step 3 ")

    def test_cli_list_marks_the_resumable_playbooks(self):
        code, out = run_cli("--list")
        self.assertEqual(code, 0)
        self.assertRegex(out, r"fight\s+2p  R ")
        self.assertRegex(out, r"door_lock\s+2p  - ")


class FailFastSettle(unittest.TestCase):
    def frozen(self, **opts):
        w, refs = registry.REGISTRY["quest_sync"].fake(2, {"client_frozen"})
        o = dict(OPTS)
        o.update(opts)
        ctx = core.Ctx(fakecoop.make_machines(w), fakecoop.FakeComparer(), tempfile.mkdtemp(prefix="coopbots-"), refs, o, "t")
        w.bots[0].start_quest("town_quest")             # the host sends an update that the frozen client never applies
        return ctx

    def test_settle_stops_early_when_nothing_moves_and_says_why(self):
        ctx = self.frozen(settle_s=3.0, settle_stall_s=0.3, settle_poll_s=0.02)
        t0 = time.monotonic()
        with self.assertRaises(core.StepFailure) as cm:
            ctx.settle()
        self.assertLess(time.monotonic() - t0, 2.5, "it waited for the whole budget")
        self.assertEqual(cm.exception.kind, "settle")
        self.assertIn("no progress for", cm.exception.reason)
        self.assertIn("stopped early", cm.exception.reason)
        self.assertIn("client acks {'client1': 0}", cm.exception.reason)
        self.assertIn("last update number per machine", cm.exception.reason)

    def test_with_the_early_stop_off_the_whole_budget_is_used(self):
        ctx = self.frozen(settle_s=0.6, settle_stall_s=0, settle_poll_s=0.02)
        t0 = time.monotonic()
        with self.assertRaises(core.StepFailure) as cm:
            ctx.settle()
        self.assertGreaterEqual(time.monotonic() - t0, 0.55)
        self.assertIn("did not converge within 0.6s", cm.exception.reason)
        self.assertNotIn("no progress", cm.exception.reason)

    def test_a_short_budget_is_never_cut_short_by_the_default_window(self):
        ctx = self.frozen(settle_s=0.4, settle_poll_s=0.02)            # default window is 6 s, longer than the budget
        with self.assertRaises(core.StepFailure) as cm:
            ctx.settle()
        self.assertIn("did not converge within 0.4s", cm.exception.reason)

    def test_a_slow_but_moving_client_is_not_cut_short(self):
        w, refs = registry.REGISTRY["quest_sync"].fake(2, set(), lag=6)
        o = dict(OPTS, settle_s=5.0, settle_stall_s=0.3, settle_poll_s=0.0)
        ctx = core.Ctx(fakecoop.make_machines(w), fakecoop.FakeComparer(), tempfile.mkdtemp(prefix="coopbots-"), refs, o, "t")
        for n in (1, 2, 3):
            w.bots[0].set_stage("town_quest", n)
        ctx.settle()                                                    # lag of 6 ticks: still moving, so it finishes

    def test_the_reason_mentions_a_mismatch_that_keeps_coming_back(self):
        ctx = core.Ctx(fakecoop.make_machines(registry.REGISTRY["quest_sync"].fake(2)[0]), None, tempfile.mkdtemp(), {}, dict(OPTS), "t")
        st = {"host": {"seq": 5, "synced": False, "counters": {"mismatches": 4}, "acks": {"client1": 3}},
              "client1": {"seq": 3, "counters": {}}}
        text = ctx._stuck_detail(st, {"host": 5, "client1": 3}, False, 1)
        self.assertIn("mismatch counter rose by 3", text)
        self.assertIn("client acks {'client1': 3}", text)

    def test_twin_the_playbook_fails_with_the_stall_reason_in_a_frozen_session(self):
        res, _ = support.run("quest_sync", bugs={"client_frozen"}, opts={"settle_s": 3.0, "settle_stall_s": 0.3, "settle_poll_s": 0.02})
        self.assertFalse(res.ok)


class OneSpeedChecker(core.Playbook):
    """A playbook that records the game speed every machine runs at while its step runs."""
    name = "speed_probe"
    needs_refs = ()
    seen = None

    def run(self, ctx):
        with ctx.step("read the speed", compare=False):
            OneSpeedChecker.seen = {m.name: m.bot.timescale() for m in ctx.machines}


class Timescale(unittest.TestCase):
    def go(self, bugs=(), x=3.0, pb=None, world=None):
        w, refs = world or registry.REGISTRY["quest_sync"].fake(2, set(bugs))
        res = run_pb(pb or OneSpeedChecker(), w, refs, opts={"timescale": x})
        return res, w

    def test_the_game_runs_at_the_asked_speed_during_the_steps_and_at_1_after(self):
        res, w = self.go()
        self.assertTrue(res.ok, res.reason)
        self.assertEqual(OneSpeedChecker.seen, {"host": 3.0, "client1": 3.0})
        self.assertEqual(w.speed, [1.0, 1.0])
        self.assertEqual(w.speed_log, [[3.0, 1.0], [3.0, 1.0]])
        self.assertEqual(res.to_json()["timescale"]["requested"], 3.0)

    def test_speed_1_does_not_touch_the_game(self):
        res, w = self.go(x=1.0)
        self.assertTrue(res.ok, res.reason)
        self.assertEqual(w.speed_log, [[], []])
        self.assertNotIn("timescale", res.to_json())

    def test_a_failed_step_still_puts_the_speed_back(self):
        e = registry.REGISTRY["fight"]
        w, refs = e.fake(2, {"stale_echo"})
        res = run_pb(registry.make("fight"), w, refs, opts={"timescale": 4.0})
        self.assertFalse(res.ok)
        self.assertEqual(w.speed, [1.0, 1.0])

    def test_a_machine_that_refuses_stops_the_run_before_any_step(self):
        e = registry.REGISTRY["fight"]
        w, refs = e.fake(2)

        class Refuses(fakecoop.FakeAdapter):
            def set_timescale(self, x):
                return Result(x == 1.0, "" if x == 1.0 else "the console refused it")
        w.bots[1] = Refuses(w, 1)
        res = run_pb(registry.make("fight"), w, refs, opts={"timescale": 3.0})
        self.assertFalse(res.ok)
        self.assertEqual([s.kind for s in res.steps], ["timescale"])
        self.assertIn("client1", res.reason)
        self.assertIn("the console refused it", res.reason)
        self.assertEqual(w.speed[0], 1.0, "the copies that took it were put back")

    def test_twin_a_copy_that_accepts_but_does_not_change_speed_is_caught(self):
        res, _ = self.go(bugs={"timescale_ignored"})
        self.assertFalse(res.ok)
        self.assertEqual(res.steps[0].kind, "timescale")
        self.assertIn("reports 1 instead of 3", res.reason)

    def test_twin_a_copy_that_stays_fast_after_the_run_fails_the_run(self):
        res, w = self.go(bugs={"speed_not_reset"})
        self.assertFalse(res.ok)
        self.assertIn("could not put the game speed back to 1", res.reason)

    def test_an_adapter_without_set_timescale_is_told_so_not_skipped(self):
        w, refs = registry.REGISTRY["fight"].fake(2)

        class NoSpeed(fakecoop.FakeAdapter):
            set_timescale = ActionAdapter.set_timescale
        w.bots[0] = NoSpeed(w, 0)
        res = run_pb(registry.make("fight"), w, refs, opts={"timescale": 2.0})
        self.assertEqual(res.status, "fail")
        self.assertIn("needs the adapter action set_timescale", res.reason)

    def test_a_fixed_pause_shrinks_by_the_timescale(self):
        w, refs = registry.REGISTRY["fight"].fake(2)
        for x, longer in ((1.0, True), (10.0, False)):
            ctx = core.Ctx(fakecoop.make_machines(w), None, tempfile.mkdtemp(), refs, {"fast": False, "timescale": x}, "t")
            t0 = time.monotonic()
            ctx.pause(0.5)
            took = time.monotonic() - t0
            self.assertTrue(took >= 0.45 if longer else took < 0.25, "pause(0.5) at x%g took %.2fs" % (x, took))

    def test_cli_timescale(self):
        code, out = run_cli("--fake", "--playbook", "fight", "--timescale", "3", "--outdir", tempfile.mkdtemp())
        self.assertEqual(code, 0, out)
        self.assertEqual(run_cli("--fake", "--playbook", "fight", "--timescale", "0")[0], 2)
        d = tempfile.mkdtemp()
        run_cli("--fake", "--playbook", "fight", "--timescale", "2", "--outdir", d)
        with open(os.path.join(d, "fight", "result.json"), encoding="utf-8") as f:
            info = json.load(f)["timescale"]
        self.assertEqual((info["requested"], info["set"], info["reset"]), (2.0, {"host": "", "client1": ""}, {"host": "", "client1": ""}))


if __name__ == "__main__":
    unittest.main()
