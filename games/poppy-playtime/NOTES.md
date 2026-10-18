# Poppy Playtime — notes

Game slug: `poppy-playtime`. Sessions: 2026-10-18.

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

## TODO

- [x] When install finishes: record buildid, exe SHA-256, engine version — done
      2026-10-18 (buildid 21905565, UE 4.27.2, hashes above).
- [x] Phase 2 workspace: test copy + UE4SS v3.0.1 installed and verified running — done
      2026-10-18.
- [x] Phase 3 first pass: full object dump + key class inventory + level names — done
      2026-10-18.
- [ ] Open "claim a game" issue in cranebeaker/coop-anything.
- [ ] Add gap entry to data/directory.yaml + regenerate DIRECTORY.md; draft PR.
- [ ] Read Huggy AI / door / objective classes in detail; map the Chapter 1 quest flow.
- [ ] Decide transport (GameNetworkingSockets vs Steam) and write the co-op skeleton mod.
- [ ] Automated launch into a level for QA (console `open /Game/Levels/PP_FinalLevel`,
      not fragile menu clicks).
