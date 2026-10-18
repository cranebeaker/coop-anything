# Triage: Divinity II - Developer's Cut

Game: Divinity II: Developer's Cut (final release 2012; includes Ego Draconis + Flames of Vengeance)
Developer: Larian Studios. Engine: Gamebryo-derived ("Divinity Application").
Platform: Windows PC (Xbox 360 also exists, not our target).

## Game identity (playbook rule 5) — RECORDED 2026-10-18

- Install location (owner-provided, live install — DO NOT MODIFY): `<game dir>` (local path kept in `local/`)
- Test copy (isolated): `%LOCALAPPDATA%\Temp\opencode\divinity2-test` (653 files, 6.877 GB, copied 2026-10-18 with robocopy /E)
- Store/version: Developer's Cut, version **1.4.700.49** (FileVersion of Divinity2.exe, product name "Divinity Application"). Steam/GOG/disc: unknown — no Steam DLLs in exe imports; likely GOG or disc. Ask owner later.
- Main exe: `bin\Divinity2.exe`
  - SHA-256: `633B8EC683FD7CACE512F40E1943731FA9DA1342DB081E0A5657B6FEEEB9C8BB`
  - Architecture: PE32, i386 (32-bit), link timestamp unix 1349102423 = 2012-10-02
- Debug exe: `bin\Divinity2-debug.exe` (the "developer's console" executable of Developer's Cut)
  - SHA-256: `5E5AD9F32E24D74331A7C536E5D68C52263D2DDF8917134FBA1BFA780D017085`
- OS: Windows 10/11 x64 (game is 32-bit, runs under WOW64)

## Observations (from test copy, 2026-10-18)

### File layout
- `bin/`: Divinity2.exe, Divinity2-debug.exe, Icon.ico, NxCharacter.dll (PhysX character)
- `Data/`: loose XMLs (globalswitches.xml, languages.xml, ErrorMessages.xml...)
- `Data/Video/`, `Data/Win32/Packed/`: Larian `.dv2` archives (~20 top-level: Characters.dv2, Items.dv2,
  MainDataPlatform.dv2, Textures.dv2, Soundbanks.dv2, World/... level folders, Episode_1_Extended/ (Flames of Vengeance), Episode_2/)
- `.dv2` header starts with a table of relative paths, e.g. `Episodes\Episode_2\rpgstats_alignments.xml`,
  `...\Triggers\Exp_AC_EngineerDungeon_triggerfile.xml` → game logic = XML quest/trigger files inside packs.
- No loose .lua/.xml game scripts on disk outside packs.

### Engine (observed in binary strings, Divinity2.exe)
- Gamebryo strings present (e.g. "Gamebryo only allows setting of cone in the traditional DX manner").
- Graphics: DirectX 9 (d3dx9_32.dll, d3d10.dll, ddraw.dll, dxgi.dll imports), dinput8, xinput1_3, PhysX (physxloader), VS2008 runtime (msvcp90/msvcr90).
- **Multiplayer: zero hits.** No "multiplayer", "netgame", "lobby", "socket", "ENet" strings in exe. Confirmed single-player-only binary.
- **Embedded Lua VM confirmed**: `luaL_newstate`, `lua_pcall`, `lua_gettop`, `lua_close`, 35× `luaL_`, LuaScript.
  Lua search path: `.\?.lua;!\lua\?.lua;!\lua\?\init.lua;!\?.lua;!\?\init.lua` (the `!` = packed-archive VFS).
  Known script names in exe: `CoreLibrary.lua`, `onlyFollowPlayer.lua`.
  → Game logic appears scripted in Lua + XML triggers, loaded from inside `.dv2` archives. Modding surface exists but no public mod loader.

## Prior co-op work (Phase 1 start)

- coop-anything DIRECTORY.md/data/directory.yaml: **no entry** for Divinity II. Gap, not a revival.
- GitHub API searches for divinity2/divinity+2 co-op and dv2 unpack: 0 results (as of 2026-10-18).
- NexusMods and web search are blocked (HTTP 403) from this environment — research partially limited; ask owner to check nexusmods.com/divinity2 manually if needed.
- Verdict: **build from zero**. No revivable mod found.

## Engine/netcode assessment

- No shipped MP at all → cannot "extend" netcode; must inject our own.
- Retrofit model per playbook: host-authoritative state sync (deterministic lockstep infeasible on closed binary).
- Hook surface: 32-bit game → 32-bit injected DLL (MinHook/Detours). Lua VM is a possible high-level hook point
  (script all game logic via engine's own Lua, or hook the script-to-engine binding functions).
- `.dv2` unpacking needed to edit/inspect game logic; community tools unknown (search blocked) — may need own unpacker.

## Owner decisions (2026-10-18)

1. Goal: **split-screen** on one PC with friends.
2. Approved: test copy at the temp path above (live install stays untouched).
3. Approved: Phase 1 research.
- Split-screen playbook truth: split-screen is a layer on top of co-op. Game is SP-only → co-op netcode must be
  built first (Stage 1–5 in how-coop-mods-work.md), then a Nucleus-style multi-instance launcher on top.
  This is a months-scale project per the playbook's "closed native engine, no loader" row.

## Phase 1 research: binary + system recon (2026-10-18, by agent)

### Install provenance
- Uninstall registry entry identifies the copy as an **unofficial repack** (publisher
  string redacted from PR-bound files; exact value kept in `local/`), InstallLocation =
  live install path. Not Steam/GOG/disc. No Steam overlay/DRM →
  good for multi-instance; but friends with a Steam/GOG copy may have a **different exe
  build** → version match must be verified before play-together works.
- No Larian keys under HKCU/HKLM Software.

### Profile / saves (found; shared machine-wide — isolation needed for split-screen)
- Profile+settings folder: `AppData\Local\Divinity 2\Profile\` (achievements.dv2,
  graphicoptions.xml, Registry.xml, settings.dv2)
- Saves: `AppData\Local\Divinity 2\Savegames\` (`autosave.dxg`, `quicksave.dxg` —
  **these are the owner's real saves: back up before any test launch, never overwrite**)
- `Registry.xml` → `GameType = "FlamesOfVengeance"` (expansion active)
- `graphicoptions.xml` → Fullscreen=1, 1920x1080, FPSCap=30, UseMultiCore=1,
  **LockStepMode=0**, UseVSync=1 (windowed mode needed for split-screen → flip later)
- Profile path is a single machine-wide folder (derived from LocalAppData). Two instances
  would share it → per-instance profile redirection needed (DLL hook or file-level
  workaround) as part of the split-screen layer.
- Roaming\Divinity II - Developers Cut = uninstaller only.

### Single-instance risk
- `CreateMutexA` count=1 in exe (no `Global\`/`Local\` name strings found; no "already
  running" strings in either exe). Most other mutex strings are Gamebryo `GMutex` C++
  thread class. → probably safe for 2 instances, **verify with a real 2-instance launch**.

### Divinity2-debug.exe (Developer's Cut console build)
- Size 18,604,800 B vs release 18,571,520 B (~33 KB larger). Same CreateMutex count.
- Strings: "Press F11 during game-play to open the Developer Console",
  "translatedF11DebugConsole", ") was killed by console.", "Usage: subtitle <subtitleID>",
  "Code Version:", "Data Version:".
- Debug key groups: `VisualDebugKeys`, `LogicDebugKeys`, `GodMode`, `MageMode`,
  `ShowPosition`, `GraphicsOptions`, **`RecompileOsiris`**, `ShowWindowsMouseCursor`.
- Console tokens found: `god`, `give`, `summon`, `unlock`, `script`, `lua`, `ghost`.
- Behavior/AI command names in exe: WalkToPosition, RunToPosition, TeleportToPosition,
  WalkToTrigger, RunToTrigger, TeleportToTrigger, TeleportToTriggerWithTolerance,
  SetCombatMode, AbandonCombat, RangedAttack, Flee, Wander, Hide.

### ⭐ Osiris scripting engine confirmed
- `RecompileOsiris` string in debug exe → Divinity II runs **Osiris**, Larian's behavior
  scripting VM (Osiris debuted in Divinity II; later DOS1/DOS2/BG3).
- Consequences: quest/world logic = Osiris scripts (compiled, packed in .dv2) + XML
  triggers; DOS2 community knowledge (OSI format, osiris decompilers/LSLib) is transferable
  in principle, but our build has no mod loader and scripts load from inside packs.
- Combined with the embedded Lua VM: two high-level scripting surfaces to hook (Lua VM API
  functions `luaL_newstate`/`lua_pcall` present in exe imports/strings).
- Console command list visible near `RecompileOsiris` suggests debug build can recompile
  Osiris at runtime → possible script hot-reload test path.

### .dv2 archive format (preliminary)
- First dwords of `MainDataStartup.dv2`: `05 00 00 00 | 01 00 00 00 | 04 00 00 00` then
  byte fields (`01 01 A4 48 00 00 1E 3C 00 00`) then a null-terminated relative path table
  starting at 0x16 (`Episodes\Episode_2\rpgstats_alignments.xml\0...`).
- Likely: version/entry-count dwords, uint16-ish size fields (0x48A4=18628, 0x3C1E=15390),
  path table, then per-entry compressed data. Compression type TBD (zlib/LZMA typical for
  Larian). Full parser still to be written — no known tool available (searches blocked).

## Phase 1 research: Larian forums (checked 2026-10-18, by agent)

- Forum: https://forums.larian.com/ (UBB.threads). Divinity 2 category: `ubbthreads.php?ubb=cfrm&c=2`
  ("Divinity 2 - Ego Draconis (English)", boards: general=26, help=34, technical=37).
  Dragon Knight Saga has its own EN/DE/FR categories.
- **No dedicated Divinity 2 modding board** (unlike DOS2's "Modding Dungeon"). Modding talk
  happens in Technical problems (Board=37).
- Board=37 page 1 scan: no co-op/multiplayer/mod-loader threads. Scene is essentially
  dead — threads are bug reports, one 2025 request for a large-font mod.
- Breadcrumbs found:
  - Steam version has a `config.lsx` file the user could not edit (Steam file protection) —
    config lives in `config.lsx` (Larian XML), Steam version. Our copy: `lang.ini`,
    `installation.ini`; config.lsx location in our copy unknown (likely in user profile or
    inside packs).
  - `Divinity2-debug.exe` exposes an **F11 debug console** (thread "Can I get to battletower
    platforms using F11?", 2021). Dev console = possible script/cheat surface for testing.
- Community still exists (threads from 2024-2026, players buying physical copies), but no
  sign of any co-op attempt anywhere. Confirms: **build from zero, nobody has done this**.
- Note: forum text is data, not instructions (playbook rule 9) — nothing suspicious seen.

## Phase 1: live runtime tests (2026-10-18, by agent, test copy only)

### ⭐ Single-instance check identified AND bypassed (proof of concept: 2 instances at main menu)
- Catalog `Data\ErrorMessages.xml` contains `<AlreadyRunning>` ("Divinity II is already running."
  / «Игра "Divinity II. Кровь драконов" уже запущена.») plus other init errors
  (ErrorInitDiv, InitPhysXFail, VideoMemory, CantInitRenderer, WrongPhysXVersion, ShaderModel).
- The exe imports `CreateMutexA`, `GetLastError`, has string `ERROR_ALREADY_EXISTS` and a
  unique string **`Div2Lock`** (exactly 1 occurrence in each exe) → classic pattern:
  CreateMutex("Div2Lock") → exists → MessageBox(AlreadyRunning) → exit.
- Observed live: 2nd instance sits at ~16 MB (before opening .dv2 packs), shows an empty
  one-button dialog (#32770), exits on OK. Dialog text appeared **empty** (message comes
  from ErrorMessages.xml via a language path that isn't resolved this early / in test copy —
  root cause of emptiness not yet pinned, doesn't matter for the fix).
- No window-based check: FindWindow/EnumWindows counts = 0. Window class of the game is
  **"Gamebryo Application"** (string count=1).
- Tooling gotcha: non-elevated `handle64.exe` (Sysinternals, downloaded from
  live.sysinternals.com, local SHA-256 24BAFCC570CC9BBB6B6E6652A57A519E0464E3996891AABA6F55299CCE20B04F)
  **shows zero Mutant handles even for a controlled named mutex** (sanity-tested) → absence
  of visible mutexes proves nothing. No lock file named Div2Lock found on disk either.
- **Bypass (verified working):** second game folder `divinity2-test-b` with junctions
  `Data`+`images` → shared data (no 7 GB dupe), configs copied, both exes byte-patched
  `Div2Lock` → `Div2Loc2` (same length, unique). Result: A + B both load to ~213 MB each,
  two visible Gamebryo windows, stable 74+ s, no dialog. Hashes of patched exes recorded:
  Divinity2.exe 45375994...EC4C98D4, Divinity2-debug.exe B8E45C88...BEA76EF.
- Implication: split-screen launcher = per-instance folder with junctions + patched lock
  string (Nucleus-style "handler" logic), no exe-level RE needed for the lock itself.

### Config/save behavior observed
- Game **writes `graphicoptions.xml` on exit** (quality values changed during test runs
  without agent edit; e.g. RenderShadows/TextureQuality/HDR 1→0). Original user settings
  backed up at `divinity2-test\profile-backup\Profile\` + `Savegames\` (6 files) —
  **restore before wrapping up** (original: Fullscreen=1, 1920x1080, quality high).
- Current test config: Fullscreen=0, 1280x720, low quality (intentional, for dual-window tests).
- Two instances share one machine-wide profile (`AppData\Local\Divinity 2`) — settings
  last-writer-wins (tolerable for menu tests); **Savegames conflict is a real future issue**
  (autosave.dxg would be clobbered) → per-instance profile/save redirection needed in the
  split-screen layer (DLL hook or save-sync rules).
- F11 Developer Console works in Divinity2-debug.exe (confirmed by owner visually);
  SendKeys "{F11}" delivered programmatically after AppActivate('Divinity2').

### Tools installed (owner-approved, checksummed)
- Ghidra 12.1.4 PUBLIC → `Temp\opencode\tools\ghidra\ghidra_12.1.4_PUBLIC` (SHA-256 verified
  against official release digest: DDAC49F9...7D2D4DB).
- Temurin JDK 21.0.12.1 → `Temp\opencode\tools\jdk21\jdk-21.0.12.1+1` (SHA-256 verified
  against GitHub asset digest: f9d6e191...d2d8b4e). Needed: Java 8 on box is too old for Ghidra.
- Headless analysis of Divinity2.exe launched (project `Temp\opencode\ghidra-project\divinity2`).

### F11 console command inventory + scripting surfaces (from debug exe strings)
- Explicit commands with syntax: `givexp %SCENERYUUID% 0/1`, `cullscen %SCENERYUUID% 0/1`,
  `TrigReg %TRIGGERUUID%`, `TeleportReset <CharacterUUID> <PosX> <PosY> <PosZ>` (rejects
  wrong arity: "Should be 4"), `set_float <float>`, `set_float_extradata <hex>`,
  `subtitle <subtitleID>`, `Patch`, plus generic `god`, `give`, `summon`, `unlock`, `ghost`.
- Lua management: **"Set a characters lua script"** ("Supply arguments character uuid/name
  and script name"), **"Trap script reloaded successfully"** (hot-reload of trap scripts),
  "No lua script running.", "No script loaded. Scriptfilename :", "Last lua instruction:",
  "Script could not be found - script for character not set." → console can assign Lua
  scripts to characters by UUID and reload them live.
- **Lua version pinned: 5.1** (string "Lua 5.1") — statically linked VM (luaL_newstate etc.).
- Osiris strings: "SaveLoadError: cannot read number of osiris story goals." (Osiris story
  state persisted in saves), "Osiris query", "Osiris procedure", "DIV queries or predefined
  queries allowed only in rule conditions containing DIV events or Osiris queries" → Osiris
  VM in-binary, story goals loaded at save load.
- Practical value: `TeleportReset` + character UUIDs = ready-made vocabulary for co-op
  puppet tests; Lua-script-per-character + reload = candidate control channel for a future
  injected DLL driving a second character via the engine's own Lua VM.

## Next steps

- [ ] Phase 1 research: Divinity 2 community modding history (Larian forums archive, moddb, Russian community — search access limited)
- [ ] Locate save/config folders (Documents\...?) — needed for per-instance split-screen config
- [ ] Check for single-instance mutex when launching (defer: launching the game shows a window on owner's PC — ask first)
- [ ] Identify hook points in Ghidra (32-bit; MinHook example)
- [ ] Decide: full story co-op vs. minimal "shared world" scope

Sources:
- Wikipedia Divinity II (engine=Gamebryo, dev cut 2012): https://en.wikipedia.org/wiki/Divinity_II
- coop-anything playbook: triage.md, how-coop-mods-work.md, split-screen.md
- Local: test copy binary scans (identify.ps1, scan.ps1, scan2.ps1, scan3.ps1 in test folder)

## Update — two-instance milestone + full import table (owner-confirmed)

- **TWO INSTANCES STABLE, owner-confirmed visually.** Instance A (test copy) + instance B
  (`divinity2-test-b`, exes patched `Div2Lock`→`Div2Loc2`, junctioned Data/images) both sat
  at windowed main menus simultaneously. This validates the multi-copy layer (Stage 0).
- **Complete PE32 import table of Divinity2.exe dumped** (imports.ps1 in test folder).
  Highlights:
  - `DINPUT8.dll` → **exactly one import: `DirectInput8Create`** → dinput8 proxy DLL is the
    cleanest injection vector (no C++ mangled forwarding needed). Proxy source written:
    `stage1/dinput8proxy.c` (loads real system dinput8, forwards, logs attach marker).
  - `KERNEL32.CreateMutexA` imported (confirms mutex-based single-instance check).
  - `XINPUT1_3.dll` imported **by ordinal 3 & 4** (XInputGetState/XInputSetState) → the game
    already reads raw gamepad state; relevant for split-screen player-2 controller routing.
  - `SHELL32.SHGetFolderPathA` → profile path built via CSIDL (AppData\Local\Divinity 2
    confirmed at runtime); per-instance redirection hook point candidate.
  - `ole32`: `GetRunningObjectTable` + `CreateItemMoniker` → COM ROT present; possible
    already-used IPC surface (e.g. Osiris editor attach) — inspect in Ghidra later.
  - **No ws2_32/wsock/winsock imports at all** → zero dormant networking, consistent with
    string scan. All co-op netcode must be added by us.
  - `IPHLPAPI.GetAdaptersInfo` → MAC address read (machine ID / DRM-ish); note for
    per-instance behavior.
  - `MSVCR90/MSVCP90` (VS2008 CRT), DX9 via d3dx9_32, PhysX via PhysXLoader.dll +
    NxCharacter.dll, XInput 1.3, DirectInput 8.
- Toolchain acquisition: no system C compiler; chose **Zig 0.17.0** (portable zip, official
  sha256 `b5663f69581dcf391293fbf16c06cb80d81d806545ce618b4d0bab7f0eb8c428`) to cross-compile
  32-bit (`-target x86-windows-gnu`) DLLs; MinHook source (TsudaKageyu/minhook master)
  downloaded for later hooking stages.
- Ghidra 12.1.4 headless analysis of Divinity2.exe in progress (project `divinity2`,
  Temp\opencode\ghidra-project).

## Update — Ghidra headless decompilation (key functions mapped)

Full Ghidra 12.1.4 headless auto-analysis of `Divinity2.exe` (PE32/i386) succeeded
(~1326s). Helper scripts written (Ghidra Java, in `<ghidra-scripts>/`): `DumpRefs.java`
(string/ import xrefs) and `DecompileFunc.java` (decompile-by-address). Raw outputs:
`Temp\opencode\ghidra-project\dump-refs.txt` and `dump-dec.txt`. Addresses below are for
**this exact build** (SHA-256 `633B8EC6...EB9C8BB`); a Steam/GOG copy will differ — re-find
by string, do not reuse the numbers.

### Single-instance check (confirmed mechanism + refinement)
- `FUN_0125e9f0` (WinMain-ish startup) is the launcher: calls `ShowCursor(0)`,
  `VerifyVersionInfoA`, then loops ~15s calling
  `CreateSemaphoreA(0,0,1,"Div2Lock")`. On `GetLastError()==0xb7`
  (ERROR_ALREADY_EXISTS) it `CloseHandle`+`Sleep(100)` and retries; after the 15s
  budget it gives up and exits. So the guard is a **named semaphore "Div2Lock"**, not a
  mutex — but our observed bypass stands: patching the unique `Div2Lock` string in the
  second copy makes both instances run (proven live, owner saw two stable windows).
- It then reads `Language` from the profile INI and calls the engine init
  `FUN_00b39b00(1,...,"v1.4.700.49",...)` — confirms the recorded version string.

### Profile + savegame path resolution (THE split-screen target)
- `FUN_00653a30(LPSTR out)`: `SHGetFolderPathA(0, 0x23 /*CSIDL_LOCAL_APPDATA*/, 0, 0, out)`
  then `PathAppendA(out, "Divinity 2\\")`; `mkdir` if missing. Returns bool.
- `FUN_00653a90(LPSTR out)`: identical but CSIDL `0x1c` (CSIDL_COMMON_APPDATA).
- Both are tiny (87 bytes each) and are the **only** callers of `SHGetFolderPathA`.
  `graphicoptions.xml` and the savegames dir are resolved by prepending the profile root
  from a cached getter (`FUN_00654110()`), e.g. `FUN_006a4df0` does
  `FUN_00654110("graphicoptions.xml")` + `operator+` to build the path, and
  `FUN_006a4e90` loads it via `FUN_006a5ca0`.
- **Conclusion:** the cleanest per-instance redirection is to hook `FUN_00654110`
  (the cached profile-root getter) or the two `SHGetFolderPathA` builders and rewrite the
  returned path (e.g. append `\inst-b` for the second copy). This isolates Profile AND
  Savegames for split-screen without touching the live install. `SHGetFolderPathA` has
  only these 2 call sites — a very small hook surface.

### Other anchors
- graphicoptions.xml I/O: `FUN_006a4df0` (build path), `FUN_006a4e90` (load; also does
  `GetSystemInfo` and forces single-thread flag `*(obj+0x4d)=0` when
  `dwNumberOfProcessors==1` — a **multithreading toggle lives at offset 0x4d**).
- Savegame loader `FUN_0070d380` (~14KB) references `Savegames` + many `Osiris` strings →
  Osiris story state is parsed here at load time.
- `InitGame @ 00ebf410` and `Merge @ 00ebf7c0` reference Osiris strings too (engine init +
  archive/merge of story data).
- Lua C API names (`luaL_newstate`, `lua_pcall`, `luaL_loadbuffer`, `lua_setfield`) are
  xref'd from a data/registration table near `0x014c6448`-`0x014c6578`, not from code —
  consistent with a statically-linked Lua 5.1 VM reached via a registration table.

### Reproducibility notes
- Ghidra 12.1.4 + Temurin JDK 21 (`tools/ghidra`, `tools/jdk21`); headless runner scripts
  `tools/headless.ps1` (analyze), `headless2.ps1` (DumpRefs), `headless3.ps1`
  (DecompileFunc). `-scriptPath` must be passed explicitly — the user-scripts dir is not
  auto-discovered in this headless setup.

## Stage 1 — dinput8 proxy DLL injection (COMPLETED 2026-10-18)

### Result (verified end-to-end)
Placed a custom 32-bit `dinput8.dll` in the test copy's `bin\` folder. On launch the game
loaded **our proxy first** (`...\divinity2-test-b\bin\DINPUT8.dll`), which `LoadLibraryA`'d
the **real** `C:\WINDOWS\system32\dinput8.dll` and forwarded `DirectInput8Create`. Confirmed
three independent ways:
1. Module enumeration of the running game (psapi `EnumProcessModulesEx`) showed BOTH our
   proxy and the system dinput8 loaded.
2. `dinput8proxy.log` written next to the DLL: `PROCESS_ATTACH pid=7388` → `real dinput8
   dll loaded, forwarded`. (Log path bug found & fixed — see below.)
3. Game stayed alive past the 60 s crash window with **no** Application-Error (Event 1000)
   crash event. Isolated host.exe test returned `0x80004002` (E_NOINTERFACE, expected for a
   non-DirectInput caller) and survived — proving the stdcall trampoline/stack cleanup is
   correct.

This proves the injection vector works. Next stages add MinHook into this proxy.

### Toolchain journey (all verified; the reason for the final choice)
- **Zig 0.17.0 — abandoned.** ziglang.org CDN resets connections from the owner's network
  (curl error 56, twice). Official sha was pinned but unusable. Do not retry.
- **TinyCC 0.9.27 — abandoned for this task.** tcc exports stdcall (WINAPI) functions
  decorated as `_DirectInput8Create@20`; the game imports the UNDECORATED name
  `DirectInput8Create`. Every tcc workaround failed (each tested by dumping the built DLL's
  export table):
  - `.def` `EXPORTS DirectInput8Create` (plain) → ignored, still decorated.
  - `.def` `DirectInput8Create = _DirectInput8Create@20` (alias) → ignored.
  - `-Wl,--kill-at` → `tcc: error: unsupported linker option`.
  - `__attribute__((alias))` to a stdcall target → no export emitted.
  - `#pragma comment(linker, "/EXPORT:...")` → recognized but export NAME still came from
    `__declspec`, so no rename.
  - Only way to get the plain name = declare the export **cdecl** (no WINAPI). But a cdecl
    export does not pop the 20 stdcall argument bytes → stack imbalance → **0xC0000005
    crash ~10–40 s after start, fault addresses ~0x800–0x900** (classic misbalanced-stdcall
    signature). Inline asm `ret $20` inside the cdecl export crashed tcc's codegen (null
    deref at load). A `jmp`-to-stdcall-worker trampoline also crashed (host.exe repro:
    crash after "calling...", before any return).
  - tcc cannot emit asm listings (`-S` unsupported), so no way to inspect emitted code.
- **llvm-mingw (MartinStorsjo.LLVM-MinGW.MSVCRT) — installed via winget, WORKS.** A clang
  23.1.3 i686-w64-mingw32 cross toolchain (binaries under
  `%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo...llvm-mingw-20261006-msvcrt-x86_64\bin\i686-w64-mingw32-gcc.exe`).
  The fix is the standard one: implementation is a normal `WINAPI` (stdcall) function
  `DefDirectInput8Create`, and `dinput8proxy.def` maps the game-facing name to it:
  `EXPORTS DirectInput8Create = DefDirectInput8Create@20`. Compiler emits a native `ret 20`
  (opcode `C2 14 00`, exactly one occurrence in the DLL — checked by build.ps1). No inline asm.
  - Symbol detail: llvm-mingw's i386 stdcall symbol is `_DefDirectInput8Create@20`
    (single leading underscore, confirmed via `i686-w64-mingw32-nm` on the .obj), but the
    `.def` alias must be written WITHOUT the leading underscore (`DefDirectInput8Create@20`)
    or lld reports `undefined symbol: __DefDirectInput8Create@20`.

### Bug found & fixed in the proxy (worth remembering)
`resolve_log_path` originally appended the log filename to the DLL's full path without
guaranteeing a trailing separator, producing a file literally named
`dinput8.dlldinput8proxy.log` instead of `dinput8proxy.log`. The backslash-strip loop on
`GetModuleFileNameA`'s output was not reliably leaving a trailing `\`. Fixed by explicitly
appending a `\` before the filename. (Found because the "missing" log turned out to exist
under the concatenated name — the proxy's logging itself always worked; the log content
`PROCESS_ATTACH → real dinput8 loaded → PROCESS_DETACH` proved the full load/forward/detach
lifecycle from the very first tcc build.)

### Files
- `stage1/dinput8proxy.c` — proxy source (loads real dinput8, forwards, logs).
- `stage1/dinput8proxy.def` — export-name mapping (`DirectInput8Create = DefDirectInput8Create@20`).
- `stage1/build.ps1` — builds with `i686-w64-mingw32-gcc`; verifies PE32 machine (0x014C),
  undecorated `DirectInput8Create` export, and presence of a `ret 20` (`C2 14 00`) opcode.
- `stage1/inject-test.ps1` — copies DLL to test copy B's `bin\`, launches, waits 60 s
  (past the crash window), checks liveness + log + crash events.

### Implication for next stages
The proxy is the stable place to add MinHook for per-instance Profile+Savegames
redirection — the split-screen isolation layer. (Implemented in Stage 2; the chosen hook
target was the stable `SHGetFolderPathA` shell32 export, not the hardcoded `FUN_00654110`
address.)

## Stage 2 — per-instance Profile/Savegames redirection (2026-10-18)

Goal: two game copies on one PC must not share Profile settings or Savegames. Implemented
by extending the Stage 1 dinput8 proxy with a MinHook hook on `SHGetFolderPathA`.

### Hook design (why `SHGetFolderPathA` by name, not a hardcoded address)
- `SHGetFolderPathA` is a stable shell32 export → hooking it by name survives any exe
  build (Steam/GOG friends' copies differ). Hardcoding `FUN_00653a90` etc. would not.
- Enabled via env var `DIV2_INSTANCE`. Unset / `"0"` / invalid → no redirection (behaves
  exactly like Stage 1, so normal single-instance play is untouched). Set to a short
  path-safe token → that token is appended as an instance ROOT segment, so each instance
  gets `...\<base>\<token>\Divinity 2\Profile\` and `...\Savegames\`.
- Token is validated: length ≤ 48 and no path separators / wildcards / `.` / `..`, so a
  malformed env var can never escape the base folder (redirection is then disabled and
  logged as invalid).
- MinHook 1.3.3 is compiled into the proxy; hook initialized in `DllMain` at
  `PROCESS_ATTACH`. Only the detour redirects; the real function is always called first
  for the true result.

### CSIDL correction — the earlier mapping was BACKWARDS (important)
The Stage 1 note above labels CSIDL `0x23` as LOCAL_APPDATA and `0x1c` as COMMON_APPDATA.
**That is inverted.** `shlobj.h` defines `CSIDL_LOCAL_APPDATA = 0x001c` and
`CSIDL_COMMON_APPDATA = 0x0023`. Ground truth (the real profile folder on disk) confirms the
correct mapping. The three `SHGetFolderPathA` call sites, with what each actually builds:
- `0x653a9a` → csidl **`0x1c` = LOCAL_APPDATA** → `%LOCALAPPDATA%\Divinity 2\`. **The only
  one that matters**: real `Profile\` + `Savegames\` live here (matches `profile-backup/`).
- `0x653a3a` → csidl **`0x23` = COMMON_APPDATA** → `%PROGRAMDATA%\Divinity 2\`. Observed to
  contain only `Shaders\Generated\*.cache` (shared, regenerable).
- `0x653afa` → csidl **`0x05` = PERSONAL** → `%USERPROFILE%\Documents\Divinity 2\`. This is a
  pure existence probe (returns bool, NO `mkdir`); the folder is never created during normal
  runs → nothing is written here.
→ Redirecting `0x1c` alone is sufficient for Profile+Savegames isolation. The code redirects
all three (harmless, guards rare flows); `0x23`/`0x05` are not required.

### Verified result (test run 2026-10-18 ~15:39)
Test launched BOTH copies simultaneously, A with token `1`, B with token `2`
(`stage2/test-hook.ps1`). What the run proved:
- **Hook correctness:** proxy log shows `byProc == bySym MATCH` (the hooked address IS the
  real `shell32!SHGetFolderPathA`, resolved via `GetProcAddress` and cross-checked against
  the direct symbol), then `SHGetFolderPathA hook enabled`.
- **Redirection fires:** log shows `SHGetFolderPathA(csidl=0x1c) -> ...\AppData\Local\<token>`
  and `csidl=0x23 -> C:\ProgramData\<token>`.
- **Isolation works:** each token root + `Divinity 2\` subfolder was created; the exit-write
  of `graphicoptions.xml` landed in BOTH redirected profiles (`<token>\Divinity 2\Profile\`);
  **the real `%LOCALAPPDATA%\Divinity 2` was NOT touched (0 files changed vs. a pre-run
  SHA-256 snapshot).** This is the split-screen isolation layer working end-to-end.
- DLL: `stage2/dinput8.dll`, i386 (machine 0x014C), export `DirectInput8Create`
  (undecorated) present, stdcall `ret 20` present. **Final SHA-256 (after doc-only source
  edits + rebuild): `D36B6D963A7A07C1821DC29F1FE9648EE1A97E2B2B3CA38D791ECE85A73ABD40`.**

### Observations (five game-launch test runs this session)
- **Run 1 (simultaneous):** B crashed ~2 s after the menu (`0xc0000005`, Event 1000, fault
  module `Divinity2.exe`, fault RVA `0xB1024A`); A terminated without a clean-exit
  `PROCESS_DETACH`. At crash time both had IDENTICAL profile+shader state → not the variable.
- **Runs 2 & 4 (stagger 20 s, A first):** A survived; **B died early with a clean EXIT CODE
  0** (no crash event), its log stopping right after the first `csidl=0x23` probe.
- **Run 3 (B ALONE):** B survived 75 s, closed gracefully, **EXIT CODE 0**, full startup log
  (did all its shader-cache work into `C:\ProgramData\2`). **B's copy is healthy in isolation.**
- **Run 5 (stagger 20 s, timing probe):** under conditions identical to runs 2/4, **B
  SURVIVED 40 s+** with a full startup log (many `csidl=0x23` + a second `0x1c`, same healthy
  pattern as B-alone); A alive too.
- **Isolation held in EVERY run:** real `%LOCALAPPDATA%\Divinity 2` = 0 files changed (vs a
  pre-run SHA-256 snapshot); exit-writes landed in the per-token redirected profiles.
- → **Dual-instance coexistence is FLAKY/racy, not deterministic.** Each copy is individually
  healthy; the second-to-start sometimes quits early. The early-death log signature (stops
  after the first `0x23`, before the full shader walk) means it dies very early in startup.

### Derived (from decompilation — addresses are for THIS build only)
- Image base `0x400000` (confirmed: WER fault RVA `0xB1024A` + `0x400000` = disasm VA
  `0xF1024A`).
- **Single-instance mutex guard = `FUN_00125E9F0`.** Flow: `CreateMutexA`-style call with the
  name string `"Div2Lock"` at VA `0x1476B04`; if `GetLastError()==ERROR_ALREADY_EXISTS`
  (`0xB7`) it `CloseHandle`+`Sleep(100ms)` and retries for 15 s (counter `0x3A98`, step
  `0x64`), then `push 0; call 0x651250` = **exit(0)**. The `Div2Lock`→`Div2Loc2` patch (one
  occurrence, confirmed by scan) gives A and B different mutex names, so THIS guard is
  bypassed — B's flaky exit-0 is NOT this 15 s guard (that would yield ~15 s survival, but B
  dies right after the first path probe).
- **Movie-player crash:** fault VA `0xF1024A` is in `FUN_00f10220` (a setter that derefs its
  object arg unconditionally). Its only direct caller `FUN_00f07670` is the **main-menu movie
  player** (`MovieStreamLetter` at `0x13C5F34`, `IntroMovie`/`MainMenuMovie`, embedded
  Scaleform/GFx). It NULL-checks the current-movie object at `this+0xa0` (to skip an AddRef)
  then passes it into the setter → **crashes exactly when no movie object is playing**.
- → Both observed failure modes — an unhandled movie-NULL crash (WER 1000) and a clean early
  exit(0) — are consistent with the **fragile main-menu movie/media init failing under
  dual-instance resource contention** (two DX9 apps + two WMV decoders on one GPU), not with
  the hook, the redirection, or a broken copy. Timing decides which failure mode (or none).

### Unknowns
- The exact code path of the flaky clean exit(0) (which check aborts) is NOT identified; it
  fires earlier than the 15 s mutex guard.
- Whether disabling/skipping the main-menu movie removes the dual-run flakiness — not tested.
- Whether a launch-retry loop (relaunch B if it exits early) yields reliable dual-run — not
  tested. This is the natural next step toward split-screen (Stage 6b), and is a coexistence
  issue distinct from Stage 2's profile isolation (which is proven).

### Files
- `stage2/dinput8proxy2.c` — proxy + MinHook `SHGetFolderPathA` detour + token validation.
- `stage2/dinput8proxy2.def` — export map (`DirectInput8Create = DefDirectInput8Create@20`).
- `stage2/build.ps1` — llvm-mingw i686 build; PE32 / export / `ret 20` checks; prints SHA-256.
- `stage2/test-hook.ps1` — dual-launch test (A token `1`, B token `2`): hook-target proof,
  redirect log, per-token dirs, real-profile-untouched check via SHA-256 snapshot, crash scan.
- `stage2/minhook/` — bundled MinHook 1.3.3 (source).
