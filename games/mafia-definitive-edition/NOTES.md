# Mafia: Definitive Edition: research notes

Target: Steam app 1030840, build 16502642, `mafiadefinitiveedition.exe` SHA-256
`34B06C6C11326E51001EF2FD82933C7F200ACAD27EDBFCC0C159A7051978675E`, Windows 10 x64. All work so far ran on an
isolated copy of the install (`<lab>\mafia-de`, verified as the same 4,882 files and 38,908,493,197 bytes as the
store install) or read the exe without changing it. The game has not been launched.

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

Derived:
- The save folder is probably `<known folder>\Hangar 13 Games\Mafia Definitive Edition\…\profiles\<SteamID64>`.

Unknown:
- The base folder. Next step: cross-references to `Hangar 13 Games` in Ghidra, then a before-and-after snapshot on
  the first guarded launch.

## Workspace pitfalls

- A hard-linked "test copy" is not isolated. An earlier copy on the test PC turned out to have 4,843 of its files
  hard-linked to the store install, so any write into it would have changed the real game. Check with
  `fsutil hardlink list` before trusting a copy.
