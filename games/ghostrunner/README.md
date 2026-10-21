# Ghostrunner (2020)

| | |
|---|---|
| Engine | Unreal Engine 4.26 (One More Level fork) |
| Versions tested | GOG release, shipping exe build 2022-06-23, SHA-256 `8AE0977F794CC3A21EF37550707A048069C81FA2A595B6FD8188CBB06FA94BD4` |
| Projects | none found (this write-up is a gap + first-session hook findings) |
| QA level | none |
| Last updated | 2026-10-21 |

## Where things stand

Nobody can play Ghostrunner in co-op today, and no co-op mod exists to revive — the
searches below found nothing on any platform. What this page adds is the practical part:
the build's engine is **UE 4.26**, its networking code is **not stripped**, and **UE4SS
v3.0.1 injects and runs Lua inside it**, which is the cheapest hook this game is ever
likely to get. That makes a from-scratch co-op prototype plausible; it does not make it
close. Treat this page as the starting line, not a status report of a working mod.

## Existing work

Searched 2026-10-21, nothing found:

- **Nexus Mods** Ghostrunner hub — 32 mods, all cosmetic/trainer/savefile/DLSS-class
  (<https://www.nexusmods.com/games/ghostrunner/mods>). No multiplayer mod.
- **GameBanana** — 1 mod (a skin) (<https://gamebanana.com/mods/games/10610>).
- **ModDB** — 0 mods listed (<https://www.moddb.com/games/ghostrunner/mods>).
- **GitHub** — repo/code search: 620 "ghostrunner" repos, none co-op; `ghostrunner coop`
  returns an unrelated 2017 student game.
- **Steam forums / Reddit r/GhostRunner / YouTube** — wishlist threads only
  (<https://steamcommunity.com/app/1139900/discussions/0/3108014879965760313/>).
- **UE4SS custom game configs** — no Ghostrunner folder.
- **Official**: publisher states the game is single-player with a level select
  (<https://support.505games.com/support/solutions/articles/150000147366/>); the Steam
  store payload lists no multiplayer category
  (<https://store.steampowered.com/api/appdetails?appids=1139900&l=english>).
  Ghostrunner 2 (UE 4.27) also has no co-op; `RogueRunner.exe` is a single-player mode.

Adjacent modding that matters as prior art (not co-op):

- **Ghost Mod** — replays your previous run as a second visible, animated character
  ("Kevin"). Proves an extra driven body can exist in this game without new engine
  features. <https://github.com/Dmgvol/Ghostrunner-Mods>
- **UnrealModLoader** (MinHook + ImGui, loads Blueprint/LogicMods)
  <https://github.com/RussellJerome/UnrealModLoader>,
  **UE4SS** <https://github.com/UE4SS-RE/RE-UE4SS>,
  **GR_Guides** (UE 4.25 project, UModel/UnrealPak/UAssetGUI toolchain)
  <https://github.com/Dmgvol/GR_Guides>.

## How the game works (for co-op)

- **Engine**: Unreal Engine 4.26, custom "OML" fork. Version confirmed twice: UTF-16
  `FEngineVersion` = `++UE4+Release-4.26-CL-0` in the shipping exe, and UE4SS
  Patternsleuth reports `Found EngineVersion: 4.26` at runtime.
- **Packaging**: one 35.8 GB `…-WindowsNoEditor.pak` (pak version 11) whose index is
  encrypted, plus `.sig` files that are byte-identical dummies. Cooked `Config/*.ini`
  and asset names are not readable statically, and asset/Blueprint edits are effectively
  closed. Plan for **runtime-only** changes (loader, loose config, hooks).
- **Netcode**: compiled in, not stripped. The exe carries (UTF-16) `UIpNetDriver::TickDispatch`,
  `UNetDriver::InitListen`/`InitConnect`, `ServerReplicateActors`, `ServerMove`,
  `ClientAdjustPosition`, `ReplicatedMovement`, `bReplicates`, `NetGUID`, `PacketHandler`.
  At runtime `/Script/OnlineSubsystemUtils.IpNetDriver`,
  `/Script/OnlineSubsystemSteam.SteamNetDriver` and
  `/Script/OnlineSubsystemRedpointEOS.EOSNetDriver` are all **loadable classes**. So a
  listen server is a code-available path — but see the caveat in "What we changed".
- **Platform services**: GOG build. `OnlineSubsystemGOG` + `Galaxy64.dll` (GOG lobbies and
  P2P), `EOSSDK-Win64-Shipping.dll` with Redpoint `OnlineSubsystemEOS`/`EOSNetDriver`
  compiled in, `steam_api64.dll` present but unused by a GOG install. The runtime subsystem
  is chosen by a **loose** file,
  `Content\JsonFiles\Platforms\OnlineSubsystem\FOnlineSubsystemData.json` =
  `{"defaultOnlineSubsystemName": "GOG"}` — the cheapest lever for changing transport.
- **Input**: the game reads its own `Content\JsonFiles\Bindings.json` (`bindPC` and
  `bindPAD` tables). One binding set per device family, **no player index** — the game
  assumes one local player who can hot-swap mouse/keyboard ↔ gamepad. Any split-screen or
  second-player scheme has to deal with that.
- **Flow**: `BP_GameManager_C` (GameInstance), `GM_MainMenu_C` (menu GameMode),
  `AGhostrunnerGameMode`/`BP_GhostrunnerGameMode`, `PlayerControllerOML`, `PlayerStateOML`,
  `LevelManager`, `CheckpointManager`/`RevertableObject`, `SaveManagerOML` +
  `GhostrunnerSaveGameBase`. Maps live under `/Game/Levels/<Name>/<Name>` and use
  sublevels (e.g. `/Game/Levels/MainMenu/MainMenu`, `MainMenu` + `Main_Menu_HEL` +
  `Main_menu_3d`). Gameplay level paths are documented publicly by the randomizer's
  runtime map-name dictionary (e.g. `/Game/Levels/01_INDUSTRIAL/01_01/01_01_World` =
  "The Climb"; three maps are "Persistant" and hold two levels each).
- **Where the game writes state**: `%LOCALAPPDATA%\Ghostrunner\Saved\` — `Config\WindowsNoEditor\*.ini`
  (this is the loose config that a mod can override; it is *not* under the game directory),
  `Crashes\`, `Analytics\`, and the save games. Worth knowing before anyone edits config:
  this is user state, back it up.
- **No anti-cheat**. No EAC/BattlEye files or brand strings; the linked EOS anti-cheat
  client code is inert. No online service exists for this game to violate.

## What we changed

Nothing has been shipped. This session built a test rig and ran three experiments.

- **Rig**: hardlinked test copy at `<test copy>` — **117 hardlinks, 0 bytes
  copied** — so the original install is never touched (verified after every run). UE4SS
  v3.0.1 in the copy's `Binaries\Win64` (`dwmapi.dll` proxy), probe mod in
  `Mods\M0Probe\Scripts\main.lua`.
- **Experiment 1 — does UE4SS load? PASS.** Log shows `UE4SS - v3.0.1`,
  `[PS] Found EngineVersion: 4.26`, `PS scan successful`, console created, CheatManager
  enabled, Lua mods running. No custom signature config was needed. This is the single
  most useful result on this page.
- **Experiment 2 — can we read the game from Lua? PASS.** `FindAllOf`, `StaticFindObject`,
  property and function reads all work. Menu state read: GameInstance `BP_GameManager_C`,
  GameMode `GM_MainMenu_C`, controller `PlayerControllerOML`, pawn `BP_MenuPawn_C`
  (with `Role=Authority`, `RemoteRole=SimulatedProxy`, `bReplicates=true`),
  live `SaveManagerOML`.
- **Experiment 3 — does a listen server start? NO (so far).** With
  `+NetDriverDefinitions=(…IpNetDriver)` in the correct config file
  (`%LOCALAPPDATA%\Ghostrunner\Saved\Config\WindowsNoEditor\Engine.ini` — packaged UE4 games
  do not read `Saved` from the game directory), the definition **is** honoured:
  `GameEngine.NetDriverDefinitions` becomes a populated array. But `listen` left
  `GameEngine.NetDriver` invalid and created no `NetDriver` instance over 30 s, and
  `open …?listen` on the menu map froze the game before a driver could be seen.
  Conclusion: a netdriver definition alone does not convert an already-loaded standalone
  world; the map has to be loaded with `?listen` through a real load path.
- **Experiment 4 — can we jump to a level with the console? NO.** `open
  /Game/Levels/01_INDUSTRIAL/01_01/01_01_World` (path taken from the GhostrunnerRNG
  randomizer's runtime map-name dictionary) kept the menu up for ~90 s, then dropped the
  PlayerController and **crashed the process** (one UE4 crash report + minidump). The
  level paths are correct; the game's level flow (`BP_GameManager` state) is being
  bypassed, and this build does not tolerate that.
- **Experiment 5 — is the engine's netcode reachable from Lua? Partially.** The classes
  are loadable and the config is read, but no NetDriver has been observed. Model A is
  `unproven`, not dead: the untested levers are loading a map with `?listen` through the
  game's own flow, and the Redpoint `EOSNetDriver`.
- **Experiment 6 — can a gameplay level be reached without `open`? PASS.** Driving the
  game's own menus (title click → `Ghostrunner` → `Level select` → `Classic mode` →
  `An Awakening`) loads `/Game/Levels/Tutorial/L_Tutorial_Persistant`. The real player pawn
  is **`BP_PlayerCharacter_C`** and it carries a **standard `CharacterMovementComponent`**
  (`CharMoveComp`, MaxWalkSpeed 1000, MovementMode Walking) plus a `SkeletalMeshComponent`,
  with `bReplicates=true` and Role=Authority. That matters: UE4's own movement replication
  applies to a stock CMC, so Model A is now *plausible with a standard pawn* rather than
  speculative.
- **Experiment 7 — does the build tolerate a second local player? YES.** Setting
  `GameMapsSettings.bUseSplitscreen = true` and calling
  `UGameplayStatics:CreatePlayer(controller, 1, true)` produced a second
  `PlayerControllerOML` and a second pawn; the controller count went 1 → 2 and stayed
  stable. This is both the local split-screen fallback and a ready "second body" for
  puppet work with no netcode at all.
- **Experiment 8 — `open <map>?listen` from inside a level (clean test): NOT DONE YET.**
  One run did report `FindAllOf("NetDriver")` = 1 after the travel started, but that session
  had already created a second local player and then crashed, so it is confounded and is not
  claimed here. The clean run is the next step.
- Not done: a clean Model A verdict, and whether two local players survive a level change.
- Side effect to be aware of: the user config `Engine.ini` was temporarily patched for
  Experiment 3 and **restored byte-identically** (SHA-256 verified before/after). The
  original install was never written to.

## QA

Nothing is automated yet, and the honest QA level is `none` — there is no co-op feature
to test, and no two-client harness has run. The probe mods log to
`Binaries\Win64\UE4SS.log`; each run was launched windowed and killed by pid. Per the
playbook, a fake-peer or one-client run would not qualify as `automated` anyway.

## How to play

Not playable in co-op. There is nothing to install from this page yet.

## Known issues

- No co-op mod exists; everything here is research and a test rig.
- Pak is opaque and signed (dummy sig), so anything that needs a modified Blueprint,
  asset or cooked config is out of reach — runtime hooks only.
- Ghostrunner is a one-hit-kill game with no second-player concept: story, checkpoints
  and level-end triggers all assume "the player". That is the real work, not the transport.
- The game's input layer has one binding set for all local devices, so local split-screen
  would need the input class reworked before it could be pleasant.
- This build's EOS integration uses the game's own baked credentials. Reusing them for
  P2P is a licensing question, not a free win.

## Next steps

1. Enter a gameplay level deterministically — either call the game's own level-start
   function from Lua, or load a known map path with `?listen`. Then re-run the NetDriver
   probe. That single test decides Model A (revive engine replication) vs Model B
   (UE4SS puppet + own relay).
2. Identify the real player pawn class and whether it carries `UCharacterMovementComponent`.
3. Model B hello-world: fake peer over a file, spawn a puppet actor, drive it to peer
   coordinates (this is the first milestone worth calling progress).
4. Only then: transport (file seam → UDP relay), then two real instances.
