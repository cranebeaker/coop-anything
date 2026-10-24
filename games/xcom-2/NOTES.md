# XCOM 2: War of the Chosen — co-op campaign notes

Running log. Observations, derived facts, guesses and unknowns are kept in separate lists.

## Goal (owner's design)
- Shared strategy layer (Avenger): both players can pick research/build/facility upgrades and
  edit any soldier.
- Squad select: each player picks their own squad (e.g. 4 + 4 = 8 on the field);
  a soldier picked by one player is locked for the other.
- Tactical: each player controls only their own soldiers; players alternate.
- 2 players, Steam, WotC + all DLC.

## Session 1 — 2026-10-24: triage + research

### Observed
- Game identity: see `local/identity.md` (Steam build 8270065, changelist 699444, UE3).
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
- Owner's install had a corrupt `XComGame/CookedPCConsole/TacticalResources_SF.upk`
  (crash at shell load: "Texture2D AdventDeskA.Textures.AdventComputerA_MSK: Serial size
  mismatch: Got 1588, Expected 1956"). Steam "Verify integrity" replaced it (6.7 MB);
  the shell menu then loads. Lesson: verify integrity BEFORE copying a test install.
- `-nohomedir` (string present in exe) makes the game write Config/Logs/SaveData into
  `<install>/XComGame/...` instead of `%USERPROFILE%\Documents\My Games\...`
  (generated XCom*.ini appear in `<install>/XComGame/Config`). Keeps tests away from
  the owner's Steam Cloud folder.
- Second instance exits with code 1 before writing a log while the first one runs.
  Cause: mutex `\Sessions\<n>\BaseNamedObjects\UnrealEngine3_<n>` (created via
  `CreateMutexW("UnrealEngine3_%d%s")`). Closing that handle inside instance A
  (DuplicateHandle with DUPLICATE_CLOSE_SOURCE, same user, no admin) lets B start;
  both log "STEAMWORKS initialized 1". Closing `CDdf212806D6EmB31yE0c` and
  `{52813408-...}_0` did not help (not the guard).
- Second install = hard-link clone of the first, but with its own copy of
  `XComGame/Config` (the game rewrites inis there).
- Tools: `tools/launch_pair.ps1`, `close_remote_handle.py`, `hardlink_clone.py`,
  `re_callers.py`, `re_dis.py`, `trace_start.py` (in the owner's workspace, to be moved
  into the game repo).

## Session 1 (cont.) — first network experiments (two instances, one PC)

Mod repo: `WOTCCoopCampaign` (owner's workspace; to be published as its own repo).
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
- Merge states fit "one editor at a time" (baton) for resource changes. Not tested yet:
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
- Edits by both players at the same time (no baton): untested, expected to conflict.

## Session 1 (cont.) — facilities and screen refresh on the receiver

Test installs: redscreens turned off (`[XComGame.RedScreenManager] bDisabled=true` in the
test copies' DefaultGame.ini, backup `.bak`). The owner's screenshots of redscreens were from
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
  own SetAvengerVisibility will also swap the room and write a local frame (not tested).

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

4. The owner confirmed moving the mouse over the game windows during the earlier runs.
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
2. Test "baton-replies" (first version): the owner clicked in both windows during the run
   (entered facilities, opened the Armory, pressed Cancel, pressed "Ask for control"
   several times): 5 handoffs back and forth, all with equal fingerprints; the edit gate
   and Cancel worked from real mouse clicks.
3. In that run B crashed 0.2 s after a harness `@nobaton`, while inside a facility
   (owner's clicks): access violation reading 0xffffffffffffffff in tbbmalloc.dll+0x29c9.
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
