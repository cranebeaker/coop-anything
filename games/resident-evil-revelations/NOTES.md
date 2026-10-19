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

## 2026-10-19: fire and reload path for the partner (static analysis)

### Observed
- Pad queries for the local player are methods of an input manager at `[0x011E0388]`: 2 pad slots of `0x90` bytes at `+0x628`, current slot index at `+0x870` (checked `< 2`), enabled flag at `+0x882`. `FUN_008888c0` run button, `FUN_00888590` aim button, `FUN_0088e610` move stick, `FUN_00888190` current slot's device type.
- Only 9 direct calls of the control-type getter `FUN_0049bea0`: 3 in `updatePad`, 2 "== 3" branches in hit/damage code (`FUN_0064a650`, `FUN_00655110`, with `FUN_00881cf0` / `FUN_00881d10`, likely session active / is host), 4 "== 1" branches (`FUN_006ac4b0`, `FUN_006cd540`, two outside known functions).
- AI action requests are bit flags at **`think+0x158`** (think = `[uPlayer+0x1630]`), read by AI-specific state controllers:
  - `FUN_0069c880`, `FUN_0069df60`, `FUN_0069eb60` (upper-body weapon controllers): while aiming (`+0x1151` set or `+0xf08 == 9`), **bit 0** fires (states 2/3/4 through `FUN_005f40b0` / `FUN_005c3e50`); bit 0 clear returns to state 0. **Bit 1** with `FUN_006b3840` gives state 6 (reload, guess from context).
  - `FUN_006999a0`: bit 1 with `FUN_006b3840` -> lower state 9; aim request via `FUN_00615330` -> state 3.
  - `FUN_0068f960`: bit 3 -> state 6 (when `FUN_006adcf0`), bit 4 -> state `0x45` (when `+0xf08 == 3`); stick input -> state 5.
  - `FUN_00694480`, `FUN_006931b0`: bit 6 -> state `0x43`, bit 5 -> state `0x42`.
  - `FUN_006af2d0`: bit 2 -> weapon switch to the AI-chosen weapon (`FUN_006178e0`), only when `uPlayer+0xe30 == 2`.
- `uPlayer+0xe30` is a second "controller kind" field (1 player, 2 AI) that picks these AI controllers; the control-type switch to 3 does not change it, which is why the partner kept using the AI upper-body controller (that controller also reads our `+0x1151` aim flag, so aiming worked).

### Derived
- For the partner, fire and reload can be driven by writing bits 0 and 1 of `think+0x158` right after `updatePad`, the same place as the pad group. Implemented in the DLL: numpad 1 = bit 0 (fire), numpad 3 = bit 1 (reload), numpad 2 = bit 3 (experiment). The DLL logs when the AI had set other bits before our write (to see whether the AI still issues actions).

## 2026-10-19: partner fire/reload test (think+0x158 bits)

### Observed (log `run 15`, owner report)
- Two game sessions. While aiming (`aim=1`) the fire bit (bit 0) was written in 37 input changes, reload (bit 1) in 10, bit 3 in 14. The AI set its own bits before our write only once (`08`). `uPlayer+0xe30` read `2` throughout.
- Owner: aiming works, but **fire does nothing**, reload (numpad 3) does nothing, numpad 2 does nothing visible. The status log shows `mLowerStatus` values 3, 4 and 6 at times; not tied to a key yet.
- Two first-chance access violations at `0x778F34D9` (system DLL, heap-like registers), both with mode `-1` (game systems already torn down) and about 2 s after the last input of each session, at 19:34:08 and 19:36:06. A new process started at 19:34:40. The owner closed the game with Alt+F4 both times: these are shutdown-time exceptions, not crashes in play.

### Derived
- Writing bits 0/1 of `think+0x158` does not make the partner fire or reload, even though the bits persist and aiming works. Either `FUN_0069c880` is not the controller that runs for the partner, a condition before the fire branch fails (`FUN_006a8050`, `+0xfec`, ammo checks), or the AI fires through another path.

## 2026-10-19: what changes when the local player fires (observer on Jill)

The partner never fires in the current area, so the F6 observer watched Jill (control type 1) while the owner aimed, fired about 6 shots and lowered the weapon. Log `run 17`.

### Observed
- Think manager fields of Jill: no change at all. Unit fields that changed (besides sticks and waist rotation): `+0xf10`, `+0xf14`, `+0xf18`, `+0xf80`–`+0xfac`, `+0x1230/+0x1234`, `+0x1590`, `+0x15e0/+0x15e8`, `+0x16a4`.
- Upper-body state machine (data from `+0xf90`): `+0xfa8` current state, `+0xfac` previous state, `+0xf94` sub-step, `+0xf80/+0xf84` state object pointers.
  - Raise weapon: `+0xf14` `0 -> 2`, `+0xfa8` `2 -> 3`, motion `+0x1230` `0x465`, then `0x404` (aim hold).
  - **Each shot**: `+0xfa8` `3 -> 6`, motion `0x552`, `+0x1590` bit 0 set for exactly one frame, back to `3` (motion `0x404`) about 0.9 s later. Seen 4 times in the first 12 s, same pattern each time.
  - Lower weapon: `+0xfa8` `3 -> 2`, `+0xf14` `2 -> 0`, motion `0xFFFF`.
  - Once `+0xfa8 = 9` with motion `0x56A` for 1.3 s right after a shot sequence (reload, guess).
- `+0xf10` (lower) / `+0xf18` (rotate) flick `0 <-> 4` and `0/1/2` during small turns.

### Derived
- `+0xfa8` is `+0xf90 + 0x18`, the slot `FUN_0049b490` writes (`*(*(ctrl+4)+0x18) = state`), so writing it requests the next upper-body state. Fire = state 6 from aim state 3.
- `mUpperStatus = 2` is the aim state for the local player; it stayed `0` for the partner in test 2 even though he visibly aimed, so the partner's aim pose comes from another path (AI controller).
- DLL change: numpad 1 requests state 6 when the partner's `+0xfa8 == 3`, numpad 3 requests state 9; the partner's `+0xf14`, `+0xfa8`, `+0xf94`, motion and shot pulse are logged on change. The AI action-bit writes were removed.

## 2026-10-19: partner fire via upper state 6 (test)

### Observed (log `run 18`, owner: "does not fire")
- While the owner aimed with the partner (aim flag set in 23 input changes) and pressed fire (16) and reload (7), the partner's upper-body state machine never changed: `+0xfa8 = 2`, `+0xf14 = 0`, `+0xf94 = 0xFFFFFFFF`, motion `0xFFFF` for the whole session. No fire/reload request was issued because the state was never `3`.

### Derived
- The partner's visible aim does not run through the upper-body state machine at `+0xf90`. With the AI controllers (`+0xe30 = 2`), aiming is likely a lower/whole-body state (the AI transition `FUN_006999a0` requests state 3 for aim; earlier status logs showed `mLowerStatus` 3/4/6).

## 2026-10-19: observer on the partner while aiming (control type 3)

### Observed (log `run 20`; run 19 observed O'Brien by mistake, fixed)
- Learning muted 2 unit fields and 1 think field. While the owner aimed three times, released, and pressed fire/reload while aiming, only these changed: `unit+0x1150` (the run/aim flag bytes, 6 times), `unit+0x1180` (waist rotation), `think+0x48`, `think+0x60`. No state-machine field changed (`+0xf10`, `+0xf14`, `+0xf80`–`+0xfac` all still).

### Derived
- With control type 3 the partner's raised weapon is only the aim pose (flag + waist), not an aim state: no controller runs aim -> fire logic for him (the AI controllers apparently run only under type 2). Fire has to be driven on his upper state machine directly.
- DLL change: aim key requests upper state 3 from 2, release requests 2 from 3; fire (6) and reload (9) requests as before.

## 2026-10-19: writing the partner's upper state directly (test)

### Observed (log `run 21`, owner: "nothing happens")
- Aim key: `+0xfa8` written `2 -> 3`; fire key: `3 -> 6`. The values stayed as written, but `+0xf94` stayed `0xFFFFFFFF`, motion stayed `0xFFFF`, `+0xf14` stayed `0`, no shot pulse. The state then sat at `6`.

### Derived
- `+0xfa8` is only storage: the partner's upper state machine is not being ticked under control type 3 with controller kind `+0xe30 = 2`. On the local player the same machine runs (state objects at `+0xf80/+0xf84` change with each state).
- Guess: the Raid second player is control type 3 with controller kind `+0xe30 = 1`, whose player controllers read the network fields (`mIsRun_Net`, `mIsAim_Net`). DLL change: F8 also sets `+0xe30 = 1` (restored on F8 off); the direct state writes were removed.

## 2026-10-19: controller kind +0xe30 = 1 (test) and the player upper-body controller

### Observed (log `run 22`, owner report)
- With `+0xe30 = 1` and control type 3, the partner's upper state machine runs by itself when the aim flag is set: `+0xf14 0 -> 2`, `+0xfa8 2 -> 3`, `+0xf94 = 1`, motion `0x465 -> 0x404`; on release `+0xfa8 -> 2`, `+0xf94 = 7`. 12 identical cycles, no exception. Same values as the local player.
- Still no shot. Owner: right mouse button during the partner's aim makes the partner "tremble".
- Player upper-body controller `FUN_0069c2f0` (vtable ref at `0x1064F3C`): in aim, `FUN_0088c880(FUN_006256a0())` true and `FUN_006b3840()` -> state 6 (fire); `FUN_0088ca00` + `FUN_006adf20(0)` -> state 9; `FUN_0088cab0` + `FUN_006ae000` -> state 10; `FUN_0088a050` + `FUN_006adf20(1)` -> state 9. `FUN_006256a0` is `return 0`; the four button functions are input-manager methods reading the shared input manager's current slot.

### Derived
- Controller kind `+0xe30` selects player vs AI controllers. Player controllers + control type 3 give a working remote player for aim; fire/reload buttons are read from the shared (local) input, which is also why the local mouse disturbed the partner.
- DLL change: `uPlayer::update` (`0x6C31E0`, virtual) is wrapped; while it runs for the partner the input manager is disabled (`+0x882 = 0`) and the four button functions return the partner's keys (numpad 1, 3, 2, decimal). Query counts are logged every 300 frames.

## 2026-10-19: input context via uPlayer::update (test)

### Observed (log `run 23`, owner report)
- All hooks installed (`uPlayer::update` `0x6C31E0`, four button functions). During the whole session the partner context never became active (`inP2 = 0`) and the button functions were never called in it (all counters 0).
- When the local player aimed while the partner was taken over, the partner's upper state flipped `3 <-> 2` every 4–5 frames for seconds (motion `0x465` / `0xFFFF`), seen by the owner as "Parker repeats the aim animation on/off, trembling".

### Derived
- `0x6C31E0` is not the path that runs the partner's controllers (or it is not called with the unit pointer); the local aim button still reaches the partner's controller and fights the partner's own aim flag.
- DLL change: the context is also set by wrapping the upper-body controller `FUN_0069c2f0` itself (unit from its stack argument), the aim button `FUN_00888590` is added to the context-aware buttons (returns the partner's aim key), and the log reports which `this` pointers reach `uPlayer::update` and how often the controller runs for the partner.

## 2026-10-19: which controller runs for the partner (diagnostics) and upper-body state vtables

### Observed (log `run 24`, owner: still no shot, aim animation keeps switching, Parker inches forward)
- `uPlayer::update` (`0x6C31E0`) is called for three units about 1.5 times per frame each: Jill `241E2ED0`, Parker `241E4E10`, and `2428E400` (the O'Brien NPC). So the context did activate for Parker, yet no button function was called inside it.
- The player upper-body controller `FUN_0069c2f0` ran 280 times, all for Jill, never for Parker.
- Upper-body state classes have 11-slot vtables in `.rdata` `0x1064E9C`–`0x10650AC` (one per state family). Pattern: slot 4 = local-player update (`0x69C2F0`, `0x6A0770`, ...), **slot 5 = remote update** (`0x69C830`, `0x69DE40`, `0x69EB00`, `0x69F240`, `0x69FB20`, or the empty `0x42E750`), slot 6 = AI update (`0x69C880`, `0x69DF60`, `0x69EB60`, ...). `0x1064E14` is a 33-slot table of a different shape.
- The remote updates only follow the aim flag: `FUN_0069c830` toggles the aim pose with `+0x1151`/`+0xf08 == 9` and calls `FUN_006b42c0`; no fire, reload or button logic.

### Derived
- With `+0xe30 = 1` and control type 3 the partner runs the slot-5 (remote) updates. In the Raid, a remote player's shots must come from network packets that set upper-body states directly; nothing local decides them.
- The aim oscillation means another writer sets the partner's `+0x1151` (guess: the AI think code mirroring the player's aim).
- DLL change: `FUN_0069c830` is hooked to force `+0x1151` to the partner's aim key right before it reads; fire / reload request upper state 6 / 9 from aim state 3 again (now that the machine runs).

## 2026-10-19: remote controller hook (test) and a corrected reading of the AI fire bit

### Observed (log `run 25`, owner: "nothing changed")
- `FUN_0069c830` (remote, slot 5) was never called for the partner (counter 0); `FUN_0069c2f0` (local, slot 4) ran only for Jill when she aimed. No fire request was logged: the partner's `+0xfa8` was never `3` at the check point. The `3 <-> 2` oscillation continued.
- Static search for the slot dispatcher (reads of `+0xe30` near indirect calls, scaled-index vtable calls, `[vt+0x10/0x14/0x18]` triads) found nothing.

### Derived
- For the partner in aim state 3 the remaining candidate is slot 6, the AI update `FUN_0069c880`. Re-reading it with the local player's numbering (state 6 = fire): **bit 1 of `think+0x158` with `FUN_006b3840` leads to state 6 (fire)**; bit 0 leads to states 2/3/4. The earlier bit test (run 15) had bit 0 as fire and ran while the partner's state machine was not ticking (`+0xe30 = 2`), so it was not a fair test.
- DLL change: `FUN_0069c880` hooked (prologue `55 56 8B 74 24 0C`); for the partner it counts calls, logs callers (to find the dispatcher), forces `+0x1151` to the partner's aim key and writes `think+0x158` bits right before the read: numpad 1 -> bit 1 (fire), numpad 3 -> bit 0.

## 2026-10-19: hot-reload split of the experiment DLL

### Observed
- Each test needed the owner to quit and relaunch the game. The DLL is now split: `dinput8.dll` (loader: proxy, `Direct3DCreate9 -> CreateDevice -> Present` chain, exception logging with the assert decoder) loads `coop_logic.dll` on the first Present and calls its `coop_frame` every frame. F5 or a file `coop_reload.txt` makes the loader call `coop_shutdown` (partner restored, every game-code patch restored from saved bytes), copy `coop_logic.dll` to `coop_logic_live<N>.dll` and load that copy. Old copies are never freed, so a game thread still inside an old stub keeps valid code.
- Both built with MSVC 2019 x86 without SDK/CRT; exports and imports checked with dumpbin (loader 6 KB, logic 15 KB).

### Derived
- Experiments can be swapped while the owner keeps the game open next to Parker. Remaining risk: patching game code while another engine thread executes those bytes (not seen so far; hooks were installed before gameplay until now).

## 2026-10-19: hot-reload tests 1–2 and the state machine layout

### Observed (logic gen 1 and 2, game kept open)
- Hot reload works: `logic gen 2` loaded at 20:31:54 while playing; shutdown restored all patches; no exception.
- The AI upper controller `FUN_0069c880` does run for the partner (16–40 calls), called from **`0x0049B3D3`**, but only during the short `3 <-> 2` flips; the owner had held the aim key (20:30:26–33) with no state change. In no run so far did the aim key alone put the partner into upper state 3; the state-3 entries came from the AI copying the player's aim.
- Writing `+0xfa8 = 3` with `+0xe30 = 1` (gen 2): the value stays but `+0xf94` stays `0xFFFFFFFF` and no motion plays: the transition is not executed.
- At 20:33:14 the partner's unit pointer changed (`23F34E10 -> 23F34380`, same class); the DLL released it as designed.
- Dispatcher `FUN_0049b3a0(sm, mode)` (virtual): state object at `sm+0xc`, owner at `sm+8`; mode 1 -> state slot `+0x18`, mode 3 -> `+0x1c`, mode 2 -> `+0x20`, then slot `+0x24` always. So the earlier "slot 4 local / 5 remote / 6 AI" reading was wrong.
- State machine properties (`FUN_0049b4a0`): `owner` `+8`; `nowState`: `state` `+0xc`, `process` `+0x14`, `branch` `+0x18`; `beforeState`: `state` `+0x10`, `process` `+0x1c`, `branch` `+0x20`; `mIsStateEnd` `+0x24`; flags `+0x26`, `+0x27`. `FUN_0049b490` writes `nowState.branch`, `FUN_0049b150` reads it, `FUN_0049b480` writes `nowState.process`.

### Derived
- The upper-body state machine of a unit sits at `unit+0xf74`: `nowState.state` `+0xf80`, `beforeState.state` `+0xf84`, `nowState.process` `+0xf88`, **`nowState.branch` `+0xf8c`**, `beforeState.branch` `+0xf94`, `mIsStateEnd` `+0xf98`. This matches the local player's log (the `+0xf84` value copies `+0xf80` on each transition). `+0xfa8`/`+0xfac` are a separate state id pair that the game updates; writing them does not request anything. The real request slot is `+0xf8c`.

## 2026-10-19: observing the AI partner firing at enemies

### Observed (F7 observer on Parker under AI, owner pressed F11 at shots; about 7 shots and one reload)
- Aim entry: `+0xf88` (`nowState.process`) `0 -> 1`, `+0xf8c` (`nowState.branch`) `-1 -> 0` for one frame, `+0xfa8` `2 -> 3`, `+0xf14` `0 -> 2`, motion `0x465 -> 0x404`, aim flag `+0x1151 = 1`.
- **Each shot**: `think+0x158` = `1` (bit 0) for one frame; next frame `+0xf80` (state object) changes, `+0xfa8` `3 -> 6`, motion `0x552`; the bit returns to 0. About 1 s later `+0xf8c` is `0` (or `2`) for one frame and the state returns to `3` (motion `0x404`).
- **Reload**: `think+0x158` = `2` (bit 1), `+0xf14 2 -> 3`, `+0xfa8 -> 0` (weapon lowered), then `think+0x158 = 3`, `+0xfa8 = 9`, motion `0x56A` for about 2 s, back to aim.
- `+0x1590` (the local player's shot pulse) did not change for the AI shots.

### Derived
- AI action bits: **bit 0 = fire, bit 1 = reload** (the first reading was right; the later swap was wrong).
- The earlier fire tests failed because the partner never truly entered aim state 3 (the aim key only set the pose; writes to `+0xfa8` are not requests). Aim entry is a request through `nowState.branch` (`+0xf8c`), as `FUN_0049b490` does.
- Logic gen 3 (hot-reloaded): the aim key writes `+0xf8c = 3` when the partner is lowered (`+0xfa8 == 2`) and no request is pending; in the AI aim controller numpad 1 -> bit 0 (fire), numpad 3 -> bit 1 (reload).

## 2026-10-19: player 2 fires and reloads (logic gen 3)

### Observed (owner: "FUNZIONA"; log after `logic gen 3 loaded`)
- F8 at 20:39:56 (partner `uPl<ParkerEp1_2>`, control type 3, controller kind 1). Aim request via `nowState.branch = 3` at 20:39:58. From then on the AI upper controller ran for the partner (caller `0x0049B3D3`).
- **7 shots** from numpad 1: `+0xfa8 = 6`, motion `0x552`, `+0xf94 = 2`, about one per second (20:40:00.7, 01.8, 02.8, 06.7, 07.8, ...). **5 reloads** from numpad 3 (and once right after the aim request): `+0xfa8 = 9`, motion `0x56A`. No exception.
- The first request (`branch = 3` from the lowered state) went straight to state 9 (reload), not to aim.

### Derived
- Player 2 in the campaign now moves, runs, aims, fires and reloads through the partner unit, with mode 0, no new spawn and no network yet. Recipe: control type `3` (`[[[u+0xe80]+0x38]+0x34]`), controller kind `+0xe30 = 1`, pad group written right after `updatePad`, aim flag `+0x1151` and AI action bits `think+0x158` (bit 0 fire, bit 1 reload) written right before the AI upper controller `FUN_0069c880` runs, and a `nowState.branch` request to leave the lowered state.
- Branch `3` from the lowered state means reload, not aim; the AI uses branch `0` for aim entry (seen in the observer log). To fix.

## 2026-10-19: online plan, stage 1a built; Raid-session idea

### Observed
- `steam_api.dll` (2024 build) exports the flat API: `SteamClient`, `SteamAPI_GetHSteamUser/Pipe`, `SteamAPI_ISteamClient_GetISteamNetworking`, `SteamAPI_ISteamNetworking_SendP2PPacket/IsP2PPacketAvailable/ReadP2PPacket/AcceptP2PSessionWithUser`. `rerev.exe` requests `SteamClient017`, `SteamNetworking005`, `SteamUser019`, `SteamFriends015`, `SteamMatchMaking009`.
- Logic DLL stage 1a built (not yet run): `coop.ini` (`role`, `transport=udp|steam`, `peer`, `port`); client sends its local player's pad group plus cumulative fire/reload counters (local upper state entering 6 / 9) as a 40-byte `RCI1` packet each frame; host uses it for the taken-over partner while fresh (< 500 ms), fire/reload as 3-frame bit pulses. Steam P2P on channel 7, unreliable; UDP via ws2_32 loaded at runtime.
- Aim entry for the partner changed to `nowState.process = 1` + `branch = 0` (as the AI does), since `branch = 3` led to reload.

### Derived
- Owner reminder: the game ships a working online co-op mode (Raid). The blocking problems with mode 1 in the campaign (cutscene end waiting for the partner, net sync of enemies/doors) assume a live network partner. Inside a real Raid session those waits complete and the native sync classes (`cActivateEnemySyncData`, `cDeathEnemySyncData`, `cNetSyncGoNextBlockData`, `cPlayerPadSyncData`, ...) are active. Route B, "campaign inside a Raid session", could reuse all of it; known blockers: Episode system missing under mode 1 (save asserts), `scene_coop` arranges (mask exists), starting a campaign episode instead of a Raid stage. Policy: it would use the game's own Steam lobbies (private, friends only); VAC is not listed; owner decision.

## 2026-10-19: route B feasibility, part 1 (static + campaign runtime)

### Observed
- Game-system holders (MT DTI): `sGameSystemsHolder<Resident>` `0x118133C`, `<Game>` `0x118135C`, `<Episode>` `0x118139C`, `<Cut>` `0x118137C`; `sCoopManager` DTI `0x1183E78` (size `0xE90`).
- `FUN_00794e10` (factory, referenced from table `0x107DB04`) builds the Episode system: loads `archive\game_system\episode\episode` and creates 19 subsystems, including `sPlayer` (`FUN_006a7040`).
- The Episode constructor `FUN_007929a0` is called from two places, neither gated by the mode: `FUN_0042a400` (creates it once the episode archive is loaded) and `FUN_0043c960` (creates `sGameSystems` and the Episode system if missing). `FUN_0042a400` and `FUN_0042b720` are both called from `FUN_0042c1f0`.
- `FUN_0042b720` (`aEpisodeBase.cpp`) picks the two character ids of an area: mode 0 from episode data `[0x0116A340]+0x24/+0x28`; mode 1 from the session (`sCoopManager+0xE4` for the local player, `+0xE8` for the other slot when `+0xF4` is set; `FUN_00882d60` = local slot index), then loads both characters' resources. The episode framework (`aEpisodeBase`) serves campaign and Raid.
- Runtime, campaign (Episode 1, mode 0): Episode system `220455E0` (state 0); `sCoopManager` exists (`22028F30`) with `+0xE4 = +0xE8 = 0x80011010`, `+0xF4 = 0xFFFFFFFF`; episode data `+0x24 = 0x80011110`, `+0x28 = 0x80011120` (the two campaign characters); units `uPl<JillEp1_2>` (control 1), `uPl<ParkerEp1_2>` (control 2), `uNp<OBrien>`.

### Derived
- Raid stages most likely run through the same episode framework with mode 1, taking characters from the session instead of the episode data. In a session the partner slot would be filled natively by the network player. Needed next: the same snapshot inside a Raid stage.

## 2026-10-19: route B, Raid snapshot attempt 1 (invalid: auto forced mode 0)

### Observed
- The DLL's auto mode (on by default since the mode experiments) dropped mode `1 -> 0` at 22:20:30 when the Raid stage started loading (Episode pointer was null in the menus). The Raid single-player stage then ran with mode 0.
- In that stage: units `uPl<Jill>` (control 1) and **`uPl<ParkerLight>` (control 2, think manager present)**; Episode system present (`2203A490`); episode data `+0x24 = 0x80011010` (Jill, the Raid character), `+0x28 = 0x80011420` (ParkerLight); `sCoopManager+0xF4 = 0`, `+0x8F4 = 2`.
- Later snapshots (22:26 on) were back in the campaign (`uPl<JillEp1_2>`, `uPl<ParkerEp1_2>`, episode data back to `0x80011110/0x80011120`).

### Derived
- A Raid stage loaded with mode 0 builds an AI partner and the Episode system, like a campaign area: the stage loader is shared and follows the mode. This is the opposite direction of route B, but it shows the two content types run through one path.
- Auto mode switched off for the next attempt.

## 2026-10-19: route B, Raid snapshot attempt 2 (valid, mode 1)

### Observed (Raid single player, stage 1, auto off)
- Mode `0 -> 1` at stage start (22:32:56). The DLL's `sArrange` mask fired once (a neighbour block loaded with mode 0, i.e. campaign arranges inside the Raid stage); no visible problem, but the masks must be off for Raid tests.
- **Episode system present** (`22038910`, state 0) in the Raid stage.
- Episode data: `+0x24 = 0x80011010` (the local Raid character, Jill), `+0x28 = 0x80000000` (empty second slot: solo), `+0x04 = 0xFFFFFFFF` and `+0x14 = 0` (campaign: `0x436FEC58` and `0xFFFFFFFF`), `+0x18 = 0x020007D0` (same value as campaign Episode 1).
- Units: one `uPl<Jill>` (control 1); no partner. `sCoopManager`: `+0xE4 = +0xE8 = 0x80011010`, `+0xF4 = 0`, `+0x8F4 = 2`.

### Derived
- The Episode system is not campaign-only: Raid stages create it too. The checkpoint-restart crash with mode 1 in the campaign is specific to that restart path, not a missing system in coop. Route B's main blocker shrinks.
- In a session the second character slot (`episodeData+0x28`) is where the network player's character goes; that is the native player 2.
- Still unknown: which value selects the stage/episode (the `+0x18` value is identical for Raid stage 1 and campaign Episode 1, both on the ship), how the Raid flow tells both machines what to load, and how campaign scripts, cutscenes and saves behave under mode 1 with a real partner.

## 2026-10-19: route B, tables and episode/scene ids (static)

### Observed
- One manager loads both tables: `archive\game\campaign_table\campaign_table` (`FUN_006f66a0`) and `archive\game\coop_table\coop_table` (`FUN_006f74d0`, `prog\game\coop_table\sCoopTableManager.cpp`), plus `coop_table_dlc`.
- Field names in the binary: `mStageNo`, `mEpisodeNo`, `mSceneNo`, `mEpisode` (registered by the Episode system's property list `FUN_00793660`), `mEpisodeID`, `mEpisodeList`, `mStage`, `mStageData`, `mJumpTable[i].mStageNo`, `mNowStageNo`.
- `FUN_00442080` registers a structure with `mEpisodeNo` at `+4` and `mSceneNo` at `+8` (area flow code).
- No readable property names for `sCoopManager::cStageInfo`.

### Derived (feasibility verdict so far)
- For: campaign and Raid share the episode framework (`aEpisodeBase`), the stage loader, the Episode system, the player-slot logic (slot 2 filled from the session in mode 1) and the table manager. The blocking problems seen with mode 1 in the campaign (cutscene end waiting, partner sync) need a live partner, which a Raid session provides.
- Open: (1) which episode/scene id the loader receives and whether a Raid session can be pointed at a campaign episode/scene; (2) campaign scripts, objectives and cutscenes under mode 1 (375 mode readers); (3) campaign saving under mode 1 (the restart crash path); (4) both machines must load the same thing: the host's choice must reach the client through the session's own messages.
- Verdict: **plausible, not proven.** The decisive experiment can be done offline and solo: in a Raid single-player stage start, replace the requested episode/scene with a campaign one (with the arrange mask on) and see whether a campaign area loads and plays in mode 1.

## 2026-10-19: route B decisive test setup

### Observed (static)
- `FUN_00415b30` registers a stage load request: `mRequestData`, `mStageNo`, `mBlockNo`, `mArchive`, `mArchiveName`, `mArrangeHandle`, ... The area to load is a (stage, block) pair, the same two arguments `sArrange` functions take.
- `FUN_00446a80`: jump point with `StageID`, `mBlockNo`, `mEpisodeNo`, `mSceneNo`, `mCutNo`, `mListNo`. `FUN_0044cfd0`: saved character state with `mCharaID`, `mEpisodeNo`, `mSceneNo`, `mStageNo[2]`, `mBlockNo[2]`, `mPosition`, `mVitality`, `mIsCoop`.
- Prologues of the 11 mode-reading `sArrange` functions are hookable (5–8 bytes, no relative branches).

### Derived
- A Raid stage and a campaign area on the same (stage, block) share geometry; what differs is the arrange set (`scene_coop` vs `scene`: enemies, items, doors, events) and the episode script position. So "campaign content in a Raid session" can be tested by running all `sArrange` code with mode 0 inside a mode-1 Raid stage.
- Logic gen 5: 12 mask hooks (11 `sArrange` + cut flow), groups off by default, toggled by F4 / file `arr` (arranges) and file `cut`; the neighbour loader logs every new (stage, block) with the mode. Auto mode is now off by default.

## 2026-10-19: route B decisive test 1 (arrange masks) and the event archives

### Observed (owner: "everything is the same")
- Campaign Continue loaded stage 1 block 2 (mode 0); Raid stage 1 loaded stage 1 block 1 (mode 1): same ship stage.
- With the arrange masks on, a Raid stage start hit only three masked functions (`45B2F0`, `4629F0`, `4688F0`, 5 calls). The owner saw no difference.
- The stage content choice that was missed: two more mode reads pick `scene` vs `scene_coop` for the **event archives**: `FUN_0042bfc0` loads `archive\game\%s\%1x%1x\%1x%1x_event` (area events; called from the `aEpisodeBase` update chain `FUN_0042c1f0`), `FUN_0041b710` loads `archive\game\%s\%1x%1x\%1x%1x_%02x\%1x%1x_%02x_event` (scene events). Ids come from `FUN_0078fda0/FUN_0078fb10/FUN_0078fb50` and `FUN_0078fc10/FUN_0078f560` on the area id object.

### Derived
- Test 1 did not reach the content choice, so it says nothing yet about feasibility. Logic gen 6 adds both event-archive loaders to the F4 mask group.

## 2026-10-19: route B decisive test 2: campaign content inside a Raid session (works in part)

### Observed (Raid single player stage 1, F4 masks on before the stage; owner report + log)
- Masked code ran with mode 0 inside the mode-1 Raid stage: area event archive loader `42BFC0` (3 calls), scene event archive loader `41B710` (>1200 calls), `sArrange` `45B2F0`, `4629F0`, `4688F0`; stage 1 block 1.
- Owner report:
  - The mission objective was the campaign one ("save/find Chris"): **correct**.
  - Items and enemies were at their **campaign positions**: correct.
  - The first campaign video played.
  - Raid HUD elements stayed: the key icon at the screen side, enemy HP bars and damage numbers like the Raid.
  - No Parker (expected: the second slot is empty in a solo session).
  - Dialogue lines were wrong.
  - The Raid stage **ended right after killing the first enemy and seeing the first video** (Raid results).
- No exception logged.

### Derived
- **Route B works in principle**: a Raid session (mode 1, its own netcode and session) loaded and ran campaign Episode 1 content (objective, arranges, events, a cutscene) once the arrange and event-archive choices were made with mode 0. Geometry is shared; content comes from the `scene` archives.
- Open problems, in order:
  1. Episode progression: the campaign event that should continue the episode (next block / next cut) ended the Raid stage. The coop branches of the end/result flow states (`cEpisodeEndFlowState`, `cSceneResultFlowState`, area flow) treat it as a stage clear.
  2. Raid presentation: HUD key, enemy HP bars, damage numbers (GUI and enemy code reading the mode or the coop tables); cosmetic but visible.
  3. Dialogue lines: wrong set, probably a mode- or partner-dependent voice/subtitle table (unknown which).
  4. Player 2: in a real session the second slot is the friend's Raid character, native network player (untested).
  5. Enemy stats: Raid levels/HP scaling may apply (coop tables); unknown.
  6. Saving campaign progress under mode 1 (the restart crash path).

## Next steps
1. Find what ended the stage after the first video (end/result flow states under mode 1) and route it to the campaign continuation.
2. Then HUD/enemy presentation masks, dialogue table, and a two-player session test with the friend.
