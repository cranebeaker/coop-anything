# Resident Evil 2 (1998)

| | |
|---|---|
| Engine | Capcom proprietary (1998 PlayStation engine; 2006 Sourcenext PC port) |
| Versions tested | Steam re-release (AppID 4249110, build 21744144) running `rebirth\bio2.exe` from Mulder's Steam Enhancement Pack: Sourcenext PC v1.1.0 + Classic REbirth 1.0.9.1, bio2.exe SHA-256 `00dd7aab0ae5d5f6afef1cb4a9071030c2e8d4edd00e5a24302a70c800c3c232` |
| Projects | [re2coop](https://github.com/shoukyperry/re2coop) (MIT) |
| QA level | automated |
| Last updated | 2026-10-28 |

## Where things stand

re2coop v0.2 (prototype) lets two players play online over the Steam relay (Spacewar AppID 480)
or direct UDP. The other player is a real NPC model (Leon or Claire) that walks, runs and turns
with the game's own animations, and the client is taken to the host's room whenever the host
changes room. The host owns the world: the client mirrors the enemies of the host's room, its
shots are applied by the host, and the host's enemies attack it too. Kill flags, items (an item one
player takes is gone for both) and story flags (doors, story, maps, files) are shared.
Inventories stay per player. Cutscenes and bosses are not synchronised, and two real Steam
accounts have not played together yet.

## Existing work

- No co-op or multiplayer mod for the 1998 game was found (web search and this directory,
  2026-10-27). Co-op mods listed for "Resident Evil 2" are for the 2019 remake (RE Engine).
- Reverse-engineering sources that match this exe:
  - [BioRand](https://github.com/IntelOrca/biorand) `biorandmod/biorand.cpp`: hooks for this
    exact Sourcenext + Classic REbirth exe (load game, SCD tasks, NPC init).
  - [biohazard-utils](https://github.com/IntelOrca/biohazard-utils): SCD opcode layouts
    (`Script/Opcodes/SceEmSetOpcode.cs`, `OpcodeV2.cs`, `Bio2ConstantTable.cs`), and
    `src/hitscan/Re2Binder.cs` (key word 0x988604).
  - [MeganGrass/Resident-Evil-2-Starter-Kit](https://github.com/MeganGrass/Resident-Evil-2-Starter-Kit)
    `source/bio2/data.h`: the PS1 structs. The PC exe keeps these layouts (`tagGLOBAL`,
    `PLAYER_WORK`, `ENEMY_WORK`, `SCE_AOT`, `IN_DOOR_WORK`); offsets were checked live.
  - [yanmingsohu/re2re](https://github.com/yanmingsohu/re2re) is a disassembly of a different
    bio2.exe build (addresses don't match); not used.

## How the game works (for co-op)

No netcode. Everything hangs off one global `G` (tagGLOBAL) at 0x9864E0, laid out exactly as on
PS1, so the PS1 struct headers compiled for i386 give the PC offsets. Full memory map:
[NOTES.md](NOTES.md).

- **Player:** `G.Player` at 0x989EF0. `Routine_0` = 1 while the player is in control, and
  `Routine_1` gives the gait: 0 idle, 1 walk, 2 run, 3 back step.
- **Entities:** `G.pEnemy[34]` at 0x98A10C ([0] player, [1] partner, [2+id] em id). The per-frame
  loop starts at 0x4C03CA and calls `[G + type*4 + 0x3C74]` at 0x4C0449.
- **Scripts (SCD):** handler table at 0x53AE10, one handler per opcode, each taking an
  `SCE_TASK*` (Data at +0x1C, work entity at +0x154). Calling a handler with a fake task runs
  one opcode, e.g. `em_set` (0x44) to spawn an NPC or `plc_dest` (0x40) to make it walk.
  `em_set` is asynchronous: while the EMD file loads (`[0x68A204]+0x13`) it returns without
  consuming the opcode, and the caller must retry next frame.
- **Doors:** the door AOT handler (0x4E9460) ends, once key checks pass, with three writes:
  `[0x991F80] = 1`, `G.pSce_aot_on (0x988848) = &IN_DOOR_WORK`, `G.Stop_flg |= 0xFF000000`.
  The game-mode state machine then plays the door and loads the room or stage. A made-up
  unlocked `IN_DOOR_WORK` therefore warps the player anywhere.
- **Input:** the pad routine 0x4D0F30 gets the raw PS1-layout pad from `call 0x43BB00` (at
  0x4D0F53) and maps it through the key-config table at 0x5338D8 into 0x988604 (held) and 0x988608
  (pressed). Classic REbirth does not patch this path, so hooking that call gives a bot full control.
- **Single instance:** the exe and Classic REbirth refuse a second copy through the mutexes
  `bio2.658b45ea117473d4.game` and `re2cr.aiory0n5jnqf3r7a.game`. See the test rig below.

## What we changed

re2coop is an ASI plugin loaded by the pack's Ultimate ASI Loader (`dsound.dll`). It adds files
only and checks the bytes it patches, so on another exe it disables itself.

- **Peer avatar:** spawns NPC type 0x50 (Leon RPD) or 0x51 (Claire) in em slot 31 with the game's
  `em_set` handler. This runs from a trampoline at the head of the per-frame entity loop, the
  same place script opcodes run. Spawning from the pad hook crashed at 0x4C0669: the shadow pass
  read `pKage_work` before the NPC's init routine had run. The NPC routine 0x4EF110 also skips
  entities while `Stop_flg` bit 0x20000000 is set, which is the case outside the loop.
- **Avatar movement:**
  - The plugin mirrors the peer's gait with `plc_dest` (kind 4 walk, 5 run), keeps the
    destination updated every tick, and idles the NPC (Routine 4/6) facing the peer's direction.
  - Kind 8 (back step) does nothing on these NPCs, so back steps play as walking.
  - The NPC snaps to the peer above 1200 units, or when it is blocked for 30 ticks.
  - The arrival flag that `plc_dest` sets in the room flags (0x98EB6C) is saved and restored
    around the NPC's routine, so room scripts never see it.
- **Room changes:** pEnemy is rebuilt by the new room; the plugin forgets the puppet and clears
  its slot on the first frame of the new room, hides it (`Be_flg` bit 0) while the peer is
  elsewhere, and respawns it when both share a room.
- **Follow host:** when the host has been in another room for 1.5 s and the client is in control,
  the client goes through a made-up unlocked door to the host's entry spot, camera cut and floor.
  This crosses stages too (tested stage 0 room 0x0F to stage 5 room 0x12).
- **Kill flags:** the two 32-byte enemy kill-flag arrays that `em_set` checks (0x98EB74) go from host
  to client every second; the client takes them as they are. (v0.1 OR-merged them both ways, which
  mixed the kills of two different saves into the host's world.)
- **Enemies (host-authoritative):**
  - The host sends the enemies of its room 15 times a second (slot, type, routine, position, life,
    damage fields). The client forces its own copies into that state before each entity-loop tick,
    kills them when the host's die, and keeps its local player out of their AI while the host is in
    the room.
  - The client's hits on enemies (the `sub [enemy+0x156]` in each damage routine) are sent to the
    host and applied there once each.
  - Enemy attacks on the client's puppet are captured on the host at `Pl_damage` (0x4DCA40) and
    replayed on the client.
  - Works with enemies placed by the room's own script (5 types tested), not only debug spawns.
- **Shared progress (items, story flags):**
  - Shared SaveData ranges: +0x390..0x3D0 (Scenario_flg, Common_flg), +0x418..0x484 (Item_flg and
    the stage-4+ item array, Item_c_flg, map flags), +0x584..0x598 (File_flg, Zapping_flg, Key_flg,
    which the door code checks). Not Room_flg, the kill flags or Pri_be_flg.
  - The host sends its copy 4 times a second; the client takes it.
  - The game's flag functions Flg_on (0x503120) and Flg_off (0x503140) are replaced by the plugin's
    own. On the client they record each bit the game changes in a shared range, sent until the
    host's copy shows it. A memory diff would have lost a pickup made during the pickup message or a
    door (the entity loop stops there) and would have mistaken a save load for changes.
  - An item pickup sets its flag through Flg_on (0x4FDFFB) and then hides the item's model
    (object work 0x98A61C + MD1 * 0x1F8, `Be_flg` = 0). When the peer's packet sets an item flag
    for an item of the current room, the plugin does the same, so the item disappears at once.
  - Two writers bypass Flg_on: a puzzle state byte at 0x98EBD0 and Zapping_flg; for those the
    host's value wins.
- **Networking:**
  - Steam: a friends-only Spacewar lobby. The client finds the hosting friend by itself through
    the friend list, or uses an invite or a lobby id. Traffic is unreliable ISteamNetworkingMessages
    at 30 Hz.
  - UDP: direct IP for LAN or VPN.
  - The exe's code is checksummed in every packet, and a peer with another build is ignored.
  - The plugin forces `SteamAppId=480`, because a Steam-launched game inherits 4249110 and two
    players launched differently would not see each other.
  - `steam_api.dll` is loaded on the first frame, not in DllMain (loader lock).

## QA

Two real game instances on one PC, driven by bots through the plugin's shared memory block. The
bots write the raw pad from Python, and for door tests they teleport the player into the door zone
(position, previous position and `Sca_old`).

- **Test rig:**
  - Two copies of `rebirth\`.
  - In each copy's `ddraw.dll` (backed up), one byte of both Classic REbirth mutex names is changed.
  - `[test] multi_instance=1` in re2coop.ini makes the plugin rename the exe's own mutex in memory.
- **Steam:** two Steam accounts can't run on one PC, so a test `steam_api.dll` mock
  (~200 lines) implements the 30 flat exports the plugin uses over localhost UDP. It reproduces the
  real-Steam behaviour the playbook warns about: sender identity at offset 12, lobby results
  delivered twice, and messages held until AcceptSessionWithUser.
- **Real Steam:** checked on one account. Init, lobby creation and rich presence work; two-account
  play over real Steam has not run yet.
- **Suite (`qa_suite.py`, 33 checks):**
  - connection both ways
  - puppets visible, with the right types
  - puppet walks while the host walks, runs while the host runs, settles under 300 units, never
    beyond the snap distance
  - kill flags reach the other player
  - client leaves through a door: host hides it, client has no puppet, client is brought back,
    both puppets return
  - follow host through a door: client in control, puppets visible
  - zombies spawned on the host: mirrored on the client, killed on both
  - no crash in either log
- **Other tests in `qa/`:**
  - `enemytest`, `mirrortest --shoot`, `hittest` (11 checks each), `multitest` (17), `attacktest` (8),
    `roomtest` (a room's own zombies, 11): enemy snapshots, mirroring, client hits, several
    zombies, host enemies attacking the client.
  - `storytest` (16): the client takes the host's flags, bits either side sets or clears through the
    game's functions reach the other, and an item the host took is gone when the client re-enters.
  - `pickuptest [client|host]` (13): one player picks up the ink ribbons of stage 0 room 0x06 with
    the buttons (cross tap, then square); the other gets the flag, the item's model disappears in
    the same room, the other cannot pick it up, and no save file is written (the typewriter shares
    the spot).
- `[test] loss=N` drops N% of each copy's outgoing packets.

Last results, 2026-10-28 (both transports):

```
SUITE udp: PASS 33/33          SUITE steam-mock: PASS 33/33
STORYTEST: PASS 16/16          (udp, steam-mock)
PICKUPTEST: PASS 13/13         (client and host picking, udp and steam-mock)
```

Earlier the same night: enemytest, mirrortest and hittest 11/11, multitest 17/17, attacktest 8/8,
roomtest 11/11 on both transports. With 25% loss everything passed; with 30% `qa_suite` was 32/33
and with 50% 31/33: the puppet falls far behind the host under heavy loss.

The cross-stage follow was run once by hand: the client loaded a save in stage 0 room 0x0F,
was taken to stage 5 room 0x12 and was in control afterwards.

## How to play

Release v0.2 on [shoukyperry/re2coop](https://github.com/shoukyperry/re2coop/releases). The package has `installer-hote.bat`
and `installer-invite.bat`; they find the game in the Steam libraries, check the exe hash and add
three files to `rebirth\`. The host loads a save; the client loads a save (ideally the same one)
and joins the hosting Steam friend automatically, taking the host's progress. Both players need
the same version (v0.2 does not talk to v0.1).

## Known issues

- Cutscenes and bosses are not synchronised; the grab animation is not shown on the client.
- Inventories are per player.
- A puzzle state byte (0x98EBD0) and Zapping_flg are written without the game's flag functions: the
  client's changes to them are overwritten by the host's.
- Under heavy packet loss (30% and more) the puppet can fall far behind.
- On arrival through follow-host, both characters can stand on the same spot.
- Steam shows both players as playing "Spacewar".
- Not tested: real two-account Steam play, cutscene rooms, rooms that use em slot 31, the Ada and
  Sherry sections, death and game over, enemy types beyond the five tested.

## Next steps

1. A two-account real-Steam run.
2. Cutscenes and bosses; show grabs on the client.
3. Partner sections: let player 2 control Ada or Sherry when the host's partner slot is in use.
4. The puppet under heavy packet loss.
