# Metro 2033 Redux (2014)

| | |
|---|---|
| Engine | 4A Engine |
| Versions tested | Steam (app 286690), Next-Gen update, `metro.exe` unpacked (SteamStub removed), SHA-256 `097df1a2be592e652e5f082bb7ecbcf4043d159034f65d2ec6ed34b7efb96f31`, Windows 11 x64 |
| Projects | [metro2033redux-coop](https://github.com/realblastex/metro2033redux-coop) (MIT), directory entry "Metro 2033 Redux" |
| QA level | automated (smoke test, see QA) |
| Last updated | 2026-11-13 |

## Where things stand

A prototype, tested with two copies of the game on one PC; no release yet. A launcher hosts or joins. Each
player has their own Artyom: both walk around the first level, and each sees the other move. NPCs near the players
and an active monster follow the host. In the armed level `l19_defence` the joiner can fire, each sees the other's
shots, and shooting the other player kills them on both sides; a killed joiner respawns. In `l03_chase` the joiner
rides the handcar next to the host. When the host dies, both games restart from the host's last checkpoint. A story
level change (console `change_map`, l03 to l04) takes the host to the next level, and the joiner's game restarts
and joins it. Doors, the handcar and story scripts are not networked; each game runs its own copy of them.

## Existing work

- No co-op campaign mod found (see the "Metro 2033" gap entry: Metro: Terminus archived, Metro 2033 H Hour dead,
  a Redux Extended World pack mentions a multiplayer mod without detail).
- [MetroDeveloper](https://github.com/tsnest/MetroDeveloper): developer tools for Redux, including a
  `multiplayer_fix` for the engine's leftover multiplayer. It predates the Next-Gen update. Its signatures no longer
  match (its `rlog` signature fails, so it disables itself), but its notes pointed at the leftover netcode.

## How the game works (for co-op)

- `metro.exe` still contains 4A's dev multiplayer: `net_core`/`net_player`, ENet on UDP 1000, console commands
  `server` and `connect <ip>`. Clients receive the host's quick save and load the host's level from it.
- The Steam exe is SteamStub-protected. Analyse an unpacked copy; disassembling the protected `.text` gives garbage
  (early "encrypted code" conclusions were wrong for that reason).
- Net players get bodies only from the server's player spawner, and only in MP mode. Story levels have one `player`
  entity, never linked to a net slot. The host can still clone `player` through the engine's player spawn event; the
  event replicates and links the clone to the joiner's slot.
- Full RVA-level notes, labelled verified/derived/unknown:
  [docs/FINDINGS.md](https://github.com/realblastex/metro2033redux-coop/blob/main/docs/FINDINGS.md).

## What we changed

| problem | cause | fix | proof |
|---|---|---|---|
| second copy refuses to start | named single-instance mutex | launcher patches the `jne` in memory, no file change | two copies in-game |
| host crash when a client connects | server writes a player slot past the vector end | grow the vector before `send_props_pack` (port of MetroDeveloper `multiplayer_fix`) | client connects, host alive |
| nothing is ever sent | a startup pause is never popped, game clock frozen | pop all engine pauses after load | state packets flow ~15 Hz |
| client stuck on loading | engine waits for a key after a map load | call `loader_finish` | client reaches gameplay |
| client crash right after joining | net player lookup returns a slot without an actor | return "no player" when the slot's actor doesn't resolve (networked only) | client alive in gameplay |
| host's player not networked | story-level actor never linked to net slot 0 | link it by hand (`@link_actor`) | client sees slot 0 move with the host |
| joining player has no body | a story level has one `player`; nothing spawns one per net slot | host clones `player` through the engine's own player spawn event (RVA 0x6A9A80 id list + RVA 0x6BA940 with is_player) 15 s after a join | slot 1 linked on both sides; client walks its own body; each side sees the other's Artyom |
| 2.5 min to start a test | 55 s unskippable intro, fixed waits | load a post-intro save, wait on engine log lines | both in-game in ~30 s |
| a killed joiner stays dead | single-player death; the engine's own slot reset makes the client reload the level, which crashed | host spawns a new body onto the still-linked slot 5 s after death; guards for two crashes on a cloned body (no ammo items, removal timer on a freed corpse) | client gets body 2 and plays on (smoke `-Respawn` PASS) |
| crashes on a second level (`l19_defence`): host at net start, client during load and when it fires | game code assumes one single player always exists (null controlled entity; ammo HUD index left unset when the cloned body has no ammo item) | link slot 0 in the net-start frame; three small guards (skip / disable the player-dependent part / zero the index) | both players armed in l19, client fires, both alive ~2.5 min |
| host death ends the session | single-player death screen; any in-process load after hosting crashes | restart both games through the launcher from the host's last checkpoint; the client rejoins by itself | 2+ death cycles, ~65 s each |
| saves written while hosting crash on load | the save stream depends on the engine's MP flag; the joiner's cloned body also broke the load | clear the MP flag while the save serializes (~15 ms); leave the body's entities out of the save loop; the joiner's game writes no saves | hosted saves load in a fresh game, a session restarts from one |
| a story level change crashes the joiner | the joiner follows `change_map_cl` and applies the level's `.player` carry-over with no controlled entity; joining after the change crashed too (carried-over items get ids 0x0..0x23) | joiner skips the engine's level change and restarts into the host's new level; host hosts again there and skips the carry-over while hosting (it loses the carried-over items) | joiner in l04_riga with its own body after the host's l03 to l04 change |

## QA

- Two copies of the game on one PC, driven by config: console commands, a fake XInput pad and waits on engine log
  lines. Results are read from heartbeat lines in each copy's log. Commands are in the project README
  ("Run a two-instance test on one PC").
- Automated smoke test: `scripts/smoke_test.ps1` in the project repo starts both copies, lets the host and the
  client each move, and checks 8 facts from the logs: both alive, no crash event, body spawned, slot 1 linked on both
  sides, client controls its body, host movement seen by the client, slot 0 and slot 1 positions equal on both sides.
  Runs 2026-11-12 15:2x: Hunter `RESULT: PASS (8 checks)`, `l19_defence` `RESULT: PASS (8 checks)`; a deliberately
  too-short run gives `RESULT: FAIL (6 of 8)`. `-Respawn` adds a kill of the client and checks the automatic
  respawn: `l19_defence -Respawn` `RESULT: PASS (10 checks)`. Latest runs 2026-11-13 15:4x (after the save and
  level-change work): Hunter `PASS (8 checks)`, `l19_defence -Respawn` `PASS (10 checks)`, `-Launcher`
  `PASS (8 checks)`. Death restarts and the level change were checked with scripted two-copy runs (not in the
  smoke test yet).
- Manual bot checks with screenshots: each side sees the other's Artyom; the host sees the client's tracer; the host
  shot the client's body to health -1.0 on both sides.

## How to play

Not playable yet. Build from source per the project README; no release.

## Known issues

- Both bodies spawn on the same spot; the join step moves the client 1.2 m in front of the host (otherwise the
  client sees the host's gun inside its own view).
- The other player's weapon is drawn wrong (the host sees the client holding a machete while it holds an AK).
- A dead joiner gets a new body 5 s later, 1.2 m in front of the host (a wall there would block it). The corpse
  stays on the host but vanishes on the client.
- The engine only networks entities in its net list (players, human NPCs and active monsters in Hunter), by distance
  priority. A far NPC did not update on the client within 15 s. The client also lists 9 dormant intro `nosalis` the
  host does not; they are inactive on both sides.
- Entities the host spawns through the engine's spawn event appear on both sides with the same id, but a spawned
  monster stays asleep (how to wake it is unknown). Doors are not networked.
- The client's cloned body has no knife (the knife is not a child of `player`). Its ammo items are cloned without a
  parent; the mod re-attaches them to the new body with the engine's attach call.
- Damage, enemy AI, doors, pickups and story events are not checked for sync. In `l19_defence` the monsters stay
  dormant for the first minutes.
- The handcar in `l03_chase` is not networked: the joiner's copy runs behind the host's.
- Checkpoints while hosting overwrite the host player's real `auto_save` (with a loadable co-op save).
- After a level change the host has the new level's default loadout, not the items carried over.
- A joiner that rejoined while a scripted fight was running crashed once on load (cause unknown).
- Only tested on one PC over loopback. Windows Firewall must allow `metro.exe` for LAN play.
- Diagnostics use fixed RVAs of one `metro.exe` build.

## Next steps

Test a level's own end trigger (not only console `change_map`); keep the carried-over items (find why their ids
break the joiner's load); network the handcar and doors; LAN play. Details: FINDINGS 5.8 to 5.11.
