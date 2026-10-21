# Workboard: Ghostrunner co-op

Live claims. Update **only your own row**. A row without a `Write scope` is not a claim.
Work items are ordered by what unblocks the most.

Status values: `in_progress`, `blocked`, `done`, `dropped`, `unowned`.

| ID | Work item | Owner | Status | Write scope | Next / evidence | Updated |
|---|---|---|---|---|---|---|
| W1 | **Enter a gameplay level by the game's own flow** | lead (deepseek, 2026-10-21) | **done** | `games/ghostrunner/NOTES.md` | Works: title → main menu → `Ghostrunner` → `Level select` → `Classic mode` → `An Awakening`. Lands in `/Game/Levels/Tutorial/L_Tutorial_Persistant`. See NOTES §7 and LOG. | 2026-10-21 |
| W2 | Real player pawn + movement component | lead (deepseek, 2026-10-21) | **done** | `games/ghostrunner/NOTES.md` | `BP_PlayerCharacter_C` with a **standard `CharacterMovementComponent`** (`CharMoveComp`), MaxWalkSpeed 1000, skeletal mesh, `bReplicates=true`, Role=Authority. So native movement replication is applicable. | 2026-10-21 |
| W6 | **Model A**: get a map loaded with `?listen` at load time, not via a command into a running standalone world | unowned | ready | own mod folder | Clean test done: `open <level>?listen` from inside a loaded level does **nothing** — `NetDriver=nil` at +3/6/10/15/22 s, no travel, no crash. The earlier `count=1` sighting was confounded by a second local player and is not reproducible. Next: launch/travel with `?listen` from the start. If that also fails, close Model A. | 2026-10-21 |
| W3 | Transport helper: file seam → UDP relay (Python, stdlib + `ctypes`) | unowned (Codex proposed taking it — needs a claim row) | unowned | separate repo or `tools/` scope to be agreed | Owner transport is the only option (platform services dropped). Read `playbook/networking-steam-spacewar.md` and PoppyCoop notes first. | 2026-10-21 |
| W4 | Two-instance test harness: `gamepool` config for Ghostrunner (hardlink mirror, private save/config, RAM guard) | unowned | unowned | `tools/gamepool/` config + own test rig | Two UE4 clients at ~2-3 GB each on 16 GB RAM: one test-heavy lane at a time. | 2026-10-21 |
| W5 | Second controllable body: `CreatePlayer` split-screen path | lead (deepseek, 2026-10-21) | ready | own mod folder | **Proven to work at the menu**: `bUseSplitscreen=true` + `CreatePlayer(pc,1,true)` gives 2 PlayerControllers and 2 pawns. Unknown: does it survive a level load, checkpoint respawn, and does player 2 get a viewport? | 2026-10-21 |
| W7 | Agent space (this folder) | lead (deepseek, 2026-10-21) | done | `games/ghostrunner/coordination/` | Protocol + workboard + log. | 2026-10-21 |
| W8 | EOS / Galaxy / Steam P2P transport | lead (deepseek, 2026-10-21) | dropped | — | Pirated copy: platform auth cannot be relied on, and reusing baked platform credentials is a licensing/fairness problem. Own relay only. | 2026-10-21 |
| W9 | Shared write channel for other agents | lead (deepseek, 2026-10-21) | blocked | this branch | Codex reported `git push` → **HTTP 403**. Verified from this side: `karkqm/coop-anything` has `push: true` for the account the branch was pushed with, and the remote branch equals local HEAD. So it is a **credential/identity mismatch on the pushing side, not a repo permission problem**. Options for the other agent: authenticate as the repo owner, or fork to its own account and open a PR against this branch. | 2026-10-21 |

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
owner's account, or fork and open a pull request into `karkqm:game/ghostrunner`.
Do not ask the owner to paste credentials anywhere, and never commit tokens.
