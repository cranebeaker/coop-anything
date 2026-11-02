"""Public repository hygiene for this folder: no em dashes, no personal paths."""
import unittest
from pathlib import Path

import support

ROOT = Path(support.HERE)


class Hygiene(unittest.TestCase):
    def test_no_em_dashes_or_personal_paths_in_this_folder(self):
        checked = 0
        for p in ROOT.rglob("*"):
            if p.suffix in (".py", ".md", ".json") and "__pycache__" not in p.parts:
                text = p.read_text(encoding="utf-8")
                self.assertNotIn(chr(0x2014), text, p.name)
                self.assertNotIn("\\" + "Us" + "ers\\", text, p.name)
                self.assertNotIn("/" + "Us" + "ers/", text, p.name)
                checked += 1
        self.assertGreater(checked, 20)


if __name__ == "__main__":
    unittest.main()
