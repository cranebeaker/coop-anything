# Far Cry 2 (2008)

| | |
|---|---|
| Engine | Dunia (a CryEngine 1 derivative), 32-bit x86. The 28 KB `FarCry2.exe` is a stub that calls `Dunia.dll!RunGame`; engine and netcode are in `Dunia.dll`, Lua game logic is in the archives |
| Versions tested | Steam, app id 19900, version 1.03, build id 52164. `Dunia.dll` SHA-256 `6353936a54aa841350bb30ff005727859cdef1aa10c209209b220b399e862765` (20,183,176 bytes). Every address in this write-up is for that file. Reproduced on a second PC (Windows 11 23H2) with the identical file on 2026-10-24, and run between that PC and a third one (Windows 11 Pro, same file) over Steam on 2026-10-25 |
| Projects | No public co-op project. A research prototype (position + heading puppet, below): https://github.com/MortyRei/fc2-coop-probe (MIT); fixes, the relay and the tests are in its open PRs #1-#3 |
| QA level | automated, for the position puppet only (two-copy test on one PC: 20/20, 22/22 through relays; see QA), plus one manual two-PC run over Steam. Story, enemies and world state are not synced, so there is nothing to test there yet |
| Last updated | 2026-10-25 |

## Where things stand

There is no campaign co-op for Far Cry 2, and the stock multiplayer cannot carry the
campaign today (see below). A research prototype shows that a second player can be done:
two copies of the game load the same save and exchange the player's position and heading;
each copy spawns an invincible avatar of the peer, moves it smoothly and turns it. This works
with both copies on one PC and, through a relay over Steam (no IP addresses or port forwarding),
between two PCs on different networks (one manual run, 2026-10-25). It is not playable as
co-op: the avatar has no walking animation, no weapons or health sync, and mission state is
not shared. Details and the dated evidence
are in [NOTES.md](NOTES.md).

## Existing work

Searched on 2026-10-19 (web search, GitHub, Nexus, ModDB, Steam forums; Reddit only through
search summaries): no Far Cry 2 co-op campaign project found. Searches were "Far Cry 2
campaign co-op mod", the same with "Dunia" and "moddb", an extended co-op / Nexus / Reddit
query, a Russian query and a dedicated-server query.

- A 2013 Steam thread says the game is single player outside multiplayer
  (a user answer): https://steamcommunity.com/app/19900/discussions/0/864973123475897875
- FCSE (Far Cry 2 Engine Extension), Nexus mod 368, a launcher that loads DLL plugins from
  `bin\plugins\`: https://www.nexusmods.com/farcry2/mods/368 (page blocked WebFetch with
  HTTP 403; known from search summaries only; licence and source unknown). A possible
  plugin base for a co-op DLL.
- FC2MPPatcher (GPL-2.0) restores the online multiplayer after Ubisoft's services closed
  on 2021-06-01, for version 1.03. No co-op or campaign: https://github.com/halvors/FC2MPPatcher
- ModDB lists overhaul mods and a 16-player deathmatch map, none co-op.

## How the game works (for co-op)

What follows was observed on the test copies; each point is dated and sourced in [NOTES.md](NOTES.md).

- **Archives.** `Data_Win32\*.dat/.fat` are FAT2 version 5, compression version 3 (2-bit
  id: 0 none, 1 LZO1x). Gibbed.Dunia (zlib licence, https://github.com/gibbed/Gibbed.Dunia,
  commit `a43405cab3ea206ed0e9fcc6222fc2e36d545eea`) unpacks them after two small local
  patches to the v5 mapping and to the output buffer size (details in the notes).
  `Gibbed.FarCry2.ConvertBinaryObject` turns FCB files into XML (141 files failed).
- **Scripting.** Lua **4.0**, statically linked into `Dunia.dll`, plus "Domino" visual
  graphs stored as Lua files. 573 globals, 21 tables; `Game` has 98 methods. The Lua entry
  points (VA = 0x10000000 + RVA): `lua_gettop` 0x2AA9C0, `lua_settop` 0x2AA9E0,
  `lua_getglobal` 0x2AAF90, `lua_dobuffer(L, buf, size, name)` 0x2AB600, `luaL_check_lstr`
  0x2D24B0; the main `lua_State*` is `**(void ***)0x11606728`; natives register through
  `FUN_102aa850(0, name, cfunc)`. The script API has no function that returns a position.
- **Game modes are data.** `engine\gamemodes\gamemodesconfig.xml` lists `<GameMode>`
  entries with services; network modes mark services with `NetPresence`, `FCXSingle`
  (the campaign) does not. The set of active mission layers (`CMissionComponent` layer
  path) is the mission state a co-op would have to share.
- **Netcode.** `NetworkEngine`, `CNetObject*` with `Host` / `Controller` / `ThirdParty`
  roles (pawns, vehicles, projectiles, weapons). Pawn replication is defined in
  `engine\online\onlineengine\*.netobject.xml` (`CPawnControllerMemento`,
  `CPawnHostMemento`, `CPawnThirdPartyMemento` plus messages for shoot, reload, vehicles).
  Sessions go through Agora (Ubisoft online), which is gone since 2021.
- **Stock multiplayer today.** `-host -login <name> -lan` runs and opens UDP 9000 and 9001.
  `FC2ServerLauncher.exe -dedicated <cfg>` with `network_type 2` starts a LAN session.
  `-join <IPv4>` hard-codes network type 2 and first tries `gconnect.ubi.com:3074` and
  `onlineconfigservice.ubi.com`, both dead, so the second copy never reaches the LAN host.
  `-noagora` makes the client quit.
- **Command line.** `-exec`, `-benchmark`, `-host`, `-join`, `-wait`, `-load` (the dispatcher
  is `FUN_10663d40`). `-load <file>` opens `<profile>\Saved Games\<file>` literally but then
  exits within 3 s for a normal campaign save, so it is not a way into the campaign here.
- **Entities and physics.** Entity ids are 64-bit (Lua sees them as decimal strings).
  Lookup is `FUN_10055820` (thiscall; ECX = `*(void **)0x11644E80`, args: out, id low, id
  high); the object is at `holder+0xC`. Physics is Havok (RTTI present). The player's
  position lives in the motion transform of an `hkpRigidBody`-like object whose vtable is at
  RVA `0xE67658`: matrix rows at `+0x30` of the transform, position in the last row
  (z up). The matrix in the entity (`entity+0x30`, position at `+0x60`) is recomputed
  every frame from it, so writing the entity does nothing. Heap addresses change per
  session; finding the body by class and nearest position worked in every session.
- **Two copies on one PC.** A second copy needs its own mutex (the string `FarCry2Instance`
  in `Dunia.dll`, file offset 0xE08950) and its own profile folder. The profile folder comes
  from the 10-byte string `\Far Cry 2` at file offset **0xE0E094** (next to `\My Games` at
  0xE0E0A0 and `\GamerProfile.xml` at 0xE0E080, used by `CGamerProfileNative`); replace it with
  another 10-byte string such as `\FC2test_A`. The save that is opened follows it
  (`<profile>\saved games\`). The **save list** behind Continue / Load Game is built from a
  third string, the 19 bytes `My Games\Far Cry 2\` at 0xE089A0; replace it with the same folder
  (`My Games\FC2test_A\`). That string alone does not move the profile (the game still opened
  the owner's real `GamerProfile.xml`, 2026-10-24); without it a copy lists the saves of the
  real profile folder, and on a PC where the game never ran it lists none, only New Story
  (2026-10-25). All three patches change 17 bytes. The profile file `GamerProfile.xml` must
  exist, otherwise the client quits.

## What we changed

All changes are in test copies only; nothing touched the owner's install, profile or saves.

1. **Single-instance and profile patches** (same-length string replacements in a test
   copy's `Dunia.dll`: the mutex at 0xE08950, the save-list folder at 0xE089A0 and the profile
   folder at 0xE0E094): lets two copies run at once, each with its own windowed profile and
   saves. Proof: the probe's `CreateFileW` log shows copy A opening `FC2test_A\GamerProfile.xml`
   and `FC2test_A\saved games\<save>.sav`, copy B the `FC2test_B` ones, with an 800x600 window
   as set in each test profile (2026-10-24). The 2026-10-19 recipe patched only 0xE089A0, which
   does not redirect the profile; the 2026-10-24 write-up then called 0xE089A0 unneeded, which was
   wrong: a copy on a fresh PC patched without it listed no saves (2026-10-25).
2. **In-process probe** (`dinput8.dll` proxy for the test copies, built with MinGW,
   about 850 lines of C, source in https://github.com/MortyRei/fc2-coop-probe). It hooks the engine's Winsock, file and `PeekMessage` imports and
   adds: a Lua channel (a file `coop_cmd.lua` is run on the main thread through
   `lua_dobuffer`, the result is read back from a Lua global), native commands (peek,
   entity lookup, body search by class, teleport of the body), memory scans, and the
   transport and avatar code below. Proof: the Lua result `alive 3`, and the position
   readings that matched the entity (notes, dated 2026-10-19).
3. **Own UDP transport** (loopback ports 9101 and 9102, not the engine's 9000 to 9004):
   a 28-byte packet with sequence, position and direction about 16 times per second per
   copy. Proof: both logs show `rx` growing and each copy's `remote` equal to the other's
   `local`; a teleport in one copy appeared in the other's log at once.
4. **Avatar of the peer.** `SpawnEntityFromArchetype(name, x, y, z, rx, ry, rz)` with the
   player's own entity name creates a second pawn; the engine event
   `CFCXCountersComponent_SetInvincible` makes it invincible (without it the pawn took damage
   from the pinned writes and ended the mission with "Game over" in both copies); the probe
   writes its body position every main-loop tick with 25 % smoothing. Proof: the owner
   watched the avatar of the other copy ride a 3 m circle smoothly, no falling, no Game over.
5. **Avatar body binding fix** (2026-10-24). Bug: when both copies load the same save and nobody
   has moved yet, the avatar is spawned where the local player stands, and the body search
   (nearest body of the class within 1 m) returned the **local player's own body**. Both copies
   logged the avatar body equal to the local body, so the probe wrote the peer's position into the
   local player instead of the avatar. Fix (8 lines in `find_body` / `av_tick` / `av_valid`): the
   avatar search skips the local body and waits until it is known. Proof (logs and `pos` readings,
   not an automated test): after the fix the two bodies differ in both copies; with `walk 3 20 2`
   in copy A, copy B's avatar followed A's circle 0.5-1 m behind while B's own player stayed put
   (about 0.1 m drift). The fix is in the probe's PR #1, not merged yet.
6. **Avatar heading** (2026-10-24). The physics body is an upright capsule whose rotation is
   always identity; mouse look changes only the yaw rows of the entity matrix (`entity+0x30`:
   `(d0,d1,0)`, `(-d1,d0,0)`). The avatar has no controller, so a rotation written there stays;
   writing the body's rotation or calling `TeleportEntity` with angles changes nothing, and
   `SpawnEntityFromArchetype` uses its last argument (yaw, degrees) only at spawn. The probe now
   writes the received direction into the avatar's entity matrix every tick (25 % smoothing).
   Proof: the two-copy test turns A with the mouse three times and B's avatar matched within
   0.0 degrees (a probe without the change failed, avatar stuck at 0 degrees); the owner saw the
   avatar turn on screen.
7. **Network mode** (2026-10-24). With a peer address other than 127.x the probe binds on all
   interfaces and uses only packets from the peer's IP (others counted as `drop`). Proof on one
   PC with its own LAN address: 19/19, 30 forged packets from 127.0.0.1 all dropped. Not run
   between two PCs (the two test PCs were on different networks).
8. **Relay over Steam** (`relay/fc2_relay.py`, 2026-10-24/25). The probe talks only to a local
   relay; the relay carries its packets as Steam P2P messages (Spacewar, AppID 480, friends-only
   lobby, session accepted only from lobby members, keep-alive and 20 s timeout), or over plain
   UDP as a stand-in for tests. Scripts `scripts/setup-copy.ps1` (copy + three patches + test
   profile) and `scripts/start-coop.ps1` (one side of a session) prepare and run each PC.
   Proof: see QA, two-PC run.

## QA

One automated test: `tests/two_copy_walk.ps1` in the probe repository (added in
https://github.com/MortyRei/fc2-coop-probe/pull/1, not merged yet). It drives two real game
clients on one PC; both copies must already be patched and have their test profiles.

```
powershell -ExecutionPolicy Bypass -File tests\two_copy_walk.ps1 -CopyA <test copy A> -CopyB <test copy B> -OutDir <results folder>
```

It starts both copies, presses Enter until each probe log shows its own save being opened,
reads the player id through the Lua channel, runs `netstart` on both, walks copy A's player
with `walk 3 20 2`, and samples `pos <avatar>` / `pos <player>` in copy B. It asserts: each copy
reads its own profile and save; the same player id in both; the avatar's body differs from the
local body; at least 10 packets/s each way; the avatar moves (span at least 1.5 x radius); the
avatar stays within 1.5 m (mean) and 2.5 m (max) of A's position as received by B; B's own
player drifts at most 0.5 m; no `CRASH` / `ACCESS VIOLATION` lines; the real profile folder is
unchanged (snapshot before and after). A kill switch stops both copies if a log line names the
real profile folder. It presses Enter in the game windows, so the keyboard must be left alone.

Last results (2026-10-24, Steam 1.03, one PC, Windows 11 23H2):

```
probe with the avatar-body fix:
RESULT: 16 checks, 0 failed -> PASS   (~2 min)
  link A/B 16.0 / 16.0 packets/s; avatar span 5.92 m; distance to A mean 0.43 m, max 1.07 m;
  B player drift 0.12 m; real profile: new 0, changed 0, removed 0

negative control, probe before the fix:
RESULT: 16 checks, 3 failed -> FAIL
  B player stays put: max drift 16.46 m (B's own player dragged); avatar body A/B: no pointers in the old log line
```

The drift check is the one that catches the avatar-body bug; `avatar moved` and
`avatar follows A` passed with the old probe too. Earlier checks, by hand and from logs, are
listed with dates in [NOTES.md](NOTES.md).

Later additions to the same test (probe PRs #1-#3): a heading phase (A turned three times with
the mouse; `avatar heading follows A` within 15 degrees), window titles A/B, `-PeerIp` (network
mode, plus a forged-packet check) and `-Relay` (the chain through two udp relays):

```
heading added:               RESULT: 18 checks, 0 failed (headings A/B 128.1/128.1, -145.4/-145.4, 149.8/149.8)
  negative control (no heading code): 1 failed, avatar stayed at 0 degrees
peer = own LAN address:      RESULT: 19 checks, 0 failed (30/30 forged packets dropped)
window titles checked:       RESULT: 20 checks, 0 failed
through two udp relays:      RESULT: 22 checks, 0 failed (relay A 837 out / 715 in, 0 dropped)
```

**Two PCs over Steam, manual run (2026-10-25).** Two PCs on different networks, two Steam
accounts that are friends, Steam 1.03 on both, `scripts/start-coop.ps1` on each side (host /
join). The joining relay found the host's lobby through the friends list and its session was
accepted; the first message decoded with the sender identity at offset 16 (64-bit layout,
confirmed only with two real accounts). Probe packets went both ways at about 16/s for ~7
minutes with 0 dropped and no disconnect. `walk 3 20 2` on the host: on the joining PC the avatar
(sampled every 2 s) went round about two circles of radius ~3 m around the expected centre
(2 m/s x 20 s = 40 m), while that PC's own player stayed within ~0.1 m; the owner saw the avatar
move on both screens. When the joining side stopped, the host's relay marked the peer down after
its 20 s timeout. There is no scripted pass/fail for these runs yet; the evidence is the two sides'
logs.

Later the same day, same two PCs (the joining relay in `--join wait` mode for the last two):
- **Heading across PCs:** the host turned its player with the mouse 171.3 → 81.3 → -171.3 → 77.6
  degrees; the joining PC's avatar (sampled every 2 s) showed the same four headings 1-2 s later,
  staying in place.
- **Steam invite:** host `--invite <name>`; the invite was accepted on the second PC, the relay got
  callback 333, joined the lobby, and the link and avatar were up within 3 s.
- **Join Game** from the Steam friends list (the host sent no invite): joined the host's new lobby,
  also through callback 333 (Steam reports Join Game on a friend in a joinable lobby as
  GameLobbyJoinRequested; 337 was not observed).
- Every run: 0 dropped, no crash lines, no real profile folder in either probe log.

Not covered: an invite or Join Game while the joining relay is not running, longer sessions, loss.

## How to play

Not playable. The probe is a research tool, not an installer: it lives in patched test copies,
and both players must load the same save. `scripts/setup-copy.ps1` and `scripts/start-coop.ps1`
in the probe repository (PR #3) set up and start one side each; the Steam relay needs 64-bit
Python and `steam_api64.dll` from Steamworks.NET (downloaded and hash-checked by
`scripts/get-steam-api.ps1`), and shows both players as "playing Spacewar". Source, build
steps, the command list and the two-copy recipe are in
https://github.com/MortyRei/fc2-coop-probe (MIT); the game code is kept there, not in this repo.

## Known issues

- The avatar slides in an idle pose: no walking animation, no weapon, no health or damage
  sync. It faces where the other player looks, not where it moves.
- The avatar passes through scenery (it mirrors the peer's already-collided position; in a
  test where the source was moved by script it went through a tree).
- No mission, objective or world-state sync; no vehicles.
- A script-spawned pawn made from the player's archetype looks like the player; other
  `PawnPlayer.<name>` archetypes also looked the same in the campaign.
- Scanning memory with direct reads crashed one copy once; `findbody` now reads in chunks, but the
  `scan` command still reads memory directly and can crash a copy.
- In the probe as published (without PR #1), the avatar can bind to the local player's body
  when both players start on the same spot, and it never turns (see "What we changed", 5 and 6).
- In network mode the probe's UDP port is reachable by anyone who can route to it (only the
  peer's packets are used; no encryption). The Steam relay avoids open ports.
- Everything depends on the exact 1.03 build; the offsets above are not portable.

## Next steps

1. Locomotion animation: decode the typed sub-objects of `CPawnControllerMemento` and
   `CPawnThirdPartyMemento` (about eleven fields at `0x70 .. 0x120`), or compare memory of a
   walking and a standing pawn and write what differs.
2. Mission state: reproduce the set of active mission layers and Domino state in the
   second copy; `Game:MissionSetState` and `SaveGame` are the entry points seen.
3. Two-PC run with scripted checks (the joining side already samples the avatar; compare it
   with the host's path and headings automatically).
4. Extend the two-copy test as features land (mission state next).
