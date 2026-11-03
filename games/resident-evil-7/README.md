# Resident Evil 7 biohazard (2017)

| | |
|---|---|
| Engine | RE Engine (TDB 70) |
| Versions tested | Steam, app 418370, buildid 22773795 (RT/DX12 build); REFramework nightly-01433 (d4611ae) with csharp-api |
| Projects | [coop-re7](https://github.com/xXCroissantXx/coop-re7) (directory entry "Resident Evil 7 biohazard") |
| QA level | none (bot tests on one PC, plus one two-player session by the author and a friend, 2026-11-02, on v0.11; v0.12-v0.17 bot-tested only) |
| Last updated | 2026-11-03 |

## Where things stand

Two players can play through the start of the game together, each in their own world: the first real session
went from the opening to the first save point after Chapter 3 and fought Mia together. Each player sees the other
as a full-body character (Ethan, or Mia, Clancy, Zoe, Chris or Joe picked in a Lobby) who walks, jogs, crouches,
sidesteps, looks around, blocks and swings their weapon (shown in hand) where the other player does, with
footstep sounds, over Steam relay. Players
install with one script, pick each other from a Steam friends list in an in-game window (the other player gets an
invite to accept), and can jump to the other player's chapter with one click.
Enemies are shared: each enemy is run by the game of the player nearest to it and copied in the other game,
damage, deaths and flinches go both ways, a paused player's enemies move to the other game, and bosses (Jack's
garage fight onward) run in each game with shared damage and defeat. Doors (unlock, open, close), drawers,
switches, one-off interactions (the fridge, the van), pushed physics objects, breakable boxes (each player gets
the box's item) and supply pickups are shared, including for areas the other player reaches later; weapons, keys,
files and maps stay for both.
Story cutscenes can be shared between players close together (experimental, off by default). A fatal hit puts a
player down until the other stands next to them for 3 seconds. Item boxes, puzzles, phone calls and weapon
sounds are not shared.
Saving is off by default and can be switched on, with automatic save backups.

## Existing work

- No RE7 co-op mod was found (GitHub, Nexus, ModDB and general web search, 2026-10-29).
- RE7's third-person options are a Cheat Engine camera script (Nexus RE7 mod 12) and "Third Person Full Model
  Placeholders". These pointed to full Ethan, Mia, Zoe and Clancy models already shipping in the game files.
- Same engine: the RE4 (2023) multiplayer mod (claybtv, closed beta, reports lag spikes and enemy desync) and
  RE2 (2019) Local Couch Co-op (local only). Neither has netcode to reuse.
- alphaZomega's EMV Engine (REFramework Lua) was the most useful reference for spawning and animation.
- RE2 (2019) got an online multiplayer mod on 2026-10-30 (Nexus RE2 mod 3118, by Kenjiro; closed source,
  REFramework plus a native plugin): a lobby in the game's menu, joining by Steam invite, IP or LAN. Its
  comments show the usual install problems (wrong REFramework version, files in the wrong folder) and
  unreliable sound for the second player. Its joining flow inspired coop-re7 v0.4.

## How the game works (for co-op)

- **No netcode**: a single-player game. REFramework gives Lua and C# (Roslyn source plugins in
  `reframework/plugins/source`, hot-reloaded) access to the RE Engine type database.
- **Player**: `app.ObjectManager.PlayerObj` (for example `Pl0000_Chapter1`). Movement state is on
  `app.PlayerMovement` (`get_isCrouch`, `get_isWalk`, and fields `WalkSpeed`/`JogSpeed`/`CrouchMoveSpeed`).
  `app.MovementController.warp(vec3, quat)` moves the player without the character controller undoing it.
- **Ethan has a full body**, just never on screen: the player object carries `ShadowBody`, `ShadowHead` (a full
  face rig), `ShadowHandL/R` and `ShadowLowerBody` with **DrawDefault=false, DrawShadowCast=true**. Ethan's
  motion bank is `Animation/Player/pl0000/pl0000.motbank` (29 banks, ~5,500 clips: Walk/Jog/WalkCrouch in
  F/B/L/R, turns, idles).
- **Saves**: `app.SaveDataManager`. Normal saves go through six `save…Request` entry points
  (`saveFileAutoRequest`, `saveFileMenuRequest`, `saveFileSystemDataRequest`, `saveFileExpansionDataRequest`,
  `saveFileBirthdayDataRequest`, `saveFileUniqueDataRequest`), **but not all writes do**: a chapter jump
  (`GameManager.chapterJumpRequest`) wrote the Steam save folder with all six skipped, through internal paths
  (`chapterJumpEnemySave`, `chapterJumpEnemyStampSave`, `folderSave`, …). Block those too, and check a
  before/after hash of `Steam\userdata\<id>\418370\remote` rather than trusting the hooks. Skipping only the
  `via.storage.saveService.SaveService.saveSingle` disk write did **not** stop it either.
- **Chapter jump**: `GameManager.getChapterJumpDataKey(ChapterNo)` names 33 targets; `chapterJumpRequest(key,
  false, "")` jumps. `_CurrentChapter` uses the same `ChapterNo` values (Chapter0=2, Chapter1=4, Chapter3=5,
  Chapter4=6, Chapter8=35, Chapter9=37). Back-to-back jumps before the last one has loaded leave cutscenes
  invisible and the player outside the map, so allow one at a time. About ten jumps in one session broke the
  game again (black screen) even with that rule; a restart fixes it.
- **Cutscenes**: `GameManager`'s static `_GameEventMan` (`app.GameEventManager`): `RunningEventControllers.Count`
  > 0, or `GameManager.PlayingMovie` set. Some scenes are missed by this check, and some events run in the
  background while the player has control (`c01e00_01` for the whole basement after the ladder), so also require
  `app.MenuManager.isPlayingEvent()`.
- **Pause menu stuck**: `MenuManager.isCanOpenPauseMenu(InputLevel.Normal)` stays false while
  `IsPlayInGameFullScreenMovie` is true, and a video cut short (chapter jump, forced cutscene) can leave it true
  for good. `set_IsPlayInGameFullScreenMovie(false)` brings the pause menu back.
- **Movement locks**: `MenuManager._PlayerOperatableFlag` / `_CameraOperatableFlag` (app.MultiBoolFlag:
  `_FalseIds` dictionary, `setFlag(id, bool)`) and `app.PlayerOrder.switchPlayerOperatable` /
  `switchCameraOperatable(bool, OperationLayer Base|Menu|Interact)`. Calling `Inventory.addItem` from a script
  left an `InventoryMenu` lock on both with no menu open; clearing it with `setFlag("InventoryMenu", true)`
  freed the player. Beware calling zero-argument methods blindly when probing: `PlayerOrder.requestCodexOperationAction`
  raised the wrist codex and started a story call.
- **Pausing**: `GameManager.get_IsPause()` (pause menu, inventory, map) or `MenuManager.isOpenPauseMenu()`.
  A paused game stops updating its enemies but REFramework's `on_frame` keeps running.
- **Player action animations**: the player object's own `via.motion.Motion` plays the body you never see.
  Layer 0 is locomotion (`pl0000_Idle`, `pl0000_Move` blend trees, `TurnL/R`); blocking is on layers 1 and 2
  (`Hands_Guard_Start` 2305, `Guard_Loop` 2300, `Guard_End` 2306, upper-body joint mask 120); door and wall
  touches on layer 4 (`Hands_TouchDoor_End` 9012, `TouchWall_Start/End` 9000/9002); camera-only clips on bank 1.
  Phone calls play on a separate event actor, not on the player. All pl0000 banks report `BankID` 0, so a clip
  is identified by motion ID. A ghost layer given the same `JointMaskID` and `BlendMode` plays the clip over
  its walk.
- **Other playable characters**: every player character has the same shadow-only pieces on Ethan's skeleton:
  `ShadowBody` plXX01, `ShadowLowerBody` plXX02, `ShadowHead` plXX50, `ShadowHandL/R` (Ethan pl0000, Mia pl2000,
  Mia on the ship pl2100, Clancy pl3000, Clancy DLC pl3100, Zoe pl3400, Chris `ch8/.../pl1000`, Joe
  `ch9/.../pl9000`; Joe's hands are not at pl9010/pl9020). The plain-game versions are incomplete (the heads
  have no real face). Nexus RE7 mod 86, "Third Person Full Model Placeholders", replaces them with complete
  models under the same paths, but then the player's own first-person body has a head too (the camera sits
  inside it). Moving the mod's `*.mesh.*`/`*.mdf2.*` files under `natives\stm\coop\...` and loading
  `coop/<path>` keeps the player's view normal; REFramework's Loose File Loader serves such new paths. The
  mod's bodies include the head (adding the plain shadow head on top shows black), its Mia is laid out
  differently (pl2001 clothes, pl2002 head, pl2020 hair, pl2023 skin) and its Joe stands in a T-pose (the plain
  Joe animates). A ghost dressed this way must not reuse the local character's parts mask (Ethan's hides parts
  1, 10 and 11, which are Mia's eyes and chest on hers).
- **Breakable boxes and drawers**: `app.ItemDropDestruct` (SaveGUID, `IsBreaked`, `RigidBodyCompo` =
  `app.Havok.RigidBodyDestruct`) and `app.InteractDrawer` (SaveGUID from `InteractObjectBase`, `NowDrawerOpen`,
  `SetSwitch(bool)`). Doors close with `DoorPush.requestClosed(false)`. **Don't call
  `RigidBodyDestruct.setBreak(dir, pos, normal, attacker, type)` from a script**: it threw with a nil attacker
  and crashed the game (access violation) with the player as attacker. `setAlreadyBreak()`,
  `dispBreakAfter(true)` and `breakObjectWithoutSetBreak()` run but leave the box drawn, so a box smashed in the
  other game is hidden instead (`set_DrawSelf(false)`, `via.physics.Colliders` and `via.dynamics.RigidBodySet`
  disabled). `ItemDropDestruct.setItem()` rolls the box's drop (`SetItemID`, or `DROP_TABLE_DEFAULT` for a
  random one) and creates `DropItem` as a child of the box at the world origin; `set_Position` doesn't stick
  until it's detached with `set_Parent(nil)`. Box items have a SaveGUID.
- **Weapon animations**: every weapon's motion list is active at once, all as `BankID` 0 with the same motion
  IDs (2000s stance and walk, 2200s aim pose, 2300s guard, 2400s attacks); the body's
  `Motion.TargetBankType` picks the set (0 hands, 10 axe, 30 knife, 100 handgun, 110 shotgun, 200 items...).
  A copy body must keep the same `TargetBankType` while a clip plays (the clip binds a frame after
  `changeMotion`; switching back at once bound nothing), and the clip list (`getMotionInfoByIndex`) also follows
  it, so list clips with it at 0. The weapon stance clips use joint mask 120, which covers the legs too (the
  player never sees them), so on a walking copy they freeze the legs. The held weapon is the player's
  `wpNNNN_*_Item` child, parented to joint `R_Wep` with a local offset (the knife: 10 cm, turned).
- **Leaving an area hides its items like a pickup** (`getItemState` 0 -> 6, often with `ItemDataID` nil), so a
  pickup watcher must check that the player was within reach. Unloaded objects throw an
  `InvalidOperationException` on every call (about 2,700 a minute from a 4 Hz poll), so drop references at once.
- **Spawning enemies by script**: the Molded wait in `MoldedPool` (`app.EnemyPool`) and are spawned by
  `app.EnemyGenerator` from `app.EnemySpawnInfo` points; a spawn requested from a point outside the loaded
  area is taken straight back into the pool, so a test Molded has to be met where the game spawns it.
- **Adding items**: `ItemManager.createDropItemInstance(owner, id, count)` creates an item (at the world origin,
  not interactable); `Inventory.addItem(item, go)` returns `LimitOver` for a weapon the story hasn't given yet.
- **Steam**: the game's `steam_api64.dll` exports the modern flat API (`SteamNetworkingMessages_SteamAPI_v002`
  and friends), so a plugin can use Valve's relay through the game's own Steam session.
- **Enemies**: `app.ObjectManager.ManagedObjects` is a list of lists; one of them holds the area's enemies
  (`EmNNNN` GameObjects, index 1 in Chapter3; the indices don't match the `ListType` enum, so find it by
  contents). Em3000 is Jack Baker; the Molded are Em4000, Em4100 and Em4200. Regular enemies and Jack carry
  `app.EnemySave.SaveGUID` (read the Guid's `mData1..3`/`mData4_0..7` fields), saved in the level, so it
  identifies the same enemy on both PCs; story characters without one can use folder path + name. Health is
  on `app.DamageController` (`get_health`, `get_maxHealth`, `get_isDead`, `setHealth(health, max)`).
- **Killing an enemy from script**: `setHealth(0, max)` marks it dead but it keeps walking. The real death
  (or Jack's knock-down) needs damage: static `DamageController.newFakeDamageInfo(damage, attacker, target)`
  then `addManualDamage(info)`. Damage is scaled (health + 1000 left Jack on 2300/10000), so overkill.
- **Test enemies**: an inactive enemy in a loaded folder wakes with `GameObject.set_UpdateSelf(true)` +
  `set_DrawSelf(true)`, and `app.MovementController.warp` places it. Only chapter-jump keys ending in
  `_Start` load into gameplay (`Chapter8` went to the title); Chapter4_Start has nine sleeping Molded but
  loaded an empty level from the title.
- **Enemy animation by hand**: enemies animate through `via.motion.Motion` (8 TreeLayers, state readable as
  `MotionBankID`, `MotionID`, `Frame`, `Speed`), driven by `via.motion.MotionFsm`. With the FSM disabled
  (`set_Enabled(false)`) and the AI off (`app.EnemyOrder.requestThinkOff(owner)`), `TreeLayer.changeMotion(bank,
  id, frame, interp, 2, 0)` plays any clip and `MovementController.warp` places the enemy: a fully driven copy.
  The held weapon (Jack's) isn't drawn that way; it's managed by the AI side.
- **Enemy targeting**: `app.EnemyThinkBase` has `_Target`, `setTarget(GameObject, ReasonType)` and
  `updateTarget()`. A target set to a non-player object (the ghost) is accepted but dropped within a second,
  because `updateTarget` re-picks from the vision/hearing sensors and the ghost isn't a valid target. Enemies
  only ever chase their own game's player.
- **Cutscenes can't be skipped by API** (no skip method on `app.GameEventController`), but
  `via.Scene.set_TimeScale(8)` on the current scene while one plays fast-forwards it with every step intact.
- **Story progress** lives in `app.GameFlowFsmManager` (singleton): `CurrentMainGameFlow`
  (`GameFlowKindEnum`: C00_Main, C01_Main, C03_1..C03_5_Main, C04_1..C04_3_Main, FF000..FF050_Main,
  C07_1..C07_4_Main, C08_Main, C03_IMD_Main, C09_Main), and per stage a progress number keyed by a uint32 hash
  (`getProgressiveNo(hash)` → `via.userdata.Variable`, `get_S32`; `setProgressiveNo(hash, int)`,
  `jumpProgressiveNo`), plus `sendEventEnded(hash)` / `isEventEnded(hash)`. In Chapter3 the progress of
  stage C03_1 went 100 → 200 → 300 → 400 as the player followed the objective. **The numbers only record the
  story**: `jumpProgressiveNo` to the next value changed nothing in the world. Chapter jumps keep a session's
  progress, and a dinner cutscene started with progress already past it hung (player locked); a full game
  restart clears it.
- **Cutscenes by name**: each has an `app.GameEventSetting` whose `get_eventName` is like `c03e01_00`
  (chapter 03, event 01, part 00); the area's settings are in `GameManager._GameEventMan.EventSettings` (41 in
  the Chapter3 main house, including Jack's scripted `em3000_*` moves). `app.GameEventManager.requestEvent(name,
  nil, false)` starts one at once. Replaying one the game had already finished hung it at `Process` 3; ending
  it with `RunningEventControllers[i].EventTask:doTerminate()` cleared the event but left the player locked
  until the chapter reloaded.

## What we changed

- **Second body.** GameObjects built at runtime (`create` + `createComponent(via.render.Mesh)`) never render
  in RE7, even with valid resources and every draw flag set. A prefab instance does:
  `app.vr.VrManager.DummyPlayerPrefab` (`Prefab/System/VrDummyPlayer.pfb`) instantiated, then `set_DrawSelf(true)`.
  One dummy per Shadow\* mesh: the body is the root, and the others are parented with `SameJointsConstraint`.
- **Shadow meshes don't animate.** RE Engine skips skeletons it thinks are invisible. Forcing
  `DrawDefault=true` on every piece fixed a ghost that only slid around.
- **Animation.** Give the body the player's `MotionBankAsset` + `JointMap`, call `setupMotionBank()`, add a
  **constructed** layer (`create_instance("via.motion.TreeLayer")` + `.ctor`, `setLayerCount(1)`, `setLayer(0)`),
  call `set_Resource(nil)`, then `changeMotion(bank, id, 0, blend, 2, 0)`. Without `.ctor` no clip binds.
  Disabling the dummy's `app.MotionManager` freezes animation, so leave it on.
- **`pl0000_Hands_Stand_Loop` jams the layer.** It never binds, and as the first clip it leaves a T-pose for
  good. The mod blacklists it and uses `StandDamage_Loop`. A watchdog rebuilds the layer, or the whole ghost,
  when a clip doesn't bind or the frame stops advancing.
- **Head look.** `Motion.setLocalRotation` in an `UpdateMotion` hook is accepted but overwritten before render.
  Writing `via.Joint.set_LocalRotation` at pre-`PrepareRendering`, based on `getCalculatedLocalRotation`, works
  without freezing or accumulating.
- **Network.** A C# plugin (UDP for testing, Steam relay for play) and a 64-byte packet carrying position,
  rotation, stance flags and look yaw/pitch. The receiver interpolates 0.15 s behind, extrapolates up to 0.1 s on
  late packets, picks Walk/Jog/Crouch × F/B/L/R from velocity relative to facing (using look-ahead velocity
  from the buffer), eases turns, and blends clips.
- **Loads, deaths and area changes.** These recreate `PlayerObj` and unload the folder the ghost lives in. A
  watcher compares the player's address every 0.5 s and re-runs the whole spawn sequence when it changes, or
  when the ghost's objects stop answering. A load first passes through a temporary loading-scene player (no
  Ethan meshes), so the ghost is only built for a player with a Shadow body. Other playable characters (the
  videotapes) work the same way, with clips matched by suffix (`Hands_Walk_Loop`…). `Hands_Crouch_Loop` jams
  the layer like `Stand_Loop`, so crouching still is the crouch-walk clip held on one frame.
- **Disconnects.** The ghost is hidden when the newest sample is more than 2 s old and shown again when data
  resumes.
- **Connecting.** The in-game window lists online Steam friends (`ISteamFriends` `GetFriendCount`/
  `GetFriendByIndex`, `GetFriendGamePlayed` for "in RE7"). Invites need no Steam callbacks: each player sets
  **rich presence** (`coop_re7` = mod version, `coop_re7_to` = the SteamID they connected to), which Steam
  shares between friends in the same game. A friend whose `coop_re7_to` is your ID shows as an invite with an
  Accept button. Connecting by SteamID still works. The plugin reads a one-line command file, switches
  between Steam and the bot live, and saves the friend's ID for the next launch.
- **Meeting up.** `_CurrentChapter` goes in every packet; "Go to my friend's chapter" jumps to the start of
  it. An automatic teleport after the jump was dropped: it fired before the new chapter's player existed,
  and onto positions that weren't loaded ground (outside the map). RE7 streams areas in around the player,
  so teleporting is manual and limited to 40 m, onto the friend's position about a second earlier.
- **Installer.** `Install.cmd` finds RE7 through Steam's library list, copies it to a separate folder, downloads
  REFramework and csharp-api from the pinned release with SHA-256 checks, installs the mod and backs up saves.
- **Saving.** Off by default (all request and internal save paths skipped). When switched on, the C# plugin
  first copies the save folder (found with `ISteamUser_GetUserDataFolder`) and keeps the last 20 backups.
  Each player saves their own game; when one really saves (hooks on `SaveDataManager.saveFileMenuRequestExecute`
  and `saveFileAutoRequestExecute`), the other is told, so both can save at the same point. What a save
  stores matters for co-op: the ghost has no folder or save component, so it's never saved; doors and items
  keep their shared state; but an enemy's save data (`EnemyStatus.EnemySaveDataClass`: IsUpdate, IsDraw,
  Health, Position, Rotation, ThinkState, CommonThinkState) would store a followed enemy's AI-off state, so a
  pre-hook on `app.EnemySave.prepareToSave` gives followed enemies their AI back for a few seconds.
  Autosave points are `app.TriggerInAction` zones under `LevelFsm_cNN/.../AutoSave/<flow>_<progress>`; they
  fire once (`TriggerInSuccessCount`, IsLoop false), even when the save itself was blocked, and don't fire on a
  warp into them. `SaveDataManager.saveFileMenuRequest(player)` doesn't open the save menu without a recorder.
- **On screen.** Connection, ping (an echo of the peer's send time in the packet), the friend's chapter and Steam
  name, a version-mismatch warning, and a nameplate. The ghost is hidden during either player's cutscene
  (a cutscene bit in the stance flags). REFramework's default font (ImGui 1.92 with Windows fallbacks)
  already draws Cyrillic, Chinese, Japanese and Korean names; loading Microsoft YaHei instead lost Korean.
- **Footsteps.** The ghost has no sound components, but the player's `app.FootEffectController` has
  `callSE(SETriggerTypeEnum, via.vec3, UInt32 material)`, which plays Ethan's footstep at any position.
  Setting its `SETarget` field (Player=0, PlayerJog=1, PlayerCrouch=2) for the call picks the walk, jog or
  crouch set. coop-re7 calls it once per stride the ghost travels, with the local ground material.
- **Enemy damage and deaths.** Each game scans the area's enemies 4 times a second. A health drop sends
  `dmg <id> <chapter> <amount>` (the health actually lost); the other game applies it with `setHealth`
  (not another hit, which would be scaled again), never below 1. A death sends `kill <id> <chapter>`, applied
  as an overkill hit so the real animation plays. Applied events are remembered so they aren't echoed back.
  A Molded killed where the other player hasn't been yet is remembered and killed when it wakes there; Jack
  and bosses get back up in RE7, so their kills are only applied if they're awake. Events go reliably
  (`k_nSteamNetworkingSend_Reliable`) as a separate packet type next to the 30 Hz movement packets.
- **Enemy movement and ownership.** Since enemies only chase their own game's player, each enemy is *owned* by
  one game: by default the game it's awake in (if both are, the lower SteamID keeps it). The owner sends its
  enemies 10 times a second (position, yaw, layer-0 bank/clip/frame/speed; unreliable, newest wins); the other
  game turns that enemy's FSM and AI off and drives it 0.15 s behind. When the other player is 3 m+ closer to
  an enemy, the owner hands it over (reliable `own <id>`), so the enemy's real AI always chases a real local
  player and its hits land in the right game. A game that hears nothing about an enemy it gave away for 3 s
  takes it back, and then waits 30 s before handing it over again. **Both games must send snapshots**: the
  first version only let the "host" send and the "guest" read, so every enemy handed to the guest was taken
  back 3 s later and Jack flickered between the two games (seen in the first two-player session, invisible in
  bot tests because the bot never owns anything). Now the guest sends the enemies handed to it, the host only
  follows those it handed away, and a snapshot from the moment of the hand-off is ignored. A paused player's
  game hands all its enemies to the other.
- **Clock offsets must reset.** The receiver maps the sender's clock with the smallest `now - senderTime` seen.
  When the sender's clock restarts (their game or plugin restarted), every sample then looks minutes old and
  the other player stays hidden; reset the offset when a sample looks 5 s+ older than expected.
- **Doors.** `app.DoorPush` (an `app.Door`; find them with `scene:findComponents(app.DoorPush)`) has a
  `SaveGUID`, `get_isLocked`, `get_doorState` (0 closed, 1 locked, 2 little open as the player approaches,
  3 pushed, 4 auto-opening, 5 opened, 6 auto-closing), `unlockDoor()` and `requestAutoOpen(OpenSide)` (Auto =
  2 swings it to 90°). Key doors are pairs (left and right halves). coop-re7 polls them 4 times a second and
  sends `unlock <id>` and `open <id>` (states 3-5 only, not the approach creak); the other game applies them.
- **Items.** `app.Item` has a `SaveGUID`, `ItemDataID` ("ShotgunBullet", "Herb", "3CrestKeyA"...) and
  `get_ItemData().Category` (`ItemCategoryType`: 0 Other, 1 Weapon, 2 Shell, 3 Drug, 4 KeyItem, 5 File, 6 Map,
  7 Material, 8 StackWeapon, 9 UsableKeyItem, 10 DiscardableKeyItem such as coins and lockpicks, 11 SupplyBox).
  A pickup does **not** go through `Item.changeStateInventoryIn` (a hook there never fired): the world item's
  `getItemState` goes 0 → 6 and a separate inventory Item is created. `disableFromScene()` produces the same
  0 → 6 without touching any inventory. coop-re7 watches world supplies for 0 → 6 and sends `took <id>`; the
  other game hides that item (or remembers it until the area loads). Categories 1 (weapons), 4, 5, 6, 9 and 10
  are never removed, so both players can progress: weapon pickups start story beats, and hiding the Chapter 1
  handgun froze the other player mid-pickup.
- **Story.** Since progress numbers don't drive the story, coop-re7 shares the triggers' results: whoever
  starts a story cutscene (hook on `GameEventController.requestSetup`, names matching `cNNeNN_NN`) sends
  its name and the other game plays it with `requestEvent`, once per side. The first version pulled the other
  player in wherever they were and copied progress numbers and ended events at once. In real play that broke
  the player who was behind: a VHS tape ended for both when one finished it, a chapter change after the Mia
  fight hung on a loading screen, a forced cutscene left the picture washed out, and the dinner chair became
  usable mid-cutscene. Now a cutscene only pulls in a player in the same chapter within 12 m, progress and
  ended events are only copied for 30 s after a cutscene both watched, and the switch is off by default. The
  list of played cutscenes is kept per chapter in a file, because replaying a finished one hangs it.
- **Lobby and actions.** Each player picks a character; the choice goes as a reliable `look <key>` event (re-sent
  every 15 s) and the other game re-dresses the ghost. The current action clip (first bank-0 motion ID ≥ 1000 on
  the player's layers 4, 1, 2) rides in the movement packet's chapter field (`chapter + 256 * motionID`), and
  the ghost plays it on a second layer with the player's own mask and blend mode.
- **v0.13-v0.17.** Every bank-0 action on the player's layers (not just blocking) is mirrored; the weapon set
  goes as a reliable `bank <TargetBankType>` event and the held weapon as `weapon <mesh> <mdf>` plus `grip
  <local pos/rot>`. Interactions (`app.InteractSendFsm`, `InteractLongPressSendFSM`: `InteractSuccessCount`
  -> `successInteract()`; saving, tapes, phones and item boxes excluded) and switches (`app.InteractSwitch`) are
  shared, and loose props (`app.Havok.RigidBodyDynamics`, keyed by name and first position) are re-placed
  with physics off. A friend's hit requests the enemy's own reaction (`requestDamageAction` with a
  `ResistResultSet`). Bosses (anything but Em3000 and the Molded) aren't owned or copied: each game runs its
  own, and only damage and the defeat go across. Unlocks and box, drawer and switch states for an area not
  loaded yet are applied when it loads.
- **Downed and revive.** The player's death goes through `app.PlayerDamageController`
  (`isEnableDieByDamageInfo`, `doDie`, `add_DieHandler`, `playDeadMotion`) and the base
  `DamageController.canDie(DamageInfo)`, then `GameManager.setGameOverFlag` and `app.GameOverScreen`.
  coop-re7 pre-hooks `isEnableDieByDamageInfo` and `canDie` (only for the player's controller, by address) and
  returns false (SKIP_ORIGINAL, return value 0) while the friend is connected: the player stays alive on 1
  health. The downed look: `PlayerMovement.set_isCrouch(true)` (the view drops; the friend's ghost crouches),
  `PlayerOrder.switchPlayerOperatable(false, OperationLayer.Base)` and `PlayerOrder.setDamageGUIMonochrome(true)`;
  `PlayerOrder.setReserveProne()` did nothing visible. The friend within 2.8 m for 3 s revives (35% health).
  Both down, or 90 s alone, lets a real death through (a self-hit via `newFakeDamageInfo`).

## QA

- Bot peer: CoopNet `"bot"` mode walks a circle around the local player, cycling stance (walk, jog, crouch) and
  facing (forward, right, back, left) with 80 ms latency, ±30 ms jitter and 5% loss through the normal receive
  path. The menu shows the bot's true state (`botExpect`) next to what the ghost chose.
- Result 2026-10-29: stance and direction match in all 12 combinations, head look works, collision push-out
  works. Checked by eye and in logs on one PC.
- Reload test (menu "Tests"): loads the session's save with `SaveDataManager.loadFileAutoRequest` (what
  Continue does), 10 rounds. Result 2026-10-29: the ghost rebuilt and the network kept flowing in 10/10 rounds,
  and the save files were byte-identical afterwards. The ghost was animating again 7–8 s after the real player
  appeared. 5 rounds were cut off by the test's own 90 s timeout, since the Chapter 1 load itself takes about
  80 s; the timeout is now 180 s and the test reports load time and rebuild time separately.
- Steam path: self-loopback over `ISteamNetworkingMessages` (`relayStatus=100`, no loss). Two Steam accounts
  not tested together.
- Save guard: a supervised chapter jump with saving off, 2026-10-30. The save files' SHA-256 were unchanged, and
  the guard logged the jump's internal saves as blocked. An earlier build without the internal hooks failed
  this check, and the owner's saves were restored from backup.
- Go to my friend's chapter, 2026-10-31: the bot claims another chapter, one click, arrived at its start and
  playable. The friend list was read on one PC (15 online friends); invites and Accept need two accounts and
  are not tested.
- Footsteps, 2026-10-31: heard at the bot's position, changing with its stance. Checked by ear on one PC.
- Enemy sync, 2026-10-31, on Jack in Chapter3 with the bot sending events: the bot's kill knocked Jack down
  (seen in game); a kill sent while Jack slept (labelled as a Molded) was applied the moment he woke; the
  bot's 2500 hit took him from 10000 to 7500 exactly; a local hit of 1000 (700 after scaling) went out as 700.
  No echoes. A real Molded is not tested.
- Enemy movement, 2026-10-31, on Jack: recorded 6 s under his own AI (59 samples), then replayed with his FSM
  and AI off: same path, same two clips, switch within 0.3 s of the recording; looked "mostly natural" to the
  owner, but his hands were posed around a weapon that wasn't drawn. Ownership: handed to the bot (which never
  sends) → frozen, taken back after 3 s; bot's `own` event → released at once. Real two-game handovers untested.
- Bot "follow me" mode (replays the local player's own path, timed by the sample clock, skipping idle stretches,
  stopping 2.5 m away with 1 m hysteresis) stays on walkable ground; an early version skipped idle time in
  0.1 s steps and made 52 visible jumps in 75 s.
- Doors and items, 2026-10-31, Chapter3 main house: the bot's `open` swung a closed door open (state 5), its
  `unlock` unlocked one half of a key-locked hall door, its `took` hid a Herb; none were echoed back. The
  owner's own door push and handgun-ammo pickup were sent. The "remembered until the area loads" path is
  untested.
- Story, 2026-10-31, Chapter3: the bot's `story c03e00_00` played in the owner's game; its `prog` raised stage
  C03_1 from 500 to 600 and a later lower value was ignored; the owner's own cutscenes (c03e00_01, c03e00_02)
  and progress were sent. Replaying the already-finished dinner (after a script reset had cleared the played
  list) hung it; fixed by keeping the list on disk.
- Downed and revive, 2026-10-31: with the bot standing still ~5 m away, a fatal self-hit left the player crouched,
  locked and monochrome on 1 health (no game over); switched to follow, the bot walked over, stopped 2.6 m away
  and the player was up again 3 s later. Both-down and the 90 s timeout are untested.
- Saving together, 2026-10-31: the bot's `saved menu` event showed "Your friend saved at a tape recorder (your
  saving is off ...)". A real save wasn't tested (no recorder loaded, the nearby autosave had already fired);
  the save files' SHA-256 were unchanged while saving was briefly on.
- Installer: run end to end against a stand-in game folder (all steps, both hash checks), plus the update path
  (keeps the friend's ID).
- **First two-player session, 2026-11-02** (v0.11 plus fixes, two PCs over Steam relay, opening to the first
  save point after Chapter 3, about 90 minutes). Worked: invite and Accept, auto-reconnect on Continue, seeing
  each other, door openings received and applied, supplies, the Mia fight (each player's hits counted in the
  other game, her takedown applied), downed and revive (back up after 14 s), "friend saved" notices. Broke:
  enemy handovers (one-way snapshots, above), door closing (not sent), shared story pushing the player behind,
  the friend's ghost sliding after a VHS tape and vanishing after the basement ladder, hiding a weapon pickup.
  Install problems: a first-time player with no save gets "create save data?" in a loop while saving is blocked
  (tick Allow saving first), joining before the opening has finished leaves the screen black, and an install
  into a OneDrive-synced Documents folder (the installer now uses `C:\RE7-coop` there). Fixed in v0.12 except
  where listed under Known issues; v0.12 hasn't had a second session.
- Lobby looks, 2026-11-03, on the bot: with the full-model mod set up, Ethan, Chris, Clancy and Zoe look
  right; Mia keeps a gap on her left side and dark eyes; Joe has no hands.
- Blocking, 2026-11-03: the bot's ghost, told to play `Guard_Loop`, raised its arms and lowered them on release;
  the owner's own blocks went out in the packet as 2305/2306.
- Weapons and boxes, 2026-11-03 (v0.16, bot copying the owner, Chapter 1 main house): the knife showed in the
  bot's hand at the right angle, the bot swung when the owner did (2400/2401 bound, 62 frames) and walked with
  the knife out (slightly choppy; fading the layer in and out made swings stop matching, so it snaps). A box
  smashed by the bot vanished and left its item, which the owner picked up. Other weapons untested.

## How to play

Both players need RE7 on Steam (current build) and the .NET 10 runtime. Download the
latest release from the [coop-re7 releases](https://github.com/xXCroissantXx/coop-re7/releases) (v0.17), unzip
it and run **Install.cmd**.
Start **Start RE7 Co-op**, press Insert, and in the **RE7 Co-op** window click **Connect** next to the other
player's name; they click **Accept**. To meet up, click **Go to my friend's chapter**, then walk to each other.
A player starting RE7 for the first time should play the opening alone first. Full steps are in the
[coop-re7 README](https://github.com/xXCroissantXx/coop-re7).

## Known issues

- Scripted moments played by other actors (phone calls, the leg scene) aren't shown on the other player; in VHS
  tapes the character may slide. Weapon swings are a little choppy; only the knife is tested.
- An enemy mostly goes after the player whose game runs it. Bosses fight each player separately. Jack's
  scripted moments happen in each game unless Share story is on and the players are close.
- Keys and story items are picked up by each player separately (the other is told to pick them up too). A box
  smashed in the other game just disappears (no debris). Item boxes, puzzles and weapon sounds aren't shared.
- Mia (non-ship) has a gap on her left side and dark eyes with the full-model mod; Joe may have no hands.
- Being downed has no fall or kneel animation (a forced crouch stands in for it).
- Saving is off by default; with it off, nothing from a session is kept.
- The face doesn't animate. Only footsteps are heard from the other player.
- Tested on the current ray-tracing build only; the older non-RT build (Steam beta `dx11_non-rt`) is untried.
  The installer warns about it (it reads `BetaKey` from `appmanifest_418370.acf`).

## Next steps

1. A second two-player session with v0.17 (enemy handovers, flinches, boss mode, weapons, boxes, interactions).
2. Other weapons' swings and shots, and weapon sounds (the held weapon's `app.WwiseContainerApp`).
3. Shared key pickups that run the item's own pickup on the other side, so scripted events fire in both games.
4. Phone calls and other scripted moments played on event actors.
