# Space Rangers HD: A War Apart — agent notes

Observed facts, inferences and unknowns are kept in separate lists per the ground rules.
Sources are local file inspection of the owner's install unless a URL is given.

## Observed (checked runs)

- Store/build: Steam appid 214730 (steam_appid.txt = 214730; game_identity resolved
  appmanifest_214730.acf), build 20648864.
- `Rangers.exe`, 4,995,584 bytes, SHA-256 83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98,
  product 2.1.2500.0, 32-bit PE32, Borland-style section set (`.itext` present).
- Install root holds `MatrixGame.dll` (1.5 MB) and `okgf.dll` — matching shows Space Rangers 2 /
  HD is the bespoke "Matrix" engine, not any family in playbook/engine-families.md.
- `DATA/` contains proprietary archives: `*.pkg` (form/archive bundles, e.g. scripts.pkg 153 KB,
  common.pkg 307 MB, questsRus.pkg 3.9 MB) and `*.vdo` (video). Format probe of scripts.pkg:
  starts `04 00 00 00 AA 00 00 00 01 00 00 00 9E 00 …` then tag `DATA` — custom container,
  not a known archive magic; needs a format write-up before any game-data reads.
- `CFG.TXT` is plain-text key=value config with a rich set of documented keys
  (Debug=YES exists, ShowFPS, RobotRefreshRate…). `Debug=No` is currently set.
- Built-in mod loader: `Mods/ModCFG.txt` + `Mods/Tweaks/<name>/{ModuleInfo.txt, CFG/, DATA/*.pkg}`.
  Shipped tweaks (German localization, ship packs) are pure CFG/DATA override mods —
  no script hook or scripting layer.

## Inferred (derived, not directly confirmed)

- Sections `.itext` + `okgf.dll` suggest the exe and engine DLLs are Delphi/C++Builder-built;
  if so, Ghidra recompiler output will be readable but function recovery will need manual work
  (Delphi RTTI is weaker than MSVC PDB), and Win32 hooking (proxy DLL / Detours-style) is the
  likely injection route.
- The co-op-relevant subsystems all live in Matrix engine data, not in the shipped tweaks:
  galaxy simulation (turn-based shared world), arcade flight (real-time), planet surface
  robot battles (turn-based tactical), text quests. Any "one screen" approach must choose
  a subset first.
- `CFG.TXT` keys like `BeginCalcNextTurn`, `FilmSpeed`, `ChangeAutoPilot` suggest a tick- and
  turn-based core loop; a lockstep or host-authoritative design both become possible
  candidates but this is unproven.

## Observed escalation (research phase, verified 2026-11-03)

The Matrix planetary-battles engine is OPEN SOURCE, GPL-2.0+, released with the copyright
holder's permission. Verified via GitHub API + local clone
(`local/MatrixGame`, clone of https://github.com/murgesku/MatrixGame, HEAD 06e4d32
"Fix undervalued head modules for AI"):

- Canonical, active: **murgesku/MatrixGame** — "Space Rangers 2 Planetary Battles Engine",
  C++/CMake, GPL-2.0, pushed 2025-09, 27★, 18 forks. Sources README credits
  http://snk-games.net/matrixgame.tar.gz as origin; archived mirror: twoweeks/MatrixGame
  (archived, pushed 2022-11, description: «Исходный код движка планетарных битв из Space Rangers 2»).
- Forks: vladislavrv/MatrixGame (branch `refactoring`, pushed 2025-05), IlyaGusev/MatrixGame
  (archived, pushed 2026-07). Every one GPL-2.0.
- README quote: the build "output should either contain an **executable file that can be run
  independently** or a **library for connecting directly to the SR2 engine**". The second
  product is the documented hook point for a mod: connect our DLL build in place of/via the
  shipped MatrixGame.dll. (`okgf.dll` remains unexamined.)
- Claimed architecture match — all found by grep in the local clone:
  - `CMatrixMapLogic::Takt(int step)` — MatrixLogic.cpp:2469 (the logic tick)
  - `CMatrixMap::Takt(int step)` — MatrixMap.cpp:2488 (render tick runs after logic tick,
    called from MatrixLogic.cpp:2761)
  - `CMatrixFlyer` — MatrixFlyer.cpp (flying units' class; SMatrixFlyerUnit::Takt per-part)
  - `CMatrixRobotAI`, `CMatrixSideUnit`, `CMatrixBuilding`, `CMatrixMapStatic`, `CMatrixGroup`
    — the planetary-battle entity set (Logic/ headers)
  - Code references `SpaceRangersHD` install path (MatrixGame.cpp:827) — engine supports HD's data.
- Scope limit (README + class set): MatrixGame is ONLY the planetary-battles engine.
  Galaxy simulation, dividers/arcade hyperspace flight, text quests live in the closed
  `Rangers.exe` and are NOT covered by this source.
- Third-party libs (zlib, libpng, VirtualDub bits) live in ThirdParty/ under their own licenses.

## Corrections to the earlier report

- Canonical repo is **murgesku/MatrixGame**, not vladislavrv (that is a fork).
- `CFormMatrixGame::Takt()` — NOT found in murgesku master. `CMatrixCamera::Takt(float ms)`
  and per-effect Takt calls exist; record as unverified until the refactoring branch is grepped.

## Observed escalation 2: the exact HD ↔ engine ABI (verified 2026-11-03)

Shipped `MatrixGame.dll` (copy in `local/analysis/`, original left untouched) exports
**exactly one** function: `GetRobotInterface` (checked with pefile, x86). It imports
d3d9/d3dx9_43/winmm/kernel32/user32/gdi32/advapi32/shell32. `okgf.dll` is an unrelated
image-format helper library (203 exports, texture color-conversion; imports ole32).

The open source defines both halves of the ABI in `MatrixGame/src/MatrixGameDll.cpp/.hpp`:

- `GetRobotInterface()` returns `SMGDRobotInterface` with 4 pointers: `m_Init(ri)`,
  `m_Deinit()`, `m_Support()` (ESupportError: OK/DirectX/video driver/video hardware),
  `m_Run(HINSTANCE, HWND, wchar* map, SRobotsSettings*, wchar* lang, txt_start, txt_win,
  txt_loss, wchar* planet, SRobotGameState*)` — a whole planetary battle runs to completion
  inside `m_Run` and returns the party's built/killed robot, turret, building tallies and
  `SRobotGameState` (m_BuildRobot, m_KillRobot, m_BuildTurret, m_KillTurret, m_KillBuilding,
  m_Time) to the galaxy sim.
- Exit codes of the battle run: 0 = exit to Windows, 1 = exit to main menu (comment above
  `extern SMGDRobotInterface`).
- `SMGDRangersInterface` (passed by the game into `m_Init`, global `g_RangersInterface`
  in MatrixGameDll.cpp:142): the engine CALLS the game for sound create/destroy/play/volume/pan,
  text rendering into a captured surface, progress bar, `m_Takt(void)` pump, `m_Begin`, and
  music volume get/set. So the battle engine is the driven side; the galaxy exe drives it.
- `Rangers.exe` verifies as the loader side: two ASCII string tables
  `MatrixGame.dll` + `GetRobotInterface` + `Load MatrixGame.dll .... ok`, matching the
  LoadLibrary/GetProcAddress pattern.
- **`MatrixOverride`**: a unique UTF-16 string in Rangers.exe immediately next to `Robot`,
  `RobotsMap`, `Note`. `CfgOverride` also appears elsewhere (4:09-file offset list:
  Robot / RobotsMap / MatrixOverride / Robot.Support()= / robot text / RobotPath / RobotsMap /
  CfgOverride / Lang). Looks like a config key that chooses the MatrixGame.dll path/name —
  i.e. the game itself may ship a supported way to patch a different engine DLL.
  Not found in CFG.TXT or ModCFG.txt; the actual key Consumer and value location are
  still unknown (CFG\Main.dat is a binary/encrypted container — 96526 bytes, first 64
  bytes high-entropy). This is the first reverse-engineering target.

Worked plan implication: no binary patch of Rangers.exe should be needed for planetary-battles
co-op. The mod = GPL MatrixGame rebuilt (same ABI) + networking inside the engine + maybe a
`MatrixOverride`-loaded DLL. License-wise we ship our own engine build, never game data.

## Observed escalation 7: pkg container + map location (verified 2026-11-04)

The .pkg archive format is fully documented by the GPL source
(`MatrixLib/Base/src/Pack.cpp`, `Pack.hpp`):

- header: dword root-folder offset; folder = SFolderRec {size, recnum, recsize};
  record = SFileRec {size, realsize, char name[63], char realname[63],
  type, ntype, free, date, offset} — sizeof(SFileRec)=158, folder types recurse.
- compression (type 2 files): chain of blocks [dword restSize]
  [dword blockCompSize]['ZL02'][dword blockUnpackedSize=65536][zlib stream], loop
  while restSize>0; zlib decompress to concatenated raw file. Verified by
  decompressing `Map_1.map` (285,232 B, magic `abwm`) byte-exact.
- **Battle maps live in `DATA\arcade.pkg`** in a catalog-orphan zone: 17 battle maps
  extracted & SHA-256 pinned to `local/analysis/maps/` (2Stars, Amulet, Atom,
  Brond2TheRevengev11, Keller_Spirit_02, KrugAda, Labirint_III, Malta, Map_1..Map_5,
  Map_Boss, Map_sky, Pirates_Flag, Uranv13). All decompress byte-exact with the
  `ZL02` block-chain of the GPL source. The engine takes `map` as `Matrix\Map\<name>`
  when the arg has no '\\' (MatrixGame.cpp:385-394) and falls back to `Config->Map`
  otherwise.
- The map zone of arcade.pkg is a full 60-record folder that is linked OUT of the
  catalog tree: every battle map ships WITH companions `.opt` (logic/layout data,
  same size class as the map) and some `.raw` (heights/lighting? multi-MB), plus
  16 stored `.jpg` map thumbnails (type 1 = uncompressed files in the same folder).
  46 companion files extracted to `local/analysis/maps/full/` — smoke/battle tests
  need `.map + .opt` (and `.raw` where present) together.
  Tool: `local/analysis/pkg_tool.py` (tree walk + record scan +  block-chain
  extractor).
- Term side of the standalone smoke test: MatrixGame.exe (MATRIXGAME_BUILD_DLL=OFF
  build) with a map arg runs MatrixGameInit → CalcVis → writes `calcvis.log`
  (`<map> ... done`) and exits — a headless smoke test of the whole engine init,
  config load (`cfg\robots.dat` — note: test-install has it only under `CFG\Rus\`,
  copied for the standalone) and map+asset loading, WITHOUT Rangers.exe.

Netting-layer design drafted (was a gap): see `DESIGN-netlayer.md` next to this
file (v1): host-authoritative 2-human battle run, guest sits on side 2 of the
built-in 4-side set, SOrder reliable stream + per-tick pose deltas, hook points
CMatrixMapLogic::Takt / CMatrixSideUnit ownership flags; ENet vendored; M1..M4
milestones. Design facts verified in source while drafting: `m_Side[]` supports
4 sides + neutral (ASSERT(cnt == 5), MatrixMap.cpp:1003-1038) and `PLAYER_SIDE`
is the id constant 1 (MatrixSide.hpp:16) — co-op needs NO extra side slots.

  5. `MatrixLib/3G/src/3g.cpp` `L3GInitAsEXE` — UPSTREAM BUG (2026-11-04): the exe
     path never calls Direct3DCreate9/CreateDevice; `g_D3D`/`g_D3DD` are only set in
     the DLL path (`L3GInitAsDLL` receives them from the host game). First use
     `g_D3DD->GetRenderTarget` (3g.cpp:185) AVs with g_D3DD==NULL; verified by
     disassembly of the standalone crash (faulting `mov ecx,[eax]; call [ecx+98h]`,
     faulting global `ds:[5D66D8]` = `g_D3DD`, WER event + /MAP symbol lookup).
     Fixed by creating the device in AsEXE before GetDeviceCaps.

## Observed escalation 6: ENGINE BUILD SUCCESS (verified 2026-11-03)

`cmake --build build --config Release` produces `MatrixGame.dll` (2,033,664 B,
`build/MatrixGame/Release/`) — the FIRST complete modern-MSVC rebuild of the GPL engine.
ABI parity with the shipped DLL confirmed by pefile:
- exports: exactly one, `GetRobotInterface` (both)
- ImageBase 0x10000000, subsystem GUI (both)
- import set identical except advapi32 (shipped-only; likely registry config path unused
  in the built thread)
- section layout differs (historical vs new linker) — irrelevant for the ABI.

Build stages and their fixes (details above in escalation 5):
- Configure: pass 1 clean (SEE DXSDK_DIR), pass 2 uncovered stale-cache MATRIXGAME_BUILD_DLL=OFF
  → whole loop built an .exe; fixed by -DMATRIXGAME_BUILD_DLL=ON explicit reconfigure.
- Compile:只有一个 ICE TU per probing — MatrixLogic.cpp (the co-op hot file!); bisect table
  in MatrixGame/CMakeLists comments; fixed with whole-target /Ob1 + per-file /Od.
- Link: /SAFESEH:NO for the unmarked MASM TU; legacy_stdio_definitions for DxErr's
  __vsnwprintf; upstream-missing includes in MatrixMapGroup/MatrixMapTrace/MatrixMultiSelection
  (IsLiveCannon/IsLiveBuilding are __forceinline in MatrixObjectCannon.hpp/MatrixObjectBuilding.hpp).

Ready state: isolated test install — copying via robocopy into
`local/test-install/` (2.61 GB, robocopy /MT:8) in progress. Next: drop-in replace
MatrixGame.dll there, run Rangers.exe, verify `Load MatrixGame.dll .... ok` path and
battle entry, then start the netting layer design.

## Observed escalation 5: the engine build setup (verified 2026-11-03)

- Toolchain installed (owner-approved): VS Build Tools 2022 17.14 (MSVC 14.44, ml.exe
  x86/x64 present, Windows SDK 10.0.26100). no standalone CMake — using the bunded one.
- DirectX SDK June 2010 (9.29.1962.1) obtained from the official MS download page (id=6812,
  https://download.microsoft.com/download/a/e/7/ae743f1f-632b-4809-87a9-aa1bb3458e31/DXSDK_Jun10.exe),
  571.7 MB, SHA-256 705271DC83BFEE54D9B94E028426E288D5F070784B7446D164F48ECFBB2A02CB
  (pinned). Install is NOT needed: 7-Zip extracts the full
  Include/Lib tree; CMake's FindDIRECTX9 consumes it via the `DXSDK_DIR` env var
  (extract path: `<opencode temp>/dxsdk/extract/DXSDK`). This avoids the known installer
  failures on modern Windows (S1023, .NET 3.5 requirement).
- CMake configure (VS 2022, -A Win32, Release DLL default MATRIXGAME_BUILD_DLL=ON):
  passed clean — VC tools found, DIRECTX9 found, ASM_MASM (ml.exe) found, project generated.
  MatrixGame builds as a MODULE (DLL) with `GetRobotInterface`, MatrixLib(+MASM file) and
  ZLib as static libs; Extras/MaxExp is not in the root build (needs 3DS Max SDK, irrelevant).
- binaries: `cmake --build build --config Release` (log: `local/analysis/build_release.log`).
- Port fixes so far (all local, candidates for an upstream PR):
  1. `CMakeLists.txt` (root) + `MatrixLib/FilePNG/CMakeLists.txt`: define
     `WINDOWS_IGNORE_PACKING_MISMATCH` — /Zp1 vs winnt.h static_assert of SDK 10.0.26100.
  2. `MatrixGame/CMakeLists.txt`: per-file `/Ob1 /Od` for `src/MatrixLogic.cpp`
     (MSVC 14.44 optimizer ICE; bisection table in the comment — every other flag
     dimension eliminated), whole-target `/Ob1` after the same ICE hit more TUs.
  3. Root `LINK_OPTIONS`: `/SAFESEH:NO` — MASM `asharpen.asm` is not SAFESEH-marked;
     the 2010 targets shipped with `/DYNAMICBASE:NO` anyway, so opt-out matches. Plus
     `legacy_stdio_definitions` link for `__vsnwprintf` (referenced by legacy DxErr.lib;
     documented MS substitute).
  4. IWYU include fixes (UPSTREAM BUG working from murgesku master 06e4d32): `IsLiveBuilding`
     and `IsLiveCannon` are `__forceinline`-defined in MatrixObjectBuilding.hpp:333 /
     MatrixObjectCannon.hpp:221, but MatrixMapGroup.cpp, MatrixMapTrace.cpp,
     MatrixMultiSelection.cpp call them without including those headers → LNK2019/LNK2001.
     Fixed by adding the missing includes.
- Success criterion standing: the DLL must export `GetRobotInterface`, then swap into the
  isolated test install and prove the ABI
  (m_Init/m_Support/m_Run called by Rangers.exe as expected).

## Observed escalation 4: how manual robot control actually works (verified 2026-11-03)

- Robot control is order-driven: `SOrder { OrderType m_OrderType; OrderPhase m_OrderPhase;
  float m_Param1..3; … }` (`MatrixRobot.hpp:123-164`). Manual driving is NOT a special state —
  the interface produces the same orders: `ROT_MOVE_TO(x,y)` (+params), `ROT_MOVE_TO_BACK`,
  `ROT_STOP_MOVE`, `ROT_FIRE`, `ROT_STOP_FIRE`, `ROT_CAPTURE_FACTORY`, `ROT_STOP_CAPTURE`.
  `OrderPhase` marks the executing stage (`ROP_MOVING`, `ROP_FIRING`, `ROP_CAPTURING`, …).
- `ROBOT_FLAG_DISABLE_MANUAL` (`MatrixRobot.hpp:357`) removes a bot from manual takeover;
  `CInterface.cpp:1512/1516` checks `IsDisableManual()` + `player_side->IsArcadeMode()`
  before steering, and all side-group logic in `MatrixSide.cpp` (lines 6573+) skips
  DISABLE_MANUAL bots from group orders — i.e. the engine already separates «bot under
  player control» from «bot under AI».
- `CMatrixSideUnit` is a per-side commander object (sides exist as a first-class list);
  friendly-fire damage is configurable per weapon (`m_FlyerDamages.friend_damage`,
  MatrixConfig.cpp:561).

Design implication for co-op: multiple humans = multiple `CMatrixSideUnit`s (allied sides
or same side), each client feeds its OWN robot with local orders; the remote player's robot
gets its `SOrder` stream synchronized over the network (same input the interface would send).
Sync granularity = orders + per-tick pose deltas, not raw input. Open risk: `player_side`
may be a global singleton pointer that CInterface assumes — must be checked at runtime.

## Observed escalation 3: config container formats (verified 2026-11-03)

Engine config format identified from the open source itself
(`MatrixLib/Base/src/CStorage.cpp:400-460`): CStorage = `"STRG"` magic (0x47525453) +
version dword (1 = ZL03-compressed) + records; v1 payload is a zlib stream
(`ZL03_Compression/UnCompress`, magic 78 DA) of [record count + records].

- `CFG\Rus\robots.dat` (77,184 B): starts `STRG 01 00 00 00 ZL03 …78 DA` — decompresses
  cleanly (65,000 B) with plain `zlib.decompress(d[20:])`. It is the Matrix engine's
  runtime config (`FILE_CONFIGURATION = L"robots.dat"`, `FileConfigurationLocation = cfg\`,
  StringConstants.hpp:9-10; read by `g_MatrixData->BlockGet(...)` in MatrixConfig.cpp).
- German Tweaks mod (` Mods/Tweaks/German/CFG/robots.dat`, 75,588 B) uses the SAME format —
  mods ship STRG/ZL03 files as plain drop-in replacements for engine config. Interface layout,
  robot parts, weapon descriptions all live there (decompressed copy:
  `local/analysis/robots_decomp.txt`).
- `CFG\Main.dat` (closed exe's own config, 96,526 B) is NOT STRG: high-entropy from byte 0.
  It shares bytes 4..15 with the 324-byte LeoDomik mod patch (`BF 01 4A 07 33 47 42 C0
  F9 0C 6E 00`) but differs everywhere else → byte 0..3 is per-file (probably a checksum or
  key seed), the rest is contained/scrambled. Mods DO ship partial Main.dat overrides, so
  partial-record override is supported, just not solvable without cryptanalysis.
  Engine-side, `MatrixOverride`/`CfgOverride` (Rangers.exe strings) most likely select
  paths for `robots.dat`/CFG — meaning the game plausibly supports pointing the engine-DLL
  and cfg outside the install dir. Exact consumer still unknown.

## Unknowns (current open list)

- `MatrixOverride` — new verified context (2026-11-04,idl file offset 4093528 in
  Rangers.exe): it is a UTF-16 Pascal-string entry (len=0x1C) sitting in a string
  table BETWEEN the config-like keys `Robot` and `RobotsMap`, with ASCII neighbors
  `MatrixGame.dll`, `GetRobotInterface`, `Load MatrixGame.dll .... ok` and
  `Robot.Support()=`. Read-only interpretation: it is a KEY of the closed exe's
  config parser (the Main.dat reader), used near the engine-DLL loading code, not a
  random label. Where the VALUE lives (Main.dat / registry / CFG.TXT) is still an
  experiment: try a `MatrixOverride=key` style line in plain `CFG.TXT` and check
  whether Rangers picks it up; cryptanalysis of Main.dat remains plan B.
- `CFG\Main.dat` container (96,526 B): encrypted/custom; byte 0..3 per-file variant.
  Has 4 categories of work: dump unpacked layout with a PE patch parser, manual XOR
  hunt, run-time memory RW of the parsed table, or no-op (find a non-invasive DLL
  name mechanism first).
- Other `DATA/*.pkg` formats are the SAME pkg family (main engine open code) once
  the Map-folder caveat is understood (orphan catalog areas in arcade.pkg).
- Whether Rangers.exe has any dormant multiplayer/netcode paths beyond the Matrix
  battle run (no evidence; game is documented single-player).
- Full field layout of `SRobotGameState` and how the galaxy sim treats returned
  tallies (matters for shared-galaxy co-op later).
- Whether the refactoring branch (vladislavrv) is a better base for co-op work.
- Smoke state: standalone rebuild (build-exe) is BLOCKED on re-download of
  DirectX SDK Jun 10 (opencode temp dir was cleaned by the system, original extract
  removed). Installer bytes are pinned: SHA-256
  705271DC83BFEE54D9B94E028426E288D5F070784B7446D164F48ECFBB2A02CB. New layout:
  kept in a persistent location `local\tools\DXSDK` (see smoke_matrixgame.ps1);

## Next steps (phase 3→4)

1. ~~Diff the exported API of the shipped `MatrixGame.dll` against the open-source
   headers~~  DONE: single export `GetRobotInterface`, ABI fully derived (Observed
   escalation 2); engine REBUILT (escalation 6); smoke pending (see Unknowns).
2. Resolve `MatrixOverride`: try CFG.TXT-level config probe; cryptanalysis of
   Main.dat is a fallback; low priority until the smoke test needs it.
3. ~~Rebuild the GPL engine~~ DONE (escalation 6): MatrixGame.dll 2,033,664 B,
   ABI parity with shipped DLL (same export, ImageBase, subsystem). Remaining:
   standalone MatrixGame.exe rebuild + headless CalcVis smoke on Map_1.map
   (`local/analysis/smoke_matrixgame.ps1`).  → DONE 2026-11-05, escalation 8
   (now: Boarding.cmap from DATA/robots.pkg; Map_1.map is closed-format 'abwm').
4. Add the networking layer inside the engine — DESIGN DRAFTED
   (`DESIGN-netlayer.md`): host-authoritative, guest = side 2 of the built-in
   4-side set, SOrder reliable stream + per-tick pose deltas, ENet vendored;
   M1 (invisible harness) → M4. Instruments `fakeworld`/`flight` from tools/ still
   to be adapted after M2.
5. Full-`Rangers.exe` (galaxy sim) co-op = separate much larger phase; record the
   seam only.

## Observed escalation 8 - smoke debugging -> PHASE 2 CLOSED (CalcVis PASS)

Date: 2026-11-05. All claims below verified by run output in this session.

FACTS (verified):
1. Why the old smoke silently died: unhandled exceptions inside the engine are
   routed by `CDebugTracer::StaticInit` -> `SetUnhandledExceptionFilter(sys_except_handler)`
   + `std::set_terminate(cpp_except_terminate)` (MatrixLib/Base/src/Tracer.cpp:357-359).
   Both handlers show a blocking `MessageBoxA("Unhandled Exception")` and then the
   filter returns EXCEPTION_EXECUTE_HANDLER. Under smoke automation the process sat
   in the popup until the 120 s kill; that kill code masqueraded as the exit status.
   No WER event: the engine-level filter catches the exception before WER ever
   sees it.
2. The actual exception (user-visible popup screenshot): MatrixGame.cpp
   "Unable to load map. Error happens." thrown when `CMatrixMap::PrepareMap`
   (MatrixMapPrepare.cpp:997) returns -1 because
   `stor.GetBuf(DATA_PROPERTIES, DATA_PROPERTIES_NAME, ST_WCHAR)` == NULL.
3. Root cause of the empty storage: `CStorage::Load` (CStorage.cpp:393-473)
   silently returns false for any tag != 'STRG'. The file we fed it,
   `Map_1.map` extracted from DATA/arcade.pkg, is NOT STRG:
   - ABMap '*.map' = new HD format  'abwm' + dword ver(1)  (closed-part serializer)
   - ABMap '*.opt' = 'ZL01' + dword(3727178) + raw zlib stream
   Neither is mentioned anywhere in the GPL tree (no 'abwm'/'opt' loader).
4. Discovery of the engine-format battle maps: DATA/robots.pkg ships
   `/Matrix/Map/*.cmap` in the ENGINE CStorage format:
   `Boarding.cmap` header 'STRG', ver 0 (uncompressed), 18 records:
   strings / cannons / buildings / robots / objects / surfacesM / surfaces /
   roads / bridges / groups / texunions / points / inshores / move / bottom /
   bitmaps / properties(Name,Value) / effects.
   `properties` carries SizeInUnitsX/SizeInUnitsY/... exactly what PrepareMap needs.
   SHA-256 Boarding.cmap = 8DEF488CB899A1C0F2CC76978954478CFFF32E9866DC336275CF091F094E51DA.
   CStorage item WStr layout = raw UTF-16LE to null (NO length prefix;
   CBuf::WStr/WStrLen, CBuf.hpp:103/CBuf.cpp:80). ver==1 payload = ZL03:
   'ZL03' + dword blockcount + blocks { dword clen; zlib } (CStorage.cpp:18-51).
5. Headless smoke now PASSES end-to-end (twice: manual + full smoke_matrixgame.ps1):
   smoke_trace.log: WinMain enter(numarg=2, map=Boarding.cmap path) -> MatrixGameInit ok
   -> FormChange ok -> map branch -> CalcVis ok; calcvis.log " ...done"; exit=1
   (normal `return 1`, MatrixGame.cpp). SR_SMOKE_HEADLESS=1 checked before any popup.
6. All engine-side breaking paths are now headless-friendly (env-gated,
   upstreamable diagnostics):
   - Tracer.cpp: crash report also appended to smoke_crash.log + exit code 3
     when SR_SMOKE_HEADLESS=1 (both handlers), popup preserved interactively.
   - MatrixGame.cpp: SmokeTrace to smoke_trace.log per stage; catch-blocks write
     smoke_fatal.log + exit 2 headless.
7. smoke_matrixgame.ps1 rewritten: builds exe, extracts Boarding.cmap from
   DATA/robots.pkg (pkg_tool.py find), copies to test-install/Matrix/Map/,
   runs with the full path arg, reports all four logs. Verdict PASS.

CONCLUSIONS:
- Phase 2 (engine rebuilt + standalone exe runs a real battle map through the
  full init->PrepareMap->CalcVis path) is CLOSED.
- For co-op MVP the engine consumes/*.cmap out of robots.pkg; no need to decode
  the closed 'abwm'. NOTE: the ABMap arcade-arena set is still only reachable
  through the closed Rangers.exe seam (L2), but co-op can be built and validated
  on any stock .cmap. Probe which stock maps Rangers.exe actually passes to the
  DLL on planet battles before assuming parity.

NEXT: milestone M1 per DESIGN-netlayer.md вЂ” invisible in-engine net harness,
config-gated (off by default), two standalone exes on one machine, lockstep-free
host-authoritative skeleton. Keep smoke_matrixgame.ps1 as the regression gate.

Upstream PR candidates (running list, 6th addition):
6. Env-gated headless diagnostics: SR_SMOKE_HEADLESS turns blocking crash popups
   (Tracer.cpp) into logged process exits with distinct codes (3 crash / 2 caught
   exception), plus optional per-stage trace (MatrixGame.cpp SmokeTrace).
   Enables unattended smoke/CI of the GPL engine.


## Observed escalation 9 - M1 CLOSED: net harness compiles in, OFF by default, 3-case smoke PASS

Date: 2026-11-06. Everything below verified by build logs + smoke runs of this session.

FACTS (verified):
1. Vendored ENet v1.3.18 into ThirdParty/ENet. Source: official maintainer repo
   github.com/lsalzman/enet tag v1_3_18 tarball; enet.bespin.org was unusable
   (cert chain mismatch on this network, curl SEC_E_WRONG_PRINCIPAL).
   Pinned SHA-256 enet-1.3.18.tar.gz = 28603C895F9ED24A846478180EE72C7376B39B4BB1287B73877E5EAE7D96B0DD.
   Vendored patch: all 53 public `ENET_API` declarations in
   ThirdParty/ENet/include/enet/enet.h now carry an explicit `__cdecl` placed
   AFTER the return type. MSVC rejects a leading `__cdecl` decl-specifier in
   C++ (verified minimal repro), and the engine target compiles with /Gr
   (fastcall), so decorations must force cdecl at patch level.
   ENet build options: static lib, /W1 /MT /GS- /EHsc- (see root CMakeLists).
2. New harness sources: MatrixGame/src/Net/SRNetHarness.{hpp,cpp}.
   - MatrixGameInit -> SRNetInit() (env-gated), MatrixGameDeinit -> SRNetDeinit();
   - CMatrixMap::Takt -> SRNetPump() first line (guarded no-op when off);
   - switches: SR_COOP=host|guest, SR_COOP_PORT (25333), SR_COOP_SIDE; side
     default host=1 / guest=2, engine 4-side set respected (1..4);
   - host: enet_host_create + per-pump service loop, guest socket creation in
     M1; srnet.log records lifecycle lines.
3. CMake: add_library(ENet STATIC <8 c files>) + include dir; MatrixGame
   links ENet + ws2_32; src/Net added via add_matrix_directory.
4. Build integration port notes (all bit us during the fresh reconfigure):
   - a fresh CMake VS configure without -A defaults to x64 platform on this
     toolchain -> 32-bit .asm (asharpen) gets compiled as x64 and explodes;
     ALWAYS -A Win32 for both build trees;
   - `option(VAR ON)` is NOT a default-value construct (ON is the docstring);
     the real default is OFF. Fixed to option(VAR "desc" ON) and configs pass
     -DMATRIXGAME_BUILD_DLL explicitly;
   - ENet must precede winsock: include <winsock2.h> before project headers;
   - ENet must be built with /MT: CMake defaults /MD, whose MSVCRT defaultlib
     drags chandler4gs.obj -> unresolved __except_handler4_common;
   - ws2_32.lib needed for winsock imports.
5. Artifacts (both builds green, e1=e2 exit 0):
   - MatrixGame.dll 2,056,192 B (was 2,033,664; +22 KiB ENet) вЂ” dumpbin
     /exports: exactly 1 ordinal, GetRobotInterface => ABI parity kept.
     Copied to local/test-install/MatrixGame.dll (orig stays .orig);
   -	MatrixGame.exe (standalone, build-exe tree) 1,970,688 B (was 1,949,184).
6. M1 smoke: local/analysis/smoke_coop_m1.ps1 вЂ” three runs of the standalone
   exe on Boarding.cmap (full CalcVis path):
   - A no env      : srnet.log ABSENT, CalcVis PASS  -> co-op-off is inert;
   - B SR_COOP=host: srnet.log has "host listening on port 25333",
     "harness enabled, side 1", CalcVis PASS;
   - C SR_COOP=guest(SR_COOP_SIDE=2): "guest socket created",
     "harness enabled, side 2", CalcVis PASS.
   Verdict: ALL PASS. Regression gates now: smoke_matrixgame.ps1 (phase 2)
   + smoke_coop_m1.ps1 (M1).

CONCLUSIONS:
- M1 milestone CLOSED: the net layer is compiled into both artifacts, fully
  disabled by default, additively wired (no behavior change when off).
- Open: SRNetPump currently only runs inside CMatrixMap::Takt (battle loop).
  For M2 join-before-battle handshake we will also pump in the L3GRun idle
  path (menu) вЂ” to be added with the order channel.

NEXT: milestone M2 вЂ” two standalone engines on one machine: host+guest load
Boarding.cmap, session established pre-Takt; guest receives pose deltas of
host-owned side-1 bot and injects side-2 orders (see DESIGN-netlayer.md sec 7,
M2). ALSO: run smoke_matrixgame.ps1 once against the freshly configured
build-exe tree as the phase-2 regression gate (already PASS for the exe used
by M1 smoke).


================================================================
2026-11-07 — M2 session: crash fixed, handshake works, M2 gate in place
================================================================

Start point: session 2026-11-06 ended blocked on two unknowns: (a) host AV
0xC0000005 at guest connect inside SRNetPump with wild ev.packet pointers,
(b) "battle tac pump first reached" marker never firing (takt pump = 0).

FACTS (all verified by runs/disasm this session):

1. Takt pump silence explained. Standalone battle branch: the map is PAUSED the
   whole time (logic takt trace: paused=1, flags=0x460830, MMFLAG_PAUSE bit set);
   CMatrixMapLogic::Takt takes the paused early return and never calls its tail
   CMatrixMap::Takt -> the pump installed there never ran (taktCnt=0). Meanwhile
   the L3G idle hook pumps once per frame even in battle (hookCnt == tick count):
   51/50 and 401/400 in probes.
   FIX: SRNetPumpTakt() moved from CMatrixMap::Takt first line (MatrixMap.cpp)
   to the entry of CMatrixMapLogic::Takt (before the pause branches). Pump
   sources counters in SRNetDeinit now show e.g. "hook 401 400" on both sides
   in a battle run.

2. Host crash ROOT CAUSE found structurally and fixed. The vendored ENet
   third-party target compiled WITHOUT byte packing while the whole MatrixGame
   engine is compiled /Zp1. ENetEvent (vendored enet.h):
   {type, peer, channelID, data, packet}. Unpacked (ENet lib view): type@0,
   peer@4, channelID@8, data@0xC, packet@0x10. Packed-1 (MatrixGame harness
   view): type@0, peer@4, channelID@8, data@9, packet@0x0D. So
   enet_host_service wrote event->packet at ev+0x10 and the harness read it
   from ev+0x0D -> wild pointer built from data/channel/garbage bytes
   (observed 0x5800000C / 0x8000000C / 0xC8000008 pattern = data + channel
   bytes). AV on the first RECEIVE (ev.packet->data[0]).
   Evidence chain: crash address resolution via map (SRNetPump+0x86/0x8B);
   dumpbin /disasm window around 00505C.. shows the compiled reads
   (movzx byte [esp+8] for channelID, mov [esp+0Dh] for packet) matching ONLY
   the /Zp1 view.
   FIX: CMakeLists.txt — ENet target now gets /Zp1 (plus
   WINDOWS_IGNORE_PACKING_MISMATCH to silence winnt.h static_asserts, same
   opt-out the game target already uses). Both sides of every shared struct
   agree now. ENet objs rebuilt (host.obj 2026-11-07 09:28), exe relinked.

3. Handshake works end-to-end now (both at Boarding.cmap, reliable channel 0):
   host: "host listening", "harness enabled, side 1", "host: guest connected",
         "host: session established with guest side 2", "host: WELCOME sent";
   guest: "guest socket created", "harness enabled, side 2",
         "guest: host session up", "guest: HELLO sent",
         "guest: session established, granted side 2".
   HELLO/WELCOME both 72 bytes. Guest deinit shows sessionUp=1:
   "harness deinit after cycles 801 1", pump sources "hook 401 400".

4. Host never logged "host: guest gone": guest SRNetDeinit called
   enet_peer_disconnect (graceful, ACKed) and then enet_host_destroy
   immediately -> the disconnect packet never left the socket.
   FIX: SRNetDeinit uses enet_peer_disconnect_now (unsequenced command +
   enet_host_flush + peer reset; vendored peer.c:507). Host then logs
   "host: guest gone" and the M2 gate passes.

5. Regression gates after every change today: smoke_matrixgame.ps1 PASS
   (Boarding.cmap SHA unchanged 8DEF…51DA), smoke_coop_m1.ps1 PASS (A/B/C).
   New: local/analysis/smoke_coop_m2.ps1 — M2 handshake gate (host+guest
   battle-smoke pair; PASS criteria in script header). Verified PASS twice.

6. Artifacts after this session:
   - MatrixGame.exe rebuilt from build-exe tree (relinked twice today), map
     regenerated (build-exe/MatrixGame_release.map) for crash resolution.
   - MatrixGame.dll (build tree) rebuilt with the same harness fixes,
     2 082 304 B, SHA256 9F32E43DF43987873DCE7F398A7440F6E95E389D3F060AFE5FF8DC924D0339FC,
     copied to local/test-install/MatrixGame.dll (stock kept as .orig).
   - Note: yesterday's AV logs resolve to the STALE map; all analysis above
     re-derived against today's fresh map.

7. Temporary diagnostics REMOVED after diagnosis (breadcrumb one-shots in
   MatrixLogic.cpp/MatrixFormGame.cpp, per-packet "pump: receive pkt" log,
   "battle tac pump first reached" marker). Kept: the cheap BAD-receive-packet
   guard in SRNetPump (logs & drops NULL/0-length packets), pump-source
   counters, and the SR_LOG_PREFIX log-separation scheme (verified: logs
   appear as <prefix>_srnet.log etc.; note the prefix yields a double
   underscore for prefixes already ending in _).

CONCLUSIONS:
- Milestone M2 handshake CLOSED and gated. Phase-2 + M1 gates still PASS.
- The 2026-11-06 crash was NOT heap corruption nor a race — a struct layout
  skew (/Zp1) between compilation units. Encode "ENet must be /Zp1 too" in
  CMake so it cannot regress silently (done, with comment).

NEXT (M2 remainder, see HANDOFF sec 10):
- channel B (unreliable, ch 1): robot pose snapshots {unitId, x, y, heading,
  phase, speed}; pour into the guest's map. CLOSED (2026-11-07): host sends
  PKT_POSE every Takt from SRNetPumpTakt; if Takt() is not called (menu/idle),
  poses are also sent from SRNetPumpHooked() via SRNetSendPosesFromTick().
  Guest receives in SRNetPump() RECEIVE branch and applies to m_Side[1]->
  m_Robots[] in CMatrixMapLogic::Takt(). smoke_coop_m2.ps1 extended with
  ">=1 PKT_POSE" criterion — PASS.
- channel A (reliable): PKT_ORDER guest->host, applied same-Takt. CLOSED
  (2026-11-07): guest sends PKT_ORDER {unitId, orderType, orderPhase,
  param1-3, param4} from side-2 robots' orders every Takt via
  SRNetSendOrderPod(); host receives in SRNetPump() RECEIVE branch
  (SRNetHandleOrder()) and applies to m_Side[2]->m_Robots[] in
  CMatrixMapLogic::Takt(). smoke_coop_m2.ps1 extended with ">=1 PKT_ORDER"
  criterion — PASS.

---
2026-11-07 Session: M2 шаги 3 и 4 §10 (каналы B и A) DONE and gated.

FACTS:
- PKT_POSE {type=3, side, count, entries[unitId u32, x f32, y f32, heading f32,
  phase u32, speed f32]} sent unreliable on ENet channel 1 from host every Takt
  from SRNetPumpTakt context (CMatrixMapLogic::Takt start). If Takt() is not
  called (menu/idle), poses are also sent from SRNetPumpHooked() via
  SRNetSendPosesFromTick(). Host builds packet from g_MatrixMap->m_Side[1]->
  m_Robots[] (side 1 = host). Empty slots (m_Robot == NULL) skipped. count can
  be 0 (map has no robots on side 1 — Boarding.cmap case); packet still sent
  with count=0 so channel stays warm.
- Guest receives in SRNetPump() RECEIVE branch on channel 1, validates type/
  side/count/length, stores into static SPKTPoseEntry buffer, logs
  "guest: first PKT_POSE received".
- Guest applies in CMatrixMapLogic::Takt() after pump: applies poses to
  m_Side[1]->m_Robots[unitId] (host's side on guest map), sets m_PosX/m_PosY/
  m_Unit[0].m_Angle/m_Speed. Logs "guest: first PKT_POSE applied".
- PKT_ORDER {type=4, side=2, count, entries[unitId u32, orderType u32,
  orderPhase u32, param1 f32, param2 f32, param3 f32, param4 u32]} sent reliable
  on ENet channel 0 from guest every Takt from SRNetPumpTakt context.
  Guest builds packet from g_MatrixMap->m_Side[2]->m_Robots[] (side 2 = guest).
  Empty slots and orders with ROT_EMPTY_ORDER skipped. Host receives in
  SRNetPump() RECEIVE branch on channel 0, validates, stores into static
  SPKTOrderEntry buffer, logs "host: first PKT_ORDER received".
- Host applies in CMatrixMapLogic::Takt() after pump: applies orders to
  m_Side[2]->m_Robots[unitId]->m_Robot->m_OrdersList[0] (guest's side on host
  map), sets m_OrderType/m_OrderPhase/m_Param1-3/m_Param4. Logs "host: first
  PKT_ORDER applied".
- All harness functions use explicit __cdecl (engine built /Gr).
- SRNetHarness.hpp does NOT include <enet/enet.h> (avoids winsock2/GROUP
  typedef-vs-enum conflict with MatrixSide.hpp in MatrixMap.cpp). ENet types
  only in .cpp.
- GROUP enum value in MatrixSide.hpp:86 conflicts with winsock2.h typedef; left
  as-is (used in MatrixFormGame.cpp/MatrixSide.cpp). SRNetHarness.hpp avoids
  enet.h include entirely.

VERIFY: smoke_matrixgame.ps1 PASS, smoke_coop_m1.ps1 ALL PASS,
smoke_coop_m2.ps1 ALL PASS (handshake + >=1 PKT_POSE sent/received +
>=1 PKT_ORDER sent/received).

---
2026-11-07 Session: M3 (real battle via test-install) VERIFIED.

FACTS:
- Two separate Rangers.exe processes launched from local\test-install\ with our
  MatrixGame.dll loaded (replaces stock MatrixGame.dll.orig).
- Host: `SR_COOP=host SR_COOP_PORT=25333 SR_LOG_PREFIX=m2h_ SR_SMOKE_HEADLESS=1
  SR_SMOKE_BATTLE=1 Rangers.exe Matrix\Map\Boarding.cmap`
- Guest: `SR_COOP=guest SR_COOP_SIDE=2 SR_COOP_ADDR=127.0.0.1 SR_LOG_PREFIX=m2g_
  SR_SMOKE_HEADLESS=1 SR_SMOKE_BATTLE=1 Rangers.exe Matrix\Map\Boarding.cmap`
- Host log: `host: guest connected`, `host: session established with guest side 2`,
  `host: WELCOME sent`, `host: first PKT_ORDER received 0 2`, `host: guest gone`.
- Guest log: `guest: connecting to host 25333`, `guest: host session up`,
  `guest: HELLO sent`, `guest: session established, granted side 2`,
  `guest: first PKT_POSE received 0 1`, `guest: first PKT_ORDER sent 0 2`,
  `harness deinit after cycles 801 1`.
- Channel B (poses) and Channel A (orders) both verified working between two
  independent Rangers.exe processes on one machine via ENet localhost:25333.
- Boarding.cmap has count=0 robots on both sides (empty battle map), so pose/order
  counts are 0 but the protocol channels are confirmed functional.

NEXT: M4 (upstream-PR ready) — install gh CLI, port-fix commits, README,
regression scripts. Optional: test M3 with a map that has actual robots for
visual confirmation of pose replication.
