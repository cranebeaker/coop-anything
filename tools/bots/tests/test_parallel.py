"""The parallel pair runner (tools/bots/run_parallel.py): the plan, the checks, the table, and real runs on the fake world."""
import io
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest

import support
from support import HERE

sys.path.insert(0, HERE)
import run_parallel as rp  # noqa: E402


class FakeProc:
    """A stand-in for a runner process: some output lines, then an exit code (or never, until killed)."""

    def __init__(self, lines, rc, hang=False):
        self.stdout = iter(l + "\n" for l in lines)
        self.rc, self.hang, self.killed, self.polls = rc, hang, False, 0

    def poll(self):
        self.polls += 1
        if self.hang and not self.killed and self.polls < 1500:      # a hung runner (it gives up after a while so a broken tool cannot hang the tests)
            return None
        return -9 if self.killed else self.rc

    def kill(self):
        self.killed = True

    def wait(self):
        return -9


def outcome(lines, rc, hang=False, summary=None, timeout_s=0.0):
    d = tempfile.mkdtemp(prefix="coopbots-")
    plan = rp.PairPlan(0, [0, 1], 0, d, ["x"])
    if summary is not None:
        with open(os.path.join(d, "summary.json"), "w", encoding="utf-8") as f:
            json.dump(summary, f)
    runs = rp.run_all([plan], out=io.StringIO(), timeout_s=timeout_s, spawn=lambda cmd, env: FakeProc(lines, rc, hang), poll_s=0.0)
    return runs[0]


def write_runner(body):
    d = tempfile.mkdtemp(prefix="coopbots-")
    p = os.path.join(d, "runner.py")
    with open(p, "w", encoding="utf-8") as f:
        f.write(body)
    return p


class Plan(unittest.TestCase):
    def test_pair_k_gets_its_own_instances_port_and_folder(self):
        plans = rp.plan_pairs([0, 1, 2], "run_coop.py", "out", 27960, 2, ["--playbook", "fight"], False)
        self.assertEqual([p.ids for p in plans], ["0,1", "2,3", "4,5"])
        self.assertEqual([p.port for p in plans], [27960, 27961, 27962])
        self.assertEqual([os.path.basename(p.outdir) for p in plans], ["pair0", "pair1", "pair2"])
        c = plans[1].cmd
        self.assertEqual(c[c.index("--instances") + 1], "2,3")
        self.assertEqual(c[c.index("--outdir") + 1], os.path.join("out", "pair1"))
        self.assertEqual(c[-2:], ["--playbook", "fight"])
        self.assertNotIn("--fake", c)
        self.assertEqual(rp.plan_problems(plans), [])

    def test_players_per_pair_and_only(self):
        plans = rp.plan_pairs([1, 3], "r.py", "out", 0, 3, [], True)
        self.assertEqual([p.ids for p in plans], ["3,4,5", "9,10,11"])
        self.assertIn("--fake", plans[0].cmd)
        self.assertEqual(plans[0].port, 0)
        self.assertEqual(rp.parse_pair_list("1,3"), [1, 3])
        with self.assertRaises(ValueError):
            rp.parse_pair_list("1,1")

    def test_twin_two_pairs_on_one_instance_port_or_folder_are_refused(self):
        a = rp.PairPlan(0, [0, 1], 27960, "out/a", ["x"])
        b = rp.PairPlan(1, [1, 2], 27960, "out/a", ["x"])
        text = " | ".join(rp.plan_problems([a, b]))
        self.assertIn("same co-op port", text)
        self.assertIn("same output folder", text)
        self.assertIn("both use instance 1", text)

    def test_options_that_belong_to_this_tool_are_refused_after_the_double_dash(self):
        self.assertEqual(len(rp.passthrough_problems(["--playbook", "fight", "--instances", "0,1", "--outdir=x", "--players", "3"])), 3)
        self.assertEqual(rp.passthrough_problems(["--playbook", "fight", "--from-step", "4"]), [])

    def test_main_refuses_with_exit_2(self):
        err = io.StringIO()
        old, sys.stderr = sys.stderr, err
        try:
            self.assertEqual(rp.main(["--pairs", "2", "--fake", "--outdir", tempfile.mkdtemp(prefix="coopbots-"), "--", "--instances", "0,1"],
                                     out=io.StringIO()), 2)
            self.assertEqual(rp.main(["--fake"], out=io.StringIO()), 2)
            self.assertEqual(rp.main(["--pairs", "2", "--runner", "no_such_file.py"], out=io.StringIO()), 2)
            self.assertEqual(rp.main(["--pairs", "2", "--players-per-pair", "5"], out=io.StringIO()), 2)
        finally:
            sys.stderr = old
        self.assertIn("--instances after --", err.getvalue())

    def test_the_preflight_hook_can_refuse(self):
        d = tempfile.mkdtemp(prefix="coopbots-")
        with open(os.path.join(d, "my_checks.py"), "w", encoding="utf-8") as f:
            f.write("def check(plans):\n    return ['instance %d is leased by another job' % plans[1].host]\n")
        old_cwd, old_err = os.getcwd(), sys.stderr
        os.chdir(d)
        sys.stderr = io.StringIO()
        try:
            code = rp.main(["--pairs", "2", "--fake", "--preflight", "my_checks:check", "--outdir", os.path.join(d, "o")], out=io.StringIO())
            text = sys.stderr.getvalue()
        finally:
            os.chdir(old_cwd)
            sys.stderr = old_err
        self.assertEqual(code, 2)
        self.assertIn("refusing to start", text)
        self.assertIn("instance 2 is leased", text)

    def test_dry_run_starts_nothing(self):
        out = io.StringIO()
        d = tempfile.mkdtemp(prefix="coopbots-")
        self.assertEqual(rp.main(["--pairs", "3", "--fake", "--dry-run", "--outdir", d, "--", "--playbook", "fight"], out=out), 0)
        text = out.getvalue()
        self.assertIn("pair2: instances 4,5", text)
        self.assertIn("dry run: checks passed, nothing started", text)
        self.assertFalse(os.path.exists(os.path.join(d, "parallel_summary.json")))


class Classify(unittest.TestCase):
    def test_exit_codes_become_statuses(self):
        self.assertEqual(outcome(["PASS  fight  3 steps"], 0).status, "PASS")
        self.assertEqual(outcome(["x"], 2).status, "SETUP")
        self.assertEqual(outcome(["x"], 3).status, "BLOCKED")
        self.assertEqual(outcome(["Traceback (most recent call last):", "boom"], 139).status, "CRASH")

    def test_a_failure_gives_the_reason_from_the_summary_file(self):
        r = outcome(["noise"], 1, summary={"ok": False, "results": [{"playbook": "fight", "status": "fail", "reason": "stuck: 300 units left"},
                                                                       {"playbook": "loot_race", "status": "pass", "reason": ""}]})
        self.assertEqual((r.status, r.reasons), ("FAIL", ["fight: stuck: 300 units left"]))

    def test_a_failure_without_a_summary_reads_the_fail_line(self):
        r = outcome(["[fight] step 01", "FAIL  fight  2 steps  raider_1 is still alive"], 1)
        self.assertEqual(r.reasons, ["fight: raider_1 is still alive"])

    def test_exit_0_with_a_failed_summary_is_still_a_fail(self):
        r = outcome([], 0, summary={"ok": False, "results": []})
        self.assertEqual(r.status, "FAIL")

    def test_a_hung_runner_times_out_and_only_that_process_is_stopped(self):
        holder = {}

        def spawn(cmd, env):
            holder["p"] = FakeProc(["working"], 0, hang=True)
            return holder["p"]
        plan = rp.PairPlan(0, [0, 1], 0, tempfile.mkdtemp(prefix="coopbots-"), ["x"])
        runs = rp.run_all([plan], out=io.StringIO(), timeout_s=0.2, spawn=spawn, poll_s=0.001)
        self.assertEqual(runs[0].status, "TIMEOUT")
        self.assertTrue(holder["p"].killed)

    def test_the_combined_exit_code(self):
        def run(status):
            r = rp.PairRun(rp.PairPlan(0, [0, 1], 0, "o", []))
            r.status = status
            return r
        self.assertEqual(rp.combine_exit([run("PASS"), run("PASS")]), 0)
        self.assertEqual(rp.combine_exit([run("PASS"), run("FAIL")]), 1)
        self.assertEqual(rp.combine_exit([run("PASS"), run("TIMEOUT")]), 1)
        self.assertEqual(rp.combine_exit([run("PASS"), run("CRASH")]), 1)
        self.assertEqual(rp.combine_exit([run("PASS"), run("BLOCKED")]), 3)
        self.assertEqual(rp.combine_exit([run("PASS"), run("SETUP")]), 2)
        self.assertEqual(rp.combine_exit([run("BLOCKED"), run("FAIL")]), 1)
        self.assertEqual(rp.combine_exit([]), 1)


class RealRunsOnTheFakeWorld(unittest.TestCase):
    def go(self, *args):
        out = io.StringIO()
        d = tempfile.mkdtemp(prefix="coopbots-")
        code = rp.main(["--outdir", d, *args], out=out)
        return code, out.getvalue(), d

    def test_three_pairs_pass_with_prefixed_logs_and_one_table(self):
        code, text, d = self.go("--pairs", "3", "--fake", "--", "--playbook", "loot_race")
        self.assertEqual(code, 0, text[-1500:])
        for k in range(3):
            self.assertIn("[pair%d] [loot_race] step 01" % k, text)
            self.assertTrue(os.path.isfile(os.path.join(d, "pair%d" % k, "summary.json")))
        self.assertEqual(sum(1 for l in text.splitlines() if l.startswith("pair") and " PASS " in l), 3)
        self.assertIn("3/3 pairs passed", text)
        with open(os.path.join(d, "parallel_summary.json"), encoding="utf-8") as f:
            summ = json.load(f)
        self.assertTrue(summ["ok"])
        self.assertEqual([p["instances"] for p in summ["pairs"]], [[0, 1], [2, 3], [4, 5]])

    def test_every_log_line_of_a_pair_carries_its_prefix(self):
        code, text, _ = self.go("--pairs", "2", "--fake", "--", "--playbook", "fight")
        before_table = text.split("PAIR   INSTANCES")[0]
        lines = [l for l in before_table.splitlines() if l.strip()]
        self.assertTrue(lines)
        self.assertTrue(all(l.startswith("[pair0] ") or l.startswith("[pair1] ") or l.startswith("pair") for l in lines),
                        [l for l in lines if not l.startswith("[pair")][:3])

    def test_a_bug_in_one_world_fails_the_table_with_the_reason(self):
        code, text, d = self.go("--pairs", "2", "--fake", "--", "--playbook", "loot_race", "--bug", "dup_pickup")
        self.assertEqual(code, 1, text[-1500:])
        self.assertIn("0/2 pairs passed", text)
        self.assertIn("pair1 (FAIL): loot_race:", text)
        self.assertIn("copies", text)

    def test_only_reruns_chosen_pairs(self):
        code, text, d = self.go("--only", "2", "--fake", "--", "--playbook", "quest_sync")
        self.assertEqual(code, 0, text)
        self.assertIn("pair2", text)
        self.assertNotIn("pair0", text)

    def test_the_runner_finds_its_pair_in_the_environment(self):
        runner = write_runner("import os, sys\nprint('PAIR=%s INST=%s PORT=%s' % (os.environ['COOP_PAIR'], os.environ['COOP_INSTANCES'], "
                              "os.environ.get('COOP_PORT')))\nprint('ARGS=%s' % ' '.join(sys.argv[1:]))\n")
        code, text, _ = self.go("--pairs", "2", "--runner", runner, "--port-base", "28000", "--", "--hello")
        self.assertEqual(code, 0, text)
        self.assertIn("[pair1] PAIR=1 INST=2,3 PORT=28001", text)
        self.assertIn("--instances 2,3", text)
        self.assertIn("--hello", text)

    def test_runner_exit_codes_reach_the_combined_exit_code(self):
        runner = write_runner("import sys\nsys.exit(3 if '--instances' in sys.argv and sys.argv[sys.argv.index('--instances') + 1] == '2,3' else 0)\n")
        code, text, _ = self.go("--pairs", "2", "--runner", runner)
        self.assertEqual(code, 3, text)
        self.assertIn("BLOCKED", text)

    def test_a_hung_runner_is_stopped_at_the_timeout(self):
        runner = write_runner("import time\nprint('start', flush=True)\ntime.sleep(4)\n")
        t0 = time.monotonic()
        code, text, _ = self.go("--pairs", "1", "--runner", runner, "--timeout", "1")
        self.assertEqual(code, 1, text)
        self.assertIn("TIMEOUT", text)
        self.assertLess(time.monotonic() - t0, 3.8)


if __name__ == "__main__":
    unittest.main()
