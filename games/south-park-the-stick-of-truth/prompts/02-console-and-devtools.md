# Prompt 02 — Gate 1: console and dev access

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/reverse-engineering.md`.

## Coordination (read this before you start)

You are **step 3 of 7** (order: `../prompts/README.md`); this prompt is `prompts/02-console-and-devtools.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step03 -b game/south-park-the-stick-of-truth-step03-console game/south-park-the-stick-of-truth
cd ../sp-step03
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/02-console-and-devtools.md`, `tools/console_exec.py`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step03 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step03
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Why

The exe ships a full Onyx console with the help text intact (`console_toggle`, `exec`, `find`,
`write_file`, `revert`, `status`, `startlevel`, `startsavegame`, `god <player_index>`,
`warp_to_player`, `connect`, `restart_server`, `kick <player_index>`) and cvar names including
`ui_screen_active`, `level_debug`, `conversation_active`, `multiplayer_*`. Every later gate is
cheap if an agent can execute one console command and read its output. Today nobody can.

## What is already known (do not redo)

- `data\global\console_scripts` is opened once as a **directory** at startup, but no `.txt` inside
  it was ever opened, with four candidate names present (`platform_win.txt`, `init.txt`,
  `console.txt`, `autoexec.txt`) in both `<game>\global\console_scripts` and
  `<game>\data\global\console_scripts`.
- `-exec probe_console` produced no side effect, and the `write_file <name>` oracle never wrote
  anything, so **execution is unproven** — the oracle may be wrong as easily as the trigger.
- Strings in the exe: `global/console_scripts`, `%s/%s`, `%s.txt`, `con_vars.txt`,
  `unable to exec missing file %s`, `unable to write to unsafe filename "%s"`, `find: no matches
  for '%s'`, `usage: exec filename`, and the `con_echo_command 0/1` pair.

## Do, in this order

1. **Find the real filename.** Improve the file logger in `../tools/save_guard.js` (`logPaths`)
   so it records every distinct open and also `NtQueryDirectoryFile` results under
   `data\global\console_scripts`. Create a wide set of candidate names at once
   (`platform_win`, `windows`, `win32`, `pc`, `default`, `init`, `startup`, `autoexec`, `console`,
   `game`, `user`, `dev`, `cheats`, `mp`, `net`) and see which is asked for. A name that is opened
   is the answer.
2. **Find the key binding.** Extract `gui/*.gui` and `gui/*.gfx` from `data_archive.oaf` (the
   previous session left an extractor in `local/`; rewrite it if absent — OAF index parsing is
   documented in `../notes/02-console-and-devtools.md`) and grep for `console_toggle` / `console_open`. If the binding is
   a key, poke it and screenshot with the foreground `BitBlt` path in
   `../tools/capture_game_window.py` (PrintWindow returns black).
3. **Find the dispatcher and call it.** The command table contains name, description and handler
   pointers. The naïve scan (dword equal to the string VA in `.rdata`/`.data`) found nothing for
   `connect`, `restart_server`, `write_file` or `startlevel`, while `status` had two hits — so the
   table is not a flat `{name, desc, handler}` array. Try: pointers into the length-prefixed string
   start (`va-1`/`va-2`), RVA-based pointers, `{name_hash, handler}` records, or a `std::map`
   with a comparison functor. At runtime, a reliable shortcut is to hook the console *print*
   function (find it by the `%s` usage strings) and watch what the engine prints; then work
   backwards from a command you can trigger.
4. **Make an oracle that cannot lie.** Good oracles: a file created on disk in a path you control,
   a counter in the probe, a visible console line in a screenshot, a cvar change readable through
   `SteamUserStats`-free means. Bad oracle: "no error appeared".

## Deliverable

- `../tools/console_exec.py` (or equivalent): one call `run_commands([...]) -> {ok, output, evidence}`
  that works on the isolated copy, with the mechanism documented (filename autoload, key press,
  or hooked dispatcher).
- A `../notes/02-console-and-devtools.md` section listing, as three separate lists: what was observed to run, what was
  inferred, and what is still unknown — including the exact console-command list you could
  actually execute.
- If the console is genuinely unreachable, say so and give the next cheapest alternative
  (hooked dispatcher call, or a memory patch applied only to the test copy).

## Do not

- Do not distribute a patched exe, and do not patch the retail install.
- Do not use `local/test_startlevel_full.ps1` (it kills every process whose name matches "South").
- Do not claim a command ran because the game did not crash.
