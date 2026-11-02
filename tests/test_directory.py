import sys
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import build_directory as bd


def contains_bool(value):
    if isinstance(value, bool):
        return True
    if isinstance(value, list):
        return any(contains_bool(v) for v in value)
    if isinstance(value, dict):
        return any(contains_bool(v) for v in value.values())
    return False


class YamlNormalizationTests(unittest.TestCase):
    def test_yes_no_and_true_false_become_catalogue_strings(self):
        raw = yaml.safe_load(
            """
            open_source: yes
            coop_scope:
              players_visible: true
              story: no
              enemies: false
            flags: [yes, no, true, false]
            """
        )
        self.assertTrue(contains_bool(raw))
        self.assertEqual(
            bd.normalize_yaml(raw),
            {
                "open_source": "yes",
                "coop_scope": {
                    "players_visible": "yes",
                    "story": "no",
                    "enemies": "no",
                },
                "flags": ["yes", "no", "yes", "no"],
            },
        )

    def test_real_catalogue_contains_no_python_booleans_after_loading(self):
        entries = bd.load_entries()
        self.assertEqual(len(entries), 261)
        self.assertFalse(contains_bool(entries))
        rendered = bd.render(entries)
        self.assertNotIn(": True", rendered)
        self.assertNotIn(": False", rendered)

    def test_render_preserves_partial_scope(self):
        entry = bd.normalize_yaml(yaml.safe_load(
            """
            game: Example
            project: Example co-op
            kind: mod
            status: alpha
            qa: none
            sources: [https://example.org]
            checked: 2026-11-01
            coop_scope: {players_visible: yes, story: no, items: per-player}
            """
        ))
        text = bd.render([entry])
        self.assertIn("players visible: yes, story: no, items: per-player", text)


if __name__ == "__main__":
    unittest.main()
