# Research: find everything that already exists

The most common mistake is announcing "nobody has made co-op for this game" too early.
Two of our projects started from that claim and both games turned out to have working
co-op: one lived on a non-default `multiplayer` branch of a fork, the other was a 2019
build on its author's own build server. Say "not found by search X" and keep going.

## Where to look, in order

1. **This repo:** [DIRECTORY.md](../DIRECTORY.md), `games/`, open issues and PRs.
2. **Web search** with several phrasings: `<game> co-op mod`, `<game> multiplayer mod`,
   `<game> coop campaign`, `<game> online mod`, the engine name plus "coop", and the
   game's name in the main languages of its fan base (Russian for STALKER, German and
   Polish for Gothic, Japanese for many console games).
3. **GitHub:** code search and repository search, then for each hit look at **forks,
   branches and releases**, not just the default branch. Search topics (`coop`,
   `multiplayer`, `<game-name>`).
4. **ModDB and Nexus:** last update dates, comments about abandonment, files tabs, and
   "articles" with dev diaries. Both block some automated fetches; use search result
   text and say so in your notes.
5. **Archive.org:** dead project sites, download pages, wikis and forum threads. Old
   build servers and FTP indexes often survive there or even still online.
6. **Community hubs:** the game's main Discord and forums, Steam community guides and
   discussions, Reddit (r/<game>, r/patientgamers threads asking for co-op).
7. **The author's other work:** what else they released on the same engine explains a
   lot about the code and may contain shared tooling.
8. **Same engine, other games:** a co-op framework for a sibling game may be most of the
   work. See [engine-families.md](engine-families.md).

## What to record for each find

- Link, author handle, license (or "none stated").
- Last activity you can confirm (commit, release, post date), not the "updated" badge.
- What it **really** syncs. Separate "players can see each other" from shared enemies,
  shared quests and story, world state and items. Authors' pages, press articles and
  players often disagree; trust the author's own current page plus players' reports
  over press.
- Where the bug reports live (issue tracker, Discord channel, forum thread).
- The newest build and how you verified it (hash, version string inside the files).

- **Steam Workshop banners are not evidence.** Every Workshop page carries hidden `bannedNotification` and
  `incompatibleNotification` blocks (`display: none`), so a text fetch reads "removed from the community" on healthy
  items. Check the style attribute, or compare with an unrelated item, before reporting a takedown. From: Tale of
  Immortal, where two research agents reported a removal that a control item disproved.

## Mining bug reports

- Issue trackers and forum threads first.
- Discord: read [discord-export.md](discord-export.md), including the Terms of Service
  part, before exporting anything.
- Turn every reported crash string into a test: a boot test per level, a scripted
  repro.

## Reading foreign-language sources

Read them directly and summarize in English, but keep original technical strings (error
messages, file names, console commands) untranslated next to the translation, because
those are what you will search for later.

## Output

Add or correct entries in `data/directory.yaml` as you go, even if you never write a line
of code. A good research PR is one of the most useful contributions.
