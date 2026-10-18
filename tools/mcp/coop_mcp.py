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
    keep = ("game", "year", "engine", "project", "url", "kind", "status", "coop_scope",
            "players", "networking", "open_source", "last_activity", "qa", "notes", "checked")
    return {k: (str(e[k]) if k in ("checked", "last_activity") else e[k]) for k in keep if k in e}


# ---------------------------------------------------------------- engine detection

# (engine, family hint for playbook/engine-families.md, test(files, dirs) -> evidence or None)
def _has(files, *names):
    return [n for n in names if n.lower() in files]


def _ext(files, *exts):
    return sorted({f for f in files if any(f.endswith(x) for x in exts)})[:5]


SIGNATURES = [
    ("Unreal Engine 1 or 2", "Unreal Engine 1",
     lambda f, d: (_has(f, "core.u", "engine.u") or None) and _has(f, "core.u", "engine.u", "engine.dll", "core.dll")),
    ("Unreal Engine 3", "Unreal Engine 3", lambda f, d: _ext(f, ".upk", ".xxx") or (["cookedpc"] if "cookedpc" in d else None)),
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
    ("RE Engine", "RE Engine", lambda f, d: _ext(f, "re_chunk_000.pak")),
    ("id Tech 4", "id Tech 2 to 4", lambda f, d: _ext(f, ".pk4")),
    ("id Tech 3 (or derived)", "id Tech 2 to 4", lambda f, d: _ext(f, ".pk3")),
    ("Quake / id Tech 2", "id Tech 2 to 4", lambda f, d: _has(f, "pak0.pak") if ("id1" in d or "baseq2" in d) else None),
    ("Doom engine (id Tech 1)", "id Tech 2 to 4", lambda f, d: _has(f, "doom.wad", "doom2.wad", "heretic.wad", "hexen.wad")),
    ("Build engine", None, lambda f, d: _ext(f, ".grp")),
    ("REDengine 4", None, lambda f, d: _ext(f, ".archive") if "archive" in d else None),
    ("LithTech", None, lambda f, d: _ext(f, ".rez")),
    ("Godot", None, lambda f, d: _ext(f, ".pck")),
    ("GameMaker", None, lambda f, d: _has(f, "data.win")),
    ("RPG Maker", None, lambda f, d: _ext(f, ".rgss3a", ".rgss2a", ".rgssad") or _has(f, "rpg_core.js", "rmmz_core.js")),
    ("XNA / FNA / MonoGame", None, lambda f, d: _has(f, "fna.dll", "monogame.framework.dll", "microsoft.xna.framework.dll")),
    # Java games ship a .jar plus a bundled runtime (jre or jdk folder), or a launcher that starts java.exe.
    ("Java (JVM)", "Java",
     lambda f, d: _ext(f, ".jar") if ("jre" in d or "jdk" in d or _has(f, "java.exe", "javaw.exe")) else None),
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
    "Java (JVM)": "a -javaagent (java.lang.instrument) loaded into the unmodified game JVM, the game's own public classes "
                  "and event bus as hook points (ByteBuddy or ASM only where no public seam exists), javap from the "
                  "bundled JDK for API facts, a decompiler (CFR, Vineflower) to read the logic",
}


def detect_engine(game_dir):
    root = Path(game_dir)
    if not root.is_dir():
        raise ValueError(f"not a folder: {game_dir}")
    files, dirs, exes, count = set(), set(), [], 0
    for dirpath, dirnames, filenames in os.walk(root):
        depth = len(Path(dirpath).relative_to(root).parts)
        if depth >= 4:
            dirnames[:] = []
        for d in dirnames:
            dirs.add(d.lower())
        for n in filenames:
            files.add(n.lower())
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
        keys = [k.strip().lower() for k in best["family"].replace(" to ", "/").split("/")]
        related = [compact(e) for e in directory() if any(k and k in str(e.get("engine", "")).lower() for k in keys)][:15]
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

def game_identity(exe_path):
    p = Path(exe_path)
    data = p.read_bytes()
    out = {"file": p.name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
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
    return {"playbook": pb, "games": games, "skills": skills, "start": "AGENTS.md"}


def t_read_playbook(page):
    page = page.replace(".md", "")
    if page.upper() == "AGENTS":
        return read("AGENTS.md")
    return read(f"playbook/{page}.md")


def t_get_skill(name):
    return read(f".agents/skills/{name}/SKILL.md")


def t_get_game(slug):
    return read(f"games/{slug}/README.md")


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
    "game_identity": (game_identity, "SHA-256, size, architecture and PE details of a game executable, to record the exact build before following any write-up.",
                      {"exe_path": ("string", "absolute path of the game exe")}, ["exe_path"]),
    "list_docs": (t_list_docs, "List the playbook pages, game write-ups and agent skills available in Co-op Anything.", {}, []),
    "read_playbook": (t_read_playbook, "Read a playbook page (e.g. triage, research, reverse-engineering, tools, how-coop-mods-work, networking, automated-qa) or AGENTS.",
                      {"page": ("string", "page name without .md, or AGENTS")}, ["page"]),
    "get_skill": (t_get_skill, "Read an agent skill (coop-triage, coop-research, coop-reverse-engineer, coop-revive, coop-build-from-zero, coop-networking, coop-qa, coop-contribute).",
                  {"name": ("string", "skill name")}, ["name"]),
    "get_game": (t_get_game, "Read the write-up for a game that has been worked on (see list_docs for slugs).",
                 {"slug": ("string", "folder name under games/")}, ["slug"]),
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
            raise KeyError(f"unknown tool {name}")
        try:
            result = TOOLS[name][0](**args)
            text = result if isinstance(result, str) else json.dumps(result, indent=1, ensure_ascii=False)
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
            reply = {"jsonrpc": "2.0", "id": msg["id"], "error": {"code": -32601 if isinstance(e, KeyError) else -32603, "message": str(e)}}
        sys.stdout.write(json.dumps(reply, ensure_ascii=False) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
