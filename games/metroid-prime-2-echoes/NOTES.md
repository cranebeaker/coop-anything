# Metroid Prime 2: Echoes (Trilogy) co-op: notes

Raw research and session log behind [README.md](README.md), kept for the next agent. Addresses are
for `rs5mp2_p.dol` SHA-1 `2375606f4e9429a699cfa02728b0bb1176421226` (NTSC-U Trilogy `R3ME01` rev 0).
`scripts/` means the author's local test scripts (not published; the code generator `coop_patches.py` and `ppc.py` are in the project repo's `tools/`) and `<workspace>` the local work folder. Part 1 is
the triage from 2026-10-21; part 2 is the reverse-engineering log, session by session.

# Part 1: triage


Draft for `games/metroid-prime-trilogy/NOTES.md` in coop-anything. Phase 0 (Triage) and Phase 1 (Research), first pass. Researched on 2026-10-21.

## 1. Game identity
- Title: Metroid Prime Trilogy (Wii, 2009). This is the compilation, not the GameCube originals.
- Game ID: `R3ME01` (NTSC-U), disc revision 0, read from `DATA/sys/boot.bin`.
- Form: an extracted disc filesystem (`DATA/files/{fe,MP1,MP2,MP3}`). There is no ISO, so no disc hash.
- Emulator: PrimeHack (shiiion's Dolphin fork). `Dolphin.exe` reports `Dolphin 5.0-12188`, and the installer is dated 2021-10. The latest upstream release is v1.0.9, so this install is older.
- OS: Windows 10 Home 22H2 (10.0.19045).

## 2. Existing work
- coop-anything DIRECTORY.md has no Metroid entry. A repo search for "metroid" finds 0 issues or PRs.
- `playbook/console-emulators.md` says no Metroid Prime co-op mod was found and lists PrimeHack as a controls-only fork.
- Web search found no co-op campaign mod for MP1, MP2 or MP3 in Dolphin. "Metroid Prime: Multiplayer [FANMADE]" on itch.io is a separate Unreal Engine fan game, not a mod of the disc.
- Randovania multiworld (https://github.com/randovania/randovania): each player runs their own game, and picking up an item can send it into the other player's game. It connects to Dolphin through Dolphin Memory Engine. It needs the GameCube MP1 or MP2 ISOs (NTSC-U or PAL). The Trilogy is not listed as supported. MP3 Corruption support is experimental.
- Dolphin NetPlay is built in and works by syncing inputs. Every player needs an identical dump and an identical emulator build.

## 3. Engine
- Retro Studios' proprietary engine, PowerPC (Wii, big-endian).
- MP1 GameCube has a community decompilation (PrimeDecomp). Its matching status and version coverage for the Trilogy have not been checked yet. MP2 and MP3 decompilations: unknown.

## 4. Source situation
- Closed binary for the Trilogy build. The MP1 GameCube decompilation might help with understanding the engine, but it is a different build.

## 5. Built-in multiplayer
- The Trilogy keeps MP2 Echoes' local 2–4 player split-screen **versus** mode (deathmatch and bounty). There is no co-op campaign. Sources: Wikipedia and Nintendo World Report reviews.
- Echoes' engine can already run several Samus actors with split-screen cameras. That makes it the strongest base for a future split-screen co-op campaign mod. That work would be Path 2 (build from zero) and would need heavy reverse engineering.
- Online play today: Echoes versus over Dolphin NetPlay. Not tested yet.
- PrimeHack issue #113 (closed): changing FOV causes culling bugs in MP2 multiplayer. A PrimeHack collaborator (SirMangler, 2022-08) says multiplayer is not supported. PrimeHack only talks to Player 1, so Player 2's controls don't work, and NetPlay is "entirely broken" under PrimeHack.
- A Dolphin forum thread (title: "primehack trilogy multiplayer controller confusion") reports the same thing: Wii Remotes 2–4 disconnect when Echoes loads, and one controller drives all four players.
- Plan for local split-screen: turn PrimeHack off or use mainline Dolphin, then give each player an emulated Wii Remote + Nunchuk with IR mapped to a stick or the mouse. Not tested yet.
- Two arenas are unlocked by single-player progress (Pipeline: Torvus Temple; Spires: Sanctuary Temple). This comes from GameCube guides and is assumed to carry over to the Trilogy.

## 6. Shipping check
- A mod can ship as Gecko/AR codes, a memory-bridge tool, or a patcher, with no game files included.
- No online services or anti-cheat are involved.

## Prior art: Echoes co-op campaign built on versus mode (found 2026-10-21)
- **SubsonicBeatz154, "Metroid Prime 2 (Trilogy) Co-op Mod".** Thread: https://forum.metroidconstruction.com/index.php?topic=4942.0. Read through an archive.org capture from 2025-08 because the live site sits behind a bot check.
  - Started 2019-04, targeting NTSC-U Trilogy (the same build as ours). Built with asset edits only: Prime World Editor PAK edits, the extracted game booted via main.dol. No code changes. Abandoned 2019-07 ("no longer being developed", topic 4368). No public download was found.
  - Got to the first U-Mos encounter. Tested 4 players in demanding rooms with no crash (Wii has more RAM). Sanctuary Dark Samus and Emperor Ing fights completed.
  - Asset workarounds: triggers in place of scans; FlagsCinematicCamera = 170 (or 670 for slow-motion cameras) so cutscenes fill the screen, which only works when P1 or the game triggers the cutscene; extra spawn points so players don't stack; FirstSpawn spawn points used to apply item loss.
  - Blockers, all needing code changes:
    1. A player dying jumps to the MP results screen.
    2. Save stations don't work.
    3. Room loading follows P1 only; P2 gets stranded in unloaded rooms.
    4. **Trilogy-specific:** WorldTeleporter (elevator or portal) overwrites every player's inventory with the destination spawn point's loadout. This made the game unplayable past the intro.
    5. Scan Visor does nothing.
    6. Seeker locks max out at 3.
    7. Inventories are separate.
    8. Friendly fire is on.
    9. Most enemies only target P1.
    10. Cutscene cameras break when P2 triggers them.
- **xjonx, GameCube MP2 and the E3 demo, around 2018.** Video: https://youtu.be/PXVwoBMD6k0. Cancelled. On GC it hit the multiplayer memory limit and crashed; IngSnatcherSwarm and spawner objects crashed multiplayer. The GC version does not reset items on world teleport.
- Aruki (PWE author), 2019: most blockers need code changes. PrimeAPI only had RE infrastructure for MP1 at the time.
- PrimeDecomp/echoes (a GC MP2 decompilation, symbols for G2ME01/G2MP01): GitHub returns 404 as of 2026-10-21. Status unknown.

## Proposed design (Path 1 revive + Path 2 code work)
- Base: Echoes versus mode with two players, loading campaign worlds (Subsonic's asset approach, rebuilt), plus code fixes for the blockers.
- Prototype each fix as a host-side Python bridge over Dolphin Memory Engine first, then port it to Gecko codes or a DOL patch to ship.
  1. Inventory: snapshot both CPlayerStates before a WorldTeleporter fires and restore them after; then share pickups between players.
  2. Room streaming: "leash" P2 to P1 on area change (teleport, Halo-style); later, load both players' areas.
  3. Death: respawn the dead player next to their partner instead of ending the match.
  4. Saves: Dolphin save states for now; real saves later.
  5. Friendly fire off; enemy target selection; Scan Visor; seeker count.
- Executables to hash-pin: rs5mp2_p.dol SHA-1 2375606F4E9429A699CFA02728B0BB1176421226; main.dol/rs5fe_p.dol SHA-1 D55B890C4E7C2CC6F2E120EC4EF988C186780308.

## Risks and unknowns
- PrimeHack's mouse camera writes to emulated memory locally. Under input-sync NetPlay this would probably desync. Not tested. Consider controller-only play, or mainline Dolphin for NetPlay.
- A 2015 Dolphin forum report says MP1 had netplay problems with deterministic dual-core. That report is old and not checked against current builds.
- Emulated Wiimote + Nunchuk netplay has a history of desync reports. Current status is unknown.

## Difficulty path
- Play together today: Echoes versus over NetPlay (not co-op), or Randovania multiworld (needs the GameCube discs).
- True shared-world co-op campaign: Path 2, console with no PC version. Use a memory-sync bridge (Dolphin Memory Engine) or extend Echoes' split-screen engine. Hard.


# Part 2: reverse-engineering log

# Echoes (Trilogy R3ME01) player state and inventory: RE log

Executable: `rs5mp2_p.dol`, SHA-1 2375606f4e9429a699cfa02728b0bb1176421226. The Trilogy front end loads it when Echoes starts.
Sessions: 2026-10-21, Dolphin 2609a (portable, **single-core**), working copy `<game dir>`.

Confidence labels:
- **live-verified**: read and/or written in a running game, with the effect seen on screen.
- **static**: Ghidra decompilation only.
- **PrimeHack**: from shiiion/dolphin `AddressDBInit.cpp` (GPL), not yet re-derived here.

## Globals
| Address | What | Evidence |
|---|---|---|
| 0x804e72e8 | `CStateManager` (static object, in .bss) | PrimeHack; live-verified (player pointers resolve in both modes) |
| r13 = 0x805d1660, r2 = 0x805d4280 | SDA bases | static (`__init_registers`; = .sdata/.sdata2 start + 0x8000) |

## CStateManager
| Offset | Type | What | Evidence |
|---|---|---|---|
| +0x14f4 | `CPlayer*[4]` | players, indexed by player number | static (≈30 functions read `sm + idx*4 + 0x14f4`); live-verified: SP has [0] only; 2-player versus has [0] and [1], [2..3] = null |
| +0x810 | object list | | PrimeHack |
| +0x1514 | camera manager (ptr, +0x10) | | PrimeHack |
| +0x153c | load state | | PrimeHack |
| +0x1e44 | area id | | PrimeHack (read 0 in both tests; unconfirmed) |
| +0x2c0c | menu state | | PrimeHack |

## CPlayer
| Offset | What | Evidence |
|---|---|---|
| +0x12ec | `CPlayerState*` (one per player) | PrimeHack; static (230 uses); live-verified (separate states for P1/P2) |
| +0x2c4 move state, +0x374 camera state, +0x378 morph state, +0x390 orbit state, +0x5f0 first-person pitch | | PrimeHack |

## CPlayerState (constructor FUN_8016d008(this, playerIndex, persistentData); size ≥ 0x5b0)
| Offset | Type | What | Evidence |
|---|---|---|---|
| +0x00 | u32 | player index | static (ctor stores arg 2); live-verified (0 / 1) |
| +0x04 | u8 flags | bits 0x80/0x40 set in ctor | static |
| +0x10 | f32 | 99.0 at spawn. **Not** live health: writing it doesn't change the HUD | live-verified |
| +0x14 | f32 | **current health** | live-verified (P2 set to 50 → only P2's HUD shows 50); static (refill helper FUN_8016db00 adds here) |
| +0x18 | f32 | 50.0, probably knockback resistance | live (value only) |
| +0x34 | u32 | current visor (0 Combat) | PrimeHack; static (setter FUN_8016e000) |
| +0x38/+0x3c/+0x40 | vector<u16> {size, capacity=0x20, data*} | sorted u16 id list, maybe scan/log entries | static (FUN_8016ebd4 / FUN_8016ecf0 / FUN_8016f17c) |
| +0x50 | u32 | current suit (0 Varia, 1 Dark, 2 Light), recomputed when ids 12–14 change | static (FUN_8016d998) |
| +0x54 | u32 = 0x6d | powerup count (109) | static (ctor); live-verified |
| +0x58 + id*0xc | `{s32 amount; s32 capacity; u32 extra}` | powerups[109] | static; live-verified (matches HUD; writing Missile amount 5→3 changed the HUD to 03/05) |
| +0x574.. | | timers/arrays (+0x57c float helper; +0x588–0x598 copied from persistent data; +0x594 vector of 8-byte entries; +0x5a0 count + 3-word entries) | static, unidentified |

### Powerup accessors (all bounds-check id < 0x6d)
| Function | Does | Likely name |
|---|---|---|
| FUN_8016d998(ps, id, n) | capacity += n, clamped to max table @0x804a4f20[id]; amount = min(amount, capacity); suit update for ids 12–14; sfx for id 0x61 | InitializePowerUp |
| FUN_8016d984(ps, id, n) | capacity = 0, then the above | ReInitializePowerUp |
| FUN_8016db60(ps, id, n) | amount += n, clamped to capacity; id 41 → health refill; id 42 → also refills health | IncrPickup |
| FUN_8016dc10(ps, id, n) | amount -= n (floor 0) unless FUN_8016dc98 says "infinite" | DecrPickup |
| FUN_8016de44(ps, id, n) | amount = n; capacity raised to n if lower | ResetAndIncrPickUp / SetPickup |
| FUN_8016de70 / FUN_8016debc(ps, id) | return capacity | GetItemCapacity |
| FUN_8016de90(ps, id) | capacity > 0 | HasPowerUp |
| FUN_8016db00(ps, f) | health(+0x14) += f, clamped | refill health (body uses paired-single ops) |

### Powerup ids
From Randovania `prime2/logic_database/header.json`, which matches what the live data shows:
- **Beams:** 0 Power, 1 Dark, 2 Light, 3 Annihilator, 22 Charge
- **Beam combos:** 4 Super Missile, 5 Darkburst, 6 Sunburst, 7 Sonic Boom
- **Visors:** 8 Combat, 9 Scan, 10 Dark, 11 Echo
- **Suits:** 12 Varia, 13 Dark, 14 Light
- **Morph Ball and upgrades:** 15 Morph Ball, 16 Boost, 17 Spider, 18 Bombs
- **Movement:** 23 Grapple, 24 Space Jump, 25 Gravity Boost, 27 Screw Attack
- **Launchers:** 26 Seeker
- **Keys:** 29–31 Sky Temple Keys 1–3, 32–40 Dark Temple keys (Agon/Torvus/Hive 1–3), 101–106 Sky Temple Keys 4–9
- **Health and ammo:** 41 Health refill, 42 Energy Tank, 43 Power Bomb, 44 Missile, 45 Dark Ammo, 46 Light Ammo
- **Other:** 47 Item %, 97–100 Translators (Violet/Amber/Emerald/Cobalt)
- **Unknown:** ids 19–21 (capacity 1 in versus mode, amount 0) and 80 (capacity 9999 in versus mode)

## Observations that matter for co-op
- **Inventory overwrite at spawn is visible.** During the opening cinematic, P1 had a different loadout (Super Missile, 10 missiles). Once gameplay started at the Landing Site, it was 5 missiles and Spider Ball. The spawn point's loadout replaced the state, which is the same mechanism behind the Trilogy elevator/portal inventory reset. Next: put a write breakpoint on the powerup array to find the spawn/teleport code that does this.
- **Versus mode** (Sidehopper Station, 2P): both CPlayerStates get the MP loadout (every beam ammo at capacity 50, Dark Visor, Grapple, Gravity Boost, ids 19–21, id 80 = 9999).
- PrimeHack's invulnerability patch site (0x8014bacc = `bne +0xc`) matches this DOL byte for byte, so PrimeHack's PRIME_2 NTSC table applies to this build.

## Tooling (<workspace>/re)
- `scripts/dolinfo.py`, `scripts/dolflat.py`: DOL section map, and a flat image with base 0x80004000.
- Ghidra project `ghidra/mp2`: flat image, PowerPC:BE:32:default. SDA registers set by `Mp2Prep.java`. Paired-single code doesn't decompile, because Ghidra has no Gekko language.
- `scripts/Mp2Query.java` + `scripts/ghq.sh`: headless queries (decomp / grep / pstate). Pass regex arguments through `ghq.sh`, because analyzeHeadless.bat mangles them.
- `scripts/dolmem.py`: stdlib-only Dolphin memory read/write (Dolphin Memory Engine method). `scripts/mp2probe.py` dumps players and inventory.
- `scripts/dolctl.py`: launch / screenshot / scancode key presses for the portable Dolphin. **It focuses the Dolphin window before each key press.**
- Portable Dolphin input:
  - Wii Remote 1 on keyboard: J=A, L=B, P=+, O=−, H=Home, arrows = IR (relative), WASD = stick, C/Z.
  - Wii Remote 2: NUM5=A, NUM0=B, NUM9=+, IJKU... = IR, TFGR = stick.
- Save states:
  - slot 1 = Echoes SP Landing Site, start of gameplay
  - slot 2 = 2-player deathmatch, Sidehopper Station
- **Dual-core hits a GPU FIFO desync in the Echoes intro.** Keep CPUThread = False.

## 2026-10-21 (session 2): inventory mirror prototype + the spawn-point overwrite

### More CStateManager fields (static, and live-verified in 2P versus)
| Offset | What |
|---|---|
| +0x14f0 | u32 player count (1 in single-player, 2 in 2P versus) |
| +0x1504 | `CPlayerState*[4]`, a direct per-player pointer list. Same values as players[i]->+0x12ec |
| +0x1e44 | current area id (compared with spawn point +0x04 area in the RSET handler) |
| +0x1dac | world object (+0x2c: area array, 8 bytes per area) |

Global `DAT_805cb314` (GameGlobalObjects-like) +0x148 = **game rules object**. Its vtable slot +0x1c is SpawnPlayer(mgr, idx):
- Single-player rules: vtable 0x804b9648, SpawnPlayer = FUN_80230238, which is an empty `blr`.
- Multiplayer rules: vtable 0x804b4838 (live object 0x807bd740 in versus), SpawnPlayer = FUN_8010cd94. Wrapper FUN_800654e4 is at vtable 0x804b4560.
  - FUN_8010cd94 rebuilds the player's CPlayerState from scratch: it constructs a new state with FUN_8016d008 and copies it over the live one, keeping only a few persistent blocks via FUN_8016e770/FUN_8016e88c.
  - Then it applies the spawn point's loadout to every one of the 109 slots (capacity via FUN_8016d998, amount via FUN_8016db60), moves the player to the spawn point and sends `SPWN`.
  - It is called through script object vtable 0x804b8938 slot +0x160 → FUN_8020f510 (on `ACTN` message, player index at obj+0x170).
- Respawn-at-spawn-point FUN_8023f45c (3 calls from FUN_8023f6c0) only *raises* the state to the spawn point's loadout: it calls InitializePowerUp/IncrPickup when below. It is not an overwrite.

### CScriptSpawnPoint (vtable entry 0x804b8950 → AcceptScriptMsg FUN_8020c5f4)
- Spawn point object: +0x04 area id, +0x1c flags (bit 31 = "first spawn"/active), +0x20 transform, +0x54 + id*4 = loadout **amount**[109], +0x20c + id*4 = loadout **capacity** override[109], +0x3c0 bit 6 = morphed flag.
  - Getters: FUN_8020c594 (amount), FUN_8020c5bc (capacity = max(amount, capacity override)).
- **On `RSET` (0x52534554)** it loops over *every* player (i < sm+0x14f0, state = sm+0x1504[i]) and *every* powerup id:
  - `0x8020c65c bl FUN_8016d984` (capacity := spawn point capacity)
  - `0x8020c67c bl FUN_8016daec` (amount := 0, then += spawn point amount)
  - This wipes and replaces every player's whole inventory. Then, if flag bit 31 is set, it teleports every player to the spawn point (FUN_80160cc4), loading the area first if needed.
- On `ZERO` (0x5a45524f) it only teleports (no inventory change).
- No code in the DOL sends `RSET` to a spawn point (the only code-side RSET sender, FUN_80229bbc, targets CONN-linked objects). So RSET comes from **level script connections** (MREA/SCLY data): story triggers, elevators/portals, and so on. This matches the 2019 report: "Whenever a WorldTeleporter is triggered, it will always override the players inventories with whatever the spawn-point on the other end has set."

### Live test: NOP the two RSET powerup calls → the intro overwrite disappears (verified)
- Unpatched run: during the opening, P1 = Power Beam, Super Missile, Combat/Scan Visor, Varia, Morph Ball, Boost, Bombs, Charge, Space Jump, Missile 10/10. At first control at the Landing Site it was reset to Missile 5/5, Super Missile gone, Spider Ball added.
- Patched run (`scripts/patch_rset.py` NOPs 0x8020c65c/0x8020c67c as soon as rs5mp2 is resident, i.e. before Echoes boots): at first control at the Landing Site P1 still has **Missile 10/10 and Super Missile**, no Spider Ball. HUD shows 10/10. State saved as slot 3.
- Conclusion: **the spawn point RSET handler is the inventory overwrite.** For co-op, patch these two calls, ideally replacing them with "raise only" (like FUN_8023f45c) instead of NOP. Keep the teleport part, so everyone still gets moved to the new spawn.
- Caveat: a full NOP also skips intended story loadout changes that rely on RSET. For example, the item loss in Echoes' opening hours may use the same path. Not tested yet. Elevators/portals are not tested live yet (needs a save past the intro).

### Inventory mirror prototype (`scripts/mirror_inventory.py`): works in 2P versus
- Polls at 60 Hz. When P1's 109-slot powerup block (or current suit) differs from P2's, it copies P1 → P2 in one write. Health and visor stay per-player.
- Live results (slot 2 save state, Sidehopper Station):
  - Injected Dark Beam + Dark Suit + Missile 20/50 into P1. P2 matched on the next tick: 1 write in 2337 ticks / 40 s.
  - P1 fired two missiles in game. Each was mirrored within a tick (20→19→18). P2's HUD followed.
- It is one-way by design (P1 wins), so P2's own ammo use and pickups get overwritten. A real shared inventory needs per-slot merge rules: max for capacities and one-off items, summed deltas for ammo.

### Respawn test: inconclusive
- Writing health = 0 does not kill a player. Death needs a damage event.
- P2's Wii Remote A (NUMPAD5) doesn't fire in game, though menus accepted it. P1's fire works. Probably a key mapping or focus issue for numpad scancodes. Still to fix.
- Moving P2 by writing the CPlayer transform (+0x20, translation at +0x2c/+0x3c/+0x4c) did not move it visibly. The physics/collision position is elsewhere (PrimeHack's noclip uses the same offsets but also patches Teleport).
- A bomb suicide at 1 HP didn't kill P1, because players don't take damage from their own bombs.

### Tooling added
- `dolctl.py launch <slot>` boots straight into a save state.
  - slot 1 = SP Landing Site, unpatched
  - slot 2 = 2P versus
  - slot 3 = SP Landing Site with RSET patch applied (patch is in RAM, so it is included in the state)
- `Mp2Query.java`:
  - `xrefs <addr>...`
  - `decomp` now creates functions that the prologue scan missed
  - `@file` argument passing (use `ghq.sh`)

## 2026-10-21 (session 3): raise-only RSET patch, plus the elevator attempt

### Raise-only patch (`scripts/patch_rset_raise.py`, `scripts/ppc.py`): verified at the intro handover
- Two 7-instruction helpers in a low-memory cave at **0x80002800**. 0x80001800 is not free: Dolphin puts its `STUBHAXX` stub at 0x80001804. Both helpers take the same args as the calls they replace (r3=state, r4=id, r5=value):
  - RaiseCap: `mulli r6,r4,12; add r6,r6,r3; lwz r7,0x5c(r6); cmpw r7,r5; bgelr; subf r5,r7,r5; b InitializePowerUp(0x8016d998)`
  - RaiseAmt: same, but on +0x58, with a tail call to IncrPickup(0x8016db60)
- The RSET call sites are redirected: 0x8020c65c → `bl RaiseCap`, 0x8020c67c → `bl RaiseAmt`. The encodings were cross-checked against existing DOL instructions (mulli/add/lwz/bl match byte for byte).
- Live (new game, Normal, patched from the moment rs5mp2 loads):
  - In the intro: Super Missile, Missile 10/10, no Spider Ball.
  - At first control (the RSET moment): Super Missile and **Missile 10/10 kept**, and **Spider Ball added** from the spawn loadout.
  - HUD 10/10. So: nothing removed, new items granted. That's the intended co-op behaviour. Saved as slot 5.
  - Compare with vanilla (5/5, Super Missile removed) and the NOP patch (10/10, no Spider Ball).

### Test aid: change the new-game start area
- FUN_800ba8ac (new CGameState) hardcodes world 0x3bfa3eff (Temple Grounds) and area 0x62b0d67d (Landing Site) at 0x800ba8e0–0x800ba900. `patch_rset_raise.py --start-area 0xMREA` rewrites the `lis/addi r4` pair, and the game starts at that area's default spawn point. MREA ids come from Randovania `prime2/logic_database/<World>.txt`.
- Tested: Temple Transport C (0xaded752e) → area index 0x09; Temple Transport B (0x4cc37f4a) → area index 0x20. Both started at the elevator node, with the generic new-game loadout:
  - Power/Charge Beam, Super Missile
  - all 4 visors
  - Varia Suit
  - Morph Ball, Boost, Spider, Bombs
  - Space Jump, Gravity Boost, Seeker Launcher
  - ids 19–21
  - Missile/Dark/Light ammo 5/5
- Slot 6 = Temple Transport C, slot 7 = Temple Transport B. Both are vanilla RSET, with Dark Beam + Light Beam + Missile 40/40 injected as markers.

### Elevator test: NOT done
- In both transport rooms the elevator could not be started. The room is full of Ing webbing that Power Beam shots don't clear. Scanning the red console (critical scan, turns green) did nothing visible. No elevator pad or hologram was found in a full 360° Scan Visor sweep. These elevators are probably inactive or blocked at this point in a new game (story layers), so this start-area shortcut doesn't give a usable elevator.
- Options for next time:
  1. Play (or script) the real route to the first elevator.
  2. Find an elevator room whose default layer is active at game start.
  3. Trigger the arrival directly: get the destination spawn point (e.g. Great Temple / Temple Transport B, MREA 0x8f01b104) sent `RSET` by starting a new game there (`--start-area 0x8f01b104`). This exercises the same handler the elevator arrival uses, but not the WorldTeleporter itself.

### Input notes
- The Trilogy uses the Wii pointer for camera turning and lock-on targeting.
  - **Absolute** IR (`irmode.py abs`, restart Dolphin) gives repeatable turns: a 500 ms LEFT/RIGHT ≈ 1/7 turn, and the pointer re-centres on release. Use it in game.
  - Menus need **relative** IR (`irmode.py rel`).
  - Dolphin rewrites WiimoteNew.ini on exit and drops default-valued keys, so `irmode.py` re-inserts the key. Default is relative.
- Scan: put the target at the screen centre in Scan Visor (set via `CPlayerState+0x34 = 2`) and hold Z (~4 s). Pressing A drops back to Combat Visor.
- Save-state slots now:
  - 1: SP Landing Site, vanilla
  - 2: 2P versus
  - 3: SP Landing Site, NOP patch
  - 4: Trilogy Single Player menu, before Echoes loads; use it to re-run patchers from boot
  - 5: SP Landing Site, raise-only patch
  - 6: Temple Transport C
  - 7: Temple Transport B

## 2026-10-21 (session 4): Great Temple arrival spawn, and which spawn path actually runs

### New tools
- `patch_rset_raise.py --start-world 0xMLVL`: rewrites `lis r31` @0x800ba8e0 and both `addi r4,r31` @0x800ba8e8/0x800ba8f4. World ids come from Randovania `<World>.json` extra.asset_id: Temple Grounds 0x3bfa3eff, Great Temple 0x863fcd72.
- `patch_rset_raise.py --inject`: writes Dark Beam 1/1 + Missile 40/40 into P1's CPlayerState while it is still empty (before any spawn code runs). These are the markers for "did existing inventory survive".
- `count_spawn_paths.py`: wraps each candidate path with a 5-instruction counter stub (stubs 0x80002900, counters 0x80002a00) and passes through to the original target, so behaviour is unchanged:
  - rset_cap/rset_amt (0x8020c65c/0x8020c67c)
  - respawn (3 bl → FUN_8023f45c)
  - mp_spawn (vtable slots 0x804b4854/0x804b5984 → FUN_8010cd94)

  Start it after `patch_rset_raise.py` reports "resident"; it wraps whatever the RSET sites currently call.

### Results (all single-player, Normal, new game from slot 4, markers injected)
| Run | Path counters after first control | Markers (Dark Beam, Missile 40/40) | Spawn loadout |
|---|---|---|---|
| Great Temple / Temple Transport B (0x8f01b104), **vanilla** | respawn=1, rset=0/0, mp_spawn=0 | kept | added (raise) |
| Great Temple / Temple Transport B, raise-only | (same start; RSET never fires there) | kept | added |
| Landing Site, **vanilla** | respawn=1 at load, then **rset=109/109** at first control | **wiped** (Dark Beam gone, Missile 40/40 → 5/5) | replaced |
| Landing Site, **raise-only** | respawn=1, **rset=109/109 via the raise helpers** | **kept** (Dark Beam, 40/40; HUD 40/40) | added (Spider Ball) |

- Starting a new game at an area's default spawn point does not send RSET. The initial spawn uses FUN_8023f45c (raise-to-loadout, called from FUN_8023f6c0), which only ever adds. So the Great Temple start cannot exercise the RSET path, vanilla or patched. It was saved as slot 8 (Great Temple Transport B, raise patch + markers).
- RSET is sent by level scripting, e.g. the Landing Site's intro cinematic end at first control. When it fires, it touches all 109 slots for each player. The raise-only replacement is now verified on a real script-driven RSET: 109+109 calls, nothing removed, new loadout items granted.
- Slot 9 was not saved: Shift+F9 isn't a default Dolphin hotkey. Slots 1–8 only.

### What this means for the elevator question
- In **single-player rules**, an elevator ride loads the new area and spawns via the same paths: respawn/raise at load, plus RSET only if that level's scripts send one. The 2019 report says arrival spawn points overwrite inventory. If that happens it would be via RSET, which the raise-only patch now handles. Still unconfirmed on a real elevator.
- In **multiplayer rules** (the 2019 mod's base, and the plan for co-op), a world change also goes through the rules object's SpawnPlayer = **FUN_8010cd94**. It constructs a brand-new CPlayerState and applies the spawn loadout, i.e. a full wipe that has nothing to do with RSET. That is the most likely cause of "elevator/portal resets everyone's items" in the Trilogy co-op mod, and the next thing to patch: keep the old powerups[] block across the rebuild, then raise to the loadout. Under the MP rules, mp_spawn has not been counted live yet. Next test: run count_spawn_paths.py from the MP front end into a 2P match and respawn.

## 2026-10-21 (session 5): multiplayer SpawnPlayer (FUN_8010cd94), keep-and-raise patch, verified live

### Which path runs in 2P versus (count_spawn_paths.py, fresh match from the Trilogy main menu)
- Match start: `respawn=2` (FUN_8023f45c, once per player; it only raises), `mp_spawn=0`, rset=0. So FUN_8010cd94 does not run at match start.
- Whether a death/respawn in versus uses FUN_8010cd94 is still unknown. A kill couldn't be set up: P2's fire button doesn't work in game, and position writes don't move players.

### Forcing the call: one-shot hook (`mp_spawn_patch.py --trigger N`)
- The HasPowerUp entry (0x8016de90, `cmplwi r4,0x6c`, called every frame by the HUD) is redirected to a 31-word cave at 0x80002b00. When the flag at 0x80002bf0 is non-zero, the cave calls `rules->SpawnPlayer(&g_StateManager, flag-1)` once:
  - rules = `*(*(0x805cb314)+0x148)`
  - vtable slot +0x1c
  - done flag at 0x80002bf8

  Then it runs the original instruction and branches back.
  - Bug caught in review before running: `lis 0x804f; addi -0x8d18` doesn't fit a 16-bit signed immediate. 0x804e72e8 = `lis 0x804e; addi 0x72e8`.
- **JIT flush:** Dolphin doesn't see external writes into code it has already compiled. Save + load of a state clears the cache.
  - **Gotcha:** Shift+Fn ("save slot n") also fires the plain Fn "load slot n" hotkey. With an existing slot file, the old state was loaded over the live game, which silently discarded every external write.
  - All earlier slot saves were to empty slots, so they were fine.
  - `mp_spawn_patch.flush()` now deletes the slot file, saves, waits for the file size to settle, then loads.
  - Verified: a P2 health write of 42.0 and a low-memory word both survive the round trip.
  - Slot 3 is the scratch slot for this (the old NOP-patch state is gone).
- Incident: the first count_spawn_paths.py run matched its single readiness word (0x8014bacc) while the **front end** was loaded. It wrote two `bl`s and its stubs into front-end memory before an assert stopped it. The MP DOL load overwrote that code. I verified the rs5mp2 words were original and zeroed the stub area. Readiness now requires all 8 instrumented words to match rs5mp2.

### Results (P2 markers: Dark Beam, Light Beam, Dark Suit + suit field 1, Missile 30/50)
| Run | After SpawnPlayer(P2) |
|---|---|
| **Vanilla** (fresh MP match) | P2 teleported to another spawn point; **all 109 slots zero** (no Power Beam either), suit field 0. HUD missile counter gone. Full wipe confirmed. |
| **Keep-and-raise** (slot 2 match, patch applied) | Inventory unchanged: markers, MP loadout, suit 1, Missile 30/50 (HUD 30). Re-fired 3×: P2 moved spawn points twice ((64,-100) → (19.6,0.4) → (-64,-101)), with the inventory kept every time. P1 untouched. |

### The patch (4 words in FUN_8010cd94, all original words asserted before writing)
| Address | Original | Patched | Effect |
|---|---|---|---|
| 0x8010d01c | `stw r0,0x50(r29)` 901d0050 | `nop` 60000000 | keep the current suit field |
| 0x8010d020 | `beq 0x8010d078` 41820058 | `b 0x8010d078` 48000058 | skip copying the fresh state's powerup block over the live one |
| 0x8010d2d0 | `beq 0x8010d300` 41820030 | `bge` 40800030 | capacity: only InitializePowerUp(loadout − cur) when cur < loadout |
| 0x8010d324 | `beq 0x8010d358` 41820034 | `bge` 40800034 | amount: only IncrPickup(loadout − cur) when cur < loadout |

- The rest of the rebuild still runs: health refills to full (99), the visor resets, the teleport and `SPWN` happen, and the persistent block goes through FUN_8016e770/FUN_8016e88c.
- Not exercised: the "raise" half on this path. The versus spawn points' loadouts are empty, so no slot needed raising. The same compare-and-call code is proven by the RSET raise test.

### Status of inventory-overwrite paths for co-op
1. Spawn point RSET (FUN_8020c5f4): **raise-only**, verified on a real script-driven RSET.
2. Initial/respawn FUN_8023f45c: already raise-only in vanilla.
3. MP rules SpawnPlayer FUN_8010cd94: **keep-and-raise**, verified via a forced call.
Still to do:
- Confirm which path a real elevator/portal ride takes under MP rules (needs the campaign world loaded in MP mode, as in the 2019 mod).
- Fold all three into one Gecko code / patcher that applies at rs5mp2 load.

## 2026-10-21 (session 6): combined patch and Gecko code, tested end to end

### `scripts/coop_patches.py`: single source of truth
- RSET raise-only helpers (cave 0x80002800, 14 words) + 2 call redirects + 4 SpawnPlayer keep-and-raise words.
- At import time it is asserted identical to the individually tested versions (patch_rset_raise.helpers, mp_spawn_patch.KEEP_RAISE).
- Commands:
  - `gecko`: print the code
  - `install-gecko`: write [Gecko]/[Gecko_Enabled] into the portable Dolphin's User\GameSettings\R3ME01.ini and set EnableCheats = True
  - `live`: Python fallback applier
  - `check`: report each word's state
- Gecko code (copy in `local/release/echoes-coop-keep-inventory.gecko.txt`):
  - Three nested `20` if-equal gates on words this patch never writes: 0x8014bacc=4082000C, 0x8023fe68=4BFFF5F5, 0x804b4854=8010CD94. They are true only while rs5mp2 is resident.
  - Then a `06` string write of the helper cave and six `04` writes.
  - Ends with `E0000000 80008000`.
  - Dolphin applies Gecko codes every frame and invalidates JIT blocks for code it writes, so no manual flush is needed. Confirmed: the patched code ran.

### End-to-end tests (fresh boot of the working copy; no Python code writes; Gecko only)
1. **Front end:** gate=False, all six patch sites hold front-end bytes ("x"), cave zero. The code does not touch the front end.
2. **Echoes load:** within one second the sites change to rs5mp2's original words, then gate=True and all six read patched, with helpers present.
3. **Single-player new game (Landing Site).** Dark Beam + Missile 40/40 were injected (data only) into the empty state before the spawn:
   - Opening loadout added on top: Super Missile etc. Missile stays 40/40.
   - First-control RSET: **markers kept, Spider Ball added**. HUD 40/40, Dark Beam in inventory.
   - Vanilla result for comparison: Dark Beam removed and Missile 5/5.
4. **2P versus (multiplayer from the Trilogy main menu):** the patches were re-applied after the multiplayer DOL load. Match start loadout is normal for both players.
   - P2 markers injected (Dark/Light Beam, Dark Suit + suit 1, Missile 30/50).
   - SpawnPlayer(P2) forced 3× via the mp_spawn_patch.py hook (the only Python code write, flushed via the slot-3 round trip).
   - P2 teleported each time ((19.6,0.4) → (-92.4,-100.9) …), with the **inventory and suit kept** and the HUD at 30. P1 untouched.
   - Vanilla wipes all 109 slots (session 5).
5. Also seen: loading the multiplayer front end from the Trilogy menu runs Echoes' single-player boot first (a P1/1 state with the normal start loadout appears), then switches to versus. The gate flips once per DOL load. Harmless.

### Not covered
- A real elevator/portal ride, or a real death in versus. Both are still blocked: no reachable elevator, and P2's fire input doesn't work.
- PAL (R3MP01) and the GameCube standalone versions. The gate means the code does nothing on any build where the three words differ.
- PrimeHack: unknown. Its own patches touch different addresses, but untested.

## 2026-10-21 (session 7): P2 input fixed, a real versus death, campaign world in 2P multiplayer mode

### P2 input: fixed
- Measured with `inputprobe.py` (key → CPlayer word changes, position delta) and `irprobe.py` (key → yaw change):
  - P2 movement, jump and morph worked all along.
  - P2's **pointer** did nothing: 0° yaw. Swapping keys between the remotes changed nothing. P1's pointer worked with either key set.
- Cause: Wii Remote 2 had `IR/Relative Input = True`. Setting P2 to **absolute** IR gives a 65° turn per press, and P2's charge shots render. In the Trilogy, aiming and firing depend on the pointer, so "fire doesn't work" was really "pointer stuck".
- Dolphin's default for this key is False (absolute), so it drops the line when it saves. Current state: P1 relative (line kept), P2 absolute (no line).
- Note: earlier sessions saw P2 menus work. Menus accept the D-pad/A, and the IR state only matters in game.

### New tools
- `remotecall.py`: a generic "call any game function on the game thread" hook at HasPowerUp (cave 0x80002c00, 29 words; FLAG/FUNC/ARGS/RES/DONE at 0x80002cc0..; scratch 0x80002d00). Installed once with a JIT flush; `call(d, func, *args)` returns r3.
- `aim.py`: turn a player to face the other using absolute-IR presses, then lock on and fire.
- `versus_death.py`:
  1. Calls the game's player teleport FUN_80160cc4(CPlayer*, CTransform4f*, mgr, 0) to put the shooter 6 units in front of the victim.
  2. Sets the victim to 1 HP with markers.
  3. Locks on and fires until the victim's health hits 0, then watches the respawn.
- `mp_world.py list|retarget`: CScriptWorldTeleporter objects (vtable 0x804b95a8; +0x20 MLVL, +0x24 MREA).
- `world_teleport.py MLVL MREA`: calls WorldTeleporter::AcceptScriptMsg (FUN_8022da4c) with a fake teleporter object (+0x1c active, +0x20/+0x24 target, +0x2c bit 0x40 = skip the transition cinematic) and a `ZERO` message. This is the same code path an elevator/portal uses after its cinematic. It injects markers into every player first.
- `coop_patches.py unpatch`: restores the original words (with cheats off) for vanilla comparison runs.

### Real death in 2P versus (P2 shoots P1 for real; P1 at 1 HP with Dark/Light Beam, Dark Suit, Missile 30/50)
| | Counters | P1 after respawn |
|---|---|---|
| **Gecko patches on** | mp_spawn=1 | respawned at another spawn point, hp 99, **suit 1, Dark/Light Beam, Dark Suit, Missile 30/50 kept** |
| **Cheats off (vanilla)** | mp_spawn=1 | respawned, hp 99, **suit 0, markers gone; Gravity Boost and Grapple Beam lost too** (spawn loadout only) |

- **A versus death respawns through MP-rules SpawnPlayer (FUN_8010cd94).** It is now confirmed live, and the keep-and-raise patch protects it.
- Both runs show an extra id 65 (capacity 1) after the respawn. Probably a per-life flag. Unknown.

### Campaign world in 2-player multiplayer mode: works
- How the multiplayer front end starts a match: it sends `ZERO` to a WorldTeleporter in its own frontend world (0xdaea74a3). Seven of these exist; the one for Sidehopper Station targets versus MLVL 0xa50a80cc / MREA 0x02690de1. Note that one of the seven already targets Temple Grounds/Landing Site.
- Retargeting that teleporter to **Temple Grounds 0x3bfa3eff / Landing Site 0x62b0d67d** before confirming the arena loads the **campaign world with both players in split-screen, under the multiplayer rules** (rules vtable 0x804b4838).
  - It plays the opening cinematic.
  - Each player gets their own spawn loadout: P1 Missile 10/10 + Super Missile (intro spawn), P2 Missile 5/5 + Spider Ball (gameplay spawn).
  - The first-control RSET fires for both players, and with the Gecko patch P1 keeps Missile 10/10 and the Super Missile.
  - Saved as **slot 7**. Slot 6 = the multiplayer front end before arena confirm.
- The versus timer keeps running: rules +0x4 = time limit 600.0, **+0x8 = elapsed seconds**. The match ends at 600 s → results screen.
  - Writing +0x8 = 0 resets the HUD clock (09:57 seen).
  - A co-op mod needs to freeze it: patch the rules update, or keep zeroing +0x8.
- **World change in 2P multiplayer mode via the real WorldTeleporter path** (Temple Grounds → Great Temple / Temple Transport B, 0x863fcd72 / 0x8f01b104), markers in both players:

| | Path counters | Both players after the world change |
|---|---|---|
| Gecko patches on | respawn=2, mp_spawn=0, rset=0 | **markers kept**: Dark/Light Beam, Missile 30/50; HUD 30. The new spawn loadout is added (Dark Visor, Echo Visor, Gravity Boost, Seeker Launcher, Dark/Light Ammo 5/5). |
| Vanilla (cheats off, slot 7 unpatched + JIT flushed) | respawn=2, mp_spawn=0, rset=0 | **also kept**, identical result |

- So in this build, a WorldTeleporter world change **does not wipe inventory**, in single-player or multiplayer rules. It spawns through FUN_8023f45c (raise only). It does not call SpawnPlayer or RSET.
- The 2019 "elevator resets items" report (Trilogy, multiplayer rules) therefore must come from the **arrival area's spawn point getting RSET from level scripts**, which the raise-only patch covers. Or it came from the author's custom FirstSpawn/loadout edits. Not reproduced here with Great Temple Transport B: no RSET fired on arrival.
- Both players arrived together. Area loading still follows P1 (the 2019 report), not tested yet.

### Open items for a co-op mod
1. Freeze or disable the multiplayer match timer and the results-screen end condition.
2. Room streaming follows P1 only. Needs a leash/teleport or multi-area loading. FUN_80160cc4 (player teleport) is now callable.
3. Scan Visor, friendly fire, enemy targeting, cutscene cameras, saves.
4. Package the front-end retarget (arena teleporter → campaign MLVL/MREA) as a Gecko code alongside the inventory patches.

## 2026-10-21 (session 8): match timer, kill limit, and the arena-to-campaign Gecko code

### Match end conditions (multiplayer deathmatch rules, vtable 0x804b4838; rules object = *(*(0x805cb314)+0x148))
| Field | Meaning |
|---|---|
| +0x04 f32 | time limit (600.0) |
| +0x08 f32 | elapsed seconds; base-rules tick FUN_8010d528: `if (!paused) elapsed += dt` via `stfs f0,0x8(r31)` @ **0x8010d560** |
| +0x38 | player count |
| +0x3c | kill limit (10) |
| +0x40 byte | flags: 0x80 = game over (checked by vt+0x38 FUN_80076644), 0x40 = kill limit enabled, 0x08 = set when someone reaches the kill limit |
| per-player kills/deaths | CPlayerState powerups **0x40** (kills) / **0x41** (deaths). The "65" seen after deaths earlier is the death counter. |

- Kill-limit check is in the deathmatch update (around 0x80076490–0x800764cc). Per player: `kills = GetAmount(0x40)`; `lwz r0,0x3c(r28)` @0x800764b4; `cmpw r3,r0` @0x800764b8; `blt +0x40` @**0x800764bc**. Falling through sets `+0x40 |= 8` and runs the end-of-match calls (vt+0x40, vt+0x3c). The same loop also runs the respawn delay, then vt+0x1c SpawnPlayer.
- Live controls (no patches):
  - Elapsed set to 596 → at 600.02 "DRAW GAME" results.
  - P2 kills set to 9, then one real kill → "PLAYER 2 WINS".
- Experiment: vt+0x38 (IsGameOver) → `li r3,0; blr` @0x8016deb4.
  - The timer ran negative (HUD 00:-6) with no end, so that check is the time-up gate.
  - It did NOT stop the kill-limit sequence (+0x40 got 0x08, end sequence started). Not used.
  - It would also block quitting from the pause menu.
- Chosen patches:
  - **0x8010d560 → nop**: elapsed never advances, the HUD clock stays at 10:00, and time-up never fires.
  - **0x800764bc → `b +0x40`**: the kill limit is never reached.
  - The game-over check itself is untouched, so quitting should still work. Not tested.

### Arena → campaign hook
- A 18-word cave at **0x80002840** runs at the entry of CScriptWorldTeleporter::AcceptScriptMsg (0x8022da4c, original `stwu r1,-0x20(r1)`).
- If teleporter +0x20/+0x24 == Sidehopper Station (a50a80cc / 02690de1), it rewrites them to Temple Grounds / Landing Site (3bfa3eff / 62b0d67d). Then it runs the original instruction and branches back to 0x8022da50.
- It touches only the teleporter object's own fields, uses only r11/r12 (volatile at entry), and does nothing for other arenas or messages.

### Packaging (`scripts/coop_patches.py`; exports in `local/release/`)
- **Code A, keep inventory** (`echoes-coop-keep-inventory.gecko.txt`): unchanged from session 6.
- **Code B, campaign session** (`echoes-coop-campaign-session.gecko.txt`, 18 lines):
  - the same 3-word rs5mp2 gate
  - a `06` write of the retarget cave
  - three `04` writes: timer nop, kill-limit branch, hook branch
  - The two codes are independent, and every original word is asserted against the DOL at build time.
- `install-gecko` installs and enables both. `check`/`unpatch`/`live` cover both. `e2e_monitor.py` reports both caves.

### End-to-end test (fresh boot, both Gecko codes, no Python code writes until the kill test)
1. Front end: gate off, all 9 sites hold front-end bytes, both caves zero.
2. The Echoes DOL loads, then all 9 sites read patched and both caves are present.
3. In the multiplayer menu: join two players, Deathmatch, **Sidehopper Station** → world **3bfa3eff Temple Grounds**, 2 players, MP rules, elapsed 0.0.
   - The opening cinematic plays, then split-screen gameplay at the Landing Site with the HUD clock **10:00, frozen** (elapsed still 0.00 after about 2.5 min).
   - Code A also ran: P2 kept Super Missile + 10/10 and gained Spider Ball at the first-control RSET.
   - Saved as **slot 8**, overwriting the old Great Temple test state.
4. P2 kills set to 9 (data), then a real kill of P1 (remotecall harness): kills 10, **+0x40 stays 0x70, no results screen**, clock still 10:00. P1 respawned with its markers kept (code A).

### Known issues / not tested
- **Black world after any death in the campaign under MP rules.** Both viewports render only the HUD. It happens without these patches and without the kill limit. Versus arenas don't do this. Probably the respawn camera/area: P1 respawns at (-96,-197.8,-48.1), near the Landing Site start. Next item to investigate.
- Picking Sidehopper Station now always starts the campaign while code B is on. The other arenas are unaffected (by design; not retested).
- Pause → quit ending the match: not tested.
- Code B on PAL/GameCube: inert (the gate doesn't match). Not run.

## 2026-10-21 (session 9): room loading follows P1 only — found, fixed (code C), verified

### How the current room is chosen
- `CStateManager+0x1e44` = current area index (the one area that is "active"; +0x1e48 = previous). Only 3 writers: SetCurrentArea **FUN_8023eb60**(mgr, &idx), FUN_8023f6c0 (spawn), FUN_8023be44 (load).
- Doors: **CScriptDock** (vtable 0x804b8088, think = slot +0x14 FUN_801e6a84). Fields: +0x2c8 dock index, +0x2cc area index, +0x2d0 state (1→2 open/armed, 3 = belongs to a non-current area), +0x2d4 flags.
- In state 2 the dock tests whether **players[0]** has crossed its plane: `lwz r5,0x14f4(r31); addi r5,r5,0x50; bl FUN_801e6cec` @ **0x801e6bf8** (r30 = dock, r31 = mgr). If crossed and the other side's area is loaded (area +0xd0 == 0x10) it calls SetCurrentArea(other side). That's the whole bug: only P1 can move the "current room".
- Area dock list: world = mgr+0x1dac; areas = world+0x2c (8 bytes each, +4 = CGameArea*); area +0x8c count, +0x94 docks (0x48 bytes; plane points +0x14/+0x20/+0x2c). Player position used for the test: CPlayer+0x50.

### Repro (`dock_cross.py`: arm a dock and teleport one player just past its plane via the game's own teleport)
- Vanilla, P2 crosses Landing Site dock 0 alone: current area stays 0x0, the dock disarms (state 0), P2 falls into the next room's unloaded space — its viewport shows only sky (`shots/r2.png`). This is the 2019 "P2 gets trapped" report.

### Fix: code C (`echoes-coop-shared-rooms.gecko.txt`)
- C2 hook at 0x801e6bf8 replacing the single-player test with: for each player k (players[0..count)), run the same FUN_801e6cec test; on the first player that crossed, teleport every other player to that player's transform (FUN_80160cc4, same call spawn points use) and return true. So the area switch proceeds and nobody is left behind.
- Live (fresh boot, all three codes via Gecko only):
  - **P2 crosses alone** → current area 0x0 → 0x1, P1 pulled through, both viewports render the new room (`shots/x7.png`).
  - **P1 crosses** → same result, P2 follows (`shots/x8.png`), both 99 HP.
- Known rough edge: the follower lands exactly on the leader; the game pushes them ~1 unit apart, and once the follower took 6 damage (99→93). Next: offset followers a couple of units behind the leader along the door normal.

### Gecko format change (all codes)
- Hooks now use the **C2 (insert ASM)** code type; bodies are position-independent (game calls via `lis/ori r12; mtctr; bctrl`). Nothing is written to fixed low-memory caves anymore.
- Why: with cheats on, **0x80001800–0x80003000 belongs to Dolphin's code handler and code list** (list extended into 0x80002000 once code C was added; earlier `06` cave writes at 0x80002800/0x80002840 only worked because the list was short). `coop_patches.py live` (cheats OFF only) still uses 0x80002800+ caves.
- Codes A and B re-verified in C2 form on the fresh boot: Sidehopper Station → Temple Grounds with 2 players, HUD clock 10:00, P2 kept Super Missile 10/10 and gained Spider Ball at the first-control RSET.
- `check` now reports hooks as `hooked->addr`; `e2e_monitor.py` shows H for hooked sites.

### Save-state gotchas (important)
- **Save states contain Dolphin's Gecko code list** (it lives in game RAM). Loading a state made with older codes runs the older codes. After changing codes, boot fresh and make new states. Slot 8 still has the session-8 codes (no code C).
- Shift+Fn save also fires Fn load: when the slot file already exists, the old state gets loaded. It hit again this session (slot 5 now holds an old SP state). `dolctl.py save <slot>` (delete file, save, wait for size to settle) is the only way to save now. F9+ are not hotkeys.
- Slots now: 6 = **2P campaign session, Landing Site, all three current codes** (use this). 8 = 2P campaign, session-8 codes. 2 = versus Sidehopper. 4 = Trilogy SP menu. 7 = MP campaign start (session 7, retargeted by Python).

### Still open
- Black world after death in the campaign under MP rules (deferred).
- Follower offset on room change; elevators/portals with code C (WorldTeleporter already moves everyone — session 7); pause → quit; Scan Visor, friendly fire, enemies targeting P2, cutscene cameras, saves.

## 2026-10-21 (session 10): follower offset, death black screen, full-screen cinematics

### Follower offset (code C)
- The door hook now builds the crossed door's plane (FUN_8033b77c on dock +0x14/+0x20/+0x2c; the normal points to the crossed side) and puts follower k at leader + normal × 2.5·k, i.e. further into the room being entered.
- It only pulls followers when the other side's area is loaded: the hook inlines the caller's FUN_8009af64/FUN_8009af7c check, areas[idx]+0xd0 == 0x10.
- Live: P2 crosses dock 0 → P1 lands 2.5 units deeper (2.49 measured), both at 99 HP, both views in the new room.

### Death black screen: two causes, both fixed (code B gets a 2nd hook)
1. **Camera filter pass 1 stays faded to black.** CStateManager +0x1e98 holds the camera filter passes (0x2c each, 11 per viewport, 0x1e8 per viewport). Pass 1 (+0x1ec4/+0x20ac) turns {1,1,0} on death.
   - Versus arenas clear it with their own script objects; campaign rooms have none, so it stays black.
   - Fix: after SpawnPlayer's final notify call (`bl 0x8010ca50` @ **0x8010d478**), call CCameraFilterPass::DisableFilter (0x80056bf8, f1 = 0.5 s) on pass 1 for every viewport.
2. **Respawn point is often in another room.** The MP rules pick the best spawn point in the whole world, so P1 respawned at (-73,-125,-38), the room behind the Landing Site door, while the current area stayed 0. That gives an unrendered world even without the fade (observed once the fade was fixed).
   - Fix: the same hook teleports the respawned player (r27 = index in the caller) to 2.5 units behind the first other player, with the same facing.
- Live:
  - P2 kills P1 → screen stays lit, P1 respawns next to P2 in the current room, HP 99, markers kept (code A), death count 1.
  - The reverse kill (P1 kills P2) also stayed lit.
  - Note: the kill harness (`versus_death.py`) now also sets the victim's CPlayerState+4 bit 0x40. Clear means immune; FUN_8014baa4 checks it. The flag was clear on one player in a reloaded state, so shots did nothing.

### Opening cinematic: now full screen in P1's view (code D)
- What happens (screenshots `cin_*.png`): during the campaign's opening cinematic, **P1's half plays the cinematic** and P2's half shows normal gameplay at the spawn point.
  - Camera manager +0x12 (cinematic camera id; 0xffff = none): P1 = 0x0530, P2 = 0xffff.
  - Cinematics are per-viewport camera state, so "both play the same cinematic" would mean driving P2's camera manager from P1's cinematic camera. Possible, but deeper work.
- The game already supports "one full-screen view during a cinematic". FUN_800e885c (top-level draw) and FUN_8024b554 (viewport layout) collapse to 1 view when FUN_8005c1ec(players[0]'s camera manager) is true. That requires the cinematic camera's multiplayer flag (cinematic obj +0x2c → +0x208 bit 1), which campaign cameras don't have.
- Code D: both call sites (**0x800e88a4**, **0x8024b584**) call FUN_8005c1d0 ("any cinematic active") instead.
- Live: the cinematic fills the whole screen. When it ends, split-screen returns automatically. P2 just waits at the spawn point meanwhile.
- **New default: P1's cinematic is full screen.** Any later cinematic that P2 triggers while P1 isn't in one would still show only in P2's half. Not tested.

### Packaging and end-to-end
- Four codes now, each independent:
  - A: keep inventory
  - B: campaign session + death fix
  - C: shared rooms
  - D: full-screen cinematics
  - Exports are in `local/release/*.gecko.txt`.
- `to_campaign.sh <slot>` scripts fresh boot → 2P campaign → save after the opening cinematic. The "Communications with the Wii Remote have been interrupted" screen shows up when the multiplayer DOL loads, so dismiss it with A and resume (`/e/coop/re/tc_resume.sh`).
- E2E on a fresh boot with all four codes (slot 8):
  - all 13 sites patched/hooked
  - cinematic full screen
  - split-screen gameplay after it
  - P1 death → lit, respawned next to P2, inventory kept
  - P2 through the door → both moved to area 1, P1 2.5 units ahead
- Slots: **8 = current, all four codes, 2P campaign gameplay (use this)**. 6 = mid-cinematic (codes A–C at the time, no D). 7 = older.

### Still open
- A cinematic triggered by P2 alone.
- Elevators with all codes.
- Pause → quit.
- Scan Visor in MP mode.
- Friendly fire.
- Enemies targeting P2.
- Saving progress.

## 2026-10-22 (session 11): Scan Visor under MP rules + no friendly fire (code E)

### Scan Visor
- CPlayer scanning: +0x5a4 scanning state (0 none, 1 scanning, 2 complete), +0x5a8/+0x5ac scan time, +0x5b0 scan target uid, +0x5b4 scan asset id. SetScanningState = FUN_80149d30; update loop around 0x8014a180–0x8014a650 (Ghidra can't decompile it — paired singles; use `scripts/dis.py`).
- **Can-scan check FUN_80149f3c**: for a locked-on target that is not a player, `if (IsMultiplayer(mgr)) return false` — `bl 0x8024b4fc` @0x8014a0e0, `beq +0xc` @**0x8014a0e8**. That's the whole "Scan Visor does nothing in MP" bug. Patch: `b 0x8014a0f4` (always continue to the range/visibility checks).
- Live: lock on in Scan Visor → state 1 → 2 in 1.5 s, scan frame shows the object; completion still sends the scanned object its scan-done script message (0x8014a430) and stores the scan time in that player's CPlayerState (FUN_8016e39c @0x8014a474), so critical scans should trigger. Verified on a fresh boot (target 0x042a).
- **Limitation:** no scan text. SP shows text in a paused text box (FUN_8024b1d4(mgr,1) pauses the world, gated by IsMultiplayer at 0x80149ef0); enabling that under MP only paused both players with an empty box — the MP HUD has no scan-text pane. Left out. Scan colours stay orange (the logbook is per-save, MP player states are fresh).

### No friendly fire
- Damage entry **FUN_80242338(mgr, &damager, &damagee, &radiusSender, info, filter, knockback)**. Both it (call @**0x802423b0**) and the radius-damage path FUN_80243a70 (call @**0x80243ad4**) call **FUN_802429ec(out, mgr, &damager, &radiusSender, &damagee, info)** to build the final CDamageInfo (it applies difficulty/AI damage multipliers). Note the order change: r5 = damager, r6 = radius sender, r7 = damagee at both sites.
- CDamageInfo: +4 damage, +8 radius damage, +0x10 knockback. Entity uid = CEntity +0x8 (u16). Weapon cast FUN_8029ede0; weapon owner uid at +0x2c8. ObjectById FUN_8023e9fc(mgr, &uid).
- Hook (C2 at both call sites): call FUN_802429ec, then if the damagee is a player and the damager or radius sender is a player or a weapon owned by a player → zero +4/+8/+0x10.
- First version had damagee/radius sender swapped (would have stopped players damaging enemies); caught from the disassembly before shipping, fixed and re-verified.
- Live (fresh boot, all five codes):
  - `dmg_test.py` (calls FUN_80242338 directly): damager = a door → P1 99→89; damager = none → 89→79; damager = P2 → 79→79 (blocked).
  - Control without the hook: door 99→89 and P2 89→79 (so the harness is valid).
  - Real shots: P2 locked on and fired at P1 (1 HP) for 12 s → P1 unharmed. Control without code E: same attack killed P1.
  - Not directly tested: player shots damaging enemies (no enemies near the start; the hook only acts when the damagee is a player, so it can't affect that by construction).

### Harness/tooling
- **Dolphin's Gecko code list now reaches 0x80002e00** (five codes). Test harness moved out of 0x80001800–0x80003000: remotecall cave 0x80003a00 (FLAG/FUNC/RES/DONE 0x80003ac0.., ARGS 7 words at 0x80003ae0, SCRATCH 0x80003b00), world_teleport fake object 0x80003c00/0x80003d00. remotecall now passes up to 7 args (r3–r9). Test-only; nothing shipped uses these.
- `dis.py` (own PPC disassembler for paired-single functions), `face.py` (turn to a world point, Scan Visor, hold lock), `dmg_test.py`.
- `to_campaign.sh` now dismisses "Communications with the Wii Remote have been interrupted" when entering the MP front end. Shift+F9 isn't a hotkey: save slots 1–8 only.
- **Idle Wii Remote 2 disconnects** after a few minutes without input (game shows "Player 2 … interrupted" and pauses; A on remote 2 resumes). Not an issue with someone holding the controller, but scripted tests must press NUM5 first.

### Codes now
A keep inventory · B campaign session + death fix · C shared rooms · D full-screen cinematics · E scan + no friendly fire. Slot 5 = fresh 2P campaign with all five codes.

## 2026-10-22 (session 12): scan text under MP rules (code E, 2 more hooks)

### Why there was no text
- SP shows scan text in the scan pane after "scan complete" (SetScanningState FUN_80149d30, state 2 branch 0x80149ee4: `if (!IsMultiplayer) { if (scan has text) ScanPause(mgr,1) }`). The MP HUD has no pane, so enabling ScanPause under MP just froze both players behind an empty frame (session 11).

### Workaround: the game's paused message box
- `FUN_8024ac68(mgr, STRG id, f1)` = show message: sets mgr+0x2bd0 = STRG, +0x2bf0 = f1, then SetPauseState(mgr, 6) (FUN_8024ab94). The message screen (FUN_800e8d50 → FUN_801599b0) draws a boxed, paged text window and pauses the world; A advances/closes, close → SetPauseState(mgr, 0).
- SetPauseState ignores every request under MP rules (`bl IsMultiplayer; cmpwi r3,0; bne out` at 0x8024abb0–b8). New hook at **0x8024abb4** lets states 0 and 6 through only.
- Scan text: actor vt+0x78 → scan info object (`+0` SCAN id, `+4` STRG id). Hook at **0x80149ee8** (the `bl IsMultiplayer` in the scan-complete branch; r29 = mgr, r30 = scanning player): under MP, look up the scan target (player +0x39c) → CastToActor (FUN_8029fd70) → vt+0x78 → if STRG != -1, show it with FUN_8024ac68; returns 1 so the SP pane path stays skipped. SP rules: returns 0, unchanged.
- Live (fresh boot, all five codes): P1 scans the Landing Site statue → box opens automatically ("Object scan complete." → A → "Statue's motivator unit is functional, but requires power to operate. The Power Beam can't energize the statue. Seek an alternate energy source." → A → closed), world paused for both players while open, both move again after. P2's scan opens the box too.
- Manual check before the hook: a STRG via FUN_8024ac68 under MP ("All Sky Temple Keys Found!", 2 pages) worked once the gate was open.

### Limitations
- **Only Wii Remote 1 advances/closes the box** (P2's A does nothing). If P2 scans, P1 presses A. Fixable by finding where the message screen reads controller input.
- The box is drawn in the top half (over P1's view); both players see it on the shared screen.
- In the paused state the game only registers A presses of ~0.4 s+ in my scripted input (150 ms presses get dropped); real button presses are fine.
- Scan text pages are the same STRG the SP scan pane uses; the logbook/scan-progress colours are not shared between players.

## 2026-10-22 (session 13): either Wii Remote advances the message box (code E, 1 more hook)
- Input path: each controller's CFinalInput is passed to **FUN_800ddc04(huds, mgr, input)** (called from FUN_800e8250-ish @0x800e835c). It loops players and hands the input only to the HUD of the player whose CPlayerState +0x588 (controller index) == input +4: `cmpw r7,r0` @**0x800ddc28** → `HUD[i] = huds+0x60+i*8` → tail call FUN_80158a40(HUD, mgr, input). FUN_80158a40 in HUD state 6 (message) → FUN_800f82f0(HUD+0x128, input) = message window input.
- The message box is opened on HUD 0 (FUN_800e8d50 uses `*(this+0x18)+0x60`), so only player 1's controller reached it.
- Hook at 0x800ddc28: when mgr+0x2c0c == 6 (message box open), dispatch every controller's input to HUD 0 (force a match on the first iteration); otherwise the original compare.
- Live (fresh boot, all five codes): P1 scans the statue → **P2's A alone** pages "Object scan complete." → statue text → closes; both players move afterwards. P1's A still works the same.
- Side effect to keep in mind: while any message box is open, both players' buttons drive it (that's the intent). Other HUD input (normal gameplay) is untouched because the state is 0 then.

## 2026-10-22 (session 14): enemies vs player 2 (code F) and Gecko space (code E rework)

### What vanilla enemies already do with 2 players
- **Detection is multi-player:** CPatterned InDetectionRange (FUN_80130864) loops over every player (sm+0x14f0 count, sm+0x14f4[]).
- **Most other base checks read players[0] only:** InRange FUN_8013071c, InMinRange FUN_80130794 (+0x3c4), InMaxRange FUN_801307fc (+0x3c8), and in CPatterned::Think (vt+0x14 = FUN_8012b93c) the "player too far" timer at 0x8012c2ac: while players[0] is farther than +0x3d8 (25) from the enemy, +0x3e0 counts up; +0x3dc (5 s) is the give-up threshold.
- Measured, vanilla:
  - Space Pirates (type 0x84) and Splinters do hit P2 when P2 is close (damage goes to the near player).
  - Pursuit and give-up follow P1. A Central Mining Station pirate walked away from P2 to end up 4 units from P1. Hive hall Splinters fighting next to P2 with P1 ~30 away ran +0x3e0 up to 15.1 s and 9.3–9.7 s (past 5 s) in two runs.
- Classes seen: 0x8d (Wall Crawler-type, passive) and 0x7d (Pill Bug-type swarm, passive) in Mine Shaft; plain CPatterned subclasses (vt in RSO) for Splinters.
- Rooms with enemies: Temple Grounds MLVL area table → internal names and MREA ids (parsed from Metroid1.pak MLVL; area MREA at area start +0x4c). Hive hall `0e_temple_hive_hall` = **1582ec4a** (8 active: 3 Splinters + 5 dormant), hive02 = d627000b (dormant Dark Troopers/Splinters), 02_temple_exploration = 62ac8ac4.

### Code F: nearest player during enemy Think
- Hook at **0x80240be8** (`bl 0x80240994(mgr, entity, f1 dt)` in the per-entity think loop FUN_80240720; orig 4bfffdad).
- For an entity that IsA CPatterned (FUN_8029da94(e, 4)) with 2+ players: find the nearest player (squared distance of +0x50), swap players[0]↔[k] and player states sm+0x1504[0]↔[k], run the think, swap back. Other entities (doors, triggers, …) are untouched.
- Live A/B on the same save (`ftoggle.py off|on` flips the hook's `cmpwi r0,2` to 99 in the code list and flushes the JIT):
  - Hive hall, P2 among the Splinters, P1 at the far door. **F off:** timers 15.1 / 9.3 s. **F on:** 0.00 for every enemy, both runs; Splinters end nearer P2 (13.7 vs 16.7).
- Central Mining Station's two nearby pirates on a fresh arrival are dormant (timers and positions frozen), so that room doesn't discriminate.

### Gecko space
- Dolphin's code list must end below 0x80003000. With six codes it would have ended at ~0x80003070, and **Dolphin silently dropped code F** (the check showed `original`).
- Code E's no-friendly-fire was two copies of a 652-byte hook at the two `bl MODIFY_DAMAGE` sites. It is now one hook at the entry of MODIFY_DAMAGE (0x802429ec, orig 9421ffa0):
  1. Push a frame and save the args.
  2. Set LR to the hook's own post code (`bl here; mflr; addi`), run the displaced stwu, and continue into the function.
  3. The function returns into the post code, which zeroes +4/+8/+0x10 for player-on-player hits, restores the caller's LR and returns.
  - The "is a player" test is a shared bl/blr subroutine.
- Code E is now 832 bytes (was 1624); the list ends at ~0x80002d58 with all six codes.
- Retested after the change: dmg_test p2 → 99 → 99 (blocked); door → 99 → 89.

### Codes now
A keep inventory · B campaign session + death fix · C shared rooms · D full-screen cinematics · E scan + no friendly fire · F enemies fight the nearest player. Slot 5 = fresh 2P campaign with all six; slot 1 = hive hall (Splinters) with all six; (slot 3 is the harness JIT-flush slot: remotecall.install() overwrites it every run).

### P2 respawning under the level (code B fix)
- Report: "the second player sometimes spawns under the level".
- Cause: respawn_with_partner put the respawned player 2.5 units **behind** the partner at the partner's height, with no collision check. A wall, step or slope behind the partner put them inside the geometry, and they fell through.
- Repro (`spawn_test.py x y z fx fy`: place P1 with a facing, kill P2 with a door's damage through ApplyDamage, log P2): Landing Site, P1 at (-74.2, -134, -37) next to dock 0.
  - Old code, 24 placements: 11 completed fine; facing 135° and 180° put P2 at z -56.5 against P1's -38.2, i.e. fallen out. The remaining 11 never ran (`remotecall` timed out after the second fall).
- Fix: teleport the respawned player to the partner's own transform (CPlayer +0x20); the hook also got smaller. Players do collide and separate to ~1.1 units, the game's own spawn spacing.
  - Same 7 placements including both failing ones: P2 always ends at the partner's height, 1.1 units away.
- World arrival is not affected: both players are created at (0,0,0) / (5,0,0), then placed together at the spawn point 1.1 apart (checked in Central Mining Station).
- Same risk, not yet seen: code C puts door followers 2.5·k along the door normal at the leader's height. If a room rises right after a door, a follower could end up in the floor. Not changed; door frames normally sit on flat floor.

## 2026-10-22 (session 15): safe door followers, real elevator ride

### Game collision ray
- **RAY_CLEAR = FUN_800a2ac8(mgr, &start, &unit dir, &CMaterialFilter, f1 length) → 1 if no static collision.** Found via the CPatterned line-of-sight predicate FUN_80130bbc. Not usable from Ghidra (paired singles); read with dis.py.
  - It loops the world's loaded areas (mgr+0x1dac → +0x54 list, next +0xd4) and calls FUN_803bd3f8 per area.
- **Filter** as built by the AI: {u64 include = 1 << *(r13-0x78d8) (= bit 59, solid), u64 exclude = 0, u32 type 1}. FUN_802b8834 is a 64-bit shift (r3:r4 << r5). r13 = 0x805d1660.
- Live check at the Landing Site door spot where P2 fell through (-74.2, -134, -37.2):
  - +x 2.5 → 0 (wall)
  - -y 2.5 → 1
  - down 3 → 0 (floor 1 below)
  - down 0.5 → 1
- `raytest.py x y z dx dy dz len`. remotecall now passes f1 (`call(..., f1=)`; F1 word 0x80003ac8).

### Code C: collision-checked follower placement
- Per follower, cast RAY_CLEAR from the leader's position +1 z (chest) along the door normal for k·2.5 + 0.5. The filter is built once per crossing.
  - **Clear:** place the follower at leader + n·k·2.5, z + 1 (FOLLOW_LIFT), and let them drop onto the floor.
  - **Blocked:** place them on the leader's own spot.
- Code C is now 744 bytes; the list ends ~0x80002d78.
- Live, fresh boot, all six codes:
  - P1 crosses Landing Site dock 0 → area 1; P2 lands 2.5 deeper, at z -37.2, then drops to -38.3 (the leader's floor).
  - P2 shoots dock 0 open and walks through (`dock_cross.py 2 --walk 2.5`) → area 1; P1 pulled in; both at floor height ~1 apart; both views render the room.
  - Forced fallback (step patched to 40 in the code list, so the ray must hit a wall) → follower on the leader's spot, floor height.
- **Harness artifact, not a game bug:** `dock_cross.py 2` in teleport mode fails with code F on.
  - remotecall runs inside HasPowerUp, which enemy Think can call while F has P2 in slot 0. A Teleport of P2 then acts as "main player" and the dock disarms.
  - Use `--walk` (crossing happens in normal gameplay).
  - Also: timed-out remote calls are now cancelled. A pending one ran after a world load on a freed player and crashed Dolphin (invalid write, PC 0x803d0770).

### Elevator, real ride (Great Temple Transport B → Temple Grounds), all six codes
- Arrival pads arm only after the player leaves them: trigger 0431 EXIT → relay → ACTV the pad trigger 046d. ENTR → relays → sequence 044c → ZERO → WorldTeleporter 041b (→ MLVL 3bfa3eff / MREA 4cc37f4a).
  - `conns.py <uid>` walks the script graph backwards. Connections are {state, msg, u32 editor id}; the entity's editor id is at +0xc.
- `elevator_watch.py --off x y z --pad x y z --seconds 100 --shots DIR`. Timeline:

| t (s) | What happens |
|---|---|
| 0–2 | P1 rides down, world → Temple Grounds |
| 4.6 | Area 0x20 loads |
| 7.9 | Both players co-located at (106, 7, -6), arrival via SpawnPlayer + respawn-onto-partner |
| 12–14 | P2 falls ~43 units down the arrival shaft while P1 is held by the arrival sequence |
| 15 | The arrival script puts both at the exit (125, -4/-5, -38) |
| 16.5 | All camera filter passes clear |

  - Arrival cinematic full screen ("Temple Grounds"), then split screen with both players side by side.
  - Inventories unchanged.
- **Not a world elevator:** Temple Grounds 00_elev_temple_cliff_c (984caa1f) has no WorldTeleporter. Teleporting onto its pad played its cinematic, then the MP rules respawned both at a spawn point in area 0x35 with camera filter passes 2 (white) and 3 (black) left on. Clearing them (DisableFilter) showed the room. Not a real ride; it needs story state.
- **Item grant on arrival:** arriving in the Great Temple (via the WorldTeleporter path) raises both players to that spawn point's loadout: Dark Visor, Echo Visor, Gravity Boost, Seeker Launcher, Dark/Light ammo, ids 19–21. This is code A's keep-and-raise in MP SpawnPlayer. In single-player an elevator arrival doesn't apply a loadout.

## 2026-10-22 (session 16): start items only once (code A)

### Cause of the Great Temple item grant
- Arrivals (session start and every WorldTeleporter world change) spawn through **FUN_8023f45c(mgr, spawn point, player index)**.
  - Callers: 0x8023fe68 (a gate word, don't patch), 0x8023fed0 (SP default), 0x80240090 (MP per-player loop).
  - It teleports the player, then runs its raise-to-loadout loop only if `rules->vt+0x20(rules, mgr, player index)` != 0 or game state +0x638 bit 0x40 is set.
  - Rules = *(game state + 0x148) via FUN_800bc40c. Player index = player+0x1388.
- **MP rules vt+0x20 = 0x8010d580 `li r3,1; blr`**: always apply. So every arrival raised both players to the arrival spawn point's loadout. The Great Temple Transport B spawn has a debug-like loadout: Dark/Echo Visor, Gravity Boost, Seekers, Dark/Light ammo, ids 19–21, 3 Energy Tanks.
- This is the only rules vt+0x20 call site (0x8023f578).
- A new MP session builds new, **empty** CPlayerStates; their first spawn fills them (statepoll, fresh boot: P1 state @8083ada0 empty at 199.1 s, Landing Site loadout at 203.6 s).

### Fix (code A)
- **Hook 0x8010d580** (orig 38600001): return 1 only while mgr+0x1504[r5] has Power Beam capacity (+0x5c) == 0, i.e. only for the session's first spawn.
- **SpawnPlayer (death respawn) keep-only:** 0x8010d2b0 `mr r3,r28` → `b 0x8010d364` skips the whole 109-slot loadout loop.
  - Replaces the two raise-only words 0x8010d2d0/0x8010d324.
  - SpawnPlayer never runs as a session's first spawn (match start: mp_spawn=0).
  - Consequence: dying doesn't refill ammo either.
- List now ends ~0x80002da0.

### Verified (fresh boot, all six codes)
| Test | Result |
|---|---|
| Session start | Landing Site start items applied as before (P1 Missile 10/10, P2 5/5, …) |
| WorldTeleporter path into Great Temple Transport B | No change to either inventory |
| P2 killed in Transport B | Respawned, inventories unchanged, HP 99 |
| Real elevator Temple Grounds → Great Temple | Arrival with both inventories unchanged; all filter passes clear; both players at the exit |

- The Transport B death respawn launched the elevator by itself. The MP "best" respawn point there is the elevator pad, and the pad was armed because the test had moved P1 off it. The ride to Temple Grounds was normal; inventories unchanged.
- Temple Grounds Transport B departure: trigger 0451 (pad, ENTR → sequence → teleporter 0473) is armed by 0442 EXIT. Both players must leave a fairly large area (≥ ~15 units) around the pad.

## 2026-10-22 (session 17): saving in co-op (code G), resume investigation

### Save stations
- The save station is a CScriptSpecialFunction (class type 0x50, vt 804b8cb8), function type 7 at +0x150.
  - AcceptScriptMsg = vt+0x18 FUN_802107f4. It dispatches through a member-function-pointer table at 0x804b8958 (12 bytes per function type); type 7 → **FUN_8020e800**.
  - On ACTN, FUN_8020e800 does `if (IsMultiplayer(mgr)) return;` (bne at **0x8020e83c**). Otherwise: IncrPickup(state, 0x2a, 1) (energy refill), then SetPauseState(mgr, 5) (the save screen).
- Temple Grounds save room 00_save_hive04 (1461dc2f, area 0x12):
  - Pad trigger 0021 (arms on exit like elevator pads) → relays → sequence 0037 … → timer 003d → ACTN → special function 0027 (type 7).
- **Code G:** 0x8020e83c → nop. Code E's SetPauseState gate now also lets state 5 through.
- Live, fresh boot, all seven codes:
  - Step off/on the pad → "Do you want to save?" full screen.
  - P1 A → save written (test NAND `User/Wii/title/00010000/52334d45/data/save.bin` modified). The Echoes slot data around 0x3660–0x4620 changed.
  - Then pause state 6 (an undrawn "saved" message): one A closes it, and play continues.
  - ~7 s later the room's script puts both players back at the station spawn.
  - Inventories unchanged.
- Backup of the test NAND save before the first co-op save: `<workspace>/nand_backup\save.bin.20261022-052038`. The file is dated 2021, so it's presumably a copy of the owner's real Trilogy save made at workspace setup; only our copy was written.
- P2's half of the screen showed a frozen image in this room (camera?), not investigated.

### Resume: what loading a save means
- Save data per slot = **2000-byte game-state blob**.
  - FUN_801b2414(slot) → FUN_802a9e88(app *0x805cb310, memstream(slot+4, 2000)): destroys the current CGameState (0x640 bytes), then builds a new one from the stream (ctor FUN_800b9178). Game state global = *(r13-0x634c) = *(0x805cb314). App+0x70 → +0x114 owns it.
  - FUN_802aa4bc = new default game state.
  - FUN_801af454 = serialize current → reload.
  - FUN_802a9174 = rebuild keeping parts.
- Game start state machine FUN_800eacec(…, 8):
  - If not FRND rules and DAT_805cb338 == 0: new state, then if `*(DAT_805cb220+0x29)` == 0 round-trip it, else load memory-card slot `*(DAT_805cb220+0x2c)` (FUN_801ad590/801adfcc).
  - With DAT_805cb338 != 0 it calls FUN_800b9ff8 ("InitialWorld"/"FrontEnd" boot path).
- In our co-op session: DAT_805cb220 = 0x807a80e0, +0x29 = 1, +0x2c = 0; DAT_805cb338 = 1; app+0x74 = 6.
- MP matches are created in FUN_800eacec(…, 0xf) from app+0x74 1–5.
- Not yet known:
  - how MP player states relate to the game state's saved player state (P2 needs an inventory on resume);
  - how to steer the MP session start into the slot-load branch and the saved world/area.

## 2026-10-22 (session 18): co-op save loading (code B), P2 save-room camera (code C), one combined code

### How a co-op session starts (what doesn't work, and why)
- The arena WorldTeleporter in the MP front end never gets ZERO. It gets XCRT/XALD/XWLD, then **ACTV** at the match start, then XDEL. The front end reads its target; the re-aim happens on the first message.
- The MP match start is **FUN_800ba388**, reached from the start state machine FUN_800eacec(…, 8) when the front end's rules report 'FRND'. In order it:
  1. reads the menu's rules object;
  2. builds the MP rules (FUN_800760d8, vt 804b4838);
  3. rebuilds the game state by a save/load round trip (FUN_802a9174) — the current world becomes Temple Grounds here;
  4. installs the rules (SET_RULES FUN_800bc414 @0x800ba6a0);
  5. writes the MP settings (gs+0x14c 'FRND', +0x150.., +0x158, per-player blocks from +0x160);
  6. per player i: state+0x588 = controller index (FUN_8016d76c), gs+0x74+8i entry (+0 word, +4/+5 bytes);
  7. finishes with FUN_800ba230, FUN_800bb364(gs), FUN_8017e00c(gs+0x94).
- Loading at ACTV (front-end game state still active, rules vt 804b4cb8) → single-player rules, 1 player, black screen + "Wii Remote interrupted".
- Loading at the FUN_800ba388 entry: the world is still the front end (daea74a3) there, so a campaign start can't be told apart from a real deathmatch.

### Co-op save loading (code B, hook at the match start's epilogue 0x800ba7d0, orig 396100e0)
- When the current world is Temple Grounds (the re-aimed arena):
  1. Keep the rules object (clear +0x144 so the old state's destructor doesn't delete it), the MP settings (+0x14c, 0x64 bytes), the per-player entries (+0x74, 0x20) and each state's controller index (+0x588).
  2. Load save file **slot 2** (third file) like single-player Continue: new(0x5c) → FUN_801ad590(t,0); FUN_801ad764(0,1); FUN_801adfcc(t, 2); FUN_801ad764(t,1).
  3. Set DAT_805cb220+0x2c = 2 again (the loader resets it; save stations save to that slot via FUN_801ade7c).
  4. Put everything back (SET_RULES deletes the loaded state's default rules).
  5. Copy P1's state +0x10/+0x14 (health) and +0x50..+0x574 (suit, powerup count, 109 × amount/capacity) to states 1–3. Not +0x0 (index) or +0x40 (owned pointer).
  6. Re-run FUN_800ba230 / 800bb364 / 8017e00c.
- An empty slot makes the game build its new-game state (Temple Grounds, Landing Site) → new co-op game; the first-spawn loadout (code A) still applies.
- Game state: +0 current world; +0x14 count (4) and +0x18 player-state rc_ptrs (8 bytes each); +0x8/+0x10 world states (0x24 each: +0 MLVL, +4 area index, +0x18 desired MREA).
  - A save holds all 4 player states.
  - The save file has a 'GMST' record per used slot: slot 0 at 0x3e44, slot 2 at 0x5d84.
- Retarget (code B) is back to the plain re-aim.

### Verified (fresh boots, all codes)
1. Empty slot 2 → new game at the Landing Site, 2 players, MP rules, controllers 0/1, normal split screen, slot field 2.
2. Mark P1 (Dark Beam, Missile 33/40, health 57) → save at the Temple Grounds save station → save.bin gets a new GMST at 0x5d84 (slot 2). Health saved as 99: the station refills energy before saving.
3. Close Dolphin, fresh boot, Sidehopper → arrive in the save room (area 0x12), P1 **and P2** both have Dark Beam and Missile 33/40, health 99, controllers 0/1.
4. P2 walks out of the station. Friendly fire is still blocked (P2 30 dmg ×3 → 0; door 30 → applied).

### P2 frozen in the save room (code C, hook 0x801db1f8, orig 807f1514)
- Camera managers: sm+0x1514 + 4·viewport; +0x12 = current cinematic camera uid (invalid 0xffff).
- The camera handler FUN_801dafbc picks the manager by the message sender: a player sender → that player's manager (FUN_8024ba90 = player index), anything else → player 1's.
- The station's arrival sequence starts its camera (003c) with P2 as sender (P2 spawns on the pad) and stops it from a timer → only P1's manager was checked. P2 kept cinematic uid 0x44f: no control, frozen view.
- Fix: the non-player "stop" branch checks every player's manager (FUN_8005c024 on a manager whose current camera +0x2c→+0x204 is this camera), then jumps to 0x801db25c. After resume, both managers are clear, P2 has a normal first-person view and moves.
- Arriving on a pad: whenever the pad is armed and someone stands on it, the station's save prompt opens (that player's view shows the station animation). Either player's save works.

### Code space
- All features are now installed as **one combined Gecko code** (`coop_patches.py gecko-all`; `install-gecko` writes only that) — 3-line gate + terminator once.
- List end ~0x80002ff0, i.e. 16 bytes left (Dolphin's limit 0x80003000; `combined_lines()` asserts).
- Shrinks made to fit: the friendly-fire player check is a loop; the P1→P2 copy is a loop with a shared copy routine.
- `COOP_WITHOUT=F` (env) leaves features out for debugging; `DEBUG_MARKS` adds counters at 0x80003cf0 (off).
- **The next features (item/scan sharing) will need code space freed first.**

## 2026-10-22 (session 19): more code space (hook bodies in MEM2), shared pickups (code H)

### Code space: hook bodies moved out of the Gecko list
- Dolphin's Gecko list must end below 0x80003000. Hook bodies now live in **MEM2 at 0x93700000+**.
  - That's above the game's MEM2 arena hi 0x935e0000 and the IOS heap 0x935e0000–0x93600000. The IOS-reserved range is all zero under Dolphin's HLE IOS. MEM1 has no gap: arena hi = FST start 0x817fa320.
- The bodies are written by Dolphin's **[OnFrame] patch engine** (host side, no size limit, applied each frame). Conditional form `addr:dword:value:comparand` also works (tested); bodies are written unconditionally so they can't go stale.
- The Gecko code keeps the gate, the single-word patches, and per hook a C2 stub `lis r12; ori r12; mtctr r12; bctr`. Each body ends with the same far jump back to site+4.
  - r12 and CTR must be free at the site. Checked for all sites: they replace a call, sit at a function entry/epilogue, or r12 is dead.
  - **Exception:** 0x800ddc28 (message input) sits in a bdnz loop (CTR = loop counter). Its far jump broke the loop: the front-end MP menu crashed reading 0x588 at 0x800ddc24. It stays an inline C2 body (`INLINE_SITES`).
- Gecko list now ends ~0x80002578 (was 0x80002ff0); bodies 0x93700000–~0x93700e00.
- `install-gecko` writes [Gecko] + [OnFrame] (+ _Enabled); `onframe-all` prints the bodies.
- Release: `local/release/echoes-coop-R3ME01.ini` (both sections) + README.
- Regression after the move (fresh boots):
  - resume from slot 2: save room, both players with the saved items;
  - friendly fire blocked;
  - death respawn next to partner at floor height;
  - P2 walks through the Landing Site door → room switch, P1 follows at floor height;
  - the gunship save prompt appears on arriving at the Landing Site ship (No dismisses it).

### Code H: item pickups go to every player
- CScriptPickup: vt 0x804b8498; Think vt+0x14 = FUN_801f7d04 (MP-aware attraction: loops players, sets +0x1b8 = nearest); Touch vt+0x50 = **FUN_801f8b2c**(pickup, other, mgr).
  - Fields: item +0x150, amount +0x154, capacity +0x158, +0x15c added to item 0x2f (pickup counter); +0x1c8 bit 0x10 → set variants FUN_8016d984/FUN_8016de44 instead of InitPowerUp/IncrPickup.
- Touch gives the item to the toucher's state only. **Hook 0x801f8d78** (`mulli r0,r25,12` right after the grant; r23 pickup, r24 mgr, r25 item, r30 toucher index) repeats the same grant for every other player's state.
- `pickups.py` lists pickups. Temple Grounds 00_pickup_temple_b (be94bd2c, area 0x21): Missile Expansion (44, +5/+5) at (227.7, 16.6, -28.7) and Energy Tank (42) at (218, -68.6, -41.3).
- Live:
  - P1 takes the Missile Expansion → P1 and P2 33/40 → 38/45, counter 0x2f 1/1 each.
  - P1 takes the Energy Tank → both 1/1.
  - Direct Touch(pickup, P2) → both +5.
  - P2 *walks* into the Energy Tank (placed 4 units away, stick forward) → both 1/1.
- Teleporting a player onto a floating pickup doesn't collect it reliably (the player drops past it before the touch pass). Not a P2 problem: the touch pass (FUN_80240720 0x80240888–0x80240944) is generic and pairs P2 with other actors (debug pair log, `touchlog.py`).
- Leaving the room and returning: both collected pickups exist but are inactive → collection persists (world layer state, which is saved).

## 2026-10-22 (session 20): shared scan progress (code I)

- Scan progress is per player: CPlayerState +0x594 count, +0x59c entries of 8 bytes {u32 scan asset id, **u8 progress**, pad}. All 821 scan ids are pre-listed (progress 0); 0xff = complete.
- **SetScanTime FUN_8016e39c**(state, scan id, f1 fraction) finds the entry, scales f1 by a constant (`fmuls f0,f0,f31`), quantizes it with `psq_st` and stores one byte at +4. GetScanTime FUN_8016e29c returns it divided back.
  - Callers: the scan update 0x8014a474 (progress every frame, then completion) and SetScanningState 0x80149e8c (resets partial progress when a scan is abandoned).
- **Code I:** both `bl SetScanTime` sites → a wrapper calling it for every player's state with the same id and fraction. So a scan by either player shows as scanned for both (logbook, scan colours) and is saved for both.
- Test (`scans.py` dumps/compares the maps):
  - The installed body was copied to scratch MEM2 behind an LR-saving wrapper and called as player 2 completing the Landing Site statue (scan id d1c33eca, actor uid 0x042a at (-84.1, -161.9, -44.2)) → both players' entry 0xff.
  - A full controller-driven scan wasn't done: scripted P2 lock-on wouldn't engage (face.py/key holds), and P1 turning failed in this boot's IR mode. To confirm in real play.
  - Test pitfalls: a hook body called directly must not return via `blr` (its calls clobber LR; it exits via `bctr` to site+4 in the game). Writing floats into the map corrupts the padding bytes (harmless).
- The resume copy (code B) doesn't copy the scan map; it doesn't need to, since both maps are kept identical live and both are saved.
- File 3 (co-op save) currently holds a Landing Site gunship save from these tests: the gunship prompt's "No" selection didn't move.

## 2026-10-22 (session 21): rough edges

- **Cinematics P2 triggers (code D, 4 words):**
  - The camera message handler FUN_801dafbc puts a camera started/stopped by a player into that player's camera manager (index from FUN_8024ba90 → `rlwinm r0,r3,2` at 0x801db154 / 0x801db22c / 0x801db2b8 / 0x801db310).
  - Each of those → `li r0,0`, so every camera goes to player 1's manager, as in single player, and plays full screen via the existing code D draw/layout change.
  - Live: P2 alone on the save station pad → camera 0x454 on manager 0 only, full-screen "Do you want to save?", afterwards both managers clear and split screen returns. Session 18's camera-stop hook stays as a safety net.
- **Pause / quit:**
  - The MP pause menu is on Wii Remote **button 2** (SetPauseState 3 from FUN_800e6f08, game action 0x43; + and − don't pause). Code E's pause gate now lets states 0, 1, 3, 5, 6 through (1 = from action 0x44 in FUN_80241530, probably the map).
  - Menu in P1's half: "Quit Game? < No >", Soundtrack, Hunters. Yes → "Are you sure?" → Yes → back to the multiplayer Game Mode menu (front-end world daea74a3) without errors.
- **P2 falling down the elevator arrival shaft:** happens during the full-screen arrival cinematic (P1 is held by it, P2 isn't). Invisible, and the arrival script then puts both at the exit. Left as is.
- **Respawn and elevators:** Great Temple Transport B with the pad armed and both players off it → P2 killed → respawns on P1's spot, no ride. The earlier "respawn launched the elevator" was P2 standing on the pad when it armed; any player on an armed pad rides, as in single player. Not a bug.
- Menus need relative IR on Wii Remote 1 (`irmode.py rel`): with absolute IR left on from scan tests, to_campaign.sh stalls on the slot menu.

## 2026-10-22 (session 22): controllers

- Connected: two Xbox Series X|S controllers (USB, VID 045E PID 0B12), XInput pads 0 and 1.
- `controllers.py [--pads 0 1]` writes WiimoteNew.ini (Dolphin closed). Each input is `` `XInput/N/Gamepad:Control` | key ``, so the test harness's keyboard keys still work. Mapping in the release README.
  - IR on the right stick, absolute (Relative Input False).
  - Dead zones 15% on IR and the Nunchuk stick. Without them the resting right stick (~3% off centre) drifted the pointer into a corner in relative mode and broke the menu script.
  - Rumble: Motor L | Motor R.
- Harness: `to_campaign.sh` sets P1 IR relative before launching (its keyboard pointing needs it). After closing Dolphin, run `irmode.py abs` to give the controllers absolute pointing back. Dolphin rewrites WiimoteNew.ini on exit, so edit only while it's closed.
- Verified: keyboard still drives both players with the new file; with both pointers absolute and the controllers at rest, positions and facing stay exactly constant over 6 s. **Not verified:** physical button/stick presses on the controllers (needs someone holding them). Backup of the keyboard-only file: `<workspace>/WiimoteNew.ini.keyboard-only.bak`.

## 2026-10-22 (session 23): script upgrades, safe zones, portals, real scan, map

### Code J: upgrades given by scripts
- Special functions 31–34 (PTMF table 0x804b8958 + 12·type → FUN_8020f780 incr amount, FUN_8020f838 incr capacity, FUN_8020f8d4 incr both, FUN_8020f9ac set) handle ACTN by giving the item to **one** player: FUN_802147ec(selector +0x170, &sender, mgr) = the sending player if +0x170 == -1, else player +0x170. Item +0x208, amount +0x174; then FUN_80241a98(mgr, player, item).
  - Hook at each entry: run the handler once per player with +0x170 = i (enter via displaced `stwu r1,-0x20(r1)` + far jump to entry+4), restore +0x170.
- Entity vt 0x804b8678 (CEntity only): AcceptScriptMsg FUN_802031e4 on ZERO does InitPowerUp(state0, +0x20, +0x28) and IncrPickup(state0, +0x20, +0x24) for player 1 only. Hook 0x8020322c loops every player's state, then continues at 0x8020324c.
- FUN_80065290 (vt 0x804b4560) is MP-rules bookkeeping that loops players itself; left alone.
- Test with stand-in objects in MEM2 (handlers read only the fields above):
  - special function 33, missiles +5, selector -1 sent by P2 → both 50→55;
  - selector 0 → both 55→60;
  - ZERO grant Light Beam → both 0→1.
- Not tested with a real in-world special function (none found in the rooms visited).

### Safe zones (no change needed)
- Sky Temple Grounds 0n_temple_hall_dark (686f5a1b, area 0xd): safe zones are RSO class vt 0x80f98bb8 (types 0x5f/0x5d/0x5c, trigger subclass). Active zone uid 0x41f at (-234.8, 42.1, -34.4).
- P1 in zone, P2 out → P1 +0, P2 -20 over 6 s; swapped → P1 -10, P2 +0. Triggers track every touching actor.

### Portals (no change needed)
- Only docks (FUN_801e6a84/801e6df4/dock message FUN_801e64bc), spawn points (FUN_8020c5f4) and the arrival spawner (FUN_8023f6c0) call SetCurrentArea. No portal-specific area switch exists (the PRTT FourCC exists, its loader wasn't traced).
- Spawn point ZERO loops over **every** player (load the target area if needed, SetCurrentArea, teleport each player to the spawn transform). Live: ZERO to a spawn point in another Agon area (0x39) → both players moved there and the current area switched 0x33 → 0x39.
- So a portal (spawn point, dock or world teleport) brings both players along.

### Code I, completed and tested on the real update path
- Third SetScanTime caller 0x8014a2d4 (scan update, resets progress when the target is a player) hooked too.
- Scan start needs, in FUN_80149f3c: action 0x42 held (FUN_8014ed30) **and** player +0x390 == 1 (orbit locked), with target player+0x39c.
  - Scripted lock-on didn't engage (not an input problem: Nunchuk C morphs, B jumps).
  - Test: patched the 7 `lhz rX,0x39c(r30)` reads in the scan functions to `li rX,<statue uid>` and 0x80149f94 to `li r0,1`, held P2's lock button with only P2 in the Scan Visor → the game's update went state 1 → 2 in 1.5 s, and **both** players' entry for the statue (scan id 1508f6ed this boot) became 255.
  - Patches reverted.
- Note: `scans.py` progress byte at entry+4. Scan ids per object: actor vt+0x78 → scan info +0 (the statue's id differs between objects/boots; look it up).

### Map
- The map is pause state 1, requested by FUN_80241530 on action 0x44 (Wii Remote **1**) only when player count == 1 (`cmplwi; bne` at 0x8024184c/54).
- Forcing it in co-op crashes: invalid write to 0x208 at 0x800e8bfc (the map screen object doesn't exist under MP rules). Kept the game's block; code E's pause gate no longer lists state 1 (now 0, 3, 5, 6).
- Not feasible without building the SP pause/map screen under MP.

### Harness notes
- `to_campaign.sh` leaves P1 relative; after closing Dolphin, run `irmode.py abs` (done; the controller config is in place again).
- Dolphin needs focus for keys; Wii Remotes time out when idle (the harness's J / NUM5 presses reconnect them).

## 2026-10-22 (session 24): map screen in co-op, and events that open it

### What opens the map (or similar screens) in single player
- SetPauseState callers with states 1/2:
  - FUN_80241530 (map button, Wii Remote 1, action 0x44; only when player count == 1);
  - **special function type 5** FUN_8020e598 (map station: on ACTN it reveals the rooms of connected objects through mgr+0x1e28 via FUN_800f5970/FUN_800f15a8, then SetPauseState(1));
  - **special function type 37** FUN_8020fbe4 (SetPauseState(2)).
- Under MP rules the original SetPauseState ignores 1 and 2, and code E's gate doesn't list them, so these requests do nothing. mgr+0x1e28 exists in co-op (0x807b32c0), so the reveal step is safe.
- Fresh boot, release codes: stand-in type 5 and type 37 objects sent ACTN → no crash, pause state stays 0, normal play. **A map station in co-op downloads its map data but doesn't open the map; nothing blocks progress.**
- (An earlier attempt crashed at 0x80240994 because it loaded save slot 1 from before a code change: old Gecko stubs + new MEM2 body layout. Save states don't survive code changes.)

### Map screen experiment (code K, not in the release; kept in <workspace>/coop_patches.map-experiment.py)
- The map lives in the HUD: FUN_800e8bcc writes HUD0(+0x60 of the HUD container)->+0x30->+0x208 and switches the HUD to state 2.
- The HUD constructor FUN_80156d7c builds the 0x3ac-byte map/pause frame object (FUN_80025aa8, vt 0x804b36c4) only when HUD+4 (a single-player flag) is set; the MP HUD gets 0 → crash.
- Experiment:
  - 0x80156e88 beq → nop (always build it) + 0x80241854 bne → nop (map button with 2 players) + pause gate allows 1.
  - Map opens in P1's half without crashing: "Light Map / <room name>" frame, controls bar, A toggles World Map ("Agon Wastes") / Level Map, B closes, play resumes, friendly fire still blocked.
  - **The map itself isn't drawn** (pale panel).
  - Also forcing the HUD texture list FUN_8015957c (0x80159598 beq → nop, gated on the same HUD+4 flag) loads map textures: a green map icon appears, but still no room geometry.
  - MAPW is requested on every world load (FUN_802864a0 / FUN_80287938) with no MP test, so the data should exist. Forcing the full-screen view didn't help (the frame doesn't redraw while paused on the map).
- Remaining suspects: the automapper's draw or its MAPA loading being skipped for the MP HUD/viewport (FUN_800ed620 / FUN_800eef14 request MAPA). Not resolved.

## 2026-10-22 (session 25): the map works in co-op (code K)

Reference: single player (Trilogy "Play Single Player" → Echoes) opens the same map with room outlines; the automapper draw inputs there: alpha 1, placement matrix diag(0.91, 1, 0.91) at depth -17.48.

### Why the co-op map was blank (three separate causes)
1. **Backdrop panel:** the HUD's map-mode draw calls a frame widget at 0x80157c14 (`bctrl` to (HUD+0x2c)->+0x4e4->+0x14 vt+0x24). In single player it's the dim backdrop; the multiplayer HUD frame's widget is a pale panel drawn over the map. Found by NOP-bisecting the calls between 0x80157a08 and 0x80157c14 (panel luminance 368 → 62 for that call only).
2. **Depth:** the map draws with z-compare 6 (GEQUAL, `li r4,6` at 0x80157c84 → FUN_803725d4). In single player the paused screen behind it is a captured image; in co-op the live world fills the depth buffer, so the map geometry loses. GX_ALWAYS (7) shows it.
3. **Placement:** HUD+0x1e4 (map placement) is recomputed every frame (FUN_80157320) from anchor nodes of the HUD frame model; the multiplayer frame's anchors give scale 0.35 at depth -36 → a few pixels. Single player's own matrix is also too small in the half-height viewport; scale 40 at single player's depth (-17.48) gives a normal size, +/- zoom on top.
- The automapper itself was fine: MAPW/MAPA loaded (state 2), update FUN_8002b824 / draw FUN_8002a3b0 run every frame (counted), visited rooms recorded per area (FUN_800f6754: areas 0x1b, 0x1c, 0x1d, 0x31, 0x33, 0x39 in Agon).

### Code K
- Words:
  - 0x80156e88 nop (HUD ctor builds the map frame object +0x30 even when HUD+4 = 0);
  - 0x80241854 nop (map button with 2+ players);
  - 0x80159598 nop (HUD texture list, map icons).
- Code E's pause gate lets state 1 through again.
- Hooks, all only with player count >= 2:
  - 0x80157c14 (inline, CTR live): skip the backdrop widget;
  - 0x80157c84 (inline): z-compare 7;
  - 0x80157cbc (`bl MTX_CONCAT`): concat with MAP_XF_MP (40, 1, 40, depth -17.479).
- Live, fresh boot, Agon Central Mining area and Temple Grounds save room:
  - Level map shows the visited rooms (orange) with door markers, Samus's position, + zooms.
  - A → World Map (visited regions orange, unvisited purple, current room highlighted).
  - B closes; play resumes; friendly fire still blocked.
  - Save station prompt (5) → "saved" message (6) → 0 still works.
  - Pause menu (button 2, state 3; B closes) still works.
- Side effect: player 1's corner minimap now draws (the map frame object exists).
- Limits:
  - Only player 1's controller opens it (action 0x44 is read from P1).
  - It draws in P1's half.
  - The level-map rooms sit slightly off-centre and a small room starts small (zoom fixes it).
- The earlier crash (invalid read 0x50 at 0x8000bb04) came from a test script calling vt+0x78 on every entity, not from code K.
- Map stations: with code K, special function 5's SetPauseState(1) now opens the map after the download, as in single player (not separately tested). Special function 37 (state 2) is still blocked.

## 2026-10-22 (session 26): pre-test checks, boss PreThink, equal start items, 4 players, lives groundwork

- **Save file reset:** the test NAND save.bin was restored to the setup-time copy (no Echoes saves at all); my test saves are kept in `<workspace>/nand_backup\save.bin.test-saves-*`. A fresh co-op start now begins a new game from the empty third file (verified).
- **Both players dying at once:** they respawn together at full health where they died (each onto the other's spot; tested in a room with spawn points, with them switched off, and far from them), no black screen.
- **Boss (Boost Guardian, Torvus 06_swamp_boostballguardian_dark a49a572f, boss uid 0x538, fight trigger 056e at (14.6, 292.4)):**
  - intro cinematic full screen on P1's camera (code D);
  - fight runs, the boss hits whoever is in the arena.
  - P1 vs P2 in the arena, 60 s each:
    - no code F: 263 / 10;
    - Think-only code F: 713 / 90;
    - **code F + PreThink: 345 / 110, and with P2 first 485 / 209.**
  - The boss's module reads players[0] in 10 helper functions. Some run from **PreThink** (vt+0x10, loop FUN_80240c40, call `bctrl` at 0x80240d50), now wrapped by code F the same way as Think (the vtable call is redone because the far jump uses CTR).
  - P2 still gets roughly half P1's attention; the rest is boss code outside both passes. Not chased further.
- **Equal start items:** on a new game each player arrives at a different Landing Site spawn point, and spawn points carry different start items (P1 lacked Super Missile, 5 vs 10 missiles). Code A's first-spawn hook (0x8010d580) now gives player 1 its spawn point's loadout and copies player 1's health, suit and items to players 2–4 (player 1 spawns first). Verified identical; respawn and friendly fire unchanged.
- **4 players:**
  - Wii Remotes 3/4 enabled as keyboard remotes (M/N = A); `to_campaign4.sh`.
  - The campaign starts with 4 players in 4-way split screen; the patches already loop over the player count.
  - Door crossing: all four follow, spaced 2.5 apart.
  - World change to Agon works.
  - Friendly fire blocked between all pairs (P3→P1, P4→P2, P1→P4).
  - Harness call rate dropped from ~92/s (Landing Site) to ~45/s (Agon, 4 players) — a rough proxy only.
  - The 2-player controller config was restored (`<workspace>/WiimoteNew.ini.2p`).
- **Respawn logic (for lives):** deathmatch rules update FUN_800762e8 loops players with rules+0x4c + 0x14·i = {f32 timer, u8 dead}.
  - On death (state+4 bit 0x80 clear; the check is `rlwinm. r0,r0,25,31,31` at 0x80076400): mark dead, timer = respawn delay, and item 0x41 (65, death counter, saved with the state) += 1.
  - Each frame: timer -= dt; when < 0 and FUN_80140cec(player, mgr) → rules vt+0x1c SpawnPlayer (call at 0x80076450).
  - A lives rule can veto at 0x80076428–0x80076450 (skip to 0x80076498 keeps the player dead and re-checks every frame).
- `ftoggle.py` now finds code F's body in MEM2, but Dolphin's [OnFrame] engine rewrites MEM2 bodies every frame, so live edits there don't stick; A/B needs `COOP_WITHOUT=F` + a fresh boot.

## 2026-10-22 (session 27): shared lives (code L)

- Pool: u32 at **0x937f0000** (MEM2 below the hook bodies' end; the [OnFrame] engine doesn't touch it, and it isn't saved).
  - Size = rules+0x38 (player count) × LIVES_PER_PLAYER (2).
  - gs+0x158 is *not* the player count (it read 1 with 2 players).
- Display: item 0x40 (kills) amount and capacity on every player. The HUD's centre counters show it ("03 / 03"). The results screen's "Kills" shows the last pool value.
- Hooks:
  - **load_coop_save (code B)**, after the state copies: fill the pool (game state states at gs+0x18+8k).
  - **0x80076428** (`mr r4,r29`, dead player's respawn timer ran out):
    - pool > 0 → continue to SpawnPlayer;
    - pool 0 and someone alive (state+4 & 0x80) → 0x80076498 (stay dead, re-checked every frame);
    - nobody alive → 0x800764c0 (the kill-limit ending: rules+0x40 |= 8, vt+0x40, vt+0x3c).
  - **0x80076454** (`stb r25,0x4(r23)`, after SpawnPlayer): pool − 1, update the display.
  - **0x8020e840** (save station FUN_8020e800 after code G's nop; r30 = mgr): refill the pool. It runs on the station's ACTN, before the "Do you want to save?" prompt, whatever the answer.
  - **0x80076414** (`lfs f0,0(r23)`, every frame for a dead player): DisableFilter(0.5 s) on camera filter pass 1 of every *alive* player's viewport whose pass+4 (target type) != 0.
    - A death fades pass 1 to black on **all** viewports, and only a respawn cleared it.
    - So the survivor played blind while a partner waited out the respawn delay, and for good once lives ran out. The dead player's half stays black.
- Live (fresh boots):
  - pool 4 with 2 players; kills count 4→3→2→1→0, the HUD counters follow;
  - with 0 left, P2 stays down and P1's view stays lit;
  - at the Temple Grounds save station (pad step) the pool went to 4 and P2 revived (4→3);
  - P2 down + P1 killed with 0 left → results screen (Rank/Kills/Deaths) → A → multiplayer menu (world daea74a3);
  - Sidehopper Station again → resumed in the save room with pool 4.
- Test notes:
  - `lives_test.py` (status / kill K… / set N / watch) and `pad_step.py` (step a player off/on a pad with the game's teleport).
  - The save room re-runs its station sequence while players stand on the pad, and damage doesn't land during it; test deaths elsewhere.
  - The test NAND save was restored to the no-Echoes-saves copy (`save.bin.before-lives-test` = `save.bin.20261022-052038`).

## 2026-10-22 (session 28): a player down — transitions, saving, spectator view

- With P2 held down (pool 0), all checked live:
  - **Door** (dock_cross P1): P2's body follows into the new room and stays down.
  - **World change** (WorldTeleporter ZERO, Temple Grounds → Great Temple 863fcd72/8f01b104): P2 comes along and stays down, P1 normal.
  - **Cutscene:** the save station's full-screen camera ran with P2 down and play recovered (session 27).
  - **Save while down:** the save prompt fired with P2 at -401 HP and was saved. A fresh boot + Sidehopper resumed with both players up, 99 HP, identical items, pool 4.
  - Nothing needed fixing.
- **Spectator view (code L, 2 more hooks):**
  - The black half is not a camera filter: with P2 down, every filter pass was off.
  - Swapping P2's camera manager to P1's didn't help either.
  - The **HUD object** for the viewport (game object +0x18 → +0x60 + 8·vp, drawn by FUN_801577fc from FUN_800dd98c) covers a dead player's viewport in black.
  - Pointing both HUD slots at P1's HUD showed P1's view in both halves.
  - Live code writes don't reach Dolphin's JIT (a NOP at 0x800dd9dc changed nothing), so code tests need a reboot. Data writes do work.
  - Hooks, only while the pool is 0 and player vp is down (`emit_spectate_target`: vp → first player who is up):
    - **0x8024b844** in FUN_8024b72c(mgr, vp), "select viewport for drawing": sets mgr+0x1da0/+0x1da4/+0x1da8 (current player/state/camera manager) from the partner, so the world is drawn from the partner's camera with the partner's arm cannon. +0x2c78 (viewport index, screen area) stays vp.
    - **0x800dd9cc** in FUN_800dd98c: draw the partner's HUD in that viewport.
    - The unfade hook also lifts a downed player's pass 1 while the pool is 0. During a normal respawn delay their half stays black as before.
  - Live: P2's half mirrors P1's view and follows P1's movement and world changes. After a save station refill P2 respawns with their own view again. Everyone down → results screen as before.
- Test NAND save restored to the no-Echoes-saves copy (md5 6071efcd…).

## 2026-10-22 (session 29): lives from the menu, P2 cutscenes, copying saves, results screen

- **Lives per player from the Deathmatch Kill Limit:**
  - The menu offers 0, 5, 10 (default), 15, … in steps of 5, changed with the D-pad (Wii Remote 1 NUMPAD4/6; the arrow keys are the pointer).
  - The rules object gets +0x3c = limit and +0x40 bit 0x40 = enabled (0x70 at 10/15, 0x30 at 0).
  - load_coop_save stores lives per player = limit / KILLS_PER_LIFE (5) at LIVES_ADDR+4. 0 = off: pool = -1.
  - The fill (`emit_fill_pool`: session start and save station) multiplies by the player count.
  - The spend hook skips pools ≤ 0. The gate and spectate hooks only act on a pool of exactly 0, so -1 never blocks a respawn.
  - `to_campaign.sh` takes `KILL_STEPS=N` (±).
  - Live:
    - 15 → per player 3, pool 6;
    - 0 → pool -1, three P2 deaths all respawned, HUD counters 0;
    - default 10 → 4 (2 players).
- **Results screen:** when everyone is down, the gate zeroes item 0x40 for all players before the match end → "DRAW GAME", Rank 1 / Kills 0 for everyone.
- **Cutscene triggered by P2 alone:** Temple Grounds area 2 (b1140c24), trigger 046f at (24.5, -198.6, -40.9) arms the pit trigger at (35.8, -223.7, -42.7). With only P2 sent in (P1 elsewhere in the room):
  - cinematic cameras 0x41c → 0x4cb all on P1's manager (code D), full screen;
  - the script moved both players into the lower room;
  - split screen returned, both in control.
  - Without arming via 046f, the camera ran but a filter pass 2 stayed black on both viewports. That's the room script's own order, not co-op (it never got its DCTV).
  - `cine_scan.py --explore N` walks loaded areas and lists ENTR trigger → camera chains. In Temple Grounds areas 0–0xd: 0, 2, 7, 8, 9, 0xa, 0xd.
- **Copying a single-player file for co-op:** Trilogy Game Select → Copy Slot → warning (Friend Credits/Vouchers not copied) → Yes, Confirm (hover Confirm, then A) → from Slot 1 → to Slot 3.
  - The save gets a byte-identical GMST record at 0x5d84 (slot 2, the co-op slot).
  - Co-op then resumed from it (save station, both players with its items).
  - Pointer: push it to the top-left corner first, then move by key-hold time (~1 px per 1–1.5 ms).
- Not done: the boss's attention to P2 (partly a boss-code issue, left) and a full elevator ride with its cutscene (needs story progress).
- Test NAND save restored (6071efcd…); WiimoteNew IR back to absolute.

## 2026-10-22 (session 30): playtest fixes — corner map, gunship save loop; gate lock investigation

- **Map drawn with the map closed (code K, new hook 0x80157b84):**
  - The HUD map block (FUN_801577fc, `cmpwi r5,0` at 0x80157b84) also draws the corner minimap in normal play.
  - Since code K builds the map frame object, that minimap went through the map-screen placement and showed as a large wireframe over player 1's view.
  - With 2+ players the block now runs only in pause state 1 (map open).
  - Live: closed → no overlay; button 1 → map; B → gone again.
- **Gunship save loop (code D, new hook at the entry of FUN_8005c1d0 "cinematic camera active"):**
  - The ship's save sequence (Landing Site trigger editor id 0x10d → relay → sequence) ends by sending ZERO to a spawn point on the ship.
  - Spawn-point ZERO moves every player. A player who walked off during the cutscene was put back on the ship, which counts as entering the save trigger: a new sequence and a new prompt, endlessly.
  - In single player Samus can't move during it. In co-op only player 1 was held, because code D puts every cinematic camera on player 1's manager and the player code asks its own manager (+0x12f0).
  - Hook: for callers in 0x80140000–0x80164000 (player/HUD code) with 2+ players, test player 1's manager.
  - Live (`ship_walk.py`): P2 held T throughout the sequence and didn't move. One prompt; after the cutscene both walked off; no more prompts.
- **Gate lock (not resolved):**
  - Scan points (SCND senders) are in the same active state in co-op as in single player at game start: `cine_scan.py --dump` over 30 rooms, diffed by editor id (only 5 unrelated differences).
  - The area-2 lock (editor 0x2004a) is switched on by player conditional 0x2005c (vt 804b7ee8: item 0x61 > 0 on player 1). It's inactive in single player at the start too, and nothing in Temple Grounds grants item 0x61, so it's later progress.
  - Area 0xa lock 04ce (editor 0xa0075): scan-complete called directly (SetScanningState 2 then 0) showed the scan text, then turned off actor 042a and the relays, as single player would.
  - A scripted Z lock-on didn't engage in later boots (orbit 0), so a real hands-on scan of it wasn't reproduced here.
  - Needs the room from the players.
- New scripts: `scan_try.py`, `lock_test.py`, `specfuncs.py`, `ship_loop.py`, `ship_walk.py`; `cine_scan.py` got --scnd, --item and --dump.
- **Gate lock mechanisms can't be scanned (root cause found, code E word 0x80166a88):**
  - Live with the players at the Temple Grounds area-6 gate (MREA 50b372a5): the mechanism actor 0baf (editor 0x60082) was active, had scan info (SCAN 9e1c7bc8) and was unscanned.
  - Lock-on never engaged for either player in the Scan Visor (`scan_watch.py`: orbit 0, no target).
  - What you lock onto is a point of interest (vt 804b79a8, type 0x22: SCNS/ATCH to the mechanism).
  - Lock-on candidate update FUN_80166a38: in the Scan Visor (state +0x30 == 2) single player uses the scan target search FUN_8016a8fc → player +0x3c8. Under MP rules a `bne` at 0x80166a88 after IsMultiplayer falls back to the combat lock-on search, which skips scan-only points. Everything else scanned so far was also targetable in combat.
  - Fix: 0x80166a88 → nop. Installed and exported, not yet confirmed in play.
  - The first gate was passed by calling SetScanningState(P2, 2) with P2 +0x39c/+0x5b0 = 0baf from outside (scan text shown; the gate script ran).
- Lock-on code map: SetOrbitTarget FUN_80168598 (+0x39c), SetOrbitState FUN_80168670 (+0x390), orbit update FUN_801651b0 (takes the candidate from +0x3c8).
  - **Tried and reverted (2026-10-23):** with 0x80166a88 → nop, player 2 in the Scan Visor got no candidate at all (+0x3c8 = 0xffff), and the players found previously scannable things hard to scan too.
    - FUN_8016a8fc picks from a buffer of object ids rendered on screen (player +0x138c, resolution halved under MP and again with 3+ players, gated on player +0xec0 != 0). That buffer evidently isn't filled for player 2 in co-op.
    - Player 1 wasn't tested with the patch.
  - Next idea: run the scan search and fall back to the combat search when it finds nothing (never worse than before). Then find why the id buffer isn't rendered for the other viewports.

## 2026-10-23 (session 31): Scan Visor targets scan-only objects in co-op (code E)

- **Cause:** in the Scan Visor the lock-on candidate (player +0x3c8) comes from the scan target search FUN_8016a8fc. It reads a per-player 64×32 buffer of object ids (player +0x138c; resolution halved under MP and again with 3+ players).
  - The buffers are GPU copies of each viewport's region (FUN_8024bf80 → graphics vt+0x138, GXCopyTex), taken during each viewport's draw pass.
  - In split screen only player 1's copy ever held ids (player 2's stayed all zero; player 1's was intermittent).
  - Under MP rules a `bne` at 0x80166a88 skipped the scan search entirely, using the combat search, which never offers scan-only points (gate locks, map stations).
- Attempts that didn't work:
  - The nop alone (P2 found nothing).
  - An extra readback after all viewports, or after the id picture.
  - Copying only the current viewport's region (also emptied P1's).
  - The real fix of the GPU copy timing is unresolved.
- **Fix in code E:**
  - 0x80166a88 → nop (use the scan search).
  - 0x80148578 → nop (FUN_8016a410, the scan aim data update, runs for every player, not only mgr+0x1da0).
  - Hook at 0x80166aac (`scan_search_fallback`, 2+ players): when the search returns nothing, pick the active actor with scan info (+0xbc → +8) nearest the first-person camera's aim. Camera manager +0x14, eye = transform col 3, aim = col 1 (includes pitch); within 40 units, cos² ≥ 0.96, players excluded. Nothing there → the combat search as before.
- Live (absolute IR for P1; relative IR makes P1 keep turning from a leftover pointer offset — harness only):
  - P2 locked onto the room-6 gate mechanism → "Lock armor disengaged" → both shootable locks active, logged for both.
  - P1 scanned the Map Station.
  - Statue: P1 scanned it, then P2 (instant completion, already logged).
- Harness:
  - The boot keys need Dolphin in the foreground; with the user's controller config (Wii Remote 1 on SDL Xbox) the keyboard can't drive the menus. Test boots set Wii Remote 1 to `DInput/0/Keyboard Mouse`, restored after.
  - `to_campaign_from_menu.sh` resumes from the main menu.
  - `dolctl.py launch 7` boots the in-game state (for absolute IR).
  - **The user's save-state slot 3 was overwritten at 05:47 during testing** (cause not found, probably a hotkey from a simulated key); restored from `<workspace>/state_backup\R3ME01.s03.user-playtest-20261023`. Back up all user slots before tests.

## 2026-10-23 (session 32): bosses and player 2 — message handling (code F)

- **Why the Boost Guardian favoured player 1 even with code F:** `vt_players.py <uid>` maps each virtual of a loaded enemy to the players[0]/state[0] reads reachable from it inside its module. Boost Guardian (vt 80f13520, module text ~80efe000–80f19000):
  - vt+0x14 Think (covered by F);
  - **vt+0x18 AcceptScriptMsg**:
    - `80f0d950`: "sender is a weapon owned by players[0]" (reactions to being shot only for P1's shots);
    - `80f0132c`: distance to players[0];
  - vt+0x9c (from CPatterned::Think, covered, and the radius-damage path 0x802432f4);
  - vt+0x124 (collision code; fallback target = players[0]);
  - vt+0x184.
- Counting `lwz rX,0x14f4(rY)` per RSO (MP2/RSO/Production): Dark Samus 46, Sandworm 38, Grenchler 36, Digital Guardian 34, Sand Boss 29, Splinter 28, Emperor Ing 3 23, Swamp boss 20/21, Boost Guardian 7.
- Messages are delivered by FUN_8024b388 (immediate, `bctrl` 0x8024b3d0) and the queue pump FUN_80244c08 (mgr+0x8e4, `bctrl` 0x80244c70). Everything sent with FUN_8024b3ec, including damage from ApplyDamage, goes through the queue. Message: +2 sender uid, +4 target uid, +8 kind.
- **New code F hooks at both sites (`relevant_player_message`):** for a CPatterned with 2+ players, swap into slot 0 for the call:
  - the sender, if it's a player;
  - else the owner of a player's weapon (CastToWeapon +0x2c8);
  - else the nearest player.
  - Then swap back. Installed; **not yet tested** (needs a fresh boot).
- **Harness bug found:** remotecall.install() flushed the JIT with a save/load of state slot 3, which overwrote the user's slot-3 save state (05:47 and 06:27). It now uses slot 8 (`HARNESS_SLOT`); slot 7 is to_campaign.sh's. User's slot 3 restored from `state_backup/R3ME01.s03.user-playtest-20261023`.
- **Tested (fresh boot, Boost Guardian):**
  - The message wrapper alone didn't change the A/B: P2 alone 75–107 vs P1 232–319 per 25 s. The A/B doesn't shoot the boss, so the weapon-owner check never came into play.
  - Real cause: the boss's **stored target uid +0x1098**. Its only writer (collision handler, module `sth r0,0x1098(r30)`) sets it from players[0] when invalid, then it never changes. It stayed 0x400 (P1) with P2 alone in the arena: the boss followed P2 (7–21 units) but aimed its attacks at P1 (110 away).
  - Forcing +0x1098 = nearest from Python: P2 alone took 299 vs 45–50.
- **Fix (code F):** `STORED_TARGETS = [(117, 0x1098)]` (type id from the IsA chain, checked with CAST_ENTITY). In the Think/PreThink nearest-player step, when the field holds a player's uid, it's set to the nearest player's.
  - Live A/B, 3 rounds: P1 alone 370/330/185, P2 alone 257/370/160 (avg 295 vs 262). The target follows the arena player.
  - Released with the message wrapper.
- Other bosses (Dark Samus, Sandworm, Grenchler, Digital Guardian, Sand Boss, Swamp boss, Emperor Ing…) likely have their own stored targets. Method: `vt_players.py <uid>` on the loaded boss, find `sth rX,OFF(rY)` of a players[0] uid in its module, add (type id, OFF) to STORED_TARGETS, A/B with boss_ab-style rounds.
- Harness: Wii Remote 1 idle-disconnects mid-test; press J/NUM5 at the start of each round.

## 2026-10-23 (session 33): all bosses — stored player targets (code F table)

- `rso_targets.py`: static scan of every MP2 RSO for "players[0] → lhz 8 → sth OFF(obj)". Hits:
  - Blogg +0x9b0, ChozoGhost +0x31c, CommandoPirate +0xbe2, DarkCommando +0xca4, DarkSamus +0x700 (×2), DigitalGuardian +0x1052, FlyingPirate +0xacc;
  - Ing +0xd7c, IngBoostBallGuardian +0x1098, IngSnatchingSwarm +0x3f6, IngSpaceJumpGuardian +0x9f0, MediumIng +0x700, Rezbit +0xca2;
  - ScriptCannonBall +0x122, SpacePirate +0x31c, Splitter +0xf0e/+0xcd8.
  - RSO `bl`s are unrelocated (0x48000001), so callers can only be found live.
- Type ids are fixed: the IsA functions are in the DOL (e.g. 0x8029e774 `cmpwi r4,117`).
- `world_scan.py` / `scan_all.sh`: world teleport into every room listed in each world's pak (pak order = area index; worlds 3bfa3eff Temple Grounds, 863fcd72 Great Temple, 42b935e4 Agon, 3dfd2249 Torvus, 1baa96c2 Sanctuary).
  - Records module CPatterned classes (type id, RSO by Think-code match) to `<workspace>/boss_scan.jsonl`.
  - Rooms create objects 6–9 s after the jump (the scanner waits until the count is stable).
  - Two rooms hang on a direct jump: Sanctuary 70e48580 and Temple Grounds 87d35ee4 (driver restarts and skips).
  - 282/284 rooms done.
- Bosses present in a fresh game: Dark Samus 111 (Sanctuary 5d3a0001/df073157), Quadraxis 112/113 (Sanctuary a92f00b3), Jump Guardian 118 (Agon 6b77f9bd), Alpha Blogg 149 (Torvus), Sandworm 130 (Agon), Emperor Ing stage 1 (type 4 only, Great Temple d7c3b839), Boost Guardian 117, Grenchler 115, Elite Pirate 114.
  - Not present without story progress: SwampBoss (Chykka), Spider Guardian, MediumIng, ChozoGhost, FlyingPirate, Dark/Commando pirates.
- `check_targets.py` (P2 6 units from the enemy, P1 60 away, sample the field):
  - Blogg +0x9b0 stuck at 0x400;
  - Space Pirate +0x31c took 0x400 over the nearer P2;
  - Dark Samus +0x700 switched to P2 by itself (not listed);
  - idle fields: Jump Guardian +0x9f0, Quadraxis 113 +0x1052 (112's +0x1052 = 5, not a uid), Ing +0xd7c, Splitter 134 +0xcd8 / 135 +0xf0e (the other offset = 0), Rezbit +0xca2 — all 0xffff.
- **STORED_TARGETS** = (117,0x1098) (149,0x9b0) (132,0x31c) (118,0x9f0) (113,0x1052) (116,0xd7c) (134,0xcd8) (135,0xf0e) (127,0xca2). The field is only rewritten while it holds a player uid.
- Live after the fix:
  - Blogg field → 0x401 with P2 near.
  - Space Pirate: target = near player in every sample; damage A/B (20 s): P2 near → 195/190, P1 near → 110/95.
  - The pirate's setter (module fn at +0x91b8 in the file) has no direct callers (called through a pointer). Code F rewrites the field before each Think.
- Not verified live: the Jump Guardian, Quadraxis, Ing, Splitter and Rezbit entries (their fights weren't started); the classes not present in a fresh game (no type id known).

## 2026-10-23 (session 34): scan text, frame rate, devices

- **P2 advancing scan text:** works (session-13 hook at 0x800ddc28). P2 scanned the statue, and P2's A alone (NUM5 = Wii Remote 2 A) paged and closed it. On Xbox controllers Wii A is RT; the Xbox A button is Wii B. That's the likely cause of the report.
- **Player drawn over the scan text:** reproduced. P2 standing in P1's view shows as a dark silhouette over the left of the text window (pause state 6, Scan Visor backdrop). Not fixed:
  - Disabling the window's depth test (`message_on_top` at 0x80157d20, GXSetZMode(0,7,0)) changed nothing. Left in the source, not installed.
  - Returning early from the player's vt+0x2c (0x80148a9c, live with a JIT flush via slot-8 save/load) changed nothing.
  - Frame order: world pass FUN_800e84d4 for all viewports (DrawWorld, 0x80157618, 0x80247054), then HUD pass FUN_800e864c (0x800dda88 over all HUDs, then HUD draw 0x800dd98c / FUN_801577fc, text window at 0x80157d20). Next suspects: the Scan Visor backdrop / camera filter, or the id picture.
- **Frame rate (`perf_rooms.py`, harness calls/s, about 90 max):**

  | Room | Before the change | Code F off | After the change |
  |---|---|---|---|
  | Landing Site | 84–89 | 89–90 | 89–90 |
  | Sanctuary | 87–90 | 89–90 | 90 |
  | Temple Grounds 16 enemies | 64–72 | 89 | 89–91 |
  | Torvus 14 enemies | 63–67 | 76–88 | 75–88 |

  - Cost was the STORED_TARGETS loop: 9 CAST_ENTITY calls per enemy in both Think and PreThink.
  - Now the own type id is read from the IsA function's `cmpwi r4,N` immediate (vt+0xc → word 0), compared inline, in Think only.
  - Re-checked: Blogg → P2; pirate damage A/B P2 near 245/285, P1 near 135.
- **Devices:** `dol_players.py` lists ~80 non-enemy DOL classes reading players[0] (mostly through the shared actor method vt+0x7c).
  - Only one class reads players[0] then its morph state (+0x378): vt 804b7f88, type chain [0], no actor, a game-wide manager. The rest is HUD/visor code.
  - No P1-only morph-ball device found. Devices use triggers (any actor), as tested for safe zones and save stations. Not tested device by device.
- Story-gated enemies: skipped (no later save available).
- **Silhouette over paused HUD screens — fixed (code K, 3 hooks):**
  - Paused frames (mode +0x20 = 2 in the frame object vt 804b57e0) still draw the live world pass FUN_800e84d4 then the HUD pass FUN_800e864c. Near geometry (another player, a pillar) was depth-nearer than the text window/frame (also the map frame).
  - Ruled out: the window's own GXSetZMode via the cached setter FUN_8030817c (model materials call GXSetZMode 0x803ec8d0 directly); 0x8015762c/0x80157618; the player's vt+0x2c.
  - Fix: HUD draw entry 0x801577fc sets MEM2 flag 0x937f0010 when mgr+0x2c0c != 0 with 2+ players. GXSetZMode 0x803ec8d0 forces compare ALWAYS while it's set. World draw entry 0x80245504 clears it and invalidates the depth-mode cache byte 0x8050d272.
  - Live: scan text clean with P2 in view; world normal after closing; map and pause menu fine; frame rate unchanged (89–91).
  - Pitfall hit: a wrong displaced word (0x81229cf8 instead of 0x8122cf08) crashed at boot. The site check in coop_patches caught it but the shell line installed anyway; now gated on the check.

## 2026-10-23 (session 35): map for any player, lives refill on save only, blank text box

- **Map, any player (code K word + code E input):**
  - The map button in FUN_80241530 (per controller input) also required input +4 (controller index) == 0: `bne` at 0x8024188c → nop.
  - Code E's input hook at 0x800ddc28 now forwards every controller to HUD 0 in pause state 1 (map) as well as 6 (message).
  - Live: P2's button 1 opened it, P2's A switched to the world map, P2's B closed it; P1's button 1 still works.
- **Lives refill only on an actual save (code L):**
  - The save prompt (state 5) goes to 0, then shows a message (state 6) for both answers:
    - station Yes 0x1dffe3e7, No 0x50b34a43;
    - gunship Yes 0x47816a80, No 0x0acdc324.
  - Refill moved from the station ACTN (0x8020e840) to the show-message entry 0x8024ac68, for SAVED_MESSAGES only.
  - Live: station No → pool 0, P2 stays down, save file untouched; station Yes → saved, pool 4 → P2 revived → 3; gunship Yes → same.
- **"Blank" text box at the second gate:** not reproducible. The gate's message (STRG 0xafc7aca5, a script message, shown in both halves) displayed at once.
  - The earlier "no text" was an unchecked guess.
  - The user's "froze" was most likely waiting for Wii A = Xbox RT (Xbox A is Wii B). README now says so.

## 2026-10-23 (session 36): smoke test of the release ini (md5 fe3665cf…)

- Fresh boot with the release file, all on one run:
  - new game start (pool 4); door crossing (P2 crosses, room changes, P1 follows);
  - P2 scans: statue (P2's A closes the text), gate lock (both shootable locks on), map station;
  - text box clean with P2 in P1's view; map opened/closed by P1 and by P2;
  - deaths 4→3→2→1; pool 0 → P2 held down; station No → nothing saved, still down; Yes → saved, refilled, P2 revived;
  - Boost Guardian: P2 alone 233, P1 alone 252, target follows the arena player;
  - frame rate 89–92 (Torvus 76–87, as before).
- Note: hits on players standing near the save station pad while its sequence runs don't land (scripted invulnerability), as in single player.

## 2026-10-24 (session 37): message boxes full screen (code D, 4 hooks)

- The message box (scan text, save/script messages; pause state 6) lives in HUD 0 and was drawn in P1's half, so long texts didn't fit.
- Code D's single-view switch (cinematics) skips the HUD pass entirely (FUN_800e864c `cmpwi r4,0; bne` → no HUD loop), so it can't simply be turned on.
- Hooks, all keyed on `emit_message_mode` (pause 6, 2+ players, world != front end daea74a3):
  - 0x800e88a4 and 0x8024b584: the old code-D `bl CINEMATIC_ACTIVE` words became hook bodies — cinematic OR message mode (single view, full-screen layout).
  - 0x800e8654 (`cmpwi r4,0` in the HUD pass): run the HUD loop anyway in message mode.
  - 0x800e870c (loop bound `lwz r0,0x14f0(r3)`): only HUD 0 in message mode.
- Live: statue text (4 lines) in a full-size box over P1's view; P2's scan likewise; P2's A closes; split screen returns after.
- Pitfalls hit:
  - 0x800e8654 sits right after `mfspr r0,LR`, so a body using r0 corrupted the saved LR. The front end went black and every boot stalled at Arena Select. The checks now use r11/r12 only.
  - An earlier Dolphin kept running because a failed check skipped the close step. Use `Stop-Process -Force` before each boot.

## 2026-10-24 (session 38): difficulty

- Difficulty getter **FUN_800bc668(gs)**: `gs+0x148` object's vt+0x44 returns the game mode tag; only for 'SNGL'
  (0x534e474c) does it return the global `*(r13-0x6440)+0x80` (0x805cb220 -> +0x80; set by FUN_800bc308 from new
  CGameState FUN_800ba8ac and at 0x800eb0ec). Otherwise `li r3,1` at **0x800bc6a0** = Veteran. Co-op's tag is
  'DTHM' (vt 0x804b4838, FUN_800767f8 `lis/addi 0x4454484d`, read from mem_cp_alive.bin), so co-op was always Veteran.
- Enum (Trilogy names): 0 Normal (new easy), 1 Veteran (GC Normal), 2 Hypermode (GC Hard). `HardModeCompleted` is
  set when difficulty == 2 at the ending (0x8013a72c); `HardModeUnlocked` after the first completion on any.
- Scales: FUN_800bc334 = 0.5 (r2-0x4490) / 1.0 / tweak+0x48 (1.53), used for damage to Samus (radius path
  0x80243000/0x80243010); FUN_800bc39c = 1.3 (r2-0x4474) / 1.0 / tweak+0x4c (0.5), used by FUN_80292110 in the
  damage builder FUN_802429ec (Samus's damage to enemies; knockback scaled by min(scale, 1)). Tweak object
  `**(r13-0x6420)` = 0x8083bdac in the dumps. Direction of the two scales inferred from the 1.53 / 0.5 Hypermode
  pair (same as Prime 1's hard-mode tweak values), not measured.
- Release: three separate gated Gecko codes "Echoes co-op difficulty: Normal / Veteran / Hypermode" each writing
  `li r3,N` at 0x800bc6a0; Hypermode enabled by default. Not yet tested live.
- **Live (fresh boot, Hypermode code on):** getter returns 2; FUN_802429ec turns P1 -> enemy 10 into 5.0 (damage,
  radius, knockback); ApplyDamage of 10 from an enemy (uid 062b, Temple Grounds d627000b) took P1 99 -> 83.7 (x1.53).
  So the 1.53 scale for damage to Samus is applied in the full path, not in FUN_802429ec.

## 2026-10-24 (session 38): playtest notes (item loss, charge pull, morph-ball door, room respawns)

- **Item loss:** Hive Chamber B (Temple Grounds MREA 0x1d7bd3ae, area 17). Trigger
  `0811005a` (type 93) --ENTR/SMAX--> `0011001c` (type 55) --MAXR/ACTV--> loss spawn point `08110021` (loadout Power
  Beam, Combat/Scan Visor, Varia, Morph Ball, Charge Beam), --PLAY/PLAY--> `08110020` (type 58). No RSET connection
  among the loaded objects, but with code A's raise-only RSET removed (hooks at 0x8020c65c/0x8020c67c dropped; vanilla
  handler loops over every player) both players lost Boost/Spider Ball, Bombs, Space Jump and Missiles 65 s after
  P1 was put in the trigger (live, `itemloss.py` + inline watch). The Landing Site intro RSET now gives both players
  the vanilla start (Missile 5/5, Spider Ball, ...). Spawn point ZERO = teleport every player there (r6 = 0).
- **Charge pull:** pickup Think loop 0x801f88f4: range 12 (MP, -0x7594(r13)) vs 30 (SP, -0x7598(r13)), picked after
  `bl IsMP` @0x801f88f8. Word 0x801f8900 `beq` -> `b 0x801f890c` (code H). Gate before the loop (0x801f87d0): +0x1c8
  bit 0x20 clear, bit 0x40 set (attractable: drops, not expansions), +0x184 >= 0, pickup in the current area; it sets
  +0x1b8 = player index and +0x1c8 |= 0x60. Charging fills gun (player +0x12ec) +0x44 to 1.0 (> +0x48 0.14).
  **Not live-verified:** no drop could be produced (ApplyDamage 500 didn't kill the RSO enemies 0x80923fb0, type-34
  objects aren't the crates, Think resets a forced +0x1c8 0x40 on an expansion).
- **Morph-ball follower:** FUN_80160cc4's last arg resets the morph ball camera (player +0x12f0 -> +0x18, vt+0x84);
  SpawnPlayer/arrival pass 1, spawn points 0. Code C's follower teleport and B's respawn teleport now pass 1. Live
  (Landing Site dock 0, P2 morphed, P1 placed past the plane): P2 moved 57.5 units into area 1, ball camera 6.4
  units from it on the next sample, then a 4-unit drop to the floor, no further motion.
- **Room respawns:** enemies, crates and drops return on re-entry as in single player; collected expansions stay
  collected (session 19). No change.
- Release ini md5 121bbe3c4a1a688881e7518358462d40.
