"""Avatar fidelity: player A's own character on A's machine vs A's puppet on B, scored apart from NPCs (avatar.* counters)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from animcheck import synth  # noqa: E402
from animcheck.analyze import analyze  # noqa: E402
from animcheck.config import DEFAULT  # noqa: E402
from animcheck.counters import check  # noqa: E402
from animcheck.model import stream_from_probes  # noqa: E402
from animcheck.synth import G, NPC_REF, make_probes  # noqa: E402

PLAYER, PUPPET = 0x00000014, 0x00FF000A
AVATAR = [{"name": "A", "owner": "host", "owner_ref": PLAYER, "observer": "client", "observer_ref": PUPPET}]
DUR = 6000.0


def player():
    def f(t):
        s = synth.walker(120.0)(t)
        s.weapon_out, s.weapon_form, s.aiming = True, 0x1234, True
        s.rotx = 0.2
        return s
    return f


def puppet_of(real):
    return synth.delayed(real, 100.0)


def run_pair(real, puppet, npc_host=None, npc_client=None, th=DEFAULT):
    hs, cs = {PLAYER: real}, {PUPPET: puppet}
    if npc_host:
        hs[NPC_REF], cs[NPC_REF] = npc_host, npc_client
    host = stream_from_probes("host", make_probes("host", hs, DUR), use_rx=True)
    client = stream_from_probes("client", make_probes("client", cs, DUR), use_rx=True)
    return analyze({"host": host, "client": client}, th, avatars=AVATAR)


class AvatarTests(unittest.TestCase):
    def test_clean_avatar_passes_and_is_separate_from_npcs(self):
        real = player()
        rep = run_pair(real, puppet_of(real), synth.walker(200.0), synth.delayed(synth.walker(200.0), 100.0))
        c = rep["counters"]
        self.assertGreater(c["avatar.compared"], 50)
        self.assertLess(c["avatar.pose_err_p95"], 2.0, c)
        for k in ("avatar.anim_mismatches", "avatar.wrong_weapon", "avatar.wrong_facing", "avatar.wrong_aim",
                  "avatar.glitches", "avatar.stance_mismatch"):
            self.assertEqual(c[k], 0, (k, c))
        self.assertTrue(rep["ok"], rep["failures"])
        self.assertGreater(c["compared"], 50)          # the NPC pair is still compared ...
        self.assertEqual(c["compared_actors"], 1)       # ... and the avatar is not part of it

    def test_pose_error_on_puppet(self):
        real = player()

        def bad(t):
            s = puppet_of(real)(t)
            s.arm_offset = 150.0
            return s
        rep = run_pair(real, bad, th=DEFAULT.with_(pose_tol_units=5.0))
        self.assertGreater(rep["counters"]["avatar.pose_err_p95"], 8.0)
        self.assertEqual(rep["counters"]["pose_err_p95"], 0.0)   # NPC numbers untouched
        self.assertFalse(rep["ok"])
        self.assertTrue(any("avatar.pose_err_p95" in f for f in rep["failures"]))

    def test_anim_weapon_aim_facing_mismatches(self):
        real = player()

        def mk(field):
            def f(t):
                s = puppet_of(real)(t)
                if 2000 <= t < 4000:
                    if field == "anim":
                        s.groups = [(0, G["idle"])]
                    elif field == "weapon":
                        s.weapon_form = 0x9999
                    elif field == "aim":
                        s.rotx = 1.0
                    elif field == "facing":
                        s.rotz += 1.5
                return s
            return f
        for field, key in (("anim", "avatar.anim_mismatches"), ("weapon", "avatar.wrong_weapon"), ("aim", "avatar.wrong_aim"),
                           ("facing", "avatar.wrong_facing")):
            c = run_pair(real, mk(field))["counters"]
            self.assertGreaterEqual(c[key], 1, (field, c))

    def test_stance_mismatch(self):
        real = player()

        def f(t):
            s = puppet_of(real)(t)
            if 2000 <= t < 4000:
                s.aiming = False
            return s
        self.assertGreaterEqual(run_pair(real, f)["counters"]["avatar.stance_mismatch"], 1)

    def test_puppet_glitches_counted_as_avatar_not_npc(self):
        real = player()
        c = run_pair(real, synth.inject_jitter(puppet_of(real), 1000, 2500, 12.0))["counters"]
        self.assertGreaterEqual(c["avatar.glitch.jitter"], 1)
        self.assertEqual(c["glitches"], 0)
        c = run_pair(real, synth.inject_rubber(puppet_of(real), 2000, 400.0, 200.0))["counters"]
        self.assertGreaterEqual(c["avatar.glitch.rubberband"], 1)
        c = run_pair(real, synth.inject_sliding(puppet_of(real), 1000, 2500))["counters"]
        self.assertGreaterEqual(c["avatar.glitch.sliding"], 1)

    def test_npc_glitch_does_not_leak_into_avatar_counters(self):
        real = player()
        rep = run_pair(real, puppet_of(real), synth.walker(120.0),
                       synth.inject_float(synth.delayed(synth.walker(120.0), 100.0), 1500, 3500))
        self.assertEqual(rep["counters"]["avatar.glitches"], 0)
        self.assertEqual(rep["counters"]["glitch.floating"], 1)

    def test_assert_on_avatar_counters(self):
        real = player()
        rep = run_pair(real, puppet_of(real))
        self.assertEqual(check(rep, {"avatar.pose_err_p95": "<=12", "avatar.anim_mismatches": "==0"}), [])


if __name__ == "__main__":
    unittest.main()
