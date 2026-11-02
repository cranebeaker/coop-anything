# The Godfather II — research notes

Checked: 2026-10-20. Goal: the PC story campaign playable in co-op.

Claim issue: <https://github.com/cranebeaker/coop-anything/issues/78>.

## Identity

- **Observed:** PC version; story campaign co-op is the goal.
- **Game:** The Godfather II (2009), developed by EA Redwood Shores; PCGamingWiki identifies the engine as Visceral Engine.
- **Store / package ID:** unknown. The clean candidate executable reports `1.0.764.0`; its SHA-256 is kept only in ignored `local/identity.md`.
- **Target-build decision (2026-10-20):** after SE v0.56 was proven incompatible with `1.0.764.0`, the tested build switched to `1.0.789.0`, taken from the SE v0.56 bundle - an unverified third-party executable, used only as a disposable test target and not a build to follow. The `1.0.764.0` isolated copy is kept only as a reference; iteration happens in new isolated copies (currently `%LOCALAPPDATA%\Temp\opencode\godfather2-test-se`, whose original install source stays untouched). The `1.0.789.0` MPR bundled exe remains a separate, byte-different `.789` build: its existing decompilation (33,506 functions) matches the MPR exe, not the SE exe, and therefore cannot be used as the analysis base for SE-offset work. The `1.0.789.0` executable came from the Multiplayer Revival archive and is not the target build.
- **Target OS:** Windows, based on the supplied Windows game install. The original install and saves were not modified.
- The Nexus Multiplayer Revival page says it requires PC build `1.0.789.0`, which is the MPR-bundled executable. It does not establish compatibility with the clean `1.0.764.0` target; do not install or use MPR on that target.

## Existing work

### Multiplayer Revival (not story co-op)

- Page: <https://www.nexusmods.com/thegodfather2/mods/95>, by `paindubreak`.
- The author says it restores the original PC online multiplayer using a community-hosted matchmaking server. Players host matches; game traffic is peer-to-peer. The page describes lobbies, quick join, and host/join, not campaign co-op or shared story state.
- Compatibility stated by the author: PC version `1.0.789.0` and an ASI loader. The page header says version 1 while its file listing says version 2 (last uploaded 2026-08-06). No Nexus archive was downloaded; a pre-existing copied `.asi` was hashed and statically analyzed, but not matched to an archive hash.
- The page lists zero formal bugs, but its posts include connection/firewall trouble and a report of a 25-FPS limit. Its Aug. 6 author reply says the archive includes an ASI loader and a Godfather II executable; inspect any package contents and redistribution terms before using it. No mod was downloaded or installed by this session.
- The original online service is listed as shut down by PCGamingWiki. Its page describes online play for up to 16 and custom-match self-hosting. Contemporary coverage describes discrete map-based multiplayer modes, including team deathmatch, rather than a shared campaign: <https://www.ign.com/articles/2009/03/18/the-godfather-ii-gets-multiplayer-mode>.
- **Not verified:** successful connection, exact gameplay sync, NAT behavior, or whether any mode offers co-op against AI.

### The Godfather II SE (a promising modding hook, not co-op)

- Page: <https://www.nexusmods.com/thegodfather2/mods/40>, by `Greavesy` / `Greavesy899`.
- The latest listed release is `v0.56` (updated 2026-08-26). Its page describes a script extender/mod menu, loose-file mod loading, spawning NPCs/items/vehicles, loading checkpoints, and SimGroup overrides. This is the strongest hook found so far for a separate co-op plugin, but none of those features proves campaign replication is available.
- The page says its archive includes a DRM-free executable. No source link or license for the extender was found; do not redistribute its executable or assume its code can be reused.
- The Nexus v0.56 file manifest lists `GF2ASI.asi`, `dinput8.dll`, `discord_game_sdk.dll`, SimGroup mod data, and a bundled `godfather2.exe` (manifest SHA-256 `31f579a80cfd14f42c0379bbfa5b5b7f151bb06ec5163593023ab5002b1f1286`).
- The SE v0.56 archive was supplied for testing; its SHA-256 (`653d037000801abf45133d7f1a011749c71c83c529336b799feb5aeb0b90253f`, 6,055,436 bytes) matches the Nexus manifest's archive hash, and all 7 extracted files match the manifest per-file SHA-256 values. The bundle is authentic to Nexus; nothing was re-hosted.
- Inspection of the verified archive: the bundled `godfather2.exe` reports file version `1.0.789.0` (13,840,384 bytes). This differs byte-wise from the Multiplayer Revival bundled exe (same version and size, SHA-256 `d0339a3d79e73ee8ec542cda79608e8194f0f68d916b74115c59dce0267ae97f`), so each mod ships its own `.789` build. `simgroup_mods/README.txt` documents that the loader injects SimGroup override behaviours (`.sgp` files from VisceralToolkit) when the game loads a SimGroup `.str` stream; `gf2asi_settings.ini` documents VSync/frame-pacing and brightness/contrast fix hooks. Neither file states a supported build range beyond the bundled exe.
- **Compatibility verdict:** SE v0.56 requires build `1.0.789.0` and is therefore incompatible with the clean `1.0.764.0` target. Per the stop rule, it was not installed on the clean copy; it was only downloaded, hash-verified, and extracted into a separate inspection folder (`%LOCALAPPDATA%\Temp\opencode\godfather2-tools\inspect\se-v0.56`).
- The Nexus bug badge showed 3 when checked. Its visible list included a crash when making Michael Corleone a crew member (reported against v0.34, last post 2026-09-29), a v0.56 issue marked fixed (last post 2026-09-03), and older "not working" / startup reports. These are mod reports, not tests of co-op.
- A Sept. 25, 2026 post also reports a startup crash with an "api doesnt work" message; the author asked for the full error and installed-mod list on Sept. 28. No resolution was visible when checked.

### Related engine tooling

- <https://github.com/Greavesy1899/VisceralToolkit> describes WIP tools for inspecting/editing Visceral games, including The Godfather II. It may help inspect stream assets, but is not a co-op framework; GitHub reports no repository license. Its contents have not been cloned or built.
- The toolkit wiki says Godfather II uses Version 12 `.str` stream files; PC loading and saving are supported, editing is partial, and adding new entries is unsupported. Its resource-type notes identify navigation graphs, MetaModels, and a `VariableDictionary`, but mark most formats unknown and the pages as incomplete. These are leads, not verified script/quest APIs: <https://github.com/Greavesy1899/VisceralToolkit/wiki/File-Resource-Type-Support> and <https://github.com/Greavesy1899/VisceralToolkit/wiki/Resource-Types>.
- PCGamingWiki lists Dead Space 3 on the same named Visceral Engine and documents a two-player online co-op campaign: <https://www.pcgamingwiki.com/wiki/Dead_Space_3>. This is a useful engine-family reference, not evidence that its networking code or tools can be reused in The Godfather II.

## Static binary observations (isolated copies; no binaries published)

- The MPR-bundled `1.0.789.0` `godfather2.exe` is PE32 x86 and imports `WS2_32.dll` and `DINPUT8.dll`, among other Windows APIs. This confirms networking APIs in that bundled executable, not in the verified clean `1.0.764.0` build or that the single-player campaign uses multiplayer replication.
- The clean `1.0.764.0` copy has PE sections named `.securom`, `Stext`, `Sitext`, `Srdata`, `Sdata`, and `Sidata`; it also includes `paul.dll`. PCGamingWiki lists SecuROM 7.38.0015 for the retail PC release. Treat it as DRM-protected; exact store provenance remains unknown.
- The Multiplayer Revival `Multiplayer.asi` is also PE32 x86. Its imports include Winsock (`WS2_32.dll`) and INI-reading APIs; embedded strings mention FESL redirection, `Multiplayer.ini`, `Multiplayer.log`, and MinHook. Ghidra headless decompiled 84 functions. Its DLL entry path calls `SetProcessDEPPolicy(0)`, writes NOPs at absolute addresses `0x006A2B5A` and `0x0068D1B2`, then starts a worker thread at plugin address `0x6B3417D0`. These patches and the worker behavior have not been mapped/validated against the target executable. The plugin derives its INI/log paths beside its own module, so the effective path depends on where the ASI is loaded. **Do not launch this plugin yet:** its DEP change has not been assessed in a sandbox. No runtime test was run.
- File inventory of the MPR-bundled `1.0.789.0` copy found 2,304 loose `.str` files, including paths under the Cuba mission/objective hierarchy; no `.lua`, `.script`, `.sc`, or `.sco` files were found. Its `.db` files are `Thumbs.db` and its `.xml` files are EA Help docs. The sampled `GODFATH0.viv` begins with `BIGF`; sampled Version 12 `.str` files begin with `3slo`. The VisceralToolkit wiki says these streams hold varied resources and most formats remain unknown, so this does not establish where story scripts execute.

## Runtime isolation check

- Windows Sandbox was enabled after explicit approval. A smoke test confirmed the guest had its own `WDAGUtilityAccount` profile/Documents path, the clean game copy was mapped read-only, and networking was disabled.
- Two offline boot probes on the clean `1.0.764.0` copy: `godfather2.exe` started and remained alive for 60-75 seconds, but had no main-window title or visible window. It was responsive according to the process API, then stopped inside Sandbox. This does not prove the game reached its menu; no gameplay or network session was run. The original install and saves were untouched.
- A third probe with `VGpu` enabled showed identical behavior (process alive, no visible windows for the game PID), so the blank window is not explained by the disabled virtual GPU. The vGPU run was a single approved test with networking still disabled.
- A fourth probe that scanned **all** guest windows found the real cause: the game process was stuck behind a Windows system-error dialog, `godfather2.exe`, reporting a missing `d3dx9_40.dll`; the process showed 0 CPU seconds while waiting, and exited with code `-1073741515` (`0xC0000135`, STATUS_DLL_NOT_FOUND) when the dialog was dismissed. A follow-up Sandbox run then reported a missing `XINPUT1_3.dll`. Both DLLs belong to the legacy DirectX 9.0c runtime (DirectX End-User Runtimes, June 2010), which a clean Windows Sandbox guest does not include. The earlier "alive process with no window" observations were this error dialog waiting on OK, and the visible-window filter did not attribute the dialog because it belongs to a different PID (744). The Windows 11 host itself runs the game, so its DirectX runtime masks this outside the Sandbox.
- With explicit approval, the DirectX End-User Runtimes (June 2010) full package was downloaded directly from the Microsoft download center link published on their page (version 9.29.1974.1, 100,275,120 bytes, SHA-256 `053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b`; the same hash is recorded independently in the winetricks repository discussion and other checksum databases). Only the two needed DLLs were taken from its cabins (`d3dx9_40.dll` 9.24.950.2656 from `Nov2008_d3dx9_40_x86.cab`; `XInput1_3.dll` 9.18.944.0000 from `APR2007_xinput_x86.cab`); they are staged for the guest's game folder rather than system-installed, so nothing in the Sandbox guest's OS is modified beyond the disposable test copy. An SE `.789` boot test with these DLLs staged is the active check.
- Disassembly confirmed the check chain (`local/ghidra_output/disasm_4c7b40_607990.txt`). Device init is `FUN_00607990`; the under-spec gate is the per-adapter GPU score from `FUN_0060f390` → `FUN_0060efc0` (9 = pass; 3 = `GetDeviceCaps` failure → "Could not initialize display hardware"; 7 = `PixelShaderVersion < 0xFFFF0300`; 6 = missing raster/prim caps bits; otherwise the format scorer `FUN_0060ee70` must return 9), followed by `CMP EAX,9 / JNZ …` at VA 0x00607a0c; the raise at 0x00607b96 is `PUSH EAX` (the score), mapped by the dialog function to the "display hardware"/"minimum requirements" messages. In the disposable SE test copy only, the 6-byte conditional jump at 0x00607a0c was replaced with NOPs (byte-level record and hashes in `local/identity.md`); the pristine verified build stays unpatched and no install was touched.
- Boot re-test after the bypass (Windows Sandbox, vGPU, networking disabled): the minimum-requirements check now passes and boot stops one stage later at raise code 1 — "Could not initialize display hardware. Please restart The Godfather™ II." The game window existed from the first tick, the process stayed responsive with CPU accumulating while the dialog waited, and the Sandbox exited cleanly. Log `local/sandbox-output/gf2_se_boot.txt`, per-tick screenshots `gf2_se_boot_T20/60/120.png`. This moves the boot blocker from the capability gate to the device-creation/display path.
- A 32-bit raw-vtable D3D9 probe was built for point diagnosis (`local/dx9probe.c`, `local/dx9probe2.c` — own C sources, raw vtable calls and an official-`d3d9.h` cross-check; built with LLVM-MinGW clang as PE32 x86; every log line is flushed so partial results survive a crash). Host baseline on the real GPU: `Direct3DCreate9(32)` works, two adapters enumerated, 97 X8R8G8B8 mode entries, HAL caps report PixelShader/VertexShaderVersion `0xFFFF0300`/`0xFFFE0300`, and all four cap fields the game's scorer reads pass its thresholds; via the official `d3d9.h` interface `CreateDevice` returned `D3D_OK` with real device pointers (windowed and fullscreen, SWVP and HWVP), a bogus adapter ordinal correctly returned `D3DERR_INVALIDCALL`, and REF failed with its own error (missing reference rasterizer). Verified constant set for later work: `D3DCREATE_SOFTWARE_VERTEXPROCESSING=0x20`, `HARDWARE=0x40`, `MIXED=0x80` — an earlier probe bug passed `0x100000`, which the retail runtime treats as undefined behavior instead of a documented HRESULT. No game file, install, or save was touched by the probe.
- Probe vtable observation, resolved (both variants recorded earlier; now explained): from the object returned by `Direct3DCreate9`, a raw call to slot +0x3C returned constant `0x00010001` while a call to +0x40 with CreateDevice-shaped arguments returned `D3D_OK` and a real device. The official `d3d9.h` interface declaration settles it — `IDirect3D9` has **14** methods after IUnknown: slot 15 (+0x3C) is `GetAdapterMonitor` and slot 16 (+0x40) is `CreateDevice`; the earlier analysis had miscounted the method list. The `0x00010001` was an `HMONITOR`, not an HRESULT — a control call `GetAdapterMonitor(0)` on slot +0x3C reproduces exactly `0x00010001` on the host. Conclusion: **the game exe's +0x40 device-creation call follows the official interface layout and was correct from the start; the boot failure is a genuine D3D9 environment failure, not a game-side ABI quirk.** On the host the CreateDevice slot is owned by `C:\Windows\SYSTEM32\d3d9.dll`; in a previous host process the same slot pointed into a second module (observation recorded; container varies per process).
- The guest (Sandbox vGPU) probe result (final run, guest probe binary `d42f8b5e:`): D3D9 works through the paravirtual adapter - 1 adapter, identity reported as the host GPU under GPU-PV (model name omitted), the host-class HAL caps (PS/VS 3.0 included) are all present, and all four cap fields the game scorer reads pass its thresholds; but mode enumeration exposes exactly one 32-bit display mode ( further indices return `D3DERR_INVALIDCALL`). `GetAdapterMonitor` control on slot +0x3C returns the identical HMONITOR as on the host, confirming the layout. CreateDevice through the official slot +0x40 succeeds in the guest for windowed HAL devices - SWVP, HWVP, MIXED, and focus=NULL all returned `D3D_OK` with real device pointers - while exclusive fullscreen returns `D3DERR_INVALIDCALL` (expected against a one-mode virtual display), REF fails without a reference rasterizer, and NULLREF succeeds. Notable guest-specific environment fact: the IDirect3D9 object's CreateDevice implementation is backed not by the d3d9.dll image but by `C:\Windows\SYSTEM32\apphelp.dll` (the Application Compatibility shim engine wraps the D3D9 interface in the Sandbox guest). Conclusion for the boot failure: the patched game's "Could not initialize display hardware" (raise code 1) matches the game requesting a display/mode configuration the one-mode virtual display cannot serve - and the probe proves plain windowed HAL devices DO work in the guest.
- Launch-mode switches exist in the SE exe's own string table (read from the verified `.789` binary, no game file modified): `-windowed`, `-fullscreen`, `-resolution`, a `cmd.txt` command file, per-window settings keys `Window.Fullscreen` / `Window.FullScreen`, `Window.Width`, `Window.Height`, `Window.Top`, `Window.Left`, `Window.VSync`, `Window.Hz`, `Window.State`, `Window.Bright`, `Window.Gamma` under `settings.txt`, plus the UI strings `ClearResolutionList` / `AddResolutionOption` / "No supported resolutions available." So forcing windowed mode needs no executable patch. Boot test v3 launched the patched `.789` test copy with `-windowed` (guest-only robocopy of the disposable test folder; log overwrite per run as before).
- SE-exe decompilation (first Ghidra run, completed 2026-10-20): 33,512 functions, project `gf2_game_10789_se`, corpus `local/ghidra_output/godfather2_10789_se.c` + function index `.tsv`. The graphics-failure dialog is `FUN_0068b590` (@0x0068b590): param 2 = "Display hardware video memory exhausted", param 3 (or 1, or anything outside 2..7 other than 4..7 branch) = "Could not initialize display hardware", parameters 4..7 = "Your Graphics Card does not meet The Godfather… II minimum requirements.", then `MessageBoxW(MB_ICONERROR-family 0x40010)` and `__exit(-1)`. The game registers it once in renderer init `FUN_0068bd30` (site 0x0068beab `PUSH 0x68b590; CALL 0x0060e780`) into slot `PTR_FUN_00f15a3c`; all raises go through that slot. Decompiler-visible raise codes are only 1 (FUN_00607990 device init), 2 (12 sites, shader/target validation), and 21 no-arg calls Ghidra swallowed (20 inside FUN_004c7b40 — the technique/shader/technique validator — the presumed source of codes 4..7). Full disassembly dump of FUN_004c7b40 and FUN_00607990 was generated to recover the pushed codes; the dump is `local/ghidra_output/disasm_4c7b40_607990.txt`. Candidate patch (test copies only): redirect the registration `PUSH 0x68b590` imm32 (4 bytes at 0x0068bead) to a cave stub that swallows codes 4..7 and passes everything else to the original dialog — keeps init/VRAM failures visible and leaves FUN_0068b590 untouched for future diagnostics. Confirmed since: the 4..7 codes come from the compare `CMP EAX,9 / JNZ` after the device-init scorer call, and the smaller working bypass was NOPing that jump (see the bullets below).
- Repeat with `VGpu` enabled (same vGPU setup; network still disabled): the game again reached its own capability check and the guest screenshot shows the identical dialog, "Your Graphics Card does not meet The Godfather™ II minimum requirements."; the process exited with code `-1`. The guest logfile for this run recorded only the first observation tick (the screenshot capture turned off later scans), so the dialog evidence for this run comes from the captured screenshot plus the exit entry. Conclusion: the game's DirectX-capability check rejects the Sandbox display path in both `WARP` and `vGPU` modes, plausibly because the paravirtual adapter reports no dedicated video memory, exactly the "1998-era GPU" style criteria an RTS-era test would use. Bypassing this one check in the disposable test copy (derived from the SE-exe decompilation, test copies only, never distributed) is the next unblocking step. vGPU log: `local/sandbox-output/gf2_se_boot.txt` (overwritten by the vGPU run).
- Root-cause chain completed (2026-10-20, test copies only). **Display init:** `FUN_00607990` builds `D3DPRESENT_PARAMETERS` via `FUN_006073e0` (called with the `Window.*` globals) or falls back to `FUN_00608b50` (`memset` PP; `Windowed = (DAT_0120587f == 0)`; format `A8R8G8B8` if fullscreen else 0). Fullscreen branch min-clamps settings up to the smallest enumerated mode and then picks a fitting mode; on a one-mode virtual display it lands on `2032×1074@32`. `CreateDevice` is the official vtable slot +0x40 with `pp = 0x011d9188` (the runtime-filled `.data` BSS area — my earlier "static PP" read was an address arithmetic slip: `0x011d9188 − 0x400000 = 0x00DD9188`, beyond the section's raw data). Failure → `PUSH 1` at `0x00607b85` = "Could not initialize display hardware". **Startup settings:** `FUN_0068d010` reads `Window.Fullscreen` default **1** (`FUN_0069bc30("Window.Fullscreen",1)`; disasm `PUSH 0x1` at VA `0x0068d1d2`), `Window.Width` default `0x400`, `Window.Height` default `0x300`, `Window.Hz` default `0x3C`, and applies them via `FUN_0069e3e0` (Creates `Godfather2WndClass`) → `FUN_0068bd30` (renderer init). Cmdline `-windowed`/`-fullscreen` override the flag but alone did not save the boot — the guest failure reproduced. **Probe v5 (guest, vGPU):** added game-shaped PPs and native-mode fullscreen variants — windowed works for every shape tested (A8R8G8B8/X8R8G8B8, W/H 0 or 800×600/1024×768/2032×1074, `IMMEDIATE`, autodepth D24S8, SWVP/HWVP/MIXED), while **exclusive fullscreen fails `D3DERR_INVALIDCALL` in every variant, including at the adapter's own enumerated mode `2032×1074@32` with `hz` 0/32/60 and both formats** — the Sandbox paravirtual display cannot switch exclusive modes at all. Conclusion: the game must run windowed in the guest. **Boot v4** (test copy patched `Window.Fullscreen` default 1→0 at file offset `0x28D1D3`): the device-init dialog disappeared; new crash `0xC0000005` (WER: faulting module `godfather2.exe`, offset `0x0029F57F` → VA `0x0069F57F`). **Crash analysis:** `FUN_0069f440` is a legacy GPU-vendor detector called by `FUN_0069f8b0` (quality-options init) only when `QualityOptions.ConfigType` is absent from settings; it does `strstr(DeviceID,"VEN_")` and dereferences the result +4 **without a NULL check** — the paravirtual adapter's `DeviceID` has no `VEN_` → access violation. **Boot v6** (test copy additionally patched `FUN_0069f440` entry to `MOV EAX,4; RET` — "unknown vendor" path): **the game boots windowed and reaches its interactive language-selection screen** (English and several other European languages), stays responsive with CPU accumulating for the full 160-second harness window, screenshots `gf2_se_boot_T20/60/120.png`. Guest `settings.txt`/`cmd.txt`/`joypad.txt` were absent after the run; the SE loader wrote an empty `GF2SE.txt` marker. Both patches exist **only** in the disposable test copy (`godfather2-test-se`), whose current SHA-256 is `201269a2…`; the pristine verified bundle stays untouched.
- **Milestone (2026-10-20 23:27): the game reaches its main menu inside Windows Sandbox.** After a manual language choice in the Sandbox window (the language picker could not be confirmed by injected input), the main menu appeared with `Resume Game / Play Online / New Game / Options / Unlocks / Credits / Bonus Materials / Select Profile / Quit to Windows` and version `1.0.789.0` bottom-left. Screenshot `local/sandbox-output/seq_T150.png` (run v10, log `gf2_se_boot.txt`). Input findings from those runs: the window proc `FUN_0069de10` forwards mouse messages to `FUN_0069eb50`; `WM_MOUSEMOVE` moves the menu highlight only in relative/delta mode (`DAT_0112a065` capture flag), `WM_LBUTTONDOWN` is unhandled, and `WM_LBUTTONUP` toggles mouse capture via `FUN_0069e8b0` — injected clicks moved the highlight to Polski without confirming. The guest's OS cursor is pinned by the Sandbox RDP input path (`SetCursorPos` has no effect; `GetCursorPos` stayed 515,410), so a real host click is the reliable input path for now; automated input for QA will need a different channel.
- **Settings and saves location resolved (2026-10-20).** The game does not keep `settings.txt` in the game folder: it lives in `%LOCALAPPDATA%\EA Games\<localized title>\settings.txt`, where the title folder is a localized name. Format is plain text `Key = value` lines with CRLF; the staged settings file (871 bytes) contains `Window.Fullscreen = false`, `Window.Width = 1280`, `Window.Height = 720`, `Window.VSync = 0`, `Window.Gamma = 0.80000001`, `QualityOptions.ConfigType = 4`, plus audio/controls/quality keys. This is the file-based equivalent of both test-copy patches: `Window.Fullscreen = false` removes the exclusive-fullscreen attempt, and `QualityOptions.ConfigType = 4` (non-zero) skips the crashing GPU-vendor detector in `FUN_0069f440`. Campaign saves live in `Documents\EA Games\<localized title>\`: `profiles.dat` (1296 bytes; magic `9A E8 0D 1D`, version 4, one profile entry) and a `<profile-id>` folder (`3280000`) with localized save files and an autosave. Copies of the settings and saves were staged (originals untouched) under `local/sandbox-input/gf2profile/`, and the sandbox boot script now copies them into the guest profile under both the English and localized title folder names before launch. With the staged settings + profile, the language picker no longer appears and the main menu shows an active **Resume Game**; screenshot `local/sandbox-output/seq_T90.png` (run v12).
- **ASI plugin hook validated (2026-10-20).** The SE bundle's `dinput8.dll` is the Ultimate ASI Loader (UTF-16 strings `ASI Loader`, `*.asi`, `scripts\`, `\modloader\modloader.asi`). A minimal test plugin (`local/cooptest.c` → `cooptest.asi`, PE32 i686, DllMain writes one marker line, no hooks) was placed in the disposable test copy's `scripts\` folder. Boot run v12 produced `local/sandbox-output/cooptest_loaded.txt`: `cooptest.asi loaded; game=C:\Games\GF2Test\godfather2.exe; module=C:\Games\GF2Test\scripts\cooptest.asi` — **the loader loads third-party ASI plugins from `scripts\`**, which is the injection point a co-op plugin can use (same mechanism the SE extender and the Multiplayer Revival ASI rely on).
- **Milestone (2026-10-21 00:08): campaign gameplay running inside Windows Sandbox.** Root cause of the disabled `Resume Game`/unknown profile was the game locale: the game writes `Locale = en` into its settings on first run, while the staged saves use a different locale. After adding the `Locale` key with the saves' language to the staged `settings.txt` (and keeping the staged `profiles.dat` and save folder in the localized `Documents\EA Games` folder), the profile was picked up and the game loaded straight into the campaign - screenshot `local/sandbox-output/seq_T60.png` shows in-game HUD (money, portrait, health bar, a crew NPC, minimap) with no Cuba intro. `profiles.dat` layout: magic `9A E8 0D 1D`, version 4, profile count 1, wide profile name, profile id (the decimal save-folder name). `settings.txt` is read/written under `%LOCALAPPDATA%\EA Games\<localized title>\`; saves and `profiles.dat` under `Documents\EA Games\<localized title>\`. Locale switching selects the localized title folder. The Sandbox guest was stable through the load (responsive, CPU accumulating, no WER events).
- **Input (confirmed, 2026-10-21): keyboard menu navigation works** in the Sandbox (arrows + Enter). The mouse path stays RDP-bound (`SetCursorPos` ineffective, injected clicks unreliable), so keyboard is the intended automation channel for menus; the next test injects `WM_KEYDOWN`/`WM_KEYUP` (`VK_UP` ×8 then `VK_RETURN`) at the main menu and checks the highlight/screenshots to see whether message-injected keys are honoured or whether the engine reads real key state only.
- **Automated boot → menu → Resume Game → campaign (2026-10-21 00:25).** Run v14 proved injected `WM_KEYDOWN`/`WM_KEYUP` (posted to `Godfather2WndClass`) reach the engine and drive the UI: at 40 s the localized splash was up, the injected `VK_UP` presses dismissed it and moved the menu selection, and `VK_RETURN` at 60/80 s activated **Resume Game** — a loading screen with the city map appeared, then the mission's opening cutscene (character close-up, later a burning car), all without any manual input (`menu_after_up_4/5.png`, `menu_after_enter_6/8.png`, `seq_T240.png`, `seq_T450.png`). The game UI switched to the saves' language once the `Locale` key matched, confirming locale-controlled data folders. This gives a fully automated path into the campaign for QA runs; the next open question is whether in-game movement also accepts injected keys or only real input (DirectInput).
- **Full manual walkthrough captured (2026-10-21 01:04, run v17, 5 s frames `seq2_1..34`).** Screen sequence: black boot -> bright splash -> **main menu (localized)** -> **the profile entry** -> profile list containing exactly one profile entry (`profiles.dat` stores it as a wide string and the profile id `0x00320C80` = folder `3280000`) -> back to the main menu with **the resume item** highlighted -> **straight into gameplay in the Corleone mansion** (Dominic, `19$`, minimap, NPCs) with **no Cuba intro**. Earlier automation attempts failed because `Resume Game` was pressed without selecting the profile first, which starts the intro sequence instead of the save. The Sandbox mouse behaves oddly (RDP input) while the game is fine on the normal desktop.
- **Unattended screen-state machine (2026-10-21, run v18).** To make the boot-to-campaign path fully automatic, the guest script now classifies the screen every 5 s by a 4×4 grayscale signature of the game window region (host-side reference tool `local/screenstate.ps1`; reference signatures embedded in the script for MENU, PROFILE and SPLASH) and drives a phase machine: phase 0 splash → `VK_UP`; phase 1 menu → `VK_DOWN`×6 + `VK_RETURN` (open profile screen); phase 2 profile → `VK_UP`×2 + `VK_RETURN` (select "22"); phase 3 menu → `VK_UP`×8 + `VK_RETURN` (Resume Game); phase 4 load → no further input; phase 5 gameplay reached. Retries are bounded (max 3/4/6 attempts) and the profile step only ever sends `VK_RETURN` after the first `VK_UP` so the the create-profile button cannot be hit accidentally. Every decision is logged with the three signature distances in `local/sandbox-output/gf2_se_boot.txt`.

## Research log

### Checked

- Repository `DIRECTORY.md` and `games/`: no Godfather II entry before this note.
- Opened the required claim issue: <https://github.com/cranebeaker/coop-anything/issues/78>.
- GitHub issue and PR search for `Godfather 2`: no results from the public API. The required `gh issue list` / `gh pr list` commands could not run because GitHub CLI (`gh`) is not installed.
- GitHub repository searches for `Godfather II coop`, `godfather2 multiplayer`, and `The Godfather II mod`: no matching repositories returned.
- English web searches for Godfather II co-op campaign, co-op mod, online co-op, and multiplayer mod; checked Nexus Mods, ModDB, GameBanana, GameFAQs results, PCGamingWiki, and press coverage. The concrete results were the multiplayer restoration and modding tools above; no campaign co-op project surfaced.
- Attempted Russian, German, Italian, and Spanish searches; the search service returned HTTP 429, so those searches did not complete.

### Not checked yet

- The game's Discord/community server, Steam guides in depth, Archive.org, and full bug/issue threads outside the Nexus pages.
- The clean copy's store/package ID, the SE's detailed compatibility/API, and the original multiplayer's replication of AI/story/world state.

## Assessment and next steps

- **Derived:** the existing revival is a way to restore the game's original online match modes, not a solution for the requested story campaign. No campaign co-op attempt was found in the searches above; this is a search result, not proof that none exists.
- **Stop point:** `1.0.764.0` is the original game executable, but its PE sections and `paul.dll` indicate SecuROM protection. Ghidra analysis of this clean exe produced invalid references/function overlaps and exited `-1`; the partial decompilation is unusable and was not used. No unpacking/decryption was attempted. The `1.0.789.0` MPR executable is not a clean baseline either.
- **Next:** the target build must be chosen explicitly. The SE v0.56 archive has a hash-matching manifest, but the `1.0.789.0` exe inside it is an unverified third-party binary and requires that build, so a co-op attempt using its loader means switching the boxed target to a `.789`-based copy, while a `.764`-only path means building our own loader and losing both mods' hooks (static analysis of the SecuROM exe stays blocked either way). Whichever path is chosen, a clean Sandbox first needs the legacy DirectX 9.0c runtime (`d3dx9_40.dll`, `XINPUT1_3.dll`) installed in the guest, which needs one official Microsoft download. The claim issue is open (#78).
- **Unknown:** store (Steam, EA app, disc, or other), whether friends have matching copies, the provenance/licensing of the community `.789` builds inside SE and MPR, and the SE author's permission to reuse code. Executable hashes are kept in ignored `local/identity.md`.
- **Checks actually run:** copied the clean candidate into an isolated test folder; read executable version metadata and SHA-256; statically read PE headers/imports and printable strings; Ghidra 12.1.4 headless with Temurin JDK 21.0.12.1+1 decompiled 84 functions from Multiplayer Revival and 33,506 functions from the MPR-bundled executable. A Ghidra run on clean `1.0.764.0` exited `-1` with extensive invalid-flow/function-overlap warnings; partial output is not used. A process-only shell-folder probe showed changing `USERPROFILE` does not redirect `SHGetFolderPathW(CSIDL_PERSONAL)`. Four offline Windows Sandbox runs (two plain boot probes, one vGPU, one all-window scan) identified the missing legacy DirectX runtime behind the blank boot. The SE v0.56 archive was SHA-256-verified against the Nexus manifest and inspected; it was not installed on any game copy. No two-client or co-op test was run.

## Sources

- <https://www.pcgamingwiki.com/wiki/The_Godfather_II>
- <https://www.nexusmods.com/thegodfather2/mods/95>
- <https://www.nexusmods.com/thegodfather2/mods/40>
- <https://www.ign.com/articles/2009/03/18/the-godfather-ii-gets-multiplayer-mode>
- <https://github.com/Greavesy1899/VisceralToolkit>
- <https://github.com/Greavesy1899/VisceralToolkit/wiki/File-Resource-Type-Support>
- <https://github.com/Greavesy1899/VisceralToolkit/wiki/Resource-Types>
- <https://www.pcgamingwiki.com/wiki/Dead_Space_3>
- <https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file>
- <https://www.microsoft.com/en-us/download/details.aspx?id=8109> (DirectX End-User Runtimes June 2010; official source for the missing `d3dx9_40.dll` / `XINPUT1_3.dll` components identified in the Sandbox log)
- <https://github.com/Winetricks/winetricks/pull/1690> (independent record of the same SHA-256 `053f76dc…` for the Microsoft-republished June 2010 redist)
- <https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build>
- <https://github.com/adoptium/temurin21-binaries/releases/tag/jdk-21.0.12.1%2B1>

- **Host-based iteration (2026-10-21 ~01:30) - the working setup.** Sandbox RDP input is the bottleneck, so the iteration target moved to the **host**: the disposable copy `%LOCALAPPDATA%\Temp\opencode\godfather2-test-se` is launched directly on the test machine (1280x720 windowed, fast boot) and the game sees a pre-existing profile with its campaign saves. A driver script `local/host-drive.ps1` watches `local/host-cmd.txt` and executes commands against the game window (`ENTER/UP/DOWN/ESC/SPACE/W/A/S/D`, `VK <code> <n>`, `CLICK x y`, `MOVE x y`, `SHOT/WIN label`, `EXIT`), logging to `local/host-drive.log` and capturing frames into `local/host-shots/`. Window lookup uses `EnumWindows` + class match (on the host `FindWindowW` returns nothing). **Verified unattended route on the host:** `ENTER` (dismiss the splash notice) -> attract mode -> **mouse click on the profile entry** -> confirm the autosave notice -> **click resume** -> campaign loads (New York map, first objective "Sweet Life Bakery") with **no Cuba intro**. Keyboard alone cannot confirm the profile (focus sits on the "create profile" item, so `VK_RETURN` there would create a new profile) - the profile item must be clicked, and on the host real mouse clicks work; that is the concrete advantage over the Sandbox. Frames: `local/host-shots/003_after_enter.png` (attract), `001_after_click22.png` (autosave notice), `003_after_next2.png` (main menu), `005_loaded.png` (campaign map). A backup of the machine's EA Games folders (with SHA-256 manifest) was taken before the first host run; all five files were still byte-identical afterwards.
- **In-process hook work (2026-10-21 ~01:50), state of `local/coopcore.c` -> `coopcore.asi`.** The plugin is loaded by the SE loader (Ultimate ASI Loader) from `scripts\` and runs inside the game process: it logs its load point, keeps a heartbeat thread, and logs frame counts. Hook attempts: (a) inline hook of `FUN_00607990` (device init) **crashed the game** - that function takes arguments and the wrapper did not forward them - replaced by polling the device-pointer global `DAT_01205750`; (b) a per-object vtable hook of `IDirect3DDevice9::Present` (slot 17 of a 175-entry d3d9 implementation vtable) **also crashed the game** - copy-and-swap is unsafe here, so it was dropped; (c) an inline hook of `FUN_006080a0` installs cleanly and the game keeps running, but that function is only used on the device-reset path (`FUN_00608250`), not per frame, so the counter stays 0. The device vtable was verified to live entirely inside `d3d9.dll` (base `0x709D0000`, 175 entries), so the object is the raw D3D9 device, not an engine wrapper. Next: IAT hooks on `GetMessageW`/`PeekMessageW` for a reliable per-frame tick.
- **Scripting API found (2026-10-21 ~02:00).** The game registers a script/behavior API (`FUN_00468cb0`): `RegisterScriptName`, `GetPositionValues`, `SetPositionValues`, `GetPlayerPosition`, `TEMP_POS`, `CreatePosition`, `GetPlatform`, plus `GetPlayerInfo` and Lua hooks (`iMsgLuaGetPlayerVenue`, `FUN_00408240`). This is the natural co-op hook: the engine already exposes player position read/write to scripts, and the SE extender can inject behaviors through SimGroup `.sgp` override files.

- **Player object located (2026-10-21 ~02:40).** Method: a memory scanner inside the plugin (own code) diffs the exe's `.data` section (VA `0x00E50000`-`0x01228A38`) once per second and reports triples of neighbouring floats that change together; with the character moving in a level, world-magnitude triples appeared and were matched to the decompilation. Result: the engine keeps the **camera look-at transform - i.e. the player character - at `0x0112A840`** (`x,y,z` floats at `0x0112A840/44/48`, an angle float at `0x0112A84C`) and the **camera eye at `0x0112A850`** (`0x0112A850/54/58`, angle at `0x0112A85C`). Source in the decompilation: the camera update `FUN_006d24d0` (VA `0x006D24D0`) copies its eye/target transforms into those globals (`_DAT_0112a850 = *piVar11; _DAT_0112a840 = param_1[0xc];`) and the camera follows the actor pointer at `param_1[0x36]` (its vtable slots `+0x10`/`+0x14` return the eye/look-at transforms). The plugin now logs the player state at 5 Hz to `coopcore_player.txt` next to the game (`player x y z | lookang | cam x y z | camang`).
- **Input reality on the host (2026-10-21).** Synthetic mouse input does **not** reach the game's UI: the window grabs the mouse (`ClipCursor`), the engine reads the pointer through Raw Input, and `SetCursorPos`/`mouse_event` are ignored; `PostMessage`-based `WM_MOUSEMOVE` only moves the menu highlight while the engine is in its captured-cursor mode (`DAT_0112a065`), and `WM_LBUTTONDOWN` is not handled by the engine window proc at all. Keyboard input works in the main menu (confirmed) but not on the profile screen. Consequence for the project: menu automation must either use real input or be done from inside the process (our plugin), not through synthetic OS input.
- **Plugin build notes (2026-10-21).** `local/coopcore.c` -> `coopcore.asi` (PE32 i686, LLVM-MinGW) is staged into the disposable copy's `scripts\` folder; the SE loader (Ultimate ASI Loader) loads it. Lessons that cost crashes: hook wrappers must forward the callee's arguments; do not swap the D3D9 device vtable (175 entries) - it kills the game; read import names from the INT (`OriginalFirstThunk`) and patch the IAT (`FirstThunk`); an exception in a plugin thread kills the whole process, and MinGW has no SEH, so every pointer walk needs bounds checks; the scanner must stay inside the committed `.data` range; and the float formatter must be `sprintf`, not hand-rolled digit writing.

- **Player transform and actor pointer (2026-10-21 ~03:00).** The engine composes the player world matrix every frame in `FUN_0088d6e0` (VA `0x0088D6E0`, `__fastcall`): it reads the actor position (`*(undefined8 *)(iVar6 + 0x30)` and `*(float *)(iVar6 + 0x38)`) and stores the composed 4x4 matrix into the global `0x0112E210` (`_DAT_0112e210 = local_80[0] ... fRam0112e21c = local_80[3]`). The actor itself is `FUN_00471610(context) = context + 0xD0`, and the context is the global world/actor manager `DAT_012233A0` (which also holds an actor array at `+0x04` and a count at `+0x0C`). Consequences: **player world position = the first row of the matrix at `0x0112E210`** (x,y,z,1) and the **actor object's own position is at offset `+0x30`**; the camera look-at/eye globals (`0x0112A840`/`0x0112A850`) are only filled in some camera modes (they stayed zero in the mansion interior). The plugin now streams `matrix | actor | cam` at 5 Hz into `coopcore_player.txt` and cross-checks the matrix against `*(manager + 0xD0) + 0x30`, which is the anchor a co-op puppet needs: write the same transform into a second actor every frame.

- **Two more plugin bugs, both worth remembering (2026-10-21 ~03:40).** (1) **Trampoline off-by-one:** the jump back to the original function must be computed from `trampoline + 11`, not `trampoline + 10` - the `E9 rel32` occupies bytes 6..10, so the next instruction is at +11. With the wrong base the CPU landed in the middle of an instruction (`IN AL,DX` -> privileged-instruction fault `0xC0000096` inside `FUN_0088d6e0`); for another hook it happened to land on a harmless byte and stayed unnoticed. (2) **Every pointer arithmetic needs a bounds check**: reading `camera + 0xD8` without validating the object size raised an access violation *every frame*; the vectored exception handler swallowed it and the game hung on a black screen instead of crashing. The plugin now validates each pointer with `is_readable(ptr, need)` (committed region + the access fully inside it) before dereferencing, and the VEH only swallows up to 5000 faults and logs every 50th. With both fixed the game runs stable and the camera hook reports the followed actor (`camera follow actor = 1af83940`).

- **Main menu and campaign reached on the host (2026-10-21 02:00-04:00).** With the staged profile (settings + saves) the game reaches the main menu (build `1.0.789.0`, screenshot `local/host-shots/003_after_next2.png`) and the campaign itself (New York map with the objective "Sweet Life Bakery", then the mansion with HUD, `local/host-shots/005_loaded.png` and later frames with the character walking in the street and an NPC in conversation). No Cuba intro - the existing profile is loaded and its save resumes.
- **External inspector `local/coopscan.c` -> `coopscan.exe` (own code, PE32).** Attaches to the running `godfather2.exe` with ReadProcessMemory/WriteProcessMemory and needs no restart and no in-game hook: `find` prints the player matrix and hunts for character objects, `dump <addr>` hex-dumps an object, `list` walks the world manager, `puppet <actor> [dx]` copies the player matrix row into an actor every 16 ms. This replaced the "crash the game and restart" loop the in-process plugin had.
- **Do not write into engine memory blindly (2026-10-21 04:30).** Writing a position into the object the camera follows crashed the game: `0xC0000005` at `0x4D006874` with the faulting module "unknown" - the `+0x40` of that object holds a **pointer**, and overwriting it with floats left the engine dereferencing garbage. The camera's follow object is not the player character. The decompilation-backed path is: hook `FUN_0088d6e0` to capture its context, the player actor is `context + 0xD0` and the actor's position is at `actor + 0x30`; the plugin now does exactly that and validates every pointer with `is_readable()`.
- **Input reality, final (2026-10-21).** The engine reads the mouse through Raw Input: `SetCursorPos`, `mouse_event` and `WM_MOUSEMOVE`/`WM_LBUTTONDOWN` do not drive the UI (only the hover highlight moved in the Sandbox). The keyboard works in the main menu (confirmed) but not on the profile screen, which needs a real click. So menu automation has to come from a real device or from inside the process.
- **Game data layout (final).** `settings.txt` lives in `%LOCALAPPDATA%\EA Games\<localized title>\` (`Key = value`, CRLF, keys `Window.*`, `QualityOptions.*`, `Locale`), campaign saves and `profiles.dat` in `Documents\EA Games\<localized title>\` (profile id `0x00320C80` = folder `3280000`). The `Locale` key (`en`/`ru`) selects the language *and* the localized title folder - that is why the localized saves were invisible until the `Locale` key matched.

- **Puppet experiment (2026-10-21 00:45-01:00).** The character object of the camera target is at `0x14A17B20` with vtable `0x00D6B350`; the engine reads its position through the virtual slot `+0x48` (`(**(code **)(*(int *)(iVar6 + -0x1f30) + 0x48))(&local_b0)` in `FUN_0088d6e0`, where `iVar6 = **(int **)(DAT_012233a0 + 4)`). The world manager chain is `DAT_012233a0` -> `+0x04` (array) -> `+0x0C` (count) -> entry -> `-0x1F30` = the character object. The player's world matrix is `0x0112E210` (4x4, row 0 = x,y,z,1) and the camera look-at/eye are `0x0112A840` / `0x0112A850`. The external tool `coopscan.exe nearpos` finds every cached transform row (x,y,z,1.0) near the player; the closest hits are the player's own copies, other hits (`0x2448xxxx`) are per-character cached matrices. **Writing the player's position into such a cached matrix does not move the character** - the engine recomputes the authoritative transform from the character's kinematics every frame, and the write is overwritten. The in-process puppet (plugin hook on the camera update `FUN_006d24d0`, config `coop_puppet.cfg` with `enabled/target/offset/dz`) does keep the value in the cache (the NPC's cached row followed the player exactly), but the renderer still uses the kinematic transform, so the model does not move yet. Two sanctioned ways forward: call the character's own position setter (the getter is vtable slot `+0x48` of `0x00D6B350`, a setter is likely in the same vtable) or drive the engine's script/behavior layer, which already exposes movement commands (`moveToPos`, `moveToPathingFailure`, `combatMoveToCSARange`, ... registered around `0x004acd20`/`0x004acc70` in the decompilation) plus the script API `SetPositionValues` / `GetPlayerPosition` / `GetPlatform` (`FUN_00468cb0`).
- **Tooling built (all own code, nothing redistributed):** `local/coopcore.c` -> `coopcore.asi` (in-game plugin: 5 engine hooks, frame logger, path redirect for profile isolation, puppet writer, VEH safety net), `local/coopscan.c` -> `coopscan.exe` (external inspector: `find`, `list`, `dump`, `chars`, `nearpos`, `snap`, `diff`, `puppet`), `local/poolscan.ps1` (container dump), `local/host-drive.ps1` (host-side input/screenshot driver: keys, real mouse clicks, frames), `local/bytepatch.c` (exe byte patcher), `local/dx9probe.c` (D3D9 diagnostics).

- **Character vtable analysed (2026-10-21 01:00-01:20) - the open end for the puppet.** The camera target's class vtable is `0x00D6B350`. Slot `+0x48` is a position **getter thunk**: `81 C1 C0 03 00 00 / E9 25 52 D5 FF` = `ADD ECX,0x3C0; JMP 0x0054DBEA`, so the position lives behind `this+0x3C0` (possibly inside a sub-object). Slot `+0x44` is a boolean state getter (`XOR EAX,EAX; CMP [ECX+0x220],0; SETNE AL; RET`). Slot `+0x4C` is a dispatcher that resolves a sub-object at `this+0x1E40` (+0xB8) and calls its vtable slot `+0x4C` - a good candidate for a **setter**. The world-manager chain is `DAT_012233A0` -> `+0x04` (pointer to an array) -> `+0x0C` (count) -> entry (e.g. `0x14A19A50`) -> `-0x1F30` -> the vtable `0x00D6B350`; the call site is `(**(code **)(*(int *)(iVar6 + -0x1f30) + 0x48))(&local_b0)` in `FUN_0088d6e0`. **`this` for that call was not yet identified** - the object at `0x14A17B20` holds that vtable but its `+0x3C0` is string data, so it is a different instance. The empirical search (`local/findpos.ps1`, matches the player's position inside an object) did not find it either. Next step: identify the caller of `FUN_0088d6e0` (the `this` it passes) or hook the getter `0x00D6B350[0x48]` in the plugin, capture `this`, and then read `this+0x3C0` to learn the layout; the matching setter should sit next to the getter in the same vtable.
- **State at the end of this session:** the game runs on the test machine from the disposable copy, the campaign loads directly (no Cuba intro), the plugin is loaded with five engine hooks, the player position is read at 5 Hz, character objects and their cached transform rows are found in the heap, and an in-process puppet writer exists but the authoritative transform of a second character is still unknown. The machine's profile, settings and saves are byte-identical to the pre-session backup, and the original installation is untouched (`godfather2.exe` SHA-256 `A5632356C94A270DAB518E0E1FA569AD40FD6520C1013CF99B023E95252AD338`).

## Handoff: how to continue (2026-10-21)

Working setup (host iteration, no Sandbox needed):

1. Test copy: `%LOCALAPPDATA%\Temp\opencode\godfather2-test-se` (exe SHA-256
   `201269a29eef055c050cd1e17a4faee24df888544d8f09299bee156050c601bc`; pristine SE
   bundle untouched in `godfather2-tools\inspect\se-v0.56`).
   The plugin lives in `scripts\coopcore.asi`; the SE loader picks it up.
2. Start the game: `Start-Process %LOCALAPPDATA%\Temp\opencode\godfather2-test-se\godfather2.exe`
   (working dir = the copy). The profile/settings/saves are redirected by the
   plugin into `<copy>\profile\`, so the machine's own files stay untouched.
3. Menu path (mouse/keys are RDP-limited in the Sandbox but normal on the host):
   any key on the splash -> select the existing profile -> resume the saved game -> a mission.
4. Tools (all own code, under `local/`):
   - `local/coopscan.exe` - external inspector/writer for the running game:
     `find` (player state), `list` (world manager), `dump <addr>` (hex dump),
     `chars` (objects of the character vtable), `nearpos <radius>` (transform rows
     near the player), `snap <file>` / `diff <file>` (heap snapshots),
     `puppet <actor> [dx]` (write the player position into an actor).
   - `local/findpos.ps1 <obj>` - locate the player position inside an object.
   - `local/poolscan.ps1` - dump the character container.
   - `local/host-drive.ps1` - input/screenshot driver (keys, real mouse, frames).
   - `local/bytepatch.exe` - byte patcher for the test exe.
   - `local/dx9probe.exe` - D3D9 environment probe.
   - `local/coopcore.c` -> `coopcore.asi` - the in-game plugin (hooks, logger,
     profile redirect, puppet writer, VEH safety net).

Immediate next task (the only open end): identify the authoritative transform of a
second character. Concretely - hook the position getter `0x00D6B350[0x48]`
(`ADD ECX,0x3C0; JMP 0x0054DBEA`) in the plugin, log its `this` and the value at
`this+0x3C0`, and then look for the matching setter in the same vtable (slot
`+0x4C` looks like a dispatcher to a sub-object). Once the authoritative slot is
known, writing the player's position into it turns the NPC into a visible
"player 2" puppet - the base primitive for the networked second player.

### SE NPC Inspector as an oracle (2026-10-21 session 2)

The SE mod's ScriptHook menu has an **NPC Inspector** window. Observed on crew
member "Joe Primanti" (values read from inspector screenshots):

- `SimNPC: 1B603A00`, `Supporting Entity: 1B8F1200` - both heap handles; neither
  object's first 0x1000 bytes contain the displayed position (checked with
  `findpos.ps1`).
- Position shown with 6 decimals (e.g. `156.101730 10.222837 -220.644165`);
  it is **derived, not stored**: `findpos ALL` (full RW scan, tol 0.02) finds no
  triple anywhere in the process. Matches `FUN_0054dbf0` = `(min+max)*0.5` of an
  AABB from the node at `*(this+0x18)`, slot `+0x18`. So the authoritative state
  is the AABB min/max (or a node transform), and the displayed position is its
  center.
- `Supporting Material Type` / `Flags` change between inspections (25/1FF0000 ->
  4/1FF0000) - material under the feet, useful as a ground-truth check.

New experiment (running): `local/nearwatch.c` -> `nearwatch.exe x y z seconds r`
records every float triple within `r` of the inspector position, waits while the
the companion is walked, then reports old->new per triple. The companion's real
movement is the cluster of slots whose delta equals the movement vector between
two inspector screenshots. `local/delta-analyze.ps1` groups the report by delta.
Rationale: whatever the representation (AABB corners, node matrix row), a pure
translation shifts it by the same delta, so the diff-by-delta technique is
representation-independent.

Tooling added: `local/nearwatch.c` (watcher), `local/delta-analyze.ps1`
(delta clustering).

### Character position slots found (2026-10-21 session 2, second half)

Method that worked: `local/findtrip.exe <x> <z> <ylo> <yhi> [tol_xz]` - full RW
scan for float triples matching (x, ?, z) with a LOOSE y window. Why the earlier
searches failed:

1. `findpos.ps1` required all three components to match, but the engine stores
   the position at several heights (feet / center / head). The NPC Inspector
   displays the FEET position: inspector y=10.010 vs player matrix y=11.145
   (difference 0.945 = the feet offset). So a tight-y search can never match the
   inspector value against center-height slots.
2. `findpos ALL` had a 25 s deadline and a 64 MB region cap - it timed out before
   reaching the 0x43xxxxxx allocations where most copies live.

Observed slot families (game running, 1.0.789.0):

- PLAYER (center at matrix 0x0112E210, y=11.145 case): static .data copies at
  0x0019E670 (y = center-0.946), 0x0019E6C0, 0x0019EA40 (y = center+0.555),
  0x0019EA50 (y = center-0.555), 0x0019F370/0x0019F3E0 (z stored as z+1000!),
  0x0019F500, 0x0019F770, 0x0019F930, plus 0x0112A840 (camera look-at),
  0x01130620, 0x011F6E40, 0x012070A0, 0x012073A0 and 300+ heap copies in
  render/anim pools (0x22D5xxxx, 0x23883xxx, 0x24C13xxx, 0x3AC49xxx, ...).
- COMPANION "Joe Primanti" (SimNPC 1B603A00), inspector pos
  (30.933395 10.210006 -53.035728): 32 exact copies, none in .data - a record
  block at 0x43438958-0x4344492C plus pools 0x2270B230/0x2270D530/0x22712C30,
  0x43708C30/0x4370BF30/0x43724630. A pedestrian's family looked the same
  (5 coherent copies at 0x00199E50/0x0019A240/0x0019A250/0x0019A260/0x0019A6E0
  - these WERE in .data, so .data also holds some NPC records; the layout rule
  is still unknown).

The "delta hunt" (`local/deltahunt.c`) is the reliable way to attribute slots to
a moving character: record all RW memory, wait for a stop file, then report every
triple whose delta equals the player's (read from 0x0112E210). A companion that
walks a different path needs its own delta from two inspector screenshots.

Puppet write test (`local/puppetdrive.c`): external 100 Hz writer pushing
player+offset into the companion's 24 slot copies. Result: **no movement** -
the copies are read-only views; the engine recomputes kinematics every frame.
Second test (`local/puppetdrive2.c`) rigid-drives all 29 slots including the
likely skeleton bones (0x2633D930, 0x26345B30, 0x3CADCC30, 0x403465E0,
0x4343E928 - each offset 0.2..0.9 m from the feet position). Result: pending.

Fallback plan if bones also fail: catch the writer of a cache slot with a
hardware watchpoint (external mini-debugger: DebugActiveProcess + DR0 write
breakpoint) and walk the code path back to the authoritative source.

**Crash/hang lesson (2026-10-21):** the plugin's `apply_puppet` wrote 16 bytes
(float4 row ending in `1.0`) into slot 0x43438958 and **hung the game**. The
dword after a position triple is NOT padding - it holds a pointer/flag. All
position writes must be 12 bytes. External 12-byte writes (puppetdrive) were
safe. The plugin was rebuilt: `apply_puppet` now writes 12 bytes to a list of
targets from repeated `target <hex> [yoff]` lines in `coop_puppet.cfg`
(config is re-read every second, so the puppet can be reconfigured without a
game restart). The in-process write happens in the camera hook
(`FUN_006d24d0`), i.e. after the engine's per-frame update - the timing the
external driver could not achieve.

### What the position slots really are (2026-10-21, later)

- **The 0x0019xxxx band is THREAD STACK, not .data.** It lies below the exe
  base (0x00400000). The "player position copies" found there earlier were
  stack locals of the engine's own position code. Writing them corrupts the
  stack - that is what hung the game both times.
- Hardware watchpoint on 0x0019F500 (watchwrite.exe) caught the writers: the
  Apt UI framework (`c:\packages001_pc\apt\0.19.optimized\source\apt\_AptValue.h`)
  pushing locals, plus vector math at 0x0043A2EF/0x0043A47C/0x0043A653. All
  scratch, not the transform source.
- Real globals confirmed in the exe image: player matrix 0x0112E210 and camera
  look-at 0x0112A840. The matrix global is field +0x198 of a .data object
  (base 0x0112E078, vtable 0x00E32A84, instance name "WhiteboxCar"); a heap
  twin of every such object exists (0x0434E078 for the player). 17 pairs found.
- The companion's position is NOT in any vtable-prefixed object's first
  0x2000 bytes (findobj wide scan: 0 hits), and not stored as an AABB min/max
  pair (findaabb: 0 hits). It IS stored as ~26-54 float triples followed by
  1.0 (a matrix-row look), including a .data/heap pair (0x01163640 /
  0x04213640 in one session) - but the 4th float is not reliably w=1 (one slot
  read back 51.37), so these are 4-field structs, not clean matrices.
- Writing the companion's matrix rows in-process at CAMERA time is overwritten
  by the engine later in the frame (observed: the value snapped back).
  Moving the write to the D3D9 PRESENT hook (end of frame) was implemented;
  writing 26 such rows **crashed the game** - one of those fields is load
  bearing.

### Open end (handoff)

The authoritative transform write primitive is still open. Ranked next steps:

1. Hardware watchpoint (watchwrite.exe works and is read-only) on one of the
   companion's matrix rows while the companion WALKS: catch the writer code,
   then read that function in `godfather2_10789_se.c` to find the source it
   copies from. This is the decisive experiment and it needs no game writes.
2. Plugin hook of the position getter `FUN_0054dbf0` (prologue is exactly
   6 bytes: 55 8B EC 83 E4 F0) logging (node, position) - MUST record into a
   plain array with no sprintf/file I/O in the hook: the first attempt did
   5000 VEH-swallowed faults (in-plugin) and crashed the game.
3. Drive the engine's own behaviour layer instead of raw memory: the script
   VM registers `SetPositionValues` / `GetPlayerPosition` / `moveToPos`
   (`FUN_00468cb0`), and the SimNPC handle (inspector) is the entity to call
   them on. This keeps the engine's kinematics consistent and is the right
   long-term primitive for co-op.

Tooling inventory in `local/`: findtrip (position search, loose y, optional
w=1 filter), findval (32-bit value search), findobj (objects with a vtable
holding a position), findaabb (min/max pairs), watchwrite (hardware watchpoint
mini-debugger with register dump), deltahunt (whole-heap delta search),
nearwatch (watch triples near a point), puppetdrive/2 (external 100 Hz rigid
write), coopscan (dump/list/chars/snap/diff).

### Render pipeline vs game state (2026-10-21, later still)

Hardware-watchpoint run on 0x01163640 (the .data address that repeatedly holds
the companion's position) caught **one instruction, 0x004175BA**, inside
`FUN_00417560` - a 4x4 matrix multiply (`out = a * b`, written with `movaps
[edx], xmm5` in a 4-iteration loop). Register/return-address capture identified
three call sites: `FUN_00419b80 + 0x106` (0x00419C86) and `FUN_0041b9d0`
(0x0041BA8C). Their decompile shows the source is a per-thread array reached
through `fs:0x2C` (TLS), indexed by `*(entity+0x18)`, with rotation/scale at
`+0x6C/+0x70/+0x74`. So 0x01163640/0x01163610/0x011629d0 are **render
matrices**, and the .data/heap twins (0x01163640 <-> 0x04303640) are render
scratch, not game state. Writing them cannot move a character - consistent with
every failed puppet write so far.

Whole-heap delta search (deltahunt with the companion's own delta from two
inspector screenshots, -14.29/+0.04/-41.52 over a 44 m walk): **only two slots
in the entire process moved by that delta**, and one of them is the render
scratch above. Conclusion: **the companion's position is not stored as a float
triple anywhere** - it is a simulation entity whose position is integrated and
kept in a different representation (physics body, handle, or derived on demand).

Next step that follows from this: don't chase float triples. Catch the *read*
path instead - watch the SE NPC Inspector's own display buffer while the
inspector refreshes (the SE computes the value it shows, so its code reveals
the authoritative source), or hook the engine's script setter
(`SetPositionValues`) from the plugin and let the engine move the character.
### The SE inspector read path, statically decoded (2026-10-21)

Static analysis of `GF2ASI.asi` (a PE32 DLL, image base 0x10000000, no packing:
the inspector strings are plain ASCII at file offsets 0x0F0C3C-0x0F1434):

- The "Position:" label is at VA `0x100F21BC`, referenced from exactly one place,
  `0x10091802` (`push 0x100F21BC`). The surrounding code is the inspector's
  General tab: `lea eax,[ebp-0x48]; push eax; call 0x10074730` fills a 12-byte
  buffer, then three `cvtps2pd`/`movsd` pairs and a printf push it as three
  doubles.
- `0x10074730`: `call 0x10074680` with ECX = the inspected object, `add eax,0x30`,
  then `call 0x10073180` (coordinate conversion into the output). So the position
  is `object + 0x100`.
- `0x10074680` is the decisive one: it copies four `movups` from
  `ecx+0xD0`, `ecx+0xE0`, `ecx+0xF0`, `ecx+0x100` into a local 16-float matrix
  and converts each row. **A character's world transform is a 4x4 matrix at
  `object + 0xD0`; its 4th row is the position.** This matches the engine's own
  composer (`FUN_0088d6e0` branch A reads a position at `ctx+0x100`).

`findmat.exe` (new, local) scans for exactly that structure: a 16-float matrix
with orthonormal rows 0..2, `w = 1`, and the 4th row equal to a given position.
On the companion it finds 30 objects: three at feet height with the inspector's
exact value (`0x04396720`, `0x043968E0`, `0x229A9E30` in that run) and a series
of 27 at centre height (`0x22B0xxxx`-`0x22B8xxxx`) whose positions vary smoothly
inside a few decimetres - a movement history/trail, not a single transform.

Writing the player's position into the 4th row of the three feet-height objects
in-process at PRESENT (12-byte writes, `w` preserved) left the model standing on
its marker - these objects are not what the skeleton render reads. The next
cheap step: put the hardware watchpoint on one of the trail objects
(`0x22B0xxxx`) while the companion walks; its writer is the movement/AI code and
its source is the simulation transform that a puppet must write.
### Watchpoint on a "found" matrix: it is a transient buffer (2026-10-21)

Hardware watchpoint on the translation row of one of the 21 findmat matches
(`0x430B4B30`) fired 12 times at a single DLL address, `0x6EF6CC8E`, with
`rep movsb` in the bytes before EIP and a byte-at-a-time source cursor
(`ESI 0x015477BD..C0`), i.e. a **generic byte copy from a loading/streaming
buffer** - not a character update. So the "orthonormal 4x4 with the right
translation" matches in that heap region are transient buffer contents that
happen to satisfy the test; value-based matrix hunting is exhausted.

The solid result stands: the SE inspector proves the *layout*
(`object+0xD0` = world matrix, 4th row = position at `object+0x100`). What is
still missing is the *object identity*: which heap object is the companion's.

Recommended next step (well-scoped, no more fishing): the SE keeps its
inspector target in its own `.data` (image base 0x10000000, section .data at
VA 0x10179000). Find the static pointer the inspector code reads
(`movl (%edi), %ecx` at 0x100917CB) by tracing back from 0x10091802's function
prologue and from `0x10179400`-style statics, then read the target object and
its +0xD0 matrix directly - that is the object the SE itself trusts, and the
same object a puppet should write. Alternative with higher long-term value:
hook the engine's `SetPositionValues` script setter and let the engine move the
character.
### Inspector chain traced; why Present-time writes cannot work (2026-10-21)

- The inspector entry function is `0x10091740` (`movl %ecx,%edi` at +0x2B, so
  `this` -> EDI). It has exactly one caller, `0x1008F891`, which passes
  `lea 0x10(%edi), %ecx` - i.e. the inspector context is `inspector_object+0x10`,
  and `*(context)` is the inspected target object (the one whose `+0xD0` matrix
  and `+0x100` position the SE displays).
- The inspected object is NOT the SimNPC: `SimNPC+0xD0` holds handle/registry
  data (`3, 0, ptr, 0, ...`), not a matrix. The target is a different entity.
- The strings `SetPositionValues` / `moveToPos` / `GetPlayerPosition` do not
  exist in GF2ASI.asi: the SE calls engine functions directly, not through the
  script names. "Teleport" in the ASI is only a menu button label (referenced
  from 0x1008D6F0 and 0x1008F793, both UI-registration calls).

**Why every direct transform write failed:** the renderer poses the skeleton
earlier in the frame than our PRESENT hook. Writing the world matrix at PRESENT
is therefore one frame too late for the skeleton, and writing earlier is
overwritten by the engine's own kinematics. The character can only be moved
through the engine's own update path (its setter / script behaviour), not by
poking a matrix. This is the conclusion of the whole transform hunt and it
re-prioritises the work: the next task is to locate the engine's setter
(the function the game itself calls when an NPC is told to move to a position),
e.g. by watching the write to `entity+0xD0` earlier in the frame - set the
watchpoint on a *known* entity's matrix and let the *engine* move that entity
(an NPC walking on its own, or the player teleporting), then read the call stack
of the writer - rather than by writing and observing.
### The SE has no direct "set position" call (2026-10-21)

Scanning GF2ASI.asi .text for `E8` calls whose target lands in the game's
address range (0x00400000-0x01230000) yields exactly six sites:
FUN_005d9ab0+0xAA, FUN_005d93e0 (a LinkedObject-style constructor),
FUN_004b6060 (a small object ctor), FUN_0094bf90 (a UI/param ctor),
FUN_0059fd60+0x2DF and FUN_005167d0+0x8EB. None is a position setter: the SE
reaches game functionality through the UI/script layer, not through a direct
engine call. So there is no shortcut to the setter inside the SE.

Consequence for the next session: implement the call into the game's own script
VM from our plugin. Relevant engine pieces (all in the decompiled corpus):
`FUN_00468cb0` registers the script names, `FUN_00468ea0` shows the call
pattern (`iVar1 = *(int *)(DAT_012233f4 + 0x18)`, `FUN_00625ca0` push name,
`FUN_00633cb0` lookup, `FUN_00636570` invoke). Decoding those three helpers and
calling `SetPositionValues` / a `moveToPos` behaviour on the companion's SimNPC
handle is the concrete, bounded next task - and it is also the right primitive
for networked co-op later.
### Script VM: object located, calling conventions disassembled (2026-10-21)

Live addresses in the running game: `DAT_012233F4` holds `0x04750C4C`; the VM is
`*(0x04750C4C + 0x18)` = `0x1ABC0108`. VM layout: `+0x08` = stack pointer,
`+0x10` = stack-limits object (`+0x20`/`+0x24`), `+0x40` = the globals table
(first dword 5 = table tag, `+0x44` = table object).

Exact calling conventions, read from the real exe with llvm-objdump:

- `FUN_00625ca0` = **cdecl(vm, const char* name)**. Prologue
  `push ebp; mov ebp,[esp+0xC]` -> arg2 = name; `mov eax,[esp+8]` -> arg1 = vm.
  Pushes a value on the VM stack: writes type 4 and a string id, advances
  `vm+0x08` by 8. A NULL name pushes a zero value.
- `FUN_00636570` = the invoke. **cdecl with 3 stack args** (a continuation code
  pointer, a pointer to the prepared frame, a byte length) **plus the VM in
  ESI**, i.e. a custom register convention. The only call site in the game
  (`0x00468F2B`) pushes `0x6261E0`, `lea eax,[esp+0x10]`, and
  `stack_top - args_start`, then `add esp,0x10`.
- `FUN_00633cb0` = cdecl(vm, table_value, key_value, depth) lookup, returns a
  pointer to the resolved value.

Because the invoke takes the VM in ESI, a plugin call needs a small assembly
stub (set ESI = VM, push the 3 args, call 0x00636570) - not a plain C call.
Plan for the implementation session: mirror the exact sequence of
`FUN_00468ea0` (push name -> lookup in globals -> push args -> invoke with the
`0x6261E0` continuation) for a harmless getter first (`GetPlayerPosition`),
log the result, and only then call `SetPositionValues` on the companion. Do it
inside the plugin's own thread with the VEH active.
### Script VM: lookup works, invoke from a render hook faults (2026-10-21)

Implemented in the plugin and tested live:

- `DAT_012233F4` -> `0x04750C4C` -> `+0x18` gave the VM (`0x1AB90108` in that
  run); `FUN_00625ca0(vm, "GetPlayerPosition")` pushed the name and
  `FUN_00633cb0(vm, vm+0x40, stack_top-8, 0)` resolved it in the globals.
  **The lookup succeeded: type 6 (a function), value 0x1A2E4650.** So the
  script API is reachable from the plugin and `GetPlayerPosition` really exists
  in the VM's global table.
- The invoke is the problem. First attempt used an inline-asm stub; second used
  a hand-assembled byte stub (`push esi; mov esi,[esp+8]; ...; call; add esp,12;
  pop esi; ret`) with the 0x6261E0 continuation. Both times the game died at the
  invoke: the VEH logged ~5000 swallowed faults at a fixed plugin address and
  the process ended. So `FUN_00636570` cannot be called from the camera hook -
  the VM is almost certainly only valid while the game's own script update is
  running, and calling it from a render hook corrupts its state.

Next step for the VM path: hook the game's script/update function and issue the
call from inside it (same thread, VM active), or better, find where the game's
own code pushes and invokes a script function by name and reuse that wrapper.
The lookup result (type 6, value 0x1A2E4650) is a good anchor for that work: it
identifies the `GetPlayerPosition` function object in the VM.
### Script VM: the safe helpers found (2026-10-21)

Scanning the corpus for the VM global (`DAT_012233f4`) yields six functions:

- `FUN_0043e140` - VM owner constructor (stores `param_1` in `DAT_012233f4`);
  `FUN_0043e250` - its destructor.
- **`FUN_0043e480`** - the safe "resolve a script name" helper: pushes the name,
  looks it up in the globals, copies the resolved value over the stack top,
  checks `type == 6`, then calls **`FUN_006255e0`** (the stack-pop helper) and
  returns whether the name is a function. This is a safe, complete example of
  the push/lookup/pop sequence with stack cleanup - use it as the reference for
  any name lookup.
- `FUN_00468ea0` / `FUN_00468f40` - the "call a script function" wrappers:
  EAX = the name, EDI = the argument count, `[ebp+8]`/`[ebp+0xC]` describe the
  arguments; `FUN_004c8460` pushes them; `FUN_00636570` invokes with the
  `0x6261E0` continuation. `FUN_00468f40` is the same for `CallLuaFunction`.

So the VM has a proper internal API (`FUN_0043e480` for lookups,
`FUN_00468ea0`/`FUN_00468f40` for calls) that keeps the stack balanced. The next
attempt at moving a character should call `FUN_00468ea0` (or mirror it exactly,
including `FUN_006255e0` for every push) instead of the raw `FUN_00636570`, and
should run while the game's script update is active.

### Script VM: the wrapper fails too - the VM needs the script update context (2026-10-21)

Second live attempt used the game's own call wrapper `FUN_00468ea0` through a
hand-assembled byte stub (name in EAX, argument count 0 in EDI, two zero stack
dwords cleaned by the wrapper's `ret 8`), called once from the camera hook.
The log recorded `vm: calling GetPlayerPosition via FUN_00468ea0` and the
process died before `vm: call returned`. Together with the earlier raw-invoke
fault this is conclusive: **the script VM cannot be driven from the camera or
present hooks** - it is only valid while the game's own script update is
executing. Calling it from any render/update hook corrupts its state and takes
the process down.

The VM test is disabled in the plugin again; the game is stable.

Next step for this thread is therefore narrower and concrete: locate the game's
script update (the function that pumps the VM each frame - the VM owner is
`DAT_012233f4`, its interpreter entry `FUN_00635ab0` reached from
`FUN_00636570`), hook that function, and issue the `FUN_00468ea0` call from
inside it while the VM is live. Only then is `SetPositionValues` reachable.