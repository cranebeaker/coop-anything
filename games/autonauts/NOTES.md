# Autonauts co-op: working notes

## Identity (observed 2026-10-20)
- Store: Steam, app id 979120 (SteamManager.cs). Steam build id: unknown (appmanifest not read).
- Engine: Unity 2018.4.35f1 (dbb5675dce2d), Mono (MonoBleedingEdge), x64 Windows build.
- Files dated 2024-10-09 in the install (owner believed last update was 2022).
- SHA-256 Autonauts.exe: ec9ac079a210f6cba1c927f7502ba38ff3499c2fc803178cdc574a2b74ca0c70
- SHA-256 UnityPlayer.dll: e41d7d32228126290ce2bc36ccd16c6fc91d659285e11f2a6b411be6bf092adf
- SHA-256 Assembly-CSharp.dll: 3ecafdbf831a92e9c37bdb25def7a044eaeaed40c9997b96336c361c7ca4155f

## Prior work (searched 2026-10-20)
- No official multiplayer. Dev quote (2019/20): "MP isn't something that will happen for a while." https://playco-opgame.com/autonauts/
- No co-op mod found. Only player requests: https://itch.io/t/175669/pls-add-a-multiplayer-mode https://itch.io/t/161660/multiplayer https://steamcommunity.com/app/979120/discussions/0/1639801448924410169
- Official Lua (MoonSharp) mod support since the Industrial update: https://videogameschronicle.com/news/autonauts-adds-mod-support-with-free-industrial-expansion . Not usable for networking (sandboxed, no method interception).
- coop-anything: no Autonauts entry or folder on main (checked 2026-10-21). Issues/PRs not searched (GitHub API not available in session). Nexus, Discord not checked.

## Tools (pinned)
- ILSpy 9.1 binaries zip (ICSharpCode.Decompiler.dll used from a tiny CLI): sha256 1e925a4ce4c79d68139a486da66c682e2191e51215188816f613866c7276fc55
- BepInEx 5.4.23.5 win x64: sha256 82f9878551030f54657792c0740d9d51a09500eeae1fba21106b0c441e6732c4 (not cross-checked against a published hash)
- Plugin compiled with Roslyn csc from .NET SDK 8 against the game's own mscorlib (-nostdlib). NuGet was blocked in this environment.

## Observed
- Game boots under Wine 9.0 + Xvfb (default D3D11 via wined3d) with BepInEx; plugin Awake runs; main menu world generates ("ready" in Player.log). -force-glcore fails (no GL shaders in build).
- Architecture summary (save/load, IDs, input choke points):
  - SaveJSON.Capture builds whole world JSON in memory; LoadJSON.LoadFromDisk sets m_FinalString -> prefix to load from memory.
  - IDs: ObjectTypeList global counter; GetObjectFromUniqueID; spawn CreateObjectFromIdentifier; destroy BaseClass.StopUsing.
  - Player input: FarmerPlayer.GoToAndAction -> Farmer.SendAction -> FarmerAction.SendAction; ActionInfo carries UIDs.
  - Tiles: TileManager.SetTileType single choke point.
  - 195 UnityEngine.Random calls, variable timestep -> lockstep not plausible.
  - Steamworks.NET bundled incl. matchmaking + networking wrappers (unused by game).

## Inferred
- Host-authoritative snapshot + delta is the right model.

## Unknown
- Snapshot size/time on late-game saves. Whether Savable.Load can be applied to a live object.

## Build log (2026-10-20)
- Mod: AutonautsCoop 0.1.0 (BepInEx 5 plugin, Harmony). Host-authoritative mirror, TCP transport.
- Join: host SaveJSON.Capture -> gzip -> client; client loads through the game's loader with MyFile.Exists/Load patched to serve the string from memory (save name __coop_session__). Client saves/autosaves blocked.
- Client load threw in OffworldMissionsManager.GetMissionObjectType (empty list) on a fresh world; fixed by skipping CheckDailyMission on clients.
- Deltas: host scans savables 5 Hz, sends save-JSON of changed objects, removed UIDs, transforms of Farmers/Animals/Vehicles 10 Hz, SetTileType and Plot.SetVisible events. Anti-entropy digest (uid,hash) every 10 s; client requests what differs and removes extras.
- Pitfalls found:
  1. Runtime Savable.Load re-registers UIDs via ChangeActionable; on a registered object it allocates a NEW UID. Unregister (RemoveActionable) first.
  2. Creating objects at runtime with nested children (FarmerCarry, tree bees nest) breaks the UID registry unless ObjectTypeList.m_Loading = true (static field; do NOT call SetLoading, it wipes the registry). With the flag set, CreateObjectFromSaveName + Load behaves like the game's loader.
  3. Children are saved inside the parent AND listed as top-level savables (BeesNest). Exclude any UID that appears nested in another object's JSON from top-level sync, on both sides.
  4. Volatile fields: "STT" (state timer) everywhere, "S"/"ST" on animals change every frame or are reset by Load. Excluded from hashing/verification.
  5. CollectionManager.GetPlayers() contains each player once per collection (3x). GetPlayers()[0] is used for input, so each machine reorders its own player to index 0.
  6. Plot UIDs are local per machine; input targets that are plots are sent as "plot at tile".
  7. Campaign new game: player stands at the map corner (83,95 on an 84x96 start map) after the intro; spawn joiners on a free in-bounds tile.
- QA (test/run.sh): two game copies under Wine 9 + Xvfb, separate prefixes, TCP on localhost, bots via COOP_TEST=host|client using the real FarmerPlayer.GoToAndAction path. Ceremonies advanced via OnAcceptClicked.
  Last runs: 15/15, 15/15, 15/15 (earlier runs before the anti-entropy/nested fixes failed the checksum check 1 in 3).

## Unknown / not done
- Joiner building placement (edit mode) and bot teaching: not forwarded.
- Steam transport: not built. Internet play: not tested. Real Windows (non-Wine) run: not tested by me.
- Late-game performance: host scan was 34 ms on a fresh world with 1.3k objects under Wine (should drop the full scan for dirty tracking before big bases).

## 0.2.0 Steam transport (2026-10-20)
- Game ships Steamworks.NET 15.0.1 / SDK 1.51 (Plugins/Steamworks.NET.txt) with ISteamNetworkingMessages v002, Matchmaking v009. Reused with the game's own app id 979120.
- Bug in that Steamworks.NET: SteamNetworkingMessage_t.Release() passes m_pfnRelease instead of the message pointer. Call SteamAPI_SteamNetworkingMessage_t_Release(msgPtr) via own DllImport.
- Peer timeout 60 s, not 20: a world load freezes Unity's main thread, so keep-alives stop on both sides.
- Real-Steam lobby/invite path: untested (needs 2 PCs, 2 owning accounts). Framing/peers tested over a UDP stand-in: 22/22.

## 0.3.0 joiner building (2026-10-20)
- Choke points (client prefixes, forwarded to host): BuildingManager.AddBuilding (8-arg), BuildingManager.DestroyBuilding, BuildingManager.MoveBuildings, GameStateEdit.ConvertDragModels (drag-place builds directly, not via AddBuilding). Building.PlayerDeleted is virtual (Converter, ConverterFoundation override): patch all three. GameStateEdit.UndoAction is a private enum: patch AddUndoAction/Undo by name.
- Building.StopUsing does NOT remove the building from MapManager tiles. Removing a building on the client needs MapManager.RemoveBuilding + BuildingManager.RefreshBuilding first. In-place Load of a moved building leaves the tile map stale: recreate when TX/TY/Rotation change.
- Edit previews (cursor, drag models) are SetIsSavable(false) but still take UIDs. Client counter moved to 1e9 after join so local UIDs never collide with host UIDs; any *savable* object with a local-range UID is a sim side effect (felled tree dropping logs/seeds on the client) and is swept every 0.5 s.
- Clicks during a non-abortable action are ignored by FarmerPlayer.GoToAndAction. Host buffers the latest joiner click for 3 s.
- Test flake root cause: my test's drop tile sometimes had a tree/grass on it, so the drop never happened. Free-tile search now requires no objects and no water.
- QA: TCP 20/20 (x2 after fixes), UDP wire 22/22.

## 0.4.0 joiner bot teaching (2026-10-20)
- Teaching records into FarmerPlayer.m_TeachingActions (via FarmerPlayer.SendAction -> SendActionTeaching); GameStateTeachWorker2.UpdateTeachInstructions turns players[0].m_TeachingActions into editor instructions; TeachWorkerScriptEdit.OnClickGo -> Worker.NewHighScriptTaught(list) commits.
- Design: joiner runs the teach UI on its mirror bot. Forwarded: Farmer.StartLearning, Worker.RequestStopLearning, GameStateTeachWorker2.SetTarget/Close (host SetIsTeaching on the joiner's FarmerPlayer), Worker.NewHighScriptTaught (HighInstructionList.Save JSON), StopAllScripts/TogglePauseScript/StartStep, FarmerPlayer.UseWhistle. Host streams new m_TeachingActions entries back; joiner appends them to its player's list.
- The joiner's FarmerPlayer can be rebuilt by sync mid-teach (carry changes), which resets m_Teaching and the list. Keep one persistent list and re-attach after every rebuild.
- Sync must not rebuild the bot open in the editor (UI holds the reference): freeze it while state is TeachWorker/SelectWorker; on close, request a resend. In the normal view, if a selected bot is rebuilt, reselect the new copy.
- Plot targets inside instructions carry local plot UIDs: remap to the host's plot at the tile.
- Test trap: the taught stick ends up in the teacher's hands, so the bot's replay finds nothing. Assert "script arrived and started", not "bot carries a stick".
- QA: TCP 27/27 (x2), UDP wire 29/29.

## 0.5.0 locks, rename, inventory (2026-10-20)
- Edit locks: each machine reports the bot open in TeachWorkerScriptEdit while state is TeachWorker/SelectWorker; host keeps uid -> (name, owner key) and broadcasts. OnClickTeach/OnClickGo refuse locked bots; host also refuses joiner teach/script ops on bots locked by others. Labels via IMGUI + Camera.WorldToScreenPoint.
- Rename: Worker.SetWorkerName forwarded.
- Inventory screen: transfers happen in InventoryBar.DragEnd / InventoryBar.OnClick (component calls: FarmerCarry.RemoveObject/AddCarry/StowObjects, FarmerInventory/FarmerUpgrades.ReleaseObject/AttemptAdd, FarmerClothes.Add/Remove, Wardrobe/Aquarium). Forward slot descriptors (holder uid, slot type, index, object uid); host replays. Host only lets a joiner touch their own player, bots, wardrobes, aquariums.
- Inventory keys use FarmerPlayer.SendAction directly from GameStateBase.UpdateInventoryKeys (bypassing GoToAndAction). Flag the call and forward only those actions (whitelist).
- HudManager.StartGame binds the HUD inventory bar to players[0] at load = the host's player on a joiner. Rebind to my player; rebind any InventoryBar whose object was rebuilt (track at InventoryBar.SetObject).
- Bug: FarmerPlayer.PostCreate instantiates FarmerPlayerStatusIndicator under HudManager.m_IndicatorsRootTransform; screens like the inventory hide that root, the indicator's Awake hasn't run, SetState throws NRE, and the joiner's player silently vanished. Activate the root (and inactive ancestors) around runtime creation.
- QA: TCP 35/35, UDP wire 37/37.

## 0.6.0 menus, world state, leaving, animations (2026-10-20)
- Engaging a building opens its menu via GameStateManager.StartSelectBuilding / StartRenameSign, called from the building's SendAction(Engaged) on whatever machine runs the engage. A joiner's engage runs on the host, so the host's screen popped the joiner's menu. Host prefix redirects to the engaging peer; joiner sets the mirror building's m_Engager to its player and opens the menu. Menu ops: recipe = m_Engager.SendAction(SetValue ResultIndex), close = DisengageObject (forwarded and run locally), plus direct calls (Converter/ConverterFoundation.CancelBuild, ResearchStation.CancelResearch/SetCurrentResearchQuest, AnimalStation.StopAll, Storage.ToggleLock, Building.BeginUpgrade, RenameSign.EndEdit -> sign JSON). Escape only pops the state: forward a "menu closed" so the host disengages.
- World state: QuestManager (Load = Reset + reload), ResourceManager, DayNightManager, WorkerScriptManager (Load appends; clear m_Scripts/m_Folders/m_Stored first), WorkerGroupManager (appends; clear m_Groups), OffworldMissionsManager, sent when their Save JSON changes (1 s).
- Unlocks differ per player: Badge.SetCompleted calls QuestManager.UnlockObject from the local player's badges (not in the world save). Joiners get the host's m_ObjectsLocked set explicitly, re-applied after every QuestManager.Load.
- QuestManager.CompleteAll throws NRE in this build (don't use it as a test fixture).
- Walking is procedural (FarmerStateMove: +0.5 y on 5 of 8 frames, vibrating drives +0.1 on 2 of 4); action animations are Farmer.StartAnimation(name). Host forwards StartAnimation names; strips the hop from the position stream; joiner re-adds it while moving. Worker.Update zeroes animator speed when normal time is off: set it back on the joiner.
- Creating a FarmerPlayer while the inventory screen is open: FarmerPlayer.Restart -> FarmerInventory.SetCapacity -> GameStateInventory.CheckTargetUpdated hits a panel bound to the destroyed copy and throws mid-create, leaving the UID registered (every later create: "same key"). Skip that callback during apply, and on any create exception remove the half-built object and its UID from ObjectTypeList's dictionaries.
- QA: TCP 42/42 (x2), UDP wire 44/44.

## 0.7.0 unlocks, wardrobe, aquarium (2026-10-20)
- `QuestManager.m_BuildingsLocked` is separate from `m_ObjectsLocked`; both must follow the host or the joiner's build palette lags. Re-apply after every Quest section load (Quest load resets them from badges).
- `WardrobeManager` is a global store, not per-building. Sync it as its own world section; clear its `m_Objects` before `Load`, under `ObjectTypeList.m_Loading`.
- `StartWardrobe`/`StartAquarium` on the host must be redirected to the engaging joiner. Their `CanDoAction`/`SendAction` check `GetActualState()`; a remote player's engage needs it forced to Normal.
- `GameStateInventory.Close` calls `m_Engager.SendAction(Disengage)` on the panel's object. After a sync rebuild that object is a destroyed copy, so no disengage fires. Track the engaged uid and send the disengage yourself.
- The host can hold an object free-standing while the joiner's copy is still parented (stick in hand, bees nest in tree). In-place load doesn't unparent; detach and recreate.

## 0.8.0 version check, save/reload seats (2026-10-20)
- Hello carries mod + game version (`SaveLoadManager.GetVersion()`); protocol bumped to 2. Host sends a `reject` Info with the reason, closes 2 s later. Old hosts close silently; the joiner then says "host may run a different version".
- The host save already contains joiner FarmerPlayers. Missing: ownership. Store `{host, seats}` under an extra `AutonautsCoop` root key (Postfix `SaveJSON.Capture`, covers autosaves); read it in a Prefix on `LoadJSON.InitialSetup` (`LoadJSON.m_RootNode`). The game ignores unknown root keys.
- After a reload `GetPlayers()[0]` must stay the host's character even before hosting starts; the GetPlayers order patch uses the saved host uid while Role is Off.
- Seats are tied to the `TileManager` instance; a new world clears them.

## 0.9.0 bot server, catapult, spaceport, planning, speed (2026-10-21)
- Script library (`WorkerScriptManager`) and planning areas (`PlanningManager`, private `m_PlanningArea`, no UIDs) are edited with direct calls and field writes. Watch the whole section on the joiner while its editor is open, send the full section on change, host replaces and rebroadcasts. Hash after a JSON parse round trip or your own echo won't match.
- Replacing the library drops `StoredScript.m_LinkedBots`; re-link from each Worker's `m_WorkerInterpreter.m_LinkedScriptUID` and each `DataStorageCrude.m_LinkedScriptUID`.
- Upload then link: push the library before the link op, or the host links to a script it doesn't have.
- Rebuild an open `BotServerSelect` after a replace (private `m_Scripts`, `CreateScripts`, `UpdateLinkButtons`).
- Catapult aim: `GameStateSetTargetTile` previews `SetTargetTile` on every hover; send only on `Close(Restore:false)`.
- `DiscardRegularMission` rolls a random replacement: host only. Missions are identified by `m_ID` (+ `m_Daily`).
- Screens whose building gets rebuilt by sync while open lose `m_Building`/`m_Engager`, so their OnDestroy disengage never fires. Send a fallback disengage when a menu state closes without one.
- Speed: `TimeManager.m_TimeScale` + `m_PauseScreenEnabled` as a world section; block `ToggleFastTime`/`SetTimeScale` on joiners.

## 0.10.0 groups, undo, leave/rejoin, saves, bug report, big bases (2026-10-21)
- Bot groups: `WorkerGroupManager` (ids + worker uid lists) with panels in `TabWorkers`. Replace = `SetWorkerGroup(w, null)` for grouped bots, `TabWorkers.RemoveGroup` each group, then `Load` (it re-adds panels/members). A bot rebuilt by sync loses `m_Group`; put it back in its group after create. Group buttons (`EditGroup.Stop/Play/Pause/DropAll/CallAllChildren`, private, recursive) run on the host.
- Undo: the joiner's `GameStateEdit` history is local and empty. Keep one step per joiner on the host (added uids / deleted type+tile+rot / moved old positions); show it on the palette by setting the private `m_UndoAction` enum and calling `UpdateButtons`.
- Leave: save the FarmerPlayer JSON (nested carry/inventory/upgrades/clothes), destroy it and its children, keep the JSON in the save under `AutonautsCoop.parked`. Restore under `ObjectTypeList.m_Loading` so UIDs stay. `CollectionManager.m_Players` keeps destroyed/pooled entries after StopUsing: remove them or `GetPlayers()[0]` breaks. A world saved with joiners online: park them in a Postfix on `LoadJSON.EndLoad`.
- Pooled objects: a destroyed copy can still be truthy with `m_UniqueID == -1`. Check the uid, not just `(bool)`.
- `ConverterFoundation` names its building-to-be by `"NewID"`, not `"UID"`; treat it as a nested child or in-place Load hits "same key" and leaves a half-built Converter that breaks every later save.
- TCP close drops queued frames; flush before closing ("lobby ended", refusals). Process the inbox before checking `Alive`, or the reason is lost.
- Joiner saves: block `SaveLoadManager.DoAutosave` (private) and QuickSave to the session name; Save As works.
- New-game options: the main menu's background world sets `SaveLoadManager.m_MiniMap = true` (84x96) until it finishes; set it false yourself. `m_Options.m_PlayerPosition` stays from the previous size: reset it or `WorldGenerator.GetPlayerStartPosition` indexes out of bounds on a small map.
- Big bases: full serialization costs ~0.035 ms/object. Incremental: rescan movers every scan, changed things for 3 s, moved things, plus a ring slice (full round ~5 s). Paths (`SandPath/StonePath/Road*`) and `Floor2D` pick rotation from neighbours per machine: leave Rotation out of hashes.
- Test sandbox: two medium-map instances (~3 GB each) get OOM-killed; stress test runs on a small map with a dense base.

## 0.11-0.12 real saves, garbage, editors (2026-10-21..26)
- Test on a real late-game save early: the first one (Settlement mode, ~10,800 objects, Workshop Lua mods) broke the joiner within seconds while the new-world suite was green.
- A bot's program names the buildings it uses by `"UID"` inside instruction JSON. Only count `{"ID":"<type>","UID":n` as nested children, or every bot update destroys those buildings on the joiner.
- The game saves floats as whole hundredths and loses one per load/save round trip; load the host's values so they save back exactly, or every verify fails and objects are rebuilt.
- A storage's loaded count waits in private `m_TempStored` for a Refresh the game skips for stacked storage; set `m_Stored` and clear the temp yourself.
- Settlement's Transmitter makes local rewards on the joiner; give them UIDs from a joiner-only range or they take the host's next UIDs.
- Workshop (Lua) mods run on the joiner too: block their world-changing `ModManager` calls there (storage writes, spawn, destroy, move...).
- Garbage, not frame time, is the cost on a big base (Unity 2018 GC pauses ~200 ms on a 1 GB heap). Cheap fingerprints (a field whitelist via reflection) before a full re-save; send saved text without parse/re-serialize.
- Huts: reload the hut without its folk (`Folks` emptied), then each folk; folk state goes to a waiting field only `Refresh` applies, so call the private `SetState` directly.

## 0.12.37 in-place reloads (2026-10-26)
- `Converter.Load` ends with `SetState(saved)`. Converting -> Converting runs `EndConverting` on your copy: ingredients destroyed, a blueprint stage counted, the mods' "converter complete" callback fired. Set `m_State = Idle` (and stop its private `m_PlaySound`) before an in-place Load.
- `ConverterFoundation.Load` caps `Stage` at stages-1; the host saves "all stages done" for the frame a build completes. Set `m_Stage` from the JSON after Load.
- `Housing.OnDestroy` walks `m_Folks`; at quit the session is reset before the scene tears down, so a `Role == Client` guard misses it: drop dead entries unconditionally.
- Four game copies with a late-game save fit on a 32 GB PC (test kit `mode=four`).

## 0.12.38-0.13.0 native menus, Creative, beta (2026-10-26)
- UI from the game's own prefabs: clone a menu's text button (hide every child except Shadow/Back/Border/Text), the sign-rename panel (`Prefabs/Hud/RenameSign`) for a text-entry dialog, the warning rollover (`Prefabs/Hud/Rollovers/WarningRollover`, destroy its `WarningRollover` component) for notices, `GameStateConfirm` for message boxes (override the title text after `SetConfirm`).
- `BaseMenu`: add buttons in a Postfix of `Awake` (before `Start` registers clicks) or call `RegisterGadget` + `AddAction` after `Start`.
- The pause screen has no free row (Planet Scan panel above Save/Load/Continue): the right margin fits 190x70 buttons at 1280x720.
- The title screen (`State.Start`) comes before the main menu; `SetState(MainMenu)` to skip it in tests. Starting a new world ~2 s into the title screen fails in `WorldGenerator.CreateObjects` (background map preview, `m_MiniMap`); start at once.
- Creative spawn (`CreativeTools.OnObjectClicked`) creates objects locally: forward type/kind/extra/shift to the host and replay it for the joiner's character.
- A joiner opening Load while connected crashes: hide `LoadButton` on the joiner's pause screen and refuse `Paused.OnLoad` / `GameStateLoad.Init`.
- Author's two-PC Steam session over the internet (2026-10-26): hosting, invites, joining and play worked; found the two issues above.
