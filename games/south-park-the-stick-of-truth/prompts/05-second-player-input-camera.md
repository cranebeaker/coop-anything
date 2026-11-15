# Prompt 05 — Gate 4: a second controlled character (input and camera)

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/how-coop-mods-work.md` §1.1-1.2 and `playbook/split-screen.md`.

## Coordination (read this before you start)

You are **step 5 of 7** (order: `../prompts/README.md`); this prompt is `prompts/05-second-player-input-camera.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step05 -b game/south-park-the-stick-of-truth-step05-second-body game/south-park-the-stick-of-truth
cd ../sp-step05
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/05-second-player-input-camera.md`, `tools/two_players.py`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step05 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step05
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Why

The product is **two players on two PCs over the network**, each with their own screen. Before any
packet exists, the engine must be able to hold a second controllable body and read a second input
source. That is what this gate buys, and nothing more: a local second character is a development
milestone, not the deliverable.

The companion half of the problem is already shipped: the game has a follower actor that moves,
animates and fights next to the New Kid, and RTTI for `Secondary_Player_Object` and
`Remote_Camera_Object`. Pick whichever body Gate 3 says is cheapest to control.

## Inputs you can reuse

- `../tools/object_model.py` from Gate 3: live player and companion objects with positions.
- Engine input strings: `handle_input_action`, `enable_analog_stick_input`, `disable_player_control`,
  `Mover::ApplyClientMotion`, `Mover::ForceUseOfClientMotion`, `m_clientMotion`, `draw_client_motion`,
  `player_get_by_index`, `ai_move_to_object`, `disable_client_motion`.
- Camera cvars and classes: `camera_multiplayer_control`, `multiplayer_tether_screen`,
  `multiplayer_tether_range`, `Multiplayer camera ownership`, `Remote_Camera`,
  `Remote_Camera::near_plane`, `Enable single-player zoom modes in multiplayer and vice versa`.
- The game already reads `XINPUT1_3.dll`, so a second physical device is realistic.

## Work

1. **Two input sources.** Get raw input for player 1 (keyboard/mouse or pad 1) and player 2
   (pad 2). The clean way is to find the engine's input mapping and give each player a context;
   the cheap way is to translate pad 2 yourself into engine move/intent calls. Document which you
   did and why.
2. **Two bodies.** Player 1 keeps the New Kid. Player 2 gets either the existing follower
   companion or a second `Player_Object` if Gate 3 showed the list can hold one. Movement must be
   engine movement (pathing/collision), not teleporting a mesh: prove it by walking around an
   obstacle.
3. **Camera.** For this milestone a single follow camera is enough. Note in `../notes/05-second-player-input-camera.md` which
   fields a remote camera would need (the engine has `Remote_Camera_Object`,
   `Multiplayer camera ownership`, `multiplayer_tether_*`) and whether the second body can be
   rendered when it is off the host's screen. The guest's own camera belongs to Gate 6.
4. **Combat and cutscenes.** Turn-based fights and scripted scenes will fight you. Record what
   happens when a fight starts with a second controlled body, and what happens to the second
   player during a cutscene. A working "explore together, fight with host-controlled turns" is an
   acceptable first milestone; write down what is missing.
5. **Proof.** Two PNG/video artefacts: both characters at different positions with both inputs
   active, and the same after a fight. Plus a JSON log of positions per player over time.

## Deliverable

- `../tools/two_players.py` or a minimal Frida/asi layer that enables the mode, with a toggle and
  a clean unload.
- The replicated-field list from Gate 3: exactly which values a remote client would need every
  tick (position, facing, animation state, tag, health, interaction state), with their offsets.
- `../notes/05-second-player-input-camera.md` section: what works, what breaks (with the exact reproduction), and the input-to-
  player mapping you settled on.
- Do **not** describe this milestone as co-op in `../README.md`: co-op means two machines.

## Do not

- Do not fake the second player by recording and replaying player 1's input on a clone unless you
  label it a demo and not a player.
- Do not teleport meshes and call it movement.
- Do not leave hooks installed after the test process exits.
