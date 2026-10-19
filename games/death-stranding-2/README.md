# DEATH STRANDING 2: ON THE BEACH (2025)

| | |
|---|---|
| Engine | Decima |
| Versions tested | Steam PC `DS2.exe` 1.10.89.0 inspected statically; game not run |
| Projects | None found in bounded search; see [directory gap](../../DIRECTORY.md) |
| QA level | none |
| Last updated | 2026-10-19 |

## Where things stand

No live co-op project surfaced in the bounded search recorded in [NOTES.md](NOTES.md). The built-in Social Strand System lets friends' separate playthroughs influence one another, but does not put them in the same live game session. The executable contains generic multiplayer and matchmaking APIs, though their use for player co-op has not been established.

## Existing work

- The official Social Strand System supports asynchronous friend connections and Strand Contracts, not simultaneous co-op. See the official [PC FAQ](https://store.steampowered.com/news/posts/?enddate=1772732685&feed=steam_community_announcements) and [Bridge Links guide](https://kojimaproductions.jp/index.php/en/deathstranding2-SupportRookiePorters).
- No DS2 live co-op project surfaced in the public search summarized in [NOTES.md](NOTES.md). Discord was not searched, so the search is not exhaustive.
- [Odradek](https://github.com/ShadelessFox/odradek) is an asset viewer/extractor, not a co-op project. The [DS2 Mod Suite](https://www.nexusmods.com/deathstranding2onthebeach/mods/72) and other ASI-loader mods provide evidence of a possible plugin loading path, not multiplayer support.

## How the game works (for co-op)

The series uses Decima, per the PlayStation [developer interview](https://blog.playstation.com/?p=407261). On Steam PC 1.10.89.0, targeted static analysis found API registration code named `MultiplayerGroup`, `MultiplayerGameLibGroup`, and `MatchmakingGroup`, as well as replication-related engine identifiers. These establish that multiplayer and replication machinery exists in the executable, but not that DS2 has an enabled live-player mode or that its gameplay entities replicate. The current networking and save model remains unknown. Static-analysis scope and limitations are recorded in [NOTES.md](NOTES.md).

## What we changed

Research only. No game code, executable, or save was changed, and no co-op feature was built.

## QA

None. The game was not run, and no scripted game clients or shared-state tests were performed.

## How to play

For the supported friend connection, use a Steam build with Patch 1.1.53 or later. At a Ring Terminal, open **Social Strand Service → Bridge Links** and sign a Strand Contract with a friend. This increases the chance of seeing that friend's structures in your separate playthroughs; it does not provide simultaneous co-op.

## Known issues

- No live co-op mod or shared campaign is documented here.
- Static evidence does not establish the executable's matchmaking or replication behavior.
- Findings cover only Steam PC 1.10.89.0 and do not establish behavior on later builds or PS5.

## Next steps

- Search relevant community Discords for private or unindexed projects, following the repository's Discord research guidance.
- Trace the matchmaking callback chain and identify which online service handles a normal session.
- Determine whether the replication metadata is used by live gameplay and whether a safe plugin hook can access player/world state.
- Map level transitions, cutscenes, mission progression, and save behavior before proposing a co-op architecture.
