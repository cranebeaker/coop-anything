# Deep Rock Galactic: Survivor — working notes

Slug: `deep-rock-galactic-survivor`
Goal: online co-op (2 players) for a game that ships single-player only.

> **Read first:** [STATUS.md](STATUS.md) — the single Ist/Soll: what is verified, what is
> broken, what "done" means. This file is the index to the detailed working notes.
> Private operational state lives in `local/` (gitignored): `local/HANDOFF-current.md`
> (current state/install/playtest) and `local/mod/DrgSurvivorCoop/PLAN.md` (master plan).

## Current state (2026-10-20)

- **Direct-IP/LAN co-op is feature-complete on two instances** and verified by automated
  runs. Host-authoritative state sync: players, enemies, loot, terrain, XP/cards,
  objectives, settings, downed/revive, camera, lobby/ready/start, loading hold.
- **UI:** the CO-OP button → connect screen → mission lobby path is verified logically
  (`20261020-182244-menu`: `button=1 connect=1 peer=1 lobby=1`). **Visual defect:** the
  cloned uGUI **Images** (panel/background/button fills) do not draw on this build and the
  TMP text is faint, so the mission art shows through. The readable interim surface is the
  IMGUI fallback (`-LobbyIMGUI`). Details and evidence: [STATUS.md](STATUS.md).
- **Release:** `DrgSurvivorCoop-0.1.4` zips were verified from a clean copy
  (`20261020-145500-packageverify`). The two-PC install and smoke test are still pending.
- **Visual QA:** only the menu-button path is valid. Do **not** use `-cooplobby`/AutoHost
  for screenshots; that shortcut builds the lobby before the menu-entry path and produces
  a canvas that is active and clickable but not drawn.

## Verified at a glance

| Area | State | Evidence |
|---|---|---|
| Transport, two instances | ✅ | UDP 9050, peer connected, counters both ways |
| Shared cave / seed / puppet | ✅ | fingerprint 0/81 differ; `puppet spawned renderers=9` |
| Enemies: spawn/death/snapshot/hp | ✅ | `enemy transport … fail=0`, `snapshot applied=…` |
| Loot / terrain | ✅ | wallet deltas match; `terrain recv=3 mirrored=3 miss=0` |
| XP, level-up cards, pause | ✅ | identical totals; different cards per side; barrier released |
| Objectives, stage relay, shop | ✅ | `recv=26 applied=2 miss=0`; `starts=3`; `timedOut=0` |
| Settings adoption + handshake | ✅ | `drift=1`, `echo=1`, `held=…` |
| Downed / revive / circle | ✅ | `revived hp=100`; `hasFillFactor=1` |
| Damage both ways | ✅ | `sent=20 applied=20 dmgTotal=532`; incoming 8×10 hp |
| Combat visuals | ✅ | fire/beam/grenade/turret/Bosco/muzzle/trail; **draw not proven** |
| CO-OP button → connect → lobby | ✅ logic | `20261020-182244-menu` |
| Lobby state / ready / start | ✅ | `20261020-172313-lobby-ui`, `20261020-172401-lobby-start` |
| Loading hold + timeout text | ✅ | `20261020-123303-loadform`, `20261020-172822-loadtimeout` |
| BACK / off-screen marker | ✅ | `20261020-172235-lobby-back`, `20261020-172040-indicator` (− `172202`) |
| Release packaging / soak | ✅ | `20261020-145500-packageverify`, `20261020-040552-soak` |

The full matrix, the Soll (design requirements), the gap table and the definition of
"verified" are in [STATUS.md](STATUS.md).

## Known open items

- **Lobby visual:** root cause of the unrendered cloned Images (material / CanvasRenderer
  alpha / canvas registration); a fresh connect-screen capture; layout at 900×600.
- **Projectile draw oracle:** spawn is proven, "is it drawn" is not; out-of-process diff.
- **Steam invite/relay** (Spacewar), **aggro** (deferred), **I1 gear half**,
  **Mastery fixture**, **client milestones**, **two-PC smoke test**.
- **Marker:** `stale` and `downed` branches are code-only (no forced-silence/downed test).

## Detailed notes

The 3,840-line chronological log was split on 2026-10-20. Content is unchanged.

- [notes/01-core.md](notes/01-core.md) — identity, modding stack, engine model, early build log.
- [notes/02-transport-world.md](notes/02-transport-world.md) — harness, transport, enemies,
  loot, terrain, early milestones.
- [notes/03-combat-progression.md](notes/03-combat-progression.md) — damage, level-ups,
  settings, saves, shop, downed, combat visuals.
- [notes/04-lobby-ui-visuals.md](notes/04-lobby-ui-visuals.md) — turret mirror, lobby L0–L3,
  Bosco, UI pass (including the visual defect).

The pre-split file is archived privately at `local/archive/NOTES-2026-10-20-full.md`.

## Canonical test paths (proven)

```powershell
# Real UI path: menu button -> connect -> peer -> lobby
coop-run.ps1 -Menu -Test menu

# Hold the lobby for a screenshot (canonical path)
coop-run.ps1 -Menu -Test lobbyhold
coop-shot.ps1 -Phase lobby -Seconds 40 -Interval 2 -OutDir ..\work\shots\lobby

# Regressions
coop-run.ps1 -Ui -Lobby -Test lobby-ui
coop-run.ps1 -Ui -Lobby -Test lobby-back
coop-run.ps1 -Ui -Lobby -Test lobby-start

# Marker (+ negative), load hold, load timeout
coop-run.ps1 -Auto -Test indicator -CameraOff
coop-run.ps1 -Auto -Test indicator -CameraOff -IndicatorOff
coop-run.ps1 -Auto -Test loadform
coop-run.ps1 -Auto -Test loadtimeout -Only host
```

Rules: never launch the game without explicit approval; back up the save profile; use
1280×720+ for UI; one test = both roles + a run ID + a negative control where available.

## Notes discipline

- **STATUS.md is the current truth.** Do not duplicate its matrix here.
- This file is the index; the `notes/` files keep the chronological detail and the
  measurements behind each claim.
- "Code only" is not verified. A claim needs a run (`RESULT: … PASS`) and, for visuals, a
  window capture (CopyFromScreen + foreground; `PrintWindow` omits the overlay on this build).
