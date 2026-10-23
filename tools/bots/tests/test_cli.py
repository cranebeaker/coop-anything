"""Command line tests: exit codes, the fake run, and a run through --adapter with refs read from a scenario file."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

import support
from support import HERE, registry
from coopbots import refs as refs_mod

CLI = os.path.join(HERE, "run_coop.py")

ADAPTER_MODULE = '''
from coopbots import registry
_world, REFS = registry.REGISTRY["loot_race"].fake(2)

def make_adapter(i):
    return _world.bots[i]
'''


def run_cli(*args, cwd=None):
    p = subprocess.run([sys.executable, "-I", CLI, *args], capture_output=True, text=True, errors="replace", cwd=cwd)
    return p.returncode, p.stdout + p.stderr


class Cli(unittest.TestCase):
    def test_fake_pass_and_twin_fail(self):
        d = tempfile.mkdtemp()
        code, out = run_cli("--fake", "--playbook", "loot_race,consent_vote", "--outdir", d)
        self.assertEqual(code, 0, out)
        self.assertIn("PASS  loot_race", out)
        code, out = run_cli("--fake", "--playbook", "loot_race", "--bug", "dup_pickup", "--outdir", d)
        self.assertEqual(code, 1, out)
        self.assertIn("FAIL  loot_race", out)
        with open(os.path.join(d, "summary.json"), encoding="utf-8") as f:
            self.assertFalse(json.load(f)["ok"])

    def test_every_playbook_passes_through_the_cli(self):
        code, out = run_cli("--fake", "--playbook", "all", "--outdir", tempfile.mkdtemp())
        self.assertEqual(code, 0, out[-2000:])
        for name in registry.REGISTRY:
            self.assertIn("PASS  " + name, out)

    def test_lossy_link_option(self):
        code, out = run_cli("--fake", "--playbook", "quest_sync", "--link", "bad_wifi", "--outdir", tempfile.mkdtemp())
        self.assertEqual(code, 0, out[-2000:])

    def test_usage_errors(self):
        self.assertEqual(run_cli("--fake", "--playbook", "nope")[0], 2)
        self.assertEqual(run_cli("--playbook", "fight")[0], 2)
        self.assertEqual(run_cli("--fake", "--playbook", "fight", "--link", "dialup")[0], 2)
        code, out = run_cli("--list")
        self.assertEqual(code, 0)
        for n in registry.REGISTRY:
            self.assertIn(n, out)

    def test_a_run_where_everything_is_skipped_does_not_exit_zero(self):
        d = tempfile.mkdtemp()
        with open(os.path.join(d, "bare_adapter.py"), "w") as f:
            f.write("from coopbots.adapter import ActionAdapter\n\ndef make_adapter(i):\n    return ActionAdapter()\n")
        with open(os.path.join(d, "refs.json"), "w") as f:
            json.dump({"item": "x", "item_base": "y"}, f)
        code, out = run_cli("--adapter", "bare_adapter:make_adapter", "--refs", "refs.json", "--playbook", "loot_race",
                            "--outdir", os.path.join(d, "out"), cwd=d)
        self.assertEqual(code, 1, out)
        self.assertIn("SKIP  loot_race", out)

    def test_adapter_run_with_refs_from_a_scenario_file(self):
        d = tempfile.mkdtemp()
        with open(os.path.join(d, "my_game.py"), "w") as f:
            f.write(ADAPTER_MODULE)
        sc = {"objects": {"pile": {"id": "world_item", "kind": "thing", "base": "healing_pack"}, "pack": {"id": "healing_pack", "kind": "item"}},
              "refs": {"item": "$pile", "item_base": "$pack"}}
        with open(os.path.join(d, "loot.json"), "w") as f:
            json.dump(sc, f)
        code, out = run_cli("--adapter", "my_game:make_adapter", "--scenario", "loot.json", "--playbook", "loot_race",
                            "--outdir", os.path.join(d, "out"), cwd=d)
        self.assertEqual(code, 0, out)
        self.assertIn("PASS  loot_race", out)
        self.assertTrue(os.path.isfile(os.path.join(d, "out", "summary.json")))


class Refs(unittest.TestCase):
    def test_aliases_become_ids_everywhere(self):
        sc = {"objects": {"a": {"id": "ID_A"}, "b": {"id": "ID_B"}},
              "refs": {"one": "$a", "many": ["$a", "$b", 5], "spec": {"ref": "$b", "dist": 300}, "plain": "text", "n": 3}}
        self.assertEqual(refs_mod.refs_from_scenario(sc), {"one": "ID_A", "many": ["ID_A", "ID_B", 5],
                                                           "spec": {"ref": "ID_B", "dist": 300}, "plain": "text", "n": 3})

    def test_unknown_alias_is_an_error(self):
        with self.assertRaises(KeyError):
            refs_mod.refs_from_scenario({"objects": {}, "refs": {"x": "$nope"}})

    def test_reads_a_file_and_the_shipped_scenario_examples(self):
        ex = os.path.join(os.path.dirname(HERE), "scenarios", "examples", "fnv", "fight.json")
        self.assertEqual(refs_mod.refs_from_scenario(ex)["raiders"], ["GSPG01Ref", "GSPG02Ref", "GSPG03Ref"])
        ho = os.path.join(os.path.dirname(HERE), "scenarios", "examples", "fnv", "ownership_handoff.json")
        self.assertEqual(refs_mod.refs_from_scenario(ho)["far_pos"], {"ref": "NovacMapMarker", "dist": 400})


if __name__ == "__main__":
    unittest.main()
