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
host's rooms, follows Director cutscenes and shares the day clock. World changes are copied
between clients but still reload the room, so this is an alpha, not a full playthrough.

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
- Held item entry (GUID, quality, metadata) is sent at 20 Hz; the puppet equips/unequips it
  and re-applies it after a reload, and the local body re-applies its own item after a
  respawn.
- Director subjects are mapped per side (host=1, guest=2, NPC=GUID) and the host's clone is
  mapped back to the guest, so a guest cutscene resolves to the guest body. A
  `DirectorVariableRef` postfix swaps player subject overrides to the clone while a guest
  node runs.
- Room follows carry a cause, keep one latest target, wait for a safe window, defer director
  nodes while pending and replay the latest after the room is stable. A stuck switch can be
  replaced through a synthetic director `SwitchRoomNode`; a 3 s mismatch check repairs
  `CurrentRoomAsset`/`Room.Instance` disagreement.

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
- host and guest Director graph nodes mirrored (12+ nodes on the guest in 6 s) and both clients entered the day-start cutscene;
- guest and host room changes both loaded on the other client; world persistence was pushed and applied once, with a 6 s cooldown;
- guest loaded the host save over TCP and had 0 NPC relationships while the host had 62;
- guest hold → host puppet wand and host hold → guest puppet wand, both cleared by unequip,
  and both re-applied after a room reload;
- guest `startday` resolved the host subject to the guest clone and the guest subject to its
  own player; host `startday` resolved to the host player, and both clients advanced the day;
- `warp`+`startday`+`move` fired together converged 3/3 runs on both clients with both
  players alive.

## How to play

Owner-facing package: `moonlight-peaks-coop/` at the workspace root. Run `install.ps1`,
start with the generated `.bat`, F8 to host or join. Simplest network without router access:
both players install ZeroTier, join the same free network, and the guest joins the host's
ZeroTier address (allow the game through Windows Firewall). With port forwarding instead:
UDP `47110` and TCP `47120`. Launching through Steam also loads the mod; the first mod
launch copies existing saves into the `default` profile so they still show in the load menu.
Uninstall to return to vanilla.
The guest stays on the main menu and joins with the host's IP.

## Known issues

- World persistence is copied and the receiver reloads the room, so it repositions; rapid
  farming can rubber-band.
- A follow during a cutscene waits for the wake-up to pass, so it can take ~20-30 s, and the
  cutscene's own room change may win over a requested warp.
- Coins live in the player inventory, so each player pays with their own coins.
- No lobby/NAT punch-through; direct IP only.
- Only build 25725760 was checked.

## Next steps

1. Route interaction states (harvest, plant, chest, shop) from the guest to host-side
   execution on the guest puppet, then send the resulting world state back.
2. Send world deltas instead of reloading the whole room, so farming stops repositioning.
3. Cut cutscene-time room latency: bound the wake-up window and leave the graph resumable
   instead of waiting out the fallback.
4. Map the sleep transition.
5. Replace the direct-IP transport with the game's Steamworks session for relay/NAT.
6. Turn the control-port smoke test into a repeatable harness with assertions.





