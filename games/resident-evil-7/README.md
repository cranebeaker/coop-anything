# Resident Evil 7 biohazard (2017)

| | |
|---|---|
| Engine | RE Engine (TDB 70) |
| Versions tested | Steam, app 418370, buildid 22773795 (RT/DX12 build); REFramework nightly-01433 (d4611ae) with csharp-api |
| Projects | [coop-re7](https://github.com/xXCroissantXx/coop-re7) (directory entry "Resident Evil 7 biohazard") |
| QA level | none (single-PC bot tests only; two real players not tested together) |
| Last updated | 2026-10-31 |

## Where things stand

You can't play the story together yet. Each player plays their own world, and sees the other as a full-body
Ethan who walks, jogs, crouches, sidesteps, backpedals and looks around where the other player does, with
footstep sounds, over Steam relay. You can't walk through each other. Players install with one script, pick each other from a
Steam friends list in an in-game window (the other player gets an invite to accept), and can jump to the
other player's chapter with one click. The ghost rebuilds itself after save loads, hides when
the other player's data stops or during cutscenes, and shows the friend's name, ping and chapter on screen.
Enemies are shared in an early form: each enemy is run by the game of the player nearest to it and copied in the
other game, and damage and deaths go both ways. Items, doors and story progress are not shared. Saving is off by
default and can be switched on, with automatic save backups.

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
  > 0, or `GameManager.PlayingMovie` set. Some scenes are missed by this check.
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
  takes it back.
- **Clock offsets must reset.** The receiver maps the sender's clock with the smallest `now - senderTime` seen.
  When the sender's clock restarts (their game or plugin restarted), every sample then looks minutes old and
  the other player stays hidden; reset the offset when a sample looks 5 s+ older than expected.

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
- Installer: run end to end against a stand-in game folder (all steps, both hash checks), plus the update path
  (keeps the friend's ID).

## How to play

Both players need RE7 on Steam (current build) and the .NET 10 runtime. Download the
[v0.7 release](https://github.com/xXCroissantXx/coop-re7/releases/tag/v0.7), unzip it and run **Install.cmd**.
Start **Start RE7 Co-op**, press Insert, and in the **RE7 Co-op** window click **Connect** next to the other
player's name; they click **Accept**. To meet up, click **Go to my friend's chapter**, then walk to each other. Full steps are in the [coop-re7 README](https://github.com/xXCroissantXx/coop-re7).

## Known issues

- Enemy copies don't show held weapons and don't flinch from the other player's hits; ownership handovers
  between two real games are untested. Items, doors and story are separate per player.
- Saving is off by default; with it off, nothing from a session is kept.
- Some cutscenes aren't detected, so the other player can stay visible during them.
- A little residual choppiness under heavy simulated jitter.
- The face doesn't animate.
- Only footsteps are heard from the other player (no voice, weapon or door sounds).
- Tested on the current ray-tracing build only; the older non-RT build (Steam beta `dx11_non-rt`) is untried.
  The installer warns about it (it reads `BetaKey` from `appmanifest_418370.acf`).

## Next steps

1. A two-player test on two PCs, including the invite and Accept (QA is still `none`).
2. Check whether blocking `folderSave` affects in-session area state (items or doors resetting).
3. Enemy copies: held weapons and hit reactions; a real Molded test.
4. The rest of world sync: story and area, doors, items.
