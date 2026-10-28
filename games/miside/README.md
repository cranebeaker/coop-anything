# MiSide (2024)

| | |
|---|---|
| Engine | Unity 2021.3.35f1, IL2CPP, Windows x64 (BepInEx 6.0.0-be.790 Unity.IL2CPP, HarmonyX, Il2CppInterop) |
| Versions tested | Steam (app 2527500), buildid 19029065. `MiSideFull.exe` SHA-256 `562723c950c2be95ecbdc516530aee56b97837e2ac6610b8b5bce394e23022c0` |
| Projects | MiSide Together ([miside-coop](https://github.com/multiplayereverything/miside-coop), [v0.3.2](https://github.com/multiplayereverything/miside-coop/releases/tag/v0.3.2)); directory entry "MiSide Together" |
| QA level | automated |
| Last updated | 2026-10-27 |

## Where things stand

MiSide Together lets 2 to 4 players play MiSide's story together. The host leads. Everyone loads the same chapter, sees the same dialogue, cutscenes and Mita, and sees the other players as the main character with their real pose. It is an alpha. Every chapter has been driven by automated tests with hidden game copies on one PC. The computers in Scenes 13 to 15 are not shared yet, and Steam has only been tested with one account.

## Existing work

- [QuestionBlock64/MiSide-Multiplayer](https://github.com/QuestionBlock64/MiSide-Multiplayer): "a work in progress MiSide Multiplayer Mod", last push 2026-09. MiSide Together took ideas from it (the Mita face relay, gaze at the nearest player, its list of death hooks, cloning native menus). That repo has no license, so no code was copied.
- No other co-op project was found in this directory. Steam Workshop, Nexus and non-English sites were not searched.

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
- **v0.3.2:** all 21 tours pass with 2 to 4 players, including host migration and a 120 ms lag run. `scripts\interactions-all.ps1` presses every interaction in every chapter with 2 players: 17 of 18 chapters pass. The one left is Scene 13, where the other player stays seated at the PC after the host takes the cartridge (presser-only, so expected).
- Not covered: the Scene 1 phone pickup itself (Scene 1's opening holds the host back in the lab), and a two-account Steam session over Valve's relay.

## How to play

1. Install BepInEx 6 (Unity.IL2CPP, win-x64), build be.790, into `<game dir>`, and start the game once.
2. Unzip a [release](https://github.com/multiplayereverything/miside-coop/releases) into `<game dir>`.
3. Open **MULTIPLAYER** in the main menu. The host picks HOST GAME.
4. Friends pick JOIN <name> (STEAM), JOIN <name> (LAN), or join by address (F5, TCP 27030).

Everyone needs the same game build and mod build.

## Known issues

- The Scene 13 and 15 computer folders, the Scene 14 phone chat and the Scene 14 PC game run separately for each player.
- The handheld Tetris is each player's own.
- Scene 19 (Glasses) cannot be started from the chapter menu, even in single player. Reach it through the story.
- A player who joins mid-chapter waits in the menu until the next chapter starts.

## Next steps

- Share the Scene 13 to 15 computers.
- Make the every-chapter run accept the Scene 13 seat, and prove the Scene 1 phone pickup.
- Run a real two-account Steam session.
- Play the story through in co-op and write it up for `verified-playthrough`.
