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
that `-coopinstance=<i>` gives each copy its own pipe `\.\pipei_sonde_<i>`.

### Observed

- `tools/gamepool` `provision --n 2` built two copies in seconds, each with 11,347 hard links. Private copies:
  `d3d11.dll`, the bi_sonde files, `8870_install.vdf` and both `GlobalShaderCache-PC-D3D-SM*.bin`.
- Arguments: `-nomovie -windowed -ResX=960 -ResY=540 -NOHOMEDIR -AllowMultipleInstances -coopinstance={i}`.
  - `-AllowMultipleInstances` is a switch the start-up code reads (`FUN_00506e40`, before the
    `AnotherInstanceRunning` message), so no patch is needed.
  - `-NOHOMEDIR` (`FUN_004d6aa0`) makes the copy create its `X*.ini` in its own `XGame\Config`.
- Both copies ran together next to CS2, answered on their own pipes, and each started a new game
  (`StartTravel Master_P` through the bridge, the modal screens closed by `bi_pilote.fermer_modales`).
- SHA-256 of the install files (exe, `d3d11.dll`, `DefaultEngine.ini`, `XCore.xxx`, shader caches) was the same before and after.
- **But the owner's `Documents\My Games\BioShock Infinite\XGame\Config\XEngine.ini` changed.** It was rewritten
  at 01:54:50, two seconds after copy 1 wrote its own config (01:54:48). Its `XUserOptions.ini` was rewritten
  in the same second with identical content. The owner's `ResX/ResY` (2560x1080) and `UseVsync` are still there;
  what changed is unknown (no earlier copy of the file).

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
- The writer of the first incident (owner's `XEngine.ini` at 01:54:50) was not reproduced. It stays unknown, but the
  game process can no longer write there.
- The guard's stack capture (`RtlCaptureStackBackTrace`) returns no frames inside the exe, probably because it is built without frame pointers.

### Saves go to Steam Cloud, not to files

- `Steam\userdata\<account>\8870\remotecache.vdf` lists the game's cloud files: `profile.bin` and
  `SaveData/S_<map>-0_<date>.sav/.bkm`. They are stored in `Steam\userdata\<account>\8870\remote`.
- The first two-copy run (new game in both copies at 01:56) **wrote a save and the owner's `profile.bin` into that
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
  `GetFileCount` (18) = 40, which is the number of files in the owner's cloud folder. `GetFileNameAndSize` (19) gave
  `profile.bin` 3717 bytes and `SaveData/...` names, and `GetFileSize` (15) and `FileExists` (13) agreed with them.
- New game in copy 0: the game called `GetFileSize profile.bin` (0, empty cloud), then
  `FileWrite profile.bin` (3518 bytes) and `FileWrite SaveData\S_Light_P-0_<date>.sav/.bkm` (105421 bytes each). It used
  no async or stream calls. All three files are in the copy's `SaveCloud`.
- The owner's `Steam\userdata\<account>\8870` folder was hashed before the run (41 files): it was identical after the
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
- Side note: saves and the owner's `XEngine.ini` changed at 03:22, 03:26 and 03:27 because of another session of
  the owner that ran the real game through `bi_pilote` (1280x720 windowed). The test copies' saves were all in
  their own `SaveCloud`. The 01:54:50 incident may have the same cause (not checked).

### Not done yet

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

## Open questions

### Guesses (not yet checked)

- A second copy that travels to `127.0.0.1` may get through the control-channel handshake, but no actors
  would replicate (see the static check).
- Script classes may have lost their `replication` blocks or `CPF_Net` flags in cooking.
- Floating sections (the boat, the islands: per-section local-to-world matrices) are BioShock-specific
  and probably not handled by the stock UE3 movement replication.

### Unknown

- Whether a second copy can run on the same PC and Steam account.
- Whether `XGameInfoSPChallenge` (DLC challenge mode) or a leftover MP GameInfo class can be the server's game type.
- How Elizabeth, Songbird and the scripted sequences behave with two player controllers.

## Next steps

1. Option 1 (own sync layer). Start from the `RepNotify` list above: player pose (with the floating
   section index), weapon fire, health, skyline state. Check whether writing a `RepNotify` property and
   calling the actor's `ReplicatedEvent(<name>)` through `bi_sonde` plays the effect on the remote copy.
2. Client connection test (lower priority now: it can only show the handshake). A second running copy needs a separate install copy (the bridge's pipe
   name `\\.\pipe\bi_sonde` is fixed, and the playbook wants isolated test installs) and probably a
   per-instance pipe name in `bi_sonde`. Owner's OK first: about 46 GB, or a smaller copy with links to the
   big read-only packages.
3. Open a "claim a game" issue (owner's GitHub account, owner's OK first).
