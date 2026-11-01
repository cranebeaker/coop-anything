# BattleTech (2018): co-op campaign feasibility notes

Status when written: phase 0–3 (triage, research, static analysis), before anything was built. Later findings are at the end.
Date: 2026-10-16.

## Game identity

| | |
|---|---|
| Store | Steam, app 637090, build id 20387569 |
| Version | 1.9.1, BuildNumber 686R, BuildStamp 200226-live-cdbbe62 (`BattleTech_Data/StreamingAssets/version.json`) |
| `BattleTech.exe` SHA-256 | `a12620dd89f2a7fc51fe228d38ba656574228a1c9412c27ca556fee08c5fc42f` |
| `Assembly-CSharp.dll` SHA-256 | `9eaa1b3a4dfd80ac03abd47097541e9883a4fa4d4021d877b9b8faa2d23c8b2d` |
| Engine | Unity (Mono, `MonoBleedingEdge`), C# game code in `Assembly-CSharp.dll` |
| OS | Windows 11 |
| Mods installed | none (no `Mods/` folder) |

## Prior work

- No co-op campaign mod found ([search 1](https://www.nexusmods.com/battletech/mods/584?tab=posts), [Mission Control](https://github.com/CWolfs/MissionControl)). No entry in DIRECTORY.md, no issues or PRs in this repo.
- Building blocks that exist: ModTek loader; HBS's built-in mod loader since 1.7 ([ModTek](https://github.com/acook/ModTek)); Mission Control (runtime contract and encounter changes, extra lances) and Bigger Drops (more than 4 player units) ([Mission Control](https://github.com/CWolfs/MissionControl)).
- Official multiplayer exists but only for skirmish (PvP). Its online service has had outages, with no official shutdown notice found ([Steam thread](https://steamcommunity.com/app/637090/discussions/0/5294572213363418960), [studio split](https://www.rpgsite.net/news/14992-paradox-and-battletech-developer-harebrained-schemes-to-part-ways-in-2024)).

## Method

REA `inspect_managed_artifact` stopped at its 64 MB metadata representation budget on the
13.6 MB `Assembly-CSharp.dll`, so metadata and the IL call graph were dumped with two small
`System.Reflection.Metadata` scripts (types, fields, methods, user strings; one line per
call/field/ldstr edge, 641k edges). No decompiled source is stored here.

## Observed (from shipped assemblies)

1. **Every combat action is an `InvocationMessage`.** Subclasses include `MechMovementInvocation`,
   `AttackInvocation`, `MechMeleeInvocation`, `MechDFAInvocation`, `MechJumpInvocation`,
   `SensorLockInvocation`, `VentCoolantInvocation`, `ReserveActorInvocation` and `EjectInvocation`.
   Each carries `RandomSeed` and `InvocationID`, and has `ToJSON`/`FromJSON`.
   Movement carries the full `Waypoints` path and `FinalOrientation`, so the path is
   computed by the issuer, not re-pathed by each receiver.
2. **`InvocationHandler::GenericInvoke` is the single choke point.** It checks
   `IsNetRouted`/`WasNetRouted` and `NetworkGameInstance::IsNetworkGameActive`, queues the
   invocation if an order is running, de-dupes by `InvocationID`, sets
   `CombatGameState.NetworkRandom.seed = invocation.RandomSeed`, records the invocation in
   `MultiplayerGameVerification`, and then calls `Invoke`. **Single-player runs the same path;
   multiplayer only adds the routing step.**
3. **The game logic RNG is deterministic.** `NetworkRandom` wraps `HBS.Math.XorShiftRandom`.
   Its constructor sets a fixed seed (no clock read), and it is reseeded per invocation.
   `UnityEngine.Random` is only used to *roll* the seed in invocation constructors (on the
   issuing machine) and for visuals (`*Representation`, VFX, camera, floaties).
4. **Routing is a dumb relay.** The client's `MessageCenterMessageHandler::HandleNetRoutedMessages`
   wraps the message in a `NetMessageCenterMessage` and sends it. The server's
   `MessageCenterMessageHandler::HandleMessage` forwards it to all sessions, and the receiving
   client sets `WasNetRouted` and republishes it into the local `MessageCenter`.
5. **A full in-process server ships with the game:** `BattleTechServerInstance`, `GameLobbyManager`,
   `NetworkSessionManager`, endpoints for Loopback, Lidgren (direct IP, Open.Nat), SSE,
   PlayByPost and Steam (`SteamServerEndpoint` creates Steam lobbies via `SteamMatchmaking`).
   Steamworks.NET (452 types, including `SteamNetworking`) is in `Assembly-CSharp-firstpass.dll`.
6. **Built-in desync detection:** `StatsComparer`/`StatsCompareNetworkMessage`,
   `MultiplayerGameVerification`, and `NetworkRandom.RecordHistory`/`GetHistory`.
7. **Turn sync exists:** `TurnDirector::SendTurnActorActivateMessage` uses a
   `NetSynchronizeMessageRequest` barrier when a network game is active (direct call otherwise).
8. **Teams:** `TeamController` = {Computer, Player1, Player2}. `Team.LocalPlayerControlsTeam`, `IsLocalPlayer`.
   `EncounterLayerData::CreatePlayerOneTeam/CreatePlayerTwoTeam` branch on multiplayer.
9. **AI is not cross-machine deterministic:** `AITeam::getInvocationForCurrentUnit` reads
   `Time.realtimeSinceStartup` (think budget), and `AITeam::IsReserveDieRollSuccessful` uses
   `UnityEngine.Random`. AI still outputs ordinary invocations (`AITeam::make*Invocation`).
10. **Dialogue advance is local UI:** `CombatHUDDialogItem::Update` reads `Input.GetKeyDown` and
    `Time.deltaTime`. The game has `DialogueContinueMessage` (a MessageCenter message).
11. **Story scripting reads campaign state:** `TScript.Ops.BattleTech` calls
    `GameInstance::get_Simulation` 41 times, and `Contract` calls it 20 times.
12. **Campaign RNG is also `NetworkRandom`:** `SimGameState::.ctor/Rehydrate/PrepContract` seed it.
13. **Saving is blocked in MP:** `GameInstance::CanSave` checks `IsMultiplayer`. Contract types have
    `IsMultiplayer`, `IsSinglePlayerProcedural` and `IsStory` flags. Story contracts were never MP-tested.
14. HBS ships `0Harmony.dll` and a `BattleTech.ModSupport.ModLoader`.

## Derived

- Combat co-op = **deterministic lockstep on invocations**. It needs: the same starting state
  on both machines; every invocation (player and AI) going through one ordering authority;
  and barriers where the game waits on local UI (dialogue, turn activation, mission end).
- Running the AI on the host only and broadcasting its invocations removes AI nondeterminism.
- Campaign layer: the guest needs the host's exact `SimGameState` (TScript conditions read it).
  The cheapest way is to transfer the host's save at contract launch. Campaign RNG is seeded and saved.
- The giant 8-player-unit problem is already solved by community mods (Bigger Drops, Mission Control).

## Guesses (unverified)

- Unity physics/raycasts used by LOS and pathing give identical results on two PCs with the
  same build. The MP skirmish mode depends on it, and moves ship their waypoints, which lowers the risk.
- Story encounter triggers (region, objective, dialogue chains) are driven by MessageCenter
  messages raised from invocations, so they replay identically. Some may poll per-frame
  timers. `Contract::GetRandomObjectiveStatus` uses `UnityEngine.Random` (call site not traced).

## Unknown

- Whether `BattleTechServerInstance` can run without HBS/Paradox backend auth (`NetworkServerAuthentication`).
- Whether non-interleaved (out-of-combat free movement) mode has extra local-only paths.
- Whether combat saves (single-player allows mid-mission saves) restore identically on another PC (needed for rejoin).
- Behaviour of Flashpoints and DLC story missions under lockstep.

## Next steps

Feasibility summary, in short: (M0) ModTek mod skeleton plus Steam P2P
transport; (M1) skirmish-vs-AI lockstep with desync checks; (M2) campaign contract with save transfer;
(M3) unit ownership and two lances; (M4) dialogue/turn/mission-end barriers; (M5) bot QA.

## Runtime check: M0 (2026-10-16)

Launch of a test copy (ModTek v4.5.1, `/W MODTEK` shown), one skirmish vs AI, 3 rounds,
with the BattletechCoop logging patch on `InvocationHandler.GenericInvoke`. The mod's log showed:

- `GenericInvoke` receives every MessageCenter message (1,242), of which 25 were invocations.
- The AI's moves (`AbstractActorMovementInvocation`) and the player's moves (`MechMovementInvocation`),
  attacks, reserves and sensor locks all reached it. That confirms observed items 1–2 and 9 at runtime.
- Invocations carry `IsNetRouted=True` in single-player; routing is skipped only because no network game is active.
- `InvocationID` = `<machine prefix>.<counter>`.

## Later findings (2026-10-17 to 2026-10-19)

Recorded in full, with each test run, in the project's
[docs/DESIGN.md](https://github.com/Cryswar/battletech-coop/blob/main/docs/DESIGN.md). The ones
that answer the unknowns above or would have saved time:

- Combat saves do **not** restore identically to the live state they came from, even on the
  same PC: components come back in a different order, so a critical hit picked a different
  component on the guest (seq 207, found by the full-stat comparison). Both sides now run from
  the same loaded save.
- The first point where a combat save is allowed (`CombatGameState.CanSave`) is after the
  mission-opening conversation, which is why the guest missed it up to 0.0.12. Fixed in 0.0.13
  by a save taken at the briefing and loaded as a combat restart
  (`GameInstanceSave.flaggedForRestart`), which launches the saved contract fresh.
- A combat restart reloads the mission definition and re-rolls tag-based lances and units
  (`LanceOverride.RequestLance`, `UnitSpawnPointOverride.RequestUnit`); the host's picks
  (`selectedLanceDefId`, `selectedUnitDefId`, `selectedPilotDefId`) have to be sent and
  written back as explicit IDs.
- A save load started while the briefing is up (`ProcessLoadedFile`) never reaches
  `CreateCombatFromSave`, even after the loading curtain is gone, so the host cannot reload
  from the briefing.
- `InterruptDialogSequence.AfterDialogHide` returns `SimGameConversationManager.ConversationState`;
  a Harmony prefix declaring `ref bool __result` makes HarmonyX refuse the patch and the
  whole mod's `PatchAll` fails.
- In autoplay tests, Player 1 is an `AITeam`, which sits on the stack and keeps `CanSave` false;
  the test uses the same conditions minus the stack check.
- Free movement out of combat (non-interleaved mode) produced false desyncs from units still
  running an order when the snapshot was taken; skipping units with a running `OrderSequence`
  fixed it.
- Unity hangs at "Input initialized" before any mod code runs while the display is asleep, so
  unattended game tests need the screen awake.
- ModTek allows one game process per install folder; the second test copy is a folder of small
  file copies with a junction to the first copy's `BattleTech_Data`.
- `MechBayPanel`, `SGBarracksWidget` and the other screens stay in the scene after first use:
  check `UIModule.Visible`, not only `activeInHierarchy`, before redrawing one.
- `SGBarracksMWDetailPanel.SetPilot` leaves `tempPilot` null until something is edited, so
  "unconfirmed edits" is `SGBarracksWidget.IsCurPilotModified()`, not a reference compare.
- `SimGameState.ReadyMech` and `ScrapInactiveMech` overwrite their `id` argument (chassis ID to
  storage stat ID) before returning, and a Harmony postfix sees the overwritten value.

Since 2026-10-20 the directory also lists a separate BattleTech co-op project by another
developer on the Co-op Anything Discord.
