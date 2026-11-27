# Saints Row: The Third Remastered (2020)

| | |
|---|---|
| Engine | Proprietary "Engine Core Technology Group" engine + Havok (reported for the Epic build by SR3R; not yet verified on Steam) |
| Versions tested | None yet. Target: Steam build 7026541; `SRTTR.exe` SHA-256 `F23BE8B32759EC708A896F2F9C8EA35B99213ED3EF1DE9F75B99BEBFEEE7996A` |
| Projects | Claim: https://github.com/cranebeaker/coop-anything/issues/440 ; no 3+ player campaign co-op project found |
| QA level | none |
| Last updated | 2026-11-27 |

## Where things stand

The Remastered game ships 2-player campaign co-op. No 3+ player campaign co-op mod was
found in this research pass. The obvious sandbox multiplayer mod, SR3MP, is closed
source, abandoned, and explicitly not a campaign co-op project. The next step is to
inspect the native co-op netcode on an isolated copy of the Steam build.

## Existing work

- **SR3MP** (https://github.com/SR3-MP, forum: https://www.saintsrowmods.com/forum/threads/sr3mp-multiplayer-mod.19447/): standalone custom multiplayer client for the Remastered game (Steam/Epic/GOG), dedicated servers, ENet, Lua client/server scripting, author-claimed up to 127 players. The org README says the project is officially abandoned and closed source. FAQ explicitly says it does not use the game's original netcode and that making co-op missions with more players is not its goal. Latest public docs commit 2024-08-11; alpha changelog stops at 0.1.4. QA level for campaign co-op: none (it does not do campaign co-op).
- **RootKiller/sr3mp-Abandoned** (https://github.com/RootKiller/sr3mp-Abandoned): older prototype, last commit 2018-12-29, no license stated. Credited by SR3MP for reversed NPC/vehicle spawn functions. Code cannot be copied without permission.
- **SR3R** (https://github.com/MrReekoFTWxD/SR3R): x64 Detours reverse-engineering project for the Epic build, last push 2021-12-30, no license stated. Names the engine and Havok and provides reversed entity/UI leads; verify addresses against the Steam build.
- **SR3-MP/SR3MP-IntelliSense** (https://github.com/SR3-MP/SR3MP-IntelliSense): MIT-licensed scripting API definitions, last push 2024-06-10.
- Official 2-player co-op is the base to extend; no project was found that extends it to 3+.

## How the game works (for co-op)

Unverified on the Steam build. SR3MP's public docs say calls that touch game state must
run on the main thread, and its 0.1.4 changelog describes a 50 Hz network thread that
synchronises packets before sending. Those are architecture findings from SR3MP, not
from the native campaign netcode. Native co-op's session/player-slot limits, replication,
mission triggers and saves are still unknown.

## What we changed

Nothing in the game yet. Research, claim issue and directory entries only.

## QA

No tests run. QA level: `none`.

## How to play

Not yet possible with 3+ players. Native 2-player co-op remains the only known option.

## Known issues

SR3MP's old site `sr3mp.net` did not resolve from the research environment on
2026-11-27; use the GitHub mirrors listed above. No 3+ player build exists to test.

## Next steps

1. Get the exact Steam build identity and an isolated copy of the install (never modify the owner's install or saves).
2. Map the native 2-player co-op session: player slots, replication, mission/story ownership, level transitions and saves.
3. Decide whether to extend native netcode to 3+ clients or build a separate sync layer, then test with bots in multiple isolated game copies.
4. Ask the SR3MP authors about licensing only if a specific finding turns out to be necessary (owner permission needed to contact anyone).
