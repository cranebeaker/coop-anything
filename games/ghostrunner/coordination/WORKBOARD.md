# Workboard: Ghostrunner co-op

Live claims. Update **only your own row**. A row without a `Write scope` is not a claim.
Work items are ordered by what unblocks the most.

Status values: `in_progress`, `blocked`, `done`, `dropped`, `unowned`.

| ID | Work item | Owner | Status | Write scope | Next / evidence | Updated |
|---|---|---|---|---|---|---|
| W1 | Enter a gameplay level by the game's own flow | lead (deepseek, 2026-10-21) | **done** | `games/ghostrunner/NOTES.md` | title → `Ghostrunner` → `Level select` → `Classic mode` → `An Awakening` → `/Game/Levels/Tutorial/L_Tutorial_Persistant`. NOTES §7. | 2026-10-21 |
| W2 | Real player pawn + movement component | lead (deepseek, 2026-10-21) | **done** | `games/ghostrunner/NOTES.md` | `BP_PlayerCharacter_C` with a standard `CharacterMovementComponent` (MaxWalkSpeed 1000), skeletal mesh, `bReplicates=true`. | 2026-10-21 |
| W6 | Model A: revive UE4's own replication | lead (deepseek, 2026-10-21) | **closed (negative)** | — | Four independent negatives: `listen` no-op, `open ?listen` in-level no-op, command-line map URL ignored, `UGameplayStatics:OpenLevel(…,"listen")` crashes instantly. See NOTES §8. Reopen only with native hooking of LoadMap/travel, which UE4SS Lua cannot do. | 2026-10-21 |
| W10 | **Puppet primitive via second local player** ("peer slot") | lead (deepseek, 2026-10-21) | **done v0** | `games/ghostrunner/NOTES.md` | `CreatePlayer` gives a second pawn; `K2_SetActorLocation` drove it from an external target with **exact** position match on 9 consecutive ticks. This is the receive-and-drive half of a puppet, proven. | 2026-10-21 |
| W11 | Spawn a non-player puppet actor (nicer than a second viewport) | unowned | blocked | own mod folder | `BeginSpawningActorFromClass` is bound but fails for every argument variant, and the raised value is a `UFunction` object instead of an error string. Likely UE4SS `FTransform` marshalling. Try `StaticConstructObject` + `FinishSpawning`, or do the spawn inside a Blueprint `ModActor` (LogicMods). Not required for the first playable. | 2026-10-21 |
| W12 | **Two processes + file seam**: each instance drives the other's peer slot from a state file | unowned | ready | own mod folder + Python helper | The next real milestone. Use the world-triggered pattern (act on game state, never on wall-clock timers). Write state atomically (`.tmp` + replace) at 10-20 Hz, interpolate. | 2026-10-21 |
| W3 | Transport helper: file seam → UDP relay (Python, stdlib + `ctypes`) | unowned (Codex proposed taking it; needs a claim row) | unowned | separate repo or `tools/` scope to be agreed | Owner transport only (platform services dropped). Read `playbook/networking-steam-spacewar.md` and PoppyCoop notes first. | 2026-10-21 |
| W4 | Two-instance test harness (`gamepool` config for Ghostrunner) | unowned | unowned | `tools/gamepool/` config + own rig | Hardlink mirror, private save/config dirs, RAM guard. Two UE4 clients at ~2-3 GB each on 16 GB: one test-heavy lane at a time. | 2026-10-21 |
| W13 | Split-screen UX: hide/ignore the peer-slot viewport so players see one camera | unowned | blocked by W10 | own mod folder | W10 proves the body is drivable, but `CreatePlayer` splits the screen. Unknown whether the second viewport can be hidden without breaking input. | 2026-10-21 |
| W7 | Agent space (this folder) | lead (deepseek, 2026-10-21) | done | `games/ghostrunner/coordination/` | Protocol + workboard + log + "if you cannot push" section. | 2026-10-21 |
| W8 | EOS / Galaxy / Steam P2P transport | lead (deepseek, 2026-10-21) | dropped | — | Pirated copy: platform auth cannot be relied on; reusing baked credentials is a licensing/fairness problem. | 2026-10-21 |
| W9 | Shared write channel for other agents | lead (deepseek, 2026-10-21) | done | this branch | Documented three routes (draft PR into `karkqm:game/ghostrunner`, PR comment with a work-item id, or labelled handoff). Verified the 403 is an identity mismatch, not a repo permission problem. | 2026-10-21 |

## Open questions for the owner (human)

- Is a friend available for a real two-machine test, or do we prove everything with two
  local instances plus bots first?
- ~~May we reuse the game's baked EOS credentials?~~ Moot: W8 dropped.

## How to claim a row

1. Put your handle in `Owner` (format: `<agent or model>, <session date>`).
2. Set your `Write scope` — the paths you will actually edit.
3. Set `Status: in_progress` and bump `Updated`.
4. Add a `### <date> — <handle> — claim W<n>` entry to `LOG.md` in the same commit.
5. When done: `Status: done`, put the evidence pointer in `Next / evidence`.

## Note on the 403

The branch `game/ghostrunner` on `karkqm/coop-anything` is writable by the account that
owns the fork. An agent that clones it with a **different** credential gets 403 on push,
which is normal and does not need the owner to change any settings: either push as the
owner's account, or open a pull request into `karkqm:game/ghostrunner`.
Do not ask the owner to paste credentials anywhere, and never commit tokens.
