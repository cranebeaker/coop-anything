# XCOM 2: War of the Chosen (2017)

| | |
|---|---|
| Engine | Unreal Engine 3 (UnrealScript, official mod SDK) |
| Versions tested | Steam, XCOM 2 buildid 8270065 (changelist 699444), War of the Chosen + all DLC, Windows x64 |
| Projects | Team Dragonpunk "Tactical Co-Op" (2016, abandoned); WOTC Co-op Campaign (prototype, this write-up): https://github.com/EvilMask587/xcom2-wotc-coop-campaign |
| QA level | automated (strategy layer only; see QA) |
| Last updated | 2026-10-25 |

## Where things stand

You cannot play a co-op campaign yet. A prototype mod shares the strategy layer
(Avenger): both players see the same campaign and take turns changing it (a "baton"), and
a scripted two-instance test suite checks that the state stays identical. Tactical
missions are not shared yet.

## Existing work

- **Team Dragonpunk, "XCOM 2: Tactical Co-Op"** (Steam Workshop 792693587): created
  3 Nov 2016, last updated 3 Sep 2017, status "Final". Two players split the squad on
  campaign missions; soldiers can be imported from the friend's game; no Ironman. Only the
  tactical layer, no shared strategy. Probably pre-WotC (last update 5 days after WotC).
  No public source found (GitHub search "xcom2 coop", "dragonpunk xcom": nothing).
  https://steamcommunity.com/sharedfiles/filedetails/?id=792693587
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
- Running two instances on one PC: the exe holds a mutex `UnrealEngine3_<n>`; closing that
  handle inside the first instance lets the second start. `-nohomedir` keeps config, logs
  and saves inside the install folder.

## What we changed

Mod: https://github.com/EvilMask587/xcom2-wotc-coop-campaign (code only, no game files). Details and every test run in
[NOTES.md](NOTES.md).

- **Client join without post-mission side effects.** A plain `open Avenger_Root` treats a
  received history as a return from a mission and adds 7 frames and new ObjectIDs.
  `CoopHeadquartersGame` (ModClassOverrides) takes the load-game path instead; only one
  unavoidable "Pre Mission End UI Sequence Hook" frame remains.
- **False desyncs.** The fingerprint command overtook its merge state; the fingerprint is
  now compared when the merge state arrives.
- **ObjectID collisions.** The player without the baton could create objects with IDs the
  holder hands out again. `CoopStrategyRuleset.SubmitGameStates` rejects frames that create
  objects without the baton (new = `GetPreviousVersion() == none`;
  `GetGameStateForObjectID` already resolves pending IDs and never says "new").
- **Baton handoff race.** A grant could arrive before the holder's last merge states.
  The receiver waits until its fingerprint and highest ObjectID match the grant and 1 s
  passes without frames. Counting merges failed (commands and merges do not pair up).
- **Holes in rooms.** RemoveRoom + StreamInRoom (what the GiveFacility cheat does) left
  rooms half drawn. The receiver now queues a swap like `XGBase.QueueRoomSwap`.
- **Edits lost on the non-holder.** Every edit screen asks for the baton first (dialog with
  Cancel); the holder hands over when its own edit screens close; HUD panel with "Ask for
  control".

## QA

- Two instances on one PC, each its own copy of the game; a test bot inside the mod runs a
  step list per instance; `tools/test_suite.ps1` in the mod repo asserts on both logs:
  expected lines, no DESYNC / crash / duplicate crew IDs / stale rooms / bot timeouts, and
  equal final fingerprints on both players.
- Scenarios: `baton-objects` (resources and units both ways, grant right after a frame,
  version chains checked), `facility` (facility removed and rebuilt, room swaps on the
  client), `blocked-without-baton`, `edit-gate` (deferred requests while an edit screen is
  open, both directions), `clock` (Geoscape time), `projects` (research and construction
  started by one player finish on the other's Geoscape), `rejoin` (the client's process is
  killed while it holds the baton, it reconnects and gets the history again), `burst` (many
  frames in one tick),
  `client-save` and `client-save-menu` (only the host saves), `panel-replies` (HUD button,
  deferred / not-mine replies, Cancel).
- Last suite result: 11/11 passed (2026-10-25, `tools/test_suite.ps1`), one PC, two
  instances, 127.0.0.1.
- A host save made after receiving the client's merges loads back the same campaign
  (fingerprint and crew equal; Geoscape time runs; saving again works), checked by hand
  with the bot's `@save` / `@load N` (2026-10-25).
- Screenshots (taken by the bot) were checked by eye for room swaps, the wait dialog and the
  HUD panel.
- Scope: one PC, two instances, strategy layer only. Not covered: two PCs, Steam networking, long sessions, the host dropping, anything tactical.

## How to play

Not playable yet. Developers: build and test instructions in the mod repo README.

## Known issues

- No tactical co-op; the prototype only shares the Avenger.
- Whoever stays on the Geoscape keeps control until leaving it.
- Popups (research done, facility built, new missions) appear only for the player on the
  Geoscape.
- One client crash while entering facilities by hand (engine package loading, tbbmalloc),
  not reproduced.

## Next steps

- Two PCs and Steam sockets.
- Join flow for real play: the host sends the history when a client joins (now the test bot
  does it), a menu entry to host / join.
- Tactical: keep one XCom player and split its turn into sub-phases per player, with the
  sub-phase owner as authority and `SendPartialHistory` at each handoff (the PvP model).
- Squad select: each player picks soldiers; locks per soldier.
