# Autonauts (2019)

| | |
|---|---|
| Engine | Unity 2018.4.35f1, Mono (BepInEx 5 + HarmonyX plugin) |
| Versions tested | Steam (app 979120), Windows build, install files dated 2024-10-09 (game version 140.2); automated tests on Windows (test kit) and under Wine 9 |
| Projects | AutonautsCoop ([Shocklinks/autonauts-coop](https://github.com/Shocklinks/autonauts-coop)), directory entry "AutonautsCoop" |
| QA level | automated |
| Last updated | 2026-10-26 |

## Where things stand

AutonautsCoop 0.13.0 is the first beta. Two or more players share one Autonauts world: one host, joiners over Steam invites or IP, started from the game's own menus. Joiners walk, carry, build, teach and program bots, use building menus, the bot server, groups, catapults, the spaceport, planning mode and the Creative spawn menu, and the whole world matches on every side (whole-world checksum). Automated tests with real game clients pass with 2 and 4 players, on a new world and on a late-game save (420x240 map, ~10,800 objects, 53 bots). One real Steam session between two PCs over the internet worked (author's test, 2026-10-26). Not yet done: wider real-world testing (more players, more saves, worse connections).

## Existing work

- No official multiplayer. Developer comment (2019/20): multiplayer "isn't something that will happen for a while" ([playco-opgame.com](https://playco-opgame.com/autonauts/)).
- No co-op mod found before this one (GitHub, Nexus, Steam Workshop and forums via web search, 2026-10-20). Only player requests: [itch.io 1](https://itch.io/t/175669/pls-add-a-multiplayer-mode), [itch.io 2](https://itch.io/t/161660/multiplayer), [Steam](https://steamcommunity.com/app/979120/discussions/0/1639801448924410169).
- The official Lua (MoonSharp) mod support is sandboxed and can't intercept game methods, so it can't carry networking.

## How the game works (for co-op)

- **No netcode.** Fully single-player, 195 `UnityEngine.Random` calls and a variable timestep, so lockstep isn't realistic. The mod is host-authoritative: the joiner's simulation is frozen and it mirrors the host.
- **Saves are one JSON document.** `SaveJSON.Capture` builds the whole world in memory; `LoadJSON` reads it from `MyFile.Load`. Joining = capture on the host, gzip, send, and let the joiner's own loader read it from memory (patch `MyFile.Exists`/`Load` for a reserved save name).
- **Objects:** every object has a UID from `ObjectTypeList`; `Savable.Save/Load` give a complete per-object JSON. That JSON is the delta format. Children (carried items, bees nests, a blueprint's building) are saved inside their parent.
- **Input choke points:** `FarmerPlayer.GoToAndAction` (clicks), `BuildingManager.AddBuilding/DestroyBuilding/MoveBuildings` and `GameStateEdit.ConvertDragModels` (edit mode), `Worker.NewHighScriptTaught` (teaching), `InventoryBar.DragEnd/OnClick` (inventory screen), `GameStateManager.Start*` (building screens).
- **Manager data outside objects:** quests/research, unlock tables (`m_ObjectsLocked` and `m_BuildingsLocked`, partly per player via badges), resources, time of day, script library, bot groups, offworld missions, the global wardrobe store and planning areas. Each is synced as a "world section".
- Detailed per-version pitfalls (UID registry traps, pooled objects, screens that lose their building when it is rebuilt, and so on) are in [NOTES.md](NOTES.md).

## What we changed

The mod lives in its own repo; this is the design and the hard parts.

- **Join:** snapshot through the game's own loader; the joiner's own UIDs move to a high range so they never collide with the host's.
- **Deltas:** the host sends changed objects' save JSON, removed UIDs, movement at 10 Hz, tile and fog changes, animations and world sections. Every 10 s it sends a (uid, hash) digest and joiners re-request anything that drifted.
- **Applying on the joiner:** in-place `Savable.Load` (unregister the UID first), verified by re-saving; otherwise recreate under `ObjectTypeList.m_Loading = true` so UIDs survive.
- **Joiner input** goes to the host as small messages and is replayed through the same game calls; results come back through sync. This covers clicks, edit mode with undo, teaching, inventory moves, menu actions, bot server, groups, catapult aim and spaceport missions.
- **Shared editors** (script library, planning areas, bot groups) send their whole section after a local edit. The host replaces its copy and broadcasts it. Planning and groups lock to one editor at a time (whoever changes first, until 5 s after their last change); the script library is merged per script (3-way, against the copy the pusher edited from).
- **Leaving:** a joiner's character (position, hands, pockets, upgrades, clothes) is taken out of the world and stored in the host's save. It comes back on rejoin: in place, or next to the host if that spot was built on.
- **Big bases:** change detection is incremental. Movers are re-checked every scan, recently changed or moved things too, and the rest in a ~5 s round. Path rotation is left out of hashes because each machine derives it from neighbours.
- **Steam:** ISteamNetworkingMessages and friends-only lobbies through the game's bundled Steamworks.NET. The bundled `SteamNetworkingMessage_t.Release()` has a bug, so the mod calls the native release itself.
- **Version check** (mod and game) with a readable refusal, "Host has ended the lobby." when the host stops, join/leave notices, and a one-click bug report zip that includes the host's log.
- **In-game UI:** host, invite and stop from the pause screen, join from a main-menu panel, notices in the game's own popup, built from the game's prefabs. Creative-mode spawns from a joiner are replayed on the host.

## QA

- `test/run.sh "<game dir>" "<BepInEx dir>" <work dir>` in the mod repo: two game copies under Wine + Xvfb with separate prefixes, driven by in-game bots through the real input path (`COOP_TEST=host|client`).
  - `COOP_TRANSPORT=udp` runs the same tests over a UDP stand-in for the Steam message framing.
  - `COOP_BIG=1` runs the big-base stress test.
- The suite asserts:
  - join: object counts and the whole-world checksum
  - edit mode: building, drag, move, delete, undo
  - teaching, edit locks, rename, inventory transfers in both directions
  - menus: recipes, research, wardrobe, bot server, groups, catapult, spaceport, planning
  - game speed and pause, world sections
  - leave/rejoin (spot free and spot built on), host save/reload with stored characters
  - version refusal, "lobby ended", joiner autosave blocked, joiner Save As loading as their own character, bug report
- Windows test kit (two or four game copies on one PC, driven by the same bots), last results 2026-10-26:
  **116/116** full test (new world, 0.13.0), **18/18** on a late-game save (0.12.37) (420x240, 10,803 objects, 53 bots; 1 joiner rebuild in
  4.5 min), **46/46** four players (host + 3 game joiners: join together, see and walk, blueprints, world checks, a
  drop and rejoin) on a new world (0.12.37), **18/18** in-game menu test (0.13.0) (pause/main-menu buttons, join panel, notices, Creative
  spawn, Load blocked for joiners; screenshots checked). Four players on the late-game save (0.12.37): 46/47 (the rejoin
  duplicate under Known issues).
- Earlier Linux/Wine results (2026-10-21, mod 0.10.0): **101/101 over TCP, 84/84 over the UDP stand-in, 7/7 big-base test**. Big base: ~8,000 objects with 400 bots and 4,000 items; joined in ~70 s; worlds matched after 90 s of churn.

## How to play

1. Download the latest zip from the mod's [Releases page](https://github.com/Shocklinks/autonauts-coop/releases) and extract it
   into the Autonauts game folder, next to `Autonauts.exe` (it includes BepInEx 5.4.23.5).
2. Host: load a world, press Esc, then Host Multiplayer (right side of the pause screen); the Steam invite window opens.
   Esc > Invite Friends reopens it, Esc > Stop Hosting ends the session.
3. Joiners: accept the invite or right-click the host in Steam > Join Game. Without Steam: main menu > Multiplayer, type
   the host's IP (host forwards TCP 27015).

Every player needs their own copy of the game and the same mod version (a mismatch is refused with a message).
F8 opens the mod's own window: port, name, status and a bug report zip.

## Known issues

- Beta: one two-PC Steam session so far. Larger groups, other saves and poor connections need real-world reports.
- Joiners can't load a save while connected (it crashes the game): the Load button is hidden for them.
- Things that don't move (e.g. storage contents) can take a few seconds to update for joiners.
- Planning areas and bot groups are edited by one player at a time (a change from someone else is undone with a message).
  The script library merges per script.
- An unlock that arrives while a joiner has the build menu open shows after they reopen edit mode.
- A joiner hitches ~200 ms about every 100 s on a large save (under investigation).
- Right after a rejoin on a busy base, an item put into a machine at that moment can be duplicated for ~3 s, until the
  joiner's self-check removes it.
- Badges and the Autopedia are per player.

## Next steps

- Collect beta reports (F8 > Save bug report from both players).
- Fix the joiner's periodic hitch and the rejoin duplicate above.
- Real sessions with 3+ players and medium/large maps over the internet.
