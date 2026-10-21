# Call of Duty: Modern Warfare 3 (2011)

| | |
|---|---|
| Engine | IW 5.0 (closed; id Tech 3 derived) |
| Versions tested | Steam appids 42680 (SP: campaign + Spec Ops) and 42690 (MP), 32-bit content set; engine tested against the pre-2026 32-bit dedicated server `iw5mp_server.exe` (SHA-256 `F271C305…418D20`) |
| Projects | AlterWare IW5-Mod (Spec Ops/Survival co-op, closed source); open-iw5 (open engine reimplementation, the base for the campaign co-op attempt) |
| QA level | none (campaign co-op is at reverse-engineering stage; no co-op test has run) |
| Last updated | 2026-10-19 |

## Where things stand

No campaign co-op exists for Modern Warfare 3 (2011) on any platform, and no project has
attempted it. What does exist is 2-player **Spec Ops / Survival** co-op: official in the
game, and reachable on PC today through the closed-source AlterWare IW5-Mod client (both
players need a Steam-owned copy). This write-up is a from-zero attempt at **campaign** co-op,
starting from an open reimplementation of the engine.

## Existing work

- **AlterWare IW5-Mod** — the current community MW3 client, mainly focused on
  singleplayer/co-op plus MP with bots and dedicated servers. Closed source, requires a
  Steam-owned copy. Its co-op is Spec Ops/Survival, not the campaign.
  ([alterware.dev](https://alterware.dev/), [install guide](https://forum.alterware.dev/t/iw5-mod-install-guide/462))
- **MW3 Survival Mode: Revived** (ModDB, 2026) - brings Survival content to the AlterWare
  client; its FAQ states its Spec Ops co-op **desyncs**, i.e. it is not a working co-op sync.
  [moddb.com/mods/mw3-survival-mode-revived](https://www.moddb.com/mods/mw3-survival-mode-revived)
- **TeknoMW3** (TeknoGods) - older client that added LAN/direct-IP for MP and Spec Ops co-op;
  still referenced by users in 2026 but historically piracy-adjacent, so it is prior art only.
- **Plutonium IW5** - multiplayer-only client, account required (https://docs.cbservers.xyz/games/iw5).
- **open-iw5** (momo5502, fork chain cydiakk → louvenarde) - open-source **GPL-3.0** MW3
  singleplayer and multiplayer client, "very experimental and unfinished", requires the MW3
  dedicated-server files and patches them with HDiffPatch deltas. Closed upstream; a surviving
  fork is https://github.com/louvenarde/open-iw5. This is the base our work builds on.

## How the game works (for co-op)

- **Levels** are fastfiles (`zone/*.ff`, unsigned `IWffu100`): an outer zlib container with a
  packed `XAssetList`, per-asset data, and nested zlib streams (ScriptFile string pools,
  RawFile text). The 32-bit build uses 4-byte pointers and a 16-byte packed
  `XAssetList`; the 2026 64-bit build uses 8-byte pointers (32-byte list).
- **Scripting** is GSC compiled into `ASSET_TYPE_SCRIPTFILE` (type `0x27`) assets inside the
  zones. Campaign mission logic, objectives and the story flag store live there. The 10 SP
  campaign zones alone carry ~23.8k script-level flags and ~2.1M strings.
- **Netcode**: the MP side used Demonware; the SP/campaign side has **no netcode at all**, so
  co-op has to be built from scratch. The save system is local.
- **32-bit vs 64-bit**: open-iw5 (and every working community client) needs the *32-bit*
  engine. The Oct-2026 Steam update replaced the game's binaries and the dedicated server with
  64-bit builds (the server depot is now a 117 KB x64 stub), so the 32-bit content has to be
  pulled from pre-2026 depot manifests.

## What we changed

- Built an isolated **32-bit test copy** from pre-2026 Steam depot manifests (42682 assets,
  42681 SP binaries, 42688 Russian localization, 42751 dedicated server). The real install was
  never touched.
- Wrote our own 32-bit-aware fastfile tooling (no third-party unlinker): a zone parser that
  auto-detects pointer size, script-string/flag inventory, nested-zlib extraction, and
  ScriptFile (`.gscbin`) extraction.
- Recovered the **real pre-2026 dedicated server** (`iw5mp_server.exe`, 4,077,480 B, PE32 i386)
  whose SHA-256 matches open-iw5's expected base exactly, and forked/rebuilt open-iw5:
  **build succeeds and the SP client plays a campaign mission.** The loader applies
  `iw5sp.exe.diff` to the dedi base in memory and the engine entry runs to window + filesystem
  init. Launched straight into a level (`open-iw5.exe -singleplayer +map sp_intro`) the client
  loads and plays the mission - live HUD and dialogue subtitles, read back with OCR. A single
  `setsaveddvar` script runtime error is logged during startup and is **non-fatal**; it does not
  stop the game. Details and build fixes are in `NOTES.md`.

## QA

- Campaign co-op: **none** - no co-op test has run.
- open-iw5 SP client was observed running a real campaign mission (window title, ~1.8 GB RSS,
  live subtitles read back with Windows OCR, screenshots captured). Manual observation, not an
  automated test yet.

## How to play

- **Campaign co-op: not available.**
- **Spec Ops / Survival co-op today**: AlterWare IW5-Mod, both players with a Steam-owned copy
  (https://forum.alterware.dev/t/iw5-mod-install-guide/462).

## Known issues

- The campaign GSC assumes exactly one player; whether a second player entity can exist in the
  SP engine is unproven.
- open-iw5's upstream repo is deleted (GitHub 404), all 11 submodules were empty in our copy,
  and it needs a deleted `xlabs` branch of gsc-tool; the fork is currently reconstructed by hand.
- A `setsaveddvar` script runtime error is logged once during startup. It is **non-fatal** (the
  mission still runs); the caller (which script sets which dvar, before the dvars exist) is not
  yet identified.
- A bare `-singleplayer` launch (no level requested) does not reliably reach the in-game
  frontend; a level must be named explicitly to get into gameplay.
- AlterWare notes an Oct-2026 Steam update breaking compatibility; a community downgrade tool
  exists. Our 32-bit copy predates that.

## Next steps

- Reach a mission through the in-game frontend (a bare `-singleplayer` launch currently does not
  reliably show the menu), and identify the `setsaveddvar` caller so the startup error can be
  explained or silenced.
- Build the host-authoritative replication layer (players, entities, story flags) - Phase 4+.
- Use the extracted ScriptFile `.gscbin` plus the campaign GSC dumps to map which scripts set
  which flags.
- Propose an engine-family row for the Call of Duty (IW) engine in `playbook/engine-families.md`.
- Raw session notes (all sessions, including depot manifests and tool details): `NOTES.md`.
