# Nova Drift - working notes

## Game identity (observed 2026-11-16)
- Store: Steam, app id 858210, build id 25890040
- `NovaDrift.exe` FileVersion 1.2.38.4 (Chimeric), SHA-256
  `6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d`
- Engine: GameMaker, 64-bit runner (`steam_api64.dll`, `Steamworks_x64.dll` extension)
- OS: Windows 11

## Observations
- `data.win` (435 MB) chunks: GEN8 OPTN LANG EXTN SOND AGRP SPRT BGND PATH SCPT GLOB SHDR FONT
  TMLN OBJT FEDS ACRV SEQN TAGS ROOM UILR DAFL EMBI PSEM PSYS TPAG TGIN FEAT STRG TXTR AUDO.
  **No CODE, VARI or FUNC chunk -> YYC build** (GML compiled into the exe), same class as Forager.
  GEN8 bytecode byte = 17; FEAT/PSEM/PSYS/UILR chunks mean a recent GMS2 runtime (2023+).
- Game already loads Steamworks (`steam_api64.dll`), so lobbies/P2P may be reachable from a DLL
  the way Forager's mod does it (unverified for this game).

## Research (2026-11-16)
- This repo: no directory entry, no `games/` folder. Issue/PR search via GitHub API
  (`repo:cranebeaker/coop-anything nova drift`): 0 results.
- Web search "Nova Drift co-op multiplayer mod": no co-op mod found.
  Only mod found: Tweaks for Nova Drift by YellowAfterlife
  (https://yellowafterlife.itch.io/nova-drift-tweaks), single-player options; a comment says it
  no longer works on current builds (unverified).
- Steam discussions (2019-2020): developer said multiplayer is out of scope for this game,
  maybe in a sequel. https://steamcommunity.com/app/858210/discussions/0/1798529872656698258/ ,
  https://steamcommunity.com/app/858210/discussions/0/3819669605963071642/
- Not yet checked: Nexus, ModDB, GitHub code search, Nova Drift Discord, Reddit, archive.org.

## Derived
- Path: build from zero (no existing attempt found by the searches above).
- Closest template: Forager Co-op Mod (games/forager): `version.dll` proxy + MinHook, host
  authority, puppets for remote players, Steam P2P. Runtime structs may carry over in shape;
  addresses will not (different runtime version, 64-bit).

## Unknowns
- GameMaker runtime version; whether the exe is packed/protected (SteamStub?).
- How the player ship, upgrades (mods/gear) and wave spawning are structured.
- Save location and format.

## Phase 3 start: structure (observed 2026-11-16, build 25890040)
- `NovaDrift.exe`: PE x64, timestamp `0x6accf84d`, sections .text .rdata .data .pdata _RDATA
  .fptable .rsrc .reloc. **No `.bind` section: not SteamStub-wrapped.**
- **UndertaleModTool CLI 0.9.2.0 cannot load `data.win`**: it throws
  `IndexOutOfRangeException ... at 00000040 while reading chunk OBJT` in
  `UndertaleChunkOBJT.CheckForManagedField`. Cause seen in the raw chunk: OBJT has 864 entries
  and **entry 0 is a null pointer**; UMT reads the first object to probe the format.
  (Possible UMT bug report; not filed.)
- A minimal read-only parser (names only: OBJT, SCPT, ROOM pointer lists, skipping nulls)
  works: 864 objects, 2773 scripts, 14 rooms.
- Rooms: init roomMenu roomLearn roomGlossary roomUnlocks roomChangelog roomSettings
  roomCustomizeKeyboard roomCustomizeGamepad roomLoadout roomGame roomCredits
  roomHighscoresDynamic roomGameEnd. The whole run happens in `roomGame`.
- The exe holds 6460 `gml_*` name strings (2133 `gml_Script_*`), as the YYC
  name-binding trick in engine-families.md expects.
- Key objects: `oPlayer` (Create, PreCreate, Step_0, Step_2, Alarms 0-11, Collision with
  oComet/oEnemyBeam/oEnemyBlast/oEnemyProjectile/oMineShield/oSolid, Destroy),
  `oController` (Step_0/1/2, many KeyPress debug keys, Draw_64), `oEnemySpawner`,
  `oSaveRunFileManager` (mid-run save), structs `enemySpawnInfo`, `buildRunData`/`saveRunData`,
  `playerBuildInfo`. Player-side objects: oPlayerProjectile, oPlayerBlast, oPlayerConstruct,
  oPlayerDroneProjectile, oPlayerShield, oPlayerSegment.

## Guesses (unverified)
- Enemies probably target via `instance_nearest(oPlayer)` or a cached `oPlayer` id, so how
  they pick a target decides whether a second ship is cheap or expensive.
- A host-authority model (host runs spawner and enemies, guest sends input and gets
  snapshots) looks like the right fit for a fast arcade shooter with heavy randomness.

## Naming functions without full Ghidra analysis (2026-11-17)
- **GML functions:** each compiled `gml_*` function references its own name string with a
  RIP-relative `lea`/`mov`; map the reference to its enclosing function with the exe's `.pdata`
  (x64 RUNTIME_FUNCTION) table. Result: 5461 of 6460 names bound, e.g.
  `gml_Object_oPlayer_Step_0` 0x140c4a650, `gml_Object_oPlayer_Create_0` 0x140c41ce0,
  `gml_Script_setPlayerTarget` 0x1405641a0, `gml_Object_oEnemySpawner_Create_0` 0x140adfe00,
  `gml_Object_oController_Step_0` 0x14097e5f0.
- **Runtime builtins** are registered as
  `xor r9d,r9d; lea rdx,[func]; lea rcx,["name"]; lea r8d,[r9+argc]; call Function_Add`
  (seen at 0x140f5b599). **The function pointer comes before the name.** A first scan that
  paired each name with the following `lea rdx` was off by one registration everywhere
  (it labelled `instance_exists` as `instance_number`). Strict pattern: 1150 builtins bound, e.g.
  `instance_number` 0x140f56d80, `instance_copy` 0x140f561d0, `instance_create_depth` 0x140f56310,
  `instance_nearest` 0x140f56ce0. Checked by decompiling `instance_number` (reads arg 0 as
  object, -1/-2 = self/other, returns REAL) and `instance_copy` (allocates a 0x208-byte
  instance, copies x/y and variables, optional create events, returns kind 15 = REF).
- Builtin ABI (x64): `void F(RValue* result, CInstance* self, CInstance* other, int argc, RValue* args)`.
  Script ABI: `RValue* gml_Script_X(self, other, RValue* result, int argc, RValue** args)`.
  RValue = 16 bytes: 8-byte value, u32 flags, u32 kind (0 REAL, 15 REF).
- Ghidra 12.1.4 headless with `-noanalysis` plus a script that labels these names and decompiles
  only chosen functions takes about a minute. Ghidra 12.2 needs JDK 25 (`application.java.min=25`).
- `oPlayer` = object index 603. Runtime helpers called by every GML function (e.g. 0x14002bdb0,
  0x140f40640, 0x140f17670) are still unnamed; decompiled GML is hard to read until they are.
- `gml_Object_oPlayer_Step_0` is too big for the decompiler ("Response buffer size exceeded").

## Probe 0 results (2026-11-17, build 25890040, one PC, human at the keyboard)
Probe: `version.dll` proxy (exports forwarded to `C:\Windows\System32\version.dll` by absolute
path) + MinHook 1.3.4 hooks on `gml_Object_oPlayer_Step_0` and `gml_Script_setPlayerTarget`;
F9 calls the builtin `instance_copy(true)` with the player as self. Test copy started directly
with `steam_appid.txt` = `858210` (Steam client running).
Observed:
- The proxy loads, the PE timestamp check passes, both hooks install (MH_OK).
- An `oPlayer` already exists in the main menu (instance_number = 1 before any run).
- `instance_copy` on the player works mid-run: returns kind 15 (REF), instance_number goes up,
  every copy runs its Step at 60 Hz. 27 ships at once with no crash in the first ~20 s.
- All copies obey the same local input and move as a formation (screenshot): input is read
  globally, as in Forager.
- `setPlayerTarget` is called in bursts (130 calls from 25 different instances in one second,
  then 0), not every frame: it looks like a one-off target pick (e.g. on enemy spawn), not
  per-frame tracking. Unknown which ship it picks with several players.
Not tested: whether enemies hit/target the copies, what happens when a copy dies, score/XP/upgrade
ownership, game over with several ships.

## Second ship with its own controls (2026-11-17, build 25890040, one PC)
Tool: `coop_local` DLL (separate mod repo, local only for now). Test runs used a save redirect (see
below), the bot (`ND_BOT=1`), and window messages to start a run; never the owner's input.

### Save isolation (important for every future agent)
- **The game saves to `%LOCALAPPDATA%\NovaDrift\save.ini`, and a copied test install shares it.**
  Steam Cloud syncs that folder (`steam_autocloud.vdf`). Our first test sessions changed the owner's
  account XP/level/lifetime score before this was noticed (owner chose to keep the changes).
- The game gets the folder from `SHGetFolderPathW(CSIDL_LOCAL_APPDATA)` (its only shell32 import).
  Hooking that from the proxy DLL (shell32 loaded in DllMain) redirects all writes; verified by
  hashing the real save.ini before/after each run (unchanged in every run since).
- The game also writes a `save-<timestamp>-0.bak` on each launch.
- Suggest a playbook note: copied installs do not isolate saves for games that save under AppData.

### Input map (observed)
- Every frame `oPlayer` Step_0 asks `inputButtonHeld/Pressed` and `gamepadHeld/Pressed` (GML
  scripts) with binding codes, and calls the builtin `mouse_check_button` directly (2 per frame).
  Call sites (return addresses): `0x140c4bdc8` held(65 'A', mb_left), `0x140c4ca02` held(83 'S', mb_right),
  `0x140c4ca8c` gamepadHeld(RT), `0x140c4f09c` pressed(Tab) + gamepadPressed(dpad up) `0x140c4f12c`,
  pressed Q/W/E/R/T `0x140c4f7ec..0x140c4fb34`, gamepadPressed dpad left/right/face3
  `0x140c4fc1e/0x140c4fd08/0x140c4fdf2`, held(39)/held(37) `0x140c51ef7/0x140c51f1d`.
- Overriding only the scripts did not make player 2 thrust; overriding `mouse_check_button(2)` for
  player 2 did (speed 10-25 with thrust held). So thrust in the default scheme is the right mouse
  button read by the builtin.
- Settings in save.ini `[settings]`: `controlScheme`, `directionalControlScheme`, `disableMouseAim`,
  `thrustControlScheme`, `gamepadSteeringLeftStick`, `customControls`. Defaults: mouse aim.
- The ship never reads `mouse_x`/`mouse_y`; `oController` Step_0 -> `inputControl` reads them once per
  frame. Neither `point_direction` nor `point_distance` is called during the ship's Step.
- CInstance float fields (from diffing): `+0xf4/+0xf8` x/y, `+0x104/+0x108` xprevious/yprevious,
  `+0xe8` facing (image_angle), `+0x10c` direction, `+0x110` speed, `+0x120/+0x124` hspeed/vspeed,
  `+0x128..+0x134` bbox.
- **Setting player 2's `+0xe8` before and after its Step steers it:** its velocity direction follows
  the set angle (bot aim rotating 60 deg/s, velocity trailing 40-60 deg, as inertia predicts).
  Not checked: whether its shots follow the same angle (screenshots inconclusive).

### Builtin registration, second form
`mov r9b,1; lea rdx,[func]; mov r8d,argc; lea rcx,["name"]` (e.g. `gamepad_axis_value` at
0x141129ba8). Looking back up to 16 bytes from each `lea rcx,[name]` for `lea rdx` binds 2936 builtins
(first form alone: 1150). `gamepad_is_connected` 0x141128850 (decompiled: checks the runtime's pad
table), `gamepad_axis_value` 0x141128550, `gamepad_button_check` 0x1411285d0.
- The owner's DualSense was listed by Windows but `gamepad_is_connected(0..11)` was 0 for all slots
  in the test copy (started outside Steam). Cause unknown (pad off/asleep, or Steam holding it).

### Death is shared
- Both ships are destroyed by `gml_Script_checkDestroyPlayer`, called from `gml_Object_oPowers_Step_2`
  (stack 0x1400d8084 / 0x140c85786), 19 ms apart, when the bot flew player 2 into enemies.
- Derived (not verified): hull/shield stats are global, so both ships share one health pool and
  one death ends the run for both.

## Separate health per ship (2026-11-17, owner's choice)
### Observed
- Health lives on the run's **oPowers** instance (object index 621), not on oPlayer and not in
  globals: `hull`, `hullMax`, `shield`, `shieldMax`, `shieldCooldownTimer`, `partialCooldown`,
  `shieldRecoveryCount`, `shieldUp`, `shieldWasDown`, `shieldbreak`, `hullReserved`, `hullDecimal`,
  `shieldDecimal`, `shieldReserved`, `regenTotal`. The ship has its own `iFrames`.
- Read with the game's `variable_instance_get/exists` builtins and hand-built strings
  (`RefString {const char*, int refCount, int size}`, kind 1). Listing variables: test every identifier
  string from the exe with `variable_instance_exists` (13156 names, 16 ms). **Skip `phy_*`, `view_*`,
  `xview`/`yview`/`xport`/`yport`..., `ztestenable`/`zwriteenable` and registered builtin names**:
  reading some of them on a normal instance crashes the game (seen twice). oPowers has 1373 numeric
  variables, oPlayer 68, global scope 470.
- Hits: oPlayer `Collision_*` events change `shield` (then `hull`) directly and set
  `shieldCooldownTimer` = 390 (6.5 s) and `partialCooldown`. oPowers Step_2 then counts the timer down
  1/frame, refills the shield 5 % of max per frame at -1, breaks it (`shield<0` -> 0, `shieldUp` 0,
  `shieldWasDown` 1, `shieldbreak` 1), and regenerates hull (about 0.0333/frame = 2/s).
- 28 other objects damage or heal the ship in their own `X_Collision_oPlayer` events (player = other).
- Death: oPowers Step_2 -> `gml_Script_checkDestroyPlayer` (0x1400d67a0) destroys every oPlayer.
- A run killed mid-way resumes from `<save dir>\NovaDrift\continue`; a run saved while player 1 was
  "down" (hull < 0) resumed with an instant death.

### What the prototype does (bot-tested, one PC)
- Player 2's copy of the health variables is swapped into oPowers around every event player 2 takes
  part in (its Step_0/Step_2/alarms/collisions, and the 28 `X_Collision_oPlayer` events with
  other == player 2), so the game's own damage code applies to player 2's pool.
- Player 2's regeneration/shield break follow the rules above (written in the DLL, not the game's).
- Player 2 at 0 hull: destroyed outside any event (`instance_destroy`, 0x140f56970), respawns after 5 s.
- Player 1 at 0 hull while player 2 is up: `checkDestroyPlayer` is skipped; player 1 is hidden
  (`visible`), its events are skipped, and it respawns after 5 s at player 2's position. Game over
  only when both are down.
- Bot run, 75 s: player 2 died and respawned twice, player 1 went down and came back once, and
  each ship's hits changed only its own pool (log `health P1 ... | P2 ...` once per second).
### Unknown / not verified
- Player 1's shield sometimes refilled only partly (to 20) in co-op runs; not compared with a
  solo baseline yet.
- Damage paths that do not go through these events (area effects, damage-over-time scripts such as
  `hullDamageDOTSelf`) still hit player 1's pool.
- Upgrades that read hull/shield outside these events (e.g. emergency systems) see player 1's values.

## Owner's requirements for online co-op (2026-11-17)
1. Online only (no local second player).
2. Shared XP: experience from every kill goes to both players.
3. Per-player level-up choices: each player picks their own modifiers and their own ship body,
   weapon and shield.
4. Modifiers that spawn helpers (drones, turrets, allies, swords...) belong to and act for the player
   who took them.
5. A pre-run lobby: the host picks the run modifiers (challenge modes / game mods), the other players
   see the choice live before the run starts.
Design consequence (derived): the whole build state in oPowers (1373 numeric variables plus
non-numeric ones), not just health, has to exist once per player and be swapped in around each
player's events, the same way health already is. Upgrade choices become lockstep inputs.
6. Lobby flow (owner's spec): a "Multiplayer" item directly under "Game Start" on the main menu
   opens a lobby: all players listed, the host's run-modifier choices visible to everyone, a ship
   colour per player (tints ship, spawned helpers, shots, shield, thruster flames). Friends join via
   the Steam overlay into the host's lobby. Non-host players toggle "Ready Up"; when all are ready the
   host's "Start Game" starts the match.
7. Enemy targeting: each enemy picks a player at spawn and keeps that player until the player or the
   enemy is destroyed.
8. The game's own hull/shield bars show the local player's ship.
9. The enemy health bar at the top shows the enemy the LOCAL player last hit (needs per-player ownership of shots; do with step 4).
   Clarified by the owner: this is the small grey enemy bar at the top of the screen. The large,
   colourful boss health bar stays unchanged (shared).

## Deterministic lockstep (2026-11-17, build 25890040, two copies on one PC)
### Observed
- **The simulation is deterministic.** With the same seed and the same per-frame ship inputs, two
  copies stay bit-identical (object, x, y of every gameplay instance, plus health) for whole runs:
  9721 frames to game over at 4x speed, 8101 frames at normal speed, every run since the fixes below.
  The random-draw count per frame also matches.
- Fixes that were needed:
  1. `randomize()` runs once at launch; the menus then consume random numbers for as long as each
     player stays there. Reseed (`random_set_seed`) when `room_goto` enters `roomGame`. **room_goto's
     argument is an asset reference (kind 15, index in the low 32 bits), not a number** (room 10).
  2. Background decoration (`oController` Alarm_3 -> `spawnNebulae` -> oNebula/oStarBright/
     oCelestialBody*) spawns at times that differ between PCs (one copy drew 134 extra random numbers
     at frame 4957). The core generator is `0x1410b70e0` (real) / `0x140f53280` (int), state = 16 dwords
     at `0x141e42f30` + index at `0x141e42f28` (see `random_set_seed`). Saving/restoring that state
     around all decoration events and `spawnNebulae` keeps gameplay randomness identical.
  3. Compiled GML does **not** call the `random`/`irandom` builtins; hooking the builtins sees nothing.
- Cosmetic objects that legitimately differ between PCs (exclude from state hashes): oCelestialBody*
  329-331, oNebula 594, oStarBright 767, oGuiCursor 523, oSoftwareCursor 741, oDamageFloater 388.
- `object_index` read with `variable_instance_get` is an asset reference (kind 15) too.
- Wall-clock reads during a run: `current_time` (background layers, oController Step_0) and
  `get_timer` (run autosave). Neither affected gameplay in these tests.
- Faster tests: `game_set_speed(240, gamespeed_fps)` (0x140f61fb0) with VSync off runs the same frames
  4x faster; results are frame-for-frame the same.
- Two copies on one PC: start both with `steam_appid.txt`; drive menus with posted window messages
  per process; capture screenshots with `PrintWindow` (never the screen: the owner may be using it).
### Lockstep prototype
- Each PC samples only its own ship's input (aim angle, fire/thrust/ability bits) for frame f+3,
  sends it with the previous 7 frames (loss-tolerant), and waits for the other ship's input for frame f
  at the end of frame f-1 (oPowers Step_2). Over UDP on one PC: identical for 5221 frames, ~1 packet
  per frame each way.
- Display may differ per PC (local player's own hull bars via a health swap during `tweenHullBars`
  and oController Draw_64, hit flash only for the local ship) without breaking sync.
- Enemy targeting: `chooseTarget` (0x1400e7a60) writes targetX/targetY/targetHspeed/targetVspeed on the
  enemy and **always picks player 1**. A per-enemy `coopTarget` (nearest ship at the first call, kept
  until that ship is down) redirects it.
- Game over runs `checkDestroyPlayer` once per ship -> two oDrawEndScore (398) score screens, each with
  its own random "Tip:"; the second is destroyed.
10. Unlocks: lobby run modifiers come from the host's unlocks; the level-up upgrade pool comes from either the host's unlocks or each player's own (lobby option).
11. Upgrades: points are stored and spent with Space; opening the upgrade screen pauses the game for both players (owner's choice).

## Per-player builds: ownership layer (2026-11-17)
- **oPowers is the player's "brain"**, not just stats: Alarm_2 fires the weapon (`shoot2`), Step_0 reads
  inputs, charges weapons, runs drones; Step_2 does health, regeneration and `checkDestroyPlayer`.
- **One event executor runs every event:** `FUN_14112c000(CCode** code, CInstance* self, CInstance* other)`.
  `Perform_Event(self, other, type, sub)` is `FUN_1410d5030`; object index at `CInstance+0xc0`, id at `+0xbc`.
- `with (obj)` compiles to `FUN_140f2d960(iter, &self, &other, RValue* target)` (returns count; iter =
  {self, other, listStart, listCursor}; the list ends at a 0 sentinel), `FUN_140f2e080` (next),
  `FUN_140f2d630` (end). Writes to another object's variables are compiled as `with` loops.
- `obj.var` reads: `FUN_140f17950(RValue* target, varId, arrayIndex, RValue* out, 0, 0)` -- **six
  arguments** (a four-argument hook breaks the game with "REAL argument is unset"). A kind-6 RValue
  holding a CInstance* targets one instance.
- Ownership design (works, two copies identical for 10081 frames): instances created by the builtins
  `instance_create_depth/layer`, `instance_copy` while a player is active are tagged with that player;
  the event executor sets the active player from the owner of self (else other); `with`/reads/
  `instance_find` of oPlayer (603) and oPowers (621) resolve to the active player's instances. Player 2
  gets its own oPowers (created fresh while player 2 is active) and fires its own weapon.
- Hooking the instance constructor `FUN_140f09a20` directly broke the game (argument types unclear);
  hooking the creation builtins is enough.
- GameMaker runtime errors go through `FUN_140f2cd50(fmt, ...)` and end the game without a Windows
  crash record; hooking it to log the message saved a lot of time.
- Level-ups: `oController` Step_0 -> `expControl` -> `gainLevelUps`; `global.level`,
  `global.effectiveLevel`, `global.levelup` (unspent points). Some per-level stat growth is written
  only to the first oPowers.
- Input sites outside the ship: oController Esc(27) pause, Space(32) open upgrades, Backspace(8)
  decline; oUpgrade Shift, arrows, R(82) reroll, Enter(13), Space. In networked play every one of these
  must come from lockstep input, otherwise one PC's real keyboard desyncs the game.
- `obj.var = value` compiles to `FUN_140f17e50(RValue* target, varId, arrayIndex, RValue* value)`, which
  writes **every** instance of the object. With two oPowers a hit on one player damaged both until this
  setter was redirected to the active player's instance as well (found by logging both players' hull
  around every event in a 30-frame window: one ship collision changed both shields).
- Run the lockstep tick on oController (Begin Step: inputs, End Step: frame end). oController exists
  once and stays active; oPowers' end step runs once per oPowers (twice with two players).
- In controlled runs any input read that is not a ship/brain/upgrade-screen site must answer "not
  pressed": oController reads the arrow keys (inputControl) and a real key on one PC desynced a test.
- Test copies must not capture the owner's mouse: hook `window_mouse_set(_locked)`, `display_mouse_set`,
  `ClipCursor`, `SetCursorPos`.
- Ownership-mode result: separate hull/shield per player, independent down/respawn, two copies
  identical for 9181 frames (4x speed).
12. Each upgrade applies only to its chooser; each player has their own randomized offers; the pause screen lists only the local player's upgrades.
- Upgrade flow in co-op (bot-tested, identical copies): `global.levelup` is held at 0 outside the
  upgrade screen; each new point is credited to both players' own counters. A networked Space press
  opens `gotoUpgrade` for that player on both PCs at the same frame with `global.levelup` = their
  points. oUpgrade (828) input sites answer from the chooser's networked keys; mouse position/button
  builtins return constants in controlled runs. **gotoUpgrade creates oUpgrade a frame later, with no
  player active**, so instances created while the screen is open (world paused) are tagged as the
  chooser's; then the chosen weapon/body/modifiers change only the chooser's oPowers (builds differed
  in 132 variables after four rounds; separate `mods`/`banishMods`/gear lists per oPowers).
- Modifier objects (`m*`, 189 kinds) apply themselves via `gainMod` and then disappear.
13. Reroll and banish points per player; banishes only affect the banisher; wild upgrades that react to future picks are tracked per player.
- Rerolls: `rerolls` lives on each oPowers (per player); global `rerollCount` is only a run statistic.
  `banishUpgrade` (Shift on the upgrade screen) reads its conditions from oPowers and ends in a
  reroll; `banishMods`, `wildMods*` are lists per oPowers. Bot banish attempts were declined by the game
  (nothing banishable selected / not unlocked), so per-player banish is not exercised yet.
- Level-ups: run `gainLevelUps` once per player with that player active (global level counters advance
  once); the second pass creates oPulseLevelup (634, kept: it acts at that player's ship), oBanner (255)
  and oPowerupLevelUp (623) (display duplicates, removed). Level-scaled stats then match per player.
- Pause in co-op: oController's Esc site answers from the network (both PCs pause the same frame via
  `pressPause` 0x140460770 / `pressUnpause` 0x14046ac70); while paused its menu keys and the mouse come
  from the pauser. The pause screen's stats and hexes are drawn in oController's GUI event with the local
  player active, so each PC shows its own player's build (verified: player 2's weapon and rerolls).
  Known gap: the pauser's navigation also drives Settings on the other PC.

## Real-input copy vs bot copy (2026-11-17)
### Fixed
- The ship's launch state depends on time spent on the START screen (x/y, `xprevious`/`yprevious`,
  `xLast`/`yLast`, `xstart`/`ystart`, launch effects oBurstGrow 303, oBurstThrust 317,
  oBurstThrustDamaging 318, companions oPlayerGlow 611, trails 616/617, oSwordController 791). The mod
  sets all of them to fixed values at the first frame of a co-op run; frame 0 then matches.
- Sounds: `playOnScreen` and friends pick random variants only when the source is on screen, and the
  view differs per PC (camera leans to the cursor). All 17 sound scripts now run with the generator
  state saved/restored.
- Every mouse builtin (`device_mouse_*`, `display_mouse_get_*`, `window_*mouse*`, wheel, buttons) and the
  raw keyboard builtins answer neutral values (or the chooser's/pauser's networked values) during runs.
- The test runner's leftover Return press after START could change the START screen setup.
- Input control starts at `room_goto(roomGame)`, before any run object steps.
### Open
- With one copy on real mouse/keyboard (`run_pair -Human`) and one bot copy, the human copy still
  diverges at frame 1 in most runs (the bot copy's frame-1 hash is the same every run). Player 1's ship
  moves ~0.56 px differently during frame 1; all ship variables are equal at launch except the ones now
  normalized. Not found yet. Bot-vs-bot runs stay identical.
- Upgrade order: `global.upgrades`, `upgradesPurchased`, `upgradePurchased` count picks for the whole
  run and decide whether weapon/body/shield are offered first. The mod swaps in the chooser's own
  counters while their screen is open (verified: separate counts, run total kept).
- Ownership resolution now covers every object type that has owned instances: in a player's context,
  `with(obj)` keeps that player's instances (or drops the other player's), `obj.var` reads/writes go to
  that player's instances (per-object registry of owned ids, filled in the creation builtins).
- Shared drawers: a `with(oPlayer)` loop started with no player active runs each iteration as that
  ship's owner (hooks on the with next/end functions 0x140f2e080 / 0x140f2d630), so code drawing every
  ship reads each ship's own build. Visual result not yet confirmed by a person.
- Ship variables are identical between players even with different bodies (hull drawn elsewhere).
- Frame 0/1 split lead (from the owner): with real players, the first copy to press START waits for
  the other before play begins; the bots start immediately. The waiting period may read input or
  advance timers on one side only. Check this when returning to the split.
- Real-time leaks into gameplay (found by world-dump diffs of two copies at normal speed over UDP):
  `delta_time` is read by shieldDamageDirect, oEnemySpawner Alarm_1, oHost Other_10, oHostDrone Create;
  `fps` and `fps_real` by oPlasmaShot Step_0. Fast-forward tests hid this. The mod reports one exact
  60 fps frame for all three during runs. `current_time`/`get_timer` are read only by run-save code.
- While an upgrade screen is open the game deactivates instances: instance_number / instance_exists /
  with() see none of them then.
- Player effects (oShield 705, oPlayerGlow 611, oPlayerTrailL/R 616/617) are rebuilt for each brain
  after every upgrade (refreshEffects -> createShieldObjects / createPlayerTrails). createPlayerTrails
  skips creation when trails already exist, and the rebuild clears old effects through with() over
  parent objects, so both need per-player filtering.
- `current_time` is read by gameplay too (checkDestroy, and the pause flow). During runs the mod reports a
  game clock: 100000000 ms + frame * 1000/60. Verified: two copies over UDP at normal speed stayed
  identical for 33061 frames (9+ minutes).

## Per-player visuals (2026-11-17, confirmed on screen by the owner)
### Observed
- Reads of a built-in variable on another object (`oPlayer.x`) go through 0x140f17670, not the
  0x140f17950 used for user variables; same signature. Direct `self.var = value` writes use 0x140f17460.
  Helpers (glow, shield, trails) followed the wrong ship until 0x140f17670 was redirected too.
- `refreshEffects` (0x1404acbe0) sets `sprite_index`/`mask_index` on every ship and is called by
  `refreshLoadout` only while ships are active. Upgrade objects call `refreshLoadout` during the upgrade
  screen (ships deactivated), so the hull was never rebuilt after later picks. Fix: run the chooser's
  `refreshEffects` 2 frames after their screen closes (both PCs, same frame) and keep the other ship's
  sprite/mask/blend around every call.
- Draw events are not dispatched through the event executor (0x14112c000); hook a Draw function itself.
  Objects without a Draw event (shots, pointer, shield) use the runner's default draw: set colours in a
  pre-draw hook (oScreenShake Draw_72, 0x140cfbc10) and undo them in post-draw (Draw_73, 0x140cfb940).
- Ship effects are separate objects: engine flames oBurstThrust 317/318 (sThrustBlue), the always-on
  ray ahead of the ship oScope 674 (sprite sSkirmishWeaponRed, blended blue by the game), shot glows
  oBlaster/oBurst*, jet trails oPlayerTrailL/R (colour in `trailColor0/1`), bullet trails oTrail and
  children (`leader` = the shot they follow; Draw 0x140e4aa00). oPlayerVisibility 619 has no owner.
- Upgrades recolour by swapping in colour-named sprite variants (`sThrustRed`, `sGrenadePink`); the
  default is the `Blue` variant or the plain name.
### What the mod does (draw time only, restored right after; copies stay identical)
- Player 2's objects draw with orange copies of their sprites, made at runtime: draw the sprite to a
  surface with blending off, read it back (buffer, RGBA), keep each pixel's brightness and saturation
  with an orange hue (grey pixels: light orange), `sprite_create_from_surface` + `sprite_add_from_surface`.
  The ray and trails use solid orange at the pixel's brightness.
- Co-op colour lock: each player's ship and effects draw with run-start colours and default sprite
  variants, so upgrades don't recolour them.
- Gotcha: the `Call()` helper had room for 8 arguments; `sprite_create_from_surface` takes 9 (stack
  overrun, `__report_gsfailure`). Now 16.
### Open
- The temporal bubble shield can still turn player 1's flames purple (owner: leave for now).
- One bot run split at frame 1 and an identical rerun did not; not investigated.

## Upgrade screen over the network (2026-11-17, confirmed by the owner)
- `confirmUpgrade` 0x1401053f0, `rerollUpgrade` 0x1404e17d0, `banishUpgrade` 0x14007aa30 take no
  arguments: they act on oUpgrade's selected card (`pos`/`position`). oUpgrade (828) Step_0 0x140e9b030
  calls confirm and the `upgradePress*` navigation scripts; `titleString` holds the selected card's title.
- The chooser's PC runs the screen on its own local input. The three actions are intercepted there and
  sent with the input (action, slot, FNV hash of the title) for frame +3; both PCs set `pos` and run the
  action after oUpgrade's Step on that frame. Running it before the Step hit a destroyed ds_list.
- Offers, alarms and actions use a per-player random stream (derived from the shared table at that
  player's first screen); the shared table is saved at open and restored at close, and the world hash
  skips instances while a screen is open (the chooser's screen differs between PCs).
- Verified: bots on both PCs, 14+ screens with reroll/banish/confirm, titles matched, 8101 frames identical;
  owner checked by hand.

## Helpers and HUD per player (2026-11-17)
- Helpers already act per player (bots, blades/construct drones): `shooterId` / `leader` point at their
  own ship. Interceptors and swarmlings seen unowned look like enemies (not confirmed).
- Grey enemy bar: oWaves (862) Draw_64 reads the last-hit enemy from a variable on oPlayer and eases
  `oWaves.enemyHull` toward it. Running that draw as the local player makes it show the local player's
  enemy (owner confirmed). The boss bar stays shared (owner: fine).
- OPEN (owner, for later): the rainbow effect on the top-left ability icons still shows on player 1's
  screen for player 2's abilities. Hiding the other player's oGUIFlash (525) draws did not fix it, so
  the icon rainbow comes from somewhere else (candidates: `hueCycling` / `createRainbowEffect` ->
  oRainbowEffect 644, or a value the HUD reads from the ship/brain without the local context).

## Remaining desyncs (2026-11-17, investigating)
- Crash fixed: the scheduled post-upgrade refreshEffects ran while a wild mod (Quantum Decoherence, +5
  levels) had already deactivated the ships ("Unable to find any instance for oPlayer"); it now waits
  until both ships are active.
- Split report (both PCs, written 10 frames after a split): random-number trace from split-240 and the
  position of every compared object for the last 256 frames. Owner's 11-minute split: at frame 41926,
  with identical positions the frame before, player 1's ship touched an oBulwark on player 2's PC one
  frame earlier than on player 1's own PC (armor stacks / hit flash / knockback). Same pattern as the
  older station-wall split: a collision of a PC's local ship lands a frame apart.
- Start split (frame 1, ~1 in 5 bot runs, more with real menus): both ships move ~0.5 px differently in
  frame 1. Frame-1 differences: controller.bgDistance, global.trailSpeedLimit, mouseConstrain*,
  background star counts. `viewDimensions` (called by `cameraZoom` every frame) reads the camera, and
  each PC's camera follows its own ship, so camera-derived values (zoom, view size, trailSpeedLimit?)
  differ per PC. Forcing bgDistance/trailSpeedLimit at frame 0 made it worse (removed).
- Next: find gameplay reads of camera-derived values (cameraZoom/viewDimensions outputs) and give the
  simulation one camera-independent value on every PC; check whether the local-ship collision offset has
  the same cause (camera-dependent zoom or collision masks).
- FIXED (start split): `cameraAndBackground` (called by oController Step_0) writes global.trailSpeedLimit,
  oController.bgDistance and mouseConstrain* from each PC's camera and mouse. On the menus these drift per
  PC (the mouse leans the camera); trailSpeedLimit eases toward its target, so the leftover changed ship
  movement in frame 1. In co-op the script is skipped until the first ship step. 6/6 bot runs and the
  owner's menu test then matched from frame 1.
- FIXED (collision splits, owner testing): draw code draws random numbers (one PC drew an extra number
  between frames 448/449, outside GML scripts), and each PC draws different things (own camera), so the
  gameplay sequence drifted and later hits/collisions differed. The generator table is now saved after
  oController's End Step and restored before its Begin Step, so nothing drawn can move it.
- Tools: split report = random trace (from split-240) + positions and image_index of compared objects
  (last 256 frames) on both PCs; ND_SNAPAT=N writes the position history at frame N; every redirected
  ownership write is logged in tests (the counts differed per PC in the owner's run; same in bot runs).

## Lobby (2026-11-17, owner tested on two local windows)
- Main menu (oMenu 577): entries are drawn with draw_text at y = 306 + 70k (menu space, drawn at gui/3072 scale);
  click areas clickPosK..clickPosKb are re-laid out by the menu every frame; the selection streak is
  oMenuCursor2 (579) at the selected entry's y; key/click handling sits in Step_0 and Step_2; selection
  sound play(92). The mod inserts "Multiplayer" as row 2 (entries >= 2 drawn/clicked one row lower, streak
  placed per on-screen row, Down/Up and mouse, mouse factor learned from the menu's mouseXPrev/YPrev).
- `cameraAndBackground` is also called by oMenu Step; it stays held in co-op until the run starts.
- Lobby screen (mod-drawn): both players, RGB sliders for each ship colour (hue for the recolouring;
  white/grey = the game's own look), host's upgrade-pool setting (not applied yet), Invite (Steam),
  Ready Up (R) / Start Game (Enter), Back. State goes as MSG_LOBBY (flags + 24-bit colour), resent twice a
  second; MSG_START carries both colours as the host has them.
- Start: both PCs call confirmMenu (Game Start) then confirmLoadout (START) on oLoadout. The run settings
  (global gameMod* flags) are only final at the run start (right after START they are still the previous
  run's), so the host sends them (MSG_RUNCFG) at its first ship step and the guest waits on the START
  screen, applies them and presses START. Seen split before this: one PC started with Enemy Territory.
- The test transport is polled on the menu, in the lobby and on the START screen.

## Upgrade pool, no-save, leaving runs, any resolution (2026-11-17, owner tested)
- Each oPowers builds its gear pools (gearWeapons/Bodies/Shields ds_lists) from global.accountLevel at its
  Create (rank 1: 7/5/6, rank 21: 9/8/8). Ranks travel in the lobby; oPowers Create runs with
  accountLevel set to the host's rank (P1, and P2 under "host's unlocks") or the guest's own (P2 under
  "each player's own"), restored right after. Without this, players of different ranks split at once.
- No saving once a networked run starts (until the game closes): ini_write_*/ini_*_delete, buffer_save*,
  file_copy/delete/rename, save scripts (saveRunToFile, triggerSaveRun, saveDataToFileNoOverwrite*, ...),
  and steam_api64 SetAchievement/StoreStats/SetStat*/UploadLeaderboardScore/FileWrite*/FileDelete.
- Leaving the run room clears the mod's run state (else menus keep neutral mouse = frozen cursor). The PC
  that leaves sends MSG_QUIT; the other gets 1.5 s to reach the same networked quit on its own (forcing it
  earlier left the game's own pending save-and-quit to fire at the next run's start), then follows.
  Stored menu/controller instance pointers are never reused after a run (crash on return to the menu).
- Menu/lobby at any resolution: entry x / first row / row height read from the menu's draw_text calls
  each frame; the mouse is mapped by a least-squares fit (scale + offset per axis) between the gui mouse
  and the menu's own mouseXPrev/YPrev, sampled while the mouse rests. Screen-space effects
  (oLevelUpEffect 571, oBurstGrowSpecial 310, tutorials 819-823) are left out of the world hash.

## Respawn restores the ship fully (2026-11-18)

While a player is down, the mod skips that ship's and brain's events. Three things did not come back on their own:
- Collisions: the death clears the ship's `cancollide` and `canHitPulse`; respawn sets both to 1.
- Glow, jet trails, shield: the death destroys them; respawn schedules `refreshEffects` two frames later,
  once both ships exist.
- Engine flames: these are short-lived `oBurstThrust` (317) puffs that `playerThrust` makes only on frames
  when the ship's flag variable (slot 107600, read at 0x14044a0c7) is 1. The ship's `Alarm_1` sets that flag
  and re-arms itself. While the ship's events were skipped, the alarm ran out without re-arming (alarm[1] = -1).
  Respawn now calls `alarm_set(1, 1)` (0x140f5eba0) on the ship. Bot run: player 1 had 0 flames after the
  respawn before this fix, and 11 after it (player 2 also had 11); the two copies stayed identical for 3781 frames.
- How it was found: the mod traced reads, `with` loops and value comparisons (the runtime's compare is
  0x140f40640) inside player 1's `playerThrust` on a thrusting frame before the death and one after the
  respawn. The first comparison that differed led to the flag, and a scan of the game code for that variable
  slot (0x141af4f98) found the functions that use it.

## End of run (2026-11-18, owner tested)

- When a hit leaves no living player (checkDestroyPlayer sees hull <= 0 with the other player already down), the mod sets `g_runOver`: pending respawns are cancelled and nobody comes back.
- The game ends a run when no oPlayer exists. A downed player's ship is only hidden, so after game over `instance_exists` / `instance_number` for oPlayer (603) skip downed players' ships. The game then ends the run as in single player and the end screen's Time Elapsed stops at the moment of death. (The P2 brain `timer` variable keeps counting, but nothing shows it.)
- The mod's co-op messages (down / upgrades available / paused) are not drawn once the run is over.
- Test only: the L key (input bit 1024) sets both players' hull and shield to 0 on the same frame on both PCs; `ND_KILLALLAT=<frame>` does the same in bot runs. Like K, it exists only when `ND_DETERMINISM=1` (set by tools/run_pair.ps1).
- Verified by a bot run (ND_NOPICKS=1, ND_KILLALLAT=600: game over at frame 601, no respawn, both copies IDENTICAL) and by the owner by hand on two windows.
