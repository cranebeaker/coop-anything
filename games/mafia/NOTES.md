# Mafia: The City of Lost Heaven (2002): working notes

Written while working, following coop-anything `AGENTS.md`. Playbook read at
`cranebeaker/coop-anything@main` on 2026-10-23 (the clone failed on a path-length error, so the
files were read from GitHub raw URLs).

## Phase 0: triage

### Game identity (observed on the owner's PC, read only, nothing changed)

| Field | Value | How |
|---|---|---|
| Title | Mafia (The City of Lost Heaven), original 2002 release, not the Definitive Edition | Steam manifest name `Mafia`, exe version info `MAFIA` / Illusion Softworks |
| Store | Steam, app `40990` | `steamapps/appmanifest_40990.acf` |
| Build | Steam buildid `2300614` | same manifest |
| Exe version string | `Master 1.0` (FileVersion resource) | PowerShell `VersionInfo` |
| `Game.exe` SHA-256 | `303eb95ee2de3433511ce0cb518921dcb96b62b0f64ebfcc436723ff5083f298` | `Get-FileHash`; identical copies at `<game dir>\Game.exe` and `<game dir>\Mafia\Game.exe` |
| `LS3DF.dll` SHA-256 | `e211c85009440d6c032f2074d79184f074a9a046574648745c2594b4cef178a6` | `Get-FileHash` |
| `rw_data.dll` SHA-256 | `30783f20cdd5175dc106970492b39f6509d9b5a176d0f0fa171f0e7534eb1b5d` | `Get-FileHash` |
| `INet.dll` SHA-256 | `f50990a80093029db2d672286a42831d61baf3672c7831ae20b832565ad61aef` | `Get-FileHash` |
| Engine | LS3D (`LS3DF.dll`, `.dta` archives, Bink video) | file listing |
| OS | Windows 11 Pro 10.0.26200 | session environment |
| Layout | Nested: `<game dir>\Mafia\` holds the game; the root holds a second `Game.exe` and `setup.exe` | file listing |

Other observations from the install:

- The Steam install script enables the Windows `DirectPlay` feature and installs `mafia.sdb`
  (a compatibility shim database that redirects the game's HKLM registry key to HKCU).
- `INet.dll` ships with the game. Whether the retail game has any usable netcode is **unknown**
  (not reverse engineered yet).

### Prior work in this repo

- `data/directory.yaml` has one Mafia (2002) entry: Lost Heaven Multiplayer / MMP, `status: partial`,
  `story: 'no'`, `open_source: unknown`, `checked: 2026-10-16`.
- No `games/mafia*` write-up exists.
- GitHub issue/PR search for "mafia" in `cranebeaker/coop-anything`: no Mafia issue or PR (the only
  hit was the Horizon Forbidden West claim issue #83, unrelated).

## Phase 1: research

Searches run (2026-10-23):
- WebSearch: `Mafia The City of Lost Heaven co-op mod story campaign`
- WebSearch: `Lost Heaven Multiplayer LHMP github Mafia 1`
- WebSearch: `MafiaHub Oakwood Mafia 1 classic multiplayer github`
- WebSearch: `Mafia 1 2002 multiplayer mod 2025 OR 2026`
- WebSearch: `"Mafia Connected" Lost Heaven multiplayer client`, `connectedmp Mafia Connected Mafia 1 multiplayer`
- GitHub API: `orgs/MafiaHub/repos`, `repos/LHMPTeam/lhmp-old`, `repos/Romop5/lhmp-old`, `repos/MafiaHub/MafiaOnline`
- Not checked yet: ModDB/Nexus file pages, archive.org for mafia-mp.eu / lh-mp.eu / oakwood-mp.net,
  MafiaHub Discord, Reddit, Czech-language sources (the game and MafiaHub are Czech/Slovak).

### Finds

**1. Mafia Online / Mafia1Online (MafiaHub)**: the most relevant find, and it is **not in the directory**.
- Repo: https://github.com/MafiaHub/MafiaOnline (created 2026-09-26, last push 2026-09-30,
  1 open PR by zpl-zak, 0 releases). Docs: https://github.com/MafiaHub/MafiaOnlineDocs
- **Targets the exact `Game.exe` SHA-256 of the owner's install** (`303eb95e…f298`, Steam app 40990).
  The README says the launcher refuses other hashes; GOG/disc exes need their own address audit.
- What it syncs, from its own README: server-owned player replicas (transform, health, model),
  server-spawned cars with seats/engine state, chat, mission selection and loading for all clients.
  A recent commit is titled "World: Suppress mission actors while preserving doors and geometry", so
  **story missions are not played together**. It is free-roam / gamemode multiplayer (Free Ride sample,
  a roleplay gamemode), not campaign co-op.
- Its README lists weapons, damage, death, vehicle edge cases, nametags and network-loss testing
  as unfinished "release gates". It is a development build, so players have to build it from source.
- Build: needs the MafiaHub Framework at commit `81b8633b90b53f2ab756f0877f9f099fd68a961b`, CMake,
  Windows x86 client/launcher, CEF web UI, V8, Node.js for the UI build and smoke tests.
- Own QA: a two-client Wine smoke test (`tools/two_client_smoke.mjs`) and a UDP impairment proxy.
  We have **not** run it.
- **License: "MafiaHub OSS license"** (`LICENSE.txt`). Clause 5: derivative works are allowed only
  with the intent to contribute to the original work, and personal or commercial use of the
  original work needs specific prior written permission. **Playing it, or building co-op on it,
  probably needs MafiaHub's permission or an upstream contribution.**

**2. Lost Heaven Multiplayer (LHMP)**: in the directory. Correction needed.
- Source: https://github.com/LHMPTeam/lhmp-old (archived, last push 2018-11-29); fork
  https://github.com/Romop5/lhmp-old (pushed 2018-04-16).
- **License: Apache-2.0** (`LICENSE`, Copyright 2013-2016 LHMP Team). The directory says `open_source: unknown`.
- README: "abandoned", free-roam SA-MP style with Squirrel gamemodes, C++ plus inline assembler,
  Visual Studio 2013, RakNet. Not story co-op.
- Steam discussion (user post, 2018-01-18, unverified): servers shut down in early February 2017.

**3. Mafia: Oakwood (MafiaHub, 2018-2020)**: not in the directory.
- itch.io page https://zaklaus.itch.io/mafia-oakwood lists it as **Canceled** (author Dominik Madarász).
- Repos: `MafiaHub/rc1-oakwood` (2020-08), `MafiaHub/oakwood-bundle` (server, 2022-12),
  `MafiaHub/oakwood-node`, `oakwood-freeride`, `oakwood-map` (archived 2019), `MafiaHub/mafiadm`
  ("Official Oakwood gamemode", 2024-12). The LHMP README says Oakwood supported GOG and Steam versions.
  Free-roam / deathmatch, not story co-op. The client's license has not been checked yet.

**3b. Oakwood update (2026-10-24):** `MafiaHub/rc1-oakwood` holds the client source (premake, `Code/`,
last commit 2020-08-11 "Uploading the BugReporter application"). Its README says "English version of
Mafia 1.2/Steam/GOG is required" and claims sync of players, vehicles and combat. Its `LICENSE` is a
custom agreement derived from the SA-MP license; clause (f) reads: "You may not create or distribute
derivative works". Not usable as a base. itch.io status checked directly: "Canceled". Added to the
directory as `abandoned`, `open_source: partial`.

**4. Unnamed 2-player co-op for Mafia 1.2 (user "Cage", 2018)**: not in the directory.
- Steam thread https://steamcommunity.com/app/40990/discussions/0/1693785035810042404: a host runs
  the game and the second player joins in a browser over an MPEG1 stream. Posts from 2023-10 say
  the video link is dead and the project is gone. Looks like streaming rather than a sync mod.

**5. "Mafia Connected" roleplay server**: mentioned in the same Steam thread (2024-02-26), no link.
  Not found by two web searches. **Unknown** what it is.

**6. Engine-level resources (not co-op, useful for building):**
- `MafiaHub/MafiaFormats`: reverse-engineered LS3D file formats (archived 2019).
- `MafiaHub/OpenMF-Archived`, `MafiaHub/MafiaUnity`: open engine re-implementations (archived).
- MafiaOnline `docs/native_contract.md` and `docs/native_script_command_mapping.md`: audited hooks
  and mission-script command mapping for this exact exe. Not read yet; most relevant for story sync.
- `MafiaHub/awesome-mafia`: list of modding tools (not read yet).

### Playbook gaps noticed

- `playbook/engine-families.md` has no LS3D / Illusion Softworks entry.

## Lists required by AGENTS.md rule 4

**Observed (checked this session):** game identity table above; the MafiaOnline README's target hash
equals our hash; MafiaOnline license clause 5; LHMP is Apache-2.0 and archived; there is no Mafia
write-up or Mafia issue/PR in coop-anything.

**Derived:** no project found syncs the story campaign. MafiaOnline is the closest technical base
(exact exe match, active, server-authoritative), but its license limits personal use and derivatives.
LHMP is the only permissively licensed base, but it is old and its hooks probably target another
exe build.

**Guesses (not verified):** LHMP probably targets an older exe (1.0 or 1.2 retail) rather than this
Steam build, because the Oakwood README advertises Steam/GOG support as new. Story co-op would need
host-authoritative mission scripts with mirrored mission actors on clients.

**Unknown:** whether any MafiaOnline binary build is distributed (e.g. MafiaHub testers page or
Discord); whether MafiaHub would grant play permission or accept story co-op upstream; what
"Mafia Connected" is; whether retail `INet.dll` contains usable netcode; which exe LHMP needs.

## Planning pass (2026-10-23, second session turn)

The options comparison and milestone plan are in [PLAN.md](PLAN.md).

### Observed

- `Game.exe` PE: i386, ImageBase `0x400000`, SizeOfImage `0x2DAD76` (image ends at `0x6DAD76`),
  entry point RVA `0x2123B4`, four sections `.text .rdata .data .rsrc`. There is **no `.bind`
  section** and `.text` entropy is 6.51, so the exe is **not SteamStub-wrapped** and can be analysed
  statically. The PE timestamp field reads 2030-10-02, so it is not a real build date.
- There is no `steam_api.dll` in the install: the game does not use Steamworks itself.
- `Game.exe` imports: `KERNEL32, USER32, ADVAPI32, WINMM, LS3DF, IJoy, rw_data`. `WINMM.dll` is not
  in the KnownDLLs list, so a proxy `winmm.dll` next to the exe would load first.
- **LHMP does not match this exe.** Its hook site `0x49BFEA` is commented in `Client/CHooks.cpp` as
  `C7 86 44 06 00 00` (`mov dword [esi+644h], 0`), but the Steam exe has `E8 41 D2 F6 FF` (a call)
  there. Its player global `0x6F9464` lies outside the Steam exe's image. Bytes at four other hook
  sites also do not look like LHMP's expected code.
- LHMP has about 326 distinct fixed addresses in the 0x4xxxxx-0x6xxxxx range across 15 client
  files: a rough count by regex, which may include some constants.
- LHMP `Loader/Main.cpp` has no exe version or hash check.
- LHMP `CGameSync` syncs weapons, shooting, hits, deaths, vehicle enter/exit/carjack, grenades,
  door state, car explosions, engine state, chat, skins, animations, sounds and respawn. It has no
  mission or story code, and it hooks `PreventCarandPedsSpawn`.
- Licenses: MafiaHub Framework uses the same restrictive MafiaHub OSS license as Mafia Online;
  MafiaUnity is Apache-2.0 (has `DTALoader.cs`); OpenMF-Archived is GPL-3.0 (DTA parser, key
  extractor, `scene2_bin` utility, mission component); MafiaFormats is Apache-2.0 (only checkbin,
  mnu, textdb specs); jovan-s/DTAUnpacker has no license stated.
- `MafiaHub/awesome-mafia` says MafiaDataXTractor 1.0 is for Mafia 1.0 and 1.1 is for
  "1.1/1.2/1.3", and that Project Angelo (an in-game script debugger) is for Mafia 1.0 only. So
  several retail versions exist, and many old tools target 1.0.
- Local tools: VS Build Tools 2026 (MSVC 14.51 with an x86 `cl.exe`, bundled CMake), Windows SDK
  10.0.26100, Python 3.14.5, Node 24.15, Git 2.53, Java 8 only (Ghidra needs 21). No Docker, gcc,
  Wine or Ghidra.

### Guesses (to verify in M1)

- The Steam exe corresponds to retail 1.2 or a later Steam-specific build; LHMP targets 1.0.
- Mission scripts live in each mission's `scene2.bin` inside the `.dta` archives, and story state
  between missions is mostly the mission index plus the profile save, not a large global flag store.
- LHMP's object field offsets (e.g. `+0xE4`, `+0x98`, `+0x644`) may still hold in the Steam
  build even though the code addresses moved.

## M0: workspace (2026-10-23, option D chosen by the owner; MafiaHub not contacted)

### Done and checked

- Project folder `<project>` (short path, so `git clone` works). Playbook cloned at upstream
  `5b86e5b` on branch `game/mafia`.
- Claim issue: https://github.com/cranebeaker/coop-anything/issues/167, opened 2026-10-23 20:43 UTC by
  the owner through the "claim a game" web form. Checked through the API: all three form sections
  are present, and the exe hash, the plan references and the branch name are intact. No label was
  applied.
- The owner forked coop-anything (2026-10-23 20:40 UTC). In the clone, `origin` is the fork and
  `upstream` is cranebeaker/coop-anything, and both `main` branches are at `5b86e5b`. Nothing pushed yet.
- Test copy `<project>\lab\base`: robocopy of the whole Steam install (9,509 files, 2.60 GiB, 8 s,
  0 failed). Every path, size and SHA-256 matches the install. Manifests are in
  `local/manifests/` (private).
- Owner-state guard `local/check-owner-state.ps1`. It compares the Steam install (full SHA-256
  manifest), the Mafia registry keys (HKCU, HKLM WOW6432Node, registry VirtualStore), the file
  VirtualStore, `Documents\Mafia`, `%APPDATA%`/`%LOCALAPPDATA%` `Mafia`, and Steam `userdata\*\40990`
  against a baseline, and prints `RESULT: PASS/FAIL`. Checked: PASS on the clean state; FAIL
  after one fake line was added to the baseline (mutation twin); baseline restored.

### Observed

- **The game has never been launched on this PC.** None of the registry keys or save folders above
  exist. `mafia.sdb` is not registered under `AppCompatFlags\InstalledSDB`, so Steam's install
  script (sdbinst + DISM DirectPlay) has not run yet. It runs on the first launch through Steam.
- `SysWOW64\dplayx.dll`, `dpnet.dll` and `dpwsockx.dll` are 10,240-byte stubs (10.0.26100.4202),
  which suggests the DirectPlay optional feature is off. Not checked with DISM, which needs admin.
- `INet.dll` imports `KERNEL32, USER32, ADVAPI32, ole32, WSOCK32` and contains no "dplay" or
  "DirectPlay" strings. Retail network code over Winsock, plus COM, so DirectPlay might be created
  through `CoCreateInstance`. Not analysed.

### First launch of the test copy (2026-10-23 19:04, owner watching)

Before: guard PASS, `HKCU\Software` exported, firewall rules listed (Steam had added inbound Allow
rules for the real install's `Game.exe` and `setup.exe` only). Launched `<lab>\Mafia\Game.exe` with
the working directory set to that folder, using `local/scripts/m0-first-launch.ps1` (window and
dialog logger, pid-scoped stop).

Observed:
- With no settings saved yet, `Game.exe` starts `setup.exe` and exits with code 0 after about
  2 s. It is effectively a loader on first run. A launcher has to make sure settings exist, or it
  will be holding the wrong pid (playbook `multi-instance-testing.md` §3).
- The Setup dialog (`#32770`, title "Setup") has a Button-class control labelled `     Fullscreen `.
  (Correction after launch 3: like `     Sound ` and ` Display `, this is almost certainly a group-box
  title, not a checkbox. The game then started in FULLSCREEN mode. **There is no evidence of a stock
  windowed mode.**) It also has "Suspend when inactive", resolution, bit depth, refresh rate, the
  Hardware T&L / W Buffer / Triple buffering / VSync toggles, sound options, language, "Save and
  exit" and "Exit".
- `Game.exe` wrote `log.txt` (84 bytes) into its own folder:
  `LS3D Engine V3.95.1  Jun 24 2003  GetVer:395`.
- `setup.exe` contains the string `Software\Illusion Softworks\Mafia` and imports
  `RegCreateKeyExA`/`RegSetValueExA`. It probably saves settings there; which hive it uses is unknown.
- Guard after the first step: PASS. No registry key, save folder or Steam install change.

Derived: the engine build date (June 2003) is later than the 2002 retail release, which fits the
guess that this exe is from the 1.2 patch era.

### Second launch, after Setup (2026-10-23 19:07)

The owner unticked Fullscreen, chose 1024x768 and clicked "Save and exit".

Observed:
- Setup wrote a single binary value, `HKCU\Software\Illusion Softworks\Mafia\LS3D_setup` (63 bytes).
  That is HKCU directly, not the registry VirtualStore. It is therefore **shared with the owner's
  real game**, and the M2 DLL must redirect it per instance. The value layout is not decoded yet
  (the backup `.reg` is kept in `local/backups/m0-launch/`).
- `Game.exe` then stayed running but **never created a visible window in 300 s**. About 5 s after
  start, Windows opened `Fondue.exe` ("Windows Features", the feature-on-demand prompt).
  `log.txt` again holds only the engine banner line. At the timeout the script stopped
  `Game.exe` by pid, plus Fondue, which the test had started.
- DirectPlay was not installed afterwards: the SysWOW64 `dplayx.dll`/`dpnet.dll` are still 10,240-byte
  stubs, and there are no Setup/CBS events.
- No firewall prompt and no firewall rule changes. No files written anywhere in the user profile,
  ProgramData or TEMP. All 9,509 files of the base copy were unchanged; only `log.txt` was added.
- Registry diff over all of `HKCU\Software`: apart from the Mafia key, the only change caused by
  the test was the Windows shell's MuiCache entry for `Fondue.exe`. All other changes came from
  unrelated apps.

Derived (strong, not proven): the game blocks at start-up until the DirectPlay optional feature
is installed. The evidence: Steam's install script for this app enables DirectPlay, the system
DLLs are stubs, the Fondue prompt appeared 5 s in, and no further engine log lines or window
followed. The prompt's text was not captured (the logger only reads `#32770` dialogs).

Cleanup: removed `HKCU\Software\Illusion Softworks` (created entirely by the test; a `.reg`
backup is kept) and moved `log.txt` out of the base copy. Left alone: the Windows MuiCache entry for
`Fondue.exe` (Windows' own cache). Guard after cleanup: **PASS**.

M0 exit test status: **not passed yet**. The copy starts, but it does not reach the main menu
without DirectPlay.

### DirectPlay check before launch 3 (2026-10-23 20:16-20:24)

The owner reported enabling DirectPlay (Windows Features > Legacy Components > DirectPlay).
Observed afterwards:
- `Win32_OptionalFeature`: `LegacyComponents` InstallState=2 and `DirectPlay` InstallState=2
  (2 = disabled). This **contradicts the report**, so launch 3 was not started (AGENTS.md rule 6).
- The SysWOW64 `dplayx/dpnet/dpwsockx.dll` are unchanged 10,240-byte stubs (they import `stub.dll`).
  The WinSxS `directplay4-payload` x86 folders also hold 10,240-byte `dplayx.dll` files, so file size
  may not be a reliable signal. The feature state is the check to use.
- No CBS `RebootPending`/`PackagesPending` keys and no Setup-log events today. `TiWorker` and
  `TrustedInstaller` were running for the whole 4-minute wait; whether that was this feature or
  routine servicing is unknown.
- Guard: PASS. The windowed setting was not restored, so the registry is clean.

Second check (after the owner re-confirmed that the change completed): still `DirectPlay`
InstallState=2. Last boot was 2026-10-18, so no restart happened. CBS package states under
`HKLM\...\Component Based Servicing\Packages` (readable without admin), which give a reliable,
admin-free check:
- `Microsoft-Windows-DirectPlay-OC-Package` and `-OC-WOW64-Package` (10.0.26100.4202 and the
  8457 language packs) are `CurrentState=0x70` (installed). These are the always-present stub layer.
- **Every** `Microsoft-Windows-DirectPlay-OC-Opt-Package` and `-OC-Opt-WOW64-Package` is
  `CurrentState=0x40` (staged, not installed). These carry the optional feature itself.
  So the feature payload is not installed.
- The agent shell has no package identity (`GetCurrentPackageFullName` returns
  APPMODEL_ERROR_NO_PACKAGE), so the registry reads and writes so far were not virtualized.
- Launch 3 is still on hold.

### Launch 3: reaches the main menu (2026-10-23 20:31, owner watching) — M0 exit test PASSED

The owner then ran `DISM /Online /Enable-Feature /FeatureName:DirectPlay /All` in an admin
terminal ("operation completed successfully"). Checked afterwards: `DirectPlay` and
`LegacyComponents` InstallState=1, the `-OC-Opt-*` 10.0.26100.4202 packages at `0x70`, SysWOW64
`dplayx.dll` 226,816 B and `dpnet.dll` 401,408 B (real payload, not stubs), no reboot pending.
Agents must not enable it themselves (it is a system setting), so tell the owner to run that
command. It is exactly what Steam's install script does.

Before: guard PASS. The `LS3D_setup` value was restored from the launch-2 backup. Launched with
`m0-first-launch.ps1 -ShotEverySec 10`, which captures only the game window via `PrintWindow`
with `PW_CLIENTONLY|PW_RENDERFULLCONTENT`. That works for this D3D8 window, even while minimized.

Observed:
- Window class `LS3D`, title `LS3D engine` → `Mafia` after about 3 s, then `Mafia ..zzZZ` whenever it
  loses focus ("Suspend when inactive"). The window is minimized while unfocused.
- Snapshots: Bink logo at 11 s, intro movie at 31 s, an in-engine office scene with an
  **"Enter Profile Name: Default / OK / Cancel"** prompt at 50 s (first run only), and the
  **Main Menu** at 291 s: Intro, Tutorial, New Game, Load Game, Free Ride, Free Ride Extreme
  (greyed), Carcyclopedia, Options, Credits, Change Profile, Quit. The test copy reaches the main
  menu, so the **M0 exit test passed.** The script stopped the game at the 300 s timeout.
- Game `log.txt` (1,198 B): engine banner, `Detected OS: Windows ?? ver. 6.2.9200` (the version
  lie for an exe without a manifest), adapter and driver lines, `Using HARDWARE T&L`,
  `Using SINGLEPASS rendering`,
  `Trying to create device with FULLSCREEN mode: 1280x1024, 0Hz, BackFormat: D3DFMT_X8R8G8B8, DepthFormat: D3DFMT_D24X8`,
  texture formats, `Sound management: 0 free 3D channels claimed`, `Speaker configuration: STEREO SPEAKERS`.
- **Saves and profiles live in `<game dir>\savegame\`**: the first-run profile became
  `savegame\mafia000.sav` (108 B, starting with ASCII `forP`, the rest looks encoded). So each
  isolated copy keeps its own saves, but the settings (`LS3D_setup` in HKCU) are still shared.
- Registry writes caused by the run, besides the restored `LS3D_setup`:
  - `HKCU\Software\Microsoft\DirectInput\GAME.EXE724545620023F000` (Name, UsesMapper), created by
    the DirectInput runtime. The id looks like name + PE timestamp `0x72454562` + `0x0023F000`, so
    the real install will use the same key.
  - `HKCU\Software\Microsoft\DirectInput\MostRecentApplication` was overwritten with GAME.EXE.
  - Windows' own bookkeeping: shell MuiCache (Game.exe, Fondue.exe), Explorer `FeatureUsage\AppSwitched`,
    `Notifications\QuietHours\FullScreenProcess=Game.exe`, a per-app audio PolicyConfig entry, and
    a counter in the PCA `Compatibility Assistant\Store`.
- No firewall prompt and no firewall rule changes.

Derived: `LS3D_setup` bytes 2-5 = `0x500` (1280) and bytes 6-9 = `0x400` (1024), byte 10 = `0x20`
(32 bpp). This matches the logged device mode. The owner's 1024x768 choice did not end up in the
saved value; why is unknown.

Cleanup (guard **PASS** afterwards): removed `HKCU\Software\Illusion Softworks` and the DirectInput
`GAME.EXE…` key, restored `MostRecentApplication` to its pre-test values (the DirectInput tree is
now identical to the pre-test export), and moved `log.txt` and `savegame\` out of the base copy
(back to 9,509 files). Windows' own bookkeeping entries were left alone. All backups and
snapshots are in `local/backups/m0-launch/` and `local/logs/m0-launch3/`.

Consequences for M2: the launcher must supply `LS3D_setup` per instance (registry redirect in the
DLL) or `Game.exe` will start Setup and exit. It has to force a windowed mode in-process, and keep
"Suspend when inactive" off for background copies. It also has to answer or pre-create the
first-run profile prompt.

### Unknown (to settle on the first test launch)

### Unknown (to settle on the first test launch)

- Whether the copy starts without Steam, whether it asks for DirectPlay, whether it needs
  `setup.exe` first, where it writes settings and saves when unshimmed, and whether it can run
  windowed without our DLL.

## M1: static recon (started 2026-10-23)

### Tool setup (owner approved each download; checked)

| Tool | Version | File | Size (B) | SHA-256 | Source |
|---|---|---|---|---|---|
| Eclipse Temurin JDK | 21.0.12.1+1 | `OpenJDK21U-jdk_x64_windows_hotspot_21.0.12.1_1.zip` | 205,073,461 | `f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e` | github.com/adoptium/temurin21-binaries, tag `jdk-21.0.12.1+1` (hash matched the Adoptium API, the GitHub asset digest and `.sha256.txt`) |
| Ghidra | 12.1.4 | `ghidra_12.1.4_PUBLIC_20260921.zip` | 569,649,598 | `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` | github.com/NationalSecurityAgency/ghidra, tag `Ghidra_12.1.4_build` (hash matched the release notes and the asset digest). 12.2 was one day old, so 12.1.4 was chosen. |
| `DecompileAll.java` | cranebeaker/deus-ex-coop @ `b5f7b75` (MIT) | `tools/ghidra_scripts/DecompileAll.java` | 2,071 | `ca387a16c52016e7e7b42208bb2701b0fe957e3781874be9e3744a900d460fd1` (git blob `3a29838`) | Read in full before use: it decompiles every function to `<out>/<program>.c` plus `<program>.functions.tsv`, and does no network or other file access. |

- Unzipped with Windows `tar.exe` into `<project>\tools\jdk\` and `<project>\tools\ghidra\`. Nothing installed;
  the system Java 8, PATH and registry are untouched.
- `support\launch.properties` (original kept as `launch.properties.orig`) now sets `JAVA_HOME_OVERRIDE` to the
  project JDK and `-Dapplication.settingsdir/cachedir/tempdir` plus `-Djava.io.tmpdir` to
  `<project>\tools\ghidra-home\{settings,cache,temp}`. Checked after a headless run: no
  `%APPDATA%\ghidra`, `%LOCALAPPDATA%\ghidra` or `%USERPROFILE%\.ghidra`, and no new ghidra items in
  `%TEMP%`. Ghidra adds `<user>-ghidra` to those folder names.

### Smoke test (checked)

`analyzeHeadless.bat <project>\re\ghidra-projects smoke2 -import <lab>\Mafia\IJoy.dll -scriptPath <project>\tools\ghidra_scripts -postScript DecompileAll.java <out> -deleteProject`
finished with exit 0 in 33 s: `DecompileAll: 198 functions, 0 failed`, `IJoy.dll.c` 226,806 B and
`.functions.tsv` 16,585 B. The import is read-only (`IJoy.dll` hash unchanged) and the temporary project
was deleted. Decompiler output stays in `<project>\re\`, outside this hub repo (rule 7).

**Pitfall (candidate playbook fix for `reverse-engineering.md`):** headless Ghidra resolves a
`-postScript` name **against the current working directory first**. The first run was started from
the folder where `DecompileAll.java` had been downloaded, so Ghidra picked that copy and failed with
`Failed to find source bundle containing script` (`GhidraScriptLoadException`), even though the
analysis still reported success and exit code 0. Run headless from a folder without a
same-named script, and check the log for `DecompileAll:` rather than trusting the exit code.

### Full headless decompile of `Game.exe` and `LS3DF.dll` (2026-10-23, checked)

Runner: `<project>\re\run-decompile.ps1`. It checks both input SHA-256 values against triage before
running, imports both files from the test copy into a kept Ghidra project `re\ghidra-projects\mafia`
(55 MB), runs auto-analysis and `DecompileAll.java`, and writes everything to `re\decomp\v1\`.

| Binary | Analysis | Functions in index | Decompiled | Failed | Named (non-`FUN_`) | `.c` size |
|---|---|---|---|---|---|---|
| `Game.exe` | 220 s | 3,717 | 3,667 | 2 | 88 | 9,148,921 B |
| `LS3DF.dll` | 66 s | 1,508 | 1,485 | 0 | 390 | 2,440,812 B |

The whole run took 735 s, exited 0, and produced 0 `ERROR` lines in the console log. The two failures:
`FUN_0046d3b0` (`process: timeout` at the script's 60 s per-function limit) and `FUN_004ab910`
(empty error message). Rerun those with a longer timeout when needed. There is no symbol information:
`Game.exe` names are almost all `FUN_*`, and `LS3DF.dll`'s 390 names come from its exports and imports.
Afterwards, still no Ghidra folders in `%APPDATA%`, `%LOCALAPPDATA%` or `%USERPROFILE%`.

Where the script keywords are used (regex over `Game.exe.c` for Ghidra's `s_<KEYWORD>_<addr>`
string labels): `FUN_0051cdd0` references 246 keyword strings, `FUN_00463960` 114 and `FUN_0051b470` 1.
Only 149 of the 512 keywords appear as such labels; the rest are probably reached through a pointer
table that Ghidra did not label. **Guess:** `FUN_0051cdd0` and `FUN_00463960` are the Mafia Script
parser/compiler (text keyword → opcode), possibly one routine split in two. Confirm by xrefs and
by renaming in the project before relying on it.

### DTA reader and mission script dump (2026-10-23, checked)

Tools (our code, in `<project>\re\tools\` for now, later the mod repo):
- `mafia_dta.py`: Python port of MafiaHub/MafiaUnity `DTALoader.cs` + the key table in `FileSystem.cs`
  (Apache-2.0, commit `0d7a12f03ce4385173377c3e9f457dcc2e891f24`, attribution in the header). Format:
  magic `ISD0`; header and file table XOR-ed with a repeating 8-byte key (`key2, key1` little-endian);
  per-file blocks either raw (type 0) or a 16-bit-flag LZSS/RLE (type 1); DPCM audio blocks are not decoded.
  Commands: `list <game dir> <tsv>`, `extract <game dir> <out> <regex>`, which writes `<out>/<archive>/<path>`.
- `scene2_scripts.py`: walks `scene2.bin` chunks (u16 type, u32 size incl. header; layout per MafiaUnity
  `Scene2BINLoader.cs`) and dumps every script to text, plus `index.tsv` and `command-usage.tsv`.

Observed:
- **MafiaUnity's per-archive keys work on this Steam build.** 11 archives (a1-a9, aa, ac; no a0/ab in this
  install) list in about 4 s with **27,018 entries**, e.g. a1 625, a2 3,219, a6 16,400, aa 3,282.
- **80 `missions\*\scene2.bin`**: 79 in `a1.dta`, plus `missions\mise16-krajina\scene2.bin` again in `a8.dta`
  (different SHA-256, presumably a patch that overrides a1; which copy the engine loads is unknown).
  This matches Mafia Online's "77 gameplay missions + 3 menu-only scenes". Every extracted file
  decompressed to exactly the size in its archive header.
- Mission folders: 63 `mise*` story scenes (20 chapters split into scenes such as `mise08-hotel`,
  `mise08-kostel`, `mise08-mesto`), 6 free ride (`freeride`, `freeridenoc`, `freeitaly`, `freekrajina`,
  `freekrajinanoc`, `extreme`), 6 cutscene scenes (`intro`, `intermezzo*`, `fmv *`), plus `00menu`,
  `tutorial`, `autosalon` and `02normancela`.
- Scripts live in three places in `scene2.bin`:
  1. Special objects of type 5 (script): chunk `0xAE24` = 10 bytes, u32 length, text. **2,602 scripts.**
  2. Special objects of type 27 (character: enemies, NPCs, partners): the `0xAE24` data holds character
     fields, then u32 length and text. **872 character scripts.**
  3. Init chunk `0xAE50` with `0xAE51` entries: u8, u32 name length, name (`GameInitStart`,
     `GameInitEnd`, ...), u32 text length, text. **178 init scripts.**
- Total **3,652 scripts, 119,276 lines, 2,003,311 bytes**, dumped to `<project>\re\scripts\<archive>\<mission>\`
  (game-derived; not committed). No dumped file contains NUL or control bytes.
- The language is plain text with `\r\n` lines, e.g. `dim_act 6`, `findactor 0,"Paulie"`,
  `settimeoutevent 1,201`, `label 101`, `wait 121000`, `setevent 0,207,101`, `goto 101`,
  `endofmission 0,01980004`. Commands are written in lower or mixed case; the parser is evidently
  case-insensitive.
- **342 distinct first-word commands are used, and all 342 are in the 512-keyword table from
  `Game.exe`**; 170 table keywords never appear in shipped scripts. Story scenes use 325 of them.
  The most used: `label` 8,714, `findactor` 7,744, `frm_seton` 6,915, `findframe` 5,971, `wait` 5,597,
  `let` 5,240, `goto` 4,608, `act_setstate` 4,343, `if` 4,042, `enemy_wait` 1,914, `enemy_playanim`
  1,661, `enemy_talk` 1,501, `human_looktoactor` 1,149, `setevent` 1,105, `human_talk` 1,066.
  By prefix (distinct commands / calls): no prefix 88/57,331, `enemy_*` 53/9,976, `human_*` 37/5,016,
  `car_*` 24/795, `frm_*` 6/7,027, `detector_*` 5/1,647, `door_*` 5/496, `actor_*` 5/427.
- Story-flow commands: `endofmission` 328 calls in 67 missions, `mission_objectives` 307 in 58,
  `mission_objectivesclear` 315 in 60, `autosavegame` 63 in 49, `autosavegamefull` 91 in 27,
  `door_lock` 128 in 29, `subtitle_add` 15 in 13. Not used at all: `missionfailed`, `camera_play`,
  `setmissiontext`, `playmusic`.

Pitfall (fixed): the first scan for character scripts accepted the first offset that looked like a
length prefix, which cut 846 scripts to 257 bytes and left the text with leading binary bytes and
truncated first words. A valid script has no NUL bytes and starts printable, and the real length
prefix gives the longest such text. The check that caught it: every first word must be in the
`Game.exe` keyword table (first run: 105 unknown tokens; after the fix: 0).

Derived (for M5 effect capture): the story is driven by many small per-object scripts (on
average 45 per mission folder), not one mission script, and NPC behaviour (`enemy_*`, `human_*`) is
script-driven too. The effect vocabulary to capture will be a subset of these 325 story commands,
mostly the world-changing ones (`frm_seton`, `act_setstate`, `door_*`, `car_*`, `mission_objectives*`,
`endofmission`, `autosave*`, `subtitle_add`, ...). Commands that only steer AI or read state stay on
the host.

### Mafia Script keyword table in `Game.exe` (string scan, 2026-10-23)

Observed (Python regex over the test copy's `Game.exe`, read-only):
- The mission-script command keywords are stored in `Game.exe` as **upper-case ASCII strings with
  one trailing space**, NUL-padded, in one block at file offsets `0x232868`-`0x235b3c`. The pattern
  `(?<=\x00)([A-Z][A-Z0-9_]{1,40}) \x00` finds **512 distinct keywords**. The full list stays in
  `<project>\re\decomp\script-keywords.tsv`, derived from the binary and not committed.
- Keywords by prefix: ENEMY 62, ENTITY 62, HUMAN 41, CAR 27, FRM 22, VECT 15, QUAT 14, GARAGE 11,
  STREAM 10, GET 8, DETECTOR 7, FREERIDE 6, ACTOR 6, CAMERA 6, DOOR 5, POLICEMANAGER 5, WEATHER 3,
  and 100 without an underscore.
- Examples relevant to story sync: `DETECTOR_WAITFORUSE`, `ENTITY_CAR_MOVETO`, `AUTOSAVEGAME`,
  `AUTOSAVEGAMEFULL`, `SUBTITLE_ADD`, `CAR_LOCK`, `CAR_LOCK_ALL`, `DOOR_GETSTATE`, `ACTOR_DELETE`,
  `CREATE_PHYSICALOBJECT`, `WEATHER_SETPARAM`, `SETFILMMUSIC`, `CITYMUSIC_ON/OFF`, `CAMERA_SETFOV`,
  `TIMER_SETINTERVAL`, `INVENTORY_CLEAR`, `SETNPCKILLEVENT`, `FRM_SETPOS`.
- Also present: `SET_REMOTE_ACTOR`, `SET_REMOTE_FRAME` and `SET_REMOTE_FLOAT`. The meaning is
  **unknown**; the guess is variables shared between scripts, not network code.
- Other strings: a main-menu camera position named `multiplayer_pos` next to `single_pos`,
  `new_game_pos`, `free_ride_pos` and so on. Whether that was a cut multiplayer menu entry is unknown.
  Missions load from `missions\%s\...` (e.g. `missions\%s\effects.bin`).
- No lower-case `human_*` style command names and no UTF-16 copies.
  `LS3DF.dll`, `rw_data.dll` and `INet.dll` hold no such keyword table (LS3DF only has Ogg `ov_*` names).

Derived: the script parser and dispatcher live in `Game.exe`. A cross-reference to this table in
the Ghidra project should lead to the parser, and from there to the per-command handlers, which are
the effect-capture hook points for M5.

## Directory and playbook changes (made in the first draft PR, 2026-10-24)

- Added Mafia Online (MafiaHub): mod, alpha, story `no`, players_visible `yes`, world_state partial (doors per
  its Free Ride guide), open_source partial (license note), dedicated-server, last_activity 2026-09, qa none.
- Added Mafia: Oakwood (MafiaHub): abandoned (itch.io "Canceled"), open_source partial (license forbids
  derivatives), last_activity 2024-12 (game mode repo; client source 2020-08).
- Corrected LHMP: url is now the GitHub source, `open_source: 'yes'` (Apache-2.0), status `abandoned`,
  last_activity 2018-11 (last commit), plus a note that its hooks do not match the Steam exe.
- Playbook: LS3D row in `engine-families.md`; the Ghidra `-postScript` working-directory pitfall and the
  `launch.properties` settings redirect in `reverse-engineering.md`; DirectPlay enablement in `methods.md` §2.
