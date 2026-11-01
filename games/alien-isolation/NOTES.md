# Alien: Isolation: working notes (second agent)

Started 2026-11-01. Path 2 (build from zero), building on the public notes of the first
attempt (issue #85, PRs #156 and #164, Joshiloveyou fork). Their code is not published, so this is
an independent rebuild guided by their write-up. The owner chose not to wait for that code.

## Identity (observed 2026-11-01)
- Store: Steam, appid 214490, buildid 4940658.
- AI.exe: 21,822,464 bytes, SHA-256 `DDE66521798AAE8C39ED4E7A3400F561FCECFFF83619D1426B9E0CEDF5819A90`.
- OS: Windows 11 Pro (build 26300).
- Engine: Cathode (Creative Assembly), per PR #156 (CATHODE RTTI namespace). Not yet re-verified here.
- This matches the build in PR #156 byte for byte (same SHA-256), so their RVAs should apply.

## Observed
- 2026-11-01: The first check found no install (only `DATA\cache`, Installed=0). The owner then
  installed the game. A second check found buildid 4940658 and the hash above, 5017 files, 22.89 GB.
- Workspace: test copy made with robocopy into `local/alien-isolation/game` (git-ignored).
- Test copy verified: 5017/5017 files, 0 failed, AI.exe SHA-256 unchanged.
- AI.exe PE header (own parser, scratch script): machine 0x14C (x86), image base 0x400000,
  ASLR and NX on. Sections .text/.rdata/.data/.rsrc/.reloc, so no packer section names.
- steam_api.dll imports: **14**, not 15 as PR #156 says. They are SteamAPI_Init, RestartAppIfNecessary,
  Shutdown, Register/UnregisterCallResult, Register/UnregisterCallback, RunCallbacks,
  SteamUtils, SteamFriends, SteamUserStats, SteamRemoteStorage, SteamUser, SteamApps.
- Other imports include WS2_32 (32), dxgi (2), d3d11 (1) and XINPUT1_3.
- String counts in AI.exe: "CATHODE" 1075, IsMultiplayerMode 2, NET_AI_UPDATE 1, NETZ_DISPATCH 1,
  character_register_accurate_pos 1. The Cathode engine and leftover multiplayer names are confirmed.
- Toolchain on the owner's PC: Python 3.13.7, git. No C/C++ compiler, Java, Ghidra or gh.

## Built 2026-11-01: steam_api wrapper (instead of a full mock)
- Toolchain: Zig 0.16.0 (`zig cc -target x86-windows-gnu`), portable. The official zip's SHA-256
  68659eb5…5a7e matched https://ziglang.org/download/index.json.
- Design: forward-by-default wrapper, not the full mock PR #156 used. Real steam_api.dll is renamed to
  steam_api_real.dll in the test copy. Every export and every interface slot we don't override is a
  `jmp` thunk (`mov ecx,[ecx+4]; mov eax,[ecx]; jmp [eax+4*i]`), so no argument counts are needed
  for pass-through. That removes the stack-skew crashes PR #156 reports.
- Overridden: RestartAppIfNecessary -> 0. ISteamRemoteStorage012 slots 0–20 and 27–32 are a local
  file store under `<game>\coop_saves\<AIC_PROFILE>\`, and IsCloudEnabled* return false, so the owner's
  Steam Cloud saves are never touched. ISteamUserStats011 slots 3,4,5,7,8,10,13 (stat/achievement writes) are no-ops.
  Interface versions were read from the game's own steam_api.dll: RemoteStorage012, UserStats011, Friends014,
  User017, Utils006, Apps006, Networking005. Slot order comes from Goldberg emulator's versioned SDK headers.
- With AIC_PROFILE set, CreateSemaphoreA/W and CreateMutexA/W in AI.exe's IAT append `_<profile>` to
  the object name. This replaces PR #156's exe byte patches, and AI.exe stays unmodified.
- steam_appid.txt (214490) is placed in the test copy so SteamAPI_Init works without a Steam launch.
- Offline test (fake steam_api_real.dll plus a harness that calls with MSVC thiscall and measures the bytes popped):
  **40 passed, 0 failed** (2026-11-01). It covers file write/read/exists/size/count/name/timestamp/delete,
  write streams with u64 handles, cloud-sync no-ops, forwarded slots on both interfaces, and blocked stat writes.

## Boot test with the game, 2026-11-01 (owner approved; Steam client running)
Observed, from the wrapper logs and window screenshots:
- p1 (AIC_PROFILE=p1): launched from the test copy, with the working dir set to the test dir.
  `RestartAppIfNecessary(214490) -> false`, `CreateSemaphoreA 'Alien: Isolation' -> 'Alien: Isolation_p1'`,
  `SteamAPI_Init -> 1`. It reached the **main menu** (Play Game / Options / Extras / Steam Store) about 35 s after launch.
  First cloud call: `FileRead input.cfg`, then `FileWrite PROGRESSION.ais 10240`, landing in coop_saves\p1.
- p2 launched while p1 was running: its semaphore was renamed to `_p2`, Init -> 1, and it reached the first-run
  calibration screen (fresh profile). **Two instances ran at once, both responding, about 1.17 GB each.**
  No exe byte patch was needed.
- The game also creates named semaphores for its worker threads (NewLoadScr, CAStreamer, SteamCloudThread, …).
  They are renamed per profile too, which did no harm.
- Closing with WM_CLOSE (CloseMainWindow) worked. Both logs end with `detach`. On exit, p2 wrote CHSV00.ais (2 MiB)
  and MSSV01.ais (1 MiB) locally.
- No stat or achievement writes were attempted during the menu-only run.
- The owner's Steam `userdata\<id>\214490` folder does not exist, so the game has no cloud save there and nothing was written.
- A fullscreen window minimizes when it loses focus (seen when the terminal took focus). Next: windowed mode in the test copy.

## Taken from PR #156 / fork NOTES.md (not yet verified here)
- 32-bit x86 PE, ASLR, no SteamStub. Base 0x400000.
- The game re-execs through Steam when it isn't launched by Steam. A dxgi.dll proxy in the test copy never
  reached gameplay. A mock steam_api.dll (RestartAppIfNecessary returns 0, Init returns 1) works.
  The named semaphore "Alien: Isolation" blocks a second instance.
- Working directory must be the game dir (DATA\engine_settings.xml).
- Player transform: `mgr=*(u32*)(base+0x12F0C88)`, `blk=*(mgr+0x58)`, pos at `blk+0x79C+0x40*n`.
  Record signature rec[7]=47.0, rec[8]=0.05, rec[9]=1000.0. Forward field is about 90 degrees off the view.
- Do not call the getter at RVA 0x4D63C0 from injected code (counter overflow, crash).
- Camera: 576-byte VS constant buffer, slot 7. View at 0x40, projection at 0x80, camera pos at 0x140. Rescan per launch.
- Avatar is drawn by a topmost overlay window, not a Present hook, so it draws through walls.

## Derived / plan
1. Toolchain (needs owner approval).
2. Mock steam_api.dll for the test copy (export list from AI.exe's import table, verified here).
3. Probe DLL: read the player transform, log it, UDP send/receive (direct IP, then Steam relay).
4. Second player visible. Then the hard part the first attempt hasn't reached: host-owned alien
   (actor-ownership.md), doors and story via EntityManager::call_method (RVA 0x526680).

## Unknown
- Whether Survivor Mode (Dropper7894's target) is an easier first target than the campaign.
- Exact ISteam* vtable slot usage (must be read from disassembly here).
