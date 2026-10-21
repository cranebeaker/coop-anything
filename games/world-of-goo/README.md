# World of Goo (2008)

| | |
|---|---|
| Engine | 2D Boy in-house C++ framework ("Boy"), SDL2 + OpenGL, own physics (`PhysBoy`) |
| Versions tested | Steam 1.5 (2019 update), build id 20824155, `WorldOfGoo.exe` SHA-256 `C99A90A5EE66CC82B3CAF4AC60EB49BAB4937CFADFDB26677B065089A9FBB379` |
| Projects | [World of Goo co-op](https://github.com/dobrodeetel/world-of-goo-coop) (SDL2 proxy, split screen, release v1.0.0), directory entry "World of Goo co-op" |
| QA level | community-reports (owner playtest, mouse + DualSense, level 1; scripted bot checks; no automated suite) |
| Last updated | 2026-10-21 |

## Where things stand

Two players can play levels together on one PC: player 1 with the mouse, player 2 with a
gamepad, split screen with a camera each, one shared world. Tested on the first level of
chapter 1 only; the rest of the game is untested.

## Existing work

- No PC co-op mod found (GitHub, web search, 2026-10-21; GitHub issues/PRs of this repo not
  checked because `gh` was not installed).
- The Wii version (2008) had 1–4 player local co-op, a cursor per Wii Remote, drop-in
  ([Co-Optimus](https://www.co-optimus.com/game/514/wiiware/world-of-goo.html),
  [Nintendo Life](https://www.nintendolife.com/reviews/2008/10/world_of_goo)).
- PC mod installer for 1.5: [goofans/globin](https://github.com/goofans/globin) (levels, not co-op).

## How the game works (for co-op)

- Closed C++ binary, no scripting; data is plain XML since 1.5 (`.level`, `.scene`,
  `.resrc`). Saves in `%LOCALAPPDATA%\2DBoy\WorldOfGoo\` (Steam Cloud).
- **The Wii multiplayer code is still in the PC build.** The platform layer creates four
  `Boy::Mouse` objects and the game four `GooCursor`s; per-player held ball, hover and
  cursor colours (`cursor1color`…`cursor4color` in levels) all exist. Only mouse 0 is fed
  from SDL. Feeding mouse 1 gives a working second player with no game-logic changes.
- No netcode (only World of Goo Corporation stats over HTTP).
- One perspective camera per level scene; the renderer is shader-based with one projection
  per frame. Full map of functions, offsets and formulas: [NOTES.md](NOTES.md).

## What we changed

All in an `SDL2.dll` proxy (the real one renamed to `SDL2_real.dll`); no game files edited.
It refuses to patch any exe other than the build above.

- **Player 2:** enables mouse 1 and drives it from an SDL game controller. SDL 2.0.9 in
  the game predates DualSense support, so the mod adds a controller mapping for it.
- **Split screen:** in levels the graphics width getter reports half the width (except in
  the end-of-frame blit), the world draw runs twice into the two halves with two cameras,
  player 1's mouse is confined to its half, and player 2's cursor is mapped from its half
  through camera 2 into world and back into camera 1, so hover, grabbing and strands keep
  using one world. Camera 2 follows camera 1 until player 2 moves it, keeps camera 1's zoom
  and stays inside the scene bounds.
- **Settings:** `wogcoop.ini`, re-read while the game runs: split on/off, host side
  left/right, cursor colours, stick dead zone and speeds, grab buttons, separate saves.

## QA

- No automated suite yet. A command file next to the exe (`wogcoop_cmd.txt`, `x y button`)
  drives player 2 for scripted checks; results were checked by screenshots.
- 2026-10-21, level 1 "Going Up": player 2 grabbed a ball and it joined the tower (shared and
  split screen); split halves with correct aspect; camera 2 unlinked, panned and stopped at
  the scene edge; host side swap and cursor colour applied live; owner played with mouse +
  DualSense (USB passthrough in VMware) without crashes.
- Install/uninstall scripts checked on a copy of the game folder.

## How to play

1. Download `WorldOfGoo-Coop.zip` from the
   [v1.0.0 release](https://github.com/dobrodeetel/world-of-goo-coop/releases/tag/v1.0.0), close
   the game and run `install.bat` (or `install.bat "<game folder>"`).
2. Start the game, plug in the pad. Player 2: left stick cursor, Cross/A or R2 hold a ball,
   right stick camera.
3. Settings: `wogcoop.ini` next to `WorldOfGoo.exe`. Uninstall: `uninstall.bat` or Steam
   "Verify integrity of game files".

## Known issues

- Only level 1 was tested. Time bug, level exit, pause menu, OCD and other chapters are unchecked.
- The retry button is drawn in both halves; the ball counter and menu button only in player 1's half.
- Each player's cursor is shown only in their own half.
- Menus and the world map stay full screen and are driven by player 1.
- Once, touching the pad's right stick sent the game window behind the desktop (not
  reproduced; the mod now keeps the window up on focus loss).
- Not tested when the game is started from Steam with Steam Input enabled.

## Next steps

- Play through the chapters and fix what breaks (time bug, exits, special levels with
  letterboxing or scripted cameras).
- An automated two-player suite on top of the command file.
- Online play: feed mouse 1 from the network instead of a pad (host-authoritative, the
  guest renders snapshots).
