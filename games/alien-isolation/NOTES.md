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
- ~~The owner's Steam `userdata\<id>\214490` folder does not exist~~ **WRONG, corrected 2026-11-01:**
  the folder exists (an earlier glob check missed it). It holds the owner's real saves in `remote\`, dated
  2026-04/05. Their sizes and the SHA-1s in `remotecache.vdf` are unchanged after the test. Only
  `remotecache.vdf` itself was rewritten (at 17:38:25), by the Steam client's session-end bookkeeping. No save data changed.

## coopnet v1 (presence layer), 2026-11-01
- Windowed mode in the test copy: in `DATA\ENGINE_SETTINGS.XML`, removed the FullScreen/WindowedFullScreen options and
  all windowed resolutions except 1280x720 (original backed up). Observed: the game opened as a 1280x720 window.
- Display settings are not in %APPDATA%\SEGA (that folder holds other SEGA games). They are probably inside the
  cloud save files. Unknown.
- Test profiles p1/p2 were seeded with read-only **copies** of the owner's Steam saves (originals untouched,
  SHA-1 equal), so Continue can skip the intro.
- `coopnet.dll`, loaded by steamwrap: reads the camera record (`blk+0x78C`), checks it against the signature
  rec[7..9]=47/0.05/1000, sends pos+quat over UDP at 20 Hz (60-byte packet with an FNV checksum), and draws
  the peer on a click-through, color-keyed overlay. Logs a CSV of all 16 record floats at 10 Hz.
- Offline protocol test: **12 passed, 0 failed** (476/476 packets over real UDP, 5% loss and 10% reorder
  simulated, garbage rejected, receiver ends on the newest packet).
- In game (p1 alone, at the main menu): the DLL loaded, bound UDP 45500, and found the 1280x720 game window.
  The camera record **passed the signature at the menu** with |q|≈1 and position (0,0,0) (menu camera).
- Hypothesis (unverified): rec[0..3] is the rotation quaternion (x,y,z,w), and #85's "facing 0x790" is its y component.
  This explains "about 90 degrees off and lags". The axis convention (+Z forward?) still needs telemetry while walking.
- **Incident:** the first overlay build drew on top of every app, and the screen-copy screenshot captured the owner's other
  windows (a video call). The test was stopped and the screenshot deleted. Fixes: the overlay only shows while the game is
  the foreground window, and winshot.ps1 now uses PrintWindow (game window contents only).
  **Lesson for the playbook:** never screen-copy for QA, and check the owner isn't using the PC before driving input.
- A fullscreen window minimizes when it loses focus (seen when the terminal took focus). Next: windowed mode in the test copy.

## In-game presence test, 2026-11-01 (owner away, PC left to the bot)
Observed:
- Bot (focus + scancode keys, mouse deltas) drove both copies title -> Play Game -> Alien: Isolation -> Continue Game
  into the level from the copied saves (the save station at the start of the "hit the power at the same time as Axel" objective).
- Camera telemetry in the level: rec[0..3] stays a unit quaternion (x,y,z,w). Mouse right turns raised y:
  q=(0,0.3079,0,0.9514), which is a 35.9 deg yaw. With yaw -72 deg the player walked 2.6 m along atan2(dx,dz) = -72.2 deg.
  Looking up adds x/z components, and forward tilts to +Y. **Convention: identity looks along +Z, +Y up, +X right**
  (the "about 90 degrees off" in #85 came from reading a single component). rec[4..6] is the camera position (eye height
  about 5.08 in this room). rec[10..15] repeats rec[0..5] (a second copy, possibly the previous frame). Unknown which.
- Two copies in the level at once. The peer marker was checked with an offscreen overlay render composited on a
  PrintWindow capture (no screen copy). p2 5.7 m down the corridor appeared on p1 at camera (-1.26, -0.01, 5.60),
  matching the geometry. Reverse: behind -> edge arrow, after turning around -> marker at (-1.27, 0, 5.85).
- Host/join mode: the host learned the joiner's address from its first packet. tx 1874 / rx 1872 at the end.
- PrintWindow on `MainWindowHandle` returned the overlay window, not the game. Tools must skip the overlay's window class.
- SetForegroundWindow on the second copy failed until an Alt tap plus AttachThreadInput was used.
Offline: steamwrap 40/40, protocol 12/12, projection 10/10 (cases from the measurements above). Installer round trip on
a fake folder 6/6 (refuses the wrong build, backs up, reinstall keeps the backup, uninstall restores).
Mod repo commit 355cfbb (phoenixd123/alien-isolation-coop).

## Partner character (3D model) work, started 2026-11-01
- Tools, owner approved. Ghidra 12.2 (GitHub release zip, SHA-256 2c2cd776…be67 matched the release notes).
  Ghidra 12.2 needs **JDK 25**: JDK 21 is refused with "JDK 25+ could not be found". Temurin 25.0.5+8
  (SHA-256 matched the Adoptium API). Headless analysis runs on a copy of AI.exe.
- Leads from strings in AI.exe (observed): CATHODE classes `SetPosition`, `DespawnCharacter`, `AssetSpawner`,
  `PathfindingTeleportNode`, `CHR_SetTacticalPosition`. Message names `character_discontinuous_positioning` and
  `request_display_character`. Error text "Failed to find some valid components for a character spawner".
- From #156's notes (not re-verified): character transforms live in entity components, read through the generic
  property getter RVA 0x4D63C0 (bbox atoms); raw memory scans only find GPU noise.
- Plan (derived): the first target is driving an existing human NPC's transform from the network (a puppet),
  then spawning a dedicated character.
- Ghidra headless auto-analysis of AI.exe finished (about 1 h, 12 GB heap). Scripts used (Java, in the mod's RE notes):
  string xrefs, vtable dump per CATHODE class, refs-to, symbol lookup.
- Observed (static): "SET_POSITION" / "DESPAWN_CHARACTER" strings are memory-tracking category labels, not logic.
  The CATHODE::SetPosition / Character / DespawnCharacter vtables share almost every slot with all ~1450 CATHODE
  vtables. Their only unique slots (198/199) are a setter/getter of a static per-class ID token
  (SetPosition: DAT_0174e860; Character: DAT_0174ea38). Behavior is reached through generic dispatch, not
  class-specific virtuals, so static tracing from class names is slow.
- The decompile of the property getter RVA 0x4D63C0 loses its arguments (calling convention). It walks the entity's
  component list (`+8`, next at `[3]`) and matches the atom against `DAT_0174d304`.
- call_method prologue (RVA 0x526680): `55 8B EC 6A FF 68 <imm32>`. The first 10 bytes are position-independent,
  so a 5-byte jmp hook with a 10-byte trampoline is safe. Installed from DllMain (before game threads start),
  the game ran through the menus into the level normally.
- Probe run (p1, menu -> level, ~2 min): 238,494 calls and 2,832 distinct target/message vtables.
  **No message-type vtable has RTTI**, and most targets don't either. Named targets were only
  CATHODE::CompositeInstance, CATHODE::MASTER and CATHODE::Sequence. So message types can't be named from RTTI.
  Next: decompile a message vtable's slots to find a name or ShortGuid accessor, or read the command
  graph from the level data (CathodeLib / OpenCAGE know the COMMANDS.PAK format) and match entity IDs.

### Level scripts via CathodeLib (2026-11-01, owner approved .NET 8 SDK 8.0.427, SHA-512 checked)
- **The engine's property atoms are CathodeLib ShortGuids** (SHA1 → reorder → uppercase hex → SHA1 → first 4 bytes).
  #156's bbox atoms resolve exactly: 0xA652ED66 = `origin_offset`, 0xF0088922 = `origin`, 0x5E1AEC47 = `target_offset`,
  0xC8DE9852 = `target`. Hashing about 322k strings (CathodeLib info.dat plus AI.exe) and matching aligned dwords in a
  runtime dump of AI.exe's data section named **10,370 atom globals**: function types (e.g. CHR_SetHealth at RVA
  0x1350BEC) and parameter names.
- CathodeLib `Commands` needs the level's REDS / COLLISION.MAP / ENVIRONMENT_ANIMATION: passing nulls gives
  NullReferenceException. Load through `new Level(levelDir, new Global(DATA\ENV\GLOBAL))` instead (about 3 s per level).
- The save used for testing (objective "hit the power at the same time as Axel") is in **SOLACE**.
- Character display models across all levels: AXEL, CONNOR, MARLOW, PLAYER_FP (the player, first-person), RICARDO,
  **RIPLEY** (third-person, archetype `Archetypes\Cutscene_Actors\Ripley_3P_v4`, only in ENG_REACTORCORE,
  HAB_SHOPPINGCENTRE, SCI_HUB, TECH_COMMS), SAMUELS, SPACESUIT_*, TAYLOR, VERLAINE, WAIT, WORKER.
- How scripts move an NPC (seen in SOLACE M27_Pt03): `TriggerBindCharacter` (characters -> npc_reference of the actor,
  bound_trigger -> apply_start of a `CMD_GoTo`), `CMD_GoTo` (Waypoint -> reference of a `PositionMarker`).
  Actor instances are composites of an archetype with a `position` transform parameter.
- Experiment 1 (test copy, original backed up and restored afterwards): added to SOLACE's root composite a Taylor actor
  (spawn_on_reset), a PositionMarker at a magic position, CMD_GoTo, TriggerBindCharacter and a self-retriggering
  LogicDelay (start_on_reset). Observed: the modded level loads without errors, but **no actor appeared**. The magic
  float3 is in the file but **nowhere in the game's memory** (full 1.5 GB scan). A shipped marker's position
  (PositionMarker_12) isn't in memory as a float3 either.
  Derived (unverified): script positions aren't kept as plain float3 after load, and/or `*_on_reset` triggers don't
  fire when a save is loaded (a save restores entity state). Next: start the level fresh (new game or a level-start
  checkpoint) instead of Continue, and drive the waypoint through a script-visible variable rather than memory.

Not done / unknown:
- Two PCs over the internet: untested.
- A 3D character for the partner: needs character spawning or NPC repurposing in Cathode. Unknown how.
- Alien / NPC / doors / items / story sync: not started.

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
