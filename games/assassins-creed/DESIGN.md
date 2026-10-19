# Assassin's Creed 1 — co-op design document

Status: **design only, nothing built or tested.** Written 2026-10-19. Read `NOTES.md`
first for how every fact here was obtained. This follows the build order in
[playbook/how-coop-mods-work.md](../../playbook/how-coop-mods-work.md#4-from-nothing-to-co-op-build-order-for-an-agent)
and the networking plan in
[playbook/networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md).

## Goal, stated honestly

Two (maybe up to four) players in the same AC1 story: each drives a real Altaïr-like
character, each sees the other move, and the host's world (guards, missions, scenes,
saves) is the truth. This is Path 2 — nothing exists to build on — and AC1 has **no
netcode**, so the entire sync layer is new code.

Before committing weeks, the owner has a cheaper alternative: **Unity's official 4-player
co-op missions** already exist, and an "Origins Together" project is in development for
Origins. AC1 is the hardest AC title to co-op, chosen deliberately.

## Architecture

| Piece | Choice | Why |
|---|---|---|
| Delivery | **ASI plugin** (`.asi`) loaded by `dinput8.dll` (Ultimate ASI Loader) | Proven on this exact exe by EaglePatch; no game file changes; easy to uninstall |
| Language/toolchain | C++ (MSVC, x86) + the `patcher` library EaglePatch uses | Matches the engine (32-bit, MSVC RTTI) and existing working code |
| Authority | **Host-authoritative**, state sync | Matches [§1.6/§1.2](../../playbook/how-coop-mods-work.md); AC1 is not obviously deterministic, so lockstep is out |
| Transport | Start plain UDP/direct-IP for two clients on a LAN; ship **Steam relay (Spacewar AppID 480)** | Playbook requirement: no port forwarding; see networking-steam-spacewar.md |
| Identity | Hook the engine's own objects via RTTI/vtable (`Human`, `PDeAssassin`) | RTTI gives 4084 `scimitar::` class names — a map already exists |

## Hook strategy (Stage 1)

1. Reproduce the EaglePatch pattern: `DllMain`, detect DX9 vs DX10 via
   `MEMCMP32(0x00401375 + 1, ...)` / `0x004013DE`, then patch by address. Its file
   `src/ac1.cpp` is the reference for: the engine allocator wrappers
   (`ac_allocate` / `ac_getNewDescriptor` / `ac_delete`), the input objects
   (`scimitar::Pad`, `PadProxyPC`, `PadXenon`) and how vtables are called
   (`vtable[10]`) — i.e. the shape of an `scimitar::` managed object.
2. Build a class map from RTTI (names + vtables) with Ghidra/`dumpbin`, seeded by the
   `Human*`, `PDe*`, `AI*`, `Scene*`, `WorldTransition*` families already listed in
   `NOTES.md`. Verify the assumption "`PDeAssassin` is the local player" against a live
   process before relying on it.
3. Keep a **logging ASI skeleton** as milestone zero: if it loads, prints the detected
   build and survives a level load, the toolchain is proven. Everything else builds on it.

## Build order and milestones

**M0 — Toolchain (hours).** Isolated test copy of the install (never the owner's
install), Ultimate ASI Loader + a logging `.asi`. Success = log line at startup and after
`/` world load, game still boots.

**M1 — Puppet player (the make-or-break gate).** Read the local player's transform and
animation state; find whether a second `Human` can be created and driven.
- If a second humanoid **can** be spawned: mirror the local player as a puppet and prove
  "player A moves, player B sees A move" on one machine, then over UDP.
- If it **cannot**: the fallback is per-client game instances with a synced *world*, and
  each player still only sees their own body (a much weaker product). Decide here, not later.

**M2 — Two real clients.** Host and guest each run the game; guest's input drives the
guest's puppet, host's simulation is authoritative. Packet loss/latency tolerated for
movement; guards/mission state still host-only (guest is a companion, world diverges
silently).

**M3 — World honesty.** Sync the pieces the [`HumanDecision` / `Scene` / `EventAccumulator`
graphs] drive: AI awareness/combat at least at the "does the guest desync the host" level,
then mission/scene state. This is where AC1's scene-driven design will hurt most.

**M4 — Transitions and saves.** `WorldTransitionManager` + the (still unknown) save format.
Guest follows the host across city/level loads and save/load.

**M5 — Shipping.** One-click host/join, Steam relay with no port forwarding, README with
known issues. QA per [automated-qa.md](../../playbook/automated-qa.md): scripted two-client
tests (mock Steam DLL per the networking page) rather than owner playtests.

## Known risks

1. **Spawning a second driven humanoid** is unproven and may be impossible without deep
   engine work — see M1. This is the single biggest unknown and should be attacked first.
2. **AC1 is scene/event driven.** Missions are sequences of condition clips and event
   callbacks; making both clients agree on scene state is the hardest sync problem here.
3. **32-bit, closed binary, no scripting layer.** Every hook is native. Ghidra work is
   required; a decompilation does not exist.
4. **Tiny community / no prior art.** No co-op framework on the Scimitar family to lean on
   (AC2 is single-player too; netcode only appears from Brotherhood).
5. **Version lock.** Addresses are per-exe (DX9 vs DX10, and patch level). Two players must
   run the same build — the build here matches EaglePatch's supported hashes.
6. **Third-party state on the owner's install.** A local `ac1-fix` package has already
   modified both exes. Test copies must start from the clean originals in `.ac1-fix-backup/`.

## What we would ship back to coop-anything

- `data/directory.yaml`: AC1 entry set to a gap today; flip to a project entry once M1 runs.
- This write-up + `NOTES.md` (every fact sourced), a playbook note if AC1 teaches something
  new about scene-driven engines, and a claim issue so other agents find the work.
