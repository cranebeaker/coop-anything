# Alien: Isolation — agent notes

Game: Alien: Isolation (2014, Creative Assembly / SEGA).
Owner goal: **online co-op with friends** (splitscreen out of scope unless asked later).

## Phase 0 — Triage (2026-10-20)

### Observations (verified on this machine)

- Store/version: **Steam**, appid 214490, **buildid 4940658** (from `appmanifest_214490.acf`,
  `TargetBuildID` = `buildid` = 4940658, StateFlags 4 = fully installed, 2026-10-20).
- Install dir: `<SteamLibrary>\steamapps\common\Alien Isolation` (owner's copy —
  read-only for us; test copies will be made per ground rule 2).
- Main exe: `AI.exe`, 21,822,464 bytes,
  SHA-256 `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90`.
- Top-level layout: `AI.exe`, `DATA\`, `Extras\`, `AMD_AGS.DLL`, `ATIAGS.DLL`,
  `CRASHREPORTER32.DLL`, `STEAM_API.DLL`, `BENCHMARK_TEMPLATE.TXT`.
- Engine: Creative Assembly proprietary ("Evolution" engine; no Unreal/Unity/CryEngine
  DLLs present). 64-bit (Steam build). See engine-families.md — no sibling co-op
  framework known for this engine.
- OS: Windows 10/11 (PowerShell 5.1 environment), VS 2026 Community + C++ workload
  installed (from prior sessions, see `local/`).

### Triage answers

1. Exact game: Alien: Isolation, 2014, Steam build 4940658. ✔
2. Already done? Per `DIRECTORY.md`: one prototype (Dropper7894, announced, never
   released). Research below — **no downloadable co-op mod found** as of 2026-10-20.
3. Engine: Creative Assembly proprietary. DLL-level modding only.
4. Source available: **nothing public** — closed binary. Heavy RE expected. No decomp
   project found (searched "alien isolation decomp", GitHub topics `alien-isolation`).
5. Engine multiplayer: **none** — the game is single-player only (Survivor Mode is a
   scoring challenge mode, not netcode). No replication layer to reuse → must build a
   sync layer (host authority + state sync).
6. Shipping check: mod must ship as a separate package (DLL proxy/hooks + scripts),
   no game files. No anti-cheat, no online service in the campaign (Steam works
   overlay only) → safe to hook offline play. Must not touch Steam multiplayer of
   other games. OK.

**Verdict: Path 2 — build from zero** (nothing usable exists to revive). Difficulty:
closed binary, no MP, no scripting language shipped → "very hard" row of the triage
table. Start with research + design doc, look harder for collaborators/existing work.

## Phase 1 — Research (2026-10-20, in progress)

### Existing attempts

1. **Alien Isolation co-op (Dropper7894)** — announced 2026-06-30 with gameplay
   video; goal: turn **Survivor Mode into 2-player co-op**. No release, no repo, no
   ETA. Author runs a Discord for progress updates: https://discord.gg/UhHCPMdMh
   (joining = contacting people → needs owner's permission, ground rule 1).
   - https://bloody-disgusting.com/video-games/3957869/fan-made-co-op-mod-for-alien-isolation-announced/
     (article fetched 2026-10-20, dated June 30 2026 — confirms Discord link + goals)
   - https://www.dsogaming.com/videotrailer-news/first-look-at-upcoming-multiplayer-co-op-mod-for-alien-isolation/
   - https://shazoo.ru/2026/06/30/186490/modder-sozdaet-kooperativnyi-rezim-dlia-alien-isolation (2026-06-30)
   - Reddit r/alienisolation "A small mod I'm working on"
     (https://www.reddit.com/r/alienisolation/comments/1uhr3gr/, ~384 upvotes,
     "short demo of a multiplayer mode… still far from being a full co-op mode") —
     this IS the Dropper7894 demo post (linked from the BD article). Direct fetch
     403'd; content confirmed via article quotes.
2. **Echo Protocol** — fan-made 2-player co-op announced in r/modding
   (https://www.reddit.com/r/Modding/comments/1jyz9dq/, ~2025-03). No repo, no
   release, no recent activity found by web search. Status: **unknown, looks silent**.
3. Steam thread "has anyone played the alien isolation co-op mod"
   (https://steamcommunity.com/app/214490/discussions/0/598198356181958694/): top
   reply says the linked "co-op" is a Garry's Mod gamemode, not an A:I mod. Treat
   player chatter as low-signal here.
4. **Nucleus Co-op** (splitscreen tool, open source) supports many games; A:I support
   **unknown** — would only matter for splitscreen, currently out of scope.
5. **OpenCAGE** — https://github.com/MattFiler/OpenCAGE (MIT, 356★, 1024 commits as of
   2026-10-20, actively maintained; also a free Steam tool app 3367530; site
   opencage.co.uk). Full modding toolkit for A:I: level logic scripts ("Composites",
   XML/BML format), **behaviour trees** (Brainiac Designer editor), character/item
   configs, assets. Built on MattFiler/CinematicTools (Hattiwatti's in-process
   camera/tool injector for A:I) + x1nixmzeng/AlienBML (file format parser).
   Repo page fetched 2026-10-20. This is the strongest foundation on this engine:
   the game's logic layer is data-editable without native code, and CinematicTools
   proves DLL injection into AI.exe works.
6. **aliasIsolation** (TAA mod): https://github.com/aliasIsolation/aliasIsolation
   — another proven AI.exe injection precedent (method not yet verified).
7. Engine families: `playbook/engine-families.md` has **no entry** for Creative
   Assembly's Evolution engine → no sibling co-op framework to port. Possible
   playbook gap to note in the PR.

### Derived (not yet verified)

- No netcode, no native source, no decomp → the sm64coopdx / KeeperFX routes are
  closed; the realistic model is **host authority + state sync via injected DLL**
  (the fnv-coop / Skyrim Together family). Level logic ("Composites") is editable
  via OpenCAGE, which may carry story/quest sync at the data layer instead of the
  native layer — to evaluate in Phase 4.
- Game AI runs on behaviour trees (Brainiac Designer lineage) — AI ownership sync
  will be the hard part (see playbook/actor-ownership.md).

### Unknowns / next questions

- Does Dropper7894 publish anything (YouTube channel, GitHub, Nexus)? Owner may
  join the mod's Discord (discord.gg/UhHCPMdMh) to ask about collaboration
  (ground rule 1 — contacting people needs permission; also Discord mining requires
  playbook/discord-export.md).
- Whether the game's own save format / script tables can be driven externally.
- Legal/licensing: SEGA's stance on A:I mods (community has modded openly for years;
  no C&D known — verify before shipping).

### Checks that ran

- `appmanifest_214490.acf` read (build identity). ✔
- `Get-FileHash AI.exe -Algorithm SHA256`. ✔
- GitHub issues/PRs of cranebeaker/coop-anything searched for "alien": 0 results. ✔
- Web searches (2026-10-20): "alien isolation co-op mod", "Echo Protocol alien
  isolation", "Dropper7894 github", nexusmods multiplayer, github topics
  `alien-isolation`. Reddit direct fetches blocked (403) — noted per research.md.

## Phase 2 — Workspace (2026-10-20)

- Test copy: robocopy of the owner's install (read-only source) →
  `<workspace>\test-install\Alien-Isolation`, 389 dirs / 7,586 files / 30.17 GB,
  0 failures (2026-10-20 21:43). Copy's `AI.exe` SHA-256 **matches** the owner's
  install (DDE66521…9A90). Owner's install and saves untouched (ground rule 2).
- Owner approved: gh sign-in, test copy, "not now" for contacting Dropper7894.

### First look at the data layer (read-only, owner install)

- `DATA\` layout: 301 `.PAK` archives + loose files. Key dirs: `BEHAVIOR\`,
  `BINARY_BEHAVIOR\` (compiled behaviour trees), `ALIENCONFIGS\`, `CHR_INFO\`,
  `GLOBAL\`, `PACKAGES\`, `TEXT\`, `UI\`.
- Loose logic/config files are **both** `.XML` and `.BML` pairs (e.g.
  `GBL_ITEM.XML` 236 KB / `GBL_ITEM.BML` 143 KB, `ENGINE_SETTINGS.XML`,
  `INPUT.XML`) — consistent with OpenCAGE's AlienBML (XML source → compiled BML).
  Confirms the game's logic layer is editable at the data level.
- Native side still closed: `.BIN` (compiled behaviour), `.HKX64` (Havok anim),
  `.USM` (movies). No scripting VM docs found → native RE still needed for
  runtime sync (Phase 3+).

## Phase 3 — Understand (2026-10-20, in progress)

### Static recon on the test copy (`test-install\Alien-Isolation\AI.exe`)

- **PE: 32-bit x86** (machine 0x14C, "32 bit word machine"), link timestamp
  Tue Feb 17 2015 10:36:13, 5 sections only (`.data .rdata .reloc .rsrc .text`),
  `Dynamic base` (ASLR). No SteamStub `.bind` section. Denuvo status **unknown**
  (check `.text` entropy before any decomp plan; deferred). Verified with
  `dumpbin /headers`. (Recorded in case other write-ups assume 64-bit.)
- **Imports (dumpbin /dependents):** `steam_api.dll`, KERNEL32, USER32, **WS2_32,
  WSOCK32** (Winsock present — telemetry/LAN? to investigate), PSAPI, imagehlp,
  SETUPAPI, **dxgi, d3d11** (D3D11 renderer), XINPUT1_3, GDI32, SHELL32, ole32,
  WINMM, ADVAPI32, WLDAP32.
- **Derived:** no `dinput8.dll` import → the dinput8-proxy entry point used on
  Resident Evil/Far Cry 2 in this hub **does not apply here**. Candidate load
  vectors instead: `dxgi.dll` proxy (loaded by any D3D11 app; also gives a Present
  hook) or `steam_api.dll` proxy (classic, game must find real steam_api). 32-bit
  process ⇒ x86 DLLs, 32-bit pointer space.
- Python 3.12.10 located at `%LOCALAPPDATA%\Programs\Python\Python312\python.exe`
  (installed via winget; **not on this session's PATH** — use full path).

### Next (Phase 3)

1. Entropy check on `.text` (Denuvo?) before planning any decompile.
2. Boot smoke test of the test copy; watch `%USERPROFILE%\Documents` for new
   config/save dirs (per reverse-engineering.md, move them into the workspace).
3. Study OpenCAGE/CinematicTools source for their injection method + how composites
   are read; reuse rather than reinvent.
4. Get Ghidra headless (pinned download, ask owner) for the runtime seam map.

### Boot smoke test (2026-10-20, test copy, owner approved)

- Launch: `Start-Process AI.exe` — the started pid **exited immediately**; a second
  `AI.exe` process owned the window ("Alien: Isolation" title) ~40 s later (intro
  movies). Find: track by process name, not launcher pid.
- Runtime: process stable, ~1.2 GB working set, responsive. Steam must be running
  (steam.exe was up; direct launch worked without Steam launch-integration prompt).
- Filesystem footprint of a bare boot + graceful exit (CloseMainWindow): **nothing**
  written to Documents, `%APPDATA%`, `%LOCALAPPDATA%`, HKCU\Software (checked:
  My Games, Feral Interactive, `Creative Assembly` key — all absent before and
  after). Settings/saves presumably appear only after a settings change or
  checkpoint; revisit redirection (junction/env) when we drive real gameplay.
- Smoke test result: **boot verified on build 4940658** (QA: boot test only).

### CinematicTools injection method (read 2026-10-20, repo page)

- Now maintained at https://github.com/OpenCAGE/CinematicTools (fork of
  Hattiwatti/CinematicTools). "A DLL for Alien: Isolation which hooks various
  functionality to provide cinematic tools."
- Build: **Win32/x86**, VS2022 v143, NuGet packages: **MinHook** (hooking),
  DirectXTK, Boost. Output `Build\CT_AlienIsolation.dll`.
- Load vector: **runtime injection** via https://github.com/nefarius/Injector
  (`inject.bat` after the game is running) — *not* a proxy DLL. So the proven
  community pattern is: external injector + hooking DLL (MinHook) into the live
  32-bit process; hooks D3D11 among others.
- Derived: our Stage 1 can copy this exact pattern (x86 MinHook DLL + nefarius
  Injector or our own CreateRemoteThread loader) instead of inventing a proxy.

### Toolchain installed (2026-10-20, owner approved)

- Ghidra 12.1.4 PUBLIC → `<workspace>\tools\ghidra\ghidra_12.1.4_PUBLIC`.
  Zip sha256 (pinned; release publishes no checksum):
  `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`
  (source: github.com/NationalSecurityAgency/ghidra release Ghidra_12.1.4_build).
- Temurin JDK 21.0.12.1 → `<workspace>\tools\jdk\jdk-21.0.12.1+1`.
  Zip sha256 **verified against Adoptium's published checksum**: `f9d6e191…d8b4e`. ✔
- `analyzeHeadless.bat` smoke-tested OK with `JAVA_HOME` set (2026-10-20).
- Zips kept in `<workspace>\coop-anything-cache`.

### Entropy check (2026-10-20, tool: `coop-anything-cache\pe_entropy.py`)

- Per-section Shannon entropy of test-copy `AI.exe`: `.text` **6.710**, `.rdata`
  5.147, `.data` 4.931, `.rsrc` 5.651, `.reloc` 5.783.
- Verdict: **no packing / no SteamStub** (packed code sits near 8.0 per triage.md).
  Plain decompilation of build 4940658 is viable. (Whether any Denuvo VM functions
  survive inline is unknown; the code section statistics say the bulk is normal
  compiled x86.)
- Ghidra 12.1.4 headless import of `AI.exe` + full analysis + DecompileAll.java
  (from cranebeaker/deus-ex-coop, read before running) launched 2026-10-20 →
  outputs `<workspace>\decomp\AI.exe.c` + `AI.exe.functions.tsv`.

### Full decompile + seam hunt (2026-10-20)

- **Analysis succeeded:** 456 s auto-analysis; **66,811 functions decompiled, 10
  failed** (9 of those "Unable to read bytes at ram:0x03030303/0x020202" —
  data-as-code false positives). Outputs: `decomp\AI.exe.c` (69.9 MB),
  `AI.exe.functions.tsv` (67,005 lines incl. header), plus
  `decomp\AI.exe.strings.txt` (62,670 strings ≥6 chars, via
  `coop-anything-cache\strings_dump.py`).
- **Engine identity: CATHODE** — RTTI class names in the exe use namespace
  `CATHODE` (e.g. `.?AVIsMultiplayerMode@CATHODE@@`). Creative Assembly's Cathode
  engine. *Triage correction: "Creative Assembly proprietary" in the directory
  should say "Creative Assembly Cathode".*
- **Script seam found:** level-logic events dispatch by string name through vtable
  calls, e.g. `FUN_00827400` sends `"SetPlayerPos"` (with floats) to the object at
  `this+200` under a critical section; siblings `"SetPlayerRotation"`,
  `"Map_SetPlayerPos"`, `"SetIsCurrentArea"`, `"ClearBreadcrumbs"`, `"show_player"`,
  `"hide_player"`, `"release_player"`, `"allow_player_to_skip"`. RTTI shows typed
  command classes: `SetPlayerHasKeycard@CATHODE`, `GetPlayerHasGatingTool@CATHODE`
  etc. → the Composite script layer has a **name→handler command table**; string
  lookup helper `FUN_008572a0` = hash(name) scanned over 20-byte records.
- **Multiplayer status: NO game netcode in this binary.** Evidence:
  - All `connect/send/recvfrom/WSAStartup` hits trace to **libcurl**
    (`curl_easy_send`, FTP/SMTP/TFTP strings) and **Havok visual debugger**
    ("Server created and will poll for new client(s) on port…", hkBase) —
    debug/HTTP only.
  - Dormant script-layer multiplayer *concepts* exist (engine heritage):
    RTTI `.?AVIsMultiplayerMode@CATHODE@@`, strings `IS_MULTIPLAYER_MODE`,
    `MULTIPLAYER_MOTIVATION`, `Multiplayer_Communications`, and a diagnostic
    "…this is okay in multiplayer, but suspicious if it triggers constantly".
  - Derived: A:I itself never shipped MP; but **Cathode as an engine family did**
    (Halo Wars 2, same engine, shipped multiplayer) — a possible reference binary
    for how sessions/replication were structured in Cathode. `engine-families.md`
    has no Cathode entry → playbook gap to propose in our PR.
- Main loop candidates: `PeekMessageA` loops at `AI.exe.c` line 873653 (engine
  pump) and 1244001; `PeekMessageW`/`PostQuitMessage` at 2452334+ (likely bundled
  third-party, e.g. crash/HTTP lib). Far Cry 2 playbook step (patch the message
  pump imports) maps onto line 873653's loop.

## Stage 1 — Load vector + pump probe (2026-10-20, TEST COPY ONLY)

**Correction to the earlier smoke-test note:** the "second AI.exe pid" behavior was
NOT a self-spawn — the test-copy exe calls `SteamAPI_RestartAppIfNecessary`
(import #A of `steam_api.dll`), which hands off to the running Steam client, and
**Steam relaunched the game from the registered real install (G:)**. So the first
smoke test actually ran the owner's real install (bare boot, verified 0 files
written — but still not the intent). Recorded here to keep the trail honest.

### Build

- `ai-coop\stage1\dxgi_proxy.cpp` + `dxgi_proxy.def` — a **dxgi.dll proxy**,
  x86 (cl /LD /O2 via vcvarsall x86), no third-party deps. Source: workspace.
- Forwards to real `C:\Windows\System32\dxgi.dll` (lazy LoadLibrary on first
  export): `CreateDXGIFactory`, `CreateDXGIFactory1`, **`CreateDXGIFactory2`**,
  `DXGIGetDebugInterface1`, `DXGIDeclareAdapterRemovalSupport`.
  - First run died `0xC0000139` "CreateDXGIFactory2 could not be located in
    C:\Windows\System32\d3d11.dll": the system 32-bit **d3d11.dll also imports
    from dxgi.dll** (its only dxgi import is `CreateDXGIFactory2`, verified with
    `dumpbin /imports C:\Windows\SysWOW64\d3d11.dll`) and it resolves against
    OUR proxy in the app dir. Lesson for any dxgi proxy: forward everything the
    in-process system consumers need, not just the game's own imports.
- On `DLL_PROCESS_ATTACH`: creates `<test dir>\stage1-probe.log`, IAT-patches
  the main module's `USER32.dll` imports (`PeekMessageA`, `PeekMessageW`,
  `PostQuitMessage`) with logging wrappers (Far Cry 2 playbook pattern — plain
  PE parsing + in-place thunk rewrite, no MinHook needed), snapshots loaded
  modules. No `LoadLibrary` work in DllMain (real dxgi deferred to first export).

### Steam handoff fix (required to stay in the test copy)

- `$env:SteamAppId = '214490'` before `Start-Process` suppresses the
  `SteamAPI_RestartAppIfNecessary` handoff. Verified: exactly 1 AI process,
  path = test copy, no G: launch. (Alternative if ever needed: a steam_api.dll
  proxy forcing that function to return FALSE.)

### Run results (assertions from `stage1-probe.log`, 2026-10-20 22:59)

| Check | Result |
|---|---|
| Probe attaches in test-copy process | ✔ attach line, correct path, pid 49788 |
| IAT patches | ✔ 3/3 "OK" |
| dxgi forwarding | ✔ `CreateDXGIFactory/1` calls `hr=00000000` (renderer initialized) |
| Message pump observed live | ✔ **PeekA=27,458 ticks** over 72.7 s uptime, log ends with detach line |
| Process count during run | ✔ exactly 1, from the test copy |
| Real install footprint | ✔ 0 files modified in G: install during whole experiment |
| Exit | ✔ graceful (CloseMainWindow), detach line written |

- Deployed artifact: `test-install\Alien-Isolation\dxgi.dll`
  (sha256 recorded in session log; rebuilt versions overwrite this copy of our
  own file — nothing of the game's is ever replaced: dxgi.dll did not exist in
  the install).
- QA level for this stage: **manual boot test with log assertions** (not yet an
  automated suite). No bots yet (playbook/automated-qa.md is Phase 6).

### Stage 2 candidates (next)

1. Hook the named-event firehose (script seam `FUN_0050e4f0` / command table) to
   log Composite commands as they fire in gameplay.
2. Locate and log the player position/entity each N frames (memory seam —
   reverse-engineering.md "find a value in memory" dance in the live process).
3. Both on top of the existing proxy; still test-copy only.

## Stage 2 — Load-vector correction + seam probe (2026-10-21)

### The dxgi-proxy load vector does not reach gameplay

- The game **re-execs itself from the registered install path**: on launch, the
  test-copy `AI.exe` calls `SteamAPI_RestartAppIfNecessary`, Steam relaunches from
  `<SteamLibrary>\steamapps\common\Alien Isolation`, so a `dxgi.dll` dropped in
  the test copy is never loaded by the process that actually runs gameplay.
- Tried a registry redirect
  (`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 214490\InstallLocation`)
  → **did not work**; Steam resolves the install via
  `libraryfolders.vdf` + `appmanifest_214490.acf`, not that key. Registry was
  **restored** to the `G:` path. Getting the test copy to run under Steam needs
  a second Steam library / a copied app manifest — an install-level change we did
  not make.
- **Adopted load vector (proven):** external x86 injector
  (`injector.exe`, `CreateRemoteThread` + `LoadLibraryA`) targets the already
  running `AI.exe` and loads `ai_probe.dll`. No install mutation at all; works
  against the `G:` process exactly as it would against the test copy.
  This matches the community pattern already proven for A:I (CinematicTools uses
  nefarius/Injector + a MinHook DLL — see Stage 1 notes).

### Tooling built (all small, pinned, in-repo)

`ai-coop\stage2\inject\` — `probe.cpp` (+`build_inject.bat`), `injector.cpp`
(+`build_injector.bat`), `snapshotter.cpp`/`regions.cpp`/`pospoll.cpp` (+build
scripts). All x86 via `cl` from VS 2026 Community.

### Probe seams

- **`call_method`** (RVA `0x526680`, thiscall, `ret 0Ch`) — EntityManager entity
  method dispatch. Fires **~58,000 calls / 50 s** in normal play. Arguments are
  smart pointers (caller pushes entity, entity, message). `this` is a stable
  context (e.g. `1C966F80`); entity pointers cluster in `1C59xxxx`. Useful as a
  firehose for the entity set, but **the player's transform is not among the
  entities seen here** (`this+0xC0..0xD4` is a static transform unrelated to the
  player).
- **`send_event`** (RVA `0x10E3B0`, cdecl 5 args) — hooked OK but fires
  **0 events in normal play** (only 2 call sites in the binary). Keep the hook for
  now; the `call_method` stream is the live seam that matters.
- **IAT hooks** (Stage 1, kept): `USER32.dll!PeekMessageA/W`, `PostQuitMessage`
  — 3/3 patch OK, drives graceful shutdown detection.

### Boot fix

- First injected build died `0xC0000142` at load: an uninitialized
  `CRITICAL_SECTION` was used before `InitializeCriticalSection`. Fix:
  `InitializeCriticalSection` first thing in `Init()`, a `g_csInit` guard so
  `Logf` is safe before that, and `Sleep(250)` before `MH_Initialize` (the probe
  thread is created from `DllMain`, so it must not race the loader lock).
- After the fix: `MH_Initialize`/`MH_CreateHook`/`MH_EnableHook` all `MH_OK`,
  renderer initializes, game stays healthy (~1.4 GB working set), graceful exit.

### Load-vector caveat (Defender)

- Windows Defender quarantined the injector **from the project folder** (the
  combination of `CreateRemoteThread` + `LoadLibrary` in the workspace path trips a
  heuristic). An elevated `Add-MpPreference -ExclusionPath` was attempted; the file
  was removed anyway. **Workaround:** run the injector + DLL from
  `%LOCALAPPDATA%\Temp\opencode\probe-inject` — it runs there with no
  interference. Note for anyone reproducing: build in-repo, execute from a trusted
  temp dir.

## Stage 2b — Player world position FOUND (verified)

Method: memory-diff (reverse-engineering.md "find a value in memory" dance).

1. `snapshotter.exe` dumps the private heap region `0x1B000000 + 0x04000000`
   (64 MB) from the live process.
2. Take a snapshot while standing, one after a **deliberate walk**, then a third
   pair with a **short** deliberate walk.
3. Diff 4-aligned float triples between snapshots and rank by how many addresses
   share the **same displacement vector**.

Result — four addresses moved with an identical, coherent displacement of
`(-2.75, 0.0, 1.84)` (≈3.3 units) during the short walk:

| Address | Layout | Role |
|---|---|---|
| `0x1CA467C0` | `[pos][fwd]` | authoritative copy |
| `0x1D5FB2A4` | `[pos][fwd]` | authoritative copy (bit-identical to above) |
| `0x1D5F9B44` | `[pos][fwd][pos...]` | nearby entity, Y ≈ 0.02 lower |
| `0x1D5F9B60` | `[pos][fwd]` | nearby entity, Y ≈ 0.79 lower (standing NPC) |

- **Layout:** `float3 position` immediately followed by `float3 facing`
  (a clean unit vector, e.g. `(-0.513, -0.311, 0.800)`). The position is the
  **first 3 floats** of the record.
- **Verified three ways:** (a) two walks produce coherent XZ translation with
  **Y constant**; (b) the two authoritative copies stay bit-identical; (c) live
  polling (`pospoll.exe`, and the probe's `pos[]` lines) shows the pair updating
  in real time while idle (breathing bob 6.5520 ↔ 6.5517) and jumping coherently
  during movement.
- **Live values (idle):** position `(6.552, 9.181, 11.429)`, facing
  `(-0.513, -0.311, 0.800)`.
- The probe now polls all four addresses each scan pass (`PosPollPass`,
   4 s interval) and logs `pos[i] <addr> = (x,y,z) fwd=(...)`.

### Address-mapping notes (for future RE on this binary)

- Ghidra/decomp/TSV addresses are **VAs with image base `0x400000`**
  (RVA = VA − 0x400000).
- Strings dump addresses are **file offsets**; `.text` file→RVA = +0xC00,
  `.rdata` file→VA = file + 0x401A00. `dumpbin` prints VAs.
- Runtime seam address = **ASLR base + RVA** (observed `AI.exe` base `0x00900000`;
  so `call_method` at runtime = `0x00900000 + 0x526680 = 0x00E26680`).
- The probe computes seam addresses from `GetModuleHandleA(NULL)` at runtime, so
  it does not depend on the observed base.

### Stage 2 status

- **Both seams instrumented:** event firehose (`call_method`, live) + live player
  position (`0x1CA467C0` / `0x1D5FB2A4`, verified).
- **Stage 2 verified.** Remaining for Stage 3: interpret the EntityManager to map
  entity pointers → gameplay objects, and pick the networking model (Phase 4).

## Stage 3 — EntityManager / script-command layer mapped (2026-10-21)

Static work on the decompilation (`decomp\AI.exe.c`, 66,811 functions). All
addresses below are **VAs with base 0x400000** (RVA = VA − 0x400000).

### The live seam is the Composite command system, not `send_event`

- `FUN_00926680` (`CATHODE:EntityManager::call_method`, RVA 0x526680) is the
  central dispatch: `call_method(entitySmartPtr, entitySmartPtr, messageSmartPtr)`
  (thiscall, `ret 0Ch`). It is named at runtime (`FUN_00e1bcc0("CATHODE:EntityManager::call_method")`).
- Smart-pointer layout: arg = `{ctrlBlockPtr, ...}`; the **object** is at
  `*arg + 0xC`. Refcount at `arg[1]`; vtable at `*object`.
- `FUN_00926a60` (`EntityManager::process_messages`) resolves a message's **target
  entity list** via vtable slot **`+0x118`**, then calls `call_method` per target.
  The message object uses `+0x8` as a type word (ASCII ids `0x41/0x43/0x44/0x55`
  = 'A'/'C'/'D'/'U' seen).
- `FUN_0091d9f0` is the **command-type dispatcher**: the message's type
  (`*param_1`) is compared against a table of per-command singleton globals
  (`DAT_0174d308`, `DAT_0174d310`, … `DAT_0174d4xx`), each mapping to a handler
  (`FUN_009366d0`, `FUN_00936c60`, `FUN_00938a20`, …). This is the Composite
  **name→handler command table**.
- `FUN_0050e4f0` (RVA 0x10E3B0, "send_event") builds a 3-`double` argument vector
  and dispatches via `(*msg + 0x18)(msg[2], 0)`. **It fires 0× in normal play** —
  script commands queue through the `call_method` path instead. Keep the send_event
  hook dormant; `call_method` is the live seam.

### The engine's logic layer is CATHODE script commands (840 classes)

- **840 distinct `CATHODE::<Class>::vftable` symbols** are present in the binary
  (full list saved to `coop-anything-cache\cathode_classes.txt`). These are the
  complete vocabulary of the game's logic layer — every scriptable action/condition.
- Base classes: `EntityInterface`, `EntityVariableInterface`,
  `EntityMethodInterface` (reflection over named entity methods/variables),
  `EntityState`, `EntityContainer`.
- Entity classes: `Entity`, `TemporaryEntity` (runtime-spawned),
  `ZoneableEntity` / `ZoneableEntityContainer` (streaming-zone props/NPCs),
  `PlayerHasItemEntity`, `GenericHighlightEntity`, plus `ASSAULT_JOB_ENTITY`,
  `RADIOSITY_ISLAND_ENTITY`, `SOUND_*_ENTITY`.
- **Every command class is stream-serializable:** constructors are
  `FUN_008bd950`-based (base `EntityInterface` ctor) and each class has a factory
  thunk of the form `FUN_008bd920(stream, &ID_TOKEN, ...)`. `FUN_008bd920` calls
  **vtable slot `+0x394`** = the "read command from stream" virtual. Each class
  owns a 4-byte **ID token** (`DAT_0175241x`, `DAT_017523ax`, …).
  → The script-command layer has a **generic ID + deserialize protocol**: commands
  can be written to a stream and replayed. This is the primitive a sync layer needs.

### Dormant multiplayer classes (the engine shipped MP; A:I stripped the netcode)

The frame schedule `FUN_007d6a00` registers every per-frame task by name
(`FUN_00a1a220(name, callback)`). It still contains:

| Task name | Note |
|---|---|
| `NET_AI_UPDATE` | network AI stage — present in the frame loop |
| `NETZ_DISPATCH` | network dispatch stage — present in the frame loop |
| `ENTITY_MANAGER_TICK` | per-frame entity processing (`LAB_00783640`) |
| `ENTITY_MANAGER` | `FUN_00783500` |
| `ZONE_MANAGER` | streaming (RVA hint for zone sync) |

CATHODE classes with **live vftables** for multiplayer concepts:

- `ParticipatingPlayersList`, `NetPlayerCounter`, `NumConnectedPlayers`,
  `NumPlayersOnStart`, `NumDeadPlayers`, `AllPlayersReady`
- `FilterIsLocalPlayer`, `FilterIsAPlayer`, `DespawnPlayer`
- `BroadcastTrigger`, `NetworkedTimer`, `TriggerSync`, `SyncOnAllPlayers`,
  `SyncOnFirstPlayer`, `IsMultiplayerMode`, `VariableThePlayer`

→ **Derived:** Cathode as an engine family shipped multiplayer (Halo Wars 2);
A:I retains the command/player classes but no network transport. The co-op layer
does not have to invent a state model from scratch — the *command* and *player*
vocabulary already exists, and the command stream is serializable. A:I also ships
`AI_UI_NET_*` strings (Steam leaderboard / sign-in UI), evidence the net-facing UI
scaffolding survived.

### Stage 3 conclusions (feed into Phase 4)

1. **Interception point:** `call_method` (RVA 0x526680) sees every queued script
   command, with entity + message smart-pointers. This is where a host writes
   commands into a network stream and where a client applies remote commands.
2. **Serialization:** per-command ID token + vtable `+0x394` deserialize — reuse
   rather than reinvent the wire format.
3. **Player identity:** `FilterIsLocalPlayer` / `ParticipatingPlayersList` give the
   engine's own local-vs-remote concept; the confirmed position record
   (`0x1CA467C0`, `[pos][fwd]`) is the per-frame transform to replicate.
4. **Next (Phase 4):** pick the networking model (playbook `how-coop-mods-work.md`
   / `networking-steam-spacewar.md`) — host-authoritative command relay +
   transform replication looks directly supported by the above.

### Stage 3 status

- EntityManager / Composite command layer **mapped**; dormant MP classes
  **enumerated**; serialization protocol **identified**.

### Stage 3 runtime verification (probe `ai_probe3`, 2026-10-21)

Enhanced the `call_method` hook to resolve, per call: the **target object**
(`*(a1 smartptr) + 0xC`) and the **message type** (`*(a3 smartptr) + 0x8`) and its
object pointer (`+0xC`), then histogram the type words. Observed live in gameplay:

- Message type words are **per-command-class vtable addresses**, stable and
  repeated (e.g. `0cf735e0`, `0cf73450`, `0cf73510`, `0cfa1930`, `0cf7d2a0`, …).
  **64 distinct message types** appear within ~1 s of play — a rich live command
  vocabulary, matching the static `FUN_0091d9f0` command-table finding.
- **Target objects live in a different region** (`0x18ABxxxx`, `0x1202xxxx`) from
  the message/entity control blocks (`0x1C59xxxx`), and the *target set is small and
  stable* during a moment of play (a handful of objects receive repeated calls).
- **Message objects are pooled and retargeted**: only ~16 distinct message-object
  (`msgCtrl`) addresses over thousands of calls, each carrying different type words
  at different times. → **A sync relay must key commands by captured *type* +
  payload at call time, never by message-object address.**
- Method used: inject `ai_probe3.dll` (probe build whose log name derives from the
  DLL filename, so several probe builds can co-exist in one process), read
  `msgtype NEW:` and `cm #` lines.

→ **Verdict:** the `call_method` seam yields every queued script command with a
resolvable type and target — the interception point Phase 4 assumes is real.

## Phase 4 — Networking model chosen (2026-10-21)

Per `playbook/networking.md`. A:I has **no live UDP netcode to tunnel** — the
net-layer strings and `NET_*` frame stages exist, but there is no transport (see
Stage 3). So section 4 (local UDP proxy) does **not** apply; this is section 1
(sync models) work.

### Chosen model (playbook defaults, adapted to the Stage 3 findings)

- **Player avatars: client-authoritative, snapshot-interpolated on peers.**
  Each player's own avatar (position + facing) is simulated locally and replicated
  by the host to the others. The confirmed per-frame transform record
  (`0x1CA467C0`, `[float3 pos][float3 fwd]`) is the datum; send it unreliable
  (stale positions are worthless — playbook §1.2), quantized per §1.2.
- **Script commands / story state: host-authoritative relay.** The whole Composite
  command layer is serializable (per-class ID token + vtable `+0x394` deserialize,
  Stage 3), so the host captures commands at `call_method` (RVA 0x526680), writes
  them to a stream, and clients replay them. This is the playbook's
  "quest and story flags — the highest-value hook" (§1.3) in this engine's own
  vocabulary.
- **NPC/enemy ownership:** one owner per NPC with transfer, per
  `playbook/actor-ownership.md`. Deferred to build; the entity set is visible via
  the `call_method` firehose and `ZoneableEntity`.
- **Transport:** Steam relay via `ISteamNetworkingMessages` (AppID 480 Spacewar for
  the prototype) per `networking-steam-spacewar.md`, **fallback** Tailscale/direct-IP.
  Keep direct-IP working (easiest to test). A:I already imports `steam_api.dll`,
  so the Steam path is native to the process.
- **Testing:** mock steam DLL + two local instances, `clumsy` for bad-network
  profiles (§5). Reuse `tools/` sync-testing utilities at Phase 6.

### Playbook gaps found (to propose in the PR)

1. **`playbook/engine-families.md` has no Cathode entry.** Cathode (Creative
   Assembly) shipped multiplayer in Halo Wars 2; A:I retains the command/player
   class vocabulary and a serializable command stream. A "Cathode" row would let
   the next agent skip the rediscovery: *games:* Alien: Isolation (gap), Halo Wars 2
   (official MP); *what carries over:* the Composite command layer (840 classes,
   per-class ID + `+0x394` deserialize), `EntityManager::call_method` as the
   dispatch seam, the dormant `ParticipatingPlayersList`/`FilterIsLocalPlayer`
   classes, and the `NET_AI_UPDATE`/`NETZ_DISPATCH` frame stages.
2. **`data/directory.yaml` engine entry for Alien: Isolation should read "Cathode"**,
   not "Creative Assembly proprietary" (Phase 3 correction).

### Phase 4 status

- Model + transport **chosen and grounded in the Stage 3 findings**. Next:
  Phase 5 build — start with the avatar replication loop (position record) and the
  `call_method` command relay, tested without the game first (fake world), then in
  two local instances.

## Phase 5 — Build: avatar sync (2026-10-21, in progress)

Sources live in the separate `ai-coop` project folder (not in this hub repo, per
AGENTS.md rule 7): `ai-coop\stage5\proto\` (protocol + interpolation, header-only)
and `ai-coop\stage5\test\` (harness + in-game capture).

### Protocol (`stage5\proto\sync_proto.h`, `interp.h`)

- `AvatarState { id, x, y, z, yaw, tick }`; `AvatarWire` quantizes position to
  24-bit fixed point at 2^16 units/metre (≈0.06 mm resolution over ±167 m) and yaw
  to 1 byte (256 steps ≈ 1.4°). Dependency-free and host-testable.
- `InterpBuffer` implements **snapshot interpolation**: keeps a bounded history
  keyed by arrival time, renders `renderDelayMs` behind the newest sample, lerps
  between the bracketing snapshots, drops reordered/duplicate ticks (positions are
  unreliable, per playbook §1.2).

### Fake-world test (`stage5\test\fake_world_test.c`) — PASSED

No game, no network: two simulated worlds exchange wire records over an in-memory
link with configurable latency/jitter/loss. `fake_world_test.exe` results:

| Test | Result |
|---|---|
| encode/decode round-trip | ✔ pos err 1.5e-5 m, yaw err 0.011 rad |
| clean link (30 ms) tracking | ✔ worst error 0.173 m (< 0.5) |
| lossy link (60 ms + 25 ms jitter + 20 % loss) | ✔ 779 poses, 0 NaN |
| duplicate/out-of-order drop | ✔ older & dup ticks dropped |
| yaw wrap over full turn | ✔ worst error 0.0123 rad |

### In-game integration (`stage5\test\sync_capture.cpp`) — VERIFIED live

The injected probe now round-trips the **live** player transform through the
protocol each pass (capture → encode → decode → interpolate) and logs the
end-to-end error. Result against real gameplay:

```
pos[1] 1d5fb2a4 = (6.5801, 9.1852, 11.4342) fwd=(-0.7340, -0.2368, 0.6365)
sync: render=(6.5801, 9.1852, 11.4342) yaw=-0.8468 err=0.000010 avg=0.000009 n=3
```

- Error **~1e-5 m** — pure quantisation, matching the fake-world prediction.
- Yaw `-0.8468` rad matches `atan2(fwd.x, fwd.z)` for the live facing vector,
  and follows when the player turns.
- The capture path reads `0x1CA467C0` (pos) + `0x1D5FB2A4+0xC` (facing, the clean
  unit-vector copy).

### Phase 5 status / next

- **Done:** protocol, interpolation, fake-world tests (all passing), in-game
  capture verified live, **real UDP transport verified between two processes**.
- **Next:** real transport between two instances — direct UDP now proven at the
  transport level; next is wiring it into the game (two instances) with a **mock
  `steam_api.dll`** so two local game processes can handshake; then the Steam
  relay path. After that, the `call_method` command relay for story/quest state.

### Transport (`stage5\proto\transport.h`) — VERIFIED between two processes

Plain Winsock UDP (the playbook's direct-IP fallback, and the layer the Steam
relay will slot behind). Framing: `PktHeader {kind, version, seq, fromId}` +
`AvatarWire`. `PKT_HELLO` / `PKT_AVATAR` / `PKT_BYE`; avatar samples are sent
**unreliable** (never retransmitted); the socket is non-blocking; the receiver
learns its peer from the first datagram (client autoconnect).

Test harness `stage5\test\transport_test.c` (modes `loopback`, `sink <port>`,
`source <host> <port>`), built with `build_transport.bat`:

| Test | Result |
|---|---|
| in-process loopback (200 avatars, real UDP) | ✔ 199/200 received, worst err 1.2e-5 m, in-order |
| two-process source→sink (600 avatars, real UDP) | ✔ 600/600 received, tick 0..599 intact, yaw 2.565 (path 2.554), smooth render |

### Bug found by the transport test: `AvatarWire` padding

The first two-process run exposed a real defect that the fake-world test could
**not** catch (it copies whole structs): `sizeof(AvatarWire)` is **24 bytes** on
MSVC/x86 due to 3 bytes of padding inserted before `tick` (which needs 4-byte
alignment after the 1-byte `qyaw`). The transport had hardcoded a 16-byte record,
so the receiver decoded garbage `tick`/`yaw` while positions (the head of the
struct) looked fine.

Fix: `AvatarWire` is now `#pragma pack(1)` (exactly **18 bytes**: id 1 + qx/qy/qz 4
each + qyaw 1 + tick 4) with a compile-time size assertion, and the transport uses
`AVATAR_WIRE_SIZE` everywhere instead of a magic number. Verified: offsets are
`id=0 qx=1 qy=5 qz=9 qyaw=13 tick=14`, and the two-process run decodes cleanly.

→ Lesson for the write-up: wire formats must be explicitly packed and size-asserted,
and a struct-copy fake-world test does not exercise the byte layout a real
transport depends on.

### Running two local instances: the startup gate (2026-10-21)

Tried launching the test copy (`<project>\test-install\Alien-Isolation`) while the
`G:` copy was running. It **exits immediately with code 0xCACA** (51914) — Steam's
`RestartAppIfNecessary` convention. Static analysis of the gate
(`AI.exe.c` → `FUN_007dacd0` @ RVA 0x3DACD0 calls `FUN_00a540d0` @ RVA 0x6540D0,
which is the Steam boot check):

```c
cVar1 = SteamAPI_RestartAppIfNecessary(DAT_0165f6b4);
if (cVar1 != 0) { DAT_016f0ca0 = 0xcaca; return 0; }        // <-- our exit code
DAT_01be46a8 = CreateSemaphoreA(0, 1, 1, param_2);
if (GetLastError() == 0xb7 /*ERROR_ALREADY_EXISTS*/) {
    MessageBoxA("Failed to start game (App already running).\n"); return 0;
}
cVar1 = SteamAPI_Init();
if (cVar1 == 0) { MessageBoxA("Steam client must be running..."); return 0; }
puVar3 = SteamApps();
cVar1 = (**(code**)puVar3)();                                // ownership check
if (cVar1 == 0) { MessageBoxA("...does not own the game..."); return 0; }
```

The semaphore name is the literal string **`"Alien: Isolation"`** (passed at
`AI.exe.c` line 775070). → **A second local instance is blocked twice over:** the
named semaphore (`ERROR_ALREADY_EXISTS`) *and* the Steam gate. Even a perfect Steam
mock would still hit the semaphore.

`AI.exe` statically imports exactly **15 symbols from `steam_api.dll`**:
`SteamAPI_Init`, `SteamAPI_RestartAppIfNecessary`, `SteamAPI_Shutdown`,
`SteamAPI_RegisterCallResult`, `SteamAPI_UnregisterCallResult`,
`SteamAPI_RegisterCallback`, `SteamAPI_UnregisterCallback`, `SteamAPI_RunCallbacks`,
and the interface accessors `SteamApps`, `SteamUser`, `SteamUtils`, `SteamFriends`,
`SteamUserStats`, `SteamRemoteStorage`. The real DLL exports 59 (mostly accessors +
GameServer variants we don't need).

**Plan for two instances (both required):**
1. **Mock `steam_api.dll`** in the test copy only: `RestartAppIfNecessary` → 0,
   `Init` → 1, `IsSteamRunning` → 1, and the accessors return objects whose vtables
   satisfy the ownership check (`SteamApps` vtable[0] → 1) and the game's optional
   feature calls. Never installed into the owner's `G:` copy.
2. **Patch/hook the semaphore name** for the second instance (e.g. hook
   `CreateSemaphoreA` in the probe, or patch the string in the test copy's exe) so
   `ERROR_ALREADY_EXISTS` never fires when deliberately running two.

Verified so far: `AI.exe` / `steam_api.dll` identities and the import list; the gate
disassembly. Not yet built: the mock.

## Stage 5 — Two local instances: mock Steam API (2026-10-21)

Goal of this stage: get **two local instances of the game booting side by side**
so the co-op sync layer can be tested for real. Three blockers had to be cleared:
the boot gate, a crash ~1 minute into the run, and the mock's Steam ABI.

### Root cause #1 — the game must be launched with CWD = the game directory

**Observation.** The engine's mount table (`DAT_0174ba60`, count `DAT_0174ba64`)
and its search-path list (`DAT_0174ba54`, count `DAT_0174ba58`) are **empty**, so
`FUN_008b0290` falls back to the **relative** path `DATA\engine_settings.xml`,
resolved against the process's current working directory. Launch the game from
anywhere else and the settings XML never opens.

**Symptoms of getting it wrong.** `LIST_BUILT` / `REGISTER_SECTIONS` never fire,
the settings registry is never populated, and the process later dies in the
settings path — the old crash site was `AI+0x5A4D01`.

**Consequence.** A pile of accumulated **null-check exe patches** was written to
paper over that crash (settings flag `0x16512AC` → 0 at file offset `0x124eeac`,
plus null-checks at RVA `0x5A4D00`, `0x60FE49`, `0x60FE68-0x60FE6F`, `0x60FE84`,
`0x60FEB6`, `0x60FDEF`). With the CWD fixed **all of them were unnecessary and
were reverted**. The test copy now carries **only** the gate patch (see below) —
a much smaller and safer diff against the shipped exe.

**Rule for everything from here on: always launch the game with its working
directory set to the game directory.** `stage5/test/launch_two.ps1` does that
(`Start-Process -WorkingDirectory`), and so does the cdb launch.

### Root cause #2 — the crash was stack skew in the mock, not a null deref

**Symptom.** With the mock `steam_api.dll` in place the game died after
40–75 s with an access violation at `AI+0xD3A9A6`, exit code `0xC0000005`.

**Disproved first:** a libcurl attribution. `curl_easy_init` / `curl_easy_setopt`
breakpoints never fire, so the crash is not in the bundled HTTP client.

**Actual cause.** AI.exe calls Steam interface methods **callee-pop `__thiscall`**
— three arguments are pushed for `SteamFriends` vtable `+0x10` and there is **no
`add esp, 0xC`** afterwards, so the *callee* pops them. The first version of the
mock used a shared "ladder" of stubs with one fixed argument count per slot
index. Several slots had the **wrong** count, so each such call left the
caller's stack skewed by 4·N bytes, and a frame or two later the game
dereferenced a garbage pointer. **That** was the crash — not a null deref, and
not the game faulting on its own.

**Fix.** One stub per `(interface, slot)` pair, each generated by a macro so the
argument count in the signature and the argument count in the log can never
drift apart.

**Verification.** One instance under `cdb` with `sxe av` then ran **220 s with no
access violation** (it previously died at 40–75 s), and the mock log contained
**zero** unexpected slots.

### The Steam interface ABI (extracted from the disassembly, not guessed)

Every Steam interface method in AI.exe is callee-pop `__thiscall`. The rules
that produce the right answer:

* **Counting arguments:** count the `push` instructions between the **VTABLE
  load** and the **CALL**. The vtable pointer is only ever loaded by a
  **no-offset** load (`mov edx, dword ptr [eax]`); a load *with* an offset from
  an interface pointer is a field access, not a vtable. Register-save pushes live
  *before* the vtable load, and argument pushes can sit on **either side** of it
  — so the naive "pushes after the slot load" rule undercounts.
* **Slot detection:** `mov Y, dword ptr [vt_reg]` is slot `0x00`;
  `mov Y, dword ptr [vt_reg+OFF]` is slot `OFF`. Offsets below `0x10` render
  **without** the `h` suffix (`[edx+4]`, not `[edx+4h]`) — that broke an earlier
  regex.
* **Accessor calls come in two forms**, both must be handled:
  `call dword ptr ds:[IAT]` and `mov reg, dword ptr ds:[IAT]` + `call reg`.
* 100 accessor call sites were walked; all sites of a given slot agree.

| Interface | slot → args |
|---|---|
| SteamApps | `0x00`→0, `0x10`→0, `0x18`→1, `0x1C`→1, `0x4C`→1 |
| SteamUser | `0x04`→0, `0x08`→1, `0x18`→2 |
| SteamUtils | `0x24`→0, `0x44`→0 |
| SteamFriends | `0x00`→0, `0x08`→0, `0x0C`→1, `0x10`→3, `0x1C`→2, `0x20`→3, `0x58`→1, `0x5C`→3, `0x64`→2, `0x94`→2, `0x9C`→3, `0xA0`→2, `0xA4`→3 |
| SteamUserStats | `0x00`→0, `0x08`→2, `0x10`→2, `0x18`→2, `0x1C`→1, `0x20`→1, `0x28`→0, `0x34`→3, `0x54`→1, `0x5C`→1, `0x70`→5, `0x78`→6, `0x7C`→6, `0x80`→4 |
| SteamRemoteStorage | `0x00`→3, `0x04`→3, `0x08`→1, `0x0C`→1, `0x3C`→0, `0x40`→2, `0x44`→2, `0x54`→1, `0x60`→6 |

**Return values the game actually depends on** (all verified in the disassembly):

* `SteamApps +0x00` (ownership) **must be non-zero**. Both `FUN_00a540d0` and
  `FUN_00a64c10` do `if (!c) { *(int*)0 = -1; }` — a *deliberate* null
  dereference when the ownership check fails.
* `SteamFriends +0x00` must return a valid **NUL-terminated string** (the game
  `strncpy`s it into the player record).
* `SteamUser +0x08` must **write 8 bytes** to its pointer argument — the game
  reads both halves (`[ebp-10h]`, `[ebp-0Ch]`).
* `SteamApps +0x10` → `"english"`.
* `SteamUser +0x04` → 1.
* Every stub clears **EDX**, so the slots that return a 64-bit value in
  `EDX:EAX` (`SteamUtils +0x24` compares one against a stored triple) are
  deterministic.

### The mock `steam_api.dll`

`ai-coop/stage5/mock/mock_steam_api.cpp` — 32-bit MSVC, built by
`stage5/mock/build_mock.bat` (`vcvarsall.bat x86`; `cl /nologo /LD /O2 /EHsc`),
linked against `mock_steam_api.def` so the exported names are pinned. It is
**test tooling only** and is never installed into the owner's `G:` copy.

* Exports the 15 symbols AI.exe imports plus a few extras.
* `SteamAPI_Init` → 1, and builds the six interface objects;
  `SteamAPI_RestartAppIfNecessary` → 0 (never relaunch from the real install).
* One stub per `(interface, slot)`; each logs `slot <iface> +0xNN args=N
  caller_rva=0x...`.
* `StubUnknown` is the fallback for a slot that is not in the table. It is a
  naked stub that **resolves the exact slot index at runtime** by scanning the
  interface's vtable for its own address, and logs
  `UNEXPECTED SLOT iface=.. slot=+0xNN caller_rva=0x..`. It deliberately pops
  **nothing**: we do not know that call's argument count, and a wrong pop would
  turn a clear diagnosis into a mystery crash.
* Call log: `ai_mock_steam_<pid>.log` beside the exe — **one file per process**
  so two instances do not interleave. Enabled with `AIC_MOCK_LOG=1`.

### The exe patch that is left (and why it is narrow)

The boot gate `FUN_00a540d0` does, in order: `SteamAPI_RestartAppIfNecessary`
→ exit `0xCACA`; `CreateSemaphoreA(..., "Alien: Isolation")` →
`ERROR_ALREADY_EXISTS` → "App already running"; `SteamAPI_Init()` → must be
non-zero; `SteamApps()->vtable[0]()` → must be non-zero.

Only the **two blocking branches** are neutralised, by turning two conditional
jumps into unconditional ones (file offsets in `AI.exe`):

```
0x6534EB: 74 0E -> EB 0E   skip the RestartAppIfNecessary exit
0x65351B: 75 18 -> EB 18   skip the named-semaphore "already running" exit
```

Steam initialisation **still runs**, so the interface objects the rest of the
game dereferences are real. (Nuking the whole function instead was tried and
crashes the process with `0xC0000005` at RVA `0x664C21`.)

Test-copy `AI.exe` state: **original + the gate patch only**, SHA-256
`570B09B2F78DE872E5A60FCA80721D17042E39D6DC3850F90D607E8F32C1F165`
(original `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90`).

### Two instances side by side — VERIFIED

`stage5/test/launch_two.ps1` (sets CWD, `AIC_MOCK_LOG=1`, `SteamAppId=214490`,
starts N instances, waits, reports liveness + mock-log summary).

Run of 2026-10-21, two instances, 90 s:

| Check | Result |
|---|---|
| instances alive | ✔ 2 / 2 (≈1.12 GB working set each) |
| crash | ✔ none (previously 40–75 s) |
| unexpected Steam slots | ✔ 0 in each mock log |
| call sequences | ✔ identical between the two instances |
| single instance under `cdb` (`sxe av`) | ✔ 220 s, no access violation |
| each instance reaches the **main menu** | ✔ verified from a screen capture |

Menu verification was done without a human: the screen was captured and rendered
as coarse ASCII, which reads the menu text directly. Scripts:
`stage5/test/shot_instances.ps1` (launch + capture), `ascii_shot.ps1` (render),
`diff_shot.ps1` (pixel diff), `cursor_probe.ps1`.

### Saves: Alien: Isolation stores them in **Steam Remote Storage**, not on disk

Symptom (reported by the owner, reproducible on the test copy): after
`PRESS ANY KEY` the game shows **"An unexpected error occurred while saving the
game progression."** and never enters the main menu.

That string is `DATA\TEXT\ENGLISH\UI.TXT` key
`AI_UI_DLG_WSETUP_CONFIRM_SAVE_ERROR` (`UI.TXT` is UTF-16LE). There is no
`%USERPROFILE%\Documents\My Games\Alien Isolation` directory at all (checked on
this machine) — the game does not use the filesystem for saves. It calls
`ISteamRemoteStorage`, i.e. **Steam Cloud**. With real Steam the files live in
`<steam>\userdata\<accountid>\214490\remote\`.

The mock's `SteamRemoteStorage` slots returned 0, so every `FileWrite` failed.
The mock log showed exactly which calls:

```
slot SteamRemoteStorage +0x04 args=3 caller_rva=0x654C66   FileRead
slot SteamRemoteStorage +0x3C args=0 caller_rva=0x661895   GetFileCount
slot SteamRemoteStorage +0x40 args=2 caller_rva=0x6618CB   GetFileNameAndSize
slot SteamRemoteStorage +0x00 args=3 caller_rva=0x663D2E   FileWrite
```

and the failure test is visible in the caller (VA = RVA + 0x400000):

```
00A63D2C  call edx                  ; vtable+0x00 = FileWrite(name, data, size)
00A63D2E  xor  ecx,ecx
00A63D30  test al,al
00A63D32  sete cl                  ; 1 when the write FAILED
00A63D36  inc  ecx
00A63D37  mov  [esi+2F8h],ecx       ; 1 = ok, 2 = failed
```

The vtable order is the **pre-async Steamworks layout** (no
`FileWriteAsync`/`FileReadAsync`), which is what a 2014 build uses. Derived from
the call sites, not from an SDK header:

| slot | method | args | notes |
|---|---|---|---|
| `0x00` | `FileWrite` | 3 | `(name, data, size) -> bool` |
| `0x04` | `FileRead` | 3 | `(name, buf, size) -> int32`; caller does `test edx,edx / js error`, so **never return negative** |
| `0x08` | `FileForget` | 1 | |
| `0x0C` | `FileDelete` | 1 | |
| `0x2C` | `FileWriteStreamOpen` | 1 | only reached while enumerating |
| `0x3C` | `GetFileCount` | 0 | return value is a **loop bound** (`test eax,eax / jle skip`) |
| `0x40` | `GetFileNameAndSize` | 2 | `(i, *size) -> const char*` |
| `0x44` | `GetQuota` | 2 | `(*total, *avail)`, 64-bit outs |

`GetFileCount` returning 0 is why the mock never hit slot `0x2C` and reported
no unexpected slots — the enumeration loop at RVA `0xA618B0` was simply skipped
(`for (i = 0; i < GetFileCount(); i++) { name = GetFileNameAndSize(i, &size); ... vtable[0x2C](name) ... }`).

**Fix:** the mock now implements a small real cloud backed by
`<game>\steam_remote\<AIC_PROFILE>\`, one directory per instance, so the two
players in a co-op test never share save files. `FileWrite` persists and
returns true, `FileRead` returns the bytes (0 for missing), and
`GetFileCount`/`GetFileNameAndSize`/`GetQuota` report the directory contents so
saved games are found again on the next run.

Verified 2026-10-22, test copy, two instances, separate profiles:

```
rs FileRead 'input.cfg' -> missing
rs GetFileCount -> 0
rs FileWrite 'PROGRESSION.ais' 10240 bytes -> ok
rs GetFileCount -> 1
rs FileWrite 'MSSV01.ais' 1048576 bytes -> ok
rs GetFileCount -> 2
rs FileWrite 'CHSV00.ais' 2097152 bytes -> ok
```

`steam_remote\default\` then contained `PROGRESSION.ais`, `MSSV01.ais`,
`CHSV00.ais`; a second instance with `AIC_PROFILE=p2` created its own
`PROGRESSION.ais`. Both sat in the main menu with **no save error** (OCR of the
screen), 0 unexpected slots.

Save file names: `PROGRESSION.ais` (profile, 10 KB), `MSSV0n.ais` (mission
save, here 1 MB) and `CHSV0n.ais` (checkpoint save, here 2 MB). The `.cfg` /
`.mmm` suffixes seen in the disassembly belong to other files (`input.cfg` is
read at startup; `.mmm` was not exercised).

### Closing the loop on menu verification

An earlier attempt at reading the menu with coarse ASCII was actually reading a
**text editor on the other display**. Two lessons, both now baked into the
tooling:

* identify the window under test before interpreting pixels
  (`enum_windows.ps1` prints class/title/rect/foreground per process);
* use real OCR. `stage5/test/ocr.ps1` drives the Windows built-in engine
  (`Windows.Media.Ocr`, no extra software, no network) over a cropped, upscaled
  PNG and prints each line with its **image-space centre**, which is what the
  click command consumes. It reads this game's menu text reliably.

The game presents fullscreen on DISPLAY2 (`(0,0)-(3440,1440)`), so captures of
the *virtual* screen hold the game in the **bottom half** (image row 1440 =
screen y 0). `keys.ps1` focuses the game's `Alien: Isolation` window and sends real
keystrokes with `keybd_event` (`-Key`, `-Press`, `-Hold`, `-MouseToX/-MouseToY`,
`-Click`), because the game reads raw input from the foreground window and
ignoring `PostMessage` is correct. Scan codes matter -- see the windowed section
below.

### Tooling notes / gotchas

* **Windows Defender quarantined `injector.exe`** (the Stage-2
  `CreateRemoteThread`+`LoadLibrary` injector) from the workspace *and* from
  `%LOCALAPPDATA%\Temp\opencode\probe-inject`. Without elevation an exclusion
  cannot be added (`Get-MpPreference` needs administrator), so the external
  injector is currently unusable. The probe has to be loaded another way — the
  mock `steam_api.dll` is already in the process and can `LoadLibrary` a test
  DLL itself, or the load vector moves to a `steam_api.dll` **proxy** that
  forwards to the real one (the classic, and it avoids the injector entirely).
* `cdbX86.exe` is on `PATH` (WinDbg store app). cdb launches must also set the
  working directory to the game directory.
* The game is 32-bit x86 with ASLR: always talk about `AI+RVA`, never an
  absolute VA.
* PowerShell here has no `tail`; use `Select-Object -Last N`. Inline
  `python -c` with newlines fails — write a `.py` file and run it with
  `%LOCALAPPDATA%\Programs\Python\Python312\python.exe`.

### Windowed mode: two instances side by side on one display

Two *exclusive-fullscreen* instances on one display fight over input (only the
foreground one receives it). Windowed mode fixes that, so the test setup runs
both instances as ordinary windows tiled across the primary display.

In-game path: **Options > Video > Screen Size**, then Left/Right to step it
through `Fullscreen` -> `Windowed Fullscreen` -> `Windowed`. A "Video settings
changed / would you like to apply and save?" dialog appears when you back out,
and the change only takes effect after **Yes** (the window does not resize
before that).

`Windowed Resolution` is a second row that only appears once Screen Size is
`Windowed`. Left/Right step it and the list **wraps** (Right from 1920x1200
goes to 800x600). The list is ordered by ascending precedence:

    800x600, 1280x720, 1280x768, 1280x800, 1280x960, 1280x1024, 1360x768,
    1360x1024, 1366x768, 1400x1050, 1440x900, 1600x1200, 1680x1050,
    1920x1080, 1920x1200

Keep it at **1680x1050** so two windows fit a 3440x1440 display
(2 x 1686 including the frame = 3372).

The game **owns its window size** and re-asserts the configured windowed
resolution within a frame, so a `SetWindowPos` resize is reverted. Window
**position** is left alone, so moving works -- `tile.ps1` therefore only ever
passes `SWP_NOSIZE`:

    powershell -File tile.ps1 -Display 1     # -> (0,0) and (1720,0)

Verified: two instances (separate `AIC_PROFILE`s) at (0,0) and (1720,0), each
1686x1079 (client 1680x1050), both at the main menu, and a Down press sent to
one window moves only that window's selection.

Where the choice is stored: `DATA\engine_settings_savefile.bin`, 248 bytes --

    uint32 header (0x13)
    30 x { uint32 value; uint32 slot }    slots 0..28 and 30 (29 unused)
    uint32 trailer (0x110d02d9)

Slots seen changing: slot 6 is `Vertical Sync` (toggling it rewrites the slot)
and slot 8 is `Windowed Resolution` (1920x1200 -> 800x600 rewrote it). The
`value` field is *not* a plain hash of the quality name -- CRC32, FNV-1/1a,
djb2, sdbm and Jenkins over `On`/`Off` all miss -- so do not hand-edit this
file; drive the UI, or decode the engine's own id function first. The file is
read at startup: a relaunch comes back at the saved 1680x1050 windowed size.

Input gotchas that cost real time:

* The game reads **scan codes** (raw input / DirectInput). `keybd_event` with
  `bScan = 0` is accepted by Windows and then ignores the key -- pass
  `MapVirtualKey(vk, 0)`, plus `KEYEVENTF_EXTENDEDKEY` for the arrow keys.
  This is why menu navigation "worked once then stopped".
* Do not re-assert focus before every keystroke. `AttachThreadInput` +
  `SetForegroundWindow` around each press disturbed delivery; focus once, then
  send with `-NoFocus`.
* A synthetic click must **hold** the button for ~140 ms. A `mouse_event`
  down+up inside one frame is invisible to a UI that samples once per frame:
  the "apply and save" dialog ignored a 0 ms click and took a 140 ms one.
  The dialog's default button is *remembered*, so `Right`+`Enter` is a coin
  flip -- click `Yes` instead (its position comes from `ocr.ps1`).
* Rapid `-Press N` on a value row loses presses (12 Rights advanced 10 steps).
  Use 400-900 ms gaps and verify, or click.
* `ocr.ps1` cannot read the dim value column reliably at small resolutions;
  the window/client size from `enum_windows.ps1` is the trustworthy oracle for
  display-mode changes.

### Stage 5 status

* ✅ One instance boots clean under cdb for 220 s with no AV.
* ✅ **Two instances boot and run side by side, both reaching the main menu.**
* **Windowed mode works, tiled side by side on one display**: 1680x1050 each at
  (0,0) and (1720,0), independent input per window (verified), and the setting
  survives a relaunch. This is the test rig for everything below.
* ✅ **Saves work**: the mock implements Steam Remote Storage for real, so the
  profile and saves persist per instance (`steam_remote\<profile>\`). The
  "unexpected error while saving the game progression" blocker is gone.
* ✅ **A bot drives the menus into a level** with no human input: the footer's
  left-hand label names the selected item, so the bot OCRs it and only presses
  keys until the wanted item is highlighted. That is what stops it landing on
  "Survivor Mode" by accident.
* ✅ **The player transform's layout is decoded** (see below).
* Next: find the *live* transform in memory, then wire it to the UDP transport.

### The player's transform

The save system keeps the player transform in the same *named value* store the
rest of the game's persistent variables live in. The value is called
`"player_pos"`, is `0x30` bytes long, and is read by exactly one function -
the save-state gatherer `FUN_00927f70` (RVA `0x527F70`..`0x529B20`).

Its bytes are a **4x3 row-major world matrix**, translation in the last
column:

    f0  f1  f2  tx
    f4  f5  f6  ty
    f8  f9  f10 tz

so the basis columns are `(f0,f4,f8)`, `(f1,f5,f9)`, `(f2,f6,f10)` and the
position is `(f3,f7,f11)`. Verified from the byte dump of a real save: the
three columns are unit length and mutually orthogonal to 3e-5, and the second
column is `(0, 1, 0)` - the world's up axis.

    position  = (3.4490342, 7.6275969, 11.461087)
    forward   = ( 0.7071068, -0.0000457, -0.7071068)   (third column)
    right     = (-0.7071068, -0.0000305, -0.7071068)   (first column)

**This copy is a snapshot, not live.** It is byte-identical across two
separate runs and does not change when the player walks (checked: the camera
moved 67 % of the screen pixels while the value stayed fixed), so it is
written at save time. The live transform has to be found in memory.

### Finding things inside the running game

The probe DLL (`stage5/coopnet`) now has a **control channel** on the same
UDP port it uses for avatar packets: any datagram starting with `!` is a
text command and is answered to the sender's address.

| command | what it does |
|---|---|
| `HELLO` | pid + current live-transform address |
| `FLOATS <addr> <n>` | dump `n` floats at `addr` |
| `MEM <addr> <n>` | hex dump |
| `SCANBLOB` | scan all readable memory for the saved 48-byte matrix |
| `SCANPOS` | scan for the saved position triple on a 12-byte stride (how the matrix lays its translation column out) |
| `SCANXFORM` | scan for any orthonormal 4x3 matrix, i.e. any world transform |
| `WATCHHOT <ms>` | re-read every candidate for `<ms>` and report the ones whose translation or basis moved |
| `WATCH <addr>` | use `<addr>` as the live transform from now on |

`WATCHHOT` is the useful one: it turns "find the live transform" into
"walk the player and see which matrix moves".

### Input gotchas (stage 5)

* The game reads raw input from the **foreground** window only. Keys sent
  while another window has focus are dropped silently - `W` produced a 1 %
  screen change unfocused and 67 % focused. `keys.ps1` therefore has to focus
  the target instance before every burst of keys, and with two instances only
  one of them can be driven at a time.

### Finding the live transform: the memory-scan route is a dead end

The `WATCHHOT` / `SCANXFORM` scan-and-diff approach was run to exhaustion:

* A standalone out-of-process scanner (`stage5\test\memtool.ps1`,
  `ReadProcessMemory` + `VirtualQueryEx`) snapshots all 1.53 GB of readable
  memory in 4.4 s, so no game restart is needed to experiment.
* Diffing "standing still" against "walking" left **139 624** floats that only
  changed while walking, and **340 359** that changed at all.
* Every float3-triple shape search (strides 4, 8, 12, 16, 20, 24, 32, 36, 48)
  and every heap-range filter over the candidate set returned **only GPU /
  D3D vertex-buffer noise** (`0x38C4xxxx`, `0x1B8Cxxxx`, `0x4FC6xxxx` -
  values that are identical across all three components, i.e. packed vertex
  data, not transforms).

**Conclusion (observed, not guessed): the live character transform is not
stored as a plain float3 or 3x4 matrix in a scannable heap block.** It lives
inside an engine object reached through pointers, so no amount of raw
scanning finds it - the object has to be *reached* through the engine's own
accessors. This is the general lesson: for a game with a component/entity
system, hook the accessor instead of scanning memory.

### The live transform IS reachable: bbox corners of the character entity

The save record's `UpdateXf` (`FUN_00597720`, RVA `0x197720`) rebuilds the
character's world matrix from **four bbox corner properties of the character
entity**:

```
A = FUN_008f7490(entity)   atom 0xA652ED66   (DAT_01754374)
B = FUN_008f75a0(entity)   atom 0xF0088922   (DAT_01754364)
C = FUN_008f7620(entity)   atom 0x5E1AEC47   (DAT_0175436c)
D = FUN_008f75e0(entity)   atom 0xC8DE9852   (DAT_01754368)

position = A + B                            (component wise)
facing   = normalize((C + D) - (A + B))     (the matrix' third column)
```

Each of those four getters is a two-line wrapper over **one** generic
accessor:

```c
undefined4 * FUN_008f7490(undefined4 *param_1, undefined4 param_2)
{
  *param_1 = DAT_0174df80; param_1[1] = DAT_0174df84; param_1[2] = DAT_0174df88;
  FUN_008d63c0(param_2, &DAT_01754374, param_1);
  return param_1;
}
```

`FUN_008d63c0` (RVA `0x4D63C0`) is the engine's generic
**entity-property getter** (`entity, &atom, out_float3`): it walks the
entity's component list, and for the component whose property id matches the
atom returns the stored float3. It is the single choke point for every
float3 property read in the game.

**That is the difference between the two calls:** the *save record* holds a
snapshot of the matrix (written at level load, byte-identical afterwards),
but `FUN_008d63c0` reads the **live** entity. Polling the four bbox atoms
gives the live world position.

Entity plumbing (needed to call it correctly):

* `FUN_00597720`'s first argument is an **entity smart-pointer slot**
  (`*(void**)slot` = the entity, `slot[1]` = refcount); it is stable because
  the record holds a reference (`FUN_00939dc0` bumps the refcount and
  registers the entity).
* `FUN_00939dc0(int *param_1, undefined4 param_2, char param_3)` increments
  `*(int*)(*param_1 + 4)` - it *acquires* a reference, it does not release.
* The four atoms live in `.data` at `DAT_01754364..DAT_01754374`
  (RVA `0x1354364..0x1354374`).

### A dormant networked-character layer (found in `.data`)

A contiguous name table at `0x012F6D58`-`0x012F7120` (pointer table at
`0x016413B0`-`0x01641418`, one 0x01 flag byte per entry) holds the engine's
**network message names**:

```
character_register_network_character_data      character_register_accurate_pos
character_register_extrapolated_network_position  request_display_character
request_logic_character                        client_entity_manager_receive_message
character_send_message_to_logic                discontinuous_positioning
physics_object_update_networked_object         start_ragdoll_death
```

These are *message names in a data table*, not script functions - script
API names (e.g. `SetExitMarkerObjectivePos`) are inlined string literals in
the code, so they show up in a Ghidra decompile while the message names
never do. So the engine has a networked-character layer, but nothing in the
single-player build drives it. Worth noting for the write-up: the message
catalogue is a good map of what the engine *could* do over the wire.

### Phase 5 VERIFIED: live per-frame player position over UDP (2026-10-22)

The avatar loop now runs on the **real** per-frame transform, not on a save
snapshot. The live transform is reached through the engine's own manager
graph rather than by scanning:

```
mgr   = *(uint32_t *)0x016F0C88          ; RVA 0x12F0C88, the engine manager
blk   = *(uint32_t *)(mgr + 0x58)        ; the player's transform block
rec0  = blk + 0x78C                     ; 40-byte transform records
```

Each 40-byte record decodes as:

| offset | meaning |
|---|---|
| +0x00 | float, constant `0.07875` |
| +0x04 | float[3] forward / facing (unit) |
| +0x10 | float[3] world position |
| +0x1C | float, constant `47.0` |
| +0x20 | float, constant `0.05` |
| +0x24 | float, constant `1000.0` |

So the live position of player *n* is at `blk + 0x79C + 0x40*n` and the facing at
`blk + 0x790 + 0x40*n`. (A 4th record sits at `blk + 0x884`.) The three
constant floats are a cheap **signature**: `rec[7] == 47`, `rec[8] == 0.05`,
`rec[9] == 1000` confirms the block is the transform block and not something
else that happens to look like a position.

**The `forward` field is the player's yaw.** This was measured, not assumed: an
800 px relative mouse turn moved it from `(0.997, 0, -0.077)` to
`(-0.776, 0, 0.631)`, i.e. **145 deg of yaw**. An earlier attempt to measure
this failed only because relative mouse injection was broken (a C# `const
MOVE` shadowed the `Move` method in `keys.ps1`); once that was fixed the yaw
tracks the mouse exactly.

> **Warning that cost a crash:** never call the engine's generic entity
> property getter `FUN_008d63c0` (RVA 0x4D63C0) from injected code. It bumps
> a byte counter on the entity owner; unreleased calls overflow at 256 and the
> engine dies with `0xC0000005` at `AI.exe+0x009d40c2`. The manager path above
> avoids it entirely.

**End-to-end test** (two instances, real UDP, no human input):

| check | result |
|---|---|
| p1 publishes its live position | yes, every frame |
| p2 receives it | yes, `REMOTE avatar id=1 pos=(...) yaw=... tick=...` |
| position changes continuously while p1 walks | yes, 400+ distinct samples |
| travel over a 3.5 s walk | ~4.8 m, smooth |
| yaw transmitted | yes, `yaw=...` |

The probe's `!LIVE` control command reports the live reader state:
`LIVE have=1 slot=0248F46C ctx=12D07730 pos=(3.510,9.194,11.062) fwd=(0.997,0.000,-0.077)`.

### Phase 6 groundwork: the engine's camera struct (2026-10-22)

The engine's own world->screen projection lives in two functions,
`FUN_00826b60` (RVA 0x426B60, "SetItemPos") and `FUN_00826f50` (RVA 0x426F50,
"SetObjectivePos"). Both read the same camera object and both prove its
layout:

```
cam = *(uint32_t *)(*(uint32_t *)(mgr + 0x44) + 0xbbc)

+0x18  float     camera x
+0x1c  float     camera y
+0x20  float     camera z
+0x24  float[3]  screen-right axis (unit)
+0x30  float[3]  screen-up axis    (unit)
+0x3c  float     vertical projection divisor
+0x40  float     horizontal projection divisor
```

and project a world point with

```
sx = 1 - (right . d) / cam[0x40]
sy = 1 - (up    . d) / cam[0x3c]        d = world - cam
```

The first 6 dwords of the camera object (`+0x00..+0x17`) are 6 `char*` name
slots: `FUN_0080b670` and `FUN_00835980` both walk them as an array of 6
strings, and the camera object is selected from a small pool of named cameras.

**The camera at that address is a static top-down debug camera**, not the
gameplay camera:

| field | value |
|---|---|
| position | (83.7667, 7.7939, -52.5946) |
| right | (0, 0, 1) |
| up | (-1, 0, 0) |
| vdiv / hdiv | 88.88889 / 51.85185 |

It does **not** move when the mouse turns (measured: an 800 px turn changed the
player yaw by 145 deg and left every camera field bit-identical), and it sits
~102 m from the live player position. A memory scan for the same signature
within 12 m of the player over the whole heap (`0x01000000`-`0x30000000`)
returns **0 hits**, and walking the manager's pointer graph two levels deep
returns 0 cameras as well.

**Derived (not yet proven):** the gameplay camera is not reached through
`mgr + 0x44`. `FUN_0080b640` is the setter for that slot, and its only callers
are `FUN_0050ce00`, `FUN_00512f80` and `FUN_005130d0` - the latter two pass
`*(undefined4 *)(in_ECX + 0xc)`, i.e. a camera pointer held by a *camera
object*, not by the manager. So the gameplay camera is owned by an object
reachable through some other path, and the manager slot is a HUD/script
camera. This is the thing Phase 6 has to find.

Useful tools written for this (`stage5/test/`):

* `camdump.py` - resolves `mgr -> mgr+0x44 -> +0xbbc` and dumps the camera
  struct with unit-length / orthogonality / divisor checks.
* `camsig.py` - heap scan for camera structs by signature, with an optional
  `--near x,y,z --radius` filter.
* `camscan2.py` - walks the manager graph and applies the camera signature to
  every pointer it finds.
* `decompq.py` - query the 73 MB Ghidra decomp:
  `body FUN_00826f50 80`, `callers FUN_0080b640`, `grep "0xbbc"`.

### Owner requirement: the two instances always stay side by side on one screen

The owner asked (2026-10-22) that the two instances **always stay side by side
on one screen**. The rig now enforces this:

* the game is windowed at **1680x1050** (client), so two frames fit a
  3440x1440 display (2 x 1686 = 3372 px);
* `tile.ps1 -Primary` places the windows at **x = 0** and **x = 1720** on the
  **primary** display (DISPLAY2 at 0,0), in PID order (the lower PID, i.e.
  player 1, goes leftmost);
* `tile.ps1` only ever moves windows (`SWP_NOSIZE`) - the game owns its size
  and re-asserts the configured windowed resolution within a frame;
* `tile.ps1` calls `ShowWindow(SW_RESTORE)` before positioning, because the
  game minimises itself when it loses focus;
* `drive_level.ps1` calls the tiler after launching **and** after the menu
  navigation, so a fresh run always ends up tiled.

**Trap worth recording:** a minimised instance that is later restored comes
back at the **desktop resolution** (3440x1440), which looks like the game
switched to fullscreen. It did not - `DATA\engine_settings_savefile.bin` was
untouched since 11:11 and the windowed setting is still 1680x1050. A fresh
launch always yields 1686x1079 windows; the fix is to restore, never to resize.

### Standalone memory scanner (`stage5\test\memtool.ps1`)

Out-of-process `ReadProcessMemory` + `VirtualQueryEx` tool that needs no
game restart: `regions`, `bytes` (pattern search), `float`, `dump`, `diff`.
Gotchas worth recording:

* A 64-bit caller gets the **64-bit** `MEMORY_BASIC_INFORMATION` (48 bytes)
  even for a 32-bit target, so `VirtualQueryEx` must be called with
  `lpLength = 48` and the C# struct must not declare an explicit padding
  field, or `Marshal.SizeOf` != 48.
* `ulong + int` is invalid in C# - cast the int to `(ulong)` first.
* 1.5 GB snapshot ≈ 4.4 s, so with a 15 s window the walk has to start
  about 7 s in.

### Phase 6: the gameplay view-projection is not in the heap — capture the pipeline

The camera hunt through memory scanning ran out of road. `vpfind` (structural
VP test: scaled-orthonormal basis, 4th basis parallel to the 3rd,
`M[3][3] == k*(M[3][2]+1)`) is clean and validated, but over a full 2.5 GB
snapshot it only ever returns UI/ortho matrices (`cam.y == 0`, aspect ≈ 1.6,
look ≈ 88°) plus one 9.45 m hit at the wrong aspect. The combined
view-projection is therefore **not stored as a contiguous 16-float block in
process memory**; the renderer almost certainly builds it per frame and uploads
it to a D3D11 constant buffer.

So the probe now watches the D3D11 pipeline instead of the heap
(`stage5/coopnet/coopnet.cpp`, section "D3D11 constant buffer capture"):

* `DllMain` hooks `D3D11CreateDeviceAndSwapChain` and `D3D11CreateDevice` from
  `d3d11.dll` **synchronously**, before the game's first D3D call. The loader
  has already mapped `d3d11.dll` (AI.exe imports it), so this needs no
  `LoadLibrary`.
* The create hook takes the device and immediate context and hooks four
  shared vtable slots with MinHook. Vtable indices below are from
  `d3d11.h`, not guessed: `ID3D11Device::CreateBuffer` = 3,
  `ID3D11DeviceContext::{Map, Unmap, UpdateSubresource}` = 14, 15, 48.
  (In C++ mode the SDK interfaces have no `lpVtbl`, so the slot has to be
  read out of the vtable pointer by index.)
* Every buffer created with `D3D11_BIND_CONSTANT_BUFFER` is registered
  (`buf`/`ByteWidth`, max 256, never released - the pointers are only ever
  compared). Fills are mirrored into a 4 KiB snapshot from all three paths:
  initial data, `UpdateSubresource`, and `Map`/`Unmap` (the mapped pointer
  belongs to the driver, so the copy has to happen **before** `Unmap`).
* A background thread runs the `vpfind` structure test over the snapshots
  once a second, in two modes: combined VP, and pure view matrix (orthonormal
  basis, 4th row/col `0,0,0,1`). The reconstructed camera has to sit within
  8 m of the live player and look at it.
* Control channel: `!CB` (summary + best), `!CBAN` (force a pass),
  `!CBSTRICT 0|1`, `!CBDUMP <bufhex>` (first 32 floats of a snapshot).

Because the create-device hooks must exist before the first device, the probe
is launched with `AIC_PROBE_DELAY_MS=0` instead of 8000. The engine hooks are
purely address-based patches, so they do not care how early they are installed.

**Verified** with `stage5/test/d3dtest.cpp` (`d3dtest.exe <coopnet.dll>`), a
throwaway harness that loads the probe exactly like the mock Steam DLL does and
then creates a real D3D11 device + swapchain and fills a constant buffer 30
times through `Map`/`Unmap` and 10 times through `UpdateSubresource`:

```
cb hook D3D11CreateDeviceAndSwapChain ok at ...
D3D11CreateDeviceAndSwapChain hr=0x00000000 ...
cb hook CreateBuffer ok / UpdateSubresource ok / Map ok / Unmap ok
cbuf #0 078C21A4 bytes=256
cb progress bufs=1 updates=40 passes=0 hits=0 ply=0
```

`updates=40` is exactly 30 + 10, so all three capture paths work. The harness
also proved the DllMain install path is safe: its engine-hook addresses are
unmapped in that process and MinHook simply fails them (error 7) without
taking the process down.

### The camera detector is shared code with a unit test

The matrix test started life inside the probe, which meant the thing that was
tested was not the thing that ran. It now lives in
`stage5/coopnet/camera_detect.h` (`CamTestMatrix`) and both the probe and
`stage5/test/cbtest.c` include that header. `cbtest.exe` builds a view
projection from a camera at (3, 2, -4) looking at the origin and checks:

```
PASS  VP (row-vector, as written) is recognised
      cam=(3.0000,2.0000,-4.0000) aspect=1.6000 dist=5.3852 look=1.0000
PASS  recovered camera equals the one that built it
PASS  recovered aspect is 1.6
PASS  the camera looks straight at the player
PASS  the transposed VP is recognised as column-vector
PASS  transposed VP still recovers the camera
PASS  view matrix is recognised in view mode
PASS  view matrix recovers the camera
PASS  the planted matrix is found by an offset scan       (at off=8, i.e. 0x20)
PASS  the offset scan recovers the camera
PASS  a plain identity is not a view-projection
PASS  a plain identity at the origin has no camera distance
PASS  arbitrary junk is rejected
PASS  a NaN entry is rejected
PASS  a camera 5.4 m away fails a 1 m distance gate
PASS  a point off the view axis fails the look gate
OK (0 failure(s))
```

Two things that bit during the test and are worth remembering:

* `|cos|` cannot tell "in front" from "behind" - a point mirrored through the
  camera also scores 0.93 - so the first version of the look-gate check passed
  a point behind the camera. The gate is deliberately unsigned (the forward
  axis's sign depends on the projection convention), and the distance gate is
  what actually rejects that case.
* MSVC folds `0.0 / 0.0` into a compile error (C2124); a NaN for the test has
  to come from a `volatile` zero.

The probe's geometric gate is now `dist <= 8 m` and `look >= 0.85` (~32 deg).
The previous 0.55 was too loose: it admitted a pure translation matrix
(`cam=(0.5,1.5,-3.5)`, basis = identity) from the menu.

### Trap: the shell can steal the foreground and silently eat a whole run

A menu-driving run ended up in the Controls screen with every footer OCR read
empty. The probe log showed why:

```
== driving pid 45100 (player 1) ==
focused hwnd=0xC20B68 pid=24624 proc=PickerHost
WARNING: front window is not the target process
```

`PickerHost` had taken the foreground, so (a) every keystroke went to it
instead of the game and (b) `winshot.ps1` captures the screen rect of the game
window, which the picker was covering, so OCR had nothing to read. The script
warned and carried on, which is exactly the wrong behaviour.

Fixes now in place:

* `keys.ps1 -FocusOnly` retries up to 5 times (Alt-tap to defeat the
  foreground lock, `BringWindowToTop`, topmost flip, `SetForegroundWindow`),
  verifies the foreground pid, and **throws** rather than sending input to the
  wrong window.
* `drive_level.ps1`'s `Footer()` retries OCR three times with two crops and
  refocuses in between; `SelectItem` is direction-agnostic (jump to the top
  with Up x8, scan down, then scan back up) instead of assuming which way the
  wanted item lies.
* Never run a test console while driving the menu: `CopyFromScreen` picks up
  whatever is on top of the game window.

### The camera detector was wrong in a way its own test could not see

The probe spent a whole session finding no gameplay camera. The detector was
not "too strict" - it rejected every genuine Direct3D view-projection, and it
mis-recovered the camera from a plain view matrix too.

The unit test passed because it built its matrices with the detector's own
formula. A test that re-implements the assumption it is meant to check is not a
test. `stage5/test/convtest.c` now builds the two standard conventions from
first principles - `clip = v*V*P` (row-vector) and `clip = P*V*v` (column
vector), each also stored transposed - and asks the detector about each one:

```
BEFORE                                        AFTER
  A row-vector V*P        rejected              cam=(3.000,2.000,-4.000)
  A transposed in memory  rejected              cam=(3.000,2.000,-4.000)
  B column-vector P*V     rejected              cam=(3.000,2.000,-4.000)
  B transposed in memory  rejected              cam=(3.000,2.000,-4.000)
  view row-vector         cam=(-3.231,-1.600,-4.000)  WRONG
  view column-vector      cam=(-3.231,-1.600,-4.000)  WRONG
```

Both bugs are the same mistake in different places: mixing up which axis the
basis lives on. In the canonical row-vector layout (`basis in the rows,
translation in the last row`) the camera is

```
cam = -(t.x*r + t.y*u + t.z*f)      with r,u,f the three basis ROWS
```

but the old code read `r,u,f` out of the *columns*, which mirrors the camera
through the plane of the basis. For a plain view matrix it returned
`-(t.z*f_row)` where `f_row = (r.z,u.z,f.z)`, i.e. the transposed basis.

For the combined view-projection the same mix-up made it invent a relation that
only holds when the camera is axis aligned, which is why the only candidates it
ever accepted were an identity-with-translation and one shadow/reflection
camera. The real structure is (row-vector, `M = V*P`):

```
col0 = ax * Vcol0                 ax = ay/aspect
col1 = ay * Vcol1                 ay = 1/tan(fovY/2)
col2 = az * Vcol2 + bz * e4       az = zf/(zf-zn), bz = -zn*zf/(zf-zn)
col3 = Vcol2
```

so the checks are: the first three entries of `col0`, `col1`, `col3` are
mutually orthogonal; `col2` is parallel to `col3` with ratio `r = +-az`; and
`col2.w = r*col3.w + bz`, i.e. `bz = M[3][2] - r*M[3][3]`. The recovered camera
is then

```
V[i] = (M[i][0]/ax, M[i][1]/ay, M[i][2]/az)      i = 0,1,2
t    = (M[3][0]/ax, M[3][1]/ay, M[3][3])
cam  = -(t.x*V[0] + t.y*V[1] + t.z*V[2])
```

Note `t.z = M[3][3]`, not `(M[3][2]-bz)/az` - the trailing `+bz` sits on row 3
only, which is what made the old "closing relation" look plausible while being
wrong for every camera that was not looking down an axis.

The sign of `r` matters: a projection with its `-1` in the w row flips `col3`,
so `bz = M[3][2] - r*M[3][3]` uses the signed ratio, not `|r|`.

### The gate assumed a third-person camera

Alien: Isolation is first person. The camera sits at eye height and looks
*forward*, so the angle between its forward axis and the direction to the
player's recorded position is around 70 degrees, not 20. The old `look >= 0.55`
(and later `0.85`) gate therefore could not accept the one matrix that mattered.
The gate is now distance only, and intentionally loose:

* the camera must be within 5 m of the player record (it is 1-2 m),
* the look angle is still reported, for diagnosis,
* the aspect ratio must be 1.2-2.2.

### The buffer table silently stopped at 256

```
#define MAX_CB 256
...
LONG n = InterlockedIncrement(&g_ncbs) - 1;
if (n < MAX_CB) { ... } else { InterlockedDecrement(&g_ncbs); }
```

The game creates more than 256 constant buffers while loading, so every buffer
created after that point - including per-frame gameplay ones - was dropped
without a word. The table is now 4096 entries with a 8192-slot open-addressing
hash (a linear scan at 60k updates/second is not affordable), and it logs if it
ever fills up. Analysis is incremental: vertex-shader-bound buffers every pass,
everything else round-robin with a per-pass budget, and buffers whose contents
have not changed are skipped.

Two more tools reduce the cost of the ten-minute restart loop:

* `!CBCSV` writes every captured buffer to `cbdump_<pid>.bin`. `cbdump.py`
  reads it back and searches for camera matrices offline, so a failed guess no
  longer costs another game launch.
* `!OVLON` starts the overlay renderer on demand. The game can be driven all
  the way into gameplay with no overlay thread at all, then the overlay is
  switched on once a camera matrix has actually been selected.
* `VSSetConstantBuffers` is hooked (vtable slot 7) to mark which buffers are
  ever bound to the vertex shader - "live per-frame data" versus hundreds of
  loading-time buffers.

## Phase 6 VERIFIED - world to screen, and a marker on the remote player (2026-10-23)

The in-world avatar is now drawn in both windows at the right place, and the
projection is checked against hand arithmetic rather than eyeballed.

### The camera lives in one constant buffer, view then projection

```
buffer 576 bytes, bound to the VS, rewritten every frame
0x040  view matrix        16 floats, column-vector form
0x080  projection block   16 floats, column-vector form
0x140  camera position    3 floats, verbatim (row 2 of the view translation)
```

The view matrix is column-vector: the three 3x3 *rows* are `(right, up, fwd)`,
the translation is in the last column, `t = -R*cam`, `right x up = fwd`
(left-handed, `z > 0` in front). The projection has no perspective term in the
usual place: `ndc = (ax*x/z, ay*y/z)` with

```
ax = 1.437402      ay = 2.299843
az = 1.000050      bz = -0.050002       (near 0.05, far 1000)
ay = 1 / tan(47 deg / 2)               (47 is the record's FOV field)
```

### The detector is one header, tested two ways

`stage5/coopnet/camera_detect.h` now holds `CamViewCol`, `CamProjBlock`,
`CamProjectCol`, `CamPair` and `CamFindInBuffer`: given a buffer, find a view
block whose recovered camera is within `maxDist` of the player record and pair
it with a projection block that has the swap chain's aspect.

* `stage5/test/convtest.c` - 36 checks, all pass: both storage conventions, real
  captured game view matrices A and B, the real projection block, the 47 deg
  identity, `right x up = fwd`, 1 m ahead maps to screen centre, signs,
  behind-camera rejection, and the buffer scan's found / gated / wrong-aspect /
  negative cases.
* `stage5/test/camfind.c` - runs the *shipping* scan over a `!CBCSV` dump
  (`camfind.exe FILE px py pz [aspect]`). On the real dumps it reports exactly
  **one buffer in 4096**, `view@0x040 proj@0x080`, `as=1.6000`, camera equal to
  the live player position; with a wrong player position it finds nothing.

The buffer address changes on every launch (`074FC1E4`, `078804A4`,
`076A7524`, ...), so a hardcoded address is not an option - the scan is required
and it is cheap (a handful of float compares per 16-float offset).

### Why the camera was never found before: two wrong gates

1. **The camera sits ON the player record.** Alien: Isolation is first person,
   so the distance from the camera to the record is ~0, not 1-2 m. Any
   "camera must be far from the player" assumption rejects the only matrix that
   matters. The gate is `dist <= 0.75 m`.
2. **The look angle gate was meaningless** for the same reason (0 deg between
   "camera forward" and "camera -> player"). Distance alone gates now.

Also: the player record's `fwd` field (`blk+0x790`) is **not** the view
direction - it is about 90 deg away from it, i.e. roughly the camera's *left*
axis, and it lags during fast turns. Use the view matrix for anything
directional. The avatar yaw on the wire still comes from that field, so the
remote avatar faces 90 deg off; publishing the camera forward vector instead is
the fix (next change).

### Projection verified end to end, in the live game

Both instances spawn on the same spot (same save), so the first test was
degenerate - `depth = 0`, correctly refused. Then, with p2 at
`(1.871, 9.195, 9.561)` and p1's camera at `(3.510, 9.193, 11.062)` looking
along `(-0.153, 0, -0.988)`:

```
CAM remote pos=(1.871,9.194,9.562) ndc=(1.153,0.001) depth=1.73 px=(1808,524) onscreen=0
```

Hand check: `d = (-1.639, 0.001, -1.500)`, `right = up x fwd = (-0.988, 0, 0.153)`,
`x = d.right = 1.389`, `z = d.fwd = 1.733`, `ndc.x = 1.4374*1.389/1.733 = 1.152`.
Exact match. Turning p1 by +900 px of mouse (measured sensitivity 0.0865 deg/px)
then gave

```
fwd=(-0.737,0,-0.676)  ndc=(0.082,-0.000)  depth=2.22  px=(909,525)  onscreen=1
```

and `1.4374 * 0.127 / 2.216 = 0.0824` by hand. Turning away by the same amount
returns `CAM remote none` (behind the camera), which is the case that must not
draw.

Axis conventions used above: `fwd = (L[8],L[9],L[10])`,
`right = up x fwd = (fwd.z, 0, -fwd.x)` for `up = (0,1,0)`, and screen
`x = (ndc.x*0.5+0.5)*width`, `y = (0.5-ndc.y*0.5)*height`.

### The render path is lock-free: the store publishes the two blocks

The analyser picks one buffer plus the two offsets. From then on, **every fill
of that buffer** copies the two 64-byte blocks into a double buffer
(`CbPublishCam`, called from `CbStore`, under `g_cbLock`); the overlay copies
whichever slot the index says and validates it with the same functions the
analyser uses. No lock, no pointer chase, and the marker is never a whole
analysis tick (1 s) behind the game.

Two bugs came out of this design change:

* **Do not clear the selection on a single validation failure.** The overlay
  runs at ~60 Hz and the analyser at 1 Hz, so "validate, clear on failure"
  turns into a flap loop: the overlay clears, the analyser re-selects, the
  overlay clears... and `!CAM` almost always sees `sel=0`. Only a *run* of
  failures (>30) drops the selection.
* **A seqlock retry silently ate the marker.** The first version guarded the
  copy with `g_camPubSeq` and retried 8 times, spinning with no yield. When the
  writer was preempted mid-copy, all 8 spins saw an odd counter and the frame
  was dropped. Symptom: the status line said `cam=1`, `drawn` had been latched
  once, and no marker ever appeared in a screenshot. Double-buffering removed
  the failure mode entirely (a 128-byte copy cannot be torn from the reader's
  side).

Diagnostics that made it visible, now part of `!OVL`:

```
OVL on=1 paints=3782 drawn=2690 noCam=1 why=0 lastndc=(0.33,-0.12) depth=2.77
    lastpx=(1114,589) sel=1 buf=076A7524 view@0x040 proj@0x080 ...
```

`paints` is a paint counter (66/s at `Sleep(15)`), `why` is 0 drew / 1 no camera
/ 2 no remote / 3 projection rejected (behind) / 4 projected off screen, and
`lastpx` is where the last marker was put. A screenshot now has an expected
pixel to check against.

The cross marker (before the avatar billboard) was drawn at the remote's eye
minus 0.15 m, so it sat about 100 px lower on screen than the `!CAM` `px=`
value - that offset was deliberate, not drift. The avatar now derives its
position from the projected feet and crown instead, so `lastpx` is the head
centre and `avh` is the projected body height.

### Verifying a marker when the agent cannot see the screenshot

`stage5/test/pix.ps1` counts pixels of the marker colour in a PNG (and reports
their centroid and bounding box); `pixmap.ps1` prints a coarse ASCII map of
them. The overlay draws the marker in `RGB(0,255,128)` and its status line in
yellow, on a black colour key, so both are unambiguous.

```
pix.ps1 -Path shot.png -X 887 -Y 607 -Radius 40
green=355  centroid = 892,606  bbox = 863,582 .. 927,630      # cross+circle at 887,607
pixmap.ps1 -Path shot.png                                      # ASCII map, finds scenery too
```

Scenery in this game also contains green, so a single count proves little; the
centroid moving with the projection (`lastpx` 884 -> 1114 while p2 walked,
`depth` 2.17 -> 2.77) is the check that matters.

### Trap, second time: the overlay is a window of the game's process

`Process.MainWindowHandle` returned **the overlay**, not the game: it is a
second visible top-level window in the same pid, deliberately the same size as
the game window, and topmost. Windows happily reports it as the main window.
`keys.ps1` then focused it, its foreground check *passed* because it only
compared process ids, and the game received no input at all - `USE MOVE-KEYS`
stayed on screen, the player never moved, while `proc` and `gp` counters kept
climbing. Mouse movement did nothing either, which is what told the two cases
apart (a paused game still loses focus, a wrong window loses input *and* the
camera).

Fix: `stage5/test/wingame.ps1` - `[GameWin]::Find(pid)` picks the largest
*visible* top-level window whose class is not `AICOverlay`;
`[GameWin]::Repaint(h)` forces a paint (used to test the paint path without
waiting for the loop); `Get-WinList` / `[GameWin]::Foreground()` print one line
per window plus class, rect, ex-style and pid. `keys.ps1` now compares
`GetForegroundWindow() -eq $hwnd`, not just the pid, and warns when the
foreground is another window of the same process.

The same assumption was in `tile.ps1` and `winshot.ps1`. It happened to work only
because tiling runs before the overlay exists, and `winshot` captured the
overlay's rect, which is the game's client area anyway. Both use
`[GameWin]::Find` now, and `winshot` captures the **client area**
(`GetClientRect` + `ClientToScreen`) instead of the window rect, so image
coordinates equal the numbers `!CAM`/`!OVL` print - no more 3 px/26 px skew
between a projection and the pixels that prove it.

### The marker is now an avatar billboard, and the yaw is the camera's

The wire's `yaw` used to come from the record's `fwd` field, which is ~90 deg
off the view direction, so the remote avatar faced sideways. The send thread now
prefers the view matrix:

```c
if (OvlGetCam(L, &ax, &ay, ccam) && CamViewCol(L, ccam, r, u, cfw))
    st.yaw = atan2f(cfw[0], cfw[2]);      /* camera forward, +Y yaw */
else
    st.yaw = atan2f(g_localFwd[0], g_localFwd[2]);
```

Evidence, same spawn position on both machines: the peer's log went from
`yaw=1.657` (the record field) to `yaw=-2.982`, and the sender's camera forward
is `(-0.153, 0, -0.988)` whose `atan2` is `-2.988`. Before the change the two
instances at the same spot disagreed about facing by ~90 deg.

The debug cross is replaced with a screen-space humanoid built from the same
projection (`OvlDrawAvatar`). The record is the eye, so:

```
feet      = eye - 1.62 m
crown     = eye + 0.12 m
height px = (crown_py - feet_py)            both ends projected separately
shoulders = 0.45 m -> 0.2586 * h px         narrowed to 0.6 of that side-on
parts     = ground shadow, 2 legs, torso, 2 arms (darker), head + 1 px ring
facing    = a line on the floor from the feet along the avatar's forward axis
```

The foreshortening uses `|sin(yaw)*toCam.x + cos(yaw)*toCam.z|`, so a side-on
avatar shows its body depth rather than its full shoulder width. Nothing here
scales with distance by hand: every size is `h` times a real body proportion,
so the figure keeps its shape at any range (8 px minimum, so a distant player is
still findable).

Checked against arithmetic, not eyeballed. At 2.16 m the figure is

```
avh=971 px     hand: ay * 1.74 / z * H/2 = 2.2998*1.74/2.16*525 = 972
bbox 784..1014 = 230 px wide
               hand: wf = 0.6+0.4*0.782 = 0.913; 0.50*w*2 = 229
```

and from the other instance at 2.02 m, `avh=1040 px` (hand: 1040) with the green
centroid at x=1591 against `lastpx x=1594`. The legs run off the bottom of the
screen at 2 m, which is correct: the camera is at eye height 1.6 m up.

Known limitation, and the next real problem: **there is no depth test**, so the
avatar draws through walls. A screen-space billboard can only fix that by
sampling the depth buffer at the projected point (a copy of the depth texture,
read on the CPU) or by ray-casting line of sight against the map; the honest
alternative is drawing the real character mesh, which means owning a draw call
and its shader state.

### The avatar interpolates, and a silent peer disappears

Packets arrive at `AIC_COOP_SEND_HZ` (20), the overlay paints at 66 Hz, so
drawing the newest packet makes the avatar step 20 times a second. The receive
path now stores a 4-entry ring of `{pos, yaw, ms}` under an `SRWLOCK`, and the
paint draws **one send interval in the past**:

```
u   = (now - newest.ms) / (newest.ms - older.ms)   clamped to 1.5
pos = older.pos + (newest.pos - older.pos) * u
yaw = older.yaw + shortest_arc(newest.yaw - older.yaw) * u
```

`u` is 0 as a packet lands and reaches 1 one interval later, so the drawn pose
is always between two packets that actually exist; the clamp allows a little
extrapolation when a packet is late, then holds. The cost is 50 ms of latency,
which is invisible at walking speed and needs no velocity estimate on the wire.
The yaw blend wraps through `+-pi`, so turning through 180 deg does not spin the
avatar the long way round.

The same ring gives the peer a **timeout**, which the avatar needed badly: with
no timeout it stayed on screen at its last position forever after the other
player quit, which reads as "my friend is standing there" when they are gone.
`REM_TIMEOUT_MS` is 1500; on expiry the draw returns 0 (`why=2`), and the log
records the transition once (`REMOTE stale: no packet for 1516 ms`) and again
when packets resume.

Evidence:

```
during a walk:  pos=(1.370,9.227,9.562)  raw=(1.357,9.231,9.563)  age=32ms
                (interpolated point trails the newest packet by 0.013 m, u~0.32)
peer killed:    CAM remote none have=1 age=3250ms stale=1
                OVL why=2 age=3485ms
                log: REMOTE stale: no packet for 1516 ms
                paints 2043 -> 2150 over 1.5 s while drawn stays 781
```

That last line is the check that the avatar is *gone* rather than frozen: the
paint loop is still running (the counter climbs) but nothing is being drawn.

### Off screen or behind you: the avatar becomes an edge arrow

In a game where two players walk different corridors, the peer is *not on screen*
most of the time, and both of those cases used to draw nothing at all:
`why=4` (off screen) and `why=3` (behind the eye) were the commonest states, so
the overlay answered "where is my friend?" with an empty screen.

Both now pin a green arrow to the screen edge, 56 px inside, with the range:

```
off screen:  direction = projected head - screen centre
behind eye:  direction = the peer's bearing on the view matrix's right/up axes
             (a peer straight behind has no bearing, so it falls back to +x)
tip:         centre + direction * t,   t = min(mx/|dx|, my/|dy|, 1)
head:        3-point Polygon, 22 px long, 15 px back at +-2.4 rad
label:       "P2 12.3 m", above the arrow when it points down
```

`t` is whatever scale brings the direction onto the inset rectangle, so the tip
lands on the edge no matter how far off screen the peer actually is, and the
arrow's angle is the true bearing - it points left for a friend on your left,
not just "somewhere off screen".

The off-screen test changed with it: it is now the **body box** (crown to feet,
shoulder width), where it used to be the head with a 25% margin. That mattered:
the normal close-range case has the feet below the window because the eye is
1.62 m up, and a margin-based test hid the figure exactly when you were standing
next to someone.

`why` is now `0` avatar, `1` no camera, `2` no live peer, `3` behind the eye
(arrow), `5` off screen (arrow); `4` is unused.

**Bug found by the first live run:** the tip came out at `(837,524)`, dead
centre, instead of on an edge. The two callers pass a direction in *different
units* - pixels for the off-screen case, metres for the behind-the-eye case -
and the first version scaled by `t = min(mx/|vx|, my/|vy|, 1)`. For a 3 m world
vector every ratio is huge, so `t` clamped to 1 and the tip landed 3 px from the
centre. Normalising the direction first is what makes one function serve both
cases. A second sign error was in the same path: screen `y` grows downward while
the view's `up` axis grows upward, so the behind case needs `vy = -(d·up)`;
without the negate a friend on the floor above got an arrow pointing down.

Verified after the fix, all three arrow states plus the return to the figure
(one turn sequence on p1, p2 standing 2-3 m away):

```
behind-left   why=3  lastpx=(56,525)    hand: right=(0.978,0,-0.210)
                                          vx = d.right = -1.590 -> left
                                          up = (0,1,0), vy = 0
                                          tip = (840-784, 525) = (56,525)
                pix  green=466  centroid=(57,512)  bbox 33,487..87,533
behind-right  why=3  lastpx=(1624,524)  hand: right=(-0.223,0,-0.975)
                                          vx = 0.397+1.343 = +1.740 -> right
                                          tip = (840+784, 525) = (1624,525)
                pix  green=465  centroid=(1625,512)  bbox 1601,487..1655,533
off screen    why=5  lastpx=(1624,463)  hand: crown is 0.12 m over the eye at
                                          1.90 m -> dy = -2.2998*0.12/1.90*525
                                          = -76, hy ~ 450, dir (979,-75)
                                          t = 784/0.997 = 786 -> (1624,465)
facing again  why=0  avh=872px          hand: 2.2998*1.74/2.41*525 = 870
                pix  green=72289  centroid=(1122,760)
```

The tip x is `56` or `1624` and the tip y is `56`, `525` or `994` in every
case - exactly the 56 px inset rectangle, which is the invariant that says the
scaling is right rather than merely plausible. The arrow also *tracks* the
bearing: turning p1 90 deg moved the tip from the left edge to the right edge
while the peer stayed put.

### Network: host, join, and a peer that learns its own address

`AIC_COOP_PEER_HOST` (default `127.0.0.1`) selects where packets go, and the
address is resolved **once** at startup rather than on every send. Two cases:

- a dotted quad, used directly;
- anything else, resolved through `getaddrinfo` on startup, with a fallback to
  loopback and a logged warning if it does not resolve at all.

The more important change is that the host side **learns** its peer: the first
avatar packet that arrives carries the real source address, and from then on the
send thread talks to that address instead of the configured one. That is what
makes this usable over the internet without both sides configuring anything —
only the joining side needs an IP, and NAT hole-punching falls out for free
because the host answers the mapped source port it actually saw.

Learning is deliberately done field by field (`sin_addr.s_addr` via
`InterlockedExchange`, `sin_port` via `InterlockedExchange16`) so the send thread
can never read a half-updated address.

**Bug that would have made the whole log useless:** `inet_ntoa` returns a
pointer into a **single static buffer**. The learning log line printed both the
new and the old address in one `Logf` call:

```
Logf("peer learned from packet: %s:%u (was %s:%u)",
     inet_ntoa(from.sin_addr), ..., inet_ntoa(g_peer.sin_addr), ...);
```

Both `%s` therefore pointed at the same buffer and printed the same string, so
the "was ..." part was a lie. Verified with a 10-line C program that compiles
and runs in the stage5 test folder:

```
naive  : 127.0.0.1 (was 127.0.0.1)     <- both calls collapsed
copied : 127.0.0.1 (was 192.168.1.7)   <- correct after copying out first
```

The fix is to `strncpy` each result into its own stack buffer before the next
call to `inet_ntoa`.

**Peer-learning test (2026-10-23):** sent a single fake avatar packet at p1
(port 45500) from a socket bound to port 45600, using a new reusable tool,
`stage5/test/fakepeer.ps1` (quantizes metres to the wire format, so it can stand
in for a second player without driving a real instance through menus):

```
peer learned from packet: 127.0.0.1:45600 (was 127.0.0.1:45501)
peer learned from packet: 127.0.0.1:45501 (was 127.0.0.1:45600)
NET id=1 bind=45500 peer=127.0.0.1:45501 cfg=127.0.0.1 learned=1 sent=9145 recv=9143
```

The first line proves the learning path fired; the second proves it self-heals
from p2's next packet; `learned=1` in NET proves the send path actually uses the
learned address. The rig was still syncing (`remote=1 age=16ms stale=0`) two
minutes later.

**A control command** `!NET` reports the bind port, the peer address actually in
use, the configured host, the learned flag, and the sent/recv counters, so a
friend on the other end of a link can be told "run status.ps1 and paste the
output" instead of describing symptoms.

### Packaging (Phase 7 groundwork)

`stage5/pack/` contains the install + run scripts, plus `aicctl.ps1` (a UDP
control client) and `fakepeer.ps1` (a scripted peer for QA):

- `install.ps1` — backs up `AI.exe` and `STEAM_API.DLL` once (never
  overwriting a pristine backup), applies `patch_gate.py` (which refuses unless
  the guard bytes match the expected function), installs the mock DLL and
  `coopnet.dll`, creates `steam_remote\p1..pN`. `-Uninstall` restores.
- `host.ps1` / `join.ps1` — one instance each, with all environment variables
  set in one place (`aiclaunch.ps1`).
- `local2p.ps1` — two instances on this PC, tiled side by side on the primary
  display (the user's standing requirement).
- `status.ps1` — asks every instance whether the link is alive and explains how
  to read the answer.
- `README.md` — honest about the limits: separate worlds, same save required,
  no occlusion, no Steam overlay, no join UI.

All the variables the mock and the transport read are centralised in
`aiclaunch.ps1` so the host/join/local scripts cannot drift apart.

### Occlusion: sampling the depth buffer at Present time

The avatar is drawn in a *separate* top-level GDI window, so the game's depth
buffer is the only honest answer to "is there a wall in front of my friend".
Sampling has to happen on the render thread, because the immediate context is
not thread-safe; the overlay thread only reads the result.

What was added:

- `IDXGISwapChain::Present` (vtable entry 8) is hooked in `Hook_CreateDevSC`,
  via MinHook, exactly like the device/context hooks. `Hook_Present` runs on the
  render thread and calls `OccludeSample()` before forwarding to the real Present.
- `OccludeSample()`:
  1. `OMGetRenderTargets` to get the bound DSV. No DSV (or not a Texture2D)
     → hard failure, logged once.
  2. Reads the avatar's projected pixel from `g_ovlPxX/Y` (published by the
     overlay thread) and bails softly (result 2, not a failure) when there is no
     on-screen figure this frame.
  3. Creates a **1x1 staging texture** in the depth buffer's own format, once,
     and reuses it. Copying a single texel via `CopySubresourceRegion` with a
     1x1 `D3D11_BOX` is cheap; recreating it every frame would not be.
  4. `Map`/read/unmap. Three formats are handled: `D32_FLOAT`,
     `D24_UNORM_S8_UINT` (depth in the high 24 bits), `D16_UNORM`. Anything
     else is a hard failure with the format number in the log.
  5. Compares the sampled `z_ndc` against the avatar's own `z_ndc`
     (`1.00005 - 0.05 / depth`): if the scene is more than 0.002 in front,
     `g_occluded` goes to 1.
- The overlay consults `g_occluded` (and `g_depthSamplingOk`) in the figure
  path: an occluded avatar is replaced with the same edge arrow used for the
  off-screen and behind-camera cases (`why=6`), with `occ=1` in `!OVL`.
- Everything is fail-safe: three consecutive hard failures disable sampling
  permanently for that run and log the reason; the avatar then draws through
  walls exactly as before. `AIC_OCCLUDE=0` disables it from the start.

Two new control verbs: `!OCC [0|1|auto]` forces / restores occlusion (so the
decision logic can be tested without a wall in the right place) and `!DEPTH`
prints the last sampled scene depth against the avatar's depth.

The depth is sampled at the avatar's **head** pixel, because that is where a wall
would first hide them, and one frame of lag between the overlay publishing the
pixel and Present sampling it is invisible.

**Result (2026-10-23): depth sampling at Present does not work for this game.**
The pipeline itself is sound — `OMGetRenderTargets` returns a DSV, the staging
texture is created in the buffer's own format (`D24_UNORM_S8_UINT`), the 1x1
copy succeeds, and `Map` returns data. But 7410 consecutive samples at
`(840,525)` all read `raw=00000000` (`scene=0.0000`), and earlier samples at
the same camera position read `0.2477` (a 6.6 cm "wall" that is really the
player's own weapon, or a cleared buffer). The depth buffer is **cleared before
Present** — there is nothing to sample by the time the frame is presented.

Consequences:

- `AIC_OCCLUDE` defaults to **off**. Sampling is left in the code (guarded by
  `scene < 0.001 -> not occluded`, and by the `AIC_OCCLUDE=1` switch) in case a
  mid-frame hook point is found later, but it does not run by default and never
  hides the avatar.
- The avatar still draws through walls. That is the honest, current limitation,
  and it is recorded as such in `stage5/pack/README.md`.
- Occlusion would need either a hook placed between scene rendering and the depth
  clear (more reverse engineering of the engine's frame graph) or a completely
  different approach (ray-casting against the map's collision, which needs the
  map geometry).

What was learned and is worth keeping: the `Present` hook, the 1x1 staging
texture trick, the `D24_UNORM_S8_UINT` byte layout (`(u >> 8) / 16777215`),
the `!FAKEPX` test hook, and the `!OCC` / `!DEPTH` diagnostics. The `!FAKEPX`
hook in particular is reusable for any future test that needs a controlled pixel
without fighting the overlay.

### Verification runs (2026-10-23)



```
p1 pair  view@0x040 proj@0x080 ax=1.4374 ay=2.2998 as=1.6000  (both instances)
OVL  paints=1425 drawn=712  why=0  lastpx=(887,607)
pix  green=355 centroid=892,606 bbox=863,582..927,630
p2 walked 1.2 s: depth 2.17 -> 2.77, lastpx 884 -> 1114, pix centroid 1105,598
p1 turned 180 deg: CAM remote none, OVL why=3, marker stops
```

(The last line is from before the edge arrow: turning away used to stop the
marker entirely. With the arrow it is `why=3` *and* a draw at the screen edge.)

After the avatar change, same session shape (fresh launch, both spawn on the
same spot, one walks away):

```
REMOTE yaw=-2.982   (camera forward atan2(-0.153,-0.988) = -2.988)
p1: CAM ndc=(0.029,0.001) depth=2.16  OVL why=0 lastpx=(864,554) avh=971px
    pix green centroid=865,763  bbox 784,472 .. 1014,1049   (230 px wide)
p2: CAM ndc=(0.898,-0.009) depth=2.02  OVL why=0 lastpx=(1594,562) avh=1040px
    pix green centroid=1591,773  bbox .. 1679,1049          (clipped at the edge)
```

Cost note: `MAX_CB` is 8192 with a 16384-slot hash; the log still prints
`cb table FULL at 8192 buffers`, which is harmless because the camera buffer is
created early and the analysis only ever needs buffers created during loading
anyway.



