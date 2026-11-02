# Fable Anniversary notes

Goal agreed with the owner: online co-op, one PC per player, Steam relay (no port
forwarding). First milestone: player 2 visible and walking in the same world, enemies
not synced.

## 2026-10-31: triage

### Observed

- Store: Steam, AppID 288470, build id 434572, vanilla (owner confirms no mods).
- Main exe `Binaries/Win32/Fable Anniversary.exe`: 32-bit, 54,301,696 bytes,
  FileVersion 1.0.8767.0 (the UE3 engine version), PE timestamp 0x545d058c (2014-11-07).
  SHA-256 `2a95eea3c2cce9b47ca0f454a605b6952216f5d25158efd12ba48b70130989f2`.
- `steam_api.dll` SHA-256 `025a1fd2b3e81a1a3d474b1707e3696cbc1a0ff2dee65c4f74b186e688f01ec9`;
  interfaces SteamClient016, SteamNetworking005, SteamUser017, SteamMatchMaking009.
  No SteamNetworkingMessages / SteamNetworkingSockets.
- Directory and `games/`: no Fable entry. GitHub issues and PRs searched for "fable": none.
  Web: no co-op project for Fable Anniversary
  ([Steam discussions](https://steamcommunity.com/app/288470/discussions/0/613936039452164692/),
  [ModDB](https://www.moddb.com/games/fable-anniversary/mods)). The official modding
  DLC covers art and animation only
  ([PC Gamer](https://www.pcgamer.com/uk/fable-anniversary-will-support-modding-with-included-unreal-engine-3-editor)).
- Scope: Fable Anniversary (Steam) only. Other Fable games are out of scope and are not
  used as sources.
- Layout: `Engine/`, `WellingtonGame/` (UE3 project, codename Wellington),
  `WellingtonGame/FableData/Build/` (original Fable data: `Data/Defs`, `CompiledDefs`,
  `Levels`, plus a leftover `cdx.xbe`).
- UE3 script packages in `CookedPC`: Core, Engine, GameFramework, IpDrv,
  OnlineSubsystemSteamworks, GFxUI, AkAudio, WinDrv, WellingtonGame.u (413 KB,
  package version 860, licensee 0x6969). Contains `class HeroPawn extends WellingtonPawn`.
- `DefaultEngine.ini`: `NetworkDevice=IpDrv.TcpNetDriver`,
  `OnlineSubsystemMgrClassName=WellingtonGame.WellingtonOnlineSubsystemMgr`.
  `DefaultGame.ini`: `DefaultGame=WellingtonGame.WellingtonGameInfo`, `MaxPlayers=16`.
- Exe RTTI: 3,983 `.?AV` type descriptors, most from the original Fable C++ engine:
  `CThingPlayerCreature`, `CPlayer`, `CPlayerManager`, `CGame`, `CMainGameComponent`,
  `CWorld`-side classes, quest scripts as `NScript::CQ_*Script` classes,
  and `CNetworkServer` / `CNetworkClient` next to `CGame` and `CMainGameComponent`.
- Exe imports: `DINPUT8.dll`, `XINPUT1_3.dll`, `d3d9.dll`, `steam_api.dll`,
  `EasyHook32.dll`, `WSOCK32.dll`, `MSVCR100.dll`; delay-loads `d3d11.dll`, `dxgi.dll`.

### Derived

- Engine: hybrid. The original Fable engine (C++, compiled into the exe) runs the
  simulation, AI and quests; UE3 renders, and `WellingtonPawn`/`HeroPawn` mirror Fable
  things. Game logic is native, not UnrealScript (WellingtonGame.u is small).
- UE3 replication is unlikely to help directly, because the world state lives in the
  Fable simulation, not in replicated UE3 actors.
- Hook point: a 32-bit `dinput8.dll` proxy (imported statically) with MinHook.
- Transport: both players own the game, so the game's own AppID and
  `ISteamNetworking::SendP2PPacket` (SteamNetworking005, relayed by Steam) should work
  without Spacewar. Not tested yet.

### Unknown

- What `CNetworkServer` / `CNetworkClient` do: a real client/server split inside the
  Fable engine (a possible second-player path) or leftover/loopback code. Next: Ghidra.
- How a Fable thing gets its UE3 pawn (needed to spawn a puppet hero).
- Whether the UE3 console can be enabled in this shipping build.
- Whether two copies can run on one PC (single-instance check, Steam).

## Workspace

- Isolated copy of the install (identical exe hash); the owner's install is untouched.
- Tools: Ghidra 12.2 (release zip SHA-256 `2c2cd776...a7be67`, checked), Temurin JDK 25,
  x64dbg, MSVC Build Tools 14.44.
- Ghidra headless analysis of the exe running; project and dumps in `local/` only.
- Pitfalls hit: Ghidra 12.2 needs JDK 25 (not 21). Launched from Git Bash, headless
  aborts in Felix (`The data file must be inside the data dir`); launched from
  PowerShell it runs. The VS Build Tools bootstrapper fails with 5002 if its own
  `BackgroundDownload.exe` still holds the Installer folder; stop it and retry.
- Headless Ghidra's default heap (2 GB, `MAXMEM_DEFAULT` in `analyzeHeadless.bat`) is too
  small for this 54 MB exe: aborted after ~20 min with `OutOfMemoryError: Java heap space`
  and saved nothing. 12 GB with `-max-cpu 8` also ran out after ~35 min (heap 99.6% full,
  abort inside `AddressTableAnalyzer`). Third run: 16 GB, `-max-cpu 4`, all analyzers on.
- Claim issue opened: https://github.com/cranebeaker/coop-anything/issues/280
  (the template's `in-progress` label could not be added from this account).

## 2026-10-31: proof of life

### Observed

- The exe imports exactly one function from `DINPUT8.dll`: `DirectInput8Create` (dumpbin).
- A 32-bit `dinput8.dll` proxy (MSVC 14.44, forwards to the system DLL) placed next to the
  exe in the test copy loads at startup; the game reaches its window
  "Fable Anniversary (32-bit, DX9)" with the proxy in place. Log:
  `proxy loaded ... exe base 00DA0000`, then `real DirectInput8Create` resolved ~6 s later.
- Exe base was 0x00DA0000, so ASLR is on: use RVAs or byte signatures, never absolute addresses.
- `steam_appid.txt` (288470) next to the exe lets the test copy start under Steam without
  being relaunched from the Steam install.
- A second copy of the game started while the first runs exits within 30 s with code 0:
  a single-instance check.
- Cause, found by hooking the exe's `CreateMutexW` import: the game creates the named mutex
  `UnrealEngine3_8`; the second copy gets error 183 (already exists) and quits.
- Fix checked: the proxy renames that mutex per process (`UnrealEngine3_8_<pid>`). Two
  copies then run side by side, both reaching the "Fable Anniversary (32-bit, DX9)"
  window (40 s each, test copy, same Steam account). Gameplay in two copies not tried yet.
- The exe does not import `FindWindowA/W`.

## 2026-10-31: targeted reverse engineering of the Fable "network" classes

Method: no full auto-analysis (three headless runs ran out of memory at 2, 12 and 16 GB
budgets on this exe; the owner chose targeted work instead). A Python RTTI walker
(MSVC x86 TypeDescriptor -> CompleteObjectLocator -> vftable, bases, and code writing the
vftable) finds a class in under a second; Ghidra imports the exe with `-noanalysis` (~3 min)
and decompiles only the requested addresses plus their direct callees (seconds per run).
Addresses below are VAs at Ghidra's default image base 0x00400000 (the game runs with ASLR).

### Observed

- RTTI hierarchy: `CNetworkServer : CInitBaseClass : CBaseClass` and
  `CNetworkClient : CInitBaseClass : CBaseClass`, 2 virtual slots each (scalar deleting
  destructor, shared empty method). `CMainGameComponent : CGameComponent :
  CBaseClassNonCopyable : CBaseClass, CDeviceResetCallback`. `CThingPlayerCreature` derives
  from `CThingCreatureBase : CThingGameObject : CThingPhysical : CThing : CThingBase`
  (99 virtual slots).
- One constructor (0x1cc9d7f) builds both: a `CNetworkClient` with a `CNetworkServer`
  embedded at +8. It is called only from the `CMainGameComponent` constructor, which
  stores the object at `CMainGameComponent+0x13ABC`. 18 instructions reference that offset.
- Methods called on that object (decompiled):
  - 0x1cdbc70: start (sets active flags at +0x2660/+0x2662, resets counters, stores a param).
  - 0x1cdbcc0: add a 40-byte message to a per-tick list (max 40, at +0x2018, count at
    +0x2014). Messages are keyed by (int at +0, byte at +4); a flag at +0x27 replaces
    an existing message with the same key.
  - 0x1cdbce0: store three values (+0x266c tick counter, +0x2670, +0x2674).
  - 0x1cdbe40: once per tick (gated by 0x1cca139, a fixed-timestep "is a tick due" test
    on the game clock), append a message of type 4 holding +0x2674 and the tick counter,
    increment the counter, then copy the whole message list (0x648 bytes) into a queue
    at +0x2010 (0x16dc100).
- Message payload writer 0x16dc150 appends bytes at +5 with the length byte at +0x25.
- None of the functions read so far calls Winsock or Steam; they only copy memory. The
  exe's socket imports (`WSOCK32.dll`) are used by UE3's `IpDrv` (strings show
  `UTcpNetDriver`).

### Derived

- `CNetworkClient`/`CNetworkServer` look like an in-process, per-tick message pipeline
  between the input side and the simulation, not real networking: input-side code queues
  up to 40 small messages per tick, the tick boundary stamps them with a tick number and
  hands the bundle to a queue the simulation side reads.
- If player input reaches the simulation only through these messages, they are a seam
  for co-op later (forward one machine's messages, or inject a second player's). Not
  needed for the first milestone, which only shows a puppet.

### Unknown

- Which message types exist and whether player movement/actions go through them
  (next: the single caller of 0x1cdbcc0 at 0x1cc9ede, and the consumer of the queue).
- Whether the Fable simulation is deterministic (would matter for lockstep).
- `CPlayerManager` / `CPlayer` roles (decompiled, not read yet).

## 2026-10-31: profile isolation, bridge, hero position

### Observed

- Saves: one 96 KB file `%USERPROFILE%\Documents\My Games\FableHD\Saves\Fable Anniversary`,
  synced by Steam Auto-Cloud (listed in the Steam `remotecache.vdf` for 288470). The exe
  finds Documents through one import, `SHGetFolderPathW`.
- Fix checked: the proxy answers `CSIDL_PERSONAL` with `<game dir>\CoopProfiles\<profile>`
  (`FABLE_COOP_PROFILE`, default `p1`). The game then creates `My Games\FableHD\Saves`
  there, and the real save file keeps the same SHA-256 across a run.
- `tools/bridge` (coopbridge) embedded in the proxy, started on the first
  `DirectInput8Create` call (not in DllMain). Endpoint file in the profile folder. Methods so
  far are read-only and answered on the transport thread: `fable.base`, `fable.instances`
  (scan private RW memory for a vftable), `mem.read`, `mem.floats`.
- In a new game (child hero in Oakvale), owner moving the hero between snapshots:
  - exactly one `CThingPlayerCreature`; one `CPlayerManager`; one `CMainGameComponent`;
    five `CPlayer` objects 0x280 bytes apart (an array of player slots?).
  - The hero object's own first 2 KB holds no position. Hero sub-objects named by RTTI:
    +0x06c `CTCPhysicsControlled`, +0x070 `NGraphicAppearance::CTCGraphicAppearance`,
    +0x07c `CThingCreatureDef`, +0x0a4 `IActor`, +0x15c `CTCCreatureModeManager`,
    +0x21c `CHeroCombatDef`, +0x220 `CPlayerMovementDef`.
  - `CTCPhysicsControlled` +0x00c = position (x, y, z), copied at +0x018; z is the height
    (follows the ground). +0x05c = facing (x, y unit vector), copied at +0x074.
    Two moves: (3497.9, 826.6, 15.4) -> (3507.3, 836.5, 13.6), facing (0.235, -0.972) ->
    (0.691, 0.723).
- Scanner pitfall: hits on the bridge thread's own stack (a value left from an earlier
  call) show up for every class; the stack filter added so far does not catch them all.

### Unknown

- `CTCPhysicsControlled` +0x0cc (a second position that moves little: a move target? the camera?).
- What the five `CPlayer` slots are for; whether `IActor` is the UE3 pawn.

## 2026-10-31: puppet for player 2 (spawn, game thread, driving)

Addresses are at the exe's preferred base 0x00400000 (Ghidra). Tools: `fable-coop/tools/puppet.py`,
proxy `fable-coop/proxy/coop.cpp`.

### Observed

- Quest scripts (`NScript::CQ_*Script` coroutines) reach the game through `CGameScriptInterface`
  (942 virtual methods, vftable 0x2ee35c4). Global pointer to the live instance: 0x35bbc34
  (2574 code references). Script objects keep it at +0x54.
- Slot 0x184 (0x1caf800) creates a creature: `__thiscall`, `ret 14h`, args
  (out `CScriptThing` 12 bytes, `const CCharString &def`, `const C3DVector &pos`,
  `const CCharString &scriptName`, bool). Seen in scripts as def + marker position + name,
  e.g. "CREATURE_RIVAL_HERO_WHISPER_APPRENTICE" at "MK_GTC_WHISSTART".
  `CCharString` is one pointer: ctor 0x16b7800 (`__thiscall (const char *, int len = -1)`),
  dtor 0x16b75d0. The returned thing's +4 is a `CGameScriptThing`.
- Game thread: `CMainGameComponent` vftable 0x2ee6e84 slot 5 (0x1ccc1fa, `__thiscall`, no args) is
  the per-frame update (contains the fixed-timestep check and the message flush). It is only
  reached through the vftable, so patching the slot hooks it. It does not run in the menus.
- Checked in game: `fable.spawn` with "CREATURE_OAKVALE_VILLAGER_MALE_UNEMPLOYED" at hero + 3 m
  makes a villager appear there (owner saw it; a new `CTCPhysicsNavigator` showed up within
  0.15 m of the requested spot). The game snaps it to walkable ground.
- NPC physics component: `CTCPhysicsNavigator` (vftable 0x2f079ac), same `CTCPhysicsBase` layout
  as the hero's: position +0x0c (copy +0x18), facing +0x5c (copy +0x74).
- Writing the puppet's position (+0x0c) every frame makes it follow the hero, but its walk/run
  animation plays far too fast, and it keeps wandering when the hero stands still (owner's
  video, 10 fps frame grids).
- "CREATURE_HERO_CHILD" spawns with a `CTCPhysicsControlled` instead, then the game hangs or
  crashes (access violation in UE3 code at 0x53bd6a, iterating a bad bit array). Twice out of
  two. Not usable as a puppet as is.
- `CAIBrain` (vftable 0x2eaf388, 0xb8 bytes, a `CSuspendableProcessFibre`: AI runs in a fiber),
  created by 0x1f365a0. brain+0x20 = owning creature. Slot 4 (0x1ad7700) thinks only while
  byte `[[brain+0x44]+0x10]` is 0. Checked in game: the object at brain+0x44 belonged to one
  brain only; setting the byte to 1 there made the villager stop following its own goals, but
  its walk action kept moving it and the forced position lost (stuck on collisions).
- `CTCPhysicsNavigator` slots (decompiled): 0x14c (0x1e760f0) adds a movement delta to an
  accumulator at +0x95; 0x90 (0x1e75ec0) queues a teleport at +0xa1 with flags at +0xad
  (bit0 pending, bit1 = second argument); 0x164 (0x1e76870) is the per-frame update: a pending
  teleport with bit1 clear is applied as is through slot 0x80 (set position), otherwise the
  accumulated movement goes through path and collision code first.

- 2026-11-01, checked in game: brain off + the navigator teleport slot every frame makes the
  villager follow the hero without getting stuck, but it slides (legs out of step with its
  speed). With a target 2 m behind the hero at the hero's height it also sank into or floated
  over slopes.
- Then: snapshot interpolation in the proxy (`fable.puppet` pushes timestamped samples; each
  frame the puppet is placed where the samples were `delay` s ago, linear between the two
  around that time). Test: the hero's own position streamed at ~20 Hz, replayed 0.7 s late.
  Owner: feet stay on the ground on slopes, catch-up slides gone, it still slides a little.
- The game has follow behaviours: `CAIStateGroup_FollowPlayer`, `_MinionFollowThing`,
  `_MinionWalkToMarker` (state groups made by a factory, 0x1adcfe9 / 0x1ade043), components
  `CTCFollowed`, `CTCRegionFollower`. Cutscene commands include `.FollowThing`,
  `.WalkUpToThing`, `.SneakTo`, `.EntitySetMaxRunningSpeed`, `.AddScriptedMode`, `.Teleport`
  (handlers around 0x17601ca-0x1764c2a); `.FollowThing` calls slot 0x1c of a cutscene actor
  object (thing, distance, true). `CScriptThing` (vftable 0x2e5cbf4, 80 slots) forwards every
  slot to the `CGameScriptThing` at +4; its slot 0x1c is GetPosition.
- `CGameScriptInterface` slot 0x7b8 turns an entity toward another (computes the angle, calls
  0x7b4); 0x7ac looks at a thing's position (calls 0x7b0). Not follow orders.

### Derived

- The fast animation and the fight come from the NPC's own movement (its action/brain) still
  running under the forced position.
- The remaining slide is animation, not position: the legs play the creature's own walk
  cycle, not one matched to the speed we move it at.

### Unknown

- How the NPC animation is chosen (action, speed, `CTCCreatureModeManager`), and whether a
  game move/follow order toward a moving point gives matched legs.
- What else the creature's actions do when its brain is switched off this way.

### 2026-11-01: the game's own follow order on the puppet

Observed:
- `NTCScriptedControl::CActionFollowCreature` (vftable 0x2ee9eec, ctor 0x1d0e630) and `CActionMoveTo`
  (0x2ee9f20) are queued by `CScriptGameResourceObjectScriptedThing` (vftable 0x2ee9f4c): slot 0x1c
  (0x1d0fa90, thiscall(target CScriptThing*, float distance, bool), ret 0xc) builds the follow action; slots
  0x10/0x14/0x18 build move-to variants. This is the "actor object" the cutscene `.FollowThing` calls.
- GSI slot 0x30 (0x1c9ec00, thiscall(CScriptThing*, out wrapper*, int priority), ret 0xc) creates that
  object and stores it at out wrapper +8; returns false while another script holds the creature. The
  trader escort quest calls it with priority 4 and retries every frame (GSI slot 0x2c = wait a frame).
- GSI slot 0x130 (0x1c90550, fastcall(gsi)) returns the hero's CScriptThing*.
- Live, called from the main-update hook on a fresh villager: begin scripting returned 1, the follow
  call returned, game frames kept advancing (`puppet.py p1 escort`, 45 s).
- Owner, watching in game: the villager follows and "walks naturally" (no foot sliding).
Unknown: how to release scripted control; whether a new action replaces or queues behind the follow.
- Actor slot 0x10 (0x1d0f460, ret 0x14) = move to: (float pos[3]*, float, int gait, bool wait, bool). Gait 0 → mode 3,
  1 → mode 5, other → 1 (derived: walk / run). `wait` makes it call GSI-style wait (slot 0x2c), so pass 0 outside a
  script fiber. The action is appended to the creature's queue (0x1eaf4e0 → 0x1eaf280).
- Actor slot 0x28 (0x1d0fee0, no args) tail-jumps to 0x1eb0cb0 on the scripted-control component; 0x1d107e0 and
  friends call 0x1eb0cb0 before queueing, so derived: it empties the action queue. Called live: no crash.
- `puppet.py p1 drive`: clear + move-to toward the hero's position 0.5 s late, re-sent when the target moved > 0.3 m,
  60 s, frames kept advancing. Owner, watching in game: the puppet moved smoothly (walk and run), and the first
  villager stopped following after the clear call (so slot 0x28 does empty the queue).

### 2026-11-01: move-to driving inside the proxy

Observed:
- `fable.spawn {scripted: 1}` takes scripted control (GSI slot 0x30) and returns the actor; `fable.puppet {phys, actor, ...}`
  replays samples `delay` s late and, from the main-update hook, sends clear + move-to at most every 0.25 s when the
  target moved > 0.3 m, run when the sample speed is > 2.4 m/s. `puppet.py p1 follow ... 60 0.5`: owner saw it move
  correctly, but a fixed 8 m teleport threshold fired 4-6 times in a row round corners and near walls, in plain view.
- Changed to: teleport at once beyond 30 m, or beyond 10 m for 3 s without getting 1 m closer. Same 60 s test:
  3 teleports logged, seconds apart (15.4, 28.8, 13.4 m off); owner: "mieux" (better), no more bursts.

### 2026-11-01: design goal from the owner: free co-op

Each player plays on their own PC with their own camera and may go anywhere, including a different town or region;
no shared camera and no leash to the other player. Consequences for the design (derived):
- The puppet stands for the other player's real position; it is not a follower. Large jumps (fast travel) are real.
- Samples must carry the region; the puppet exists on a PC only while both players are in the same region.
- Each region needs an owner for its creatures (see playbook/actor-ownership.md) once enemies are synced.
- Owner decision (2026-11-01): main-story quests are done together. Either both players must be near the quest
  start, or one player starts it and the other is teleported to them. Side quests and other optional activities:
  no constraint, players may be anywhere.
- Owner decision (2026-11-01): the game's quest card colours decide. Gold (story) quests: both players together.
  Silver and bronze quests: may be started and finished solo. All quest progress is shared: a quest completed by one
  player is completed for both.
- Open: where the card colour lives in the game data (Ghidra). Cross-check list: supersoluce.com Fable Anniversary
  walkthrough, which groups quests as histoire (or), secondaires (argent), annexes (bronze).

### 2026-11-01: two games on one PC, linked over local UDP

Observed:
- Two copies of the test install run side by side (profiles p1/p2, single-instance mutex renamed per process); both
  keep their frame rate in the background (72 and 82 frames/s measured through the bridge).
- The proxy sends the local hero's position at 20 Hz (`FABLE_COOP_LISTEN`, `FABLE_COOP_PEER`), and on receipt spawns
  the other player's puppet itself (villager, scripted control) and drives it with move-to orders, 0.1 s late.
- Reaching the bodies from the DLL: hero = GSI slot 0x130 → CScriptThing +4 → CGameScriptThing +0xc handle →
  0x16e6ea0 → CThing, +0x6c physics. Puppet = 0x16e6ea0 on actor +0x14 → CThing, +0x6c physics. Both checked live.
- Both puppets spawned at load; owner walked p1's hero left and p2's hero right in Oakvale, and each window showed
  the other hero's puppet going the right way. Milestone (a) holds locally.
- Region: UHUD_RegionNameDisplay::execGetRegionName → 0x20365a0 reads the hero's component 0x8e
  (CTCRegionDisplay); its region handle was empty in Oakvale, so that route is no region id. Still unknown.
Unknown: whether the puppet-gone check (handle returns 0) fires on a region load; jitter over Steam.

### 2026-11-01: current region

Observed (p1 stayed in Oakvale, p2 left through a loading screen; `fable-coop/tools/regionsnap.py` snapshots
CWorld / CWorldMap / CEngineWorldMap plus one level of pointers before and after, keeps fields equal in both
games before that changed only in p2):
- Global `0x3630374` → CWorldMap (vftable 0x2f2b48c). CWorldMap +0xbc = current map index (also +0xc0, and
  CWorld +0x1a0): 4 in Oakvale, 44 in p2's new region.
- CWorldMap +0x30 = map count (73), +0x34 = map table, stride 0x64, +0x2c = CRegionDef*. One region def per map,
  same table in both copies. Oakvale = map 4 = region def global id 0x1c5b (CRegionDef +0x20; +0x18 = index in
  its class, +0x14 = 0x115 class id). 94 CRegionDef objects in memory.
- The DLL's puppet-gone check fired on p2's region load (actor handle → 0) and respawned the puppet.
Derived: map index is the region id to send; it needs no name lookup.
- Region gating in the DLL (packet "FCO2" carries the map index). Two games on one PC, owner watching:
  p2 loaded the Heroes' Guild (map 44) → p1 logged "peer region 44, here 4: puppet parked" and p2's puppet vanished
  from p1's screen; p2's own puppet of p1 was removed by the load and not respawned. p2 reloaded an Oakvale save →
  p2 spawned p1's puppet, p1 logged "peer back" and teleported the parked puppet back (500 m off), visible again.
  The parked puppet sat 500 m under the map for ~2 min without being removed by the game.
- But p1's log then showed a teleport every frame ("500.0 m off", ~2900 times): the parked puppet's physics
  stayed at z -484 with the teleport queued (+0xad = 1) and never applied, even after writing the position by
  hand. Derived: a creature parked under the map is put to sleep; parking is not usable.
- Removal: the cutscene command `RemoveAllThings` (handler near 0x176686f) gets things by script name (GSI slot
  0x144) and calls GSI slot 0x1c8 (0x1c8ec40, thiscall(CScriptThing*, bool, bool)) with (thing, 0, 1) on each.
  0x1c8ec40 checks the thing, sets bit 2 of thing +0x9f when the second bool is set, then calls 0x1f2e530(first
  bool). The DLL now removes the puppet with it when regions differ or the peer is silent for 5 s. Test pending.
- Removal tested (two games on one PC): p2 went to the Guild → p1 logged "peer in region 44, here 4 … puppet
  removed"; p2 logged "puppet gone" (its puppet removed by the load). p2 reloaded Oakvale → both games logged
  "puppet spawned" in the same second.

### 2026-11-01: the game pauses when its window is not in front

Observed: with p1 in the background while the owner played p2, p1's puppet stood still: the move-to orders and
the stuck-teleport ("puppet teleported, 11.0 m off" every 3 s, same 11.0 each time) never took effect, and no
NPC moved in either game while both were in the background, though frames ran at 62/s. The owner clicked p1's
window: within 3 s the puppet walked to p2's hero (0.7 m off).
- `bPauseOnLossOfFocus=FALSE` in the Engine ini files: the pause is Fable's own code, not the UE3 setting.
- Callers of `GetForegroundWindow` (12) and `IsIconic` (5) in the exe; in Fable's range, 0x1f9f0a0 =
  "world paused?" (thiscall, object `[[0x362b3cc]+4]`): `IsIconic(hwnd 0x35a0fb4)` → background branch,
  `GetForegroundWindow() == hwnd` → in-front branch. In front it returns paused only if byte +0xa4, byte +0xa5
  or `[0x20527d0() + 4]` is set (+0xa4 written by setter 0x1f9f080 from menu code); in the background it returns
  paused unless `[0x2052720() + 0x58]` or `+0x3c` is set. Callers 0x1cca9a7, 0x1f9f199, 0x1faac62 skip the
  world update when it returns true.
- Setting +0xa4 to 1 in the background did nothing (it is a pause flag, not a "keep running" flag).
- Fix in the DLL: patch the two jumps so the in-front branch is always taken (0x1f9f0b1 `75 0e` → `90 90`,
  0x1f9f0bf `74 15` → `eb 15`), after checking the original bytes.
- Tested (two games on one PC, owner played p2, p1 in the background): both logs "run in background patch: on";
  in p1 0x1f9f0a0 now returns 0 and 10 of 33 NPCs moved in 2 s (0 before). A 3 min recording shows p1's puppet
  walking p2's straight moves about 0.5 s late; the owner sees p2 move on p1's screen "with a small lag".
  Open: while p2 turns in tight ~2 m circles the puppet barely moves (a clear + move-to every 0.25 s keeps
  restarting it); 0x16756ec only mutes audio and lowers thread priority in the background.
- Fix for tight circles: when the puppet is under 3 m from its target, one order every 0.75 s instead of 0.25 s.
  Owner's phone video (14 s, both screens, p2 running small circles, p1 in the background): on p1's screen the
  villager puppet walks and runs round p1's hero, changing place every second, a little behind p2.
  Seen in the same test: p1 logged "silent 5.0 s: puppet removed" then respawned it 1 s later (p2 sent nothing for
  5 s, likely a menu: unknown). The two games show different times of day (p2 darker): world time is not synced.

### 2026-11-01: time of day

Observed:
- The darker p2 in the owner's video was not the game: both copies read time of day 0.5 (noon), not advancing
  in the Oakvale prologue (likely the second monitor filmed at an angle).
- `CGameTimeManager` (no vftable; RTTI only for `CConsoleCommand<CGameTimeManager>`, vftable 0x2ee5140), built by
  0x1cb8a90, called from 0x1facc64, stored at owner +0x1c: instance = `[[[0x362b3cc] + 4] + 0x1c]`. Console
  commands `SetTimeOfDay` (handler 0x1cb8470: arg * 1/24 → +0xc and +8, clamped to 0..1 or to day / night
  windows 0.229–0.8125 and 0.375–0.708 depending on bytes +0x18 / +0x20) and `SetDaySpeed` (0x1cb84f0: +0x10 =
  1 / arg). So +8 = time of day as a fraction of 24 h.
- DLL: packet "FCO3" adds the sender's time of day; FABLE_COOP_HOST=1 marks the host; the guest writes the host's
  value into +8 and +0xc when they differ by over 0.001 (wrap-aware), whatever the regions.
- Tested on one PC: wrote 0.95 into p1 (host) → p2 logged "time of day 0.500 -> host's 0.950" within 1 s;
  owner: night on both screens. Wrote 0.5 back afterwards.
Unknown: day count (string "DayCount" next to the commands) is not synced.

### 2026-11-01: hero look-alike puppet

Observed:
- Creature defs in `<game dir>/WellingtonGame/FableData/Build/Data/CompiledDefs/Development/names.bin` (556
  CREATURE_ names). Hero-like: CREATURE_HERO, CREATURE_HERO_CHILD, CREATURE_YOUNG_HERO, CREATURE_HERO_TRAINING,
  CREATURE_HERO_HUMAN_BASE, CREATURE_HERO_TEMPLATE, CREATURE_HERO_RIVAL_GOOD_01 (only the first three tested or named).
- Spawning a second CREATURE_HERO_CHILD (GSI CreateCreature) froze p1's game thread (0 frames in 3 s), then the
  game closed. A second player-hero creature is not usable.
- CREATURE_YOUNG_HERO spawns fine (CTCPhysicsNavigator, an NPC); free, it walked off ~45 m by itself. Owner:
  same clothes and hair as the child hero, but a wide-eyed, open-mouthed face that never changes, scripted or
  free. `Defs/DevHeaders/meshdata.h` lists it only in prologue cutscene animations (CS_OAKVALE_INTROFEET_YOUNG_HERO_
  ... SCARED, CS_OAKVALE_DEAD_DAD_YOUNG_HERO_...): derived, it is the cutscene double of the child hero.
- The DLL's puppet def is now CREATURE_YOUNG_HERO.
Unknown: how its face is set (faces are UE3 FaceFX: USkeletalMeshComponent Play/StopFaceFXAnim exist in the exe);
an adult look-alike; copying the real hero's appearance (clothes, hair, morphs) onto the puppet.
- Face investigation (p1, live):
  - Faces are not FaceFX here: the hero's and the puppet's USkeletalMeshComponent (vftable 0x2f51fac; pawn =
    `[[thing+0xa4]+8]`, AHeroPawn / AAIPawn, component at pawn +0x384) both have their FaceFX instance (+0x5c0)
    stopped (+0x30 = 2). Exec natives: StopFaceFXAnim 0xaf6660 → 0xaf5fe0, IsPlayingFaceFXAnim 0xaf7900.
  - Facial animations exist as body anims (`meshdata.h`: ANIM_NEW_HERO_FACIAL_IDLE_01.., ANIM_BIPED_BOY_01_FACIAL_DEFAULT,
    BANDIT_FACE_NEUTRAL_OVERRIDE; `creature_anim.h`: ANIM_FLAG_OVERRIDE_FACE_WITH_DEFAULT).
  - CThing components: vector of {TCI id, component*} at thing +0x44 (begin) / +0x48 (end) / +0x4c (cap). Ids are
    `ETCInterfaceType` in `Defs/tc_types.h` (TCI_CREATURE_EXPRESSION = 0xc, TCI_REGION_DISPLAY = 0x8e).
  - The hero has CTCCreatureExpression (id 0xc, vftable 0x2ef15e4, +0x10 = 1); the YOUNG_HERO puppet has none.
    Its update (0x1d643a0, every 16 frames) picks squint / evil / content / default and calls 0x1d63f20(expr,
    weight), which looks up a facial anim for that expression in the creature's anims and queues it (+0xc list).
  - `CThing::AddTC` = 0x1f2f3f0, thiscall(thing, const CCharString *class name, char replace, int 0), ret 0xc,
    returns the component; the caller frees the name. Adding "CTCCreatureExpression" to the puppet worked (no
    crash), but its expression stayed 0 and calling 0x1d63f20(1, 1.0) by hand queued nothing: derived, the
    YOUNG_HERO anim set has no facial anims. Owner: face unchanged.
  - Pointing the puppet's CCreatureDef expression table (+0x38) at the hero's made 0x1d63f20 queue
    ANIM_HERO_FACIAL_DEFAULT (3135), same queue as the hero's; owner: face unchanged. The expression update
    (0x1d64040, called from the creature anim update at 0x1f41b5b) builds the play request (0x1b75f20) and
    drops it, so in this build that queue plays nothing (observed in the disassembly, 0x1d641d0..0x1d641fd).
  - **UE3 reflection works over the bridge.** GNames = TArray<FNameEntry *> at 0x35071e8 (ANSI text at entry
    +0x10, entry 0 = "None"; 60877 names). UObject: +0x2c Name (index, number), +0x34 Class, +0x38 archetype.
    UStruct: +0x48 super, +0x4c first field. UField +0x3c Next. UProperty +0x40 ArrayDim, +0x44 ElementSize,
    +0x48 flags, +0x60 Offset (BoolProperty bit mask +0x6c). Scratch dumper: `ue.py` / `cmpnodes.py` (local).
  - Hero (CREATURE_HERO_CHILD) and puppet (CREATURE_YOUNG_HERO) use the **same** mesh MESH_YOUNGHERO_02, the same
    AnimTree template "BlendTree" (26 nodes) and the same 5 AnimSets. SkeletalMeshComponent: +0x1e4 SkeletalMesh,
    +0x1ec AnimTreeTemplate, +0x1f0 Animations (tree), +0x1f4 node list. AnimNode +0x5c SkelComponent,
    +0x88 NodeName. AnimNodeSequence +0xf0 AnimSeqName, +0x100 CurrentTime, +0x10c AnimSeq, +0x110 AnimLinkupIndex.
  - **Cause (observed):** the puppet's sequence nodes play villager-man anims (ANIM_VILLAGER_MALE_IDLE_BREATHE_01,
    ANIM_VILLAGER_MALE_GAZE_SKY_01, ANIM_BIPED_GENERIC_MAN_WALK); the hero's play ANIM_HERO_IDLE / WALK / RUN and
    ANIM_YOUNGHERO_*. The adult anims pose the face bones of the child mesh (confirmed by the hook below).
  - Writing AnimSeqName / AnimSeq by hand does not hold: the game sets them again every frame (1912 rewrites in
    90 s). Swapping the puppet thing def's CCreatureDef id (0x2ad4 → hero's 0x29af, via thing def vf 0x38
    "CCreatureDef") did not change the anims (restored).
  - UAnimNodeSequence::SetAnim(FName) = 0x804ea0, virtual only (vftable 0x2b43090 slot 0x19c, also 0x2b4322c /
    0x2b4342c / 0x2b43634 entries; no direct calls). execSetAnim 0x894c20, execPlayAnim 0x894760.
    The proxy now hooks that slot: on MESH_YOUNGHERO_02 outside AHeroPawn, ANIM_VILLAGER* / ANIM_BIPED* become
    ANIM_HERO_RUN / WALK / IDLE by name. Result (2026-11-02): live dump shows the puppet on ANIM_HERO_IDLE /
    ANIM_HERO_WALK and it holds; owner: "Visage normal". So the adult anims were the cause (confirmed).

### 2026-11-02: animation sync

- Packet "FCO4" adds `char anim[48]`: the UE3 name of the sender hero's main anim (the AnimNodeSequence with
  the highest NodeTotalWeight +0x78). Names, not GNames indices: indices can differ between copies.
- Receiver: looks the name up in GNames once per change; every frame calls the real SetAnim on each of the
  puppet's sequence nodes whose AnimSeqName differs (the game calls SetAnim only on change), and the SetAnim hook
  keeps the puppet's own calls on the remote anim. The puppet is matched by its SkeletalMeshComponent, so other
  young heroes (cutscenes) are untouched. Before the first packet: villager → hero idle/walk/run fallback.
- Test (p2 moved, read through both bridges every 1.5 s for 90 s, `animsync.py`, local): 60/60 samples the
  puppet's nodes played p2's main anim: ANIM_HERO_IDLE, _RUN, _JOG, _UNARMED_COMBAT_IDLE,
  _UNARMED_STRIKE_RIGHT_2, _KICK_CHICKENS_AND_HEADS_01, ANIM_YOUNGHERO_KICK_FLOOR / SCRATCH_EAR / SCRATCH_ASS.
- Known ceiling: one anim per packet (an upper-body anim over walking legs sends the walk); playback time is not
  synced (one-shots start where the puppet's node is). Owner's visual check: pending.
- Owner check: all fine except the unarmed combat stance: the puppet stood in T-pose. Dump (`combatdump.py`):
  the puppet's nodes had AnimSeqName ANIM_HERO_UNARMED_COMBAT_IDLE but AnimSeq = None. Cause: anim variants share
  a name and differ by FName number (ANIM_HERO_IDLE 0 and 3..10; ANIM_HERO_UNARMED_COMBAT_IDLE and
  ANIM_HERO_UNARMED_KICK_RIGHT_C90_2 are number 2); the packet sent number 0, which matched no sequence.
  Fix: packet "FCO5" adds `int animNum`, applied with the name. Owner: no more T-pose, combat stance shown.
- Owner: a strike makes the puppet slide on the ground. Measured in p2 (`strikerec.py`, ~60 Hz through the bridge,
  stick released): each ANIM_HERO_UNARMED_STRIKE_RIGHT_2 (0.77 s) moves the hero's physics 2.4 units forward;
  idle and combat idle move < 0.1. The engine moves the body while the anim stays in place. The puppet got one
  walk order per 0.75 s near its target and glided there. Fix: outside walk/jog/run anims the proxy drops the walk
  order and places the puppet on the interpolated remote position every frame (navigator teleport slot).
  Owner (2026-11-02): no more sliding on strikes.
- Owner: on a sharp turn while walking, the puppet stood still for a moment before turning. Not the network
  (local UDP, 0.1 s interpolation delay): the walk orders (one per 0.75 s within 3 m of the target, only when it
  moved > 0.3 m) made it finish its old path, stop, then wait. With the anims coming from the remote hero, the
  orders are no longer needed: the proxy now empties the puppet's action queue and places it on the interpolated
  remote position every frame, always (walk orders, gap/near/stuck constants removed). Owner (2026-11-02):
  sharp turns now fluid.
- Known flaky start: a copy sometimes crashes ~10 s after launch, before the menu (access violation at exe
  0x62f361, function 0x62f270, copying 16 bytes from a bad pointer). Seen 2026-10-31 19:00 (before any anim code)
  and 2026-11-02 14:43; relaunching that copy works. Cause unknown.

### 2026-11-02: the main menu (for a Coop entry)

Observed (read-only, test copy):
- The front end is a Scaleform movie: `CookedPC/Maps/Regions/Region_GameFrontEnd.umap` references textures exported
  from a movie named FrontEnd (`GFxExportTemp\FrontEnd_I*.tga`), the clips `Root_Menu`, `RootMenu`, `BaseFullGridMenu`,
  and the entries CONTINUE, OPTIONS, ACHIEVEMENTS, XBOX_LIVE_MARKET, QUIT_TO_WINDOWS, TYPE_SAVE_LOAD_GAME. The pause and
  inventory menus are their own packages (`CookedPC/UI/PauseMenu_SF.upk`, `InventoryMenu_SF.upk`); `GFxUI.u` is shipped.
  `WellingtonGame.u` names `WellingtonFrontEnd`, `GFxMoviePlayer`, `mMovieRootObject`, `resetMenuIndex`.
- The movie bytes are not visible in the package as stored (no SWF/GFX signature found): the exports are compressed.
- The exe has Scaleform GFx 4 (namespace `Scaleform::GFx`, `Scaleform::Render`) with both `ACTIONSCRIPT2` and
  `ACTIONSCRIPT3`, `attachMovie` (AS2) and the Direct Access API `AttachMovie`, `ExternalInterface`, `FSCommand`,
  and "Failed loading SWF" (it can load SWF files by name).
Derived: a Coop entry can be added to the live menu without changing game files: from the proxy, through the
GFxMoviePlayer/GFxObject natives (or Scaleform's Direct Access API), attach a copy of an existing menu button
symbol to the root menu, label it, and catch its press. A lobby list screen can reuse the save/load list layout
(`BaseFullGridMenu`) or be a small SWF of our own that uses the game's font library (`gfxfontlib`).
Unknown: whether FrontEnd is AS2 or AS3 (decides how buttons are cloned); how a menu press reaches the game
(ExternalInterface name or UnrealScript event); where the GFx natives are in the exe.

### 2026-11-02: Steam relay

Observed:
- The exe imports `SteamAPI_Init`, `SteamAPI_RunCallbacks`, `SteamNetworking`, `SteamUser`, `SteamMatchmaking`, ...
  from the game's own `steam_api.dll` (2014 SDK: accessor exports, C++ interfaces, no flat API, no
  `ISteamNetworkingMessages`). The game initializes Steam itself under AppID 288470.
- The proxy now takes `FABLE_COOP_PEER=steam:<SteamID64>`: it calls `SteamNetworking()` from the game thread,
  `AllowP2PPacketRelay(true)`, and sends the same 20 Hz packet with `SendP2PPacket` (unreliable, channel 7); it reads
  only channel 7 and only packets from that SteamID. Both sides send to a known id, and the proxy also calls
  `AcceptP2PSessionWithUser(peer)` once a second, so no `P2PSessionRequest_t` callback is needed.
  Interfaces are declared as C++ vtables in the order of the SDK headers (ISteamNetworking005 slots 0-7,
  ISteamUser017 slots 0-2; `GetSteamID` returns the struct through a hidden pointer, like MSVC built it).
- One PC, one account: `FABLE_COOP_PEER=steam:me` sends to the player's own SteamID. One copy (p1), Oakvale save:
  `GetSteamID` returned a 17-digit id starting 7656119; 2 s after "Steam P2P ready" the copy received its own
  packets and spawned the puppet on its own hero; owner: the second player is there, right behind the hero.
  So Steam loops a P2P packet back to the sender, and send / read / user slots are right.
Unknown: two different accounts on two PCs (relay route, latency, session accept order) not tested yet.

### 2026-11-02: kit for a friend

- `fable-coop/kit/FableCoop.bat`: one `.bat` with a PowerShell body (the shipping playbook's pattern); `make_kit.py`
  embeds the DLL as base64 with its SHA-256. Menu or `install | play host|guest <SteamID64> | uninstall`, `--game-dir`.
- The kit installs into the player's own game folder (a copy would be 8.7 GB), so the DLL is now inert unless
  `FABLE_COOP_PROFILE` is set (only the kit and tests set it). Checked: test copy started without it, our DLL loaded and
  forwarded, no log written, nothing written under the real `My Games\FableHD`.
- Own SteamID64: recent Steam no longer writes `MostRecent` in `config\loginusers.vdf` (none of 4 accounts had it);
  `HKCU\Software\Valve\Steam\ActiveProcess\ActiveUser` (32-bit account id, + 76561197960265728) gives the logged-in
  one; it matched the id the game reported.
- Started from PowerShell 7, Windows PowerShell inherits a PSModulePath where `Get-FileHash` is missing; the kit
  hashes with .NET.
- `kit/test_kit.ps1` (fake Steam folder, fake Documents, the real exe copied in): 13/13 pass: game found through
  `libraryfolders.vdf`, foreign `dinput8.dll` backed up and restored, second install is a no-op, bad / own SteamID
  refused, saves copied, peer remembered, wrong exe refused with nothing written, missing game explained.
  `play` on the test copy started the game with profile `coop` and the Steam peer (log checked).
