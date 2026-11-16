# TimeShift (2007)

| | |
|---|---|
| Engine | Saber3D (Saber Interactive), Direct3D 9, Havok, FMOD, Bink; its own typed, event-driven level scripts; GameSpy-era client-server multiplayer |
| Versions tested | PC patch 1.2 (menu "Ver 1.02"), 32-bit `bin\TimeShift.exe` SHA-256 `689c63da36cd5902edcad2abe1111d4697a92b12c4e17fe0659b856752641be4`, and the community's pre-patched exe of the same code; Windows 10 and 11. Steam copies are patch 1.0 and need a community 1.2 update package that the player supplies (simulated Steam folder tested, a real Steam install not yet) |
| Projects | [TimeShift Co-op](https://github.com/dolbaras/timeshift-coop) (directory entry "TimeShift Co-op") |
| QA level | automated (a bot drives a host and a client through all 24 campaign levels with the level changes; menus, split screen and the installer are driven by scripts). Nobody has played 0.1.4 to 0.1.6 by hand |
| Last updated | 2026-11-16 (0.1.6) |

## Where things stand

Two to four players can play the story campaign together over a local network (the Internet path through OpenSpy
and direct IP is written but not yet tested between two real homes), and two players can share one PC in split screen.
The mod is a `dinput8.dll` proxy plus data built on the player's own machine from their own game archives; it
uses the game's own network code, not a second one. A bot has run a host and a client through all 24 levels, with the
level changes, for 0.1.0, 0.1.1 and 0.1.3 (the last full run); 0.1.4 to 0.1.6 were checked in lab sessions on test PCs
and by offline tests (see QA), and nobody has played them by hand yet. A whole campaign played by people is still to come, and
sound has only been checked in logs, not by ear. Releases come with a one-file setup exe or a zip, and the "TimeShift Co-op" shortcut
installs a newer release from GitHub before each start ([releases](https://github.com/dolbaras/timeshift-coop/releases)).
The newest release is [0.1.6](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.6). It keeps a player whose
PC is still loading a level in the game (the host used to drop it after 15 silent seconds), makes every level wait for the
players still loading, turns Skip in a cutscene into a vote of every player, hides the partner in cutscenes and lets
split-screen player 2 follow player 1's teleports, movement locks and lifts. 0.1.5 (2026-11-11) changed texts only.

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
[0.1.4](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.4),
[0.1.5](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.5),
[0.1.6](https://github.com/dolbaras/timeshift-coop/releases/tag/v0.1.6)):

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
- **Texts reviewed in all nine languages (0.1.5):** where the game names a thing, the mod uses the game's word in that
  language ("health" instead of "life" in all nine, the game's words for the checkpoint and the controller); one word for
  each thing (the host, the mod) and one form of address per language (German "Sie" as the game says it, Spanish "tú" also
  in the setup exe); wrong meanings fixed (the Italian Regroup messages, the Polish and Czech setup exe message, a
  Hungarian grammar slip). The text test checks the nine languages, the placeholders and that only characters of the
  game's font are used.
- **Bug-report collector and setup exe in 16 languages (0.1.5):** the collector's window (where the zip went, what it
  holds, what was removed, the Issues link, the error messages) follows the game's language (`bin\steamwars.ini`), else
  Windows' language; the report inside the zip stays in English for the developers. The setup exe's messages (a damaged
  download, PowerShell blocked, the installer window not started) follow Windows' language in 16 languages instead of nine.
- **No drop while a level loads (0.1.6):** the host dropped a player whose PC was still loading the next level once the
  load had been silent for more than 15 seconds (the player saw "Server was shut down."), for example on the way into level
  1 after level 19 or at a team restart there; 0.1.5 did it too. The cause is the game's own idle limit for a loading
  player; in a co-op game a loading player may now stay silent for up to 120 seconds. Checked on two test PCs: on the way
  from level 19 to level 1 the loading client was silent for 17.4 s and stayed, and a client held back 25 s on purpose
  stayed through a team restart on level 1 and the change to level 2; with the old limit the same test dropped it after 13.5 s.
- **Every level waits for the players still loading (0.1.6):** after a level change nobody can move until every player has
  loaded the level (60 seconds at most), so nobody misses the opening or the first story events; the screen names the
  players still loading, and a player who leaves while loading ends the wait. The first chapter keeps its Ready step.
- **Skip is a vote (0.1.6):** in a co-op game Skip in a cutscene or film (Start on the pad, Esc on the keyboard) is that
  player's vote; the screen shows who wants to skip and how many have voted, and when every player has voted the scene ends
  on every machine at the same moment (16-47 ms apart in the tests). In split screen player 2 votes with its own Start.
  Alone, the game skips as before.
- **No partner in cutscenes (0.1.6):** the other player's name tag (it was drawn over the opening film of level 1) and the
  other players' bodies are hidden while this machine shows a cutscene.
- **Split screen carries player 2 (0.1.6):** the story's teleports and movement locks of player 1 are repeated on player 2;
  the all-players places (lifts, trains) wait for player 2 with a line in both halves and put it in beside player 1 after
  20 seconds; player 2 is brought to player 1 when it stays more than 40 m away for 10 seconds.
- **Level 19's armoured car (0.1.6):** the ride no longer stays set on the host after a level change (the host kept looking
  for the car in the next level).
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
- 0.1.5: offline tests all pass (the text test, 41 checks: nine languages, placeholders, only characters of the game's
  font, the setup exe's 16 languages; the native tests; the installer test, 51 checks, among them the 0.1.4 release
  installed over an ASI loader and then updated; the setup exe's self-test; the collector's test, 114 checks); the published
  0.1.4 updated itself to 0.1.5 through its own launcher (download checked against its SHA-256) and was uninstalled back to
  the folder as it was; on a test PC the Co-op menu, the Host and Split screen frames and the Video options in Russian,
  Polish, Czech and Hungarian, every letter drawn; `Collect logs.bat` in a Windows 10 console window in Russian, German,
  Polish, Czech and Hungarian (Japanese prints its English lines too where the console's font has no Japanese). The ready
  room was not run live in 0.1.5.
- 0.1.6: offline tests all pass (the text test, 41 checks: the new lines in all nine languages, the placeholders, only
  characters of the game's font; the native tests, among them radar 39, ready room 777 and skip vote 20; the installer test,
  51 checks, among them the published 0.1.5 installed over an ASI loader and then updated; the setup exe's tests, 20 checks
  in a path with Cyrillic letters and spaces plus 7 of the Steam flow on a simulated Steam folder; the setup exe's
  self-test, 34 files; the collector's test, 114 checks). The published 0.1.5 updated itself to 0.1.6 through its own
  launcher in a test folder (download checked against its SHA-256, both shortcuts kept and refreshed, nothing downloaded on
  the next start, uninstalled back to the folder as it was; 10 of 10). With the release DLL, a host and a client on two test
  PCs: the level 19 -> level 1 change that dropped the client in 0.1.5 (the client stayed, and the level waited 18 s for it
  with its name on the screen), a team restart and a level change with the client held back 25 s on purpose (no drop), the
  skip vote on level 1's opening film (one vote kept the film going, the second ended it on both PCs 15 ms after the host's
  order), the ready room on level 2 as the first chapter (no start until both were ready) and two level exits the player's
  way (2 -> 3 -> 4, the host waiting 8-9 s for the loading client). Split screen on one PC (level 7): player 2's skip vote, a
  lift that waited for player 2 and put it in at 20 s, player 1's teleport repeated on player 2. The Co-op menu and its
  Host frame in German, French, Italian, Spanish (in the game for the first time) and English. While the features were made:
  the cutscenes of levels 1, 8, 23 and 24 on both PCs with no partner body or name tag in any frame.
- Not covered by the lab runs: a whole campaign played by people, the Internet between two homes, a real Steam install and
  the Program Files (x86) prompt, sound by ear, the in-game camera scenes of levels 8 and 22, a pad's skip vote on the second
  test PC, the Tab table in the ready room, mouse clicks on Accept and Continue, a change of resolution, an ASI loader under
  another DLL name, `BothViews=1` on a card other than Intel, and split screen on a client beyond its pick-ups.
- Per-level results are in `docs/campaign-audit.md` in the mod repository.

## How to play

Download `timeshift-coop-setup-<version>.exe` (or the zip and `Install Co-op.bat`) from the
[releases](https://github.com/dolbaras/timeshift-coop/releases), start the game with the "TimeShift Co-op" shortcut, and use
Co-op in the main menu to host a chapter or join by LAN list, Internet list or address. Every player needs the same version.

## Known issues

- Soldiers' voices and gibbing sounds are heard only by the host; another player's jump, landing and crouch sounds are not
  heard on the other machines.
- Scripted people in some level openings (for example the kneeling rebels at the start of level 2) stand idle on a
  client's screen while the host sees them play their scene. A fix is in the code but stays off: it does not yet work for
  every kind of scene.
- Joining over a very bad link (around 30 % loss) can need a second click on Join.
- Ready step (0.1.4, still in 0.1.6): a split-screen player 2 in a network game is not held in the ready room and does not
  take part in the ready check.
- In the Host frame the row labels are narrow: in several languages "Max. players" (and the game's own "Server type" /
  "Difficulty") touches its value's arrow (seen in 0.1.5).
- While the host's rewind plays back, the soldiers, loose objects and doors on the clients' screens get no update from the
  host until it ends, and a soldier the rewind brings back to life stays a corpse there. After the rewind, clients take the
  rewound machinery's state from the host on level 2 (the walkway) only.
- In split screen the story's zones, the hint boxes and the automatic time power follow player 1 only, so the RB auto-power
  on the level 2 walkway reverses time only while player 1 stands on it (the reverse button itself works for both players).
  Cutscenes, the pause menu and loading screens are full screen, and each half runs at half of the game's frame rate unless
  "Show both views" (`BothViews=1`) helps on your card.
- Split screen on a client is basic (its second player's pick-ups work).
- A Start or Esc pressed when no scene runs (for example right after one ended) opens the game's pause menu as usual.
- The setup exe is not code-signed, so Windows SmartScreen can warn.
- The client log shows about 150 harmless "unknown object name" lines per run on one level.

## Next steps

- A whole campaign played by people (nobody has played 0.1.4 to 0.1.6 by hand), and the Internet between two homes.
- Hold a split-screen player 2 in the ready room of a network game; check the Tab table there.
- The scripted people of level openings on clients (the rule is in the code, switched off).
- Rewound machinery on levels other than level 2, and the soldiers, objects and doors on clients during the host's rewind.
- Soldiers' voices and gibbing sounds on clients.
- A real Steam installation and the Program Files (x86) permission path.
- Sound judged by ear; split screen on a client with its own time power.
