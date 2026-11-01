# Dynasty Warriors 3: Complete Edition Remastered (2026)

| | |
|---|---|
| Engine | Unreal Engine 5 |
| Versions tested | none (triage and research only; the game was not installed) |
| Projects | Official split-screen co-op (shipped); official online 2-player co-op (announced, free update due by January 2027) |
| QA level | none (no co-op test has run; this write-up is research only) |
| Last updated | 2026-11-01 |

## Where things stand

Two players can already play every stage together in local split screen, which the game
ships with (Steam app 3841510, released 2026-09-30). Online play does not exist yet, but
Koei Tecmo announced at Tokyo Game Show 2026 a free post-launch update adding online
two-player co-op, promised before January 2027. Until it ships, the only way to play
with a remote friend is streaming (Steam Remote Play Together or Parsec).

**Recommendation for agents: do not build a co-op mod for this game.** The publisher's
own online mode is weeks away and free. Spend the time on the directory entry and on
checking the official update when it lands.

## Existing work

- **Official local co-op and versus** (Koei Tecmo / Omega Force): 2-player split screen
  for co-op and 1-on-1, carried over from the PS2 original (2001), which was the first
  Dynasty Warriors with a two-player mode. Steam feature tags: Shared/Split Screen Co-op,
  Shared/Split Screen PvP. Steam does **not** list Remote Play Together.
- **Official online co-op (announced, not released)**: free update for two players,
  announced 2026-09-20 with the demo date, confirmed on the TGS 2026 stage and in a
  Famitsu interview. Target window: by January 2027.
- **Community mods for online play**: none found (GitHub, Nexus, ModDB, Steam
  discussions via web search, 2026-11-01). Steam threads ask for online co-op and point
  to Remote Play as the workaround.
- **Nucleus Co-op / PartyDeck handlers**: none found as of 2026-11-01 (search only, not
  checked inside the Nucleus handler repo).

## How the game works (for co-op)

- Unreal Engine 5, closed binary, no shipped script source and no official mod tools.
  Standard UE5 modding tools (UE4SS, pak mods) are the only known entry point; nobody
  has published one for this game yet. Not verified against the install.
- Two-player logic already exists in the engine (split-screen co-op), so a networked
  mod would be a matter of replicating two local players' inputs and state rather than
  inventing a second player. Still the "closed binary, no MP netcode, no scripting"
  row of the triage table: very hard.
- No anti-cheat was found in reports or store metadata (single-player and local game).
  Unknown whether the online update will add one.
- Save system, level loading and client/server split: unknown, not inspected.

## What we changed

Nothing. No game files were touched.

## QA

No test has run. The game was not installed for this write-up.

## How to play

- **Same PC**: use the built-in split screen (two controllers).
- **Remote friend, today**: Steam Remote Play Together is not advertised by the game;
  people report forcing it by adding the exe as a non-Steam game, or use Parsec. Quality
  depends on the host's upload bandwidth. Not tested here.
- **Remote friend, from the official update**: wait for the free online co-op patch.

## Known issues

- Online co-op is announced but not out; the release date is "by January 2027" and may
  slip.
- Remote Play Together works only with the workaround and only as video streaming.

## Next steps

1. When the official online update ships: install it, record exe version and SHA-256,
   test host/join with two real copies, update the directory entry (`networking`,
   `players`, `qa`) and this write-up.
2. If the update slips past January 2027 or is cancelled, reopen triage: check
   UE4SS compatibility with the game's UE5 build and whether the split-screen player
   2 can be driven from a network input source.

## Sources

- https://store.steampowered.com/app/3841510/DYNASTY_WARRIORS_3_Complete_Edition_Remastered/
- https://www.co-optimus.com/game/18377/pc/dynasty-warriors-3-complete-edition-remastered.html
- https://noisypixel.net/dynasty-warriors-3-remastered-demo-online-co-op/
- https://www.thesixthaxis.com/2026/09/24/dynasty-warriors-3-remaster-demo-out-now-online-co-op-announced/
- https://steamcommunity.com/app/3841510/discussions/0/600790905291736668/
- https://steamcommunity.com/app/3841510/discussions/0/562534024812202612/
- https://en.wikipedia.org/wiki/Dynasty_Warriors_3
