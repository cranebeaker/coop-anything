# Far Cry (2004)

| | |
|---|---|
| Engine | CryEngine 1, 32-bit x86. Game logic in `CryGame.dll` (C++, source in the official MOD SDK 1.4) and Lua 4 scripts in `FCData\Scripts.pak` |
| Versions tested | Steam, app id 13520, version 1.4.0.1405. `Bin32\FarCry.exe` SHA-256 `21a907fae0b6e2d4b10d546dba0965ef89c39a69f8cb00036307c0564ca1a813`, `CryGame.dll` SHA-256 `db9c7660b698600ae34dd4a18f7ca5989911ad51bf9286ba1cc047ae52ef7634` (PE timestamp `0x44e2f64d`). Every address below is for that `CryGame.dll` |
| Projects | [FarCryCoop](https://github.com/Bantila/FarCryCoop/tree/main) (new, this write-up; MIT); [Assault Coop](https://www.moddb.com/mods/assault-coop) (2011); [FCAV](https://farcry1.itch.io/far-cry-advanced-version) (co-op announced, not released) |
| QA level | none (FarCryCoop: two players checked by hand by the author, no automated test); community-reports (Assault Coop) |
| Last updated | 2026-10-22 |

## Where things stand

FarCryCoop (0.2 beta, started 2026-10-18) runs the stock campaign maps as a network game. The host runs a listen server and plays as in single player; friends join as clients. Movement, weapons, hits on enemies, enemy AI, area triggers, mission objectives, map changes and respawn after death work for two players. The host also has campaign saves (autosave at a checkpoint, save and load commands (F6 / F7 untested), loading a save from a fresh start); they keep the players and the dead enemies, not the mission progress. The launcher lists the maps in story order and sets the difficulty (the five levels of the single-player menu). Missing so far:
- helicopters for clients;
- pushing physics props;
- cutscenes for clients;
- mission progress in saves;
- smooth paraglider flight as seen by other players.

A full campaign has not been played through. A sweep of all 20 levels (two games on one PC, map changes, hops through up to 4 checkpoints per level) found:
- 18 levels loaded at first; **Treehouse and Swamp crashed at the level load** (`CRITICAL ERROR:: ReadPacked i== 20`, [#1](https://github.com/Bantila/FarCryCoop/issues/1), now worked around: see the table below); after the workaround all 20 load, and Treehouse, Swamp, Factory, Dam and Volcano were swept again with working checkpoint hops;
- a level change **sometimes crashes the host** (random heap corruption in `CrySystem.dll`: Bunker -> Steam 3 of 11 runs, Cooler -> Boat 4 of 25, [#2](https://github.com/Bantila/FarCryCoop/issues/2)); a client lost once at Cooler -> Boat did not repeat;
- on 9 levels (Training, Carrier, Fort, Research, Bunker, Regulator, Rebellion, Archive, Cooler) both players stood at the same spot after every checkpoint hop; on Control, Pier and Fort the client was late or far at times ([#3](https://github.com/Bantila/FarCryCoop/issues/3), closed: the server now watches a move of a remote player and sends it again until the player arrives).

**Written but not tested in game:** joining through Steam (Spacewar "Join Game"), the launcher lobby, the player shirt colors, and the F6 / F7 save keys. Everything about them below is what the code is meant to do, not something seen working.

## Existing work

| Project | Scope | Networking | Evidence and limits |
|---|---|---|---|
| [Assault Coop](https://www.moddb.com/mods/assault-coop) v3.0 (2011) | about 7 campaign maps re-edited for co-op, randomised AI | LAN / direct IP | Players report unfinished missions and missing cutscenes. Its author forbids reuse of the scripts without permission. FarCryCoop took only one idea from it (a client relay for area triggers), and later dropped it. |
| [FCAV](https://farcry1.itch.io/far-cry-advanced-version) | patch and fix mod; co-op "being worked on" | unknown | not released |
| [FarCryCoop](https://github.com/Bantila/FarCryCoop/tree/main) | stock campaign maps, all missions in principle, 2-4 players (2 tested) | direct IP, VPN; Steam Spacewar relay and launcher lobby over TCP (both untested) | write-ups and every address in its `games/far-cry/NOTES.md` |

## How the game works (for co-op)

Observations from the FarCryCoop notes, each checked in game or in logs:

- Single player is already a local server plus a local client. `Scripts\Default\gamerules.lua` (the single-player rules) handles `OnClientConnect`, `OnClientDisconnect` and `RespawnPlayer` per server slot.
- The game type is the Lua global `g_GameType`. A campaign map starts as a network server with `Game:LoadLevelListen` (listen server, the host keeps a local player) or `Game:LoadLevelMPServer`. A second call of `LoadLevelMPServer` changes the map and keeps the clients connected.
- Mods live in `<game dir>\Mods\<name>\` with their own `.pak` that overrides scripts. Start with `FarCry.exe -DEVMODE -MOD:<name>`; `#` Lua console commands need `-DEVMODE`.
- A custom game type (`COOP`) can simply load the single-player rules and add to them. Client to server: `Client:SendCommand` → `GameRules:OnClientCmd`. Server to client: `server_slot:SendCommand` / `Server:BroadcastCommand` → `ClientStuff.ServerCommandTable`.
- **Snapshot limit:** CryNetwork collects a frame's unreliable data in a `CStream` with a fixed 1124-byte (8992-bit) buffer, and drops writes past it silently. A campaign map has ~650 net entities, so ~40% of every snapshot was lost. The own player sat at the end of the list and never reached the client.
- **Area triggers** (`AreaTrigger` and other shape areas) fire only for the machine's own player: `CPlayer::Update` runs `m_XAreaMgr.UpdatePlayer` only when `IsMyPlayer()`.
- **AI on a dedicated server** stays asleep until woken (`AIEVENT_WAKEUP`). It gets no animation events at all, because character animations never tick without rendering, so AI melee (`KEYFRAME_APPLY_MELEE`) and jumps never happen.
- **Net presence:** level entities spawn with `netPresence = false`. Only players, AI and vehicle-class entities are sent. Helicopters (`Gunship`) are not a vehicle class.
- **Player color:** the multiplayer team color is a second shader, `PlayerMaskModulate` (effect 2, turned on by the `"FX"` server command), and needs the `hero_mp.cgf` model; the single-player `hero.cgf` has no color mask.
- **Saves:** `CXGame::Save` and `LoadFromStream` return early in multiplayer, and saving needs a local client player. A checkpoint (`GameEvent`) calls `Game:OnAfterSave` once it has set `_LastCheckPPos`.

## What we changed

FarCryCoop never replaces game files. It has three parts:
- a Lua mod pak (game type `COOP`);
- an ASI plugin (`FarCryCoopNet.asi`, loaded by Ultimate ASI Loader) that hooks `CryGame.dll` 1.4 by address;
- a launcher.

Addresses were found with Ghidra (through REA) and checked against the MOD SDK 1.4 source. A `CryGame.dll` rebuilt from the SDK was tried and rejected: it needs VC 7.1 with the SDK's STLport, because STLport strings cross every engine interface.

| Bug | Root cause | Fix | How it was checked |
|---|---|---|---|
| Remote player frozen, no weapon, endless reload | snapshot tail past 8992 bits dropped | ASI: sort `m_lstNetEntities` by priority × age before `CXSnapshot::BuildAndSendSnapshot` (`0x3306faba`); hook `CXServerSlot::SendUnreliableMsg` (`0x3306c620`) and hold back updates past an 8800-bit budget, so they go first next time | client receives 100% of sent updates (ASI counters), own player 16-17 updates/s, positions match |
| Missions stall on "reach this place" | area triggers only for the own player | ASI: the `je` at `0x330ae17e` in `CPlayer::Update` jumps to a stub. For remote players on the server, it runs `CXAreaMgr::UpdatePlayer` (`0x330d3ee8`) | monitor log shows `AreaTrigger80 Empty->Occupied` when the client walks in |
| Enemies stand still on a dedicated server | puppets asleep | Lua: wake puppets within 150 m of any player once a second, `bSleepOnSpawn` ones within 40 m or when shot | AI behaviours and targets in the server log; enemies fight |
| AI melee never hits | no animation events on a dedicated server | Lua: on `OnCloseContact`, run the melee keyframe handler 0.35 s later; send the swing animation to clients | mutants hit and kill; the swing is visible on the client |
| Dedicated server crashes in cutscenes | `CMovieUser` and CryMovie sound keys call a NULL input / sound system | ASI: skip them when NULL; Lua: `mov_NoCutscenes 1` on a dedicated server | Training loads and plays |
| Remote hits do not register | not root-caused | Lua: client-side hits (`CHIT`), applied by the server with `target:Damage` | server-side check with a simulated hit; the author confirmed in game |
| Checkpoints never set on the server | `GameEvent:OnTimer` reads `_localplayer` (nil) | Lua: run it with the nearest player as `_localplayer` | monitor event; not seen on every map |
| Client keeps gliding after the server dropped it | paraglider has no client `OnUnBind` | Lua: server broadcasts `VEHOUT`, client releases the seat | checked in game |
| Dedicated server only 17 snapshots/s | `Sleep` at 15.6 ms timer resolution | ASI: `timeBeginPeriod(1)` on `FarCry_WinSV` | 35-36 snapshots/s |
| Pushed barrels never move on clients | no net presence | ASI: give net presence to `RigidBody` / `DestroyableObject` / `BasicEntity` once they move on the server | no client packet failures, respawn still works |
| No keys with the Russian layout | CryInput builds its key table from `GetKeyboardLayout(0)` | ASI: hook it before CryInput loads, return the US layout | bindings work with the Russian layout |
| No saves in a network game | `CXGame::Save` and `LoadFromStream` return early in multiplayer | Lua, host only: a file `Mods\FarCryCoop\save_<name>.lua` with the map, each player's position, health, armor, weapons and ammo, the ids of dead AI and the respawn point. `Game:OnAfterSave` autosaves at a checkpoint. A load reloads the map with `LoadLevelMPServer`, removes the dead AI and puts the players back | Pier, two games on one PC: save, move both players 60 m away, load: both are back on the saved spot; a checkpoint writes `save_auto.lua`; a game started from the launcher's `save:<name>` entry puts both players on the saved spot. F6 / F7 and the single-player "Load Game" menu not tested or not connected |
| Helicopters hang still on clients | level entities have no net presence, so a helicopter that flies on the host is not sent | ASI: a `Gunship` or `CargoChopper` gets net presence once it has moved on the server and then goes in every snapshot | Pier: the host moved two helicopters by 40 m, the client was on the same spot at the first check (gap 0 m, 28 s). A helicopter that has not moved hovers 8-34 m higher on the client. A real mission flight (the Boat boss) not seen |
| Treehouse and Swamp: the game dies at the level load (`ReadPacked i== 20`) | the level file bakes a physics state as text into some entities (Treehouse: entity 410, a `BasicEntity`); `SetStateFromSnapshotTxt` turns it into a stream and the rigid body's reader finds 20 parts where it allows 16. Ignoring the error only moves the crash | ASI: a hook on the rigid body's `SetStateFromSnapshot` (`CryPhysics.dll` RVA `0x9c9b0`) skips the call when `SetStateFromSnapshotTxt` makes it, in a game that ran a `CoopMap(`, `CoopLoad(` or `Game:Connect(` line | Treehouse, Swamp, Factory, Dam and Volcano loaded for both games and 3 checkpoint hops worked on each (the client was late at Swamp checkpoint 4 and Volcano checkpoint 3, [#3](https://github.com/Bantila/FarCryCoop/issues/3)). Not known: why the text does not fit, what the skipped states look like in the game, which other levels bake states |
| Every player white | single-player spawn has no color, model has no mask | Lua: server sends `PCOLOR`; clients load `hero_mp.cgf`, turn on `PlayerMaskModulate` and wrap `UpdatePlayerEffectParams` | untested in game |

Joining:
- **Steam bridge (not tested with a real friend).** It follows [playbook/networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md): a console exe running as Spacewar, using `SteamNetworkingMessages` and a UDP relay to `127.0.0.1:49011`. The ASI runs one console line from `Mods\FarCryCoop\join.txt` through `IConsole::ExecuteString`, which is vtable `+0x80`; `+0x84` is Exit. Checked on one PC: with `-loop` a second game joined through the tunnel and stood next to the host; with the real Steam client the bridge starts as Spacewar and sets the "connect" presence once the server is up. A friend's real Join (a second Steam account on another PC) is not tested.
- **Launcher.** Rust and Tauri. "Test on one PC" and starting from a save were run in game. The TCP lobby (untested in game) is meant to show names, shirt colors and loadouts before the start.

## QA

None automated. A Lua monitor (`coop_monitor 1`) logs players, nearby AI, vehicles and events once a second on server and client. A PowerShell diff of the two logs lists entities that are frozen, displaced or different on the client. It is a debugging aid, not an asserting test. All "works" above, except the items marked untested (Steam join, launcher lobby, shirt colors, F6 / F7), were checked with two game instances (one PC, then host and client), not with a test suite. The save check, the difficulty check, the helicopter check and the level sweep were driven by console lines sent to the running games, not by hand; the sweep is a script run by the author, not a test suite.

## How to play

See [INSTALL.md](https://github.com/Bantila/FarCryCoop/blob/main/games/far-cry/release/INSTALL.md) in the project. Each player needs Far Cry 1.4 and the same mod version:
1. Copy `Mods`, `Bin32` and the launcher into `<game dir>`.
2. The host creates a lobby and presses Start.
3. Friends join through the lobby (direct IP or VPN), or through "Join Game" in the Steam friends list.

Saves (host): a checkpoint saves by itself, F6 saves, F7 loads, and a save chosen in the launcher's map list continues the game after a restart.

Only direct IP / VPN with the console is checked by hand. The launcher lobby, shirt colors and Steam "Join Game" are **untested** (INSTALL.md lists them as things to test).

## Known issues

- Helicopters: a client follows one once it has moved on the host; before that it hovers up to 34 m higher. A real mission flight (the Boat boss) is not seen.
- **A level change sometimes crashes the host** (random heap corruption; seen on Bunker -> Steam and Cooler -> Boat, [#2](https://github.com/Bantila/FarCryCoop/issues/2)). Treehouse and Swamp load only because the ASI skips the physics state that their level files bake into some entities in a co-op game; those objects start where the level puts them, and what that looks like in the game is not seen ([#1](https://github.com/Bantila/FarCryCoop/issues/1), closed).
- Fixed after the sweep: a client that missed a move is moved again by the server, also when it falls through the ground ([#3](https://github.com/Bantila/FarCryCoop/issues/3)); the Bunker AI conversation error is guarded ([#4](https://github.com/Bantila/FarCryCoop/issues/4)). Open: vehicles with fewer seats than players, e.g. Swamp and buggies ([#5](https://github.com/Bantila/FarCryCoop/issues/5)).
- A joined player cannot push barrels or crates.
- Clients do not see cutscenes.
- A level has one paraglider: a copy now spawns for each next player; to others a climbing glider looks as if it falls.
- Saves keep the players and the dead enemies only. After a load the mission script starts from the beginning of the map; triggers, pickups and vehicles are not saved.
- On a dedicated server, AI never jumps; enemies can move in small jerks on clients.
- The short console commands (`coop_map pier`) do nothing in the game client; the `#CoopMap("pier")` form works.

## Next steps

- Find the random crash on a level change, then repeat the sweep over all 20 levels in one run.
- A scripted two-client test that asserts player, AI and mission state (the monitor diff and the sweep script are a start).
- A real helicopter flight and the paraglider as seen by clients.
- Save the mission progress (triggers, objectives, pickups), or real engine saves on a listen-server host: bypass the multiplayer checks in `CXGame::Save`.
- AI jumps on a dedicated server; it needs animation events without rendering.
- Joining without Steam or a VPN (join code, UDP hole punching, relay), as the Crysis co-op mod does.
