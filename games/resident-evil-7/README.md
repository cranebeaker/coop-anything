# Resident Evil 7 biohazard (2017)

| | |
|---|---|
| Engine | RE Engine (TDB 70) |
| Versions tested | Steam, app 418370, buildid 22773795 (RT/DX12 build); REFramework nightly-01433 (d4611ae) with csharp-api |
| Projects | [coop-re7](https://github.com/xXCroissantXx/coop-re7) (directory entry "Resident Evil 7 biohazard") |
| QA level | none (single-PC bot tests only; two real players not tested together) |
| Last updated | 2026-10-29 |

## Where things stand

You can't play the story together yet. Each player plays their own world, and sees the other as a full-body
Ethan who walks, jogs, crouches, sidesteps, backpedals and looks around where the other player does, over
Steam relay. You can't walk through each other. Players install with one script, connect from an in-game
panel, and can meet up by jumping to the same chapter. The ghost rebuilds itself after save loads, hides when
the other player's data stops or during cutscenes, and shows the friend's name, ping and chapter on screen.
Enemies, items, doors and story progress are not shared. Saving is off by default and can be switched on, with
automatic save backups.

## Existing work

- No RE7 co-op mod was found (GitHub, Nexus, ModDB and general web search, 2026-10-29).
- RE7's third-person options are a Cheat Engine camera script (Nexus RE7 mod 12) and "Third Person Full Model
  Placeholders". These pointed to full Ethan, Mia, Zoe and Clancy models already shipping in the game files.
- Same engine: the RE4 (2023) multiplayer mod (claybtv, closed beta, reports lag spikes and enemy desync) and
  RE2 (2019) Local Couch Co-op (local only). Neither has netcode to reuse.
- alphaZomega's EMV Engine (REFramework Lua) was the most useful reference for spawning and animation.

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
  false, "")` jumps. Back-to-back jumps before the last one has loaded leave cutscenes invisible and the player
  outside the map, so allow one at a time.
- **Cutscenes**: `GameManager`'s static `_GameEventMan` (`app.GameEventManager`): `RunningEventControllers.Count`
  > 0, or `GameManager.PlayingMovie` set. Some scenes are missed by this check.
- **Steam**: the game's `steam_api64.dll` exports the modern flat API (`SteamNetworkingMessages_SteamAPI_v002`
  and friends), so a plugin can use Valve's relay through the game's own Steam session.

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
- **Connecting.** An in-game panel shows your own SteamID (with Copy) and takes the friend's. The plugin reads a
  one-line command file, switches between Steam and the bot live, and saves the friend's ID for the next launch.
- **Installer.** `Install.cmd` finds RE7 through Steam's library list, copies it to a separate folder, downloads
  REFramework and csharp-api from the pinned release with SHA-256 checks, installs the mod and backs up saves.
- **Saving.** Off by default (all request and internal save paths skipped). When switched on, the C# plugin
  first copies the save folder (found with `ISteamUser_GetUserDataFolder`) and keeps the last 20 backups.
- **On screen.** Connection, ping (an echo of the peer's send time in the packet), the friend's chapter and Steam
  name, a version-mismatch warning, and a nameplate. The ghost is hidden during either player's cutscene
  (a cutscene bit in the stance flags).

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
- Installer: run end to end against a stand-in game folder (all steps, both hash checks), plus the update path
  (keeps the friend's ID).

## How to play

Both players need RE7 on Steam (current build) and the .NET 10 runtime. Download the
[v0.3 release](https://github.com/xXCroissantXx/coop-re7/releases/tag/v0.3), unzip it and run **Install.cmd**.
Start **Start RE7 Co-op**, press Insert, and in the **RE7 Co-op** window copy your SteamID to the other player,
paste theirs and click **Connect to friend**. To meet up, both jump to the same chapter with the chapter
switcher. Full steps are in the [coop-re7 README](https://github.com/xXCroissantXx/coop-re7).

## Known issues

- No world sync: enemies, items, doors and story are separate per player.
- Saving is off by default; with it off, nothing from a session is kept.
- Some cutscenes aren't detected, so the other player can stay visible during them.
- A little residual choppiness under heavy simulated jitter.
- The face doesn't animate.

## Next steps

1. A two-player test on two PCs (QA is still `none`).
2. Check whether blocking `folderSave` affects in-session area state (items or doors resetting).
3. World sync, host-authoritative: story and area, doors, items, then enemies.
