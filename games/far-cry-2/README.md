# Far Cry 2 (2008)

| | |
|---|---|
| Engine | Dunia (a CryEngine 1 derivative), 32-bit x86. The 28 KB `FarCry2.exe` is a stub that calls `Dunia.dll!RunGame`; engine and netcode are in `Dunia.dll`, Lua game logic is in the archives |
| Versions tested | Steam, app id 19900, version 1.03, build id 52164. `Dunia.dll` SHA-256 `6353936a54aa841350bb30ff005727859cdef1aa10c209209b220b399e862765` (20,183,176 bytes). Every address in this write-up is for that file. Two PCs (Windows 11) on different networks over Steam since 2026-10-25; most later work on two copies on one PC |
| Projects | **Far Cry 2 Campaign Coop**, https://github.com/HitchLangley/far-cry-2-campaign-coop (MIT, maintained by HitchLangley; renamed from `fc2-coop-probe` in 2026-11). It started from the research probe https://github.com/MortyRei/fc2-coop-probe (initial import 2026-10-19) |
| QA level | automated for position and heading (two-copy test on one PC); everything newer was checked by hand on two copies on one PC, with logs. Story and most world state are not synced |
| Last updated | 2026-11-14 |

## Where things stand

Two players can start the campaign together from the host's save, joining from the game's own menu over Steam,
and see each other as animated avatars with weapons, shots and hit damage; they can share a car. The host runs
every NPC and the joiner sees the same NPCs as puppets. It is a playable prototype for walking around and fighting
together, not campaign co-op yet: missions, pickups, destruction, death and revive and most world state are not
synced, enemies attacking the joiner is an experiment, and the newest work has only run on one PC. The project's
own estimate is about 38 %. Open problems are tracked in the project's issues:
https://github.com/HitchLangley/far-cry-2-campaign-coop/issues

## Existing work

Searched on 2026-10-19 (web search, GitHub, Nexus, ModDB, Steam forums; Reddit only through search summaries): no
other Far Cry 2 co-op campaign project found.

- **Far Cry 2 Campaign Coop** (this write-up's project, above). Before 2026-10-26 the work happened as open PRs on the
  MortyRei repository; it moved to the maintained repository, which left the fork network on 2026-11-14.
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
- **Network player code drives the avatar.** The sender encodes its own player with the game's ThirdParty encoder
  (`FUN_100a6ec0`) into a memento; the receiver writes those fields into the avatar's memento and the stock NetPlayer
  code applies position, velocity, aim, stance and fire, so the avatar walks and runs with the game's own animations.
  The avatar archetype `player.MainCharacter.PawnPlayerNetwork` comes from the Far Cry 2 modding guide's entity library
  patch.
- **AI.** Each NPC has a `CPawnAgent` (component `FE4090`, agent at `+0x14`); `vf+0x164(1)` pauses it. The AI world
  `[0x11607608]` keeps a class map at `+0x44` whose `CPawnAgent` entry lists every agent; walking it replaces a memory
  scan (a 16 MB scan step took 47-78 ms in paged-out memory, a visible hitch). NPC target selection reads the other
  entity's AI component, so an avatar without one is invisible to them; a copy of the player's AI component with
  `CPawn.bIsAI = 1` makes host NPCs fight the avatar (with `bIsAI = 0` the agent takes over the global player agent
  `[0x11652108]` and the game crashes).
- **Town ceasefire.** Towns are ceasefire zones. A status object `[[0x11607408]+0xC]` holds "town alarmed" at `+0x198`,
  set each tick (`FUN_1095d5a0`) when an NPC with zone flag 0x02 is in combat states 2-5; `FUN_109613a0` shows the
  "Escape the cease-fire zone" message from it. An agent's zone kind is at `+0x998` (6 = town) and its zone flags at
  `+0x99C`. The ceasefire logic only knows the global player agent, which matters for a second player.

## What we changed

All changes are in test copies only; nothing touched an owner's install, profile or saves. The code, the build and
the full feature list are in the project repository; the dated evidence for the early steps is in [NOTES.md](NOTES.md).

- **Two copies on one PC** (2026-10-19 / 24): three same-length string patches in a copy's `Dunia.dll` (mutex,
  profile folder, save list; see "How the game works").
- **In-process probe** (`dinput8.dll` proxy, C, MinGW): Lua channel, native commands, own UDP transport, and a Python
  relay that carries it over Steam (friends lobby under Far Cry 2's AppID 19900, invites, Join Game).
- **Joining from the game's menu** (2026-11): Story Mode offers Singleplayer / Cooperative, then "Host a game" or
  "Join a friend's game" with the friends hosting; the joiner loads the host's save, reloads with it, and returns to
  the main menu when the host leaves; corner notices with Steam names.
- **Animated avatar** (2026-10-26 on): network player code instead of position writes; weapons with holster and draw
  animations, shots, grenade throws, doors, the map and GPS in the hand, the other player's arrow on the map.
- **Hits** (2026-11): the shooter's copy decides; a hit on the avatar damages that player; the joiner's hits on NPCs
  are applied on the host (bullets and crossbow bolts, counted by instigator).
- **Vehicles** (2026-11): cars move for both, the avatar gets in, drives and gets out with the game's animations.
- **Host-run NPCs** (2026-11): the host's AI runs every NPC within 80 m of either player; the joiner pauses its own AI
  and shows puppets that follow the host's path, idle actions, smart terrain (sitting, smoking), mounted guns,
  weapons and visible shots; NPCs the joiner's world lacks are spawned there. Fixed 2026-11-13: the joiner had copied
  save NPCs that were still streaming in (a timer compared unsigned) and removed the originals, so NPCs differed and
  the joiner crashed twice.
- **Enemies fight the joiner** (experiment, off by default): the avatar gets the player's AI component (patched
  entity library), so host NPCs shoot it and their hits take the joiner's health.
- **Performance** (2026-11-13): NPCs from the game's agent list instead of a memory scan; no frame over 50 ms in the
  check runs (before: 2-5 hitches of 60-95 ms every 10 s on both copies).

## QA

- `tests/two_copy_walk.ps1` in the project repository drives two real game clients on one PC: the joiner's avatar
  must follow the host's position (mean within 1.5 m) and heading (within 15 degrees), the joiner's own player must
  stay put, no crash lines, the real profile folder unchanged. Last recorded pass: 22 of 22 checks through two relays
  (2026-10-24/25). The early results and the two-PC runs are in [NOTES.md](NOTES.md).
- The later features were checked by hand on two copies on one PC, from the probe logs (packet counts, applied hits,
  puppet states) and the owner's view; menu joining, the animated avatar, hits and the map arrows also ran between two
  PCs over Steam. The probe logs frame times (`perf`) and the town ceasefire state (`cflog`).
- Not covered by an automated test: fights, NPC sync, vehicles. Not yet run on two PCs: everything since early
  November (NPCs, fights, vehicles).

## How to play

Prototype. Steam 1.03 only. Follow the quick start in https://github.com/HitchLangley/far-cry-2-campaign-coop: make a
test copy with `scripts/setup-copy.ps1`, build the entity library patch from the modding guide's files with the
project's tools, fetch `steam_api64.dll` with `scripts/get-steam-api.ps1`, put the probe into the copy, and start
`scripts/start-coop.ps1 -Role menu` on both PCs; then Story Mode > Cooperative > Host a game / Join a friend's game.
Both Steam accounts must own the game and be friends. There is no release build yet.

## Known issues

- Missions, objectives, pickups, destruction, fire and most world state are not synced; death and revive are not.
- Enemies attacking the joiner is off by default. In towns, after the ceasefire breaks, NPCs attack the host's
  player but rarely the joiner, and without a target they turn in place.
- A wounded NPC finished by the joiner can stay alive on the host.
- NPCs spawned for the joiner can look different from the host's; at most 24 NPCs are synced.
- The avatar has no animation at mounted guns and can keep a seated pose after leaving a vehicle.
- The two players can damage each other when they touch.
- Everything depends on the exact 1.03 build; the offsets are not portable.

## Next steps

1. Make NPCs choose the joiner as a target in towns after the ceasefire breaks, then make enemies attacking the
   joiner the default.
2. Run everything since early November between two PCs.
3. Wire the hub's sync test tools into the probe: `animcheck` (pose and weapon stream from the probe) and `inspect`
   (both copies side by side) first, then `chaos` for loss and delay.
4. World state with one owner per object (the plan is in the project): pickups, destruction, outposts.
5. Missions: capture the host's mission effects instead of running the scripts twice
   ([effect-capture-story-sync](../../playbook/effect-capture-story-sync.md)).