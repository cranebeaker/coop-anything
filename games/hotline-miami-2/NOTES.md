# Hotline Miami 2: Wrong Number — identity (owner's install, read-only)
- Store: Steam (app 274170), Windows
- Install: <game dir> (Steam library)
- Main exe: HotlineMiami2.exe, 26,086,400 bytes, PE32 x86 (32-bit), link timestamp 2017-10-20 13:21:32 UTC
- SHA-256: 56fde3b1e4bbb650e1afdfcafaa5585ee93330f035b907c0ef5528781b88fb80
- Engine: "GameBaker 2.0 & SilverWare by Abstraction Games" (GML converted to native C++; not a GameMaker runner)
- Renderer: D3D9 or OpenGL via SDL2; audio FMOD; Steam via steam_api.dll (32-bit)
- Data: hlm2_data_desktop.wad (94 MB), hlm2_music_desktop.wad, hlm2_patch_desktop.wad ("AGAR" header)
- Netcode in exe: none found (no SteamNetworking / lobby strings) -> Path 2, build from zero

## Session 1 (2026-10-20)

### Observed
- Data archive `hlm2_data_desktop.wad`: magic "AGAR", v1.1; header = magic, u32 major, u32 minor, u32 extra-len, extra,
  u32 file-count, then per file {u32 name-len, name, u64 size, u64 offset}; then u32 dir-count, per dir {u32 len, name,
  u32 count, per entry {u32 len, name, u8 type}}; file data base = end of that index (455028 in this build). 4522 files.
- Game logic tables live in the wad, not the exe: `GL/hlm2_objects.bin` (2414 objects), `GL/hlm2_rooms.bin` (321 rooms),
  `GL/hlm2_collision_events.bin`, `GL/hlm2_sprites.bin`. Object/room names are absent from the exe (GML compiled to C++,
  objects referenced by integer id).
- `hlm2_objects.bin`: u32 count, then 44-byte records starting with the object id, a name-offset field near the end;
  names are a null-terminated string pool at 97580. Ids: objPlayer=36, objEnemy=50, objPlayerSwan1=266,
  objPlayerSwanChainsaw=1051, objPlayerSwanGun=1052.
- Player variants present: objPlayer, objPlayerTiger, objPlayerZebra, objPlayerBear, objPlayerCobra, objPlayerHenchman,
  objPlayerSwan1/SwanChainsaw/SwanGun, objPlayerGhost, objPlayerDead.
- No Steam networking/lobby strings in the exe; Steam is used for achievements/leaderboards/workshop only.

### Derived
- The Swan levels (Alex & Ash) already run two player-type objects at once, one driven by AI. Replacing the partner's
  input with a remote player's input is the smallest first co-op slice. (Not yet verified in code.)

### Guesses (unverified)
- Other 44-byte record fields: sprite id, depth (-100 for objPlayer), parent id. Field order not confirmed.

### Unknown
- Where the exe's object-event functions are dispatched; per-instance struct layout (x, y, direction, weapon).

### Tooling notes
- Cloud workspace: PyPI/npm/Maven and GitHub release downloads are blocked; git clone from GitHub works.
  radare2 built from source (radareorg/radare2 @ 6211fbee, v6.2.5). Ghidra release zip not reachable.

### Code mapping (radare2)
- Observed: an init function (around 0x43dbed) registers objects: `mov ecx,[0x19ba408]; push <id>; call 0x12d7fe0 (lookup by id); push eax; mov ecx,<16-byte global handle>; call 0x12a14d0`.
  Handles: objPlayerSwanChainsaw(1051)=0x199fea8, objPlayerSwanGun(1052)=0x199feb8, 1053=0x199fec8, 1054=0x199fed8.
- Observed: code references to these handles: SwanChainsaw 68, SwanGun 85.
- Lesson: most raw hits for small ids are MSVC SEH state writes `mov [ebp-4], N`, not object refs. Search for the handle address instead.
- Derived: the handle array is not one uniform id*16 array across all ids (objPlayer's computed slot has 1 ref); map handles per id from the init function instead.
- Observed: radare2 `aa; aac` finds 5944 functions (incomplete for a 26 MB exe). Functions referencing the Swan handles
  include fcn.01123ee0 (15.5 KB, 6 refs), fcn.00f2bdb0 (4), fcn.01150350, fcn.00fc7260, fcn.00f4e300, fcn.00f3ec90,
  fcn.0041bbe0 (3 each). Not yet identified which is Step/Create/Draw of which object.
- Observed: object id->handle registrations matched by byte pattern: 2216 of 2414 ids.
- Caveat: my name-table parse of hlm2_objects.bin has false positives (some names appear under several ids);
  ids 1051/1052 for the two Swan player objects match consecutive registrations in code but the name link is derived.

### Next steps
1. Dynamic analysis on the owner's Windows PC (needs owner OK to install x64dbg): break on reads of the 1051/1052
   handles in a Swan level to label Create/Step/Draw and find the partner-AI input path.
2. Find the per-instance struct (x, y, direction, sprite, weapon) from Step code; confirm with a memory read in-game.
3. Proof of concept: DLL proxy (e.g. a d3dx9_43.dll or SDL2.dll proxy in a *copied* install) that overrides the AI
   partner's movement from a local second controller, before any networking.
4. Then Steam relay (Spacewar 480) lockstep-ish input sharing per playbook/networking-steam-spacewar.md.

### Session 1, dynamic (x32dbg 2026.05.27, test copy, windowed)
- Observed: ASLR on; base was 0x5D0000 in this run (static VA = runtime - base + 0x400000).
- Observed: x32dbg hung the game at the save notice screen until exceptions were passed to the game
  (ini `[Exceptions] IgnoreRange=00000000-80000002:...,80000005-FFFFFFFF:...`). The game raises MS_VC_EXCEPTION 406D1388
  to name its threads (AgAudioManagerFMOD, AgThread, AgRenderContext).
- Observed: the 16-byte object handles are GML values: double at +0, type tag at +8 (3 = reference, followed through
  3 pointer hops).
- Observed: with hardware read breakpoints on the SwanGun/SwanChainsaw handles during ~30 s of the first level,
  ~185 hits each, all inside generic value helpers: 0x12a1872 in fcn.012a1860 (value->int, then object lookup
  0x12d7fe0; 1231 callers), 0x12ac60b in fcn.012ac5c0 (1392 callers), 0x44dd8a in fcn.0044dd40 (value compare; 4697 callers).
- Derived: some code checks the Swan player objects regularly even in a non-Swan level (e.g. instance_exists in a
  controller step). Next: log the caller of those helpers.
- Observed (caller logging, same run): the Swan handle reads come from 3 real call sites (static VAs):
  - fcn.0041bbe0 (4.2 KB) @0x41c1e1: `call fcn.012ac5c0(out, &objPlayerSwanGun)`, result compared `> 0`
    (fcn.012ac5c0 looks like instance_number/instance_exists). Called from 0x76e605 and from fcn.00e6a750.
  - fcn.00e6a750 (3 KB): queries objPlayerSwanGun at its start; called from 0x76efa7.
  - fcn.00faca20 (861 B): references every player variant (Zebra, Bear, Cobra, Henchman, SwanChainsaw, ...);
    guess: "which player character is active" helper. Called from 0xa69ee3 (fcn.00a69eb0).
  - 0x76e605 / 0x76efa7 are inside fcn.0076e190 (22 KB), which references objPlayerDead, objPlayerBear, mask/style
    menus. Guess: objPlayer Step or the level controller's Step. Unverified.
- Caveat: object-name mapping from my hlm2_objects.bin parse is noisy (e.g. objShardRenderer shows up for many
  handles). Trust handle addresses and ids, not the names, until the record layout is pinned down.
- Hit cadence: GUN, GUN, SAW repeating ~185 times in ~30 s of play under HW-breakpoint logging (game slowed down).
