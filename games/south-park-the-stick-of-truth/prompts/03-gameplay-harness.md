# Prompt 03 — Gate 2: gameplay-state harness

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/automated-qa.md`, `playbook/multi-instance-testing.md` and `playbook/methods.md`.

## Coordination (read this before you start)

You are **step 2 of 7** (order: `../prompts/README.md`); this prompt is `prompts/03-gameplay-harness.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step02 -b game/south-park-the-stick-of-truth-step02-harness game/south-park-the-stick-of-truth
cd ../sp-step02
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/03-gameplay-harness.md`, `tools/in_game.py`, `tools/capture_game_window.py`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step02 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step02
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Why

No agent can currently prove anything about gameplay. Three facts block every other gate:

- the owner's profile **does** contain saves (`save\Save Slot 001–008.sav`, `Checkpoint 1–3.sav`); `-startsavegame` was never tested (corrected 2026-11-15);
- `PrintWindow` returns a black frame for this title (this copy runs DX9, `use_directx_11 = false`), so the harness
  cannot see the screen;
- no "am I in gameplay?" signal exists, so a bot cannot tell a menu from a level.

## What is already known

- The main menu is reachable under the guarded harness (readable 1280x720 screenshot exists from
  the 2026-11-04 session, taken with the foreground `BitBlt` fallback in
  `../tools/capture_game_window.py`, not with `PrintWindow`).
- Candidate ways into a level: console `startlevel <level> …` / command line
  `-startlevel world\levels\1_southpark\2_hub.opf 0 0 0 0 0 0 0` (tried, unverified);
  `-load`; starting a new game through the menu.
- `data\movies` ships loose and `data\global` is empty in the retail install, so the `data/`
  directory layer is read — a mod seam, useful later.
- Guard rules: every launch goes through `../tools/run_net_probe.py` / `guard_session.py`; a run
  is valid only if `original_profile_unchanged`, `guard_integrity` and
  `original_exe_hash_still_matches` are `true`.

## Deliverables

1. **A capture that works every time.** Replace or wrap the capture: foreground `BitBlt` of the
   client area when the window can be focused, `PrintWindow` as a fallback, and a
   `nonblack_ratio` / perceptual hash so two frames can be compared. Never call a picture "the
   menu" without a human-checkable artefact (keep the PNG in the run folder and reference it).
2. **A gameplay detector.** Something a bot can assert on, for example: the engine's
   `ui_screen_active` / `conversation_active` cvars, the currently loaded level name read from
   memory (`level_debug` prints it; the AMX per-level scripts are loaded), or the presence of a
   `Player_Object`. Cross-check at least two independent signals before returning `in_game: true`.
3. **A path from launch to gameplay.** Menu automation is acceptable if it is idempotent and
   verifiable (clicks at fixed coordinates are fine on a fixed 1280x720 window as long as the
   screenshot confirms the state). Starting a new game will create a save: prove the new files
   land in the redirected test profile and **not** in the retail profile.
4. `../tools/in_game.py` with `start_run(save=None, level=None) -> {pid, in_game, level,
   screenshot, evidence}` and a `--once` CLI that prints a JSON summary.

## Test it can fail

Show that the detector returns `false` on the menu and `true` in gameplay, in two separate runs,
and show what a failed run looks like. Add the two run folders to `../notes/03-gameplay-harness.md`.

## Do not

- Do not create saves in the retail profile, and do not use Steam Cloud.
- Do not "fix" the black frame by copying pixels from the desktop when the window is not
  foreground — that produced misleading results earlier.
- Do not require a human to click anything; the whole point is a bot path.
