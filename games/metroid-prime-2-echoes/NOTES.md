# Metroid Prime 2: Echoes (Trilogy) co-op: notes

Technical reference behind [README.md](README.md). All addresses are for `rs5mp2_p.dol`, SHA-1
`2375606f4e9429a699cfa02728b0bb1176421226`, from Metroid Prime Trilogy NTSC-U `R3ME01` rev 0. Function names
are Ghidra's `FUN_xxxxxxxx` unless a better name is known. "Live" means read or written in the running game
with the effect observed; "static" means disassembly only.

## Game, engine, executables

- Metroid Prime Trilogy (Wii, 2009), extracted disc. Retro Studios' engine, PowerPC big-endian, closed binary.
- The front end `rs5fe_p.dol` (= `main.dol`, SHA-1 `d55b890c4e7c2cc6f2e120ec4ef988c186780308`) loads
  `rs5mp2_p.dol` when Echoes starts and is loaded back when Echoes is left, so every patch is gated on rs5mp2
  being resident (see Packaging).
- Enemies and many script classes are RSO modules loaded at runtime (`MP2/RSO/Production`); their `bl`s are
  unrelocated on disc (0x48000001), so callers can only be found live. SDA bases: r13 0x805d1660, r2 0x805d4280.
- PrimeHack's PRIME_2 NTSC table matches this DOL, but PrimeHack drives player 1 only; use mainline Dolphin,
  single-core (dual-core hit a GPU FIFO desync in the Echoes intro).
- Echoes' versus mode already runs 2-4 Samus actors, cameras and HUDs in one world; the mod loads the campaign
  world in that mode and fixes what assumes one player.

## Existing work

SubsonicBeatz154's 2019 asset-only Trilogy co-op mod (abandoned) listed the blockers that need code: death ends
the match, save stations off, rooms follow player 1, world teleports overwrite inventories, Scan Visor dead,
separate inventories, friendly fire, enemies target player 1, cutscene cameras break for player 2. xjonx's
GameCube attempt hit the multiplayer memory limit. Randovania multiworld links separate GameCube games.

## Memory map

### Globals

| Address | What |
|---|---|
| 0x804e72e8 | `CStateManager` (static object in .bss) |
| 0x805cb314 | `CGameState*` (= r13-0x634c); +0 current world MLVL; +0x148 rules object (owned pointer at +0x144) |
| 0x805cb220 | options/system data (r13-0x6440): +0x29 load flag, +0x2c save file index, +0x80 difficulty |
| 0x805cb310 | application object; +0x70 -> +0x114 owns the game state |
| 0x805cb338 | boot path flag (1 = InitialWorld/FrontEnd path) |
| 0x804a4f20 | powerup maximum capacity table [109] |

### CStateManager (0x804e72e8)

| Offset | What |
|---|---|
| +0x810 | object list |
| +0x8e4 | message queue (pumped by FUN_80244c08) |
| +0x14f0 | u32 player count (1 single player, 2-4 versus) |
| +0x14f4 | `CPlayer*[4]`, indexed by player number (about 30 functions read `sm + idx*4 + 0x14f4`) |
| +0x1504 | `CPlayerState*[4]` (same as players[i]->+0x12ec) |
| +0x1514 | camera managers, 4 bytes per viewport; manager +0x12 = current cinematic camera uid (0xffff none), +0x14 first-person camera |
| +0x153c | load state |
| +0x1da0 / +0x1da4 / +0x1da8 | player / state / camera manager used for the viewport being drawn (set by FUN_8024b72c) |
| +0x1dac | `CWorld*`: +0x24 area count, +0x2c area array (8 bytes, +4 = `CGameArea*`), +0x54 loaded-area list (next at +0xd4) |
| +0x1e28 | map/automapper data (exists in co-op) |
| +0x1e44 / +0x1e48 | current / previous area index |
| +0x1e98 | camera filter passes: 0x2c each, 11 per viewport, 0x1e8 per viewport; pass 1 at +0x1ec4 (P1), +0x20ac (P2) |
| +0x2bd0 / +0x2bf0 | message box STRG id / time |
| +0x2c0c | pause state: 0 none, 1 map, 2 (special function 37), 3 MP pause menu, 4 Data Network, 5 save prompt, 6 message box |
| +0x2c78 | index of the viewport being drawn |

Writers of +0x1e44: SetCurrentArea FUN_8023eb60(mgr, &idx), FUN_8023f6c0 (spawn), FUN_8023be44 (load).

### CPlayer

| Offset | What |
|---|---|
| +0x8 | entity uid (u16; CEntity), +0xc editor id |
| +0x20 | transform (3x4, translation at +0x2c/+0x3c/+0x4c); writing it does not move the player |
| +0x50 | position used by docks, triggers and AI |
| +0x1a0 | velocity |
| +0x2c4 / +0x374 / +0x378 / +0x390 | move / camera / morph / orbit state (orbit 1 = locked) |
| +0x39c | orbit (lock-on) target uid; +0x3c8 lock-on candidate |
| +0x5a4 | scanning state (0 none, 1 scanning, 2 complete); +0x5a8/+0x5ac scan time, +0x5b0 target uid, +0x5b4 scan asset id |
| +0x5f0 | first-person pitch |
| +0xec0 / +0x138c | id-buffer enable / per-player 64x32 buffer of on-screen object ids (Scan Visor) |
| +0x12ec | `CPlayerState*` |
| +0x12f0 | camera manager (player code asks this for "in a cinematic"); +0x18 morph ball camera |
| +0x1388 | player index |

Player teleport: FUN_80160cc4(CPlayer*, CTransform4f*, mgr, bool). The last argument resets the morph ball
camera (via +0x12f0 -> +0x18, vt+0x84); SpawnPlayer and arrivals pass 1, spawn points 0.

### CPlayerState (ctor FUN_8016d008(this, playerIndex, persistent); size >= 0x5b0)

| Offset | What |
|---|---|
| +0x00 | player index |
| +0x04 | flags; bit 0x80 = alive (deathmatch rules), bit 0x40 clear = immune (checked by FUN_8014baa4) |
| +0x10 | 99.0 at spawn, not live health |
| +0x14 | current health (f32) |
| +0x34 | current visor (0 Combat, 2 Scan; setter FUN_8016e000) |
| +0x38..+0x40 | vector<u16> {size, capacity 0x20, data*} |
| +0x50 | current suit (0 Varia, 1 Dark, 2 Light), recomputed when ids 12-14 change |
| +0x54 | powerup count = 0x6d (109) |
| +0x58 + id*0xc | powerups[109]: {s32 amount; s32 capacity; u32 extra} |
| +0x588 | controller index (set by FUN_8016d76c) |
| +0x594 / +0x59c | scan map: count / entries of 8 bytes {u32 scan id, u8 progress (0xff = done), pad}; all 821 ids pre-listed |

Accessors (ps, id, n; all bounds-check id < 0x6d): FUN_8016d998 InitializePowerUp (capacity += n, clamped to
0x804a4f20[id]; amount = min(amount, capacity)); FUN_8016d984 capacity = 0 then that; FUN_8016db60 IncrPickup
(amount += n, clamped; ids 41/42 refill health); FUN_8016daec amount = 0 then IncrPickup; FUN_8016dc10
DecrPickup; FUN_8016de44 set amount; FUN_8016de70 / FUN_8016debc capacity; FUN_8016de90 HasPowerUp (called
every frame by the HUD); FUN_8016db00(ps, f) health += f; FUN_8016e39c(ps, id, f1) SetScanTime (quantised byte
at entry+4) / FUN_8016e29c GetScanTime.

Powerup ids (match Randovania's `prime2` header): beams 0 Power, 1 Dark, 2 Light, 3 Annihilator, 22 Charge;
combos 4 Super Missile, 5 Darkburst, 6 Sunburst, 7 Sonic Boom; visors 8 Combat, 9 Scan, 10 Dark, 11 Echo;
suits 12 Varia, 13 Dark, 14 Light; 15 Morph Ball, 16 Boost, 17 Spider, 18 Bombs; 23 Grapple, 24 Space Jump,
25 Gravity Boost, 26 Seeker, 27 Screw Attack; keys 29-31 and 101-106 Sky Temple, 32-40 Dark Temple;
41 health refill, 42 Energy Tank, 43 Power Bomb, 44 Missile, 45 Dark Ammo, 46 Light Ammo, 47 item %,
0x2f pickup counter, 0x40 kills, 0x41 deaths (saved with the state), 0x61 used by a later gate condition,
97-100 translators. Unknown: 19-21 (capacity 1 in versus), 80 (capacity 9999 in versus).

### Game state and rules

- CGameState (0x640 bytes, ctor FUN_800b9178): +0 current world; +0x8/+0x10 world states (0x24 each: +0 MLVL,
  +4 area index, +0x18 desired MREA); +0x14 player state count (4), +0x18 player-state rc_ptrs (8 bytes each);
  +0x74 + 8i per-player MP entries; +0x14c MP settings ('FRND', 0x64 bytes); +0x638 bit 0x40 forces loadouts.
- New game: FUN_800ba8ac hardcodes world 0x3bfa3eff (Temple Grounds) and area 0x62b0d67d (Landing Site) at
  0x800ba8e0-0x800ba900 (`lis r31` at 0x800ba8e0, `addi r4,r31` at 0x800ba8e8/0x800ba8f4). Rewriting them
  starts a new game at another area's default spawn point (a useful test aid).
- Rules object = `*(*(0x805cb314)+0x148)`; game mode tag from vt+0x44: 'SNGL' single player, 'FRND' front
  end, 'DTHM' (0x4454484d) deathmatch, the mode co-op runs in. IsMultiplayer FUN_8024b4fc = not SNGL/FRND.
  - Single-player rules vt 0x804b9648 (SpawnPlayer = `blr`).
  - Deathmatch rules vt 0x804b4838 (built by FUN_800760d8). vt+0x1c SpawnPlayer FUN_8010cd94, vt+0x20
    0x8010d580 (`li r3,1; blr`, "apply spawn loadout"), vt+0x38 IsGameOver FUN_80076644.
  - Fields: +0x04 time limit (600.0), +0x08 elapsed (advanced by `stfs f0,0x8(r31)` at 0x8010d560 in the tick
    FUN_8010d528), +0x38 player count, +0x3c kill limit, +0x40 flags (0x80 game over, 0x40 kill limit enabled,
    0x08 limit reached), +0x4c + 0x14i per-player {f32 respawn timer, u8 dead}.
- Deathmatch update FUN_800762e8: per player, death detected at 0x80076400 (`rlwinm.` on state+4 bit 0x80),
  marks dead, sets the timer and increments item 0x41; each frame the timer runs down (`lfs f0,0(r23)` at
  0x80076414), then FUN_80140cec(player, mgr) and SpawnPlayer (call at 0x80076450). Kill limit check:
  `blt +0x40` at 0x800764bc; falling through sets +0x40 |= 8 and calls vt+0x40, vt+0x3c (match end, 0x800764c0).
- SpawnPlayer FUN_8010cd94 rebuilds the CPlayerState from scratch (new state via FUN_8016d008 copied over the
  live one, persistent blocks kept via FUN_8016e770/FUN_8016e88c), applies the respawn point's loadout to all
  109 slots, teleports, then notifies `SPWN` (`bl 0x8010ca50` at 0x8010d478). It is the death respawn path.
- Arrival spawn FUN_8023f45c(mgr, spawn point, player index), callers 0x8023fe68, 0x8023fed0 (single-player
  default), 0x80240090 (MP per-player loop): teleports, then raises to the spawn point's loadout only if rules
  vt+0x20 != 0 (only call site 0x8023f578) or game state +0x638 bit 0x40. Used at session start and every
  WorldTeleporter world change.
- MP match start FUN_800ba388 (state machine FUN_800eacec(..., 8), front end rules 'FRND'): builds the DTHM
  rules, rebuilds the game state by save/load round trip (FUN_802a9174; the world becomes the arena's), installs
  the rules (FUN_800bc414 at 0x800ba6a0), writes MP settings and per-player entries (state+0x588), ends with
  FUN_800ba230, FUN_800bb364(gs), FUN_8017e00c(gs+0x94); epilogue 0x800ba7d0.

### Spawn points, world teleporters, special functions

- CScriptSpawnPoint (vt 0x804b8950, AcceptScriptMsg FUN_8020c5f4): +0x04 area, +0x1c flags (bit 31 first
  spawn), +0x20 transform, +0x54 + 4*id loadout amount, +0x20c + 4*id capacity override (getters FUN_8020c594,
  FUN_8020c5bc).
  - `RSET`: for every player and every id, FUN_8016d984 (0x8020c65c) and FUN_8016daec (0x8020c67c), i.e. the
    whole inventory is replaced by the loadout; then teleports every player if bit 31 is set. RSET comes only
    from level scripts (story loadout changes: the Landing Site intro, the item loss).
  - `ZERO`: loads the target area if needed, SetCurrentArea, teleports every player (no inventory change).
- CScriptWorldTeleporter vt 0x804b95a8, AcceptScriptMsg FUN_8022da4c; +0x1c active, +0x20 MLVL, +0x24 MREA,
  +0x2c bit 0x40 skip transition. The MP front end world 0xdaea74a3 has one per arena; Sidehopper Station's
  targets MLVL 0xa50a80cc / MREA 0x02690de1. It receives XCRT/XALD/XWLD, ACTV at match start, XDEL; never ZERO.
  A world change through it spawns via FUN_8023f45c (raise only), not SpawnPlayer or RSET.
- CScriptSpecialFunction vt 0x804b8cb8, type at +0x150, AcceptScriptMsg FUN_802107f4 dispatching through a
  member-function table at 0x804b8958 (12 bytes per type). Type 5 map station FUN_8020e598, type 7 save station
  FUN_8020e800, types 31-34 power-up grants FUN_8020f780/f838/f8d4/f9ac (item +0x208, amount +0x174, player
  selector +0x170 resolved by FUN_802147ec, grant FUN_80241a98), type 37 FUN_8020fbe4 (pause state 2).
- Entity vt 0x804b8678 (FUN_802031e4) on ZERO grants item +0x20 (capacity +0x28, amount +0x24) to state 0 only.
- Script connections are {state, msg, u32 editor id}; the target's editor id is at +0xc.

### Areas, docks, doors

- CGameArea: +0x3c min xyz, +0x48 max xyz, +0x8c dock count, +0x94 docks (0x48 bytes each; plane points at
  +0x14/+0x20/+0x2c), +0xd0 == 0x10 when loaded (the dock think's own test, FUN_8009af64/FUN_8009af7c).
- CScriptDock (type 55, vt 0x804b8088, think vt+0x14 FUN_801e6a84): +0x2c8 dock index, +0x2cc area index,
  +0x2d0 state (1 -> 2 armed, 3 = in a non-current area), +0x2d4 flags. In state 2 it tests
  `bl FUN_801e6cec(dock, mgr, players[0]+0x50)` at 0x801e6bf8 (r30 dock, r31 mgr); if crossed and the other
  side is loaded it calls SetCurrentArea. That is why only player 1 could change the room.
- CScriptDoor (type 56, vt 0x804b8128): its trigger (type 92) sends ENTR->INCR / EXIT->DECR; the door sends its
  dock OPEN->INCR and CLOS->DECR, so a door stays open while anyone stands in its trigger. Dock message handler
  FUN_801e64bc handles INCR/DECR and ends in `bl 0x801e6398(opened, other side's CGameArea*, CWorld*)` at
  0x801e6a5c. Room unloading (ULOD) comes from streaming, not from the door.
- Only docks, spawn points and the arrival spawner call SetCurrentArea; portals use those paths. Plane from
  three points: FUN_8033b77c(out {n, d}, a, b, c), normal toward the crossed side.
- Static collision ray: FUN_800a2ac8(mgr, &start, &unit dir, &filter, f1 length) -> 1 if clear. Filter as the
  AI line-of-sight test builds it: {u64 include = 1 << *(r13-0x78d8) (solid bit), u64 exclude 0, u32 type 1};
  FUN_802b8834 is the 64-bit shift.

### Cameras, HUD, pause, messages

- Camera message handler FUN_801dafbc: a camera started/stopped by a player goes to that player's manager
  (index FUN_8024ba90), other senders to player 1's. FUN_8005c1d0(manager) = cinematic active; FUN_8005c1ec =
  active with the camera's multiplayer flag (+0x2c -> +0x208 bit 1); FUN_8005c024 ends it. Top-level draw
  FUN_800e885c and viewport layout FUN_8024b554 collapse to one view when FUN_8005c1ec(players[0]'s) is true.
- Frame: world pass FUN_800e84d4 for all viewports (viewport select FUN_8024b72c), then HUD pass FUN_800e864c
  (`cmpwi r4,0` at 0x800e8654 skips it in single view; loop bound at 0x800e870c), HUD draw FUN_800dd98c ->
  FUN_801577fc (message window at 0x80157d20).
- HUDs: frame object +0x18 -> HUD container, HUD k at +0x60 + 8k. HUD ctor FUN_80156d7c builds the 0x3ac-byte
  map/pause frame object (+0x30) only when HUD+4 (single-player flag) is set. HUD_SET_MODE 0x80159854 (1/2 map,
  3/4 pause, 6 message, 7 MP "Quit Game?" menu).
- Camera filter pass: CCameraFilterPass::DisableFilter 0x80056bf8(this, f1 fade time). A death fades pass 1 to
  black on every viewport; versus arenas clear it with their own script objects, campaign rooms do not.
- SetPauseState FUN_8024ab94(mgr, state) ignores every request under MP rules (`bl IsMultiplayer; cmpwi; bne`
  at 0x8024abb0-0x8024abb8). Show message FUN_8024ac68(mgr, STRG, f1) sets +0x2bd0/+0x2bf0 and state 6; the
  message screen FUN_800e8d50 -> FUN_801599b0 draws a paged box on HUD 0.
- Controller input: FUN_800ddc04(huds, mgr, input) gives each CFinalInput only to the HUD whose state +0x588
  equals input +4 (`cmpw r7,r0` at 0x800ddc28, inside a `bdnz` loop); FUN_80158a40 -> FUN_800f82f0 (message).
- Pause button = game action 0x43 (Wii 2), handled in FUN_800e8xxx; state 4 handler FUN_800e8ccc. Map button =
  action 0x44 (Wii 1) in FUN_80241530; map opened by FUN_800e8bcc; placement HUD+0x1e4 recomputed each frame by
  FUN_80157320; automapper update FUN_8002b824, draw FUN_8002a3b0. Sites: see code K.

### Scanning

- Can-scan FUN_80149f3c needs action 0x42 held (FUN_8014ed30) and orbit locked, then returns false under MP
  for non-player targets. SetScanningState FUN_80149d30 opens the scan pane (ScanPause FUN_8024b1d4) only in
  single player; completion sends the target its scan-done message (0x8014a430). Actor vt+0x78 -> scan info
  {+0 SCAN id, +4 STRG id}; CastToActor FUN_8029fd70.
- Lock-on candidate update FUN_80166a38: in the Scan Visor single player uses the scan target search
  FUN_8016a8fc (reads the id buffer); MP falls back to the combat search, which never offers scan-only points
  (gate locks, map stations). Scan aim update FUN_8016a410. SetOrbitTarget FUN_80168598, SetOrbitState
  FUN_80168670. The id buffers are GPU copies (FUN_8024bf80 -> graphics vt+0x138) of each viewport region.

### Damage

- ApplyDamage FUN_80242338(mgr, &damager, &damagee, &radiusSender, info, filter, knockback); radius path
  FUN_80243a70. Both call FUN_802429ec(out, mgr, &damager, &radiusSender, &damagee, info) (calls at 0x802423b0
  and 0x80243ad4) to build the final CDamageInfo (+4 damage, +8 radius damage, +0x10 knockback).
- ObjectById FUN_8023e9fc(mgr, &uid); CastToWeapon FUN_8029ede0, weapon's firing entity uid at +0x2c8.

### Enemies and AI

- Per-entity think loop FUN_80240720: `bl 0x80240994(mgr, entity, f1)` at 0x80240be8 calls vt+0x14 Think.
  PreThink loop FUN_80240c40 (vt+0x10 via `bctrl` at 0x80240d50). Touch pass 0x80240888-0x80240944.
- Messages: immediate FUN_8024b388 (`bctrl` 0x8024b3d0) and queue pump FUN_80244c08 (`bctrl` 0x80244c70);
  everything sent with FUN_8024b3ec, including damage, is queued. Message: +2 sender, +4 target, +8 kind.
- IsA: FUN_8029da94(entity, type id); each IsA function in the DOL compares `cmpwi r4,N` (type id read from
  vt+0xc word 0). CPatterned is type 4.
- CPatterned: InDetectionRange FUN_80130864 loops every player; InRange FUN_8013071c, InMinRange FUN_80130794
  (+0x3c4), InMaxRange FUN_801307fc (+0x3c8) and the "player too far" timer in Think FUN_8012b93c (0x8012c2ac:
  +0x3e0 counts while players[0] is beyond +0x3d8 = 25; give up at +0x3dc = 5 s) read players[0] only.
  Line of sight FUN_80130bbc.
- Stored target player uid, set once from players[0] (type id: class +field): 117 Boost Guardian +0x1098,
  149 Alpha Blogg +0x9b0, 132 Space Pirate +0x31c, 118 Jump Guardian +0x9f0, 113 Quadraxis (DigitalGuardian)
  +0x1052, 116 Ing +0xd7c, 134 / 135 Splitter +0xcd8 / +0xf0e, 127 Rezbit +0xca2. A static scan of every RSO
  for "players[0] -> lhz 8 -> sth OFF(obj)" also found ChozoGhost +0x31c, CommandoPirate +0xbe2, DarkCommando
  +0xca4, DarkSamus +0x700 (111; retargets by itself), FlyingPirate +0xacc, IngSnatchingSwarm +0x3f6, MediumIng
  +0x700, ScriptCannonBall +0x122. Also seen in a fresh game: Quadraxis 112, Grenchler 115, Elite Pirate 114,
  Sandworm 130. `lwz rX,0x14f4(rY)` per RSO: Dark Samus 46, Sandworm 38, Grenchler 36, Digital Guardian 34,
  Sand Boss 29, Splinter 28, Emperor Ing 3 23.
- CScriptPickup vt 0x804b8498: Think FUN_801f7d04 (nearest player in +0x1b8), Touch vt+0x50 FUN_801f8b2c(pickup,
  other, mgr); item +0x150, amount +0x154, capacity +0x158, +0x15c added to 0x2f, +0x1c8 bit 0x10 = "set" variants.
- Safe zones (RSO class, trigger subclass) and save/elevator pads are triggers: they track every actor.

### Saves

- Save data per file = a 2000-byte game-state blob (a 'GMST' record per used file; the third file at 0x5d84).
  FUN_801b2414(slot) -> FUN_802a9e88(app, memstream) replaces the game state. Load task: FUN_801ad590(mem, 0)
  (0x5c bytes, from operator new 0x80315990), FUN_801ad764(task, 1), FUN_801adfcc(task, file). Save stations
  save to file `*(0x805cb220)+0x2c` (FUN_801ade7c). A save holds all 4 player states.
- Save prompt answers show a message: station Yes 0x1dffe3e7 / No 0x50b34a43, gunship Yes 0x47816a80 /
  No 0x0acdc324.

## Packaging

- One combined Gecko code: the gate, single-word `04` writes, and per hook a C2 stub
  `lis r12; ori r12; mtctr r12; bctr` to a body in MEM2. Each body ends with the same far jump to site+4.
  r12 and CTR must be free at every site (they replace a call, sit at a function entry or epilogue, or r12 is
  dead). Exceptions with CTR live stay inline C2 bodies: 0x800ddc28 (a `bdnz` loop; a far jump there crashed
  the front end MP menu at 0x800ddc24), 0x80157c14, 0x80157c84.
- Bodies are written by a Dolphin `[OnFrame]` patch at 0x93700000-0x93800000: above the game's MEM2 arena
  (hi 0x935e0000) and the IOS heap (0x935e0000-0x93600000); all zero under Dolphin's HLE IOS. MEM1 has no gap
  (arena hi = FST start 0x817fa320). OnFrame applies every frame, so bodies cannot go stale, but live edits to
  them from outside do not stick. MEM2 data used by the codes: 0x937f0000 lives pool, +4 lives per player,
  0x937f0010 HUD-on-top flag, 0x937f0014 controller that opened the Data Network.
- Dolphin's code list runs from 0x80002338 and must end below 0x80003000 (0x80001800-0x80003000 belongs to
  the code handler). Codes past the limit are dropped silently; before the MEM2 move the list reached
  0x80002ff0. Writing caves into that range with `06` codes only works while the list is short.
- Gate: 23 `20` (if equal) words: 0x8014bacc = 0x4082000c, 0x8023fe68 = 0x4bfff5f5, 0x804b4854 = 0x8010cd94
  (words no patch touches), plus one word every 256 KB of rs5mp2 text (0x80006508 to 0x8049ff00, each away
  from patch sites; listed as `GATE` in the project's `coop_patches.py`). With only the first three, leaving
  Echoes let the code handler patch the incoming front end for a frame (0x80076414 held the C2 stub branch,
  0x800bc6a0 the difficulty word), which then crashed into a hook body ("Invalid read from 0x000014f0,
  PC = 0x93702078"). With 23, quitting multiplayer to the main menu leaves the front end untouched.
- Every original word is asserted against the DOL before the codes are built (a wrong displaced word once
  crashed at boot). Dolphin invalidates JIT blocks for code written by Gecko; external writes into already
  compiled code need a JIT flush (save and load a state).
- Save states contain Dolphin's code list (it lives in game RAM): a state made with older codes runs the
  older codes, and old stubs with a new MEM2 body layout crash. Boot fresh after changing codes.

## Codes

Hook tables list site, original word, and what the replacement does. "nop" = 0x60000000.

### A. Keep inventory

Problem: a death respawn (SpawnPlayer) leaves only the respawn point's loadout (vanilla: every marker item,
Gravity Boost and Grapple lost, suit 0). Every arrival also raised players to the arrival spawn point's
loadout, since DTHM vt+0x20 always says yes (Great Temple Transport B hands out Dark/Echo Visor, Gravity Boost,
Seekers, ammo, 3 Energy Tanks). On a new game each player arrives at a different spawn point with other items.

| Site | Original | Patched |
|---|---|---|
| 0x8010d01c | 0x901d0050 `stw r0,0x50(r29)` | nop: keep the suit field |
| 0x8010d020 | 0x41820058 `beq 0x8010d078` | `b 0x8010d078`: keep the powerup block |
| 0x8010d2b0 | 0x7f83e378 `mr r3,r28` | `b 0x8010d364`: skip the 109-slot loadout loop |
| 0x8010d580 (hook) | 0x38600001 `li r3,1` | 1 only while state Power Beam capacity (+0x5c) is 0, i.e. the session's first spawn, and only for player 1; players 2-4 get a copy of player 1's health, suit and items |

Verified: a real versus death (player 1 at 1 HP shot by player 2) respawned at 99 HP with suit 1, Dark/Light
Beam, Dark Suit and Missile 30/50 kept. A WorldTeleporter jump into Great Temple Transport B and a real elevator
ride Temple Grounds <-> Great Temple changed neither inventory. New game: both players identical. Dying does
not refill ammo. Story RSET stays vanilla and reaches every player: at the item loss (Hive Chamber B, MREA
0x1d7bd3ae, area 17; trigger 0811005a -> 0011001c -> loss spawn point 08110021) both players lost Missiles,
Space Jump, Bombs, Boost and Spider Ball.

### B. Campaign session

Problem: versus ends on time (600 s) or kill limit, only loads arenas, and after a death both views stay
black and the respawn point is often in another room.

| Site | Original | Patched |
|---|---|---|
| 0x8010d560 | 0xd01f0008 `stfs f0,0x8(r31)` | nop: elapsed never advances (HUD clock stays 10:00) |
| 0x800764bc | 0x41800040 `blt +0x40` | `b 0x800764fc`: kill limit never reached |
| 0x8022da4c (hook) | 0x9421ffe0 `stwu r1,-0x20(r1)` | WorldTeleporter aimed at Sidehopper Station (a50a80cc / 02690de1) is re-aimed at Temple Grounds / Landing Site (3bfa3eff / 62b0d67d) |
| 0x800ba7d0 (hook) | 0x396100e0 `addi r11,r1,0xe0` | load the co-op save (below) |
| 0x8010d478 (hook) | 0x4bfff5d9 `bl 0x8010ca50` | after the notify: DisableFilter(0.5 s) on pass 1 of every viewport; place the respawned player (r27) beside the first other player (code C's placement), morph ball camera reset |

Co-op save load, when the current world is Temple Grounds (the re-aimed arena): keep the rules object (clear
gs+0x144 so the old state's destructor does not delete it), the MP settings (+0x14c, 0x64 bytes), the
per-player entries (+0x74, 0x20) and each state's +0x588; load save file index 2 (the third file) as
single-player Continue does (operator new 0x5c -> FUN_801ad590(t, 0); FUN_801ad764(0, 1); FUN_801adfcc(t, 2);
FUN_801ad764(t, 1)); set `*(0x805cb220)+0x2c` = 2 again (the loader resets it); put everything back; copy
player 1's +0x10/+0x14 and +0x50..+0x574 to states 1-3 (not +0x0 index or +0x40 owned pointer); fill the
lives pool (code L); re-run FUN_800ba230 / FUN_800bb364 / FUN_8017e00c. An empty file gives a new game.

Verified: Sidehopper Station starts Temple Grounds with 2 players, clock frozen (elapsed 0.00 after 2.5 min);
kills at 9 plus a real kill: no results screen. Saved with player 1 marked (Dark Beam, Missile 33/40), fresh
boot: both players resume in the save room with Dark Beam and 33/40. A single-player file copied to the third
file in the Trilogy menu gives a byte-identical GMST record and resumes in co-op. Death: screen stays lit,
respawn next to the partner. Placing the player 2.5 units behind the partner instead fell through the floor
(facing 135 and 180 deg at a Landing Site door: z -56.5 vs -38.2); on the partner's spot the player always
ends at the partner's height, about 1.1 units away, but two Morph Balls on one spot stick together, so
respawns now use code C's placement (checked walls and floor): placed 2.25 units away, on the floor. Both
dying at once: both respawn where they died.

### C. Shared rooms

Problem: the dock think tests only players[0] (0x801e6bf8). Player 2 crossing alone leaves the current area
unchanged, the dock disarms, and player 2 falls into an unrendered room.

| Site | Original | Patched |
|---|---|---|
| 0x801e6bf8 (hook) | 0x480000f5 `bl FUN_801e6cec` | any player crossing switches the room |
| 0x801e6a5c (hook) | 0x4bfff93d `bl 0x801e6398` | on door close, bring players still behind it |
| 0x801db1f8 (hook) | 0x807f1514 `lwz r3,0x1514(r31)` | camera "stop" from a non-player sender checks every player's manager, then continues at 0x801db25c |

- Crossing: the original FUN_801e6cec test per player, plus distance^2 from the door centre (mean of the
  dock's four plane points, +0x10 count, +0x14..+0x38) at most farthest corner^2 + 6^2, i.e. within 6 units of
  the opening. A fixed 6 from the mean of three points failed at big doors: Hive Chamber C's door to Hive
  Transport Area is 15 x 10 and that point sat 6.7 above the floor, so walking through never switched the room
  and the room unloaded when the door closed (its lift console then dropped every scan). The dock tests once, when the door reports someone passing; a player waiting in the
  old room is past the plane too, and plane-only made the room flip back and forth. Players in the old or new
  room are left alone; players in neither (two or more rooms back) are placed beside the crosser at once.
  In-room test: position within 1 unit of the area's bounds.
- Door close: on DECR (r3 == 0) with 2+ players, take this dock's plane from its own area's list (the room
  behind is beyond it when the dock's area is current). Leader = first player not on that side; every player on
  that side and inside that room is placed beside the leader. r3-r5 are saved (clobbering r5 crashed).
- Placement (also used by respawns): right (transform column 0), left, behind, front, and (door close) away
  from the door along its normal, at 2.25 units, then all again at 1.6; a loop over a direction table in the
  frame (unrolled, it added about 1,700 words of hook body). A spot needs: a clear static ray (RAY_CLEAR
  0x800a2ac8) from the leader at ray height to 0.5 past it; a ray straight down from it hitting floor within 4;
  and (door close) not on the closing room's side of the plane, since the door is not static collision. All
  fail: the leader's spot. Standing leader: ray height = lift = 1, leader's facing. Morph Ball leader (CPlayer
  +0x374 != 0; 2 = ball): the CPlayer transform rolls with the ball (forward (0.01, 0.59, -0.81) in a test), so
  the frame is upright, facing away from the door (or +y for respawns); ray at 0.7 (the position is the ball's
  bottom), spot 0.15 up (1 unit put the ball's top into a 1.8-unit tunnel's ceiling), checks at ray height (a
  floor probe from 0.15 up started under a gently rising tunnel floor and found nothing). The follower gets the
  frame's facing; FUN_80160cc4 with the morph ball camera reset.
- Camera stop: a save room's arrival camera is started with player 2 as sender and stopped by a timer, so only
  player 1's manager was cleared and player 2 kept cinematic uid 0x44f (frozen view, no control).

Verified: player 2 opens a Hive Access Tunnel door and walks through, player 1 waits 14 units back: room
2 -> 0 once at 1.1 s; at 2.2 s (door closed) player 1 placed right +2.72, forward -0.47, up +1 of player 2.
Player 1 waiting 3 units back (in the door trigger): no teleport for 5 s, then walks through, room stays 0.
With the earlier crossing-time teleport: a morph-ball follower moved 57.5 units, ball camera 6.4 units away on
the next sample, no launch; four players all followed. The door-close version was tested with two players.
Big door (Hive Chamber C -> Hive Transport Area): player 2 at foot level 5 past the plane (old rule 8.32 > 6,
new 6.73 < 10.83): room switch at once; door closed: player 1 placed 2.27 units to player 2's side; the lift
console then scanned in 1.5 s. Morph Ball tunnel between Hive Chamber B and C (about 1.7 wide and tall): before
the ball fixes, rolling from C's part into B's put player 1 exactly on player 2 (no spot passed); after, player 1
landed 1.6 to the side (balls resting 1.40 apart, touching) and, B to C, 2.25 to the side. Playtests confirmed
the big door and the tunnel both ways.

### D. Cutscenes and full-screen message boxes

Problem: a cinematic played in player 1's half only (campaign cameras lack the multiplayer flag); a camera
player 2 triggers went to player 2's manager; only player 1 was held during cinematics; message boxes were
drawn in player 1's half and long texts did not fit.

| Site | Original | Patched |
|---|---|---|
| 0x801db154, 0x801db22c, 0x801db2b8, 0x801db310 | 0x5460103a `rlwinm r0,r3,2` | `li r0,0`: every camera goes to player 1's manager |
| 0x8005c1d0 (hook) | 0xa0830012 `lhz r4,0x12(r3)` | for callers in 0x80140000-0x80164000 (player/HUD code) with 2+ players, test player 1's manager, so everyone is held |
| 0x800e88a4 (hook) | 0x4bf73949 `bl 0x8005c1ec` | cinematic active (FUN_8005c1d0) or message mode: single view |
| 0x8024b584 (hook) | 0x4be10c69 `bl 0x8005c1ec` | same, full-screen layout |
| 0x800e8654 (hook) | 0x2c040000 `cmpwi r4,0` | run the HUD pass in message mode |
| 0x800e870c (hook) | 0x800314f0 `lwz r0,0x14f0(r3)` | only HUD 0 in message mode |
| 0x800e8014, 0x800e7324 | 0x4bf7c769, 0x4bf7d459 `bl 0x8006477c` | `li r3,1`: a skip-after-viewing cinematic counts as seen |
| 0x800e8090 | 0x38a00053 `li r5,0x53` | `li r5,0x43`: the pause button (Wii 2, Start on the Xbox layout) skips |
| 0x800e80a8 | 0x48163455 `bl 0x8024b4fc` | `li r3,0`: skipping allowed in multiplayer |

Message mode = pause state 4 or 6, 2+ players, world not the front end (0xdaea74a3). 0x800e8654 follows
`mfspr r0,LR`, so its body may use only r11/r12 (a version using r0 corrupted the saved LR and stalled every
boot at Arena Select).

Verified: opening cinematic full screen, split screen returns after; player 2 alone on a save pad gives camera
0x454 on manager 0 only and a full-screen prompt; a cinematic triggered by player 2 alone (Temple Grounds area
2, trigger 046f) played full screen and moved both players. The gunship save loop (ship spawn point ZERO put a
player who walked off back on the save trigger, endlessly) is gone with everyone held: one prompt only.

Skipping (game flow update, cinematic branch 0x800e7fc4-0x800e8108): a cinematic camera with +0x208 bit 0x800 is
always skippable, bit 0x8 only once seen (FUN_8006477c, system options). The skip special function (type 15,
handler 0x8020ece4) stores its uid at mgr+0x2bec on INCR and clears it on DECR; SkipCinematic is 0x8024a90c.
The skip also needs a hold timer (+0x28 of the flow object) at 2.0, mgr+0x3121 bit 0x8, the skip action, and
FUN_8024b4fc(mgr) == 0, which is IsMultiplayer (1 unless the mode is 'SNGL' or 'FRND'): skips were off in
versus. During a cinematic the pause press does nothing else (0x800e7f04). Hit counters on each step found the
multiplayer check as the last block. Verified: the opening of a new game skipped part-way through, both players
kept the start items (Missile 5/5); playtest confirmed. Player 2's Start: the press check reports any controller
and code K removed the controller-0 test; not tried separately.

### E. Scan Visor and no friendly fire

| Site | Original | Patched |
|---|---|---|
| 0x8014a0e8 | 0x4182000c `beq +0xc` | `b 0x8014a0f4`: allow scanning under MP |
| 0x80166a88 | 0x40820030 `bne` | nop: use the scan target search |
| 0x80148578 | 0x40820008 `bne` | nop: scan aim update for every player |
| 0x80166aac (hook) | 0x5460843e `rlwinm r0,r3,16,16,31` | search found nothing: nearest active actor with scan info (+0xbc -> +8) within 40 units and cos^2 >= 0.96 of the first-person camera aim (manager +0x14: eye = column 3, aim = column 1), players excluded; else the combat search at 0x80166ab8 |
| 0x80149ee8 (hook) | 0x48101615 `bl IsMultiplayer` | under MP: target (+0x39c) -> CastToActor -> vt+0x78 -> show its STRG with FUN_8024ac68; returns 1 |
| 0x8024abb4 (hook) | 0x2c030000 `cmpwi r3,0` | allow pause states 0, 1, 3, 4, 5, 6 under MP |
| 0x800ddc28 (inline) | 0x7c070000 `cmpw r7,r0` | in pause states 1 and 6 every controller goes to HUD 0; in state 4 only the controller that opened the Data Network |
| 0x802429ec (hook) | 0x9421ffa0 `stwu r1,-0x60(r1)` | entry of the damage builder: run it, then zero +4/+8/+0x10 when the damagee is a player and the damager or radius sender is a player or a player's weapon |

The damage hook sets LR to its own post code, runs the displaced `stwu` and falls into the function, so one
body covers both callers. Argument order: r5 damager, r6 radius sender, r7 damagee.

Verified: scan complete in 1.5 s; statue text pages and closes (player 2's A alone works), world paused for
both meanwhile. Player 2 scanned the Temple Grounds room-6 gate mechanism (both locks active); player 1 the Map
Station. ApplyDamage from a door 99 -> 89, from player 2 79 -> 79; player 2 firing at player 1 (1 HP) for 12 s:
unharmed (without the code it killed). Blocked between all pairs with 4 players.

### F. Enemies and bosses

| Site | Original | Patched |
|---|---|---|
| 0x80240be8 (hook) | 0x4bfffdad `bl 0x80240994` | for a CPatterned with 2+ players: swap the nearest player (squared distance of +0x50) into players[0] and states[0], update the stored target field, run Think, swap back |
| 0x80240d50 (hook) | 0x4e800421 `bctrl` | same around PreThink (the vtable call is redone because the far jump uses CTR) |
| 0x8024b3d0, 0x80244c70 (hooks) | 0x4e800421 `bctrl` | message delivery to a CPatterned: swap in the sender if a player, else the player who fired the sending weapon (+0x2c8), else the nearest player |

Stored target fields (see Enemies and AI) are rewritten only while they hold a player uid. The own type id is read
from the IsA function's `cmpwi` immediate and compared inline in Think only (9 CAST_ENTITY calls per enemy in
Think and PreThink had cost up to 25 harness calls per second in 16-enemy rooms).

Verified: Splinters next to player 2 with player 1 far: give-up timers 15.1 / 9.3 s without, 0.00 with.
Boost Guardian damage in 60 s (player 1 / player 2): none 263 / 10, Think 713 / 90, Think + PreThink 345 / 110.
Its +0x1098 stayed 0x400 (player 1) with player 2 alone in the arena; rewritten: player 1 alone 370/330/185 vs
player 2 alone 257/370/160. Blogg field -> 0x401 with player 2 near; Space Pirate damage A/B player 2 near
195/190, player 1 near 110/95. 16-enemy rooms back to 89-91 calls per second.

### G. Save stations

0x8020e83c: 0x408200a4 `bne` -> nop (FUN_8020e800 returned under MP before the refill and SetPauseState(5)).
Needs code E's pause gate. Verified: "Do you want to save?" full screen, save written, one A closes the
"saved" message (state 6), inventories unchanged; either player can save. Players have to step off the pad;
standing on an armed pad reopens the prompt.

### H. Pickups

0x801f8d78 (hook, orig 0x1c19000c `mulli r0,r25,12`, right after the grant; r23 pickup, r24 mgr, r25 item,
r30 toucher index): repeat the same grant for every other player's state (set variants when +0x1c8 bit 0x10).
Verified: Missile Expansion taken by player 1: both 33/40 -> 38/45, counter 0x2f 1/1 each; Energy Tank walked
into by player 2: both 1/1. Collected pickups stay collected (world layer state, saved).

### I. Shared scan progress

Hooks at 0x8014a474 (orig 0x48023f29), 0x80149e8c (0x48024511) and 0x8014a2d4 (0x480240c9), all
`bl SetScanTime`: call it for every player's state with the same id and fraction. Verified on the game's own
update path: player 2 scanning the statue set both players' entry to 0xff.

### J. Script upgrades

Hooks at the entries of FUN_8020f780, FUN_8020f838, FUN_8020f8d4, FUN_8020f9ac (orig 0x9421ffe0 each): run
the handler once per player with +0x170 = i (displaced `stwu`, then far jump to entry+4), restore +0x170.
0x8020322c (orig 0x807e1504): loop every player's state, continue at 0x8020324c. FUN_80065290 (MP-rules
bookkeeping) loops players itself. Verified with stand-in objects only: type 33 missiles +5 sent by player 2
-> both 50 -> 55; ZERO grant Light Beam -> both 0 -> 1.

### K. Map screen and Data Network

| Site | Original | Patched |
|---|---|---|
| 0x80156e88 | 0x41820034 | nop: HUD ctor always builds the map frame object |
| 0x80241854 | 0x40820058 | nop: map button with 2+ players |
| 0x8024188c | 0x40820020 | nop: map button from any controller |
| 0x80159598 | 0x418202a8 | nop: load the single-player HUD texture list (map icons) |
| 0x800e7f50 | 0x41820068 | `b 0x800e7fb8`: no per-player MP menu |
| 0x800e7fc0 | 0x4082032c | nop: any controller opens the Data Network |
| 0x80157b84 (hook) | 0x2c050000 | with 2+ players draw the map block only in pause state 1 (no corner minimap overlay) |
| 0x80157c14 (inline) | 0x4e800421 | skip the backdrop widget (a pale panel in the MP HUD frame) |
| 0x80157c84 (inline) | 0x38800006 | z-compare 7 (ALWAYS) instead of 6: the live world fills the depth buffer |
| 0x80157cbc (hook) | 0x48210579 | concat with (40, 1, 40, depth -17.479) instead of HUD+0x1e4 (MP anchors gave scale 0.35) |
| 0x801577fc (hook) | 0x9421fe80 | HUD draw entry: set 0x937f0010 when pause state != 0 with 2+ players |
| 0x803ec8d0 (hook) | 0x8122cf08 | GXSetZMode: compare ALWAYS while the flag is set |
| 0x80245504 (hook) | 0x9421fea0 | world draw entry: clear the flag, invalidate cache byte 0x8050d272 |
| 0x800e82d8 (hook) | 0x38800003 `li r4,3` | state 4 (Data Network); store the pressing controller (r1+0x5c) at 0x937f0014 |
| 0x800e8cf4 (hook) | 0x4bff4e11 `bl 0x800ddb04` | HUD_SET_MODE(4) on HUD 0 only |

The three depth hooks fix other players and near geometry drawing over paused HUD screens (text box, map):
paused frames still draw the live world pass first.

Verified: level map shows visited rooms, doors and Samus; A toggles World Map; B closes; player 2's button 1
opens it and player 2's A/B drive it. Text box clean with player 2 in view. Data Network: player 2's pause
button opens it full screen and player 2 browses and closes it. A playtest found player 1 could not control it
when player 2 was idle (the last input to arrive won); with the opener recorded, the pause button works for
both. The MP "Quit Game?" menu is no longer on a button; quitting is through the Data Network's Options.

### L. Shared lives

Pool (s32) at 0x937f0000, lives per player at +4 = Deathmatch Kill Limit / 5 (menu values 0, 5, 10, ...;
rules +0x3c, +0x40 bit 0x40). 0 = off (pool -1). Filled at session start and on a save as per-player x player
count (rules +0x38). Shown as item 0x40 amount/capacity on every player (the HUD's centre counters).

| Site | Original | Patched |
|---|---|---|
| 0x80076428 (hook) | 0x7fa4eb78 `mr r4,r29` | dead player's timer ran out: pool != 0 -> SpawnPlayer; pool 0 and someone alive -> 0x80076498 (stay down); nobody alive -> zero item 0x40, 0x800764c0 (match end, "DRAW GAME") |
| 0x80076454 (hook) | 0x9b370004 `stb r25,0x4(r23)` | after SpawnPlayer: pool - 1 (if > 0), update display |
| 0x8024ac68 (hook) | 0x9421ffd0 `stwu r1,-0x30(r1)` | show-message entry: refill the pool for the two "saved" messages only |
| 0x80076414 (hook) | 0xc0170000 `lfs f0,0(r23)` | every frame for a dead player: DisableFilter(0.5 s) on pass 1 of every alive player's viewport (pass+4 != 0); with pool 0 also the downed player's |
| 0x8024b844 (hook) | 0x57e0103a | pool 0, viewport's player down: set mgr+0x1da0/+0x1da4/+0x1da8 from the first player who is up (spectate) |
| 0x800dd9cc (hook) | 0x57e01838 | same, draw that player's HUD in the viewport |

The black half of a downed player is the HUD object covering the viewport, not a camera filter.

Verified: pool 4 with 2 players counts 4 -> 0; at 0 player 2 stays down and player 1's view stays lit; save
station No: pool 0, nothing saved; Yes: saved, pool 4, player 2 revived (-> 3); everyone down -> results screen
-> multiplayer menu; Sidehopper again resumes with pool 4. Kill Limit 15 -> 3 per player (pool 6); 0 -> three
deaths all respawned. With player 2 down: doors and world changes carry the body along, cutscenes and saving
work, and a resumed save has both players up.

### Difficulty

Getter FUN_800bc668(gs) returns `*(0x805cb220)+0x80` only for 'SNGL', else `li r3,1` at 0x800bc6a0, so
versus and co-op were always Veteran. 0 Normal, 1 Veteran, 2 Hypermode. Scales: FUN_800bc334 0.5 / 1.0 /
tweak+0x48 (1.53) for damage to Samus; FUN_800bc39c 1.3 / 1.0 / tweak+0x4c (0.5) for Samus's damage to
enemies (FUN_80292110 from FUN_802429ec). Tweak object `**(r13-0x6420)`.

Three codes, each writing `li r3,N` over 0x800bc6a0 (orig 0x38600001): Normal 0, Veteran 1, Hypermode 2.
Verified with Hypermode: getter 2; player 1's 10 against an enemy became 5.0 (damage, radius, knockback); an
enemy's 10 took player 1 from 99 to 83.7 (x1.53, applied in the full ApplyDamage path, not in FUN_802429ec).

## Dead ends and pitfalls

- NOPing the two RSET calls removed the intro overwrite but also skips story loadout changes; the raise-only
  version blocked the item loss. Vanilla RSET is right for co-op.
- A WorldTeleporter world change does not wipe inventory in this build (raise only, both rules). The 2019
  report of elevators resetting items is RSET at the arrival, or that mod's own loadout edits.
- Writing CPlayerState +0x10 does not change health; writing health 0 does not kill (death needs a damage
  event); own bombs do not hurt; writing the CPlayer transform does not move the player (use FUN_80160cc4).
- IsGameOver (vt+0x38) -> `li r3,0`: stops time-up (clock ran negative) but not the kill-limit end, and would
  block quitting.
- Teleporting all followers 2.5 units ahead along the door normal at the moment of crossing kept putting them
  in the way and could land them in geometry; replaced by the door-close placement.
- Scan text through ScanPause under MP froze both players behind an empty pane (the MP HUD has none); the
  message box is used instead.
- Scan Visor: the 0x80166a88 nop alone gave player 2 no candidate (+0x3c8 = 0xffff); an extra id-buffer
  readback after all viewports, or copying only the current viewport, did not fill player 2's buffer and
  emptied player 1's. The GPU copy timing is unresolved; the aim-based fallback covers it.
- Map: forcing pause state 1 without the frame object crashed (invalid write to 0x208 at 0x800e8bfc). Building
  the object gave a frame with a pale panel and no map until the three draw fixes (found by NOP-bisecting calls
  between 0x80157a08 and 0x80157c14). Forcing the full-screen view did not help.
- Silhouette over paused HUDs: disabling the text window's depth test (0x80157d20), returning early from the
  player's vt+0x2c, and 0x8015762c/0x80157618 changed nothing; material draws call GXSetZMode directly.
- Boss attention: Think-only swapping left player 2 far behind; message-only swapping did not change the A/B.
  The stored target fields were the real cause.
- `lis 0x804f; addi -0x8d18` is not 0x804e72e8 (16-bit signed immediate): use `lis 0x804e; addi 0x72e8`.
- RSET cannot be tested by starting a new game at another area: a default-spawn start uses FUN_8023f45c and
  never sends RSET. Temple Transport B/C elevators at such a start are blocked by Ing webbing and story layers.

## Test harness (not published)

- Python reads and writes Dolphin's emulated memory (the Dolphin Memory Engine method) to dump players,
  inventories, scan maps, script objects and connections.
- Game functions are called on the game thread through a hook at HasPowerUp (0x8016de90, runs every frame):
  it checks a flag and calls an address with up to 7 integer arguments and f1, returning r3. Used for the
  player teleport, ApplyDamage, SetScanningState, script messages to real or stand-in objects, the world
  teleporter with a fake object, the collision ray and SpawnPlayer. Its RAM must sit outside the code list
  (0x80003a00 up). It can run during enemy Think while code F has swapped players[0], so door tests walk
  instead of teleporting. Timed-out calls must be cancelled, or they run later on freed objects.
- Key presses to Dolphin (focused window) drive both Wii Remotes; Wii Remotes disconnect when idle, so each
  round starts with a press. Absolute IR gives repeatable turns in game; menus need relative IR. In the Trilogy
  aiming and firing depend on the pointer, so a stuck pointer looks like a dead fire button.
- Counter stubs around candidate call sites (pass-through to the original target) showed which spawn paths run.
- A world scanner teleported into 282 of the 284 rooms of the five main worlds (two hang on a direct jump) and
  recorded every enemy class; rooms create objects 6-9 s after arrival.
- Frame rate proxy: harness calls per second (about 90 max). A/B comparisons build the codes with one feature
  left out and boot fresh (live edits to MEM2 bodies are overwritten by OnFrame).

## Known issues and untested

- Only the opening hours were played by people; later areas were checked by the harness only. Story-gated
  enemies, devices and bosses past the opening (Chykka, Spider Guardian, Emperor Ing stages, Dark/Commando
  pirates) are untested, and stored-target fields of Jump Guardian, Quadraxis, Ing, Splitter and Rezbit were
  not checked in a fight.
- Bosses still pay player 2 somewhat less attention than player 1 in places (boss code outside Think).
- The Scan Visor id buffer is not filled for players 2-4; they rely on the aim fallback.
- Cutscenes and message boxes use player 1's view. Players 2-4 can fall during an elevator arrival cinematic
  (invisible; the script then places both at the exit). Map opens in player 1's half, rooms slightly off centre.
- Code J tested only with stand-in objects; a map station opening the map (type 5 with code K) not separately
  tested; special function 37 (pause state 2) stays blocked.
- NTSC-U Trilogy only. PAL, Japanese and GameCube builds have other addresses; the gate keeps the codes inert
  there. Dolphin NetPlay untested.
