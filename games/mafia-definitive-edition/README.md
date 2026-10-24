# Mafia: Definitive Edition (2020)

| | |
|---|---|
| Engine | Illusion Engine (Hangar 13); shares code with Mafia III (see below) |
| Versions tested | Steam app 1030840, build 16502642, `mafiadefinitiveedition.exe` FileVersion 1.0.0.1, SHA-256 `34B06C6C11326E51001EF2FD82933C7F200ACAD27EDBFCC0C159A7051978675E`, Windows 10 x64. Static analysis, plus bot-driven launches of an isolated, offline test copy with a logging probe DLL |
| Projects | MafiaMP <https://github.com/MafiaHub/MafiaMP> (free roam); MafiaCoop (JustAddMP) <https://github.com/JustAddMP/MafiaDE> (story co-op, pre-release); both in [DIRECTORY.md](../../DIRECTORY.md). This write-up's own work: story co-op from zero, claim [#173](https://github.com/cranebeaker/coop-anything/issues/173) |
| QA level | none |
| Last updated | 2026-10-24 |

## Where things stand

Two projects exist. MafiaMP is free roam on a dedicated server, without the story. MafiaCoop (JustAddMP) says the
story follows the host, but it is a pre-release that its author has tested only on one PC, in a few chapters. This
write-up covers a third attempt, story co-op built from zero. It is past static analysis: a probe DLL in a test copy
logs the game's story script calls while a bot plays, and the calls that start a mission, set a checkpoint and change
objectives are identified. No co-op code exists yet.

Research log with sources, and observed, derived and unknown facts kept apart: [NOTES.md](NOTES.md).

## Existing work

- **MafiaMP** (<https://github.com/MafiaHub/MafiaMP>, MafiaHub; last commit 2026-10-15, nightly pre-release v2.5.1
  from 2025-12-21): free-roam multiplayer on the MafiaHub Framework, with a dedicated server, a server scripting API
  and 34 open issues and pull requests. Missions and story are not synced. The client hooks the game with MinHook and IDA-style byte
  patterns. **All 394 of its client patterns occur in build 16502642, 382 of them exactly once** (scan of the exe,
  2026-10-24), so its reversed SDK (`code/client/src/sdk`: humans, vehicles, traffic, camera, streaming, game
  director) very likely matches this build. Licence: MafiaHub OSS, whose clause 5 allows derivative work only to
  contribute back to MafiaMP and needs written permission for personal use. Use it as a reference, not as a base.
- **MafiaCoop (JustAddMP)** (<https://github.com/JustAddMP/MafiaDE>, game-agnostic part
  <https://github.com/JustAddMP/core>; repo created 2026-10-08, latest pre-release v0.17.1 on 2026-10-21): one DLL
  and a launcher for build 16502642. The README says chapter, checkpoint, objectives and cutscenes follow the host,
  and that since 0.17.0 the host runs traffic, police, pedestrians and enemies. The author tested story co-op in
  Omerta, Taxi and Bon Appetit, on one PC with two game copies; two PCs are untested. Transport is UDP or Steam
  (`--transport udp|steam`), joined through Steam invites or by IP. Neither repo has a licence file, so no reuse
  rights are granted. Not run here.
- **Nomad Group ScriptHook (+Trainer)**: runs custom Lua in the game. The listing dates from 2020-12, and its
  current-build status is unknown
  (<https://www.gamepressure.com/download/mafia-definitive-edition-mafia-definitive-edition-scripthook-tra/ze12c26>).
  MartinJK's earlier Script Hook is discontinued
  (<https://www.gamepressure.com/download/mafia-definitive-edition-script-hook-v10-mod/z812a13>).
- **Mafia Toolkit** (<https://github.com/Greavesy1899/MafiaToolkit>, release 2.36 from 2026-07-09): reads Mafia DE
  SDS archives. The toolkit itself is PolyForm Strict 1.0.0 (source-available, non-commercial use only, no
  distributing changes), but its archive code (`Gibbed.Mafia2.FileFormats`, `Gibbed.Illusion.*`, (c) 2017 Rick
  "gibbed") is under the zlib licence.

## How the game works (for co-op)

- **Executable**: x64 MSVC, 111 MB, about 76 MB of code. No SteamStub wrapper: there is no `.bind` section, so the
  exe decompiles directly. It has an unusual `.interpr` section, purpose unknown. The PDB (`MafiaDefinitiveEdition.pdb`)
  is not shipped. Most MSVC RTTI type names are missing, but class names survive in reflection and serialization
  strings. The Steam API is linked statically (the game never loads `steam_api64.dll`).
- **Shared with Mafia III**: the exe contains Mafia III script paths, for example
  `/scripts/entities/interactions/int_kill_olivia_game_room_guard_l_hold.vi` and `int_kill_the_judge_*`, so Mafia III
  research largely carries over.
- **Scripting**: the engine embeds HavokScript (namespace `hks`), a Lua 5.1-compatible VM. **Missions are not Lua
  files, though.** The 150 archives in `sds_retail/script` hold 9,249 Generic resources: `.ires.compiled` 4,326,
  `.vi.compiled` 4,210, `.entity.compiled` 358, `.iprofai` 189, `.iproftime` 150 and `.gxml` 15 (extension names from
  Mafia Toolkit's hash table). There are also 15,528 animations, 8,261 textures, 14 cutscenes and **exactly one Lua
  script**, `/scripts/missions/statistic_tracking/statistic_tracking.lua` (Lua 5.1 bytecode). The `.vi.compiled`
  format is undocumented. The compiled mission scripts still drive the story through the game's Lua C API, so that
  API is where story changes can be seen and replayed (below).
- **Missions** ship as one archive set each: `mm_<n>_<name>_p` plus sub-area archives `mm_<n>_<name>_s_*`. Examples
  are `mm_20_taxi_p`, `mm_110_omerta_p` with `mm_110_omerta_s_bank`, and `mm_200_bank_p`. Open-world content
  includes `lh_trap_manager_*` and `lh_garage_quest_loc_*`, alongside `game_manager` and `game_over_manager`. In
  game, chapter 1 runs as mission `mm_10_chase` and chapter 2 as `mm_20_taxi`.
- **SDS v20 archive layout** (worked out from the zlib-licensed Gibbed code and checked against these files): a
  12-byte head (`SDS\0`, version 20, `PC\0\0`), then a 4-byte checksum. Next comes a 52-byte file header (type-table
  offset, block-table offset, XML offset, RAM and VRAM sizes, flags, 16 unknown bytes, resource count) with its own
  checksum. The resource-type table follows (id, u32-length name, parent). The data is a `zlEU` block stream:
  alignment 0x10000 with flag 0x1000000 for Oodle, compressed blocks have a 128-byte header, and Oodle decompression
  goes through the game's own `oo2core_8_win64.dll`. Each resource is a 34-byte header (type, size, version, u64
  name hash, 4×u32 memory) plus a 4-byte checksum, followed by `size - 38` bytes of data. Generic resources begin with
  a u32 type hash and carry a `GENR` tag.
- **Script API**: 436 `luaL_Reg`-style tables (`{const char *name, lua_CFunction fn}`) in `.rdata`/`.data`, 5,800
  functions in all. The story-related ones for this build:

  | Table | Functions (address) |
  |---|---|
  | Quests `0x145ad0780` (72) | `QuestSetActive` `0x142245e30`, `QuestSetFinished` `0x142246010`, `QuestSetCustomState` `0x142246e60`, `ObjectiveActivate` `0x142242cc0`, `SetCurrentAct` `0x142244210`, `SetCurrentSubAct` `0x142244470`, `GameOver` `0x142245970`, `CityTransfer` `0x142245af0` |
  | Saves `0x145af5810` | `EnableSaveLogic` `0x142e56940`, `RequestCheckpointSave` `0x142e56ef0`, `RequestMissionSave` `0x142e56f40`, `Retry` `0x142e570b0` |
  | HUD `0x145af7b50` (237) | `AddQuestChapter` `0x142f51880`, `AddQuestObjective` `0x142f54590`, `ShowMissionComplete` `0x142f4db40`, `ShowMissionRestart` `0x142f4d830`, `AcceptMissionRestart` `0x142f5bec0` |
  | `0x145ae3450` (purpose unclear) | `SetMissionState` `0x142677270` |
  | Game `0x145ae3b20` | `SetCurrentStoryId` `0x142683340`, `GetMissionTime` |

  Most functions are methods (argument 1 is the wrapper userdata). The prologues of the Lua glue functions repeat,
  so byte signatures are not unique. Resolve them at run time by name from the registration tables instead. The
  quest functions call a global manager at `0x14635a740` through its vtable (for example `QuestSetActive` → vt+0x70);
  that it is `C_GameDirector` is a guess.
- **HavokScript values** (confirmed in the running game): `lua_State` +0x48 top, +0x50 base; stack values are 16
  bytes, a u32 tag at +0 and the value at +8. Tags: 1 boolean, 3 number (float), 4 string, 7 userdata (the `self`
  wrappers). A string object holds a u64 length at +0x08, a u32 hash at +0x10 and the text at +0x14.
- **Story calls in chapter 1**, logged from a bot run (queries and per-frame reads left out; times are seconds after
  launch):

  | Time | Calls | In game |
  |---|---|---|
  | 96.6 | `EnableSaveLogic("main")`, `RequestMissionSave("mm_10_chase")`, `SetMissionState("mm_10_chase", true)`, `UpdateRolloverDateAndImage("$mm_10_chase_date", "mission01")` | mission starts loading |
  | 108.5 | `CreateScriptActorAction` ×4, `RegisterVehicleDirect("enemy")` | actors set up |
  | 108–318 | none | opening cutscene |
  | 318.7 | `ShowTitleCard("$title_card_mm_010_…", …)`, later `HideTitleCard` | chapter title card |
  | 384.8 | `SetRespawnPosDir`, `QuestSetCustomState("mm_10_chase", "cp_020")`, `UpdateSimpleObjective("$mm_10_chase_cp_020", …)`, `AddSecondaryObjective("$mm_10_chase_cp_030_…", false, 1)`, `OpenScotchTutorial("tutorial_accelerate", …)` | gameplay starts at a checkpoint |
  | 396.8 | `RegisterIconEntityDirect("traps")`, `AddEntityIndicator("vehicle_trap")` | marker on the gangsters' car |
  | 400.8 | `ZombieShotAtEntityStart`/`Stop`, `ModifyGameTime(0.3, 1000)` | scripted shots, slow motion |
  | 411.7 | `ShowMissionRestart("mm_10_chase", "mm_000_go_death")` | "you died" screen (the bot does not drive) |

  Every frame in gameplay, scripts call `GetActivePlayer` about 60 times and `GetMissionTime` 120–240 times a second.
  The derived sync channel: mission start is `SetMissionState(<mission>, true)`, the checkpoint is
  `QuestSetCustomState(<mission>, "cp_NNN")`, objectives are `UpdateSimpleObjective`/`AddSecondaryObjective` with `$`
  text ids, and death is `ShowMissionRestart(<mission>, <reason>)`.
- **All 20 chapters**, loaded one by one from Chapter Replay by a bot and recorded until shortly after the first
  gameplay checkpoint (one launch per chapter; the bot does not play, so later checkpoints are not covered):

  | Chapter (list position) | Mission | Title card | Checkpoints seen | First objective text id |
  |---|---|---|---|---|
  | 1 | `mm_10_chase` | `$title_card_mm_010_…` | `cp_015`, `cp_020` | `$mm_10_chase_cp_020` |
  | 2 | `mm_20_taxi` | `$title_card_mm_020_title` | `cp_020` | `$mm_20_taxi_cp_010` |
  | 3 | `mm_30_molotov` | `$title_card_mm_030_title` | `cp_020` | `$mm_30_molotov_cp_020_look_around` |
  | 4 | `mm_40_motel` | `$title_card_mm_040_title` | `cp_010`, `cp_020` | `$mm_40_motel_cp_005` |
  | 5 | `mm_50_race` | `$title_card_mm_050_title` | `cp_020`, `cp_030`, `cp_040`, `cp_050` | `$mm_50_race_cp_030` |
  | 6 | `mm_60_sarah` | `$title_card_mm_060_title` | `cp_020` | `$mm_60_sarah_cp_020` |
  | 7 | `mm_70_hoodlums` | `$title_card_mm_070_title` | `cp_020` | `$mm_70_hoodlums_cp_020` |
  | 8 | `mm_80_brothel` | `$title_card_mm_080_title` | `cp_015`, `cp_020` | `$mm_80_brothel_cp_015` |
  | 9 | `mm_100_farm` | `$title_card_mm_100_title` | `cp_015`, `cp_020`, `cp_030` | `$mm_100_farm_cp_015` |
  | 10 | `mm_110_omerta` | `$title_card_mm_110_title` | `cp_020`, `cp_030` | `$mm_110_omerta_cp_020` |
  | 11 | `mm_120_mansion` | `$title_card_mm_120_title` | `cp_020` | `$mm_120_mansion_cp_010` |
  | 12 | `mm_130_parking` | `$title_card_mm_130_title` | `cp_020`, `cp_030` | `$mm_130_parking_cp_010` |
  | 13 | `mm_140_salieri` | `$title_card_mm_140_title` | `cp_020` | `$mm_140_salieri_cp_020` |
  | 14 | `mm_150_boat` | `$title_card_mm_150_title` | `cp_020`, `cp_030` | `$mm_150_boat_cp_010` |
  | 15 | `mm_160_harbor` | `$title_card_mm_160_title` | `cp_015`, `cp_020` | `$mm_160_harbor_cp_020` |
  | 16 | `mm_170_plane` | `$title_card_mm_170_title` | `cp_020` | `$mm_170_plane_cp_020_1` |
  | 17 | `mm_180_sniper` | `$title_card_mm_180_title` | `cp_020` | `$mm_180_sniper_cp_010` |
  | 18 | `mm_190_cigars` | `$title_card_mm_190_title` | `cp_020`, `cp_030`, `cp_040` | `$mm_190_cigars_cp_020` |
  | 19 | `mm_200_bank` | `$title_card_mm_200_title` | `cp_050`, `cp_060` | `$mm_200_bank_cp_010` |
  | 20 | `mm_210_gallery` | `$title_card_mm_210_title` | `cp_020` | `$mm_210_gallery_cp_020` |

  Every chapter is one mission, started with `SetMissionState("<mission>", true)`, then a title card
  `$title_card_mm_<NNN>_…`. Checkpoints are always `QuestSetCustomState("<mission>", "cp_NNN")`, and objective ids
  follow `$<mission>_cp_NNN…`, so a client needs only the mission name and checkpoint id to follow the host. The
  first objective can belong to an earlier checkpoint than the first `QuestSetCustomState` (for example `cp_010`
  before `cp_020`), so the mission start acts as an implicit first checkpoint. Chapter 19 starts at `cp_050`.
  Archive names in `sds_retail` also include `mm_90_funeral`, which no Chapter Replay entry loaded, and
  `mm_03_morello`, `mm_010_bridge`, `mm_020_prologue`, `mm_040_chase`, `mm_070_acetylene` and `mm_180_prison`;
  what they are used for was not checked.
- **Saves**: the save system writes named blocks, among them `S_QuestsManagerSaveData`,
  `S_PlotlinesManagerSaveData`, `S_DialogStateManagerSaveData`, `S_JournalSaveData`, `S_StreamingWorldStateSaveData`,
  `C_PoliceForceSaveBlock` and `S_GameActivitiesSpawnSystemSaveData`. Everything goes under
  `<Documents>\My Games\Mafia Definitive Edition\` (Documents from `SHGetFolderPathA(CSIDL_PERSONAL)`, seen on disk
  in a redirected test instance): `Saves\launcconfig.cfg` and `Saves\videoconfig.cfg`, the profile in
  `Data\<SteamID64>\profiles\temporaryprofile\*.pf`, and game saves in `Data\<SteamID64>\gamesaves\default\aslot\save<n>.sav`
  (binary, `GENR` header). The folder does not depend on the install path, so every copy of the game shares it, and
  if Documents is redirected to OneDrive, so are the saves.
- **Profile files** (`*.pf`): XML (`C_SaveDataUnwrapped` with save blocks) followed by a 4-byte trailer, the CRC32
  of the XML (little endian). Script data storage maps a `C_HashName` (FNV-1 64-bit of the name, multiply then xor;
  all-digit names are parsed as numbers) to an `S_Variable*`; `m_Type` is 2 for `S_VariableNumber` and 3 for
  `S_VariableString`. The Chapter Replay menu (`0x142fe01b0`) shows chapter *i* as playable if *i* is below the
  number `MissionReplayUnlocked` in the `PersistentDataStorage` block, stored in
  `profile_data_storage_persistantdata.pf`. Setting it in a test profile unlocks all 20 chapters; each loaded
  directly, with no prompt, ending on a loading screen that waits for "Continue". So both players should be able to
  load the same chapter without playing up to it (derived, not tested with two instances).
- **Input**: keyboard and mouse through DirectInput 8, pads through XInput (imported and also looked up with
  `GetProcAddress` at run time). Window messages posted to the game have no effect. The UI follows the real cursor
  (it switches to mouse prompts when the cursor moves) and calls `ClipCursor`. Cutscenes pause while the window is
  inactive; the game reacts to `WM_ACTIVATE`/`WM_ACTIVATEAPP`. Tapping or holding A, or pressing B, does not skip
  cutscenes; the skip binding is unknown. For bots: a button held 0.4 s carries over into the next screen (an A
  that closes the "cannot connect to 2K" dialog then also hits Continue on the main menu), and in the Chapter Replay
  list one DOWN sometimes moves two rows, even with 0.15 s presses. Verify the highlighted entry before selecting.
  The "cannot connect to 2K" dialog does not appear on every offline start.
- **Online services**: on start the game contacts `*.my.2k.com` and `*.api.2kcoretech.online` (single sign-on,
  telemetry, promotions). Its HTTP library writes verbose request logs, **including a bearer token**, to standard
  output. When those lookups fail, it shows a "cannot connect to 2K" dialog and runs normally offline.
- **Single instance and netcode**: the exe imports `CreateMutexA`/`OpenMutexA`; whether it refuses a second instance
  is not tested. The game is single player, and whether any engine-level network code remains is unknown.

## What we changed

Nothing in the game files. Tools written so far (not published yet):

- A read-only SDS v20 script extractor and a byte-pattern scanner.
- Ghidra headless scripts: string cross-references with decompilation, and `luaL_Reg` table finding.
- A probe DLL loaded as a `version.dll` proxy, in the test copy only. It redirects `SHGetFolderPathA` to a
  per-instance folder (and exits if it can't), blocks non-loopback network lookups and connects, provides a virtual
  XInput pad fed from a text file, pins the cursor, keeps the window "active", and wraps 579 Lua C functions from
  8 tables. Each wrap swaps a table pointer only if the entry's name and address match this build. It logs calls
  with decoded arguments, rate-limited per function.
- A launch harness: it snapshots the real install, the exe hash, `Documents\My Games`, Saved Games, Steam
  `userdata` and the 2K/Hangar 13 registry keys before and after each run and reports any change. A watchdog kills a
  game started from the real install. Bot scripts drive the menus to any chapter, checking the screen before each
  press (main-menu title present, highlighted row in the chapter list) from window screenshots.
- A `.pf` editor that sets a number variable and fixes the CRC (refuses files whose CRC is already wrong).

## QA

None for co-op: no co-op code exists. Probe and harness runs so far (36 guarded launches of the test copy, all
offline except the first): every run's guard left the real install, exe hash and saves unchanged. Only Steam client bookkeeping
changed (`localconfig.vdf`, `librarycache\*.json`, `gamerecording.pb`, and the Steam Notes cache
`userdata\<id>\2371090\remotecache.vdf`, re-stamped with unchanged content). One run also caught another game's
Steam files, from a game started on the PC during the run.

## How to play

Not playable from this work yet. For playing today, see the two projects under Existing work.

## Known issues

None, because nothing ships yet.

## Next steps

1. Record each chapter past its first checkpoints: list every `cp_NNN` per mission and the calls at mission end
   (`ShowMissionComplete`, the next mission's start). Needs a driving and walking bot, or a way to jump to a
   checkpoint.
2. Hook below the Lua glue: the manager behind the quest functions (`0x14635a740`), `SetMissionState` →
   `0x142438730`, and the save requests. Confirm that native code changes story state only through them.
3. Let the host own the story. Send mission start, checkpoint, objective and restart calls from host to client and
   replay them there with a re-entrancy guard. Suppress the client's own calls for those functions.
4. For late join, load the host's chapter on the client through Chapter Replay, then apply the host's checkpoint.
   For resync, use the host's save blocks (quests, plotlines, journal, dialog state, streaming world state) as a
   snapshot.
5. Test two instances on one PC (each with its own redirected Documents folder) with bots, before two PCs.
