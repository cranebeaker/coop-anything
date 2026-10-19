# Poppy Playtime — notes

Game slug: `poppy-playtime`. Sessions: 2026-10-18, 2026-10-19.

The mod built here lives in its own repository:
<https://github.com/romannazarov057-spec/PoppyCoop> (UE4SS Lua mod, Python relay, MIT).

## Phase 0 — Triage (2026-10-18)

### Identity (rule 5)

| Field | Value | Source |
|---|---|---|
| Title | Poppy Playtime (base game = Chapter 1) | Steam store API appid 1721470 |
| Store | Steam | owner confirmation + `appmanifest_1721470.acf` |
| Install path | `<game dir>` = `D:\Steam\steamapps\common\Poppy Playtime` (owner's PC; local-only) | read-only scan of Steam libraryfolders.vdf |
| Version / build id | Steam buildid **21905565** (appmanifest StateFlags 4 = complete, SizeOnDisk 5,899,523,032 bytes, LastUpdated 1792338989) | `appmanifest_1721470.acf`, 2026-10-18 |
| Main launcher exe SHA-256 | `1E6CE253F97D663E8E3D6E8F2C7D3E20314D9906BEC5208BFF07578C4E31CC4D` — `<game dir>\WindowsNoEditor\Poppy_Playtime.exe` (345,600 bytes) | Get-FileHash, 2026-10-18 |
| Game exe SHA-256 | `0808123FFE2A8A002CA83DDD090C7796010DEB7C79CB52A79B71463F3DAD5912` — `<game dir>\WindowsNoEditor\Poppy_Playtime\Binaries\Win64\Poppy_Playtime-Win64-Shipping.exe` (80,168,960 bytes) | Get-FileHash, 2026-10-18 |
| Engine | **Unreal Engine 4.27.2** — exe `FileVersionRaw 4.27.2.0`; layout `WindowsNoEditor/`, paks `pakchunk0-WindowsNoEditor.pak` (2.65 GB) + `pakchunk0optional` (27 MB) | exe version info, dir listing |
| Release | 12 Oct 2021 | Steam store API |
| DLC | Chapters 2–5: appids 1817490, 2555190, 3008670, 4100940 | Steam store API |
| Steam categories | **Single-player only** (no Remote Play Together, no LAN, no co-op categories) | Steam store API `categories` |
| OS | Windows only | Steam store API `platforms` |
| Multiplayer leftovers in binaries | `Poppy_Playtime\Binaries\Win64` ships **`vivoxsdk_x64.dll` (5.3 MB)** and **`ortp_x64.dll`** — VoxeX voice-chat SDK (multiplayer voice infra) inside a single-player game; also `OpenImageDenoise.dll`, `tbb12.dll` | dir listing, 2026-10-18 |

### Answers to the triage questions

1. Which exact game? Poppy Playtime base (Chapter 1), Steam — install still downloading.
2. Already done? **No.** No `DIRECTORY.md` entry, no `games/` write-up, no issue/PR in
   cranebeaker/coop-anything mentions Poppy Playtime (checked via GitHub search API,
   `total_count: 0`, 2026-10-18).
3. Engine? Unreal Engine 4 (pending install verification).
4. Source available? None. Closed binary; no script source shipped. Modding scene exists
   on Nexus (58 mods: utilities, cosmetics, cheat menus — "UETools", "PlaytimeReborn"
   mod menu) → UE4 modding via UE4SS/blueprints is proven feasible by the community.
5. Engine multiplayer? Poppy Playtime itself: none shipped (single-player category only).
   **Lead:** TCRF documents an unused multiplayer minigame inside Chapter 2 builds
   (https://tcrf.net/Poppy_Playtime:_Chapter_2/Multiplayer — fetch returned 403 to the
   agent, text seen only via search snippet) → MOB Games had in-house UE multiplayer work.
   MOB's separate game Project Playtime is full UE multiplayer (6p) in the same universe.
6. Legal? Players own the game (base is free-to-play on Steam anyway). A co-op mod would
   ship as a separate package, no game files, no anti-cheat involved (single-player game).

## Phase 1 — Research (2026-10-18, agent search pass)

### Findings (observations — each with source)

1. **No usable co-op mod exists** as of 2026-10-18:
   - Nexus Mods Poppy Playtime: 58 mods, tops are utilities/cosmetics/cutscene
     compressors, no multiplayer mod (nexusmods.com/poppyplaytime, search pass).
   - GitHub repo search "poppy playtime" (435 results) and "poppy playtime multiplayer":
     no co-op/multiplayer projects — fan-game clones and junk only. One result looked
     like a piracy download page; not used (rule 10).
2. **A fan multiplayer mod existed and was deleted.** Reddit r/PoppyPlaytime thread
   "Anybody have a poppy playtime multiplayer mod?" (2022):
   https://www.reddit.com/r/PoppyPlaytime/comments/uut6zj/ — dev of the YouTube mod
   "Poppy Playtime's Multiplayer Mod Is INSANE"
   (https://www.youtube.com/watch?v=7nRBqSjCB0k) confirmed it was possible, files
   deleted. Follow-up video: "Scrapped Mods: Poppy Playtime Multiplayer WIP Mod"
   (https://www.youtube.com/watch?v=RuR7kCDEjeQ).
3. **Unused multiplayer content in Chapter 2** (TCRF, link above; snippet-level evidence
   only, direct fetch 403).
4. **Official multiplayer spin-off: Project Playtime** (Steam appid 1961460, free,
   6 players: 5 survivors + 1 monster). Private lobbies/playing with friends: community
   guides and 2025–2026 videos confirm it works; older threads complain about missing
   invite system. Server status pages report operational (low-confidence source).
   https://store.steampowered.com/app/1961460/
5. **Steam Remote Play Together: not supported** for Poppy Playtime (absent from store
   categories). Parsec (game-streaming with shared control) remains possible but gives
   friends control of the same single character — not true co-op.
6. **Nucleus Co-op: no Poppy Playtime handler found** (searches on splitscreen.me /
   nucleuscoop community, 2026-10-18). Nucleus needs the game to have local-split or
   LAN play to link instances; Poppy Playtime has neither → not applicable.

### Derived (conclusions from the observations)

- True campaign co-op for Chapter 1 = **Path 2 (build from zero)**: UE4 reverse
  engineering (UE4SS, SDK dump/Dumper-7), host-authority state sync; per README this is
  days+ of agent work with intermediate non-playable states.
- Quick options for the owner's actual goal ("поиграть с друзьями"):
  1. Project Playtime — official, free, works with friends today (different game mode).
  2. Parsec co-pilot session — watch/help one player, not real co-op.
  3. The mod itself — declined for now by the owner.

### Guesses (unverified)

- ~~Chapter 1 build likely also contains leftover multiplayer references like Chapter 2's~~
  **Confirmed on binary level 2026-10-18:** VoxeX voice SDK (`vivoxsdk_x64.dll`,
  `ortp_x64.dll`) ships in `Binaries\Win64` of the Chapter 1 base install — voice chat
  has no purpose in a single-player game, so multiplayer/online-service code existed in
  this UE4.27.2 build (likely shared with the Project Playtime codebase).

### Unknowns

- Exact installed build id / exe hash (install incomplete).
- Whether Chapter 1 Steam build matches any known modding write-up (UETools etc. target
  specific builds).
- Population/server health of Project Playtime long-term.

## Phase 2 — Workspace (2026-10-18)

Test install copied from `<game dir>` to a scratch folder on another drive
(`<test dir>`, 122 files / 5.23 GB, robocopy, 0 failures). The owner's install and
saves are untouched (rule 2).

- Save location (owner's PC, read-only check): `%LOCALAPPDATA%\Poppy_Playtime\Saved\
  SaveGames` — only `SG_Audio.sav`, `SG_Graphics.sav` existed (settings only, no story
  save yet). Backed up before any test launch.
- Tool: **RE-UE4SS v3.0.1** (official release `UE4SS-RE/RE-UE4SS`, asset
  `UE4SS_v3.0.1.zip`, 5,523,402 bytes, SHA-256
  `4B47D4BCEDDD2F561A4E395BFA00924CCFC945AF576A2D0C613E6537846C57EC`; the release
  publishes no checksum, so this is our own pin). Installed into the test copy's
  `Poppy_Playtime\Binaries\Win64`.

### Observation — UE4SS works on this build

Test launch of the copied game logged:

```
UE4SS - v3.0.1 Beta #0 - Git SHA #d935b5b
Using engine version 4.27 for internal logic ...
PS scan took ... Game: Poppy_Playtime-Win64-Shipping.exe (80168960 bytes)
```

UE4SS images the game, finds EngineVersion **4.27**, resolves its offsets, and runs our
own Lua inside the process (see Phase 3).

## Phase 3 — Understand (2026-10-18)

### Method (repeatable, no keypresses)

Custom Lua mod `CoopProbe` under the test copy's `Mods\`, enabled in `mods.txt`. It
(a) proves the hook (logs the local player each second), and (b) triggers UE4SS dumps
programmatically (`DumpAllObjects()`, `GenerateSDK()`), so nothing depends on a human
pressing Ctrl+J/Ctrl+H. Full class coverage was obtained by setting
`LoadAllAssetsBeforeDumpingObjects = 1` (and `...GeneratingCXXHeaders = 1`) in
`UE4SS-settings.ini`, then dumping from the main menu — **no need to enter the chapter**
to see its classes.

### Observation — the dumps

- `UE4SS_ObjectDump.txt`: 19 actors loaded at the menu, **10,701 after loading all
  assets**; dump grew 11.2 MB → 23.7 MB.
- Huggy is present, 1774 matches, e.g.
  `Package /Game/Assets/Characters/Huggie/Scary/Huggy_AI/.../Huggy_RIG`,
  `/Game/Assets/Blueprints/BridgeBreak/HuggyDeath/...`, camera rig
  `/Game/Player/Rig/CameraRig/Huggy_Jumpscare_camera`.
- `GenerateSDK` (`CXXHeaderDump`) crashed the game after the full-asset load — this is
  the documented UE4SS warning ("not stable & will crash the game if you load past the
  main menu after dumping"). The object dump is what we needed; headers can be
  regenerated per-class without the full load if required.

### Observation — levels (from the full object dump)

- `/Game/Levels/PP_FinalLevel` — the main Chapter 1 playable map.
- `/Game/Levels/Level_Streaming_Areas` — streaming sub-levels.
- `/Game/Levels/Power_Room_LightsOn`, `/Game/Levels/AchebesLightingScenarios` — lighting
  scenarios / sub-levels.
- `/Game/Levels/MainMenu` (+ `MainMenu_BuiltData`).

### Observation — game classes with the co-op-relevant fields (from `CXXHeaderDump`)

- `APlayerBP_C : ACharacter` — WalkSpeed/RunSpeed/Crouch, `ExtendoHandR/L`,
  `LengthOfWire`, `InventoryItems`/`StoredInventory`, `ObjectivesText`, `SlotName`/
  `UserIndex`/`SaveVariable`, health/damage, `DeathScreen`, `StartIntro`/`SkipIntro`,
  `DoorInteraction`. Signature detail: several functions take an explicit
  `APlayerBP_C* PlayerBP` parameter and there is a `User Index` field → the blueprint
  logic was written with more than one player instance in mind.
- `UPoppyPlayTime_GameInstance_C : UGameInstance` — the story/quest **flag store**:
  `CurrentObjective : FObjective`, `ObjectiveList : TArray<FObjective>`,
  `CurrentQuestInt`, `SaveFileName`, `SaveSlot`, and
  `CreateNewObjective/AnnounceObjective/CompleteObjective/FailObjective/DeleteObjective`.
- `APlayerGameModeBP_C : AGameModeBase` — `LoadGame()`/`SaveGame()`, `AsPPSaveGame`,
  `SlotName`.
- `UPPSaveGame_C` — `FS_SaveData` = `PlayerLocation`, `LevelSaved`, `MapName`, and a
  `TArray<FGuid>` (collected-item ids), `SaveSlotOne`.
- `USG_Chapters_C` — `CreatedNewGame`.
- GrabPack / interaction: `AExtendoHand_C`, `AExtendo_Rope_C`, `UPickUp_Component_C`,
  `DoorInteraction`, `GrabPackUpgradeStation`, `UHandleReplacement_C`, `UHandleBox_C`.
- UI/flow: `MainMenu_UI_Two_C` (Play/Merch/Settings/Quit), `UPlayMenu_C` (Play =
  New Game, `Skip Intro()`, `LoadGameButton`).
- Networking present in the build: `SteamCore`, `OnlineSubsystemSteam`,
  `OodleNetworkHandlerComponent`, `PacketHandler`, `SessionMessages` — the engine's
  replication/online stack is compiled in (Project Playtime heritage) but dormant; the
  game's own actors expose no replicated properties.

### Derived

- Model to use is **host-authoritative state sync** (playbook `how-coop-mods-work.md`):
  one client owns enemies/physics/objectives, the others send input and receive state.
- The single-map structure (`PP_FinalLevel` + streaming areas) means level-transition
  sync is mostly "keep the same streaming state", not "reload a level".
- The centralized `UGameInstance` objective store is exactly the "one flag store"
  practical rule for story co-op — replicate that structure and most of the campaign
  logic follows.

### Unknowns

- Huggy AI's actual class/behaviour graph (class list found; not yet read in detail).
- Which streamed sub-levels are gameplay-critical vs lighting only.
- Whether the pak is encrypted (a raw string scan for `/Game/Levels/` returned nothing;
  the header does not start with the standard UE4 pak magic `E1 12 6F 5A`). Asset
  extraction is not needed for the mod and is deferred.

## Phase 5 — Build, first slice: the puppet (2026-10-18)

The goal of this slice (playbook `how-coop-mods-work.md`, stage 2) is a second,
player-shaped actor in the live world whose transform we can drive from code. No
networking yet. Mod: `poppy-coop/PoppyCoop/Scripts/main.lua` (owner's workspace,
deployed into the test copy by `poppy-coop/deploy.ps1`).

### Observation — what works, verified by run

A single test launch (`killed after 110–130 s (NO CRASH)`) does all of this:

```
world = World /Game/Levels/PP_FinalLevel.PP_FinalLevel
playing: pawn=PlayerBP_C ...Player_Level:PersistentLevel.PlayerBP_2
spawn: summon SkeletalMeshActor (try 1, existing=4)
spawn: adopted puppet = SkeletalMeshActor ...PP_FinalLevel:PersistentLevel.SkeletalMeshActor_2147482034
dress: mesh=SkeletalMesh /Game/Player/Rig/Plegs_rig.Plegs_rig materials=0
drive: target=-3792 108 93 actual=-3792 108 93 moved=true
```

So: entry into Chapter 1, summon of a second actor, adoption of it, giving it the
player's skeletal mesh, and `K2_SetActorLocation` following a local orbit — all work
and the process survives. The puppet's actual position equals the commanded target.

### Observation — the spawn API is the trap (source: upstream issue)

- `World:SpawnActor(Class, {}, {})` (the call the shipped `BPModLoaderMod` uses)
  returns **nil** on this build; even the exact `SkeletalMeshActor` case fails, and the
  process then dies (exit `0xC0000409` / access violation).
- `UGameplayStatics:BeginDeferredActorSpawnFromClass` + `FinishSpawningActor` (the
  documented workaround) also returned nil and the process died (`0xC0000005`).
- Cause is upstream and open in v3.0.1:
  [UE4SS-RE/RE-UE4SS#527](https://github.com/UE4SS-RE/RE-UE4SS/issues/527) —
  *"UWorld::SpawnActor() crashes too often in a game thread from both Lua and C++"*;
  the crash is in the animation/ProcessEvent path. `v3.0.1` (2024-02-14) is the latest
  stable; `experimental-latest` is ~1181 commits newer (not used yet).
- **Workaround that works:** spawn through the game's own console cheat — send
  `summon <class path>` via `KismetSystemLibrary:ExecuteConsoleCommand` (the
  ConsoleEnablerMod creates a working `CheatManager`), then pick the new actor up with
  `FindAllOf(<ClassName>)` and a before/after name diff. This is the engine's own code
  path, not UE4SS's re-implementation.

### Observation — small API facts that cost test runs

- **`AActor::K2_GetActorTransform` does not exist** in this build (calling it raises).
  Present: `K2_GetActorLocation`, `K2_GetActorRotation`. For an `FTransform` use
  `UKismetMathLibrary:MakeVector` + `:MakeTransform` (present in the dump).
- **`K2_SetActorLocation(FVector, bool, FHitResult&, bool)`** — the `FHitResult&` is an
  out-param: passing `nil` makes UE4SS fail the call; passing a table `{}` works.
- **No `SetAnimInstanceClass`** on `SkeletalMeshComponent` in this build; `AnimClass`
  is a plain property.
- **Assigning the player's `AnimClass` (`Plegs_rig_AnimBP_C`) to a bare
  `SkeletalMeshActor` crashes** the game: the AnimBP dereferences its owning pawn,
  which is not a `PlayerBP_C` on a plain actor (access violation at a near-null address).
- **Copying the player's materials crashes.** `SetMaterial(i, playerMesh:GetMaterials()[i])`
  reproducibly killed the game ~2 s after dressing in three runs; with material copy
  disabled the same code ran 130 s clean. Exact cause not yet isolated (the material
  asset vs the render path); recorded as a known crash to avoid.
- Bisect evidence: run A (copyAnim=false, copyMaterials=**true**) → crash ~2 s after
  dressing, before driving; run B (both false, 15 s parked then driving) → no crash,
  puppet moves; run C (both false, drive immediate) → no crash, 130 s.

### Derived

- The "second body" half of the playbook's stage 2 is achievable on this engine, but
  only through `summon` + a *bare* skeletal mesh (no game AnimBP, no material copy).
  Making that puppet look/behave like the real player (animation, materials) is a
  separate, currently-blocked problem caused by UE4SS's engine hooks, not by the game.
- Everything the puppet needs for the networking stage (a stable actor reference and a
  working `SetActorLocation`) is proven. Transport work can proceed against this actor
  even while the visual question stays open.

### Unknowns

- Whether the material crash is the material asset, `SetMaterial` itself, or the
  rendering of a skeletal mesh wearing the player's material.
- Whether the `experimental-latest` UE4SS build fixes #527 (would unlock a normal
  `SpawnActor` and possibly animation).
- Whether a self-contained idle animation (single-node `SetAnimation` with a sequence
  from `/Game/Player/Rig/`, no AnimBP) avoids the animation crash.

## Phase 5 — Build, second slice: a transport seam, and two real clients (2026-10-18)

Goal: get the puppet's transform from one running game to another. The playbook's
remote transport (Steam "Spacewar", AppID 480, through a helper exe —
[playbook/networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md))
is a large step, so this slice first **separates the transport from the sync logic**
behind a seam, and proves both directions over a stand-in that needs no network code:
shared files in a folder.

- Seam: `CONFIG.transport = "file"`, `CONFIG.netDir`, `CONFIG.driveMode = "peer"`.
  Each process writes its own `<playerId>.state` (`"x y z yaw"`) and drives its puppet
  to the other player's file. `playerId` comes from `playerid.txt` in the game's working
  directory, so two copies of the game can run side by side.
- `CONFIG.driveMode = "orbit"` (the old local demo) stays as a fallback when no peer
  file exists yet.

### Observation — receive path, one instance, a fake peer

`poppy-coop/test-peer.ps1` writes a moving `peer.state` while one client runs. The
puppet lands exactly on the injected coordinates and tracks them as they change:

```
net: A sees peer B at -3400 120 95 -> puppet=-3400 120 95 moved=true     (…then -3200, -3000, …)
```

and the mod's own position appears in `A.state` (`-3881.63 -34.46 90.15 0.00`), so the
send direction works too.

### Observation — two real clients, two instances on one PC

Two copies of the test install (`poppy-playtime` = player A, `poppy-playtime-b` =
player B), each with its own `playerid.txt`; both launched at once. Two instances of
this build do run side by side (~2.2 GB each). Both entered Chapter 1, summoned a
puppet, and synced both ways:

```
A: net: A sees peer B at -3963 -24 90 -> puppet=-3963 -24 90 moved=true
B: net: B sees peer A at -3976  -0 90 -> puppet=-3976  -0 90 moved=false
A.state: -3975.87 -0.17 90.15 0.00     B.state: -3963.20 -23.57 90.15 0.00
```

So each client's puppet sits at the *other client's real player position*, both ways.
Clean re-run: **both processes alive after 100 s, 0 crash reports**, 28–29 mod log
lines each. (`moved=false` means the actor was already at the target — no movement —
not an error. Two instances share `%LOCALAPPDATA%\Poppy_Playtime\Saved`, so the game
writes its checkpoint there; the backup was restored after every run.)

### Observation — one unreproduced two-instance crash

The first two-instance run produced one crash report 26 s after start:
`EXCEPTION_ACCESS_VIOLATION reading address 0x10`, and the crashing thread's stack top
is **UE4SS code**, not the game:

```
UE4SS                           + 47a916
UE4SS                           + 462c83
Poppy_Playtime-Win64-Shipping   + 11a0eb4
…
UE4SS                           + 5550bc
```

It did not reappear in a clean re-run, and neither client in that first run stopped
logging at the reported time, so the report's owner is unclear. The stack being UE4SS
puts it in the same family as issue #527. Guessed, not established: entering the level
through the `open` console command (which skips the menu's normal startup) makes an
already-degraded setup; that call also produced the `WorldShake_C` crash earlier.

### Derived

- The sync logic and the transport are now separable. Whatever carries the bytes
  (Steam, UDP, a file), the mod's contract stays "one line of position per player".
- Auto-testing two clients is practical on one PC, no second machine needed.
- The real remote transport remains to be built; the file transport is a test stand-in
  and cannot cross a network by itself.

### Unknowns

- What the `0x10` UE4SS-side crash was, and whether two instances plus the `open`
  shortcut is a reliable trigger.
- Whether the two-game-instance trick keeps working when both clients are on *different*
  PCs (nothing observed yet — everything so far is one machine).

## Phase 6 — Build, third slice: a relay, and a real end-to-end run over it (2026-10-18)

Goal: put a real transport behind the seam. The Lua mod cannot open a socket, so the
transport has to be a separate process that sits next to the game and talks to the mod
through the files it can already read and write. That is the playbook's helper pattern
([networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md)).

### Where the helper is written (a real constraint, found by looking)

There is no C/C++ toolchain on this machine: `cl`, `gcc`, `g++`, `clang`, `tcc` are all
absent from PATH, `dotnet` is a runtime without an SDK, and the Steamworks samples in the
playbook assume a compiler that is not here. Python 3.8.3 is installed. The Steam
**flat C API** (`SteamAPI_InitFlat`, `SteamAPI_SteamUser_v023`, …) exists precisely to be
called from a language with no headers, so the helper is Python + `ctypes`:

- `poppy-coop/relay/cooprelay.py` — one program, two transports:
  - `--transport udp`   host binds a port, client sends to it. The stand-in for Steam in
    automated tests; no download, no account, two instances on one PC.
  - `--transport steam` loads `steam_api64.dll`, inits with appid 480, creates or joins a
    lobby, and moves the same lines through `ISteamNetworkingMessages` (Steam relay
    network, so no IPs and no port forwarding).
- File contract, unchanged from the mod's side: the relay reads `<netdir>\<self>.state`
  and writes `<netdir>\<peer>.state`, atomically (`.tmp` + replace) so the game's reader
  never sees a half-written line.

Mod-side additions so one machine can run two relayed clients:

- `netdir.txt` in the game's working directory pins that copy's transport folder
  (`deploy.ps1 -NetDir`). Copy A writes `…\net\A\A.state`; copy B writes `…\net\B\B.state`;
  each machine's relay bridges its own folder to the wire, so the two folders never
  collide.
- `<netdir>\cmd.txt` is an automated input channel, consumed on read: `pawnloc X Y Z`
  teleports the local player pawn, `console <text>` runs a console command. This is how a
  test can make a player move without a human at the keyboard.

### Observation — full chain, two clients, both directions, distinct positions

`poppy-coop/test-relay.ps1`: game A + relay A (udp host) and game B + relay B (udp
client), then a `pawnloc` moves A to a landmark `-3000 800 200`. After the move:

```
net\A\A.state  ->  -2963.87  798.36  200.72  32.86     (game A writes its own position)
net\B\A.state  ->  -2963.87  798.36  200.72  32.86     (same bytes, after going over UDP)
net\B\B.state  ->  -3675.69 -318.04   90.15  34.31     (game B writes its own position)
net\A\B.state  ->  -3675.69 -318.04   90.15  34.31     (same bytes, after going over UDP)
```

and the two games agree, in their own logs:

```
game A: net: A sees peer B at -3676 -318 90  -> puppet=-3676 -318 90  moved=true
game B: net: B sees peer A at -2964 798 201 -> puppet=-2964 798 201 moved=true
```

So the whole chain runs both ways — game A → file → relay A → UDP → relay B → file →
game B, and the reverse — with the two players standing far apart, so the numbers cannot
be confused with a shared spawn point. No crash reports during the run.

### Observation — the DLL shipped with the game is too old for the flat API

The playbook warns that old bundled `steam_api.dll` copies lack `SteamAPI_InitFlat`. Both
copies in the install do (checked with `poppy-coop/tools/check_steam_exports.py`):

| copy | size | flat API |
|---|---|---|
| `Engine\…\Steamworks\Steamv151\Win64\steam_api64.dll` | 265 504 | **missing** `SteamAPI_InitFlat` (SDK 1.51) |
| `PlaytimeLauncher\…\redistributable_bin\win64\steam_api64.dll` | 301 928 | **missing** `SteamAPI_InitFlat` |

The real Steam backend therefore needs the pinned Steamworks.NET standalone build the
playbook names. Downloaded from the official release (tag `2025.164.1`, asset
`Steamworks.NET-Standalone_2025.164.1.zip`): the zip's SHA-256 matched GitHub's published
digest `9412348c…796a537d`, and `Windows-x64/steam_api64.dll` inside it hashed to exactly
the playbook's pin `eb17909a…10bfb75` and exported all 31 flat-API symbols used here.

### Observation — the Steam backend initialises and creates a lobby (one instance)

With the pinned DLL in place, `poppy-coop/test-steam-smoke.ps1` runs the relay alone on the
owner's PC (no game, no second account) against the real Steam:

```
loading D:\coop-work\relay\steam\steam_api64.dll      (SHA-256 matches the playbook pin)
SteamAPI_InitFlat ok, appid 480
logged on as SteamID <owner's id, not reproduced here>
CreateLobby call handle=...
LobbyCreated result=1                                  (k_EResultOK)
LOBBY READY. Friend joins with: --lobby <lobby-id>
LobbyEnter lobby=<lobby-id> response=1                 (k_EChatRoomEnterResponseSuccess)
```

So init, logon, lobby creation and entering one's own lobby all work through the `ctypes`
prototypes. While the test runs, Steam shows the owner as playing "Spacewar" — expected for
AppID 480, and worth knowing before a test run on someone's account.

Bring-up note that cost real time: without explicit `argtypes` on every prototyped
function, `ctypes` passes a 64-bit interface pointer to the callee as a C `int` and
truncates it, so the calls fail or crash with nothing pointing at the cause. Declaring
`c_void_p` for every pointer parameter is what made it work.

### Derived

- The transport is now a program, not a hope: swapping `--transport udp` for
  `--transport steam` is the whole difference between the test path and the real one, and
  both ends share one file contract and one message format.
- An automated in-game input channel (`cmd.txt`) exists, which later phases need anyway
  (moving to find Huggy, opening doors, walking a route).

### Unknowns

- The `steam` backend's **message leg** is untested: init, logon and lobby creation are
  proven above, but carrying a line between two players is not, because Steam allows one
  logged-in client per PC and the P2P leg needs a second Steam account on another machine.
  Everything proven so far moved data over UDP on this machine, and no second machine was
  available for this work, so that leg stays open.
- Whether Steam's relay accepts the helper's `ISteamNetworkingMessages` sessions between
  two Spacewar (480) users once a second account exists — the playbook says yes, not yet
  seen here.
- Whether a Python helper is acceptable to ship (a player would need Python, or the helper
  needs freezing to an exe later).

## Phase 5 — Build, fourth slice: the puppet's looks, measured not guessed (2026-10-18)

The earlier slice kept `copyAnim = false` and `copyMaterials = false`, and the notes then said
the puppet had no materials and no animation. Half of that was wrong, and the way it was wrong
is worth the next agent's time.

**Observation — the materials were never missing.** A diagnostic that prints, per slot, what the
component would actually draw (`GetMaterial(i)` resolves the component's override if there is
one, and the mesh asset's own material otherwise):

```
diag[player]: mesh=/Game/Player/Rig/Plegs_rig slots=1 animInstance=Plegs_rig_AnimBP_C animClass=Plegs_rig_AnimBP_C
diag[player]: slot 0 = Material /Game/Assets/Materials/lambert1.lambert1
diag[puppet]: mesh=/Game/Player/Rig/Plegs_rig slots=1 animInstance=<invalid> animClass=<invalid>
diag[puppet]: slot 0 = Material /Game/Assets/Materials/lambert1.lambert1
```

Same mesh, same slot count, same material object: `SetSkeletalMesh` on its own already gives the
puppet the player's material, so the `SetMaterial` loop that crashed the earlier slice is not
needed at all. Note the material is a flat `lambert1` shape material — "correct" and "looks
untextured" are the same picture here, which is exactly why this had to come from data rather
than from looking at the screen.

**Observation — why the player's AnimBP can never go on the puppet.** The rig has 7 bones
(`GetNumBones()` = 7) and the player's animation is *procedural*: `Plegs_rig_AnimBP` contains a
dynamic cast to `PlayerBP_C` and feeds on the owning pawn's velocity and location. A bare
`SkeletalMeshActor` owns no such pawn, so the graph reads through a failed cast. That is the
crash the previous slice hit; no way of assigning the class makes it valid.

**Observation — a walk sequence plays with no AnimBP.** The rig ships a plain sequence on the
same skeleton, `/Game/Player/Rig/Anim/legs_Walk_anim`, so single-node animation is available:

```
anim: lookup 'legs_Walk' -> AnimSequence /Game/Player/Rig/Anim/legs_Walk_anim.legs_Walk_anim
anim: playing ..., animInstance now AnimSingleNodeInstance ...AnimSingleNodeInstance_2147480736
diag[puppet]: ... animInstance=AnimSingleNodeInstance animClass=<invalid>
```

`SetAnimationMode(AnimationSingleNode)` + `SetAnimation(seq)` + `Play(true)`. The game stayed up
and wrote no crash report. That gives the puppet legs that walk on their own, with nothing
borrowed from the player's rig.

### Unknowns

- Whether the walk *looks* right for a remote player: speed is chosen, not derived, and it says
  nothing about facing or direction. What is proven is "an anim instance is attached and the
  process survives", not the look.
- Bone names come back as `FNameUserdata` (`tostring` prints an address, not the name); `:ToString()`
  does print the name, so the names are readable — the *positions* are what the fifth slice could
  not read back by any safe route.
- The appearance checks are driven one console command or `cmd.txt` line at a time, which is
  workable for a session but is not yet a repeatable suite.

### Notes for the next agent

- `tools/screenshot.ps1` saves a PNG of the game window, which the owner can look at — but an
  agent reading these notes cannot view images, so visual claims stay programmatic here and the
  owner is the one who confirms the actual look.
- On this machine, newly written `*.ps1` files in the project folder were observed to stay
  locked ("used by another process") for the rest of the session, which is why the appearance
  checks were run as inline commands plus `cmd.txt` instead of a script file.

## Phase 5 — Build, fifth slice: why the puppet looked absent, and the thread rules (2026-10-19)

Fourth slice ended with "an anim instance is attached and the process survives", and a puppet that
could be seen in a screenshot as, at most, a few pixels. This slice found the reason and paid for
it with three game deaths.

**Observation — the summoned actor has no scale of its own; the player's mesh has 14.37.** The
player pawn's `CharacterMesh0` carries a component scale of `14.37 14.37 14.37`, which is how the
rig reaches a human size. A `summon`-ed `SkeletalMeshActor` starts at `1.0`, so the puppet was
rendered at roughly 1/14 of the player's size — parked 250–350 units away that is a few pixels, and
"the puppet is not there" and "the puppet is microscopic" look identical in a log. Copying
`K2_GetComponentScale()` from the player's mesh onto the puppet is what fixed the look:

```
diag[player]:  scale=14.37 14.37 14.37  refBone0=0.00 0.00 6.00
diag[puppet]:  scale=14.37 14.37 14.37  refBone0=0.00 0.00 6.00
diag: distance player->puppet = 250.0
```

Lesson worth carrying to other rigs: **measure the source component's scale before concluding a
spawned actor is missing.** A summon command creates an actor with a default transform, not a copy
of anything.

**Observation — the puppet is on screen, proven by moving it into the camera.** Frustum tests are
not enough: the player's *own* mesh reports `WasRecentlyRendered(0.5)=false` (first-person rigs
often skip drawing it), so a false there means nothing. The check that worked was teleporting the
puppet onto the player's location with the drive loop disabled first (see below): a frame before
and a frame after differ in 8334 of 20736 sampled pixels, where two frames of the *same* static
scene differ in ~9300 at a strong threshold. That is a large, real change and it settles "does it
render".

**Observation — pixel diffing cannot answer "does the walk move the legs" in this game.** The
scene has enough of its own motion that the noise floor swamps the signal:

```
two frames 3 s apart, no command sent, no camera input:
  d>12: 17210/20736   d>40: 12940   d>80: 9342
```

Camera translation is not the cause (the pawn's position is byte-identical across the pair). A/B
pairs with the animation on vs off land anywhere between 93 and 8334 changed samples depending on
when they are taken, so the numbers carry no information. Screen-space evidence is only usable for
gross changes, like "the puppet covers a third of the screen" — not for a walk cycle.

**Observation — all three routes to a bone position are closed on this build.**
1. `USkeletalMeshComponent::GetBoneLocationByName` does not exist here; it belongs to
   `UPoseableMeshComponent` and calling it raises.
2. `USceneComponent::GetSocketLocation` (a skeletal mesh resolves bone names as sockets) **froze the
   mod's thread every time**: from the tick loop directly, and queued with `ExecuteInGameThread`,
   which stopped the loop just the same. Symptom: `A.state` stops ticking, nothing is logged, no
   Lua error, no crash report, the process stays alive and responsive. `ExecuteInGameThread`
   therefore appears to be synchronous enough that a call that blocks the game thread blocks the
   caller too.
3. Reading the `USkeletalMeshComponent` property `CachedBoneSpaceTransforms[i].Translation` — a
   property read, not a call — returns `-nan 0.00 0.00` and `2.8e37 ...`, i.e. the wrong bytes.

**Observation — the animation clock is the usable proof that a pose is being re-evaluated.**
`UAnimSingleNodeInstance` gives both a position and a length (`GetLength()` = 2.0 s for
`legs_Walk_anim`). Sampling it once a second with the walk playing:

```
animstat[puppet]: mode=1 playing=true pos=1.24 rate=1.0
animstat[puppet]: mode=1 playing=true pos=0.0  rate=1.0
```

The position advances continuously, and a single-node instance only advances by evaluating the
sequence, so the skeleton is posed from the walk rather than left in its reference pose. That is a
weaker statement than "the legs visibly move", and it is stated as such.

**Observation — the thread rules, learned from two crashes and two silent exits.**
- Everything that mutates a UObject (mesh, materials, scale, animation, actor transform) must be
  queued with `ExecuteInGameThread`. Changing the anim instance straight from the mod's own thread
  produced `EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000010` with the call stack
  pointing at a native call originating from the mod thread.
- Driving the puppet's transform from the mod's own thread survives minutes and then dies with
  **no crash report and no Lua error**, the log simply stopping. After moving `drivePuppet`'s
  `K2_SetActorLocation`/`K2_SetActorRotation` (and the adoption-time placement) onto the game
  thread, the game ran four minutes in continuous "park the puppet in front of the player" mode,
  ~244 placements, with **zero Lua errors and no new crash report**. Two data points are not proof;
  they are the reason to keep the rule.
- A silent death is diagnosable: `A.state`'s mtime is the tick loop's heartbeat. Frozen mtime +
  live, responsive process = the mod thread is blocked inside a call, and the log will never say so.
- `log()` raises on a format argument of the wrong type (a UObject into `%.1f`), which aborts the
  rest of that tick, including commands queued behind it. Format vectors through a helper.

### Derived

- The puppet's look is the player's, by data: same mesh asset, same slot-0 material
  (`lambert1`), same component scale (14.37), and a `legs_Walk_anim` single-node animation whose
  clock runs. It is not borrowing the player's AnimBP and cannot.
- A screenshot is for the owner, not for the agent. Every visual claim in these notes is a number.

### Unknowns

- Whether the walk *looks* convincing at 14.37, and how it reads at a distance: only the owner can
  say, from a PNG.
- Whether the two silent exits were the off-thread transform writes at all, or something else on
  this machine after ~10 launches in a row (no crash report, no Windows error event, process gone).
  The game-thread change is defensible either way, but the attribution is unproven.
- The bone names are readable now (`GetBoneName(i):ToString()`, 7 of them: `joint3`,
  `l_leg_JNT`, `joint21`, `joint32`, …), but no safe way to read their *posed* positions was found,
  so per-bone verification is still not possible.

### Notes for the next agent

- `animpose <t>` freezes the walk at time `t` and `animplay` resumes it: the cleanest way to make
  two screenshots comparable. `animstat` prints the clock once and then once a second.
- If a test needs the puppet left where you put it, set `drive manual` first, or the drive loop
  re-parks it within two seconds and the test silently measures nothing.
- On this machine, when PowerShell is asked to write a `cmd.txt` with several lines in one
  `Set-Content -Value`, the shell call intermittently hangs for the whole session. Writing the lines
  with piped `Out-File`/`-Append`, one per call, worked every time.

## TODO

- [x] When install finishes: record buildid, exe SHA-256, engine version — done
      2026-10-18 (buildid 21905565, UE 4.27.2, hashes above).
- [x] Phase 2 workspace: test copy + UE4SS v3.0.1 installed and verified running — done
      2026-10-18.
- [x] Phase 3 first pass: full object dump + key class inventory + level names — done
      2026-10-18.
- [x] Open "claim a game" issue in cranebeaker/coop-anything — done: #19 (no
      `in-progress` label exists upstream, so it was opened unlabelled).
- [x] Add gap entry to data/directory.yaml + regenerate DIRECTORY.md; draft PR — done:
      DIRECTORY.md regenerated (245 entries), draft PR #20.
- [ ] Read Huggy AI / door / objective classes in detail; map the Chapter 1 quest flow.
- [x] Transport seam + two-client sync — done 2026-10-18: position sync works both ways
      between two real game instances (file transport, one PC). Not yet across a network.
- [x] Real remote transport, written and proven over the test stand-in — done 2026-10-18:
      `poppy-coop/relay/cooprelay.py` (Python + ctypes, two transports), full chain verified
      both ways with two clients (`test-relay.ps1`). The `steam` transport itself is
      untested pending the pinned DLL and a second Steam account.
- [x] Download the pinned Steamworks.NET `Windows-x64/steam_api64.dll` and smoke-test the
      `--transport steam` backend alone — done 2026-10-18: init, logon, lobby created,
      lobby entered (all `result=1`/`response=1`). Zip and DLL hashes matched the pins.
- [ ] Test the Steam P2P **message** leg — needs a second Steam account on a second machine
      (asked the owner: no second machine available yet). Until then only UDP is proven to
      carry the lines.
- [ ] Layer real co-op behaviour on the sync: send full transform + animation state,
      not just position; then sync the Chapter 1 objective (`UPoppyPlayTime_GameInstance_C`).
- [x] Puppet visuals, first pass — done 2026-10-18: materials were already correct (no copy
      needed); the player's AnimBP can never apply (it casts to `PlayerBP_C`); single-node
      `legs_Walk_anim` plays on the puppet with no crash. Look still unverified visually.
- [x] Puppet visuals, second pass — done 2026-10-19: the puppet was rendered at 1/14 size because a
      summoned actor has no component scale; the player's mesh carries 14.37 and the mod now copies
      it. Bone-position reads are closed off on this build (see the fifth slice), so a moving pose
      is proven by the single-node animation clock, not by bone data, and the walk's *look* is still
      for the owner to confirm from a screenshot. All puppet mutation now runs on the game thread.
- [ ] Drive the walk rate from the synced velocity instead of a fixed loop, and pick the walk
      direction from the peer's movement rather than a hard-coded yaw.
- [ ] Have the owner confirm the puppet's look from a PNG (`tools/screenshot.ps1`); if the walk
      reads badly at 14.37, try another sequence on the same skeleton or a static pose.
- [ ] Attribute the two silent exits: run a long soak with the off-thread transform writes restored
      on a machine that is not launching ten clients in a row, and see whether it dies again.
- [ ] Need the UE4SS `experimental-latest` build for #527? Only if spawning via UE4SS itself
      turns out to be needed; `summon` still avoids the issue entirely.
- [x] Automated launch into a level for QA (console `open /Game/Levels/PP_FinalLevel`,
      not fragile menu clicks) — done and used by every test run since.
