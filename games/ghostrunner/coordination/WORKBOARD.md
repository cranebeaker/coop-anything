# Workboard: Ghostrunner co-op

Live claims. Update **only your own row**. A row without a `Write scope` is not a claim.
Work items are ordered by what unblocks the most.

Status values: `in_progress`, `blocked`, `done`, `dropped`, `unowned`.

| ID | Work item | Owner | Status | Write scope | Next / evidence | Updated |
|---|---|---|---|---|---|---|
| W1 | **Enter a gameplay level by the game's own flow** (the blocker) | lead (deepseek, session 2026-10-21) | in_progress | `games/ghostrunner/NOTES.md`, probe mod under the test copy | Get past the menu into `/Game/Levels/01_INDUSTRIAL/01_01/01_01_World` without `open`; record the world name and the pawn class. `open` crashes — see LOG. | 2026-10-21 |
| W2 | Identify the real player pawn and its movement component (CharacterMovement vs custom parkour) | unowned | blocked by W1 | `games/ghostrunner/NOTES.md` (via LOG) | Needs a live gameplay pawn. Answer decides whether native replication is even plausible. | 2026-10-21 |
| W3 | Transport helper: file seam → UDP relay (Python, stdlib + `ctypes`) | unowned | unowned | separate repo or `tools/` scope to be agreed | Read `playbook/networking-steam-spacewar.md` and the PoppyCoop notes first. Must not use Steam/EOS (see README decisions). | 2026-10-21 |
| W4 | Two-instance test harness: `gamepool` config for Ghostrunner (hardlink mirror, private save/config dirs, RAM guard) | unowned | unowned | `tools/gamepool/` config + its own test rig | Two UE4 clients at ~2-3 GB each on 16 GB: run test-heavy work one at a time. | 2026-10-21 |
| W5 | Puppet hello-world: fake peer over a file, spawn a second body, drive it to peer coordinates | unowned | blocked by W1, W2 | own mod folder | This is the first milestone worth calling progress. Ghost Mod already proves a second driven body can exist in this game. | 2026-10-21 |
| W6 | Native replication probe (Model A) from inside a gameplay level | unowned | blocked by W1 | own mod folder | `open <map>?listen` from a real level; then check `FindAllOf("NetDriver")` and `GameEngine.NetDriver`. If no driver, close Model A and say so. | 2026-10-21 |
| W7 | Agent space (this folder) | lead (deepseek, session 2026-10-21) | done | `games/ghostrunner/coordination/` | Protocol + workboard + log. | 2026-10-21 |
| W8 | EOS / Galaxy / Steam P2P transport | lead (deepseek, session 2026-10-21) | dropped | — | Dropped: the copy under test is a pirated distribution, so platform auth cannot be relied on and reusing baked platform credentials is a licensing/fairness problem. Own relay only. | 2026-10-21 |

## Open questions for the owner (human)

- Is a friend available for a real two-machine test, or do we prove everything with two
  local instances plus bots first?
- ~~May we reuse the game's baked EOS credentials?~~ Moot after W8 was dropped.

## How to claim a row

1. Put your handle in `Owner` (format: `<agent or model>, <session date>`).
2. Set your `Write scope` — the paths you will actually edit.
3. Set `Status: in_progress` and bump `Updated`.
4. Add a `### <date> — <handle> — claim W<n>` entry to `LOG.md` in the same commit.
5. When done: `Status: done`, put the evidence pointer in `Next / evidence`.
