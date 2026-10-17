# Stacklands (2022)

| | |
|---|---|
| Engine | Unity 2022.3.4f1, Mono (official mod loader with Harmony) |
| Versions tested | Steam, Windows build under Proton Experimental, buildid 16550853 |
| Projects | Stacklands Co-op ([stacklands-coop](https://github.com/Dudiebug/stacklands-coop)) |
| QA level | automated |
| Last updated | 2026-10-17 |

## Where things stand

Stacklands Co-op lets 2+ players share one board online: same cards, same moons, same quests. Every core loop works and is covered by an automated two-instance test: opening packs, stacking villagers on resources, harvest timers, selling, buying packs, changing speed and clicking through cutscenes. Steam lobbies are implemented but untested (they need two Steam accounts).

## Existing work

No co-op or multiplayer mod for Stacklands turned up in searches of the Steam Workshop, GitHub, Nexus or general web search (2026-10-17). The official game is single-player only.

## How the game works (for co-op)

- **Mods:** Stacklands ships an official mod loader. A mod is a folder in `LocalLow/sokpop/Stacklands/Mods/<id>/` containing `manifest.json` and `<id>.dll`. The DLL has a class deriving from `Mod : MonoBehaviour` with a `Ready()` hook and a ready-made `Harmony` instance. `Ready()` is called **three times** per launch, so guard it.
- **Logging is off** in release builds (`Debug.Log` prints nothing after the loader starts), so write your own log file.
- **No netcode at all.** All state lives in `WorldManager.instance`: `AllCards`, `AllBoosters`, `MonthTimer`, `BoardMonths`, `SpeedUp` and `CurrentBoard`. Each card is a `GameCard` (`Draggable`) with a `CardData`, and is identified by `CardData.UniqueId`.
- **Saves:** `JsonUtility` throughout. `GameCard.ToSavedCard()` gives a compact, complete card state (position, stack parent, equipment and worker holders, timer, status effects, extra data). That makes it a natural network format. `WorldManager.GetSaveRound()` / `LoadSaveRound()` handle whole-run snapshots.
- **Simulation:** card timers (`GameCard.StartTimer` / `UpdateTimer`), moon ticks and month end all scale by `WorldManager.TimeScale`. Patching `TimeScale` to 0 and blocking `CreateCard`, `DestroyCard`, `ChangeToCard`, `CreateBoosterpack` and the timer methods freezes a client completely.
- **Input:** `WorldManager.Update` starts a drag on `DraggingDraggable`. The private `DropCard()` either calls `NearbyCardTarget.CardDropped` or `CheckIfCanAddOnStack`. A click is a drag released under the threshold, routed through `Draggable.StopDragging` → `Clicked()`.
- **Cutscenes** are coroutines that wait on `WorldManager.ContinueClicked`. Multiple-choice answers set `ContinueButtonIndex` (see `CutsceneScreen.CreateMultipleOptions`).

## What we changed

The architecture is a host-authoritative mirror:

- **Host:** runs the real game. At 15 Hz it sends a gzip-compressed delta containing every card whose `ToSavedCard()` JSON changed, plus globals (moon timer, speed, board, quests and ideas found, boosters, cutscene text and buttons). Every second it sends a keyframe with all cards. A new joiner gets a full snapshot: the host's save with the live round inserted as `LastPlayedRound`.
- **Joiners:** load the snapshot through the game's own `LoadPreviousRound()` under a temporary save id that is never written to disk. Their simulation is frozen with Harmony patches. Incoming cards are created with `IsLoadingSaveRound` set, so they don't trigger quests or magnets, and are then matched by `UniqueId`. Drags, drops (with a key for the target sell/buy box), clicks, sells, speed changes and cutscene button presses are forwarded to the host, which replays them through the game's own methods.
- **Transports:** TCP for LAN/IP, and Steam lobbies with `SteamNetworkingMessages`, using Steam's game-invite "Join game" (`+connect_lobby`).

Problems found along the way:

1. **`JsonUtility` drops nested classes from a mod assembly.** A `[Serializable]` class defined in the mod and nested in another came back as `null`, while game types such as `SavedCard` nested fine. Fix: serialize the mod's own classes as separate JSON strings and keep nested data in plain lists.
2. **An unfocused Unity window ran at 2 fps under Proton/XWayland.** Fixes: `Application.runInBackground = true`, plus turning vsync off while the window is unfocused during a session. On the test machine the 2 fps persisted, but that machine was heavily loaded by other jobs.
3. **The client's speed change was overwritten before it was noticed.** Incoming host state was applied in the same frame. Fix: check for local speed changes before polling the network.
4. **A host cutscene blocked all input.** For example, "New Pack Unlocked!" waits for Continue. Fix: send the cutscene text and buttons to joiners, and let any player press them.

## QA

- `test/run.sh` builds the mod, copies it into two separate Proton prefixes (separate saves) and launches two game copies. The bots start with `COOP_TEST=host` and `COOP_TEST=client`. The host bot starts a new run, spawns test cards and hosts over TCP. The client bot joins and drives the game through the real input paths: the patched `DropCard` and `Clicked`.
- Assertions: snapshot loads; card count matches; starter pack mirrored; clicking the pack spawns cards; villager stacks on berry bush; harvest timer runs on the mirror; berry appears; selling gives a coin; buying a pack with coins works; a host-side card move is seen; a speed change reaches the host; the client presses a cutscene Continue.
- Last result (2026-10-17): **13/13 pass.**

## How to play

See the mod's README. In short: copy the `stacklands_coop` folder into the Mods folder on every PC. The host starts a run and presses F8 → Host. Joiners stay on the main menu and either accept the Steam invite or press F8 → Join IP.

## Known issues

- The Steam transport is untested. It needs two accounts; LAN/IP is tested.
- Combat animations and some effects (screen shake, wobble, particles) show only on the host. Joiners see the results.
- Cities DLC energy and sewer connectors are not synced.
- Only the host can travel between boards; joiners follow.
- All players should run the same mods.

## Next steps

- Test Steam lobbies with two accounts.
- Sync Cities connectors (`CardConnector`) and the combat/attack animation events.
- Play a full run (mainland → island → forest) in co-op to reach `verified-playthrough`.
