# Agent notes: Dragon Age: Origins

Working notes for adding co-op to **Dragon Age: Origins – Ultimate Edition**
(Steam app 47810, 1.05.13263.0). Plan and evidence live in the game's own
repository `dao-coop`; this file is the cooperator's log for the hub, so the
next agent starts from here. Started 2026-10-20.

## Triage (phase 0)

| | |
|---|---|
| Game | Dragon Age: Origins – Ultimate Edition, 2009 |
| Store / version | Steam, app 47810, build id 21245176, game **1.05.13263.0** (GOG, EA and Steam builds differ; all three are 1.05.13263.0) |
| Engine | **Eclipse** (BioWare in-house, an upgraded Aurora — Neverwinter Nights' engine), 32-bit x86, RTTI intact |
| Existing co-op | **none public, ever**; directory entry is a gap |
| Source available | no; a closed 2026 decompilation project is ~60% done and plans net/co-op (not an artifact) |
| Modding surface | native plugin loader exists (DAOLib, SKSE-style); script VM with 827 commands; engine message bus; Flash UI |
| Path | 2 (build from zero), but **not from zero RE**: DAOLib already maps the engine. |

## What exists (phase 1, checked 2026-10-20)

- **No public DAO co-op mod.** Searched GitHub (repo/code/topics), Nexus,
  ModDB, GameBanana, Steam Community, Reddit, Russian/German/Polish forums,
  archive.org, YouTube, Nucleus Co-op's handler hub. Negative evidence and the
  search ledger are in the research notes of the working session; the summary
  below is what matters.
- **DAOLib** — https://github.com/Whocares66-Dev/DAOLib — GPLv3, SKSE/CommonLibSSE
  style loader (`dinput8.dll`) + C++ library for DAO 1.05.13263.0 on GOG/EA/Steam.
  Address library (100k+ stable IDs) at
  https://github.com/Whocares66-Dev/DAOAddressLibrary. Public releases v0.1.0
  (2026-10-10) → v0.5.0 (2026-10-18). Exposes: all 827 script commands
  callable/interceptable, engine messages, per-frame callback on the script
  thread, object/creature state, orders that look like player clicks, input
  injection, custom Scaleform screens, save hooks + co-save files, game
  modes/pause.
- **DAOKadanMP** — `Whocares66-Dev/DAOKadanMP`, **private** (GitHub 404
  unauthenticated, absent from the public repo list). The only live multiplayer
  attempt; evidence survives only in public DAOLib/DAOEluvianMCP issue text:
  two game processes on one save, clipboard-address join, host/guest object-id
  matching (partial), shared chest contents, in-session saves loading. Known
  gaps from the same issues: kill bodybags/loot not synced (DAOLib #103), blows
  >60 m from the host's camera not landing (DAOLib #88). No release, no
  license, no download — a lead to contact, not a dependency.
- **DAOEluvianMCP** — https://github.com/Whocares66-Dev/DAOEluvianMCP — GPLv3 MCP
  test bench plugin: serves a live game over MCP/HTTP, drives it, supports
  multiple instances on their own ports. Intended automated QA rig for the
  two-client tests.
- **DAO Turn Based** (Nexus 7053), **DAO Photo Cam** (7060), **DAO Elgara
  Shaders** (7061), **Extended Item Variations** (7069) — public DAOLib plugins;
  none networked. The turn-based mod's engine findings (pause, command queue,
  clock) are in DAOLib's `dev/TURN_BASED.md`.
- **2026 decompilation project** — Reddit user `Shot_Let_1344`, 6 devs, ~3
  years, 60% decompiled/55% linked of 34,415 functions, release only at ~90%
  (targeted 2027), **plans** 64-bit, controller, per-player inventories, pause
  rework, "potential net support and co-op campaigns". No repo, site or build
  yet. Treat as a future enabler; no public link to DAOLib's author found.
- **xoreos** (GPLv3 Aurora reimplementation) has DAO as a target but only
  partial spectator gameplay, no netcode. **OpenDAO / Nikami Aurora** is a
  separate MIT Godot compatibility runtime; no multiplayer.

## Engine facts that shape the design

- No netcode, no client/server split; one controlled creature at a time, one
  party inventory, global real-time-with-pause, one dialogue/cutscene camera.
- Game logic runs in an in-engine script VM on one thread; script commands must
  be called from that thread (DAOLib gives a per-frame callback on it).
- A multiplayer mode existed and was cut in development (2021 interviews).
- Two copies can run on one PC as DAOLib **named instances**
  (`DAOLIB_INSTANCE=<name>`): own mutex + own Documents profile, seeded once
  from the default profile's saves.
- A save is a folder; a guest can be sent the host's save, write it into its
  own profile and load it — both games then agree on every object id (DAOLib
  `dev/SAVES.md`, seen in play 2026-10-12 by the private mod's session: guest
  in the host's world 9–19 s later).

## Chosen approach

Two processes, host-authoritative: guest joins by **save transfer**, then state
sync (player state, orders, hostiles, effects, transitions) with host-resolved
damage. Two players share the same party (no puppet spawning needed — both
worlds hold the same objects). LAN UDP first (owner and friend are on the same
network), Steam/Spacewar relay later. Full design and milestones in
`dao-coop/docs/DESIGN.md`.

## Sources

- BioWare no co-op in 2009: https://www.eurogamer.net/dragon-age-has-no-co-op-multiplayer
- Multiplayer mode cut (2021 interviews): https://www.thegamer.com/dragon-age-origins-multiplayer-cut/
- DAOLib: https://github.com/Whocares66-Dev/DAOLib , docs https://whocares66-dev.github.io/DAOLib/
- DAOAddressLibrary: https://github.com/Whocares66-Dev/DAOAddressLibrary
- DAOEluvianMCP: https://github.com/Whocares66-Dev/DAOEluvianMCP
- DAO Turn Based (Nexus): https://www.nexusmods.com/dragonage/mods/7053
- Decompilation project press: https://www.pcgamesn.com/dragon-age-origins/modders-decompiling-source-code
- DAO engine lineage discussion / wiki: https://en.wikipedia.org/wiki/Dragon_Age:_Origins
- xoreos: https://github.com/xoreos/xoreos
- Steam app: https://store.steampowered.com/app/47810/Dragon_Age_Origins__Ultimate_Edition/
