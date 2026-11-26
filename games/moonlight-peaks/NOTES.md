# Moonlight Peaks: co-op working notes

Raw notes, 2026-11-24. Paths use `<game dir>` / `%USERPROFILE%`.

## Identity (observed)

- Moonlight Peaks, Steam app `2209900`, build `25725760`, Windows x64, installed at `D:\SteamLibrary\steamapps\common\Moonlight Peaks`.
- `<game dir>/Moonlight Peaks.exe` SHA-256 `04BE5CFFF9909AF892968276D085E9C11B93BA82886749516A521250113511DB`, file version 6000.3.24.5143451.
- Unity `6000.3.24f1` (`4e7b9b5b6244`), Mono scripting backend (`Assembly-CSharp.dll`, no `GameAssembly.dll`).
- Ships Steamworks.NET + `SteamManager` initialising the real app id (`2209900`, demo `2584240`).
- Only this build checked. Steam was in offline mode at the start; the game hangs at the splash until `SteamAPI_Init` succeeds. Steam online + `SteamAppId=2209900` env was needed for the test copy.

## Workspace

- Test copy: `local/moonlight-peaks/game` (robocopy of `<game dir>`), BepInEx 5.4.23.2 x64 (SHA-256 `F752CE4E838F4C305B9DA1404B6745F2CFF23B8BFD494F79F0C84D0A01F59B46`).
- Saves are isolated per `--coop-profile=<name>` by a Harmony postfix on `Application.persistentDataPath` to `BepInEx/coop-saves/<name>`. Real saves untouched so far.
- Save layout: `%USERPROFILE%/AppData/LocalLow/Little Chicken Game Company/Moonlight Peaks/<steamid>/Saves/<save-guid>/GameData.json` + `GameMetaData.json`, and `GlobalData.json`. `GameData.json` is plain JSON.

## Observations (game architecture)

- Main gameplay assembly is `Vampire.Runtime.dll` (about 13 MB, readable names, no obfuscation).
- One save-wide `GamePersistence` singleton (`GamePersistence.Instance`) with nested `Player` (`PlayerPersistence`), `NpcRelationships`, `EntityCharacters`, `Rooms`, `Quests`, `Time`, etc. One player record per save.
- Player is `PlayerView` (MonoBehaviourSingleton) wrapping an `EntityCharacter`. `PlayerView` holds the state machine, input, interactor, equipment, energy, mana, animator and customization. NPCs are also `EntityCharacter`.
- Player the world entry point: `Director.Nodes.PreloadCharacterNode` instantiates `AssetLibrary.PlayerView` into `RoomContainers`; room load is via Addressables, room state via `RoomPersistence`.
- `EntityBehaviour<T>` keeps a static registry keyed by entity asset GUID and, on duplicate, destroys the previous component's GameObject. This destroyed the local player's `Character`/`Animator` when a second `PlayerView` was spawned. Fixed for the spawn with a Harmony patch making `allowDuplicateEntityAsset` true while the puppet is instantiated (base + `EntityLocation` override).
- `MonoBehaviourSingleton<PlayerView>.Instance` is a plain static dictionary; the last `Awake` wins. The plugin pins it back to the local player.
- Day advance: `GamePersistence.IncrementDay`, `DayProgresser`, sleep is `EntitySleepCoffinActivity`/`PlayerDirectorState` (not mapped yet).
- Relationships: `NpcRelationshipPersistence` is one record per NPC in the save-wide list, so a second player's relationships need a parallel list plus swapping when running guest code.
- Cutscenes: `Cutscene`, `BeginCutsceneNode`, `CutsceneNpcView`; a cutscene has a cast list and can be staged/spawned by the Director.
- The game has no network layer, no lobby, no `NetworkManager`.

## What works (verified in one client)

- BepInEx 5 loads into Unity 6 Mono; plugin loads; saves isolated under the profile.
- Main menu automation: `AppMenuStateMachine.LoadGame(SerializedGuid, null)` loads the newest save; menu buttons can be clicked through `Button.onClick`.
- `F9` spawns a second full `PlayerView` (prefab `AssetLibrary.PlayerView`) as a ghost; both players' bodies stay alive, no NullReferenceException afterwards.
- `F8` toggles debug overlay; `F6` load save; `F7` click `Continue`.

## Design being built

Host-authoritative. Guest loads a copy of the host's save, keeps its own `PlayerPersistence` and `NpcRelationships`. Guest actions go to the host and are executed there using the guest character; world state goes host→guest. Direct UDP transport first (Steam P2P later).

## Unknown / next

- Whether a second PlayerView can run its own state machine without hijacking player-scoped singletons (`GameInventory`, `GamePersistence.Player`) - impersonation still to test.
- Guest save transfer and load entry point.
- Cutscene cast staging and per-player relationship store.
- Steam transport through the game's own Steamworks session (Steam online in the test copy).

## Final state 2026-11-24

Implemented and smoke-tested on two local clients: save isolation + TCP save transfer, ghost spawn with duplicate-entity protection, 20 Hz position/rotation/speed/state, host room guid sync -> guest GameSwitchRoomState, day/time sync, guest-side fresh PlayerPersistence and empty NpcRelationships. Local player respawns after room unload.

Checked: positions followed in both directions, Room_Exterior_Plot loaded on guest, day 2 -> 3 on guest after host IncrementDay, guest rels 0 vs host 62, installer and uninstaller ran on the test copy.

Not implemented: guest action forwarding (harvest/plant/chests), cutscene staging, sleep transition, equipment visuals, Steam transport.

User-facing package: `projects\moonlight-peaks-coop` (install.ps1, uninstall.ps1, dist with pinned BepInEx zip and plugin).


## Continued work 2026-11-24 (owner away)

Added bidirectional Director node mirroring (GoToNextNode prefix/postfix; guest local graphs are forwarded, host echoes), bidirectional RoomState, and a persisted world push (full GamePersistence over TCP) with a semantic room signature (grid/growable/harvest/water/pickup counts and fields) and a 6 s apply cooldown. Verified: guest startday mirrored 12+ director nodes and advanced both clients to day 3; guest and host warps loaded on the other client; world applied once per change; idle 25 s stayed stable with one apply.

Remaining: held item/equipment visuals, coins shared through GameMeta, cutscene subject should map to the acting player's unique entity, world reload rubber-banding, Steam transport.


## 0.1.2

Guest actors now get a cloned player entity on the host (unique GUID, EntityAsset.IsPlayer patch, registered under its own key) and Director actor ids choose host/guest player vs NPC. Held item GUID is sent and the puppet equipment instantiates it, unverified with real tools. World reload stores and restores local position to reduce rubber-banding. Coins are inventory items and stay per-player. Tested: host and guest startday both advanced to day 4, cutscene graphs ran on both, 16 s stable.

## 0.1.3

Steam launches now share the same default profile as the launcher; existing saves are copied into the isolated profile on first mod launch so the load menu is not empty. Verified on the real install: 0.1.3 loads from a plain exe/Steam launch, profile=default, saves copied, real saves untouched.

## 0.1.4 held item verification 2026-11-24

Held item sync now sends the full ItemEntry (guid, quality, metadata, custom name) and the puppet keeps its own held state. The puppet re-instantiates the held view if it is missing (checked once a second) and retries for up to 0.5 s, so the visual returns after a room reload. New ctl commands: `hold` uses the real `GameInventory.TryGrabItem` path (first non-form tool with a view) and `unhold` uses `TryGrabNone`. Status adds `lheld`, `rheld` and `msg`.

Verified on two local clients: guest hold -> host puppet rheld=ItemView_ToolView_Wand(Clone); guest unhold -> host puppet rheld=no; host hold -> guest puppet rheld=ItemView_ToolView_Axe05(Clone); held visual re-appeared on the other client after two host warps (rheld returned after the room load).

New gap seen: the local player's own held view is gone after the local body is respawned by the room reload (host lheld=no while its persistence still holds the axe, guest lheld=no at first). Puppet visuals are fine.


## 0.1.4 room follow 2026-11-24

Room changes from the host now use a forced GameSwitchRoomState with fades and room-enter dispatch off, because both make the switch break out early while a director state/cutscene is active. Only the host sends RoomState now (the guest echo was overwriting a pending host switch). The guest tracks a pending target room and retries every 8 s for up to 30 s if Room.Instance never becomes it. Local held item is also re-applied after a respawn, not just the puppet.

Verified: host warp -> guest followed and loaded Room_Exterior_Plot with both local players respawned, held wand visible on both sides; host home -> both clients back in Room_Interior_HousePlayer; guest hold -> host puppet wand and guest local wand; host unhold cleared host only.

Still racy: if a director wake-up/cutscene state runs during the switch, the load is interrupted and the 30 s watchdog retry has to take over (took 2-3 retries, ~30 s, in one run). Guest log shows DirectorWakeUpState.WakeUpFromCoffinRoutine NREs during the unload. Room=none and a missing local player are the visible symptom until a retry lands.


## 0.1.4 actor mapping 2026-11-24

Director actor mapping is now symmetric. ActorFor/Resolve map the shared PlayerEntity by direction (host=1, guest=2), map the host-only guest clone back to actor 2, and only send raw guids for real NPCs. The wire message carries a loiter actor byte too. On the host, guest-originated nodes set DirectorSync.FromGuest; a Harmony postfix on DirectorVariableRef.GetAssetValue swaps a PlayerEntity subject override to the guest clone while that flag is set. A host-initiated GoToNode clears the flag.

Verified: guest startday -> host resolved subject CoopGuestPlayer, guest resolved @Player; both advanced to day 3. Host startday advanced both to day 4 with NPC subjects shared identically. esolve/0..3 mapping: host 1=@Player, 2=CoopGuestPlayer, 3(empty)=null; guest 1=@Player, 2=@Player. efresolve: host override @Player -> CoopGuestPlayer when FromGuest, guest stays @Player. Regression: guest hold -> host puppet wand; guest warp reached Room_Exterior_Plot after retries; guest local and host puppet held wand both restored.


## 0.1.4 room arbitration 2026-11-24

RoomState now carries a cause (0 local request, 1 remote forced, 2 director). Only the host sends room, every 2 s as well as on change. The guest keeps one latest pending target. Follows wait for a safe window (not in GameSwitchRoomState, not in a cutscene, not in GameDirectorStateMachine, no director node for 1 s); a pending older than 10 s may replace a stuck switch, and the whole pending expires after 30 s. A 3 s mismatch check reloads when CurrentRoomAsset and Room.Instance disagree for 3 s with the director quiet.

Both clients defer director nodes while a forced follow is pending and replay the latest one only after the target room has been stable for 1 s. The host also routes a replacement switch through a synthetic director SwitchRoomNode so the graph owns completion instead of cancelling it.

Also fixed: per-frame subsystem NREs (GamePersistence.CurrentRoomAsset during unload) used to kill Probe.Tick and stall all room work; each per-frame call is now isolated and the room getter is guarded.

Verified: chaos warp+startday+move x3 converged 3/3, both clients, both rooms, players alive; settled warp 1.8 s; guest hold -> host puppet wand survives a room change; guest lheld kept. During the intro/wake-up cutscene a follow waits and can take ~20-30 s before converging (previously it could stall indefinitely), and the deferred graph switch can legitimately land in the room the cutscene wants rather than the requested warp.

