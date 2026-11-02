"""Profiles: bone names, animation group tables and units come from a per-game JSON file, not from the code."""
from __future__ import annotations

import contextlib
import json
import math
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from animcheck import detectors, synth  # noqa: E402
from animcheck.analyze import analyze  # noqa: E402
from animcheck.config import DEFAULT, Thresholds  # noqa: E402
from animcheck.model import parse_actor, stream_from_probes, write_probes  # noqa: E402
from animcheck.profile import DEFAULT_PROFILE, GENERIC, Profile  # noqa: E402
from animcheck.synth import NPC_REF, make_probes  # noqa: E402

PKG = Path(__file__).resolve().parent.parent
CLI = str(PKG / "cli.py")
FNV_PATH = PKG / "examples" / "fnv" / "profile.json"
FNV = Profile.load(FNV_PATH)

NAMES_ONLY = Profile.from_dict({
    "name": "names",
    "categories": {"idle": ["Idle"], "walk": ["Walk"], "run": ["Run"], "death": ["Death"], "jump": ["Jump"],
                   "reload": ["Reload"], "attack": ["Attack"]},
})


@contextlib.contextmanager
def profile_in_synth(p):
    synth.use(p)
    try:
        yield
    finally:
        synth.use(None)


def events_with(p, script, dur=6000.0, expect=()):
    st = stream_from_probes("host", make_probes("host", {NPC_REF: script}, dur), use_rx=True, profile=p)
    return detectors.run_all(st.by_ref[NPC_REF], p.thresholds(), expect, profile=p)


def kinds(evs):
    return sorted({e.kind for e in evs})


def cli_env():
    env = {k: v for k, v in os.environ.items() if k != "ANIMCHECK_MUTANT"}
    env["ANIMCHECK_MUTANT"] = os.environ.get("ANIMCHECK_MUTANT", "")
    return env


def run_cli(*args):
    return subprocess.run([sys.executable, "-I", CLI, *args], capture_output=True, text=True, env=cli_env())


class ProfileTests(unittest.TestCase):
    def test_generic_categories(self):
        g = DEFAULT_PROFILE
        self.assertIs(g, GENERIC)
        self.assertEqual(g.category(g.groups.index("walk")), "walk")
        self.assertEqual(g.category(g.groups.index("talk")), "misc")
        self.assertEqual(g.category(999), "unknown")
        self.assertEqual(g.category(-1), "unknown")
        self.assertEqual(g.group_name(999), "group999")

    def test_fnv_example_profile_table(self):
        p = FNV
        self.assertEqual(len(p.groups), 245)
        self.assertEqual(p.root_bone, "Bip01 Pelvis")
        for name, cat in (("Idle", "idle"), ("Forward", "walk"), ("FastForward", "run"), ("DodgeBack", "dodge"),
                          ("TurnLeft", "turn"), ("AimIS", "aim"), ("AttackRight", "attack"), ("Counter", "attack"),
                          ("PlaceMine2", "attack"), ("ReloadA", "reload"), ("JamB", "jam"), ("Holster", "equip"),
                          ("HandGrip1", "equip"), ("Stagger", "hit"), ("Death", "death"), ("JumpLoop", "jump"),
                          ("BlockIdle", "block"), ("Talking", "misc"), ("PipBoyChild", "misc")):
            gid = p.groups.index(name)
            self.assertEqual(p.group_name(gid), name)
            self.assertEqual(p.category(gid), cat, name)
        self.assertEqual(p.category(9999), "unknown")
        # every group of the table lands in a real category
        self.assertEqual([i for i in range(len(p.groups)) if p.category(i) == "unknown"], [])

    def test_from_dict_validates(self):
        for bad in ({"bogus": 1}, {"categories": {"flying": ["x"]}}, {"angle_unit": "grad"}, {"thresholds": {"nope": 1}},
                    {"units_per_meter": 0}, {"groups": 5}, {"move_flag_bits": {"run": 3}}, {"default_category": "x"}):
            with self.assertRaises(ValueError, msg=str(bad)):
                Profile.from_dict(bad)
        with self.assertRaises(ValueError):
            Profile.from_dict([])

    def test_sparse_group_table_and_ids_in_categories(self):
        p = Profile.from_dict({"groups": {"5": "Stroll", "9": "Sprint"}, "categories": {"walk": ["Stroll"], "run": [9]}})
        self.assertEqual(p.group_name(5), "Stroll")
        self.assertEqual(p.category(5), "walk")
        self.assertEqual(p.category(9), "run")
        self.assertEqual(p.category(6), "unknown")
        self.assertEqual(p.first_group("walk"), 5)

    def test_thresholds_scale_and_override(self):
        p = Profile.from_dict({"units_per_meter": 140.0, "thresholds": {"slide_min_speed": 77.0, "loco_bands": {"walk": [1, 2]}}})
        th = p.thresholds()
        self.assertEqual(th.slide_min_speed, 77.0)
        self.assertEqual(th.rubber_dist, DEFAULT.rubber_dist * 2)
        self.assertEqual(th.loco_bands["walk"], (1.0, 2.0))
        self.assertEqual(th.loco_bands["run"], (DEFAULT.loco_bands["run"][0] * 2, DEFAULT.loco_bands["run"][1] * 2))
        self.assertEqual(th.slide_ms, DEFAULT.slide_ms)         # times are not scaled
        with self.assertRaises(ValueError):
            DEFAULT.with_(nope=1)
        self.assertIsInstance(Thresholds().with_(jitter_amp=1.0), Thresholds)

    def test_angle_unit_and_move_flags(self):
        deg = Profile.from_dict({"angle_unit": "deg"})
        a = parse_actor({"ref": 1, "rot": [10.0, 0.0, 90.0]}, 0.0, profile=deg)
        self.assertAlmostEqual(a.rotz, math.pi / 2, places=6)
        self.assertAlmostEqual(a.rotx, math.radians(10.0), places=6)
        a = parse_actor({"ref": 1, "rot": [0.0, 0.0, 1.0]}, 0.0)
        self.assertEqual(a.rotz, 1.0)
        raw = {"ref": 1, "move_flags": (1 << 9) | (1 << 11)}
        a = parse_actor(raw, 0.0, profile=FNV)
        self.assertTrue(a.sneaking and a.swimming)
        a = parse_actor(raw, 0.0)    # the generic profile does not know the bit layout
        self.assertFalse(a.sneaking or a.swimming)
        a = parse_actor({"ref": 1, "stance": {"sneaking": True}}, 0.0)
        self.assertTrue(a.sneaking)

    def test_summary(self):
        self.assertIn("245 groups", FNV.summary())


class FnvProfileDetectorTests(unittest.TestCase):
    """The same detectors, driven by the New Vegas bone names and group table."""

    def test_clean_streams_are_clean(self):
        with profile_in_synth(FNV):
            self.assertEqual(events_with(FNV, synth.walker(120.0)), [])
            self.assertEqual(events_with(FNV, synth.walker(320.0)), [])
            self.assertEqual(events_with(FNV, synth.idler()), [])
            self.assertEqual(synth.G["walk"], FNV.groups.index("Forward"))
            self.assertEqual(synth.G["run"], FNV.groups.index("FastForward"))

    def test_bone_names_come_from_the_profile(self):
        with profile_in_synth(FNV):
            b = synth.bones_for(synth.State(pos=(0, 0, 0)))
            self.assertIn("Bip01 L UpperArm", b)
            self.assertNotIn("upper_arm_l", b)
        self.assertIn("upper_arm_l", synth.bones_for(synth.State(pos=(0, 0, 0))))

    def test_tpose_arms_out_uses_the_profile_bones(self):
        with profile_in_synth(FNV):
            evs = events_with(FNV, synth.inject_tpose(synth.idler(), 1000, 2000, mode="arms"))
            self.assertEqual(kinds(evs), ["tpose"])
            # the same stream read with the generic bone names finds no arms at all
            st = stream_from_probes("host", make_probes("host", {NPC_REF: synth.inject_tpose(synth.idler(), 1000, 2000, mode="arms")}, 6000.0), use_rx=True)
            self.assertEqual(detectors.run_all(st.by_ref[NPC_REF], DEFAULT, profile=GENERIC), [])

    def test_locomotion_categories_from_the_table(self):
        with profile_in_synth(FNV):
            evs = events_with(FNV, synth.inject_sliding(synth.idler(), 1000, 2500))
            self.assertEqual(kinds(evs), ["sliding"])
            evs = events_with(FNV, synth.inject_death_alive(synth.idler(), 1000.0), 8000.0)
            self.assertEqual(kinds(evs), ["deathpose"])
            self.assertEqual(events_with(FNV, synth.proper_death(synth.walker(120.0), 2000.0), 9000.0), [])

    def test_pair_comparison_with_profile_bones(self):
        with profile_in_synth(FNV):
            host = synth.walker(120.0)

            def bad(t):
                s = synth.delayed(host, 100.0)(t)
                s.arm_offset = 150.0
                return s
            h = stream_from_probes("host", make_probes("host", {NPC_REF: host}, 6000.0), use_rx=True, profile=FNV)
            good_c = stream_from_probes("client", make_probes("client", {NPC_REF: synth.delayed(host, 100.0)}, 6000.0), use_rx=True, profile=FNV)
            bad_c = stream_from_probes("client", make_probes("client", {NPC_REF: bad}, 6000.0), use_rx=True, profile=FNV)
            ok = analyze({"host": h, "client": good_c}, FNV.thresholds(), profile=FNV)
            self.assertLess(ok["counters"]["pose_err_p95"], 2.0)
            self.assertTrue(ok["ok"], ok["failures"])
            self.assertEqual(ok["profile"], "fnv")
            worse = analyze({"host": h, "client": bad_c}, FNV.thresholds(), profile=FNV)
            self.assertGreater(worse["counters"]["pose_err_p95"], 8.0)

    def test_cli_profile_flag_end_to_end(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            with profile_in_synth(FNV):
                write_probes(d / "a.jsonl", make_probes("host", {NPC_REF: synth.inject_tpose(synth.idler(), 1000, 2000, mode="arms")}, 6000.0))
            r = run_cli("analyze", f"host={d / 'a.jsonl'}", "--profile", str(FNV_PATH))
            self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
            self.assertIn("glitch.tpose = 1", r.stdout)
            r = run_cli("profile", str(FNV_PATH))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("245 groups", r.stdout)
            (d / "bad.json").write_text(json.dumps({"bones": 5, "categories": {"nope": []}}), encoding="utf-8")
            self.assertEqual(run_cli("profile", str(d / "bad.json")).returncode, 2)
            (d / "notjson.json").write_text("{", encoding="utf-8")
            self.assertEqual(run_cli("profile", str(d / "notjson.json")).returncode, 2)


class NamedGroupTests(unittest.TestCase):
    """A game whose animation states are names, not numbers."""

    def test_string_groups_work_end_to_end(self):
        with profile_in_synth(NAMES_ONLY):
            self.assertEqual(synth.G["walk"], "Walk")
            self.assertEqual(events_with(NAMES_ONLY, synth.walker(120.0)), [])
            self.assertEqual(kinds(events_with(NAMES_ONLY, synth.inject_sliding(synth.idler(), 1000, 2500))), ["sliding"])
            host = synth.walker(120.0)

            def bad(t):
                s = synth.delayed(host, 100.0)(t)
                if 2000 <= t < 4000:
                    s.groups = [(0, "Idle")]
                return s
            h = stream_from_probes("host", make_probes("host", {NPC_REF: host}, 6000.0), use_rx=True, profile=NAMES_ONLY)
            c = stream_from_probes("client", make_probes("client", {NPC_REF: bad}, 6000.0), use_rx=True, profile=NAMES_ONLY)
            rep = analyze({"host": h, "client": c}, NAMES_ONLY.thresholds(), profile=NAMES_ONLY)
            self.assertGreaterEqual(rep["counters"]["group_mismatch_episodes"], 1)

    def test_unknown_groups_are_reported(self):
        with profile_in_synth(NAMES_ONLY):
            def moon(t):
                s = synth.walker(120.0)(t)
                s.groups = [(0, "Moonwalk")]
                return s
            st = stream_from_probes("host", make_probes("host", {NPC_REF: moon}, 3000.0), use_rx=True, profile=NAMES_ONLY)
            rep = analyze({"host": st}, NAMES_ONLY.thresholds(), profile=NAMES_ONLY)
            self.assertEqual(list(rep["unknown_groups"]), ["Moonwalk"])
            self.assertEqual(rep["counters"]["unknown_groups"], 1)
            clean = analyze({"host": stream_from_probes("host", make_probes("host", {NPC_REF: synth.walker(120.0)}, 3000.0),
                                                        use_rx=True, profile=NAMES_ONLY)}, NAMES_ONLY.thresholds(), profile=NAMES_ONLY)
            self.assertEqual(clean["unknown_groups"], {})


if __name__ == "__main__":
    unittest.main()
