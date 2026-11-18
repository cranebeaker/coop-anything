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
- A second co-op mod is described in [PR 336](https://github.com/cranebeaker/coop-anything/pull/336) (own repo, not
  published when this was written). Its write-up will share this folder.

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
