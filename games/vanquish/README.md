# Vanquish (2010)

| | |
|---|---|
| Engine | Platinum Engine (in-house, C++; Havok physics) |
| Versions tested | none (research only) |
| Projects | none found |
| QA level | none |
| Last updated | 2026-10-22 |

## Where things stand

Nobody can play Vanquish co-op today, and no project is trying. The game has no
multiplayer of any kind, and nothing usable exists to revive. This is a
"build from zero" gap, and a hard one: a closed, custom-engine console port
with no netcode, no scripting layer and no same-engine co-op framework.

## Existing work

Searches (repo directory, GitHub repo and code search, web search incl. Nexus
and ModDB mentions, Steam forums) found no co-op or multiplayer project, alive
or abandoned. What exists on Nexus Mods for the PC port is limited to a Cheat
Engine table / trainer (ARS suit limiters, AR switcher, melee adjustments) and
texture/skin packs. Steam forum threads ask for co-op but contain no project.

Unknown: archive.org, Japanese-language modding communities (the Bayonetta /
Vanquish PC mod scene), any private Discord projects. Sources are in the
directory entry.

## How the game works (for co-op)

Facts and unknowns, since nothing was run on a local install:

- **Engine:** Platinum Engine, PlatinumGames' in-house C++ engine, the same
  family as Bayonetta and NieR: Automata (which runs a heavily reworked
  variant). Havok physics. No public source, no known decompilation.
- **Scripting:** `unknown`. The PC port stores data in package-style archives;
  tooling exists for Bayonetta PC archives (e.g. noesis plugins) and some of
  it is said to apply to Vanquish, but nothing was verified here.
- **Netcode:** none. PlatinumGames confirmed at release the game has no
  multiplayer modes and none were ever built; the PC port is a straight
  port of the console build.
- **Slow motion:** the core mechanic (Augmented Reaction mode, rocket-sliding)
  is a local time-scale change, which makes a shared-world design much harder
  than a normal TPS: either every client slows when one player triggers AR
  mode, or AR becomes local-only and desyncs combat.

## What we changed

Research only: directory entry and this write-up.

## QA

Not applicable (research only). `python -I tools/test_all.py` was run; one
pre-existing unrelated failure (`check_msvc_environment[vcvars_ignored]`,
sandbox has no MSVC).

## How to play

Not possible today.

## Known issues

Not applicable.

## Next steps

For whoever picks this up, in order:

1. Record the game's identity: Steam build id of the current exe and its
   SHA-256, archive formats on disc.
2. Survey tooling from the sibling-game modding scene (Bayonetta PC, NieR:
   Automata) — archive unpackers, Noesis plugins, Cheat Engine table symbols
   from the existing trainer. These give the first hooks into the binary.
3. Static analysis in Ghidra of `Vanquish.exe`: find the game-state layout,
   the player actor, the input and camera paths. The existing CE table's
   known addresses are the entry point.
4. Decide the networking model per the playbook. Host authority with state
   sync is the only realistic option, and AR-mode slow motion needs a design
   decision before any code is written.
5. This is a long project. Realistic expectations: months of reverse
   engineering before two Sam models stand in the same room.
