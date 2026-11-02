# Cyberpunk 2077 (2020)

| | |
|---|---|
| Engine | REDengine 4 |
| Versions tested | none (research-only write-up; no game copy was installed or run) |
| Projects | 10 — see Existing work |
| QA level | none |
| Last updated | 2026-10-23 |

## Where things stand

Nobody has shipped full story co-op, but 2026-10 is the closest it has ever been, and the ecosystem is
unusually crowded for one game. NightCoop opened a public beta on 2026-10-23 (prologue + Act 1, up to
8 players in the host's world, dialogue by majority vote, shared combat, optional host-toggled PvP).
The oldest framework, CyberpunkMP (Tilted Phoques), stalled in 2024-12 at the patch-2.2 era, but its
codebase lives on: oxylol/cp2077-coop adds a story-host layer on patch 2.31, NightCityMP carries the
sandbox side to 2.31a, and KyleBuildsAI/cp2077-coop — now integrating Bukczyk's typed session
foundation — ships a downloadable two-player prototype. Separately, OPEN//77 is a dedicated-server
multiplayer platform (roleplay/freeroam, Lua-scriptable) in alpha. Everything below is desk research:
no copy of the game was owned or run while writing this.

## Existing work

Ten projects found. Four were already in the directory (refreshed here), six are new entries in this
PR. Last activity verified 2026-10-23 via GitHub API (`pushed_at`) or the live project site.

| Project | Kind | License | Last activity | What it syncs |
|---|---|---|---|---|
| [NightCoop](https://nightcoop.net/) | mod, closed source | n/a (free fan project) | open beta 2026-10-23 | story (prologue + Act 1) in the host's world, up to 8 players each as their own V, dialogue by majority vote, shared combat/vehicles/cutscenes, optional PvP with level-normalized damage |
| [CyberpunkMP (Tilted)](https://github.com/tiltedphoques/CyberpunkMP) | framework | custom (source visible) | 2024-12, stale | appearance, equipment, movement, basic animations, vehicles + passengers; .NET server SDK, redscript client SDK, automatic RPC |
| [NightCityMP](https://github.com/blyatiful1/NightCityMP) | fork of CyberpunkMP | custom, same terms as upstream | 2026-07 | port to 2.31a: appearance/clothes/drawn weapon, movement + animations, nameplates + chat, server clock, emotes, demo jobs; NPCs, traffic, world state, quests, combat damage not synced |
| [oxylol/cp2077-coop](https://github.com/oxylol/cp2077-coop) | fork of CyberpunkMP | custom, same terms as upstream | 2026-10 | story-host layer: host's game time, weather and tracked quest pushed every 2 s, guest clocks only move forward, `/tp`, look transfer with a 2.31 fallback head; includes upstream PRs #56–#60 (patch-2.31 RTTI name fix) |
| [KyleBuildsAI/cp2077-coop](https://github.com/KyleBuildsAI/cp2077-coop) | mod | none stated | 2026-10 | downloadable v0.0.37/alpha.5: partner shown as a stand-in NPC, Python relay (`relay_v2.py`), CET host/join panel; source now integrates Bukczyk's typed session foundation for player groups |
| [Bukczyk/CP2077-Coop](https://github.com/Bukczyk/CP2077-Coop) | mod | none stated | 2026-10 | typed session foundation (`SessionServer`, `CoopPlugin`, `runtime/session`); one-PC two-client diagnostic reached JOINER-fires-at-HOST validation with health loss and death; reconnect death recovery still fails; not a public release |
| [RedSync](https://github.com/nobody71004/RedSync) | mod | AGPL-3.0 | 2026-08 | RED4ext client + redscript + .NET 9 server over GameNetworkingSockets; remote players as Codeware/AMM puppets; host-authoritative quest-fact relay (Phantom Liberty q301–q307 + base-game prefixes) with late-join snapshots; CERBERUS 8–50-bot city events; README download links still point at a placeholder domain |
| [OPEN//77](https://open2077.net/) | platform / framework | partial (site + app org public, client source not confirmed) | 2026-10 | dedicated Windows/Linux servers, native client module, accounts + launcher + server browser; movement, combat, PvP, vehicles, time of day, weather and world destruction replicate ([devblog 2026-08-25](https://open2077.net/devblog/2026-08-25-state-of-the-net)); Lua-scriptable resources; not story co-op |
| [CyberMP](https://cyber.mp/) | mod, closed source | n/a | 2026-06 (press) | GTA-Online-style sandbox with player-run servers, 30–40 players tested per press reports; no public download; site still live |
| [Chaoskjell/Coop-Cyberpunk-Mod](https://github.com/Chaoskjell/Coop-Cyberpunk-Mod) | scaffold | MIT | 2026-08 (one-day burst) | v0 scaffold: CET Lua mods talk to a Python bridge over files/HTTP, bridged over TCP; no gameplay sync claims verified |

Searched: this repo's directory and open PRs/issues, GitHub search and API, project sites
(nightcoop.net, open2077.net, cyber.mp), press coverage (Polygon, PCGamesN, GamingBible, DSOGaming,
player.one). Not checked: Nexus Mods deep pages, Russian/Chinese CP2077 modding communities beyond
what the projects themselves link, Discord archives.

## How the game works (for co-op)

Desk-researched from the projects' READMEs and docs linked above; nothing here was verified by
running the game.

- REDengine 4 is proprietary and the game shipped with no netcode; CD Projekt RED dropped the planned
  multiplayer (Eurogamer, Dec 2022). Every project builds its own UDP layer.
- The community stack all of them sit on: RED4ext (native C++ plugin loader with RTTI access),
  redscript (script-layer compiler; `@wrapMethod`/`@addMethod` annotations wrap game script
  functions), Cyber Engine Tweaks (Lua + in-game UI), ArchiveXL/TweakXL (asset/tweak injection),
  Codeware (redscript framework, entity spawning). This matches the REDengine row in
  `playbook/engine-families.md`, which already carries the Witcher 3 experience.
- Version sensitivity is the dominant maintenance cost. Patch 2.2 (2024-12) is where CyberpunkMP
  stopped. Patch 2.31 renamed/moved RTTI names so the game refuses to start without a name fix
  (upstream PRs #56–#60, merged in oxylol's fork). Current projects target 2.31/2.31a; NightCoop
  requires exactly 2.31.
- Remote players are represented as puppets or stand-in NPCs (Codeware or AMM appearance) because V
  is a single-player entity. NightCoop instead gives each guest their own V: appearance, level, gear
  and cyberware travel with the guest into the host's world.
- World/save authority differs per project. NightCoop and oxylol use the host's save as the world:
  quest progress and city state belong to the host's save, and a guest's own campaign is untouched.
  NightCityMP plans dedicated "multiplayer saves" where everyone keeps their own character.
- Networking is self-built everywhere: raw UDP sockets, Valve GameNetworkingSockets (CyberpunkMP
  lineage, RedSync), or a small Python relay (KyleBuildsAI). NAT traversal via relay hub (NightCoop)
  or Tailscale (NightCityMP dev setup). No project reuses official netcode; none exists.
- Quest sync: the deepest public attempt is RedSync's quest-fact relay (fact whitelist, host
  authority, late-join snapshot; journal, mission NPC AI and dialogue relay still open). NightCoop's
  beta claims full quest and cutscene sync in the shared world. Shared NPC/enemy simulation is
  unsolved in public projects: NightCityMP leaves crowd simulation client-side, RedSync spawns
  scripted bot events, nobody documents shared AI authority.

## What we changed

Nothing. Research-only write-up: no game copy was owned, installed or run, and no mod was built. All
facts come from the linked public sources, verified 2026-10-23.

## QA

none. No claim here was play-tested by the author of this write-up; project capability statements
are quoted from their own READMEs, sites and press, with links. Per the contributing rules,
`unknown` and `untested` are the honest answers for a research-only PR.

## How to play

- NightCoop: open beta since 2026-10-23 at nightcoop.net — everyone installs RED4ext + the same
  NightCoop build; the host opens a session from the in-game Co-op menu; guests join directly or
  through the relay hub.
- KyleBuildsAI/cp2077-coop: downloadable v0.0.37/alpha.5 ZIP from the repo's releases (CET panel
  host/join, Python relay on UDP 11778).
- NightCityMP: no public release ZIP yet; a build-from-source release-kit script is provided.
- No public player download found for: CyberMP (semi-open phase), RedSync (placeholder links),
  Bukczyk (research tree), OPEN//77 (alpha access), CyberpunkMP (stale at patch 2.2), Chaoskjell
  (scaffold).

## Known issues

- Directory freshness was the gap this PR fixes: CyberpunkMP's `last_activity` was `unknown` (it is
  2024-12 — stale), and six live projects were not listed at all.
- NightCoop capability claims are vendor claims (site + press) until community reports land; the
  beta opened the day this was written.
- Story co-op remains partial everywhere: NightCoop covers prologue + Act 1 in beta; RedSync relays
  quest facts only; oxylol syncs world framing (time/weather/tracked quest) but quest internals are
  unverified here.
- Shared NPC/enemy simulation is unsolved in every public project (see above).

## Next steps

1. Watch NightCoop's open beta (started 2026-10-23): if community reports confirm prologue/Act-1
   co-op, the entry's qa can move `none` → `community-reports` and "Where things stand" tightens.
2. The KyleBuildsAI + Bukczyk integration is the open-source story-co-op line worth tracking (typed
   session foundation, player groups). An agent with the game could run the v0.0.37 prototype and
   report what actually works.
3. Finish the unchecked searches: Nexus Mods deep pages, Russian and Chinese CP2077 modding scenes,
   Discord archives of CyberpunkMP and NightCoop.
4. `playbook/engine-families.md`'s REDengine row could gain CP2077 specifics (RTTI rename pain
   across patches, puppet-based remote players, quest-fact sync) once someone contributes hands-on
   evidence rather than desk research.
