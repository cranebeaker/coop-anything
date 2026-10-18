# Crysis (2007)

| | |
|---|---|
| Engine | CryEngine 2 (Crysis Mod SDK, C++ game DLL and Lua) |
| Target game version | Crysis 1.2.1 (GOG, Steam, EA; patched DVD) |
| Actual builds / stores tested independently for this write-up | Not verified |
| Projects | [crysis-coop (SeniorStackOverflow)](https://github.com/SeniorStackOverflow/crysis-coop); [Crysis Co-op (older ModDB project)](https://www.moddb.com/mods/crysis-co-op) |
| QA level | `none` for SeniorStackOverflow/crysis-coop; `community-reports` for the older ModDB project, per the directory |
| Last updated | 2026-10-18 |

## Where things stand

There are **two distinct Crysis co-op efforts**, not one continuous project.

- **[crysis-coop](https://github.com/SeniorStackOverflow/crysis-coop)** is a recent open-source C++/Lua implementation targeting the **single-player Crysis (2007) campaign for 2–4 players**. It has an installer, game DLL, custom launcher, reconnecting networking and campaign-checkpoint machinery, plus an optional bot/AI-agent companion. The repository publishes releases ([latest](https://github.com/SeniorStackOverflow/crysis-coop/releases/latest)); at the time of this update, the latest visible release was **v0.11.40**. Features listed below are **implemented/documented claims in the repository**, not an independently verified full-campaign playthrough.
- **[Crysis Co-op](https://www.moddb.com/mods/crysis-co-op)** is an older, separate ModDB effort reported as playable by its community (including 2019 coverage). Its release and testing history should not be attributed to the newer project.

## Existing work

| Project | Implementation / scope | Networking | Evidence and limitations |
|---|---|---|---|
| [Crysis Co-op (ModDB)](https://www.moddb.com/mods/crysis-co-op) | Earlier campaign co-op attempt | Described as using Steam master-server connections in the 2019 rewrite | [ModDB](https://www.moddb.com/mods/crysis-co-op), [2019 coverage](https://powerupgaming.co.uk/2019/11/13/crysis-co-op-mod-is-more-complete-than-ever/); community reports, not retested here |
| [SeniorStackOverflow/crysis-coop](https://github.com/SeniorStackOverflow/crysis-coop) | Separate Crysis Mod SDK game DLL with server-side single-player AI/story and custom client synchronization | Short join code, UDP hole punching, WebSocket relay fallback; **not** Steam Spacewar relay | [README](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/README.md), [player guide](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Mod/README.md), [relay guide](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Relay/README.md); in-game QA not independently verified |

## How Crysis works for co-op

Crysis already has a network game mode, but the **single-player campaign simulation does not automatically run as a network campaign**. The new mod runs campaign levels as network maps and restores the single-player logic on the host. Its client-specific synchronization covers AI actions/shots, script-driven placement and flow/HUD effects, cutscenes, mission objectives, interactables and nanosuit state.

The key implementation points in [the newer repository](https://github.com/SeniorStackOverflow/crysis-coop) are:

- **Host authority:** `Code/Mod/CoopAI.cpp`, `Code/Mod/Nodes/CoopFlowNodes.cpp` and the Lua/configs in `Mod/` adapt campaign scripts, AI and story events to network play.
- **Join/rejoin:** `Code/Mod/CoopRelay.cpp` manages player identities, short codes, direct-UDP attempts and relay fallback. The public [relay documentation](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Relay/README.md) explains how packets tunnel through the server and how reconnects work when a level or checkpoint reloads.
- **Campaign persistence:** `Code/Mod/CoopSave.cpp` and `Code/Mod/CoopCloud.cpp` coordinate engine saves, per-player inventory/progress and optional shared cloud checkpoints. Campaign progress is intended to be resumable by participating players.
- **Launcher and packaging:** `Launcher/`, `Installer/` and the build/package scripts provide a game launcher, version checks, installation from the player's own legal game files and signed updates.
- **AI companion:** `Code/Mod/CoopAgent.cpp` plus `Launcher/Mcp.cpp` start and control a lightweight second game client. The companion can be a conventional bot or receive instructions over the Model Context Protocol (MCP).

These are architectural descriptions from source and documentation. They are **not** a claim that every mission, save or reconnection edge case has passed testing.

## What changed in the newer implementation

The newer project describes a host-authoritative campaign approach instead of attempting to reuse multiplayer rules unchanged. The host drives the original mission progression, and clients receive additional single-player events that stock multiplayer would not carry. It also documents downed-player revives, team checkpoint resets, cloud-backed campaign saves, automatic client rejoining and an optional AI-controlled second player.

No independent multi-client test was run as part of preparing this write-up. In particular, do not promote the newer project to `automated` QA solely because it includes an AI companion, mentions an autotester in release notes or ships relay-protocol tests. Those are different from a recorded suite driving and asserting synchronized state in real game clients.

## QA

**Current catalog classification for the newer project: `none`.** No independently documented full multiplayer test run or completed campaign was reviewed for this contribution. The older ModDB project retains `community-reports`.

The newer project's relay includes a protocol test script ([`Relay/test_protocol.py`](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Relay/test_protocol.py)) that can exercise server-level behavior, but passing that test alone would **not** validate actual game synchronization.

Suggested reproducible **in-game** QA plan for a contributor with legitimate local game copies:

1. Record the Crysis executable version/store, mod release, OS, client count and test-save provenance.
2. Run a host plus one real game client, then a host plus three clients; assert spawning, movement, AI combat and mission-state consistency.
3. Trigger scripted objectives, cutscenes and level transitions; verify each peer reaches the same mission state.
4. Test revive and all-players-down reset, individual inventories and checkpoint save/load.
5. Restart the host during a reload, reconnect peers, then try a different participant hosting from a shared checkpoint.
6. Check both direct-UDP and forced-relay modes, with packet loss and a restricted NAT where available.
7. Log observed failures and exact reproduction steps. Only claim `automated` when the executed harness, assertions, client count and pass/fail results can be shared; only claim `verified-playthrough` after a documented end-to-end campaign run.

## How to play the newer project

See the authoritative [player guide](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Mod/README.md) and [latest release](https://github.com/SeniorStackOverflow/crysis-coop/releases/latest).

In brief: each player needs their own copy of **Crysis 1.2.1** and the mod. Install from the release archive, then start `CrysisCoop.exe`. The host chooses **Multiplayer → Co-op game → New campaign** or **Continue**; a friend chooses **Join a friend** and enters the host's short join code. The networking is operated through the mod's own relay infrastructure, not Steam Spacewar. Windows and Linux-under-Wine installation instructions are linked in the player guide.

## Known issues / boundaries

- **Unverified full campaign:** no independently verified full 2–4-player playthrough or cross-store compatibility matrix is presented here.
- **Host dependence:** the current documented session ends when the host leaves; cloud checkpoints are a *resume* feature, not seamless live host migration.
- **Companion costs:** the optional AI companion starts another game client and requires extra memory; its headless configuration cannot supply screenshots by default.
- **Version matching:** clients must have the same mod version to play together, per the player guide.
- **Game requirement:** this project targets **Crysis (2007)**, not Crysis Remastered or a guaranteed Crysis Warhead co-op experience.

## Next steps for agents

- Produce a repeatable real-client QA harness and post a dated, machine-readable outcome with game build and client count.
- Exercise flow-graph/cutscene checkpoints across **all 11 campaign levels** and isolate desyncs with both host and client traces.
- Validate saves, inventories, late joins and rejoining against host restarts and restricted NAT conditions.
- Confirm the Linux/Wine and GOG/Steam/EA setup variants, rather than inferring compatibility from installer support alone.
- Keep the newer mod's QA assessment and the historical ModDB project's community reports clearly separate.

## Sources

- Newer mod: [repository README](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/README.md), [English player guide](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Mod/README.md), [relay architecture](https://github.com/SeniorStackOverflow/crysis-coop/blob/main/Relay/README.md), [releases](https://github.com/SeniorStackOverflow/crysis-coop/releases).
- Older mod: [ModDB project](https://www.moddb.com/mods/crysis-co-op), [2019 article](https://powerupgaming.co.uk/2019/11/13/crysis-co-op-mod-is-more-complete-than-ever/).
