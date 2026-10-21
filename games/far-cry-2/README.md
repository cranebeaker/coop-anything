# Far Cry 2 (2008)

| | |
|---|---|
| Engine | Dunia (a CryEngine 1 derivative), 32-bit x86. The 28 KB `FarCry2.exe` is a stub that calls `Dunia.dll!RunGame`; engine and netcode are in `Dunia.dll`, Lua game logic is in the archives |
| Versions tested | Steam, app id 19900, version 1.03, build id 52164. `Dunia.dll` SHA-256 `6353936a54aa841350bb30ff005727859cdef1aa10c209209b220b399e862765` (20,183,176 bytes). Every address in this write-up is for that file |
| Projects | No public co-op project. A research prototype (position puppet, below): https://github.com/MortyRei/fc2-coop-probe (MIT) |
| QA level | none (the prototype was checked by hand, the owner looking at the screen; there is no automated test) |
| Last updated | 2026-10-19 |

## Where things stand

There is no campaign co-op for Far Cry 2, and the stock multiplayer cannot carry the
campaign today (see below). A research prototype shows that a second player can be done:
two copies of the game on one PC load the same save and exchange the player's position
over their own UDP socket; each copy spawns an invincible avatar of the peer and moves it
smoothly. It is not playable as co-op: the avatar has no walking animation, no heading, no
weapons or health sync, and mission state is not shared. Details and the dated evidence
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
  in `Dunia.dll`) and its own profile folder (the 19-byte string `My Games\Far Cry 2\` at
  file offset 0xE089A0, replaced by another string of the same length). The profile file
  `GamerProfile.xml` must exist, otherwise the client quits.

## What we changed

All changes are in test copies only; nothing touched the owner's install, profile or saves.

1. **Single-instance and profile patches** (two same-length string replacements in a test
   copy's `Dunia.dll`): lets two copies run at once, each with its own windowed profile.
   Proof: two copies running together in the world for minutes.
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

## QA

No automated suite. What was checked, by hand and from logs, is listed with dates in
[NOTES.md](NOTES.md). A real test would drive two copies with the probe's own commands and
assert on the `net:` log lines and the entity positions; that harness does not exist yet.

## How to play

Not playable. The probe is a research tool, not an installer: it lives in the test copies,
needs per-copy patches, and the two copies are started and connected by hand. Source, build
steps, the command list and the two-copy recipe are in
https://github.com/MortyRei/fc2-coop-probe (MIT); the game code is kept there, not in this repo.

## Known issues

- The avatar slides in an idle pose: no walking animation, no turning towards the
  direction of travel, no weapon, no health or damage sync.
- The avatar passes through scenery (it mirrors the peer's already-collided position; in a
  test where the source was moved by script it went through a tree).
- No mission, objective or world-state sync; no vehicles.
- A script-spawned pawn made from the player's archetype looks like the player; other
  `PawnPlayer.<name>` archetypes also looked the same in the campaign.
- Scanning memory with direct reads crashed one copy once; the probe now reads in chunks.
- Everything depends on the exact 1.03 build; the offsets above are not portable.

## Next steps

1. Pawn heading and locomotion animation: decode the typed sub-objects of
   `CPawnControllerMemento` and `CPawnThirdPartyMemento` (about eleven fields at
   `0x70 .. 0x120`), or compare memory of a walking and a standing pawn and write what
   differs.
2. Mission state: reproduce the set of active mission layers and Domino state in the
   second copy; `Game:MissionSetState` and `SaveGame` are the entry points seen.
3. A real transport (hosted or direct, with a join code) instead of loopback ports.
4. Add a test harness to the probe repository (two copies driven by its own commands,
   asserting on the `net:` log lines and entity positions).
