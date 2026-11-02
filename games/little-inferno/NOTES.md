# Little Inferno — working notes

Running log. Observations, derivations and guesses are kept apart.

## 2026-10-22 triage

Game: Steam app 221260, build id 10124709, version resource 2.0.3.0, `Little Inferno.exe`
SHA-256 `B61315CC90C508D7403C2DF1E12D1A4082A78C42CF56C337EAD458EC417FDCE4`, PE32 x86, Windows 10.

### Observed
- Install folder is `steamapps\common\Little Inferno Beta` (not `Little Inferno`).
- One 328 MB exe; game data sits in `.rsrc` (paks, see [liTools](https://github.com/meh2481/liTools)).
- Static imports: WINHTTP, WINMM, steam_api, KERNEL32, USER32, GDI32, SHELL32, IPHLPAPI, WS2_32.
  No d3d, dxgi, xinput, dinput or hid imports. `d3d11.dll` and `d3d9.dll` appear as
  wide strings (loaded by name; `-d3d9` switch selects D3D9).
- Input on PC: window messages + `GetCursorPos`; dragging items uses a Box2D `b2MouseJoint`.
  Tuning names: `MISC_mouseDragForce`, `MISC_pointerFireRadius`, `UI_CATALOG_itemBtnMaxPointerDist`.
- A developer console exists (help strings: `bind`, `coins`, `stamps`, `toggle ai player`,
  `toggleui`, fixed time step, ...). How to open it on the retail build: unknown.
- "Another instance of Little Inferno is already running ... Clicking Yes could cause your
  save data to be lost." — multiple instances are allowed after a prompt.
- Saves: `%APPDATA%\Little Inferno\` (`profiles.bin`, `settings.txt`, `imagecache.bin`).

### Prior work
- `DIRECTORY.md` / `data/directory.yaml`: no Little Inferno entry.
- No PC co-op mod found (web search 2026-10-22).
- The Switch edition (2017) has official 2-player local co-op: the second player does
  everything the first can (menus, catalog, fires), pointer per Joy-Con
  ([Tomorrow Corporation](https://tomorrowcorporation.com/posts/grab-a-friend-switch-editions-will-have-local-multiplayer),
  [Nintendo Life](https://nintendolife.com/reviews/switch-eshop/little_inferno)).
- No player-2 / multi-pointer strings found in the PC exe (unlike World of Goo, which kept
  the Wii four-cursor code).

### Plan (agreed with owner)
One PC, one screen. P1 mouse, P2 gamepad cursor (left stick, Cross/A or R2 grab/click,
R2 on empty space = fire). P2 can do everything. Shared coins, stamps, save.
Hook: `d3d11.dll` proxy next to the exe (changed to `winmm.dll`, see build below: it loads
at startup, so the save path can be redirected for test copies).
Owner, during the build: the menus outside the game can stay player 1 only.

## 2026-10-22 reverse engineering (Ghidra 12.1.4 headless, code-only copy of the exe)

Addresses are Ghidra VAs with image base 0x400000; the exe is ASLR'd (rebase at runtime).

### Observed
- Window proc `FUN_0049e7e0`; mouse is polled, not taken from messages:
  `FUN_0049c830` reads `GetAsyncKeyState(VK_LBUTTON/VK_RBUTTON)` + `GetCursorPos`.
- Input is a table of 22 float channels (`DAT_0083ae30`, 0x14 bytes per channel, two
  frames double-buffered). Bindings are set in `FUN_0049c700` (type 2 = key, 3 = mouse
  axis/button, 4 = other). `FUN_00473e70` polls them once per tick into a local array at
  `[ebp-0x5c]` (call to `FUN_0049c830` at 0x473ec1) and then copies it into the table.
- Channel "on" means |value| >= 0.5 (`DAT_0061968c` = -0.5, `DAT_0061967c` = 0.5).
- **Two pointers already exist in the engine** (multi-touch / Switch Joy-Con leftover).
  `FUN_00461100` builds a pointer state (`buttons pressed`, `down`, `active` bitmasks, then
  x/y pairs) for pointer 0 from channels 8 (active), 9 (button), 0xb/0xc (x/y in 0..1) and
  for pointer 1 from channels 0xe, 0xf, 0x10, 0x11. On PC channels 0xe..0x11 are unbound.
- Pointer helpers: `FUN_00460f60` pressed, `FUN_00460fb0` down, `FUN_00461070` active,
  `FUN_00461090` pos, `FUN_004610b0` pixels, `FUN_00460fd0` through a matrix,
  `FUN_00460ec0/0f00/0f30` clear.
- Fireplace scene update `FUN_00482070`, item tray `FUN_00479ee0` (loops over both
  pointers), generic button `FUN_00408980` (both pointers), front end `FUN_00467330`
  (title/mode/slots; pointer 0 only in places), warning screen `FUN_00419470`.
- Render commands go through `FUN_004a4630` (`switch (*(short*)cmd)`); command 0x17 calls
  `IDXGISwapChain::Present` on the swap chain at `*(void**)0x7134dc + 0x10`.
- Saves path: `FUN_0049ed30`, `SHGetFolderPathW(CSIDL_APPDATA)` + `\Little Inferno`.
- The Steam overlay hooks Present too; patching the swap chain vtable crashed in
  `gameoverlayrenderer.dll` (null call), so the mod hooks the render command instead.

### Derived
- Feeding channels 0xe..0x11 right after the poll gives a second, fully working pointer
  with no game-logic changes: grab, throw, fire, tray, letters, buttons.

## 2026-10-22 build

`winmm.dll` proxy (game imports WINMM statically, so it loads before the game code).
All 193 winmm exports jump to the system DLL. Hooks: poll result (0x473ec6), render
command (0x4a4630), front end + warning (0x467330, 0x419470), IAT `SHGetFolderPathW`
(test copies keep their own profile via `saves_dir=`). Pad: raw HID for DualSense /
DualShock 4, XInput otherwise. P2 ring drawn with D3D11 before Present.

### Checks that ran (scripted, player 2 through `licoop_cmd.txt`, results by screenshot)
- P2 lights fire, burns the tutorial sign, opens and pages the first letter.
- P2 drags an item from the tray into the fireplace.
- P2 holds an item while P1 pulls another from the tray (both held at once).
- P1 and P2 light two fires at the same time; coins from P2's burn go to the shared counter.
- Front end ignores P2 (owner's choice); P1 navigates menus as usual.
- Real DualSense (USB through VMware) opens over HID; cursor shows and stays still at rest.
  Stick and buttons not yet tried by a person.
- install.bat / uninstall.bat on a copy of the game folder.

### Not checked
- Catalog by P2 (locked at that point of the tutorial for P1 too), later chapters, ending,
  Ho Ho Holiday mode, fullscreen, Steam Input enabled, XInput pads.
