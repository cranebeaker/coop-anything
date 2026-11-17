# The Outer Worlds 2 (2025)

| | |
|---|---|
| Engine | Unreal Engine 5.4.4 (Obsidian's `Arkansas` project, Microsoft GDK build) |
| Versions tested | Steam, app 1449110, build 24225441 (game version 1.2.0.1) |
| Projects | [tow2-coop](https://github.com/nicoNaeg/tow2-coop) ([v0.1.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0), direct IP; test build [v0.2.0-beta.2](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.2.0-beta.2), Steam), directory entry "tow2-coop" |
| QA level | automated (two copies on one PC; not yet across two PCs) |
| Last updated | 2026-11-17 |

## Where things stand

Two players can share one campaign with tow2-coop: the friend joins from the main menu by receiving the host's save,
both see each other, and NPC health and deaths, quest progress, map travel, doors and containers follow the host.
v0.1.0 connects by IP; the v0.2.0-beta.2 test build connects Steam friends through the game's own Steam session (no IP
or port), keeps the friend's own saves untouched (saving is off in the joined game) and shows armor and helmet
changes. Everything was tested with two copies of the game on one PC (automated suite and a 30-minute soak); the Steam
link was checked on one copy only, and a session across two PCs has not been played yet.

## Existing work

None found besides tow2-coop (searches on 2026-11-15 and 2026-11-16: this directory, GitHub, Nexus and web search;
details in [NOTES.md](NOTES.md)). The closest same-engine project is Gothic 1 Remake Coop (UE 5.4), whose public
docs describe the same two-world design.

## How the game works (for co-op)

Single-player UE 5.4 shipping build, no anti-cheat, `.text` readable despite SteamStub, paks and IoStore not encrypted.
The game has no netcode to reuse, so each player runs their own world and a native mod syncs it:

- **Saves:** `SaveGameManager.SaveGame/LoadGame`; `AActor.bShouldSaveGameState` (an engine addition) decides whether a
  spawned actor goes into the save, and only counts before `FinishSpawning`.
- **Quests:** every quest is in the `QuestBundleSet` asset; scripts are GUID-based (`QuestScripts`,
  `GlobalVariableScripts`); `OwQuestManager.GetQuestState_BP` reads state. Completions can't be applied by script.
- **Travel:** `TravelManager.BeginTravelToBP(destination)`. **Doors:** `OCLComponent`. **Containers:**
  `Container.HandleItemRemoved`, `InventoryComponent.RemoveItemFromActor`.
- **Look:** `AppearanceComponent` (Customization struct, head, body) and `EquipmentComponent.SetArmorVisualOverride`.

Everything above is reached through UE reflection from a DLL loaded by a `dsound.dll` proxy; [NOTES.md](NOTES.md) has the
sources, layouts and pitfalls.

## What we changed

- **Native core** (`tow2coop.dll`): checks the exe build, walks UE reflection, hooks the engine tick and
  `ProcessEvent`; no UE4SS (it picked an empty `ProcessEvent` stub on this branch).
- **Two-world sync** over UDP: player transforms and animation, NPC health and deaths owned by the host, acknowledged
  damage from the client, story events on an ordered acknowledged channel, doors and containers.
- **Join and resync:** the host saves and hands its campaign over TCP; the client loads it into its own
  `CoopHost_<id>` slot. The same path carries quest completions and map changes (the client follows the host).
- **Stand-in:** an NPC body with its AI stopped, god mode, no interaction or targeting, the player's look, kept out of
  saves (a save made during co-op held an extra NPC until `bShouldSaveGameState` was cleared before spawning).
- **Player install:** `install.ps1` finds the game through Steam, writes `tow2coop.ini` (role, host address, port,
  optional shared key) and uninstalls cleanly; strangers with another key are ignored and the campaign goes only to
  the joined player's address.
- **Steam transport (v0.2):** `ISteamNetworkingMessages` through the game's own `steam_api64.dll` (SDK 1.57, flat API):
  game packets unreliable on channel 0, the campaign hand-over reliable on channel 1; the players find each other with a
  rich-presence key (`tow2coop` = `host` / `join:<host id>`); the host accepts friends only.
- **Joined player's saves (v0.2):** the joined game's autosave and quicksave slots are the same files as in its owner's
  own campaign, so the client calls `SaveGameManager.DisableSaves` once settled in each world (lifted for a hand-over
  load); a hand-over never sends an old campaign (a refused host save cancels it and the client asks again).
- **Outfit (v0.2):** the armor set and helmet each player wears go to the other every 2 s and dress the stand-in; the
  stand-in body's own cap is hidden when the player wears no helmet.

## QA

- Two isolated, hard-linked test copies on one PC (the hub's `tools/gamepool`), a bridge into each copy, scripted
  tests with real game clients: `qa/run_suite.ps1` (auto-join, NPC authority, melee with real input from both
  players, story, doors and containers, quest-outcome and map-travel resync, co-op resume after restarting both
  games), `qa/soak_run.ps1 -Minutes 30`, `qa/run_key.ps1`, `qa/test_install.ps1`.
- Last results (2026-11-16): suite all PASS (run 33, melee re-run in runs 37 and 39 on a hostile NPC); 30-minute soak
  PASS (run 38: 60 checks, no problems, stand-in to player median 0.2 units, NPC health difference 0, no crash);
  shared key: same key joins in about 15 s, different keys are ignored; installer 8/8 on a fake game folder.
- v0.2.0-beta.2 (2026-11-17, runs 50-53): the suite gained an outfit check, a client-saves check and a check that the
  core's fault guard caught nothing during the run. Run 50: 15/15 PASS. Run 52: 14/16, both FAILs were test faults
  (fixed in the tests). Run 53 on the final build: the first 10 steps PASS, then the agent environment stopped the run
  for low memory during map travel; the 6 later steps had passed in run 52, whose build differs only by the helmet
  visibility. Installer 9/9 on a fake game folder. Steam link: relay up and a message to the own SteamID came back,
  and a campaign sent to the own SteamID arrived byte-identical and loaded (one copy).

## How to play

Each player needs their own copy of the game (a Steam Families copy can't be played by two people at once).

- **Over Steam** ([v0.2.0-beta.2](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.2.0-beta.2), test build, the players are Steam friends): the
  host runs `install.ps1 -Role host`, the friend `install.ps1 -Role client [-Friend "<host's Steam name>"]`. No address
  or port.
- **By IP** ([v0.1.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0), or beta.2 with `-Transport udp`): the host runs
  `install.ps1 -Role host` and gives the printed key to the friend, who runs
  `install.ps1 -Role client -Peer <host address> -Key <key>`. The host's PC must accept UDP 27960 and TCP 27961 (port
  forwarding or a VPN).

The host loads their campaign; the friend waits at the main menu and is pulled in about a minute later.

## Known issues

- Not yet played across two PCs.
- Keep the host's window un-minimized while the friend joins or follows a map change (a minimized copy can't finish a
  save).
- Enemies don't attack the friend's stand-in; each player is attacked in their own world.
- Two players only. v0.2.0-beta.2 and v0.1.0 / beta.1 don't connect to each other (both players need the same version).
- Quest completions reach the friend through a short reload.

## Next steps

- A two-PC session over Steam (two accounts), which one PC can't test.
- Enemies that target the remote player, dialogue sync, items beyond containers.
