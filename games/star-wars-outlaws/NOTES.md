# Star Wars Outlaws (2024) — session notes

## Identity
- Store: Ubisoft Connect. Main exe `Outlaws.exe`, SHA-256 371BE3E5657B6F46F8C9E2D331696BDE754DF16BFC23F7D5DCCACD9FE19B1D2F (528 MB). Build id: unknown.
- Engine: Snowdrop. Source: https://www.pcgameshardware.de/Star-Wars-Outlaws-Spiel-74306/News/DLSS-3-Ray-Tracing-und-Reflex-1442996/

## Observations
- No entry in DIRECTORY.md / data/directory.yaml, no `games/` folder, 0 issues matching "Outlaws" in the hub (2026-11-01).
- Web search found no co-op or multiplayer mod. Developers describe it as single-player only. Source: https://www.gamesradar.com/games/action/star-wars-outlaws-multiplayer-co-op
- A general Snowdrop mod loader exists on Nexus ("Universal Snowdrop Modloader"), script loading. Source: https://nexusmods.com/starwarsoutlaws/mods/24 (not downloaded, not read).
- `Outlaws.exe` has a `.code` section and is 528 MB, a pattern this playbook reports for Denuvo builds (methods.md, Code Vein).

## Derived / guesses (unverified)
- Snowdrop has engine-level netcode in other titles (The Division 1/2, Avatar: Frontiers of Pandora co-op). Whether Outlaws ships any of it is unknown.
- Denuvo on this build: unconfirmed.

## Unknowns
- Build id, whether replication code is linked in, save format, script layer the mod loader exposes.

## Triage verdict
- Path: build from zero (nothing to revive).

## Research pass (2026-11-01)
searched: web (several phrasings: "co-op mod", "multiplayer mod", github/nexus/reddit), Nexus Mods
starwarsoutlaws, GitHub topics, coop-anything issues. Not searched: Discord servers, non-English forums, Reddit directly.

### Observations
- No co-op or multiplayer project found for Star Wars Outlaws.
- Existing tooling (single-player): Universal Snowdrop Modloader (Nexus mods/24); Ultimate-SWO-ModLoader, an ASI
  loader, archived 2024-12-31 (https://github.com/patrickmollohan/Ultimate-SWO-ModLoader); SnowplowCLI, a Snowdrop
  data extractor, archived 2024-07-23 (https://github.com/CosmicDreamsOfCode/SnowplowCLI).
- Denuvo Anti-Tamper confirmed by the store DRM notice quoted on Steam
  (https://steamcommunity.com/app/2842040/discussions/0/4625854988055223265). VMProtect reported by a secondary source
  only. The EULA says circumventing the DRM ends the license: do not touch it; inject alongside, as for Shadow of War.
- Game data: `helix/sdf/pc`, about 70 GB.
- Strings in `Outlaws.exe` (grep -a, counts): `Coop` 178, `CoopLobby`, `CoopLobbyMaxSize`, `CoopLobbyPlayerCount`,
  `CoopReinforcementsManager`, `CoopTalentAbility`, `myTooCloseToCoopPlayerRadius`, `myExtraTicketsPerCoopPlayerInCombat`,
  `HC_ReplicatedVariablesNode`, `HC_ReplicatedVariablesScriptNode`, `HC_MultiplayerBarrierNode`,
  `HC_AutoTestMultiplayerBarrierNode`, `HC_MultiplayerSessionNode_*`, `MatchmakingClient_*`, `UPC_MultiplayerInvite`.
  No RakNet / Steam networking strings.

### Derived / guesses
- The co-op names match Snowdrop's shared gameplay framework used by The Division and Avatar: Frontiers of Pandora
  (which ships 2-player co-op). Guess: Outlaws links engine co-op/replication code it never uses. If it works, the
  route is enabling a dormant co-op lobby rather than writing sync from zero. Not verified.
- `HC_*` looks like visual-script node classes, so mission scripts may already have replication-aware nodes.

### Next steps
1. Copy the install to a test folder (needs ~600 MB for exes plus a link or copy of the 70 GB data; ask owner).
2. Check whether Denuvo moved the functions behind those strings (methods.md: test one decompile first).
3. Find xrefs to `CoopLobby` / `MatchmakingClient` and see if they lead to live code or stubs.
4. Compare with Avatar: Frontiers of Pandora's exe if the owner has it (same engine, working co-op).

## Static survey of Outlaws.exe (2026-11-01, read-only on a copy, same SHA-256)
Tools: Python 3.12.10, Temurin JDK 21.0.13, Ghidra 12.1.4 (SHA-256 ddac49f9...d4db, official NSA release).

### Observations
- Sections (sampled entropy): `.code` 0x1000, 89 MB, exec, 6.58 (normal compiler output); `.debug$P` 339 MB, exec,
  6.85 (guess: Denuvo VM); entry point is in the 0x61-byte `.shared` stub; `.data2` holds the strings.
  ASLR on, image base 0x140000000.
- The exception directory is file-backed (in the section named `.tls`) with 475,547 RUNTIME_FUNCTION entries, so
  function bounds come from the file without dumping memory (same as Shadow of War in methods.md).
- Each of 13 co-op strings has a RIP-relative code reference in `.code` (none through absolute pointers):
  - `CoopLobbyPlayerCount` + `CoopLobbyMaxSize`: both in a 119-byte function at RVA 0x3cec950.
  - `HC_MultiplayerBarrierNode`, `HC_ReplicatedVariablesScriptNode` (twice each): one 28 KB function at 0x4478d10
    (guess: script-node type registry).
  - `HC_AutoTestMultiplayerBarrierNode`: 13 KB function at 0x269a9a0. `HC_ReplicatedVariablesNode`: 0x3f7da70.
  - `CoopReinforcementsManager`: 0x50d70 (39 bytes). `MatchmakingClient_RequestMatchState`: 0x54d4480.
  - `myTooCloseToCoopPlayerRadius`: 0x3b1b290; `myExtraTicketsPerCoopPlayerInCombat`: 0x3b1d140.
  - `CoopTalentAbility` ref at 0x31a9e80 has no pdata entry (guess: leaf or Denuvo-moved code).

### Decompiled (Ghidra 12.1.4 headless, -noanalysis, single functions)
- 0x3cec950: writes a key/value block: `CoopLobbyPlayerCount` = this+0xC, `CoopLobbyMaxSize` = constant 4.
- 0x50d70: static initializer that registers the type name `CoopReinforcementsManager` (atexit-style).
- 0x54d4480: `MatchmakingClient` constructor; builds 10 named backend requests (`MatchmakingClient_*`), 60000 ms
  timeout. This talks to Ubisoft's online backend: off-limits for a mod (rule 10). Do not use it.

### Network stack present in the exe (observations)
- Imports: ws2_32 (WSASendTo, WSARecvFrom, getaddrinfo), iphlpapi, winhttp, uplay_r264 / upc_r2_loader64.
- A tunables table (one contiguous string run) holds a reliable-UDP transport and co-op session settings:
  `UDP_TimeoutMicros`, `UDP_KeepAliveIntervalmillis`, `UDP_ReliableRecentIDWindowSize`, `UDP_FrameRecentIDWindowSize`,
  `UDP_ResendTimeoutMultiplier`, `UDP_RTTEstimatorAlpha`, `UDP_RelayDiscoveryInterval`, `UDP_FragmentTTLmicros`,
  `FindMatchDelayBeforeHosting`, `CoopLoadingTimeoutMS`, `CoopHotjoinSyncTimeoutMS`, `GroupJoinBusyRetryIntervalMS`,
  `Voicechat_MaximumBackOffTime`, next to `LOGIN_Ubi*` settings.
- An enum string: `None, Landing, CustomGame, Quickplay, MapVoting, Briefing, HostMigration, EndGame`.
- Script node catalog includes a `Coop/` category: `Coop/Replicated Variables` (HC_ReplicatedVariablesNode),
  `Coop/Are Replicated Variables Initialized`, plus `Prefab/Player Rewinded Skeleton` (lag compensation).

### Derived
- Snowdrop's shared online framework (session states, host migration, hot-join, reliable UDP, replicated script
  variables) is compiled into Outlaws. Most likely inherited from The Division / Avatar. So "no netcode" (press
  claim) is wrong at the engine level.

### Unknown (the deciding questions)
- Whether Outlaws' own gameplay classes (player, NPC AI, missions, vehicles, speeder, ship) register replicated
  state, or only the engine base does. Unwired content means syncing by hand anyway.
- Whether a session can start without Ubisoft's backend (direct-IP / LAN path). Next: decompile the users of
  `CoopHotjoinSyncTimeoutMS`, `FindMatchDelayBeforeHosting` and the WSASendTo wrapper.
- Whether a copied install starts at all (Ubisoft Connect + Denuvo; Denuvo has daily activation limits).

## Session / transport pass (2026-11-01)

### Observations
- A complete co-op *mode* is defined in the exe, beyond engine leftovers:
  - Frontend nodes: `HC_FrontendNode_CoopHeistMode`, `_CanStartCoopMatch`, `_IsCoopSessionHost`, `_GetCoopMapsInfo`,
    `_GetUICoopContactsInfo`, `_ReturnToCoopFrontend`.
  - Game-state nodes: `HC_CoopGameStateIsCoopGameStartedNode`, `_TeamLivesControlNode`, `_TimerControlNode`,
    `_TriggerEndNode`; scoring (`HC_GetCoopTimeScore`, `_ScoreRank`, `_LivesScore`, `_FinalTimePlayed`).
  - Gameplay: `CoopReinforcementsManager` (waves, closets, fireteams), `HC_CoopNPCCheckpointManager`, co-op ping
    points, "coop sense" (shaders `UI/coopsense/*`), co-op taunts, `PlayerDiedInCoop`, `WaitingForRespawnCoop`,
    `Coop/Class And Character/*`, `QuestObjectiveCoopGoToLocation`, `HC_IsHostOrOfflineScriptNode`.
  - Data paths referenced: `game system data/juice/settings/coop{settings,mapsettings,charactersettings,alarmsettings,
    pingpointsettings,tauntsettings}.juice`, `game system data/fruit/coop/coopcharacter.fruit`.
  - Serializer at 0x2ec2580 reads `myCoopUISettings`, `myCoopDifficultySettings`, `myCoopHeistStates`,
    `myCoopScoringSettings`, `myDownedDamageMultiplier` from data.
- 0x3e8c980 registers 29 online tunables by index: LOGIN_* (Ubisoft auth), AutoPilotLocalPlayerTimeoutMS,
  FindMatchDelayBeforeHosting, 11 UDP_* transport values, AsyncRequestsDefaultTimeoutMS, GroupJoinBusyRetryIntervalMS,
  CoopLoadingTimeoutMS, CoopHotjoinSyncTimeoutMS, FirstPartyNotifyPremiumContentIntervalMS, Voicechat_*, NEWS_*.
- `multiplayer.coop_intent` is written (5-arg call) in two small functions, 0x3b448b2 and 0x3b5ae72.
- UDP layer: one `WSASendTo` site (loop fragment at 0x972230) and one `WSARecvFrom` site (0x968012): batched,
  non-blocking, per-packet queue with buffer objects behind a vtable; handles 0x2733 WSAEWOULDBLOCK,
  0x2746/0x2744/0x2738 on receive. `getaddrinfo` called from 7 sites.
- Static call tracing stops: no direct E8/E9 callers and no absolute pointers to these functions anywhere in the
  file. `.reloc` is only 0x120 bytes on an ASLR image, so pointer tables and vtables are built or relocated at
  runtime (guess: by the Denuvo loader). The exception table's entries are partly loop fragments (chained unwind);
  decompiling 0x96e460 failed for that reason.
- `helix/sdf/pc/data`: 1,236 `.sdfdata` packs plus `sdf.sdftoc` (65 MB, magic `WEST`, version 0x2a, compressed:
  no plain-text file names). `sdf.sdfver` lists 16 patch entries; the largest first field is 2631516 (guess: data
  changelist of the current build).

### Derived
- Massive built (or started) a co-op heist mode for Outlaws: matched lobbies up to 4, host / hot-join, team lives,
  reinforcement waves, scoring, own maps and characters. It never shipped. Guess: a cut mode, the code left in.
- The working route, if any, is to *re-enable the cut mode*, not to sync the open-world campaign. Campaign co-op
  would still be from-zero work on top.

### Blockers / unknowns
- Whether the co-op data (`coop*.juice`, co-op maps, `coopcharacter.fruit`) ships in the sdf packs. Needs an sdf
  TOC reader (SnowplowCLI targets an older Snowdrop; format v0x2a support unknown).
- Whether a session can be created with no Ubisoft backend. The session path likely goes through
  `MatchmakingClient` / `PartyClient` (Ubisoft services: off-limits); a local/LAN path is unknown.
- Everything else needs the game running (runtime memory for vtables). Running costs a Denuvo activation and uses
  the owner's Ubisoft account: ask first.

## Data pack pass (2026-11-01)
Reference: SnowplowCLI commit 6147909c41c362ce3e8d95e12b28a2bc86b06f4f (2023-09-21). The repo has no license of its
own (only its dependencies' licenses), so it was read for format notes only; no code copied. It supports Mario +
Rabbids (Snowdrop TOC versions up to about 0x17); Outlaws is version 0x2a.

### Observations (`helix/sdf/pc/data/sdf.sdftoc`, 65,154,061 bytes)
- Header: `WEST`, version 0x2a, u32 0x07963d5e (decompressed table size, about 127 MB), u32 0x03dbd987 (compressed
  table size), then more u32 fields, a `massive` / 32 bytes / `ubisoft` ID at 0x30, a `01 01` flag and high-entropy
  bytes from 0x62 (SnowplowCLI notes 0x140 unknown bytes after a non-zero flag).
- The table block is the last 0x03dbd987 bytes before a 48-byte end ID: offset 0x65256. First bytes
  `8613b12d420b9035`. Entropy of the first MB: 7.9998 bits/byte.
- Not zlib, raw deflate, LZMA, zstd (framed or magicless; zstandard 0.23.0) or LZ4 framed. Not an Oodle LZ block
  either: Oodle needs low nibble 0xC in byte 0, this is 0x6.
- The exe links Oodle statically (`oo2::OodleLZ_Decompress`, Kraken, Mermaid; no oo2core DLL in the install).

### Derived
- The file table is most likely encrypted, then compressed (Oodle). The bytes after the `01 01` flag are probably key
  material or a signature. So file names (and whether `coopsettings.juice` etc. ship) can't be read offline.

### Decision needed (stopped here, rule 6)
- Decrypting the table would mean pulling a key out of the exe. That's against the EULA and not obviously within this
  repo's rules. Not done; owner and maintainers to decide.
- Alternative that decrypts nothing: run the game and log the paths it opens through its own file layer (a hook in an
  injected DLL, the way the existing Snowdrop mod loaders work). That also answers whether the co-op data loads.

## Runtime file-log prep (2026-11-01)

### Observations
- Settings registry at 0x414e080 (19.6 KB): for each settings file it formats `"%s/%s"` (root + path), normalizes
  it (0x1410ad500), registers it in a table (0x144175f30, table at 0x149846cd8). It's an alphabetical list that includes
  all six `coop*.juice` files. Registration is unconditional, so it doesn't prove the files ship.
- The engine logs with `{file:%s}` tags: `Missing file {file:%s} included from file {file:%s}`,
  `File '%s' does not exist.`, `Failed to open file '%s' for read.`, `Could not open the file '%s' for data named
  '%s'.`
- `OutputDebugStringA` is called only from a Havok stack-tracer init (0x7c540). `OutputDebugStringW` has a jmp stub at
  0xac58 with no direct callers (pointer tables are filled in at runtime). Whether the retail log reaches
  OutputDebugString: unknown.
- No C compiler installed (no MSVC/vswhere, gcc, clang, zig, docker).

### Plan
- A (passive): `local/re/dbwin_listen.py` reads the DBWIN shared buffer (DebugView's protocol). Doesn't attach,
  inject or modify. Self-test captured a line. Owner launches the game normally; filter on `Outlaws`.
- B (only if A is silent): proxy DLL in a test copy that hooks the game's file-open path and logs names. Needs a
  compiler (ask), a copy of the exes plus a junction to `helix`, and possibly a Denuvo activation.

### Run 1: passive debug-output capture (2026-11-01)
- Owner launched the normal install from Ubisoft Connect (no copy, nothing injected). Capture 20:12:26 to 20:47:18,
  game running about 35 min.
- 264 Outlaws.exe lines, all from bundled third-party libraries: NVIDIA Streamline 2.7.11 (`sl.*`), Intel XeLL/XeSS,
  and one `UplayAPI [ERROR]` line. No Snowdrop engine log lines; no `coop`, `.juice`, `.fruit`, `Missing file` or
  `does not exist` matches.
- Result: the retail Snowdrop logger does not write to OutputDebugString. Plan A answers nothing; Plan B (hook
  inside a test copy) is the remaining runtime route.

## Hook DLL (built 2026-11-01, not run yet)
Toolchain: LLVM-MinGW 20261006 ucrt x86_64 (clang 23.1.3), zip SHA-256 317492c4...d17a (GitHub asset digest).

### Observations used
- `FUN_14012da70` (RVA 0x12da70, 37 bytes) is a varargs wrapper: `(string **out, const char *fmt, ...)` ->
  `vformat(*out, fmt, va_list)` at RVA 0xbfe20. It builds every settings path (`"%s/%s"`) and formats
  `Missing file {file:%s} included from file {file:%s}` and the `could not load fruit data` message.
- Engine string object: 16 bytes, byte 15 == 0 means inline text, else a heap pointer at +0.
- The juice include loader (0xb2f626) asks `FUN_140baa680` (merges include-path lists, then resolves through the
  global at 0x148a07be0) and reports "Missing file" when the result is null.
- Proxy candidates: the exe imports VERSION.dll, WINMM.dll, DINPUT8.dll (not KnownDLLs).

### Design
- `version.dll` proxy; forwards 17 exports to System32. Checks that the host is Outlaws.exe and all 37 wrapper bytes
  match, then writes a 14-byte absolute jmp to a C replacement that calls the game's own vformat and logs strings
  matching `.juice|.fruit|coop|heist|missing file|does not exist|could not load|failed to open`, each once.
  Optional patch delay via `coop_filelog_delay.txt`.
- Offline test: harness loads the proxy, the forwarded call works, and it refuses to patch a non-Outlaws host. The
  patch path itself is untested until the game runs.
- Risks: Denuvo integrity checks over `.code` may crash on the patch (then try a delay); the first start from a new
  folder may use a Denuvo activation; launching the exe directly may hand off to Ubisoft Connect.

### Run 2: hook DLL in the test copy (2026-11-01)
- `testcopy\Outlaws.exe` (pid 38428) loaded the proxy at 21:14:06.418; byte check passed; patch written at RVA
  0x12da70 (base 0x7FF7D9E90000). No crash at patch time.
- Ubisoft Connect `launcher_log.txt` 21:14:07: `Gamestarter must be initialized before game can be launched.`, then a
  play session for product 17903 and `Game with process id 39092 ... started`. `overlay_log.txt` shows pid 39092
  runs `<Ubisoft dir>\games\Star Wars Outlaws\Outlaws.exe`: the launcher restarted the *original* install. The owner
  played that (21:14 to 21:21); the patched process exited at once. Zero hook lines: no data, not a negative result.
- Product id 17903 = this Ubisoft Connect build.
- Lesson for the playbook (Ubisoft Connect games): starting the exe directly from another folder hands off to the
  launcher, which starts the registered install path. A test copy needs the launcher to start the copy itself, or
  the hook must survive the relaunch.

## External injector (built 2026-11-01, not run on the game yet)
- `injector.exe` polls every 20 ms for `Outlaws.exe` and injects only if its image is under the installed game
  folder, and only after `upc_r2_loader64.dll` is mapped (imports resolved). Then `LoadLibraryW(coop_filelog.dll)`
  via `CreateRemoteThread`. Injects once and exits. Install untouched; log written next to the DLL.
- `coop_filelog.dll` = filelog.c built with `-DCOOP_INJECTED` (no version.dll exports), plus a 30 s heartbeat line
  (wrapper call count, distinct matches) so "hook never fired" can be told apart from "nothing matched".
- Offline test: injected into a dummy `sleeper.exe`; DLL loaded and logged `host is sleeper.exe, not Outlaws.exe:
  nothing patched`.
- Limitation: injection lands after the imports load, maybe after some early static initializers. Paths formatted
  before that are missed.

### Run 3 attempt: injector blocked (2026-11-01)
- Windows Defender flagged `injector.exe` at 21:27:32 as `Behavior:Win32/DefenseEvasion.A!ml` (behaviour/ML
  detection of the CreateRemoteThread + LoadLibraryW pattern, set off by the offline test injection). Its next start
  failed with "Permission denied". The game never started; nothing was injected.
- Not worked around: evading antivirus is out of scope, and Defender settings are the owner's.
- Playbook lesson: on a stock Windows 11 PC, a self-built CreateRemoteThread injector trips Defender's behaviour
  monitor on first use. Prefer a proxy DLL the loader picks up.

### Run 4: proxy DLL in the real install, once, with owner approval (2026-11-01)
Deliberate exception to rule 2, approved by the owner in chat. The install's file list (30 files, name/size/mtime)
was snapshotted first. Afterwards the DLL and log were removed and the list compared: identical; Outlaws.exe hash
unchanged.
- Two processes loaded the proxy (pid 39224 at 22:27:51, then pid 35996 at 22:27:53, the Ubisoft Connect relaunch),
  both from the install, both patched cleanly (same base 0x7FF65DCD0000). No crash; owner loaded a save and played
  about 5 min.
- Heartbeat: 243,563 wrapper calls by 22:28:23, 3,152,010 by 22:32:53; 114 distinct matches, all in the first
  minute (22:28).
- By format string: 106 x `%s/%s` (the settings registry: every `helix/game system data/juice/...` path, 7 of them
  co-op or heist: `coopalarmsettings`, `coopcharactersettings`, `coopmapsettings`, `cooppingpointsettings`,
  `coopsettings`, `cooptauntsettings`, `heisttargetsettings`), 6 x `%s.%s` (type names: `HeistTarget.HeistTargetCarried`,
  `HeistTarget.HeistTargetEscorted`, `Item.EquipmentItem.CoopTalentAbilityEquipmentItem[.FocusFieldAbilityEquipmentItem]`,
  `Item.EquipmentItem.HeistTargetEquipmentItem`, `QuestObjective.QuestObjectiveCoopGoToLocation`), 1 x `%s%s.mshader`
  and 1 x `%s/%s%s`: `helix/shaders/UI/coopsense/coopsense_post.mshader/d256b2995352e79a81b.hlsl`.
- Zero `Missing file ...`, `could not load fruit data`, `does not exist` or `failed to open` strings formatted.

### Derived
- The 106 juice paths are the unconditional registry list (`demomodesettings.juice` is in it too), so their presence
  proves nothing about the files.
- No include-load error fired during startup and a save load. That rules out *included* juice files failing, but
  says nothing about whether the top-level co-op settings were ever requested.
- The co-op sense shader got a permutation/cache path with a content hash. Guess: its shader source/data ships.
  Moderate evidence, not proof.

### Next
- Read the settings registry at runtime (table at 0x149846cd8, filled by 0x144175f30) from the same DLL: for each
  co-op entry, is a loaded data object attached after the save loads? Needs the entry layout from 0x144175f30.

## Settings slots (static, 2026-11-01)
- Registry 0x414e080: per file, `index_global = counter++`, then format path, register (0x144175f30), and append a
  zeroed u64 to the slot array (pointer at RVA 0x9846ce8, count at 0x9846cf0). 106 files mapped by
  `local/re/settings_map.py`, e.g. coopsettings: index global 0x8a6d668, playersettings: 0x8a68630.
- Getters are lazy loaders: `if (!slots[idx]) slots[idx] = FUN_14254db80(ctx, path);` (0x2200470 playersettings,
  0x284c620 coopsettings).
- Code reads of each co-op index (including the registry write): coopsettings 10, coopcharactersettings 13,
  heisttargetsettings 7, cooptauntsettings 5, cooppingpointsettings 5, coopalarmsettings 2, coopmapsettings 2.
  playersettings: 380.
- 0x2c4ae40 reads coopsettings as (int key, int value) pairs at obj+0x40, count obj+0x48, default 100 when the key
  is missing. Guess: per-player-count scaling.
- Unknown: what FUN_14254db80 returns for a file not in the packs (null, or an empty default object).
- DLL now also dumps all 106 slots (read-only, via ReadProcessMemory) at 1, 2, 4 and 7 min; proxy SHA-256
  b271e3ce...ab79.

### Run 5: slot dump (2026-11-01, DLL in install once more with owner approval; restored and verified identical)
- Same two-process start (pid 39652 then 39600); both patched. About 11 min; 6.78 M wrapper calls.
- Slot array at 0x1f5b5321f00 holds exactly 106 slots (matches the registry). Loaded: 55 at 1 min, 78 at 2, 82 at 4,
  89 at 7 (save loaded and played).
- Co-op slots, identical in all four passes:
  - loaded: `coopcharactersettings` (vtable RVA 0x6599f78), `coopmapsettings` (0x6599f98),
    `cooppingpointsettings` (0x601f408, stable pointer at +0x40). `demomodesettings` (0x6599fb8) is loaded too.
  - empty: `coopsettings`, `cooptauntsettings`, `coopalarmsettings`, `heisttargetsettings`.
  - The +0x40/+0x48 reads for the 0x6599fxx objects change between passes and hold ASCII: those objects are
    smaller than 0x48 bytes, so the reads went past them. The values are meaningless.
- `FUN_14254db80(ctx, path)` (static): waits for ctx+0x50, then looks `path` up in a hash table of loaded settings
  (entries 0x18 bytes = 16-byte engine string + object pointer at +0x10; count at ctx+0x10; bucket heads after the
  entries); returns 0 if not found. It never creates a default.

### Derived
- **coopcharactersettings, coopmapsettings and cooppingpointsettings data ships with the game.** A non-null slot can
  only come from that table.
- The four empty slots were most likely never requested in single-player play: readers such as 0x2c4ae40
  dereference the result without a null check, so a request for a missing file would crash.
  Shipped or not: still unknown.

### Next
- Enumerate the loaded-settings hash table itself: ctx = *(*(RVA 0x97d20f0) + 0x6e8) (from 0x2c4ae40). That lists
  every settings file that shipped and settles coopsettings/heist directly.

### Run 6: settings database listing (2026-11-01, DLL in install a third time with owner approval; restored and verified identical)
- Settings DB ctx 0x1e5e7c6dc20: entries 0x1e5ed170000, count 106, ready 2. Same in both passes (1 and 2 min).
- The 106 names equal the registry's 106 paths exactly (set comparison), and **every entry has a non-null object**.
  The co-op/heist entries and their objects:
  `coopalarmsettings` (#28), `heisttargetsettings` (#87), `cooptauntsettings` (#94), `coopsettings` (#95),
  `cooppingpointsettings` (#96), `coopmapsettings` (#97), `coopcharactersettings` (#98).
- Conclusion (observed): **all seven co-op and heist settings files ship with the game and load at startup.** The
  empty slots in run 5 were lazy getters that single-player never calls.

## Where this leaves co-op (summary, 2026-11-01)
Observed:
- An unreleased co-op heist mode is in the exe: frontend (`CoopHeistMode`, `CanStartCoopMatch`, `IsCoopSessionHost`),
  game state (team lives, timer, scoring), reinforcement waves, ping points, co-op sense, co-op classes and
  characters, heist targets (carried, escorted), co-op quest objectives; lobby max size constant 4.
- Its settings data ships and loads (all 7 files); the co-op sense shader has a compiled-cache path.
- The engine's session layer is linked: reliable UDP transport (single WSASendTo/WSARecvFrom sites), hot-join and
  loading timeouts, host migration state, replicated-variable script nodes.
- Matchmaking and party go through Ubisoft services (MatchmakingClient, PartyClient): off-limits.
Unknown, the next questions in order:
1. Whether co-op *maps* ship (`myCoopMaps` entries in coopmapsettings point to level data; read the object).
2. Whether a session can be hosted and joined with no Ubisoft backend (direct-IP path under the session layer).
3. Whether the frontend can be reached (a script/flag that opens CoopHeistMode).
Not attempted: decrypting the data index (needs a key from the exe; EULA).
