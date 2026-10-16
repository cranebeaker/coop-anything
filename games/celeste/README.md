# Celeste (2018)

| | |
|---|---|
| Engine | XNA/FNA (MonoGame lineage, C#) |
| Versions tested | Steam PC, current build, with Everest `stable-1.6623.0` |
| Projects | CelesteNet, Co-op Helper, Partnership, Intertwined (see the directory) |
| QA level | community-reports |
| Last updated | 2026-10-16 |

## Where things stand

Celeste has no built-in multiplayer. A working mod stack does exist, and it is the same
stack for everyone: install **Everest** (the mod loader), then **CelesteNet** (puts other
players in your level, with chat and emotes), then optionally **Co-op Helper** (entities
that synchronize between players). For a real shared goal, two players install a co-op
map built on those two. The intended two-player co-op campaign is **Partnership**, a
mini campaign designed for exactly two players.

What this does *not* give you: CelesteNet does not synchronize the vanilla story,
enemies or world state. Two people can be in the same level and see each other, but their
progress, deaths and items are their own. Anything that feels like "playing the campaign
together" comes from a co-op map, not from the base game. There is no drop-in co-op for
the vanilla story.

## Existing work

| Project | What it is | Author | License | Last activity |
|---|---|---|---|---|
| [CelesteNet](https://gamebanana.com/mods/53695) | The online multiplayer mod (Everest). Players, chat, emotes, per-map sessions. Client v2.4.2. [Source](https://github.com/0x0ade/CelesteNet) (MIT), [site](https://celestenet.0x0a.de/). | 0x0ade | MIT | client 2024-12, repo 2026-06 |
| [Co-op Helper](https://gamebanana.com/mods/453706) | Helper library for CelesteNet: entities/triggers whose state syncs between players, plus a beta "Co-op Everywhere" mode. v1.1.4. [Source](https://github.com/corkr900/CoopHelper). | corkr900 | CC BY-NC-ND 4.0 | 2026-09 |
| [Partnership](https://gamebanana.com/mods/150725) | Two-player co-op mini campaign built on CelesteNet + Co-op Helper. v1.0.1. The closest thing to a playable co-op campaign. | MunTheory | CC BY-NC-ND 4.0 | 2022-01 |
| [Intertwined](https://gamebanana.com/mods/713075) | Two-player co-op Summit map, death-link style, Expert to Grandmaster. v1.1.3. Pulls 14 helper dependencies. | Clantis | CC BY-NC-ND 4.0 | 2026-10 |

Older, superseded: **GhostNet** (the pre-CelesteNet multiplayer mod) is deprecated;
CelesteNet replaced it.

## How the game works (for co-op)

- **Engine:** FNA/XNA, C#. Game code lives in `Celeste.dll`; assets in `Content/`.
- **Mod loader:** **Everest** patches the game on install and loads mods from `Mods/`.
  Mods declare dependencies in an `everest.yaml`. Everest resolves the load order from
  the dependency graph, so folder names do not matter.
- **Multiplayer model:** CelesteNet is **client/server**, not peer-to-peer. Each player
  connects to a CelesteNet server (the public one, or a self-hosted one). Players appear
  in the same map as ghosts/avatars; a "channel" groups players together. There is no
  native netcode in Celeste, so nothing is server-authoritative about the vanilla game.
- **Co-op entities:** Co-op Helper adds entities that, when a map is built with them,
  synchronize their state over CelesteNet (doors, switches, spawns). This is the piece
  that makes an objective shared rather than just the space.
- **Saves:** Vanilla progress is per-profile in `Saves/`. Co-op maps keep their own
  progress; the two players' vanilla saves are independent.
- **Reading it:** mods are C# against Everest's API and most are open source; the game
  itself is closed and only shipped binaries exist.

## What we changed

This is a documentation PR. No game or mod code was changed.

- **Corrected the Celeste directory entry.** It pointed at a third-party file mirror
  (`vgtimes.com`) as the project URL, listed `open_source: unknown` for a mod that is MIT
  and public, and carried a stale `last_activity: 2024-07` / v2.3.1. The entry now points
  at the CelesteNet source repo and the live GameBanana page, with current facts.
- **Added directory entries** for Co-op Helper, Partnership and Intertwined, the co-op
  projects a two-player session actually needs.
- **Corrected a claim in our own notes.** An earlier note said CelesteNet's README link to
  `gamebanana.com/gamefiles/12267` is broken and now points at a Team Fortress 2 mod. It
  is not: that URL redirects to the correct CelesteNet page (`mods/53695`). The TF2 mod
  came from the separate GameBanana `Mod` ID space (`apiv11/Mod/12267`). See "Known
  issues" for the lesson.

## QA

**QA level: community-reports.** There is no automated harness for Celeste in this repo
and none was built here. What was actually verified this round:

- **Dependency satisfaction (static).** The five mods staged (CelesteNet.Client 2.4.2,
  CoopHelper 1.1.4, Partnership 1.0.1, MaxHelpingHand 1.40.10, LunaticHelper 1.1.1) all
  declare Everest and each other within the installed Everest `1.6623.0` and the versions
  above; every hard dependency resolves. This is a read of each mod's `everest.yaml`, not
  a runtime test.
- **Download integrity.** Each artifact was hash-checked (SHA-256, and MD5 where the
  source published one). LunaticHelper MD5 `f14f38265dc4c23699ac38090f8c691b` matches the
  GameBanana listing; the others were checked against the fetched bytes.
- **Launch + mod load (verified).** The isolated install launched and Everest's own log
  (`log.txt`) shows all five mods load and register, ending with `DONE LOADING`, with no
  fatal errors: CelesteNet.Client 2.4.2, CoopHelper 1.1.4, LunaticHelper 1.1.1,
  MaxHelpingHand 1.40.10, Partnership 1.0.1. Only warnings: first-run settings, three of
  MaxHelpingHand's *optional* dependencies absent (BounceHelper, FrostTempleHelper,
  FlaglinesAndSuch), and a sprite override conflict.
- **Gotchas found while testing:** Everest writes `log.txt` in the game folder, not
  `everest-log.txt`. A copied install needs a `steam_appid.txt` containing `504230` next
  to `Celeste.exe`, or Windows/Steam hijacks the launch and starts the Steam install
  instead of the copy.

Not run: two real clients connected to a server, any assert on shared state. A Celeste
build/test that drives two clients would be a large effort: the game is not headless and
CelesteNet needs a server plus authenticated clients.

## How to play

Step-by-step owner guide: [SETUP.md](SETUP.md). Short version:

1. Install Everest (through Olympus, or the manual MiniInstaller).
2. Install the mods: CelesteNet, Co-op Helper, MaxHelpingHand, LunaticHelper, Partnership.
3. Launch, set a CelesteNet name, connect to a server.
4. Make a private channel and invite your partner.
5. Start the Partnership chapter from the chapter select.

## Known issues

- **No vanilla story co-op.** CelesteNet shows players in the same map; it does not sync
  story, enemies or world state. Only co-op maps built on Co-op Helper share goals.
- **Partnership is short and unfinished.** It is a mini campaign ("not many chapters yet"),
  by the author's own description, and last released in 2022.
- **Dependency chain is easy to get wrong.** Partnership needs MaxHelpingHand and
  LunaticHelper. Note that **MaxHelpingHand's GameBanana page is titled "Maddie's Helping
  Hand"** (it was renamed, the everest.yaml name was kept); searching for "MaxHelpingHand"
  on GameBanana does not obviously find it. Intertwined needs 14 helpers.
- **Old `gamefiles/<id>` links and GameBanana's ID spaces.** GameBanana keeps `Mod` and
  `GameFile` IDs in separate namespaces. `gamebanana.com/gamefiles/12267` redirects to the
  right CelesteNet page, but the API path `apiv11/Mod/12267` is an unrelated Team Fortress
  2 mod. Check which space a number is in before calling a link broken.

## Next steps

- **Two-client connect test:** the single-client load test passed; the next honest step is
  two clients on one server, asserting they see each other and that a Co-op Helper switch
  syncs.
- **Automated QA:** Celeste is not headless. The plausible route is two windowed clients
  plus a self-hosted CelesteNet server, driven by Co-op Helper's synced entities as the
  oracle (put a switch in a test map, assert both clients see it change). This is a project
  of its own.
- **A co-op menu entry:** CelesteNet only adds a **Mod Options** section (Everest's settings
  menu) and an in-game overlay; there is no top-level "Co-op" item on the main menu. A small
  Everest mod could add one (a text menu with connect / channel / quick-launch Partnership).
  Not built here.
- **Intertwined:** its 14-dependency tree was not installed. Verify it before recommending
  it over Partnership.
