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
