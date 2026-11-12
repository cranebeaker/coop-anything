# Queen's Domain — lab notes

Steam app 1799840, build 26356968, shipping exe SHA-256
`D3FD3846E99EEED520CD71BA71F50C45760DA73FA4192EFE07AB08B6C791A417`, Unreal Engine 4.27.2, Windows 11.
Released 2026-10-30. Mod code: separate repo (`queens-domain-coop`), not in this hub.

## 2026-11-04 — triage and first runtime probe

### Observations (checked)

- No co-op mod or attempt found: web search, coop-anything issues/PRs (`api.github.com/search/issues?q=repo:cranebeaker/coop-anything+queen`
  returned 0), DIRECTORY.md and `games/` (no entry). Steam page lists single player only.
- Exe: no `.bind` section (no SteamStub), ASLR on, PE timestamp equals the Steam manifest date. `steam_api64.dll` (SDK 1.63)
  is loaded dynamically by OnlineSubsystemSteam; it is not in the exe import table.
- Netcode compiled in (`UIpNetDriver`, `USteamNetDriver` strings). Not used: the game is single player.
- Pak `QueensDomain-WindowsNoEditor.pak`: version 11, **index not encrypted, no compression**, 44,049 entries. A 150-line
  Python reader lists and extracts it; cooked `.uasset` name/import/export tables parse with a small parser.
- The exe imports `XINPUT1_3.dll` **by ordinal** (2 = XInputGetState, 3 = XInputSetState). A proxy `xinput1_3.dll` with the
  same ordinals next to the exe loads before the engine; not a KnownDLL; no conflict with UE4SS's `dwmapi.dll`.
- `-NotInstalled` on the command line moves `Saved/` (config, logs, crashes, saves) into the test copy's own
  `QueensDomain/Saved`. Verified: owner's `%LOCALAPPDATA%\QueensDomain\Saved` hashed identical before/after runs.
- Shipping build writes no log file (logging compiled out); the mod writes its own logs.
- UE 4.27.2 layout facts measured at runtime (self-test in the mod, all PASS):
  - `UClass::ClassDefaultObject` is at **0x118** (not 0x110): `ClassGeneratedBy` is still present in 4.27 shipping.
  - ProcessEvent vtable index **0x44**; `KismetMathLibrary.Add_IntInt(2,3)` called through it returned 5.
  - `AActor::RootComponent` at 0x130. FNamePool found at exe+0x4a27f00, GUObjectArray chunk array at exe+0x4a64260
    (this build; the mod finds both by pattern, not by address).
  - Name pool blocks are 0x20000 bytes; the tail of a full block holds garbage after the terminator — a walker must
    bound every entry by the block end (first probe crashed there).
- Title screen: `TitleScreenUI_C` has `SkipIntroSplashes`, `ConfirmLanguage`, `SaveSlotSelected(Index,Backup,BackupSlot)`,
  `NewGameOnNewSlotConfirmed`. Calling those four from the bridge starts a new game in slot 0 without input.
- World: one persistent map `/Game/RuinsLevel/Maps/Ruins_Level_00` with ~45 streamed sublevels (`RuinsLevel/Maps/*.umap`),
  managed by native `FS_LevelManager`. ~515k objects in the starting area.
- Gameplay pawn: `FS_Full_PlayerChar_C` (child of `FS_PlayerChar_C` : native `FS_PlayerCharacter` : Character). Its
  `CharacterMesh0` has **no SkeletalMesh**; only the first-person `Arms` mesh (`Player_FPArms_Arms`, `Player_FPArms_Arms_ABP_C`)
  is set. No third-person player model ships.
- Player stats are BP properties: `Health`, `BaseHealth`, `MaxHPPerLevel`, `Level`, `UltraCharge`, `NewXP`, `MaxPotionCount`.
  No stamina value; attacks use a power meter (`PowerMeterLogic`).
- Player BP functions of interest: `TryApplyDamage`, `Set Health Points`, `AddAmountTOHealth`, `HealToFull`, `OnHealthChanged`,
  `Kill(B33smentDeath)`, `DeathAnim`, `Respawn`, `RestoreToSavePoint`, `UseHealingItem(Item, Potion)`, `ScriptUseItem`,
  `AddXP(XP, FromSteal)`, `InventoryChanged(Item, Added, NewItem)`, `AutoEquipItem`, `EquipItem`, `SetSaveCheckpoint(Checkpoint)`,
  `EnemyWasDamaged(...)`, `Teleport to Loc`, `SuperFastTeleport`, `TeleportToTeleporterData`.
- Enemies: `EnemyBase_C` : native `FS_EnemyCharacterBase` (has `bDead`, `bIsAIDisabled`, `bAggrod`, `CurrentStateIndex`,
  `OnEnemyDied`, `GetTargetPlayer()`); BP fields `TargetPlayer : FS_PlayerChar_C*`, `HP`, `OwningSpawner : EnemySpawner_C*`.
  Enemies are spawned by `EnemySpawner_C`, which is a `SavableActor_C`.
- Save model (native `FS_SaveGame`, BP `FS_SaveSlot_C`): world state lives in keyed maps —
  `SavableActorStates : TMap<FGuid,int>` (doors, elevators, barricades, breakables, secret walls, quest-state actors,
  enemy spawners), `NamedSaveData : TMap<FName,int>`, `ItemDrops : TMap<FGuid,ItemDropSave>`, `EnemySaveStates : TMap<FGuid,FS_EnemySaveState>`,
  `UnlockedTeleporters`, `SavePointsVisited`, `LastTouchedCheckpoint`. Per-player state: `Inventory`/`InventoryContents`,
  `EquippedItemSlots`, `EquippedItems`, `XPAmount`, `SavedPlayerLevel`, `WeaponXP`, `WeaponLevels`, `WeaponUpgrades`, health.
- `FS_GameInstanceBase`: `BaseActiveSave`, `OnWorldStateChange`, `OnInventoryChanged`, `NotifyGotItem(ItemToShow, ItemAcquired, Count)`,
  `LoadSaveSynch(Index, bBackup)`, `LoadMapWithLoadingScreen(Map, MapPackage)`.
- Plugin `SteamBridge` (Blueprint Steamworks wrapper) is compiled in, with lobby/friends/overlay enums.

### Derived (reasoned, not yet tested)

- Remote players must be real `FS_PlayerChar_C` instances for enemies to target them (`TargetPlayer` is typed). Plan: spawn
  `FS_Full_PlayerChar_C` puppets, hide `Arms`, give `CharacterMesh0` a full-body humanoid mesh.
- World progression sync = host-authoritative replication of `SavableActorStates`, `NamedSaveData`, `ItemDrops`,
  `EnemySaveStates`, `UnlockedTeleporters`, plus a refresh call on the affected actor (`SavableActor_C.RestoreFromSave`).
- Shared loot = intercept `InventoryChanged(..., Added=true, ...)` / `NotifyGotItem` on the collector, replay the item add on
  every other player through the same inventory API.

### Unknowns

- Whether a second `FS_Full_PlayerChar_C` survives BeginPlay without side effects (HUD, input, camera).
- Which humanoid AnimBP can drive a body owned by a non-enemy pawn.
- How the game grants items (which function writes `InventoryContents`), and how bosses gate their arenas.

### 2026-11-04 later — puppet spawn and window mode

- Observed: a second `FS_Full_PlayerChar_C` spawned with `GameplayStatics.BeginDeferredActorSpawnFromClass` + `FinishSpawningActor`
  (bridge `ue.call`) survives BeginPlay: no crash, the PlayerController keeps its pawn, the new one has no controller, Health 100.
  HUD side effects not yet measured precisely.
- Observed: `-windowed -ResX=960 -ResY=540` is ignored; the game starts borderless-fullscreen (WS_POPUP, whole screen).
  `exec r.SetRes 960x540w` through the bridge fixes it at runtime. Launcher must seed windowed settings before start.
- Observed: hot reload of the core works (loader copies QDCoop.dll and swaps it between frames); after a reload the
  ProcessEvent slot points at the loader hook, so index detection must scan the loader's saved original (fixed).

### 2026-11-04 afternoon — network layer, two-player puppets (automated, real game)

Observed:
- Own network layer (reliable/unreliable messages over UDP, sessions for 4 players) passes 11 game-free tests under
  25% loss, duplication, 150 ms reordering, a 3 s blackout, host restart and rejoin; 5 mutation twins are all caught.
  Fuzzing found two real bugs before any game ran: (1) a peer's packets claimed "I received your packet 0" before it had
  received anything, so a lost first Welcome was never resent and a joiner hung; fixed with an ack-valid header flag;
  (2) the world-state client skipped writing a host value equal to the last one it was told, although its own game had
  changed meanwhile; values are now always handed to the game adapter.
- `SkipIntroCutscene()` on the player BP ends the new-game beach intro (`InIntroCutscene`); the bot calls it until the
  player is `MOVE_Walking` (the intro can start a few seconds after the world appears).
- Test copies crashed with `SwapChain->ResizeBuffers(...) failed ... DXGI_ERROR_INVALID_CALL (960x540 -> 2560x1600)`:
  the game re-applies its saved resolution after the mod forced a small window. Seeding the copy's own
  `Saved/Config/WindowsNoEditor/GameUserSettings.ini` (windowed, 960x540, scalability 0) stopped it. The full message is
  in the minidump's stack memory, not in CrashContext.runtime-xml.
- Spawning a puppet within a second of the world load (and blocking-loading its body mesh then) faulted once; puppets
  now spawn only after the local player has been in the world 4 s and `FS_GameInstance.bLoadingMap` is false.
- Puppet = spawned `FS_Full_PlayerChar_C` with its BP `ReceiveBeginPlay`/`ReceiveTick` (and its components') blocked
  in the ProcessEvent hook, `Arms` hidden, `JosamiKnight` skeletal mesh + `JosamiKnight_Mace_ABP_C` (driven by setting
  the ABP's `bIsMoving`/`bIsFalling` flags) in `CharacterMesh0`. 4.27's UFunction is `SetAnimClass`, not `SetAnimInstanceClass`.
- Live test `tests/live/test_two_players.py`: RESULT PASS — B walked 1123 units, A's puppet of B ended 0.0 units from B,
  max lag during motion 26 units (loopback); reverse direction 0.0 units. Owner saves hash-identical.

### 2026-11-04/06 — shared world, combat, team rules (automated, real game, two copies over loopback)

Observed:
- The game "pauses" for menus and item inspection by setting global time dilation to 0.001 (not `SetGamePaused`).
  In a session that froze the host's whole world; the mod restores 1.0 after 0.3 s (short hit-stops are untouched).
- Unreliable messages bigger than one packet were dropped silently by the link (by design, but invisible): the host
  sent enemy state in batches of 20 x 66 bytes and only the last partial batch ever arrived. Now: oversize drops are
  counted (`oversizeDropped`), the game layer logs them, enemy state uses 64-bit id hashes and byte-budgeted batches,
  and a net test pins the size limit.
- `FS_PlayerChar_C.TryApplyDamage` with no `Applier` does nothing (health unchanged); with any actor as applier it
  applies. Real enemy hits do not pass through ProcessEvent (BP-internal call), but `OnHealthChanged(Old, New)` does,
  for every change including the lethal one, before the game acts on it. Writing `Health = 1` right after the original
  `OnHealthChanged` returns keeps the character alive: no death, no death screen (checked with a real lethal enemy hit).
- Death flow: `IsDead` -> about 6 s later `DieUI_C` opens and time dilation goes to 0.001; its Continue button event
  (`BndEvt__DieUI_ContinueButton_..._CommonButtonBaseClicked__DelegateSignature(Button)`) reloads the level and spawns a
  new pawn at the last checkpoint (no checkpoint yet: the beach, with `InIntroCutscene` again).
- The character BP sets `CharacterMovement.MaxWalkSpeed` in its tick; a crawl speed must be re-applied after the
  BP `ReceiveTick` (ProcessEvent) or it is lost before movement runs.
- `UseHealingItem(Item, Potion)` heals and fires `InventoryChanged` and `OnHealthChanged` in the same frame.
  `IsUseItemDisabled()` is true on a fresh save (the input path does not use items there).
- Live bindings: `PlayerController.PlayerInput.ActionMappings` has `Interact` on `E` and `Gamepad_FaceButton_Left`;
  `IsInputKeyDown(FKey{KeyName})` reads them.
- Enemies: `Enemy_GetBossInfo(Is A Boss, Group, Is Part Of Boss Encounter)`; max HP is a BP function (`GetMaxHP`), no
  property, so party scaling divides damage instead of raising health. Drops: `FS_ItemDropSystem.RollItemDrop` (native,
  called directly by the BP VM, not through ProcessEvent) rolls per machine; bags are saved in `FS_SaveSlot.ItemDrops`
  (GUID -> position, pickup class, contents). Save also holds `XPAmount`, `WeaponXP`/`WeaponLevels`,
  `SavePointID`/`LastTouchedCheckpoint`/`SavePointsVisited`, `KilledEnemyCounts`.
- Live tests, all RESULT PASS: `test_loot_world.py`, `test_combat.py` (host-driven enemy mirrored at 0.0 units with AI
  off, a real client swing claimed and applied, enemy targets the client, kill reaches both),
  `test_team.py` (down instead of dead, crawl 80, revive interrupted by walking away, 3.0 s hold revives at 35%,
  herb heal shared 60% at 2 m / nothing at 20 m / never revives, wipe -> both continue and respawn, session intact),
  `test_boss_scaling.py` (gate holds a forced-boss enemy with "Waiting for teammates: 1/2", no damage while waiting,
  warp to group opens it; damage scale 1.65 boss / 1.45 normal for host hits and client claims; client-first kill
  reaches the host).

Not yet verified: a real boss (none reachable near the start), enemy drop dedupe, more than two players, latency.

### 2026-11-06 — saves, HUD, menu, Steam, latency, party, and a level-load crash

Observed:
- World saves are `Saved/SaveGames/World<N>.sav` (+ `_backup`); the title screen shows slots 0-4. Co-op uses slot 8
  (host) and 9 (client). New game in a slot: title UI `SaveSlotSelected(N)` + `NewGameOnNewSlotConfirmed()`; continue:
  title UI `Load Slot(N, false, 0)` (SaveSlotSelected alone only selects). A death reload reads the save from disk, so
  a checkpoint set in memory (`FS_PlayerChar.SetSaveCheckpoint`) must be followed by `GI_SaveGameToDisk()`.
- `FS_GameInstance.GetActiveSaveSlotIndex()` raised a software exception (code 0x4000 via RaiseException) on the
  title screen; the mod's fault guard used to swallow every exception code and then disabled co-op. The guard now
  only contains hardware faults and C++ exceptions and passes everything else on to the game.
- HUD: drawn with AHUD.DrawText/DrawRect/GetTextSize/Project after `ReceiveDrawHUD`. The default font has no hyphen.
  Live bindings for Interact come from `InputSettings` (PlayerInput.ActionMappings is only the player's overrides).
- Item display names: `FS_ItemData_C.Name` (FText); HealingHerb is "Dried Herbs".
- Steamworks SDK 1.63 flat API is exported by the game's steam_api64.dll (SteamFriends_v018, SteamMatchmaking_v009,
  SteamUser_v023, SteamUtils_v010, SteamNetworkingMessages_SteamAPI_v002). Not exercised live: one Steam account.
- Level-load crash: about 2-3 s after `Ruins_Level_00` loads, engine code (exe+0xe99e4b / +0xe1fe4d / +0xe1fe88,
  callers +0x2cc9e11, +0x29af743) dereferences a destroyed object (it validates the object by its GUObjectArray index,
  then walks a TArray at +0x160). With all 1971 vtables hooked: 9 crashes in 90 loads; with the ProcessEvent hooks off:
  1 in 61 - so it happens without the hook. A vanilla rate cannot be measured automatically (the harness needs the
  mod). The mod now hooks only the 10 native vtables it needs.
- Chaos relay (hub tools/chaos) at 50/100/150 ms RTT with 2% loss, 2% reorder, 0.5% duplicates: movement, loot,
  enemy mirroring, damage claims and revive all pass; a 3 s total outage is survived.
- Live tests added (all RESULT PASS unless noted): test_progress, test_drops, test_hud, test_menu, test_chaos,
  test_party (3 players), test_anim (player actions pass; enemy montage playback on the client not verified).
- Final soak numbers for the level-load crash: all 1971 vtables hooked 9/90, hooks off 9/80 (0/20 alone, 9/60 while
  another soak ran), release build hooking 10 vtables 3/40 (busy PC). Not the hook. A control loading
  `Ruins_Level_00` from the command line with a passive mod does not work: the game ignores the map argument.
- The 4-player test's nearest enemy at a far checkpoint was `Boner_Shrineboss_Demo_BP_C`, flagged by the game's own
  `Enemy_GetBossInfo`: boss scaling (2.95 with four) and the gate ("1/4") ran on a real boss; no fight was played.
- Installer: tested on a separate folder (copy of the exe + a stand-in xinput1_3.dll); an update used to drop the
  record of the backed-up foreign xinput1_3.dll (fixed, retested).
- Level-load crash, resolved in the harness: the bots pressed the title screen's buttons the moment it appeared.
  With a 3 s wait after the title UI exists (`TitleUiReady`), 0 crashes in 100 loads (40 alone, 60 with two soaks at
  once on C and D), against about 1 in 9 before. The mod's own host/join from the title waits the same way.
- Performance (test_perf, A host + B client in the dungeon, both moving): host 114 us average and 805 us worst per
  frame; client 261 us and 1076 us (final build). Fixed on the way: the 2 s enemy scan (`FindInstances` over ~500k objects, ~10 ms
  in one frame) is now a sweep of 15000 objects per frame; a per-object linear class cache in that sweep made it
  worse (20 ms) and was dropped; the reflection layer's name lookup re-parsed the engine's current FName block (up
  to 128 KB) whenever its cursor had moved: now only on a miss and only the new entries. The engine bumps
  `CurrentByteCursor` under its lock but writes the entry after, so the previous increment is read once more. The
  client skips moving enemies that stand still on the host (84% of moves skipped with 65 remote enemies).
- Fault guard (test_fault_guard): a deliberate access violation inside the mod's frame is caught, logged with the
  stack, co-op turns itself off, and the game keeps running and rendering in the level; no crash report.
- test_anim enemy part now passes: a montage the host's enemy plays (`Boner_Stagger_Montage`) plays on the
  client's copy.
- test_chaos failures (2026-11-07) were in the test: blind teleports at fixed offsets from the enemy sometimes ended
  outside the level; the player fell out of it (no floor anywhere around) and went down, and the team wiped. With
  placement fixed, a second cause: the aggro'd enemy hit the reviver for 21 every ~2.9 s, and damage interrupts the
  3.0 s revive by design. The test now verifies placement and holds the enemy's AI (`bIsAIDisabled`) for the step.
- The game has no fall damage (a 2500-unit fall cost 0 health). `K2_SetActorLocation` with bTeleport keeps the
  character's velocity; the mod's warps now call `CharacterMovement.StopMovementImmediately()` after it.
