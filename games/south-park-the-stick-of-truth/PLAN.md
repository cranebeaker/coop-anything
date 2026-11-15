# Plan: co-op for South Park: The Stick of Truth

## Product target

**Two players, two PCs, two screens, over the network**: player 1 hosts on their own machine,
player 2 joins from theirs and plays on their own monitor with their own camera and controls.
No split screen, no second window on the host, no port forwarding on the router (Steam relay or a
VPN is acceptable). Story, turns, quests and saves have to agree.

Everything local in this plan (two characters driven from one machine) is a **development
milestone only**: it is how you prove a second controllable body and a second input path exist
before you put packets between them. If a gate is described as "on one machine" it is a harness,
not the deliverable.

## How the prompts are organised

Everything below is a **gate**: later work is wasted if an earlier gate answers "no". Seven
self-contained prompts for seven agents/chats are in `prompts/`; each names its gate, its inputs
and what counts as proof. The order to run them in is in the next section, because one answer
(Gate 0) decides which order is worth paying for.

## Fixed facts every agent starts from

| | |
|---|---|
| Game | South Park: The Stick of Truth, Steam app **213670**, build **228353**, x86 PE32 |
| Exe | 11,468,656 bytes, SHA-256 `54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6` |
| Engine | Onyx (Obsidian), inherited from Dungeon Siege III |
| Data | `data_archive.oaf` 1.9 GB, `engine.oaf`, localized `.oaf`; loose `data/` layer (movies) is read; `data/global` is empty in the retail install |
| Scripts | 316 AMX, 314 native imports, bytecode marked `CRYPT`; no session/server/network natives |
| Netcode | Onyx Steam session + P2P + LAN + chat + voice managers, host/ghost replication RTTI, `LOCAL_WITH_NETWORK`, `connect/status/restart_server/kick/warp_to_player` console commands, `multiplayer_*` cvars — all linked, none observed active |
| Runtime today | menu only: Winsock initialised, 0 bind/listen/connect, `SteamNetworking()` polled ~18/s, no lobby, no game server |
| Protection | `.text` is Steam-CEG encrypted; static disassembly of game functions is unreliable |
| Profile | `%USERPROFILE%\Documents\My Games\South Park - The Stick of Truth`; owner disabled Steam Cloud; **save files exist** in `save\` (`Save Slot 001–008.sav`, `Checkpoint 1–3.sav`), corrected 2026-11-15 |

## Rules for every agent

1. Work only on a **copy** of the install. Never touch the retail folder or the owner's saves.
   Use `tools/` (`run_net_probe.py` / `guard_session.py`) so every launch is guarded and every
   run is provable from `result.json`.
2. Ask the owner before launching the game (it starts through Steam with their account), before
   installing anything, and before publishing.
3. Do not publish game files, extracted assets, saves, tokens or paths containing a username.
4. Write findings in `NOTES.md` as you go, in three separate lists: observed, derived, unknown.
5. Do not claim behaviour from a name, a string, an RTTI symbol or an address in `.text`.
   Only a hook, a controlled call with a result, or a screenshot counts.
6. A run is only valid if `original_profile_unchanged`, `guard_integrity` and
   `original_exe_hash_still_matches` are all `true`.
7. Stop and report on the first contradiction with this plan.

## Working in parallel without colliding

Read `PARALLEL.md` before the first commit. The short version:

8. **One file per agent.** Write only in `notes/<your step>.md` and in `tools/<your file>`. Never
   edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`, `data/directory.yaml` or `DIRECTORY.md`;
   an integrator does that once, after the last parallel agent is idle.
9. **One worktree, one branch per agent** (`git worktree add ../sp-stepNN game/...-stepNN-<slug>`),
   so nobody shares an index or a private `local/` folder.
10. **One launch at a time, machine-wide.** Take `tools/launch_lock.py` before the first game launch
    of your session and hold it through the whole experiment, two-instance tests included.
    `run_net_probe.py` acquires it itself. Never kill another agent's game process.
11. **Never regenerate a generated file or a shared table** while another agent is running.
12. If you find yourself wanting to change a file you do not own, put the proposed text and the
    evidence in your own notes file and hand it to the integrator.

## Base branch and the integrator

- **Base branch for every step: `game/south-park-the-stick-of-truth`.** It holds `tools/`, `notes/`,
  `PARALLEL.md` and these prompts. An agent that cannot see those paths is on the wrong branch — it
  must stop and ask the owner, not recreate them.
- **The integrator is a separate chat (9th) or the owner.** Only it writes `NOTES.md`, `README.md`,
  `PLAN.md`, `prompts/`, `data/directory.yaml`, `DIRECTORY.md` and `playbook/`. It runs when no other
  agent is mid-run, reads every `notes/*.md`, and writes one story from them.
- Integrator checklist: (1) every notes file has Observed / Derived / Unknown filled; (2) every
  "works" cites a run folder or a command that was actually executed; (3) the README QA level matches
  what ran; (4) the directory entry is still truthful; (5) no agent-owned file was overwritten;
  (6) commit, then show the owner the diff before any draft PR.

## Decision: thin path first (2026-11-15)

Goal in one line: two friends on two PCs, each with their own copy of the game, one launcher button each, and a
playable session with the least custom code.

- **Thin path (first):** use the engine's own session and replication (Onyx). The mod is a launcher plus a small DLL
  that starts the host or the client through the engine's own commands, with an overlay for the code and status.
  Simplest for players, least code to maintain.
- **Thick path (fallback only):** write our own network layer, replication and turn sync. Start it only if the thin path
  is proven impossible.

Trigger for the thin path: `restart_server` (host) and `connect <addr>` (client) reach `bind`/`listen` and
`ws2_32!connect` in the instances, and both processes stay alive. Until that is shown, nothing in the thick path is
built. The decision is revisited only on a clear "no" from the direct handler tests.

Transport for the first release is still a VPN with a direct address (no port forwarding). It is not decided by this
section.

## Run order

Give one prompt per chat/agent, in this order. Do not start a step before the step it depends on
has an answer in `NOTES.md`. "Session" below means one working block of an agent, not a calendar
day.

| # | Prompt | Question it settles | Depends on | Parallel with | Rough size |
|---|---|---|---|---|---|
| 1 | `prompts/01-net-reachability.md` | Can the shipped Onyx netcode host/join at all? | — | 2 and 3 may start the same day | 1-2 sessions |
| 2 | `prompts/03-gameplay-harness.md` | Can a bot reach a loaded level and prove it? | — | 1, and 3 | 1-2 sessions |
| 3 | `prompts/02-console-and-devtools.md` | Can an agent execute a console command and read the result? | — | 1, and 2 | 1-2 sessions |
| 4 | `prompts/04-object-model.md` | Where is the live player, the companion, the player list? | 2 (needs gameplay), static part can start earlier | — | 2-3 sessions |
| 5 | `prompts/05-second-player-input-camera.md` | Can a second body be driven by a second input? | 4 | Track B's 6 | 2-4 sessions |
| 6 | `prompts/06-transport-and-relay.md` | Do packets move between two PCs without port forwarding? | 1; and 5 for the custom branch | 5 (Track B only) | 1-2 (A) / 2-3 (B) |
| 7 | `prompts/07-remote-player-and-session.md` | Does the friend on the other PC actually play? | 1, 4, 5, 6 | — | 3-6 sessions |
| 8 | Gate 7 sync scope (no prompt yet) | Turns, story, saves, transitions agree | 7 | — | 4-8 sessions |

### Two tracks after step 1

**Track A — Gate 0 answers "yes, the netcode starts".** Steps 2, 3, 4, 6, 7. Step 6 becomes
"make the engine's own host/join work across two PCs" and step 5 is a short sanity check rather
than a design job, because the engine already replicates `Player_Object` and `Remote_Camera_Object`.
This is the cheap world.

**Track B — Gate 0 answers "no" (or only local play).** Steps 2, 3, 4, 5, then 6 and 7 built from
zero on the Gate 5 layer. Steps 5 and 6 can run in the same period: the body/input layer and the
transport/protocol layer meet at a documented interface (the replicated-field list from step 4).

### Gates that stop the queue

- Step 1 "no" for all three tests → do not spend on Track A at all; go Track B and say so in the
  write-up.
- Step 2 fails (no reliable way into a level) → stop: every later gate needs it. Fix the harness
  before paying for anything else.
- Step 4 shows there is exactly one player object and no second body candidate → the project
  becomes "engine netcode or nothing": re-open step 1 with the console (step 3) before continuing.
- Step 7 fails on the guest's own camera or on join-after-load → still a valid prototype, but not
  co-op; write down what a player would experience.

### What to do with the owner before step 1

Agree once, in writing: which PC is host, whether a second machine or VM exists for tests, that
launches happen through Steam on both, and that nothing is published until the diff is reviewed.
Without a second machine, steps 6 and 7 can only be prepared, not finished.

## Gate details

### Gate 0 — can Onyx netcode be started? (decisive, do first)

Everything else is optional if this is "no", and the whole project becomes cheaper if it is
"yes". Three escalating tests: (a) does the session manager have a callable host path at all;
(b) can a client `connect 127.0.0.1:<port>` and reach a join error instead of a refusal;
(c) does a second instance (`-multiple_instance`) join and produce a second `Player_Object`.
Deliverable: a recorded run per test plus a one-line verdict, and if "yes", a note on which
replication classes actually appear.

→ `prompts/01-net-reachability.md`

### Gate 1 — console and dev access

The engine has a full console (`console_toggle`, `exec`, `find`, `write_file`, `status`) and a
command set with help text, but autoloading from `data/global/console_scripts` and `-exec` were
not observed to run. Needed for cheap experiments (`startlevel`, `status`, `god`, `warp*`) and
for reading cvars. Deliverable: one reproducible way to execute an arbitrary console command and
read its output, machine-checkable, no game files patched if avoidable.

→ `prompts/02-console-and-devtools.md`

### Gate 2 — gameplay-state harness

Saves exist, but `-startsavegame` was never tested, and `PrintWindow` returns black on this DX9 copy, so no agent can currently prove anything about
gameplay. Needed by Gates 3-5. Deliverable: a scripted path from launch to a loaded level with an
automatic "am I in gameplay" check (screenshot + a state signal), plus save isolation for
new saves.

→ `prompts/03-gameplay-harness.md`

### Gate 3 — actor and object model at runtime

Required for either path (engine netcode or puppet). Find the live New Kid object, its
`Player_Object`/`Actor` layout, the companion objects, and whether the player list can hold more
than one entry. Deliverable: a documented runtime model with addresses/module+offset, found via
vtable/RTTI scanning and verified by reading plausible values (position, health, tags).

→ `prompts/04-object-model.md`

### Gate 4 — a second controllable body (local development milestone)

Make a second character move under a second input source on **one** machine. This is not the
product; it is how you prove that the engine can hold a second controllable body and that a
second input path exists at all before packets are involved. Deliverable: two independent inputs
driving two characters, plus the field list the remote player will have to replicate (position,
animation, tag, health) taken from Gate 3.

→ `prompts/05-second-player-input-camera.md`

### Gate 5 — transport for two machines

Only when Gates 0-4 are answered. Prefer reusing Onyx/Steam P2P if Gate 0 says yes; otherwise
Steam Spacewar relay or a VPN, and a relay wrapper for the UDP/TCP path with a chaos run.
Acceptance: host on PC A, guest on PC B, no port forwarding.

→ `prompts/06-transport-and-relay.md`

### Gate 6 — remote player and session lifecycle (the actual product)

The part that makes it "two screens over the network": the guest's own camera and screen, the host
seeing the guest move and animate, join mid-game, leave, reconnect after a drop, and who owns the
world. Deliverable: a two-PC checklist run with logs from both machines.

→ `prompts/07-remote-player-and-session.md`

### Gate 7 — sync scope

After movement works across machines: combat turns, quest/story flags, inventory, saves, level
transitions, cutscenes. This is where the game is actually different from a shooter: it is
turn-based combat with a companion slot, and the story is scripted scene by scene. Not yet split
into a prompt — do not start it before Gate 6.

## Parallelism

The order in "Run order" is the authority; this is only what may overlap.

- Steps 1, 2 and 3 can run at the same time: Gate 0 is independent of the harness and console work.
- Step 4's static half (RTTI/vtable index) can start while step 2 is still being written.
- Track B's steps 5 and 6 can overlap, meeting at the replicated-field list from step 4.
- Steps 6 and 7 need two machines (or two VMs) and the owner's other PC; ask early, never assume
  access to a friend's machine.
- One agent per step, one branch per step, one `NOTES.md` section per agent with its name and date.

## What "done" looks like for a first release

Two players on two PCs, same campaign, host owns story and combat, the guest controls a real
character and sees their own screen, turns and quest transitions stay synced through a 30-minute
scripted run of the first area with one deliberate disconnect and reconnect
(`playbook/automated-qa.md`). Nothing less is called co-op.
