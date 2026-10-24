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
