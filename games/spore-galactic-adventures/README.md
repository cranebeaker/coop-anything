# Spore: Galactic Adventures (2009)

| | |
|---|---|
| Engine | Spore (proprietary EA/Maxis, RenderWare rendering) |
| Versions tested | Steam app **24720**, Galactic Adventures exe `SporebinEP1\SporeApp.exe`, 25 066 744 bytes, file version 3.1.0.29, SHA-256 `3010A413F9A4A05EE9C4DE24756C1C94C7FB218F70CAD29BCF5BC0B4ABFA2466`. The ModAPI Launcher's own version table calls this build **`Steam_October2024`**. Tested on Windows, Russian language. |
| Projects | [Spore Multiplayer Mod](https://github.com/emerald194520350-alt/spore-multiplayer-mod) (v1.0.1) · [Spore Multiplayer Mod, Co-Me fork](https://github.com/Co-Me/spore-multiplayer-mod) (Creature-stage patch, written and published here) · [CoopMeld "Spore Coop"](https://coopmeld.com/spore/) (service paused) · [SporeMP](https://github.com/raph559/SporeMP) (GOG build only) · [Spore-Multiplayer](https://github.com/Spore-Multiplayer/Spore-Multiplayer) (empty) · [sunglocto/SporeMultiplayer](https://github.com/sunglocto/SporeMultiplayer) (README only) |
| QA level | `automated` for the Cell-stage mod and for the fork (see QA), `none` for the rest |
| Last updated | 2026-10-22 |

## Where things stand

**The Cell stage of the Spore Multiplayer Mod is verified on the owner's exact
Steam build, with two real game processes talking to a real session server — and
the co-op session now continues past Cell into the Creature stage.** The second
half is a patch written here and published as a fork of the upstream mod;
upstream itself still stops at Cell. The owner's priority (the **Space** stage)
is **not covered by anything that exists**.

**What the milestone does:** the shared session moves from `cell` to `creature`
and both windows agree on it, the native landfall runs instead of the version 1
end screen, and the land editor stays shared. **What it does not do:** sync
Creature-stage gameplay — two players still do not see each other's creature, a
shared planet, shared herds or creature interactions. The Creature simulator
stays local to each window; that needs its own reverse engineering (see
[Next steps](#next-steps)).

Five Spore co-op projects were found. Two are real code (the *Spore Multiplayer
Mod*, whose licence metadata is ambiguous, and the GPL *CoopMeld Spore Coop*),
one is a prototype pinned to a different store build, and two are empty. All of
them stop well short of Space, and the one that aimed furthest has a **paused
live service** (HTTP 503) and had only reached "Civilization and Space — coming
later".

## Existing work

### Spore Multiplayer Mod — `emerald194520350-alt/spore-multiplayer-mod` ✅ works

| | |
|---|---|
| Author | `emerald194520350-alt`, single author |
| Licence | Ambiguous upstream: the `LICENSE` file is MIT, but 20 core source files (including `native/Probe.cpp` and `server/SessionServer.cs`) carry `SPDX-License-Identifier: GPL-3.0-or-later` headers and 38 carry no header at all. Left untouched upstream; the fork documents it and releases the patch under GPL-3.0-or-later. |
| Latest release | v1.0.1, 2026-09-27, protocol 10 |
| Asset | `spore-multiplayer-mod-v1.0.1-windows.zip`, 357 314 bytes, 8 downloads |
| SHA-256 | `787cac1f7b262dba76093923a75dcecc07916c4a5185182b94893b8c4d181e78` (published sum, verified) |
| Activity | 21 commits between 2026-09-17 and 2026-09-27, then silent |
| Issue tracker | **none**. There is nowhere to report a bug. |
| Stages | **Cell only** (1 of 5) |

Architecture, read from the source:

```
SporeApp.exe window 1 (host)  ─┐
  native/Probe.cpp   C++ client, Spore ModAPI + Detours hooks
  native/CoopNet.cpp JSON over length-prefixed TCP
                                 ├──► SporeCoop.Server.exe   C# session server
SporeApp.exe window 2 (guest) ─┘     server/SessionServer.cs
                                        central broker, owns shared state,
                                        broadcasts position / appearance /
                                        npcSnapshot / worldAction
```

The server is a plain `TcpListener` process with `--listen` and `--port`, and the
clients read their address from `SPORE_COOP_SERVER`, `SPORE_COOP_PORT`,
`SPORE_COOP_TOKEN`, `SPORE_COOP_ROLE` and `SPORE_COOP_PROFILE`.

What it syncs: player movement and appearance, NPC replication, shared growth,
DNA and parts, diet history, species name, the creature editor (shared model plus a
shared DNA budget with revision merging), shared progress and unlocks, the History
timeline, one-sided campaign saves (the owner writes, the guest is blocked), pause,
and editor entry and exit.

What it does not do: the Creature, Tribal, Civilization and Space stages. After
Cell the mod shows an end screen instead of the landfall transition — confirmed
in-game, see QA.

### Spore Multiplayer Mod, Co-Me fork — https://github.com/Co-Me/spore-multiplayer-mod ✅ patched and published here

| | |
|---|---|
| Base | `emerald194520350-alt/spore-multiplayer-mod` @ `d837f83` (v1.0.1 + docs translation) |
| Patch commit | `6b8f436` "Creature stage: continue the co-op session past the Cell stage" — 11 files, +343 / −12 |
| Licence of the patch | GPL-3.0-or-later |
| Stage coverage | Cell → **Creature transition**; Creature gameplay not synchronised |

One code commit on top of upstream (`6b8f436`), plus a documentation commit
(`103db8b`) that records the verified patch-boundary compatibility. The upstream
behaviour and the reason it exists:

> Upstream intercepts the landfall transition, replaces it with the version 1
> end screen and closes the cooperative session. `native/CellStageEnd.h` holds
> the gate, and the server also refused editor work once the Cell stage was
> complete.

What the patch changes:

| Area | Change |
|---|---|
| Native client | `Simulator::IsCreatureGame()` (from the vendored Spore ModAPI SDK) detects the entered stage. A new `stageTransition` message reports it to the server. The report repeats every 2 s **while the server still reports `cell`**, because the other player may still be finishing the stage, and stops as soon as the server confirms `creature`. A server without the field is reported once. |
| Native client | In a live cooperative session the Cell-stage ending is no longer intercepted and the version 1 end screen is dismissed; the end screen is still used when the mod runs alone. |
| Session server | New `stageTransition` handler. Either peer may send it, it is idempotent, and it is accepted only with an accepted invitation, a matching world generation, a completed Cell stage and the supported stage name. The server advances the shared stage, clears the editor and evolution state and broadcasts the new stage to both windows. |
| Session server | The Cell stage completion became a fact about the **shared world** instead of transient session state, so it survives a server restart; the shared stage is persisted the same way. The land editor is no longer blocked by the Cell stage completion, because the editor is the design step for the creature. |
| Protocol | **Stays at 10.** The message is additive, so an unmodified 1.0.1 client still connects to the patched server and vice versa. |
| Server banner | `Supported stages: Cell, Creature.` |

The fork is the deliverable of this milestone: it keeps the upstream history, so
the patch is visible as one commit and can be sent to the author as a pull
request.

### CoopMeld "Spore Coop" — https://coopmeld.com/spore/ ⚠️ service paused

The only project that ever went past the Cell stage. GPL sources (installer,
native mod, launcher), 2–4 players, online rooms, Cell + Creature + Tribe.

- The **live service is paused**: `/api/check?nonce=…` → **HTTP 503 "Release
  unavailable."**; `/lobby` → **HTTP 426** `{"error": "Update SporeCoop in the
  launcher before playing."}`.
- Its own site lists Civilization and Space as *"Coming later"*.
- Its build guard (`launcher/game_build.h`) matches this owner's install
  (`October2024Steam` = 25 066 744 bytes **and** version 3.1.0.29), and the
  release policy is RSA-signed with the exe hash in a `games` set — so the build
  is compatible, only the service is down.
- Its policy requires `stage: cell`.

### SporeMP — https://github.com/raph559/SporeMP ⚠️ wrong store build

GPL-3.0, has a roadmap for all stages, ships a dedicated server. Its byte-level
guard is pinned to the **GOG** build (24 895 536 bytes, SHA-256 `dc04aee5…`) and
**rejects the Steam build** this owner has. Prototype quality.

### Dead ends

- [Spore-Multiplayer/Spore-Multiplayer](https://github.com/Spore-Multiplayer/Spore-Multiplayer)
  — an empty Visual Studio skeleton; `dllmain.cpp` is the ModAPI template,
  `Client/README.md` says *"Files for the Client. TBA"*.
- [sunglocto/SporeMultiplayer](https://github.com/sunglocto/SporeMultiplayer) —
  a 545-byte README, no code.
- `NeDether/SporeMultiplayer` — **404 / gone**.

Two more Spore projects exist but are not co-op:
[SporeNEXT](https://github.com/bisfranz/SporeNEXT) is a mod manager, and the
[Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI) is the C++
modding framework everything is built on.

## How the game works (for co-op)

- **Engine**: proprietary EA/Maxis. Rendering through RenderWare. No shipped
  multiplayer and therefore **no existing netcode** to reuse.
- **Modding surface**: the Spore ModAPI SDK is a reverse-engineered C++ SDK that
  hooks the game with Microsoft Detours and is injected by the
  [SPORE ModAPI Launcher Kit](https://github.com/Spore-Community/ModAPI-Launcher-Kit).
  The SDK ships Ghidra import files (`SporeGhidra_march2017.xml`) for deeper work.
- **The launch path is the hard part.** The kit is a 32-bit .NET app that
  `CreateProcess`es the game **suspended**, injects `ModAPI.DLLInjector.dll`
  with `WriteProcessMemory` + `CreateRemoteThread(LoadLibraryA)`, tells the
  injector which DLLs to load, and lets the game run. Every step has to be
  right, and the kit's GUI flow does two things the upstream script skips:
  - it writes **`steam_appid.txt`** with the app id (`24720`) into the game's
    `SporebinEP1` folder, so `SteamAPI_Init` succeeds when the game is started
    outside Steam — without it the game exits immediately with **code 53**;
  - it copies `coreLibs\SporeModAPI.<version>.dll` over `mLibs\SporeModAPI.dll`
    on **every** launch, so a second concurrent window from the same kit folder
    fails with *"cannot copy SporeModAPI.dll, the file is in use by another
    process"*. **Two windows therefore need two separate kit folders.**
- **Version detection** is by exe size: `ModAPI.Common.GameVersion.ExecutableSizes`
  maps 25 066 744 → `Steam_October2024`, 24 885 248 → `Steam_March2017`,
  24 895 536 → `GOG_October2024`, 24 909 584 → `Disc`, and so on. The upstream
  script labels the october build "Steam March 2017"; both resolve to the same
  core DLL (`SporeModAPI.march2017.dll`), so the label is harmless for the kit.
- **Build pinning**: the mod detours functions at hashed RVAs
  (`VerifiedMotionCode`), so it only works on the executable it was built
  against. A different Galactic Adventures build breaks it. The mod's
  `Check-Environment.ps1` whitelists five accepted exe sizes, and this build is
  one of them.
- **Save system**: a campaign lives in `%APPDATA%\Spore\Games` plus creature assets
  under `Documents\My Spore Creations`. The mod isolates a second profile at
  `%APPDATA%\SporeCoop2` by detouring `App::cAppSystem`, and hides the borrowed
  campaign copies from the guest's galaxy menu (`native/BorrowedCampaigns.h`).
- **World ownership**: the invitation sender is the world owner. It applies
  authoritative world changes and writes the campaign; the guest's writes are
  blocked at the native save hooks (`native/CampaignSync.h`).
- **Stage scope**: the Cell stage is a 2D single-screen world. The Space stage
  is a different game — 3D galaxy flight, planets, cities — and nothing in any
  of these projects touches it.

## What we changed

### Milestone 1 — research and launch glue, no mod code changed

**No mod code was changed.** The native DLL is pinned to hashed RVAs and would
need MSVC v143 to rebuild, which the owner declined; the **prebuilt release DLL**
was used as-is. What was added is the missing launch glue, and the diagnosis
that explains why the mod "does not start" for anyone following the upstream
README:

| Problem | Root cause | Fix | Proof |
|---|---|---|---|
| Game exits with **code 53** when started outside Steam | the Steam build needs `steam_appid.txt` with its app id for `SteamAPI_Init`; the upstream script bypasses the kit's `Execute()` flow that writes it | write `steam_appid.txt` = `24720` into `<game>\SporebinEP1` | direct launch then runs (≈ 408 MB, window present); with the wrong id `17390` it exits with code 86 instead |
| Second window fails to launch | the kit overwrites `mLibs\SporeModAPI.dll` on every launch, and the first game process holds it open | use a **second copy of the kit** for the second window | two windows start and stay up |
| Mod never appears to load | none — this one was a *reporting* artefact: `Get-Process .Modules` from a 64-bit shell lists only the 7 WoW64 host modules of the 32-bit game | use the mod's own probe log (`%TEMP%\SporeCoop.Probe.log`) as the evidence | the log carries the game PID and the mod's initialisation lines |

### Milestone 2 — the Creature stage: the native client and the server were changed

The toolchain is now on the machine (Visual Studio Build Tools 2022 17.14.42,
MSVC v143 14.44.35207, Windows SDK 10.0.22621.0, installed to `D:\VSBuildTools`
after the owner approved it), so the pinned native DLL could be rebuilt from
source and patched. The result is one code commit on top of upstream, published
as [Co-Me/spore-multiplayer-mod](https://github.com/Co-Me/spore-multiplayer-mod)
(commit `6b8f436`; `103db8b` adds the verified compatibility note). Upstream's own
`LICENSE` and sources are untouched.

| File | Change | Why |
|---|---|---|
| `native/CoopNet.h`, `native/CoopNet.cpp` | `Snapshot.stage` field and a `SubmitStageTransition` message | the shared stage has to travel over the wire |
| `native/Probe.cpp` | reports the transition when `Simulator::IsCreatureGame()` is true, repeating every 2 s while the server still reports `cell` | the other player may still be finishing the stage, and the report must survive a lost message |
| `native/CellStageEnd.h` | in a live cooperative session the Cell-stage ending is no longer intercepted and the version 1 end screen is dismissed | otherwise the session stops at Cell |
| `server/SessionServer.cs` | `stageTransition` handler, persisted `Stage` and `CellStageComplete`, editor no longer blocked by the Cell stage completion | the server owns the shared stage and the shared world facts |
| `tests/native-stage.test.mjs` | new end-to-end test: two real native clients against the real server | proves the transition over real TCP, not over a mock |
| `tests/server.test.mjs`, `tests/native-visual.test.cpp`, `tests/native-source.test.mjs` | new protocol, native and source assertions for the stage behaviour | regression cover |

The rebuilt client is `bin\SporeCoop.Probe.dll`, 857 088 bytes, SHA-256
`07178AE31CD1ADB7718EA0D2E91323F2872311F8F893C2FBEABD94B895ED2405`.

**Design decisions and why**

- **A new message, not a protocol bump.** The protocol stays at **10**. A
  `stageTransition` message is additive: an unmodified 1.0.1 client still
  connects to the patched server, and the patched client still connects to an
  unpatched server (it reports once and the server ignores what it does not
  understand).
- **Either peer may send it, and it is idempotent.** There is no election and
  no ordering dependency: the transition is a fact about the shared world, so
  the second report is a no-op.
- **The Cell stage completion became a shared-world fact.** Upstream kept it in
  transient session state, so a server restart lost it and the game could not
  continue. It is persisted next to the shared stage now.
- **The editor stays open.** Upstream blocked editor work once the Cell stage
  was complete. The land editor *is* the Creature-stage design step, so blocking
  it would defeat the purpose.
- **The report is driven by the game's own state**, not by a timer that assumes
  the stage was reached: `Simulator::IsCreatureGame()` from the vendored Spore
  ModAPI SDK is the same predicate the game itself uses.

**What is not implemented:** Creature-stage gameplay synchronisation. The
Creature simulator keeps running locally in each window; the shared world state
is still the Cell-stage state plus the stage itself.

## QA

### Suites that were actually run

| Suite | Command | Result |
|---|---|---|
| Upstream server protocol suite, on the untouched v1.0.1 tree (baseline) | `Test-Server.ps1` (rebuilds `SporeCoop.Server.exe` with the .NET Framework `csc.exe`, then runs it) | **PASS: 142 protocol assertions. No gameplay was tested.** |
| Upstream native engine checks, on the untouched v1.0.1 tree (baseline) | `Test-Native.ps1` | **PASS: 115 native visual/network assertions. No gameplay was tested.** |
| Fork's server protocol suite | `Test-Server.ps1` in the fork checkout | **PASS: 148 protocol assertions.** (+6 for the stage work) |
| Fork's native engine checks | `Test-Native.ps1` in the fork checkout | **PASS: 119 native visual/network assertions.** (+4) |
| Fork's native source invariants | `node tests/native-source.test.mjs` | **PASS: native lifecycle source invariants**, now with 8 new invariants for the stage work |
| Fork's native idle check | `node tests/native-idle.test.mjs` | **PASS** — it runs now; it needs `bin\tests\NativeVisualTests.exe`, which could not be built in the first milestone because MSVC was not installed |
| Fork's stage end-to-end test | `node tests/native-stage.test.mjs` | **PASS: native clients and the real server moved the shared world into the Creature stage.** Two real native clients complete the real invitation handshake against the real session server over TCP |
| Patch-boundary compatibility | a raw protocol probe run against both servers | **both directions work.** An upstream-shaped client against the patched server gets `welcome`, `state` (`stage = cell`) and `pong`. A patched client against the **unpatched** upstream server gets `{"type":"error","message":"Unknown message type"}` and the session **survives** — the server rolls the state back and answers the next `ping` with `pong`, and the native client stores the error in `lastError` instead of disconnecting |
| Fork build from a clean checkout | `Build-Probe.ps1 -LauncherRoot "<kit>" -SdkRoot "vendor\Spore-ModAPI"` | **0 errors** with MSVC v143 (93 pre-existing SDK warnings), `bin\SporeCoop.Probe.dll` |
| This repo's own tool suite | `python -I tools/test_all.py` | 13 of 15 suites pass (`chaos`, `ownership`, `fakeworld`, `fuzz`, `gameslot`, `gamepool`, `scenarios`, `bots`, `animcheck`, `bridge`, `inspect`, `hotreload`, `buildlock`); 2 fail for reasons that have nothing to do with this game, see below |
| This repo's CI | `python tools/build_directory.py --check` and `python -m unittest tools/mcp/test_coop_mcp.py` | `OK: 303 entries` and `Ran 13 tests … OK` |

The two failing suites of this repo's own tooling were checked against a
**pristine `upstream/main` checkout** (`b877c9e`) and are not caused by this
work:

- `mutants` fails there too, with the same assertion
  (`check_cpp_shipped_build_ignores_twins` — "the test build contains the twin
  name"), so it is a pre-existing issue of the repo's C++ tooling under
  MSVC v143.
- `flight` passes in a checkout with an ASCII path and fails in this one
  because the workspace path contains Cyrillic characters: the tool builds the
  replay command as `"C:\...\toy_replay.py"` inside an already-quoted string
  (`can't open file '...\"C:\...\toy_replay.py\"'`).

The upstream "154 native engine checks" figure quoted in the first milestone was
the author's own claim about the release; the suite that actually ships builds
**115** assertions on the untouched tree and **119** with the patch.

The repo's own tool suite exercises this repo's sync tooling, which this game
does not use; it is listed because it is part of the CI gate for this write-up.

### End-to-end test run here (milestone 1, upstream client)

Real game clients, real session server, the owner's Steam build, launched from a
**test copy** of the install (the owner's own install and saves were never
touched).

**Setup**

1. `steam_appid.txt` = `24720` written into `<game>\SporebinEP1` of the test copy.
2. `SporeCoop.Server.exe --listen 127.0.0.1 --port 5523 --host-token <host> --guest-token <guest> --save <path>\session.json`
3. Host window: `Start-SporeCoopInstance.ps1 -Profile 1 -Role host -LauncherRoot "<kit copy 1>" -GameRoot "<test copy>" -Server 127.0.0.1 -Port 5523 -Token <host>`
4. Guest window: the same script with `-Profile 2 -Role guest -LauncherRoot "<kit copy 2>" -Token <guest>`

**Observed**

- Both game processes stayed alive, launched from the test copy
  (`<game>\SporebinEP1\SporeApp.exe`), not from the Steam install.
- The mod's probe log, for **both** windows:

  ```
  [pid=8252  role=host ] Native build: Sep 27 2026 17:43:00; registered peer renderer.
  [pid=8252  role=host ] Initialize completed; commands registered and cooperative client started.
  [pid=8252  role=host ] Cooperative connection state reset after handshake.
  [pid=20468 role=guest] Profile 2 paths selected: SporeCoop2 and My Spore Creations Coop 2.
  [pid=20468 role=guest] Initialize completed; commands registered and cooperative client started.
  [pid=20468 role=guest] Cooperative connection state reset after handshake.
  ```

- The session server's own stdout:

  ```
  SporeCoop Version 1 server (protocol 10). Supported stage: Cell.
  HOST TOKEN (keep local): …
  GUEST TOKEN (share with friend): …
  LISTENING 127.0.0.1:5523
  Connected: host
  Connected: guest
  ```

- Two **Established** TCP sessions on port 5523, both owned by the server
  process, plus the listening socket.
- The server wrote `session.json` (`"Stage":"cell"`, `"Format":1`, `"Revision":0`).

**In-game proof of the stage limit** — the mod's own log, in the running game,
for both windows:

```
Version 1: verified Cell-stage ending after History; further stages disabled in co-op.
```

and the server prints `Supported stage: Cell.`

### End-to-end test run with the patched client (milestone 2)

The rebuilt DLL was installed into **both** launcher kits' `mLibs\` folders
(installed copy verified by SHA-256 against the build output), and two real game
windows were launched from the test copy of the game against the real session
server on `127.0.0.1:5523`.

**Observed**

- The server logged `Connected: host` and `Connected: guest`.
- The probe log of **both** windows carried
  `Initialize completed; commands registered and cooperative client started.`
  and `Cooperative connection state reset after handshake.`
- `session.json` after the run: `"Stage":"cell"`, `"CellStageComplete":false`,
  `"InviteAccepted":false`.
- All game and server processes were stopped afterwards (0 left running).

**What this does and does not prove.** It proves the patched DLL loads in the
real game and that both windows talk to the patched server. It does **not**
prove the stage transition itself: the invitation is an in-game UI click, so
`InviteAccepted` stayed false and no window ever entered the Creature stage.
The transition is proven by `tests/native-stage.test.mjs` (two real native
clients, the real server, real TCP) and by the source invariants, not by a
gameplay session. See [Not verified](#not-verified).

Evidence kept locally (not in the PR): the probe log, the server log and the
session file of that run.

### Not verified

- **The Cell stage was never played to its end in a running game.** So the
  landfall, the land editor opening, the stage report and the server's stage
  advance were **not** observed in the real game. They are proven by the
  end-to-end native test and the source invariants, not by a gameplay session.
  This is the single most important gap in the milestone.
- **Two players in the same world.** The invite is in-game UI: the host clicks
  "Invite", picks a player in a picker, the guest clicks "Accept"
  (`kInviteButtonID` / `kInvitePlayerButtonID` / `kAcceptInviteButtonID`). The
  diagnostic commands `coopStatus` / `coopSpawn` / `coopJoin` are registered via
  `CheatManager.AddCheat` and have no scriptable trigger.
- **Creature-stage gameplay.** Nothing about the creature is synchronised: two
  players do not see each other's creature, a shared planet, shared herds or
  creature interactions, and the Creature simulator runs locally in each window.
- The end screen, saving, return to the menu and fragment drops — the author's
  own list of unconfirmed behaviour.
- Remote play between two machines (world transfer is not implemented).
- The upstream "154 native engine checks" claim (the shipped suite builds 115
  assertions; the 154 figure was the author's own).

## How to play

Not ready to play with friends yet. The verified local setup (Cell stage, one PC,
two windows):

1. Install Spore: Galactic Adventures from Steam and the
   [SPORE ModAPI Launcher Kit](https://github.com/Spore-Community/ModAPI-Launcher-Kit)
   — and make a **second copy of the kit folder**, one per window.
2. Close both game windows, extract `spore-multiplayer-mod-v1.0.1-windows.zip`.
   For the Creature-stage patch, use the
   [Co-Me fork](https://github.com/Co-Me/spore-multiplayer-mod) instead; it
   needs `.\Build-Probe.ps1` and `.\Build-Server.ps1` to be run once, because
   the release zip ships no binaries for a patched client.
3. Write `24720` into a file `steam_appid.txt` in `<game>\SporebinEP1`
   (the app id of the executable that runs there). Without it the game exits
   with code 53.
4. Start the session server:
   `SporeCoop.Server.exe --listen 127.0.0.1 --port 5523 --host-token <host> --guest-token <guest> --save session.json`
5. Start the host window with `Start-SporeCoopInstance.ps1 -Profile 1 -Role host
   -LauncherRoot "<kit copy 1>" -GameRoot "<game dir>" -Server 127.0.0.1 -Port 5523
   -Token <host>`, and the guest window with `-Profile 2 -Role guest
   -LauncherRoot "<kit copy 2>" -Token <guest>`.
6. Load a Cell world in the host window, open the Esc menu, choose "Invite friend
   to co-op", accept in the guest window.

The upstream scripts hardcode the author's machine (`C:\Games\SPORE Collection`,
`C:\ProgramData\SPORE ModAPI Launcher Kit` and a second copy named
`… Launcher Kit 2`), so they need repointing for any other install.

## Known issues

- **The Creature stage is a transition, not a shared stage.** The session
  reaches the Creature stage and both windows agree on it, but the creature
  itself is local to each window: no shared creature, no shared planet, no
  shared herds, no creature interactions. The other three stages — Tribal,
  Civilization, Space — are not playable in co-op by any project.
- **The Space stage has no implementation at all.** This is the honest answer to
  "can I play Space co-op with friends today": no.
- Two windows on one PC are what was verified. Remote play between computers is
  unfinished and world transfer between machines is not implemented.
- The invite/accept flow still needs an interactive confirmation by a human, and
  the Cell stage was never played to its end in a running game, so the landfall
  and the stage report are unconfirmed in-game.
- Only one save is kept: the world owner's. The guest's campaign is protected from
  being written.
- The guest needs a copy of the host's campaign. The launcher does this copy
  locally; across machines it has to be done by hand.
- The mod detours functions at hashed RVAs, so a game update breaks it.
- Rare save states, tutorials and unusual editor transitions may still misbehave.
- The launcher kits, the game install and the two profiles are all hardcoded
  paths that have to be repointed per machine.
- The upstream licence metadata is inconsistent (MIT file, GPL SPDX headers);
  the patch is released under GPL-3.0-or-later, but that does not resolve the
  upstream ambiguity.

## Next steps

1. **One interactive session** to confirm the invite → accept → two cells in one
   world, and then the Cell stage played to its end, so the landfall, the land
   editor and the stage report are observed in the real game. This is the last
   unverified piece of the milestone and it needs a human at the keyboard (or a
   UI bot).
2. **Creature-stage gameplay synchronisation** — the real next milestone.
   The reference implementation is CoopMeld's GPL source
   (`creature_hooks.inc`, 76 KB, plus `herd_sync.inc` and `interaction_sync.inc`),
   which is the only code in existence that reached this stage. The mod's own
   architecture has the pieces to build on: the shared world state, the world
   owner and the broadcast channel already exist, and the native side already
   knows when it is in a creature game (`Simulator::IsCreatureGame()`).
3. **World transfer**, the real blocker for remote play: the guest's profile has
   to receive the host's campaign and creature assets. Everything else is
   plumbing — the transport is already TCP and the server is already a
   standalone process.
4. **Decide the target stage.** Cell-stage co-op is one product; the Space stage
   is a different game (3D galaxy flight, planets, cities) and would be new
   development, not a port. CoopMeld's "coming later" is not a plan.
5. **Send the patch upstream.** The upstream project has no issue tracker, so
   the route is a pull request to
   `emerald194520350-alt/spore-multiplayer-mod`; the fork keeps the history so
   the diff is one commit. The two launch fixes (the `steam_appid.txt` step and
   the "two kit folders because the kit overwrites its own DLL" note) go in
   the same pull request.
