# God of War (2018)

| | |
|---|---|
| Engine | Santa Monica Studio proprietary engine (internal name **Kinetica**, branch `//int8-code/main81`), 64-bit, DirectX 11, Lua 5.2 gameplay scripting |
| Versions tested | Steam AppID `1593500`, build `11168363`, `GoW.exe` `1.0.475.7534` (`20,083,552` bytes, PE timestamp `0x628e8031` / `2022-05-25`, SHA-256 `caebcb027980d7eac9203d190f9ee649eebc549f8defce138e2114dc91f40452`) on Windows 11 x64 |
| Projects | Built working asymmetric Kratos + Atreus network co-op prototype (`version.dll` + `mods\lua\gameart\scripts\libraries\creature\locomotion.lua`). Reference modding bases: [Nukem9/godofwar-gameplay-tweaks](https://github.com/Nukem9/godofwar-gameplay-tweaks), [adore11s/God-of-War-VR-Mod](https://github.com/adore11s/God-of-War-VR-Mod) (MIT), [kainotoa/GOWTool](https://github.com/kainotoa/GOWTool) |
| QA level | `automated` (standalone sync + mutation twin suite `5/5`, live `GoW.exe` Host + `tools/chaos` `bad_wifi` UDP relay test, and simultaneous 2-instance `GoW.exe` Host + Client sync test on 1 PC) |
| Last updated | 2026-10-28 |

## Where things stand

No public co-op mod existed for *God of War (2018)* on PC prior to this work. Using static and runtime reverse engineering on Steam build `11168363` (`GoW.exe` `1.0.475.7534`), we built and verified an **asymmetric 2-player Kratos + Atreus network co-op mod**:
- **Player 1 (Host)** controls **Kratos** (`heroa00`) and runs the authoritative world, combat, and story state.
- **Player 2 (Client)** controls **Atreus** (`son00`) over non-blocking UDP (`socket.core` + `cmsgpack` built into `GoW.exe`), driving Atreus's locomotion (`SetActuator` + `Warp`), sprint/strafe, arrow shots (`Regular` / `Charged`), arrow-type switching (`SwitchArrow`), and runic summons (`Tap`).

## Existing work

Searched on 2026-10-28: this repository's directory, issues and PRs, GitHub repository search (including forks), and web search covering Nexus Mods, ModDB, Reddit, and press articles. Raw notes: [NOTES.md](NOTES.md).

- **Prior co-op / multiplayer mods:** None found for *God of War (2018)* or *God of War Ragnarök*. Speclizer showed a single-player playable-Atreus video in 2022, not a co-op mod.
- **[Nukem9/godofwar-gameplay-tweaks](https://github.com/Nukem9/godofwar-gameplay-tweaks)** (last commit `2024-02-01`): C++ `version.dll` proxy that hooks `IFFProcessLuaScriptChunk` (`0x140466de0`) and `LoadLuaScriptChunk` (`0x140466260`) to dump `.wad` Lua 5.2 bytecode and compile/inject `.lua` source overrides from `<game dir>\mods\lua\` at runtime.
- **[adore11s/God-of-War-VR-Mod](https://github.com/adore11s/God-of-War-VR-Mod)** (MIT License, last push `2026-10-26`): Full-source C++ `version.dll` proxy built against this exact Steam executable (`PE timestamp 0x628e8031`).
- **[kainotoa/GOWTool](https://github.com/kainotoa/GOWTool)** and **[JeanxPereira/GoWToolkit](https://github.com/JeanxPereira/GoWToolkit)**: Asset browsers and extractors for `.wad`, `.toc`, `.lodpack`, and `.texpack` files.

## How the game works (for co-op)

Observed statically and verified at runtime on Steam build `11168363` (`GoW.exe` `1.0.475.7534`, image base `0x140000000`; full disassembly and hook details in [NOTES.md](NOTES.md)):

- **Unencrypted binary & SteamStub v3.1 header:**
  - `.text` (`13.3 MB`), `.rdata` (`3.1 MB`), and `.data` are unencrypted on disk (`SteamStub` flags at `0x14519f230` = `0x06`, no section encryption).
  - The PE entry point is `0x14519f310` (`.bind`) and the real MSVC `mainCRTStartup` (`OriginalEntryPoint`) is at `0x1403ec680` (`RVA 0x3ec680`).
  - For running a second simultaneous `GoW.exe` client instance on one PC, patching `0x14519f310` with a 5-byte relative jump (`E9 6B D3 24 FB`) to `0x1403ec680` and hooking IAT `steam_api64.dll!SteamAPI_RestartAppIfNecessary` (`0x140d49328`) to return `false` allows multiple instances to run side-by-side with `steam_appid.txt` (`1593500`).
- **Lua 5.2 scripting in `.wad` archives & VM isolation:**
  - Scanning all 597 `.wad` files in `<game dir>\exec\wad\pc_le\` found **6,627 uncompressed `IFFLuaScriptChunkHeader` chunks** (`IFFHeader` type `0x24`, `0x60` bytes + `0x204`-byte path/context header), representing **1,423 unique Lua scripts** (`%PROJ_PATH%/gameart/scripts/...`). Note: `IFFHeader.m_Length` (at offset `+4` of the `0x60`-byte `IFFHeader`) is measured from the start of `IFFLuaScriptChunkHeader`, so the Lua bytecode slice is `[chunk_start + 0x204 : chunk_start + m_Length]`.
  - Each character (`heroa00` for Kratos, `son00` for Atreus) runs in its **own separate `lua_State*` VM**. Inside `son00`'s VM, `game.AI.FindSon()` returns Atreus (`son`), `game.Player.FindPlayer()` returns Kratos (`player`, including `player:GetWorldPosition()`, `player:GetWorldForward()`, and `player.Pad`), and `_G.OnCommandSonStart(son, cmd)` + `locomotion.SetActuator` execute natively in the same VM.
  - Game Lua VMs do **not** load the standard `io` or `os` libraries (`io == nil`), and any unhandled Lua runtime error invokes `GoW.exe`'s Lua error handler (`0x14063d4d0`), which formats the traceback at `0x14063d610` and executes `__debugbreak()` (`int 3`, `0xCC`) at `0x14063d629` (`STATUS_BREAKPOINT` / `0x80000003`). Injecting configuration directly into the Lua source buffer in `hk_IFFProcessLuaScriptChunk` and patching `0x14063d629` (`0xCC -> 0x90`) prevents Lua errors from crashing the process.
- **Built-in networking and serialization (`socket.core` + `cmsgpack`):**
  - Every game Lua VM preloads `socket.core` (`0x140a05bd0`, LuaSocket TCP/UDP) and opens `cmsgpack` (`0x1409fa370`, 32-bit float MessagePack).

## What we built

1. **`version.dll` proxy + runtime Lua 5.2 loader + multi-instance & save isolation:**
   - Forwards all 17 `C:\Windows\System32\version.dll` exports.
   - Hooks `IFFProcessLuaScriptChunk` (`0x140466de0`) and `LuaScriptChunkMemoryAllocatorHookLoc` (`0x1404662f2` -> 14-byte stub at `0x140466dee`) to compile `.lua` source files from `<game dir>\mods\lua\...` using `GoW.exe`'s own `luaL_newstate` (`0x1409e4c60`), `lua_load` (`0x1409e3720`), and `str_dump` (`0x140a03650`), prepending `[Coop]` settings from `coop_config.ini` into `_G.__COOP_CFG`.
   - Hooks `LuaScriptAllocatorHookLoc` (`0x1405a6a1f` -> 14-byte stub in the `0xCC` cave at `0x1405a0fae`), `LuaGoldPrint` (`0x1405aacb0`), `LuaPrint` (`0x1405aacc0`), and the Lua runtime error handler (`0x14063d610` -> `0x14063d4ad`, plus `nop` at `0x14063d629`) to log all Lua output and tracebacks to `coop_lua.log`.
   - Hooks IAT `SHELL32.dll!SHGetKnownFolderPath` (`0x140d485c0`) to isolate saves into `<instance dir>\saves\God of War\` and IAT `USER32.dll!GetForegroundWindow` (`0x140d487a8`) to return `Renderer::WindowHandle` (`0x14504c390`) so background instances keep rendering and processing input.
   - Hooks `GameModule::ShowHideSLI` (`0x140850160`) and `StartTimerSLI` (`SLIFunctionTable[360]` at `0x1411cc4c8`) to skip boot logos (`BootSiePresentsLogo`, `Boot_SMS_Logo`), cutting boot-to-world time to ~12–14 seconds.
2. **`mods\lua\gameart\scripts\libraries\creature\locomotion.lua` co-op sync module:**
   - Runs inside `son00`'s VM (`crt == game.AI.FindSon()`).
   - Opens a non-blocking UDP socket (`socket.core.udp()`) and exchanges binary `cmsgpack` state packets (`t="H"` from Host, `t="C"` from Client).
   - Suppresses autonomous Atreus AI `SetActuator` calls (`_G.__coop_override_until`) while applying Player 2's target movement (`son:SetActuator` + interpolated `son:Warp`) on every frame, clamped to a 35m leash around Kratos and paused whenever `son:IsDoingSyncMove()` is active (cutscenes, paired animations, title-screen pose).
   - Delivers discrete Atreus combat commands (`Regular`, `Charged`, `SwitchArrow`, `Tap`) via 6–8x packet piggybacking with sequence deduplication (`cs > coop.last_applied_cmd_seq`) into `_G.OnCommandSonStart(son, cmd)`.

## QA

Automated verification (`qa: automated`):

1. **Standalone sync & mutation twin suite (`test_sync.py`):**
   - `5 checks, 5 twins, 0 failed — ALL OK`:
     - `check_stale_rejected` (catches `accept_stale_seq`)
     - `check_cmd_loss_and_dedup` under 40% deterministic packet loss (catches `no_cmd_repeat` and `double_apply_cmd`)
     - `check_sync_move_guard` during `IsDoingSyncMove()` (catches `ignore_sync_move`)
     - `check_leash_clamp` beyond 35m from Kratos (catches `no_leash`)
     - `check_position_convergence` over 30 ticks
2. **Live `GoW.exe` Host + `tools/chaos` (`bad_wifi` profile) + Simultaneous 2-Instance `GoW.exe` Host & Client test (`run_live_qa.py`):**
   - Provisioned 2 isolated hardlinked instances (`156,589` hardlinks each) via `tools/gamepool/gamepool.py --config local/gow_gamepool.json provision --pairs 1`.
   - Verified `Host` (`instance 0`) reached `[COOP] READY` in `12.0s`, redirected saves to `<instance 0>\saves`, and exchanged `97` Client packets / `704` Host packets across `tools/chaos` (`bad_wifi`: 35±25ms latency, 2% base + 60% burst loss, 2% reorder, 0.5% duplicate) with `94` unique in-order packets accepted and all `3/3` combat commands (`Regular`, `SwitchArrow`, `Charged`) applied exactly once (`ack=3`).
   - Verified simultaneous 2nd `GoW.exe` instance (`Client`, `instance 1` with `BypassSteamStub=1`, `BotMode=1`) booted alongside `Host`, reached `[COOP] READY role=client`, and received `108` live Host state packets (`rx_seq=3944`, `kratos=(-399.59,38.00,-800.85)`, `son=(-400.71,38.09,-808.64)`).
   - Verified SHA-256 hashes of `%USERPROFILE%\Saved Games\God of War` before and after all runs (`100%` unchanged).

## How to play

1. Extract `version.dll`, `mods\lua\gameart\scripts\libraries\creature\locomotion.lua`, and `coop_config.ini` into each player's game directory (or isolated test copy).
2. Connect both PCs on the same LAN or virtual LAN (Tailscale / ZeroTier).
3. On **Player 1 (Host — Kratos)**, set `Role=host` and `Port=27960` in `coop_config.ini`.
4. On **Player 2 (Client — Atreus)**, set `Role=client`, `HostIP=<Host_IP>`, `Port=27960`, and `ClientPort=27961` in `coop_config.ini`.
5. Launch the game on both PCs and load the same save. Player 2 controls Atreus using their gamepad (Left Stick to move, Cross to sprint, L2 to strafe/aim, Square for Regular arrow, Triangle for Charged arrow, D-Pad Up to switch Light/Shock arrows, R1 for Runic Summon).

## Known issues

- **Asymmetric camera on Client:** Client runs its own local world loaded at the same save point while mirroring Host's Kratos and Atreus coordinates and sending Player 2's gamepad inputs + combat commands to Host. Full world enemy sync onto the Client is not yet implemented, so for the best visual experience Player 2 can either watch Host's low-latency stream (Parsec / Steam Remote Play) while their local client or UDP gamepad sender transmits inputs, or play in the synchronized local world alongside Host.
- **Sync moves and cutscenes:** During `IsDoingSyncMove()` sequences (cutscenes, boat entry, paired finishers, and the title-screen idle pose before pressing Continue), autonomous AI control temporarily takes over Atreus so scripted sequences cannot soft-lock.

## Sources

- https://github.com/Nukem9/godofwar-gameplay-tweaks
- https://github.com/adore11s/God-of-War-VR-Mod
- https://github.com/kainotoa/GOWTool
- https://github.com/JeanxPereira/GoWToolkit
