# Spore: Galactic Adventures — co-op notes

Working notes for the Spore: Galactic Adventures co-op effort. Written as I go.
Sources are linked; observations, derivations, guesses and unknowns are kept apart.

Game identity: see `local/identity.md` (private, git-ignored).

Milestones: **M1** research and launch glue (phases 0-4, partial 6) and **M2**
the Creature stage (phases 5 Build and 6 QA). Phases finished: **0 Triage**,
**1 Research**, **2 Workspace**, **3 Understand**, **4 Choose**, **5 Build**,
**6 QA**. Not started: **7 Ship**, **8 Give back** (this file + `README.md` +
the directory entries are the give-back, plus the published fork).

M2 delivered a patch, published as a fork of the upstream mod:
<https://github.com/Co-Me/spore-multiplayer-mod> (commit `6b8f436`).

---

## Phase 0 — Triage

### 1. Which exact game?

Spore: Galactic Adventures (2009 expansion of Spore, 2008), Steam app **24720**,
Russian language, Windows. The Galactic Adventures executable is
`SporebinEP1\SporeApp.exe`; the base game executable is `SporeBin\SporeApp.exe`.
Both are present in the Steam depot and the GA exe matches one of the sizes the
existing mod explicitly supports (25 066 744 bytes).

> **Correction (2026-10-21, Phase 3).** The upstream script calls that size the
> "Steam March 2017" build. The ModAPI Launcher's own version table
> (`ModAPI.Common.GameVersion.ExecutableSizes`) maps 25 066 744 bytes to
> **`Steam_October2024`**. `Steam_March2017` is 24 885 248 bytes, a *different* exe.
> Both variants resolve to the same ModAPI core DLL (`SporeModAPI.march2017.dll`),
> so the mislabel is harmless for the kit — but the mod's own Detours hooks are
> pinned to hashed RVAs, and the october build is a different binary, so this
> matters for the native side.

The install was **not finished** on the owner's PC at triage time: `common\spore`
was empty and the payload was still in Steam's staging area.

### 2. Is it already done?

**Yes — a co-op mod exists**, but it stops at the Cell stage and was never
confirmed in a running game.

`emerald194520350-alt/spore-multiplayer-mod` — "Spore Multiplayer Mod", MIT,
29 stars, 21 commits between 2026-09-17 and 2026-09-27, latest release **v1.0.1**
(2026-09-27). Repository has **0 issues** and **0 open PRs**, so there is no public
bug list to mine — the author did the work in ten days and left no written
backlog. Every release note says the same thing in different words: *not verified
in a running game*.

There is **no entry for Spore** in this repo's `data/directory.yaml` and no
`games/spore/` folder, so nobody has covered this game here yet.

### 3. Which engine?

Proprietary EA/Maxis Spore engine with RenderWare rendering. No shipped
multiplayer, no netcode, no scripting layer. The community's answer to "closed
binary" is the **Spore ModAPI SDK** (`Spore-Community/Spore-ModAPI`, 86 stars),
a reverse-engineered C++ SDK that hooks the game with Microsoft Detours and ships
`SporeModAPI.march2017.dll` for exactly the March 2017 executable the owner has.
Mods are injected by the **SPORE ModAPI Launcher Kit**
(`Spore-Community/ModAPI-Launcher-Kit`, latest v1.5.13, 2026-10-16).

That puts Spore in the "plugin / script extender" tier of `playbook/triage.md`:
hook the engine from a DLL. No decompilation is needed for the mod itself, and
the ModAPI SDK ships Ghidra import files (`SporeGhidra_march2017.xml`) for anyone who
does need to go deeper.

### 4. How much source is there?

- The game itself: closed binary only.
- The mod: full C++ source (native client) + C# source (session server) + tests,
  MIT licensed, and the Spore ModAPI SDK vendored under `vendor/Spore-ModAPI`.
- The ModAPI SDK itself is public source (C++).

### 5. Does the engine have any multiplayer?

**No.** Spore has no multiplayer mode. The existing mod therefore had to build the
networking layer from scratch — and it did: a native client DLL talking to a
separate **C# TCP session server** (`SporeCoop.Server.exe`, `server/SessionServer.cs`).

### 6. Shipping check

Fine. The mod ships as a DLL + a launcher script; no game files, no assets, no
anti-cheat, no online service. Spore is an offline game. Nothing in the repo
touches EA or Valve services. The mod is MIT; the vendored ModAPI SDK and Detours
keep their own licences and the release archive ships the notices.

---

## Phase 1 — Research

### Searches run (2026-10-21)

| Where | Query | Result |
|---|---|---|
| GitHub repo search | `spore galactic adventures` | 32 repos; 1 co-op project (the mod below) |
| GitHub repo search | `spore modapi OR sporemods` | the ModAPI SDK and the community mod repos |
| GitHub repo search | `spore multiplayer` | see below |
| GitHub issues/PRs | `repo:cranebeaker/coop-anything spore` | **0 results** — nobody has touched Spore here |
| GitHub releases API | mod repo, 15 releases (beta-1 … v1.0.1) | read in full |
| GitHub issues API | mod repo | **0 issues, open or closed** |
| GitHub commits API | mod repo | 21 commits, all 2026-09-17 → 2026-09-27 |
| Nexus Mods | `spore` | **403 / Cloudflare challenge**, not readable from here |
| ModDB | `spore` multiplayer | nothing co-op for GA |
| Reddit | `spore multiplayer mod` | only the "Project Multispore" turn-based idea, no code |
| Steam forums (app 24720) | co-op | same "Project Multispore" thread, turn-based, no code |
| DavoOnline (via Wayback, 2024-06-24 snapshot) | Spore multiplayer | a 2017 thread with **concepts only**, no implementation |
| playground.ru, Discord | Spore co-op | no implementation found |
| coopmeld.com/spore | "Spore Coop" | **the most advanced project — see below** |

### The existing project

| Field | Value |
|---|---|
| Repo | https://github.com/emerald194520350-alt/spore-multiplayer-mod |
| Author handle | `emerald194520350-alt` (single author, no org) |
| Licence | MIT (`LICENSE` in repo) |
| Latest release | **v1.0.1**, 2026-09-27, protocol 10 |
| Release asset | `spore-multiplayer-mod-v1.0.1-windows.zip`, 357 314 bytes |
| Published SHA-256 | `787cac1f7b262dba76093923a75dcecc07916c4a5185182b94893b8c4d181e78` |
| **Verified SHA-256** | `787CAC1F7B262DBA76093923A75DCECC07916C4A5185182B94893B8C4D181E78` — **matches** |
| Downloads | 8 |
| Activity | 21 commits, all within 10 days, then silent |
| Issues / PRs | none |

The archive contains a prebuilt `bin\SporeCoop.Probe.dll` (857 600 bytes) and
`SporeCoop.Server.exe` (70 656 bytes), so the mod can be **used** without a C++
toolchain. Rebuilding from source needs Visual Studio 2022 (MSVC v143 + Windows
SDK); the C# server rebuilds with the .NET Framework 4 `csc.exe` that ships with
Windows; the `.mjs` test files need Node.js 22+.

### Architecture (read from source)

```
SporeApp.exe (window 1, host)  ─┐
    native/Probe.cpp  (C++, Spore ModAPI + Detours hooks)
    native/CoopNet.cpp (client protocol, JSON over length-prefixed TCP)
                                ├──► SporeCoop.Server.exe  (C#, TcpListener)
SporeApp.exe (window 2, guest) ─┘      server/SessionServer.cs
                                          • central broker + state authority
                                          • broadcasts position / appearance /
                                            npcSnapshot / worldAction
                                          • holds progress, unlocks, DNA budget,
                                            evolution state, species name
                                          • roles identified by host/guest token
```

This matters a lot: **the clients already talk to a server over TCP**, so
remote play is not a networking rewrite. The server is a plain process that can
listen on any address (`--listen`, `--port`) and the clients take their address
from environment variables (`SPORE_COOP_SERVER`, `SPORE_COOP_PORT`,
`SPORE_COOP_TOKEN`, `SPORE_COOP_ROLE`, `SPORE_COOP_PROFILE`).

### What the mod actually syncs (from the v1.0.1 README and release notes)

Shared: player movement and appearance, NPC replication, shared growth/DNA/parts,
diet history, species name, the creature editor (shared model + shared DNA budget,
with revision merging), shared progress and unlocks, History timeline, one-sided
campaign saves (owner writes, guest is protected), pause, editor entry/exit.

**Not** supported: Creature, Tribal, Civilization and Space stages. After the Cell
stage the mod shows an end screen instead of the landfall transition.

### Honest QA level of the existing mod

`qa: none` at triage time. The author's own words in the v1.0.1 release body:

> Fragment drops and collection in two windows still need gameplay verification.
> The ending screen, saving, return to the menu, and earlier guest fixes are also
> not confirmed by automated tests alone.

> These checks do not constitute a full two-player playthrough. (README)

There are 142 server checks, 115 client checks and 154 isolated engine checks,
which is a lot of automation — but the engine checks run against a **private
executable image** (the game binary mapped into memory without starting it), not
against a running game. **No two-window playthrough has ever been confirmed.**

### Known gaps, ranked by how much they block "play with friends"

1. **Remote play is unfinished.** README: *"Remote LAN/VPN play and complete world
   transfer between computers are still unfinished."* The launch scripts hardcode
   `127.0.0.1`. Beta 1 notes: *"Cross-PC Radmin VPN play and complete world
   transfer are not validated."*
2. **Only the Cell stage.** Four of the five stages have no co-op at all.
3. **Never verified end to end.** Every release stops at "needs two-window
   verification".
4. **Hardcoded developer paths** in the scripts: `C:\Games\SPORE Collection`,
   `C:\ProgramData\SPORE ModAPI Launcher Kit` and `...\ Launcher Kit 2`.
5. **Two Launcher Kit installs are required** (one per profile/window), and a
   second profile is created at `%APPDATA%\SporeCoop2` with the host's saved
   campaign and `Documents\My Spore Creations` copied into it.

### Other Spore projects seen (context, not co-op)

`bisfranz/SporeNEXT` (mod manager, v2.0.9), `Liskomato/Spore-*` (many small
Spore ModAPI mods), `MADPSI/*`, `0KepOnline/SPOREMod_*`, `Spore-Community/*`.
None of them are co-op. `SporeNEXT` may be useful for installing `.package` mods
but the co-op mod is a DLL and needs the Launcher Kit, not SporeNEXT.

### The other four Spore co-op projects (all dead ends for the Space stage)

| Project | What it is | Why it does not solve Space-stage co-op |
|---|---|---|
| **CoopMeld "Spore Coop"** — https://coopmeld.com/spore/ | GPL sources (installer + native mod + launcher), 2–4 players, online rooms, Cell+Creature+Tribe | The **live service is paused**: `/api/check?nonce=…` returns **HTTP 503 "Release unavailable."**, and `/lobby` returns **426 `{"error": "Update SporeCoop in the launcher before playing."}`**. The site itself lists Civilization and Space as *"Coming later"*. It is the only project that ever targeted more than the Cell stage. |
| **raph559/SporeMP** — https://github.com/raph559/SporeMP | GPL-3.0, roadmap for all stages, native dedicated server | Its byte-level guard pins the **GOG** build (24 895 536 B, SHA-256 `dc04aee5…`) and **rejects the Steam build** this owner has. Also a prototype. |
| **Spore-Multiplayer/Spore-Multiplayer** | empty Visual Studio skeleton, 18 280 B | `dllmain.cpp` is the ModAPI template; `.gitmodules` points at `emd4600/Spore-ModAPI`; `Client/README.md` says "Files for the Client. TBA". No code. |
| **sunglocto/SporeMultiplayer** | 545-byte README | README only, no code, no releases. |
| **NeDether/SporeMultiplayer** | — | **404 / gone.** |

**Conclusion of the research phase:** *no existing project makes Space-stage co-op
work.* The best available candidates are CoopMeld (paused service, never reached
Space) and the emerald mod (Cell only, works locally once launched correctly).

### Derived (my reasoning, not claims by the sources)

- **Remote play is mostly plumbing, not a rewrite.** The transport already exists
  and the server already speaks "one host authority + one guest" over plain TCP.
  Running `SporeCoop.Server.exe` on a reachable machine and pointing the guest at
  it is the natural design. This is a *design hypothesis*, not a tested fact.
- **The real remote-play blocker is world transfer.** The guest's profile must
  contain a copy of the host's saved campaign (`Games\` Game0 files) and the
  creature assets under `Documents\My Spore Creations`; `BorrowedCampaigns.h`
  hides those borrowed copies from the guest's galaxy menu and
  `CampaignSync.h` blocks the guest from writing them. The launcher does this
  copy locally. Across machines it has to be done by hand or implemented.
- **Steam relay is probably the wrong tool here.** The client DLL would have to
  talk to the Steam Spacewar relay, which is a lot of native work for no gain
  over "run the session server on a VPS/LAN box". Worth revisiting only if the
  owner wants zero-setup joins.
- The mod uses `DetourAttach` on hashed RVA addresses (`VerifiedMotionCode`), so
  it is pinned to the exact executable it was built against. A different GA build
  breaks it. *(The owner's build is the october one; the mod still initialised on
  it — see Phase 6 — but the RVA pinning is a real fragility.)*

### Unknowns

- Whether the mod works at all in two live windows. Nobody has confirmed.
- Whether the guest's local campaign must match the host's exactly, or only
  needs *a* valid Cell-stage campaign (the shared state sync would then decide
  the rest).
- Whether the "prepared copy" mechanism can survive a profile that is not a
  byte-for-byte copy of the host's.
- Whether Spore's cell stage can be entered at all on a fresh profile without
  going through the galaxy menu first.
- Issue tracker for bugs: the repo has none. The author's e-mail is not published.
  **There is nowhere to file a bug report against this project.**

---

## Phase 2 — Workspace

### Toolchain (owner approved installs)

| Tool | State |
|---|---|
| Python 3.12.10 + `pyyaml` 6.0.3 | installed (winget) |
| Node.js 24.20.0 | installed (winget) |
| Git 2.55.0.5, GitHub CLI 2.102.1 | installed (winget) |
| .NET Framework 4 `csc.exe` | present in Windows, used to build the C# session server |
| SPORE ModAPI Launcher Kit **v1.5.13** | installed at `C:\ProgramData\SPORE ModAPI Launcher Kit` (WiX bundle, `/quiet /norestart`, exit 0) |
| Visual Studio / MSVC v143 + Windows SDK | **declined by the owner** → native DLLs cannot be rebuilt here |

The kit install did **not** modify the game install: no `SporeApp_ModAPIFix.exe`
was created and `steam_api.dll` is byte-identical in both installs
(87 288 B, SHA-256 prefix `7AC1090D725DCBD3`). That compatibility executable is
only produced for the EA App build; this install is Steam.

A **second copy** of the kit was made by copying the folder to
`C:\ProgramData\SPORE ModAPI Launcher Kit 2` (not a second WiX install). Phase 3
explains why two copies are genuinely required.

### Test copy of the game

Per the repo ground rules the owner's install and saves are never touched. A
**robocopy** clone lives in a separate test folder (exit code 1 = files copied;
1458 files, 6 151 060 957 B). Every launch test in this file points there.

The only file ever written into any Spore folder is
`<test copy>\SporebinEP1\steam_appid.txt` — see Phase 3, it is required
for the game to start outside Steam, and it was written to the **test copy only**.

---

## Phase 3 — Understand

This phase produced the single most useful result of the whole effort: the mod
does run in this game, but the upstream launch script cannot start it.

### 3.1 The game would not start (exit code 53)

**Observation.** Launching `<game>\SporebinEP1\SporeApp.exe` directly (not through
Steam) made the process exit immediately:

| `steam_appid.txt` in `SporebinEP1` | Result |
|---|---|
| absent | process exits, **code 53** |
| contains `17390` | process exits, **code 86** |
| contains `24720` | **runs normally** (≈ 408 MB working set, window present) |

**Root cause (derived, then confirmed).** The Steam build calls `SteamAPI_Init`,
which fails when the process was not started by the Steam client. `steam_api.dll`
reads the app id from `steam_appid.txt` in the game folder when the process is
not Steam-launched. The file has to contain the **app id of the executable that
is running**: the GA executable is app **24720** (the base `SporeBin` game is
app 17390). The value was confirmed independently from the ModAPI Launcher's own
constant `ModAPI.Common.SporePath.GalacticAdventuresSteamID = "24720"`.

The ModAPI Launcher **does this automatically** in its GUI flow, and the upstream
`Start-SporeCoopInstance.ps1` **bypasses that flow** (it calls
`InjectSporeProcess` directly instead of `Execute()`), so the file is never
written. That is the whole reason the mod "does not start" for people who follow
the README.

### 3.2 The ModAPI Launcher, reverse-engineered

`Spore ModAPI Launcher.exe` is a 32-bit .NET assembly. It was read with
reflection and a hand-written IL disassembler (both under
`%WINDIR%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe` — a 64-bit host
cannot load it: *"The given program is not a valid Win32 application"*).

Facts extracted from `ModAPI.Common` and from the launcher's IL:

| Item | Value |
|---|---|
| `GameVersion.ExecutableSizes` | 24 909 584→Disc, 24 898 224→Origin_March2017, 24 906 040→EA_October2024, 24 885 248→Steam_March2017, 24 895 536→GOG_October2024, **25 066 744→Steam_October2024** |
| `GetNewDLLName(Steam_October2024)` | `SporeModAPI.march2017.dll` (from `coreLibs\`) |
| `GetVersionName(Steam_October2024)` | `steam_patched` (→ `SporeModAPI-steam_patched.dll`) |
| `GetExecutableFileName(Steam_October2024)` | `SporeApp.exe` (no `SporeApp_ModAPIFix.exe` needed) |
| `RequiresModAPIFix(Steam_October2024)` | **False** — no admin fix required for this build |
| `SporePath.GalacticAdventuresSteamID` | `24720` |
| `SporePath.GetSporebinEP1Path()` | the Steam install (`<Steam root>\steamapps\common\spore\SporebinEP1`) |

The launch flow (`Program.Execute` → `Program.InjectSporeProcess` → `Injector`):

1. `CreateSporeProcess()` — `CreateProcessW` with `CREATE_SUSPENDED`,
   `lpApplicationName = NULL`, `lpCommandLine = "<exe>" <own command line>`,
   `lpCurrentDirectory = SporebinPath`.
2. `Injector.InjectDLL(pi, <kit>\ModAPI.DLLInjector.dll)` — `OpenProcess` →
   `VirtualAllocEx` → `WriteProcessMemory` → `CreateRemoteThread` running
   `LoadLibraryA`.
3. `GetDLLsToInject("steam_patched")` — copies
   `coreLibs\SporeModAPI.march2017.dll` → `mLibs\SporeModAPI.dll` and
   `coreLibs\SporeModAPI.lib` → `mLibs\SporeModAPI.lib`, and returns
   `mLibs\SporeModAPI.dll`, `<kit>\SporeModAPI-steam_patched.dll` and every other
   file in `mLibs` (that is where the mod DLL goes).
4. `Injector.SetInjectionData(pi, hInjector, isDisc=false, dlls)` — the injected
   injector loads the listed DLLs into the suspended game and lets it run.

**Consequence, and the reason for the second kit copy:** step 3 **overwrites**
`mLibs\SporeModAPI.dll` on every launch. A second launch from the same kit copy
fails with *"не удалось скопировать файл … SporeModAPI.dll, так как он
используется другим процессом"* (the first game process holds the file open).
Two concurrent windows therefore need **two separate kit folders** — one per
window. This confirms the upstream scripts' "two Launcher Kit installs" rule, and
explains it.

### 3.3 The Steam "Select Launch Option" dialog

Steam shows a CEF dialog for app 24720 ("Select Launch Option (%1s)",
`LaunchOptionsDialog_SelectHeader` in SteamUI's localization). It is a leftover
from the app's launch options and it clears itself; it is not a blocker and it
was not part of the injection problem.

---

## Phase 4 — Choose

**Path 1: revive/finish `spore-multiplayer-mod`.** Not Path 2 — there is a real,
substantial, MIT-licensed implementation to build on, and rebuilding co-op for
Spore from zero would be a far bigger job.

**Second, smaller decision: what is actually deliverable.**

| Goal | Verdict |
|---|---|
| Two players, one PC, Cell stage | **Achievable and achieved** (Phase 6) |
| Two players, different PCs, Cell stage | Plausible: the transport is already plain TCP and the server is a standalone process. **Not tested** — world transfer between machines is still missing. |
| **Space stage co-op** | **Not achievable with any existing project.** No project implements it, CoopMeld's service is paused and it never got past "Coming later" for Space. This would be new development on top of the Cell-stage base. |
| **Creature stage** | **Achievable as a transition** (M2). The upstream mod stops the session at the end of Cell; the patch makes the session continue and keeps the shared stage. **Creature-stage gameplay sync is not achievable as a small patch** — it is a second milestone of its own (see Phase 5). |

**Third decision (M2): which route to the Creature stage.** Three routes were
weighed:

| Route | Verdict |
|---|---|
| **C. Patch the emerald mod** | **Chosen.** The source is public, the protocol is documented in the code, and the change is small: one new message plus a persisted stage. The upstream history stays intact in a fork, so the patch is reviewable as one commit and can be sent upstream later. |
| A. Write a new mod from zero | Rejected: the Cell-stage protocol, the shared world, the world-owner rules and the editor sync would all have to be rebuilt. Weeks of work for the same result. |
| B. Continue with CoopMeld | Rejected: its live service is paused (HTTP 503 on `/api/check`, HTTP 426 on `/lobby`), and its `protection/` layer is obfuscation, not a licensing wall. Its GPL sources are still the best *reference* for Creature-stage gameplay (see below). |

**Fourth decision (M2): how much of the Creature stage?** The honest split is
"the session reaches the Creature stage and both windows agree on it" versus
"the creature is shared". Only the first is a patch-sized change; the second
needs the creature simulator itself, which no Spore project has ever
synchronised except CoopMeld. M2 does the first and states the second as open.

---

## Phase 5 — Build

Two rounds, in this order.

### 5.1 M1 — no native code was changed

The native DLL (`SporeCoop.Probe.dll`) is pinned to hashed RVAs and must be
recompiled with MSVC v143 to be modified, and the owner declined the Visual
Studio Build Tools at that point. The prebuilt release DLL was used as-is.

What *was* built here is the missing launcher glue that the upstream scripts do
not contain, plus the knowledge to write it:

- A working launch recipe (see "How to play" in `README.md`) that
  (a) writes `steam_appid.txt` = `24720` into the game's `SporebinEP1`,
  (b) starts the session server with `--listen/--port/--host-token/--guest-token/--save`,
  (c) starts the host window from kit copy 1 and the guest window from kit copy 2,
  (d) points both at the same server and gives them distinct roles/tokens.
- The mod DLL (`bin\SporeCoop.Probe.dll`, SHA-256
  `859FCC53796F8AFCCB2343AC7064144AC23229EF6647D6753A55053C56313C2E`) copied into
  `mLibs\` of **both** kit copies.

The upstream `Start-SporeCoopInstance.ps1` can be reused as-is once
`steam_appid.txt` exists and a second kit copy is present; it was not modified
(it lives in the upstream release, not in this repo).

### 5.2 M2 — the native client and the server were changed

**Toolchain (owner approved the install):** Visual Studio Build Tools 2022
17.14.42, installed to `D:\VSBuildTools` (MSVC v143 14.44.35207, Windows SDK
10.0.22621.0, MSBuild, cmake). With it the pinned DLL could be rebuilt from
source, which is the precondition for everything in this section.

**What the upstream does at the end of the Cell stage** (read from the v1.0.1
source):

- `native/CellStageEnd.h` intercepts the landfall transition and shows the
  version 1 ending screen instead of the native landfall and the Creature
  editor;
- the server keeps the stage in the session as `cell` and refuses editor work
  once the Cell stage is complete (`editorOpen` requires it);
- `CellStageComplete` is transient session state, so a server restart loses it.

**The patch** (one commit, `6b8f436`, 11 files, +343 / −12):

| File | Change |
|---|---|
| `native/CoopNet.h`, `native/CoopNet.cpp` | `Snapshot.stage` field, `SubmitStageTransition` message |
| `native/Probe.cpp` | report the transition when `Simulator::IsCreatureGame()` is true; retry every 2 s while the server still reports `cell` |
| `native/CellStageEnd.h` | in a live cooperative session the ending is not intercepted and the version 1 screen is dismissed |
| `server/SessionServer.cs` | `stageTransition` handler, persisted `Stage` and `CellStageComplete`, editor no longer blocked by the Cell stage completion, banner `Supported stages: Cell, Creature.` |
| `tests/native-stage.test.mjs` | new end-to-end test (two real native clients + the real server) |
| `tests/server.test.mjs`, `tests/native-visual.test.cpp`, `tests/native-source.test.mjs` | new protocol, native and source assertions |
| `README.md`, `RELEASE_NOTES.md` | the patch, its limits and the upstream licence inconsistency |

Rebuilt client: `bin\SporeCoop.Probe.dll`, 857 088 bytes, SHA-256
`07178AE31CD1ADB7718EA0D2E91323F2872311F8F893C2FBEABD94B895ED2405`.

**Design decisions and their reasons**

- **New message, protocol stays at 10.** The patch is additive, so an
  unmodified 1.0.1 client still connects to the patched server, and the patched
  client still connects to an unpatched server. **Verified in both directions
  with a raw protocol probe** (6.8): the unpatched server answers the new
  message with `{"type":"error","message":"Unknown message type"}`, rolls the
  state back and keeps the session alive; the native client stores that error
  in `lastError` and stays connected.
- **Either peer may send it; it is idempotent.** The transition is a fact about
  the shared world, so no election and no ordering rule is needed.
- **`CellStageComplete` became a shared-world fact** and is persisted, like the
  stage itself, so a server restart no longer loses the progress.
- **The editor stays open.** Upstream blocked the editor after the Cell stage;
  the land editor is the Creature-stage design step, so blocking it would
  defeat the purpose of the patch.
- **The stage is detected from the game's own state:**
  `Simulator::IsCreatureGame()` (vendored Spore ModAPI SDK,
  `Spore\Simulator\SubSystem\GameModeManager.h:91`), not from a timer that
  assumes the stage was reached.
- **The report retries only while the server still says `cell`.** The other
  player may still be finishing the stage, so the report is repeated every 2 s
  until the server confirms `creature`. A server without the `stage` field is
  reported once, not in a loop.

**Published as a fork:** <https://github.com/Co-Me/spore-multiplayer-mod>,
one commit on top of upstream `d837f83`. The owner chose the fork route over a
standalone repository, because it keeps the upstream history and makes the
patch visible as a single diff.

**Licence finding (upstream, unchanged):** the upstream `LICENSE` file is MIT,
but 20 source files — including `native/Probe.cpp` and `server/SessionServer.cs`
— carry `SPDX-License-Identifier: GPL-3.0-or-later` headers and 38 carry none.
The inconsistency is upstream's and was left untouched; the patch and the new
test file are released under GPL-3.0-or-later, and the fork README documents the
conflict.

**Not built (deliberately):** Creature-stage gameplay sync. That needs the
creature simulator, herds and interactions. The only existing reference is
CoopMeld's GPL source (`creature_hooks.inc`, 76 KB, plus `herd_sync.inc` and
`interaction_sync.inc`), which is the natural starting point for milestone 3.

---

## Phase 6 — QA

### 6.1 The mod's own test suites — actually run

Baseline first, measured on an **untouched copy of the v1.0.1 release tree**
(`local/downloads/src/spore-multiplayer-mod-1.0.1` copied to a scratch folder so
the original stayed pristine), then the same suites on the patched fork.

| Suite | Command | Upstream v1.0.1 | Patched fork |
|---|---|---|---|
| Server protocol | `Test-Server.ps1` (rebuilds `SporeCoop.Server.exe` with the .NET Framework `csc.exe`, then runs it) | **PASS: 142 protocol assertions** | **PASS: 148 protocol assertions** (+6) |
| Native engine checks | `Test-Native.ps1` | **PASS: 115 native visual/network assertions** | **PASS: 119** (+4) |
| Native source invariants | `node tests/native-source.test.mjs` | **PASS** | **PASS**, +8 new invariants |
| Native idle check | `node tests/native-idle.test.mjs` | **could not run** — needs `bin\tests\NativeVisualTests.exe`, not in the release zip, and MSVC was not installed at the time | **PASS** — the binary can be built now |
| Stage end-to-end | `node tests/native-stage.test.mjs` | does not exist | **PASS: native clients and the real server moved the shared world into the Creature stage** |
| Build from a clean checkout | `Build-Probe.ps1 -LauncherRoot "<kit>" -SdkRoot "vendor\Spore-ModAPI"` | n/a (no patched source) | **0 errors**, MSVC v143, 93 pre-existing SDK warnings |

The upstream "154 native engine checks" figure, repeated in the first milestone,
was the author's own claim about the release; the suite that actually ships
builds **115** assertions on the untouched tree and **119** with the patch.

### 6.1a This repo's own tooling — also run

| Check | Result |
|---|---|
| `python tools/build_directory.py --check` (what CI runs) | **OK: 303 entries** (302 before the fork entry) |
| `python -m unittest tools/mcp/test_coop_mcp.py` (what CI runs) | **Ran 13 tests … OK** |
| `python -I tools/test_all.py` | **13 of 15 suites pass**, 2 fail for reasons unrelated to this game (see below) |

With cmake and MSBuild on `PATH` the full suite runs further than in milestone 1
(where several suites could not even build). Per suite:

```
PASS  chaos  (90s)          PASS  animcheck  (16s)      PASS  hotreload  (28s)
PASS  ownership  (3s)        PASS  bridge  (115s)        PASS  buildlock  (28s)
PASS  fakeworld  (3s)        PASS  inspect  (24s)
PASS  fuzz  (4s)             FAIL  flight  (9s)
PASS  gameslot  (3s)         FAIL  mutants  (61s)
PASS  gamepool  (120s)       PASS  scenarios  (30s)
PASS  bots  (47s)
```

**The two failures were checked against a pristine `upstream/main` checkout**
(`b877c9e`, detached worktree in a scratch folder) and are **not** caused by
this work:

- `mutants` fails there too, with the same assertion
  (`check_cpp_shipped_build_ignores_twins` — "the test build contains the twin
  name"), i.e. a pre-existing issue of the repo's C++ tooling under MSVC v143.
- `flight` **passes** in that checkout and fails in this one. The difference is
  the path: this workspace is `C:\Users\<user>\Documents\<Cyrillic>\coop-anything-main`,
  and the flight tool builds the replay command as `"C:\...\toy_replay.py"`
  inside an already-quoted string, so Python gets
  `can't open file '...\"C:\...\toy_replay.py\"'`. A path with non-ASCII
  characters breaks it; an ASCII path does not.

Both failures are in this repo's own tooling, not in anything Spore-related,
and the two commands CI actually runs are green, so the PR is not blocked.

### 6.2 End-to-end injection test (this session) — the real result

Run on the **Steam October 2024** build, from the **test copy**, with the mod's
**prebuilt** release DLL and no source changes.

**Setup**

```
<game>\SporebinEP1\steam_appid.txt          = 24720          (test copy only)
SporeCoop.Server.exe --listen 127.0.0.1 --port 5523 \
        --host-token <host> --guest-token <guest> --save <path>\session.json
Start-SporeCoopInstance.ps1 -Profile 1 -Role host  -LauncherRoot "<kit 1>" -GameRoot "<test copy>" ...
Start-SporeCoopInstance.ps1 -Profile 2 -Role guest -LauncherRoot "<kit 2>" -GameRoot "<test copy>" ...
```

**Observed results**

1. Both game processes stayed alive, launched from the **test copy**
   (`<test copy>\SporebinEP1\SporeApp.exe`), not from the Steam install.
2. The mod's probe log (`%TEMP%\SporeCoop.Probe.log`) shows, for **both** windows:

   ```
   [pid=8252  role=host ] Native build: Sep 27 2026 17:43:00; registered peer renderer.
   [pid=8252  role=host ] Initialize completed; commands registered and cooperative client started.
   [pid=8252  role=host ] Cooperative connection state reset after handshake.
   [pid=20468 role=guest] Profile 2 paths selected: SporeCoop2 and My Spore Creations Coop 2.
   [pid=20468 role=guest] Initialize completed; commands registered and cooperative client started.
   [pid=20468 role=guest] Cooperative connection state reset after handshake.
   ```

3. The session server's own stdout (`local/testrig/server.log`):

   ```
   SporeCoop Version 1 server (protocol 10). Supported stage: Cell.
   HOST TOKEN (keep local): …
   GUEST TOKEN (share with friend): …
   LISTENING 127.0.0.1:5523
   Connected: host
   Connected: guest
   ```

4. Two **Established** TCP sessions on port 5523, both owned by the server
   process, plus the listening socket.
5. The server wrote `session.json` (`"Stage":"cell"`, `"Format":1`,
   `"Revision":0`, `"Applied":[]`).

   Captured in `local/testrig/` (`probe-2026-10-21.log`, `server.log`,
   `session.json`).

**Conclusion of 6.2:** the mod's native client *does* load into this game build
and *does* complete a protocol handshake with the session server from two
separate game processes. That had never been demonstrated before.

### 6.3 What is still **not** verified

- **The Cell stage was never played to its end in a running game.** So the
  landfall, the land editor opening, the stage report and the server's stage
  advance were **not** observed in the real game. The transition is proven by
  the end-to-end native test (6.6) and by the source invariants, not by a
  gameplay session. **This is the biggest gap of milestone 2.**
- **Two players in the same world.** The invite flow is in-game UI: the host
  clicks "Invite", picks a player in a picker window, the guest clicks "Accept"
  in a prompt (`native/Probe.cpp`, `kInviteButtonID` / `kInvitePlayerButtonID` /
  `kAcceptInviteButtonID`). Driving that needs either a human or a UI bot; the
  diagnostic commands (`coopStatus`, `coopSpawn`, `coopJoin`) are registered
  through `CheatManager.AddCheat`, i.e. they are only reachable from the game's
  cheat UI, and there is no scriptable trigger.
- **Creature-stage gameplay.** Nothing about the creature is synchronised: no
  shared creature, no shared planet, no shared herds, no creature interactions.
  The Creature simulator runs locally in each window.
- The end screen, saving, return to the menu, and fragment drops — the author's
  own list of unconfirmed behaviour.
- Remote play between two machines (world transfer is not implemented).
- The upstream "154 native engine checks" claim (the shipped suite builds 115).

### 6.4 The Space-stage gap, in the mod's own words

The probe log of the running game says, for both windows:

```
Version 1: verified Cell-stage ending after History; further stages disabled in co-op.
```

and the session server prints `Supported stage: Cell.`

That is in-game, server-side proof that **co-op ends after the Cell stage** —
the four later stages (Creature, Tribal, Civilization, Space) have no co-op at
all in this project. The owner's stated priority (Space stage) is therefore
**not covered by any project that exists**.

### 6.5 A note on module enumeration

`Get-Process … .Modules` from a 64-bit PowerShell into the 32-bit game process
lists only **7** modules — the WoW64 host layer (`wow64.dll`, `wow64base.dll`,
…) — and never shows the ModAPI DLLs. The module list is **not** usable as
injection evidence here; the probe log is, because it is written by the mod's own
code inside the game process and carries the game PID.

### 6.6 The stage transition, end to end over real TCP (M2)

`tests/native-stage.test.mjs` (new in the patch) is the test that carries the
milestone. It is not a mock:

1. it starts the **real** `SporeCoop.Server.exe` on a free loopback port with
   two tokens and a save file;
2. it starts **two real native clients** (`bin\tests\NativeVisualTests.exe
   --stage-transition`), which speak the same protocol as the game DLL;
3. it drives the **real invitation handshake** (host connects, guest connects,
   host invites, guest accepts);
4. it makes the clients report the stage transition;
5. it asserts that the server advanced the shared stage and that the saved
   session file records it.

Result: **PASS — native clients and the real server moved the shared world
into the Creature stage**, and `session.json` ends with `"Stage":"creature"`
and `"CellStageComplete":true`.

The test was also run from a **clean checkout of the published fork** (not from
the scratch working copy), so the artifact that other people clone is the one
that passes.

### 6.7 The patched DLL in the real game (M2)

The rebuilt DLL was installed into `mLibs\` of **both** launcher kits (the
installed copies were verified by SHA-256 against the build output), and two
real game windows were launched from the test copy of the game against the real
server on `127.0.0.1:5523`.

**Observed**

- server stdout: `Connected: host`, `Connected: guest`;
- probe log, both windows: `Initialize completed; commands registered and
  cooperative client started.` and `Cooperative connection state reset after
  handshake.`;
- `session.json` after the run: `"Stage":"cell"`, `"CellStageComplete":false`,
  `"InviteAccepted":false`;
- all game and server processes stopped afterwards (0 left running).

**Interpretation (derived, not observed):** the patched DLL loads in the real
game and both windows talk to the patched server. `InviteAccepted` stayed false
because the invitation is an in-game UI click, so no window ever entered the
Creature stage and **the transition itself was not observed live**. The
transition is proven by 6.6 and by the source invariants.

Evidence kept in `local/` (not in the PR): the probe log, the server log and
the session file of that run.

### 6.8 The patch boundary, checked in both directions (M2)

The claim "the protocol stays at 10, so old and new clients interoperate" was
checked with a raw protocol probe (a small Node script that speaks the framing
the server uses: 4-byte little-endian length + JSON) against **both** servers:

| Direction | Probe | Observed |
|---|---|---|
| upstream-shaped client → **patched** server | `hello`, `snapshot`, `ping` | `welcome`, then `state` with `stage = cell` and `cellStageComplete = false`, then `pong` — **PASS** |
| patched client → **unpatched** upstream server | `hello`, `stageTransition`, `ping` | `{"type":"error","message":"Unknown message type"}`, then the session **still answers** `ping` with `pong` — **PASS** |

Why the second case is not fatal: the upstream server catches
`ArgumentException` per message, rolls the protocol state back and replies with
an `error` message instead of dropping the peer
(`server/SessionServer.cs`, the receive loop). The native client handles an
`error` message by storing the text in `lastError` and returning — it does not
disconnect (`native/CoopNet.cpp`).

**Derived, not observed:** in the real game the patched client would report the
transition once per session against an unpatched server, the guest would see
the error text in the status line, and the co-op session would carry on in the
Cell stage — i.e. the old behaviour. That path was not exercised in the game.

---

## What to tell the owner

1. **Good news:** the emerald mod *does* work on this exact Steam build once it
   is launched correctly. Two windows, one PC, both connected to the session
   server — verified. The missing piece was never a broken mod; it was a
   missing `steam_appid.txt` and two kit copies, neither of which the upstream
   README explains.
2. **Good news (M2):** the session now **continues past the Cell stage into the
   Creature stage**. The patch is published as a fork
   (<https://github.com/Co-Me/spore-multiplayer-mod>, one commit on top of
   upstream), it builds from a clean checkout, and it passes 148 protocol
   assertions, 119 native assertions, the source invariants and a new
   end-to-end test that moves the shared world from `cell` to `creature` over
   real TCP with two real native clients and the real server.
3. **Honest limits:** the **Creature itself is not shared** — no shared
   creature, planet, herds or interactions; that is the next milestone. And the
   Cell stage was **never played to its end in a running game**, so the
   landfall and the stage report still need one interactive confirmation.
4. **Bad news:** beyond the Creature stage there is still nothing. The Space
   stage has no co-op implementation anywhere, and the one project that aimed
   higher (CoopMeld) has a paused service (HTTP 503).
5. **Realistic options:**
   a. Ship the Cell-stage mod as a working local/remote co-op (needs: a world
      transfer step, a reachable server, and one interactive in-game session to
      confirm the invite flow).
   b. Port/implement the later stages on top of it — real engineering, and the
      Space stage is a different game (a 3D galaxy flight with planets and
      cities) from the 2D cell stage the mod handles. The Creature stage is now
      the cheapest of them, because the session already reaches it; the missing
      part is the creature simulator itself, and CoopMeld's GPL sources
      (`creature_hooks.inc`, `herd_sync.inc`, `interaction_sync.inc`) are the
      only existing reference.
   c. Wait for CoopMeld to come back — but it had not reached Space either.
6. **Upstream:** the upstream project has no issue tracker, so the patch goes
   there as a pull request to `emerald194520350-alt/spore-multiplayer-mod`. The
   fork keeps the history, so the diff is one commit. The two launch fixes
   (the `steam_appid.txt` step and the two-kit-folders note) belong in the same
   pull request.
