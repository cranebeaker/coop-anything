# XCOM 2: War of the Chosen: full-campaign co-op design research

Research only, 2026-11-11. Nothing here was built or run; the game was not launched.
Builds on the existing prototype and notes in this folder ([README.md](README.md),
[NOTES.md](NOTES.md)) and the mod repo
[EvilMask587/xcom2-wotc-coop-campaign](https://github.com/EvilMask587/xcom2-wotc-coop-campaign)
(MIT, last push 2026-10-25).

Sources read:
- WOTC SDK `Development/SrcOrig` (Steam app 602410, buildid 3075887), written `<SDK>` below.
- X2WOTCCommunityHighlander at commit `1cba26f17f3d1a7a426df097f1ec4ea0d08b7b8a`
  (2026-11-06), written `CHL` below. Line numbers are from that commit.
- Team Dragonpunk Tactical Co-Op source (GPL-3.0, last commit 2017-03-30, vanilla only).

Firaxis code is summarised and cited by file and line, never copied.

## The goal, split into parts

| # | Part | State today |
|---|---|---|
| 1 | Shared Avenger (both players see and change one campaign) | **Prototype exists** (baton model, 11/11 two-instance tests, 2026-10-25) |
| 2 | Each soldier belongs to a player | Not started. Storage found (below) |
| 3 | Squad select splits the slots between players | Not started. Squad size and spawn limits found |
| 4 | Each player controls only their own soldiers in missions | Not started. WOTC turn groups look like the best base |
| 5 | Joiner gets the host's mods automatically | Not started. Needs a helper outside the game |
| 6 | Visual mods, units and voices show correctly for both | Follows from 1 and 5 (below) |

## 1. Shared Avenger

Already prototyped; see README.md. What matters for the other parts:

- One editor at a time (the "baton"). Both sides creating objects at once hands out the
  same ObjectID twice (tested there), so any new feature that creates state objects must
  run on the baton holder or the host.
- Every frame goes to the partner with `SendMergeGameState`; the joiner gets the whole
  history with `SendHistory` on join.
- Squad select (`UISquadSelect`) is already one of the screens that asks for the baton
  (their `CoopEditGate`, NOTES.md "edit screens take the baton").

## 2. Unit ownership

**Observed:**
- Units can carry named values: `XComGameState_Unit.SetUnitFloatValue(name, value, cleanup)`
  and `GetUnitValue` (`<SDK>/XComGame/Classes/XComGameState_Unit.uc:1239-1240`). The
  cleanup enum has `eCleanup_Never` next to `eCleanup_BeginTurn` and
  `eCleanup_BeginTactical` (`X2TacticalGameRulesetDataStructures.uc:509-514`).
- Any state object can carry component state objects: `AddComponentObject`,
  `FindComponentObject` (`XComGameState_BaseObject.uc:69-86`).
- Tactical control is per `XComGameState_Player`: `XComTacticalController.ControllingPlayer`,
  and remote-ness is `XGPlayer.IsRemotePlayer` comparing the player state's net id with the
  local one (`CHL XGPlayer.uc:952-964`). Both co-op players share the one XCom player.

**Derived (proposal, not tested):**
- Tag each soldier with `UnitValue 'CoopOwnerSlot'` (0 = host, 1 = guest), cleanup
  `eCleanup_Never`. It lives on the unit, so it travels into and out of missions with it
  and is saved in the history like everything else.
- Keep a singleton `XComGameState_CoopCampaign` for the player roster: slot -> last known
  name and net id, ownership policy settings. Slots, not Steam ids, own soldiers, so a
  campaign can be continued with a different friend.
- Policy for soldiers nobody owns yet (recruits, rewards, rescued soldiers): unowned until
  a player puts them in one of their squad slots or claims them in the Armory. Optional:
  the guest imports soldiers from their own Character Pool, owned by the guest
  (Dragonpunk had a soldier import).
- Ownership changes only on the baton holder, as an ordinary frame.

**Unknown:**
- Whether `eCleanup_Never` values survive the strategy -> tactical -> strategy round trip
  unchanged (the round trip copies units; `eCleanup_BeginTactical` exists, so values are
  touched at mission start). Test: tag a soldier, run a mission, read the value back.
- Whether a component object on a unit is carried into the tactical start state.

## 3. Squad select: splitting the slots

**Observed:**
- Squad size: `GetMaxSoldiersAllowedOnMission` (`CHL X2StrategyGameRulesetDataStructures.uc:1933`)
  starts from mission `MaxSoldiers` or the config `m_iMaxSoldiersOnMission`, adds one per
  squad-size unlock (`SquadSizeIUnlock`, `SquadSizeIIUnlock`) and for the
  `ExtraSoldier_Intel` tag, then applies sitrep limits. Shipped value:
  `m_iMaxSoldiersOnMission=4` (`XCom2-WarOfTheChosen/XComGame/Config/DefaultGameData.ini:991`).
  So vanilla is 4 to 6 soldiers (7 with the Intel option).
- Spawn space: `XComGroupSpawn.GetValidFloorLocations` has a Highlander hook
  (`GetValidFloorSpawnLocations` DLC hook and `CHHelpers.SPAWN_EXTRA_TILE`, Issue #18,
  `CHL XComGroupSpawn.uc:54-75`), and `HasValidFloorLocations` checks the floor points
  against `XComHQ.Squad.Length`. Larger squads are a supported Highlander case.
- The squad is `XComGameState_HeadquartersXCom.Squad`, indexed by slot; `UISquadSelect.ChangeSlot`
  writes `XComHQ.Squad[m_iSelectedSlot]` (`UISquadSelect.uc:1273-1297`). WOTC also has
  `AllSquads` for missions with several squads (`XComGameState_HeadquartersXCom.uc:48`).
- Squad select fires `EnterSquadSelect` and size events (`OnSizeLimitedSquadSelect`,
  `OnSuperSizeSquadSelect`; `CHL UISquadSelect.uc:149-180`).
- robojumper's Squad Select (`robojumperSquadSelect_WotC`, Workshop 1122974240), a widely used
  mod, replaces the vanilla squad select screen.

**Derived (proposal):**
- Slot owner per index in `XComGameState_CoopCampaign` (default: first half host, second
  half guest; odd sizes alternate per mission). A player can only put their own or
  unowned soldiers into their own slots; the other player's slots show as locked.
- For 4 + 4, raise `m_iMaxSoldiersOnMission` in the mod's config (or as an option) and
  rely on the Highlander spawn hook. Keep 3 + 3 (vanilla 6) as the default so maps
  without room still work.
- Hook the slot lock in both the vanilla screen and robojumper's (it is popular). Read its source before choosing the hook.

**Unknown:**
- Which maps or mission types fail to spawn 8 (Avenger defense, Chosen stronghold,
  `AllSquads` missions such as the final mission).
- How robojumper's Squad Select exposes slot changes to other mods.

## 4. Tactical: each player controls their own soldiers

**Observed:**
- WOTC turn order is a list of **initiative entries**, not just players:
  `BattleData.PlayerTurnOrder` holds `XComGameState_Player` and `XComGameState_AIGroup`
  references (`CHL X2TacticalGameRuleset.uc:45-46`, `BuildInitiativeOrder` at 1028).
- At mission start all XCom units go into ONE group (`StartStateInitializeSquads`,
  `CHL X2TacticalGameRuleset.uc:994-1025`).
- Groups of the same team are ordered by `XComGameState_AIGroup.InitiativePriority`,
  commented "in what relative initiative order should this group be processed compared
  to other groups for the same player. Higher # = process initiative later"
  (`CHL XComGameState_AIGroup.uc:66`; `InsertGroupByInitiativePriority`,
  `X2TacticalGameRuleset.uc:1129-1153`).
- In the unit-actions phase only units of the group that has initiative can act ("no
  units act when a player has initiative; they only act when their group has
  initiative", `ActionsAvailable`, `CHL X2TacticalGameRuleset.uc:4987-5040`).
- Events at group and player turn boundaries: `UnitGroupTurnBegun`, `UnitGroupTurnEnded`,
  `PlayerTurnBegun`, `PlayerTurnEnded` (`CHL X2TacticalGameRuleset.uc:4715, 4729, 4880, 4955, 4980`).
- Unit selection and Tab cycling: `XComTacticalController.Visualizer_SelectUnit`
  (`<SDK> XComTacticalController.uc:189`) and `Visualizer_CycleToNextAvailableUnit` (531),
  which cycles over all of the controlling player's units, not the active group.
- PvP sync model: the active player is the authority and sends `SendPartialHistory` at the
  end of its turn; the other side watches a replay (NOTES.md, SDK reading).
- Dragonpunk instead subclassed `X2TacticalGameRuleset` (about 4,000 lines, vanilla) and
  rewrote the turn states; it never knew WOTC's groups or `TurnPhase_StartTimer`,
  `PostCreateTacticalGame`, `CreateChallengeGame`.

**Derived (proposal):**
- At mission start, split the XCom group into two groups by `CoopOwnerSlot`: host group
  (priority 0) and guest group (priority 1). The game's own initiative code then gives
  the host's soldiers a turn, then the guest's, then the aliens. This reuses WOTC code
  instead of rewriting the ruleset like Dragonpunk did.
- The machine whose group has initiative is the authority; at `UnitGroupTurnEnded` it
  sends a partial history and hands over (the PvP model, at group level instead of
  player level). The aliens, the Lost and the Chosen run on the host.
- Gate `Visualizer_SelectUnit` and Tab cycling so a player can only select their own
  units of the active group (other units stay viewable).
- Optional later mode: both groups active at once with an action-level baton (one action
  at a time, either player). Harder: same ObjectID problem as the strategy layer.

**Unknown (each needs a test):**
- Does anything assume one XCom group: squad concealment, Bonds, Reaper and Skirmisher
  abilities (Skirmisher's interrupt uses `InterruptInitiativeTurn`), evac, mission timers
  (`TurnPhase_StartTimer` ticks per player turn or per group?), the "End Turn" button
  (does it end only the group or the whole XCom turn, `bSkipRemainingTurnActivty`)?
- Which group gets units that join mid-mission (rescued VIPs, reinforcements, mind control).
- Whether `XGPlayer.IsRemote` can stay "both local" (one XCom player state) while the
  group owner decides input.

## 5. Joiner gets the host's mods

**Observed:**
- Script can read the mod list and hashes: `Helpers.GetInstalledModNames()`,
  `GetInstalledDLCNames()`, `NetGetInstalledModsHash()`, `NetGetMPINIHash()`,
  `NetGetVerifyPackageHashes()` (`<SDK> Helpers.uc:90-104`). PvP refuses a join when any of
  these differ (`XComOnlineEventMgr.CheckInviteGameVersionMismatch`, 3593-3618).
- UnrealScript has no Steam Workshop API: `OnlineSubsystemSteamworks` has no
  subscribe/UGC functions (classes: Auth, Game, XCom game, Lobby, Subsystem, TitleFile).
- Steamworks `ISteamUGC::SubscribeItem`: "Subscribe to a workshop item. It will be
  downloaded and installed as soon as possible." No confirmation step is documented
  ([Steamworks docs](https://partner.steamgames.com/doc/api/ISteamUGC)).
- Workshop layout on the test PC: `steamapps/workshop/content/268500/<WorkshopId>/`,
  exactly one `<ModName>.XComMod` per folder (every folder checked). The `publishedFileId=` line in
  the `.XComMod` can be `0` (seen on `robojumperSquadSelect_WotC`), so the folder name is
  the reliable id.
- Mods are enabled through `ActiveMods` in `XComModOptions.ini`, regenerated from
  `DefaultModOptions.ini` (NOTES.md, test runner). Script packages load at startup.

**Derived (proposal):**
- In the game: the host sends its `GetInstalledModNames()` list and the hashes in the
  join handshake (a remote command before `SendHistory`). On mismatch the guest shows
  which mods are missing or extra and stops before receiving the history.
- Outside the game, a small "CoopSync" helper (same repo, runs before the game):
  1. On the host: map each active mod name to its Workshop id from the workshop folders,
     and export the list (mod name, Workshop id, mod config files).
  2. On the guest: compare, `SubscribeItem` the missing ids through the Steamworks API
     (game's own `steam_api64.dll`, app 268500, Steam running), wait for
     `GetItemState` installed, then write `ActiveMods` to match the host. Unsubscribing or
     disabling extra mods only after asking.
  3. Restart the game (mods can't load into a running game).
- Mods not on the Workshop (local `Mods` folder) can't be fetched; list them by name.
- Gameplay config must match too, not just the mod list: mod settings (MCM and other user
  `XCom*.ini` files) change rules. The helper can copy the host's mod config files.

**Unknown:**
- What `GetInstalledModNames()` returns exactly (`.XComMod` name, `DLCIdentifier`, or
  title) and whether it means installed or active.
- What happens when a received history contains a state object whose class comes from a
  mod the guest lacks (crash, or the object dropped?). This decides whether the check is
  mandatory before `SendHistory` (assume yes).
- Whether a helper calling `SteamAPI_Init` as app 268500 while the game is closed is
  enough for Steam to download items promptly.

## 6. Visuals, units and voices

**Observed:**
- A soldier's look is stored as template names in `kAppearance` (`TAppearance`), including
  `nmVoice`; voices resolve on each machine through
  `X2BodyPartTemplateManager.FindUberTemplate("Voice", name)` and load the template's
  archetype (`<SDK> XComHumanPawn.uc:1050-1066`). If the template is missing, nothing
  plays; there is no fallback. Tactical preloads voices from `nmVoice` (409-412).
- Defaults when generating soldiers fall back to `MaleVoice1_English_US` /
  `FemaleVoice1_English_US` only at creation (`XGCharacterGenerator.uc:916-926`), not
  when a template is missing later.

**Derived:**
- The history already carries every soldier's `kAppearance`, so once both players have
  the same mods (part 5), the guest sees the same heads, armour, custom units and hears
  the same voices with no extra sync.
- A join-time content check is cheap: for every crew unit, look up each `kAppearance`
  part and `nmVoice` with `FindUberTemplate`, and report units with parts that don't
  resolve on the guest. That turns "invisible head / silent soldier" into a clear
  "missing mod X" message.

## Risks, ranked

1. **Mod mismatch in the history** can crash the guest before any check runs; the
   handshake must happen before `SendHistory`.
2. **Group split side effects** in tactical (concealment, timers, End Turn, interrupts,
   mid-mission units): unknown until tested.
3. **Squad sizes above 6** on small spawn zones and special missions.
4. **robojumper's Squad Select** and other UI mods replacing the screens we hook.
5. **Steam downloads** from a helper: timing and failure cases; local-only mods.
6. Everything in the README's Known issues (two PCs, Steam sockets, host dropping).

## First experiments (in order, each a scripted test on two instances)

1. Ownership tag round trip: set `CoopOwnerSlot` on two soldiers, run a mission start and
   return, read the values.
2. Group split, single player: at mission start split the XCom group by tag, check the
   turn order is host group -> guest group -> aliens, and that End Turn, concealment and
   the mission timer still behave.
3. Selection gate: only the active group's units are selectable and Tab-cyclable.
4. Group handoff over the network: partial history at `UnitGroupTurnEnded`, the other
   instance takes input.
5. Mod handshake: two instances with different `ActiveMods`, the guest refuses before
   `SendHistory` and names the difference.
6. Helper dry run: host list -> Workshop ids -> diff on the guest (no subscribing without the
   tester's explicit consent: it changes their Steam account's subscriptions).

## Target product (decided 2026-11-11)

| Area | Decision | What it changes in the current code |
|---|---|---|
| Players | Up to 4 (host + 3 guests); 2 first | Player slots 0-3 instead of host/guest. A guest's slot must be assigned by the host on join (today: client connection = slot 1). Unknown: whether `XComGameStateNetworkManager` serves more than one client (PvP only ever has two). |
| Mission turns | Host's soldiers, then each guest's in slot order, then the aliens | One XCom initiative group per player, `InitiativePriority` = slot (today: two groups). |
| Waiting players | Watch with a free camera, no input | Input off outside your own group's turn; the selection gate stays on. |
| Avenger and Geoscape | The host makes every campaign decision; guests manage their own soldiers (loadout, promotion, customisation) | The baton model shrinks: guests never hold the campaign baton, only per-soldier edit rights for soldiers they own. The edit-screen list splits into host-only screens and own-soldier screens. |
| Squad slots | The host assigns slots to players for each mission | Slot owners are set by the host in squad select (UI still to build); the fixed half/half split stays only as the default. |
| Guest soldiers | The host gives existing soldiers to guests | Ownership is assigned by the host (Armory action), not claimed by placing a soldier in a slot. |
| Ownership changes | The host can reassign any soldier at any time | Same host-only Armory action; no permanent ownership. |
| Disconnect mid-mission | The host takes over the guest's soldiers; the guest gets them back on rejoin | "Effective controller" = owner if connected, else the host; the group stays the guest's. |
| Saves | Host only; Ironman allowed | Matches the prototype (client saves blocked). Ironman on the host still needs a test. |
| Mods | Auto-subscribe the guest to the host's mods, enable the same list, restart | CoopSync `subscribe` + `write-modoptions`; the in-game join check stays as the gate. |
| Joining | Steam invite and direct IP | Steam sockets (`steamsockets` URL option) are untested; direct IP works on one PC. |
| Communication | Map pings and text chat | Pings: new (remote command with a tile). Chat: the game's MP chat panel (`UIMultiplayerChatManager`) may be reusable. |

### Game modes (decided 2026-11-13; scenario mode replaced horde defense 2026-11-16)

The co-op menu offers three modes. The host picks one; guests join whatever the host runs.

| Mode | What it is | Status |
|---|---|---|
| **Co-op campaign** | The regular campaign, unchanged in balance, shared by up to 4 players: everything in the table above. The "Host co-op" button can also turn an existing single-player campaign into a co-op one (it adds the co-op data). | Built; tested on one PC (see results below). The mode picker in the menu is not built yet. |
| **Co-op balanced campaign** | A campaign tuned for co-op: **4-6 soldiers per player**, and **enemies scaled to the number of players**. More changes to be decided later. | Planned. Starts after the co-op campaign works on two PCs. |
| **Scenario mode** | Single co-op missions without a campaign: pick a game stage, build operators from a point budget, play a mission with several objectives that pull the squad apart. See below. | Planned (replaced horde defense, 2026-11-16). Starts after the balanced campaign. |

Order of work: finish the co-op campaign first (two PCs, Steam, content coverage), since both other modes
reuse its join, turns, ownership and saves.

Open questions for the balanced campaign (for the owner to decide later):

- Whether "4-6 soldiers per player" is a fixed number set by the host, a choice per player, or grows
  with the game's squad size upgrades.
- How enemies scale: more pods, bigger pods, stronger enemies, or a mix; and whether it changes when a
  player drops mid-mission.
- Whether the mode is picked at campaign start only (new game) or can be switched on an existing campaign.
- Which other changes come with it (economy, recruits, Avenger build times, Chosen).

What has to be checked before building it (guesses, not checked in the game or the SDK yet):

- With 4 players at 6 soldiers each, a mission has 24 soldiers. Unknown: whether squad select, the
  mission start spawn area and the turn order UI handle that many; the vanilla squad select shows at most
  6 slots.
- Enemy scaling probably goes through the mission's encounter and pod settings (the mission schedules in
  the game's config), or through added reinforcements. Which one works without breaking story missions is
  unknown.

Found in the SDK source (2026-11-14, read only, not run): the game's sitreps already carry the two knobs as
effect templates, which a mod can add to every mission of the balanced campaign as a hidden sitrep.

- Squad size: `X2SitRepEffect_SquadSize` (`MinSquadSize`, `MaxSquadSize`, `SquadSizeAdjustment`), read by
  `UISquadSelect` when it builds the slots (`UISquadSelect.uc:1818`).
- Pod size: `X2SitRepEffect_ModifyPodSize` (`PodSizeDelta`, `MinPodSize`, `MaxPodSize`, per team; native). The
  game has `PodSizeIncreaseByTwoEffect` (`PodSizeDelta = 2`) and three that shrink pods
  (`X2SitRep_DefaultSitRepEffects.uc:363-400`).
- Also there: `X2SitRepEffect_ModifyAlertLevel`, `X2SitRepEffect_ModifyForceLevel` (enemy count and tier through
  the mission schedules; unchecked how much), `X2SitRepEffect_ModifyXComSpawn`.
- The bot scenario `bigsquad` puts 12 soldiers (6 per player) into a mission by filling the HQ squad directly,
  past squad select; results under "Game content".

#### Scenario mode (decided 2026-11-16, replaces horde defense)

Goal (owner): play missions together without committing to a whole campaign, designed around co-op.

- **Game stage:** the host picks early, mid or late game; it sets the enemies the players face and the gear
  and ranks they can buy.
- **Operators from a point budget:** each player builds a pool of operators; higher stats, ranks and better gear
  cost more points.
- **Several objectives per mission:** more objectives than one squad can cover together, so the players have to
  decide how to split up.
- Same co-op layer as the campaign: each player moves their own operators in turn, joins and reconnects the same
  way.

Decided (owner, 2026-11-16):

- **12 operators per mission, split evenly among the players:** 1 player 12, 2 players 6 each, 3 players 4 each,
  4 players 3 each. The squad size is the mode's own, the same for any player count.
- **XCOM soldiers only.** Points buy gear, abilities, stats, and special units: the three faction heroes
  (Reaper, Skirmisher, Templar) and the Psi Operative cost more than a standard soldier.
- **Gear tier follows the game stage**, not points: early game conventional weapons and Kevlar armor, mid game
  magnetic weapons and Predator armor, late game beam (plasma) weapons and Warden armor. Special suits cost extra
  points on top: Spider Suit and EXO Suit (mid), Wraith Suit and W.A.R. Suit (late).
- **The budget scales with the game stage.**
- **Ranks:** each stage has a minimum rank; higher ranks can be bought with points.
- **Utility items** (grenades, medkits, ammo, vests...) are bought with points.
- **Objectives come in sets.** A mission is a sequence of sets; each set has 1-3 objectives running at the same
  time, most with their own turn limit, and the next set starts when the current one ends. Late game has more
  objectives than early game. Example (owner):
  1. Kill the ADVENT officer (5 turns) + capture a zone (6 turns).
  2. Regroup at the zone and defend it for 8 turns.
  3. Set charges at the zone (no limit) + extract (5 turns).
- **Objective sets are random**, with rules for which objectives may not appear together (for example extract and
  kill the officer never in the same set).
- **Failure depends on the objective:** some trigger an instant mission failure when missed, others make the rest
  of the mission harder.
- **No scoring.**
- **Saved operators:** players keep their operators between sessions instead of rebuilding the squad each time,
  and each operator counts the missions completed with it. Each player's operators are saved on their own PC and
  sent to the host during mission setup. Operators are kept when they die in a mission.
- **Difficulty modifiers:** a mix of more enemy HP, more enemies, and enemies from a later game stage appearing.
- **"Harder" after a missed objective** can use the same kinds of change: more enemies, reinforcements, a lost
  advantage. Further ideas (suggestions, not decided): enemy HP or armor up for the rest of the mission, a shorter
  turn limit on the next set, the next set's zone placed farther away, an alerted pod or a Chosen arriving, the
  evac zone offered later.

Item and class names in the game data (SDK source, read only): weapons by tier `AssaultRifle_CV`, `_MG`, `_BM`
(the same suffixes for the other weapon families); armor `KevlarArmor`, `MediumPlatedArmor` (Predator),
`MediumPoweredArmor` (Warden), and the special suits `LightPlatedArmor` (Spider), `HeavyPlatedArmor` (EXO),
`LightPoweredArmor` (Wraith), `HeavyPoweredArmor` (W.A.R.); the faction heroes have their own armor per tier
(`ReaperArmor`, `PlatedReaperArmor`, `PoweredReaperArmor`, the same for Skirmisher and Templar); classes
`Reaper`, `Skirmisher`, `Templar`, `PsiOperative`. Derived: the multiplayer point costs price fixed presets
(class, rank, abilities, loadout); a budget that buys single abilities and stat points needs its own price table.

What the game already has (found in the SDK source 2026-11-16, read only, not run):

- **Skirmish mode** (War of the Chosen, `UITLE_SkirmishModeMenu`, reached from `UITLEHub`) builds a one-off mission
  outside a campaign: map, mission type, biome, difficulty, a tech level, squad and enemies, then a start state
  with a mission site. Its setup is the closest match for "pick a game stage and a mission".
- **Point costs** from the multiplayer squad builder (`UIMPShell_SquadEditor`): every unit preset has a cost
  (`X2MPCharacterTemplate.Cost`, per character or soldier class and rank, with its abilities and loadout) and every
  item has `MPCost`; `XComGameState_Unit.GetUnitPointValue` adds them up. The PvP budget screen
  (`UIMPShell_SquadCostPanel`) shows the total. A point-bought operator pool can reuse these numbers, or replace
  them with its own table.

Open questions (owner):

- Budget: how many points; the same budget per operator, so each player's share is the same whatever the player
  count, or one budget per player; does it grow with the game stage.
- Prices: what one ability, one stat point (aim, HP, mobility, will, dodge...) and each special unit cost; are
  ranks bought (a rank brings its ability picks), or abilities one by one; caps per stat.
- Utility items (grenades, medkits, ammo types, vests): included in the gear tier, or bought with points.
- Prices and amounts: the budget per stage, what an ability, a stat point, a rank above the minimum, each special
  unit, special suit and utility item cost; caps per stat.
- Objective sets: how many sets and objectives per stage; which objective kinds exist (kill a target, capture,
  defend, hack, rescue, destroy, set charges, extract...); the full list of pairs that may not share a set; whether
  a set's order follows any rule (extract last).
- Failure: which objectives fail the mission when missed, and what "harder" means for the others (more enemies,
  reinforcements, a lost advantage...).
- Saved operators: whether one can be taken into a different game stage, and how its rank and bought items fit
  that stage's rules (cost re-checked, items downgraded to the stage's tier, rank kept above the minimum);
  whether deaths are counted alongside missions; whether the host checks each sent operator against the budget.
- Difficulty modifiers: whether the host picks them per mission, how they combine with the game stage, and how
  much each one adds.
- Saved operator pools and a record of scenarios played, or everything fresh each time.

What has to be checked before building it (unknown so far):

- Running objective sets. In the game a mission type comes with its own map parcels and objective scripting
  (Kismet), one objective chain per mission type. Sets of 1-3 objectives in sequence probably need the mod to run
  its own objective manager (a turn counter and event listeners for kills, units in a zone, interactions) rather
  than each map's scripts. Building blocks found in the SDK (map-script actions, not run): turn-limit display
  `SeqAct_DisplayUISpecialMissionTimer`, objective list `UIObjectiveList` / `SeqAct_DisplayMissionObjective`,
  `SeqAct_MakeObjective`, `SeqAct_SpawnAdditionalObjective` (an objective added mid-mission), evac zone
  `SeqAct_SpawnEvacZone`; an officer to kill exists as `AdvCaptainM1`/`M2`. Unknown: whether a mod can call these
  from script during a mission, and where zones and charge points go on a map that was not made for them.
- Whether the Skirmish start state can be built on a co-op host and sent to guests like a campaign mission (our
  join and mission handoff assume a campaign behind the mission).
- Whether the game stage maps cleanly to Skirmish's tech level and the enemy force level.
- Modifiers through sitreps: the game's sitrep effects already change pod size and force level per mission
  (`X2SitRepEffect_ModifyPodSize`, `X2SitRepEffect_ModifyForceLevel`, see "Found in the SDK source" above), which
  covers "more enemies" and "later enemies" if they can be applied to a scenario mission; an enemy HP modifier
  was not looked for yet.
- Sending operators to the host: a remote command carrying each operator's data during setup, like the mod list
  check already does in chunks (`CoopModCheck`); the host then builds the units into the mission's start state.
- Saving operators: the game's character pool (`CharacterPool` files, outside any campaign save) stores soldiers'
  appearance outside any campaign; whether it can carry the extra data (bought abilities, stats, items, mission
  count) or the mode needs its own file is unknown.
- A 12-operator mission: the `bigsquad` scenario already ran 12 soldiers split between two players (13/13); squad
  select itself shows 6 slots at most, so the scenario mode needs its own operator screen anyway.

## Implementation status (2026-11-11, compiled, not run in the game)

Built on a local clone of the prototype mod (upstream commit `cb1828b`), not published yet.
Everything below compiles against the WOTC SDK (0 errors); none of it has run in the game.

| Part | Code | Checked by |
|---|---|---|
| 2. Ownership | `CoopOwnership` (unit value `CoopOwnerSlot`, cleanup never; unowned soldiers become the slot owner's when placed), `XComGameState_CoopCampaign` (player slots, owner per squad slot, default split first half host), `CoopRole` (local slot: client connection = guest, config override for tests) | compile only |
| 3. Squad slots | `CoopSquadRules`, called from the prototype's `CoopStrategyRuleset.SubmitGameStates` on every local frame: an illegal placement (partner's slot, or partner's soldier) is undone in the pending frame instead of rejecting it, because the game itself rewrites the squad (auto-fill, removing dead soldiers); removals always pass. `UIScreenListener_CoopSquadPicker` closes the soldier picker for the partner's slots and greys out the partner's soldiers (works for vanilla and robojumper's Squad Select: both set `UISquadSelect.m_iSelectedSlot` and open `UIPersonnel_SquadSelect`: robojumper's list item calls `super.OnClickedSelectUnitButton`, and the vanilla one calls `HQPRES.UIPersonnel_SquadSelect`, `UISquadSelect_ListItem.uc:680`). | compile only; scenario `squad` written |
| 4. Tactical turns | `CoopTactical.SplitXComGroup` (guest soldiers into a second XCom group, `InitiativePriority + 1`, `BuildInitiativeOrder` in the same frame) and `X2EventListener_CoopCampaign` on `OnTacticalBeginPlay` (vanilla listener templates register with `ELD_OnStateSubmitted`, `X2EventListenerTemplate.uc:48`, so the split is its own frame submitted right after begin play), off by default (`bSplitXComGroupAtStart=false`) until experiment 2 passes. Selection gate: `CoopTacticalController` (ModClassOverrides on `XComTacticalController`) refuses `Visualizer_SelectUnit` for the partner's soldiers, which also covers Tab cycling (it tries `Visualizer_SelectUnit` per candidate); off by default (`bGateSelection=false`). A scan of a real Workshop setup (over 100 mods) found no other mod overriding `XComTacticalController`, `XComHeadquartersGame` or `XComAutosaveMgr`. Network handoff between the two groups: not written (needs two-instance tests). | compile only; scenario `exp12` written |
| 5. Mod matching (in game) | `CoopModCheck`: sends the installed mod names in chunks of 8 plus the engine's mod and MP-ini hashes; the partner compares, logs `-missing +extra`, answers `CoopModsResult`. Wired into the join flow by `CoopJoin` (off by default, `bGateJoinOnMods`): on a partner joining, the host sends its list; the guest answers match plus the difference; the host sends the history only on a match, otherwise `CoopJoinRefused` and a dialog on the guest naming the missing and extra mods. | compile only |
| 5. Mod matching (helper) | `tools/coopsync/coopsync.py`: `manifest` (active mods -> Workshop id from the folder name, local mods, version fingerprint over scripts, config, localization and content sizes), `diff`, `plan` (subscribe / update / cannot fetch / deactivate), `write-modoptions` (backup first, refuses hard links and the real `Documents\My Games` folders), `subscribe` through the game's own `steam_api64.dll` (`SteamAPI_Init`, `SteamUGC`, `SteamAPI_ISteamUGC_SubscribeItem`, `GetItemState`; only with `--yes-use-my-steam-account`). | 14 unit tests pass; 3 mutants (fingerprint ignores scripts, hard-link check removed, re-subscribes everything) each make tests fail; read-only manifest of a real Workshop folder (over 100 mods): every mod resolved to its id, about 12 s |
| 6. Visuals and voices | `CoopModCheck.AuditContent`: every soldier's 31 appearance fields and `nmVoice` looked up with `FindUberTemplate` (part type names taken from the game's own calls); logs soldiers with parts that do not resolve | compile only |

### Rework for the target product (2026-11-11, compiled, not run)

- Player slots 0-3 (`XComGameState_CoopCampaign.MAX_PLAYERS = 4`, `PlayerCount`); default squad split in
  consecutive blocks (6 slots, 2 players: `000111`; 4 players: `001123`).
- `CoopJoin` gives each joining guest the lowest free slot (`CoopAssignSlot`); a guest accepts only while it has
  none, so guests must join one after another. The host tracks connected guest slots per connection index
  (`CoopRole.ConnectedSlots`); `CoopRole.ControllerOf` returns the host for a guest who is not connected
  (disconnect takeover).
- `CoopHostActions` (host only, each an ordinary frame): `AssignSquadSlot`, `GiveSoldier` (also takes back),
  `SetPlayerCount`. Squad rules: the host may fill any slot; a guest only slots the host gave them; a slot only
  takes soldiers of its player or unowned ones (unowned are claimed by the slot's player).
- `CoopTactical.SplitXComGroup`: one XCom group per guest, `InitiativePriority` = host group + slot.
- `CoopTactical.ControllerOfInitiative` / `IsInitiativeRemote`: which machine runs an initiative entry
  (XCom group = its members' owner, or the host while that guest is away; everything else = host).

### Network handoff between groups: plan from the PvP ruleset (read, not built)

`X2TacticalMPGameRuleset` (WOTC SDK) already hands control between initiative **groups**:
- `TurnPhase_MPUnitActions` (around line 1335): runs local groups in a loop; when `IsNextInitiativeGroupRemote()`
  it breaks out, calls `SetInputState('Multiplayer_Inactive')`, `SendPartialHistory()`, waits for the partner's
  `HistoryReceived`, waits for the visualizer, then ends the phase.
- `TurnPhase_PlayerSync` (around line 1211): sends the partial history; on `HistoryReceived` sends `SwitchTurn`;
  the now inactive machine calls `ReplayMgr.ResumeReplay()`, the other one stops replaying on `SwitchTurn`
  (`ReplayMgr.RequestPauseReplay()`).
- Remote-ness is one function, `GameStatePlayerOrGroupIsRemote` (line 437): a group's team player's net id vs the
  local one. In co-op every group is XCom, so all would look local; `CoopTactical.IsInitiativeRemote` is the
  co-op replacement.
- The PvP ruleset cannot be used as is in campaign missions: `IsTurnRemote` casts to `XComGameState_BattleDataMP`
  (campaign missions have `XComGameState_BattleData`), and its start phase places units for PvP.

Plan: a `CoopTacticalRuleset extends X2TacticalGameRuleset` that ports only these pieces (the unit-actions loop
with the remote-group break, the player-sync phase, the `HistoryReceived` / `SwitchTurn` commands and the replay
manager calls), with `IsInitiativeRemote` as the remote test. Aliens, the Lost and the Chosen run on the host,
so a guest is in replay for everything except its own group. Dragonpunk subclassed the same ruleset for the
same reason (vanilla, about 4,000 lines); this port should be much smaller because WOTC already has groups.
Needs two-instance tests.

### Handoff draft and two-copy tools (2026-11-11)

Review fixes (2026-11-12, compiled): no double replay start when handing over; the begin-play split listener submits its own frame; the host waits for one acknowledgement per connected guest; `CoopSwitchTurn` names the player slot that takes over, so other watchers keep watching. For 3-4 players, where guests connect only to the host: the host waits for every guest's mission acknowledgement before `CoopLoadMission`, forwards a guest's partial history to all guests, and forwards a guest-to-guest `CoopSwitchTurn` only after every guest acknowledged the forwarded frames (compiled, not run).

- `CoopTacticalRuleset` (draft, compiled, **not registered**, never run) ports the PvP handoff to campaign
  missions: when `NextInitiativeGroup` hands initiative to a group another machine runs, the active machine
  enters `TurnPhase_CoopHandoff` (recorded in the history), sends a partial history, waits for
  `CoopHistoryAck`, sends `CoopSwitchTurn` and starts watching through an `XComMPReplayMgr` it puts on the GRI
  (campaign missions spawn a plain `XComReplayMgr`). The watcher stops its replay on `CoopSwitchTurn` and
  resumes `TurnPhase_UnitActions` at `BattleData.UnitActionInitiativeRef`, doing the steps of the base
  ruleset's load-during-unit-actions branch itself (`bLoadingSavedGame` is `privatewrite`). `CoopTacticalNet`
  holds the network delegates (an Object, not the ruleset Actor). Gated by `CoopTactical.bHandoff` and a
  connection. Mission entry: the host sends the whole history at its first unit actions; the guest answers `CoopMissionHistoryAck`; only then the host sends `CoopLoadMission` (commands can overtake history); the guest opens `BattleData.m_strMapCommand` (as the `MPLoadTacticalMap` console command does) with its replay manager already in replay, so `LoadTacticalGame` goes to `PerformingReplay` and it watches from the start. `bInReplay` is `protectedwrite`, hence `CoopReplayMgr extends XComMPReplayMgr` with a setter. Interrupt exactly at a handoff: the resume picks the interrupted group's index when the handed-over initiative is the interrupting group (as the base load path does); the base's private `bInitiativeInterruptingInitialGroup` only makes it call `XGPlayer.OnUnitActionPhase_NextGroup` when the interrupted group resumes after the interruption, so the co-op ruleset remembers that group and makes that call itself in `NextInitiativeGroup`. Load path, derived from source: `XComTacticalGRI.InitBattle` (about line 336) calls `StartNewGame` only when `History.GetStartState()` returns a start state and `BattleData.bIntendedForReloadLevel` is false, otherwise `LoadGame`; a received history of a running mission should have no pending start state, so the guest takes `LoadGame` (not tested).
- Second game copy on one PC without Sysinternals: `close_ue3_mutex.py` lists the first copy's handles
  (`NtQuerySystemInformation` class 0x40), asks only `Mutant` handles for their name, and closes
  `UnrealEngine3_<n>` with `DuplicateHandle(DUPLICATE_CLOSE_SOURCE)` (same user, no admin). Tested against a
  stand-in process holding the target and a decoy mutex: 9/9 checks pass; a mutant that "closes" without
  `DUPLICATE_CLOSE_SOURCE` fails 2 checks. Not yet run against the game.
- Two-copy runner scenario `mission` (not run): registers the ruleset and turns on split, selection gate and handoff in the test copies' private mod config for that run only (restored afterwards); expects the host to send the mission, the guest to load it watching, a handoff to the guest's group, and a handoff back.
- Two-copy runner scenarios (not run): `join` (mods match: host assigns slot 1, sends the history, guest
  receives it) and `joinmismatch` (guest has an extra dummy mod: refused, no history sent; checked with
  "must not appear" expectations). Unknown: whether `GetInstalledModNames()` lists a mod folder that has no
  script package.

### First in-game results (2026-11-12, one game copy, Steam build 8270065)

Ran on a hard-linked test copy with `-nohomedir`, after each run the real install and saves were
checked unchanged.

- Scenario `squad` (Avenger): **13/13 pass**. Roster `0011`; host fills its own and a guest slot (unowned
  soldiers claimed by the slot's player); placing another player's soldier is undone; a guest filling the
  host's slot is undone; `give` refused for a guest; host assigns a slot and sets 4 players (`0123`);
  guest rights close another player's Armory soldier and the research screen; appearance audit 42
  soldiers, 0 with missing parts.
- Scenario `exp12` (Avenger -> Guerilla Op mission -> Avenger): **11/11 pass**. Owner tags `0 0 1 1` set on the
  Avenger are the same in the mission and again after returning. The controller override is active
  (`CoopTacticalController`). With the guest not connected the host may select every soldier (takeover);
  with the guest connected the host's selection of the guest's soldiers is refused. Splitting the XCom
  group gives turn order XCom player -> host group (prio 0) -> guest group (prio 1) -> aliens. **Turn 2:
  after the host ended its turn, initiative went to the guest's group, whose soldiers could then act and
  the host's could not.** Turn 1 skipped the guest's group: the split was made mid-turn in this test
  (after turn 1 had begun); the mod does it at mission start.
- Found by running: travelling from the shell with the Firaxis Live login screen open (fresh install)
  keeps the shell World alive (`X2FiraxisLiveClient.ConnectionStatusDelegates`) and asserts
  (`UnGame.cpp:3107`); its state versions of `CloseScreen` do not always clear the delegates, so the test
  bot clears them itself. `Helpers.GetInstalledDLCNames()` crashes the game on the Avenger (access
  violation); not used any more. `Helpers.GetInstalledModNames()` returned only the enabled mod (1), not
  every subscribed Workshop item. `Helpers.NetGetMPINIHash()` is empty outside PvP. The drop-ship briefing
  waits for a key once the map is loaded (`PC.bSeamlessTravelDestinationLoaded`); the test presses it with
  `CleanupDropshipStart()`, as the game's input handler does. A new debug campaign has no available
  mission; `SpawnMission MissionSource_GuerillaOp Reward_Supplies` adds one.

### Two-copy results (2026-11-12, one PC, 127.0.0.1)

- Second copy started with `close_ue3_mutex.py` (closed `\Sessions\1\BaseNamedObjects\UnrealEngine3_8` in the
  first copy, both ran).
- Scenario `join`: **7/7 pass**. Guest connects, host assigns player slot 1, mod lists compared (match),
  host sends the campaign (227 frames), guest enters the Avenger as mirror with the same fingerprint
  (supplies 4000, crew 25, 22 soldiers, same first soldier); a chat line from the host arrives.
- Scenario `joinmismatch`: **5/5 pass**. Guest with an extra mod: both sides name it (`+CoopDummyMod`), the
  host refuses, no history is sent. `GetInstalledModNames()` lists a mod folder without a script package.
- Scenario `mission` (handoff, ruleset registered for the run): first **not reached**, because the host's
  `CreateTacticalGame` stalled with a server open. After the fixes in "Mission scenario: blockers and
  attempts" below, it passes **22/22**: the host's mission, the guest's entry as a watcher, a handoff to the
  guest's group and a handoff back to the host at the alien turn.
- Found by running: the engine hands every remote-command listener the same parameter buffer and each
  read consumes it; two listeners reading `CoopModsResult` crashed the host (access violation,
  "GettingNameData"). Now one reader per command. A second copy of an ini section is ignored for plain keys
  (only the first counts; `+` list entries from both are merged).

### Mission scenario: blockers and attempts (2026-11-12)

Each attempt is a build plus a run of `run_pair.ps1 -Scenario mission`. Install and saves were checked
unchanged after every run (`instance_prep.py installcheck`, `userdata_guard.py verify`).

**Blocker A: the host's `CreateTacticalGame` stalls when a network server is open.**

- Cause, from source and log: `XGStrategy.LaunchTacticalBattle` plays `CIN_XP_UI_LogoSpin.bk2` for the
  map change. `CreateTacticalGame` waits `while (IsAnyMoviePlaying() && !IsLoadingMoviePlaying())`. With a
  connection open the movie is not reported as a loading movie, so `HideLoadingScreen` never stops it.
- Attempt A1: stop that movie from a 1 s timer in `CoopTacticalController` once `CreateTacticalGame` is
  running with a connection open. Result: the host's mission started (5 of 13 expectations passed). Run
  `exp12srv` (one copy, server open) confirmed the stall and the fix.
- Attempt A2 (needed after a crash): the host crashed with `Object is not packaged:
  XComGameState_EnvironmentDamage None` right after "Finished Map Streaming". At first this looked like the
  movie being stopped during map generation. So the stop now waits until `ParcelManager.IsGeneratingMap()`
  is false and streaming is complete. Result: **the same crash, and the log had no "stopping the stuck
  mission load movie" line**, so the movie stop was not the cause.
- Attempt A3: the crash came from the strategy layer's batch sync (`CoopSession.FlushBatch`). It was still
  collecting frames during the mission and sent a merge state while the host generated the map ("sent batch
  of 7 frames" at 120.20 s, during "Generating Map"). Fix: `CoopSession.InTactical()` turns batching and
  flushing off while a tactical ruleset runs; the mission history and partial histories cover missions.
  Result: no crash; the host stopped the movie at 122.05 s with `gen=False streamed=True`; **13/13 pass**
  (run `mission-20261112-112701`).

**Blocker B: the guest hangs in `LoadTacticalGame` after loading the host's mission as a watcher**
(seen in the A1 run: log stops about 123 s in, harness timers stop, process responding with high CPU).

- Attempt B1, a diagnostic: `CoopTacticalController.LogLoadState` logs what the begin block waits on (map
  generation, streaming, `UIIsBusy`, `WaitingForVisualizer`, movies, pause, frame count) on change and every
  10 s. The runner now keeps both logs in `local/runlogs/`.
- Attempt B2: `LoadTacticalGame` ends with the same movie loop as `CreateTacticalGame`, so the movie stop
  also covers `LoadTacticalGame` once `bProcessingLoad` is false. This was written after the A3 run and is in
  the following build.
- Result of the A3 run (B1 in, B2 not yet built): **the guest did not hang**. Its load states were
  `loadmovie=True` at 128.79 s, then `uibusy=True, movie=False` at 129.90 s, then "watching from frame 429" at
  132.86 s and `PerformingReplay`. Its loading movie was a real loading movie and ended by itself.
- Derived, not proven: the A1 hang most likely came from the same strategy batch, which also ran while the
  guest loaded. The A1 guest log was overwritten before it was kept, so this is unknown.

**Bug C, found in the A3 logs although all 13 expectations passed: the host resumed at the wrong initiative.**

- The guest ended its turn and handed over the alien player (initiative 2328), but the host "resumed unit
  actions at initiative 2548", which is the guest's own group. The host then tried to select the guest's
  soldiers ("select blocked unit=422 owner=1").
- Cause, from source: the guest joined through `LoadTacticalGame`, which sets `bLoadingSavedGame`. The base
  clears it only in its own `TurnPhase_UnitActions.BeginState` load path, which the co-op resume skipped.
  While the flag is set, `EndInitiativeTurn` and `BeginInitiativeTurn` submit no turn frames, so
  `BattleData.UnitActionInitiativeRef` still named the guest's group. A second problem: a player entry such
  as the aliens writes `BattleData` only in its `PlayerTurnBegin` frame, so resuming from `BattleData` was
  fragile anyway. The base's `bLoadingSavedGame` is `privatewrite`.
- Fix C1: the machine that runs an initiative begins it. In `TurnPhase_UnitActions`, `BeginInitiativeTurn`
  is skipped for a remote initiative. `CoopSwitchTurn` now carries the slot, initiative id, turn-order index
  and interrupt flags, and the receiver calls `BeginInitiativeTurn` itself. A guest's first resume runs the
  base `BeginState` once, which takes the load path and clears `bLoadingSavedGame`. This replaces the
  `InterruptedGroupToNotify` workaround, because the flags now travel with the switch.
- The scenario now also expects:
  - turn-order index 2 for the guest and index 3 (aliens) for the host;
  - the guest's "base load path ran, bLoadingSavedGame=False";
  - no "WARNING" from the resume;
  - a final `@groups` showing the XCom player at `turns=2`, which means a full round came back to the host.
- Result of C1 (run `mission-20261112-113744`): 17 of 18 pass.
  - The guest took over at index 2 after the base load path ran (`bLoadingSavedGame=False`).
  - The host took over at "initiative 2320 index 3 flags 0" (the aliens), began their turn (`turns=1`),
    then the Lost (`turns=1`).
  - No resume warnings.
  - The one failure, `turns=2`, is not a sync fault: at the first alien turn the mission script logged
    "0 : Alien units remaining" and the ruleset went to `EndTacticalGame`, so the mission ended before a
    second round.
  - So the last expectation now checks that the host began the alien turn (`eTeam_Alien turns=1`).
    `@groups` now also prints living units per team, to find out whether this test mission (a Guerrilla Op
    added with `SpawnMission` in a fresh debug campaign) has any aliens at all.

**Blocker B again: the guest crashed on the Avenger right after receiving the mission history** (run
`mission-20261112-114553`, 5 of 18 pass).

- Log: "event ReceiveHistory frames=432". Then the Avenger's visualization manager built visuals for the
  mission frames: `X2Action_SyncMapVisualizers`, about 1000 `XGUnit:CreateVisualizer` warnings, then
  "Accessed None 'PRES'", then "Crash: GettingNameData".
- The two passing runs show the same warnings (29 "Accessed None 'PRES'", about 2700 `CreateVisualizer`)
  but survived. So this is a race present in every run, and the derived explanation for the A1 guest hang
  is now this rather than the strategy batch (still unproven, since that log is lost).
- Same run, from the new unit counts: **this test mission has no enemies at all** ("units xcom=4 alien=0
  lost=0" at mission start). Ending at the first alien turn is the mission's content, not a sync fault.
- Attempt B3: in `CoopTacticalNet.OnReceiveHistory`, a guest that receives a full history outside a mission
  calls `EnableBuildVisualization(false)` on the current map's visualization manager. Delegates run before
  the manager's next tick, and the mission map brings a new manager that its bootstrap enables.
  `CoopSession.InTactical` also no longer touches `XCOMGAME` when the current game is not an `XComGameInfo`.
- The scenario now also fails on "Accessed None 'PRES'" on the guest and on "Crash Detected" on either copy.
- Result of B3 (run `mission-20261112-115403`): **22/22 pass.**
  - The guest turned visual building off for both received histories: the campaign history in the shell
    and the mission history on the Avenger.
  - It logged no "Accessed None 'PRES'" and no `CreateVisualizer` warnings, then loaded the mission and
    watched from frame 500.
  - Turn order: host group, then handoff to the guest at index 2 (the base load path ran,
    `bLoadingSavedGame=False`), then the guest's end turn handed the aliens (initiative 2322, index 3) to
    the host, which began their turn (`turns=1`).
  - The guest's map ping reached the host. No crash on either copy. Install and saves unchanged.

**Final check (2026-11-12, one build, runs in sequence):**

| Scenario | Result |
|---|---|
| `mission` | 22/22 (second clean run) |
| `squad` | 13/13 |
| `exp12` | 11/11 |
| `join` | 7/7 |
| `joinmismatch` | 5/5 |

Every run reported 0 real-install files changed and saves unchanged against the backup.

Open after this:

- The test mission has no enemies, so an alien turn with real AI actions on the host, followed by a guest's
  second turn, has not been seen.
- The ruleset is still registered only by the runner.
- 3-4 players and Steam invites are untested.

### Enemies, turn rules on by default, reconnect and disconnect (2026-11-12, afternoon)

**Why the co-op mission had no enemies.**

- Source: `X2TacticalGameRuleset.SetupUnits` calls `StartStateSpawnAliens` only
  `if (XENGINE.IsSinglePlayerGame())`.
- The same check gates other mission setup:
  - mission objectives (`XComParcelManager.GenerateMapUpdatePhase4`, "HACK: Skip this if in MP");
  - the drop-ship streaming maps and the mission intro;
  - quicksave and quickload (`XComAutosaveMgr`);
  - abilities not flagged for multiplayer (`X2TacticalGameRuleset.InitializeUnitAbilities`);
  - flank crits, which use `MP_FLANKING_CRIT_BONUS` instead of the unit's stat;
  - pause in the pause menu, and some visibility and camera paths.
- Run `netprobe` (one copy, new runner scenario) logged `NetMode`, `IsSinglePlayerGame` and the network
  manager state at each step:
  - `NetMode` stays `NM_Standalone` throughout;
  - opening a server (`MPCreateServer`) flips `single=false multi=true`, and closing it (`Disconnect` +
    `ResetConnectionData`) flips it back;
  - with the server closed before launch, the mission had "units xcom=4 alien=6";
  - reopening a server inside the mission flips the check to multiplayer again.
- So the check means "a co-op connection is open", not "this is a PvP game".

**Fix: the host builds every mission without a connection** (`CoopLink`, `OnPreMission`).

- Host:
  - at `X2DownloadableContentInfo.OnPreMission` (strategy, after the start state, before the map travel)
    it remembers which guests were connected and closes the connection;
  - the mission is then built as single player: aliens, objectives, drop-ship, intro;
  - at its first unit actions it reopens the server on the same port, waits for those guests (up to
    `WaitForGuestsSeconds`), then sends the mission history.
- Guest:
  - the engine sends a client whose host closed the connection back to the main menu
    ("IpNetDriverSteamworks_0 shut down", "Failed; returning to Entry"), which also killed the first
    reconnect timer (run `mission-20261112-123526`);
  - it now reconnects from the main menu every 3 s (a timer actor restarted on each map by
    `UIScreenListener_CoopLink`), gets the mission history and loads the mission from there;
  - it asks for its old slot with `CoopWantSlot`. The join request URL carries only the player's name, so
    a URL option cannot carry the slot.
- During the mission the connection is open, so the multiplayer branches above still apply. Handled so
  far:
  - abilities: `CoopTacticalRuleset.InitializeUnitAbilities` skips the multiplayer filter in co-op
    campaigns (reinforcements, units from the Avenger, possessed units);
  - quicksave: `CoopAutosaveMgr.HostQuicksave` lets the host quicksave, except in Ironman, while
    watching, or mid-handoff.
- Still multiplayer during play: flank crit bonus, pause menu (does not pause), quickload, achievements,
  some visibility and camera branches.

**Turn rules registered in the mod.**

- `CoopTacticalRuleset` is now in the mod's `XComEngine.ini`, and `bSplitXComGroupAtStart`, `bHandoff` and
  `bGateSelection` default to true.
- Without a connection the ruleset behaves like the game's.
- The group split now happens only when guests were connected at launch (`CoopLink.MissionSlots`), so a
  campaign with a co-op object played alone keeps one XCom group.
- Conflict: any other mod that replaces `X2TacticalGameRuleset`.

**Disconnect: the host takes over.** If the guest whose turn it is drops, `CoopRole.ConnectionClosed`
calls `CoopTacticalRuleset.OnGuestDisconnected`:

- while watching that guest's turn, the host stops the replay and begins that guest's group itself. The
  guest's actions from that turn are lost, because frames travel only at the end of a turn;
- while waiting for that guest's acknowledgement of a handoff, the host stops waiting and keeps the turn.

A guest that comes back gets the running mission's history and watches. The restarted guest crashed on
its first try: the mod's own load from the main menu did not clear the Firaxis Live login delegates (GC
assert UnGame.cpp:3107). That clean-up moved from the harness into `CoopTacticalNet` (rerun below).

**Results:**

- `netprobe` 3/3.
- `mission` **30/30** twice (runs `mission-20261112-124631`, `-125752`, the second with the rules on by
  default):
  - the host closed with guests [1] and `single=true`;
  - "units xcom=4 alien=4 lost=7";
  - the guest went to the main menu, reconnected as player 1 and loaded the mission;
  - round 1: host group, guest group, aliens (AI turn on the host), the Lost and the other players;
  - round 2: XCom `turns=2`, the host group, then the guest took over its group again.
- `dropguest` (new: the runner kills the guest 8 s into its own turn and restarts it 10 s later), run
  `dropguest-20261112-130657`, 11/15:
  - the host logged "player 1 left during its turn: the host takes over initiative 2809" 3 s after the
    kill, then "resumed unit actions at initiative 2809 index 2";
  - `@groups` showed that group active with `runBy=0`;
  - the guest rejoined and the host sent it the mission history;
  - the 4 failures are the restarted guest's crash described above.
- `dropguest` after moving the delegate clean-up (run `dropguest-20261112-131648`): **15/15**:
  - the host took over the killed guest's group;
  - the restarted guest joined from the main menu, loaded the running mission and watched;
  - in the next round it took over its own group again;
  - no crash on either copy.

**3-4 players: setup.**

- Copies 2 and 3 were added with `gamepool provision --pairs 1 --first-pair 1` (hard links, copies 0 and 1
  untouched), then `instance_prep.py privatize 2 3`. `build_mod.ps1` mirrors the mod to every copy that
  exists.
- `run_pair.ps1` now runs two to four copies (A host, B-D guests). It closes each copy's single-instance
  mutex before starting the next.
- New scenario `join3` (three copies): **12/12** (run `join3-20261112-164601`):
  - the guests got slots 1 and 2, and neither got the other's;
  - both received the campaign; all three copies show the same fingerprint (226/227 frames, supplies 4000,
    crew 25, 22 soldiers);
  - guest 1's chat line reached guest 2 through the host.
- Not run with three copies yet: a mission (relay of a guest's frames to the other guest, guest-to-guest
  handoff). That code is compiled and the copies are ready.

**Steam invites: setup.** `CoopSteamJoin` now opens the host's server and makes the guest's connection
through `CoopLink`. So a Steam-sockets server is reopened after each mission build, and a Steam guest
reconnects with the host's Steam address. It is still off by default and has never run: it needs two Steam
accounts on two PCs. That test is the owner's call.

### Mission end and the return to the Avenger (2026-11-12, evening)

**How it works now** (`CoopLink`, `CoopTacticalNet`, `CoopTacticalRuleset`; it mirrors the mission start):

- When the host's ruleset enters `EndTacticalGame`, the host remembers which guests were in the mission
  and closes the connection. The mission summary, the after action and the strategy transfer then run as
  single player.
- `X2DownloadableContentInfo.OnPostMission` (during the Avenger load) schedules the server to reopen 5 s
  later. Once those guests are back, the campaign history goes out. Each guest acknowledges it and the host
  answers `CoopEnterAvenger`, so the guest opens the Avenger with that history. This is the same as joining,
  done by the mod instead of the test command `@mirror`.

**Found by running `missionend`** (new scenario: host and guest in a mission, `EndBattle true`, the host
clicks through the after action, both copies must end with the same campaign fingerprint):

1. A guest inside a mission keeps its client net driver when the host closes, unlike on the Avenger, where
   the engine shuts it down. `HasClientConnection()` stayed true, so it never reconnected (run
   `missionend-20261112-173212`). The guest now drops the dead connection on the next timer tick.
2. A guest that reconnected while still on the mission map received the campaign there and crashed
   ("GettingNameData", run `-174209`). The guest now leaves the mission map for the main menu
   (`disconnect`) before reconnecting.
3. Leaving the mission map then hit the GC assert (UnGame.cpp:3107). The mission World was held by
   `CoopReplayMgr` through the `ReceiveHistory` delegate that the PvP replay manager registers on the
   network manager and clears only in `Destroyed()`, which does not run on a map change (run `-175102`).
   The delegate does nothing, so `CoopReplayMgr` clears it right after the base registers it.
4. The host's after action waits on `UIDropShipBriefing_MissionEnd` ("press a key"). The test command
   `@afteraction` now continues it (`bContinueToSeamlessTravelDestination`, as a key press does). The
   one-copy `exp12` never got past that screen either; it did not check.
5. Once, the guest hung while loading the main menu after the host closed for the mission build (log stops
   right after `LoadMap: XPACK_Shell_Intro`, no crash record; run `-180015`). The same step passed in about
   six other runs, so it is recorded as an intermittent engine hang, cause unknown.

**Result:** `missionend` **11/11** (run `-180910`). The guest returned to the main menu, reconnected as
player 1, received the campaign and entered the Avenger. Both copies show the same campaign fingerprint.
The host clicked through the summary, the drop-ship, the after action, the loot and two alerts.

### Three players in one mission (2026-11-12, evening)

New scenario `mission3`, three copies:

- soldier owners 0 0 1 2 (`@tagowners`);
- each copy plays when one of its own XCom groups has the initiative (`@waitmyturn`, so the test does not
  depend on guessed timings);
- expected turn order: host's group, guest 1's, guest 2's (guest 1 hands over through the host's relay),
  aliens, then again in round 2.

**Found by running it:**

1. **Both guests took slot 1** (run `mission3-20261112-182510`).
   - They reconnected within 0.1 s of each other after the mission build. The host's broadcast
     `CoopAssignSlot(1)` reached both while neither had a slot, so guest 2 played guest 1's group too.
   - The network manager cannot send to one connection, and the join URL carries only a name. So slots are
     now agreed by a nonce:
     - the host greets a new connection with `CoopWelcome`;
     - the guest answers `CoopHello(nonce, wanted slot)`;
     - the host answers `CoopSlotFor(nonce, slot)`, and only the guest with that nonce takes it.
   - The old wanted-slot commands were removed.
   - For the same reason the host cannot tell which guest a closed connection belonged to. It now runs a
     roll call: `CoopRollCall`, guests answer `CoopHere(slot)`, and a slot that does not answer within
     `RollCallSeconds` (2) is gone. The host also clears its list before it closes the connection on
     purpose, so that close needs no roll call.
2. **The relay of guest 1's turn never completed** (runs `-185409`, `-192009`).
   - Sending the relay from inside the receive callback was suspected first and moved to the next timer
     tick. It made no difference.
   - The host log showed the real cause: guest 2 could not apply the partial history
     (`OnSendError_RequestNewHistory`), and the engine itself sent it the whole history instead ("Wrote
     entire history to network queue: 401.42 kb (551 states)").
   - Guest 2 answered that full history with the mission-entry acknowledgement, so the host never counted
     it for the relay. A guest that receives a full history during a mission now acknowledges it as the
     partial history it replaces.

**Result:** `mission3` **24/24** (run `-194635`):

- guest 1 took its group at index 2 and guest 2 kept watching;
- the host relayed guest 1's frames and forwarded the switch once both guests had acknowledged;
- guest 2 took its group at index 3, then handed to the host (aliens, index 4);
- in round 2 (XCom `turns=2` on all three copies) both guests played their groups again;
- guest 1's ping reached the host and guest 2.

**Open:** every relay ends in such a full resend (about 400 KB) to one guest. It works, but it is wasteful;
why the partial history does not apply there is unknown.

**Found in the full rerun afterwards:**

- A guest whose join attempt failed crashed. The host copy had hung at start-up (its log stops 0.5 s in), so
  the guest's connection timed out. The engine reloaded the main menu, which asserted on the Firaxis Live
  login screen's delegates (UnGame.cpp:3107). `CoopLink.TryConnect` now clears them before every attempt.
- `mission` missed one check because of test timing. The host quit before the guest's fixed-delay round-2
  `@groups`, and the guest correctly left the mission when the host went. The guest's round-2 steps now use
  `@waitmyturn`.

### Saving, reloading and a second mission (2026-11-12, night)

**Joining on the Avenger is now done by the mod** (`CoopLink.HostJoinedOnAvenger`).

- When a guest gets its slot and the host is on the Avenger, the host sends the campaign 3 s later
  (`JoinCoalesceSeconds`, so guests joining together share one send).
- Each guest acknowledges it, the host answers `CoopEnterAvenger`, and the guest opens the Avenger.
- With the mod check on, the send waits for a match (`CoopJoin.OnModsResult`).
- Before this, the test runner did it with the console command `MPSendHistory` and the bot's `@mirror`.
  Every scenario now uses the automatic join.
- A guest already on the Avenger gets the campaign again when another guest joins, and reloads the Avenger.

**New scenario `saveload`** (two copies, the host copy is restarted in the middle):

1. Mission 1 with a guest: the host's turn, then the guest's, then back to the host (round 2); the host wins
   and the guest returns to the Avenger with it.
2. The host saves (`@save`) and quits. The guest goes back to the main menu and keeps reconnecting.
3. The runner restarts the host copy. It loads its newest save from the main menu (`@loadnewest`) and hosts
   again. The guest rejoins and gets the campaign automatically.
4. Mission 2 with a handoff to the guest; the host wins; both return.

**Result: 17/17 on its first run** (run `saveload-20261112-222252`, with A1 = the host's log before the restart):

- the owner tags are identical before the save and after the reload ("410=0 416=0 422=1 428=1");
- the guest took over its group in both missions;
- the host also autosaved during mission 2 (autosaves have no multiplayer check);
- both copies end with the same campaign fingerprint;
- the save files are in the test copy (`-nohomedir`).

Not covered yet: Ironman, a guest restarting its game between sessions, and loading a save made in the middle
of a mission.

### Host and join buttons, leftover multiplayer rules, cheaper relay (2026-11-13, night)

**Buttons** (`UIScreenListener_CoopMenu`, `CoopMenu`, `CoopStatusPanel`):

- **Avenger HUD, "Host co-op":** gives the campaign its co-op object if it has none (two players, default
  squad split by the mission's soldier limit), then opens the server: Steam sockets with the invite overlay
  when `bSteamInvites`, otherwise direct IP on `CoopLink.DefaultPort`.
- **Main menu, "Join co-op":** a text dialog for "IP" or "IP:port", then `CoopLink.GuestConnect`. The guest
  gets the campaign and enters the Avenger by itself.
- **Status line on both screens**, refreshed every second: hosting and who is connected; joining; connected
  as player N; reconnecting while the host builds a mission or loads.
- **Test:** new scenario `buttons`, **14/14**. The host only presses "Host co-op". The guest presses "Join
  co-op" and enters the address (bot commands `@clickui`, `@inputaccept`). Both end with the same campaign
  fingerprint.
- **Found by it:** the address check read `.Length` directly from `SplitString(...)`'s result and always got
  0. It now splits into a local array first.

**Leftover multiplayer rules** (`CoopRules`). With the connection open the game is "multiplayer" during
missions. These now use the single-player values in a co-op campaign:

| Rule | Multiplayer value | Fix |
|---|---|---|
| Flank crit | flat `MP_FLANKING_CRIT_BONUS` (30) | `CoopToHitCalc_StandardAim` sets it to the shooter's own `eStat_FlankingCritChance` (33/40/50 by unit) for the duration of the roll |
| Suppression aim penalty | `MultiplayerTargetAimPenalty` (-35) | `CoopEffect_Suppression`: -50 or -35 by side |
| Shadowbind | the new shadow loses its action points | `CoopEffect_SpawnShadowbindUnit` keeps them |
| Pause menu | does not pause | `UIScreenListener_CoopPauseMenu` pauses; closing it unpauses |
| Quickload | blocked | **left blocked on purpose**: it would rewind the host under the guests |

- The three classes are registered as `ModClassOverrides`. That reaches objects created with `new` but not
  the aim objects X2Ability builds in its `defaultproperties` (e.g. `SimpleStandardAim`, used by
  `StandardShot`).
- So `OnPostTemplatesCreated` replaces each ability's plain standard-aim object with a co-op copy of the same
  settings: 13 abilities, 4 distinct objects. The rest already got the co-op class through the override.
  The subclass inherits the base class's config (`LOW_COVER_BONUS` 20 in both).
- Conflicts: any other mod that overrides these three classes.
- Checked in `mission` (**36/36**):
  - `@mprules` shows the co-op classes in place;
  - `@tacpause` shows the world paused and then unpaused. The check runs on `CoopPauseProbe`, an actor with
    `bAlwaysTick`, because the bot's own timers stop while paused; the first attempt froze the test.
  - **Not seen yet:** a real shot rolled with the connection open, i.e. the flank crit value in an actual
    roll.
- Lesson: the WOTC script compiler crashed outright, with no error message, on a probe that read
  `(new class'X').Class.Name` inline and ran `foreach` over an array reached through a function call's result.
  Written out with locals, it compiled.

**Cheaper guest-to-guest handover.**

- Cause, from the native log: a partial history carries the size and hash of the history it builds on
  ("Base size mismatch ... Base hash mismatch"). After the host applied guest 1's partial history, its base
  is guest 1's newest frame, which guest 2 does not have. So a relayed partial history can never apply on
  guest 2, and the engine fell back to the full history after an error round trip
  (`OnSendError_RequestNewHistory`). Two-player games never hit this.
- With this mechanism a true delta relay is not possible, so the host now relays the full history directly
  (`SendHistory`): one send, no failed attempt, no error round trip.
- The size is unchanged, about 400-460 KB for a mission of 400-570 frames.
- `mission3` **27/27** with new checks: no `OnSendError_RequestNewHistory` on the host and no base mismatch
  on either guest.
- A real delta would need another channel, for example sending the new frames one by one; not tried.

**Test runner:** a start-up watchdog restarts a copy that has not reached the harness's first "ready" line
within 150 s. Two runs earlier lost a copy that hung while booting (log stops a few seconds in, no crash).

**All scenarios on the final build (2026-11-13).** Every scenario ran on the same build; the watchdog never
had to restart a copy:

| Scenario | Result |
|---|---|
| `squad` | 13/13 |
| `exp12` | 11/11 |
| `join` | 7/7 |
| `joinmismatch` | 5/5 |
| `join3` | 12/12 |
| `mission` | 36/36 |
| `dropguest` | 15/15 |
| `missionend` | 11/11 |
| `mission3` | 27/27 |
| `saveload` | 17/17 |
| `buttons` | 14/14 |

The real install was unchanged and the saves matched the backup after every run.

### Untested features, four players, saves, flags and banner (2026-11-13)

**Features that existed but had never run** (new scenario `features`, two copies, plus checks in `mission`):

- The host's Armory "Owner" button and squad select's "Slot N" / "Player" buttons work; the guest sees the
  new owner.
- Guest rights are **on by default** now (`bHostDecidesCampaign=true`). The guest may open the Armory's
  soldier list and its own soldiers' screens (taking the baton); other players' soldiers and every other
  edit screen close.
  - Consequence: while the host keeps any edit screen open, the Geoscape included, a guest's request to
    edit its own soldier waits. That is the baton design; worth revisiting if it annoys players.
- Chat: the "Chat" button was missing on the host (added only when the HUD opened, before "Host co-op"); the
  status panel now adds it once a connection is open. Chat and pings work both ways on the Avenger and in
  missions.
- Watching another player's turn, the input is in the game's `Multiplayer_Inactive` state, which allows
  camera panning: both host and guest moved the camera about 2,660 units in 2 s of key presses.

**Four players** (`mission4`): four copies, one soldier each. Three guest turns run in a row, each handed
guest to guest through the host's relay, then the aliens, then the host.

**Saves:**

- **Mid-mission save** (`midsave`): the pause menu offers "Save" to the host during its own turn (not while
  watching: the game hides it in replay). After loading such a save there is no "Host co-op" button, so the
  host now **opens the server by itself when it loads a co-op campaign's save** (`CoopLink.HostLoadedGame`,
  on the Avenger and in a mission; `bAutoHostOnLoad`). The guest, sent to the main menu, reconnects and
  joins the running mission.
- **Ironman** (`ironman`): the Ironman save is the game's autosave at the start of the XCom turn on the host
  (guests never save). Loading it works the same way.
- **Guest restart** (`guestrestart`, three copies): a guest remembers its slot per host address in
  `XComCoopGuest.ini` (`CoopGuestMemory`) and asks for it again after restarting its game, so it gets its
  soldiers back even when guests rejoin in another order. The host ignores a remembered slot the campaign
  does not have.

**Found by screenshots, not by the logs.** The test copies now take screenshots (console `shot`).

- The guest had the game's PvP notice "Opponent left - connection to the session has been lost" on screen
  during its own turn. It is queued whenever a multiplayer connection drops; in co-op the drops are planned.
  Once a game has hosted or joined, `CoopLink.SweepNotices` drops it from the queue every second and closes
  it if it is already up.
- The bot's `@dismiss` closes the tactical HUD and unit flags along with debug red screens; tests now close
  only the red screen.
- The test copies show the game's own debug "red screens" (map and camera warnings from the base game).

**Flags and turn banner:**

- **Unit flags** (`CoopUnitFlag`, a `ModClassOverrides` of `UIUnitFlag`): next to each soldier's flag, a tag
  in the owner's color: HOST gold, P2 green, P3 pink, P4 orange. Checked on screenshots from both machines.
  - Conflicts: other mods that replace `UIUnitFlag`.
- **Turn banner** (`CoopTurnBanner`, `CoopTurnBannerPanel`): a box at the top of the screen, below the turn
  order icons, for 3.5 s: "YOUR TURN" on the machine whose player now acts and "PLAYER 2'S TURN" /
  "HOST'S TURN" on the others, in that player's flag color. The machine that begins an XCom group announces
  it ("CoopTurnOf"); the host passes it to the other guests. Enemy turns keep the game's own banners.
  - First version set the text of the game's own turn overlay. Screenshots showed no text: the game draws
    no banner at the start of an XCom turn (`XComPresentationLayer.UIEndTurn`), so the box is the mod's own.
    Now seen on a guest screenshot ("YOUR TURN" in player 2's green).

**The game's PvP lost-connection handling is off in co-op missions.** When the host closes the connection
(mission build, save load), each guest's tactical controller ran `XComTacticalController.LostConnection`:
it queued the "Opponent left" notice and started an asynchronous `PingMCP` whose callback points at the
controller. The guest leaves the mission map at once, so a late ping completion found its object gone and
the engine asserted on the main menu ("Failed to find function None in XComMCP"), in about one run in three
of the save scenarios. `CoopTacticalController` now skips `LostConnection` while co-op is active.

**Acks by slot.** The full regression found a race: the host sent the campaign to a rejoining guest while
it still counted a guest whose connection had just closed, waited for two acks, got one, and never told the
rejoining guest to enter the Avenger. The host now records which guest slots still owe an ack (campaign,
mission history, and its relay of a guest's frames in a mission) and drops a slot that disconnects.

**All scenarios (2026-11-13).** Two builds. Build 1 has every change above except the `LostConnection`
skip; build 2 adds only that skip, which lives in the tactical controller (missions only). So the
Avenger-only scenarios ran on build 1 and every scenario that enters a mission ran again on build 2:

| Scenario | Players | Build | Result |
|---|---|---|---|
| `squad` | 2 | 1 | 13/13 |
| `exp12` | 2 | 1 | 11/11 |
| `join` | 2 | 1 | 7/7 |
| `joinmismatch` | 2 | 1 | 5/5 |
| `join3` | 3 | 1 | 12/12 |
| `buttons` | 2 | 1 | 14/14 |
| `features` | 2 | 1 | 20/20 |
| `guestrestart` | 3 | 1 | 19/19 |
| `mission` | 2 | 2 | 53/53 |
| `dropguest` | 2 | 2 | 15/15 |
| `missionend` | 2 | 2 | 11/11 |
| `mission3` | 3 | 2 | 27/27 |
| `mission4` | 4 | 2 | 33/33 |
| `ironman` | 2 | 2 | 18/18 |
| `saveload` | 2 | 2 | 18/18, twice |
| `midsave` | 2 | 2 | 16/16, three times |

On build 1, `saveload` and `midsave` failed with the `XComMCP` assert, which led to the `LostConnection`
skip. On build 2, the skip logged in the 9 runs where a running guest loses the host during a mission, and no run
logged `XComMCP` or a crash. The real install was unchanged and the saves matched the backup after every
run.

Not covered by a test: a guest leaving while the host relays another guest's frames (the case the relay
part of the ack fix handles).

**All scenarios (2026-11-12, late evening).** `join`, `mission` and `missionend` ran on the final build;
the other six ran on the build before it, which differs only in the join-failure fix:

| Scenario | Result |
|---|---|
| `squad` | 13/13 |
| `exp12` | 11/11 |
| `join` | 7/7 |
| `joinmismatch` | 5/5 |
| `join3` | 12/12 |
| `mission` | 30/30 |
| `dropguest` | 15/15 |
| `missionend` | 11/11 |
| `mission3` | 24/24 |

The real install was unchanged and the saves matched the backup after every run.

**Regression after all of the above (one build, 2026-11-12 evening):**

| Scenario | Result |
|---|---|
| `squad` | 13/13 |
| `exp12` | 11/11 |
| `join` | 7/7 |
| `joinmismatch` | 5/5 |
| `mission` | 30/30 |
| `dropguest` | 15/15 |
| `join3` | 12/12 |

The real install was unchanged and the saves matched the backup after every run.

### Game content: mind control, Chosen, the Lost, story and defense missions (2026-11-13, evening)

New bot steps (`CoopContentTests`): `@useability SRC ABILITY TGT` (as the game's `X2UseAbility` cheat:
the ability is given to the unit if it lacks it, the hit is forced), `@dropunit TEMPLATE TEAM` (the
`DropUnit` cheat next to the squad), `@removemc`, `@mcstate`, `@trysel`, `@captured`, `@mark`; and in
`CoopExperiments` `@spawnmission SOURCE REWARD`, `@launch SOURCE`, `@avengerdefense`, `@tagbarracks P`.

| Scenario | What it does | Result |
|---|---|---|
| `mindcontrol` | an alien takes over a guest's soldier, the guest's soldier takes over an alien, the guest's soldier is freed | 24/24 |
| `mcall` | both of a guest's soldiers mind-controlled, then freed | 16/16 twice, after the fix below |
| `lostchosen` | the Lost and a Chosen Assassin dropped in; the Chosen kidnaps a guest's soldier; mission ends | 21/21 |
| `retaliation` | a Chosen retaliation: resistance fighters, civilians, aliens | 14/14 |
| `blacksite` | the story mission `GP_Blacksite`, two rounds | 14/14 |
| `avengerdefense` | the Avenger defense, two rounds | 15/15 three times |
| `avdefreinf` | the Avenger defense with the barracks owned by the guest | 15/15 |

What held without changes: mind control moves a unit into the instigator's group and back
(`XComGameState_AIPlayerData`, `m_arrDisplaced`), with the mod's split groups too, so a controlled soldier
acts for the side that controls it. A kidnapped soldier keeps its owner tag on both machines after the
mission. The Lost, resistance and civilian turns run on the host.

**Hang: a takeover waits on a visualization that never ends.** A guest starts its turn only after its
replay of the host's frames stops, and `XComMPReplayMgr.PlayingReplay.Tick` stops only while the visualizer
is idle. An `X2Action_RevealAIBegin` there never finished (game redscreen: a time out "will likely result in
a hang"), so the guest never played. Now, while a machine waits to take over, `CoopTacticalRuleset` checks
each second and, once the game marks an action as timed out, finishes it the way the game's debug command
does (`XComGameStateVisualizationMgr.DebugClearHangs`). It fired once in the later runs (`mindcontrol`, after
36 s), and the guest played its turn. Why the reveal times out on the guest is unknown.

**Soldiers who join during a mission go to their owner.** The group split runs once, at mission start. The
Avenger defense adds a free barracks soldier to the XCom player during the mission
(`SeqAct_SpawnUnitFromAvenger`), and the game puts it in the host's group. A soldier a guest owns would have
stayed there, and since a group belongs to its first guest-owned member, the host's whole group would have
gone to that guest. Now the host, at each XCom player turn begin, moves every soldier a guest owns into that
guest's group (`CoopTactical.RegroupOwnedUnits`). The reinforcement arrives at that same turn begin, and the
move comes before the host's group acts, so no soldier acts twice in a round.

**The guest "freeze" was a crash; fixed (2026-11-14).** When the host closes the connection (mission build,
save load, quit), a guest on the Avenger goes back to the main menu. In 7 of 269 such departures in the kept
runs, the guest's log ended right after `LoadMap: XPACK_Shell_Intro`. The game wrote no crash record, but
Windows kept a minidump of each (`%LOCALAPPDATA%\CrashDumps`): an access violation at `XCom2.exe+0x6d0695` on
Wwise's event thread (`AK::EventManager`), in the game's handler for callbacks from matinee sound tracks
(`InterpTrackInstAkEvent`). A sound an Avenger matinee posted just before the unload calls back after the map
unload has nulled the world. The game's lost-connection handling unloads the Avenger in the same frame the
close arrives, so it races any sound posted then. Now the guest takes the way out itself
(`CoopHeadquartersController` skips `NotifyConnectionError` for that case): it drops the connection, pauses
the world so no matinee posts anything new, and one second later calls the same `ReturnToMPMainMenu`. A guest
leaving a mission map takes the same pause. Details and the dump analysis: NOTES, 2026-11-14.

Not run: mind control by the Chosen, the Blacksite objective cutscene, a rescued soldier joining
mid-mission. Run since (2026-11-14): an Avatar's mind control (`avatarmc`), the Chosen stronghold, the Chosen
assault on the Avenger and the final mission's map (README, QA table).

### Long sessions and effects on a guest's own turn (2026-11-14 to 2026-11-15)

**Four missions in a row (`soak`).** The first run hung the guest. After the first mission the host plays a
98 s movie and sends nothing; the guest's connection times out after 60 s and it reconnects from the main
menu. The host took that hello for a duplicate (same game process, old slot still connected during the roll
call), never sent the campaign, and dropped the guest. Now a hello is a duplicate only if no connection closed
since that guest's last hello; otherwise it is a reconnect with the old slot (`CoopJoin`, `ClosedCount`). The
guest, meanwhile, got a campaign update at the main menu and looped forever in one frame trying to add frames
there; `CoopSession.AlignHistory` now runs only on the Avenger and stops when no frame is added. With both
fixes, `soak` passed 13/13: four missions, both players, the guest back by itself after each timeout.
`EndBattle` wins without completing objectives, so that run replayed the Blacksite; `soakstory` completes
every objective first (`@winmission`) and passed 14/14 over four different missions, the Blacksite among them.
`soakstory8` passed 22/22 over eight, with a Chosen retaliation where the bot closed the Chosen reveal.

**Group-turn effects on a guest's soldiers.** A stun (`X2Effect_Stunned`) on a guest's soldier used up its
action point as in the game but never ended: the effect is removed by its tick on `UnitGroupTurnBegun` of the
soldier's group, and that group's turn runs on the guest, which did not have the listener after the handoff.
`CoopListenerRepair` now has every machine register the tick again for effects on groups it runs (the host
also for player-turn effects such as mind control). The guest repairs nothing else, which keeps it clear of
the broader guest-side repair that came before an earlier guest crash. In `stunguest` the guest repairs the
stun's tick just before taking its turn, the soldier starts that turn with one action point, and the stun is
gone on both machines. In `dazeguest` a Chosen Warlock's Mind Scorch dazes a guest's soldier (4 stun points):
it loses both actions in guest turns 1 and 2 and has both back in turn 3, on both machines, as the game's
`X2Effect_Stunned` tick predicts. Other group-turn effects use the same path but are untested.

**A host crash at the bot's launch.** Once in about 280 bot launches, the host crashed loading the mission
map with the same Wwise matinee sound race as the guest's Avenger exit. The bot launches straight from the
Avenger while room matinees play; it now pauses the world one second first. Whether a player's launch (after
the Skyranger flight, from the Geoscape) can hit the race: unknown.

**Bot gap: the Chosen reveal screen.** A Chosen's first appearance opens `UIChosenReveal`, which pauses the
game until a player clicks it away. The bot now closes it while waiting for its turn (exercised in
`soakstory8`), and `@afteraction` closes the Chosen's mission summary that follows such a mission.

### Goal checklist (2026-11-12)

| Goal item | Built (compiled) | Ran in the game | Still to build |
|---|---|---|---|
| Shared Avenger | prototype (baton) | yes, by the prototype's suite | host-only decisions: guests never take the campaign baton |
| Soldiers belong to players | ownership tag, host give/take back, roster; Armory soldier screen button "Owner: ..." (host cycles Host -> Player 2 ... -> nobody; guests see it disabled); colored owner tag on unit flags in missions | tags and give: pass (squad, exp12); button: pass (`features`); flag tags: pass (`mission`, screenshots) | - |
| Host assigns squad slots per mission | slot owners, rules, picker lock; squad-select buttons "Slot N" / "Player: X" for the host (vanilla and robojumper's screen) | rules: pass (squad); buttons: pass (`features`, vanilla screen); picker not run | robojumper's screen not run |
| Each player controls own soldiers in missions | group per player, selection gate, handoff draft, host relay, guest mission entry | one copy: groups, turn order and selection gate pass. Two copies (`mission`, 30/30): mission built with enemies, guest reconnects and watches, two full rounds incl. the AI turn; rules on by default. Three copies (`mission3`, 27/27): two guests, handover guest to guest. Single-player rules (flank crit, suppression, Shadowbind, pause) swapped in: `mission` 36/36. Four copies (`mission4`, 33/33). Watchers pan the camera; turn banner for every player | flank crit not yet seen in a real shot |
| Guests manage their own soldiers | `CoopGuestRights` (on by default): a guest may open the Armory soldier list and its own soldiers' screens, everything else closes; the edit gate skips refused screens | yes: `features` (own soldier opens, other soldier and research close, give refused) | guest edits wait while the host has an edit screen open |
| Disconnect: host takes over | controller rule; takeover of a dropped guest's turn; guest reconnect (`CoopLink`) | yes: `dropguest` 15/15 (guest killed in its turn, host takes the group, guest restarts, rejoins the mission, plays its group next round) | the dropped guest's actions of that turn are lost |
| Mods match automatically | in-game check and join gate (two copies: join 7/7, joinmismatch 5/5); CoopSync list, diff, plan, subscribe, mod settings export/write | CoopSync: 17 unit tests, 6 mutants caught | - |
| Visuals and voices | appearance audit | no | - |
| Steam invite join | `CoopSteamJoin` draft (off by default): host session marked as co-op (squad cost and turn time marker values), server over `steamsockets`, Steam invite overlay; guest invite handler replaces the game's PvP one only while enabled, joins, resolves the address, connects over Steam sockets, passes other invites on | no (needs two Steam accounts) | test on two PCs; it goes through `CoopLink` now |
| Host and join buttons | "Host co-op" on the Avenger HUD, "Join co-op" with an address dialog on the main menu, status line on both | yes: `buttons` 14/14 | Steam invite path from the button untested |
| Pings and chat | pings: `CoopPing` console command (bindable) at the 3D cursor, shown with `QueueWorldMessage`; chat: "Chat" button on the Avenger HUD and `CoopChat [text]` in missions, game's text input dialog, shown with `Notify`; both forwarded by the host to other guests | yes: `features`, `mission` (both ways), `join3`/`mission3` (forwarded) | - |

Test commands added to the prototype's bot (`@` steps): `squad N`, `tagsquad`, `tags`, `missions`,
`launch`, `groups`, `split`, `actors`, `endturn`, `watchturns S`, `afteraction`, `modinit`, `mods`,
`modsend`, `audit`, `roster`, `putsquad SLOT N`, `squadinfo`, `asslot N`, `gate 0|1`, `trysel`, `assignslot I P`, `give N P`, `players K`, `connected S`, `joingate 0|1`.

Lessons from building:
- The X2ModBuildCommon build deletes and rebuilds `<SDK>/XComGame/Script/*.u` and mirrors
  `Development/Src`; build from a private copy of the SDK.
- Running the SDK's `XComGame.exe make` from a new folder triggers a Windows Firewall prompt
  for that exe path (it does not need the network; cancel is fine).
- UnrealScript: `local` is a keyword in any case (a variable named `Local` does not compile);
  `%` with a `const` operand failed ("Right type is incompatible with '[Modulo]'"); a parameter
  named `Name` shadows `Object.Name`; a config variable must not also be set in defaultproperties.
