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

- Run 4 (same setup, **Steam client running**): the game starts. Observed in `probe.log` (45 s at the title screen):
  - Steam init OK (`Starting workshop, AppID: 268130 init: 1 logged on: 1`).
  - `PlayerCreate` runs once at start. One `oPlayer` exists even on the title screen (the menu background flies it).
  - `global.Player` is a **real holding the instance id** (`real 100495`), equal to the `oPlayer`'s builtin `id`.
    It is not a pointer.
  - Step events run at 60 per second (`oPlayer.Step` count +120 per 2 s).
  - `global.TimeScale` = `real 1` at the menu. No `SetTimeScale`, `PauseFor` or `PauseMission` calls at the menu.
  - `SlowMo`/`FastMo` are unset on `oPlayer` (kind 0xffffff), so they live on another object (to find).
  - The probe reads strings from `DebugLog` arguments: a GMS1 string RValue's `ptr` points at a struct whose first field
    is the `char*`.
- Derived: Steam must be running for the game to reach its first room, so tests need a logged-in Steam client.
  Steam lobbies and P2P networking under app id 268130 are therefore available to the mod (Forager did the same).

## Input (observed, probe run 5, title screen)
- `ControlDown`, `ControlPressed`, `ControlReleased` take a **control index** (real) and sometimes a second real.
  Indices at the title screen: 0, 5, 6, 7 to 10, 13, 17, 20 to 24, 31.
- Derived: the index is the control's position in `<game dir>/DefaultControls.txt` (0 Use, 1 Loot, 2 Attack,
  3 Secondary Attack, 4 Inventory (Space), 5 Thrust, 6 Brake, 7 to 10 Move Right/Left/Down/Up, 13 Menu, 17 Hide HUD,
  20 to 24 Pan, 30 Slow Mo, 31 Fast Mo, 32 to 39 Item 1 to 8). The indices polled at the title screen fit this order;
  injecting a control has not confirmed it yet.
- A probe can answer these scripts itself (set the result RValue to real 1), which gives scripted input for a bot and,
  later, remote players' input.

## Bot attempt 1 (probe run 6)
- The second launch of a fresh prefix opens character select: `PlayerCreate` ran 5 times and `global.Player` was `real 49`,
  not the `oPlayer` id (unknown what 49 is).
- `BeginPracticeMode(argument0 = practice terminal id, argument1 = scenario)` is only called from
  `oPracticeTerminal.Step_0` (scenarios 14 and 15 seen). Calling it from the DLL with `noone` as the terminal **never
  returned** (the game hung or crashed). Starting a practice scenario needs a real terminal, or the bot has to go
  through the menus.

## Steam Cloud and test runs
- The test copy runs under the owner's Steam account, and the game has Steam Auto-Cloud. After probe run 6, Steam
  rewrote `userdata/<id>/268130/remotecache.vdf`. Checked: all 34 cloud entries keep their old timestamps, and their
  SHA-1s match the owner's real save files. Nothing from the test prefix was uploaded.
- Not known: whether a longer run, or one that saves characters, would upload. Safer: turn off Steam Cloud for the game
  while testing, and back up the real saves first (done, kept in `local/`).

## Input and time, confirmed with injected input (probe runs 9 and 10, 2026-10-30)
Setup: test prefix with `Options.txt` changed to `WindowedMode = 1`, `CaptureCursor = 0`, `PauseInBackground = 0`,
`MusicVolume = 0`, `SoundVolume = 0`, `SeasonalEvents = 0` (the "Space Halloween" pop-up otherwise covers character
select). The probe reads `bot_cmd.txt` every 15 frames from an `oControls.Step_0` hook.

Observed:
- **Clicks:** `PostMessage(WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP)` to the game's own window acts as a real click; the
  OS cursor does not move. Character select picks with `ControlPressed(12 Menu Click, 1)` +
  `CharacterIsChooseable(NearestCharacter)` -> `SelectThisCharacter(NearestCharacter, <id>)`. Afterwards
  `global.Player` = the chosen character's instance id. (Before the pick it is `real 49`; meaning unknown.)
- **Control indices confirmed:** one left click registered as `ControlDown` 2 (Attack), 5 (Thrust) and 12 (Menu Click),
  the three controls bound to mouse button 1 in `DefaultControls.txt`. So the index is the file order.
- **Injection works:** answering `ControlDown(7)` with real 1 for 2 s moved the controlled player from x -187 to -74;
  `ControlDown(10)` moved it up (y -60 to -86). The player stops when the override ends.
- **Time:** each source calls `gml_Script_SetTimeScale(scale, priority[, extra])` every frame it is active:
  Inventory (Space, `ControlPressed(4)`) -> `SetTimeScale(0, 9)` until pressed again (pause);
  Slow Mo held (`ControlDown(30)`) -> `SetTimeScale(0.2, 7, 0.3)`;
  Fast Mo held (`ControlDown(31)`) -> `SetTimeScale(6, 8)`.
  `global.TimeScale` follows (0, 0.2, 6) and goes back to 1 when no source calls.
- Derived: the highest priority wins each frame (pause 9 > fast 8 > slow 7); this is not read from code yet.

Derived design for co-op time (shared pause, owner's choice):
- The host collects each player's time request per frame (pause, slow, fast or none) and calls `SetTimeScale` itself:
  any pause -> `SetTimeScale(0, 9)`; else any slow -> slow; fast only when every player asks for fast.
- Guests do not run their own time; they take the host's `global.TimeScale` and the name of whoever paused.

## Second local player (probe, 2026-10-30)
Goal: two players walking independently in one game, no networking yet.

Observed:
- Object indices from the `OBJT` chunk: `oCamera` 9, `oControls` 14, `oCursor` 17, `oMenu` 26, `oTime` 42, `oPerson` 52,
  `oPlayer` 137, `oEnemy` 139, `oPlayerShip` 163. Script indices from `SCPT`: `PlayerStateFree` 1198 and the other
  `PlayerState*` scripts 1192 to 1203.
- `SelectThisCharacter(c)` -> `PlayAsCharacter(c)`. `PlayAsCharacter` first runs `with (oPlayerShip) DestroyContents(id)`
  on **every** pod, sets `global.Player`, creates the character's pod (`global.PlayerShip`), docks it, clears
  `Resting`, `CharSelectable` and `Highlighted`, and puts the previous character back in its seat
  (`State = "CharacterSelect"`). So it cannot be reused as-is to add a second player.
- `State` / `NewState` (slots 0x27f / 0x4d7) hold strings ("Free", "CharacterSelect"). `ChangeStateTo(<State RValue>)`
  sets `NewState`.
- Player input is read inside `oPlayer.Step_0` -> `PlayerStateFree` -> `PlayerMovementControls`. But
  `PlayerMovementControls` returns early unless `oCamera.Object` (slot 0x1b4) equals `global.Player`.
- **Two players work** with an impersonation window around player 2's `oPlayer.Step_0`. For that step only:
  `global.Player` = P2's id, `global.PlayerShip` = P2's pod (if any), `oCamera.Object` = P2, and every
  `ControlDown/Pressed` is answered from P2's input arrays (keyboard ignored). All values are restored afterwards.
  To prepare P2: `ChangeStateTo(P1.State)` (makes it "Free") and `CharSelectable = 0`.
  Result: P2 held Move Right for 2 s and moved from x -163 to 214 while P1 stayed at -163;
  then P1 held Move Up and moved from y -39 to -118 while P2 stayed. The camera stays on P1.

Not done yet (derived from the code read so far):
- Pods: one per player, without `PlayAsCharacter` destroying the others.
- Other code that asks "the player": guards' `EnemyCanSeePlayer`, `ChasePlayer` and doors use `global.Player`, so guards
  ignore P2 until those take a player argument or loop over players.
- P2's Draw and HUD: on a real guest machine P2 is that machine's own player, so this matters only for the host.

## Pods for a second player (probe, 2026-10-30)
Observed:
- `PlayAsCharacter(P2)` can build P2's pod without harming P1 if, for the duration of the call, `global.Player` is
  set to `noone` (-4), `DestroyContents` is skipped (it ran on 15 `oPlayerShip`-family instances), and
  `AddPlayerToCharacterSelect` is skipped (it reseats every other selectable character, P1 included). Then
  `global.PlayerShip` holds P2's new pod (P1's pod is untouched) and both players stay `"Free"`.
- Station layout (this test galaxy): the central hub at (0, -2) offers "Return to pod"; the green pad at (160, 0)
  offers "Get a mission". Use prompts are HUD text for the current player, not world labels. Use = `ControlPressed(0)`
  (a hold does not trigger it). `PathfindToCursor` = `ControlPressed(11)` + the mouse override works for P1 and P2.
- P1 at the hub + Use -> `InteractWith(100225)` -> P1 is in its pod (camera zooms out to the station exterior).
- P2 at the hub + Use -> `InteractWith(100222)` runs **inside** P2's window (`global.Player` = P2), but afterwards
  **P1** is in state `"Controlling"` (in its pod) and P2 stays `"Free"`. The action is carried out later by another
  object, which uses `global.Player` outside the window.

Derived (design consequence):
- Impersonating a second player on one machine breaks wherever the game defers work to another object that reads
  `global.Player` (teleporter, doors, guards' reactions). Each case would need its own patch.
- Better model: **each machine simulates its own player as the real `global.Player`** (pods, teleporters, inventory
  and use actions work unmodified), and the **host owns the world** (ships, guards, doors, loot, time). Remote
  players are puppets: an `oPlayer` (+ pod) per remote player whose position, animation and state come from the
  network. Guests send their position and the world actions they cause (door opened, guard hit, item taken); the host
  applies them and sends the world state back. On the host, guards must also see puppets (`EnemyCanSeePlayer`,
  `ChasePlayer` loop over players, or the impersonation window is used only around guard logic).
- What the single-machine tests still give us: spawning a puppet character + pod (`p2play`), driving a puppet's
  movement (`p2hold`), and the list of places that read `global.Player`.

## RESOLVED: the "frame stalls" were an R6025 error box (2026-10-30, 16:20)
- With `ptrace_scope` 0 (owner set it temporarily), gdb showed the game's main thread in `NtUserGetMessage`, a modal
  message loop. Listing the game's X windows found a visible **"Microsoft Visual C++ Runtime Library"** box (190x128,
  usually behind other windows): `R6025 - pure virtual function call`.
- A `MessageBoxW` hook in the loader caught it during the probe's own `layout` command: the probe called the instance
  variable getter (`vtable[1]`) on an instance that had been destroyed. Its destructor had reset the vtable to the
  base class's pure-virtual stubs. Fix: every instance access first checks that `inst->vtable` equals the vtable of a
  known live instance (`oPlayer` instances share one); the global instance has its own vtable and is exempt.
- So the earlier conclusions about Steam state, ntsync, overlay or Xalia were wrong: restarting Steam only "helped"
  because the next run happened not to touch a stale pointer, and the "no DLL" run was the normal galaxy intro.
  Correction (17:50): a *start-up* stall (GML stops after 1 to 3 frames, no error box, no dialog window) also exists.
  It happened four launches in a row with the current loader, with the older loader from commit a6700e9, with the
  loader but no core, and (judged by a static screen over 4 s, weaker evidence) with no DLL at all. Restarting Steam
  cleared it twice before. So there are two problems: the R6025 box (the mod's bug, fixed) and this start-up stall
  (unexplained, Steam-related). `game.sh start` now detects it (no frame 30 within 25 s) and relaunches up to 3 times.
  Best current explanation (18:10, derived, not yet confirmed): the desktop session was idle (`loginctl ... IdleHint=yes`,
  idle since 16:53), so the monitors were probably asleep. The stall comes exactly when the game starts drawing
  ("ModulesOnSurface set to -1 because we have no surfaces" is its last log line), and a Steam restart at 18:00 did
  **not** clear it while the owner was away. Earlier "fixes by restarting Steam" coincided with the owner being back at
  the PC. To confirm: launch with the display awake; long term, run test copies on a virtual display (gamescope
  headless or Xvfb) so they do not depend on the owner's monitors.
  Confirmed in practice (18:15): under headless gamescope the copies start every time, while the owner is still away.
  Lesson for the playbook: when a Proton game seems frozen, list *all* its X windows (`xdotool search --class
  steam_app_<appid>` + `getwindowname`) before anything else, and hook `MessageBoxA/W` from the start.

## Deterministic mission ships (practice scenario, 2026-10-30)
- Practice mode can be started from the DLL: `BeginPracticeMode(terminal id, scenario)` with `self` = the station's
  `oPracticeTerminal` (as its `Step_0` does; with `noone` as the terminal it hangs). Scenario 14: a practice ship with
  an airlock, 3 doors, 2 guards, 4 terminals and a practice terminal, all `Parent` = the ship.
- The game shifts its coordinate origin with the player (the station moved to about 12 million, 15 million when the
  practice ship was at the origin), so everything must be compared and synced ship-relative (`Fore`/`Starboard`).
- `random_set_seed(N)` can be called from the DLL through the builtin dispatcher `0x010a7f30`
  (`(self, other, RValue* result, argc, fn id, RValue** args)`, fn id read from `0x014c3fbc`).
- Observed: same seed (12345) on both copies right before `BeginPracticeMode` -> the 11 ship parts have identical
  `Fore`/`Starboard` (only a patrolling guard differs). Different seeds (1, 999) -> terminals at different places.
- Derived: with the same mission parameters and the same seed at generation time, a guest can build the host's ship
  locally; only live state (guards, doors, loot, alarms) then needs syncing. Not yet tested for galaxy missions, whose
  ships exist before their interiors are generated and whose generation may interleave with other random consumers.

## Galaxy missions: findings so far (2026-10-30, not yet tested end to end)
- Ship interiors are generated over several frames: `oShip.Step_0` calls `GenerateShipInSteps` (`0x00bc55b0`) while
  `Generated` (slot 0x2a0) is false, counting `ShipGenerationStep`. Other objects may draw random numbers in between.
- RNG state (observed by diffing memory across `random_set_seed(1)` and `(2)`): 16 dwords at `0x04b5c3c8`..`0x04b5c404`
  (WELL512-like), seed at `0x04b5c40c`; 72 bytes from `0x04b5c3c8` cover it. Plan: give each mission ship its own
  saved RNG state and swap it in around every `GenerateShipInSteps` call.
- `SaveMissionToFile(mission, file)` (`0x00a099d0`, called from `SaveCharacter`) writes the mission and ship parameters
  (type, faction, guard counts, alarms, sensors, segments, protocols, mercenaries, guard kits) to an open text file;
  `LoadMissionFromFile` (`0x009ed2d0`, 4 arguments when called from `CreateScenario`) reads them and creates the
  mission and its ship. Plan: host saves the chosen mission, sends the text and a seed; guest loads it.
- **Mission transfer works** (2026-10-30, observed): the host opens a text file with the builtin `file_text_open_write`,
  calls `SaveMissionToFile(mission id, file)`, closes it. The file (in the save folder: GameMaker sandboxes file
  access) holds base64 lines of a `<Mission>` block (type, faction, target, description, difficulty), a `<Ship>` block
  (guard and boss counts, segments, sentries, protocols, mercenaries) and `<GuardKit>` blocks; 45 lines, about 2 KB.
  The guest needs a `<Header>` block with `Encoded = 1` in front (otherwise the game's `Crash` box says "No mission data
  found in file for: <character>") and calls `LoadMissionFromFile("coop_mission.txt", <its player id>, 1, undefined)`
  (the call `CreateScenario` makes). It returned a new mission; saving that mission again on the guest gave text
  identical to the host's (45 of 45 lines).
- **Same ship from the same seed** (observed): a mission ship is generated when the mission is accepted
  (`AcceptMission(mission, 1)` -> `GenerateShip` -> `GenerateShipInSteps`, all in one call). The core gives a seeded ship
  its own 72-byte RNG state (seeded with `random_set_seed`, copied from `0x04b5c3c8`) and swaps it in around every
  `GenerateShipInSteps` call. Both copies loaded the same mission text and accepted it with ship seed 777: identical
  final RNG hash `0bd83e16`, 17x17 modules, 81 modules. A second round on both copies with seeds 777 and 778: 777 again
  gave `0bd83e16` / 81 modules, 778 gave `f3760070` / 87 modules.
- **Over the network** (observed, two headless copies on localhost): `share <mission> <seed>` on the host saves the
  mission (1152 bytes), sends it in 1 KB chunks with the seed (resent every 30 frames until acknowledged), and loads and
  accepts the same text itself. The guest received it, loaded and accepted it: both ships generated with RNG hash
  `7d88d413`, 17x17, 81 modules. Every copy shows "Mission shared with your crew".
- **Pods in space** (observed): `UndockFromStation()` (`0x00b33fb0`, normally called by `oStationDockLift.Step_0`)
  launches the player's pod; holding Thrust (control 5) with the cursor override flies it toward the cursor. In open
  space the pod's `x`/`y` equal `xReal`/`yReal` and change smoothly (no origin shift seen at about 1 million units).
  Galaxies differ between players, so each copy sends its pod's offset from its own instance of the shared mission
  ship, and places the remote player's puppet pod at the same offset from its own instance (writing `x`, `y`,
  `xReal`, `yReal`, `image_angle` after the pod's step; unlike characters aboard ships, this holds). Test: host flying,
  guest's puppet pod within about 200 units of the received offset at about 20 000 units/s (about one frame).
- **Boarding the shared ship** (observed): `DockWithAirlock(airlock id)` with `self` = the pod (`0x00ad4f90`, as
  `CreateScenario` uses it) docks at an airlock of the shared ship; the player can then walk aboard. Ship-relative to
  room coordinates: `GetX/GetY(Fore, Starboard, Parent)` (`0x00a24610` / `0x00a249c0`), checked against the player's
  own x/y (equal). The remote player's packet says whether they are aboard the shared ship; the local puppet boards its
  copy with `CopyTheLattersParentAndPosition(puppet, <an airlock of that ship>)` (`0x00853a40`: the first argument takes
  the second's Parent and Fore/Starboard; the second must itself be aboard, the ship itself makes the game's code fail
  and GameMaker opens its error dialog) and is then steered by `PlayerPathfindingTo(GetX/GetY(remote Fore, Starboard))`.
  Result: puppet aboard the guest's copy (Parent = that ship), at Fore 1393 / Starboard 3370 for the host's
  1408 / 3127. Likely held back by a door that opened for the host's player only (doors react to `global.Player`).
- Pod sharing question (owner, 2026-10-30): one shared pod would only simplify the flight, which already works; it would
  remove separate escapes (a core part of the game). Proposed instead: an optional "follow the host" autopilot.
- So the shared-mission plan holds: the host sends the mission text and a ship seed, every copy loads and accepts it
  with that seed, and only live state (guards, doors, loot, alarms) is synced afterwards.
- Headless test displays (2026-10-30): test copies now run in `gamescope --backend headless` (installed by the owner).
  Screenshots come from the game itself (`screen_save` builtin, written to the save folder), clicks are window messages
  inside the process. The loader answers message boxes automatically when `COOP_AUTO_DIALOGS=1` (set by `game.sh`).
- Strings and banner (observed): `ShowUpdate(text, 3)` from the DLL showed "Paused by your partner" on the host and
  "Paused by you" on the guest when the guest paused; both copies at `TimeScale` 0.
- GML strings from the DLL: `0x010ab350` is `YYCreateString(RValue* out,
  const char* text)` (cdecl, about 5600 call sites with literals); `0x00401790` frees an RValue's payload (the game calls
  it when `((kind + 0xffffff) & 0xfffffc) == 0`). `ShowUpdate(text, 3)` (`0x00844290`) is the game's on-screen update line.

## Two copies on one PC, UDP on localhost (probe, 2026-10-30)
Setup: copy B is a second game folder (hard links to copy A's files, its own `version.dll`, logs and command file) and a
second Proton prefix. Both ran at the same time under one Steam account.

Observed:
- Each copy picked a character; another bar character became the "puppet" for the remote player. A hosted on
  127.0.0.1:27715, B joined. About 60 packets per second each way, 0 bad packets (`netstat` counters).
- Writing a puppet's `x`/`y` with the runtime setter `0x01098cb0` (`(self, var_id, ARRAY_NONE, RValue*)`) holds only until
  the next frame: `gml_Script_ShipwiseEndBit` rebuilds `x`/`y` from `Fore`/`Starboard` (slots 0x262/0x263), the position
  relative to the ship or station the character is aboard. Found by hooking the setter and logging the caller's return
  address for writes to the puppet. So the network sends and applies `Fore`/`Starboard` (plus x/y).
- Puppet position kept snapping back between frames: to the local player's Fore/Starboard when the puppet's step ran
  un-impersonated, to its own bar seat (Fore 1085) when it ran impersonated. Skipping `MovePersonOutOfWalls` for the
  puppet did not change it. Hardware debug registers (DR0/DR7 via `SetThreadContext` or via an exception's context) are
  not honoured under Proton; a page-protection watch (read-only page + VEH + trap flag) works (self-test counted its own
  write) but saw no other write to the puppet's `Fore` in 2 s. Unexplained.
- Changed approach (untested when launches stalled again): move the puppet with the game's own pathfinding during its
  impersonated step (mouse override = remote position, `ControlPressed(11)` every 10 frames when more than 6 units away).

Blocker (unexplained, 2026-10-30 from about 14:40 local time):
- Every new launch of either test copy runs GML for one or two frames, then no more steps. The main thread is in the
  runner's `PeekMessage` loop (winedbg backtrace: `heat_signature+0xcf886b`), CPU about 1 %. The window shows a galaxy
  view. The same happens with the previous probe DLL, with no DLL, with freshly deleted saves, with `GetForegroundWindow`
  / `GetActiveWindow` / `GetFocus` hooked to the game window, with `WM_ACTIVATE*` posted every 0.5 s, and with vsync off
  (`DXVK_CONFIG="d3d9.presentInterval = 0"`, `MESA_VK_WSI_PRESENT_MODE=immediate`). Proton was not updated.
  The window was on screen. **Fixed by restarting the Steam client** (15:10): the next launch ran normally. Cause
  (derived): Steam held stale state after dozens of force-stopped launches. Launch only after Steam has fully started.
- **It came back** after about ten more launches (15:24). Same symptoms, also with `GamepadEnabled = 0` and with
  `PROTON_NO_NTSYNC=1`. Observed from a stall dump (main thread stack, taken from a helper thread in the DLL): the
  runner's main loop (`0x010f8e30`..`0x010f8e52`: `while (byte[0x0494d85c] == 1) { frame 0x011216e0; pump 0x010f8840 }`)
  keeps going, but the frame function's timer check (it starts with `QueryPerformanceCounter` via `0x01132c80`) never lets
  a frame run. Not resolved. Mitigation: launch rarely (hot-reload the mod's logic instead of restarting the game).
- Third occurrence (16:00, about four launches after a Steam restart). Narrowed down, all observed:
  - The runner's frame function `0x011216e0` is **not called at all** during the stall: the timing globals it writes
    first (`0x04947648`, "now") stay frozen between two dumps 5 s apart.
  - The main thread is inside the **first** `PeekMessageW` of the pump `0x010f8840` (return address `0x010f886b` on top
    of the stack, 20 of 20 samples at the same `win32u.dll` syscall return). Zero messages were dispatched
    (`DispatchMessageA/W` hooked), so it is not a message flood.
  - Linux side: the main thread sleeps in `do_select`; the prefix's wineserver idles in `do_epoll_wait`; game CPU ~4 %.
    Native backtrace not possible (`kernel.yama.ptrace_scope = 1`).
  - Ruled out: the probe/mod (also with no DLL), saves, gamepad option, focus, vsync, ntsync, the Steam overlay
    Vulkan layer (`DISABLE_VK_LAYER_VALVE_steam_overlay_1=1`, layer verified absent), Xalia (`PROTON_USE_XALIA=0`).
  - Derived (unverified): Wine's `PeekMessage` blocks while reading from the X connection; something Steam-side
    (restarting Steam clears it) leaves the X/driver path wedged after several force-stopped launches.
- Checked while investigating: Steam injects `STEAM_COMPAT_DATA_PATH=.../compatdata/268130` and the real install path
  into the environment of test copies, but they run with `WINEPREFIX` = the test prefixes, and no file in the owner's
  real save folder changed.

## Hot reload and working puppets (2026-10-30, after a Steam restart)
- The mod is now two DLLs: `version.dll`, a permanent loader (VERSION forwarding, keep-running-unfocused hooks, a
  watchdog that dumps the main thread's stack when frames stop, one per-frame hook on `oControls.Step_0`), and
  `coop_core.dll`, the reloadable core. The loader runs the hot-reload host from `tools/hotreload` (`CoreHost`,
  vendored). A swap is requested by creating `reload.txt` next to the exe (the control pipe is a Windows named pipe,
  which a Linux client cannot reach under Proton). The core uses the plain C ABI (`abi.h`): `before_unload` saves its
  state with `blob_put` (socket handle, peer, puppet and player instance pointers), takes every MinHook hook off
  (`MH_DisableHook(MH_ALL_HOOKS)`, `MH_Uninitialize`) and returns 1; `init` restores it with `blob_get`.
  Observed: generation 1 -> 2 in a running game, state restored (5 players), the game kept running and answering
  commands. A swap takes about 1 s; this replaced most game restarts (and so the stalls).
- **Puppets work** (observed, two copies, host A and guest B on localhost): the puppet's step is impersonated
  (`global.Player`, `global.PlayerShip`, `oCamera.Object` = the puppet; every control reads 0) and, every 10 frames while
  it is more than 6 units from the remote position, the core calls `gml_Script_PlayerPathfindingTo(x, y)`
  (`0x00a4e5f0`) for it. Result after moving both players: host's puppet of B at (-163, -41) for B at (-163, -40); guest's
  puppet of A at (160, -2) for A at (160, -1). The puppet walks with the normal animation and collides with walls.
- What did not work: writing `x`/`y` and `Fore`/`Starboard` directly (reverted each frame, cause still unknown);
  faking the cursor plus `ControlPressed(11)` only during the puppet's step (the route was never computed).

## Shared time (2026-10-30)
- Each copy's core holds back every `SetTimeScale(scale, priority[, extra])` call and keeps the highest-priority request
  of the frame. Once per frame (from the loader's per-frame hook) the host combines its request with the guest's:
  pause (scale 0) from anyone wins, else slow motion (scale < 1) from anyone, else fast-forward (scale > 1) only when
  both ask, else nothing. The host sends the result and who paused; each copy then calls the original `SetTimeScale`
  once with that result.
- Test (two copies, bot input, `global.TimeScale` read on both after 1.5 s), all observed:

| Step | Host | Guest |
|---|---|---|
| guest presses Inventory (`ControlPressed(4)`) | 0 | 0 |
| guest presses it again | 1 | 1 |
| host presses Inventory | 0 | 0 |
| host holds Fast Mo (31) alone | 1 | 1 |
| both hold Fast Mo | 6 | 6 |
| guest holds Slow Mo (30) | 0.2 | 0.2 |
| everything released | 1 | 1 |

- Not done: the "paused by" banner (needs a way to create GML strings from the DLL), and "anyone can unpause" (today a
  pause lasts while the pauser's inventory is open; unpausing someone else's pause would mean closing their inventory).

## Unknown
- How the Nexus mod loader works and whether it can host networking code.
- Whether UndertaleModTool reads a GMS1 data chunk that is embedded in the exe.
