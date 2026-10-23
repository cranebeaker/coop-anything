# The Lord of the Rings: The Two Towers (2002, Xbox): co-op notes

The findings and evidence behind couch co-op on The Two Towers Recompiled (code:
[Volverine-Dude/TooTawersRecomp-coop](https://github.com/Volverine-Dude/TooTawersRecomp-coop), see its `COOP.md`). Two
players on one PC, one shared camera, one game process: no networking, so no desync.

- Every address is an Xbox virtual address in the US retail `Default.xbe` (identity below).
- **Observed**: seen at run time (logs, frame dumps, bot runs). **Derived**: read from the code, not checked at run
  time. **Unknown**: not found.
- The bot runs drive the heroes with scripted pads, steering test bots or the P2 Bot, and set situations up with
  `TT_COOP_*` test switches. The suite that repeats the main checks is in the fork's `tools/coop-bot/`.

## 1. Identity

| | |
|---|---|
| Game | The Lord of the Rings: The Two Towers, original Xbox, 2002 (EA / Stormfront Studios) |
| Media | US disc, extracted (`<game dir>\game_files`) |
| XBE certificate | title ID `0x45410010`, "The Two Towers(tm)", region `0x1` (North America), version 1 |
| `Default.xbe` | SHA-256 `ae2d1b5304b3e9572339b693a6a45ad0869bdaeec89aae374b5c14feebbb8d30`, 2,224,128 bytes (the hash the port's `generate.py` requires) |
| PC port | The Two Towers Recompiled 1.9.4 (xboxrecomp 0.12.0); work started on 1.9.2 |
| Renderer tested | Direct3D 11, 16:9 (Vulkan and 4:3 untested) |
| Host | Windows |

Engine (observed in the XBE's strings): Stormfront's "Fusion" (`C:\Fusion\xbox\Game\Burn\FusionXbox.exe`), XDK
4928, Bink, XACT. Lua 4.0 is linked in (`$Lua: Lua 4.0 ...`), but no level ships Lua: 0 Lua 4.0 bytecode
signatures (`1B 4C 75 61 40`) and no Lua source in the 4.3 GB of game files, whose archives aren't compressed
(entropy 5.4-5.6 bits/byte). The levels are scripted in Stormfront's own command language (4.1).

## 2. Prior work

- No co-op mod, patch or hack for this game: the Co-op Anything directory, and web searches ("Two Towers 2002 co-op
  mod two player PC port", "Two Towers Xbox 2 player coop hack/mod/player 2"), 2026-10-17. ModDB, Nexus, Reddit and
  the ports' Discords: not checked.
- The game has no multiplayer and no netcode. Return of the King has 2-player co-op, on a different engine (EA's own
  `EAThirdPerson`, per Athelas' `docs/ROTK-ENGINE-NOTES.md`): a design reference only.
- Two PC ports, neither with co-op:
  - [The Two Towers Recompiled](https://github.com/Gumpolas/TooTawersRecomp): static recompilation with
    [xboxrecomp](https://github.com/sp00nznet/xboxrecomp). MIT for its own code; xboxrecomp MIT plus LGPL-2.1 parts
    from xemu. The repo has no game code: `generate.py` translates the player's own `Default.xbe`.
  - [Athelas](https://github.com/Shagg88/Athelas): another recompiler (XboxXBE-Recomp), MIT. Its headless Linux test
    harness (CPU renderer, scripted pads) is a good model for bot tests.
- Built on The Two Towers Recompiled: its hand-written C sits on the generated code, it already filters every pad
  read (`xbox_pad_filter`), and its `heroes.c` (the Extra Heroes mod) already rebuilds a level with another hero's
  model, animations and texture.

## 3. Building the port

- `generate.py` needs **capstone**, though the port's README says "no extra packages" (observed:
  `ModuleNotFoundError: No module named 'capstone'` in `tools.disasm`). A venv with `capstone==5.0.9` (installed with
  `--require-hashes --only-binary=:all:`) works. Worth reporting upstream.
- Toolchain used: clang 22.1.8 (llvm-mingw style, `x86_64-w64-windows-gnu`), CMake 3.31, Ninja 1.13, Python 3.15.
  `generate.py`: 8.6 minutes, "8718/8724 functions (0 failed)", 27 unresolved call targets stubbed. Build: 52 s.
- Our build differs from the release by hash (the release: clang 23.1.2 with msvcrt; ours: clang 22.1.8 with UCRT),
  with the same sections, imports, strings and behaviour in a smoke run.
- Hooks: a hand-written `sub_X` replaces a generated function, which is renamed `sub_X_gen` and can be called. A new
  `sub_X` needs `generate.py` again (about 9 minutes). xboxrecomp's `manual_scan.py` only takes declarations on their
  own line (`^extern void sub_X_gen(void);`): one that follows a `}` on the same line isn't seen, and the build fails
  with duplicate symbols.
- 1.9.2 to 1.9.4: upstream changed 26 files; the co-op changes 9 of them (`e3hud.c`, `heroes.c`, `menus.c`,
  `options.c`, `twotowers_manual.c`, `kernel_bridge.c`, `nv2a_d3d11.inc`, `nv2a_gl.inc`, `nv2a_pb_exec.c`). A three-way
  merge was clean in 7, with one-hunk conflicts in 2 (options.c's "Options just opened" now counts frames;
  twotowers_manual.c's includes, where 1.9.4's `tt_env.h` caches `getenv` per name).

## 4. How the game works

### 4.1 Level scripts

- A table of 500 commands, 40-byte records `{char name[36]; u32 handler}` from `@TEST_SCREEN` (0x173EE3).
- Handlers are cdecl `(vm)`: 0xB4520 checks the argument count, 0xB4590 an argument's type (2 number, 3 object),
  0x9F2A0 reads a number, 0x9F2E0 an object id, **0xB4690 turns an id into an actor**, 0x9F350 / 0x9F380 push a
  result. Every handler's object lookup goes through 0xB4690, which makes it the place to answer a script with player
  2's objects.
- 31 commands are `return 0` in retail (0xD3BA0), among them `@CameraGoto`, `@CameraSetMode`, `@CameraSetTarget`,
  `@CameraSnapTo` and `@InitHealth`: the camera is native code only.
- **Scripts address actors by name.** An actor's script handle `+0x18` is its placement record + 8, the name.
  - The hero is set up by name at the level's start (observed with a hook on 0xB4690): `@SetAnimMachine`,
    `@SetBrainMachine`, `@AttachToBone` x3, `@SetEnemyID`, `@PassBrain`, `@StartPlayerWeaponPfx`, `@UnlockHeading`,
    `@OnComboBreak`, `@PlayerLargeMass`, `@ObjectCollisionOn`, `@SetAlpha`.
  - Each weapon has a set-up script of its own: `@Activate`, `@SetRenderable`, `@SetWeaponID`, `@Deactivate`,
    `@ObjectCollisionOff`.
  - The death check (`@ObjectDie`) and the enemies' brain scripts (`@IsPlayer`, `@ObjGetPosition`, `@Probe`) name the
    hero. One run with four Ringwraiths targeting player 2: `@IsPlayer` 1174 lookups, `@ObjGetPosition` 623 (12 on
    player 2), `@Probe` 553, all on player 1.

| Command | Handler | Notes |
|---|---|---|
| `@GetPlayer` | 0xAF610 | `[0x43B15C]+0x18` |
| `@IsPlayer` | 0xA6850 | actor == `[0x43B15C]` |
| `@SetPlayerControl(b)` | 0xA6B50 | the byte 0x43B1C0 (one, not per player) |
| `@EndMission(n)` | 0xA6370 | `[0x8D2484]->0xF1230(code)`; n 0-4 gives code 0, 1, 2, 5, 6 |
| `@NoDie(actor, b)` | 0xAE5D0 | bit 0x4 of `+0x110` (derived) |
| `@AllowHeroCam` / `@AllowDeathCam` | 0xA3D90 / 0xA3DE0 | camera `+0x18A` / `+0x18B` |
| `@PassBrain(actor, n)` | 0xA50F0 | the actor manager's 0xC6150 (4.6) |
| `@ObjectDie` | 0xAB200 | the hero's die method (4.8) |
| `@SetRespawnId` | 0xB0FB0 | the checkpoint (4.10) |
| `@KilledByPlayer` | (calls 0x99D40) | the kill award (4.9) |
| `@ChangeTarget` | 0xA1CA0 | asks for a new pick: component `+0x23C` = 1, `+0x128` = -1.0 |
| `@FollowPlayer` | 0xA5A20 | gives a companion's AI component its leader (0x54C00 with `[0x43B15C]`) |
| `@PlaySound` / `@SetSfxVolume` | 0xB3760 / 0xB3450 | 4.15 |
| `@TeleportToStartPoint`, `@TeleportPlayerTo` | 0xAC710, 0xAF840 | not hooked |

### 4.2 The player and the characters

**"The player" is a set of globals** (observed):

| Address | Meaning |
|---|---|
| `[0x43B15C]` | the player's character (270+ uses in the generated code compare against it) |
| `[0x43B160]` | its component (`+0x494`) |
| `[0x43B164]` | the camera object |
| `0x43B1C0` | "control taken" byte: 1 while the level holds the player (about the first 4 s of Weathertop) |
| `[0x1CB8B4]` | the pad port the game reads as the player's |
| `0x8D2484` | the mission manager; 0xF1230(code) stores the pending end state at `+0x4808` |
| `[0x4AB69D]` | the current hero byte (1 Aragorn, 2 Legolas, 3 Gimli, 4 Isildur) |

The per-player state the control step and its callees write (swapped for player 2): `0x43B114` (0x48 bytes),
`0x43B168` (0x58), `0x43B1C1` (0x5F), `0x43B298` (0xF8), `0x43B3A4` (0x10), `0x1B0C40` (0x20), `0x1B1080` (0x24),
plus the current hero's record (0x348 bytes, 4.9).

**Characters** are one class, vtable 0x1741C0: the heroes, companions, enemies, the Watcher and its tentacles. Built
with `0xE6B50(0x540)` and the constructor 0x82C10.

| Field | Meaning |
|---|---|
| `+0x18` | script handle (placement record + 8: the name) |
| `+0xF0..+0xF8` | position (`+0x90` and `+0x130`, `+0x140` hold more copies) |
| `+0x110` | flags: 0x4 no-die, 0x2000 active, 0x20 (cleared while dying) |
| `+0x114` | role: 1 the player, 2 a companion, 3 an enemy |
| `+0x2EC` | its pathmap index (-1: none) |
| `+0x2F0` | character type: sets health, ammo and its maximum (0x8C850). Aragorn 0 (30 arrows), Legolas 7 (60), Gimli 8 (25 axes) |
| `+0x2FC`, `+0x300` | health (int32), max health |
| `+0x304`, `+0x306` | ammo (16-bit), its maximum |
| `+0x3D8` | its state machine: `+0x408` the state, `+0x420` the state's time in seconds |
| `+0x3D0` | its event source's listener list (a hero's weapons are on it) |
| `+0x464`, `+0x465` | lock flags the level sets on the player |
| `+0x466` | in a hit window (set from an animation event of type 4) |
| `+0x494` | component: `+0` flags (0x8000 player-controlled, 0x10000), `+0x20` brain, `+0x50` brain machine object, `+0x98` target, `+0xA0..+0xB0` aim details, `+0x148` target weighting |
| `+0x49C` | flags: 0x100 the player's hit path, 0x40 set while dying (no hits reach it), 0x400 (a tentacle) striking |
| `+0x4BC` | its last attacker |

Methods (vtable slots): 1 = 0x843F0, **what moves a character** (its locomotion; the control step only sets things
up); 21 = 0x85340, the message handler; 36 = 0x838E0, die; 42 = 0x84220, take a hit; 43 = 0x86B30, apply the
character's stats (max health).

**Weapons** are placed objects, class 0x174980: owner `+0x11C`, weapon id `+0x2F4` (it selects the weapon's data and
its attack box's reach). Ids (observed): Aragorn's sword 0x01 and dagger 0x1D, Legolas' knives 0x1F and dagger 0x1D,
Gimli's axe 0x17, the bow model 0834F2D4 0x27; Isildur's unknown. `@AttachToBone(parent, bone, child)` calls the
parent's vtable `+0xC4(child, slot)`.

| Hero | Slot 0 | Slot 1 | Slot 2 | Slot 3 |
|---|---|---|---|---|
| Aragorn | sword 13F82514 / 29636120 | - | - | dagger 69DAC4B8 / BD9E230B |
| Legolas | knife EF8EBCEC / 9126E92A | knife EF8EBCEC / 9126E92A | dagger 69DAC4B8 / BD9E230B | - |
| Gimli | axe 2B9252D0 / AB926BE7 | - | - | - |
| Isildur | sword 13F82514 / 29636120 | - | - | dagger 69DAC4B8 / BD9E230B |

(model / texture keys, observed with `TT_COOP_WLOG` in Balin's Tomb.) The ranged weapon is another object, taken
while aiming (4.13).

### 4.3 How a level makes the player

- **The player is a placed actor.** The level loader 0xF5E50 walks the placed actors; the first with role 1 becomes
  the player: `+0x49C = (+0x49C & ~6) | 1`, vtable slot 0, then **SetPlayer 0x99000** (`[0x43B15C]`, `[0x43B160]`,
  bit 0x8000 in the component, the camera attached with 0x68750, the player state reset). Each actor is then
  registered with the manager at 0x43C5E8 (0xC5E20).
- Objects are made from **placement records** by 0xF5BC0 (a switch on the record's type; then 0x19880(obj, type)
  links it in). Characters by 0xF3430 (thiscall world 0x4B0EF0, record, key; `ret 8`).
- A record (observed): `+0` type (1 hero, 0x16 weapon, 0xB ...), `+4` name length, `+8` name, `+0x28` key (the
  name's hash), `+0x2C` position; records are 0x64 bytes apart. A hero record has its mesh, animation set and texture
  keys at `+0x44`, `+0x48`, `+0x4C`; a weapon's model and texture at `+0x40`, `+0x50`.
- **The actor heap (0xE6B50) only gives memory while the level loads.** In play it returns 0 (observed), so nothing
  can be built in play: no second hero, no weapon copies, no extra enemies.
- The level index is `[0x1BB2A4]` (0xD1C00 sets it, 0xD1BF0 reads it; table at 0x17B7A4): 0 00ISUL1 (the prologue),
  1 01WEAT1, 2 02GATE2, 3 04BALIN1, 4 13AMON1, 5 14FANG2, 6 16ROHAN2, 7 16ROHAN1, 8 16ROHAN3, 9 17TOP1, 10 18HOLE1,
  11 20YARD1, 12 21ORTH1, 13 21ORTH2.
- Level files per hero: `.bfa` Aragorn, `.bfl` Legolas, `.bfg` Gimli, `.bfi` Isildur. Each carries every hero's
  weapon records. Hero keys (the same in every level):

| Hero | Mesh | Animations | Texture |
|---|---|---|---|
| Aragorn | E9B808FE | DE94124A | 66829AC2 |
| Legolas | 9EDC669E | EE4A9EBA | 58C546AD |
| Gimli | 71EB3C66 | AAE0DF66 | F35ADE70 |
| Isildur | 37EA8A4E | 53DFD442 | CD25E3B2 |

- What gets loaded is named by resource lists (`{index, key}` pairs) in type-500 resources (packs). Weathertop has 6
  packs, Balin's Tomb 2; **0E01E39C is the player's pack**, the others are the level's cutscenes'.

### 4.4 Input

- **The game sees one controller.** xboxrecomp's emulated USB (`src/usb/ohci.c`) has one gamepad device, and
  `usb_gamepad_report()` reads host port 0 only. A second PC pad never reaches the game.
- The game polls the pads into **`0x6B2758 + 0x14C * port`** (0xE7DB0; nothing outside the input module
  0xE7D30-0xE8B70 reads it but 0xD12C0), and every query indexes it with `[0x1CB8B4]`:

| Offset | Contents |
|---|---|
| `+0x00..+0x11` | the raw XINPUT_GAMEPAD: buttons (u16), 8 analog buttons, LX LY RX RY (s16) |
| `+0x14..+0x20` | the sticks as floats, dead zone applied |
| `+0x34` | held mask (digital buttons, analog buttons over a threshold at bit 8+i, stick directions) |
| `+0x24` / `+0x28` | last frame's mask / released this frame |
| `+0x2C`, `+0x30` | latches |
| `+0x144` | device handle (a record with 0 is skipped); `+0x148`, `+0x149` insert / remove flags |
| `+0xE4` | rumble request |

- XInputGetState is 0x1697D9 (stdcall handle, XINPUT_STATE*; `ret 8`).
- On a PC with one pad, its handle sat in port record 2, and with the test route (`TT_TWEAKS` level start)
  `[0x1CB8B4]` is unset ("primary 4294967295"), so the game reads every pad for player 1.
- Controls (observed): **X alone never makes an attack box, for player 1 either**; Y does. To shoot, hold LT and press
  A (or Y); LT alone only draws. White blocks.

### 4.5 A frame

- 0xF2FF0, the world update (from 0x7C340), runs ... 0x9E1D0, **0x9C380 the player's control step**, then **the
  camera update 0x68DC0** (thiscall camera), 0x89220, 0xD43E0, the actor updates, then `[0x43B160]->0x54E70` (the
  player's component).
- **0x9C380** (cdecl; its argument is the frame time, 0.013-0.057 s; the game keeps a copy at 0x1B0C54) reads
  `0x43B1C0`, `[0x43B15C]`, the camera (camera-relative sticks) and the pad, and writes the state blocks above. It also
  calls the camera update twice (0x9C8BF, 0x9CA35).
- 0x5A450 (once a frame, player-centric) sorts the characters into engagement lists, and 0x5A920 (from 0x5AAC0) runs
  the target arbitration (4.7).

### 4.6 Messages, brains, the state machine

- Every message goes through the enqueue 0xC6F60; the dispatcher is 0xC73E0. A brain's handler is its vtable `+0x54`
  = **0x922A0** (then 0x9E9D0, the state machine and the VM): the hero's attack scripts run from messages to its
  brain.
- `@PassBrain` stores a brain id through component `+0x20` (0x55C70): the hero gets 22 at set-up, switched to 2 on
  each hit taken and back.
- Brain and animation machines are per-type definitions: all five Ringwraiths share one component `+0x50` and one
  `+0x3F8`. Two heroes of one type sharing them is normal.
- **State machine inputs** (0x53090, thiscall machine = actor + 0x3D8, input; a 64-bit pending mask at `+8`, a block
  mask at `+0x50`). Their names are in the XBE (`INP_*`; id = the reverse file order minus 64): 1 `INP_DIE`,
  2 `INP_KNOCKDOWN`, 3 `INP_KNOCKBACK`, 5 `INP_TAKEDAMAGE`, 13 `INP_ATTACKGROUND`, 16 `INP_BLOCK`, 28 `INP_RESSURECT`
  (sic), 35 `INP_JUMP`, 37 `INP_ARROWS`, 38 `INP_SHOOT`, 50 `INP_DRAWBOW`, 51 `INP_BOWCHARGE`, 52 `INP_LONG_BLOCK`,
  57 `INP_SLAM`, 58 `INP_DEEP_WATER`, 63 `INP_NULL`.
- States seen: 2 idle; attack 30 / 34 (X), 11 / 13 / 42 (Y); 27 drawing the bow, then 28, 30, 26 and the shot;
  41 then 47 the player's scripted death; 44 / 45 then 50 the death from a hit (lying; terminal); 48 / 51 getting
  up; 55 wading. **The state is behaviour, not animation**: raising knockdown or knockback inputs changed the state
  while the model stood upright.

### 4.7 Combat

- **Attack boxes:** `@WeaponCollisionOn` ends in 0xC6750, then 0xBA020 on the contact manager `[0x49E8DC]`: a box
  with the attacker (`+8`), the weapon (`+0x14`), damage values and a mask chosen by role (`+0x4C`).
- **The box test** is 0xBE0D0 (a pooled delayed message, type 0x19, run from the world update). An attacker with
  `+0x49C & 0x100` takes the player path: it needs `+0x466`, sweeps the blade (0x6DD70 on the weapon id gives the
  reach; 0x257B0) and queries the collision world (0x6A160). Its hit branch at 0xBE7D2 tests attacker ==
  `[0x43B15C]`, but player 2's hits land without a swap there (3 runs with it, 2 without: all dealt damage). Other
  attackers take an NPC path that can only hit their own target. A contact posts **message 3 from the weapon to the
  victim**; hit messages are `{0x103, weapon or projectile, victim}`.
- A hero updates its weapons through its listener list (`+0x3D0`: 0x86500 calls each listener's `vt+0xC`, the
  weapon's slot 3, 0x905D0). Player 1's hit events go through the dispatcher 0x265D0 (listeners at `src+0x74`, count
  `src+0x174`).
- **Taking a hit:** slot 42 (0x84220). The hit handling 0x849F0 (from the message handler 0x85340, on the victim)
  feeds the attacker's rating meter only if the victim's last attacker `+0x4BC` is `[0x43B15C]` (at 0x84DF8).
- **Targets:** component `+0x98`. Setters 0x539F0 (target and details), 0x53AE0 (`+0x98`, `+0x9C`), 0x54410; 0x53B30
  clears. The arbitration: 0x5A450 fills the lists 0x4110C0 (count 0x411394), 0x411250 (0x4114A4), 0x411140
  (0x4114AC), 0x411398 (0x4110BC); 0x5A920 takes the candidates (`[0x43B15C]` first, then 0x4110C0 and 0x411250) and
  the attackers (0x411140, 0x411398); 0x57A70 clears each attacker's target and 0x5A330 re-picks. A hero's own aim
  pick is 0x5A150 (the camera's or the hero's heading, `[0x43B110] ? [0x43B10C] : camera+0x60`, plus the stick's angle
  `[0x43B18C]`). On Weathertop both heroes got their targets from the matching pass 0x57350 (call 0x5741E): the game
  picks player 2's target too (201 picks against player 1's 199 in 100 s, observed).
- **The kill award** 0x99D40(enemy) is called by the enemy's message handler at its death (0x85706 / 0x85711, cdecl).
  It is gated by the enemy's `+0x526` and `+0x538`, not by who killed it, and runs in whoever's context is current:
  the rating from the record's meter (0x9DE10), the popup (0xCB9D0), experience = the enemy type's base (`+0xA4` of
  its type data; 80 for Balin's Tomb's goblins) x the meter x factors (0x99CA0), added by 0x9D3E0, kill counts.
  Repeated awards for one enemy object happen in solo too (Balin's Tomb reuses enemy objects, derived).

### 4.8 Death

- Player 1's death (observed): health 0 puts the hero in hit state 41; about 4 s later the level script's `@ObjectDie`
  (0xAB272) calls the die method 0x838E0 (dead state 47). Its player branch starts the death cam 0x69130 (slow motion,
  the Eye of Sauron), and about 10 s later the death-cam code ends the mission with code 5 (from 0x64EE6). **Code 5 =
  mission failed after the player's death.**
- The die method branches: role 3, the enemy death (0x8CAD0); this == `[0x43B15C]`, the death cam; anyone else, only a
  common part. It tears down a lot (event 46 on component+0x20, queued messages flushed, targets cleared): a hero it
  ran on couldn't be revived properly.
- The script's death check names player 1, so **the game never kills a second hero**: at 0 health it carries on.
- A fatal hit (inside slot 42) starts the death, as `INP_DIE` does: 44 / 45, then lying in 50, terminal (no input moves
  it). From 50:
  state set to 47 with its time at 0, then `INP_RESSURECT` (28) gets the hero up (51, then idle within 0.7 s).
- The death sequence sets `+0x49C` bit 0x40 (and sometimes clears `+0x110` bit 0x20), and nothing clears them: a
  revived hero took no hits until they were put back (observed: 0 hits after a respawn without the fix, 2-3 with it).

### 4.9 Hero records, experience, upgrades

- **Records:** `0x4AB598 + index * 0x348` (0xD26F0; 0xD2310 maps the hero byte to the index). From `+0x64` on it is
  the save's block for that hero.

| Offset | Meaning |
|---|---|
| `+4` | combat rating meter |
| `+0x5C`, `+0x60` | Perfect time left, Perfect's length (10 s) |
| `+0x68` | the next level's threshold (recomputed, not saved) |
| `+0x6C` | committed experience |
| `+0x74`, `+0x78` | level fields (0x75430 prints `+0x74` + 1; the gauge reads `+0x78`): not untangled |
| `+0x7C` | the level-up flash |
| `+0x90` | the level's running experience (float) |
| `+0x94..+0xB3` | per-level stats, kill counts among them (`+0x98`, `+0xA0`; also `+0x128`) |
| `+0x204` | a per-level working value (0, 0x10000 or 0x20000 at a shop's close) |

- **The rating meter:** 0x9D9D0 raises it (0 to 30 on a kill-type hit; from 0x99AB0, 0x99E00, the hit handling
  0x849F0), 0x9DF70 (every frame, from 0x9E080) decays it, faster in the lower bands, and damage taken drains it
  (0x9DEC0, 0x98EF0). Bands (0x9DE10): above 98 Perfect, 58 Excellent, 25 Good, else Fair. At 98 Perfect mode starts
  (`+0x5C` = `+0x60`), and when it ends the meter goes to 0. A level is 5000 experience.
- **Upgrades:** 80 bytes per hero at `0x4AC5B0 + hero * 0x50` (Aragorn 0x4AC600, Legolas 0x4AC650, Gimli 0x4AC6A0,
  Isildur 0x4AC6F0). 0xD2710 gives a hero's block, 0xD25B0(upgrade, hero) asks one, 0xD2570 sets one (the shop's buy).
  Buying Legolas' War Rush set Legolas' upgrade 31, took 5000 off `0x4AB67C` (the points left) and put it in `0x4AB270`
  (the shop's spent points).
- **Move picks:** 0x99A90 and the 20 functions in 0x9A1F0-0x9C37E ask the hero getter 0xD2310 which hero's upgrades
  to read and which branch to take (0x9B950: upgrade 15 bought gives move 0xE9, else 0xE7).
- **After a level:** the stats screen moves the level's experience into the points to come `[0x4AB67C]` (0xD2630:
  record `+0x64` plus `+0x90`; flags `[0x4AB678]`, `[0x4AB270]`) and levels the record up; the shop works from those.

### 4.10 Checkpoints

- `@SetRespawnId` (0xB0FB0) fills the checkpoint record at 0x1BB354 (0xD24E0, stored by 0xD24F0): the respawn id, the
  hero's health (virtual `+0xA4`), ammo (`+0x304`), then its record's `+0x90` (as an int) to `+0xB3`.
- A game over's Replay reloads the level from it and puts player 1's level experience and per-level stats back (800
  to 500 in a test; with a zero checkpoint, 500 to 0). For a co-op mod the reload looks like a new level.
- In Gates of Moria, respawn id 1 starts at the Watcher's fight; ids 2-4 loaded to a black screen.

### 4.11 Front end

- The screen object is 0x4AD050: `+4` the next screen. Screens: 1 the map, 3 the saves, 4 the main menu, 5 Options,
  6 play, 7 the save, 9 the mission stats (`+0x9474`, vtable 0x173C40), 10 the upgrade shop (`+0x9A78`, vtable
  0x173BCC). The main loop is 0xD1850 (enter = vtable `+8`, exit `+0xC`).
- After `EndMission(1)`, play's tick (0x7C340) sets the level + 1 and goes to screen 9. The stats screen's name and
  portrait come from 0x71970, which its enter (0x71F60) runs only while `ui\endlvl.bff` is still to load.
- The shop: dialog state `+0x3A0` (4 = "Do you want to save your game?"; Yes sets `+4` = 7). Focus: `0x4AD050 +
  0xA008` = 1 the upgrade list; `+0xA00C` 1 Continue, 2 Replay; both 0 the level tabs, `+0xA004` the page. Left from
  the list or Continue goes to Replay. Replay's leaf and word: the shop 0x425520 / 0x4256D8, the stats 0x426CD0 /
  0x426E88.
- 0xC35E0 fades music and sound out for the save screen (`[0x43BD21]` = 1), 0xC3610 clears it.
- The hero select: object 0x4AD058; `+0x18` the cursor, `+0x20` the mode (2 turning, 3 picked, with `+0x220` = 1).
  0x75430 sets the level label's text every frame just before it is drawn (element draw: 0xC8940).
- The start-up screen's handler 0x7F000: with no save it sets level 0 and starts a new game (0xD1C10): a first launch
  goes straight into the prologue.
- `UI\*.BFF` pictures (type 350, a 0x80 header): stills 640x448 linear 32-bit; menu backgrounds 512x256 DXT1
  (`options.BFF` key 0 is the Options screen's).

### 4.12 HUD and renderer

- 21 element lists at 0x1BAAE4 (`{elements, count, -}`), drawn by 0xCF450, on/off bytes at 0x4A559C. An element is
  0xDC bytes: `+8` x, `+0xC` y, `+0x10` w, `+0x14` h (HUD space, 640x480), `+0x20` colour AABBGGRR, `+0x24` shown,
  `+0x2C` font, `+0x34` a text pointer (or UTF-16 at `+0x38`). **A negative width draws a textured element
  mirrored.**
- List 0 (0x4A0398), the health sword: 0 blade strip, 7 red bar, 8 green fill, 16 art; 24 the quiver, 25 its flash,
  26 the count. List 1 (0x4A3D80), the gauge: 0-6 (tube, fill, flash, cap, medallion, marker, hit icon), 7-9 the
  Perfect glow, 10 / 14 the rating popup, 11-13 the level-up flash, 15 a message line. List 2: the companion's
  portrait.
- The gauge (0xCF9A0) shows `(int)+0x90 + +0x6C` minus the threshold (0x9D6D0) out of (0x9D6A0 - 0x9D6D0). The arrow
  count `[0x4A82BC]` comes from `[0x4ACBB0]`, filled by 0xD3530 from `+0x304`.
- The quiver's texture object is `{0x1709C4, 0, 0, 0, data}`: a 0x80 header (the D3D texture at `+0x2C`), then
  swizzled ARGB pixels. The level file has two such icons (type 350, keys 6 and 0xD), per hero variant.
- Free guest memory: 0x4BA600 onward was zero in every run (0x4BA1C0 and 0x4BA400 are the main menu's Exit item).
- The port's renderer pins HUD pieces to the screen edges in widescreen: pieces within 10 units form a group, and a
  group in one half goes to that edge (`hud_frame_end`, `hud_wide_x`). Overlay quads (`xbox_overlay_cb`, up to 8 BGRA
  quads in 640x480 space) follow the group their centre is in. The port's mouse cursor overlay adopts whatever is
  installed as its "previous", so chaining another callback from outside loops (a stack overflow); menus.c now has a
  slot for an extra one.
- Xbox D3D: SetTransform 0x117230; the device `[0x124180]` keeps state n at `+0x810 + n * 64`. The projection (state 1,
  `+0x850`) is real; the view (state 0) is identity.
- **The camera's frustum:** SetPerspective 0x4E890(fov, aspect, near, far) (fov in radians) keeps an object (0x4B2F80
  in play): near and far (`+0`, `+4`), the frame (`+0xC`, `+0x10` = 640, 480), view-space corners (`+0x18`..), world
  bounds (`+0xA8`, `+0xB8`), the four near corners in the world (`+0xC8..+0xF8`), the four far ones
  (`+0x108..+0x138`), the planes (`+0x158`..). It runs only when a camera shot starts (10 calls in 40 s), each shot
  with its own fov (0.436, 0.663, 0.785 seen).

### 4.13 Ranged weapons and projectiles

- A hero aiming takes a ranged weapon object with 0x85FB0 (thiscall character, weapon, 0xA; `ret 8`), which makes it
  the weapon's owner (`+0x11C`). The hero's attack scripts fire from it by name.
- The launch is 0xD9460: `[esp+4]` the bow object, `[esp+16]` the projectile id, `[esp+0x18]` the shooter. Ids
  (observed, player 1 shooting): Aragorn 13, Legolas 19, Gimli 16 (the throwing axe), Isildur 15; Frodo's stone 10. The
  projectile table is the same in every variant and names no model: **a projectile is drawn with the shooter's
  bow-hand weapon model.**
- Ranged weapon models (model / texture): Legolas' bow 0834F2D4 / 4167DADC, Gimli's throwing axe E7EEC090 / AB926BE7,
  Aragorn's bow 36AC3F58 / D82C5CAB. Isildur takes none.

### 4.14 The world

- **Pathmaps** (the AI's walkable grids): the manager at 0x41F428; an actor's pathmap is
  `[0x41F428 + 0x1088 + 4 * actor+0x2EC]` (0x5CC70). A pathmap: origin `+0x48`, `+0x4C`; cell size `+0x68` (int);
  cells `+0x6C` (a byte each); columns `+0x70`; rows `+0x74`. A cell's state is `(byte >> 2) & 3`, or the override
  table at 0x418C00 where that is non-zero: 0 walkable, 2 walkable too (heroes stand on it), 3 blocked. Gates of Moria:
  pathmap 11, 40 x 20 cells of 2 units. The debug switch "AI/Draw Pathmaps" (0x8D24DD) drew nothing in the frames.
- **Spawners** (class vtable 0x174B60): each runs a script that adds the level's pre-built characters to its pool
  (`@AddToSpawnPool` 0xA6480 then 0x93250: placed at the spawner, `+0x3B8` = 1, hidden by `+0x110` bit 0x200000).
  A pool holds up to 9 (levels use 2-5): ready (`+0x3D0`) and spawned (`+0x3DC`) lists. Spawning is vtable `+0xC4` =
  0x930F0 (`@SpawnAnObject` 0xAB520). A dead pooled character gets `+0x3B8` again at the end of its death and goes
  back to the ready list. Per level (60 s survey): Weathertop none (its enemies are placed), Gates of Moria 25
  spawners, Rohan 4, the Deeping Wall about 50, the Courtyard 35.
- **Pickups:** class 0x1748B0, kind `+0x3D0` (7 arrows, 1 health), message handler 0x8FC60. A touch (message 13, the
  toucher as source) runs 0x8F960, which refills `[0x43B15C]` (0x9D400 health, 0x9D460 arrows). Drops follow what the
  player needs: with player 1 full, none dropped.

### 4.15 Sound

- `@PlaySound` (0xB3760) goes to 0xC3BF0 and 0xED4E0, gated by `[0x43BD69]`; only scripts use it. The heroes' combat
  sounds go another way (unknown). `@SetSfxVolume` (0xB3450) goes through 0xC3060 to the sound library (0xEA550).

### 4.16 The Watcher (Gates of Moria's boss)

- `WatcherObj`, `TentacleObj`, `TentacleObj3`, `TentacleObj4`: ordinary characters (vtable 0x1741C0, role 3). Their
  bodies are out in the lake (tentacles at x 33-40, y 389-410; the Watcher at 53.9, 393.8). The level's script
  objects include `activateWatcher`, `introduceWatcherCine`, `makeTentaclesNotTAble`, `bossMissionStart`,
  `setRespawnBossId`.
- A tentacle's cycle (observed): rises (state 5, about 2.6 s); strikes (2, 3 or 4, with `+0x49C` bit 0x400; message
  28 to the hero it is after as the strike starts, at the spot that hero stood; the heroes answer with type 40);
  lies stretched toward that spot (9, 3-5 s); strikes again or pulls back (14, about 5 s) and sinks (1). Strikes
  knock heroes into states 33 / 37 / 49 / 52 (message 51).
- In bot runs the Watcher stayed in state 0 at 225 health, and no hero's weapon ever sent a contact (message 3) to a
  tentacle, even with the hero's target set to it and swinging at its middle: the only contacts were the tentacles'
  own weapons hitting the heroes.

## 5. How player 2 is made

What works, in order (`twotowers_manual.c`). Each step was found by a failure without it.

1. **A second pad.** Port 1's pad record gets a virtual handle (0xFEED0001), re-attached in the poll hook (0xE7DB0)
   whenever it is 0. The XInputGetState hook answers it with host pad 2 (or a test script); rumble for it returns at
   once. The game's own poll, edge detection and dead zones then work for it.
2. **The twin.** During the load, right after player 1's placement record is built (0xF5BC0), the original runs
   again on the same record: a second character (the twin), with its role set to 2 so the loader keeps the original
   as player 1. It shares player 1's record and script handle. (Its own renamed record copy crashed on the first
   enemy hit on player 1: a script event reached the twin with no script object behind the name.)
3. **The level's set-up for it.** The script's hero set-up commands run a second time with 0xB4690 answering the
   twin (set-up phase only: `@OnComboBreak`, `@PlayerLargeMass`, `@UnlockHeading` keep firing in play), plus
   `@PassBrain` (0xC6150). Vtable slot 43 gives it max health and stats. Without the set-up the twin had 1 health and
   hung the game: its component posted messages to a null target (no `component+0x50`).
4. **The take-over**, 3 s into play: player 2's state block set up as SetPlayer does, `+0x49C` bits as player 1's,
   put beside player 1.
5. **Every frame, player 2's context:** the control step runs a second time with player 2's globals swapped in
   (`[0x43B15C]`, `[0x43B160]`, `[0x1CB8B4]` = 1, the state blocks, the hero byte and its record), and the same swap
   runs around its vtable methods, its component update and 0x5A450. The camera pointer and the control flag stay
   player 1's. The frame time is player 1's (from 0x1B0C54; a stray stack word left player 2's Perfect time frozen).
6. **The lock flags:** player 1's `+0x464`, `+0x465` and `component+0 & 0x18000` mirrored onto the twin every frame.
   The level changes them on player 1 only (by name); without the mirror the twin's slot 1 skipped its locomotion and
   it never moved. With it, a scripted stick on pad 2 moved the twin 6.6 / 52.9 / 0.7 units in three windows, and 0.00
   with no input (two runs).
7. **Its brain's messages:** while 0x922A0 handles a message whose source is the twin, object lookups of player 1 or
   one of player 1's weapons answer the twin or its copy, in player 2's context. Without it `@FlagAttacking`,
   `@WeaponCollisionOn/Off` and `@LockHeadingToTarget` all landed on player 1 when player 2 swung.
8. **Its own weapons:** copies of the weapon records (renamed, own keys) are built during the replayed
   `@AttachToBone` (the heap is still open), then the weapon set-up script is replayed for them. By slot, from player
   2's own hero (a slot player 1's hero lacks gets an extra copy); then each copy gets player 2's hero's weapon id
   (with player 1's ids, Gimli swung with Legolas' knife's reach: 2 contacts in 278 box tests, against 209 in 704
   after). The twin keeps its own listener list (copying player 1's dropped its weapons where they were).
9. **Its own weapons only:** player 2 takes only weapons built for it (a hook on 0x85FB0). Before, it took player
   1's bow and became its owner, so its aim turned player 1's bow and knife.
10. **Its own ranged weapon:** at the first replayed attach, copies of each ranged weapon object in the level with
    player 2's hero's ranged model (imported from its own level variant by heroes.c), given to it in place of player
    1's.
11. **A sink for the rest:** inside player 2's events, a lookup of one of player 1's weapons it has no copy of answers
    an unused ranged copy, never drawn. Without it, Legolas' events (the twin of a Legolas player 1 is Legolas' clone)
    set the pose of player 1's left knife whenever player 2 Gimli aimed.
12. **Its own character type** (health, ammo): set again after `@SetEnemyID`, the take-over and the start of play,
    which all gave it player 1's.
13. **Its own look:** heroes.c builds the level with player 2's model, animations and texture under their own keys,
    hands the twin's record those keys for its build only, and names them in the player's pack (0E01E39C) only, so
    cutscenes keep player 1's hero. A guest hero (Extra Heroes) plays with its own skeleton and composed animations.
14. **Its own record:** a copy of its hero's record from the save, swapped in with its context; written back only
    through its own post-level screens (6.6).

The twin is the same class as every character, so most of the game works for it unchanged in its context. What
needed handling is everything that asks "is it the player?" (`this == [0x43B15C]`, or the hero's name in a script):
6.3 to 6.10.

## 6. What co-op does, and the evidence

### 6.1 The shared camera

- **Midpoint:** for the length of the camera update 0x68DC0 (and the death cam 0x69130 and the camera modes 0x69070,
  0x69310, 0x695F0, 0x69760), player 1's position holds the midpoint of both heroes; it is put back after. Shifting it
  by 10 units moved the look-at point 8-9 units (observed). The calls from inside the control step are skipped in
  player 2's step. Bot: the look-at point stayed 4.4-11.6 units from the midpoint with the heroes up to 46 apart; with
  player 1 only it stayed by player 1 while player 2 got 80 away.
- **Zoom:** the field of view, re-issuing the shot's SetPerspective with fov' = 2 atan(tan(fov/2) x zoom). The zoom is
  1 up to 4 units apart, +1 per 20 units more, eased; capped at 1.3 (Normal) or 1.5 (Wide). At 20 units apart: zoom
  1.30, fov 0.785 to 0.984. (Pushing the camera's eye away instead fed back through its smoothing and ran into the
  walls; 2.0, a 79-degree view, was too wide.)
- While a hero is down, the camera frames the one standing.
- At the Gates of Moria the level moves player 1 to the start point after the opening movie (47 units), and the midpoint
  framed a rock: player 2 is now put beside player 1 when play starts (6.2).

### 6.2 Keeping player 2 near

- **Placement on the pathmap:** a flood from player 1's cell over walkable cells (up to 5 cells), taking the cell
  whose eight neighbours are walkable too, nearest 3 units from player 1; with no such cell, player 1's own spot (they push
  apart). Never blindly to the side: 3 units along +x put player 2 off a ledge into the void.
- **When:** at the start of play and after each cutscene (more than 8 units apart); in the frame the level jumps
  player 1 more than 6 units; at a fixed offset while a cutscene walks player 1 (on walkable cells); the catch-up (P2
  Teleport) when too far for 0.5 s; a fall (more than 12 units below player 1 for 1 s).
- **The leash** (off while the P2 Bot drives): an outward step past R from player 1 is undone right after the twin's
  slot 1. Bot: 28.1 units apart at most with R = 20 (the extra from player 1 being knocked back), 48.2 without.
  (Pulling player 2 back at the frame's start leaked: its movement and collision run after it.)

### 6.3 Combat for two

- **Player 2's damage** (5 of 5 steering-bot runs, 170-1686 enemy health per 25 s), with steps 7 and 8 of section 5
  and its target set. Both heroes fighting at once: 3 of 3 runs, both landing contacts, no crash.
- **Its target:** the game's own pick (4.7). An early filler that replaced any target further than 10 units with the
  nearest enemy overrode it; it now only fills in when there is no living target within 35 units.
- **Its aim pick** (0x5A150) runs in player 2's context for the twin.
- **Hit credit:** a hit message is player 2's when its source is player 2 or owned by it (`+0x11C`), and its hit
  handling then runs in its context (rating meter, last attacker). Reading `+0x4BC` first named the attacker before:
  player 2's axe hits came out as not its, and the next ally's hit on the same enemy as its.
- **Kills:** the kill award (0x99D40, from 0x85711) runs in player 2's context when the enemy's last attacker is
  player 2. Before, each of its kills added a kill count to player 1's record ("Fair 8" for an idle player 1).
  Bot (Balin's Tomb, player 1 idle): 7 awards, all in player 2's context, 167 experience; player 1's stats 0 / 0 / 0 /
  0. Suite: kills_post.
- **Threat:** each living enemy keeps a threat score per player (its health lost to that player's weapon, halved every
  6 s); after each arbitration an enemy with threat is pointed at its top-threat player (0x53AE0), switching only when
  the other passes by 25 % and 60 health. Bot: an enemy player 2 hit turned on player 2 within 2-3 s and went back to player
  1 once the threat faded. A straight switch by out-damaging was never exercised.
- **Enemies attacking player 2:** a target field alone isn't enough (their scripts work with "the player"): with two
  Ringwraiths targeting player 2 for 27 s it took no damage. So a brain message from an enemy targeting player 2 runs
  in player 2's context, and player 2 is appended to the arbitration's candidate list. Hits on player 2 per run: 0 and
  0 with neither, 1-4 with them (about half player 1's rate per enemy-second).

### 6.4 Down, respawn, game over

- Rules: a hero who goes down respawns beside the other after a wait (15 / 30 / 45 s by difficulty); game over only
  when both are down.
- **Health guard:** each hit on a hero runs with its health raised by 100000, so the hit handler never sees 0; the
  real damage is applied after, and at 0 or less the hero is down at 1 health. The last hero standing takes the game's
  own hit, so its fatal hit is the game's death.
- **Down:** `INP_DIE` (the death animation, lying in 50), no hits (the dying flags held), health held at 1, pad
  input held back (but not at game over, or the "Replay mission / Exit to menu" menu heard nothing). **Up:** state 47,
  `INP_RESSURECT`, the hit flags restored.
- **Game over:** when player 2's fatal hit ends the game, it runs in player 2's context, where player 1's die method
  takes the plain branch (no death cam). Player 1's death is now queued and run at the next frame in player 1's own context:
  the death cam and the Eye of Sauron, then the game's own end state 5.
- Bot (Weathertop and Balin's Tomb): player 2 down then up, fighting again (46 new contacts in the last 15 s);
  player 1 down with no death cam or mission end, then up (124 / 87 new contacts); either order of both down: game
  over. Suite: down_respawn, down_gameover.

### 6.5 HUD

- **Player 2's health sword:** player 1's sword elements copied into free guest memory each frame and drawn mirrored
  (negative widths) at the top right, through a switched-off HUD list slot; the companion's portrait moved 72 units
  down for the draw.
- **The respawn counter** ("P1 RESPAWN n" / "P2 RESPAWN n") under the swords, in the game's font. The renderer now has
  exclusion boxes (`xbox_hud_excl`, `xbox_hud_excl_more` in `nv2a_pb_exec.c`): pieces whose centre is in one stay out
  of the edge groups (at y 110-160 the counter had merged the whole top band into one centred group).
- **Gauge, popup, arrows:** player 2's gauge (list 1's pieces mirrored, filled from its record), its rating popup on
  its side for 2.6 s after its kill (hidden on player 1's), its quiver and count from its own `+0x304`, with its own
  hero's ammo icon (from its level variant).
- **Perfect rings** (new): a ring round each player's medallion, filling with the meter in the band's colour, then the
  Perfect time left. Overlay quads with bits 31 / 30 of their version pinning them to the left / right edge
  (`ovl_wide_x`).
- **Head markers** ("P1" blue, "P2" amber, grey while down), from the Co-op Options:
  - Projected through the frustum object (4.12): the eye from a near and a far corner, the ray to the head through
    the near rectangle. It matched the earlier eye/target projection exactly where that one is right, and follows
    scripted cameras and blends (`TT_COOP_OLDPROJECT` the old way).
  - At the top of the hero's head (Aragorn, Legolas, Isildur 5.6 units; Gimli 3.6; hobbit guests 3.4, a guess), then
    14 frame units higher. A fixed 6.8 above the feet floated far above a hero near the camera.
  - **Dimmed behind walls:** each frame the game asks for the depth at both heads; at the flip a small shader reads
    the frame's depth there into a 4x1 target, copied to a staging ring and read two flips later. With the
    projection's z terms that becomes a view distance. 3 units or more in front for 8 results running dims the marker
    to a third (orcs passing in front set it off as often as walls, so not hidden). Self-test: a point 4 units under
    player 1's feet reads as behind the ground (25 of 26). Direct3D 11 only; 58-60 fps with it.
- In cutscenes the co-op HUD is hidden (`tt_in_scene()`).

### 6.6 Progress for two

- **Its own record** (5.14). After a game over's Replay, its level experience and per-level stats go back to its
  values at the checkpoint, as the game does for player 1's: both are kept when a checkpoint is taken. Suite:
  checkpoint (500 / 300 kept at the checkpoint, 800 / 700 after it, game over, Replay: 500 / 300 back; its stat words
  `+0x98`, `+0x9C` 7 / 3 back after 20 / 20).
- **Post-level screens:** player 1's stats, player 2's stats, player 1's shop, player 2's shop, the save. Player 2's
  record goes into its hero's slot and the hero byte becomes its for its screens. Each player's record and the three
  post-level globals are kept when that player's stats end and put back before that player's shop (player 2's stats
  had overwritten player 1's level-up). Player 1's save question is answered automatically; player 2's shop asks it (without
  running 0xC35E0, which left that shop silent). No Replay on the screens that follow each other.
- **Saved and loaded:** player 2's purchases and experience are in its hero's record in the save. Suite: save_buy,
  save_reload (its whole record comes back: a checksum over `+0x64..+0x348` without the working values `+0x68`,
  `+0x90`, `+0x204`).
- **Bought moves:** in player 2's context, the move picks' hero getter sees player 2's hero byte, so its moves follow
  its own hero's upgrades. Suite: weapons, kills_post (upgrades bought in memory for Gimli open its moves; Aragorn's
  never do). Left as they are: the HUD and front end's upgrade displays (0x6E350-0x6F220, 0xCCA50, 0xCCB30).
- Suite: levelup_shops (each shop gets its own player's level-up and points), shop_left (left pages the level tabs,
  and is held back where it would reach the hidden Replay).

### 6.7 Menus

- **Co-op on the main menu**, under Credits. It opens a page of the Options screen (options.c): New Game, Load Game
  and the co-op settings. New Game and Load Game go back to the main menu under a black cover and press its own item,
  so the game's own flow follows (the saves, the map, the hero select). Switching screens straight from Options
  crashed in the renderer on leaving the map, and play's enter loads nothing when the previous screen is 5 (that's the
  way back from the pause menu).
- **The hero select, twice:** player 1 picks, the pick is undone, player 2 turns the ring (its controller drives the
  menu), player 1's pick is made again. Player 2 can't take player 1's hero (its label reads "Unavailable"). Two
  status lines, one per player. Suite: menus.
- **Co-op Options** (ini `[Coop]`): Difficulty, Player Icons, Camera Zoom, P2 Teleport, P2 Bot.
- **The Co-op menu's background:** a still from the player's own game files (`stills3.BFF` key 2), resampled,
  darkened and swizzled at run time. Suite: menus.
- A first launch with no save goes to the title screen instead of straight into the prologue (the start-up handler's
  object `+0x3F8` set first).

### 6.8 Difficulty

| | Easy | Normal | Expert |
|---|---|---|---|
| Enemy damage | x0.75 | x1 | x1.5 |
| Heroes' max health | x1.5 | x1 | x0.75 |
| A downed hero's wait | 15 s | 30 s | 45 s |
| Enemies from spawners | as solo | each spawn brings 1 more | 2 more |

- Damage is scaled in the health guard; max health whenever the game sets it (level start, upgrades).
- The extra enemies come from the spawner's ready list (0x930F0), recycled as the game recycles its own; nothing can
  be built in play. Deeping Wall, 150 s: 0 extras at 1x, 31 at 3x, up to 18 pooled enemies in play at once (8 at 1x).
  Weathertop's placed enemies aren't multiplied.

### 6.9 Pickups

- A pickup touched by player 2 runs in its context, so it refills player 2. Bot: "touched by (P2), in his context |
  ammo P1 30 -> 30, P2 2 -> 42". Suite: pickups.

### 6.10 Ranged attacks for player 2

- Its projectile is its own hero's (Gimli's axe 16, Legolas' arrow 19, ...); with its own moves (6.6) the game
  already picks it. Its throws leave from its own ranged copy (Gimli's throwing-axe model E7EEC090): before, they left
  from player 1's bow, with no axe model, so they flew invisible. Isildur, who has no ranged weapon, gets a copy of
  Aragorn's bow. Its ammo goes down, player 1's stays. Suite: weapons, weapons_bow.
- Gandalf's and Sauron's beam (Extra Heroes) for player 2: started from its own hand height. The beam particles of a
  shot made inside player 2's context stayed for good, and so did player 1's own Gandalf's: their spawn (0x8E470) got
  a life of -1.0; now 0.6 s (in solo too).

### 6.11 The P2 Bot (a stopgap)

An option that plays player 2 until there are proper controller settings for a second player. Any real input on pad
2 hands it back to the player for 3 s.

- **The group:** player 1 and the companions (role 2) within 20 units of player 1. Centre = their mean; radius = the
  furthest member + 4, kept in 10-18 units; with no companion near, 8 units round player 1. It keeps inside (back to
  75 % of the radius), and steps away when closer than 3 units to player 1 with nothing to fight.
- **Melee:** the nearest enemy within 6 units of it, or the nearest inside the circle within 12. It walks right up to
  it (to 1.2 units) and swings only within 2 units, the stick turned toward it. An enemy it has never got within 2
  units of may be swung at within 3.5 after 1 s of getting no closer (big bodies; untested). No closer in 2.5 s: left
  out for 5 s. Its attacks are random combos (XXX, XXY, XYY, YY, XY, XXXY, Y, XX, YX), with now and then a block (1 in
  15 every 1.5 s when an enemy within 4 units is after it).
- **Ranged:** inside the circle, nothing near it, ammo left: a shot from where it stands at an enemy within 35 units
  (those after a hero first), once its lock is on the enemy (or after 1.2 s of LT). Out of the circle: back in first.
  Never during the Watcher's fight.
- **Player 1 down:** it roams, after the nearest enemy within 30 units (else a walk to a point 10 units off), with
  combos and now and then a parry; back to the group's rules once player 1 is up.
- **The Watcher:** near the nearest tentacle's strike point it taps White; a strike at it that costs no health counts
  as parried, then 3.5 s attacking the nearest stretched tentacle in the middle of its body with its target on it.
- **Moving:** the stick follows the shared camera (stick up = the camera's forward on the ground). It routes over its
  pathmap (breadth-first over walkable cells, 8 ways without corner cuts, in a 64 x 64 box; steering at the furthest of
  the next 6 cells in a straight walkable line, every 250 ms), with half of the last frame's direction kept (it
  zig-zagged). Stuck (under 1 unit in 1.5 s): a sidestep; more than 22 units behind for 3 s: put beside player 1.
- Bot runs (Balin's Tomb, Gimli, player 1 idle, 130 s): all 31 swing checks within 2 units of the enemy; 2-11 kills
  by player 2 per run (it varies with when player 1 goes down). Suite: checkpoint (at the Watcher it parries by a
  tentacle and never shoots).

## 7. Wrong leads (for the next one)

- A twin whose fields are copied from player 1 to make it whole: guesswork. The game's own set-up, replayed (5.3), is
  what worked.
- `@IsPlayerWeapon` (0xABD00) isn't on the hit path: 0 calls while player 1's hits did damage.
- The weapons' "target" `+0x420` (set by the listener callback 0x8FF50) is a HUD event, a result of hits.
- Running 0x5A450 a second time for player 2 moved it by 0: the control step, component update and that pass only set
  things up; slot 1 moves a character.
- "15 = Gimli's axe" for projectile ids: the ids come from each hero's own throw (4.13).
- The state machine's states don't drive the model's animation (4.6): raising inputs to make a downed hero lie down
  only worked with `INP_DIE`.
- The camera's vectors `+0x40` / `+0x50` aren't look-at and eye in every camera mode (1 unit apart in Balin's Tomb).
- Legolas' knife swinging 44-46 s into some runs was its idle fidget (it happened with player 2 idle too).

## 8. Open

- **The Watcher's tentacles take no hits** in bot runs from either hero (4.16), so how the fight is won, and what the
  bot should do there, is unknown. Next to read: the tentacles' hit volumes and contact masks, and the script object
  `makeTentaclesNotTAble`.
- **The lake before the Watcher:** player 2 has been held there wading (state 55, no motion) with the same flags as
  player 1; cause unknown. The boss checkpoint starts past it, so no bot reaches it.
- **Head markers drifting outward** from the screen's centre was reported once, zoomed out with the heroes apart, and
  not reproduced; the frustum projection may fix it (unconfirmed).
- **Combat sounds** were missing in some bot runs (every character's voices and hits; music fine), coming back at the
  cave troll's scene. Solo and co-op were equally loud second by second in audio dumps; seen only with
  `TT_SKIPMOVIES`, so taken as a test-only effect.
- Weathertop: no kills by player 2 in bot runs (unknown why); with `TT_SKIPMOVIES` the level stays black (it waits for
  `wea1_2.bik`).
- Not covered: a second physical controller (the bot tests drive pad 2 by script), Vulkan, 4:3, a hero downed
  mid-air or in a grab, big enemies for the P2 Bot, the multiplier's effect on wave ends and objectives.
- Upstream port: the README's "no extra packages" for `generate.py` (it needs capstone).

## 9. Test switches and tools

- The co-op code reads its test switches with `getenv` where they are used; each is described there. Groups:
  - set-ups: `TT_COOP_TESTEND="ms:code"` (EndMission), `TT_COOP_TESTDOWN="ms1,ms2"` (a hero down),
    `TT_COOP_TESTEXP` / `TT_COOP_TESTEXP1` / `TT_COOP_TESTXP12[B]` (level experience), `TT_COOP_TESTCKPT=ms` (a
    checkpoint), `TT_COOP_RESPAWNID=n`, `TT_COOP_TESTAMMO`, `TT_COOP_TESTUPG="hero:upgrade,..."`, `TT_COOP_TESTHP1/2`;
  - pads: `TT_COOP_PAD2SCRIPT`, `TT_P1SCRIPT` (with `TT_P1SCRIPT_INPLAY`), `TT_COOP_P1PORT=0` (player 1 on its own idle
    pad), steering test bots `TT_COOP_PAD2SEEK`, `TT_COOP_P1SEEK`;
  - off switches for each fix (`TT_COOP_NO...`), and research loggers (`TT_COOP_*LOG`).
- `TT_SKIPMOVIES` (kernel_bridge.c): movie opens fail as missing, so the game skips them (but Weathertop waits for its
  first). `TT_SHOTS="ms:file,..."`: frame dumps timed from the launch, menus included.
- The bot suite: 14 scenarios, 73 checks, in the fork's `tools/coop-bot/`. Last full run on the published build:
  "73 passed, 0 failed of 73 checks in 14 scenarios" (2026-10-21, 24 min).

## 10. Playbook gaps found

- `playbook/split-screen.md` covers multi-instance split screen only; **shared-screen, single-process local co-op in
  a source or recompilation port** (this project) isn't covered anywhere.
- `playbook/decomp-and-recomp.md` lists original-Xbox recompilation tooling as "not researched": there are at least
  xboxrecomp (sp00nznet) and XboxXBE-Recomp (TGP482).
