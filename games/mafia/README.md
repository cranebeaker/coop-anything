# Mafia: The City of Lost Heaven (2002)

| | |
|---|---|
| Engine | LS3D (Illusion Softworks); the game log reports `LS3D Engine V3.95.1 Jun 24 2003` |
| Versions tested | Steam app 40990, build 2300614, `Game.exe` SHA-256 `303eb95ee2de3433511ce0cb518921dcb96b62b0f64ebfcc436723ff5083f298`, Windows 11 |
| Projects | Lost Heaven Multiplayer, Mafia Online, Mafia: Oakwood (see [DIRECTORY.md](../../DIRECTORY.md)); story co-op work started here, [claim #167](https://github.com/cranebeaker/coop-anything/issues/167) |
| QA level | none |
| Last updated | 2026-10-24 |

## Where things stand

Nobody can play the story together yet. Every project found is free-roam multiplayer, and none syncs the
campaign. A story co-op mod is being started from zero (plan: [PLAN.md](PLAN.md)). So far it has reached
research and static analysis; there is no mod code yet.

## Existing work

| Project | What it syncs (from its own pages) | State | Usable as a base? |
|---|---|---|---|
| [Lost Heaven Multiplayer](https://github.com/LHMPTeam/lhmp-old) | Players, cars, weapons, doors, pickups, chat; Squirrel game modes; no missions | Abandoned (2013-2015 code, repo archived, last commit 2018-11) | Apache-2.0, but its fixed hook addresses do not match the Steam `Game.exe` (e.g. the code at hook site `0x49BFEA` differs, and its player global `0x6F9464` is outside the image) |
| [Mafia Online](https://github.com/MafiaHub/MafiaOnline) | Server-owned players and cars, server-chosen mission loading, chat; mission actors suppressed | Alpha, development build only (created 2026-09) | Matches the Steam `Game.exe` exactly, but the MafiaHub OSS license allows derivatives only as contributions upstream and personal use only with written permission |
| [Mafia: Oakwood](https://zaklaus.itch.io/mafia-oakwood) ([source](https://github.com/MafiaHub/rc1-oakwood)) | Players, vehicles, combat per its README | Canceled per itch.io (client source 2020-08) | Its README requires English 1.2, Steam or GOG, but the license forbids derivative works |
| Unnamed 2-player mod for 1.2 ([Steam thread](https://steamcommunity.com/app/40990/discussions/0/1693785035810042404), 2018) | The host streams video to a browser | Gone (video link dead per 2023 posts) | No |

"Mafia Connected" (a roleplay server mentioned in the same thread in 2024) was not found by web search,
so it is unknown. Not checked yet: ModDB/Nexus file pages, archive.org copies of `mafia-mp.eu`/`lh-mp.eu`, the
MafiaHub Discord, Reddit and Czech-language sources.

## How the game works (for co-op)

Everything below was observed on the Steam build listed above. The details and commands are in [NOTES.md](NOTES.md).

- **Binaries.** `Game.exe` is 32-bit (image base `0x400000`), not SteamStub-wrapped and without Steamworks.
  It imports `KERNEL32, USER32, ADVAPI32, WINMM, LS3DF, IJoy, rw_data`. `LS3DF.dll` is the engine.
  `INet.dll` imports Winsock and COM; whether any usable retail netcode exists is unknown.
  A main-menu camera named `multiplayer_pos` exists next to `single_pos`.
- **Start-up.** The game needs the Windows **DirectPlay** optional feature (Steam's install script
  enables it). Without it, `Game.exe` sits behind a Windows Features prompt and never opens a window.
  Without saved settings, `Game.exe` starts `setup.exe` and exits with code 0.
- **Settings and saves.** Setup writes one 63-byte binary value,
  `HKCU\Software\Illusion Softworks\Mafia\LS3D_setup` (bytes 2-5 and 6-9 hold the width and height).
  It is shared by every copy of the game on the PC. Profiles and saves live in `<game dir>\savegame\`
  (e.g. `mafia000.sav`, starting with `forP`), so each copy keeps its own.
- **Rendering.** Direct3D 8, fullscreen per the log line `Trying to create device with FULLSCREEN mode`. The
  window class is `LS3D`. With "Suspend when inactive" on, the title becomes `Mafia ..zzZZ` when the window
  loses focus. `PrintWindow` with `PW_RENDERFULLCONTENT` captures the game window (useful for bot screenshots).
- **Data archives.** `.dta` files (`ISD0`): header and file table are XOR-ed with an 8-byte key per archive,
  and blocks are raw or LZSS/RLE. The keys published in
  [MafiaUnity](https://github.com/MafiaHub/MafiaUnity) (Apache-2.0) work on the Steam build: 11 archives,
  27,018 files, 80 `missions\*\scene2.bin` (79 in `a1.dta`, plus a newer `mise16-krajina` in `a8.dta`).
- **Story logic is text scripts ("Mafia Script")** inside each mission's `scene2.bin`. They live in script
  objects (special type 5), in characters (special type 27: enemies, NPCs, partners) and in init entries
  (`0xAE50`/`0xAE51`, e.g. `GameInitStart`). The 80 scenes hold 3,652 scripts with 119,276 lines.
  `Game.exe` holds a table of 512 upper-case command keywords. The shipped scripts use 342 of them
  (325 in story scenes), and none outside the table. Examples: `findactor`, `frm_seton`, `act_setstate`,
  `enemy_move`, `human_talk`, `door_lock`, `mission_objectives`, `autosavegame`, `endofmission`.
  Two large functions reference most keyword strings and are probably the parser (unconfirmed).
- **Story state.** The story is many small per-object scripts with labels, events and timers (about 45
  per scene), plus checkpoint saves (`autosavegame`, `autosavegamefull`) and `endofmission`. There is no
  single global quest store like the Bethesda games have.
- **Tools that worked.** Ghidra 12.1.4 headless with `DecompileAll.java`: `Game.exe` 3,667 functions
  decompiled (2 failed), `LS3DF.dll` 1,485 (0 failed), with almost no symbols. A Python DTA reader and
  script dumper (port of MafiaUnity, Apache-2.0) will be published in the mod's repo.

## What we changed

Nothing in the game yet. A test copy of the Steam install reaches the main menu unmodified once
DirectPlay is installed.

## QA

None yet. Every test launch so far ran on an isolated copy. A guard script hashes the owner's Steam
install, the Mafia registry keys and the save locations before and after each run. It passed after every
run and failed when its baseline was deliberately altered.

## How to play

Not playable yet.

## Known issues

Not applicable yet. For the stock game: install DirectPlay first, and expect Setup on the first start.

## Next steps

See [PLAN.md](PLAN.md). Next is M2: a `winmm.dll` proxy in a test copy that redirects `LS3D_setup` per
instance, forces windowed mode, skips the first-run profile prompt and logs the local player each frame.
After that: puppets (M3), mission sync with client scripts gated (M4), and effect capture on the
script command handlers (M5).
