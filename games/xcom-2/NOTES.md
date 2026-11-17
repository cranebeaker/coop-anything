# XCOM 2: War of the Chosen — co-op campaign notes

Running log. Observations, derived facts, guesses and unknowns are kept in separate lists.

## Goal
- Shared strategy layer (Avenger): both players can pick research/build/facility upgrades and
  edit any soldier.
- Squad select: each player picks their own squad (e.g. 4 + 4 = 8 on the field);
  a soldier picked by one player is locked for the other.
- Tactical: each player controls only their own soldiers; players alternate.
- 2 players, Steam, WotC + all DLC.

## Session 1 — 2026-10-24: triage + research

### Observed
- Game identity: Steam build 8270065, changelist 699444, UE3.
- Team Dragonpunk "XCOM 2: Tactical Co-Op", Workshop 792693587: status "Final", created
  3 Nov 2016, last updated 3 Sep 2017, 61.8 MB. Two-player tactical co-op on campaign
  missions, players split the squad; can import soldiers from the friend's game; no
  Ironman; both players must be in Squad Select when inviting. Team moved to other games.
  Source: https://steamcommunity.com/sharedfiles/filedetails/?id=792693587
- No public source repo found (GitHub repo search "xcom2 coop", "dragonpunk xcom": 0 results).
- robojumper's "WOTC Multiplayer mods" (Workshop 1571537500) soft-deprecated in favour of a
  X2WOTCCommunityHighlander feature for mods in multiplayer.
  Source: https://steamcommunity.com/sharedfiles/filedetails/?id=1571537500
- Steam app 602410 = "XCOM 2 War of the Chosen SDK" (installdir name in steamapps).

### Searched (nothing more found)
- web: "XCOM 2 co-op campaign mod", "Team Dragonpunk ... github source",
  "WOTC shared campaign multiplayer strategy layer XComGameStateNetworkManager"
- GitHub API repo search: "xcom2 coop", "dragonpunk xcom"
- coop-anything DIRECTORY/games/issues: no XCOM entry

### Unknown
- Whether Tactical Co-Op works on WotC (last update 5 days after WotC release; probably vanilla).
- Whether its Workshop package ships `Src/` (many XCOM 2 mods do).
- How the vanilla MP layer (XComGameStateNetworkManager) sends game states — read SDK source.

## Session 1 (cont.) — SDK source reading

SDK: Steam app 602410 "XCOM 2 War of the Chosen SDK", buildid 3075887, 4137 .uc files in
`<SDK>/Development/SrcOrig`. Community Highlander (MIT, github.com/X2CommunityCore/
X2WOTCCommunityHighlander, pushed 2026-10-12) cloned for reference.

### Observed (source, WotC SDK SrcOrig/XComGame/Classes)
- `XComGameStateNetworkManager` (native(MP), script-callable): CreateServer/CreateClient(URL),
  SendHistory, SendPartialHistory, SendGameState, SendMergeGameState, SendGameStateContext,
  SendRemoteCommand(string, array<byte>) + AddCommandParam_Int/String/Bool helpers,
  SendLoadTacticalMap, SendPlayerReady, SendSyncPoint, validation/desync reports.
  Delegates for every receive. `HasServerConnection/HasClientConnection`.
- `X2GameRuleset.SubmitGameStateContext`: if NetworkMgr.HasConnections() and the context
  was user input (`bSendGameState`), it is queued for sending; remote contexts
  (`bNetworkAdded`) reuse the sender's random seed.
- Shipped config `XComGame/Config/DefaultGame.ini:105 bPauseGameStateSending=true`:
  MP uses "active player is authority": active player runs the ruleset; at end of turn
  `SendPartialHistory`, remote replies "HistoryReceived", then "SwitchTurn"; the inactive
  player watches via `XComMPReplayMgr` (X2TacticalMPGameRuleset.uc ~300-360, 1095-1275).
- Remote-ness = `XComGameState_Player.GetGameStatePlayerNetId() != local UniqueId`
  (X2TacticalMPGameRuleset.GameStatePlayerOrGroupIsRemote).
- Transport: `UIMPShell_Lobby.CreateServer(Port=7777, bSteam)`: plain IP, or Steam sockets
  when URL op "steamsockets" is added. Engine NetworkDevice = IpNetDriverSteamworks.
- Cheat exec commands (XComCheatManager.uc 3697-4091): MPCreateServer, MPCreateClient,
  MPSendHistory, MPSetPauseGameStateSending, X2PrintNetworkDebug, MPCreateLobby, ...

### Derived (design direction, not yet tested)
- Tactical: keep ONE XCom player; split the XCom turn into sub-phases A then B. The
  sub-phase owner is the authority; handoff = SendPartialHistory + replay (same as MP
  end-turn). Alien/Lost/Chosen turns run on the host. Units carry an owner tag.
- Strategy: host-authoritative save; client gets full history on join (SendHistory).
  Edits via a "baton": one player edits at a time, handoff sends history. Later: per-object
  locks + SendMergeGameState for parallel soldier editing.
- Test transport: IP on 127.0.0.1, two instances on one PC (unknown if the exe allows it).

### Unknown
- Can two XCom2.exe instances run at once on one PC/Steam account?
- Does SendHistory work while in the strategy layer (Avenger), and does the client's
  visual state (Avenger, Geoscape) refresh after receiving it?
- Where test runs write config/logs (likely %USERPROFILE%\Documents\My Games\...).

## Session 1 (cont.) — running test instances

### Observed
- The test install had a corrupt `XComGame/CookedPCConsole/TacticalResources_SF.upk`
  (crash at shell load: "Texture2D AdventDeskA.Textures.AdventComputerA_MSK: Serial size
  mismatch: Got 1588, Expected 1956"). Steam "Verify integrity" replaced it (6.7 MB);
  the shell menu then loads. Lesson: verify integrity BEFORE copying a test install.
- `-nohomedir` (string present in exe) makes the game write Config/Logs/SaveData into
  `<install>/XComGame/...` instead of `%USERPROFILE%\Documents\My Games\...`
  (generated XCom*.ini appear in `<install>/XComGame/Config`). Keeps tests away from
  the real Steam Cloud folder.
- Second instance exits with code 1 before writing a log while the first one runs.
  Cause: mutex `\Sessions\<n>\BaseNamedObjects\UnrealEngine3_<n>` (created via
  `CreateMutexW("UnrealEngine3_%d%s")`). Closing that handle inside instance A
  (DuplicateHandle with DUPLICATE_CLOSE_SOURCE, same user, no admin) lets B start;
  both log "STEAMWORKS initialized 1". Closing `CDdf212806D6EmB31yE0c` and
  `{52813408-...}_0` did not help (not the guard).
- Second install = hard-link clone of the first, but with its own copy of
  `XComGame/Config` (the game rewrites inis there).
- Tools: `tools/launch_pair.ps1`, `close_remote_handle.py`, `hardlink_clone.py`,
  `re_callers.py`, `re_dis.py`, `trace_start.py` (in the author's workspace, to be moved
  into the game repo).

## Session 1 (cont.) — first network experiments (two instances, one PC)

Mod repo: `WOTCCoopCampaign` (the author's workspace; to be published as its own repo).
Test bot: `CoopHarness` (Actor, steps from `XComCoopHarness.ini`, progress kept on the
class default object across map loads) + `CoopHarnessNet` (Object holding network
delegates). Runner: `tools/run_pair_test.ps1` (activates the mod through the test installs'
`DefaultModOptions.ini`; the game regenerates `XComModOptions.ini` from it).

### Observed
1. `XComGameStateNetworkManager.CreateClient` with a dotted host does not send a single
   packet (UDP sniffer on the target port: 0 packets; B log shows the peer as
   `0.0.0.127`). With the host as ONE decimal integer (127.0.0.1 -> "2130706433") the
   client sends at once (95 packets in 30 s). Applies to `IpNetDriverSteamworks` IP mode.
2. Client and server on one PC must not share the port: the client logs
   "Game client on port 7777" (URL default port). Server on 7778 works.
3. `HasConnections()` is already true while a server only listens; use the
   `PlayerJoined` / `NotifyAcceptedConnection` delegates to detect a real peer.
4. Delegates on the network manager must not point at an Actor: the manager outlives
   maps, the old World is kept alive and the next map load asserts
   ("World XPACK_Shell_Intro.TheWorld not cleaned up by garbage collection!",
   UnGame.cpp:3107, referencer `XComGameStateNetworkManager...ReceiveHistoryDelegates`).
5. Two-way remote commands work (CoopPing/CoopPong both directions).
6. **Strategy history transfer works**: A starts a campaign (cheat start, 225 frames,
   22 soldiers, 4000 supplies), `MPSendHistory`; B in the main menu receives
   `ReceiveHistory frames=225` with the same first soldier (ObjectID 411) and supplies,
   then `open Avenger_Root?game=XComGame.XComHeadQuartersGame` loads A's campaign on B.
   The connection survives the map travel (ping/pong after the load).
7. After loading the Avenger, B's history grows by 7 local frames (225 -> 232) that A
   does not have: map-load side effects create game states locally.

### Derived
- Join flow for the shared campaign: host loads save -> client connects -> host sends
  full history -> client opens the Avenger. Proven end to end in the harness.
- Frames created locally on load (item 7) must either be suppressed on the client or
  the client must accept being overwritten by the host's next partial history.

### Unknown / next
- Steam transport (`steamsockets`) between two different Steam accounts.
- Does SendPartialHistory (delta) apply on top of the client's 7 extra frames?
- How the Avenger/Geoscape visuals refresh when history changes under a loaded map.

## Session 1 (cont.) — strategy sync with a baton (merge game states)

Mod changes: `CoopHeadquartersGame` (ModClassOverrides on XComHeadquartersGame) lets the
client enter the Avenger through the loaded-save path ("mirror") instead of the
return-from-tactical path. `CoopSession` sends every new local frame of the baton holder
as `SendMergeGameState` + a "CoopFrame" fingerprint command (supplies, intel, alloys,
elerium, crew, facilities, techs, squad size).

### Observed (test "baton-both-ways", run_pair_test.ps1, two instances, 127.0.0.1)
1. Mirror entry: B loads A's campaign (222 frames) and adds only ONE local frame,
   "Pre Mission End UI Sequence Hook" (`XGGeoscape.OnEnterMissionControl`, created
   unconditionally, also on a normal save load). The plain `open Avenger_Root` path
   added 7.
2. A -> B: `GiveResource Supplies 123`, `GiveResource Intel 7` on A; B logs
   "in sync" for both (sup4123, int1007).
3. B -> A after the baton moves (`@nobaton` on A, `@baton` on B): `GiveResource
   Supplies 50`, `GiveResource AlienAlloy 9` on B; A logs "in sync" for both
   (sup4173, all1009). Final fingerprints are equal on both sides.
4. A received merge state is applied to the receiver's current frame, it does NOT add a
   frame: A stays at 225 frames while B, which made the changes, has 227. Frame counts
   and history indices are therefore not comparable between the players.
5. The "CoopFrame" remote command sent AFTER the merge state is handled BEFORE it on the
   receiver (same tick). The first run compared fingerprints in the command handler and
   logged false DESYNCs. Fix: queue the fingerprint and compare in
   OnReceiveMergeGameState.
6. Two frames sent in one tick (cheat + "Analytics Resource Changed") arrived as two
   CoopFrame commands but ONE merge-state callback. The newest fingerprint is used.
7. The client creates local frames from pure UI actions while it does not hold the baton
   (Entered/Exited Facility, Unmark needs attention, Psi Lab Intro alert seen, Trigger
   Objective Narrative Moment). A full `MPSendHistory` from the host overwrites them.

### Derived
- Fingerprint/content comparison works, frame-index comparison does not (item 4).
- Merge states fit "one editor at a time" (baton) for resource changes. Not covered yet:
  object creation (new soldier, facility build, research start), where new ObjectIDs are
  allocated on one side only.

### Unknown / next
- Do merge states carry newly created objects and keep ObjectIDs aligned?
- Do Avenger/Geoscape UI elements refresh after a merge (resource bar was not checked
  visually; only the game state was read).
- Policy for the client's UI-only local frames (item 7): drop, send, or overwrite.

## Session 1 (cont.) — objects created through merge states

Harness additions: `@crewinfo N` (last N crew members, duplicate-ID check), `@objrange a b`
(class of every ObjectID in a range). The fingerprint now also carries the last ObjectID
of Crew, Facilities and TechsResearched. No full `MPSendHistory` after the join in these
tests, so only merge states move data.

### Observed (tests "baton-objects", "baton-objids")
1. A (baton): `GiveScientist` -> new Unit 2316 on A; `GiveTech Psionics` (adds the tech +
   3 "Trigger Objective Narrative Moment" frames). B: "in sync", the scientist exists on
   B with the same ObjectID and name.
2. Baton to B: `GiveEngineer` -> new Unit 2318 on B; A: "in sync", same ID and name.
3. `@objrange 2280 2345` gives the same class for every ID on A and B, before and after
   both creations. ID 2317 is empty on BOTH sides (allocated, never stored; cause unknown).
   No duplicate crew IDs. So the receiver's ObjectID counter moves past the received
   objects; no ID collision when the other player creates objects next.
4. Mirror entry allocates no ObjectIDs in that range ("Pre Mission End UI Sequence Hook"
   only modifies existing objects).
5. A sent 6 frames for the scientist + tech; B got 2 merge-state callbacks (one per
   tick). Merge states coalesce per tick.
6. run_pair_test.ps1 printed only the last 60 log lines, so long dumps hid everything
   else; now 200 lines, empty `obj N none` lines dropped.

### Derived
- With one editor at a time (baton), merge states are enough for resource changes and
  for creating units and techs, including ObjectID alignment.

### Unknown / next
- Facilities (GiveFacility also streams room maps on the sender; the receiver's Avenger
  visuals are not refreshed by a merge state).
- UI refresh on the receiver after a merge (resource bar, crew lists).
- Client's UI-only local frames while it has no baton (drop / send / overwrite).
- Edits by both players at the same time (no baton): not covered by tests, expected to conflict.

## Session 1 (cont.) — facilities and screen refresh on the receiver

Test installs: redscreens turned off (`[XComGame.RedScreenManager] bDisabled=true` in the
test copies' DefaultGame.ini, backup `.bak`). The screenshots of redscreens were from
the test windows: "FullyLoading in DynamicLoadObject: DLC_60/DLC_90_NarrativeMoments" and
"Plot_Multiplayer_Test ... Unable to find associated package": vanilla dev warnings, also in
logs of runs without changes, unrelated to the mod. Still logged as `Warning: Redscreen:`.

Harness additions: `@rooms` (per room: facility, wanted map, streaming state, STALE when the
streamed map differs from the state), `@hud` (HQPres.ClearUIToHUD), `@lookroom N`,
`@clearroom N` (removes a facility: RemoveStateObject), `@buildroom N Template` (state
changes of GiveFacility, screen via RefreshAvenger). In-game `shot` writes
`<install>/XComGame/Screenshots/PCConsole/ScreenShotNNNNN.bmp` (works in windowed mode).
Cheat start fills all 19 rooms, so tests clear room 3 and build into it.

`CoopSession.RefreshAvenger(state)`, run on every received merge state (and after the
harness' own room edits): for each HeadquartersRoom in the state whose streamed map differs
from `XGBase.GetMapname(room)`, queue a room swap exactly like the private
`XGBase.QueueRoomSwap` (add to public `m_queuedRoomSwaps`, `StreamInRoom(idx, true)`; the
old map is removed in `OnRoomLevelLoaded`), then `m_kAvengerHUD.UpdateResources()` and
`m_kFacilityGrid.UpdateData()`.

### Observed
1. Removal travels in merge states: `@clearroom 3` on A -> B "in sync", facility count
   19 -> 18 on B, room 3 swapped to AVG_ER_A.
2. Creation: `@buildroom 3 Laboratory` on A -> new facility ObjectID identical on B, room
   swapped back to AVG_Laboratory_A on B, "in sync". `@rooms` on B: no STALE room.
3. B's resource bar follows: power 42/18 -> 39/18 -> 42/18 in B's screenshots, same as A.
4. The GiveFacility cheat's own screen update (RemoveRoom + StreamInRoom) left a hole in the
   room on the SENDER ("A rebuilt" screenshot, first run). The queued swap shows the
   room correctly on A in both later runs.
5. B's camera after `@hud` + `@lookroom 3` differs from run to run (4 runs): correct
   lab view (2x), view "inside a wall" with neighbour rooms (1x), view outside the Avenger
   on the landing site desert (1x). All 19 rooms are loaded and visible on B
   (ld/vis/want=true) when this happens. A's camera was correct in all runs.

### Unknown / next
- Cause of item 5: the mirror entry path (skips the normal load camera setup?), a
  race between ClearUIToHUD's exit from the strategy map and CAMLookAtRoom, or merges
  arriving during the transition. Must be solved before players use the client's Avenger.
- Real construction (build project + time passing) sets `UpdateRoomMap`; the receiver's
  own SetAvengerVisibility will also swap the room and write a local frame (not covered by tests).

## Session 1 (cont.) — client camera

Harness: `@cam` logs camera state, CameraState class, CurrentRoom, AnimTime and the player
view point (loc/rot). `unsuppress DebugHQCamera` (console) enables the game's camera log
(suppressed in DefaultEngine.ini).

### Observed (test "camera")
1. Mirror entry (`CoopHeadquartersGame`) differs from the shipped load path
   (`XComHeadquartersGame.StartMatch`, bPerformingStandardLoad) only by
   `OnlineEventMgr.FinishLoadGame()` (native, "instantiate saved actors").
2. After mirror entry B's camera is in `FreeMovementView` (the Avenger cross-section, as
   after `BootstrappingStrategy`); A after the cheat start is in `EarthView`.
3. After `@hud` + 8 s + `@lookroom 3`: A and B both `BaseRoomView`, room AddonCam_R1_C1,
   loc=-224,-280,1380 rot=0,16384,0; both screenshots show the lab correctly.

4. Mouse movement over the game windows during the earlier runs was confirmed by the tester.
   B is launched second and has the window focus; A never had it and never showed a wrong
   view; FreeMovementView pans with the mouse.

### Derived
- The wrong client views (inside a wall, desert outside the Avenger) were mouse input,
  not a sync or mirror-entry bug. Lesson for tests: nobody touches the PC during a run;
  log `@cam` next to every `shot`.

## Session 1 (cont.) — local frames on the player without the baton

### Observed
1. Test "client-local-alloc": B WITHOUT the baton ran `GiveScientist` -> local Unit 2339.
   Then A (baton) ran `GiveEngineer` -> Unit 2339 on A too. A's merge state replaced
   B's object 2339 with the engineer; B's scientist disappeared. Fingerprints said
   "in sync" (they compare the end state, the collision is invisible to them).
2. `CoopStrategyRuleset` (subclass of X2StrategyGameRuleset, returned by
   `CoopHeadquartersGame.GetGameRulesetClass()`) overrides `SubmitGameStates`: while
   connected, without the baton and not applying a remote state, a frame that creates
   an object is rejected (`CleanupPendingGameState`, logged "blocked frame without
   baton"). Frames that only modify existing objects pass.
3. First version tested "new" with `History.GetGameStateForObjectID(id) == none`: never
   true, nothing was blocked (the history already resolves IDs of the pending state).
   `StateObject.GetPreviousVersion() == none` works.
4. Test "client-local-alloc-blocked-2": B's GiveScientist blocked (ID 2319 allocated,
   never stored); the cheat's follow-up "New Popup Event" frame still passed; no
   script warnings. A's engineer arrives on B as 2319, no replacement. After the baton
   moves to B, B's scientist (2321) reaches A; both sides end equal, no duplicate IDs.

### Derived
- Rule: the player without the baton must not create objects; blocking at
  SubmitGameStates is the safety net. Real UI flows assume the submit succeeds, so the
  UI for the non-holder must not offer such actions (hire, build, research start...).

### Unknown / next
- Baton handoff over the network (request/grant command, both sides agree) — today the
  harness sets it on each side by hand.
- Non-holder UI: which screens create objects; disable or forward them.
- Frames that modify existing objects on the non-holder diverge until the next merge of
  the same object; harmless for UI flags, unknown for anything else.

## Session 1 (cont.) — baton handoff over the network

Protocol (CoopSession): the non-holder sends "CoopBatonRequest"; the holder drops the
baton and sends "CoopBatonGrant" (epoch, fingerprint, highest ObjectID in the history);
the receiver takes the baton when its fingerprint equals the grant's, its highest
ObjectID is >= the grant's, and no remote frame (CoopFrame or merge) arrived for 1 s;
then it sends "CoopBatonAck". The check is retried on every merge state and by a
one-shot actor timer (CoopGrantRetry). Harness: `@batonreq`, `@grant` (unrequested grant,
for the race), `@waitbaton`.

### Observed
1. First version waited until the queue of received CoopFrame fingerprints was empty.
   Test "baton-protocol": B sent 3 frames, A got 3 CoopFrame commands but fewer merge
   callbacks; one fingerprint stayed queued forever and A never took the baton.
   Commands and merge callbacks do not pair up 1:1.
2. Second version: `WorldInfo.SetTimer(..., inObj=session)` for the quiet period never
   fired (test "baton-protocol-2": B had matching fp + maxID, waited forever).
   A spawned Actor's own timer works.
3. Test "baton-protocol-3": A GiveScientist (2323) then `@grant` 0.05 s later. B received
   the merge first, then the grant; matched at once, waited 1 s of quiet, took the baton,
   GiveEngineer -> 2324, A received it "in sync". Then A `@batonreq`: B granted, A took
   it immediately (last remote frame 23 s earlier), GiveEngineer -> 2325, B received it.
   End: both 28 crew, IDs 2323/2324/2325, no duplicates, fingerprints equal.
4. A frame the game creates right after a grant on the old holder (cheat follow-up
   "New Popup Event") stays local (modifies existing objects; not blocked, not sent).

### Unknown / next
- MaxObjectID iterates every object in the history (~2300 here) on each check; cost in a
  late campaign unknown.
- Late frames that only modify existing objects and arrive after the quiet period would
  be merged over the new holder's edits.
- UI: who asks for the baton and when (button, automatic on opening a screen that edits).

## Session 1 (cont.) — edit screens take the baton (CoopEditGate)

Not only object creation needs the baton: an edit by the non-holder that only modifies
objects (e.g. a loadout) is never sent and is lost on the next merge of that object. So
every screen where the player changes the campaign requires the baton.

`CoopEditGate` (UIScreenListener for all screens), list `EditScreens` in
`Config/XComCoopCampaign.ini` (class names, subclasses count: UISimpleCommodityScreen,
UIInventory, UIArmory, UIBuildFacilities, UIChooseFacility, UIFacilityUpgrade,
UIChooseUpgrade, UIRecruitSoldiers, UISquadSelect, UIBlackMarket_Sell, UICovertActions,
UIStrategyMap, UIStrategyPolicy, UIPersonnel). UIStrategyMap is in the list because the
Geoscape runs the clock.
- Edit screen opened without the baton -> RequestBaton + modal `UIProgressDialog`
  ("Partner has control", Cancel closes the edit screen); closed by SetBaton(true).
- Holder gets a request while an edit screen is open -> deferred (`bGrantRequested`),
  granted in OnRemoved of the last edit screen.
- Texts in `Localization/WOTCCoopCampaign.int` and `.rus` (UTF-16).
Harness: `@open research`, `@open build`, `@close`, `@screens`.

### Observed (test "edit-gate")
1. A on the Geoscape after the cheat start (stack: UIAlert < UIMouseGuard <
   UIStrategyMap[edit] < ...). B `@open research` -> wait dialog on B; A logs
   "baton request deferred: edit screen open".
2. A `@hud` (leaves the Geoscape) -> "last edit screen closed, handing over the requested
   baton"; B takes it after the 1 s quiet period; the dialog closes; B's screenshot shows
   the research list.
3. A `@open research` while B is in research -> dialog on A, deferred on B; B `@close` ->
   grant -> A takes it, dialog gone, A's screenshot shows the research list.
4. Russian texts render in the dialog. Opening research on the non-holder still writes a
   local "Open Choose Research Event" frame (modification only, passes).
5. Two merges during one grant wait scheduled two CoopGrantRetry timers; the second call
   is a no-op.

### Unknown / next
- Cancel button: not clicked in a test yet (needs a harness command to press it).
- Screen list is a first guess: staff slots inside UIFacility, Avenger room menus,
  UIPersonnel used read-only (now also asks for the baton), screens added by DLC/mods.
- The holder who stays on the Geoscape never hands over (by design, but needs a visible
  hint for the partner: who has control, "ask for control" button).

## Session 1 (cont.) — baton indicator on the Avenger HUD

`CoopBatonPanel` (UIPanel, added to UIAvengerHUD by CoopEditGate.OnInit; top right under
the resource bar, 600x40; polls CoopSession every 0.5 s; hidden while not connected):
"Control: you" / "Partner asks for control - close the screen to hand it over" (holder
with a deferred request) / "Control: partner" + button "Ask for control" (disabled
"Asked..." after a click until the baton arrives) / "Control is being handed to you...".
`CoopSession.bRequestSent` stops repeated requests, cleared when the baton arrives.
Harness: `@panel`, `@ask` (calls the button handler), `@cancelwait` (UIProgressDialogue.
OnCancel, the same path as clicking Cancel).

### Observed (tests "baton-panel", "panel-cancel")
1. B: "Control: partner" + button -> `@ask` -> "Asked..." (disabled). A (on the Geoscape)
   shows "Partner asks for control..." (log), A `@hud` -> grant -> B "Control: you",
   button hidden; A "Control: partner" + button. Screenshots match the log.
2. The Russian button label was cut by the screen edge at width 470/button 185; at
   600/260 it fits.
3. Cancel: B `@open research` -> dialog -> `@cancelwait` -> "waiting for baton cancelled,
   closing UIChooseResearch", stack back to UIFacilityGrid. The request stays sent; when
   A leaves the Geoscape B still receives the baton.
4. Launch.log shows the Russian labels as mojibake (log encoding); the screen is fine.

5. The panel is visible on the Geoscape too (UIAvengerHUD stays on screen there),
   so the holder on the Geoscape sees "Partner asks for control".

## Session 1 (cont.) — request replies, empty frames, a client crash

Request replies: the holder answers a deferred request with "CoopBatonDeferred" (the
requester shows "Partner is busy - control comes once they leave their screen", no
timeout); a non-holder answers "CoopBatonNotMine" (requester's button is enabled again).
Without any answer the request expires after 10 s (`CheckRequestTimeout`, run by the
panel). Long texts get a two-line panel background (70 px), short ones 40 px.

Empty frames: frames without objects (the game's "Entered Facility", harness
`@emptyframe`) are no longer sent. SendMergeGameState refuses them ("Failed to send game
state for merging! GameState has 0 Objects!") while our CoopFrame command still went out:
this was the cause of more CoopFrame commands than merge states seen earlier.

### Observed
1. Test "baton-replies-2": B `@ask` -> "CoopBatonDeferred" -> busy text, unchanged after
   16 s; A leaves the Geoscape -> grant -> B "Control: you". B `@nobaton` + `@ask` -> A
   "CoopBatonNotMine" -> B's button enabled again. No "0 Objects" lines after the fix.
2. Test "baton-replies" (first version): a tester clicked in both windows during the run
   (entered facilities, opened the Armory, pressed Cancel, pressed "Ask for control"
   several times): 5 handoffs back and forth, all with equal fingerprints; the edit gate
   and Cancel worked from real mouse clicks.
3. In that run B crashed 0.2 s after a harness `@nobaton`, while inside a facility
   (the tester's clicks): access violation reading 0xffffffffffffffff in tbbmalloc.dll+0x29c9.
   Stack scan (tools `minidump_info.py`, `re_func_strings.py`, heuristic) shows
   UWorld::Tick and a UnLinker.cpp function (package loading) above the allocator. No
   script call stack in the log. RAM 32 GB, 13 GB free.
4. Ten empty frames in a row (test "empty-frames", before the fix) did not crash.

### Unknown
- Cause of the crash in item 3. Engine package loading during a facility visit is the
  best lead; not reproduced. Watch for it in later runs; a run that enters facilities
  on the client while frames arrive would be the next reproduction attempt.

### Unknown / next
- A request that gets no answer expires after 10 s; a deferred one never expires.

## Session 1 (cont.) — test suite

`tools/test_suite.ps1` in the mod repo turns the runs above into five scenarios with
assertions on both logs (expected lines; no DESYNC, crash, duplicate crew ID, stale room,
bot timeout; equal final fingerprints). First run: false FAIL, the forbidden word "STALE"
matched the engine's "MaxStaleCacheSize" (Select-String is case-insensitive by default);
now case-sensitive "STALE loaded=". Second run 2026-10-25: 5/5 passed.

## Session 2 — 2026-10-25: Geoscape clock, lost frames, batching

Harness: `@time` (clock in history / X2StrategyGameRuleset.GameTime / XGGeoscape.m_kDateTime,
research projects' completion dates), `@research <TechTemplate>` (XComHQ.SetNewResearchProject,
what UIChooseResearch does), `@geoscape`, `@scan` (XGGeoscape.FastForward), `@pause`,
`@burst N` (N frames in one tick, +1 supply each).

### Observed
1. The strategy clock lives in memory (`STRATEGYRULES.GameTime`, `XGGeoscape.m_kDateTime`);
   the history copy (`XComGameState_GameTime.CurrentTime`) is written by `XGGeoscape.Pause`
   ("Geoscape Pause") and some other frames.
2. Test "clock": A researched AlienBiotech and scanned 20 s (3-1 -> 3-5, research completed).
   B's history got 3-5, B's memory clock stayed at its join time (3-1). B took the baton,
   entered the Geoscape: its clock ran from 3-1; after a scan and pause (3-3) the merge state
   moved A's HISTORY clock back from 3-5 to 3-3. Fingerprints said "in sync" (no time in them).
3. Fix: on a received merge state with a GameTime object, copy it into the memory clock and
   `XComEarth.SetTime` (`CoopSession.ApplyClock`); before a grant the holder writes its memory
   clock into the history if it differs (`CommitClock`); the fingerprint starts with the history
   clock to the hour. Test "clock-2": B's memory clock followed to 3-5, B continued 3-5 -> 3-7,
   A received 3-7; both end with equal fingerprints including the hour.
4. Test "burst-2" (one frame per SendMergeGameState): B sent 10 + 20 frames in two ticks, A got
   3 merge callbacks; A ended with 4021 supplies, B with 4030. Frames were lost in the network
   layer (the sender logged all 30 "Wrote new game state frame to network queue"). The grant
   check refused the baton (fingerprints differed): no silent corruption, but a deadlock.
   (First "burst" run: 6 bursts up to 20 each way arrived late but complete; loss is not
   deterministic.)
5. Fix: batching. OnNewGameState only records the ObjectIDs a frame touched; 0.2 s later
   (`CoopFlushTimer`, and right before a grant) one merge state is built with
   ModifyStateObject (or RemoveStateObject) of each touched object's newest version, sent,
   and discarded with CleanupPendingGameState (never submitted); one CoopFrame fingerprint per
   batch, compared in order on the receiver (one merge callback per batch). Test "burst-3":
   bursts of 10/20/50 both ways -> 6 batches, both sides 4160 supplies, all "in sync".
6. Client crash again (test "clock", B 2 s after leaving the Geoscape as holder, 3 "Geoscape
   Pause" frames in one tick): Launch.log has a native stack; crash at XCom2.exe+0x4ce235 reading
   null+0xe0, in a worker thread whose task function references the string
   "FWriteHistoryObjects" (serialising history objects). With the earlier crash (also right
   after a client-side burst) this points at a race in the network layer's background
   serialisation of merge states. Bursts of up to 20 unbatched frames (test "burst") and 50
   batched ("burst-3") did not crash; not reproduced on demand. Batching sends far fewer merge states, which should also
   make this race rarer; unverified.

### Unknown / next
- Whether batching removes the crash (watch the suite and longer runs).
- Merge states built from copies: tested for HQ resources, research, facilities (suite); not for
  every object type.

## Session 2 (cont.) — the client crash: client sends while it saves

Harness: `@autosave` (XComAutosaveMgr.DoAutosave, what BaseRoomView.BeginState does when the
camera goes from the Geoscape back to the Avenger).

### Observed
1. Suite run with "clock": B crashed again at the same address as before (XCom2.exe+0x4ce235,
   reading null+0xe0, worker thread, task "FWriteHistoryObjects"), 1.4 s after leaving the
   Geoscape, with no merge state sent in between. Leaving the Geoscape autosaves.
2. Test "autosave": B without the baton autosaved 10 times (frames local only): no crash.
3. Test "autosave-burst": B with the baton, `@autosave` then `@burst 5` 0.05 s later: crash on
   the first autosave (the burst went out as a batch while the save ran).
4. Test "autosave-burst-2": A (host, baton) autosave + burst + batch 6 times: no crash.
   B without the baton autosave + local bursts 6 times: no crash.
5. Test "client-send-then-save": B takes the baton and autosaves: crash on the first save (the
   frames the save itself creates are batched and sent 0.2 s later, during the save).
6. Fix: `CoopAutosaveMgr` (ModClassOverrides of XComAutosaveMgr): no autosave / quicksave on the
   co-op client (`HasClientConnection`); FlushBatch waits while `XComOnlineEventMgr.SaveInProgress()`
   and a grant waits for the save too. Test "client-send-then-save-2": 6 client autosaves
   skipped, host saved while sending, no crash, equal final state.

### Derived
- Crash condition: the CLIENT sends a merge state while it is saving. Native, not fixable from
  script; avoided. Only the host saves the shared campaign (it was the plan anyway).

### Unknown
- The exact native cause (why the client and not the host).
- The save menu on the client (manual "Save game" screen) is not blocked yet.

## Session 2 (cont.) — the writer task, and a host crash

Tools (workspace): `re_xrefs.py` (vtable / lea references to a function), `re_calls.py`
(direct callers + their strings). Harness: `@netwatch S` (logs the network manager's
OutGameData/InGameData queues when they change), `@pendingstress N` (N pending states created
and discarded, like FlushBatch, no network).

### Observed
1. Suite (8 scenarios) 2026-10-25 01:58: 7/8; `clock` and the new `client-save` passed; in
   `baton-objects` the HOST crashed for the first time, same address (XCom2.exe+0x4ce235,
   null+0xe0), 0.01 s after receiving a merge state and 2.1 s after its own last batch + grant.
   It was not saving (its last autosave belonged to the previous scenario).
2. Disassembly: 0x4ce230 is a TMap lookup by ObjectID; the caller (0xcb6930) passes
   `frame + 0x9c` where `frame` comes from the history's frame array, and that frame pointer
   was null (fault at 0x9c + 0x44 = 0xe0). The worker reads a history frame slot that is empty.
3. The task "FWriteHistoryObjects" (vtable 0x14175c440) is created by: the save routine
   (0xbbc340, strings "Save0", "open Avenger_Root..."), challenge-mode post-game save, and
   0xce2c60, which is called by UXComGameStateNetworkManager::SendGameState and (through
   0xce2db0) SendMergeGameState and SendValidateGameState. Receiving does not create it
   (no native caller found).
4. `@netwatch`: OutGameData/InGameData were always empty when sampled every 0.05 s.
5. Test "pending-stress" (one instance, no network): 10 autosaves each followed by 20 pending
   states created and discarded: no crash.
6. Test "send-race": 30 batches per side, each followed 0.22 s later by 30 local frames while
   the send task may still run: no crash, all "in sync", both end at 4990 supplies.

### Derived
- Client crashes that involved a save are covered by CoopAutosaveMgr (5 of 6 crashes so far
  happened on the client right after or during a save).
- The host crash stays unexplained: one occurrence in about 45 scenario runs. A send task
  reading a history frame slot while it is empty is the best description; what empties it is
  unknown (candidates: merge states written into the current frame on the receiver, the
  batch's discarded pending state).

### Next
- Measure the frequency (repeat the suite) before more reverse engineering.

## Session 2 (cont.) — manual saves on the client

### Observed
- UIPauseMenu builds its Save item in InitScreen (index `m_optSave`, -1 when saving is not
  allowed); OnChildClicked opens the save screen through `Movie.Pres.UISaveScreen()` without
  checking whether the list item is disabled (the tutorial case re-checks it by hand).
- New listener CoopSaveGate: on the co-op client it disables the Save item with the reason
  "In co-op only the host saves the campaign" and closes UISaveGame if it opens anyway.
- Test "client-save-menu": B (client) logs "pause menu: save disabled on co-op client", then
  `@savescreen` (the menu item's call) logs "save screen closed" and the stack is back to
  UIPauseMenu; A (host) opens its pause menu with Save untouched. PASS on 2026-10-25.

## Session 2 (cont.) — repeated suite runs

### Observed
- Full suite (9 scenarios) three times in a row on 2026-10-25: 7/9, 9/9, 8/9.
- Host crash "FWriteHistoryObjects" (XCom2.exe+0x4ce235 reading null+0xe0) once more, again in
  `baton-objects` (round 1). Both host crashes so far happened in that scenario.
- Two other failures (`clock` round 1, `client-save` round 3) were a test-bench fault: the
  host's Launch.log stopped at 0.51 s ("Loading value for StaleCacheDays"), before any mod
  code ran; the client waited for the history, timed out and hit an UnAsyncLoading.cpp
  assertion while being stopped. 2 of 27 pair launches.
- run_pair_test.ps1 now relaunches both instances when either has not logged
  "CoopHarness: ready" within 60 s (up to 3 tries); test_suite.ps1 keeps both logs of a
  failed scenario in `<workspace>\suite-failures\`.

### Next
- `baton-objects` ten times in a row to get the host crash with its logs.

## Session 2 (cont.) — the host crash explained: merged versions name the sender's frames

### Observed
- `baton-objects` alone: host crash in 3 of the first 4 runs, then 1 of 4 (5 of 11 counting
  the suite rounds). Always 0.01 s after the host received the client's batch holding the
  new Engineer and the HQ.
- The minidumps hold the process heap. With the name table (array of FNameEntry*, found from
  a pointer to a known entry; entry = 8 bytes flags, int Index<<1, 8 bytes hash link, ANSI
  chars) and the UObject layout of this build (Outer +0x40, Name +0x48, Class +0x50), the
  UFunction pointers on the crashing thread's stack name the script stack:
  XGGeoscape.Tick > UpdateUIVisualsOnly > XComStrategyMap.UpdateVisuals > UpdateMapItemVisuals
  > UIStrategyMapItem.UpdateVisuals > UpdateFromGeoscapeEntity (on UIStrategyMapItem_Avenger_0)
  > XComGameStateHistory.GetGameStateForObjectID. The crash was the main thread, not a worker.
  Tools: `tools/mdmem.py`, `tools/md_uobj.py`, `tools/minidump_threads.py`, `tools/re_natives.py`
  (exec function table: {"UClassexecName" string, function} pairs in data).
- UpdateFromGeoscapeEntity looks the entity (the Avenger = XComHQ) up as of
  `HistoryIndexOnLastUpdate`, an older frame, so the history walks back through versions.
- XComGameState_BaseObject has `private int PreviousHistoryFrameIndex` ("linked list to the
  history frame of the direct ancestor"). Logged on receipt: a merged version's parent is the
  receiver's current frame, its previous version is "none" on one side, and calling
  GetPreviousVersion on it crashed the host the same way (2 of 2 runs).
- Frame counts at the crash: the client's batch went out at its current frame 231 (the HQ
  changed in its last frames); the host's last frame was 229. In the SendGameState
  experiment: client current 230, host last frame 228, crash again. The value of the private
  field itself was not read (unknown); that it exceeds the host's last frame is inferred.
- Experiment SendGameState + SubmitGameState of the received state on the receiver: the state
  arrives with HistoryIndex -1 and not in the history; submitting it adds a frame, but the
  versions still show no previous version, the receiver's ObjectID counter does not advance
  (the client then created a second unit with the same ID 2327) and the host still crashed.
  Dropped.

### Derived
- A merged object version keeps the sender's PreviousHistoryFrameIndex. The receiver applies
  the merge into its current frame, and its frame count differs (merges add no frames; the
  player without the baton makes local frames), so the index points at an unrelated frame
  (lookup returns none or a wrong version) or past the end (empty slot: crash). This also
  explains the client crashes in the save writer ("FWriteHistoryObjects"), which walks the
  same links.

### Fix
- Harness `@walkback N`: looks up the HQ and crew as of each of the last N frames (what the
  Avenger map item does).
- First try: before each merge the sender sends "CoopAlign <its current frame>" and the
  receiver submits empty frames ("Coop: align history") until that is its current frame, so
  the merge lands in the sender's frame number. No crash in 3 runs, but both instances hung
  (log stops; once in the receiver's `@walkback`, once 0.02 s after a merge): a version taken
  from the sender's current frame names that frame as its previous one, and landing in the
  same frame makes it name itself (derived: an endless walk).
- Now: the sender sends its current frame + 1, the merge lands right after every frame its
  versions can name, so a named frame is always earlier than the version's own frame and
  never past the end. Commands are handled before a merge sent after them ("CoopAlign" logs
  before "received merge state" in every run). `baton-objects` 6 of 6 PASS on 2026-10-25,
  `@walkback 20` answered on both sides (found=529 and 543 lookups).
- Not known: what a lookup returns when the named frame holds no version of the object (one
  observation: GetPreviousVersion gave none), and whether saves written after merges load
  back the same campaign (to test).

## Session 3 — host saves after merges, and version chains

Harness: `@save` (new slot, logs its number), `@load N` (by save ID, like UILoadGame),
`@verchain` (follows GetPreviousVersion from the current version of the HQ and every crew
member and counts the frames that hold a version; "short" = the chain ends early, "bad" = a
link that does not go back in time), `@localburst N` (frames that are not batched),
`@sendvar K` (four ways of sending the HQ as a merge state).

### Observed
1. Pair run: B (client) took the baton and sent two batches (Engineer + 9 alloys, then 10
   frames); A (host) was "in sync" after each and saved to slot 61 (`save done ok=true`).
2. One instance, load slot 61: fingerprint after loading = the one at save time
   (sup4010, all1009, crew27#2321), crew equal (the client's Scientist 2320 and Engineer
   2321 present, duplicateIDs=0). Frames 242 -> 250 after loading. Then Geoscape, scan
   (clock to 2035-3-6, history = ruleset = Geoscape), 3 frames, save to slot 62: ok, no crash.
3. `@verchain`: without network (new campaign, bursts, pending states created and discarded)
   every chain is complete (176/176, 181/181). Over the network the HQ chain was cut on BOTH
   sides, also on the sender, right after the sender's own batch went out (A: 176/176 before
   its send, HQ chain=1 after it, ending at its own last local frame).
4. `@sendvar` on the host (each followed by one unsent local frame and `@verchain`):
   pending state discarded without sending: chain intact; pending state sent and not
   discarded: cut; pending state sent and discarded (FlushBatch): cut; a submitted frame
   sent with SendMergeGameState: intact.
5. In that experiment no "CoopAlign" was sent; B then crashed in `@verchain` at the known
   address (XCom2.exe+0x4ce235, null+0xe0): a link past the end of its history.
6. Fix: FlushBatch submits the batch as its own frame ("Coop batch", not batched again) and
   sends that frame; "CoopAlign" carries its index, so the merge lands in the same frame
   number on the receiver. Rerun: sender chains complete after every send (A 179/179; B's
   own versions chain back across its batches, 4 then 7 links); the chain now ends only at a
   version received from the partner (it names a padding frame "Coop: align history" with no
   version of the object). `@walkback 20` answered, host save after merges ok (slot 63).

### Derived
- SendMergeGameState of a state that is not in the history clears the previous-version link
  of the newest history version of each object in it, on the sender. Native; avoided.
- A received version's previous-version lookup returns none (no crash) when the named frame
  holds no version; saves keep that and load back the same campaign.

### Unknown
- Whether game code needs the previous version of a received object (e.g. code comparing old
  and new state after a merge). Script that dereferences none logs "Accessed None"; not seen yet.

Full suite with the new FlushBatch and `@verchain` in baton-objects (no "bad=" allowed):
9/9 PASS on 2026-10-25 20:43.

Bench note: running `bash tools/build_mod.sh` from a PowerShell background job did not build
(no error shown); build from Git Bash and check the `.u` timestamp.

## Session 3 (cont.) — projects finishing on the partner's Geoscape

Harness: `@construct N Template` (XComHQ.AddFacilityProject, what UIChooseFacility does on
confirm: build time and cost), `@dismiss` (closes popups down to the Geoscape / Avenger base
screens), `@geoscape` now only enters when the map is not open; `@time` lists every HQ project
(class, focus, completion date, points) and the project / tech / facility counts; `@rooms`
marks a room under construction.

### Observed
1. Test "projects": A (host, baton) clears room 3, starts a Laboratory (done in 20 days) and
   AlienBiotech research (4.5 days), then grants the baton. B enters the Geoscape and scans.
   - The project objects, the room's build state and the cost reach B (room 3 swap
     AVG_ER_A -> AVG_ER_BuildState_A on B; supplies 4000 -> 3850 on both).
   - The research completed on B during the scan; A received it (techs 21, project removed),
     "in sync".
2. First run: the facility never completed (time passed its date by a month, points
   unchanged). Cause: the test's `@close` closed B's Geoscape screen, then its HUD; time still
   advanced through the Geoscape actor. XComGameState_HeadquartersXCom checks projects only
   while `Pres.StrategyMap2D != none`, not in flight and with no UIAlert on the stack.
3. With `@dismiss` and 12 scan rounds (third run): the Laboratory completed on B between
   2035-3-23 and 3-28 (its date was 3-21; until then a UIAlert or the alien facility mission
   screen was open at almost every check). A swapped room 3 AVG_ER_BuildState_A ->
   AVG_Laboratory_A; both end with projects=0 techs=21 facilities=19 and equal fingerprints,
   35 "in sync", no DESYNC.
4. Popups (research done, facility done, alien facility mission, other alerts) appear only on
   the player holding the Geoscape; the partner sees the result through the merges.
5. During a scan the history clock (XComGameState_GameTime) lags the ruleset / Geoscape clock
   and catches up at events and on pause; the partner's clock follows the history copy.

### Derived
- Projects started by one player complete on the other's Geoscape; the result and the room
  swap reach the starter. No mod change needed for this.

### Unknown
- Whether a facility completes two days late in single player too when popups are left open
  (not compared).
- The partner gets no popup for events that happened on the other side (design question:
  show a summary?).

## Session 3 (cont.) — a lost merge, one merge in flight

### Observed
1. Suite 2026-10-25 21:51: 7/10. `projects` failed with DESYNC on A: B sent two batches 0.23 s
   apart (B log 219.49 and 219.72); A handled both "CoopAlign" commands (frames 281 and 283)
   but got ONE merge callback; the first batch (1 object) was lost. A's queue of "CoopFrame"
   fingerprints was then one behind, so every later comparison said DESYNC. The final
   fingerprints matched only because the lost object was overwritten later.
2. `edit-gate` and `panel-replies` failed on timing: after the mirror map load B had a loading
   movie (12-15 s) and GPU stalls ("Timed out while waiting for GPU"); its step due 15 s later
   ran 46-57 s later, so the two instances' steps came in the wrong order. The mod behaved
   correctly for the order that happened.

### Fix
- CoopSession: one merge state in flight. The receiver answers every merge with
  "CoopMergeAck"; frames made meanwhile wait in the next batch; a merge without an ack after
  5 s is sent again (its objects re-added). The fingerprint travels in "CoopAlign" and is
  compared once after the next merge ("CoopFrame" and its queue are gone), so one lost merge
  cannot shift later checks.
- Harness: `@waitnobaton`, `@waitgrantreq`; `edit-gate` and `panel-replies` wait on these
  conditions (A holds the Geoscape until B's request is deferred) instead of fixed delays.
- Suite 2026-10-25 22:33: 10/10 PASS (with the new `projects` scenario). No "not acknowledged"
  resend in the last scenario's logs (the other scenarios' logs were overwritten; unknown).

## Session 3 (cont.) — client drops and rejoins

Tools: mod repo `tools/run_rejoin_test.ps1` (A and B start as usual; when B logs "done" its
process is killed, no clean disconnect; then a new B process starts alone with other steps:
A no longer holds the single-instance mutex). Harness: `@waitjoins N` (N-th PlayerJoined),
`@waitclosed N` (N-th ConnectionClosed). The suite runs a scenario with `B2` steps this way.

### Observed
1. Test "rejoin-explore" (before the fix): A granted the baton, B made an Engineer (merged,
   "in sync") and was killed holding the baton. A logged ConnectionClosed about 3 s later.
   A then had no baton (panel "partner has control"); nobody could change the campaign.
2. The new B process connected, A sent the history again (`MPSendHistory`), B entered the
   Avenger as mirror with 234 frames: the Engineer it made before dying was there, crew
   equal, duplicateIDs=0.
3. A's change after the rejoin (+5 supplies) was "local frame without baton" and never
   sent: A ended with 4005 supplies, B with 4000.
4. Fix: CoopSession.OnConnectionClosed drops everything in flight (grant pending, merge in
   flight, batch) and the host takes the baton back. The side that entered the campaign as
   mirror (`bMirrorClient`, set by CoopHeadquartersGame) is the client: it only logs that
   the host left and holds no baton.
5. Test "rejoin-1" / suite "rejoin": A logs "connection closed: the partner left; the host
   takes the baton back (had it: no)", panel "you have control"; after B's return A's +5
   supplies reaches B ("in sync"); A grants (epoch 2) to the new B process, B's +3 alloys
   reaches A ("in sync"); both end with 236 frames, equal fingerprints, duplicateIDs=0,
   verchain bad=0.

### Unknown
- The host dying while the client plays (only the log line exists on the client).
- Joining is still driven by the test bot (host `MPSendHistory`, client `@mirror`); real play
  needs a join flow (the host sends the history when a client joins, a menu entry).
- Frames the client made after its last batch and before dying are lost (by design: the
  host's history is the campaign).
- Full suite with `rejoin` 2026-10-25 23:35: 11/11 PASS.

## Session 4 — 2026-11-11: design research for the full campaign (second agent)

A second agent (another owner, same build 8270065) researched the parts the prototype
does not cover yet: unit ownership, squad slot split, per-player tactical control, mod
matching, appearance and voices. Nothing built or run. Write-up:
[DESIGN-campaign-coop.md](DESIGN-campaign-coop.md).

### Observed
- Correction to Session 1: Team Dragonpunk's source IS public:
  https://github.com/quixotic-cloud/Tactical-Co-Op (GPL-3.0 per GitHub, created
  2016-07-22, last push 2017-03-30, vanilla XCOM 2) and
  https://github.com/quixotic-cloud/Long-War-2-Coop (one commit 2017-03-30, no license).
  Found by GitHub code search for `XComCoOp`, not by repo-name search. Its `Src/XComGame/`
  folder (about 2,010 files) is the Firaxis SDK source copied by the old ModBuddy
  template, so don't reuse that part. Its own classes use `XComGameStateNetworkManager`
  (`SendHistory`, `SendMergeGameState`, `SendRemoteCommand`) and subclass
  `X2TacticalGameRuleset` (about 4,000 lines).
- WOTC exe `XCom2-WarOfTheChosen/Binaries/Win64/XCom2.exe` SHA-256
  `d830aad0bdc5aea35e3a62f51cbd7f0c22b4d297ac3c02cdea37a746fde110ae` (32,006,600 bytes,
  `Version.txt` changelist 699445); SDK `Binaries/Win64/XComGame.exe` SHA-256
  `336338a2477c5cf0b9858e5214ac0b40d4ad42670619af499b74a89c32e4c7d9`.
- WOTC tactical turn order holds XComGameState_AIGroup entries ordered by
  `InitiativePriority`, and only the group with initiative can act; all XCom units start
  in one group. Details and line references in the design doc.

### Derived
- Splitting the XCom group in two by owner gives alternating per-player control with the
  game's own initiative code (to be tested).
- Mod parity needs a helper outside the game (no Workshop API in UnrealScript).

## Session 4 (cont.) — 2026-11-12: two copies on one PC, mission handoff runs

Game: Steam WOTC build 8270065, exe SHA-256 as above. Two test copies on one PC (127.0.0.1),
mod clone of EvilMask587/xcom2-wotc-coop-campaign at cb1828b plus local changes (not pushed).
Full attempt log with log evidence: [DESIGN-campaign-coop.md](DESIGN-campaign-coop.md),
"Mission scenario: blockers and attempts".

### Observed (test runs)
- With a network server open, `CreateTacticalGame` waits forever on the `CIN_XP_UI_LogoSpin.bk2`
  map-change movie: the engine does not report it as a loading movie, so `HideLoadingScreen` never
  stops it. Stopping it once the map is generated and streamed lets the mission start.
- The strategy batch sync (`SendMergeGameState`) must be off in missions: a merge sent while the host
  generated the mission map crashed it ("Object is not packaged: XComGameState_EnvironmentDamage None").
- A guest that receives a running mission's full history while on the Avenger builds tactical
  visualizers there ("Accessed None 'PRES'"), and once crashed. `EnableBuildVisualization(false)` on that
  map's visualization manager, from the receive delegate, stops it.
- A guest that joins a mission through `LoadTacticalGame` keeps the base ruleset's `bLoadingSavedGame`
  (`privatewrite`) set until the base `TurnPhase_UnitActions.BeginState` load path runs. While it is set,
  no turn-begin or turn-end frames are submitted.
- `BattleData.UnitActionInitiativeRef` is written for groups (`UnitGroupTurnBegin`) and players
  (`PlayerTurnBegin`) only by the machine that begins that initiative. Handing over "the initiative in
  BattleData" was wrong. The switch command now names the initiative id, its index and the interrupt
  flags, and the receiver begins it.
- Result: scenario `mission` 22/22, twice. Afterwards: `squad` 13/13, `exp12` 11/11, `join` 7/7,
  `joinmismatch` 5/5. Install and saves unchanged after every run.

### Unknown
- An alien turn with real AI actions handed back to a guest: the test mission (Guerrilla Op added with
  `SpawnMission` in a fresh debug campaign) had no enemies ("units ... alien=0").
- Whether the first guest hang (log lost) was the visualizer race or the strategy batch.

## Session 4 (cont.) — 2026-11-12 afternoon: enemies, reconnect, disconnect, three copies

### Observed (test runs, same build and PC as above)
- `XComEngine.IsSinglePlayerGame()` is false whenever `XComGameStateNetworkManager` has a server or
  connection open (`NetMode` stays `NM_Standalone`). With a server open, the mission start skips alien
  spawning (`SetupUnits`), mission objectives (`XComParcelManager`), the drop-ship and the intro. Closing
  it before launch gave 6 aliens on the same kind of mission (scenario `netprobe`).
- The host now closes the connection in `X2DownloadableContentInfo.OnPreMission` and reopens it at its
  first unit actions. The mission had aliens and the Lost, and the alien AI took its turn.
- A client whose host closes the connection is sent back to the main menu by the engine
  ("IpNetDriverSteamworks_0 shut down", "Failed; returning to Entry"). It can reconnect from there and
  load the host's mission. A load from the main menu needs the Firaxis Live login delegates cleared
  first (GC assert UnGame.cpp:3107 otherwise).
- The join request URL seen by the host (`PlayerJoined`) carries only the player's name, not URL
  options.
- Results: `mission` 30/30 (two rounds, enemies), `dropguest` 15/15 (guest killed in its turn, host
  takes over, guest restarts and rejoins), `join3` 12/12 (three copies, slots 1 and 2, same campaign
  fingerprint everywhere).

### Unknown
- A mission with three or four copies (the guest-frame relay and guest-to-guest switch code is built,
  not run).
- Steam invites (needs two Steam accounts on two PCs).

## Session 4 (cont.) — 2026-11-12 night: mission end and three players

### Observed (test runs)
- Mission end with a guest (`missionend` 11/11): the host closes at `EndTacticalGame`, reopens after
  `OnPostMission`, and sends the campaign. The guest enters the Avenger, and both end with the same
  fingerprint. Problems found on the way:
  - a guest inside a mission keeps its client net driver when the host closes;
  - a campaign received on the mission map crashed the guest;
  - the PvP replay manager's `ReceiveHistory` delegate kept the mission World alive (GC assert);
  - `UIDropShipBriefing_MissionEnd` waits for a key.
- Three players in one mission (`mission3` 24/24):
  - a broadcast slot assignment gave two guests that reconnected together the same slot, so slots are now
    agreed by a nonce (`CoopHello`/`CoopSlotFor`), and closed connections are found by a roll call;
  - the host's relayed partial history fails for one guest, and the engine resends the whole history
    instead (`OnSendError_RequestNewHistory`, about 400 KB), which must be acknowledged as the partial.
- A failed join attempt reloads the main menu; with the Firaxis Live login screen open that asserted.
- Once, a guest hung while loading the main menu (no crash record); not seen again in about 10 runs.

### Reach goal
- After the campaign co-op works end to end: add a second co-op mode. Current idea, not final: horde
  defense (waves of enemies).
  Tracked in [DESIGN-campaign-coop.md](DESIGN-campaign-coop.md), "Reach goal".

## Session 4 (cont.) — 2026-11-12/13: saves, second mission, buttons, rules, relay

### Observed (test runs, one final build, all 11 scenarios pass)
- Host saves on the Avenger, quits, restarts, loads and hosts again; the guest rejoins by itself and the
  soldier owners survive the reload; a second mission with a handover follows (`saveload` 17/17).
- "Host co-op" on the Avenger HUD and "Join co-op" with an address dialog on the main menu (`buttons` 14/14).
  Bug found: `SplitString(...).Length` read directly from the call's result was always 0.
- With the connection open, missions take the multiplayer branch for flank crit, suppression, Shadowbind and
  the pause menu. Swapped for single-player values (`mission` 36/36 shows the co-op classes in place and the
  pause menu pausing). `ModClassOverrides` did not reach the standard-aim objects built in X2Ability's
  `defaultproperties`; they are replaced in `OnPostTemplatesCreated` (13 abilities, 4 objects).
- Relay to a third player: a partial history carries the size and hash of its base; the host's base after
  guest 1's turn is a frame guest 2 lacks, so a relayed partial can never apply. The relay is now a full
  `SendHistory` (one send, no error round trip; `mission3` 27/27, no base mismatch logged).
- The script compiler crashed with no message on `(new class'X').Class.Name` inline and on `foreach` over an
  array reached through a function result; with locals it compiled.
- The real install and the saves were unchanged after every run.

### Unknown
- Flank crit in an actual shot with the connection open.
- Ironman; a guest restarting between sessions; loading a mid-mission save; four copies; Steam invites.

## Session 4 (cont.) — 2026-11-13: untested features, saves, flags

### Observed (test runs)
- `features` 20/20 (two copies, Avenger):
  - the host's Armory "Owner" button gives a soldier to Host, then to Player 2; the guest sees the new owner;
  - squad select's "Slot N" / "Player" buttons assign slot 2 to a player;
  - guest rights (now on by default): the guest opens its own soldier (baton granted), cannot give it away,
    and has another player's soldier and the research screen closed;
  - "Chat" button both ways.
- Bugs found: the Avenger HUD's "Chat" button was added only when the HUD opened, so the host (who presses
  "Host co-op" later) never had one. The status panel now adds it once a connection is open.
- Test lesson: `@spawnmission` opens the Geoscape (an edit screen). While the host keeps it open, a guest's
  baton request waits, as designed. The bot now closes screens by class (`@closescreen`).
- `mission` 40/40: both players pan the camera with the keyboard while watching the other's turn (input state
  `Multiplayer_Inactive`, about 2,660 units in 2 s); pings and chat both ways inside the mission.
- Debug "red screens" (`UIRedScreen`) appear in the test copies in every scenario (the bot dismisses them).
  A screenshot of one in a mission listed the game's own map warnings ("Unable to determine destructible actor
  dimensions for Plot_TUN_Sewer_Guerilla_02...") and a sound package load, nothing from this mod.
- `mission` 48/48 with unit flag owner tags (`CoopFlags: flag unit=N owner=1 label=P2` on both copies) and
  turn banners ("YOUR TURN" on the player's own machine, "PLAYER 2'S TURN" / "HOST'S TURN" on the other).
  Screenshots taken with the console command `shot` land in `XComGame\ScreenShots\PCConsole\*.bmp` of the copy.
- `mission4` 33/33: four copies, one soldier each, three guest turns in a row relayed through the host, back
  to the host's group after the aliens.
- `guestrestart` 19/19: both guests quit their game on the Avenger and start again, the second guest first;
  each asks for its old slot (remembered in `XComCoopGuest.ini`) and gets it and its soldiers back.
- `midsave` 16/16 and `ironman` 18/18: the host saves inside a mission (pause menu offers "Save"; Ironman writes
  its autosave at the start of the XCom turn), quits, loads it and hosts again by itself (new: no button exists
  inside a mission); the guest, sent to the main menu, rejoins the mission and both play on.
- Found by the screenshots, not the logs:
  - the guest showed the game's PvP notice "Opponent left - connection to the session has been lost" during
    its own turn. It was queued when the host closed the connection for the mission build. Now dropped from
    the queue (and closed if already up) once a game has hosted or joined (`CoopLink.SweepNotices`).
  - the bot's `@dismiss` inside a mission closes the tactical HUD, the turn overlay and the unit flag
    manager along with the red screen; screenshots now close only the red screen.
- Test lesson: the game's save list comes from a cached index in `profile.bin`, so moving old save files
  aside does not shorten it; `@loadnewest tactical` picks the newest save made inside a mission.
- Full regression on that build (16 scenarios): 14 passed. Two failures:
  - `guestrestart` 17/19, a real bug: the host sent the campaign to rejoining player 2 while it still counted
    player 3, whose connection had closed a second earlier (player 3's script quit). It waited for 2 acks,
    got 1, and never sent "enter the Avenger"; player 2 sat on the main menu. Fix: the host now waits for acks
    by slot (`CoopMissionHistoryAck` carries the guest's slot) and stops waiting for a slot that disconnects
    (`CoopTacticalNet.OnGuestGone`), for the campaign, the mission history and the host's relay of a
    guest's frames during a mission (`CoopHistoryAck` carries the slot too; a guest-to-guest turn switch that
    waits for the relay is forwarded once the last guest still missing it has acked or left).
  - `midsave` 12/16: the guest crashed 1.3 s after reaching the main menu with an engine assert, "Failed to
    find function None in XComMCP" (UnObj.cpp:2456). It passed on rerun, then the next full regression hit
    it twice (`saveload`, `midsave`), always on a guest leaving a mission for the main menu after the host
    closed the connection. Cause, from the SDK source: the dropped connection runs the game's PvP
    `XComTacticalController.LostConnection`, which queues the "Opponent left" notice and starts an
    asynchronous `MCPManager.PingMCP` with a completion delegate on the tactical controller. The guest
    leaves the map at once; when the ping completes on the main menu, its delegate's object is gone. A race.
    Fix: `CoopTacticalController` skips `LostConnection` (global and in state `PlayerWaiting`) while co-op
    is active; co-op reconnects by itself. This also stops the "Opponent left" notice at its source.
- The turn banner never showed on screen: four screenshots 1 to 2.3 s after "YOUR TURN" show the teal
  turn glow and the owner tags, but no text. The game's code says why: `XComPresentationLayer.UIEndTurn`
  shows "no banner for the start of the XCom turn", and `m_sXComTurn` is the Resistance banner. The banner
  is now the mod's own box (`CoopTurnBannerPanel`) on the turn overlay screen, in the player's flag color,
  for 3.5 s. Seen on a screenshot of the guest 1 s after its turn began: "YOUR TURN" in player 2's green,
  below the turn order icons.
- Final runs, two builds (the second adds only the `LostConnection` skip, which is in the tactical
  controller):
  - build 1, Avenger only: `squad` 13/13, `exp12` 11/11, `join` 7/7, `joinmismatch` 5/5, `join3` 12/12,
    `buttons` 14/14, `features` 20/20, `guestrestart` 19/19;
  - build 2, every scenario that enters a mission: `mission` 53/53, `dropguest` 15/15, `missionend` 11/11,
    `mission3` 27/27, `mission4` 33/33, `ironman` 18/18, `saveload` 18/18 twice, `midsave` 16/16 three times.
  - On build 2 the skip line ("co-op: skipped the game's multiplayer lost-connection handling") appears in
    9 of 11 runs; the two without it (`mission`, `dropguest`) have no running guest that loses the host during a mission (in `dropguest` the guest quits its game).
    No run logged `XComMCP` or "Crash Detected". The real install was unchanged and the saves matched the
    backup after every run.

### Unknown
- A guest leaving while a relay is in flight (the case the relay fix covers) has no test scenario yet.

## Session 4 (cont.) — 2026-11-13 evening: game content tests

Goal: run content that is likely to break turns or soldier ownership, on two copies, with new bot steps
(`CoopContentTests`: `@useability SRC ABILITY TGT` as the game's `X2UseAbility` cheat does, `@mcstate`,
`@removemc`, `@trysel SPEC`, `@dropunit TEMPLATE TEAM`, `@captured`, `@mark`; `@spawnmission SOURCE REWARD`
and `@launch SOURCE` in `CoopExperiments`).

### Observed (test runs)
- `mindcontrol`, first run (24 checks, 11 failed, all from the test itself; see below). The mission ran
  through three host turns and two guest turns without a crash or a handoff warning.
  - An alien used `Domination` on a guest soldier (host's turn): the soldier left the guest's group for the
    alien's group (`XComGameState_AIPlayerData.UpdateForMindControlledUnit` found the guest's group; the
    groups added by the mod's split are seen). In the guest's turn it had 0 action points.
  - The guest's soldier used `Domination` on an alien (guest's turn): the alien joined the guest's group,
    is run by the guest's machine, and had 2 action points in the guest's next turn.
  - Freeing the guest's soldier (host's turn, effect removed as a cleanse would) put it back into the
    guest's group (`m_arrDisplaced`), with 2 action points in the guest's next turn.
  - The freed soldier gets 2 action points at once, during the host's turn (`X2Effect_MindControl.OnEffectRemoved`
    gives them when the XCom player is active; all XCom groups share one player). The selection gate keeps
    the host from moving it.
- Test lessons: the log printed teams as numbers (`ETeam` is a sparse enum: XCom = 8, Alien = 16), so the
  checks never matched; now `GetEnum`. And `Visualizer_SelectUnit` does not check the unit's side in the game
  either (clicks and Tab only offer your own units), so "can select" is no test for a unit the other side
  controls; the checks use action points there.

- `mindcontrol` rerun with fixed checks: 24/24. `lostchosen` 21/21: two Lost and a Chosen Assassin dropped
  next to the squad (`DropUnit` cheat's `DropUnitAt`); the Chosen kidnapped a guest soldier (`ChosenKidnap`,
  forced); the guest played on with its other soldier; the Lost got their own turns (turn order host, guest,
  aliens, Lost, ...); the host ended the mission. On the Avenger both machines list the soldier as captured
  by the Assassin with owner tag 1, and both campaign fingerprints match.
- `retaliation` 14/14 (the game made it a `ChosenRetaliation`: 6 resistance fighters, 14 civilians, 9 aliens;
  their turns run on the host) and `blacksite` 14/14 (story mission `GP_Blacksite`, two rounds; no pod was
  active, so the alien turns were short; its objective cutscene was not reached).
- `mcall` (new, both guest soldiers mind-controlled), first run 14/16, a real hang: the guest's group was
  empty and the host skipped it as the game does, but in the next round the guest got the switch
  ("taking over initiative") and never started its turn. The guest's replay waits for an idle visualizer
  before it stops (`XComMPReplayMgr.PlayingReplay.Tick`), and an `X2Action_RevealAIBegin` there never ended
  (game redscreen: "detected a time out, which will likely result in a hang"). Fix: while a machine waits to
  take over, once the game marks a visualization action as timed out, `CoopTacticalRuleset` finishes it the
  way the game's debug command does (`XComGameStateVisualizationMgr.DebugClearHangs`) and logs it.
  - Test lesson: the bot's `@waitmyturn` took the host's run of the guest's empty group for the host's turn;
    it now ignores empty groups.
- Build with the fix: `mcall` 16/16 twice (the reveal did not hang in either run), `mindcontrol` 24/24,
  `lostchosen` 21/21, `mission` 53/53, `mission3` 27/27, `midsave` 16/16; real install unchanged after each.
  In the `mindcontrol` run the fix fired on the guest: "takeover waits on a timed-out visualization action:
  X2Action_RevealAIBegin after 36 s", then "finished 1 timed-out visualization action(s)", and the guest
  played its turn. So the reveal hang is not tied to mind control; it is intermittent.
- `dropguest` failed once (9 of 15): the guest's log stops at 113 s, while it loads the main-menu map
  after the host closed the connection to build the mission (same lines as passing runs up to
  `LoadMap: XPACK_Shell_Intro`, then nothing; no crash line). That is before any of this session's changes
  run. Two reruns: 15/15 each. The first `avengerdefense` run froze the same way (4 of 15 failed).
  Across all kept logs: 5 freezes in 844 such main-menu loads, every one right after the host closed
  the connection (mission build 4 times, host quit once; `missionend -180015`, `features -063502`,
  `ironman -085005`, `dropguest -213040`, `avengerdefense -220011`). No crash dump in the copy's Logs.
  Guess (not checked): an engine assert dialog waiting for a click, such as the "World ... not cleaned up by
  garbage collection" assert of item 4 in the first session. The runner now records a copy's windows and a
  screenshot when its log stops for 30 s after a main-menu load (`window_titles.py`); it has not caught one yet.
- `avengerdefense` (new: `@avengerdefense` makes a UFO with the `SpawnUFO` cheat's code and calls its
  `CreateAvengerDefenseMission` in the Avenger's region; `@launch MissionSource_AvengerDefense` then does
  what the defense's squad screen does on close, `SquadSelectionCompleted`): 15/15 three times in a row.
  Map `Plot_WLD_AvDef_LgObj_Stream`, 13 aliens; the guest played its two soldiers.
- Found in those runs: the defense adds XCom soldiers during the mission (`SeqAct_SpawnUnitFromAvenger`
  picks any free soldier in the barracks and gives it to the XCom player); soldier 435 arrived at turn 2 in
  the host's group. Two more XCom units (owner none) start in groups of their own that the game empties into
  the host's group; what they are is unknown. A soldier a guest owns would have stayed in the host's group,
  and `ControllerOfInitiative` hands a group to the first guest-owned member, so the host's whole group would
  have gone to that guest. Fix: on the host, at each XCom player turn begin, `CoopTactical.RegroupOwnedUnits`
  moves every soldier a guest owns into that guest's group (priority = lowest XCom group + slot, as the start
  split makes it; a new group if the guest has none). New test `avdefreinf` (`@tagbarracks 1` gives the
  barracks to the guest first): 15/15. Reinforcements 433 (turn 2) and 439 (turn 4) moved to the guest's
  group in the turn they arrived, before the host's group acted; the guest had 433 with 2 action points
  in its next turn; the host's group never went to the guest.
- Regression batch on that build (9 scenarios): `mission` 53/53, `mission3` 27/27, `mindcontrol` 24/24,
  `mcall` 16/16, `lostchosen` 21/21, `dropguest` 15/15, `midsave` 16/16; `avengerdefense` 11/15 and
  `avdefreinf` 10/15, both with the guest's log ending right after the main-menu load. The runner's stall
  check did not fire for them, because it skips a copy whose process has exited: the guest had died.

### The guest "freeze" is a crash in the game's Wwise callback (2026-11-14)

Observed:
- Windows Error Reporting keeps minidumps in `%LOCALAPPDATA%\CrashDumps` when the `LocalDumps`
  registry key exists (other installed software often creates it; Windows keeps the newest 10, so older
  crashes are gone). Ten `XCom2.exe` dumps from
  2026-11-12/13, all from test copy 1 (the guest in two-player runs; the exe path in each dump's module list).
  Four are the old "opponent left" crash (fault `+0x36569a`, before that fix). The six others, at the times of
  the "freezes", are one crash: access violation reading address `0x90` at `XCom2.exe+0x6d0695`.
- Read with `local/scripts/minidump_info.py` (exception record, registers, stack scan; no debugger installed)
  and `llvm-objdump` on the test copy's exe. `+0x6d0690` is `f(this, x) { if (!x) x = this->[0x90]; g(x) }`,
  called with this = a global that is null. The caller (`+0xa7d5b0`) runs only for callback type 8 and
  references the names `Engine`, `AkAudio`, `InterpTrackInstAkEvent`; the crashing thread's start routine
  references `AK::EventManager`. So it is Wwise's event thread delivering a callback to the game's handler for
  matinee sound tracks. (`local/scripts/pe_strrefs.py` prints the strings code near an address refers to.)
- In the runlogs (`local/scripts/avenger_exit_times.py`): 7 of 269 guest departures from the Avenger ended
  the log (2.6%), at 42-46 s and once 185 s after the Avenger load. Four of them came 46.2-46.5 s after it, where
  4 of about 16 departures crashed; 0 of 58 at 45.x s. Departures from other maps: none ended the log.
- The guest leaves the Avenger in the same frame the host's close arrives: the engine reports
  `PMT_ConnectionFailure`, `XComGameViewportClient.NotifyConnectionError` calls
  `XComPlayerController.NotifyConnectionError`, which goes to `OnDestroyedOnlineGame` ->
  `XComOnlineEventMgr.ReturnToMPMainMenu` -> `DisconnectAll` (`EngineLevelDisconnect`: "Failed; returning to
  Entry") -> `LoadMap: XPACK_Shell_Intro?closed`.
- Ruled out on the way: the 2K online client. Its presence heartbeat goes out every 30 s from login
  (`ctpHttp.log`), and one was in flight at one of the crashes, but the crashing thread is Wwise's. That
  log holds a login token and location data; it stays out of notes and PRs.

Derived:
- A sound an Avenger matinee posted in the last frames before the unload calls back after the map unload has
  nulled the world, and the handler dereferences it.

Guess (not checked): callback type 8 is `AK_Duration` (flag value recalled from Wwise's `AkCallback.h`, which
is not at hand; Audiokinetic describes AK_Duration as sent once the sound engine knows the sound's duration,
https://www.audiokinetic.com/qa/1336/is-it-possible-to-know-when-a-sound-has-finished-playing). The fix does
not depend on which callback it is.

Derived:
- It is a race that depends on what the Avenger's matinees are doing at that moment, which is why one
  scripted timing (46 s) hit it often.

Fix (`CoopHeadquartersController`, ModClassOverrides on `XComHeadquartersController`; `CoopLink.StartQuietLeave`):
- On a co-op guest whose host connection just closed, `NotifyConnectionError` on the Avenger skips the game's
  handling and sets `bIgnoreNetworkMessages` (the engine repeats the error every frame otherwise; seen 35 times
  in the first test). CoopLink drops the dead connection, pauses the world (`WorldInfo.Pauser`: no Kismet or
  matinee updates, so nothing new is posted), and `QuietLeaveSeconds` (1.0) later calls the same
  `ReturnToMPMainMenu(QuitReason_OpponentDisconnected)`. `CoopLinkTimer` has `bAlwaysTick` so it fires while
  paused. A guest leaving a mission map takes the same pause before its `disconnect`.
- First run on the fix build: `avengerdefense` 15/15; the guest logged "world paused=True", left 1.0 s later
  and rejoined the mission.

### Unknown
- Mind control by the Chosen or by an Avatar has not run. The AI reveal's root cause on the guest (why it
  timed out) is unknown; the fix only unblocks it.
- Whether the host's own departure from the Avenger (the game's launch flow) can hit the same Wwise race: no
  host crash in about 270 departures. Whether vanilla single player can: not checked.
- What exactly the handler at `+0xa7d5b0` reads from the world, and the symbol of the null global: not
  decompiled (no symbols); "world" is derived from the null-during-unload behaviour.
- Other ways a soldier joins mid-mission (a rescued soldier, the Chosen stronghold) have not run; the regroup
  covers any XCom soldier with a guest's owner tag, by its code.
- A Blacksite objective cutscene was not reached by the bot (it plays two rounds only).

### Regression on the crash fix, and new content tests (2026-11-14)

Regression batch on the quiet-leave build (19 runs, one copy of each scenario unless noted): `avengerdefense`
16/16 three times, `avdefreinf` 16/16 twice, `mission3` 27/27, `mission4` 33/33, `dropguest` 15/15, `midsave`
16/16, `saveload` 18/18, `ironman` 18/18, `missionend` 11/11, `guestrestart` 19/19, `mindcontrol` 24/24,
`mcall` 16/16, `lostchosen` 21/21, `retaliation` 14/14, `blacksite` 14/14. `mission` failed 1 of 53: it
expected the guest's "opponent left" notice, which the guest no longer gets (it leaves quietly). New
expectation: the guest logs "leaving the Avenger now" and never the notice; then 54/54. No crash dump in any
run (the runner now fails a run on a new `XCom2.exe.*.dmp` in `%LOCALAPPDATA%\CrashDumps`). Real install
unchanged after every run.

New scenarios (new bot commands `@owned`, `@shotinfo`, `@stronghold`, `@chosenassault`):

Observed:
- `flankcrit` 10/10 (map `Plot_ABN_Guerilla_ElTrain_01`): the shot breakdown of a host soldier and of a guest
  soldier on a flanked trooper uses `CoopToHitCalc_StandardAim` with the multiplayer flags on
  (`coopmp=true mpgame=true`); the flanking crit bonus is 40 = the shooter's `eStat_FlankingCritChance`, not
  the multiplayer constant 30.
- `bigsquad` 13/13 (`Plot_TUN_Sewer_Guerilla_02`): 12 soldiers on the mission (the bot fills the HQ squad past
  squad select), 6 per player, two rounds played.
- `stronghold` 13/13 (`Plot_CSH_Stronghold_Short_03`, the Hunter's stronghold from the HQ cheat) and
  `chosenassault` 13/13 (`Plot_WLD_ChosenDef_Stream_01`, the Assassin's attack on the Avenger): three rounds
  each, guest turns and selection work.
- `finalmission` failed: `@launch MissionSource_Final` found no mission, since the bot only launched missions
  with a Skyranger flight (`MissionSource_Final` has `bRequiresSkyRangerTravel = false`; the Avenger flies
  there). Fixed in the bot; rerun pending.
- `avatarmc` failed 2 of 17: the Avatar (`AdvPsiWitchM3`) mind-controls a guest's soldier, the soldier joins
  the aliens (`owned 1 xcom=1 alien=1` on both machines), but it was still mind-controlled four player turns
  later. The game's countdown (`X2Effect_MindControl`): `MIND_CONTROL_AI_TURNS_DURATION=2`, +1 for the turn it
  is applied, ticking at `PlayerTurnBegun` of the effect's player. The earlier `mindcontrol` test freed the
  soldier with `@removemc`, so natural expiry had never run. `@mcstate` now logs the turns left and the
  ticking player; rerun pending.
- `relayleave` failed 2 of 16, both on the rejoin: the restarted guest never started. With three copies the
  last one started (C) still holds the single-instance mutex, and the runner's kill-and-restart path did not
  close it (the other restart path did). Runner fixed; rerun pending. The relay part passed: guest 1 died
  while the host relayed guest 2's turn, guest 2 resumed at index 3, the host took over guest 1's soldiers at
  index 4 and the round went on (turn 2).

Rerun on the fixed bot and runner: `finalmission` 13/13 (the Avenger flies to the fortress map; three rounds),
`relayleave` 16/16 (the restarted guest rejoined the running mission and got its soldiers back),
`avengerdefense` 16/16. The two unowned XCom units at the start of an Avenger defense are `XComTurretM1`
(the Avenger's defense turrets; the group dump now names unowned XCom units). `soak` was stopped partway by
the owner (PC restart); no result.

#### Effects lose their countdown at a handoff (2026-11-14)

Observed (`avatarmc` with `@mcstate` logging the effect's countdown and `IsRegistered` on its tick listener):
- Host, right after the Avatar's `PsiMindControl` on a guest's soldier: `mcleft=3 tickTeam=eTeam_XCom
  tickTurns=1 listening=true`.
- Host, its next turns (after the guest handed the turn back each time): `listening=false`, `mcleft=3` at
  `tickTurns` 2, 3 and 4. The guest: `listening=false` in every turn.
- The XCom player's own turn counter (`XComGameState_Player`, its 'PlayerTurnBegun' listener registered at
  mission start) kept counting on the host.

Derived: a listener that an object registers during the mission on one machine is missing after that machine
receives a partial history from the other. The game's mind control ticks on 'PlayerTurnBegun' of the
target's old player (`X2Effect_MindControl`, `WatchRule=eGameRule_PlayerTurnBegin`; `XComGameState_Effect.PostCreateInit`
registers it), so it never ends. Every timed effect applied in a co-op mission is likely hit the same way
(burning, poison, stun, disorient...), and so are effects' removal on the source's death.

Unknown: what the native partial-history receive does with the event manager (`X2TacticalMPGameRuleset`
only rebuilds a cache in script; the shipping build's `AllEventListenersToString` returns nothing). Whether
ability listeners and triggers of units that spawn mid-mission are also lost (now counted, not repaired).

Fix (`CoopListenerRepair`, run after each received partial history and when a machine takes over a turn):
each active effect whose first core listener is missing gets the listeners of `PostCreateInit` again (same
events, filters and order) and its template's `RegisterForEvents`. Ability listeners missing are counted in
the log, not repaired: some abilities unregister their own listener after it fires; the game's challenge-mode
cheat (`XComCheatManager`) shows the repair with `FixupRegisterListener` if it turns out to be needed.

First version of the repair (every machine, every effect) and what went wrong:
- The host kept the listener after it took the turn back (the guest had repaired it and sent it with its
  partial history; the host's repair found nothing missing). Derived: a machine that receives a partial
  history ends up with the sender's listeners; listeners the host registers mid-mission do not reach the guest.
- `avatarmc` still failed: the Avatar's `NullLance` killed two soldiers in the first alien turn (a new
  `UnitDied` log names the ability and its user), the squad hit heavy losses and the mission ended. Ordinary
  combat; the scenario now runs the game's `SkipAI` cheat on the host after the mind control.
- In one of three runs the guest crashed in native code as its first turn began (`XCom2.exe+0xb158a7`, no
  game crash report; minidump in `%LOCALAPPDATA%\CrashDumps`), right after it had registered 17 effects it
  never had. Not seen in any run before the repair. Cause not analysed further.

Second version (host only, effects with `WatchRule` = player turn begin or end only; the guest is untouched,
player turns always begin and end on the host): `avatarmc` 17/17 twice (with `SkipAI`), `mindcontrol` 24/24,
`mission` 54/54, no crash dump. The host repaired 2 to 4 effects after the guest's first turn (the mind
control, `Regeneration`, `RedAlert`). The countdown then ran: `mcleft` 3 at XCom turn 2, 2 at turn 3, and the
soldier was back with its owner at turn 4 on both machines; that matches the game's numbers (2 AI turns + 1,
ticks at XCom turns 2, 3 and 4). Why turn 2 still read 3: unknown (guess: logged before that tick's frame).

Still open:
- Effects that tick on a group's turn (`WatchRule` = unit group turn begin or end) of a guest's group, and
  effects ticking on every action of a guest's soldier: not repaired (they would have to be repaired on that
  guest). Which effects use those rules was not listed.
- 4 ability listeners are missing on both machines after every handoff (counted, not repaired); which ones
  and whether it matters: unknown.

#### The 4-mission soak hung the guest (2026-11-14)

Run `soak` (host and one guest, four missions in a row with Geoscape time between them) on the host-only
listener-repair build. It never finished: it was found stopped hours later, and the guest's
`Launch.log` was 23 GB.

Observed (host log `soak-20261114-142213-A-partial.log`, first 1700 lines of the guest log kept; both local):
- Mission 1 went through on both. The guest was back on the Avenger first; the host then played the movie
  `CIN_TP_WelcomeResistance` after the first mission's after-action. The host's log has no line for its
  98 s (`Movie Finished Event: CIN_TP_WelcomeResistance, 98.08`).
- The guest's connection timed out 60 s into it (`Connection timed out after 60.000000 seconds`). The guest
  left the Avenger for the main menu and reconnected, as designed, 38 s later.
- Host, in the same 0.1 s: the old connection closed (roll call sent), the new one was accepted, and the
  guest's `CoopHello` arrived. No `partner joined: player slot` line followed, and 2.7 s later the roll call
  ended with `guest slot 1 disconnected; the host controls its soldiers`. The campaign was never sent.
- The host's strategy batches still went out (`merge not acknowledged after 5 s` from then on). The guest,
  at the main menu, got `CoopAlign` and logged `Accessed None 'GameRuleset'` in `CoopSession.AlignHistory`
  157 million times, all with the same timestamp: one endless loop in one frame.
- The host launched missions 2 to 4 alone (`guests at launch: [] single=true`), finished all four and quit.
- No crash dump. The real install unchanged (checked after the restart).

Derived:
- `CoopJoin.HostOnHello` took the hello for a duplicate: the guest is the same process, so its nonce was
  known, and its old slot was still connected while the roll call ran. That branch answers the slot but
  skips `CoopLink.HostGuestJoined`, so the campaign never went out. The guest got its slot after the roll call
  had been sent, so it never answered it, and the roll call dropped the slot.
- `AlignHistory` adds empty frames with `GameRuleset.SubmitGameState` until the history reaches the target;
  with no ruleset (main menu) nothing is added and the loop never ends.
- The runner searched the whole log every 3 s, which takes minutes on a log that size, so the run's deadline
  was not checked in time, and it would then have read the whole log into memory. Guess: that is what slowed
  the PC earlier the same day.
- Any host movie longer than the 60 s connection timeout drops the guests the same way. This one plays after
  the first mission of every campaign.

Fixes:
- `CoopJoin`: a hello counts as a duplicate only if no connection closed since that guest's last hello.
  Otherwise it is a reconnect: the same slot, counted as present in a running roll call
  (`CoopRole.MarkPresent`), and `HostGuestJoined` sends the campaign or mission as for any join.
- `CoopSession.AlignHistory`: does nothing off the Avenger, and stops when a submit adds no frame.
- Runner: a copy whose log passes 200 MB is killed and the run fails ("runaway log"). Logs over the limit are
  kept as their first 20,000 and last 2,000 lines.
- Not changed: the connection timeout (the game's own 60 s). A longer timeout would also delay noticing a
  guest that really dropped. With the reconnect fixed, a guest that times out during a host movie comes back
  by itself, after a trip through the main menu.

Not covered by the soak: `EndBattle true` wins a mission without completing its objectives, so the
Blacksite (id 850) stayed on the map and missions 2 to 4 were that mission again.

#### Rerun on the reconnect fix (2026-11-15)

Results:
- `dropguest` 15/15, `relayleave` 16/16; real install unchanged after each.
- `soak` 9/13. Missions 1 and 2 went through with both players. The guest timed out during the host's movie
  after mission 1 again and came back by itself (`guest: reconnected, given player 1`); a second timeout,
  during the host's Geoscape scan before mission 3, used the new path on the host (`partner reconnected:
  player slot 1`, then `the campaign goes out in 3 s`). Mission 3 was a retaliation where a Chosen appeared
  for the first time: the host's game opened `UIChosenReveal` and paused itself until the player clicks it
  away (`X2Action_RevealAIBegin` waits for `UIChosenReveal_OnRemoved`, then unpauses). The bot never clicked,
  so the host sat there for the rest of the run. A test-bot gap, not a co-op fault.
- `stunguest` 11/13. A Stun Lancer stunned a guest's soldier during the host's turn (`stunned 1 count=1
  stunAP=1`). In guest turn 1 the soldier's turn setup used up the stun action point (`stunAP=0`), as in the
  game. The Stunned effect then stayed through guest turns 2 and 3 on both machines (`count=1`), and the
  soldier could still be selected. In the game, `X2Effect_Stunned`'s tick removes the effect at the start of
  the next group turn once `StunnedActionPoints` is 0, and that tick listens on `UnitGroupTurnBegun` of the
  soldier's group, whose turn runs on the guest. The guest does not have that listener (see the handoff
  listener section above).
- `mission3` 9/27: the host crashed 4 s into loading the mission map, at `@launch`. Minidump: access violation
  reading `0x90` at `XCom2.exe+0x6d0695` on a worker thread, the same Wwise matinee sound callback as the
  guest's Avenger exit before its fix (see "The guest freeze is a crash in the game's Wwise callback"). It
  answers that section's open question: the host's own departure can hit it too. First host crash of this
  kind in about 280 bot launches.

Derived: the bot launches with `PrepareTacticalBattle` + `ConfirmMission` straight from the Avenger, while
its room matinees play. A player launches after the Skyranger flight, from the Geoscape. Whether that path
can hit the race, in co-op or in single player: unknown.

Fixes (build of 2026-11-15 01:20):
- Bot: while waiting for its turn it closes a `UIChosenReveal`, as a player would (`CoopHarness.CloseChosenReveal`).
- Bot: `@launch` pauses the world and launches one second later, as the guest's exit does
  (`CoopExperiments.FinishLaunch`, `bAlwaysTick`). This covers the bot's launch only.
- `CoopListenerRepair`: every machine now also repairs effects that tick on the turn of a group that this
  machine runs (stun, daze on a guest's own soldiers; the host for alien groups and for a dropped guest's
  groups). The guest still repairs nothing else, so the broad guest-side repair that came before the earlier
  guest crash stays out.
- Runner stopped with `TaskStop` mid-scenario: the runner's config backups (`*.runpair-bak`) were left in two
  copies and were moved back by hand, as the runner does at its end.

#### Results on the build of 2026-11-15 01:20

Run one after another, real install unchanged after each, no crash dump:
- `mission3` 27/27 (the bot's launch now pauses a second first), `avatarmc` 17/17, `midsave` 16/16, `mission`
  54/54.
- `soak` 13/13: four missions with both players (a Guerrilla Op, then the Blacksite three times, since the
  bot's `EndBattle` wins without completing objectives). No Chosen appeared this time, so the bot's reveal
  close did not run: still untested.
- `stunguest` 12/13. The guest repaired the stun's tick just before taking its turn (`listener repair: effect
  2939 Stunned turnsLeft=1`); the stunned soldier started guest turn 1 with `ap=1` (the other guest soldier
  `ap=2`) and `stunned=false`, and the host showed `count=0` from its turn 2 on. That is the single-player
  behaviour. The failing check expected the guest to still see the stun 9 s into its turn 1, which was wrong:
  the tick removes it at that turn's start. The checks now look for the repair line and the lost action
  point. Rerun: `stunguest` 14/14.

#### Daze on a guest's soldier (2026-11-15)

Source read (SDK): `X2Effect_Dazed` extends `X2Effect_Stunned`; `CreateDazedStatusEffect` builds it infinite,
ticking on `eGameRule_UnitGroupTurnBegin`, with `StunLevel` = the daze turns (Mind Scorch: 4, from
`XComGameData_SoldierSkills.ini`). It is also `bCanTickEveryAction`, but that only applies when the target's
template has `bCanTickEffectsEveryAction`, which soldiers don't, so on a soldier it takes the group-turn path,
the one `CoopListenerRepair` now repairs on the machine that runs the group. Derived: it should cost two full
turns and end at the start of the third.

New bot output: `@stunned P` and `@mcstate` also print `dazed` (`IsDazed`; `IsStunned` is false for a daze).
New scenario `dazeguest`: the host drops a `ChosenWarlock` and forces `MindScorch` on a guest's soldier
(`contest:1`, the 1-4 band).

Result: 15/15, real install unchanged. Observed: host after the hit `stunAP=4 dazed=1`; the guest repaired
`effect 2965 Dazed turnsLeft=1` before its turn; guest turn 1 `ap=0 stunAP=2 dazed=true`, turn 2 `ap=0
stunAP=0 dazed=true`, turn 3 `ap=2 dazed=false`; the host agreed each turn. Dropping the Warlock with the
cheat opened no Chosen reveal screen. `stunguest` again 14/14 on the same build.

#### Story progress in the soak (2026-11-15)

`EndBattle` (the cheat) wins without completing objectives, so the old `soak` launched the same Blacksite
three times. New bot step `@winmission` (host): marks every objective of the active mission complete
(`XComGameState_BattleData.CompleteObjective`), submits it, then ends the battle as a win. New scenario
`soakstory` is `soak` with that step.

Result: `soakstory` 14/14, real install unchanged, no crash dump. Observed (host log): four different missions,
`winmission mission=Hack_Train objectives=2 strategy=1 allDone=true`, then `GP_Blacksite` (1 objective),
`Recover` and `SabotageTransmitter` (2 each), all `allDone=true`. The Blacksite was not offered again after it
was won. The guest rejoined each mission and Avenger return (`guests back [1] of [1]`), reconnected once after
the host's long movie (`partner reconnected: player slot 1`), and both ended with the same campaign
fingerprint (`samefp`).

#### Eight missions (`soakstory8`, 2026-11-15)

`soakstory` with four more mission blocks (generated from the mission-2 block), timeout 8400 s.

Result: 22/22, real install unchanged, no crash dump. Observed (host log): eight different missions,
NeutralizeFieldCommander, GP_Blacksite, Recover_ADV, ProtectDevice, a retaliation (`Terror`, 4 objectives),
DestroyRelay, Recover_Vehicle, SabotageTransmitter, all `allDone=true`. In the retaliation a Chosen appeared for
the first time, and the host bot closed `UIChosenReveal` while waiting for its turn (`closed UIChosenReveal (as
a player would)`), the first run of that fix. The guest reconnected twice after the host's long movies
(`partner reconnected: player slot 1`) and rejoined every mission and Avenger return.

Observed, slow return after the retaliation: the host's after-action showed `UIChosenMissionSummary`, which the
bot's `@afteraction` did not know, so the host stayed in the mission map for 200 s until a later strategy step
closed it. Meanwhile the guest, back at the main menu, tried to join every 64 s and timed out twice (60 s each,
no server: the host reopens it only on the Avenger), then got in 1 s after the server reopened. The runner's
stall check took a screenshot of the guest then (`soakstory8-stall-B-...png`, local). Derived: a bot gap, not a
co-op fault; a player clicks that screen away. Fix: `@afteraction` closes `UIChosenMissionSummary` (closing it
opens the regular `UIMissionSummary`, per `UIChosenMissionSummary.CloseScreen`). A Chosen dropped in with the
cheat (`lostchosen`) does not produce that screen, so the check is a rerun of `soakstory8`.

Rerun on the `@afteraction` fix: 22/22, real install unchanged, no crash dump. Eight missions (Hack_ADV,
GP_Blacksite, DestroyRelay, Recover_Train, a `Terror` retaliation, NeutralizeFieldCommander,
SabotageTransmitter, DestroyRelay). The Chosen appeared again in mission 5, a retaliation; the bot closed the
reveal and then `UIChosenMissionSummary` (`afteraction screen=UIChosenMissionSummary`, then
`UIMissionSummary`), and the host was back on the Avenger 89 s after the win (200 s in the first run). The
guest's first join attempt timed out while the host was still in the mission and the second got in 1 s after
the server reopened. Two `soakstory8` runs in a row put a Chosen retaliation at mission 5: guess, the game's
pacing of the first Chosen encounter, not chance.

#### Two-PC package and a rehearsal on one PC (2026-11-15)

Made for the first two-PC test (direct IP over a home network, one Steam account; Steam invites need two):
- Runner, host side: `-RemoteB` runs only the host copy and marks the guest's expectations PENDING; `-ExportB FILE
  -HostIP IP` writes the guest's bot steps with `127.0.0.1` replaced; `-CheckOnly -ALog -BLog` grades two kept
  logs (checked on the last `mission` logs: 54/54, as in the normal run). Scenarios that start, kill or restart
  copies are refused for now.
- Guest PC: one PowerShell script. `setup` checks the War of the Chosen exe's SHA-256 against the host's, makes
  a hard-linked test copy on the same NTFS drive (folders the game writes into are real copies, as in the
  gamepool copies here), installs the mod in the copy only; `run -Scenario X` writes the steps, starts the copy
  with `-nohomedir`, waits for the bot and saves the log; `remove` deletes the copy. Setup on the host PC: 52 s,
  83,482 links, 231 copied files, real install unchanged.

Rehearsal on one PC (host-only runner + the guest script with its own copy, guest aimed at that PC's own LAN
address instead of 127.0.0.1). Observed: the guest logged `Host name resolution failed with 11001` 3 s after
`connecting to 192.168.1.x:7778`, went back to the main menu, and waited until its timeout; the host never saw
it. Derived: `CoopLink.TryConnect` packs the four octets into one signed 32-bit int (the network driver needs
the address as one decimal number, see "CreateClient with a dotted host" above). Every address from
128.0.0.0 up comes out negative (`-1062731...`), which the driver then tries to resolve as a host name. Only
127.0.0.1 ever worked, so every one-PC test passed, and every join over a home network (192.168.x.x) would
have failed, from the menu's Join button too.

Fixes:
- `CoopLink.DecimalIPv4` builds the unsigned decimal in base 10000 (checked against Python's `ipaddress` for
  127.0.0.1, 10.0.0.5, 172.16.0.1, 192.168.0.1, 192.168.1.x and 255.255.255.255).
- A first join now retries like a reconnect (`bJoinRetry`, every `ReconnectInterval`, for `JoinRetrySeconds`
  = 300 s); before, a failed first attempt left the guest on the main menu for good. A refusal by the host
  (mods differ) stops the retries.

Results on the fixed build (real install unchanged, no crash dump each time):
- Rehearsal `join`: the guest joined at the PC's LAN address in under a second (`the host gave us player slot
  1`); host 3/3 plus guest 4/4 by `-CheckOnly`: 7/7.
- Rehearsal `mission`: host 30/30 plus guest 24/24 by `-CheckOnly`: 54/54. After the host built the mission
  the guest reconnected by the LAN address (`guest: reconnected, given player 1`).
- One PC over 127.0.0.1: `joinmismatch` 5/5 (the refused guest logged no retry), `join` 7/7, `dropguest` 15/15.
- Not covered: two real PCs, Steam in offline mode on the guest PC, a real network between two machines.

#### First run on two PCs: `join` 7/7 (2026-11-15)

Setup (owner): guest PC with XCOM 2 + War of the Chosen from Steam (same account), started once from Steam,
then `coop-guest.ps1 setup` (build hash matched), Steam put in offline mode. Host PC: `run_pair.ps1 -Scenario join
-RemoteB`. Both PCs on the same home network; the host's test copy needs an inbound firewall allowance for its
`XCom2.exe` (UDP), which Windows asks for the first time the server opens.

Observed: the guest connected 0.35 s after `connecting to 192.168.1.x:7778`; `compare received=1/1 match=true`,
mods hash and MP ini hash equal; `the host gave us player slot 1`; `ReceiveHistory frames=228`; the host's chat
arrived; host and guest fingerprints the same apart from the frame count (228 on the host at its `@histinfo`,
229 on the guest at its own). Host 3/3, guest 4/4 by `-CheckOnly`: 7/7. No crash on either PC; the host PC's
real install unchanged.

Answered: the game's network driver (`IpNetDriverSteamworks`) works for direct IP with Steam offline on the
guest PC and one Steam account on both PCs. Not covered yet: a mission over two PCs, Steam online on both.

Second run on two PCs: `mission` 54/54 (host 30/30, guest 24/24 by `-CheckOnly`), no crash, host PC's install
unchanged. Observed on the guest: first join at 66 s; the host closed the connection to build the mission
(`lost the host ... reconnecting`), the guest reconnected over the network 31 s later (`reconnected, given
player 1`); it took over its group in both rounds (`switch turn received: taking over initiative ... index 2`,
`resumed unit actions`), sent a ping and a chat, received the host's ping, and its watcher camera moved
(`camtest ... moved=3585`). The guest kept its slot from the `join` run (`slot wanted=1`, `CoopGuestMemory`).

Third run on two PCs: `missionend` 11/11 (host 6/6, guest 5/5), same campaign fingerprint on both
(`t2035-3-1h3...`), no crash, host PC's install unchanged. Observed on the guest: it left the Avenger when the
host closed the connection to build the mission (`leaving the Avenger for the main menu in 1 s, world
paused=True`), reconnected, left the mission map the same way when the mission ended, reconnected 35 s later,
loaded the Avenger and got the campaign.

#### A guest's soldier moves and shoots (`guestaction`, 2026-11-15)

Gap found by the owner watching the two-PC runs: the guest bots took their turn and handed it back without
acting. Only `mindcontrol` had a guest's soldier act, through a forced cheat-style ability. No test had a guest
move or shoot normally and then compared the result on the host.

New bot steps: `@move SPEC TILES` (a reachable tile about TILES away, path from the unit's
`X2ReachableTilesCache`, submitted with `XComTacticalController.GameStateMoveUnitSingle(..., bUserCreated=true)`,
the move a click on a tile makes); `@shoot SPEC` (the first matching unit whose `StandardShot` is `AA_Success`
fires at the first offered target through `XComGameStateContext_Ability.ActivateAbility`, the HUD fire button's
call: normal validation and hit roll); `@ustate ABILITY P` (finds the last frame in which a unit of slot P used
ABILITY and logs every XCom and alien unit as of that frame: tile, HP, alive, action points). Runner:
`sameustate` compares those lines between the host and guest logs (also in `-CheckOnly`).

Scenario `guestaction`: the host drops an `AdvTrooperM1` 7 tiles from the squad and skips the AI; on its turn
the guest moves one soldier about 4 tiles and fires; both machines dump the unit states after the move and
after the shot. One PC: 13/13, both comparisons equal (13 lines each). Observed: unit 421 moved 11,37,1 ->
15,37,1, fired at the trooper (`result=eHit_Crit`), trooper HP 6 -> 1; the same frame indices on both machines
(444, 447). Not covered: whether the host's screen shows the walk and the shot (visualization); the state check
is on the history.

#### Overwatch, the AI's turn and three players (2026-11-15, night)

New bot steps: `@moveto SPEC TARGET` (the reachable tile nearest TARGET, at least 2 tiles from it, through the
same move as `@move`); spec `newest:TEMPLATE` (the living unit of that template with the highest ObjectID, the
one dropped last; `template:` finds the first, which may be a pod's unit far away); `@ustate` takes `*` for any
ability and `alien`/`any` for the user; `@listeners SPEC` (for one unit, whether each event-listener trigger of
its abilities is registered on this machine, and its reserved action points). Runner: `sameustate` compares every
copy of the run (A, B, C).

Scenarios and results (one PC):
- `aiturnguest` 8/8: two troopers dropped near the squad, AI left on; after the alien turn both machines agree on
  every unit as of the last alien action (a trooper's `StandardShot` at a host soldier, `eHit_Miss`).
- `guestaction3` 11/11: three players; guest 1's soldier moved and hit a dropped trooper (`eHit_Success`), guest
  2's soldier moved; host and both guests agree on every unit after each of the three actions.
- `overwatchhost` (control) 8/8 once the setup targeted the dropped trooper (`newest:`): the trooper on overwatch
  fired at the host's soldier as it walked into view (`OverwatchShot ... eHit_Miss`), the same on both machines.
- `overwatchguest`: the same with the guest's soldier walking: no overwatch shot in 3 runs of 3 (host and guest
  agree there was none).

Observed (`@listeners` on the dropped trooper, host turn and guest turn): host `OverwatchShot event=ObjectMoved
registered=true`, guest `registered=false`; reserved points 1 on both. Derived: a unit created mid-mission on the
host (`DropUnit`, and so reinforcements, Lost swarms, a Chosen arriving) reaches a guest only as data; the
ability triggers that `X2TacticalGameRuleset.InitAbilityForUnit` registers never exist there, so its overwatch
never fires at a guest's soldiers (and other triggered abilities of such units likely don't either). Units
present at the mission start are registered by the guest's load path. The "4 ability listeners missing" counted
since 2026-11-14 were counted on the host only.

Fix: `CoopListenerRepair.RepairAbilityListeners`, every machine, in the same places as the effect repair (after a
received partial history, when taking over a turn): every alive unit's missing ability listeners and
event-listener triggers are registered again with the game's own filters, as `XComCheatManager`'s fixup does
(`FixupRegisterListener`) and as loading a save does for every unit. Abilities that unregister their own listener
after firing get it back, as on a load.

A guest crash, again at `XCom2.exe+0xb158a7`: in the first `overwatchguest` run (the setup still put a far pod
trooper on overwatch), the guest died as its first turn began (`resumed unit actions`, then `Crash Detected`),
access violation reading address 0. The same address as the guest crash of 2026-11-14 (avatarmc, then blamed on
the guest repairing 17 effects); this time the guest had repaired nothing. Both times the host had just changed
an alien (mind control, overwatch) before the guest's turn. Not seen in the next 4 `overwatchguest` runs. Cause
unknown; noted as an open rare guest crash, and the 2026-11-14 attribution to the effect repair is weaker now.

#### Full one-PC suite on the night's build (2026-11-16)

40 runs (every scenario but `soak` and `soakstory8`, plus `squad` and `exp12` on one copy), 23:25 to 04:30, the
ability-listener repair and the join fix in. 38 passed; real install unchanged in every run.

Two failures:
- `guestaction3` 9/11: guest 1's only soldier walked 4 tiles out of the trooper's sight (`AA_NoTargets`) and had
  nothing to shoot. A test setup gap: `guestaction` and `guestaction3` now walk toward the trooper
  (`@moveto owner:1 newest:AdvTrooperM1`).
- `chosenassault` 7/13: the guest crashed (`XCom2.exe+0xccc6a7`, access violation reading 0) on the Avenger map
  during the mission. Observed, host: the guest rejoined on the Avenger at 193.6 (`the campaign goes out in 3 s`);
  at 197.2 the delayed campaign send went out (`campaign history sent to guests, waiting for ack`) and 0.1 s
  later the host closed the connection to build the Chosen's assault. After the build the guest rejoined and the
  host sent the mission history; the guest acknowledged it, the host's still pending campaign wait took that ack
  (`guests have the campaign: telling them to enter the Avenger`), and the guest opened the Avenger with the
  mission's history (`entering Avenger as mirror, frames=487`), logged 2,326 `Accessed None 'UnitVisualizer'`
  from `XGUnit.CreateVisualizer` and died. Derived: both histories are acknowledged with the same command, and a
  stale campaign wait wins. A player who launches a mission seconds after a guest rejoins can hit it.
  Fix (`CoopTacticalNet`): sending the mission drops a pending campaign wait and sending the campaign drops a
  pending mission wait; a campaign ack counts only while the host is outside a mission.

Also seen: `relayleave` and `mission3` ran until the runner's timeout although they passed. A guest whose host
had quit reconnects from the main menu every ~64 s, each reload re-created the test bot, and the bot restarted
its 90 s step delay each time, so the step never came (in every earlier `mission3` run too, which is why it
always took 25 min). Fix (`CoopHarness`): the pending step's due time is kept on the wall clock across map loads.

Reruns on the fixed build (all four test copies on the same build): `chosenassault` 13/13 three times (once on a
mixed build, see below; one run hit the same window and logged `a pending campaign ack is dropped: the mission
replaces it`, then `guest has the mission history: telling it to load the map`), `guestaction3` 11/11,
`guestaction` 13/13, `relayleave` 16/16 in 6 min (25 min before the bot's timer fix), `mission3` 27/27 in 6 min,
`missionend` 11/11, `join` 7/7, `mission` 54/54. Real install unchanged in all 48 runs that ended normally.

Also seen in the first `chosenassault` rerun: the guest got its slot on the main menu 5 s before the host closed
the connection for the mission build, never saw the close, and stayed "connected" without a history until the
host quit 190 s later (no timeout: guess, the host's reopened server on the same port kept the old connection's
traffic alive). Fix: `CoopLink` reconnects a guest that has a slot but no history after `JoinNoHistorySeconds`
(150 s, longer than the host's 120 s wait for other guests). Not reproduced on purpose; untested.

Process slip (test tooling, no effect on results reported above): a build was started while a rerun batch ran;
its copy step blocked on the running guest's files and left copy 0 on a newer build than copies 1-3. The loop
was stopped, one run finished on the mixed build (counted above as such), the build was redone with no game
running, and the reruns restarted.

#### Red screens with Steam's launch options (2026-11-16)

Observed: Steam's own launch configuration for XCOM 2 passes `-noRedScreens -review` (Steam's app info);
the test copies leave `-noRedScreens` out. `mission` with `-review -noRedScreens` added (the copies' other
options `-allowconsole -log -nohomedir` kept): 54/54, and the bot still closed red screens (1 on the host, 2 on
the guest; 2 and 3 in the run before). Every red-screen message in both logs is the base game's own (destructible
actor sizes on a sewer plot, `LIST_TheLost_PrePlaced`, `Plot_Multiplayer_Test`, DLC narrative sounds loading
synchronously); none names a mod class. Unknown: whether a plain Steam launch (without `-allowconsole -log`)
shows them.

#### Plan change: scenario mode instead of horde defense (2026-11-16)

Owner decision: the third co-op mode is a scenario mode, not horde defense. Single co-op missions without a
campaign; the host picks a game stage (early, mid, late); players build operators from a point budget (higher
stats and better gear cost more); missions carry several objectives so the squad has to split up. Design,
what the SDK already offers (WOTC Skirmish mode setup, the multiplayer squad builder's point costs) and open
questions: DESIGN-campaign-coop.md, "Scenario mode".
Owner decisions the same day: 12 operators per mission split evenly among the players (12/6/4/3 for 1-4); XCOM
soldiers only; points buy gear, abilities, stats and special units (Reaper, Skirmisher, Templar, Psi Operative);
the weapon and armor tier follows the game stage (conventional/Kevlar, magnetic/Predator, beam/Warden), with the
special suits (Spider, EXO, Wraith, W.A.R.) costing extra points.
Further owner decisions: the budget scales with the game stage; each stage has a minimum rank, higher ranks
bought with points; utility items bought with points; objectives come in sets of 1-3 running at once, most with
a turn limit, the next set starting when the current one ends, more objectives in late game (example: kill an
ADVENT officer in 5 turns + capture a zone in 6; then regroup and defend the zone for 8 turns; then set charges
with no limit + extract in 5).
Further owner decisions: operators are saved between sessions, each with a count of missions completed; objective
sets are random, with exclusion rules (extract and kill the officer never together); some objectives fail the
mission when missed, others make it harder; no scoring.
