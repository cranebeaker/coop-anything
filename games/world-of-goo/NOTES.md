# World of Goo: working notes

Goal: local split-screen co-op for 2 players in one running copy of the game. Player 1 uses
the mouse, player 2 a gamepad. Each player has their own half of the screen and their own camera.
The ball pool is shared, either player can catch the time bug (undo of the last move, whoever made it), and player 1
drives menus and the world map. Online play over Steam may come later on the same mod (player 2
fed from the network instead of a pad).

## Game identity

| Field | Value |
|---|---|
| Store | Steam, app 22000, depot 22001 |
| Build id | 20824155 |
| Version | 1.5 line (2019 update: 64-bit, SDL2, 2x assets, unencrypted data). Inferred from layout; the exe has no version resource |
| Main exe | `Win64\WorldOfGoo.exe`, 3,653,120 bytes |
| SHA-256 | `C99A90A5EE66CC82B3CAF4AC60EB49BAB4937CFADFDB26677B065089A9FBB379` |
| Engine | 2D Boy in-house C++ framework ("Boy" namespace), SDL2 + OpenGL, steam_api64 |
| OS | Windows 10 Pro 22H1 (10.0.19042), VMware SVGA 3D |

## Observations (checked)

- Data is plain XML: `game\res\levels\<id>\<id>.level / .scene / .resrc`, `game\properties\*.xml`.
- 7 `.level` files carry `cursor1color` … `cursor4color` attributes, and the exe has the
  `cursor%dcolor` format string next to the other `<level>` attribute names. These are
  leftovers of the Wii version's 1–4 player mode.
- RTTI class names in the exe: `GooCursor`, `ControllerBuddy`, `GooController`,
  `LevelController`, `WorldController`, `IslandController`, `Boy::NetworkInterface`,
  `Boy::NullNetworkInterface`.
- The only network use found is World of Goo Corporation tower stats (`/wogbuddy2.php`).
- The exe imports no SDL joystick or game-controller functions, but the shipped `SDL2.dll`
  exports them (`SDL_GameControllerOpen` …), so the mod can read a pad through the same SDL.
- OpenGL is loaded at run time (no `opengl32.dll` import). Frames end in `SDL_GL_SwapWindow`.
- Saves and config: `%LOCALAPPDATA%\2DBoy\WorldOfGoo\` (`pers3.dat`, `config.ini`), found
  through `SHGetFolderPathW(CSIDL_LOCAL_APPDATA)`. Steam Cloud syncs this folder
  (`steam_autocloud.vdf`).
- Runs in VMware (SVGA 3D) at ~60 fps.

## Mod loading (works)

`SDL2.dll` proxy: the original is renamed to `SDL2_real.dll`; the proxy forwards all 656
exports through `#pragma comment(linker, "/EXPORT:x=SDL2_real.x")` (a `.def` file with
`x=SDL2_real.x` lines did not produce forwarders with MSVC 14.51: every name failed with
LNK2001) and implements its own `SDL_GetPrefPath`, `SDL_GL_SwapWindow`, `SDL_PollEvent`.
It also IAT-patches `SHELL32!SHGetFolderPathW` in the exe.

Test run 2026-10-21: the test copy started from the main menu with the proxy, log showed
`SHGetFolderPathW(0x1c)` redirected to the test profile, a fresh `config.ini` was written there,
and the real save folder kept its old timestamps.

Testing a copy outside the Steam folder needs `steam_appid.txt` (`22000`) next to the exe,
with Steam running.

## Engine map (Ghidra 12.1.4 headless, exe PE timestamp 0x691ACF5C, SizeOfImage 0x398000)

Addresses are RVAs (image base 0x140000000). Function names are Ghidra's `FUN_` names.

| What | Where | Notes |
|---|---|---|
| App/platform singleton | global `0x3507e8` (returned by `0x972a0`) | vtable `+0x70` graphics (null during the splash), `+0x88` getMouse(i) |
| Mouse array | app `+0x78`, `Boy::Mouse*[4]` | created in `0xaaf00` (SDL2 platform init); all 4 get vfunc `+0x18(false)`, then mouse 0 `+0x18(true)` |
| `Boy::Mouse` (size 0x88, base `Boy::Controller`) | ctor `0x9c000(this, index)` | `+0x50/+0x54` x/y (float), `+0x5c` index, `+0x60` enabled, `+0x61` has position, `+0x62+b` button b down |
| Mouse move | `0x9c470(Mouse*, float x, float y)` | bounds check against graphics size, notifies listeners |
| Button down / up | `0x9c200(Mouse*, int button, int clicks)` / `0x9c630(Mouse*, int button)` | button 0 = left |
| SDL mouse events → mouse 0 | `0xaa9b0` | only mouse 0 is ever fed by the platform |
| `GooCursor` ×4 | game object `+0x2e0`, created in `0x58400`, ctor `0x25e10(this, index)` | index at `+0x18`; draw `0x261a0` |
| Per-level cursor colours | level `+0x2ec + i*4`, set in level loader `0x39b00` | defaults: 0 white `0xffffffff`, 1 orange `0xffff7e00`, 2 green `0xff06ff00`; `cursorNcolor` overrides |
| Held ball per player | level `+0x1a8 + i*8` | grab `0x411c0(level, player, ball, flag)`: a ball taken by another player is dropped by them first |

## Second player works in the engine (checked 2026-10-21)

The Wii multi-cursor code is alive in the PC build. Enabling `Mouse[1]` (vfuncs `+0x18` and
`+0x08` with `true`) and feeding it `move` / `down` / `up` every frame from the
`SDL_GL_SwapWindow` hook is enough:

- an orange second cursor with a trail is drawn in menus and levels;
- the ball under it is highlighted orange (player 1's hover stays white);
- player 2 picked up a ball in level 1 (Going Up), the strand previews to the tower showed,
  and on release the ball joined the tower (screenshots, run of 18:52–18:55).

Mouse coordinates are game coordinates (about 1600×900 for a 1405×803 fullscreen window).
Calling mouse move during the splash crashes at `WorldOfGoo.exe+0x9c4a6` (graphics is still
null), so the mod waits until app vfunc `+0x70` returns non-null.

Test channel: `wogcoop_cmd.txt` next to the exe with `x y button` drives player 2 (used by bots).

## Player 2 gamepad (checked 2026-10-21, DualSense over USB passthrough in VMware)

- The game never reads pads, but `SDL_Init(0x221)` includes the joystick subsystem; the mod adds
  `SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)` and reads the first game controller each frame.
- The shipped SDL is **2.0.9**, older than SDL's DualSense support (2.0.14): the pad appears as a
  DirectInput joystick, GUID `030000004c050000e60c000000000000`, with no mapping. The mod adds a
  gamecontrollerdb-style mapping for it (and for DualSense Edge, PID 0DF2; Edge not tested).
- Controls: left stick moves player 2's cursor (deadzone 0.22, quadratic curve, 1.1 screen widths
  per second at full tilt), Cross (A) or R2 holds a ball.
- Owner test: the cursor followed the left stick, a ball was dragged with Cross and joined the
  tower (mod log `hold=1`/`hold=0` and screenshot), mouse player 1 at the same time.
- Once, touching the right stick sent the game window behind the desktop (the game kept running;
  no crash event). Not reproduced in a 2-minute recording of the foreground window. The mod now
  sets `SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0`. Cause unknown; Steam Input or VMware are suspects.
- Not checked yet: the pad when the game is started from Steam (Steam Input may wrap the pad).

## Split screen (works, checked 2026-10-21 with bot input on level 1)

Render path: main loop `0xae960` calls `beginFrame 0xbd150` → `Wog::draw` (vtable slot 0x40,
`0x89860`) → `WogRenderer::draw 0x933c0` → `endFrame 0xbdfc0` (blits the FBO to the window and
calls `SDL_GL_SwapWindow`). In a level (`Wog+0x10` state, mode `+0x18` == 3) `WogRenderer::draw`
calls the world draw `0x91580(renderer, gfx, ?, level, ...)` from `0x93618`; it runs
`Scene::draw 0x99420` per depth layer with the level camera `level+0x178 → scene+0x28`. UI and
cursors are drawn after it. Graphics (`Boy::SDL2Graphics`, vtable `0x2b8768`): slot 0x138 width,
0x140 height; slot 0x130 (clip rect) is an empty `ret` on PC. GL entry points come from
`SDL_GL_GetProcAddress` (glViewport global `0x368538`). The renderer is shader based: beginFrame
builds the projection from the width/height getters and uploads it once.

`Boy::Camera` (0x110 bytes, perspective): position `+0x18/+0x1c`, distance (zoom) `+0x78`,
fov `+0xb8`, tan(fov/2) `+0xbc`. Screen→world on z=0 (`0x9b990`):
`world = cam + z * (screen − size/2) / k`, y flipped, `k = H / (2·tan(fov/2))`.
Scene bounds from the `.scene` file: `scene+0xe8` minx, `+0xec` maxx, `+0xf0` miny, `+0xf4` maxy.

How the mod splits the screen (all in the SDL2 proxy):

1. Graphics width getter (vtable slot 0x138) returns half the width while a level is up, except
   inside endFrame, which uses it to fit the FBO into the window. With that, projection, mouse
   mapping, edge panning and UI layout all work for a half-width screen.
2. The `call`s to beginFrame/endFrame in the main loop (`0xaea62`, `0xaea93`) and to the world
   draw (`0x93618`) are redirected to hooks. After beginFrame the GL viewport is the left half.
   The world draw runs twice: left half with the level camera, right half with player 2's
   camera (position and zoom swapped into the camera object for that call only).
3. Player 1's SDL mouse events are clamped to the left half of the window and stretched.
4. Player 2's cursor lives in the right half; the mod maps it right half → world (camera 2) →
   screen of camera 1, and feeds that to mouse 1, so hover, grab, strands and drop use one world.
   The four bounds checks in mouse move (`jb` at `0x9c4d5/0x9c4ed/0x9c506/0x9c51e`) are NOP'd,
   because that point can be far off player 1's screen.
5. Player 2's cursor draws from a 24-point history in mouse 1 coordinates (`cursor+0x28 +
   i*0x10`); the cursor draw hook (`GooCursor` vtable `0x2b0d60` slot 0x18) re-projects it into
   the right half for the draw.
6. Camera 2 follows camera 1 until player 2 pans (right stick, or cursor within 6% of the edge of
   the right half); zoom always follows camera 1; camera 2 is clamped to the scene bounds.

Checked by screenshots: both halves with correct aspect; linked cameras show the same picture and
player 2's hover highlight sits on the same ball in both; after unlinking, camera 2 panned right
and stopped at maxx; player 2 picked up a ball in its own half and it showed in both halves at the
same world spot; player 1's mouse stays in the left half and edge panning still moves camera 1.

Known issues: the retry button is drawn in both halves (part of the world draw); the UI (ball
counter, menu) is drawn in the left half only; each player's cursor is shown only in their own
half; menus and the world map stay full screen.

## Prior work

- Nothing for World of Goo in `DIRECTORY.md` / `data/directory.yaml` (2026-10-21).
- GitHub issues and PRs: not checked (no `gh` on this machine).
- No PC co-op mod found on the web. The Wii version had 1–4 player local co-op with a cursor per
  Wii Remote and drop-in ([Co-Optimus](https://www.co-optimus.com/game/514/wiiware/world-of-goo.html),
  [Nintendo Life](https://www.nintendolife.com/reviews/2008/10/world_of_goo)). Only player 1 moved
  the camera there (single user review, unverified).
- Mod installer for 1.5: [goofans/globin](https://github.com/goofans/globin).

## Unknown

- Whether the game supports Steam Remote Play Together (Steam store page did not load).
- Physics: own `PhysBoy` namespace (`PhysBoy::Body` RTTI); internals not read yet.
- Camera and level render path: not mapped yet.
- Whether the time bug, menus and level exit react to player 2's clicks.

## Settings and packaging (2026-10-21)

- Owner playtest of split screen with mouse + DualSense: works, no crashes.
- `wogcoop.ini` next to the exe (default written on first start), re-read every 60 frames:
  `[screen] split, host_side`, `[cursors] p1_color, p2_color` (written to the mode
  controller's colour table `+0x2ec`), `[gamepad] pad_index, deadzone, cursor_speed,
  camera_speed, edge_margin, swap_sticks, invert_camera_y, grab`, `[saves] separate`
  (start only), `[debug] log`. Checked live: host_side right (mouse in the right half,
  player 2 left), p2_color 00C0FF (cursor and hover highlight), split 0 ↔ 1.
- Separate saves are now opt-in (`[saves] separate = 1`); the test copy uses it.
- Package: `SDL2.dll`, `install.bat`, `uninstall.bat`, `README.txt`. Batch files must be
  ASCII with CRLF (LF line endings and UTF-8 text broke `cmd`).

## Next

1. Time bug, level exit, pause menu and OCD flags with two players.
2. Show each player's cursor in the other half; UI placement.
