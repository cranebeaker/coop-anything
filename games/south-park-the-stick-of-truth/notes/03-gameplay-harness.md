# Step 2 — Gate 2: gameplay-state harness

This file belongs to one agent step. Do not edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`
or `data/` — the integrator folds this file into them once no other agent is running
(see `../PARALLEL.md`).

- Prompt: `prompts/03-gameplay-harness.md`
- Agent: step02 (subagent chat, DeepSeek V4.1 Flash)
- Branch / worktree: `game/south-park-the-stick-of-truth-step02-harness` in `../sp-step02`
- Launch lock held: 2026-11-15 **11:23:23 – 11:43:14** (owner `step02`), after waiting ~30 s for
  `step03` to release it. Three launch series, back to back, ~90 s each.

## Verdict

**Gate 2 is answered "yes, with one caveat": a bot can reach a loaded level and prove it, and the
detector was shown returning `false` on the front end and `true` in the level, in two runs.**
What is *not* shown is that the New Kid is under player control in the state we reach — the level
renders and idles, and holding `W` for 2 s changed nothing measurable. Details below.

The first version of the detector **lied** (`in_game: true` on the main menu). It is kept in the
record as the failure case, and the report says why it lied.

## Observed (ran and saw)

Three guarded launches, all under `tools/launch_lock.py` and `tools/guard_session.py`, all on the
copy `<game dir>` = `%SP_GAME_DIR%` (default `D:\coop-work\game-test`). In every run
`original_profile_unchanged`, `guard_integrity` and `original_exe_hash_still_matches` were `true`,
and no new save was written into the test profile.

| # | started | run folder (inside this worktree, git-ignored `local/`) | what it shows |
|---|---|---|---|
| 1 | 11:23:27, pid 16072 | `local/game-runs/in-game-39eaae080fc640519e4c6f9ec5af3069` | plain `-window`; legal screen → main menu → (no input) a loaded level at 72 s. First detector said `true` at the **menu**. |
| 2 | 11:29:07, pid 7072 | `local/game-runs/in-game-83f32f95998b4cfc8811e1e098bd0b3e` | menu at 42 s, **click "Continue" at 47 s**, level from 56 s. Detector `false` at 42 s, `true` at 56/70/84 s. |
| 3 | 11:37:23, pid 15908 | `local/game-runs/in-game-2ac6bd9dd1eb4591b90300d5c70d6911` | decisive run with the final detector: menu 30/42 s → click 47 s → level 56/70/92 s; `W` held 2 s at 87 s. |

Run 3 verdict history, exactly as printed (`--replay`):

```
at 12.1  in_game=False screen=unknown   (legal screen)
at 30.1  in_game=False screen=front_end (main menu)
at 42.1  in_game=False screen=front_end (main menu)
at 56.1  in_game=True  screen=level
at 70.1  in_game=True  screen=level
at 92.1  in_game=True  screen=level     (after the W hold)
```

- **The owner's profile is not empty.** `%USERPROFILE%\Documents\My Games\South Park - The Stick
  of Truth\save\` holds `Save Slot 001..008.sav` (08–09.07.2025) and `Checkpoint 1..3.sav`
  (14.07.2025), plus `EOptions.opt`/`GOptions.opt`. The same files are present in
  `original-profile-backup/save/` of *every* run folder from the 2026-11-04 and 2026-11-15
  sessions, so they existed all along: the earlier "no save files exist" check looked at the top
  level of the profile folder, not the `save\` subfolder. The main menu confirms it — "Продолжить"
  (Continue) is enabled and highlighted.
- **The menu, read from a captured frame** (run 1 `frames/frame-048.png`, run 2/3 `frame-042.png`):
  South Park logo, `30 FPS` top-left, right-hand column `Продолжить / Новая игра / Загрузить игру /
  Загружаемый контент / Настройки / Выход из игры`. This copy of the game is **Russian-localized**
  (`data_localized_ru.oaf`), so click targets are text-based; the harness therefore clicks
  *fractions* of the client area, not pixel offsets.
- **Clicking "Продолжить"** at client (1080, 473) = fraction (0.844, 0.657) started the game:
  frame at 47.2 s shows the click landing (`screen: [1400, 640]`), frame at 56.1 s is the loaded
  level, i.e. ~9 s from click to level.
- **The level that loads** (frames `frame-056/070/092.png`): the New Kid in the wizard hat sitting in
  front of a TV in his bedroom, `30 FPS`, an in-game toast "Доступно обновление" ("update
  available"), small particle animations. The same scene appears in run 1 (`frame-072.png`, darker)
  and run 2 (`frame-056.png`, brighter) — it is **engine-rendered**, not a Bink movie: in runs 2 and
  3 the guard's file log shows only the four startup logo movies (`000_autodesk`, `000_obsidian`,
  `000_spds`, `000_ubisoft`.bk2) and no other `.bk2` open.
- **Holding `W` for 2 s** at 87 s in run 3 did nothing visible: `mean_luma` 119.63 → 119.52 and an
  identical dHash (`000e1c39ed2b1851` before and after). So the state is a loaded level, but "the
  player responds to input" is **not** observed.
- **Capture.** Foreground `BitBlt` of the client area produced readable 1280×720 frames in 17 of 19
  samples. The two failures (run 1, 12.6 s and 36.6 s) were genuinely black screens: `PrintWindow`
  was then tried and returned `nonblack = 0.0`, i.e. the known black-`PrintWindow` behaviour
  reproduces. Frames are PNGs in each run folder; no game pixels are committed.
- **Engine-side numbers that separate the states** (same process, run 3): working set 455.8 MB at
  the menu frame (42.1 s) → 489.6/490.8/490.6 MB in the level (56.1/70.1/92.1 s). Run 2: 455.5 →
  489.8 MB. CPU was ~1.8–2.1 cores in both states, so CPU alone does not separate them.
- **Two engine signals that do *not* separate the states**, recorded so nobody repeats the work:
  (a) `world\levels\...opf` paths: the level *manifest* is resident at the front end and
  `world\levels\1_southpark\2_hub.opf` is already held **9** times on the menu (identical count in
  the level); (b) AMX script headers (pattern `?? ?? ?? ?? e1 f1 0a 0a 14 00 0c 00 ??`, taken from
  the extracted scripts): **0** matches at the menu and **0** in the level, so file-level AMX
  headers are not what the engine holds in memory.
- **Frame-fingerprint separation** (grids recomputed from the PNGs, matrix in
  `local/grid_report.py` output): menu × menu = **1.00**, menu × level = **0.31–0.34**, level ×
  level = **1.00**, across runs and across the menu's brightness/fade changes.
- The launch lock was held by `step03` when I asked for it; `launch_lock.py acquire --wait` waited
  ~30 s and then took it. No game process of another agent was touched.

## Derived (concluded from evidence, with the evidence named)

- **A click on a fixed fraction of the client area is a reliable menu automation primop.** Evidence:
  two independent runs, click (0.844, 0.657) → loaded level 9–14 s later. Confidence: high for this
  resolution (1280×720, `window_width/height` in `engine.ini`) and this localized build.
- **"Level loaded" is detectable today with two independent signals**: (1) the frame's coarse
  structure is not the front end (correlation margin 1.00 vs 0.33), and (2) the working set grew
  ≥5 % after a front-end frame, or is ≥470 MB, or AMX headers are resident. `in_game` requires
  both. Evidence: runs 2 and 3, both directions, plus run 1 where the engine side is missing and
  the detector correctly refuses. Confidence: high for signal 1, medium for the 470 MB floor
  (measured on this machine and build only; the growth test is preferred and self-calibrating).
- **The first detector design was wrong and the screenshots caught it.** "A `world\levels\` path is
  in memory ⇒ in gameplay" returned `in_game: true, level: 111_lost_forest` on the **menu** in run 1
  (that is literally what `result.json` of run 1 still says, because it was written by the old
  code). Cause: the level manifest is loaded at startup. Confidence: certain.
- **The main menu can leave itself without input.** Run 1 (no click, no keys) was on the menu at
  60.6 s and in the level at 72.6 s. Run 2/3 reached the level sooner, right after a click, so the
  click does cause the load; what happened in run 1 is unexplained (idle attract/auto-continue).
  Confidence: observation certain, cause unknown.
- **This title is DX9 in this copy, not D3D11.** `engine.ini` and `config.ini` both carry
  `use_directx_11 = false`, and `config.ini` is the application override that ships with the test
  copy. So "`PrintWindow` is black because the game is D3D11" is at best half the story; the
  foreground `BitBlt` works, `PrintWindow` returns 0.0 non-black. Confidence: high on the setting,
  medium on the causal wording.

## Unknown

- **Which level is loaded.** The bedroom scene is not nameable from the evidence: the most-held path
  in memory (`2_hub`) is the same at the menu and in the level. Cheapest answer: step 3's console
  (`status` / `level_debug`) once it lands; no extra launch needed for me.
- **Whether the New Kid is controllable in that state.** `W` for 2 s changed nothing. Cheapest next
  tests, in order: (a) press `Esc`/`Enter`/`Space` once to advance a possible intro cutscene, then
  hold `W` again; (b) click once inside the world first (the game may need a click to capture the
  mouse), then hold `W`; (c) try the arrow keys; (d) `-startlevel` straight into `2_hub`.
- **Whether `-startsavegame <name>` works.** It was never actually tested before (the "no saves"
  claim was wrong) and I had no launch left after the third series. Cheapest next: one 40 s run,
  `py -3.14 -B tools/in_game.py --once --startsavegame "Save Slot 001"`. If it loads, gameplay can
  be reached with **no clicking at all**.
- **Whether `-startlevel "world\levels\1_southpark\2_hub.opf" 0 0 0 0 0 0 0` works.** Tried by an
  earlier session, unverified; one launch answers it and would remove the menu from the loop.
- **Why AMX script headers are not in memory** in either state (0 matches). The scripts are loaded
  through the archive with their `CRYPT` flag; the header may be transformed before it is mapped.
- **Why run 1 left the menu on its own.** Attract mode, an idle timer, or a queued input from the
  console session — not distinguishable from three runs.

## Deliverable

Files (both mine, both on branch `game/south-park-the-stick-of-truth-step02-harness`):

- `tools/in_game.py` — `start_run(save=None, level=None, ...)` returning
  `{pid, in_game, level, level_candidate, level_path, screenshot, frames, signals, evidence,
  warnings, validity, profile, verdict_history, ...}`, plus `--once`, `--replay RUN_DIR`, and
  scripted `--action SECONDS:click:X,Y | keys:a,b | hold:w,2.0`.
- `tools/capture_game_window.py` — readable-frame capture with `nonblack_ratio`, `mean_luma`,
  `std`, an 8×8 `luma_grid` and a `dhash`; foreground `BitBlt` first, `PrintWindow` fallback, and
  `captured: false` instead of a misleading frame. Also `click_client`, `send_keys`, `hold_key`,
  `grid_correlation`, `dhash_distance`.

Commands actually run (from `games/south-park-the-stick-of-truth/`, `SP_GAME_DIR` default):

```
py -3.14 -B tools/launch_lock.py acquire --owner step02 --minutes 45 --wait 1800
py -3.14 -B tools/in_game.py --once --no-lock --duration 90 \
    --notes "series 1: plain launch"
py -3.14 -B tools/in_game.py --once --no-lock --sample-times 12,30,42,56,70,84 \
    --probe-times 24,78 --action 46:click:0.844,0.657 \
    --notes "series 2: menu, click Continue, level"
py -3.14 -B tools/in_game.py --once --no-lock --sample-times 12,30,42,56,70,92 \
    --probe-times 30,78 --action 46:click:0.844,0.657 --action 82:hold:w,2.0 \
    --notes "series 3 (decisive)"
py -3.14 -B tools/launch_lock.py release --owner step02
py -3.14 -B tools/in_game.py --replay local/game-runs/in-game-2ac6bd9dd1eb4591b90300d5c70d6911
```

Reproduce a verdict without launching anything: every run folder keeps `result.json` with raw
samples (frame metrics, working set, CPU) and raw probe output, so `--replay` re-decides it.

## Handoff notes for the integrator

Lines I want published, one per bullet, with the evidence attached:

- **`PLAN.md` fixed-facts table, profile row is wrong.** Replace "**no save files exist**" with
  "8 save slots + 3 checkpoints under `save\` (July 2025); `-startsavegame` was never actually
  tested, the earlier check missed the `save\` subfolder". Evidence: the retail profile listing and
  every `original-profile-backup/save/` in `local/game-runs/` from 2026-11-04 and 2026-11-15.
- **`PLAN.md` / `prompts/03`: "`PrintWindow` returns black because D3D11"** → "`PrintWindow` returns
  an all-black frame for this title (this copy runs DX9 per `engine.ini`/`config.ini`:
  `use_directx_11 = false`); the foreground `BitBlt` of the client area is the working method".
- **`NOTES.md`, new dated section (Gate 2 answered):** "A bot can reach a loaded level under the
  guard and prove it. `tools/in_game.py` clicks `Продолжить` at client fraction (0.844, 0.657), the
  level loads ~9 s later, and the detector requires two independent signals (frame fingerprint not
  the front end; engine working set grown ≥5 % / AMX headers). It returns `false` on the menu and
  `true` in the level (runs `in-game-83f32f95`, `in-game-2ac6bd9d`, all three validity flags
  `true`). Player control in that state is **not** shown: holding `W` for 2 s changed nothing."
- **`NOTES.md`, correction of a detector that lied:** "The first design (`a world\levels\…opf path
  is in memory ⇒ in gameplay`) reported `in_game: true` on the main menu: the level manifest is
  resident at startup and `2_hub` is held 9 times there. Do not use path presence as a state
  signal."
- **`README.md` Known issues:** replace "Nothing is known about reaching gameplay under the
  harness…" with "The harness reaches a loaded level (see QA). `PrintWindow` is black; frames come
  from the foreground `BitBlt`. The loaded state idles and did not answer a held `W`; whether the
  player is controllable there is open. The level's name is not established."
- **`README.md` QA:** keep QA level `none` for co-op, but add: "3 guarded launches on 2026-11-15
  (step 2), ~90 s each: profile unchanged, guard integrity and exe hash `true` in all three; menu →
  level reached twice with a scripted click; detector verified in both directions."
- **What this unblocks:** Gate 3 (object model) can now be run *in a loaded level* with a
  reproducible 60 s path, and `local/game-runs/*/result.json` gives it the working-set/AMX numbers
  to compare against. Gate 5 (second body) needs the control question answered first — cheapest is
  `Esc`/click-then-`W`, then `-startlevel`.
- **Not mine to edit:** `tools/run_net_probe.py` still imports `capture_game_window(pid, path)`; the
  signature is unchanged and its returned dict gained keys, so it keeps working — but its
  `observation.method` values changed from `ForegroundClientBitBlt`/`PrintWindow` to
  `bitblt`/`printwindow`.
