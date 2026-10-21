# The Co-op Anything MCP server

`tools/mcp/coop_mcp.py` gives any MCP-capable agent the shared knowledge in this repo plus a
few local, read-only analysis tools. It needs Python 3.9+ and PyYAML (`pip install pyyaml`),
and nothing else. It reads your clone, so `git pull` updates it.

| Tool | What it does |
|---|---|
| `search_directory` | Search the directory of co-op projects and gaps by game, engine, status or QA level, with each entry's sources so its claims can be checked |
| `detect_engine` | Scan a game install folder and identify the engine from file signatures (archive headers where an extension is shared), with evidence, hooking tools, anti-cheat warnings and co-op projects on the same engine |
| `game_identity` | SHA-256, architecture, PE details and version resource of the game exe, plus the Steam app id and build id for Steam installs, to record the exact build |
| `list_docs`, `read_playbook`, `get_skill`, `get_game` | Read the playbook, the agent skills and the game write-ups |
| `get_test_tool` | Read the README of a sync test tool in `tools/` (chaos box, flight recorder, fuzzer, bots and more) |
| `validate_entry` | Check a directory entry against the schema before contributing it |

Pair it with a reverse-engineering MCP such as [REA](https://github.com/morluto/rea) for the
binary work: Co-op Anything knows what to do, REA looks inside the game.

## Setup

**Claude Code:** open this repo and the project's `.mcp.json` offers the server automatically.
From anywhere else (`-s user` makes it available in every folder, not only the current one):
```
claude mcp add -s user coop-anything -- python <path to clone>/tools/mcp/coop_mcp.py
```

The `coop-*` skills live in `.claude/skills/` of this repo, so Claude Code only sees them while
it runs inside the clone. To use them while working in a game folder, link them into your user
skills folder; `git pull` then updates them too. Paths inside the skills (`local/`, `playbook/`,
`games/`) stay relative to the clone. `triage-contributions` is the maintainers' skill and is
left out on purpose.

Windows, in `cmd` (a directory junction needs no admin rights):
```
for /d %s in ("<path to clone>\.claude\skills\coop-*") do mklink /J "%USERPROFILE%\.claude\skills\%~nxs" "%s"
```
Linux and macOS:
```
for s in <path to clone>/.claude/skills/coop-*; do ln -s "$s" ~/.claude/skills/; done
```

**Codex** (`~/.codex/config.toml`):
```toml
[mcp_servers.coop-anything]
command = "python"
args = ["<path to clone>/tools/mcp/coop_mcp.py"]
```

**Gemini CLI** (`.gemini/settings.json`) and **Cursor** (`.cursor/mcp.json`):
```json
{ "mcpServers": { "coop-anything": { "command": "python", "args": ["<path to clone>/tools/mcp/coop_mcp.py"] } } }
```

Use `python3` instead of `python` on Linux and macOS.

After changing the server, run its tests: `python -m unittest tools/mcp/test_coop_mcp.py`.

## Ask your agent

```
Use coop-anything: detect the engine of my <game> install at <folder>, check what co-op work
exists for it and for its engine, and tell me which path (revive or build from zero) fits.
```
