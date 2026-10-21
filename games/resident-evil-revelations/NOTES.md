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

## 2026-10-19: why the Raid stage ends after the first video (setup)

### Observed
- Owner (campaign knowledge): after the first cutscene the campaign changes level to the second area (the beach) with a tutorial. A Raid stage is a single area, so the campaign's "go to next area" lands in the coop flow that ends the stage (results).
- The three `+0xFC` accesses found in `0x797xxx` belong to `uGuideFlow` (`sCoopManager+0xFC`), not to the Episode system: false lead.
- `ReRevPrologue.java` computes safe hook prologues (whole instructions, >= 5 bytes, no branches) for a list of functions; 37 of 40 mode-reading area/flow functions in `0x412600`–`0x4519B0` qualified (skipped `0x42DE90`, `0x431510`, `0x433720`).
- Logic gen 7: count-only probes on those 37 functions (stub = `inc counter; jmp trampoline`), first call per probe logged with mode and Episode state; file command `probe` re-arms.

## 2026-10-19: what ends the Raid stage after the first video (found)

### Observed (probe run, Raid stage 1 with F4; owner: "the final enemy was not there this time, the stage still ended with results")
- Probe first calls in order: gameplay from 22:58:46; **23:00:24.9 `421030`**; **23:00:29.2 `427a50`**; **23:00:29.3 `41bd00`, `4225a0`**; 23:00:29.37 Episode system gone (`42b290` sees Episode `0`); 23:00:32 `431660`, `4358b0`, `4373c0`, `435d80`, `4383b0` (`cEpisodeEndFlowState`, results).
- `FUN_004225a0` (area transition after a cut/event): mode 0 calls `piVar1->vfunc+0x28(&DAT_0116B2FC)` (jump to the next area with the jump data); mode 1 with `sCoopManager+0x8F0 == 2` (set by the coop branch of `cCutFlowState` at cut end) sets states 7 and 8 on the flow from `FUN_0078ee80` (stage end / results). Case 4 has the same split.
- `FUN_00427a50`: mode 1 calls `FUN_0041e1f0(1)` instead of the campaign path. `FUN_00421030` case 0: mode 0 without a session returns; otherwise waits on session state (`FUN_00874b90(0x1c,1)`, `FUN_00874bc0`, `FUN_0087e900`). `FUN_0041bd00`: mode 1 adds `FUN_00844760(0x15)` (GUI).

### Derived
- The stage ends because the coop branch of the area-transition handler turns the campaign's "next area" into stage end. Logic gen 8 adds `4225A0`, `427A50`, `421030` to the F4 mask group (campaign path for transitions).
- The missing enemy this run is unexplained (spawn may depend on Raid/coop code not masked, or on timing).

## 2026-10-19: transition masks (test) and the campaign transition window

### Observed (gen 8, owner: "results, and no final enemy")
- `event start 421030` ran with mode 0 (23:04:24) and `area transition 4225A0` ran with mode 0 (23:04:28.9), i.e. the campaign "jump to next area" call was made. Right after, `41bd00` ran, the Episode system was destroyed (`42b290`, 23:04:29.0) and the episode-end chain (`431660`, `4358b0`, `4373c0`, `435d80`, `4383b0`) produced the results screen again.
- The "first enemy" was missing again this run (second time since the transition/flow masks).

### Derived
- The next-area chain after the jump request also splits campaign/coop (episode end flow functions read the mode). Logic gen 10: once mask 14 (`4225A0`) runs in a Raid session with F4 on, the whole game is held in mode 0 until the next Episode system exists and has been stable for 300 frames, then mode 1 is restored ("transition window").
- The hot-reload script now refuses to reload when the build fails (gen 9 was a stale copy of gen 8).

## 2026-10-19: first campaign area change inside a Raid session (works)

### Observed (gen 10, F4 on, Raid single player stage 1; owner: "we are on the beach")
- 23:07:47 event start (`421030`, mode 0 inside); 23:07:51.9 area transition (`4225A0`): transition window opened, game held in mode 0; 23:07:52 old Episode destroyed; **23:08:02 new Episode `22241820`, arrange load stage 1 block 2** (the beach; the same block campaign Continue loads); **23:08:07 window closed, mode 1 restored**. No exception; game still running.

### Derived
- A Raid session can follow the campaign from Episode 1's first area to the second (beach) when the area-transition chain runs as campaign (mode 0) and the session mode is restored afterwards. Route B now covers: campaign content in the area, the first cutscene, and an area change.
- Still open: Raid HUD/enemy presentation, dialogue set, the first enemy missing since the flow masks, the player-2 slot during a mode-0 transition (the window also runs the player setup `FUN_0042b720` with mode 0, which would pick the campaign partner instead of the session player), and the two-player test.

## 2026-10-19: beach area built as campaign, then mode 1 (mismatch)

### Observed (gen 10, owner)
- On the beach: **Parker present** (AI partner), **normal campaign HUD**, **dialogue correct**.
- After mode 1 was restored: the player aimed and fired at the first enemy, but with the magazine empty **could not reload** and stayed **stuck in the aim pose**; **could not pick up items**.

### Derived
- The 300-frame window built the whole beach area as campaign, including the player setup (campaign inventory, AI partner, campaign HUD). Running that with mode 1 breaks the player's weapon/item logic (coop rules on a campaign inventory). In the first area (stage start in mode 1 with masks) fire and reload worked.
- At a Raid stage start the player setup `FUN_0042b720` runs right after the Episode is created (probe log). Logic gen 11 closes the window as soon as the new Episode exists, so player setup runs coop (session slots, coop inventory) while area content stays campaign via the F4 masks. Expected solo result: no Parker (slot 2 empty), working reload/pickups.

## 2026-10-19: beach with coop player setup (gen 11)

### Observed (owner + log)
- The window closed at the new Episode (23:16:44.1); player setup then ran with mode 1. On the beach: **fire, reload and item pickup work**; Parker absent (solo session, slot 2 empty: expected); **the Genesis scanner cannot be used**; **the level looks set up like the Raid and enemies are at Raid positions**; voice lines broken but subtitles correct.
- Masked calls after the window: `45B2F0` (arrange load stage 1 block 2, mode 0 inside), `41B710` (scene events), `4629F0`. The area event loader `42BFC0` did not run for the new block.

### Derived
- Player side is now consistent (coop player, coop inventory). The Genesis scanner is a campaign item missing from the coop loadout; a campaign co-op build will need the campaign inventory with the coop slot logic (open).
- Enemy/object placement comes from mode readers not covered yet. Logic gen 12 adds the 14 remaining mode-reading functions in the `sArrange` area (`0x4516A0`–`0x45AA50`, prologues from `ReRevPrologue.java`) to the F4 group.

## 2026-10-19: beach with 31 masks (gen 12) and a wide probe set (gen 13)

### Observed (gen 12, owner)
- Beach: **objects at campaign positions** (the 14 new masks fixed objects), **enemies still at Raid positions**, Genesis scanner still unusable.
- Quitting to the menu runs the area-transition handler too: the transition window opened at 23:24:04 and would have held mode 0 in the menus.
- No enemy-specific mode reader is identifiable by name (no enemy set file strings; mode readers spread over `0x410000`–`0x8D0000`).

### Derived
- Enemy placement is decided by a mode reader outside the `sArrange` area. Logic gen 13: count-only probes on 300 mode-reading functions (all 311 not already hooked whose prologue `ReRevPrologue.java` accepts, plus the earlier set), reset automatically when the transition window closes, so the log lists the functions that run while the new area is built. Window timeout: 20 s without a new Episode restores mode 1.

## 2026-10-19: results vary between runs: a timing race at the window close (gen 13 -> 14)

### Observed (gen 13 run, owner)
- Beach: **enemies correct (campaign)**, **Genesis works but scans give 1% each**, voice lines still broken, **the first beach monster does not get up**, **objects are Raid ones** this time (the reverse of gen 12), and first-area enemies show Raid HP bars.
- Probe log: the window closed at frame 14597 (first frame with the new Episode), but player setup `42b720` was first-called at **14607**. In between, unmasked mode-1 calls: `412600`, `6d1700`, `7b7b30`, `7bfee0`, `7c1790`, `42ad70`, `8c1e90`, `42ae90`. About 60 mode readers ran during the beach build (`5c8cc0`, `5ca480`, `5d5bc0`, `5f3f30`, `5f4830`, `5fa660`, `5faf90`, `60b1d0`, `6b33a0`, `6c63e0`, `72a7b0`, `855380`, `858110`, `8913b0`, `46ec80`, `56bc20`, `6ade60`, `6b54d0`, `70b0a0`, `7b5010`, `7b6cc0`, `7b72b0`, `846a60`, `846b60`, `846dd0`, `863f20`, `5f48d0`, `67dd60`, `5712f0`, `572940`, `67ee50`, ...).

### Derived
- Which loads fall before or after a per-frame window close varies run to run, hence the swapping results. Gen 14 closes the window **at the entry of player setup `FUN_0042b720`** (hooked as a never-masking entry): everything before runs campaign, player setup and later loads run coop with the F4 masks. Deterministic by construction.
- 300 probes risk altering functions whose first bytes are a branch target inside the function (possible cause of the monster not getting up). Gen 14 is back to 31 probes.

## 2026-10-19: gen 14 runs (deterministic window close)

### Observed (owner + log)
- Run 1 (F4 on): first area: no mission name banner, **no enemies**, voice lines wrong. Window: transition 23:34:43, new Episode 23:34:52.685, **closed at player setup 23:34:52.836**, beach arrange stage 1 block 2 with masks. Beach: voice **correct**, Genesis works but 1% per scan, the enemy is there but **does not move**, Raid objects.
- Run 2: F4 was pressed again during the quit transition (23:37:06), so the masks were **off**: a normal Raid stage (Raid mission name, enemies with HP bars, results without the beach). Not a determinism failure; the F4 toggle is error-prone.

### Derived
- The deterministic close works as designed. The first-area enemy has been missing since gen 8, when the event-start function `421030` was masked; that mask probably blocks its spawn event (guess, to bisect).
- What remains for route B is sorting each mode reader into "campaign" or "coop". Plan: masks switchable by group from `coop.ini` (no F4 toggle), bisection rounds per defect (first enemy, frozen beach enemy, Raid objects, Genesis 1%, voice lines, HP bars).

## 2026-10-19: mask switchboard in coop.ini, bisection setup

### Observed
- Logic gen 15: F4 removed; masks are switched one by one from `coop.ini` (`masks=` with ids, ranges such as `0-10,12-15`, or `all` = every content mask except the cut flow). The file is re-read every 120 frames, so configurations can be changed while the owner plays. Mask ids: 0–10 `sArrange` set, 11 cut flow, 12 area events `42BFC0`, 13 scene events `41B710`, 14 area transition `4225A0`, 15 `427A50`, 16 event start `421030`, 17–30 `sArrange` area (`4516A0`–`45AA50`), 31 player setup hook (never listed).
- Bisection, first-area enemy missing since gen 8: round 1 = all content masks except 16.

## 2026-10-19: bisection round 1 result

### Observed (owner)
- Masks `0-10,12-15,17-30` (all content masks except 16): **the first enemy is present** in the first area, with a Raid-style HP bar.

### Derived
- Mask 16 (event start `FUN_00421030` run with mode 0) suppressed the first enemy. In mode 1 that function waits on session state before starting the event; with mode 0 it takes the campaign branch, which apparently skips the enemy's spawn event in a session. Mask 16 stays off.
- The HP bar is a separate presentation difference (enemy/GUI code reading the mode, not yet located).

## 2026-10-19: area change still works without mask 16

### Observed
- Owner killed the first enemy, saw the video and reached the beach. Log: transition 23:54:01.9, new Episode 23:54:11.29, window closed at player setup 23:54:11.37, beach arrange stage 1 block 2. No exception. (The log label "arr mask off" is a leftover of the F4 flag; fixed in the next build.)

## 2026-10-19: beach state with mask 16 off; object bisection setup

### Observed (owner)
- Beach: **monster moves**, **voice lines correct**, **no HP bars on enemies**; **objects are Raid ones**; **Genesis works but scans give 1%, except the rising monster**. (First area enemies still have Raid HP bars.)

### Derived
- Everything built before player setup is campaign now (enemies, voice, HUD on the beach). Objects are chosen after player setup by an unmasked mode reader. The Genesis 1% probably follows from the Raid objects/enemy set lacking campaign scan values (guess; the campaign monster scans normally).
- Logic gen 16: masks 32–82 = the 51 mode readers first-called during the beach build after player setup (gen 13 probe log, player code `0x6A0000`–`0x6D0000` excluded). `coop.ini` switchboard extended to 128 ids; `masks=all` now skips 11, 16, 31. Round A: all candidates on.

## 2026-10-20: crash with 51 candidate masks; per-site masks (gen 17)

### Observed
- Owner tip: Raid **level 2 is the beach** (log: stage 1 block 2). Starting it directly with round A (masks 32–82 on) crashed after ~16 s: two first-chance access violations at the same moment, at `0x005F9EFC` and `0x00000020`, mode `0` at the exception, stacks through `0x699b3a`/`0x699cb6` (upper-body controllers), `0x6b38ed`, `0x6af19b`, `0x49b3d3` (state machine dispatcher), `0x9c8ed0`.
- Several candidates run thousands of times per second (`6d1700` > 10,000 calls, `412600`, `8913b0` ~6,000).

### Derived
- The global-flip mask (set mode 0, run, set mode 1) races with the engine's other threads: with high-frequency functions masked, the player update on another thread reads mode 0 halfway. A latent flaw of the method, harmless while only load-time functions were masked.
- Gen 17 replaces it with **per-site masks**: each masked function's own compares `cmp dword [reg+0x20], imm8` (`83 7x 20 ib`) get the immediate rewritten (`== 1` -> 7, `== 0` -> 1), one atomic byte per site, applied only while the game is in mode 1 outside the transition window and restored otherwise. 160 sites cover 77 of 83 masks; 6 masks have only register compares and keep the global flip (`45B2F0` load-time; candidates 38, 56, 57, 80, 81 left off).

## 2026-10-20: gen 17 still crashes on the beach: a candidate, not the race

### Observed
- Per-site masks applied at stage start (157 sites patched, mode 1). Beach arrange loaded 00:05:44.9; crash 0.7 s later, same two addresses (`0x005F9EFC`, call to `0x00000020`) and same stacks (`0x699b3a`/`0x699cb6` upper-body transitions that read the think manager `unit+0x1630`, `0x6b38ed`, `0x6af19b`, `0x49b3d3`), now with **mode 1**.

### Derived
- Not the thread race (that flaw was real and is fixed). One of the candidate masks 32–82, forced to the campaign branch, makes a unit run the AI upper-body controllers without a think manager (null call): probably a campaign "partner" decision applied to the empty second slot or to the local player. Bisection rounds on crash / no crash, starting Raid level 2.

## Next steps
1. Crash bisection B1: candidates 32–55 only.

## Crash bisection on the beach start (Raid level 2), gen 18 fix

- Observed: rounds B1 (candidates 32-55) and B2 (32-43) crashed 0.7-0.8 s after "arrange load: stage 1 block 2" with `EXCEPTION C0000005 at 00000000/00000001`. Round B3 (32-37) ended without an exception logged. The owner reported a fatal error dialog from an earlier run: `Failed open file <game dir>\nativePC\game\scene_coop\12\12_arrange.lar`.
- Observed: only `FUN_00453340` (mask 0, neighbour arranges) references the format `game\scene_coop\%x%x\%x%x_arrange` (string xref tool). No `scene_coop\12` arrange exists in the install (loose or archive), so an unpatched mode-1 read in 453340 produces that fatal error.
- Derived (two bugs in the gen 17 test DLL, not in the game):
  1. The mask stubs kept return addresses on one global stack. Masked functions also run on engine worker threads, so two threads could pop each other's return address and jump to garbage, which matches `at 00000001`. The more candidates hooked, the more likely.
  2. Site patches were applied once per rendered frame. When the game switches to mode 1 during a load, a loader can run before the next frame with its compares still unpatched, which gives the `scene_coop` fatal error.
- Gen 18: return stack per thread (TlsAlloc), global flips counted (mode restored only when the last flip ends), site patches checked at every masked function entry before its body runs, site pages made writable once (no VirtualProtect race).
- Test: all candidates 32-82 (38 excluded) on Raid level 2. Result: unknown (pending).
- Gen 18 result (observed): Raid level 2 loaded with all candidates, no access violation. A few seconds later the same `scene_coop\12\12_arrange.lar` fatal error. Log: candidates without patchable sites `41c8d0` (56) and `44ddc0` (57) did a global flip about 100 times per frame.
- Derived: when a flip ended on a worker thread, a frame could read mode 0 with no flip counted and unpatch every site in the middle of an arrange load.
- Gen 19: patch immediately, unpatch only after 60 frames of mode 0 (immediately for the transition window and shutdown); hook entries only patch. Hot candidates without sites (56, 57, 80, 81) taken out of coop.ini. Result: unknown (pending).
- Gen 19 result (observed): same fatal error, right after "arrange load: stage 1 block 2", before the cutscene.
- Observed (archive listing): the current block's arrange ships inside the area event archive: `archive\game\scene\12\12_event.arc` holds `game\scene\12\12_arrange` and `archive\game\scene_coop\12\12_event.arc` holds `game\scene_coop\12\12_arrange`. No loose `scene_coop\12` arrange exists. With mask 12 the campaign event archive is loaded, so any `scene_coop` arrange request for block 12 misses both memory and disk. The only code that builds that name is 453340.
- Gen 20: "== 1" sites patched to 7 select the campaign branch in both modes, so they now stay patched whenever their mask is on (no dependence on the mode, no race). Only "== 0" sites follow the mode. Logging of the first 40 calls to 453340 (block, caller, mode, site bytes, thread). Result: unknown (pending).
- Gen 20 result (observed): crash again. 453340 was called once (current block 12, caller 0045B464 inside 45B2F0), with both sites patched (07 07) and mode 0. So 453340 did not build the failing name.
- Derived: every bisection round so far crashed, including round B3 with only 6 candidates. The level-2 crash may come from a base mask (0-30), not from a candidate. Untested.
- Known (static): the error box is `FUN_009ba000` (MessageBoxA "Fatal error."); "Failed open file. %s %d" is printed by `FUN_008e9b90` / `FUN_008ea060` (file open).
- Gen 21: hook on the fatal box that logs the message and the return addresses on the stack, to find the requester. Result: unknown (pending).
- Gen 21 result (observed): the fatal box fired on the same thread as the arrange load, mode 1, about 1 s after it. Call chain from the stack (Ghidra containing functions): `FUN_0046ae00` (sArrange per-frame update; loops over the arrange request list at `+0x248`, vcall +0x1c) → `FUN_007479a0` (asks the resource manager, vtable +0x30, for the resource named at `[[set+0x20]+8]`, the parent arrange of an arrange set) → `FUN_00d51440` → `FUN_009b2680` → `FUN_009b14b0` → `FUN_008eaa90` → `FUN_008e9b90` (file open fails) → `FUN_009ba000` (box).
- Derived: the name comes from data, not from a format string. A coop arrange set (the `scene_coop\12\12_event.arc` sets `12_00_s200_em_Dark/Deep/Abyss`, `_it`, `_om`, `_BPit_*`) points to its parent `game\scene_coop\12\12_arrange`. That parent is no longer in memory (the campaign event archive was loaded through mask 12 instead), so it is loaded again from disk, where it does not exist. Unknown: which code adds the coop sets on a direct level-2 start.
- Derived: this is tied to starting directly at Raid level 2. The level 1 → beach route (transition window, mode 0 during the load) never hit it. The earlier `at 00000001` crashes match the shared return stack fixed in gen 18.
- Gen 21, level 1 with all candidates (observed): same fatal error with `game\scene_coop\11\11_arrange.lar` about 1 s after "arrange load: stage 1 block 1". Level 1 with the base masks only (0-10, 12-15, 17-30) loaded fine in earlier runs, so a candidate makes the game request coop arrange sets. Stack scan of a resource worker path (`FUN_006ebaa0` → `FUN_00a26a30` → `FUN_00d56d80` → `FUN_00a267a0` → file open) was not conclusive. New bisection on level 1 (rounds C1...).
- Rounds C1 (32-55) and C2 (32-43) on level 1: same fatal error. No bisection round has ever come back "no crash", which points at something common to every round rather than at a candidate.
- Derived: the base masks (0-30) loaded level 1 fine in gens 15/16, when every mask was a global flip. Gen 17 turned them into per-site patches and they were never run alone on level 1 after that. A site patch only covers the `cmp [reg+0x20], imm8` compares in the function body. The global flip also covers every callee (e.g. 45B2F0 → 453340 and the resource/set builders), the `cmp [reg+0x20], reg` reads and helper reads. So the site masks are weaker and let coop branches in callees run. Most likely cause, not yet verified.
- Gen 22: global flip again by default (`sites=1` in coop.ini switches back to per-site patches); the per-thread return stack and flip counter from gen 18 stay. Control run: base masks only, level 1. Result: unknown (pending).
- Gen 22 control result (observed): base masks with the global flip, level 1 loaded with no fatal error and no exception; the owner confirmed it plays. Confirms the derivation above: per-site patches (gens 17-21) were the cause of every bisection crash. Lesson for the playbook: a per-site patch is not equivalent to running a function in the other mode, because callees keep reading the real value.
- Next: candidates added live through coop.ini (without the per-frame hot ones 56, 57, 80, 81) before the beach transition.
- Gen 22 with all candidates except 56/57/80/81 (observed): level 1 loaded; enemies had no HP bar but showed damage numbers (Raid). The beach loaded, Jill was invisible, and the game crashed when the owner looked around (`EXCEPTION C0000005 at 0061EB29`, mode 1, stack through player code `0x6BC74C` → `0x61F7EF` → `0x6149F1` → `0x6193A3`).
- Observed (call counts in the log): 14 candidates run every frame (412600, 6d1700, 7b7b30, 7c1790, 5d5bc0 about 5 per frame, 5fa660, 8913b0, 46ec80, 56bc20, 863f20, 5712f0, 41f420, 4209c0, 8896b0 about 3 per frame), so their global flip happens all the time while other threads run. 11 ran only at load (41ae20, 41c350, 41cfc0, 424cf0, 42ae90, 42afe0, 6d1a70, 70b0a0, 7b24d0, 7bfee0, 846b60); the rest were not called.
- Round D1: base masks + every candidate except the hot ones, level 1 → beach.
- Round D1 (base + load-time candidates, no hot ones), observed: level 1 had Raid HP bars back, the player got stuck in the aim pose and could not reload (same symptom as gen 10's mode-0 player setup); beach: Jill invisible, camera like first person, then the same `EXCEPTION at 0061EB29`.
- Derived: the 0061EB29 crash and the invisible player appear with and without the hot candidates, so they come from candidates that run during the beach load: 42afe0, 41c350, 41ae20, 7b72b0, 424cf0, 7b24d0. The stuck aim at level 1 comes from a candidate that runs during the level 1 load (only active from the start in D1): 42ae90 (runs just before the arrange load, near player setup), 41cfc0, 7b5010, 7b6cc0, 846b60, 846dd0, 70b0a0, 6d1a70, or one of the shared ones. Hot candidates (on in run 1, off in D1) are what removed the Raid HP bars and left damage numbers.
- Round D2: base + hot candidates + 42afe0, 41ae20, 41c350 (half of the beach-load set).
- Round D2 (base + hot + 42afe0/41ae20/41c350, all on from the start), observed: level 1, stuck in the aim pose, no door interaction. Log: the hot candidates 6d1700 (from the frame mode became 1), 412600, 7c1790 and 7b7b30 already ran before the level 1 arrange load, i.e. while the player was being built. In run 1 the same hot candidates were switched on after level 1 had loaded, and aim and doors worked.
- Derived: per-frame mode readers must not run in mode 0 during the area/player build. Gen 23 adds `late=` masks to coop.ini: active only while the area is live (600 frames after its arrange load, off again at the next arrange load or transition).
- Round E1: base masks + `late=` the 14 hot candidates; every load-time candidate off. Expected (not verified): level 1 like run 1 (no Raid HP bars, aim/reload/doors OK), beach with Jill visible and no crash.
- Round E1 with gen 23 (game fully restarted), observed: level 1, aim/reload/doors work and enemies have **no Raid HP bars**. Beach: no crash, objects are the Raid ones, Genesis scans give 1%. Log: late masks went live 600 frames after each arrange load; about 15 minutes with no exception.
- Derived: stable base for route B = base masks (global flip) + hot candidates as `late=`. Still open on the beach: objects and Genesis. Gen 12 (window closed before player setup) once gave campaign objects, so the object choice is made around or after player setup by a load-time reader. Suspects: the candidates that run during the beach load (41c350, 41ae20, 424cf0, 42afe0, 7b72b0, 7b24d0, 6d1a70, 7bfee0, 42ae90, 41cfc0); one of them also makes Jill invisible and crashes at 0061EB29.
- Gen 23b: a transition no longer lets late masks go live before the next area's arrange load. Round F1: + 41ae20, 41c350, 424cf0 (the scene/resource loaders).
- Round F1 (+ 41ae20, 41c350, 424cf0), observed: same as E1 (aim OK, no bars, Raid objects, Genesis 1%, no crash). Those three are harmless and do not choose the objects.
- Gen 24: `post=` masks, active from the second area on (after the first area has been live), so beach-load candidates can be tested without touching the level 1 build. Round F2: post = 7bfee0, 42ae90, 41cfc0, 6d1a70. Remaining half for F3: 42afe0, 7b72b0, 7b24d0.
- Round F2 (post = 7bfee0, 42ae90, 41cfc0, 6d1a70), observed by the owner: no change on the beach (Raid objects).
- Change of method: blind mask bisection no longer converges for objects. Gen 25 logs every resource request whose name contains "scene" (resource manager `[0x011E00FC]`, vtable +0x30, hooked through the vtable slot) with caller, mode, flip depth and live state. One level 1 → beach run shows which arrange/event sets the beach really loads and who requests the coop ones.

## 2026-10-20: resource log (gen 25): the beach loads only campaign files

- Observed (gen 25 run, level 1 → beach, stable base config): 293 scene resource requests logged. The only `coop` name requested in the whole run is `game\episode\scene_start_list_coop` at boot (next to `scene_start_list`). The beach loads `archive\game\scene\12\12_event` (via `0x0042C094`, area events) and from it `12_00_s200_en01`, `en02`, `om01`, `om02`, `it01`, `it01_HELL`, then `archive\game\scene\12\12_00\12_00_event` and `game\scene\12\12_arrange`. Every set is a **campaign** set; no `scene_coop` set, no `BPit`.
- Derived: the "Raid objects" and the Genesis 1% are not different files. They are campaign objects whose own code reads the mode at runtime (spawn or update) and behaves as in Raid because the game is in mode 1. The mode is read in 375 functions (686 places); the masks cover about 80, picked from "what runs during the load" probe lists, which is the wrong lens for runtime behaviour. With global flips that are timing-dependent this is why the rounds felt circular.
- Exit crash: `EXCEPTION at 59BE90C6 mode=-1` when the owner closed the game (mode -1 = game data gone, shutdown). Not a route-B defect.

## 2026-10-20: new direction, campaign base inside a Raid session (gen 26)

- Owner decision: keep the route-B state as a backup (`local/backups/gen25_stable_route_b_2026-10-20/`) and use Raid only as the connection base, with everything else campaign.
- Static map (string xrefs): the game's own sync layer covers what campaign co-op needs: `sArrange::cNetSyncArrangeData` (arrange, guest arrange, wanted enemies, go next block, cut move), `cCutRushState::cNetSyncData`, `uCharaBase::cNetSyncData` (position, damage), `cNetEnemySync`, `sItem`/`uItem` sync, `uDoor2pBase`, elevators, `uLoadRoom`, `sFsmManager::cNetSyncFsmData`, `cPlayerPadSyncData` and nine more player sync types, `cCoopSyncData` (lobby in/return). Managers: `prog\game\network\sCoopManager.cpp` (~`0x87EEE0`), `sNetworkManage.cpp` (~`0x881BA8`), `cCoopSyncData.cpp` (`0x874D00`-`0x876400`). Few mode readers in that region (`87FAA0`, `87FC40`, `87FCD0`, `8AD7F0` sReNetAccessManager, a block of 16 small getters at `86F050`-`86F5F0`). Unknown: whether the senders in object code gate on the mode or on the session.
- Known from the accidental run on 2026-10-19: a Raid stage loaded in mode 0 builds the campaign stage (AI partner, Episode system) and `sCoopManager` exists.
- Gen 26: `base=campaign` in coop.ini. The `setMode` setter (`0x8C6960`) is hooked; when the game asks for mode 1 the request is recorded and 0 is written (plus a per-frame hold). `islands=` lists mask ids that run with mode 1 (31 = player setup, for the session slots). Test I1: Raid solo, no islands. Expected (not verified): the stage plays as the campaign (Parker AI, Genesis, campaign objects, native area change).
- Test I1 result (owner): "literally the campaign inside the Raid". Log: `game setMode(1) -> 0 (campaign base)` at stage start, campaign event archives, native area change to the beach (block 2) without the transition window, no exception.
- Plan for two players (game's own lobby and netcode): `FUN_0042b720` (player setup) takes the two characters from the session (`sCoopManager+0xE4` local, `+0xE8` other) in mode 1, and the Raid second player is a full `uPlayer` with control type 3 fed by `cPlayerPadSyncData`. So player setup runs as an island (mode 1) and the rest stays campaign; the friend picks Parker in the Raid character select. Unknown: whether the sync senders (pad, position, enemies, arrange, FSM) still run with the game in mode 0; whether the online lobby calls `setMode(1)` before the stage (base=campaign would then also apply in the lobby).
- Gen 27: dump of session (`sCoopManager +0xE4/+0xE8/+0xF4/+0x8F4`), episode data and units with control types at each `setMode` and each time an area goes live.
- Test J1 (alone): Raid online private room, start level 1 alone if the game allows it, `base=campaign`, `islands=31`. Friend package prepared in `local/friend_package/` (DLLs, coop.ini, steam_appid.txt, Italian readme); not sent (owner's decision).
- Test J1 (observed): Raid online cannot be started alone. Log: entering the online menu already calls `setMode(1)` (session `+0xE4 = +0xE8 = 0x80011010`, `+0xF4 = FFFFFFFF`), so gen 27 forced mode 0 in the online lobby too.
- Gen 28: the campaign base engages at the Episode factory (`FUN_00794e10`, hook-only mask id 83) when the game is in a Raid session, with a fallback when an Episode pointer appears; the lobby keeps mode 1. Disengages when the game sets a mode other than 1. Friend package updated.

## 2026-10-20: first two-player test (gen 28), owner + friend over the game's own lobby

- Observed (owner): both entered the private Raid lobby and the level. The host crashed after a few steps; the friend crashed when the host loaded the level, and the friend's character was invisible.
- Host log: lobby `setMode(1)` at f=3201 (session `+0xE4 = +0xE8 = 0x80011010`), again at f=4483 (`+0xE8 = 0x80000000`), an accidental F9 at f=5743 (debug hotkey, called setMode(1); harmless here). Session at stage start: `+0xF4 = 2`; episode data `+0x24 = 0x80011010` (Jill), `+0x28 = 0x80011020`. The Episode factory hook did not fire; the fallback engaged the campaign base when the Episode pointer appeared (f=8521). Campaign event archives loaded; player setup ran three times as an island (mode 1). Crash 48 s later: `EXCEPTION C0000005 at 004A6E05` (mode 0) in `FUN_004a6a80`, called from `uPlayer::update` (`0x6C3BB2`), stack through player code `0x6B90B5`, `0x6B9426`, `0x6E4139`. Unknown: which unit (no unit dump: the "area live" check needed mode 1).
- Gen 29: area live and periodic unit dumps (every 600 frames) also under the campaign base; debug hotkeys except F11 ignored with base=campaign.
- Friend log (gen 28, guest side, saved privately as `local/friend_logs_K1_rerev_coop.log`): lobby `setMode(1)` ×3; at the third, session `+0xE4 = +0xE8 = 0x80011010`, `+0xF4 = 2`, `+0x8F4 = 3`. Campaign base engaged by the fallback; campaign archives; player setup ran three times as an island. 20 s later: **engine assert** `chara\control\switch_parts\..\cScopedResource.h line 108: ptr_ != nullptr` (from `0x4B5461`), in the upper-body controller path (`0x49B3EE` dispatcher → `0x699BD6` → `0x6ADFF0` → `0x8885E0` → `0x883FA8` → `0x69BC67` → `0x49CD63` → `0x6A0581`). Owner: the friend's character was invisible.
- Player setup in mode 1 (`FUN_0042b720`): own slot index from `FUN_00882d60()`; own slot takes `sCoopManager+0xE4`, the other `+0xE8` (or "empty" `0x80000000` when `+0xF4 == 0`); equipment comes from `[0x0116A7DC]` (coop) instead of `[0x0116A7D8]` (campaign). On the host the other slot was `0x80000000` at stage start.
- Derived (likely, not verified): the session asked for Raid character ids whose model and switch parts were not loaded by the campaign-mode stage load (invisible character, missing switch-parts resource). Gen 30: `role=host|guest` in coop.ini; during the player-setup island the session ids are replaced by the stage's own pair from the episode data (`+0x24` slot 0 for the host, `+0x28` slot 1 for the guest), `+0xF4` forced non-zero, and restored after; unit dump after each player setup.

## 2026-10-20: second two-player test (gen 30): both players in the campaign level

- Observed (owner + friend, screenshots from both sides): **each sees the other; the friend controls Parker** (remote player, name tag and HP bar over the partner). Host log: player setup swapped the session pair `80011010/80011020` for the episode pair `80011010` (Jill, local) / `80011420` (ParkerLight, other).
- Defects: (1) on the guest **doors are missing** (he walks into the building), while on the host Parker runs in place against the closed door; (2) the guest's character froze during the ship-rocking wave event and was released when the host opened the door.
- Derived (not verified): in a Raid session the guest does not build the arrange itself; it gets objects from the host (`sArrange::cNetSyncArrangeData::cGuestArrangeSyncData`, `cArrangeSyncData`). With the host in mode 0 the arrange senders (sArrange mode readers) probably do not send, so the guest has no doors. Need the guest log of this run.
- Side issue found: the `role=` key also switched on the DLL's own stage-1a UDP netcode (it listened on 27777). Gen 31 renames the slot key to `slot=host|guest`; the own netcode now needs `netrole=`. Fewer dumps.
- Guest log of the gen 30 run (private copy `local/friend_logs_K2_rerev_coop.log`): no crash in 12 minutes; the guest loaded the same campaign sets as the host (`11_event`, `en01/en02/om01/om02/it00`, `11_arrange`). The owner reports both players then got through the doors and went on (first door: opened on the host side later; second door: stuck for a while for both).
- Static: the network primitives are session-based, not mode-based: `FUN_00881cf0` = online (`[FUN_009c7340()+0xAA88] & 1`), `FUN_00881d10` = isHost (`+0xAA80 == +0xAA84`, true when offline), `FUN_00881dc0` = own peer id, `FUN_00881f20` = send. `sObjModel` net handlers assert on isHost, not on the mode. `sArrange` spawn `FUN_004688f0`: when online and not host (and not called from a host message), the guest **sends a request to the host and returns without spawning** (`FUN_00881f20(+0x260, ...)`); the host spawns and syncs. The mode reads in that function pick difficulty fields (`sCoopManager+0x104` vs `[0x0116A7E4]+0x30`) and two equal branches.
- Gen 32: hook on the game's net send (`0x881F20`): per-caller counts, each new caller logged, table every 600 frames, on both PCs. Goal: see whether the host answers the guest's arrange requests in the campaign base.

## 2026-10-20: third two-player run (gen 32)

- Observed (owner): same as before. The guest loses control at the ship-rocking wave and gets it back when the host opens the door; the guest still sees no doors. Both reached the beach.
- Host log: sends on `0x881F20` only from `FUN_0044c2b0` (online block-change notice, 17) and `FUN_00411680` (8). Host answers to guest arrange requests use another function, the broadcast `FUN_00882fd0` (136 callers; `FUN_0046a670` / `FUN_0046a7e0` keep/limit-arrange handlers: host spawns with `FUN_004688f0(...,1,1)` and broadcasts the result, guest sends the request with `0x881F20`). Gen 33 logs the broadcast too.
- Host crash on the beach 3 s after its arrange load: engine assert `ScopedMemoryPtrDetail.h line 62: index<size_` at `0x747C9A` in `FUN_00747b30` (arrange request update, online path): it copies up to `set+0xC` entries into its pending buffer (`+0x28` data, `+0x2C` size), and the buffer was smaller than the beach set. Gen 34 hooks `FUN_00747b30` and, when online and the buffer is too small, resizes it first with the game's own `FUN_007468a0(this+0x24, count)`. Unknown why the buffer size differs from Raid.

## 2026-10-21: fourth two-player run (gen 34): session lost after about 13 s

- Observed (owner): each saw the other standing still the whole time; the guest now saw the doors and a door prompt for a second, but they did not open; the host reached the beach with Parker driven by the AI; the guest stayed stuck in level 1.
- Logs (guest copy `local/friend_logs_K5_rerev_coop.log`): both sides stopped sending anything on the game's network about 13 s after the level started (guest send table frozen from f=8100, host from about 00:02:26). The arrange-request fix never triggered on either side. Derived: the session dropped; the host's partner then fell back to the AI (campaign behaviour without a remote player). The previous runs with gen 30-32 stayed connected for minutes; gen 33/34 added the broadcast hook on `0x882FD0` (called from the network threads) and the `0x747B30` entry hook. Unknown which of the two broke the session, or whether it was a network drop.
- Host broadcast log (before the drop): `FUN_0045ae??` area broadcast about once per second (`0x45AE86`), player broadcast `0x6965A9`; lobby broadcast `0x7C9004` once per frame in the lobby.
- Gen 35: broadcast log off by default (`bcastlog=1` to enable), arrange-request fix on (`arqfix=0` to disable); both read from coop.ini before the hooks are installed.
- Gen 35 run (owner): connection held (host still sending 50 s in), but the guest again saw no door. Derived: in gen 34 the guest saw doors only because the session had dropped: offline, `isHost()` is true and the guest places the arrange itself.
- Gen 36: `isHost` (`FUN_00881d10`, starts with `call FUN_009c7340`) is replaced by an equivalent function; on the guest (`slot=guest`, `guestarrange=1`, default) it returns true while the thread is inside the sArrange spawn `FUN_004688f0` (mask hook id 10, per-thread depth), so the guest places the area objects itself as in the campaign while staying connected. Unknown: whether door opening still syncs when each side spawned its own copy.
- Owner requirement (2026-10-21): door opening animations and every other interaction (doors, pickups, events) must stay in sync between host and guest; checked after each two-player test.
- Gen 36 run (owner): no change, the guest still sees no doors. Guest log: the override fired only 5 times, all in one frame right after the arrange load, so the doors are not placed through that path. Guest kept sending `0x45366A` (31 in a minute).
- Gen 37: on the guest the isHost override also covers every caller in the sArrange code range `0x440000`-`0x46C000`, and a table of the callers that get isHost = false on the guest is logged every 600 frames (to find the code that really gates the doors). Risk: guest-side keep/limit handlers may now act as host.
- Gen 37 run: guest isHost=false callers (5 min): sArrange `0x468AC3`/`0x44C030` (37 each), `0x4557AB`, `0x45AC55`; enemy manager `FUN_0056bc20` (`0x56BD47`/`0x56BDF0`, every frame: only the host broadcasts the enemy list, `cNetEnemySync` vtable `0xFF928C`); player code `0x6BF775`, `0x697F7E`, `0x6CD58E`; `0x61F80B`, `0x5CDB5C`. Doors still missing with isHost forced over the sArrange range.
- Derived: `sArrange::move` (`FUN_0046ae00`) calls `FUN_00450230` only when **offline** (`FUN_00881cf0() == 0`); the gen 34 guest saw doors when the session had dropped. Gen 38: isOnline (`FUN_00881cf0`, starts with `call FUN_009c7340`) replaced; on the guest, callers in `0x440000`-`0x46C000` see offline.
- Gen 38 run, host: the `0x747C9A` `index<size_` assert came back on the beach although the gen 34 entry check never triggered (the buffer had room at entry). Derived: the buffer is resized smaller while `FUN_00747b30` is still filling it (re-entry through `FUN_004688f0`). Gen 39 hooks the resize `FUN_007468a0` itself (prologue `push esi; push edi; push 0x011855E0`) and raises any count below 256 to 256; extra slots stay -1 (empty). Host also sent an `sObjModel` message (`0x67474E`) for the first time in this run.
- Gen 38 run, observed (owner): the guest **now sees the doors**; a padlock shot by the guest broke on both PCs; both could open doors, but the guest stayed one door (one room) behind on the host's screen (host sees him running into a closed door); the host did not see the guest firing. Guest log (`local/friend_logs_K9_rerev_coop.log`): isOnline override active (callers `0x45932A`, `0x46AEA3`, `0x46B05E`, about 360 per second); guest sent `sObjModel` `FUN_00674510` (`0x6745FF`, 240), host `FUN_006746a0` (`0x67474E`, 121): object ownership messages keyed by arrange object id (`FUN_0044f410(id)`).
- Static: sArrange net handler `FUN_004660e0` (vtable `0xFF41B4`, slot 8): type `[0x0116BE54]` → `FUN_004557a0` (guest forwards `FUN_004535c0` to host); type `[0x0116BC14]` (the request, DTI `0x0116BBF8`) → `FUN_0087bfc0` (queued). Ghidra had merged this handler into the previous function; `local/tools/scripts/ReRevForceDecomp.java` splits and decompiles such entries.
- Open: door state guest→host (doors opened on the guest's local copy are not opened on the host's), remote fire animation (host does not see the guest shoot).
- Door code map (static): door/obj-model code `0x628000`-`0x639000`. Door unit init `FUN_00628a60`: when online and the arrange id `+0xE2C` != -1 it creates a per-door net channel (`FUN_00628900(unit, id + 0x23B34)`, stored at `+0xFD8`). Door broadcasts `FUN_006285d0` / `FUN_00628640` go through that channel (`FUN_00882fd0(+0xFD8, ...)`) and depend on the player kind `+0xE30` (3 = remote) and `+0xFC8`. `FUN_00630e20` (door event with both players: `FUN_0067af10` local player, `FUN_0067d850` partner) branches on online, `FUN_0087e900` (session) and isHost. `sObjModel` lock protocol: host `FUN_006734a0` (TryLock answer broadcast), guest `FUN_006735d0` (check state), receiver `FUN_00674970` (cases 0 host / 1-2 guest / 4).
- Gen 40: trace hooks (first 80 calls per site, then every 200th) on `6285D0`, `628640`, `630E20`, `674510`, `6746A0`, `674970`, `6734A0`, `6735D0`, logging ecx, caller, door id, kind and channel; `doortrace=0` turns them off. Includes the gen 39 buffer fix and the gen 38 guest offline arrange.
- Gen 40 run: host crashed 1.5 min in with `EXCEPTION at 0x009C7340` (ecx = 4) from the obj-model receive path (`0x674C8A`). Cause: `FUN_009c7340` is **thiscall** (`mov eax,[ecx+0x7F0]`, network manager in ecx; callers load it from `[0x011E0510]` before calling isHost/isOnline). The trace called isHost with a garbage ecx, and the gen 36-39 replacements relied on ecx surviving by luck. Gen 41: the replacements pass the caller's ecx through (`net_sys(mgr)`), the trace no longer calls isHost, and the arrange fix reads the online flag through `net_sys`. No door trace line fired on the host before the crash.

## 2026-10-21: host vs guest comparison, the coop-session check (gen 42)

- Gen 38 session, side by side (guest clock about 6 s behind the host's): the guest asked the host about one arrange object ~5 times per second (`FUN_00674510`, 240 sends); the host answered (`FUN_006746a0`, 121) and stopped answering after 25 s while the guest kept asking. `FUN_006746a0` refuses (answer 0) when the object already has a unit on the host: the guest never gets ownership. Both PCs then crashed on the beach with the same `0x747C9A` `index<size_` assert (fixed since gen 39).
- `FUN_0067af10` = local player (`FUN_00882670(own slot)`), `FUN_0067d850` = partner (first unit in the sPlayer list `+0x30..+0x34` whose slot `+0xE2C` differs from the own slot). The DLL's old unit walker used `+0x34/+0x38` and printed garbage in the campaign base.
- **Key finding:** `FUN_0087e900` (fastcall, ecx = sCoopManager) is the game's "in a coop session" check: session state `+0xF4` in {1, 2} **and mode == 1**. 71 callers: event/cut flow `0x41xxxx`-`0x43xxxx` (including the event start `0x421xxx` and the area transition `0x4225xx`), sArrange `0x451B8A`, `0x45275D`, `0x457008`, door code `0x630EED`-`0x637B7F` (door event with both players `FUN_00630e20`: host/guest set the partner's state only when this is true), obj model `0x6777F8`-`0x6794A7`, GUI `0x7Cxxxx`-`0x7Fxxxx`, net `0x879D6A`, `0x87A26D`. In the campaign base it always answered "no session", so every two-player branch (who drives a door, event waits, partner states) ran its single-player version on both PCs. Derived (to verify): explains the doors opened on one copy only, the guest held by the wave event until the host's own event flow moved on, and the guest one room behind.
- Gen 42: `FUN_0087e900` replaced; with `coopsession=1` (default) it is true when the session state is 1 or 2, whatever the mode; per-caller true/false table every 600 frames. Black box: one `bb` line per second with both players (slot, class, control type, kind, position, flags `+0xE1C`, upper state), the current block and the per-second counts of the door/obj-model trace sites, on both PCs.

## 2026-10-21: first black-box comparison (gen 42, host + guest logs)

- Both black boxes (one line per second, guest clock about 5-6 s behind) agree on the model: on each PC the own character is control type 1 and the other is control type 3 (remote). Gen 42 ran 5 minutes without a crash, level 1 and beach; at the beach the pair became the campaign `JillEp1_2` / `ParkerEp1_2`.
- Observed: each remote character follows the real one with gaps, then jumps. Examples: host's remote Parker stayed at (510, 30, 16870) for over 8 s while the guest's Parker was already at (446, 0, 16102); on the beach host's remote Parker stood at (2259, 307, -1591) for about 15 s while the guest's own Parker had already moved on, then jumped by about 1400 game units to catch up. The guest's remote Jill showed the same stalls at the **same coordinates** (`2259, 307, -1591`; `-473, -600, 12178`), i.e. both remote characters get stuck at the same doorways, each on the viewer's closed door copy.
- Wave event: the guest's Parker did not move for about 15 s at the start (from -177,-200,18184) while the host's Jill walked on.
- coopSession callers that ran (true on both): `0x45700D` (sArrange, ~60k), `0x4230A9`, `0x411B18`, `0x4128BA` (flow), `0x841564`, `0x452762`, `0x573476`, `0x422F19`, guest also `0x41F9E4`. None of the door code callers (`0x630xxx`-`0x637xxx`) ran on either PC: the two-player door path is never reached.
- Object 78: guest `FUN_00674510` (called from the obj-model receive at `0x674C2A`) and host `FUN_006746a0` (from `0x674C8A`) ping-pong about 5 times per second for minutes; the host refuses because the object already has a unit on the host.
- Position sync (static): `uPlayer::update` (`FUN_006cdd70`) ticks the component at `unit+0xEBC` (`FUN_0049a1c0`: timer `+0x58` += 1 / (fps x interval `+0x54` x speed)); on wrap the local player sends its true position (`FUN_006cd540`, message `cPositionSyncData`, vtable `0xFF71B8`, DTI `0x0116C64C`); the receiver (`FUN_006cfd90` → `FUN_0049a200`) warps the remote unit (`FUN_006cd6b0`). Derived: between sends the remote is simulated from pad input only and collides with the viewer's closed doors; that is the "one room behind".
- Gen 43: `posync=<ms>` (default 150) sets the local player's interval every frame; the black box shows each unit's interval (`ps`).
- Owner report on the gen 42 run: the wave event **no longer froze the guest** (coop-session check). Doors visible on the guest, same desync. At the first enemy the AI took over Parker for 1-2 s (guest black box: local Parker control type 2 / kind 2 for about 5 s at 21:37:08-13). Raid-style damage numbers on the first enemy only. The host sees Parker fire but not aim, and hears no shot. Beach: an enemy died first on the guest and stayed invincible on the host.
- Derived: with `guestarrange=1` the guest spawns its own enemies, so each PC has its own copy and the arrange-object ownership never settles (object 78 ping-pong); damage and death apply only to the local copy. The override was a workaround for doors missing on the guest, whose likely real cause (the mode-gated coop-session check) is fixed since gen 42.
- Gen 43b: `guestarrange` default 0 (the host owns arrange objects again, as in Raid); control guard every frame (own slot control type 1 / kind 1, other slot 3 / 3; `ctlguard=0` disables); coop-session check kept false for the Raid HUD caller `0x841564` (damage numbers); the old player-2 takeover hooks (updatePad, upper controllers, buttons, input context) are not installed with `base=campaign` (they may interfere with the remote player's aim and sound); position sync interval 150 ms. If doors vanish on the guest again, `guestarrange=1` in the guest's coop.ini brings the gen 38 behaviour back.
- Owner decision (2026-10-21): the guest keeps placing the area objects (`guestarrange=1` in the guest's coop.ini); next step is our own door/event flag protocol (flag + acknowledgement next to the position check), and the host must see the door-entry animation for the remote player.
- sArrange (`[0x011695C4]`) arrange-object arrays (from `FUN_0044f410`, lookup by id): six (count, data) pairs at `+0x70/+0x7C`, `+0x84/+0x90`, `+0x98/+0xA4`, `+0xAC/+0xB8`, `+0xC0/+0xCC`, `+0xD4/+0xE0`; object `+0x0C` id, `+0x04` unit, `+0xD0` owner.
- Gen 44 (= gen 43b + recorders): inventory of arrange objects per area (id, list, unit DTI, owner, position) and every change of unit fields `+0xF80..+0xF8C`, `+0xFC4..+0xFD0`, `+0xE1C..+0xE24` every 30 frames (max 80 lines per object); muted diff of `unit+0xE00..+0x1200` of both players every 6 frames (fields that change more than 40 times are muted) to catch the door-entry action. `arrrec=0` disables.

## 2026-10-21: gen 44 recorder results (doors, door-entry action, position sync)

- Arrange object classes resolved from their DTI registration (`push name; mov ecx, DTI`): doors in level 1 are `uShipDoor08` (DTI `0x01175D0C`) and `uShipDoor09` (`0x01175D38`), lockers `uLockerDoor`, plus `uOMWaif*`, `uOMActDuct`, `uOMRat`, `uItBullet*`, `uItGreenPlant`, `uWpGndHand`, `uZombieBase`. **Door ids and positions are identical on both PCs**: 61 (-518,0,15686), 62 (-1631,-200,14080), 63 (-1558,-199,13037), 64 (-425,-599,12268), 66 (425,-600,13990), all in arrange list 4.
- Door fields (observed on both PCs): idle `+0xE1C` 0x40002080 (0x40003080 while it is the "next" door), `+0xE24` 0xB, `+0xFC8` 0. **Opening:** `+0xE1C` gains 0x20000, `+0xE24` 0xB→3, `+0xFC8` 0→3 (stays 3 afterwards); about 2 s later `+0xE1C` and `+0xE24` go back (0xB) as the door closes behind. Each PC opened its own copy when its own player went through (door 61: guest ~10 s before the host; door 63: host three times).
- Player at a door (both PCs, own player): `+0xE1C` 0x40000088→0x4000208A, `+0xF14` 0→3, `+0xF20` 0→1, `+0xF3C` 0x400→0x402402, `+0xF44` 0→0x10 (one frame), `+0xF80/+0xF84` (upper state objects) swap, `+0xFA8` 2→0; `+0x1024` 1→3 just before.
- Position sync: the game's interval is 500 ms (not seconds). Sender `FUN_006cd540` needs flag `+0xE1C` bit 0x80 and class `0x8001xxxx` (players) → `FUN_006cafa0`. Receiver `FUN_006cd6b0`: for players (`+0xECC` set) it calls `FUN_004a1720`, i.e. the remote **walks** to the received position and collides with the viewer's closed doors (host's remote Parker stood at door 63 while the real one was 15 m on). The 150 ms interval of gen 43 does not change that.
- Gen 45: receive hook on `FUN_0049a200` stores the last target per remote slot; a remote unit farther than `warp=` units (default 300) from a fresh target (≤ 30 frames old) is put there. `coop_cmd.txt` "door <id>" writes `+0xE24 = 3` on that arrange object's unit (solo experiment for opening a door copy without a player).
- Gen 45 solo test (Raid single player): first-person view and crash on the first shot (switch_parts assert): the player-setup island ran in a solo session and the swap forced the session state to 2, giving a Raid loadout whose parts were not loaded. Gen 45b: islands only when `sCoopManager+0xF4` != 0 (a second player is in the session); the swap no longer forces the state.

## 2026-10-21: gen 45b coop run, ID mismatch on the beach, own sync channel (gen 46)

- Gen 45b run (both logs): the guest crashed on the beach on the engine assert `sArrange.cpp line 14397: pCharaList` (`0x454D77`, inside `FUN_00454c80`): a received sync message named an arrange object id that `FUN_0044f410` could not find. **Beach object ids are shifted by one** between the PCs (same object at 2077,50,-804 is 51 on the host, 50 on the guest; the host has an extra object first). Derived: with the guest placing objects itself, ids follow creation order and diverge; the game's own messages (damage, death, ownership) then hit the wrong object (explains the enemy dead on the guest and invincible on the host) or none (the assert). In level 1 the guest this time created none of the units (doors, padlock missing) although the objects and ids matched the host's: a race between the local spawn and the host's ownership messages.
- The remote warp of gen 45 is undone every frame (10,000+ warps from the same point): the unit position is also kept elsewhere. The control guard caught 3 AI takeovers.
- Owner decision: "mode B": every PC keeps local copies, and ids are aligned per area (host sends its object table, the guest matches by class + position and renumbers), so the game's own sync hits the right object.
- Gen 46: own sync channel on Steam P2P channel 8 (reliable). The peer is found automatically as the Steam friend playing appid 222480 (`ISteamFriends::GetFriendGamePlayed`), no SteamIDs in coop.ini. PING/PONG every 2 s with counters; when an area becomes live the host sends its object table (id, unit DTI, position) and the guest logs how many match with the same id, a different id (first 25 pairs) or are missing. Measurement only, no renumbering yet. `syncnet=0` disables.
- Gen 46 runs (two sessions, both PCs): the own channel works. Peer found automatically (friend #10 of 29 playing appid 222480) a few seconds after boot, before the lobby; about 580 reliable messages each way, none lost (tx = rx on both sides), round trip 8-10 frames (~150 ms). The host's table at "area live" held only the two players (doors/enemies spawn later); at the beach it held 36 objects: on the guest **all 36 present, 34 with a different id** (almost always host id - 1), 2 equal. No crash in these sessions. The first matching was not unique (three host objects matched guest 64).
- Gen 46b: the host resends its table every 5 s while the area is live, when the spawned set changed. Gen 47: unique greedy matching (same DTI, nearest within 200 units, each guest object once) and, with `idalign=1` (default), the guest renumbers its matched objects to the host ids; unmatched guest objects that hold a target id are moved to id + 2000 first.
