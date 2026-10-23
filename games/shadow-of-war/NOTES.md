# Middle-earth: Shadow of War: agent notes

Raw notes, newest phase last. Paths use `<game dir>`. Goal: online shared-world co-op
of the main campaign (two players in one Mordor, one Nemesis world).

## Session 2026-10-20: triage and research (phases 0 and 1)

Nothing in this session touched a game install. Every fact below comes from web sources
or public GitHub metadata; nothing was run against the game.

### Game identity

| | |
|---|---|
| Title | Middle-earth: Shadow of War (Monolith Productions / WB Games, 2017) |
| Store | Steam, app 356190 ([api.steamcmd.net](https://api.steamcmd.net/v1/info/356190)) |
| Public build | buildid 3095001, last updated 2018-09-05 (same source). No update since. |
| Exe | `<game dir>\x64\ShadowOfWar.exe`, working dir `x64\` (Steam launch config, same source) |
| Exe SHA-256 | `ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef` (117,732,784 bytes; see session 2026-10-20 b) |
| File version | 1.0.8874.0, PE timestamp 2018-08-24 |
| Engine | Firebird, Monolith's LithTech-derived engine; Shadow of War is its first game. Monolith's site calls it "V7", Shadow of Mordor's engine "V6" ([Wikipedia: LithTech](https://en.wikipedia.org/wiki/LithTech), [blood-wiki](https://blood-wiki.org/index.php/LithTech)) |
| Archives | `.arch06` (file-extension sites only, unverified) |
| DRM | Denuvo at launch ([guru3d](https://www.guru3d.com/story/middle-earth-shadow-of-war-pc-drm-was-cracked-within-24-hours/)). Current Steam build: SteamStub confirmed, Denuvo very likely still present (see session 2026-10-20 b). |
| Other PC versions | Microsoft Store / Game Pass exists (SystematicSkid's save converter moves saves between it and Steam). Treat as a different build. |
| OS | Windows 11 (test machine); machine details stay out of the hub |
| Studio | Monolith closed 2025-02-25 (Wikipedia). No further patches expected: good for stable offsets. |

### Observations (checked directly, with sources)

Repo and tracker:
- This repo had no Shadow of War or Shadow of Mordor entry in `DIRECTORY.md`,
  `data/directory.yaml` or `games/` (grep, after `git pull` on 2026-10-20). Only LithTech
  hits are F.E.A.R. and NOLF2 (much older LithTech, not useful for Firebird internals).
- Open hub issues/PRs were **not** checked in this session (GitHub CLI not set up yet);
  checked before claiming the game (claim issue #93).

Official multiplayer (none in real time):
- No co-op or split screen; online features are asynchronous only: Online Vendettas,
  Online Conquest (attacking other players' fortresses), Social Conquest
  ([Twinfinite](https://twinfinite.net/?p=305996), Steam forum threads).
- July 2018 patch removed the Market, Gold and War Chests; follower cap 80, captain cap 85
  ([PCGamesN](https://pcgamesn.com/middle-earth-shadow-of-war/shadow-of-war-removes-microtransactions-loot-boxes),
  [TrueTrophies](https://www.truetrophies.com/n15548/huge-middleearth-shadow-of-war-patch-removes-microtransactions)).
- Shadow of Mordor's servers shut down 2021-01-12; Shadow of War's stayed up then
  ([Worthplaying](https://worthplaying.com/article/2021/1/16/news/123693)). 2024 Steam
  user says online still works; no official 2025/2026 notice found. Status today: unknown.

Existing co-op attempts:
- **JustAddMP/ShadowOfWar** ([repo](https://github.com/JustAddMP/ShadowOfWar)): created
  2026-10-17, one commit, README only, "Status: planned. Work hasn't started yet." Part of
  a GitHub org (created 2026-10-08) with 26 "planned" game repos and one shared framework,
  [JustAddMP/core](https://github.com/JustAddMP/core) (C++, launcher, injection, Steam
  lobbies, very active: v0.14.0 on 2026-10-20; no license file detected by the GitHub API).
  Their only game with real code linked from this hub is MafiaDE. Also has a
  `JustAddMP/ShadowOfMordor` placeholder. **Nothing to revive**, but someone may start
  soon: worth contacting to avoid duplicate work.
- No other co-op or multiplayer mod found for Shadow of War or Shadow of Mordor: web
  search (several phrasings), GitHub repository search for "shadow of war" (first 30
  hits read), Nexus (blocked automated fetch: 403), Steam forums via search results,
  2026-10-20. Reddit, ModDB, Discord and non-English searches **not** done yet.

Modding foundation (all closed binary hooking, no official mod support):
- **ReaperAnon/Middle-Earth-Mod-Loader** ([repo](https://github.com/ReaperAnon/Middle-Earth-Mod-Loader)):
  C++ DLL loader, proxies `bink2w64.dll` in `x64\` (original renamed `bink2w64_.dll`),
  loads `plugins\*.dll`, plugin API with ImGui menu (F10) and log (`` ` ``). Uses MinHook.
  Created 2023-10, last commit 2026-10-10. No license file. 23 stars, 6 forks.
- **ReaperAnon/Shadow-Of-War-Mods** ([repo](https://github.com/ReaperAnon/Shadow-Of-War-Mods)):
  14 plugins on that loader (cheats, skill tree, orc spawn rarity, Uruk intel, FOV).
  Last commit 2024-01-05. No license. Proves gameplay functions are already found and hooked.
- **survivalizeed/ShadowOfWarPacketLoader** ([repo](https://github.com/survivalizeed/ShadowOfWarPacketLoader)):
  resource loader ("hook_loading_routines", "designed_strings"), last commit 2025-01-28,
  no license. The Nexus overlay mod below needs "Survivalzeed's DLL mod loader".
- **survivalizeed/ShadowOfWarFPS** (MIT, 2024-12): FOV/first-person hook.
- **SystematicSkid** (same author, several repos):
  - [shadow-of-war-save-editor](https://github.com/SystematicSkid/shadow-of-war-save-editor):
    "legacy tools for debugging shadow of war systems", originally to reverse the save
    format; README says it helped the community understand the Nemesis system. 2024-01, no license.
  - [shadow-of-war-save-converter](https://github.com/SystematicSkid/shadow-of-war-save-converter):
    Python, MIT, 2024-11; Game Pass <-> Steam saves.
  - [shadow-of-war-server](https://github.com/SystematicSkid/shadow-of-war-server):
    rebuild of the online backend, 2025-03. Rust "Nemesis" server (Actix), C++ debugger
    hooking WinAPI to dump requests, "Hydra" value types, "AG Binary protocol". No license.
  - [sow-freecam](https://github.com/SystematicSkid/sow-freecam): 2025-06, shows a
    `sow::GameClient` class with `CameraOwner*` at offset 0x898.
- **Nexus mod 218** by GENETICZZ2K (via search result text, page itself 403): overlay with
  Uruk/captain editing, NPC spawning, fly mode. Spawning NPCs and editing captains at
  runtime is exactly what a co-op client needs to mirror the host.
- **Cheat Engine table** by reverser69, v1.1, Aug 2024 ([GamePressure](https://www.gamepressure.com/download/middle-earth-shadow-of-war-cheat-table-v11-mod/z914e87)).
- Steam users report that captain edits from tables can break orc behaviour, crash the
  game or corrupt saves ([Steam thread](https://steamcommunity.com/app/356190/discussions/0/1734342793779694605)).
- Other games on Firebird: none known. Shadow of Mordor runs on the previous engine (V6).

### Derived (reasoned from the above, not run)

- **Path: build from zero.** Nothing playable exists to revive. The JustAddMP repo is a
  placeholder; the useful prior art is the hooking ecosystem (mod loader, plugins, save
  and backend research), not a co-op mod.
- **Model: host authority with state sync.** The game has no real-time replication to
  piggyback on (the online backend is HTTP-style request/response with the Hydra
  backend, not peer gameplay traffic). The host runs the real simulation and Nemesis
  world; the client runs a mirrored copy where its own Nemesis/AI decisions are
  suppressed and remote actors are puppets.
- **Injection point is solved:** the `bink2w64.dll` proxy loader works on the Steam
  build, so we can ship the co-op layer as a plugin or our own proxy. Hooks only, no
  game files shipped: fits the hub rules.
- **Frozen build is an advantage:** one Steam build since 2018-09, studio closed, so
  offsets found by the existing mods should stay valid. Pin buildid 3095001 + exe hash.
- **Reuse candidates (license permitting):** JustAddMP/core (Steam lobbies/transport),
  the hub's own sync tools (`tools/`, coop-sync-testing skill), ReaperAnon's loader for
  menus/logging. None of the SoW repos have a license except the FPS mod and save
  converter (MIT), so we can learn from them but must ask authors before copying code.

### Biggest technical risks (ranked, my judgement)

1. **The Nemesis system is the world state.** Captains, ranks, traits, memories,
   promotions, betrayals and "time passes" events are generated by RNG and run off-screen
   (notably on player death). Two clients will diverge instantly unless the client's
   Nemesis simulation is fully disabled and every Nemesis event is replayed from the host.
   We don't yet know where Nemesis state lives in memory or which functions mutate it
   (SystematicSkid's save editor is the best lead). Also open design: what happens when
   the *client* dies (host-side Nemesis event, or no event).
2. **Save compatibility.** The host's save is the only canonical world. The client must
   never write its own save while connected (or only to a separate slot), or it would
   overwrite its single-player Nemesis world with a mirrored, half-puppeted one. Captain
   edits via memory are already known to corrupt saves (Steam reports). Mitigation: back
   up saves before every session, block client autosave, hash host save before/after
   sessions in tests. Steam vs Game Pass saves differ (converter exists).
3. **No second player character.** The game has one protagonist (Talion, plus Eltariel
   in the Blade of Galadriel DLC). The remote player needs a spawned humanoid pawn
   driven by network input, with combat, wraith abilities and domination. Spawning NPCs
   works (Nexus mod 218), but making one behave like the player is unknown territory.
4. **Global time effects.** Bow aiming slows time, executions/kill-cams, Elven Rage and
   cinematics probably change a global time scale or camera. In co-op one player's
   slow-motion must not slow the other's world. Unknown how the engine implements it.
5. **Scale of battles.** Fortress sieges run hundreds of AI. Full state sync may not fit;
   we may need host-authoritative events plus client-side AI for crowds.
6. **Story missions and cutscenes.** Scripted missions, cinematics and teleports to
   mission areas must be gated to the host and the client pulled along. Whether the game
   has a scripting VM (and where mission logic lives) is unknown.
7. **Online services and fairness.** The game talks to WB's backend for Online
   Conquest/Vendettas. A modded co-op session must run with online features off so we
   never send modded state to other players' worlds (hub rule 10).
8. **Denuvo (if still present).** Hooking already works for the existing mods, so it is
   probably not a blocker, but any anti-tamper must be left alone, not bypassed.

### Guesses (unverified, check before relying on them)

- Denuvo may have been removed from the Steam build at some point (one 2024 forum claim).
- The Firebird engine may still contain LithTech-era client/server networking code from
  F.E.A.R. 3 or Guardians of Middle-earth; if so it could be a replication backbone. No
  source found either way.
- The second player could reuse the Eltariel (DLC) or Celebrimbor character rig.

### Unknowns (to resolve in phase 2/3)

- Exe SHA-256, file version, Denuvo presence (needs a copied install).
- Whether the hub has open issues/PRs for this game (not checked yet).
- Where Nemesis state lives, how saves are serialised, whether a scripting layer exists.
- Whether Shadow of War's online servers still run in 2026.
- License status of every SoW mod repo except the two MIT ones.
- Whether JustAddMP intends to start on SoW soon.

### Not searched yet

Reddit (r/shadowofwar, r/ShadowOfMordor), ModDB, the Shadow of War modding Discord,
Nexus pages directly (403 for automated fetch), Russian/German/Polish searches,
Archive.org, Steam guides.

### Next steps

1. Set up `gh` so hub issues/PRs can be checked; open a "claim a game" issue.
2. Copy the Steam install to a test folder; record exe hash, version, Denuvo
   presence in `local/identity.md` and here.
3. Phase 3: build the mod loader + a logging plugin on the test copy; read
   SystematicSkid's save editor and freecam sources for class layouts; map the Nemesis
   structures and the save path.

## Session 2026-10-20 b: workspace (phase 2) and exe identity

### Observations (checked directly)

- Steam install: buildid 3095001 (appmanifest `buildid` and `TargetBuildID`),
  901 files, 117,711,336,590 bytes. Copied read-only with robocopy to an isolated test
  folder (`<workspace>\local\test-installs\sow-a`, git-ignored): 901 files, same byte
  total, 0 failures. The Steam install was only read.
- `x64\` holds `ShadowOfWar.exe`, `bink2w64.dll` (the mod loader's proxy target),
  `steam_api64.dll`, `oo2core_5_win64.dll` (Oodle), `amd_ags_x64.dll`, `AnselSDK64.dll`.
- `ShadowOfWar.exe`: 117,732,784 bytes, SHA-256
  `ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef`, FileVersion
  1.0.8874.0, x64, PE timestamp 1535072130 (2018-08-24, fits the 2018-09-05 Steam update).
- PE layout (read-only Python parse of the file, nothing executed): 17 sections. Entry
  point is in `.bind`, which is the SteamStub (Steam DRM wrapper) section. `.text` is 20
  bytes; the code sits in oddly named sections: `.arch` 30.9 MB, `.xtext` 8.5 MB,
  a read/write/execute section named `.bss` + garbage byte, 73.5 MB at entropy 7.09,
  `.srdata` 1.2 MB at entropy 7.83. No "denuvo", "vmprotect" or "themida" strings.

### Derived

- SteamStub is present: launching the copy outside Steam needs `steam_appid.txt` and
  `SteamAppId`/`SteamGameId` env vars (see playbook/methods.md), with Steam running.
- Denuvo is very likely still in this build: a 117 MB exe, a near-empty `.text`, and a
  73 MB high-entropy RWX section are the usual Denuvo layout, and no later patch removed
  it as far as research found. Not proven: no string marker, and we don't run anything
  to check. Consequences:
  - Static analysis (Ghidra) of the file on disk will see packed/obfuscated code;
    readable code exists only after the game unpacks itself at runtime. The existing
    mods (bink2w64 proxy + MinHook, patterns or offsets) work at runtime, so hooking is
    feasible without touching the protection.
  - Hub rule 10 / our rule: do not bypass, strip or patch Denuvo or SteamStub. Hook game
    functions from our own DLL inside our own process only.
  - Unknown: whether Denuvo limits several game copies at once on one PC (needed for
    two-instance tests) or counts activations per folder. Test with copy B later.

### Unknowns

- Save and settings locations (needed before the first launch of the copy, so the copy
  does not use the real saves).
- Whether the game runs two instances on one PC (mutex/window checks, Denuvo, Steam).

### Next steps

1. Find where saves and config live (Steam Cloud? `%USERPROFILE%\Documents\WB Games\...`?)
   from docs and file strings, and plan how to redirect the test copy before any launch.
2. Build ReaperAnon's mod loader + a logging plugin for the test copy only.

## Session 2026-10-20 c: where saves and settings live, isolation plan

### Observations (read-only listing of the real save and Steam folders, names and sizes only)

- `%LOCALAPPDATA%\WB Games\Shadow of War\` holds everything local:
  - saves: `<8 hex>_ShadowOfWar.sav` plus `<8 hex>_1..5_ShadowOfWar.sav`, ~2.3 MB each
    (the 8-hex prefix is per account, unknown how derived);
  - `render.cfg`: plain text `"Key" "float"` pairs (resolution, `Windowed`, quality presets);
  - `gpu.nfo`;
  - `CloudStorage\`: WB online content cache (`socialcmt*`, `freetrial*`, `mktg-*`
    files), i.e. the game downloads event/marketing data from WB's servers.
- Steam Cloud: `Steam\userdata\<steam id>\356190\remote\<8 hex>_ShadowOfWar.sav`, same
  size as the main local save. Matches player reports that Steam Cloud restores saves
  you delete locally ([Steam thread](https://steamcommunity.com/app/356190/discussions/0/4699035289200469903)).
- Older builds saved to `Documents\WB Games\Shadow of War`
  ([Microsoft Q&A](https://learn.microsoft.com/en-us/answers/questions/5614199/not-saving-data-or-save-files-for-middle-earth-sha));
  that folder does not exist on the test machine.
- No `WB Games` key in HKCU, HKLM or WOW6432Node registry.
- Exe imports (read-only parse; Denuvo leaves a small static import table, most APIs are
  resolved at runtime): `shell32!SHGetKnownFolderPath` statically. Strings in the exe:
  `SHGetFolderPathW`, `getenv`, `\WB Games\`, `\USER\render.cfg`, `render.cfg`,
  `ShadowOfWar.sav`, `STEAMREMOTESTORAGE_INTERFACE_VERSION014`, `CreateMutexA/W`,
  `CreateMutex failed`, UTF-16 `SOFTWARE\WB Games\Kraken\1.00.0000`. Imports `winhttp`
  and `wininet` (online services).
- PCGamingWiki page: 403 for automated fetch, not read.

### Derived

- Saves reach disk two ways: plain files under the LocalAppData known folder, and
  Steam Cloud through `ISteamRemoteStorage` (v014). Redirecting the folder alone would
  still let a test copy overwrite the real cloud save, which Steam then pulls back
  into the real install.
- `SetEnvironmentVariable LOCALAPPDATA` will not redirect `SHGetKnownFolderPath`
  (it reads the user profile, not the environment). Redirection has to happen in
  process, from our DLL.
- `CreateMutex failed` suggests a single-instance mutex (matters for two-copy tests).
- `\USER\render.cfg` may be an alternate (portable?) config path. Unknown.

### Isolation plan (nothing launched until every step passes)

1. **Backup with a hash manifest** of `%LOCALAPPDATA%\WB Games\Shadow of War` and
   `userdata\<id>\356190` into `local/backups/<date>/` (read from the real folders,
   write only into the workspace).
2. **Manual step in the Steam client:** turn off Steam Cloud for Shadow of War
   (game Properties > General) for the testing period. Belt and braces with step 3.
3. **Never launch the copy vanilla.** First launch only with our own proxy
   `bink2w64.dll` in the copy's `x64\` (original renamed to `bink2w64_.dll`, same trick
   as ReaperAnon's loader). Inside the game process only, it:
   - hooks `SHGetKnownFolderPath` / `SHGetFolderPathW` so LocalAppData (and Documents)
     resolve to `local\test-installs\sow-a-profile\` for this process;
   - makes `ISteamRemoteStorage` writes and deletes no-ops (logged) and reports cloud
     disabled, so nothing reaches the real cloud slot;
   - blocks `winhttp`/`wininet` connections (offline mode; no modded state to WB's
     servers, hub rule 10);
   - logs every file opened for writing outside the copy and its profile folder, and
     **terminates the game** if one targets the real save folder or Steam userdata
     (fail closed);
   - writes its own log to `local\test-installs\sow-a-profile\coop-log.txt`.
4. **Seed the sandbox profile** with a `render.cfg` set to windowed, 1280x720, low
   preset, and no save (new game), or a copy of one real save if needed later.
5. **Launch from the copy**, not through the Steam library: `steam_appid.txt` (356190)
   next to the exe, `SteamAppId`/`SteamGameId` env vars, Steam running. Kill only
   processes whose path is inside `local\test-installs\`.
6. **After every run:** re-hash the real folders against the manifest (the backup
   check); any difference = stop and report. Also check the real `x64\` is unchanged.
7. Before building step 3, test the redirect DLL on a harmless target first if
   possible; unknown whether SteamStub/Denuvo tolerate the proxy before ReaperAnon's
   loader proves it on this build (it is documented to work on the Steam version).

### Open questions

- Does the game also read `getenv("LOCALAPPDATA")` or `USERPROFILE` anywhere? Hook
  logging on first run will show it.
- Where controls/audio/gameplay settings live (not in `render.cfg`; maybe in the save).
- Does the WB online layer need to answer for the game to reach the main menu offline?

## Session 2026-10-20 d: isolation steps 1 and 2 done

- Step 1 (backup): `%LOCALAPPDATA%\WB Games\Shadow of War` (28 files) and
  `userdata\<id>\356190` (2 files) copied into `local/backups/2026-10-20/`, with
  `MANIFEST.sha256` (SHA-256, size, path). Every backup file was hashed against its
  original: 30 files, 0 mismatches.
- Observation: the main save, save slot 1 and the Steam Cloud save have the same SHA-256,
  so the unnumbered `.sav` looks like the current/last-used slot mirrored to the cloud.
- Step 2 (Steam Cloud off): done in the Steam client. Checked read-only: Steam's
  `userdata\<id>\7\remote\sharedconfig.vdf` has `"356190" { "cloudenabled" "0" }`.
- Next: step 3, the redirect/guard DLL. Nothing launched yet.

## Session 2026-10-20 e: guard DLL built and tested offline (isolation step 3)

Code lives in its own repo (`sow-coop`, local only for now, not pushed); this hub gets
the write-up only.

### Built

- `sowguard`, a `bink2w64.dll` proxy: 73 exports forwarded to the original renamed
  `bink2w64_.dll` (same names and ordinals, via `/export:Name=bink2w64_.Name` linker
  pragmas; a `.def` file with `Name=bink2w64_.Name` lines failed to link here with
  LNK2001 for every export). MSVC 14.44, `/W4 /WX`, static CRT.
- Hooks six ntdll functions in-process: `NtCreateFile`, `NtOpenFile`,
  `NtQueryAttributesFile`, `NtQueryFullAttributesFile`, `NtSetInformationFile`
  (rename/link targets), `NtDeleteFile`. Observation on the test machine (Windows 11 26200):
  all six are plain 24-byte syscall stubs
  (`4C 8B D1 B8 <id> F6 04 25 08 03 FE 7F 01 75 03 0F 05 C3 CD 2E C3`), so a byte copy is
  a working trampoline; no hooking library or download needed. Any other layout stops
  the game (exit `0xC0DE0005`) instead of guessing.
- Redirects `%LOCALAPPDATA%\WB Games`, `Documents\WB Games` and
  `Steam\userdata\<any id>\356190` to a sandbox profile; protects those plus the real
  install (write = terminate, `0xC0DE0010`); logs writes elsewhere in Steam userdata;
  denies `\Device\Afd` (sockets) when `block_network=1`.
- Start-up refusals: exe outside `allowed_root` or inside the Steam folder
  (`0xC0DE0002`), exe not size 117,732,784 + the recorded SHA-256 (`0xC0DE0003`),
  `steam_appid.txt` not `356190` (`0xC0DE0004`); sets `SteamAppId`/`SteamGameId`.
  The real paths come from the environment and registry at runtime, never hard-coded.

### Tested (offline, no game process)

- Suite `test_guard.exe run`: **RESULT: PASS (20 of 20 cases passed)**, 2026-10-20.
  Each case builds a fake user tree, loads the DLL into a child process in test mode
  (real user paths refused) and checks disk state + exit code. Covers: redirected
  write/read/attributes/listing; forward slashes, lower case, `\?\`, cwd-relative and
  directory-handle-relative `NtCreateFile`; rename/delete/mkdir in the profile; Steam
  userdata redirect; install read allowed; install write, delete, create and
  rename-into all terminate with `0xC0DE0010` and leave the tree unchanged; sockets
  denied/allowed by flag; the four start-up refusals; all 73 forwarders resolve to the
  original; own SHA-256 matches the exe hash above.
- First run had 19/20: the forwarder test looked for `bink2w64_.dll` before resolving any
  export. Forwarders load their target on first resolution (in the game, the exe's
  static `BinkGoto` import does that). Test fixed, not the DLL.
- Negative control `test_guard.exe mutant` (redirect and guard compiled out):
  **5 of 5 dependent cases fail as they should**.
- Dry run of the staged game-mode config inside the test copy with a non-game exe: the
  DLL parsed the config and refused with `0xC0DE0003 unknown exe`.
- Real saves and Steam userdata re-checked against the backup manifest after all of
  this: 30/30 unchanged.

### Observed in passing

- The real install has `x64\steam_appid.txt` (`356190`), created 13:59 on
  2026-10-20, about a minute before Steam's `remotecache.vdf` for the game was last
  written, and before this session first looked at the install. Most likely the game
  writes it itself at launch (guess, unconfirmed). The test copy inherited it.

### Staged (not launched)

- Test copy A `x64\`: guard as `bink2w64.dll`, original as `bink2w64_.dll` (hash checked
  equal to the real install's), `sowguard.ini` (game mode, `block_network=1`, backups folder
  also protected). Sandbox profile `render.cfg`: `Windowed 1`, 1280x720, qualities 0,
  60 fps cap (value meanings partly guessed).
- Launch script written but not run: runs the backup check, checks Steam is running and
  no game is running; starts the copy's exe directly; kills only test-copy processes on
  timeout; prints exit code, guard log and the backup check.

### Unknown until the first launch

- Whether Denuvo/SteamStub accept ntdll hooks made before the game's entry point.
- Whether SteamStub relaunches through Steam despite `steam_appid.txt`. If it did,
  Steam would start the real, unguarded install: watch for that in the
  launch output (the script lists every ShadowOfWar.exe still alive).
- Whether the game reaches the main menu with sockets denied, and whether it needs
  WB's servers.

## Session 2026-10-20 f: first guarded launch of the test copy

Launched with `launch-sow-a.ps1` (Steam running, backup check passed first), timeout 300 s.

### Observations

- Only one game process, from the test copy; nothing started from the real install
  (checked 2 s in and at the end). SteamStub did **not** relaunch through Steam.
- Guard accepted the exe (size + SHA-256), installed all six ntdll hooks before the game's
  entry point, and the game ran with them: Denuvo/SteamStub tolerated the hooks this run.
- Game exited by itself with code 0 after 139 s. No crash dump, no WER report. Why it
  exited is unknown (nobody watched the window; possibly the network denial).
- Guard log, 71 lines: 20 socket attempts denied (`\Device\Afd\Endpoint`), 32 redirects,
  0 protected-path hits, 0 watched writes, no FATAL.
- Order seen: sockets denied and Steam userdata `<id>\356190\175823492` probed during the
  first ~1.4 s; then a ~68 s gap with nothing logged; then the game's own boot: probes
  `LocalAppData\WB Games\Shadow of War\` and `Documents\WB Games\Shadow of War\`,
  creates a `{25FFEBFB-B407-4789-ABEB-43655FEB78C6}` file (write test?), opens
  `render.lasterror`, creates `CloudStorage\`, reads and rewrites `render.cfg` several
  times, writes `gpu.nfo`, writes Steam userdata `356190\175823492` (1,178 bytes). All
  of it landed in the sandbox profile.
- The game opened a window: it saved `WindowWidth 960`, `WindowHeight 516`,
  `ScreenX 480`, `ScreenY 258` into the sandbox `render.cfg`.
- **Backup check failed on one file:** the real `render.cfg` was rewritten at
  17:14:04.53, 3 s after launch, in a format the game doesn't use (`"1080"` instead of
  `"1080.000000"`, trailing blank line), with `Windowed 0` (was 2, borderless),
  `TextureQuality 3` (was 2). Saves, gpu.nfo and Steam userdata unchanged.
- Cause: the **NVIDIA App** (separate process). Its backend refreshed
  `NvBackend\Recommendations\middle_earth_shadow_of_war\*` at 17:14:03.42-04.08 and
  `ApplicationState\928249434.json` at 17:14:04.65. Its `current_game.lua` for this game
  has `GetConfigFilePath`, `ScreenWidth16by9`, `Render.Setting.TextureQuality` and the
  Display Mode values (Full-screen 0, Windowed 1, Borderless 2); its state JSON records
  "Full-screen" and "1920x1080". It writes the real LocalAppData path whenever it sees
  the game start, from outside the game process, so the guard cannot see or stop it.

### Derived

- The in-process guard works as designed on the real game; the gap is other processes
  that react to the game starting. NVIDIA App "optimize" (or auto-apply) is one; others
  could exist on other PCs (AMD Adrenalin, overlays). Add to the playbook as a QA pitfall
  for any "isolated test copy" setup.
- The ~68 s pause is probably Steam/Denuvo or WB online code timing out on denied
  sockets (guess; unverified).

### Next

- Options: restore `render.cfg` from the backup; stop the NVIDIA App from optimizing
  Shadow of War (or close it) before test launches.
- Launch script: also snapshot NVIDIA App's recommendation folder and name the writer
  when the backup check fails; watch the window to learn why the game quits after ~139 s.

### Seen on screen during the same launch (added after the run)

- With sockets denied, the game showed an error/bug-report dialog about not reaching
  WB's server. Getting past it took pasting the report text into WB's support website.
  That explains the ~68 s gap in the guard log. The game was then closed by hand from
  the main menu, which explains the clean exit 0 at 139 s.
- The menu looked as if it recognised existing save data.

### Checked against that

- The guard log has no access to any `.sav` path (a read through the real save
  folder would have been redirected and logged), and the sandbox has no `.sav` files.
  The six real saves and Steam userdata are byte-identical to the backup. So the game
  did not open the save files in this process. Unknown what the menu showed and where
  it came from (Steam stats or achievements over Steam's IPC are a guess).
- `steam-userdata\356190\175823492` is a hex-text blob (1,178 bytes) that differs from
  the real copy in 1,061 bytes: regenerated, not copied. Not a save.

### Consequences

- Denying socket creation looks like a bug to the game, not like being offline. Every
  launch would stop at that dialog, which rules out unattended bot tests, and it
  invites reports to WB about modded runs (contacting anyone needs approval under the
  hub rules anyway). Next guard version: allow sockets, fail outbound
  connects and DNS the way an offline PC does, and see whether the game then starts
  offline without the dialog. Unknown whether it needs WB's server at all to start.
- Still open: whether Steam serves `ISteamRemoteStorage` file reads/writes from inside
  the game process (where the guard redirects them; the in-process write of
  `175823492` suggests yes) or from `steam.exe` (where it can't).
- Decided: keep the NVIDIA-rewritten `render.cfg` (display settings get fixed in the
  game). The backup check now reports `render.cfg` and `gpu.nfo` changes as info only;
  any change to saves or Steam userdata still fails it and blocks a launch.

## Session 2026-10-20 g: offline-style network block (guard v2)

Why: denying socket creation (v1) made the game show a "can't reach server" bug-report
dialog that needed a human. Goal: look like an unplugged PC instead.

### Observations (measured on the test machine, Windows 11 26200)

- Probe (`test_guard.exe probe`, guard with `net_trace=1`, `network=allow`) traced the
  AFD requests behind each Winsock call; unroutable documentation addresses
  (192.0.2.1, 2001:db8::1) and loopback only:
  - `0x12007` connect (TCP, and UDP connect): sockaddr at offset 24 (length 40 for IPv4,
    52 for IPv6);
  - `0x120C7` ConnectEx: 1-entry TDI header, sockaddr at offset 10 (length 26 / 38);
  - `0x12023` UDP sendto: pointer to a sockaddr at offset 96, its length at 88 (len 104);
  - UDP `connect`+`send`: address only on the connect, the send is `0x1201F`.
  - `NtDeviceIoControlFile` is a plain syscall stub like the six file functions.
- Guard v2 hooks it: with `network=offline`, those three requests to a non-loopback (or
  unparseable) destination return `STATUS_NETWORK_UNREACHABLE` without reaching the
  driver. Winsock then reports `WSAENETUNREACH` (10051); WinHTTP reports
  `ERROR_WINHTTP_CANNOT_CONNECT` (12029) after 0 ms. Loopback passes.
- Suite: **RESULT: PASS (21 of 21 cases passed)**. New case `network_offline`: sockets
  create; TCP connect, ConnectEx, UDP sendto, UDP connect+send to 192.0.2.1 and TCP,
  ConnectEx, sendto to 2001:db8::1 all give 10051; WinHTTP to 192.0.2.1 gives 12029 in
  under 2 s; TCP and ConnectEx to a 127.0.0.1 listener, TCP to a ::1 listener and a UDP
  datagram over 127.0.0.1 all work. Mutant build: **6 of 6** dependent cases caught.
- One false failure on the way: once the guard was staged in the test copy,
  `sow-a\x64\bink2w64.dll` *is* the guard, and passing it as the "original" made the
  proxy forward to a second copy of itself. That copy found the stubs already hooked
  and exited `0xC0DE0005` (fail closed), but could not log because the first copy holds
  the log file. Fixed the test input (`bink2w64_.dll`) and made the harness refuse a
  sowguard build as the original. Lesson for the playbook: a proxy loaded twice fails
  silently if its log is opened without write sharing.

### Derived / unknown

- DNS still resolves (the DNS Client service does it outside the game process), so the
  game sees "name resolves, connection fails", like a PC whose network drops after DNS
  or a firewall. Unknown whether the game still shows its dialog in that case. If it
  does, next try: also fail name resolution.
- Staged in test copy A with `network=offline`; dry run with a non-game exe refused
  (`0xC0DE0003`); backup check unchanged (render.cfg info only). Not launched.

## Session 2026-10-20 h: second guarded launch (guard v2, network=offline)

NVIDIA App window closed before the launch; its `nvcontainer` services and `NVIDIA
Overlay` processes were still running. The game window was watched by hand.

### Observations

- One game process, from the test copy. All seven hooks installed; game ran 67 s,
  exit 0 (closed by hand from the main menu, see below).
- Guard log, 48 lines: 24 redirects, 4 offline refusals, 0 protected hits, 0 watched
  writes, no unparsed network request, no FATAL.
- Timeline: +0.7 s Steam userdata `356190\175823492` opened (read, redirected); +3.5 s
  the game's own boot starts (save-dir probes, write test file, `render.lasterror`,
  `CloudStorage\`); +3.8-8.3 s `render.cfg` read/written, `gpu.nfo` read; +46.5 s
  `render.cfg` written again; **+46.7 s two HTTPS connects refused, to
  3.213.33.214:443 and 184.193.6.155:443, both via ConnectEx; retried once at +61.9 s,
  refused again.** No other outbound attempt.
- Compared with v1: no 68 s stall before the game's own boot (v1: +1.4 s -> +69 s with
  sockets denied; v2: boot at +3.5 s). In v1 the early socket creations at +0.5-1.4 s
  were denied; in v2 they were allowed and none of them connected anywhere remote.
- The real `render.cfg` was **not** rewritten this time (same hash, still 17:14:04).
  The NVIDIA backend wrote only `ApplicationStorage.json` (17:49:53, at game exit) and
  did not refresh the Shadow of War recommendations. Unknown whether that is because
  the App window was closed or because it had already applied its settings at 17:14.
- Backup check: unchanged (render.cfg info only).

### Derived

- The online calls happen late (about 43 s after boot), probably when the main menu or
  its online features load; two hosts, HTTPS, one retry 15 s later. 3.213.33.214 is in
  an Amazon AWS range (3.0.0.0/8); who runs the second host is unknown. Hostnames
  unknown (DNS happens outside the process; not looked up, to avoid more traffic).
- Seen on screen: **no error dialog**; the game reached the main menu and was closed
  from there by hand (the exit at 67 s). So "connections fail as on an unplugged PC" is enough for the
  game to start and reach the menu offline; failing DNS is not needed. The two HTTPS
  attempts at +46.7 s and +61.9 s happen while the main menu is up and are silently
  given up on.
- Phase 2 isolation is done: test copy A starts unattended to the main menu with saves,
  settings and Steam userdata sandboxed, the network offline, and the real files
  checked before and after every launch.

## Session 2026-10-20 i: phase 3 plan (find player and Nemesis data at runtime)

### What prior work gives us (read from public source, 2026-10-20; no licenses stated,
so we learn from it and do not copy code)

- **SystematicSkid/shadow-of-war-save-editor** (`src/main.cpp`, 2024-01, written for the
  same Steam build as far as we know: the game has not been patched since 2018-09):
  - Calls the game's own developer commands, each `void(int argc, const char** argv)`,
    by RVA: `0x1C76D08` dump factions to JSON (`factions.json`), `0x1C76784` dump one
    faction member (`faction_member_single_save.dat`), `0x1C7709C` dump all (`all.dat`),
    `0x1C77070` load a save file, `0x1C762AC` dump a faction profile, `0x1B7663C` quest
    data, `0x1BECB58` inventory, `0x1C920D4` add follower.
  - Internally the Nemesis system is **"Faction" / "FactionMember"**: debug switches
    `NemesisForge.Debug`, `Faction.Debug.ZonePower`, `.Tribe`, `.TraitCatalyst`,
    `.Reward`, `.BundleMgr`, `FactionMember.Debug.Resurrection`, as bools at RVAs
    `0x2A34DA3..0x2A34DB8`.
  - A **script-variable registry**: a linked list found via pattern
    `48 89 05 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 05 ? ? ? ? 48 89 05 ? ? ? ? 48 89 05 ? ? ? ? 48 89 05 ? ? ? ? E8`;
    each var has `name` (+0x08), `description` (+0x10), source `file` (+0x18), `line`
    (+0x20), bool value at +0x38; node is at var+0x28.
  - A compiled-in **debug print** `(category, fmt, ...)` (categories like `[LOOT]: `),
    found as the call target of
    `E8 ? ? ? ? 48 83 C3 10 48 8D 54 24 40 48 89 5C 24 30 48 8D 4C 24 30 E8 ? ? ? ? 84 C0 75 AE`.
  - String database (localised text by id): pointer at RVA `0x2701720`, getter `0x23A6A0`;
    "lua string lists" tables between RVAs `0x1B28C09` and `0x1B399F9`.
- **SystematicSkid/sow-freecam**: `GameClient*` global via
  `48 8B 0D ? ? ? ? E8 ? ? ? ? C7 47` (rip-relative at +3); `GameClient+0x898`
  CameraOwner, `+0x50` gameplay Camera, `+0x98` transform, transform `+0xAC` position
  (vec3) then rotation (quat); CameraOwner mode int at `+0x80`.
- **ReaperAnon mods** (byte-pattern scans after the game is up; loader waits 10 s, then
  polls every 500 ms): health store `0F 2F F2 F3 0F 11 93 2C 04 00 00`
  (`movss [rbx+0x42C]`: a character's health at `+0x42C`), a "dev bool" check
  `48 8D 0D ? ? ? ? E8 ? ? ? ? 84 C0 74 37 48 8B CB`, mission timer, hit streak, orc
  spawn rarity, Uruk intel. The loader also replaces the game's `GetActiveWindow` call
  so it keeps running unfocused.
- Our own check today: the exe has 1,323 MSVC RTTI names, but only for Havok (`hk*`,
  incl. `hkaiCharacter`), WB's online SDK (`hydra`: Session, Match, PlayerJoined...)
  and std. **Game classes carry no RTTI names**; names must come from the script-var
  registry and debug prints.

### Constraints

- Code on disk is packed (Denuvo): everything below runs in the game process after it
  has unpacked itself (poll for a known pattern, as the existing mods do). No patching
  or stripping of Denuvo/SteamStub. No debugger attached (anti-debug risk; also an
  extra install). No full dump of the unpacked exe for now.
- Only in test copy A, guard always on (saves sandboxed, network offline). Dumps and
  game-data output stay in `local/`; we publish names, offsets and signatures only.

### Steps

1. **Probe module (tooling).** Add a `sowprobe` part to the guard DLL: a background
   thread that waits until the game has unpacked, keeps the game running unfocused,
   and serves a named pipe so the agent can ask questions while the game runs:
   `module` (base, sections), `scan <pattern>` (all hits), `read <addr> <len>`,
   `rip <addr>` (resolve rip-relative), `vars` (script-var list), `log` (captured debug
   prints). Read-only by default; `write`/`call` only with `probe_write=1`. Python
   client in `sow-coop/tools`. Offline tests like the guard's.
2. **Check every known signature and RVA on our build.** Each pattern must hit exactly
   once; each RVA from the save editor must land on a function start. Record a
   signature table (pattern, hits, RVA, meaning, source, date) here. Expect matches
   (same build), verify anyway.
3. **Let the game describe itself.** Dump the script-var registry (names, descriptions,
   source files, lines): a map of the engine's systems. Hook the debug print to a log
   file. Switch on the `Faction.*` / `NemesisForge` debug vars and record what the
   Nemesis system prints during play.
4. **Player.** From `GameClient` find the local player character (camera follow target,
   or the object whose `+0x42C` changes when the player is hurt). Map position,
   rotation, health, animation state by snapshot-and-diff while the player moves (keys
   posted to the game's own window, no OS input). Same for an orc near the player.
5. **Nemesis world.** Run the game's own dumps (`factions.json`, `all.dat`,
   single-member) in the sandbox and study the format. Test `load save` on a dump:
   if a client can load the host's dumped world, that is the join-time sync. Compare
   dumps before and after a Nemesis event (a captain promoted, killed, betrays) to see
   what changes; hook the code behind the related debug prints to get events.
6. **Save system.** Log every save read/write with timing (the guard already sees
   them), relate saves to `all.dat`, find the autosave trigger.

### Needs

- **A save with progress in the sandbox.** Nemesis data only exists after the prologue.
  Proposal: copy one existing save from the backup into the sandbox profile (sandbox
  only; the original stays untouched). Alternative: a new game, hours of play.
- Later, only if steps 2-5 stall: install Ghidra (+ JDK) or a debugger, or dump the
  unpacked exe for static analysis.

### Risks

- Denuvo may react to code patches in its protected range (the existing mods patch game
  code and work, so probably not).
- Calling developer commands can crash or corrupt state: sandbox only, crash logged.
- The save editor's RVAs may be from a different build than ours (step 2 settles it).

## Session 2026-10-20 j: phase 3 step 1, probe built (not yet run in the game)

- The latest real save (unnumbered file == slot 1, from 2025-11-20) copied from the
  backup into the sandbox profile only; hashes equal; backup check unchanged.
- `sowprobe` added to the guard DLL (`probe=1`): waits for a pattern that only exists
  after the game unpacks (default: the freecam's GameClient load
  `48 8B 0D ? ? ? ? E8 ? ? ? ? C7 47`), logs how long that took, and serves read-only
  `ping`, `module`, `scan`, `read`, `rip`, `deref` on a local named pipe. It reads only
  committed, readable, non-guard pages (VirtualQuery first), so it never trips a guard
  page. Client: `sow-coop/tools/probe.py`.
- Offline suite: **RESULT: PASS (23 of 23 cases passed)**; new `probe_serve` checks every
  command against fixtures in a live child (marker scan exact and with wildcards, read,
  rip-relative resolve, pointer deref, unmapped read -> err, unknown command -> err,
  still alive after errors) and `probe_off` (no pipe without `probe=1`). Mutant 6/6.
  Python client checked against a live probe.
- Staged in test copy A (`probe=1`, `network=offline`). Next: launch with the probe on,
  confirm the ready pattern appears and when, then step 2 (check every known signature
  and RVA on this build).

## Session 2026-10-20 k: phase 3 step 2, signatures checked in the running game

Launched test copy A with guard + probe (`network=offline`), left at the main menu
(save not loaded). All checks below are read-only (`sow-coop/tools/sigcheck.py`,
`diskcheck.py`); raw output in `local/phase3/`.

### Observations

- Probe ready **187 ms** after start; GameClient pattern hit once at RVA `1775F54`.
  Image base `0x140000000` (the preferred base), so RVA + 0x140000000 = address.
- **Correction to the phase 2/3 assumption "code on disk is packed":** all 7,559 pages of
  `.arch` (RVA 0x1000, 31 MB, the game's code), all of `.pdata` (unwind table, i.e. every
  function's start and end) and `.sdata` are **byte-identical in memory and on disk**.
  Only the first 2 pages of `.xtext` (4.6 KB) and 16 bytes of `.idata` differ (filled at
  load). The opaque part is the 73 MB RWX `.bss` (Denuvo's own code). So the game's code
  can be studied statically from the exe file (e.g. `dumpbin /disasm`, `.pdata` for
  function bounds): no memory dump needed, nothing that touches the protection.
- **Signature table (Steam build 3095001, exe SHA-256 `ed0d95ca...0bdaef`); hit counts
  are exact (scan max 16):**

  | what | pattern | hits | hit RVA | resolves to | source |
  |---|---|---|---|---|---|
  | GameClient global | `48 8B 0D ? ? ? ? E8 ? ? ? ? C7 47` | 1 | 1775F54 | 26FFA98 (rip +3/7) | freecam |
  | script-var registry | `48 89 05 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 05 ? ? ? ? 48 89 05 ? ? ? ? 48 89 05 ? ? ? ? 48 89 05 ? ? ? ? E8` | 1 | 816D2A | 2A58BE0 (rip +3/7) | save editor |
  | debug print | `E8 ? ? ? ? 48 83 C3 10 48 8D 54 24 40 48 89 5C 24 30 48 8D 4C 24 30 E8 ? ? ? ? 84 C0 75 AE` | 1 | 36C447 | 19C83AC (call) | save editor |
  | debug print call sites | save editor probes 2, 3, 5 | 1 each | 101A63, 101A99, 4CDCC8 | all 19C83AC | save editor |
  | save editor probes 0 and 4 | (see its source) | 0 | | | save editor |
  | health store `movss [rbx+42C]` | `0F 2F F2 F3 0F 11 93 2C 04 00 00` | 1 | 2E4D26 | | ReaperAnon |
  | dev bool check | `48 8D 0D ? ? ? ? E8 ? ? ? ? 84 C0 74 37 48 8B CB` | 1 | 2D769F | 2A34D6A (rip +3/7) | ReaperAnon |
  | mission timer | `48 8D 59 10 48 8B F9 48 8B CB E8 ? ? ? ? 84 C0 0F 85` | 1 | 16F382 | | ReaperAnon |
  | 13 other ReaperAnon patterns (item challenges, elven speed, FOV, ice storm, hit streak, mounted might, multi equip, mirian, orc rarity, skill level, prerequisites, story locks, uruk intel) | see their repo | 1 each | | | ReaperAnon |
  | mod loader's GetActiveWindow call | `FF 15 2A D7 16 01` | 1 | C25638 | | mod loader |

- **The save editor's fixed RVAs do not fit this build.** Its nine command RVAs
  (`1C76D08`, `1C76784`, `1C7709C`, `1C77070`, `1C762AC`, `1B7663C`, `1BECB58`,
  `1C920D4`, `23A6A0`) all land mid-function (no padding before, no prologue), and its
  debug bools `2A34DA3..2A34DB8` are all 0 and match no registry entry. Its *patterns*
  match. It was probably written against another build (Microsoft Store?). **Never call
  those RVAs on this build.**
- **Script-variable registry** (RVA `2A58BE0`, walked as the save editor does): 680
  entries, 170 source files under `C:\P4\KRAKEN_CODE_STREAM\source\` ("Kraken" is the
  engine's internal name, as in the exe's `SOFTWARE\WB Games\Kraken` string). Entries
  are 0x40-byte records: `+0` vtable (kind), `+8` name, `+0x10` description, `+0x18`
  file, `+0x20` line, `+0x28/+0x30` list links, `+0x38` **handler pointer for console
  programs** / value for variables. Kinds by vtable RVA: `1E69180` program (449),
  `1E69FD0` bool (69), `1E69FA0` (58, floats?), `1E69FB8` int (28, e.g.
  `NetMeshGamePort` = 0x408 = 1032), `1E69F88` (27), `1E69168` string (8,
  `Cloud.APIKey`, `Cloud.Server`...), render settings (a few others).
- Source layout is LithTech's client/server split: `Game\ClientShellDLL\` (client),
  `Game\ObjectDLL\` and `ObjectDLL_AI\` (server-side game objects: `PlayerObj.cpp`,
  `Combat.cpp`, `Quest.cpp`, `SlowMoUtil.cpp`), `Game\Shared\` (incl. all Faction code),
  `Engine\Runtime\client\clientmgr.cpp`, `Engine\Runtime\net\NetStateMachine_HostVote.cpp`.
- Co-op relevant console programs (name: description, source):
  - `Faction.Save` / `Faction.Load` / `Faction.Dump`: save, load or dump **all faction
    members and relevant data** to a named file (FactionConsolePrograms.cpp:254/285/261).
  - `FactionMember.Save` / `.Load` / `.Save.Pos` / `.ProfileDump` / `.SetDead` /
    `.SetHealthPct` / `.SetXPLevel` / `.Rebuild` / `.BuildWithBuilder` / `.SlotAsOverlord`.
  - `Faction.AdvanceTime` ("advances time, and will generate new simevents"),
    `Faction.SimEvent`, `.SimEvent.Generate` / `.Simulate` / `.Flush`,
    `Faction.SimEventDump`, `Faction.AddEvent`, `Faction.ActivateSimEventSequence`: the
    off-screen Nemesis simulation runs on "SimEvents".
  - `QuestMgr.Save` / `QuestMgr.Load`, `Quest.ListActive`, `Quest.CompleteActiveQuest`,
    `QuestMgr.ReplayQuest`, `QuestMgr.SetQuestComplete`.
  - `Debug.Save` / `Debug.Load` ("a debug save that is not user specific"), User.cpp.
  - `Teleport <x> <y> <z>` (PlayerObj.cpp:4224).
  - `SlowMO.Set` ("time scale denominator"), `.Start`, `.Stop`, `.Pause`, `.Resume` in
    `ObjectDLL\SlowMoUtil.cpp`: slow motion is a world (server-side) time scale.
  - `GameServerShell.DebugPause` / `.DebugUnpause` / `.PrintPauseFlags` /
    `.ResetWorldIdentical` ("resets the world to its identical state") /
    `.AdvanceTimeOfDay`, defined in `ClientShellDLL\WorldSimSession.cpp` and
    `GameClientShell.cpp`.
  - `Replay.StartRecordingPlayback` / `.SavePlayback` / `.LoadAndStartPlayback` /
    `.VerifyPlayback` ("runs the loaded playback 10 times and outputs the number of
    successes/failures"): input and frame-time recording and playback (PlaybackMgr.cpp).
  - `NetMeshGamePort` ("port to use for the game NetMesh when hosting"),
    `Net.NSM.HostVote.Timeout` / `.PauseInStart` (net state machine with a host vote).
- Camera chain at the main menu: `*(26FFA98)` GameClient -> `+0x898` CameraOwner ->
  `+0x50` gameplay camera = null (expected outside the world).
- Probe usage note: `deref` offsets are C-style numbers, so hex needs `0x`.

### Derived

- Use patterns and registry names, never fixed RVAs from other builds.
- Every console program can be found by name; its handler is at entry `+0x38`. The
  calling convention is unknown for this build (the save editor used `(int argc,
  const char** argv)`); check it in the disassembly before ever calling one.
- The engine kept LithTech's client/server structure, has a NetMesh with hosting, a
  host-vote state machine, a replay system that looks deterministic, and world-level
  save/load of the Nemesis data. If the client/server split is real at runtime (a local
  server object world plus a client), co-op could attach a second client to the host's
  server instead of syncing everything by hand. **Unverified; the most important open
  question for the co-op design.**

### Next

- Static pass on the exe file: function table from `.pdata`; disassemble the handlers of
  the programs above and the server-shell, clientmgr and NetMesh/host-vote code.
- Step 3 (needs a write/hook-capable probe, `probe_write=1`): hook the debug print to a
  log; call harmless programs (`Faction.SimEventDump`, `Quest.ListActive`) in the sandbox
  once their calling convention is confirmed statically.

## Session 2026-10-20 l: phase 3 step 3, debug-print capture

### Built (sow-coop `7606a1f`, offline suite 26/26, mutant 6/6)

- `probe_write=1` mode: pipe command `write <addr> <hex>` (logged), and hooks.
- `hook_debug_print=1`: the debug print (found through the save editor's call-site
  pattern, target RVA `19C83AC`) is, in this build, a **12-byte stub**:
  `4C 89 44 24 18` (mov [rsp+18h],r8), `4C 89 4C 24 20` (mov [rsp+20h],r9), `C3`, `CC`,
  with the next function right after (static disassembly, `dumpbin /disasm`). The hook
  checks those exact 12 bytes, then replaces them with `mov rax,<stub>; jmp rax` (12
  bytes, so the next function is untouched; rax is free at entry). Our stub spills
  rcx/rdx/r8/r9 to the caller's home area like the original, calls a guarded formatter
  and returns; unwind info is registered for it. Output: `<profile>\debugprint.log`
  (64 MB cap), a ring for `dbg`, per-category counts for `dbgstats`.
- Tests cover: patch shape and the neighbouring function, write and read-back, 1,005
  calls from 5 threads including stack arguments, a bad `%s` pointer (logged as
  `<fault while formatting> <format>`), `%n` (logged as `<bad format> <format>`), refusal
  when the stub differs, and no hook or write without `probe_write=1`.

### In the game (launch 4, main menu)

- Hook installed 330 ms after launch, before the game's own boot; game ran normally.
- **0 debug lines** in the first 2+ minutes: release builds gate almost every call behind
  a per-channel byte flag that is off.
- Static catalogue of the callers (`sow-coop/tools/dpsites.py`, exe file only):
  **1,100 call sites**; categories include `[COMBAT|...]` (most), `[QUEST]` (99),
  `[QUEST|JOB]` (47), `[AI|SPAWNAMBIENT|REJECT]` (33), `[FACTION|AMBUSH]` (25),
  `[FACTION|RESURRECTION]` (19), `[FACTION|CATALYST]` (18), `[LOOT]` (18),
  `[FACTION|BUNDLE]` (15), `[SIMEVENT]` (8), `[SIMEVENT|OUTCOME]` (8), `[SLOWMO]` (19).
  449 sites have a `cmp byte ptr [rip+x], 0` / `movzx` guard within 64 bytes before the
  call; 651 have none found (other guard shapes or unconditional).
- Channel flag table (byte per channel, RVA -> category of the calls it gates):
  `2A34D9E` QUESTMGR, `2A34DA0` QUEST|JOB, `2A34DA1` QUEST|PREDICATE, `2A34DA3`
  INVENTORY|PERSISTENT, `2A34DA5` FACTION|CATALYST, `2A34DA6` FACTION|HIJACK, `2A34DA8`
  FACTION|REWARD|REJECT, `2A34DAA` FACTION|GRUNTTRAIT, `2A34DAB` FACTION|STATQUERY,
  `2A34DAE`/`2A34DAF` SIMEVENT, `2A34DB0` SIMEVENT|OUTCOME, `2A34DB1` SIMEVENT|REPLAY,
  `2A34DB2` SIMEVENT|SEQUENCE, `2A34DB3` FACTION|RESURRECTION, `2A34DB4` FACTION|AMBUSH,
  `2A34DB5` LOOT, `2A34DB6` FACTION|BUNDLE, `2A34DB7` FACTION|FORTDEFENSE, `2A34DB8`
  FACTION|TRIBE, `2A34DBC` SLOWMO, `2A34DBE` NEMESISFORGE, `2A34DBF` GAMEMODE|SESSION,
  `2A34F95` QUEST, `2A366B1` FACTION|PURPOSE; combat channels `2A34D1A..2A34D98`,
  `2A25D80/81` COMBAT|HINTS.
- **Correction to session k:** the save editor's debug-bool RVAs (`2A34DA3` inventory,
  `2A34DA5` trait catalyst, `2A34DAA` grunt trait, `2A34DAB` stat query, `2A34DB3`
  resurrection, `2A34DB4` ambush, `2A34DB5` loot, `2A34DB8` tribe) are exactly this
  build's channel flags for those categories. They read 0 because channels default off,
  and they are not registry variables, which is why the session-k check found no match.
  So its *data* RVAs fit this build; its *function* RVAs still do not.
- Set 23 channels on with `write` (quest, faction, simevent, loot, slowmo, nemesis
  forge, game mode session; combat left off): flags read back as 01. Still 0 lines at
  the main menu (no world loaded). Next: load the sandboxed save and capture.

## Session 2026-10-20 m: crash with the first debug-print hook, root cause and fix

### Observations

- Launch 4 (hook v1, 23 channels on, written by hand after launch): the sandboxed save
  was loaded by hand. Within ~0.3 s of world load the hook captured 85,558 lines
  (13 MB): 85,248 `[FACTION|REWARD|REJECT]` ("Reward '...' Failed Filter -
  IncludedTraitMarkerList (...)"), 156 `[FACTION|PURPOSE]`, 104 `[FACTION|TRIBE]`, 49
  `[SIMEVENT|OUTCOME]` (e.g. `HandleFactionEventAction: Event =
  FactionMember_RoleSelected_Standard; Subject = Gubu`), 1 `[QUESTMGR]: GameModeSet -
  SinglePlayer`. One second later the game crashed: Application Error, `ShadowOfWar.exe`,
  `0xC0000005`, fault offset `0x598817`. Sandbox saves were not rewritten; backup check
  unchanged.
- Static analysis (exe file): `598817` is the first real instruction of function
  `598804..59894B` (from `.pdata`): `mov rbx, [rcx+48h]`, i.e. a bad `this`. Of its 5
  callers, `F4F41A` is right after the `GameModeSet - %s` print:
  ```
  F4F3D5  cmp byte ptr [2A34D9E], 0      ; QUESTMGR channel
  F4F3DC  mov r9, [r14+6C60h]            ; value needed after the print
  F4F412  call 19C83AC                   ; debug print (only when the channel is on)
  F4F417  mov rcx, r9                    ; r9 used after the call, not reloaded
  F4F41A  call 598804
  ```
  `r9` is volatile in the Win64 ABI, but the compiler knew the callee (the 12-byte stub)
  changes no register and kept a live value in it. Our v1 stub called a normal C++
  function, which clobbered r9; the `mov rax,imm64; jmp rax` patch also clobbered rax.
  **Root cause: our hook, not the game.**

### Fix (sow-coop `b9e5421`)

- Patch is a 5-byte `jmp rel32` (+ int3) to a stub page allocated within 2 GB of the
  game code: no register touched. The stub writes only the two home slots the original
  writes (`[rsp+18h]`, `[rsp+20h]`), saves and restores rax, rcx, rdx, r8-r11, xmm0-5
  and RFLAGS, passes `va_list = &[entry rsp + 18h]` (contiguous with the stack
  arguments), and registers unwind info for its pushes.
- New test: generated machine code sets rax, rcx, rdx, r8-r11, xmm0-5 and two sentinels
  in the caller's home slots, calls the stub, and checks all of them after. The hooked
  stub and the original stub both pass ("regs ok"); a mutant that does not restore r9
  fails ("changed: r9 r11"). Suite 26/26, mutant 7/7.
- `debug_channels=<rva,...>` sets channel flags right after the hook installs (no race
  with the player loading a save). Staged with 22 channels; `FACTION|REWARD|REJECT`
  (`2A34DA8`) left off as noise.
- Residual risk, not covered: the game has some AVX code (39 `vzeroupper`, ~22 256-bit
  moves in `.arch`); the stub does not save ymm upper halves. MSVC emits `vzeroupper`
  before calls out of AVX code, so they should not be live across this call.

### Lesson for the playbook (hooking)

- A tiny leaf function (a release-build stub) can have callers that rely on it not
  touching *volatile* registers: whole-program optimisation may keep values in rcx,
  rdx, r8-r11 or rax across such a call. When replacing such a function, preserve every
  register it preserved, write only memory it wrote, and reach it with a jump that
  clobbers nothing (`jmp rel32` to a nearby page). Test it with a register-snapshot
  thunk and a mutant, not only with C++ calls (which never notice).

## Session 2026-10-20 n: launch 5 (fixed hook) and an isolation failure via Steam

### Observations

- Hook v2 installed 330 ms after launch; 22 channel flags set (read back as 01;
  `FACTION|REWARD|REJECT` left 00). Sandboxed save loaded, ~10 min of manual play;
  game exited normally (exit 0 after 635 s, no Application Error). Debug capture
  `debugprint.log` 4.1 MB (analysis pending). Offline mode: 8 HTTPS ConnectEx refusals
  (the same two hosts, every ~15-45 s).
- The game saved during play: in the sandbox, `<prefix>_ShadowOfWar.sav` and `_1_` were
  rewritten (2,361,712 bytes, 19:17:51.955) and the old slot 1 moved to `_2_`
  (save rotation). All through the guard's redirect, as designed.
- **Backup check failed:** in the real `Steam\userdata\<id>\356190\`,
  `remote\<prefix>_ShadowOfWar.sav` was rewritten at 19:17:51.949 with **the same bytes**
  as the sandbox save (SHA-256 prefix `0033D4753E3610BC`), and `remotecache.vdf` at
  19:18:02.908 (size, time, sha updated; `syncstate` 1 -> 3). The real
  `%LOCALAPPDATA%` saves and everything else in the manifest are unchanged.
- The guard logged no write to that folder from the game process (it would have been
  redirected or stopped the game). So another process wrote it: the Steam client.
- Steam Cloud for 356190 is still `cloudenabled 0`; nothing indicates an upload.

### Derived

- The game sends its save through `ISteamRemoteStorage` as well as writing it to disk;
  the Steam client process (`steam.exe`) writes that copy to `userdata\<id>\356190\remote`
  even with Steam Cloud off for the game (it keeps the local copy and marks it unsynced).
  **In-process file redirection cannot cover this.** Correction to session c and
  session f: the Steam remote-storage writes are not made by the game process (the
  `175823492` write seen in-process is a different file).
- Before any further session in which the game can save, the guard must stop
  `ISteamRemoteStorage` writes inside the game process (e.g. make FileWrite/
  FileWriteAsync/FileDelete/FileForget fail or turn them into no-ops) and the test
  must check the real Steam folder after a save. Until then: no saving in the test
  copy.

### Proposed repair of the real Steam folder

- With Steam fully closed (Steam may rewrite `remotecache.vdf` from memory on exit),
  restore `remote\<prefix>_ShadowOfWar.sav` and `remotecache.vdf` from the verified
  backup `local/backups/2026-10-20/steam-userdata-356190/`, then rerun the backup check.

### Playbook lesson (isolation)

- An isolated test copy is not isolated from the store client. Steam's cloud-save API
  is serviced by `steam.exe`, which writes into the real userdata folder whatever
  the test copy saves, even with Steam Cloud disabled for the game. Block the
  remote-storage writes inside the game process and check the store client's folders
  after every test session that saves.
- Restored (Steam fully exited, no Steam or game process running):
  `remote\<prefix>_ShadowOfWar.sav` and `remotecache.vdf` copied back from the verified
  backup with their original timestamps (robocopy /COPY:DAT); the test-written versions
  kept in `local/backups/2026-10-20-steam-overwritten-by-test/`. Backup check after:
  30/30 present, **unchanged** (render.cfg info only). Not yet seen: what Steam
  does with these files on its next start (it should find them matching its cache).

## Session 2026-10-20 o: Steam remote-storage guard (built, tested offline, staged)

- Static (exe and the shipped `steam_api64.dll`, SHA-256 `f12cf470954d9d956f7d44f9eb84659890c5722a021548ab88710340c0965230`,
  image base `0x3B400000`, 796 exports): the game's only static Steam import is
  `SteamInternal_ContextInit` (cached-accessor SDK style). Each flat wrapper is
  `mov rax,[rcx]; jmp [rax+off]`, giving the ISteamRemoteStorage (v014) vtable offsets:
  FileWrite 0x00, FileRead 0x08, FileWriteAsync 0x10, FileForget 0x28, FileDelete 0x30,
  FileShare 0x38, SetSyncPlatforms 0x40, FileWriteStreamOpen/WriteChunk/Close/Cancel
  0x48/0x50/0x58/0x60, SetCloudEnabledForApp 0xB8, UGCDownloadToLocation 0x1B0;
  ISteamClient::GetISteamRemoteStorage 0x88.
- Guard (sow-coop `37a594b`, always on): waits for the game's SteamAPI_Init, gets the
  same interface through the flat API, reads the slot offsets from the wrappers' bytes
  (any other shape: exit `0xC0DE0006`), and replaces those 12 write methods in the vtable
  with functions that log and report success (async/stream: invalid). Reads untouched.
- Offline tests with a stand-in `steam_api64.dll` (byte-exact wrappers, counting
  vtables; never copied into a game folder): **28/28 pass**; `steam_block`: all 12 writes
  answered by the guard, none reached "Steam", FileRead passed through; mutant without
  the guard: all 12 reached it (caught, 8/8). Note: this compiler emits
  `mov r10,[rax+off]; jmp r10` for such wrappers, so the fake's wrappers are raw bytes.
- Staged in test copy A; the test copy's real `steam_api64.dll` is untouched (hash equal
  to the real install's). Backup check unchanged.
- Not covered: ISteamUserStats (achievements/stats earned in test sessions go to the
  real Steam account via steam.exe); other Steam interfaces.
- Next: a guarded session that includes an autosave, then the backup check must stay
  clean for `userdata\<id>\356190`.

## Session 2026-10-20 p: guarded save session (launch 6), Steam folder stays clean

- Pre-check with Steam restarted after the restore: unchanged (Steam accepted the
  restored remote files).
- Launch 6: debug-print hook + 22 channels in at ~330 ms; **Steam remote-storage guard in
  at 1,015 ms** (before the game's own boot). Sandboxed save loaded and played by hand;
  exit 0 after 608 s; no crash, no FATAL/PROTECTED/WATCH.
- **16 saves** in the session (first at +289.7 s, then every 0.6-46 s while playing:
  autosave is frequent). Each save writes `<prefix>_ShadowOfWar.sav` and `_1_` to disk
  (redirected to the sandbox) and, at the same moment, calls
  `ISteamRemoteStorage::FileWrite('\<prefix>_ShadowOfWar.sav', ~2.3 MB)`; all 16 blocked
  by the guard (logged with sizes; the last, 2,315,168 bytes, equals the final save on
  disk). Slots rotate once per session, at the first save (`_1`->`_2`->`_3`).
- **Backup check after: unchanged.** The real `remote\<prefix>_ShadowOfWar.sav` is
  untouched (2,334,736 bytes, 2025-11-20). Steam rewrote `remotecache.vdf` at game exit
  (19:45:44) with byte-identical content (hash equals the backup).
- Debug capture this session: 3.4 MB (`debugprint.log`, local only); analysis pending.
- Isolation status: file redirect + guard (game process) and the remote-storage guard
  (Steam API) together keep the real saves and Steam remote folder unchanged through
  a real save session. Still open: ISteamUserStats (achievements/stats).

## Session 2026-10-20 q: what the Nemesis system logs (debug-print capture, read-only)

Sources: two captures with 22 channels on (combat and `FACTION|REWARD|REJECT` off), manual
play on the sandboxed save: session 5 (42,977 lines, ~10 min, two captain kills) and
session 6 (36,108 lines, ~6 min of play, 16 autosaves). Tool: `sow-coop/tools/dpanalyze.py`
(message templates per category). Raw logs stay in `local/`.

### Observations

- **One thread.** Every line of every category in a session comes from a single thread
  (session 5: 5264, session 6: 25236): faction, quest, SimEvent, loot, ambush and slow-mo
  logic all run on the same game thread.
- **Volume by category** (session 5 / 6): FACTION|BUNDLE 23,522 / 24,002; QUEST|JOB 5,510 /
  2,948; FACTION|PURPOSE 3,204 / 2,667; FACTION|TRIBE 3,079 / 2,531; SIMEVENT|OUTCOME 1,617 /
  1,260; LOOT 1,532 / 1,132; SLOWMO 1,549 / 142; SIMEVENT 1,176 / 461; FACTION|AMBUSH 1,067 /
  311; QUEST 630 / 597; SIMEVENT|REPLAY 25 / 2; FACTION|CATALYST 24 / 0;
  FACTION|RESURRECTION 7 / 0; plus a few GRUNTTRAIT, HIJACK, STATQUERY, QUESTMGR,
  GAMEMODE|SESSION.
- **Central event dispatch:** `HandleFactionEventAction: Event = <name>; Subject = <captain>;
  Other = <captain|None>` (SIMEVENT|OUTCOME). 40 event types in session 5, e.g.
  `FactionMember_RoleSelected_Standard/_Notable/_BazGazDaz` (role assignment, ~1,400 at
  load), `IsPlayerFollowerApply`, `Promote`, `TimePassed`, `PromoteEmpty_Builder*`/
  `PromoteDead_Builder*` `_Simulate`/`_Clear`, `ActiveKilledByPlayer`,
  `BrotherKilledByPlayer`, `ActiveDominationStarted`, `ActiveFlee`, `FleeRunStart`,
  `ActiveTerrorTriggered_*`, `ActiveHateTriggered_All`, `ActiveShowdownGiven_Distance`,
  `ActiveInitialPlayerThreat`, `ActiveGainInterrogationIntel`, `ActiveLevelUpPlayerLimitLevel`,
  `Ambush_Presented_BloodBro`, `ArcherPutsPlayerInLastChance`, mission resets
  (`CampRaid2_Clear`, `Chase_Clear`, `FeastSingle_Clear`, `Raid_Checkpoint_Clear`,
  `Trial_Clear`), DLC3 setup (`DLC3_SlayerSetup`, `DLC3_MtnChiefSetup`,
  `DLC3_FakeDeadOverlord`); session 6 adds `ActiveInterrogated`, `DatalLevelChanged`,
  `ActiveKillWarg`. Also `Debug_RecruitFollower` (34) and `DEBUG_AddPIS` (16) at load in
  both sessions (normal flow or caused by the debug channels: unknown).
  143 distinct captain names appear as Subject in session 5.
- **A captain's death, live (session 5):** `ActiveDominationStarted` -> 26 s later
  `ActiveKilledByPlayer` -> `AddFactionEventReplay: BR_Single_Died` -> resurrection check
  (`Killed - Evaluating Resurrection Record 'Resurrection_HeadRemoved'; Attack:
  PC_Bip_Standing_InstaKill_Lvl2_B, TotalInteractionScore = 60`; no chance record: stays
  dead) -> `BrotherKilledByPlayer` for his blood brother. Second kill: record
  `Resurrection_Executed`, chance record `Resurrection_3rdFew`, **`Chance to resurrect:
  0.100000, roll 0.904506 = Remain Dead`**.
- **Promotions:** 72 s after the second death, `PromoteDead_Builder` (Faction_MtnPass):
  `Promote; Subject = <winner>; Other = <dead>`, `Loser (...) Vacating their position 4, 7`,
  `Winner (...) moving into position 4, 7`. Five seconds later a burst of
  `PromoteEmpty_BuilderWC` / `PromoteEmpty_Builder_Transform` simulations in
  Faction_desert fills ~9 empty positions (`moving into position <row>, <col>`).
- **Event replays** (`SIMEVENT|REPLAY`): `BR_Promo_FromSystem` (13), `BR_TimePassed` (8),
  `BR_Single_Died` (2), `BR_TimePassed_CreatePromotions` (2), `BR_Single_Succeed` (2):
  presumably the data for the game's "while you were away" screens.
- **Regions (factions):** Faction_MtnPass, _MinasIthil, _Demovolcano, _volcano, _snowMtn,
  _desert_Real, _island, _desert, _desert_mercs_DLC and `_DLC` variants of six of them.
  Captains hold positions addressed as (row, col) inside a region's hierarchy.
- **Jobs and SimEvents:** `[QUEST|JOB] Process Job: <job>` with reject reasons (`Random Roll`,
  `PlayerFollower predicate`, `has disabled simevents`, `PrisonerIfFollower predicate`);
  jobs such as `Relationship_Nemesis`, `Relationship_BloodBrother_SpawnIn`,
  `PS_Subject_Cap_Feast`, `PS_Other_Cap_GuardWarCouncil`, `SC_DeathThreat`.
  `[SIMEVENT] Evaluating Event: <event>` (`Challenge_DT`, `FeastSingle_Revenge`,
  `Guard_WarCouncil_Revenge`, `SpyGuardBetrayal`, `Rescue`, `Feast`, `Location`...),
  `Passed` / `Rejected, Unable to find Participant for Required Job ...` /
  `Simulate - <event> (<faction>)`.
- **Nemesis missions are quests:** `qFeastSingle_*`, `qCampRaid_*`, `qChase_*`, `qGuardWC_*`,
  `qGauntlet_*`, `qFortDefense`, `qspSotP_*`, with state-machine states `Inactive_OnEnter`
  (773), `SimEventEval_OnEnter` (89), `PreActivate_OnEnter` (89), `Cleanup_OnEnter` (17), and
  `Turning ON quest trigger ...` / `NavMarkerType`.
- **Ambushes** are chance-based with cooldowns and deferral: `(Deferred) Attempting to
  trigger Ambush: <record>`, `Deferring Ambush ... for 7.241 minutes`, `Ignoring Ambush ...
  The faction has changed` / `... within a FactionAmbushDisableBrush`; records e.g.
  `PC_Damage_BloodBrother_Ambush`, `CheatDeath_FollowerStillLoyal`, `BruzBrothers_Daz_Ambush`,
  `AI_BecomeHighestLevelFollower_Betrayal`, `PC_LowHealth_*`, `PC_Showdown_Ambush`,
  `NewClass*`.
- **Slow motion sources** (`[SLOWMO] Start: <record>`): `HitCounterSlowMo`, `WraithMode`,
  `WraithDash_Slomo`, `Instakill_01_SM`, `Ranged_Aerial_SlowMo`, `ShadowStrike_SM`,
  `Kill_Decap_TrackHead_SM`, `Identify_Slowmo_Timestop_50_SM`, `Dom_Default_SM` /
  `Dom_UI_SM` / `Dom_Eye_Zoom_*` (domination), `PreDeathAimStruggle_Faster`, and others;
  set through `SetCombatDynamicSlowMo` / `SetCombatWeaponModeSlowMo` /
  `SetPresentationSequenceSlowMo`.
- **Timeline (session 6):** a 2,500-line Nemesis build at 8.6 s, while the game was still
  at the main menu; silence until the save was loaded (~210 s); a load burst (purposes,
  tribes, 1,900 job evaluations); then during play mostly loot, ambush and slow-mo lines.
  `FACTION|BUNDLE` comes in two bursts of ~11,500 lines repeating the fortress overlord's
  traits, upgrades and bundles (not periodic).

### Derived (for co-op)

- `HandleFactionEventAction(Event, Subject, Other)` is the obvious hook for a host-side
  Nemesis event stream; its address can be found from its format string's call site.
- Outcomes are random (resurrection roll, job "Random Roll", ambush chances, promotion
  candidates): a client must never run these rolls itself; it takes the host's results
  (or the host's world state via `Faction.Save`/`Load`).
- The whole Nemesis/quest/ambush logic runs on one thread, which makes it simpler to
  pause, hook and order events.
- Slow-mo comes from many records (combat, wraith, aiming, domination presentation); co-op
  needs a policy per source (local presentation only vs world time).

### Next

- Find `HandleFactionEventAction` and the promotion/death functions statically from their
  format strings; then decide what to sync: event stream vs `Faction.Save` state.

## Session 2026-10-20 r: Nemesis event, death and promotion functions located (static)

Method (read-only, exe file; `.arch` is identical in memory, see session k): find each
log format string, find RIP-relative `lea/mov` references to it in `.arch`, map them to
functions through `.pdata` (following chained unwind entries), disassemble with
`dumpbin /disasm`, find callers (E8/E9 rel32). Tool: `sow-coop/tools/xref.py`
(`strings`, `callers`, `func`, `sig`). Signatures: built from the disassembly with only
rel32 call/jump targets and RIP-relative disp32 wildcarded, grown until they match
**exactly once** in `.arch` (checked by the tool). Build: Steam 3095001, exe SHA-256
`ed0d95ca...0bdaef`, image base 0x140000000.

### Functions

| RVA (range) | name we use | evidence | prototype (derived from register use) | unique signature |
|---|---|---|---|---|
| `40C304`-`40C8F3` | **HandleFactionEventAction** | only reference to `"HandleFactionEventAction: Event = %s; Subject = %s; Other = %s"` (1FDD3A8), also to `"Loser (%s) Vacating their position %d, %d, (Event = %s)"` and `"Winner (%s) moving into position %d, %d (Event = %s)"`: **promotions are applied inside this handler** | `void (Mgr* this /*rcx*/, const FactionEventAction* a /*rdx*/)`; `a+0x00` subject ref, `a+0x10` other ref (16-byte refs), `a+0x20` event record, `a+0x56`/`a+0x57` two flags; caller ignores rax | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 70 4C 8B FA` |
| `62E670`-`62EA4D` | ApplyFactionEventOutcome (caller) | only caller of 40C304 (at 62E8B4) and a caller of 741284 (at 62E95D) | `(Mgr* this /*rcx*/, const FactionEventAction* a /*rdx*/, ...)`; resolves subject and other the same way | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 81 EC 80 00 00 00` |
| `6DB95C`-`6DBCD2` | OnMemberKilled_EvaluateResurrection | only reference to `"%s: Killed - Evaluating Resurrection Record '%s'; Attack: %s, TotalInteractionScore = %d"`; 4 callers (4A066D, 17C05E1, 17C0869, 17C098E) | `(FactionMember* dead /*rcx*/, Record* attack /*rdx*/, ? /*r8*/, bool /*r9b*/)`; picks a resurrection record, stores it at `dead+0x2C0`, calls 6DBD4C | `48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 60 20 55 41 56 41 57 48 8D 68 A1 48 81 EC A0 00 00 00 4C 8B F2` |
| `6DBD4C`-`6DC157` | RollResurrection | references `"%s: Resurrection Table Record: %s"` and `"%s: Chance to resurrect: %f, roll %f = %s"` (outcome strings `"Remain Dead"` / `"EPIC WIN (set to Resurrect)"`); called by 6DB95C and 1BBA058 | `bool (FactionMember* m /*rcx*/, Record* resurrection /*rdx*/)`; `chance = 6DC504(rec)`, `roll = RandomFloat(1.0)`, resurrect if `chance > roll`; reads `m+0x228` | `48 89 5C 24 10 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 60 48 8B 99 28 02 00 00` |
| `622A84`-`622BCD` | SimEvent::Simulate | only reference to `"Simulate - %s (%s)"`; 11 callers | `(SimEvent* this /*rcx*/)`; `this+0x48` SimEvent record, `this+0x40` faction record; checks channel flag 2A34DAE itself | `48 89 5C 24 08 57 48 83 EC 20 80 3D ? ? ? ? 00 48 8B D9 74 2B` |
| `741284`-`741580` | AddFactionEventReplay | only reference to `"------------------AddFactionEventReplay: %s------..."`; callers 62E95D, 1C1DA72 | `(this /*rcx*/, rdx, r8, r9, stack arg 5 (tested for null), ...)`: at least 5 arguments, 7 passed at 62E95D | `48 8B C4 48 89 58 08 48 89 70 10 48 89 78 20 4C 89 40 18 55 41 56 41 57 48 8D 68 B9` |
| `1B3C6C`-`1B3C96` | RandomFloat | used for the resurrection roll; 85 callers | `float (float max /*xmm0*/)` = `(Rng() & 0xFFFFFF) * 2^-24 * max` | `48 83 EC 28 0F 28 C8 E8 ? ? ? ?` |
| `1807908` (leaf, no .pdata) | Rng (global LCG) | callers: RandomFloat and 426840 only | `uint32 ()`: `state = state * 0x0BB38435 + 0x3619636B; return state >> 32`; **state: 8 bytes at RVA `2A25C88`** (in .bss) | (instruction bytes: `48 69 05 ? ? ? ? 35 84 B3 0B 48 05 6B 63 19 36`) |
| `18DDCC` (leaf) | LookupFactionMember | 5 callers incl. 40C326/40C335 | `FactionMember* (this /*rcx*/, const MemberRef* r /*rdx*/)`: id = `*(uint32*)(r+8)`, table at `this+0x20` | `8B 52 08 E9 ? ? ? ? 48 8B 49 20` |
| `18BDB0` (leaf) | RecordName | 2,214 callers | `const char* (Record* r /*rcx*/)`: null if `!r` or `!(r[1] & 2)`, else `[r+0x20]` or `""`; touches only rax and flags | `48 85 C9 74 18 F6 41 01 02 74 12 48 8B 41 20` |

Data: `FactionMember+0x498` inline name (char[]); `FactionMember+0x2C0` chosen
resurrection record; `"None"` (1DB4ED8) is the name used for a missing subject/other.

### Observations worth keeping

- **More callers rely on volatile registers across calls to tiny leaf functions** (the
  pattern that crashed hook v1): across `RecordName` (18BDB0) at 40C380 (rdx, r9), at
  6DBBE0/6DBBEB (r10, rdx), at 622A9E (r9); and across the debug print at 6DC12E (r10).
  Any hook on such a function must preserve every register it preserves.
- **The `rand` for these outcomes is one global LCG with 8 bytes of state** (RVA
  `2A25C88`), reached through RandomFloat. Only two functions call it directly; other
  random sources may exist elsewhere (not checked).

### Derived (for co-op)

- Hook points for a host-side Nemesis event stream: `HandleFactionEventAction`
  (40C304) sees every applied event incl. promotions; `OnMemberKilled_EvaluateResurrection`
  and `RollResurrection` decide deaths; `SimEvent::Simulate` covers off-screen
  simulation; `AddFactionEventReplay` the "while you were away" data.
- A client must not call `RollResurrection` (or `Rng`) for shared outcomes; it applies
  the host's results. Separately: the single LCG state could make QA runs reproducible
  (set it before a scenario) and could be synced for short deterministic sequences.
- Prototypes are from register use only; struct field meanings beyond those listed are
  unknown.

## Session 2026-10-20 s: located functions confirmed in the running game (read-only)

- Run with `probe_write=0` and `hook_debug_print=0` (nothing in the game's code patched;
  file/network/Steam guards on), game left at the main menu, `sow-coop/tools/sigverify.py`
  through the probe's read-only commands. **RESULT: PASS.**
- All nine signatures from session r match **exactly once** in the running game, at the
  RVA found in the file, and the 64 live bytes at each equal the file: HandleFactionEventAction
  40C304, ApplyFactionEventOutcome 62E670, OnMemberKilled_EvaluateResurrection 6DB95C,
  RollResurrection 6DBD4C, SimEvent::Simulate 622A84, AddFactionEventReplay 741284,
  RandomFloat 1B3C6C, LookupFactionMember 18DDCC, RecordName 18BDB0. Debug-print stub
  19C83AC unpatched (original 12 bytes).
- **Correction to session r:** the LCG step (`48 69 05 ? ? ? ? 35 84 B3 0B 48 05 6B 63 19 36`)
  occurs **twice** in `.arch`: the `Rng` function at 1807908 and a copy inlined into a cold
  part of function FC0770 (at FC0E93). Both use the same state at 2A25C88, so it is still
  one global RNG state, but it is also advanced from inlined code, not only through `Rng`'s
  two callers. Locate `Rng` as RandomFloat's call target (`rip RandomFloat+7, 1, 5`);
  verified live: 1807908.
- RNG state at 2A25C88 read twice 5 s apart at the main menu: unchanged
  (`33A3E99A5479618B`): no random draws at the menu, so the LCG step itself was not
  observed live (needs a session with play).
- Steam remote-storage guard in at 359 ms. Game stopped by the agent after the check
  (exit 0xFFFFFFFF, forced); backup check unchanged; test-copy config restored.
