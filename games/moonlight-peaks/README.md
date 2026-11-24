# Moonlight Peaks (2025)

| | |
|---|---|
| Engine | Unity 6000.3.24f1, Mono |
| Versions tested | Steam app 2209900, build 25725760, exe SHA-256 `04BE5CFFF9909AF892968276D085E9C11B93BA82886749516A521250113511DB`, Windows x64 |
| Projects | Moonlight Peaks Co-op 0.1.0 (source in the owner's workspace; directory entry) |
| QA level | none (scripted two-instance smoke tests only) |
| Last updated | 2026-11-24 |

## Where things stand

A host/guest co-op mod runs: player 2 connects by IP, downloads the host save, gets a
separate player record and NPC relationship list, is visible and animated, loads the
host's rooms and shares the day clock. Farming, shops, story cutscenes and the sleep
transition are not properly networked yet, so this is an alpha, not a full playthrough.

## Existing work

- This repo and GitHub repository searches for "Moonlight Peaks" with `coop` and
  `multiplayer`: no project, 2026-11-24.
- No GitHub issue or PR in this repo mentions the game (`gh issue/pr list --search`).
- Not checked: Nexus, Thunderstore, Steam Workshop/community, Reddit, Discord.

## How the game works (for co-op)

Unity 6 Mono build; `Vampire.Runtime.dll` (about 13 MB) is unobfuscated and decompiles
readably. There is no netcode. One save-wide `GamePersistence` singleton holds the player,
NPC relationships, rooms, time and every entity persistence record. The player is a
`PlayerView` MonoBehaviourSingleton wrapping an `EntityCharacter`; NPCs use the same
`EntityCharacter`. Rooms are Addressables scenes switched by `GameSwitchRoomState`; the
Director graph stages characters and cutscenes.

`EntityBehaviour<T>` keeps a static registry keyed by entity asset GUID and destroys a
previously registered object when the same asset is registered twice. Spawning a second
`PlayerView` therefore destroyed the local player's body until a Harmony patch made
`allowDuplicateEntityAsset` true during puppet spawns.

Saves are JSON under
`%USERPROFILE%/AppData/LocalLow/Little Chicken Game Company/Moonlight Peaks/<steamid>/Saves/<guid>/GameData.json`.
A Harmony postfix on `Application.persistentDataPath` puts each mod profile in its own
folder.

## What we changed

All changes are a BepInEx 5 plugin; no game file was edited.

- Isolated save root per `--coop-profile`, and a save copy is downloaded by the guest over
  TCP as the join step, loaded through `AppMenuStateMachine.LoadGame`.
- Second `PlayerView` spawned as a puppet with its controller, mover, input, interactor and
  state machine disabled. Position, rotation, speed and state are sent at 20 Hz and applied
  to the puppet. `MonoBehaviourSingleton<PlayerView>.Instance` is pinned to the local player.
- The duplicate-entity destruction above is suppressed only while a second player is being
  instantiated, so the local body survives.
- Host sends its `RoomAsset` GUID when it changes; guest loads the asset with `Asset.Load`
  and runs `GameSwitchRoomState` with the same target room.
- Host sends day and day progression; guest applies them. Guest replaces its
  `GamePersistence.Player` and `NpcRelationships` after loading the host save, so
  relationships are per player.
- Player destroyed by a room unload is re-spawned from the room fallback or next to the
  other player, as local or puppet respectively.

## QA

No automated suite. The harness is a localhost control port (`--coop-ctl=<port>`) plus
`twotest.ps1` in the owner's workspace: it launches two copies with separate profiles,
loads, hosts/joins, then reads status with player positions, room, day and interaction
counters.

Checked on 2026-11-24:

- connection both ways, position/rotation exchange matching on both clients (moved one
  client with a teleport command, the other's puppet followed);
- host room change to `Room_Exterior_Plot` loaded on the guest and both local players were
  re-spawned there;
- host `IncrementDay` changed the guest clock from day 2 to day 3 within one second;
- guest loaded the host save over TCP and had 0 NPC relationships while the host had 62.

## How to play

Owner-facing package: `moonlight-peaks-coop/` at the workspace root. Run `install.ps1`,
start with the generated `.bat`, F8 to host or join. Host forwards UDP 47110 and TCP 47120.
The guest stays on the main menu and joins with the host's IP.

## Known issues

- Guest farming/shop results are local only; harvest/plant and container changes do not
  reach the host.
- Host cutscenes do not stage on the guest. Cutscenes already running when the guest loads
  still play locally.
- Sleeping moves the clock but the guest does not get the sleep transition.
- No lobby/NAT punch-through; direct IP only.
- Guest tools/equipment visuals and NPC name labels are not synced.
- Only build 25725760 was checked.

## Next steps

1. Route interaction states (harvest, plant, chest, shop) from the guest to host-side
   execution on the guest puppet, then send the resulting world state back.
2. Mirror Director cutscene begin/stage/end to the guest with staged stand-ins.
3. Send the sleep transition and day-start state to the guest.
4. Replace the direct-IP transport with the game's Steamworks session for relay/NAT.
5. Turn the control-port smoke test into a repeatable harness with assertions.
