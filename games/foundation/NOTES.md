# Foundation (Polymorph Games): co-op notes

Session 1, 2026-11-04. Phase 0 (triage) and the start of phase 1 (research).

## Game identity (rule 5)

| Field | Value |
|---|---|
| Store | Steam, app 690830 |
| Steam build id | 26138506 (from `appmanifest_690830.acf`) |
| exe version | 1.12.0.16 (`foundation.exe` version info) |
| SHA-256 `foundation.exe` | 576845171FF3F6E44F1F8C5F231691D732768D3398C0A0DCA8734B42E5252B49 |
| Engine | Polymorph's own C++ engine, not Unity (no `*_Data`, no Mono or IL2CPP). Assets are in `*.package` files |
| Scripting | Lua plus LuaJIT (`third-party-legal-notices.txt`), official mod API, mod.io |
| Other libs | Dear ImGui, FFmpeg (avcodec-61 etc.), CrashRpt 1403, zlib, Steamworks (`steam_api64.dll`) |
| OS | Windows 11 |

## Observations (checked this session)

- No Foundation entry in `data/directory.yaml`, no `games/foundation`. A search of this
  repo's issues and PRs for "foundation" found nothing about this game.
- Web search ("Foundation Polymorph Games multiplayer co-op mod" and similar) found no
  co-op mod. Steam threads say multiplayer was never planned for the base game:
  https://steamcommunity.com/app/690830/discussions/0/787664130474524838/ ,
  https://steamcommunity.com/app/690830/discussions/2/5064850575728849287/
- GitHub repo searches ("foundation multiplayer polymorph", "foundation coop polymorph",
  "foundation bepinex polymorph") found nothing. Only mod-editing tools came up:
  simonsays187/FoundationModdingAPIPlugin (2019), Helron1977/PolymorphFoudationModEditor (2022).
- The install ships LuaSocket at `<game dir>/mods/libs/socket/core.dll` (425,472 bytes).
- Official debugging page: https://www.polymorph.games/foundation/modding/debugging-mods
  says scripts in `%USERPROFILE%\Documents\Polymorph Games\Foundation\moddev` load
  "without any of the restrictions imposed on regular mod files". It shows
  `package.cpath = package.cpath .. ";mods/libs/?.dll"` for loading LuaSocket (to use
  the MobDebug / ZeroBrane debugger). It also says mods should not depend on this folder.

## Derived (not yet tested)

- A `moddev` script could `require("socket")` and open TCP or UDP connections inside the
  game's Lua VM. That would give a Lua-only network channel with no exe patching and
  no DLL injection. This is the most promising path. Next test: does `require("socket")`
  work from `moddev`, and does a normal mod's sandbox block it?
- If LuaJIT's `ffi` is reachable from `moddev`, Win32 and Steamworks calls are reachable too.

## Guesses

- The simulation is probably not deterministic enough for lockstep (villager AI, floats).
  The likely model is host authority: the host runs the city, and clients send build and
  order commands and get state back. The fallback is turn-based save handoff.

## Unknown

- Whether the mod API can intercept player actions (place building, zone, set job) and
  replay them on another client.
- Whether the mod API exposes enough of the world state to serialize it.
- Whether `ffi` is available in `moddev`.
- Whether the simulation is deterministic.

## Next steps

1. Copy the install to a test folder (about 3.6 GB) and test there only (rule 2).
2. Test `require("socket")` in `moddev` on the test copy: a loopback echo between two instances.
3. Map the mod API (events, building placement, save hooks) from the wiki and the example mods.
4. Open a "claim a game" issue and a draft PR with the directory entry (needs the owner's GitHub).

## Session 1, part 2 (2026-11-04)

Done:
- Test copy at `<test dir>\game` (robocopy, 623 files, 3.9 GB). Its `foundation.exe`
  SHA-256 matches the original. Added `steam_appid.txt` (690830) to the copy only.
- The real and the test install share one user folder: `%USERPROFILE%\Documents\Polymorph Games\Foundation`
  (the game resolves it with `SHGetKnownFolderPath FOLDERID_Documents`,
  string in the exe). It holds `Save Game` (3.6 GB), configs and logs. Backed up in full to
  `<test dir>\backup-userdata` (102 files, byte count identical) before any test run.
- The mod API map so far (https://www.polymorph.games/foundation/modding/api):
  `LEVEL` has `createObject`, `find(id)`, `pick*`, `getTimeScale`; `GAME_OBJECT` has full transform
  setters and `destroy`; `COMP_MAIN_GAME_LOOP` fires `ON_NEW_DAY`, `ON_NEW_WEEK` and `ON_NEW_MONTH`;
  components can override `init`, `onEnabled`, `onDestroy` and similar. No player-input or
  "building placed" event has been found yet (unknown).
- Exe strings include `LUA_CPATH`, `SteamNetworkingSockets012` and `Software\Polymorph Games\Foundation`.

Test run 1 (observation): launching the test copy with Steam closed exits at once.
Log: `GamCSteamAdapter::initialize  Steam API init failed`, then the message box "Steam initialization failed".
The game needs a running Steam client. The probe (`moddev/coop_probe.lua`) did not run yet.
Side effect: `foundation.log` in the user folder was rewritten (the original is in the backup).

Test run 2 (observation, Steam running): the probe in `moddev` ran at start-up. Output:
```
_VERSION = Lua 5.1
jit = LuaJIT 2.0.5
io = table / os = table / require = function
package.cpath(before) = .\?.dll;<game dir>\?.dll;<game dir>\loadall.dll
require ffi = ok
require socket = LuaSocket 3.0-rc1   (after adding ";mods/libs/?.dll;mods/libs/?/core.dll" to package.cpath)
udp loopback receive = foundation-coop-hello
tcp create = ok
```
So a `moddev` script has full LuaJIT (ffi, io, os) and working UDP and TCP. This is the network channel.
The game reached the main menu (window title "Foundation - 1.12.0.16").

Test run 3 (observation): a second instance started while the first was running shows
`Fatal Error: Can't create log file`. Both instances use one user folder and the log is locked.
This is not a mutex: the window stayed open with that error until it was killed.

Derived: two instances need separate user folders. `foundation.exe` imports `SHGetKnownFolderPath` directly from
SHELL32 (import table, checked with a small PE parser). A test-only proxy DLL can patch that import in the test
copy and return `<instance>\Documents` (the method in playbook/multi-instance-testing.md, used there for Unity's LocalLow).
Proxy candidates that the exe imports and that load from the game folder: `dwmapi.dll` (1 import), `WINMM.dll` (2 imports).
No compiler is installed (no gcc, clang, cl, zig or Docker; Python 3.11 is present).

Cleanup: the probe was moved out of `moddev` into `<test dir>\scripts`, and `moddev` was removed.

## Session 1, part 3: two instances on one PC (2026-11-04)

Toolchain: Zig 0.17.1 portable (`zig-x86_64-windows-0.17.1.zip` from https://ziglang.org/download/index.json,
SHA-256 b03f477a1943673e02aca0f570e8030dffd536ce1f45cbc4d3326ec31159562e, checked), unpacked to `<test dir>\tools`.

Built (source in the session workspace `foundation-coop/instance/`, binaries in `<test dir>\bin`):
- `launch.exe <game.exe> <instance dir>`: starts the game suspended with `FCOOP_INSTANCE_DIR` set, queues
  `LoadLibraryW(folderhook.dll)` as an APC on the main thread, then resumes it.
- `folderhook.dll`: patches the exe's SHELL32 import `SHGetKnownFolderPath`. `FOLDERID_Documents` becomes
  `<instance>\Documents` and `FOLDERID_LocalAppData` becomes `<instance>\LocalAppData`. Every call is logged.
- Each instance starts from copies of the owner's `usersetting.config`, `input.config` and `modlist.xml`
  (copied from the backup), with an empty `Save Game`.

Test run 4 (observation): instances A and B are both running, both at the main menu (title "Foundation - 1.12.0.16").
`folderhook.log` shows exactly two calls per process: LocalAppData, then Documents, both redirected, HRESULT 0.
The game wrote `foundation.log` and `modding.log` in each private folder. The second instance no longer fails.

Test run 5 (observation): `coop_pingpong.lua` in each instance's private `moddev` folder. A binds UDP 27770 and
B binds 27771 on 127.0.0.1; each pings the other for up to 20 s at start-up.
```
A: received 'hello from B #1' after 3.66 s  RESULT PASS  (sent 23)
B: received 'hello from A #16' after 0.00 s RESULT PASS  (sent 9)
```
Two game processes on one PC exchanged UDP in both directions from inside their Lua VMs.

Check after the runs: SHA-256 of all 102 files in the owner's real user folder are identical to before (0 differences),
there is no `moddev` in the real folder, and the real `foundation.exe` hash is unchanged.

Limits (stated honestly): this test ran only at start-up, in a blocking loop. It is not yet known how a
`moddev` script gets a per-frame tick during gameplay. That is needed for real sync (unknown; next step).

## Next steps (updated)
1. Find a per-frame or per-tick hook for Lua (a registered component's update, a behavior tree node, or ON_NEW_DAY as a coarse tick).
2. Find how to catch player actions: building placement, destruction, job assignment, policy changes. Check whether
   the `moddev` environment can wrap the game's Lua-visible functions.
3. Prototype: host-authoritative command relay (client action → host → applied on both), plus a snapshot check.
4. Build a bot to drive both instances for automated QA (playbook/automated-qa.md).

## Session 1, part 4: looking for a per-frame tick (2026-11-04)

Observations:
- The example mod `Example Mithril Factory/scripts/component/COMP_ANTENNA.lua` defines a Lua component
  (`ParentType = "COMPONENT"`, registered with `mod:registerClass`) with an `update()` method that runs every frame
  (it uses `self:getLevel():getDeltaTime()`). `update` is not on the documented COMPONENT page. This is the likely tick.
- moddev environment dump (test run 6, `envdump_moddev.txt`): 145 globals, including all enums, `event`, `foundation`,
  `debug` (with `sethook`), `io`, `os`, `jit`, `bit`, `require`, `loadstring`, `setfenv`. `getfenv(1) == _G`.
  `foundation` has: createAsset createData createMod findAsset findGameObject getGameVersion getModVersion
  isDlcEnabled isModEnabled isModLoaded localize.
- Local mods go in `Documents\Polymorph Games\Foundation\mods\<name>` (wiki "start" page), so they are private per test instance.
  The game found the test mod `coop_probe` and logged `can't open file ...\mods\coop_probe/generated_ids.lua`.
  The mod did not run: no mod log was written. Mods seem to be enabled per new game (`NewGameModList` in
  `usersetting.config`, empty here; exe strings `PlaCPlayerSetting_NewGameModList`, `PlaSSavegameData_ModList`).
- Test run 7: calling `foundation.createMod()` from moddev crashes the game natively right after start-up
  (the log ends at `GamCGame::GamCGame Foundation - version 1.12.0.16`). Don't do this.
- Test run 8: `debug.sethook(fn, "", 1000)` from moddev, 60 s at the main menu: no callbacks. The game runs no Lua
  at the main menu, so no tick there.

Derived: a tick needs a real mod (registered component with `update()`) loaded in a running game. Getting there
means enabling the mod for a new game and starting a map. That needs UI input (New Game → mods → start), or the
`NewGameModList` format plus a way to start a map without clicks (unknown).

## Session 1, part 5: per-frame tick found and tested (2026-11-04, after an unrelated PC crash)

After the crash (cause outside this work): the owner's real user folder was hashed again, 102 files, 0 differences.
Both exe hashes unchanged.

UI driving: `foundation-coop/bot/ui.ps1` (screenshot of the game window, click, key; DPI-aware) drives the
windowed test instance (1280x720, set in that instance's private `usersetting.config`). The new-game wizard
(German UI) is: Neues Spiel → Mods auswählen → enable mod → Karte → "Errungenschaften deaktiviert" prompt (Ja) →
Regeln (tutorial off) → Herausforderung → Dorf beschreiben → Spiel starten. Positions are in the session log.

Observations:
- Enabling a mod writes `modlist.xml` as `<mod id="<ModId>" name="..." version="..."/>` and
  `usersetting.config` as `<NewGameModList><Item value="<ModId>"/></NewGameModList>`. ModId comes from the mod's
  `generated_ids.lua` (here written by hand with a fixed GUID; the game accepted it).
- Mod sandbox (modding.log): 124 globals, and `require`, `io`, `os`, `debug` and `getfenv` are all nil. moddev globals are
  not visible. But the `foundation` table is shared: a field set by moddev with
  `rawset(foundation, "FCOOP", {...})` is visible in the mod ("bridge visible: true, socket: true").
  So the moddev bridge can give the sandboxed mod LuaSocket.
- The mod writes its log at game exit only (modding.log was 0 bytes while running). Live output needs another channel (UDP used here).
- Component managers have `ON_COMPONENT_INITIALIZED` and `ON_COMPONENT_DESTROYED` events (`Example Taxation`,
  `TaxationFunctionOverTime.lua`); `getAllComponent(type):forEach(fn)` lists existing ones.
- `game.package` contains 309 distinct `PREFAB_*` ids (grep of strings), including `PREFAB_GAME_CAMERA` and `PREFAB_MANAGER`.

Test run 9 (PASS): `mod:registerPrefabComponent("PREFAB_GAME_CAMERA"/"PREFAB_MANAGER", {DataType="COMP_COOP_TICK"})`
with a Lua component that has `update()`. On a new map, both instances were created ("init on Camera", "init on Manager"),
and `update()` ran every frame: about 80 frames per second, `getDeltaTime` about 0.012 s, `getTimeScale` 1.00.
Once a second it sent a UDP datagram (via the bridge socket) to a Python listener on 127.0.0.1:27790, for over 50 s with no gap.
The camera's global position is readable (663.9 115.8 654.5 at start).

Result: step 1 (per-frame tick with networking inside a running game) works. Use the `PREFAB_MANAGER` instance as the
co-op net loop. Its world position is 0,0,0, so it is likely one per level (assumed, not proven).

## Session 1, part 6: recording player actions (2026-11-04)

Recorder: `foundation-coop/mod/coop_probe/mod.lua`. On the `PREFAB_MANAGER` instance's `init`, it takes
`level:getComponentManager("COMP_BUILDING" / "COMP_BUILDING_PART" / "COMP_WORKPLACE")`, registers
`ON_COMPONENT_INITIALIZED` and `ON_COMPONENT_DESTROYED`, and on each building registers `ON_CONSTRUCTION_STARTED`,
`ON_BUILDING_COMPLETED`, `ON_BUILDING_PART_BUILT`, `ON_BUILDING_PART_REMOVED`, `ON_MARKED_FOR_DESTRUCTION`,
`ON_BUILDING_DESTROYED`, `ON_PREVIEW_PART_PLACED` and `ON_EDIT_MODE_CHANGED` (`event.register(owner, obj.EVENT, fn)`).
Every event is streamed as a UDP line. All reads are wrapped in pcall.

Test setup: the new-game rules "Alle Territorien im Besitz" and "Unbegrenzte Münzen" set to Ein. Without them the first
placement fails with "Kann nicht bauen – Außerhalb des Territoriums" and the starting-territory UI is hard to drive.
This adds a second prompt ("Errungenschaften deaktiviert ... Benutzerdefiniert": Ja).
The bot placed buildings through the UI: Bauen (hammer) → building icon → click ground → "Bauen" button.

Test run 11 (observation, recording `rec-run11-actions.txt`):
```
place village centre:  comp_building.initialized name=VillageCenter (build tool opened, building is a preview)
                       building.edit_mode true  asset=VILLAGE_CENTER
                       comp_part.initialized    name=VillageCenter
                       building.preview_part_placed ok=true pos=533.72,97.61,511.62 parentBuilding=<building guid>
  press "Bauen":       building.completed finished=true → building.part_built → building.construction_started → building.edit_mode false
place lumber camp:     same sequence, asset=LUMBER_CAMP, part name=Lumber_Core_Part, pos=544.95,99.91,541.41,
                       plus workplace.initialized just before construction_started
assign worker (+):     workplace.worker_changed args=2 (same building guid)
demolish (Abreißen):   comp_part.destroyed → building.destroyed → comp_building.destroyed (same guids)
cancel a preview (X):  only building.edit_mode false; no destroyed event seen within the run (unknown when it is cleaned up)
```
Asset ids: `b.AssetBuilding.Id` gives `VILLAGE_CENTER` / `LUMBER_CAMP` (nil right at ON_COMPONENT_INITIALIZED, set by
the edit_mode event). The building's game object Name equals the asset id after init. All rotations were identity
(no rotation was used). GUIDs are unique per object and stable across the events.

Derived: the commit point of a player placement is `construction_started` (or `edit_mode false` after a placed preview).
The data a replica needs for a single-part building is asset id, part list (name/type), position and orientation.
Multi-part buildings, roads (BUILDING_PATH), zones and rotation are not tested yet.

Owner's real user folder re-hashed after the run: 0 differences.

## Next steps (updated)
1. Replay: on a second instance, create the same building from the recorded data (needs an API that places a building
   by asset, part and transform; candidates: `LEVEL:createObject(prefab, pos, rot)`, building_constructor; unknown).
2. Test multi-part buildings, rotation, roads and zones in the recorder.
3. Load-game path (only new games are tested).

## Session 1, part 7: trying to replay a building (2026-11-04)

Dev tool: the mod polls a UDP "eval" channel on 127.0.0.1:27791 (test instance only). The bridge passes
`compile(src, env)` (loadstring + setfenv), and `foundation-coop/bot/eval.py` sends a Lua snippet and prints
the result from inside the running game. Bot script `foundation-coop/bot/newgame.ps1` runs launch + wizard + village.

Observations (run 12):
- Level teardown (quitting) fires `comp_part.destroyed` / `comp_building.destroyed` for every building. The sync
  layer must not treat these as player demolitions.
- Asset ids differ from the names seen in events: `foundation.findAsset("BUILDING_LUMBER_CAMP")` → `Woodcutter Camp.data`,
  while `AssetBuilding.Name` is `LUMBER_CAMP`. `findAsset("LUMBER_CAMP")` and `findAsset("BUILDING_VILLAGE_CENTER")` return nil.
  The chain works: `asset.AssetCoreBuildingPart.ConstructorData` (`BUILDING_CONSTRUCTOR_DEFAULT`) `.CoreObjectPrefab`.
- `vec3f` and `quaternion` are method tables with no constructor. Copy a vector from an object and set `.x/.y/.z`.
- E3: `level:createObject(corePrefab, pos, rot)` creates a game object with only `COMP_BUILDING_PART` (not placed, no parent).
- E4: an empty object + `getOrCreateComponent("COMP_BUILDING")` + `AssetBuilding = asset` + part `setParent` → the building
  inits, but `getBuildingPartList()` is empty, there's no placement and no construction. Not a working building.
- COMP_BUILDING exposes only getBuildingPartList, getEditMode and getSubBuildingList (plus events) to Lua (exe binding names).
  No Lua binding places or constructs buildings. Result: replaying a building from Lua alone is not possible with the
  bound API (as far as tested).

Key finding (exe strings, not yet confirmed in disassembly):
- 28 `PlaCPlayerCommand*` classes: CreateBuildingPart, DestroyBuilding, AttachBuildingPart, MoveBuildingPart,
  RotateBuildingPart, RemoveBuildingPart, AssignBuildingFunction, ChangeVillagerJob, ChangeVillagerStatus, PaintZone,
  GameSpeed, Trade, StartMandate, SolveQuest, ToggleTerritoryRegion, PromoteVillager, ... plus `PlaCBuildingCommand*`
  (AddPart, MovePartData, RotatePartData, WallSplit, ...).
- A built-in deterministic replay system: `GamCReplayFrame`, `GamEReplayMode`, log channel `Engine - Replay`,
  `GamCGame::updateFrame` with "%d - Replay Seed is out of sync. %llu instead of %llu" and "%d - Too many IDs stored in frame",
  `GamCGame::pushPlayerCommand` with "Command %s ignored cause it has been pushed during replay", save fields
  `command commands frame deltaTime startSeed finalSeed`, and the object flag `IGNORE_REPLAY_SPEED`.

Derived: every player action goes through one function (`GamCGame::pushPlayerCommand`) as a serializable command.
The engine can replay recorded commands frame by frame and checks an RNG seed per frame. Object ids are stored per
frame, so ids stay deterministic. This is the same structure lockstep RTS multiplayer uses. A co-op design:
capture commands at pushPlayerCommand on each peer, serialize them with the engine's own serializer, broadcast them,
and inject them on every peer at the same frame (lockstep, the host decides the frame), checking the seed every frame.
This needs native reverse engineering (Ghidra): find pushPlayerCommand, the command (de)serializer and the replay
frame loop, then hook them from folderhook.dll (later a real co-op DLL). Unknown: how replay mode is turned on
(no command-line or file string found), and whether the simulation stays deterministic outside replay.

## Design draft: one city on two PCs (2026-11-05, guesses marked)

Goal from the owner: play ONE shared city on TWO PCs.

Model: host records, clients replay live (built on the engine's own replay system).
1. Same start: the host sends its save (or new-game seed and settings) to the client. Same game build (exe SHA-256) and the same mod list are required.
2. The host runs the simulation normally. Every frame, its replay recorder produces a frame record
   (deltaTime, the player commands pushed that frame, the ids created, the RNG seed). Guess: this is what
   `GamCReplayFrame` holds (fields seen: command, commands, frame, deltaTime, startSeed, finalSeed).
3. The host streams those frame records to the client over the network (TCP, in order).
4. The client runs in replay mode and consumes the frames as they arrive (a few frames of buffer).
   The engine already checks the seed per frame ("Replay Seed is out of sync"), which gives desync detection for free.
5. Client input: hook `GamCGame::pushPlayerCommand` on the client and serialize each command instead of executing it
   (in replay mode the engine already ignores pushed commands: "Command %s ignored cause it has been pushed during replay").
   Send it to the host, and the host pushes it as if it were local. It then comes back to everyone in the frame stream.
   Latency = round trip + buffer (fine for a city builder).
6. UI-only state (camera, open panels) stays local. Show the other player's camera position via the Lua mod (already readable).

What reverse engineering must deliver:
- the address and calling convention of `GamCGame::pushPlayerCommand` (xref of its log string) and how a command object is built
- the command serializer (the engine serializer with typename, so commands can go to bytes and back)
- the replay frame structure, how replay mode is entered (GamEReplayMode values), and how frames are fed during replay
- whether replay can start from a loaded save, not only from a new game (unknown)
Fallback if live replay can't be fed: lockstep on commands only (both peers run their own simulation, apply the same
commands at the same frame with a fixed deltaTime), using the seed check to catch desyncs.

## Session 2: Ghidra analysis (2026-11-05)

Tools: Ghidra 12.2 (`ghidra_12.2_PUBLIC_20261016.zip`, SHA-256 2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67,
from the GitHub release notes) and Temurin JDK 25.0.5+8 (`OpenJDK25U-jdk_x64_windows_hotspot_25.0.5_8.zip`, SHA-256
636bdc13dc649ddc7d5921c5c7d23cca6e44fd19d662b45cc9c02135a6c96c42, Adoptium API), both portable in <test dir>\tools.
Gotchas: Ghidra 12.2 needs JDK 25 (not 21). Headless scripts failed with Felix "The data file must be inside the data dir"
until `-Dapplication.settingsdir=<test dir>/re/ghidra-settings` was added to support/launch.properties.
Full auto-analysis of foundation.exe (image base 0x140000000) took 2515 s. Scripts: re/scripts/DumpByStrings.java,
DumpReplay.java, DumpAddrs.java. Launchers: re/run_analyze.cmd, re/run_script.cmd.

Observations (decompiled, build 26138506, exe SHA-256 5768...2B49; addresses valid only for this build):
- `GamCGame::pushPlayerCommand` = 0x1401b3210 (game*, GamIPlayerCommand* cmd).
  If game+0x3d8 == 2 (replay playback): it logs "ignored" and deletes cmd (vtable slot 1 = deleting destructor).
  Otherwise it appends cmd to an intrusive list in GamCGame: link-offset table at +0x3e8, count +0x3f0, head +0x3f8, tail +0x400.
  vtable slot 0 returns type info; (typeinfo + 0x20) is the type name used in logs.
- `GamCGame::updateFrame` = 0x1401c2bf0 (game*, float dt, ...). Per frame:
  1. mode (+0x3d8) == 1 (record): `FUN_14029eb60(replay, dt)` appends a new GamCReplayFrame (frame+0x18 = dt), and the frame start seed is set from game+0x3e0.
     mode == 2 (playback): `FUN_140294db0(replay)` returns the next frame (or null → replay deleted, mode = 0), and dt is replaced by frame+0x18.
  2. The simulation runs: `FUN_1401c27d0(world = game+0x1d0, dt * timescale(+0x330), ...)`.
  3. If not playing back: drain the command queue in order. For each cmd: `vtable+0x48(cmd, world)` = EXECUTE;
     if a replay object exists, `FUN_140298250(replay, cmd)` serializes cmd into the frame (engine object serializer,
     with the type name); then delete. Then the frame's final seed (frame+0x28) = game+0x3e0.
  4. If playing back: for each stored command, `FUN_140294bf0(frame, world, serializer)` deserializes it, then execute and delete.
     If game+0x3e0 != frame+0x28: "Replay Seed is out of sync".
  5. The global frame counter DAT_143fcff48++.
- Replay object: +0x30 handle to the serializer store, +0x38.. intrusive frame list (tail +0x50 = current frame), +0x58 playback cursor.
  GamCReplayFrame: 0x90 bytes, ctor FUN_14027ed80 (only called from FUN_14029eb60 → loaded replays come from the generic
  reflection deserializer). No code found that creates the replay object or sets mode 1/2 (set-replay/reset = FUN_1401b0580 sets it to 0).

Derived design (replaces the "live replay" draft): own lockstep in an updateFrame hook, no replay object needed.
- Hook 0x1401c2bf0 on every peer. Before the original runs: (a) unlink the commands the local player pushed this frame
  (queue +0x3f0..+0x400) and send them to the host; (b) wait for the host's frame N {dt, commands}; (c) put those commands into
  the queue in host order and call the original with the host's dt. All peers then run the same simulation steps with the same commands.
- After the original: send the seed (game+0x3e0) for frame N. A mismatch = desync (the engine's own check, done by us).
- Command bytes: reuse the engine serializer as used by FUN_140298250 / FUN_140294bf0 (next RE target), or call those with a
  minimal replay object.
- Open: whether the simulation is deterministic outside replay (the seed suggests yes, but threads, float timing and the job
  system are unknown); how a joining peer gets the same start state (send the save file, then load on both at frame 0).

## Session 2, part 2: native hook and command transport (2026-11-05)

`foundation-coop/native/coop.c` → coop.dll (Zig 0.17.1, `-target x86_64-windows-gnu`, `-lws2_32`), loaded by launch.exe
as a second APC after folderhook.dll. It hooks updateFrame (0x1401c2bf0) with a 15-byte prologue copy and a 14-byte absolute
jump; it patches only if the bytes match build 26138506. Owner approval for a localhost-only receiving DLL in the test setup: 2026-11-05.

Test run 13 (PASS, step A): the hook logs per frame. Player commands seen live:
`.?AVPlaCPlayerCommandCreateBuildingPart@@` (preview placed), `.?AVPlaCPlayerCommandBuildMonument@@` ("Bauen").
The game log also prints executed commands: channel "Gameplay - Player Commands", e.g.
`PlaCPlayerCommandCreateBuildingPart::execute  Placing building part VillageCenterPart.data on building VILLAGE_CENTER`.

Transport findings (decompiled + crash dumps read with foundation-coop/bot/mdinfo.py):
- `ComCBinarySerializerWriter` ctor 0x140126b20(mem 0x70, char compress). Vtable 0x142c78bb8: slot 1 saveToFile,
  slot 3 (+0x18) `bool getBytes(writer, std::vector<u8>*, status*)`. The status pointer is REQUIRED: the compressor
  FUN_140141e50 writes status+4 (passing garbage crashed). slot 29 (+0xe8) sets name/version.
- `ComCBinarySerializerReader` ctor 0x140126a20(mem 0x58). Vtable 0x142c783c0: slot 5 (+0x28) `bool loadFromBuffer(reader, data, size)`,
  slot 12 (+0x60) find(reader, out[2], nodeId, key, type), slot 31 (+0xf8) readString.
- updateFrame calls the recorder FUN_140298250(replay, cmd) after every executed command whenever game+0x3c8 != NULL,
  in any mode. Arming a fake replay object (writer holder at +0x30, current frame at +0x50) makes the engine serialize commands for us.
- The object-id generator FUN_1401a5e30: while a replay is attached, every new object's GUID is appended to
  `std::list` at frame+0x60 (size +0x68); in playback (mode 2) GUIDs are taken from that list (cursor +0x70). This is how the
  engine keeps object ids identical in replays, and co-op needs the same thing. The fake frame needs a valid empty list (sentinel).
- Memory the engine may free must come from ucrtbase's heap (the writer holder is freed with sized delete 0x10).

Test run 14 (partial): B (send) serialized its first command: CreateBuildingPart, 76 bytes compressed, status 0, 71 new
object ids that frame, sent as 92 bytes over UDP 127.0.0.1:27800. B kept running. A (recv) crashed in
ComCObjectDefinition::instantiate (0x14020fb20, null class definition): readCommand found no type name for the node id.
Next: the check added in coop.c (find + readString before instantiate) will show whether node ids from the writer are valid
after loadFromBuffer, or need remapping. Not run yet (blocked by the permission check; it needs an owner rule).

## Session 2, part 3: first command executed across instances (2026-11-05 night)

Settings: owner approved the localhost test DLL; scoped allow rules added in the session workspace
`.claude/settings.local.json` for `foundation-coop/bot/run_ab_test.ps1` and `foundation-coop/native/build.sh`.

More serializer facts (decompiled + packet dumps, build 26138506):
- Writer node layout (0x18 bytes in the writer buffer at writer+0x38, id = byte offset): +0 name string index, +4 next top-level
  node, +8 next sibling, +0x10 first child, +0x14 last child. Slot 22 (+0xb0) createNode, slot 25 (+0xc8) addChild(parent, child),
  allocator FUN_1401414e0 (+0x40 capacity, +0x44 used).
- getBytes writes: header {version, compressFlag, compressedSize, rawSize}, then zlib of {string table, data size,
  top-level head, nodes}. ONLY the top-level list (head at writer+0x58, linked via node+4) is serialized. The replay recorder leaves
  command nodes detached, so they must be chained into writer+0x58 before getBytes (otherwise the packet is just the string table).
- The reader keeps the same offsets: reader+0x30 data, +0x38 size, +0x40 top-level head (next at data+id+4).
  The reader rewrites string indices into pointers after load (FUN_140180f40).
- readCommand's third argument is a reader CONTEXT, not just the reader: [0] = binary reader; the object reader stores the
  context at +0x50 and resolves object references (GUID property, key 0x143fd2a00) through an unordered_map at context+0x28
  (FUN_1402993e0 → FUN_140185400, hash 0x9e3779b9, node value at +0x20). An empty valid MSVC unordered_map makes
  unknown references resolve to null.

Test run 20 (PASS for transport, observation):
```
B: CreateBuildingPart serialized, 163 bytes (179-byte packet); BuildMonument 130 bytes (146-byte packet)
A: recv … reader top-level nodes 1, type-name found
A: recv:   command 0 -> … .?AVPlaCPlayerCommandCreateBuildingPart@@   (pushed, executed next frame)
A game log: PlaCPlayerCommandCreateBuildingPart::Building should not be null
            PlaCPlayerCommandCreateBuildingPart::execute  null component building when placing building part VillageCenterPart.data
```
A rebuilt B's command (with the correct building-part asset) and its engine executed it. It failed only because the
referenced building does not exist in A: B's build tool created that preview building directly (not through a command),
with a B-only GUID. A exited after the following BuildMonument (also null building), so it is likely a crash there (not checked).

Derived: next problem = object identity. Options:
1. Replicate the build tool's preview-building creation: when a command references a GUID the receiver does not know, create
   the matching building (asset from the sender) on the receiver and map sender GUID → local object in the context map.
2. Make GUIDs identical on both sides by using the engine's replay id list (frame+0x60, consumed in mode 2 by FUN_1401a5e30)
   while the receiver creates those objects.
3. Long term (lockstep): every peer must run the same object creations; UI-side creations (previews) must become commands or be
   replicated before the commands that reference them.
Also seen: the two instances' seeds already differed on the same map code, so the "same map code" setup alone does not give
identical states (unknown why: maybe a time-based seed per new game).

## Status at end of session 2 (2026-11-05, autonomous run)

Working (tested on one PC, two test instances, localhost):
- Two instances with private user folders (folderhook.dll + launch.exe).
- coop.dll hooks updateFrame, logs every player command, and transports commands B → A using the engine's own
  serializer. A rebuilds the command object and its engine executes it.
- Bot scripts: newgame.ps1 (wizard, fixed map code, test rules, village), run_ab_test.ps1 (whole A/B test), ui.ps1.

Not working yet / open, in order:
1. Object identity. Commands reference objects by GUID (e.g. CreateBuildingPart.Building = the preview building that the
   build-tool UI created without a command). The receiver needs (a) the live GUID→object map instead of the empty test map,
   so shared objects resolve, and (b) a way to create the sender's UI-created objects with the SAME GUID (the engine's
   replay id list, frame+0x60 consumed in mode 2 by FUN_1401a5e30, is the natural tool).
   RE targets: the live registry behind Lua `LEVEL:find(id)`, and what the context map value is (object vs component).
2. Same starting state: two "new games" with the same map code already had different seeds. Plan: the host saves,
   the client loads that exact save (file transfer), then lockstep starts at a known frame.
3. Lockstep timing: the host decides per-frame dt and the command list; the client waits (dt from the host, like replay mode 2).
   Then compare game+0x3e0 seeds per frame (desync detection).
4. Networking beyond localhost: authenticated session, accept packets only from the configured peer, allow only known
   command types, size limits. Required before any real two-PC use.
5. Crash-proofing: the receiver must validate packets fully (currently test-only checks).

## Session 3: object identity solved for placed buildings (2026-11-05 morning)

Findings (decompiled, build 26138506):
- PlaCPlayerCommandCreateBuildingPart::execute = 0x14125ff40 (vtable 0x142f2fc28, slot 9). The building is kept as a weak
  HANDLE at cmd+0x50, resolved at execute through the global ComCRefManager<GamCComponent> at 0x143fd4e28
  (FUN_14020e420). Handle = (generation << 32) | slot index, slots = 16 bytes {int gen; pad; void* obj}. It creates the part with
  FUN_140d72fa0(assetPart, world, building, 0). The command carries NO transform.
- CreateBuildingPart is pushed when the build tool OPENS (it creates the preview part). The player then drags the preview
  directly (UI, no command); BuildMonument ("Bauen") finalizes. So part transforms are pure UI state.
- The preview building itself is created by PlaCCompBuildingManager::createBuilding = 0x140c81540(manager, asset**, parent{handle,ptr})
  → COMP_BUILDING*. No player command for it (28 command classes, none creates buildings).
- Asset identity: asset GUID at asset+0x14; GUID → asset via the asset manager singleton FUN_1402186c0 + FUN_1401a1a80
  (asset object at node+0xf0). The same GUID in both processes.
- Object identity: game object GUID at +0x1d0 (same byte order as the Lua getId() string); component owner at component+0x28.
- The reader context map (context+0x28) maps GUID → owner GAME OBJECT pointer (not a component, not a handle). The engine then
  finds the component of the property's type (FUN_14028e8d0) and derives the weak handle itself.
- The serialized "Building" GUID of a command = the sender's building owner GUID = sender Lua getOwner():getId().

Implementation (coop.dll + mod):
- Sender hooks createBuilding and sends "FOCB" + asset GUID. The receiver creates the same building with its own manager
  (captured from its own createBuilding call) and queues the owner object as pending.
- Receiver hooks the reference resolver FUN_1402993e0 while it deserializes peer commands: an unknown GUID is bound to the oldest
  pending building and returned as owner pointer; it writes "peerGuid localGuid" to <instance>\bindings.txt.
- Lua mod: the sender sends "T <buildingGuid> <partIndex> <pos> <quat>" on every ON_PREVIEW_PART_PLACED (UDP 27793). The
  receiver maps the GUID via bindings.txt (read through the moddev bridge's readFile), finds the same part by index and calls
  setGlobalTransform on the part's game object.

Test run 27 (PASS): B built a woodcutter camp (and its village centre); A, with no clicks, got:
```
recv: CB asset 57f98d74d44a4767a3e2f721556ec8cc (PlaCAssetBuilding) -> created … ; resolve: peer guid … -> new binding
A game log: CreateBuildingPart::execute  Placing building part … ; BuildMonument::execute  Ordering Monument Construction
XF applied peer d733d02e-… -> local e9a6fca2-… ok=true now 575.73,2.68,611.84
A parts: Lumber_Core_Part pos=575.73,2.68,611.84 (B: 575.7325 2.6821 611.8420); building root 576.21,2.68,611.71
```
Both instances stayed alive. Limits: the pending binding is FIFO (one building at a time); single part per building tested;
one direction only (B → A); the other UI-only actions (move/rotate preview, zones, roads) are not covered; the game states
are not otherwise synchronized (different seeds); localhost only.

Test run 29 (PASS, two-way): roles FCOOP_ROLE=a / b (A listens 27800, B 27801; Lua transforms on 27793 / 27794).
Peer commands are now executed directly on receipt (vtable slot 9 execute(world), slot 1 deleting destructor), outside the local
queue, so they are never re-serialized and echoed back. The Lua side ignores preview events while it applies a peer transform.
B built a woodcutter camp at screen (920,440), A built one at (1000,480):
```
A: recv CB 57f98d74… -> created; resolve peer 111757f6… -> binding; CreateBuildingPart + BuildMonument executed;
   XF applied peer 111757f6… -> local 1e1402ef… now 575.73,2.68,611.84
B: recv CB 57f98d74… -> created; resolve peer 2c2f8731… -> binding; CreateBuildingPart + BuildMonument executed;
   XF applied peer 2c2f8731… -> local fe0cbb35… now 590.33,2.68,623.72
```
Both instances stayed alive. Messages sent while the peer is not running are lost (no reliability layer yet).

## Lockstep design (2026-11-05, derived from the code; not built yet)

GUIDs: FUN_14016cdb0 = boost uuid v4 from BCryptGenRandom (994 callers) → two running games never agree on ids.
The engine's replay playback (mode 2) already solves this: updateFrame takes dt, commands AND the ids for new objects
(frame+0x60 list, consumed by FUN_1401a5e30) from the replay frame, and compares the RNG seed at the end of each frame.
Plan:
- Host: arm the recorder every frame; after the frame send {frame no, dt, serialized commands + node ids, created-id list
  (frame+0x60 list values, 16 bytes each), seed after (game+0x3e0)}.
- Client: game+0x3c8 = fake replay, game+0x3d8 = 2. Hook play_nextFrame FUN_140294db0 to return the next host frame
  (block until it arrives; never return null, or the engine ends the replay). Context for readCommand at *(R+0x28).
  The engine then replays the host frame exactly; "Replay Seed is out of sync" in the log = desync detection.
- Client input: in mode 2 the engine drops pushed commands. Instead, serialize them with the recorder FUN_140298250 called
  directly (no execute) and send them to the host, which pushes them into its queue → they come back in the frame stream.
- Start: both load the same save, paused; lockstep starts; the host unpauses with a GameSpeed command executed by both on the same frame.
- UI-created preview objects (build tool) must stay local until commit; on "Bauen" a commit command recreates
  createBuilding + parts + transforms + BuildMonument inside the frame on both peers (later step).
Unknown: whether the simulation is deterministic (threads, job system); whether objects created outside updateFrame
(UI) avoid consuming replay ids (game+0x3d0 is only set inside updateFrame, so probably yes).

## Session 3, part 2: LOCKSTEP WORKS (spectator stage) (2026-11-05)

Setup: both instances load the same save (bot loadsave.ps1; a fresh Save Game\COOP folder; the game groups saves by village id,
so the newest save is at the top of the right list; loading starts PAUSED, speed x0). Note: the save dialog has "In der Cloud
speichern" ON by default; it was switched off for the test save (Steam Cloud could otherwise sync test saves into the owner's
real save folder). The owner's real save folder was re-hashed after saving: 0 differences.
Seeds after loading the same save differ between processes (RNG state is not in the save) → the client takes the host's seed.

Implementation (coop.dll, FCOOP_ROLE=host / client, trigger file <instance>\lockstep_go):
- Host: every frame arms the recorder (fake replay, mode 0), runs the frame, sends {seq, dt, arg3, seed before, seed after,
  GUIDs, serialized commands} to UDP 27801. Waits (throttle) when more than 8 frames ahead of the client's last ack.
- Client: on the first host frame, sets game+0x3c8 = fake replay, game+0x3d8 = 2 (playback). play_nextFrame (FUN_140294db0) is
  REPLACED (its prologue has a relative jump; the original logic is re-implemented for real replays) and returns frames built from
  host packets (dt, seeds, commands + reader context). In mode 2 the engine sets game seed = frame start seed every frame and
  compares the end seed. The client acks every frame (UDP 27800).
- Object GUIDs: per-frame id lists failed ("Recorded frame does not contain any more ID" / "Too many IDs stored in frame"):
  the same objects are created on both sides but sometimes a frame apart (all on the main thread, inside updateFrame).
  Fix: an ordered, frame-independent GUID stream. Hook GamCLevel::internalCreateObject (FUN_1401a5e30) and the GUID generator
  FUN_14016cdb0 (boost uuid v4 / BCryptGenRandom, 994 callers; only active while inside internalCreateObject):
  the host records every generated GUID in order, and the client hands out the same GUIDs in the same order (its replay frame
  pointer game+0x3d0 is hidden during creation so the engine skips its per-frame list).
- The resolver also falls back to "live object with this GUID" (registry scan, owner GUID at +0x1d0).

Test run 36 (PASS): load the same save, lockstep_go, unpause the host only (speed button) → the client follows; 45 s of play,
then pause on the host (a GameSpeed command, replayed by the client). State fingerprint via the Lua dev channel (statehash.lua):
```
HOST:   day 1 villagers 10 sum 5679.366,5777.269 buildings 1 | 061b9113@566.61,577.83 14044246@565.00,580.29 …
CLIENT: day 1 villagers 10 sum 5679.366,5777.269 buildings 1 | 061b9113@566.61,577.83 14044246@565.00,580.29 …
```
Same villager GUIDs and positions. Client log: seeds equal at every check, desyncs 0, queue steady at 6–7 frames.
The engine's own replay checker: 0 errors. Limits: the client cannot act yet (spectator), localhost only, short run.

## Session 3, part 3: client actions + building in lockstep (2026-11-05 morning)

- Client input (PASS): pushPlayerCommand (0x1401b3210) hooked on the client; while lockstep runs, each command is serialized
  WITHOUT executing (recordCommand FUN_140298250 called directly with a temporary fake replay), sent to the host (UDP 27800,
  "FOLC"), deleted locally. The host deserializes and pushes it into its own queue, so it is executed + recorded + streamed back.
  Test: the client pressed pause → PlaCPlayerCommandGameSpeed went client → host (queued, executed at host seq 1255) → both
  paused; state fingerprints identical afterwards (villagers, GUIDs, positions).
- The GUID stream now only covers creations inside the simulation frame (UI creations stay local).
- UI createBuilding (outside updateFrame) as a lockstep event: the host captures exactly the GUIDs of that call and sends
  {asset GUID, GUIDs}; the client calls createBuilding (manager found by RTTI "PlaCCompBuildingManager" in the registry) with
  those GUIDs fed → the same building GUID on both (PASS).
- But mirroring the host's build UI is not enough (FAIL, analysed): the UI also creates the preview PART outside the frame,
  and sets UI state: part IsPlaced=true (host) vs false (client); building EditMode=true (host) vs false (client) (both are
  reflected properties). The client then creates the part inside the frame (different number of objects → GUID stream
  starvation "no host id left") and the first desync follows right after BuildMonument.

Plan (commit-based building, next): previews stay local; on "Bauen" the builder sends a construct event {building asset,
parts (asset + transform)}; host and client both execute it at the same frame inside the simulation (createBuilding, create parts,
set transforms + placed state, finalize); the local preview is discarded. Needs: native transform setter, part placement /
construction entry points (RE).

## Session 3, part 4: building in lockstep, both directions (2026-11-05)

Instead of the commit-based plan, the UI calls that change simulation-relevant state are replicated as lockstep events.
New entry points (all hooked in coop.dll, role host/client only):
- `PlaCCompBuilding::setEditMode(comp, mode, force, notify)` FUN_140cd1fa0 (writes comp+0x744; fires ON_EDIT_MODE_CHANGED
  at comp+0x250; recursive). Prologue 48 8b c4 44 88 48 20 44 88 40 18 88 50 10.
- `PlaCCompBuildingPart::setIsPlaced(part, placed, recursive)` FUN_140cd3b90 (writes part+0x4fd = Lua getIsPlaced; fires
  ON_PART_PLACED part+0x318 and the building's ON_PREVIEW_PART_PLACED). Prologue 48 89 6c 24 18 56 48 83 ec 30 0f b6 ea 48 8b f1.
- GameObject transform natives: getGlobalPosition FUN_1404bf320(go, float[3]), getGlobalOrientation FUN_1404bf2a0(go, float[4]),
  setGlobalTransform FUN_1404cb3a0(go, pos[3], quat[4]) (Lua thunk FUN_14030f790).
- `ComCEvent<PlaCCompBuildingPart*>::trigger` FUN_1409ffa70 (used by CreateBuildingPart::execute for the build tool's
  "part created" callback stored on the command at cmd+0x28).
- readCommand FUN_140294bf0 (node index at frame+0x78, read before increment), recordCommand FUN_140298250.
- Build tool update FUN_140c28be0 → FUN_140c396a0(tool, mode) → setEditMode(building, mode, 0, 1): runs INSIDE updateFrame,
  only on the machine whose player uses the tool (return address 0x140c39730).

Event types (24-byte head {u32 type, owner GUID[16], u8 a, b, c, flag}):
- 1 createBuilding {asset GUID, n, GUIDs} (existing), 2 setEditMode, 3 setIsPlaced (+ pos[3] + quat[4], 56 bytes total).
- Host: UI calls outside the frame are captured and streamed with the next frame; the client applies them before that frame.
- Client: its own UI calls (createBuilding with captured GUIDs, setEditMode, setIsPlaced + transform) are applied locally and
  sent to the host ("FOLE" packets, UDP 27800); the host applies them (createBuilding with the client's GUIDs fed) without echo.
- In-frame build-tool setEditMode calls are DEFERRED to a shared frame boundary (flag byte = 1): host applies after the frame
  and streams it with the next one; client suppresses it, sends it, and applies it when the host streams it back.
- Client commands: the client keeps its original command object (it carries UI callbacks); the host marks client-issued
  command indices per frame (recordCommand hook, packet field ncli); for CreateBuildingPart the client executes its original
  instead of the deserialized copy (other types use the host's copy). Part-event listeners on a command (cmd+0x28) inside
  the frame run isolated (RNG seed restored, created objects get local GUIDs).
- The Lua XF transform sync is off for host/client (it echoed the host's (0,0,0) preview transform back).

Test runs (load coop-start save, 2 instances, localhost):
- Host builds a woodcutter's camp (preview, place, Bauen): client gets the same building + part GUIDs and position
  (575.73, 2.68, 611.84); 0 desyncs; state fingerprint identical after 2 min of construction (PASS).
- Client builds a woodcutter's camp: the request goes client → host; same GUIDs, same position, construction on both;
  0 desyncs; identical state after 1.5 + 2.5 min (PASS).
Earlier failures on the way: client build tool never showed the preview (callback on the deleted command object); client
part at (0,0,0) (Lua XF echo); seed desync in the Bauen frame (build-tool setEditMode inside the frame, client only).

Bot notes: the "Rang 5" popup after unpausing eats clicks for ~30 s; the first placement click often only picks the preview
up (client_lumber.ps1 now waits and retries until coop.log shows setIsPlaced (1,1,0)). compare.ps1 pauses the host and
compares statehash.lua on both (retries while the client catches up).

Known limits: localhost only, no authentication yet; only building placement was tested (no deletion, roads, zones,
multi-part monuments); seed check only detects RNG divergence; save transfer between PCs not done.

## Session 3, part 5: authenticated network transport + player kit (2026-11-05)

- Lockstep traffic moved from localhost UDP to ONE TCP connection (reliable + ordered; UDP had no retransmission),
  `foundation-coop/native/lsnet.h`: pre-shared 32-byte key (`<instance>\coop_key.txt`, 64 hex), host listens on
  FCOOP_LS_BIND:FCOOP_LS_PORT (default 127.0.0.1:27810) and accepts only FCOOP_LS_ALLOW; nonce handshake
  (HMAC-SHA256 via Windows BCrypt, no extra download), session key, every message HMAC'd + strictly counted;
  known types per direction only (host: ACK/CMD/UIEV, client: FRAME); 1 MiB cap; host frames must have exactly
  consistent section lengths; client commands must match their declared length. Not encrypted (documented).
- Tests on this PC (127.0.0.1): client builds over TCP → identical state, 0 desyncs (PASS); wrong key → "client failed
  authentication", no session (PASS); FCOOP_LS_ALLOW=10.0.0.9 → "rejected connection from 127.0.0.1" (PASS).
- `lockstep_go` is ignored until a city is loaded (PlaCCompMainGameLoop exists) — pressing Enter too early is harmless.
- Player kit: `foundation-coop/play/` (start.ps1, make_kit.ps1, README.md) → `<test dir>\kit` with
  launch.exe, folderhook.dll, coop.dll, start.ps1, README, SHA256SUMS. No Lua mod / dev bridge for players (the dev eval
  channel stays test-only). Kit run on this PC without the test mod: key exchange, go after load, 0 desyncs over
  ~3000 frames, client build requests reached the host (bot placement click missed: different camera without the mod).
- NOT yet tested: two real PCs (needs the user: firewall prompt on the host, the friend's PC, LAN or VPN).

## Session 4: drift detection by comparing the game's own saves (2026-11-05)

- Quicksave = Ctrl+S (input.config QUICK_SAVE: 0x11+0x53); it works on the client too (local only, no command).
- Save file: u32 3, u32 1, sizes, PNG thumbnail (zlib IDAT), then the world as one zlib stream (~97 MB raw) in the
  engine's binary tree format (ComCBinarySerializerReader, vtable 0x142c783c0; loadFromBuffer FUN_140164af0):
  `u32 nkeys, NUL-terminated key names, u32 datasize, data`; `data[0]` = root offset;
  node = {u32 key, u32 next sibling, u32 X, u32 Y}. Leaf: X = type (0 int32, 1 int64, 4 float, 5 double, 7 string,
  8 GUID, 17 UTF-16 string, 6 list → Y first item, 2/16 typed object with child links). Object: X = first child
  element, Y = first attribute (XML-like). Children always come after their parent.
- `bot/savediff.py A B`: walks both trees side by side (objects matched by "id" GUID, map Items by Key), ~16 s and
  ~300 MB for two 97 MB worlds. Wall clock, UI, notifications and animation fields are counted separately.
  Sanity check: start save vs later save → 5337 simulation fields differ (works).
- `bot/savecheck.ps1`: pause host, wait for equal Lua fingerprints, Ctrl+S on both, savediff.
- Earlier lockstep session (paused after ~10 min): 0 differing simulation fields out of ~1.9 M
  (59 UI/visual-only differences: animations, speech-bubble timers, notification timers/panels).
- Note: an older version of savediff kept every field in memory (16 GB) and probably ended the previous session.

## Session 4b: hunting drift (residential zones) — sources of non-determinism found and fixed (2026-11-05)

Test method: `bot/bisect.ps1` (play N s, pause host, quicksave both, savediff; repeat). Scenario results:
- Soak 11 in-game days at x3 with client speed changes: IDENTICAL (0 simulation fields differ).
- Residential zone: DRIFTED (houses built differently, later villagers). Causes, in the order found:
  1. createBuilding inside the frame (the zone system building houses) was captured as a UI event -> duplicate
     house on the host -> crash. Fix: only out-of-frame createBuilding calls are UI events.
  2. GUID stream as ordered FIFO per creation path: one object created early/late on one side (pooled spawner,
     timer driven) shifted every later GUID on that path. Fix: host ids are tagged with their frame; the client only
     uses an id from the same frame (+-2), otherwise a local id; no cascade, no 500 ms waits.
  3. CRT rand()/srand() (~90 callers): per-process state. Fix: IAT hook; on the simulation thread inside the frame
     rand() is a lockstep LCG whose state travels in the frame header (rand0/rand1); srand ignored there.
  4. Navmesh processing ("Heavy Algo", PlaCNavMeshProcessing) submits thread-pool jobs (FUN_1401f8c40, 148 callers)
     from inside the frame -> results arrive at machine-dependent times. Fix: jobs pushed from 0x1407b0000-0x140800000
     on the simulation thread inside the frame run inline. (Running ALL in-frame jobs inline crashed in the NVIDIA GL
     driver: texture uploads (GenCTexture 0x140443c40) and audio jobs must stay on the pool.)
  5. The house lot seed: PlaCHouseLot ctor (FUN_141315040) does seed = next(G)<<32 | next(G) with a SECOND global
     random generator G = 64-bit LCG at 0x143fce520 (next FUN_140163e10, range FUN_1401633d0; 254 + 94 callers),
     also used by render/other threads during the frame. Fix: hooks redirect the simulation thread's calls (inside the
     frame) to a lockstep copy whose state travels in the frame header (grng0/grng1).
  6. The pooled object spawner FUN_140279840 (timer driven, runs at different frames per machine) drew from the
     lockstep generators. Fix: while it runs everything is "local" (own GUIDs, real random generators).
- After the fixes the residential-zone test is IDENTICAL (0 differing simulation fields, 0 seed/rand/global-RNG
  desyncs). Non-simulation fields ignored by savediff: UnscaledStartTime, house upgrade requirement cache
  (InstRequirementListByLevel/.../Level, lazily filled, values look uninitialized), notifications, animations.
- Diagnostics left in coop.dll: per-frame RAND / GLOBAL RNG DESYNC checks, "local id" log, in-frame job log,
  OS RNG caller log (only the GUID generator draws OS randomness inside the frame), lot build dumps (lot_NN_ctor.bin).
- Known remaining risk: component registry indices (handles = gen<<32|index) differ between machines whenever an
  object exists on one side only (e.g. UI/tool visuals); logic that orders by handle could still drift.
- Long test after the fixes: both players paint zones (host residential, client logging), x3 speed, 8 save
  comparisons over 27 in-game days: IDENTICAL every time, 0 desyncs.
- Held build preview (client keeps a building preview in hand for minutes): drifted (resources "older" on the
  client: DaysSinceReplenish 3 vs 2, tree Weight ahead, need timers). Two more fixes:
  7. The build tool moves the preview INSIDE the frame on the player's machine (PlaCBuildableMover::processMove
     FUN_140cbf4c0 and FUN_140c47070 -> GameObject setGlobalMatrix FUN_1404cabc0). These matrix writes are deferred
     to the shared frame boundary (event type 4 = {object GUID, matrix}); the client's preview follows the cursor one
     round trip late.
  8. More simulation jobs left on the thread pool: PlaCDesirabilityGrid and residential/lot lambdas (callers
     0x1412ecff2, 0x141324895, 0x1412ee9c1). Inline whitelist is now 0x1407b0000-0x140800000 and
     0x1412e0000-0x141350000. (The slower client frames with the build tool open changed when pool jobs finished.)
  Result: held market preview, 4 rounds over 11 days: villagers/parts identical, 1 differing field (the preview
  itself), 0 desyncs. Jobs still on the pool inside the frame: textures (0x443e1f, 0x44371f), audio (0x111cae3).
  9. More in-frame pool jobs: resource containers (PlaCCompResourceContainerManager 0x140b15bc0, 0x140b065b0) and
     0x140fdc640; whitelist extended with 0x140b00000-0x140b20000 and 0x140fa0000-0x140ff0000. Still on the pool
     (render): signed distance fields 0x14043668d, texture arrays 0x140445ee2, 0x14048737d.
  10. Objects created while applying a UI event (e.g. construction-site objects when edit mode ends) got different
     GUIDs on both sides ("component not found" for later events). Now every event 2/3/4/5 carries a GUID trailer
     (u32 n + GUIDs): whoever applies it first captures the GUIDs (g_cb_mode 1), the other side feeds them (mode 2);
     events are applied with UI semantics (g_in_frame = 0) on both sides.
  11. Cancelling a build preview (X) = destroyBuilding FUN_140c85bd0 called from 0x140c86493 inside the frame on
     the player's machine only. Deferred like the tool's setEditMode (event type 5).
- savediff also ignores each player's camera (object with a LookAt component), render settings (grahicRoot) and
  the build menu "new" badges (NewlyUnlocked).
- Tests after 9-11 (two instances, one PC): client pans its camera far away: IDENTICAL; host and client each open
  a build tool, move the preview, cancel: IDENTICAL x2; host builds a woodcutter's camp, then the client builds one
  (preview follows the mouse via the host): IDENTICAL x3 over 11 days, 0 desyncs.
- Still open: a woodcutter preview HELD for minutes on the client made all resource containers "age" faster on the
  client (DaysSinceReplenish 11 vs 9, tree Weight ahead), cause not found (not stored in the container component;
  possibly a lazy update triggered by the tool's harvest-area query). Normal short placement does not drift.
- Harness: run_ab_test.ps1 now waits for old game processes and verifies the coop.dll hash (a locked DLL had
  silently left the old build in place for some runs). Test diagnostics (creation log, lot dumps, RNG caller log)
  only run with FCOOP_DEV=1, which only the test harness sets.
