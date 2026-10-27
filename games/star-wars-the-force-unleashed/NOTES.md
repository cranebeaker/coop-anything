# Star Wars: The Force Unleashed (Ultimate Sith Edition, PC) — notes

## Identity

- Store: Steam app 32430, build id 6522
- `SWTFU.exe` SHA-256 `1C156705548FCE9E33FA3423E75B5CBE36248319B96F4B4850BF2E8125692032`, PE32 x86
- Port by Aspyr Media (exe VersionInfo CompanyName)
- OS tested: Windows 11

## Observations (2026-10-23)

- Install layout: `LevelPacks/`, `Shared/`, `Scum/`, `FMV/`, `Support/`.
- File types by count: `.pcd` 765, `.pcp` 765, `.lp` 159, `.cab` 123, `.bik` 51, `.csv` 42, `.z` 34.
- DLLs in root: `AWL_Release.dll`, `AWL.net.dll`, `ScumLibLEC_Retail.dll`,
  `libScumEuphoria_win32_release_DLL.dll`, `SlimDX.dll`, `binkw32.dll`, `mss32.dll`, `zlib1.dll`,
  `GameuxInstallHelper.dll`.
- `PC_LevelSelectList.txt` and per-DLC list files are plain text in the root.
- Hub check: no entry in `DIRECTORY.md`, no `games/` folder, no GitHub issues or PRs matching
  "Force Unleashed" or "TFU" (gh search, 2026-10-23).

### SWTFU.exe static scan (2026-10-23, own Python scan of the test copy)

- Link timestamp 2010-02-12 23:56:03 UTC.
- Sections: `.text .rdata .data .tls .rsrc .bind`. `.bind` is the SteamStub DRM wrapper section
  name (needs confirming / unwrapping before attaching a debugger to the test copy).
- MSVC RTTI present with namespace `Ronin` (e.g. `.?AUCharacterManager@Ronin@@`,
  `.?AVAIBehaviorAttack@Ronin@@`, `RoninEngine::RoninUnloadWorld`): engine is LucasArts' Ronin.
- Embedded Lua (`$URL: www.lua.org $`), with script bindings carrying help strings such as
  "Sets the linear velocity of a Ronin Physics Entity."
- Script functions mention a multiplayer camera: "Setup the multiplayer camera with 2 splines",
  "... with a axislocked cameraposition". 24 "multiplayer" hits total.
- DLL references include `wsock32.dll`, `dinput8.dll`, `xinput1_3.dll`, `d3d9.dll`, `loadall.dll`.
- Keyword hit counts: Ronin 674, lua 249, Havok 103, Euphoria 42, DMM 143.

## Research (2026-10-23)

### Observed (with sources)

- No co-op / multiplayer / netplay mod for TFU1 PC found (web, Reddit, YouTube, ModDB, GitHub).
  Steam thread: co-op "only for wii": https://steamcommunity.com/app/32500/discussions/0/523897277916637168
- Engine Ronin: https://www.pcgamingwiki.com/wiki/Star_Wars:_The_Force_Unleashed ,
  https://mobygames.com/game/star-wars-the-force-unleashed . Matches RTTI scan above.
- Wii (Krome) had local 2-player Duel; PSP (Krome) 2–4 player modes. Different codebase from PC/360/PS3.
  https://www.engadget.com/2008-04-15-joystiq-hands-on-star-wars-the-force-unleashed-duel-mode-wii.html
- TFU-Modloader (Reloaded-II, C#), **no license**, last push 2026-04-16:
  https://github.com/LongTermMess/TFU-Modloader . Checked in its `LuaHooks.cs:47-50`:
  `LuaRegister` 0x4d2120, `Scripts_SendMsg` 0x4f46a0, `ronin_loadworld` 0x478520 (wrapper);
  `luaexec 0x4d2550` in a comment. README: exe must be stripped with Steamless.
  Which exe build these addresses match: unknown — check against our SHA after unwrapping.
- TFU-Import-3dsMax (GPL-3.0): kaPA `.lp` format, GTO models, `tfudump` CLI:
  https://github.com/DennisHerrm/TFU-Import-3dsMax
- pc-trainers (Unlicense), CE scripts for SWTFU.exe: player via health hook (`ecx`, health +4,
  max +8), Force `[edi+0x32C]`/`+0x328`: https://github.com/unknownv2/pc-trainers
- Nexus mod 30: D3D9 mod menu with runtime NPC spawner (source/license unknown):
  https://www.nexusmods.com/starwarstheforceunleashed/mods/30
- Game capped 30 fps; physics issues reported above 60 fps:
  https://www.nexusmods.com/starwarstheforceunleashed/mods/35 ,
  https://steamcommunity.com/app/32430/discussions/0/2250055685734987808
- Steam build: SteamStub DRM (Steamless needed). GOG build DRM-free (PCGamingWiki).
- No decompilation or published Ghidra project found.

### Decision

Build from zero (no existing co-op to revive). Host-authority model on the owner's PC, 2 players
first, designed for 4. Start: unwrap test exe with Steamless, confirm modloader addresses on our
build, then puppet a second Starkiller actor driven by remote input.

## DRM unwrap of the test copy (2026-10-23)

- Steamless v3.1.0.5 from https://github.com/atom0s/Steamless/releases/tag/v3.1.0.5 , zip 610,646
  bytes (matches size GitHub lists), SHA-256
  `E3E2D22E098FF3FB359B2876AA2BED9596F0501E6FF588CBFFAE90A76D2DC4F5`. The project publishes no
  official hash, so this pins our download only; it is not checked against an upstream value.
- `Steamless.CLI.exe` on the test copy: "File is packed with SteamStub Variant 2.1", `.bind`
  removed, `.text` decrypted. Unpacked exe 15,069,184 bytes, SHA-256
  `2D872CA97BA05C6E9777506549B514AC2DA7E74D5A0AA08DDD4F8299A95D7B8A`. Sections now
  `.text .rdata .data .tls .rsrc`, ImageBase 0x400000.
- Static check of TFU-Modloader addresses on the unpacked exe: each is preceded by `CC` padding
  and starts with a normal prologue:
  - `LuaRegister` 0x4d2120: `56 e8 5a f5 ff ff 8b c8 ...`
  - `luaexec` 0x4d2550: `56 8b 74 24 08 57 6a 00 ...`
  - `Scripts_SendMsg` 0x4f46a0: `53 56 8b 74 24 0c 57 6a ...`
  - `ronin_loadworld` 0x478520: `8b 44 24 10 8b 54 24 08 ...`
  So the modloader addresses very likely match Steam build 6522. Not yet confirmed at runtime.

## Boot test of the unpacked exe (2026-10-23)

- Launched `<test dir>\SWTFU.exe` (Steamless-unpacked) directly, no Steam launcher, no
  `steam_appid.txt`. Process stayed up, window "Star Wars: The Force Unleashed", 1920x1080,
  ~253 MB working set at 20 s. Window capture showed the main menu (New Game / Continue Game /
  Load Game). Closed with WM_CLOSE, exited cleanly within 10 s. So the unwrapped exe boots to menu.
- **Shared user data (important for every test run):** the test copy uses the same per-user files
  as the real install:
  - Settings: `%LOCALAPPDATA%\Aspyr\Star Wars The Force Unleashed\Config.xml`. The game rewrites
    it on exit (mtime changed during our run).
  - Saves: `<Documents>\Aspyr\Star Wars The Force Unleashed\SWTFU.BIN`, with
    `steam_autocloud.vdf` (Steam Cloud). The path comes from AWL_Release.dll's
    `<SavedGames baseKnownFolderID="{FDD39AD0-...}" path="Aspyr\Star Wars The Force Unleashed" />`
    (FOLDERID_Documents) and `Aspyr::Win::GetPathUserVisible`. The save was not written by our run.
  - Registry: AWL `RegPath` = `Aspyr\Star Wars The Force Unleashed`.
  - "Continue Game" was offered, so the test copy read the real save.
  - Before any run that goes past the menu, redirect `GetPathUserVisible` /
    `GetPathUserNonVisible` (AWL_Release.dll exports) to a test folder, or run under a separate
    Windows user. Without that, the test copy writes the owner's save, which Steam Cloud then syncs.

## User-data redirect for test copies (2026-10-23)

- Import scan: in the DLLs SWTFU.exe loads, only `AWL_Release.dll` asks for user folders
  (`SHELL32!SHGetFolderPathW`); it also uses `ADVAPI32!RegOpenKeyExW/RegCreateKeyExW`.
  `GameuxInstallHelper.dll` also imports it but is not loaded by the exe.
- SWTFU.exe statically imports `DINPUT8.dll` (only `DirectInput8Create`), so a `dinput8.dll`
  proxy in the game folder loads before the game runs. It patches AWL's import table:
  CSIDL 0x05/0x1A/0x1C go to `<game dir>\coop-userdata\{Documents,RoamingAppData,LocalAppData}`,
  and HKCU `...Aspyr\X` goes to `Software\Aspyr-coop-test\X`. Source: own repo `tfu-coop/redirect`.
- Test run: the log shows patches active before AWL's first folder call (18:25:47.846 patch,
  .867 first SHGetFolderPathW). Calls seen: CSIDL 0x1C (Config.xml) and 0x05 (saves). HKLM
  `Software\Aspyr\Star Wars The Force Unleashed` is read (left alone).
- The game booted at the default 1024x768 (fresh config). Escape skipped the intros, and the main
  menu showed "Continue Game" greyed out (no save visible). It created a new `SWTFU.BIN` (267,728
  bytes) and `Config.xml` under `coop-userdata`. The owner's Config.xml, SWTFU.BIN and
  steam_autocloud.vdf had the same SHA-256 and mtime before and after the run.
- Fullscreen D3D9 minimizes when it loses focus; restore it with `ShowWindow(SW_RESTORE)` before
  capturing. DirectInput accepts `SendInput` scan codes (Escape skipped the movies).

## Graphics: resolution + DXVK on the test copy (2026-10-23)

- Resolution: `VideoWidth`/`VideoHeight` in the (redirected) Config.xml set to 2560x1440. The game
  rendered at 2560x1440 (window capture size), no configurator needed for resolution.
- DXVK v3.1.1 from https://github.com/doitsujin/dxvk/releases/tag/v3.1.1 , `dxvk-3.1.1.tar.gz`
  SHA-256 `40565b4a...4f54e608`, matching GitHub's published asset digest. The x32 `d3d9.dll` sits
  next to SWTFU.exe and loads alongside our `dinput8.dll` proxy (no conflict).
- `SWTFU_d3d9.log`: DXVK has a **built-in profile for SWTFU.exe** (`d3d9.deviceLossOnFocusLoss = True`).
  Device: the test PC's GPU (AMD, Vulkan). 0 `err` lines, 1 warn (built-in frame limiter;
  DXVK suggests an external limiter).
- DXVK HUD: main menu 30.0 fps (game's own cap), frame time steady around 33 ms. During
  loading/transition 144 fps (vsync at 144 Hz), 6.3–7.6 ms.
- The loading/transition scene showed glowing cylinders with strong bloom. Not compared with
  native D3D9, so whether it renders correctly: unknown.
- Not tested yet: gameplay in a level under DXVK.

## Graphics: ReShade on top of DXVK (2026-10-23)

- ReShade 6.8.0 from https://reshade.me/downloads/ReShade_Setup_6.8.0.exe (4,075,064 bytes).
  No official hash, so this pins our download only: SHA-256
  `207AEA16205FBF952BC8FE1879966672454CF04002E7AD34237C7990A5B3C0B4`.
  Authenticode signer `CN=ReShade, E=info@reshade.me`, self-signed (Windows: UnknownError).
  `ReShade32.dll` (from the setup's embedded zip) SHA-256 `538998F6...CE518B88`.
- DXVK and ReShade both want `d3d9.dll`, so ReShade runs as a Vulkan layer under DXVK. The stock
  `ReShade32.json` has no `enable_environment`, so it would load into every 32-bit Vulkan app. We
  register a copy with `"enable_environment": {"ENABLE_VK_LAYER_reshade_tfu": "1"}` in
  `HKCU\Software\Khronos\Vulkan\ImplicitLayers`, so only processes started with that variable get
  it. That other apps stay clean follows from the Vulkan loader's implicit-layer rules; not tested.
- Shaders: crosire/reshade-shaders `slim` @ fd0022170615 and CeeJayDK/SweetFX @ 93ddf39b357f (MIT),
  the sources ReShade's own `EffectPackages.ini` lists. Preset: SMAA, CAS (Sharpening 0.6),
  Vibrance 0.15.
- Run: ReShade.log "loaded from ...\layer\ReShade32.dll into ...\SWTFU.exe", SMAA/CAS/Vibrance
  compiled, 0 ERROR lines; DXVK 0 err. Main menu rendered at 2560x1440, 30 fps. Compared with the
  DXVK-only menu capture: mean saturation 0.6395 → 0.6686, mean adjacent-pixel brightness delta
  0.00857 → 0.00933 (menu animates, so only an indication).
- With the ReShade layer active, WM_CLOSE did not end the process within 15 s (force-stopped).
  Without ReShade it closed in under 10 s. Cause unknown.

## Lua binding table (2026-10-23)

- `LuaRegister(category, name, fn, description)`, cdecl at 0x4d2120 (signature from TFU-Modloader
  `LuaHooks.cs:20`). Our byte scan (`tfu-coop/re/lua_bindings.py`) finds 2,147 `call 0x4d2120`
  sites with four `push imm32` args (16 sites have non-immediate args). It writes category, name,
  function address and help text to a TSV. Top categories: vfx 354, trans 316, physics 139, ai 139,
  game 124, edl 124, light 119, ronin 79, ui 77, animation 72, player 44, **Mp 13**.
- **Leftover multiplayer plumbing, exposed to Lua (observed in help strings only; whether it works
  on PC is unknown):**
  - `Mp.SetPlayerController(renID, playerIndex)` 0x8354c0: "Set the character defined by renID to
    be controlled by the player defined by playerIndex. (playerIndex is 1 based)"
  - `Mp.GetMpActorName(playerIndex)` 0x835480, `Mp.FixupPlayerSpawn(renID, playerIndex)` 0x835440
    "Fix up a player ren after you spawn it", `Mp.RequestRespawn(renID)` 0x835500,
    `Mp.CacheEncounterAsset(name) = renID` 0x835570 "Returns ren id of loaded actor".
  - Encounter mode: `Mp.GetMPEncounterSpawns`, `EnableEncounterSpawnGroup`, encounter deck,
    cards and energy per "player who controls renID".
  - `player.*` (44) take `playerNumber` (Dash, ForcePush, Jump, SaberSwing, SetInvincible,
    TeleportNext...). `game.Set*Rank` / `EquipCostume` take `playerControllerNo`.
    `ren.GetPlayer(Ren) = Controller Index`.
  - Shared camera: `camera.SetUpMpArenaCamera` / `SetUpMpSplineCamera` 0x83b840,
    `SetUpMpAxisCamera` 0x83b920.
  - Input maps per controller: `ronin.InputPushMap(sMap, iController)`, `InputGetMappedAxisValue(MapName, Controller)`.
- Derived: the PC exe still contains an engine-level N-player model (player index → controlled ren,
  per-controller input, MP camera), probably from a cut encounter/arena mode. If it works, co-op
  can drive player 2..4 through it instead of writing a puppet system from scratch. Next step:
  decompile these functions and call them from Lua in a test level.

## Ghidra decompile of the MP functions (2026-10-23)

Ghidra 12.1.4 headless full analysis of the unpacked exe succeeded (project in the test folder).
Decompiled with `tfu-coop/re/ghidra/DecompileAt.java`. The functions are real code, not stubs:

- `FUN_00c33160(slot)`: walks a `std::map` of player-controller objects; returns the ren whose
  controller index equals `slot`.
- `FUN_00c33270(ren, slot, flag)`: sets the controller of `ren` to `slot` (vfunc +0x30) and resets
  other controllers on that slot to -1. `Mp.SetPlayerController` calls it with `playerIndex-1`.
- `player.Jump/Dash/...` → `FUN_00833d30(bit)`: get slot N's ren, then set action bit `bit`
  (Jump 0x11, Dash 0x18) in its component's bitfield. A per-player action injection point.
- `Mp.FixupPlayerSpawn` → `FUN_007b8770(slot)`: large per-player setup (controller index, components).
- `camera.SetUpMp*Camera`: loops slots 0..3 → the engine is built for 4 player slots.
- Unknown: how many controller objects exist in a story level, and whether a second pad reaches slot 1.
- Plan: `tfu-coop/docs/plan-coop.md` (host simulates everything; remote players drive slots 1..3).

## Milestone 1: Lua console (2026-10-23)

- **Lua 5.1** (`_VERSION` printed "Lua 5.1"). The stack slot (TValue) is 8 bytes, so lua_Number is a 32-bit float.
  API (cdecl): gettop 0x616850, settop 0x616860, pushstring 0x616f10, pushcclosure 0x616fe0,
  settable 0x6173a0, tolstring 0x616ca0, type 0x616a00, luaL_loadbuffer 0x618a00, lua_pcall 0x617600.
  Global `lua_State*` at 0x12a17d0.
- Engine Lua VM lock: `FUN_004d1680()` returns a `lec::CriticalSection` (exported Lock/Unlock in
  ScumLibLEC_Retail.dll). Every engine entry into Lua takes it.
- `luaexec` (0x4d2550) → `FUN_004d1e70(L, name, ...)` loads a script **asset** (`"@%s.lua"` via
  AssetCatalog), not a code string. `FUN_004d27d0(name)` = run script asset under lock. The engine
  script logger `FUN_004d0f60` is an empty function in retail, so script errors are silent by default.
- Globals are strict: reading an undefined global raises "Accesing non-existent global [x]".
  Use `rawget(_G, name)`.
- The engine calls Lua `ronin.Update(dt)` every frame with dt = 0.0333 (30 Hz) and looks it up from
  the global table each time, so it can be wrapped from Lua.
- Engine Lua runs on the same thread as the `PeekMessageA` message pump (thread ids matched in 3 runs).
- The protected-call wrapper 0x4d1990 makes no calls while the main menu is idle (0 per 10 s), about
  30/s during menu animation, and 0 during intro videos.
- `ronin.LoadWorld("TieFactory_Act1")` called from our pump at the main menu → the main thread
  stopped, and Windows logged "Application Hang" (event 1002) and closed the game. Reproduced twice
  (once with DXVK device-loss-on-focus disabled, so not a focus issue).
  Calling it from inside a wrapped `ronin.Update` worked: the level loaded.
- Main-menu input: Escape skips videos. Enter/Space/absolute mouse clicks via SendInput did not
  reliably start New Game (one Space press played "Ui_frontend_select"). The menu's New Game does
  not call any traced `game`/`ronin`/`ui` Lua function (375 wrapped), so it starts from C++.
- In `TieFactory_Act1`: `ren.GetPlayer(0)` = player ren at
  `TieFactory_Act1_part2.region/TieFactory_Act1_part2.markerset`, position [229.6, 5.73, 69.1];
  `ren.GetPlayer(1..3)` = nil.
- With DXVK's built-in `d3d9.deviceLossOnFocusLoss = True`, focus changes during a level load
  logged "Device lost" and the game died. The test copy's `dxvk.conf` sets it to False.
- WM_CLOSE does not end the game while in a level (force-stopped after 15 s).

## Milestone 2 progress (2026-10-23, paused by owner mid-run)

Observed:
- `Mp.GetMpActorName(1..4)` = multiPlayerShaakTi, multiPlayerKota, multiPlayerShaakTi, multiPlayerKota.
- `Mp.CacheEncounterAsset(name)` creates an actor ren (`FUN_00c55df0` → `FUN_00c58590`, ren named
  `ActorCacheForAssetPacks_<name>_`). `player.IsPlayer` result by asset:
  `player.actor.xml` true, `player` true, `multiPlayerKota` false, `multiPlayerShaakTi` false,
  `playeranakin.actor.xml` false.
- Player manager: global `*(0x03a1be80)`. `IsPlayer(ren)` = ren is in its map (+4/+8). Controller index
  at player component +0x18 (`FUN_00c34b30`).
- Run 7: after `Mp.SetPlayerController(p2, 2)`, `ren.GetPlayer(1)` returned the spawned
  `player.actor.xml` ren, placed 2 m from player 1 via `location.SetPosition`. Slot assignment works.
- In that run, neither player moved during the bot (stick on virtual pad 1, `player.Jump/Dash/ForcePush(1)`),
  because the "SAVE FAILED" dialog was up (screenshot). Movement of player 2 is **not verified yet**.
- "SAVE FAILED" dialog appears after the level-intro scene when the level is loaded via
  `ronin.LoadWorld` (no save slot set up). The menu's normal path presumably avoids it.
- Virtual pad 0 D-pad presses moved the dialog selection (screenshot). The A press from console files
  was lost (press+release ran in one batch), so timed pad events were added in C.
- The game pauses when its window is not in front: `Aspyr::Win::IsActive` (imported from
  AWL_Release.dll) gates simulation. Forcing it true lets a level load fully in the background.
  The co-op host needs this too.
- `VideoWindowed` (TRUE/FALSE) is a Config.xml key in SWTFU.exe; set TRUE in the test copy.
- Run 8 (last): world loaded but `ren.GetPlayer(0)` was still nil 2.5 min later; not diagnosed (owner
  asked to stop launching the game).
- SendInput keyboard/mouse does not reach the in-game UI; XInput (virtual pads) does.

### Run 10 (2026-10-25, hands-off via `tfu-coop/tools/m2-run.sh`, monitors off)

- Full flow ran unattended: load → intro scene → save dialog answered with pad 0 (D-pad down ×2, A,
  timed in the XInput hook) → spawn → bot.
- Slot 1 = spawned `player.actor.xml` ren at [232.2, 5.87, 67.98], 2 m from player 1.
- **Player 2 driven by the bot**: `player.Dash(1)` at frame 90 moved player 2 from z 67.98 to z 76.58
  (8.6 m) by frame 120 while player 1 stood still. `player.Jump(1)` moved y by +0.01 only.
- Stick on virtual pad 1 did nothing. Explained by the next finding.

### XInput on PC: only controller 0 is polled

- `XInputGetState`/`XInputGetCapabilities` are called from one place, `FUN_006a2940` (fastcall on
  the input manager), always with user index **0**. It converts the state and calls the input
  manager's `vtbl+0x50(controller=0, lx, ly, 0, rx, ry, 0×7, buttons[16], 16)`. Sticks are divided by
  32767.0 (`DAT_00fa1ef0`). Buttons are bytes, 0xFF = pressed: [0..9] = XInput bits 0–9,
  [10..13] = A/B/X/Y (bits 12–15), [14] = LT, [15] = RT. There is also a DirectInput-pad branch,
  likewise for controller 0. Single call site: `0x006a3d4c` (E8 rel32).
- Injection for players 2–4: wrap that call, then call `vtbl+0x50` with controller 1..3 from our
  virtual pads (`redirect/input_pads.cpp`).
- Input manager vtable at 0xfacbdc (RTTI COL at 0xfacbd8). `+0x50` = `FUN_006a3ab0`: the controller
  index is an int (Ghidra mistypes it as float). Per-controller state lives at `this + idx*0x60`
  (sticks, after deadzone `FUN_0069bf40`) and button states at `this + 0x670 + idx*0x80` (8 bytes
  per button: state 1 pressed / 2 held / 3 released).
- The poll's caller `0x6a3d40` (vtable slot +0x44) skips polling unless `this+0x10` is set, which
  clears when the window is unfocused. Patched jnz→jmp so input keeps flowing (host may be unfocused).
- `FUN_006a8b10` (`*(this+0x64)`): when set, the poll reads keyboard/mouse (DirectInput) instead of
  XInput, so `XInputGetState` is never called. Matches Config.xml `IsKBAndMouse` = TRUE (run 13:
  0 XInputGetState calls vs about 145 polls/s). The test copy is set to FALSE so pad 0 drives
  player 1 and menus.
- Runs 11–12: controller-1 stick injection did not move player 2. `player.Jump(1)` did
  (y 5.73 → 8.80 and back).
- Run 14 (IsKBAndMouse FALSE): XInputGetState now polled once per input poll (about 145/s).
  - Virtual pad 0 drives the UI: D-pad down ×2 + A on "SAVE FAILED" → "Do you really want to disable
    saving?" (YES preselected) → A → "Saving has been disabled." → A → gameplay (frame dt back to
    1/30). `lua/ui_continue_no_save.lua` does all three.
  - Pad 0 stick moved player 1 about 12.5 m in 2 s (x 228.6 → 216.0).
  - Controller-1 stick reached player 2: moved about 1.2 m in the first 10 frames, then player 2's
    position became NaN (`-1.#IND`).
  - Fresh player 2 + `camera.SetUpMpAxisCamera()`: no NaN; the stick moved player 2 only about
    1.4 m in 3 s, in steps. The MP axis camera (defaults, or offsets 0,4,-8) ends up inside level
    geometry in the TIE Factory corridor.
  - Interpretation (unverified): stick locomotion for players 2–4 needs a per-player camera (or a
    camera-independent move direction). Next milestone.
  - `game.StartSinglePlayerGame` takes two numbers: `FUN_0074a790(arg1)`, then `FUN_0074b4b0(arg2)`.
    Meaning unknown; called with one arg it errors "bad argument #2".
### Milestone 3 findings (2026-10-25)

- **One camera on PC.** Player component camera getter `FUN_00c35100`: controller index at +0x18;
  returns `*(game+0x90 + idx*4)` only when `idx < 1`, else camera 0. Every player shares camera 0.
  `camera.FocusFit(iViewport, …)` / `EndScriptControl(viewport)` only act when viewport == 0.
  Per-player views are not possible without engine render changes.
- Same stick input on pads 0 and 1 together (run 16): player 1 walked about 9 m, player 2 about
  0.1 m (then about 0.7 m after release). Player-2-specific, not camera.
- `ren.AddToWorld`, `location.MoveToSameSectorAs`, `physics.Activate`, `dmm.Activate` on player 2:
  no change.
- `Mp.SetPlayerController(p2, 1)` (controller 0 to player 2, leaving player 1 without a controller)
  crashed: access violation at 0x86a2c2 (`FUN_0086a2c0`, getter on a null object).
- Engine player spawn: `FUN_00418d70` (level start) → player manager `FUN_00c34480` → spawner
  `FUN_00c3b6a0("Player", actorName)` → `FUN_00410b10`, `FUN_007b7cf0`, `FUN_007b8770(0)`,
  `FUN_00436d60` + `FUN_00799ef0(crc "Player")`, **`FUN_008db8d0(ren, 1)`** (puts the ren into the
  level volumes/zones it is inside). Our `CacheEncounterAsset` path skips the zone step; exposed as
  `coop_enter_zones(ren)` (run 17 tests it).
- `coop_enter_zones(p2)` (FUN_008db8d0): no change in walking (run 17).
- Run 19: controller-1 stick stored steadily in the input manager (raw 0.977, deadzoned 0.964 every
  frame), so the input feed is fine.
- `player.GetLeftStickDir2D(1)` (PlayerComp +0x80, world move direction) = **NaN** while the stick is
  held; slot 0 gives a unit vector. Chain: player update `FUN_007c2200` → `FUN_00c35da0` (reads
  `MOVE_X_AXIS`/`MOVE_Y_AXIS` via input maps for the component's controller) → `FUN_00c35710`
  (stick → world) → **`FUN_00c349b0(matrix, controller)`**, which copies the camera matrix only for
  controller 0 (`test eax,eax; ja skip` at 0xc349b9). Controllers 1–3 used an uninitialised matrix.
  Patch: `85 c0 77 2f` → `31 c0 90 90`, so all controllers use camera 0 (shared camera).
- **Run 21 (patch applied, hands-off): player 2 walks normally.** Stick on controller 1 moved
  player 2 z 70.0 → 71.2 → 73.5 → 75.9 → 78.2 in 40 frames (about 7 m/s, same as player 1) until
  the corridor wall. The single camera follows player 1 only (screenshot), so player 2 goes off-screen.
- Monitors in standby + DXVK FIFO present → game ran at about 0.5 fps (4 input polls / 10 s). The test
  copy's `dxvk.conf` sets `d3d9.presentInterval = 0`; the game keeps its own 30 fps cap.
- PlayerComp vtable 0x1069e24 (`+0x30` SetControllerIndex 0xc34ae0). Leftover network-MP UI classes:
  `UINetworkPlayerCharsAvailableEventSW`, `UINetworkPlayerDecksAvailableEventSW`.
### Milestone 4/5 (2026-10-25)

- Architecture changed to one game per player PC (owner requirement: own perspective on own PC);
  see `tfu-coop/docs/plan-coop.md`.
- Single-instance lock: AWL `CreateMutexW` (`{4C5F61A1-2E84-4B12-B3C8-7B661DD86963}`, config
  `OneInstance` True). With `COOP_INSTANCE=n` our DLL renames it `…_coop<n>` and uses
  `coop-userdata-<n>`. Two instances booted to the Lua console side by side at 13:15.
- Two-instance link test at 14:14: both instances never ticked (0 input polls), then both logged
  `VK_ERROR_DEVICE_LOST` at 14:16, crashed in DXVK d3d9.dll (offset 0x24676), and Windows logged an
  IOMMU fault on device 0x300 at 14:17:15. Afterwards the GPU showed `Status Error /
  CM_PROB_FAILED_ADD` and Vulkan instance creation failed. Cause unknown (GPU hang from two DXVK
  instances? pre-existing driver state?). Needs a PC restart before further tests; then re-test
  one instance, then two.
- After PC restart (14:23): GPU OK; single instance and two staggered instances (45 s apart) booted
  with 0 DXVK errors and the GPU stayed OK.
- **Winsock on the game's main thread crashes the game**: EIP 0 via `KiUserApcDispatcher` →
  KERNELBASE I/O completion dispatch. The main thread has I/O completion APCs queued with a null routine
  (ScumLib async I/O on the stack); any alertable wait (Winsock calls) dispatches them. Standalone
  32-bit Winsock test works. Fix: all socket work on a worker thread; the main thread only copies data
  and uses non-alertable waits (`redirect/net.cpp`). Rule: never make alertable calls on the main thread.
- Engine Lua globals are protected: assigning a new global errors "globals are protected. Use
  CreateGlobal()". Use `rawset(_G, …)`.
- Two-instance link (rounds 3–5): the link starts in both; B places A's puppet exactly on A's pose
  (drift 0.00). But instance A, in the background, runs at about 0.4 fps (4 input polls / 10 s), so its
  player never moved. The window minimizes itself on focus loss (not via the ShowWindow import);
  a WH_CBT hook now cancels it (log: "minimize of window … cancelled (cmd 6)"), but A stays slow
  even unminimized. (Resolved 2026-10-26, below.)
- Background slowdown, root cause (2026-10-26). `GetForegroundWindow` is not it: its 3 call sites
  (0x69da0f, 0x6a3d96, 0x6a3ec5) only show/hide the cursor and gate mouse input. A stack sampler
  (`COOP_SAMPLE=1`, `redirect/crash_log.cpp`, writes `<root>\sample.log`) caught A's main thread in
  `Sleep(100)` at 0xb5de18. The WM_SIZE handler 0xbab410 → FUN_00b984a0 sets renderer+0x1a4 ("device
  lost"). FUN_00b5dd90 clears it only after `TestCooperativeLevel` returns D3DERR_DEVICENOTRESET, then
  Resets. DXVK reports that only when it fakes loss on focus loss (its built-in SWTFU profile:
  `d3d9.deviceLossOnFocusLoss = True`), which our dxvk.conf turns off. So a resize while unfocused left
  the game "lost" forever: Sleep(100) and no rendering each frame.
  Fix: patch 0xb5ddab `3d 69 08 76 88 74 04` → `3d 68 08 76 88 75 04` (reset unless DEVICELOST).
- Also seen in the samples: while a load-screen movie plays (flag 0x01240452, set in the load loop
  FUN_00419080), asset loading does `LECSleep(30)` per asset (0x4060b9, 0xc85810). This is normal.
- Two-instance link, run 9 (2026-10-26, automated `tools/m5-run.sh`): both instances ran at full rate,
  with the background one at 301–603 input polls / 10 s (was 4). Packet age 0–31 ms. A walked about
  10 m (forward 2 s, turn 1 s). B's puppet tracked it with drift ≤ 0.47 m during the walk and 0.22 m
  at rest; correction snaps 7→15. Run 8 was the same with a 0.53 m peak. **Milestone 5 gate (puppet
  within 0.5 m): met in run 9, marginal (0.53 m peak) in run 8.**
- With full speed in the background, the dt probe in `m2-run.sh` no longer sees the SAVE FAILED dialog
  freeze the world, so run 8 left both instances on the dialog and pad input went to the menu. The
  script now answers the dialog even when no freeze is seen.
- Save dialog source: UI binding `storageDisabled` (symbol at 0x012a2c88), derived from the save
  profile `DAT_012f1228`. 0/-8 = no slot, which is the case after skipping the menu. `load_level.lua`
  now calls `game.LoadMasterData()` and `game.StartSinglePlayerGame(0)` first (run 14 tests it).

## Milestone 6: host-authoritative enemies (2026-10-26)

Observed:
- Lua API (from the LuaRegister table, `re/lua_bindings.py`): `ai.GetAllRens`, `ai.GetAllInRadius`,
  `ai.IsAlive`, `ai.DisableAllBrain`, `game.GetHealth`/`OverrideHealth`/`Kill`/`IsDead`,
  `location.GetPosition`/`SetPosition`/`GetRotationYawDegrees`/`SetRotationYPRDegrees`/`Teleport`.
- TieFactory_Act1 hangar: 6 AI, health 14.1 each. `ren.GetName` returns "" for them, and they all share
  one path. Ren handles (`coop_ren_id`, the 32-bit handle as hex) were different between instances in
  one run and the same in later runs, so they cannot be used as network IDs.
- Patrolling AIs on the two instances drift up to 14 m apart within a minute, so pairing by current
  position failed (2 of 5 paired). Pairing by **spawn point** works: `lua/m6_spawn_record.lua` records
  each AI's first position from level load. Spawn points matched within 0.06 m (5/5 paired, 3 runs).
- Design: UDP "world" packets (`'TFUW'`, up to 1200 bytes of text, latest wins, `redirect/net.cpp`).
  The host sends AIs within 60 m of either player at 10 Hz:
  `id x y z yaw hp alive sx sy sz;`. It also re-sends each death 20 times, because dead AIs drop out of
  `GetAllInRadius`. The client calls `ai.DisableAllBrain` every second and pairs by spawn point. It snaps
  position when off by more than 0.3 m, and copies heading and health; on alive = 0 it calls `game.Kill`.
- `tools/m6-run.sh` runs (monitors off, windows 640x480; runs m6r7–m6r9):
  - Scripted kill (`game.Kill` on A): **gate passed** 3/3, applied on B 27–47 ms later.
  - Lightsaber fight on A (bot teleports next to the nearest trooper and spams X): trooper 14.1 → 0 on A,
    and B's copy is dead (health 0, not alive). **Passed** in m6r9.
  - Client error before snapping: 0.21–0.60 m.
- Client attacking (m6r9, manual follow-up): bot on B attacked trooper F5F00A88. It died on both, but
  all 6 hangar troopers were dead on both by then, and the next wave (15 AIs, health 120–300) had
  spawned on **both** instances. Which hits killed the host's trooper (B's puppet or something else)
  was not isolated.
- Harness bugs found and fixed: `m2-run.sh` matched old log lines when a run label was reused (the run
  then sat in TEM_JediTemple_A with the player spawning 114 m up). Markers are now unique per run.
  The save dialog is now detected by input poll rate: 5k–90k / 10 s while it is up, 300 in the level.

- Client hits (m6r10–m6r11): client copies now keep 100,000 health locally (`game.SetMaxHealth` +
  `OverrideHealth`) and die only on the host's alive = 0. The host's health is kept in `coop_m6_host_hp`.
  Test: host AIs set to 1,000,000 health, then B's bot attacks a trooper. In m6r11 B's copy died only when
  the host reported the death, and no "local death without host death" warnings appeared. **Passed.**
  - Observed: B's puppet on A killed the host trooper **despite 1,000,000 health**, so some attacks kill
    outright regardless of health (which move is unknown).
  - The test's `OverrideHealth` also revived three already-dead host troopers (alive again, health
    1,000,000), so `OverrideHealth` on a dead AI brings it back.
- CPU (m6r11, host instance, monitors off): main thread 100% of one core, every other thread under 3%.
  The stack sampler put 138 of 150 main-thread samples in `win32u` `PeekMessage`, called from the main
  loop at 0x5C55C9 (through our PeekMessageA hook). So the frame limiter busy-waits for the next
  30 fps frame; the load is not real work. Unknown: how much real work per frame (not measured).

Known gaps (not yet built):
- Client copies have their brains off, so they slide (snaps) instead of walking with animation. Try
  `ai.DisableBrainExceptMoveSet`, or drive their move set.
- Level scripts still run on the client (the next wave spawned on both). Client-side spawns need
  suppressing or pairing (milestone 7, story sync).
- At most ~17 enemies fit in one 1200-byte packet; more need several packets.

## Milestone 7: story and level flow (2026-10-26)

Player death (observed):
- In single player a death runs the death handler FUN_007c2200, which calls the respawn
  FUN_007b9330(isMultiplayer = FUN_00476eb0(), ...). With 0 the respawn restores the checkpoint or
  reloads the whole world (FUN_004114a0). A fall in m7w1 reloaded both instances about 2 s later.
- With 1 (the path `Mp.RequestRespawn` uses) the checkpoint block is skipped. `coop_mp_respawn(1)` patches
  0x7c2865 `8b 44 24 20` → `b0 01 90 90`. `lua/m7_respawn.lua` moves the respawned player 1.5 m from its
  partner.
- Client enemies have their brains off, so the client player is only hit as its puppet on the host. The
  host owns that health: it sends `P hp deaths;`. The client dies only when the death count goes up, and
  ignores one within 5 s of its own death (a fall: the puppet follows it into the pit).
- `tools/m7-death-run.sh` (m7d2): three deaths of B (scripted kill, fall, kill of its puppet on A). Each
  respawned in ~3.1 s at full health (4000) next to A's player, with no world reload on either instance.
  B's health followed its puppet (1500). **Passed.**

Level flow (observed):
- The host sends `W <world>;`. A client whose world differs for 20 packets (~2 s) calls `ronin.LoadWorld`
  from inside `ronin.Update`. A world change resets enemy pairings and spawn points (handles are reused),
  and `m5_link.lua` re-creates the puppet.
- `tools/m7-level-run.sh` (m7l1): host loaded Raxusprime_Act1_A, B followed on its own, both had a
  puppet again, link drift 0.01 m. **Passed.** No AI was in range at the Raxus start, so enemy sync there
  is untested.
- The save dialog's poll-rate signature also appears during loading screens; the answerer pressed 5–6
  times per instance with no visible harm.

Cinematics (observed):
- Mostly in-engine EDLs, plus Bink movies in `FMV/`. `edl.List()` names look like
  `vnTFCF010_PlayerIntro.edl.xml`, while `edl.Go` wants the name without extension.
  `edl.StopEdl()` also stops EDLs that `edl.IsEdlSkippable()` reports as not skippable.
- `lua/m7_cine.lua` wraps `edl.Go/go/RunEdl`. The host sends `C counter name playing;`. The client runs a
  new host cinematic unless it is already in it, and stops its own 15 frames after the host's ended.
- `tools/m7-cine-run.sh` (m7c2): the client played the host's cinematic (~60 ms later) and stopped it
  ~0.5 s after the host. **Passed.**

Waves (observed, m7w2–m7w5):
- Killing all 6 hangar troopers does not spawn the next wave; standing at (148, 11) does (m7w3, m7w5).
  The same move also once spawned the wave on the client only (m7w4), so triggers are not reliable across
  instances.
- `ren.PathToStringDebug(ren.PathInside(r))` ends in `Spawn_<n>_<type>`, e.g. `Spawn_6_stormTrooper`,
  `Spawn_11_protoRebelSaboteur` (rebel allies, who fight the troopers on the host), and
  `Spawn_8_EWEBHeavyRepeaterSingle`. Between instances only 2 of ~20 living AIs had the same name, so
  the level scripts' spawns diverge in numbering and type.
- `Mp.CacheEncounterAsset("<type>.actor.xml")` spawns any of these three types (manual test on B).
- Deaths of AIs out of range were never sent (5 of 6 applied, m7w2). Now the host scans all AIs once a
  second and sends known deaths near a player round-robin: 6 of 6 applied (m7w3).
- Rule now: only the host's AIs count on the client. Own AIs unclaimed for 3 s are hidden (suspended +
  invisible, not killed), shown again when a host AI pairs with them by spawn point. A copy is spawned by
  type when nothing pairs.
- m7w5: after the wave, visible living AIs were equal on both (12/12, then 11/11), 15/15 paired, error
  ≤ 0.29 m. The client hid 14 own AIs and showed 15 again when paired. No copies were needed in that run.

Corrections:
- m7w1 "new wave paired" was the hangar troopers re-created by a world reload (the players had fallen),
  not a new wave.
- The CPU sampler (`thread-cpu.ps1`) runs on the host instance only.

Open:
- Copy-by-type spawning inside the sync (only tested by a manual spawn so far).
- Level scripts on the client still run: anything they start that the host does not (a trigger only the
  client's player reaches) is not reconciled.
- Checkpoints and saves on the host only: saving is disabled in the test setup (SAVE FAILED dialog), so
  untested.
- Bink movies between levels play on each PC; not synced.

## Milestone 8: three and four players (2026-10-26)

Observed:
- Spawning two puppets in one frame on 3 instances crashed all three at 0x86a5f6 (read 0x13C). Alone,
  `Mp.FixupPlayerSpawn(r, 3)` (controller 2) also crashed. Without the fixup, the per-frame update crashed
  on the null profile at PlayerComp+0x2dd4 (0x7b717c). Sharing the profile then crashed at 0x885421.
- Cause: the per-controller object at game(*0x01240440)+0xac exists only for controller 0. `coop_peek`
  showed `+0xac 26823F30, +0xb0 01000000 (flag bytes), +0xb4 0`. Four player-code sites (0x7aba7c,
  0x7aeee5, 0x7bdd8d, 0x7f0dc3) read `[game + controller*4 + 0xac]` with no bounds check, then use entry +
  0x100. Controller 1 survived only by reading readable garbage (0x01000100). Two other sites check
  `index < 1`, and 0x83b327 null-checks the entry.
- Fix: those four loads go through stubs that return the real entry for controller 0 and a zeroed 64 KB
  dummy object otherwise. Afterwards `FixupPlayerSpawn` works for controllers 2 and 3, and puppets in
  slots 2 and 3 walk on virtual pads 2 and 3 (m8_slots_test.lua: ~15 m and ~6 m in 3 s).
- Network: the host (id 0) relays player packets between clients. Remote player p uses local controller
  (p − myId) mod 4. Slots can have gaps (client 1 has the host in slot 3), with no problem seen.
- `tools/m8-run.sh` (m8r2, 3 instances on one PC):
  - Every instance tracked every other player (drift ≤ 0.43 m).
  - The host's kill was applied on both clients.
  - Client 2's walk was tracked by the host and client 1 within 0.07 m, through the host's relay. This
    needed a manual answer to client 2's save dialog.
- Save dialog signature: 60 fps (~600 polls / 10 s) with the monitors on, uncapped (5k–90k) with them
  off; the level runs at 30 fps (~300). The detector threshold is now 450.
- The m8r2 run with 3 instances caused no GPU problems (instances 45 s apart).

- Four players (m8r5, 4 instances on one PC, monitors on, fresh run):
  - Every instance tracked the other three players.
  - Client 3's walk (~9.6 m) was tracked by the other three within 0.11–0.46 m.
  - The host's kill was applied on all three clients.
  - Each save dialog was detected and answered once.
  - The GPU stayed OK. **Passed.**
- Save dialog detection, take 3: with the monitors on, the intro cinematic and the dialog both run at
  60 fps, so the poll rate answered the cinematic (m8r3). The dialog keeps calling `ronin.Update` with
  dt = 0 (all 4 instances, m8r4). Now: a heartbeat counts calls with dt > 0, and a count that does not
  move between two reads, while the console still answers, means the dialog (`tools/lib-dialog.sh`).

- Slot-2 attacks (m8r7, 3 instances): client 2's player was moved next to host trooper F5E60AAA and
  attacked (X on pad 0). On the host, its puppet (slot 2, controller 2, dummy per-controller object)
  killed the trooper (14.1 → 0; nobody else within ~45 m), and both clients applied the death.
- Config.xml was found empty (0 bytes) after a forced stop: the game writes it on exit. The game then ran
  with defaults (1024x768, keyboard/mouse mode ignoring XInput), so the save dialog could not be answered
  (m8r6). `launch-test.ps1` now restores an empty config from `<game>\Config.template.xml`, and
  `stop-test.ps1` asks the windows to close first (all three closed cleanly).

Unknown / open:
- What the controller-0 object is (HUD/targeting?). Puppets get a zeroed one; basic attacks work
  (m8r7); Force powers by puppets in slots 2 and 3 are untested.
- The dummy object is shared by every puppet.

## Milestone 9: over the internet (2026-10-26, in progress)

Observed:
- Simulated network conditions (`COOP_NET_DELAY_MS/JITTER_MS/LOSS_PCT`, net.cpp). m6net1: 75 ms one-way
  each side (150 ms round trip), 20 ms jitter, 5% loss. The kill, fight and client-hit checks pass. Walk
  tracking stays within 0.43 m of the 75 ms-old remote pose.
- Autorun: `coop.ini` (per instance, or `<game>\coop\coop.ini`) with enabled=1 makes the DLL queue
  `coop\lua\coop_autorun.lua` until the engine's Lua is up. The autorun then starts the modules once a
  level with a player runs. m9a2: co-op started on both without harness files, and the host kill reached
  the client.
- Real start path: New Game from the main menu (pad 0: left, A, slot, A, A) → Kashyyyk_Prologue as
  Vader. **No SAVE FAILED dialog**: that dialog comes only from our level loader skipping the menu's
  save-slot setup. The intro EDL makes no Lua calls (the console queue waits ~2.5 min).
- Each level has its own player actor (69 `player*.actor.xml` assets; the prologue's is
  `playerDarthVader`). A `player.actor.xml` puppet never took its slot in the prologue, and the link
  retried every 2 s, leaking rens. m5_link now finds the local player's actor (`ren.IsOfActorType`)
  and caps spawn tries at 3.
- m9m3: both instances started a campaign from the menu; autorun linked them, Vader puppets tracked
  within 0.05 m, and the host kill reached the client.
- The menu can ignore input for a while after Lua is up (m9m2); the test resends the sequence.
- A relaunch right after killing the previous instance exits at once (the AWL single-instance check sees
  the dying process).

Not yet tested: two real PCs over the internet, port forwarding / VPN, loading saves with clients
connected, checkpoints mid-level.

Security review of the DLL (fresh-context reviewer, 2026-10-26):
- Verdict: fine on a LAN or among trusted peers, not ready for the internet.
- High:
  - Anyone could take over a client slot: the host re-learned addresses from any packet, and a seq jump
    locked a slot out.
  - The host could be made to send an unbounded stream to a forged address: peers never expired, and
    every packet was relayed.
  - Clients accepted packets from any address.
  - Medium-high: network floats (NaN/Inf) went into the engine.
- Also found:
  - WSAECONNRESET / WSAEMSGSIZE ended the read loop.
  - An overlong coop.ini value could leave a string unterminated.
  - `coop_fill_controllers` was latent-dangerous (+0xb0 holds flags).
  - The respawn patch set only al.
- Patches checked correct against the exe bytes: device reset, camera, poll gate, the four controller
  stubs, and respawn. Threading: no alertable or Winsock calls on the main thread.
- Fixed (packet v3):
  - A session-key hash in every packet (coop.ini `session_key`).
  - Clients accept only the host's address.
  - The host binds a slot to its address while it was heard in the last 5 s, expires silent peers, relays
    only fresh packets, and rejects seq jumps over 100000.
  - Floats must be finite and in range.
  - The read loop continues past errors, SIO_UDP_CONNRESET is off, and reads are bounded.
  - The ini value is terminated; `coop_fill_controllers` is removed; the respawn patch is `push 1; pop eax`.
- Tested with an empty key (`tools/spoof_test.py` against a running host): 300 wrong-key packets, 300
  right-key packets from another address, and 300 NaN packets claiming player 1 were all ignored. The
  host's view of player 1 stayed at the real position, and the client stayed paired.
- Non-empty key (m9a5, 2026-10-27, `session_key=testkey` on both):
  - The autorun linked both, and the host kill reached the client.
  - With the same key, wrong-key, right-key-from-another-address and NaN floods were all ignored. The host
    tracked the real player 1, and the client stayed paired (5/5).
  - m9a4's failure was the harness: heartbeat probes queued during loading ran in one burst and looked
    frozen, and the watch window ran out while loading. Fixed in `tools/lib-dialog.sh`; m9a5 answered each
    dialog once.

## Enemy copies walking instead of sliding (2026-10-27, not adopted)

Observed:
- `ai.MakeMoveTo` does not move an AI whose brain is off.
- `mobility.RequestPathToPosition` with the brain off except the move set
  (`ai.DisableBrainExceptMoveSet`) walks the AI, at ~0.5 m/s whatever the speed argument (1, 3, 6), after
  ~1.3 s. One trooper (F5E60AAA) never moved.
- `ai.GetCurrentMovesetState` returns "" in the retail build (debug-only), so animation states cannot be
  mirrored that way.
- m6walk1 (walking for gaps 0.3–3 m, snapping beyond):
  - Copies lagged up to 2.07 m (snaps 28, walk requests 231).
  - With the move set on, a copy reacted to the client's hits and died locally without a host death
    (client-hit check failed).
- Kept as an option (`COOP_ENEMY_WALK = true`), off by default. m6walk2 (default): all three checks
  pass, max error 0.20 m.

Open: copies still slide. Ideas: drive a walk animation directly (animation chores), or move copies
smoothly every frame with interpolation instead of snapping.

## Level drift check and on-screen status (2026-10-27)

Observed:
- World diff (`lua/m11_world_dump.lua`, `tools/m11_world_diff.py`): ~5,670 non-player, non-AI rens per
  instance in TieFactory_Act1. Most share keys (only their markerset path); ~460 have no path at all.
  - Baseline noise: TIE fighters on splines, spline markers, `_ActorCacheForAssetPacks_*` and Breakable
    names carrying ren ids.
  - After the host cleared the hangar and both players stood at (148, 11) (wave trigger), no named object
    differed beyond the baseline. Only unnamed objects did (A −30, B +104: likely props/weapons of spawned
    AIs and effects).
  - So no door or console drift in this section. Not checked: later sections, objectives.
- On-screen text: `ui.RewardMessage("text")` shows plain text centred in the game's font.
  `ui.MessagePlayer` and `ui.ShowSubtitle` with plain text showed nothing (they look up text ids).
  `ui.HideSubtitle()` hid the game's own hint line.
- m5_link shows status: hosting / connecting, "player N joined" / "connected to host",
  "lost connection" (5 s silence), "rejoined". m12a: all seen, including the lost connection after
  closing the client.

## Force powers for puppets (2026-10-27)

Observed (3 instances, frozen target troopers on the host via `ai.DisableBrain`):
- B (0x2000) from the client's pad, relayed to its puppet on the host, did nothing for slots 1 and 2. The
  host's own B threw a frozen trooper 11 m (control). Lightsaber attacks (X) by puppets do work.
- `player.ForcePush(1)` on the host made the slot-1 puppet push (trooper thrown 8 m), so the power itself
  works.
- Pad Force Push needs both the per-controller object of controller 0 (puppets got the zeroed dummy) and a
  profile with the powers unlocked (puppets got the controller-1 profile). Either alone did nothing; both
  together: trooper thrown 13 m.
- Force energy stays per character: the puppet's push took it 100 → 74; the host stayed at 100.
- Sharing controller 0's object on clients made puppets act on the local player's state: both players
  drifted ~2.8 m and puppets killed enemy copies locally (m8r10). So it is enabled on the host only, where
  puppets matter; client puppets stay cosmetic.
- m8r11: all three checks pass, no local-death warnings. 20 s idle with all players next to troopers on the
  host: no unexpected kills. Player 3's pad push from client 2: trooper thrown 18 m.

Untested: lightning and repulse, and whether host-side sharing affects the host's own targeting in combat.

### Force Grip for puppets (2026-10-27, not working)

Button map from `ControllerConfig.txt`: Saber BUTTON_1 (X), Jump 2, Push 3 (B), Lightning 4 (Y), Dash 5,
LockOn 6, Block 7, Grip 8. On the pad, Grip is the right trigger (confirmed by test).

Observed (2 instances, Kashyyyk prologue from the menu, m9m5):
- The trigger reaches the host. The pad poll (FUN_006a2940) puts LT and RT into button slots 14 and 15
  (0..255). With the client holding RT, `coop_input_raw(1)` on the host showed slot 15 = FF for the whole
  hold, so the network path carries it.
- The host's own RT with nothing in view: Force energy 150 → 120, then back to 150. Something was
  gripped, but `game.IsGripped` was false for every AI (probably a physics object).
- The puppet holding RT in the same place: energy stayed 150, nothing gripped. `game.SetForceGripRank(1, 1)`
  did not change this.
- `tools/m13-power-run.sh grip "0, 0,0,0,0, 0, 255" 2500 1` (target trooper with its brain off, player put
  2 m away facing it): neither the host nor the puppet lifted it. So that setup does not test grip; the
  host's earlier successful lift (y 13.7 → 15.3) was on a live trooper in view of the camera.

Derived (not verified):
- Grip state lives in a global ForceGripManager (`*0x012f0fa4`, created in FUN_0041e2b0), a map from
  ren to a grip entry (+0x4c = slot index, -1 = none). Lookup: FUN_0053a0a0 / FUN_0053a140.
  `player.StopForceGrip` (0x833ae0) and `player.IsGrippingObject` (0x833b70) take a controller number.
- Grip picks its target by aim (strings `AutoLockDefAsset`, `Force Grip cam ...`), most likely from
  controller 0's camera, which puppets never see. Next: decompile the `PlayerInputGripPressed` handler and
  the auto-lock target search to find the controller-0-only step.

## Derived (not yet verified)

- `libScumEuphoria` suggests NaturalMotion Euphoria; `Scum*` names suggest the LucasArts in-house
  engine layer. Engine name: unknown until a source confirms it.
- `AWL.net.dll` + `SlimDX.dll` are .NET, probably the Aspyr launcher / wrapper layer. Not checked.
- No multiplayer mode on PC known to me, so no replication to reuse. Unverified.

## Unknowns

- Engine name and scripting system (if any).
- Whether any co-op or multiplayer mod for TFU exists.
- Whether level/script data in `.pcd/.pcp/.lp` is documented or has tools.
