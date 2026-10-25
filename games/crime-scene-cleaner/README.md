# Crime Scene Cleaner (2024)

| | |
|---|---|
| Engine | Unity 2021.3.45f2, IL2CPP, URP (BepInEx 6 bleeding edge, build be.791) |
| Versions tested | Steam, app 1040200, buildid 26046462, Windows 10; `CrimeCleaner.exe` SHA-256 `e9167a32...4f560173`, `GameAssembly.dll` SHA-256 `8af483d7...ae999fa3847` (full hashes in [NOTES.md](NOTES.md)) |
| Projects | CSC Coop prototype (this write-up; source not published yet) |
| QA level | automated (seven pass/fail live suites, two real game copies on one PC over loopback, each with a mutant that must fail; players and items also played by the owner on two Steam accounts) |
| Last updated | 2026-10-25 |

## Where things stand

A BepInEx plugin lets 2 players (designed for up to 4) work the same job together. The owner played three sessions with a friend on two real Steam accounts, which covered players and items: players appear as cyan capsules, and grabbing, carrying and throwing items stays in sync.

Since then the plugin also shares the following, tested so far only with two game copies on one PC:
- blood and other stains, including stains made during play and cleaning progress
- bodies: carrying, throwing, loading them into the van
- quest progress
- doors, including key doors, card-reader doors and one-side doors, and the keys themselves
- breaking objects, with the same shards for everyone
- trash picked up with the trash can, trash bags, merging bags, loading bags into the van
- each player's own bucket, lamp and power washer, and the cleaning products in them; anyone can use anyone's (rinse a mop or sponge in another player's bucket)
- scene lamps, stolen valuables (their money is on everyone's report), collected evidence
- leaving the crime scene: when one player leaves, everyone leaves and gets their own report and money in their own save

Players meet in a Steam lobby (a small in-game panel), with no SteamIDs to type. When the host starts a job from their hub, everyone else's game starts the same job. Creating a lobby was checked on the owner's Steam account; joining one has not been tried with a second account yet.

## Existing work

- No co-op or multiplayer mod was found. Only one general web search was run (2026-10-23), and the owner reports no prior attempt. GitHub, Nexus, Steam Workshop and Discord were **not** searched.
- [Crowd Control's Crime Scene Cleaner guide](https://crowdcontrol.live/guides/crimescenecleaner) shows that a BepInEx-based mod for the Steam build exists. That was the only hint that BepInEx loads on this game.

## How the game works (for co-op)

- **Engine:** Unity 2021.3.45f2, compiled with IL2CPP (`GameAssembly.dll` plus `il2cpp_data/Metadata/global-metadata.dat`, metadata version 31). Rendering is URP. Libraries in the build: Steamworks.NET, Rewired, Wwise, DOTween, Easy Save 3, PixelCrushers Dialogue System, Odin, Cinemachine, UniRx, Addressables.
- **Modding:**
  - BepInEx 6.0.0-pre.2 does **not** load. Its Cpp2IL supports IL2CPP metadata 23-29, not 31, and the game exits at once.
  - BepInEx 6 bleeding-edge be.791 (`builds.bepinex.dev`) works. Its first start generates the interop assemblies (about a minute).
  - Cpp2IL 2022.1.0-pre-release.21 dumps the metadata on its own.
- **Netcode:** none. No Mirror, Photon, Netcode for GameObjects or UNet types are in the metadata. `steam_api64.dll` is loaded and initialised by the game itself, with the game's real app id.
- **Game logic:** split between `MainGame.dll` and `Assembly-CSharp.dll`. Classes useful for co-op:
  - `FirstPersonController`: `CanMove` becomes true when the mission intro cutscene ends, and `IsBlocked` also exists.
  - `GrabController`: `GrabbedRb`, `GrabItem(item)`, `TryRelease(Vector3 force)` (a throw), `TryRelease()`.
  - `GrabableItemController`: `Rb`, `startingWorldPosition`, `IsHeld`.
  - Stains: `Taint` (with `CleanableDecal`), created by `BloodSpreaderController.ProjectBlood`; cleaning lowers `Taint.Intensity` and `DestroyDecal` removes the stain.
  - Bodies: `BodyRagdollController` with `RbToBone` bones, `GrabController.BodyGrabber.GrabBody`, and the van's `TrunkController`, which ends in `GarbageItem.CollectAndDestroy` (the object is switched off, not destroyed).
  - Quests: PixelCrushers Dialogue System (`QuestLog`, `DialogueLua`), wrapped by `DialogueSystemQuestController`.
  - Breakables: `DestroyableItem.CrackItem` swaps the item for a prefab of shards (`CreatedCracks`, new grabbable items); `DestroyableObject` breaks into `CrackedObject` parts.
  - Trash: the trash can is `GarbageCollector` (tool slot `trash`); picked-up `CollectableItem`s are switched off; `SpawnBag` makes a `GarbageBagConnector` bag; bags merge in `GarbageBagConnector.OnCollisionEnter`, limited by perks.
  - Doors: `ShortcutDoors` with a requirement (AnySide, FrontSide, BackSide, RequireKey, Locked). `IsLocked` is the "not opened yet" flag, and `UnlockDoors()` clears it. Keys go into the player's `KeyHolder` (`ItemInspector.OnKeyCollected` -> `KeyHolder.CollectKey`).
  - Card readers (`DoorsCardReader`, only in the "BoxBox" job) are key doors whose reader lights follow the door's `OnUnlock`. Lockpicked doors (`PickableDoors`, a lockpick minigame) exist in the code but in none of the job scenes (every job scene was loaded and counted).
  - Valuables: `StealableItem`, stolen through `MissionStealingController.Steal`; the report pays from its `stolenItems` list. Evidence: `InspectableItem.Collect`.
  - Leaving: `MissionLeaveController.LeaveMissionWithReport`. The report is computed on each machine from its own world; closing it pays (`PerksController.AddMoney`).
  - Tools: a bucket, lamp or power washer station is a `PortableItem` the game makes from the player's inventory, active while held or set down. Products go into a full bucket with `LiquidContainer.TryAddBuff`; `CleanTool(mop or sponge)` copies them onto the tool. A power washer needs a perk for products.
  - Jobs start through `GameManager.StartMission(MissionData, variant)` (the hub's job map); `MissionController.GetMission(id)` finds a job by id.
  - `SaveSystem`, `PlayerSave`.
- **Method bodies:** Cpp2IL's `--output-as isil` writes the calls of each method, which is enough to find which game function does what. Field names are not resolved there, so findings were confirmed in game with probe plugins.
- **Saves:** Easy Save 3 files are written **inside the install** (`CrimeCleaner_Data/Saves/<steam id>/Slot1.es3` and `SavesBackups0/`), not under `%USERPROFILE%\AppData\LocalLow`. That folder (`President Studio/CrimeCleaner`) holds only `Player.log`. A copied install therefore has fully separate saves.
- **Starting a job without menus:** `SaveSystem.Instance.StartGame(1)` from the main menu (`MenuScreen_Nightmare` scene) loads slot 1. The test save went `LoadingScreenScene` → `HubResidence-GameIntro` → the "Dealer's Apartment" job (554 `GrabableItemController` objects).
- **Several copies on one PC:** a hard-linked second install runs alongside the first. No single-instance mutex or window check was seen. Only `BepInEx/`, `CrimeCleaner_Data/Saves`, `PlayerPrefs.es3`, `KeyBindingSaveFile.es3` and `doorstop_config.ini` need real copies.

## What we changed

The co-op layer is a BepInEx plugin (`CscCoop.dll`) plus a game-free sync core (`CoopCore.dll`) with its own test program.

1. **Transport.**
   - Players connect through `ISteamNetworkingMessages`, called with the flat C API on the `steam_api64.dll` the game has already initialised.
   - Every player owns the game, so the real app id is used and the [Spacewar](../../playbook/networking-steam-spacewar.md) trick is not needed.
   - Steam callbacks are not used for messages: they are polled. Sessions are accepted for every lobby member (before the lobby: every SteamID listed in `BepInEx/coop.cfg`).
   - A loopback UDP transport with the same interface is used for local tests.
   - x64 message layout confirmed with a send-to-self test: data at offset 0, size at 8, sender identity at 16.
2. **Players.**
   - Each player sends their position and yaw at 20 Hz. The host relays poses to everyone, and the newest sequence number wins.
   - **Bug:** remote players drew only a shadow. `GameObject.CreatePrimitive` uses the built-in `Standard` shader. Unity reports it as supported, but this URP game renders only its shadow pass. Fix: a material from `Shader.Find("Universal Render Pipeline/Unlit")`, coloured cyan. If that shader is missing, a URP material is copied from the scene.
   - **Bug:** a player's capsule stayed after they left. Fix: a bye message sent on quit, plus a 5 s silence timeout. A restarted player is accepted again from sequence 1.
3. **Items.**
   - **Authority:** the host is authoritative. Items are identified by their index in a table sorted by name and `startingWorldPosition`. Both PCs logged the same table hash, `5962476C` for n=554.
   - **Holding:** the holder owns an item; the host simulates everything else.
   - **Versions:** the host stamps every update with a version. Clients send a clock-seeded proposal counter, so duplicate and late client packets are dropped.
   - **Bug:** thrown items dropped straight down on the other PC. Updates had no velocity, and the receiver zeroed it. Fix: updates carry linear and angular velocity, and the release packet is resent 4 times.
   - **Bug:** a client's own throw stuttered.
     - Cause, measured with a per-packet trace: the host's copy trails by about a round trip, 5-17 cm in flight, and the client snapped back to each of the host's updates.
     - Fix: for 5 s after a local throw, the client ignores the host while either copy still moves. Skipped updates are not recorded as known, so the host's final state still arrives.
   - **Bug:** after a throw, the item jumped a short distance once it stopped. Two PCs' physics land it in slightly different places. Fix: if both copies are at rest and 5 cm to 1.5 m apart, the client slides the item to the host's spot over 0.2-0.6 s (smoothstep, kinematic while sliding).
   - **Leaving:** a player who leaves has their held items freed by the host.
   - **Bug, found by the world suite:** the host's repeated snapshots carry the same version, so a client copy that a player bumped while it rested was never corrected. Fix: a client slides a resting item that drifted back to the host's spot and holds it still until the host moves it or a player picks it up (at most 5 tries per item a minute). Without the hold, a pile of tin cans that settled differently kept knocking itself over.
4. **Stains.**
   - Every machine scans the `Taint` objects every 0.25 s. A new stain, a change in intensity or a removal becomes a record: the newest version wins, and a removal is final.
   - New stains are rebuilt on the other machines with the creator's own `ProjectBlood` arguments, then given the exact transform. A stain both machines made from the same physics event is merged, not doubled.
   - Scene stains are matched by hierarchy path. Sorting by position failed because stains on furniture settle a fraction of a millimetre differently on each PC.
   - **Bug:** small cleaning steps (under 0.02) were never sent. Fix: a stain that has stopped changing for 0.6 s sends its exact value.
5. **Bodies.**
   - The machine carrying or simulating a body sends all bone poses at 10 Hz; the others follow kinematically and glide. A thrown body is simulated by the thrower until it rests, then by the host.
   - The host grants each body to one player at a time, so a second player's grab is refused.
   - The van only counts a body on the machine simulating it. The others replay the game's own van collection, so each machine's quest counts the body once.
6. **Quests.**
   - Quest states, quest entries and event variables (such as "room discovered") are shared. Quest states only move forward, flags only turn on, and other values are newest-wins.
   - Not shared: each player's current room, and the progress counters, which every machine recomputes each frame from its own world.
   - **Bug:** writing a quest state as text did nothing (the Dialogue System uses lowercase names). Fix: the typed overload, and a warning when the game reads back something else.
7. **World: doors, keys, breakables, trash, bags.**
   - Key/value records on the same sync as quests, under their own message type: door open (newest wins); door unlocked, key picked up, item broken, object part broken, trash collected (flags); bag made, bag size, bag merged, item loaded into the van.
   - Replay goes through the game's own functions: `UnlockDoors`/`OpenDoors`/`CloseDoors`, `KeyHolder.CollectKey` plus hiding the key (as loading a save does), `CrackItem` (the same prefab gives the same shards), `CollectableItem.Collect`, and the trash can's own bag prefab with `SetLevel`.
   - Objects made during play (shards, broken-off parts, bags) get ids every machine derives the same way, so they join item sync without any extra messages.
   - Who decides: a hit breaks an item, and two bags merge, only on the machine of the player holding or just throwing it, otherwise on the host. So the thrower's perks decide how far bags merge.
   - Also on these records: scene lamps, stolen valuables (replayed with `AddStolenItemFunds`), evidence (`InspectableItem.Collect`), and leaving (sent at once, since the scene ends soon; the others call `LeaveMissionWithReport` too).
   - Lockpicked doors use the door records; once one is unlocked anywhere, it opens like a normal door instead of starting the minigame again (not exercised: no such door in any scene).
   - **Bug:** patching `PickableDoors.ValidateUnlock` (it only returns false) ran the patch for unrelated objects, 75,000 errors in one run: its native body is shared with other identical methods. That hook was removed.
8. **Each player's tools.**
   - An active `PortableItem` that is not in the scene's item table is this player's own tool. It gets an id from the player's own range and joins item sync. The other machines make a copy from the same tool data's prefab.
   - Its state (water, dirt, products; lamp on/off) is a record anyone may change, so a product poured into a copy reaches the owner's tool. Only the owner can take a tool into their inventory.
9. **Lobby.**
   - Steam lobby through the flat API. The owner hosts as player 0 and gives members 1..3 in the lobby data. Mod and game versions must match.
   - **Bug, seen live:** the lobby's result never arrived. The game dispatches Steam through Steamworks.NET's manual dispatcher, which frees results nobody registered with it (reason 2, invalid handle), and `SteamAPI_RegisterCallResult` does nothing in that mode. Steamworks.NET's generic `CallResult<T>` is stripped from this build. Fix: a class derived from the surviving `Steamworks.CallResult` base, injected and added to `CallbackDispatcher.m_registeredCallResults`.
   - When the host starts a job, the lobby carries a job ticket; a member in their own hub starts the same job through `GameManager.StartMission`. Sync only runs while a player is in the host's job (each hub is that player's own world).
   - **Bug:** a client still on the previous run's report screen counted the new job as joined and never followed. Now a job counts as joined only when entered after the ticket.
   - A panel (IMGUI, bottom left of the main menu, F8 elsewhere): Host co-op, Join <friend>, Invite, Leave. For tests on one PC a folder stands in for the Steam lobby.

## QA

- **Game-free core tests:** `dotnet run -c Release` in the core's test project. 104 checks, ALL PASS (2026-10-25). They cover codecs and bad packets, pose and item convergence at 30% loss, hold exclusivity, late join and leave. For stains, bodies, quests and world state each, they also cover the merge rules, lossy 4-peer runs with a late joiner that must end identical, and a mutant without the repair cycle that must diverge. Lobby rules: player numbers, version checks, job tickets.
- **Live suites, pass/fail** (2026-10-24 and 2026-10-25). A script starts two real game copies on one PC over loopback. Bots drive the game's own functions, then each copy prints a state line and the script checks both. Each suite also runs as a mutant, with the client ignoring that feature's messages, and must then fail.

  | Suite | What the bots do | Result | Mutant |
  |---|---|---|---|
  | `stain_qa.ps1` | both mop with the real mop, bleed, leave blood, mop a scene stain | PASS (twice, identical digest) | FAIL (5) |
  | `body_qa.ps1` | host loads a body into the van; client carries and throws one; grab conflict; host carries it last | PASS | FAIL (4) |
  | `quest_qa.ps1` | quests started and finished on either side, a conflict, a step backwards, the client discovers a room | PASS | FAIL (4) |
  | `world_qa.ps1` | doors opened and closed from both sides; one-side door from front and back; key taken by the host, key door opened by the client; bottles broken by impact on each side; plank broken; trash can on both sides (the client picks up a shard of the host's bottle); bags merged and loaded into the van | PASS (20 checks; shards within 0.01 m) | FAIL (14) |
  | `tool_qa.ps1` | both set down a bucket; the client rinses mop and sponge in the host's (gets its product) and cannot pick it up; the host pours a product into the client's bucket and power washer; the client switches the host's lamp on; a scene lamp off; each steals a valuable; the client collects evidence; the host leaves and the client follows; both close their report | PASS (23 checks; tools within 0.01 m; identical reports, 200 from valuables, 200 paid into each save) | FAIL (14) |
  | `card_qa.ps1` | both load the "BoxBox" job; the host takes the key card; the client opens the card door | PASS (door open and all 4 readers green on both) | FAIL (5) |
  | `lobby_qa.ps1` | with the lobby stand-in: host creates, client joins (player 1, no SteamIDs); both play a job; the host leaves; from their hubs the host starts the job again and the client follows by itself; the client leaves the lobby | PASS (8 checks; same item table, sync on in the job, off outside it) | FAIL (5) |

  After each new feature the earlier suites were run again and passed.
- **Earlier bot runs read by hand:** movement, ghost screenshot (a solid cyan capsule), item throws (0 corrections during a throw, copies within 1 cm), host quit and host kill.
- **Real Steam, two accounts** (owner and a friend, reported by the owner, 2026-10-23): capsules visible; items synced, and throws land in the same place and look smooth; capsule removed on quit, and held items free to take; landing jump gone after the slide fix. Everything added since (stains, bodies, quests, world state, tools, leaving, joining a lobby) has **not** been tried over Steam yet.
- **Real Steam lobby, one account** (2026-10-25): created a friends-only lobby, the host's lobby data read back, a session started as host, left cleanly; the panel was checked on screenshots.

## How to play

There is no release yet. The test kit is BepInEx be.791 plus the two plugin DLLs plus a `coop.cfg` that says `lobby=steam`, unzipped into the game folder. In the main menu one player clicks "Host co-op" and the others "Join <name>". Everyone loads their own save and goes to their hub; when the host starts a job, the others' games start it too. Start jobs fresh, not from a half-finished save of that job.

## Known issues

- Not tried over Steam yet: everything but players and items, including joining a lobby (one Steam account cannot join its own lobby).
- Only 2 players tested; 3 and 4 are designed for but have never been run.
- Not exercised:
  - a successfully finished job (the bots leave unfinished jobs: only the valuables were paid, not the job reward)
  - the dealer's body (not grabbable at job start)
  - a real bag-on-bag merge (needs a perk; the test triggered the merge directly)
  - the game's own key pickup and inspect screens (the bots collect directly)
  - lockpicked doors (none in any job scene)
  - filling a bucket at a tap, dirty water
  - joining a job already in progress, resuming a job from a mid-job save (a member starts it fresh), a member who lacks the host's job
  - dialogue state
- Each player's mission time is their own (it did not change the pay in the test).
- Players see each other as plain capsules, with no character model or animation.
- The client trusts its own claim of who sent a packet. That is fine between friends, but there is no protection against a cheating player.
- Every player must load the same job from a save in the same state: scene items, stains, bodies and doors are matched by their place in the scene.

## Next steps

1. **Steam test** with the friend: joining the lobby, following the host into a job, then everything synced.
2. **A real 3- and 4-player test.**
3. **Joining a job in progress:** a full state transfer from the host.
4. **Publish the source** in its own repository and link it here.
