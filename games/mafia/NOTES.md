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

Tools (our code; since 2026-10-24 in the mod repo `mafia-coop`, `tools/re/`):
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

## M2: hook point (started 2026-10-24)

### Static findings (decompile + disassembly of the test copy; checked)

- **Version:** the Steam `Game.exe` is **1.2** by LHMP's own detector (`CGame::GetGameVersion`: 1.0 has
  `0x180` at `0x5F99FE` = GetVer 384; 1.02 has `0x18B` at `0x5BEC2E` = GetVer 395). The Steam exe has `0x18B`
  at `0x5BEC2E` (the operand of `push 0x18B`), matching `GetVer:395` in the game log. LHMP's hook
  addresses are the 1.0 ones.
- **Imports that matter:** `Game.exe` takes the registry from `ADVAPI32` (`RegOpenKeyA`, `RegCreateKeyExA`,
  `RegQueryValueExA`, `RegSetValueExA`, `RegCloseKey`) and only `timeGetTime` from `WINMM`. `LS3DF.dll` imports
  `Direct3DCreate8`, `DirectInput8Create`, `DSOUND #11`, five `WINMM` timer functions, and the window and
  cursor functions (`CreateWindowExA`, `ClipCursor`, `SetCursorPos`, `GetForegroundWindow`...).
- **Start-up** (`Game.exe` around `0x5BEC..`, decompile `Game.exe.c` line ~235880):
  `CreateMutexA("Mafia Launcher Super Mutex Shit")` exits when `GetLastError() == ERROR_ALREADY_EXISTS`,
  which is the single-instance lock. Next comes `RegOpenKeyA(HKCU, "Software\Illusion Softworks\Mafia")` and
  `RegQueryValueExA("LS3D_setup", 0x3F bytes)`, copied to the global at `0x647EE4` when the size is 0x3F.
- **`LS3D_setup` layout** (defaults from `FUN_005bf520`): `[0x00]` version `0x14`; `[0x02]` u32 width
  (default 800); `[0x06]` u32 height (600); `[0x0A]` u32 bpp (32); `[0x0E]` refresh; bytes `0x12`-`0x21` are
  options; `[0x22]` u32; `[0x26]` byte; `[0x27]`/`[0x2B]` floats 1.0; `[0x2F]`/`[0x33]`/`[0x37]`/`[0x3B]` u32 2/2/3/2.
  `FUN_005bf5a0` builds the engine flags: `0x13`→`0x4000`, `0x12`==4→`0x2000`, ==2→`0x1000`, `0x17`→`0x100`,
  `0x14`→`0x8000`, `0x16`→`0x10`, **`0x15`→`0x2`**, `0x19`→`0x4`, **`0x1F`→`0x20`**, always `|0x10090`; the window
  rect is `SetRect(50, 50, width, height)`.
- **Windowed mode is native.** `LS3DF.dll` logs either `Trying to create device with WINDOWED mode` or
  `... FULLSCREEN mode`, depending on `[0x101C52DC]`, which is the `Windowed` field of a
  `D3DPRESENT_PARAMETERS` at `0x101C52C0`. At `0x1006F27D` the engine checks `TEST AL,0x2` on its flags
  (`[0x101C55E8]`); if the bit is clear it takes the windowed path. **So `LS3D_setup[0x15] = 0` means windowed.**
  The game's own default is 0; the owner's Setup run saved 1. Ghidra does not include this code in any
  function (an unresolved jump table), so it was read with `mafia-coop/tools/ghidra_scripts/DumpRange.java`
  (read-only `-process`).
- **Suspend when inactive:** the `LS3DF` window procedure retitles to `..zzZZ` and suspends a thread only
  when `([0x101C55E8] & 0x20) != 0`, so **`LS3D_setup[0x1F] = 0` keeps background copies running**.
- **Local player (derived, strong):** LHMP (1.0) reads `[[0x6F9464] + 0xE4]` and documents
  `[0x65115C] = 0x6F9440`, so `0x6F9464` is field `+0x24` of a static object. In 1.2, `0x63788C` holds
  `0x647DF8` (a static object in `.data`), and `*(*(PTR_DAT_0063788c + 0x24) + 0xE4)` occurs 135 times in
  the decompile, often compared with a function's actor argument. +0x24 is the most used field of that
  object (514 uses). The pointer chain is therefore `[[0x63788C] + 0x24] + 0xE4` → human. Position
  `+0x24/+0x28/+0x2C`, car `+0x98`, car position `+0x40C` come from LHMP (1.0) and are **unverified on 1.2**.
- Profiles: the first-run "Enter Profile Name" prompt appears when `savegame\` has no profile (observed in
  M0). Seeding `savegame\mafia000.sav` from an earlier run is expected to skip it (to verify).

### Mod skeleton (repo `mafia-coop`, Apache-2.0; not published yet)

- `winmm.dll` proxy: all 193 exports of the 32-bit system `winmm.dll` are re-exported with the same
  ordinals (generated by `tools/gen_winmm_exports.py`). Each stub pushes its index and jumps to one dispatcher,
  which loads the real DLL on first use (never in `DllMain`) and returns into the real function with the
  caller's stack intact.
- `mod.cpp`: a build gate (`Game.exe`, image base `0x400000`, TimeDateStamp `0x72454562`, `0x18B` at
  `0x5BEC2E`). If it passes, Game.exe's IAT is patched (memory only, in `DllMain`):
  - the 5 registry functions keep `LS3D_setup` in `<game dir>\coop\ls3d_setup.bin` behind a fake HKEY, with
    width and height from `coop.ini`, `[0x15]=0` and `[0x1F]=0` forced on read, so the owner's HKCU is
    never touched;
  - `CreateMutexA` renames the instance mutex to `..._<pid>`.

  A logger thread writes the local player's position once a second to `coop\coop.log`.
- Build: `scripts\build.cmd` (MSVC 14.51 x86 from VS Build Tools 2026, `/W4 /WX /MT`). The DLL imports
  only KERNEL32 and USER32, and its export set is identical to the system DLL's (checked).
- Test: `scripts\test.cmd` builds `tests\proxy_test.cpp` (`/RTCs`), loads the proxy by full path and calls
  `timeGetTime`, `timeGetDevCaps`, `timeBeginPeriod`/`timeEndPeriod` and `waveOutGetNumDevs` through it
  from 4 threads × 2,000 iterations: **PROXY_TEST PASS (0 failures)**. Mutation twin: dispatching to
  index+1 aborts the test with exit code 3. In a non-game process the log shows `build gate FAILED`
  and nothing is patched (checked).
- Test instance: `scripts\make_instance.py` builds `lab\inst1` with 9,509 hard links to `lab\base`, a
  real copy of `winmm.dll`, `coop\coop.ini` (1024x768), `coop\ls3d_setup.bin` (seeded from the owner's M0 Setup
  bytes) and `savegame\mafia000.sav`. The base manifest was unchanged afterwards.

### First launch with the mod (2026-10-24 01:52, owner watching; `lab\inst1`)

Run with `mafia-coop\scripts\launch_instance.ps1` (300 s; game-window snapshots every 10 s). Owner
approved. Before: guard PASS, `HKCU\Software` and DirectInput exported.

Observed:
- `coop.log`: `build gate passed`, then `mutex: ... renamed to 'Mafia Launcher Super Mutex Shit_45180'`,
  `registry: RegOpenKeyA(Software\Illusion Softworks\Mafia) redirected`,
  `setup: from ls3d_setup.bin, forced 1024x768 windowed, suspend off`.
- **Windowed mode works.** The game log says
  `Trying to create device with WINDOWED mode: 1024x768, 165Hz, BackFormat: D3DFMT_X8R8G8B8, DepthFormat: D3DFMT_D24X8`;
  the window (class `LS3D`, title `Mafia`) has a 1024x768 client area at `47,24`-`1077,821` with a frame.
- **Registry redirect works.** `HKCU\Software\Illusion Softworks` never appeared; the guard passed with
  no game-key cleanup.
- **The profile prompt was not skipped.** With a seeded `savegame\mafia000.sav`, the game shows a
  "Player Profile: Default / Create New Profile / Delete Profile / Quit Game" chooser instead of the name
  prompt, and the owner picked Default. The game does not remember the last profile across starts,
  so skipping it needs an in-process selection. Mafia Online does this too (`quickJoin.profileIndex`).
- After New Game the intro movies and the in-engine intro played. `player: found (game 0B1D9DA0, human
  0B2861E8)` with `pos -1040.46 -8.18 96.87` at 01:55:39; `player: none` for 4 s; then the same game pointer
  with `human 1337C560` and `pos 238.61 -0.99 181.94`. There were no further changes before the 300 s timeout
  (a cutscene, so no player control). **Partly verified:** the pointer chain resolves, the game pointer
  stays stable across a scene change, the human changes with the scene, and the values look like world
  coordinates (y is the small ground value). Tracking of player-controlled movement is **not verified yet**.
- Writes: the game created `log.txt`, `savegame\mafia000.000` (439 B) and grew `savegame\mafia000.sav` to
  1,076 B, all private files in the instance. **The base copy stayed byte-identical** (full SHA-256 of
  9,509 files), so no hard-linked file was written. DirectInput again created `GAME.EXE724545620023F000` and
  overwrote `MostRecentApplication` (Windows' DirectInput runtime). Windows also added shell bookkeeping
  entries (MuiCache, audio PolicyConfig).

Cleanup: removed the DirectInput `GAME.EXE…` key and restored `MostRecentApplication` (the DirectInput tree
is identical to the pre-test export). Moved `log.txt`, `coop.log` and the savegame files into
`local/backups/m2-launch1/inst1-files/` and re-seeded the 108-byte profile, so `inst1` is back to its prepared
state. Guard: PASS. Windows bookkeeping entries were left alone.

Next in M2: select the profile in-process; verify position tracking while the player walks and drives
(ideally with a scripted bot, not the owner); add a per-frame hook (e.g. `IDirect3DDevice8::Present`);
run two instances side by side (the mutex rename is in place).

### Profile chooser and two instances (2026-10-24, prepared, not run yet)

Static findings (checked in the decompile and the raw exe):
- The start-up profile screen is `FUN_00604ee0`, called once at `0x5BFE1D` with `ECX = 0x6D4628`, the static
  profile manager. Its name string `profileselect` is referenced only at `0x605011`. It enumerates
  `savegame\mafia%03d.sav` (0x54-byte records with magic `0x50726F66` "Prof"), loads scene `00menu`, creates a
  menu object with vtable `0x6279DC`, runs the generic menu loop `FUN_005eba40`, and then, unless the result
  is `0x0F` (Quit), calls `FUN_006057f0`. That loads profile number `[manager + 0x28]` (= `0x6D4650`).
- `FUN_005eba40` renders while `DAT_006BD8A8 == 0`; the current menu object is `DAT_006BD890`. On a
  non-zero `DAT_006BD8A8` it breaks, runs `FUN_005ea690` (cleanup), pops the menu state and returns the
  value. Menus close themselves through `FUN_005eae40(.., code)`, which sets `DAT_006BD8A8 = code`.
- The mod's start-up thread does the same: when `[DAT_006BD890]` is the `0x6279DC` vtable and the result
  is still 0, it writes the configured profile number to `0x6D4650`, then 1 to `0x6BD8A8`. It also moves the
  `LS3D` window to `coop.ini [window] x,y` (async `SetWindowPos`, re-applied until it holds for 3 checks) and
  sets the title to `Mafia - <name>`. It does nothing if the profile file is missing.
- No other single-instance check found: neither `Game.exe` nor `LS3DF.dll` imports `FindWindowA`.

Prepared: `lab\inst1` (x=40) and `lab\inst2` (x=1000), each 800x600 at y=120, with names in the titles, and
the same seeded profile and setup. `scripts\launch_instance.ps1` now takes `-GameDirs "a;b"` and
starts the copies one after another (the next once the previous has its game window); snapshots go
per instance. The DLL rebuilt cleanly; proxy test PASS.

### Two instances side by side (2026-10-24 12:26, owner watching; 300 s) — checked

- Both copies ran at once. Each `coop.log` shows its own mutex (`..._45620`, `..._8344`), the registry
  redirect, `forced 800x600 windowed, suspend off`, `window: moved from 47,24 to 40,120` and `... to 1000,120`.
  The launcher saw `title="Mafia - inst1" rect=40,120,846,749` and `title="Mafia - inst2" rect=1000,120,1806,749`.
  Both game logs say `Trying to create device with WINDOWED mode: 800x600`.
- **Profile auto-select works:** `profile: chooser open (menu 0B319EC8), selected profile 0` in inst2 at
  +9 s and in inst1 at +41 s (after its intro movies). Neither window showed the chooser to the player,
  and the snapshots at 285 s show the **Main Menu in both** copies.
- **The background copy keeps rendering:** the last 6 snapshots of each copy are all different frames
  (the menu camera moves), so suspend-when-inactive is really off.
- No game was started, so no position lines were logged.
- After the run: no `HKCU\Software\Illusion Softworks`; guard PASS; **base copy byte-identical** (full
  SHA-256 of 9,509 files); the instances only gained `log.txt` and `coop.log` (profiles unchanged).
  DirectInput again created `GAME.EXE724545620023F000` and overwrote `MostRecentApplication`: restored (tree
  identical to the pre-test export). The logs were moved to `local/backups/m2-launch2/`.

Main menu results (decompile of the switch after `FUN_005eba40` in the main loop, checked): `0x11`
tutorial, `0x12` new game (intro, then mise01), `0x13` load, `0x15` an object of size 0x688 (probably
Carcyclopedia), `0x16` intro only, `0x17` `FUN_0060e710`, `0x18` quit (then the settings are saved with
`RegCreateKeyExA(HKLM, ...)`, which the hook also redirects), `0x19` autosalon, `0x1B` extreme, `0x1C` freeitaly,
**`0x1D` freeride**, `0x1E` freeridenoc, `0x1F` freekrajina, `0x20` freekrajinanoc. The main menu object's vtable
is `0x625EB8`. The mod's `[autostart] mission=` ends the main menu loop with that result once, after the
profile step (mafia-coop, not run yet).

### Free Ride autostart and position check (2026-10-24 18:29, owner playing; 300 s)

Observed:
- **Autostart works in both copies:** `profile: chooser open ... selected profile 0`, then
  `autostart: main menu open (menu 0B278EA0), starting freeride (result 0x1D)`, then `player: found` 1-2 s later
  (inst1 at +42 s after its intro movies, inst2 at +10 s). Both loaded Free Ride (city, day) **in a car**.
- **The logged "position" was wrong for this purpose:** human `+0x24` jumped from `(1,0,0)` to
  `(-0.22,-0.13,-0.36)` and stayed there while the car drove, which is the **seat offset relative to the car**.
  So `+0x24` is a position relative to the parent frame. (In the M2 first launch, `-1040.46 -8.18 96.87`
  were on-foot values, where the parent is the world.)
- **LHMP's car fields do not hold on 1.2:** human `+0x98` first read `001B00BD` (not a pointer); car `+0x40C`
  gave values with z only 0..1, and they changed in a pattern unlike a world position. Not used any more.
- Snapshots: both copies show the player's car on city streets; inst2's car was in a different place at
  61 s and 121 s with 98 health. inst2 was started second and so had keyboard focus first; inst2's values
  moved in 20-120 s and inst1's in 80-200 s, so the owner presumably drove inst2 first, then inst1.
  How the game sets up DirectInput (foreground/background) is **unknown**.
- After the run: no `Illusion Softworks` key, guard PASS, **base byte-identical**. The instances gained
  `log.txt`, `coop.log`, an updated profile and `savegame\mafia000.570` (~142 KB, Free Ride). DirectInput
  keys restored, files moved to `local/backups/m2-launch3/`, profiles re-seeded.

Engine layout (checked in `LS3DF.dll` `I3D_frame::UpdateWMatrixProc`): the world matrix is at frame `+0x10`
(4x4, row-major), computed as the local matrix (`+0x50`) times the parent's world matrix (parent at `+0x120`);
scale at `+0x90`, rotation quaternion `+0x9C`, flags `+0xAC` (`0x20` = world matrix up to date). **World
position = frame `+0x40/+0x44/+0x48`.** LHMP's `OBJECT` struct puts the actor's frame at `+0x68`, so the mod
now logs `world` from `[human + 0x68] + 0x40` next to `local` (`human + 0x24`). This should give world
coordinates both on foot and in a car (the human's frame hangs under the car). Built and proxy-tested,
**not run yet**.

### World position test (2026-10-24 18:40, owner planned to drive and walk in inst1; 300 s)

Observed:
- Start-up was identical in both copies (profile, autostart `0x1D`, `player: found (... frame 18EAED48)`).
- **World position reads plausible world coordinates:** `world -1984.88 -5.03 23.14` at spawn (the same values
  `+0x24` showed on foot, before the player was seated), then `-1984.65 -5.03 23.52` once seated, while
  `local` became the seat offset `-0.22 -0.13 -0.36`.
- The log then stopped changing in **both** copies 2-3 s after spawn. The snapshots agree: the car sits on
  the same parking-lot spot in inst1 at 101, 151, 201 and 291 s, and in inst2 at 101 and 201 s, while
  traffic and pedestrians move. **So nothing moved, and the logger was right.** World tracking during
  movement is still **not verified**.
- Correction (owner, afterwards): they had driven in the run before, not in this one. So nothing was
  wrong with input. My focus hypothesis (a copy that loses focus never gets input back) was wrong; the
  next run refutes it.
- After the run: no `Illusion Softworks` key, guard PASS, base byte-identical; DirectInput restored; logs
  and saves moved to `local/backups/m2-launch4/`; profiles re-seeded.

### World position verified while driving (2026-10-24 18:56, owner drove in both copies; 300 s)

The logger also writes an `alive` line every 5 s, so a stalled reader would show. Observed:
- Heartbeats all run long: inst1 43, inst2 51. World-change lines: inst1 129, inst2 108. One human pointer per
  copy the whole time.
- **The world position follows the car.** inst1: path 176 m, 111 m from spawn at the end, top speed 41 km/h,
  movement 20-60 s after spawn. inst2: path 184 m, 120 m from spawn, 46 km/h. Height `y` stays -6..-3.75
  while `x` and `z` change by 50-115 m, so **Y is up** and x/z are the ground plane. The snapshot of inst1 at
  110 s shows the car on a street under the elevated railway, away from the spawn parking lot, as the
  log says.
- **Both copies took the owner's input** after he clicked into each window, so a copy that lost focus
  at start-up still gets keyboard input back, with suspend-when-inactive off.
- On foot, world equals local (difference 0.02 m, one sample at spawn). A longer walk is not logged yet.
  The owner stayed in the car.
- After the run: no `Illusion Softworks` key, guard PASS, base byte-identical; DirectInput restored; files
  moved to `local/backups/m2-launch5/`; profiles re-seeded.

M2 status: hook point, windowed mode, isolated settings, two copies and profile skip are **done**.
Position tracking in a car is **done** (see the run above); Free Ride autostart works. Still open: a longer
on-foot check (to be scripted, not left to the owner) and a per-frame hook.

### Frame hook and walk bot (2026-10-24, built and offline-tested, not run in the game yet)

- `LS3DF.dll` calls `Direct3DCreate8` and `DirectInput8Create` through `jmp [IAT]` thunks (`0x1007D9D0`
  → slot `0x1009B254`; `0x10080364` → slot `0x1009B000`; no `call [slot]` sites), so patching its IAT slots
  catches them. (Neither call appears in the decompile: Ghidra left that code outside functions.)
- `mafia-coop/src/frame.cpp`: wraps `IDirect3D8::CreateDevice` (vtable 15) to replace
  `IDirect3DDevice8::Present` (vtable 15), giving a per-frame tick on the game thread; wraps
  `IDirectInput8A::CreateDevice` (3) to replace the keyboard's `GetDeviceState` (9) and `GetDeviceData` (10).
  DX8 headers are not in the Windows SDK, so the slots are numbered by hand.
- Bot (`coop.ini [bot] steps`): `wait ms`, `tap KEY`, `hold KEY ms`, and `exitcar`, which tries F, then
  RETURN, then E until the player is no longer seated (4 s each). It runs from `Present`, ORs its keys into
  the keyboard state, and appends buffered events. No OS input is sent, and it also works in a window
  without focus (a failed `GetDeviceState` is answered with the bot's keys while the bot is active).
- `tools/check_bot_walk.py <coop.log>` passes only if `bot: exitcar ok` appears, every sample while W is held
  is on foot (world == local within 0.05 m), and the player moved at least 2 m. Checked offline: FAIL on
  the human-driven log of the previous run (no bot), PASS on a synthetic log.
- Both instances now have `steps=wait 2000; exitcar; wait 2000; hold W 6000; wait 2000; hold W 4000`.

### First bot run (2026-10-25 03:19, owner hands off; 300 s) — checker FAIL in both copies

Observed:
- **Frame hook works:** `frame: Direct3DCreate8(220) -> ...`, `frame: device ... created, Present hooked`, and the
  frame counter rises. Menus run uncapped (about 262,000 Present calls in the first ~43 s in inst1), while
  in-game heartbeats show only **10-15 frames/s for both background windows**. A frame cap for the menus,
  and a look at the background throttle, are follow-ups.
- **Input hooks work:** `input: keyboard ... hooked` (plus an unhooked `{6F1D2B60}` mouse). The engine
  reads the keyboard only with `GetDeviceState`, about once per frame; `GetDeviceData` calls: **0**.
  The engine's routine at `0x100717F0` (raw disassembly, outside Ghidra's functions) copies the
  previous state `0x101C5300` → `0x101C5488`, clears it, and calls `GetDeviceState(256, 0x101C5300)`
  on `[0x101C5434]`. On failure it calls `Acquire` (slot 7) in a loop while
  `0x8007001E` (DIERR_OTHERAPPHASPRIO) and finally zeroes the buffer. After success it checks only
  debug hotkeys (LShift+LAlt+LCtrl with F12/F11, toggling `0x101C59CC`/`0x101C59CD`).
- **The bot's keys had no visible effect:** F, RETURN and E (each tapped for 150 ms, then 4 s of
  checking) left the player seated, and holding W for 6 s and then 4 s did not move the car (world
  position unchanged to 0.01 m in both copies). `tools/check_bot_walk.py` reports
  `FAIL (no "bot: exitcar ok" line)` in both, so the checker caught the failure.
- `IJoy.dll` exports only `_CreateJoyBase@20` and handles joysticks (DirectInput 7), so it is not the
  keyboard path.
- The manual's text streams hold no controls table (probably an image); the default key bindings are
  **unknown**.
- Open question with two candidate causes: (a) the bindings are not W/F/RETURN/E; (b) the game ignores
  input while its window is not active. Both windows were in the background, and in the owner's
  driving run each window had focus while he drove. Next experiment: one instance with focus and the bot,
  plus the owner's key bindings.
- After the run: no `Illusion Softworks` key, guard PASS, base byte-identical; DirectInput restored; files
  moved to `local/backups/m2-bot1/`; profiles re-seeded.

### Second bot run, arrow keys (2026-10-25 10:04, owner hands off; 300 s) — checker FAIL, input injection works

The owner drives with the arrow keys and leaves the car with E or F (unsure which). Bot: `hold UP 4000`,
then `exitcar` (F, RETURN, E, now held 400 ms each), then two `hold UP`.
- **Injected keys work in a window without focus:** the first `hold UP` (4 s) moved the car **18.55 m**
  (inst1) and **7.06 m** (inst2). The first bot run failed to move only because W is not bound. Both
  windows were in the background the whole time.
- **Getting out still failed:** `exitcar FAILED, still seated after F, RETURN and E` in both copies, so the
  checker reports FAIL. inst2's car then moved another 26.94 m and 10.55 m on the later UP holds, with the
  player still seated; inst1's car stayed put (probably blocked after the first drive).
- No default key-binding table was found in `Game.exe` `.data`/`.rdata` (scanned for the four arrow DIK
  codes within 12 entries, as 1-, 2- and 4-byte values). The bindings are expected in the profile:
  `FUN_006057f0` loads 0x2D0 bytes into `manager + 0x1F4` (plus 0x5C at `+0x4C4` and 0x9C at `+0x520`).
- After the run: no `Illusion Softworks` key, guard PASS, base byte-identical; DirectInput restored; files
  moved to `local/backups/m2-bot2/`; profiles re-seeded.

Built but not run: at bot start the mod logs those three profile blocks as hex, and `exitcar` can try a
longer key list (`[bot] exit_keys`, `exit_check_ms`).

**Owner (2026-10-25): "in FREE RIDE you cannot get out of the car."** This fits both bot runs, where F,
RETURN and E did nothing while the player sat in a parked car. The exit-key search was dropped. The free
ride start is set up by engine code (`FUN_0060e9d0`/`FUN_0060eef0` after the `0x1D` result); the
`freeride` scripts hold no car-entry commands, only `freeride_score*` and `freeride_traffsetup`.

Next on-foot test: `[autostart] mission=tutorial`, main menu result `0x11` (`missions\tutorial`, a construction
site with a dog, workers and enemies; its scripts toggle `player_lockcontrols`). Bot steps `wait 3000; hold UP
6000; wait 1000; hold UP 4000`; checked with `tools/check_bot_walk.py --start-on-foot` (offline: PASS on a
synthetic on-foot log, FAIL on the Free Ride bot log because the player is seated). Built, proxy test
PASS, both instances switched; not run yet.

### Tutorial walk bot (2026-10-25 10:27, owner hands off; 300 s) — **checker PASS in both copies**

(The first attempt was held: the guard FAILED because the owner had started Mafia from Steam at 10:16, which
created `<game dir>\Mafia\log.txt` in the real install. The owner confirmed this, and the guard was re-baselined
with the old baseline kept. None of our scripts start the Steam exe.)

- Both copies: `autostart: main menu open ..., starting tutorial (result 0x11)`, then `player: found` within
  0.6 s, then the bot started 3 s later. Both windows stayed in the background.
- **`tools/check_bot_walk.py --start-on-foot`:**
  `PASS (... on foot in all 17 samples while UP held 6.0 s; moved 10.94 m)` (inst1) and
  `PASS (... 15 samples ...; moved 10.49 m)` (inst2). On foot, world equals local to 0.01 m, and the speed is
  about 4.8 m/s (1.25 m per 0.26 s sample).
- **Both copies behave identically:** same spawn `30.01 20.16 -37.80`, same path, and both stop at
  `19.18 20.16 -40.56` after about 2.5 s (an obstacle, presumably); the second `hold UP` (4 s) does not move
  them. The tutorial ground is at y = 20.16.
- The first sample after spawn in inst2 read a garbage local position (`-1.66e35`) while world was
  already valid. The actor is not fully built at that moment, so sync code must not trust the first frame.
- Present-based frame counts during the walk: about 16-17 frames per 0.26 s (~62 frames/s) in both background
  windows.
- **Key-binding table (derived, partial):** the profile manager block `+0x1F4` (0x2D0 bytes) holds 12-byte slots
  `00 00 00 00 | 01 00 <DIK> 00 | 00 00 00 00` (device 1 = keyboard), two slots per action (24 bytes). Action 0 =
  `UP`/`W`, action 1 = `DOWN`/`S`, action 2 = `LEFT`/..., action 3 = `RIGHT`/..., which are the on-foot moves. A
  second group around byte 402 binds `UP`, `DOWN`, `LEFT`, `RIGHT` again without the letter keys,
  presumably the car controls; that would explain why W did nothing in the car. F is in action 6's second slot,
  and E in action 8's second slot and action 27's first. Which actions these are is **unknown**.
- After the run: no `Illusion Softworks` key, guard PASS (new baseline), base byte-identical; DirectInput restored;
  files moved to `local/backups/m2-bot3/`; profiles re-seeded.

**M2 exit test met, automated:** two copies run side by side, and each logs its player position from a
per-frame hook and a logger thread. A scripted bot drives the input with no owner involvement, and a
checker decides PASS/FAIL (verified both ways). Leftovers for later milestones: a frame cap for menus
(uncapped Present rate in menus), decoding the binding table's action ids, and treating Free Ride as
car-only.

## Directory and playbook changes (made in the first draft PR, 2026-10-24)

- Added Mafia Online (MafiaHub): mod, alpha, story `no`, players_visible `yes`, world_state partial (doors per
  its Free Ride guide), open_source partial (license note), dedicated-server, last_activity 2026-09, qa none.
- Added Mafia: Oakwood (MafiaHub): abandoned (itch.io "Canceled"), open_source partial (license forbids
  derivatives), last_activity 2024-12 (game mode repo; client source 2020-08).
- Corrected LHMP: url is now the GitHub source, `open_source: 'yes'` (Apache-2.0), status `abandoned`,
  last_activity 2018-11 (last commit), plus a note that its hooks do not match the Steam exe.
- Playbook: LS3D row in `engine-families.md`; the Ghidra `-postScript` working-directory pitfall and the
  `launch.properties` settings redirect in `reverse-engineering.md`; DirectPlay enablement in `methods.md` §2.
