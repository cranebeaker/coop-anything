# Crime Scene Cleaner - notes

Research notes on the game for co-op work, from an earlier, unpublished prototype (BepInEx plugin, its own sync
core, Steam through the game's `steam_api64.dll`). That work went on in the existing mod,
[Crime Scene Cleaner Co-op](https://github.com/gogoigor51-tech/crime-scene-cleaner-coop) (see
[README.md](README.md)): its fixes are in
[crime-scene-cleaner-coop#1](https://github.com/gogoigor51-tech/crime-scene-cleaner-coop/pull/1) (open PR,
2026-11). "Observed" means seen in the game or in a bot run; everything else is marked.

## Build

- Steam app 1040200, build 26046462. Unity 2021.3.45f2, IL2CPP (metadata v31), URP, 64-bit Windows.
- Libraries: Steamworks.NET, Rewired (input), Wwise, DOTween, Easy Save 3 (saves), PixelCrushers Dialogue System
  (quests), Odin, Cinemachine, UniRx, Addressables, Unity Localization. No networking library.
- Game logic: `MainGame.dll` (quests, cleaning, report, lamps, doors) and `Assembly-CSharp.dll` (player, grabbing,
  inventory, bodies, missions, saves).

## Tooling

- BepInEx 6.0.0-pre.2 fails (its Cpp2IL reads metadata 23-29; the game exits at once). Bleeding-edge be.791 works.
- Cpp2IL 2022.1 `--output-as isil` gives per-method call lists; field offsets show up as `[rcx+N]`, which was
  enough to see where a flag is written (doors, UV lamps below). Many calls go through UnityEvents and virtual
  calls, so "who calls this" often has no direct caller.
- Saves live inside the install (`CrimeCleaner_Data/Saves/<account>/Slot1.es3`), so a copy of the install is fully
  isolated. Two copies can run at once on one PC (the second hard-linked to the first, with its own `BepInEx` and
  `Saves`).

## Pitfalls (observed)

- IL2CPP folds identical method bodies: a Harmony patch on one method also fires for other types
  (`PickableDoors.ValidateUnlock`, which is just `return false`: 75,000 NullReferenceExceptions). Patch real logic
  and cast the instance first.
- `BodyRagdollController.KYS` runs per bone per frame (about 40,000 calls a second): don't hook it.
- A generic prefix (`object[] __args`) on `MissionController.SendNewMails` made the game's own calls throw.
- `GameObject.CreatePrimitive` keeps the built-in `Standard` material; URP draws only its shadow. Use
  `Universal Render Pipeline/Unlit`.
- `SaveSystem.StartGame(slot)` starts a new game in that slot; loading a save is `SaveSystem.LoadSlot`.
- Numbers formatted with the system locale (Russian: "0,5") broke log parsing: format with the invariant culture.
- An interop call through a base-class wrapper (`LampController.Interact` on a UV lamp) is not virtual: call the
  derived type's method.
- `Rigidbody.IsSleeping()` said "asleep" while a knocked item moved 20 cm; judge movement by position.
- Test runs: the game locks the cursor every frame (`Controller_Switch`, `FirstPersonAIO`, `MoveCamera`,
  `InputController`) and keeps it clipped through its loading stalls; it also takes focus when its window opens and
  when a job loads. Rewired already ignores input while unfocused (`ignoreInputWhenAppNotInFocus` is true).

## How the game works (observed)

- Items: `GrabableItemController`. Grab `GrabController.GrabItem`, throw `TryRelease(force)`. Players' own tools
  (bucket, lamps, power washer station) are `PortableItem`s made from the inventory, inactive while in it; put back
  with `PortableItem.PickUp` (the station overrides it). Take one out the game's way with
  `InventoryController.Equip(category, 0)` or `EquipItem(category, slot)`.
- Stains: `Taint` + `CleanableDecal`. Intensity follows the decal texture (`CleanableDecal.Update` ->
  `Taint.SetIntensity`), and strokes only erase it. Every way of adding blood (dirty tool `CleaningItem.CleanOrTaint`,
  footprints `FootprintSpawner.LeaveFootprint`, bucket spill, drips) makes a new decal through
  `BloodSpreaderController.ProjectBlood`; no existing stain gets dirtier (probe: a dirty mop over a cleaned stain and
  blood on top left it unchanged; no rise on any of 371 stains). The type-only `ProjectBlood` takes one prefab per
  blood type; footprints pass their own.
- Object names can contain '/': the floors in Developers' Hell are under a root named `WALLS/WINDOWS/FLOORS`, so a
  hierarchy path must not be split at every '/'.
- Doors: `ShortcutDoors` (84 in Developers' Hell). A click is `Interact` -> `OnOpen` -> `Toggle`; a push
  `TryForceOpen`/`TryForceClose`; a switch `ExternalToggle`. None calls `OpenDoors`/`CloseDoors`. All of them set
  `isClosed` at once. Requirements: AnySide, FrontSide, BackSide, RequireKey, Locked. Card readers (BoxBox only) are
  key doors. `PickableDoors.TryOpen` always starts the lockpicking minigame (no such door was found in any job scene).
- Keys: `ItemInspector.OnKeyCollected` -> `KeyHolder.CollectKey`; each player has their own `KeyHolder`.
- Bodies: `BodyRagdollController`; grab `BodyGrabber.GrabBody`, throw `TryReleaseBody`; the van is
  `TrunkController` -> `GarbageItem.CollectAndDestroy` -> `BodyQuest.CollectBody`. `BodyQuest` adds 1 to the value
  it reads from Lua, so a replayed collect counted one body twice.
- Quests: Dialogue System quests (`QuestLog`, `DialogueLua`); per-room counters (`.Current`, `.Max`) are recomputed
  each frame from the world, so they must not be shared. State strings are lowercase: use the typed overloads.
- Breaking: `DestroyableItem.OnCollisionEnter` -> `CrackItem` (the same prefab gives the same shards, listed in
  `CreatedCracks`); parts: `CrackedObject.CrackItem(direction)`.
- Trash: the trash can (`GarbageCollector`) makes bags (`GarbageBagConnector`); bags merge in `OnCollisionEnter`
  (the faster grows, the other is destroyed), limited by perks.
- Valuables: `MissionStealingController.Steal` -> `StealableItem.OnSteal`; the report pays from the stolen list.
- Leaving: `MissionLeaveController.LeaveMissionWithReport` (quick save, report); money on closing the report.
- Buckets: `LiquidContainer`; products (`activeBuffs`) need a full bucket and a free slot (one for Bucket 1 Star);
  `CleanTool(mop)` copies them onto the tool. The power washer station needs a perk for products.
- Lamps: `LampController.Switch(on)` only sets the light; `Interact` flips `isOn` and calls it.
- UV lamps light blood through one shader matrix: `UVLampController` takes a slot when enabled
  (`CleaningSenseController.TryGetSenseIndex` into `cleanerSenseIndex`, -1 when none is left) and lights only slots
  above 0: three UV lights at a time for everyone. `Interact` does nothing unless the lamp stands
  (`DetectObjectOrientation.IsStanding`), and `Update` switches a fallen one off.
- Missions (`MissionController.missions.data`): 0 Dealer's Apartment, 1 Villa, 2 Spa, 3 Toxic Duo, 5 Short Circuit
  (Smart House), 6 Party's Over, 7 Pizzeria, 8 Tyler, 9 Modern Art, 10 Warehouse, 11 Game Dev (Developers' Hell,
  scene `GameDevOffice`), 12 Lab, 18-21 later jobs.
- No `TrashSpawner` in any of the 15 job scenes of the base game (all loaded, inactive objects included).

## Steam

- The game initialises Steam with its real app id, so a plugin can use its `steam_api64.dll` flat API (no Spacewar).
  Observed: `ISteamNetworkingMessages` to self (single account), then between the host and a tester on two real
  Steam accounts (players and items shared; the tester's report).
- The game dispatches Steam manually through Steamworks.NET's `CallbackDispatcher`. Polling `GetAPICallResult`
  fails (reason 2) and `SteamAPI_RegisterCallResult` is never called back; a class derived from
  `Steamworks.CallResult`, added to `CallbackDispatcher.m_registeredCallResults[call]`, gets `CreateLobby`'s result
  (observed on one account: lobby created, data written, left). See
  [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md).
- Not tested: joining a lobby from a second Steam account, the invite overlay, 3-4 players.

## Fixes for Crime Scene Cleaner Co-op (open PR, 2026-11)

Each with a bot suite in that repo (two copies on one PC over localhost) that fails with the old behaviour (a
"mutant" build) and passes with the fix. Not yet played by testers.

- CCTV recordings: the game makes them with `new CCTVRecord(name)`, so Unity's `== null` is true for each; the UI
  deletes through `HackableComputerPresenter.HandleFileDelete`. Deleting them is now shared (`cctv` 7/7).
- A shard picked up by another player's trash can now goes away on the machine that simulates it (`breakhit` 6/6).
- Items at rest get physics near the action (a client's thrown item used to bounce off resting items as off a
  wall; physics everywhere made vases tip over by themselves) (`bump` 7/7).
- Anyone can carry another player's bucket or lamp; only the player it belongs to can put it into an inventory (`toolcarry` 7/7).
- Doors share their open/closed state after every door path; a replayed toggle had left doors inverted for good
  (`doorsync` 4/4). Stains on objects under names with '/' are shared (`footsteps`: 12 of 12 missing before, 5/5).
- Bucket and power washer products are shared, and using another player's bucket changes it for everyone
  (`products` 6/6).
- Players' own tools are left out of the scene tables (a replay by index switched the other player's own lamp), and
  UV light slots go to each player's own lamps first (`lamps` 10/10).
- Test runs skip the intros and leave the mouse alone (cursor freed, focus given back, clip released).

## Open (seen by a tester on Developers' Hell, not fixed)

- The elevator moves every player, also those outside it.
- Heavy furniture lifted by two players at once ends up not lifted on one screen.
- Two merging trash bags make duplicates for a while.
- The Super Stacker arcade machine opens the minigame for everyone and breaks the session ("player in another
  place" although all are in the same level).
- Other players' flashlights are not shown.
- The report's score animations play twice, about a second apart.
