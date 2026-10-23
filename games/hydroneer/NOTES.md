# Hydroneer: co-op notes

Goal: true online co-op with host and join (each player on their own PC and screen),
not the official split-screen / Steam Remote Play Together mode.

## Game identity (rule 5)

| Field | Value |
|---|---|
| Store | Steam, app 1106840 |
| Version / build id | Steam build 23218978 (in-game version string not yet read) |
| `Mining\Binaries\Win64\Mining-Win64-Shipping.exe` SHA-256 | `DF27B86E4564100FA792464EA625F64C96C4D155C6C911486FE3769C8D5570B1` |
| `Mining.exe` (launcher) SHA-256 | `B56C45269DC4B36A86AFBD6C5934D0C30F9DCFDC09D58C07810847B81D4F6B70` |
| Engine | Unreal Engine 4.26 (`++UE4+Release-4.26` in the exe), PhysX 3 |
| OS | Windows 64-bit |
| Install size | 13.8 GB, 30 files, two paks (`pakchunk0`, `pakchunk1001`) |

## Phase 3 session 1 (2026-10-22): UE4SS probe on a test copy

Setup, all actually run:
- Test copy made with robocopy; file count, sizes and both exe hashes match the real install.
- UE4SS **v3.0.1** (latest stable, 2024-02-14), asset `zDEV-UE4SS_v3.0.1.zip`, 23,810,106 bytes,
  SHA-256 `af777d2af33fce4d520de35a8665e417376bcc530fec647778012ce6cdf51b2b`. Upstream publishes no
  hash for v3.0.1 (GitHub digests exist only on the moving `experimental-latest` tag), so this
  hash is the one we computed; size matches the release API.
- Only `ConsoleEnablerMod` (read: creates the UE console on F10), `Keybinds` and our own
  read-only `CoopProbe` Lua mod enabled. `steam_appid.txt` (1106840) next to the shipping exe so
  Steam does not relaunch the real install. Started `Mining-Win64-Shipping.exe` directly with
  `-userdir=<workspace> -windowed -log`. UE4SS found GUObjectArray and ran fine on this build.

### Observations (from the probe log and dumps)

- `GameEngine.NetDriverDefinitions`: `GameNetDriver` = `/Script/OnlineSubsystemUtils.IpNetDriver`,
  `DemoNetDriver`. No `NetDriver` instance at the menu (standalone).
- Framework: GameInstance `BP_GameInstance_C` (C++ base `UCGameInstance`), GameMode
  `GM_Hydroneer_C`, GameState `BP_GameState_C`, PlayerController `BP_GameController_C`,
  pawn `BP_GameCharacter_C` / `BP_Character_HydroGuy_C`. The main menu runs inside
  `/Game/Levels/L_GameWorld` (55 pawns at the menu, mostly `BP_NPC_C`).
- **No replication in game Blueprints**: no `OnRep_`, `Server*`, `Multicast*` or `Client*`
  functions in any game class header. Engine replication alone would sync nothing.
- `GM_Hydroneer_C` has `AddPlayer(FSteamRemotePlaySessionConnected Data, bool isRemote, out
  ABP_GameController_C*)`, `AddRemotePlayer`, `AddLocalPlayer`, `RemovePlayer`, `RemoveLocalPlayer`,
  `SpawnHydroguy(Transform, Controller)`, `MultiplayerStart`, `ContinueGame(Creative, CloudSaving,
  ClearResources, Slot)`, `NewGame`. `FSteamRemotePlaySessionConnected` is just `int32 SessionID`.
- Plugins of interest: `OnlineSubsystemSteam`, **SteamCore** (Blueprint wrappers for Steam
  matchmaking/lobbies, `Networking`, sessions), **EasyMultiSave** (`EMSActorSaveInterface`,
  async save/load), **Voxel** (VoxelPro terrain; a Voxel class exposes `StartServer(IP, Port)` and
  `ConnectToServer(IP, Port)`), `AutoSettings`, `PakLoader`, `EasyFileSystem`.
- Game C++ module `/Script/Mining`: `UCGameEngine`, `UCGameInstance`, `UCHydroneerLibrary`
  (`GetSaveDirectory`, `GetAppdataLocalDir`, `BackupSave`, `RenameSave`, ...),
  `UCModdingSubsystem` (Workshop mods), `ULocalCableComponent`.
- `SaveGame_GameData_C` only holds `CurrentGameType`, `CloudSave`, `CreativeMode`, `CanAchieve`;
  world contents go through EasyMultiSave.
- 7,939 `StaticMeshComponent`s at the menu, 0 simulating physics (menu state, not a loaded world).
- **`-userdir` does not isolate everything.** Engine configs, logs and the asset-registry cache
  went to the redirected dir, but the game still rewrote
  `%LOCALAPPDATA%\Mining\Saved\Config\WindowsNoEditor\Settings.ini` (`r.setres=...w` to `...f`).
  Restored from backup and verified by hash. Saves were not touched in this run (no world loaded).
  `UCHydroneerLibrary.GetAppdataLocalDir` explains the hard-coded path.

### Derived

- Saves very likely ignore `-userdir` too (same library). Do not load or save a world in a test
  copy until saves are isolated.
- Host side can create a real player slot with `GM_Hydroneer_C:AddPlayer(..., isRemote=true)`,
  the same path Remote Play uses. That gives a controller and pawn the host world fully supports
  (tools, inventory, machines), driven by our own network input instead of Remote Play.
- For the joining player to render on their own PC they still need a local copy of the world:
  host save sent on join (EasyMultiSave files), then host-authority state sync for players,
  loose items (physics), voxel edits, machines, shops/money. Voxel's own `StartServer` /
  `ConnectToServer` may cover terrain edits: untested.

## Phase 3 session 2 (2026-10-22): isolating saves, first world load

### Save isolation (all runs below actually ran)

- `SaveRedirect` Lua mod: `RegisterHook` post-callbacks on `/Script/Mining.CHydroneerLibrary:
  GetAppdataLocalDir` and `:GetSaveDirectory` rewrite any path under the real `%LOCALAPPDATA%` to a
  workspace root (from env `HYDRO_COOP_APPDATA`); it also checks AutoSettings'
  `USettingsManager.IniFilename`. A post-callback's return value overrides a native UFunction's
  return in UE4SS v3.0.1 (changelog, [UE4SS #366](https://github.com/UE4SS-RE/RE-UE4SS/pull/366)).
  Verified: `GetAppdataLocalDir()` returns the workspace root after the hook.
- Launch with `-userdir=<workspace root>\Mining` so the engine's own paths and the hooked ones
  agree. A wrapper script snapshots the real folder (SHA-256 plus mtime) before each run and
  compares it after, restoring changed files from the backup.
- Observed without any hook: `GetSaveDirectory` (native and `BP_GameController`),
  `USettingsManager.IniFilename` and EasyMultiSave all follow `-userdir`. Only `GetAppdataLocalDir`
  returns the real `%LOCALAPPDATA%`. The game never called `GetAppdataLocalDir` through reflection at
  the menu or during a new game in these runs (only our probe did).
- EasyMultiSave `GetSortedSaveSlots` returned 0 slots in the test copy (the owner's real saves did
  not show up).
- Real folder unchanged after 5 runs (3 menu-only with the redirect, 1 menu-only without the
  redirect and with the dumpers on, 1 new game with save).
- **Unexplained:** in session 1's first run the real `Settings.ini` was rewritten once
  (`r.setres` `w` to `f`). Not reproduced in any later run, including an exact repro of that run's
  setup. Shipping build writes no engine log, so the cause is `unknown`. Keep the hash guard on.

### First world (new game, test copy)

- `GM_Hydroneer_C:NewGame()` called from Lua at the main menu starts a new world. It autosaved slot
  `MySaveGame` into the workspace: `Level.sav` 21,881,873 bytes, `Player.sav` 573, `SaveData.sav` 21.
  Played-in saves on a real install are about the same size (~21 MB), so the level file is mostly
  fixed-size (voxel data is the guess).
- Fresh world: 7,782 actors in 249 classes (5,570 `StaticMeshActor`, 316 `BP_OreVein_C`, 56
  `BP_Water_C`, 16 `BP_WorldStore_C`, 9 `BP_VehicleStore_C`, ...), 12,618 primitive components,
  7 `VoxelWorld`, 0 `VoxelWorldNetworkInfo`.
- 0 components reported `IsSimulatingPhysics() == true` in a fresh world. Unverified method: a
  failing call is counted as not simulating. Recheck with items the player has dropped.
- **`GM_Hydroneer_C:AddLocalPlayer()` from Lua works**: LocalPlayer 1 to 2, a second
  `BP_GameController_C` with its own `BP_Character_HydroGuy_C`, spawned about 10 m from player 1.
  Engine split-screen follows. This is the slot a remote player would drive.

## Phase 3 session 3 (2026-10-22): driving player 2, hiding split screen

Run under the hash guard (real folder unchanged afterwards). New game, `AddLocalPlayer()`, then
from Lua, every ~16 ms on the game thread (`LoopAsync` + `ExecuteInGameThread`):

| Input sent to P2 (the game's own BP input events) | Split screen on | Split screen off |
|---|---|---|
| `BP_GameCharacter:InpAxisEvt_MoveForward_K2Node_InputAxisEvent_0(1.0)` for 2 s | P2 moved 767 cm (116 ticks), P1 0 cm | P2 moved 791 cm (117 ticks), P1 0 cm |
| `BP_GameCharacter:HandleYawInput(1.0)` for 1 s | P2 control yaw -50 to 95, P1 unchanged (-75) | 95 to 240, P1 unchanged |
| `Character:Jump()` | P2 Z 239 to 362 after 300 ms | 274 to 397 |

- `/Script/Engine.Default__GameplayStatics:SetForceDisableSplitscreen(world, true)` works:
  `IsSplitscreenForceDisabled` true. Screenshots (`shot` console command via
  `KismetSystemLibrary:ExecuteConsoleCommand`, written into the redirected userdir): before, two
  vertical halves (P1 left, P2 right); after, P1's view fills the window. P1's two after-shots are
  identical while P2 moved, turned and jumped, so P2 keeps simulating with no viewport on the host.
- Movement direction follows P2's own control rotation (the second trial walked a different
  heading after the yaw), so the game's input BP ran for P2, not a raw position write.
- `GameplayStatics:IsSplitScreen` call failed from Lua (returned a function value); not needed.

### Derived

- The host-slot model works at the input level: a remote player's intents (move axes, look,
  jump, and later the controller's action events) can be replayed on a host-side P2 that the host
  does not see on screen. The host world simulates P2 with all the game's own rules.
- What the remote player sees still has to come from their own client world (state sync), or from
  a per-player stream (Remote Play again, which the owner rejected).

## Phase 3 session 4 (2026-10-22): player 2 pickup, drop, dig (split screen off)

Five runs, all under the hash guard (real folder unchanged after each). Calls were logged with
UE4SS `RegisterHook` pre-hooks on the game's own functions, with `self` recorded.

Input mappings (read from `Default__InputSettings.ActionMappings`): Pickup = E, LMB = left mouse,
RMB = right mouse, Jump = Space, Sprint = LeftShift, Crouch = C, Rotate = R/T/Y, VehicleMode = F,
VehicleKey = Q, Wiki = B, EscMenu = Escape (plus gamepad keys).

Generated input functions on `BP_GameController_C` (which one is key-down, observed):
- `InpActEvt_Pickup_K2Node_InputActionEvent_12` = E down (alone it calls `StartFreeplacing`,
  the hold-E placement mode), `_13` = E up. A tap is down then up 100 ms later.
- `InpActEvt_LMB_K2Node_InputActionEvent_5` = LMB down (calls the carried tool's
  `LMBDownWhileCarrying`), `_6` = LMB up.
- Sending up before down, or `PlaceDownItem()` directly, left P2 in a broken state (controller
  `CarryingItem` cleared but item still `BeingCarriedBy` P2). Send real key order only.

Results, P2 driven only by these input calls, split screen force-disabled:

| Action | Call chain logged | State after |
|---|---|---|
| Aim at shovel (SetControlRotation) | — | P2 `LastThingLookingAt` = the shovel, so P2's crosshair trace runs without a viewport |
| E tap (pickup) | `TryPickupItem`, `PickUpItem` on P2's controller | P2 `CarryingItem` = shovel, shovel `BeingCarriedBy` = P2's pawn, P1 carries nothing; the shovel moved with P2 while walking |
| E tap (drop), looking at ground | `ScanForDropCollision`, `ItemDropped`, `DropItem` (P2) | nothing carried, shovel on the ground |
| LMB tap at Ember Cradle voxel dirt (`BP_HydroVoxelWorldEmberCradle`) | `MoveDirt` (P2 controller), `HitActorItem`, `SetDirt`, `LMBDownWhileCarrying` | shovel `Dirt` mesh visible; surface at the spot Z 160 to 146 (trace now hits the landscape under the voxel) |
| P2 drops the full shovel, **P1** picks it up and clicks the same spot (control) | same chain on P1's controller | shovel empty, surface Z 160 to 171 (dirt dumped) |
| LMB at Mildews voxel world | only `LMBDownWhileCarrying` | nothing happens |
| LMB at landscape near spawn | only `LMBDownWhileCarrying` | nothing happens |

- P2 `IsMainController?` = false, P1 = true.
- 7 voxel worlds found: Dreck, EmberCradle, Icehelm, Mildews, Snowrune, SouthHope, plus a
  `BP_VoxelNoDig_C`. Surface found by `KismetSystemLibrary:LineTraceSingle` from Lua.
- Screenshot after the handover: host view full screen, P1 holding the empty shovel over dirt.

### Derived

- Pickup, drop and digging run for P2 through the same Blueprint code as for P1, triggered by the
  same input events. A network client can send key events (down/up per action) plus look/move axes,
  and the host replays them on P2.
- Mildews refusing to dig in a new game is probably the per-area dig licence, not a P2 problem:
  the dig chain stopped before `HitActorItem` there and on plain landscape alike. Not checked with
  P1 at Mildews.

## Phase 4 session 1 (2026-10-22): network input prototype, two copies on one PC

Goal: a second game copy sends its player's inputs over the network and drives P2 on the host.

### Setup (all actually run)

- Two copies: the host test copy and a client copy built from **hard links** to it (28 links:
  paks and engine files) with a **private** `Mining\Binaries\Win64` (17 files: exe, UE4SS, mods;
  UE4SS writes its log there). Client exe SHA-256 matches. The second copy started fine: no
  single-instance check seen.
- `tools/gameslot` lease for 2 slots around every two-copy run; every started pid registered.
- Transport (prototype only): UE4SS Lua has no sockets, so
  `client game -(Lua appends a line per ~16 ms to outbox file)-> client_relay.py -UDP-> host_relay.py -(appends to inbox file)-> host game Lua`.
  Relays are Python standard library. The host Lua keeps the inbox open and reads new bytes each
  tick (Lua 5.4 `read` clears EOF, so appended data shows up).
- Client mod `CoopNet` hooks its own input events with `RegisterHook` (BP functions):
  `BP_GameCharacter_C:InpAxisEvt_MoveForward_*_0` / `MoveRight_*_1` (largest-magnitude value per
  tick, because the engine also fires them with 0 every frame), and both generated events (down and up)
  of Pickup, LMB, RMB, Rotate*, VehicleKey, RotationMod, BuildPlace*, Jump, Crouch, Sprint. It sends
  absolute control rotation (pitch, yaw), not look deltas. Each line repeats the last 8 events with ids,
  so a lost datagram does not lose a key press.
- Host replays: the latest axes every host tick while packets keep coming (stop after ~300 ms
  of silence), `SetControlRotation` from the client's rotation, and each new event by calling the
  same function name on P2's controller or pawn with the matching `FKey`.
- Test: a bot on the client (rule 11), no human input. Markers in the event stream let both
  sides log positions at the same points.

### Results (two clean runs; second run shown, first run within 2 percent)

| | client P1 (bot input) | host P2 (replayed) |
|---|---|---|
| walk forward 2 s | 785 cm, yaw -75 | 784 cm, yaw -75 |
| turn (`HandleYawInput` on the client) | yaw to 67.5 | yaw to 67.5 |
| diagonal 1.5 s (fwd 0.7, right -0.7) | delta (496, 205) | delta (530, 219) |
| jump (Jump events down/up) | +122 cm | +121 cm |
| E tap, LMB tap | sent | replayed in order; game called `TryPickupItem` on P2 |
| transport | 630 lines sent | 630 received, 0 lost, UDP one-way p99 0.5 ms (loopback), longest gap between lines 3 host ticks |

The diagonal leg differs by about 7 percent: the host applies the held axis once per host tick, and
host and client tick counts differ. Movement will drift without position correction.

### Real `Settings.ini` writes: cause narrowed down

- The guard caught the real `%LOCALAPPDATA%\Mining\Saved\Config\WindowsNoEditor\Settings.ini` being
  rewritten in the first two-copy run (`r.setres=<width>x<height>w` to `...f`), the same change as session 1
  run 1. Restored from backup.
- Reproduced on purpose: a brand-new hard-linked copy, menu only, nobody touching it, rewrote it
  on its **first launch**; the **second launch** of the same copy did not. Session 1 run 1 was the
  test copy's first launch; the failing two-copy run was the client copy's first launch.
- The redirected `Settings.ini` is never created, and `SettingsManager.IniFilename` points at the
  redirect when mods load, so the writer uses another path, probably resolved before UE4SS mods
  load. Nothing is written inside the install copies. Writer: `unknown`.
- Minimizing and restoring the game window did not trigger a write.
- Hooking `/Script/AutoSettings.SettingsManager:SaveSettingStatic` and
  `ConsoleUtils:SetStringCVar`/`SetIntCVar` with UE4SS v3.0.1 `RegisterHook` crashed the game at
  startup ("Tried to execute UFunction::FuncPtr hook but there was no function map entry"). Do not
  hook static native AutoSettings functions this way.
- Rule until the writer is found: every new copy gets one warm-up launch under the guard (which
  restores the file and keeps the changed one in `quarantine-*`), before any real test.

### Derived

- The network input path works: the client's real input events, captured by hooks, drive the host's
  P2 through the game's own Blueprint input code, with movement within 1 to 7 percent and actions in
  order. The same capture works for human input, because it hooks the events, not the bot.
- Still missing for playable co-op: the client sees its own unsynced world. Next: send host state
  back (P1 and P2 transforms first, then items, voxel edits, money), correct the client's own player
  from the host, and replace the file+relay transport with in-process sockets (a UE4SS C++ mod, or
  SteamCore P2P for internet play).

## Phase 4 session 2 (2026-10-22): host state back to the client

Both runs under the gameslot lease and the hash guard; real folder unchanged after each.

### What was built

- Relays are two-way. The host relay learns the client's address from its first datagram and
  sends the host's outbox lines back. The client relay writes them to a client inbox file.
- Host sends (prototype rates, host ticks of ~16 ms):
  - `S`: P1 and P2 location and control yaw every 3 ticks (~20 Hz).
  - `I`: items (`BP_ParentItem_C`) that moved more than 2 cm or turned more than 2 degrees, in
    lines of 10. Only items within 50 m of a player are scanned (every 6 ticks), the near list is
    rebuilt every 120 ticks, all items are swept every 300, and the near list is resent in full
    every 186 ticks to cover loss.
  - `M`: test markers with P1, P2 and the tracked items' positions, each sent 3 times.
- Client:
  - Puppet for host P1: the client calls `AddLocalPlayer()` too, sets split screen off, sets the
    puppet's `CharacterMovement` mode to none, and moves it 35 percent of the way to the host
    position every tick, with host yaw.
  - Own player correction against host P2: snap above 400 cm; blend 10 percent per update above
    150 cm while moving, 25 percent above 40 cm while still.
  - Items matched by object name (`GetFName`). The client turns off physics on an item's root
    when it applies a host transform.
- Test harness: a debug teleport event (`D`, `TP_x_y_z_yaw`) moves P2 on the host and the client's
  player locally at the same time.

### Results (second run; first run the same within a few cm)

| Check | Result |
|---|---|
| Host P1 seen on the client (puppet vs host P1) | 0 cm at 8 of 9 markers, 17 cm right after the host's walk |
| Client's own player vs host P2 | 0 to 4 cm when still; 16 avg / 45 max cm while walking (run 1); 29 to 34 cm right after walking stops |
| Local-only 5 m desync | detected (498 to 510 cm), snapped back, 0 cm after 100 ms |
| Lantern carried by the host's P1 | client copy 0 to 1 cm from the host's at every marker |
| Shovel picked up, carried and dropped by the client (through host P2) | 0 to 1 cm on both sides; the host's logs show `TryPickupItem` and `DropItem` on P2 |
| Transport | 1,754 client lines, 0 lost; 2,258 state lines, 3,453 item updates in 400 lines back |
| Screenshots | client view: the host's character standing at the lamp post where host P1 is; host view: P2 (the client's player) walking by the post |

### Problems found

- Item updates with names the client does not have (37 per run): created when an item is picked
  up (`BP_Lantern_C_2147478289`, `BP_ShovelT1_C_2147478271`), very likely the held proxy copies
  (`CreateProxyItem`, `ToggleOnlyOwnerSee` on `BP_ParentItem_C`). The real items stayed in sync.
  Runtime-spawned items (bought, dug, crafted) have no cross-game identity yet either.
- Item scan costs up to 22 to 23 ms on the host game thread (a visible hitch). Needs a cheaper change
  test or spreading the scan over ticks.
- After a debug teleport the client snapped back twice to stale host state before the host's
  teleport arrived (643 cm). Needs a sequence check; real play does not teleport.
- The still threshold (40 cm) leaves about 30 cm of residual error after walking. Lower it.
- The puppet has movement off, so it likely slides in its idle pose (screenshot shows it standing).
  Animation state is not sent yet.
- The client's own action events also run locally (it picks up its own copy of the shovel). The
  host transform overrides it, but the client's local game state (carrying, dirt) is not
  host-controlled.

## Phase 4 session 3 (2026-10-22): fixes for session 2's problems

Three runs under the gameslot lease and the hash guard; real folder unchanged after each.

### Changes

- **Proxy items**: `BP_ParentItem_C` has `ProxyItem` (the held copy) and `CreateProxy?`. The host
  records every item's `ProxyItem` name and never sends those.
- **Item scan cost**:
  - No full scan on one tick. Each tick checks 6 items round robin (a full sweep of 557 in ~1.5 s),
    plus "hot" items (carried, or moved in the last ~1 s) every tick. Items that ever moved are
    resent every 125 ticks.
  - No periodic `FindAllOf`: one at start, then `NotifyOnNewObject` on `BP_ParentItem_C` appends
    new items, and the sweep drops destroyed ones.
  - `collectgarbage("generational")` in the host mod (it makes many short strings each tick).
- **Correction with acks**: the host's `S` line now carries the last client input line and event
  it applied (`ack_seq`, `ack_ev`). The client keeps its own position for each line it sent and
  compares host P2 with its position *at the acked line*, so latency no longer looks like
  divergence. Corrections: snap above 400 cm, else 30 percent of the difference above 10 cm, and the
  recorded positions after the ack are shifted by the applied amount. This also fixed the teleport
  race (the client's teleport and the host's are compared at the same line).
- **Puppet animation**: the host sends P1's `GetVelocity()`. The client writes it into the
  puppet's `CharacterMovement.Velocity` every tick while its movement mode stays none. The anim
  instance (`ABP_HydroGuy_C`) reads it: `Speed` matched host P1's speed every sample
  (for example 239/239, 425/425, 312/312). An automatic fallback to walking movement mode was
  never needed.

### Results

| Check | Session 2 | Run 1 (anim, acks, spread scan) | Run 2 (+ notify list) | Run 3 (+ generational GC, 6 per tick) |
|---|---|---|---|---|
| Unknown items on the client | 37 | 0 (2 proxies skipped) | 0 | 0 |
| Host tick work avg / max | scan alone up to 23 ms | 0.78 / 16 ms | 0.78 / 11 ms (10 ticks over 4 ms) | **0.56 / 5 ms** (1 of 1,761 ticks over 4 ms) |
| Snaps at the debug teleport | 2 (643 cm) | 0 | 0 | 0 |
| Client player vs host P2 when still | 30 to 34 cm | 0 to 1 cm | 0 to 1 cm | 0 to 10 cm |
| Reconciled error while walking (avg / max) | — | 1.6 to 14 / 30 cm | 5 / 21 cm | 3 to 7 / 21 cm |
| Puppet anim `Speed` vs host P1 speed | not sent | equal | equal | (not logged) |
| Puppet vs host P1, lantern, shovel at markers | 0 to 17 cm, 0 to 1 cm | 0 to 12, 0 to 1 cm | 0 to 10, 0 to 1 cm | 0 to 14, 0 to 1 cm |
| Local-only 5 m desync | snapped back | snapped back, 0 cm at 100 ms | same | same |

- Client screenshot one second into the host's walk, enlarged: the puppet is mid-stride (legs
  apart, arms swinging), not standing.
- `os.clock` on this build has 1 ms resolution, so phase times are whole milliseconds. The last
  over-4 ms tick was in the item scan.
- Residual: the client's player can rest up to 10 cm off the host (the blend threshold); lower it
  if that shows.

## Phase 4 session 4 (2026-10-22): host decides actions, client mirrors

Two runs under the gameslot lease and the hash guard; real folder unchanged after each.

### Changes

- **Client actions no longer run locally.** World-changing actions (Pickup, LMB, RMB, Rotate,
  Rotate2, Rotate3, VehicleKey, VehicleMode, RotationMod, BuildPlaceCloser/Further) lose their key
  mappings on the client.
  - `InputSettings:RemoveActionMapping` from Lua removed nothing (22 before, 22 after), even with
    the modifier flags copied from each mapping.
  - What worked: setting each of those mappings' `Key.KeyName` to `None` in
    `Default__InputSettings.ActionMappings`, then `ForceRebuildKeymaps()` (22 to 0).
  - Jump, crouch and sprint stay local (they only move the client's own player, which is reconciled).
- **Keys are polled instead:** `PlayerController:IsInputKeyDown` for E, LMB, RMB, R, T, Y, Q,
  LeftControl and the gamepad equivalents. Edges become the same down/up events (lower generated
  index = down), which are only sent.
- **Host `S` line also carries** what P1 and P2 carry (item name or `-`) and the carried item's
  dirt flag (`Dirt` mesh visible, 1/0, -1 if none).
- **Client mirrors carrying:** host P2 carries X and the client does not, so the client calls
  `PickUpItem(X)` (the game's own carry logic). Host P2 carries nothing, so the client calls
  `DropItem`. At most two attempts per second. The client does not apply host transforms to the
  item it carries itself.
- **Client mirrors shovel dirt:** `item.Dirt:SetVisibility(...)` to the host's flag (own carried
  item and host P1's).
- **Digging:** the host hooks `BP_GameController_C:MoveDirt(PickUp?, Radius)` (args readable in
  the post hook) and sends `V|id|pickup|x|y|z|radius`, with the location from that controller's
  `CrosshairTraceHit`, sent 3 times.
  - Scoop on the client: `RemoveDirtByLocation(L, R)` at the host's point.
  - Dump on the client: the controller has no add-at-location function (only `MoveDirt`,
    `RemoveDirtByLocation`, `RemoveDirtLine`, `RemoveDirtVehicle`, `Scan for Items in Voxel Dirt`),
    so the client calls its own `MoveDirt(false, R)`; its aim equals host P2's aim.
  - Both sides trace the ground at the dig point 500 ms later; the host sends `Z|id|z` to compare.
- **Real key test:** the orchestrator brings the client test window to the front, checks
  `GetForegroundWindow` equals it, then sends E down/up with `keybd_event`. It refuses and presses
  nothing when the window is not in front.

### Results (second run)

| Step | Host (authority) | Client |
|---|---|---|
| E (bot event) at the shovel | `TryPickupItem` on P2 | mirror `PickUpItem`; both carry the shovel |
| LMB on Ember Cradle dirt | `MoveDirt(true, 30)` on P2; ground 160.2 to **145.7** | `RemoveDirtByLocation`; ground **145.7**; shovel dirt full |
| LMB 1.5 m to the side | `MoveDirt(false, 28)`; ground **194.5** | `MoveDirt(false)`; ground **194.5**; shovel dirt empty |
| E (drop) | `DropItem` on P2 | mirror `DropItem`; shovel 0 cm apart |
| **Real E key pressed on the client window** | `TryPickupItem` on P2 | key polled (down and up); mirror `PickUpItem`; **no local `TryPickupItem`** |
| Local game calls on the client, whole run | — | `PickUpItem`=2, `DropItem`=1, `RemoveDirtByLocation`=1, `MoveDirt`=1: exactly the mirrors (pickups 2, drops 1, digs 2) |

- First run of this session: mappings were not removed, and the real E press also ran
  `TryPickupItem` locally (count 1). Fixed by blanking the keys.
- While carried, the shovel is 60 cm apart between client and host: the client's first-person
  carry pose versus the host's third-person one. Expected; the client skips host transforms for
  its own carried item.

## Phase 4 session 5 (2026-10-22): runtime item identity (blocked on client spawning)

All runs under the hash guard (one two-copy run with the gameslot lease, three single-copy
experiments); real folder unchanged after each.

### Host side: works

- Every new `BP_ParentItem_C` (`NotifyOnNewObject`) is registered ~10 ticks after construction.
  Proxies are excluded by checking the carried items' `ProxyItem` first (1 new proxy skipped in
  the run). Others get a net id `N<k>`. The host sends
  `N|id|class path|transform` (3 times) and uses the id everywhere on the wire: `I` lines, the
  `S` carry fields, and markers.
- Game spawn path used for the test: `GM_Hydroneer_C:SpawnInitialItems()` created 8 items, all
  registered: `N1` `BP_WoodenCart_C`, `N2` `BP_WorldMap_C`, `N3` `BP_TreasureChest_C`,
  `N4` `BP_Lantern_C`, `N5` `BP_Bucket_C`, `N6` `BP_Brush_C`, `N7` `BP_Pan_C`, `N8` `BP_ShovelT1_C`,
  with class paths such as `/Game/WorldObj/Items/Tools/BP_Pan.BP_Pan_C`. Host P1 picked up `N1`
  through its input events (`P1 carries N1`) and dropped it.
- **Destroy detection:** after `K2_DestroyActor`, UE4SS 3.0.1 still reports `IsValid() == true`
  and `FindAllOf` still lists the actor; its location reads 0,0,0 and **its root component is
  invalid at once**. The first run therefore sent no `X` for the destroyed `N2`. The host now
  treats "root component invalid" as gone. **Not yet run in a two-copy test.**

### Client side: spawning from Lua fails on UE4SS 3.0.1

| Attempt | Result |
|---|---|
| `GameplayStatics:BeginDeferredActorSpawnFromClass(ctx, class, FTransform-table, 1, nil/pawn)` | pcall fails; the error value is a Lua function, no message in the log; 3 variants |
| `KismetMathLibrary:MakeTransform(...)` then pass its result | MakeTransform returns a plain Lua table; passing it fails the same way. Inferred: FTransform parameters do not convert in this UE4SS build |
| `GameplayStatics:SpawnObject(actor class, outer)` (only to test class-param conversion) | Class parameter converts; it **constructed a `BP_Bucket_C` object owned by the controller**, not placed in the level (test world only). Do not use for actors |
| `GameplayTask_SpawnActor:SpawnActor(owner = BP_AIC_C, location, rotation, class, false)` (no FTransform) | returned nil (no task); the `TScriptInterface` owner probably does not convert |
| Game functions that spawn from a class | `BP_ParentMasterwork_C:SpawnItem(Class)` (crafting output; same graph has `CheckForAchievements`) and `C_Task*:SpawnReward(...)` (quests). **Not used**: they may touch Steam achievements or quest state on the owner's account. No other game or engine function takes class + location for an ordinary actor |

### Options (owner decision)

1. Try a newer UE4SS build (`experimental-latest`, `UE4SS_v3.0.1-1181-g83330635`, published with
   a GitHub SHA-256 digest; uploaded 2026-10-17) and repeat the spawn tests. Cheap; unknown whether
   it fixes FTransform or interface parameters.
2. A UE4SS **C++** mod: real `SpawnActor`, and in-process UDP sockets (no files and relays). Needs a
   C++ toolchain (Visual Studio Build Tools, CMake) and the UE4SS C++ build, which per its docs
   needs a GitHub account linked to the Epic Games organization for the Unreal headers.
3. Replay the cause, not the result: call the same game function on the client (for example
   `SpawnInitialItems`) and pair new items by class and order. Duplicates side effects (money,
   quests, achievements), so only for side-effect-free spawns. Not recommended as the general path.

## Phase 4 session 6 (2026-10-22): newer UE4SS, client spawning works

### UE4SS experimental build (test copies only)

- Release tag `experimental-latest`, asset `UE4SS_v3.0.1-1181-g83330635.zip`, 8,748,315 bytes,
  uploaded 2026-10-17. SHA-256 `b9a166a756605b21058a64cde93bcaab1a855dba3553e5c7e6ce0fa8fdfad7b9`
  **matches the GitHub release digest**. Log header: `UE4SS - v3.0.1 Beta #0 - Git SHA #83330635`.
  The tag moves; this exact asset is the pin.
- Layout change: `dwmapi.dll` stays next to the exe; everything else is in `Win64\ue4ss\`
  (`UE4SS.dll`, settings, `Mods\` with `mods.txt` and `mods.json`, `UE4SS.log`). The 3.0.1 files
  were moved, not deleted, to a backup folder per copy. Only our mods plus `ConsoleEnablerMod`
  and `Keybinds` are enabled (both read).
- New Lua API in this build (strings in `UE4SS.dll`): `UWorld:SpawnActor(UClass, table Location,
  table Rotation)`, `LoopInGameThreadWithDelay`, `ExecuteInGameThreadWithDelay`,
  `LoopInGameThreadAfterFrames`, `CancelDelayedAction`, `PauseDelayedAction`,
  `RegisterLoadMapPre/PostCallback`.
- Single-copy test: `pc:GetWorld():SpawnActor(BP_Bucket_C, loc, rot)` and a `BP_Pan_C` both spawned
  into `PersistentLevel` at the requested positions; moving them afterwards works.
- `InputSettings:RemoveActionMapping` works on this build (22 to 0); on 3.0.1 it did nothing.

### Crash on the new build, and the fix

- First two-copy run: the host exited (code 3) right when its 16 ms loop started. The game's crash
  report (`<userdir>\Saved\Crashes\UE4CC-Windows-...\CrashContext.runtime-xml`; it lands in the
  redirected userdir) says `EXCEPTION_ACCESS_VIOLATION reading address 0x0`, call stack inside
  `UE4SS` from the engine tick, 95 s after start. A single-copy host-only rerun logged
  `[Lua::Registry::get_function_ref] Ref was not function` after tick 4, and the loop stopped.
- That loop queued a new `ExecuteInGameThread` callback every 16 ms from `LoopAsync`, and the host
  had switched Lua to `collectgarbage("generational")`. Both were removed: all loops and delays now
  use `LoopInGameThreadWithDelay` / `ExecuteInGameThreadWithDelay` when present (the old pattern
  stays as a fallback for 3.0.1). Which of the two caused the crash is **unknown**.
- After the change: host-only run past tick 100 without errors, and the full two-copy run below
  finished with no crash report.
- The crash reporter (`CrashReportClient`) was not left running.

### Results: two copies, both on the new build

| Check | Result |
|---|---|
| Runtime items from `GM_Hydroneer_C:SpawnInitialItems()` on the host | 8 registered (`N1`..`N8`); 2 new proxies skipped |
| Copies spawned on the client (`World:SpawnActor` with the host's class path, location and rotation) | **8 of 8**, at 0 to 1 cm from the host's items |
| Host destroys `N2` (stand-in for sold or consumed) | host: `item gone: N2` (root component invalid); client: copy destroyed, still gone at every later marker |
| Client's player carries `N8` through the host | host `TryPickupItem` on P2 (carry field `N8`); client mirrors `PickUpItem` on its spawned copy; carried, dropped; `N8` 0 to 7 cm apart |
| Digging and dumping (repeat) | ground 145.7 / 194.5 on both sides |
| Unknown items on the client | 0 |

### Problems found

- **Brush `N6` was 47 cm off on the client** from `h_moved` to the end. Likely cause (not
  verified): the client's spawned copies overlap the client's own starter items at the same spots,
  and client physics pushed it. The host never moved it, so it never resent it (the periodic resend
  only covers items that moved on the host). Fix options: keep physics off on all client copies, or
  a slow full resend of all items.
- Host P1's bot picked up the original starter cart instead of `N1` (both carts at the same spot).
  Harmless here.

## Phase 4 session 7 (2026-10-22): client copies stay where the host has them

One two-copy run (both copies on UE4SS v3.0.1-1181) under the gameslot lease and the hash guard;
real folder unchanged, no crash report.

### Changes

- **Client physics guard:** each tick, 8 item copies (round robin over the client's starting items
  plus every spawned copy) get their root's physics turned off if it simulates. The client's own
  carried item is skipped.
- **Host slow full resend:** besides change-based updates, 2 items per tick are sent forced, so
  every item goes out again every ~4.5 s (564 items at ~60 ticks/s).
- **Whole-world check (test harness):** at the end, host and client each write every live item
  (wire name and location) to a file in the run folder; `local/tools/coopnet/compare_items.py`
  compares them.

### Results

| Check | Result |
|---|---|
| All items, host vs client, at the end | **564 vs 564, all matched, 0 missing, 0 extra, max error 0.0 cm** |
| Brush `N6` (47 cm off in session 6) | 1 cm at markers, 0.0 cm in the final comparison |
| Physics guard | turned physics off **0 times**: no copy simulated physics this run, so the drift did not happen and the guard did not act. The fix that shows its effect is the full resend; session 6's drift cause stays unverified |
| Runtime items, destroy, carry, digs | as session 6 (8 copies, `N2` removed, `N8` carried, ground 145.7/194.5) |
| Item traffic | 15,656 item updates in 7,850 lines (session 6: 927 in 437). About 50 datagrams/s; a forced resend line holds only 2 items |

### Problems found

- Batch the forced resend better (it flushes every tick with 2 items).
- Reconciled error while the client carried `N8` reached 67 cm max (22 avg) on one stretch; other
  runs stayed under ~20 cm. Host tick cost was not measured in this run; check whether the resend
  adds load.

## Phase 4 session 8 (2026-10-22): resend batching, host cost, the correction spike

Six two-copy runs under the gameslot lease and the hash guard; real folder unchanged after each,
no crash reports. All on UE4SS v3.0.1-1181.

### Resend batching and host cost

- Forced resends now go in their own batch, sent at 12 items (~every 6 ticks): 15,057 to 15,556
  resent items in 1,254 to 1,296 full lines per run (session 7: ~7,500 half-empty lines).
- Host tick work: avg 0.46 to 0.50 ms, max 3 to 5 ms, 0 or 1 tick over 4 ms per ~3,000 ticks.
  Phase maxima: item scan 3 to 5 ms, axes+S up to 5 ms, resend 1 to 3 ms, inbox+replay 1 to 3 ms.
- **Observed:** the game-thread loop (`LoopInGameThreadWithDelay(16)`) runs about every **34 ms**
  (~30 Hz), not 16 ms. One ~520 ms gap per run at start (world load).

### The correction spike

- Client logging (every reconciliation step over 30 cm) showed the error growing steadily in the
  walking direction to ~70 cm: host P2 walked ahead of the client's own position at the acked
  line, with the acked line 33 to 34 ms old. The client's **own** movement (position change per
  tick, corrections excluded) was **11 to 18 cm/s** against `MaxWalkSpeed` 425; host P2 moved at
  399 to 425. The client was being dragged by the blends, not walking.
- Isolation runs (three walks each: nothing carried, after picking the original shovel, carrying
  spawned copy `N8`):

| Run | Client physics guard | Host full resend | Own speed, cm/s | Max reconciled error | All 564 items max error |
|---|---|---|---|---|---|
| A | on | on | 18 / 11 / 17 | 70 cm | 0.0 cm |
| B | off | off | 338 / 76 / 331 | 35 cm | 4.3 cm |
| C | on | off | 17 / 12 / 17 | 69 cm | 25.1 cm (2 items over 5 cm) |
| D | off | on | 139 / 49 / 239 | 42 cm | 0.0 cm |
| final | off | on | 233 / 96 / 112 | 54 cm | 0.0 cm |

- **Observed:** with the physics guard on, the client's player barely moves (A, C), although the
  guard only read component state (`K2_GetRootComponent`, `IsValid`, `IsSimulatingPhysics` on 8
  items per tick) and changed nothing in any run. **Mechanism unknown.** Guess (unverified): the
  extra UE4SS calls in the client's game-thread callback delay the bot's movement calls, which
  also run as delayed game-thread actions.
- **Observed:** the full resend is what keeps items right (C without it: 25 cm drift; D and final
  with it: 0.0 cm).
- The guard is now off (only with env `HYDRO_COOP_GUARD=1`). Applying a host transform still turns
  physics off on that item.
- Still open: the client's own walking speed varies a lot between runs (the "after pickup" walk is
  always slow: 49 to 96 cm/s), so 35 to 54 cm corrections remain during walks. The bot drives
  movement by calling the axis event from a ~30 Hz timer, not every frame as real input does. Next
  test: hold a real key (W) on the client window and measure own speed, to see whether real input
  shows the same slowdown.

## Phase 4 session 9 (2026-10-22): real held W on the client

Two two-copy runs under the gameslot lease and the hash guard; real folder unchanged after each.

### Method

- The orchestrator now takes `HOLD <key> <ms>` requests (max 5000 ms). It brings the client test
  window to the front, checks `GetForegroundWindow`, presses W with `keybd_event`, keeps checking
  every 20 ms that the window is still in front (releases at once if not), and releases after the
  time. Both holds lasted 2,004 to 2,033 ms.
- The client logs its own speed (corrections excluded), distance moved and the worst reconciled
  error during each walk; spike lines give per-update detail.

### Run 1: real input versus bot input (P2 replay still on the 30 Hz timer)

| Walk | Client own speed | Worst correction |
|---|---|---|
| bot (axis event from a ~30 Hz timer), nothing carried | 182 cm/s | 29 cm |
| **real W held, nothing carried** | **400 to 405 cm/s** (MaxWalkSpeed 425) | 52 to 74 cm |

- **Observed:** the slow client walks of session 8 come from the bot's input method. Real input
  walks at full speed.
- With real input the error is the reverse: host P2 trails the client's acked position by a steady
  34 to 53 cm along the walk, peaking at 74 cm when the client stops. Host P2 replays axes once per
  host timer tick (~34 ms) while real input arrives every frame, so P2 starts later and moves a
  little slower.

### Fix: replay P2's axes every frame

- The host replays the latest movement axes and control rotation with
  `LoopInGameThreadAfterFrames(1, ...)` (UE4SS v3.0.1-1181): **70 to 81 calls per second**
  observed. The rest of the host work stays on the 34 ms timer. The stale-input timeout is now
  300 ms of clock time (it was 18 timer ticks, ~600 ms at the real tick rate).

### Run 2: with per-frame replay

| Walk | Client own speed | Worst correction | Host P2 |
|---|---|---|---|
| **real W held, nothing carried** | **409 cm/s**, 800 cm in 2 s | **19 cm** | 425 cm/s |
| real W held, carrying shovel `N8` (picked with a real E) | 39 cm/s, 79 cm | 13 cm | did not move faster either |
| bot walk, nothing carried | 162 cm/s | 75 cm | 425 (now ahead of the bot) |

- The real carrying walk was slow on both sides alike. The player was looking steeply down at the
  shovel when W was held; whether that or an obstacle slowed it is **unknown**. The sides agreed
  (13 cm), so it is not a sync problem.
- Host tick work avg 0.44 ms, max 3 ms, 0 of 3,383 ticks over 4 ms.
- All items: host 564, client 565, 564 matched, max error 0.2 cm. The extra client item
  `BP_ShovelT1_C_2147478441` is very likely the client's own held proxy from its local carry; the
  client's dump does not leave out its own proxies yet.

### Run 3 (same session): bot movement every frame, client proxy left out of its dump

- The bot's `drive()` now calls the axis event every frame with `LoopInGameThreadAfterFrames(1)`
  (timer fallback on 3.0.1). The client's item dump leaves out the `ProxyItem` of the item it
  carries (logged: `leaving out our own held proxy BP_ShovelT1_C_2147478377`).

| Walk | Client own speed | Worst correction |
|---|---|---|
| bot, nothing carried | **405 cm/s** (was 162 to 182) | 18 cm |
| real W held, nothing carried | 409 cm/s | 18 cm |
| bot, carrying spawned copy `N8` | **395 cm/s** | 13 to 19 cm |
| bot right after picking a shovel (looking steeply down) | 56 cm/s | 13 cm |
| real W right after a real E pickup (looking steeply down) | 43 cm/s | 13 cm |

- Worst reconciled error over the whole run (after the start snap): **19.4 cm**.
- The two slow walks both start while the player looks steeply down at the shovel just picked up,
  and host P2 was equally slow (32 cm/s logged). **Likely** the downward look direction; not
  verified. Not a sync issue (sides within 13 cm).
- All items: host 564, client 564, all matched, max error 0.1 cm, nothing extra.
- Host tick work avg 0.44 ms, max 3 ms, 0 of 3,392 ticks over 4 ms after the client connected.
  One item-scan phase maximum of 10 ms falls outside those counted ticks, most likely before the
  client connected (world load).
- Real folder unchanged.

### Still open

- The dump mirror relies on the client's aim matching host P2's. Not tested with lag or movement
  during the click.
- `MoveDirt(true)` on the host may spawn items found in the dirt (`Scan for Items in Voxel Dirt`);
  items spawned at runtime still have no cross-game identity.
- Only the E key was tested as a real key press. LMB/RMB/gamepad polling only ran via bot events.
- Transport is still files plus relays.

### Unknowns

- Other tools: pickaxe on `BP_OreVein_C`, pan, bucket, building; shop and other UI widgets for P2
  (they would need to show on the remote client's screen instead).
- Whether P2's HUD/UI widgets are created and where they render with split screen disabled.
- P2 `CrosshairTraceHit` did not change in one run after a second `SetControlRotation`
  (session 4, v4 run); in the next run it was right each time. When it refreshes is unknown.
- Save behaviour with P2 present (whose `Player.sav`, does P2's inventory persist).
- Physics item counts and behaviour in a played-in save.
- Whether the Voxel network functions are compiled in and working in shipping.
- How many physics-simulated items a mid-game save has (needs a loaded world).
- Exact save path logic in `GetSaveDirectory`.

## Observations (sourced)

- The official multiplayer (2.0 update) is split-screen; online play goes through Steam Remote
  Play Together, so the remote player sees a stream of the host's split screen.
  [TechRaptor guide](https://techraptor.net/gaming/guides/hydroneer-multiplayer-guide),
  [2.0 update news](https://techraptor.net/gaming/news/hydroneer-20-multiplayer-update-is-out-now)
- The developer team says it "tried all avenues" for true multiplayer and blames physics load;
  a moderator calls a full multiplayer mod "highly unlikely" (2022-08-03/04).
  [Steam thread "Any multiplayer mod?"](https://steamcommunity.com/app/1106840/discussions/0/3458220484817654192/)
- Community mods load through HML (Hydroneer Mod Loader); Workshop mods work in split-screen.
  [Steam thread](https://steamcommunity.com/app/1106840/discussions/0/3729575504107129027)
- A community UE editor modkit exists for game version 3.0.8 and used UE4SS's UHT generator
  for dummy headers, so UE4SS runs on at least that build.
  [Hydroneer-Modkit](https://github.com/Buckminsterfullerene02/Hydroneer-Modkit)
- [Hydroneer-Template](https://github.com/Buckminsterfullerene02/Hydroneer-Template) describes
  itself as "A template UE4.26 project for BP modding".
- Related but not host/join co-op:
  [Hydroneer-P2-Keyboard](https://github.com/codeopen0-cpu/Hydroneer-P2-Keyboard) (keyboard for
  split-screen P2, pushed 2026-06-07);
  [HydroneerAPMod](https://github.com/Thedragon005/HydroneerAPMod) (Archipelago randomizer,
  item sync only, pushed 2026-09-26).

## Derived

- Engine 4.26 comes from the modding template, not from the exe. Check the exe version info.
- Split-screen means the game already handles several player pawns and controllers in one
  world. A host/join mod could reuse that: the host keeps simulating everything (physics,
  water, machines, saves) and a remote client drives a host-side player slot as a puppet,
  with its own camera on its own PC.

## Guesses (untested)

- The UE4 net driver may still be in the shipping exe (`open <ip>` / `?listen`), but the
  game's Blueprints are almost certainly not replicated, so engine replication alone would
  not sync the world.
- Full physics state sync (many loose ore, pans, buckets) is the hard part the developer hit.
  A host-authority design with snapshot interpolation for nearby objects is the likely route.

## Unknowns

- Exact build, exe hash, UE version.
- Whether `IpNetDriver` / `SteamNetDriver` are in the shipping build.
- Save format and where saves live.

## Searches run (2026-10-22)

- This repo: DIRECTORY.md, `games/`, issues and PRs for "hydroneer": nothing found.
- Web: "Hydroneer multiplayer co-op how to host join", "Hydroneer online multiplayer mod host
  join", "Hydroneer modding UE4SS ...": no host/join mod found.
- GitHub repo search: "hydroneer", "hydroneer multiplayer", "hydroneer coop", "hydroneer mod":
  no host/join mod found. Forks and branches of hits not yet checked.
- Not yet checked: Nexus, Hydroneer modding Discord, Reddit, Steam Workshop.

## Phase 7 session 1 (2026-10-22): friend-test build 0.1 over Steam (Spacewar 480)

All game runs under the gameslot lease and the hash guard.

### Inputs (downloads approved by the owner, each pinned)

- Steamworks.NET Standalone 2025.164.1: zip SHA-256 `9412348cc404563be5a43a28347cfeda3c679ee044a14d87a507ed2d796a537d`
  (matches the GitHub release digest); `Windows-x64/steam_api64.dll` SHA-256
  `eb17909a76668cf9ae0b92a618a34a50f6c73d3a6787cb4dd8ce36a8b10bfb75` (matches
  [playbook/networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md)). The game's own
  `Steamv147\Win64\steam_api64.dll` has no `SteamAPI_InitFlat`, no `ManualDispatch` and no
  `SteamNetworkingMessages` flat exports, so the relay cannot use it.
- Python 3.12.10 from NuGet (`python.3.12.10.nupkg`, 14,515,433 bytes, SHA-512 matches nuget.org's
  `packageHash`; python.org publishes only MD5 and Sigstore for the embeddable zip). Trimmed to what
  the relay needs.

### What was built

- `coopnet_relay.py`: one relay, `--transport udp | steam | mocksteam`. Steam: AppID 480, ctypes
  flat API, manual callback dispatch, friends-only lobby tagged `hydroneer_coopnet=1`, rich presence,
  `ISteamNetworkingMessages` (data channel 0 unreliable + NoNagle, control channel 1 reliable:
  hello, 1 s keep-alive, bye; peer timeout 20 s), session accept only from lobby members (host) or
  the host (client), message size at offset 8 and sender at 16 or 12 (auto-detected, logged). The
  friend finds the host by scanning their Steam friends for a 480 lobby (no overlay invite from a
  console program); `--lobby <id>` as fallback. `mocksteam` copies the lobby / session-accept /
  keep-alive rules over localhost for one-PC tests.
- Mod player mode: `coopnet_config.lua` (written by the installer) names the folder shared with the
  relay; the relay writes `role.txt`; the player presses **F7** in a world; status lines go back to
  the relay window. Test bots only with `HYDRO_COOP_BOT=1`.
- Package `HydroneerCoop-0.1` (19.0 MB zip): `Install.bat` / `Uninstall.bat` / `Host.bat` / `Join.bat`,
  `README.txt`, `files\` (UE4SS v3.0.1-1181 with only CoopNet), `relay\`, `python\`, `licenses\`.
  The installer finds Hydroneer through Steam, warns on an untested exe hash, refuses when another
  mod already uses `dwmapi.dll` / UE4SS, backs up `SaveGames` and `Config`, and writes a manifest;
  the uninstaller removes exactly that.

### Tests that ran

| Test | Result |
|---|---|
| Two relays over `mocksteam` (no game) | 20/20 lines each way, session accepted |
| **Real Steam, this PC's account**: host relay | `InitFlat` OK as 480, friends-only lobby created and tagged, clean shutdown |
| **Real Steam**: client relay friend scan | ran, reported no friend hosting (correct) |
| Player mode, two test copies, `mocksteam` (repo files) | 564/564 items within 0.1 to 0.4 cm, real held W 404 to 407 cm/s, worst correction 16 to 31 cm, late join OK |
| **Installed package**, two fresh copies, the package's own Python and relay, `mocksteam` | 564/564 items within 0.2 cm, real held W 408 cm/s, worst correction 19 cm, digs, pickups, 8 runtime items, removal |
| Install twice / uninstall twice | refused with a clear message, nothing changed |
| Uninstall | both game folders byte-identical to before install (SHA-256 of every file) |
| Real Steam from the package's own Python | lobby created, clean shutdown |

- Bug found and fixed: a late-joining client read a backlog of host state lines that acked the same
  input line, and each re-applied the same correction (27 snaps, 25,900 cm). The correction now
  also shifts the acked line. The host now repeats spawn lines of runtime items and the removed-item
  list every ~5 s for late joiners. Digs made before the friend presses F7 are not replayed.
- Bug found and fixed: the relay showed a status line left over from an earlier session.
- The first launch of each new test copy rewrote the real `Settings.ini` once more
  (`r.setres ...w` to `...f`); the guard restored it. Players install into their existing game
  folder, so this is expected at most once (window mode), and the installer backs up `Config`.

### Not tested (needs two Steam accounts)

- A real two-account session over Steam's relays: lobby join, session accept, the message sender
  offset on x64, latency and loss through Valve's network.
- Different game versions, other mods next to it, and the Steam overlay or Spacewar status side effects.

## Phase 7 session 2 (2026-10-22): Linux (Proton) support in the friend-test build

Hydroneer has no native Linux build (Steam platforms: Windows only); ProtonDB rates it Gold
(62 reports, checked 2026-10-22). On Linux the game, UE4SS and the mod run under Proton; the relay
runs natively next to the Linux Steam client.

### What was added

- Relay: loads `libsteam_api.so` on Linux (from the same verified Steamworks.NET zip,
  `OSX-Linux-x64/`). Steam packs callback structs with 4 on Linux/macOS and 8 on Windows; of the
  callbacks used, only `LobbyCreated_t` moves (lobby id at 4 instead of 8). The relay reads the
  platform's offset and checks that it decodes a chat-type SteamID (top byte 0x01, type 8), else
  tries the other. `SteamNetworkingMessage_t` and the other structs used have the same layout.
- `install.sh`: finds Steam (native `~/.steam/steam`, `~/.local/share/Steam`, Flatpak, Snap), reads
  `libraryfolders.vdf`, finds `appmanifest_1106840.acf`, checks the exe hash, refuses when another
  mod has `dwmapi.dll`/UE4SS, backs up `SaveGames` and `Config` from
  `<library>/steamapps/compatdata/1106840/pfx/drive_c/users/steamuser/AppData/Local/Mining/Saved`,
  writes the run folder as a Wine path (`Z:/home/...`), records a manifest, and prints the Steam
  launch option `WINEDLLOVERRIDES="dwmapi=n,b" %command%` (Proton loads its own dwmapi otherwise).
- `uninstall.sh`, `host.sh`, `join.sh` (system `python3` 3.7+, checked); README Linux section.

### Tests that ran (WSL Ubuntu 22.04, Python 3.10.12; no game, no Steam client in WSL)

| Test | Result |
|---|---|
| `bash -n` on the four scripts | all parse |
| Two Linux relays from package copies, `mocksteam` and `udp` | 10/10 lines each way, session accepted, roles written |
| Load `libsteam_api.so`, resolve every flat function the relay uses | 38 resolved; `IsSteamRunning()` false (no Steam in WSL) |
| `LobbyCreated_t` parsing, Linux and Windows layouts | both give the lobby id |
| Steam transport with no Steam client | stops with "Steam is not running..." |
| `install.sh` on a fake Steam layout (second library, Proton save folder, the real exe) | found the game through `libraryfolders.vdf`, backed up 2 save files, installed 7 files, Wine-path config, launch option printed |
| Install twice; another mod's `dwmapi.dll`; changed exe answered "no" | refused / refused, folder unchanged / nothing installed |
| `uninstall.sh` | game folder identical to before (SHA-256); second uninstall refused |

### Not tested (needs a Linux PC with Steam and the game)

- UE4SS loading under Proton with the launch option; the mod's file paths through `Z:`; real
  Steam from a native relay on Linux; Flatpak/Snap Steam (the relay may not reach a sandboxed client).

### Finding: the real `Settings.ini` changed again at 18:00, an hour after the last test run

- Hash check of the owner's real folder against the backup: 52 files, only `Settings.ini` differs
  (`r.setres ...w` vs `...f`, modified 18:00:28). Steam `LastPlayed` for app 1106840 is 17:01:12 (the
  test copies, which run as app 1106840); no game process was started after that.
- Guess (unverified): Steam Cloud. The new test copies wrote the real file at 16:58 (first launch),
  Steam may have uploaded it when those test games exited, the guard restored the local file at
  17:01, and Steam synced the cloud copy back at 18:00. If so, test copies running as app 1106840 can
  push this one file through Steam Cloud. Saves are unaffected (all match the backup; test saves
  are redirected).

## Phase 7, session 3: one launcher per OS (2026-10-22)

### Built

- Windows: `Start Hydroneer Co-op.bat` opens `launcher.ps1`, a WinForms window. It has buttons for
  Install, Uninstall, Read me, Open logs, Host, Join and Stop, a Lobby box, and a live log. It calls
  `install.ps1 -Detect` / `-NonInteractive` (exit code 3 means the version differs, then a yes/no box),
  runs the bundled Python relay with `--stop-file`, and Stop writes that file (it kills the relay
  after 4 s only if the relay is still running).
- Linux: `bash hydroneer-coop.sh` uses a zenity window when zenity and a display are present,
  and a text menu otherwise (`HC_TEXT=1` forces the text menu). Host/Join show a live log, and
  closing that log stops the relay through the stop file.
- The relay got `--stop-file` (also honoured while waiting for a mock lobby), `STOPPED:` lines for
  early errors, and a clear error when several friends are hosting and there is no terminal.
- The old .bat/.sh scripts are unchanged and still work on their own.

### Tests that ran

| Test | Result |
|---|---|
| Two launcher windows (host + join), `mocksteam`, package test copies, `-SelfTest` | host connected after 11 s, client after 5 s; Stop gave a clean exit (code 0, "relay stopped"); buttons enabled correctly in each state; after uninstall both game folders were byte-identical; the owner's real data was unchanged (52 files) |
| Linux text menu, host and join at once from two package copies (WSL, `mocksteam`) | both `STATUS: connected`; install/uninstall through the menu; both fake game folders identical to before (SHA-256) |
| Linux zenity path with a stand-in `zenity` that records calls | list → install → host log window → question → uninstall → list; status text updates; fake game folder identical afterwards |

### Not tested

- Real zenity and a real desktop; real Steam between two accounts; Proton.

### Bug found by the owner: "Install says already installed, Uninstall can't find it"

- Cause, observed: two places held the "installed" state. Install checks a marker inside the game
  folder (`ue4ss\HydroneerCoop-installed.txt`), while Uninstall and the launcher looked for a
  record in the package folder (`installed.json` / `installed-linux.txt`). After the package
  folder was replaced (a fresh unzip, or here a rebuild of the folder the owner had installed from),
  the game still had the mod but no package copy could remove it.
- Fix: the game folder decides. `install -Detect` / `--detect` now prints `INSTALLED=yes|no` from
  the marker, and the launcher and Linux menu use that. Install also keeps a copy of its record in
  the game's `ue4ss` folder. Uninstall finds the game through Steam when the package has no record
  (or a stale one). For installs made before this fix (a marker and no record), it removes
  `dwmapi.dll` only if it matches the package's own copy.
- The build script now empties the dist folder instead of deleting it, and refuses to touch one
  that has been installed from (`installed.json`, `installed-linux.txt` or `backups`).
- Tests (test copy `hydroneer-pkgtest-host` on Windows; a fake Steam layout in WSL), each passing on
  both OSes:
  - Install from copy A, uninstall from copy B.
  - The old state (marker only, no record): uninstalled from a fresh copy.
  - Uninstall from A after B already removed the mod: says nothing to remove and clears the stale record.
  - Installing twice: refused.
  - Linux menu: install from B, then the menu from A shows it as installed and uninstalls it.
  - In every case the game folder came back byte-identical (SHA-256).

## First real Windows + Linux (Proton) session (2026-10-22)

- Observed (host's relay log and UE4SS.log; the Linux player's logs not seen yet):
  - The Steam relay connected and the client's lines reached the host (~60/s), with P2 driven
    correctly (replays, `TryPickupItem` on P2).
  - The host sent ~24 lines/s (p99 735 bytes, max 746, no CR).
  - Yet the Linux player saw nothing from the host: no host player, no digs, no pickups.
- Derived: the client game never read its inbox. Wine's CRT (`dlls/msvcrt/file.c`) sets `WX_ATEOF`
  on an fd after a 0-byte read, and every later `_read` returns 0 until `_lseek` clears it. The mod
  opened `from_net.txt` while it was still empty, so on Proton it stayed at EOF forever. Windows
  doesn't do this, which is why every Windows-only test passed.
- Fix: `f:seek("cur", 0)` before each `f:read("a")` in `line_reader`. Shipped to the Linux player as
  `CoopNet-linux-fix-1` (a replacement `main.lua`).
- Not tested yet:
  - Under Wine (none available here).
  - The Windows regression with the seek (needs a game run; the owner's game was in use).
- Playbook lesson: tailing a file that another process appends to needs a seek before each read under Wine/Proton.
- Windows regression check with the seek fix (2026-10-22, two copies, PlayMode, mocksteam, under the guard): client
  items applied 14206 unknown 0; mirror pickups 2 drops 1, dirt 2, digs 2; dig ground diff 0.0 cm both digs;
  echo comparisons 0-8 cm; runtime copies spawned 8 failed 0; host lines=2860 lost=0. The real-key
  steps were skipped (client window not in front, by design). Owner's real data unchanged (52 files).
  Still not tested under Wine/Proton.

## Second real Windows + Linux session: desyncs (2026-10-22)

The owner reported three problems: the host never turned on the friend's screen, the friend's own
pickups didn't show for him, and digs and dumps didn't line up. Evidence: the friend's UE4SS.log,
relay log and inbox file, plus the host's UE4SS.log.

### Observed

- **Starter items.** A new game spawns its starter tools at runtime, with game-made names
  (`BP_ShovelT1_C_2147479685`). These names differ between two PCs.
  - The host sent them by name. The friend had none of them: 1,653 updates for the shovel alone,
    and "unknown" climbed to over 2,000.
  - The friend carried the host's starter shovel, so `mirror pickups` stayed at 0 all session.
  - Two copies on one PC get *identical* names, which is why every earlier test passed.
- **Stale lines.** The friend's inbox began with a quit host game's lines. The client read the
  inbox from the start at F7 and took in that game's N1–N8, which clashed with the new host's N1.
- **Digs.** Host P1's digs were mirrored with the client's own `RemoveDirtByLocation` and
  `MoveDirt`. The client's ground never changed: it stayed at 155 while the host's went to 32.
  Dumps also went to the client's own aim.
- **Turning.** The puppet's yaw was set on the pawn, and its local controller overwrote it.

### Fixes

- Pairing: the host sends `A` lines for items that existed at F7 with game-made names. The client
  pairs each with its nearest unpaired item of the same class, or spawns a copy if it has none.
- Terrain edits: the host hooks `/Script/Voxel.VoxelSphereTools` (RemoveSphere, AddSphere,
  SetValueSphere, WithDurability and the Async variants). It sends each call as a `W` line, and the
  client makes the same call on `Default__VoxelSphereTools`.
  - Discovery: a scoop is `RemoveSphere(EmberCradle, pos, 30, true x4)`; a dump is
    `AddSphere(…, 28, true x4)`.
  - With the host capability `W`, the old V-path dig mirror is skipped.
- Inboxes skip their existing content at the first read. S lines carry a session id; on a change,
  the client resets its copies and shows "host restarted co-op".
- The puppet's controller gets SetControlRotation as well.
- Logging: everything also goes to `<run>/coopnet-game.log`. Added:
  - unknown host names with counts every ~20 s;
  - carry-mirror misses;
  - puppet yaw and distance;
  - per terrain edit, the ground before and after.
- Test switch `-RenameClientItems` (`HYDRO_COOP_TEST_RENAME`): the client remakes its starter items
  under new names, the case two real PCs have.

### Test (two copies, PlayMode, mocksteam, `-RenameClientItems`, under the guard)

- Pairing: 6 paired, 0 cm apart.
- Terrain edits: dig 1 `RemoveSphere`, ground 160.2 → 145.7 on both (diff 0.0 cm); dig 2
  `AddSphere`, 167.2 → 194.5 on both (diff 0.0).
- The client made no local dig calls of its own.
- Puppet yaw matched the host's (−69 / −69).
- Mirror: pickups 2, drops 1, dirt 2.
- Owner's real data unchanged.

### Not tested

- Host P1 digging in the harness. It goes through the same Voxel hook, so W is player-independent
  by construction, but no P1 dig ran.
- A real Linux session.

## Third real session: bucket water, own dig animation (2026-10-22)

### Observed (the friend's coopnet-game.log)

- Pairing worked: 10 starter items, 0 cm apart.
- Terrain edits matched within 0–2.4 cm (one 10 cm).
- Each dig compare was logged 3 times, because Z lines are sent 3 times; deduped now.
- "Not found here" kept reporting counts from before pairing; it now resets after each report.

### Derived

- Bucket water: the bucket's state lives in fields (`HasLiquidType?`, `DirtLoads`, `IceLoads`,
  `Vein`) and in mesh visibility (`WaterMesh`, `DirtMesh`). Only the shovel's `Dirt` mesh was
  mirrored before.
- Own dig animation: an item's swing is `BP_ParentItem_C:PlayPrimaryUseAnim` /
  `PlaySecondaryUseAnim` (no args), and the effects are `BP_ParentShovel_C:DirtFX(bool)`. The
  client's dig only runs on the host, so none of these ran on the client.

### Built

- Generic item state: per class, reflection (`ForEachProperty` up to BP_ParentItem_C) finds the
  int, byte, enum, bool and float fields and the mesh components. Carry, hover, proxy and physics
  names are skipped.
  - The host sends `T|name|state` on a change, and again in the full resend once changed.
  - The client applies it to its copy, and to its held proxy when it carries the item.
- Use animations: the host hooks PlayPrimary/SecondaryUseAnim and DirtFX and sends
  `Y|id|@P1/@P2/name|fn|arg`. The client plays the same function on its item (its proxy if held).
- Action trace on both sides: an allow-list of action-like function names on BP_GameController_C,
  AC_Animator_C and the class chain of every carried item, with simple args. The first 12 calls of
  each are logged, then every 50th, plus a 30 s summary.
- Two crashes on the way, both access violations inside UE4SS (at 0x10):
  - Hooking all 104 controller functions (per-frame traces, highlighting, HitResult out params).
  - Calling methods on null object properties during reflection: pcall cannot catch a native
    access violation. Now only "Mesh"/"Dirt" object fields are considered, after an IsValid check.

### Test (two copies, PlayMode, mocksteam, -RenameClientItems, guard)

- Animation: host P2 dig → `DirtFX` and `PlayPrimaryUseAnim` on @P2; the client played both on
  its shovel.
- Bucket test: the host set `HasLiquidType?` 1, then `EmptyLiquid()`; the client mirrored 1 and 0.
  But `HasWater()` called directly did not show `WaterMesh`: how the game really shows water is
  not known yet.
- Digs: diff 0.0 cm.
- Host tick: avg 1.18 ms, max 8 ms.
- Owner's real data unchanged.

## Fourth real session: no animations or sounds on the friend's side (2026-10-22)

### Observed

- Host log: it sent 49 use-animation/DirtFX lines (30 for @P2).
- Friend log: 12+ were played, none failed. But his `AC_Animator_C:PlayAnimation` count was 0.
  The host's trace shows `PlayPrimaryUseAnim` → `AC_Animator:PlayAnimation(0.5, StaticMesh,
  C_Mov_ShovelDig, C_Rot_ShovelDig, false)`.
- Derived: on the client, PlayPrimaryUseAnim's blueprint never gets to PlayAnimation. A guess
  (unverified): it checks for a real local carrier.

### Fix

- The host hooks `AC_Animator_C:PlayAnimation` and sends seconds, mesh component name and curve
  asset paths. The client calls PlayAnimation on its item and its held proxy.
- Sounds: the host hooks five GameplayStatics functions: PlaySoundAtLocation,
  SpawnSoundAtLocation, PlaySound2D, SpawnSound2D and SpawnSoundAttached.
  - A sound is attributed to a player by its context object: controller, carried item or proxy,
    or P1's pawn. Failing that, by a world action within 1.2 s and 10 m.
  - Sent as `U` lines; the client plays them, with 2D sounds only for P2.
  - P2's own pawn sounds are skipped, and so are footsteps, which come from the walk animation and
    play on the puppet too.
  - The same sound at the same 50 cm cell again within 0.4 s is dropped: a shovel's bounces gave
    20+ S_DropMetalNormal.

### Test (guard)

- The client ran PlayAnimation for the dig and empty curves.
- 12 sounds sent and 11 played (the one skipped was the host's own 2D sound):
  - S_ShovelDig and S_EmptyDirt (2D);
  - S_Pickup ×3, S_ThrowItem;
  - S_DropMetalNormal ×2, S_EnterWater2, S_Hrrm ×3.
- Digs diff 0.0. Owner's data unchanged.
- Not checkable here: whether the swing is visible on screen and the sounds are audible
  (the bot can't see or hear).

## Fix 5, and a guard lesson (2026-10-22)

### Fix 5

- His side still had no animations or sounds. The host log shows 16 sounds and 25 animation lines
  sent for P2, so the problem is on the client side: an older fix installed, or playback failing
  under Proton. Unknown until his log arrives.
- Version check: `MOD_VERSION`. The client sends a `V` event; the host's S line carries the
  version. Each side shows a status line on a mismatch, and the host warns when a client sends no
  version (older than fix 5).
- Sounds:
  - P2's pawn sounds now count for P2, except jump, landing and footsteps (SC_EmptyWater had been
    skipped).
  - An item a player just dropped counts as theirs for 5 s, which covers landing sounds of thrown
    items.
- Test: versions matched (fix5/fix5); 12 sounds sent, 11 played; digs 0.0 cm.

### Guard lesson

- During that test the owner's REAL game was running. It autosaved its world 9 s after the guard's
  snapshot, and the guard reported 5 changed save files.
- It had no copy of those files, so it left them alone. With a copy, it would have "restored" them
  and overwritten the owner's newest progress.
- `run-coop-proto.ps1` now refuses to start while a Hydroneer runs outside `local\test-install`.
  Worth a playbook note: a guard must not run while the owner plays.

## Save transfer / late join, step 1: how the game saves and loads (2026-10-22)

Probe mod `SaveProbe` (test copy only, `HYDRO_SAVEPROBE=1`, saves redirected), under the guard.
Hooks on the EasyMultiSave natives, OpenLevel, and the blueprint save, load and menu functions.

### Observed

- New game: `SpawnInitialItems`, `GamestateNewGame`, then `EMSAsyncSaveGame:AsyncSaveActors(GM, 3)`.
- Save: `BP_GameController:Autosave()` calls `AsyncSaveActors(controller, 3)`, then `SaveToCloud`.
  The slot is EasyMultiSave's current one (default `MySaveGame`);
  `EMSFunctionLibrary:GetSlotInfoSaveGame(ctx, out name)` reads it.
- Back to the menu: `GameplayStatics:OpenLevel(ctx, "L_GameWorld", true, "")`. Then
  `GM_Hydroneer.InGame?` is false; it is true in a world. The menu has a player pawn too, so a pawn
  is no sign of a world.
- Load, the save manager's way:
  1. `SetCurrentSaveGameName(slot)`.
  2. The main menu's Continue click handler
     (`BndEvt__UI_MainMenu_ContinueButton_1_…`) opens `UI_Menu_SaveManager_C`.
  3. On that widget, `SaveSelected(slot)` (which calls `AsyncLoadActors(…, 1, true)` as a
     preview), then `ContinueMainGame()`.
  4. The game mode then runs `SetCurrentSaveGameName`, `DoesSaveSlotExist`,
     `IsSaveSlotCorrupted` and `AsyncLoadActors(GM, 3, true)`, and the world is up with
     `InGame?` true.
- After the load: 557 items. 555 kept their names, including all 8 starter tools with game-made
  names; only the two vehicles (drill, harvester) got new names.
- All 8 starter tools moved 3 m before the save came back at 0 cm.
- A level-placed shovel (`BP_ShovelT15`) moved before the save came back at its old spot (364 cm
  away). Guess: level shop/display items are not saved.
- Owner's real data unchanged in all four runs.

### Derived

- A client that loads the host's save gets the host's item names, so most items match by name
  without pairing. Vehicles (new names on load) still need pairing.

## Late join, step 2: built and tested (CoopNet 0.3, 2026-10-23)

### How it works

1. The friend presses F7 at the main menu (`InGame?` false). The client sends `J,late` and its
   version in its line format every 2 s.
2. The host:
   - reads its slot (`GetSlotInfoSaveGame`) and calls `Autosave()`;
   - waits for `IsSavingOrLoading` to clear (3 s minimum);
   - copies `Level.sav`, `Player.sav` and `SaveData.sav` to `run/save_out/` and writes
     `save_out.ready`;
   - remembers the last terrain edit id.
3. The relays move the files on a bulk channel:
   - 48 KB chunks, 8 in flight, acks, resend after 1 s, SHA-256 per file;
   - Steam: channel 2 reliable, with SendBufferSize (9), SendRateMin (10) and SendRateMax (11)
     raised; the enum values were read from Steamworks.NET's metadata;
   - the save does not compress (zlib 98.5%: EasyMultiSave already compresses it).
4. The client copies the files into its `CoopNetHost` slot (`GetProjectSavedDirectory`,
   `mkdir` via `os.execute`) and loads it the save manager's way. Then the normal client starts
   and sends `L,loaded`; the host repeats the terrain edits after the mark.
5. `N` lines carry the host's FName. A late joiner maps the net id onto the item it already has
   from the save instead of spawning a copy.

### Bugs found by the test

- **Input from every character.** The pawn input hooks fired for the client's puppet too. Its
  per-frame jump/crouch events pushed "version" and "loaded" out of the 8-event window before they
  were ever sent, and its axis events mixed into the friend's movement.
  - Now only events whose pawn's full name is ours count.
  - A first try compared `GetAddress()`; it failed inside pcall and silently dropped ALL own input
    (the client's walk measured 52 cm/s).
  - One-off events stay in every line for ~10 s ("sticky").
- **Lua `RemoveSphere` is not saved; the game's own dig is.** A test hole made with a Lua
  `RemoveSphere` was missing after the load (11.6 cm). A real P1 shovel dig (`MoveDirt`) was there
  (0.0 cm). Guess: the game saves its own dig records, not raw voxel data.
- **The in-world check.** F7's check used "has a pawn", which the main menu also has. It now uses
  `InGame?`.

### Tests (two copies, PlayMode, mocksteam, `-LateJoin`, guard)

- Relay only, with the real 21.9 MB save:
  - mocksteam and udp: 3.3 s, byte-identical;
  - with 10% of bulk messages dropped (`--mock-drop 0.1`): 16.7–19.8 s, byte-identical;
  - the same with the package's own Python 3.12.10.
- Late join, last run:
  - before the join the host made 8 runtime items and one real dig (158.7 → 147.1);
  - the client's ground at that spot was 147.1 (diff 0.0);
  - 572/572 items match (max 7.3 cm, a shovel just dropped);
  - later digs: 0.0 cm; the bot walk: 405 cm/s.
- Normal co-op (`-RenameClientItems`), no regression: 6/6 paired, 572/572 within 0.0 cm, digs 0.0.
- Owner's real data unchanged in every run.

### Not tested

- A real two-PC session.
- Linux/Proton: does `os.execute("mkdir")` work there, and is the save path under Proton right?
- A played-in save with many items and machines.
