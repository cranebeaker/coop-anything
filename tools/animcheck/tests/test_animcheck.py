"""Game-free tests of animcheck on synthetic probe streams. Every detector has a twin (ANIMCHECK_MUTANT=<kind>) that
run_tests.py requires to FAIL this suite."""
from __future__ import annotations

import json
import math
import os
import random
import socketserver
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from animcheck import detectors, synth  # noqa: E402
from animcheck.analyze import analyze  # noqa: E402
from animcheck.collect import Collector, TcpBridge  # noqa: E402
from animcheck.compare import angdiff, local_bones, percentile  # noqa: E402
from animcheck.config import DEFAULT  # noqa: E402
from animcheck.counters import check  # noqa: E402
from animcheck.model import parse_actor, read_probes, stream_from_probes, write_probes  # noqa: E402
from animcheck.synth import G, NPC_REF, State, make_probes  # noqa: E402

PKG = Path(__file__).resolve().parent.parent
CLI = str(PKG / "cli.py")
DUR = 6000.0


def one(machine, script, dur=DUR, **kw):
    return stream_from_probes(machine, make_probes(machine, {NPC_REF: script}, dur, **kw), use_rx=True)


def events(script, dur=DUR, expect=(), **kw):
    st = one("host", script, dur, **kw)
    return detectors.run_all(st.by_ref[NPC_REF], DEFAULT, expect)


def kinds(evs):
    return sorted({e.kind for e in evs})


def cli_env():
    env = {k: v for k, v in os.environ.items() if k != "ANIMCHECK_MUTANT"}
    env["ANIMCHECK_MUTANT"] = os.environ.get("ANIMCHECK_MUTANT", "")
    return env


def run_cli(*args):
    return subprocess.run([sys.executable, "-I", CLI, *args], capture_output=True, text=True, env=cli_env())


class CleanTests(unittest.TestCase):
    def test_walk_run_idle_are_clean(self):
        self.assertEqual(events(synth.walker(120.0)), [])
        self.assertEqual(events(synth.walker(320.0)), [])
        self.assertEqual(events(synth.idler()), [])

    def test_noise_does_not_trigger(self):
        sc = synth.noisy(synth.walker(120.0), random.Random(3))
        self.assertEqual(events(sc), [])

    def test_jump_and_fall_are_not_floating(self):
        def jump(t):
            s = synth.walker(120.0)(t)
            if 2000 <= t < 3200:
                k = (t - 2000) / 1200.0
                s.pos = (s.pos[0], s.pos[1], s.pos[2] + 120 * 4 * k * (1 - k))
                s.groups = [(1, G["jump"])]
            return s
        self.assertEqual(events(jump), [])

    def test_teleport_with_cell_change_is_not_rubberband(self):
        def tele(t):
            s = synth.idler()(t)
            if t >= 3000:
                s.pos = (s.pos[0] + 5000, s.pos[1], s.pos[2] + 300)
                s.cell = 0x00000BEEF
            return s
        self.assertEqual(events(tele), [])

    def test_swimming_and_knockdown_are_not_judged(self):
        def sw(t):
            s = synth.idler()(t)
            if 2000 <= t < 5000:
                k = min(1.0, (t - 2000) / 1000.0, (5000 - t) / 1000.0)   # wade in over a second, wade out over a second
                s.swimming = True
                s.pos = (s.pos[0], s.pos[1], s.pos[2] - 60 * k)
            return s
        self.assertEqual(events(sw), [])

    def test_proper_death_is_clean(self):
        self.assertEqual(events(synth.proper_death(synth.walker(120.0), 2000.0), 9000.0), [])

    def test_first_person_flag_does_not_hide_the_third_person_body(self):
        probes = make_probes("host", {NPC_REF: lambda t: State(pos=(0, 0, 0), groups=[])}, 3000.0)
        for p in probes:
            p["actors"][0]["first_person"] = True
        st = stream_from_probes("host", probes, use_rx=True)
        self.assertEqual(kinds(detectors.run_all(st.by_ref[NPC_REF], DEFAULT)), ["tpose"])


class GlitchTests(unittest.TestCase):
    def test_sliding_moves_without_locomotion(self):
        evs = events(synth.inject_sliding(synth.idler(), 1000, 2500))
        self.assertIn("sliding", kinds(evs))
        self.assertEqual(kinds(evs), ["sliding"])
        e = [x for x in evs if x.kind == "sliding"][0]
        self.assertTrue(900 <= e.t0 <= 1300 and e.t1 <= 2600, (e.t0, e.t1))

    def test_sliding_speed_outside_animation_band(self):
        def too_fast(t):
            s = synth.walker(120.0)(t)
            if t >= 1000:
                s.pos = (s.pos[0] + 700.0 * (t - 1000) / 1000.0, s.pos[1], s.pos[2])
            return s
        self.assertIn("sliding", kinds(events(too_fast, 3000.0)))

    def test_tpose_no_sequence(self):
        evs = events(synth.inject_tpose(synth.idler(), 1000, 2000))
        self.assertEqual(kinds(evs), ["tpose"])

    def test_tpose_arms_out(self):
        evs = events(synth.inject_tpose(synth.idler(), 1000, 2000, mode="arms"))
        self.assertEqual(kinds(evs), ["tpose"])

    def test_floating(self):
        evs = events(synth.inject_float(synth.idler(), 1500, 3500, 90.0))
        self.assertEqual(kinds(evs), ["floating"])

    def test_sinking(self):
        evs = events(synth.inject_float(synth.idler(), 1500, 3500, -90.0))
        self.assertEqual(kinds(evs), ["sinking"])

    def test_jitter(self):
        evs = events(synth.inject_jitter(synth.idler(), 1000, 2500, 4.0))
        self.assertEqual(kinds(evs), ["jitter"])

    def test_rubberband_with_snapback(self):
        evs = events(synth.inject_rubber(synth.walker(120.0), 2000, 400.0, 200.0))
        rb = [e for e in evs if e.kind == "rubberband"]
        self.assertGreaterEqual(len(rb), 1)
        self.assertIn("snapped back", rb[0].detail)
        self.assertEqual(kinds(evs), ["rubberband"])

    def test_stuck_only_when_movement_is_expected(self):
        self.assertEqual(events(synth.idler()), [])
        evs = events(synth.idler(), expect=[(1000.0, 5000.0)])
        self.assertEqual(kinds(evs), ["stuck"])
        self.assertEqual(events(synth.walker(120.0), expect=[(1000.0, 5000.0)]), [])

    def test_deathpose_alive_animation_on_dead_actor(self):
        evs = events(synth.inject_death_alive(synth.idler(), 1000.0), 8000.0)
        self.assertEqual(kinds(evs), ["deathpose"])

    def test_deathpose_death_animation_on_live_actor(self):
        def wrong(t):
            s = synth.idler()(t)
            if t >= 1000:
                s.groups = [(0, G["death"])]
            return s
        self.assertEqual(kinds(events(wrong, 8000.0)), ["deathpose"])

    def test_suppress_rule_hides_a_deliberate_teleport(self):
        sc = synth.inject_rubber(synth.walker(120.0), 2000, 400.0, 200.0)
        st = one("host", sc)
        rep = analyze({"host": st})
        self.assertGreaterEqual(rep["counters"]["glitch.rubberband"], 1)
        rep2 = analyze({"host": st}, suppress=[{"kind": "rubberband", "ref": NPC_REF, "t0": 1500, "t1": 2600}])
        self.assertEqual(rep2["counters"]["glitch.rubberband"], 0)


class CompareTests(unittest.TestCase):
    def pair(self, host_sc, client_sc, th=DEFAULT, **kw):
        host = one("host", host_sc, **kw)
        client = one("client", client_sc, **kw)
        return analyze({"host": host, "client": client}, th)

    def test_clean_delayed_client_is_in_sync(self):
        host = synth.walker(120.0)
        rep = self.pair(host, synth.delayed(host, 100.0))
        c = rep["counters"]
        self.assertLess(c["pose_err_p95"], 2.0, c)
        self.assertEqual(c["wrong_weapon"], 0)
        self.assertEqual(c["wrong_facing"], 0)
        self.assertEqual(c["group_mismatch_episodes"], 0)
        self.assertEqual(c["glitches"], 0)
        self.assertTrue(rep["ok"], rep["failures"])
        self.assertGreater(c["compared"], 50)

    def test_latency_jitter_inside_allowance(self):
        host = synth.walker(120.0)
        rep = self.pair(host, synth.delayed(host, 190.0))   # 100 ms buffer + 90 ms network
        self.assertEqual(rep["counters"]["group_mismatch_episodes"], 0)
        self.assertLess(rep["counters"]["pos_err_p95"], 12.0, rep["counters"])

    def test_far_too_late_client_is_caught(self):
        host = synth.walker(120.0)
        rep = self.pair(host, synth.delayed(host, 900.0))
        self.assertGreater(rep["counters"]["pos_err_p95"], 30.0, rep["counters"])

    def test_pose_defect_is_scored(self):
        host = synth.walker(120.0)

        def bad(t):
            s = synth.delayed(host, 100.0)(t)
            s.arm_offset = 150.0
            return s
        rep = self.pair(host, bad, th=DEFAULT.with_(pose_tol_units=5.0))
        self.assertGreater(rep["counters"]["pose_err_p95"], 8.0, rep["counters"])
        self.assertFalse(rep["ok"])
        self.assertTrue(any("pose_err_p95" in f for f in rep["failures"]))

    def test_wrong_weapon_needs_to_persist(self):
        host = synth.idler()

        def long_bad(t):
            s = host(t)
            if 2000 <= t < 4000:
                s.weapon_out, s.weapon_form = True, 0x0000FACE
            return s

        def blip(t):
            s = host(t)
            if 2000 <= t < 2150:
                s.weapon_out, s.weapon_form = True, 0x0000FACE
            return s
        self.assertGreaterEqual(self.pair(host, long_bad)["counters"]["wrong_weapon"], 1)
        self.assertEqual(self.pair(host, blip)["counters"]["wrong_weapon"], 0)

    def test_wrong_weapon_form_while_drawn(self):
        def drawn(form):
            def f(t):
                s = synth.idler()(t)
                s.weapon_out, s.weapon_form = True, form
                return s
            return f
        self.assertGreaterEqual(self.pair(drawn(0x1111), drawn(0x2222))["counters"]["wrong_weapon"], 1)
        self.assertEqual(self.pair(drawn(0x1111), drawn(0x1111))["counters"]["wrong_weapon"], 0)

    def test_wrong_facing(self):
        host = synth.idler()

        def bad(t):
            s = host(t)
            if 2000 <= t < 4000:
                s.rotz += 1.5
            return s
        c = self.pair(host, bad)["counters"]
        self.assertGreaterEqual(c["wrong_facing"], 1)
        self.assertGreater(c["facing_err_p95_deg"], 60.0)

    def test_group_mismatch(self):
        host = synth.walker(120.0)

        def bad(t):
            s = synth.delayed(host, 100.0)(t)
            if 2000 <= t < 4000:
                s.groups = [(0, G["idle"])]
            return s
        c = self.pair(host, bad)["counters"]
        self.assertGreaterEqual(c["group_mismatch_episodes"], 1)
        self.assertGreater(c["group_mismatch_pct"], 10.0)

    def test_ref_map_for_puppets(self):
        host = stream_from_probes("host", make_probes("host", {0x14: synth.walker(120.0)}, DUR), use_rx=True)
        cl = stream_from_probes("client", make_probes("client", {0x00FF0001: synth.delayed(synth.walker(120.0), 100.0)}, DUR), use_rx=True)
        none = analyze({"host": host, "client": cl})
        self.assertEqual(none["counters"]["compared"], 0)
        mapped = analyze({"host": host, "client": cl}, ref_maps={"client": {0x14: 0x00FF0001}})
        self.assertGreater(mapped["counters"]["compared"], 50)
        self.assertLess(mapped["counters"]["pose_err_p95"], 2.0)

    def test_different_clocks_need_offset_estimate(self):
        walker = synth.walker(120.0)
        host = stream_from_probes("host", make_probes("host", {NPC_REF: walker}, DUR, rx=False), use_rx=False)
        cl = stream_from_probes("client", make_probes("client", {NPC_REF: synth.delayed(walker, 100.0)}, DUR, rx=False,
                                                       clock_offset=1750.0), use_rx=False)
        bad = analyze({"host": host, "client": cl})
        self.assertGreater(bad["counters"]["pos_err_p95"], 100.0)
        good = analyze({"host": host, "client": cl}, auto_offset=True)
        self.assertLess(good["counters"]["pos_err_p95"], 5.0, good["pairs"])

    def test_pose_math(self):
        b = synth.bones_for(State(pos=(10, 20, 0), rotz=0.7))
        a = parse_actor({"ref": "0x1", "pos": [10, 20, 0], "rot": [0, 0, 0.7], "bones": b}, 0.0)
        c = parse_actor({"ref": "0x1", "pos": [500, -40, 0], "rot": [0, 0, -2.0],
                         "bones": synth.bones_for(State(pos=(500, -40, 0), rotz=-2.0))}, 0.0)
        la, lc = local_bones(a.bones, a.rotz), local_bones(c.bones, c.rotz)
        for k in la:
            self.assertAlmostEqual(math.dist(la[k], lc[k]), 0.0, delta=0.05)
        self.assertAlmostEqual(angdiff(3.0, -3.0), 2 * math.pi - 6.0, places=6)
        self.assertEqual(percentile([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 95), 10)
        self.assertEqual(percentile([], 50), 0.0)


class PlumbingTests(unittest.TestCase):
    def test_counters_check(self):
        rep = {"counters": {"pose_err_p95": 5.0, "glitches": 0}}
        self.assertEqual(check(rep, {"pose_err_p95": "<=12", "glitches": 0}), [])
        self.assertEqual(len(check(rep, {"pose_err_p95": "<=3"})), 1)
        self.assertEqual(len(check(rep, {"nope": "==0"})), 1)
        self.assertEqual(len(check(rep, {"glitches": "garbage"})), 1)
        self.assertEqual(check({"counters": {"a": 2}}, {"a": ">1"}), [])

    def test_all_counter_keys_exist_and_clean_report_passes(self):
        rep = analyze({"host": one("host", synth.walker(120.0))})
        for k in ("pose_err_p50", "pose_err_p95", "pose_err_max", "group_mismatch_pct", "wrong_weapon", "wrong_facing",
                  "glitches", "glitch.sliding", "glitch.tpose", "glitch.floating", "glitch.sinking", "glitch.jitter",
                  "glitch.rubberband", "glitch.stuck", "glitch.deathpose", "samples", "actors", "unknown_groups"):
            self.assertIn(k, rep["counters"])
        self.assertTrue(rep["ok"])
        self.assertEqual(check(rep, {"glitches": "==0", "samples": ">10"}), [])

    def test_glitch_counter_reaches_report(self):
        rep = analyze({"host": one("host", synth.inject_float(synth.idler(), 1500, 3500))})
        self.assertEqual(rep["counters"]["glitch.floating"], 1)
        self.assertEqual(rep["counters"]["glitches"], 1)
        self.assertFalse(rep["ok"])
        self.assertEqual(check(rep, {"glitches": "==0"})[0].split(":")[0], "glitches")

    def test_jsonl_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            probes = make_probes("host", {NPC_REF: synth.walker(120.0)}, 1000.0)
            p = Path(d) / "rt.probe.jsonl"
            self.assertEqual(write_probes(p, probes), len(probes))
            with open(p, "a", encoding="utf-8") as f:
                f.write("\n# comment\nnot json\n")
            back = read_probes(p)
            self.assertEqual(len(back), len(probes))
            self.assertEqual(back[3]["actors"][0]["pos"], probes[3]["actors"][0]["pos"])

    def test_cli_analyze_exit_codes(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            host, cl = d / "cli_host.jsonl", d / "cli_client.jsonl"
            write_probes(host, make_probes("host", {NPC_REF: synth.walker(120.0)}, 3000.0))
            write_probes(cl, make_probes("client", {NPC_REF: synth.delayed(synth.walker(120.0), 100.0)}, 3000.0))
            r = run_cli("analyze", f"host={host}", f"client={cl}", "--html", str(d / "cli.html"), "--json", str(d / "cli.json"))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("PASS", r.stdout)
            self.assertTrue((d / "cli.html").read_text(encoding="utf-8").startswith("<!doctype html>"))
            self.assertEqual(json.loads((d / "cli.json").read_text(encoding="utf-8"))["schema"], "animcheck.report.v1")
            r2 = run_cli("analyze", f"host={host}", "--assert", "samples<5")
            self.assertEqual(r2.returncode, 1, r2.stdout + r2.stderr)
            r3 = run_cli("analyze", f"host={host}", "--assert", "nonsense")
            self.assertEqual(r3.returncode, 2)
            r4 = run_cli("analyze", f"host={host}", "--profile", str(d / "missing.json"))
            self.assertEqual(r4.returncode, 2)

    def test_cli_expect_moving_and_suppress(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            idle = d / "idle.jsonl"
            write_probes(idle, make_probes("host", {NPC_REF: synth.idler()}, 6000.0))
            r = run_cli("analyze", f"host={idle}")
            self.assertEqual(r.returncode, 0, r.stdout)
            r = run_cli("analyze", f"host={idle}", "--expect-moving", "0x10A2B3:1000-5000")
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn("glitch.stuck = 1", r.stdout)
            r = run_cli("analyze", f"host={idle}", "--expect-moving", "0x10A2B3:1000-5000", "--suppress", "stuck:*:0-9000")
            self.assertEqual(r.returncode, 0, r.stdout)
            r = run_cli("analyze", f"host={idle}", "--expect-moving", "nonsense")
            self.assertEqual(r.returncode, 2)

    def test_cli_demo_finds_the_planted_defects(self):
        with tempfile.TemporaryDirectory() as d:
            r = run_cli("demo", "--out", d)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rep = json.loads((Path(d) / "report.json").read_text(encoding="utf-8"))
            c = rep["counters"]
            self.assertFalse(rep["ok"])
            self.assertGreaterEqual(c["avatar.pose_err_p95"], 8.0, c)
            self.assertGreaterEqual(c["avatar.glitch.sliding"], 1, c)
            self.assertEqual(c["glitch.sliding"], 0, c)     # the NPC pair is clean
            self.assertTrue((Path(d) / "report.html").read_text(encoding="utf-8").startswith("<!doctype html>"))

    def test_collector_with_fake_bridge(self):
        class Clock:
            t = 100.0

            def __call__(self):
                return Clock.t

            @staticmethod
            def sleep(d):
                Clock.t += d

        class Bridge:
            def __init__(self, script, delay):
                self.script, self.delay, self.n = script, delay, 0

            def call(self, method, params=None):
                assert method == "anim.probe"
                self.n += 1
                t = (Clock.t - 100.0) * 1000.0
                return {"v": 1, "frame": self.n, "t_ms": t,
                        "actors": [synth.actor_dict(NPC_REF, self.script(max(0, t - self.delay)), True, t)]}

        host_sc = synth.walker(120.0)
        c = Collector({"host": Bridge(host_sc, 0.0), "client": Bridge(host_sc, 100.0)}, clock=Clock())
        c.run(seconds=3.0, hz=20, sleep=Clock.sleep)
        self.assertGreaterEqual(len(c.probes["host"]), 55)
        rep = analyze(c.streams())
        self.assertGreater(rep["counters"]["compared"], 40)
        self.assertEqual(rep["counters"]["glitches"], 0, rep["machines"])
        self.assertLess(rep["counters"]["pose_err_p95"], 2.0)

        class Broken:
            def call(self, m, p=None):
                raise OSError("pipe closed")
        c2 = Collector({"a": Broken(), "b": Bridge(host_sc, 0.0)}, clock=Clock())
        self.assertEqual(c2.poll_once(), 1)
        self.assertEqual(c2.errors["a"], 1)

    def test_tcp_bridge_round_trip(self):
        script = synth.walker(120.0)
        calls = []

        class H(socketserver.StreamRequestHandler):
            def handle(self):
                for line in self.rfile:
                    req = json.loads(line.decode("utf-8"))
                    calls.append(req)
                    if req["method"] == "anim.stats":
                        reply = {"error": "no stats here"}
                    else:
                        n = len(calls)
                        reply = {"v": 1, "frame": n, "t_ms": n * 50.0,
                                 "actors": [synth.actor_dict(NPC_REF, script(n * 50.0), True, n * 50.0)]}
                    self.wfile.write(json.dumps(reply).encode("utf-8") + b"\n")

        srv = socketserver.ThreadingTCPServer(("127.0.0.1", 0), H)
        srv.daemon_threads = True
        th = threading.Thread(target=srv.serve_forever, daemon=True)
        th.start()
        try:
            port = srv.server_address[1]
            with tempfile.TemporaryDirectory() as d:
                with Collector.from_addresses({"host": ("127.0.0.1", port)}, out_dir=d, timeout_s=3.0) as c:
                    for _ in range(5):
                        self.assertEqual(c.poll_once({"max": 4}), 1)
                    self.assertEqual(c.costs(), {})          # the error reply is not a cost record
                self.assertEqual(calls[0], {"method": "anim.probe", "params": {"max": 4}})
                back = read_probes(Path(d) / "host.probe.jsonl")
                self.assertEqual(len(back), 5)
                self.assertEqual(back[0]["machine"], "host")
                self.assertIn("rx_ms", back[0])
            b = TcpBridge("127.0.0.1", port, 3.0)
            with self.assertRaises(RuntimeError):
                b.call("anim.stats")
            b.close()
        finally:
            srv.shutdown()
            srv.server_close()


if __name__ == "__main__":
    unittest.main()
