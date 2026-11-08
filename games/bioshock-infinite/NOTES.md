# BioShock Infinite (2013): working notes

Path: build from zero, but on top of netcode the game still ships (see triage below).
Owner's tooling, which this work builds on: a private modding kit with a UE3 package reader
(`upk.py`) and an in-game bridge, `bi_sonde` (a `d3d11.dll` proxy with a named-pipe JSON driver).

## Identity

| | |
|---|---|
| Store | Steam, app 8870, build id 10404041 (2K Launcher removed) |
| Exe | `Binaries\Win32\BioShockInfinite.exe`, 32-bit, 18,368,840 bytes |
| SHA-256 | `C20A42529C4181B1D10EED416D9847A823F9453DD267CA478EFB3A10014A0E77` |
| Engine | Unreal Engine 3, licensee build (package version 727 / licensee 75), D3D11, PhysX 3.1, Wwise, Morpheme animation |
| OS | Windows 11 |

## Triage (2026-10-30)

### Observed (checks that actually ran)

- Not in `DIRECTORY.md` or `games/`. No issue or PR mentions BioShock Infinite (`gh issue/pr list --search`).
- No co-op mod or multiplayer attempt for Infinite was found on the web. Two official modes,
  "Spec Ops" (up to four players in co-op on stages from the campaign, with loot and progression)
  and "Border Control" (tower defence), were cut in 2012
  ([Shacknews](https://shacknews.com/article/83435/bioshock-infinites-scrapped-multiplayer-detailed),
  [Co-Optimus](https://www.co-optimus.com/article/8743/bioshock-infinite-loses-intended-co-op-modes-in-mid-development.html)).
- The shipped config still selects a network driver:
  - `XGame\Config\DefaultEngineSteamworks.ini`: `NetworkDevice=OnlineSubsystemSteamworks.IpNetDriverSteamworks`.
  - `Engine\Config\BaseEngine.ini`: `NetworkDevice=IpDrv.TcpNetDriver`, `[URL] Port=7777`.
  - `XGame\Config\DefaultEngine.ini` `[IpDrv.TcpNetDriver]` has developer comments that tune
    `InitialConnectTimeout` down to 45 s, so the netcode was in use during development.
- The exe contains the strings `TcpNetDriver`, `UNetConnection`, `NetPendingLevel` and `ServerTravel`.
  `IpDrv.xxx` and `OnlineSubsystemSteamworks.xxx` ship in `CookedPCConsole_FR`.
- Script RPCs in the cooked packages (names of `Function` exports starting with `Server`/`Client`,
  read with the owner's `upk.py`):
  - `Engine.xxx`: 88, the stock UE3 set (`PlayerController.ServerMove`, `ClientAdjustPosition`...).
  - `XCore.xxx` (Irrational's game code): 112, among them
    - movement and skylines: `ServerStartSprint`, `ServerJumpOffTBar`, `ServerPerformGroundToTBarTransfer` and 8 more TBar moves;
    - combat: `XWeapon.ServerStartFire/StopFire/StartReload`, `XInventoryManager.ServerNextWeapon/NextPlasmid`,
      `XWeaponCharge.ServerChargeAtTarget`, `XWeaponUndertow.ServerPerformUndertowFire`, `XWeaponSlave.ServerCreateTurret`;
    - loot and economy: `ServerTransactLootItem`, `ServerAcceptLootFromContainer`, `ClientAcquireLootItem`, `ClientAdjustCurrency`, `ServerSetVotedToSkipLootRoll`;
    - party and sessions: `ServerSpawnAs`, `ServerDisbandParty`, `ClientReplacePartyHost`, `ClientJoinSession`,
      `ClientTravelToJoinedSession`, `XPlayerReplicationInfo.ServerSetIsReadyToStartGame`;
    - world: `ServerUpdateLevelVisibility` (level streaming), `ServerCauseEvent`, `ServerRemoteEvent` (Kismet).
  - `XCore.xxx` classes: `XGameReplicationInfo`, `XPlayerReplicationInfo`, `XOnlineGameSettings`,
    `XOnlineGameSearch`, `XResurrectionSpawnPoint`, `XSeqAct_SpawnPlayer`, `XSeqAct_TravelBackToMPLobby`.
- The owner's bridge (`bi_sonde`) already reads GNames/GObjects, calls any UFunction through
  `ProcessEvent`, reads and writes properties, freezes and dilates time, hides focus loss (no pause
  menu in the background) and starts a new game with `XGameViewportClient.StartTravel("Master_P", 0, None)`.
  The native console commands (`OPEN`, `SLOMO`, `PAUSE`) are gone from this build.

### Derived

- This is the "engine has MP plus script" row of `playbook/triage.md`, not "closed binary, no MP":
  UE3 replication and Irrational's own co-op RPCs (from the cut Spec Ops mode) are in the shipped
  game. Co-op means getting a listen server and a client to connect, then making the single-player
  story logic (Kismet, Elizabeth, streaming, saves) run on that netcode.
- The playbook's in-game bridge (`playbook/in-game-bridge.md`) already exists as `bi_sonde`:
  main-thread queue via a `PeekMessageW` hook, named pipe, JSON replies.

## Runtime check (2026-10-30, live game through `bi_sonde`)

Launched windowed 1280x720 with `-nomovie`, at below-normal priority, to the title world `XEntry_P`, then
`appeler vue StartTravel "Master_P?listen" 0 None`. Game closed afterwards.

### Observed

- Title world: `WorldInfo.NetMode` = 0 (standalone). No live NetDriver, NetConnection or channel
  instances. `Transient.OnlineSubsystemSteamworks_0` is live.
- Of the 3,103 live `Class` objects, these networking classes exist: `Engine.NetDriver`, `NetConnection`,
  `ChildConnection`, `Channel`, `ControlChannel`, `FileChannel`, `VoiceChannel`, `ChannelDownload`,
  `PendingLevel`, `NetPendingLevel`, `IpDrv.TcpNetDriver`, `TcpipConnection`,
  `OnlineSubsystemSteamworks.IpNetDriverSteamworks`, `IpNetConnectionSteamworks`, `XCore.XNetPendingLevel`,
  `XCore.XUdpBeacon`, `XGameReplicationInfo`, `XPlayerReplicationInfo`.
- **No `ActorChannel` class is live, and the exe has no `ActorChannel` string** (ASCII or UTF-16),
  while `ControlChannel`, `FileChannel` and `VoiceChannel` each appear once in ASCII.
- **`?listen` works.** After `StartTravel("Master_P?listen")` a new game starts and
  - `WorldInfo.NetMode` = 2 (listen server);
  - `Transient.IpNetDriverSteamworks_0` is live;
  - the game process binds UDP `0.0.0.0:7777` and `0.0.0.0:27015` (`netstat -ano -p UDP`);
  - the game type is `XGameInfoSingleplayer_0`, with `XGameReplicationInfo_1` and one `XPlayerReplicationInfo_2`.

### Derived

- The server half of UE3 networking starts in the shipped game with no patching: one URL option
  through the bridge. This is the "engine has MP" path.
- The missing actor channel is the main risk. In stock UE3 every replicated actor, the client's own
  PlayerController included, goes through `UActorChannel`. If it was compiled out, a client may get
  through the handshake (control channel) and then see nothing, and actor replication would have to
  be rebuilt in a DLL. Irrational could also have renamed or replaced it. Only a client connection or
  Ghidra work on `UNetDriver::TickFlush` / the `UChannel::ChannelClasses` table will tell.

## Static check: where is the actor channel? (2026-10-30, Ghidra 12.2, owner's analyzed project, read-only)

Addresses are for the Steam exe above (fixed base 0x400000).

### Observed

- `FUN_00606680` registers the Engine package's native classes. Its channel classes are `UChannel`
  (`FUN_00792160`), `UControlChannel` (`FUN_007b86f0`), `UFileChannel` (`FUN_007921d0`) and
  `UVoiceChannel` (`FUN_007a7530`), in that order. Every class registration (`FUN_008b9ff0`) that names
  `FUN_00792160` as its parent is one of those three. There is no `UActorChannel` registration.
- `UChannel::ChannelClasses[8]` is at `0x013b76bc`. The class static constructors write slot 1 (`0x013b76c0`,
  Control, sets `ChType` +0x3C = 1), slot 3 (`0x013b76c8`, File, 3) and slot 4 (`0x013b76cc`, Voice, 4).
  **Slot 2 (`0x013b76c4`, `CHTYPE_Actor`) has no references at all.** Slot 5 (`0x013b76d0`) has none either.
- The table is read in two places only:
  - `FUN_0078eb70`, `IsKnownChannelType`: type < 8 and slot not null;
  - `FUN_00799740`, `UNetConnection::CreateChannel`: picks channel index 0 for type 1, 1 for type 4,
    2 for type **5** (not a stock UE3 type), 3 and up otherwise; then constructs `ChannelClasses[type]`.
- `CreateChannel` has three callers:
  - `FUN_007b5f40`, the packet receive path: it creates a channel for an incoming bunch only if
    `IsKnownChannelType` passes, so actor bunches would be dropped;
  - `FUN_0084a340`, connection setup: `CreateChannel(1, 1, 0)`, the control channel;
  - `FUN_007a0bb0`: `CreateChannel(4, 1, 1)`, the voice channel.

### Derived

- **Native actor replication is compiled out of the shipped exe.** The transport survives: net driver,
  connections, packets, control channel, voice and file channels. But no code creates an actor channel
  on the server, and a client would drop actor bunches.
- Script RPCs (`ServerMove`, `ServerStartFire`, the 112 in `XCore`) travel over actor channels in UE3,
  so none of them can reach the other machine as shipped. They remain useful as named gameplay entry
  points to call locally through the bridge (`ProcessEvent`).
- The script side of replication is probably intact: dozens of `*execReplicatedEvent` /
  `*execXReplicatedEvent` natives, `ReplicatedAttachData`, `DevTBarReplication`. That fits a game whose
  multiplayer build was cut late. It is unknown whether property `RepOffset`/`CPF_Net` data and the
  replication condition bytecode survived cooking.

### Options

1. **Own sync layer (playbook default, host authority with state sync).** Each copy runs its own
   single-player world. A DLL (bi_sonde's reflection and game-thread queue) sends snapshots over Steam
   relay (`playbook/networking-steam-spacewar.md`). The partner's Booker is a puppet pawn on each side;
   inputs and events are re-played locally through the game's own functions. This needs no UE3
   replication, and the engine's transport is not used.
2. **Rebuild `UActorChannel` in a DLL** and register it into slot 2, using the surviving transport and
   script replication metadata. It is closer to the real engine and would reuse Irrational's RPCs, but
   it means rewriting UE3's replicator (`ReplicateActor`, `ReceivedBunch`, `ServerTickClients`), which
   the exe no longer has. Only viable if `CPF_Net` / `RepOffset` data survived. Check that first.

## Static check: did the script replication data survive? (2026-10-30, cooked packages, offline)

Read every `*Property` export in `Engine.xxx`, `XCore.xxx` and `Core.xxx` with the owner's `upk.py`.

### Observed

- Cooked UProperty layout in this build: `NetIndex` (int32), `None` terminator (FName), `Next` (int32),
  `ArrayDim` (int32) at +0x10, `PropertyFlags` (uint64) at +0x14, `Category` (FName) at +0x1C,
  `ArrayEnum` (int32) at +0x24, then the subclass fields (struct, class, inner property...). The
  `RepOffset` (uint16) would follow `ArrayEnum` when `CPF_Net` (0x20) is set.
- **`CPF_Net` is set on 0 of 36,042 properties** (Engine 11,422, XCore 23,855, Core 765). Examples that
  replicate in stock UE3: `Actor.RemoteRole` has flags 0, `Actor.Velocity` 0x2000, `Actor.bHidden` 0x3.
- Every property's size matches the layout with no `RepOffset`; there are no mismatches (one export too short to read).
- The other flags look stock (0x80 `Parm` 3,783 times in Engine, 0x400 `ReturnParm` 687 times).
- **`CPF_RepNotify` (0x100000000) survived**: 31 properties in Engine and 131 in XCore. Some XCore examples:
  - `XPawn` (26): `ReplicatedTBarAttachData`, `TBarToGroundData`, `CurrentLHWeapon`/`CurrentRHWeapon`,
    `LastTakeHitInfo`, `KillingBlowHitInfo`, `PawnResurrectedByOtherPlayerTrigger`, `StartedRescuingOtherPlayerTrigger`;
  - `XWeapon` (11): `ReplicatedTraceFireInfo`, `ReplicatedNonTraceFireInfo`, `AmmoCount`, `ReplicatedWeaponEquipState`;
  - `XReactiveActor` (10), `XGameReplicationInfo` (10), `XPlayerController` (6), `XDoor.ServerState`,
    `XTurret`, `XSecurityCamera`, the shields, the projectiles;
  - `XFloatingSection`: `ReplicatedTBarAttachData`, `ReplicatedTotalTBarSplineDistance`.

### Derived

- The script replication metadata is gone too. Without `CPF_Net` and `RepOffset`, the engine cannot
  know which properties to send, and the replication conditions in the class bytecode (which
  `RepOffset` points to) can no longer be found. **Option 2 (rebuilding `UActorChannel`) would also mean
  rebuilding every replication list by hand.** It stays a backup only.
- The surviving `RepNotify` list is a map of what Irrational synchronised in the cut co-op mode, including
  revives (`PawnResurrectedByOtherPlayerTrigger`). It is a ready-made checklist for option 1's snapshots:
  each of these properties has a `ReplicatedEvent` handler in script that plays the effect on the
  receiving side, and the bridge can call that handler after it writes the value.

## Two copies on one PC (2026-10-30)

Owner's co-op repo: a separate local repo (not public yet). It builds bi_sonde with a forced-include header so
that `-coopinstance=<i>` gives each copy its own pipe `\\.\pipe\bi_sonde_<i>`.

### Observed

- `tools/gamepool` `provision --n 2` built two copies in seconds, each with 11,347 hard links. Private copies:
  `d3d11.dll`, the bi_sonde files, `8870_install.vdf` and both `GlobalShaderCache-PC-D3D-SM*.bin`.
- Arguments: `-nomovie -windowed -ResX=960 -ResY=540 -NOHOMEDIR -AllowMultipleInstances -coopinstance={i}`.
  - `-AllowMultipleInstances` is a switch the start-up code reads (`FUN_00506e40`, before the
    `AnotherInstanceRunning` message), so no patch is needed.
  - `-NOHOMEDIR` (`FUN_004d6aa0`) makes the copy create its `X*.ini` in its own `XGame\Config`.
- Both copies ran together, answered on their own pipes, and each started a new game
  (`StartTravel Master_P` through the bridge, the modal screens closed by `bi_pilote.fermer_modales`).
- SHA-256 of the install files (exe, `d3d11.dll`, `DefaultEngine.ini`, `XCore.xxx`, shader caches) was the same before and after.
- **But the owner's `Documents\My Games\BioShock Infinite\XGame\Config\XEngine.ini` changed.** It was rewritten
  two seconds after copy 1 wrote its own config. Its `XUserOptions.ini` was rewritten in the same second with
  identical content. The owner's own resolution and vsync settings are still there; what changed is unknown (no
  earlier copy of the file).

### File guard (second run, same day)

- A guard DLL logs and refuses every write under `\My Games\BioShock Infinite` from the game process. It hooks the
  KERNEL32 file functions (`CreateFileW/A`, `DeleteFileW`, `MoveFileW/ExW`, `CopyFileW`, `SetFileAttributesW`,
  `CreateDirectoryW`) in the import tables of every loaded module. The owner's files were also read-only during the run.
- **`d3d11.dll` loads too late for this.** The copy wrote its `XEngine.ini` at +1.1 s, while `d3d11.dll` (bi_sonde) loaded
  at +1.7 s. The guard therefore lives in a `dinput8.dll` proxy: `dinput8.dll` is a direct import of the exe, not a
  KnownDLL, so it loads before the game's start-up code.
- With the guard, two fresh copies (empty config) started side by side. Every config write
  (`..\..\XGame\Config\X*.ini`, write access, `CREATE_ALWAYS`) went into the copy. There were no refusals, and the owner's
  folder was unchanged (same hashes).
- The writer of the first incident (the owner's `XEngine.ini` rewrite) was not reproduced. It stays unknown, but the
  game process can no longer write there.
- The guard's stack capture (`RtlCaptureStackBackTrace`) returns no frames inside the exe, probably because it is built without frame pointers.

### Saves go to Steam Cloud, not to files

- `Steam\userdata\<account>\8870\remotecache.vdf` lists the game's cloud files: `profile.bin` and
  `SaveData/S_<map>-0_<date>.sav/.bkm`. They are stored in `Steam\userdata\<account>\8870\remote`.
- The first two-copy run (new game in both copies) **wrote a save and the owner's `profile.bin` into that
  folder.** `-NOHOMEDIR` does not cover saves, and the guard cannot see the writes: the Steam client process writes them.
- **Rule: a test copy must not start a game (only the title world) until saves are isolated.** Candidates: a per-copy
  `steam_appid.txt` with 480 (Spacewar) so remote storage goes to another app, if the game still starts; or hooking
  `ISteamRemoteStorage` in the guard.
- Tried: a per-copy `steam_appid.txt` with 480. **Refused by the exe's Steam DRM**: an "Application load error
  V:0000065432" box, then the copy exits. The copies keep 8870 (a private file, not a link).
- The exe gets its Steam interfaces through the modern flat API (`SteamInternal_FindOrCreateUserInterface`,
  `SteamInternal_ContextInit`, `SteamInternal_CreateInterface`) and asks for `STEAMREMOTESTORAGE_INTERFACE_VERSION014`
  (also `SteamUser020`, `SteamFriends017`, `SteamUtils009`, `STEAMAPPS_INTERFACE_VERSION008`,
  `STEAMUSERSTATS_INTERFACE_VERSION011`).

### Saves isolated: wrapped ISteamRemoteStorage (works)

- The guard DLL (`dinput8.dll`) hooks `SteamInternal_FindOrCreateUserInterface` and `SteamInternal_CreateInterface` in
  the exe's import table. When the game asks for `STEAMREMOTESTORAGE_INTERFACE_VERSION014`, it gets a wrapper:
  - vtable entries 0 to 23 (`FileWrite` ... `SetCloudEnabledForApp`) are local and work on `<copy>\SaveCloud\`;
    they never call Steam;
  - entries 24 and up (UGC, Workshop) jump to the real interface through naked x86 thunks (`mov ecx,[ecx+4]; jmp [vtbl+i*4]`).
- The vtable order (SDK `isteamremotestorage.h`, v014) was checked with a read-only self-test on the real interface:
  `GetFileCount` (18) matched the number of files in the owner's cloud folder. `GetFileNameAndSize` (19) gave
  `profile.bin` and `SaveData/...` names with their sizes, and `GetFileSize` (15) and `FileExists` (13) agreed with them.
- New game in copy 0: the game called `GetFileSize profile.bin` (0, empty cloud), then
  `FileWrite profile.bin` (3518 bytes) and `FileWrite SaveData\S_Light_P-0_<date>.sav/.bkm` (105421 bytes each). It used
  no async or stream calls. All three files are in the copy's `SaveCloud`.
- The owner's `Steam\userdata\<account>\8870` folder was hashed before the run: it was identical after the
  new game and after closing the copy. `My Games` and the install were identical too.
- A copy starts with an empty cloud: a new player profile, no saves. A known save can be seeded by putting files in `SaveCloud`.
- The fake `GetQuota` must be large: with 100 MB total and 90 MB free, the game shows a "limited storage device" box (it
  wants 28 MB more for chapter saves). It now reports 1 GiB.

## Milestone 1: the other player's Booker as a puppet (2026-10-30)

Both copies in a new game, in the opening boat (`Master_P`, floating section 1). Everything goes through the two
bridges (`\\.\pipe\bi_sonde_0` and `_1`) from one Python process; there is no network yet.

### Observed

- Player pawn: `XCore.XHuman`, archetype `Arc_XPlayer.XPlayerPawn`.
- **`Actor.Location` and `Rotation` are not reflected properties in this build.** Position goes through Irrational's
  floating-section API on `Actor`: `XGetWorldLocation`, `XGetFloatingLocation` (returns `{LocalVector, SectionIndex}`),
  `XGetFloatingRotation`, `GetFloatingSectionIndex`, `MoveToFloatingSection`, and the properties
  `XPrivateLocalLocation`, `XPrivateLocalRotation`, `XFloatingSectionIndex`.
- `Actor.Spawn` takes one more parameter than stock UE3: `SpawnClass, SpawnOwner, SpawnTag, SpawnLocation,
  SpawnRotation, ActorTemplate, bNoCollisionFail, FloatingSectionIndex`.
  `appeler pc Spawn XCore.XHuman None None <x,y,z> <p,y,r> Arc_XPlayer.XPlayerPawn 1 1` returned a new `XHuman`
  in the boat.
- The archetype's mesh is the first-person body: arms and legs only. The full third-person Booker is
  `Booker.Booker_MESH` (packaged in `S_Light_Capsule.xxx`, already loaded in `Master_P`).
  `SetSkeletalMesh Booker.Booker_MESH` on the puppet's mesh component gives a complete Booker in an idle pose.
  Another candidate for later levels: `Tear_Prefabs.Meshes.DecoyTear_Booker` (Elizabeth's decoy tear).
- `XSetLocalLocationAndMoveActor`, `XSetLocalRotationAndMoveActor` and `Move` all return `False` from the bridge, even with
  the puppet's collision off; `SetPhysics 0` does not stick. **Writing `XPrivateLocalLocation` / `XPrivateLocalRotation`,
  then calling `ForceUpdateComponents`, moves and turns the actor**, on the puppet and on the real player.
- Exit test (`outils/marionnette.py cercle 1 0` in the owner's co-op repo): the copy 1 player is moved around a 60-unit
  circle, two turns of 24 steps, turning as it goes. After each step, the copy 0 puppet is placed from copy 1's pose
  and read back. Result: `{"pas": 48, "ecart_position_max": 0.0, "ecart_lacet_max": 0, "memes_sections": true,
  "source_suit_le_cercle_max": 0.001, "resultat": "OK"}`.
- Two frame captures of copy 0 from a fixed camera show the puppet at the two poses sent from copy 1.
- **Rate: 2.3 steps per second in the test, about 5 updates per second in plain follow mode.** Each bridge command
  waits for one game frame (the commands run in the `PeekMessageW` hook), and one update takes 5 commands.

### Derived

- The puppet path works with the game's own classes and meshes; no new asset is needed.
- Copying local pose plus section index makes the puppet ride the same floating section (the boat, later the islands)
  in the other copy, even when the two copies' sections are at different points of their motion.
- The bridge is fine for tests, but too slow for play. The real sync must run inside the DLL, every frame, with the
  transport there too (`playbook/networking-steam-spacewar.md`).

### Not done yet

- No animation: the puppet slides in its idle pose. It needs the walk/run state from the source pawn (Morpheme network inputs).
- No walking test: the copy 1 player was moved by writing its pose, not by input. In the boat the player has no control.
- The puppet's own physics was not turned off (`Physics` stays 1), and it has no controller. It did not fall in the boat; this is not checked on open ground.

## Milestone 2: sync inside the DLL, two transports, player package (2026-10-30, night)

The co-op module is in the d3d11.dll proxy. It includes bi_sonde's `pilote.cpp` in its own translation unit,
to reuse the UE3 reflection, and replaces `Pilote::TickJeu`: the original tick, then the co-op tick on the game thread.

### Observed

- 30 pose packets per second each way. The puppet is placed up to 120 times per second, interpolated between the
  last two packets with a 60 ms delay.
  - Circle test over loopback UDP: mean error 5.5 to 7 units while moving, 0.0 after stopping, same section;
    both copies still alive 60 s later.
- **Crash 1 (Wwise "Fatal error", fake stack).** Calling `ForceUpdateComponents` through `ProcessEvent` with
  zeroed parameters: optional-parameter defaults only apply to script calls, so `bTransformOnly` was False.
  At 470 calls per second the game crashed after about a minute. Passing `bTransformOnly = True` fixed it.
- **Crash 2 (same dialog).** A vectored exception handler in the test guard DLL logged a write through a null
  pointer at `0x00bd35b4` (`FUN_00bd35b0`, called from `FUN_00bda3f0` and `FUN_009735f0`) on the game thread.
  It happened with a second `XHuman` pawn without a controller, idle or moving, with the `XPlayerPawn` or the
  `XBaseHuman` archetype. The game's own crash dialog names `AK::MemoryMgr::SetMonitoring`; that is only the
  nearest exported symbol.
- The puppet is now an actor with a mesh, not a pawn:
  - `Engine.SkeletalMeshActorSpawnable` was stable, but Booker was blue and flat;
  - `XCore.XReactiveSkeletalMeshActor` is lit like a character, because its `XSkeletalMeshComponent` takes the
    irradiance volumes. This build has no `LightEnvironment` property.
  Both are in the reference pose, arms out.
- Transports, in `coop.ini` next to the DLL:
  - `role=hote` / `role=client` with `hote=<name:port>` over UDP. The host learns the client's address from its
    packets, so only the host must be reachable. Tested on loopback, same results.
  - `role=steam` with `ami=<SteamID64>`: Steam P2P through the flat API exported by the game's `steam_api.dll`
    (`SteamAPI_SteamNetworking_v006`, `SteamAPI_ISteamNetworking_SendP2PPacket` / `ReadP2PPacket` /
    `AcceptP2PSessionWithUser`). Tested with both copies on the same account and `ami` = own SteamID:
    Steam still delivered between the two processes, and each process also read some of its own packets,
    which it rejected. Circle test OK (mean 6.0 units). **Not tested with two accounts.**
- The puppet is hidden when the partner has been silent for 2 s, is in another world, or is in a floating section
  that does not exist locally. `Master_P` holds the whole campaign, so the same world does not mean the same place.
  Checked by killing copy 1: copy 0 hid the puppet and kept running.
- Player package (`outils/paquet.ps1`):
  - `d3d11.dll` built with `COOP_JOUEUR`, so the command pipe is never created;
  - `installer.ps1` checks the exe SHA-256 and that the game is closed, moves a foreign `d3d11.dll` aside,
    writes `coop.ini`, and prints the player's SteamID64;
  - `desinstaller.ps1` restores the original DLL;
  - `LISEZMOI.txt`.
  Tested on test copies: install, reinstall and uninstall restore the original DLL byte for byte. The player build
  started as host and as client with no pipe present. A game cannot be started without the pipe, so only the
  title screen was checked with it.
- Side note: saves and the owner's `XEngine.ini` changed later that night because of another session of the owner
  that ran the real game through `bi_pilote` (windowed). The test copies' saves were all in their own `SaveCloud`.
  The first `XEngine.ini` incident may have the same cause (not checked).
- Puppet class at the time: `XReactiveSkeletalMeshActor`. Since packet v17 (2026-11-02) it is an
  `XProtectionObjectiveActor`, which AI can target; the old class is the fallback (see "Host-authoritative enemies").
  The cause of crash 2 was found then: `XPawn.ActionModel` is never created with a player archetype.

### Not done yet (as of that night; most of it is done since, see the sections below)

- Animation (Morpheme): **solved for standing still, not for walking.** An actor spawned with an archetype of the
  same class inherits that archetype's mesh component, including its Morpheme anim tree, and `Booker.Booker_MESH`
  plays the anim trees of the game's male characters. Archetype `Arc_ActorBase.XIdleActor` (`XReactiveSkeletalMeshActor`,
  tree `GenMale_AnimTrees.Idle_Base_AT`, placeholder mesh `Shared_Textures.CharacterWrong`) gives a standing idle with
  the arms down. `Arc_SLighthouse_doNotPlace.Lutece_Male` gives the boat-rowing animation, oars included. There is a
  dark patch on Booker's face with both. Walking needs a locomotion tree driven by the received speed; the
  `GenMale_AnimTrees.*` trees for that are packaged in later levels.
- Nothing is shared but the partner's position: enemies, Elizabeth, story and saves stay per player.
- Steam P2P between two real accounts; play over the internet in IP mode; a normal Steam launch of the player build
  (only `-nomovie -windowed` test launches were run).

## Test copies in Columbia: loading a save without a human (2026-10-30, early morning)

### Observed

- With `-nomovie`, the front end is never built: only archetypes of `XPressStartScreen` / `XMainMenu` exist and the
  title world stays black. **Without `-nomovie`**, the real menu shows. Live screens:
  - `ScreenFlows_Mergeable.InWorldMenuFlow.XScreenFlow_PlayMovie_9.XPressStartScreen_0` ("press any key");
  - then `..._PlayMovie_0.XMainMenu_0` ("Main game" selected);
  - then `..._PlayMovie_4.XSinglePlayerMenu_0` ("Continue" selected).
  Calling `AcceptOrSelect` on each, through the bridge, loads the most recent save of the copy's `SaveCloud`.
  A copy of the owner's `TWN-0_*.bkm` (welcome center), duplicated as `.sav` (pairs are byte-identical), took copy 0
  into Columbia in game.
- Dead ends, for the record:
  - `XGameViewportClient.StartTravel(URL, TravelType, CheckPointName)`: `TravelType` is UE3's `ETravelType`
    (2 = relative crashes on a null base URL), not `EMainMenuLoadGameTypes`. `CheckPointName` (`const out name`,
    flags `0x182`) set to `S_TWN_P` still starts a new game.
  - `XGameEngine.CheckpointFilenameToLoad` (`+0x51c`, a string) can be written with the engine allocator
    (`FUN_00482ab0` = `appRealloc` through `GMalloc` at `0x01371cc8`), after
    `PopulateSavedGameInfoFromSelectedStorageDevice`. Neither `StartTravel("Master_P", 0)` nor `XRestartLevel`
    uses it.
  - Kismet: `Prep_TownCenter`, `Load_TownCenter` and `TownCenter_Ready` are remote events, and `GoToTownCenter` is a
    console event (`XPlayerController.ServerCauseRemoteEvent` / `ServerCauseConsoleEvent` return true). Fired from the
    lighthouse, they stream in all `S_TWN_*` levels but leave the player at (59600, -148492) behind a black screen.
    Firing all four at once hung the loading screen.
- In Columbia, the male locomotion set `GenMale_Unarmed_Animations.GenMale_UA_Locomotion_AnimSet` is loaded
  (76 sequences, including `WalkFwd_untitled`, `RunFwd_untitled`, `SprintFwd_untitled` and `IdleBase_untitled`).
  `SetAnim("WalkFwd_untitled")` on the puppet's `AnimNodeMorphemeSequence` nodes does nothing (`AnimSeq` stays None)
  until the set is in the component's `AnimSets` array. The co-op module appends it with the engine allocator.
- Two-copy test in Columbia (`essai_coop.py --colombie`):
  - both copies resumed the save through the menu and spawned each other's puppet;
  - circle: mean error 8.2 units, 0.0 after stopping;
  - walk: frame captures from a fixed camera show the puppet striding across the welcome center (`WalkFwd_untitled`);
  - regroup (F8, simulated by a file): the player went from 400 to 90.6 units from the partner;
  - no crash; the owner's files were unchanged.
  - Defect: in copy 1 the puppet was created while the copy was still loading Columbia, before
    `Arc_ActorBase.XIdleActor` existed, so it had no template and no anim nodes. Fix: look for the template every 2 s,
    spawn without it only after 20 s, and never spawn in the menu world `XEntry_P`. Re-run: both puppets had their
    template and 2 anim nodes with the locomotion set.
- Endurance test (`--colombie --endurance=10`): both players wandered at the same time (about 140 units/s, random
  heading within 300 units of their start) for 10 minutes, with an F8 every 2 minutes. Every 5 s, each player was
  stopped for 1 s and compared with its puppet in the other copy: 162 measurements, maximum error 0.0 units, 4 F8,
  no crash, both copies alive, owner's files unchanged.
- Partner marker on the HUD, not solved: `XCore.XHudMarker` spawns (template
  `Arc_SkyLineFX.LocalPlayer_TBarPointHUDMarker`, `MarkerTemplateName` `BasicHUDMarker`) and `MarkerEnabled` can be
  set, but nothing is drawn, and `XOnToggle` throws. Markers probably have to be registered natively
  (`XWorldInfo.ActiveHUDMarkers`). Quest markers (`XObjectiveMarker`, `XDynamicObjectiveMarker` in `S_TWN_Quests`)
  drive the N navigation guide, so they are not reused.

## Milestone 4: shared enemies (2026-10-30)

Design: "twin worlds". Each copy runs its own AI. The co-op module only exchanges damage and deaths. The other
option, AI on one side only with the partner as a real pawn, is blocked: a second `XHuman` crashes the game
(milestone 2). (Superseded from 2026-11-02: the puppet became an AI target without being a pawn, and the host now
drives the client's enemies; see "Host-authoritative enemies" below.)

### Observed

- Owner's save `TWN3-0_*` (Columbia station, "Ticketing") resumed in both copies through the menu. Both had the same 6
  enemies (`Master_P.TheWorld.PersistentLevel.XHuman_1..6`, each with its own `XAIController_N`, archetypes
  `Arc_XAIMelee.XAI_MeleeBeta_Pawn` and others).
- A live AI pawn's `SpawnInfo` struct is `{NameOfSpawner, ContainingLevelName, AppearanceOverride, SpawnedDeadAI}`,
  e.g. `XAIScriptedSpawner_3` / `S_TWN_Station_Game`. The spawners' `SpawnInfo` is a different, larger struct
  (`PawnArch`, `Faction`, `LootList`...). The pair (level, spawner name) was the same in both copies for all
  6 enemies, so it is the identity. The module adds a rank per spawner, for waves.
- `WorldInfo.PawnList` / `Pawn.NextPawn` lists the pawns, including the player's. `XPawn.XHealth` / `XHealthMax` are
  plain floats. Writing `XHealth` lowers health, and it stays lowered (no regeneration seen in 3 s).
- `XPawn.XKilledBy(EventInstigator)` kills: health goes to 0, the controller is detached, and the body ragdolls
  (captured in both copies). `EventInstigator = None` throws. The pawn itself as instigator works. The dead pawn
  leaves `PawnList` within about 1 s, and the object is destroyed a little later.
- A test player left standing among enemies is killed; the "door" death screen appears. Enemies regain health when
  the player dies, so the test keeps both players' `XHealth` at max.

### Implemented (sonde/coop.cpp, `SuivreEnnemis`)

- Every frame, walk `PawnList`, skip player-controlled pawns, identify each new pawn once (cache keyed by pointer and
  FName, because pointers are reused after GC).
- Each copy sends, 10 times per second, the cumulative damage it dealt to each enemy plus a dead flag (packet magic
  `BICE`, at most 100 entries, 922 bytes, under Steam's unreliable packet size). A lost packet loses nothing.
- On receipt, the part of the partner's cumulative damage not yet applied is subtracted from `XHealth`. If that
  reaches 0, or the partner says dead, the module calls `XKilledBy(self)`. An enemy the partner already killed dies as
  soon as it spawns locally.
- Pose and enemy packets share both transports (UDP and Steam P2P). Steam receive now accepts any size up to the max.

### Test (`outils/essai_ennemis.py`, both copies on `TWN3`)

- 6 to 7 enemies in each copy, all matched by spawner.
- Damage 1→0 and 0→1 (write `XHealth - 100` in one copy): the twin lost exactly 100 in the other.
- Death 1→0 and 0→1 (`XKilledBy` in one copy): the twin died in the other within 3 s (logs: "tué par le partenaire").
- Owner's files unchanged. Result: OK over local UDP and over Steam P2P (same account, both copies launched fresh).
- Enemies changed in the last 3 s go in every packet; the others take turns in the remaining slots, because the
  list grows over the whole campaign (`Master_P` is one world).

### Shields (added later the same day)

- `XCore.XPawn.XShield` / `XShieldMax` are float properties (found by listing reflected properties whose name contains
  "Shield"). The `TWN3` enemies (gunners, pistols, melee) all have 0 / 0, with `bIsShieldedVariant = False` and an
  empty `PawnShields`. The player has 1240 / 1240.
- An enemy's "points" are now shield + health. A drop in the sum is local damage, and a rise (shield recharge) is
  ignored. Received damage comes off the shield first, then health. Death is still decided on health alone.
  Packet version bumped to 2. A peer with another version is logged once and its packets are dropped.
- Test: give two enemies a 300 shield in both copies (`XShieldMax` and `XShield` written over the pipe; the game kept
  the value, no recharge seen). Copy 1 sets it to 200, and the twin dropped to 200 with health unchanged. Copy 0
  removes 400 (shield 0, health −100), and the twin ended at shield 0 and health −100. OK over UDP and Steam, and the
  pose test still passes.
- Not checked: enemies that really spawn with shields, and the "shield broken" effect, which is not played on the
  receiving side because the value is written directly.

### First hands-on test (owner plays copy 0, `outils/bot.py` plays copy 1)

- **Kills not mirrored.** The game sometimes replaces live enemies in one copy only, for example when that player
  respawns. The per-spawner rank ("#2" on one side, "#1" on the other) then broke the identity. The fix counts deaths
  per spawner instead: each entry is (spawner hash, deaths, damage dealt to the live one), packet version 3.
  `essai_ennemis.py` gained a clean-up test: player 0 dies, then copy 0 kills everything for 40 s, and nothing it
  killed may stay alive in copy 1. One limit remains: a wave the game spawns on one side only (spawners 11 and 8 at the
  station) does not exist on the other side.
- **Purple "error" puppet.** The puppet kept the template's `Shared_Textures.CharacterWrong` mesh.
  `Booker.Booker_MESH` lives only in `S_Light_Capsule`, `S_Light_Geo` and `S_Lut_LighthouseInf2_Geo`, so it is not
  loaded at the station. Two loading routes fail:
  - `DynamicLoadObject("S_Light_Capsule.Booker.Booker_MESH")` returns None, because it loads no packages in this
    build.
  - The `STREAMMAP S_Light_Capsule` console command loads the mesh but replaces the current levels, and the player
    falls into the void.

  What works is `PlayerController.ClientPrepareMapChange('S_Light_Capsule', true, true)`. It loads the 3 MB capsule
  without showing it, and the mesh appears within 0.3 to 2 s. Master_P runs 39 `SeqAct_PrepareMapChange` and 39
  `SeqAct_CommitMapChange` of its own, so the pending change must be cancelled, in this order:
  1. Wait for `WorldInfo.IsMapChangeReady`.
  2. Set `RF_RootSet` on every object the preload brought, except actors and components under a `TheWorld`
     (284 objects).
  3. Call `ClientCancelPendingMapChange`.

  Object flags are 32 bits at +0x08 and `RF_RootSet` = 0x4000. This was checked against a class, a `Default__` object
  and the Core package, which have it, and a controller, an archetype and the fresh mesh, which do not.
  `_Linker` is at +0x0C and `_LinkerIndex` at +0x10. The objects the capsule shares with the station (first-person
  hands, player camera rig) keep their object, linker and index, so they are not reloaded.
- **Render-thread crash** (read at 0x200, instruction 00686CE1 in `FUN_00686c10`, called from render command
  0066eed0). The function skins a chunk with 55 bones (EDI) against an empty reference-to-local array.
  - Cause 1: cancelling without the root set. The next GC, about 60 s later, freed part of the load. It crashed 2 runs
    out of 2, and did not crash in 200 s when the change was kept pending.
  - Cause 2: using the mesh before `IsMapChangeReady`. The object exists before it is finished. It crashed 4 runs out
    of 4 once the puppet's bones were updated while not rendered.
  - With both fixes, 6 full runs in a row passed with no crash. The puppet now sets `bUpdateSkelWhenNotRendered`,
    `bTickAnimNodesWhenNotRendered` and calls `ForceSkelUpdate` after `SetSkeletalMesh`.
- **Floating puppet.** The pose sent is the pawn location, the centre of its collision cylinder (`CollisionHeight`
  130). The body is drawn at `Mesh.Translation` (Z = -133 for the player, -138 for a station soldier), so the puppet
  floated by that much. It is now lowered by the local pawn's `Mesh.Translation.Z`.
- `sonde/journal_plantage.cpp` now names engine objects it finds in registers, one dereference from each register,
  and on the stack.
- `outils/copies.py` had a race. `bi_pilote.TUBE` is global, so two threads connecting at once could open the wrong
  pipe. A lock fixes it.

### Partner's weapon and shots

- The weapon in hand is `XPawn.CurrentRHWeapon`. `CurrentLHWeapon` holds the vigor. Each player weapon has an outside
  view model, `XWeapon.ThirdPersonModelDefinitionArchetype`; for example `PistolFounder` uses
  `Arc_XPistol.Models_ThirdPerson.Pistol_TPModel`. A shot is detected when `XWeapon.LastFiredTime` changes.
  `InvManager.EquippedWeaponIndex` is not an index into anything script-visible.
- A station soldier's model (`XWeaponModelThirdPerson`) has `Base` set to the pawn, `BaseSkelComponent` set to its
  body and `BaseBoneName` R_Grip, with zero relative offset. The model's pose equals the R_Grip bone's world pose.
  Booker's mesh has R_Grip too (bone 49).
- `SetBase` does nothing on a spawned model: not on the puppet, and not on a soldier either, whether called through
  the pipe or through ProcessEvent. The module copies the bone pose every frame instead, with `GetBoneLocation` and
  `GetBoneQuaternion` (Space 0), brought into the puppet's floating section.
- The archetype's mesh component has `HiddenGame` true. The game unhides it on equip; the module calls `SetHidden(0)`
  itself.
- Effects: `MuzzleFlashPrimary` is an `Engine.XEffect.XEffectContainer`. For the pistol it pairs the visual
  `Arc_HandCannonVisualFX.HandCannonThirdPersonMuzzleFX` (an `XEffectMultiple` containing an `XEffectParticle` that
  plays `FX_MachineGun.Particles.MuzzleFlashThird_par` on socket `muzzle`) with the sound `Arc_Pistol_SFX.FireEffect`.
  Script has no general play-effect function.
  - What works: `XPlayerController.ClientScriptedPlayEffect(InActor, PlayEffectInterface)`, where the interface is an
    `XRPCPlayEffectInterface`. `XSeqAct_PlayEffect` is one, and its `Effect` field is the container. The module
    borrows a loaded `XSeqAct_PlayEffect`, swaps its visual and audio pointers for the call, and restores them.
  - What fails: spawning `Engine.EmitterSpawnable` throws, and `EmitterPool.SpawnEmitter` is not in this build's
    script.
- Result (`outils/essai_arme.py`): copy 1 shows the pistol in the puppet's hand, a muzzle flash with light on copy 0's
  shot, then smoke. Pose packet version 4 adds `Arme` (hash of the model path) and `Tirs` (shot counter).

### Aiming

- Station soldiers' armed locomotion sets are loaded at TWN3:
  - two-handed weapons: `GenMale_MachineGun_Animations.GenMale_MG_Locomotion_AnimSet` (80 sequences, including
    IdleBase, WalkFwd and RunFwd) and `GenMale_MG_Weapon_AnimSet` (AimPoses, AimPoses_Locomotion, Fire, Reload,
    Equip...);
  - pistol: `GenMale_Pistol_Animations.GenMale_PI_Locomotion_AnimSet` (only IdleBase and runFwd_AlternateA) and
    `GenMale_PI_Weapon_AnimSet`.

  Sequence names repeat across sets, and lookup takes the last set in `AnimSets`. The module reorders the tail per
  stance: unarmed is [UA]; two-handed is [UA, MG locomotion, MG weapon]; pistol is [UA, MG..., PI...], so the pistol
  stance walks with the MG walk.
- `AimPoses_untitled` (9 frames, 0.267 s, not flagged additive) looks wrong when played alone: arms over the head.
  `Fire_untitled` is additive.
- The weapon actor takes the hand position from R_Grip, but its rotation comes from the partner's view: the
  controller's pitch, sent in `Rotation[0]` (the puppet itself stays upright), and the pawn's yaw. The barrel is the
  model's +X axis, so no offset is needed.
- The guard stances turn the shoulders relative to the actor's yaw: +63° with the pistol, +60° with two-handed
  weapons, about +53° at the hips. These were measured from the `GenericHumanL/RUpperarm` and `L/RThigh` bone
  positions. Soldiers' bodies stay within roughly ±50° of the pawn's yaw, and their aim offsets fix the rest. The
  puppet has no aim offset, so its actor is turned back by the stance angle.
  `essai_arme.py` measures shoulders 0 to 5° from the aim, for both weapons.
- Superseded on 2026-11-01 (see "Partner puppet, armed"): the weapon now takes the `R_Grip` bone rotation, the body
  turns so the barrel points at the aim, NPC aim poses are used as frozen frames, and the tests measure the barrel.
- `Destroy` returns False on spawned `XWeaponModelThirdPerson` actors, and the old pistol kept floating. Instead,
  each model is kept per weapon, hidden (actor and mesh) and reused.

### Movement sync, measured (`outils/essai_synchro.py`)

- Setup: owner's save `TC_PreLottery` (the open town center before the raffle, no enemies). Copy 0 moves with its
  real inputs: `PlayerInput.aBaseY` / `aStrafe` rewritten every frame, `XPlayerController.StartSprint`, and the
  controller rotation. `time.sleep(0.004)` between writes lasts about 15 ms on Windows, and the player then only
  moved at 31 units/s. Real speeds are about 650 units/s forward, 480 backward and 560 strafing. Walking straight
  for 3 s fell off the island, so the route returns to its start.
- Before:
  - The puppet was rendered 60 ms behind receive time, between the last two packets only. The target time almost
    always fell before the older packet, so the puppet stood still for 11 to 54 % of moving samples.
  - The body faced the movement direction: 180° wrong walking backward, 90° wrong strafing.
- After (pose packet v5 adds `Temps`, the sender's clock):
  - A 64-packet history is kept. The clock offset is the minimum (receive - send), and the jitter is the 90th
    percentile minus that minimum.
  - The render target is newest - max(jitter + 10 ms, 1.5 send periods), interpolated between the two packets
    around it, and extrapolated by at most 100 ms when a packet is late.
  - The body faces the view when standing and when walking backward. Backward plays the forward walk at a negative
    rate unarmed, or `WalkBwd` / `RunBwd` from the MG set when armed. The body faces the movement direction going
    forward, and turns 45° toward the step when strafing, since the game's male sets have no strafe animations.
  - First results: stalls 0 to 4 %, delay 40 to 50 ms, lag-compensated position error 5 to 12 units on average. The
    body is 0° off the view walking backward and 45° strafing, and 7° behind when turning on the spot at 120°/s.
    These came before the timestamp and root-motion fixes below, which brought the error to 1-5 units at 60-70 ms.
- Faster test launches: the three startup movies (21 s) were replaced by `LoopingMovieUntilStartupFinishes` in each
  copy's own XEngine.ini.
  - Leaving no startup movie, or naming a missing file, means the press-start screen never comes.
  - `-nomovie` blanks the Scaleform menus.
  - The press-start screen still ignores input for about 7 s while the game finishes starting up.
  - Copies are now in game 44 s after launch, down from 65 to 70 (32 s since 2026-11-01, see "Faster test launches").
- Lag found in hands-on testing: bi_sonde's `Resoudre` compares every object's path, costing 20 to 64 ms per
  command that names an object, on the game thread. The module now rewrites queued pipe lines to `0x` addresses
  through a cache that is re-validated on each use, and such a command costs 4.7 ms. With the bot running, both
  copies hold 144 fps (before: 1 to 85).
- Quit crash: a spawned `XWeaponModelThirdPerson` with no `WeaponObject` crashes in its destructor
  (`FUN_00ac0330`, read at +0x8C) during level teardown, even with `bNoDelete`. The partner's weapon is now a plain
  `XReactiveSkeletalMeshActor` carrying the template's mesh.

### Movement sync, follow-up (2026-10-30 evening to 2026-10-31)

- **Teleports were a clock bug.** The send time was a float of `QueryPerformanceCounter` seconds, which counts from PC
  boot: about 524,000 s after six days of uptime, where a float only has 62.5 ms steps. Pose packet v6 sends integer
  milliseconds since the first send. The render margin now follows the jitter by at most 5 % of elapsed time (it
  jumped when a late packet entered the window). Teleports per move went from 7-17 to 0 (one in the whole run);
  lag-compensated error 1.4 to 2.7 units, delay 60 ms.
- **The other "teleports" were root motion.** The game's male walk sequences carry their own translation; the
  puppet's pelvis went up to 418 units ahead of the actor, then snapped back when the sequence restarted.
  `bZeroRootTranslation` on the puppet's anim nodes keeps the pelvis within 2-12 units (sway).
- Pose packet v7: the pose is sent at the feet (pawn centre plus the sender's `Mesh.Translation`), with state bits
  for in air and crouched. Measured root speeds: `WalkFwd` 210, `RunFwd` 691, `SprintFwd` 1,006 units/s; the player
  moves at 480-680 at normal pace. At normal pace the walk is sped up (at most x2.4, the feet slide a little), above
  850 units/s it switches to `RunFwd`. In air (`PHYS_Falling`, only after 0.15 s): `Falling_untitled`. Crouched:
  `CrouchIdle`, `CrouchWalkFwd`/`Bwd` from `GenMale_UA_Crouch_AnimSet` (and the MG crouch set when armed). No
  landing animation: `LandLight` with root motion cancelled crushed the body; the blend out of the fall is enough.
- Blends: the extras' tree has an `AnimNodeBlendPerBone` "IsTalking" (child 1 Idle, child 2 Talking). With its 90
  per-bone weights set to 1, each new sequence starts on the inactive child and `SetBlendTarget` moves to it in 0.2 s.
- Trot (v0.10): the game has only walk, run and sprint. At normal pace, run and walk play together on the two
  children of the blend node, at the same step cadence, mixed by speed (20 to 90 % run); the walk is phase-locked to
  the run with `SetPosition`.
- `essai_synchro.py` gained jump (`Pawn.DoJump`) and crouch (`ToggleCrouch`) and checks the dominant anim node per
  move: all OK, error 0.6 to 4.4 units.

### Pause menu entry, partner collisions, vigors (2026-10-31)

- The pause menu list is filled in C++: method at `0x00b43480` calls `FUN_00b68160(section, L"MenuSelection", 5,
  list)`, labels come from `FUN_00b29050`, the choice from `FUN_00b44550(index)` (ignores index > 4), and
  `FUN_00b403c0(index, enabled)` greys entries 1, 3, 4 every frame on the displayed widget. Hooking those four and
  patching the `PUSH 5` byte to 6 adds a MULTIPLAYER entry (3rd position) with its own sub-page. The grey-out call can
  run before the list is refilled: its index must be checked against the widget arrays (+0xd8, +0xb4), or it reads past
  the end and crashes.
- The puppet's cylinder already blocked the local player; `SetCollision` is ignored for it. Writing `bBlockActors` on
  the actor and `BlockActors` on its `CollisionComponent` toggles it (menu setting, off by default).
- Alt-tab: the game pauses on deactivation messages and re-installs its own window procedure after startup, so a
  filter must re-insert itself. The bridge's "focus keep" mode also closes any pause menu, so test copies leave it.
- Vigors: `CurrentLHWeapon`, `StartFirePlasmid` / `StopFirePlasmid`, `LastFiredTime`. Third-person vigor models carry
  sound only; first-person effects are camera-lens particles, camera anims and hand material overlays, invisible (or
  harmful) for an observer. A pooled `ParticleSystemComponent` (`EmitterPool.GetPooledComponent`) shows only once
  attached to an actor (`AttachComponent`); kept absolute and moved to the hand bone each frame; looping systems need
  `KillParticlesForced`. Visible from outside: `FX_Mesmerize.MesmerizeBurst_par` (Possession), `FX_Incinerate.Flame_par`.
- No third-person Booker without the bandaged hand exists (three identical `Booker_MESH` copies; a texture swap in
  video memory worked, but the knot is geometry, so it was reverted).

### Projectiles, friendly fire, enemy positions (2026-10-31, night)

- Projectiles (vigor fireballs, traps, homing Possession, rockets) are mirrored by streaming each local-instigator
  `XProjectile`'s archetype, position and state; the partner moves a `DynamicSMActor` carrier and plays the archetype's
  `FlyingEffect`, `StuckEffect`/`StuckStateEffect`, `ExplodedEffect`. Those effects are made for world view and show.
- Friendly fire: aim ray vs puppet cylinder on the shooter, damage from the weapon's `XDamageType.DamageStimuli`,
  cumulative value in the pose packet, applied to `XShield` then `XHealth` (clamped at 1) by the victim.
- `XWeapon.LastTraceShotEndLocation` gives each shot's end point. Third-person tracers are Beam2 emitters whose target
  is user-set (`SetBeamTargetPoint`, native, not exposed to script): no tracers yet. (Later findings: that end point is
  the end of the weapon's range, not the hit point; and a beam end can be set without the native, see "Vigors,
  impacts and bullet holes".)
- Enemy positions: host streams positions per spawner; the other copy snaps an enemy that drifted beyond 250 units, at
  most every 1.5 s. Both AIs keep running, so drift stays 2-10 m in combat (23 m without). Tight sync would need the
  non-host AI disabled and its pawns driven like the puppet. (Replaced from 2026-11-02, see "Host-authoritative
  enemies".)
- Debug tricks: `XWeapon` instances can be granted with Spawn (archetype template) + `AddInventory` +
  `StartGivenTo` + `TryToEquip`; vigor energy is a native int at `XPlayerController+0x510`.

## Partner puppet, armed (2026-11-01)

### Observed

- At Battleship Bay (save `BW2`) the extras' archetype `Arc_ActorBase.XIdleActor` is not loaded yet when the puppet is
  due. Spawned without it, the component had only an empty `AnimNodeSequence` and Booker stood in a T-pose. Setting
  the extras' tree (`Idle_Base_AT`) with `SetAnimTreeTemplate`, or the one-node `SingleAnim_AT` that is always loaded,
  removed the T-pose, but the one-node tree has no blends or trot and looked stiff, so it was dropped. The puppet now
  waits for the archetype with no time limit (it arrives when the player walks toward the promenade: 83 s in the
  test; searched every 5 s after the first minute).
- Shotgun stance: animation sets [UA, MG, SG], with the NPC shotgun idle and aim (`GenMale_SG_*`) and the MG walk and
  run.
- The weapon sat on `R_Grip` but was turned to the partner's view; with the NPC hip poses it came out of the hands.
  It now takes the full `R_Grip` bone transform, as NPC weapons do. Checked in close-up for shotgun, machine gun and
  pistol; recoil kept.
- At rest, the body turns (three-quarters, depending on the stance) so that the barrel points where the partner looks:
  the barrel yaw (weapon +X on `R_Grip`) is measured relative to the body and smoothed. Walking keeps the fixed
  shoulder offset; the switch between the two is blended.
- `AimPoses_untitled` (wrong when played, see "Aiming") works as a source of frozen frames: middle at 4/30 s, up at
  1/30, down at 7/30, blended by view pitch and corrected from the measured barrel pitch. It is held at rest the whole
  time with a blend in and out (first only for 1.2 s after a shot). NPC `Fire_untitled` is additive and the tree has
  no node to play it, so there is no firing gesture.
- Measured: machine gun, pistol and shotgun at 0, +30 and -30° of pitch, barrel within 5° of the aim. Latest automated
  runs (2026-11-07): `essai_tirs` 1.1 / 0.9 / 1.5° (shotgun), `essai_arme` 0.2° (pistol) and 0.7° (machine gun).
- The shotgun muzzle flash did not play: `XEffectSelectorFirstOrThirdPerson` did not recognise the puppet's weapon.
  The module plays its `ThirdPersonEffect` directly.

### Derived

- A puppet can borrow an NPC aim sheet as a pose library even when the sheet cannot be played as an animation.

## Vigors, impacts and bullet holes (2026-11-01, packets v10 to v14)

### Observed

- Flying effects stayed at the launch point: Possession's `XSkeletalMeshEffectActor` sat on the carrier and is never
  destroyed, and the Devil's Kiss ball stayed in the puppet's hand. Flying particles are now lit separately and
  re-attached to the carrier every frame; the Possession actor moves with the carrier, then is hidden. `Destroy` is
  refused for actors spawned in game, so actors are hidden and carriers reused.
- Stuck effects play as state effects (`bIsStateEffect`, stopped with `ClientScriptedStopStateEffect`): before, the
  Possession trap stopped and the Devil's Kiss trap did not show. Packet v10 adds `BounceNum` and `LastHitNormal`, so
  each Devil's Kiss bounce plays `BouncedEffect` on a carrier turned to the surface.
- The game turns a stuck effect's X axis along the surface normal, outward (a Possession trap on a wall was upside
  down otherwise). Shock Jockey traps face into the surface instead.
- Packet v11: projectile packets carry the send time on the pose clock, and the receiver interpolates between the
  timestamped points around the puppet's display time. Before, the last packet was extrapolated: the Possession ghost
  stood still for 0.15 s, then caught up at 10,000 units/s (3,800 on the thrower). After: same speed and animation time
  as on the thrower, under 40 units apart.
- The carrier takes the projectile's mesh and materials: Shock Jockey crystal `FX_VoltSwarm.SingleCrystal` (six traps
  laid, six crystals on the other side). `TrapBoltArmedEffect` plays as a second state effect.
- Packet v12, Shock Jockey arcs: the thrower reads the `XTrapBoltNetworkManager` connections (an array of pointers to
  `TrapBoltConnection`) and sends, per trap, the ids of the traps it arcs to. The receiver lights
  `TrapBoltConnectedBeamEffect` at the trap's attach point. **`SetBeamTargetPoint` is native and not in script, so the
  beam end is written straight into each emitter instance's `UserSetTargetArray` at +0x210**, after checking that the
  instance belongs to that beam.
- Crash: calling `SetStaticMesh(None)` on a carrier before reusing it crashed the copy (read at 0xA0: the Kynapse AI
  obstacle re-read the mesh). Free carriers are now pooled per mesh and keep it.
- Packet v13, area effects (`BICZ`): for 2 s after a cast, the thrower lists particle systems that became active in
  `XEmitterPool` (Bucking Bronco wave, Undertow jet, Return to Sender vortex, Shock Jockey hand sparks) and sends
  template, position, rotation, scale and end. The receiver lights them at the same place; those born near the
  thrower's eyes (hand effects seen in first person) follow the puppet's left hand. Filtered out: first-person
  systems (by name), systems far from the thrower, near a projectile (already mirrored) or near what is shown of the
  partner (no echo).
- Crash found on the way: the projectile search read every object's name; during garbage collection, on a
  half-destroyed object, it crashed, and the engine then raised "Illegal call to StaticFindObject()". The (cached)
  class is tested first now.
- Murder of Crows short cast: six `XReactiveSkeletalMeshActor` built from `MOC_CrowFlockActor`, with the player as
  `Instigator`. They go through the projectile packets as "actor" entries with rotation; the receiver spawns the same
  template, without its own lifespan, and moves it like on the thrower.
- Shock Jockey short cast: on the thrower the bolt is made of first-person trails, which stayed short when replayed at
  the puppet's hand. The receiver draws the vigor's beam (`StEBeamNoLoop_par`, end set as for the arcs) from the
  puppet's hand to each relayed impact, for 0.35 s.
- Bullet impacts: `XWeapon.LastTraceShotEndLocation` is the end of the range (3,000 units), not the hit point. The
  impact particles the shooter's game lights at the real hit point (one per pellet, right material and normal) are
  relayed through `BICZ` for half a second after a shot, with the weapon's impact sound (not checked by ear: test
  copies are muted).
- Packet v14, bullet holes: the shooter diffs `DecalManager.ActiveDecals` (fixed parent material of the fading
  instance, position, orientation, size, rotation; 30 entries per packet at most). The receiver takes a pooled
  component (`GetPooledComponent`), sets it with `SetDecalParameters` and attaches it (`AttachComponent`); it is
  released with `DecalFinished` after 30 s (the game: about 32 s). `SpawnDecal` and `SpawnPooledComponent` set nothing,
  and the `_FADE` instance used as is stays invisible. A 2 s machine-gun burst gave 19 holes on both sides; the latest
  `essai_tirs` (2026-11-07) gave 5/5 shotgun holes and 20/20 machine-gun holes.

### Derived

- Writing `UserSetTargetArray` at +0x210 should also give bullet tracers (`TracerThirdPerson_par` beams). Not done.
- Watching `XEmitterPool` and `DecalManager` on the sender is a generic way to mirror "whatever the game just drew",
  without knowing each effect's owner.

### Not covered

- No automated test covers vigor projectiles, arcs or area effects; all of the above was checked on captures.
- Area effects are visual only: the lift, pull or push of enemies is not shared.

## Faster test launches (2026-11-01)

- `relancer.py` stops the copies, drops the freshly built DLLs in, and launches to in-game. Gamepool stagger 5 to 1 s,
  watch 8 to 3 s. The press-start intro is skipped (its countdown set to 0.01 s).
- `-coopsansfocus`: a `WH_CBT` hook in the guard `dinput8.dll` refuses activation for 2 minutes except on a click or
  Alt+Tab, and the game's own activation calls are made non-activating while it is not in front.
- `-coopmuet`: the module mutes the process's audio sessions on every active output.
- Measured: 52 s to 32 s from launch to in game, and no copy steals the foreground. The 7 s wait after the key press
  (2K connection and news) could not be shortened.

## Robust network (2026-11-02, packets v15)

### Observed

- A partner who restarted the game started again at sequence 1, and every packet was rejected as late until the
  other player restarted too. Every pose now carries a random 32-bit session id drawn at launch. A new session clears
  the 64-packet history, resets the shot and cast counters, stops and stores the partner's effects, and resets the
  enemy counters on both sides. Enemy packets (cumulative counters) carry the **receiver's** session as the sender
  knows it, so counters from before a restart are refused (otherwise re-spawned enemies died at once).
  Test `essai_relance`: copy 1 restarted alone, copy 0 sees it again with 0 rejected packets, its shots and bullet
  holes come through (last OK 2026-11-07).
- Received values are checked: finite positions inside the world (pose, projectiles, area effects, enemy positions),
  finite and bounded damage, and a projectile's floating section must exist locally (otherwise
  `MoveToFloatingSection` would crash).
- The packet version is checked on all six packet types: `BICP` pose, `BICE` enemy damage and deaths, `BICX` enemy
  positions and state, `BICJ` projectiles, `BICZ` area effects, impacts and decals, `BICT` turrets. A mismatch is
  logged and shown in the MULTIPLAYER menu. There is no compatibility between versions.
- Silent or absent partner: projectiles, arcs and area effects are stopped and stored (before, they froze in place).
  Test: Possession trap laid by copy 1, copy 1 killed, trap removed on copy 0. A projectile not heard of for more
  than 1 s and then heard again is recreated.
- An exception in the co-op tick hides the puppet and weapon, stores the effects, forgets everything and restarts
  5 s later; the third one stops the module. A failed puppet spawn is retried 10 s later (3 times). Not provoked in a
  test.
- The JOIN PARTNER menu entry shows why joining is not possible (no connection, absent or elsewhere, other mod
  version, co-op stopped).
- Packet sizes stay under Steam's unreliable P2P limit (static asserts in the code): `BICE` 100 entries, `BICX` 18
  enemies, about 1 KB each.

### Derived

- A per-launch session id, plus "cumulative counters valid only for the receiver's session", lets counter-based sync
  (deaths and damage per spawner) survive a one-sided restart without a handshake.

### Not covered

- All of this ran over loopback UDP between two copies on one PC. The player modes were last run on 2026-10-30
  (packets older than v9): host/client over 127.0.0.1, and Steam P2P with one account sending to itself (each process
  also read some of its own packets, which it rejected). There is still no test between two PCs, two Steam accounts, over the internet, or with
  simulated latency and loss.

## Skyline (2026-11-02)

### Observed

- The skyline is the "TBar" in the code: native actions (`XAction_TBar_*`); no script field says "on a rail".
- Mounting through the bridge failed: the target (`XPlayerController.GroundToTBarTarget`) is only computed while the
  real Space key is held (`XActivateGroundToTBarTransfer`, handled by native input), and rail areas are only loaded
  when the player gets there (teleported, the player hangs frozen in the air, physics 0).
- Loaded rails are `SplineComponent`s on `XTBarSpline*` actors (`SplineInfo` points, Hermite curves in world space).
  The sender sets a `SurRail` state bit when the player is not walking and its centre is under a segment: within
  70 units horizontally and 0 to 300 units below.
- The partner hangs from the game's hook (`Transit_Hook.TransitHook_RIG`) on the `L_Grip` bone in the NPC
  `Tbar_StoppedIdle` pose (`GenMale_UA_SkyLine_AnimSet`; the riding poses are additive), weapon hidden, body facing
  the ride; without that set in the level, the fall pose.

### Not covered

- Checked only on captures at the station (`TWN3`, where rails are close), with the state forced by a test file. The
  thresholds are not tuned on a real ride, and no automated test covers it.

## Host-authoritative enemies (2026-11-02 to 2026-11-03, packets v16 to v20)

The design moved from "twin worlds" (milestone 4) to the host deciding for enemies, in steps. All measurements are
at the `TWN3` station fight (6 enemies), on two copies on one PC.

### Observed

- **v16, steering every frame.** The old snaps left 2.4 m median drift, often 8.5 m, 18 m at worst, with visible
  jumps. The host now sends position, velocity and yaw 15 times a second (`BICX`). The client slides each enemy toward
  the host position extrapolated by velocity, takes the host yaw, and snaps beyond 4 m. Measured with
  `ecart_ennemis.py 30 --combat`: median 24 cm, 90 % under 42 cm, 1.15 m at worst (that tool prints to the console only;
  no later figure is recorded). Writing `Velocity` does not make a pawn run, with or without physics: its motion comes
  from its animations. AI on each side still aimed at its own player, so an enemy could run on one screen and aim on
  the other.
- **v17, AI sees the partner.** The puppet became an `XCore.XProtectionObjectiveActor` (same family as before, plus a
  faction: `FactionID` 0 = Players, and `bTargetableForAI`), with the same `Arc_ActorBase.XIdleActor` template. It
  registers itself with the faction registry. `bTargetableForAI` must be written after spawn (the template has it
  false), and `bBlockAIVision` set false. Enemies in both copies see it, target it, shoot it, chase it and hit it
  with the baton, like a player; it never takes damage and bullets pass through it. It stops being a target when
  hidden or when the partner is on the death screen (pose bit `JoueurMort`); AI drops it within a second. The crash of
  a second `XHuman` came from `XPawn.ActionModel` never being created with a player archetype. Test `essai_cible`:
  6 of 6 enemies target it within 8 s; 0 while the partner is dead; 6 again after; with the old class, 0.
- **v18, brainless enemies on the client.** An enemy is put under host authority (`XAIController.bIsSuspended`,
  `Physics` 0) when, for 0.5 s, the host says it is steerable (in combat, not in a scripted sequence
  `PlayingScriptedSequence`, only living one of its spawner, not possessed, partner targetable), the client saw it go
  on the attack itself with its weapon out (cut earlier, it ran empty-handed), and both sides count the same deaths
  for its spawner. It then copies the host's position, yaw and speed every frame. It goes back to local AI at once if
  lifted, thrown or possessed locally, or if it changes section; after 1 s if the host stops proposing it; and when
  the host has been silent for 1 s. A death from the host wakes it, then kills it. Cost: 0.2 to 0.3 ms per frame in
  combat. Test `essai_pilote` (2026-11-07): first enemies brainless in 2.2 s, median position gap 0 over 60 samples,
  host silent: 0 still brainless after 3 s; host back: 6 taken in 1.4 s; killed on the host, dead on the client.
- **v19, real weapon fire on the client.** For each host shot the host sends its kind (at the partner, at the host,
  other, melee) and the aim point relative to the target's centre (feet + 90), from the real trace start
  (`XPawn.GetWeaponStartTraceLocation`, eye height: 105 above the standing centre, 45 crouched). The client calls
  `XWeapon.StartFireTowardLoc` then `StopFire` in the same frame: one bullet. What it took:
  - an AI weapon **ignores** the point given to `StartFireTowardLoc`, `AimLocation`, the accuracy model's
    `ModifiedAimLocation` / `ModifiedAimDirection` and the controller rotation. Suspended, it fires at the point its AI
    last aimed at;
  - it fires along the **animation aim tracker**, `ActionModel.AimTracker.CurrentRotationWorld` (and `…WithSaccade`),
    **and its child trackers** (`ChildTracker`; without them one trace in three still went to the old target). They
    are steered every frame from the real trace start;
  - the accuracy model (`bDisableAccuracyModel`) and weapon spread (`SpreadModifierAttrib.CurrentValue`) are off
    during the shot: the host's spread is already in the aim point;
  - a per-shooter bias (animation) is corrected shot by shot: half of the measured direction error is removed from the
    next shots;
  - the weapon refuses a shot too close to the previous one (fire rate): retried on the next frames (up to 8), then
    played as effects;
  - a baton fired this way moves without hurting, so melee damage stays host-decided.
  Results (`essai_pilote`, 2026-11-07): 197 native shots, 0 refused; client/host bullet ratio 0.81 (0.73 to 0.84 over
  the 2026-11-06/07 runs); aim offset by 300 units with a test file: median 22 units off (13 to 36 over recent runs);
  aim point under the floor: 34 of 34 bullets stop at the floor. Remaining aim error: 15 to 35 units at 15 m.
- Damage: the client's own game now decides hits, with the real hit volumes. About half of the bullets fired at the
  client's player land (62 % since the child trackers), against about 75 % "hits" counted on the host with a 40-unit
  cylinder. The client's player loses about 0.35x what the host counts (2214 for 6344 in the last run; 0.33 to 0.38
  over recent runs).
- **v20, crouch.** An enemy crouched on the host (`bIsCrouched`, 5 to 12 % of the time at the station) is crouched on
  the client. The host's cylinder shrinks (135 to 75) and its centre drops 60; the client's cylinder stays standing,
  so its centre is raised 60 (measured: 60 exactly, feet on the ground). Agreement 33 of 34 samples at the time,
  12 of 12 in the last run.
- Fallbacks in `coop.ini`: `ennemis=pilote` (v18 behaviour), `ennemis=cible` (v17 only), `ennemis=jumeaux` (before
  v17). Automatic fallback to v18 if a weapon never fires in 40 tries or throws.

### Derived

- Shots aimed at an "other" target (possessed enemy, decoy) stay effects only. Client bullets can hit another enemy on
  their way (AI friendly fire, as in the game), and that death then comes from the client.

## Turrets (2026-11-03, packets v21 and v22)

### Observed

- Destroyed on the host (wreck, `ReplicatedTurretState` 6, `CurrentHealth` 0), the station turret stayed intact on
  the client and kept firing. Writing health 0 or calling `KilledBy` does not destroy it; only real bullets do.
- v21: each copy lists its `XTurret`s (same path on both sides; at world arrival, then every 10 s) and sends the dead
  ones twice a second (`BICT`). The other side writes `CurrentHealth` 0, `ReplicatedTurretState` 6, then calls
  `XReplicatedEvent ReplicatedTurretState` (the update written for the game's own netcode): dome off, no target, no
  burst. No explosion or wreck on that side. Test `essai_tourelle`: switched off in 1.2 to 1.4 s at the time, 0.1 s in
  the last run (2026-11-07).
- v22: Possession moves the turret to the players' side for about 21 s (`FactionID` 3 to 1, `MesmerizedByPawn`,
  `HackingData.bHacked`), then the game restores it. `BICT` now also carries possessed turrets and their faction; the
  other copy writes the faction, and restores it when the partner stops announcing it (or after 3 s of silence).
- No double damage: bullets from the mirrored turret hit without hurting. An enemy whose `XPawn.LastDamager` is that
  turret gets back the points it had the frame before. (`TurretDamageModificationInfo.StandardDamageModifier` = 0
  changed nothing.) Copying the state flags (`StateFlags` 0x280000) crashed the copy at the first bullet the turret
  received (the game looks for the absent caster), so the green possession veil is not mirrored.
- Test `essai_possession` (2026-11-07): possessed in 0.2 s, mirrored in 0.4 s, targets enemies only, the enemy placed
  behind the turret kept 295 of 295 points, 78 hits cancelled, faction restored in 0.3 s. Two earlier failures in
  full runs (the placed enemy lost 15 to 16 points) are unexplained; the probable cause (a stray shot from another
  enemy) is not proven. The client-to-host direction was last run on 2026-11-03.

## Enemy actions replayed on the client (2026-11-04 to 2026-11-05, packets v23)

### Observed

- Enemies are animated by a Morpheme network (`XMorphemeNetwork`). Logging each change of `CurrentlyActiveSequences`
  over 30 s of combat showed that the brainless client enemy walked, crouched and fired like the host's, but reloads,
  side steps, `MoveToFire_*` manoeuvres, taunts, baton attacks and bullet reactions only played on the host.
- The host now takes each enemy's action sequence every frame (anything but locomotion, crouch, aim, fire, death and
  additive clips) and sends a hash of "set:sequence" and an action counter; `BICX` carries 18 enemies instead of 20.
  The client plays it on the network's Matinee slot (`AnimNodeSlot.PlayCustomAnim` by sequence name) and stops it
  with `StopCustomAnim`. Sets the client mesh does not have yet (its AI never used them) are grafted into `AnimSets`
  first. Checked on bones: during a replayed reload the left hand goes from 86 to 33 units from the pelvis.
- Root motion: clips on the slot keep their root translation in the pose, so the body left its pawn (95 % of samples
  under 233 to 281 units during gestures, up to 589, against 37 / 134 on the host). The mesh already ignores root
  motion (`RootMotionMode` 2) and the Morpheme network does not extract it (`OutputRootMotionTranslationX/Y/Z` false;
  setting them true changes nothing). **`AnimNodeSlot.SetRootBoneAxisOption(1, 1, 0)`** (discard X and Y, keep Z)
  works: 74 units (max 134, like the host) against 281 (max 554) without. It only applies to the slot's current child
  (`CustomChildIndex`), and the slot alternates between two children when gestures follow closely, so both are set
  before each gesture and reset to 0 when the enemy goes back to its AI. `bForceDiscardRootMotion` on the mesh also
  works, but for the whole body.
- Tried and dropped: playing the host's walk on that slot too. The slot replaces the whole body (a crouched, aiming
  soldier stood up and ran with dangling arms).
- Test `essai_actions` (2026-11-07): 61 actions sent, 55 played, 0 unknown, 0 refused, 7 sets grafted, slot seen
  playing on all 6 enemies, 95 % of gesture samples under 81 units (max 132). Over the 2026-11-06/07 runs: 27 to 61
  sent, 21 to 55 played.

## Enemy locomotion, stops and turns on the client (2026-11-05 to 2026-11-07, packets v24 to v26)

### Observed

- An enemy's legs do not follow its velocity. Its action model picks starts, walk, run, backward walk and stops from
  its movement model (`XPawn.ActionModel.MovementModel`), which the AI keeps up to date. Suspended, the model stayed
  stale: the old path recomputed every frame, never "arrived", forward pedal. Over 90 s of combat: 148 to 228 starts
  and as many stops on the client against 16 to 36 on the host, and no backward walking.
- **v24, gait.** The host sends a gait byte per enemy (pedal type, 2 = backward; move type; arrived; intent). Every
  frame the client writes on the suspended enemy: idle on the host means `HasArrived` and `bMovementIsFrozen`; moving
  means `bManualSteeringMode` toward a point 1 s ahead along its velocity, plus the host's pedal, move type, intent
  and `bManageSmartCombatTrackPedalMoveType`, and the pawn's acceleration. Its yaw slides toward the host's instead of
  being set 15 times a second (the steps were read as turns). Test `essai_marche`: starts 26/20 and stops 21/40
  (host/client) at the time; without gait (kill-switch file), 36/176 and 31/178, and the test fails. Tried and
  dropped: writing `DesiredRotation` (more turns on the spot).
- **Turns on the spot** (`TurnInPlace`) were left: 29 to 74 in 90 s on the client against 0 to 9 on the host.
- **v25.** Cause 1: the face tracker target (`ActionModel.FaceTracker`, `TrackTargetEntries`). On the host these
  enemies have none; on the client they kept the one their local AI picked before suspension (the local player), and
  the action graph turned them toward it. The client now clears it (null actor, as on the host) and gives it back to
  the AI. Disabling the entry (`bIsEnabled`) made it worse. Cause 2, late stops: on the host, acceleration drops to 0
  when the stop sequence starts, at full speed; `HasArrived` comes 0.15 to 0.3 s later or never. The host now sends a
  "braking" bit; the client treats the enemy as idle as soon as it is set. Stops play 0.03 to 0.06 s after the host
  instead of 0.53 to 0.57. Unmatched client transitions (starts, stops, turns with no host counterpart within
  ±0.7 s, three 90 s fights, measured with a tool outside the repo): 119 with nothing, 91 with the target cleared,
  62 with braking too.
- **v26.** The turn rule, from the disassembly (below): with a steering direction (`ActionContext.DesiredSteeringParams`
  non-zero), turn when the heading is more than `MoveDirectionTurnTresholdDeg` (30°) off. That property belongs to the
  action, is shared by all enemies of a copy, and is **read live**. Without a steering direction, turn toward the face
  target, else the aim target, else the look target, using the target's `XAnimationTrackerTargetEntry.TurnToThresholdOverride`
  (+0x58) when positive (it is -1 everywhere), else the action's threshold (aim 50°, look 65°). Now:
  - the host sends a signs byte: `SignePivote` while the enemy plays a `TurnInPlace` sequence, `SigneFreine` while its
    acceleration is 0;
  - while any enemy is steered, the moving threshold of the client's `XAction_TurnTo` is 180° (restored after);
  - each steered enemy's aim and look targets get a 180° override, except while the host's enemy pivots, when it
    pivots too; -1 again when it goes back to its AI;
  - braking holds as long as it lasts: with zero acceleration the host enemy does not walk, even while moving (it stops,
    or an animation carries it: slow slides of 59 to 121 units/s on the idle pose; 474 samples with zero acceleration
    and steady speed, never a walk sequence; 301 with acceleration, always one). The client keeps it still as long as
    the host does not accelerate; the 1 s limit only applies to "arrived" alone.

  | Version | Turns | Stops | Starts | Total unmatched |
  |---|---|---|---|---|
  | v25 | 27 | 36 | 11 | 74 |
  | v26 without the turn block | 18 | 39 | 17 | 74 |
  | v26 with the block, braking limited to 1 s | 2 | 23 | 7 | 32 |
  | v26 | 1 | 2 | 2 | 5 |

  Stops play 0.04 to 0.06 s after the host (medians). Tried and dropped: a 1° threshold while the host pivots (11
  against 5: extra turns); a steering goal 6 or 10 m ahead like the host's; stopping on "arrived" alone.
- Test `essai_marche` (2026-11-07): starts 25/19, stops 19/17, turns 6/1 (host/client), all within bounds; the host
  walked backward only twice, so the backward criterion did not apply.

### Disassembly (Ghidra 12.2, exe without ASLR, base 0x400000, addresses valid in the running game)

- `FUN_00bfb790`: validation of `XAction_TurnTo` (rule above).
- `FUN_00bf7c60`: validation of the start transitions `XAction_IdleMoveTransitionState`. Its thresholds
  (`IdleToMoveAllowedYawOffsetDeg` 30°, `MaxHeadingYawOffsetDeg` 110°) are shared by all enemies of a copy and
  precomputed as cosines at load, so writing them later does nothing; not touched.
- `FUN_00bddfc0`: action model update (`XActionModel`). Calls `FUN_00a197f0` (movement model update), then
  `FUN_00bd81f0`, which fills the action context (`ActionContext` / `QueryContext`).
- `FUN_00a13820` writes the remaining path distance (+0x2E4): `FUN_00a12730` (the larger of the Kynapse path and the
  straight line to `TargetLocation`) if `HasArrived` or `FUN_00a123a0`, else 0. It only chooses walk or run near the
  goal (`MinDistanceToRun` +0xB8, `CurrentForcedWalkDistance`), so the stale local path was not the cause of the
  turns.
- `FUN_00a10620` copies only the move type into the context.
- `FUN_00964740`: the lower-body turn rule (`InPlaceTurn` beyond `InPlaceTurnThresholdDeg`, 60°, from
  `TPLowerBodyFacingYaw2B`), called from the pawn tick `FUN_00974140`, but not for these enemies.

### Not covered

- The client replays only part of the host's turns (4 of 7 over three fights; the heading still follows), and
  backward walking is only partly mirrored (`essai_marche` over the 2026-11-06/07 runs: 0 to 10 backward walks on the
  client for 2 to 20 on the host, for example 10 for 20 and 2 for 8).
- One stop seen about 0.3 s late once.

## Automated suite (2026-11-02 to 2026-11-07)

### Observed

- `outils/essai_tout.py [--complet]` runs everything on two copies on one PC, driven through the bridge pipes, with
  one relaunch per save (`TWN3` is relaunched once more before the enemy-position measurement): `BW2` (Battleship Bay) for `essai_tirs` and `essai_relance`; `TWN2` (Columbia, no enemies)
  for `essai_synchro`; `TWN3` (station, 6 enemies and a turret) for `essai_arme`, `essai_ennemis`, the
  `ecart_ennemis` measurement, `essai_cible`, `essai_pilote`, `essai_tourelle`, `essai_possession`,
  `essai_actions` and `essai_marche`. `--complet` adds `essai_coop` (circle). Each test writes
  `local/essais/<timestamp>_<name>/resultat.json`; the suite prints one line per step. A `--complet` run takes about
  24 minutes (its docstring still says ten). 409 run folders exist, 396 with a result.
- Last full run (2026-11-07): all 12 tests OK in their saved results (the suite's own 14-step summary line is printed,
  not saved). Nothing has run since. Of 9 full runs reconstructed from the folders
  since 2026-11-04, 2 were fully green, the latest included.
- **Test files.** In test copies, a `.essai` file dropped next to the DLL switches a fix off (`sans_allure`,
  `sans_pivot`, `sans_surplace`, `sans_face`, `sans_arret`, `freine_limite`, `jumeaux`, next to the client DLL) or
  injects a condition (`muet` next to the host DLL silences it, `decale` offsets the enemies' aim point, `mort`,
  `rail`). The kill switches were used to check that a test fails without its fix (for example `essai_marche`: 36/176
  starts host/client without gait sync, `essai_actions` 281 units without the root-motion fix).
- **Mostly a flaky test, not a flaky mod.** `essai_synchro` failed 23 of 52 recorded runs between 2026-11-02 and
  2026-11-07 (some early ones on 2026-11-02/03 because the puppet was missing in copy 1). The failures examined (6 of 9
  runs from 2026-11-05 to 2026-11-07) all came from the test's route, with the puppet following the player: the
  player strafed into obstacles at a spot that changed from run to run,
  then got stuck or ran into a wall (the sudden stop counted as a puppet teleport), then jumped under an obstacle (8 to
  63 units high instead of 200, no fall animation). Rewriting the view every loop also halved the input rate (92
  units/s instead of about 480 when turning). Fix: reset the player to a fixed start and heading before each move
  group, write the view at most every 30 ms, compare jumps over a 0.45 s window, and require at least 10 moving
  samples before counting stutter. 14 recorded passes in a row since (all on 2026-11-07). The mod did not change.
- The bridge sometimes reads the puppet pose as (0, 0, 0) (once: 68,091 units off, which pushed the circle mean from
  about 5.5 to 407); such samples are discarded.
- In the suite, `essai_coop`'s new-game step never completes (the copies are already in game): it waits 240 s and the
  verdict ignores it.
- Owner-file protection: every test hashes the Steam Cloud folder for app 8870, `My Games` and key install files
  before and after, and fails on any difference; `remotecache.vdf` is excluded because Steam rewrites it at every
  launch. Since the protection exists, one recorded run listed changed owner files: a hand-played session with the bot
  on 2026-10-30, where real save files and the owner's `XEngine.ini` changed. Its cause is not documented. No change
  has been recorded since.
- Reverse engineering: portable Ghidra 12.2 with JDK 25, driven headless by batch files (`analyzeHeadless` import,
  then a read-only search script), outside the mod repo.

### Not covered by any automated test

- Vigors and projectiles, friendly fire (one hand test on 2026-10-31), skyline, the MULTIPLAYER menu, collisions,
  no-pause on alt-tab, a mod-version mismatch, tick-exception recovery, real shielded enemies, Steam P2P and IP modes
  (last run 2026-10-30), two PCs or two Steam accounts, and the player package (still v0.12, built 2026-10-31).
- `ecart_ennemis.py` prints only; it has no recorded history.
- The turret and possession tests have been run host to client only since 2026-11-04.

## Not done yet (2026-11-08)

- No bullet tracer; no arm gesture for shots or vigors; the aim pose only plays standing still (fixed shoulder offset
  while walking).
- Possession of human enemies is not mirrored (only turrets); Undertow and Bucking Bronco on enemies are visual only;
  Charge has not been looked at (nothing in the code or the notes).
- Turrets: same target on both sides, explosion and wreck on the side that did not destroy it, green possession veil.
- The puppet appears about 35 s after arriving in game (Booker mesh preload). At Battleship Bay it also waits, with no
  time limit, for the extras' archetype, which only loads when the player walks toward the promenade (83 s in the
  test).
- About 9 to 22 % of the enemy actions the host sends are not played on the client (`essai_actions`, 2026-11-06/07:
  21 to 55 played for 27 to 61 sent, none refused or unknown); the reason is not documented.
- Not tested between two PCs or two Steam accounts, or in levels other than the test saves; no hand-played session
  is documented for packets v16 to v26. The player package is still v0.12 and cannot talk to the current build.

## Open questions

### Guesses (not yet checked)

- A second copy that travels to `127.0.0.1` may get through the control-channel handshake, but no actors
  would replicate (see the static check).
- Writing `UserSetTargetArray` at +0x210 on `TracerThirdPerson_par` instances gives bullet tracers.
- The client player takes less damage than the host estimates (about 0.35x) because the host counts hits in a wide
  cylinder; whether the game feels easier for the client is not checked in real play.

### Answered since

- A second copy runs on the same PC and Steam account: yes, with `-AllowMultipleInstances` (see "Two copies
  on one PC").
- `CPF_Net` did not survive cooking: set on 0 of 36,042 properties (static check above).
- Third-person beam ends can be set from script without `SetBeamTargetPoint`: write the emitter instance's
  `UserSetTargetArray` at +0x210 (Shock Jockey arcs and bolt, 2026-11-01).
- Enemies can target the partner without a second `XHuman`: the puppet as an `XProtectionObjectiveActor` with
  `FactionID` 0 and `bTargetableForAI` (2026-11-02). The second `XHuman` crash comes from `XPawn.ActionModel` never
  being created with a player archetype.

### Unknown

- Whether `XGameInfoSPChallenge` (DLC challenge mode) or a leftover MP GameInfo class can be the server's game type.
- How Elizabeth, Songbird and the scripted sequences behave with two players in the same world.
- How the jitter buffer, the 2 s silence purge and the 1 s enemy fallback behave with real internet jitter and loss.
- Why the enemy placed behind the possessed turret lost 15 to 16 points in two full runs.

## Next steps

Status overview: [README.md](README.md).

1. Rebuild the player package for packet v26, then test between two PCs and two Steam accounts, with network loss
   and latency (`playbook/network-chaos-testing.md`).
2. Test in other levels and by hand: enemy damage felt by the client, skyline thresholds on a real ride, collisions,
   menu and alt-tab with the new puppet class.
3. Automated tests for vigors and friendly fire; run the turret and possession tests client to host again; record
   `ecart_ennemis` results.
4. Shared story state: doors, scripted events (`ServerCauseEvent`/`ServerRemoteEvent` names show what the
   cut co-op synced), level changes, then Elizabeth.
5. Publish the mod repo (it builds on the owner's private bridge sources: ask first), then the directory
   entry and a "claim a game" issue (owner's GitHub account, owner's OK first).
