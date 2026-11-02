# Making a StarCraft II campaign co-op-ready

What the SC2Coop launcher does to a campaign by itself, what you (the map author) still have to decide or edit, and how to
check a new campaign. Based on porting WoL@Kit campaigns (2026-10). Run the checker first:

```
SC2Coop-console.exe campaign-check "<campaign folder>" report.md
```

It lists, per map, every script call aimed at player 1 that the launcher does **not** already handle.

## How co-op works (one paragraph)

The host is player 1 as usual. The buddy joins as an extra player (a free slot, e.g. 6-14) that shares control, vision and
spending of player 1's army ("shared army" mode). Each player walks around their own copy of the hub (story map); a mission
pick starts the mission for both through StarCraft II's API in multiplayer. Campaign progress is the host's banks; the
launcher copies them to the buddy and back around every mission.

## Done automatically by the launcher (no map edits)

| Area | What the launcher does | Why |
|---|---|---|
| Guest slot | Copies player 1's MapInfo record into a free slot | The map needs a second human player |
| Shared army | Alliance with shared control, vision and spending; re-applied every second | Missions change alliances as they go |
| Guest input | Click bridge (selection, orders, targeting, build placement) and camera keys | The API drops a joining player's world clicks |
| Objectives | `ObjectiveSetState` / Kit's `libWCMI_gf_ZS_SetMissionObjectiveState`: completed/failed are shown as a name suffix; the engine state stays active | A completed or failed objective makes the API end the game for both players |
| Cutscene camera | 13 camera functions aimed at player 1 also run for the guest | Both players see the same shots and input lock |
| Pings | Pings for player 1 also go to the guest | Objective pings were host-only |
| Skipping | Skippable scenes need one player's Esc instead of everyone's | Required count 0 = every player |
| Mission end | Victory/defeat through the campaign library ends both games; banks synced; both hubs reopen | |
| Next map | `GameSetNextMap` cleared | The launcher decides what loads |
| Hub | Mission launches (`libWoLC_gf_LaunchMission`) recorded for the launcher; debugger window kept closed | |
| Difficulty, colors, names | Set per mission from the hub pick and launcher settings | |
| Test commands | `-cooplaunch <mission>` (hub), `-coopwin`, `-coopwinnow`, `-cooplose`, `-coopobj <objective> [fail]` (missions) | Skip ahead while testing |

Also automatic: setup check (same launcher, maps, mods on both PCs), crash recovery (bank autosave before each mission),
loading curtain between games.

## Needs a decision or an edit by you

### Shared army mode (current): nothing required, but know these

- **Host-only input events.** Triggers on player 1's input (`TriggerAddEventButtonPressed(t, 1, ...)`, key presses, chat,
  unit selection) only react to the host. Tutorial prompts like "select the marines" wait for the host. Mission choice
  buttons (e.g. a dialog to pick a side) are the host's to press. If the buddy should be able to do these, add the guest
  player to those events (or use `c_playerAny` and check `EventPlayer()`).
- **Forced selection** (`UnitSelect(u, 1, ...)`) only changes the host's selection.
- **Difficulty.** Two players on one army make missions easier. Consider bumping enemy difficulty or wave sizes.
- **Player-left triggers.** Most missions have victory/defeat cheat triggers on `TriggerAddEventPlayerLeft(..., c_playerAny, ...)`.
  The launcher ends both games when either leaves, so these don't matter today. Check new ones.

### Separate armies (planned mode): real map work

The checker's "Matters for separate armies" section is the to-do list:

- **Units of player 1** (counts, ownership, spawns). Objectives like "train 10 marines" or "bring units to X" count only
  player 1. Decide per objective: count both players, or keep it the host's job.
- **Resources and supply of player 1** (starting minerals, rewards).
- **Tech of player 1** (unlocks, upgrades, research from the hub). The guest needs the same calls for their slot, or their own
  tech from their own banks (separate-upgrades mode).
- **Spatial content** you have to build in the editor: a second start location, base space and starting units for the guest.
  This can't be generated.

**Design (decided 2026-10-21).** A mission opts in by having **two human (user) players** in its map. Only then does the
launcher show the "separate armies" choice and its settings. A normal map (one human) always plays as a shared army with
shared upgrades. In a two-human map:

| Who does it | What |
|---|---|
| Map author | The second player's slot, start location, base space, starting forces, and the difficulty/balance for two armies |
| Launcher (automatic) | The guest takes the author's second slot (no cloned slot). Player-1 objective checks and ownership tests also count the second player; tech unlocks for player 1 are mirrored |
| Setting: mission-given units (rescues, reinforcements) | Discoverer (default; the player whose units triggered it), split/alternated, or duplicated |
| Setting: money the mission gives | Duplicated (default) or split |
| Setting: credits, research, upgrades (tied to the hub sync mode) | Shared (both use the host's progress) or separate (each player's own campaign progress and hub purchases) |

**Built (2026-10-21, `SeparateArmies.cs`).** What the map needs:

- A second player with control **User** in the map's player settings (any slot; the first User slot after player 1 is the
  guest's), a race, and the units/structures you place for it. A start location is optional: after the opening the guest's
  camera goes to its own units. Name it in the script if you like (`gv_pLAYER01_USER2 = 14;`); the launcher doesn't need it.
- Nothing else. A two-human map is only played in co-op, so no solo fallback is needed; one-human maps stay shared army.

What the launcher does with such a map:

- The guest joins in that slot with that slot's race (no cloned slot).
- Alliances: everything player 1 has with the other players (hostility, vision, help, chat, defeat, pushable) is copied to
  the guest every second; player 1 and the guest are allies with shared vision (no shared control or spending).
- Counting: player-1 unit groups and ownership tests count both armies, so "bring units to X", rescues and "all your units
  are dead" (defeat only when both armies are gone) work for either player. If your script already counts the guest's
  units itself (any `UnitGroup(..., <guest>, ...)`), this is skipped for the whole map.
- Money and tech: every `PlayerModifyProperty*` and `TechTree*` setter for player 1 also applies to the guest (starting
  minerals, unlocks, research); the campaign's armory upgrades are applied to the guest at start. Launcher setting
  "Mission money": both get it (default) or split it (only mission income once under way: half each).
- Resource pickups: Liberty pallets (game data: the pallet's `PickupMinerals100`/`PickupGas100` ability pays whoever walks
  over it), other data pickups found in the map's or its mods' ability/effect data, and Kit's scripted
  `libCamp_gf_DestroyCacheAndGivePlayerResources` caches: the other player gets the same amount, or half each with "split it".
- Units the mission gives player 1 once under way (`UnitSetOwner`, `libNtve_gf_RescueUnit`, `UnitCreate`,
  `libNtve_gf_CreateUnitsAtPoint2`, `libNtve_gf_CreateUnitsWithDefaultFacing`): launcher setting "Units the mission gives": who found them (default; the
  player whose units are nearest), take turns, or both get them. Heroes, and anything given before the mission is under
  way or during a cinematic, stay player 1's. Vision of a unit shared with player 1 is shared with the guest too.
- Not yet: separate credits/research/upgrades from each player's own hub (both use the host's campaign progress).

**Planned: hub purchases and progress (decided 2026-10-22, not built).** One setting for two-faction (separate armies)
campaigns, covering mission money, Hyperion armory upgrades and research together: "player 1's bank is what counts" (both
use the host's progress) or "each player's money, upgrades and research are tracked in their own bank". One-army (Archon)
play always uses the host's bank. Open: what the buddy's hub allows in Archon play. Either block purchases there, or let
the buddy spend the shared (host's) credits with a clear notice. Vanilla WoL has no refunds, so an accidental purchase
can't be undone; Kit campaigns refund for free. Today the buddy's purchases only count if the buddy starts the mission.

The settings row appears in the launcher only when the campaign folder has at least one such map. `SC2_COOP_SEPARATE=1`
still tries the same thing on a normal one-human map (cloned slot plus copies of player 1's starting units).

### Other campaigns

- **WoL@Kit-style campaigns** (story map TStory01 + `libWoLC_gf_LaunchMission`): should work as is; run the checker and the
  smoke test (below).
- **Blizzard's own campaigns and other mods:** the hub launch hook and bank list depend on the campaign library. Progress lives
  in banks too, but under different names and possibly signed. Expect to add a hub adapter for that library.
- **Vanilla-based WoL mods: supported** (2026-10-21, tested on UED 3.0.3). The launcher tells the libraries apart by the hub's
  launch calls and the mission's library calls; nothing to set. Hub: `libCamp_gf_LaunchMission` is replaced by the
  launcher's copy of it (A/B choice, briefing, PrepareForMission, game over) that records the final pick; `-cooplaunch
  TRaynor01` works (after the mission briefing plays). Missions: `-coopwin/-coopwinnow/-cooplose` use libCamp's names,
  `-coopobj <objective id> [fail]` takes the engine objective id. Not yet: Blizzard's Mission Archives replays and the
  guest's campaign tech in separate armies (see the table).
- Assessment (2026-10-21: UED 3.0.3, WoL Nod 1.1, RaptorX's 3-Race Nightmare, WoL+ CCM): all keep
  Blizzard's hub and campaign library (`libCamp`, Liberty/LibertyStory), with the same five `libCamp_gf_LaunchMission(mapID,
  simulation, difficulty)` calls in the hub script, so one "vanilla" adapter covers them. Kit's libraries are a renamed copy of
  libCamp, so most hooks map one to one:

  | Launcher hook (Kit) | Vanilla | Notes |
  |---|---|---|
  | `libWoLC_gf_LaunchMission(name, ...)` | `libCamp_gf_LaunchMission(mapID, ...)` | Mission id is an enum; file name from `libCamp_gv_tS_MapFile[id]`. A/B choice missions (THanson03, TTosh03, TValerian02) pick inside the library. |
  | `libWCMI_gf_EndCampaignMission` | `libCamp_gf_EndCampaignMission` | Same: victory shows the score screen, defeat ends the game. |
  | `libWCMI_gf_ZS_SetMissionObjectiveState` | none needed | Vanilla missions call `ObjectiveSetState` themselves (already rerouted); libCamp only stores a number. |
  | `libWCMI_gf_GameOver/GameIsOver`, `libWCMI_gt_WM_VictoryContinue` (test commands) | `libCamp_gf_RunMissionVictorySequence`, score-screen continue | Needs its own game-over flag. |
  | Banks (BankList.xml) | TCampaign, CampaignPS, TStory01, one per mission | Plain unsigned player-1 banks; the existing sync works as is. |
  | `libWoLC_gf_WC_ApplyTech(player)` (separate armies) | `libCamp_gf_TS_UpdateTechStates()` | Player 1 only; the guest's campaign tech needs another way (open). |
  | Mission Archives (Kit library) | Blizzard's archives | Not looked at yet. |

  Runtime (2026-10-18): Blizzard's TStory01, repacked, runs as an API game (intro, mission select, launch); `RequestGameInfo`
  on it crashed SC2.

  Missions the launcher starts are not transition maps (`GameIsTransitionMap()` is false), which both libraries read as
  "started from the editor or a restart" (2026-10-22, credits jumping between missions):
  - libCamp's `libCamp_gf_PurchaseStorymodeTech()` (called by every mission) then runs the debug "buy average tech": credits
    recomputed from the expected rewards minus purchases, mission progress set to the expected path, and the mission saved
    as the last successful one (a defeat still paid its reward in the hub). The launcher removes the call (from the hub it
    does nothing). TRaynor01-03 still get the same debug purchase inside `libCamp_gf_LoadCampaignData`; the defeat dialog
    saves the defeat (`libCamp_gf_PrepareForStoryMode(defeat)`) so the hub pays nothing for it.
  - Both libraries load the mission's restart snapshot (vanilla: the mission's own bank; Kit: `<mission>|...` sections of
    the campaign bank, `MissionSectionExist`) when one exists, which undid purchases made since an earlier attempt. A start
    from the hub drops the snapshot first (vanilla: the launcher deletes the bank; Kit: the hub clears the flag); a Retry
    keeps it.
  - The hub pays the last mission's reward every time it opens until a save marks it paid, so the host's hub saves 10 s
    after opening and again before it closes without a launch.

## Testing a ported campaign

1. `campaign-check` and read the per-map list.
2. Smoke test every mission on one PC (local mode): `SC2_COOP_TEST_LAUNCH=<mission,mission,...>` launches each from the hub,
   `SC2_COOP_TEST_WIN=45` wins after 45 s. Look for missions that don't end, errors, or the guest's API status leaving 3.
3. Objectives: `-coopobj <objective>` in a mission; both players must stay in the game.
4. Play a few missions properly on two PCs.
