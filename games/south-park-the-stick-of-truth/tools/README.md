# tools — guarded test harness for South Park: The Stick of Truth

Research tooling used for the 2026-11-04 and 2026-11-15 sessions. It is not a mod,
not a launcher for players, and it does not touch the retail install.

| File | What it does |
|---|---|
| `launch_lock.py` | Machine-wide lease so only one agent launches the game at a time (`acquire` / `refresh` / `release` / `status`). Required by `PARALLEL.md`. |
| `save_guard.js` | Frida script loaded before the game resumes. Redirects the game's profile/save paths into a run folder, denies writes outside an allowlist, blocks child processes and deletes. Optional `logPaths` prints every open whose path contains a string. |
| `guard_session.py` | Spawns the game suspended under Frida, loads the guard plus optional extra scripts, resumes, keeps it alive for `duration`, then kills it and returns telemetry. |
| `net_probe.js` | Read-only hooks on `ws2_32.dll` (`WSAStartup`, `socket`, `bind`, `listen`, `accept`, `connect`, `send*`, `recv*`, ...) and `steam_api.dll` (`SteamAPI_Init`, `SteamNetworking`, `SteamMatchmaking`, ...). Counts calls, parses `sockaddr`, stores fuzzy backtraces of the first calls. |
| `capture_game_window.py` | PNG capture of the game's client area. `PrintWindow` returns a black frame for this title; the fallback foreground `BitBlt` is what produced the readable menu shot. |
| `run_net_probe.py` | Glue: hash check, profile snapshot/backup, console-script candidates, guarded launch, net telemetry, probe dump detection, `result.json`. |
| `in_game.py` | One call from launch to a loaded level, and a detector that can fail: it clicks through the front end, measures the screen and the working set, and returns `in_game` only when both agree. `--replay` re-decides a stored run. |
| `guard_multi.py` | Two-instance stand: two guarded copies side by side with separate profiles, one lock, separate result folders. |
| `console_exec.py` | The console/`Frida` bridge: guarded launch, hook the console print, call the engine's `exec` handler with a script file, read the engine's own output; also live-`.text` string/xref scans, `call_handler`, `call_void` and net-event counters. `ok` means "the engine ran it and answered", never "the command succeeded". |
| `rtti_index.py` | Static RTTI/vtable index of the hash-pinned exe: type descriptors, COLs, vtables, base lists, focus names; writes `rtti_index.json`. `--self-test` checks the demangler. |

## Requirements

- Windows x86 build of the game in a **copy** of the install.
- Python 3.x with a pinned Frida wheel installed **without dependencies** into a
  local folder that is added to `sys.path` (`guard_session.py` expects `<cwd>/runtime-libs`).
- The owner's approval for every launch: the game starts through Steam and uses their account.

## Run

Set `SP_GAME_DIR` to the path of the **copy** of the install (defaults to the previous
session's `D:\coop-work\game-test`), then:

```
py -3.14 -B run_net_probe.py          # menu-only run, ~50 s
py -3.14 -B run_net_probe.py --exec   # additionally passes -exec probe_console
py -3.14 -B run_net_probe.py --no-lock  # only when you already hold the launch lock
```

The runner takes the machine-wide launch lock itself (`SP_AGENT` names the owner,
`SP_LOCK_WAIT` sets how long to wait, default 1800 s) and releases it after the run.

Each run creates `game-runs/<uuid>/` with `before.json`, `after.json`, `result.json`,
`original-profile-backup/`, `test-profile/` and `game-window.png`. A run is only a
valid result if `original_profile_unchanged`, `guard_integrity` and
`original_exe_hash_still_matches` are all `true` in `result.json`.

## Limits found the hard way

- The guard is not an OS sandbox: it only redirects the paths it is given and denies
  unknown writes that pass through `NtCreateFile`/`NtOpenFile`/`NtWriteFile`.
- Steam must be allowed to start normally (SteamAppId/GameId env vars), otherwise the
  guard blocks the child process and the game dies before the menu.
- `PrintWindow` yields black frames here; use the foreground `BitBlt` fallback, which
  needs the game window focused and visible.
- The retail profile folder does contain saves (in `save\`); earlier notes said it had none because
  the check looked at the wrong folder.
- `-startsavegame` does **not** load a save in this build: two guarded runs (`Save Slot 001` and
  `Save Slot 001.sav`) stayed in the front end for 84 s. The reliable way into a level is the menu
  click on Continue, which is what `in_game.py` does.
- Console commands are **not registered** in this build: `exec` runs a script and echoes each line,
  then answers `unknown var` for `find`, `status`, `write_file`. Calling `restart_server` or
  `connect` directly kills the process.
- `.text` is Steam-CEG protected and decrypted page by page, so live disassembly of `call` targets is
  not trustworthy, and RVAs read from a live listing can point into `.rdata`. Use behaviour, counters
  and memory values as evidence.
