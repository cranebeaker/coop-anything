# Queen's Domain (2026)

| | |
|---|---|
| Engine | Unreal Engine 4.27.2 (Win64 Shipping, no SteamStub, logging compiled out; pak v11, unencrypted) |
| Versions tested | Steam build 26356968 (exe PE timestamp 6AE3DAC2), English, game files only (a third-party pak in the tester's install was left out of the test copies) |
| Projects | Queen's Domain co-op mod (own repo, not yet published; no directory entry yet) |
| QA level | automated |
| Last updated | 2026-11-07 |

## Where things stand

A 1-4 player online co-op mod exists and its main loop is tested with real game copies: players see each other,
share the host's enemies, loot (once each) and world, go down and revive, wipe together, share checkpoints and play in
separate co-op save slots. Direct/LAN play is tested, including at 150 ms round trip with loss; Steam lobbies and
invites are written but not tested between two accounts. No full campaign has been played in co-op yet.

## Existing work

None found before this project (no co-op or multiplayer mod for Queen's Domain on GitHub, Nexus, ModDB or the Steam
forums via web search, 2026-11-04).

## How the game works (for co-op)

- Single-player UE4 game, no netcode used. Blueprint-heavy: almost all game logic is Blueprint, reachable through
  UE reflection (`GUObjectArray`, `FNamePool` found by scanning `.data`; offsets in `NOTES.md`).
- The mod's loader is an `xinput1_3.dll` proxy (the exe imports XINPUT1_3 by ordinal); it hooks `PeekMessageW` for a
  per-frame game-thread callback and the `ProcessEvent` vtable slot (index 0x44) of the 10 native classes the mod
  needs. Blueprint-to-Blueprint calls do not pass through ProcessEvent; events from native code (input, overlaps,
  ticks, delegates, `OnHealthChanged`, `ReceiveDrawHUD`, `GetTargetPlayer` from behaviour trees) do.
- Saves: `FS_SaveSlot_C` (`SavableActorStates` GUID->int for pickups/doors/spawners, `NamedSaveData`,
  `InventoryContents`, `ItemDrops`, `XPAmount`, `WeaponXP`, checkpoint fields), files `World<N>.sav`; the title screen
  shows slots 0-4. A death reload reads the save from disk.
- The game "pauses" by setting global time dilation to 0.001 (menus, item inspection, death screen).
- Death: `IsDead` -> death screen `DieUI_C` -> Continue reloads the level at the checkpoint.
- Enemies: `EnemyBase_C` with `HP`, `bDead`, `bIsAIDisabled`, `TargetPlayer`, `OwningSpawner` (spawner save GUIDs
  give the same id on every machine), `Enemy_GetBossInfo`. Drops are rolled per machine (`RollItemDrop`).

## What we changed

The mod (host-authoritative, own reliable/unreliable UDP link with session epochs, star topology):

- Remote players are spawned `FS_Full_PlayerChar_C` puppets (so enemy AI can target them) with their own Blueprint
  logic blocked and a knight body from the game's assets; interpolated snapshots.
- Shared world state (versioned deltas, snapshots) and shared loot with per-pickup arbitration; enemy drops keyed by
  their enemy so each player gets them once.
- Enemies simulated by the host, mirrored on clients with AI off; damage claims from clients; hits on puppets
  forwarded to their owner with the attacker as applier (the game ignores damage without one); multi-target AI.
- Downed instead of dead (health restored inside `OnHealthChanged` before the game acts), hold-to-revive, team wipe
  through the game's own death and continue, consumable heals shared by distance.
- Damage scaling by party size (max HP is a BP function, so damage is divided instead); a boss gate.
- Shared checkpoint (applied with `SetSaveCheckpoint` and saved to disk), co-op save slots 8/9, an atomic co-op
  campaign file.
- HUD on the game's own canvas, a co-op menu (F8 / gamepad View), join codes with a CRC check, Steam lobby code path.

Bugs found and fixed on the way, each with a test: early ack before any packet (lost Welcome never resent);
world-state client skipping equal values; unreliable messages larger than one packet silently dropped; the fault
guard swallowing the game's own software exceptions; checkpoint lost on death reload (not on disk); a 2 s object
scan stalling one frame for ~10 ms; name lookups re-parsing the engine's newest name block on every call; warps
keeping the speed a character had.

## QA

- `build_tests.cmd` then `py -3 -I tests/run_tests.py`: 28 game-free tests (net 14, sync 6, save 4, codes 4) and 9
  mutation twins, all caught. Last run 2026-11-07: ALL OK.
- `tests/live/*.py`: 15 live test scripts, 16 runs (the party one with 3 and with 4 players), with real game copies
  (see the mod's `docs/TESTING.md`): all RESULT PASS on the final build on 2026-11-07, after fixing three of the
  tests themselves (placement and timing; listed in `TESTING.md`). Also a load soak: 0 crashes in 100 level loads.

Suite summary, final build, 2026-11-07 (one Windows 11 PC, game copies A-D, loopback UDP):

```
tests/run_tests.py         code 4/4, net 14/14, save 4/4, sync 6/6 passed; mutation twins 9/9 caught; ALL OK
test_two_players           RESULT: PASS
test_loot_world            RESULT: PASS
test_combat                RESULT: PASS
test_team                  RESULT: PASS
test_drops                 RESULT: PASS
test_chaos                 RESULT: PASS   (50/100/150 ms RTT, 2% loss, 2% reorder, 0.5% dup, 3 s outage)
test_hud                   RESULT: FAIL   (4 checks: camera faced the floor) -> test fixed, reruns PASS, PASS
test_boss_scaling          RESULT: FAIL   (gate check raced B's move)       -> test fixed, reruns PASS, PASS
test_progress              RESULT: PASS
test_menu                  RESULT: PASS
test_anim                  RESULT: PASS
test_perf                  RESULT: PASS   (host 114 us, client 261 us per frame on average)
test_fault_guard           RESULT: PASS
test_warp                  RESULT: PASS
test_party (3 players)     RESULT: PASS
test_party (4 players)     RESULT: PASS
soak_world_load            40/40 alone, then 30/30 + 30/30 on two copies at once: 0 crashes (build before the
                           performance changes)
```

## How to play

Install with the package's `Install.cmd`, host from the title screen (F8), invite friends or share the LAN code.
See the mod's README.

## Known issues

- A crash 2-3 s after a level load, inside engine code, in about 1 of 10 test loads while the bots started a game
  the instant the title screen appeared (the same with the mod's hooks off). With a 3 s wait first: 0 in 100 loads.
  Whether a person pressing that fast crashes an unmodded game was not measured. See `NOTES.md`.
- Steam play untested between two accounts; enemies only synced in the host's level; teammates use a knight model.

## Next steps

Test Steam play with two accounts; a co-op playthrough with a real boss; per-level authority for players in other
levels; teammates' weapons on their bodies; check whether an instant New Game press crashes an unmodded game
(manual).
