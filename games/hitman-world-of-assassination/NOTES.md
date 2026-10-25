# HITMAN World of Assassination: co-op notes

Session 2026-10-24. Research only (phases 0-1). No game install was available, so nothing was run against the game.

## Game identity (rule 5)
- Store: Steam app 1659040 (from the Steam announcement URL). Version, build id, exe SHA-256, OS: **unknown** (no install).
- Engine: Glacier 2 (ZHMModSDK targets it; its type names use the `Z`/Glacier conventions).

## Observations (checked this session)
- `OrfeasZ/ZHMModSDK` contains `Mods/Hitmen`, added 2023-04-09 by OrfeasZ. It is commented out (`#Hitmen`) in the root
  `CMakeLists.txt`, so it is not built or released.
- Hitmen sync, read from `Mods/Hitmen/Src/Hitmen.cpp`:
  - Transport: Steam Networking Sockets, direct IP plus port, one client (`// TODO: Multiple clients.`).
  - Player: the world matrix plus a raw 0x148-byte copy of the input struct, written directly into the other Hitman's
    input processor.
  - NPCs: the host sends a world matrix for every live actor, indexed by actor id; the client calls `SetWorldMatrix`.
  - Not synced: AI state, deaths, disguises, items, doors, objectives, mission scripting, saves.
  - Packet parsing has no bounds checks, and the source flags it four times with `// TODO: This is extremely incredibly unsafe`.
- `sdwhw/ZHMModSDK` is 2 commits ahead of upstream (a merge and a Proton/D3D12 wrapper change), with no extra multiplayer work.
- Official: IOI announced "Stone & Knight", a two-player co-op mode for WoA, at its June 2025 showcase. Search on
  2026-10-24 found no release date, beta or playtest.

## Derived
- Hitmen is a tech demo of "two 47s in one scene", not playable co-op. Turning it into co-op means building the
  whole gameplay sync layer (AI, damage, objectives, mission state). See playbook/how-coop-mods-work.md.
- Because the receive path copies remote bytes into game memory, anyone you connect to can write into your game
  process. Never use it with untrusted peers.

## Unknowns
- Whether Stone & Knight has shipped or has a date (blocked: Steam, PCGamingWiki, hitmanforum.com and pcgamesn.com
  are denied by this sandbox's network policy; only search snippets were seen).
- DRM and anti-cheat status of the current build.
- Whether Hitmen still compiles or runs on the current game version.
- Discord-only community projects (not searched).

## Searched
- WebSearch 2026-10-24: "Hitman World of Assassination co-op mod multiplayer", "Hitman 3 multiplayer mod github
  Glacier 2 co-op ZHMModSDK", Peacock server, Stone & Knight status (several phrasings).
- GitHub: cloned OrfeasZ/ZHMModSDK and sdwhw/ZHMModSDK. GitHub search API not available in this session.
- Not checked: Nexus file pages, Reddit, Hitman modding Discord, this repo's issues/PRs (API not available).

## Session 2026-10-25: build from zero, step 1 (design and game-free sync core)

Owner has a Windows PC with the game; this session ran in a Linux cloud sandbox without it, so this step is game-free.

### Done
- Design: authority (host runs every NPC; players own themselves; dragged bodies owned by the dragger with epochs;
  host-ordered mission log; world epoch per save load), engine facts from ZHMModSDK headers, phased plan with tests.
- Sync core `hmcoop` (C++17, no engine dependency) built on `tools/ownership`, wrapped as a `tools/fuzz` target.
- Lives in its own repo (not published yet): `hitman-coop`, with DESIGN.md, tests and regression scenarios.

### Checks that ran (Linux, g++ 13.3, 2026-10-25)
- `tools/ownership/run_tests.py`: ALL OK (30 tests, 7 of 7 twins). `tools/fuzz/run_tests.py`: ALL OK (12 of 12 twins).
- `hmcoop_tests`: 24 passed, 111 checks, 2995 fuzz sessions, 7 of 7 mutation twins caught, ALL OK.
- `hmfuzz --minutes 2`: 254377 sessions over 2, 3 and 4 players, 0 failed.
- QA level for the game: none (no game client involved).

### Bugs the fuzzer found in our own core (fixed, regression scenario kept for each)
1. Delayed hello from an old connection hijacked the new session (fix: session ids grow per connection, host keeps the
   last one across a leave).
2. Host's refused drag request still made the body follow the host.
3. Host acked a position packet while rejecting the body update inside it, so the body's last position was lost.
4. A guest re-requesting a body it had just dropped ended up owning it without reporting it.

### Observed in ZHMModSDK source (not yet in game)
- `IActor`: IsAlive / IsDead / IsPacified, ItemPickup, DropItem, Knowledge(). `ZActor::m_OutfitRepositoryID`, `m_rOutfit`.
- `ZPlayerRegistry` has 4 player slots; `ZNetPlayerController` has `m_bConnectedToMultiplayer` and a RakNet replica
  pointer (likely Ghost Mode leftovers).
- Scene load/clear hooks exist; QuickSave saves by signalling `SaveQuick` on an entity.

### Unknown (next session needs the game)
- Whether NPC AI perceives a second Hitman. This is the gate for the whole project.
- How to stop a client's NPC AI without disabling the NPC; which event carries target kills and objectives.
- Game identity (version, exe SHA-256) of the owner's install.
