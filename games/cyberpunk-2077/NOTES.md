# CP2077 research notes (2026-10-23, re-verified 2026-10-25)

Research-only write-up: no copy of the game was bought, installed or run at any point
(per AGENTS.md ground rules). Everything below is desk research with sources.

## How data was gathered and verified

* GitHub repo metadata (pushed_at / stars / license / archived) via the public
  `api.github.com/repos/<owner>/<repo>` endpoint on 2026-10-23; all dates
  re-verified 2026-10-25. Per-project API URLs are listed in each entry's `sources`.
* Project READMEs via `api.github.com/.../readme` (raw\.githubusercontent direct
  fetches returned empty from this network; the API endpoint worked).
* Live project sites fetched directly: nightcoop.net (Russian original says
  "Открытая бета идёт" = open beta running; opened 2026-10-23 15:00 UTC),
  open2077.net, cyber.mp (all HTTP 200 on 2026-10-23).
* Press coverage: Polygon / PCGamesN / GamingBible / player.one / kocpc — used to
  keep NightCoop and CyberMP (two different projects) clearly separated.

## Ecosystem summary (10 projects)

1. NightCoop — open beta since 2026-10-23, prologue + Act 1, 8 players, story
   dialogue by majority vote, closed source, free. Closest to story co-op.
   Sources: <https://nightcoop.net/> and the Polygon piece (see entry sources).
2. CyberpunkMP (Tilted) — last push 2024-12-21 (API), patch-2.2 era, stalled.
   64 stars. Source: <https://api.github.com/repos/tiltedphoques/CyberpunkMP>
3. NightCityMP — CyberpunkMP continuation fork, game patch 2.31a, last push
   2026-07-20 (API). Sandbox-oriented; NPCs/quests/combat damage not synced.
   Source: <https://api.github.com/repos/blyatiful1/NightCityMP>
4. oxylol/cp2077-coop — story-host layer; host pushes time/weather/tracked quest
   every 2 s; guest clocks only move forward. Last push 2026-10-10 (API).
   Source: <https://api.github.com/repos/oxylol/cp2077-coop>
5. KyleBuildsAI/cp2077-coop — v0.0.37/alpha.5 downloadable (CET panel + Python
   relay on port 11778); now integrates Bukczyk's typed-session foundation.
   Last push 2026-10-20 (API).
   Source: <https://api.github.com/repos/KyleBuildsAI/cp2077-coop>
6. Bukczyk/CP2077-Coop — typed session foundation (SessionServer / CoopPlugin /
   runtime/session); one-PC two-client diagnostic reached JOINER-fires-at-HOST
   validation with health loss and death; reconnect death recovery still fails.
   Last push 2026-10-20 (API).
   Source: <https://api.github.com/repos/Bukczyk/CP2077-Coop>
7. CyberMP — press-level entry only: reported 2026-06, no public download, site
   still up. Kept distinct from CyberpunkMP (different projects).
   Sources: PCGamesN and dsogaming articles (see entry sources).
8. OPEN//77 — dedicated-server platform (RP/freeroam/Lua-scriptable), still
   active: org repo open77-app pushed 2026-10-25 (API). Not story co-op.
   Source: <https://api.github.com/repos/Open2077/open77-app>
9. RedSync — AGPL-3.0, RED4ext + redscript + .NET 9 server over
   GameNetworkingSockets; host-authoritative quest-fact relay (PL q301-q307).
   README download links still point at a placeholder domain (your-domain.com),
   so no public release yet. Last push 2026-08-25 (API).
   Source: <https://api.github.com/repos/nobody71004/RedSync>
10. Chaoskjell/Coop-Cyberpunk-Mod — MIT, v0 scaffolding (CET Lua + Python file
    bridge), one-day burst of activity 2026-08, no gameplay sync. Last push
    2026-08-09 (API).
    Source: <https://api.github.com/repos/Chaoskjell/Coop-Cyberpunk-Mod>

## Technical consensus (fed into the write-up's "How the game works")

* Community stack: RED4ext (C++/RTTI) + redscript (@wrapMethod) + CET (Lua/UI)
  * ArchiveXL/TweakXL + Codeware.
* Version sensitivity is the biggest maintenance cost: 2.31 RTTI renames made the
  game refuse to start (fixed by upstream PRs #56-#60; oxylol's fork includes them).
* Remote players appear as puppet/stand-in NPCs (Codeware/AMM); NightCoop is the
  exception — every guest brings their own V.
* World authority: NightCoop/oxylol = the host's save IS the world; NightCityMP
  plans a dedicated multiplayer save.
* Networking is all self-built: UDP / GameNetworkingSockets / Python relay; NAT
  via relay hub or Tailscale.
* No public project solves shared NPC/enemy simulation; the deepest public quest
  sync is RedSync's quest-fact relay.

## directory.yaml change log

* 4 existing entries: checked dates refreshed; CyberpunkMP last_activity
  unknown -> 2024-12 with a stale note; oxylol/KyleBuildsAI last_activity
  unknown -> 2026-10; notes expanded; API URLs added to sources.
* 6 new entries: NightCoop / NightCityMP / RedSync / OPEN//77 / Bukczyk /
  Chaoskjell, inserted after the KyleBuildsAI entry to keep the game contiguous.
* `python tools/build_directory.py` run to regenerate DIRECTORY.md
  (CONTRIBUTING requirement).

## Follow-ups for the next agent

* Flip NightCoop qa: none -> community-reports once beta word-of-mouth lands.
* Anyone owning the game could field-test KyleBuildsAI v0.0.37 and report back.
* Not yet searched: deep Nexus pages, Russian/Chinese community forums, Discord
  archives.
