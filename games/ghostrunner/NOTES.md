# Ghostrunner — co-op attempt notes

Session 2026-10-21. Playbook: coop-anything.
Status: **triage + research done; hook validated; netcode probe done. No co-op yet.**
Published: draft PR #107 / claim issue #108 in the upstream repo.

Short version: no co-op mod for Ghostrunner exists anywhere. The build is Unreal Engine
4.26 and **UE4SS v3.0.1 injects and runs Lua in it** (proved). The engine's networking
classes (`IpNetDriver`, `SteamNetDriver`, `EOSNetDriver`) are all present and loaded, but a
listen server does not start from a console command alone. Everything here is a from-scratch
build, and none of the cheap shortcuts worked.

---

## 0. Game identity (per AGENTS.md rule 5)

### OBSERVED

- **Ghostrunner** (2020, One More Level / 505 Games). **GOG-based release.**
  Install: `<game dir>`.
  GOG evidence: `goggame-1443435189.info`/`.hashdb`, `gog.ico`; app ids seen:
  1443435189, 1519337965, 1596358146, 1797774287, 2009792833, 2072875151, 1957528513.
- **The copy under test is a third-party (pirated) distribution**, reported by the person
  testing 2026-10-21. Consequence for the design: no platform service (EOS, GOG Galaxy,
  Steamworks) may be assumed to authenticate, and none is used. Transport is our own relay.
  No DRM, pak-signing or anti-tamper work is done in this project.
- Install size 34.28 GB / 117 loose files. Content pak
  `Ghostrunner\Content\Paks\Ghostrunner-WindowsNoEditor.pak` = 35,845,557,950 B, plus a
  Russian overlay pak (179,256,121 B). Two `.sig` files (2,188,368 B each) are **byte
  identical** (SHA-256 `5F7B1F43…FCB77B24`) — they are not genuine per-pak signatures.
- Pak footer: magic `0x5A6F12E1`, **pak version 11**, index offset 35,842,217,633,
  index size 737,712. Past the mount point the index is **opaque (encrypted/compressed)** —
  cooked `Config/*.ini` and asset names cannot be read statically.
- Launcher `Ghostrunner.exe` = stock UE4 `BootstrapPackagedGame`, 540,672 B, no version
  resource, SHA-256 `487FEA621B582CE57E998ADAC5BB4641B92BA2C75193DA6C180E3A51DA52D560`.
- Real game binary `Ghostrunner\Binaries\Win64\Ghostrunner-Win64-Shipping.exe`
  **88,627,712 B**, SHA-256
  **`8AE0977F794CC3A21EF37550707A048069C81FA2A595B6FD8188CBB06FA94BD4`**.
  PE x64, TimeDateStamp 2022-06-23 14:24:11Z.
- **Engine: Unreal Engine 4.26**, custom One More Level fork. UTF-16LE
  `FEngineVersion` in the exe = `++UE4+Release-4.26` and `++UE4+Release-4.26-CL-0`.
  Confirmed at runtime by UE4SS Patternsleuth: `[PS] Found EngineVersion: 4.26`.
  Engine source root baked into strings: `<engine source root>\Engine\Source\...`.
- Plugins: `DLSS`, `OnlineSubsystemGOG` only. Bundled DLLs: `EOSSDK-Win64-Shipping.dll`
  23,390,688 B, `Galaxy64.dll` 15,274,496 B (GOG), `steam_api64.dll` 290,768 B,
  `CgSDK.x64_*`, `turbojpeg.dll`, `xaudio2_9redist.dll`, `nvngx_dlss.dll`.
- No dedicated-server executable. No EasyAntiCheat/BattlEye files or brand strings.
  EOS *AntiCheatClient* code is linked but inactive ("dummy client" per SDK log strings).
- Loose (non-pak) writable seams that exist on disk:
  `Content\JsonFiles\Bindings.json` (custom input: `bindPC` / `bindPAD` tables, one set per
  device family, **no player index**),
  `Content\JsonFiles\Platforms\OnlineSubsystem\FOnlineSubsystemData.json` =
  `{"defaultOnlineSubsystemName": "GOG"}`,
  `Config\Automation\Presets\{BaseTest,GraplingHook}.json` (UTF-16LE dev presets),
  `Content\JsonFiles\Debug.json`.

### DERIVED

- Pak is opaque and `.sig` is dummy, so **asset/Blueprint editing is effectively closed**;
  all co-op work must be runtime (mod loader, config overlay, hooks) — which also means we
  never ship game files.
- GOG + Galaxy means no Steamworks lobbies/P2P through the store; transport must be our own
  relay or the bundled EOS/Galaxy SDK.

---

## 1. Existing work — nothing to revive

**No co-op or multiplayer mod for Ghostrunner (2020) or Ghostrunner 2 exists.**

Searched (2026-10-21): Nexus Mods Ghostrunner hub (32 mods — sword skins, DLSS unlocker,
ReShade, savefile, trainer), GameBanana (1 mod: a Raccoon skin), ModDB (**0 mods**),
GitHub repo+code search (620 "ghostrunner" repos; `ghostrunner coop` → 1 unrelated 2017
student game), Steam forums (wish threads only), Reddit r/GhostRunner (wish threads),
YouTube, general web, UE4SS custom-config list (no Ghostrunner folder).
Official: 505 Games support says "Ghostrunner is a single player game with a campaign";
Steam appdetails for 1139900 and 2144740 carry no multiplayer category. Ghostrunner 2 is
UE 4.27, not UE5, and has no co-op either (RogueRunner.exe is a single-player mode).

Modding scene that exists (proves in-engine scripting depth, all .pak/LogicMods based):
- `Dmgvol/Ghostrunner-Mods` — Trainer, WaveMode (spawns waves/drones/lasers), Movement Mod,
  Collision Visualizer, Unlocker, GhostrunnerRNG randomizer, **Ghost Mod**.
- **Ghost Mod replays your previous run as a second, visible, animated character ("Kevin")
  in the world** — evidence that a second body can exist and be driven from recorded data.
- Loaders: `UnrealModLoader` (RussellJerome, MinHook + ImGui, loads Blueprint/SDK mods);
  UE4SS (`UE4SS-RE/RE-UE4SS`). Tooling: `Dmgvol/GR_Guides` (UE 4.25 project, UModel,
  UnrealPak, UAssetGUI). Communities: GRSR modding Discord, official GR Discord.

Prior art for this class of problem: **Stray Local Co-op** (UE 4.27, UE4SS+Lua only,
uses the game's disabled split-screen; local only), **HogWarp** (Hogwarts Legacy UE 4.27,
native hooks + own netcode; the online reference), **PoppyCoop** (closed single-player UE4,
UE4SS Lua + Python relay, movement only, `automated`), **Pacific Drive Co-op** (UE4
listen-server netcode revived).

---

## 2. Netcode: what is actually in the build

### OBSERVED

- The engine's networking code is **compiled in, not stripped**. Strings in
  `Ghostrunner-Win64-Shipping.exe` (these are **UTF-16LE**, which is why an ASCII-only
  scan misses them):
  `UIpNetDriver::TickDispatch: Socket->RecvFrom: %i (%s) from %s`,
  `UNetDriver::InitListen`, `InitConnect`, `IsNetResourceValid`, `NetDriverDefinitions`,
  `ActiveNetDrivers`, `ServerConnection`, `ClientConnections`,
  `ENetworkFailure::NetDriverListenFailure`, `ServerReplicateActors`, `ReplicationDriver`,
  `ServerMove`, `ServerMovePacked`, `ClientAdjustPosition`, `ServerMove_Validate`,
  `ReplicatedMovement`, `bReplicates`, `SetReplicates`, `RemoteRole`, `NetGUID`,
  `PacketHandler`, `StatelessConnectHandlerComponent`, `bUseSeamlessTravel`,
  `NetServerMaxTickRate`, `bClampListenServerTickRate`, `p.NetEnableListenServerSmoothing`.
- Netdriver classes **loaded in the running game** (verified via UE4SS `StaticFindObject`):
  `/Script/OnlineSubsystemUtils.IpNetDriver` → valid,
  `/Script/OnlineSubsystemSteam.SteamNetDriver` → valid,
  `/Script/OnlineSubsystemRedpointEOS.EOSNetDriver` → valid,
  `/Script/Engine.NetDriver` → valid.
  `/Script/OnlineSubsystemUtils.DemoNetDriver` and a GOG netdriver → **invalid/absent**.
- EOS is integrated via Redpoint `OnlineSubsystemEOS` with baked settings
  (`EOSNetDriver`, `RedpointEOSPlatformDefault`, `EnableIdentityChecksOnListenServers`,
  `epic.clientId`/`refreshToken` keys); the exe delay-imports 527 `EOS_*` functions
  including `EOS_P2P_*` (19), `EOS_Lobby_*` (110), `EOS_Sessions_*` (39).
- **A listen server does NOT start from the menu world with a console command.**
  Two probes: `open /Game/Levels/MainMenu/MainMenu?listen` (froze the game, no NetDriver
  observed before the freeze) and `listen` with a loose
  `Saved\Config\WindowsNoEditor\Engine.ini` supplying `+NetDriverDefinitions=(…IpNetDriver)`
  plus `DefaultPlatformService=Null` → `ExecuteConsoleCommand ok=true`, but
  `FindAllOf("NetDriver")` stayed empty and `GameEngine.NetDriver` stayed invalid for
  ~12 s. `World.NetMode` did not read as a number through UE4SS (property access resolved
  to an object), so NetMode was not directly observed.

### DERIVED

- The code path for a listen server exists and the driver class is loadable; what is
  missing is a world that is created with `?listen` (i.e. a real level load) plus a
  GameMode willing to log a second player in. The menu world is the wrong place to test it.
- The 4.27-style `EOSNetDriver` is the most interesting transport: it would give relay
  P2P with no port forwarding. Using the game's own baked EOS credentials is a licensing
  question, not something to do silently.
  **DECIDED 2026-10-21: platform services are out.** The copy under test is a pirated
  distribution, so EOS / GOG Galaxy / Steamworks auth cannot be relied on, and reusing the
  baked credentials would be a licensing and fairness problem (AGENTS.md rules 1, 8, 10).
  Transport is our own UDP relay (direct IP, LAN, or an overlay VPN). Assume ports or a VPN
  are needed — there is no "no port forwarding" shortcut on this copy. No DRM, pak-signing
  or anti-tamper work in this project.

### UNKNOWN

- Whether a gameplay level with `?listen` produces a NetDriver.
- Whether `AGhostrunnerGameMode`/`BP_GhostrunnerGameMode` can spawn a second player pawn
  (`SpawnDefaultPawnFor` exists in the binary; behaviour untested).
- Whether the real player pawn uses `UCharacterMovementComponent` (replication-friendly)
  or a custom parkour movement — never observed, because a gameplay level was never
  entered in this session.
- Where the game writes saves/config (game appears never launched before: no `Saved\`,
  no `%LOCALAPPDATA%\Ghostrunner`).

---

## 3. Runtime findings (UE4SS)

### OBSERVED

- **UE4SS v3.0.1 injects into Ghostrunner 4.26 and runs Lua.** Log:
  `UE4SS - v3.0.1 Beta #0 - Git SHA #d935b5b`,
  `[PS] Found EngineVersion: 4.26`, `PS scan successful`, `Console created`,
  `Event loop start`, mods start, no crash.
- Artifact pinned: `UE4SS_v3.0.1.zip`, 5,523,402 B, SHA-256
  `4B47D4BCEDDD2F561A4E395BFA00924CCFC945AF576A2D0C613E6537846C57EC` — matches the hash
  recorded independently in the hub (`games/poppy-playtime/NOTES.md`). Downloaded from the
  official release URL; GitHub's API did not publish a digest for this asset.
- UE4SS's own built-in mods work: console (F10) created, CheatManager constructed+enabled,
  `BPModLoaderMod` created a `LogicMods` directory.
- Object reflection works end to end from Lua: `FindAllOf("World")`, `FindFirstOf`,
  `StaticFindObject`, property and function reads all succeed.
- Objects seen at the main menu:
  - GameInstance: `BP_GameManager_C` (`/Game/Blueprints/Managers/BP_GameManager`),
  - GameMode: `GM_MainMenu_C` (`/Game/Blueprints/GM_MainMenu`),
  - PlayerController: `/Script/Ghostrunner.PlayerControllerOML`,
  - Pawn: `BP_MenuPawn_C`,
  - live manager instance: `SaveManagerOML /Engine/Transient.BP_SaveManager_C`.
  - Worlds: `/Game/Levels/MainMenu/MainMenu`, `/Game/Levels/Main_menu/Main_Menu_HEL`,
    `/Game/Levels/Main_menu/Main_menu_3d` (so map convention is
    `/Game/Levels/<Name>/<Name>`, with sublevels).
- On the menu pawn: `Role=3` (Authority), `RemoteRole=1` (SimulatedProxy),
  `bReplicates=true` — actors are not locked to Role_None, i.e. the replication
  properties are live and a network role could be assigned.
- Driving the menu by input injection (`SendKeys` Enter + a synthetic centre click) did
  **not** get into a gameplay level; the pawn stayed `BP_MenuPawn_C` for 20+ ticks.
- `UClass:ForEachFunction`/`ForEachProperty` from Lua iterate only the class's own members
  (a superclass walk is needed to get inherited functions) — worth knowing before anyone
  builds tooling on it.

### DERIVED

- UE4SS is a viable hook for this title: no signature config was needed (default
  configuration), which matches its Patternsleuth support entry for Ghostrunner.
- Because reflection works, the Model-B (puppet + state sync) path is unblocked in
  principle: local pawn state is readable and actors can be spawned/manipulated from Lua.

---

## 4. Test rig built in this session

- Hardlinked test copy at `<test copy>` — **117 hardlinks, 0 bytes copied**
  (same-volume NTFS hardlinks), exe SHA-256 verified identical, original install untouched
  (verified: the real `Binaries\Win64` still has no UE4SS files).
  Hardlinks matter here: overwriting a hardlinked *file* would edit the original install, so
  only new files are added and no existing file is replaced.
- UE4SS v3.0.1 installed into the copy's
  `Ghostrunner\Binaries\Win64\` (`dwmapi.dll` proxy + `UE4SS.dll` + `Mods\`), with the probe
  mod `Mods\M0Probe\Scripts\main.lua` enabled in `Mods\mods.txt`.
- Loose config override written at
  `Ghostrunner\Saved\Config\WindowsNoEditor\Engine.ini` (netdriver definitions) — kept for
  the next level-load test; it is in the copy only.
- Probe mods are read-only with respect to game state: they log, and the only console
  commands issued were `listen` and one `open …?listen`.
- Runs are launched windowed at 1280×720 and killed by pid; each run ≤70 s.

## 5. Chosen direction (not yet built)

Host-authoritative **state sync** in UE4SS Lua, transport through a small Python relay
(the PoppyCoop shape: file seam first, then UDP). Probe the native-replication path
(Model A) only on a real gameplay level, time-boxed, because the classes exist but the
listen server did not come up from the menu.

## 6. Probe log (chronological, all runs on the hardlinked copy)

Each run = launch windowed 1280×720, drive from the `M0Probe` Lua mod, kill by pid.

| # | Test | Result |
|---|---|---|
| 1 | Does UE4SS v3.0.1 inject and run Lua? | **PASS.** `UE4SS - v3.0.1 Beta #0`, `[PS] Found EngineVersion: 4.26`, `PS scan successful`, `Console created`, `Event loop start`. No custom signature config. First Lua attempt failed on my own syntax error, fixed. |
| 2 | Can Lua read game objects? | **PASS.** Worlds, GameMode, GameInstance, PlayerController, pawn, `bReplicates`/`Role`/`RemoteRole`, `SaveManagerOML` all readable. Map paths visible. |
| 3 | `listen` with a netdriver ini placed at `<game dir>\Ghostrunner\Saved\Config\WindowsNoEditor\Engine.ini` | **INVALID TEST.** No NetDriver appeared — but see correction below: packaged UE4 games read `Saved` from `%LOCALAPPDATA%`, so that file was never loaded. Do not cite this as evidence. |
| 4 | `open /Game/Levels/MainMenu/MainMenu?listen` | Game froze before any NetDriver was observed (~60 s, killed). One earlier run also issued `open …?listen` and the same freeze. |
| 5 | `open /Game/Levels/01_INDUSTRIAL/01_01/01_01_World` (level path from the GhostrunnerRNG map dictionary) | **Crash.** Menu stayed up ~90 s, then the PlayerController disappeared and the process exited; one UE4 crash report (`UE4CC-Windows-C52FDF964AA3F89357D80489FF657EFB`, 8 crash/CrashContext/UE4Minidump.dmp). Jumping to a level with `open` bypasses the game's own level flow and crashes this build. |
| 6 | `listen` with the netdriver ini in the **correct** location, `%LOCALAPPDATA%\Ghostrunner\Saved\Config\WindowsNoEditor\Engine.ini` | Config proven read — `GameEngine.NetDriverDefinitions` becomes a populated `TArray`. But `GameEngine.NetDriver` stayed `invalid` and no `NetDriver` instance appeared for 30 s after `listen`. **So a netdriver definition alone is not enough; `listen` on an already-loaded standalone world does not convert it.** the user config was backed up and restored byte-identically (SHA-256 `C808E16C…48272B` verified before/after). |

### Corrections to earlier statements in this file

- **Config location**: packaged UE4 games keep `Saved/` under `%LOCALAPPDATA%\<Project>\Saved\`,
  not under the game directory. The real user config is
  `%LOCALAPPDATA%\Ghostrunner\Saved\Config\WindowsNoEditor\`. It already contains
  `[Core.System] Paths=` entries (the plugin list, including `EOSOnlineSubsystem`,
  `DeltaDNAAnalytics`, `CorsairiCue`, `Aura Unreal Plugin`, `SpiderNavigation`,
  `DonAINavigation`, `DragonIK`, `DLSS`) and **no** `NetDriverDefinitions`.
- **Crash history is clean**: exactly one UE4 crash report exists on this machine, created
  by probe 5. Probes 1-4 and 6 did not crash; the freezes in 3/4 were hangs, killed.
- Only one of the `.sig` files is meaningful; both are byte-identical, so pak integrity is
  not actually enforced against mods.

### Where Model A stands after these probes

The pieces exist (`IpNetDriver` class loadable, `NetDriverDefinitions` array honoured,
`ServerMove`/`InitListen` strings in the binary) but no listen server has been observed.
The two untested levers are (a) load a map **with** `?listen` through the game's own
level flow rather than the console, and (b) the Redpoint `EOSNetDriver` path. Until one of
those runs, Model A is `unproven`, not dead.

## 7. W1 solved: how to get into a level, and the real player pawn (2026-10-21)

### OBSERVED

- **Entry path that works (game's own flow, no `open`)**:
  title screen → click → main menu → `Ghostrunner` → `Level select` → difficulty
  (`Classic mode`) → click the unlocked level card (`An Awakening`) → level loads.
  Resulting world: **`/Game/Levels/Tutorial/L_Tutorial_Persistant`** (the persistent
  tutorial map that hosts "An Awakening" and "Look Inside").
- **The real player pawn is `BP_PlayerCharacter_C`**, at
  `/Game/Levels/Tutorial/L_Tutorial_Persistant.L_Tutorial_Persistant:PersistentLevel.BP_PlayerCharacter_C_…`
  - **`CharacterMovement` = `CharacterMovementComponent`** (`CharMoveComp`) — a **standard
    UE4 CMC, not a custom parkour component**. `MaxWalkSpeed = 1000.0`, `MovementMode = 1`
    (Walking) at spawn.
  - `Mesh` = `SkeletalMeshComponent` (`CharacterMesh0`).
  - `Role = 3` (Authority), `RemoteRole = 1` (SimulatedProxy), `bReplicates = true`.
  - Spawn location `(-20179, -67068, 2099)`.
- **`UGameplayStatics:CreatePlayer` works**: setting
  `GameMapsSettings.bUseSplitscreen = true` and calling `CreatePlayer(pc, 1, true)` added a
  second `PlayerControllerOML` and a second pawn. Player controller count went 1 → 2 and
  stayed stable over several ticks. So this build tolerates a second local player.
- **`open <map>?listen` does create a NetDriver** — in one run `FindAllOf("NetDriver")`
  reported `count=1` after the travel started. That run then crashed, **but it was
  confounded**: `CreatePlayer` had already added a second local player in the same session,
  and a split-screen layout changes the menu flow. Treat "netdriver creation works" as
  suggestive, not proven.
- Menu automation by fixed screen coordinates is **fragile**: creating a second player
  (split-screen viewport) shifts the layout and later clicks miss. Two of four automated
  runs failed to reach a level for that reason.

### DERIVED

- Because the pawn is a stock `UCharacterMovementComponent` on a `Character`, UE4's own
  movement replication (`ServerMove` / `ClientAdjustPosition`, both present in the binary)
  is applicable to it. Model A went from "speculative" to "plausible with a standard pawn",
  which is the strongest argument yet for the native-replication route.
- A second local player existing is useful on its own: it is the local split-screen
  fallback, and it is a ready "second body" for puppet development without any netcode.

### CLEAN Model-A RESULT (2026-10-21, second clean run)

OBSERVED, no confounds (no `CreatePlayer` in the session):

- Entered `/Game/Levels/Tutorial/L_Tutorial_Persistant` through the menus; pawn
  `BP_PlayerCharacter_C` present, `PlayerControllers = 1`.
- Issued `open /Game/Levels/Tutorial/L_Tutorial_Persistant?listen` via
  `KismetSystemLibrary:ExecuteConsoleCommand` → `ok=true`.
- At +3 s, +6 s, +10 s, +15 s, +22 s: world unchanged, **`NetDriver = nil` the whole time**,
  `PlayerControllers = 1`, same pawn. **No crash, no travel, no netdriver.**
- The earlier `NetDriver = count 1` observation came from a session that had already created
  a second local player and then crashed. It is **not** reproducible in a clean session and
  must not be cited as "listen works".

Interpretation: with the map already loaded and standalone, the console `open …?listen`
does not re-travel and does not convert the world. This is consistent with `listen` also
doing nothing. Model A therefore needs a **real map load with `?listen` from the start**
(launch option / GameMode travel path), not a command issued into a running standalone
world. That is the next experiment, and it is the last cheap one for Model A.

### UNKNOWN

- Whether a fresh process launched straight into a map with `?listen` (e.g. a startup map
  or a travel at level start) produces a NetDriver. Untested.
- Whether two local players survive a level transition and a checkpoint respawn.
- Whether the second local player gets a camera viewport (not observed on screen).

## Next actions

0. **Launch-to-listen**: get a map loaded with `?listen` at load time rather than by a
   console command into a running standalone world (options: a startup map switch, or
   hooking the game's own level-start/travel from Lua so the URL carries `listen`).
   If that yields no NetDriver either, close Model A and go full Model B.
1. Model B hello-world: fake peer over a file, spawn a puppet from `BP_PlayerCharacter_C`,
   drive it to peer coordinates. The class and a standard CMC are now both known.
2. Split-screen viability check: does `CreatePlayer` survive a level change, and does the
   second player get its own viewport and input ?
2. Identify the real player pawn class and whether it has `CharacterMovement`.
3. M0.5: fake peer via file → spawn and drive a puppet actor to peer coordinates.
4. Two instances + file seam → M1; then replace the seam with a UDP relay.
5. No publishing without explicit approval.

## Sources

- UE4SS releases: https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/v3.0.1
- UE4SS game support (Patternsleuth list, incl. Ghostrunner): https://docs.ue4ss.com/dev/patternsleuth-games.html
- Ghostrunner mods used as prior art: https://github.com/Dmgvol/Ghostrunner-Mods
- Modding guide: https://github.com/Dmgvol/GR_Guides
- 505 Games: single-player statement https://support.505games.com/support/solutions/articles/150000147366/
- Steam appdetails (no multiplayer category): https://store.steampowered.com/api/appdetails?appids=1139900&l=english
- Stray local co-op (UE4SS-Lua precedent): https://github.com/CyberLifeYT/Stray-Local-Co-op-Mod
- HogWarp (closed UE4 online co-op precedent): https://hogwarp.com/

## 8. Model A closed; Model B puppet primitive proven (2026-10-21)

### Model A: four independent negatives — CLOSED

| Attempt | Result |
|---|---|
| `listen` console command in a loaded standalone world | no NetDriver (earlier probe) |
| `open <map>?listen` into a loaded level | no-op: `NetDriver=nil` at +3/6/10/15/22 s, no travel, no crash |
| map URL as a command-line argument (`Ghostrunner-Win64-Shipping.exe /Game/Levels/…?listen`) | **ignored**: game still boots to the main menu |
| `UGameplayStatics:OpenLevel(pc, LEVEL, true, "listen")` from the menu | **crashes immediately**: the Lua call never returns, process dies |

The engine-side pieces are all present (classes loadable, `NetDriverDefinitions` honoured),
but this build's level flow is not network-tolerant: the only paths that actually start a
travel either ignore the option or crash. Model A is closed for now — not because the
netcode is missing, but because the game's own game-mode/level bootstrap cannot survive a
`?listen` load. Reopening it would need native hooking of `UEngine::LoadMap`/travel, which
UE4SS Lua cannot do (it hooks UFunctions only).

### Model B: the puppet primitive works

- `bUseSplitscreen = true` + `UGameplayStatics:CreatePlayer(pc, 1, true)` adds a second
  `PlayerControllerOML` and a second pawn (proven twice).
- Driving that body from **external data** works: every tick the mod computed a target point
  on a circle around the host pawn and called
  `remotePawn:K2_SetActorLocation(target, false, hit, false)`. Result `ok=true, res=true`
  and the pawn's actual location matched the target **exactly** on every logged tick
  (e.g. target `(-83, 182, 0)` → actual `(-83, 182, 0)`; 9 consecutive ticks).
  That is the receive-and-drive half of a puppet, demonstrated with real engine calls.
- Design consequence: a remote avatar does **not** have to be a spawned actor — the proven
  route is a "peer slot" second local player whose pawn is driven from network state.
  The cost is that split-screen viewports appear, which is a UX problem to solve later.

### Still blocked: spawning a non-player puppet actor

- `UGameplayStatics:BeginSpawningActorFromClass` **exists and is bound** (the field reads
  back as a `UFunction`), but every argument variant fails:
  `(pc, class, xform, false, nil, nil)`, `(pc, class, xform, false)`,
  `(pc, class, xform, true)`, `(pc, class, xform)` all raise. The raised value is not a
  string — it comes back as a `UFunction` object, so the real cause is hidden.
- Most likely cause: UE4SS's Lua-to-`FTransform` marshalling (the table
  `{Translation=…, Rotation=…, Scale3D=…}` may not be accepted). Next agent should test a
  scalar/simple-struct argument on another function to confirm, or try
  `StaticConstructObject` + `AActor::FinishSpawning`, or use a Blueprint helper actor
  (the community's `ModActor` + LogicMods convention) to do the spawn inside BP.
- Not required for the first playable: the second-local-player route above avoids it.

### Workflow lesson (cost me three runs)

Input automation needs the game window to be foreground; `SetForegroundWindow` from a
non-foreground process often silently fails, and then `SendKeys` goes to whatever window is
focused. Fix by driving the game from Lua and checking game state, not by timed clicks.
