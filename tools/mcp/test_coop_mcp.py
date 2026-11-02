"""Tests for the Co-op Anything MCP server. Stdlib only, plus PyYAML for the directory.

    python -m unittest tools/mcp/test_coop_mcp.py
"""
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import coop_mcp as m  # noqa: E402


def make_install(files):
    """A throwaway install folder: {relative path: first bytes}."""
    root = tempfile.TemporaryDirectory()
    for rel, content in files.items():
        p = Path(root.name, rel)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(content)
    return root


def best(files):
    with make_install(files) as d:
        guess = m.detect_engine(d)["best_guess"]
        return guess and guess["engine"]


class FamilyKeys(unittest.TestCase):
    def test_ranges_and_bare_numbers_keep_the_family_name(self):
        self.assertEqual(m._family_keys("id Tech 2 to 4"), ["id tech 2", "id tech 3", "id tech 4"])
        self.assertEqual(m._family_keys("Unreal Engine 4/5"), ["unreal engine 4", "unreal engine 5"])
        self.assertEqual(m._family_keys("Source / GoldSrc"), ["source", "goldsrc"])

    def test_numbers_do_not_match_other_engines(self):
        keys = m._family_keys("id Tech 2 to 4")
        for engine in ("4A Engine", "REDengine 4", "Unreal Engine 4", "Source (2004)"):
            self.assertFalse(m._same_family(engine, keys), engine)
        self.assertTrue(m._same_family("id Tech 4", keys))
        keys = m._family_keys("Unreal Engine 4/5")
        self.assertFalse(m._same_family("Unreal Engine 2.5", keys))
        self.assertFalse(m._same_family("id Tech 5", keys))
        self.assertTrue(m._same_family("Unreal Engine 4.27", keys))

    def test_related_projects_stay_in_the_family(self):
        with make_install({"base/pak000.pk4": b""}) as d:
            related = m.detect_engine(d)["same_engine_projects"]
        keys = m._family_keys("id Tech 2 to 4")
        self.assertTrue(related)
        self.assertTrue(all(m._same_family(e["engine"], keys) for e in related))


class Signatures(unittest.TestCase):
    def test_one_stray_upk_is_not_unreal_engine_3(self):
        self.assertIsNone(best({"Mod/research/embedded.upk": b""}))
        self.assertEqual(best({f"Game/p{i}.upk": b"" for i in range(3)}), "Unreal Engine 3")
        self.assertEqual(best({"Game/CookedPC/x.bin": b""}), "Unreal Engine 3")

    def test_pck_needs_the_godot_header(self):
        self.assertIsNone(best({"Audio/SFX.pck": b"AKPK"}))  # Wwise sound bank package
        self.assertEqual(best({"game.pck": b"GDPC"}), "Godot")

    def test_archive_headers(self):
        self.assertEqual(best({"Data00.packed": b"BFPK"}), "MercurySteam ScrapEngine")
        self.assertEqual(best({"txd/gui.txd": b"\x16\x00\x00\x00"}), "RenderWare")
        self.assertEqual(best({"data/misc.vpp_pc": b"\xce\x0a\x89\x51"}),
                         "Volition (.vpp_pc archives, engine family unresolved)")
        self.assertIsNone(best({"Data00.packed": b"PK\x03\x04", "gui.txd": b"text"}))

    def test_genome_needs_sharedbase_next_to_engine(self):
        g3 = {"SharedBase.dll": b"", "Engine.dll": b"", "Script.dll": b"", "Data/Projects_compiled.pak": b""}
        self.assertEqual(best(g3), "Genome (Piranha Bytes)")
        self.assertIsNone(best({"bin/engine.dll": b"", "Script.dll": b""}))
        self.assertIsNone(best({"SharedBase.dll": b""}))


def utf16(s):
    return (s + "\0").encode("utf-16-le")


def version_string(key, value):
    head = struct.pack("<HHH", 0, len(value) + 1, 1) + utf16(key)
    body = head + b"\0" * (-len(head) % 4) + utf16(value)
    body = struct.pack("<H", len(body)) + body[2:]
    return body + b"\0" * (-len(body) % 4)


class Identity(unittest.TestCase):
    def test_version_resource(self):
        fixed = struct.pack("<13I", 0xFEEF04BD, 0x10000, (1 << 16) | 2, (3 << 16) | 4, (5 << 16) | 6, (7 << 16) | 8,
                            0, 0, 0, 0, 0, 0, 0)
        body = utf16("VS_VERSION_INFO") + b"\0\0" + fixed
        body += version_string("ProductName", "Test Game") + version_string("FileVersion", "1.2.3.4")
        data = b"\0" * 64 + struct.pack("<HHH", 6 + len(body), len(fixed), 0) + body
        info = m._version_info(data)
        self.assertEqual(info["file_version"], "1.2.3.4")
        self.assertEqual(info["product_version"], "5.6.7.8")
        self.assertEqual(info["strings"], {"ProductName": "Test Game", "FileVersion": "1.2.3.4"})
        self.assertEqual(m._version_info(b"\0" * 64), {})

    def test_steam_build_from_appmanifest(self):
        acf = ('"AppState"\n{\n\t"appid"\t\t"123"\n\t"name"\t\t"Test Game"\n\t"installdir"\t\t"Test Game"\n'
               '\t"buildid"\t\t"456"\n\t"InstalledDepots"\n\t{\n\t\t"124"\n\t\t{\n\t\t\t"manifest"\t\t"789"\n'
               '\t\t}\n\t}\n}\n').encode()
        with make_install({"steamapps/appmanifest_123.acf": acf,
                           "steamapps/common/Test Game/bin/game.exe": b"\0" * 64}) as d:
            out = m.game_identity(str(Path(d, "steamapps/common/Test Game/bin/game.exe")))
        self.assertEqual(out["steam"], {"appid": "123", "buildid": "456", "name": "Test Game",
                                        "manifest": "appmanifest_123.acf"})
        with make_install({"game.exe": b"\0" * 64}) as d:
            self.assertNotIn("steam", m.game_identity(str(Path(d, "game.exe"))))


class Directory(unittest.TestCase):
    def test_search_returns_sources(self):
        entries = m.t_search_directory("", limit=1000)["entries"]
        projects = [e for e in entries if e["kind"] != "gap"]
        self.assertTrue(projects)
        self.assertTrue(all(e.get("sources") for e in projects))


class TestTools(unittest.TestCase):
    def test_every_listed_tool_has_a_readable_readme(self):
        names = m.t_list_docs()["test_tools"]
        self.assertIn("chaos", names)
        for name in names:
            self.assertTrue(m.t_get_test_tool(name).strip(), name)

    def test_no_path_escape(self):
        with self.assertRaises(ValueError):
            m.t_get_test_tool("../../..")


class Protocol(unittest.TestCase):
    def call(self, *messages):
        lines = "\n".join(json.dumps(msg) for msg in messages) + "\n"
        out = subprocess.run([sys.executable, str(Path(m.__file__))], input=lines, capture_output=True,
                             text=True, encoding="utf-8", timeout=60).stdout
        return [json.loads(line) for line in out.splitlines()]

    def test_unknown_tool_and_method(self):
        tool, method = self.call(
            {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "nope", "arguments": {}}},
            {"jsonrpc": "2.0", "method": "notifications/initialized"},
            {"jsonrpc": "2.0", "id": 2, "method": "nope"})
        self.assertEqual(tool["error"], {"code": -32602, "message": "unknown tool nope"})
        self.assertEqual(method["error"], {"code": -32601, "message": "method not found: nope"})


if __name__ == "__main__":
    unittest.main()
