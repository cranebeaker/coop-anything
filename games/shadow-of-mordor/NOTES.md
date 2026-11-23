# Middle-earth: Shadow of Mordor: research notes

Raw notes, newest phase last. Paths use `<game dir>`. Goal: online shared-world co-op of
the main campaign (two players in one Mordor, one Nemesis world).

## Session 2026-11-19: triage (phase 0)

Nothing in this session changed a game install. The exe was hashed and its version info
read; everything else comes from web sources or public GitHub metadata.

### Observations

| | |
|---|---|
| Title | Middle-earth: Shadow of Mordor (Monolith Productions / WB Games, 2014) |
| Store | Steam, app 241930, depot 241931 |
| Build id | 7348332 |
| Main exe | `<game dir>\x64\ShadowOfMordor.exe`, FileVersion 1.0.1951.29, 27,930,624 bytes |
| SHA-256 | `f5d72578f7e5fcf05e6d15d8bd231f763078620c8545f8464bb186e842279455` |
| `x64\bink2w64.dll` SHA-256 | `3c8a28354d790755a53902cc6390091dcbaa2e2a4d9e6778c8cee7fa5aae30fe` |
| Data | `*.arch05` archives (base, patch, DLC layers), `x64\default.archcfg` |
| OS | Windows 11 |

- Engine: LithTech-derived; Monolith calls it "V6", Shadow of War's Firebird "V7"
  (see [games/shadow-of-war/NOTES.md](../shadow-of-war/NOTES.md) and
  [Wikipedia: LithTech](https://en.wikipedia.org/wiki/LithTech)).
- Shadow of Mordor ships a `bink2w64.dll` in `x64\` like Shadow of War, so the same DLL-proxy
  loading route is likely available (unverified for this game).
- Servers shut down 2021-01-12
  ([Worthplaying](https://worthplaying.com/article/2021/1/16/news/123693)).

### Existing work

- [JustAddMP/ShadowOfMordor](https://github.com/JustAddMP/ShadowOfMordor): placeholder,
  "planned", created 2026-10-17, size 0, no pushes since (GitHub API, 2026-11-19).
- No co-op or multiplayer mod found: web search, GitHub repository search for
  "shadow of mordor" co-op/multiplayer (first ~18 of 28 results read), Steam forum
  threads via search results, 2026-11-19. Monolith shipped it single-player only.
- No Mordor issue or PR in cranebeaker/coop-anything (GitHub search, 2026-11-19).
- Claim issue: cranebeaker/coop-anything#418 (2026-11-19).
- Possibly useful tool: [WarpZephyr/MordorUnpacker](https://github.com/WarpZephyr/MordorUnpacker)
  (archive unpacker; not read yet).
- Several 2026 repos offering Mordor "trainers", "cheats" or "save editors" with sudden
  star counts match the usual malware-lure pattern. Not downloaded; don't use.

### Derived

- Triage row: closed binary, no multiplayer, no script source → build from zero, "very
  hard". The Shadow of War research (V7) is the nearest prior work; how much of its
  function map carries over to V6 is unknown.

### Unknown

- Whether SteamStub / Denuvo is present on this build.
- Whether the console registry, `debug_print` and the Faction (Nemesis) functions found in
  Shadow of War exist in the same form here.
- Reddit, ModDB, Nexus, Discord and non-English searches: not done yet.

## Session 2026-11-19: workspace (phase 2)

### Observations

- Settings live in `%USERPROFILE%\Documents\WB Games\Shadow of Mordor\` (`Game.ini`,
  `render.cfg`, `settings.cfg`). No `%LOCALAPPDATA%\WB Games` folder exists on the test PC.
- The only save file found is `GameData.sav` (exactly 1 MiB) in
  `Steam\userdata\<id>\241930\remote\`, i.e. Steam Cloud (`ISteamRemoteStorage`) storage, with
  no local copy beside the settings. Shadow of War kept both a local save and a cloud copy;
  Mordor appears to keep only the cloud one (one profile observed, not yet confirmed by a launch).
- Test copy made with `robocopy /E /COPY:DAT` (a real copy, per methods.md). Real settings and
  save backed up and hashed before any launch.

### Derived

- Redirecting file paths inside the game will not isolate a test copy's save: the save goes
  through Steam's remote-storage API, so `FileWrite` **and** `FileRead` must be served from a
  sandbox (see the methods.md lessons from Shadow of War) before the first launch.
- Copy check: 335 of 335 files, 42.67 GB; names and sizes identical to the store install;
  every file in `x64\` SHA-256-identical; `fsutil hardlink list` on three large `.arch05`
  files lists one path each (not hard links).
- Tools: Ghidra 12.2 (release zip SHA-256 `2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67`,
  checked against the GitHub release digest), Temurin JDK 21.0.13+9 (winget, installer hash verified).

## Session 2026-11-19: first static look at the exe (phase 3)

Read-only analysis of the test copy's `x64\ShadowOfMordor.exe` (SHA-256 as above). No launch.

### Observations

- **PE layout.** x64, image base 0x140000000, PE timestamp 1608406735 (2020-12-19, i.e. a
  post-shutdown patch). Sections: `.text` (0x139FADF, entropy 6.44), `.interpr`, `.rdata`,
  `.data`, `.pdata` (full exception table, 0x1266CC), `.SHARED`, `text`, `data`, `.rsrc`,
  `.reloc`. **No `.bind` (SteamStub) section and no Denuvo-style section**; entry point is in
  `.text`. Code looks like plain compiler output, so static analysis of the file is direct.
- **Engine is Firebird, not an older engine.** The export table names the module
  `Firebird_C.exe`, and embedded build paths read `D:\proj\Firebird\code\External\...`
  (Havok `hk2012_2_0_r1`, PathEngine SDK). This **contradicts**
  `games/shadow-of-war` ("Shadow of War is its first game", Mordor on "V6"); Monolith's V6/V7
  naming may be a version label inside Firebird. Proposed correction for the Shadow of War
  write-up and `engine-families.md`.
- **Exports.** 241 named exports: Wwise (`AK::SoundEngine`), Scaleform, and a few engine
  globals (`_pLTMemoryLinkageExported`, `g_LEngineDefaultPoolId`, `g_pAssertHook`).
- **Imports of note.** `steam_api64.dll`: `SteamNetworking`, `SteamMatchmaking`,
  `SteamMatchmakingServers`, `SteamGameServer`, `SteamGameServer_Init/Shutdown`,
  `SteamRemoteStorage`, plus the usual init and callbacks. `WS2_32` (32 imports,
  `getaddrinfo`). `SHGetFolderPathA` (single shell-folder import; likely the Documents
  settings folder). `bink2w64.dll` (17 imports), so a `bink2w64.dll` proxy is a viable load
  route, as on Shadow of War.
- **Same console-program families as Shadow of War** (string dump): `Faction.*` (148 strings:
  `Faction.Dump`, `Faction.AddEvent`, `Faction.EventReplay*`, `Faction.Cheat.ForceNextRole`, ...),
  `FactionMember*` (84), `SimEvent` (118), `SlowMO.Start/Stop/Pause/Resume`, `Teleport`,
  `GameServerShell.AdvanceTimeOfDay/ChangeWeather/PrintPauseFlags`,
  `CGameServerShell::ResetWorld`, `Presentation.Replay.Record/Load`.
- **Netcode remnants.** `..\..\net\NetStateMachine_HostVote.cpp`, `Net.NSM.HostVote.Timeout`,
  `NetMeshGamePort` ("Specifies the port to use for the game NetMesh when hosting"),
  `kNetState_Start for mesh(0x%08x) NetNodeID(%d)`, lobby enums (`ELobbyMode`, `ELobbyState`,
  `ELobbySlotType`, `ELobbyClientPermissions`), `PREGAMELOBBY`.
- **Saves.** `GameData.sav` string present next to `SteamRemoteStorage`; consistent with the
  only save being in Steam cloud storage (see workspace session).
- **Online features.** Vendetta (`ImportVendettaTarget`, `GainSharedVendettaTarget`, ...) and
  WBID (`CreateNewWBID`, WBID menu layers); servers are off since 2021.
- **Shadow of War signatures (README table, build 3095001): 0 of 9 function signatures
  match.** The RNG LCG constants do: `0x0BB38435` and `0x3619636B` each appear 142 times in
  `.text` (first at RVA 0x473091 / 0x473097), so the same 64-bit LCG is used and heavily
  inlined.

### Derived

- The Shadow of War function map does not carry over byte-for-byte; functions must be found
  again (from the console-program strings and source-file names, then xrefs). The *design*
  findings (Faction event pipeline, one global LCG, console registry) very likely do, given
  the same strings and constants. Not verified.
- `GameServerShell` + NetMesh + HostVote suggest the LithTech client/server split and a
  peer-mesh layer still exist in the binary. Whether any of it runs in single-player: unknown.

### Unknown

- Where the RNG state lives and whether there is a non-inlined `RandomFloat`.
- Whether the console registry (linked list of named programs with descriptions) has the
  same layout as Shadow of War's.
- What the `.interpr`, `text` and `data` sections are (small; possibly Havok script or a
  third-party library).

### Tools

- Ghidra 12.2 needs **JDK 25+**; JDK 21 fails with "JDK 25+ (64-bit) could not be found".
  Temurin JDK 25.0.5+8 installed via winget (hash verified). Headless analysis started.
- Second failure, with JDK 25: `ERROR: Bundle org.apache.felix.framework [0] The data file must
  be inside the data dir` then a `NullPointerException` on `dataFile`. Cause: the agent ran
  inside a packaged (MSIX) Windows app, which virtualizes writes to `%APPDATA%` into the
  package's own `LocalCache`, so Ghidra's OSGi cache path and its canonical path differ. Fix:
  add `VMARGS=-Dapplication.settingsdir=<lab>/ghidra-settings` and
  `VMARGS=-Djava.io.tmpdir=<lab>/ghidra-tmp` to `support/launch.properties`. Headless import
  then started normally. (Candidate playbook note for methods.md section 3.)

## Session 2026-11-19: Ghidra pass (phase 3)

Ghidra 12.2 headless full auto-analysis of the test copy's exe: succeeded, **18,070 s (~5 h)**
with an 8 GB heap. All later scripts ran with `-process -noanalysis -readOnly`.
RVAs below are relative to image base 0x140000000, build 7348332 only.

### Observations

- **Console registry entry layout (same as Shadow of War's):** 0x40-byte static entries in
  `.data`: `+00` type pointer (`13BAFD0` for programs), `+08` name, `+10` description, `+18`
  source file, `+20` line, `+28`/`+30` list links (point at the entry itself on disk, so the
  list is linked at startup), `+38` handler. Examples, all from `SharedFactionHierarchy.cpp`:

  | entry RVA | name | line | handler RVA |
  |---|---|---|---|
  | `1802580` | `Faction.SimEventDump` | 0x24C4 | `8DE3E0` |
  | `18025C0` | `Faction.Save` | 0x24D5 | `8DE460` |
  | `1802600` | `Faction.Dump` | 0x24DC | `8DE460` (same handler as Save) |
  | `1802640` | `Faction.Load` | 0x24F4 | `8DE530` |

  Other registry entries found by string reference: `Faction.AddEvent` (entry near `18011B8`),
  `ValidateResurrection` (`1801BE8`), `NetMeshGamePort` (`17FA3A8`), `Net.NSM.HostVote.Timeout`
  (`188FC88`), `Teleport` and spawn-point teleports (`17F9F20`-`17F9FA0`).
- **RNG.** `FUN_140473080` is `RandomFloat(float max)`: `state = state * 0x0BB38435 + 0x3619636B`,
  returns `((state >> 32) & 0xFFFFFF) * max * k`. State: a 64-bit global at **RVA `18F37F0`**
  (zero in the file, seeded at runtime). The other 141 LCG sites are inlined copies; whether all
  of them use `18F37F0` is not checked yet.
- **Save name.** `FUN_14057A800` writes `"GameData.sav"` into the profile struct (`+0x71C`) and
  the localized `SaveLoad_Profile_SubTitle` string.
- **Steam P2P transport is present in the binary (block around `EA4xxx`-`EAAxxx`):**
  - `FUN_140EA4DB0`: `WSAStartup`, `gethostbyname("")`, then
    `SteamGameServer_Init(0, 8766, <game port from object +0xE0>, 27016, 3, "1.0.0.0")`
    (mode 3 = authentication and secure).
  - `FUN_140EA9BE0`: receive loop, `ISteamNetworking` vtable `+0x10` (`ReadP2PPacket`) into a
    1,400-byte buffer, finds the peer by 64-bit Steam ID in a locked peer list and passes the
    packet to `FUN_140EA95C0`.
  - `FUN_140EA9CF0`: broadcast, vtable `+0x00` (`SendP2PPacket`) to every peer whose state is 1,
    send type 0, channel 1.
  - About 15 more functions here call `SteamMatchmaking` (lobbies).
- **Net state machine.** `FUN_140E92400` logs "reached kNetState_Start for mesh(..) NetNodeID(..)";
  `FUN_140E92FC0` logs "Host vote failed" and calls a mesh vtable `+0xD0` (likely a shutdown).
  Both log through `FUN_140E90860`.
- `SteamRemoteStorage` callers: `E01E10`, `DF93F0`, `DF9450`, `DF9490`, `DF94D0`, `E87240`.
  `CGameServerShell::ResetWorld` is in `FUN_140C35D00`; `SlowMO.Start` handler near `C37610`.

### Derived

- The console registry can be walked at runtime exactly as on Shadow of War (same entry
  layout); `Faction.Save`/`Faction.Load` give a ready-made whole-Nemesis-world serializer to
  test as a sync path. Not run yet.
- The Steam P2P transport, lobby code and NetMesh/host-vote state machine form a complete
  networking stack in the shipped binary. If the client/server split (`GameServerShell`) still
  works, co-op could reuse the game's own replication instead of a new one. **Whether any of
  it is reachable or functional in single-player: unknown.**

### Corrected

- A first heuristic (most-referenced global near LCG sites) pointed at `18640A0`; that is a
  particle-system type table (magic `0x12345678`, "ParticleSystem"), **not** the RNG state.

### Next

- Decompile the callers of `FUN_140EA4DB0` and the NetMesh hosting path to see what starts
  the network stack, and whether anything in single-player calls it.
- Map the `Faction.Save`/`Load` handlers and the file format they write.
- Build the test-copy guard (save sandbox, `ISteamRemoteStorage` block, offline sockets)
  before the first launch.

## Session 2026-11-19: test-copy guard built and tested offline (phase 2, isolation)

The guard's code and tests are our own and not published yet; results are described here.

### What the game needs isolated (observed)

- The exe fetches the cloud interface through its `steam_api64.dll` import `SteamRemoteStorage()`
  every time (6 callers, all through the import). `steam_api64.dll` (2.01.77.37) exposes
  `STEAMREMOTESTORAGE_INTERFACE_VERSION012`.
- Decompiled callers use exactly four vtable slots: `+0x00` FileWrite(name, data, size),
  `+0x08` FileRead(name, buf, size) (returns -1 when cloud is off), `+0x18` FileDelete(name),
  `+0x60` GetFileSize(name). This matches the v012 slot order
  (FileWrite, FileRead, FileForget, FileDelete, ..., FileExists 10, FilePersisted 11, GetFileSize 12).
- One shell-folder import (`SHGetFolderPathA`), used for `Documents\WB Games\Shadow of Mordor`.
- `bink2w64.dll` (version 1.994c) has 83 named exports, ordinals 1-83.

### Built: `somguard` (a `bink2w64.dll` proxy)

- 83 forwarders to the original renamed `bink2w64_.dll` (`/export:Name=bink2w64_.Name,@ord`).
  MSVC 14.51, `/W4 /WX`, static CRT.
- **Cloud saves by import patch, not vtable patch:** the exe's `SteamRemoteStorage` import
  returns a proxy object whose slots 0/1/3/12 read and write `<sandbox>\cloud\`; every other
  slot terminates the game (`0xC0DE0021`) because the game was never seen calling it. The
  real interface is never called. Simpler than Shadow of War's vtable rewrite, and possible
  because Mordor always goes through the import.
- `SteamAPI_RestartAppIfNecessary` import -> `false` (Steam can never relaunch the real install).
- `SHGetFolderPathA` import: Documents, AppData and LocalAppData -> sandbox folders.
- Network: the exe's `connect`, `sendto`, `getaddrinfo`, `gethostbyname` imports (ws2_32
  ordinals 4, 20, by-name, 52) fail with "network unreachable" / "host not found" for anything but
  loopback, the "unplugged PC" behaviour Shadow of War needed. Only the exe's own imports are
  covered (not other DLLs or `GetProcAddress` lookups); acceptable while WB's servers are off.
- Whole-process write protection: ntdll `NtCreateFile`, `NtOpenFile`, `NtDeleteFile`,
  `NtSetInformationFile` hooked by copying the 24-byte syscall stub (same layout as on Shadow
  of War; **confirmed on Windows 11 build 26300**). Write-intent access (write/append/attributes/
  EA/delete/DAC/owner/generic/maximum-allowed, any create disposition, delete-on-close, rename or
  link target) to a protected path terminates the game (`0xC0DE0010`). Paths are normalized from
  `\??\`, `\?\`, `\Device\HarddiskVolumeN\` (QueryDosDevice table), root-directory handles,
  `..`, forward slashes, case, and 8.3 short names. Protected: real `Documents\WB Games`,
  Steam `userdata` (both derived from the registry at runtime), `steamapps`, the backups,
  the guard source.
- Start-up refusals: exe outside the lab root or inside a `steamapps` folder (`0xC0DE0002`),
  exe size/SHA-256 not this build (`0xC0DE0003`), `steam_appid.txt` not 241930 (`0xC0DE0004`),
  ntdll stub layout unknown (`0xC0DE0005`), an import missing (`0xC0DE0006`).

### Tested offline (no game process)

Each case runs in a child process with the guard loaded in test mode against a fake tree;
the parent checks exit code, disk state and the guard log. Run on 2026-11-19:

```
PASS read_protected, write_protected, create_in_protected, delete_protected,
     rename_into_protected, mkdir_protected, attrs_protected, write_protected2,
     variant_slashes, variant_upper, variant_prefix, variant_dotdot, variant_relative,
     variant_rootdir, variant_shortname, write_free, write_sandbox, cloud_roundtrip (1 MiB,
     real slot never called), cloud_sanitize, cloud_unexpected_slot, net_block,
     net_loopback, folder_redirect, restart_app, forwarders (83/83 resolve to the original),
     game_refuse_location, game_refuse_unknown_exe
negative control: 14 of 14 protection cases fail without protection (all must)
SUMMARY: 27 passed, 0 failed, 0 skipped
```

- Real settings and save re-hashed against the backup manifest afterwards: unchanged.

### Staged in the test copy (not launched)

- `x64\bink2w64.dll` = guard, `x64\bink2w64_.dll` = original (hash equal to the store
  install's), `x64\somguard.ini` (game mode, network blocked), `x64\steam_appid.txt` = 241930.
- Sandbox: settings copied from the owner's, set to windowed 1280x720; `cloud\GameData.sav`
  seeded with a copy of the owner's save.

### Unknown until the first launch

- Whether the game starts with the proxy cloud object (it may call `IsCloudEnabled*` through
  another path we have not seen; the guard would then stop it with `0xC0DE0021` and log the slot).
- Whether processes started from a packaged (MSIX) host app inherit its `%APPDATA%`
  virtualization (seen with Ghidra); Mordor's own paths are redirected by the guard anyway.
- Whether the NVIDIA App rewrites the real `render.cfg` on launch, as it did for Shadow of War
  (outside the game process; only the after-run hash check can see it).

## Session 2026-11-19: first guarded launch of the test copy

Launched the test copy's exe directly (Steam running, backup check passed first), 300 s limit,
then the script closed it. Nobody watched the window; no screenshots taken.

### Observations

- One game process, from the test copy, for the whole run; no game process from the store
  install appeared (checked every 2 s). The guard accepted the exe, installed the four ntdll hooks
  and all import patches; the game ran with them until the time limit (no crash, no guard FATAL).
- `SteamAPI_RestartAppIfNecessary(241930)` was called at ~0.5 s and answered `false`.
- `SHGetFolderPathA` was called with CSIDL `0x8005` (Personal | FLAG_CREATE) eight times, all
  redirected; the game rewrote its three settings files in the sandbox at ~16 s.
- `SteamRemoteStorage()` first called at ~11.7 s; the proxy was used and the real interface was
  never called. **The game asks for `"\GameData.sav"` (leading backslash)**; Steam stores the file
  as `GameData.sav`. The guard first mapped the name to `_GameData.sav` and answered "missing",
  so the seeded save was not found. Fixed (leading separators stripped), new offline test
  `cloud_leading_slash`; suite now 28/28, negative control 14/14.
- After the "missing" read at ~16 s the guard logged nothing else for the remaining ~284 s:
  no cloud write, no further cloud read. What the game showed is unknown.
- Writes the guard saw outside the lab (not blocked, all from in-process drivers/overlays):
  NVIDIA driver files (`ProgramData\NVIDIA Corporation\Drs\nvdrssel.bin`, `nvAppTimestamps`,
  `ShadowPlay\CaptureCore.log`, `LocalAppData\NVIDIA\DXCache\*.nvph`), the Steam overlay's own logs
  (`Steam\logs\gameoverlay_renderer*.txt`), a shell-customisation tool's status file, AFD socket endpoints.
- **The NVIDIA App rewrote the owner's real settings, as on Shadow of War.** The game started at
  13:17:57; at 13:18:08 the NVIDIA App updated `NvBackend\ApplicationState\<id>.json`,
  `InstalledApplicationsOfflineStorage` and `ngxdlssoverridestate.json`, and in the same second
  the real `Documents\WB Games\Shadow of Mordor\render.cfg` (`AmbientOcclusionQuality` 3 -> 4,
  `DepthofFieldToggle` 0 -> 1, trailing blank line) and `settings.cfg` (trailing blank line) were
  rewritten. The NVIDIA App has `Recommendations\middle_earth_shadow_of_mordor\` and an ontology
  wrapper for the game. The game process never opened those files for writing (the guard would
  have stopped it). Both files restored from the backup; backup check then clean.

### Derived

- The in-process isolation works on this build. The remaining leak is the NVIDIA App reacting
  to *any* Shadow of Mordor process, from outside; the guard cannot stop that. Options: the owner
  turns off NVIDIA App game optimization for this game, or every launch ends with an automatic
  restore of the two settings files from the backup.

### Next

- Relaunch with the fixed cloud name and capture the window periodically, to see whether the
  game reaches the main menu and can continue the seeded save.

## Session 2026-11-19: second guarded launch (fixed cloud name, screenshots)

Same launch script, plus a window capture every 30 s (`PrintWindow` with `PW_RENDERFULLCONTENT`,
only the game's own window) and an automatic restore of the owner's real Mordor settings files
if the NVIDIA App rewrites them (approved by the owner; any other real file changing is reported,
never auto-restored).

### Observations

- `CLOUD FileRead "\GameData.sav" 1048576 bytes <- sandbox` at ~41 s: the seeded save was read
  through the proxy; the real interface was never called. No cloud write during the run.
- Timeline from the window captures: 17 s no window yet; 48-205 s black window with a loading
  spinner (bottom right) and an FPS counter (~96 fps, likely the NVIDIA/Steam overlay);
  **236 s: main menu** ("Play", "Marketplace", "Options", "Quit to Desktop", plus store
  tiles); still on the main menu at 268 s and 299 s. Closed by the script at 300 s.
- Start-up was slower than in launch 1 (first `SHGetFolderPathA` at 33.5 s vs 10.7 s).
- **NVIDIA App did not rewrite the real settings this time**: backup check clean after the run,
  so the auto-restore had nothing to do (it has not yet been exercised against a real rewrite).
- No guard FATAL; no game process from the store install.

### Guess (unverified)

- The ~3.5-minute black loading screen may be the game waiting on WB online services (WBID),
  which the guard makes unreachable and which were shut down in 2021. Not checked: compare a
  run with the network allowed only for the guard's log, or find the timeout in the code.

### Status

- Isolation on this build: working for two launches. The test copy reaches the main menu
  offline with a sandboxed copy of the owner's save. **QA level: none for co-op** (nothing built yet).

## Session 2026-11-19: the network stack, statically (phase 3)

Read-only Ghidra scripts (caller trees, vtable walks, data xrefs) on build 7348332.

### Observations

- **No RTTI for game classes.** Only 630 `.?AV` type names exist, all Havok, MSVC STL and an
  ATI texture codec. Class names below come from strings, not RTTI.
- **`INetOnlineService`** (string next to the engine's interface names) is a ~180-slot
  interface whose vtable starts at RVA `148F050`; built by `FUN_140E8A930` (stores the vtable,
  registers six Steam callbacks: ids 0x44D, 0x44E, 0x44F, 0x458, 0x130, 0x14B), destroyed by
  `FUN_140E89F80`. Its builder sits under `FUN_140E384F0`, which has ~200 callers (looks like
  a get-or-create singleton). Field `+0x30` is a **session** object (0x430 bytes) with a
  P2P peer list at `+0xA0` and a server/lobby object at `+0x188`.

  | slot | offset | function | role (from the decompile) |
  |---|---|---|---|
  | 1 | `+0x08` | `E8A0D0` | create the session (`+0x30`), init its server part with a port argument, read the persona name (`SteamFriends`), `SteamApps` check |
  | 2 | `+0x10` | `E88F60` | tear the session down (also called from the destructor) |
  | 3 | `+0x18` | `E8A250` | per-frame pump: `SteamAPI_RunCallbacks`, then P2P receive (`EA9BE0`) and server/lobby pump (`EA93D0`), only when a session exists |
  | 25 | `+0xC8` | `E87780` -> `EA88F0` | **host**: `SteamGameServer_Init` (`EA4DB0`), server key values (`LANOnly`, version), then `ISteamMatchmaking +0x68` (`CreateLobby` in v009) with a lobby type of 2 and a max-members argument, registers the lobby call result |
  | 26/27/28/32/33/35 | `+0xD0`..`+0x118` | `E877B0`..`E88620` | further lobby/session calls into `EA8F90`, `EA7BD0`, `EA5F90`, `EA5E80`, `EA6590` (`AppID`, `LobbyName`), `EA6610` |
  | 156 | `+0x4E0` | `E88500` | broadcast to all peers (`EA9CF0`, `SendP2PPacket`, channel 1) |
  | 73, 75 | | `E873C0`, `E87400` | `steamid`, `Achievements` |

  All of these are only reached through the vtable (no direct calls).
- **Session creation is gated only on "Steam API initialised".** Slot 1 checks byte
  `RVA 19E53D4`; that is engine-object field `+0xA94` (engine object at `RVA 19E4940`), set to 1 by
  `FUN_140E02BD0` right after `SteamAPI_Init()` succeeds and checked again at shutdown before
  `SteamAPI_Shutdown()`. So with Steam running, nothing else blocks the session.
- **Command line.** `ICommandLineArgsCommonImp` (`-cmdfile` support); the options object at
  `RVA 19D8010`, method `+0x40` = get option by name. Options seen: `workingdir`,
  `userdirectory`, `projectconfig`, `settingsconfig`, `renderconfig`, `cinematicconfig`,
  `userconfig` (config files `projectconsole.cfg`, `\USER\settings.cfg`, `\USER\render.cfg`,
  `autoexec.cfg`). No option named host/connect/join/server/port found as an exact string.
- **Client/server engine core is present:**
  - `FUN_140E05230` = create server: requires the client manager (engine `+0xCA0`; message
    "Creating a server before ClientMgr is initialized"), creates the `IServerShell`
    implementation (`thunk FUN_140EF5490`), stores it at engine `+0xC58`, calls its `+0x08` (init).
  - `FUN_140E04930` validates the **replication class table** at engine start-up: for every
    replicated class it finds the proxy class by name hash and checks a matching field
    ("Failed to find proxy specified by class '%s'", "Replication class mismatch between '%s'
    and its proxy"). Called from `FUN_140E05E40` ("shell"), which the Steam start-up function
    `FUN_140E02BD0` calls. So the replication registry is built and checked on every launch.
  - `..\..\client\clientmgr.cpp`, `Net.MaxFinalizeTime`, `AttachRemoteView` (engine init).
  - `FUN_1409AAB50` handles pause reasons `CLIENT`, `SERVER`, `PLAYERPAUSE`, `PREGAMELOBBY`.
- `CONNECT` is only the HTTP-proxy tunnel in the bundled HTTP client (`FUN_140FA1F90`).

### Derived

- The binary contains three network layers: (1) Steam platform service (lobbies, game server,
  P2P send/receive), (2) a NetMesh/host-vote state machine, (3) a LithTech-style engine
  client/server split with replicated classes and proxies. Layer 3 is the important one for
  co-op: if single-player already runs a local server shell plus a client, co-op becomes
  "attach a second, remote client" rather than "build replication from zero".

### Unknown (needs a runtime look)

- Whether single-player creates a server shell (engine `+0xC58` non-null in game) and a client
  manager (`+0xCA0`).
- Whether anything calls `INetOnlineService` slot 1 (create session) or slot 25 (host) in a
  normal session; the earlier guarded launches logged no network activity beyond blocked sockets.
- What the replicated classes are (the table `FUN_140E04930` walks) and how many there are.

### Next

- Add a read-only runtime probe to the guard: log engine `+0xA94`, `+0xC58`, `+0xCA0` and the
  `INetOnlineService` object's `+0x30` every few seconds, and dump the replicated class names
  once. No game behaviour changes.

## Session 2026-11-19: third guarded launch, read-only runtime probe

Guard with a probe thread (every 2 s, SEH-guarded reads only; logs changes plus a 30 s
heartbeat). Run limit 480 s; the script posted Enter (`WM_KEYDOWN`/`WM_KEYUP` to the game's
own window) at 262, 290, 321 and 351 s. 19 window captures.

### Observations

- **`INetOnlineService` is created and its session is opened during a normal single-player
  start-up**, with no user action: online-service object (vtable at `148F050`, checked) present
  at ~16 s, session (`+0x30`) non-null at ~18 s and stayed for the whole run. So slot 1 (create
  session) is called by the game itself, and the per-frame pump (slot 3, which reads P2P packets
  when a session exists) has a session to work on.
- `steamInit` (`19E53D4`) = 1 from the first sample.
- The probe's engine fields were **read from the wrong object**: `FUN_140E05E40` (which fills
  `+0xBA0` game shell, `+0xCA0` ClientMgr, and `CreateServer` `+0xC58`) is called with
  `*(RVA 19D80E8)`, not with the static object at `19E4940` (that is `FUN_140E02BD0`'s object, which
  holds `+0xA94`). The values logged for those three fields this run are meaningless. Fixed
  (probe now dereferences `19D80E8`), offline suite 29/29, negative control 14/14; not yet run.
- **The game never left the black loading screen in 480 s** (spinner bottom right, ~97 fps).
  It was still loading at 234 s, before the first Enter, so the keys did not cause it. Launch 2
  reached the main menu at ~236 s. Load time varies a lot between runs; cause unknown.
- Seeded save read through the proxy at ~17.7 s (1 MiB). No cloud write. Real files unchanged
  (backup check clean, no auto-restore needed).

### Guess (unverified)

- The long and variable black screen may be the online session or WBID start-up waiting on
  network timeouts (sockets fail with "unreachable"). A test: log the session's lobby/server
  calls, or compare a run with `block_network=0` but no route out (not done).

## Session 2026-11-19: fourth guarded launch, fixed probe

Same as the third launch with the engine fields read from `*(RVA 19D80E8)`. Limit 480 s; Enter
posted at 301, 330, 360, 390, 420 s. 19 window captures.

### Observations

- From the first sample (~2 s): game shell (`+0xBA0`) present, vtable `exe+13BC070`;
  ClientMgr (`+0xCA0`) present, vtable `exe+148C010`. **Server shell (`+0xC58`) stayed null for
  the whole run.** Online service present at ~2 s; its session opened by ~6 s and stayed open.
- **Replication class table: 327 classes** (game shell getter `exe+4045E0`, called once by the
  probe). Server-side object classes include `MainWorldServer`, `GameBase`, `CPlayerObj`,
  `CCharacter`, `NPC`, `CProjectile`, `Quest`/`QuestTrigger`/`QuestRelay`, `TravelPoint`,
  `ResurrectionPoint`, `InventoryItem`, `GrapplePerch`, `Combat`, `Cinematic*`, and Nemesis-world
  spawners (`AISpawnerAmbientNemesis`, `AISpawnerAmbientGangSimEvent`, `AISpawnerAmbientSimEvent`,
  `AISpawnerAmbientBodyGuard`, ...). Client-side proxy classes: `CGameBaseProxy`,
  `CCharacterProxy`, `CClientModelBase`, `CClientWorldModelBase`, `CClientKeyframer`,
  `CClientInventoryItemModel` and ~20 `CClientCinematic*`. Field `+0x3C` was 0 for 222 entries.
  Full list kept locally (not game data, but long).
- **The game stayed on the black loading screen for all 480 s again** (all captures black with
  the spinner). So the null server shell describes start-up/loading only, not a loaded world.
- Seeded save read through the proxy at ~4.4 s. No cloud write. Real files unchanged.

### Derived

- The engine keeps the LithTech object model: world objects are server-side classes with
  client-side proxies (`CGameBaseProxy`, `CCharacterProxy`). The client manager exists from start-up.

### Open question: does the probe stall loading?

- Launch 2 (no probe) reached the main menu at ~236 s; launches 3 and 4 (probe on) never left the
  loading screen in 480 s. In launch 3 the probe only read memory (wrong base, getter never
  called). Two samples against one; correlation only. Test: one launch with `probe=0`, then one
  with the probe reading but not calling the getter, each long enough to reach the menu.

## Session 2026-11-19: fifth guarded launch, probe off

Same guard and test copy, `probe=0`. Limit 480 s; Enter posted at 301, 330, 361, 390, 421 s.
19 window captures.

### Observations

- Captures: black loading screen to 205 s; middleware logo screen at 236 s; **main menu at 268 s**
  (PLAY / MARKETPLACE / OPTIONS / QUIT TO DESKTOP) and still at 330 s after two Enters.
- ~337 s: the game re-resolved Documents (sandboxed) and rewrote the sandbox `render.cfg` and
  `settings.cfg`. **345.7 s: `CLOUD FileWrite "\GameData.sav" 1048576 bytes -> sandbox (ok)`**, the
  first cloud write seen in any launch, through the proxy.
- 361 s black; 390 s and 421 s story narration loading screens; 457 s the prologue cinematic. The
  game went from the menu into a loading world.
- The written sandbox save differs from the seeded copy (hash changed). Kept as evidence in the lab;
  the sandbox was reseeded from the backup afterwards.
- No guard FATAL, the real remote-storage interface was never called, backup check after the run:
  0 changed. No NVIDIA rewrite this time.

### Derived

- Probe off: reached the menu (~236–268 s), same as launch 2. Probe on: launches 3 and 4 never
  left the black screen in 480 s. Now 2 against 2, so the probe very likely causes the stall.
  Which part (the 2 s polling reads, or calling the shell's getter from a foreign thread during
  loading) is **unknown**.
- The guard's cloud write path works with the real game.

### Unknown

- Whether the Enters chose Continue or New Game. The prologue cinematic suggests New Game, but the
  menu screen between 330 s and 361 s was not captured.
- Whether the server shell (`+0xC58`) exists once a world is loaded (probe was off).

### Next

- Probe that sleeps until after the first cloud read plus a delay, reads memory only (no game
  function calls), then samples the server shell in-world.

## Session 2026-11-19: sixth guarded launch, late read-only probe, in-world

Guard changes: the probe touches nothing until the first cloud `FileWrite`, then waits
`probe_delay_s` (60), then samples every 5 s. Every read first checks the page with `VirtualQuery`
(committed, readable, not `PAGE_GUARD`/`PAGE_NOACCESS`, whole range). The replication getter call
is opt-in (`probe_repl=1`) and was off. New test asserts: a guard page is refused and keeps
`PAGE_GUARD`; a read straddling into an uncommitted page is refused. Suite 29/29, negative control
14/14. Limit 900 s; Enter posted at 300, 330, 361, 390, 421, 520, 601, 680 s. 37 captures.

### Observations

- Main menu at 79 s this time (launch 5: ~268 s; second run in a row, warm file cache likely).
- **The game left the menu before any posted key** (first Enter at 300 s): black at 110 s, story
  narration at 141 s, first cloud write at 93 s. The owner confirmed afterwards that they started
  the game from the menu by hand; the script's keys did not do it.
- **The window was resized during the run** (captures 736x519 up to 297 s, 1229x797 from 576 s;
  sandbox `settings.cfg` WindowWidth/Height 1213x758 written ~510 s). Owner-confirmed: done by the
  owner, not the script. So this run is not a scripted test of reaching the world; it only shows
  the probe and guard behaving in a world the owner entered.
- In world: the prologue tutorial area with Talion and NPCs at 266 s; walking in the same area at
  669 s; a tutorial loading card at 793–887 s.
- **Probe, first sample 154 s (in the prologue): `serverShell` non-null, vtable `exe+1462960`.**
  Game shell and ClientMgr as before; online session open.
- **~764 s: the server shell pointer changed to a new object with the same vtable `exe+1462960`**
  while the client shell and ClientMgr stayed the same. A loading card was on screen from 793 s.
- 16 cloud writes, all `-> sandbox (ok)`. Real remote-storage interface never called. No guard
  FATAL. Backup check after: 0 changed. No NVIDIA rewrite.
- The sandbox save was kept as evidence in the lab and the sandbox reseeded from the backup.

### Derived

- **Single-player runs a local server inside the game process.** The server shell exists once a
  world is loaded, beside the client shell and ClientMgr, as in LithTech's listen-server model.
- The server shell is recreated on a world/level transition while the client side stays.
- The probe stall in launches 3 and 4 is consistent with reads touching guard pages or calling
  the getter during loading; with both removed, no stall. Which of the two was the cause is
  **unknown** (not needed now).

### Unknown

- Whether the engine still has a working remote-client path (a second ClientMgr connecting to this
  server over a transport), or the server only talks to the local client in memory.
- What the server shell vtable `exe+1462960` offers (connect, client list, message send).

### Next

- Static: decompile the server shell vtable at `exe+1462960` and the code that creates it
  (`CreateServer` e05230) to find the client-connection and message paths.

## Session 2026-11-19: the server shell, statically (phase 3)

Ghidra headless, read-only, on the analysed project. Reports kept locally in the lab. New read-only
script `SomFieldUse.java` lists functions using a given struct-field displacement.

### Observations

- The server shell is the game's **`CGameServerShell`** (strings `CGameServerShell::ResetWorld`,
  `GameServerShell.cpp`, `GameServerShell.AdvanceTimeOfDay/ChangeWeather/PrintPauseFlags`). Factory
  `ef5490` allocates 0x9F8 bytes and runs the constructor `c362f0`, which stores vtable
  `exe+1462960`. The factory refuses unless a string from its argument is empty or `"runtime"`.
- Its vtable has **12 slots**:

  | Slot | Function | What it does (from the decompile) |
  |---|---|---|
  | 0 | c365a0 | destructor |
  | 1 | c355a0 | init: sets global `g_pGameServerShell` (RVA 1989318), creates the server-side managers |
  | 2 | c34850 | term: frees them |
  | 3 | c34e10 | **builds a message (8-bit id 0x13 + payload) and sends it to the client given as argument**, through `IWBEngine` (+0x168 create message, +0x160 send) |
  | 5, 6 | c35000, c349b0 | world load / world loaded hooks |
  | 8 | c34f90 | **removes a player by client id**: finds the player in the player list (RVA 1988a90..1988a98) whose byte `+0xB68` equals the id, then deletes its object |
  | 9 | c36720 | per-frame update (AI, combat, Nemesis, time of day, world reset) |
  | 11 | c365d0 | **client-message handler**: `(shell, ?, clientId byte, ILTMessage_Read*)`, reads an 8-bit id and dispatches ids 1, 2, 4, 5, 7, 8 |
- Every per-player handler (ids 1, 2, 4, 5 and the cheat/debug handler `cf26a0`) first looks the
  player up **by client id in the player list** and acts on that player. Ids 7 and 8 set and clear
  server pause flags.
- The engine object (`*(RVA 19D80E8)`) drives both shells directly in the same frame:
  - `e06780` and `e066e0` (frame steps) call the client shell (`+0xBA0`) and then server shell
    slots 9 and 10.
  - `e052d0` / `e05840` (world start / end) call server slots 5 and 6, then the client shell.
  - `e04a80` (shutdown) calls slots 4, 6, 2 and deletes the server.

  No network layer was seen between the two shells in these functions.
- The engine interfaces are found by name through a registry (`3eecf0` registers 24 names:
  `IWBEngine.Default`, `INetOnlineService.Default`, `IGameServices.Default`, ...). The game's own
  classes have **no RTTI** (only Havok's do), so the `IWBEngine` implementation (and with it how
  "send to client" travels) was not found statically.
- `CGameModeClientHydra::EnterWorld` (`50acd0`): the client side has a "Hydra" game mode.

### Derived

- The server keeps the LithTech client/server split: a player list keyed by a **client id
  byte**, per-client messages in both directions, and remove-client by id. Nothing in the server
  shell assumes exactly one player; the list is a vector. (A design read, not a test.)
- In single-player, client and server run in one process and are stepped by the same engine loop.

### Unknown

- How `IWBEngine` delivers "send to client" and how client messages reach slot 11: an in-memory
  queue only, or a transport that could carry a remote client.
- Where a player is added (the "add client" path); not in the server shell's own slots.
- What "Hydra" is (WB online service backend? the game-mode name?), and whether it is single-player
  only.

### Next

- Read-only runtime probe (same late start, no calls), logging:
  - the vtable RVA of `*(RVA 19D80C8)` (`IWBEngine` implementation) and its `+0x128`/`+0x160`/`+0x168` entries;
  - the player-list size, and each player's client id at `+0xB68`.

  Then decompile those functions statically.

## Session 2026-11-19: seventh guarded launch (engine interface, player list) and the message path

Probe additions (memory reads only, same late start): log the vtable of `*(RVA 19D80C8)`
(`IWBEngine`) and of `*(RVA 19D80D0)`, and the player list `RVA 1988A90..1988A98` with each
player's byte at `+0xB68`. Suite 29/29, negative control 14/14. Limit 900 s; Enter posted from
121 s to 540 s (12 presses); no owner input reported.

### Observations (runtime)

- First cloud write 186 s; probe sampled from 246 s. `IWBEngine` is a static object in the exe
  (vtable `exe+1483670`). Slots used by the server: `+0x168` → `e01840`, `+0x160` → `e01df0`;
  also `+0x148` → `e01dd0`, `+0x150` → `e01db0`, `+0x158` → `e01df0`.
- **`players=0` in every sample** (246–876 s).
- Captures: menu, then cinematics 150–301 s, then a **black loading screen with spinner from
  326 s to the end (900 s)**. Only one cloud write (launch 6, owner-driven, had 16). Real files
  unchanged (backup check 0); sandbox save kept as evidence and reseeded.

### Observations (static, from those RVAs)

- `e01840` (create message): allocates a 0x20-byte ref-counted message (vtable `exe+1483950`).
- **Server → client:** `e01df0` → `e01cb0(engine, msg)` appends the message to a queue on the
  engine object (`+0xCC8`, double-buffered with `+0xCB0`). **No client id is stored**; the id
  the server passes is dropped. Each frame, `e062d0` swaps the buffers and hands every message to
  the client shell (`+0xBA0`) vtable `+0x78`.
- **Client → server:** `e01dd0` → `e01c20(engine, msg, 0xFF)` and `e01db0` → `e01c20(engine, ...)`
  append `{msg, client id byte}` to a queue at `+0xCF8` (double-buffered with `+0xCE0`). Each
  frame, `e06470` swaps the buffers and calls server shell slot 11 (`+0x58`) as
  `(shell, 0, clientId, msg)`. That is the client-message handler from the previous session.
- No socket, Steam P2P or other transport sits on either path.

### Derived

- **The engine's client/server link is two in-memory message queues on the engine object.**
  There is no network transport between client and server, so "re-enable remote clients" is not
  an option. A remote player would need a transport we build.
- The queues make a clean seam: messages are already serialised (`ILTMessage_Read` bit reader)
  and pass through two functions (`e01c20` client→server, `e01cb0` server→client). A co-op layer
  could forward them between two game instances.
- The game layer is ready for more than one client id; the engine layer is not (server→client
  has no id).

### Unknown

- Whether these messages carry world state, or only commands and events (ids seen: player
  actions, pause flags, cheats). Object replication (server objects → `C*Proxy`) may happen by
  direct memory access in the same process, which a remote client would not have.
- Why `players=0`: the address may be a different list (filled only for some players), or no
  player existed during the cinematics and the loading screen.
- **Launch 7 stalled on a loading screen after the cinematics.** The cause is unknown. Candidates:
  - The new probe reads, which go to static data only and are page-checked.
  - Window focus: there was no owner input this time, and the server has pause flags set by
    messages 7 and 8.
  - Something unrelated.

  Launch 5 (no probe) was killed at 480 s, during the cinematic, so it says nothing about this
  phase.

### Next

- Find how client proxies get server object state (in memory vs. messages): trace the writers of
  `CCharacterProxy` fields, or log the message ids passing through `e01c20`/`e01cb0`. Logging the
  ids needs a hook, not just reads, so it needs a new guard feature and the owner's go-ahead.
- Settle the loading stall with one probe-off scripted run past the cinematics.

## Session 2026-11-19: eighth guarded launch (owner playing, probe on)

Same guard and probe as launch 7 (vtable log + player list, `probe_delay_s=60`, getter off). The
owner asked to play through the tutorial by hand; no scripted keys. Planned 1800 s.

Observations:
- Seeded save read at 7.4 s; one cloud write at 68.6 s; probe armed at 69 s, sampled from 129 s.
- Captures: prologue narration at 110 s, then a **black loading screen with spinner from 266 s**
  (99-100 fps, so frames kept running). The owner reported an infinite loading screen and closed
  the game; it exited with code 0 at 528 s.
- Probe: server shell present through the whole stall (same pointer, vt exe+1462960), `players=0`
  in every sample; at 519 s the server shell was null (game shutting down).
- Only one cloud write in the run. Real files unchanged (backup check 0 before and after).
  Sandbox save kept as evidence `launch-8-sandbox-save`, sandbox reseeded with the original.

Derived:
- The stall happens with the owner at the keyboard and the window in focus, so focus/missing
  input (a launch 7 candidate) is ruled out.
- Launch 6 (older probe: sample line only) reached the world and the tutorial with 16 cloud
  writes; launches 7 and 8 (probe adds a one-time vtable dump and a per-sample player-list read)
  both stalled on the first loading screen after the prologue. The new probe reads are now the
  main suspect. **Not verified**: a probe-off run past that point has not been done yet.

Unknown: how a read-only thread could stall loading (no lock is taken by the reads themselves;
logging does file I/O through the hooked ntdll calls).

## Session 2026-11-19: ninth guarded launch (owner playing, probe off)

Observations:
- `probe=0`, otherwise the same guard. Seeded save read at 4.5 s. Captures 17-266 s: black window
  with the spinner (start-up loading, before the menu). The owner closed the game at 277 s
  (exit 0). No cloud write; sandbox save unchanged (original). Real files unchanged.
- Time to the first cloud write (≈ just after the menu) in earlier runs: launch 5 ~346 s (menu at
  268 s), launch 6 ~93 s, launch 7 ~186 s, launch 8 ~69 s. Start-up length varies a lot between runs.

Derived: launch 9 was closed after 277 s, only just past the slowest menu time seen (268 s), so
it does not show a new start-up hang. It also says nothing about the post-prologue stall.
Unknown: why start-up varies from ~1 to ~4.5 minutes (blocked online services timing out is a guess).

## Session 2026-11-19: tenth guarded launch (owner playing, probe off) — probe ruled out

Observations:
- `probe=0`. Seeded save read at 4.6 s. Start-up black screen until ~540 s; first cloud write at
  586 s (hash `14753a37…`, identical to launch 8's single write: the new-game start save).
- Captures: prologue cinematic at 695 s (window active), then a **black loading screen with the
  spinner from ≤789 s until the owner closed the game at 978 s** (exit 0). Window title bar was
  active during the stall, so the game had focus.
- One cloud write only. Real files unchanged (backup check 0). Sandbox save and `settings.cfg`
  kept as evidence `launch-10-sandbox-save`; sandbox save reseeded with the original.

Derived:
- **Correction to launch 8's "derived": the probe is not the cause.** The post-prologue stall
  happens with the probe off. Launches 7, 8 and 10 all stall at the same point; launch 6 did not.
- Start-up time varies from ~1 to ~9.5 minutes between runs with the same guard and files.

Differences between launch 6 (got through) and 7/8/10 (stalled) that remain:
- The sandbox `settings.cfg`: launch 6 started before the owner resized the window, and wrote
  windowed 1213x758 at ~510 s; every later run started with that file. The real (backed-up) file
  is fullscreen 1920x1080. What launch 6 started with is **unknown** (not recorded).
- Nothing else known: same exe, guard network block, save seed and sandbox paths.

Unknown: whether the unmodified game gets past this point on this PC; whether the guard (network
limited to loopback, path redirects) causes it. A web search found no matching known issue.

## Session 2026-11-19: eleventh guarded launch (owner playing, probe off, original display settings)

Observations:
- Sandbox `settings.cfg` reset to the backed-up real one (fullscreen 1920x1080) before the run.
- Seeded save read at 7.4 s. Captures 16-207 s: window present; 238 s: window minimised; 270 s:
  no window handle. Game exited (code 0) at 274 s, closed by the owner during start-up.
- No cloud write; sandbox save unchanged (original). Real files unchanged (backup check 0).

Derived: the run ended before a new game started, so it does not test the post-prologue stall.

## Session 2026-11-19: why loading is slow (disk), intro-skip mod, windowed settings

Observations:
- The owner added an intro-skip mod **to the test copy only** (real install unchanged): 16 videos
  in `game\interface\videos` (intro + 11 language variants, `nvidia_splash`, 3 `legal\*_Patch`)
  replaced by identical 520-byte stubs, SHA-256 `5e19ab18…a1b982`. Originals remain in the real
  install, so the test copy can be restored from it.
- **The test copy (42.5 GB) is on a 7200 rpm HDD; the real install is on a SATA SSD.** The PC's RAM is too
  small for the OS file cache to hold the game.
- No `NET ... refused` line in any guard log: the exe's own `connect/sendto/getaddrinfo/
  gethostbyname` were never called with a non-local target. Sockets were opened at ~0.4 s
  (`\Device\Afd\Endpoint`), source unknown (Steam overlay/`steam_api64` likely, unverified).
- Sandbox `settings.cfg` set to windowed 1600x900 at the owner's request.

Derived (not verified):
- Disk speed explains the start-up spread (79 s right after another run with a warm cache, up to
  ~9.5 min cold) better than a network timeout, since the guard saw no refused network calls.
- The post-prologue "infinite" loading screen loads the open world; on this HDD it may be very
  slow rather than hung. Launch 6 may have succeeded on a warm cache.

Proposed test: run the test copy from the SSD and time start-up and the post-prologue load.

## Session 2026-11-20: root cause of slow/"infinite" loading — the test copy's disk is failing

Observations:
- Copying the test copy from the HDD to the SSD ran at ~1 MB/s average, 0.4 MB/s at the end (3.5 GB read in
  ~50 min); stopped at ~00:03 with 7 GB (partial) on the SSD.
- Windows reports the HDD holding the test copy as `HealthStatus Warning`, `OperationalStatus
  Predictive Failure`; System log: `disk` 52 (device predicted it will fail, 2026-11-19 14:26),
  repeated `disk` 153 (I/O retried) and `storahci` 129 (device reset) on 2026-11-19 from 07:03
  through 23:54. Average read time on that disk during the copy: ~7.8 s.

Derived:
- The slow and varying start-up and the "infinite" post-prologue loading screens in launches
  7-11 are explained by reads from a failing disk (retries and controller resets), not by the
  probe, the guard or the game. Launch 6 likely got lucky (cached or readable sectors).
- Playbook lesson: before blaming hooks for a stall, check disk health and the System event log
  (`disk` 153/52, `storahci` 129) for the drive that holds the test copy.

Next: rebuild the test copy on a healthy disk from the real install (read-only), not from the
failing copy; verify by SHA-256 against the real install.

## Session 2026-11-20: test copy rebuilt on an SSD

- Lab files (notes, guard source/build, reports, evidence, backups), Ghidra scripts and the
  project workspace copied off the failing HDD to the SSD first: 842 files, 0 failed.
- Test copy rebuilt from the real install (read only) to `<lab>\base` on the SSD with robocopy:
  335 files, 42.67 GB, 0 failed, 18 min (~40 MB/s; the copy from the failing HDD had managed
  ~1 MB/s).
- Then installed: guard `bink2w64.dll` (SHA-256 `60bd5f5b…f685e`, same build as before), the
  original renamed to `bink2w64_.dll` (`3c8a2835…30fe`, equals the real install's),
  `steam_appid.txt`, `somguard.ini` (allowed root and sandbox on the SSD, probe off, backups on
  both disks protected), and the owner's 16 intro-skip stubs.
- **Verified by SHA-256 against the real install:** 318 identical, 17 different (the 16 stubs
  and the guard DLL, as intended), 3 extra (`bink2w64_.dll`, `somguard.ini`, `steam_appid.txt`),
  0 missing.
- Sandbox copied over: original save seed `c8ffb967…`, windowed 1600x900 `settings.cfg`.
- Not yet run from the SSD; the next launch tests whether loading times become normal.

## Session 2026-11-20: launch 12, first run from the SSD (owner playing)

Observed (`launch-12.txt`, screenshots every ~30 s):
- Pre-flight: backup check 0 changed. Started 01:50:03; the owner quit from the menu at 462 s
  (exit code 0, before the 1800 s limit).
- Start-up: first cloud write (to the sandbox) at 17.6 s; screenshot at 49 s already shows the
  prologue narration (intro videos skipped by the stubs). Was 1 to 9.5 min from the HDD.
- Gameplay at 111 s (sparring tutorial, ~99 FPS in the 1600x900 window). At ~300 s the
  "Fast Travel UNLOCKED" map screen: **the post-prologue load that hung in launches 7, 8 and 10
  completed.** At 457 s the save list shows one game at 1 %, 0h 6m.
- 37 `CLOUD FileWrite GameData.sav` calls, all to the sandbox; the real remote-storage
  interface was never called. No `NET … refused` lines. No FATAL.
- WATCH-only lines for writes by injected third-party software in the process (NVIDIA driver
  cache and DRS files, Steam overlay logs, a shell-customisation tool's status files). Not game data; not
  blocked by design.
- Backup check after: 0 changed. No auto-restore was needed this run.
- Owner's save kept: `<lab>\evidence\launch-12-sandbox-save\GameData.sav`
  (SHA-256 `2e071313…7cb10fe8`) plus that run's `settings.cfg`. Not reseeded.

Derived:
- Confirms the disk explanation: same game files (verified by SHA-256), same guard, same
  settings, only the disk changed, and both the slow start-up and the hang are gone.

## Session 2026-11-20: how the client side gets world state (phase 3)

Ghidra re-installed on the SSD (same release, same SHA-256) and the exe re-analysed there:
664 s of analysis instead of ~5 h on the failing disk. Check: four functions from earlier
reports (`e01cb0`, `e01c20`, `e062d0`, `e06470`) decompile identically. New read-only script
`SomStrRefs.java` (strings containing given needles, with their references).

### Observations (static)

- In the replication table, only three server classes name a client proxy: `CCharacter`,
  `CPlayerObj`, `NPC`, all with the same proxy hash (`41A597C0`). The other 324 classes have none.
- Class descriptors are `{name, parent class, flags, create fn, destroy fn, pool name fn, ..., size}`.
  `CCharacterProxy` (descriptor RVA `17faaa0`, size `0x1B0`) has parent `CGameBaseProxy`
  (`17fbc50`); constructor `424d70`, vtable `exe+13BDEA0` (21 slots).
- **The character proxy reads the server object directly.** Its per-frame update (`4274d0`, slot 12,
  the one with `DrawClientSkeleton`):
  - Takes the engine object at proxy `+0x08`.
  - Walks that object's class chain at `+0x48`. If it finds `CCharacter` (the *server* class,
    descriptor `185e360`), it takes `engineObject+0x38` and reads fields of that server
    `CCharacter` (`+0x294`, `+0x298`, `+0xA50`).
  - It also reads the transform straight from the engine object (`+0x08`..`+0x20`).
- The other direction exists too. `81f880` takes an object's engine object (`vt+0x40`), walks the
  class chain at `+0x50` for `CCharacterProxy`, then takes the proxy at `engineObject+0x40` and
  reads its transform.
- Client shell message handler `40a620` dispatches server→client ids 1, 6, 7, 9, 10, 0xC, 0xF,
  0x13. Id 0x13 is what server shell slot 3 sends. Several handlers (6, 7, 0xC, 0x13) receive an
  object reference read out of the message (`reader vt+0x28`) and act on it directly. For 0xC
  (`405110`): if it is a `CGameBaseProxy`, call its `+0xA0`.

### Derived

- One engine object record links both sides of an entity. From the two functions above:
  - `+0x38` = server game object, `+0x48` = its class.
  - `+0x40` = client proxy, `+0x50` = its class.
  - `+0x08`.. = transform.

  The client proxy gets character state by **reading the server object's memory in the same
  process**, not from messages. Messages carry events, and they refer to objects by in-process
  reference.
- So the in-memory message queues are **not** a complete world-state stream. A second game
  instance attached as a "remote client" would see no world: no engine objects, no server
  characters to read. Rebuilding that would mean writing a replication layer for the engine.
- The practical co-op shape is therefore **two full game instances, each running its own world,
  with a sync layer between them**. The host is authoritative for chosen state (other player's
  character, enemies near players, Nemesis/quest events); the guest mirrors it. That is how most
  single-player co-op mods work. (Design read, not tested.)

### Unknown

- What exactly is in the message object reference (pointer, handle, or id), i.e. whether
  messages could be forwarded between processes at all.
- Where engine objects are created and the `+0x38`/`+0x40` links filled in.
- How the player character is spawned: needed to put a second, remote-driven Talion into a world.

### Next

- Find the player spawn path (`CPlayerObj` create, the server's add-player) and whether a second
  `CPlayerObj` can exist in one world. That decides whether the guest can be shown as a second
  Talion in the host's world.

## Session 2026-11-20: the player object and the player list (phase 3)

Static, read-only (Ghidra on the SSD). Reports `strrefs-spawn`, `refs-playerlist`,
`decomp-spawn1`..`7`, `fielduse-b68`, `refs-firstplayer`, `strrefs-gamemode` in the lab.

### Observations

- `CPlayerObj` class descriptor `185e240`: parent `CCharacter`, create function `be1800`, which
  only calls the constructor `be13c0` (vtable `exe+1460160`). Object size `0xDD8`.
- **The "player list" (`RVA 1988A90`..`1988AA0`) is a list of every live `CPlayerObj`.**
  - The constructor appends `this` to it. It is a growable vector: capacity at `1988AA0`, grows
    in steps of 32 entries, no fixed maximum.
  - The destructor (`bdfd60`) removes `this` from it and from a second list at `1988AA8`..`1988AB0`.
  - Only these two functions (plus static init `1395b30`) write it.
  - So `players=0` in launch 7 just meant no `CPlayerObj` existed at that time (cinematics and
    loading).
- The client id byte `+0xB68` is set to 0 by the constructor. In the code that calls itself
  `CPlayerObj`, nothing else was seen setting it, so every player has client id 0 in this build.
  Three other writers of a `+0xB68` byte exist (`49b660`, `aaf9c0`, `abc9a0`); their classes are
  not identified.
- The server **does address the player's client**. `be0e80`, a `CPlayerObj` method that places
  the player at a position (respawn/teleport):
  - Builds messages with the engine (`IWBEngine +0x168`).
  - Sends them with `IWBEngine +0x158 (msg, clientId = byte at +0xB68, 1)`.
  - The engine then drops the id (earlier session), so in this build the id is cosmetic.
- **Single-player assumptions are concentrated in one helper.** `c47c10` returns the first entry
  of the player list, or null. It is called from **171 sites in 158 functions** (AI, combat,
  camera, UI, quests...). By contrast:
  - `c47c30` (player by index): 2 call sites.
  - The radius queries `c47c60`, `c47d30`, `c47e00` already loop over **all** players, from 13
    sites.
  - About 20 more functions loop over the list directly (AI info, camera, damage...).
- No code was found that creates a `CPlayerObj` directly. Its create function has no direct
  callers, so it runs through the class descriptor, i.e. the engine's generic "create object of
  class X". Who asks for it (world file, game mode, start point) is not yet known.
- `CGameModeClientHydra::EnterWorld` (`50acd0`) is the client game mode used when entering the
  world. It calls into the player code (`be0e80` via the client game mode's player at `+0x1D8`).
  "Hydra" also appears as `MPHydra`, `MPHydraState`, `EHydraLoadState` and in
  "Playing Atlas/Hydra" (`ea9f00`, in the online-service code).

### Derived

- **A second Talion is not ruled out by the data structures.**
  - The player list, the remove-by-client-id path and the radius queries all handle N players.
  - Nothing found so far caps the number of `CPlayerObj` at one.
- The cost is in the 158 functions that ask for "the first player". Each one is a place where a
  second player would be ignored: enemies that only target, notice or follow player 1, UI and
  camera code for player 1. For a co-op mod this matters only for the guest's Talion as it
  exists in the host's world. The guest plays in their own instance as their own player 1, so
  their camera and UI are fine. What needs work is how the host's world reacts to the guest's
  Talion (AI aggro, damage, quest triggers).
- A cheaper first milestone that avoids `CPlayerObj` entirely: show the guest as a **puppet** in
  the host's world, an NPC-class character (it already has a `CCharacterProxy`) driven by the
  guest's synced position and animation. That gives "see each other and fight side by side"
  without touching the 158 sites. Real damage, aggro and quest credit would come later.
  (Design read, not tested.)
- The client id field and the per-client message API exist but carry no meaning in this build
  (always 0, and the engine ignores it). That confirms the earlier session: the multiplayer
  plumbing was not finished, or was stripped.

### Guess (unverified)

- "Hydra" may be the name of WB's online backend or of a cut multiplayer mode (`MPHydra`). No
  evidence either way yet.

### Unknown

- Where the engine is asked to create the `CPlayerObj` (start point, world load, game mode).
- Whether the engine itself would accept two `CPlayerObj` (input, camera and controller binding
  may be engine-side).

### Next

- Runtime, needs the owner's go-ahead and a new guard feature: log each `CPlayerObj` construction
  with a short return-address stack. That names the spawn path in one launch, with no behaviour
  change.
- Static: the spawn path for NPCs, to see how a puppet character could be created.

## Session 2026-11-20: launch 13, where the player and NPCs are created (owner playing)

Guard change (logging only), SHA-256 `8e11354d…60b994cb` (previous `60bd5f5b…f685e` kept in the
lab). With `spawnlog=1` it swaps the create-function pointer (`descriptor+0x18`) of two class
descriptors, `CPlayerObj` and `NPC`, for a wrapper. The wrapper:
- calls the original with the same arguments and returns its result;
- logs the return-address stack (first 50 players, first 8 NPCs).

Installation is refused unless the descriptor's name and current pointer match this build. New
offline test `spawn_hook`:
- refused on a wrong name or a wrong pointer;
- passes all four arguments and the return value through;
- writes the log line.

Suite 30/30, negative control 14/14. `spawnlog` was set back to 0 after the run.

### Observations (runtime, launch 13)

- Pre-flight backup check 0. The owner loaded their save and played; the game was closed from
  the menu at 553 s, exit 0. Backup check after: 0 changed. Owner's save kept in
  `evidence\launch-13-sandbox-save` (`8ca550b7…`).
- Both hooks installed at 0.4 s.
- **One `CPlayerObj` was created, at 419.9 s** (entering the world): `players=1`, client id 0.
  NPCs followed 4 s later (more than 8; only the first 8 logged).
- Player creation stack (innermost first):
  - `e51c02` / `e54922`: engine generic create-object.
  - `e64f50`
  - **`50abcc` in `50aa00`**
  - `50daf2` in `50d9b0` = `CGameModeClientHydra::Update`
  - `40c279`: client shell.
  - `e06835`: engine frame step, client side.
  - `e06d31`, `e04137`, `e044d5`, `3ef1af`: main loop.
- NPC creation stack: the same engine create-object, then `b02be7` (in `b02bc0`), then AI spawner
  code (`b038a3`, `ae2b58`, `ae7669`/`ef843d`, `add8b6`, `adea5a`, `adebe9`, `ad5c9c`), then
  **`c367e0` = server shell slot 9 (per-frame update)**, then `e067c5` (engine frame step, server
  side).

### Observations (static, from those addresses)

- **`50aa00` is the player spawn function.** It fills a create-params block:
  - name `"Player"`;
  - class = `engineIface2 (*RVA 19D80D0) +0x70 ("CPlayerObj")`, i.e. look a class up by name;
  - position and rotation (7 floats from its first argument);
  - an object set.

  Then it calls **`engineIface2 +0x48 (params)` = create object**, which returns the new player.
  After that it checks for the attached `CCharacterProxy` and binds the player to the client and
  controller (`dff0a0` with the client byte at `player[2]+0x78`, `418ad0`). Its caller is
  `CGameModeClientHydra::Update`.
- The NPC path ends in the same `engineIface2 +0x48`. `b02bc0` checks that the result is an
  `NPC` and can remove it again with `engineIface2 +0x50`.
- The engine's create-object (`e54860`) confirms the object layout derived earlier:
  - It allocates the game object with the descriptor's create function.
  - It links game object `+0x08` to the engine object, and the engine object's `+0x38` to the
    game object and `+0x48` to the class.
  - If the class names a proxy, it creates the proxy in the same call and sets engine object
    `+0x40` to the proxy and `+0x50` to the proxy class.

  Server object and client proxy are created together, in one process.

### Derived

- `engineIface2` (`*RVA 19D80D0`) is the game's object API (LithTech `ILTServer`-like):
  - `+0x70` get class by name;
  - `+0x48` create object from params;
  - `+0x50` remove object;
  - `+0x100` and `+0x98` set object state.
- **The player is spawned by the client game mode (`CGameModeClientHydra`) calling the
  spawner `50aa00` once.** A co-op mod has two clean routes, both through the game's own API with
  the same parameter block:
  1. Call `50aa00` again for a second `CPlayerObj` (it also binds a controller and camera to it;
     the 158 "first player" sites still apply).
  2. Create an `NPC`-class (or other character class) object through `engineIface2 +0x48` as a
     puppet for the remote player, and drive its transform and animation from the network.
     Route 2 avoids the controller binding and the 158 sites. Its creation parameters (which
     character template, which object set) still need reading.
  (Design read, not tested.)
- NPCs are spawned by the server's per-frame update through AI spawners. For a co-op host that
  means enemy spawns happen in one place, which helps if the host must tell the guest what spawned.

### Unknown

- The full layout of the create-params block (`local_1a8` in `50aa00`) and what the NPC spawner
  puts in it (template or archetype).
- What `dff0a0` / `418ad0` bind (input, camera, HUD) and whether that binding accepts a second player.

### Next

- Static: map the create-params block from `50aa00` and the NPC spawner `b038a3`/`ae2b58`.
- Then a first test that changes behaviour: spawn one extra NPC-class puppet next to Talion in
  the test copy. Needs the owner's go-ahead.

## Session 2026-11-20: the object-create parameter block (phase 3)

Static, read-only. Report `decomp-params.txt`. The object API vtable (`*RVA 19D80D0`) was already
logged by the probe in launch 7: `+0x48` → `e64f10`, `+0x50` → `e63ad0`, `+0x70` → `e63b50`
(not a Ghidra function boundary).

### Observations

- `+0x48` create (`e64f10`): copies the caller's block (`e643d0`, field-by-field copy, so this
  is its exact layout), then calls the engine create `e54860`, which refuses a block without an
  object set ("Invalid ObjectSet. CObjectCreateParams need to have a valid m_pObjectSet
  assigned"). Returns the new object's game-side object (`engineObject+0x38`).
- `+0x50` remove (`e63ad0`) → `e53050`.
- **`CObjectCreateParams`, 0x198 bytes**, as filled by three callers: the player spawner
  `50aa00`, the NPC spawner `b03640` and its prop branch.

  | Offset | Type | Player spawn | NPC spawn | Meaning (derived) |
  |---|---|---|---|---|
  | `+0x00` | u16 | `0x20` | `0x20 \| 0x100` | flags |
  | `+0x04` | 3 × f32 | position | position | position |
  | `+0x10` | f32 | `DAT_1414a7314` | same | scale (1.0, guess) |
  | `+0x14` | 4 × f32 | rotation | rotation | rotation quaternion |
  | `+0x28` | ptr | → `{0, character data, 0}` | → `{character record, p9, p7}` | spawn data / template |
  | `+0x34` | u16 | `0xFFFF` | `0xFFFF` | |
  | `+0x40` | ptr | class `"CPlayerObj"` | class `"NPC"` (`13f26d4`) | class, from `+0x70` get-class-by-name |
  | `+0x48` | 0x100 bytes | zero | zero | (properties?) |
  | `+0x150` | char[32] | `"Player"` | caller's name | object name |
  | `+0x180` | u64 | 0 | 0 | copied to engine object `+0xD0` |
  | `+0x188` | u16 | `0xFFFF` | `0xFFFF` | |
  | `+0x190` | ptr | game server shell `+0x9A8` | object API `+0x118 (anchor)` | object set (required) |

- **NPC spawn helper `b03640(anchor, name, transform[7], record, flagA, flagB, p7, p8, p9)`**:
  1. Takes the object set from the anchor object and makes the name unique (`cd7df0`).
  2. If `record` is a character record (`8b7b20`), it fills the block with class `NPC` and creates
     the object through `b02bc0` (create, check it is an NPC, remove it again if `be30f0` refuses).
  3. Otherwise it creates a `CombatProp`.
- The AI spawner's spawn (`ae28e0`):
  1. Picks a character record from a pool (`ad97b0(DAT_141984e60)`).
  2. Uses the spawner object's own transform and name + `"AI"`.
  3. Calls `b03640`.
  4. Then configures the new NPC through the spawner's `vt+0xF8` (loadout, group, links to
     player 1 through `c47c10`).

### Derived

- Spawning any character is one call with a filled block. A remote-player puppet needs only:
  - an anchor (for the object set): the local player works;
  - a name;
  - a transform;
  - a **character record**.

  `b03640(localPlayer, "CoopPuppet", transform, record, 0, 0, 0, 0, 0)` matches how the game
  itself spawns NPCs. (Design read, not tested.)
- The open piece is the record: which character records exist (orc types, humans, a Talion-like
  model) and how to get a pointer to one (`ad97b0` picks from a pool; the player spawner derives
  its data from its second argument through `474dc0`/`405600`).
- An NPC puppet comes with AI. For a puppet the AI must be kept from acting (or a non-AI character
  class used), and its transform driven from the network. How to disable an NPC's brain is not
  yet known.

### Next

- Static: the character record type (`8b7b20` check, `ad97b0` pool) and how to find a record by
  name.
- Then the first behaviour-changing test: spawn one puppet next to Talion in the test copy, and
  log that it exists and where it is. Needs the owner's go-ahead.

## Session 2026-11-20: character records and stand-in candidates (phase 3)

Static, read-only. Reports `decomp-records1`..`4`, `strrefs-records`, `strrefs-aioff`,
`refs-charprop` in the lab.

### Observations

- Game data records are property tables:
  - record `+0x08` → sorted field table (16-byte entries, looked up by a 32-bit field id with a
    binary search, `3f3800`);
  - record `+0x10` → values;
  - field type 10 = link to another record.
- The record the NPC spawner receives is an **`AI.Template`** record. Its field descriptors
  (`184bc10`..) include:
  - **`CharacterModel`** (link): `8b7b20` reads it, and a non-null value means "spawn an `NPC`";
  - **`CombatProp`** (link): `ae5c80` reads it, the prop branch;
  - **`Alignment`** (link);
  - siblings `AI.TemplateFilter`, `AI.TemplateSwap`, and `Swaps`.

  The string `GameDatabase` (`9e75a0`) and `FindRecordLinkPath` point at a game database; the
  records themselves live in the game's data archives (not read: game files).
- The AI spawner gets its template through `ad97b0`: a per-spawner swap table (`+0x60`, 16-byte
  pairs `{template, replacement}`) that may substitute the template. The base template comes from
  the spawner's own data. No "find record by name" call was found yet.
- The player spawner's second argument is a **bundle name**: `474dc0` matches it against the
  loaded `*.bndl` names and falls back to `"Default"`. The player's data comes from that bundle
  through the client shell (`+0x81018`, `vt+0xA8`), not from an `AI.Template`.
- **No AI on/off switch was found by name.** Searched for `DisableAI`, `EnableAI`, `SetAI`,
  `Brain`, `Puppet`, `Possess` and others. The only hits are unrelated (detection brushes,
  spectator invisibility, AI scaling).
- Character-looking classes that are **not** `CCharacter` (so no AI, and no `CCharacterProxy`):

  | Class | Descriptor | Parent | Create fn | Size | Notes |
  |---|---|---|---|---|---|
  | `CharacterProp` | `1861130` | `Prop` | `cb1170` | `0x390` | node tracking (look-at) script calls |
  | `CinematicModel` | `1863c60` | cinematic visible base | `d0a6b0` | | driven by cinematics |
  | `WorldUICharacterPreview` | `1860380` | game base | `c7c2b0` | `0x500` | menu character preview |

  `CCharacter` itself is abstract (no create function).

### Derived

- Two puppet routes, each with a known cost:
  1. **`NPC` from an `AI.Template`.** Looks and animates like a real character, and the world
     (combat, targeting) knows it, but it brings AI that must be stopped. Getting a template is
     easiest at runtime: reuse one the game itself just spawned with.
  2. **`CharacterProp` / `CinematicModel`.** No AI by construction, but probably no combat
     presence (enemies ignore it, nothing hits it), and whether it can play locomotion
     animations is unknown.
- For a first visible milestone ("see your friend move around"), route 2 is the safer
  experiment. For "fight side by side", route 1 is needed eventually.
  (Design read, not tested.)

### Unknown

- A by-name record lookup (to pick, say, a human or Ranger template without waiting for a spawn).
- How to stop an `NPC`'s AI, or whether parking it in a non-combat state is enough.
- Whether `CharacterProp` animates.

### Next

- First behaviour-changing test (test copy only; needs the owner's go-ahead): once the player
  exists, spawn **one** extra character next to Talion and log it. The spawn uses the game's own
  helper `b03640` with the `AI.Template` of the first orc the game spawns. Watch whether it
  appears and what it does. Remove it after a minute through the object API's remove (`+0x50`).

## Session 2026-11-20: launch 14, first spawn test (behaviour change, owner-approved)

Guard SHA-256 `fcc39be2…3fe9f5719`. With `spawntest=1`:
1. The CALL rel32 at `exe+AE2B53` (AI spawner → NPC spawn helper `b03640`; bytes `e8 e8 0a 02 00`
   checked in the file and again at runtime) is redirected through a trampoline near the exe.
2. The wrapper calls the original unchanged.
3. Once, after the first successful game spawn, it calls the same helper again: same template,
   anchor and flags, name `"CoopPuppet"`, at the local player's transform + 2 on x and z.

New offline test `call_hook` (refuses a wrong target or a non-CALL; installs a
`mov rax; jmp rax` trampoline into the guard). Suite 31/31, negative control 14/14. Before the
run, the guard, the ini and the sandbox save (`8ca550b7…`) were kept in
`evidence\before-spawntest`. `spawntest` and `spawnlog` were set back to 0 afterwards.

### Observations

- Pre-flight backup check 0. Player created at 36.2 s (same stack as launch 13). The first game
  NPC spawned at 39.5 s: object name `NBS_09.Name.TargetAI`, flags 1/1, at a forge tower in
  Udûn Foothills.
- **The puppet spawn returned an object.** NPC #2, a game object with vtable `exe+14498B0`
  (the NPC vtable, same as the others). The guard read its engine object back at
  (-3499.00, 2709.53, 4003.00), next to the player at (-3501.08, 2713.21, 4001.15). The height
  differs by 3.68 from the requested value, so the engine moved it, probably onto the ground.
  The creation stack goes through the trampoline (module addresses `00007FFE5E68…` between
  `b038a3` and `ae2b58`). No exception was raised.
- The game kept spawning its own NPCs normally afterwards (#3..#8 within 0.8 s), wrote three cloud
  saves (43.6, 58.7, 61.8 s), and **exited with code 0 at ~66 s**. There was no guard FATAL and no
  Windows application error event for the process. Whether the owner quit, and why, is not yet
  known (asked).
- Screenshot at 47 s: Talion in Wraith view at the tower, with **an orc standing directly behind
  him**, overlapping. Probably the puppet (the offset was 2 units and it was placed at the
  player). Not proven: the screenshot cannot show which orc it is.
- Backup check after: 0 changed. Post-test sandbox save `06afc9f5…` kept in
  `evidence\launch-14-spawntest`. It may now contain the puppet.

### Derived

- **The game's own spawn helper creates an extra, fully typed NPC on request**, from outside
  the spawner's normal flow, without the spawner's post-spawn configuration (`vt+0xF8`), and the
  game keeps running. This is the first behaviour change in this project that worked.
- 2 units of offset is small enough that the puppet overlaps Talion. The world unit is small
  (centimetres would fit), but this is not measured.

### Owner report (after the run)

- **The orc appeared, and it did not attack.** The owner quit the game themselves at ~66 s, so the
  exit was not a crash.

### Derived (from the owner report)

- An NPC spawned this way, without the spawner's post-spawn configuration (`vt+0xF8`: group,
  loadout, links to player 1), stays passive at least for ~25 s next to Talion. That is close to
  what a puppet needs: present in the world, not acting on its own. Whether it would react to
  being hit, or after longer, is unknown.

### Unknown

- Whether the puppet is now stored in the save. The pre-test save is kept in case it is needed.

### Next

- Next experiment: move the puppet each frame to a scripted path (a circle around Talion). That
  shows whether an NPC can be driven from outside, which is what a co-op puppet needs.

## Session 2026-11-20: launch 15, moving the puppet every frame (behaviour change, owner-approved)

Guard SHA-256 `ec75f551…cd00c5cae`. `spawntest=1` as in launch 14, plus `movetest=1`:
1. Server shell vtable slot 9 (`exe+14629A8`, holds `c36720`, checked before patching) now points
   to a wrapper. The wrapper runs the original per-frame server update unchanged.
2. Then, for 80 s after the puppet spawned, it teleports the puppet on a circle around the local
   player with the object API's set-position-and-rotation (`*RVA 19D80D0`, `vt+0xB0` = `e646f0`,
   identified statically together with:
   - `+0x98` = set position;
   - `+0x90` = set rotation;
   - `+0xA8` = set pos/rot/scale;
   - `+0xD0` / `+0xD8` = move/rotate through physics).
3. The circle has radius 3 units for 40 s, then 100, and makes one turn every 8 s.
4. Every frame it first checks the puppet is still the same live NPC (vtable, engine object
   back-link).

New offline test `slot_hook`. Suite 32/32, negative control 14/14. Backups in
`evidence\before-movetest`. All test switches set back to 0 afterwards.

### Observations

- Pre-flight backup check 0. Both hooks installed at 78 ms. The puppet spawned at 32.2 s next to
  the player (same forge tower).
- **The move ran every frame for 76 s: ~7,500 frames at ~99 per second, every call returned 0, no
  exception.** Read back right after each call, the puppet's engine position equals the
  requested position exactly.
- Between frames, something else rewrites the puppet's **height**. At each logged frame, the
  position read before our call has the x/z we set one frame earlier but y = **2716.58**
  (requested: the player's y, 2712.13), every time from t=1 to t=74. At t=75–76 it reads back
  2712.13 instead. So the game's own update (probably the character controller or ground
  snapping) owns y and leaves x/z alone, at least for teleports this small.
- The player moved a lot during the test: about 100–360 units per second, with jumps across the
  tower area. The circle followed.
- The game exited with code 0 at ~113 s; the log ends at t=76 (before the 80 s end). Probably the
  owner quitting, as in launch 14 (not yet confirmed).
- Screenshots at 49 s (circle phase r=3) and 80 s (phase r=100) both show an orc **right next to
  Talion**.
  - At 49 s it is raising a weapon at arm's length.
  - At 80 s it walks directly behind him, wearing chains.

  In the r=100 phase the logged puppet position was 100 units from the player, so the orc
  behind Talion at 80 s is probably **not** the puppet (another NPC, or the drawn model does not
  follow the engine position). Not settled from the screenshots alone.
- Backup check after: 0 changed. Post-test save `1dd7a822…` kept in `evidence\launch-15-movetest`
  with the log and screenshots.

### Derived

- **An NPC's position can be driven from outside the AI, every frame, through the game's own object
  API, without errors.** That is the core mechanism a co-op puppet needs. Height is taken over by
  the game (ground or controller), which is what a puppet wants anyway.
- The world unit is not settled. The player moves ~100–360 units/s; at a running speed of a few
  metres per second that suggests 1 unit ≈ a few centimetres, but this is a guess until the owner
  says what they saw at r=3 and r=100.

### Owner report (after the run)

- **There was only one orc**, so the orc in both screenshots is the puppet. The guess above, that
  the 80 s orc was a different NPC, was **wrong**.
- At spawn it stood exactly where Talion was. Later "the circle got bigger and the orc got a
  little bit of space": the switch from r=3 to r=100.
- It **slid**, "like he was attached to me". Correction from the owner: **the orc did play its
  animations** (its AI's own: moving, attacking), but it could not go anywhere, because the guard
  put it back next to Talion every frame.
- **The orc attacked the owner** this time (in launch 14 it did not).
- The owner quit the game themselves (not a crash).

### Derived (from the owner report)

- **The drawn model follows the engine position we set.** Teleporting the engine object each
  frame moves the visible orc.
- **World unit ≈ 1 cm.** r=3 overlapped Talion and r=100 looked like "a little bit of space"
  (about a metre in the 80 s screenshot). The player's logged speeds of ~100–360 units/s then
  mean 1–3.6 m/s, a plausible walk or run. (Estimate, not measured.)
- **The puppet's AI is live.** It attacked, so an NPC spawned without the spawner's
  configuration is not inert; launch 14 just did not last long enough. A co-op puppet needs its
  AI or hostility turned off.
- The puppet's animations are still the AI's own choices. Our per-frame teleport only overrides
  where it ends up, so the AI's walk/attack animations play while it slides with Talion. For a
  co-op puppet the animation must come from the remote player's state instead. That needs the AI
  out of the way first.

### Next

- Find how to stop the puppet acting on its own: alignment or faction (the template's
  `Alignment` link), the AI update, or a non-hostile state.
- Then drive the puppet's animation from outside (walk/run matching its speed, later attacks)
  instead of the AI's choices.

## Session 2026-11-21: making the puppet non-hostile, statically (phase 3)

Static, read-only. Reports `strrefs-aicmds`, `strrefs-aioff`, `decomp-brand1`..`4` in the lab.

### Observations

- No switch that turns an NPC's AI off was found. The exe holds about 200 `AI.*` record-type
  names (actions, goals, perception, morale, movement), but nothing like "disable AI".
- The game's own **brand** mechanic (a branded orc becomes Talion's ally: `BrandedAlignment`,
  `BrandedDefendPlayer`, `AlignmentOverrideBranded`) is reachable three ways:
  - **Debug command `AI.AddBrand`** ("Gives an NPC/AI a brand.", `AI.cpp`), handler `aac260`:
    1. Finds the object by name (object API `+0x108`).
    2. Checks it is an `NPC`.
    3. Calls `npc->vtable[0x170](npc)`, then `bf2820(npc, 0, 1)`.
  - **Script binding `CCharacter.AddBrand`** (`7cbe40`): calls the same `vtable[0x170]`.
  - **The AI spawner itself** (`ae28e0`) calls `abfb10` when its spawn data asks for a branded NPC.
- NPC vtable slot `0x170` (`exe+1449A20`) holds `ac3b90`, a `jmp abfb10`. `abfb10`:
  1. Clears a flag (`+0xDC3`).
  2. Picks an alignment record (`8b8330`/`8b8350` from the character's data) and applies it
     (`ab6990`).
  3. Resets combat state (`abeed0`, `abcb80`) and notifies the player side.

  No story-progress check was seen. `bf2820` refreshes the NPC (`bf0340`) and posts an event.

### Derived

- Branding the puppet right after it spawns, the same way the debug command does, should switch
  it to Talion's side, so it stops attacking him. It is still AI-driven (branded orcs follow
  and defend Talion), so this is "friendly", not "inert". For a co-op puppet that is a usable
  intermediate state; full AI removal stays open.

### Built (not yet run)

- Guard option `brandtest=1` (with `spawntest=1`). Right after the puppet spawns, it checks
  that the puppet's vtable is the NPC vtable and that slot `0x170` holds `ac3b90`, then calls
  `vtable[0x170](npc)` and `bf2820(npc, 0, 1)` under SEH, and logs the result.
- Guard SHA-256 `84277553…8373e7c`. Suite 32/32, negative control 14/14.
- Installed with `spawntest=1`, `brandtest=1`, `movetest=0`, so the branded orc moves freely
  and its behaviour is visible. Backups in `evidence\before-brandtest` (save `1dd7a822…`).

## Session 2026-11-21: launch 16, branded puppet (behaviour change, owner-approved)

Guard `84277553…8373e7c` with `spawntest=1`, `brandtest=1`, `movetest=0`.

### Observations

- Pre-flight backup check 0. The game spawned its first NPC at 33.4 s (same tower, same
  template name).
- **`BRANDTEST puppet … branded through vt+0x170 and bf2820: returned`**: no exception. Puppet
  engine object at (-3499.00, 2704.02, 4003.00), 9.2 below the requested height.
- Screenshot at 48 s: the orc stands beside Talion with a **glowing blue hand mark on its face**,
  the game's brand mark. A HUD counter with a blue marker and "1" appears at the left and is
  still there at 80 s. That is probably the count of branded followers (guess).
- The game exited with code 0 at ~102 s. Backup check after: 0 changed. Post-test save
  `725d41e8…` kept in `evidence\launch-16-brandtest` with the log and screenshots.
  `spawntest`/`brandtest` set back to 0.

### Derived

- **The game's own brand path works on the puppet.** It shows the brand mark and is counted
  by the HUD, so the alignment change took effect. Whether it still attacks Talion: owner
  report pending.

### Owner report (launch 16)

- **The branded orc did not attack**, did not follow Talion, and did not fight other orcs.
- The owner closed the game themselves.

### Derived (from the owner report)

- Branding the puppet right after spawn makes it **non-hostile and passive**. That is exactly
  the state a co-op puppet needs: present, visible, not acting on its own. It probably does not
  follow because it never received the spawner's group/loadout setup, which branded followers
  would normally get (guess).
- Combined with launch 15 (position driven every frame), the pieces for a visible remote-player
  puppet are now: spawn (`b03640`), brand (`vt+0x170`, `bf2820`), place (`vt+0xB0`). Animation is
  the remaining gap.

### Next

- Brand plus move together, to confirm the branded puppet stays passive while it is placed every
  frame.
- Animation: instead of teleporting, ask the NPC's own movement to go to a target point each
  update (the game would then play walk/run). That needs the NPC "go to" path
  (`AI.Actions`/`AI.Movement`), not yet traced.

## Session 2026-11-21: how an NPC is told to walk somewhere (phase 3)

Static, read-only. Reports `strrefs-walk`, `refs-npc-script`, `decomp-walk1`..`3` in the lab.

### Observations

- The game exposes **~110 NPC script commands**: functions that register as class `"NPC"` with a
  method name through `73a800`; full list in `refs-npc-script.txt`. Among them:
  - movement: `GotoNode`, `ClearGoto`, `SetFollowLeader`, `ClearFollow`, `Wander`, `Patrol`,
    `Guard`, `SetAmbientMovementSpeed`, `SetConfinement`;
  - animation: `DEBUG_Animate`, `AnimateAttackType`, `AnimateSmartObject`, `NodeAnimate`,
    `AnimateClear`;
  - state: `SetSenses`, `SetAlignment`, `ResetAlignment`, `SetSolid`, `SetMorale`, `ActivateBrand`,
    `DeactivateBrand`.

  `MoveTo` exists only for cameras and keyframers, not NPCs.
- The native functions behind them (each script wrapper only checks the object is an `NPC` and
  forwards):

  | Script command | Native call |
  |---|---|
  | `GotoNode(node, movementName, flag)` | `abc040(npc, nodeObject, name, flag)`: looks up the movement name, takes the node's AI node via `node->vt[0xF0]`, then `ab8610(npc, aiNode, flag, movementType)` |
  | `ClearGoto()` | `ab8580(npc)` |
  | `SetFollowLeader(leader, bandType)` | `ab8400(npc, leaderCharacter, bandTypeRecord)`: builds a follow behaviour through the AI manager (`*(g_pGameServerShell+0x168)+0x48`) |
  | `SetSenses(on)` | `aaa650(npc, on)`: one byte, `*(npc+0xC78)` → flag |
  | `DEBUG_Animate(name, flag)` | `abbe60(npc, animationName, flag)` |
  | `AnimateAttackType(type, flag)` | `abbe00(npc, attackTypeRecord, flag)` |

- `GotoNode` targets an **AI node object**, not a free position. AI node classes exist in the class
  table (`AINodeGoto`, `AINodeWander`, `AINodeAnimate`, … and `PlayerNodeGoto`).

### Derived

- A walking puppet could work like this (design read, not tested):
  1. Spawn the puppet, brand it, and turn its senses off (`aaa650(npc, 0)`) so it ignores the
     world.
  2. Create one `AINodeGoto` object through the object API (same create call as NPCs).
  3. A few times a second, move that node to the remote player's position (`vt+0x98`) and issue
     `GotoNode` again (`abc040`).

  The NPC's own movement would then walk or run there with the game's animations. Unknown:
  whether a node created at runtime is accepted by the AI node system (nodes are normally
  placed in the level), and how quickly a repeated goto reacts.
- `SetFollowLeader` is a fallback if runtime nodes fail: the puppet follows a leader character,
  for instance a hidden second puppet that is teleported.
- `DEBUG_Animate` and `AnimateAttackType` are the likely way to play the remote player's
  attacks on the puppet later.

### Next

- Test (needs the owner's go-ahead), in this order:
  1. Spawn, brand, senses off.
  2. Create one `AINodeGoto` 5 m from Talion and `GotoNode` the puppet to it.
  3. Then every 2 s, move the node to Talion's current position and re-issue `GotoNode`.

  Watch whether it walks with animation, and log the puppet and node positions.

### Built (not yet run): walk test

- Guard option `walktest=1` (with `spawntest=1`, `brandtest=1`). It uses the same server-update
  slot hook as the move test, but does not teleport the puppet. On the first update after the
  puppet exists:
  1. Senses off: `aaa650(npc, 0)`, the native behind script `NPC.SetSenses`.
  2. Create one `AINodeGoto` 500 units (≈5 m) from the player, with the object API: get class by
     name `+0x70`, object set `+0x118(anchor)`, create `+0x48`. The block is filled like the
     NPC spawner's: flags `0x120`, scale 1.0 (the constant at `RVA 14A7314`, checked = 1.0),
     identity rotation, name `"CoopNode"`, zeroed spawn data.
  3. Log the node and its AI node (`node->vt[0xF0]`).
  4. Send the puppet with `abc040(npc, node, "", 0)`, the native behind script `NPC.GotoNode`.
- Then every 2 s for 120 s: move the node to 150 units beside the player (`+0x98` set position),
  issue `GotoNode` again, and log player, node and puppet positions and the puppet-to-node
  distance.
- Before each tick it checks that the puppet and the node are still the same live objects. Every
  game call runs under SEH and stops the test on an exception.
- Guard SHA-256 `5b29070e…6b3477a2`. Suite 32/32, negative control 14/14. Installed with
  `spawntest=1`, `brandtest=1`, `walktest=1`, `movetest=0`. Backups in `evidence\before-walktest`
  (save `725d41e8…`).

## Session 2026-11-21: launch 17, walk test (behaviour change, owner-approved)

Guard `5b29070e…6b3477a2` with `spawntest=1`, `brandtest=1`, `walktest=1`.

### Observations

- Pre-flight backup check 0. Puppet spawned and branded at 31.3 s (calls returned), same tower.
- **The runtime node worked as an object.** `AINodeGoto` was created: game object vtable
  `exe+144E620`, placed at (-3001.00, 2626.55, 4001.00), i.e. moved down 87 units from the
  requested height. Its AI node getter returned a non-null AI node.
- 29 `GotoNode` calls over 56 s, every 2 s, all returned with no exception. The node followed the
  player (150 units beside them) each time.
- **The puppet did not walk to the node.** It stayed at (-3499.00, 2703.03, 4003.00) from 0 to 42 s
  while the node moved 140–550 units away. Between 42 s and 46 s it moved about 115 units:
  (-3445.67, 3996.32), then (-3384.23, 3953.03). That direction does not match the node at those
  times. Then it stood still until the end.
- Screenshot at 80 s: the branded orc stands idle on the platform (brand marks visible).
- Game exit code 0 at ~93 s. Backup check after: 0 changed. Post-test save `77d27ca5…` kept in
  `evidence\launch-17-walktest` with the log and screenshots. All test switches set back to 0.

### Derived

- Creating the node and calling the `GotoNode` native is accepted by the game (no errors), but
  **the goto does not drive the puppet**. Possible reasons (unverified):
  - senses off or the brand state may stop the goal from running;
  - the node may not be registered in the AI node manager (nodes are normally set up at level
    load);
  - the flag argument (`0`) or the empty movement name may mean "do nothing";
  - the goal may need the NPC to be in a state the spawner normally sets up.
- The one short move at 42–46 s is unexplained. It may be the NPC's own idle or ambient
  behaviour.

### Owner report (launch 17)

- "The orc did not move." The 115-unit shift logged at 42–46 s was not noticeable to the owner
  (about a metre; possibly a nudge, not a walk).
- The owner asked why the tests use an orc when the goal is co-op. Answer recorded in
  "Why the stand-in is an orc" below.

### Why the stand-in is an orc (owner question)

- The engine cannot host a remote player (session "how the client side gets world state"), so
  each player runs their own game, and the **other** player must be shown in each world as a
  character the mod controls: a puppet.
- The orc is only the first character template the guard could get hold of at runtime (the
  game's own first spawn). The mechanisms being tested do not depend on the model: spawn, make
  passive, place, animate.
- Once they work, the puppet's look must change to a Talion-like character. Options:
  - a human or ranger `AI.Template` record;
  - a second `CPlayerObj` (has Talion's model, but brings the controller and camera binding);
  - the `CharacterModel` link of the template.

  None is solved yet.

### Next

- Then try `SetFollowLeader` (`ab8400`) instead: following a character is how branded orcs
  normally move with Talion, so it is likelier to work on a branded NPC.

## Session 2026-11-21: why the goto did nothing, and a second walk attempt (phase 3)

Static, read-only. Report `decomp-walk4.txt`.

### Observations

- `ab8610(npc, aiNode, flag, movementType)`, the core of `GotoNode`:
  1. Returns at once if bit 0 of `npc+0x178` is set.
  2. Clears the current order (`b0a1b0`).
  3. Checks whether the node is claimed by another NPC (`aiNode->vt[0xA8]`, `b34ec0`, `b09f80`).
  4. Takes a **goto task** from the NPC's task manager (`b1e9f0(*(npc+0xC70))`); if there is none,
     nothing happens.
  5. Fills that task with the node, the flag (`+0x48`) and the movement type (`+0x7C`).
  6. Sets byte `+0x401` of the structure at `npc+0xC78`, i.e. "re-evaluate".
- `ClearGoto` (`ab8580`) clears the order and sets the same `+0x401` byte.
- **`SetSenses` (`aaa650`) writes byte 0 of that same structure at `npc+0xC78`.** So "senses" is
  probably the NPC's brain or decision object as a whole, not perception only.
- `SetFollowLeader` → `b266d0(aiManagerBands, leader, bandTypeRecord)` finds or creates a band from
  a band-type record. With no valid record (we have none) it would pass null into the band
  constructor (`b265a0`, `b236e0`). Not usable without a record.

### Derived

- **Launch 17 most likely failed because senses were off**: the goto task was set, but the NPC's
  decision object, the one that re-evaluates and runs tasks, was switched off. Launch 16 showed a
  branded puppet is passive even with senses on, so senses are not needed for passivity.
  (Derived from the code, not tested.)

### Built (not yet run): walk test, second attempt

- New guard option `walk_senses_off` (default 0). Everything else as in launch 17.
- Guard SHA-256 `2fcc7221…52cfe965`. Suite 32/32, negative control 14/14.
- Installed with `spawntest=1`, `brandtest=1`, `walktest=1`, `walk_senses_off=0`. Backups in
  `evidence\before-walktest2` (save `77d27ca5…`).

## Session 2026-11-21: launch 18, walk test with senses on (behaviour change, owner-approved)

Guard `2fcc7221…52cfe965` with `spawntest=1`, `brandtest=1`, `walktest=1`, `walk_senses_off=0`.

### Observations

- Pre-flight backup check 0. The first game NPC spawned at 87.6 s (later than before). Puppet
  spawned and branded (calls returned); node created (`exe+144E620`, AI node non-null), same as
  launch 17.
- 35 `GotoNode` calls over 68 s, all returned.
- **Puppet still from 0 to 50 s**: (-3499.00, 2705.03, 4003.00), while the node was 148–540 units
  away.
- **52 s**: puppet height 2723.61 (≈19 units up), position nearly the same. **54 s**: puppet 500
  units away at (-3710.64, 4498.66). The screenshot at 141 s process time (≈ t=53) shows the
  orc **on its knees** at the platform edge, with the brand marker over it and the game's prompt
  "Hold [L SHIFT] + tap … to kill downed enemies". So the puppet had been knocked down at that
  point (cause not known; owner report pending).
- **From 62 s to the end (68 s) the puppet moved steadily toward the node**: (-3730, 4473) →
  (-3700, 4440) → (-3618, 4376) → (-3533, 4318). That is ~100 units per 2 s, along the direction
  to the node at (-3269, 3986).
- Screenshot at 110 s: the branded orc standing near Talion. The game exited with code 0 at
  ~160 s. Backup check after: 0 changed. Post-test save `e215169f…` kept in
  `evidence\launch-18-walktest2`. All test switches set back to 0.

### Derived

- **First sign the goto drives the puppet**: after the knock-down, it walked toward the node at
  walking speed (~0.5 m/s, consistent with ~1 cm units). It took over a minute to start, and
  only after an event (the knock-down) changed its state. Possibly the goto task only runs once
  the NPC re-evaluates for another reason, or the first event woke its decision-making. Not
  settled.
- The "downed enemies" prompt suggests the game still treats the branded puppet as knockable or
  killable (it may be the generic prompt for any downed orc).

### Owner report (launch 18) — corrects the derivation above

- **The owner knocked the orc down.**
- **It walked when the owner pressed G to attract it**: the game's own command for calling a
  branded orc. So the walking at 62–68 s came from the game's attract order, **not** from our
  `GotoNode`. The goto still has no visible effect, with senses on or off.

### Derived (corrected)

- `GotoNode` with a runtime `AINodeGoto` does not drive the puppet (2 tests).
- **The game's own "attract a branded orc" order does make the puppet walk** with its normal
  movement, toward the player. That order is the next lead: if it can be issued with a chosen
  position (or toward a moving object), it gives a walking, animated puppet.

### Next

- Trace what the G "attract" input does to a branded orc. Leads: `AI.Goals.ExtraData.Attract`,
  `CAIGoal…Attract`, `DismissBaitAttract`, `BrandedTarget`. Find its target argument and whether
  it accepts a position or an object other than Talion.

## Session 2026-11-21: what "attract" (G) might be, and a stimulus log (phase 3)

Static, read-only. Reports `strrefs-attract`, `strrefs-whistle`, `decomp-stimulus`,
`refs-stimulus`.

### Observations

- Attract-related names in the exe: `CAIGoalAttract`, `CAIGoalBaitAttract`,
  `AI.Goals.ExtraData.Attract`, `AINodeAttract`, `AIStationAttract`, `Beckon`/`CAIActionBeckon`,
  `CAIInterpreterAttractTactile`, `FriendLure`, `CreateDisturbance`, `Disturbance*`.
- AI reactions to things happening at a place go through **`CAIStimulusMgr::RegisterStimulus`
  (`b0cf30`)**, called directly from ~25 sites: footprints, dead bodies, combat hits,
  projectiles, `DisturbanceSound`/`DisturbanceVisible` (`d0b010`) and others.
  - Its argument is a descriptor: `[0]` stimulus type record, `[4]` source object, `[0xB]` target
    object, and a position block around `+0x40` (layout not confirmed).
  - It refuses stimuli whose source or target is a `CPlayerObj` while a global flag
    (`1988a80`) is set.
  - For NPC-targeted stimuli it builds a task through the NPC's task manager (`npc+0xC70`), the
    same manager `GotoNode` uses.

### Guess (unverified)

- G might be a lure, whistle or beckon that registers a stimulus at Talion's position, which the
  branded orc then walked to. If so, a stimulus at the remote player's position could move the
  puppet. It would also draw other orcs, unless the stimulus is aimed at the puppet as its target.

### Built (not yet run): stimulus log

- Guard option `stimlog=1`. It is an **entry hook** on `b0cf30`:
  - The first 17 bytes are a plain prologue (`push rbp; push rbx; push r14; lea rbp,[rsp-70h];
    sub rsp,170h`, no RIP-relative operands; checked byte-for-byte before patching). They are
    copied to a trampoline that jumps back to `+17`.
  - The entry becomes `mov rax, wrapper; jmp rax`.
  - The wrapper calls the original unchanged, then logs the caller, the result, the type record,
    source, target and the descriptor fields.
  - At most 6 logs per distinct caller, plus every 1000th call. **Logging only.**
- New offline test `entry_hook`: a fake function with the same prologue runs and returns
  correctly before and after hooking, the hook refuses different bytes, and the log line is
  written. Suite 33/33, negative control 14/14.
- Guard SHA-256 `d95e4f9f…09f54631`. Installed with `spawntest=1`, `brandtest=1` (a branded orc to
  attract) and `stimlog=1`; `walktest`/`movetest` off. Backups in `evidence\before-stimlog`
  (save `e215169f…`).

## Session 2026-11-21: launch 19, stimulus log (logging only, owner-approved)

Guard `d95e4f9f…09f54631` with `stimlog=1`, `spawntest=1`, `brandtest=1`.

### Observations

- Pre-flight backup check 0. The entry hook installed at 78 ms. The game ran normally: player at
  ~32 s, puppet spawned and branded at 35.6 s, about 2,200 stimuli registered in 92 s. Exit
  code 0. Backup check after: 0 changed. Post-test save `0619f184…` kept in
  `evidence\launch-19-stimlog`. All test switches set back to 0.
- **Descriptor layout, read from the log** (my logged "f@+40" field was the wrong offset):
  - `+0x00` type record;
  - `+0x20` source engine object;
  - **`+0x28` position x, y, z** (floats; e.g. -3501.08, 2713.2, 4001.15 = the player's spawn
    point);
  - `+0x58` target engine object;
  - `+0x60` = 1.0.
- Callers seen (each the return address in the exe):

  | Caller | Calls | What (from the code/strings) |
  |---|---|---|
  | `bea53a` | 6 at spawn | `bea3b0`, at NPC spawn |
  | `bea6ba` | ~800+, all rejected (-1), type null | `bea5d0` "FootprintVisible": footprints |
  | `abe2f9` | 5 | `abe130`: **the NPC's on-hit handler** registers a stimulus about the attacker |
  | `b320b3` | 560+ | combat (`b31e90`) |
  | `c10f33` | several | combat (`c10d00`) |
  | `bf1b02` / `bf1c88` | 3 each | `bf1600` |
  | `befb5b` | 1 | dead body investigated |

- `abe2f9` fired at 54.9, 70.6 and 80.8 s with **source = the puppet** and target = the spawn anchor
  object (probably Talion's engine object). Those are the puppet being hit, not G.
- **No caller stands out as a G "attract" press.** Either G does not register a stimulus, or its
  caller is one of the generic ones above. Owner report pending: when G was pressed and what the
  game calls that key.

### Owner report (launch 19)

- **G is the game's "Attract" key.** In stealth, pressing G makes nearby orcs (including the
  branded puppet) go to the place where Talion was when G was pressed.

### Derived

- Attract is "an AI goal toward a position that the player chose", exactly what a walking puppet
  needs. The NPC side has `CAIGoalAttract`, `AI.Goals.ExtraData.Attract` and
  `CAIInterpreterAttractTactile` (a listener for that kind of stimulus).
- The player-side stimulus function `bde7d0` (player object; keeps two stimulus handles at
  `+0xD50`/`+0xD54`, with types from a player constants record via `bd8460`/`bd8500`) never
  fired in launch 19. Probably G was not pressed in stealth that run, and `bde7d0` may not be
  Attract at all.

### Built (not yet run): stimulus log v2

- Same entry hook. It now also logs **every** stimulus whose source is Talion's engine object and
  whose type is not null (up to 300), with type, target and position (`+0x28`). Other callers stay
  limited to 6 logs each.
- Guard SHA-256 `922449e8…5cf4f996`. Suite 33/33, negative control 14/14. Installed with only
  `stimlog=1` (no puppet). Backups in `evidence\before-stimlog2` (save `0619f184…`).
- Plan for the run: the owner goes into stealth near some orcs and presses G a few times, noting
  roughly when.

## Session 2026-11-21: launch 20, stimuli from Talion (logging only, owner-approved)

Guard `922449e8…5cf4f996`, `stimlog=1` only.

### Observations

- Pre-flight backup check 0. World loaded at ~117 s; the game exited with code 0 at ~241 s.
  Backup check after: 0 changed. Post-test save `cccdf047…` kept in
  `evidence\launch-20-stimlog2`. `stimlog` set back to 0.
- **31 stimuli had Talion as source.** Grouped by caller and type record:
  - Combat groups (attack, hit, react), each with a target orc: `c10f33`, `c11063`, `c74806`,
    `bf1b02`, `bf1c88`.
  - `bde7d0` (`bdeae0`/`bdec78`) once at 232.6 s, with a target.
  - **`bea53a` (= `bea3b0`), no target, at Talion's own position**: type `…D748` and `…CF38` at
    load; then **`…CAB8` at 161.9 s, `…CF38` at 165.4 s; `…CAB8` at 234.6 s, `…CF38` at 235.3 s.**
- Static:
  - `bea3b0(character, type, &handle)` registers a stimulus of `type` from the character at its
    current position, and replaces the one held in `handle`.
  - It is called from `bee640`, which is in **both the NPC vtable and the `CPlayerObj` vtable**:
    the per-character "presence" stimuli. Two handles: `+0x2D4` (type from the character
    record, `be7e50`) and `+0x2D8` (type from the character's own `vt+0x318`).
  - For `CPlayerObj`, `vt+0x318` (`bde0d0`, not a Ghidra function) checks a list at
    `*(player+0xB60)+0x630` and a counter at `+0xD64`. If an entry is present it returns a
    type through `bea5c0`; otherwise it returns the default from the character record (`bd83bd`).

### Derived (needs the owner's timing to confirm)

- Talion's second presence stimulus **switches to type `…CAB8` for a few seconds, twice**. That
  matches two G presses, ~45 s and ~117 s after the world loaded. If so:
  - **Attract = Talion's presence stimulus temporarily switched to an "attract" type**, at his
    position, with no target;
  - nearby orcs (including the branded puppet) then walk to that position.
- To reproduce it for a puppet:
  1. Get the attract type (capture it at runtime while the player attracts, or find its record
     getter `bea5c0`).
  2. Register a stimulus of that type at the remote player's position (`RegisterStimulus` with
     our own descriptor: type `+0x00`, source `+0x20`, position `+0x28`, scale `+0x60`), with
     the puppet as target if targeting restricts it to the puppet.

### Owner report (launch 20)

- The owner pressed G **several times from different positions**, to attract enemy orcs (exact
  count and times not noted).
- That fits the log: the two `…CAB8` switches happened at clearly different positions,
  (-404, 520, 4721) and (1800, -1150, 4537). Further presses inside an active attract window
  would not show, because `bea3b0` only re-registers when the type changes. **`…CAB8` is taken
  as the Attract stimulus type for that run** (record pointers differ between runs).

### Next (proposed)

- Attract test (behaviour change, needs the owner's go-ahead):
  1. The guard learns the attract type at runtime, from the first player presence stimulus
     whose type differs from the one at world load. The owner presses G once to "teach" it.
  2. With the branded puppet spawned, every 3 s it registers a stimulus of that type at a point
     1.5 m beside Talion. The descriptor is built like `bea3b0` builds it: type `+0x00`, weak
     reference to a source object `+0x08`..`+0x20`, position `+0x28`, scale 1.0 `+0x60`, `+0x68` = 1.
     The source is the runtime node object (so the position is ours), and the target is the
     puppet if possible.
  3. Watch whether the puppet walks there, and whether other orcs are drawn too.

### Built (not yet run): attract test

- Guard option `attracttest=1` (needs `stimlog=1`, `spawntest=1`, `brandtest=1`).
- **Learning the type**: the stimulus entry hook watches player presence stimuli from `bea3b0`
  (return address `exe+BEA53A`, source = Talion, no target). Types seen in the first 3 s after
  the first one count as "normal"; the first different type afterwards is taken as Attract and
  logged. The owner presses G once in stealth to teach it.
- **Emitting**: in the server-update hook, once the type is known and the puppet exists, every
  3 s for 90 s:
  1. Remove the previous test stimulus (`b0be50(mgr, handle)`; manager `*RVA 19855E8`).
  2. Build a 0x90-byte descriptor exactly like `bea3b0`: type `+0x00`; weak ref `+0x08`..`+0x20`
     with vtable `exe+13DDF90`, source = Talion's engine object; position `+0x28` = Talion + 300
     units on x; `+0x38` = player `+0x48`; empty target weak ref `+0x40`..`+0x58`; `+0x60` = 1.0;
     `+0x68` = 1.
  3. Link the source reference into Talion's engine object list (`+0x28`/`+0x30`) as `bea3b0` does.
  4. Call `RegisterStimulus` (through the hooked entry, so it is logged) under SEH, then unlink.
  5. Log the handle, Talion's and the puppet's positions, and the puppet-to-point distance.
- Guard SHA-256 `03bf0fe8…5620262d`. Suite 33/33, negative control 14/14. Installed with
  `stimlog=1`, `spawntest=1`, `brandtest=1`, `attracttest=1`; walk/move off. Backups in
  `evidence\before-attracttest` (save `cccdf047…`).

## Session 2026-11-21: launch 21, attract test (behaviour change, owner-approved)

Guard `03bf0fe8…5620262d` with `stimlog=1`, `spawntest=1`, `brandtest=1`, `attracttest=1`.

### Observations

- Pre-flight backup check 0. At 31.6 s the normal player presence types were learned (`…E748`,
  `…DF38`), and the puppet was spawned and branded.
- **56.6 s: learned attract type `…DAB8`** (the first new player presence type; the owner's G).
- **29 test stimuli registered over 84 s, every call returned a handle, no exception, no FATAL.**
  The point followed Talion (300 units on x).
- The puppet barely moved while Talion went far away (down the tower to y≈490 and back).
  Puppet-to-point distance 300–3,750 units most of the time. Between 75 and 81 s it moved about
  400 units, (-3524, 4530) → (-3570, 4128), ending near the point's z but on the other side in x.
  Unclear whether it was walking to the stimulus.
- The game exited with code 0 at ~143 s (the 142 s screenshot shows the main menu: the owner
  quit). Backup check after: 0 changed. Post-test save `daa2f058…` kept in
  `evidence\launch-21-attracttest` with the log and screenshots. All test switches set back to 0.

### Derived

- **A hand-built attract stimulus is accepted by the game**: registering and removing 29 of them
  raised no error and broke nothing visible in the log.
- The puppet did not clearly follow it. Possible reasons (unverified):
  - a branded orc may ignore attract stimuli from Talion (allies are not lured);
  - the point was often far away (Talion moved quickly);
  - the attract goal may need the NPC to be idle in stealth context.

### Owner feedback (launch 21)

- The owner did not see the point of the attract experiments: in stealth, G attracts any orc,
  branded or not. The purpose (making the puppet walk to a chosen spot automatically, with the
  game's own movement) had not been made clear enough.
- Derived from that remark: Attract may only work while Talion is in stealth. The test's
  automatic signals were sent whether or not he was crouched, which could explain why the
  puppet ignored them. (Guess, untested.)

## Decision 2026-11-21: sliding puppet accepted for now

- The owner chose to accept the teleported ("sliding") puppet for now and move on. Proper walking
  (`GotoNode`, Attract) is parked. Known state: spawn, brand (passive) and per-frame placement
  work; walking does not.
- Next missing pieces:
  1. A **network link** between two game instances (send own Talion's position and rotation,
     receive the other's, place the puppet).
  2. A **Talion-like look** for the puppet.
- Proposed order: the network link first, tested with a **bot**, per AGENTS.md "test with bots,
  not the owner". A small local program sends a scripted path over loopback UDP, and the guard
  places the puppet from what it receives. The guard's network block already allows loopback,
  so this needs no change to the isolation. Talking to a friend's PC later is a separate,
  owner-approved step (non-loopback traffic, NAT/relay).

## Session 2026-11-21: first network link (loopback) and a bot

### Built (not yet run)

- Guard option `netlink=1` (with `spawntest=1`, `brandtest=1`). It runs in the server-update
  hook, on the game thread.
  - UDP socket bound to **127.0.0.1 only**, port `net_port` (default 47610), non-blocking.
  - Every frame it reads all waiting packets and keeps the newest valid one. If that packet is
    less than 1 s old, the puppet is placed at its transform (object API `vt+0xB0`, as in the
    move test, after the same live-object check).
  - 20 times a second it sends the local Talion's transform to 127.0.0.1:`net_peer_port`
    (default 47611).
  - Packet: 36 bytes, `"SOMC"` | u32 sequence | f32 position[3] | f32 rotation[4] (x y z w).
    Dropped and counted: wrong length, wrong tag, sequence 0 or not newer, non-finite values,
    position beyond 1e6 units, rotation length outside 0.9–1.1.
  - Logs once a second: received, dropped, placements, sent, last packet, puppet and player
    positions.
- **Bot** `bot\somc_bot.py` (lab): listens on 127.0.0.1:47611 for Talion's packets and sends
  "remote player" packets to 127.0.0.1:47610: a circle of 300 units (~3 m) around Talion, one
  turn per 10 s, 30 per second, facing along the circle. Checked offline against a fake guard:
  70 packets in 2.5 s, radius exactly 300.0, unit rotations.
- `launch-test.ps1` gained `-Bot`: starts the bot (hidden, same duration, log
  `bot-<stamp>.log`) after the pre-flight, stops it after the game, prints its last log lines.
- New offline test `net_packet` (accepts a valid packet; rejects each kind of bad one). Suite
  34/34, negative control 14/14. Guard SHA-256 `42846dc9…77fc17c6a`.
- Installed with `spawntest=1`, `brandtest=1`, `netlink=1`. Backups in `evidence\before-netlink`
  (save `daa2f058…`).
- Nothing leaves the PC: the guard sends only to 127.0.0.1, and the exe's own network stays
  limited to loopback. Talking to a friend's PC is a later, separate step.

## Session 2026-11-21: launch 22, network link with the bot (behaviour change, owner-approved)

Guard `42846dc9…77fc17c6a` with `spawntest=1`, `brandtest=1`, `netlink=1`. Launched with `-Bot`.

### Observations

- Pre-flight backup check 0. The bot started before the game; the guard bound 127.0.0.1:47610 at
  26.8 s. The puppet spawned and was branded once the world loaded (packets flowed from ~35 s).
- **Over ~70 s in the world:**
  - **1,809 bot packets received (~27/s), 0 rejected.**
  - **~6,300 puppet placements** (every frame while packets were fresh).
  - 1,071 Talion packets sent (~16/s; aimed at 20/s, limited by the frame-time check).
  - The bot received 1,079 of them, with 0 bad.
- **The puppet sat exactly at each received position** (log: puppet = last packet). Packet age
  at log time was 0–31 ms. The puppet stayed ~300 units from Talion and **followed him from the
  tower top (y≈2712) down to the ground (y≈518)** and across the area.
- Screenshot at 80 s: Talion at the foot of the tower, the branded orc a few metres away.
- The game exited with code 0 at ~106 s (owner quit). The bot was stopped by the script. Backup
  check after: 0 changed. Post-test save `fa4fcfd1…`, log, bot log and screenshots kept in
  `evidence\launch-22-netlink`. Test switches set back to 0.

### Derived

- **The full loop works on one PC: game → network → other side → network → game → puppet.** The
  receiving side places a puppet from network data in real time with no rejects and low latency;
  the sending side publishes Talion's transform. This is the "see each other" core of co-op,
  minus a second real game, the internet path and the Talion look.

## Session 2026-11-21: two copies on one PC, prepared (not yet run)

- **Single-instance check (static):** no window search imports, no "already running" or "another
  instance" strings; `CreateMutexA/W` are imported, but no named-mutex guard was found. Whether a
  second copy starts, and whether Steam accepts a second `SteamAPI_Init` from it, is unknown
  until tried.
- **Guard profiles:** env `SOMGUARD_PROFILE` (1–8 letters or digits; anything else is ignored).
  An ini key `<key>_<profile>` overrides `<key>`. Profile B uses `sandbox_B` (its own saves,
  settings and log) and swapped ports: `net_port_B=47611`, `net_peer_port_B=47610`. So copy A
  listens on 47610 and sends to 47611, and copy B the reverse. New offline test
  `profile_sandbox`: profile B writes to `sandbox-B`; a bad profile value falls back to the
  default sandbox. Suite 35/35, negative control 14/14. Guard SHA-256 `b515ab70…bc808cff`.
- **`launch-test.ps1 -Second [-SecondDelay 25]`**:
  - starts copy B (profile B) 25 s after copy A, from the same test copy (no second 42 GB copy);
  - reports B's exit;
  - closes B when A ends or at the time limit;
  - prints B's guard log.

  Screenshots are of A only.
- **`sandbox-B`**: copy of the current sandbox save (`fa4fcfd1…`) and settings, with a 960×540
  window at (940, 500) so both windows fit on screen.
- Installed with `spawntest=1`, `brandtest=1`, `netlink=1`. Each copy shows the other's Talion
  as a branded orc 1:1 at the received position (no offset). Backups in
  `evidence\before-twocopies` (guard, ini, save, launch script).
- Free RAM before the run was low for two copies. The owner should close
  other programs first.

## Session 2026-11-21: launch 23, two copies: the second one quit (owner-approved)

Guard `b515ab70…bc808cff`, `launch-test.ps1 -Second`.

### Observations

- Pre-flight backup check 0. Copy A started at 02:14:52. Copy B (profile B) started at 02:15:17
  and **ended at 02:15:19 with exit code 0**. Its guard had fully set up (sandbox-B, hooks,
  `profile=B`). The game then exited before `SteamAPI_RestartAppIfNecessary`, which copy A
  reaches at ~0.4 s.
- Copy A ran ~11 min. Its puppet spawned and was branded. It sent ~10,000 packets to port 47611
  and received none (no copy B). The owner quit; exit code 0. Backup check after: 0 changed.
  Log, B's log and A's save (`a61be21f…`) kept in `evidence\launch-23-twocopies`.
- **Cause (static, `decomp-mutex.txt`)**: engine main `e043e0` creates
  `CreateMutexW(NULL, FALSE, L"Global\{43570706-DA86-477B-9C2D-2F3D4F95FDE4}")`. If it
  already exists (`GetLastError() == 183`), it restores and focuses the first copy's window
  (HWND stored in the exe's `.SHARED` section at `RVA 1B55000`, shared by all copies) and
  returns 0. The check is skipped when the command-line option `RunningUnderToolControl` is
  present (a developer mode; other effects unknown, not used).

### Fix (built, not yet run)

- For a profile only, the guard patches the exe's `CreateMutexW` import. That one mutex name gets
  `_coop<profile>` appended; every other call passes through unchanged. Copy A (no profile) is
  unaffected.
- New offline test `mutex_rename` (profile B): the renamed mutex exists, the original name does
  not, other names are untouched. Suite 36/36, negative control 14/14. Guard SHA-256
  `bc43bcf4…34d3d62`. Installed (ini unchanged: `spawntest`, `brandtest`, `netlink` on). Previous
  guard in `evidence\before-mutexfix`.
- Known side effect: both copies still share the exe's `.SHARED` section (one window handle, used
  only by this check).

## Session 2026-11-21: launch 24, two copies talking to each other (owner-approved)

Guard `bc43bcf4…34d3d62` with `spawntest=1`, `brandtest=1`, `netlink=1`, `launch-test.ps1 -Second`.

### Observations

- Pre-flight backup check 0. Copy A started at 02:32:13; copy B (profile B) at 02:32:38.
- **Copy B got past the single-instance check**: "single-instance mutex renamed to
  …_coopB". It reached `SteamAPI_RestartAppIfNecessary` (0.5 s) and the sandboxed
  `SteamRemoteStorage` (1.7 s), so **Steam accepted a second copy** launched this way.
- Both copies loaded the world, spawned and branded their puppet (A at 45.1 s, B at 51.0 s), and
  exchanged packets over loopback (A: 47610 in / 47611 out; B the reverse):
  - **B received A's Talion** and placed its puppet exactly there. For example, B's puppet at
    (-3323.60, 2712.14, 4267.71) = A's player position at that time.
  - **A received B's Talion** and placed its puppet there: 969 packets by 135 s, 0 rejected,
    ~4,100 placements; packet age 15–63 ms while both ran.
- A's screenshot at 111 s: Talion on the tower with the branded orc (B's Talion) next to him.
- The owner closed both: B at 02:34:39, A at 02:34:47, both exit code 0. Memory held for ~2 min
  with both copies in the world. Backup check after: 0 changed. Logs, both saves (A `08e0e0f2…`,
  B `dfce80c2…`) and screenshots kept in `evidence\launch-24-twocopies`. Test switches set back
  to 0.
- **Glitch**: around 75 s, A briefly placed its puppet at (0, 0, 0). B sends as soon as its player
  object exists, and its first packets carried a zero transform while it was still loading. Fix
  to do: ignore (sender) or reject (receiver) an all-zero position.

### Derived

- **Two-way "see each other" co-op works on one PC**: two real game copies, each showing the
  other's Talion as a puppet that follows in real time. Remaining for the first playable step:
  1. Talion look.
  2. The internet path to a friend (non-loopback traffic, NAT; owner-approved separately).
  3. Proper walking animation (parked).
  4. The zero-position glitch.

## Session 2026-11-21: giving the puppet Talion's look (built, not yet run)

### Observations (static)

- Model-related names in the exe: `CharacterModel`, `CharacterModelRecord`, `DBCharacterModels`,
  `OverrideCharacterModel` (a bool field), `DefaultPlayerModel`, `OverridePlayerModel`,
  `AllowOverridePlayerModel`, `InGameCharacterModel`, `DefaultCharacterModel`, `SkinBundles`,
  `Player_Character_Skin7` (Talion's skins). The Nemesis faction-member save also stores a model
  record (`8558c0`, member `+0x2B8`).
- A character's main record is at **`character+0x2A8`**. `bee640` reads the presence stimulus
  type from it (`be7e50(char+0x2A8)`, `bd83bd(char+0x2A8)`).
- Record layout used by the getters (`3f3800`, `8b7b20`, `ef7ba0`):
  - `+0x00` field count; `+0x01` flags (bit 0 picks the key layout);
  - `+0x08` 16-byte field entries: key at `+0`/`+4`, type `+0xA` (10 = link, 1 = bool),
    present `+0xC`, value index `+0xE`;
  - `+0x10` 8-byte value slots (bools are bits: slot `idx>>5`, bit `idx&31`).

### Derived (guess, checked at runtime by the test)

- If `character+0x2A8` **is** the CharacterModel record, then the AI.Template's CharacterModel
  equals the `+0x2A8` of an NPC spawned from it. Talion's `+0x2A8` is then his model, and a copy of
  the orc template pointing at it should spawn an NPC that looks like Talion.

### Built

- Guard option `talionlook=1` (with `spawntest=1`).
  1. When the puppet is about to spawn, it reads the template's CharacterModel (own reader, same
     logic as `8b7b20`, no game code called), the `+0x2A8` of the game's own NPC that was just
     spawned from that template, and Talion's `+0x2A8`, and logs all three.
  2. **Only if the first two match** (and Talion's differs), it clones the template: header and
     value array copied to heap memory that is never freed, CharacterModel slot = Talion's record.
     The puppet is spawned from the clone.
  3. Otherwise it logs the mismatch and spawns a normal orc puppet.
- New offline test `talion_clone` (fake descriptor and record in the same layout: correct read,
  clone gets the new model, original untouched, other slots including a bool slot copied, a
  record without the field gives 0). Suite 37/37, negative control 14/14. Guard SHA-256
  `a94cb791…3f156265`.
- Installed with `spawntest=1`, `brandtest=1`, `netlink=1`, `talionlook=1`, for a run with `-Bot`
  (the puppet circles Talion at 3 m). Backups in `evidence\before-talionlook` (save `08e0e0f2…`).
- Risk: Talion's model may lack NPC parts (the exe has "CharacterModel (%s) does not specify a
  combat tree"). The spawn is SEH-guarded, but a later crash would not be caught.

## Session 2026-11-21: launch 25, Talion look, first try (owner-approved)

Guard `a94cb791…3f156265`, `spawntest`/`brandtest`/`netlink`/`talionlook` on, launched with `-Bot`.

### Observations

- Pre-flight backup check 0. At 36.4 s:
  - template CharacterModel `…258A6810`;
  - the game NPC's `+0x2A8` `…250BECF0`;
  - Talion's `+0x2A8` `…250BDF10`.
- **The self-check failed** (the first two differ), so the guard did **not** change anything and
  spawned the usual branded orc. The bot circle then worked as in launch 22: ~2,600 packets, 0
  rejected.
- At 32.9 s the guard sent Talion's position as (0, 0, 0) (player object not placed yet), and the
  bot echoed a circle around the origin. This is the same zero-position glitch as launch 24.
- The game exited with code 0 at ~143 s (owner). Backup check after: 0 changed. Evidence in
  `evidence\launch-25-talionlook` (save `8ae32b5e…`). Test switches set back to 0.

### Derived

- `character+0x2A8` is **not** the template's CharacterModel record. It is a different record,
  perhaps reached from the CharacterModel (an appearance variant, for example; orcs get
  generated looks).

### Changes (built, not yet run)

- **Zero-position glitch fixed:** the guard never sends a Talion position of exactly (0, 0, 0),
  and rejects such packets (offline test extended: error 130).
- **Talion look, second try:**
  1. Log all fields (keys, types, value slots, link values) of the three records.
  2. If the template's CharacterModel record has a link field whose value is the NPC's `+0x2A8`
     record, clone the CharacterModel with that slot = Talion's `+0x2A8`, then clone the template
     with CharacterModel = that clone, and spawn the puppet from it.
  3. Otherwise spawn a normal puppet; the dump still shows the structure for the next step.
  - New helpers `RecordSlots`, `CloneRecordSlot`, `FindLinkSlot` (offline test: finds link slots
    only, not bool slots). Suite 37/37, negative control 14/14.
  - Installed with `spawntest`, `brandtest`, `netlink`, `talionlook` on, for a `-Bot` run. Backups
    in `evidence\before-talionlook2`.

## Session 2026-11-21: launch 26, Talion look, second try (owner-approved)

Guard `6179299b…f0e2200b`, `spawntest`/`brandtest`/`netlink`/`talionlook` on, `-Bot`.

### Observations (record dump)

- **Template CharacterModel record**: 88 fields, 78 value slots. **Slot 50** (field key
  `64A6C5C3/249112DF`, a link) holds **the game NPC's `+0x2A8` record**.
- **The NPC's `+0x2A8` record and Talion's `+0x2A8` record have the same layout**: 65 fields,
  identical keys, types and slots. So both are the same record type: the character's visual or
  character record. Some links are empty in Talion's (orc-only parts), others differ
  (equipment, voice and similar).
- The guard took the two-level path:
  1. Cloned the CharacterModel record with slot 50 = Talion's record (`→ 1F2B72DB0`).
  2. Cloned the AI.Template (4 slots; CharacterModel in slot 3) to point at that clone
     (`→ 1F2B72ED0`).
  3. Called the NPC spawn helper with it.
- **The spawn helper returned null**: no object, no exception, no puppet. The game kept running
  normally; the owner quit at ~75 s, exit code 0. Backup check after: 0 changed. Evidence in
  `evidence\launch-26-talionlook2` (save `59091342…`). Test switches set back to 0.

### Derived

- The record chain is: **AI.Template →(CharacterModel)→ CharacterModel record →(slot 50)→ character
  record**, the same type as Talion's own `+0x2A8`.
- Spawning from **heap copies** of records fails quietly. Guess: the game identifies records by
  where they live (the loaded database), or `b02bc0` removed the result. Next approach: no
  copies. Temporarily write Talion's record into slot 50 of the **real** CharacterModel record,
  spawn the puppet, and put the original value back straight after, all on the game thread
  inside the same call.

### Built (not yet run): Talion look, third try (in-place swap)

- Instead of heap copies, the guard briefly points the **real** CharacterModel record's link
  slot (the one holding the NPC's `+0x2A8` record; slot 50 in launch 26) at Talion's `+0x2A8`
  record, **only for the puppet's spawn call**: same thread, same call. It restores the original
  value right after and logs the read-back ("ok"/"MISMATCH").
- `PokeSlot` refuses slots outside the record and memory that is not committed and writable.
  The offline test `talion_clone` gained cases: swap and restore in place, refused beyond the
  record, refused on read-only memory (untouched). Suite 37/37, negative control 14/14.
- Installed with `spawntest`, `brandtest`, `netlink`, `talionlook` on, for a `-Bot` run. Backups
  in `evidence\before-talionlook3`.
- Risks:
  - other code may read that slot during the same call (only the spawning thread runs it);
  - the game may cache per-model data elsewhere (a mixed look, or another null spawn).

## Session 2026-11-21: launch 27, Talion look, third try (owner-approved)

Guard `68272fc0…42c1d01b`, `-Bot`.

### Observations

- Same chain as launch 26 (CharacterModel slot 50 → NPC character record; Talion's record has
  the same layout).
- In-place swap: "slot 50: …5CF0 -> …4F10 for this spawn only", then "restored slot 50 = …5CF0
  (ok)". **The spawn helper still returned null.** No puppet, no exception. The bot loop ran with
  nothing to place. The game exited with code 0 at ~163 s (owner). Backup check after: 0 changed.
  Evidence in `evidence\launch-27-talionlook3` (save `b2d137a6…`). Switches set back to 0.

### Derived

- Copies were not the problem: **an NPC built on Talion's character record is rejected.**
  Likeliest point: `b02bc0` deletes a new NPC when spawn flag A (spawn data `+0x29`, = the
  helper's 5th argument, 1 in all game spawns seen) is set and `be30f0(npc)` fails. `be30f0` asks
  the character's `+0x9F0` object (or its parent's) `vt+0x138` whether it is valid; probably a
  controller or physics object that Talion's record does not build for an NPC. (Guess.)

### Built (not yet run): keep variant

- Option `talionlook_keep=1`: when the in-place swap is active, the puppet's spawn passes flag A
  = 0, so `b02bc0` skips the `be30f0` removal. `spawnlog=1` is also on, so the log shows whether an
  NPC object is created at all.
- Suite 37/37, negative control 14/14. Guard SHA-256 `95071354…62d830be`. Installed with
  `spawnlog`, `spawntest`, `brandtest`, `netlink`, `talionlook`, `talionlook_keep`. Backups in
  `evidence\before-talionlook4`.
- Risk: a kept but invalid NPC may behave badly (no controller: it may fall, freeze or crash the
  game later).

## Session 2026-11-21: launch 28, Talion look, keep variant (owner-approved)

Guard `95071354…62d830be`, `spawnlog` + Talion look with `talionlook_keep=1`, `-Bot`.

### Observations

- Same chain and in-place swap as launch 27. Flag A was cleared ("1 -> 0").
- **`SPAWN NPC #2 obj=…51D84E0` was logged during the puppet's spawn call**: the NPC create
  function ran, so the game object was allocated. The spawn helper still returned null, and
  **NPC #3 (the game's next orc) got the same address `…51D84E0`**. So the puppet object was
  destroyed straight away, inside the create path. The slot was restored ("ok").
- The game exited with code 0 at ~50 s (owner). Backup check after: 0 changed. Evidence in
  `evidence\launch-28-talionkeep` (save `997818bb…`). **All test switches set back to 0**,
  including `spawnlog` and `talionlook_keep`.

### Derived

- The rejection is **not** the `be30f0` check in `b02bc0`. It is earlier, in the engine create
  `e54860`: after allocating the game object it calls `object->vt[0x40](object, params)` (the
  object's own set-up). If that returns false, it frees the object (`e51c20`/`e51c80`) and returns
  null. **The NPC's set-up refuses a character record that is Talion's.** Why is unknown: the
  record has empty orc-only links that an NPC may require (combat tree, AI parts).
- Options from here:
  1. Read the NPC set-up (`NPC vtable +0x40`) to find which field it requires, and fill that one
     link from the orc's record during the swap (a "hybrid" record). Static work, then a test.
  2. Use a **human NPC** template instead of Talion (the game has human NPCs, e.g. slaves). It
     would not be Talion, but human-looking and likely accepted. Capturing a human template
     needs a run in an area with humans.
  3. A second `CPlayerObj` through the player spawner `50aa00`: exactly Talion's look, but it
     binds controls and camera (unknown how to prevent).

## Session 2026-11-21: what the NPC set-up needs (phase 3)

Static, read-only. Reports `decomp-npcinit.txt`, `decomp-npcinit2.txt`, `decomp-fields.txt`.

### Observations

- NPC vtable `+0x40` = **`ab5090`**, the NPC's set-up from the create params (called by `e54860`;
  false means the object is freed). It returns false when:
  - a global flag (`1983590`) is set;
  - `bee2b0` (character set-up) fails;
  - the spawn data asks for something `aad890` rejects;
  - **`ab1fa0` (NPC set-up) fails**;
  - with flag A, the nav-mesh placement query (`DAT_1418f1450 +0x28` → `vt+0xC0`) fails.
- `ab1fa0` reads seven link fields of the character record (`npc+0x2A8`) through generated
  getters: `aafba0`, **`aafcc0` (returns false if empty)**, `aafd60`, `aafe00`, `aafea0`,
  `aac7b0`, `ab0240`. It then looks the `aafcc0` value up in a manager (`aac580`) and sets up
  the NPC's AI parts (`npc+0xC20`…`+0xCC8`).
- These getters use a field-descriptor table at `*(RVA 184C200)` (`= RVA 1973AE0`, **filled at
  start-up**, zero in the file). Descriptor offsets: `+0x150`, `+0x1B0` (the required one),
  `+0x1E0`, `+0x210`, `+0x270`, `+0x3F0`, `+0x5A0`. Each descriptor is
  `{name, key u32 @+8, mode @+0x18, index byte @+0x28}`, read the same way as the
  CharacterModel field.
- Launch 26's dump: Talion's record has several of its 65 link fields empty where the orc's are
  set, so the required field is probably one of them.

### Built (not yet run): diagnosis plus hybrid

- During the puppet's spawn (with the slot-50 swap), the guard logs for each of the seven
  descriptors: its **name** (read at runtime), and whether Talion's and the orc's record have it,
  with values.
- With `talionlook_hybrid=1`, every one of those fields that is **empty in Talion's record and set
  in the orc's** gets the orc's value written into Talion's record for the spawn call only. It is
  restored right after, in reverse order, and read back ("ok"/"MISMATCH"). The flag-A keep is
  also on.
- Suite 37/37, negative control 14/14. Guard SHA-256 `bc452735…98e3b231`. Installed with
  `spawnlog`, `spawntest`, `brandtest`, `netlink`, `talionlook`, `talionlook_keep`,
  `talionlook_hybrid`. Backups in `evidence\before-talionhybrid`.
- Risk: Talion's own record (the player's) is modified for that one call. Only the spawning
  thread runs then, but anything reading the player's record at that moment would see orc AI
  parts.

## Session 2026-11-21: launch 29, hybrid Talion record (owner-approved)

Guard `bc452735…98e3b231`, Talion look with `keep` and `hybrid`, `-Bot`.

### Observations

- The NPC set-up fields, by name (read at runtime from the descriptor table):

  | Descriptor | Name | Talion's record | Orc's record |
  |---|---|---|---|
  | `+0x150` | SenseUpdateRate | no entry | no entry |
  | `+0x1B0` | **AIGoalSet** (required) | slot 29 = empty | set |
  | `+0x1E0` | AIActionSet | slot 48 = empty | set |
  | `+0x210` | TargetSelectSet | slot 20 = empty | set |
  | `+0x270` | AIMovementSet | slot 38 = empty | set |
  | `+0x3F0` | DespawnAwayFromPlayerWhenBranded | no entry | no entry |
  | `+0x5A0` | SensorSet | same value | same value |

- Hybrid: the four empty links in Talion's record were set to the orc's values for the spawn call.
  All were restored afterwards (read back "ok"), and so was CharacterModel slot 50.
- **The puppet spawned and was kept**: object `…5F8FDB0`, NPC vtable. It was branded and placed
  by the bot loop (968 placements by 31.7 s). The game's next NPC (#3) got a different address,
  so the puppet survived.
- The owner quit after ~50 s (main menu in the 48 s screenshot; exit code 0). There is no in-world
  screenshot of the puppet. Backup check after: 0 changed. Evidence in
  `evidence\launch-29-talionhybrid` (save `763b6a0a…`). **All test switches set back to 0.**

### Derived

- **Talion's character record is missing only the NPC AI links** (AIGoalSet, AIActionSet,
  TargetSelectSet, AIMovementSet). Lending the orc's for the one spawn call makes the game accept
  an NPC built on Talion's record. Whether it **looks** like Talion: owner report pending.

### Owner report (launch 29)

- **"There was an orc circling me."** The owner quit themselves.

### Derived

- An NPC built on **Talion's character record** (`+0x2A8`, the 65-field record) still **looks like an
  orc**. So that record carries behaviour (AI sets, senses, stimuli), **not the appearance**. The
  look comes from the 88-field **CharacterModel** record. It has many links that are probably
  mesh, skin and part sets (slots 40–76).
- Next: find **Talion's own CharacterModel record**. Scan the player object for a pointer to a
  record with the same layout as the orc's 88-field CharacterModel. Point the template's
  CharacterModel at it for the spawn, and fill every link that is empty on Talion's side from the
  orc's, at both levels (CharacterModel and character record). Restore everything after.

### Built (not yet run): whole-CharacterModel swap

- Option `talionlook_model=1` (with `talionlook=1`, `talionlook_keep=1`; the older swap and hybrid
  paths are skipped).
  1. **Find Talion's CharacterModel**: scan the player object (`0xDD8` bytes, then its `+0x10`
     object) for a pointer to a record of the same type as the orc's CharacterModel (same field
     count and same first four keys). Also scan the game NPC to learn where an NPC keeps it. All
     hits are logged.
  2. Point the AI.Template's CharacterModel slot at Talion's CharacterModel (in place).
  3. **Borrow empty links**: every link empty in Talion's CharacterModel but set in the orc's
     gets the orc's value; same for Talion's character record (the one his CharacterModel links in
     the orc's slot-50 field) against the orc's `+0x2A8` record.
  4. Spawn the puppet with flag A = 0.
  5. **Undo every write in reverse order** and read each back; mismatches are counted and logged.
- New offline test cases (in `talion_clone`): same-type detection; fill borrows only empty→set
  links and ignores bool slots; restore returns the slot to empty. Suite 37/37, negative control
  14/14. Guard SHA-256 noted in the private notes. Backups in `evidence\before-talionmodel`.
- Risk: Talion's own CharacterModel and character record are changed in place for that one call.

## Session 2026-11-21: launch 30, whole-CharacterModel swap (owner-approved)

Guard `8173050d…d5bfd3df`, `talionlook_model` + `keep`, `-Bot`.

### Observations

- Scan: no CharacterModel-type pointer in the game NPC object or in Talion's player object
  itself, but **one in Talion's `+0x10` object at `+0x70` → `…2595BAA0`** (same type as the orc's
  88-field CharacterModel `…2595D810`). So **Talion's CharacterModel is at `*(player+0x10)+0x70`.**
- The template's CharacterModel slot (3) was pointed at Talion's for the spawn. **6** empty links in
  Talion's CharacterModel and **15** in his character record were borrowed from the orc's.
- **The puppet spawned and was kept**: object `…56684E0`, NPC vtable, branded, placed by the bot
  loop. **All 22 in-place writes were undone with 0 mismatches.**
- The game exited with code 0 at ~63 s (owner). The 48 s screenshot shows Talion alone in
  Wraith view; the puppet is out of frame. Backup check after: 0 changed. Evidence in
  `evidence\launch-30-talionmodel` (save `8d582a54…`). **All test switches set back to 0.**

### Unknown

- What the puppet looked like: owner report pending.

### Owner report (launch 30)

- **"Orc."** The puppet built from Talion's CharacterModel and character record still looked
  like an orc.

### Derived

- An orc's appearance does not come from the AI.Template → CharacterModel → character-record
  chain alone. Likely sources (unverified):
  - the Nemesis/hierarchy appearance system (`CharacterModelMgr`, `HierarchyCharacterModelMgr`,
    `HeadModelSwap`, `ModelSwap`), which generates each orc's look;
  - the spawner's appearance data (the helper's arguments 7 and 9, which the puppet spawn passes
    as null).
- Alternative that sidesteps NPC looks entirely: a **second `CPlayerObj`**, created directly with
  the object API's create (`+0x48`, class `"CPlayerObj"`, the same block the player spawner
  `50aa00` builds), **without** the spawner's controller and camera binding that follows the
  create in `50aa00`. It would carry Talion's real model. Unknowns: whether a second `CPlayerObj`
  initialises without a client binding, and how the "first player" sites react (the real player
  stays first in the list).

## Session 2026-11-21: a second CPlayerObj as the puppet (built, not yet run)

- Guard option `secondtalion=1`, with `spawntest=1` (used only as the trigger: the first game NPC
  spawn) and `netlink=1`. Instead of an NPC puppet, the guard creates a **second `CPlayerObj`**
  with the object API's create (`+0x70` get class `"CPlayerObj"`, `+0x48` create), filling the block
  as the player spawner `50aa00` does:
  - flags `0x20`;
  - position = Talion + (200, 0, 200), Talion's rotation, scale 1.0;
  - `+0x28` → `{0, character data, 0}`, with character data =
    `405600(*RVA 18F1228 (client shell), 474dc0(null) (bundle "Default"), *RVA 18F99C8)`;
  - name `"CoopTalion"`;
  - object set = `*(g_pGameServerShell+0x9A8)`.
- **Not done:** the client, controller and camera binding that `50aa00` performs after the create.
  The character-data and create calls are SEH-guarded and logged; the player list is logged after
  (expected `players=2`, the real Talion still first).
- The network link and move code now check the puppet against a per-puppet vtable
  (`CPlayerObj` `exe+1460160` here, NPC otherwise).
- Suite 37/37, negative control 14/14. Installed with `spawntest`, `netlink`, `spawnlog`,
  `secondtalion` on (no brand: that is NPC-only). Backups in `evidence\before-secondtalion`.
- Risks:
  - a `CPlayerObj` without a client binding may fail in its per-frame update later (outside the
    SEH-guarded call), crashing the game;
  - systems that loop over all players (AI targeting, radius queries) will see it.

## Session 2026-11-21: launch 31, second CPlayerObj (owner-approved)

Guard `359cd064…d30a56e1`, `spawntest` (trigger), `netlink`, `spawnlog`, `secondtalion`; `-Bot`.

### Observations

- Talion's own `CPlayerObj` was created as before (`50aa00`).
- At 47.3 s (first game NPC spawn), character data from `405600(…, "Default" bundle, …)` =
  `…25B5BAA0`, the same `*BAA0` pattern as Talion's CharacterModel in launch 30, so it is probably
  a CharacterModel record.
- **`SPAWN CPlayerObj #2` logged: a second `CPlayerObj` was created** (`…6803240`, vtable
  `exe+1460160`). Engine object at (-3301.08, 2713.21, 4201.15). **`players=2`**, the real Talion
  still `[0]`, both with client id 0. No exception.
- The bot loop placed it: 1,342 placements by 61 s, 0 bad packets. The game ran on and exited with
  code 0 at ~69 s (owner). The 48 s screenshot is black (still loading at that moment). Backup
  check after: 0 changed. Evidence in `evidence\launch-31-secondtalion` (save `69f6d0d5…`). **All
  test switches set back to 0.**

### Derived

- **A second player object can be created and live without the spawner's client and controller
  binding**, at least for ~20 s, and the network link can move it. Whether it is visible and looks
  like Talion, and what happened to the owner's controls and camera: owner report pending.

### Owner report (launch 31)

- **"Now I see Talion, but he is circling me in an A pose."**

### Derived

- **The second `CPlayerObj` shows Talion's real model.** The Talion look for the remote player is
  solved this way.
- It stands in the **A-pose** (the rest pose): no animation runs on it. Likely because the parts
  the player spawner sets up after the create are missing (client binding `dff0a0`, controller and
  animation set-up `418ad0`, and the calls through `+0xF0` and `784dc0` in `50aa00`). They drive
  Talion's animation from input. (Guess.)
- New state of the co-op pieces: remote player shown **as Talion**, moved from the network
  (slides), **no animation yet**.
- Next options:
  1. Animate the second Talion: find what in `50aa00`'s post-create sets up the animation
     without binding input or camera, or play the remote player's animation state on it.
  2. The internet path to a friend.

## Session 2026-11-21: animating the second Talion (phase 3, built, not yet run)

Static, read-only. Report `decomp-anim1.txt`.

### Observations

- The player spawner `50aa00` does this after the create, in order:
  1. `dff0a0(clientShell+0x10, clientByte)`: the client by id.
  2. **`418ad0(proxy, client, player)`**: `proxy+0x118 = 3f0080(clientShell+0x70, client, proxy,
     player)`, i.e. the proxy's local-player link (the `CCharacterProxy` update takes a different
     path when `+0x118` is set), then engine `vt+0x10` → `vt+0x1E8(engineObject, 7)` (camera or
     focus, guess).
  3. **`comp = player->vt[0xF0]()`**: `bde0` slot `be03c0` = `mov rax,[rcx+0xB60]; ret`. That is
     the 0x4230-byte component the `CPlayerObj` constructor allocates.
  4. **`79f950(comp)`**: resets `comp+0x3968` and `comp+0x518`, `79d7b0(comp)`, registers with a
     manager (`902210`).
  5. **`784dc0(comp, engineObject, *(player[2]+0x70))`**: builds the animation instance
     (`7a6000(comp+0x518, engineObject, …)`) from the **character model** at
     `*(player+0x10)+0x70` (the same place Talion's CharacterModel was found in launch 30), names
     it, registers (`902140`).
  6. Two engine calls (`vt+0x18`→`+0x50`, `vt+0xD0`→`+0xD8`).
- So steps 4–5 are the animation set-up; steps 1–2 are the client, controller and camera binding.

### Built

- Option `secondtalion_anim=1`: right after creating the second `CPlayerObj`, call only
  `79f950(comp)` and `784dc0(comp, engineObject, model)`. The model comes from the new player's
  `*(+0x10)+0x70`, or the real Talion's if empty. Both calls run under SEH and log the stage
  reached. Steps 1, 2 and 6 are skipped.
- Suite 37/37, negative control 14/14. Guard SHA-256 `6fecdf42…f1c51c1f`. Installed with
  `spawntest`, `netlink`, `spawnlog`, `secondtalion`, `secondtalion_anim` for a `-Bot` run.
  Backups in `evidence\before-secondtalion-anim`.
- Expectation (guess): at least an idle animation instead of the A-pose. Walk and run probably
  need movement state the controller would normally feed; teleporting may keep it idle.

## Session 2026-11-21: launch 32, second Talion with the animation set-up (owner-approved)

Guard `6fecdf42…f1c51c1f`, `secondtalion` + `secondtalion_anim`, `-Bot`.

### Observations

- Second `CPlayerObj` created as in launch 31 (`players=2`, real Talion `[0]`).
- Animation component `…10BD7B860` (`*(player2+0xB60)`). The character model was taken from the
  new player's `*(+0x10)+0x70` = `…2634FAA0`, **the same record as the real Talion's**.
- **`79f950` and `784dc0` both returned** ("reached stage 3 of 3"), no exception.
- The bot loop placed the second Talion ~1,170 times by 38.8 s, 0 bad packets.
- The game exited with code 0 at ~42 s, with no Windows application error event. Only the 17 s
  screenshot exists (loading). Backup check after: 0 changed. Evidence in
  `evidence\launch-32-secondtalion-anim` (save `c9770182…`). **All test switches set back to 0.**

### Unknown

- Whether he was animated, and why the session was so short: owner report pending.

### Owner report (launch 32)

- **"Same thing, Talion circling me in A pose."**

### Derived

- The animation component set-up (`79f950`, `784dc0`) is not enough. The CCharacterProxy update
  (`4274d0`) behaves differently when the proxy's local-player link (`proxy+0x118`, set by
  `418ad0` → `3f0080(clientShell+0x70, client, proxy, player)`) exists. The player's animation is
  probably driven from that client-side controller, which turns input and movement state into
  animation. Without it, nothing feeds the animation graph, so it stays at the rest pose. (Guess.)
- Binding that controller to the second Talion would tie it to the **local** client: it would
  likely react to the owner's own input, and the camera call follows in `418ad0`. For co-op the
  remote Talion's animation has to come from the **friend's game** anyway (animation sync). So the
  real next step is to capture the animation state on one side and replay it on the other, not to
  make the second Talion animate itself.

## 2026-11-21: animation probe (launches 33 and 34)

Question: which methods does the game call on the real Talion every frame that it never calls on
the second Talion (who stays in the A-pose)?

### Method

New guard switch `animprobe=1` (logging only, needs `secondtalion=1`). For the `CPlayerObj` vtable
(`exe+1460160`, 108 code slots), the animation component's vtable (`*(player+0xB60)`,
`exe+13EDF28`, 3 slots) and the animator embedded at component `+0x518` (`exe+13EDF78`, 3 slots),
every slot is replaced by a 64-byte stub:

```
mov rax,<slot record>; cmp rcx,[rax]; jne +16; lock inc qword [rax+16]
push rdx; mov rdx,[rsp+8]; mov [rax+40],rdx; pop rdx     ; last caller of a real call
cmp rcx,[rax+8]; jne +5; lock inc qword [rax+24]; jmp qword [rax+32]
```

It counts calls whose `this` is the real or the second object and jumps on to the original. Only
`rax` and flags change, and `rsp` is back at its entry value at the jump. Offline test: int, float
(xmm) and stack-argument methods return the same values through the stub, counts and callers are
right, and restore puts the original pointers back. Suite 38/38, negative control 14/14. Every slot
is restored after 120 s. Both runs: one copy plus the loopback bot moving the second Talion, owner
playing; save and settings checked against hashed backups before and after: 0 changed; exit 0;
all slots restored with 0 mismatches.

### Observed (launch 34, 120 s; launch 33 matched)

- Both `CPlayerObj`s report client id `0` (byte at `+0xB68`).
- Animation component and animator: **0 virtual calls** on either Talion. Their work goes
  through non-virtual code.
- `CPlayerObj` slots called on the real Talion only (count, last caller):

| Slot | Function | Calls | Called from |
|---|---|---|---|
| 29 | `be03c0` | 10,619 | `be2670` (component `vt+0xE8` → `7866e0`) |
| 40 | `bef6e0` | 11,654 | `bdaa60` |
| 93 | `be94d0` | 11,676 | `bde360` |
| 89 | `be0320` | 63,630 | `b50730` |
| 107 | `bddb30` | 53,903 | `b84b90` |
| 13 | `bf67c0` | 3,581 | `e53fd0` |
| 100 | `bd6450` | 325 | `bea5ff` |
| 41, 53, 88, 91, 98, 99 | | 1–66 | |

- Called on the second Talion only: slots 18 (`befc00`, 8 calls) and 87 (`be4c90`, 12).
- Slot 94 (`be05d0`, the per-frame `CPlayerObj` update) runs for both.

### Derived

- `be19d0(player)` returns `*(proxy+0x118)`, where `proxy = *(*(player+8)+0x40)` is the
  `CCharacterProxy`. That is the client player controller `418ad0` stores there.
- The per-frame update `be05d0` calls `bdaa60`, which does nothing when `be19d0` returns 0. That
  is where slot 40 (`bef6e0`) comes from. `bde360` calls slot 93 (`be94d0`, which reaches the
  animation component and `77b1c0`) and then the controller's update `3f9410(controller)`.
- So the second Talion skips the controller-driven part of the frame because his proxy has no
  controller. This confirms the launch 32 guess: animation comes from the 0xDA0 client controller
  (`3f0080`, one per local-player slot 0–3), not from the animation component on its own.

### Next

- Either give the second Talion a controller in an unused local-player slot (1–3) without input or
  camera, or replay the friend's animation state into the second Talion's animation component
  directly. The first needs the local-player entry layout (`dff0a0` list at client shell `+0x10`)
  and what `3fd990` binds per slot.

## 2026-11-21: a client controller for the second Talion (launches 35–41)

All runs: one copy plus the loopback bot moving the second Talion, owner playing. The real save and
settings were checked against hashed backups before and after every run: 0 changed. New guard
switches, all off unless set:

- `ctrlinfo` (logging only): dumps the local-player list, the controller slot table, both
  Talions' proxy and controller fields and the head of each animation component.
- `ctrlslot` (changes behaviour): gives the second Talion a client controller in local-player
  slot 1 (details below).
- `crashlog` (logging only): a vectored handler that logs the first 5 access violations in the
  exe with registers and an unwound call stack, and never handles them.
- `compdiff` (logging only): snapshots both animation components (0x4000 bytes) and both
  `CPlayerObj`s (0xE00 bytes) every 5 s for 30 s, and logs the 4-byte fields that changed in at
  least 3 intervals on one Talion only.
- `postspawn` (changes behaviour): the two argument-less engine calls `50aa00` makes after the
  animation set-up.
- `animprobe` now also covers the `CCharacterProxy` vtable.

Offline suite after these changes: **43 passed, 0 failed; negative control 14 of 14.** New
cases: `ctrl_dump`, `ctrl_slot` (create, the four refusals, an exception in the create call,
unlisting), `crash_log`, `comp_diff`, `post_spawn`.

### Observed

- **Local players (launch 35).** The list at client shell `+0x10` (`{u32 current; +0x08 begin;
  +0x10 end}`) holds **6 entries**; byte 0 of each is its index. Entry 0 is the owner's (pointers
  at `+0x58`/`+0x60`/`+0x68`); entries 1–5 are idle. The controller slot table at client shell
  `+0x70` (`{byte count; +0x08 four pointers}`) holds one controller, in slot 0. Both Talions have
  a `CCharacterProxy` (vtable `exe+13BDEA0`); only the real one's `+0x118` holds a controller.
- **Controller in slot 1 (launch 36).** `3f0080(clientShell+0x70, entry 1, proxy, player)`
  returned a controller (slot 1, device 1, client = the second proxy, character = the second
  player), and `proxy+0x118` was set; `418ad0`'s camera call was skipped. About 4 s later the game
  stopped with `0xC0000005` at `exe+50D034` (Windows error report). That is `50ce20`, the client
  game mode's camera pass: it walks every controller in the slot table and reads
  `*(controller+0x2D0)+0x338`; the new controller has no camera, and the engine's assert is a
  write to address 0. The slot table's count is also passed to the renderer as a view count.
- **Controller unlisted (launch 37).** Taking the controller back out of the table right after
  the create (slot 1 → 0, count 2 → 1, proxy link kept) avoided that crash. With it, the number of
  `CPlayerObj` methods called on the real Talion only dropped from 12–13 to 6; slot 40
  (`bef6e0`) and slot 29 now also run for the second Talion. Owner report: **still circling and
  rotating in the A-pose.**
- **Exit crash (launches 37–41).** Every run with `ctrlslot` ended with `0xC0000005` at
  `exe+D59E56`, a reference-count underflow assert in `d59db0`. `crashlog` stack (launch 38):
  `D59E56 ← F0FE75 ← F100FD ← thread start`; `F0FE75` is the CRT's `doexit`. So it happens in the
  destructors that run when the game quits, not during play.
- **Animation components (launch 38).** Over 30 s, **96 fields of the real Talion's animation
  component changed** (world transforms at `+0x65C`/`+0x678`, more transforms from `+0x3970`,
  `+0x39B0`, `+0x3B44`, timers); **none of the second Talion's changed**, not even his transform,
  although the bot moved him every frame. `CPlayerObj`: 18 fields real-only.
- **Registration (launch 39).** `784dc0` registers the component with `902140`: list link at
  `+0x08`, bucket key `*(u32*)(comp+0x18) & 0x3F`, engine handle at `+0x438`. Both components are
  listed together with the **same key** `9B0D1BB9` and the **same handle**. `comp+0x28` holds the
  behaviour file path as text (`…\player\player_self.bvr`), so the key is a hash of that path.
- **Post-spawn calls (launch 40).** `engine->vt[0x18]()` (`e00420`) → object vtable
  `exe+1482580`, `vt[0x50]` = `df3410`; `engine->vt[0xD0]()` (`e00650`) → object vtable
  `exe+146FE30`, `vt[0xD8]` = `d39dd0`. Both returned. The second component still changed 0
  fields.
- **Proxies (launch 41, ~20 s of probing).** Of the proxy's 21 vtable slots, 4 run for both
  proxies; only slot 13 (`vt+0x68`, `427940`) runs for the real proxy only, called from
  `exe+E54062`, the same caller as the real-only `CPlayerObj` slot 13 (`bf67c0`).

### Derived

- `e55000` walks the engine's in-process message queue (variable-size records: size `u16` at
  `+0x28`, payload at `+0x2A`). For each record, `e53fd0(record, dir)` calls `vt+0x68` on
  `*(object+0x38)` (the server object) for direction 0, or on `*(object+0x40)` (the client proxy)
  for direction 1. So slot 13 is each side's message handler.
- The proxy's handler `427940` passes the message to its controller's `+0x2D8` sub-component
  (`42f600`) and to `425060`.
- So the real Talion's client and server halves exchange messages every frame, in both
  directions, and the second Talion receives none. The `CPlayerObj` constructor subscribes a
  listener (`+0xAB8`, vtable `exe+1460060`) to the player's own animation component
  (`42e610(player+0xAB8, player, animComp+0x4D0)`); its update `bde360` is what reaches
  `be94d0` and the controller update `3f9410`. With no messages, the component is never updated,
  so that listener never fires.
- A controller alone does not animate the second Talion.

### Corrected

- Launch 39 first read `comp+0x48..+0x78` as "state words" (real: markers and hashes, second:
  zeros) and concluded the second graph was never started. Those bytes are the rest of the text
  buffer after the behaviour path. In launch 40 the real one held zeros and the second one held
  pointer-like bytes. They say nothing about the graph.

### Unknown

- Which message types carry animation, and whether the second Talion's objects accept messages
  addressed to them.
- Which object's reference count underflows at exit.

### Next

- Log the message types, sizes and rates the real Talion's two handlers receive (logging only).
- Then try replaying the real Talion's messages into the second Talion's objects. If the game
  animates him from them, the same messages, sent from the friend's game, are the co-op link.

## 2026-11-21: what the messages are (launches 42–43) — correction

New guard switch `msglog` (logging only, not together with `animprobe`): wraps slot 13
(`vt+0x68`) of the `CPlayerObj` vtable and the `CCharacterProxy` vtable, after checking the slots
still hold `bf67c0` and `427940`. For the real Talion's two objects it counts messages by type and
payload size and logs payload samples; it counts the second Talion's messages; after 60 s it
restores both slots. Offline suite: **44 passed, 0 failed; negative control 14 of 14** (new case
`msg_log`). Both runs: one copy plus the bot, owner playing, save and settings unchanged, both
slots restored, and **exit code 0** with the slot-1 controller switched off.

### Observed

- Arguments as `e53fd0` passes them: `msg[0]` points to an iterator whose first two qwords are a
  byte range `[begin, end)`; `*(u32*)(msg+8)` is 1 or 2.
- Launch 43, about 100 s: **1,104 messages in each direction** to the real Talion, 0 to the
  second. The payloads are **ASCII animation event keys**: `HeelStrike`, `ToeStrike`,
  `HeelLift`, `ToeLift`, `snd`, `ComplexAnim`, `CLFACE`, `HIDE_PIECE_KEY`,
  `RIGID_BODY_ANIMATE`, `UNSHEATH`, `MeleeF…`. Type 2 carried footstep and animation keys; type 1
  mostly `CLFACE` and similar.
- Without `ctrlslot` the game exits with code 0, so the exit crash of launches 37–41 came from
  the slot-1 controller.

### Corrected

- The previous section derived that the second Talion does not animate **because** he gets no
  messages, and proposed replaying the friend's messages to animate him. That reverses cause and
  effect: these messages are the events a **playing** animation fires (footsteps, sounds, prop
  keys). The second Talion gets none because his animation never plays. Replaying them could at
  most add footstep sounds.
- What stays true: his animation component is never updated (launch 38), the controller alone
  does not change that, and the real Talion's slot-13 calls are message delivery.

### Next

- Find the code that updates the real Talion's animation component, for example from the call
  stack where these event messages are produced for him.

## 2026-11-21: the second Talion animates, walks, and is driven over the link (launches 44–50)

All runs: one PC, save and settings checked against hashed backups before and after: 0 changed.
New guard switches (all off unless set): `dwatch` (logging only), `animtick`, `ctrlbind`,
`animparams`, `animcal` (logging only), plus the freeze detector inside `animparams` and packet
v2 in `netlink`. Offline suite after these changes: **50 passed, 0 failed; negative control 14 of
14** (new cases `dwatch`, `anim_tick`, `ctrl_bind`, `loco_params`, `anim_cal`,
`net_packet_v2`).

### What updates a player's animation (launch 44)

`dwatch` puts hardware write breakpoints (debug registers, set on all 66 game threads by a helper
thread, never handling the write) on the real Talion's animation component at `+0x65C` and
`+0x3B44`, and on the second Talion's `+0x65C`. In 20 s: 5,001 writes to the real component from
11 distinct stacks, all on the main thread; **0 writes to the second one**. Every stack shares:

```
engine frame E06835 -> 40C516 -> 3f0380 (loop over the 4 controller slots)
  -> 4009a0 (controller update) -> 428970(sub) -> 7a0940(component) -> writers
```

- `3f0380` runs `4009a0(controller)` for each controller in the slot table that passes
  `3f2ab0` and `411100`.
- `4009a0` calls `81b950(component+0x3968)`, then `428970(sub)` with `sub = *(controller+0x2D8)`:
  `{ sub+0x5E0 = 0; 7a0940(*(sub+0x3B8)); sub+0x5E0 = 1 }`. `7a0940` is the animation component
  update; `*(sub+0x3B8)` is the component.
- The second Talion's controller was taken out of the slot table (launch 37), so nothing ran
  these calls for him.

### The pieces, one run at a time

- **Launch 45, `animtick`** (`81b950` + `7a0940` on his component once per server frame, after
  checking that the real controller's `+0x2D8 → +0x3B8` is the real `player+0xB60`; the first
  exception stops it): 6,000+ ticks, no exception. His component now changed in 23 fields with
  the real one (was 1), including its world transform, but the pose blocks (`+0x3970…`,
  `+0x3A50…`) did not. Owner: **still A-pose.**
- **Why:** `7a0940 → 786740(component, *(component+0x4218))` builds the pose only if
  `component+0x8D8` (a pose output) is set: it updates each animation layer in the list at
  `+0x890` and hands the result to that output. `+0x8D8` is set by `42ead0(sub)`, the controller
  sub-component's bind (`sub+0x3B8 = component`, `component+0x8D8 = sub+0x28`, three listeners on
  the component), which the game calls lazily from the sub-component's update inside `4009a0`.
- **Launch 46, `ctrlslot` + `ctrlbind` + `animtick`** (bind called once, after the animation
  set-up, as the game orders it): bind linked (`component+0x8D8 = sub+0x28`), ticks ran. Owner:
  **he is now in an idle pose.**
- **Launch 47, `animparams`.** The controller update `3f9410` sets the graph's locomotion inputs
  by name: `785bc0(component, name)` → handle, `77b1c0(component, handle, value, 1)`; `77b250`
  reads one back. It sets `RawMoveMag` (stick magnitude 0..1 from the slot's input, so always 0
  for slot 1), `Game_SmoothedVelocity` and others. `animparams` sets them on his component before
  and after each update, from his speed (position change per tick, smoothed). Owner: **he
  walked** (screenshot: mid-stride next to the owner's Talion).
- **Launch 48, `animcal`** (the real Talion's speed, yaw rate and graph inputs every frame,
  average and maximum per second):
  - The player graph has `RawMoveMag`, `Game_SmoothedVelocity`, `DeltaYaw`, `CameraDeltaYaw`,
    `CameraYawMinusPlayerYaw` and `LastSpike`. It does not have `Game_Velocity`,
    `Game_VelocityX/Y/Z` or `Game_FallingTime`.
  - `Game_SmoothedVelocity` is the speed in units/s: run about 226 (measured 230), sprint about
    607 (measured 622).
  - `RawMoveMag` is 1.0 whenever a key is held (jog and sprint), 0 when still.
  - `CameraDeltaYaw` comes in steps of π/4, so these angles are radians.
  - So `RawMoveMag` = speed / 230, clamped.
  - The owner jumped from a tower: the second Talion's engine position followed
    (height 2712 → 1290 → 488, every packet placed), but he was then seen sliding in a frozen
    pose (see Unknown).
- **Launch 49:** the bot cycled walk 100 / jog 230 / sprint 650 units/s / stand. The inputs
  followed the phases; the freeze detector saw his pose change throughout except about 2 s at
  the start of each stand. Owner: **he cycles through the animations** (hard to judge while he
  circles and turns).
- **Launch 50, two copies on one PC with packet v2:**
  - Packet v2 (48 bytes, `SOMD`) adds the sender's own `RawMoveMag`, `DeltaYaw` and
    `Game_SmoothedVelocity`, read from its own graph; −1 in the first field means "not available
    yet". The checks: `RawMoveMag` 0..1 or −1, `|DeltaYaw|` ≤ 7, speed 0..5000, all finite. v1
    packets are still accepted.
  - The receiver uses fresh values (under 0.5 s old) as they are and otherwise derives them
    from position.
  - Both copies: controller, bind, tick and inputs all worked. 777 and 984 packets accepted, 0
    rejected. 51 of the parameter log lines say "from network"; the 14 "derived" ones come from
    the first seconds.
  - Owner, with the windows side by side: **the other copy's Talion moved like the one being
    steered.**

### Recipe

For a second `CPlayerObj` created with the object API:
1. run the animation set-up (`79f950`, `784dc0`);
2. create a client controller in local-player slot 1 (`3f0080`, entry 1) and link it at
   `proxy+0x118`;
3. take the controller out of the slot table again;
4. call `42ead0(*(controller+0x2D8))` once;
5. then, every frame: `81b950(component+0x3968)`, `7a0940(component)`, and set `RawMoveMag`,
   `DeltaYaw` and `Game_SmoothedVelocity` by name.

### Unknown

- Why he froze after the tower drop. A guess: a fall state in his graph that never gets a
  "landed" signal, because the full controller update and its movement component do not run for
  him.
- Which object's reference count underflows at exit while the slot-1 controller exists.

## 2026-11-21 to 2026-11-23: internet play, shared combat moves, shared enemies, v0.1.0 (launches 51–78, sessions with a friend)

From here the work moved into the mod's own repo,
[danudns/shadow-of-mordor-coop](https://github.com/danudns/shadow-of-mordor-coop) (MIT). Its
[docs/how-it-works.md](https://github.com/danudns/shadow-of-mordor-coop/blob/main/docs/how-it-works.md)
lists every message and game function in use. This section keeps the findings and corrections in run order.
"Lab" = the test copy (two copies on one PC where stated); "session" = owner and a friend, each on their own
PC and real install, over the internet.

### Internet transport and the mod kit (launch 51, first session)

- The game's `steam_api64.dll` exposes `SteamNetworking005`, `SteamMatchMaking009`, `SteamUser017` and
  `SteamFriends014`; the exe itself never uses `SteamNetworking`. The mod uses `ISteamNetworking005`
  peer-to-peer (relay allowed) and accepts only the partner's packets.
- Lobby: the host creates a friends-only lobby (`CreateLobby` slot 13, data via `SetLobbyData` slot 20) and
  invites through the Steam overlay. The friend's game is started by Steam with `+connect_lobby <id>`, joins
  (slot 14), and reads the host's SteamID from the lobby data. Callbacks used: 504 `LobbyEnter_t`,
  506 `LobbyChatUpdate_t`, 333 `GameLobbyJoinRequested_t`, registered with `SteamAPI_RegisterCallback`.
- Save transfer: the friend asks for the host's save on reliable channel 8 and receives a header (size and
  SHA-256) and 1024-byte chunks. The friend's game reads it from a separate co-op slot.
- **Observed (launch 51, one copy in "mod mode"):** lobby created 16.7 s after start; the session copy of the
  save was seeded from the real save (read only) and received 31 autosaves; real save unchanged.
- **First session (2026-11-21):** friend joined at 333 s, 1 MiB save sent in under 1 s, friend's Talion created
  on the first packet. Owner: "I can see my friend when he is walking."
- **Correction after the first session:** the owner reported that saves "reset". Cause: in that build *every*
  start (also alone) used the session copy and copied the settings into it. Changed: alone or hosting, the real
  save is used as without the mod and a dated backup is kept (newest 10); only an invited player is redirected,
  and Documents (settings, language) is never redirected.
- Installer: a `.ps1` started with "Run with PowerShell" closes at once under the default execution policy,
  before any of the script's own error handling runs. Ship `Install.cmd` / `Uninstall.cmd` launchers
  (`powershell -NoProfile -ExecutionPolicy Bypass -File …`).

### Attacks, sword, stealth (launches 52–65)

- **Slot mirroring is the wrong level (launch 53).** The 7 active graph-node slots (`comp+0x34A0`) change only
  with top-level modes. Activating the real Talion's node on the second one (`7933c0`) crashed in a graph node
  that needs a combat context.
- **Named animation requests (launches 54–56).** All attacks, counters, ground finishers and stealth kills go
  through the combat model `457120` → `79f150(comp, &request, 0)` ("play by request"). Request built with
  `77af10(&req, name)`; `7a16b0(comp)` ends the previous combat move. Replaying captured requests on the second
  Talion worked (launch 55, owner: "he attacked"), and over loopback between two copies every attack was
  replayed in order (launch 56).
- **Sword and graph switches (launches 57–61).** The sword flag is per item (weapon component `player+0x910`),
  set with `7bdc70`. The graph reads named bool switches set through `785bb0` (a 16-byte thunk into
  `785a70`): `SwordEquipped`, `InCombat`, `Sheathed`, `InStealth`, `CanHide`, `InPresentation`. Mirroring them
  is not enough for the sheathe: it is a clip request ("play by action", `79f9b0`) named `WeaponSheathe` /
  `WeaponSheathe_Low`. The draw is part of the attack animations.
- **Stealth (launches 62–63)** is switches only: `SwordEquipped` 0, `Daggers` 1, `Stealth` 1 in one frame.
  Mirrored, owner confirmed.
- **Feedback loop (launch 62):** with nothing positioning the second Talion, his own animation's root motion was
  fed back as derived walking input and he walked off. Fixed with a dead zone (< 25 units/s).
- **Corrections from the sessions:**
  - Replaying a remembered action pointer from `79f9b0` crashed inside the game (caught, but the component was
    left broken: the partner's Talion froze). Action pointers go stale. Now the mod keeps a 0x80-byte copy of the
    action and checks its asset name (`+0x60`) before replaying.
  - Paired moves (`…_Sync_…`, stealth kills) were never ended when no other attack followed: the partner stayed in
    the pose. Now ended after 3.5 s or when the partner moves again.
  - Position steps: sends were on a 50 ms timer over a 15.6 ms tick (~16/s). Now ~30/s on QPC, and the receiver
    renders 100 ms behind with blending.
  - "Only one of us sees the other": every message kind required a higher sequence number, so a restarted
    partner (starting at 1) was ignored; failed lobby enters were not retried. Fixed: after 2 s without position
    packets any number is accepted; lobby enters are retried.
  - "Now we do not see each other at all": the spawn record for the second Talion had 3 of the 6 qwords that
    `bee2b0` reads; the rest came from whatever followed in the DLL's data. Harmless in some builds, a crash in
    others. Now a zeroed 8-qword record with an empty handle `{0, -1}` and the last field 1.
- **Launch 65 (two copies, network):** movement smooth, attacks, sword and switches crossed both ways, 0 bad
  packets.

### Shared enemies (launches 66–78)

- **NPC table:** `aae540` registers each NPC in `exe+19835C0` (0x30-byte entries, `+0x00` NPC, `npc+0xE88` = its
  entry), capacity at `exe+1984C40`. 50–60 live NPCs from about 35 m to 243 m around the player. NPC objects are
  pooled (the same pointer comes back), so the network id is slot + generation. A killed orc leaves the table
  about 1 s later; dead bit `character+0x178 & 1`.
- **Types:** record name at `+0x20` (`Infantry_Orc_FacA` …), but spawning needs the *template* name
  (`Orc_Infantry`, `Orc_Archer`, `Warg_Brown`, `Resistance_Prisoner`).
- **Hiding is not suppression (launch 68):** `be1cc0(char, 1)` hides the model, but the outline stays and the AI
  keeps walking and attacking.
- **No own population on the joining side (launches 69–70):** the population spawn call at `exe+AE2B53` (into
  `b03640`) can be skipped; the spawner treats it as a failed spawn and retries slowly (25 skips at load, then
  about 5 per 30 s). The skipped calls' arguments are recorded per template and used to spawn puppets of that
  type on demand.
- **Phase 1 (launches 71–74):** the host streams the 40 nearest NPCs within 60 m of either player at 10/s; the
  joiner shows puppets (senses off) at the host's positions. Moving a dead puppet 1000 m down did not stick
  (navmesh snaps it back); removing it with `4b65f0` (object API remove) did.
- **Orc animations (launches 75, 78):** an NPC's animation component is `*(npc+0xC38)`, same class as the
  player's. Locomotion is clip-driven (`Idle`, `WalkForward`, `Run`, transitions); ambient loops are named
  requests. The host sends each NPC's `RawMoveMag` / `DeltaYaw` and its named animations; the joiner replays
  them on the puppets.
- **Stutter (launch 76):** the mod, not the game: every memory read was checked with `VirtualQuery`, strings byte
  by byte (up to 313 ms per frame with orcs near). A small cache of readable regions brought it to under 3 ms.

### Kills from the friend and joining next to the host (v0.1.0, untested in a session)

- The console's `Player.FragSelf` builds a damage record (type `Explode`) and delivers it through
  `4b7790(target, 0, &record)` → target `vt+0xB8`. The friend's kill of a puppet is sent to the host, which
  applies that record to the real NPC. In the first session try the kill failed (the first version looked for a
  player-style handle at `+0x420` that NPCs do not have); fixed, not yet seen in a session.
- The console teleport ends in `3f1050(player, float xf[7])`. The friend's Talion is moved 1.5 m beside the host
  when the first host position arrives more than 10 m away.
- A session check every 5 s compares, on both sides, where each Talion and the nearest enemies are shown, and logs
  the differences in metres.

### Released

- 2026-11-23: v0.1.0 published (source, offline tests, installer, Ghidra scripts; MIT). 70 offline tests pass.
  The README lists each feature with its test state.
- QA stays `none`: the checks are lab runs and two-player sessions reported by the author, not an automated
  in-game suite or reports from other players.
