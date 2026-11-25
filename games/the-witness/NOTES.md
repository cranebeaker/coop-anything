# The Witness: research notes

Status: working prototype (two game copies on one PC, localhost): puppet, solved panels, opened doors, panel power-on, lit cables, restart catch-up and save-reload resync, with an automated suite. Branch `game/the-witness`. Date: 2026-10-22.

## Game identity (tested build)

- Steam app 210970, build id 11370412 (Windows).
- `witness64_d3d11.exe`: PE32+ x64, 6864896 bytes, SHA-256 `8d672d444df6a6df7130f25a517bdab1af92731fe2138dc5b324d519561d55a5`
- `witness_d3d11.exe`: PE32 x86, 5657600 bytes, SHA-256 `6b8f8d9a3d595843335a96dc0d48fb133d0e77868c769673f7e4e6855811b8f6`
- Assets packed in `data-pc.zip` (about 2.7 GB). Ships `steam_api(64).dll`, `openvr_api.dll`, Bink 2, Ansel.
- Engine: `unknown` per `detect_engine` (133 files scanned, no signature match). Wikipedia calls it a proprietary engine.

## Observations (with sources)

- No co-op or multiplayer mod found. The game is single-player. Searched: this repo's directory, `games/`, `gh` issues and PRs (nothing), GitHub repository search ("the witness multiplayer", "witness coop", "witness64_d3d11"), and web search for Witness multiplayer/co-op mod. Not yet searched: Nexus, ModDB, archive.org, Reddit, speedrun/puzzle Discords, non-English communities. So "nothing found" is not "nothing exists".
- [jbzdarkid/witness-randomizer](https://github.com/jbzdarkid/witness-randomizer) (MIT, pushed 2026-01-09): external process, `Source/Memory.cpp` reads and writes the running game with typed `ReadData`/`WriteData` along an offset chain (globals, then an entity table indexed by entity id, then a field offset).
- [jbzdarkid/witness-trainer](https://github.com/jbzdarkid/witness-trainer) (MIT, pushed 2026-09-20): attaches with `OpenProcess`, finds addresses by byte-pattern signature scans (`AddSigScan`), re-scans when the game reloads. README features: noclip, sprint speed, FoV, disable saving, save/load position (X, Y, Z, theta), challenge timer. Says it should work with any Steam release; Windows only.
- Archipelago has a Witness world (`worlds/witness` in ArchipelagoMW/Archipelago); the client is a separate program run next to the game. How it hooks the game: not read yet.
- [ClementSparrow/TheWitnessExplorer](https://github.com/ClementSparrow/TheWitnessExplorer): Python tools to explore assets (no license, last push 2021).
- A 2016 Steam thread asked about screen-share "co-op" only; no mod ([discussion](https://steamcommunity.com/app/210970/discussions/0/458607699611570322/)).

## Memory layout work (2026-10-22, static, read-only on the exe)

Ran the randomizer's and trainer's signature scans over `witness64_d3d11.exe` (SHA-256 above) with a small script (`python -I`, parsing PE sections). Each signature matched exactly once.

| Item | Signature source | Result (RVA from module base) |
|---|---|---|
| GLOBALS | randomizer `Memory.cpp` `LoadPanelOffsets` | `0x62D0A0` |
| camera position | trainer `Trainer.cpp` (+0x10 adjust in trainer) | scan target `0x6304B0` |
| camera angles | trainer `Trainer.cpp` | `0x6303EC` |

Observed in the randomizer source: entities are read as `[GLOBALS + 0x18]` (pointer array), entry `id*8`, then a field offset. For `GLOBALS == 0x62D0A0` it hard-codes `POSITION 0x24`, `ORIENTATION 0x34`, `POWER 0x2A0`, `TARGET 0x2B4`. The same file also hard-codes a second build (`0x5B28C0`) and leaves `0x62B0A0` as TODO, so builds do differ.

Derived, not run: this Steam build matches a layout the randomizer already supports, so its entity field offsets should apply. Nothing was attached to a running game, so none of this is runtime-verified yet.

### Runtime probe (2026-10-22, first attach)

Read-only attach (`PROCESS_VM_READ`, Windows Python 3.11 + ctypes, no writes) to a disposable copy of the install, module base `0x140000000`, game at its menu/start. Observed: camera position and angles read as sane floats; the entity array pointer resolved; entity `0x64` (Tutorial Straight) and `0x1B`, `0x1C` returned plausible positions (z 3.7 to 6.7) and `power` = `1.0, 1.0`. Not yet checked: that the camera values follow the player when moving, and that `power`/solved state changes when a panel is solved.

Side effect: launching the copy rewrote `Launcher.variables` in the real `%APPDATA%\The Witness` (a launcher settings file, not a save). The save folder was backed up first (14 files, hashes identical) to `local/saves-backup/`.

Second probe (after the player started a New Game and went through the first door). Observed: camera position changed from (-36.1, -54.5, 4.4) at the menu to (-183.8, -163.4, 3.8) in the world, about 4.5 m from entity `0x64`, and stayed identical across two samples 5 s apart while the player stood still. So the camera address tracks the player (moving-walk delta not yet sampled). Entity window `+0x290..+0x2D0`: solved/used panel `0x64` has `1.0` in many float slots (`+0x290`..`+0x2AC`) and `+0x2B4` = 216795; unsolved panel `0x65` has `+0x29C` = 1.0, `+0x2B4` = 110 and `+0x2BC` = 111. Unexplained: what 216795 means (not a plausible entity id), and which field is "solved". Needs a before/after sample of one panel being solved.

Third probe (before/after solving one panel). Correction: the panel at the player's feet was entity `0x182` ("Tutorial Bend", 0.6 m from the camera), not `0x65` (about 180 m away). Observed: solving `0x182` changed `+0x290`, `+0x294`, `+0x2A8`, `+0x2AC` to `1.0`, `+0x298` to int `1`, and `+0x2B0` to `1.322` (float; meaning unknown). After the solve its field pattern equals the earlier-solved panel `0x64`.

Static cross-check: the trainer's `solvedTargetOffset` signature (`84 C0 74 19 0F 2F B7`) resolves to `0x294` in this exe. The trainer reads `solved = (float@0x294 == 1.0)`, `hasEverBeenSolved = int@(0x294+0x04) = +0x298`, `state = int@(0x294-0x14) = +0x280`, name at `0x294-0x7C = +0x218`, traced edges at `0x294-0x6C = +0x228`. The runtime change at `+0x294` and `+0x298` matches that. So for build 11370412: solved flag = float at entity `+0x294`, ever-solved = int at `+0x298` (verified once, on one panel). `+0x280` (state) and the traced-edge data (needed to show the same path) were not sampled.

Camera: tracked entering the world (see second probe); smooth walking deltas not sampled.

Save-folder finding: the game keeps saves in `%APPDATA%\The Witness`, whatever folder the exe runs from, so a "test copy" does not isolate saves. Starting a New Game rewrote the latest existing `.witness_campaign` (and its png) there and added new files; the launch also rewrote `Launcher.variables`. Steam Cloud (`steam_autocloud.vdf`) may sync such changes (not checked). Back the folder up before any test launch; the player chose not to restore it here. Lesson for the playbook: check where a game writes saves before launching a copy.

## Entity census and puppet candidates (2026-10-22, read-only, build 11370412)

Launch note: the exe opens a graphics/video options dialog first, and nothing loads until it is confirmed. The command-line flag `-skip_config_dialog` (found in the exe strings, with `-windowed`, `-new_game`, `-no_hud`, `-non_interactive`, `-command`, `-width`, `-height`) skips it; launching `witness64_d3d11.exe -skip_config_dialog -windowed` reached the world with no user input. `powershell.exe` hung from this WSL setup, while Windows Python `subprocess.Popen` launched it fine.

The game's own classes carry no RTTI (only third-party library classes do), so entity types are told apart by the vtable pointer at the start of each entity (an address inside the exe's `.rdata`, `0x5156A8` to `0x521678` from the module base). Observed in the live world: 49,422 non-null entities of 48 types. Largest types: `0x518320` (26,675), `0x5156a8` (5,505), `0x516c88` (4,890), `0x51f2c8` (3,119), `0x517ed8` (2,067).

- Panels are type `0x518ca8` (594 instances). Entities `0x64`, `0x65` and `0x182` are all this type. (`0x65` is a panel too, about 180 m away from the start; the earlier "wrong panel" correction was about which one was at the player's feet.)
- A singleton of type `0x518090`, id `0x1e465`, sits at x,y equal to the camera's and z = 1.8, while the camera z is 3.51: 1.71 above it, i.e. an eye height over feet. Derived, not proven: this is the player entity. It would be the natural entity to read for the local player's position.
- Neighbouring entities of type `0x5183d0` (39 instances, ids from `0x1e467`) sit within a few metres of the player. Unknown what they are.

Not known: which of the 48 types draw a mesh, and whether moving an entity's `+0x24` position moves its rendering (the render transform may live elsewhere). That is the next experiment, and it needs a write to the test copy.

### Does moving an entity's position field move what is drawn? (2026-10-22)

Test on the live copy (the only writes made to the game so far; each restored and read back): wrote entity z (`+0x2C`) +3.0 m, held 10 to 24 s, then restored the original. Observed: the write took and stayed for the whole hold (the game did not overwrite it), the restore read back exactly the original. Entity `0x182` (panel at the player's feet, likely out of frame): the player saw nothing. Entity `0x64` (panel, about 8 m away): a screenshot taken during the displacement looks identical to the one before (same panel at the same pixels in the doorway ahead). Not proven: that the glowing panel in the screenshot is `0x64`.

Screenshot method that needs no installs: Windows Python `ctypes` (find the window with `EnumWindows`, `BitBlt` from the screen DC, write the PNG with `zlib`). Works on this windowed copy.

Derived: panel entities' `+0x24` position is gameplay data, not what places the mesh (the level geometry is likely baked or placed through another transform). So "reuse an existing panel as the puppet by writing its position" does not work. Other types were not tried (48 exist).

Imports of `witness64_d3d11.exe` that are plain files next to it, so they can be replaced with a proxy DLL without installing anything: `AnselSDK64.dll`, `amd_ags_x64.dll`, `openvr_api.dll`, `bink2w64.dll`, `steam_api64.dll`. It also imports `D3D11CreateDevice` (d3d11.dll) and `CreateDXGIFactory` / `CreateDXGIFactory1` (dxgi.dll), so a D3D11 overlay is possible by loading from a proxy and hooking `IDXGISwapChain::Present`. A MinGW-w64 C++ compiler is installed here; an overlay would also need the camera's view/projection (camera position and angles are known; field of view is not yet read).

## Proxy DLL: first code running inside the game (2026-10-22)

Result: a proxy for `AnselSDK64.dll` (MinGW-w64, 9 exports) loads into `witness64_d3d11.exe` in the test copy and logs from inside the process. Verified by: its log line (`proxy loaded: pid=... host=...witness64_d3d11.exe`), the log line `real Ansel DLL loaded` (the game called an Ansel export, so the stubs are exercised), memory climbing to 1.7 GB, and a screenshot of the normal rendered world. The original DLL is kept in the copy as `AnselSDK64_real.dll` (SHA-256 starts `3b0677ae1e01fc17`). The real install was not touched.

What went wrong first (useful to the next agent):
- Export forwarders in the `.def` file (`updateCamera = AnselSDK64_real.updateCamera`) made the loader report "AnselSDK64_real.dll was not found" although the file sat next to the exe. Fixed by making each export a naked stub that calls `LoadLibraryW` on the full path on first use and tail-jumps to the real function.
- That loader error is a Windows "System Error" box owned by `csrss.exe`, not by the game: the game process showed no window, 0 CPU and about 5.8 MB, which looked like a hang. The box stays on screen after the process is killed and shows up in later screenshots. Find it by enumerating top-level windows titled `<exe> - System Error` and close it with `WM_CLOSE`.
- Do no I/O in `DllMain` (loader lock): logging to a `\\wsl.localhost` path runs on a thread started from `DllMain`.
- A full-desktop screenshot (pid 0 in my capture script) is the quick way to see such boxes.

Imports of the exe from `AnselSDK64.dll` were not listed by my first `objdump` filter; all 9 exports are stubbed. Per-frame use of `updateCamera` is derived from the name, not observed.

## Overlay: drawing inside the game's frame (2026-10-22)

Result: from the proxy DLL, a hook on `IDXGISwapChain::Present` draws screen-space markers at world positions, and they land on the right objects. Screenshot evidence: a magenta square on the golden panel in the doorway ahead (entity `0x293`) and one on the stone column (entity `0xc335`), both within a few pixels of where the model predicts (`0x293`: predicted (1055, 689) of 2405x1353, observed on the panel).

How (source in [witness-coop](https://github.com/TommoDotCommo/witness-coop), `proxy/overlay.cpp`):
- Hook: create a dummy D3D11 device and swap chain in the proxy, read `Present` from its vtable (index 8) and overwrite that slot (`VirtualProtect`), since swap chains of the same implementation share the vtable. The game's `Present` then calls our function, which draws and calls the original. The game ran normally afterwards (log: `Present hook called`, resources created, backbuffer 2405x1353, format 28 = `R8G8B8A8_UNORM_SRGB`).
- Drawing: runtime-compiled HLSL (`d3dcompiler_47.dll` via `LoadLibrary`), one instanced quad draw with a constant buffer of rectangles, depth and blend off.
- Camera model (read in-process from module base + `0x6304C0` position, `0x6303EC` angles): `forward = (cos(phi)cos(theta), cos(phi)sin(theta), sin(phi))`, `right = (sin(theta), -cos(theta), 0)`, `up = (-sin(phi)cos(theta), -sin(phi)sin(theta), cos(phi))`, z up. Field of view: vertical 60 degrees fits (about 89 degrees horizontal at 16:9); calibrated from two points in one frame, so treat as +-2 degrees.

Not verified yet: this was calibrated on the title-screen camera (pitch exactly 0). The same addresses followed the player earlier, but the projection has not been checked while walking in the world. `updateCamera` (Ansel) was never called in about 100 s of running, so it does not give a per-frame camera; the camera comes from game memory.

Walking check (same day). Method: launch the copy with `-skip_config_dialog -windowed -new_game` (starts straight in the world), markers on the 16 nearest in-front panels from the randomizer's id list, keypress sent with `SendInput` (scancode `W`, 1.5 s) after bringing the window to the foreground, screenshots before and after. Observed: the camera went from (-201.4, -114.8, 3.73) to (-199.9, -119.5, 3.73) at heading theta = -1.257; the model's forward vector (cos, sin) = (0.306, -0.952) times 4.9 m gives (+1.5, -4.7), the observed delta. A marker sat on the glowing end of the start tunnel before and after the walk, and the far markers spread outward as expected. Two different headings (theta 0.225 on the title screen, -1.257 in the tunnel) both fit, so the angle convention is right in both. Not tested: turning in place with the mouse while watching markers (the math was checked at two headings only), a non-16:9 window, and fullscreen. The overlay has no depth test, so markers show through walls.

## Two players on one PC: pose exchange and puppet (2026-10-22)

Result: two copies of the game, each with the proxy DLL, exchange camera pose over UDP on localhost and each draws the other as a puppet through the `Present` overlay. Steam allowed two game processes of the same app from two folders at once.

Setup: a second install copy (3.9 GB, about 33 s to copy). Per-copy `witness_coop.cfg` (`id`, `listen`, `peer` ports 47001/47002). A thread in the proxy sends `{magic, id, seq, pos[3], angles[2]}` (36 bytes) at 30 Hz and keeps the latest packet from the peer. The overlay draws the remote player as a tall green rectangle from feet (eye minus 1.71 m) to head, plus a yellow dot 0.7 m ahead of the head along the remote heading. Both games were started with `-skip_config_dialog -windowed -new_game` and placed side by side with `SetWindowPos`.

Observed, with a before/after test: A (id 1) was sent a `W` keypress for 1.6 s and walked about 5 m. A's camera read from its memory: (-199.819, -119.816, 3.727). B's log: `recv ... from id 1 ... pos (-199.82,-119.82,3.73)` and `overlay: remote id 1 age 31ms`. An earlier packet (`recv #600`) showed an intermediate position (-200.59, -117.45), so positions update during the walk. B's screenshot shows the green body ahead in the start tunnel at the expected size and place. A drew nothing for B (B is behind A, so the puppet is behind the camera).

Not tested: more than two players, a real network (LAN or internet; localhost only), packet loss or reordering (no sequence check yet), the puppet while the remote player turns or jumps, a puppet that is occluded by walls (no depth test), or the puppet drawn while the local player has the panel-solving cursor up. The body is a flat rectangle, not a model. Pose is the camera (eye) position, so crouch or prone states are not separated.

## Shared panel and door state (2026-10-22)

Goal: when one player solves a panel, the other player's game shows it solved and the gated door opens. Design follows [effect-capture-story-sync](../../playbook/effect-capture-story-sync.md): the machine where it happens detects it, sends numbered effects, the other side applies them directly and never replays the player's input.

### What the game does on a solve (observed, build 11370412)

Entity type names come from the game's own type descriptors: `entity+0x08` points to a descriptor whose `+0x08` is the name (the trainer reads names the same way, and the door function compares `[entity+8]` with a global descriptor). 48 types in the live world, among them `Machine_Panel` (594), `Door` (327), `Power_Cable` (268), `Pressure_Plate` (263), `Multipanel` (41), `Group` (741), `Inanimate` (26,675), `Collision_Volume` (4,890), `Human` (1, the player, entity `0x1E465`). Descriptors in the exe: `Machine_Panel` `0x62E670`, `Door` `0x62DE70` (RVAs).

A natural solve of the first panel (`0x64`), diffed over every entity between a snapshot before, an idle snapshot (noise baseline) and one 4 s after (`wsnap`/`wdiff2` scripts, 49,414 entities, bytes `+0x20..+0x400`): 15 entities changed in a way idle time does not explain.
- Panel `0x64`: `+0x278` `0 -> 1.0`, `+0x280` `0 -> 1` (state "Solved"), `+0x290` `0 -> 1.0`, `+0x294` `0 -> 1.0` (solved flag), `+0x298` `0 -> 1` (ever solved). `+0x2B0` ticks all the time (a clock), so it is not part of a solve.
- The panel's target id at `+0x2B4` is `0x34EDB` (an `Inanimate` group node). The `Door` entity `0x34EDA` got three power words `0 -> 1.0` (`+0x1D0`, `+0x1D4`, `+0x1E4`), an `Inanimate` `0xD05D` the same, and the door's panel and four collision/mesh children moved (the door swings open).
- Panels power 18 different target types (doors, other panels, cables, groups, plates, ...), and 341 of 594 panels have no valid target, so a solve cannot be mirrored by one special case.

### Writing fields does not open a door

Writing exactly those 11 words into the other game (all writes succeeded) changed nothing in the world: the door stayed closed. The door's opening is started by a function call, the one the trainer calls (`_doorOpenStart(door, 1.0)`). In this build it is `base + 0x8E4B0` (found by the trainer's signature `F2 0F 10 41 24 48 89 68 10`, start 15 bytes before the hit; disassembly shows `this` in `rcx`, the amount in `xmm1`, a type check against the Door descriptor and recursion over linked entities). Calling `DoorOpenStart(0x34EDA, 1.0)` inside the other game moved panel `0x64` to exactly the position the real solve produced there, `(-186.232, -158.897)`, with no crash.

A door is "open" when its power word at `+0x1D0` equals `1.0` (10 doors read as open by default in a fresh game, identical in both games, plus `0x34EDA` after the call).

### Implementation (in the proxy DLL, [witness-coop](https://github.com/TommoDotCommo/witness-coop) `proxy/`)

- Detector thread (10 Hz): every `Machine_Panel` solved (`+0x294 == 1.0` and `+0x298 == 1`) and every `Door` open (`+0x1D0 == 1.0`) is compared with its last known value; a change becomes an event.
- Transport: events are numbered per stream epoch, acknowledged cumulatively, resent every 150 ms until acknowledged, applied once and in order, duplicates dropped. A peer restart (new session id in its pose packets) starts a new epoch and a full resend of every solved panel and open door; applying is idempotent.
- Applier (render thread, between frames): a remote panel is written as solved (the five words above); a remote door is opened by calling the game function, after checking the entity's type. Both mark the entity as suppressed for 5 s so the local detector does not send the change back.
- Control channel for development: `witness_cmd.txt` (`door <hexid>`), polled by a thread, run on the render thread.

### Two bugs found by running it (both are in the playbook's warnings)

1. A baseline taken while the world was still loading saw 39 transient "open" doors; the other game applied 30 of them. Fix: nothing is detected, resent or applied until the camera is valid and the panel and door counts are identical on three scans 3 s apart.
2. The echo guard was only a "known" flag, which the detector reset when the door was not yet powered a moment after the call, so 24 doors bounced back. Fix: a per-entity suppression window (5 s) after every remote apply.

### Test

A bot (Windows Python, `SendInput`) plays player A: walks 46 m to the first panel (3 s key presses with a distance check), aims by feedback from the camera angles (0.00255 rad per mouse count horizontally, 0.00237 vertically), clicks to enter the puzzle, moves the cursor onto the start circle (0.218 m left of the panel centre, 3.6 mouse counts per pixel in puzzle mode), presses, drags to the end and clicks to finish. First run, no flags: A logged `local panel 0x64 solved -> event` and `local door 0x34eda opened -> event` (11 events sent in total, 11 acknowledged); B logged `remote panel 0x64 applied as solved` and `remote door 0x34eda opened (call ok)` and emitted nothing back; a state diff over all 594 panels and 327 doors read from both games' memory was identical (1 solved panel, the same 10 open doors).

## The mod repository (2026-10-22)

The prototype now lives in its own public repo, [TommoDotCommo/witness-coop](https://github.com/TommoDotCommo/witness-coop) (MIT, `main`), laid out like mdk2-coop: `proxy/` (C++ source and Makefile), `tools/` (install, restart, bot, state diff, scenario suite, Windows probes), `README.md`, `CLAUDE.md`, `AGENTS.md`. No game files and no third-party data: the debug markers that used the randomizer's panel-id list were removed from the overlay (they were only for calibration), and the detector finds panels and doors by type from the live entity table. The two install copies moved into that repo's ignored `local/`. `tools/install.sh` refuses any exe whose SHA-256 is not the supported build and any `AnselSDK64.dll` that is not the original.

Re-run from the new repo (all four scenarios, same results as before the move): baseline PASS, lost first send PASS (A resent 11), MUTANT no resend diverged as required, MUTANT echo guard off echoed 2 events as required.

A bug the move turned up: in the first baseline run from the new repo, player B logged `state: local door 0x338d5 opened -> event` with nobody having solved anything. `0x338d5` is one of the 10 doors that are open by default; it was powered a few seconds after the counts-only "world stable" gate had passed. Evidence: the log was kept (`evidence_late_door_B.log`, local). Fix: the gate also needs an identical set of solved panels and open doors on three scans, and changes in the first 15 s after the gate are folded into the baseline instead of sent. It happened once in about nine starts and did not recur in the final run, so the fix is in place but not shown to trigger. Lesson added to `playbook/methods.md`.

QA level: the suite meets the repo's definition of `automated` (a scripted harness that drives real game clients and checks shared state, with mutants), so the directory entry is set to `automated`. Its coverage is narrow (one puzzle, localhost), which the entry notes and the write-up say.

## Running several copies and agents at once (2026-10-22)

Measured on one test machine (build 11370412), to plan several agents per game:
- Memory: each running Witness takes about 1.8 GB resident (a pair 3.6 GB); the install is 3.9 GB on disk (2.7 GB `data-pc.zip`, 1.4 GB `data/videos`).
- Speed: the frame counter at `base + 0x63954C` (build 11370412, from the randomizer's Memory.cpp) advanced at 724 fps with the active window and 10 fps minimized. (A first note here said 724 fps for a window "sent to the bottom, not focused"; that was wrong, that window was still the active one. Phase 1 measured it properly, see below.) The game is uncapped, so instances need a frame cap in the proxy.
- Input: the exe imports `RegisterRawInputDevices` and `GetRawInputData` (plus `GetCursorPos`, `SetCursorPos`, `ClipCursor`, `SetCapture`), so keyboard and mouse reach it only through the focused window; the `SendInput` bot needs the desktop to itself.
- Saves: the exe imports `SHGetFolderPathW` (no environment variable is involved), so every copy, and the player's real game, share `%APPDATA%\The Witness`. Redirecting that call inside the proxy would isolate instances and protect the player's saves (built in Phase 1, below).
- Hard links: a hard-linked `data-pc.zip` made the second concurrent instance fail (`error opening bundle file data-pc.zip`, then `fatal_error_data_corruption` in its log), while the first ran fine; everything else (exe, DLLs, `data/videos`) shares fine. So the zip is a private copy per slot; a slot then costs 2.7 GB plus about 40 MB. Whether the proxy could instead relax the file's share mode so one zip serves all instances is untested.
- Windows Firewall prompts once per exe path and a cancelled prompt leaves a Block rule (an older test folder had them); 32 narrow Allow rules (one per slot and game, inbound, from 127.0.0.1 only, only under the test folder) were created with one UAC approval.

Result: a local shared layer (slot leases with a free-RAM guard, identity from the git worktree folder, hard-linked instance pools on NTFS, per-agent ports and run folders, a desktop input lock, path-scoped cleanup), and Witness tools that use it. Test with three agents at once (two Witness worktrees and one MDK2 checkout started in the same second, getting slots 4-5, 2-3 and 0-1): Witness baseline PASS twice, mdk2-coop's own QA 34 of 34, no collisions, nothing left running, base copy intact. Not covered yet: a full Witness suite from two agents (only the baseline ran), more than three agents, MDK2 in more than one agent with its own sweeps.

## Phase 1: isolation, frame cap, shared zip (2026-10-22)

All three are import-table or `Present` hooks in the proxy (`hooks.cpp`, `config.cpp`, `overlay.cpp` in witness-coop), configured in `witness_coop.cfg`.
- **Save folder:** with `save_dir=instance` the hook on `SHGetFolderPathW` returns `<game folder>\appdata` for `CSIDL_APPDATA` (the game asks for csidl 0x1A; log line `hooks: SHGetFolderPathW(csidl 0x1a) -> redirected`). After a full baseline run (new game, solve, autosave) the real `%APPDATA%\The Witness` was byte-identical (76 files, every SHA-256 the same) while each instance's own `appdata\The Witness` held its new save and screenshot. The suite now checks this after every scenario. Before the redirect, a three-agent run had written saves into the real folder.
- **Frame cap:** `cap_fps=30` in `Present` (QueryPerformanceCounter, `Sleep(1)` then spin). Measured with `wfps.py`: the active game ran at 30.0 fps; a game that is not the foreground window ran at 9.8 fps. So the game throttles itself to about 10 fps whenever it is not the foreground window (also minimized). Posting `WM_ACTIVATEAPP`, `WM_ACTIVATE` and `WM_SETFOCUS` to its window every 20 frames (`force_active`, removed again) did not change that; the mechanism turned out to be two bytes the game keeps (found in Phase 2, below). Observed: the game also takes the foreground by itself while loading although it is started with `SW_SHOWNOACTIVATE`; the foreground belonged to the last-started game until a neutral window took it.
- **Shared zip:** the hook log shows `CreateFileA opens data-pc.zip access 0x80000000 share 0`, i.e. the game opens the archive exclusively, which is why a hard link broke the second instance. With `share_zip=1` the hook adds `FILE_SHARE_READ` (`-> FILE_SHARE_READ added` in both instances): two instances ran on one hard-linked 2.7 GB file, both reached a stable world, and the bot solve plus state diff were identical. A test slot now costs about 40 MB instead of 2.7 GB.
- Suite additions: after every scenario, real saves untouched, redirect fired and wrote files, frame cap in force; a new mutant (`cap_fps=0`) that must read as uncapped.

## Phase 2: input, teleport and screenshots without a focused window (2026-10-22)

Result: a bot plays the first puzzle with **no window focused, no `SendInput` and no desktop use**. Two such bots ran at the same time (two agents) beside an MDK2 agent; a monitor sampling the foreground window every 0.2 s over the whole run (1,648 samples) never saw a game own it, the system cursor clip stayed the full screen, and the owner's real saves stayed byte-identical.

How the game takes input (build 11370412; addresses are module-relative; all read from the exe, not guessed):
- **Mouse motion** is raw input: `GetRawInputData` is called for each `WM_INPUT` (real records have device handle 0 and size 48). Our proxy posts `WM_INPUT` to the game window with a private handle (`0xC0DE00xx`) and answers the game's `GetRawInputData` call for that handle with a synthetic record; this works with the window unfocused. Looking: 0.0025 rad per count. Puzzle cursor: 3.6 counts per pixel.
- **Keys and mouse buttons are not raw input.** A real `SendInput` click produces no raw button record at all, and no raw keyboard record. They are ordinary messages (`WM_KEYDOWN`, `WM_CHAR`, `WM_LBUTTONDOWN`, ...) taken by a second message pump (`PeekMessageA` call site `exe+0x345aca`, called each frame) and handled inline by a jump table at `exe+0x345bc0`; the window procedure (`exe+0x37aea0`, found by hooking `RegisterClassExA`) ignores them. So the proxy posts those messages (`lParam` as the OS builds it; clicks at the window centre).
- **Two gates.** (1) The key and button handlers start with `cmp BYTE [input_manager+0xE5C], 0 / je skip`, where `input_manager = [[exe+0x469A060]]`; the byte is cleared when the window loses activation, so injected keys and clicks are dropped until it is 1. Mouse motion does not check it. (2) The main loop does `Sleep(100)` (about 10 fps; found by logging every `Sleep` call and its caller, `exe+0x69845`) when the byte at `exe+0x62D394` and byte `+0x21` of the window object `[exe+0x469A5B0]` are both 0; `+0x21` is the window's "active" state, set by `WM_NCACTIVATE`/`WM_ACTIVATEAPP` with `wParam == 1`. `force_active=1` writes both bytes to 1 every frame in the `Present` hook.
- **Side effect that had to be fixed:** believing it is active, the game moved the REAL cursor to the window centre every frame (`SetCursorPos`) and clipped it to a 2x2 box (`ClipCursor`), locking the real mouse; noticed by comparing `GetCursorPos` and `GetClipCursor` with the window rectangle. With `force_active=1` the proxy now no-ops `ClipCursor`, `SetCursorPos`, `SetCapture`, `ReleaseCapture`, `ShowCursor` and answers `GetCursorPos` with the window centre (a virtual cursor); the suite checks that the clip rectangle stays the full screen.
- **Teleport:** writing the camera position does nothing (the game recomputes it); writing the player entity `0x1E465` position (`+0x24`) moves the player and the camera follows, 1.69 above the feet. `tp x y z yaw pitch` holds it for 12 frames. A real `W` held 3 s walked 9.1 m; the injected `W` did the same once the gate byte was held.
- **Screenshots:** the proxy copies the back buffer in `Present` to a staging texture and writes a top-down BMP (`shot <name>`), also while the window is unfocused or covered; `tools/bmp2png.py` converts it.

Dead ends, so nobody repeats them: posting `WM_ACTIVATEAPP`, `WM_ACTIVATE`, `WM_SETFOCUS` to the window (the handlers have their own preconditions and the flag we need is another byte); faking `GetActiveWindow` (the string is in the exe but the game never resolves it through `GetProcAddress`; it only resolves CRT helpers, NVAPI and XInput); a low-level keyboard hook (the game installs none at start-up); fake raw keyboard records (it registers raw input for the mouse only); a memory diff of `.data` between focused and unfocused (the flags are on the heap).

Two mistakes the suite itself caught: (1) a baseline passed while the bot had done nothing (both games untouched, so "identical"), because the Windows paths in the bot command lost their backslashes in the shell; the suite now requires the bot to report the panel solved and checks `A emitted a panel event`. (2) The same shell problem as with `eval` of exported paths: quote every Windows path. The old `SendInput` bot and the desktop input lock are no longer used by the Witness suite.

## Phase 3: the sync core without the game (T0) (2026-10-22)

Result: the sync logic (events, acks, resend, epochs, stable-world gate, settle window, echo guard, full resend) moved out of `state.cpp`/`coop.cpp` into a pure C++17 library, `proxy/core/sync_core.{h,cpp}` in witness-coop. It sees the game only through a `World` interface (scan, read and apply panel/door state, camera pose), so the hub's fuzzer (`tools/fuzz`, `ITarget`) drives two cores over a lossy link with a fake world (loading phase, doors open by default, a door that powers up late by itself, cascade doors that open 200-800 ms after a solve, a remote door that shows up late). `make -C proxy/core test` (or `python3 proxy/core/tests/run_t0.py`) builds it and runs 500 seeded 150 s sessions clean (about 1,500 restarts, tens of thousands of replayed stale packets), then once per bug twin; all 10 twins are caught (2.7 s in total, no game, no slot):

`no_resend` and `no_checksum` -> not_quiescent/converge, `no_echo_guard` -> echo_sent, `no_dedupe` -> double_apply, `no_epoch_reset` and `apply_out_of_order` -> out_of_order_apply/converge, `no_resync_on_restart` -> converge, `no_gate` -> spurious_event/converge, `no_settle` -> spurious_event, `wrong_ack_epoch` -> not_quiescent.

What the simulator found that the real two-game suite had not:
- **Peer restarts flapped.** Any different session id was treated as a restart, so two packets in flight from an old and a new session made each side reset the other forever. Sessions and epochs now only grow; older ones are ignored. (The production session id is now `time(nullptr)`, which grows on every start.)
- **The settle window swallowed a real solve.** Changes in the first 15 s after the gate were folded into the baseline, which also folded a real panel solve made in that time: never sent. Panels are now never folded; doors are folded except within 5 s after a local solve (the cascade of that solve). Limit, by design and counted in the target (`startup_flip_beside_a_solve`): a door that powers up by itself within 5 s of a local solve looks like that solve's cascade and is sent.
- My own bug: the session was read from the checksum bytes (offset 36, not 32). Only the lossy test saw it.
- **Coverage depends on the session profile matching the protocol's timing.** Seven of ten twins were first not caught because one machine restarted every 1.4 s while the gate needs 9 s or more. The profile now has a game not quit within 45 s of starting, longer sessions, and a player who acts only once the world has loaded and settled; `no_settle` additionally needed the late flip to land after the gate and no solve to delay it. A twin that passes means the test cannot see that protection, not that the protection is fine.

Not covered by T0: the real game's memory layout and timing (the `GameWorld` adapter in `state.cpp`), only the T2 suite checks that. The T2 suite was re-run on the refactored build (see the QA section of the README).

## Derived

- Player position and panel/entity state are reachable from outside the process, and the community already finds them by signature scan, so a mod can be an external or injected process without source. (Confirmed: the mod is an injected proxy DLL.)
- Sig scans tolerate patches better than fixed addresses, but this build's hashes above are the only ones tested.
- Co-op here means: show a second player (puppet) and share progress (solved panels, open doors, power, cables). The prototype treats these as monotonic events, so there is no host and no authority rule; reversible state would need one.

## Guesses

- Puzzles are likely deterministic from the entity table, apart from the random-door and challenge sections.

## Unknowns

- Engine family and whether it has any networking code.
- Whether any script layer exists inside `data-pc.zip`.
- Whether `witness64_d3d11.exe` has debug symbols or a known pdb.
- Whether the GOG/Epic builds differ from the Steam build.
- How the Archipelago client reads the game.
- Power-off switching, cables other than the one tested, `:slider_cable` bridges, plates, lasers, a real network and more than two players: not implemented or not tested.
- Whether any door closes in normal play (the save-reload heuristic would misread it as a load).

## Next

1. Extend sync to the other cable-driving panels, plates, lasers and bridges, one natural solve and state diff each; define authority for reversible state.
2. A real network (direct IP or relay), then lag and loss runs.
3. More puzzles in the suite (so far three tutorial puzzles).
4. Occlusion and a real puppet model.
5. Finish the unchecked searches (Nexus, ModDB, Reddit, Discord).

## Phase 4: sync beyond the first puzzle (2026-10-22)

Each item below is a witness-coop commit with a real-game scenario and a mutant, plus game-free (T0) tests.

### Retain acknowledged events until the game applies them

Implementation: witness-coop commit `bfe1b7e`.

#### Observations

- `SyncCore::onPacket` acknowledges ordered events when queued, before the render thread applies them. Previously `applyRemote` removed events even when the entity was absent or `applyDoorOpen` returned false; the latter also incremented applied counters and installed an echo guard for a refused call.
- Added a deterministic, game-free receiver regression (`proxy/core/tests/apply_retry.cpp` in witness-coop). Before the fix, missing-panel, missing-door and refused-door cases failed; wrong-type rejection passed.
- After the fix, all four cases pass. Each retry case catches a `noApplyRetry` twin. They check ACK-before-application, convergence without another packet, unrelated-door progress, accurate applied counters, and no local echo after success.
- `make -C proxy/core test`: deterministic regressions pass, 500 clean fuzz sessions pass, all 10 existing fuzz twins caught. `make -C proxy`: DLL builds. `git diff --check`: clean.

#### Derived

- Transport reliability alone does not guarantee state application: the receiver must retain responsibility for an acknowledged event until its game adapter succeeds.
- Retries make at most one attempt per queued event per frame, within the existing 1,024-event receive bound. These panel/door changes are monotonic and independent, so a temporarily unavailable entity need not block another entity.

#### Unknowns

- No real-game missing entity or refused call has been induced; these failure modes are simulated at T0.
- First real-game baseline attempt failed during startup: A never logged the overlay patch or networking startup, bot entity lookup returned an unreadable table, and measured 0 fps. B reached the stable-world gate. Save hashes, redirect and cursor checks passed; the suite released its slots. A second fresh baseline attempt passed: one solved panel and ten open doors identical in both games, no echo from B, no resends, 30.0 fps, save hashes/redirect/cursor checks passed, slots released. The first startup failure did not recur; its cause remains unknown.

### Follow-up: real-game restart catch-up

#### Observations

- Reviewed the hub's world-state build order (`playbook/how-coop-mods-work.md`, stage 4), effect capture and desync guidance. Chose the missing real-game join/rejoin check: full-state resend already exists, but only the game-free simulator covered it.
- Added selective `tools/restart.sh B` / `tools/install.sh B`; the surviving A process, config and loaded DLL are preserved. Newly launched processes are registered immediately rather than after both launches finish.
- Added clean and disabled-catch-up mutant scenarios. Preconditions read both worlds from memory and require the original solve to have reached B and all sender events to have been acknowledged. After B restarts with `-new_game`, A must retain its PID and state and B must have a new PID and complete census. The clean case requires the whole solved/open state to match and no B echo; the mutant specifically requires the first panel and its door to be missing on B.
- `bash -n tools/restart.sh tools/install.sh`, Python compilation, DLL build, and T0 (application regressions, 500 clean fuzz sessions, all ten fuzz twins) pass. Real-game `tools/qa.sh 'restart catch-up'`: both scenarios PASS. Clean restored one solved panel and ten open doors, with no echo. Mutant left exactly panel `0x64` and door `0x34eda` missing on B, after the original two events had been acknowledged. Both runs kept real save hashes unchanged, redirected instance saves, left the cursor free and measured A at 30.0 fps. All leased games were released. Implementation: witness-coop `df7b276`.

#### Derived

- Restarting a receiver only after the original events are acknowledged distinguishes snapshot catch-up from retransmission of outstanding deltas.

#### Unknowns

- Real-game first-time late join, same-process save reload and WAN behavior remain untested.

### Follow-up: second tutorial puzzle exposes skipped door zero

#### Observations

- Read Tutorial Bend `0x182` using the panel graph offsets from [witness-randomizer Memory.cpp](https://github.com/jbzdarkid/witness-randomizer/blob/master/Source/Memory.cpp), build branch `GLOBALS == 0x62D0A0`: positions at `+0x3C0`, flags `+0x3C8`, connections `+0x3D0/+0x3D8`, counts `+0x3B4/+0x3B8`. Three nodes: `(0.8,0.25)`, `(0.8,0.75)`, `(0.2,0.75)`, connections `0-1`, `1-2`.
- Standing camera command `tp -182.71 -164.90 3.727 0.303 -0.066` sees the puzzle screen head-on. The actual eye settles at z about 3.896 on the floor. A proxy back-buffer shot shows the start circle at `(1016,640)` in 1920x1080; entering aiming mode initially puts the cursor at `(960,540)`.
- Free cursor placement uses 112 counts right and 200 down at this viewport. A short constrained trace stopped before the end; 40 frames of `(0,-18)` then 40 of `(-18,0)` reaches both segment ends. Clicking finishes the natural solve.
- Memory after the solve: A and B both contain solved panels `0x64` and `0x182`; A has 11 open doors, B 10. The only difference is door ID `0x0`, open only on A. `tools/state_diff.py` exited 1 with exactly that difference.
- Root cause in `proxy/state.cpp`: the entity scan starts at ID 1. The read-only census starts at 0 and finds 327 doors, while the adapter sees 326. `GameEntity(0)`, packet validation and the door-call bridge otherwise accept zero.
- Added `wbot2.py`, shared `wcontrol.py`, read-only `wpanel.py`, and a second-tutorial suite scenario that requires both panels solved plus both specific doors open. A skip-zero mutant must retain the second panel solve on B but lose door zero. The fresh pre-fix clean scenario FAILed with exactly door zero missing on B, while both bots reported solved from memory. The mutant also reproduced that same missing-door state. The scanner fix includes table index zero by default, and `skip_entity_zero=1` restores the old scan for the mutant. Post-fix clean/mutant state checks both PASS: clean has 2 panels and 11 doors on both sides, no B echo; mutant has both panels but lacks only door zero on B. Save hashes, redirect, free cursor and 30.0 fps checks pass; no games/leases remain. The wrapper unexpectedly returned 1 after ALL OK; a repeat with scripts left unchanged returned 0, with both scenarios PASS and all resources released. The earlier status mismatch did not recur; cause unknown. Implementation: witness-coop `cfb67a0`. Shell syntax, Python compilation and whitespace checks pass. DLL build and all existing T0 tests pass.

#### Derived

- Table index zero is valid in this build; zero cannot be used as a table-index sentinel during census. The mod/probes call these normalized table indices entity IDs; the game's stored engine IDs use the separate base described below.
- Correction to the earlier target-type interpretation: this repo's event IDs and probes use entity **table indices**, while the game's target fields hold engine IDs. Read `table_base_id = u32([base+0x62D0A0]+8)`, observed `1`. At `exe+0xB9F00`, the game reads panel `+0x2B4`, subtracts this base ID, indexes `[globals+0x18]`, checks the Door descriptor, and calls `exe+0x8E4B0`. Thus target `0x34EDB` means table index `0x34EDA`, and target `1` means table index zero. The earlier notes' claim that the first target is an Inanimate at index `0x34EDB` was a namespace error. `wpanel.py` now reports both raw target and normalized table index.

#### Unknowns

- Cables and powered-panel replication remain unimplemented. The second tutorial panel is another door gate, so it first exposed this earlier correctness gap.

### Follow-up: panel power-on progression

#### Observations

- A read-only target census found a simple already-powered tutorial panel at table index `0x18AF`, raw target `0x1C` = target table index `0x1B` (Machine_Panel, initially requested and visible power both zero). Its 7-node graph has a middle horizontal route that separates the black and white squares.
- Natural proxy input solve: teleport above the standing point `(-158.745,-69.925,10.0)`, yaw `2.749`, pitch `0.003`; the eye settles at z `6.7143`. Enter aiming, 80 counts left to the start, click, 40 frames of +18 x, click finish. A source solve lights up target `0x1B` on A; B gets the solved source but leaves its target at `(0,0)` power. In-game shots show the second panel lighting on A. Real save hashes unchanged across the research session; slots released.
- A first long teleport at ground height was displaced by collision resolution. Arriving above the ground works; the new bot checks the actual camera position and retries up to three times before failing.
- Disassembly of build 11370412 at `exe+0xC2890` takes a panel pointer, sets requested power `+0x2A4` to `1.0`, handles redraw/randomization and linked doors. Visible power `+0x2A0` follows in the normal game loop. The applier type-checks Machine_Panel and checks the function prologue, then calls this on the render thread.
- Added deterministic power-on core tests; both live sync and restart recovery failed before the new event. After implementation they pass, including dropped first sends, refused calls held past the echo interval, no echo, and ensuring a powered target is not marked solved. Disabled-power, no-apply-retry, no-echo-guard and no-resync twins are caught. Existing application regressions plus 500 clean fuzz sessions and all ten old fuzz twins pass. DLL build passes.
- New event kind 3 shares panel power-on via the reliable queue and full-state snapshots. Power state is part of the stable-world signature and has its own echo guard. The wire magics are v2 (`WTP2`, `WTE2`, `WTA2`) to avoid an older receiver silently skipping the new vocabulary. Both copies must use the updated DLL.
- Real-game `tools/qa.sh 'panel power chain'`: clean and disabled-power mutant both PASS, command exit 0. Clean also restarts B after all original events are acknowledged: both worlds match 2 solved panels, 10 open doors and 238 powered panels. The target remains unsolved, B emits no power echo and applies one power event. The mutant preserves both solved panels and doors but has 237 powered panels on B, missing only `0x1B`. Save hashes, instance save redirect, free cursor and 30.0 fps checks pass; own games and slots released.
- Protocol compatibility fixture: checksummed v1 event and pose are ignored, while matching v2 controls are accepted. All power tests and the existing 500-session fuzz/twin suite pass. Implementation: witness-coop `9799af8`. Python compilation and `git diff --check` pass; final DLL rebuild passes.

#### Derived

- Reading requested power for detection catches the discrete activation while letting each engine animate its visible brightness. The real-game assertion requires both requested and visible power to reach 1.

#### Unknowns

- This adds power-on progression only, matching the existing solved/open progression model. Power-off switching, conflict/authority rules for reversible state, and cable sync remain unimplemented.

### Follow-up: first-time late join

#### Observations

- Added scenarios `late join` and `MUTANT: late join without snapshot` (`tools/suite.py`, `late_join`). Only A is started (`restart.sh A`; `restart.sh` now tolerates a peer with no pid file). The bot solves panel `0x64` on A while B does not exist, then B is started for the first time with `restart.sh B`.
- A runs with `no_resend=1`, so the original events are sent once into the void and are never retransmitted; only the full-state snapshot sent when A first sees B's session can deliver them. The mutant adds `no_resync_on_restart=1`.
- Real-game result (witness-coop, `tools/qa.sh 'late join'`, exit 0): clean B matches A in memory (1 solved panel, 10 open doors, 237 powered, 594 entities) with no echo; mutant B has 0 panels and 9 doors, lacking exactly `0x64` and `0x34eda`. Save, cursor and 30.0 fps checks pass; slots released.

#### Derived

- First contact and restart share one code path (`peerSession_` increases, snapshot flag set), so late join is covered by the same protection; the mutant flag name says "restart" but gates both.

#### Unknowns

- Late join with the default resend behaviour (events resent into a starting B) was not isolated. Same-process reload and WAN untested.

### Follow-up: same-process save reload

#### Observations

- The pause menu is keyboard-drivable through the in-process input (Esc, S, S, Enter = "Load a Game"; the newest save is selected). New `tools/win/wreload.py` does this with no focus or desktop input.
- The game keeps one save file per session and rewrites it in place; opening the pause menu also saves. A first reload therefore loaded the current state and changed nothing. The scenario backs up B's first save (written about a minute after launch, before any progress), restores it after Esc and before the list is read, then loads it. `restart.sh` now empties each instance's own save folder so there is one known save.
- Before the fix (real game, manual then scripted): B reverted to 0 panels / 9 doors and stayed there; A had no reason to resend (no session change, all events ACKed). An earlier load with a stale save also emitted the load's start-up door openings and a power event from B as local events, which A applied (A showed 6 extra open doors).
- Fix in `sync_core.cpp` (`reloadSeen`): a known open door closing, or two or more known panels unsolved (one panel can be redrawn), is read as a load. The core goes back to the stable-world gate, bumps its session (the peer then resends its full state, as for a restart), clears its stream and echo suppressions, and publishes its own full state once stable, so start-up transients are folded into the baseline. Twin: `no_reload_sync=1` / `Flags::noReloadSync`.
- T0: new `proxy/core/tests/reload_sync.cpp` (older save, second reload, progress only in the save, each with the twin caught); it failed before the fix. 500 fuzz sessions and the 10 existing twins pass; DLL builds.
- Real game, `tools/qa.sh 'same-process'`: clean B loses and then regains panel `0x64` and door `0x34eda` (log shows "save reload suspected"), no process restarted, B emitted no events, both worlds identical; mutant B stays without both. Full `tools/qa.sh` afterwards: 17 scenarios ALL OK, exit 0. Save, cursor and 30.0 fps checks pass; slots released.

#### Derived

- A same-process load has no session signal, so progress regression is the only evidence available; the heuristic could misread a real door that closes.

#### Unknowns

- Whether any door in the game closes in normal play (would trigger a spurious re-gate and resync: harmless but wasteful). Session is bumped by 1, so a real restart in the same second as a reload could be ignored by the peer. Loading a different save slot or a save from another session is untested.

### Follow-up: cables

#### Observations

- `Power_Cable` has 268 entities; 167 panels target a cable. All are mazes of 26+ nodes except none reachable by the older bots. Panel `0x293` (5x5 grid, 26 nodes, target cable `0x349`, powered, a plain maze with start flag `0x2` and end flag `0x1`) is 3 m from spawn and has a visible black cable that glows yellow once solved.
- Natural solves of `0x64` and `0x18af` change no cable word in either game. A natural solve of `0x293` changes exactly one cable in the whole world on A: `0x349` `+0xC4` and `+0xC8` go 0 -> 1.0 (two other cables are lit from the start). B stays 0. Other A-vs-B cable differences (`+0x18c`, pointers) are per-process load state, not solve state.
- The game's target dispatcher is `exe+0xC24F0(target id, source entity)`: it switches on the target's type descriptor (Gauge, Pressure_Plate, Machine_Panel -> `0xC2890` then writes the source id to the target's `+0x2C8`, Multipanel, Power_Cable -> `0x175EE0(cable, 0)`, Door -> `0x8EB60`, Light, Laser, Marker and others). `0x175EE0(cable, 0)` lights the cable. The loop over `Power_Cable` children in the panel update at `exe+0xB9E00..` only handles cables tagged `:slider_cable` (sliding bridges), not this chain.
- Panel input: head-on at 3 m (stand = panel position + 3 * (cos rz, sin rz), yaw = rz + pi, rz = 2*atan2(qz, qw) from the panel quaternion at `+0x34..+0x44`, eye z equal to the panel z; a teleport above ceiling height lands elsewhere). The panel spans about 222 px per unit of its 0..1 coordinates, the cursor starts at the screen centre and free motion is 2 counts per pixel. A constrained trace lags at corners, so the bot sends 16-count steps and reads the traced-node count at panel `+0x228` (it counts the start; the end stub does not add to it). Closed loop on that count replaces distance guessing.
- Implementation (witness-coop): event kind 4 (cable lit), protocol v3 magics, `GameCablePowerOnNow` (type and prologue checked, render thread), core detection with the same start-up and cascade gating as panel power, echo guard, retry, snapshots; `Flags::noCableSync` / `no_cable_sync=1`. T0: new `cable_sync.cpp` failed before the change; clean, restart-recovery and four twins pass; 500 fuzz sessions and 10 twins pass; DLL builds. `wbot_maze.py` solves any plain maze by BFS; `wstatedump.py`/`state_diff.py` report lit cables.
- Real game, `tools/qa.sh 'cable maze'`: clean A and B match on solved panels, powered panels (238) and lit cables (3), B emits no cable event, and a fresh B recovers the lit cable after the events were ACKed; the mutant leaves exactly `0x349` dark on B. Save, cursor and 30.0 fps checks pass. Full `tools/qa.sh` afterwards: 19 scenarios ALL OK, exit 0.

#### Derived

- Calling the dispatcher on the solved panel would replicate every target type, including the `+0x2C8` source back-link our panel power path does not set; only door and cable targets are covered by dedicated events today.

#### Unknowns

- The other cable-driving panels, `:slider_cable`, cable power-off, and what `+0x2C8` and the `+0x37c` flags do visually. Whether the cable glow also needs the source panel's trace shown on B. Screenshots of B at the cable were not compared (B's streaming did not load that area after a teleport); the assertion is on memory.

### Random packet loss (T2)

- witness-coop `proxy/coop.cpp`: config keys `drop_pct` and `drop_seed` (deterministic LCG per seed) drop outgoing datagrams in the send hook only.
- `tools/suite.py`: scenario "30% random packet loss" (A seed 7, B seed 11, must be identical) and "MUTANT: no resend + 30% random loss" (must diverge).
- Ran: `make -C proxy/core test` (ALL OK), `make -C proxy` (builds), `tools/qa.sh random`: both PASS (loss baseline identical, A resent 21; mutant: 2 differences).
- Caveat: the mutant's divergence depends on which datagrams the seed drops. Deterministic per seed, but packet timing still varies, so this is one run. Flakiness over repeated runs: `unknown`.

### Second panel for the T2 bot (resolved)

An early attempt to script the second tutorial panel (`0x182`) stalled: with the panel's facing and the start circle unknown, four teleported views showed no puzzle screen. Reading the panel graph (positions, flags and connections at `+0x3B4..+0x3D8`, as the randomizer does) and a back-buffer screenshot of the head-on view fixed this; the second-puzzle section above (Follow-up: second tutorial puzzle) records the working bot and the door-zero bug it exposed.


### Follow-up: looking for a second cable maze (2026-10-24)

#### Observations

- A panel's target is at `+0x2B4` as an engine id: table index = value - `table_base_id` (1 in this build). Reading it for all panels and keeping those whose target is a `Power_Cable`, with exactly one start flag (`0x2` without `0x1`) and one end flag, gives plain-maze candidates; none of the low 16 flag bits are decoration for the best ones. Nearest to spawn: `0x293` (cable `0x349`, the one already covered), `0xc335` (253 nodes), `0x295` and `0x3505` (50 nodes), `0x2c2` (122 nodes). Powered and plain farther away: `0x290` (17 nodes), `0x28a69`, `0x17c31`.
- `0x9b8` (two starts, two ends) and `0x143` (sixteen end nodes) are not plain mazes; `wbot_maze.py` refuses them.
- A `tp` to the standing point of `0x3505` (about 10 m from `0x293`, z from 7.7 up to 16) and of `0x290` leaves the player at the spawn point `(-131.8, -164.2, 7.42)` every time, although the yaw is applied; `0x293`'s standing point works. A bot that only teleports cannot reach these panels.

#### Derived

- Cable sync is verified on one panel only. A second cable-driving panel needs a walking bot (or a teleport that survives the area check); the detection and apply code does not depend on the panel.

#### Unknowns

- Why the teleport is rejected there (collision resolution, a region check, or unloaded geometry). Whether the other candidates solve and light their cables as `0x293` does.

### Follow-up: second cable maze, source back-link, other target types, cable power-off, faster QA (2026-10-24)

#### Observations

- No walking bot was needed. Solving maze `0x293` lights its cable `0x349`, and that powers maze `0x295` (4 m away, black until then; no panel targets it); solving `0x295` lights cable `0x45c` and powers the 122-node maze `0x2c2`. The existing maze bot solves `0x295` unchanged. New T2 scenario `cable chain` (both cables, both solves, powered panels and a fresh B match; mutant `no_cable_sync` leaves both cables dark on B). The earlier "teleport rejected" cases (`0x3505`, `0x290`) stay unexplained: the player returns to the spawn point `(-131.8, -164.2, 7.42)`.
- A panel's solve also writes the source engine id to the target's `+0x2C8` (the dispatcher does `[target+0x2C8] = [source+0x10]`). After the tutorial power chain, A has `0x18b0` there (panel `0x18af` + base id 1) and B had 0. The applier now finds the source (the panel whose `+0x2B4` names the target, a solved one first) and writes it. T2 `panel power chain` compares it; the mutant `no_power_link` leaves B without it.
- Panel target types (all 594 panels): panel 224, cable 167, door 84, none 89, laser 10, pressure plate 9, gauge 6, and one each of pylon, double ramp, force bridge, record player, light. The dispatcher `exe+0xC24F0(engine id, source entity)` handles each. For panel `0x32f5` (target laser `0x32f9`) a native-style solve on A and B's call of the dispatcher change the same words of the laser: `+0x12C` 0 -> -1.0, `+0xD4` 7 -> 1 (this one keeps advancing, 1 -> 6, so it is not compared) and a flag at `+0xAC`. A remote solve now also calls the dispatcher for targets that are not panel, cable or door. A test command `solve <id>` (fields plus dispatcher) is used because those panels are not reachable. T2 `bot-free effects` checks the laser; the mutant `no_target_dispatch` leaves it off on B. Only the laser was checked; the other types are `unknown`.
- Cable power-off: `exe+0x175E50(cable)` sets `+0xC8` to 0 (the visible `+0xC4` fades). The game calls it, with the power-on `0x175EE0`, from the update of `Double_Ramp` entities (named cable pairs switched by a direction flag). Natural tracing of `0x293` without finishing produced no cable or panel power-off. Protocol v4 adds event kind 5 (cable dark), a snapshot of default-lit cables that were switched off, a rule that a peer's snapshot never relights a cable this player switched off, and a tie-break (lower player id wins) when both switch the same cable at once. T0 `cable_off.cpp` has 4 positive cases and 5 twins; T2 `bot-free effects` switches default-lit cable `0x18458` off with a test command (`cableoff`) and has mutants for no power-off sync and no snapshot. Two snapshot twins were needed because the reassert path also repairs a missing snapshot.
- Some doors cycle by themselves (ids `0x2fe2d`, `0x2fe94`, `0x2fe95`, `0x307d7`, `0x391a5` rose to 0.998 and fell to 0.0005 every 15.6 s near spawn). They do not reach exactly 1.0 in the samples taken, so the open-door test (`== 1.0`) ignores them; whether any reaches 1.0 at some point was not sampled long enough: `unknown`. A door that opened late (animation, a laser panel's consequence) made one state diff run before it reached B; the suite now retries a clean diff for up to 20 s.
- QA time: the 28-scenario serial `tools/qa.sh` took 48 min 15 s. `tools/qa_par.sh` (3 shards, each with its own two game copies) with polling instead of fixed sleeps, `cable maze` folded into `cable chain`, and the laser and cable-off scenarios merged into `bot-free effects` ran 23 scenarios in 13 min 33 s (the last of five runs; the others took 13.5 to 18.5 min and showed load flakes: the first-save hygiene check, a game not in the world when the bot started, a missed click, a state dump of a loading game). The suite now waits for the save, retries a scenario once when its bot never reached its precondition, and retries the state dump.

#### Derived

- Cable-lights-panel is a second power path (cable to panel, no panel target). Both ends are synced (cable event and panel power event), so a chain of mazes converges whichever event arrives first.

#### Unknowns

- Power-off for panels (nothing seen), the other target types, a real lever or ramp driving the off call, and loading a different save slot.

### Player model (2026-10-24, witness-coop branch `player-model`, commit 23baee7)

Replaced the flat puppet with a skinned glTF character drawn in the `Present` hook, hidden by walls through the game's own depth buffer.

#### Observations

- Patching the immediate-context vtable of a dummy device (`OMSetRenderTargets`, slot 33) sees the game's own calls, the same way the swap-chain vtable patch sees `Present`. Over one frame it showed 53 calls: a 1920x1080 `R24G8_TYPELESS` depth texture with 4 samples and bind flags depth-stencil + shader-resource (the scene), a 960x540 one, two shadow-sized 16-bit ones (4096x1024, 3072x1024).
- That scene depth texture still holds the frame's depth at `Present` time. A shader view (`R24_UNORM_X8_TYPELESS`, `Texture2DMS<float>`) can be read from the overlay's pixel shader, no copy needed (a staging copy of a multisampled depth texture fails with `E_INVALIDARG`).
- Depth is reversed and has no far plane: stored value = `0.0597 / distance`. Measured with the player looking at panel `0x64` from three distances (the proxy `probe` command reads the depth at the screen centre): 0.03960842 at 1.508 m, 0.02382934 at 2.505 m, 0.01784265 at 3.343 m, i.e. value x distance = 0.05972, 0.05968, 0.05965 (distance = camera to panel position). Three more distances in the same run were bad samples (the teleport had not landed); they were not used.
- `SV_Position.w` in the pixel shader was 1.0 or more at a point 6.3 m away, so it was not `1 / view depth` there. Passing the clip-space `w` from the vertex shader as a separate interpolant gave the right distance; with it the occlusion test works.
- A player cannot be teleported behind the closed first door: the pose B sent put the figure at the door, not behind it (the game resolves the collision). The proxy therefore has a test hook (`fakepose`) that draws the peer at a chosen place.
- Results (real games, `tools/qa.sh model`, 5 scenarios, all PASS, real saves byte-identical after each): figure 5 m ahead 27571 px (GPU occlusion query, samples passing), standing speed 0.0, walking at about 3.6 m/s measured from the received poses with the clip time advancing, idle and still again after stopping, 138589 px at 2 m in front of the closed door and 0 px with the figure 3 m behind it. Mutants: depth test off -> 16193 px behind the door (only that check fails); gait off -> clip time stays 0 (only that check fails); model off -> the six checks that need a drawn model fail.
- T0 (`make -C proxy/model test`, also run by `make -C proxy/core test`): 229 checks on the three bundled models (skinning against the bind pose, loop, quaternion slerp, camera matrix vs the puppet's projection, placement and facing, walk state, clip choice by name) and 6 mutants killed; the sync-core fuzzer and its 10 bug twins still pass.

#### Derived

- Walking speed in the game is about 3 m/s (9.1 m in 3 s, measured earlier); the figure's walk cycle is played at `speed / model_speed` of its clip rate.
- The visible-pixel count measures the depth test (4 MSAA samples averaged for the edge alpha); it does not measure colours. Lighting, textures and facing were checked by eye on screenshots only.

#### Unknowns

- The depth near value was measured on one camera setup (field of view 60 degrees, 16:9, 1920x1080); other resolutions or fields of view: `unknown`. Behaviour while the game is in a different render path (menus, puzzle view): `unknown`.
- Models with several materials or textures: only the first base-colour texture is used. Crouch/prone: the game has none, so not handled.
- A player's own model choice is local (what you see for the other player), not sent on the wire; announcing it needs a pose packet change (protocol bump) and is not done.
- The Cesium Man model carries the Cesium logo (a trademark, CC BY 4.0 for the model itself); whether to ship it as the default in a public release: `unknown`, the tester decides. A different default can be set in `proxy/config.h`.

### Puzzle sweep: teleport to every puzzle and solve it with the mouse (2026-10-25, witness-coop branch `panel-sweep`)

Goal: a way to test every puzzle automatically (a bot teleports with the dev menu, draws the solution through the in-process mouse, and the game's own solved flag is the verdict) and a way for a tester to do the same for one puzzle. No panel is marked solved by a dev action in this sweep.

#### Observations

- The world has 594 placed panels (a fresh game, build 11370412). 23 have no graph at all (nodes 0: dummy or hidden panels, mostly below ground), 96 plain-looking grids and mazes carry a stored solution line, 165 are plain mazes/lines, 309 carry cell decorations; 56 have a mirror map (symmetry).
- **Stored solutions.** 97 panels have a node list at panel `+0x454` (length) and `+0x458` (nodes) from the first frame of a new game (the 96 undecorated ones are the grids and mazes whose rule is not in the cell decorations). A shortest path to the end is rejected on them (state `+0x280` goes 0 to 2); drawing the stored line is accepted (state 1, solved flags set). Panels `0x139` and `0x198b5` were checked both ways. A drawn line, accepted or not, did not change the stored sequence on any panel compared (0 of 594).
- **Panel state `+0x280`:** 0 idle, 3 line drawn and not finished, 1 solved, 2 finished and rejected. The finishing click on a line that is not at the end also ends at 2.
- **Teleport.** The dev menu's old teleport (player dropped 8 m above the panel, 12 frames of position writes) put the player back at the spawn point for the far panels. Cause (measured): the game snaps the player to the ground under it once per frame, to one fixed low level where no collision is loaded, so a released player ended below the island. A new `hover` writes the player's position in a spin loop for 2 s after the menu teleport; after that the player stood still at ground + 1.69 m (`0x139`: camera 20.88, panel 20.885) for the whole 4 s watched. A once-per-millisecond write left a visible share of frames underground (camera height jittered by up to 0.5 m); a write per frame was worse.
- **Pixel size.** How big a panel unit looks is not a function of anything read from the panel: pixels per unit x distance was 585 on `0x64`, `0x182` and `0x139`, 666 assumed for `0x293`/`0x295` (worked), 745 about right on `0x2c4`. The start circle is found by trying sizes in 3.5 % steps (the cursor starts at the screen centre; a click that misses the circle does nothing). The panel centre is also shifted on the screen by (height of the eye below the panel, sideways offset) x 1033 / distance pixels.
- **Mouse gain.** With a steady camera one edge costs the same number of counts on both axes: 96 counts for a 0.172-unit edge on `0x139` (557 counts per unit), about 806 per unit on `0x293`/`0x295`: so not the free cursor's 2 counts per pixel, and it differs per panel. The traced-node count at `+0x228` steps up exactly when the line arrives at a node (1 at the start node, i + 1 at path[i]); the end stub does not add to it.
- **Camera jitter was the reason earlier drawings ran away** (one vertical count moved the line a whole edge): with the camera height bobbing the cursor's position on the panel moved by tens of pixels.
- A cursor pushed past the panel's edge while the line is at the end stub made the finish fail; the stub near the top of the panel leaves 11 px.
- Results of the drawing bot (real game, panel by panel, as of this note): tutorial `0x64`, `0x182`, mazes `0x293`, `0x295`, `0x86`, stored-solution grids `0x139`, `0x143`, `0x198b5`, and others; see `sweep_results` in the run folder for the full list of the current run. A stored line for `0x2c4` (a pill-shaped circuit with short chamfered edges) was not reproduced on the first tries: the line took a different branch at a node (the traced count cannot see that).

#### Derived

- `tools/win/wdraw.py` solves panels whose line is made of long axis-aligned edges and whose rule the solver knows; the path the game accepted is always one the solver (or the stored data) gave.
- psolve (`tools/psolve`, C++, T0 tests) models dots (black, blue, yellow), gaps (missing edges), squares, stars, triangles, polyominoes (rotations, negatives), erasers and symmetry (mirror line from `+0x438`) on grids. It finds a line for 283 of the 329 decorated panels and 45 of 47 mirror panels in its own model; whether the model is right is decided by the game, panel by panel.

#### Unknowns

- Whether the stored line is the only accepted line on those panels: the rule behind them is `unknown` (nothing in the decoration, region or style fields explains why a shortest path fails).
- The layout of cell decorations (row 0 at the bottom, value = shape << 16 | type | colour) is inferred from the values and checked only through accepted lines.
- Panels with a 90-degree turn between short edges (`0x2c4`): `unknown` how to steer reliably.
- The 23 panels without a graph: `unknown` what they are.

#### Update (same day, later): what the sweep found about the panels it could not draw on

Observations:
- **Power.** A panel whose `+0x2a4` is 0 cannot be started (no click anywhere on its face starts a line); solving something upstream sets it. 297 of the 594 panels start with it 0. Whether a start works is judged by the traced count (`+0x228` >= 1) or panel state 3 (`+0x280`); state 2 is the game's "wrong".
- **Floor puzzles.** Four panels (`0x1be9`, `0x1cd3`, `0x1d3f`, `0x33ea`, in the Keep) are not drawn with the mouse: each has one pressure plate per node and one per edge of its graph (66 plates for 25 nodes and 41 edges on `0x1cd3`). A plate has its panel's engine id at `+0xe4`, `1` (edge) or `0` (node) at `+0xe8`, and the node, or the two nodes of the edge, at `+0xf0` / `+0xf4`. Holding the player over the 34 plates of a rules-solver path, in order, was accepted by the game on `0x1cd3` (`tools/win/wwalk.py`). A click on the face of such a panel did nothing anywhere on it (10 px raster over `0x1be9`).
- **Panel groups.** `+0x2d8` / `+0x2dc` of a panel is the same for the panels of one area (137 groups); on some areas almost no panel could be tried because their power does not come from a panel or cable the sweep can reach: plates, `Multipanel`, `Gauge`, lasers and `Group` entities point at them.
- **Rule solver accuracy.** Of the lines drawn from `tools/psolve` the game accepted all but 2 (state 2) at the time of writing.
- **Start circle.** Where the start circle is on the screen needed a search over the panel's size in metres (0.5 to 1.2), a vertical shift, an aspect factor (1.4 on some panels) and mirrored axes (`0x1cd3`-like panels were shown turned 180 degrees); panels of one kind (scale, grid size, symmetry) are found at the same place, which the bot remembers.
- A game died on teleports to places with nothing to stand on (about 6 times in 4 hours); `sweep.py` notices and restarts it, and it gets the solved panels back from its partner.

Unknowns:
- How the panels in the dark rooms and the 0x20 / 0x21 group are meant to be powered in play (they were solved only after the dev menu powered them, for diagnosis, not counted).
- What `Multipanel`, `Gauge`, lasers and `Group` entities need from the player.
- Whether the Keep's plates also have a time limit.

#### Update (later the same day): more sweep facts

Observations:
- **Mouse gain per direction.** On the swamp panels (`0x17dc8` and neighbours) the line needs about 2.3 to 3 counts per pixel horizontally and 8 to 12 vertically (the projection's width and height scale did not match); learning the gain per direction made `0x17dc8` solve where one number overshot. On the other panels both directions agree.
- **Floor puzzles.** All four (`0x1cd3`, `0x1d3f`, `0x1be9`, `0x33ea`) were solved by walking. The traced count (+0x228) goes up on the EDGE plate (one per node of the path, the end stub's edge plate last); stepping on a node plate alone does not count. An edge without a plate (on `0x1be9`, edge 9-14) cannot be walked, whatever the graph says: the rule solver is run on the edges that have a plate (and marker nodes on an edge are skipped).
- **Power chains.** 55 panels got power during the sweep: 28 from a panel, 25 from a cable (the panel's `+0x2c8` names the source, as an engine id). 65 panels that are unpowered at the start have no panel or cable that powers them.
- **Negative result.** Teleporting the player through every `Touch_Collision` volume (489), `Collision_Path`, `Audio_Recording` and `Obelisk_Report` (0.25 s each, held over it) did not change the number of powered panels (237 before and after).
- Pair 1 of the sweep finished everything it could power (63 panels waiting for power that nothing it can solve provides); 188 of 594 panels were solved at this point.

Unknowns:
- What powers the 65 root panels (the dark-room panels `0x6`..`0xa`, `0x7`, `0x65`, `0x84`, `0x8d`, ...): multipanels, lasers and `Group` entities reference them; how a player triggers that is `unknown`.

## 2026-10-25 panel power links: `+0x2cc` is the static predecessor

Observations (live scan of all 594 panels, one game, nothing solved by forcing):
- `+0x2cc` holds an engine id on 231 panels. On a powered panel `+0x2c8` equals `+0x2cc`; on an unpowered one `+0x2c8` is 0 and `+0x2cc` still names a panel (e.g. `0x4` -> `0x2`, `0x5` -> `0x4`, `0x6d` -> `0x65`, `0x26e` -> `0x26d`).
- 221 of the 357 panels that start unpowered have a `+0x2cc`, 102 have a cable aimed at them.
- After counting both, 40 unpowered panels have neither (earlier count: 65 using `+0x2c8` only): `0x7 0x65 0x7c 0x84 0x8d 0x26d 0x390 0x3b2 0x88e 0x999 0xc92 0x32f7 0x3317 0x3545 0x3549 0x354e 0x354f 0x3552 0x3553 0x356b 0x360e 0x383f 0x3859 0x9e56 0x9e5a 0x9f01 0x9f8e 0x9fda 0xa036 0xa049 0x19797 0x28ad9 0x335ac 0x3369d 0x33961 0x339bb 0x34c7f 0x3d9a8-0x3d9aa` (a few of these are named as a cable's target, so the list over-counts; see derived).
- A scan of the first 0x600 bytes of every entity for the ids of these panels finds `Marker` entities pointing at `0x3545..0x3553`, `0x3b2`, `0x3317`, `0x360e`, `Occluder` entities at `0x390`, a `Landing_Signal` at `0x9eff`; nothing points at `0x7`, `0x65`, `0x7c`, `0x84`, `0x8d`, `0x26d` except small-integer collisions.

Derived:
- A panel becomes powered when the panel in its `+0x2cc` is solved (the chain `0x1b..0x21`, `0x5d..0x61`).

Unknown:
- What powers the 40 roots, and whether any of them is powered by something other than a link (the first of each chain may simply be powered from the start in the real game and need a frame near the player).

## 2026-10-25 power gating explained: multipanels, and why the sweep stalls

Observations:
- Every `Multipanel` entity owns a group of panels (the panel's `+0x2bc` is the multipanel's engine id; 41 groups). In 31 of 41 groups exactly one panel (the head of the `+0x2cc` chain) is powered in a new game; in 10 groups none is.
- 9 of those 10 multipanels have a cable aimed at them (cable `+0xcc` = multipanel id), and following the cables upstream ends at a panel (e.g. multipanel `0x6e` is fed from panel `0x79`, `0x26c` from `0x17cab`, `0x98c` from `0x56f`).
- With edges "panel solved -> the panel its cable chain ends at", "-> every panel of the multipanel it ends at" and "-> panels whose `+0x2cc` names it", 533 of 594 panels are reachable from the 237 that start powered; 61 are not (the dark-room loop `0x7..0xa`, the `0x35xx` group, the `0x1972x` group, and a few more).
- The 285 panels without a result in the sweep are mostly gated by a failed or unsolved upstream panel, not by an unknown power source. Ranked by how many untried panels sit behind them: `0x24` (14, a glass strip), `0x7c`, `0x17d88`, `0x21b6`, `0x290` (7), `0x5c`, `0x17e3c`, `0x17d9b`.
- Glass strips: the depth probe at the screen centre reads what is behind the glass, so the "is the panel in view" check failed for `0x22..0x24`; with the check waived the three are solved by drawing.
- `0x290` (4x4, stored solution) is drawn as a diamond: its grid is turned 45 degrees in the panel's plane while the quaternion is yaw-only and no rotation field was found in the first 0x700 bytes. Solved with a 45 degree roll and a panel size of 1.88 m (the search now tries it).

Derived:
- The sweep order does not need unknown triggers for most of the world; it needs the failing panels fixed in blocker order.

Unknown:
- The source of the 61 unreachable panels, and which field holds the roll of a turned grid.

## 2026-10-25 sound dots, swamp end stubs, unsupported classes

Observations:
- Eight jungle panels carry node flags `0x2000` / `0x4000` / `0x8000` (sound dots of pitch 1 / 2 / 3) plus the dot flag `0x20`. Seven of them hold the panel's own sequence at `+0x460` (count) / `+0x468` (pitches). A line that meets the sound dots in that order is accepted: `0xafb` (7x7, symmetry, sequence 2,3,2,1) and `0x34e3` (no sequence stored: all four dots visited, any order) are solved by drawing.
- `psolve` models the order (`dseq`); T0 test `test_sound_dots` plus mutant `pitch` (the order check off must fail a test).
- Swamp-style panels with a mouse gain that differs per axis finished with state 0 because the end stub was pushed with the gain of the last edge. With the stub's own axis gain `0x17d9b`, `0x17d88`, `0x17e3c` are solved.
- 23 panels have no graph at all (0 nodes), e.g. `0x51f`: a black screen in a cave. `unknown` what fills them.
- `0x383a` (6x4 node grid, 145 nodes, cell array 6 x 3) teleports to a dark cave with a pillar and an orange slider rather than a drawable grid; not solved, `unknown` how it is operated.

## 2026-10-25 swamp rows: how the "root" panels get their power (tester walked it)

Observations (one game, a person playing with the real mouse, the bot drawing where asked):
- The first dark row (`0x7`..`0xa`, multipanel `0x94`) has no panel or cable in the extracted links. Solving the row `0x982 -> 0x97f -> 0x98f -> 0x990` lit cable `0xbfc` (to multipanel `0x9aa`), opened door `0x184b7` and powered `0x999`. Solving `0x999 -> 0x99d -> 0x9a0 -> 0x9a1` lit cable `0xbc9` (to door `0x18507`), opened that door and two more cables whose target field reads -1 (`0x18458`, `0x33863`), and powered `0x7` (the head of the dark row). `0x8`..`0xa` stay unpowered until `0x7` is solved.
- A platform control panel (6x6, polyominoes, `0x181f5`) turns the platform it stands on when the yellow shape lies in the same region as the two coloured marks for the directions to turn between (tester's finding). Solving it changed no panel, door or cable flag.
- A row whose first panel is "unpowered, no source" is therefore not a root: its power comes from a lit cable whose target is not in the extracted `+0xcc` links, after an earlier row is solved.

Derived:
- The sweep's root list (40 panels) is a list of rows whose feeding cable has target -1 in the extract; the cables that light when the upstream row is solved name them. Next step: for every cable with target -1, record when it lights during the sweep.

Unknown:
- The field that ties cables `0x18458` and `0x33863` to multipanel `0x94`.
- Teleporting to a panel can land on a level that cannot be left (stair pit, lower walkway); the player has to be placed from the panel's own platform.

## 2026-10-25 hand play: the panel cursor and the real mouse

- With `force_active=1` (bot mode) the game's panel cursor moves about 1/3 to 1/2 pixel per mouse count while the real pointer moves 1 pixel, so the circle lags and the pointer leaves the window. `WITNESS_HUMAN=1` (no `force_active`) makes the mouse follow normally. A proxy-side fix exists too: F3 multiplies real mouse motion by `panel_mouse_gain` (default 3), bot events are not touched.

## 2026-10-25 entry area: the pillar, the floor panel and the environmental puzzle (tester played, web notes agree)

Observations:
- `0xc335` is a stone pillar (6 columns round, 6 rows, triangle clues of count 1 in columns 1 and 4 of every row). Its cell array is 6 x 6 and the graph closes on itself, so the solver now treats a cell array of `gx x (gy-1)` as a cylinder (T0 mutant still to do). A line found by the solver was accepted in the game.
- Solving the pillar lit cable `0x34d5a`, which feeds `0xc373` (floor panel, 6x6 nodes, two black dots, `+0x2c8` = that cable). `0xc373` has no outgoing link in the extracted data (target -1).
- Solving `0xc373` normally (hitting both dots) turns its line blue and changes no flag the state dump tracks. Drawing it so the first dot is missed turns the line yellow; the yellow line is part of an environmental puzzle among the flowers around the panel, which the tester solved. That activated a section of a black obelisk (6 `Obelisk`, 135 `Obelisk_Report` entities exist; their fields are not decoded).

Derived:
- A panel can be accepted ("solved") and still need a deliberate fail to do its job; a sweep that only asks "is it solved" cannot see the environmental or obelisk effect.

Unknown:
- Which fields change when an obelisk section activates.

## 2026-10-25 entry area: bars, two exits, two starts, effects the flags do not show

Observations (tester played, state dumped before and after):
- `0x3629` (the gate control with three blocking bars) is unblocked by solving `0x293`, `0xa3b2` and `0xa3b5`. After those, `0x295` and `0x2c2` became powered and were solved, three doors opened (`0x288e8`, `0x288f3`, `0x28942`) and cables `0x349`, `0x3c`, `0x3f`, `0x45c`, `0x51` lit. The links from those panels to the bars are not in the extracted cable data.
- `0xa3b5` has two end nodes (103 and 104); the tester says the second one runs to a place that cannot be seen from the panel. `0xa3b2` has two start nodes (0 and 6).
- `0x3505` (triangle counts 1, 2, 3 in the first column, node flags unlike normal panels) is a panel whose drawn path turns into an environmental puzzle on a building; its own solved flag stays off after the tester completed the environmental puzzle. `0xc373`'s solved flag goes off after the deliberate fail.

Derived:
- "Solved" in the panel flags is not the goal for panels with several exits, several starts, or an environmental result: a sweep has to try every start and every exit and have the result checked some other way.

Unknown:
- Where the second exit of `0xa3b5` leads.

## 2026-10-25 panels with no graph are not always panels to draw on

Observations (tester played):
- `0x1c343` / `0x1c344` (powered, no graph) are rotating reflectors with a floor controller; the tester rotated one. No laser reaches it in this state.
- `0x33d4` (8x8, extra start circles in the middle, black and white squares, odd dot flags `0x240020` / `0x440020`) was solved by hand, opened door `0x33d0`; `0x3481` (a 2-node straight line) then opened door `0x3506` and a box with a paper of connected hexagons.
- Per fan sources (not verified in game): 11 lasers, one per region, aimed at the mountain; the Desert laser must be reflected up to the mountain from the Town; environmental puzzles belong to obelisks; Discarded Panels are triangle collectibles.

Derived:
- A sweep needs a type for "not a line puzzle" (reflector, screen, pillar, control with bars); `no graph` should be reported as that type, not as a failure.

Unknown:
- What the 23 blank-screen panels (e.g. `0x51f`) are.

## 2026-10-25 theater doors, hole cells, and what the "rule" flag was

Observations (tester played the left door; web notes place these as the exit doors of the underground A/V room):
- `0xa168` (left, eraser cells at the bottom, black and white squares) opened door `0xa16d` when solved; the tester walked through a tunnel to the outside. `0x33ab2` (right, four orange corner polyominoes, four black squares) is the other door.
- Both have two cells of decoration type `0xa` (`0xa02`) in the bottom row either side of the start: they are not there (no region, no area). The panel summary's word "rule" is that type. With them counted as ordinary cells the solver found no line for `0x33ab2`; with them removed it finds one (`U1 L1 U1 R2 D1 R1 U4 L1 D1 L2 U1 R1 U1`, from the start node; not yet tried in the game).
- The two small icons at the bottom of `0xa168` are erasers (cells 1 and 2 of the bottom row).

Derived:
- T0 test `test_hole_cells` and mutant `hole` pin the rule down.

## 2026-10-25 faster tests: what was cut and the one cut that broke drawing

Observations (commits ea1b29b, 3df5030, c5e9f6d of the mod repo):
- T0 (`make -C proxy/core test`) runs in 6 s. The T2 scenarios (sweep, pov, lines, tags, ui) run in three parallel shards on six game copies, each shard with its own game copies: `tools/qa_fast.sh`, 535 s for all five, down from about 22 min before the draw fix and about 15 min for one scenario alone at the start.
- The first parallel run failed `sweep` and `lines`: the bot's line never started ("did not start with any panel size or offset tried"). Not the frame rate (same failure at 30 fps). With the previous `wdraw.py` `lines` passed (5 min 20 s); with the faster `wdraw.py` plus the old fixed 3.2 s wait after the menu teleport it passed (4 min 25 s).
- One run of `tags` read no tag rows at all on the clean build and on the first mutant, and passed in two runs after the wait for B's row went from 20 s to 60 s. The log of the failing launch was overwritten, so whether B had not connected or `tagstat` did not answer is `unknown`.

Derived:
- Polling for "the player stands in front of the panel" ends while the game still holds the player for 2 s after a teleport; drawing started in that window did not take. A poll must wait for the condition that matters (the hold is over), not for a position that is already right.
- A mutant that is "caught" while the clean build also fails proves nothing: check the clean result first.

Unknown:
- Whether the `tags` flake is gone (two passes since).

## 2026-10-25 a character model with its own skeleton: eye height, gait clips, cross-fades

Observations (a 151-joint, 38k-triangle Sketchfab character, CC BY, used locally and not added to the mod repo):
- The file's one clip is a 20.8 s reading loop that moves only the arms, a book and the eyes: no leg, hip or spine joint is animated. Our loader chose clips by name, switched between them abruptly and had no idea where the eyes were.
- The nodes above its skeleton carry a 0.01 scale (two matrix nodes with opposite 90 degree turns), while the inverse bind matrices are in the unscaled space. Vertices are 1744 units tall, the loader's bind-pose bounds 20.7. A joint's origin from its inverse bind matrix (46.2 for the eyes) is therefore not in the space `bmin`/`bmax` are in (-14.4): the first version of the eye-height code mixed them, and the unit test (a synthetic model with no such scale) did not see it.
- Cesium, Fox and the Rigged Figure still load and play (T0, 247 checks; T2 model suite: see the commit message of the branch).
- In game (two games, the viewer's cfg names the model): the eye line check finds the frog's two red eyes on the middle row of the screen (rows 530.5 at 3 m and 534.4 at 6 m, screen middle 540, tolerance 21), the clip is idle at rest, walk at 1.6 m/s and run at 5 m/s with a running phase, and every change of state is cross-faded (blend 0.08 .. 1.0 seen; 0.08, 0.55, 0.94 on the way from run to walk).

Derived:
- The walk and run loops were generated (`tools/frog_gait.py`) on top of the idle clip's first frame, so the arms keep holding the book. Stride gains were calibrated against the planted foot's speed measured by the loader: 1.61 m/s for a 1.60 m/s walk and 4.70 m/s for a 4.5 m/s run, lowest foot point 3 mm below the sole in the walk.
- Teleport commands streamed at 30 Hz do not make an avatar walk (its measured speed stayed 0.00); `fakepose` does, and it makes the speed exact.
- The avatar's smoothed speed approaches a constant speed from below, so a stream at exactly the run threshold never starts the run clip.
- A red-pixel centroid is not an eye detector: the red gem on the book pulled it 120 px down. The check now looks for a pair of red blobs on one row.

- The facing that looked inconsistent in the first runs was the test tool: `tp x y z yaw pitch` holds the player for 12 frames and the game's own camera heading comes back within 0.3 s (read from the game: asked 1.885, back to -1.257 after 0.3 s). The puppet follows the other game's real heading: a real mouse turn of 0.51 rad moved the puppet's yaw from -1.257 to -1.767 against the other game's camera heading of -1.769. A heading under test needs `fakepose` (explicit yaw) or real mouse input.
- A negative number is a height: the eye height is measured in the space the bounds are in, where a raised eye can be below zero (-0.14 for the walk pose of this model, 1.634 m above the feet). A first version used "negative" for "no eye joints" and reported -1 in the middle of every cross-fade; T0 test with negative coordinates and mutant EYE_NEG.
- A check that passes on "nothing found" proves nothing: the face-away check also requires the frog's green pixels on screen.
- Eye height while moving (posed eye joints, metres above the feet, 1.71 is the camera): standing 1.710, walking 1.634 .. 1.710, running 1.634 .. 1.775.

Unknown:
- Foot placement on slopes and stairs (the avatar is only given a pose, no ground contact: no foot IK).
- Whether the generated walk and run look right to a person: the evidence is contact sheets and measured foot speeds.

## 2026-10-26 the pause menu, many players with their own models, a showcase of eight real games

Observations:
- The pause menu: the first dword that flipped when Esc was pressed (module + 0x62DAC4) also flips with F6 / F7 and with entering a puzzle (read 0 -> 1 -> 0 -> 1 around F7, F7, F6; 1 in a puzzle; 0 in the pause menu on one reading). A fix keyed on it hid every puppet on alternate model switches; the model-selector scenario found it, the first pause test (Esc only) had passed. Fourteen dwords followed Esc and ignored F6 / F7; five of them are floats that read 0.0 in every state tried (walking, dev menu open and closed, in front of a panel, entering and leaving a puzzle, the model panel, after F7) and 1.0 in the pause menu: +0x630070 and +0x62FF3C are used (both above 0.01).
- Memory: a Witness game costs about 1.3 GB of free memory here (5 games took free RAM from 15.6 to 9.3 GB; each working set shows 2 GB because the hard-linked game files are shared), not the 1.8 GB the preflight assumed; eight games ran with 14.8 GB free.
- The `shot <name>` command keeps 15 characters of the name: a longer name is saved under the cut name and a script that waits for the long one waits for ever.
- `tp` holds the heading for 12 frames only; the game's own heading comes back within 0.3 s. Headings under test: `fakepose` (explicit yaw, one remote) or real mouse input (0.0025 rad per count, about 14 events taken per command batch).
- A model file may have several skins on one skeleton (RobotExpressive: 2 skins, 14 meshes), many flat-colour materials (Brainstem: 59) and no texture. Taking only the first skin left parts of the robot unbound; the base colour factor of each material now travels in the vertices. The Magic Frog's skeleton sits under a 0.01 scale in the nodes above it while the inverse bind matrices are in the unscaled space (vertices 1744 units tall, bind-pose bounds 20.7).
- With `model_by_player=1` and eight games (player i drawn with the i-th model of the list, the same in every game) every game shows seven different characters; two pictures from two viewers show all eight. Two straight rows hide players behind each other whatever the spacing; seven players on an arc, alternating two distances, are all visible.
- Streaming `tp` commands does not make an avatar walk (speed stays 0.00); holding W in the other game does, and so does `fakepose` at a set speed. A smoothed speed approaches a constant from below, so a stream at exactly the run threshold never starts the run clip.
- A parallel run once took four times as long as usual (POV scenario 881 s against 220 s, one mutant run unable to read the first game's stats); it did not repeat in a standalone run or in the next full run: `unknown` cause, load is the guess.

Derived:
- A state flag needs a state table, not an on/off pair: read the candidates in every state the feature will meet, and write the test so that each of those states must leave the feature alone (the pause test now presses the model keys and enters a puzzle first; a mutant uses the wrong flag and must fail there).
- A check that passes when nothing is found proves nothing (a frog missing from the picture passed "no face"): require evidence the subject is there.

Unknown:
- Mixamo's terms for redistribution of the characters in the three.js examples (Xbot, Michelle, Soldier): not checked, kept out of the repository.

## 2026-10-26 social features, voice, launcher, saves, new game with progress, map picture

Observations (The Witness, two to eight real games on one PC, T0 and T2 as named in `features.md` of the mod repo):
- Chat, typing dots, pings, emotes, a character list and free drawing run on one reliable ordered message layer (`WTX4`, ack `WTK4`, resend every 250 ms) and a presence packet (`WTS4`: emote, typing, speaking, character). The core test (loss, jitter, restart, corruption, 4-player group) passes clean and fails for 8 bug twins.
- The game keeps its player's pitch within about 53 degrees of level whatever is written (read back: -0.932 rad when -1.55 was asked). A top-down picture is therefore made of oblique frames rectified onto the ground plane; 103 frames at 70 m make a 1029 x 809 picture of the island. At 130 m a cloud layer covered tiles; at 70 m it did not.
- Flying the player to a place with no known ground nearby makes the game reset it ("emergency reset player to reset_marker"): 2262 resets in one stalled run, then the game stopped answering. Keeping the player within 90 m of known ground, skipping a tile the player was moved from and stopping after six in a row gave 5 of 5 finished runs (before: about one in three stalled).
- Pressing Esc to close our own list or chat box opened the game's pause menu: the game acts on the release of Esc (and on the raw key), so the mod now takes the release of every key it took and hides the matching raw press. Found by looking at the showcase pictures (the game was sitting in its pause menu); the T2 check and its mutant came after.
- The game's video options are a text file in the save folder (`Launcher.variables`: render_profile, fullscreen, vsync, highdpi, resolution 0-4, multisampling, custom size, five quality values); its log prints "Applying user settings" with what it took (1280 x 720 prints as "720p", not as a custom size). Its gameplay options are named values inside each save file (name, NUL, 4-byte value, 4-byte number); yes / no are the integer 1 (not 1.0), mouse speed 8.0, field of view 54.0, brightness 1.0.
- Windows remembers a program's volume in the mixer: a later run with the volume setting skipped still read the old value (0.35), so the "not set" mutant slipped through until the mutant run asked for a different value.
- `tp` to a place 28 m or more away put the player back at the start; `hover x y z yaw pitch` held it there. Every distance in the voice test uses hover, with the real positions read from both games.

Derived:
- Panel ids and their prerequisites come from the Archipelago randomizer's logic file (MIT), solved as a graph (regions as nodes, both ways unless marked one way, settled cheapest first). Symmetry Island needs 52 panels (its own 32, the Glass Factory's 12, the tutorial's 8); Desert, Keep and Town together need 103 (94 of their own areas' and 8 of the tutorial's, plus one from the jungle that comes in through a boat panel). The mountain needs the seven cheapest lasers.
- A tutorial door (0x3ba2, opened by panel 0xa3b5) is opened by the game's script: the panel's target field is empty, so solving the panel did not open it; the open doors read from memory showed it, and doors whose panels are done are now opened by hand.
- The mixer's walkie-talkie is measured, not heard: with a three-tone voice (120 Hz, 1 kHz, 5.5 kHz) at 80 m the outer tones are 12 dB or more below the middle one; at 5 m all three are there. Voice WAV files for listening are written by `proxy/voice/voice_demo.cpp`; nobody has listened to them yet.

Unknown:
- How the voice sounds with a real microphone and real speech (only a synthetic voice and a test tone were used), and whether 25 m and a radio gain of 0.85 feel right.
- Why one map run in three stalled before the fix: the game log showed the repeated resets, but not what stopped the game. The five clean runs after the fix do not prove it cannot happen.
- Whether the map picture lines up with the panel dots to within a few metres: only the agreement between neighbouring tiles was measured (0.33 correlation, against 0.23 for a wrong field of view or a mirrored picture), and the picture looks right next to the dots.

## 2026-10-26 a lobby, a race, see-through players and voice channels

Observations (The Witness; two real games and two real launchers on one PC; game-free tests as named in `features.md` of the mod repo):
- Voice, measured from outside the game with a separate program (per-process audio sessions with a peak meter): with two windows and no forced focus, the unfocused game's voice is still played (session peak 0.68 whether or not its window has the focus), while the game's own sound goes to 0.0000 when its window loses the focus. Reading the same session from the game's render thread hung the game.
- Voice channels (proximity, radio, open line, one on one): read from the listener's mixer at 80 m and at 5 m, each channel has the expected gains, pan and tones (the radio keeps 1 kHz and loses 120 Hz and 5.5 kHz by 12 dB or more; the open line keeps all three at any distance); one on one with a player that is not there reaches nobody. The first real-game run heard nothing: the network thread still routed only the old packet magic, which the game-free test could not see.
- See-through players: the alpha a game draws another player with (read from the game) goes to 0.22 when the player stands between the camera and a panel ahead, 1.0 two metres out of the line, 0.22 again when back in, and faded when right behind the camera; the table of panel positions (594) comes from the dev menu's entity scan whether or not the dev menu is on.
- Race: with `game_mode=race` a panel A solved was not solved in B's memory, B saw A's progress and finish time (the same number in both), both started within 100 ms of each other after a countdown, and a panel solved in the countdown did not count. A first design in which every player adopts the latest GO any peer reports never started: each round trip added the one-way delay (found in the game-free simulation).
- Lobby: with two launchers and two games the host and a guest chat, hear each other (a test voice: all three tones present, not the radio), the host's choices reach the guest, the start waits for ready, each launcher writes its game's cfg from one roster, the games find each other and race, and after the games end both launchers are back in the lobby with the result. A lock-order deadlock between the voice mixer and the lobby session froze the receiving launcher in the first run.

Derived:
- A race needs one player to set the moment of GO; a start every player extends to the latest anyone reports ratchets for ever.
- The game's own sound stops when its window is not active but the mod's voice, which plays through its own wave-out stream, does not: a voice test with two windows on one PC is possible, as long as the voice mode is not `off` (both cfgs of the earlier manual test had `voice_mode=off`, and push to talk only reads the focused window).
- Players that join a lobby after the game started must not be in the race roster: the race waits for every member of the cfg.

Unknown:
- How the voice sounds with a real microphone and real speech, and whether the one on one and open line channels feel right to people.
- How the launcher lobby behaves across the internet (only one PC, addresses on the local network, was used): the host's port has to be reachable and nothing here punches through a NAT.
- Whether the see-through fade and its speed (0.22, 5 per second) look right to a person: only the alpha was read.
- The race goal `panel:9F7F` (the mountain's last box, "Box_Short" in the randomizer's logic) as "the end of the game": not played to.

## 2026-10-26 what the first manual test of the lobby found

Observations (two real games started from two launchers by a person, one PC, then reproduced with the bots):
- Line sharing. The game keeps a drawn line as world positions written while the line is drawn and never updated; a trace record is a segment: two node numbers, its start and its end in the world, and the progress as a fraction (two words that were equal in the runs read, 0.049 rising to 1.0) - the end is the whole edge from the moment the segment exists. Sending the end made the other player see the whole horizontal edge of the first panel's line at once (read from the record while the bot drew it; seen in the game's own picture).
- The first panels are part of a door: after the solve the panel entity swung about 70 degrees (quaternion z/w from (-0.811, -0.587) to (-0.950, 0.316) in about 3 s) and moved about 1 m while the line's world positions stayed where they were written; lines sent as offsets from the panel's position at each scan, and added to the other game's panel position, then sat where the panel had been (the picture showed a red ribbon floating in the open doorway). Sent in the panel's own frame (position and rotation as of the moment the trace last changed) and turned back with the other game's current panel pose, the half-drawn tip was 0.28 m of a 0.41 m edge, the solved line 0.41 m, and the line was identical in the frame after the door had moved 1.0 m.
- Frame rate. A game window that is not active is throttled by the game's own loop to 9.6 frames a second (measured, real focus on the other window); with only the "active" byte set (+0x21 of the object at exe+0x469A5B0) it ran at 60. With two windows on one PC the player's second window, where they watched the other player's figure, was the choppy one.
- POV. `V` needed the cross-hair within a few percent of the other player's body and did nothing otherwise, with no message; with the numbered roster the lobby writes (instead of the single legacy peer) POV works the same (checked with the bots).
- Launcher. The tab row flickered under the pointer: repainted from scratch on every mouse move, and the tab control's own handler repainted again when the move was passed on (72 repaints for 70 moves; 3 after). Synthetic WM_MOUSEMOVE does not show it: Windows sends "mouse left" at once when the real pointer is elsewhere; the check moves the real pointer under the lab's input lock. Dropdowns 220 wide ran under hints that start 110 px earlier.

Derived:
- A shared line must be in the frame of the thing it is drawn on, as of when it was valid; world offsets from "where the panel is now" are wrong for any panel that moves (doors, platforms).
- A bot's synthetic mouse messages do not exercise hover code; use the real pointer under the input lock for that.
- WSL passes an environment variable to a Windows program only if WSLENV names it: a test hook that "never happened" cost a 200 s timeout once.

Unknown:
- Whether other moving panels (elevators, boats, platforms) behave like the first door: only the first panel was measured.
- Whether `run_unfocused` should be the default for people with one window (it costs CPU while alt-tabbed); it is off.


## 2026-10-25 groups of up to sixteen players (mod branch `coop16`)


Observations:
- The original sync core and UDP transport each retained one remote peer. Shared state includes panels, doors, panel power and reversible cable power.
- An eight-world addressed transport test passes loss, reversed delivery, duplicates, independent acknowledgements, all-player progression, pose enumeration and fresh-client recovery. A broadcast-ACK mutant is caught.
- A concurrent cable-toggle test failed with the original two-player arbiter. Group-only v5 events now retain Lamport clock/player-id versions, including snapshots; the same test passes.
- Existing two-player T0 suite passed 500 clean sessions and all 10 bug twins after the initial group changes. Later edits still need the full repeat.

Derived facts:
- Group reliability needs per-recipient acknowledgement and per-sender receive epochs/sequence counters; a single peer's ACK cannot retire an event for everyone.
- Reversible state requires a shared event ordering beyond the original player-one tie break.

Unknown:
- Real-client group validation and multi-avatar rendering remain unverified.
- Work is isolated on the mod's `coop16` branch; no changes have been merged or published.

### Implemented phase: mod commit 652020b (`coop16`)

Observations:
- Configured full mesh now accepts 15 remote peers (16 players total), with a roster generator and validation. Legacy two-player configs retain v4; configured groups use v5, including versioned cable events.
- Every remote pose is exposed to the renderer and map. Avatars retain separate gait state and share one depth buffer per frame. A test bridge reports current peer IDs/ages, queue depths and submitted avatar count.
- The final DLL builds. Full `make -C proxy/core test` passed: eight-player group stress plus four caught group mutants, existing retry/power/reload/cable tests, model tests and their six mutants, 500 clean legacy fuzz sessions and all ten original bug twins. Config generator tests and script syntax checks passed.
- `make -C proxy/core transport-test` passed on the final source: eight small Windows processes using production `coop.cpp` with fake worlds and real UDP, 25% outgoing loss, all-player progression, all seven remote poses, cable on/off, drained queues and one restarted client. This is game-free evidence, not eight real games.
- `tools/qa_group.sh 2` passed with real games on one PC: a normal input-driven first-panel solve and door cascade, command-driven progression originating on both clients, fresh remote poses, one submitted avatar each, 25% packet loss, restart catch-up and drained queues. Real saves were byte-identical.
- New stress exposed silent event loss after an absent peer filled the 4,096-event backlog. Groups now reset the stream and replace that backlog with a current snapshot; its capacity always fits a complete world snapshot. The eight-player test holds one receiver out through 4,200 cable mutations, restores the link and verifies state/queue convergence; the old-drop mutant is caught.

Derived facts:
- Per-recipient resend budgets are as necessary as per-recipient ACKs: a single unreachable member otherwise pins the first resend window and blocks connected members.
- A fixed outbox limit must have an explicit recovery path. Updating a detector's known state after silently dropping an event loses that change permanently.

Unknown / remaining:
- Eight simultaneous real-game clients and multi-avatar GPU rendering with seven actual remote clients remain unverified.
- Remote-PC networking and sixteen live game clients were not tested.

### Seven-avatar GPU and eight-client hybrid validation

Observations:
- A hybrid test connected one real game to seven lightweight Windows clients running production UDP code over fake worlds, with panel IDs selected from the real game's entity table.
- `tools/qa_group_render.sh` passed its shared-avatar-state mutant first (the script caught a stationary player inheriting another player's gait), then the healthy build.
- In the healthy run the real D3D renderer submitted all seven remote IDs at their distinct positions. Across 24 independently sampled frames, the moving peer reached 1.074 m/s and the six stationary peers remained at zero gait speed. Assertions read the animation state actually used for the draw calls through a render-thread JSON bridge, not screenshots or log messages.
- An ordinary input-driven solve in the real game reached all seven protocol clients, and unique panel solves from all seven clients reached the real game's memory. All receive/apply queues and reliable streams drained under 25% packet loss. Real saves remained byte-identical. The one real game and all protocol clients were closed.
- The original eight-client production UDP transport suite also passed again after the fixture gained its optional real-entity mapping mode.

Derived facts:
- Counting seven draw calls alone cannot prove independent avatars: a singleton gait state can submit all seven meshes while mixing their positions and motion. A moving peer beside stationary peers makes a useful mutant-sensitive check.

Unknown:
- Eight simultaneous real game processes have not been run. Hybrid evidence is explicitly one real game plus seven protocol clients, not eight real games.

Evidence commits: `3e13e60` adds the seven-avatar render/progression test and caught shared-state mutant.

### Rapid cable reversals during echo suppression

Observations:
- Review found that a genuine local cable command within five seconds of a remote command could be folded into the echo baseline and never transmitted. A new eight-world test failed on that reversal; another failed when a requested ON was still fading visually.
- Group detection now compares requested cable power against the last applied/requested state, so a new command bypasses echo suppression while the remote command's own fading glow produces no echo. Group snapshots read requested state and run after detection, preventing a pending restart resync from absorbing a new local command with an old version.
- Three game-free cases pass: immediate local reversal, reversal while the glow is still fading, and a reversal during a peer-restart snapshot. The old bookkeeping mutant fails all three as required. Existing group stress and four earlier group mutants still pass.
- The full T0 suite passed, including 500 clean legacy fuzz sessions and all ten original twins. The DLL builds.
- The eight-client production Windows UDP test now explicitly has client 2 reverse client 1's cable after one second. All eight clients observed the OFF before the five-second guard elapsed; later all-world progression, poses, queue drainage and restart recovery passed as well.

Derived facts:
- A time-only echo guard is insufficient for reversible state. Track the requested level that the remote call actually set; a different requested level is fresh intent, while brightness interpolation is not.
- Snapshot baselining must follow local detection, or it can consume the transition that should receive a new logical version.

Additional observations:
- The one-game/seven-client real-renderer suite passed again with the new detector and snapshot order. Both clean and mutant runs now assert that the real tutorial panel and door are incomplete before the bot runs; the healthy bot then solves the panel and opens the real door. All seven protocol clients and the real game's queues converged, seven independent GPU avatars were submitted, and saves were unchanged.
- The eight-client UDP reversal test prevents client 1's later timer from turning its own cable off: only client 2's remote reversal can make all eight worlds dark. It passed, followed by the existing all-player/restart checks.

Evidence commit for rapid cable intent and its tests: `f611b21`.


## 2026-10-27 character quality passes


### Observations

- Baseline previews show a clean traveller with a coat, scarf, hat and camping gear. The face reads as doll-like, the sleeves and coat are tubular, and the clothing lacks tailored construction detail.
- Baseline asset: 35,959 vertices, 55,096 triangles, 20 joints, one embedded base-colour texture, idle/walk/run clips.
- Baseline `tools/check_model.py`, `tools/test_idle_seam.py`, and `tools/test_run_pose.py` passed.
- First visual-quality pass reshaped the adult face and recessed the eyes; added stubble, folded lapels, shoulder seams, topstitching, layered bracers, fingerless gloves, boot lacing, a feather vane, and more muted weathered materials.
- Final pass asset: 59,368 vertices, 85,996 triangles, 20 joints, one 2048-square embedded JPEG, 4.56 MB GLB. It remains below the renderer's 60,000-vertex limit after UV splitting.
- `models/build.sh adventurer`, 11 `test_check_model.py` checks, idle seam, run pose, strap fit with mutant, gait contact with mismatched-stride mutant, and material bake with flat-material mutant passed.
- The real two-client adventurer scenario passed all 11 checks: visibility, idle/walk/run clips, advancing animation, render-thread statistics, wall occlusion, and untouched real saves.
- The two-client `hero` showcase captured idle and walking images in the actual game and reported stable world, every peer drawn, correct monitor placement, and byte-identical real saves.

### Derived

- The largest first-pass gains should come from natural facial proportions, layered/tailored clothing silhouettes, construction details, and less saturated weathered colours.
- The renderer currently supports one base-colour texture, so normal/roughness-map parity with a modern AAA character is not available through the existing material path.

### Unknown

- The tester's preferred final character art direction beyond the Enshrouded quality reference.

### Second pass - pose, drape and scalp

#### Observations

- The tester selected the refined-traveller direction: keep the hat, coat, scarf and camping gear.
- Changed to a relaxed asymmetric arm bind pose, added coat drape, and replaced the ellipsoid hair with a surface-following scalp. A solid beard and raised hair locks were rendered but rejected: they looked applied to the face and created artifacts. The delivered model keeps textured stubble instead.
- Rebuilt asset: 58,020 vertices, 83,492 triangles, 20 joints; 4,477,964-byte GLB with one 2048-square JPEG. It remains below the renderer's 60,000-vertex limit.
- Offline build, 11 `test_check_model.py` checks, idle seam, run pose, material bake with flat mutant, strap fit with unprojected mutant, and gait contact with mismatched-stride mutant all passed. `git diff --check` passed.
- The real two-client model scenario passed all 11 checks. The two-client hero showcase passed its stable-world, peer-visibility and monitor checks and captured idle and walking pictures. Real saves were byte-identical.
- Visual review: stance and scalp improved, but the rounded protruding eyelids still read as toys, the lips are poorly defined, and the coat and sleeves remain too tubular. Further work is needed; Enshrouded-equivalent quality has not been achieved.

#### Derived

- Eye whites, lids and lips should follow the facial surface rather than sit on it as ellipsoids.
- Cloth folds should describe waist compression, hanging weight and elbow compression rather than uniform corrugation.

#### Unknown

- How close the single-base-colour material path can get to the tester's visual reference without normal and roughness textures.

## 2026-10-27 to 2026-11-10: from prototype to public beta (v0.1.0-alpha, v0.2.0-beta.1, v0.3.0-beta.1)

Source for this section: the mod's own developer notes at the release tag
([docs/DEVELOPMENT.md](https://github.com/TommoDotCommo/witness-coop/blob/v0.3.0-beta.1/docs/DEVELOPMENT.md),
[docs/STEAM.md](https://github.com/TommoDotCommo/witness-coop/blob/v0.3.0-beta.1/docs/STEAM.md)), which name the test
for each claim. Addresses are for Steam build 11370412.

### Observations

- **Loading the mod without touching a game file.** The exe imports `DSOUND.dll` (`DirectSoundEnumerateA` and
  `DirectSoundCreate8`, by ordinal) and the game folder does not ship one, so Windows loads a `dsound.dll` placed there.
  The proxy exports the system DLL's twelve names and ordinals and forwards each to the system copy, loaded by full path
  (the bare name would be the proxy itself). Until v0.1.0-alpha the proxy replaced the game's own `AnselSDK64.dll`: Steam
  restores that file on every update and a mod manager cannot install it. `winmm.dll` was the other candidate and was
  ruled out because the mod's voice capture uses `waveIn` from it.
- **Under Wine and Proton the built-in `dsound` wins** over one in the game folder. The Linux install script writes
  `"dsound"="native,builtin"` for the game's exe into the game's own prefix (`user.reg`), or the player sets
  `WINEDLLOVERRIDES="dsound=n,b" %command%`. Tested on fixture folders and fixture prefixes only.
- **A solve does more than set the solved fields.** After them the game's solve handler runs `exe+0xB7F80`: about 70
  behaviours chosen by the panel's name (`+0x58`), among them the Swamp's sliding and rotating bridges and floodgate, the
  Quarry ramps, the windmill, the Town's lights, the Desert's water levels and elevators. Several read the line that was
  drawn (`+0x228` count, `+0x230` records of 13 dwords), for example which exit a bridge control was left by. A remote
  solve therefore writes the solver's final line into the panel's trace buffer (grown with the game's allocator,
  `exe+0x2E7FA0`), marks the panel, runs the reactions, dispatches the target only when they ask for it, and then takes
  the line out again. Left out on a remote solve: the ending and final elevator, the theatre videos, the boat, the timed
  challenge and the sound-room birds.
- **Treehouse bridges** grow because the solve handler tells the panel's `Bridge` (`+0x2C0`) that the panel is solved
  (`exe+0x84910`). Before the remote solve did the same, the other game had the panel solved and the next one powered but
  no bridge to reach it.
- **Report a door when it is told to move (`+0x1D4`), not when it arrives (`+0x1D0`).** Detecting arrival made a sliding
  bridge reach the other game 40 s late and the Caves elevator 12 s late.
- **The game builds a panel's line picture when the panel comes into view** (render object `+0x4A8`) and does not rebuild
  it for a line written from outside. A player who watched a remote solve saw the panel solved with no line. Another
  player's finished line is now drawn by the overlay, in that player's colour, while the panel is solved.
- **Holding a teleported player against the game's emergency reset crashed it.** A player held inside geometry is reset
  by the game to the nearest marker; a hold that wrote the position about every millisecond made the game reset every
  frame, and twice it crashed rebuilding the ground at the marker (`exe+0x2D03EC`). The hold now lets go when the game
  moves the player more than 1.5 m sideways.
- **Steam transport.** The launcher starts the Steam API as the game's app through the `SteamAppId` environment variable,
  which the game inherits, and the mod uses the game's own Steam connection: friends-only lobbies for up to 16, relayed
  by Steam, no port forwarding. Game-free tests run three production sync cores over a fake Steam link that loses 20%,
  duplicates 10% and reorders; an impostor claiming a member's id and a stranger are never heard.
- **Nothing is written into the game folder while playing.** Settings, the log and the update schedule live in
  `%LOCALAPPDATA%\witness-coop`. Every file Setup writes has a receipt beside it, so uninstall removes exactly those.
- **Playing the game through with N bots.** The order comes from Archipelago's vanilla logic for The Witness (MIT): its
  584 panel ids are the game's own entity ids. A planner hands each free bot a different available panel, the bot draws
  the solution with in-process input and takes the game's own solved flag as the verdict; every solve must turn solved
  in every other world. The route to the credits is 238 panels. Panels the bot cannot draw are recorded from a person
  and replayed.
- **A receiver that mistook a timer panel for a save load re-gated itself.** Early four-bot runs showed a 95th percentile
  propagation time of 12 to 13 s (maximum 36 s): every slow arrival was a receiver that took a timer panel or timed door
  for a save load and waited for a stable world again. Deciding "this was a load" from the world's generation instead
  brought the 95th percentile to 0.25 s in the core's lossy-link test.
- **Randomizers and the puzzle editor run inside the mod.** In a lobby the host's choice and seed (and the host's own
  puzzle set) are sent to everyone before the game starts, so every world has the same puzzles; a continued save keeps
  the puzzles it was started with.

### Derived

- When a game's solve handler branches on what the player drew, a remote "mark as solved" is not enough: replay the
  input the handler reads (here the traced line), run the handler's reactions, and guard what they change from being
  sent back.
- A proxy for a system DLL the game imports but does not ship survives store updates and file verification, and is a
  plain file copy for mod managers; replacing a file the game ships is neither.

### Unknowns

- A session between two PCs with two Steam accounts: the test steps are written in the mod's docs/STEAM.md; no result is
  recorded in the mod repository. One PC with one account cannot show two members in one Steam lobby.
- Steam Deck and Linux under Proton on real hardware; a person speaking into a real microphone; sixteen real game
  clients at once (eight were run for the showcase).
- Whether the bots can finish the 238-panel credits route unaided: no completed run is recorded in the mod repository.
- Stores other than Steam (GOG, Epic): the mod refuses any exe but build 11370412.
