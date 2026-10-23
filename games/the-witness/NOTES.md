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
