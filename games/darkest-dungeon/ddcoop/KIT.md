# 0.5.0-test58

Battle actions carry persistent actor/friendly-target IDs. Door/loading blocks
outside-combat item use; inventory use has no walking-pixel delay. Bag repairs
verify their final hash. Apply failure sends a peer stop outside the FIFO.
See runtime/DESYNC.md; this stops divergence, not an automatic combat repair.

Validation performed locally on Windows x64: native protocol/fuzz/town/runtime
bridge tests, isolated OpenGL overlay, MSVC ASan runtime bridge, launcher tests
including live-party IPC and fake-Steam preparation/restore. All120 game address
patterns match the pinned executable. Prior native ABI/read-only DLC checks pass.
No real game, installer, owner save or Steam account was launched by these tests.

New behavior has not yet been validated in a live two-PC run. Reports from the
previous kit are evidence for the regression fixtures, not proof of this repair.
Private raw logs are deliberately excluded. Host save preference is retained;
guest always-nosave. Both players must use the same runtime kit. Builds use /MT.

Per-kit GitHub source commits are now part of delivery. Source and test evidence
are published; player ZIPs/binaries remain outside git.

Companion build: **0.5.0-duo41** uses the same gameplay source with DD_DUO,
its separate runtime pin and launcher version. RELEASES.json records both
kits and their exact binary/ZIP hashes; binaries remain outside git.

Test57 adds the shared dungeon default-order button and keyboard shortcut. Either
player may initiate it; actual native reset, default and post-state are checked on
both copies. Repeated/crossed clicks and animation delays are covered by bridge
fixtures. A formation difference stops before new travel. See runtime/FORMATION.md
and tests/test_formation_layout.py for the exact native ABI proof. Manual dungeon
formation drags are not newly synchronized. Real two-PC validation remains pending.

Test58 adds a scoped normal Raid Restore guard for subsequent fresh departures.
Initial campaign restore stays native; the bridge regression models initial
resume followed by two missions while guest disk remains unchanged. Mission,
party and seeds must match. Exact optional restore branch and13-byte prefix are
verified against the pinned executable. See runtime/FRESH-RAIDS.md.

English launcher and DDCoop notices, synthetic GIF animation/timing/paint/cleanup
fixtures and hidden native layout preview also passed. The supplied player GIF
is embedded in binaries but excluded from source. Optional local artwork can be
provided by developers. No live two-PC lifecycle test or MinGW build was run.
Rejoining a running host after disconnection remains refused; restart both games.
