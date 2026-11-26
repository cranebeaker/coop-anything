# The Witcher, Enhanced Edition (2007)

| | |
|---|---|
| Engine | BioWare Aurora (NWN lineage) |
| Versions tested | Steam Enhanced Edition, game version 1.4.5.1304 |
| Projects | one private prototype (not published); no public co-op mod found |
| QA level | none (agent-driven log checks on loopback, no test suite yet) |
| Last updated | 2026-11-17 |

## Where things stand

A private prototype runs two game copies on one PC over TCP: both characters and NPCs are synchronized,
area changes work in both directions, and a test cutscene injected on the server can be shown to both players
or to one. Combat, dialogue and quests are not done.

## Existing work

None in this directory. A wider web search for other attempts was not done. Raw engine notes and the
cutscene/world-pause finding are in `NOTES.md`.

## How the game works (for co-op)

The Aurora client/server message layer is still inside the game and runs in-process in single player, so it
can be revived. See `NOTES.md`.

## What we changed

Cutscene handling: the root cause of a frozen world after a second player's cutscene was that Input 6.25 is a
world-pause toggle, not a cutscene-ended signal. Fix: never forward a remote client's 6.25; end detection from
the client's cutscene player. The host's own cutscene keeps the engine's pause (running the world during it
crashed the host's client afterwards). Proof: runs listed in `NOTES.md`.

## QA

No suite yet. Runs were agent-driven and checked from the mod's log lines (pause mask, message counts,
host movement during and after the cutscene).

## How to play

Not released.

## Known issues

Only the injected test cutscene was exercised; scripted cutscenes and their on-end scripts are untested.

## Next steps

Test a scripted cutscene started by the second player; revive combat and dialogue messages.
