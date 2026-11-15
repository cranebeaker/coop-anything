# Prompts, in the order they should be run

One prompt = one agent/chat. Full context, rules and stop conditions: `../PLAN.md`.
Do not start a step before the step it depends on has an answer written in its `../notes/<step>.md` file.

| 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | later |
|---|---|---|---|---|---|---|---|
| `01-net-reachability.md` — **closed, no** | `03-gameplay-harness.md` **and** `02-console-and-devtools.md` (same period) — done | `04-object-model.md` (static done, runtime running) | `05-second-player-input-camera.md` | `06-transport-and-relay.md` | `07-remote-player-and-session.md` | Gate 7 sync scope (no prompt yet) | shipping |

**The thin path is closed (2026-11-15).** The shipped Onyx netcode cannot be started from outside:
the console registers no commands, and calling `restart_server` or `connect` directly kills the game,
at the menu and inside a level, without reaching a socket. So everything from `04` on builds the
thick path: our own transport and replication. The decision and its MVP are in `../PLAN.md`
("Decision: thin path first", closed) and the release shape is in `../SHAPE.md`.

Why this order:

1. **`01` first, alone in its decision.** It answered "no" — no host, no client, no join. That is
   why the thick path is the plan, and why nothing after it waits on the engine's session code.
2. **`03` and `02` in parallel, right after `01` starts.** `03` gives every later agent a way to
   reach a loaded level and prove it; `02` gives cheap experiments and cvar reads. Both are done:
   the level route is a click on Continue, and the console pipeline runs even though it exposes no
   useful command.
3. **`04`** gives the runtime object model: which classes have live instances, where the player body
   is, and which values a second machine would need. Static RTTI is done (`tools/rtti_index.json`);
   the runtime scan is the half that unblocks `05`.
4. **`05`** turns that model into a second controllable body on one machine. Development milestone
   only: a local second body is not co-op.
5. **`06`** moves packets between two PCs. The first release uses a direct address inside a VPN
   (ZeroTier, Radmin, Tailscale). No port forwarding at any point; the Steam relay comes later, in
   shipping.
6. **`07`** is where it becomes co-op: the friend's own screen and camera, join after load, leave,
   reconnect.
7. **Sync scope** (combat turns, story, saves, transitions) only after movement across machines
   works. It is explicitly outside the MVP in `../PLAN.md`.

If `03` cannot reach a level reliably, stop the queue and fix the harness. If the runtime half of
`04` finds no second body the engine can drive, `05` falls back to the existing follower companion
as the guest's body and says so in its notes.

## One file per agent

Agents run in parallel without fighting over files: each step starts from branch
`game/south-park-the-stick-of-truth` in its own worktree, writes only `../notes/<its own>.md`
and its own `../tools/` file, works in its own git worktree, and holds the machine-wide launch lock
while the game runs. Shared documents (`NOTES.md`, `README.md`, `PLAN.md`, `data/`) are written by a
single integrator afterwards. Rules: `../PARALLEL.md`.
