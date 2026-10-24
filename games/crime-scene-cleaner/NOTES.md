# Crime Scene Cleaner - notes

## Identity (observed, read-only)
- Title: Crime Scene Cleaner (President Studio, PlayWay publisher), Steam appid 1040200
- Steam buildid: 26046462 (appmanifest, LastUpdated epoch 1792525833)
- CrimeCleaner.exe SHA-256: e9167a329f7c8b1c9eef4d0a8dee6d6b08acdee0ce265f1e78ca474f4d560173
- GameAssembly.dll SHA-256: 8af483d77daca17387f4875638f180072195e0f0c8541e9f1e7e8ae999fa3847
- Engine: Unity 2021.3.45f2, IL2CPP (GameAssembly.dll + il2cpp_data/Metadata/global-metadata.dat), 64-bit Windows
- Plugins seen: Rewired, Wwise (AkSoundEngine), Burst, steam_api64.dll (no obvious netcode lib), Easy Save 3 (.es3 saves)
- OS: Windows 10 Pro 10.0.19045

## Observations
- No Mono Managed/ folder: scripts are native IL2CPP, need Il2CppDumper/Cpp2IL-style metadata dump.
- Web search (standard, 1 query) found no co-op mod. Only a Crowd Control (BepInEx) streamer mod, which shows BepInEx IL2CPP runs on the Steam build. Not yet verified by me.

## Unknown
- Any existing co-op attempt (need to search Discord/Nexus/Workshop/GitHub per research.md)
- Whether game has any shipped multiplayer code (expect none)
- Anti-cheat / online service: none seen in install listing

## Next
- Phase 1 research, then Phase 2 workspace (needs owner OK: copy of 53 GB install, tool downloads)

## Workspace log
- Test copy: testbed/game (robocopy from the Steam install; git-ignored territory, never commit).
- Cpp2IL 2022.1.0-pre-release.21 Windows exe: SHA-256 663fb432...e4e04c3 matches the value published by GitHub for the release asset.
- BepInEx 6.0.0-pre.2 IL2CPP win-x64 zip: SHA-256 616ec7eb06cf11b2a0000e8fcef04d1b12bb58e84a2e0bdac9523234fc193ceb. GitHub publishes no digest, so this is hash-on-first-download, not verified against an upstream value.
- Tools in tools_dl/ (not installed anywhere else).

## Phase 3 findings (observed)
- Test copy verified: exe and GameAssembly.dll hashes identical to the Steam install; 249 entries in CrimeCleaner_Data on both sides.
- Cpp2IL (dummydll output) works: IL2CPP metadata v31, Unity 2021.3.45f2, 127383 methods mapped. Dump in dump/cpp2il (not committed).
- Libraries: Steamworks.NET (com.rlabrecque.steamworks.net.dll), Rewired, Wwise, DOTween, Easy Save 3, PixelCrushers Dialogue System, Odin, Cinemachine, UniRx, Addressables, Unity Localization.
- NO networking library (no Mirror/Photon/Netcode/UNet types in metadata). Networking must be built from scratch.
- Game logic assemblies: MainGame.dll (quests, cleaning targets, ozonator, lockpicking, mission report, dialogue events) and Assembly-CSharp.dll (FirstPersonController, GrabController, GrabableItemController, PlayerInventory, Grabber, BodyRagdollController, GarbageCollector, Missions/MissionController, SaveSystem/PlayerSave...).
- Saves live inside the install (CrimeCleaner_Data/Saves, SavesBackups0), so a copy isolates them. The copy contains the owner's save folder (named by Steam id): local only, never commit.
- Baseline SHA-256 of the real save files taken before any launch: local/real_saves_baseline.json.

## Unknown
- Whether BepInEx 6 pre.2 loads on this build (not launched yet).
- Where Player.log / registry PlayerPrefs go (Unity default: AppData\LocalLow\<company>\<product>, HKCU).

## First launch results (test copy, observed)
- BepInEx 6.0.0-pre.2 zip (reports itself as be.697) FAILS: Cpp2IL inside it supports IL2CPP metadata 23-29, this game has 31. The game process exited at once.
- BepInEx 6.0.0-be.791+b782da4 (builds.bepinex.dev, SHA-256 bb1c5109...6f9a, no upstream hash published) WORKS: interop assemblies generated (11606 methods unstripped, 1037 failed, IL bodies 6267 ok / 1170 failed), chainloader started, game stayed alive 90 s. 0 plugins loaded (none written yet). Not yet checked: that the game reached the menu and ran correctly with the hook.
- Launch recipe: env SteamAppId/SteamGameId=1040200, args -screen-fullscreen 0 -screen-width 800 -screen-height 600 -logFile local/player.log, run from testbed/game.
- Real-save check after the run: 34 of 35 baseline files identical. The one changed file is Steam's own <Steam dir>\userdata\<account>\config\librarycache\1040200.json (Steam client library cache, rewritten when a game starts), not a game save. Save folder CrimeCleaner_Data/Saves of the real install was unchanged.

## Probe plugin (observed)
- src/CoopMod (BepInEx IL2CPP plugin, net6.0, builds with `dotnet build -c Release`, deploys to testbed/game/BepInEx/plugins). Loads on be.791; reflection over interop types works.
- Game reached SplashScreen -> MenuScreen_Nightmare (main menu scene) with the plugin running; probe logs every 5 s. No player or grabbables at the menu (expected).
- Public API of key types dumped to dump/api-dump.txt (CleaningTarget, BloodTarget, BodyRagdollController, FirstPersonController, GrabableItemController, GrabController, PlayerInventory, MissionController, Missions, SaveSystem, PlayerSave, Ozonator...).
- Missions.AcceptMission(int) exists: candidate way for a bot to start a mission without menu clicks. Unverified.

## Next (unverified plan)
1. Bot: load a mission via the game's own API, log player position and grabbable counts in a mission scene.
2. Hook points for sync: FirstPersonController (player pose), GrabController/GrabableItemController (items), CleaningTarget/BloodTarget (stains), BodyRagdollController (bodies), MissionController/quest events.
3. Transport: Steamworks.NET already in the game (Steam relay P2P), host authority, up to 4 players.

## Bot reaches a mission (observed)
- Saves are RELATIVE TO THE INSTALL (CrimeCleaner_Data/Saves/<steamid>/Slot1.es3, ES3), not %LocalLow%. persistentDataPath is AppData/LocalLow/President Studio/CrimeCleaner, which holds only Player.log. So the test copy is fully isolated. Copy-side save backup kept in local/copy_saves_backup.
- Bot (src/CoopMod, active only if BepInEx/bot.flag exists): after 40 s on the menu scene calls SaveSystem.Instance.StartGame(1). Result: LoadingScreenScene -> HubResidence-GameIntro (18 grabbables) -> "Dealer's Apartment" mission scene with FirstPersonController at (6.70, 0.04, 28.99) and 554 GrabableItemController objects. Whole run needs ~2 min.
- Real-install save hashes re-checked after these runs (see below).

## Network core, game-free (ran; no game launched)
- src/CoopCore: wire format (pose packet, 26 bytes, sequence numbers, newest wins), RemoteWorld, ITransport, loopback UdpTransport (host relays between clients).
- src/CoopCore.Tests: 1 host + 3 clients on loopback with 30% receive loss, 100 steps of movement plus 40 resend ticks. Result: ALL PASS (15 checks: codec, garbage rejected, stale pose ignored, all 12 peer-to-player final poses converge). This tests the sync core only; nothing is proven yet inside the real game, over Steam, or with items/stains/bodies.
- Not yet done: Plugin integration (read FirstPersonController pose, draw ghost avatars), Steam relay ITransport, item/stain/body sync.

## Two game instances talking (observed, localhost)
- testbed/game2 = hard-linked copy (2410 files linked, 269 real copies: BepInEx, Saves, PlayerPrefs, KeyBindings, doorstop ini). A second process CAN run alongside the first on the same linked files; no mutex/single-instance problem seen. Both used SteamAppId=1040200.
- coop.cfg (BepInEx/coop.cfg) sets role=host|client, id, port. Plugin sends the local FirstPersonController position at 20 Hz over CoopCore UDP; host relays; remote players get a collider-less capsule "ghost".
- Result after ~150 s: both instances reached "Dealer's Apartment" via the bot, and each logged the other's pose (host rx 682 packets, client rx 1128, sequence numbers rising, remote position equal to the other's player position).
- LIMITS: both players stood still at the same spawn point, so this proves the packet path in both directions, not movement tracking. Ghost capsule rendering was not checked on screen. No world sync (items, stains, bodies, quests) yet. Localhost only; Steam relay not written.

## Walking test (observed, localhost, 2 instances)
- Host bot (BepInEx/walk.flag) teleports its player on a circle 3 m in front of player 1 (CharacterController disabled around the write). Client log over ~6 samples: remote position changed every sample (x 7.14 -> 6.87 -> 6.64 -> 6.38 -> 6.20 -> 6.19, z 30.98 -> 31.47). So movement of a remote player IS tracked, on the client side numbers.
- Screenshot from the client (shot.flag, ScreenCapture) shows the mission INTRO CUTSCENE (player sitting in a car), not free movement. So the ghost capsule was NOT seen on screen: the client's camera is a cutscene camera and its player position stays at the car/spawn point. Ghost visibility remains unverified.
- Both instances stay in this intro until it ends; the bot needs to skip/finish the intro (or wait for gameplay control) before ghost and item tests mean anything. Unknown how to detect "intro over" (FirstPersonController.CanMove is a candidate).

## Step results: intro, items, Steam (observed unless marked)
### Walk/ghost
- CanMove turned True after the intro; with the walk bot gated on it, the client log shows the host's ghost at viewport (0.25, 0.19, z 2.4) with Renderer.isVisible = True. A screenshot (local/shot_client_walk.png) was taken but the camera faced a fence/wall and the ghost was not in that frame, so "ghost rendered" rests on the isVisible/viewport log, not on a picture.
### Items (src/CoopMod/ItemSync.cs, src/CoopCore/Items.cs)
- Game-free: item codec, host-stamped versions, holder exclusivity, 50 items converging on 4 replicas at 30% loss via snapshot resend, late-join snapshot: all PASS (ran).
- Live, 2 instances, localhost: item tables identical (n=554, hash 5962476C on both). Host moved #100 (BoxBigOpen) +2 m; both sides then show it at the same place after it fell (-4.14, 0.22, 24.79). Client grabbed #200 (Crate 23) with GrabController.GrabItem, carried it to its player; host saw it at (6.60, 1.75, 29.98) = client value while held; after release the host simulated the fall and both ended at (6.60, 0.22, 29.98).
- Limits: one host item and one client item tested; the carry was a teleport of the held item, not a human hand movement; no conflict test (two players grabbing the same item) in the live game; client owner id is taken from the packet (spoofable, fine for friends); throwing, stacking, containers, garbage bags, bodies, stains, doors, quest state NOT synced.
### Steam transport (src/CoopMod/SteamTransport.cs)
- Uses the steam_api64.dll flat API already initialised by the game (real app id 1040200, so every player owns the game; no Spacewar needed). No callbacks (game's own RunCallbacks keeps running); sessions are accepted up front for every SteamID in coop.cfg (peers=... on host, host=... on client).
- Single-account smoke test inside the real game (ran): interface getters resolved, own SteamID read, SendMessageToUser to self returned EResult 1, the message came back through ReceiveMessagesOnChannel with the right length (3). This confirms calling convention, send flags and message layout (data @0, size @8).
- NOT proven: delivery between two real accounts, whether SteamID decoded from the identity at msg+16 is right (not logged), the accept-up-front assumption (playbook says the first message from a stranger raises callback 1251; if messages are held, the fix is to poll callbacks), lobbies/invites (none written: players exchange SteamID64 by hand), packet size limits (reads cap at 4096).
- Scripts: scripts/run_two.ps1 launches host+client copies and prints logs.

## Two-account Steam test kit (prepared, NOT yet run)
- package/CscCoop-friend.zip (35.5 MB, SHA-256 727a9f2a...091a, built by scripts/make_package.py from package/stage): BepInEx 6 be.791 + CscCoop.dll + CoopCore.dll + coop.cfg (client, id 1, host SteamID64 filled in) + README-FRIEND.txt. No game files. Contains the host's SteamID64 in coop.cfg: do not commit or publish.
- scripts/launch_host.ps1 starts the host test copy (testbed/game, steam transport, no bots).
- Expected evidence: both logs show "[net] steam transport ready", host log gets rx counts from the friend's SteamID, ghost visible, items move. If the friend's packets never arrive, suspect the unaccepted-session case (callback 1251).

## Two-account Steam test (owner + friend, reported by owner, not observed by me)
- Steam transport worked between two real accounts: remote player position updated (seen as a moving shadow) and grabbed items synced.
- Bug 1: ghost capsule invisible, only its shadow drawn. Cause (observed): CreatePrimitive uses the built-in 'Standard' shader; Unity reports it supported=True but this URP game draws only its shadow pass. Fix: material from Shader.Find("Universal Render Pipeline/Unlit") (found in the build), cyan _BaseColor; fallback copies a URP material from the scene. Verified on localhost: screenshot local/shot_ghost_urp.png shows a solid cyan capsule in the client's view.
- Bug 2: a thrown item just dropped on the other PC. Cause: release packet had no velocity and OnPacket zeroed velocity; release was also sent once, unreliably. Fix: ItemState carries linear + angular velocity (61-byte packet); clients send a proposal counter (clock-seeded) in Version and resend the release 4 times; the host drops duplicate/late proposals per sender. Game-free tests: velocity roundtrip, resent release ignored, late held packet ignored, host simulates flight: all PASS.
- Live localhost throw: client threw Crate (23) through GrabController.TryRelease(force); release sent v=(-0.62, 2.84, 5.97); host and client then logged the same trajectory sample by sample (first sample (6.59, 0.22, 30.65), bounce back to (6.91, 0.21, 30.36) on both). Previous build dropped it straight down at (6.60, 0.22, 29.98). The crate hit something about 0.7 m ahead (fence near spawn), so a long free flight was not observed.
- Still not verified: these two fixes over real Steam between two accounts.

## Second Steam session (reported by owner)
- Cyan capsules visible on both PCs; thrown items land in the same place on both.
- Bug 3: client sees a little stutter when IT throws. Bug 4: ghost stays after the host disconnects.

## Fixes (observed on localhost, two copies; not yet re-tested over Steam)
- Leaving: new Bye message (sent 3x from OnApplicationQuit, relayed by host) plus a 5 s silence timeout. On leave: ghost destroyed, RemoteWorld.Forget (a restarted player's seq 1 is accepted again), host frees every item the leaver held (ItemAuthority.ReleaseAll, broadcast). Live: normal host quit -> client logged "player 0 left (bye)", ghost gone; host process killed -> client logged "player 0 left (timeout)", ghost gone. Freed-items path covered only by game-free tests.
- Throw stutter. Measured cause (per-packet trace on the client, one throw): the host's copy of a client-thrown crate trails the client's by about a round trip, 0.05-0.17 m during flight; the client applied each of those 18 updates in 2.1 s, snapping its crate back each time. Fix: for 5 s after a local throw the client ignores host updates while either copy still moves (> 0.2 m/s); updates skipped this way are not recorded, so the host's final resting state (resent by the snapshot) still applies; updates within 5 cm / 3 degrees are not applied at all.
- Live after the fix: two real throws (release v ~ (-0.21, 5.4, 1.99)), 0 host snaps applied during either flight, final positions equal within 1 cm on both copies.
- Measurement mistakes found and fixed on the way (so earlier numbers in this session are invalid): the snap counter ran before the grace check; the walk bot kicked landed crates around; the bot's "carry" put crates on the car roof so some throws never launched (now carried to camera + 1.2 m).
- Game-free tests now 34 checks, ALL PASS.

## Third Steam session (reported by owner)
- Friend's own throws look smooth. Freed items after a player quits: owner could pick them up (ReleaseAll works over Steam). Capsule disappears on quit.
- Bug 5: on the client, a thrown item sometimes jumps a short distance after it stops. Cause: two PCs' physics do not land the item in exactly the same spot; after the throw grace period the client snapped to the host's resting state in one frame. Localhost gaps were < 5 cm (below the correction threshold), so local tests never showed it.
- Fix: when both copies are at rest and differ by 5 cm - 1.5 m, the client slides the item to the host's spot (smoothstep, 0.2-0.6 s, kinematic while sliding; a grab or newer state cancels the slide). Over 1.5 m it still snaps.
- Live localhost check with a forced gap (bot pushed the client's copy 0.4 m sideways mid-flight): "blend #201 done: 0.37 m over 0.29 s, 19 frames, largest step 3.1 cm"; final positions identical on both. Five other items that settled 5-11 cm differently at scene load also slid instead of jumping (largest step 2.4 cm).

## Stains: probe results (observed, single copy, src/CoopMod/StainProbe.cs, stainprobe.flag)
- "Dealer's Apartment" starts with 371 Taint objects, all Intensity 1.0, each with a CleanableDecal component; names like "ForwardBlood3 Variant", "BloodDecalHand Variant", parents "Apartment_Blood", "MaterialSheet", "Canvas".
- Real mop: InventoryController.Instance.Equip(ItemCategory.mop, 0) puts "WeakMop(Clone)" (CleaningItem) in hand ~0.5 s later (PlayerInventory.EquipCurrentItemInCategory did nothing visible). CleaningItem.StartCleaning() -> CleanOrTaint() each interval. FirstPersonController.LookAt does not stick (mouse look overrides), so the bot calibrates where the mop lands instead of aiming.
- New blood during play: PlayerBloodSpawner.SpawnBlood(type) -> BloodSpreaderController.ProjectBlood(type, target, point, normal, randomRotation) -> ProjectBlood(prefab, type, targetGO, point, normal, forward, randomRotation). Seen targets: "Cornice (47)", "Floor5x5 (49)", "TrashbinBlue (15)". Creates "StraightBlood3(Clone)".
- Cleaning a spawned stain: no mask painting; TaintableBase.Taint(CleaningData) on the mop, then Taint.SetIntensity on the stain 0.31 -> 0.12 -> 0.05, then BloodSpreaderController.RegisterDeleteDecal(taint) and the object is destroyed.
- Cleaning near starting floor stains deleted two of them ("BloodDecalStain05 Variant", "StraightBlood1 Variant") through RegisterDeleteDecal; intensity path presumably the same (SetIntensity log was capped at 3 calls).
- Masked path: CleanableDecal.PaintTextureImmediate(worldHit, size, CleaningData, ITaintable) -> PaintTexture(strength 6.25, size 1823.99, uv, effectiveness 0.072, exponent 1). Seen only on "BloodDecalHand Variant" with uv outside [0,1]; 10 s of mopping left its Intensity 1.00 and coverage 100. When this path actually removes blood is unknown.
- Footprints: FootprintSpawner "BloodyFootstepsSpawner" exists on the player; calling UpdateBloodiness(1) + LeaveFootprint made no new Taint (its intensity went back to 0). How real footprints are created: unknown (needs real walking).

## Stain sync design (derived)
- State-based, not call-based: each peer scans Taint objects every 0.25 s; new stain / intensity change / deletion become records. Catches creation paths not mapped yet (footprints, drips, bodies).
- Record = LWW register: (version, writer) orders updates; deletion is a tombstone. Ids: starting stains = index in a table sorted by name+position (hash compared like items); spawned = (creator player, counter).
- Creation replays the creator's ProjectBlood arguments (captured by a hook; fallback derives them from the transform), then sets the exact transform.
- Duplicate splats from physics simulated on two PCs: an incoming create that matches a local, unsent new stain of the same type within 15 cm is merged (alias), not spawned.
- Repair/late join: each peer resends its own recent records for a few seconds; the host round-robins all records continuously.

## Stain sync: implementation and results (observed, localhost, 2 copies)
- Core (src/CoopCore/Stains.cs): StainRecord (LWW register, tombstone), StainCodec (variable length, create info with ProjectBlood args + transform + target path or item index), StainRegistry (merge, alias, PickDuplicate), StainSync (send now + 2 resends, every peer cycles its own records, host cycles all). Game-free tests: codec, LWW/tombstones, alias, duplicate matching, two 4-peer star runs (30% loss, 0-5 tick delay, 10% duplicates, one late joiner) converge; mutant with the repair cycle off diverges. Core suite now 49 checks, ALL PASS.
- Game side (src/CoopMod/StainLink.cs): scan of Taint objects every 0.25 s; remote apply via BloodSpreaderController.ProjectBlood (6-arg), Taint.SetIntensity, Taint.DestroyDecal. Destroyed = Unity-null only (an inactive, culled stain is not "cleaned").
- Starting-stain identity: first attempt sorted by name + position; tables hashed differently on the two copies because stains parented to furniture (coffee table, couch pillows) settle 0.1-0.4 mm apart. Fixed by sorting on the hierarchy path "Name[rank]/..." (rank among same-named siblings). Tables then matched every run (hash 2CD78E63, n=371).
- Probe correction: the mop casts a HORIZONTAL ray from eye height (TaintCleaner.CastClean origin/direction, dir ~(-0.10, 0.00, 0.99)), so the bot can only mop walls at eye level; reach about 2 m. PaintTextureImmediate only runs when a decal is under the mop, so it cannot be used to calibrate on clean floor.
- Cleaning these starting stains is scalar: CleanableDecal.coveragePercentage stayed equal on both copies and unrelated to intensity (cov 100 with intensity 0.257); no mask strokes need syncing for the mop in this job (other tools not checked).
- Bug found by the suite: changes under the 0.02 send threshold were never sent (light touches left 0.988 vs 1.000; final steps of mopping 0.109 vs 0.125). Fix: a stain that has not changed for 0.6 s sends its exact value. After the fix the 31 touched stains were identical to 3 decimals.
- Footprints: a real FootprintDecal(Clone) (type footprint) appeared when the bot was moved through blood; the scan caught it and the other copy built it, without footprint-specific code.
- Report: CleaningAreaEvent.CurrentCount stayed 0.0 on both copies in this phase (CurrentMax 155.2 Dealers Apartment, 38.2 Attic), so it is not exercised. The game's own BloodSpreaderController.StartingTaintsByArea / NewTaintsByArea (Dictionary<string, List<Taint>>) are compared instead: identical on both copies, list lengths included (so remote DestroyDecal updates the game's own lists). The end-of-job report screen itself was not opened.

## Live stain suite (scripts/stain_qa.ps1, pass/fail)
- Host bot, then client bot: real mop (InventoryController.Equip(mop,0) + StartCleaning), blood via ProjectBlood where the mop ray lands, mopped away; second blood left; a wall starting stain the mop ray provably hits is mopped 10 s. Client also bleeds (PlayerBloodSpawner.SpawnBlood).
- Checks: both QA lines present; host blood built on client; client blood built on host; cleaning progress applied on the other copy; a stain deleted on one copy disappears on the other; dynamic stains left and equal in number; same stain count; scene digest (stain id + intensity in 0.01 steps, dynamic stains by position); game's room counters and per-room stain lists equal and non-empty.
- Results 2026-10-24: RESULT: PASS twice in a row, identical final digest 591C28313548C77B (372 stains, 3 made during play; host 88 cleaning changes, client 227; 3+1 deletions). Mutant (-Mutant: client ignores stain records): RESULT: FAIL (5): host blood not on client, dynamic count, stain count, digest, room lists.
- Earlier run of the same suite PASSED while the host bot left no lasting change (it mopped away its own blood): the digest would have matched even with a deaf client. Caught by the first mutant run; fixed by making each bot leave blood and mop a reachable starting stain.
- Not covered: Steam transport for stains (only loopback), 3-4 players, other tools (sponge, power washer, detergent), bodies dripping blood, stains on carried items (code path exists, not exercised), the job report screen.

## Bodies: probe (observed, single copy, src/CoopMod/BodyProbe.cs, bodyprobe.flag)
- "Dealer's Apartment" has 3 BodyRagdollController: "Girl02 Anim" (33 RbToBones, loose ragdoll), "Girl01 Ragdoll" (11 bones, frozen kinematic, grabbable), "DrugDealerAnim Variant" (11 bones, frozen, IsGrabbable False at job start). Each has a BodyQuest: "Dispose of the victims body" 0/1 and 0/2, "Dispose of dealer's body" 0/1, and 2 GarbageItem components in children.
- Do NOT hook BodyRagdollController.KYS: it runs ~40,000 times a second (per bone per frame); a prefix on it flooded the log with NullReferenceExceptions.
- Grab: GrabController.BodyGrabber.GrabBody(bodyPart, GrabController.animator) -> EnableGrab(true) -> GigaUnfreeze -> Unfreeze. While carried the root rigidbody is kinematic and rides about 1.6 m above the player's feet.
- Throw: GrabController.TryReleaseBody(force) -> BodyGrabber.ThrowBody coroutine -> EnableGrab(false), GigaUnfreeze, Unfreeze; IsGrabbed stays true for a moment after the call, then the ragdoll is dynamic.
- Disposal: van trunk "TrunkTrunkController" (trigger centre about (9.05, 1.18, 27.38), size 2.25 x 0.60 x 2.69). Release near it -> GarbageItem.OnAddedToTruck -> TrunkController.TryAddItemToTruck -> BumpObjects / TryShowProp / ShowProp (body bag prop) -> GarbageItem.CollectAndDestroy -> BodyQuest.CollectBody. The body GameObject is then inactive, not destroyed.

## Body sync (derived design, then observed results)
- Core (src/CoopCore/Bodies.cs): BodyRecord (per-bone world pose, rotation as 4 x int16), BodyAuthority on the host (grab only if nobody else holds it; poses/release/rest only from the simulating machine; rest hands simulation back to the host; collected is final, refused only while someone else holds the body; ReleaseAll on leave; per-client proposal counters drop duplicates and late packets), BodyReplica. 33-bone packet 674 bytes. Game-free tests incl. grab races in both orders and two lossy 4-machine runs; mutant without the host's resend diverges.
- Game side (src/CoopMod/BodyLink.cs): bodies identified by hierarchy path (hash 09FFEC60, bones 11,11,33 on both copies). Simulating machine sends poses at 10 Hz; followers SetKinematic(true) and glide (lerp 15/s, snap beyond 2 m). Thrower simulates until all bones < 0.15 m/s for 1 s (or 8 s), then Rest. TrunkController.OnTriggerEnter is skipped for bodies this machine does not simulate; collection is replayed with TrunkController.TryShowProp + GarbageItem.CollectAndDestroy, so the game's own BodyQuest runs on every machine once.
- Live suite scripts/body_qa.ps1 (first pass): host carried Girl01 to the van (collected, quest flag set on host AND client: wasCollected True on both); client carried and threw Girl02, host took it over at rest; host tried to grab Girl02 while the client held it and lost (let go); no body held on both; non-collected bodies identical to 0.01 m. RESULT: PASS. Mutant (client ignores body records): FAIL (3). Gap found: Girl02's end position matched even with a deaf client because the client moved it last; added a final host carry and a check for it.
- Bugs found on the way: decimal-comma number format broke the runner's parsing (now invariant culture); the host re-detected a just-lost grab 6 times before the game finished letting go (1.5 s cooldown).
- Re-run with the final host carry (2026-10-24): RESULT: PASS (bodies identical to 0.01 m). Mutant: RESULT: FAIL (4): Girl01 not collected on the client, no collection travelled, poses one way only, Girl02 1.78 m apart. Both directions are now exercised.
- Not covered: Steam transport for bodies, 3-4 players, the dealer's body (not grabbable at job start; what unlocks it is unknown), bodies dragged by a limb vs carried (only GrabBody/TryReleaseBody used), body-bag items, blood dripping from carried bodies (stain sync should catch it, not checked), a player leaving while holding a body (only covered by the core test).

## Quests: probe (observed, single copy, src/CoopMod/QuestProbe.cs, questprobe.flag)
- Quests are PixelCrushers Dialogue System quests (DialogueSystem.dll): QuestLog.GetAllQuests/GetQuestState/SetQuestState/GetQuestEntryState/SetQuestEntryState, DialogueLua.GetAllVariables/GetVariable(name).asString. The game's DialogueSystemQuestController wraps them (SetQuestEntry, SetQuestEntryPercentage, MarkForUpdate, Save/Load).
- In "Dealer's Apartment" the job adds 14 quests (54 keys with entries and variables), e.g. "Clean blood in apartment", "Clean blood in the attic", "Clean trash in apartment/attic", "Collect evidence in apartment/the attic", "Collect Tylers Payment", "Dispose of dealer's body", "Dispose of the victims body", "Find a way into apartment", "Fix furniture in the apartment", "InvestigateAttic", "Leave Crime Scene", "Collect big trash".
- Variables: per-room counters (Apartment.BloodMin/Max, Clean_blood_in_apartment.Current/.Max = 155.22, ...BodiesMin/BodiesMax, VictimsDisposed), location flags (Courtyard.Discovered, Dealers_Attic.Discovered, set by DialogueSystemQuestController.SetLocationDiscovered when the player enters), and PlayerCurrentLocation (the player's own room, 1 courtyard, 6 apartment, 7 attic).
- After one body went into the van, Dispose_of_the_victims_body.BodiesMin went 0 -> 2 and the quest -> Success (max is 2). Girl01 has 2 GarbageItem components; whether a single body really counts twice, and what the client's replay (one CollectAndDestroy) counts, is unknown.
- Probe pitfall: a generic Harmony prefix (object[] __args) on MissionController.SendNewMails made the game's calls to it throw NullReferenceException inside the patched copy. Not hooked any more.

## Quest sync (design derived, results observed, localhost 2 copies)
- Shared: quest states (Q:), quest entry states (E:), variables that record events. Not shared: PlayerCurrentLocation; counters ending .Current/.Max/.Min/Min/Max/Disposed (every machine recomputes them each frame from its own world, which stain/body/item sync keep equal; sending them would make two machines overwrite each other forever).
- Merge (src/CoopCore/Quests.cs): quest states only move forward (Unassigned < Grantable < Active < ReturnToNPC < Success/Failure/Abandoned/Done; same rank -> newest version), flags only turn on, other values newest-wins. A local step backwards is not sent; the shared state is written back. Game-free: 10 checks incl. two lossy 4-peer runs with a late joiner, mutant without repair diverges. Core total 80 checks, ALL PASS.
- Game side (src/CoopMod/QuestLink.cs): scan every 0.5 s; write via QuestLog.SetQuestState(name, QuestState) / SetQuestEntryState(name, n, QuestState); variables via Lua.Run("Variable[\"<DialogueLua.StringToTableIndex(name)>\"] = value") (Il2Cpp boxed values are read-only, so DialogueLua.SetVariable(name, object) could not be fed); then DialogueSystemQuestController.MarkForUpdate().
- Bug found by the first live run: writing the state as text ("Success", the enum name) did nothing (the Dialogue System's strings are lowercase) and the read-back was silently stored as "seen". Fixed with the typed overload and a warning when the read-back differs.
- Live suite scripts/quest_qa.ps1: host starts InvestigateAttic; client walks into the attic (game sets Dealers_Attic.Discovered) and finishes "Find a way into apartment"; both finish "Collect big trash" differently (host Failure, client Success); client sets InvestigateAttic back to Unassigned. RESULT: PASS (both Active / Success / Success, attic discovered on the host from the client, PlayerCurrentLocation 1 vs 7 and never sent, digests equal, client restored 1). Mutant (client ignores quest records): FAIL (4).
- Not covered: Steam, 3-4 players, quests changed by real game events other than location discovery (evidence, payment, furniture), dialogue/conversation state (SimStatus), "Leave Crime Scene" and the end of the job (each player still leaves and gets the report on their own), mission money and saves.
- Regression after adding quest sync (2026-10-24): stain_qa.ps1 RESULT: PASS, body_qa.ps1 RESULT: PASS.

## Breakables, trash, doors: probe (observed, single copy, src/CoopMod/WorldProbe.cs, worldprobe.flag / worldprobe2.flag)
- Method bodies: Cpp2IL 2022.1 `--output-as isil` (same pinned exe) writes readable call lists per method (local/isil, not committed). Field names are not resolved there; behaviour below was confirmed in game.
- "Dealer's Apartment": 61 DestroyableItem (bottles, balloons; each is a scene item), 34 DestroyableObject (planks, paintings; 89 CrackedObject parts), 133 CollectableItem (trash), 14 ShortcutDoors, 4 DoorKey (DA_Keyes: Dealers_Apartment_Key, ... Spare, Secret_Room_Key_1/2), no plain Door, no bags at start.
- Breaking an item: DestroyableItem.OnCollisionEnter calls CrackItem itself (tools go through CheckForDamage/TryCrackItem from MopePunch and PowerWasherLance). CrackItem instantiates the item's destroyedModel (CrackedItem prefab); its shards (4 for a beer bottle) are new GrabableItemController + CollectableItem, listed in DestroyableItem.CreatedCracks in prefab order; the original item is switched off.
- Breaking an object part: CrackedObject.CrackItem(direction) also cracks its siblings; the parts become root objects with Rigidbody, GrabableItemController and CollectableItem.
- Trash can = ItemCategory.trash (GarbageCollector(Clone) under the player's left hand; capacity 36). TryToCollectItem -> GrabItem -> CollectableItem.Collect (GarbageManager.RemoveCollectableItem) -> item switched off (not destroyed); GarbageCollector.Collect adds the fill. SpawnBag makes "Trashbag(Clone)" (GarbageBagConnector + GarbageItem, trashValue = fill) in a delayed DOTween callback.
- Bag merging: GarbageBagConnector.OnCollisionEnter with another bag; the faster one grows (IncreaseBagSize) and the other DestroyBag()s, limited by perk values (PerksController.GetUnlockedPerkValues). Two size-1 bags dropped on each other did not merge on this save. Loading a bag in the van: GarbageItem.CollectAndDestroy switches it off.
- Doors: ShortcutDoors.requirement AnySide / FrontSide / BackSide / RequireKey / Locked. get_IsLocked returns the "not opened yet" flag; OnOpen = event + UnlockDoors() (clears it, OnUnlock event, optional Lua door variable) + Toggle(). The "Shortcut" door (BackSide) validates only from its back; after one opening it validates from every side.
- Keys: ItemInspector.OnKeyCollected -> KeyHolder.CollectKey (list add). DoorKey.CollectKey only fires its OnCollect event. KeyHolder.Load adds the key and switches the key object off. KeyHolder.UseKey(door) is how RequireKey doors validate. Each machine has its own KeyHolder on its player.

## World sync: doors, keys, breakables, trash, bags (design derived, results observed, localhost 2 copies)
- Core (src/CoopCore/World.cs): world state is key/value records on the quest record sync under message type 8 (quest and world records never mix). D:<door>.open newest-wins; D:<door>.unlocked, K:<key>, B:<item>, P:<object>#<part>, C:<item> flags; S:/L:/X: bag spawn, size, merged; G:<item> loaded in the van. Ids for objects made during play, the same on every machine with no messages: scene items [0,2000), parts 2000+16*object+k, bags 10000+2500*player+n, shards 20000+16*item+k. Game-free: 12 more checks incl. two lossy 4-peer runs with a late joiner and a mutant without repair. Core total 92, ALL PASS.
- Game side (src/CoopMod/WorldLink.cs): state read every 0.25 s, changes sent; replay through the game's functions (UnlockDoors/OpenDoors/CloseDoors; KeyHolder.CollectKey + hide key; DestroyableItem.CrackItem, same prefab so the same shards; CrackedObject.CrackItem; CollectableItem.Collect + hide; Instantiate the can's bag prefab + SetLevel; Destroy for a merged bag; TrunkController.TryShowProp + GarbageItem.CollectAndDestroy for the van). New shards, parts and bags are added to item sync (ItemSync.Register), so their positions sync like any item.
- Who decides: a hit breaks an item, and two bags merge, only on the machine of the player holding or just throwing it (5 s), otherwise on the host (prefixes on DestroyableItem.OnCollisionEnter and GarbageBagConnector.OnCollisionEnter). So the thrower's perks set how far bags merge. Breaking by tools and breaking object parts is allowed everywhere.
- Item sync fix found by the live suite: the host's repeated snapshots carry the same version, so a client copy pushed by a player standing next to it (a shard 0.34 m off) was never corrected. Now a client slides a resting item that drifted back to the host's spot and holds it still (kinematic) until the host moves it or a player picks it up; max 5 tries per item per minute. Without the hold, a tin-can pile that settled differently on the client knocked itself over again ~40 times per can (264 corrections); with it 10.
- Live suite scripts/world_qa.ps1 (2026-10-24): host opens a door, client closes it; client cannot open the one-side door from the front, opens it from the back, then the host opens and closes it from the front; host takes the dealer's key, the client has it, opens the dealer's door with it; host drops a bottle (breaks on impact on the host), client throws one (breaks on impact on the client, not on the host); host breaks a plank; both use the trash can (the client picks up a shard of the host's bottle); host merges the client's bag into its own (direct IncreaseBagSize + DestroyBag) and loads it into the van. RESULT: PASS (20 checks; shards of both bottles at the same spot to 0.01 m, plank piece to 0.00 m, digests equal). Mutant (client ignores world records): RESULT: FAIL (14).
- Not covered: Steam, 3-4 players, a real bag-on-bag merge (needs the perk; the merge was triggered directly), the inspector's own key pickup (the bot calls KeyHolder.CollectKey), lockpicking doors (PickableDoors), card readers, locked windows, ladders, trash spawners, items broken by tools on another machine at the same moment, a player leaving mid-throw.
- Regression after world sync (2026-10-24): stain_qa, body_qa, quest_qa RESULT: PASS; core 92 ALL PASS.
