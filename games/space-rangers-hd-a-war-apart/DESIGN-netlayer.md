# Co-op netting layer — design draft (Space Rangers HD planetary battles)

Status: DRAFT v1 (2026-11-04). Facts marked [V] are verified in the GPL source clone
(murgesku/MatrixGame @ 06e4d32 + local port fixes, fully rebuilt). Everything else is
a design decision — feedback loop is the smoke test + tools in `local/analysis/`.

## 1. Ground facts the design rests on [V]

- The battle is a self-contained run: Rangers.exe calls `m_Run(HINSTANCE,HWND,map,…)`.
  Everything inside is our GPL code — no closed-source seam inside the battle.
- Sides are already a first-class concept: `CMatrixMap::m_Side[m_SideCnt]` is built in
  `LoadSide()` (MatrixMap.cpp:999-1041) from the `robots.dat` `Side` block, which holds
  **4 sides + 1 neutral** (`ASSERT(cnt == 5)`). `PLAYER_SIDE` is the constant `1`
  (MatrixSide.hpp:16); `m_PlayerSide` is a pointer into the array chosen by that id.
- Control is order-based: `SOrder { OrderType; OrderPhase; m_Param1..3; }`
  (MatrixRobot.hpp:123-164). Manual driving and AI produce the same `SOrder` vocabulary
  (`ROT_MOVE_TO/BACK/RETURN, ROT_STOP_MOVE, ROT_FIRE, ROT_STOP_FIRE,
  ROT_CAPTURE_FACTORY, ROT_STOP_CAPTURE`). `OrderPhase` = execution stage.
- `ROBOT_FLAG_DISABLE_MANUAL` + side group logic (MatrixSide.cpp:6573+) already keep
  player-steered robots out of group AI orders.
- The deterministic loop: `CMatrixMapLogic::Takt(step)` → `CMatrixMap::Takt(step)`
  (MatrixLogic.cpp:2469 / MatrixMap.cpp:2488), engine pumps the game via
  `g_RangersInterface->m_Takt()`.
- Battle result: `SRobotGameState` (m_BuildRobot / m_KillRobot / m_BuildTurret /
  m_KillTurret / m_KillBuilding / m_Time) returned by `m_Run` to the galaxy sim.

## 2. Topology for MVP: host-authoritative battle run, 2 humans

- **Host** runs the full battle sim the normal way; his own robot is driven by his local
  input (identity path — zero code change).
- **Guest** sends his orders to the host and receives the world.
- Second human sits on side id 2 (host on id 1). Both sides' robots belong to "playable"
  units; sides 3/4 stay as shipped AI.
- Ownership rule: **the owner client is authoritative for its own robot's orders**;
  the host is authoritative for everything else (NPC bots, buildings, effects, capture
  counters). Pose corrections flow one-way host→guests for non-owned units.

## 3. What we sync

Channel A — reliable, ordered (TCP-like):
- join/handshake (map file name + robots.dat hash + side assignment),
- the **order stream**: every `SOrder` produced by a human,
- capture/factory events, kill notifications, battle end + `SRobotGameState`.

Channel B — unreliable, sequenced (UDP):
- per-tick pose deltas of units the guest does not own: position (3 floats), heading,
  `m_Phase` (`ROP_*`) of executing order, turret state compressed as engine already does,
- effect markers (fire/plasma bursts) as fire-and-forget events.

Tick rate: engine `Takt(step)` is the natural cadence [V]; deltas flush per tick,
orders flush per command. 2-player, LAN/internet with late 1990s asset flow —
bandwidth < 30 KB/s expected.

## 4. Code injection points (all inside GPL tree)

- `CMatrixMapLogic::Takt` — before/after the step: send queued deltas, apply received
  orders to `CMatrixSideUnit`-internal bots (same call the interface uses).
- `CMatrixSideUnit` — add `m_Human(bool)` / `m_NetId` (which connection drives it);
  ownership partition in group logic loops.
- `CMatrixRobotAI` — flag `m_RemoteOwned`; when set, skip local input/selection draws
  and the AI side logic; orders arrive from net queue only.
- `MatrixGameDll.cpp` `m_Run` — accept an extra net-bind descriptor (listen/accept +
  which side_id is ours). For the standalone smoke build: bind/enter session after
  form init.
- RNG discipline: NPC logic reads rng; host is the only one who runs NPC takt as
  authority, guests never diverge because they DON'T run NPC logic as authority —
  they replay host assertions. If we later want soft-lockstep, we must pin the rng seed
  and audit float paths; not required for MVP.

## 5. Library choice

ENet (MIT/Zlib-style, tiny, battle-tested, 1 UDP socket, channels+reliability):
vendored copy under `ThirdParty/ENet` + CMake `add_library`.
Steam sockets (steam_api.dll) rejected for MVP: license surface + the game's own
steam_api.dll must not be modified for redist builds.

## 6. Open questions (check at runtime, then close)

- [O] Does any code path assume exactly one side with `m_Human`-like notion (e.g.
  `IsFaractic()` / victory tally reads `m_PlayerSide->…`)? CInterface reads
  `player_side` broadly → find all its callers, define `g_CoOpPlayerMask`.
- [O] `SRobotGameState` aggregation across sides — m_KillRobot etc. are struct-level;
  need per-side attribution for a meaningful report to the galaxy sim (matters for L2).
- [O] Which Takt phase is safe to mutate orders from a workshop thread (engine is
  single-threaded for logic; network read must land in a queue and be consumed inside
  `Takt` as a design rule, not as a thread).
- [O] Map identity verify: map arg + `robots.dat` hashes; on mismatch refuse to join.

## 7. Milestones (each independently checkable)

Status (2026-11-06): M1 CLOSED — ENet v1.3.18 vendored+pinned, harness
(MatrixGame/src/Net/SRNetHarness.*, env-gated) compiled into DLL+exe,
OFF-by-default verified by smoke_coop_m1.ps1 (NO srnet.log when off; host
listens; guest socket up), ABI parity kept (single GetRobotInterface export).
Details in NOTES.md escalation 9.

1. M1 — "invisible harness": net layer compiles into engine, all switchable OFF
   (`CFG.TXT` / env var); behavior with co-op OFF byte-identical (verifiable by
   running the same battle input twice).
2. M2 — standalone 2×engine on one machine: two MatrixGame.exe (matrixgame -DLL), host
   listens; both load Map_1.map; both parties' bots move (host runs authority, guest
   has order injection + pose replay). Testable fully headless via `calcvis.log`-style
   smoke + in-window observation later.
3. M3 — turn a real planetary battle inside test-install on 2 LAN instances of
   Rangers.exe + our MatrixGame.dll, side 1 & 2 humans, shared tally,
   2 games record ≤1 desync per 5 min.
4. M4 — upstream-PR-ready: ports (#1-#5 in NOTES), ENet vendoring, co-op switch,
   README; tools `fakeworld`/`flight`-style regression scripts for sync.
