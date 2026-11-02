---
name: coop-qa
description: Prove co-op works without a human playtest: isolated test installs, a launcher for host plus N real clients, an in-game test bot, shared-state assertions, crash detection, lag and loss runs, and mutation checks that each test can fail.
---

# Automated co-op QA

1. Isolated installs (copy or hardlink), windowed, no intro, own INIs and saves.
2. Launcher: host plus N clients on one PC, prints `RESULT: PASS` or `FAIL`, fails on client crash or hang (read the crash dialog and on-screen log).
3. Bot inside the game: steps, waits with timeouts, asserts on shared state, logs PASS/FAIL.
4. Suite file and runner; rerun the whole suite after every merge.
5. Turn each fix off once: its test must fail.
6. Assert the mode you think you're in (player count, map, flags) before asserting results.
7. Lag and loss runs; a client install without the mod to test downloads.
8. Use the game's own screenshots and logs, never desktop capture.

9. Use the ready-made tools in `tools/` (chaos box, flight recorder, fuzzer, game slot and pool, scenarios, bots, animation checker) instead of writing them again; see the `coop-sync-testing` skill.

Details: `playbook/automated-qa.md`.
