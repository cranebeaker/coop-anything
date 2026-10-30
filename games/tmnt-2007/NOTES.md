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
  14..17 brother team attacks, 18 downed/dead, 24 revive, 25 ground co-op charge, 0 absent).
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
- Cloned Leonardo in solo levels is missing the katana scabbards (the blades are there): the bodies share the same
  `anim[5]/[6]` shape pointer, every skeleton bone's visual geometry matches player 1's, and copying `anim[1]` (which is
  null on the clone) does not restore them, so the scabbards are most likely separate prop objects that the engine's
  object duplication does not copy. Not investigated further.

## Derived

- Swapping the 584-byte pad and wrapper buffer (`[0x01502198]`) solves per-player button/stick history in Level 1
  without per-action emulation when a physical pad is present, though cloned Leonardo still shows a combo action index difference
  (`7`/`11` vs `8`) on attack presses.

## Unknown

- Store/edition of the tested copy; whether other builds match.
- Behaviour of player 2 and team moves across multi-turtle levels, boss fights, cutscenes, and level transitions.

## Checks that actually ran

- Automated self-test (`selftest both`) and team-move script (`coopmove`) in an isolated windowed test copy of Level 1
  (2026-10-26 to 2026-10-27), recorded from the D3D9 back buffer (`selftest_170644.mp4`, `team_raw.mp4`,
  `team_fixed.mp4`).
