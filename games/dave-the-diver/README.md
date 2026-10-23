# Dave the Diver (2023)

| | |
|---|---|
| Engine | Unity 6000.0.52f1, IL2CPP (BepInEx 6.0.0-be.788 + HarmonyX) |
| Versions tested | Steam (app 1868140), Windows. `GameAssembly.dll` SHA-256 `8544e01549c9c9329c2c18283f7e35b652b174f59834cfc0279cf8732637be59`, `DaveTheDiver.exe` SHA-256 `d5e61b65ba86c35f92c2fcf749b62c02f2a7fe1b6bd13b0ff8d6a1d3ea2976c3` |
| Projects | [Daves the Divers](https://github.com/masonan777/Daves-the-Diver-Coop-Mod) (local co-op), [Coop-Divers](https://github.com/S1ii/Coop-Divers) (online) |
| QA level | automated (creature grabs in dives); the rest is hand-tested |
| Last updated | 2026-10-19 |

## Where things stand

Two people can dive together on one PC today. Player 2 is a second, purple Dave on the right side of the keyboard or an Xbox controller, with their own oxygen, HUD and loadout. The sushi bar has a second restaurant Dave who can serve, clean and play the mini-games. Story scenes, the farm and other side activities are single-player as before.

## Existing work

- **[Coop-Divers / DaveTheDiverMP](https://github.com/S1ii/Coop-Divers)** (MIT, last commit 2026-07). Online, host-authoritative co-op over direct UDP. Its README lists synced fish, pickups, bosses, story progress and the sushi bar. No GitHub releases. It started this project, but **Daves the Divers shares no code with it**.
- **[Daves the Divers](https://github.com/masonan777/Daves-the-Diver-Coop-Mod)** (MIT, 2026-10). Same-screen co-op in a single game instance. This write-up is about this one.

## How the game works (for co-op)

- **No netcode, one player everywhere.**
  - Gameplay code finds "the player" through a singleton field: `InGameManager.playerCharacter`, which is `Singleton.Instance + 0xA8` in the native code.
  - Restaurant code does the same through `SushiBarStaffManagerImpl.m_Dave`.
  - Grabs, bites, the button-mash bar, HUD updates, the camera and many animation events all act on that one field.
- **Input:**
  - Dave reads his controls in `PlayerCharacter.OnDirectHandler(input)` (`StaffDave.OnDirectHandler` in the restaurant) through the game's `InputSystemWrapper`. Each input map (`Dave`, `InGame`, `QTE`, `MiniGame` …) has its own direct handler.
  - Mini-games take over the input, so the Dave map stops getting actions.
- **Creature grabs** are spread over several parts: skill runners (`QTEReadyToEnterSkillRunner`, `BiteHoldSkillRunner`, `OpenQTEUISkillRunner`), the `DR.AI.Biteable` ability, and creature animation events (`AniClipEvents.OpenQTEUI`, `PlayDaveAnimation`, `DoDamageEvent`). All of them target "the player".
- **Reading the code:**
  - Il2CppInterop (BepInEx `interop/`) gives signatures.
  - For method bodies, run Cpp2IL `2022.1.0-pre-release.21` with `--output-as diffable-cs` for layouts and field offsets (~16 s), and `--output-as isil` for bodies (~1 min, ~800 MB).
  - Then name the call targets with an RVA map. [tools/annotate_isil.py](https://github.com/masonan777/Daves-the-Diver-Coop-Mod/blob/main/tools/annotate_isil.py) in the mod repo does this.

## What we changed

The core trick is **player 2's turn**. Player 2 is a runtime clone of player 1's `PlayerCharacter`. When the game needs to run code for player 2, the mod points `InGameManager.playerCharacter` at player 2, calls the game's own method, and points it back. That reuses the game's bite, grab, mash-bar, helper-item and HUD code instead of rewriting it. The rest is the clone's shared state, split per player:

| Bug | Root cause | Fix |
|---|---|---|
| Player 1's mouse aim froze after player 2 joined | Each input action has one callback slot, and the clone overwrote player 1's | Snapshot every action callback before cloning and restore it (`ActionCallbacks.cs`) |
| Player 2's keys also moved player 1 | Both read the same keyboard | Player 2's keys are read from Win32 (`GetAsyncKeyState`) or XInput. A virtual keyboard that carries only player 1's keys can replace the real one (`KeyboardFilter.cs`) |
| Player 2's knife worked once | A shared "melee command" asset never got player 2's reset | Give player 2 its own command copies (`OwnCommands.cs`) |
| A shark grab on player 2 froze player 1 and hid the HUD | Grab sequences only work on "the player" | Skip the game's grab on player 2 and run a mod-side hold with a copy of the game's mash bar (`P2Grab.cs`, `P2MashUI.cs`); the sawtooth's grab is off for both divers |
| Whitetip stuck in its charge animation after player 2 broke free | The game's mash result sends the fish `SetTrigger(UserQTESucces0 / UserQTEFail0)` before `BiteSkillEnd`; the mod only called `BiteSkillEnd` | Send the same trigger, then end the bite during player 2's turn |
| Player 1 panicked when player 2 was grabbed | Animation event `PlayDaveAnimation` starts the battle-QTE animation on "the player" | Run it during player 2's turn when the creature is really on player 2 |
| A tiger shark holding player 2 also grabbed player 1 | Nothing stopped a second grab | Skip grabs on player 1 by the creature holding player 2 |
| Player 1 locked while player 2 played a sushi mini-game | The mini-game owns the input; player 1's Dave only gets input through the game | The mod drives player 1's Dave directly from player 1's own key bindings, and gives a keyboard player 2 a virtual gamepad for the mini-game |

The repo's `src/` has more, and each file starts with the problem it solves.

## QA

- **Test bot:** an in-game bot (`CoopTest.cs`, following [automated-qa.md](../../playbook/automated-qa.md)). Set `[Test] AutoRun = true` and launch the game. It goes title → Continue → normal dive, joins player 2, then for each creature in `Saw,Whitetip,Tiger_Shark` and each diver it spawns the creature and makes it attack.
- **What it asserts:**
  - The right diver is hit or held.
  - The other diver is never locked.
  - Neither diver is still locked, and the HUD is not still hidden, 4 s after the creature leaves.
- **Mutation pass:** the bot then runs the suite again with the mod's grab rules off, and that pass **must fail**. This proves the tests catch the bugs.
- **Results:** `BepInEx\coop_tests.log`.
- **Last result (2026-10-19):** normal pass PASS, 6 of 6. Mutation pass FAIL as required: 5 pass, 1 fail ("P2 still locked 4 s after the creature left").
- **Not covered by the bot:** the sushi bar, and the grab fixes made after that run (tiger double grab, sawtooth animation). Those were only hand-tested or not tested yet.

## How to play

Install BepInEx 6 IL2CPP (`6.0.0-be.788`), start the game once, then build the mod or drop `DaveTheDiverLocalCoop.dll` into `BepInEx\plugins\DaveTheDiverLocalCoop\`. In game, F9 (or LB + RB on a controller) adds player 2. The [mod's README](https://github.com/masonan777/Daves-the-Diver-Coop-Mod#readme) has the controls.

## Known issues

- Player 2 sometimes flashes as if hit when only player 1 was attacked (sawtooth). The mod logs each flash with its cause to track this down.
- Only one mini-game or conversation screen can be open at a time.
- Story scenes, the farm, bosses and other side activities are single-player or untested.

## Next steps

- Read the flash logs and stop player 2's stray flash.
- Extend the test bot to the sushi bar: serve, clean and mini-games with both players.
- Try the "player 2's turn" approach on boss fights.
