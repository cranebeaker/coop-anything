# The Outer Worlds 2 (2025)

| | |
|---|---|
| Engine | Unreal Engine 5.4.4 (Obsidian's `Arkansas` project, Microsoft GDK build) |
| Versions tested | Steam, app 1449110, build 24225441 (game version 1.2.0.1) |
| Projects | [tow2-coop](https://github.com/nicoNaeg/tow2-coop) ([v0.1.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0)), directory entry "tow2-coop" |
| QA level | automated (two copies on one PC; not yet across two PCs) |
| Last updated | 2026-11-16 |

## Where things stand

Two players can share one campaign with tow2-coop v0.1.0: the friend joins from the main menu by receiving the host's
save, both see each other, and NPC health and deaths, quest progress, map travel, doors and containers follow the
host. Everything was tested with two copies of the game on one PC (automated suite and a 30-minute soak); a session
across two PCs has not been played yet.

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

## QA

- Two isolated, hard-linked test copies on one PC (the hub's `tools/gamepool`), a bridge into each copy, scripted
  tests with real game clients: `qa/run_suite.ps1` (auto-join, NPC authority, melee with real input from both
  players, story, doors and containers, quest-outcome and map-travel resync, co-op resume after restarting both
  games), `qa/soak_run.ps1 -Minutes 30`, `qa/run_key.ps1`, `qa/test_install.ps1`.
- Last results (2026-11-16): suite all PASS (run 33, melee re-run in runs 37 and 39 on a hostile NPC); 30-minute soak
  PASS (run 38: 60 checks, no problems, stand-in to player median 0.2 units, NPC health difference 0, no crash);
  shared key: same key joins in about 15 s, different keys are ignored; installer 8/8 on a fake game folder.

## How to play

Download the [v0.1.0 release](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0). Each player needs their own
copy of the game. The host runs `install.ps1 -Role host` and gives the printed key to the friend, who runs
`install.ps1 -Role client -Peer <host address> -Key <key>`. The host's PC must accept UDP 27960 and TCP 27961 (port
forwarding or a VPN). The host loads their campaign; the friend waits at the main menu and is pulled in.

## Known issues

- Not yet played across two PCs.
- Keep the host's window un-minimized while the friend joins or follows a map change (a minimized copy can't finish a
  save).
- Enemies don't attack the friend's stand-in; each player is attacked in their own world.
- Armor changed after the join isn't shown to the other player. Two players only.
- Quest completions reach the friend through a short reload.

## Next steps

- A two-PC session, then a Steam transport (lobby invite and Valve's relay instead of IP and ports; see
  [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md)).
- Enemies that target the remote player, armor changes after the join, dialogue sync, items beyond containers.
