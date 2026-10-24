# Mafia: Definitive Edition (2020)

| | |
|---|---|
| Engine | Illusion Engine (Hangar 13); shares code with Mafia III (see below) |
| Versions tested | Steam app 1030840, build 16502642, `mafiadefinitiveedition.exe` FileVersion 1.0.0.1, SHA-256 `34B06C6C11326E51001EF2FD82933C7F200ACAD27EDBFCC0C159A7051978675E`, Windows 10 x64. Static analysis only: the game has not been launched for this work |
| Projects | MafiaMP <https://github.com/MafiaHub/MafiaMP> (free roam); MafiaCoop (JustAddMP) <https://github.com/JustAddMP/MafiaDE> (story co-op, pre-release); both in [DIRECTORY.md](../../DIRECTORY.md). This write-up's own work: story co-op from zero, claim [#173](https://github.com/cranebeaker/coop-anything/issues/173) |
| QA level | none |
| Last updated | 2026-10-24 |

## Where things stand

Two projects exist. MafiaMP is free roam on a dedicated server, without the story. MafiaCoop (JustAddMP) says the
story follows the host, but it is a pre-release that its author has tested only on one PC, in a few chapters. This
write-up covers a third attempt, story co-op built from zero, which is still at research and static analysis: no
mod code exists yet and the game has not been launched for it.

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
  strings.
- **Shared with Mafia III**: the exe contains Mafia III script paths, for example
  `/scripts/entities/interactions/int_kill_olivia_game_room_guard_l_hold.vi` and `int_kill_the_judge_*`, so Mafia III
  research largely carries over.
- **Scripting**: the engine embeds HavokScript (namespace `hks`), a Lua 5.1-compatible VM. **Missions are not Lua,
  though.** The 150 archives in `sds_retail/script` hold 9,249 Generic resources: `.ires.compiled` 4,326,
  `.vi.compiled` 4,210, `.entity.compiled` 358, `.iprofai` 189, `.iproftime` 150 and `.gxml` 15 (extension names from
  Mafia Toolkit's hash table). There are also 15,528 animations, 8,261 textures, 14 cutscenes and **exactly one Lua
  script**, `/scripts/missions/statistic_tracking/statistic_tracking.lua` (Lua 5.1 bytecode). The `.vi.compiled`
  format is undocumented.
- **Missions** ship as one archive set each: `mm_<n>_<name>_p` plus sub-area archives `mm_<n>_<name>_s_*`. Examples
  are `mm_20_taxi_p`, `mm_110_omerta_p` with `mm_110_omerta_s_bank`, and `mm_200_bank_p`. Open-world content
  includes `lh_trap_manager_*` and `lh_garage_quest_loc_*`, alongside `game_manager` and `game_over_manager`.
- **SDS v20 archive layout** (worked out from the zlib-licensed Gibbed code and checked against these files): a
  12-byte head (`SDS\0`, version 20, `PC\0\0`), then a 4-byte checksum. Next comes a 52-byte file header (type-table
  offset, block-table offset, XML offset, RAM and VRAM sizes, flags, 16 unknown bytes, resource count) with its own
  checksum. The resource-type table follows (id, u32-length name, parent). The data is a `zlEU` block stream:
  alignment 0x10000 with flag 0x1000000 for Oodle, compressed blocks have a 128-byte header, and Oodle decompression
  goes through the game's own `oo2core_8_win64.dll`. Each resource is a 34-byte header (type, size, version, u64
  name hash, 4×u32 memory) plus a 4-byte checksum, followed by `size - 38` bytes of data. Generic resources begin with
  a u32 type hash and carry a `GENR` tag.
- **Story state**: the exe names the story systems. Missions: `C_GameDirector`, `C_GameDirectorScriptWrapper`,
  `C_MissionsDatabase`, `C_Mission` with `C_MissionWaitForOpenState`, `C_MissionOpenDuringReloadState` and
  `C_MissionCloseState`. Quests and objectives: `C_QuestsManager`, `C_QuestsDatabase`, `C_Quest`,
  `C_ObjectivesManager`, `C_ObjectiveScriptModule`. Plotlines: `C_PlotlinesManager`, `C_PlotlinesDatabase`,
  `C_PlotlinesCmdLineInspector`. Also `C_DialogStateManager`, `C_StreamingWorldState`, `C_ScriptMachine` and
  `C_GameActivity`. Their addresses and functions are not mapped yet.
- **Saves**: the save system writes named blocks, among them `S_QuestsManagerSaveData`,
  `S_PlotlinesManagerSaveData`, `S_DialogStateManagerSaveData`, `S_JournalSaveData`, `S_StreamingWorldStateSaveData`,
  `C_PoliceForceSaveBlock` and `S_GameActivitiesSpawnSystemSaveData`. Saves and the launcher and video settings go
  to **`<Documents>\My Games\Mafia Definitive Edition\Saves\`**. The exe gets Documents from
  `SHGetFolderPathA(CSIDL_PERSONAL)` (`xor r9d,r9d; lea edx,[r9+5]` before four of its five calls). The same
  functions use `My Games`, `Mafia Definitive Edition`, `\Saves`, `/gamesaves` and
  `%s%s\%s\Saves\launcconfig.cfg` / `videoconfig.cfg`. Found statically and not yet seen on disk: the game had never
  been started on the test PC. The folder does not depend on the install path, so every copy of the game shares it,
  and if Documents is redirected to OneDrive, so are the saves. `Hangar 13 Games` has a single reference outside this
  code, probably the crash reporter. `/profiles/` and `%s/%llu` probably name subfolders below `Saves` (not checked).
- **Netcode**: the game is single player. Whether any engine-level network code is left from development is unknown.

## What we changed

Nothing in the game yet. Tools written so far (not published): a read-only SDS v20 script extractor and a
byte-pattern scanner.

## QA

None yet. No game code exists, so there is nothing to test.

## How to play

Not playable from this work yet. For playing today, see the two projects under Existing work.

## Known issues

None, because nothing ships yet.

## Next steps

1. Finish the Ghidra analysis of the exe. Find each story manager's instance and the functions that change its state
   (quest and objective stage, plotline state, mission open and close), using cross-references from the name strings
   above.
2. Write a probe DLL for an isolated test copy. Log those functions while a bot plays the first mission, to confirm
   which ones fire when the story advances.
3. Let the host own the story. Replicate mission open and close, then quest, objective and plotline changes, from
   host to client, with a re-entrancy guard.
4. For late join and resync, apply the host's own save blocks (quests, plotlines, journal, dialog state, streaming
   world state) on the client as a snapshot.
5. Before the first launch of a test copy, redirect `SHGetFolderPathA(CSIDL_PERSONAL)` per instance (an import-table
   patch, as for The Witness in `playbook/methods.md` §2), and keep Steam remote storage out of the owner's profile.
   Snapshot `<Documents>\My Games\Mafia Definitive Edition` and Steam `userdata` before and after every run.
