# TimeShift (2007)

| | |
|---|---|
| Engine | Saber3D (Saber Interactive), Direct3D 9, Havok, FMOD, Bink; its own typed, event-driven level scripts; GameSpy-era client-server multiplayer |
| Versions tested | PC patch 1.2 (menu "Ver 1.02"), 32-bit `bin\TimeShift.exe` SHA-256 `689c63da36cd5902edcad2abe1111d4697a92b12c4e17fe0659b856752641be4`, and the community's pre-patched exe of the same code; Windows 10 and 11. Steam copies are patch 1.0 and need a community 1.2 update package that the player supplies (simulated Steam folder tested, a real Steam install not yet) |
| Projects | [TimeShift Co-op](https://github.com/dolbaras/timeshift-coop) (directory entry "TimeShift Co-op") |
| QA level | automated (a bot drives a host and a client through all 24 campaign levels with the level changes; menus, split screen and the installer are driven by scripts). Nobody has played 0.1.4 by hand |
| Last updated | 2026-11-10 (0.1.4, published 2026-11-09) |

## Where things stand

Two to four players can play the story campaign together over a local network (the Internet path through OpenSpy
and direct IP is written but not yet tested between two real homes), and two players can share one PC in split screen.
The mod is a `dinput8.dll` proxy plus data built on the player's own machine from their own game archives; it
uses the game's own network code, not a second one. A bot has run a host and a client through all 24 levels, with the
level changes, for 0.1.0, 0.1.1 and 0.1.3 (the last full run); 0.1.4 was checked in lab sessions on test PCs and by
offline tests (see QA), and nobody has played it by hand yet. A whole campaign played by people is still to come, and
sound has only been checked in logs, not by ear. Releases come with a one-file setup exe or a zip, and the "TimeShift Co-op" shortcut
installs a newer release from GitHub before each start ([releases](https://github.com/dolbaras/timeshift-coop/releases)).
The newest release is [0.1.4](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.4) (2026-11-09).

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

Added after 0.1.1 (release notes: [0.1.2](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.2),
[0.1.3](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.3),
[0.1.4](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.4)):

- **Walking speed in split screen (0.1.2):** the game smooths the walk through two values that both local players shared:
  player 1 raised them every update and the idle player 2 took them back, so player 1 crept along. Each player has its own
  smoothing now. Measured with two virtual pads on one PC (camera speed): player 1 alone about 4.3 m/s, player 2 alone about
  4.6 m/s, both about 4.6 m/s each; before the fix 2.5 / 0.6 / 4.4 m/s.
- **Level 2 walkway after a reverse (0.1.3, network):** the host's world rewinds but the clients kept the wreck going (platforms
  seen by one player only, kill zones half open). Clients now take the walkway's animation, collision pieces and kill-zone locks
  from the host during and after the rewind; the host also resends its time-power permissions whenever they change, so a client
  that was still loading when the story unlocked reverse is no longer stuck without it. Checked on two game copies on one PC by
  reading the values from both games (same bridge 28 s after a client's reverse). Other levels with rewound machinery are not
  covered yet.
- **Split-screen respawn loop (0.1.3):** a player killed by the walkway's kill zone was put back inside the same zone, died again
  and again, then ended up alive in the water. A player who dies again right after coming back now gets the game's own restart
  from the checkpoint. Checked with virtual pads in the single-player game and in split screen.
- **Player 2's time-power locks (0.1.3):** player 2's permissions now follow player 1's, so the story's locks apply to both.
- **Bug-report collector (0.1.3):** one click makes a zip on the desktop with the logs, the launcher's log and the installer's
  log, with folder paths, player and Steam names and IDs, e-mail and MAC addresses, Windows user and computer name and public
  IPs removed. It uploads nothing. It is the collector shared by our co-op mods.
- **Partner on the HUD radar (0.1.4):** the other players are blue blips on the game's radar, also in each split-screen half.
  Checked with a host and a client on one PC (blips in the right direction with the client 12 m ahead, 10 m to the side and
  20 m behind) and in split screen.
- **Ready step (0.1.4):** the first chapter waits until every player picks READY in the pause menu, using the game's own
  multiplayer warm-up and Ready button without its 30-second timer; nobody can move meanwhile, a player who leaves no longer
  counts, later chapters start at once. Checked with a host and a client (nobody moved until both were ready; a client who left
  ended the wait).
- **Other mods' ASI loaders (0.1.4):** with more than 8 CPU threads the game crashed at start when an Ultimate ASI Loader
  (`winmm.dll`, `ddraw.dll` or `d3d9.dll` in `bin`) ran before the co-op DLL: the loader writes its own changes back at the
  game's first call and undid the CPU fix. The start-up fixes now survive such a loader. The installer keeps an Ultimate ASI
  Loader that is already `bin\dinput8.dll` running as `bin\winmm.dll` (its plugins stay on), keeps any other mod's
  `dinput8.dll` in a backup and puts it back on uninstall. Checked on a 12-thread PC with Ultimate ASI Loader 10.0.2 and a test
  plugin: the game reaches its menu, the plugin loads, the CPU fix holds, uninstall restores every file.
- **Texts:** the installer, the shortcut's update messages and the setup exe are in all nine languages of the game (Polish,
  Czech and Hungarian added); the controls screen's pad page names the stick directions with the game's own words.
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
- 0.1.2 and 0.1.3: walking speed measured with two virtual pads; the walkway checked in the single-player game and in split
  screen with virtual pads, and over the network by reading values from host and client; the bot's 24-level run on 0.1.3
  (client followed every level change, no crash); installer and self-update from 0.1.2 on a clean game copy.
- 0.1.4: offline tests all pass (texts in nine languages, radar, ready room, ASI-loader stand-ins, installer and update from
  0.1.3); a host and a client on a test PC (Co-op menu, ready room, radar, a level exit into the next chapter); a PC with 12 CPU
  threads (the ASI-loader cases and the controls screen). These are scripted checks and lab sessions, not a playthrough.
- Per-level results are in `docs/campaign-audit.md` in the mod repository.

## How to play

Download `timeshift-coop-setup-<version>.exe` (or the zip and `Install Co-op.bat`) from the
[releases](https://github.com/dolbaras/timeshift-coop/releases), start the game with the "TimeShift Co-op" shortcut, and use
Co-op in the main menu to host a chapter or join by LAN list, Internet list or address. Every player needs the same version.

## Known issues

- Soldiers' voices and gibbing sounds are heard only by the host.
- Joining over a very bad link (around 30 % loss) can need a second click on Join.
- Ready step (0.1.4): a split-screen player 2 in a network game is not held in the ready room and does not take part in the
  ready check; the Tab table during the ready room was not checked.
- ASI loaders (0.1.4): tested with Ultimate ASI Loader 10.0.2 and a test plugin only; a loader renamed to another DLL name was
  not tried.
- After the host's rewind, clients take the rewound machinery's state from the host on level 2 (the walkway) only.
- In split screen the story's zones react to player 1 only, so the RB auto-power on the level 2 walkway reverses time only
  while player 1 stands on it; the reverse button itself works for both players.
- Split screen on a client is basic: its second player's pick-ups work, its time power and kills over the network are not
  proven. "Show both views" (`BothViews=1`) was tried on Intel graphics only.
- Not yet tested: the Internet between two homes, a real Steam install and the Program Files (x86) permission prompt,
  mouse clicks on Accept and Continue, a change of resolution, sound by ear.
- The setup exe is not code-signed, so Windows SmartScreen can warn.
- The client log shows about 150 harmless "unknown object name" lines per run on one level.

## Next steps

- A whole campaign played by people (nobody has played 0.1.4 by hand), and the Internet between two homes.
- Hold a split-screen player 2 in the ready room of a network game; check the Tab table there.
- Rewound machinery on levels other than level 2.
- Soldiers' voices and gibbing sounds on clients.
- A real Steam installation and the Program Files (x86) permission path.
- Sound judged by ear; split screen on a client with its own time power.
