# Call of Duty (2003) and United Offensive (2004)

| | |
|---|---|
| Engine | IW 1.0 (id Tech 3 derived) |
| Versions tested | CoD 1.5 (`version.inf` ExtVersion=1.5), UO 1.51 (ExtVersion=1.51), French language packs. Build identity (PE timestamps) in [NOTES.md](NOTES.md). |
| Projects | none found; directory entries "Call of Duty" and "Call of Duty: United Offensive" (`kind: gap`) |
| QA level | none |
| Last updated | 2026-10-25 |

## Where things stand

**Research only: nothing is playable.** No campaign co-op mod exists for either game as far as these searches go.
This write-up maps what the singleplayer engine still has and what a co-op mod would need. The native networking
part is early and open for contributors, so check the open PRs and issues for "call of duty" before starting from
scratch.

## Existing work

- "CoD 1 Cooperative Mod": only a name, in a 2025 reply on a
  [Steam thread](https://steamcommunity.com/discussions/forum/0/2284960483102477437/). No link, author or build found.
- [Call of Duty 1 Splitscreen PC v1.1](https://www.moddb.com/downloads/call-of-duty-1-splitscreen-pc-v11) (ModDB,
  2024): a 318-byte zip for multiplayer splitscreen, not campaign co-op.
- [CoDUO-YAP](https://github.com/Clippy95/CoDUO-YAP) (GPL-3.0, active 2026-10): UO singleplayer and multiplayer
  plugin (widescreen, FOV, raw input, SP tweaks). Loads as a `ddraw.dll` proxy or ASI. No co-op, and its FAQ says it
  does not support CoD 2003. The most useful base for UO.
- [Open CoD:UO](https://github.com/opencoduo/coduomp) (2026): reconstructed source of the UO **multiplayer** client,
  server engine and game module. No license file, no AI code. Useful to read for engine structures; ask the author
  before building on it.
- iw1x / CoDExtended / libcod / t1x: multiplayer client and server extensions for CoD 1.1 / UO. Not singleplayer.
- A Steam Workshop item called "Call of Duty 2 - Deluxe Edition" (2327804851) turned out to be a Men of War: Assault
  Squad 2 mod, unrelated.

## How the game works (for co-op)

From static analysis with Ghidra of the 1.5 build only. Details, addresses and evidence labels are in
[NOTES.md](NOTES.md). Nothing below has been checked at runtime.

- **The singleplayer exe keeps the Quake 3 client/server split.** It runs a server and a client in one process. The
  server allocates **4 client slots** (`SV_Startup`), reads packets for all 4 and builds snapshots per client. The game
  DLL (`gamex86.dll`) also handles clients 0-3: connect, begin, think and disconnect take a client number, each player
  gets its own AI "sentient", and entities 0-3 are reserved for clients.
- **What is cut:** the network transport is reduced to an in-process loopback (no socket library is imported).
  `SVC_DirectConnect` only sets up slot 0, and the server sends snapshots to client 0 only. The local client reads
  some server state directly from shared memory instead of over the network.
- **So co-op is a host-authoritative design on the game's own netcode, not lockstep:** player 2 would run a normal
  `CoDSP.exe` as a client of the host. That needs a native module (transport, slot connection, sending to all
  slots) plus script work.
- **Scripts:** level logic is plain-text GSC inside the `.pk3` archives: 809 files in CoD with 1210 `level.player`
  references, 315 files in UO with 1916. Shared scripts set `level.player` with `getent("player", "classname")`,
  which fails if more than one entity matches. So the plan keeps the host as `level.player` and gives other players
  another classname, as World at War's co-op does with its host.
- **UO exe:** the code is not encrypted in the tested copy. The SafeDisc section names are leftovers. Same engine
  layout is expected but unchecked.

The campaign order, a per-mission count table and a coarse classification of vehicle missions are in
[NOTES.md](NOTES.md) under "Script (GSC) analysis".

## What we changed

Nothing yet. No code in this PR.

## QA

None. No test ran against the game.

## How to play

Not playable.

## Known issues

Not applicable yet.

## Next steps

1. **The native module** for `CoDSP.exe` 1.5. It is the blocker and early work is underway. If you want to
   pick it up, comment on the PR or open a claim issue. Start from the RE notes and verify every address at runtime first.
2. Script layer for player 2, from the plan in [NOTES.md](NOTES.md): shared libraries first (`_load`, `_loadout`,
   `_spawner`, `_mg42`, `_autosave`), then triggers per mission, small maps first.
3. Same analysis for UO 1.51 (`CoDUOSP.exe`, `uo_gamex86.dll`).
4. Design goals for the finished mod (in NOTES.md): 2 players first, designed for 4; Steam Spacewar
   (AppID 480) relay with direct IP as fallback; downed and revive, or respawn next to the teammate after 5 s; host-driven
   progression with a level select; clear diagnostics when a launch fails.
