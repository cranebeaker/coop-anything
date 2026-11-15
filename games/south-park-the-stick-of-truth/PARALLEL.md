# Running several agents at once

Steps 1-3 of `PLAN.md` are meant to run in the same period, and Track B overlaps steps 5 and 6.
That only works if the agents cannot step on each other. This page is the contract.

## The three things that actually collide

1. **The repository.** Two agents editing `NOTES.md`, `README.md`, `data/directory.yaml` or
   `DIRECTORY.md` produce conflicts that a human then has to untangle.
2. **The machine.** There is one GPU, one desktop, one Steam account and one game install. Two
   agents launching the game at the same time is not "parallel", it is a broken test: the build
   refuses a second instance, the window focus is stolen from the other agent's capture, and both
   harnesses write to the same profile paths.
3. **Shared state on disk.** One test install, one `local/game-runs/` tree, one Frida session.

## File ownership

Each step owns files; nobody edits another step's files. "Integrator" is the only role that
touches shared documents.

| Step | Owns | Must not edit |
|---|---|---|
| 1 Net reachability | `notes/01-net-reachability.md`, `tools/net_probe.js`, `tools/run_net_probe.py` | `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`, `data/` |
| 2 Gameplay harness | `notes/03-gameplay-harness.md`, `tools/in_game.py`, `tools/capture_game_window.py` | same |
| 3 Console | `notes/02-console-and-devtools.md`, `tools/console_exec.py` | same |
| 4 Object model | `notes/04-object-model.md`, `tools/object_model.py`, `MODEL.md` | same |
| 5 Second body | `notes/05-second-player-input-camera.md`, `tools/two_players.py` | same |
| 6 Transport | `notes/06-transport-and-relay.md`, `tools/relay.py`, `NETWORK.md` | same |
| 7 Remote player | `notes/07-remote-player-and-session.md`, `SESSION.md` | same |
| Lead / integrator (no step agent was given it) | `mod/` — mod DLL, bridge glue, transport, remote body | every step's `notes/` and step-owned `tools/` |
| Lead / integrator (no step agent was given it) | `launcher/` | same |
| Lead / integrator (no step agent was given it) | `tools/bridge` (the hub bridge at the repo root) | same |
| Integrator (9th chat) | `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`, `data/directory.yaml`, `DIRECTORY.md`, `playbook/` | nobody else's `notes/` or `tools/` |

**`mod/src/mod.cpp` is announce-first.** Two agents collided on it once. Write the intended change
and who is making it in `NOTES.md` under the current session **before** you edit it; if the note is
not there, do not touch the file and ask the lead. The same applies to any shared file under `mod/`
that another agent is editing.

Write-up format inside your own notes file: `## <date> — step N — <agent name>`, then three
separate lists **Observed / Derived / Unknown**, then a "Deliverable" line with the exact commands
and run folders. The integrator folds these into `NOTES.md` and `README.md` in one pass, at the end,
when there is no other agent running.

## Branches and worktrees

- One branch per agent: `game/south-park-the-stick-of-truth-stepNN-<slug>`, cut from the same
  `main` (or from `game/south-park-the-stick-of-truth` if the owner is using that branch).
- One `git worktree` per agent, so nobody shares an index or a `local/` folder:
  `git worktree add ../sp-step01 game/south-park-the-stick-of-truth-step01-net`.
  The agent works inside its own worktree; `local/` is git-ignored and therefore private per
  worktree, which is what we want for dumps and run folders.
- Small fix, small commit. Never `git push --force`, never rebase another agent's branch.

## The launch lock (mandatory)

The game may be launched by **one agent at a time**, on any branch, in any worktree. Take a
machine-wide lock before the first launch of a session and hold it for the whole experiment,
including two-instance tests:

```
py -3.14 -B tools/launch_lock.py acquire --owner step01-net --minutes 45
py -3.14 -B tools/launch_lock.py status
py -3.14 -B tools/launch_lock.py release --owner step01-net
```

`run_net_probe.py` takes and releases the lock itself (`--lock-wait` seconds, default 1800), so a
plain run is already safe. `acquire` waits instead of failing, prints who holds the lock and how
long is left, and breaks a stale lock whose process is gone. If you need the desktop for a capture,
say in your notes when you held it, so the integrator can explain a black frame later.

Rules around the lock: never kill another agent's game process, never start Steam's own "second
instance" workaround while holding nothing, and never run two instances without `-multiple_instance`
(step 1's test is the only place where two instances are expected).

## What "writing together" means here

They do not write in the same file at the same time. They write in **their own file**, in their own
worktree, under the lock, and the integrator writes the one story afterwards. If two agents
genuinely need the same sentence, the integrator writes it once and links both notes files.

## When a conflict still happens

1. Do not resolve it by overwriting. Keep both versions in your notes file.
2. Write a `.patch` or a copy of your section into `notes/` and stop editing the shared file.
3. Tell the owner which two agents disagree, with both claims and the run folders, and let the
   integrator decide from evidence.
