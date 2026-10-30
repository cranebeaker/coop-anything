# Heat Signature: working notes

Phase 0 (triage), 2026-10-30.

## Game identity (observed)
- Title: Heat Signature (Suspicious Developments, 2017). Store: Steam, app id 268130, buildid 6124596.
- In-game version: **2021.1.20.1**, GameMaker engine 1.4.1804, build mode YYC (header line of the game's own
  `Logs/Log.txt`). Correction: the top entry of `ExtendedPatchNotes.txt` (2018.12.19.1) is stale; it is not the version.
- OS: Windows. Exe: `Heat_Signature.exe`, PE32 (i386), 73,653,760 bytes,
  SHA-256 `a5f8befe28786206ed1e2a52385051049524fea51e9e80660e3115e7126274d2`.
- Engine: GameMaker: Studio 1.x, YYC. Exe has the "Made with GameMaker: Studio" string, `c:\hudson\GMBase\GMGreen\...\VC_Runner` paths,
  and `options.ini` has a `[LLVM-WINDOWS]` section pointing at Visual Studio 11.0. `gml_Script_*` name strings are present
  (e.g. `gml_Script_AutopilotStep`).
- No separate `data.win`: there is a `FORM` chunk with `GEN8` inside the exe (GEN8 at byte offset 13355631).
- `steam_api.dll` ships next to the exe.

## Prior work (observed)
- coop-anything DIRECTORY.md / games/: no entry for Heat Signature.
- Web search, 2026-10-30: no co-op or multiplayer mod found.
  The Steam page says there is no multiplayer: https://store.steampowered.com/app/268130/Heat_Signature/
  Steam forum threads asking for co-op: https://steamcommunity.com/app/268130/discussions/0/144512526678821083/ ,
  https://steamcommunity.com/app/268130/discussions/0/1520386297686058374/
- Nexus Mods has a QoL mod that needs a "Heat Signature Mod Loader": https://www.nexusmods.com/heatsignature/mods/4
  (the page returned 403 to the fetcher, so how the loader works is unknown).
- Closest engine match with co-op: Forager (GMS2 YYC, `version.dll` + MinHook), games/forager.

## Derived (not tested)
- The YYC name-string trick from games/forager should work for naming functions here.
- Forager's runtime layer probably does not carry over directly: GMS1 is an older runtime than Forager's 2.2.5.
- Design problem: the player can pause time whenever they want (Steam forum threads above). Co-op needs a rule for
  this, such as pausing only when everyone pauses, or slow-mo instead of pause.

## Goal (from owner)
- Online co-op for up to 4 players total (host + 3 guests).
- Pause rule (owner's choice, 2026-10-30): **shared pause**. Anyone pausing pauses everyone, a banner shows who paused,
  and anyone can unpause. Slow-mo and fast-forward still need a rule (to decide once the code is mapped).
- Play style (owner's choice, 2026-10-30): **one shared mission**. All players fly pods together and board the same
  mission ship; the host picks missions. Each guest brings their own character file.

## Phase 2: workspace (2026-10-30)
- Test copy of the install in `local/game-test/` (exe SHA-256 matches the original).
- Tools: Ghidra 12.1.2 (Arch package), .NET 10.0.13, UndertaleModTool CLI 0.9.2.0 (Ubuntu build, zip SHA-256
  `d182f00c0e5ced8d9252aa1f796374cfee077a3889a658e70b68685f9cd559c7`, from the GitHub release).
- The data chunk sits inside the exe: `FORM` at file offset 17597096, size 54608162 (ends at 72205266 of 73653760).
  Cut those bytes out to a `.win` file and UTMT CLI `info` reads it (observed):
  `Is GMS2 - False`, `Is YYC - True`, bytecode version 16, 2005 scripts, 213 objects, 1 room, 7 shaders,
  0 extensions, 2783 strings.
- `strings` on the exe gives 2561 unique `gml_Object_*`/`gml_Script_*` names (557 object events).
- Objects of interest (names observed, roles guessed from the names): `oPlayer`, `oPlayerShip`, `oCamera`, `oControls`,
  `oTime`, `oSlowMotionEffect`, `oEnemy`, `oShip`, `oGalaxy`, `oSector`, `oMission`, `oSteamWorkshopManager`.
- Scripts of interest (observed names): `HandleInputs`, `InitialiseInputs`, `PauseMission`, `PauseFor`,
  `PlayerControlling`, `PlayerControllingObject`, `KillPlayer`, `EnemyCanSeePlayer`, `GeneratePlayerChecksum*`,
  `BlockListContainsSteamID`.
- Ghidra headless run started: name functions by the `gml_*` string each one references (same method as Forager), then
  decompile them. Script: `local/ghidra_scripts/YycNameAndDecompile.java` (to go into a game repo later).
- Save folder (observed, Proton): `%APPDATA%\Heat_Signature` (Roaming, not Local), i.e.
  `compatdata/268130/pfx/drive_c/users/steamuser/AppData/Roaming/Heat_Signature/`. Steam Cloud is on (`steam_autocloud.vdf`).
  - `Galaxy N/Galaxy.txt`: plain `key = value` text (`VersionNumber = 20170822`, `LiberationProgress`, then `<Station>` blocks
    with name, x, y, faction). About 55 KB.
  - `Galaxy N/Characters/<Name>.dat`: one file per character, about 2 KB. A `<Header>` block, then every line is base64 of a
    `key = value` line (`FileVersion = 7`, `Status`, `Forename`, `Skin`, ...). Shop files end with a `Checksum` line;
    the exe has `GeneratePlayerChecksumV2`..`V7`.
  - `Logs/Log.txt`: the game logs save and load steps with a ms timestamp (`[SaveSystem] Started saving character ...`).
    Useful for the QA bot.
- Derived: a guest's character is one small file, so a guest can bring their own character to the host's galaxy.
  The host keeps the galaxy and stations.

## Possible multiplayer leftovers
- Observed: scripts `PlayerOrItemIsLocal`, `PlayerRemoteControls`, `FindPlayerByName`, `PlayerParent` exist in the
  compiled exe, and there is a `PlayerStateGhost`. Also `SetTimeScale`, `SlowMoControls`, `PauseMission`/`UnpauseMission`.
- Observed: no `network_*`/`buffer_*` strings in STRG. That proves little, because YYC builtins are called through the
  runtime's table, not by name.
- Checked with objdump: `gml_Script_PlayerOrItemIsLocal` (the only function referencing its name string, VA `0xdc7f40`)
  does `with (argument0)` and compares instance variable slot `0x2bd` = `OriginalOwner` with a string. There is also a
  `RemoteControl` / `RemoteTriggerSignal` variable pair. Derived: "local" means "belongs to the player" and "remote"
  is the remote-trigger gadget, so these are most likely **not** network leftovers. Decompiling them will settle it.

## Runtime facts (observed, exe SHA-256 a5f8befe...)
- Image base `0x400000`, PE32, sections `.text`, `.rdata`, `.data` (55 MB, holds the embedded data chunk), `.mydata`, `.rsrc`, `.reloc`.
- YYC prologue: each GML function stores the VA of its own `gml_*` name string on its stack frame
  (`mov [esp+0x64], <name VA>`) and links the frame into the YYC stack-trace list at `0x49356e8`.
- Variable name table: `{const char* name; int id}` pairs at `.data` `0x14ba080` up to `0x14c0870`, 3200 unique ids.
  Code reads instance variables by these ids (`0x2bd` = `OriginalOwner`).
  Found by scanning `.data` for runs of 8-byte entries whose first field points to an identifier string.
- Builtin function name table: `{name; id}` pairs at `0x14c3cb0` (337 entries, starts `instance_nearest`, `point_distance`).
- The lowest ids are globals or near-globals: `Mission` 2, `Player` 5, `PlayerShip` 7, `TimeScale` 10, `Controls` 3071.
  Derived (to check): the game keeps **one** player in `Player`/`PlayerShip` and one `TimeScale`, the same
  single-player-behind-one-variable pattern as Forager.
- Globals (observed in decompiled oCamera/oControls/oMission events): GML globals live on one global instance whose
  pointer is at `0x0493d610`; a global is read at `yyvars + slot*16` (`_DAT_0493d610[1] + 0x50` = `global.Player`,
  `+ 0x20` = `global.Mission`), or through the vtable getter when `yyvars` is null. Same slot numbering as the
  variable-name table. Confirmed: **the player is `global.Player`**, one instance.
- Instance variables: `self->yyvars[slot]` (`param_1[1] + slot*16`), or `self->vtable[1](slot)` when `yyvars` is null.
- YYC stack-trace list head at `0x049356e8` (each function links a frame holding its own name).
- Time variables: `TimeScale`, `SlowMo`, `FastMo`, `SlowMoScale`, `FastMoScale`, `CanFastMo`, `PauseWithGame`,
  `PausedShips`, `PauseSecondsLeft`. The game has slow motion and fast forward, not only pause.

## Phase 5 probe (2026-10-30)
- Mod repo (local for now): `heat-signature-coop`, probe in `probe/` (MinHook v1.3.4, commit c3fcafdc, checked against the tag).
- The game imports only `GetFileVersionInfoW`, `GetFileVersionInfoSizeW`, `VerQueryValueW` from `VERSION.dll`, so the
  probe is a `version.dll` proxy. Under Proton it needs `WINEDLLOVERRIDES=version=n,b`.
- The exe has ASLR enabled (DllCharacteristics 0x8140), but under Proton it loaded at 0x400000 (observed). The probe
  rebases anyway.
- Run 1 to 3 (Proton Experimental, fresh test prefix, `steam_appid.txt` = 268130, **Steam client not running**): the probe
  loads, and all 9 hooks report `MH_OK`. But no GML ran in 45 to 75 s: no `DebugLog` calls, no `oMenu.Step`, no
  `Log.txt`. Derived (not tested): the runner stops before the first room without Steam.
- Lesson: `pkill -f Heat_Signature.exe` also kills Ghidra jobs that have the exe path in their command line. Stop the
  game with the test prefix's `wineserver -k`.

## Unknown
- How the Nexus mod loader works and whether it can host networking code.
- Whether UndertaleModTool reads a GMS1 data chunk that is embedded in the exe.
