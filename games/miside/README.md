# MiSide (2024)

| | |
|---|---|
| Engine | Unity 2021.3.35f1, IL2CPP, Windows x64 (BepInEx 6.0.0-be.790 Unity.IL2CPP, HarmonyX, Il2CppInterop) |
| Versions tested | Steam (app 2527500), buildid 19029065. `MiSideFull.exe` SHA-256 `562723c950c2be95ecbdc516530aee56b97837e2ac6610b8b5bce394e23022c0` |
| Projects | MiSide Together ([miside-coop](https://github.com/multiplayereverything/miside-coop), [v0.3.5](https://github.com/multiplayereverything/miside-coop/releases/tag/v0.3.5) pre-release, [v0.3.2](https://github.com/multiplayereverything/miside-coop/releases/tag/v0.3.2) full release); directory entry "MiSide Together" |
| QA level | automated |
| Last updated | 2026-10-30 |

## Where things stand

MiSide Together lets 2 to 4 players play MiSide's story together. The host leads. Everyone loads the same chapter, sees the same dialogue, cutscenes and Mita, and sees the other players as the main character with their real pose. It is an alpha. Every chapter has been driven by automated tests with hidden game copies on one PC, and all of Scene 1 has been played through by two copies into Scene 2. The computers in Scenes 13 to 15 and the Scene 1 and Scene 14 phones are shared. The mod's author has played it with two friends, three players together over Steam on separate PCs with their own accounts. They reported 19 problems, and 0.3.5 fixes most of them.

## Existing work

- [QuestionBlock64/MiSide-Multiplayer](https://github.com/QuestionBlock64/MiSide-Multiplayer): "a work in progress MiSide Multiplayer Mod", last push 2026-09. MiSide Together took ideas from it (the Mita face relay, gaze at the nearest player, its list of death hooks, cloning native menus). That repo has no license, so no code was copied.
- On Nexus, QuestionBlock64's mod (#778) and WiSide MultiSplit (#779) show the other players only; neither syncs the world, Mita or the story.
- No other co-op project was found in this directory. Steam Workshop and non-English sites were not searched.

## How the game works (for co-op)

- **Engine:** IL2CPP. BepInEx 6 generates interop assemblies on first start, about 89 assemblies in about a minute, and Harmony patches go through Il2CppInterop. Decompiled interop gives signatures only, with no method bodies. The mod's `-mt-trace Type1,Type2` switch logs the native address of each method and every call while playing, so you can decompile the functions that actually run.
- **Story:** linear and scripted. It is driven by `UnityEvent`s on `ObjectInteractive.Click()` (the generic "press E"), trigger volumes, timelines (`CutscenePlay`) and `Dialogue_3DText`. That made relaying story *events* a better fit than relaying state. Late arrivals get a scene's events replayed from a catch-up history.
- **Loading chapters:** `SceneManager.LoadScene("Scene 1 - RealRoom")` loads a broken scene, with a NRE every frame and no `PlayerMove`. The game's own route works instead: set `GlobalGame.LoadingLevel` to the chapter name, then load `SceneLoading`. Chapter changes in co-op run the game's `Scene_Load` on every machine.
- **Player:** `PlayerMove` is the controller (`animPerson` is the body Animator, plus `TeleportPlayer`). `PlayerPerson.meshes` and `meshHead` are the body renderers. Held items are parented under the hand bones, at `GlobalTag.playerRightItem`.
- **Steam:** `steam_api64.dll` ships with the game, and Steamworks.NET is in the interop. Starting the exe directly makes Steam relaunch it and drop the command-line arguments, so lab copies carry `steam_appid.txt` (2527500).
- **First start:** a fresh install (no settings in the save folder) stops in `SceneLoading` on a LANGUAGE and VOICE picker (`ChangeLanguageStart`) until someone picks, so automated copies hang on the loading screen with no error in any log. The lab answers it the way a player would: `ChangeLanguage(i)`, `ChangeVoice(i, true)`, then `Apply()`, with the indexes read from the English buttons (`ChangeLanguageStartButton.index`, told apart by their `Language` / `Voice` parent).
- **Saves:** clients continue from the host's save files. Their own saves are backed up while connected and restored afterwards, or at the next start after a crash.

## What we changed

The full feature list is in the mod's README and [docs/SYNC.md](https://github.com/multiplayereverything/miside-coop/blob/main/docs/SYNC.md). [docs/SYNC-INVENTORY.md](https://github.com/multiplayereverything/miside-coop/blob/main/docs/SYNC-INVENTORY.md) classifies every game script in all 19 chapters. This section lists the lessons that apply to other games.

### Inventory the whole game before syncing it

A one-player tour (`inventory`) loads every chapter. It lists each chapter's game scripts, dialogue lines, choices and timelines into `SYNC-INVENTORY.md`, with 0 unclassified. Each row says how it syncs, who is in charge, and which tour proves it. Work then goes down that list instead of following bug reports.

### The host runs every cutscene

- **Bug:** cutscenes started by each machine's own triggers drifted, and a client could start one the host never saw.
- **Fix:** during a chapter, a client's cutscene start becomes a request. The host starts the cutscene, and every machine plays it from the host's command on the host's clock. Other players are hidden during the cutscene and gathered next to the host on safe floor afterwards.
- **Proof:** in the `hostcut` tour, the client's cutscene ran 0.13 s from the host's.

### Held items: read the rule from the game, not from a list per cutscene

- **Rule:** if an interaction's own setup puts an item in the player's hand, only the presser does it, and everyone sees the item in the presser's right-hand slot. This covers the toy plane, the phone, the cartridge and Scene 2's batteries, spoon, bow and pencil. Every other interaction is mirrored: each player plays the same animation on their own character at the same spot.
- **Detection:** the mod reads this from the game's interaction data, so no list per cutscene is kept.
- **Trap:** the item is the object itself, moved into the hand slot when the pickup animation reaches that moment. The first version watched the wrong object, and some earlier test passes were false. Check what really ends up under the hand bone.

### A cutscene can be playing without having been started

- **Bug:** at the Scene 12 exit door, the host went into the cutscene and the client did not, ending 4 m apart. Two causes: the host's cutscene command reached the client a moment before the client's story had switched the cutscene object on, so it was ignored; and the client's timeline was already playing on awake, without `Playable_Animation.Play` (which puts the player into it) ever running. A third piece: the door's event also starts a player animation (hiding from Mita) that only fires on the host.
- **Fix:** cutscene starts are retried for 5 s like other story events, and "already playing" only counts when `Play` really ran on that machine. The host reports every animation its story starts on its own player; a client that has not started the same one within a second plays it (checked by the client's own object key, which differs where sibling order differs).
- **Proof:** Scene 12 in the every-chapter run, both players ending 0.00 m apart.

### A copy of the first-person body needs its shadows back

- **Bug:** the other player looked flat and over-bright next to Mita. Their materials are the same toon shader with the same values; the difference is that the first-person body has `receiveShadows` off, so the copy stayed fully lit in shade.
- **Fix:** turn `receiveShadows` on for the copy. A lab dump (`MT-LOOK`) lists every renderer's shader, probes, shadow flags and toon values for the local body, the copy and every other character, which is how this was found.

### Presses must survive a machine that is a moment behind

- **Bug:** a press reaching a client whose room objects weren't enabled yet, or were in a different order, was lost. This broke the Scene 17 levers.
- **Fix:** a press is retried for up to 5 s and matched by key, falling back to the object's name. Only presses that actually worked on the sender are shared, so pressing something the story hasn't enabled does nothing for anyone.

### One compiled body, many methods: a hook fires for code you never hooked

- **Bug (friends' playtest):** in Scene 1, a client became "owner" of the Scene 18 tic-tac-toe minigame, and from then on the host's phone pickup was blocked.
- **Root cause:** IL2CPP folds methods with identical machine code into one function. The Harmony hook on `Location18_TicTacToe.StartGame` sits on that shared function, so it also ran when Scene 1's tutorial hint faded.
- **Fix:** every hook that acts on its instance checks `__instance.GetIl2CppType().Name` first.
- **Proof:** the new lab screenshots and the `scene1` tour showed the false claim; after the fix the client ends Scene 1 on the host's day.

### Find story gates in the wiring, not by tracing

- **Bug:** in hidden lab copies the Scene 1 phone was never armed, so its pickup could not be tested. Tracing the game's timer scripts with `-mt-trace` changed their timing and hid the cause.
- **Fix:** `-mt-dump <scene>` writes every persistent `UnityEvent` call in the scene (`source [Type].field -> target [Type].Method`), including events held in arrays, lists and game structs. Searching it for the phone and walking backwards found the gate in one run: with quest hints on, the tutorial switches on a T-shirt, and the end of the T-shirt's dialogue arms the room's interactives (`ObjectInteractiveGroup.ActivationAndDestroy`).
- **Proof:** the `phone` tour passed for the first time once the lab clicked the T-shirt.

### Drive the game's UI the game's way

- **Bug:** making the Scene 1 phone app follow the host. Setting the dialogue's current string directly skipped the game's text handling (the client showed a later line, in Russian). Blocking `NextDialogue` on clients also blocked the game's own first line.
- **Fix:** only the client's own input (`KeyNextDialogue`, app buttons) is blocked; the client steps forward like a Space press once its own line has finished typing. The host's button presses, name, purchases, cooking result and money are relayed (decompiled from the app's dialogue code to find which call does what).
- **Proof:** the `scene1` tour plays every day (1, 2, 5, 10, 20, 33, 37) with two copies, both arriving in Scene 2.

### Leaving to the menu goes through the loading scene

- **Bug:** a client who exited to the main menu was pulled back into the chapter, and a client following the host to the menu was left with its first-person hands in the menu.
- **Root cause:** the game's exit loads the menu through the loading scene. The follow logic saw a load and reloaded the host's chapter; a plain menu load skipped the game's own cleanup.
- **Fix:** hook `GameController.ExitGame`: the player's own exit leaves the session, and following the host uses the same call, flagged as the mod's own.
- **Proof:** the `menuleave` and `menureturn` tours, both failing before the fix.

### Note your own press before the game's click code runs

- **Bug:** found by the story autopilot: in Scene 1 the host's phone pickup was held back as "another player's animation" whenever the client happened to be animating, and the client ran the phone's story timeline alone, ending a quest ahead.
- **Fix:** a player's own press is recorded in a prefix on `ObjectInteractive.Click`, because the pickup animation starts inside `Click`. Clients no longer start a host timeline that belongs to a presser-only interaction.
- **Proof:** the `storypilot` tour in Scene 1, which failed before; `phone`, `held`, `mirror`, `carry`, `interactions` and `scene1` pass afterwards. This fix is in the repo after v0.3.5.

### Other playtest fixes

- **Held items ride on the hand bone:** the game's hold slot sits near the camera, so other players saw the phone floating at the holder's side. Carried items are now placed on the holder's nearest hand bone (`phone` tour: 0.12 m from the hand).
- **Rooms stay on around a player:** the host's room culling switched off the room a client stood in. A room (solid colliders at least 2.5 m wide) stays on while this machine's player is inside it (`roomhold` tour, mutation-checked).

### IL2CPP stripping

- **`Animator.parameters`** throws a TypeLoadException under Il2CppInterop, and the `get_parameters` method is stripped. The build reads every AnimatorController from the user's own game data with UnityPy instead: 282 parameters across 109 controllers. The table is embedded and looked up by controller name. The string `Get/Set*` overloads work.
- **`GUILayout.BeginArea`** is stripped too. The F5 panel uses plain `GUI` calls, and each widget falls back to a label plus Input handling if it is missing.

## QA

- `scripts\run-tour.ps1 -Tour <name>` runs one tour with 2 to 4 hidden lab copies over loopback. `scripts\lab-suite.ps1` runs them all, one at a time. The lab backs up and restores the user's save folder and refuses to start while the user's own MiSide is running.
- Log lines are judged by `tests/verify-lab.py`:
  - `MT-CHECK`: assertions;
  - `MT-METRIC`: frame cost, including the worst frame;
  - `MT-STATE`: every 2 s, the position, rotation and state of players, NPCs, dialogue lines and timelines, lined up across machines into desync numbers;
  - `MT-ANIM`: how late each NPC animation started on clients.
- Lab screenshots (`LabShot`, converted by `tools/shots_to_png.py`) let a tour compare what each player sees.
- **v0.3.5 (2026-10-28):** the suite has 34 tours with 2 to 4 players, including host migration, a 120 ms lag run, the Steam stand-in (`steam`: 60 of 60 messages in order with 68 failed sends), the shared computers (`core`, `quest`, `mobile`), all of Scene 1 (`scene1`, `scene1end`) and Scene 3's opening (`scene3`). Last full run: 33 of 34; `scene19` stuck on the first splash screen and passed on its own rerun. `scripts\interactions-all.ps1` presses every interaction in every chapter with 2 players: 18 of 18 chapters pass.
- **Story autopilot (after v0.3.5):** `scripts\storypilot-all.ps1` has the host play each chapter as far as simple actions go (answer choices, press armed objects, look at and walk into story triggers) while the client must hold the host's quests, checked every 30 s. First run: 17 of 18 chapters stayed together; Scene 1 did not, which led to the "own press" fix above, after which Scene 1 stays together too. In 9 chapters the autopilot gets only 0 to 2 actions in (they need chases, minigames or aiming), so those chapters prove little yet.
- **Hands-on playtest (2026-10-28):** the mod's author and two friends, three players on separate PCs and Steam accounts, joined over Steam. Their 19 reports (with screenshots of errors) became the 0.3.5 work; most were reproduced in the lab first and given a tour.
- Not covered: four real players, and the dialogue-choice and NavMesh guards added for playtest errors that the lab could not reproduce.

## How to play

1. Install BepInEx 6 (Unity.IL2CPP, win-x64), build be.790, into `<game dir>`, and start the game once.
2. Unzip a [release](https://github.com/multiplayereverything/miside-coop/releases) into `<game dir>`.
3. Open **MULTIPLAYER** in the main menu. The host picks HOST GAME.
4. Friends pick JOIN <name> (STEAM), JOIN <name> (LAN), or join by address (F5, TCP 27030).

Everyone needs the same game build and mod build.

## Known issues

- The Scene 13 and 15 computers and the Scene 14 PC game are run by one player; the others watch through their own eyes.
- In the Scene 1 phone app's mini-games, the result and money are the host's, but each cut or move can look a little different on the others' screens.
- The handheld Tetris is each player's own.
- Scene 19 (Glasses) cannot be started from the chapter menu, even in single player. Reach it through the story.
- A player who joins mid-chapter waits in the menu until the next chapter starts.

## Next steps

- Play 0.3.5 with friends again over Steam to confirm the playtest fixes, and to check the dialogue-choice and NavMesh guards.
- Teach the story autopilot the actions it stops at (chases, minigames, aiming) so it plays further into Scenes 9 to 11, 14 to 16 and 20.
- Play the story through in co-op and write it up for `verified-playthrough`.
