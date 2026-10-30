---
name: coop-build-from-zero
description: Build co-op for a single-player game that has no co-op mod: choose a hook point, show the second player as a puppet, pick an authority model, sync world and story state, handle level transitions and saves, and test with two real clients.
---

# Build co-op from zero

Order matters: each step must work and be tested before the next.

1. Hook point: script extender, mod loader (BepInEx, UE4SS, REFramework), injected DLL, or source port. See `playbook/tools.md`.
2. Transport: pick a library and connection method (`coop-networking`). Start with direct IP on one PC.
3. Second player visible: spawn a puppet of the remote player and stream its transform and animation.
4. Authority: host owns enemies, NPCs and world objects; each player owns themselves. Lockstep only if the game is deterministic.
5. Interactions: doors, pickups, damage, deaths, with the host deciding.
6. Story: quest flags, dialog and cutscenes. Decide shared vs per-player for every piece of state.
7. Transitions: level changes, saves, late join, disconnects.
8. Tests at every step with real game clients (`coop-qa`).
9. Ship: one install file per OS that finds the game, installs the loader and the mod with pinned SHA-256 checks, updates and uninstalls. See `playbook/shipping-to-players.md`.

Study how existing mods did each step first: `playbook/how-coop-mods-work.md`.
