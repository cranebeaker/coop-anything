# Spider-Man Remastered LAN co-op (work in progress)

Two players on the same LAN see each other in the city: your partner appears as a second
Spider-Man with their position and animation. Built for **Steam build 23986256 only**. Any other
build is detected and the mod does nothing.

This is an early prototype. Each player is still in their own single-player game, so missions,
enemies and the world aren't shared. Only the partner's Spider-Man is.

## Install (each player)
1. Back up your saves: `Documents\Marvel's Spider-Man Remastered` and
   `%APPDATA%\Insomniac Games\Marvel's Spider-Man Remastered`.
2. Copy `winmm.dll` and `winmm_sys.dll` (a copy of `C:\Windows\System32\winmm.dll`) into the game
   folder, next to `Spider-Man.exe`.
3. Allow `Spider-Man.exe` through Windows Firewall on private networks. The mod uses the game's LAN
   ports plus UDP 59010.

To uninstall, delete `winmm.dll` and `winmm_sys.dll` from the game folder.

## Play
1. **Host:** start the game and press **F7 on the title screen**, then continue your save.
2. **Partner:** start the game, continue into the city, then press **F9**. Their game finds the host
   on the LAN, joins, and warps next to the host.
3. **F8** warps you next to your partner again. **F6** writes a status line to the log.

Keys only work in the focused game window. Log: `coop_log_<pid>.txt` in the game folder.

## Known limits
- The partner's suit loads in its default style, so suit options (such as variants) may not match.
- If the partner disconnects, their Spider-Man is parked out of sight. It isn't destroyed.
- Pose updates are about 15 KB, sent up to 30 times a second (around 450 KB/s each way). That's fine
  on a LAN, but this isn't meant for internet play.
- Tested on one PC with two copies of the game. It hasn't been tested on two PCs yet.

## Build
`build.bat` builds `build\winmm.dll`. `build.bat research` also compiles the reverse-engineering
tools in `src/research.inc`: memory diffs, part dumps, the watchpoint and the partner-suit
tests (`preload`, `pstrip`, `pparts`). How everything was found: [../NOTES.md](../NOTES.md).

## Test tools
`tools/` holds the scripts used to test on one PC with two copies of the game. Set `COOP_GAME_DIR`
to a **copy** of the game folder (never the real install) and `COOP_BACKUP` to a folder of save
backups.
- `start_instance.ps1`, `drive.ps1`, `click_play.ps1`: start an instance and send it keys or clicks.
- `player_test.ps1`: two instances using only F7/F9, then screenshots of both.
- `coop_test.ps1`, `puppet_test.ps1`: the older step-by-step runs over the command file.
- `cmd.sh <pid> <command>`: write one command to `coop_cmd_<pid>.txt`. `shot.sh`: window screenshot.
- `click.ps1`: a real mouse click in the game window (its menus ignore posted clicks).
- `restore_saves.sh <stamp> <tag>`: restore saves after a test and check them by hash.
