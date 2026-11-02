# God of War (2018) — Co-op Research & Reverse-Engineering Notes

Date: 2026-10-28
Status: Phase 0 (Triage), Phase 1 (Research), and Phase 3 (Static Reverse Engineering of installed Steam build `11168363`). No game processes were launched and no files in the owner's install or saves were modified.

## 1. Game Identity (Observed on Owner's Install)

- **Title:** God of War (2018; PC release 2022)
- **Store:** Steam (AppID `1593500`), manifest `appmanifest_1593500.acf`
- **Steam Build ID:** `11168363`
- **Main Executable:** `<game dir>\GoW.exe`
  - **Size:** `20,083,552` bytes
  - **SHA-256:** `caebcb027980d7eac9203d190f9ee649eebc549f8defce138e2114dc91f40452`
  - **Architecture:** x64 (`PE32+`), image base `0x140000000`
  - **PE Timestamp:** `1653506097` (`0x628e8031`, `2022-05-25 19:14:57 UTC`)
  - **File / Product Version:** `1.0.475.7534` / `GoW-4757534-Wed May 25 11:52:20 2022`
  - **Boot Options (`<game dir>\exec\boot-options.json`):** `content-id: PC0000-GOWA08000_00-00000000PLATINUM`, `app-version: 01.35`, `build-id: 79291`, `wad-pack-id: 379064`, `code-changelist: 4757534`, `data-changelist: 5336418`, `code-branch: //int8-code/main81`, `data-branch: //int8-data/main81`
  - **PE Sections:**
    - `.text`: RVA `0x00001000`, VSize `0x00d4649e`, RawPtr `0x00000400`, RawSize `0x00d46600` (~13.3 MB)
    - `.rdata`: RVA `0x00d48000`, VSize `0x0031139e`, RawPtr `0x00d46a00`, RawSize `0x00311400` (~3.1 MB)
    - `.data`: RVA `0x0105a000`, VSize `0x04064bd8`, RawPtr `0x01057e00`, RawSize `0x001b3a00`
    - `.pdata`: RVA `0x050bf000`, VSize `0x00097f5c`, RawPtr `0x0120b800`, RawSize `0x00098000`
    - `_RDATA`: RVA `0x05157000`, VSize `0x000030c0`, RawPtr `0x012a3800`, RawSize `0x00003200`
    - `.rsrc`: RVA `0x0515b000`, VSize `0x00013a58`, RawPtr `0x012a6a00`, RawSize `0x00013c00`
    - `.reloc`: RVA `0x0516f000`, VSize `0x0002fbbc`, RawPtr `0x012ba600`, RawSize `0x0002fc00`
    - `.bind`: RVA `0x0519f000`, VSize `0x0003d160`, RawPtr `0x012ea200`, RawSize `0x0003d160`
- **Engine:** Santa Monica Studio proprietary engine (internal C++ namespace `Kinetica`, branch `int8`, ported to PC by Jetpack Interactive).
- **Shipped DLLs in `<game dir>`:** `bink2w64.dll`, `libSceGnm.dll`, `libSceGpuAddress.dll`, `libSceJobManager.dll`, `libScePad.dll`, `nvngx_dlss.dll`, `steam_api64.dll`, `exec\crs-client.dll`.

## 2. Search Log

- **This repo (`DIRECTORY.md`, `data/directory.yaml`, `games/`, GitHub issues & PRs):**
  - `data/directory.yaml` had a gap entry for `God of War (2018)` (`checked: 2026-10-16`).
  - No `games/god-of-war-2018` write-up existed.
  - GitHub issue/PR search on `cranebeaker/coop-anything` for `"god of war"` returned 0 open or closed issues/PRs.
- **GitHub API repository searches:**
  - Queries: `god of war coop OR multiplayer`, `godofwar mod OR lua OR wad`, `"GoW.exe" OR "Kinetica" god of war`, `GOWTool OR godofwar-gameplay-tweaks OR gow-gameplay-tweaks`, plus forks of `Nukem9/godofwar-gameplay-tweaks`.
- **Web searches:**
  - Queries: `"God of War" 2018 "co-op" OR "multiplayer" mod PC`, `"God of War" PC modding tools wad dcb lua unpacker github`, `"God of War" "Atreus" playable mod OR "Speclizer" OR "multiplayer" OR "co-op"`, `"God of War Ragnarok" "co-op" OR "multiplayer" mod PC`.
- **Not searched directly:** Discord servers, non-English forums.

## 3. Observations (Verified Facts)

### 3.1 Existing Mods and Tooling (Public Sources)

- **No co-op or multiplayer mod exists** for *God of War (2018)* on PC (or *God of War Ragnarök*).
- **[Nukem9/godofwar-gameplay-tweaks](https://github.com/Nukem9/godofwar-gameplay-tweaks)** (last push `2024-02-01`, no license stated / "No license provided. TBD", 3 forks with no newer commits):
  - Loads into `GoW.exe` as a `version.dll` proxy.
  - Hooks `IFFProcessLuaScriptChunk` (RVA `0x466de0`, file offset `0x4661e0`) and `LoadLuaScriptChunk` (RVA `0x466260`, file offset `0x465660`) to dump Lua 5.2 bytecode chunks (`.lua.bin`) from `.wad` loads and compile/inject replacement `.lua` source files from `mods\lua\` at runtime using the game's own `luaL_newstate`, `lua_load`, and `str_dump`.
  - Bypasses the engine's fixed WAD Lua chunk pool by patching the allocator call sites (`LuaScriptChunkMemoryAllocatorHookLoc` and `LuaScriptAllocatorHookLoc`) to use `_aligned_malloc` and `realloc`/`free`.
  - Maps the engine's internal RTTI tables (`Kinetica::RTTI::TypeDeclTable`, `EnumDeclTable`, `TypeAttributeTable`, `ArrayAttributeTable`, `HashMapAttributeTable`), `GameModule::SLIFunctionTable`, `GameModule::SLIPropertyTable`, `gCameraBlender`, and `gLocalPlayerGO`.
  - Includes a warning in `CameraHooks.cpp` (`hk_CameraObjectGetFov`): `GoW.exe` is compiled with whole-program optimization and expects certain functions to preserve volatile register `R9` across calls (same hazard documented for *Middle-earth: Shadow of War* in `playbook/methods.md` item 67).
- **[adore11s/God-of-War-VR-Mod](https://github.com/adore11s/God-of-War-VR-Mod)** (MIT License, active, last push `2026-10-26`):
  - Full-source C++ `version.dll` proxy for *God of War (2018)* supporting Steam, Epic, and GOG builds.
  - Its recorded reference build (`PE timestamp 0x628e8031 = 1653506097`) is the exact Steam build (`11168363`) installed on this machine (`source/src/core/addrsigs.inc` lists 166 static VAs and byte signatures).
  - Maps the character/entity table (`*(0x1422a98c0)`, array at `+0x20`, count at `+0x30`), the hero pointer (`*(0x1422c6950)` and `*(*(*(0x1422e7d50) + 8) + 8)`), the skeleton layout (`*(obj + 0x238)`, joint count `*(int16_t*)(skel + 0x38)`, joint hash array `*(uint64_t**)(skel + 0x30)`, world matrix array `*(uint8_t**)(skel + 0x58) + (count + i) * 0x40`), local-to-world skeleton evaluation (`0x140591550`), cinematic mode flag (`*(uint8_t*)0x1422e9d2f`), virtual XInput pad (`XInputGetState` IAT hook) and `libScePad` PlayStation pad, focus override (`GetForegroundWindow` hook so background instances accept input), and a live command channel (`gowvr_logs\cmd.txt`).
- **[kainotoa/GOWTool](https://github.com/kainotoa/GOWTool)** (last push `2024-10-31`, no license file) and **[JeanxPereira/GoWToolkit](https://github.com/JeanxPereira/GoWToolkit)**:
  - Asset browsers and extractors for `.wad`, `.toc`, `.texpack`, and `.lodpack` containers.
- **Playable Atreus video (Speclizer, 2022):**
  - Demonstrated swapping player control/character to Atreus in single-player; never released as a co-op mod.

### 3.2 Static Analysis of Installed `GoW.exe` and `.wad` Archives (Verified by Local Scripts)

1. **Unencrypted code and data sections on disk:**
   - Although `GoW.exe` has a `.bind` section (SteamStub), `.text`, `.rdata`, and `.data` are **not encrypted** on disk. All byte signatures tested from `Nukem9/godofwar-gameplay-tweaks` matched `GoW.exe` on disk with `1 match(es)` each:
     - `wadLoader::OpenFile`: file offset `0x41cba0` (RVA `0x41d7a0`, VA `0x14041d7a0`)
     - `dc::LookupSymByHash`: file offset `0x430f90` (RVA `0x431b90`, VA `0x140431b90`)
     - `IFFProcessLuaScriptChunk`: file offset `0x4661e0` (RVA `0x466de0`, VA `0x140466de0`)
     - `LoadLuaScriptChunk`: file offset `0x465660` (RVA `0x466260`, VA `0x140466260`)
     - `lua_newstate`: file offset `0x9e8110` (RVA `0x9e8d10`, VA `0x1409e8d10`)
     - `lua_load`: file offset `0x9e2b20` (RVA `0x9e3720`, VA `0x1409e3720`)
     - `UpdatePlayerPositionJob`: file offset `0x7924b0` (RVA `0x7930b0`, VA `0x1407930b0`)
     - `gLocalPlayerGO`: RVA `0x022e7d50` (VA `0x1422e7d50`)
     - `gCameraBlender`: RVA `0x022a93d0` (VA `0x1422a93d0`)
2. **Uncompressed Lua 5.2 bytecode with full debug symbols in `.wad` files:**
   - Scanning all 597 `.wad` files in `<game dir>\exec\wad\pc_le\` found **6,627 `IFFLuaScriptChunkHeader` chunks** (`IFFHeader` type `0x24`, `0x60` bytes + `0x204`-byte script header), covering **1,423 unique Lua script paths** (`%PROJ_PATH%/gameart/scripts/...`).
   - Every chunk begins with `1b 4c 75 61 52 00 01 04 08 04 04 00 19 93 0d 0a 1a 0a`: standard Lua 5.2 little-endian bytecode with 4-byte `int`, 8-byte `size_t`, 4-byte `Instruction`, and **4-byte `float` `lua_Number`**.
   - Debug information is **not stripped**: chunks retain original source paths (`@Z:\int8\gameart\scripts\...`), function names, local variable names, and upvalue names.
   - Key WADs and scripts:
     - `r_perm.wad` (198 scripts): `libraries/core/thunk.lua` (`__hook_thunks`, `InstallHook`), `libraries/creature/locomotion.lua` (`SetActuator`, `SetFocus`, `FollowNavCurve`, `motionWarp.SetWarp`), `libraries/design/puppeteerlibrary.lua` (`game.Puppeteer.NewForce`, `SyncAndPlayBranch`, `GoToLocation`), `libraries/design/questlibrary.lua` (`game.QuestManager.GetQuestState`, `StartQuest`, `ProposeQuestState`, `IncrementQuestProgress`), `libraries/socket.lua`, `libraries/mobdebug.lua`.
     - `r_heroa00.wad` (4 scripts): `characters/heroa00/main.lua` (405,218 B bytecode), `perks.lua`, `audio_luahooks.lua`, `libraries/ui/util.lua`. Contains hooks such as `SendInputDataToSon`, `OnSonRevive`, `OnSonCoopJumpAttack`, `LuaHookDecision_CanSonPerformSpecial`, `LuaHook_SpawnGameObject`.
     - `r_son00.wad` (22 scripts): `characters/son00/main.lua` (181,180 B), `son_brain.lua` (224,654 B), `son_awarenessbrain.lua` (107,105 B), `son_motionbrain.lua` (72,605 B), `son_helper.lua` (34,656 B), `son_poiframework.lua` (28,727 B), `son_gamestate.lua` (15,598 B), `son_input.lua` (12,099 B), `son_targetbrain.lua` (8,544 B), `son_interaction.lua` (5,432 B).
3. **Built-in `socket.core` (LuaSocket TCP/UDP) and `cmsgpack` (MessagePack) in `GoW.exe`:**
   - Inspecting `luaL_openlibs` (`loadedlibs[]` and `preloadedlibs[]` at file offset `0xd9cff0`..`0xd9d090`, RVA `0xd9e5f0`..`0xd9e690`) in `GoW.exe` revealed that every Lua state registers:
     - `_G` (`0x1409fdb80`), `package` (`0x1409fff80`), `coroutine` (`0x140a08860`), `table` (`0x1409fb720`), `string` (`0x140a048d0`), `bit32` (`0x140a09260`), `math` (`0x140a057d0`), `debug` (`0x140a080d0`), **`cmsgpack`** (`0x1409fa370`), and preloads **`socket.core`** (`0x140a05bd0`, with `udp`, `sendto`, `receivefrom`, `settimeout`, `setsockname`, `setpeername`).
4. **Native C-to-Lua API bindings and engine reflection in `GoW.exe`:**
   - **2,057 `(name_va, func_va)` entries** in `.rdata`/`.data` (24-byte `0x18` stride in `.data` from RVA `0x011bc500` to `0x011c9000`), exposing:
     - Entity / Transform / Spawning: `GetWorldPosition` (`0x14018f800`), `SetWorldPosition` (`0x1405fc0f0`), `SetLocalPosition` (`0x1405fbf20`), `SetWorldFacing` (`0x1405fbd00`), `SetWorldOrientation` (`0x1405fbb20`), `SetWorldTransform` (`0x1405fb7d0`), `SpawnGameObject` (`0x1405ffc20`), `SpawnProp` (`0x1405ff930`), `SpawnWildlife` (`0x1405f9850`), `ClonePose` (`0x1405feec0`), `AnimSync` (`0x140607b80`).
     - Player / Companion / AI / Moves: `GetPlayer` (`0x140222030`), `GetAI` (`0x140221e30`), `IsCreature` (`0x14018f420`), `GetCreature` (`0x1405fa4f0`), `TriggerMoveEvent` (`0x1407dc860`), `IsPlayingMove` (`0x1407dd1a0`), `IsInNavigationMove` (`0x1407dd130`), `GetActiveMovePercent` (`0x1407dcff0`), `IsDoingSyncMove` (`0x140223470`), `IsSyncMaster` (`0x140223550`), `IsSyncSlave` (`0x140223630`), `GetSyncInfo` (`0x1407da010`), `RequestInteract` (`0x1407d3150`), `HasInteractAvailable` (`0x1407d2f30`).
     - Story / Variables / Saves / Cinematics: `GetVariable` (`0x140787c60`), `SetVariable` (`0x1407878d0`), `OnVariableChange` (`0x1407804c0`), `CreateEntityVariableMonitor` (`0x1407806e0`), `GetHeroPersistentLevel` (`0x14020afe0`), `GetSonPersistentLevel` (`0x14020b080`), `GetPermLevel` (`0x14020b130`), `MarkForSave` (`0x140781120`), `SetSaveRequired` (`0x1405fb470`), `SetInhibitSave` (`0x1405fb3c0`), `EnableCinematicMode` (`0x1407a1830`), `DisableCinematicMode` (`0x1407a17e0`), `IsInCinematicMode` (`0x1407a17b0`).
   - **395 `SLIFunctionHandler` entries** at RVA `0x011ca300` and **86 `SLIPropertyHandler` entries** at RVA `0x011cc9a0`.
   - **482 `EnumDecl` entries** statically initialized at RVA `0x01184e70` (including `eTeams`, `ePad`, `ePadMask`, `ePlayerControls`, `eSynchFlags`, `eCombatMoveType`, `eSaveOption`), and **1,198 `TypeDecl` slots** at RVA `0x011daa70` (entries `2..1197` populated by dynamic initializers at runtime).
   - **1,263 MSVC RTTI classes/structs** in `.data`, revealing a command-queue architecture (`ScopedLuaRequestQueue`, `DMARequestAllocator`) where Lua bindings enqueue typed requests (`SetPositionRequest`, `SetFacingRequest`, `SetTransformRequest`, `WarpCreatureRequest`, `SpawnGameObjectRequest`, `ClonePoseRequest`, `MimicRequest`, `ModifyCreatureMotionRequest`, `ModifyMoveSystemRequest`, `LevelSetVariableRequest`).
5. **Saves, Settings, and Single-Instance Checks:**
   - `GoW.exe` reads `settings.ini` from the working/game directory (`./settings.ini`).
   - Saves use `SHGetKnownFolderPath` (`FOLDERID_SavedGames` -> `%USERPROFILE%\Saved Games\God of War\<steam_id>\`). `GoW.exe` has **0 imports/strings for `ISteamRemoteStorage`** (Steam Cloud uses Auto-Cloud on the `Saved Games` folder rather than the `ISteamRemoteStorage` API).
   - `GoW.exe` has **0 `OpenMutex` and 0 `FindWindow` hits** (only 1 `CreateMutexA` import in the crash/fiber runtime).

## 4. Derived (From Static Analysis & Existing Mods)

- **Why asymmetric Kratos + Atreus co-op is the shortest path:**
  - *God of War (2018)* keeps both Kratos (`r_heroa00.wad`, `game.Player.FindPlayer()`) and Atreus (`r_son00.wad`, `game.AI.FindSon()`) loaded, animated, and combat-capable throughout virtually the entire campaign.
  - Trying to spawn a second Kratos (`goPlayer`) in a single-player camera/sync-move engine would fight every grapple, boat transition, and `PlaySingleSynchMove_KratosSonObject`. Having **Player 1 play Kratos (host)** and **Player 2 play Atreus (joiner)** reuses the game's existing Atreus entity (`son00`) without needing to invent a second player class.
- **How Player 2 can drive Atreus (`son00`) in Lua + C++:**
  - Atreus's AI brain (`son00/main.lua`, `son_brain.lua`, `son_motionbrain.lua`, `son_input.lua`) already exposes `gVFSDisableSonCommands`, `gVFSDebugNoBrainLogic`, `creature.locomotion` (`SetActuator`, `SetFocus`, `motionWarp.SetWarp`), `TriggerMoveEvent`, and `game.Puppeteer`.
  - Even better, because `GoW.exe` has `socket.core` (UDP) and `cmsgpack` built into every Lua VM, a Lua hook loaded via `IFFProcessLuaScriptChunk` (`Nukem9` method) can open a non-blocking UDP socket (`require("socket").udp()`) directly from Lua!
- **Two architectures to evaluate in Stage 1–2:**
  1. **Two-instance state sync (Kratos on Host, Atreus on Client):**
     - Both instances run the same level/save. Host runs Kratos locally and overrides Atreus's AI/locomotion (`SetActuator` / `TriggerMoveEvent` / `SetWorldPosition` / `SetWorldFacing`) from Client's controller input or state packets; Client runs Atreus locally (with camera attached to Atreus via `gCameraBlender` hook from `Nukem9`/`God-of-War-VR-Mod`) and mirrors Kratos + enemies + `SetVariable`/`QuestManager` state from Host.
  2. **Single-instance / Parsec / Steam Remote Play Together asymmetric co-op (or local dual-pad):**
     - Because both Kratos and Atreus are already in the same world and on screen (or with a detached second viewport/camera), a second XInput/PlayStation controller on the host machine (or over Parsec/Remote Play) can directly drive Atreus's locomotion (`SetActuator`), aiming, bow attacks (`TriggerMoveEvent`), and runic summons while suppressing his autonomous AI (`gVFSDebugNoBrainLogic`), with zero network desync on enemies, quests, or cutscenes!

## 5. Runtime Verified Findings (2026-10-28)

- **SteamStub v3.1 Entry Header & Multi-Instance Launch:**
  - `.bind` header at `0x14519f220..0x14519f310` has magic `0xC0DEC0DF`, `OriginalEntryPoint = RVA 0x3ec680` (`0x1403ec680`), and flags `0x06` (no section encryption).
  - Patching `.bind` entry `0x14519f310` with a 5-byte relative jump (`E9 6B D3 24 FB`) to `0x1403ec680` and hooking IAT `steam_api64.dll!SteamAPI_RestartAppIfNecessary` (`0x140d49328`) to return `false` allows a second simultaneous `GoW.exe` instance (`BypassSteamStub=1`) to launch alongside a running `GoW.exe` Host on the same PC with zero file-lock contention on hardlinked `.wad` / `.lodpack` archives.
- **Lua 5.2 Disassembly & VM Structure:**
  - Disassembled all scripts in `r_son00.wad`, `r_perm.wad`, and `r_heroa00.wad` using a custom Lua 5.2 4-byte float disassembler (`IFFHeader.m_Length` at `h - 0x60 + 4` is measured from `IFFLuaScriptChunkHeader` at `h`, so bytecode is `data[h + 0x204 : h + m_Length]`).
  - Game Lua VMs (`LuaScriptAllocatorHookLoc` at `0x1405a6a1f`) do **not** load `io` or `os` (`io == nil`), and any unhandled Lua error enters `0x14063d4d0`, formats the traceback (`printf` at `0x14063d610`), and executes `__debugbreak()` (`int 3`, `0xCC`) at `0x14063d629` (`STATUS_BREAKPOINT` / `0x80000003`).
  - Injecting `_G.__COOP_CFG` directly into the Lua source buffer in `hk_IFFProcessLuaScriptChunk` (`0x140466de0`), hooking `0x14063d610` via the 19-byte `0xCC` cave at `0x14063d4ad`, and patching `0x14063d629` (`0xCC -> 0x90`) provides full Lua error logging without crashing the process.
- **Single-VM Co-op Hook inside `son00` (`gameart/scripts/libraries/creature/locomotion.lua`):**
  - `locomotion.lua` is `require`d and `.Install()`ed by both `heroa00/main.lua` and `son00/main.lua`.
  - Filtering `OnCreatureUpdate(crt)` by `crt == game.AI.FindSon()` runs the co-op loop inside `son00`'s own `lua_State*` VM, where `son` (`Atreus`), `player = game.Player.FindPlayer()` (`Kratos` + `player.Pad`), `_G.__coop_override_until`, `son:SetActuator`, `son:Warp`, and `_G.OnCommandSonStart(son, cmd)` (`"Regular"`, `"Charged"`, `"SwitchArrow"`, `"Tap"`) all reside in the same VM.

## 6. Guesses & Remaining Unknowns

- **Guesses:** Synchronizing enemy health/transforms or streaming Host's viewport to Client would eliminate world enemy divergence on the Client screen during long combat encounters.
- **Unknowns:** Behavior of realm-travel loading transitions (`CompletedCineNumber` jumps) across multi-hour campaign sessions.

## 7. Test Log

- Ran `coop_mcp.game_identity` and `coop_mcp.detect_engine` against `<game dir>\GoW.exe` (read-only).
- Ran `local/decompile_lua52.py` to disassemble all Lua 5.2 scripts in `r_son00.wad`, `r_perm.wad`, and `r_heroa00.wad`.
- Ran `local/coop_mod/test_sync.py`: `5 checks, 5 twins, 0 failed — ALL OK` (`accept_stale_seq`, `no_cmd_repeat`, `double_apply_cmd`, `ignore_sync_move`, `no_leash`).
- Built `version.dll` (`cl.exe` 14.44.35207 x64) and provisioned 2 isolated hardlinked instances via `tools/gamepool/gamepool.py --config local/gow_gamepool.json provision --pairs 1`.
- Ran `local/coop_mod/run_live_qa.py`:
  - Verified `SHGetKnownFolderPath` save redirection to `<instance>\saves` and 100% unchanged SHA-256 hashes of `%USERPROFILE%\Saved Games\God of War`.
  - Verified live `GoW.exe` Host (`PID 27392`) + `tools/chaos` (`bad_wifi` profile: 35±25ms latency, 2% base + 60% burst loss, 2% reorder, 0.5% duplicate): `97` Client packets sent, `94` unique in-order packets accepted (`rx_seq=96`), `704` Host packets received, and `3/3` reliable commands (`Regular`, `SwitchArrow`, `Charged`) applied exactly once (`ack=3`).
  - Verified simultaneous 2-instance `GoW.exe` launch (`Host PID 27392` + `Client PID 16076` with `BypassSteamStub=1`, `BotMode=1`): both instances reached `[COOP] READY` simultaneously and exchanged live `cmsgpack` state packets (`Client rx=108, rx_seq=3944`).


