# Sync contract — what must be shared for smooth co-op

Draft 2026-10-21, to be confirmed. This is the **Soll** for synchronisation; the
current Ist is in [../STATUS.md](../STATUS.md), the measured differences in
[05-sync-gaps.md](05-sync-gaps.md). Nothing here is a claim that it works.

Rule of thumb: **host decides, client shows.** Each item has an authority (who may change it),
a quality bar (what the player must not notice) and how a test could prove it.

## A. Must be identical on both sides (determinism)

| # | Item | Authority | Bar | Proof |
|---|---|---|---|---|
| A1 | Run config: mission, sector, node, biome, hazard, **seed** | host | client never builds a map before the config arrived | fingerprint of the cave equal; test with the host start delayed (seed-race twin) |
| A2 | Map layout, veins, ore, mission objects (Bobby, detonators, TNT zone, fuel) | seed | same positions | `worlddiff` census, 0 differences outside the ignore list |
| A3 | Class, class mod, skin of **each** dwarf as the other side sees it | each player, announced to the other | host sees the client's real class/skin and vice versa | puppet renderer count + skin id in both logs |
| A4 | Host unlocks (weapons, upgrades, artifacts) usable by both | host | client plays on them, **default on** | live-read test with a non-default unlock |

## B. Host-authoritative world state (client only mirrors)

| # | Item | Bar | Proof |
|---|---|---|---|
| B1 | Enemies: spawn, position, hp, death | motion looks continuous (target ≥ 10 Hz or extrapolation; today 2 Hz), no ghost/missing enemies | census equal; position error under a lag profile |
| B2 | Projectiles, grenades, turrets, beams of **both** players | both see both; nothing accumulates | census; object count flat over time (G3, G4) |
| B3 | Loot and pickups: spawn, one claim removes for both, no doubles | no duplicate loot (G5) | claim test + census |
| B4 | Terrain / mined cells | both see the same holes | terrain mirrored counts |
| B5 | Objectives and mission progress, stage end, victory/defeat | same state, same end screen on both | RESULT on both roles |
| B6 | Pause, level-up and shop barriers | never longer than the other player's real choice time; timeout is the last resort (G1: 45 s freezes today) | hold time asserted, not only "no timeout" |

## C. Per-player state, reported to the host

| # | Item | Bar | Proof |
|---|---|---|---|
| C1 | Position, facing, animation, weapon aim (20 Hz) | smooth, not teleporting | puppet vs. real position error |
| C2 | Hp, **downed** (no movement or attack), revive circle that completes, death | the downed player really cannot move; revive finishes with both present | state-machine test (today: moves, never completes) |
| C3 | Damage the client deals and takes | host applies once | `EnemyHit` counts |
| C4 | XP is shared, level-up cards are chosen per player | different cards, same level | card test |
| C5 | Gold/Nitra pool and shop purchases | one wallet, host arbitrates | shop barrier test |
| C6 | Client's own earnings (meta) stay separate from the host's | host profile is read, client save untouched | save before/after |

## D. Session lifecycle (where the real-test bugs lived)

| # | Item | Bar | Proof |
|---|---|---|---|
| D1 | Connect, ready gate, lobby, start together | both load the same level, neither starts alone | lobby-start test; host/client start order swapped |
| D2 | Loading: each waits for the other | no one plays alone | load hold test (exists) |
| D3 | Run end → back to menu → **second run** without restarting | CO-OP button is rebuilt, no stale state | new test: two runs in a row (missing; the real test found the button gone) |
| D4 | Peer lost mid-run | notice, no freeze, clean exit | exists |
| D5 | Reconnect mid-run | optional; today both must restart | not built, decide scope |
| D6 | Lag: 50 ms, 150 ms + jitter + 2 % loss, 300 ms + 5 % loss, 3 s blackout | no desync, no freeze, no duplicate events | **no latency harness yet** |

## E. Presentation only (nice to have, must not change state)

Off-screen marker, partner name/health panel (out of scope per S14), vanilla camera (no co-op
framing, decision 5), no visible wizard clicking when a run is
started from the lobby, notices.

## Decisions (2026-10-21)

1. **Enemy motion (B1):** snapshots from 2 Hz to **10 Hz**, then measure.
2. **Host unlocks (A4):** the host's unlocks apply to both, **on by default** (today only with
   `-coopprofileadopt`, which the release does not set).
3. **Reconnect (D5):** **not needed.** If a player drops, the host plays on; the client does not
   reconnect (the client run ends cleanly, as built).
4. **Lag bar:** 150 ms + jitter + 2 % loss with no freeze and no desync must pass.
5. **Camera (E):** **stays as in the base game**, following the own dwarf. If the other dwarf runs
   off screen, that is accepted; the off-screen marker shows where they are. So the co-op camera
   that frames both dwarves (`CoopCamera`) is to be switched off, and the `camera` test and the
   STATUS row "Camera frames both dwarves" are retired and replaced by "camera is vanilla,
   marker visible when the partner is off screen".

## Open: not yet decided

Mission types beyond Elimination/Escort (S9 says main missions only), Steam relay (S10),
aggro per player (deferred).
