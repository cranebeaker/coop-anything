# Metro 2033 Redux (2014)

| | |
|---|---|
| Engine | 4A Engine |
| Versions tested | Steam (app 286690), Next-Gen update, `metro.exe` unpacked (SteamStub removed), SHA-256 `097df1a2be592e652e5f082bb7ecbcf4043d159034f65d2ec6ed34b7efb96f31`, Windows 11 x64 |
| Projects | [metro2033redux-coop](https://github.com/realblastex/metro2033redux-coop) (MIT), directory entry "Metro 2033 Redux" |
| QA level | none (scripted bot runs, no automated suite) |
| Last updated | 2026-11-12 |

## Where things stand

Nobody can play the story together yet. A prototype runs two copies of the game on one PC. Each player has their own
Artyom: both walk around the first level, and each sees the other move. NPCs near the players and an active monster
follow the host. Combat, doors and story sync are untested.

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

## QA

- Two copies of the game on one PC, driven by config: console commands, a fake XInput pad and waits on engine log
  lines. Results are read from heartbeat lines in each copy's log. Commands are in the project README
  ("Run a two-instance test on one PC").
- Last run 2026-11-12 10:13: both copies alive for the whole run (~2 min of co-op). The host's pad moved slot 0 from
  x=-7.3 to -3.5; the client saw it. The client got body 0x10B, moved it (x -7.3 to -1.4, then a strafe), and the host
  saw slot 1 follow. Host and client screenshots each show the other player's Artyom. There are no pass/fail
  assertions yet.

## How to play

Not playable yet. Build from source per the project README; no release.

## Known issues

- Both bodies spawn on the same spot; the client steps away with `@teleport`.
- The engine only networks entities in its net list (players, human NPCs and active monsters in Hunter), by distance
  priority. A far NPC did not update on the client within 15 s. The client also lists 9 dormant intro `nosalis` the
  host does not; they are inactive on both sides.
- Entities the host spawns through the engine's spawn event appear on both sides with the same id, but a spawned
  monster stays asleep (how to wake it is unknown). Doors are not networked.
- The client's cloned body has no knife (the knife is not a child of `player`).
- Combat, doors, pickups and story events are not checked for sync. Combat needs story progress: weapons are holstered
  at the start of Hunter. No level transitions, no save/load.
- Only tested on one PC over loopback. Windows Firewall must allow `metro.exe` for LAN play.
- Diagnostics use fixed RVAs of one `metro.exe` build.

## Next steps

Get a test save at a fight (or find how scripts wake spawned monsters) and test combat; aim the bot at a door and test
door use with two bodies; give the client a knife; then level transitions and LAN play. Details: FINDINGS 5.4.
