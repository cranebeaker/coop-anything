# Fallout: New Vegas - working notes

Notes from an agent session (2026-10-22) that started from a player request: "I want to play
Fallout: New Vegas in co-op with my friends."

Everything below was checked on the machine that ran the session. Paths are written as
`<game dir>` and `%USERPROFILE%`; nothing private goes into this file.

## 1. Triage: the game identity used for everything below

| Field | Value |
|---|---|
| Store | Steam, **Russian/Polish/Czech release**, appid `22490` ("Fallout: New Vegas PCR") |
| Steam buildid | 45574, `UserConfig.language = russian` |
| Exe | `FalloutNV.exe`, file version **1.4.0.525** |
| SHA-256 (exe) | `40d5a7c967a1ec5df1dc19c25f8df3ca74480aa95e8755171567f1129a3001fa` |
| Data | `FalloutNV.esm` + `FalloutNV_lang.esp` (Russian strings live in the `_lang.esp`) |
| DLC | none installed |
| Engine | Gamebryo |

Read from `appmanifest_22490.acf` in the Steam library (`"name" "Fallout: New Vegas PCR"`,
`"installdir" "Fallout New Vegas enplczru"`, `"language" "russian"`).

**This is not the English Steam release (appid 22380).** It matters, because the two FNV projects
both say "Steam or GOG" without ever mentioning the PCR release.

## 2. Finding: fnv-coop already supports the PCR release

`tools/installer/fnv_find.py` in [fnv-coop](https://github.com/cranebeaker/fnv-coop) looks for
installs by appid and by folder name, and both the Russian release and its appid are listed:

```python
STEAM_NAMES = ("Fallout New Vegas", "Fallout New Vegas PCR")
...
for appid in ("22380", "22490"):
```

and the same file defines the only accepted game build:

```python
SUPPORTED_GAME = (1, 4, 0, 525)
```

`inspect_game()` then requires `FalloutNV.exe` to report exactly `1.4.0.525`, a `Data` folder with
`FalloutNV.esm`, and xNVSE `>= 6.3.0`.

The install checked here matches on every point: appid `22490`, folder `Fallout New Vegas enplczru`,
`FalloutNV.exe` `1.4.0.525`, `Data\FalloutNV.esm` present. So the Russian/PCR release is an
**accepted target of the co-op installer**, not an unknown. The localisation is only strings
(`FalloutNV_lang.esp`), and form IDs - what a co-op layer syncs - are not localised, so the
Russian release is not a special case for quest sync.

Neither the fnv-coop write-up in this repo nor `data/directory.yaml` mentions the PCR release at
all. That is worth fixing (see section 6).

## 3. State of the two co-op projects (2026-10-22)

### NV:MP - the only thing a player can use today

* Status: playable, community-tested. Wiki: <https://nuwiki.nv-mp.com/wiki/Getting_Started>.
* Client files: `build_release_client.zip`, **version 9.4** (published 2026-10-09), 89 MB, from
  <https://github.com/NVMP/ClientDistribution/releases> (official NV:MP org).
* Needs: xNVSE (6.3.4 per the wiki, 6.4.9 is current), the 4GB patcher, the client zip.
* Hosting a co-op session for friends: `Launch Local Server` in the launcher -> friends use
  `Connect via IP` with `<ip>:27015`. Port forward UDP+TCP 27015, or Radmin VPN, or plain LAN.
* Requires a Discord account only for the official ATK server and public SDK servers; a private
  local co-op server does not.
* **What it does not do: quests.** No quest stage, objective, global or script state is
  replicated. Quest-flagged actors are deliberately localised on the client, so every player runs
  their own quest log and their own quest NPCs. (fnv-coop's `docs/05-nvmp-intel.md` reverse
  engineering of the 9.4 binaries confirms this: encounter logic localises quest-flagged actors,
  and there is no quest replication API in the server SDK.) The 2016 announcement article says the
  same in one line: "Each player maintains their own quest log, although there will be one host
  player."

### fnv-coop - the story layer, developer preview

* Status: developer preview, no GitHub release. The installer, launcher and player guide exist in
  the tree (`docs/PLAYER-GUIDE.md`, `tools/installer`, `tools/launcher`) but the guide's own step 1
  says "unzip **the release**" and there is no release.
* Actively developed: the tracking issue was updated the day before this session.
* Roadmap state: 10 steps. Steps 1, 3, 4, 5, 6, 7 have code written and in-game testing pending;
  step 2 (enemy sync in the real game) is claimed by another contributor; step 8 (the whole main
  story) has 99.98 % of script calls synced and 65 of 104 Goodsprings stages passing in game.
* It is designed to run **next to NV:MP**: NV:MP gives avatars, enemies and combat, fnv-coop
  gives the shared quest/story state.
* Build from source needs: Python 3.11+, CMake >= 3.20, the VS 2022 generator with `-A Win32`
  (x86 is mandatory), and an xNVSE checkout in `third_party/NVSE` (not bundled; ENet is bundled).

## 4. What this means for a player who wants to play tonight

The honest split:

* **Avatars, combat, the same world, right now** -> NV:MP. It works, it is supported, friends can
  join an hour from now.
* **One shared story** -> fnv-coop only, and only as a build from source that is still being
  finished. Expect to run it as a developer preview, not a polished release.

## 5. What was done in this session (observations)

Everything was done on a **copy** of the game in `<game dir>`; the owner's Steam install and
saves were never touched.

1. **Built fnv-coop from source** (VS 2022, Win32/x86): `nvcoop.dll` plus every lane's test
   executables. The build needs `third_party/NVSE` headers (not in the repository) and a
   `third_party/nvmp` client copy for the L6 signature checks.
2. **Generated the quest data** from the game files:
   `python -I tools/questdata/run_all.py --data "<game dir>\Data"` -> 436 scenarios, 1183 steps,
   12 quests / 104 stages / 73 resolved NPC refs for the Goodsprings milestone.
3. **Packaged a player release** with the repository's own packager
   (`tools/installer/package_release.py`): 27 files, ~4.4 MB, `FNV-CoOp-1.0.0.zip`, SHA-256
   verified, and installed it into the copy with the repository's own installer (which backed up
   what it changed and wrote `nvcoop.ini`).
4. **Ran the repository's full test suite** (`python -I tools/test_all.py`). See the QA section
   of the write-up for the result.
5. **Found and fixed four bugs in fnv-coop** (see section 6).

## 6. Bugs found in fnv-coop (all still present in `main` at `32e12cc`)

Each one is small, has a clear root cause and a test that proves the fix.

| # | File | Bug | Fix |
|---|---|---|---|
| 1 | `tools/test_all.py` | `NEEDS` is defined twice, first with callables and then with file lists. The second wins, so `need[0]` is a tuple and `need[0]()` raises `TypeError: 'tuple' object is not callable` - the run dies instead of skipping the suite whose data is missing. | Keep the file-list table, drop the dead first one, ask `missing_data()` what is missing. |
| 2 | `tools/installer/fnv_find.py` | Steam's `libraryfolders.vdf` / `appmanifest_*.acf` are read as UTF-8 with `errors="replace"`. Steam writes them in the system ANSI code page when that is not UTF-8 (a Russian Windows, cp1251), so every non-ASCII character of a library path becomes U+FFFD, the folder does not exist any more, and the install is never found - with no error anywhere. | Decode UTF-8 first, fall back to the ANSI code page (`fnv_ini.ini_encoding()`). New check plus its mutation twin. |
| 3 | `tools/questdata/run_all.py` | `--data` was forwarded to `effects.py` but not to `m1.py`, so m1 resolved no form signatures: every reference came out without a `sig`, and the in-game dumper's watch list (which needs `sig == "REFR"`) stayed empty. | Forward `--data` to `m1.py` as well. |
| 4 | `tools/questdata/extract.py`, `plugin/dialogue/tools/gen_consent_messages.py` | The US Steam path was hardcoded, so on a regional release (`Fallout New Vegas enplczru`) the tools read nothing and produced empty output. | Find the `Data` folder the way the installer does: `FNV_GAME_DIR`, then the known path, then `fnv_find.candidates()`. |

Fixes live on the branch `fix/portable-paths-and-non-ascii` (three commits) in a fork of
`cranebeaker/fnv-coop`. They are not in `main` yet.

## 7. Blockers found on the machine used for this session

Recorded so the next agent does not rediscover them (no private paths, no personal data):

* The toolchain was missing and had to be installed first: Python 3.12, CMake 4.4, Visual Studio
  2022 Community with the MSVC v143 x86/x64 tools and a Windows SDK, and Git for Windows.
* 8 GB of RAM: two in-game copies of FNV fit, four do not (the project's own split-screen notes
  ask for ~10 GB free RAM for four players).
* **A save is needed before anything world-level can be checked.** The in-game bridge refuses the
  reserved names (`autosave`, `quicksave`, `systemsave*`) and there is no automated "New Game"
  flow, so one human action (start a new game and save it in the copy) unblocks the in-game tests.
* No DLC is installed, so the DLC questline sweeps (roadmap step 9) cannot be run here.

## 8. Proposed follow-up (not done in this session)

1. Add the PCR / Russian release to the FNV write-up and to `data/directory.yaml` notes, so the
   next agent does not assume the English release is the only option. (Done in this PR.)
2. Record the exe SHA-256 of the PCR release next to the English one, for the "record the game's
   identity" rule. (Done in this PR.)
3. Report the four bugs above upstream, with the branch.
4. When fnv-coop ships a real release, say in the write-up that the installer accepts appid
   22490 and that this was checked against the actual files of a PCR install.
