# Prompts, in the order they should be run

One prompt = one agent/chat. Full context, rules and stop conditions: `../PLAN.md`.
Do not start a step before the step it depends on has an answer written in its `../notes/<step>.md` file.

| 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | later |
|---|---|---|---|---|---|---|---|
| `01-net-reachability.md` | `03-gameplay-harness.md` **and** `02-console-and-devtools.md` (same period) | `04-object-model.md` | `05-second-player-input-camera.md` | `06-transport-and-relay.md` | `07-remote-player-and-session.md` | Gate 7 sync scope (no prompt yet) | shipping |

Why this order:

1. **`01` first, alone in its decision.** It answers "can the shipped Onyx netcode host and join"
   and that answer decides everything after it. Nothing here is a product without it.
2. **`03` and `02` in parallel, right after `01` starts.** `03` gives every later agent a way to
   reach a loaded level and prove it; `02` gives cheap experiments and cvar reads. Neither
   depends on `01`.
3. **`04` next**, because both the engine-reuse track and the build-from-zero track need the live
   actor model and the replicated-field list.
4. **`05`** turns that model into a second controllable body on one machine. Development
   milestone only.
5. **`06`** moves packets between two PCs, no port forwarding. Start it with `05` only in the
   build-from-zero case.
6. **`07`** is where it becomes co-op: the friend's own screen and camera, join after load, leave,
   reconnect.
7. **Sync scope** (combat turns, story, saves, transitions) only after movement across machines
   works.

If `01` answers "no" for all three tests, skip straight to `05` + `06` and note the verdict in
the notes file of the step that failed. If `03` cannot reach a level reliably, stop the queue and fix the harness.

## One file per agent

Agents run in parallel without fighting over files: each step starts from branch
`game/south-park-the-stick-of-truth` in its own worktree, writes only `../notes/<its own>.md`
and its own `../tools/` file, works in its own git worktree, and holds the machine-wide launch lock
while the game runs. Shared documents (`NOTES.md`, `README.md`, `PLAN.md`, `data/`) are written by a
single integrator afterwards. Rules: `../PARALLEL.md`.
