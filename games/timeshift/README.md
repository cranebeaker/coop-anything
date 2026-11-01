# TimeShift (2007)

| | |
|---|---|
| Engine | Saber3D (Saber Interactive), Direct3D 9, Havok, FMOD, Bink; its own typed, event-driven level scripts; GameSpy-era client-server multiplayer |
| Versions tested | PC patch 1.2 (menu "Ver 1.02"), 32-bit `bin\TimeShift.exe` SHA-256 `689c63da36cd5902edcad2abe1111d4697a92b12c4e17fe0659b856752641be4`, and the community's pre-patched exe of the same code; Windows 10 and 11. Steam copies are patch 1.0 and need a community 1.2 update package that the player supplies (simulated Steam folder tested, a real Steam install not yet) |
| Projects | [TimeShift Co-op](https://github.com/dolbaras/timeshift-coop) (directory entry "TimeShift Co-op") |
| QA level | automated (a bot drives a host and a client through all 24 campaign levels with the level changes; menus, split screen and the installer are driven by scripts) |
| Last updated | 2026-11-01 (0.1.1) |

## Where things stand

Two to four players can play the story campaign together over a local network (the Internet path through OpenSpy
and direct IP is written but not yet tested between two real homes), and two players can share one PC in split screen.
The mod is a `dinput8.dll` proxy plus data built on the player's own machine from their own game archives; it
uses the game's own network code, not a second one. A bot has run a host and a client through all 24 levels, with the
level changes, for both 0.1.0 and 0.1.1. A whole campaign played by people is still to come, and sound has only been
checked in logs, not by ear. Releases come with a one-file setup exe or a zip, and the "TimeShift Co-op" shortcut
installs a newer release from GitHub before each start ([releases](https://github.com/dolbaras/timeshift-coop/releases)).

## Existing work

- Before this project: no co-op mod found (GitHub, Nexus, ModDB, Steam Workshop and discussions, PCGamingWiki, searched
  2026-10-24; the only "TimeShift co-op" Workshop item belongs to another game). Claim issue: #191.
- The game's own 16-player multiplayer still runs on a LAN or by direct IP, and the mod revives the Internet list through
  OpenSpy instead of the closed GameSpy.
- A community "CPU fix" exe (by demon27248) for the 8+ thread crash is recognised by the installer; the mod does the same
  start-up fixes in memory by itself. The Steam 1.2 update package is community made, is not shipped or downloaded by the
  mod, and is only applied with the player's explicit consent.

## How the game works (for co-op)

- **Archives and scripts:** `.s3darc` archives (an extractor and a patch-archive writer are in the mod's `tools/`). Levels
  carry their logic as source in the engine's script language inside the level files; about 400 native functions are
  exposed to it; the scripts assume one global `$player`.
- **Netcode:** the engine has a replicated client-server multiplayer (a string dictionary for synced names, replicated
  objects). Campaign levels can run on a server with it; the mod adds its own messages on top of the game's (levels,
  story events, checkpoints, joins, split-screen state, Continue loadouts, AI weapon events).
- **Starting on today's PCs:** more than 8 CPU threads crash the engine, the driver-version check blocks the start, GPUs
  that are neither NVIDIA nor ATI are refused ("unsupported shader platform"), and the 100 MB video memory minimum fails on
  integrated GPUs. The proxy fixes each in memory.
- **Saves:** the campaign checkpoint of a co-op game is stored per profile with the loadout; the mod's Continue rebuilds the
  host's and each client's weapons, ammunition and grenades.
- Details are in the mod repository's `docs/` (`multiplayer.md`, `splitscreen.md`, `campaign-audit.md`, `coop-continue.md`).

## What we changed

- **Host story, clients replay it:** story zones, checkpoints, level exits and team restart run on the host and are
  replayed on clients; a client can join a level in progress and gets its state.
- **Time powers:** slow and stop apply to everyone, one power at a time, each player has their own energy; the game's own time
  recorder drives reverse on the host.
- **AI on clients:** movement, deaths, corpses, weapon fire and, since 0.1.1, the shots' sound (weapon events applied to the
  clients' weapon copies). Root cause of an `AccessViolation` seen in about a third of the logs: the mod's own
  `IsBadReadPtr` page loop; replaced by a `VirtualQuery` check, 0 occurrences in the logs after.
- **Campaign for several players:** zeppelin intro voices, levers, buttons, mounted guns and quad on clients, AI aims at the
  nearest living player (a second split-screen player counts), lifts and trains wait for everyone and say how many are
  missing, a Regroup entry in the pause menu.
- **Split screen:** own HUD, weapons, pick-ups, saves; turret seats for player 2; gamepad-only entry of address and password.
- **Continue:** real loadout restore for the host and each client.
- **Hints:** the game's `ShowGamePlayHint` shows nothing in this build; the mod shows its hints with `$player.ShowMsg`
  (forwarded to clients), found by an on-screen check in 9 languages.
- **Steam and installer:** the installer recognises a Steam copy, asks for explicit consent, applies the player's 1.2 package
  with a backup and restores it exactly on uninstall; a single-file setup exe; a bug-report collector that removes user
  names, computer names and the public IP.

## QA

- A bot (scripted menus, a host and a client on two PCs) goes through all 24 levels with the level changes. Last run
  2026-11-01 on 0.1.1: ready on every step, the client on the next level each time, 0 crashes, 0 `AccessViolation`.
- Split screen was run on four levels (two with turrets and fire from player 2), a real host-and-client pair for a level
  change and Continue with a client, emulated links with 5 %, 15 % and (earlier) 30 % packet loss, 8 KB/s and 300 ms.
- Installer: zip and exe installs, reinstall and uninstall compared to a clean folder by file hash; the Steam branch with a
  simulated Steam folder; self-update from 0.1.0 to 0.1.1 through the real shortcut with a local feed.
- Per-level results are in `docs/campaign-audit.md` in the mod repository.

## How to play

Download `timeshift-coop-setup-<version>.exe` (or the zip and `Install Co-op.bat`) from the
[releases](https://github.com/dolbaras/timeshift-coop/releases), start the game with the "TimeShift Co-op" shortcut, and use
Co-op in the main menu to host a chapter or join by LAN list, Internet list or address. Every player needs the same version.

## Known issues

- Soldiers' voices and gibbing sounds are heard only by the host.
- Joining over a very bad link (around 30 % loss) can need a second click on Join.
- Split screen on a client is basic: its second player's pick-ups work, its time power and kills over the network are not
  proven. "Show both views" (`BothViews=1`) was tried on Intel graphics only.
- Not yet tested: the Internet between two homes, a real Steam install and the Program Files (x86) permission prompt,
  mouse clicks on Accept and Continue, a change of resolution, sound by ear.
- The setup exe is not code-signed, so Windows SmartScreen can warn.
- The client log shows about 150 harmless "unknown object name" lines per run on one level.

## Next steps

- A whole campaign played by people, and the Internet between two homes.
- Soldiers' voices and gibbing sounds on clients.
- A real Steam installation and the Program Files (x86) permission path.
- Sound judged by ear; split screen on a client with its own time power.
