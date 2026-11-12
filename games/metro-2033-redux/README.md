# Metro 2033 Redux (2014)

| | |
|---|---|
| Engine | 4A Engine |
| Versions tested | Steam (app 286690), Next-Gen update, `metro.exe` unpacked (SteamStub removed), SHA-256 `097df1a2be592e652e5f082bb7ecbcf4043d159034f65d2ec6ed34b7efb96f31`, Windows 11 x64 |
| Projects | [metro2033redux-coop](https://github.com/realblastex/metro2033redux-coop) (MIT), directory entry "Metro 2033 Redux" |
| QA level | none (scripted bot runs, no automated suite) |
| Last updated | 2026-11-12 |

## Where things stand

Nobody can play the story together yet. A prototype runs two copies of the game on one PC. The host's movement
reaches the client, but the joining player has no body of its own and watches through the host's character.

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
  entity, never linked to a net slot.
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
| 2.5 min to start a test | 55 s unskippable intro, fixed waits | load a post-intro save, wait on engine log lines | both in-game in ~30 s |

## QA

- Two copies of the game on one PC, driven by config: console commands, a fake XInput pad and waits on engine log
  lines. Results are read from heartbeat lines in each copy's log. Commands are in the project README
  ("Run a two-instance test on one PC").
- Last run 2026-11-12: both copies alive for the whole run. Host slot 0 resolves to `player`. A 4 s pad push moved
  slot 0 from x=-7.3 to -3.5 and back, seen on host and client. There are no pass/fail assertions yet.

## How to play

Not playable yet. Build from source per the project README; no release.

## Known issues

- The joining player has no body, and its own input does nothing.
- AI, doors, pickups and story events are not checked for sync. No level transitions.
- Only tested on one PC over loopback. Windows Firewall must allow `metro.exe` for LAN play.
- Diagnostics use fixed RVAs of one `metro.exe` build.

## Next steps

Create a second player entity on the host before the client joins (clone the `player` record with a new id via
the engine's create-from-record functions), then link it to net slot 1. The client's quick save then carries it.
After that, check the client's input and camera, and AI and door sync. Details: FINDINGS section 5.
