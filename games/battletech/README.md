# BattleTech (2018)

| | |
|---|---|
| Engine | Unity 2018.4 (Mono), game code in C# (`Assembly-CSharp.dll`); mods load through ModTek 4.5.1 (HarmonyX with a Harmony 1.2 shim) |
| Versions tested | Steam app 637090, build id 20387569, BattleTech 1.9.1 (BuildNumber 686R). `BattleTech.exe` SHA-256 `a12620dd89f2a7fc51fe228d38ba656574228a1c9412c27ca556fee08c5fc42f`, `Assembly-CSharp.dll` SHA-256 `9eaa1b3a4dfd80ac03abd47097541e9883a4fa4d4021d877b9b8faa2d23c8b2d`. Windows 11. |
| Projects | [battletech-coop](https://github.com/Cryswar/battletech-coop) (MIT, work in progress, directory entry `battletech-coop`); a separate project by another developer is listed as "BattleTech Co-op (Co-op Anything Discord)" |
| QA level | automated (two real game copies on one PC, scripted), plus human sessions over Steam with two players |
| Last updated | 2026-10-21 |

## Where things stand

Two players can play skirmishes and Campaign or Career missions together over Steam, and
share the company between missions: **pre-alpha, not finished**. In missions every player can
command every 'Mech of the player lance; the host's game is the authority and the guest's
game replays the same orders. Between missions the guest sees the host's company and can
refit, repair, rename, ready, store and scrap 'Mechs, train, rename, hire and dismiss
pilots, buy and sell, cancel work orders and customize the company; the host's game applies
it, and the host's own changes reach the guest. Both players watch the mission opening (camera
intro, dropship, opening conversation) and advance it together. There are no per-player lances
yet.

## Existing work

- **battletech-coop** (this write-up), [github.com/Cryswar/battletech-coop](https://github.com/Cryswar/battletech-coop),
  started 2026-10-16, releases 0.0.2 to 0.0.13 (pre-release zips with ModTek).
- **BattleTech Co-op (Co-op Anything Discord)**: a different developer posting progress in the
  BattleTech topic on the Co-op Anything Discord; no public build when this was written. See
  its directory entry.
- No earlier co-op campaign mod was found (Nexus, GitHub, Steam forums, 2026-10-16). The game
  ships versus multiplayer for skirmish only. Community mods that matter here: ModTek (loader),
  Mission Control and Bigger Drops (more player units per drop), BattleTech Advanced 3062 (large
  pack; not covered by our tests).

## How the game works (for co-op)

Details and sources in [NOTES.md](NOTES.md); the full design and every test run are in the
project's [docs/DESIGN.md](https://github.com/Cryswar/battletech-coop/blob/main/docs/DESIGN.md).

- **Combat already has a lockstep layer.** Every combat action is an `InvocationMessage`
  (move with its full waypoint path, attack, melee, jump, sensor lock, reserve...), all of
  them pass `InvocationHandler.GenericInvoke`, and that reseeds the game's logic RNG
  (`NetworkRandom`, a fixed-seed XorShift) from the invocation's own `RandomSeed`. The versus
  mode relays these messages; single-player runs the same path without the relay. So co-op
  combat is deterministic lockstep on invocations, with the host as the ordering authority.
- **The AI is not deterministic across PCs** (think-time budget from `Time.realtimeSinceStartup`,
  `UnityEngine.Random` in reserve rolls), but it outputs ordinary invocations. Only the host's
  AI thinks; the guest gets its results through the stream.
- **The game's own serializer** (`BattletechSerialization`, as used by
  `NetworkMessageResolver`) round-trips invocations and contracts; saves are
  `SaveBlock<GameInstanceSave>` bytes.
- **The campaign layer (`SimGameState`) has no command layer.** UI screens call its methods
  directly (`InitializeMechLabEntry`, `UpgradePilot`, `StarSystem.HirePilot`,
  `Shop.Purchase`...), so it is not lockstepped: the host owns it and the guest's copy is
  a snapshot plus mirrored changes.
- **Saves** are Steam Cloud files under the Steam user folder (`userdata/<id>/637090/remote/C0/SGS1`);
  test copies of the install share them, so tests must never save.

## What we changed

The mod is a ModTek mod written against the Harmony 1.2 API; transport is Steam lobbies plus
`SteamNetworking` reliable P2P (512 KB chunks), with a TCP transport for tests.

- **Combat lockstep.** A prefix on `GenericInvoke` records every invocation on the host and
  streams it with a sequence number; the guest replays them in order. Guest orders become
  requests that the host validates, gives a new invocation ID (`G<player>.<n>`) and publishes.
  Turn activations are counted on both sides; the guest's AI never thinks. A per-unit digest of
  position, armor and every stat (culture-invariant, components keyed `def@location`) is
  compared after each activation; units still running an order are skipped (they caused false
  alarms). Test: `-Combat` (AI plays both sides, guest replays) and `-Shared` (half the
  orders are guest requests), each with `-InjectDesyncAt` as the mutation twin that must FAIL.
- **Campaign missions by save.** A contract regenerated on the guest would differ, so the
  host builds a `GameInstanceSave` in memory and sends it; the guest loads it through
  `LoadTransitioning.FileReadComplete`. Up to 0.0.12 the save was taken at the first point
  where `CombatGameState.CanSave` holds, which is after the mission opening, and the host
  reloaded it too, because a loaded combat is not identical to the live one it was saved from
  (components come back in a different order; a critical hit landed on a different
  component). Saving is blocked on the guest (`TriggerSaveNow`, `SaveGameStructure.Save`,
  `SaveCampaignResult`) and on test copies.
- **Mission opening for both (0.0.13).** The host now takes the save at its briefing, and the
  guest loads it flagged as a combat restart (`GameInstanceSave.flaggedForRestart`; the game's
  "mission save out of date" popup is skipped). `GameInstance.CreateCombatFromSave` then runs
  `Contract.ResetStateForRestart` and a fresh `LaunchContract`, so both sides launch the same
  mission from the start and the host does not reload. The restart reloads the mission
  definition and re-rolls its tag-based enemy lances (first run: 6 actors on the host, 7 on
  the guest), so the host sends its picks (lance, unit, pilot per spawn point) with the save
  and a postfix on `ResetStateForRestart` writes them back as explicit IDs. "Begin mission" is
  shared (held until every briefing is loaded), and conversation lines
  (`InterruptDialogSequence.AfterDialogHide`, the Continue) go through the host, which
  advances and tells the guests to follow. Timed side chatter is not synced.
- **Ship view.** Between missions the host sends its campaign as an in-memory save; the guest
  loads it the same way. The new `SimGameState` exists before the save is applied to it
  (default company, funds 0), so "loaded" means the game's `_OnAttachUXComplete` fired for the
  received company. Found by the first ship-view test, which compared the guest's company
  with the host's and failed on the placeholder.
- **Ship changes both ways.** Changes are captured where the screens call the game
  (`InitializeMechLabEntry` for every refit and repair, `UpgradePilot`, `HirePilot`,
  `Shop.Purchase`/`SellInventoryItem`, `ReadyMech`, `UnreadyMech`, `Scrap*Mech`,
  `DismissPilot`, `CancelWorkOrder`, the heraldry screen's close) and sent as data (work orders
  as install/armor/structure/repair lists, pilots as target skill levels). The other side
  repeats them through the same game functions with its own objects, so costs, refit time and
  storage behave as in single-player. While applying a remote change the capture patches stay
  quiet (no echo), and of nested calls only the outermost is sent.
- **Same IDs on every copy.** New pilots and 'Mechs get IDs from a counter
  (`"SGRef_" + ++UIDCount`). Each change carries the counter its side started from and is
  applied from it; the guest counts in its own range (1e9 x player index above the host), so
  IDs never collide. Without this, a pilot hired on the guest and later dismissed would not be
  found on the host.
- **Open screens redraw** after a remote change (Mech Bay `RefreshData`, Barracks `Reset`,
  Store and Hiring Hall lists), waiting while the MechLab is open or a pilot has unconfirmed
  training (`SGBarracksWidget.IsCurPilotModified`).
- **Session end.** A closed session clears all combat-sync state (a stale one blocked
  re-hosting), and a guest whose host leaves returns to the main menu.

## QA

Run from the project repo (Windows PowerShell, two copies of the game; see its README):

- `tools/two-copy-test.ps1` starts a host and a guest copy over localhost TCP with
  `-coop-no-saves`, drives them with in-game test modes, asserts on both logs and closes them.
  Modes: plain (connection), `-Combat [-Shared] [-SaveTransfer] [-InjectDesyncAt n]`,
  `-HostQuitTest`, `-ShipView`, `-ShipRequests`.
- `-ShipRequests` makes both copies change the company (rename and refit 'Mechs, train and
  rename pilots, hire, buy, sell, store and ready a 'Mech, cancel that order, dismiss the other
  side's hire, rename the company), then compares a digest of company, funds, pilots, bays,
  work orders, storage counts and pilots for hire; the host's last digest must equal the
  guest's final one. It also requires a Barracks redraw on the host.
- `dotnet run --project tests/NetTests`: the network core over real localhost sockets.

- `-MissionStart`: the host loads a career save, sends the ship and launches the first
  ordinary contract as the lance screen's Accept does; it requires both sides to finish the
  opening dialogue with identical lines in the same order, Continue presses from both sides,
  then stream items with no desync.

Last results (2026-10-25, release 0.0.13): `-MissionStart` PASS (same 8-unit battle on both,
10 picks pinned, the same 3 opening lines on both, 48 stream items, 0 desyncs);
`-Combat -Shared` PASS (60 stream items, 0 desyncs). 2026-10-18, release 0.0.12:
`-ShipRequests` PASS (9 requests and 9 updates applied, none refused, the readied 'Mech has
the same ID on both, digests equal). Earlier: a whole save-transfer mission
with full-stat comparison PASS (176 items, 38 guest orders, 0 desyncs), `-InjectDesyncAt`
caught as required, `-HostQuitTest` PASS.

Human sessions (two players on two PCs over Steam): a campaign mission with 77 orders (17 by
the guest: moves, attacks, jump, melee, DFA) and 0 desyncs; ship requests (pilot rename and
training, 'Mech rename, a refit) applied on the host, none refused.

## How to play

Download the zip from the project's [releases](https://github.com/Cryswar/battletech-coop/releases)
(pre-release). Every player installs the same zip (ModTek plus the mod) into the game folder;
`BattletechCoop-README.txt` inside has install, host, join and uninstall steps. The host loads
their campaign, opens the co-op window (Ctrl+Shift+C), hosts via Steam and invites; friends
accept the Steam invite from the main menu.

## Known issues

- The shared mission opening (0.0.13) is tested on ordinary contracts only, not on story
  missions or Flashpoints. Side chatter during a mission is not synced.
- Everyone controls the whole lance; there are no per-player lances or larger drops yet.
- Travel, passing time and accepting contracts are the host's; if the guest does them on its
  copy they are lost at the next refresh. Pilot portraits are not synced.
- A full ship refresh takes 15 to 20 seconds (the game's own load).
- No saving in co-op combat and no rejoining a battle after a disconnect.

## Next steps

- Test the shared mission opening on story missions and Flashpoints.
- Per-player lance ownership on top of the shared control, and Bigger Drops / Mission Control.
- Block host-only actions on the guest (travel, time, contracts) instead of letting them be lost.
- A compatibility pass with BattleTech Advanced 3062.
