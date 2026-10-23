# Tainted Grail: The Fall of Avalon: working notes

Raw notes, written as we go; the write-up is README.md. Claimed in
https://github.com/cranebeaker/coop-anything/issues/30 (2026-10-19).

## Triage (playbook/triage.md)

1. **Which game.** Tainted Grail: The Fall of Avalon (Questline / Awaken Realms, PC).

   | | |
   |---|---|
   | Store | Steam, app 1466060, beta branch `mono` |
   | Build | buildid 25966682, game version 1.30.102 (`[1.30a]`) |
   | Main exe SHA-256 | `e01f1a4cfa8ebb93f7d5c9ea3925cef2883e1d230741ede76ad4f4c7d1bcd594` (`Fall of Avalon.exe`) |
   | Engine | Unity 6000.0.41f1, Mono scripting on the `mono` branch |
   | OS | Windows 11 |

   The default Steam branch is presumably IL2CPP (TGCoop's description says BepInEx 5 can't
   hook it); not checked by us. The `mono` branch is an official Steam beta, so a co-op mod
   for it needs no game patching beyond the mod loader.
2. **Already done?** One co-op mod exists, TGCoop (below). Nothing in this repo before our
   claim.
3. **Engine.** Unity, Mono scripting backend (`Fall of Avalon_Data\Managed\`, 279 DLLs; the
   game log says "[Modding] Mono build"). Game code in `TG.Main.dll` plus `Awaken.*.dll`.
   Unity co-op mods for other games (Nitrox, HKMP, QSB, CelesteNet in
   playbook/engine-families.md) show the usual BepInEx/Harmony route; none target this game.
4. **How much source.** Closed binary, but Mono: the managed DLLs decompile to readable C#
   with original names (ilspycmd 11.1, about 6,200 files for `TG.Main`). BepInEx 5 +
   Harmony load and patch it. The developer also publishes an official modding toolkit,
   [AR-Questline/merlin-workshop](https://github.com/AR-Questline/merlin-workshop), a Unity
   project with "a simplified version of the game's source code". It makes **asset and data
   mods only**: it "allows you to replace Addressables assets" (asset bundles, `catalog.json`,
   `mod.meta` in `%USERPROFILE%\AppData\LocalLow\Questline\Fall of Avalon\Mods`), so it
   cannot carry a co-op mod's code. Its license is an EULA for the open-beta tool
   ("by using the Tool, you agree"): free use solely for making mods for the game, no
   profit, no sharing or modifying the tool itself, no reverse engineering or decompiling
   the tool or the game. We did not use it.
5. **Multiplayer in the engine.** None. The game ships Steamworks.NET and Heathen (Steam
   features, cloud saves), GOG Galaxy, and Unity's engine module `UnityEngine.MultiplayerModule`,
   but no networking library. A moderator on the Steam forum: "There will not be any form of
   multiplayer in this game" (thread 686368595766265552). So co-op means building a sync layer
   with host authority.
6. **Shipping.** A BepInEx plugin ships without game files. No anti-cheat in the install
   (no EasyAntiCheat or BattlEye folder) and no online service. The game sends automatic bug
   reports with saves attached when a load fails (Unity User Reporting): block them in test
   copies.

## Existing work

- **TGCoop** ("2 Players Steam Coop"), https://www.nexusmods.com/taintedgrailthefallofavalon/mods/211,
  author hektormenta. BepInEx 5 + Harmony, 2 players, Steam lobbies and
  SteamNetworkingMessages on the game's own app id, host authority. Newest build we saw:
  0.5.36 (`TGCoop.dll` dated 2026-07-28; zip SHA-256
  `6a4c372e82b026668e7e75fe0d93ad771b5ce0f83140bd72a5a0ea65315d272e`). No license file, no
  source; readme in Spanish. The page states it was built with AI assistance.
  - Session model (its readme): both players load their own save, then the guest joins and
    the host pushes a baseline.
  - Story sync (from reading the decompiled DLL): live hooks on `StoryFlags.Set`,
    `QuestUtils.SetQuestState`, `QuestUtils.ChangeObjectiveState`; on join the host sends only
    the true boolean facts of each memory context. Other writes to the game's flag store
    (`GameplayMemory` → `ContextualFacts`: NPC actor states, discarded places, story
    variables, visited scenes) and false or non-boolean values are not synced.
  - Bug reports (Nexus comments, July to October 2026): after the Odrán dialogue the fortress
    gate stays closed for the teammate; dialogue started in a boss fight locks the teammate's
    movement until a rejoin; constant combat desync (flickering, teleporting); A-pose unless
    both players use third person; the mod's dialogue box covers the game's choices;
    "buggy mess prone to softlocks". The author (2026-07-14): a hobby project, only Act 1
    really tested; the first-person body pose is reconstructed because the game never
    computes one. No author reply to the September and October comments.
  - More comments, 2026-07-21 to 2026-08-06 (pasted by the owner 2026-10-20; author replies
    2026-07-26 to 2026-07-28, none after):
    - Players: the guest gets stuck running in place near the Archspire teleport (F3, teleport
      to the host, frees them; 2026-08-06); enemies alive for one player and dead for the
      other; a boss dead for one only; joining a player who is behind auto-completed their
      Act 1 quests but the progression flags did not follow; side-quest recipes (blacksmith)
      and Druid Shrine rewards reach only the player in the dialogue; "general bugs and
      crashes", fixed by the host quitting and rejoining.
    - Author, 2026-07-26, two bugs fixed in 0.5.36: (1) "The game stops simulating anything
      far from your own camera"; the host reported enemies it was not simulating as "attacking
      nobody", so the guest's AI retargeted them (one monster chasing both players), and the
      host's frozen health overwrote the guest's hits (enemies "nearly unkillable"). Fix: the
      host reports "I can't observe this one" and the guest stays in charge of that enemy
      until the host can see it. (2) "The game refuses to save while it thinks you're in
      combat"; a phantom attacker stayed latched onto the host, so every save, manual and
      auto, was refused silently for 4 hours; a host death then lost those hours. "Changing
      area clears the stuck state."
    - Author, 2026-07-27: play the prologue alone, then connect; each player keeps their own
      character, inventory, gold, XP and level, only world and story are shared; both must
      play or both skip the prologue, else story triggers break ("Caradoc's boat simply
      refusing to work"); the guest's story "converges toward the host's".
    - Author, 2026-07-28: version mismatches were only written to the log (fixed: on-screen
      warning); bosses are a weak spot because "their internal IDs differ per machine and
      have to be paired by position"; dialogue voice lines are not played for the listener,
      only text and choices travel; two players are "baked fairly deep" (one partner, one
      stand-in body, one connection).
  - Derived, not tested: since each player keeps their own world and only those three hooks
    are synced live, world state outside them can drift; a plausible cause of the closed
    gate.
- Mentions without a name: a Steam forum moderator in January (year not shown, presumably
  2026): "Afaik someone is working on a multiplayer mod, but no idea how far along that one
  is" (thread 686368595766265552); a player in April: "Check out the discord mod chat
  channel. There is someone working on it" (thread 796712917458249380); later posts about a
  BepInEx co-op mod on Nexus (585058364697005929, 589558792744841182), presumably TGCoop.
  Whether the January and April mentions are TGCoop's author or another project: unknown.
- A Nexus comment (2026-07-15) claims its writer built local and online co-op and that "two
  other people made AI versions"; no project, link or build found.
- Not co-op: RealSkye (https://ko-fi.com/realskye) makes Tainted Grail mods (Avalon Atlas,
  Better UI, Better Movement, Better Mounts, Wyrd Sight, Avalon Mod Manager) and No Man's Sky
  mods. The page, read by the owner on 2026-10-20, lists no co-op mod for any game. (Ko-fi
  returns 403 to automated fetches. Two search-engine summaries of the page had claimed a
  co-op work in progress, once for Dying Light: The Beast, once for Tainted Grail "in the
  Restored Land"; neither is on the page. Search summaries of a page we cannot open are not
  a source.)

## Where we looked (2026-10-19, rechecked 2026-10-20)

- This repo: DIRECTORY.md, `games/`, issues and PRs: nothing before our claim.
- Web search: English ("Tainted Grail Fall of Avalon co-op mod", "multiplayer mod",
  "coop online mod BepInEx", Reddit co-op threads, TGCoop and its author by name), Polish
  ("Upadek Avalonu tryb kooperacji mod multiplayer"), Russian ("кооператив мод мультиплеер");
  added 2026-10-20: German ("Koop Mod Mehrspieler"), Chinese ("圣杯传说 阿瓦隆的陨落 联机 mod";
  the Chinese title is 《污痕圣杯:阿瓦隆的陨落》), Japanese ("マルチプレイ 協力 MOD"), "new release
  October 2026", Steam guides. Only general articles, TGCoop (still 0.5.36, last update
  2026-07-28, per its Nexus listing in search results) and the board game (a separate,
  co-operative product). The search engine indexes Nexus poorly.
- GitHub: repository search ("tainted grail coop/multiplayer", "fall of avalon
  coop/multiplayer/mod", "TGCoop", "avalon coop"), code search (`Awaken.TG.Main`); added
  2026-10-20: the 30 most recently updated "tainted grail" repos (to 2026-10-19) and topic
  `tainted-grail`. Mod collections, the official toolkit and single-player mods, no co-op.
  Several repos named "free full version", "trainer undetected", "cheats" look like download
  lures: not opened.
- Nexus: TGCoop's page, comments and files (pages blocked to automated fetches; read by the
  owner and from the downloaded build).
- Steam forum: search "coop mod" (24 results), threads above. Developer statement found
  2026-10-20, thread "Add Co-Op" (4630359473422132923), kpx_ar [developer], 2024-12-03: "To be
  honest, that's unlikely to happen. This game was made with a solo-only experience in mind."
- RPGWatch "mod support" thread (63758): 403 to automated fetches; read by the owner
  2026-10-20: no co-op project.
- ModDB, Reddit: through web search only, nothing found. Archive.org: not checked (the game
  is from 2025). The game's Discord: not checked (owner's decision, 2026-10-19).

## Decision: build from zero, not revive TGCoop (owner's decision, recorded 2026-10-19)

Path 2 instead of path 1. Reasons: TGCoop's reported bugs; its way of handling sync and saves
(each player loads their own save, the host overlays only true story flags, so world state
outside those hooks can diverge); and visible band-aid fixes in its code ("Guard" patches such
as `SaveLoadGuard`, `CombatFlagGuard`, `TppIkGuard` that suppress symptoms). Doing it properly
from scratch looked like the better route. No TGCoop code is used; its audit informed the design
(host's world is the shared world, guest character merged into the host save on load).

## How the game works (for co-op)

### Observations (from the decompiled code)

- Own MVC framework (`Awaken.TG.MVC`): `Model` with string IDs `<Type>:<counter>`, nested
  elements `<parentId>:<Type>:<counter>`, counters per type in `IdStorage`, events through
  `ModelExtensions.Trigger`, services through `World.Services`.
- Story state is a key/value store: `GameplayMemory` → `ContextualFacts.Set/Change/Remove`.
  `StoryFlags.Set`, quest state (`Context(quest).Get("state")`) and objective state all live there.
  Other writers include `ActorsRegister` (`Context("actors")`), `Location`
  (`Context("discarded.places")`), enemy behaviours, story variables, visited scenes.
- One player everywhere: `Hero.Current` is static and referenced in about 1,240 places.
- Saves: one zip per slot, one `.data` file per domain (`Gameplay.data`: hero, items, quests,
  memory; one file per visited scene; `MetaData.data`). All saves and prefs go through
  `CloudService` (Steam Cloud, Steam without cloud, GOG, debug).
- Models are saved and restored in world registration order, in one `RestoreWorld` pass. While
  restoring, world quests read hero stats (`StatTracker`), and hero elements read world models
  (`TrespassingTracker` → `GameRealTime`), so hero and world depend on each other at load time.
- New Game+ (`NewGamePlusUtils`) already extracts a character (hero subtree, hero and stash items
  minus quest items, keys and readables, journal, achievement quests, game time) into a blob and
  restores it into another world (`LoadSystem.DeserializeNewGamePlusCache`).
- Difficulty is a setting keyed by the character's `HeroID` GUID in PlayerPrefs, not part of the
  save slot. Story, Easy, Normal, Hard, Survival (save restrictions), Challenge (locked, adds a
  `HeroChallengeMode` element to the hero).
- The game ships Steamworks.NET + Heathen, GOG Galaxy, Unity User Reporting (automatic bug
  reports with recent saves, logs and settings attached when loading fails), Sentry, GameAnalytics,
  and its own `Automations` hook on the title screen.

### Character transfer experiment (2026-10-18)

Goal: put player B's character into player A's world, so a co-op guest keeps their character.
Method (BepInEx plugin, test install only):

1. Export B's character with the New Game+ selection and serializer.
2. Load A's save normally. Right after the Gameplay domain is restored, discard A's hero and its
   items, then deserialize B's blob. While reading the blob, every `Item:<n>` ID is offset by
   100000 (hooked in `SaveReader.ReadAscii`, which all IDs pass through) so B's items cannot
   collide with A's world items; B's journal, achievement quests and game time are skipped.
3. Teleport B's hero to A's hero position after loading (the blob carries B's old position).
4. When saving, move the hero subtree and imported items back to their normal place in the model
   order (after `PlayerJournal`), otherwise the reload restores quests before the hero and fails.

Results:

- Steps 1 to 3: loads into gameplay, hero controllable. Same character (two saves of one
  playthrough) as source and target.
- Step 4, save then fresh start then plain load: hero, items and position intact, no errors beyond
  a normal load of the same save.
- Two different characters (2026-10-18): "Berian" (level 3, 251 gold, 45 items, new game) into
  the world of "Selvar" (level 2, 129 gold, 151 items). After swap, save, fresh start and plain
  load: Berian, level 3, 251 gold, 41 items at Selvar's position; no errors beyond a normal load.
  The 4 missing items are the quest items, keys and readables the New Game+ filter drops.
- Keeping all items (2026-10-18): during export, `NewGamePlusUtils.ShouldItemBeSaved` is
  overridden to keep every item owned by the hero or `HeroStorage`. Berian: 45 of 45 items after
  swap, save and reload.
- Spawn during loading: prefix on `LoadingScreenUI.CleanupAfterLoading` teleports the imported
  hero to host position + 1.5 m along the host's right vector, snapped with
  `AstarPath.active.GetNearest(pos, NNConstraint.Walkable)`. Host at (-95.99, -0.23, 59.27), guest
  spawned at (-94.65, -0.24, 58.62) before the loading screen faded. Setting `Hero.MoveTo` right
  after the blob restore did not stick (position not yet tracked down).
- Orders that did not work: restoring the guest before the host's Gameplay domain (hero needs
  world models); loading the host's Gameplay domain without its hero (quests need the hero).

### Planned save design (owner's decision, 2026-10-18)

- Co-op saves are separate from solo saves. The host's save is the only source of truth: the
  world plus one character file per player, keyed by Steam ID.
- On join the host sends the whole save; the guest loads it and swaps in their own character
  (the transfer above). On save or leave the guest sends their character back to the host.
- One world means no divergence between copies. A player new to a save starts as a copy of the
  host's character (fallback); later maybe a fresh character.
- Co-op uses the host's difficulty.

### Co-op save data (2026-10-19)

- Loading a slot copies **every** zip entry into the domain cache (`LoadSave.LoadSaveSlotToCache`)
  and every save writes **every** cached entry back (`SaveSystem.SaveDomainsAsync`). Extra entries
  therefore travel through saves, autosaves and quicksaves with no hook; unknown names are safe
  (`LargeFilesStorage` creates an empty record on demand). Entries are written with
  `LoadSaveDomainsCache.GetCachedDomainFileWriteStream` + `SaveDomainFileSize`, outside a save.
- End of a save: `SaveInProgressHandle.Dispose` (held for the whole `SaveAsync`).

### Remote player body and animation (2026-10-19)

Observations (decompiled code unless marked "tested"):

- The hero's third-person body is the prefab `VHeroController.maleHeroBodyTPP` /
  `femaleHeroBodyTPP` (male: `HeroMale_TPP`). Tested (component dump of an inactive instance):
  hero-only components `HeroBodyData`, `ARHeroAnimancer`, `HeroAnimatorClipPlayer`,
  `VCTppSpineIK`, `VCTppHeroAnimationRigging`, `RigBuilder` (+ 2 `Rig`, 6 `MultiAimConstraint`),
  `VCHeroRaycaster`; generic visual parts `KandraRig`, 3 `KandraRenderer`,
  `AnimationPostProcessing`, 14 `MagicaCapsuleCollider`, `CharacterDefaultClothes`,
  `RenderersMarkers`, `RagdollController`.
- `ARHeroAnimancer` is wired to the local hero: its `OnAnimatorMove` moves
  `Hero.Current.VHeroController`, and it reads `Hero.Current` when animations load. A second body
  must not keep it.
- `ARAssetReference` holds a single load handle (`LoadAsset` twice logs "already used"); the
  hero's body is loaded through the reference stored on `VHeroController`. Load other copies
  through `DeepCopy()`.
- Hero animation: layers (legs, weapon FSMs, overrides) each play a `HeroStateType`, resolved to
  clips through `ARHeroStateToAnimationMapping` assets: base mappings plus overrides that items
  apply on equip. Legs in third person are a 2D mixer fed `Hero.RelativeVelocity` (y, x)
  (`TppMovementState`). A remote body can replay "state per layer + mixer parameter" through the
  same mappings.
- Appearance is `BodyFeatures`, an element any `IWithBodyFeature` owner can carry (hero, NPCs,
  photo mode's `WorldHeroRenderer`); it serializes on its own and has `CopyFrom`.
- Photo mode (`WorldHeroRenderer`) already shows a copy of the hero in the world with features,
  equipped clothes and loadout poses, but is tied to `Hero.Current`.
- Bot movement: `PlayerInput.ProcessLateUpdate` sets `MoveInput` from Rewired each frame; a
  postfix that overrides it walks the hero through the game's own movement code (tested).
- Current area: `World.Services.TryGet<SceneService>()?.ActiveSceneRef?.Name`.

### Steam Cloud (from the decompiled code, 2026-10-19)

Observed in the code (`Saving/Cloud/Services/*`, `Saving/Cloud/SteamCloudOrigin.cs`):
- The game picks `SteamCloudService` when Steam Cloud is on for the user, else
  `SteamNoCloudService` (`CloudService.CreateCloudService`); GOG has its own service. All save
  reads and writes go through it. The game uses the Steam API itself (`ISteamRemoteStorage`
  through Heathen's wrapper): saves are `Saved/<file>.zip`, settings `Saved/PlayerPrefs.data`.
- A save is `BeginSave(slot ID)`, `SaveSlotFile` per entry (stored as `<name>.data`),
  `EndSave(file name)`; under Steam Cloud `EndSave` also uploads. `LoadSave.Save` pauses Steam
  callbacks (`App.canRunCallbacks`) and `SavingWorldMarker` ends with `EndSaveBatch` and
  `PrefMemory.Save`.
- Each machine keeps a record of synced files with both timestamps (`SteamCloudOrigin`, in
  the *unsynchronized* prefs, `Unsynchronized/PlayerPrefs.data`, plain JSON). Writes update it
  in memory; `PrefMemory.Save` stores it.
- Startup sync (`InitCloud`), per file: local only and no record → upload; cloud only and no
  record → download; cloud only and the record knows it → **deleted from the cloud** (taken as
  deleted on this machine); local only and the record knows it → **deleted locally** (unless
  the local copy is newer: conflict); both present and no record → conflict dialog; both
  changed since the record → conflict dialog.

Derived (for mods and test setups):
- Write saves only through the cloud service and end with `PrefMemory.Save`; a file added or
  removed behind the game's back is uploaded, deleted or a conflict at the next startup.
- A test copy with Steam Cloud on needs a data folder of its own that is never used without
  Steam Cloud (its record and its files must stay in step).
- A guest's received save carries the host's large-file references
  (`SaveSlot.usedLargeFilesIndices`, `lf_*` files via `LargeFilesStorage`); they mean nothing
  on the guest. Deleting that save runs `RemoveSaveSlotUsedFilesData`, which could remove a
  guest's own large file of the same index. Unknown what writes large files; none existed on
  the test PC (searched 2026-10-19).

Unknown: whether Steam also runs Auto-Cloud for this game's save folder (the NieR: Automata
notes saw Auto-Cloud upload a test copy's folder on exit). The test copies' folders are
redirected, so Auto-Cloud on the real folder would not see them. A one-off test on a real
Steam Cloud (`tools/steam-cloud-test.ps1` in the mod repo) is written but not run yet.

### Derived (not tested)

- Quest items, keys and readables in a co-op save belong to the same story, so they are kept.
  Edge case: a guest file older than the host's world may carry quest items for finished quests.
- Co-op should use the host's difficulty; Survival and Challenge need special handling.

### Unknowns

- Transfer between saves at very different story points (both test worlds are in the prologue).
- Whether equipped items, skills and talents all survive the transfer.
- Which difficulty effects are computed where once enemies are host-driven.

### World design (2026-10-19, owner's decisions)

- Each game's own player is its only `Hero`; a remote player is a stand-in (an NPC the
  world can see and fight, dressed with the player body). A second real `Hero` per guest on
  the host was tried and rejected: the hero is built as the local player (views, camera,
  ~20 runtime parts), so every system needed another exception.
- The guest's world is a view of the host's, following the playbook's host-authority rule
  (`playbook/methods.md` 5, from FNV): the host runs every story and world script; a
  guest's action is a request the host runs; effects on a hero (items, gold, XP) go to the
  right player by a rules table and are applied there with the game's own calls. No story
  step may run on a guest (to be a test). A first draft let the guest's game run its own
  actions, story included; corrected because it contradicts that rule.
- One area for the whole party to start; downed and revive; no menu pause.
- Open: what is shared and what is per player (draft table: story, world, NPCs, time
  shared; inventory, level, stats per player; reputation, crimes, map, reward split
  proposed); how a story run for a guest reads the guest's stats (the host's hero is
  `Hero.Current` there).

## Known and expected bugs

What TGCoop players report, the cause where its code shows one, and what our design must do.
"Not built" means the live-sync part does not exist in our work yet.

| Reported bug | Cause in TGCoop 0.5.36 | Our design |
|---|---|---|
| Enemies in different places for the guest; "the mob jumps around while chasing the host" | Guest keeps running its own enemy AI; host sends snapshots every 0.12 s for up to 32 NPCs within 60 m; guest only corrects drift above 1.2 m (2 m in melee) with lerped teleports. Two AIs fight over each enemy. (observed in code) | Not built. One brain per enemy: AI runs on the host only; guest-side NPCs have AI off and interpolate the host's position, rotation and animation state. |
| Enemies mismatched or unsynced | Each player loads their own save, so NPC IDs differ; matched by ID prefix and distance (35 m). (observed in code) | Mostly solved by the single host save: same IDs on both sides at join. NPCs spawned during play need host-assigned IDs. |
| Guest stuck after a dialogue or encounter (Conchur at the asylum, boss fights) | Guest's own story triggers fire locally. (guess, not reproduced) | Not built. Story triggers and scene starts run on the host only; the guest receives results and never enters a blocking story state on its own. |
| No animations in first person | The game never computes a full body pose in first person; TGCoop reconstructs one. (from the author's reply) | Not built. Send movement state (speed, direction, attack, block, cast), animate the remote player on the receiving side with the normal third-person animations. |
| Gate stays closed for the guest; story out of sync | Only `StoryFlags.Set`, quest state and objective state are synced live. (observed in code) | Single host save plus syncing every `ContextualFacts` write. |
| Mod dialogue box covers the game's choices | UI layout. (player report) | Design the spectator dialogue UI so it never overlaps the local one. |
| Only 2 players | Hardcoded. | Character files are per player already; start with 2. |
| Enemies near the guest but far from the host: one monster chases both, enemies "nearly unkillable" | The game stops simulating entities far from the local camera; the host reported its frozen copies as idle and overwrote the guest's damage. Fixed in 0.5.36 by leaving unobserved enemies to the guest. (author's reply) | Not built. Stage 3 must decide what the host does with enemies near a guest and far from itself: keep them simulated on the host, or hand authority over (playbook stage 3: "an authority hand-off rule"). Test: guest fights an enemy far from the host; health goes down and stays down. |
| Host saves silently refused for hours | The game refuses to save in combat; a phantom attacker kept the host "in combat". Fixed in 0.5.36. (author's reply) | Not built, and a real risk: with host-only saves the host's save is the only copy. Remote-player stand-ins must never stay a combat target after a fight; tell both players on screen when a save is refused. Test: after a fight involving the guest's stand-in, the host can save. |
| Boss dead for one player only | IDs differ per machine, paired by position. (author's reply) | Same IDs on both sides from the single host save; deaths come from the host (stage 3 exit test). |
| Quest rewards (recipes, Druid Shrine) only for the player in the dialogue; flags missing after joining a player who is behind | Each player keeps a separate world and hero. (player report; cause a guess) | Partly decided: one world (the host's). Per-player rewards follow DESIGN.md's shared-or-per-player table (open rows). |
| Guest stuck running in place near the Archspire teleport | Unknown. | Unknown until we reach that area; add to the test route then. |
| Dialogue voice not heard by the listener | Only text and choices are sent. (author's reply) | Not built (story stage). |
| Version mismatch only in the log | (author's reply, fixed later) | Our handshake checks versions; whether the guest sees the refusal on screen: unknown, check. |

### Seen in our own tests

- **Guest's tracked quest leaks into the host world** (2026-10-18 join test, old transfer): after
  joining, the guest's HUD tracks "Set a bonfire", a quest from the guest's own world. Not seen with
  the combined load: the guest tracks the host world's "The Great Escape". ("Set a bonfire" turned
  out to be the talent-points hint, not a quest.) The hero's
  `QuestTracker` element travels with the character. The tracked quest must come from the host
  world (or be cleared on import).

- **Received save keeps the host's slot ID** (2026-10-18): saved as `Coop_QuickSave_6.zip` but
  its metadata ID is the host's `QuickSave_6`. At the guest's title scan two slots claim that ID;
  the game renames whichever loads second (alphabetical file order), and `SaveSlot.IsQuickSave`
  is `ID.Contains("QuickSave")`, so a co-op slot can pass for the guest's quicksave. Co-op slots
  need their own IDs. Still open (2026-10-19): since co-op save slots the host always sends a
  `Slot_N`, so the quicksave case is gone, but the received slot still keeps the host's `Slot_N`
  and collides with the guest's own slot of that number. Seen at the guest's title scan:
  `Slot_38 file=Slot_31`, `Slot_35 file=QuickSave_6` (a leftover from the old design). The mod
  finds slots by file name and is not affected; a test that loaded by ID loaded the wrong slot
  after a restart. **Fixed 2026-10-19**: while loading the received slot's metadata the game
  already gives it a fresh ID on a collision (`SaveSlot.EnsureIdIsValid`, in memory only).
  The guest now reads the metadata from the received bytes and writes the whole slot through
  the cloud service as the game's own save does, with freshly serialized metadata holding the
  new ID (see "Steam Cloud" below). Test: the guest owns a slot with the host's ID; after
  receiving, the zip's `MetaData.data` must hold the guest's new ID (twin: the host's
  metadata is stored unchanged, the check fails). Full suite passes.
  - Game finding on the way: `SaveSlot.ApplyChanges` (what renaming a slot calls) never
    writes to a slot on PC. `LoadSave.SaveMetadataDomainSynchronous` loads
    `Saved/<file>.data` through the cloud service, but slots are `Saved/<file>.zip`
    (decompiled `SteamCloudService` and `SteamNoCloudService` both add `.data`), so it finds
    nothing and returns. The next full save writes the metadata anyway. A first fix patched
    `MetaData.data` in the zip directly (worked, but behind Steam Cloud's back); replaced.
  - Superseded the same day by host-only saves: the guest no longer stores the world at all.

- **Host-only saves** (2026-10-19, owner's decision, built): the host's co-op save is the only
  permanent copy. The guest keeps the received world in memory: a slot is made from the
  metadata in the received bytes, and a Harmony prefix on `ZipArchiveUtils.LoadArchive` (where
  Steam's cloud services open a slot zip) serves it from memory, so the game's own load runs
  unchanged. The guest's game can't save: the game's own `SaveBlocker` model (used by duels,
  boss fights) stops player, quick and auto saves but not the domain caching on area changes.
  The guest's "save my character" sends its character; the host saves at once only if it
  could save by hand (`LoadSave.CanPlayerSave`), else at its next save. Tested: the guest's
  save folder is byte-identical to its baseline after a session; quick save refused.
  - Replaced bug class: rejoining used to silently drop a guest's solo progress in its
    `Coop_` slot. Gone, since there is no guest slot.
  - Game finding: `LoadSave.Save` itself ignores save blockers (only the `Can*Save` checks and
    the menus look at them); `SavePostpone` is the only check inside `Save`.
  - Game finding: saving needs `Hero.Grounded`, which is false while swimming: the movement
    system zeroes vertical velocity in water, the character controller reports no ground,
    and the 0.5 s ground-touched timer runs out (`VHeroController.UpdateGroundTouchedTimeout`),
    although the ground sphere check still hits. The test bots' walks on the prologue beach
    sometimes end in water, so a test that needs the host to save must put it on land first.
  - Gaps: GOG's cloud service opens slots differently (a received world would not load);
    the guest's quick save key does nothing yet (should be "save my character"); a host
    save doesn't collect guests' current characters yet.

- **World design findings** (2026-10-19, from the decompiled game; design in the mod's
  DESIGN.md):
  - One hero: `Hero.Current` is a static property set by the hero model itself, read 1,190
    times in 514 files. Enemy noticing (`Perception`) only looks at it; fighting uses the
    generic `ICharacter` target NPCs use against each other.
  - Every interaction enters through `AbstractHeroAction.StartInteraction(Hero, interactable)`,
    every story through `Story.StartStory` and `StoryStep.Execute` (~210 runtime step types),
    every story flag through `ContextualFacts.Set/Change/Remove`. Actions mix effects in one
    method (`PickItemAction`: creates the item, records the theft, fills the actor's
    inventory, may start a dialogue, removes the pickup).
  - Story triggers fire on physics contact with the local hero's controller (`HeroTrigger`).
  - IDs are per-type counters saved with the world (`IdStorage`): identical in two games from
    one save, divergent as soon as either creates something.
  - Areas are separate save domains (`Domain.Scene`), loaded one at a time.
  - **Proof, rejected: a second, data-only hero on the host for a guest.** Patching the
    `Hero.Current` getter at plugin load does reach the game's code (~1,100 reads/s); patched
    later, already compiled code misses it (inlined). But a hero that is not the local player
    needs exceptions everywhere: views (controller, camera, HUD, keys, crosshair), ~20
    runtime parts that assume a body and camera, the wyrd stalker, the world's "only hero"
    lookups (`World.Only<HeroCombat>` and ~45 more), body bones. With six such exceptions it
    loaded and the host ran a pickup for it (item in the guest's inventory), but it reacted
    differently from a played hero, and the pickup's tutorial appeared on the host's screen.
    Conclusion: the hero is built as the local player; remote players are better stand-ins
    (the Skyrim Together approach, playbook how-coop-mods-work.md 2.1).
  - Playbook lesson candidate (phase 8, with the owner's OK): a proof needs a question, a
    yes/no, a time box and a stop rule written down first; a failure inside a proof is
    evidence, not a bug to fix. Without that, this proof became a chain of patches.
  - Test-setup lesson: Harmony compiles patches only in the game; a patch that failed there
    made a save load fail as "corrupt". The launcher now stops a copy whose plugins report a
    failed patch before anything loads.

- **Puppets are bare placeholders** (2026-10-19, presence step 1, by design for now): the other
  player shows as the default male hero body, bald, in underwear, in the rig's T-pose, sliding
  along. Female characters also show as male. Next steps: animation, then appearance and
  equipment. Puppets have no collision and enemies cannot see or hit them.

### Expected problems (derived, not seen yet)

- **Guest crash or quit without saving loses progress**: a guest's character reaches the host's
  co-op save only when the guest saves. Planned with live character sync: regular and
  event-driven pushes, a handshake on clean quit, and a host autosave when a guest leaves.
  A host crash loses pushed characters the host has not saved yet.

- **Enemy targeting with two players**: enemy AI knows only `Hero.Current`; on the host the guest is a puppet. The puppet must become a valid target (attackable, aggro, line of sight) or enemies will ignore the guest.
- **Damage and difficulty**: difficulty multiplies damage dealt and received, stamina and mana use (`Difficulty.DamageDealt/DamageReceived/...`, applied to NPCs via `NpcDifficultyStatModifier`). Hits the guest lands are resolved on the host with the host's difficulty; damage to the guest must use the guest's armour and stats. Decide whether enemies scale with player count.
- **Survival and Challenge difficulty**: save restrictions and a locked hero element; block in co-op at first.
- **Self-reported player IDs**: characters in the co-op save are keyed by the Steam ID the guest
  sends in its hello, and the host trusts it; a guest could claim another player's character.
  With a Steam transport, use the sender's Steam ID from the Steam network instead.
- **A newcomer always plays a copy of the host's hero**, gear included. Placeholder: a
  first-time guest should create its own character (design open: joining a story that is far
  along, levels, balance). Bringing one's own character from a solo save is a later option
  at most (balance, abuse). The test bot can (`join --bring`).
- **Quest items from stale character files**: a guest file older than the host world may carry items for finished quests.
- **Duplicate pickups**: both players looting the same container or drop; host must arbitrate.
- **Time, weather, sleeping, fast travel, wyrdness nights**: world-wide effects one player triggers; host authority needed.
- **Menus pause the game** in single player; in co-op they must not pause the shared world.
- **Death and loading screens**: one player loading a new area or dying must not freeze the other.

## Co-op milestones

| Date | Milestone | Test | Result |
|---|---|---|---|
| 2026-10-19 | Scorer self-test (playbook automated-qa.md, "Test the scorer, not only the test"): known good and bad session outputs, made by the test library's own functions, go through the real scorer first in every suite run | 9 cases; against the old scorer it failed exactly the 2 predicted holes (a run, or a twin run, with checks missing counted as a pass) | Fixed; Full: SUITE: PASS in 614 s, every run ran all its checks (main 40/40, bad network 31/31, twin runs failed on exactly their 17 and 1 listed checks). |
| 2026-10-19 | Testing gaps from the playbook filled: bad network (relay, 100 ms each way, 20 ms jitter, 5% loss) and exit (guest quits through the game's exit, host sees it leave, host quits, no crash report) | `run-suite.ps1 -Full`: new bad-network run; exit ends every run; twins "guest crashes" and "relay clean" | Over a real 108 ms / 5.1% loss link all in-session checks pass; host noticed the guest's exit by timeout. SUITE: PASS in 457 s (3 runs; twin run failed on exactly its 12 listed checks). |
| 2026-10-19 | Test suite restructured: one shared session runs every feature's checks; only save/reload and persistence restart games; twins with independent switches share one run; tiers `-Quick` / `-Full`; save folders reset to a pinned baseline each run | `tools/run-suite.ps1 -Quick` and `-Full`, same checks and twin expectations as before (duplicates merged) | Quick: SUITE: PASS in 95 s (22 checks). Full: SUITE: PASS in 327 s (main 34/34; twin run failed on exactly its 10 listed checks), was 659 s. |
| 2026-10-19 | Seeing each other, step 1 (playbook stage 2): roster slots, 20 Hz player state (unreliable), host forwards; each remote player is the game's own third-person hero body with the hero-only components removed, snapshot-interpolated 100 ms behind; both directions | `tools/presence-test.ps1` + twin (guest sends nothing: fails on exactly the 4 host-side puppet checks), in `run-suite.ps1` | SUITE: PASS in 659 s (6 cases); guest walked 16.6 m, host's puppet ended 0.08 m away. Puppet is a T-posed default body for now. |
| 2026-10-19 | Co-op save slots: hosting turns the world into its own co-op slot; on join the host saves it and sends the live world; guests' characters are stored in the host's save (as extra entries the game's domain cache carries through every save) and come back on the next join | `tools/run-suite.ps1`: join, persistence (join, change, save, both quit, rejoin), each with a mutation twin | SUITE: PASS in 470 s (4 cases); guest kept 777 gold across sessions, host 129. |
| 2026-10-18 | Combined load: guest character merged into the host save inside the game's own Gameplay-domain load (host hero dropped at definitions, guest models with fresh item IDs inserted in its place, one RestoreWorld); co-op save made on the guest reloads cleanly without the earlier save-order patch | `tools/run-suite.ps1`: join + reload (15 checks, incl. real end of loading screen) and twin | SUITE: PASS in 270 s; guest HUD tracks the host world's quest. |
| 2026-10-18 | Guest joins host over local UDP (LiteNetLib), receives the host's save (chunked, SHA-256 checked), loads it with its own character next to the host, both stable for 30 s | `tools/run-suite.ps1`: join test + mutation twin (join without character must fail on exactly the 3 character checks), two isolated installs on one PC | SUITE: PASS in 191 s, 10/10 checks per case. |

## Test setup lessons

- **LiteNetLib's network simulation does nothing in the NuGet build.** `SimulateLatency` /
  `SimulatePacketLoss` only work in DEBUG or `SIMULATE_NETWORK` builds; in the release 2.1.4
  DLL the methods are empty (`[Conditional]`, decompiled `Base.cs`). Use a UDP relay between
  the copies instead (ours: 127.0.0.1 relay with latency, jitter and loss, counting what it
  really did) and check its counts, not the settings.
- **The game's exit kills its own process.** `MenuUI.ExitGame` → `TitleScreenUI.Exit` saves
  prefs, ends analytics and calls `WindowsKernelHelpers.KillCurrentProcess()`; no Unity quit
  or destroy callback runs, so a mod can't rely on them. In tests the game closes about 3 s
  after the command.
- **Crash reports**: a Unity crash writes
  `%LOCALAPPDATA%\Temp\Questline\Fall of Avalon\Crashes\Crash_<UTC time>\crash.dmp` (shared by
  the real game and every test copy), with no dialog and no process left. Tests check for new
  folders since the run started.

- **Game launches dominate test time** (about 45 s per session start; checks take seconds).
  Run many features' checks in one shared session and restart games only for tests that need
  a quit (save and reload, rejoin). Twins whose switches cannot affect each other's checks can
  share one run; the run must fail on exactly the union of their checks.
- **Address save slots by file name in tests, never by slot ID**: the game reassigns IDs at
  startup when two slots claim one, so an ID can name a different slot after a restart. And
  reset test save folders to a pinned baseline at the start of each run, or slots from earlier
  runs pile up and cause those collisions.
- Windows PowerShell 5.1 variable names ignore case: a `[string]$Restarts` parameter and a
  `$restarts` list are one variable, and the type constraint turns the list into a string.

- The test copy shares `%USERPROFILE%\AppData\LocalLow\Questline\Fall of Avalon` with the real
  install, and the game repacks saves on first launch of this build. Redirect the data folder
  (`CloudService` backing field) and force `SteamNoCloudService` before launching a test copy.
- The game also keeps save scratch data in Unity's `Application.temporaryCachePath`
  (`%LOCALAPPDATA%\Temp\Questline\Fall of Avalon`): `CachedDomains\` (unpacked slot while
  loading) and `SaveFiles.zip` (slot being written). It is shared by the real game and every test
  copy; two copies loading at once corrupted a save. Redirect `IOUtil.TemporaryCachePath`,
  `ZipArchiveUtils.ZipPath` and the `LoadSaveDomainsCache.CachedDomainsDirectoryPath` backing
  field per instance (all static, computed once).
- Two copies run from one install (no single-instance lock in the code), each with its own data
  folder chosen by an environment variable read by the isolation plugin.
- **Isolation must fail closed.** Two copies started at the same moment from one install hit a
  sharing violation on `BepInEx\config\BepInEx.cfg`; the BepInEx preloader threw (written to
  `preloader_<timestamp>.log` in the game folder), Doorstop carried on, and that copy ran
  **unmodded**: `SteamCloudService` and the real save folder, showing a Steam Cloud conflict
  dialog. Real saves were not changed (checked timestamps), but nothing stopped it. Fixes:
  one install per instance (hardlinked game files, own BepInEx folder, plugins folder as a
  junction to the first install's), copies started one after another, and a launch interlock
  that kills a copy unless its isolation plugin reports the expected data folder within 6 s
  (the game initialises its cloud service about 8 s after start). The interlock has its own
  negative test.
- A launch interlock needs failure signals, not a guessed time: the first start after a plugin
  rebuild took longer than 6 s and was killed. Kill on concrete signs instead (BepInEx log not
  rewritten within 5 s, `preloader_*.log` crash file, game log line "Initializing Cloud with
  SteamCloudService"), allow up to 30 s for the confirmation, and wait until every process of the
  install (game and `UnityCrashHandler64`) has exited.
- Never store an install-specific path in a BepInEx config file: a copied config pointed the
  second install at the first install's saves. Derive paths from `Paths.GameRootPath`.
- LiteNetLib 2.1.4 `NetDataWriter.PutBytesWithLength` / `PutArray` store the length as a
  `ushort` and silently truncate arrays over 65,535 bytes. A 104 KB save arrived as a broken zip
  with no error anywhere. Write blobs with an `int` length yourself and send a SHA-256 next to
  anything the receiver writes to disk.
- "In gameplay" must mean the loading screen is gone (`LoadingScreenUI.IsLoading`); the hero model
  exists during loading, so checks and screenshots based on it pass too early.
- A focused test window reads keyboard and mouse: a host screenshot showed the inventory with a
  "Save outfit" dialog open that no bot opened. Test copies should ignore player input.
- Test copies are forced into a 1280x720 window, 30 FPS, `runInBackground`, by patching right
  after `ScreenResolution.Refresh` (where the game applies its display settings).
- Block `AutoBugReporting.SendAutoReport` in test copies: broken experimental loads otherwise send
  reports with saves attached to the developers.
- Automating the title screen: wait for `TitleScreenUI.SpawnTitleScreenUI`; loading earlier
  leaves the title menu drawn over gameplay for the rest of the session. "Press any button" can be
  skipped by setting `TitleScreenUI.s_shownFocusPopup`.
- Windows PowerShell 5.1 test scripts started from a PowerShell 7 environment (here: an agent
  shell) inherit its `PSModulePath`, load the wrong modules and miss built-in commands
  (`Get-FileHash`); every run aborted. Clear `PSModulePath` before starting 5.1.
