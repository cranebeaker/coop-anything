# The Last of Us Part II Remastered (2025)

| | |
|---|---|
| Engine | Naughty Dog proprietary, native x64, DirectX 12. Closed binary, no official mod API. |
| Versions tested | None. No retail copy was available to this agent, so no mod was run. Retail identity: Steam app 2531310, released 2025-04-03. Awperati's co-op mod targets the launch build, `tlou-ii.exe` PE timestamp `67ED75F7` (2025-04-03). |
| Projects | ProjectSync (no public repo yet); TLOU2 Co-op by Awperati; TLOU2 Co-op Hook by foufouadi; TLOU2 Multiplayer Mod by Speclizer (dead). Directory entries use the same names. |
| QA level | none |
| Last updated | 2026-10-23 |

## Where things stand

No co-op mod for TLOU2 on PC has been released. The closest project, Speclizer's
multiplayer mod, reached 4v4 PvP and an experimental co-op mode, then was cancelled in
September 2026 after Sony asked for it not to be released. Two early projects are active:
ProjectSync (online No Return co-op, no download yet) and Awperati's two-PC story co-op
(network layer works, game link not mapped). Couch co-op is not possible yet: it is a
split-screen layer on top of working co-op, and the game has no netcode to build on.

## Existing work

Searched on 2026-10-23: this repo's directory and `games/`, GitHub repository and code
search via the GitHub API, web search (including Reddit results), and the linked project
pages. Nexus Mods and ModDB were not searched directly. No download was run.

- **ProjectSync** (Reddit user `leiden_core`), revealed 2026-10-18. Online two-player
  co-op for No Return first, then the story campaign, then PvP. The demo shows Dina and
  Ellie in the safe house; movement, animations, weapons, backpack, flashlights, melee,
  enemy AI and damage are reported synced. No download, no public repo, free and open
  source promised. This is the most promising route to actual co-op.
- **TLOU2 Co-op (Awperati)**, created 2026-10-09, v0.1.0 released 2026-10-17, commits
  through 2026-10-19. Two PCs, host-authoritative, player 2 takes the chapter companion
  (Dina, Lev, Jesse, and so on). The README says the network layer, teleport, roster,
  overlay and loader are tested, but the game link is not mapped, so characters cannot be
  moved in game. Target build is the 2025-04-03 launch build. LAN with an optional
  internet relay. Public source, no license file.
- **TLOU2 Co-op Hook (foufouadi)**, one commit 2026-08-02. DLL injection, position sync
  over local UDP, and a visual clone of Ellie on F10. No second controllable player and
  no NPC state sync. Not functional; useful for its technical notes.
- **TLOU2 Multiplayer Mod (Speclizer)**, developed January to September 2026. PvP (4v4)
  plus an experimental co-op mode shown in April 2026. It had five maps, loadouts and
  leaderboards by July. Cancelled 2026-09-14 after a letter from Sony Interactive
  Entertainment; never released. Do not revive.

## How the game works (for co-op)

- Native x64 DirectX 12 build of Naughty Dog's in-house engine, shipped as one large
  executable plus middleware DLLs. Awperati's roadmap records no anti-cheat and no
  official mod API.
- The single-player game ships no multiplayer mode and no usable netcode. The Last of Us
  Online was cancelled in 2023, so there is no stock replication or session layer to
  extend.
- Awperati's roadmap reports that the game pauses itself when it loses focus. That matters
  for any future split-screen or multi-instance setup.
- Unknown: the save/profile layout, how levels and story state are represented, whether
  two instances on one PC can reach a playable frame rate, and how ProjectSync transports
  its traffic.

## What we changed

Nothing. This write-up records the game's identity and the state of the existing
projects. No game files were copied and no mod was run.

## QA

No test was run. No retail copy was available to this agent, and none of the projects has
a public playable build. QA level is `none`.

## How to play

Not available yet.

- ProjectSync: wait for a public build.
- Awperati's mod: requires two PCs, a retail copy of the 2025-04-03 launch build, and the
  mod's own build; it cannot move characters in game yet.
- Speclizer's mod: never released, and Sony asked for it not to be; do not look for
  reuploads.

Couch co-op on one PC will need a launcher layer such as Nucleus Co-op after co-op works,
because the game has no split-screen and no netcode.

## Known issues

- No released co-op, so nothing to play today.
- The closest complete project was cancelled at the publisher's request.
- The active mods are early and target one specific game build; game updates can break
  offsets.
- Two instances on one PC would need separate saves/profiles, input isolation and a focus
  fix; the game pauses when it loses focus.
- The Awperati mod's public source has no license file, so treat it as source-available,
  not open source.

## Next steps

- Track ProjectSync; when a build appears, record its game build, transport and whether it
  can run two clients on localhost.
- Check Awperati's milestones and ask whether later game builds will be supported.
- Once co-op works, build a Nucleus Co-op handler for couch play: per-instance saves,
  controller isolation, borderless windows and fake focus.
- Map the save/profile layout and the story-state representation for multi-instance
  testing.
- Do not revive Speclizer's mod; the publisher asked for it not to be released.
- Open a "claim a game" issue if someone starts a new TLOU2 co-op build.

## Sources

- https://www.pcgamer.com/games/action/that-last-of-us-2-multiplayer-mod-everyone-was-looking-forward-to-has-been-killed-by-sony/
- https://www.pcgamer.com/games/action/the-last-of-us-multiplayer-mod-has-hit-a-big-milestone-as-it-prepares-for-launch-in-september
- https://www.gamingbible.com/news/the-last-of-us-projectsync-co-op-mod-128815-20261018
- https://www.reddit.com/r/thelastofus/comments/1x8w74m/coop_for_no_return_in_the_last_of_us_part_ii/
- https://www.gamingbible.com/news/platform/pc/the-last-of-us-2-co-op-mod-finally-914636-20260410
- https://github.com/Awperati/tlou2-coop
- https://github.com/foufouadi/TLOU2_Co-op
- https://store.steampowered.com/app/2531310/
