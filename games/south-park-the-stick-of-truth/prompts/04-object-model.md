# Prompt 04 — Gate 3: the runtime actor and object model

You are one agent on **South Park: The Stick of Truth** (Steam app 213670, build 228353, x86).
Read `../../AGENTS.md`, `../README.md`, `../PLAN.md` and `../PARALLEL.md`, then
`playbook/reverse-engineering.md` and `playbook/actor-ownership.md`.

## Coordination (read this before you start)

You are **step 4 of 7** (order: `../prompts/README.md`); this prompt is `prompts/04-object-model.md`.
The contract is `../PARALLEL.md`.

**Start from the right base.** The paths this prompt names (`../notes/`, `../tools/launch_lock.py`,
`../PARALLEL.md`) exist only on branch `game/south-park-the-stick-of-truth`. If you cannot see them, stop and ask the owner for
that branch — do not recreate them.

**Your own worktree and branch**, so you never share an index or a private `local/` folder:

```
git worktree add ../sp-step04 -b game/south-park-the-stick-of-truth-step04-objects game/south-park-the-stick-of-truth
cd ../sp-step04
```

Work only inside that directory from then on.

**Your files.** Write only `../notes/04-object-model.md`, `tools/object_model.py`, `MODEL.md`. Never edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`,
`data/directory.yaml`, `DIRECTORY.md`, or another agent's notes/tools: a separate integrator merges
your notes file afterwards, when no other agent is running.

**One launch at a time, machine-wide** (there is one Steam account, one desktop, one game
install). Take the lease before your first launch and hold it through the whole experiment,
two-instance tests included:

```
py -3.14 -B tools/launch_lock.py acquire --owner step04 --minutes 45 --wait 1800
py -3.14 -B tools/launch_lock.py release --owner step04
```

`run_net_probe.py` takes and releases it by itself. If another agent holds it, wait — never start a
second game "just for a minute", and never kill another agent's game process.

**Handoff.** Fill all five sections of your notes file, including "Handoff notes for the
integrator" with the exact `README.md`/`NOTES.md` lines you want published. End your chat with:
verdict, evidence (run folders and commands), what your result unblocks, and what you could not do.

## Why

Both possible co-op paths need the same thing: find the live player character, the companions,
and the list the engine keeps players in. The exe contains Onyx RTTI for `Player_Object`,
`Secondary_Player_Object`, `Remote_Camera_Object`, `Companion`, `Creature`, `Item`, `Door`,
`Level_State`, `Game_State`, and host/ghost replication controllers for each. That is a map of
what to look for, not a map of where it is.

## Hard constraint

`.text` is Steam-CEG encrypted at rest and the runtime bytes at candidate handler addresses did
not disassemble into coherent functions (two different natives produced identical 12-instruction
readouts). Do not build a model from static code. Build it from **RTTI + live heap scanning +
value validation**, and say which of those each claim rests on.

## Work

1. **RTTI/vtable index.** Parse the MSVC RTTI structures (`.?AV…@OEI@@` type descriptor names,
   Complete Object Locators, vtables) from the on-disk image (`.data`/`.rdata` are not encrypted).
   Build `{class name -> vtable RVA(s)}`. Start with `Player_Object`, `Secondary_Player_Object`,
   `Companion`, `Creature`, `Actor`, `Remote_Camera`, `Game_State`, `Level_State`,
   `Component_Network`.
2. **Live object finder.** At runtime, scan committed heap ranges for dwords equal to a vtable
   VA. Validate a hit by reading: the actor tag string (the AMX layer uses tags, e.g.
   `object_get_by_tag`), a position (three floats that move when you walk), health, and the
   component list. A "hit" that does not move when the player moves is not the player.
3. **Player list.** Determine whether the engine stores more than one local player. Search for the
   array/list that holds player objects: `player_get_by_index` exists as an AMX native; its
   argument conventions are unknown, so prefer the data structure over the call. Report the
   index→object mapping and whether index 1 can exist.
4. **Companion slot.** The game already has a follower/companion actor (see
   `playbook/how-coop-mods-work.md` §1.1 on puppets): document how it is spawned, whether it has
   a `Player` component, and whether it is the cheapest second-controllable body.
5. **Reproducible output.** `../tools/object_model.py` → JSON with `{players: [{index, tag,
   vtable, position, health}], companions: [...], level, module_base, time}` plus a short
   `MODEL.md` listing offsets that were **observed** versus **guessed**.

## Deliverable

- The JSON model above, taken twice: standing still and after walking, so positions prove identity.
- `MODEL.md` with a table: field, offset, how it was found, confidence (observed/inferred/unknown).
- In `../notes/04-object-model.md`: three lists (observed, derived, unknown), and the reproduction command.

## Do not

- Do not write to the game process until the read-only model is stable; then write only in the
  test copy and only through a hook you can remove.
- Do not call `player_get_by_index` / `set_party_companion` with guessed arguments — an earlier
  session correctly refused to, keep that rule.
- Do not publish heap addresses from the owner's machine without the module base, so they mean
  something on another machine (prefer module+offset).
