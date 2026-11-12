# Outlast (2013)

| | |
|---|---|
| Engine | Unreal Engine 3 (`CookedPCConsole`) |
| Versions tested | Steam build 576074 (Whistleblower DLC installed), Windows 11. OpenOL 2.2.0 (release of 2026-11-02, `OLGame.exe` SHA-256 `5d3fb292baeec5cb03afb4765d361a24f2e07ea6ed6f353d71792cfd6cb4f7d1`) with OpenOL-Relay `69998e7`. Earlier work: OpenOL 2.1.0 (`OLGame.exe` SHA-256 `2e58afbb1203d04476581f34216428d1dfc96228e5f3df8c6881323c56c93346`) |
| Projects | [OpenOL](https://github.com/ShyKiss/OpenOL) with [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) (directory: partial). Archived own work for 2.1.0: [G3RKA/OpenOL](https://github.com/G3RKA/OpenOL), tag `legacy-2.1.0-final` (directory: abandoned) |
| QA level | automated (log-based checks on two real clients, see [QA](#qa) and [NOTES.md](NOTES.md)) |
| Last updated | 2026-11-04 |

## Where things stand

Play Outlast together with [OpenOL 2.2.0](https://github.com/ShyKiss/OpenOL/releases/tag/2.2.0) and [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay).
Its multiplayer covers players, doors, enemies (one AI owner per room, shown to the others as puppets with their animations), checkpoint loads shared
between the players, and server-arbitrated triggers; the traffic is encrypted and rooms can have a password. Two clients on one PC were checked
(test run 33 in [NOTES.md](NOTES.md)); two PCs, Steam P2P and three players are not tested yet.

## Existing work

| Project | What it is | Last activity |
|---|---|---|
| [OpenOL](https://github.com/ShyKiss/OpenOL) | Modified UE3 client (rebuilt `OLGame.exe` and packages) dropped over a licensed Steam install; multiplayer since 2.0.1, rewritten in 2.2.0 (2026-11-02). Source is public, no licence stated | 2026-11 |
| [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) | Standalone UDP relay (CLI and GUI), also linked into the game as the embedded relay; the OpenOL repository pins the commit as a submodule | 2026-11 |
| [G3RKA/OpenOL](https://github.com/G3RKA/OpenOL) | Own work for 2.1.0 (an exe patch, relay patches, play kit, test scripts), archived; its `harness/` also has scripts that test 2.2.0 without a proxy | 2026-11 |
| [Outlast-Together](https://github.com/LucianLandIsBack/Outlast-Together) | MIT-licensed school project with a networking DLL and a client folder, no releases | 2025-02 |

Also looked at: the Steam Workshop item "Outlast multiplayer" (a Garry's Mod map, removed), ModDB and Nexus Outlast mods (single-player only in the results).

## How the game works (for co-op)

- UE3 with a native `OLGame` module. OpenOL ships a rebuilt `OLGame.exe` plus `.upk` files; its source tree has the game code (`Development/Src/OLGame`) and the multiplayer layer (`Development/Src/Multiplayer`).
- Each client talks to the relay only. 2.2.0 opens a handshake (X25519 key exchange, the room password proven without being sent, the server key pinned by the client) and then sends XChaCha20-Poly1305 sealed datagrams on a reliable and an unreliable lane.
- Enemies: every way an enemy enters the world goes through one factory gate that only the AI owner passes; the other clients get puppets (`[MP] Enemy factory ...: spawn` or `not the owner` in the log).
- Checkpoint loads travel as a session change (`[MP] Travel: cp=... epoch=N`) and the other clients apply the same checkpoint.
- With Steam on, the modded build reads the player's Steam Cloud profile; with `-nosteam` it uses `OLGame\SaveData\*.olprofile` and `OL_<date>.sav` inside the game folder. UE3 config goes to `<game dir>\OLGame\Config\` (including `OLMultiplayer.ini`: `IP`, `UdpPort`, `UserName`, `RoomCode`, `Password`, the `Sync...` switches).
- A client can start straight on a map from the command line: `OLGame.exe prison_persistent?game=Multiplayer.MultiplayerGame -nosteam`. The overlay is on `~` in 2.2.0 (Checkpoints tab with a search box, Spawns, Player and Scene tabs); F6 opens the engine console.

## What we changed

Nothing for 2.2.0. For 2.1.0 the work is in the archived fork (see its README) and in the dated test runs of [NOTES.md](NOTES.md) (runs 1 to 32).

## Licence

Checked on 2026-10-25 (GitHub API): [OpenOL](https://github.com/ShyKiss/OpenOL) and [OpenOL-Relay](https://github.com/ShyKiss/OpenOL-Relay) state no licence, so all rights stay with their author by default. OpenOL's README says its files are
"provided only for modding convenience" and that the built `OLGame.exe` replaces the files of your licensed Steam copy. No OpenOL or Outlast binary is redistributed by this repository or by the own work linked here:
no `OLGame.exe`, no `.upk`, no release archive, no game assets. The archived fork is a GitHub fork (upstream files under the upstream terms); its own work in `tools/outlast-coop/` is MIT licensed.

## QA

- 2.2.0 (one PC, two clients, standalone relay on loopback, `-nosteam`): the traffic is encrypted, so the checks read the game logs and the process state. Scripts in the archived fork's `harness/`: `v22.py` (start and stop the relay and clients), `v22_soak.py` (idle run), `v22_scenarios.py` (AI owner, handover, rejoin, relay restart; no mouse or keyboard), `v22_churn.py` (players leave and join), `v22_cp.py` and `v22_sweep.py` (checkpoint loads through the overlay and a log check of both clients).
- Last results (2026-11-04, [NOTES.md](NOTES.md) run 33): 10 minutes idle without error lines; AI owner, handover, rejoin and relay restart 5 of 5; leaving and joining 5 times without failure; at 17 checkpoints of the Prison, Admin, Lab and Male groups the enemies were spawned by one client only, shown on the other, animations playing on both.
- 2.1.0: a scripted suite with real clients and protocol tests, 83 checks passed on the last patched build (details in NOTES.md, runs 1 to 32).

## How to play

Unverified between two PCs. The setup used on one PC:

1. Copy your Outlast install (needs the Whistleblower DLC) and unpack the OpenOL 2.2.0 release over the copy (`OpenOL.zip` or `OpenOL.7z` from the [release](https://github.com/ShyKiss/OpenOL/releases/tag/2.2.0); the GitHub page shows each file's SHA-256, check it). Keep the original untouched. Everyone must use the same game and relay version.
2. Host: either the relay inside the game (Multiplayer, Start Server; guests connect through Steam P2P, room `DEFAULT`), or the standalone relay `OpenOL_Relay --port 7777` built from the commit the OpenOL repository pins (see the OpenOL docs `docs/MULTIPLAYER.md`: `build.sh` or `build_win.bat`). The standalone relay prints `Server key: <hex>` on first start and keeps it in `relay.db.key`; keep that file.
3. Everyone: set `IP`, `UdpPort` (7777) and `RoomCode` (and `Password` if the room has one) in `OLGame\Config\OLMultiplayer.ini`, then use the in-game Multiplayer menu (Connect), or start the multiplayer map from the command line. Add `-nosteam` to keep the Steam Cloud profile out of it. A client remembers the server key on the first connection; the host can hand the key to the players to put into the ini beforehand.
4. Start a game, or load a checkpoint from the overlay (`~`); the other players are taken to the same checkpoint.

## Known issues

None recorded for 2.2.0 beyond what is not tested yet (see below).

## Next steps

- Two PCs over a real network and Steam P2P; three players.
- The remaining checkpoint groups (Revisit, Female, Courtyard, Sewer) on 2.2.0.
- Triggers, doors, pickups and pushables in a real play-through; the scene where an enemy breaks a door.
