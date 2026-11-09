# 0.5.0-test56

Battle actions carry persistent actor/friendly-target IDs. Door/loading blocks
outside-combat item use; inventory use has no walking-pixel delay. Bag repairs
verify their final hash. Apply failure sends a peer stop outside the FIFO.
See runtime/DESYNC.md; this stops divergence, not an automatic combat repair.

Validation performed locally on Windows x64: native protocol/fuzz/town/runtime
bridge tests, isolated OpenGL overlay, MSVC ASan runtime bridge, launcher tests
including live-party IPC and fake-Steam preparation/restore. All117 game address
patterns match the pinned executable. Prior native ABI/read-only DLC checks pass.
No real game, installer, owner save or Steam account was launched by these tests.

New behavior has not yet been validated in a live two-PC run. Reports from the
previous kit are evidence for the regression fixtures, not proof of this repair.
Private raw logs are deliberately excluded. Host save preference is retained;
guest always-nosave. Both players must use the same runtime kit. Builds use /MT.

Per-kit GitHub source commits are now part of delivery. Source and test evidence
are published; player ZIPs/binaries remain outside git.

Companion build: **0.5.0-duo39** uses the same gameplay source with DD_DUO,
its separate runtime pin and launcher version. RELEASES.json records both
kits and their exact binary/ZIP hashes; binaries remain outside git.
