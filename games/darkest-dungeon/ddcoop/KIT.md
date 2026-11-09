# 0.5.0-test55

Live party IDs are published by the runtime through an own-PID read-only
launcher mapping. Launcher/F9 sort and mark actual party members independently
of ownership revision. Stale/unknown reports remove markers.

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
