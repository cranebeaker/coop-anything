# Prompt 07 — Gate 6: remote player and session lifecycle (the actual product)

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/coop-build-from-zero.md` if present, `playbook/how-coop-mods-work.md` §1.1-1.4 and §1.8,
`playbook/actor-ownership.md` and `playbook/desync-hunting.md`.

## The product requirement (read this twice)

Two friends, **two PCs, two monitors, over the network, no port forwarding**. Player 1 hosts
from their machine; player 2 joins from theirs and sees their own screen with their own camera and
controls. The host's screen is not a split screen and does not show a second window.

This gate is the difference between "there is a second character somewhere" (Gate 4/5) and "my
friend is playing the game from his house". Nothing before this gate is a product.

## Coordination (read this before you start)

You are **step 7 of 7** (order: `../prompts/README.md`); this prompt is `prompts/07-remote-player-and-session.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step07 -b game/south-park-the-stick-of-truth-step07-remote-player game/south-park-the-stick-of-truth
cd ../sp-step07
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/07-remote-player-and-session.md`, `SESSION.md`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step07 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step07
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Prerequisites

- Gate 0 (`prompts/01-net-reachability.md`) — whether the shipped Onyx replication is usable.
- Gate 3 (`prompts/04-object-model.md`) — live actor model and the field list to replicate.
- Gate 5 (`prompts/06-transport-and-relay.md`) — a working transport between two machines.

If either of the first two is missing, stop and say so; do not invent a session layer on guesses.

## Work

1. **Presentation of the remote player on each side.** The host must see the guest's character
   move and animate; the guest must see the host's character and the world. Every value the
   renderer needs (position, facing, animation/timeline state, visibility, equipment/skin where it
   matters) is derived from the Gate 3 field list, not from guesswork. Interpolate, do not snap.
2. **The guest's own camera and screen.** The guest is the client of the world the host simulates,
   so the guest needs a camera that follows *their* character, not the host's. If the engine can
   only own one camera (`Multiplayer camera ownership`), say what you did instead and what it costs.
3. **Session lifecycle.** Host starts, guest joins — including joining **after** the host already
   loaded a level (this game will be played for hours, not from a lobby). Guest leaves and
   reconnects without restarting the host. Host leaves → guest gets a clear message, not a hang.
4. **Who owns what.** Write it down in one table: world/quest flags, NPC state, doors and loot,
   combat turns, inventory, saves, level transitions. The host owns story; the guest owns their own
   body and their own UI. Anything else is either shared or refused, never silently divergent.
5. **Turn-based combat — next milestone, not the MVP.** The MVP (see `../PLAN.md`) is shared
   exploration of the hub with synced bodies. When you get to fights, establish what the guest can
   do (act on their own turn? only watch? own their companion?) and implement the smallest honest
   version. A guest who watches fights is a valid first release if it is written down.
6. **A two-PC acceptance run.** Host on machine A, guest on B: walk, quest trigger, one fight, one
   level transition, one deliberate disconnect + reconnect. Logs from **both** machines, screenshots
   from **both** screens, and a checklist with pass/fail per item.

## Deliverable

- `../SESSION.md`: the ownership table, the join/leave/reconnect protocol, the guest camera
  decision, and the known-broken list with reproductions.
- The acceptance run artefacts (both logs, both screenshots) and a summary table in `../notes/07-remote-player-and-session.md`.
- In your notes file: the exact README claims the integrator may publish, limited to checklist items that actually passed.

## Do not

- Do not call it co-op because two clients connected and one character moved.
- Do not test only on one machine and write "online" — a loopback test is a transport test, not a
  product test.
- Do not require port forwarding and do not require the player to edit a config file. The first
  release asks for one thing only: the other player's address inside a VPN that the launcher helps
  them set up.
- Do not publish the owner's Steam IDs, friend codes or machine names.
