# Queen's Domain (2026)

| | |
|---|---|
| Engine | Unreal Engine 4.27.2 (Win64 Shipping, Blueprint-heavy, pak not encrypted) |
| Versions tested | Steam build 26663759 (app 1799840), Windows 10 and 11 |
| Projects | [Knights' Domain](https://github.com/Lotisphere/KnightsDomain) by Mangolotis (directory entry: Queen's Domain / Knights' Domain); test rig: [queens-domain-coop-lab](https://github.com/cranebeaker/queens-domain-coop-lab) |
| QA level | automated (Knights' Domain 1.0.3 and 1.0.6, tested by Co-op Anything, not by the mod's author) |
| Last updated | 2026-11-18 |

## Where things stand

Up to four players can fight through Queen's Domain together today with Knights' Domain (1.0.6, Steam lobbies and
invites). Players see each other, enemies and bosses are shared and combat sync is solid. The world is not shared:
every player opens their own chests and doors, picks up their own items and rests at their own checkpoints, so each
player progresses in their own save. Enemy arrows and boss fire attacks only exist for the player they are aimed at.

## Existing work

- **Knights' Domain** (Mangolotis, [GitHub](https://github.com/Lotisphere/KnightsDomain),
  [Nexus](https://www.nexusmods.com/queensdomain/mods/46), [site](https://www.mangolotis.com/knightsdomain/)).
  First release 2026-11-09, 1.0.6 on 2026-11-17, with an in-game updater. Releases are binaries; the source is public on the `source` branch
  (1.0.6 since 2026-11-18) under an all-rights-reserved licence: you may read it and build it for your own use, and
  need the author's written permission to modify, redistribute or reuse it. So fixes go to the author as bug reports.
- A second co-op mod, the Queen's Domain co-op mod (own repo, not published yet), is written up in its own
  section below: [Queen's Domain co-op mod](#queens-domain-co-op-mod).

## How the game works (for co-op)

- Single-player UE 4.27 game, almost all logic in Blueprints. A Dumper-7 SDK dump gives every class.
  The exe imports `XINPUT1_3.dll`, so an `xinput1_3.dll` proxy next to it loads a mod (Knights' Domain does this).
- One persistent map (`Ruins_Level_00`) with streamed sublevels. Actor names repeat across sublevels, and actors of
  streamed-out sublevels sit at the origin.
- World state is one map in the save: `FS_SaveSlot_C.SavableActorStates` (GUID to int). Every chest, door, lever,
  lift, secret wall, breakable, barricade and pickup is a `SavableActor_C` with its GUIDs and a
  `Restore State From Save Data` event. Story counters are in `NamedSaveData` (name to int). Checkpoints write
  `LastTouchedCheckpoint` and `SavePointsVisited`. That makes world sync a matter of replicating two maps and
  calling the restore event.
- `-NotInstalled` on the command line moves `Saved/` into the game folder, so test copies never touch the real saves.
- A new game in a used save slot keeps parts of the old world; tests must start from an empty `SaveGames`.

How Knights' Domain works: the player an enemy is fighting runs its real AI and sends snapshots; everyone else
follows with the AI off. Since 1.0.6 bosses and dormant enemies are run by the host and stay locked to one machine
during a fight. Remote players are drawn as knight models, not player characters. Transport is
`ISteamNetworkingMessages` through the game's own `steam_api64.dll`; a dev build adds UDP loopback.

## What we changed

Nothing in the mod (its licence does not allow it). We built a test rig around it:

- `qdbridge.dll`, built on this hub's `tools/bridge`: injected into each game copy, it starts a new game, moves the
  player, uses any world object, and returns player, save, enemy, pickup and world-object state as JSON.
- A runner that boots two copies (host and client over the mod's dev UDP transport) on one PC and runs 19 scenarios,
  each from a fresh pair with empty saves. Lag runs go through this hub's `tools/chaos` relay.

Findings were written up as draft reports for the mod's author (in the rig's `reports/` folder).

## QA

`py -3 -I runner/scenarios.py --suite all` (about 35 minutes) and `py -3 -I runner/scenarios.py lag` in the rig repo.
A scenario passes when the co-op behaviour is right. Knights' Domain 1.0.6 dev build, 2026-11-18, two full runs with
identical results (5 pass, 13 fail, 0 skip), two lag passes and six extra death runs:

```
movement                  PASS   teammate model 0 cm from the player
client_progress_persists  PASS   own items and checkpoint survive save and reload
enemy_hp_agree            PASS   23 alternating hits, HP gap 0.0, none lost (1.0.3 lost some)
death                     PASS   respawn in about 10 s, both see each other again (7 of 8 runs, see known issues)
host_leaves               PASS   client notices after 10.9 s and keeps playing
loot_and_world            FAIL   host pickup stays on the ground for the client
client_loot               FAIL   client pickup stays for the host
host_world_objects        FAIL   chest, secret wall, door, keyed door, lift, barrel, barricade: changed for the host only
client_world_objects      FAIL   six objects of the same kinds: changed for the client only
host_gate                 FAIL   lever opens the gate for the host; the client is still blocked (collision sweep)
client_gate               FAIL   the same the other way round
checkpoint                FAIL   only the resting player's save records the checkpoint
client_checkpoint         FAIL   the same the other way round
story_flags               FAIL   8 story counters set for the host, missing for the client
rejoin                    FAIL   a rejoining client gets a fresh world
ranged_attacks            FAIL   host sees 12 to 13 arrows and takes 12 hits; the client in the line of fire sees 0
area_attacks              FAIL   dragon boss: 32 to 93 fire actors on the host, 0 on the client
drops_and_xp              FAIL   XP and kill counts equal (432 / 432); each machine rolls its own loot
lag (50 to 150 ms, 2 % loss)     nothing worse than on the plain link, two passes
```

Not tested: Steam sessions between two accounts (the rig uses the dev transport), more than two players, levels
after the Ruins, a full playthrough.

## How to play

Install Knights' Domain from its [releases](https://github.com/Lotisphere/KnightsDomain/releases/latest): extract the
zip into the game folder, start the game from Steam, press F5, create a room and invite friends through Steam.
Back up `%LOCALAPPDATA%\QueensDomain\Saved\SaveGames` first.

## Known issues

- Every player has their own world: a door, chest or gate one player opens stays shut for the others, and each
  player has to pick up items and rest at checkpoints themselves. A friend can be blocked by a gate you walked
  through.
- A player who rejoins sees their own world, not the host's.
- Enemy arrows and boss fire puddles, fireballs and flame breath only show and hurt for the targeted player.
- Each player gets different loot from the same kill.
- After the host dies and continues, players can become invisible to each other for the rest of the session
  (1 of 8 runs on 1.0.6, 2 of 6 on 1.0.3). Leaving and hosting again fixes it.

## Next steps

- Send the draft reports to the author (world state, projectiles, loot, the crash after death).
- World sync is the big missing piece. It needs the author's permission or a mod of its own; the save maps above are
  the place to start.
- Extend the rig to later levels and to three and four players.

## Queen's Domain co-op mod

A separate 1-4 player online co-op mod, built from zero (own repo, not published yet, so no directory entry yet). Its
raw lab notes are in [NOTES.md](NOTES.md).

| | |
|---|---|
| Engine | Unreal Engine 4.27.2 (Win64 Shipping, no SteamStub, logging compiled out; pak v11, unencrypted) |
| Versions tested | Steam build 26356968 (exe PE timestamp 6AE3DAC2), English, game files only (a third-party pak in the tester's install was left out of the test copies) |
| Projects | Queen's Domain co-op mod (own repo, not yet published; no directory entry yet) |
| QA level | automated |
| Last updated | 2026-11-07 |

### Where things stand

A 1-4 player online co-op mod exists and its main loop is tested with real game copies: players see each other,
share the host's enemies, loot (once each) and world, go down and revive, wipe together, share checkpoints and play in
separate co-op save slots. Direct/LAN play is tested, including at 150 ms round trip with loss; Steam lobbies and
invites are written but not tested between two accounts. No full campaign has been played in co-op yet.

### Existing work

None found before this project (no co-op or multiplayer mod for Queen's Domain on GitHub, Nexus, ModDB or the Steam
forums via web search, 2026-11-04).

### How the game works (for co-op)

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

### What we changed

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

### QA

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

### How to play

Not available yet: the mod's repository and package are not public. This section will link them once they are.

### Known issues

- A crash 2-3 s after a level load, inside engine code, in about 1 of 10 test loads while the bots started a game
  the instant the title screen appeared (the same with the mod's hooks off). With a 3 s wait first: 0 in 100 loads.
  Whether a person pressing that fast crashes an unmodded game was not measured. See `NOTES.md`.
- Steam play untested between two accounts; enemies only synced in the host's level; teammates use a knight model.

### Next steps

Test Steam play with two accounts; a co-op playthrough with a real boss; per-level authority for players in other
levels; teammates' weapons on their bodies; check with a bot whether an instant New Game press crashes an unmodded game.
