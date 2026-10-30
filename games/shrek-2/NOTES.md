# Shrek 2 (PC, 2004): triage notes

Working notes for the `game/shrek-2` branch. Phase 0 (triage) only; nothing built yet.
`<game dir>` = the install folder, never edited. All checks below were read-only.

## Identity (rule 5)

| | |
|---|---|
| Edition | Russian disc edition (4 CD, InstallShield), published in Russia by 1C per the owner; "Activision" in the registry |
| Store | disc (not Steam, not GOG) |
| Game version | `1.00` (uninstall registry entry), language id `1049` (Russian) |
| Main exe | `System\Game.exe`, 274,432 bytes, SHA-256 `4900AA131CF35D4951943F2D6EE831841745AF8E5D657FFDCFD7FB012CBAA2EF`, PE timestamp 2004-04-14 |
| Engine DLL | `System\Engine.dll`, 4,980,736 bytes, SHA-256 `79EB368BA102740644EB6537E1CABFE7BA58DF7BBA47DC4045653CD30A2385E3`, PE timestamp 2004-12-13 |
| Game package | `SHGame.u` 3,333,691 bytes; `KWGame.u` 1,656,207 bytes; files dated 2005-03-18 |
| Engine | Unreal Engine 2 (see below) |
| OS | Windows 11 Pro, install under `C:\Program Files (x86)\Activision\Shrek 2\` |
| Size | 463 files, 658,499,322 bytes |

The English and Russian editions may differ (Russian text sits in `*.Rus` next to `*.int`).
Treat any write-up as matching only after comparing the hashes above.

## Observations (things I saw in the install)

- 32-bit x86 binaries (machine 0x14c). `Game.exe` and `Engine.dll` carry PDB paths
  `c:\Projects\Shrek 2\Game\Engine\Lib\Release\Engine.pdb`.
- Unreal-style layout: `System\Core.dll`, `Engine.dll`, `Fire.dll`, `IpDrv.dll`, `Window.dll`,
  `WinDrv.dll`, `D3DDrv.dll`, `ALAudio.dll`, `PixoDrv.dll` (Pixomatic software renderer),
  `KWGame.dll`. UnrealScript packages `.u`, maps `.unr`, textures `.utx`, meshes `.usx`/`.ukx`,
  audio `.uax`/`.ogg`, video `.bik`.
- `Default.ini`: `DefaultGame=ShGame.ShGame`; `[Engine.GameEngine]` has `ServerPackages=` lines for
  `AmbientCreatures`, `GamePlay`, `KWGame`, `ShGame`; net rates `NetServerMaxTickRate`,
  `LanServerMaxTickRate`; `IpDrv.dll` is present. So the engine's network code is shipped.
- `Packages.md5` is present (package integrity list).
- No UnrealScript source (`.uc`) in the install; only compiled `.u`.
- Maps (29): the 11 story levels (`1_Shreks_Swamp` to `10_Castle_Siege`, `11_FGM_Battle`),
  `Book_*` storybook maps, `Beanstalk_bonus*`, `Entry`, `Credits`, `SH2_Preamble`.
- `HpMenu.int` has no multiplayer, co-op, split-screen or join strings. `HpDialog.int` hits for
  "join" are only dialogue ("joints").
- `readme.txt` (Russian, cp1251) lists 4 CD, single install; I did not find a multiplayer
  mention in the part I read (first 120 lines).

## Derived (from the observations)

- The game is built on Unreal Engine 2 (UnrealScript `.u`, `Core/Engine/Fire/IpDrv` split, `Entry.unr`).
  The exact UE2 revision is unknown.
- The engine has client/server replication code, but I have not seen a multiplayer game mode or
  menu in this build. Co-op would mean making the single-player story run on the engine's netcode.
- Gameplay is in `SHGame.u` and `KWGame.u`; decompiling them is possible with existing UE2 tools.

## Existing work (research.md, first pass)

Only a first pass by web search; GitHub code search, ModDB, Archive.org, Discord not done yet.

| Find | What it is | Co-op? | Source |
|---|---|---|---|
| Master_64 mods and tools (itch.io) | Hiver mod loader, DevKit with UE2 editor patch, MPak code library, level editor, Chaos 2, MasterToolz, Project: SMR | not mentioned; DevKit is marked obsolete, last devlog 2023-02-18 | https://master-64.itch.io , https://master-64.itch.io/shrek-2-pc-devkit |
| Shrek 2.2 | full-game mod, alpha, 186 commits on main; lists "Online P2P Multiplayer" as a **future** feature | no, planned only | https://github.com/Master-64/Shrek-2.2 |
| Shrek 2 Modding SDK and Mod Manager | loads `.dll`/`.zip` mods | not mentioned | https://kevinjpetersen.itch.io/shrek-2-mod-manager |
| Shrek 2 Remastered (Aspyr) | official remaster, announced for 2026-11-10, up to 4 players couch and online | official, **not** the 2004 game | https://gematsu.com/2026/10/dreamworks-shrek-2-remastered-announced-for-ps5-xbox-series-switch-2-switch-and-pc |

Not found by search: a co-op or multiplayer mod for the 2004 PC game. Say "not found by search",
not "does not exist".

Conflicting claims, unresolved:
- Activision press text and Wikipedia mention multiplayer for the game; GamePressure tags the PC
  version as single-player only; Co-op.gg lists PC local co-op (4 players). The page behind the
  last two blocked automated fetches (HTTP 403). Console versions are a different codebase.
  Resolve by playing/inspecting the owner's copy, not by trusting these.

## Package analysis (2026-10-30, on the copy under `local/shrek2-test`, read-only)

Method: small Python parser for the Unreal package header, name/import/export tables (scratch
script, not committed).

Observations:
- All `.u` packages: tag `0x9E2A83C1`, **package version 129, licensee 0** (UT2004-era UE2.5
  numbering; the exact engine build is still unknown).
- `Engine.u` is the stock Epic set: `GameInfo`, `PlayerController`, `PlayerReplicationInfo`,
  `GameReplicationInfo`, `SavedMove`, `AccessControl`, `Mutator`, `HUD`, `PlayerInput`, 233 classes.
- `KWGame.u` (286 classes) is the Beenox/Luxoflux framework: `KWGame`, `KWHeroController`, `KWPawn`,
  `KWCutScene`, `KWScriptedSequence`, `CompanionCorral`, `CompanionCombatManager`, `SplineManager`.
- `SHGame.u` (994 classes) is the Shrek layer: `Shrek`, `ShHeroPawn`, `ShHeroController`,
  `ShrekController`, `SHCharacter`, `SHHud`, `SHInterestManager`, `SavePointFairy`, enemies.
- **Script source is stripped.** Every `ScriptText` TextBuffer in `Engine.u` (233 of 281),
  `SHGame.u` (994 of 994), `KWGame.u` (286 of 288), `GamePlay.u` and `UWindow.u` is a 12-byte empty
  object. Only `Core.u` and a few `GUI.u` classes keep text. Compiled bytecode is present
  (2,838 functions in `SHGame.u`, 1,898 in `KWGame.u`).
- Unlike Deus Ex (UE1, [methods.md](../../playbook/methods.md)), dumping source text from the
  packages does not work here.

Derived:
- To read the game's logic we need a bytecode decompiler for UE2.5 packages (for example UE Explorer
  / UELib), or our own. Neither is installed yet.
- Co-op can be written as a **new UnrealScript package** that subclasses `ShGame`/`KWGame`
  classes and is added to `ServerPackages`, as HX does for Deus Ex. That needs the UnrealScript
  compiler (`ucc make`), which is not shipped (no `ucc.exe`); Master_64's DevKit/editor patch is
  the known way to compile for this game. Not tested.

## Decompiling the bytecode (2026-10-30)

Tool: UE Explorer 1.6.2 portable (GPL-3.0, official release
https://github.com/UE-Explorer/UE-Explorer/releases/tag/1.6.2), archive
`ue-explorer-1.6.2-net48-portable.7z`, 1,892,704 bytes, SHA-256
`085267f3192acb27c2a6a98367b2e659a202a80adda79d8562cddd3e7e8e53d9` (matches the digest GitHub
lists for the asset). Extracted to `local/tools/` (ignored). I did **not** start the UE Explorer
window (it has an auto-updater); I loaded `Eliot.UELib.dll` (UELib) from PowerShell and called
`UnrealLoader.LoadFullPackage` + `UClass.Decompile()` on the **copy** only. Output goes to
`local/shrek2-src/` (ignored; decompiled game code stays out of the hub).

Result: `Engine` 277 classes, `KWGame` 426, `SHGame` 1,146 files, `GamePlay` 275, all 0 failures
(about 150k lines). UELib reads the packages as build `Default` / `DefaultEngineBranch`.

Quality problems seen:
- Native function calls come out as `__NFUN_130__(...)`, not names, because the native table for
  this game is not loaded. Next step: load a UE2.5 native table (the `Native Tables` folder ships
  with UE Explorer) and re-run.
- Some array properties fail with "PropertyTag value size error" (default-property blocks of
  `SVehicle`, `KWGame.MessageBox`, `OptionsPage`, `MainMenuPage`, `FONT_KW_Button`). Those are
  default values, not code.

What the source says (observed):
- `KWGame.KWGame.Login()` does not spawn a pawn: it looks for an existing `KWPawn` with
  `bIsMainPlayer` already placed in the map and possesses it. A second client logging in would
  find no pawn. `PostLogin` sets the HUD and `StartLocation` from `NewPlayer.Pawn`.
- `ProcessServerTravel` appends `?GameState=` to the next URL: level changes go through the
  engine's server-travel path, and a game-flow state string rides in the URL.
- `Default.ini`: `DefaultGame=ShGame.ShGame`; `SHGame` uses `ShGame.ShrekPawn` and
  `ShGame.ShrekController`; `KWGame` uses `KWGame.KWHeroPawn` / `KWHeroController`.
- `Engine.u` keeps the stock replication blocks (`Actor`, `Controller`, `Inventory`, ...).
  **`KWGame`, `SHGame` and `GamePlay` have no `replication` block at all**, so none of the Shrek
  or Kuju gameplay classes replicates anything today.
- Only three places check `Level.NetMode`: `KWHeroController.ServerUTrace` (debug),
  `ShHeroPawn` (line ~5583, main-player check) and `SHMenuBook` (`RemoteRole=0`). The game
  was written for standalone only.

Derived:
- The engine's netcode is intact but the game never used it. Co-op needs (a) our own `GameInfo`
  subclass whose `Login` spawns a second hero pawn, (b) `replication` blocks on the hero pawn,
  controller and the actors that matter, (c) a way to compile UnrealScript (no `ucc.exe`; try the
  Master_64 DevKit, licence terms unread), or patching bytecode in the packages directly.

### Native names and compile attempts (2026-10-30)

- Loading UE Explorer's `NativesTableList_UT2004.NTL` into UELib (set `UnrealPackage.NTLPackage`
  before `InitializePackage`) turns `__NFUN_130__` into real names (`AllActors`, `Log`, `Spawn`).
  Regenerated all four packages that way; 68 of 2,124 files still hold a few unknown
  game-specific native numbers (for example 603, 2000, 265).
- `Editor.dll` in the install contains `UMakeCommandlet` and `MakeScript`, but the install has no
  `ucc.exe` or `UnrealEd`. `Game.exe` has no commandlet code (no "commandlet" string).
- Sandbox test (`local/shrek2-make`: copy of `System`, junctions to the data folders, a probe class
  in `ShrekCoop\Classes`, `EditPackages=ShrekCoop`): `Game.exe make` and a copy renamed `ucc.exe`
  both just ran as the game (no log, still running after 60 s, killed). **Result: this install
  cannot compile UnrealScript by itself.**
- Master_64's tooling is the known route: the "Level Editor [v8.0]" (841 MB on itch.io, built on
  the free UE2 Runtime level editor, "lets you compile your own modifications via UnrealScript")
  and the DevKit + Editor Patch (439 MB). The Hiver mod loader says it supports UnrealScript mods.
  None downloaded yet. Licences: no named licence, credit the author, ask before publishing.

## Level Editor, compiling and the first mod package loaded (2026-10-30)

Owner's direction: the mod will be a broad "improve the game" mod, with co-op as its foundation.
Plan the package as a base co-op layer plus separate feature modules.

Tool: Master_64 "Shrek 2 (PC) Level Editor [v8.0]" from https://master-64.itch.io/shrek-2-pc-level-editor
(free download, no named licence, credit the author and ask before publishing). Archive
`Shrek2-LevelEditor.zip`, 882,093,507 bytes, SHA-256
`703445c43a4e6ab4cc81338e941bfec66a9803a97bcb725cf1a095fe71473835` (computed by me; the page lists
no hash). Extracted to `local/tools/level-editor/` (ignored). Stock "UE2 Runtime 2226.19.03" plus
`UCC.exe`, `UnrealEd.exe`, and **UnrealScript source for the game's own packages**:
`KWGame` 289 classes, `SHGame` 994, `UWindow`, `AmbientCreatures`, `UDebugMenu`, `CBBP`,
`HackEngine`, `HackGUI`. (Same class counts as the stripped packages in the install, so it is a
rebuilt copy of the shipped logic.) Its DLLs and `.u` files are **not** the game's (hashes differ).

Observed:
- `UCC.exe make ini=UE2Runtime.ini` rebuilds `KWGame` and `SHGame` from source with
  `Success - 0 error(s), 1076 warning(s)`. Needs: delete the old `SHGame.u` first (otherwise
  "Error saving 'SHGame.u'"), and drop `EditPackages=MPak` (source not in the archive).
- A new package `ShrekCoop` (`CoopGame extends ShGame.ShGame`, overrides `PostBeginPlay` with one
  `Log`) compiled to a 963-byte `ShrekCoop.u`.
- Copied into a sandbox of the **original** game and started with
  `Game.exe 1_Shreks_Swamp?Game=ShrekCoop.CoopGame -log`, the game log shows
  `Game class is 'CoopGame'` and `ShrekCoop: CoopGame is running, NetMode=0`. So **a package built
  with the editor's compiler loads and runs in the shipped game**, subclassing the game's own
  `ShGame`.
- A/B on the intro cutscene, same 50 s: baseline (no `Game=`) reached cutscene step 60, mod run
  reached step 70, both alive and responding. The first mod run looked frozen to the owner but its
  log was cut off by my kill; the repeat run shows no difference from the baseline.
- `Game.exe` has no working `ini=`/`userini=` option (crashes at start, exit `0xC0000005`).

Pitfall found (must not repeat):
- The game keeps its settings, log and saves in `%USERPROFILE%\Documents\Shrek 2\` (`Game.ini`,
  `User.ini`, `Game.log`, `Save\`) whatever folder it is started from. My first sandbox runs used a
  `Default.ini` I had edited (`EditPackages` replaced), and that was merged into the owner's
  `Game.ini`; a renamed `ucc.exe` also created `ucc.ini` and `ucc.log` there. `Save0.usa` was not
  touched (timestamp unchanged). Rule for runs: keep the sandbox `Default.ini` identical to the
  installed one, back up that folder first (`local/shrek2-docs-backup`), and diff afterwards.
  Only `Game.log` changed in the later runs.

Next:
1. Read the decompiled `KWGame`/`SHGame` code around the player: `KWGame.Login`, `ShHeroPawn`,
   `KWHeroController`/`ShrekController`, cameras and cutscene controllers, saves.
2. Find out whether the game can host (`open <map>?listen`) and whether a second client can join.
3. Design `ShrekCoop`: second hero pawn and controller, replication on the hero pawn, cutscene
   and camera policy, level-change and save policy.

## Listen server and the first join attempt (2026-10-30)

Observed (sandbox copy of the original game, mod package `ShrekCoop`, logs under `local/`):
- `Game.exe 1_Shreks_Swamp?Game=ShrekCoop.CoopGame?listen -log` starts a **listen server**:
  `DevNet: TcpNetDriver on port 7777`, `ShrekCoop: CoopGame is running, NetMode=2`. The game's
  netcode works with the shipped binaries.
- A second `Game.exe` with `127.0.0.1:7777` connects: `HELLO`, `NETSPEED`, `LOGIN`, the server
  sends `WELCOME LEVEL=1_Shreks_Swamp` and the package list (78 `USES` lines), the client loads the
  level, spawns its player controller (`Login: Player`) and sends `JOIN`.
- **The server then rejects the client on the MD5 check** (`The client has failed one of the MD5
  tests`, host log `[INVALID PACKAGE]`, then `Spoof Attempt` and the connection closes). With the
  database as installed, the 11 code packages plus `ShrekCoop` pass; exactly one package fails:
  stock `AmbCreatureAnims.ukx` (GUID `765C249C701BA1AF491827AC56C32E43`). Reason: the shipped
  `Packages.md5` holds only 21 native-package entries, so the server only knows packages it adds at
  start-up itself.
- How start-up entries appear: the engine adds an MD5 entry for every package in
  `[Editor.EditorEngine] EditPackages` of the user `Game.ini` (11 entries with the original list,
  1 entry when the list is just `ShrekCoop`). Listing `AmbCreatureAnims` there did **not** add an
  entry for it (a `.ukx`); `ServerPackages=` entries are sent to clients but do not add entries.
- `ucc Engine.MasterMD5 -s` (the Level Editor's UCC, `Engine.MasterMD5` is the commandlet name) shows
  the database; `-a` with a hard-linked `..\MD5\1\` folder added nothing. Unsolved.

Other findings:
- A second `Game.exe` exits silently (code 0) because of a `UnrealIsRunning` mutex created by
  `Game.exe` (imports `CreateMutexA/W`). Not patched in the exe; a test-only proxy `d3d8.dll` (5
  forwarded exports, system `d3d8.dll` kept as `d3d8_real.dll`) rewrites the mutex name per process
  in its own process. Same proxy redirects `SHGetFolderPathW(CSIDL_PERSONAL)` (the game fetches it
  with `GetProcAddress` from `shfolder.dll`) to the folder in the env var `SHREK2_DOCS`, so every test
  instance has its own `Shrek 2\Game.ini`, log and saves. Built with the existing llvm-mingw
  (`i686-w64-mingw32-clang`); source kept in `local/shrek2-tools/mutexfix` (not published; only used in
  sandboxes). Verified: runs with it changed 0 files in the owner's `Documents\Shrek 2`.
- Window focus loss calls `PlayerController.SetPause` and froze the host (the shared world). `CoopGame`
  now overrides `GameInfo.SetPause` to refuse (log `pause request ignored`).
- `KWGame.Login()` returns the existing main-player controller; the second player needs a different
  path (new hero pawn). Not started.

MD5 check solved (2026-10-30):
- Disassembled the game's own `Engine.dll` (copy) with Ghidra headless; only the functions that
  reference MD5 strings were decompiled (`local/shrek2-tools/ghidra/DumpMd5.java`, output kept in
  `local/`, not published).
- `Engine.MasterMD5` takes a sub-command word first (`-a` add, `-c` create, `-s` show, ...). For `-a`,
  **the following words are file masks, not a revision number**. The commandlet lists the folders
  next to its own `System` (the parent folder of the working directory), and for each folder `X`
  applies the masks to `..\X\`. Hash and GUID of every matching package are read; new GUIDs are
  added to the database and `Packages.md5` is saved.
- Recipe that worked: copy the game's `Packages.md5` into the Level Editor's `System`, put hard
  links of the game's packages into a folder `MD5` beside that `System`, run
  `UCC.exe Engine.MasterMD5 -a *.u *.ukx *.utx *.usx *.uax *.umx *.unr` from `System`
  (157 new GUIDs, 139 from the game's packages, 18 stray ones from the editor's own `System`), copy the
  resulting `Packages.md5` into the sandbox game's `System` (21,349 bytes instead of 2,612). The
  installed game's file is untouched.
- Result: host log has **0** `[INVALID PACKAGE]`; the client is accepted by the MD5 test.
- Consequence for a release: every player's `Packages.md5` must contain the entries (a mod-supplied
  replacement file, or entries produced at start-up from `EditPackages` in `Game.ini`, which would
  edit the player's ini). Decide before shipping.

The next failure appears right after the join request, as predicted by the source:
`CoopGame0 Login found existing player/controller Shrek0 KWCutControllerII3`, `Login failed`,
`Join failure`. `KWGame.Login()` hands back the host's own controller, which the engine refuses for a
second connection.

Next:
1. Override `Login` in `CoopGame` to give the new connection its own `KWHeroController`.
   `KWHeroController.SwitchToPawn(P)` shows the game's own way to move control: unpossess the old
   controller, `Possess(P)`, `Camera.InitTarget(P, ...)`. The second player would take the companion
   (Donkey) that normally follows the player as an AI companion.
2. Camera and input per player, replication of the hero pawns and controllers, cutscene policy.

## Second player on the map (2026-10-30)

Result: the second player joins and stands on the map as **Donkey**, controlled over the network.
Verified in the host log (`CoopPlayerController0 player=TcpipConnection0 ... pawn: Donkey0`, moved to
the real start spot) and by looking at the windows (host shows Shrek with Donkey beside him).
Sources live in `local/shrek2-coop/ShrekCoop/Classes` (`CoopGame`, `CoopPlayerController`,
`CoopDebug`); build with `build.ps1`, test with `run2.ps1` (host plus client, separate settings
folders), watch with `show.ps1`. Not published yet (decide the repository first).

What it took, in order (each was a crash or a blocker, with the cause):
1. `foreach AllActors(...)` in my own function crashed the host. Walking `Level.ControllerList`
   instead works. Cause not proven; the bytecode comes from the editor's compiler.
2. `Possess` inside `Login` crashed: the client's connection is attached only after `Login`
   returns. Possession now happens in `PostLogin`.
3. The game's own hero controller (`DonkeyController`) on the joined client has no `BaseCam` camera and
   its HUD items read server-only actors: 30,000 "Accessed None" lines per minute, the window
   looked frozen. Player two now uses a stock `CoopPlayerController` (third-person chase camera) and
   `HUDType=Engine.HUD`.
4. Donkey sits at his intro-cutscene staging spot (about x=-17400); the hero is moved to the first
   `PlayerStart` of the current game state after possession.
5. Fields that differ between the editor's engine and the game's engine (for example `Pawn.Health`,
   `LevelInfo.PlayerHeroActor`) must be read as text (`GetPropertyText`) in code compiled with the
   editor, otherwise compile errors or crashes.

6. Joining during the intro and taking Donkey from his cutscene controller froze the host's intro for
   good (the sequence waits for his cue). Fixed: if `Hero.Controller` is a `KWCutController`, player
   two only watches him (`SetViewTarget`) and a one-second timer (`CoopAttach`) hands Donkey over when
   the cutscene releases him. Test t10 (join at host time 28.8 s): intro ended at 113 s, the host's Shrek
   got his pawn at 115 s, player two possessed Donkey at 113.2 s and stood at the start spot; 0 crashes in
   a 5-minute run, client log 489 lines with no spam. Nobody moved player two yet (no input), so
   movement and replication of a moving pawn are untested.

**Open problem (host crash on a late join).** If the client joins after the game has saved once, the host
crashes in `UPackageMap::Compute` (assertion `List(i).Linker` in `UnCoreNet.cpp:238`, found with Ghidra
in the game's `Core.dll`). Cause: the game's save (`SaveSlottedGame`, which sets `Level.LevelAction=2`)
makes the engine detach the level package's loader (log line `Unloading: Package 1_Shreks_Swamp`),
while the network package map built at level start still points at it. All successful joins so far
happened during the intro cutscene, before the first save. Consequences for the design: a join has to
happen before the first save of a level (lobby that waits for player two), or the map has to be
rebuilt after each save. Not solved.

Confirmed with a probe (test t14): `obj linkers` (a console command, callable from script through
`ConsoleCommand`, returns the list of open package loaders) shows the level package `1_Shreks_Swamp` at
t=3.4 s and **not any more at t=111.7 s**, right after the save (`Saving game...`, `Unloading: Package
1_Shreks_Swamp`; the listing shrank from 9453 to 9328 characters). The other probed packages (`ShGame`,
`KWGame`, `AmbientCreatures`, `ShrekCoop`, `AmbCreatureAnims`, `1_Shreks_Swamp_SM`) stay. So the save does
detach the level's loader, as suspected. In t11 to t14 the late client never got past loading its
local placeholder map, so the crash itself was not reproduced again; a repair by `obj load` was never
exercised. Two pitfalls: logging the whole 9 KB listing in one `Log` crashed the engine (log only
length and probes), and string parsing of that listing in script crashed it as well.
Decision (owner): avoid the problem by design, with a lobby where player two connects before the game
starts (see Plan).

Other notes:
- A game window that is not in the foreground does not progress: a client started in the background
  took almost three minutes to join. `show.ps1` keeps the client in front while it loads.
- Joining during the intro destroys Donkey's cutscene controller (`KWCutControllerII5`); the intro
  keeps running without him.
- The second player has no game HUD, no Shrek abilities (attack, potions) yet; the pawn is the game's
  own `Donkey`. Many game scripts look at `Level.PlayerHeroActor` (one global hero reference), for
  example enemies, crates, pickups: 80 places use `IsA('Shrek'|'Donkey'|...)`. Per-hero abilities come
  from the shared `ShHeroPawn`; `PIB` additionally can climb ladders.

## Plan (owner's decisions, 2026-10-30)

The mod is a broad "make the game better" mod with co-op as its foundation. Decisions given by the owner:

1. **Both players play the same character** (Shrek), so they have the same abilities. This replaces the
   Donkey experiment, where player two could not do everything player one can.
2. **Stronger enemies, a harder ("hardcore") game.**
3. **Money and health-recovery balance stay as they are** in the original game.
4. **Checkpoints:** solve the checkpoint problem. **Both players must pass a checkpoint** before it counts.
5. **Cutscenes:** during cutscenes the second player disappears (if he would otherwise appear in them).
6. **A new in-game menu** with the co-op settings and the lobby.
7. **Player two connects before the game starts** (in the lobby), not during or after play.
8. **Player two's character appears only after the cutscenes** (the intro included), not before. This
   refines items 5 and 7: the connection is made in the lobby, but his pawn is created or shown only
   once the opening cutscene has finished.
9. **Engine-side changes are done for modern PCs and technologies** (multi-core CPUs, fast SSDs, high
   resolutions and widescreen, current Windows), not for 2004 hardware limits. The save rework follows
   this: the old save is one synchronous 80 ms write on the game thread; on a modern machine the
   level should be serialized to memory in one short step and written to disk on a worker thread, with no
   saving screen and no freeze. How much of the 80 ms is serialization and how much is disk is unknown;
   measure it before deciding (see "Save system").
10. **Save system rework**: a checkpoint save must not stop the game, must not show the saving
   plaque, must not detach the level's loader, must leave player two out of the file, and counts only when
   both players are at the checkpoint (see "Save system: how it works today").

What follows from these decisions (derived from the findings above, not yet tested):
- Two Shreks: the second hero is a second pawn of the Shrek class (spawned, not taken from the level), with
  `Level.PlayerHeroActor` (one global hero reference used by about 80 scripts) needing a rule for who it
  points at. Story scripts that look for "the" Shrek may need per-player handling.
- The lobby also fits the **late-join crash**: the host waits in the lobby until player two is connected
  and only then starts the level, so nobody joins after the first save of a level.
- Checkpoints go through `SavePointFairy` / `SHSavePoint` and the game's save call
  (`SaveSlottedGame`); a checkpoint would need to count both players (both inside the trigger) before the
  save runs. To be read in the decompiled sources.
- "Disappears in cutscenes": hide and freeze player two's pawn while a `KWCutController` is active and
  bring him back to player one's position afterwards (the intro already shows how cutscenes take pawns).
- Harder enemies: scale health, damage and aggression of the enemy classes; money and health-pickup values
  untouched. Settings belong in the new menu.
- The menu has to be built on the game's own GUI classes (`KWGame` `MainMenuPage`, `OptionsPage` and the
  GUI package) and compiled with the editor's toolchain like the rest of the mod.

## Save system: how it works today (2026-10-30)

Sources: decompiled scripts in `local/shrek2-src`, and the game's own `Engine.dll` (copy) in Ghidra
(`FUN_10418de0` = the native save, `FUN_10419720` = the console command handler, `FUN_10422cb0` = the engine
tick; the addresses are image addresses of this build, not stable across builds).

**Who triggers a save** (all script, all on the host's hero controller):
- A checkpoint: `SHSavePoint` is touched by `Level.PlayerHeroActor` (the one global hero) -> its
  `SavePointFairy` controller goes to `HeroTouched` -> `ShHeroPawn.OnSaveGame()` -> `SHSavePoint.OnSaveGame`
  -> `ShHeroController.SaveGame()` which sets the slot, `SetSaveGameImage`, and `bQueuedToSaveGame = true`;
  then the fairy plays its magic animation and flies away. `SaveTimer` and the level-start autosave
  (`KWHeroController.OnEnginePreFirstTick`, `Level.AutoSaveAtLevelStartSlot`) call the same functions.
- `ShHeroController.SaveSlottedGame` and `KWHeroController.SaveSlottedGame` set `Level.LevelAction = 2`
  (LEVACT_Saving). `QuickSave` runs the console command `SaveGame 9`.

**What the engine does** (`FUN_10418de0(slot)`, runs synchronously inside one tick):
1. `LevelAction = Saving`; log `Saving into slot [n]: ...\Save\Save<n>.usa`.
2. Calls `PreSaveGame` then `SavePersistentData` on **every actor** of the level.
3. `GWarn->BeginSlowTask("Saving", ...)` (the engine's progress indicator).
4. `UObject::SavePackage(Level.Outer, Level, ..., filename)`: the **whole level package** is written
   (about 80 ms measured on the swamp level, `Save=79`); this resets the level package's loader.
5. Copies the per-level `Game<i>.usa` snapshot files next to it and deletes stale ones.
6. Sets a few `AMover` fields to -1, `EndSlowTask`, `LevelAction = None`, flushes the memory cache, calls
   `PostSaveGame` on every actor.

**Side effects that matter for co-op and for the owner's request**
- A hitch of the length of the save (no frames are drawn in between) and the "saving" indication:
  `ShHud.DrawLevelAction` draws the `saveimage` tile while `LevelAction == 2`, and `BeginSlowTask` shows the
  engine progress text.
- The loader of the level package is detached (the late-join crash above).
- The package contains **everything in the level**, including a second player's pawn and controller.
  On load the game restores actors from the package (`RestorePersistentData`, `PostPersistentDataRestored`),
  so a second hero stored in a save will confuse it (the "more than one hero" problem seen in other
  projects).
- Load goes through `?load=<slot>` / `ClientTravel` and `FUN_1041b480` ("Loading game").

**Rework: options**
- A. Cosmetics only (script): a HUD subclass that does not draw the saving tile. Does not remove the hitch
  or the loader problem.
- B. Native hook layer (owner agreed on 2026-10-30 to change the engine side too): a DLL loaded next to
  `Game.exe` (the sandbox already has such a proxy, `d3d8.dll`) hooks the engine's save routine and
  replaces it with a version that (1) does not show the saving indication, (2) keeps the level
  package's loader attached (or re-attaches it) so the network package map stays valid, (3) leaves out
  player two's actors, and (4) can run only when **both players** have reached the checkpoint. Also
  patches or avoids `UPackageMap::Compute` for entries without a loader.
- C. Replace the file save by a small **checkpoint record** (level, checkpoint, per-player position, health,
  inventory, the game's persistence flags) and rebuild the level state on load instead of dumping the
  level package. Biggest change, removes the hitch and the loader problem completely, needs a full
  re-implementation of what `SavePersistentData` and the package restore did.
Recommended order: B (hook, no plaque, loader kept, player two excluded, both-at-checkpoint rule), then
decide on C after measuring how big a save is in larger levels (the swamp level is small).

## Save rework, variant C: design (owner decision 2026-10-30)

Decision: no stock save at all. The game's level dump (`SavePackage` of the whole level) is replaced by a
small **checkpoint record**, and the game's save and checkpoint classes are replaced by the mod's own.
Reason given: the owner wants the game reworked, not patched, and does not want the stock functions kept.

**What already exists and can be reused (observed in the decompiled scripts)**
- The game has a **state layer**: `PersistentStore` (arrays `Keys` and `Values`, strings) in `KWGame`, native
  `CreateSlot/FindSlot`, with `SaveBool/Byte/Int/Float` and `Restore*`. Actors opt in through
  `SavePersistentData()` and `RestorePersistentData()` (plus `PrePersistentDataRestored` and
  `PostPersistentDataRestored`). Keys are `Level.ID + ActorName + "_suffix"`. Classes that implement
  it: `KWTrigger` (active flag), `KWPickup` (picked up, location), `KWMover`, `KWDecoration`, `KWGenericSpawner`,
  `BreakableObject` (destroyed), `Counter`, `RoundRobin`, `MaterialTrigger`, `PlayerStart`.
- It is used for level changes: `LevelInfo.ServerTravel` calls `SavePersistentData()` on every actor, the
  store goes into `KWWorldInfo.TempPersKeys/TempPersValues` (travel) and `PersistentStore.TravelPostAccept`
  copies it back; at every level start the engine calls `PrePersistentDataRestored`, `RestorePersistentData`
  and `PostPersistentDataRestored` (seen in the logs after `PostBeginPlay` of the game).
- The game state is a string, `KWGame.CurrentGameState` ("GSTATE000"...), set by `SetGameState`, passed on level
  changes as the URL option `?GameState=`.
- Inventory (coins, potions) lives on one shared actor, `Level.InventoryCarrierPawn` (`GetInventoryCount('Coin')`
  reads it). Only `Health` is copied by `ShHeroPawn.TravelPreAccept`. Some hero numbers are `globalconfig`
  (user ini): `bonuscoins`, `bonusHealth`, the first-strength-potion flags.
- Iterating actors works in code built with the editor (`foreach AllActors(Class'Engine.Actor', A)` counted
  1,800 actors, `Triggers` 44, no crash; the earlier host crash was probably the `return` inside a `foreach`).
  Enemies and in-progress scripted sequences are **not** in the state layer.

**Checkpoint record** (stored by the mod, small: a few hundred key/value pairs):
header (format version, time, map, game state, checkpoint name, slot), the `PersistentStore` pairs after calling
`SavePersistentData()` on every actor, the shared inventory counts, per-player data (position, rotation,
`Health`, `MaxHealth`, selected potion), the co-op settings. Not saved: enemy state (they come back as the
level starts), running cutscenes, projectiles.

**Load**: open the map with `?GameState=` and a mod option naming the record; the mod's game class fills the
`PersistentStore` from the record in `InitGame`/`PostBeginPlay`, before the engine's restore pass; after the
players exist, put them at the saved positions and restore health and inventory.

**How the stock classes are replaced without touching the game's files**: a mod `Mutator` (`CheckReplacement`
sees every actor when the level starts; the game already runs a base mutator) swaps `SHSavePoint` for the mod's
`CoopSavePoint` (touch rule: both players inside, then capture the record; no fairy sequence that freezes the
hero, no saving tile), swaps the hero controller for the mod's subclass of `ShrekController` (its
`SaveGame()` and `SaveSlottedGame()` no longer set `bQueuedToSaveGame` or `LevelAction`, so the engine's
native save is never entered), and scales enemies in place for the hardcore setting. The same mutator is the
place for the second Shrek and the cutscene hiding.

**Why this also fixes the other problems**: no `SavePackage` means the level's loader is never detached
(late-join crash gone), no `LevelAction=Saving` means no saving tile and no engine progress text, nothing
heavy runs on the game thread (a record of a few KB), and the second player is never part of a file.

**Build stages**
1. Mutator `CoopMutator` plus a log of what `CheckReplacement` sees at level start (order, count).
2. `CoopSave`: capture the record at a trigger (first by console command for testing), write it (config
   class or the game's ini mechanism), read it back and log it; measure time.
3. `CoopSavePoint` replacing `SHSavePoint`, with the both-players rule; stock hero controller swapped so the
   engine save is never reached; check that no `Saving game` appears in the log.
4. Load: option on the URL, store filling, positions, health and inventory; test: collect items, save at a
   checkpoint, quit, load, compare.
5. Autosave at level start and between levels through the same record.
Risks to check first: whether the store exists when `InitGame` runs; whether swapping the hero controller
keeps all hero code working; where to keep the files (the owner's `Documents\Shrek 2\Save` is for the stock
format; the mod should use its own folder or name).

## Save rework: stages 1 and 2 done (2026-10-30)

**Stage 1, the mutator.** `CoopGame.InitGame` adds `CoopMutator` (`AddMutator("ShrekCoop.CoopMutator")`).
`Actor.PreBeginPlay` calls `Level.Game.BaseMutator.CheckRelevance`, which reaches `CheckReplacement`. Observed
on the swamp level: it sees **all 1,800 actors** (126 classes) when the level starts, including `ShGame.Shrek`,
`ShGame.SavePointFairy`, `ShGame.ShrekController`, `ShGame.SHCompanionController`, `Engine.PlayerStart`,
`KWGame.PersistentStore` (the store exists by then), `ShGame.SHHud`, 31 `CutSceneTrigger`, 23 `Coin`, 13
`BriarPatch`. There is no `SHSavePoint` on this level; the checkpoint is a `SavePointFairy` placed directly, so the
checkpoint logic has to be read from `SavePointFairy` and its controller, not from `SHSavePoint` only.

**Stage 2, the record.** `CoopSaveData` (`PerObjectConfig`, file `ShrekCoopSave.ini`, one section per slot) and
`CoopGame.CaptureCheckpoint(slot, name)` write: map, game state, checkpoint name, time, the key/value pairs, coins,
one line per player. Verified: record written and read back (`keys=13 values=13`, map, state, stamp), no crash.
The pairs are the live `PersistentStore` plus what the ten persistent classes would write, produced by typed
`foreach` loops over `KWTrigger`, `KWPickup`, `KWMover`, `KWDecoration`, `KWGenericSpawner`, `BreakableObject`,
`Counter`, `RoundRobin`, `PlayerStart` (the keys and value strings are the game's own, so its
`RestorePersistentData` can read them back at load). `MaterialTrigger` (switch index per material) is not
written yet. Heroes and coins stay empty while a cutscene owns the players (the capture ran during the intro).
Cost: 20 captures without a file in one frame stayed under 50 ms (so about 2.5 ms each); one capture with the
file write is inside the normal frame noise of this level (the log shows 50 to 150 ms frames every few
seconds anyway). The file write is the part to move off the game thread later.

**Toolchain limits found on the way (matter for every later stage)**
- The Level Editor's UCC compiles against the *editor's* `Engine.u` and Master_64's rebuilt `KWGame`/`SHGame`,
  not against the game's. Members that exist only in the game's engine do not compile:
  `Actor.SavePersistentData`, `LevelInfo.PlayerHeroActor`, `LevelInfo.LevelAction` fields used as typed members,
  `GameInfo.PersistentStore`, `LevelInfo.InventoryCarrierPawn`. Workarounds that work: `GetPropertyText` and
  `SetPropertyText` for properties, finding objects by class (`foreach AllActors(Class'KWGame.PersistentStore', PS)`),
  and reproducing small engine-side loops in the mod's own code.
- UCC cannot be run against the game's real packages: the editor's `UCC.exe` with the game's `Core.dll` and
  `Engine.dll` dies in an assertion (`UnObjBas.h:1789`), and with the editor's DLLs the game's `Core.u` fails to
  bind native functions (`NeedSAPFont`). One import (`appSaveStringToFile`, signature differs) had to be patched
  in a copy of `UCC.exe` to even start it. A native bridge would remove this class of problem: a DLL that
  registers a native in the engine's native table can call any game function by name.
- A string longer than a few KB passed to `Log` crashed the engine, and a string-parsing loop over a 9 KB
  `ConsoleCommand("obj linkers")` result did too.
- `return` inside a `foreach` and a typed `foreach` over a class unrelated to the out variable both break
  (the first at runtime in this engine, the second at compile time).
- A tiny proxy `shfolder.dll` (source kept in `local/shrek2-tools/shfolder`) redirects the Documents folder of
  tools through `SHREK2_DOCS`; the same is done for the game by the `d3d8.dll` proxy.

## Save rework: stage 3a done, the stock save is out of the loop (2026-10-30)

**Finding: one choke point.** Every save trigger of the game ends in `ShHeroPawn.OnSaveGame()`: the fairy's
`Touch`, the fairy controller's `HeroTouched`, `SHSmartStart`, `SaveTimer`, the wanted poster, the bonus-level
timer, `SHSavePoint`, and `ShHeroController.SaveSlottedGame` (which also sets `LevelAction = 2`). That function
fills the stock save-slot UI info (`SaveInfo`, written to the ini) and then calls
`ShHeroController(Level.PlayerHeroActor.Controller).SaveGame()`, which sets `bQueuedToSaveGame = true`; the engine
turns that into the native save. Level-start autosave also goes through `SaveSlottedGame` (slot 1 here, at the
end of the intro).

**What was built.** `CoopShrekController extends ShrekController` overrides `SaveGame()` and `SaveSlottedGame()`
and calls `CoopGame.OnCheckpointRequest` instead of queuing the engine save (requests within one second are
merged). The mutator sets `Shrek.DefaultPlayerControllerClass` to the new controller, so the game's own `Login`
makes the host's controller from it (the level did not hold a placed hero controller at that time; the removal
branch in `CheckReplacement` never fired).

**Verified (host alone, 150 s, and a second run with a late client):**
- `CoopShrekController0 got player WindowsViewport0`: the host plays with the mod's controller. HUD, health bar,
  potion, coin bag, Shrek and Donkey in the frame (screenshot), the intro ends and Shrek gets his pawn.
- At the end of the intro (t=110 s) the autosave request is intercepted: `checkpoint requested by
  CoopShrekController0 (SaveSlottedGame 1)` and the record is written with `heroes=1`. **No** `Saving game`,
  `Saving into slot` or `SetSaveGameImage` line in the log, no save file written by the engine.
- The loader of the level package stays: the `obj linkers` probe shows the same list before and after the
  autosave (in the stock flow it shrank at t=111.7 s and `1_Shreks_Swamp` disappeared). So the cause of the
  host crash on a late join (`UPackageMap::Compute`) is gone. Zero `Critical` lines in both runs.
- Open: a client started late (after about 110 s of host time) connects (`NetComeGo Open`, `HELLO` arrives) but
  never finishes loading its own placeholder map (client log stops at `Initialising Karma for Level`), so the
  join itself could not be completed in that situation. Early joins still work. Cause unknown; it is on the
  client side, not in the host. The lobby design (connect before the game starts) avoids the situation.

## Save rework: stage 4a done, world state loads from the record (2026-10-30)

Load is started with the URL option `?CoopLoad=<slot>` (`CoopGame.InitGame` reads the record with
`new(None, Slot) CoopSaveData` and checks the map). The saved pairs are put into the game's `PersistentStore`
with `SaveValue` **by the mutator when it sees the store actor start** (`CheckReplacement` of
`KWGame.PersistentStore`): the store does not exist yet in the game's `PostBeginPlay`, and in the mutator's
call it exists and keeps the pairs. The game's own restore pass then reads them.

Test (host alone, no input): run 1 marked `BreakableCrate2` as destroyed (`WasDestroyed=1`), took a
checkpoint at t=20 s (`Keys=BreakableCrate2_WasDestroyed`, `Values=1`, other breakables `0`). Run 2 started with
`?CoopLoad=test`: log `load: store had 0 pairs, now 13 (record 13)`; at t=6.4 s **crates=3** (baseline run:
4), door blocks 8 and coins 23 unchanged, and the actor count is two lower. Zero `Critical`. So a level can be
re-entered from a small record without the stock level dump.

Findings and gaps:
- The record file lands in `System\ShrekCoopSave.ini` (the config name decides the folder); in a player's
  install that is the game folder. It needs a per-profile place before release.
- Not yet applied on load: the players' positions, health, coins and potions (the record has them), and the
  skip of the opening cutscene (the intro still plays after a load, because the level starts as a fresh
  level; the stock game avoids it through its `bLoadFromSaveGame` path).
- The record keys per breakable are `<Name>_WasDestroyed` without the level id (the game's own scheme): a
  second level with an object of the same name would clash. To check when other levels are tested.

## Save rework: stage 4b done, hero state loads and the intro is skipped (2026-10-30)

How the game starts the opening cutscene: the level's `PlayerStart` has an `Event` (here `Cut_NEWSwampIntro`)
that the `CutFactory` plays when the player spawns. The game's "smart start" (arriving from another level)
calls `ShHeroController.ClearPlayerStartEvents()`, which sets every `PlayerStart.Event` to `None`, and a stock
load passes `bLoadFromSaveGame = true` into `OnEnginePreFirstTick`, which also skips the level-start autosave.

`CoopShrekController` overrides `OnEnginePreFirstTick`: when the mod is loading a record (or the debug option
`?CoopSkipIntro=1` is given) it clears the `PlayerStart` events itself, passes `true` to the game's code, and
afterwards `CoopGame.ApplyHero` puts the saved hero state back (position, yaw, `Health`, coins through
`AddToInventoryCollection(CoinCollection, difference)`).

Test (host alone): run 1 with `?CoopSkipIntro=1`: no intro lines, the hero is free from t=0 (`pawn=Shrek0`,
`PlayerWalking`), the test moved him 300 units, set `Health` 37, added 25 coins and took a checkpoint
(`heroes=1 coins=25`). Run 2 with `?CoopLoad=test`: `hero Shrek0 placed at -12558.33,1234.32,-247.40 yaw 49352
health 37.000000`, `coins were 0, record 25, now 25`, `crates=3`, and no `NEWSWAMPINTRO.MAIN` step in the log
(the cutscene does not play). Zero `Critical`.

Open: only the first hero line is applied (single player); potions and the other inventory collections are
not recorded yet (only coins); `Health` is restored as text. The debug mutations (crate, hero) still run in
every session of the test build (`bMutateTest` in `CoopDebug`) and must be switched off for any real use.

## Both players and the checkpoint, the leash, the exits (2026-10-30)

Owner's rules: a checkpoint counts only when **both players have passed it**; a partner must not run away
from his friend: either he is brought back to the checkpoint, or the level change must not happen until the
second player is at that point.

**Checkpoint rule** (`CoopGame.OnCheckpointRequest`, `MarkPassed`, `AllPassed`, `Poll` from `CoopTicker`, every
half second). A request (from `OnSaveGame`, i.e. the fairy, the autosave, ...) fixes the place of the
checkpoint (the requester's position). A player has *passed* it once he has been within `CheckpointRadius`
(1500 units) of that place, at any time; leaving again does not undo it. The record is taken when every
player with a pawn has passed it. A second request at the same place is merged; a request at another place
replaces the waiting one. Verified (host plus joined client): everybody there: counts at once (`heroes=2` in
the record); partner far away: `checkpoint waits ... 1 of 2 have passed`; partner arrives later: counts.

**Leash** (`CheckLeash`, same heartbeat). Two players farther apart than `LeashDistance` (5000) for longer
than `LeashGrace` (6 s): the player who is ahead (farther from the last counted checkpoint, or from the level
start) gets the message "Wait for your partner" and is then moved to the last checkpoint if his partner is within
`LeashDistance` of it, otherwise next to his partner. Verified: `leash: ... CoopPlayerController0 is returned
to the last checkpoint at -12557.52,1233.12,-247.40` and the waiting checkpoint counted in the same second.
The "brought to his partner" branch was not exercised (the test positions were close to each other). Two players
only; a third player is not considered.

**Exits** (`RegisterExit`, `UpdateExits`). The mutator registers every `ChangeLevelTrigger` (two on the swamp
level, both to `2_Carriage_Hijack`). An exit works only while every player is within `ExitRadius` (1500) of it;
otherwise its collision is switched off, so the game's own trigger and level-change code never runs and nothing
needs to be undone (calling `ServerTravel` and cancelling it would leave the game state advanced and a fade
started). Verified: the host stood at an exit with the partner far away: nothing happened; when the partner
arrived the exit opened and, because the host was standing inside, **the trigger fired at once**:
`ProcessServerTravel: 2_Carriage_Hijack.unr`, then `LoadMap 2_Carriage_Hijack ...?Game=ShrekCoop.CoopGame?listen`
(the URL keeps the mod's game class and options) and the mod registered the exits of the new level. Not covered:
level changes that are not triggers (cutscene actions `KWACTION_ChangeLevel`, `KWCutScene` commands,
`SHMovieManager`, `bonusLevelTransferTimer`), what the joined client does after the travel, and a third player.

**Test stand**: `?CoopTest=1` (state mutations, test checkpoint, the rule/leash/exit sequence), `?CoopSkipIntro=1`,
`?CoopLoad=<slot>`. `run2.ps1` starts a host and a client; the mod writes a `status` record (`joined`) when the
client logs in, and the script restarts a client that does not join within 75 s (up to three tries): the client
sometimes hangs while loading its own placeholder map (log ends at `Initialising Karma for Level`), early or
late, about one run in three or four. Cause unknown.

**Still to do in stage 3:** the rule that both players must have reached a checkpoint before it counts
(distance of each player to the fairy's place, kept by the mutator), re-capture the coins and potions
(`coins=0` here), `MaterialTrigger` pairs, and replacing the fairy's visible "saving" sequence if it freezes
the hero (the fairy still flies in and plays its animation; nothing stops the hero in the log).

## Unknowns

- Whether this PC build has any multiplayer or split-screen mode at all.
- UE2 build number (look in `Engine.dll` `GBuildLabel` / package versions).
- Whether the Hiver loader and Master_64 SDK work on the Russian 1C edition (they target English).
- How the story levels run: loading, level transitions, save format, character switching
  (Shrek, Donkey, Puss in Boots, Fairy Godmother's Monster).
- Licenses of the Master_64 and SDK projects (itch pages: "open-source to an extent", credit
  required, contact before publishing). Check before building on them.
- Anti-cheat or online services: none seen (offline game, no launcher).

## Next steps (proposed, nothing done)

1. Owner confirms what "co-op" should mean: two local players on one screen, or online.
2. Copy the install to a test folder under `local/`, hash the copy, never run from the original.
3. Decompile `SHGame.u`/`KWGame.u` (UE2 package tools) and read the player controller, party
   switching and level-load code. Check whether a second `PlayerController` / `ViewPort` exists.
4. Check `Engine.dll` for `UViewport`/splitscreen and `IpDrv.dll` listen-server code paths.
5. Run the remaining research.md searches and open a "claim a game" issue.
6. Try launching the copy and entering `open <map>?listen` style commands only on the copy.

## Russian texture adaptation (comparison of the 1C install against the English baseline)

Method (observation): for every `UTexture` in the 12 packages that differ between the Russian install
and the English Level Editor set, the first mip's bytes (read by StorageOffset/StorageSize from the
package file) were hashed and compared per object path. Only mips[0] was compared.

Russian-adapted textures (pixel data differs), by package:

- `storybookanimTX`: 15 shared textures differ (character pages: rumple, georgie, tom, hare, goldi,
  emperor, Chicken, wolf, marge, humpty, knave, bopeep; In-Laws_1a, In-Laws_1b, Title_Page);
  RU-only: piedpiper, giant, pinocchio.
- `Shrek2_Univ_Tex.PickUps`: poster1..12_sm, herobar_tx, herocrate_tx.
- `SH_Menu.Wanted_Posters`: poster1..12 (only 3 of 15 textures are identical).
- `SH_Hud.Loading_Elements`: loading, loading_solid, saving, TimesUp.
- `EnvAnimsTx`: tour_sign, Hero_Arm, FGM_Menu, flash, Yelcab_LoPoly_TPage_tx, PaddyWagon_Tpage_tx.
- `10_Castle_Siege_Tex`: PaddyWagon_Tpage_tx, Yelcab_LoPoly_TPage_tx.
- `4_FGM_PIB_Tex`: sign_4, sign_5, FGM_Banner.
- `3_The_Hunt_Tex`: cauldron, signs (plus water_base / water_base_noleaves / water_sewer_clean,
  which differ by name between sets, probably not localization).
- `6_Hamlet_Tex`: sign_welcome2. `1_Shreks_Swamp_Tex`: sign_stayout.
  `FGM_Potion_Menu.new_drivethru`: FGM_Drvthru_Main3.
- `SH_Fonts`: the Russian set has the glyph pages SH*InkFont_Page* (13), English has `*_New` pages (9).
  The font textures must be kept as they are.

Everything else in these packages (hundreds of textures) is identical to English, so a pack of new
textures only has to keep the ~65 textures above. Guess (unverified): a pack that replaces whole
packages would overwrite them; replacing per texture by name avoids that.

## ShrekHD.zip (owner's HD texture pack, downloaded from the owner's server)

Identity: ShrekHD.zip, 706,482,733 bytes, SHA-256 `6d5bbd94d35320370d2e349794e33e0de6d11411bc2e959af0afcde85e720596`;
unpacked in `local/shrek2-hd/x/Shrek2HD` (not committed). Source Nexus page: unknown (the owner did not name it).

Observations:
- Contents: `LocalMips_0.tfc` (1.7 GB), `Shrek 2.TFCMapping`, `GameProfile.xml` (`enableCustomTFCs="true"`,
  package folders System/Animations/Maps/Sounds/Music/StaticMeshes/Textures), `GameProfile.IdRemappings.xml`,
  `ObjectDescriptors.xml`, `PackageExtensions.xml`. No readme, no installer, no exe.
- This is the "texture file cache" mod layout known from UE3 games (a `<Game>.TFCMapping` file plus `LocalMips_N.tfc`).
  Stock UE2.5 packages (version 129) have no external mip storage, so the pack needs a tool or engine patch that is
  not in the archive. How it is installed: `unknown`.
- The mapping holds 1251 texture entries by object path without the package name. Each entry lists 9 mips; for
  `signs\sign_welcome2` the top mip is 512x512 where the game has 256x256, so about 2x per axis.
- Overlap with the Russian-adapted list above (matching by name only, so unverified): present are georgie, goldi,
  emperor, marge, bopeep, Title_Page, In-Laws_1a/1b, PickUps herobar_tx / herocrate_tx, tour_sign, Hero_Arm,
  flash, the two truck atlases, sign_4, sign_5, FGM_Banner, cauldron, signs, sign_welcome2, sign_stayout,
  SH_Menu poster1/poster2. Not present: the SH_Hud loading/saving tiles, poster3..12, the *_sm posters, fonts.

## HD texture pack installed by our own tool; subtitles

Texture pack (observations, run on a copy): the pack's `LocalMips_0.tfc` holds ready mips (DXT1, DXT5 or
BGRA8) and `Shrek 2.TFCMapping` lists 1255 entries (object path without package, mip offsets and sizes). The
TFC Installer from the same author targets UE3; stock UE2.5 packages (v129) cannot reference external mips.
Our tool (private, `local/shrek2-hd/tool`) writes the HD mips into the texture exports of the `.utx` files
instead: new export bytes are appended to the package, the export table is rewritten at the end and the header
points to it. Result: 1277 textures patched in 33 packages, 25 Russian-adapted ones skipped (they stay Russian
at original size). Verified in the game: the log shows `tree_5` 1024x1024, `water_base` 2048x2048 (DXT5); on
screenshots at 1600x900 foliage and cliffs are visibly sharper than the original. At 640x480 the difference
is hard to see.

UE2 package facts used: a bool property carries its size byte when the info byte says so (size code 5); a
texture export is properties, then the mip array with a lazy-array skip pointer that is an absolute file position.

Subtitles (observation): the stock controller `KWCutTextController` draws a black bar and blue text. The mod
replaces it with its own controller (white text, name of the speaker in a soft colour, light shadow, translucent
dark band behind the lines). The game passes only the text to the HUD, not the dialog key, so the speaker is
found by the text: a table text-hash -> speaker code is built from the game's own dialog files
(`HpDialog.*`, `BumpDialog.*`, keys `pc_<speaker>_...`) and shipped as a localization file. Short lines said by
several characters are left without a name on purpose (149 of 2650 in the Russian set).

The mod was renamed to Shrek 2 Enhanced (package `Enhanced`); co-op is one of its features.

## Russian-adapted textures on the HD pack: what worked

Observation: 65 textures of the Russian install differ from the English ones. Compared per pixel at the original size:
about 20 differ only by the lettering (under 12% of the pixels), about 30 differ in the picture itself (15-50%),
8 are already drawn at twice the English size, 3 exist only in the Russian set.

What worked (private tool, run on copies): for lettering-only textures take the HD English picture, find the
English letters (by the Russian/English difference, or by the letters' own colour when the difference also
catches artwork), rebuild the background (smooth tone from the surroundings plus the grain of a quilted patch fill, so
no seams), then put the lettering on top and rebuild every mip level of the changed area.
- Lettering traced from the Russian original (crisp edges from the low-resolution glyphs; paint look on wood: warm
  off-white, grain showing through, rough edge) looks like the original and natural.
- A system font set on top looked foreign; a text in a book font on a sign that locals painted looked wrong. For such
  signs the lettering is drawn letter by letter, capitals with random size, tilt and baseline shift, rough paint edge,
  the colour of the English original.
Eight textures are done this way so far (well sign, cauldron, title page, two In-Laws pages, hero arm, two truck
atlases of one package). Open: poster sets, the ~30 pictures that differ, loading tiles.

Test harness note: the second Shrek must be placed on a floor at the host's height in sight of the host
(a ring spot over a ledge killed it by a fall of 119 units); the client of the two-process test stalled while
loading its local map with idle CPU: the main thread sat in Sleep. The cause is that the game sleeps while its window
is not the foreground window (and its connection then times out after 15 s). Windows refuses SetForegroundWindow
from a background process; faking an Alt press and attaching to the foreground thread's input queue lets it through,
and the test has to hold the client's window in front for the whole run.

## Friendly fire (setting, off by default)

The stock hero pawn refuses every hit from another hero (`ShHeroPawn.TakeDamage`: instigator is a hero -> return) and
the attack search skips pawns with `bIsAFriend` (true for heroes). The mod's pawn for player two overrides both
`TakeDamage` and `HitSHPawn`: with `FriendlyFire` > 0 (a damage scale, URL option `?FriendlyFire=0.5`) the hit goes
through with the scale and without an instigator (so the stock check does not refuse it), and `bIsAFriend` of both
Shreks is cleared. Test on a listen server with a second player: hit of 10 with the scale 0 -> health 100 -> 100;
with 0.5 -> 95. Not tested: real attacks by input (player two has no combat input yet).

## Player two's view: camera, animations, input (observations on a listen server plus one client)

- The game sleeps while its window is not the foreground window; a test harness must find the game window (the
  process's main window is its log console) and keep it in front.
- A joined client had no camera during cutscenes (it spectated its own controller). The cutscene camera of the host is
  computed by the game's `PlayerCalcView`; the server calls it every frame, moves a replicated invisible actor to that
  pose and makes it the view target of the second player, who then watches the same shots. After the cutscene the view
  goes back to his Shrek (a chase camera looking the way the host's does, a little from above: foliage has no collision
  for the camera).
- The other machine's pawn stood in the T pose: pawns of this single-player game do not send their animation state.
  Setting `Actor.bReplicateAnimations` on every pawn (done in the mutator when a pawn is created) is enough: the
  host's Shrek and the Donkey animate on the client.
- Keys reach the client (movement works). The hero's attack/pick up/throw is one server call
  (`ShHeroPawn.DoSomeAction`), sent from the client by an RPC.
- Player two is placed 125 units from the host on a floor at the host's height in sight of the host.

## Waiting for player two before the opening cutscene (option `EnhWait`)

The opening cutscene starts from the PlayerStart's event. With `?EnhWait=1` the mod takes the event off at the level
start, shows "waiting for player two" on the host's screen and sends the event 3 s after player two has joined (the host's
fire key starts the game alone). Player two's Shrek appears at once next to the host's, is hidden during the cutscene
(his camera is the host's cutscene camera) and comes back after it. Observation, one run: held at 0 s, player two
joined at 31 s, released at 47 s, the first checkpoint after the cutscene passed by both players at 157 s.
The chase camera of player two is computed by the mod (eye of his Shrek, straight back along the controller's
rotation, pulled in at geometry), the yaw after a cutscene comes from the host Shrek's facing, not from the host's
controller (its rotation is arbitrary after a cutscene).

## Level change with a client, without a fallback map or a waiting room (engine patch + script)

Observations (listen server on one PC, client on the same PC; stock `Engine.dll` of the 1C edition, sha256 79eb368b...):
- On `ServerTravel` the host closes the listening socket while it loads, the client's first join request is lost, and the
  engine neither repeats it nor keeps the old level alive: `ULevel::TickNetClient` (Engine.dll RVA 0x136e50) puts the travel
  `?closed` on the viewport when the old connection is closed, and that cancels the pending connection; the client then
  browses to the default map from `[URL] Map=` (`HP-Test.unr`, which does not exist in this game), in an endless loop.
- A second `Browse` while the old level still ticks crashes the client (`TickNetClient`, `UpdatePreNet`, `UpdateTime`): the
  old level keeps a net driver pointer that the pending connection already replaced.

What worked (all in a copy of the game, the patch is a script that checks the original bytes and never edits the input):
1. `UGameEngine::Tick` (RVA 0x122cb0): the tick of the current level is skipped while `UGameEngine+0x13c` (the pending
   level) is set; the old level stays on the screen as the last frame. The extra test sits in padding before the function.
2. `ULevel::TickNetClient`: checks that the net driver and the server connection exist, and skips the `?closed` travel
   while a pending level exists.
3. A script on the client (a viewport `Interaction`, which is ticked in every state while the old level is not) repeats
   `open <host>` every 3 s until the host answers. Result in one run: three requests, then "Welcomed by server" for the
   second level, no `?closed`, no missing-map errors, no crash; the host logged the client's login 5.5 s after its
   new level started.
Not tested: slow networks, several clients, the real cutscenes of the second level.
