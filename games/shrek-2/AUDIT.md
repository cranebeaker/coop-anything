# Shrek 2 Enhanced: audit of the mod and of the game on modern hardware

Date: 2026-10-31. Game: the Russian 1C PC edition of Shrek 2 (Unreal Engine 2.5, Kuju). Scope: the mod (`Enhanced.u`, installer, tests),
the game's own scripts and log output, and what a current PC (high-DPI, wide or ultra-wide screens, high refresh rate, 64-bit Windows 11)
asks of a 2004 game. Method: reading the decompiled scripts, reading the logs of test runs, reading the PE header of `Game.exe`,
and test runs of the mod in a copy of the game. Every item says whether it was seen (observed), worked out (derived) or not known.

## 1. Defects found and fixed in this pass

| # | What | Evidence | Fix |
|---|---|---|---|
| A1 | **The HUD was not drawn** (no health bar, coins, potions) in every run of the mod. | Observed: a screenshot of a swamp run had no HUD; the log line added for the check showed `items=0`, then `items=7` and `scale=0.00` after the first fix. | Two causes. The mod's HUD class skipped the stock setup of the HUD items, and its `PostRender` override bound its `Super` call to the engine's HUD, skipping the stock `KWHud.PostRender` (which draws the items and sets the scale). The mod is compiled against the **Level Editor's copies** of the game scripts, where `KWHud` has no `PostRender` and `KWHudItem` has no `RenderHud`; the retail scripts have both. The class now creates the items itself and overrides nothing that the Level Editor's copy lacks. Checked by screenshot: health bar, potions and coins are drawn. |
| A2 | About 11 000 script warnings a minute in the front-end level (`PlayerController.PlayerWalking.PlayerMove`, twice per frame). | Observed in the logs of the menu runs (10 940 times each in one 3-minute run). | The hero controller of the mod ignores `PlayerMove` while it has no pawn (the hero is held by the menu cutscene). The same run now has 3 warnings. |
| A3 | Three warnings per pawn from `KWPawn.AddDefaultInventory` (reads element 0 of empty equipment lists). | Observed in every level log (`Accessed array 'RequiredEquipment' out of bounds (0/0)`). | The mutator gives a pawn with empty lists one empty entry, which means what the empty list meant. |
| A4 | Menu pages of the previous level stayed open over the next one (the book was still opening and the "Create a game" page was already on top of it). | Observed on a screenshot sent by the owner. | Every hero controller and every guest controller closes all pages in its first tick; every page of the mod is opened on a clean stack. |
| A5 | The engine's `NumPlayers` does not count the second player (the mod's own `Login` does not call the stock one). The LAN list showed 0 players, a gate would have counted one player. | Observed in the LAN answer (`...|0|2|...`). | The mod counts its player controllers itself (`PlayerCount`). |

| A6 | In a network game a hero who dies stays dead: the stock death state ends in `stop` unless the game is single-player (`NM_Standalone`). Player two's pawn also lost its controller there (warnings in `stateHeroDying`). | Observed in the logs of a run in which player two died (stock code read in `ShHeroPawn.stateHeroDying`). | `EnhancedGame.DeathWatch`: player two is brought back after 4 s next to the host with full health; when the host dies the level is loaded again after 5 s from the last co-op checkpoint of the session (or from its start) and everybody follows. Verified in a scripted run (kill player two, kill the host). |
| A7 | A subtitle could stay on the screen for minutes (the owner's "texts hung"; seen again in the gate runs: the Donkey's hint stayed over 30 s). | Observed. | `EnhancedCutText` clears a line that has stayed longer than 6 s + 0.12 s per letter. |
| A8 | `KWGame.PostLogin` read the pawn of player two, which does not exist yet (2 warnings, and the start location from nothing). | Observed in the log. | The mod does what the stock one does for player two, without the pawn. |
| A9 | The host's lobby mode survived into the game level (the options of the old URL are merged into the new one), so a second LAN beacon tried to bind the same port (`BindPort: bind failed`). | Observed. | A game level is never a lobby. |

## 2. Script warnings that remain (stock game, one-off or per object)

Observed in a plain swamp run (30 warnings in 40 s, none repeating per frame): `ShHeroPawn.IsFighting` reads a controller that is not there (2),
`KWCutController.Scripting.UnPossess` (2 at the menu), `PotionBottle*` `HasAnim: No mesh` (5 lines per bottle), and on the preamble map
`DummyPlayer` `TravelPreAccept` / `PostLoadGame`, `KWGame.Persistence_RestoreBool`, `KWHeroController.vCameraStart` / `rCameraRot`.
None is a mod defect and none costs time. They can be silenced by overriding the function in a subclass; not done, there is no behavior behind them.
`SHHud.DrawLevelAction` writes `DrawTile: Missing Material` once on a joined guest in the lobby (the guest's HUD is not the stock one).

## 3. Modern hardware and screen formats

| # | Area | Finding | Status |
|---|---|---|---|
| C1 | Memory | `Game.exe` is a 32-bit program without the large-address-aware flag (PE characteristics `0x10f`): at most 2 GB of address space on 64-bit Windows. The planned HD texture pack and the mod's data compete for it. Observed. | Done in the installer (hash-checked, backup, the uninstall restores it): characteristics `0x12f`. The patched test copy ran the swamp without error. Stability above 2 GB of use is not tested. |
| C2 | High-DPI | No DPI-awareness in the manifest of `Game.exe` (observed). At 125 to 200 % Windows scaling the window is bitmap-scaled by the system (blurry, wrong size). Derived. | Done in the installer: an external `Game.exe.manifest` (the exe has no manifest of its own), removed by the uninstall. Looks of the window at 125 to 200 % scaling not checked on such a screen. |
| C3 | HUD on wide screens | The stock HUD is laid out for 1024 x 768 and scaled by width and by height separately (`CalculateScaleX` = width / 1024, `CalculateScaleY` = height / 768). On 16:9 the icons and numbers are stretched sideways by 33 %, on 21:9 by 75 %. Derived from the script. | Done in the mod's HUD: one scale from the height; items keep their place (they are placed by fractions of the width). To check on a real wide window. |
| C4 | Field of view | Unreal Engine 2 keeps the horizontal field of view fixed, so a wide screen sees less above and below than a 4:3 one (Vert-). Derived. | Proposed: compute the horizontal FOV from the aspect ratio ("Hor+") for the gameplay cameras. The cutscenes set their own FOV (269 `FOV` verbs in the scripts) and need to be left alone or scaled the same way. |
| C5 | Menus | The book is a 3D mesh, the buttons are GUI fractions of the screen; on a non-4:3 screen the buttons no longer sit on the printed buttons of the book. Derived; observed only at 4:3 so far. | To test at 16:9; the pages of the mod use the same fractions as the stock ones, so they behave the same. |
| C6 | Frame rate | The config has `MinDesiredFrameRate=35` and no cap or v-sync switch (no entry found). How the physics and animation behave at 144 or 240 Hz is not known. | To test. The per-frame code of the mod is small (see D1). |
| C7 | Renderer | Direct3D 8 (`D3DDrv.dll`); a `d3d8.dll` proxy sits in the test copy. On a current GPU this works through Windows' Direct3D 8 layer; anisotropic filtering, anti-aliasing and correct stretch options are what a wrapper (a D3D8 to D3D9/11 layer) adds. Unknown for the owner's install. | To decide with the owner. |
| C8 | Resolutions | The sample config names 1920 x 1080 for fullscreen; 4K was not tried. The GUI fonts are bitmap fonts of fixed size, so on 4K the text is small. Unknown. | To test. |
| C9 | Input | DirectInput 8 for mouse and keyboard; the mod's input class already handles look, arrow keys and acceleration the same for both players. Raw Input is not used. | No action. |

## 4. Optimization in the mod

- D1. `GateTick` runs every frame over the cutscene triggers of the level (about 30 in the swamp) and the players. It read a trigger's active flag as text
  (`GetPropertyText`) once per trigger per frame; it now reads the typed variable. The rest is a distance test per pair.
- D2. The LAN search sends 10 to 15 small UDP packets every 2 seconds, only while the search page is open or a lobby is hosted.
- D3. The checkpoint record walks the actors of nine classes once per counted checkpoint (measured earlier, `NOTES.md`); no per-frame cost.
- D4. The 10 940-warning loop (A2) was the largest cost found: every warning is a log write.
- D5. The `[SDBG]` log lines are always on. Proposed: a config switch for the release, they are useful only in test runs.

## 4b. Seen once, not reproduced

At a 1280 x 720 window one picture of the first swamp seconds showed three Shrek models around the camera. A second and third run at that size showed one
model. Cause unknown; most likely the start cutscene's stand-ins. Not treated as a defect.

## 5. Build environment (the main risk)

`Enhanced.u` is compiled with the Level Editor's compiler against the Level Editor's copies of `Engine`, `KWGame` and `SHGame`. They are older than the
retail scripts (A1 is the proof: `KWHud.PostRender` and `KWHudItem.RenderHud` are missing there). A `Super.Name()` call, or an override, of a function that
exists in the retail game but not in the Level Editor's copy is compiled against the wrong class, and at run time silently skips the stock behavior.
Every stock function the mod overrides or calls through `Super` should be checked in both versions. Done for the HUD; open for the rest
(`EnhancedHeroController`, `EnhancedGame`, `EnhancedCutText`, `EnhancedShrek`), which behave correctly in the tests that were run.

## 6. Not audited

Sound mixing and the voice-over timing (the sound desync report is open, see `NOTES.md`), the save format of the stock game, the Beanstalk bonus
levels, the Karma physics objects, controller (gamepad) support.
