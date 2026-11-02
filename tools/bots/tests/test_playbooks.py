"""Game-free tests for the playbooks, with mutation twins (every twin must make its playbook FAIL).

What is proven:
  * every playbook passes on a correct fake co-op world with 2, 3 and 4 players (where it allows) and on a laggy one,
  * every twin (a playbook plus a deliberately wrong fake behaviour) FAILS, with a reason that names the problem,
  * a rising desync counter fails every playbook,
  * a playbook whose adapter lacks an action is SKIPPED, never passed,
  * impaired links: quiet windows, repairs budget, a message lost for good,
  * a failed step saves the evidence files.
"""
import json
import os
import shutil
import tempfile
import unittest

import support
from support import OPTS, core, fakecoop, links, registry, run, run_pb
from coopbots.adapter import ACTIONS, QUERIES, ActionAdapter


class CleanRuns(unittest.TestCase):
    def test_every_playbook_passes_with_its_default_players(self):
        for name in registry.REGISTRY:
            with self.subTest(name):
                r, _ = run(name)
                self.assertTrue(r.ok, "%s: %s" % (name, r.reason))
                self.assertEqual(r.status, "pass")
                self.assertGreaterEqual(len(r.steps), 3, "a playbook with fewer than 3 steps proves little")

    def test_two_three_four_players(self):
        for name in registry.REGISTRY:
            pb = registry.make(name)
            for n in (2, 3, 4):
                if not pb.min_players <= n <= pb.max_players:
                    continue
                with self.subTest("%s x%d" % (name, n)):
                    r, _ = run(name, n=n)
                    self.assertTrue(r.ok, "%s with %d players: %s" % (name, n, r.reason))

    def test_laggy_replication(self):
        for name in ("fight", "consent_vote", "container_share", "quest_sync"):
            with self.subTest(name):
                r, _ = run(name, lag=4)
                self.assertTrue(r.ok, "%s: %s" % (name, r.reason))

    def test_wrong_player_count_is_refused_not_crashed(self):
        w, refs = registry.REGISTRY["fight"].fake(1)
        res = run_pb(registry.make("fight"), w, refs)
        self.assertFalse(res.ok)
        self.assertIn("needs 2 to 4 players", res.reason)

    def test_missing_ref_is_a_clear_failure(self):
        w, refs = registry.REGISTRY["loot_race"].fake(2)
        refs.pop("item")
        res = run_pb(registry.make("loot_race"), w, refs)
        self.assertFalse(res.ok)
        self.assertIn("missing refs", res.reason)

    def test_registry_is_consistent(self):
        known = {**ACTIONS, **QUERIES}
        for name, e in registry.REGISTRY.items():
            pb = registry.make(name)
            self.assertEqual(pb.name, name)
            self.assertTrue(pb.summary)
            for req in pb.requires:
                self.assertIn(req, known, "%s requires an unknown adapter method %s" % (name, req))
            self.assertTrue(FakeCoopHasAll(e.players), name)


def FakeCoopHasAll(n):
    bot = fakecoop.FakeAdapter(fakecoop.FakeCoop(n), 0)
    return all(bot.supports(x) for x in list(ACTIONS) + list(QUERIES) + ["status", "snapshot"])


class Skips(unittest.TestCase):
    def test_missing_action_is_a_skip_not_a_pass(self):
        w, refs = registry.REGISTRY["loot_race"].fake(2)

        class NoDrop(fakecoop.FakeAdapter):
            drop = ActionAdapter.drop
        w.bots[1] = NoDrop(w, 1)
        res = run_pb(registry.make("loot_race"), w, refs)
        self.assertEqual(res.status, "skip")
        self.assertFalse(res.ok)
        self.assertIn("drop", res.reason)
        self.assertEqual(res.steps, [], "a skipped playbook must not have run anything")

    def test_supports_tells_overridden_from_missing(self):
        class Tiny(ActionAdapter):
            def cell(self):
                return "x"
        t = Tiny()
        self.assertTrue(t.supports("cell"))
        self.assertFalse(t.supports("pickup"))
        self.assertFalse(t.supports("status"))
        self.assertTrue(fakecoop.FakeAdapter(fakecoop.FakeCoop(2), 0).supports("pickup"))

    def test_a_step_that_uses_a_missing_action_fails_with_a_hint(self):
        class Needy(core.Playbook):
            name = "needy"

            def run(self, ctx):
                with ctx.step("uses an action nobody declared"):
                    ctx.host.bot.pickup("x")
        w, refs = registry.REGISTRY["fight"].fake(2)

        class Half(ActionAdapter):
            """Only the engine surface; every action is missing."""

            def __init__(self, inner):
                self.inner, self.name = inner, inner.name

            def status(self):
                return self.inner.status()

            def snapshot(self):
                return self.inner.snapshot()
        w.bots[0] = Half(w.bots[0])
        res = run_pb(Needy(), w, refs)
        self.assertFalse(res.ok)
        self.assertIn("does not implement pickup", res.reason)

    def test_optional_feature_missing_is_a_note_not_a_failure(self):
        w, refs = registry.REGISTRY["fight"].fake(2)

        class NoCount(fakecoop.FakeAdapter):
            ref_count = ActionAdapter.ref_count
        for i in range(2):
            w.bots[i] = NoCount(w, i)
        res = run_pb(registry.make("fight"), w, refs)
        self.assertTrue(res.ok, res.reason)


class Twins(unittest.TestCase):
    def test_every_twin_is_caught(self):
        for name, bug in registry.TWINS:
            with self.subTest("%s + %s" % (name, bug)):
                clean, _ = run(name)
                self.assertTrue(clean.ok, "clean %s must pass first: %s" % (name, clean.reason))
                bad, _ = run(name, bugs={bug})
                self.assertFalse(bad.ok, "twin %s was NOT caught by %s" % (bug, name))
                self.assertTrue(bad.reason, "a failure needs a reason")

    def test_every_fake_bug_has_a_twin_or_is_universal(self):
        covered = {b for _, b in registry.TWINS} | registry.UNIVERSAL
        self.assertEqual(set(fakecoop.BUGS) - covered, set())
        self.assertEqual(covered - set(fakecoop.BUGS), set(), "a twin names a bug the fake does not have")

    def test_rising_counters_fail_every_playbook(self):
        for name in registry.REGISTRY:
            with self.subTest(name):
                bad, _ = run(name, bugs={"counter_noise"})
                self.assertFalse(bad.ok, "%s ignored a rising desync counter" % name)
                self.assertIn("desync counters rose", bad.reason)

    def test_specific_reasons(self):
        self.assertIn("copies", run("loot_race", bugs={"dup_pickup"})[0].reason)
        self.assertRegex(run("door_lock", bugs={"door_unsynced"})[0].reason, "door open|snapshots differ")
        self.assertIn("stage", run("consent_vote", bugs={"barrier_early"})[0].reason)
        self.assertIn("tether", run("fast_travel_tether", bugs={"tether_off"})[0].reason)
        self.assertIn("flapped", run("ownership_handoff", bugs={"flap_owner"})[0].reason)
        self.assertIn("lock", run("container_share", bugs={"no_container_lock"})[0].reason)
        self.assertIn("consent", run("fast_travel_tether", bugs={"consent_ignored"})[0].reason)
        self.assertRegex(run("save_load_rejoin", bugs={"no_rebase"})[0].reason, "rebase|follow the host back")

    def test_snapshot_comparison_alone_catches_what_no_query_asks(self):
        class Quiet(core.Playbook):
            name = "quiet"

            def run(self, ctx):
                with ctx.step("host spawns and nobody asks about it"):
                    ctx.must(ctx.host.bot.spawn("enemy_type", "spawn_point"), "host spawns")
        w, refs = registry.REGISTRY["spawn_once"].fake(2)
        self.assertTrue(run_pb(Quiet(), w, refs).ok)
        w, refs = registry.REGISTRY["spawn_once"].fake(2, {"double_spawn"})
        res = run_pb(Quiet(), w, refs)
        self.assertFalse(res.ok)
        self.assertEqual(res.steps[-1].kind, "snapshot")
        self.assertEqual(res.steps[-1].snapshot, "different")


class ImpairedLinks(unittest.TestCase):
    LOSS = 1     # every first delivery is lost once, so even single-commit playbooks see drops

    def test_clean_playbooks_pass_on_a_lossy_link(self):
        for name in registry.REGISTRY:
            with self.subTest(name):
                r, w = run(name, opts=support.lossy_opts(), loss_every=self.LOSS)
                self.assertTrue(r.ok, "%s: %s" % (name, r.reason))
                self.assertGreater(w.n_deliv // self.LOSS, 0, "the link never dropped anything: the test proves nothing")
                self.assertGreater(sum(w.stats["mismatch"]), 0, "no transient mismatch was produced")

    def test_a_message_lost_for_good_fails(self):
        for name in ("loot_race", "quest_sync", "container_share"):
            with self.subTest(name):
                bad, _ = run(name, bugs={"no_retransmit"}, opts=support.lossy_opts(), loss_every=self.LOSS)
                self.assertFalse(bad.ok, "%s accepted a message that was lost for good" % name)
                self.assertRegex(bad.reason, "converge|stage|differ|snapshot|timed out")

    def test_late_repair_in_the_quiet_window_fails(self):
        bad, _ = run("loot_race", bugs={"repair_after_quiet"}, opts=support.lossy_opts(), loss_every=self.LOSS)
        self.assertFalse(bad.ok)
        self.assertIn("quiet window", bad.reason)

    def test_repairs_over_budget_fail_and_loosening_is_explicit(self):
        bad, _ = run("loot_race", bugs={"repairs_in_flight"}, opts=support.lossy_opts(), loss_every=self.LOSS)
        self.assertFalse(bad.ok)
        self.assertIn("budget", bad.reason)
        ok, _ = run("loot_race", bugs={"repairs_in_flight"}, opts=support.lossy_opts(repair_budget_per_min=10000.0), loss_every=self.LOSS)
        self.assertTrue(ok.ok, ok.reason)

    def test_budget_in_the_profiles_file_needs_a_reason(self):
        d = tempfile.mkdtemp()
        path = os.path.join(d, "profiles.json")
        with open(path, "w") as f:
            json.dump({"bad_wifi": {"repair_budget_per_min": 2}}, f)
        with self.assertRaises(ValueError):
            links.repair_budget(path, "bad_wifi")
        with open(path, "w") as f:
            json.dump({"bad_wifi": {"repair_budget_per_min": 2, "repair_budget_reason": "known reorder bug"}}, f)
        self.assertEqual(links.repair_budget(path, "bad_wifi"), 2.0)
        with open(path, "w") as f:
            json.dump({"bad_wifi": {}}, f)
        self.assertEqual(links.repair_budget(path, "bad_wifi"), 0.0)
        self.assertEqual(links.repair_budget(os.path.join(d, "missing.json"), "bad_wifi"), 0.0)
        self.assertEqual(links.repair_budget(None, "bad_wifi"), 0.0)

    def test_hard_errors_fail_immediately_on_every_link(self):
        for k in ("bad_messages", "apply_failures", "rebases"):
            self.assertIn(k, core.MUST_NOT_RISE)
            self.assertNotIn(k, core.REPAIR_COUNTERS)

    def test_link_options(self):
        self.assertEqual(links.link_options(None), {})
        self.assertEqual(links.link_options("lan"), {})
        o = links.link_options("hell")
        self.assertFalse(o["strict_counters"])
        self.assertGreater(o["quiesce_s"], links.link_options("bad_wifi")["quiesce_s"])
        with self.assertRaises(ValueError):
            links.link_options("dialup")
        prof = links.default_profiles_path()
        if prof:
            for p in links.PROFILES:
                self.assertGreaterEqual(links.link_options(p, prof).get("repair_budget_per_min", 0.0), 0.0)


class Engine(unittest.TestCase):
    def test_settle_timeout_names_the_machines(self):
        r, _ = run("loot_race", lag=10 ** 6, opts={"settle_s": 0.05, "settle_poll_s": 0.01})
        self.assertFalse(r.ok)
        self.assertIn("did not converge", r.reason)

    def test_failure_files(self):
        d = tempfile.mkdtemp(prefix="coopbots-fail-")
        run("loot_race", bugs={"dup_pickup"}, outdir=d)
        fails = [x for x in os.listdir(d) if x.startswith("FAIL-")]
        self.assertEqual(len(fails), 1, os.listdir(d))
        fd = os.path.join(d, fails[0])
        with open(os.path.join(fd, "failure.json"), encoding="utf-8") as f:
            info = json.load(f)
        self.assertEqual(info["playbook"], "loot_race")
        self.assertEqual([m["name"] for m in info["machines"]], ["host", "client1"])
        for m in info["machines"]:
            self.assertIn("status", m)
            self.assertTrue(os.path.isfile(m["snapshot"]), m)
            self.assertTrue(os.path.isfile(m["capture"]["actions"]), m)
        self.assertTrue(os.path.isfile(os.path.join(d, "result.json")))
        shutil.rmtree(d, ignore_errors=True)

    def test_missing_capture_is_recorded_not_fatal(self):
        d = tempfile.mkdtemp(prefix="coopbots-fail-")
        w, refs = registry.REGISTRY["loot_race"].fake(2, {"dup_pickup"})

        class NoCapture(fakecoop.FakeAdapter):
            def capture(self, directory):
                raise RuntimeError("no dump available")
        w.bots[0] = NoCapture(w, 0)
        run_pb(registry.make("loot_race"), w, refs, outdir=d)
        fd = os.path.join(d, [x for x in os.listdir(d) if x.startswith("FAIL-")][0])
        with open(os.path.join(fd, "failure.json"), encoding="utf-8") as f:
            info = json.load(f)
        self.assertIn("capture_error", info["machines"][0])

    def test_bot_crash_is_a_failed_step_with_a_trace(self):
        w, refs = registry.REGISTRY["loot_race"].fake(2)

        def boom(*a, **k):
            raise RuntimeError("connection exploded")
        w.bots[1].pickup = boom
        d = tempfile.mkdtemp()
        r = run_pb(registry.make("loot_race"), w, refs, outdir=d)
        self.assertFalse(r.ok)
        self.assertIn("connection exploded", r.reason)
        self.assertTrue([x for x in os.listdir(d) if x.startswith("FAIL-")])

    def test_steps_are_recorded_with_timing_and_snapshot_result(self):
        r, _ = run("container_share")
        self.assertTrue(all(s.ok for s in r.steps))
        self.assertIn("equal", {s.snapshot for s in r.steps})
        with open(os.path.join(r.outdir, "result.json"), encoding="utf-8") as f:
            self.assertTrue(json.load(f)["ok"])
        self.assertTrue(os.path.isdir(os.path.join(r.outdir, "snap")))

    def test_playbook_with_no_steps_is_not_a_pass(self):
        class Empty(core.Playbook):
            name = "empty"

            def run(self, ctx):
                pass
        w, refs = registry.REGISTRY["fight"].fake(2)
        r = run_pb(Empty(), w, refs)
        self.assertFalse(r.ok)
        self.assertIn("no steps", r.reason)

    def test_vacuous_snapshot_is_refused(self):
        c = core.DictComparer()
        self.assertEqual(c.diff({"shared": {"a": 1}}, {"shared": {"a": 1}}, [])[0], 0)
        self.assertEqual(c.diff({"shared": {"a": 1}}, {"shared": {"a": 2}}, [])[0], 1)
        self.assertEqual(c.diff({"shared": {"a": 1}}, {"shared": {}}, [])[0], 2)
        self.assertEqual(c.diff({}, {}, [])[0], 2)

    def test_ignore_patterns_hide_only_what_they_name(self):
        c = core.DictComparer()
        a, b = {"shared": {"cam": 1, "hp": 5}}, {"shared": {"cam": 2, "hp": 5}}
        self.assertEqual(c.diff(a, b, ["shared/cam*"])[0], 0)
        self.assertEqual(c.diff({"shared": {"cam": 1, "hp": 5}}, {"shared": {"cam": 2, "hp": 6}}, ["shared/cam*"])[0], 1)

    def test_parallel_actions_really_overlap(self):
        import threading
        w, refs = registry.REGISTRY["loot_race"].fake(3)
        ctx = core.Ctx(fakecoop.make_machines(w), None, tempfile.mkdtemp(), refs, OPTS)
        gate = threading.Barrier(3, timeout=5)
        out = ctx.on_all(lambda m: gate.wait() >= 0)       # only returns if all three run at the same time
        self.assertEqual(sorted(out), ["client1", "client2", "host"])
        ctx.close()

    def test_same_everywhere_reports_the_differing_machines(self):
        w, refs = registry.REGISTRY["loot_race"].fake(2)
        ctx = core.Ctx(fakecoop.make_machines(w), None, tempfile.mkdtemp(), refs, OPTS)
        with self.assertRaises(core.StepFailure) as cm:
            ctx.same_everywhere("answer", lambda m: m.idx)
        self.assertIn("differs between machines", cm.exception.reason)


if __name__ == "__main__":
    unittest.main()
