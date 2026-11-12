# Mafia: Definitive Edition: research notes

Target: Steam app 1030840, build 16502642, `mafiadefinitiveedition.exe` SHA-256
`34B06C6C11326E51001EF2FD82933C7F200ACAD27EDBFCC0C159A7051978675E`, Windows 10 x64. All work so far ran on an
isolated copy of the install (`<lab>\mafia-de`, verified as the same 4,882 files and 38,908,493,197 bytes as the
store install) or read the exe without changing it. From the 2026-10-24 probe section on, the test copy was
launched, offline and guarded, by bots.

## 2026-10-24: triage and research

Observed:
- The hub had no write-up for this game. Issue #167 and PR #172 are about the 2002 original (LS3D).
- MafiaMP: <https://github.com/MafiaHub/MafiaMP>, last push 2026-10-15, pre-release v2.5.1 from 2025-12-21. The
  client hooks with MinHook plus byte patterns, e.g. `core/hooks/game_director.cpp` hooks
  `C_GameDirector::OnActiveCityChanged` through `hook::get_opcode_address("E8 ? ? ? ? 80 BE ...")`. Licence: MafiaHub
  OSS, clause 5 (<https://github.com/MafiaHub/MafiaMP/blob/master/LICENSE.txt>).
- MafiaMP's story coverage: `sdk/mafia/framework/director/c_game_director.h` (`m_pActiveCityID` at +0x28,
  `GetInstance`, `GetDistrict`) and a forward declaration of `I_Mission`. Nothing on quests, plotlines or missions.
- JustAddMP/MafiaDE: <https://github.com/JustAddMP/MafiaDE>, created 2026-10-08, pre-release v0.17.1 on 2026-10-21,
  no licence file in either it or <https://github.com/JustAddMP/core>. `README-dev.md` lists
  `--transport udp|steam` and a generated `player_sync.lua`.
- Mafia Toolkit: <https://github.com/Greavesy1899/MafiaToolkit>, PolyForm Strict 1.0.0. Its archive code is zlib
  ((c) 2017 Rick "gibbed").

Method: pattern scan
- Collected all 394 unique pattern strings passed to `hook::` helpers in MafiaMP master's 462 client source files,
  then counted the matches of each in the exe (Python regex, `?` = any byte). Result: 382 occur once, 12 several
  times, 0 are missing. Whether MafiaMP picks a specific match for the 12 multi-match patterns was not checked.

Derived:
- MafiaMP's reverse engineering very likely matches build 16502642.

## 2026-10-24: executable

Observed:
- PE sections: `.text`, `BINK`, `.interpr`, `.rdata`, `.data`, `.pdata`, `.tls`, `BINKDATA`, `.gfids`, `_RDATA`,
  `.rsrc`, `.reloc`. There is no `.bind`, so no SteamStub.
- Ghidra 12.1.4 headless import: PE loader, `x86:LE:64:default:windows`, TLS callbacks at `0x144b1d900`.
  `MafiaDefinitiveEdition.pdb` (GUID `1caa58cd-95da-4f0f-8938-6ea47e2b591b`, age 1) was not found. RTTI
  analysis logged about 120 "TypeDescriptor … doesn't point to a vfTable" errors. Analysis is still running.
- Strings with a regex for MSVC RTTI names (`.?AV…`) and story keywords gave only 9 hits, so names come from other
  strings. Story and save classes found are listed in the README.
- Mafia III script paths are present in the exe (e.g. `/scripts/entities/interactions/int_kill_the_judge_dealer_1_hold.vi`).
- The `hks` namespace (`HeapVisualizer@hks`) is present.

Unknown:
- What the `.interpr` section is for.

## 2026-10-24: script archives

Method:
- Wrote my own read-only SDS v20 reader in Python, porting the format knowledge from the zlib-licensed Gibbed code.
  Pitfalls found against real files: the 12-byte head and the 52-byte file header are each followed by a 4-byte
  checksum, so the file header starts at offset 16, not 12. Every 34-byte resource header in the block stream is also
  followed by a 4-byte checksum, which is why resource data is `size - 38` bytes. Oodle blocks decompress with the
  game's `oo2core_8_win64.dll` (`OodleLZ_Decompress`, decompressed size known from the block header).

Observed (all 150 archives in `sds_retail/script`):
- Resource types: hkAnimation 15,528; Texture 8,261; Generic 9,249; Cutscene 14; Script 1.
- Generic subtypes by their first u32, named from Mafia Toolkit's `GenericResource.cs` table: `0x64D0E75C`
  `.ires.compiled` 4,326; `0x5B770C22` `.vi.compiled` 4,210; `0xB4347D18` `.entity.compiled` 358; `0xA27F694D`
  `.iprofai` 189; `0x4A336D64` `.iproftime` 150; `0x45F07C8B` `.gxml` 15.
- The only Lua file is `/scripts/missions/statistic_tracking/statistic_tracking.lua`, 19,442 bytes of Lua 5.1
  bytecode. Across all decompressed streams, `\x1bLua` occurs once.

Derived:
- Mission logic is not Lua. My earlier assumption that it was turned out wrong (rule 6). The approach changed to
  syncing story state instead of script calls.

Unknown:
- The `.vi.compiled` format, which runtime interprets it, and whether other `sds_retail` folders hold Lua (not
  scanned).

## 2026-10-24: save location

Observed:
- On the test PC, no folder with a name matching mafia, hangar, 2k or 1030840 existed under `%LOCALAPPDATA%`,
  `%APPDATA%`, LocalLow, Documents (which was redirected to OneDrive), Saved Games, ProgramData or Steam
  `userdata\<id>\1030840` (depth 4). Steam had marked the game as played at the moment of install, but no game-written
  files appeared.
- exe strings: `Hangar 13 Games`, `Mafia Definitive Edition`, `/profiles`, `/profiles/`, `/profiles_b/`,
  `/profiles_xml/`, `%s/%llu`, `SaveSystem`, `SaveManager`, `SaveAsyncWriteHelper`, and `SOFTWARE\2K Games\CrashReport`
  (registry).

Derived (first guess, later corrected):
- I first guessed `<known folder>\Hangar 13 Games\Mafia Definitive Edition\…\profiles\<SteamID64>`. That was wrong,
  see below.

Method: import and reference scan, with no Ghidra (its project was locked by the running analysis)
- Parsed the import table and searched `.text` for RIP-relative ModRM operands (mod=00, rm=101) whose disp32 lands on
  an import slot or a string.
- Folder imports: `SHELL32!SHGetFolderPathA` (IAT `0x144a6f8e8`) and `KERNEL32!ExpandEnvironmentStringsA`. There is no
  `SHGetKnownFolderPath`.

Observed:
- `SHGetFolderPathA` is called 5 times. At `0x1406eb87d` the folder is `lea edx,[r9+0x1a]` (`CSIDL_APPDATA`), and
  that call is not near any save string. At `0x14240d440`, `0x1424196ca`, `0x14241a642` and `0x14241a892` it is
  `xor r9d,r9d; lea edx,[r9+5]` (`CSIDL_PERSONAL`, Documents).
- Strings used near those calls: `/edit`, `My Games`, `Mafia Definitive Edition`, `\Saves` and `/gamesaves`
  (`0x14240d300`-`0x14240d800`). Also `%s%s\%s\Saves\launcconfig.cfg` and `%s%s\%s\Saves\videoconfig.cfg`, each
  with `My Games` and `Mafia Definitive Edition` (`0x1424196a0`, `0x14241a600`).
- `Hangar 13 Games` (`0x144b1f028`) has a single reference, at `0x14374209b`, outside the save code.

Derived:
- Saves and the launcher and video settings live in `<Documents>\My Games\Mafia Definitive Edition\Saves\`. The
  exact separators used by the format strings were not traced.
- The folder does not depend on the install path, so a test copy shares it with the real game. If Documents is
  redirected to OneDrive, as on the test PC, the saves sync to the cloud.

Unknown:
- What goes below `Saves` (`/profiles/`, `%s/%llu` per Steam ID?). To be checked on the first guarded launch, after
  redirecting `CSIDL_PERSONAL` for the test copy.

## Workspace pitfalls

- A hard-linked "test copy" is not isolated. An earlier copy on the test PC turned out to have 4,843 of its files
  hard-linked to the store install, so any write into it would have changed the real game. Check with
  `fsutil hardlink list` before trusting a copy.

## 2026-10-24: story API (static, Ghidra)

Method:
- Ghidra 12.1.4 full auto-analysis (about 2 h), then headless scripts of my own: string cross-references with
  decompilation of each referencing function, and a scan of `.rdata`/`.data` for arrays of
  `{pointer to ASCII name, pointer into .text}` pairs ending in `{0, 0}`.

Observed:
- Story class names (`C_MissionCloseState`, `C_Quest`, `C_PlotlinesManagerSaveBlock`, `C_GameDirectorGRD`, …) are
  only used by reflection type registration (for example `C_MissionCloseState` hash `0x2aaa22d5`). They do not lead
  to the managers' code.
- `0x143812940` is a generic script-wrapper type check with 588 call sites, so game objects are exposed to
  HavokScript.
- 436 `luaL_Reg`-style tables, 5,800 entries. Story functions and addresses are in the README. The quest functions
  call a global object at `0x14635a740` (+8 interface) through its vtable: `QuestSetActive` → vt+0x70,
  `SetCurrentAct` → vt+0x1a8. `SetCurrentStoryId` → getter `0x142d61440`, then `0x142d6e330(mgr, id, -1)`.
  `SetMissionState` → getter `0x142438260`, then `0x142438730(mgr, id, bool)`.
- The first 24 bytes are unique only for `GameOver`, `SetCurrentAct`, `SetCurrentSubAct`, `SetCurrentStoryId`,
  `SetMissionState`, `Retry` and `CrossedCheckPoint`.

Guess:
- `0x14635a740` is `C_GameDirector` or its quest interface. Not confirmed.

## 2026-10-24: probe DLL and guarded launches

Method:
- The exe imports `VERSION.dll` (6 functions), so a `version.dll` proxy in the test copy loads first. It forwards the
  17 version exports to the system DLL and patches the exe's import table.
- `SHGetFolderPathA` for `CSIDL_PERSONAL`, `CSIDL_APPDATA` and `CSIDL_LOCAL_APPDATA` returns a per-instance folder.
  The probe exits the game if the folder is not set.
- A harness snapshots, before and after each run: the real install's file list and exe hash, `Documents\My Games`,
  Saved Games, Steam `userdata` (recursive), the top level of Documents, AppData, LocalAppData, LocalLow and the user
  profile, and HKCU `Software\2K Games` and `Software\Hangar 13`.

Observed (first run, online, 120 s, no input):
- The redirect worked (calls for 0x1a and 0x5). The game created `Documents\My Games\Mafia Definitive Edition\Saves\`
  (`launcconfig.cfg`, `videoconfig.cfg`) and `Data\<SteamID64>\profiles\temporaryprofile\*.pf` there.
- Guard: install and exe hash unchanged. Only Steam client bookkeeping changed: `config\localconfig.vdf`,
  `config\librarycache\<appid>.json`, `gamerecordings\gamerecording.pb`. No Steam remote-storage folder appeared.
- No `GetProcAddress` for Steam functions, so the Steam API is linked statically.
- The window was 1280×720 windowed. The 960×540 written to `videoconfig.cfg` beforehand was not used.
- **The game's HTTP library wrote verbose HTTPS traffic to standard output, including a 2K single sign-on bearer
  token.** It contacted `*.my.2k.com` and `sso`, `telemetry` and `promotions` hosts under `api.2kcoretech.online`,
  and posted to an SSO endpoint by itself. Keep the game's stdout in a private file, never in shared logs.

Method (offline mode, all later runs):
- Import hooks on `getaddrinfo`, `gethostbyname`, `connect` and `sendto` (ws2_32 imports by ordinal 52, 4 and 20)
  fail non-loopback targets.

Observed:
- Offline, the main menu is reached after a "cannot connect to 2K" dialog. No tokens were logged.

## 2026-10-24: bot input

Observed:
- Posted `WM_KEYDOWN`/`WM_KEYUP` have no effect. The game reads the keyboard through DirectInput 8 and pads through
  XInput: the import plus run-time `GetProcAddress` of `XInputGetCapabilities` and `XInputSetState`.
- Hooking only the XInput imports was not enough: the UI ignored the virtual pad until the probe also returned its
  hooks from `GetProcAddress`.
- The UI follows the real cursor, and moving the mouse switched it to keyboard-and-mouse prompts. Pinning
  `GetCursorPos` to the window centre and ignoring `ClipCursor` fixed it.
- Cutscenes paused (with a keyboard skip prompt) when the window lost focus. Subclassing the window procedure fixed
  it: `WM_ACTIVATEAPP` and `WM_ACTIVATE` forced active, `WM_KILLFOCUS` swallowed.
- The game polls `XInputGetState` about 120 times a second in menus and about 60 in a mission.
- Start flow, offline: intros (A skips after about 20 s), title screen (A), "connecting", "cannot connect to 2K"
  (A), main menu (Continue, Story, Free Ride, …). A scripted path from launch into the profile's mission passed
  several times, about 97 s from launch.
- Tapping A, holding A for 1.5 s and pressing B do not skip cutscenes.

Unknown:
- The pad binding for skipping cutscenes.

## 2026-10-24: chapter 1 call sequence

Method:
- The probe wraps 579 functions from 8 tables (`0x145ad0780`, `0x145af5810`, `0x145af7b50`, `0x145ae3b20`,
  `0x145ae3450`, `0x145af5d20`, `0x145af45d0`, `0x145af9720`), metamethods excluded. Each entry is swapped only if its name pointer and function pointer match this build: all
  579 matched. Logging is limited to 8 calls per function per 5 s, with call counts.
- A string argument's object layout was read from live values: u64 length at +0x08, u32 hash at +0x10, text at
  +0x14 (`mm_10_chase`: length 0x0b, hash `b96a5469`).

Observed:
- The sequence in the README, from Continue on a profile whose autosave is at the start of chapter 1. The idle bot
  reached the first checkpoint, `cp_020`, and died about 27 s later in the car chase.
- Earlier in the same log, when the main menu opens: `RegisterScriptedMenuEvent("MainMenu", 0..7, …)`.

Derived:
- Mission start, checkpoint, objectives and death each have their own call with string ids, so they can be replayed
  on a client.

## 2026-10-24: chapter replay unlock

Method:
- Cross-references to `MissionReplayUnlocked`, `PersistentDataStorage`, `S_VariableNumber` and others.

Observed:
- The Chapter Replay list (`0x142fe01b0`) reads the number `MissionReplayUnlocked` from the script data store and
  marks entry *i* unlocked if *i* is below it. The store group comes from the `PersistentDataStorage` save block
  (`0x1423632f0`, group key at `0x14635f3b0`).
- Names are hashed by `0x143cbae50`: FNV-1 64-bit (multiply by `0x100000001b3`, then xor), offset basis
  `0xcbf29ce484222325`. A name made only of digits (not starting with 0) is parsed as a decimal number instead.
- `.pf` profile files are XML followed by 4 bytes. For all 11 files in the test profile, those bytes are the CRC32 of
  the XML, little endian.
- Variable classes: `S_VariableNumber` id `0x48c8e903`, `S_VariableString` `0x48c8e904` (`m_Type` 3 in a saved
  file), `S_VariableBool` `0x48c8e905`.
- After adding `MissionReplayUnlocked` = 99 (`S_VariableNumber`, `m_Type` 2) to the test profile's
  `profile_data_storage_persistantdata.pf` and fixing the CRC, the menu Story → Chapter Replay listed all 20
  chapters. Chapter 2 loaded at once and started mission `mm_20_taxi`.

Guess:
- `m_Type` 2 = number, from the id order. The game accepted it, but the full enum was not read.

## 2026-10-24: all chapters from Chapter Replay

Method:
- One guarded, offline launch per chapter: Story → Chapter Replay → entry *N*, press A on the loading screen's
  "Continue" prompt, record until the first `QuestSetCustomState` plus 45 s (or a death screen, or 15 min), stop,
  compare snapshots. Calls are summarised from the probe log (queries and per-frame reads dropped).

Observed:
- All 20 entries recorded; the table is in the README. Every chapter loads exactly one `mm_*` mission.
- Loading from Chapter Replay ends on a loading screen that waits for "Continue". Loading through the main menu's
  Continue did not.
- Bot pitfalls, each found from a wrong chapter or a stuck run:
  - The "cannot connect to 2K" dialog does not appear on every offline start. Blind, timed presses then hit
    Continue on the main menu and load the autosave.
  - A press held 0.4 s carried over from the dialog into the main menu (screenshots: connecting, dialog, then the
    autosave loading).
  - In the Chapter Replay list, one DOWN sometimes moved two rows: 7 presses of 0.15 s moved 11 rows. The list
    always opens on row 1.
  - Working method: check the screen before pressing. The main menu's gold title (top left) is present on the
    main menu and Story submenu only, and the selection box's left border marks the highlighted list row.
- Running the batch while another game ran fullscreen on the same PC: the test copy stopped responding (Windows
  reported `AppHangB1`) about 18 s after that game started, and on a second try it exited silently. The batch
  needs the PC to itself.
- Each game launch re-stamps Steam's Notes cache (`userdata\<id>\2371090\remotecache.vdf`, app 2371090), with the
  same content. The guard treats that file as expected bookkeeping.

Unknown:
- Where `mm_90_funeral` is played: no Chapter Replay entry loaded it.
- The checkpoints after the first few in each mission, and the calls at mission end.
