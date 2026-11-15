# Step 1 — Gate 0: can Onyx netcode be started?

This file belongs to one agent step. Do not edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`
or `data/` — the integrator folds this file into them once no other agent is running
(see `../PARALLEL.md`).

- Prompt: `prompts/01-net-reachability.md`
- Agent: step01 (net reachability)
- Branch / worktree: `game/south-park-the-stick-of-truth-step01-net`, worktree `sp-step01`
- Base: `game/south-park-the-stick-of-truth` at 820077c
- Launch lock held: two series, each under `launch_lock.py` (owner `step01`), taken by
  `run_net_probe.py` and released in `finally`. Series 1 (single) waited about 11 min behind `step02`
  and under a minute behind `step03` (the logged waits), then ran. Series 2 (pair) acquired the lock right after
  series 1 released it. No series was killed by the probe.
- Launch series used: 2 of 4 allowed.

## Verdict table (Gate 0)

| Question | Verdict | Evidence (run folder, counter, screenshot) |
|---|---|---|
| Host path (session or server created, or socket listening) | **no, in menu-only runs** (reachability of a host path after a console `restart_server` is **unknown**) | Single `b179cee8…`: `bind`=0, `listen`=0, `accept`=0, `SteamMatchmaking` getter=0 calls, `SteamGameServer` getter=1794 calls, all returning NULL, `SteamGameServer_Init`=0. Pair `f6dfa69f…`: same zeros in A and B. |
| Client path (`ws2_32!connect` or `WSAConnect` fires) | **unknown — blocked on console `connect` (step 3)** | `connect`=0 and `wsaConnect`=0 in all 3 instances. No console command could be executed on this branch, so the client was never started through `connect`. |
| Two instances (both run side by side) | **yes, both ran** | Pair `f6dfa69f…`: A and B both alive at the 80 s stop, `guard_integrity` true, separate redirected profiles, screenshot B shows the title menu. |
| Two instances (join) | **unknown** — no join was attempted, because there is no client trigger | Same run: no connect, no bind/listen, no P2P or lobby call. |

## Observed (ran and saw)

Launch series 1, single instance, menu only, 80 s:
- Command: `py -3.14 -B run_net_probe.py --mode single --seconds 80 --lock-wait 3600` (with
  `PYTHONPATH` set to the owner's pinned frida runtime folder). Run folder `local/game-runs/b179cee81c314db6aab6b7c14ef35742/`.
- Validity: `original_profile_unchanged`=true, `guard_integrity`=true, `original_exe_hash_still_matches`=true, `errors`=[].
- Winsock: `WSAStartup` 2, `socket` 3, `closesocket` 2; `bind`, `listen`, `accept`, `connect`,
  `WSAConnect`, `send`, `sendto`, `recv` all 0.
- Steam: `SteamAPI_Init` 1; `SteamAPI_RunCallbacks` 1794; `SteamNetworking()` getter 1794, and its
  vtable slot 2 fired 1794 times; `SteamUser()` getter 1, its slot 2 once; `SteamGameServer()` getter 1794 calls,
  returned NULL 1794 times. `SteamMatchmaking`, `SteamFriends`, `SteamGameServer_Init`: never called.
- Interface hooks: `SteamNetworking` 22 vtable slots hooked, `SteamUser` 22. Only slot 2 of
  SteamNetworking ran.
- Screenshot `game-window-A.png`: title menu, Continue highlighted, 30 FPS overlay.
- Guard events: `guard-ready` 1, `net-probe-module` 2, `net-probe-ready` 1, `blocked-write-open` 14, `redirect` 20.
  `new_game_files` = 0 (nothing new written to the test copy).

Launch series 2, pair, menu only, 80 s:
- Command: `py -3.14 -B run_net_probe.py --mode pair --seconds 80 --lock-wait 3600`. Run folder
  `local/game-runs/f6dfa69f7fc14d2ab843122e3b4681d2/`. Instance A is plain; instance B has `-multiple_instance`
  and no `-connect`.
- Validity: all three flags true, `errors`=[].
- A and B both: `wsaStartup` 2, `socket` 3, `closesocket` 2, `bind` / `listen` / `accept` / `connect` /
  `WSAConnect` / `send` / `sendto` / `recv` all 0. `SteamGameServer()` NULL, `SteamMatchmaking` never
  called, `SteamGameServer_Init` never called.
- A: `SteamAPI_RunCallbacks` 1775, `SteamNetworking()`+slot 2 1775 each.
- B: `SteamAPI_RunCallbacks` 1779, `SteamNetworking()`+slot 2 1779 each.
- Screenshots: B shows the title menu (`game-window-B.png`). A is a fully black frame (`game-window-A.png`,
  2.7 KB). The cause is unknown. It is probably capture order or focus, since the capture helper
  uses the foreground BitBlt path. A was still alive at the end, so this is a capture
  problem, not a crash.
- Guard events for both instances: `guard-ready` 1 each, no `guard-error`.

Earlier evidence (from the notes already on the branch, not re-run here): `-connect` on the command line
produced no `connect`, in one run and in the two-instance run of 2026-11-15.

## Derived (concluded from evidence, with the evidence named)

- No host or lobby call happens at the title menu. Evidence: zero `bind`/`listen`/`accept`, zero
  `SteamMatchmaking` getter calls, `SteamGameServer()` returns NULL. Confidence: high for "menu only", since
  both runs agree and the menu is the only state reached. Says nothing about after a level or console start.
- The game polls `SteamNetworking` about 22 times a second even at the menu. Evidence: 1794 and 1775 calls in
  80 s. Confidence: high. The slot number is an index only; its name is unknown.
- The game has no game-server interface at menu time. Evidence: `SteamGameServer()` NULL 1794 times in both runs.
  Confidence: medium. The NULL could come from Steam being in a state that makes the interface unavailable.
- A second instance starts, runs, and stays alive. Evidence: pair run, A and B both alive at 80 s, guard clean.
  Confidence: high for "both run". Not evidence that the two can join.
- The client row cannot be decided from this branch. Evidence: no console on this branch can run `connect`, and the
  command-line route is dead. Confidence: high.

## Unknown

- Whether a session or server can be created at all. Needs a console `restart_server` (step 3) or a level start with a known level name (`-startlevel <level>`, level names unknown).
- Whether `connect <ip:port>` from the console reaches `ws2_32!connect`, and what error it gets. Needs step 3's console.
- Whether a `SteamNetworking` vtable slot 2 is `IsP2PPacketAvailable` or another call. The probe only gives the index, and the SDK header order is not in this repo.
- What caused the black A frame in the pair run.
- Whether the `SteamGameServer()` NULL is a menu-only effect.
- What the `SteamUser` slot 2 call does.
- Whether a second `Player_Object` can exist. Not tested by this step.

## Method notes (for reproduction, not evidence)

- `launch_lock.py status` shows `recorded_process_alive: false` for the lock, while the lock is still held. This field is
  misleading. The lock stores the PID of the `acquire` CLI process, which exits right away. Only expiry or an explicit
  `release` by the owner frees the lock. Do not treat `recorded_process_alive: false` as a stale lock.
- A first queued attempt (`--lock-wait 1800`) was stopped after about one minute of waiting, because `step02`'s
  lease (about 2150 s left) outlasted that wait. Only my own waiting powershell/py/python processes were stopped
  (no game process, no lock file touched). It never acquired the lock and left no run folder. The retry used `--lock-wait 3600`.
- Frida runtime: `PYTHONPATH` points at the owner's pinned frida 17.25.2 folder (the one under the main repo's `local/`). The
  worktree has no `local/runtime-libs` of its own, and the wheel was not copied or reinstalled.
- Run folders are under `local/game-runs/` (git-ignored, per worktree). Only the two valid runs exist, no others.

## Deliverable

- Files: `tools/net_probe.js` (extended: Winsock 27 hooks incl. `connect`/`WSAConnect` return codes, Steam
  getters and their vtable slots), `tools/run_net_probe.py` (rewritten: `--mode single|pair`, lock, guard,
  validity flags, screenshots; `--connect` removed on purpose).
- Reproduce: `cd games/south-park-the-stick-of-truth/tools` and set
  `PYTHONPATH` to the pinned frida folder, then
  `py -3.14 -B run_net_probe.py --mode single --seconds 80 --lock-wait 3600` and
  `py -3.14 -B run_net_probe.py --mode pair --seconds 80 --lock-wait 3600`.
  Each run takes and releases the lock itself.
- Run folders (in the worktree's `local/`): `game-runs/b179cee81c314db6aab6b7c14ef35742/`
  (single), `game-runs/f6dfa69f7fc14d2ab843122e3b4681d2/` (pair).
- Launch series used: 2 of 4.

## Handoff notes for the integrator

- README/NOTES: "Gate 0 host path: no call observed at menu (2 runs, 80 s each, 0 bind/listen/lobby/matchmaking/game-server
  calls). Reachability of a host path after a console `restart_server` is unknown."
- README/NOTES: "Gate 0 client path: unknown. The client cannot be started without console `connect` (step 3). No
  `connect` or `WSAConnect` call in any run, including the earlier `-connect` runs."
- README/NOTES: "Gate 0 two instances: both run side by side with `-multiple_instance` (pair run, guard clean). Joining is unknown,
  because no client trigger exists yet."
- NOTES: "The menu polls `SteamNetworking` ~22/s and `SteamGameServer()` returns NULL; `SteamMatchmaking` is never requested."
- NOTES: "`launch_lock.py status` field `recorded_process_alive` is not evidence of a stale lock; only expiry or the owner's `release` frees it."
- PLAN: "Gate 0 is not decided yet. The decision waits for step 3's console `connect`/`restart_server`. Until then, the
  Track A/B split cannot be made on evidence."
