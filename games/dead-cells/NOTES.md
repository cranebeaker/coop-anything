# Dead Cells — co-op notes

Goal (owner): local couch co-op, one PC, two gamepads, two real heroes; per-player loot parity
(flasks, mutations, starting weapons, every "pick one" choice repeated for P2). Prefer Steam Workshop mod.

## Observations (sourced)
- Not in coop-anything DIRECTORY.md or games/ (checked 2026-10-23).
- Engine: Haxe/HashLink, game v35 (log line Boot.logClientInfos).
- Official mod tools (ModTools/ModsDoc.pdf): data paks (CDB, rooms) + .hx scripts for level structure/mob rosters only. Cannot add a second player.
- DCCM (github.com/dead-cells-core-modding/core, MIT): C# mods on .NET 10 hooking HashLink functions (Hook_<Class>.<method> += ...), events (IOnHeroUpdate), Hxbit serialization, workshop upload command in mdk/DCCMTool. Latest dev commit 2026-10-20, release 35.14.1.
- DCCM Workshop item 3633185550 and DeadCellsMultiplayerMod 3657857836: both show "removed ... violates Steam Community & Content Guidelines" (checked 2026-10-23). Reason unknown.
- DeadCellsMultiplayerMod (github.com/vaiserYT/DeadCellsMultiplayerMod, MIT, 584 commits, last 2026-10-19): ONLINE host/client (TCP 1234 or Steam P2P). Remote player is a "ghost" (GhostKing : KingSkin entity), not a real Hero. No local/split-screen.
- Hero has `controller` field of type ControllerAccess (deepnight Controller lib) — candidate for binding P2 to pad #2.

## Derived
- Path 2 (build from zero) for LOCAL co-op, reusing DCCM + lessons from the online mod (cine/death/Homunculus null-check pitfalls).
- Use the "second player inside one copy" route (playbook/split-screen.md, Dave the Diver): spawn real 2nd Hero, "P2's turn" swap of game.hero for game systems, shared camera on midpoint.

## Unknowns
- Whether a second dc.en.Hero can be constructed and updated in one Level without breaking singletons (Game.hero, HUD, inventory UI).
- Whether Steam will keep a DCCM code mod on the Workshop.

## 2026-10-23 — reverse engineering (game v35, findings from bytecode via crashlink)
- Input: tool.Controller reads ONE GamePad when `gc` is set, else merges activeGamepads (padIsDown f@2444). Physical pads go to GamePad objects in creation order (libs.heaps.GamePad.onDevice f@2472), so the boot Controller owns every pad. => P2 = new Controller(s2d), mode=ENABLE_PAD, gc = boot controller's GamePad for pad #2; boot controller gc = pad #1.
- Hero.init (f@1935) always binds `controller = Boot.ME.controller.createAccess("hero")` -> swap after init for P2.
- Hero ctor (f@4902) clones game.data.inventory; each hero has its own inventory, HeroMainSkillsManager and HealBlackBoard (flask charges `blackboard.healings`) => flasks/weapons/mutations are per-hero by construction.
- Level change: Game.activateSubLevel (f@663) and loadMainLevel (f@656) call hero.clean() then Game.initHero (f@665). Hook clean (take P2 out) + initHero (put P2 in).
- Camera: Viewport.update (f@2875) aims at tracked.spr.x/y. Shared camera = park P1 sprite at midpoint during the call.
- Mutations: PerkMaster.onActivate(by) opens `new ui.PerkSelect(by, npc)` -> each player activates Guillain themselves. TierSelect (scroll stat choice) opened from Hero.applyItemPickEffect(this).
- ReplaceItem/ReplaceSingleItem/ReplaceItemWithDual have no hero argument (use game.hero) -> "P2's turn" swap of game.hero during ctor/validate/update.
- DCCM's own Hook_Hero.init sets Game.HeroInstance = self for ANY hero -> after P2 spawns, DCCM HeroInstance points at P2. Mod uses Game.ME.hero instead.
- Build: Ubuntu dotnet-sdk-10.0 + MDK nupkg from win-x64.zip (offline, nuget.org not needed).

## Status
- v0.1.0 built and copied to <game dir>\coremod\mods\DeadCellsLocalCoop. QA level: untested (not yet run in game).

## Owner design backlog (decide after first working test — do NOT implement yet)
- Camera v2 (owner idea, 2026-10-23): camera centred on the players' midpoint and *drags* left/right/up/down
  according to each character's offset from the centre (soft dead-zone / look-ahead), instead of only moving
  when both players push the same edge. Reason: with the v0.1.1 hard screen-bound, both players must crowd
  the screen edge to move forward, which makes them run blind into enemies/traps and is risky on high-altitude
  biomes with moving platforms and pits (e.g. cloud/tower maps). Revisit once the base mod works in-game.
- Current state: v0.1.1 = midpoint camera + hard horizontal screen-bound + vertical pull-back after 1.5 s.

## Playtest 1 (owner, 2026-10-23, v0.1.1) — QA level: player reports
Worked: P2 spawned on pad 2 (P1 pad#2, P2 pad#3), movement, midpoint camera (owner happy with feel).
Bugs -> fixed in v0.2.0 (untested):
- P1 frozen after P2's ReplaceItem menu: menus open an EXCLUSIVE ControllerAccess on the boot controller
  (ControllerAccess ctor f@2477 sets parent.exclusiveId); v0.1 overwrote ui.controller without dispose ->
  boot controller stayed exclusive forever. Fix: dispose original access, give menu exclusive P2 access.
- Players pushed each other: hook Entity.canHaveRepellingWith -> false for the hero pair.
- Loot copies of swapped/dropped gear (log showed ThunderShield #720 "Equipped" copied): items ever in an
  inventory are "owned" and drops created during pick/P2 menus are never copied. DeathMoney no longer copied
  (consumables whitelisted: *Tier scrolls, QuarterUp, Food/Meat).
- Starting pipes (en.inter.Random*Spawner, meta unlocks RandomBow/RandomShield/RandomCC/Recycling): owner wants
  each player to pick their own build. Now each pipe rolls a separate item for P2 (spawnItem called again),
  reserved per player, side by side.
- P2 HUD: second ui.HUD instance built/updated under a P2 scope (game.hero, game.hud, HUD.ME -> P2's),
  leftFlow mirrored to bottom-right; P1 minimap hidden, money/cells/kills moved to top-right.
Cosmetic, owner OK to leave: P2 has no head.
- Deploy lesson: re-committing the same staged path to the PC silently wrote the PREVIOUS build (owner ran
  v0.1.1 again). Always stage each build under a new versioned folder and verify by reading it back (SHA-256)
  and checking the "[LocalCoop] vX loaded" log line. v0.2.0 dll sha256 b155a45c...de6c6 verified on PC.

## Playtest 2 (v0.2.0) -> v0.2.1
- P1 freeze on P2 menus: FIXED (confirmed by owner).
- Crash: Null access .isFullscreen in Hero.updateInteraction (P2 preUpdate) and 5x ItemDrop.minimapTracking null:
  v0.2.0 swapped game.hud/HUD.ME to P2's HUD during P2's update; P2's HUD has no minimap. v0.2.1 never swaps
  game.hud; hero->HUD reports (setHealings, updateAmmo, tiers, buffs...) made while P2 acts are routed to P2's HUD.
- Starting pipes are the ARMORY: en.inter.ArmoryButton.onActivate (f@23374) drops ItemDrops within 3 cells of the
  pulled switch (inArmory=false, gravity) and retracts every other ItemDrop in the room (tween up), other switches
  deactivate. Random*Spawner.spawnItem hook never fired for them. v0.2.1: co-op armory - each player pulls their own
  switch; first pull releases that tube only (reserved for that player), partner pulls another tube or gets a copy
  of the same one; switches turn off only after both chose. Items still inArmory can't be taken.
- P1 counters: HUD.rightFlow spans the whole screen and bottom-aligns; set verticalAlign=Top to move them up.
- Map = action 9 = pad SELECT (Share/Create/View), keyboard Tab (Options.configController f@5790).

## Playtest 3 (v0.2.2) -> v0.3.0 (verified on PC, sha256 dd9915df...)
Root causes found in bytecode:
- Life bar: Hero.updateLifeBar (f@2025) writes to ui.HUD.ME.heroLifeBar -> P2's life went to P1's bar (P1 bar showed
  P2 damage, glitched white, low-life sound looped; P2's bar never updated -> stuck orange "recover" part).
  Fix: HUD.ME = owner's HUD during updateLifeBar and during P2 scopes; P2's HUD shares P1's minimap + counters.
- Crash "Null access .alpha" in Hero.postUpdate (spr null): P2 updated after being cleaned when P1's death tore the
  level down. Guard: skip P2 pre/fixed/postUpdate when spr/_level null.
- P1 death ended the run with P2 alive: death reached Hero.onDie without Hero.kill. Down/revive now also on onDie.
- Teleport statue: Teleport.onActivate -> HUD.ME.minimap.chooseTarget(..., startTeleport(by)); minimap reads its own
  `controller` -> P1 chose. Swap to P2 access when P2 activated. Partner now follows any >8-cell jump (teleports).
- Scrolls: ids BrutalityUp/SurvivalUp/TacticUp/QuarterUp (+ choice scroll -> TierSelect) go through
  Hero.applyItemPickEffect, not pickItem. Partner gets the same scroll after the picker's TierSelect closes.
- Venus flytrap = en.ltrap.CarnivorousPlant; fixedUpdate only checks pr.Game.ME.hero. Run with game.hero = nearest.
- Dupe exploit: P1 dropped SeismicStomp via P1's ReplaceItem (outside pick depth, new InventItem instance) and P2's
  pick copied it. Now: owned by permanentId, drops during any replace menu / dropAndUpdateItem never copied.

## Playtest 4 (v0.3.0) -> v0.3.1
- Both bugs were one regression of mine: v0.3.0 pointed P2's HUD at P1's counters; HUD.onResize does
  aboveMapFlow.getProperties(bossCellCount).paddingRight -> null (counter not in P2 HUD's flow) -> P2 HUD creation
  failed -> error path destroyed P2's HUD, and HUD.onDispose (f@5346) sets this.minimap.destroyed = true -> P1's shared
  minimap died -> LevelTransition ctor (ZDoor, Giant's broken door) crashed on null getTile.
- Fix: share only the minimap; always null hud2.minimap before destroying hud2; restore HUD.ME if dispose cleared it.
- Process lesson: build.sh hid a failed build and an old dll was staged once; build.sh now fails on build errors.

## Playtest 5 (v0.3.1) -> v0.3.2
- Crash leaving the PrisonStart sub-area: P2's HUD shared P1's MiniMap; on area change Game.activateSubLevel ->
  HUD.initHero rebuilds the map, P2 HUD's onResize also ran minimap.onResize (and rebound its get_pixelScale) ->
  MiniMap.initUI reflow null .x -> P1 HUD crash. Lesson: never share HUD sub-objects between two HUD instances.
- v0.3.2: P2 HUD shares nothing; no global HUD.ME swap (only inside Hero.updateLifeBar); any exception inside
  P2's HUD code is caught and logged once instead of crashing (P2 HUD is cosmetic).

## Playtest 6 (v0.3.2, first session with no crash) -> v0.4.0 (verified on PC, sha256 f04cc7aa...)
- Cooldown icons flickering + "ready" sound loop on P1's HUD: BeheadedActiveSkillsManager.fixedUpdate (f@23217) gets
  icons via HeroActiveSkillsManager.hudGetSkillPower -> ui.HUD.ME.skillPowers; P2's manager wrote P2's timers into P1's
  icons every frame. Scanned bytecode (gamecode/hudscan.py) for every tool/hero, en/Hero, mainSkills, weapon function
  reading the HUD global (g@426): 39 functions. All per-hero ones now run with HUD.ME = owner's HUD (HudRouting.cs);
  shared ones (money, cells, minimap, death flow) stay on P1's HUD.
- Scroll given twice: P1 property was game.hero, which is P2 during P2's turn -> Partner(P2) returned P2. P1 is now a
  stored reference; and a TierSelect's resulting applyItemPickEffect (e.g. SurvivalUp from AnyUp) is not re-shared.
- P1 locked after P2 used a teleporter: MiniMap.fullscreen (f@3314) makes minimap.controller exclusive on P1's controller,
  MiniMap.restore releases whatever access the map holds. Now: release P1's access when handing the map to P2, give P2 an
  exclusive access, dispose it when the map closes.
- Open: P2 floated after a double jump until down-slamming (owner: temporary). Cause unknown - need repro conditions.

## Playtest 7 (v0.4.0, first full biome + Prison Depths) -> v0.5.0 (verified on PC, sha256 be83ca47...)
- Map not revealed: the fog of war is painted while the minimap draws (MapMask.drawRec f@3522 -> MapFow.drawRec f@3430,
  circle placed in MiniMap.postUpdate). v0.4.0 hid minimapCont, so nothing was ever revealed. Now the minimap stays
  drawn but parked off-screen (x = -100000), back in place while the full-screen map is open or ShowMinimap=true.
- P2 survival tier not shown on P2's HUD: some tier writes happen after P2's turn. Every frame the HUD tier numbers are
  corrected from each hero's real tiers (Entity.survivalTier/brutalityTier/tacticTier vs HUD.tierBar.*.value).
- Prompts stuck on keyboard after Esc: Controller.updateMainMode (f@2447) should switch back on a pad press; with P1
  pinned to one pad it did not. Fallback: any activity on P1's pad (GamePad.lastActivity) switches P1 back to pad prompts.
- Flytrap bounce: CarnivorousPlant.onTouch (f@26845) ignores any non-mob that isn't game.hero; P2's touches now run in
  P2's scope. fixedUpdate prefers the hero standing on the plant over the closest one.
- Minor forge showed P1's items: SmallBlacksmith.openUI (f@14941) builds ui.ForgeUnderground() from game.hero. Smith
  onActivate runs as the activating player; ForgeUnderground/ForgeLegendary get P2's controller like the other menus.
- HUD stacking after the end of biome 1: layout now reapplied every frame, P1 HUD alignment tracked per HUD instance,
  P2 HUD rebuilt if the game destroyed it.
- Fountain once only: Fountain.onActivate (f@13844) checks game.hero and sets `used`. Now one use per player
  (per-fountain P1/P2 flags), run as the activating player. The single vial on higher difficulties is the
  "FlaskRefill" ItemDrop from the Fountain ctor (genData.flaskRefill): now a per-player loot item (copy for partner).
- Door soft lock (Prison Depths): TriggeredDoor.onDistantTrigger (f@21492) closes the door when a plate/trigger fires
  again. If a closing door now separates the players, the other one is brought to the side of whoever closed it.
  The mutation room's door (PerkMaster.setDoor f@26126, opened when getAvailablePerks()==0 for game.hero) now waits
  for both players. New escape hatch: P2 holds Back/Select 1.5 s -> P1 is brought to P2.
- Not reproduced: the exact door from bug 8 (no screenshot of it kept); if it is not a TriggeredDoor, use the regroup.

## Playtest 8 (v0.5.0) -> v0.6.0 (built, sha256 66f88974...; deploy waits for the game to be closed)
- Fountain per player, forge as P2: confirmed working in the log.
- Prompts stuck on keyboard (again): root cause found. Haxe enum values without arguments are singletons and the game
  compares them by pointer (Controller.updateMainMode f@2447 vs g@15232/g@15009, ControlIcon.setAction f@4401 reads
  Boot.ME.controller.mainMode). `new MainMode.MM_*()` from C# creates a third value equal to neither -> icons frozen on
  keyboard and the game's own switching dead. Note HaxeEnum's C# ==/!= compare by index, hiding this. The real singletons
  are captured from the game (updateMainMode with keyboard-only mode; onAnyPadEnabled on an empty list) on P2's controller.
- Minimap: minimapCont is a child of the rightFlowR flow (positions children on reflow, so x writes were undone).
  Now FlowProperties.isAbsolute + globalToLocal to put it top-left. Visibility left to the game (boss bars hide it).
- P2 pause: Game.update opens ui.pause.DefaultPause on action 8 of game.controller only. P2's Start (GamePad index 5)
  now opens DefaultPause in P2's scope with P2's controller (same pattern as other menus).
- Scythe ghosts: the spawn closure (f@36200, tool/weap/AdeleScythe.hx) passes game.hero as parent. Ctor hook swaps the
  parent to P2 when the scythe item is in P2's inventory.
- Diverse Deck: pow.DiverseDeckBase.doActive reads owner._level.game.hero (transformInventoryItem on P1). doActive of all
  deck variants + Base.fixedUpdate run in the power owner's scope. P2's own hero update runs with game.hero = P1
  (AsP2Acting swapHero:false), so other skills that read game.hero may show the same bug.
- Kills doubled: Mob.notifyHeroOfDeath (f@1062) calls onMobDeath -> addKillCount on every living hero. Now counted once
  per mob (same hero may still add twice, e.g. Scissor).
- Selling: ItemDrop.recycle (f@10054) destroys the drop; a parity copy for the partner is now left like for picking.
- Reward altars (owner idea: one room per player): instead, each player chooses. ItemAltar.disable(picked) (f@16519)
  disables every altar in the same room+row; on the first pick those are kept enabled for the partner only (plus the
  parity copy of the first pick). Partner's pick (altar or copy) closes the group.
- Life bars: HUD.updateLifeSize (f@5325) widens the bar 217..350px with max life; in co-op a HUD that would cross the
  middle is called again with overrideMaxLife=1.0 (= smallest bar).
- Door after the cursed chest still closed (image: orange barred door next to the chest). Owner asked for a button:
  L3 (GamePad index 10, "analogClick") - P1's L3 brings P2 to P1, P2's L3 brings P1 to P2. Replaces P2's Select-hold.
- GamePad toggle indices: A0 B1 X2 Y3 Back4 Start5 LT6 RT7 LB8 RB9 L3 10 R3 11 Dpad12-15 (from the MAPPING init).
- Tooling: gamecode/dumpall.py writes gamecode/alldis.txt (all functions, call targets named) - grep it instead of
  decompiling one function at a time.

## Playtest 9 (v0.6.0, first full run start to Bank: "fully playable with minor issues") -> v0.7.0 (verified on PC, sha256 dbd3c5f8...)
- Confirmed working: L3 regroup (used constantly), per-player altars, fountain, forge, P2 pause, prompts (no complaint).
- Shop/decline infinite copies + occasional 3rd copy: the partner's copy was made right after Hero.pickItem returned,
  but the pick can still be undecided (ReplaceItem screen, shop confirmation) and declined. Now the copy waits until the
  pick is really done (source destroyed / item moved / altar disabled / item in inventory); a declined pick (no pick
  menu open, item still in place) makes no copy and un-marks the item as owned. Shop booths (ShopBooth.buy is deferred,
  outside onActivate) never make copies.
- Boss arena: cine.EnterRoomBoss & co. call BossDoor.closeAndLock (f@8875) when P1 (game.hero) walks in; the partner is
  brought inside if the door separates them.
- Bank keycards: LockedDoorButton.hasKey (f@25290) counts keys in game.hero's inventory only. The card reader now runs
  as whichever player carries the key (canBeActivated/onActivate/onFocus).
- Diverse Deck Watcher: charge stored in game.data.gameFlags[DiverseDeckWatcher.gameDataChargekeyName] -> shared.
  P2's Watcher methods run with that static name swapped to "<name>_coopP2".
- Still open: P2 floating after double jump (random, no trigger found).

## v0.7.1 (owner request, no playtest yet; verified on PC, sha256 96fa6304...)
- "Find other places like the bank keycard": bytecode scan (gamecode/invscan.py: functions doing
  X.hero.inventory.hasItem/countItemKind/removeOne) found key checks on game.hero only in: LockedDoorButton (key/card
  doors), BankSubTeleporter.get_heroHasCard, CemeteryGraveCave.hasKey/consumeKey, HiddenTrigger.trigger (trigger zones
  incl. boss rooms), Merchant.talk, EliteShrine, BreakableGroundConditional, RedTeleporter, VineLadder, HomonFountain,
  TopLighthouseGetAway, Mob.onDie/checkForExecute (item effects, not keys).
- Fix: shared key ring. Inventory.countItemKind/hasItem/removeOne on a player's inventory also see the partner's KEYS
  (ids *Key, *Key<n>, *Keycard, ALKey*; not Meta* runes, Richter*, skins). Using a key the asked player lacks
  takes the partner's.
- Boss doors: the anchor is now whoever stepped on the HiddenTrigger that started the boss cinematic (P1 or P2).
- Not found in code: a "rose key" item; Rose ids are room names (RoseKeyConverter*) - covered by the key ring if
  their keys follow the *Key naming.

## v0.8.0 (owner requests; verified on PC, sha256 fa4e226c...; not playtested)
- Choosing P1's controller: pads are ordered by hxd.Pad.index (stable per physical controller, so "black" was always
  P1). New saved config SwapPads flips it. In game: hold Back+Start for 1 s on a controller -> it becomes P1 (rumble +
  "PLAYER 1"/"PLAYER 2" pop text); F10 swaps from the keyboard. Pad names are now logged.
- Outfit room / head room "simulation" (from bytecode): outfit = User.heroSkin, head = User.heroHeadSkin, ONE value in
  the profile; every hero is drawn from it (Hero.getSkinInfo -> User.getHeroSkinInfos; HeroHead.init/initHead read
  heroHeadSkin). Skinner/SkinSelector and TailorDaughter/HeadSelector open with P1's controller and apply to game.hero
  (SkinSelector.close -> game.hero.applySkin, which also writes the profile). Before v0.8.0: P2 at the wardrobe would
  have opened the menu for P1's pad and changed P1's outfit; P2 would then also take that outfit at the next door
  (P2.initGfx re-reads the profile). Heads: same, and P2 has no visible head anyway.
- Now: P2's activation opens the menu on P2's pad as P2; the choice is saved as P2Skin/P2Head in the mod config and the
  profile is put back to P1's choice; P2 is drawn with P2Skin/P2Head (getSkinInfo/getGlowColorsData/HeroHead.init
  run with the profile values swapped). The camera is put back on the shared view if a menu left it on P2.

## Published (2026-10-27)
- Mod repo: https://github.com/Osman-Mujic/DeadCellsLocalCoop (public, MIT), release v0.8.0 pre-release with
  DeadCellsLocalCoop-0.8.0.zip (SHA-256 a92963d2...096e6; the DLL inside is the build installed on the owner's PC,
  fa4e226c...). Draft PR to coop-anything: cranebeaker/coop-anything#237.
- Build note: the MDK's props read an MSBuild property named DCCM_ROOT, so an environment variable with that name
  breaks the build (every DCCM type "not found"). build.sh now takes DCCM_DIR.
