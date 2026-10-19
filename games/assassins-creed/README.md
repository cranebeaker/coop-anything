# Assassin's Creed (2007)

| | |
|---|---|
| Engine | Ubisoft Scimitar (the original Anvil; renamed AnvilNext from AC2) |
| Versions tested | Steam Director's Cut, exes `1.0.2.1` (DX9 + DX10). Identity and hashes in `NOTES.md`. |
| Projects | none found — this is a gap. Tooling to build on: [EaglePatch](https://github.com/Sergeanur/EaglePatch) |
| QA level | none |
| Last updated | 2026-10-19 |

## Where things stand

There is **no way to play AC1 co-op today.** No co-op mod exists, no multiplayer netcode
was ever shipped in the engine, and there is no scripting layer or decompilation to hook
cheaply. What *does* exist is a working native-patching precedent (`EaglePatch`) and full
RTTI class names in the exe, so a from-zero custom sync layer is at least feasible. Plan:
`DESIGN.md`. Everything here is research only — nothing has been built or tested.

## Existing work

Searched GitHub (repo + topics), DuckDuckGo (co-op / multiplayer / engine phrasings),
Nexus/ModDB via search snippets, and another agent's engine-family survey. Full table with
links in `NOTES.md`. Summary:

- **No AC1 co-op mod.** No co-op mod for *any* AC game was found; the survey reached the
  same conclusion.
- **[EaglePatch](https://github.com/Sergeanur/EaglePatch)** — ASI plugin patching AC1/AC2
  natively (XInput, multisampling, telemetry, intro-skip). Not co-op, but the reference for
  how to hook this exe (hardcoded addresses, engine allocator wrappers, vtable calls). Last
  release v1.1, 2025-12-02. The local install's exe hashes match its supported builds.
- **[ACExplorer](https://github.com/gentlegiantJGC/ACExplorer)** (archived) and Turfster's
  `.forge` extractor/replacer — `DataPC_*.forge` archive tooling.
- Community view (r/assassinscreed) is that AC games are unusually hard to mod; existing
  AC1 mods are mostly retextures.

## How the game works (for co-op)

- **Engine:** Scimitar/Anvil, 32-bit, DX9 + DX10 exes. Content in ~6 GB of `DataPC_*.forge`
  archives. The port is console-derived (pad classes for Xenon/PS2; 8 pad sets in the input
  stack). Audio uses DARE (`DARE.INI`, DirectSound3D).
- **Netcode: none.** Raw scan of the clean exe: `WSAStartup` ×1, `socket` ×1, `gconnect`
  ×1 (removable Ubisoft telemetry); `DirectPlay` / `NetDriver` / `Replication` / `Udp` = 0.
  Series multiplayer only arrives in Brotherhood.
- **Reverse engineering surface:** full MSVC RTTI — 4084 `scimitar::` classes, 2575
  top-level. Gameplay names include `Human`, `HumanDecision`, `HumanNPCDecision`,
  `PDeAssassin`, `AIGlobalManager`, `Scene`, `SceneHandler`, `WorldTransitionManager`,
  `AssassinationConditionClip`, `AltairRanker`-style rank/progress classes, and `AnimTrack*`
  animation types. Missions are scene/event driven (`EventCallback`, `EventAccumulator`).
- **Save system:** location and format **unknown** — no saves exist on the test machine yet.
- **Build lock:** addresses differ between DX9 and DX10 (and patch level). Both clients must
  run the same exe; ours matches EaglePatch's confirmed GOG/Steam hashes.

## What we changed

Nothing yet. Research and a design document only (`NOTES.md`, `DESIGN.md`).
The owner's install also carries an unrelated third-party `ac1-fix` package that predates
this session (see `NOTES.md`); we did not write or run it.

## QA

No tests. QA level `none`. When M0/M1 exist, automated two-client tests per
[playbook/automated-qa.md](../../playbook/automated-qa.md) will replace any playtest.

## How to play

Not available. No release exists.

## Known issues

N/A (nothing playable). Biggest technical risk: whether a second, driver-controllable
humanoid can be spawned at all — see `DESIGN.md` milestone M1.

## Next steps

1. Decide with the owner whether to commit to the from-zero build (personal recommendation
   is in this write-up's opening: consider AC Unity's official co-op or the in-development
   "Origins Together" first).
2. If yes: isolated test install from the clean `.ac1-fix-backup` exes, Ghidra project, and
   the **M0 logging ASI skeleton**.
3. Attack **M1** (spawn a second driven `Human`) before anything else — it decides the design.
4. Claim the game with an issue on coop-anything so other agents find this work.
