---
name: coop-revive
description: Finish an existing but buggy, stalled or abandoned co-op mod: get its last build, audit its code by subsystem, fix it as a separate package, and test it with real game clients.
---

# Revive an existing co-op mod

1. Get the newest build (check build servers and release pages, verify by hash).
2. Get readable code: dump scripts, decompile native parts, get the base game's reference source.
3. Audit by subsystem with parallel read-only worker agents; verify every finding before fixing.
4. Fix without rebuilding the original: a new package with subclasses swapped in through the mod's own hooks, one module per subsystem. Keep `BUGS.md` (fixed, open, refuted, out of reach).
5. Live test a host plus a real client on one PC, then turn every fix into an automated test (`coop-qa`).
6. Make it easy: one install file per OS (`playbook/shipping-to-players.md`), one-click host and join, Steam relay (`coop-networking`).
7. Credit the original author and offer the fixes upstream.

Details: `playbook/reviving-abandoned-mods.md`. Worked example: games/deus-ex.
