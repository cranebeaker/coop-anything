# Modern Warfare 3 (2011) — session notes

Session date: 2026-10-18. Repo state: zip snapshot of `main` (git/gh installed at end of
session, not yet used). Owner request: "I want to play MW3 (2011) in co-op with friends."

## Game identity (triage.md step 1 / AGENTS.md rule 5)

- Title: Call of Duty: Modern Warfare 3 (2011), Steam
- Steam appids: 42680 (SP: campaign + Spec Ops, buildid 24615914),
  42690 (Multiplayer, buildid 24615796)
- Last updated on owner's PC: 2026-10-03 09:06:47 UTC
- Main executables:
  - `iw5sp.exe` — 5,625,400 bytes, SHA-256 `A97D2BBC…E8DC1023`
  - `iw5mp.exe` — 6,167,104 bytes, SHA-256 `E4E9531D…5784B286`
- Engine: IW 5.0 (closed binary). Layout: `main\*.iwd` archives, `zone\`, `players2\`
- OS: Windows 10 10.0.19045
- Exact local paths: kept in `local/` (private, per AGENTS.md)

## Observations (facts, with sources)

- The repo snapshot contains **zero** coverage of Call of Duty: case-insensitive grep for
  `duty|warfare|treyarch|infinity ward` over the whole repo → 0 matches (checked 2026-10-18).
  → This is a "game nobody covers yet": claim issue + directory entry needed.
- `engine-families.md` has **no row** for the IW/id-Tech-derived CoD engine family. Gap to
  propose in the PR.
- **AlterWare IW5-Mod**: current MW3 client, "mainly focused on Singleplayer/COOP", plus
  "Bot Warfare" in private MP matches, GSC mod support, dedicated servers, security patches
  (https://forum.alterware.dev/t/iw5-mod-install-guide/462, https://alterware.dev/).
  Requires a Steam-owned copy. Org active as of Oct 2026.
- AlterWare client source policy: IW5-Mod is closed-source although "based on work that was
  licensed under the MIT License" (https://forum.alterware.dev/t/why-is-iw5-mod-closed-source/463);
  their GitHub profile README points to a self-hosted Gitea (https://git.alterware.dev/alterware)
  which returned HTTP 500 on fetch 2026-10-18.
- **Plutonium IW5**: multiplayer-only client, account required
  (https://docs.cbservers.xyz/games/iw5; ModDB FAQ below confirms MP-only).
- **iw5_bot_warfare** (GitHub: inedbots): GSC bot mod for PlutoniumIW5 — real AI bots in MP.
  Candidate for the automated-QA "bots drive real clients" requirement.
- **TeknoMW3** (TeknoGods): older client adding LAN/direct-IP for MP and Spec Ops co-op;
  still referenced by users in 2026 (ModDB comment, 2026-03-12). Historically associated
  with piracy-adjacent scenes → legal caution per ground rule 10; prefer AlterWare route.
- **MW3 Survival Mode: Revived** (ModDB, FreakingNewGuy): released 2026-01-09, actively
  updated (files through Oct 2026), requires AlterWare client, Steam-only. Its FAQ states:
  "Does this work with coop? A: No. …connecting to another player's Spec Ops match will
  result in both players experiencing desync."
  (https://www.moddb.com/mods/mw3-survival-mode-revived)
- ModDB file "MW3 Downgrader [FIX ALTERWARE]" (GalvarinoDev v1.2.1, uploaded ~2026-10-18):
  "To fix AlterWare compatibility and play Survival Mode mods again, you may also need to
  uninstall the Modern Warfare…" (truncated by ModDB listing) → a recent Steam update broke
  AlterWare compatibility; a downgrade tool exists. Owner's install was updated 2026-10-03.
- **open-iw5** (GitHub: Rackover): open-source MW3 SP+MP client; author: "Very experimental
  and unfinished. Will probably never be finished."
- Steam guide "How to play Multiplayer / Spec Ops with your friends" (Hamachi) exists —
  implies Spec Ops co-op lobby creation works from the PC version at some point
  (https://steamcommunity.com/sharedfiles/filedetails/?id=2953063348, 2023).
- Safety signal: Reddit thread "[MW3] 2011 Spec Ops DLC/RCE?" warns of a malware risk around
  MW3 2011 lobbies/DLC (content not fetchable — Reddit blocks automated fetch; treat as
  unverified). AlterWare advertises patching "exploits present in Steam versions".
- Wiki: MW3 Spec Ops supports up to 2 players, online and split-screen (console only for
  split-screen) — https://callofduty.fandom.com/wiki/Special_Ops/Modern_Warfare_3

## Derived (my conclusions from the observations)

- Path 1 (finish what exists) = **AlterWare IW5-Mod** for Spec Ops + Survival co-op (2
  players, online, Steam-owned copies on both sides). No code needed from us to get the
  owner playing; our job = verify it on the owner's exact build, document, package, PR.
- **Campaign co-op does not exist for MW3 on any platform and no project attempts it**
  (not found by the searches below). Building it = Path 2 from zero: no SP netcode, closed
  binary, no decomp → very hard (months of agent-hours); the sm64coopdx-style host-authority
  sync model in the playbook applies.
- The Oct-2026 Steam update is the top compatibility risk for the whole AlterWare route;
  must be tested first on the owner's build (buildids above).
- For later automated QA (rule 11 / playbook/automated-qa.md): Bot Warfare (GSC) on the
  AlterWare/Plutonium MP side is the realistic bot vehicle; Spec Ops co-op test would need
  two headless-ish clients — open question.

## Guesses (labelled, unverified)

- Guess: the Steam SP app (42680) existing as a near-empty second app means Steam attributes
  most data files to the MP app (42690, 16.6 GB) even though both apps share one installdir.
- Guess: "IW5 SP Warfare" (YouTube, AlterWare-related) is an SP-side warfare-style mod for
  the AlterWare client; description not fetchable, exact function unknown.

## Unknowns

- Whether IW5-Mod Spec Ops co-op works on buildid 24615914 / 24615796 (post-2026-10-03
  update) — untested.
- Whether vanilla (unmodded) Steam MW3 Spec Ops online still works in 2026 — untested.
- IW5-Mod license text — "none stated" per forum thread 463.
- Max players in IW5-Mod Spec Ops lobbies — vanilla is 2; unknown if raised.
- Exact contents/behavior of the "MW3 Downgrader" tool — not run, not fetched (would need
  owner approval per rule 1/8 before downloading anything from ModDB).

## Checks that actually ran (this session)

- Repo-wide case-insensitive grep for CoD terms → 0 matches.
- `games/` listing → 6 write-ups, none CoD.
- Steam manifests 42680/42690 read; exe listing, sizes, SHA-256 hashes computed (read-only).
- Web searches: "MW3 2011 co-op mod", "TeknoMW3", "IW5x client", Russian-language co-op
  search, "AlterWare IW5 status", "site:forum.alterware.dev iw5 sp warfare".
- Fetches: github.com/alterware, alterware.dev, forum threads (search page blocked),
  moddb.com/mods/mw3-survival-mode-revived (fetched OK), reddit thread (blocked), YouTube
  video page (description not returned).

## Session 2 (2026-10-18, continued) — owner decision + Phase 2/3 progress

Owner decision: **campaign co-op, build from zero (Path 2)**. Spec Ops route (AlterWare)
remains valuable prior art but is not the goal.

### Phase 2 (workspace) — done

- Approved and created isolated test copy: `<test copy>` via robocopy `/E /COPY:DAT`
  (296 files, 15.512 GB, exit 1 = success). Original install untouched.
- Tooling installed (owner-approved, winget): Git 2.55.0.5, GitHub CLI 2.102.0,
  Python 3.13.15.

### Phase 3 (understand) — new finds (verified by fetch 2026-10-18)

- **open-iw5** (momo5502, fork chain cydiakk → louvenarde; 590 commits; CI build badge):
  "Open source MW3 singleplayer and multiplayer client. Requires the MW3 dedicated server
  files. Very experimental and unfinished." **License: GPL-3.0** (fetched LICENSE).
  https://github.com/louvenarde/open-iw5 — ancestor of the closed AlterWare IW5-Mod
  (same author, per forum thread 463 naming momo5502). Implication: a viable open base
  for an SP client; anything built on it must be GPL-3.0 (rule: follow the license).
  → Contradiction to resolve later: forum says IW5-Mod is "based on MIT-licensed work"
  while open-iw5 is GPL-3.0; different ancestors possible. Record, don't guess.
- **xensik/gsc-tool**: compiles AND decompiles IW5 scripts (PC), incl. campaign.
  Extraction from fastfiles needs Zonetool (https://github.com/ZoneTool/zonetool) or
  Jekyll (https://github.com/EthanC/Jekyll). Usage: `gsc-tool -m decomp -g iw5 -s pc <file>`.
- **SyndiShanX/COD-GSC-Source**: dumped+formatted GSC/CSC for many CoD games incl. an
  `IW5-GSC/` folder; dumped with Jekyll/ACTS, decompiled with gsc-tool. Convenient
  reference; verification path = dump our own copy from `<test copy>` (rule 4).
- **ate47/atian-cod-tools (ACTS)**: CoD toolbox used by that community (fastfile I/O).

### Phase 4 (choose) — provisional direction (needs owner sign-off)

Host-authoritative state sync (playbook §1.6: retrofits onto closed SP engines are almost
always host-authoritative), with a **GPL-3.0 open-iw5 fork as the hook point** instead of
raw DLL injection into the closed `iw5sp.exe`:

- Stage 1 target (playbook build order): open-iw5 fork boots one campaign mission, logs
  player position — replaces weeks of blind RE, since open-iw5 already RE'd the SP engine.
- Script layer: campaign logic is GSC in fastfiles — dump + decompile with
  Zonetool/Jekyll + gsc-tool from the test copy; the story flag store (Stage 4 of build
  order) is likely GSC-accessible.
- Long-term risks already visible: campaign scripts assume exactly one player; multiplayer
  player entities in an SP binary; cutscene/trigger barriers (playbook §1.5).

## Next actions

1. Show owner the claim-issue draft; `gh auth login`; open claim issue.
2. Clone open-iw5 + COD-GSC-Source (git now available); audit open-iw5 `src/` for SP boot
   path and symbol maps.
3. Extract + decompile IW5 campaign GSC from the test copy (Zonetool/Jekyll — downloads
   need owner approval, pinned official sources).
4. Fork cranebeaker/coop-anything, branch `game/modern-warfare-3`, add
   `data/directory.yaml` entry + `games/modern-warfare-3/` write-up, draft PR.
5. Ghidra on `iw5sp.exe` only where open-iw5's maps are insufficient (large download —
   ask owner first).

---

## Session 2026-10-18 (cont. 2) — fastfile format and script-layer verification

All work below is on the test copy `<test copy>` (private path in `local/`). Nothing in
the owner's real install was touched.

### IW5 fastfile (.ff) format — derived from OpenAssetTools source (read, not guessed)

Read from `Laupetin/OpenAssetTools` (GPL-3.0), files
`src/ZoneCommon/Game/IW5/ZoneConstantsIW5.h`,
`src/ZoneLoading/Game/IW5/ZoneLoaderFactoryIW5.cpp`,
`src/ZoneLoading/Loading/Steps/StepLoadZoneSizes.cpp`, `src/Common/Game/IW5/IW5.h`:

```
offset 0..7   magic (8 bytes)
              "IWffu100" = unsigned zone          <- every SP campaign zone we checked
              "IWff0100" = signed / official IW   <- every MP zone we checked
              "IWffs100" = RSA auth header (only inside signed zones)
offset 8..11  version, u32 LE, expected value 1
offset 12     1 unknown byte (the game ignores it too)
offset 13..20 timestamp, u64 LE
offset 21..   zlib (deflate) stream of the XFile payload   [unsigned zones]
              for signed zones an RSA-signed auth header sits here instead
```

Decompressed payload begins with `u32 size`, `u32 externalSize`, then the XFile block
table (9 blocks: TEMP, PHYSICAL, RUNTIME, VIRTUAL, LARGE, CALLBACK, VERTEX, INDEX,
SCRIPT), then the asset list. In-file pointers are offsets with the block index in the
top 4 bits (`OFFSET_BLOCK_BIT_COUNT = 4`).

*Consequence:* an unsigned IW5 zone is a 21-byte header plus one zlib stream. Stock Python
`zlib` can inflate a whole campaign zone with no third-party tool.

### Self-built extraction tools (this session)

- `mw3-research/tools/ff_probe.py` — reads magic/version/timestamp, inflates the zlib
  stream, writes the raw XFile blob.
- `mw3-research/tools/ff_scan.py` — regex scan over an inflated blob.

Ran on the test copy (all eof=True, trailing=0, i.e. the stream is exactly one zlib
member):

| zone | on disk | inflated |
|---|---|---|
| `so_stealth_prague.ff` | 656,864 | 1,233,399 |
| `hijack.ff` | 198,766,636 | 300,445,645 |
| `payback.ff` | 173,775,764 | 290,792,230 |

### Verification of the third-party GSC dump (ground rule 4)

`SyndiShanX/COD-GSC-Source` is a decompiled dump, not our own extraction. Checked it
against our own copy by searching the inflated campaign zones for script identifiers that
appear in that dump's campaign sources:

- `hijack.bin` contains `hijack_airplane`, `tarmac_combat_wave`, `pre_zero_g`,
  `hijack_door_explosion` — all present in
  `COD-GSC-Source/IW5-GSC/scripts/maps/hijack.gsc`.
- `payback.bin` contains `payback_main`, `payback_util`, `sandstorm_code`,
  `payback_rescue` — all present in the `payback*.gsc` sources.

Both zones' script identifiers match, so the dump's IW5 campaign scripts are consistent
with our own retail (Russian) copy. Not every identifier is found verbatim — IW5 stores
the script string table with prefix/suffix sharing, so shared fragments appear once
(e.g. only `tarmac_combat_wave` is stored for `..._wave2/3/4`).

### OpenAssetTools Unlinker crashes on retail IW5 zones (unresolved)

A prebuilt Windows release exists, so building from source was not needed. Downloaded
`oat-windows.zip` v0.33.0 (3,636,251 bytes) and verified SHA-256
`4bf366cec03558a4b0f20bc8f197a51f4b9dad0aa3edc52b0e40fef2dd95a4f5` against the GitHub
release API digest. Also fetched v0.32.0 (`473b94f0…927a`) and v0.30.0 (`450d3b7a…ecb6`),
both hash-verified.

Behaviour on the test copy, `Unlinker.exe --list <zone>`:

| zone | magic | size | result |
|---|---|---|---|
| `sp_prague.ff` | IWffu100 | 207 | OK |
| `sp_intro.ff` | IWffu100 | 303 | OK |
| `so_trainer2_so_deltacamp.ff` | IWffu100 | 1,693 | OK |
| `code_pre_gfx.ff` | IWffu100 | 3,969 | OK |
| `code_pre_gfx_mp.ff` | IWff0100 | 24,597 | OK |
| `ui_mp.ff` | IWff0100 | 24,597 | OK |
| `patch.ff` | IWffu100 | 112,197 | OK |
| `ui.ff` | IWffu100 | 463,655 | OK |
| `code_post_gfx_mp.ff` | IWff0100 | 253,973 | **crash** |
| `so_stealth_prague.ff` | IWffu100 | 656,864 | crash |
| `hijack.ff` | IWffu100 | 198,766,636 | crash |
| `common.ff` | IWffu100 | 137,962,380 | crash |
| `common_mp.ff` | IWff0100 | 49,422,357 | crash |

- Exit code `-1073741819` = `0xC0000005` access violation, faulting offset `0x002f2452`
  inside `Unlinker.exe`, identical for every crashing zone (Windows Application event log
  + WER report, `ReportArchive\AppCrash_Unlinker.exe_…`).
- Deterministic (3/3 identical runs). Reproduced on OAT 0.30.0, 0.32.0 and 0.33.0, so it
  is not a recent regression.
- The crash happens *before* any output is flushed, i.e. during zone load, not during
  dumping. `--include-assets` does not gate loading (still crashes), so the crashing asset
  type cannot be isolated that way.
- Discriminator is content, not magic and not raw size: `ui.ff` (463 KB, unsigned) loads
  while `code_post_gfx_mp.ff` (253 KB, signed) crashes. Tiny/patch zones load but list as
  empty, so they only prove the step pipeline starts.
- Status: **not root-caused.** OAT is therefore not usable yet as the campaign fastfile
  dumper. Next step if we pursue it: build OAT with symbols (VS Build Tools 2022 + C++
  workload is installing) and get a stack trace, or run its unit tests. Upstream fix would
  be a real contribution.

### Other fastfile tools ruled out for IW5 (2011)

- **ACTS** (atian-cod-tools) 3.3.0 ships `data/games/mw3.json`, `mw3sp.json` and
  `acts-mw23.dll` — that "mw3" is the **2023** reboot (Modern Warfare III), not IW5 2011.
  No IW5 2011 plugin exists. Its plugin API (`include/acts_api/fastfile.h`, `gsc.h`,
  `examples/*.c`) is the only hook point and would need MSVC to compile a new plugin.
- **Greyhound** (fork of Wraith Archon) — asset extractor; see above, not an IW5 script dumper.
- **gsc-tool** — compiles/decompiles IW5 GSC but needs an already-extracted `.gsc`/`.gscbin`
  as input; it cannot open a fastfile.
- **Zonetool** — 2021, hooks the running game.
- **Jekyll** — no IW5 support found.

### Current blocker

`gh auth login` (device flow) has now expired a fourth time
(`failed to authenticate via web browser: context deadline exceeded`) — the owner must
open the device URL and approve while the command is waiting.

### Next actions (revised)

1. Owner completes `gh auth login`; then publish the claim issue draft (already approved
   verbatim) and continue with the fork/PR.
2. Decide on OAT: either build it with symbols and chase the `0x002f2452` crash, or extend
   `ff_probe.py` into a real XFile parser (asset table walk) which needs no third-party
   tool. The latter is more work but is fully under our control and matches the
   "build from zero" path.
3. Use `ff_scan.py` on every campaign zone to build an inventory of script identifiers
   (functions, `level.*` flags, dvars) — directly feeds the Stage-4 story-flag work.

---

## Session 2026-10-18 (cont. 3) — STOP: the owner's install is the 64-bit build

**This supersedes the "32-bit" assumption in every section above.** Ground rule 6 (stop at
the first contradiction): the game does not match the playbook/tooling assumption, so the
plan below is paused for an owner decision.

### Observation (verified, on the owner's real install, read-only)

- `iw5sp.exe` and `iw5mp.exe` are **amd64**: PE machine `0x8664` (read from the PE header).
  The install ships `steam_api64.dll` (119,720 B) and `bink2w64.dll` (431,616 B), no 32-bit
  `steam_api.dll` / `binkw32.dll`.
- The game's fastfiles are therefore **64-bit zones**: they carry IW5's container and XFile
  header (`IWffu100`, version 1, 44-byte header, 9 blocks) but store **8-byte pointers**.
  Parsed `so_stealth_prague.bin` (our own dump) directly:
  - `XAssetList` at offset 44 = `{ int stringCount = 187; <pad4>; char** strings = FOLLOWS;
    int assetCount = 331; <pad4>; XAsset* assets = FOLLOWS }` (32 bytes).
  - The script-string list then reads correctly with 8-byte pointers: 187 strings, the first
    ones `end`, `tag_origin`, `tag_eye`, `tag_weapon_right`, `footstep_right_large`, …
    (`FOLLOWS` = `0xFFFFFFFFFFFFFFFF`; `0` = null).
  - The asset array at `0x1052` is 331 × 16 bytes of `{ u32, u32 0, u64 FOLLOWS }`.
  - This is exactly why OAT's Unlinker segfaults (see above): OAT's IW5 loader is 32-bit and
    reads `assetCount` from the wrong word.
- The old 32-bit expectation (`assetCount` at word 2) is what produced the earlier "OAT
  mis-reads the XAssetList" finding. Root cause now known.

### Corroboration (independent sources)

- `eithan1231/open-fastfile`, `reverse/x86-zone/README.md` ("The 64-bit MW3 build cannot be
  walked, and says so"): *"The Steam install at `steamapps/common/Call of Duty Modern
  Warfare 3` is the 64-bit build (`steam_api64.dll`). … their zones store 8-byte pointers.
  `mp_dome`'s XAssetList reads `0x201, 0, -1, -1`: a count, then one 64-bit pointer. The
  walker refuses such a list at once. The pipeline reads the 32-bit depots."*
- A **2026 Activision update** changed MW3 (2011) from 32-bit to 64-bit, breaking Plutonium,
  AlterWare/IW5-Mod and FoV changers. Community guides exist to revert to the old 32-bit
  build via Steam's console (`download_depot`):
  - Steam guide "CoD MW3: How to Install the Old (Stable) 32-Bit Version (+DLC)"
    (`steamcommunity.com/sharedfiles/filedetails/?id=3796576439`).
  - Gist "Alterware/Plutonium-compatible Steam depots"
    (`gist.github.com/Josu-A/b3698ee46b66225401a5583044f5789a`).
  - ModDB "MW3 Downgrader [FIX ALTERWARE]", "AlterWare FIX After 64-Bit Update".

### Why this matters for our plan

- Every piece of prior art we chose to build on — open-iw5, OAT, AlterWare/IW5-Mod,
  Plutonium, the GSC dumps' verified toolchain, and our own `ff_probe.py` plan — targets the
  **32-bit** build. The owner currently has the 64-bit build, so open-iw5/our parser will not
  run against it as-is.
- Two ways forward:
  1. **(Recommended) Target the 32-bit build.** Add the 32-bit depots to a *separate* test
     copy (never the real install). This is what the whole modding ecosystem does, so our
     work is reusable and all existing tools apply. Reversible.
  2. Target the 64-bit build. No tooling exists; we would re-derive the whole 64-bit zone
     format and write a 64-bit client from scratch. Far larger, and nothing to build on.
- The owner's copy is the **Russian** localization (`UserConfig.language = russian`), so the
  32-bit depots to fetch are the Russian set, not English.

### 32-bit depots for the owner's (Russian) copy — from the gist above

- SP binaries: `download_depot 42680 42681 5651167211650965131`
- Main assets: `download_depot 42690 42682 2661317971072643596`
- Russian localization: `download_depot 42690 42688 5872940112953203418`
- (DLC, only if owned: `42695`/`42696`/`42697`/`42698` with the same manifests.)
- Manifests are not permanent (Activision changes them); re-check the guide/gist before use.
- Depots land in `steamapps/content/app_<appid>/depot_<id>/` and are copied into the game
  folder. We would copy them into a **separate test directory**, not the real install.

### Unknowns / to verify

- Whether the owner wants 32-bit (recommended) and will authorise the Steam-account depot
  download (~8–15 GB) and where the isolated 32-bit test copy should live.
- Whether open-iw5 actually targets the Steam `iw5sp.exe` or only the dedicated-server files
  (its README says "Requires the MW3 dedicated server files") — to check in its source.
- The exact 64-bit `XAssetType` enum/`XAsset` layout (the first asset-array word is `0x2d`,
  which is wider than IW5's 40 32-bit asset types) — parked unless we choose option 2.

---

## Session 2026-10-18 (cont. 4) — open-iw5's real base binary, and the dedicated server

### Correction to the plan: open-iw5 does not patch the retail exe

Read from our local copy, `mw3-research/open-iw5/src/loader/binary_loader.cpp` and
`loader.cpp`:

- `load_base()` reads a file **`iw5mp_server.exe`** from disk and, unless verification is
  off, requires its SHA-256 to equal
  `F271C305117B79242E254E9F64BD5AA2993CAC8E57975243EBD44CD576418D20`. Otherwise it throws
  *"Your iw5mp_server.exe is incompatible with this client."*
- `load(mode)` = that base **+ an HDiffPatch delta** (`src/resources/iw5sp.exe.diff` and
  `iw5mp.exe.diff`, each ~1.6 MB, zstd-compressed) -> the client binary, which the loader
  maps as a module inside its own launcher process.
- `README.md`: *"Open source MW3 singleplayer and multiplayer client. Requires the MW3
  dedicated server files."* It links steamdb app **42750**.

So open-iw5 is **not** a patch of the owner's `iw5sp.exe`; it is a *dedicated-server* base
plus a version-locked binary diff. Consequences:

1. The 32-bit **dedicated server** binary is a hard dependency, separate from the retail game.
2. The base is SHA-256-locked, so it must be the exact 32-bit server build the diffs were
   made against. A rebuilt server may not match.

### Dedicated server facts (public Steam metadata, `api.steamcmd.net/v1/info/42750`)

- App **42750** "Call of Duty: Modern Warfare 3 - Dedicated Server", type `Tool`, launch
  executable `iw5mp_server.exe`. **Not installed** on the owner's PC (no local copy).
- Public buildid **24939921**, rebuilt ~late Aug / early Sep 2026 (`timebuildupdated`
  1787706924, `timeupdated` 1788451381). That is **newer** than the game builds
  (`42680`=24615914, `42690`=24615796): the server was rebuilt in the same wave as the
  64-bit update. Whether the new `iw5mp_server.exe` is still 32-bit is **unknown** and is
  cheap to check (it is a small depot).
- Depots: the shared game depots (42682–42689, 42692) plus **42751** (the server binary
  depot; current public manifest gid `2427974803223634451`).

### Corrected 32-bit depot commands (current, from the Steam guide updated 2 Oct 2026)

Manifest ids changed since the gist we used before — re-check every run. Current values:

- Main assets: `download_depot 42690 42682 6067691797475126381` (~7 GB)
  *(cont. 3 had `...2661317971072643596` — now stale.)*
- SP binaries: `download_depot 42680 42681 5651167211650965131`
- EN localization: `download_depot 42690 42683 6381783964590594065`
- RU localization: `download_depot 42680 42688 5872940112953203418`
- DLC (only if owned): `42695 9005316271397236436`, `42696 2478272765185873756`,
  `42697 5810618727794750362`, `42698 5997898371746217629`
- Caveat (guide comment): a language depot needs the matching license ("missing license for
  depot (No subscription)"). The owner has the RU copy, so RU should be fine.
- Caveat: the guide does **not** cover MP binaries (depot 42691) or the dedicated server
  (42750/42751). For campaign co-op we need SP only, plus the server base for open-iw5.

### The decision matrix (for the owner)

Three real targets. "Prior art" = things that already exist and work on that target.

- **A. 32-bit retail game (ecosystem default).** Get the old depots above into a *separate*
  test copy. Prior art: OAT, Greyhound, GSC toolchain, our own parser plan, the whole
  Plutonium/AlterWare scene. Cost: ~7–8 GB download. Gives us the 32-bit zones and exes.
  Reversible. **Does not by itself give us open-iw5** (which needs the server base).
- **B. 32-bit + dedicated server (open-iw5 as designed).** A, *plus* the 32-bit
  `iw5mp_server.exe` (depot 42751 at an old manifest). This is the only way to actually
  build/run a fork of open-iw5. Extra unknowns: which old 42751 gid is the 32-bit, hash
  `F271C305…` build.
- **C. 64-bit retail (no download).** Target what the owner already has. Cost: no download,
  but zero prior art — re-derive the 64-bit zone format, write our own 64-bit hook/client,
  no reference parsers or injectors. Largest and riskiest; the 64-bit build also gets
  re-updated by Valve/Activision.

Recommendation: **B** (with A as its first step) — the only path where our work stands on
existing, testable tooling, and the only one that matches open-iw5's actual design.

### Verification attempt: SteamCMD (2026-10-18)

- SteamCMD (Valve official, `steamcdn-a.akamaihd.net/client/installer/steamcmd.zip`,
  774,825 B, SHA-256 `7669B170DEE42DB8EE2273775ED7DFB2D173BDBA1B849F70E…`) self-updated fine.
- **SteamCMD refuses paths with non-Latin characters** ("cannot run from a folder path that
  includes non-English characters"). Our workspace path is Cyrillic, so SteamCMD must run from
  an ASCII path; used `%LOCALAPPDATA%\Temp\opencode\steamcmd` (approved temp dir).
- **Anonymous login cannot fetch app 42750**: `download_depot 42750 42751 …` returned
  *"Depot download failed : missing license for depot (No subscription)"*. Consistent with
  `visibleonlywhensubscribed = 1` and `dependantonapp = 42680` in app 42750's metadata.
  → The owner's Steam license is required.
- App 42750 metadata (via `app_info_print`): type `Tool`, launch `iw5mp_server.exe`,
  public buildid `24939921`. Depots: shared `42682` (+ localizations 42683-42692) from 42690,
  `228989/228990` from 228980, and the only app-specific depot **42751**.
- **Depot sizes are literal content bytes** (calibrated): `42681` = 5,625,400 = exactly
  `iw5sp.exe`; `42691` = 6,167,104 = exactly `iw5mp.exe`; `42682` = 8,456,418,603 (~7.9 GiB).
  So depot `42751` = **117,744 B (~115 KB)** — far too small for a ~6 MB engine exe.
- SteamDB's patch page for build `24939921` (Sep 3 2026) lists only `admin/server.cfg`,
  `admin/default.dspl`, `admin/TJ_default.dsr` (~127 KiB) — i.e. depot 42751 looks like the
  `admin/` config folder, not the server exe. Where `iw5mp_server.exe` actually ships (app
  42750's own assigned `42682` manifest, which may differ from app 42690's) is **unresolved**
  and needs a licensed login to inspect.
- Unconfirmed from the metadata: whether the Sep 2026 rebuild changed the server **binary**
  (it looks like only the `admin/*` files changed, which would mean the server exe is still the
  pre-update — likely 32-bit — build).

Next action: owner runs one `download_depot 42750 42751 <gid>` from their Steam client console
(or a SteamCMD login) so we can list the files and, if the exe is elsewhere, locate it.

### RESULT — the current dedicated server is a dummy 64-bit stub (decisive)

Owner ran `download_depot 42750 42751 2427974803223634451` in the Steam console. Depot 42751
contains exactly one file: **`iw5mp_server.exe`, 117,744 B**. Probed with our own
`mw3-research/tools/pe_probe.py` and `embed_probe.py`:

- PE: **`machine = 0x8664` (amd64), PE32+, 6 sections**, timestamp `1787640969` (~2026-08-30).
- Code is tiny (`.text` vsize `0xd4c`, ~3.4 KB); `.rsrc` (95,744 B) is just the DigiCert
  code-signing chain — **no embedded engine**.
- Imports: `USER32.dll`, `KERNEL32.dll`, `VCRUNTIME140.dll`, `api-ms-win-crt-*`.
- Embedded PDB path: **`H:\IW\dev\p4\outrun\iw5\code_source\dummy_server\x64\Release\iw5mp_server.pdb`**
- SHA-256: `6349B95F3EBC49CB3092260FC3CCDE9D791A60BEB52830F53211A711CABCC2B5` — **not**
  open-iw5's expected `F271C305…`.

Conclusions:

1. The 2026 update replaced the real dedicated-server engine with a **dummy x64 stub**. The
   MW3 dedicated server shipping today does nothing (consistent with community "Auto-fix" /
   downgrade work).
2. **open-iw5 cannot use the current server files.** Path B now additionally requires an
   **older (pre-2026) manifest of depot 42751** — the real 32-bit `iw5mp_server.exe` that
   hashes `F271C305…`. Finding that manifest gid is now a tracked unknown.
3. Path A (32-bit retail game depots) is unaffected and still the prerequisite; the 32-bit
   revert guides are current and community-confirmed.

### The pre-2026 manifests (from the community gist + MW3Downgrader)

Source: `gist.github.com/Josu-A/b3698ee46b66225401a5583044f5789a` ("Alterware/Plutonium-compatible
Steam depots", the canonical community depot list) and `github.com/GalvarinoDev/MW3Downgrader`
(MIT, open source, bundles SteamRE DepotDownloader; supports base game, MP, Dedicated Server and
DLC, and writes to a separate `downgrade` folder so Steam can't undo it).

Key manifests (32-bit era):

| App | Depot | Manifest | Contents |
|---|---|---|---|
| 42680 | 42681 | `5651167211650965131` | SP binaries (`iw5sp.exe`) |
| 42690 | 42682 | `2661317971072643596` | main assets (~8.4 GB) |
| 42690 | 42691 | `4104640605720756125` | MP binaries (`iw5mp.exe`) |
| 42690 | 42683 | `1595601894688570808` | English localization |
| 42690 | 42688 | `5872940112953203418` | Russian localization |
| 42690 | 42695..42698 | `9005316271397236436`, `2478272765185873756`, `5810618727794750362`, `5997898371746217629` | DLC 1-4 |
| **42750** | **42751** | **`9089183337461621316`** | **real (pre-2026) dedicated server** |

Also confirmed by the Plutonium forum thread "iw5 Dedicated server Download not working Sept 2026"
(Oct 7 2026): *"Seems like steam has patched the dedicated server workaround. The only file I get
now is iw5mp_server instead of iw5mp.exe as well."*

Next: verify the pre-2026 `iw5mp_server.exe` hashes `F271C305…` (open-iw5's expected base).

### RESULT — pre-2026 server matches open-iw5 exactly (path B confirmed)

Owner ran `download_depot 42750 42751 9089183337461621316`. The downloaded
`iw5mp_server.exe` is:

- **4,077,480 B**, PE `machine = 0x014C` (**i386 / 32-bit**, PE32), 5 sections, timestamp
  `1525273957` (~2018-05-02). Imports incl. `steam_api.dll`, `d3d9.dll`, `WS2_32.dll`,
  `WINMM.dll`; markers include `iw5`, `demonware`, `zlib` — the real engine.
- SHA-256 = `F271C305117B79242E254E9F64BD5AA2993CAC8E57975243EBD44CD576418D20` — **exactly**
  open-iw5's `DEDI_HASH`.

Saved to `mw3-research/dumps/iw5mp_server_32bit_F271C305.exe` (verified copy) so a Steam update
cannot remove it.

**Conclusion:** the current 2026 build ships a dummy x64 stub for the dedicated server, but the
real 32-bit server (matching open-iw5 byte-for-byte) is recoverable via the pre-2026 manifest.
Path B is confirmed viable. Remaining work for path B: the 32-bit game depots (main assets
42682, SP binaries 42681, Russian localization 42688) plus the server base above.

### Chosen target and test location (owner decision, 2026-10-18)

- Owner approved: target the **32-bit build (path B)**; isolated test copy on **drive E:**.
- Test copy path: **`<test copy>\`** (outside the workspace).
- Depots to fetch, copy into the test folder, then verify:
  - `42690 42682 2661317971072643596` main assets (~8.4 GB)
  - `42680 42681 5651167211650965131` SP binaries (`iw5sp.exe`, 32-bit)
  - `42690 42688 5872940112953203418` Russian localization
  - `42750 42751 9089183337461621316` server base (already saved to `dumps/`)
- **Never** touch `<game dir>` (real 64-bit
  install). Take the depots from `<steam content>\` only.
- Owner runs the `download_depot` lines in the Steam console (SteamCMD needs an interactive
  login; the console route uses the already-signed-in client).

## Session 2026-10-19 — 32-bit test copy built; zone format + script inventory

### Depots downloaded and verified (all complete)

Steam was closed mid-session, but `<steam log>` proves all three finished:
"Depot download complete" for depot_42682 (23:30:38), depot_42681 (23:32:55), depot_42688
(00:58:54). Closing Steam did **not** truncate them.

| Depot | Manifest | Contents | Size |
|---|---|---|---|
| 42690/42682 | `2661317971072643596` | base assets (`iw_00`..`iw_24`, dlc zones) | 7,032 MiB |
| 42680/42681 | `5651167211650965131` | `iw5sp.exe` (32-bit) | 5,650,432 B |
| 42690/42688 | `5872940112953203418` | Russian zones + `localized_russian_iw0*.iwd` | 7,725 MiB |

32-bit markers confirmed: `main/iw_00.iwd` = 314,819,587 B (the 64-bit one is ~420 MB). A Russian
install legitimately has **no** loose base `zone/*.ff` — only `zone/russian` (152) + `zone/dlc` (7),
exactly matching the real install.

### Isolated 32-bit test copy

`<test copy>\` — **14.42 GB**. Built by robocopy-merging depot 42682 + 42688 + 42751,
then copying `iw5sp.exe`. Real install untouched (read-only).

Verified binaries:
- `iw5sp.exe` — 5,650,432 B, PE `0x014C` i386, sha256 `EA284F38…` (timestamp 2012-11-30)
- `iw5mp_server.exe` — 4,077,480 B, PE `0x014C` i386, sha256 `F271C305…` = open-iw5's base
- `admin/` — dedi configs (`server.cfg`, `*.dsr`/`*.dspl`)

### 32-bit zone format (confirmed on real campaign zones)

`zone\russian\so_stealth_prague.ff` inflates to 1,206,736 B (magic `IWffu100`, zlib at 21). The
XAssetList at offset 44 is **packed 16 bytes**: `{int stringCount; char** strings; int assetCount;
XAsset* assets}` with 4-byte pointers (vs the 32-byte/8-byte-pointer form in the 64-bit build).
Each XAsset is `{u32 type; u32 header}` = 8 bytes; header is a file offset or `FOLLOWS`
(`0xFFFFFFFF`) meaning the data follows inline.

New tools (all our own, `mw3-research\tools\`):
- `zone_probe.py` — auto-detects pointer size; lists script strings + asset-type histogram.
- `zlib_scan.py` — finds nested zlib streams (ScriptFile string pools, RawFile text).
- `zone_inventory.py` — per-zone identifier inventory; writes extracted strings.
- `hexdump.py`, `pe_probe.py`, `embed_probe.py`.

Known type: `0x27` (39) = `ASSET_TYPE_SCRIPTFILE` (open-iw5 `src/game/structs.hpp`).

### Script-identifier inventory (10 SP campaign zones)

`mw3-research\inventory\` — totals **23,842 distinct flags**, 2,132,955 strings.

| zone | inflated | assets | scriptfile | zlib streams | flags |
|---|---|---|---|---|---|
| sp_berlin | 129,840,207 | 3047 | 3 | 5 | 11905 |
| sp_dubai | 20,713,359 | 557 | 19 | 21 | 1413 |
| sp_intro | 558 | 4 | 2 | 0 | 0 |
| sp_ny_harbor | 16,461,491 | 459 | 16 | 18 | 988 |
| sp_ny_manhattan | 66,090,779 | 3352 | 9 | 10 | 5468 |
| sp_paris_a | 19,036,905 | 327 | 3 | 6 | 1808 |
| sp_paris_b | 13,444,185 | 2314 | 3 | 2 | 778 |
| sp_payback | 27,899,505 | 1671 | 15 | 22 | 1672 |
| sp_prague | 405 | 3 | 1 | 1 | 0 |
| sp_warlord | 72,289,579 | 1273 | 42 | 30 | 5219 |

Sample recovered flags — the story/state layer stage 4 must sync:
`NY_HARBOR_OBJ_FOLLOW_SANDMAN`, `NY_HARBOR_OBJ_CONTROL_ROOM`, `NY_HARBOR_OBJ_ESCAPE`,
`NY_HARBOR_OBJ_PLANT_MINE`, `AC130_OBJECTIVE_SUPPORT_FRIENDLIES`, `AC130_FRIENDLIES_DEAD`,
`SO_STEALTH_PRAGUE_OBJ_1`, `SO_STEALTH_PRAGUE_COLLECT_CIVILIAN`, `_stealth_enabled`,
`_stealth_spotted`. `sp_intro` (558 B) and `sp_prague` (405 B) are near-empty stubs — the real
content lives in the named mission zones.

### Next

1. Run `zone_inventory.py` over the `so_*` mission zones too; classify flags (objective / state /
   ui) and de-noise (many extracted "strings" are binary false positives).
2. Extract ScriptFile `.gscbin` per zone (name + 3 lengths + zlib pool + bytecode) as the
   compiled-script reference.
3. Continue Phase 3: map open-iw5's SP boot path (`iw5sp.exe.diff` vs base) and symbol bindings.
4. GitHub auth when the owner is ready (claim issue, fork, draft PR).

## Phase 3 milestone - open-iw5 SP build + boot (2026-10-19)

Result: **BUILD OK, SP CLIENT BOOTS.**

Upstream `momo5502/open-iw5` is deleted (GitHub 404). The local tree shipped with no `.git`, so
all 11 dependency submodules were **empty**. Each dep was recovered from its canonical repo. The
deleted `xlabs` gsc-tool branch (required API: `xsk/gsc/interfaces/*`, `xsk/resolver.hpp`,
`xsk/utils/compression.hpp`) survives at `Zelobby/gsc-tool@xlabs` (7d37402, 2022-10-15).

Build fixes applied to our fork tree (needed for MSVC 14.44 + current deps):
1. `src/std_include.hpp`: `#undef byte` before `<gsl/gsl>` - libtomcrypt
   `tomcrypt_macros.h:434` defines a function-like `byte(x,n)` macro that collides with `gsl::byte`.
2. `src/utils/cryptography.cpp`: 2x `const_cast<ecc_key*>` for `ecc_ansi_x963_export` /
   `ecc_export` (libtomcrypt 1.18.2 API is non-const).
3. `deps/premake/libParallel.lua`: add `libParallel/*.c` to the glob - the `c_locker_*` /
   `c_condvar_*` / `c_thread_parallel` symbols live in `parallel_import_c.c`, which the `*.cpp`
   glob missed.
4. The Cyrillic workspace path breaks `premake5.exe` (narrow-char file paths). Build through an
   ASCII junction `<build junction>` -> `<workspace>\mw3-research\open-iw5`.

Pinned dep versions (drift fixes): libtomcrypt v1.18.2, libtommath v1.2.1, GSL v4.0.0,
gsc-tool xlabs@7d37402. Others at branch tips (zlib develop, HDiffPatch master, rapidjson
master, udis86 master, zstd dev, minhook master, discord-rpc master).

Build: `tools\premake5.exe --file=premake5.lua vs2022` -> `build\open-iw5.sln`; MSBuild
Debug|Win32. Output `build\bin\Win32\Debug\open-iw5.exe` (9,225,728 bytes).

Boot (isolated test copy `<test copy>\`):
- staged `open-iw5.exe` beside `iw5mp_server.exe` (hash F271C305... == `DEDI_HASH`, exact match).
- ran `open-iw5.exe -singleplayer`; loader applied `iw5sp.exe.diff` to the dedi base in memory.
- OBSERVED: process alive + responding, window title `Call of Duty(R): Modern Warfare(R) 3`,
  ~1.6 GB RSS; wrote `userraw/console_sp.log`, `players2/config.cfg`,
  `players2/settings_c.zip.iw5`, `players2/settings_s.zip.iw5`.
- Evidence screenshot: `mw3-research\docs\evidence\sp_boot.png`.

`console_sp.log` (219 bytes) contents:
```
[     25693] logfile opened on Mon Oct 19 04:28:20 2026
[     25693] ******* script runtime error ********
[     25693] in call to builtin function "setsaveddvar"
```
`setsaveddvar` is raised by the reconstituted engine's own builtin table (SP table @ 0x186C68C),
not by open-iw5's GSC layer - a game-data/startup issue, not a loader failure.

Conclusion: toolchain validated end-to-end (build -> HDiffPatch -> in-memory PE load -> engine
entry -> window + FS init). The co-op replication layer remains Phase 4+.

## Session 2026-10-19 (cont.) — GitHub publish (claim issue + draft PR)

Owner said go. Steps and results:

- `gh auth login --web` done as **Polygon2-1** (device flow, scopes repo/read:org/gist).
- **Claim issue opened:** cranebeaker/coop-anything **#39** "Claim: Call of Duty: Modern Warfare 3
  (2011) - campaign co-op". No `in-progress` label exists on the repo (the issue template names one),
  so it was opened without a label (the MW2 claim issue #31 is also unlabelled).
- **Fork:** Polygon2-1/coop-anything -> `game/modern-warfare-3`. Base is upstream `main` bffd56f
  (0 behind). Note: the local zip snapshot we had was 13 entries behind main; always base on a fresh
  clone.
- **Installed PyYAML** (owner-approved) - `tools/build_directory.py` needs it.
- **PR:** cranebeaker/coop-anything **#40**, opened **draft**, "Modern Warfare 3 (2011): campaign
  co-op write-up + directory entries". CI `check` passes; mergeable, no conflicts.
  Contents:
  - `games/modern-warfare-3/README.md` (write-up from `templates/game.md`).
  - `games/modern-warfare-3/NOTES.md` (this file; local paths sanitized to `<game dir>` / `<test copy>`
    / `<steam content>` / `<build junction>` for the PR).
  - `data/directory.yaml`: 2 new entries - `AlterWare IW5-Mod` (kind mod, Spec Ops/Survival, closed
    source) and a `(none found)` **gap** for campaign co-op with the `searched:` list.
  - regenerated `DIRECTORY.md` (223/32 -> 224 projects/33 gaps).
- Owner review of the draft PR is the next human step; the "owner reviewed" box is left unticked.

Next actions:

1. Owner reviews PR #40; un-draft when happy.
2. Load one campaign mission on the open-iw5 client. **DONE** - see "Phase 3 milestone 2" below.
3. Phase 4: host-authoritative replication layer (players, entities, story flags).
4. Optional follow-up PR: engine-family row for the CoD (IW) engine in `playbook/engine-families.md`.

## Phase 3 milestone 2 - SP client runs a campaign mission (2026-10-19)

Correction to the earlier "boot stops" note above: the `setsaveddvar` script runtime error is
**non-fatal**. It is logged once during startup and the game carries on; the earlier run simply
never got into a level.

What was actually missing was the level argument. Observed on the isolated test copy:

- `open-iw5.exe -singleplayer` (no level): does not come up to a usable frontend. Window title
  becomes the game name, RSS grows to ~0.5 GB then ~0.9-1.1 GB, the window stops responding, and
  the 219-byte `userraw/console_sp.log` holding the `setsaveddvar` error is all that is written.
- `open-iw5.exe -singleplayer +map sp_intro`: process alive + responding, ~1.79 GB RSS, the same
  startup error is logged, and the mission loads and plays.

Proof it is real in-engine campaign gameplay and not a menu or a dialog: OCR of the captured frame
(`Windows.Media.Ocr`, ru-RU - the test copy is the Russian localization) reads live dialogue
subtitles:
- `Трак: Внимание! Вертолет на подходе!`
- `Гринч: Мне это нравится. Сэндман: Наверх по лестнице! За мной!`

Grinch and Sandman are Modern Warfare 3 campaign characters. Three frames sampled 12 s apart
differ, so the game is running, not frozen.

Evidence (local captures, not committed to this PR): `mw3-research/docs/evidence/`
`sp_intro_subtitle_1.png`, `sp_intro_subtitle_2.png`, `sp_boot_intro_hud.png`,
`sp_intro_gameplay.txt`.

Next: Phase 4 - host-authoritative replication layer (players, entities, story flags).
