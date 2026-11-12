# Shrek 2 (PC, 2004): how the game is built

A full study of the game for the Shrek 2 Enhanced mod. Everything here was read from the owner's own
install (Russian 1C edition, see `NOTES.md` for the identity and hashes) and from scripts decompiled out of
its packages. No game files or code are copied. `<game dir>` is the install folder.

Labels: **Observed** = seen in a file or a log. **Derived** = concluded from observed facts.
**Unknown** = not checked.

## 1. Engine and packages

Observed:
- Unreal Engine 2.5 (package version 129). `Engine.dll` 4.98 MB, 32-bit, the game is a Kuju "KW" framework
  (`KWGame`) with the Shrek layer (`SHGame`) on top.
- Script packages and their size in decompiled form: `Engine` 277 classes (34.7k lines), `GamePlay` 275
  (5.8k), `KWGame` 426 (38.5k), `SHGame` 1,146 (72.6k). Others: `Core`, `Editor`, `Fire`, `GUI`, `UWindow`,
  `AmbientCreatures`, `UDebugMenu`.
- Content: 29 maps, 180 cutscene scripts, 26 texture packages, 26 static mesh packages, 4 animation
  packages (`ShrekCharacters.ukx` holds every character), 26 sound packages, 69 music `.ogg` files,
  17 `.bik` movies (logos and one commercial), Russian dialogue in `AllDialog.Rus_uax`.
- Text tables: `HpMenu` (27.6k lines), `HpDialog` (194.6k), `BumpDialog` (138k), each as `.int` and `.Rus`.

## 2. Class layers

Observed (inheritance, simplified):
- `GameInfo` > `KWGame` > `SHGame`. `KWGame` owns the game-state string (`GSTATE000` style, rides on the
  travel URL), `Login` (finds the placed pawn flagged `bIsMainPlayer` and possesses it, it never spawns
  one), `PostLogin` (HUD) and the persistence helpers.
- `Pawn` > `KWPawn` (6.5k lines, the base of everything that walks) > `shpawn` > `SHCharacter` >
  `ShHeroPawn` (5.7k lines) and `ShEnemy`. Props are `shpawn` > `ShProps`.
- `Controller` > `PlayerController` > `KWHeroController` > `ShHeroController` > `ShrekController` /
  `DonkeyController`. AI: `AIController` > `KWAIController` (3.6k lines) > `Sh_NPCController` >
  `ShEnemyController` > `CombatController` and about 15 specialised combat controllers.
- `BaseCam` (2.3k lines) is a `KWPawn` that is the game camera. The hero controller follows it.
- Cutscenes: `KWCutScene`, `KWCutSceneFile`, `KWCutController`, `KWCutSceneManager`, `CutSceneTrigger`.
- Menus: `ShFEGUIPage` (front end), `ShInGameMenuGUIPage`, `SHPotionSelectMenu`,
  `ShWantedPosterMenuGUIPage`, `Tab_*Options*`, `DriveThroughMenu`, built on the UT2004 `GUIController`.
- 30 game-flow marker classes (`A_mark`..`T_mark`, `One_mark`..`Ten_mark`, subclasses of `GameFlowMarkers`).
  Their use is unknown (derived: labels for scripted sequences).

## 3. Playable characters

Observed: the **same level holds several heroes**, one of them under the player (flag `bIsMainPlayer`)
and the others under `SHCompanionController`.
- `Shrek` (ogre), `ShrekHuman` (handsome, with a sword), `Donkey`, `Steed` (donkey as a noble steed),
  `PIB` (Puss in Boots, sword, ribbon trails), `Mongo` (giant gingerbread man, picks up trees and carts,
  ground shake), `FionaOgre`, `FionaHuman`, `King`, `PrinceCharming`, `ShrekCreature` (the small forms
  from the swamp/creature levels).
- Each hero has its own camera set (`CameraSetStandard`: look-at offset, distance, pitch limits
  -10000..+10000), speeds, animation names for 3 attack steps, run attack, air attack, special attack,
  knock back, throw/drink potion, shimmy, ladder. The small `ShrekCreature` forms are a hero subclass
  (purpose unknown, derived: Shrek turned into a creature by a potion or scene).
- Level contents (observed in the map exports): which heroes are present per level:
  - swamp, carriage: Shrek, Donkey, Fiona; the Hunt (4 parts), FGM Office: Shrek, Donkey, PIB;
  - FGM PIB: Shrek, Donkey, PIB; FGM Donkey: Shrek, Donkey; Hamlet: Shrek, Donkey, PIB;
    Hamlet Mine: Shrek only; the three Prison levels: Shrek, ShrekHuman, PIB, Steed (Donkey is the
    steed there, and there is a fat knight boss in each);
  - Castle Siege: Shrek, ShrekHuman, PIB, Steed, Mongo; FGM Battle: Shrek, ShrekHuman, PIB, Donkey,
    Steed, Fiona (human and ogre), King, Prince Charming and the Fairy Godmother boss.
- Which one is the player in a given level is set per map (`bIsMainPlayer` on the placed actor). Unknown:
  the exact choice per map (the actor property block was not parsed).

Derived: the game was built so that more than one hero already exists in almost every level. A second
player can take over one of the companion heroes without spawning a new pawn type. The owner's plan
(both players are Shrek) needs a second spawned Shrek instead (done in the mod).

## 4. Controls, input and camera

Observed:
- The hero controller is `KWHeroController`. Its input class is `KWPlayerInput` (`ShPlayerInput` in the
  Shrek layer). Both add two input axes the stock Unreal controller does not have: `aArrowRight` and
  `aArrowUp` (the arrow keys), plus mouse X to `aTurn` and mouse Y to `aLookUp`, with the camera driven
  by `BaseCam` (orbit, `fMaxMouseDeltaX/Y` 190/65, pitch -10000..10000).
- The stock Unreal `PlayerInput` sends mouse Y to `aForward` (walk) unless `bLook` or `bAlwaysMouseLook`
  is on. A controller without the game's input class therefore walks forward when the mouse moves
  vertically, never looks up or down, and ignores the arrow keys.
- `UpdateRotation` clamps the pitch at +18000 / -16384 (49152) in the stock controller.
- `User.ini` of the game keeps the key aliases: `Axis aBaseY` forward/back, `aStrafe`, `aArrowRight`
  with `SpeedBase`, `Jump | Axis aUp`, `MouseX/MouseY` as `aMouseX/aMouseY Speed=6`.

Derived (the owner's bug "the second player's camera does not move in all directions"): the joined
client's controller is the stock Unreal one, so vertical mouse movement does not tilt the camera and the
arrow keys do nothing. Fix: give it an input class that sends mouse Y to `aLookUp` and reads the arrow keys.

## 5. Combat

Observed:
- The player has one action key. `ShHeroPawn.DoSomeAction()` picks by state and context, in this order:
  throw the carried object; quick throw; pick up the closest object; throw a switch; use the drive-through
  window; otherwise a combo: `stateAttack1` > `stateAttack2` > `stateAttack3` > `stateAttack3Attack1`,
  an air attack (`stateStartAirAttack` > `stateContinueAirAttack`), a run attack and a special attack.
  It does nothing if the pawn is not `bIsMainPlayer`, is under a cutscene controller, is knocked back, or
  is throwing or drinking a potion.
- Damage goes through `ShHeroPawn.HitSHPawn` / `HitKarmaActor` / `HitSHPropsStatic` and the enemy
  controller's `DamageEnemy`. Hits come from bone positions of the weapon swing
  (`AttackStartBoneNames` / `AttackEndBoneNames`), with ribbon trail emitters.
- Hero health is `Health`; energy bars are the healing pickups; `TakeDamage` knocks the hero back
  (`stateKnockBack` / `stateKnockForward`); death has `stateSwingingDeath` and `PlayerDyingAnim`. That death
  reloads from the checkpoint is derived, not seen.
- Bosses: `BossFGM` (1.2k lines, the Fairy Godmother, several phases with chicken, mouse and frog forms),
  `BossPib` (1k lines, Puss), `BanditBoss`, `FatKnight` (`EnemyBoss`), triggered by `BossEncounterTrigger`
  (options: lock onto the boss, reverse input, keep hero fixed, show boss meter).

## 6. Enemies and AI

Observed: `ShEnemy` has a state machine in its controller with `PreCombatController` (idle, wander,
notice) and `CombatController` (states `MoveTowardHero`, `AttackHero`, `stateTakeDamage`, `stateFaint`,
`stateSpecialAttack`, `stateAirAttack`, `TurnIntoCoward`, `WhereIsTheHero`, `FoundTheHero`, and potion
states `BeAFrog`, `Frozencontroller`, `Sleepcontroller`, `Shrinkcontroller`, `Stinkycontroller`, love).
- Enemy families: peasants (melee, lunger, up-ender, projectile, tough, wimp, coward, child, boy, girl
  and variants), bandits (melee, special, boss), knights (plain, halberd, milk, Mongo, fat, castle hall,
  barrel), elves (projectile, slider, shower, hazmat, ramp), creatures (rat, bat, spider, mouse,
  chicken, frog, slug), a wheel stealer, searchlights, laser beams, vents, snapdragons, apple and
  cherry trees that throw fruit, exploding pumpkins and mushrooms.
- Everything chases and targets one hero: `Level.PlayerHeroActor` (the single global hero, the first pawn
  with `bIsMainPlayer`). Enemy code reads that field directly in many places, so a second player is
  invisible to the AI unless it is made to see him.
- Companions use `SHCompanionController` (`KWAIController` > `SHAIController`) and `CompanionTrigger`
  (types: follow, wait in corral, cheer, switch hero, enemy focus). Their follow radius and tolerance are
  trigger properties.

## 7. Items, potions, coins

Observed:
- Pickups are `KWInventoryCollection` subclasses (coins, coin stacks, money bags, energy bars, boxes of
  bars, energy kegs, mouse and pixie potions, strong and weak mice potions).
- Nine potions (`Potion1sh`..`Potion9sh`, collections `Potion1Collection`..), chosen in
  `SHPotionSelectMenu` (button mapping in `ShHeroController.UsePotion`: selection indexes 0, 5, 2, 3, 4,
  1, 6, 7, 8 map to potions 1..9). By the music files the effects are: freeze, frogs, invisible, love,
  nighty-night (sleep), shrink enemies, shrink hero, stinky enemies, strength. Strength has a
  first-time hint (`...FirstStrengthPotion` per hero in `User.ini`).
- Inventory is stored on one carrier pawn, `Level.InventoryCarrierPawn`, not per hero.
- Coins count for the coin bonus level and the end of game dialogue (`CoinBonusGame` with a time limit,
  `Beanstalk_bonus*` maps).
- Throwable and pickable props: carts, trees, wheels, pumpkins, rocks, apples, milk, cards, feathers; a
  slot machine; a drive-through window (`DriveThrough`); breakable crates and bars; bounce mushrooms;
  switches; ladders and shimmy vines.

## 8. Levels and game flow

Observed (map names, size, trigger counts from the exports):

| Map | Size | Cutscene triggers | Checkpoints | Notes |
|---|---|---|---|---|
| `Entry`, `SH2_Preamble` | tiny | 0 | 0 | startup; preamble jumps to `4_FGM_PIB` |
| `Book_FrontEnd`, `Book_StoryBook` | 170 KB | 0 | 0 | the book menu |
| `Book_Story_1..4` | 190-230 KB | 0 | 0 | storybook interludes (1: before swamp, 2: before the Hunt, 3: before Prison, 4: before credits) |
| `1_Shreks_Swamp` | 1.9 MB | 31 | 0 | 82 coins, 2 start spots |
| `2_Carriage_Hijack` | 0.9 MB | 5 | 0 | wheel stealer, carriage |
| `3_The_Hunt_Part1..4` | 3.5-4.5 MB | 8/5/3/12 | 2/1/1/3 | PIB boss at the end of part 4 |
| `4_FGM_Office`, `4_FGM_PIB` | 0.3 / 1.7 MB | 0 / 14 | 0 / 4 | Fairy Godmother factory |
| `5_FGM_Donkey` | 1.6 MB | 4 | 3 | Donkey level |
| `6_Hamlet`, `_Mine`, `_End` | 2.4 / 2.2 / 5.4 MB | 4 / 3 / 2 | 2 / 3 / 5 | boss encounter at the end |
| `7_Prison_Donkey`, `8_Prison_PIB`, `9_Prison_Shrek` | 7-9 MB | 11 / 6 / 5 | 5 / 5 / 3 | fat knight bosses |
| `Beanstalk_bonus` (+ `_dawn`, `_knight`) | 0.6 MB | 0 | 0 | coin bonus rounds |
| `10_Castle_Siege` | 1.5 MB | 8 | 1 | Mongo |
| `11_FGM_Battle` | 1.6 MB | 4 | 1 | final boss |
| `Credits` | tiny | 0 | 0 | back to swamp |

Derived story order: `Book_Story_1` > swamp > carriage > `Book_Story_2` > the Hunt 1-4 > FGM office >
FGM PIB > FGM Donkey > Hamlet (mine, end) > `Book_Story_3` > prison Donkey > prison PIB > prison Shrek >
(beanstalk bonus) > castle siege > FGM battle > `Book_Story_4` > credits. The map names found as
strings inside other maps also include debug jumps, so only the clean pairs (storybook > level, credits >
swamp) are certain. Unknown: the exact `NewMapName` of each `ChangeLevelTrigger`.

Level change = `ChangeLevelTrigger` (properties `NewMapName`, optional `NewGameState`) that fades the screen
and calls a travel. `GameState` strings (`GSTATE000`...) mark story progress and ride on the URL.

## 9. Cutscenes and dialogue

Observed:
- Cutscenes are plain text scripts, one per scene (`System\Cutscenes\*.int`, 180 files), played by
  `KWCutController` / `KWCutSceneManager`. Verbs counted over all files: `WaitForCue` 2,193, `Cue` 1,872,
  `Sleep` 762, `FlyTo` 513, `TargetFlyTo` 481, `Say` 462, `WaitForSay` 458, `FOV` 269, `PlayAnim` 262,
  `TurnTo` 204, `Trigger` 173, `Teleport` 162, `WaitForAnim` 158, `WalkTo` 147, `Face` 110, `LookAt` 89,
  `RunTo` 87, `PlaySound` 68, `PlayMusic` 52, `StopMusic` 51, `FadeTo` 31, `Shake` 14, `FadeIn` 14,
  `FadeActor` 12, `NoBypass` 15, `SaveGame` 11.
- `PawnTag=MAIN` in a sequence names the actor taken over from the player. During a cutscene the hero
  controller is replaced by a cutscene controller and put back afterwards (`PreCutPossess`).
- Dialogue lines are keys (`pc_dnk_CarHijack_34`) resolved through `HpDialog` (text and speaker) and the
  Russian audio in `AllDialog.Rus_uax`. Side comments ("bump lines") come from `BumpDialog` and are played
  by `BumpLineSpeaker` actors per situation (low health, coins, potions, swimming).
- Subtitles are drawn by the cutscene HUD, replaced in the mod by `EnhancedCutText`.

## 10. Menus and options

Observed: front end in `Book_FrontEnd` (3D book, `SHMenuBook`), wanted-poster menu, in-game menu with
controls and video/sound/game option tabs, potion select, drive-through menu, credits page. Mouse
sensitivity, key bindings, video and sound are `globalconfig` values in `User.ini` / `Game.ini`. A search
for "difficulty" in the scripts found only small per-prop `eDifficulty` enums (apple trees, elf throwers),
no global difficulty setting. No multiplayer, join, host or split-screen text exists in `HpMenu`.

## 11. Saves and persistence

See `NOTES.md` ("Save system" and the save rework sections): the engine writes the whole level package per
slot (`Save<n>.usa`); `PersistentStore` (travel-carried key/value store) holds story flags; `SHSavePoint` is the
checkpoint fairy; the mod replaces the file with a small checkpoint record.

## 12. Sound, music, HUD

Observed: each level has a music track (`3_TheHunt_Part1.ogg` ...) plus `KWMusicTrigger` actors; music is
switched for the potion duration and restored (`PrePotionMusic`); sound effects are per character
(`Shrek.uax`, `Donkey.uax`, `PussInBoots.uax`, `Mongo.uax` ...). The HUD (`SHHud`, `HudItem` x6) draws hearts,
coins, potion timers (`SHHudPotionTimer`), boss meter, load and save tiles, "times up" bar for the bonus.

## 13. Networking state of the stock game

Observed: the engine's net code (IpDrv, replication blocks in `Engine.u`) is present. `KWGame`, `SHGame`,
`GamePlay` contain **no `replication` blocks**. Only three places read `Level.NetMode`. The game has no
multiplayer mode. Anything co-op is therefore added by the mod: a game class that spawns the second hero,
replication of pawns' animation state, a client controller, a patched `Engine.dll` for level changes.

## 14. Consequences for co-op (derived)

1. One global hero (`Level.PlayerHeroActor`) is read by AI, triggers, saves, HUD, music, cutscenes. Each
   such use needs a "nearest player" or "all players" rule.
2. Inventory and potions live on one carrier pawn: shared by both players unless split.
3. Cutscene scripts name the actor by `PawnTag=MAIN`; player two must be parked or cast (done: he watches
   from a placed spot).
4. The stock hero controller needs actors that exist only on the server, so a joined client uses its own
   controller (`EnhancedPlayerController`) whose input must match the game's (section 4).
5. A level change closes the client's connection; the patched engine keeps the old level on screen and the
   client retries until the host is ready.

## 15. Not yet studied (unknown)

- The exact player hero and `NewMapName` per map (actor property blocks not parsed).
- Individual enemy numbers (health, damage), per-level enemy counts (they spawn through
  `ShGenericSpawner` and `EnemySpawnDirector`).
- Beanstalk bonus rules and rewards in detail, Karma physics objects, the `SaveTimer` autosave cadence.
- The Russian dialogue audio layout and the lip-sync data.
