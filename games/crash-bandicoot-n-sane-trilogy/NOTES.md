# Crash Bandicoot N. Sane Trilogy (2017 console, 2018 PC) - notes

## Identity (observed, 2026-11-01)
- Store: Steam, app 731490, build id 2964270
- Exe: `CrashBandicootNSaneTrilogy.exe`, 29,922,392 bytes, SHA-256 `170c61d37fb5277dd94b3e41f7a918f227b76bc67aa338f5c97f63ac317b6af3`, PE timestamp 2018-07-19, no version info
- OS: Windows 11 x64
- Engine: Vicarious Visions Alchemy (exe strings: "Alchemy" x16, igCore, igRegistry; `MemoryConfiguration.igz`; `archives/` = 171 `.pak` + 2 `.bk2`; no UnrealEngine string). NOT Unreal Engine 4, unlike Crash 4.
- DRM: `.bind` section (178,384 bytes) -> SteamStub (same as Crash 4). Denuvo: no "Denuvo" string; status unknown.
- Root files: build.xml (`SaveFileGameIdentifier="SAVE"`), bink2w64.dll, d3dcompiler_47.dll, steam_api64.dll
- Exe has one "SteamNetworking" string; whether any netcode is used: unknown

## Existing work (research)
- No co-op mod found. Steam thread 2022-08-11 asking for one: only reply "Impossible."
  https://steamcommunity.com/app/731490/discussions/0/4794664409784156366/
- Alchemy/IGZ modding tools list N. Sane (asset tools only, no scripting): see games/crash-bandicoot-4/NOTES.md
  (https://github.com/AdventureT/IgzModelConverter)
- Original PS1 games, not N. Sane: Crash 1 static recomp (Matteo842/CrashBandicoot-Launcher, RecompOne, credits a "c1" decomp, unverified),
  Crash 2 decomp https://codeberg.org/Gabriel-TwoBits/crash2-decomp and c2c https://github.com/ughman/c2c. Not checked further.

## Derived
- Per playbook/triage.md: closed binary, no shipped multiplayer, no script layer -> hardest category.

## Unknown
- Whether the game runs twice on one PC; Denuvo status; Alchemy netcode presence.

## Not done
- gh issue/PR search for this game (gh not installed).

## Feasibility test 1: two copies on one PC (2026-11-01)
Setup: copy of the Steam install in a test folder, `steam_appid.txt` (731490) added so the copy does not restart through
Steam; SteamStub untouched; Steam client running; copy A launched, copy B launched 45 s later.
Observed:
- Both processes alive and responding for 180 s after B started (well past the 56 s at which Crash 4's second copy dies),
  ~735-750 MB working set each, 0 crash dumps.
- One Windows "RADAR_PRE_LEAK_64" event (Windows memory-leak diagnostics, not a crash).
- Not tested: both copies inside a level, input to each window, separate save folders (both copies share
  `%APPDATA%\Crash Bandicoot(TM) N. Sane Trilogy`).
Derived: one-PC multi-instance testing (playbook/multi-instance-testing.md) looks possible for this game.

## Workspace findings (2026-11-01)
Observed:
- Decrypted image: dumped the running test copy's module with ReadProcessMemory (base 0x7FF636B20000, size 0x1D74000,
  0 unreadable pages); `.text` entropy 6.42 in memory vs 8.00 on disk. Section raw offsets set to virtual addresses so
  Ghidra loads it as a PE. Exe on disk and SteamStub untouched.
- Ghidra 12.2 needs JDK 25 (`application.java.min=25` in Ghidra/application.properties); JDK 21 is refused.
- Separate save folder per instance: launching with the `APPDATA` environment variable pointed at another folder makes
  the game write `GraphicsOptions.sav` / `WindowData.sav` there; the real `%APPDATA%\Crash Bandicoot(TM) N. Sane Trilogy`
  was not modified. (Steam Cloud interaction: unknown.)
- The game prints a debug log on stdout when started without the shell: `app:/archives/update.pak not found.`,
  `[Streaming] Loaded chunk '<name>:<pool>'` (chunks include `permanentdeveloper`, `juicedomain_permanent`,
  `permanent_havok`), and `LOADING LEVEL: 'Crash1/C1_StartScreen/C1_StartScreen'`. Level loads are therefore observable
  without hooks.
Guesses (untested):
- `archives/update.pak` is an optional override archive the game looks for at start; possible route to load changed data.
- Havok physics (chunk name `permanent_havok`).

## Strings in the decrypted image (2026-11-01)
83,075 printable strings (>= 6 chars) extracted from the in-memory dump. Observed (names only; whether the code behind
them runs in this game is UNKNOWN until checked in Ghidra / at runtime):
- Alchemy entity network layer: `CNetwork`, `CNetworkConfig`, `CNetworkConnectionEvent`, `CNetworkEntityTokenComponent`,
  `CNetworkAnimationReplicaComponent`, `CNetworkDisableDefaultReplicationComponent`, `CNetworkDisableFieldReplicationComponent`,
  `CEntityJipPeerNetField` (join-in-progress), `CGameVariableStoreNetField`, `CEntityCreateAndAddComponentNetworkEvent`,
  `CBehaviorCharacterNetworkEvent`, `CNetSkipCutsceneEvent`, `CBehaviorSyncComponent`, `CMatchmakingOpenLobbyEvent`,
  `COnPeerConnectedEventList`, `CNetworkTaskDisconnectAll`. Messages: "Joining game...", "NetJoinInProgress",
  "%s: Processing session change. Disconnecting all existing peers.", "%s: Failed to create network object 0x%I64x of type %s",
  "%s: Invalid Trigger sent from Network.  Handle is %s.  Pointer is %s."
  Counts: "Network" 201, "Multiplayer" 72, "Replicat" 78, "Session" 77, "Lobby" 35.
- Multiplayer game-mode classes (team / flag / round / spectator / scoreboard): `CMultiplayerAssignTeamsComponent`,
  `CMultiplayerFlagCaptureZoneComponent`, `CMultiplayerRound`, `CMultiplayerSpectatorComponent`, `CMultiplayerHudComponent`, ...
- Several players: `CPlayerManager`, `CPlayerList`, `CPlayerEntityManager`, `CPlayerHandleList`, "Player 1" .. "Player 12",
  `CLinearLandVehicleCoopCamera`, `CAchievementEventVehicleEnteredCoop`, `Player2RingEffect.Player2RingEffect`,
  `StopFollowPlayer2_VSC`, `_accelerationModeButtonOnlyPlayer2`.
- Debug: `CCrashDebugMenu`, `CLevelSelectData`, `AllowFreeCamMode`; Havok visual debugger server
  ("Server created and will poll for new client(s) on port "); command-line flags seen: `-gpuWrite -journal -mmtid -plugin`.
- Third-party: libcurl (HTTP/FTP/SOCKS strings), FMOD.
Derived:
- The engine build appears shared with Alchemy games that had local two-player and online play (Skylanders-era class
  names: vehicles, team modes). This is a guess about origin, not verified.
Unknown:
- Whether `CNetwork` has a working transport in this exe, whether a second player entity can be spawned, whether the
  debug menu can be opened.
- Ghidra 12.2 headless on the 29.5 MB dump runs out of memory with the default 2 GB heap
  (`java.lang.OutOfMemoryError: Java heap space` during auto-analysis). Set `GHIDRA_HEADLESS_MAXMEM=8G` (read by
  support/analyzeHeadless.bat). Run with 8G: result pending.
- 8 GB heap confirmed from inside Ghidra (`Runtime.maxMemory()` = 8192 MB) and auto-analysis still dies with
  OutOfMemoryError, even with Decompiler Parameter ID / Switch Analysis / Aggressive Instruction Finder / Function ID /
  Non-Returning Functions - Discovered / Apply Data Archives disabled. Not a plain heap-size problem: some analyzer
  runs away on this image (or on my dump's headers). Next: whitelist only core analyzers
  (scripts: local, to be published with the write-up), then re-enable the rest one by one to find the culprit.

## Targeted Ghidra pass (2026-11-01)
Method: import with `-noanalysis` (4 GB heap), script TargetedStringUsers.java: find needle strings, scan executable
memory for RIP-relative operands pointing at them (or at 8-byte pointers to them), map hits to functions via `.pdata`
(95,084 entries), decompile only those. Ran in minutes, no memory problem.
Observed (addresses are absolute in the dumped process, image base 0x7FF636B20000; RVA = addr - base):
- "Processing session change. Disconnecting all existing peers." is used by real logic (RVA 0x1A292E): it walks a
  peer/entity list and removes entries. Network session code is compiled in. Reachable in this game: unknown.
- "Joining game..." only appears in a 4.5 KB function that fills a table of UI text ids (localisation table), not join logic.
- `CCrashDebugMenu` (RVA 0x82E920), `CPlayerManager` (RVA 0x253B40, object size 0x68), `CNetworkConfig`, `CLevelSelectData`:
  the hits are Alchemy type registrations (common call RVA 0xDF02C0 with name, size, constructor). Proves the types exist,
  not that they are used.
- `SteamNetworking`: standard Steamworks context init that fetches every interface (SteamUser, ..., SteamNetworking);
  every steam_api game has it. Not evidence of online play.
- "A new network client has been received": Havok hkBase visual-debugger server, not gameplay netcode.
- `-journal`: NOT a command-line flag. Allocation-failure return value 7 and the "-journal" suffix match SQLite's
  rollback journal. Correction of the earlier guess.
- `update.pak`: no direct code reference (built from a format string or path table; unknown).
Derived:
- Strings alone overstated the multiplayer leftovers. What is established: the Alchemy network session layer is compiled
  into the exe. Whether it is ever instantiated, and whether more than one player entity can exist, needs a runtime check.

## Runtime: driving the test copy and finding the player position (2026-11-01)
Observed:
- Menus: intro skips with Enter; title says "click to start" (needs a mouse click); fresh APPDATA shows the Activision
  licence/privacy screen (accepted with the owner's OK); main menu -> Crash 1 "Nouvelle partie" -> `crash1/c1_intro/c1_intro`
  cutscene (skip needs two clicks) -> `crash1/l101_nsanitybeach/l101_nsanitybeach`.
- Keyboard (French PC defaults on this machine, unverified elsewhere): arrow keys move; `W` does nothing; `Space` did
  not make Crash jump (jump key unknown).
- Synthetic keys (keybd_event) and clicks (mouse_event) work while the game window has focus.
- Memory scan (read-only, private RW memory = ~4.3 GB in 1,845 regions): snapshot/diff + move/idle + direction filters.
- Player position: float3 at object+0x480, layout (X, Y, Z, w). Walking forward with Up for 640 ms: Y 625.18 -> 886.93,
  Z 175.47 -> 124.52 (beach slopes down), X 73.37 -> 76.83, w stays -0.01. Smooth per-frame updates.
- A second float4 (…, …, …, 1.0) follows in steps with lag: probably camera-related.
- Static pointers (from the exe image) into the player object, run 1 (module base 0x7FF72E520000):
  `[exe+0x1AA08A8] + 0x480` (pointer to object start), `[exe+0x1A890E0] + 0xE0`, `[exe+0x1AA1F68] + 0x230`.
  Not yet checked across a restart.
Derived (guess): Z is up (Alchemy convention as far as known; unverified).
- Restart check (same boot, module base 0x7FF72E520000 both runs): player object moved from heap 0x2779194B600 to
  0x2A60A56C600; all three chains still resolve to the same float3 and it tracks movement
  (level start (69.90, -758.33, 233.40); after 900 ms of Up: (69.10, -456.45, 226.12)).
  `[exe+0x1AA08A8]` holds the object start; position = object+0x480. The other two chains point inside the same object
  (+0x3A0 +0xE0 and +0x250 +0x230). Not yet checked: after a reboot (different ASLR base, chain is module-relative so
  expected fine), in other levels, in Crash 2 / Crash 3, after death/respawn.
- "Continuer" on the Crash 1 slot loads `crash1/l100_hub/l100_hub` (map); Enter on the map loads N. Sanity Beach.

## Demo attempt: copy A's position into B (2026-11-01)
Setup: two test-copy instances (separate APPDATA folders), both in N. Sanity Beach, windows side by side; focus switching
between instances needs AttachThreadInput (SetForegroundWindow alone and title-bar clicks fail; the focused game seems
to hold the mouse). Space = jump (in-game hint "[ESPACE] SAUTER", "[CLIC GAUCHE] TOURBILLONNER", "[TAB] INVENTAIRE").
Observed:
- Reading A and writing B at ~100 Hz works (1,279 writes, 0 failures), but B's Crash does not move.
- obj+0x480 is a derived record, rewritten by the game from another source: 7 floats = position (3), rotation (3,
  e.g. -0.006 / 0.279 / 1.569), scale 1.0; a second record follows at obj+0x49C (probably previous frame). The same
  pair exists again at obj+0x1990 / +0x19AC.
- Every other place holding the exact X,Y pair (13 in B, incl. 4 in another allocation and three with w = 0) is
  rewritten by the game within 150 ms after a write: all derived. A search for non-adjacent / x0.01 layouts gives 310
  matches, mostly coincidences: not usable.
Derived:
- The authoritative position (probably the Havok character body) is not stored as plain adjacent floats equal to the
  displayed position, or lives somewhere the scans don't cover.
Next: find the code that writes obj+0x480 with a hardware write breakpoint (debug registers), then follow its source.

## Hardware write breakpoint from an external debugger: fails (2026-11-01)
Method: own minimal debugger (DebugActiveProcess, DR0 = obj+0x480, DR7 = 0xD0001 write/4 bytes, set on all 97 threads
at the attach breakpoint, DebugSetProcessKillOnExit(false), drain events before DebugActiveProcessStop).
Observed (2 runs): attach and arming succeed (97/97 threads); while Crash walks, the debugger receives NO exception
event at all (308 events = attach sequence only), and the game exits with 0x80000004 (STATUS_SINGLE_STEP unhandled).
Guess: the writing thread is hidden from the debugger (ThreadHideFromDebugger-style anti-debug, DRM or engine), so its
breakpoint exception skips the debugger and kills the process. Unverified.
Consequence: external debugging of this exe is not usable; a hardware breakpoint needs an in-process handler
(vectored exception handler in an injected DLL).
Static follow-up: 9 functions contain both 0x480 and 0x49C as 32-bit constants (RVAs 0xA5043, 0xA670A, 0xAAD40,
0x183B50, 0x19FEB0, 0x32B940, 0x7D5D58, 0x8381C0, 0x1260D1D); the two small ones (0x32B940, 0x8381C0) READ the
transform (copy to another structure), they do not write it. Not pursued further.

## In-process probe DLL (Zig 0.17.0, `zig cc -target x86_64-windows-gnu -shared`) (2026-11-01)
Injected into the test copy with CreateRemoteThread(LoadLibraryW). Vectored exception handler (first) + DR0/DR7 write
breakpoint on all 96 threads, report written every 100 ms. Works and the game survives (the earlier "external debugger"
crashes were not anti-debug inside the game's writer thread; first in-process attempt crashed in MY handler, which
scanned 4 KB of stack past StackBase: bound the scan with NtCurrentTeb()->StackBase).
Observed, watching obj+0x480 for 8 s while walking: 1,208 writes from 3 sites (RVA of the instruction AFTER the write):
- 0x3370D6: generic 16-byte copy helper `movups xmm0,[rdx]; movups [rcx],xmm0` - copies obj+0x1990 -> obj+0x480
  (called from 0x37769A).
- 0x387661: integration: loads [rbx], [rbx+4], [rbx+8], adds scaled terms, `movsd [rbx]` (rbx = obj+0x480)
  (call chain 0x378039 <- 0x3777F6).
- 0x37799F: `movss [r15], xmm3` after a compare (clamp/snap of X), r15 = obj+0x480.
Watching obj+0x1990: 148 writes, one site, the same copy helper 0x3370D6, source = a separate heap allocation
(rdx = 0x1CD3B6AEB40 in that run, rbx/rdi = obj+0x1960), call chain 0x37E77D <- 0x361DCE <- 0x376A3B <- 0x37D7A5 <- 0x37767C.
Writing Y+150 into that source: reverted within 150 ms, player not moved -> also derived.
Derived: per frame the player transform is rebuilt as  source(elsewhere) -> obj+0x1990 -> obj+0x480 -> + movement
integration -> clamp. The authoritative state is further upstream (next probe target: the source allocation).

## Ghost demo v1: override obj+0x480 in B after every game write (2026-11-01)
Build: one source, `-DROLE_HOST` (publishes obj+0x480 x7 floats to a named file mapping "Local\CrashCoopGhostDemo"
every 2 ms) and `-DROLE_GHOST` (VEH + DR0 write breakpoint on its own obj+0x480; after each game write, overwrites
X/Y/Z with the host's; thread-local re-entrancy flag because our own write trips the breakpoint; re-arms when the
player object changes; stop file disarms both). Both copies stayed alive through the run and after disarm.
Observed:
- Shared memory works between the two test copies (host seq ~250/s, position live).
- Ghost: 9,246 breakpoint hits, 4,616 overrides, no crash.
- B's CAMERA follows A's player (B's view matches A's location as A walks), but B's Crash model is not visible there:
  the mesh is placed from a different transform (presumably left at B's start position).
Derived: obj+0x480 feeds the camera, not the character's render transform. A visible ghost needs the render-side
transform (skeleton root / world matrix), or the upstream authoritative state.
Possible by-product: a "spectator camera" (B watches A's position) already works with this method.

## Ghost demo v2: looking for the render transform (2026-11-01)
ghost2.dll: up to 4 targets (DR0-DR3) from a config file; value = host position + per-target offset measured at arm
time; applied after every game write AND pushed every 2 ms (some copies are only rewritten when the local player moves).
Tested in B (camera untouched) all 10 exact X,Y copies found outside the player object, in 4 trials (incl. the
4-record group in a separate allocation, the w = 0 vectors, the w = -1 / -71 / 73 ones): B's Crash model never moved.
Both copies survived every trial.
Strided search (X, then Y and Z at +12/+16/+32): only hit is a broadcast (xxxx yyyy zzzz) shader-constant block:
1264 x 711 (window client size), 32 and 1/32 (tile grid), a direction vector, then the player position with w = 1.
Probably a per-frame constant for foliage / lighting around the player, not the mesh transform.
Derived: the model's world transform is not an exact copy of the position in private RW memory. It may include an
offset/rotation, be computed at draw time, or live in a mapped / write-combined GPU upload buffer (skipped by the scans).
Next: follow the data flow with READ breakpoints on obj+0x480 / obj+0x1990 (who reads it, where does it go), then
decompile those functions.

## Following the reads; finding the respawn/reset path (2026-11-01)
Probe in read-or-write mode (DR7 RW=11) on obj+0x480, 8 s walking: 22 sites (3 known writers + readers), e.g.
- RVA 0xDC2CD0 (4 reads, 2x per frame): a LookAt builder - cross products, translation = -dot(axis, eye). Builds a VIEW
  matrix from the player position (camera / shadow view). Explains why B's camera followed A in ghost v1.
- RVA 0xC057D9 (leaf, no .pdata): `movss xmm0,[rcx+0x20]; subss xmm0,[rdx]` - distance to the player.
- Other readers (0x330A3C, 0x1867B3, 0x2FDD1F, 0x37C930, 0x3263E4, 0x32D1C0, 0x1AD7C4): copy the position into other
  structures (effects, audio, camera rigs); none is clearly the model transform.
Working model (derived, partly guessed): obj+0x480 = desired position; physics (Havok) turns the change into a
velocity with collisions; the physics result flows back upstream and drives the model. A 700-unit jump written into
the desired position moves the camera, not the body.
Probe keyed by (RIP + 3 exe return addresses), 20 s, Crash falls in a pit and respawns:
- one-off write (site seen once): copy helper 0x3370D6 copying obj+0x17E0 -> obj+0x480 with the level START transform
  (69.904, -758.331, 233.399), call stack 0x36B987 <- 0x36B78E <- 0x5454FA.
- 0x36B740 = FUN(component, arg): walks component+0x20 (array, count at +0x10) and calls 0x36B7B0(element, arg) on each.
  Fragment 0x36B970: copy(rdi+0x80, (**(rax+0x20)) + 0x30) = reset an element's transform from its spawn record.
- 0x5454D0 calls 0x36B740 with the global at exe+0x1A890E0 (one of the 3 stable player pointers found earlier; it
  points into the player object at +0x3A0).
- After the reset the player ends at the checkpoint (68.37, 37.00, 221.98) via other writes (copy from obj+0x6E0).
Next: decompile 0x36B7B0 (what the reset touches - physics too?), then from a DLL on the game thread: set the spawn
record to A's position and call the reset -> proper teleport of B's Crash.

## Teleport attempts via the reset path and the character controller (2026-11-01)
- 0x36B7B0 (reset element to its first saved state): heavy. Walks state lists at +0x58 / +0x60 (restores other
  entities: probably the level reset on death), then calls 0x367420 and finally copies the first saved record (+0x30)
  into element+0x80 (element = obj+0x400, so obj+0x480). Not usable as a lightweight teleport.
- 0x367420: trigger-volume membership bookkeeping (lists at +0x58 / +0x68, test 0x35A7F0), not a teleport.
- Upstream chain, probe passes on absolute addresses:
  obj+0x1990 <- copy helper from CTL+0x30 (CTL = character controller, separate heap object);
  CTL+0x30 <- function around 0x37E3D0 from CTL+0x100 (plus fields at CTL+0x120...);
  CTL+0x100 <- single instruction 0x3804BE `movsd xmm0,[rbx+0x10]; movsd [rsi+0x100],xmm0` (rbx = result of a call;
  several different source objects), reached from many call paths incl. Havok-looking code (0x10E..., 0x117...) and
  the respawn path (0x367450 <- 0x36B961 <- 0x36B78E <- 0x5454FA). Part of a large character-motor routine
  (0x380132..0x380DDB, fields +0x1B8, +0x1C0..+0x218).
- Stable pointer: CTL = [[obj+0x1910] + 0x28] (also [[obj+0x1AC0] + 0x28]); CTL+0x100 = player position.
- Ghost v2 on B's CTL+0x100 (override after each write + 2 ms push): B's CAMERA follows A again, B's model still not
  visible; after stopping, B's Crash and camera are back at B's own position. CTL+0x100 is also rebuilt every frame.
Derived: the authoritative state is the physics side (Havok body / ground probes), one step upstream of 0x3804BE.
A proper teleport needs either the engine's own "set character position" call or Havok's body setPosition.

## Upstream of 0x3804BE: Alchemy scene entity accessors (2026-11-01)
Decoded from the probe's code bytes (RVAs): 0x38048E mov rcx,[rax]; 0x380491 call 0x7C83B0; 0x380496 movss xmm1,[r14];
0x38049B mov r9b,1; 0x38049E mov r8,rax; 0x3804A1 mov rcx,rsi; 0x3804A4 mov rbx,rax; 0x3804A7 call 0x3514B0;
0x3804AC movsd xmm0,[rbx+0x10]; 0x3804B1 lea r13,[rsi+0x100]; 0x3804B8 movsd [r13],xmm0.
- 0x7C83B0: Alchemy object factory (meta type -> pooled allocation, zero-fill, construct). Returns a temp object.
- 0x3514B0 (ctl, float, out): resolves handle ctl+0x1B0 (0x520430 = handle deref: flags bits 28/29, else
  *(handle+0x28)), then 0x4E5000 / 0x4E4DF0 / 0x4E5150 fill out+0x10 (position), out+0x20, out+0x2C. 0x4E5000 =
  joint world position: entity rotation (0xBFFF80) -> matrix (0x13B7330), entity position (0xC003B0), skeleton joint
  local (0x13B7A00) transformed by it (0x13C7CD0).
- 0xC003B0 = entity world position: local at entity+0x20 (x,y,z); parent at entity+0x10 (vtable +0xA0 = world matrix).
- 0xBFFF80 = entity world rotation: quaternion at [entity+0x30]+0x10, parent via vtable +0xB0.
Observed: for the player, [[ctl+0x1B0]+0x28] (flags 0x40880006) is an entity at (139.54, -481.76, 195.02), no parent,
that does NOT move when Crash walks -> not the player's node (some other object of the level). Which object's
joint feeds the player's ctl+0x100 on the per-frame path is still unclear (rsi/rbx differ by call path).
Status: the authoritative player transform is still not identified. Suggested next step: label heap objects by class
(Alchemy type registrations at 0xDF02C0 give name, size, constructor -> vtable -> class name), so each pointer
in the chains above can be named (character controller, Havok proxy, scene entity...) instead of guessed.

## Breakthrough: Alchemy class table, the real player, and a working teleport (2026-11-01)
### Class identification (local\scripts\classid.ps1)
igMetaObject layout (observed): +0x00 meta vtable (exe), +0x10 char* class name, +0x28 parent meta (probably),
+0x38 INSTANCE vtable (exe; matches the vtable passed to the type registration call at 0xDF02C0).
Scanning private RW memory for that pattern gives 7,147 classes; vtable RVA -> name table (stable across runs).
Any object is named by its first qword (vtable). Tool: build / id <addr> / walk <addr> / find <regex>.
### Correction of everything above
The "player object" [exe+0x1AA08A8] is the CStreamingManager; +0x3A0 CCameraSystem, +0x400 CCameraSystemInstance,
so obj+0x480 is the CAMERA TARGET, obj+0x1990 sits in a CCameraBox, and the "controller" [[obj+0x1910]+0x28] is a
CSplineCamera. All ghost attempts v1/v2 moved B's camera, never Crash. (Kept above as history.)
### Real player chain (stable)
CPlayerManager = [exe+0x1A665E8]; CPlayerList = [mgr+0x10] (count int at +0x10, array at +0x20);
CPlayer = [array+0]; CActor = [[player+0x10]+0x28] (handle: object at +0x28); igEntityTransform = [actor+0x30].
- CActor+0x20 = position (x,y,z), no parent for the player. igEntityTransform: +0x10 quaternion, +0x20..+0x5F cached
  world matrix (translation at +0x50), +0x60 euler, +0x70 scale, +0x7C dirty byte.
- CActor has 76 components (igComponentList at actor+0x18), incl. CProxyComponent -> CBehaviorCharacter (Havok Behavior
  graph data), CMovementControllerComponent, CPlayerRespawnComponent, CPlayerJumpComponent, COverrideRenderMatrixComponent
  (driven by .NET VSC script), and NETWORK components: CNetworkDisablePreTeleportActorReplicationComponent,
  CNetworkPlayerMovingPlatformComponent, CNetworkEventsComponent.
- Writers: CActor+0x24 is written every frame by 0xC05833 inside 0xC05A60 = SetWorldPosition(entity, pos): converts via
  parent, writes entity+0x20, sets [entity+0x30]+0x7C = 1, entity+0x38 |= 0x400, calls vtable+0xB0(entity, 1).
  Callers 0x500F10 / 0x4FF6A0 (position-if-changed) <- 0x546838 <- 0x172704 (per-frame movement/physics sync).
  The transform matrix cache is rebuilt by 0xC002E2 (clears +0x7C).
### Teleport recipe (verified in a test copy, from outside the process)
write CActor+0x20..+0x28; set [CActor+0x30]+0x7C = 1; CActor+0x38 |= 0x400.
Result: Crash moved 578 units back along the path, Z re-snapped to the ground (-116 -> -59.6), and the next step
continued from the NEW position (physics accepted it). Without the 0x400 flag the write is overwritten next frame.
### Ghost v3 (local\src\ghost\ghost3.c) - WORKS
Host (A) publishes CActor+0x20 and quaternion (xform+0x10) every 2 ms via "Local\CrashCoopGhost3"; ghost (B) applies the
teleport recipe + quaternion every 2 ms. Observed with screenshots: when A's Crash walks left, B's own Crash model walks
to the same spot and faces the same way; B's camera follows. Both copies stayed alive, also after disarm.
Not yet: animation state (B's Crash keeps its own pose), a SECOND Crash in the same game (needs spawning a CActor),
network (both copies on one PC through shared memory).

## Second Crash: engine spawn + one-PC co-op visibility demo (2026-11-01)
- Level entity listing (local\scripts\entities.ps1, names from char* fields of the entity / its data): N. Sanity Beach has
  4 CActor: "Hero_Crash" (the player, 75-76 components), "Crash" (prototype: 0 components, position 0), "GameOver_Uka",
  and a meta template; 80 CGameEntity (crates, gems, camera splines, "*_gen_Spawned_Entity" objects).
- Spawn API: "Spawned_Entity" string -> 0x772E30 / 0x772F90 = SpawnEntity(prototype, float pos[3], float rot[3],
  owner?, ?, int, attachEntity?, ?) -> builds "<name>_gen_Spawned_Entity" and calls core 0x772CB0
  (attach path 0x504940, normal path 0x504710).
- Called on the GAME THREAD from a DLL: execute hardware breakpoint (DR7 = 1) on SetWorldPosition 0xC05A60 entry,
  caught in a vectored exception handler (clear DR7 in the context, set RF in EFlags, call once, resume).
  SpawnEntity(prototype "Crash", player pos + (0,150,0), 0) returned a new CActor with 75 components at the requested
  spot (Z adjusted by physics). Game stable. The spawned Crash does NOT take input (CPlayerList still has 1 player).
- coop.dll (local\src\coop\coop.c), injected in both test copies (config local\logs\coop-<pid>.cfg = "me other proto"):
  publishes own player pos+quaternion (Local\CrashCoop_<me>), spawns a puppet Crash, teleports it every 2 ms to the
  other copy's player (teleport recipe). Observed with screenshots: each window shows its own controllable Crash AND
  the other copy's Crash in the matching place; moving A's Crash moves the puppet in B (and vice versa). Both copies
  stayed alive through the run and after disarm.
Known issues of the demo: puppet keeps facing the camera (quaternion at xform+0x10 is not the rotation source; try
the euler at +0x60 or the actor's rotation setter), puppet has no animation (idle pose, slides), no handling of
level change / death / respawn, transport is shared memory (same PC only).

## Rotation fix + UDP transport (2026-11-01)
- Entity setters near SetWorldPosition (scan of 0xBFF000..0xC08000 for flag / dirty writes): 0xC05860 = SetRotation(entity,
  euler-ish input): compares with igEntityTransform+0x60/+0x64/+0x68, writes them, sets +0x7C = 1 and entity+0x38 |= 0x800.
  0xC055E0 / 0xC05BD0 are related rotation setters (quaternion input / parent-aware). So the authoritative rotation is
  the EULER at xform+0x60 (yaw at +0x68 for the player: +-1.57 when facing left/right); xform+0x10 quaternion is derived.
  Flags: 0x400 = position changed, 0x800 = rotation changed.
- coop2.dll (local\src\coop\coop2.c, `zig cc ... -lws2_32`): 60 Hz UDP packets (magic "CCOP", v1, seq, pos[3], euler[3],
  quat[4]); config "me proto bindIp listenPort peerIp peerPort"; puppet gets pos + euler + quat, dirty, flags 0x400|0x800.
  Tested with both copies on 127.0.0.1 (ports 27901/27902): ~60 packets/s each way, puppets show the other player at the
  right place AND facing the right way (screenshots: both windows show the same scene). Both copies stayed alive.
Still missing: animation, death/respawn/level change, internet path (NAT), automatic prototype lookup inside the DLL,
an installer/launcher, bot-driven QA.

## coop3: Steam transport, automatic prototype, level-change safety (2026-11-01)
- Steam: the game's steam_api64.dll (FileVersion 04.28.51.07, 859 exports) has the OLD flat API (interface pointer as
  first argument), no SteamAPI_InitFlat / NetworkingMessages. Interface versions in the exe: SteamClient017,
  SteamNetworking005, SteamUser019, SteamFriends015, SteamUtils009. coop3 gets them with SteamInternal_CreateInterface +
  SteamAPI_ISteamClient_GetISteamNetworking / GetISteamUser on the game's own hUser/hPipe (the game already ran
  SteamAPI_Init with app 731490), AllowP2PPacketRelay(true), SendP2PPacket (UnreliableNoDelay, channel 7),
  IsP2PPacketAvailable / ReadP2PPacket, AcceptP2PSessionWithUser for the configured friend only; packets from any other
  SteamID are dropped. Verified on one PC: interfaces and own SteamID obtained ("steamprobe"). NOT verified: real P2P
  (needs a second Steam account; same-account P2P is impossible).
- Prototype found in-process: CActor (vtable RVA 0x1592220), name "Crash" in the entity or its data (+0x40),
  zero components, not the live player.
- Crashes found and fixed, each from a test run:
  1. scanning game memory with plain dereferences crashed both copies -> every read/write goes through
     Read/WriteProcessMemory on our own process (fails instead of faulting).
  2. a puppet spawned on the map crashed every map -> level transition at exe+0xFFE0F7 (hash lookup in level teardown),
     also with no writes at all (A/B isolation test: spawn-without-writes crashed, no-spawn survived).
     Cause: SpawnEntity's 4th argument selects the memory pool (core 0x772CB0: pool = pool(prototype) unless an owner is
     given). Passing the live player as owner fixes it; adding an extra AddRef (0xE557B0, lock xadd [e+8]) moved the
     crash into the release path (0xDF3979), so no extra reference.
  3. the DLL now drops the puppet whenever the player actor changes or the puppet's vtable is no longer CActor, and
     respawns after 2 s of stable player.
  Result: DLL injected at the main menu -> map (puppet spawned) -> N. Sanity Beach: game survives, the engine destroys the
  map puppet itself, a new puppet is spawned in the level; two copies linked over UDP show each other.
Not yet tested: leaving a level back to the map, death/respawn with a puppet, Crash 2/3.

## First real two-PC test over Steam (2026-11-01)
Two Steam accounts, two PCs, real installs, release build (local\release: crash-coop.dll + coop.ps1, Steam mode,
ISteamNetworking P2P of the game's own Steamworks). Result: each player sees the other's Crash move in N. Sanity Beach.
Owner log: ~62 packets/s received, 0 rejected. First attempt on the friend's PC failed to inject from a OneDrive folder
(launcher now unblocks the DLL, warns about OneDrive and reports the failing step).
Known bug: packets are ordered by seq; if one player restarts the mod, its seq restarts at 1 and the other side ignores
it until it passes the old value (fix: accept a seq reset after ~1 s of silence, or add a session id).

## Release v5 (local\src\coop\coop-release.c -> local\release\crash-coop.dll) (2026-11-01)
Packet v3: magic "CCOP", version 3, seq, session (random per mod start), level (FNV-1a of the lower-case level name),
pos[3], euler[3], quat[4], input[48]. Old and new builds do not talk to each other (version check).
- Level name: [[exe+0x1A64690] + 0xB8] = char* (CClient), e.g. "crash1/l101_nsanitybeach/l101_nsanitybeach"
  (found by scanning for the string and naming the holders with classid: CClient +0xB8, CZoneInfoSystem
  [exe+0x1A5B690] +0x50, CStandardStreamingLevel +0x30, CZoneInfo +0x10).
- Session id: a restarted peer is accepted at once (fixes "friend frozen after a mod restart").
- Visibility: puppet only when the friend was heard < 3 s ago AND is in the same level; otherwise it is parked at
  z = -100000: the game's kill plane destroys it cleanly; the DLL notices (vtable no longer CActor) and respawns
  when the friend is visible again. On stop the puppet is parked the same way (nothing left frozen).
- timeBeginPeriod(1): Sleep(2) was ~15.6 ms, so the loop sent ~8 packets/s instead of ~60. Fixed (~60/s measured).
- Animation: ActorInput (CActor+0x1A8). Diff idle / Up / Space: +0x20 (8 bytes) button bits now (Up 0x4000,
  jump 0x01000000), +0x24 press edges, +0x78 previous buttons, +0x3C stick magnitude, +0xA0..+0xAB and
  +0xC0/+0xD0 stick vectors. Copying the whole 0xC8-byte block crashed both copies (B at 0xE557B5 AddRef on a
  copied pointer). Copying ONLY those fields into the puppet's ActorInput makes the game animate it itself
  (run / jump poses seen in screenshots; control without input: idle pose). Position is still corrected every frame.
Tested on one PC with two copies: mod restart on one side, death and respawn, leaving the level to the map and
coming back (puppets hidden/destroyed/respawned), ~60 packets/s, animated puppet. Both copies stayed alive.
Not tested: Crash 2 / Crash 3 levels; puppet spin attacks breaking crates in the viewer's world (possible side effect
of replicated input); two PCs with v5.

## Crash 2 / Crash 3, crate side effect, menu pitfall (release v6, 2026-11-01)
- Crash 2: new game -> crash2/l200_intro (playable intro path). Prototype "Crash" found, puppets visible and animated,
  no crash. Mod injected at the main menu survived the menu -> Crash 2 transition.
- Crash 3: new game -> crash3/c3_intro (cutscene) -> crash3/l300_hub (Time Twister). The "Crash" prototype there
  spawns WITHOUT an igEntityTransform (entity+0x30 = 0): nothing could be applied (applied 0, no "friend visible").
  The engine creates transforms on demand: SetRotation 0xC05860 calls 0xBFCFE0(entity) when +0x30 is null. v6 calls
  0xBFCFE0 on the game thread right after the spawn (+ position-only fallback if the transform is still missing).
  After that: puppet has an igEntityTransform, both players see each other in the hub (screenshots).
- Crate side effect (point 2): A spins next to a crate in Crash 2 -> crate breaks in A's world; teleported B to the
  same spot -> the crate is intact in B's world. Replicated input does not break the viewer's crates.
- Menu pitfall: the main menu selection follows the MOUSE. Clicking the title screen leaves the cursor over the
  centre logo (Crash 2), which selected Crash 2 several times by accident. Select with the arrow keys and check a
  screenshot before Enter.
- The white-background studio logo can be skipped: Enter ~14 s after launch.
Not tested: Crash 2 warp room and regular levels, Crash 3 regular levels (only the hub), two PCs with v6.

## First real session feedback + release v7 (2026-11-01)
Two-PC play over Steam with v6 (owner + friend, logs coop-me-*.log): many level changes handled (puppet hidden /
destroyed / respawned), ~50-60 packets/s both ways, 0 rejected. Owner's game died during a Crash 3 level change: the
log ends with "player changed -> 0x0" and no "stopped"; NO Windows Error Reporting entry, no minidump (cause unknown).
Reproduction attempt on one PC: Crash 3 hub with puppets -> pause -> Quit to the title screen: no crash. Entering a
Crash 3 level from the hub was not reproduced (level buttons are on the hub floor; navigation by screenshots is slow).
Player feedback: puppets only animate jumps; otherwise they slide.
Findings:
- CActor+0x60/+0x64 = physical velocity (0 idle, ~509/105 running), +0x74 vertical component, +0x170 = 0 idle /
  1.0 running. A puppet teleported every frame has zero real velocity, so its locomotion animation stays idle.
- v7 "velocity steering": set the puppet's velocity (+0x60, x/y) to 12 * (target - current), max 900, and let the
  engine move it; teleport only when hidden or > 150 units off. Rotation still applied each frame (euler + flag 0x800).
  Result in the Crash 3 hub: 1 and 8 teleports in ~8 s (steering does the work), puppet visibly runs (motion blur,
  stride poses in screenshots).
- v7 logs the level name on every player change, and a last-in-line vectored handler writes "FAULT code at exe+RVA
  ... stack exe RVAs ..." to the log for access violations / illegal instructions / stack overflows (does not handle them).

## Spin / crouch on the puppet (release v9, 2026-11-01)
- Button bits, ActorInput +0x20 (held): forward 0x4000, jump 0x01000000, spin (left click) 0x02000000 (~80 ms),
  crouch/slide (Shift) 0x00400000. +0x24 press flags: spin 0x10000000, crouch 0x02040000, jump 0x24000000; 0x60 always.
- Copying the sender's "previous buttons" (+0x78) made every press look already held: no longer written. Both words
  (+0x20/+0x24) are OR-latched on the sender between packets and held >= 60 ms on the receiver.
- Crouch: comes through with input replication (screenshot). Spin: never, because the attack component's update
  (0x48E3BC) first looks for the controlling player (0x1755190 / FUN_..275190) and a puppet has none, so the puppet's
  attack component ignores input (measured: A's CPlayerAttackComponent +0x30/+0x38/+0x44 4 -> 0, +0xB8 0 -> 1,
  +0x108 timer during a spin; the puppet's: unchanged).
- Probe on A's attack +0xB8: written by 0x468DEE in 0x468DB0 = StartAttack(component, index). Caller chain 0x46C2F4
  (TryAttack) / 0x46BB10 = PerformAttack(component, index, force): sends the attack data to the character, posts a
  CPlayerAttackMessage (index, active) to the actor, then StartAttack.
- StartAttack / PerformAttack on the puppet start the attack logic (+0xB8 = 1, timer) but no animation. The animation
  follows +0x30/+0x38/+0x44 (0 = spin): holding them at 0 on the puppet for 450 ms plays the spin (screenshots).
  The puppet's own update resets them to 4 every frame; writing 0 every 2 ms is enough. Verified that B's own player's
  attack component is untouched (only the puppet's +0x30 toggles).
- v9 (local\release\crash-coop.dll): on a new spin press from the friend: PerformAttack(puppet attack component, 0, 1)
  on the game thread (execute breakpoint trick), then hold the three fields at 0 for 450 ms.
Possible side effect (not verified): PerformAttack posts a real attack message, so a friend's spin next to a crate or
an enemy in YOUR world may break / hit it.

## Regression in real play with v9, fixed in v10 (2026-11-01)
Owner's v9 log from a two-PC session: 81 teleport corrections in ~70 s, 21 spins triggered; players reported jerky
movement and no visible animation at all. Cause: velocity steering (v7-v9) only worked in the one-PC lab (no latency,
short tests). Over the internet the puppet kept getting blocked (slopes, jumps, crates that exist only in the viewer's
world), fell more than 150 units behind and was snapped back several times a second.
Lesson for the playbook: a one-PC lab hides latency; test movement changes with added latency/jitter before shipping.
v10 (packet v4): position authoritative again (written every frame, as in v6), plus the friend's real velocity block
(CActor+0x60..+0x77) and moving flag (+0x170) sent in the packet and written into the puppet, so the locomotion
animation runs. Lab check: puppet crossing the viewer's screen shows changing leg poses frame to frame (20 frames,
60 ms apart), 0 teleports. Spin check (numeric): the puppet's attack component mirrors the real one (+0x30/+0x38/+0x44
4 -> 0, +0xB8 0 -> 1, timer). Test-bench pitfall: a burst-capture script and a click script both forcing focus sent the
click to the wrong window (B spun itself); use the numeric probe for spin checks.

## v11: puppet animation through the Havok behavior graph (2026-11-02)

Context: the owner reported that in v10 the friend's Crash still slid in a fixed pose and that the spin only played
on the sender's screen. Both were true; v10's claims came from over-read screenshots.

### Observations (each checked in the lab with two test copies)
- Crash's animation is a Havok Behavior graph (`Crash_behavior.hkb`). Chain: CActor -> CBehaviorComponent
  (vtable RVA 0x150C710) +0x38 CBehaviorCharacter -> +0x30 hkbCharacter (MSVC RTTI names are present in the exe)
  -> +0x80 hkbBehaviorGraph -> +0x110 hkbVariableValueSet (+0x10 word values, 96 variables). Variable and event
  names: graph +0xC8 hkbBehaviorGraphData -> +0x68 hkbBehaviorGraphStringData (+0x10 events, 359 names; +0x30 variables).
- The game drives the graph with events through FireEventById = exe+0x33FEA0(CBehaviorCharacter, id). The by-name
  wrapper is exe+0x34F6A0, which hashes the name with FNV. Event ids observed on the player: run start 81 (fired for
  the first ~10 frames of a run), stop 86, spin 193, crouch 301, jump 82/87/83. The ids are not indexes into the name
  table (the names printed for them were wrong).
- Running sets GroundLocomotionStateID (#24) 0 -> 2, RunSpeedMultiplier (#21) 0 -> 1 and TurboMotion (#18) 0 -> 1.
  On a puppet the game writes 0 into #21 and #18 every frame, which freezes the run cycle.
- The puppet's graph does run with time (its idle moves). Its own logic pulls it back to idle after a lone run event.
- Firing an event from inside SetWorldPosition (exe+0xC05A60) crashed the game.
- exe+0x3733F0(rcx = CBehaviorCharacter) is the per-character behavior variable sync. It is called once per
  character per frame on the game thread. exe+0x366836 is right after it returns. Firing events at the entry of
  0x3733F0 was stable over thousands of calls.
- Keyboard: the owner's layout is AZERTY. Movement is Z/Q/S/D; the W and A keys do nothing in game.

### What v11 does
- Sender: a persistent execute breakpoint (DR1) on 0x33FEA0 records every event fired on the local Crash. The
  packet (v5) carries the last 32 events with sequence numbers, plus #21, #18 and #24.
- Receiver: queued events are re-fired on the puppet at the entry of 0x3733F0 (DR3). While the friend's #24 is 2
  and the puppet's is not, event 81 is re-fired; when the friend's #24 is 0 and the puppet's is 2, event 86 is
  re-fired. After the sync (DR2 at 0x366836), the friend's #21 and #18 are written over the puppet's values.
- The v10 spin hack (PerformAttack plus holding the attack state fields) was removed.

### Checks run (lab, two copies, UDP)
- Run: the puppet's graph reaches GroundLocomotionStateID 2 while the friend runs. The leg stride was visible in
  close-up captures at the same moment as on the sender's own Crash. Control without events: the puppet stood in
  its idle pose.
- Spin: the puppet spins (event 193 replayed). Captured on the viewer's screen while the viewer's own Crash stayed still.
- Level change (level -> map -> level) with v11 hooks active: the puppet was dropped and respawned, with no fault.
- Packet rate is ~44/s, down from ~62/s in v10.

### Unknown / not yet checked
- Crouch/slide, jump and death replay were not checked visually.
- Crash 2 and Crash 3 graphs may use different event ids. Replay passes ids through unchanged and was tested only in Crash 1.
- Not yet tested over Steam between two PCs.

### v11 checks, 2026-11-02 (lab, two test copies, N. Sanity Beach, UDP)
- Observed: jump on A (Space) -> events 82, 87, 83 replayed on B; contact sheet of B shows the puppet taking off
  with the jump pose, coming down and landing, while B's own Crash stays still. No FAULT in either log.
- Unknown: crouch. Neither LShift nor C made A's own Crash crouch through gamectl (B only replayed idle events
  259/258), so the key injection failed. Event 301 replay is still not checked visually. Death not checked.
- Correction (owner): Crash 1 has no crouch/slide, so the failed crouch test above is expected. Crouch/slide must be checked in Crash 2 or 3. The 'crouch 301' label on event 301 seen in Crash 1 is unverified.
- Crash 2 (crash2/l200_intro), v11 lab: puppet spawned and visible, no FAULT. Menu: Enter (logo), click title, the
  selection then sits on Crash 2 ("Nouvelle partie"), Enter -> l200_intro. Crouch not reproduced: keys sent to A
  gave Shift/X/E/K -> idle events only, Ctrl -> 349, L -> 352, C -> nothing; B contact sheet with Ctrl held shows no
  crouch. Crouch key under this keyboard layout is unknown.
- Crash 3 (crash3/l300_hub), v11 lab: menu = Enter, click title, click the Crash 3 logo (client 744,380), click
  "Continuer" (750,333). Puppet spawned, no FAULT. Same event ids as Crash 1: jump 82 87 83, run 81 (x9) then 86.
  Contact sheet of B while A runs: puppet visibly striding. Ctrl on A -> 301. Spin not checked in Crash 3 (key unknown
  to the lab script). Crash 3 regular levels and the level-change crash reported earlier: not re-tested.

## Coco as the friend's model, crate sync: first research (2026-11-02)
Observed:
- Crates are not CActor/CGameEntity "named" actors: N. Sanity Beach has 52 CDestructibleComponent instances (vtable RVA
  0x14D1458), each owned by a CPhysicalEntity, with a CVscComponent (visual script) and a CDotNetEntityComponent_2.
  Prototypes "Levels\Crash1\crates\Crash_Crate_Basic" exist. The break logic seems to live in the visual script.
  Not done yet: which field changes when a crate breaks, how to break one on purpose.
- CPlayerEntityManager = [exe+0x1A68600], 12 player slots. Per slot i: +0x10+4i state (0 idle/compare, 1 despawn,
  2 load, 3 wait for data, 4 spawn hero via 0x504A10), +0x40+8i hero data request (its +0x18 = hero name,
  +0x10 = 3 when loaded), +0xA0+8i hero entity, +0x100+8i wanted hero name (igStringRef "Crash").
  State machine 0x511480(mgr, x) runs every frame; 0x4EC190(mgr, i, 1) = load step (0x17DA10 hero-data slot,
  0x18B0B0/0x1932F0/0x1A2840 chunk calls, 0x4EBDB0 pack request).
- char* -> igStringRef: exe+0xDCE380(&out, "text"). (0xDCE340 takes an object with char* at +8: calling it with a
  char* crashed the game.)
- Writing "Coco" into slot 1's wanted name makes the manager run load -> spawn for a player 1 that does not exist:
  the game crashed. Calling 0x17DA10 + chunk calls alone: no crash, nothing loaded. Adding 0x4EBDB0 with our own
  request pointer: request stayed 0 and the game died ~20 s later.
- Crash 2 intro has a live NPC_Coco (9 components, names include "Playable\Coco"); no inert Coco prototype anywhere.
Unknown: how to load the Coco hero pack without a real second player. Next idea: give the friend a real player
slot (study how the game adds a local player), or clone NPC_Coco where it exists.
- Lab note: the old evlog.dll probe crashed the game right after injection; cocoload probe (src\probe\cocoload.c).
- Crate break, observed (crash2/l200_intro, scripts\cratediff.ps1 + src\probe\cratewatch.c): teleporting Crash on top
  of a crate does not break it; jumping from there does. A broken crate's CPhysicalEntity and its
  CDestructibleComponent are freed (vtable overwritten), so "crate broken" = its component no longer has the
  CDestructibleComponent vtable. Detecting breaks on the sender is therefore a simple poll of the level's crate list;
  identity across the two games = the crate's position (entity+0x20), identical in both copies of a level.
- The free comes from a deferred destroy task exe+0x55E0C0 (task+0x18 = entity) -> exe+0x4D4A80 (generic destroy
  entity), run from the visual-script interpreter (stack C0476C DF5ADA C046B6 C07E87). Calling 0x4D4A80 directly
  would remove a crate without fruit / crate counter. Unknown: the entry point that DAMAGES a crate (the script's
  trigger). A write watch on component+0x60 saw no write. Next: watch the entity's +0x38 flags earlier in the
  frame, or find the attack -> damage message path (the puppet's spin already breaks crates in the viewer's game,
  so damage reaches crates from any CActor attack).

### Breaking a crate on purpose (2026-11-02, test copy, Crash 1 N. Sanity Beach)

Observed (probes `hitwatch`, `cratehit`):
- A crate break is a normal damage hit: `exe+0x4D63A0(CDamageData *def, attacker actor, target entity)` builds a
  `CDamageInstance` (vtable RVA 0x13FF478, filled by 0x358220), then `0x4CC900` calls the target's vtable+0x178
  (`0x4C3B00`, apply damage). The crate's message handlers (registered in 0x474200) then call `0x468130(comp, 1)`
  (next damage stage), and the crate's visual script destroys it (0x55E0C0 -> 0x4D4A80).
- Captured on a jump onto a crate: def = a `CDamageData` (vtable RVA 0x13FF3D8) whose `+0x10` object has the strings
  "COM_Impact_Victim_Basic" (+0x40) and "bounce" (+0x78). 13 such "bounce" defs exist in the level.
- Calling `0x4D63A0(any bounce def, own player actor, crate entity)` on the game thread (DR3 on 0x3733F0) breaks a
  crate in front of the player: crate gone, wumpa counter shown (25), Aku Aku mask spawned from an Aku crate.
- Not enough: `0x468130` alone (only plays the stage effect), or setting health to 0 with `0x4C8D80` first.

Derived: the receiver can replay a friend's crate break with its own player as attacker, so fruit and crate count go
to the receiver like a real hit.

### Crate sync, mod v12 / packet v6 (2026-11-02, two test copies, UDP lab, N. Sanity Beach)

- Sender: a background memory scan lists the crates (CDestructibleComponent that is in its own entity's component
  list; vtable copies on thread stacks gave 2 fake hits before that check) and one "bounce" CDamageData. Every 100 ms
  the mod checks each crate's vtable; a crate whose vtable is gone has been broken, and its position goes into the
  packet (last 8 breaks plus a sequence number). If more than 3 crates vanish at once, the mod treats it as a level
  reload, sends nothing and rescans.
- Receiver: finds its own crate within 25 units of that position and calls 0x4D63A0(bounce def, own player, crate
  entity) from the game-thread hook (DR3 at 0x3733F0).
- Test that ran: A jumped on the crate at (-8.2, -142.1, -46.2). A's log says "crate broken here ... (sent)", B's log
  says "friend broke the crate ...: breaking ours", B's crate list no longer has it, and screenshots of A and B from the
  same spot show the same crates.
- Not tested: B to A (same code), multi-hit crates (bounce crates), nitro/TNT, death and checkpoint reloads, Crash 2
  and Crash 3, play over Steam.

### Shared wumpa counter, crate fixes, puppet parking: mod v13 / packet v7 (2026-11-02)

Observed (test copies):
- The real wumpa counter is a `CGameVariableValue` (vtable RVA 0x155F090) holding an int at +0x10. Writing it
  works: written 97, the game counted 97 -> 98 from there and the HUD followed. Writing 99 also stuck. The pass from
  99 to 100 was not tested: the only crate broken after it was a life crate.
- The player's HUD script component (CVscComponent 0x13BBBD0, [+0x38]+0x10 = "Scripts.Graph.CrashBandicoot_HUD")
  keeps a display copy at +0x80. Writing that copy does nothing: the HUD rewrites it.
- A `CGameIntVariable` "WumpaCount" (namespace "gamevariables") exists, but its +0x18 stayed 0 and a write watch on it
  saw nothing during a pickup.
- Breaking a crate far from the player with 0x4D63A0 gives the player no fruit (that is the user's report).
- In the lab the crate list "lost" a crate about 6000 units away (it was unloaded, not broken).
- After a crate breaks, its component can keep its vtable while its entity is already freed: the old `crates.ps1`
  counted those, the mod's membership check does not.

Mod v13:
- Scan: the background scan also lists every CGameVariableValue. The mod keeps those whose value equals the HUD copy
  each time the copy changes, until one is left; that one is the counter.
- Sending: each player sends the total fruit it gained itself (`fruitTotal`); the friend adds the difference to its
  own counter, capped at 99 (the next real fruit makes the game do the 100 -> life step).
- Until the counter is found, the friend's fruit waits; the first fruit you pick up finds it.
- No echo or double count: for 4 s after replaying a friend's crate, our own gains are not sent and are taken off the
  friend's incoming fruit.
- Crate sends: only crates within 600 units of our player are sent, and the same spot is sent once.
- Puppet parking: when the friend is absent, the puppet is parked 100000 units above the level instead of below it.
  Parking below let the kill plane destroy it, and the user's game crashed at exe+0xDDE1D0 after the second such
  destruction (the link is likely, not proven).

Lab test (UDP, N. Sanity Beach):
- A found its counter and received B's 10 fruit (6 -> 16). B received A's 8 fruit (10 -> 18).
- A crate B broke near a crate A still had was broken in A as well ("breaking ours").
- Not tested: the 4 s anti-echo window, the above-level parking, Crash 2 and Crash 3, play over Steam.

### Coco as the friend's model: works, mod v14 (2026-11-02, two test copies, Crash 1)

Observed:
- Hero memory pools, one per chunk id: exe+0xDF0AD0(id) gives the pool. Id 92 (the local hero) has memory (+0x20
  block, +0x28 size 0x8E62000, about 149 MB). Ids 90, 91 and 93-96 have none, which is why loading a hero for player
  slot 1 failed: the request came back null, and the manager crashed in state 3.
- Writing "Coco" as slot 0's wanted name swaps the local hero to Coco: the game logs "Loaded chunk
  'coco:Hero_Coco_data_92'". Keeping a reference on that request does not keep Coco loaded once "Crash" is written
  back: the chunk is unloaded.
- What works: exe+0x4EBDB0([exe+0x1A61C88], &req, igStringRef "Coco", pool(92), 1) on the game thread. The game logs
  "Loaded chunk 'coco:Hero_Crash_data_92'"; Crash stays the player, and an inert "Coco" CActor (0 components) appears
  next to Crash's. The puppet cloned from it is Coco.
- Replaying Crash's behavior event ids on the Coco puppet made it shatter into pink pieces and vanish after a jump
  (events 82, 87, 83). Coco's graph numbers its events differently.
- Without event replay, the Coco puppet runs and jumps from the friend's buttons and stays alive. It was visible in both
  copies, and both moved for about 2 minutes with no crash.
- One crash, in B only, after injecting the mod a second time into the same game: exe+0x5112F3 (input processing
  exe+0x5110E0, read at address 0x39). In a fresh game with a single injection there was no crash.

Mod v14: an empty file `coop.coco` next to the dll shows the friend as Coco. The pack is requested once per level,
the Coco prototype is looked for for up to 8 s (fallback: Crash), and no events are replayed on a Coco puppet.
Not done: translating events by name for Coco; Crash 2 and Crash 3; level changes with Coco loaded; play over Steam.

### The puppet takes the friend's real hero: mod v15 / packet v8 (2026-11-02, two test copies)

- Our own hero = slot 0's wanted name in CPlayerEntityManager (+0x100, a char*). It is sent in the packet (`hero`).
- The receiver spawns the puppet with that hero. If it is not its own hero, it first loads that pack with the v14
  trick: exe+0x4EBDB0 into pool(92). This works both ways: a Coco player gets "Loaded chunk 'crash:Hero_Coco_data_92'"
  and a Crash puppet.
- Events are replayed only when the puppet's hero equals the friend's hero, so the behavior graphs match. The
  "Show my friend as Coco" option still makes a Crash friend look like Coco, without events.
- If the friend switches hero, the old puppet is retired (kept parked above the level, never left to the kill plane)
  and a new one is spawned.
- Bug fixed on the way: find_prototype must skip the mod thread's own stack. Its locals (the CActor vtable value and
  the hero name) matched first, and the spawn then failed.
- Test that ran: A switched to Coco by writing slot 0's wanted name (as the game's own hero switch does; the friend's
  100% save shows "CHANGER DE HEROS" on the Crash 1 map). B saw A as Coco with replayed events (158, 163...): running,
  jumping, spinning, alive. A saw B as Crash.
- Side note: loading a 100% save in a test copy unlocks Steam achievements on the account running it.

### Crash on level changes with a second hero pack: mod v16 (2026-11-02)

Observed:
- The owner's real game crashed twice with v14 and "show my friend as Coco" on, at exe+0x768E00 (20:31) and
  exe+0x375832 (20:37). The offsets come from the Windows Application log, event 1000.
- Reproduced in the lab: with Coco's pack loaded next to Crash in pool 92, going back to the map and entering a level
  again crashed the game itself at exe+0x768E00, while it spawned the PLAYER hero (stack 0xDE22EB 0x768CAE 0xCB715B
  0x74CBAD 0x494310 0xC06D68 0x4D758D 0xC06E93 0x5046E4 0x4D0540), before the mod did anything in the new level.
- Asking for the pack again in each level, and waiting 3 s before the prototype search, did not prevent it. Note: the
  player actor keeps the same address from one level to the next, so anything keyed "per actor" is not per level.
- With cross-hero pack loading disabled: three map <-> level round trips on both copies, no crash, and the puppet
  respawned each time.

Mod v16: the puppet only uses our own hero. A friend who plays the same hero gets full animations; otherwise they
appear as our hero, driven by their buttons only. Cross-hero loading stays behind a `coop.experimental` file, for
research. Open question: how to drop the extra request from the loader's cache ([exe+0x1A61C88]+0x18, keyed by name
and pool) before a level change.

Lab mishap: the owner's launcher was open and auto-enabled the release mod inside the test copies, and that
overwrote coop-me.log. Since v16, the launcher and coop.ps1 only take a game under `\steamapps\`, and the mod keeps the
previous log as coop-<name>.prev.log.

### Deaths and falls: mod v17 (2026-11-02, two test copies)

Observed (A dropped below N. Sanity Beach and died):
- A's game rebuilt all crates at once (45, then 4 more): the "more than 3 at once = reload, not sent" guard held,
  and no false crate break reached B.
- Crates A broke before dying stay broken in B, and come back in A. If A breaks them again, B logs "none here".
  Harmless.
- In B, the puppet followed A below the level and was destroyed by the kill plane ("puppet gone"), then was respawned.
  That is the path of the 18:38 crash (exe+0xDDE1D0).

Mod v17: when the friend falls more than 400 units below their highest point of the last 1.5 s, the puppet is parked
above the level. It comes back when they are up again, or when they reappear more than 600 units away horizontally (a
respawn elsewhere). Lab: B logged "friend falling: puppet parked above the level" then "back on their feet"; there
was no "puppet gone".

### Automated two-client suite, and a disproved Coco idea (2026-11-02)

- `scripts/suite.ps1` (local lab) starts two test copies, enters N. Sanity Beach in both, injects the mod (UDP
  between the copies), drives A and asserts on both games and both mod logs: presence, crate replay (and no echo),
  fruit, fall/respawn, level change, and invariants (both alive, no FAULT, no Windows error dialog, sampled 3 times).
  A scorer self-test runs first. Last run: 15/15 checks, RESULT: PASS (mod dev16 = v18 code).
- The suite found a bug: a crate B broke for A was re-listed by a rescan before the game freed it, and B then sent it
  back as its own. Fixed in v18: spots broken for the friend are never sent.
- Startup flake: a test copy crashed by itself while starting (exe+0x9945FF, no mod loaded). The suite now restarts a
  copy that died during setup.
- Disproved: removing our Coco request from the loader cache right after loading it ([loader+0x18], key =
  exe+0xE45910(&pool, 8, exe+0xE556E0(name, 0x811C9DC5)); remove = exe+0xE03250(table, &key, key); lookup =
  exe+0xDF5A10). The removal worked ("cache removal 1"), yet the next level entry still crashed at exe+0x768E00. The
  game logged the pack as "coco:Hero_Crash_data_92", so Coco seems attached to hero chunk 92 itself. Next idea: a
  chunk of its own for the puppet's hero, instead of pool 92's chunk.

### Crash 2 and Crash 3, and loading any level from a script (2026-11-02)

- Console command found: the game builds "loadmap <level>" strings and runs them through a console object
  (exe+0x117F30() returns it; vtable+0x30(console, char*) executes, as exe+0x4C030 does). Sent once on the game
  thread (probe loadmap.c, DR0 on exe+0x511480), "loadmap crash3/l301_toadvillage/l301_toadvillage" and
  "loadmap crash2/l201_turtlewoods/l201_turtlewoods" load those levels straight from the hub or the main menu.
  Level names come from memory strings (crash3/l301_toadvillage, L201_TurtleWoods_zoneInfo...).
- The suite now enters the level with loadmap and takes the level and crate positions as parameters. Its level-change
  check reloads the level with loadmap. Results with the v18 mod: Crash 1 N. Sanity Beach 15/15, Crash 2 Turtle Woods
  15/15, Crash 3 Toad Village 15/15. So crates, fruit, the fall guard and the puppet work the same in all three games.
- Fruit counter lock: one pickup can leave two candidates (in Toad Village a crate gave 1 fruit while another game
  variable also went 0 -> 1). The counter is found at the next change; until then the friend's fruit waits.

### Checkpoints and multi-hit crates: mod v19 (2026-11-03, automated suite)

- A checkpoint crate is not freed when hit: it opens and its component stays, so "vtable gone" never saw it. The same
  is true for bounce crates, which take several hits. Every hit advances the crate's damage stage (component +0x30,
  via exe+0x468130). Since v19 the poll also sends a stage increase as a hit, and the receiver applies one bounce hit
  per message.
- Echo control: a crate hit for the friend stays listed, and anything it does in the next 3 s (stage change,
  disappearing) is not sent back. The duplicate guard on our own sends is now time-based (1.5 s), so later hits on
  the same crate still go out.
- Fruit anti-echo window: since v19 it opens only when our player is within 300 units of the replayed crate. A 4 s
  window opened for any replayed crate swallowed our own fruit (the Crash 3 run failed check 3 once).
- Crate type names are readable from the crate entity (e.g. "Levels\Crash1\crates\Crash_Crate_Checkpoint",
  "Crash1\Crates\BounceCrate"). Script: scripts/cratenames.ps1. N. Sanity Beach checkpoint: (-14.6, 7374.6, 2074.5).
- Suite check 6 (checkpoint): A opens the checkpoint crate, B's opens too, B dies, and B must respawn at the
  checkpoint (without the sync it respawned at the level start: y -400). The jump key was sometimes not taken while
  standing on the crate, so the mod has a file-gated test hook: coop-<name>.hit "x y z" hits our own crate with the
  damage function, and the normal detection sends it.
- loadmap from the main menu crashed the game twice at exe+0x75BBE8 right after the hero loaded. From the Crash 1
  map it works, and the suite goes through the map first.
- Results with v19: Crash 1 N. Sanity Beach 19/19, Crash 2 Turtle Woods 15/15, Crash 3 Toad Village 15/15.

### Freeze fix: game-thread work moved to the hero manager's update, mod v20 (2026-11-03)

Observed: twice the suite failed on everything A did (no crate, no respawn, no reload) while both games stayed
alive. A's screen was frozen, its CPU was still busy (the GPU driver thread), and the mod's event replay counter had
stopped. Thread sampling (probe threadrip.c) showed the main thread waiting at exe+0xDF8AF3 and all worker threads
waiting at exe+0xDF8AE6. The last mod action had been replaying the friend's hit on a bounce crate.

Derived: the behavior-sync hook exe+0x3733F0, where the mod ran crate hits and pack requests, runs on the game's
worker threads too, and damaging a crate from there could deadlock.

v20: those tasks run from DR0 on the hero manager update exe+0x511480 (main thread). DR0 is lent to the spawn hook
while a puppet spawns, then goes back. Results: Crash 1 19/19 twice in a row, Crash 2 15/15, Crash 3 15/15.
Players on v13-v19 could see the same freeze in a real game.
