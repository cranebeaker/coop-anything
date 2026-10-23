# Vanquish - working notes

Session 2026-10-22. Research only, no local install of the game in this
session, so no binary was examined and no identity (build id / SHA-256) was
recorded. Everything below is sourced, derived, or an explicitly labelled
guess.

## Observations (sourced)

- Wikipedia: Vanquish, PlatinumGames/Sega, 2010 (PS3, Xbox 360), Windows port
  2017-05-25. Havok physics. Mikami-directed.
- Eurogamer (2010): PlatinumGames confirmed no multiplayer modes, none ever
  planned; they preferred to focus on single-player.
- Steam forums (app 460810): multiple "co-op please" threads; community
  consensus is that no mod exists or is likely; comparisons to JC2-MP are
  user speculation, not projects.
- Nexus Mods (vanquish game section): a Cheat Engine table / trainer
  (limiter release, AR switcher, melee adjustment) and skin/texture packs.
  Nothing networking-related.
- PCGamingWiki: Vanquish is singleplayer-only; the Platinum Engine page notes
  Vanquish is the one game in the family without the 60 fps GPU-render lock.
  (Page itself behind Cloudflare; snippets from search results only.)

## Derived

- Gap entry: nothing to revive, nothing to port. Build from zero, worst row
  of the triage table (closed binary, no MP, no scripting).
- Same-engine leverage is limited: no Platinum-engine game has a co-op mod;
  NieR: Automata (this repo covers it) also has none.

## Guesses (unverified)

- The Bayonetta PC modding scene's archive tools may partially apply to
  Vanquish's data files. Needs a local install to check.
- AR-mode slow motion will be the hardest design problem, ahead of even
  actor sync.

## Unknown

- Scripting layer, if any. Data archive format. Build id of the current
  Steam exe. Any Japanese-community project. Anything on archive.org.

## Search log (2026-10-22)

- Repo: DIRECTORY.md, games/, open issues and PRs: nothing for Vanquish.
- GitHub search (repos and code): nothing (hits are unrelated "vanquish"
  name matches).
- Web search: English queries for co-op/multiplayer/online mod; Nexus,
  ModDB, Steam forums; no project found.
- Not checked: archive.org, Reddit in depth, Japanese communities, Discord.
