# Prompt 06 — Gate 5: transport between two machines

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/networking.md`, `playbook/networking-steam-spacewar.md`,
`playbook/network-chaos-testing.md` and, if the game's own netcode is used,
`playbook/desync-hunting.md`.

## Coordination (read this before you start)

You are **step 6 of 7** (order: `../prompts/README.md`); this prompt is `prompts/06-transport-and-relay.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step06 -b game/south-park-the-stick-of-truth-step06-transport game/south-park-the-stick-of-truth
cd ../sp-step06
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/06-transport-and-relay.md`, `tools/relay.py`, `NETWORK.md`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step06 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step06
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Prerequisite

Gate 0 (prompt 01) and Gate 4 (prompt 05) must have answers. Do not start this gate on guesses.

## Scope

You own the wire, not the product: packets move between two machines, the handshake refuses mixed
builds, and a drop is survivable. Showing the guest on their own screen with their own camera,
join-after-load and reconnect semantics is Gate 6 (`prompts/07-remote-player-and-session.md`). You
provide the two-machine link it needs; it provides the acceptance run.

Your acceptance test is still two machines, not loopback: host on PC A, join from PC B, no port
forwarding, with one short walk from each side.

## Two branches, pick from evidence

**Branch A — Gate 0 said the Onyx net layer starts.** Then the transport already exists: Steam
P2P or the LAN session manager. Your job is to make it work between two machines without port
forwarding, and to find out what the engine needs to agree on (version string, level name, player
count, host authority). Start by replaying the exact sequence that worked on one PC (host + client)
with the relay in between, and capture the engine's own error strings (`Server is full`,
`incompatible version`, `invalid connection params - bad level name`) as your diagnostics.**Branch B — Gate 0 said no, or only local play works.** Then you own the transport for a mod
built on the Gate 4 input/actor layer:
- prefer the Steam Spacewar relay (app 480) so friends connect without port forwarding;
- wrap the mod's UDP socket in a relay so the same wrapper can also run through a VPN;
- keep the protocol small and versioned, with a handshake that refuses mixed builds;
- run one chaos pass (latency, jitter, loss, reorder) and record what the game does.

## Work that is the same in both branches

1. **A connection recipe a player can follow** in under two minutes: one button to host, one to
   join, no editing config files. Write it down as steps with the exact UI text.
2. **Failure messages that mean something.** Every failure mode you hit (Steam not running, wrong
   build, host already in a scene, second instance refused) needs a message the player sees, not a
   log line.
3. **A two-machine smoke test** you can repeat: host on machine A, join on B, both walk, both see
   each other move, one deliberate disconnect and reconnect.
4. **Security and fairness.** No anti-cheat bypass, nothing that affects other players' games, and
   no traffic to services the game did not already use.

## Deliverable

- A `NETWORK.md` describing the chosen transport, the wire format (if Branch B), the handshake,
  and the tested failure modes with the exact strings the game produced.
- `../tools/relay.py` (Branch B) or a documented launch recipe (Branch A), plus the run logs of
  the two-machine test.
- `../notes/06-transport-and-relay.md`: three lists (observed, derived, unknown) and the one thing that is most likely to
  break for players on a bad connection.

## Do not

- Do not claim "works online" from a LAN test, and do not claim a reconnect works unless you
  deliberately killed the connection and saw the session recover.
- Do not publish Steam tokens, friend codes or the owner's account identifiers.
