"""Co-op Anything MCP server (stdio, JSON-RPC 2.0, no SDK needed).

Gives an agent the shared knowledge (directory, playbook, skills, game write-ups) and a few
local analysis tools (engine detection from an install folder, exe identity, directory
entry validation). Reads the repo it lives in, so `git pull` keeps it current.

    python tools/mcp/coop_mcp.py          # started by your agent, see playbook/mcp.md

Needs Python 3.9+ and PyYAML (pip install pyyaml).
"""
import hashlib
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

try:
    import yaml
except ImportError:  # still serve the text tools
    yaml = None

VERSION = "0.1.0"
PROTOCOL = "2025-06-18"

# ---------------------------------------------------------------- helpers


def read(rel):
    p = (ROOT / rel).resolve()
    if ROOT not in p.parents and p != ROOT:
        raise ValueError("path outside the repo")
    return p.read_text(encoding="utf-8")


def directory():
    if yaml is None:
        raise RuntimeError("PyYAML missing: pip install pyyaml")
    return yaml.safe_load(read("data/directory.yaml")) or []


def compact(e):
    # sources and searched come along so the agent can verify an entry before relying on it
    keep = ("game", "year", "engine", "project", "url", "other_urls", "kind", "status", "coop_scope",
            "players", "networking", "open_source", "last_activity", "qa", "notes", "up_for_adoption",
            "sources", "searched", "checked")
    return {k: (str(e[k]) if k in ("checked", "last_activity") else e[k]) for k in keep if k in e}


# ---------------------------------------------------------------- engine detection

# (engine, family hint for playbook/engine-families.md, test(files, dirs) -> evidence or None)
# files maps each lower-case file name to the first path it was seen at.
def _has(files, *names):
    return [n for n in names if n.lower() in files]


def _ext(files, *exts, at_least=1):
    found = sorted(f for f in files if any(f.endswith(x) for x in exts))
    return found[:5] if len(found) >= at_least else []


def _magic(files, ext, magic):
    """Names ending in ext whose first bytes are magic. Extensions like .pck are shared by
    unrelated formats (Godot packs, Wwise sound banks), so the header decides."""
    hits = []
    for name in sorted(f for f in files if f.endswith(ext))[:3]:
        try:
            with open(files[name], "rb") as fh:
                if fh.read(len(magic)) == magic:
                    hits.append(name)
        except OSError:
            pass
    return hits


def _family_keys(family):
    """Engine names a family covers: 'id Tech 2 to 4' -> id tech 2, 3, 4 and
    'Unreal Engine 4/5' -> unreal engine 4, 5. A bare '4' would match any engine with a 4 in it."""
    keys, prefix = [], ""
    for part in (p.strip() for p in family.lower().split("/")):
        span = re.fullmatch(r"(.*?)\s*(\d+)\s+to\s+(\d+)", part)
        if span:
            prefix = span.group(1) + " "
            keys += [f"{prefix}{n}" for n in range(int(span.group(2)), int(span.group(3)) + 1)]
        elif re.fullmatch(r"[\d.]+", part):
            keys.append(prefix + part)
        else:
            prefix = re.sub(r"[\d.]+$", "", part)
            keys.append(part)
    return [k for k in keys if k]


def _same_family(engine, keys):
    engine = str(engine or "").lower()
    return any(re.search(r"\b" + re.escape(k) + r"(?!\d)", engine) for k in keys)


SIGNATURES = [
    ("Unreal Engine 1 or 2", "Unreal Engine 1",
     lambda f, d: (_has(f, "core.u", "engine.u") or None) and _has(f, "core.u", "engine.u", "engine.dll", "core.dll")),
    # A real UE3 game ships hundreds of packages; one stray .upk (a mod's research folder) is not enough.
    ("Unreal Engine 3", "Unreal Engine 3",
     lambda f, d: [x for x in ("cookedpc", "cookedpcconsole") if x in d] or _ext(f, ".upk", ".xxx", at_least=3)),
    ("Unreal Engine 4 or 5", "Unreal Engine 4/5",
     lambda f, d: (_ext(f, ".utoc", ".ucas", ".pak") if "paks" in d or _ext(f, ".utoc") else None)),
    ("Unity (IL2CPP)", "Unity", lambda f, d: _has(f, "gameassembly.dll") and _has(f, "unityplayer.dll", "gameassembly.dll")),
    ("Unity (Mono)", "Unity", lambda f, d: _has(f, "unityplayer.dll", "assembly-csharp.dll") if _has(f, "assembly-csharp.dll") else None),
    ("Unity", "Unity", lambda f, d: _has(f, "unityplayer.dll")),
    ("Source", "Source / GoldSrc", lambda f, d: (_ext(f, ".vpk") and _has(f, "gameinfo.txt", "engine.dll", "hl2.exe")) or None),
    ("GoldSrc", "Source / GoldSrc", lambda f, d: _has(f, "hw.dll", "sw.dll") and _ext(f, ".wad")),
    ("Gamebryo / Creation Engine", "Creation Engine / Gamebryo", lambda f, d: _ext(f, ".esm") and (_ext(f, ".bsa", ".ba2") or None)),
    ("X-Ray (STALKER)", "X-Ray", lambda f, d: _has(f, "xrengine.exe", "xrcore.dll", "xr_3da.exe")),
    ("CryEngine", "CryEngine", lambda f, d: _has(f, "crysystem.dll", "cryaction.dll", "crygame.dll")),
    ("MercurySteam ScrapEngine", "ScrapEngine", lambda f, d: _magic(f, ".packed", b"BFPK")),
    # RenderWare texture dictionaries start with chunk type 0x16 (GTA III to San Andreas, Manhunt).
    ("RenderWare", "RenderWare", lambda f, d: _magic(f, ".txd", b"\x16\x00\x00\x00")),
    # Volition's .vpp_pc archive header (Red Faction Guerrilla); which engine family it is stays unresolved.
    ("Volition (.vpp_pc archives, engine family unresolved)", None,
     lambda f, d: _magic(f, ".vpp_pc", b"\xce\x0a\x89\x51")),
    ("RE Engine", "RE Engine", lambda f, d: _ext(f, "re_chunk_000.pak")),
    # Gothic 3: SharedBase.dll next to Engine.dll, Game.dll and Script.dll, data in Projects_compiled.pak.
    ("Genome (Piranha Bytes)", "Genome",
     lambda f, d: _has(f, "sharedbase.dll", "engine.dll", "game.dll", "script.dll", "projects_compiled.pak")
     if _has(f, "sharedbase.dll") and _has(f, "engine.dll") else None),
    ("id Tech 4", "id Tech 2 to 4", lambda f, d: _ext(f, ".pk4")),
    ("id Tech 3 (or derived)", "id Tech 2 to 4", lambda f, d: _ext(f, ".pk3")),
    ("Quake / id Tech 2", "id Tech 2 to 4", lambda f, d: _has(f, "pak0.pak") if ("id1" in d or "baseq2" in d) else None),
    ("Doom engine (id Tech 1)", "id Tech 2 to 4", lambda f, d: _has(f, "doom.wad", "doom2.wad", "heretic.wad", "hexen.wad")),
    ("Build engine", None, lambda f, d: _ext(f, ".grp")),
    ("REDengine 4", None, lambda f, d: _ext(f, ".archive") if "archive" in d else None),
    ("LithTech", None, lambda f, d: _ext(f, ".rez")),
    ("Godot", None, lambda f, d: _magic(f, ".pck", b"GDPC")),
    ("GameMaker", "GameMaker", lambda f, d: _has(f, "data.win")),
    ("RPG Maker", None, lambda f, d: _ext(f, ".rgss3a", ".rgss2a", ".rgssad") or _has(f, "rpg_core.js", "rmmz_core.js")),
    ("XNA / FNA / MonoGame", None, lambda f, d: _has(f, "fna.dll", "monogame.framework.dll", "microsoft.xna.framework.dll")),
    # Java games ship a .jar plus a bundled runtime (jre or jdk folder), or a launcher that starts java.exe.
    ("Java (JVM)", "Java",
     lambda f, d: _ext(f, ".jar") if ("jre" in d or "jdk" in d or _has(f, "java.exe", "javaw.exe")) else None),
    # Santa Monica Studio Kinetica (God of War 2018): libSceGnm.dll next to GoW.exe, .dcb and .wad archives under exec/.
    ("Santa Monica Studio Kinetica", "Santa Monica Studio Kinetica",
     lambda f, d: _has(f, "libscegnm.dll") and _ext(f, ".dcb") and _ext(f, ".wad")),
]

HOOK_TOOLS = {
    "Unreal Engine 1 or 2": "UnrealScript source in the .u packages, the game's own compiler (UCC or a mod's HCC), mod packages that subclass and swap classes",
    "Unreal Engine 3": "UE3 package tools (UE Explorer, UModel), script patching, DLL hooks",
    "Unreal Engine 4 or 5": "UE4SS (Lua and C++ mods), Dumper-7 SDK dump, DLL hooks",
    "Unity (IL2CPP)": "BepInEx or MelonLoader, Il2CppDumper or Cpp2IL, Harmony patches",
    "Unity (Mono)": "BepInEx or MelonLoader, dnSpyEx, Harmony patches",
    "Unity": "BepInEx or MelonLoader, Harmony patches",
    "Source": "Source SDK, server plugins, map and entity scripting (see Synergy, Sven Co-op)",
    "GoldSrc": "Half-Life SDK, AMX Mod X / Metamod style plugins",
    "Gamebryo / Creation Engine": "script extender (SKSE, F4SE, xNVSE, OBSE) plugin, Address Library, Tilted Online style design",
    "X-Ray (STALKER)": "Lua scripts and LTX configs in .db archives, xrMPE multiplayer extension",
    "CryEngine": "Lua game scripts, mod SDK, DLL hooks",
    "RE Engine": "REFramework (Lua and C++), DLL hooks",
    "MercurySteam ScrapEngine": "Lua 5.1 gameplay scripts in .packed (BFPK) archives (.lc bytecode, unluac), "
                                "-InitLuaLine: to run your Lua at startup, OnRenderDebug as a per-frame hook, "
                                "a proxy DLL to reuse the exe's Lua Net library",
    "RenderWare": "ASI plugins (Ultimate ASI Loader), plugin-sdk for GTA III, Vice City and San Andreas, CLEO scripts in GTA",
    "Genome (Piranha Bytes)": "the unofficial Gothic 3 SDK (C++ headers and import libraries), a native plugin DLL in scripts/ "
                              "with a ScriptInit export, wrapping named game scripts (AddHitPoints, GetAttitude)",
    "Java (JVM)": "a -javaagent (java.lang.instrument) loaded into the unmodified game JVM, the game's own public classes "
                  "and event bus as hook points (ByteBuddy or ASM only where no public seam exists), javap from the "
                  "bundled JDK for API facts, a decompiler (CFR, Vineflower) to read the logic",
    "Santa Monica Studio Kinetica": "a version.dll proxy hooking IFFProcessLuaScriptChunk to dump and override the uncompressed "
                                    "Lua 5.2 scripts in .wad files (Nukem9/godofwar-gameplay-tweaks), the exe's built-in "
                                    "socket.core (LuaSocket TCP/UDP) and cmsgpack in luaL_openlibs, and the MIT-licensed "
                                    "skeleton/input/camera hooks in adore11s/God-of-War-VR-Mod",
}


def detect_engine(game_dir):
    root = Path(game_dir)
    if not root.is_dir():
        raise ValueError(f"not a folder: {game_dir}")
    files, dirs, exes, count = {}, set(), [], 0
    for dirpath, dirnames, filenames in os.walk(root):
        depth = len(Path(dirpath).relative_to(root).parts)
        if depth >= 4:
            dirnames[:] = []
        for d in dirnames:
            dirs.add(d.lower())
        for n in filenames:
            files.setdefault(n.lower(), os.path.join(dirpath, n))
            if n.lower().endswith(".exe") and depth <= 2:
                exes.append(str(Path(dirpath, n).relative_to(root)))
            count += 1
        if count > 60000:
            break
    hits = []
    for engine, family, test in SIGNATURES:
        try:
            ev = test(files, dirs)
        except Exception:
            ev = None
        if ev:
            hits.append({"engine": engine, "family": family, "evidence": ev})
    best = hits[0] if hits else None
    related = []
    if best and best["family"] and yaml is not None:
        keys = _family_keys(best["family"])
        related = [compact(e) for e in directory() if _same_family(e.get("engine"), keys)][:15]
    extras = {
        "steam_api": _has(files, "steam_api.dll", "steam_api64.dll"),
        "steam_appid": _has(files, "steam_appid.txt"),
        "eos": _has(files, "eossdk-win64-shipping.dll", "eossdk-win32-shipping.dll"),
        "anticheat_files": _has(files, "easyanticheat.exe", "easyanticheat_x64.dll", "beclient_x64.dll", "battleye"),
    }
    return {
        "folder": str(root),
        "files_scanned": count,
        "executables": exes[:10],
        "best_guess": best,
        "all_matches": hits,
        "hooking_tools": HOOK_TOOLS.get(best["engine"]) if best else "unknown engine: start with playbook/reverse-engineering.md",
        "other_signals": {k: v for k, v in extras.items() if v},
        "anticheat_note": "Anti-cheat files found: co-op work must stay offline and must not touch anti-cheat." if extras["anticheat_files"] else None,
        "same_engine_projects": related,
        "next": "Read playbook/engine-families.md for this family, then search_directory for the game itself.",
    }


# ---------------------------------------------------------------- exe identity

VERSION_KEYS = ("CompanyName", "ProductName", "FileVersion", "ProductVersion", "OriginalFilename")


def _version_info(data):
    """Version numbers and strings from the exe's VS_VERSION_INFO resource, or {} without one."""
    key = data.find("VS_VERSION_INFO\0".encode("utf-16-le"))
    if key < 6:
        return {}
    block = key - 6  # wLength, wValueLength, wType come before the key
    end = block + struct.unpack_from("<H", data, block)[0]
    out = {}
    fixed = data.find(struct.pack("<I", 0xFEEF04BD), key, min(end, key + 128))
    if fixed >= 0:
        fms, fls, pms, pls = struct.unpack_from("<IIII", data, fixed + 8)
        out["file_version"] = f"{fms >> 16}.{fms & 0xFFFF}.{fls >> 16}.{fls & 0xFFFF}"
        out["product_version"] = f"{pms >> 16}.{pms & 0xFFFF}.{pls >> 16}.{pls & 0xFFFF}"
    strings = {}
    for name in VERSION_KEYS:
        at = data.find((name + "\0").encode("utf-16-le"), key, end)
        if at < 6:
            continue
        chars = struct.unpack_from("<H", data, at - 4)[0]  # wValueLength, in UTF-16 units
        value_at = (at + 2 * (len(name) + 1) + 3) & ~3
        value = data[value_at:value_at + 2 * chars].decode("utf-16-le", "replace").rstrip("\0").strip()
        if value:
            strings[name] = value
    if strings:
        out["strings"] = strings
    return out


def _steam_build(exe):
    """App id and build id from the appmanifest of the Steam library the exe is installed in."""
    parts = [x.lower() for x in exe.parts]
    for i in range(len(parts) - 2):
        if parts[i] == "steamapps" and parts[i + 1] == "common":
            library, installdir = Path(*exe.parts[:i + 1]), parts[i + 2]
            for acf in sorted(library.glob("appmanifest_*.acf")):
                kv = {}
                for k, v in re.findall(r'^\s*"(\w+)"\s+"([^"]*)"', acf.read_text(encoding="utf-8", errors="replace"), re.M):
                    kv.setdefault(k.lower(), v)  # top-level keys come first
                if kv.get("installdir", "").lower() == installdir:
                    return {"appid": kv.get("appid"), "buildid": kv.get("buildid"), "name": kv.get("name"),
                            "manifest": acf.name}
    return None


def game_identity(exe_path):
    p = Path(exe_path)
    data = p.read_bytes()
    out = {"file": p.name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    steam = _steam_build(p.resolve())
    if steam:
        out["steam"] = steam
    try:
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] == b"PE\0\0":
            machine, nsect, stamp = struct.unpack_from("<HHI", data, pe + 4)
            out["machine"] = {0x14C: "x86 (32-bit)", 0x8664: "x64", 0xAA64: "arm64"}.get(machine, hex(machine))
            out["pe_timestamp"] = stamp
            opt = pe + 24
            out["pe_format"] = {0x10B: "PE32", 0x20B: "PE32+"}.get(struct.unpack_from("<H", data, opt)[0], "?")
            names = [data[off:off + 8].rstrip(b"\0").decode("latin-1") for off in
                     (opt + struct.unpack_from("<H", data, pe + 20)[0] + 40 * i for i in range(nsect))]
            out["sections"] = names
            packed = [n for n in names if n.lower() in (".bind", "upx0", "upx1", ".vmp0", ".themida", ".enigma1")]
            if packed:
                out["note"] = f"Packed or DRM-wrapped sections {packed}: analyse an unpacked copy only if that is legal for you; never publish DRM removal."
            out.update(_version_info(data))
    except Exception as e:  # not a PE or truncated
        out["pe_error"] = str(e)
    return out


# ---------------------------------------------------------------- tools

def t_search_directory(query="", status=None, qa=None, engine=None, limit=25):
    q = (query or "").lower()
    res = []
    for e in directory():
        hay = " ".join(str(e.get(k, "")) for k in ("game", "project", "engine", "notes")).lower()
        if q and not all(w in hay for w in q.split()):
            continue
        if status and e.get("status") != status:
            continue
        if qa and e.get("qa") != qa:
            continue
        if engine and engine.lower() not in str(e.get("engine", "")).lower():
            continue
        res.append(compact(e))
    return {"count": len(res), "entries": res[: int(limit)],
            "hint": "No match does not mean nothing exists: run the coop-research skill and add what you find."}


def t_list_docs():
    pb = sorted(p.stem for p in (ROOT / "playbook").glob("*.md"))
    games = sorted(p.name for p in (ROOT / "games").iterdir() if p.is_dir())
    skills = sorted(p.name for p in (ROOT / ".agents" / "skills").iterdir() if p.is_dir())
    test_tools = sorted(p.parent.name for p in (ROOT / "tools").glob("*/README.md"))
    return {"playbook": pb, "games": games, "skills": skills, "test_tools": test_tools,
            "start": "AGENTS.md"}


def t_read_playbook(page):
    page = page.replace(".md", "")
    if page.upper() == "AGENTS":
        return read("AGENTS.md")
    return read(f"playbook/{page}.md")


def t_get_skill(name):
    return read(f".agents/skills/{name}/SKILL.md")


def t_get_game(slug):
    return read(f"games/{slug}/README.md")


def t_get_test_tool(name):
    return read(f"tools/{name}/README.md")


def t_validate_entry(entry_yaml):
    from build_directory import errors_for
    e = yaml.safe_load(entry_yaml)
    if isinstance(e, list):
        e = e[0]
    errs = errors_for(0, e)
    return {"valid": not errs, "errors": errs,
            "next": "Append to data/directory.yaml, run python tools/build_directory.py, then follow the coop-contribute skill."}


TOOLS = {
    "search_directory": (t_search_directory, "Search the Co-op Anything directory of co-op mods, forks, ports and open gaps by game, project, engine or keyword. Returns what each project really syncs, its status and its QA level.",
                         {"query": ("string", "game, project, engine or keyword; empty lists everything"),
                          "status": ("string", "optional: playable, partial, alpha, prototype, abandoned, official, none"),
                          "qa": ("string", "optional: none, community-reports, automated, verified-playthrough"),
                          "engine": ("string", "optional engine filter, e.g. Unity, Unreal Engine 1"),
                          "limit": ("integer", "max results, default 25")}, []),
    "detect_engine": (detect_engine, "Scan a local game install folder (read-only) and identify its engine from file signatures, with the evidence, suggested hooking tools, anti-cheat warnings and co-op projects on the same engine.",
                      {"game_dir": ("string", "absolute path of the game install folder")}, ["game_dir"]),
    "game_identity": (game_identity, "SHA-256, size, architecture, PE details and version resource of a game executable, plus the Steam app id and build id when it sits in a Steam library, to record the exact build before following any write-up.",
                      {"exe_path": ("string", "absolute path of the game exe")}, ["exe_path"]),
    "list_docs": (t_list_docs, "List the playbook pages, game write-ups, agent skills and sync test tools available in Co-op Anything.", {}, []),
    "read_playbook": (t_read_playbook, "Read a playbook page (e.g. triage, research, reverse-engineering, tools, how-coop-mods-work, networking, automated-qa) or AGENTS.",
                      {"page": ("string", "page name without .md, or AGENTS")}, ["page"]),
    "get_skill": (t_get_skill, "Read an agent skill by name, starting with coop-triage (list_docs lists them all).",
                  {"name": ("string", "skill name")}, ["name"]),
    "get_game": (t_get_game, "Read the write-up for a game that has been worked on (see list_docs for slugs).",
                 {"slug": ("string", "folder name under games/")}, ["slug"]),
    "get_test_tool": (t_get_test_tool, "Read the README of a co-op sync test tool (chaos, flight, fuzz, ownership, fakeworld, gameslot, gamepool, scenarios, bots, animcheck, bridge, inspect, hotreload, mutants, buildlock; list_docs lists them).",
                      {"name": ("string", "folder name under tools/")}, ["name"]),
    "validate_entry": (t_validate_entry, "Check a new or edited directory entry (YAML text) against the schema before contributing it.",
                       {"entry_yaml": ("string", "one entry as YAML")}, ["entry_yaml"]),
}


def tool_list():
    out = []
    for name, (_, desc, props, req) in TOOLS.items():
        out.append({"name": name, "description": desc, "inputSchema": {
            "type": "object", "properties": {k: {"type": t, "description": d} for k, (t, d) in props.items()},
            "required": req}})
    return out


# ---------------------------------------------------------------- JSON-RPC loop

def handle(msg):
    method, mid, params = msg.get("method"), msg.get("id"), msg.get("params") or {}
    if method == "initialize":
        return {"protocolVersion": params.get("protocolVersion", PROTOCOL),
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "coop-anything", "version": VERSION},
                "instructions": "Co-op Anything: shared knowledge for making single-player games co-op. "
                                "Start with get_skill coop-triage, use detect_engine on the owner's install folder, "
                                "search_directory before claiming nothing exists, and send findings back with the coop-contribute skill."}
    if method == "tools/list":
        return {"tools": tool_list()}
    if method == "tools/call":
        name, args = params.get("name"), params.get("arguments") or {}
        if name not in TOOLS:
            raise ValueError(f"unknown tool {name}")  # MCP reports unknown tools as invalid params
        try:
            result = TOOLS[name][0](**args)
            text = result if isinstance(result, str) else json.dumps(result, indent=1, ensure_ascii=False, default=str)
            return {"content": [{"type": "text", "text": text}], "isError": False}
        except Exception as e:
            return {"content": [{"type": "text", "text": f"{type(e).__name__}: {e}"}], "isError": True}
    if method == "ping":
        return {}
    if mid is None:
        return None  # notification
    raise KeyError(f"method not found: {method}")


def main():
    sys.stdin.reconfigure(encoding="utf-8")
    sys.stdout.reconfigure(encoding="utf-8")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        try:
            result = handle(msg)
            if msg.get("id") is None:
                continue
            reply = {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        except Exception as e:
            if msg.get("id") is None:
                continue
            code = -32601 if isinstance(e, KeyError) else -32602 if isinstance(e, ValueError) else -32603
            reply = {"jsonrpc": "2.0", "id": msg["id"], "error": {"code": code, "message": str(e.args[0]) if isinstance(e, KeyError) and e.args else str(e)}}
        sys.stdout.write(json.dumps(reply, ensure_ascii=False) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
