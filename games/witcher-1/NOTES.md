# The Witcher (2007, Enhanced Edition): agent notes

Raw notes from a private co-op prototype. Paths use `<game dir>`. Addresses are from the decrypted
in-memory image of the Steam build (32-bit, no ASLR, image base 0x400000).

## Game identity

| | |
|---|---|
| Store | Steam, app 20900 (Enhanced Edition), `<game dir>\System\witcher.exe` |
| Engine | BioWare Aurora (the Neverwinter Nights lineage), with CD Projekt's changes |
| Binary | 32-bit, no ASLR, base 0x400000, exe encrypted with SteamStub |
| Where to disassemble | Dump the decrypted image from a running process; the on-disk exe is encrypted |

## What the engine already has

- The NWN client/server message layer is still inside the game: server-to-player and player-to-server
  messages with major/minor ids (for example creature updates 5.1, area loading 3.x, player input 6.x).
  Single player runs it in-process. A second `CNWSPlayer` can be added to the server and its messages
  forwarded over TCP, so the layer can be revived instead of writing sync from scratch.
- Movement is not networked in single player: the server drives client objects directly. A mod has to
  send creature transforms and animations itself.
- Area changes are module-style reloads. Two game copies share save files, so the second copy needs its
  own `%LOCALAPPDATA%` subfolder, and the two loads must not start at the same time.

## Cutscenes and the world pause (the useful finding)

- A scripted cutscene reaches the client as server-to-client message 18.19: two strings (cutscene name,
  on-end script) followed by five 32-bit integers. The server sends it to **player 0 only** (the player id
  is a hard-coded 0 in both senders), so a cutscene triggered by a second player plays on the host.
- **Client-to-server Input 6.25 is not "cutscene ended". It is "world pause ON/OFF" (pause type 4).**
  The cutscene player sends it twice: ON at its first update after loading, OFF at the end. Payload is two
  bits, byte `0xB0` = ON and `0xA0` = OFF. The server keeps one pause mask for the whole world (no
  per-player counter), so one stray ON freezes the server timer for everyone until some OFF arrives.
- The cutscene's on-end script is requested separately, by client-to-server 15.8 `(script, objectId, bool)`,
  just before the final OFF.
- Consequence for co-op: if the host forwards a second player's 6.25 to the server as the host, a lost OFF
  freezes the world for good. Never forward a remote client's 6.25. Decide per mode instead:
  - everyone watches: let the host's own client pause the world natively (this is the default);
  - only a remote player watches: drop that player's 6.25 so the host's world keeps running (the watching
    character is unprotected while it runs);
  - only the host watches: keep the pause. We also tried dropping the host client's own 6.25 so the world kept
    running: the interception worked, but the host crashed in 1 of 2 runs about 5 s after its cutscene ended,
    right after its client received a backlog of server messages (it receives none while its cutscene plays;
    232 creature-update messages (5.1) arrived in one second instead of 16 with the pause; 0 crashes in 4 runs with the pause).
    The client is built to run its own cutscene with a paused world.
- Detecting the end of a remote client's cutscene: poll the client's cutscene-player global (a pointer that
  is non-zero while it plays) and treat "zero for 3 s" as the end.

## Test results (agent-driven, loopback, two game copies on one PC, one run per mode unless noted)

| Mode | Result |
|---|---|
| Guest-only watch, host world keeps running (1 run) | Works. World pause mask stays 0, host keeps walking, sync resumes |
| Host-only watch, world paused (the engine's own behaviour) | Both worlds frozen for the length of the cutscene, then resume; 0 crashes in 4 runs |
| Host-only watch, host's 6.25 dropped (world keeps running) | Interception works, but 1 of 2 runs crashed the host after the cutscene; reverted |
| Guest's 6.25 forwarded to the server | The world froze for good (the OFF was lost); never do this |
| Initiator unknown: both watch | Works; guest ends 0.06 s after the host; no double execution |
| Default configuration, both watch (2 runs) | Works 2 of 2 |

Not tested: a scripted (not injected) cutscene, the on-end script, two PCs, manual input.

## Tooling notes that cost time

- Cold Ghidra import of the 16 MB dump through an MCP wrapper took about 14 minutes; the default 330 s
  startup deadline fails, and parallel first requests start parallel imports that all time out. Raise the
  deadline and send one request first.
- Offline decompile helpers that guess function starts by scanning back for `CC CC` mislabel addresses of
  the neighbouring function. Re-check a function boundary before trusting an xref listing.
- A game that checks for the Steam client launches fine from Steam but exits silently (or shows
  `Application load error 5:0000065434`) when started directly with the usual `SteamAppId` environment
  variables **if Steam itself runs elevated**. Restart Steam without administrator rights. A game started by an
  elevated Steam is elevated too, and cannot be closed from a normal shell.
- Window-capture APIs (`PrintWindow`) return a black frame for the game's D3D9 window; tests had to rely on
  the mod's own log lines.
