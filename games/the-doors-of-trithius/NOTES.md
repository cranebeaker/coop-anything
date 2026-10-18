# The Doors of Trithius: notes for the next agent

Game 0.6.3b (Steam build 25513945). Each fact below was checked with `javap` against the
game's own `DOT.jar`, or measured live with the host doing the same action alone. Class
names are the game's. Facts that only hold for a two-player mod are marked (co-op).

## Turns and the world map

- A world-map step is N cleanup units plus one start of turn. `OverworldMovementService.turnCleanup`
  runs every start-of-turn procedure once per unit of terrain cost (40, 50, 80, 100 or 120).
  `RefreshPlayerAbilities` ticks `updateCooldowns`, then `updateToggles`, in each unit.
  Measured: a cooldown of 300 falls to 259 on a 40-unit plains step.
- `OverworldData.movementDetails` is one slot for the one player. `turnCleanup` writes the
  tile details of the seated player into it, and `OverworldGameState.load` computes it.
  (co-op) Keep a copy per player and reset it when a player moves without a turn of its own.
- A pass turn (Space on the world map) fires no `PlayerTurnCompleteEvent`, and it never lowers
  weariness in vanilla either.
- `PlayerHungryCheckCommand` subtracts nothing after an overworld movement turn.

## Interaction and doors

- E (`PlayerInteractsWithNearbyObject`) keeps entities that have both a feature component and
  a player-interaction component within line distance 1. One match fires
  `PlayerInputUseFeatureEvent`, several open a picker. It never reads the door component.
- No shipped door has a player-interaction component (`modules/map_features.dat`: four door
  features, all with `interaction=null`). Doors open by C (`PlayerOpenClosesAdjacentDoors`), a
  bump or a click, all ending in `PlayerUsesDoor`. E beside a door does nothing.
- Vanilla re-plans a walk toward a creature at every step (`HandlePathContinue`).

## Shops and reputation

- `ShopInventory.isTradeable` hides stacks that are worn (condition 0.5 or lower), poisoned,
  cursed, unidentified or unpriced. `ShopPane.isTradeable` also hides favorites and stacks in
  study, and the window lists unequipped, non-alternate stacks only.
- `ShopItem(ItemStack)` copies the item, amount, enchantment, condition, study and name, and
  keeps no reference to the bag stack. You cannot tell which of two equal stacks a player
  moved.
- `PlayerInventory.findItem` matches the enchantment by reference. Replacing it with an equal
  new object while the window is open makes the sale fail with "One of the items you are
  selling is no longer available".
- `PlayerAcceptTradeEvent` carries no data. The moved stacks are in `ShopPane.shopkeeper`
  (sold) and `ShopPane.playerItems` (bought), and the pane is still open when the event runs.
- Reputation modifiers live in one hidden status `reputation`, set on the player at each place
  load (`PrepareZone` step `LoadReputation` -> `refreshReputationEffects` ->
  `givePassiveReputationGain`, which can add a rolled gain and a "passage of time" action).
  The status is saved with the player, and the load replaces it.
- Travelling merchants are not active shops.

## Stats

- `RefreshStatsService.recalculateTotalHealth` sets the total life to the `MaximumHealth` stat
  total on every `refreshStats`. A total that no stat backs is lost. `LifeComponent.setTotalLife`
  moves the current life by the change of the total, so restore the total first.
- A bare passive refresh unsubscribes triggers that other code subscribed. (co-op) Re-claim the
  seated player's passives after any refresh.

## Engine traps

- Autosave renames the zone file. When another process holds it (an antivirus scan, a second
  reader), the rename fails and the town loads half way.
- A Location pane opened twice gives two loader threads for one place, which crashed the game
  in a town. (co-op) Guard the pane so a second open is refused.
- Many systems read `context.player` directly (feed lines, tutorial notes, fog of war, the
  hotbar). (co-op) Count those reads by kind before you trust an impersonation window.

## Harness notes

- Posted Windows key messages reach the AWT window only for some keys while it has no focus.
  Inject keys inside the JVM instead (the AWT event queue), and post clicks in screen
  coordinates.
- A test window that requests focus drops a fullscreen game on the same PC to the desktop.
  Open test windows without a focus request, on the monitor nobody plays on.
- The test host runs with a 2 to 6 GB heap (`-Xms2G -Xmx6G`), and two seats run on one PC.
  Keep the JVM temp folder on a drive with space: a full system drive made gates time out,
  not fail.
