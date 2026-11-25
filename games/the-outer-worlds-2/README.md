# The Outer Worlds 2 (2025)

| | |
|---|---|
| Engine | Unreal Engine 5.4.4 (Obsidian's `Arkansas` project, Microsoft GDK build) |
| Versions tested | Steam, app 1449110, build 24225441 (game version 1.2.0.1) |
| Projects | [tow2-coop](https://github.com/nicoNaeg/tow2-coop) ([v0.1.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0), direct IP; test build [v0.6.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.6.0), Steam or direct IP), directory entry "tow2-coop" |
| QA level | automated (two copies on one PC) and one manual session across two PCs (beta.4) |
| Last updated | 2026-11-19 |

## Where things stand

Two players share one campaign with tow2-coop, each with their own character. The friend joins from the main menu by
receiving the host's save, and the friend's mod puts the friend's own character into it (items, money, level, skills,
look). Each player sees the other as a stand-in NPC wearing the other's look, armor and helmet. Fights are shared: an
enemy that spots either player starts the fight in both worlds and fights the same player (or companion) in both, so
the enemies spread over the two players. NPC health and deaths, doors, containers and dead-enemy loot follow; quest
progress and outcomes go both ways without a reload; map changes, fast travel and the host's reloads happen together;
XP from kills, lock picks and conversations is shared; the players give each other items through the other's stand-in.
The stand-in holds the other player's weapon with its mods and uses it (shots, swings, a gun's bash, reloads,
holstering) and crouches with them; enemies the game spawns during play are shared. The game's menus don't pause it in
co-op; the player who starts a conversation leads it and the other player's game plays the same conversation (the
same lines, camera and voices, the same choices shown; only the leader chooses, and the pick is made in both games);
two players can start a new game together from its very beginning; a quest one completes is completed in the other
game the game's own way; out of a fight the host's companions are in the same places in both worlds.

v0.5.0 (test build, 2026-11-20, installed with `tow2coop-setup.exe`) connects Steam friends, or anyone in a public game,
through the game's own Steam session (no IP or port), hosted and joined from the game's main menu or Steam's "Join game",
or two players by direct IP. All of it was tested with two copies of the game on one PC. The only session across two PCs
so far (2026-11-17) ran beta.4: the friend joined over Steam, both played for 15 minutes and the friend followed four
map changes. What the players reported then is fixed in beta.5 (stand-ins without walk animation or hair, dead enemies'
loot, the wait after a map change) and beta.6 (the friend's character becoming a copy of the host's, enemies standing
still in the other world).

## Existing work

None found besides tow2-coop (searches on 2026-11-15 and 2026-11-16: this directory, GitHub, Nexus and web search;
details in [NOTES.md](NOTES.md)). The closest same-engine project is Gothic 1 Remake Coop (UE 5.4), whose public
docs describe the same two-world design.

## How the game works (for co-op)

Single-player UE 5.4 shipping build, no anti-cheat, `.text` readable despite SteamStub, paks and IoStore not encrypted.
The game has no netcode to reuse, so each player runs their own world and a native mod syncs it:

- **Saves:** `SaveGameManager.SaveGame/LoadGame`; `AActor.bShouldSaveGameState` (an engine addition) decides whether a
  spawned actor goes into the save, and only counts before `FinishSpawning`. Quickload (F9) goes through its own
  native function, not `LoadGame`.
- **Characters:** a save folder's `Player.dat` holds the player's character (items with their state, equipment, money,
  XP, level, skills, perks); putting the friend's own `Player.dat` into the host's campaign gives the friend their own
  character in the host's world.
- **Quests:** every quest is in the `QuestBundleSet` asset; scripts are GUID-based (`QuestScripts`,
  `GlobalVariableScripts`); `OwQuestManager.GetQuestState_BP` reads state. Completions can't be applied by script; the
  quest's own state can be set through the game's native quest lookup.
- **XP:** every gain goes through the native `UPlayerInfoComponent::AddXP(XP, reason, ...)` (found from the
  `AddXPDebug` exec thunk, which inlines the same code); the reason tells kills, lock picks, hacks, conversations,
  quests and discoveries apart.
- **Travel:** `TravelManager.BeginTravelToBP(destination)`. **Doors:** `OCLComponent`. **Containers:**
  `Container.HandleItemRemoved`, `InventoryComponent.RemoveItemFromActor`. **Clock:** `GameCalendar.K2_AdvanceTime`.
- **Giving items:** `UIndianaUI::OpenItemTransfer` opens the game's transfer screen on a container (on an NPC it
  crashed, reading the NPC's clothes).
- **Look:** `AppearanceComponent` (Customization struct, head, body) and `EquipmentComponent.SetArmorVisualOverride`.
- **AI:** `AIDetectionComponent.ForceFullDetection` makes an NPC see an actor.
  `AITargetInfoPriorityFunctionLibrary.ForceOverridePrimaryTarget` returns a priority node handle that only takes
  effect once `PriorityNodeFunctionLibrary.Activate` is called on it, and holds a reference until `Deactivate`: keep
  exactly one copy of it. The AI of an NPC far from the local player sleeps (no detection at 200 m). The game's
  `Spell_MakeNpcUntargetable` (cast and removed with `IndianaCharacter.CastSpell` / `RemoveSpell`) hides an NPC from
  the AI.

Everything above is reached through UE reflection from a DLL loaded by a `dsound.dll` proxy; [NOTES.md](NOTES.md) has the
sources, layouts and pitfalls.

## What we changed

- **Native core** (`tow2coop.dll`): checks the exe build, walks UE reflection, hooks the engine tick and
  `ProcessEvent`; no UE4SS (it picked an empty `ProcessEvent` stub on this branch).
- **Two-world sync** over UDP: player transforms and animation, NPC health and deaths owned by the host, acknowledged
  damage from the client, story events on an ordered acknowledged channel, doors and containers.
- **Join and resync:** the host saves and hands its campaign over TCP; the client loads it into its own
  `CoopHost_<id>` slot. Since beta.6 the same path is only needed to join, after a map change the friend could not
  follow, and for the host's reloads.
- **Stand-in:** an NPC body with its AI stopped, god mode, the player's look, kept out of saves (a save
  made during co-op held an extra NPC until `bShouldSaveGameState` was cleared before spawning). Out of a fight it is
  untargetable; while the party fights it joins the player's team so enemies can pick it.
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
- **Own characters (beta.6):** the friend's game keeps its own character in a private save before each hand-over and
  every 3 minutes, and puts its `Player.dat` into every campaign the host sends. First join: the character of the
  friend's newest own save, with empty pockets.
- **Party XP (beta.6):** a hook on `AddXP` shares XP by reason: kills, lock picks, hacks and conversations for both,
  quest XP from the host's campaign, discoveries only for the finder.
- **Giving items (beta.6):** E on the other player's stand-in opens the game's transfer screen on a hidden empty chest;
  items put in it go to the other player (acknowledged, back after 20 s otherwise), weapons and armor with their mods.
- **Together (beta.6):** either player's travel starts the same travel for the other at once (fast travel too); the
  host's `LoadGame` and Quickload send that save to the friend before loading it; the friend's death fetches the
  host's current game; quest outcomes go both ways live; one clock.
- **Co-op combat (beta.6):** one world decides each enemy's fight: the host's, or the friend's for enemies near the
  friend when the host is more than 50 m away (the AI sleeps far from a world's player). The other world forces its copy
  onto the same target (`ForceFullDetection`, then `ForceOverridePrimaryTarget` and `Activate`), so a detection in
  either world starts the fight in both and the game's own target choices spread the enemies over both players.
  Drift between the two copies is eased back (snapped past 6 m). A dead player's stand-in leaves the other world until
  that player lives again.

## QA

- Two isolated, hard-linked test copies on one PC (the hub's `tools/gamepool`), a bridge into each copy, scripted
  tests with real game clients: `qa/run_suite.ps1` and the shorter `qa/run_quick.ps1` (auto-join, NPC authority, melee
  with real input from both players, own characters, party XP, co-op combat, death and reload, world and clock, story,
  doors and containers, quest-outcome and map-travel resync, co-op resume after restarting both games),
  `qa/soak_run.ps1 -Minutes 30`, `qa/run_key.ps1`, `qa/test_install.ps1`.
- Last results (2026-11-16): suite all PASS (run 33, melee re-run in runs 37 and 39 on a hostile NPC); 30-minute soak
  PASS (run 38: 60 checks, no problems, stand-in to player median 0.2 units, NPC health difference 0, no crash);
  shared key: same key joins in about 15 s, different keys are ignored; installer 8/8 on a fake game folder.
- v0.2.0-beta.2 (2026-11-17, runs 50-53): the suite gained an outfit check, a client-saves check and a check that the
  core's fault guard caught nothing during the run. Run 50: 15/15 PASS. Run 52: 14/16, both FAILs were test faults
  (fixed in the tests). Run 53 on the final build: the first 10 steps PASS, then the agent environment stopped the run
  for low memory during map travel; the 6 later steps had passed in run 52, whose build differs only by the helmet
  visibility. Installer 9/9 on a fake game folder. Steam link: relay up and a message to the own SteamID came back,
  and a campaign sent to the own SteamID arrived byte-identical and loaded (one copy).
- v0.2.0-beta.3 (2026-11-17): the first two-PC test on beta.2 failed. Both logs said "Steam is not initialised in this
  game" 15-22 s after the mod's start, and beta.2 never checked again. beta.3 retries every 2 s. Test 54
  (`qa/test_steam_late.py`, one copy with Steam held back 40 s): the mod waited, linked up 41 s after it started, and
  logged no caught faults. Installer 10/10 on a fake game folder (new check: installing over an older build
  replaces it, so players update without uninstalling).
- beta.4 (2026-11-17, not published; part of beta.5): in the second two-PC test (beta.3), Steam was up on both PCs and the friend found the
  host. The host never accepted the session: the friend timed out every 10 s and received nothing. beta.4's host
  accepts a pending session from any Steam friend and logs what it sees. Test 55 (one copy on real Steam): the accept
  pass ran over the account's 10 friends, with no caught faults. The third two-PC test confirmed it (the host accepted
  the friend; the session-request callback never came).
- v0.2.0-beta.5 (2026-11-17, runs 57-58, two copies on one PC): join, the stand-in's look and hair, its walk
  (animation speed 400 while the client walks, 0 when it stops) and jump, doors, chests, dead-enemy loot, map travel
  (the host hands over 8 s after its map changed, before ~15 s), no caught faults, owner's files unchanged.
- v0.2.0-beta.6 (2026-11-18/19, runs 62-111, two copies on one PC): the friend joined with 0 bits and none of the
  host's items, gained money, ammo and two levels and kept them after the host reloaded; party XP by reason, both ways
  (8 cases); giving items through the stand-in both ways; travel together (the friend arrived 0.0-1.1 s after the
  host, no save sent); the host's reload reached the friend 2.7 s after it; quest outcomes both ways, live; one clock.
  Co-op combat (`qa/test_combat.py`, run 109: 27/27 PASS): the friend's copies of the enemies fought whom the host's
  game said in 73 of 73 samples, both players took hits in their own world, the two copies of an enemy were 1.4 m
  apart (median), the friend's game took over the enemies near it within 0.6 s of the host walking 150 m away, and a
  dead player's enemies turned to the other. Run 111: the quick suite 14/14 PASS, owner's files unchanged.
- v0.3.0 (2026-11-19, runs 112-134, two copies on one PC): host and join through the main menu's co-op choice (keys
  and clicks; the friend loaded the host's game 7-8 s after joining). The stand-in held the other player's weapon,
  fired as many rounds as they did (its last shot 0.3-2.1 m from theirs; 4.3 m once, run 133), its shots at the friend
  were dropped, it swung,
  holstered, drew and reloaded with them, showed a silencer put on the host's rifle, played the NPC bash for the
  rifle's quick melee, and crouched and stood up with the host (its feet within 1 cm of the host's). Spawned Ursopods
  got a copy in the other game, or were paired when both games spawned one, and died in both. The friend's
  companions' hits are no longer reported as the friend's (106 reports in 20 s before, 0 after). Run 134: the quick
  suite through the menu, 16/16 steps PASS, owner's files unchanged.
- v0.6.0 (2026-11-21, runs 189-201, probes 128-145, two copies on one PC): a quest item either player got was given to the other (host to client, client to host; one both got at once not given twice) and after the join the client held the host's 38 (its game later dropped 2 leftovers of later quests by itself while catching up on the story); a level-placed loose item picked up with E was gone in the other world, both ways, and after the host's quicksave and quickload neither came back (the save itself kept them gone); a switch used in one world took the same state in the other, both ways, with nothing going back and forth; the quick suite passed in parts (run 200 up to spawned enemies; runs 191-192 and 197-198 the rest), the two copies growing to 9-11 GB each, too much for one run on the test PC. Lockpicking and hacking are on doors and containers, already shared; a hand-over's time is mostly the game's own (its save refused during a loading screen, its loads), the transfer about 1-2 s.
- v0.5.3 (2026-11-20, runs 175-177, probes 121-127, two copies on one PC): in a shared conversation a real skill check (the Order's light worker, the host's skills 20, the client's 0) was taken in the follower's game past its lock, with no restart and the same next line (0.5.2 restarted it); a door's wave of enemies started in both games and its enemy came out of the door in both (12/12 with the hives); a squad of armed Protectorate soldiers fought both players in both games, up to 8 at once, each soldier's two copies within 4 m, the client's hits counted, deaths shared (9/9); ship travel (boarding, flying to another planet's landing) was followed both legs, the ship at the same place in both; a new game together left the intro for the first planet, the client with its own character; a quest set without its banner showed "Quest Completed" and its title; 10 travels and 5 starts without a crash. A whole suite in one go was cut by low memory on the test PC; its halves passed.
- v0.5.2 (2026-11-20, runs 171-174, probes 114-120, two copies on one PC): a hive or floor-hatch wave started in either game (the game's `Encounter.ActivateWave`) started in the other, its enemies came out of their spawners in both and a death in the host's game was one in the other (10/10 both ways; before, the other game's enemies stayed hidden for about a minute); the co-op menu worked with a controller (Y, the D-pad, A, B) and X opened the giving screen, with buttons the bots set through the mod; a quest set without its banner showed its title in the game's notification; all 21 game steps of the quick suite passed in one run (run 173), and in run 174 all but one sword-swing check, which passed 3/3 after a test fix; a fault in the mod's object scan (an object being made or freed) was fixed.
- v0.5.0 (2026-11-20, runs 144-163, two copies on one PC; final build: runs 159-163 and the new game test): a conversation led by one player was played in the other game at the same node, with the same response picked there and the same randomly drawn line, the other's own choices dropped, both ended together (18/18 both ways); two copies went through the New Game and the character creator, the friend's new character was kept and joined the host's intro (16/16); a quest the client completed was completed on the host by the game's own quest steps (2 steps, its XP 16085 -> 21260 for both); the rest of the quick suite passed (one game crash once on arriving after a map travel, not in the rerun).
- v0.4.0 (2026-11-19, runs 135-143, two copies on one PC; final build: runs 142-143, 22 of 23 steps): the game's menus left the world running in both copies
  (world time +2.05 s in 2 s with the pause menu or the inventory open); a conversation's screen reached the other
  player both ways, with the same responses, and a response the other player chose was taken; the host's companions were
  0.0 m from their places in the host's world; a quest the client completed first gave its XP to both (16085 -> 21260);
  with 60 % of the packets lost, 4 of 4 door changes arrived (12 resends); the friend's character was kept by itself
  after a change; a minimized host finished its hand-over save; on one Steam account a public game's lobby was made and
  left; the installer passed 15 checks on a fake game folder.

## How to play

Each player needs their own copy of the game (a Steam Families copy can't be played by two people at once).

- **Over Steam** ([v0.6.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.6.0), test build): both run
  `tow2coop-setup.exe` (*Install*). In the game's main menu the Moon Man's choices have one more, *Let's play together
  (co-op)*: the host picks *I'll host* (Steam friends), *I'll host a new game* (both start from the very beginning: the
  friend makes their character in the game's character creator first) or *I'll host: anyone can join (public game)*, the other player
  *I'll join a friend's game* and the host in the list (friends who host, public games), or *Join game* on the host in
  the Steam friends list. No address or port.
- **By IP** ([v0.1.0](https://github.com/nicoNaeg/tow2-coop/releases/tag/v0.1.0), or 0.5.0 with *Direct IP* in the installer): the host runs
  `install.ps1 -Role host` and gives the printed key to the friend, who runs
  `install.ps1 -Role client -Peer <host address> -Key <key>`. The host's PC must accept UDP 27960 and TCP 27961 (port
  forwarding or a VPN).

The host loads or continues their campaign; the friend's game joins it and loads it (by IP, the friend waits at the
main menu and is pulled in about a minute after the game started).

## Known issues

- Everything since beta.5 was tested on one PC only; a session across two PCs with 0.6.0 is still to come. Public
  games, "Join game" and the list of friends who host need two Steam accounts to test (one account: a public game's
  lobby made, searched for, left).
- The co-op choice is in the main menu only, by mouse, number key or controller (Y, the D-pad, A, B; tested with
  buttons the bots set through the mod, not a real controller).
- In a conversation, a choice the other player's character can't take (a skill check, a flaw) shows as unavailable on
  their side and is taken all the same when the leader picks it, without a restart (tested with a real skill check).
  The follower's game counts the leader's choices as the follower's (it can offer a flaw for them, e.g. Compulsive
  Liar). When two responses lead to the same line, the follower's game may take the other one. Both games run the conversation's effects (an item an NPC gives goes to both players); payments are the
  leader's only.
- Spawned enemies were tested with Ursopods the test spawned the way a spawner does, and the game's own hives and
  floor hatches and a door with waves the test started the game's way; a wave the story starts was not met. Two of the same kind spawned more than 15 m apart in the two games give one more enemy. Co-op
  combat was tested against creatures and a squad of up to 9 armed soldiers.
- While the host's game is paused in a fight (a menu, or the game's own flaw offer), the host's enemies wait.
- The friend's character is kept by a private save every 3 minutes and before a hand-over: after the host's reload or
  the friend's death, up to 3 minutes of the friend's gains are lost.
- Keep the host's window un-minimized while the friend joins or a hand-over runs (a minimized copy can't finish a
  save).
- Giving items: E or a controller's X in front of the other player opens the screen. Quest items go to both players by
  themselves; loose items one player picks up are gone for the other; switches and radios are shared. Items dropped
  during play, NPCs out of a fight and physics objects are each game's own.
- Two players only. Use 0.6.0 on both PCs (protocol 7: 0.5.x does not connect to it).

## Next steps

- A two-PC session with 0.6.0 over Steam (two accounts): a public game, "Join game", the friends list, a new game
  together played past the intro.
- A real controller in the co-op menu and in a shared conversation; a wave the story starts.
- Three game crashes seen once each (runs 161, 164, 178; the last in the game's world tick after the test teleported
  both players ~250 m and back, the mod not in its call chain): the test copies log where an access violation happens
  and keep the crash dumps.
