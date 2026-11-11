# TMNT (2007, PC): co-op notes

Addresses are for `TMNTGame.exe` 1.0.0.188 (SHA-256 in [README.md](README.md)), image base 0x400000, read from the
unpacked image in memory (OEP 0x0076B59F). The exe is UPX-packed with the UPX header stripped (`upx -d` refuses) and
has SecuROM traces; the game exes carry a RUNASADMIN compatibility flag, so memory tools need admin.

## Observed

- Log: "Engine v428-42", "Opening PC BF"; data archive `turtles.bf`. Script messages (`Switch_SwitchMainActor`,
  `Coop_SetPartner`, `Buttons_RegisterPad`, `Turtle_*`) are compiled into the exe (Jade AI2C).
- `TMNTcontrol.INI`: one `[GamePad]`/`[Keyboard]`/`[Mouse]` set, no player index; includes debug actions
  (`GHOSTMODE`, `KILLTHEMALL`, `SPECIALLOADMAPMENU`). `COOP=` is the team move, not a second player.
  `DevicesMapping.ini`: per-pad-model DirectInput button maps, no player slots.
- Main actors: `AI_MainActorGet` 0x004B4A50; main actor 0 at 0x014775F0, main actor 1 at 0x014775F4 (null in solo).
  Writing main actor 1 := idle turtle: no crash, nothing visible.
- Turtle id: obj -> [+0x18] -> [+4] -> [+0x48] -> +0x6A4 (1-4). Turtle AI struct: health float +0x174 (0-100),
  IO port +0x708 (0 controlled, -1 idle), present state +0x938 (2 ground playable, 3 jump/air, 4 fall, 11 air leapfrog,
  14..17 brother team attacks, 18 downed (revivable), 24 when player 1's health ran out with no brother able to revive
  him (the checkpoint reload follows about 2 s later), 25 ground co-op charge, 26 in toxic water, 0 absent).
- `Switch_SwitchMainActor` 0x00575AE0 only moves main actor 0 and the port; the old turtle stays alive.
- `Turtle_Spawn` 0x005C1F70 (id, spawn point, offset, port): teleports and makes present; port set via 0x004E5190.
  Spawning an idle brother with port 0 puts a second turtle on screen.
- Setting a turtle's port to 1 does not stop pad/keyboard from moving it: the port is a gate, not a controller index.
  Input comes from the shared IO object 0x014F2F60 (reads via 0x00565D20, in protected/virtualized code).
- Pad manager 0x0063DD40, four XInput pad objects at mgr+0x30..+0x3C; `GamePadX360` read ~0x00646B30 calls
  `XInputGetState([this+0xEC], this+0xD8)`. If `[0x0150218C]` (DirectInput joystick list at launch) is NULL and an
  XInput pad connects later, `0x0063DD40` dereferences NULL+4 and crashes.
- The game's active pad record at `[0x01502198]` (`0x160` bytes) and its wrapper at `pad+0x168` (`0xE8` bytes), totaling
  584 bytes, are updated each frame by `FUN_00639c80` (call site `0x00418E0B`). Swapping these 584 bytes around a second
  pipeline pass and around player 2's AI tick gives player 2 an independent input history when a second pad is connected
  (automated testlane scripts without a physical pad fall back to action and move vector interception).
- Solo-turtle levels (Level 1, *Mystical Jungle*) only register Leonardo (`id 1`) in the turtle manager (`0x1B059C01`).
  `FUN_00491c40` (engine object duplicate) with the turtle manager as caller duplicates Leonardo, but AI init opcode
  `0x32D9` calls `FUN_006ac0f0` (`ANI_ShareSkeleton`, call site `0x0049ECA1`), which makes the clone share player 1's
  skeleton (`anim+0x08`), saves the clone's own skeleton at `anim+0x10`, and sets `0x00200000` (`ForceInvisible`) on
  player 1. Calling `FUN_006ac230` (`ANI_UnshareSkeleton`) + `FUN_006abf40` (`kInitSkeletonBones`), patching AI
  self-references (`+0x49C`, `+0x1718`, `+0x26B8`), and clearing `0x00200000` separates both bodies.
- Team move partner lookup is centralized in `FUN_005759d0` (`GetCoopPartner`), which queries the HUD/family meter
  (`managerAi[25]` via `FUN_00565aa0`). Callers: `0x005DE230` (`Coop_TransitionToCoopMove`, air leapfrog `state 11`),
  `0x005380E0` (ground charge start -> `0x005221E0` `Coop_StartHoldTimer` -> `state 25`), `0x0053828A` (`CoopAttack`
  -> `states 14..17`), `0x00557614` (downed revive), and quick-tap tag switch callers `0x00538000` / `0x00601872`.
  `FUN_005c1df0` (`CheckCoopLock`) checks cooldowns, and `FUN_005c17c0` (`Turtle_LockCoop`) locks the brother and
  darkens the HUD portrait.
- `FUN_00538190` (`CoopAttack`) checks `self->identity == masterIdentity`: if equal, it runs the Master branch and
  recursively calls `FUN_00538190(partner, &ok, masterIdentity)` at `0x005382F4`. For a clone with the same identity
  (`1`), temporarily changing `partner->identity` during that call enters the Slave branch (`ai[+0x58C] = 1`).
- At the end of a team move, `FUN_00586590` calls `FUN_00585c50(slave, 0)` (absent) after the slave's animation leaps
  off-stage. Redirecting `state 0` to `state 2` and teleporting (`0x004A6150`) to player 1's ground position keeps
  player 2 in the level, though the off-stage leap still plays right before the teleport snap.
- Team-move timing (measured in the test copy): the master leaves `state 11` on the frame it is thrown (about 0.4-0.5 s
  after entering), while the slave stays in `state 11` ~1.2 s longer playing the exit leap; `state 14` ends for both
  turtles in the same frame and the slave does not leap there, only the teleport showed.
- The state-level fix (mod 77-85) ends the move on the slave one frame after the master leaves the co-op state
  (`11`, `14..17`): `FUN_00585c50(slave, 2)` or `4` (fall) is called from the AI tick, so the exit-leap animation never
  starts and the slave lands where it stands. The engine's later `state 0` is swallowed for 3 s, and the old placement
  now runs only when player 2 is really far (>3 m) from player 1; the stale `g_lastSafeGroundPos` height (which
  teleported the clone 3 m up after a family attack) is no longer used when player 1 is readable.
- `AI + 0x58C` (master/slave role) reads `1` for player 2 even outside a team move, so it is a leftover field, not an
  "in move" flag; `AI + 0x6C8` stayed `0` throughout the observed moves, and `state 11` sub-stage values `1/2/3` from
  earlier sessions were not reproduced in the 2026-10-27 runs.
- Cloned Leonardo in solo levels was missing the katana scabbards: they are a separate prop. Duplicating the prop
  (pointer at `0x014F4294`, or the turtle AI `+0x8FC`) and calling `Weapon_AttachLeoKatanaCase` (`0x005DEAC0`) on the
  copy attaches its own scabbards.
- Turtle identities: 1 Leonardo, 2 Raphael, 3 Michelangelo (nunchucks), 4 Donatello (bo), read from the turtle manager
  slots in Map 5 (slot i holds identity i). The manager's turtle list is at manager AI `+0x0C + 4 * (id - 1)`.
- In Map 5 (*Ninja Tag Time*) all four turtles are registered. `Turtle_Spawn` on an absent brother gives him an IO port
  and state 2, but his body was not drawn: status `120C00A5` (Culled set, Visible clear), draw LOD byte at object
  `+0x1C` = 0, while his weapon and bandana props were drawn. The game hides an absent brother through its control-flag
  setter `0x0045B1D0`; clearing `ForceInvisible` (`0x00200000`) in memory alone does not put him back on the draw
  lists. Setting the flag and clearing it again through `0x0045B1D0` does (status `120C00A3`, LOD `0x2C`), for all three
  brothers. Forcing `InPause` (`0x04000000`) on the object, as one earlier build did every frame, also kept him hidden.
- Writing a brother's identity (`+0x6A4`) or weapon skeleton (`anim + 4`) into a Leonardo copy makes its attacks resolve
  Leonardo's tables against the other brother's objects: the copy pulled katanas and stayed in state 13 without
  playing an attack. A copy that keeps Leonardo's identity and weapons attacks normally.
- State 13 is the fight stance after an attack; player 1 also stays in it for about 10 s, and moving the stick leaves
  it. It is not a lock.
- The game's projection matrix (`GetTransform(D3DTS_PROJECTION)` at EndScene) is `_11 1.4282`, `_22 -2.5390`,
  `_33 1.0`, `_43 -0.05`, `_34 1.0` in Map 5 at 1280x720: a vertical focal factor of `|_22| / 2 = 1.27` viewport heights.
  The turtle position (`[obj+0x0C] + 0x30`) is at the feet.
- The game reads keyboard (256-byte state) and mouse (20-byte state) through DirectInput `GetDeviceState` also while
  its window is in the background. Alt is the co-op key (`COOP=LMENU,RMENU`); held, it charges a team move (state 25),
  so Alt+Tab started team moves over and over. The ground charge needs the button held; a tap only asks
  `GetCoopPartner` (caller `0x005380E5`).
- When player 2 starts a team move, player 1 is the partner and `FUN_00586590` sets him absent (call returning to
  `0x005865E5`); he vanished and lost control until that `state 0` was redirected to 2 with IO port 0.

- Story missions: the world keys are the non-zero entries of the table at `0x008003B8` (read by `FUN_00405830`);
  mission index *i* (0-15) uses the *i*-th non-zero key, and `FUN_005f5110` maps the same 16 indices to their intro
  videos. Writing the key into the next-map slot (`[0x014F4530]`) and the index into `[0x014F4544]`, then forcing the
  index in the launch call `0x0040B380`, starts any mission from a new game.
- `FUN_00491db0` is the worker behind the script function `OBJ_Destroy` (`0x00491F10`, always on the calling object):
  a normal object is only hidden and deactivated through `0x0045B1D0`; one with the Generated identity flag `0x20000`
  (made by the duplicate `0x00491C40`) is freed. Checkpoint reloads and restarts also free Generated objects (not
  through this function).
- A duplicate of player 1 keeps pointers to three of player 1's own objects in its AI block (`+0x6C4`, `+0x930`,
  `+0xA7C`; `+0x8FC` is its own), for Raphael and Nightwatcher also the body object at `+0x49C`, and shares his
  animation data (`[[obj+0x14]] + 0x14/+0x18/+0x34`; `+0x30` is shared only while both play the same animation).
  Freeing the copy corrupted the heap: the next duplicate crashed in `FUN_006625e0` (`0x006ACCCA`). Clearing the
  shared animation pointers first crashed at once (still read). Keeping the copy from being freed works: clear the
  Generated flag `0x20000` before the reload, the reload then only hides it. Clearing the flag at creation made some
  restarts re-initialise the live copy and crash (2 of 9).
- Restarts from the pause menu and level changes run `ChangeMap_Execute` (`0x005F4C70`, return address `0x0055CF4B`
  in the tests; prologue `83 EC 44 A1 90 91 49 01`); it also runs at mission start. Its argument is the calling object;
  it moves that object's AI `+0xC0` into the mission data and clears it.
- The game's AI functions take actor codes as well as object pointers: 1 = main actor 0, 2 = main actor 1, 3 = the
  current object (e.g. `0x005762F0`, `0x0052EC05`). Guard hooks that treated values below `0x00400000` as invalid broke
  enemy death sequences (a dying enemy with `[obj+0x18] = 0` was still run; crash in `FUN_004fc3f0`, `+FC3F7`).
- The retail game registers debug input actions (`FUN_005912a0`: `0x16` Death, `0x19` KillAllEnemies, `0x2E`
  SpecialLoadMapMenu, `0x2F` StartMenu_StartNextMap, ...) but never queries them.
- The pause menu puts a second turtle into state 18 (downed) within seconds (the brother is parked under the level).
- Player 1's death (state 24, no brother able to revive him) reloads the checkpoint about 2 s later and frees
  Generated objects; a kept (non-Generated) copy is re-initialised there as a visible idle turtle.
- In toxic water the game puts main actor 0 back on safe ground after about 3 s; a second turtle stays in state 26
  until downed. Position writes (`0x004A6150`) to a turtle in state 26 keep z but x/y snap back.
- Crash `TMNTGame.exe+2B3E91`: `FUN_006b3e80(a, b, flag)` tests `[b+4]`; the AI function at `0x0052EC05` passes main
  actor 0 as `b`. It crashed when the mod swapped main actor 0 to a null player 2 around enemy AI.

## Derived

- Swapping the 584-byte pad and wrapper buffer (`[0x01502198]`) solves per-player button/stick history in Level 1
  without per-action emulation when a physical pad is present, though cloned Leonardo still shows a combo action index difference
  (`7`/`11` vs `8`) on attack presses.

## Unknown

- Store/edition of the tested copy; whether other builds match.
- Behaviour of player 2 in boss fights, cutscenes and level transitions; no mission was played to its end.
- How to make a duplicate of player 1 that shares none of his data.
- How a level change behaves with player 2 (not reached by any test).
- The `27A2AE` crash (null call) has not been seen since its two known triggers were fixed (Alt held in the background
  starting team moves, and a teleport during restart teardown); its exact cause was not traced.

## Checks that actually ran

- Self-test script (`selftest both`) and team-move script in a windowed test copy of Level 1 (2026-10-26 to 2026-10-27),
  recorded from the D3D9 back buffer.
- 2026-11-01 and 2026-11-02, same test copy with a virtual controller for player 2: in Map 5 each of Raphael,
  Michelangelo and Donatello as player 2 (4-hit armed combo, unarmed attack, walk, jump); in Level 1 the Leonardo copy
  (same moves), a team move started by each player, and 6 s idle with team moves allowed (nothing started by itself).
  Checked by looking at the recorded frames one by one. The recordings stay on the developer's machine; they are not in
  this repo.
- 2026-11-03/04, `suite.py` in the test copy: all 16 story missions, 10 pass/fail checks each, judged from the mod's
  log; results and failures are listed in [README.md](README.md#qa). `p1_death.py` (player 1 killed by enemies in Map 8
  with a copy as player 2) and `probe_map.py` (frames of one mission) were used to find the hazard and death crashes.
- 2026-11-05/06: mutation runs (`suite.py --mutant`), repeated suite runs, `depth.py` (revives, team moves, framing,
  enemy damage with an idle control), `stress.py` (restarts, leave/join, 20-minute random input), `combo_crash.py`
  (fights; used to find the guard hooks behind the fight crash by removing them in halves). Results in
  [README.md](README.md#qa).
