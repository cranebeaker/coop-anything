<<<<<<< games/heat-signature/NOTES.md
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
  remove separate escapes (a core part of the game). Owner chose an optional "follow the host" instead.
- **"Follow the host" for guests' pods** (observed): the game's `AutopilotTo` does nothing for player pods (only
  `oShip.Step_0` runs `AutopilotStep`; for the pod, `Autopilot` only changes the thrust display). Steering by cursor:
  the pod thrusts toward the **`oCursor` instance** (`PlayerThrustTowardsCursor` reads `oCursor.x/y`, which
  `oCursor.Step_1` copies from the mouse), not `mouse_x`; the cursor override now also moves `oCursor`. In open space
  the player's own pod is the coordinate origin (stays at about (0, 0), the world moves). Steering a pod that keeps its
  momentum toward a target millions of units away overshot, so the follow mode carries the guest's pod instead: up to
  1500 units per frame toward a point 1500 units behind the host's pod, own `xSpeed`/`ySpeed` (0x18b/0x18d) zeroed.
  Result: closed 3.2 million units at about 79 000 units/s, then trailed 1900 units behind; when the host docked at
  the shared ship, the guest's pod went there and docked by itself ("Docked with your crew's ship").
- **Doors** (observed): both copies list the same 17 active doors of the shared ship at the same `Fore`/`Starboard`.
  A door's state is `Open` (0x438), `OpenSignal`, `Security` (0x374, lock level), `SecondsUntilClosing` (0x43a);
  `OpenDoor(door)` / `CloseDoor(door)` (`0x00602830` / `0x006017b0`) toggle it. The core sends door messages every
  15 frames: the host its door states with the seconds left, a guest the doors it opened itself; the receiver uses
  `OpenDoor`/`CloseDoor`. Messages flowed (guest applied host states 9 times, host opened 4 doors for the guest), but a
  door opened with nobody in it closes again within a few frames, usually before the next message. Walkers open doors
  themselves through `AutoOpenDoors` (called from `PlayerStateFree`, i.e. inside the puppet's impersonated step, and from
  the guards' states), which checks the door's `Security` against `SecurityLevel` (keycards). Derived: puppets already
  open ordinary doors locally; keycard doors need the remote player's clearance, which a puppet does not have yet.
  Door messages matter for lasting state (locks, hacks, crashed doors), not for walking through.
- **Guards** (observed): guards carry `OriginalFore`/`OriginalStarboard` (0x4ff/0x500), their creation position, which
  is identical on every copy of the shared ship, so it identifies a guard across copies. The host sends every 6 frames
  each active guard of the shared ship: original position, `Fore`/`Starboard`, `MyImageAngle` (0x1b6), `Conscious`
  (0x4c7), `Dead` (0x307), `State` text. On a guest the census hook on `oEnemy.Step_0` skips the AI of a guard the host
  reported in the last 2 s and writes those values (x/y via `GetX/GetY`). Result: 38 of 38 guards matched; a patrolling
  guard was at (3456.0, 3456.0) on both copies at the same sample, sentries identical to 0.1 unit. Not mirrored yet:
  the `State` label and animation (the guest's skipped AI keeps its own label), guests' hits/knockouts going to the
  host, host guards seeing guest puppets (`EnemyCanSeePlayer` uses `global.Player`), keycard clearance for puppets.
- **Host guards notice guests** (observed, first test): on the host, each shared-ship guard's step runs with
  `global.Player` = the nearest player aboard the same ship (the host's own player or a guest's puppet). After the guest
  walked toward guards, the host had 2 guards "Panicking" and 2 "Searching" (none in earlier runs), and the host's game
  ran a pause, which the shared time sent to the guest ("Paused by the host"). Correction (2026-11-01): it was not a
  "spotted" pause. In this build no spotting changes time (see "Banners with names" below); which pause it was is unknown.
- **Guests' hits reach the host** (observed): a hit is put on its target with `SetDamageSignal(target id, damage mask)`
  (`0x005d0050`, sets `DamageSignal` 0x400 / `DamageMask` 0x401 inside `with(target)`); the guard handles it in its step
  (`EnemyHandleDamageSignal`). On a guest the hooked function also sends the hit (guard's original position + mask,
  3 copies with an event id); the host calls `SetDamageSignal` on its guard. Test (`hitguard 0 1` on the guest): event 1
  sent, applied on the host, and the host's guard went to state "Down". Not yet visible on the guest: the mirrored
  guard keeps its own `State` and animation (its AI step is skipped), so the knockdown must still be mirrored as a state.
- **Knockouts visible on guests** (observed): mirrored guards now run their own step (for animations) blind, with
  `EnemyCanSeePlayer` hooked to answer "no" during it, after taking the host's `State` via `ChangeStateTo` when that
  state needs no extra data. Forcing other states fails: "Responding" raised a GameMaker runtime error in
  `EnemyStateResponding` line 66 (found by walking the YYC GML call stack from the loader's dialog hook: each GML
  function links `{previous, name, line}` into a list headed at `0x049356e8`). Running guards with `global.Player =
  noone` failed the same way. Allowed: Down, Dead, Sentry, Static, Idle, PatrolPath, Patrolling. Test (`hitguard` on the
  guest after `board.sh`): exactly one "Down" guard on host and guest; host guards then went "Fetching Body", "Leaving
  Body", "Searching" (kept local on the guest, positions mirrored); no error dialogs on either copy.
- **Puppet contents list**: a ship keeps a contents list; boarding the puppet with `CopyTheLattersParentAndPosition`
  alone left it off the list, and an airlock forcefield ejecting it raised the game's "Trying to remove a oPlayer
  from contents list, but couldn't find it in there". Boarding now calls `RemoveFromParentsContents(puppet)`
  (`0x008606c0`), the copy, then `AddToParentsContents(puppet)` (`0x00852570`).
- `local/scripts/board.sh` (test harness): starts both copies, shares a mission, undocks, moves and docks both pods,
  checking each step and retrying (3 tries) before failing with the step's name.
- Test-sequence note: in that run the host's `undock` did not take (`gotoship`: "need my pod"), so the host stayed at
  the station. The bot steps need checks (pod found, state "Controlling") before going on.
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

## Keycards and hits on puppets (2026-10-31)

- **Clearance** (observed in the decompile): `AutoOpenDoors` checks a door with `Security` > 0 only for the walker that
  is `global.Player` (the puppet, inside its impersonated step): `SecurityLevel(walker, door.OriginalShip)` (`0x008bcc80`)
  must reach `door.Security`. `SecurityLevel` walks the person's `PassiveInventory` for keycards; a puppet has none.
  Pathfinding never asks it, so a puppet routes through a keycard door and then stops there. Callers: `AutoOpenDoors`,
  `oAirlock.Step/Draw`, `oDoor.Draw`, `oTerminal.Step/Draw`, `oKeyCloner.Step`, `InteractWith`, `EnemyStateDown`.
  `GiveThisPersonAKeycard(person, level)` (`0x00a59a60`) makes a keycard for the person's current `Parent` ship.
- **Fix:** every player packet carries the sender's `SecurityLevel(own player, shared ship)` (from the game, every 15
  frames); a `SecurityLevel` hook answers `(puppet, shared ship)` with that number. Observed: host given a level-3 card,
  the guest's copy answered 3 for the host's puppet (0 before). (Correction, see the next section: the bot can walk the
  players; the targets tried here were outside the ship.)
- **Hits on puppets** (observed, the cause of "Personal mission failed" right after docking in every run): the host's
  guards attack the guest's puppet (by design, they see guests). The hit lands on the puppet's own `DamageSignal`
  (0x400) and `PlayerHandleDamageSignal` (`0x00ab90f0`, from `PlayerStateFree` line 71) handles it inside the puppet's
  step, where `global.Player` is the puppet; when the puppet went down, the host's game took it as its player knocked
  out: "UNCONSCIOUS", injuries 5, mission failed, while the guest on its own copy was unhurt. No `SetDamageSignal` call
  targeted the host's player at all. Player attacks carry the damage mask as an int64 RValue (kind 10).
- **Fix:** the `PlayerHandleDamageSignal` hook clears a puppet's signal and sends the hit (mask kind and 8 value bytes)
  to the puppet's player, whose copy applies `SetDamageSignal(own player, mask)`. Observed: 44 guard hits on the guest's
  puppet in 60 s, 44 applied to the guest's player, host injuries 0. The guard-hit forwarding now also keeps int64 masks
  (it sent `undefined` for them before).
- **Seen in the same runs, not fixed:** an alarm on the host's copy made the mission target escape and failed the
  host's mission while the guest's copy still counted down (alarms are not synced). While a player stands inside their
  own docked pod (fore/starboard about (1408, 3127)), their puppet on the other copy waits in the airlock at about
  (1420, 3365), in the guards' view; that is what drew the guards in these tests.
- **Likely explanation of a mutation-run oddity** (derived): with puppet hits not forwarded (`nopuppethits`), the host's
  mission fails and its failure screen holds `TimeScale` at 0, so every later time check reads 0 on both copies, the same
  pattern as the earlier `nohits` run.

## Alarms, the target's escape, parking, and why time stuck (2026-10-31)

- **Alarm** (observed in the decompile): `TriggerShipsAlarm(ship)` (`0x00cac330`, from `CanTriggerAlarm`, security cameras,
  `DestroyModule`) sets the ship's `Alarm` (0x56b), counts the mission alarm, starts the escape protocol and makes the
  mission target flee. It does nothing unless the ship's `Bridge` (0x7c4) exists and is `Manned` (0x3bf, kept by
  `oControlModules.Step_0`): knocking out the guard flying the ship (state `"Controlling"`) disables alarms. The host's
  game calls it every frame while an alarm is on.
- **Sync:** host authority. A guest's own trigger on the shared ship is suppressed and sent as a request in its player
  packets; the host sounds it with the game's function; every host packet carries "alarm on" and a guest sounds it the
  same way (with its bridge counted as manned for that call: the host's copy decided). Observed: guest-side trigger ->
  host sounded it 33 ms later -> guest 17 ms after that.
- **Escape countdown** (observed): "N seconds until target escapes" is the target guard's `EscapeSecondsLeft` (0x504, set
  in `EnemyStateEscaping` from `EnemyGetPathTimeRemaining`), not the ship's `AlarmSecondsLeft` (read -1). On a guest the
  mirrored target's countdown stood still (14.44 while the host's went 12.5 -> 7.1): its position comes from the host, so
  it never progresses. Guard packets now carry it; observed within 0.4 s of the host's.
- **Escape** (observed in the decompile): on arrival `EnemyStateEscaping` reads the teleporter's 0x507 and either sets the
  guard's `DestroyMe` (0x2e3) or calls `TeleportRandomlyOutsideShip(guard)` (`0x00869430`). A guest's mirrored target never
  arrives (its countdown restarted at 14.08 after reaching 0). The host's `EnemyStateEscaping` hook notices a shared-ship
  guard leaving and sends an event; a guest applies the same outcome. Observed: host "destroyed", guest 16 ms later;
  both copies show "Personal mission failed / Objective lost", mission time 0:45 on both (witness counts differ: 4 on the
  host, 0 on the guest; witnesses are counted where the guards see).
- **Parking** (observed): both pods dock at the same airlock in their own copies, so a player standing in their docked pod
  (about 15 units from the pod's centre) is, on the other copy, inside that copy's own pod; the puppet waited in the
  airlock and the host's guards saw it, attacked it and sounded alarms within 30-60 s. Each player packet now says "in my
  own docked pod" (player-to-pod distance < 150, both read in the same frame: right after docking the ship moves about
  15 000 units/s and a position from earlier in the frame was off by more); the other copy hides that puppet, does not
  walk it, and the host's guards do not target it. Observed: no alarm and no hits in 60 s (38 hits and an alarm before).
- **Guest hits** (fixed): a guest's hit on a shared guard was applied on the guest's copy as well as forwarded; when the
  host's guard was not knocked out (shield, angle), the guest's copy kept it down. Now only the host applies it, with
  the guest's puppet as the attacker (shields and angles are judged from the attacker; it was the host's player).
  `MeleeAttack` itself refuses shielded guards (`GuardHasShield` `0x006fa070`, `GuardShieldHacked`) before any hit.
- **Time stuck at 0** (observed, explains the earlier mutation-run oddity): `oTime.Step_0` pauses at `MenuPriority` (9)
  whenever `oMenu.GameRunning` (0x251) is 0, i.e. a menu screen is open; the stuck runs had `oMenu.Mode` = "Mission End
  Screen" on both copies: the suite's own knockout had alerted guards, an alarm followed and the target escaped during
  the time checks. The suite now runs the time checks before anything that alerts the ship.
- **Start-up flake** (observed): sometimes a copy never took its character (`global.Player` stayed 49), so it had no pod;
  `two.sh` now checks `SelectThisCharacter` ran and clicks again.

## Walking through a keycard door, and guards on guests that acted by themselves (2026-10-31)

- **Bot walking** (observed; corrects the note above): `PlayerPathfindingTo(x, y)` on the controlled player works when
  the target is reachable. It always returns 0; it stores `DestinationFore/Starboard` (0x523/0x524) and calls
  `SetPathToShipwisePoint`, which puts a path in `MyPath` (0x3e5), or -4 when it finds none. `PlayerMovementControls`
  then calls `MoveTowardsDestination` while `path_exists(MyPath)`, and `EndRoute` when a movement key is pressed. The
  earlier targets were outside the hull (the docked player stands in the pod, outside the airlock): `MyPath` -4, no move.
- **Keycard door** (observed, ship seed 4242): door 4 at (2544, 3456) has `Security` 2. The guest, holding a level-1
  card, walked from its pod toward fore 2650: it stopped at fore 2518 on its copy, and its puppet on the host's copy
  stopped at 2517. With a level-2 card both went through (2650 and 2649). With the `noclearance` mutation the host's
  puppet stopped at the first keycard door already (door 3, `Security` 1, fore 1749) while the guest walked on.
- **Guests' copies of guards acted by themselves** (observed, fixed):
  - The guest's copy of a guard still ran parts of its AI. During its mirrored step it is blind to the local player,
    but after the suite's guard hit it went to "Responding", and when the host has not reported it lately its ordinary
    step runs ("FetchingBody"). Both reached `CanTriggerAlarm` -> `TriggerShipsAlarm`, which became a request to the
    host, and the host sounded the alarm (stacks: `CanTriggerAlarm:9 < EnemyStateResponding:61 < oEnemy_Step_0:82`,
    `< EnemyStateFetchingBody:30`).
  - Fix: on a guest, an alarm, a damage signal, a gun's bullet or grenade, or a thrown item is dropped when a
    shared-ship guard causes it (its mirrored step, or the guard is `self`). The host's copy of the guard does these
    for real, and its hits on a guest's puppet already go to the guest. Observed: 1600-6500 such alarms dropped per
    suite run, and the guest's copy asked the host for none; with the `mirroralarm` mutation it asked.
  - Not seen yet: a guest-side guard's own shot or throw (counted, 0 so far).
- **Sentry guns are not synced** (observed): `oSentryGun_Step_0` -> `SentryTriesToShootAt` ->
  `SentryFiresBulletInDirection` runs on every copy and shoots that copy's own player (`global.Player`). The guest's
  sentry shot the guest walking past door 4. Its bullet also hit a guard, and that hit went to the host as if the guest
  had made it. The host's sentry ignores the guest's puppet. To do: sentries decided by the host, like guards. The suite
  makes the guest's sentries hold fire for its walk (`nosentry`, test only).
- **The host's own alarm** (observed, the game's rule): the suite's guard hit once killed a guard (`KillEnemy` from
  `EnemyHandleDamageSignal`), which dropped a grenade (`CreateContentsOnTopOf`). It exploded
  (`AlertAllGuardsInShipwiseRadius < ExplodeOnShip`), and a guard reacting to the body (`ReactToBodies`) sounded the
  alarm. The suite no longer requires "no alarm at all" before its own alarm, only that the guest's copy asked for none.
- **Who fires** (diagnostic): the core logs the GML stack when a bullet or grenade is created (`oBullet_Create_0`
  `0x00ee1c50`, `oGrenade_Create_0` `0x00f53a40`) and when a guard gets `SetRespondingSignal` (`0x00a6d7f0`) or
  `ChangeStateToResponding` (`0x00a67370`).

## Sentries, the mission's success and witnesses (2026-10-31)

- **Sentries decided by the host** (observed in the decompile, then tested): `oSentryGun_Step_0` calls
  `SentryTriesToShootAt(global.Player, SentryDamageMask)` (`0x00866c20`), which checks angle, stealth shield and line of
  sight, fires (`SentryFiresBulletInDirection`) and asks for the spotted slow-down (`SetTimeScale`).
  - Host: a hook calls it a second time with the guest's puppet as the target (not when the puppet is parked).
  - Guest: its own shared-ship sentries' calls are dropped.
  - Observed: the host's sentry fired 3-8 shots at the puppet, the guest's injuries went 0 -> 1 or 2, and the guest's copy
    dropped about 6500 tries of its own per suite run.
  - The guest did not see these bullets at first (see the next section).
- **Mission success** (observed): the mission type here is "Assassinate".
  - The target's own step (`EnemyCheckObjectiveDelivered`) calls `CompleteAssassination`, which sets the mission's
    `AssassinationCompleted` (0x788) and clears `global.Objective` to `oNoone` (object index 49). The game then shows
    "Return to pod".
  - The mission itself completes later: `CheckIfAssassinationMissionCompleted` (from `PlayerStateFree`) calls
    `CompleteMission` once the player is aboard a station, i.e. on each player's own copy when they fly home.
  - Observed (`tools/mission.sh`): the guest's hit on the unaware target (forwarded, applied by the host) killed it.
    `AssassinationCompleted` became 1 on the host, and on the guest's copy too, whose target is the host's dead one
    mirrored. There was no `FailMission` on either copy.
  - Not tested: docking at a station. A direct `DockAtStation(home station)` from the pod closed the game, as did
    `KillEnemy` called with the killer as its argument (its argument is the victim: `with (argument0)`).
- **Witnesses** (observed): the end screen's numbers are fields of the player instance: `Kills` 0x2c2, `Knockouts` 0x2c3,
  `TimesInjured` 0x2c4, `Alarms` 0x2c9, `LivingWitnesses` 0x2ca, `TimesSeen` 0x2cb.
  - `IncreaseMissionLivingWitnesses` (`0x009e8ff0`, from `EnemyBecomeWitness`) adds to `global.Player`. On the host,
    during a guard's step aimed at a guest, that is the guest's puppet, so the host counted the guest's witnesses on the
    puppet (3), while the guest's copy counted its own (1).
  - The guest's copies of guards still saw the guest through `SeeingPlayersBody` (`0x00a6b5c0`), a second vision path
    besides `EnemyCanSeePlayer`: `EnemyBecomeWitness:6 < SeeingPlayersBody:27 < EnemyStateErrand:39`. It now answers "no"
    for them (about 37 000-74 000 calls per run).
  - Every host packet carries what the host counted on the guest's puppet since the shared mission was accepted (kills,
    knockouts, living witnesses, times seen). The guest's copy shows its own baseline from that moment plus those.
  - Observed: host saw the puppet 3-6 times; the guest's copy showed the same, and counted none itself. Alarms stay per
    copy (both sound the same alarms); injuries per player.
  - A third path (observed in a mutation run): a guard's ordinary step on the guest's copy (not mirrored at that moment),
    Chasing or Confronting, recognized the guest: `EnemyBecomeWitness:6 < UpdateRecognizingPlayer:16 <
    EnemyStateChasing:31`. On a guest, `IncreaseMissionLivingWitnesses` caused by a shared-ship guard is now dropped
    outright.
- **Heartbeat** (observed, test tooling): the loader writes `heartbeat.txt` every 5 s, so a stall check must wait longer
  than that; `mission.sh` waited 4 s and reported a healthy host as stalled.

## Bullets on every copy (2026-10-31)

- **How a bullet flies** (observed in the decompile): guns call `GunFiresBulletInDirection(gun, direction)`
  (`0x007e5c60`, from `oGun_Step_0`), which creates an `oBullet` (object index 57) with `CreateInstance(x, y, object)`
  (`0x00db4370`) and copies values from the gun. Aboard a ship, `oBullet_Step_0` (`0x00ee2650`) moves it ship-wise:
  `ForeSpeed`/`StarboardSpeed` = `BulletSpeed` along `MyImageAngle`. A hit is `SetDamageSignal` from the bullet's step.
- **Sync**: each copy sends, once, every real bullet created aboard the shared ship, read at the start of the next frame
  (its gun sets heading and speed after `Create`): object, sprite, fore/starboard, `MyImageAngle`, `BulletSpeed`,
  `FirstStep`, the firer (the sender's player or a shared guard) and five values the step reads on a hit. The other copy
  creates the same object, puts it aboard its copy of the ship (`CopyTheLattersParentAndPosition` + `AddToParentsContents`),
  sets those values and lets it fly. During its step its `SetDamageSignal` calls are dropped: the real bullet's hit
  already reaches the player as a hit message.
- **Error found**: the first version copied only position, heading and speed; the look-alike's first hit raised a
  GameMaker error (`GetDamageFlag` line 5, from `oBullet_Step_0` line 186) because its damage mask was undefined. Copying
  `WeaponDamageMask` (0x641), `BulletDamageMask` (0x642), `BulletKnockbackSpeed` (0x643), `KnockbackSpeed` (0x644) and
  `NormalBulletSpeed` (0x646) fixed it.
- **Observed**: the host sent 28-81 bullets per suite run (sentries and guards), the guest showed all of them, none
  failed, and their hits were dropped (53 in one run). No hit on the guest's player came from its own copy.
- **Not done**: grenades. A grenade is an inventory item (`oGrenade`, thrown with `ThrowInventoryItem`), and
  `ExplodeOnShip` can breach the hull, which is not synced, so a look-alike would breach only one copy. A guest's own
  gunshots go through the same code but are not tested (the test bot cannot fire a gun).

## Reinforcements and jammers after an alarm (2026-10-31)

- **What an alarm does** (observed in the decompile): `TriggerShipsAlarm` sets the ship's `Alarm` (0x56b), counts it,
  and with `EscapeProtocol` (0x74b) makes the mission target flee. A ship with `ReinforcementProtocol` (0x796, from the
  mission text) has an `oTeleporterModule`; its step calls `TeleportInReinforcementsAtIntervals` (`0x00ca8c90`), which
  now and then calls `CreateMercenaryOnShip(parent, fore, starboard, type)` (`0x00709b70`): a random mercenary (name,
  type, kit) at the teleporter.
- **Test ship** (observed): the default test mission had `ReinforcementProtocol = 0`, so its alarm brought no
  reinforcements. The test galaxy is new every run, so the security scenario changes the shared mission's text (base64
  lines `Key = value`) to `ReinforcementProtocol = 2` and `EscapeProtocol = 0` before sharing; both copies build the
  ship from that text.
- **Before the fix** (observed): both copies beamed in their own guards (38 -> 46 in 85 s on each), with different
  names. All reinforcements share an original position (the teleporter's), the key guests use to match guards to the
  host's, so on the guest every one of them followed the same host guard.
- **Fix**: each call runs on a fixed random stream (seed from the reinforcement's number; the game's RNG state saved and
  restored around it). A guest's own calls are dropped (about 4 200 per run); when the host reports reinforcement n, the
  guest sets its teleporter's `SecondsUntilReinforcements` (0x9bb) to 0 and calls it once. Both copies add 0.01 * n to the
  new guard's `OriginalStarboard`; guard matching now uses 0.004 instead of 1. The game sets `OriginalFore/Starboard`
  a few frames after the guard appears (-1 first), so the tag is re-applied when the game changes it.
  Observed: the same names and types in the same order on both copies (e.g. "Defender Cherry" 4, "Jammer Jones" 2,
  "Tracker Quintana" 3).
- **Jammers** (observed): Jammer mercenaries place `oJammerDevice` (object 64) items (`EnemyStatePlacingJammers` ->
  `CreateContentsOnTopOf`); `PositionIsScrambled(fore, starboard, ship, 1)` (`0x0070eb80`, 1 = a player, 2 = guards and
  sentries) checks them, and `UpdateScrambled` stores the result in the player's `Scrambled` (0x855). The guest's copies
  of the guards placed none (host 7, guest 0). Now the host sends its devices' positions every 15 frames; the guest
  creates `oJammerDevice` there (`CreateInstance`, then aboard the ship like a bullet), adopts or removes any other, and
  when one of its copies is destroyed it asks the host to destroy the real one (`DestroyMe`).
  Observed: the same devices on both copies and the same answer from `PositionIsScrambled` at every device; a new
  device jams at once.
- **Test tooling** (observed): the game's log lines end in `\r`; a check comparing a value with `= 1` failed until the
  `\r` was stripped.
- **Not done**: Trackers' `RebootPredatorSensor`, `SecurityProtocol` and `AlarmTime` ships; the guest's player was
  knocked out by the host's guards while walking during the alarm (not investigated).

## Guards' gear, loot and items (2026-10-31)

- **Gear differed between copies** (observed): a guard gets its kit and gun in its first step (`GiveGuardEquipment`,
  `0x006f53b0`, from `oEnemy_Step_0`), from the game's random numbers at that moment; one guard was armed differently
  on the two copies, and the mission target got a different extra item at `AcceptMission` (line 86). A guard that goes
  down drops its gear (`EnemyDropLoot`, `0x00693f60`), so drops would differ too.
- **Fix**: both scripts run on a random stream seeded from the guard's original position (the game's random state saved
  and restored; both scripts end with `randomize()` anyway), and the shared mission's accept on one seeded from the
  ship seed. Observed: all 38 guards' inventories identical on both copies (`guardlist`), and the target's 2 drops at
  the same places on both after the guest's kill.
- **Side effect found by the kill test** (observed): with fixed gear this ship's target (the mission's boss, whose kit in
  the mission text lists a shield) has a shield (`GuardHasShield` 1), which stopped the guest's punch; before, the boss
  kit was random and often had none. Correct game behaviour; the kill test uses another draw of the gear (test helper
  `GEAR_SALT=1`), where the target has no shield.
- **Ship contents** (observed): a ship keeps `Contents` (0x32f, an array) and `ContentsCount` (0x32e); the runtime's
  get-variable-by-id functions (`0x01099410` instance variables, `0x01099210` built-ins, with an array index) read it.
  The test ship holds 17 doors, 35 terminals, 4 fuel barrels, 3 loot boxes (and lids), 1 money, 1 ammo, people and AI
  markers (`oErrandBuddyPosition`, which differ between copies and are left out of comparisons).
- **Taking things** (observed in the decompile): items go into an inventory with `AddToInventory(item, person)`
  (`0x00872040`; by hand or `PickUpLootAutomatically` when walking over); money and other loot are used (`UpdateUseHandle`)
  and then `CollectLoot` (`0x00a72330`) runs. Now whatever the local player takes off the shared ship is sent (object,
  ship-wise position, 3 times) and the other copy removes the same object with `DestroyContents(id)` (`0x00c3a220`). The
  host's puppet never picks anything up. Observed: the guest took one of the target's drops and the money; the host
  removed both, and the contents matched (75 and 75).
- **Test tooling**: suite check 7 counted a hit on the guest as an injury; with more gunfire since the gear change the
  guest was already knocked out (`Unconscious`, no more injuries counted), so the check now also accepts the hit
  arriving at a knocked-out guest.
- **Loot boxes** (observed): `oLootBox_Step_0` (`0x00f03740`) opens a box when `Activate` (0x377) is set: `Open` (0x438),
  then `MyLoot` (0xb96, 0 before) = `CreateRandomItemOfRarity`, put on top of the box, then `ShowInventoryMenu`
  (`0x009372e0`) for the opener, all in one step. Both copies opening the same box in the same step with the same random
  state (hash of the 72-byte state identical) made different items (e.g. "Long Range Glitch Trap" and "Silenced
  Quickfire Gun"), so the item pool depends on more than the random numbers (per-player unlocks: a guess, unverified).
  Sync: a box that opens here is sent; the other copy sets `Activate` on the same box, lets it open with
  `ShowInventoryMenu` dropped, and removes the item it made (`DestroyContents`): the item is the opener's only.
  Observed: 3 runs, the host opened the guest's box, no menu, no item on the host's box, the guest took its item.
- **Not done**: the box's item for both players, keyed boxes, what a puppet holds; the pickups are tested with the
  test helper `takeitem`, which calls `AddToInventory` (or sends and removes loot as `CollectLoot` would), not by walking.

## Items dropped and thrown (2026-10-31)

- **Where** (decompiled): `DropInventoryItem`, `DropTrap` and `ThrowInventoryItem` (`0x008cafb0`) all call
  `EjectFromInventory` (`0x0088bf00`, self = the person, argument = the item id): the item leaves the owner's inventory
  and is put aboard the owner's ship at the owner's feet; a throw then sets `ForeSpeed` (0x37d) and `StarboardSpeed`
  (0x380). Aboard a ship things move ship-wise; `HorizontalSpeed`/`VerticalSpeed` (0x54f/0x550) are not speeds there
  (observed: 4287, -11207 on a dropped item at rest). The throw's direction comes from the thrower, not from the target
  arguments (observed: three targets in different directions, same direction each time); unknown which variable.
- **Item variables** (observed): items have no flat `yyvars` array; their variables live in a hash map at instance
  `+0x60` (the runtime's `GetYYVarRef`, `0x010b6960`: capacity at `+0`, elements at `+0x10`, 12 bytes each
  `{slot, RValue *, hash}`, hash 0 = empty). A Sidewinder had 157 set: 128 numbers, 24 texts (`Name`, `BaseName`,
  `Grade`, `ChecksumString`...), 3 lists (`0x298` = the trait names, read by `DrawDescriptionAndTraitsForItem`;
  `TraitTypes`, `TraitChances`) and 2 instance ids (`Parent`, `UseHandle`). A list is `{refs, row array, owner, visited,
  rows}`, a row `{length, RValue *}`.
- **Instances by id** (disassembly of `0x01098ea0`): buckets at `[0x048df1b0]`, mask at `[0x048df1b4]`, 8 bytes per
  bucket, nodes `{?, next, id, instance}`.
- **Sync**: when my player's item lands on the shared ship, the next frame (after a throw has set its speed) the core
  sends every variable set to a number, text or one-row list, except the place, `Parent`, `Owner`, `DestroyMe` and any
  value that looks like an instance id (an integer from 100000 to 2e7); about 2 KB. The other copy creates the object
  there (`CreateInstance`, then into the ship's contents) and sets them. A thrown item is watched until it has been
  still for 20 frames (a bounce off a guard stopped one for a frame and it moved on: 5 units apart on the first try);
  then its resting place is sent and the copy is put there. A shown item in flight does not impact anyone: the game calls
  `InventoryItemImpactsObject` (`0x00894e30`, which can call `SetDamageSignal` and distract guards with the bounce sound)
  from the item's step for each nearby person; for a shown item in flight it is dropped, so the thrower's copy alone
  decides a hit (a guest's hit already reaches the host).
- **Matching**: a taken item is looked up by object and place; the tolerance went from 3 to 24 units for thrown items.
- **Event numbers** for taken items, boxes and drops now start from the clock: after a hot reload of the sender's core
  they started again at 1 and the receiver ignored them as already seen.
- **Observed** (items scenario): a Grenade Launcher the guest dropped appeared on the host with the same name and the
  same 171 variables; the host took it (gone on the guest), threw it 32 units; both copies had it at the same place; the
  guest took it back under the same name.
- **Not done**: a thrown item's hit on a guard is not tested (no throw in the tests did damage, so the `ghostimpacts`
  switch has no check that can fail); a guest's thrown item does not distract the host's guards (its bounce sound plays
  only on the guest's copy); armed traps not tested (`Armed` is copied, so a shown trap may go off on both copies: unknown); an item that leaves
  the shared ship (thrown out of an airlock) is not followed; items dropped in space or on another ship.


## Banners with names (2026-11-01)

- **No spotted slow-down in this build** (decompiled): guards chasing or confronting a player and sentries shooting at
  one call `SetTimeScale(oTime.SlowMoScale, oTime.EnemyTargetPriority, 1)`; `oTime_Create_0` sets
  `EnemyTargetPriority` (0x3e9) to 0 and nothing else writes it, so the request never applies (observed: about 10000 such
  calls at 0.2/0 in one suite run, time unchanged). The game itself shows nothing when a player is spotted.
- **Spotted banner** (new, the tester's request): a sighting adds to the player's `TimesSeen` (0x2cb). The host's guards
  decide sightings (a guest's copies see nobody), so the host watches `TimesSeen` of its player and of the puppet and
  sends both counts in its player packet; each copy shows "You were spotted" or "<name> was spotted" when a count grows,
  at most once per 10 s per player.
- **Names**: each player packet carries the sender's character `Name` (0x2a1); pause banners read "You paused the game"
  or "<name> paused the game" ("Your partner"/"The host" until a name arrives). Steam names can replace character names
  once lobbies exist.
- **Observed**: suite check 4d3, the guest's pause showed "You paused the game" on the guest and "<guest's character>
  paused the game" on the host; the host's sentry spotting the guest showed "You were spotted" on the guest and
  "Tyeesha Prophecy was spotted" on the host.


## Leaving the ship and flying home (2026-11-01)

- **How a player leaves** (observed): standing in its docked pod, Use (`InteractWith(<the pod>)`) seats the player
  (state "Controlling") and the pod undocks. Flying home ends at a station: `oShip_Step_0` calls `DockAtStation` when
  the pod reaches it, which calls `CompleteMission` (or `FailMission`) on that copy; the mission end screen opens.
  Test helper `gotohome <distance>` puts the pod that far from `global.HomeStation`, stopped (a pod just off a ship
  kept its speed and flew past), instead of the flight. Observed: pod 100 units from the station, docked within 5 s,
  `Completed` 1, end screen open. (A direct `DockAtStation` call had closed the game earlier.)
- **Three bugs found and fixed** (observed in this order):
  1. A guest who flew off left its puppet standing in the host's airlock. The host's guards shot it, and those hits
     reached the guest in its pod and knocked it out into space (state "Floating", injured 1 -> 5). Now a puppet whose
     player has been aboard the shared ship and is no longer is held there, hidden, its step skipped (its own step had
     walked it after its player into the station).
  2. The guest's end screen is a menu, which pauses the game; shared time made it pause the host, still aboard.
     A copy's own mission end screen is no longer shared: it pauses only that copy, with no banner.
  3. The host could not dock at home after the guest had: on the host's copy the guest's puppet had walked after its
     player into the station (Parent = the station) and the puppet pod sat at the station's centre; the host's pod was
     pushed away (drifted 38 000 units) and never got a docking opportunity. Fix 1 (the puppet held, its step skipped)
     ended this: the host docked in every later run. A second fix that also moved the puppet pod far away made no
     difference when switched off (mutation `nostow`: 9/9), so it was removed.
- **Docking timing** (observed): twice in 5 runs the guest's pod, put 100 units from the station, had not docked 6 s
  later; put there again it docked (the station may not be ready right after the jump; unknown). The test retries every
  5 s for up to 20 s. *Superseded, see "Docking at home, corrected" below.*

## Docking at home, corrected; banners for the other player; leaving early (2026-11-01)

- **Docking at home failed in about half the runs** (observed: host in 3 of 4 runs, guest in 1, after 20 s of retries).
  Investigation (observed unless marked):
  - The bot's pod was pushed west at exactly 450 units/s right after `gotohome 100`. The station's `Radius` is 3976,
    so 100 (and the old default 3000) is inside its hull; placed 1500 out and drifting in, the pod bounced off about
    600 units from the centre. Not a docking path either way.
  - `PlayerShipIsAllowedToDockAtStation` (0x00af6320, hooked and logged) was never called. The flying state's dock
    check uses the pod's `NearestStation` (0x83b), which named a station of another faction 820 000 units away; the
    home station, about 30 000 units away, was not a candidate.
  - `ControlSomething` finds the nearest station from the **player's** x/y, and oGalaxy activates the galaxy around
    the camera's sector (`ActivateNearbyGalaxyElements`; derived from the decompile). The camera follows the player.
    After Use in a docked pod and the undock, the player (state "Controlling", `Controlling` = the pod) has
    `Parent` = itself and its x/y **stay where the pod left the ship**: observed over 6 s, the body did not move while
    the pod flew 100 000 units. So the camera stayed at the mission ship, the home station was not activated, and the
    pod could not dock unless the mission ship happened to be near home (which explains the flaky passes).
  - Test fix: `gotohome` also puts the player's body at the pod. With it the pod docked within 2 s.
  - **Unknown, important**: whether a real player is left behind too, or only the bot. The bot is placed in its pod by
    `dockship` (the game's `DockWithAirlock`) and never walks in; undocking (`UndockAtSpeedAndAngle`) moves the ship's
    `Contents` that lie in the pod's footprint to the pod, so a player missing from the ship's `Contents` list would be
    left behind (guess). A walk-in test (`walkto` out and back, then Use) did not seat the bot. Needs a check by a
    walking bot before anyone flies home for real.
- **Correction to bug 3 above**: the host's pod "pushed away" was the station's hull pushing out a pod put inside it,
  and the host not docking was the body left behind. Putting the guest's puppet pod far away was tried again
  (mutation `nostow`) and again made no difference (home 12/12 with it switched off), so it is not in the mod. On the
  host's copy the guest's puppet pod may still sit at the home station after the guest docks (cosmetic, not checked).
- **Leaving early does not fail the mission** (observed): the guest flew home with the target alive; it docked
  (state "Free" at the station), `Completed` 0 and no `FailMission`. When the host later killed the target, the
  guest's `CheckIfAssassinationMissionCompleted` (run from `PlayerStateFree`) completed the guest's mission at home.
  `FailMission` is called from `DockAtStation`, `MissionCheckEarlyFailure`, `StartDeathZoom`, objective-delivery
  scripts (decompile); none of them fired here.
- **`CompleteMission` repeats** (observed): once the mission is complete the game calls it every second
  (`CheckIfAssassinationMissionCompleted` from the player's step), so a call count is no "just docked" signal.
- **Banners for the other player** (new): each player packet carries the last mission that ended on the sender's copy
  and how (`CompleteMission`/`FailMission` argument and kind). The other copy shows "<name> left the ship" when the
  sender's puppet starts being held (it had been aboard and no longer is), "<name>: mission complete" or
  "<name>'s mission failed" when the pair changes (not the first one seen after joining). Observed: host shown
  "Jigme Cuba left the ship" and "Jigme Cuba: mission complete" once each; the guest shown the host's. A failure
  banner has not been seen (no failure happened).
- **Not done**: a failed mission (a player captured or the pod destroyed); the real flight (bot steering) instead of
  `gotohome`.

## The pilot left behind at undock (2026-11-01)

Observed, unless marked:
- **Real players too**: the bot walked from its pod into the ship and back (`walkto`, with the host's guards told to
  ignore it), then Use: same result. Its body stayed where the pod left the ship, the camera with it; a screenshot
  showed empty space while the pod flew off. So this is not a test-placement artefact.
- **Frame by frame** (a per-frame watch of Parent, state and camera object): Use calls `InteractWith(<pod>)` (no
  `SitIn`, no Parent change); on the next frame the pod's `PlayerShipStateDocked` calls `UndockAtSpeedAndAngle`
  (`ContentsToMoveCount` 1: the player); the player's `Parent` becomes itself while still "Free", then "Floating",
  then "Controlling". The camera's `Object` stays the player throughout.
- **Why the body stays**: the undock removes the player from the ship's contents and calls
  `AddToPlayerShipsContents(player)`, which skips `global.Player` (decompile: it compares the id with global.Player
  and jumps past the re-parenting; observed: Parent stays itself after the call, with global.Player and
  global.PlayerShip correct). With no Parent, `UpdatePosition` moves the player by its own `xSpeed`/`ySpeed`
  (`UpdateSpacePosition`), which are 0. The carry test itself passes (inside the pod's bounds 0x637-0x63a, no module
  at that spot; test helper `carrycheck`).
- **Unknown**: what keeps the pilot with its pod in the unmodded game. The game's own `UndockFromStation` gives the
  same "Controlling, Parent = itself" state; pushing that pod by its speed left the body behind too, but that pod was
  still tied to the station (clamped at x 100000), so the check is not conclusive. Bot thrust (control 5 held) did not
  move it.
- **Fix**: while my player is at the controls of its own pod, the player's Parent is itself and the pod is free in
  space, the player's x/y (and xReal/yReal) are set to the pod's each frame. Observed: distance 0 in flight, the
  screenshot shows the pod centred. `gotohome` no longer moves the body. Mutation `nopilotfollow`.
- Side finding: on the host's copy, the host's own player was missing from its ship's `Contents` list (helper
  `incontents`); effect unknown.
- Side finding: walking into the shared ship without `unseen`, the guest was knocked out by 38 hits from the host's
  copy right inside the airlock (a guard or sentry there; likely normal play, not investigated).

## Unknown
- How the Nexus mod loader works and whether it can host networking code.
- Whether UndertaleModTool reads a GMS1 data chunk that is embedded in the exe.

## Knocked out: what the game does, and what co-op did (2026-11-01)

Observed, unless marked:
- **Unmodded flow, on the host (its copy runs the guards)**: with the target already dead (`hitguard -2 1`, so no escape
  could end the mission), the host walked into the guards' view (`walkto 3400 3400`): knocked out within 30 s, then
  state `Carried` (a guard in "Carrying Body"), after ~20 s `Floating` (thrown out of an airlock into space), after
  ~50 s more `Resting` and the "Character Select" screen; `global.Mission` gone. No `FailMission` and no
  `CompleteMission` call on that copy (the assassination was already complete). From the decompile (derived):
  `PlayerStateUnconscious` calls `SendNearestGuardToSavePlayer` (sets the guard's `MySaviour`/state), the guard carries
  the body to `NearestAirlock` and ejects it; `PlayerStateFloating` can end in `KillPlayer` (oxygen,
  `PlayerCanLoseOxygen`) -> `StartDeathZoom`, or a pod scooping the body up (`CanBeScoopedUp`).
- **The guest knocked out (co-op, before any fix)**: `walkto` into view with `unseen 0`: `Unconscious` within 5 s,
  injuries 7 -> 10, and then nothing: no guard came for it (the guest's copies of the guards are mirrors and do not
  act; the host never sends one for the puppet), and the host's guards kept shooting the puppet (hits events 56-65
  every ~0.7 s, forwarded to the guest). The alarm they raised made the target escape ~35 s later: `FailMission`
  from `MissionCheckEarlyFailure` on both copies in the same frame (then every frame), both end screens, and the
  banners "<guest>'s mission failed" on the host and "<host>'s mission failed" on the guest (first failure banners
  seen).
- **Misleading banner**: when the host was carried off and thrown out, the guest was shown "<host> left the ship".
- **Unmodded throw-out, in detail** (observed on the host, hooks on `EjectContentsFromAirlock`): the guard carries the
  body to its `DestinationAirlock` ("Fetching Body" -> "Carrying Body" -> "Leaving Body"); 0.8 s after it lays it down,
  `oAirlock_Step_0` calls `EjectContentsFromAirlock(<player id>, <airlock id>, 5000, <direction ~ the airlock's
  image_angle>)` with self = the airlock; a puppet standing in the same airlock went out in the same step. The airlock
  was not docked (`DockedState` 0) and its doors did not cycle. What starts the throw looks at `global.Player` only
  (unknown how): running the airlock's step with the puppet as `global.Player` threw nothing out.
- **Fixed, the knocked-out guest** (all observed in `fail.sh`, see below):
  1. The host's puppet goes "Unconscious" when the guest does (`ChangeStateTo`); while the guest stays down the puppet
     stays down (`SecondsUntilGetUp` held), and gets up with it. The guards stop shooting it.
  2. The guard sent for it (the puppet's `MySaviour`) keeps the puppet as `global.Player` during its step. Before, the
     host's own player standing nearer made the guard take the host as the body it carried; it put the puppet down
     ("Leaving Body" after 14.5 s every time).
  3. A guest knocked out in the airlock beside its docked pod no longer counts as "in its pod": that parked (hid) the
     puppet, and the guard lost the body.
  4. 0.8 s after the guard lays the puppet in an airlock, the host makes the same `EjectContentsFromAirlock` call for it,
     and does not board it onto the ship again until its player follows.
  5. The guest's copy follows its puppet (packet fields `body`, `body_fore/star`, `body_dx/dy/vx/vy`): held down while
     carried (positioned where the guard carries it), and when the puppet is thrown out, out of the ship at the same
     offset and speed, state "Floating"; from there its own copy runs the rest. Observed: oxygen ran out after ~50 s,
     "Death Zoom", then the character select, as for the host's own player unmodded.
  6. The guest's copy sends none of its own guards for its player (`SendNearestGuardToSavePlayer` dropped there): one
     of them (a guard the host had not reported lately runs its own step) had carried the guest off and thrown it out
     on the guest's copy alone, while on the host the puppet lay in the airlock.
  7. Banners: "<name> was thrown out of an airlock" (instead of "left the ship") and "<name> was lost in space".
- **A death crashed the game in co-op, host and guest alike** (observed, also with no knockout involved: the host's own
  player thrown out and lost): "Reached SwitchToCharacterSelect and no chooseable players exist, must have failed to
  generate choices". Cause, observed by an A/B run: `StartANewLife` builds the chosen station anew (new characters to
  choose) only if `station.MyBuilding` does not exist; with a puppet (made from one of our roster characters by
  `PlayAsCharacter`), the home station's building was still there after our player left (without a puppet, `NOPUPPET=A`,
  it was gone and the select worked), while its characters were gone. Why the building outlives the departure with a
  puppet: unknown (moving the puppet out of the building right after the undock did not change it). Fix: during a new life (after `EndLife`),
  if the station `GetRandomStationOfFactionWithNoClient` returns has a building with no character to choose in it, the
  mod removes it with `DestroyShip` (as `oStation_Destroy` does; `DestroyInstance` raised "Destroying a ship without
  destroying its contents first"). Observed: four new characters, the select open, no dialog.
- **Notes for tests**: a knockout made by the test helper alone (`knockout`, `ChangeStateTo` + `KnockOutTime`) brought no
  guard, even with the puppet's `SecondsSinceSeen` set to 0 (the game sends one only to a player its guards are onto,
  exact rule unknown); `fail.sh` lets the host's guards knock the guest out for real. Walking the guest through the
  ship with the guards watching raised the alarm and the target escaped: both missions failed early (`fail.sh` kills
  the target first). With the target dead, the lost player's mission ends without `FailMission` (observed, host and
  guest).
- **Later runs** (observed): once in 7 runs the guest, thrown out beside the airlock its own pod was docked at, was at
  that pod's controls a moment later (state "Controlling", off the ship): picked up by its pod (cause not checked; the
  game has `CanBeScoopedUp`). A pod undocking puts its pilot in "Floating" for a frame, so "thrown out" is now named only
  for a player who was down before it floated. In the suite, the host's guards now knock the walking guest out and
  carry it off as the game does (the sentry then holds fire at the downed puppet, and a down player takes no hits:
  `PlayerStateUnconscious` handles no damage); two suite checks were changed to accept that. A hit on the guest made it
  drop a grenade (`PlayerHandleDamageSignal` -> `CreateContentsOnTopOf`) whose blast hurt it on its own copy; grenades
  are not synced yet, so such blasts are counted apart from hits not from the host.
- Not done: a pod destroyed; a test that reaches the cases of `noguestsave` and `nodownpod`.

## Rescued by your own pod (2026-11-01)

- **Only your own pod** (decompiled): `PlayerStateFloating` calls `CanBeScoopedUp`, which looks at `global.PlayerShip`
  (needs fuel, `PlayerShipHasFuel`); while floating, the player steers its pod from outside (`PlayerRemoteControls`).
  The other player's pod cannot pick you up in the game; that would be a new feature, not a sync.
- **The pod flying free** (fixed): a player's pod was sent to the other copy only while the player sat at its controls
  ("Controlling"); a pod steered from outside by a floating player stayed docked at the shared ship on the other copy
  (observed with the change switched off, mutation `nofreepod`: nothing sent, the host's copy of the pod 884 000 units
  away). It is now sent whenever it is free in space too. Observed: the host showed it 125 units from where the guest's
  copy had it, while it drifted fast from the throw.
- **Observed** (`tools/rescue.sh`): thrown out, the guest's own pod picked it up (test helper `podtome`: the pod
  undocks and is put beside the player, instead of the flight), it flew home and its mission completed; the host was
  told "mission complete", not "lost". In 2 of about 10 runs the guest, thrown out beside the airlock its pod was
  docked at, was picked up at once without the helper.

## A new life, and the next shared mission (2026-11-01)

Observed (`tools/life.sh`), unless marked:
- **A new character**: after the guest was lost, a click on the first seat of the character select (as at the start)
  gave it a new character at its station, state "Free"; the next player packet carried the new name and the host's copy
  used it. Nothing in the mod had to change for this.
- **The other player's puppet was lost with the mission ship**: `EndLife` cancels the dead player's mission and
  destroys its mission ship with all aboard (decompile: `CancelMission`, `DestroyShip`), and a new shared mission
  replaces the old ship the same way; the other player's puppet, aboard it, went with it (on the guest's copy the host
  was invisible from then on; not live already before `EndLife`, so the ship had gone with the mission end before).
  Fix: once both players are aboard the next shared ship and the puppet has been gone for 2 s, another roster
  character becomes the puppet (as `two.sh` does at the start; the next life has one choice fewer). Mutation `norenew`.
- **A GameMaker error after the next mission was shared** (on the host, then on the guest): "GetFore" in
  `PlayerPathfindingTo`, called by the mod for a puppet left in space by the throw-out (Parent = itself) or on a ship
  that was gone. Such a puppet now waits out of sight, its step skipped, until its player boards the shared ship;
  boarded again, it is stood up ("Free") if its player is. Mutation `nospacewait`.
- **Test notes**: the pod picked up its thrown-out pilot even when free in space 5000 units away (whether the game
  flies a free pod to its floating pilot by itself: unknown), so `life.sh` empties the pod's `Fuel` and `EmergencyFuel`
  first (`CanBeScoopedUp` needs fuel). The test's walk from the pod found no path on the second ship (another layout), so
  the last check takes the puppets parked in the pods (aboard, hidden). `life.sh` passed 4 of 9 runs on the final build: every failure
  was the test re-docking the new character with its jump-and-dock helpers (the body stayed in the station's building,
  Parent = the building, state "Controlling", while its pod docked; two or three times a GameMaker error in
  `DockWithAirlock`, called by the helper `dockship`). Causes unknown; a real player flies the pod instead.

## Windows broken: hull breaches (2026-11-01)

Derived from the decompile (`oWindowGlass_Step_0`):
- A window is an `oWindowModule` with an `oWindowGlass` (`MyModule` = the module). The glass breaks when its
  `DamageSignal` carries a damage flag that breaks glass, or when its `BurstOverride` is set: a sound,
  `WindowChangesCollisionGrid`, `GiveModuleAForcefieldInDirection`, `CreateAirlock`, `DestroyContents`, and the glass is
  gone. Explosions break windows through `DamageWindowsInRadius` (`ExplodeOnShip`).

Observed:
- **Before the fix a broken window broke on one copy only**: a shown bullet's hit is dropped (`ghost:` lines), and the
  test break on the guest left the host's glass whole. On the guest an airlock (object 116) and a forcefield (61)
  stood where the glass was; on the host the "Window" (83) was still there.
- `oWindowModule.Burst`, which the glass's step looks to set, read 0 on the module after a break (why: unknown). The
  mod sees a break as the glass going away while its ship lives on.
- **Fix**: each copy remembers the shared ship's windows by their module's ship-wise position (the same on every copy,
  instance ids differ), and sends every window broken there (both directions, every 15 frames; breaking is one-way, so
  the union is enough). The other copy sets that window's `BurstOverride`, and the glass breaks the game's own way at
  its next step: the same objects in its place on both copies. Mutation `nobreach`.
- **Blown out**: the guest standing next to a window the host's copy broke was pulled out through it on its own copy
  (state "Floating", flying off fast), as the game does to anyone at a broken window. The host's banner said "left the
  ship"; now "... was blown out into space" when the other player starts floating within 3 s of a window breaking (or,
  since the next section, a module going). Mutation `noblownnews`.
- **Test** (`tools/breach.sh`, 8 checks): windows are broken with `BurstOverride` (helper `breakwin`), because the
  bot cannot aim a gun or throw a grenade yet; a break by a real shot or blast is not tested.
- Not covered: a copy that was not running when the glass broke (a hot reload) never reports it.

## Modules destroyed by explosions (2026-11-01)

Derived from the decompile:
- `DestroyModule(module, name)` (called by `ExplodeOnShip`, missiles, `BlastDestroyModule`,
  `DestroyAllModulesInBlastRadius`) works `with (module.Parent)`, skips a station, and then: alarm, screen shake, an
  explosion in space, the ship's `ModuleGrid` and `DestroyedModule`, turrets and thrusters, `CreateBreachForcefield`,
  the player's pod undocked if its airlock went (`UndockAtSpeedAndAngle`, `PlayerReturnToShip`), `SplitShip`, and the
  module destroyed. `DestroyMultipleModules` has its own copy of that code (it does not call `DestroyModule`).
- `ModuleAt(fore, starboard, ship)` finds a module from the ship's grid (`ModulesLong`, `ModulesWide`, `ModuleLength`,
  `ModuleWidth`).

Observed:
- The mod first called `DestroyModule` and nothing happened: the game's own `Exists` said no for a module that was
  there. Modules and contents away from the players are deactivated; `ExplodeOnShip` calls `ActivateMyModules` and
  `ActivateMyContents` first, and with those calls it worked.
- **Fix**, as for windows: each copy tracks the shared ship's modules (its `ModuleList`) by ship-wise position and sends
  every one it has seen destroyed (both directions); the other copy activates the ship's modules and contents and runs
  `DestroyModule` on its module at that position. A part cut off from the ship splits off the same way on both copies
  (the end module of an arm, held only by the destroyed one, went on both). Mutation `nomodloss`.
- A guest standing in a module whose neighbour the host's copy destroyed was blown out on its own copy ("Floating"), and
  the host was told "... was blown out into space".
- Game rules seen on the way (the same on both copies, nothing to sync): a test blast that took the assassination
  target with it failed the mission at once (`MissionCheckEarlyFailure`: the target no longer exists); destroying the
  module whose airlock held the guest's pod undocked the pod with the guest at its controls.
- **Test** (`tools/modloss.sh`, 9 checks): modules are destroyed by the helper `blastmod` (the game's `DestroyModule`),
  not by a real explosion; positions are those of the fixed test ship. Its walk to the window module found no path once
  other modules were gone, so the blow-out check runs first.
- Not covered: grenades themselves (the explosion, damage to people) are still not shown on the other copy.
=======
>>>>>>> /proc/self/fd/11
