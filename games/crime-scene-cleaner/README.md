# Crime Scene Cleaner (2024)

| | |
|---|---|
| Engine | Unity 2021.3.45f2, IL2CPP, URP (BepInEx 6 bleeding edge, build be.791) |
| Versions tested | Steam, app 1040200, buildid 26046462, Windows 10; `CrimeCleaner.exe` SHA-256 `e9167a32...4f560173`, `GameAssembly.dll` SHA-256 `8af483d7...ae999fa3847` (full hashes in [NOTES.md](NOTES.md)) |
| Projects | CSC Coop prototype (this write-up; source not published yet) |
| QA level | automated (four pass/fail live suites, two real game copies on one PC over loopback, each with a mutant that must fail; players and items also played by the owner on two Steam accounts) |
| Last updated | 2026-10-24 |

## Where things stand

A BepInEx plugin lets 2 players (designed for up to 4) work the same job together. The owner played three sessions with a friend on two real Steam accounts, which covered players and items: players appear as cyan capsules, and grabbing, carrying and throwing items stays in sync.

Since then the plugin also shares the following, tested so far only with two game copies on one PC:
- blood and other stains, including stains made during play and cleaning progress
- bodies: carrying, throwing, loading them into the van
- quest progress
- doors, including key doors and one-side doors, and the keys themselves
- breaking objects, with the same shards for everyone
- trash picked up with the trash can, trash bags, merging bags, loading bags into the van

Leaving the crime scene, the job report and money are **not** synced: each player still ends the job on their own.

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
   - Steam callbacks are not used: the game keeps calling `SteamAPI_RunCallbacks` itself. Sessions are accepted up front for every SteamID listed in `BepInEx/coop.cfg`: `peers=` on the host, `host=` on clients.
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

## QA

- **Game-free core tests:** `dotnet run -c Release` in the core's test project. 92 checks, ALL PASS (2026-10-24). They cover codecs and bad packets, pose and item convergence at 30% loss, hold exclusivity, late join and leave. For stains, bodies, quests and world state each, they also cover the merge rules, lossy 4-peer runs with a late joiner that must end identical, and a mutant without the repair cycle that must diverge.
- **Live suites, pass/fail** (2026-10-24). A script starts two real game copies on one PC over loopback. Bots drive the game's own functions, then each copy prints a state line and the script checks both. Each suite also runs as a mutant, with the client ignoring that feature's messages, and must then fail.

  | Suite | What the bots do | Result | Mutant |
  |---|---|---|---|
  | `stain_qa.ps1` | both mop with the real mop, bleed, leave blood, mop a scene stain | PASS (twice, identical digest) | FAIL (5) |
  | `body_qa.ps1` | host loads a body into the van; client carries and throws one; grab conflict; host carries it last | PASS | FAIL (4) |
  | `quest_qa.ps1` | quests started and finished on either side, a conflict, a step backwards, the client discovers a room | PASS | FAIL (4) |
  | `world_qa.ps1` | doors opened and closed from both sides; one-side door from front and back; key taken by the host, key door opened by the client; bottles broken by impact on each side; plank broken; trash can on both sides (the client picks up a shard of the host's bottle); bags merged and loaded into the van | PASS (20 checks; shards within 0.01 m) | FAIL (14) |

  After each new feature the earlier suites were run again and passed.
- **Earlier bot runs read by hand:** movement, ghost screenshot (a solid cyan capsule), item throws (0 corrections during a throw, copies within 1 cm), host quit and host kill.
- **Real Steam, two accounts** (owner and a friend, reported by the owner, 2026-10-23): capsules visible; items synced, and throws land in the same place and look smooth; capsule removed on quit, and held items free to take; landing jump gone after the slide fix. Stains, bodies, quests and world state have **not** been tried over Steam yet.

## How to play

There is no release yet. The test kit is BepInEx be.791 plus the two plugin DLLs plus a `coop.cfg`, unzipped into the game folder. Each player sets `transport=steam`. The host sets `role=host` and `peers=<friends' SteamID64s>`; each friend sets `role=client`, `id=1..3` and `host=<host SteamID64>`. A player's SteamID64 is printed to `BepInEx/LogOutput.log` on start. Everyone then starts the same job.

## Known issues

- Leaving the crime scene, the job report, money and saves are each player's own.
- Not tried over Steam yet: stains, bodies, quests, doors, keys, breakables, trash and bags (only two copies on one PC).
- Only 2 players tested; 3 and 4 are designed for but have never been run.
- Not exercised:
  - other cleaning tools (sponge, power washer, detergent)
  - the dealer's body (not grabbable at job start)
  - a real bag-on-bag merge (needs a perk; the test triggered the merge directly)
  - the game's own key pickup screen (the bot puts the key in the holder)
  - lockpicked and card-reader doors
  - dialogue state
- Players see each other as plain capsules, with no character model or animation.
- No lobby or invite: SteamID64s are typed into a config file by hand.
- The client trusts its own claim of who sent a packet. That is fine between friends, but there is no protection against a cheating player.
- Every player must load the same job from a save in the same state: scene items, stains, bodies and doors are matched by their place in the scene.

## Next steps

1. **Steam test** of everything added since the first sessions, with the friend.
2. **Ending the job together:** "Leave Crime Scene", the report, money and saves.
3. **Lobbies and invites** instead of hand-typed SteamIDs. Then a real 3- and 4-player test.
4. **Publish the source** in its own repository and link it here.
