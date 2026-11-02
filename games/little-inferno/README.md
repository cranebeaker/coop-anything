# Little Inferno (2012)

| | |
|---|---|
| Engine | Tomorrow Corporation in-house C++ engine, Direct3D 11 (D3D9 fallback), Box2D; data in paks inside the exe |
| Versions tested | Steam build 10124709, version 2.0.3 (Ho Ho Holiday update, 2022), `Little Inferno.exe` SHA-256 `B61315CC90C508D7403C2DF1E12D1A4082A78C42CF56C337EAD458EC417FDCE4`, 32-bit |
| Projects | [Little Inferno co-op](https://github.com/dobrodeetel/little-inferno-coop) (winmm.dll proxy, local co-op, release v1.0.0), directory entry "Little Inferno co-op" |
| QA level | community-reports (owner played with mouse + DualSense; scripted two-pointer checks with screenshots; no automated suite) |
| Last updated | 2026-10-22 |

## Where things stand

Two players can play at one fireplace on one PC: player 1 with the mouse, player 2 with a
gamepad, both grabbing, throwing and burning at once, with shared coins and save. Checked
through the tutorial and first letters; the rest of the game is untested.

## Existing work

- No PC co-op mod found (web search, 2026-10-22; `DIRECTORY.md` had no entry).
- The Nintendo Switch edition (2017) has official two-player local co-op: the second player
  does everything the first can (menus, catalog, fires), one pointer per Joy-Con
  ([Tomorrow Corporation](https://tomorrowcorporation.com/posts/grab-a-friend-switch-editions-will-have-local-multiplayer),
  [Nintendo Life](https://nintendolife.com/reviews/switch-eshop/little_inferno)).
- Pak tools: [liTools](https://github.com/meh2481/liTools) (unpack/repack, not co-op).

## How the game works (for co-op)

- One 32-bit exe; game data (`resource.pak`, `embed.pak`, `frontend.pak`) sits in its `.rsrc`.
  Static imports include WINMM (used by the mod as its entry point); `d3d11.dll` is loaded by
  name. Saves in `%APPDATA%\Little Inferno\` (`profiles.bin`, `settings.txt`, Steam Cloud).
- Input is a table of 22 float channels filled once per tick. The mouse is polled
  (`GetCursorPos`, `GetAsyncKeyState`), not read from window messages.
- **The engine already handles two pointers.** The pointer state used by the fireplace,
  the item tray, letters and buttons has slots for pointer 0 and pointer 1 (a touch / Joy-Con
  leftover). Pointer 1 reads channels 0xe..0x11, which nothing binds on PC. Writing them right
  after the poll gives a working second player with no game-logic changes.
- No netcode. Full map of functions and addresses: [NOTES.md](NOTES.md).

## What we changed

All in a `winmm.dll` proxy; no game files are edited. It refuses to patch any exe other than
the build above (timestamp and image size check).

- **Player 2:** reads a DualSense or DualShock 4 over raw HID (XInput for other pads) and
  writes pointer 1's channels (active, button, x, y) after each input poll.
- **Cursor:** player 2's ring is drawn with Direct3D 11 just before the game's Present
  command (`FUN_004a4630`, command 0x17). Patching the swap chain's vtable instead crashed
  inside the Steam overlay, which hooks Present too.
- **Menus:** the warning screen and the front end (title, mode, slots) are player 1 only, at
  the owner's request; player 2 is switched off while those scenes update.
- **Settings:** `licoop.ini`, re-read every second: on/off, cursor speed, dead zone, size,
  colour, and `saves_dir` to give a test copy its own profile.

## QA

- No automated suite. Player 2 can be driven by a command file next to the exe
  (`licoop_cmd.txt`, `x y button`); PowerShell helpers in the mod repo start a test copy on a
  fresh profile, click through the menus, drag with the real mouse and take screenshots.
- 2026-10-22, fresh profile, results checked by screenshot: player 2 burned the tutorial
  sign, opened and paged the first letter, pulled an item from the tray; player 2 held an item
  while player 1 pulled another; both made fire at the same moment; coins from player 2's burn
  landed in the shared counter; the front end ignored player 2.
- Real DualSense over USB (VMware passthrough) is detected; the owner then played on the main
  install with mouse + DualSense and reported it working.
- `install.bat` / `uninstall.bat` checked on a copy of the game folder.

## How to play

1. Download `LittleInferno-Coop.zip` from the
   [v1.0.0 release](https://github.com/dobrodeetel/little-inferno-coop/releases/tag/v1.0.0),
   close the game and run `install.bat` (or `install.bat "<game folder>"`; Steam's folder is
   `Little Inferno Beta`).
2. Start the game, plug in the pad. Player 2: left stick moves the cursor, Cross/A or R2 holds
   (grab an item, or fire on empty space).
3. Settings: `licoop.ini` next to `Little Inferno.exe`. Uninstall: `uninstall.bat` or Steam
   "Verify integrity of game files".

## Known issues

- Only the start of the game was checked. Later catalogs, the ending and Ho Ho Holiday mode
  are untested; the catalog has not been opened by player 2 yet.
- Menus outside the fireplace are player 1 only (by choice).
- Not tested with Steam Input enabled for the pad, or with an Xbox pad.

## Next steps

- Play through and confirm the catalog, combos and the ending with two players.
- An automated suite on top of the command file (the console's `autoplay` AI player and
  `coins`/`stamps` commands exist but the console's hotkey is unknown on the retail build).
- Online play: drive pointer 1 from the network instead of a pad.
