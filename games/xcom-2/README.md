# XCOM 2: War of the Chosen (2017)

| | |
|---|---|
| Engine | Unreal Engine 3 (UnrealScript, official mod SDK) |
| Versions tested | Steam, XCOM 2 buildid 8270065 (changelist 699444), War of the Chosen + all DLC, Windows x64 |
| Projects | Team Dragonpunk "Tactical Co-Op" (2016-17, abandoned); WOTC Co-op Campaign (prototype, this write-up): https://github.com/EvilMask587/xcom2-wotc-coop-campaign, extended to missions and up to 4 players (2026-11, not published yet) |
| QA level | automated (two to four game copies on one PC, direct IP; see QA) |
| Last updated | 2026-11-13 |

## Where things stand

No one else can play it yet, but a full campaign loop runs in automated tests. Up to four
players share one Avenger: the host makes the campaign decisions, and each guest manages the
soldiers the host gives them. In missions each player moves their own soldiers in turn (host,
then each guest, then the aliens) while the others watch. Saving and loading mid-mission,
Ironman, disconnects and rejoins all work. Everything was tested only on one PC with direct IP.
The mission code is not in a public repo yet, and two PCs, Steam invites and Steam mod
subscription are untested.

## Existing work

- **Team Dragonpunk, "XCOM 2: Tactical Co-Op"** (Steam Workshop 792693587, now removed):
  created 3 Nov 2016, last updated 3 Sep 2017, status "Final". Two players split the squad
  on campaign missions; soldiers can be imported from the friend's game; no Ironman. Only
  the tactical layer, no shared strategy. Probably pre-WotC (last update 5 days after WotC).
  Source: https://github.com/quixotic-cloud/Tactical-Co-Op (GPL-3.0 per the repo, last push
  2017-03-30, vanilla XCOM 2; its `Src/XComGame` folder is the SDK source copied by the old
  ModBuddy template, not the mod's own code), and a Long War 2 variant
  (https://github.com/quixotic-cloud/Long-War-2-Coop, one commit, no license). It uses the
  game's `XComGameStateNetworkManager` over Steam lobbies.
- **robojumper, "WOTC Multiplayer mods"** (Workshop 1571537500): lets mods run in the
  built-in PvP multiplayer; soft-deprecated in favour of the Community Highlander. Not co-op.
- Built-in multiplayer: tactical PvP only.

## How the game works (for co-op)

- Everything that matters is a **game state history** (`XComGameStateHistory`): a list of
  frames, each a set of changed state objects with stable ObjectIDs. Strategy and tactical
  both use it. The mod SDK ships the full UnrealScript source (`Development/SrcOrig`).
- `XComGameStateNetworkManager` (native, script-callable) already moves histories:
  `SendHistory`, `SendPartialHistory`, `SendMergeGameState`, `SendRemoteCommand` with
  typed parameters, receive delegates. Built-in PvP uses "the active player is the
  authority": it runs the ruleset and sends a partial history at the end of its turn.
- Transport: `IpNetDriverSteamworks`, plain IP or Steam sockets. In IP mode the client
  host must be ONE decimal integer (127.0.0.1 -> "2130706433"); a dotted host sends
  nothing. Client and server on one PC need different ports.
- Delegates on the network manager must point at plain Objects: it outlives maps, and a
  delegate bound to an Actor keeps the old World alive (assert on the next map load).
- Every frame submitted on one side is sent to the partner with `SendMergeGameState`.
  A merge state is applied into the receiver's current frame (it does not add a frame), so
  frame counts differ between players; compare content, not indices. Frames without
  objects are refused by SendMergeGameState. Remote commands sent after a merge state are
  handled before it; several frames from one tick can arrive as one merge.
- Merge states carry new and removed objects with their ObjectIDs, and the receiver's ID
  counter moves past them. If BOTH sides create objects, they hand out the same ObjectID
  and the merge silently replaces one object with the other (tested), hence one editor at a
  time.
- **Missions.** WOTC orders turns by initiative groups (`XComGameState_AIGroup.InitiativePriority`),
  and the PvP ruleset (`X2TacticalMPGameRuleset`) already hands control between groups with a
  partial history and a `SwitchTurn` command while the other machine watches a replay. One XCom
  group per player reuses that. Guests connect only to the host, so the host relays a guest's
  frames to the other guests.
- With a connection open, the game treats a campaign mission as multiplayer in several places:
  single-player rules (flank crits, suppression, Shadowbind, pausing) have to be put back, the
  mission load movie never ends by itself, and a dropped connection runs the PvP "opponent left"
  handling, whose asynchronous online-service ping crashes the guest if it leaves the map first.
- Running two instances on one PC: the exe holds a mutex `UnrealEngine3_<n>`; closing that
  handle inside the first instance lets the second start. `-nohomedir` keeps config, logs
  and saves inside the install folder.

## What we changed

Mod: https://github.com/EvilMask587/xcom2-wotc-coop-campaign (code only, no game files) has the
strategy prototype. The campaign work below extends it and is not published yet. Design, results
per build and every decision: [DESIGN-campaign-coop.md](DESIGN-campaign-coop.md); every test run:
[NOTES.md](NOTES.md).

**Strategy prototype (2026-10):**

- **Client join without post-mission side effects.** A plain `open Avenger_Root` treats a
  received history as a return from a mission and adds 7 frames and new ObjectIDs.
  `CoopHeadquartersGame` (ModClassOverrides) takes the load-game path instead.
- **False desyncs.** The fingerprint is compared when the merge state arrives, not when the
  command does (the command overtook it).
- **ObjectID collisions.** `CoopStrategyRuleset.SubmitGameStates` rejects frames that create
  objects without the baton.
- **Baton handoff race.** The receiver of a grant waits until its fingerprint and highest
  ObjectID match and 1 s passes without frames.
- **Holes in rooms.** The receiver queues a room swap like `XGBase.QueueRoomSwap`.
- **Edits lost on the non-holder.** Every edit screen asks for the baton first.

**Campaign co-op (2026-11):**

- **Up to four players** with slots 0-3; the host assigns a guest's slot on join. A guest
  remembers its slot per host address and asks for it again after restarting its game.
- **Host decides, guests manage their soldiers.** Guests never take the campaign baton; they
  may open the Armory list and their own soldiers' screens, and every other edit screen closes.
- **Soldier ownership.** An "Owner" button on the Armory soldier screen (host only), squad
  select "Slot N" / "Player" buttons for the host, and a colored owner tag next to each
  soldier's flag in missions (HOST gold, P2 green, P3 pink, P4 orange).
- **Mission turns per player.** One XCom initiative group per player, a selection gate so you
  can only select your own soldiers, handover between machines through the host, and the
  single-player rules put back. A banner shows whose turn it is. Watchers can pan the camera.
- **Disconnects.** The host takes over a dropped guest's soldiers; the guest reconnects by
  itself, joins the running mission and gets them back. The host tracks acknowledgements per
  guest slot, so a guest that leaves never blocks the others (found by `guestrestart`).
- **Saves.** Host only. The host saves mid-mission or with Ironman; after loading, it opens
  the server by itself and the guests rejoin.
- **Join and talk.** "Host co-op" on the Avenger HUD, "Join co-op" with an address on the main
  menu, a status line, an in-game mod list check that refuses mismatched guests, map pings
  and text chat (forwarded to every guest).
- **Crash fixed:** the game's PvP lost-connection handling is skipped in co-op missions (it
  crashed the guest in about one save test in three).
- **Crash fixed:** a guest leaving the Avenger when the host closed the connection died silently in about
  1 departure in 40 (no crash record; Windows' minidumps showed Wwise's event thread calling the game's
  matinee sound handler after the map unload had cleared the world). The guest now pauses the world for a
  second before it leaves.
- **Reconnect after a timeout.** When a guest timed out during a host movie and came back from the
  same game process, the host took it for a duplicate, never sent the campaign, and played on alone. The
  host now counts closed connections and treats a known guest that returns after a close as a
  reconnect with its old slot (found by `soak`).
- **Guest hang on the main menu.** A campaign update that reached a guest at the main menu sent it
  into an endless loop within one frame (a 23 GB log), adding frames that the main menu cannot add.
  History alignment now runs only on the Avenger and stops when a frame isn't added (found by `soak`).
- **Stuns on a guest's soldiers never ended.** Effects that count down on a group's own turn (stun)
  lost their countdown on the machine that plays that group; each machine now registers it again
  for the groups it plays (found by `stunguest`).
- **Joining by network address failed above 127.x.x.x.** The address is passed to the game's network code
  as one number, built in a signed 32-bit int, so every address from 128.0.0.0 up (home networks:
  192.168.x.x) came out negative and failed as a "host name". Only 127.0.0.1 worked, which every one-PC test
  used. Fixed, and a first join now retries for 5 minutes if the host isn't up yet (found by a two-PC
  rehearsal on one PC, guest aimed at the PC's own network address).
- **Enemies that arrive mid-mission never fired overwatch at a guest's soldiers.** A unit the host creates
  during a mission (reinforcements, the Lost, a Chosen arriving) reaches a guest as data only, without the
  event listeners that trigger its abilities, so its overwatch shot never fired when a guest's soldier moved.
  Each machine now registers missing ability listeners again, as loading a save does (found by
  `overwatchguest`; the host-only control `overwatchhost` fired normally).
- **A guest crashed when a mission started right after it rejoined.** If the host launched a mission while a
  guest's campaign download was still unacknowledged, the guest's acknowledgement of the *mission* was taken
  for the campaign's, and the guest opened the Avenger with the mission's data and crashed. A new history now
  replaces any pending one (found by `chosenassault` in the full suite). In the same window a guest could also
  keep a dead connection; a guest that gets its player slot but no history within 150 s now reconnects
  (not reproduced on purpose yet).

## QA

- Two to four copies of the game on one PC (127.0.0.1), each with its own folder. A test bot
  inside the mod (`CoopHarness`, `@` steps) drives each copy; a runner script starts the
  copies, asserts on every copy's log (expected lines, no crash, no desync, equal campaign
  fingerprints) and checks afterwards that the real install and saves are unchanged. The
  runner scripts are not published yet. The copies also take screenshots, checked by eye.
- Last results (2026-11-13 to 2026-11-16). Each row is the latest run of that scenario; the
  scenarios after `avdefreinf` are new content tests. On 2026-11-16 every scenario but the 8-mission soak ran
  in one batch on the same build: 38 of 40 passed; both failures were fixed and passed when rerun
  (`chosenassault`, a mod bug; `guestaction3`, a test setup gap). The real install was unchanged in every run.

  | Scenario | Players | What it covers | Result |
  |---|---|---|---|
  | `squad`, `exp12` | 2 | ownership tags, squad slot rules, group split | 13/13, 11/11 |
  | `join`, `joinmismatch` | 2 | join with matching mods, refusal on a mismatch | 7/7, 5/5 |
  | `join3` | 3 | two guests join one host | 12/12 |
  | `buttons` | 2 | "Host co-op" / "Join co-op" buttons | 14/14 |
  | `features` | 2 | Owner and slot buttons, guest rights, chat | 20/20 |
  | `guestrestart` | 3 | both guests restart, get their slots back | 19/19 |
  | `mission` | 2 | full rounds with enemies, pings, chat, camera, flags, banner | 54/54 |
  | `dropguest` | 2 | guest killed in its turn, host takes over, guest rejoins | 15/15 |
  | `missionend` | 2 | mission end and return to the Avenger | 11/11 |
  | `mission3`, `mission4` | 3, 4 | guest-to-guest handover through the host | 27/27, 33/33 |
  | `ironman` | 2 | Ironman autosave, load, rejoin | 18/18 |
  | `saveload` | 2 | save, load, second mission | 18/18 (twice) |
  | `midsave` | 2 | save inside a mission, load, rejoin | 16/16 (four times) |
  | `mindcontrol`, `mcall` | 2 | mind control of and by a guest's soldiers, freeing them | 24/24, 16/16 |
  | `lostchosen` | 2 | the Lost; a Chosen kidnaps a guest's soldier, mission end | 21/21 |
  | `retaliation`, `blacksite` | 2 | Chosen retaliation (resistance, civilians); a story mission | 14/14, 14/14 |
  | `avengerdefense`, `avdefreinf` | 2 | Avenger defense; its reinforcements go to their owner | 15/15, 15/15 |
  | `flankcrit` | 2 | flank crits use the single-player bonus (shot breakdown, host and guest) | 10/10 |
  | `bigsquad` | 2 | 12 soldiers on one mission, split between two players | 13/13 |
  | `stronghold`, `chosenassault` | 2 | a Chosen stronghold; the Chosen attack on the Avenger | 13/13, 13/13 (twice after the fix) |
  | `finalmission` | 2 | the final mission's map, three rounds | 13/13 |
  | `avatarmc` | 2 | an Avatar mind-controls a guest's soldier; control ends on time | 17/17 (three times) |
  | `relayleave` | 3 | a guest leaves while the host relays another guest's turn, rejoins | 16/16 |
  | `soak` | 2 | four missions in a row with the Avenger in between; the guest reconnects after the host's long movie | 13/13 |
  | `soakstory` | 2 | four missions with their objectives completed: the story moves on past the Blacksite | 14/14 |
  | `soakstory8` | 2 | eight missions the same way, a Chosen retaliation among them | 22/22 (twice) |
  | `guestaction` | 2 | a guest's soldier moves and shoots with the game's normal actions; every unit the same on both machines | 13/13 (three times) |
  | `guestaction3` | 3 | the same with three players: one guest's move and shot reach the host and the other guest | 11/11 (twice) |
  | `aiturnguest` | 2 | the enemy AI takes its turn against the squad; every unit the same on both machines afterwards | 8/8 (twice) |
  | `overwatchhost`, `overwatchguest` | 2 | an enemy on overwatch fires at a soldier walking into view: the host's, then a guest's | 8/8 (twice), 8/8 (twice, after the fix) |
  | `stunguest` | 2 | a Stun Lancer stuns a guest's soldier; it loses one action and the stun ends | 14/14 (twice) |
  | `dazeguest` | 2 | a Chosen Warlock's Mind Scorch dazes a guest's soldier: two turns lost, free on the third | 15/15 |

- Two-PC test kit (not published): the runner can run the host alone and grade a guest log from another PC;
  a script on the guest PC makes its own test copy and runs the guest bot. Rehearsed on one PC over its network
  address: `join` 7/7, `mission` 54/54.
- **Two PCs (2026-11-15):** `join` 7/7, `mission` 54/54 and `missionend` 11/11 over a home network, direct IP, one Steam account (guest PC in
  Steam offline mode).
- Not covered: long sessions on two PCs, Steam sockets and invites, auto-subscribing a guest to Workshop mods
  (the CoopSync tool has unit tests only), a full campaign played to the end, sessions longer than eight missions.

## How to play

Not playable yet: the campaign code has to be published first. Developers: the prototype's build
and test instructions are in the mod repo README.

## Known issues

- Two PCs tested over direct IP only (join, mission, mission end); untested over Steam (invites and Steam
  sockets).
- A guest who drops mid-turn loses the actions of that turn; the host plays on from there.
- While the host has any edit screen open (the Geoscape included), a guest waits to edit
  its own soldiers.
- Other mods that replace `UIUnitFlag` conflict with the owner tags, and mods that replace
  `XComHeadquartersController` with the guest's Avenger exit. robojumper's squad
  select screen has not been tested with the slot buttons.
- Test copies show the game's own debug "red screens", still with Steam's `-noRedScreens -review` added; every
  message is from the base game (maps, DLC sounds), none from the mod. Whether a plain Steam launch shows them: unknown.
- When a visualization on a guest times out while it waits for its turn, the mod finishes it so the turn
  can start; the root cause (an AI reveal timing out on the guest) is unknown.
- Not tested: mind control by the Chosen, story cutscenes, a rescued soldier joining mid-mission.
- Timed effects lost their countdown after a handoff and never ended. They are now registered again
  (`CoopListenerRepair`): by the host for the player's turn (mind control), and by each machine for
  the groups it plays (stun, daze). Tested: mind control, stun, daze. Other group-turn effects use the same
  repair but are untested.
- The host sends nothing while it plays a long movie (98 s after the first mission). Guests time out
  after 60 s and come back by themselves through the main menu (checked in `soak`).
- A rare guest crash as its first turn begins (`XCom2.exe+0xb158a7`, 2 runs, 2026-11-14 and 2026-11-16), both
  right after the host changed an alien (mind control, overwatch) before the guest's turn. Cause unknown.
- The host crashed once in the game's sound engine (the same Wwise race the guest fix covers) when
  the test bot launched a mission straight from the Avenger; the bot now pauses for a second first.
  Whether a player's normal launch, after the Skyranger flight, can hit it is unknown.

## Next steps

- Publish the campaign code (a fork or a PR to the prototype's repo).
- Two PCs: direct IP, then Steam invites; auto-subscribe a guest to the host's mods.
- Play a whole campaign.
- Two more modes on the co-op menu, after the co-op campaign works on two PCs: a **balanced co-op
  campaign** (4-6 soldiers per player, enemies scaled to the number of players) and a **scenario mode** (single
  co-op missions without a campaign: 12 operators split among 1-4 players, gear tier set by the game stage,
  abilities, stats, special suits and hero classes bought with points, several objectives to split up for).
  Plan and open questions in [DESIGN-campaign-coop.md](DESIGN-campaign-coop.md#game-modes-decided-2026-11-13-scenario-mode-replaced-horde-defense-2026-11-16).
