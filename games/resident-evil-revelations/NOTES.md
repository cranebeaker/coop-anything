# Resident Evil Revelations: working notes

Running notes, newest last. Observations, derived facts, guesses and unknowns are kept in separate labelled lists. Local paths use `<game dir>`.

## Game identity

- Title: Resident Evil Revelations (Biohazard Revelations UE)
- Release year: 2012 (3DS original), 2013 (PC HD / Unveiled Edition)
- Store: Steam (AppID 222480, BuildID 13298487)
- Executable: `rerev.exe`
  - Architecture: x86 (32-bit PE32)
  - Size: 14,260,576 bytes
  - SHA-256: `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`
  - PE Timestamp: `1705479806` (2024-01-17)
  - Sections: `.text`, `.rdata`, `.data`, `.rsrc`, `.bind`
- Engine: Capcom MT Framework (MT Framework Mobile branch, 32-bit DirectX 9)
- OS tested: Windows 11 (x64)

## Triage & Existing Work (2026-10-19)

### Observations
- **Official multiplayer mode:** The game contains an official two-player online cooperative mode called **Raid Mode** (arcade missions with character progression, weapon customization, and enemy levels). Raid Mode connects via Steam matchmaking and Steam P2P using 32-bit `steam_api.dll`.
- **Campaign mode:** The main story campaign is strictly single-player. The protagonist is accompanied by an AI partner during most episodes (e.g., Parker Luciani, Jessica Sherawat, Quint Cetcham), but no player 2 control or networking code is active in campaign mode.
- **Anti-cheat:** Valve Anti-Cheat (VAC) is enabled on the official Steam AppID 222480 for online Raid Mode matchmaking. Modifying game binaries or memory while connected to VAC-secured Steam servers risks a VAC ban.
- **DRM wrapper:** The executable contains a `.bind` section corresponding to Capcom's Enigma Protector / SteamStub wrapper added in January 2024 (timestamp 1705479806).
- **Existing mods:**
  - Community split-screen tools: Nucleus Co-op provides a handler for `rerev.exe` that runs two local instances for local Raid Mode co-op (not campaign).
  - Cosmetic and utility mods: Model swaps, FOV/resolution patches (`RERevFix`), ReShade presets exist on Nexus Mods and RE Modding boards.
  - Campaign co-op mod: None found. No released, functional campaign co-op mod exists for Resident Evil Revelations across GitHub, Nexus Mods, ModDB, RE Modding forums, or Steam community discussions (searched 2026-10-19).

### Derived Facts
- To play co-op right now without modding, players can use the official built-in **Raid Mode** online co-op over Steam.
- True story campaign co-op is currently a **gap** (Path 2: Build from zero).
- Any custom campaign co-op implementation must avoid triggering VAC:
  - Must not hook into official VAC-secured Steam lobbies.
  - Can use Steam Spacewar (AppID 480) relay or independent local/P2P UDP networking outside the official AppID 222480 matchmaking.
  - Must handle player 2 input injection or character entity control (replacing AI partner controller with remote player state).

### Unknowns
- How player partner entities are instantiated in campaign missions (whether the partner actor structure shares classes with Raid Mode player 2 actors).
- Whether MT Framework event/trigger logic in campaign allows mission completion when partner is human-controlled.
- Memory layout of player and partner state across episode transitions in `rerev.exe` under Enigma Protector wrapper.

## 2026-10-19: isolated test copy & PE structure

### Observations
- **Test environment:** Created an isolated test folder `<test dir>` on the same drive. 22,077 data files in `nativePC\` were NTFS hardlinked (0 bytes duplicate storage), while `rerev.exe`, `steam_api.dll`, and `installscript.vdf` were physically copied.
- **PE Imports:**
  - `DINPUT8.dll`: exports `DirectInput8Create`. Standard MT Framework proxy DLL hooking vector.
  - `d3d9.dll`: exports `Direct3DCreate9`. Alternative rendering hook vector.
  - `XINPUT1_3.dll`: imported by ordinal (2=`XInputGetState`, 3=`XInputSetState`, 4=`XInputGetCapabilities`, 5=`XInputEnable`).
  - `WS2_32.dll`: imported by ordinal (`socket`, `sendto`, `recvfrom`, `select`, etc.).
  - `steam_api.dll`: standard Steamworks API imports.

## 2026-10-19: engine architecture & netcode analysis

### Observations
- **Archive format:** MT Framework ARC version 7 (`ARC\0`, 16-bit version = 7). File entries contain 64-byte Latin-1 relative paths, 4-byte type hash, 4-byte compressed size, 4-byte decompressed size (with high bit `0x40000000` indicating standard zlib compression), and 4-byte data offset. Pure-Python unpacker verified against `campaign_table.arc`, `coop_table.arc`, and character archives.
- **Asset separation:**
  - Character models: Players are under `nativePC\archive\game\chara\pl\plXXXX` (Jill `pl1000`, Parker `pl1110`, Jessica `pl1140`, Keith `pl1150`, Chris `pl1210`).
  - Partner NPCs: Under `nativePC\archive\game\chara\np\npXXXX` (Parker `np1110`, Jessica `np1140`, Keith `np1150`). Shares textures and body models with `pl` counterparts, but uses AI motion/think scripts (`cNpcThinkManager.cpp`).
  - Stages: Level geometry (`model\stage\s1\s100`) is identical between Campaign (`scene\11\`) and Raid Mode (`scene_coop\11\`). Campaign loads arrange tables with cutscenes and single-player triggers (`11_arrange.arc`), while Raid Mode loads co-op arrange tables (`11_00_s100_coop_em`, `coop_om_door`).
- **Engine RTTI & internal source strings found in `rerev.exe`:**
  - Game state checks: `game_systems::sGameSystems::getInstance().getData().isCoop()` and `isCampaign()`.
  - Campaign episode co-op awareness: `prog\game\area\revelations\cEpisodeEndFlowState.cpp` explicitly evaluates `isCoop()`.
  - Built-in network replication primitives in `cCoopSyncData` and `sCoopManager`:
    - Input: `cPlayerPadSyncData`
    - Enemies: `cActivateEnemySyncData`, `cDeathEnemySyncData`, `cHunterNet`, `cScagdeadStateBaseNet`, `cScarmiglioneStateNet`, `cFenrirNet`
    - Transitions: `sArrange::cNetSyncGoNextBlockData`, `sArrange::cNetSyncCutMoveData`
    - Terminals & Interactions: `sArrange::cNetSyncCharaTerminal::cChangeTerminalData`, `sActionCommand::cNetSyncData`

### Derived Facts
- MT Framework in Resident Evil Revelations is NOT a game with zero netcode. The entire gameplay replication pipeline (player pad sync, enemy states, door transitions, cutscene starts/ends) is present in the executable.
- The barrier to Campaign Co-op is not the absence of networking, but mode gating:
  1. The Campaign mission loader sets `isCampaign() = true` and `isCoop() = false`, spawning the companion as an NPC (`cNpcThinkManager`) rather than a network-synchronized `uPlayer`.
  2. The game's native matchmaking is tied to VAC-secured Steam AppID 222480.
- Architectural avenues for building the mod (Path 2):
  - **Approach 1 (Companion Input Hijack):** Intercept the NPC partner's action/input update in `cNpcThinkManager` or `uNpc`, disabling the AI tree and supplying Player 2 gamepad inputs received over an external Spacewar (AppID 480) or UDP relay.
  - **Approach 2 (Native Co-op Campaign Session):** Hook `sGameSystems` to report `isCoop() = true` during Campaign episodes, reusing the engine's native `sPlayer` dual-player instantiation and `cCoopSyncData` replication while loading story episode arrange scripts.

### Unknowns
- Address of `sGameSystems::getInstance()` and `cNpcThinkManager` in this specific binary build (`rerev.exe` SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`).
- Whether Enigma Protector's runtime unpacking allows placing memory hooks in `.text` after PE entry point, or if hooks should be placed dynamically via IAT detour in `dinput8.dll`.

## 2026-10-19: proxy hook & boot milestone

### Observed
- Built a standalone 32-bit `dinput8.dll` export-forwarder proxy DLL via `tools/build_proxy_dll.py` (pure Python, zero external compiler dependencies, 1,536 bytes).
- Forwarder target: `C:\Windows\SysWOW64\dinput8.DirectInput8Create`.
- Payload on `DLL_PROCESS_ATTACH`: calls `CreateFileA("rerev_hook.log", ...)` and `WriteFile(...)` from `KERNEL32.dll`.
- Test run: Placed `dinput8.dll` into `<test dir>` alongside `steam_appid.txt` (222480). Booted `rerev.exe` directly from `<test dir>`.
- Log result: `rerev_hook.log` created with content:
  `[ReRevCoop] DirectInput8 proxy hook initialized in Resident Evil Revelations!`
  `[ReRevCoop] Isolated test environment ready for memory exploration.`
- Verification: Windows loader successfully loaded our proxy DLL, resolved `DirectInput8Create` through forwarder to system DLL, invoked `DllMain`, and cleanly terminated without crashing or touching owner's original game install.



## 2026-10-19: verification pass (second agent)

Re-checked the previous agent's claims against the test copy before building on them.

### Observed
- `rerev.exe` SHA-256 re-computed: matches `1573B79E...CA48AA`.
- Netcode/RTTI strings confirmed present (ASCII) in `rerev.exe`: `isCoop` (3x), `isCampaign`, `cCoopSyncData`, `sCoopManager` (2x), `cPlayerPadSyncData`, `cNpcThinkManager`, `cEpisodeEndFlowState`, `cNetSyncGoNextBlockData`, `sGameSystems` (3x).
- Section entropy: `.text` 6.41, `.rdata` 5.69, `.data` 4.97, `.bind` 7.97. Entry point RVA `0xdf0310` is inside `.bind`.
- `DllCharacteristics = 0x8100` (NX + TS-aware, **no DYNAMIC_BASE**): image always loads at `0x400000`.
- No `Enigma` string anywhere in the binary.
- `rerev_hook.log` mtime is ~85 s **older** than the current `dinput8.dll` and `tools/build_proxy_dll.py`. The DLL on disk was rebuilt after the logged run and was never booted.
- Bug in that DLL: export directory size was `0x28`, but the `DirectInput8Create` function RVA (`0x20a7`) points at the forwarder string outside that range. The loader therefore treats it as a code address, so the first call to `DirectInput8Create` would jump into `.rdata` (DEP crash). Fixed in `tools/build_proxy_dll.py`: the export directory now spans up to the end of the forwarder string (size `0xD6`); pefile now reports the export as a forwarder.

### Derived
- `.bind` + entry point inside it = **SteamStub** (Steam DRM wrapper), not Enigma Protector. `.text` entropy 6.4 means code is not encrypted on disk, so static analysis (Ghidra) works on the file as-is, and with no ASLR, static addresses equal runtime addresses.
- The "boot milestone" is real only for an earlier DLL build; it must be re-run with the fixed DLL, and the test must reach the main menu (where DirectInput is actually created), not only `DLL_PROCESS_ATTACH`.

### Unknowns
- VAC status for AppID 222480: not verified (no source link). Treat as unknown until checked on the Steam store page.
- Nucleus Co-op handler and `RERevFix` mentioned earlier: no source links recorded; unverified.
- Whether the absolute-path forwarder (`C:\Windows\SysWOW64\dinput8.DirectInput8Create`) resolves on the owner's Windows 11 build: needs the boot test.

## 2026-10-19: boot test, game-mode flag, corrections (third pass)

### Observed
- Rebuilt `dinput8.dll` with `tools/build_proxy_dll.py`: byte-identical to the one in `<test dir>` (SHA-256 `b255edd9...ddabffdc`).
- Boot test with the fixed DLL: `rerev_hook.log` rewritten at launch; 32-bit module list shows the proxy `DINPUT8.dll` from `<test dir>` **and** the system `dinput8.DLL`, so the absolute-path forwarder resolves on Windows 11. `d3d9.dll`, `XINPUT1_3.dll`, `steamclient.dll` loaded; window titled `RESIDENT EVIL REVELATIONS / BIOHAZARD REVELATIONS UE`; process alive and responding at 77 s, then stopped by us. The window was minimized (client rect 0x0), so the main menu was **not** seen.
- String xrefs (`tools/pe_string_xrefs.py`, scans `.text` for the string VA as imm32):
  - `...getData().isCoop()` (`0xff1bf8`): pushed at `0x436948`, `0x436a5b`, `0x43884f` (functions `0x436910`, `0x436970`, `0x4383b0`, all in `cEpisodeEndFlowState.cpp`).
  - `...getData().isCampaign()` (`0xff1cd8`): `0x43858c` (function `0x4383b0`).
  - `isCoopMode() == false` (`0xff2128`): `0x43a240` (function `0x43a210`, `cSceneResultFlowState.cpp`).
  - `cNpcThinkManager.cpp`: `0x614d65`, `0x61e095` (functions `0x614d50`, `0x61e080`).
  - `sCoopManager.cpp`: `0x87eee0`. `cPlayerPadSyncData` type name: `0xf8ab4e`.
- Hand-decoded code before the asserts: `mov ecx,[0x01169108]` (null-check assert from `cGameSystem.h`), `mov eax,[reg+0x24]`, then `cmp dword [eax+0x20],1` for `isCoop()` and `cmp dword [edx+0x20],0` for `isCampaign()`.
- 2,112 references to `0x01169108` in `.text`; 686 of them are followed within 20 bytes by `mov r,[r+0x24]` + a compare on `[r+0x20]` (432 vs 1, 147 vs 0, 107 vs register). No inline `mov [r+0x20],...` writes after such loads.
- Runtime read (ReadProcessMemory, read-only) at 5/15/30/45 s after launch: `sGameSystems=0x221C6460`, `data=0x221C6570`, mode `0` every time.
- Steam store page for 222480 (read 2026-10-19): features are single-player, co-op, online co-op, achievements, trading cards, Steam Cloud, Family Sharing. **No Valve Anti-Cheat entry.**

### Derived
- The game mode is one int: `[[0x01169108]+0x24]+0x20`, `0` = campaign, `1` = coop (Raid). `isCoop()`/`isCampaign()` are inlined everywhere, so there is no single function to hook; a mode flip changes all 686 branches at once (Approach 2 is "set one int", with unknown fallout).
- `data` sits at `sGameSystems+0x110` in this run (allocated inside the singleton).
- The earlier claims "VAC enabled" and "Enigma Protector" were wrong or unsourced; fixed in README, directory and engine-families.
- The directory's `qa: verified-playthrough` for Raid Mode had no test behind it; set to `none`.

### Unknowns
- Mode value inside Raid Mode (expected `1`, not read yet). Other values used via register compares.
- Where the mode is written (setter not found by the inline scan).
- Whether DirectInput is created at boot or at the menu.

## 2026-10-19: Ghidra pass (mode setter, partner AI save/load)

Tools: Ghidra 12.1.4 (`ghidra_12.1.4_PUBLIC_20260921.zip`, SHA-256 `ddac49f9...87d2d4db`, matches the GitHub release) with Temurin JDK 21.0.12.1+1 (SHA-256 `f9d6e191...2d8b4e`, matches the Adoptium API). Headless import + default auto-analysis of `rerev.exe` (no PDB), then Java GhidraScripts (kept local for now, game-specific).

### Observed
- Dataflow scan over 2,942,698 instructions (track `[0x01169108]` -> `+0x24`): **0** direct writes to `data+0x20`; 21 distinct methods called with `ECX = data`. Exactly one of them stores to `this+0x20`:
  - `FUN_008c6960(this=data, int mode)`: `this->mode = mode;` then asserts the singleton at `0x0116a328` and calls `FUN_008b6be0`.
- `FUN_008b6be0`: if `this+0x5bc` differs from the current mode, loops over 3 listener arrays (stride 0x14, from `this+0x88`) and calls each listener's vfunc `+0x2c(mode)`, then caches the mode at `+0x5bc`. A mode-changed broadcast.
- 16 call sites of the setter, with the pushed value:
  - `uTitleMenuManager.cpp` functions: `FUN_0083eb20` -> `1` (enter Raid from the title menu), `FUN_0083c1c0` -> `0` (when `mCoopMenu->+0x2c == 1`, leaving the coop menu).
  - `FUN_007cabe0` -> `1` twice (paths that also set `DAT_011e0388+0x20`, `DAT_011e0558+0xa00`, `DAT_011e057c+0x92` to 1), and once with a register.
  - `FUN_007ce6f0`, `FUN_007cf040`, `0x7cba8f` -> `0` on paths that first call `FUN_0087f1c0(1,1)` and clear `DAT_01169398+0x8f4`; these paths set negative state codes (`-0x62`, `-99`, `-0xc0`, `-199`), so they look like session error/leave handlers.
  - `FUN_00431e60` (state machine switch on `this+100`) -> `1` in case 4, `0` in others. `FUN_008b1660` -> `0` (also sets `DAT_01169104+0x24 = 2`). `FUN_008397c0` -> `1`. `FUN_00402ef0` -> register.
- `FUN_00614d50` / `FUN_0061e080` (`cNpcThinkManager.cpp`, asserts `isSetup()` at lines 0x336/0x358): copy partner-AI state out to / back in from a record (fields `+0x3c0` mode, `+0x3e0..+0x3e8` position-like triple, `+0x43c` float, `+0x488`, `+0x490`, `+0x4a0`). Save/load (checkpoint) of the partner AI, **not** its per-frame update.
- No MSVC RTTI type descriptors (`.?AVcNpcThinkManager@@`, `.?AVuPlayer@@`, `.?AVsCoopManager@@` all absent). MT Framework uses its own DTI: type-name strings are pushed by small registration functions (`0xf9a9c0`..`0xf9aa80` for `sCoopManager::*`, `0xf8ab40` for `cPlayerPadSyncData`).

### Derived
- `0x8c6960` is `cGameSystemData::setMode(int)` (our name). A raw memory write to `data+0x20` would skip the broadcast in `FUN_008b6be0`, so any experiment must **call the setter**, not poke memory.
- `FUN_0087f1c0(1,1)` + `DAT_01169398+0x8f4 = 0` right before `setMode(0)` on error paths: `DAT_01169398` is likely the `sCoopManager` singleton and `0x87f1c0` a leave/reset-session call (guess: both functions sit next to `FUN_0087edc0`, which is in `sCoopManager.cpp`).
- The partner AI's per-frame update has to be found through the MT DTI / vtable of `cNpcThinkManager` (registration function around `0x61c550`), not RTTI.

### Unknowns
- What each listener does on `+0x2c(mode)`; which systems re-init when mode becomes 1.
- Whether `DAT_01169398` really is `sCoopManager` (guess above).
- `cNpcThinkManager` vtable and update method.

## 2026-10-19: Approach 2 experiment, setMode(1) inside a campaign episode

### Setup
- Test copy hardening first: `<test dir>\dat\` and `nativePC\sa\` were NTFS hardlinks to the real install (link count 2). Replaced with physical copies so no game write can reach the real install. Checked: no file in hardlinked `nativePC` changed after the copy was made.
- Experiment DLL (`dinput8.dll`, C, MSVC 2019 x86, built **without the Windows SDK or CRT**: own declarations + import libs generated from stub DLLs). Forwards `DirectInput8Create`; hooks `Direct3DCreate9` in the exe IAT -> `CreateDevice` -> `Present` (vtable slot 17) and runs commands on the render thread. Commands: F9/F10/F11 or a `coop_cmd.txt` file (`mode 1`, `mode 0`, `dump`). Logs mode changes, the mode-listener tables, and exceptions (decodes engine asserts: `FUN_00401370` does `sub esp,0x380`, so the caller is `[esp+0x394]`, then file, line, expression).
- Input bot: PowerShell `SendInput` with scan codes (DirectInput reads them; note the x64 `INPUT` struct is 40 bytes, union at offset 8) + full-screen captures. The game runs borderless full screen (style `0x14000000`), so `CopyFromScreen` works.

### Observed
- Hooks: `Direct3DCreate9(32)`, `CreateDevice` OK, `Present` hooked; mode `-1` (no data yet) -> `0` within 8 frames.
- Mode listeners at the title: 3 tables with 3, 8 and 10 objects (21 total). **Every** listener's `+0x2c` is `0x42E750` = `ret 4` (empty). So the broadcast does nothing: `setMode` is in effect just the field write.
- First launch from an account with no saves: the game asked to create a save; the bot's confirm picked "Yes" and the game created `userdata/<id>/222480/remote/DATA0.DAT` (1,655,972 bytes) on the owner's Steam account (Steam Cloud). There was no save before, so nothing was overwritten. The RESIDENT EVIL.NET data-sharing prompt was answered "No".
- New game, Easy, Episode 1 start (Jill on the ship deck). `setMode(1)` from the file command: returned normally, mode stayed 1, no exception, game kept running (20 s, movement input, menus).
- Pause menu: with mode 0 it shows the campaign objective ("Cerca Chris") top-left; with mode 1 the objective is gone. Menu entries are identical (Restart / Options / Quit).
- `setMode(0)` back: works, objective returns.
- **Restart from checkpoint with mode 1 crashes, reproducibly (2/2 runs):** `ENGINE ASSERT from 0x006D0E17: prog\game\save\../utility/cGameSystem.h line 62: false`. The code at `0x6D0E00` is `mov ecx,[0x0116A7D4]; test ecx,ecx; jnz` = `getInstance()` of a null singleton. Return addresses on the stack include `0x794D35` (`FUN_00794a70`, `game_systems` loop calling `vfunc+0x18` on each scoped_ptr subsystem).
- `0x0116A7D4` has exactly one writer, `0x7929D2` in `FUN_007929a0`, the constructor that loads `archive\game_system\episode\episode`: the **Episode game system** (`sGameSystemsHolder<Episode>`).
- Raid Mode at runtime: entering the Raid menu from the main menu changes mode `0 -> 1`; leaving it changes `1 -> 0`. The Raid menu offers "Single player", "Player match", "Private match".

### Derived
- Approach 2 as a blunt global flip works while an area is loaded, and fails at the first area (re)load: with mode 1 the reload does not keep or create the Episode system (campaign-only), and the campaign save/checkpoint code then asserts on it.
- So mode 1 must not be visible to the system lifecycle (`game_systems` holders / area load), or the Episode system has to be kept alive. Options: (a) flip to 1 only after each area finishes loading and back to 0 before any load/restart/door transition; (b) leave mode 0 and patch individual `isCoop()` sites (686 inline reads) for the behaviours we want; (c) find where the Episode holder is created/destroyed by mode and force it to exist.
- Raid "Single player" runs the coop code path offline: a safe testbed for the netcode side.

### Unknowns
- Which load path skipped the Episode system (holder destroyed on restart vs never recreated).
- What else differs in campaign with mode 1 (enemy sync, partner AI, triggers): the 20 s observation only covered a quiet area.

## 2026-10-19: option (a), mode 1 only between loads

### Observed
- Load signal (watcher on the Episode system `[0x0116A7D4]` and its `+0x5c` NowLoading object): pressing Continue creates the Episode system; a checkpoint restart sets `Episode+0xFC` from `0` to `3` about 36 frames before the Episode system is destroyed, and a new one is built 11 frames later. During play after a restart `+0xFC` reads `1`, so it is a load-state value, not a one-shot request flag.
- `sArrange` arrange loader `FUN_00453340`: builds `archive\game\scene_coop\%x%x\%x%x_arrange` when mode is 1, else `archive\game\scene\...`, for every block in the area except the current one (neighbour streaming). Same pattern in 9 more `sArrange.cpp` functions.
- `cCutFlowState` `FUN_00425e20` (cutscene end): mode 0 advances (`FUN_0084a4a0(3,1)`, state+1); mode 1 writes `sCoopManager(0x01169398)+0x8f0 = 2`, calls `FUN_008768e0` and goes to state 10, waiting for the network partner. A later branch asserts `isCoop() == false` (`cCutFlowState.cpp` line 0xf88).
- Mode-read map (`ReRevModeSites.java`): **375 functions** read the mode. Includes 10+ in `sArrange.cpp`, `cCutFlowState`, `aEpisodeBase`, `cEpisodeEndFlowState`, `cSceneResultFlowState`, GUI (`uGUI_EpisodeTitle`, `uGUI_CutInReward`, `uGuideFlow`).
- Run with a time-based re-arm (mode 1 three seconds after the Episode system appears): mode 1 landed during the Episode 1 intro movie and the game stayed on a **black screen forever** (60 fps, responsive process), also after setting mode 0 again 45 s later. Reproduced 3 times, also with mode masks on `sArrange` and `cCutFlowState`, which were never called while mode was 1 (no mask log), so the blocking reader is another of the 375.
- Mode masks: a naked x86 stub (saves the return address, writes mode 0, calls the original through a trampoline, restores mode 1) hooked on `FUN_00453340` (7-byte prologue) and `FUN_00425e20` (5-byte prologue). Installed and verified in the disassembly; not yet exercised in play (no door or cutscene reached with mode 1).
- Final rule that worked: arm mode 1 by command once the game is visibly playable; while mode is 1, a change of the Episode pointer or of `+0xFC` drops mode to 0 at once; no automatic re-arm. **Checkpoint restart with mode 1 armed: no crash** (state 0->3 seen, mode dropped 7 s after arming, Episode rebuilt, intro replayed), where the plain flip crashed 2/2 times before.
- Player control with mode 1: measured on the minimap, `W` 2 s moves (0,-16) px in mode 0 and (-10,-17) px in mode 1; `S` (+10,+6) in both. Movement works the same in both modes.
- Not done: reaching the first door and the first cutscene. The input bot needs the game in the foreground; the owner switched windows during the walk, so the last bot keys (`W`, `A`) went to the owner's browser and the run was stopped.

### Derived
- Mode 1 is unsafe during any load and during cutscene ends without a network partner: it changes which systems exist (Episode), which assets load (`scene_coop` arranges), and makes flows wait for net sync. "Mode 1 between loads" holds only if the switch happens after the area is fully live and every load start is caught before the load reads the mode.
- The intro soft-lock shows the window between "Episode exists" and "area live" is long and has unknown mode readers; a reliable "area live" signal is still missing (the Episode object fields watched so far do not give one).
- With 375 readers, option (b) (keep mode 0, flip individual coop checks for the behaviours we need, such as player 2 spawn and pad sync) looks more contained than (a).

### Unknowns
- Which reader blocks the intro when mode is 1.
- Door transitions (`sArrange::cNetSyncGoNextBlockData`) with mode 1: likely another wait for the partner, untested.
- A dependable "area live" signal.

## 2026-10-19: owner playtest with mode 1 (Episode 1 start)

The owner played the test copy by hand with the experiment DLL (auto drop on, F9/F10/F11), because the input bot needs the game in the foreground. Data from `rerev_coop.log`; the owner reported what was on screen.

### Observed
- Two DLL attaches 27 s apart (18:36:24, 18:36:51); the first process logged no frames after `CreateDevice`. Likely the exe started directly and Steam relaunched it (SteamStub); not confirmed.
- F9 at 18:38:20 (frame 5256): mode 0 -> 1. Mode 1 held for **173 s (~10,400 frames)** of play in Episode 1: walking, the deck area, no exception logged, no mask call logged.
- 18:41:23 (frame 16237): Episode state `+0xFC` went `0 -> 4` with the same Episode object; auto dropped mode to 0. The owner reports reaching the first video and then seeing the first enemy, with no black screen. Mode stayed 0 afterwards (no automatic re-arm), so the video and the first enemy ran in mode 0.
- `DATA0.DAT` last written at 18:38:02, before F9 (checkpoint/autosave in mode 0). No save was written while mode was 1.
- Neither mask fired: `sArrange` neighbour loading and `cCutFlowState::update` were not called while mode was 1.
- The owner confirms going through at least one door while mode was 1. The log shows no load window (Episode pointer and `+0xFC` unchanged), no mask call and no exception for that door.

### Derived
- `+0xFC = 4` marks a transition into a scripted video (restart was `3`). The state-change rule catches it before the video runs, so the cutscene problem from the plain flip is avoided by dropping to mode 0, not by the mask.
- Door transitions in this area do not start a load the watcher sees, do not stream arranges through `FUN_00453340`, and do not wait for a network partner with mode 1. - Normal play in this area tolerates mode 1 for minutes. What mode 1 changes during play is not yet visible (no partner, no enemy while it was on).

### Unknowns
- Enemy behaviour and combat with mode 1 (enemy sync code `cActivateEnemySyncData`, `cDeathEnemySyncData` reads the mode).
- Meaning of the other `+0xFC` values.

## 2026-10-19: owner playtest 2, first enemy with mode 1

### Observed (log `run 12` + owner report: "no problems")
- Restarted from the checkpoint saved at 18:38:02. F9 at 18:47:13 (frame 2429): mode 1. No load window followed for 2 minutes, so this time no video changed the Episode state (the owner pressed F9 again at 18:49:12 while mode was already 1).
- F11 marks at 18:49:19 and 18:49:23 (first enemy seen, combat) with mode 1.
- 18:49:28: Episode state `+0xFC` `0 -> 2`, same Episode object; auto dropped mode to 0. F11 marks after that (18:49:30, 18:49:44) read mode 0.
- `DATA0.DAT` written at 18:49:36, **8 s after the drop**, so the save ran with mode 0.
- No exception, no mask call.

### Derived
- The combat window with mode 1 was about 9 s (enemy seen -> state change). The state change right after combat fits a scripted event after the first kill (event + checkpoint save), so the enemy most likely died with mode 1 (guess: the log has no kill event).
- `+0xFC` values seen so far: `0` first load/play, `1` play after a restart, `2` event + save after the first enemy, `3` restart requested, `4` transition into a video. Every non-play change came before a load, video or save, and the auto drop caught each one in time.
- Option (a) as built (manual arm, auto drop on any `+0xFC` or Episode change, loader masks) gives stable mode 1 play in Episode 1: walking, a door, the first enemy, a restart, a video and a save transition, with no crash or soft-lock. The only failure left is switching to mode 1 during a load, which the manual arm avoids.
- Mode 1 alone shows no visible gameplay difference except the hidden objective: no second player appears, because players are created at area load, which always runs in mode 0 now.
- Candidate for player-2 work: `FUN_006c31e0` (`uPlayer.cpp`, 9.6 KB) reads the mode 4 times (`== 0` / `== 1`).

## 2026-10-19: player 2, Raid second player vs campaign partner (static analysis)

### Observed (Ghidra 12.1.4 on build 13298487, scripts `ReRevStr`, `ReRevWhere`, property parser)
- MT DTI registrations (`push size; push parentDTI; push name; mov ecx, DTI; call MtDTI::ctor`):
  - `uNpc`: DTI `0x0117B4B8`, size `0x1710`. `uPlayer`: DTI `0x0117B498`, size `0x1760`, **parent `uNpc`**. `sPlayer` (player manager singleton): DTI `0x0117B394`.
  - 32 subclasses of `uPlayer`, all size `0x1760`: `uPl< Jill >`, `uPl< JillEp1_2 >`, `uPl< Chris >` ..., and the campaign partners **`uPl< Parker >`, `uPl< ParkerEp1_2 >`, `uPl< Jessica >`, `uPl< Keith >`, `uPl< Quint >`, `uPl< Rachel >`**, plus Raid-only `uPl< Hunk >`, `uPl< Norman >`, `uPl< PL11A0 >`, `uPl< PL15A0 >` and others.
  - 22 subclasses of `uNpc`, size `0x1710`: civilians and human enemies only (`uNp< Terrorist >`, `uNp< BSAA_AgentA >`, `uNp< ScientistA >`, `uNp< Raymond >`, `uNp< Morgan >`, ...).
- `uPlayer` property list (`FUN_006bfba0`, MT property entries `{name, type}{owner, &field}`):
  - group `pad`: `mMoveAnalog` `+0x1138` (2 floats), `mRotateAnalog` `+0x1140`, `mAimAnalog` `+0x1148`, `mIsRun_Net` `+0x1150` (byte), `mIsAim_Net` `+0x1151` (byte).
  - `NPC思考` ("NPC think", object pointer) at `+0x1630`. Other fields: `mHealthStatus` `+0xf04`, `mLowerStatus` `+0xf10`, `mUpperStatus` `+0xf14`, `mPlayerFlag` `+0xf3c`, `mControlFlag` `+0xf44`, `mLifePoint` `+0x1368`, `mMaxLifePoint` `+0x1370`, `mItemPack` `+0xfe8`, `mVitalControl` `+0x1364`.
- **`FUN_006afbd0` = `uPlayer::updatePad`**. Control type `t = FUN_0049bea0([uPlayer+0xe80])`, where `FUN_0049bea0` is `mov eax,[ecx+0x38]; test; jz ->0; mov eax,[eax+0x34]`:
  - `t == 1`: local player. Reads the real pad (`FUN_0088e610` move, `FUN_0088e730` / `FUN_0088a340` rotate, `FUN_0088e6a0` aim, run/aim buttons) into the pad group, then applies sensitivity tables.
  - `t == 2`: AI. Copies the think manager's virtual pad: `think+0x50/54` -> move, `+0x58/5c` -> rotate, `+0x60/64` -> aim, run from `FUN_00614a50`, aim from `FUN_00615330` (`cNpcThinkManager` methods).
  - `t == 3`: remote. Does not read any pad; values come from network sync. Only a timeout: a timer at `+0x1360` grows each frame and `FUN_006a82d0` clears the pad when it passes a limit or the net check `FUN_004175e0` fails.
- The `+0x34` field is `mPadNo` of the controller class registered by `FUN_00413c60` (`mName` `+0x30`, `mPadNo` `+0x34`, `mFlag` `+0x38`, `mObject` `+0x40`, actions `Add`/`Remove`/`Reset`).
- Writers of the think pointer `+0x1630`: `FUN_006b7170` (`uPlayer.cpp`, scoped_ptr reset: the old think manager is deleted, the new one is from `FUN_006167c0`), `FUN_006b7f40`, `FUN_00573100`, `FUN_00574490`, `FUN_00bd2180`.
- Mission scripts can already switch the partner AI: properties `mIsThinkOffPartner`, `mIsThinkOnPartner`, `mIsLockPartner`, `mIsDrawPartner`, FSM actions `cFsmAction::cSetEnablePartnerParameter`, `cSetPartnerWarpParameter`. Coop-specific AI states also exist: `cNpcThinkCoop_Move`, `cNpcThinkCoop_Combat`, `cNpcThinkCoop_SwimWS/UW`.
- `FUN_006c31e0` (named in the plan) is the `uPlayer` per-frame update; its 4 mode reads only pick tutorial hints / voice prompts (`FUN_00844530`) and the campaign vs coop GUI singleton (`0x0116A344` vs `0x0116A348`). It does not create players.
- 10 player sync types exist: `cPlayerPadSyncData`, `cPlayerVitalSyncData`, `cPlayerDeadSyncData`, `cPlayerDyingSyncData`, `cPlayerApparentDeadSyncData`, `cPlayerHerbSyncData`, `cPlayerEquipSyncData`, `cPlayerThrowGrenadeSyncData`, `cPlayerEmotionSyncData`, `cPlayerCoopKeySyncData`.

### Derived
- The campaign partner is not an NPC class: it is a full `uPlayer` whose control type is 2 (AI). The Raid second player is the same class with control type 3 (remote, fed by `cPlayerPadSyncData`). **Player 2 in the campaign = switch the partner's control type from 2 to 3 (or 1) and feed its pad group.** No new character has to be spawned, and the campaign scripts keep working because the partner they expect is still there.
- Raid's own creation path (how it spawns a type-3 `uPlayer`) was not traced; it is not needed for this route.
- Comparison:
  - Route A, take over the partner: the unit already exists in every episode, with campaign scripts, cutscenes, warps and the `mIsThinkOff/OnPartner` flags. The change is one int (control type) plus writing 5 pad fields per frame. Mode stays 0, so none of the 375 coop branches or the load problems apply. Open: buttons beyond move/rotate/aim/run/aim (shoot, reload, melee, item) come from other code (not yet located for types 2/3); the AI also drives actions through its think states; partner-specific limits (invincibility, no inventory UI, revive rules) may differ from a real player; camera and HUD for the second player do not exist in one window (needs a second instance over the network).
  - Route B, spawn a second player like Raid: needs mode 1 or the Raid creation path in campaign, which collides with the Episode system, `scene_coop` arranges and partner-sync flows seen in the mode experiments, and there would be two partners (AI partner + new player) unless the AI partner is removed.
  - Route A is cheaper and safer. Route B's sync classes (`cPlayer*SyncData`) are still useful as the wire format for route A.

### Unknowns
- Where actions (fire, reload, melee, sub-weapon, interact) come from for control types 2 and 3.
- Whether setting control type 3 on the partner disables its think manager or only bypasses it in `updatePad` (think states may still trigger actions).
- Which value the local player and the partner actually hold at runtime (expected 1 and 2): to verify.

## 2026-10-19: partner takeover test 1 (control type 3 + pad from numpad)

### Observed (log `run 13`, owner report)
- Unit list from `sPlayer` (`[0x01169394]`, lists `+0x34/+0x48/+0x58/+0x70`; class from the vtable's `mov eax, DTI; ret`): `uPl<JillEp1_2>` control type **1**, think pointer non-null; `uPl<ParkerEp1_2>` control type **2**, think pointer non-null; a third unit in list `+0x48` with DTI `0x01174FE4` = `uNp< OBrien >`, type 2, far below the ship. Two more entries are list sentinels/garbage (class `?`, nonsense values).
- F8: Parker `2 -> 3`. Movement from the numpad worked at once: forward and back move Parker in opposite directions (+32 / -65 units on X in 0.2–0.5 s), left and right too (about ±80 units in 0.4 s). Directions follow the camera, not world axes. The owner confirms controlling the character.
- Aim did not work (owner). Cause found in `updatePad` for type 3: `mIsAim_Net` (`+0x1151`) is cleared every frame unless `FUN_006ad740` (weapon allows aiming) and `mUpperStatus == 2` (already aiming). The DLL wrote the flag at Present, before the next `updatePad`, so it never reached the state machine.
- F8 off restored type 2 (AI took over again). The second F8 on (26 s later) set type 3, but Parker did not move for 4 s while input was held (position unchanged). A third F8 on moved him, slowly (6 units in 0.8 s). The owner saw the second takeover as "not getting control back".
- No exception in the log.

### Derived
- Route A works for movement: a campaign partner can be driven by a second input with no new spawn and mode 0.
- Aim (and likely other actions) must be written after `updatePad` runs. The DLL now hooks `updatePad` (`0x6AFBD0`, 6-byte prologue `55 8B EC 83 E4 F8`) and writes the partner's pad group right after the original returns.
- The stuck second takeover is unexplained. Guess: the AI left Parker in a lower-body or think state that ignores pad input (e.g. a follow/warp or guide-wait state). The DLL now logs `+0xf08`, `mLowerStatus`, `mUpperStatus`, `mControlFlag`, `mPlayerFlag` and the think state (`think+8`) at each takeover and when they change.

## 2026-10-19: partner takeover test 2 (updatePad post hook)

### Observed (log `run 14`, owner report: "seems to work correctly")
- `updatePad` post hook installed at `0x6AFBD0`. Parker controlled for about 6 minutes (19:13:51 -> 19:19:43), 155 input changes, no exception.
- F8 cycles: off/on at 19:19:30/31, 19:19:36/38, all three took effect; after each takeover `mLowerStatus` toggled `0 <-> 2` (idle / moving) within 1–2 s. The stuck second takeover from test 1 did not happen again.
- Status fields during control: `+0xf08 = 0`, `mControlFlag = 0x3C`, `mPlayerFlag = 0x400` constant; think state (`think+8`) mostly `0`, briefly `3` while Parker was moved (19:19:01–04).
- Run (`+`) pressed 27 times and aim (`0`) 34 times; both reached the pad group. **`mUpperStatus` stayed `0` in every sample**, also while aim was held, so the log does not show an aim state.

### Derived
- Route A movement is solid: repeated switching between AI and second-player control works, and the AI stays in its idle think state while bypassed.
- Aim works: the owner confirms Parker raised the weapon and aimed while numpad 0 was held. So `mUpperStatus` (`+0xf14`) is not the aim indicator, or it is sampled at the wrong moment; the value `2` checked in `updatePad`'s type-3 branch means something else (unknown).

## Next steps
1. Fire / reload / melee for a type-3 unit (`cPlayerPadSyncData` apply / action sync).
2. Second input source over the network (second instance), replacing the numpad.
