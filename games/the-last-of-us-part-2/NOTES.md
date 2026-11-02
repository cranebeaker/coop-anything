# TLOU2 Remastered co-op notes

Date: 2026-10-23
Status: research only. No retail copy was available, no mod was run.

## Search log

- Checked this repo's `DIRECTORY.md` and `games/`: only *The Last of Us Part I* was
  listed, as a gap. No TLOU2 entry and no TLOU2 write-up.
- GitHub issue and PR search in this repo for "last of us": no results.
- GitHub API repository search: "last of us co-op mod", "tlou coop", "tlou2 co-op",
  "ProjectSync last of us".
- Web search: Speclizer's mod, the Sony cancellation, ProjectSync, No Return co-op,
  Nexus Mods results.
- Read: PC Gamer 2026-07-29 and 2026-09-14; GamingBible 2026-04-10 and 2026-10-18;
  GitHub repos `Awperati/tlou2-coop` (README, roadmap, playing guide, release, commits)
  and `foufouadi/TLOU2_Co-op` (README, one commit); Steam appdetails for app 2531310.
- Not searched directly: Nexus Mods (only web results), ModDB, Discord, X (only through
  articles).

## Observations (public sources)

- Speclizer's mod reached 4v4 and was planned for September 2026, then was cancelled on
  2026-09-14 after a letter from Sony Interactive Entertainment. It never released.
- ProjectSync was revealed 2026-10-18. It targets No Return first, then the story, then
  PvP. No download and no public repository as of 2026-10-23. It is promised free and
  open source.
- Awperati's LAN mod has a v0.1.0 release dated 2026-10-17 and commits to 2026-10-19. Its
  README says the game link is not mapped and characters cannot be moved in game yet. Its
  roadmap targets `tlou-ii.exe` PE timestamp `67ED75F7` (2025-04-03).
- The Awperati roadmap records synthetic input through XInput, a `version.dll` loader, an
  authoritative host, a companion puppet, and that the game pauses when it loses focus.
- foufouadi's repo has one commit (2026-08-02), a visual clone, position sync over local
  UDP, and no NPC state sync.
- TLOU2 on PC has no stock multiplayer and no official mod API. The Last of Us Online was
  cancelled in 2023.

## Derived

- Couch co-op is a second layer: it needs working co-op first, then multi-instance
  launching, input isolation and focus handling.
- ProjectSync is described as online co-op; nothing in the sources says it can run two
  clients on localhost, which a couch setup would need.
- A mod that targets the 2025-04-03 launch build cannot be assumed to work on later
  retail patches, let alone on a repack.
- Reviving Speclizer's mod is not appropriate: the publisher asked for it not to be
  released.

## Guesses

- ProjectSync is likely a DLL hook with a host-authoritative session, by analogy with the
  other two projects. This is not confirmed.
- A Nucleus Co-op handler for TLOU2 would be feasible once a co-op mod exists, but the
  focus pause and the large install are likely pain points.

## Unknowns

- ProjectSync's repository, license, transport and release date.
- Whether ProjectSync supports LAN or localhost, or only internet play.
- Whether Awperati's mod will support later retail builds.
- Whether two TLOU2 instances can run on one PC at a playable frame rate.
- The retail save/profile layout and how much story state lives in saves.
- Whether Sony would object to a free, open-source co-op mod. The cancellation letter was
  aimed at Speclizer's project; the reason is not public.

## Test log

None. No retail copy was available, so nothing was launched. The directory entries and
the write-up are based only on the public sources listed above.
