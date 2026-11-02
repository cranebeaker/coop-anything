# MDK2 HD: working notes

## Game identity
- Store: Steam, app 213350, build id 3902
- Exe: `mdk2hd.exe`, PE32 (32-bit x86), SHA-256 `bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43`
- Files: `mdk2hd.exe`, `steam_api.dll`, `mdk2.tar` (1.6 GB)
- Renderer: OpenGL 3.0 (exe string "MDK2 HD requires OpenGL 3.0."); input: imports `DINPUT8.dll`
- OS tested: Windows 11

## Observations (checked)
- `mdk2.tar` is a plain GNU/ustar tar, flat (no directories), 5842 entries:
  2210 .mod, 1808 .dds, 998 .wav, 692 .bsp, 101 .lua, 27 .mus, plus mdk2.str, stars.sta, shading.shd, a font.
- The 101 .lua files are plain-text Lua source (CRLF), not bytecode. Exe embeds Lua 3.1 ("Lua 3.1 Copyright (C) 1994-1998 TeCGraf") with tolua bindings.
- Boot script is `mdk2.lua`; it dofile()s strings, menuinit, script, effects, debug. Levels are `levelN.lua` + `lN.lua` + `lightsN.lua`.
- Player characters are created from Lua: `CreateKurt()`, `CreateMax()`, `CreateDoctor()`, `CreateHyde()` (mdk2.lua ~l.800-860). `start.lua` is just `CreateKurt()`.
- `debug.lua` ships in the archive ("Do not ship this file") with debug key bindings (F9 debug menu, jet cheat, free camera).
- Engine-side Lua API prefixes: `om*` (objects/gobs/anims/commands), `ch*` (core: sound, video, input, `chRecordInput`), `mdk*`, `vv*` (var viewer), `do*`. ~460 such names in exe strings.
- No Lua io library (`writeto`/`readfrom` absent); `dofile`/`dostring` present.
- Exe strings reference `override\`, `override\<language>\`, `scripts\`, `script\lights%d.lua`, `save\auto.sav`, `save\Auto%d.sav`, `settings.lua`.
- Game creates `%LOCALAPPDATA%\MDK2HD\` (empty after several launches).

## Tried
- Launched the test copy directly (with `steam_appid.txt` = 213350) via PowerShell Start-Process.
  Process stays alive 30 s+, but screen captures at 8-32 s all show the Interplay logo, Escape via SendKeys did not visibly advance it.
- Loose `menuinit.lua` containing a deliberate Lua error placed in `override\`, `scripts\`, `script\`, `override\english\`: no observable effect,
  but INCONCLUSIVE because the game never visibly got past the Interplay logo in automated runs.

## Session 1 results (2026-10-20)
Automation: game in windowed mode (800x600). Keys must be sent as scancodes via SendInput (KEYEVENTF_SCANCODE)
for DirectInput to see them; .NET SendKeys does not work. Window capture must be DPI-aware (SetProcessDPIAware)
or it grabs the wrong screen area. Escape skips intros; two Escapes reach the title menu.
The earlier "stuck on Interplay logo" was both of those problems, not a hang.

- `override\`, `scripts\`: a loose `menuinit.lua` containing only an error was IGNORED (menu loaded normally).
  Loose-file override does not apply to Lua scripts (may apply to other assets: untested).
- `%LOCALAPPDATA%\MDK2HD\settings.lua` is written by the game (video/audio/key bindings as Lua calls) and
  executed at boot AFTER mdk2.lua: appending `chSetVideoMode(...1024,768...)` changed the window size, and
  `if CreateKurt then ... end` took the defined branch. So it is a working injection point that can redefine
  any game Lua function.
- `dofile()` only reads from mdk2.tar: `dofile('C:\\coop\\mod\\coop.lua')` silently did nothing.
  `dofile('debug')` (archive name) works.
- `mdk2.lua` guards `dofile('debug')` with the Lua 3.1 preprocessor `$if features`. Appending `dofile('debug')`
  to settings.lua enables BioWare's developer debug menu: F9 in gameplay shows God Mode, Spawn Object,
  Spawn Doctor Item, Warp To Point, Play Movie, Toggle Bars/Status, Varview, Lights, Ambient,
  Switch Player Character, Spawn Character, Show Scene Graph, Record Demo, Play Demo, Load Game.
  F9 does nothing on the title screen (only bound for game/menu control schemes).
- Final boss (boss.lua ~l.548-640) spawns the two non-player heroes with AI in the same scene as the player:
  `mdkCreateObjectLua("kurt", OBJ_KURT, ...)`, `mdkKurtInitAI`, `mdkKurtAIHeadToGob`, `mdkKurtAISetPosition`,
  same for Max/Doc (`mdkMaxAIGiveGun`). Engine therefore supports several hero gobs at once.
- Position API exists: `mdkGobSetPosition`, `mdkGobSetPositionXYZ`, `mdkGobSetOrientation`, `mdkGobSyncSetPosition`,
  `mdkGetPlayerGob`, `mdkGobDistancePoint`. Input record/playback exists: `chRecordInput`, `mdkPlayDemo` (useful for bot QA).

## Unknown
- Whether a Lua getter for gob position exists (needed for sync); none seen by name yet.
- Whether Lua can do any networking/file I/O (no io lib), so the transport probably needs a native DLL (dinput8 proxy).
- Whether loose-file override works for non-script assets.
- Any existing MDK2 multiplayer/co-op attempt (none found in DIRECTORY.md, repo issues/PRs, one web search).

## Session 1b: second player puppet (2026-10-20)
Mod code: separate repo `mdk2-coop` (local for now), `lua/coop_dev.lua` injected via `tools/inject.sh`.
Checked (screenshots):
- `comfuncs.fkey[...]` handlers fire from any control scheme, including the title screen.
  Rebinding unused debug commands works: `omMakeCommand(COM_TOGL1, "F6", CON_BUTTON_FALL, 0, 0)`.
- `mdkNewGame(1, 2)` from the title screen loads level 1 checkpoint 2 directly (Kurt on foot).
- Gob fields are readable from Lua: `gob.position.x/y/z`, `gob.orientation.yaw` (debug.lua COM_PRINTINFO).
- `mdkCreateObjectLua("coop_p2", OBJ_KURT, mdkGetScene(), nil, nil, 0,0,0, 1,0,0,0, nil,0,0,0,0)` spawns a
  second Kurt next to the player Kurt. With `mdkSetLuaEvent(p2, "OnUpdate", fn)` calling
  `mdkGobSetPositionXYZ` each frame, it tracks the player (pose frozen mid-animation: no anim sync).
- `mdkKurtInitAI(p2)` + `mdkKurtAIHeadToGob(p2, player)` makes it walk to the player with correct animation.
Derived: a remote player can be shown either as a teleported puppet (needs anim sync) or by steering the
game's hero AI toward the remote position (animation for free, precision unknown).
Unknown: orientation setter semantics, whether the AI Kurt can shoot/take damage sensibly, how a second
hero gob interacts with checkpoints, level transitions, saves, cutscenes.

## Session 1c: native bridge (2026-10-20)
Checked:
- mdk2hd.exe is wrapped in SteamStub: entry point in `.bind`, `.text` entropy 8.0 on disk. Not unwrapped.
  Instead a dinput8.dll proxy (game imports DINPUT8.dll) dumped the in-memory image at the first
  DirectInput8Create call; `.text` entropy there is 6.63 (decrypted). Analysis uses that dump only, kept in local/.
- Proxy DllMain runs before the game initialises Lua, and `.data` is writable then.
- Lua 3.1 builtin table {name, fn} at 0x5064f8..0x5065d0 (setfallback .. type). `copytagmethods` entry at 0x506520
  (name string 0x4d7d34); no game script uses it. Repointing it to a C function before Lua starts gives Lua a
  global that calls into the DLL. Works.
- Lua C API addresses (build 3902, cdecl), found by disassembling builtins dostring/tonumber/setglobal and the
  engine's registration code:
  | addr | function |
  |---|---|
  | 0x476690 | lua_getparam(int) |
  | 0x476a20 | lua_isnumber(obj) |
  | 0x476a60 | lua_getnumber(obj) -> double |
  | 0x476bb0 | lua_pushnumber(double) |
  | 0x476c40 | lua_pushstring (derived, untested) |
  | 0x476c70 | lua_pushcclosure(fn, n) (derived) |
  | 0x476930 | lua_setglobal(name) (derived) |
  | 0x478100 | luaL_check_lstr(narg, &len) |
  | 0x478de0 | lua_dobuffer(buf, len, name) (derived) |
  | 0x4776b0 | tolua_function(module, name, fn): engine registers mdk* functions with it (e.g. 0x442893) |
- Bridge registers `coop_send(x,y,z,yaw)` / `coop_recv()` via tolua_function(NULL, ...) and sends UDP.
- Two instances run at once from two folders (second folder hardlinks mdk2.tar). Both share
  %LOCALAPPDATA%\MDK2HD (settings and probably saves).
- Two-instance test, level 1 cp 2: A logged `send #2700 (85.25, 186.00, -30.00)`, B logged
  `recv #2700 (85.25, 186.00, -30.00)`; B logged `send #2400 (83.52, ...)`, A `recv #2400 (83.52, ...)`.
  Screenshot of B shows the puppet Kurt at A's spot.
- `gob.orientation` is nil from Lua (indexing it errors "indexed expression not a table" every frame).
  Lua errors print on screen in game, which is useful for debugging.
Unknown: how to read/set facing (orientation) from Lua; whether unfocused instances keep running at full rate.

## Session 1d: orientation and animation (2026-10-20)
Checked:
- OMGOB field accessors registered with tolua_tablevar (0x4774f0) on class "OMGOB" (0x4c3058):
  position get 0x437af0 / set 0x437b30 -> gob+0x18 (3 floats);
  orientation get 0x437ba0 / set 0x437be0 -> gob+0x24 (4 floats, setter copies raw).
  `gob.orientation` from Lua is a quaternion userdata with no fields, so `.yaw` errors.
- Quaternion layout is (w, x, y, z): identity logs as (1,0,0,0); turning with X gave (0.913, 0, 0, -0.408).
  z is up (floor at z = -30), Kurt with quat (0,0,0,1) walks toward -y.
- Anim system: gob+0x60 = controller; controller+0x24 = array of 40-byte state blocks,
  controller+0x28 / +0x2c = current / previous block. `0x44c640(gob, anim_id)` returns the block
  (model+0x68 is a byte map id -> slot). Native omAnimPlay = 0x44ca90(gob, id, flags),
  omAnimIsPlaying = 0x44cc20(gob, id).
- Block contents seen: [0] playing, [1] mode (1 normal, 0xa overlay), [3]/[4] time, [5] rate,
  [6] weight, [7] 4.0, [9] 1. No pointers. Kurt idle = anim 107 (also anim 1), walk = 22, jump overlay = 77.
- Copying the active blocks plus current/previous ids from one Kurt to another every frame makes the
  puppet's states match (dumped: puppet anim 107 time 0x3f313b87 vs player 0x3f3113b5). Loopback test
  with the puppet drawn 3 units above the player: same idle pose, puppet legs in a walk cycle while walking.
- Two-instance test: A's quaternion (0.913,0,0,-0.408) received by B; puppet in B visibly re-oriented.
- debug.lua binds D (free camera), I (prints orientation.yaw -> the recurring "indexed expression not a
  table" error), 0/9 (game speed) and more; clearing them with omMakeCommand(COM_x, "") fixes input clashes.
- Space is not jump in default bindings (jump is mouse button 1001); it toggled sniper mode.
Unknown:
- A puppet placed 2 or 6 units along -y from the player was not visible from the default camera although
  its logged position was right; 3 units up was visible. Not explained (camera direction vs. room culling).
- Whether a puppet that leaves its spawn room stays rendered (rooms: mdkAddRoom, mdkRoomSetBB, mdkRoomAddVisible).
- F7 sometimes spawns at x=80 (open area) and sometimes x=105 (by a wall): SOLVED in session 2, the solid
  puppet overlapping the player at the shared spawn point pushed the player into the wall.

## Session 1e: rooms (2026-10-20)
Checked:
- Level 1 rooms are scene-graph entries (l1_r2, l1_tun05crv, ...) with `visible` lists; only a few have
  bounding boxes (bmin/bmax), e.g. l1_r2sky. `mdkGetCurrentRoom()` returns the room gob (userdata).
- `mdkGobSetPositionXYZ(player, 133, 46, -40)` (level 1 cp3 spot) moves the player into a different room
  without a reload; mdkGetCurrentRoom() changes.
- Loopback: puppet drawn 3 units above the player stays visible after that teleport.
- Two instances: A teleports to the cp3 room, then B does too; B draws A's puppet at A's spot in the new
  room. So a puppet moved by mdkGobSetPositionXYZ is re-filed into the right room; no room code needed
  for this case.
- F12 is Steam's screenshot key; dev keys avoid it.
Unknown: whether a puppet in a room the viewer can see into (not the viewer's own room) is drawn;
the earlier "puppet 2-6 units ahead not visible" case is still unexplained.

## Session 1f: smoothing (2026-10-20)
Checked (two instances, A walks 7 s, B receives with simulated 120 ms latency, 40 ms jitter, 5% loss, 30 Hz sends):
- Without interpolation (latest packet): steady-walk window stalled 83% of frames, max step 0.97, cv 2.45.
- With 100 ms interpolation buffer: stalled 0%, max step 0.096, cv 0.18 (cv = sd/mean of per-frame step).
- An unfocused instance keeps running at full rate (330 frames per 2 s window at ~165 fps).
Derived: anim fields [3]/[4] behave like a 0..1 play position (all observed values < 1); interpolated with wrap.
Unknown: whether interpolating the anim play position looks right on screen (only position smoothness was measured).

## Session 1g: damage (2026-10-20)
Checked:
- Lua event names are a char[16] table at 0x4f6370 indexed by event id (OnUpdate 0, OnCollision 1,
  OnDestroy 2, OnDamage 3, OnHear 4, ..., OnDie 9, OnEnterRoom 10, ...). 0x40cf60(gob, id) = has Lua handler.
- Central damage function 0x40d590(from, target, damage, damagetype, part), cdecl, 31 call sites. Calls the
  gob's Lua OnDamage if set, else a per-class C handler (class table 0x4f64a8[gob+0xb4 & 0x3f] + 0x14).
  Hit points: *(short *)(*(gob+0x84) + 0x12). Prologue 55 8b 6c 24 0c, hookable with a 5-byte jmp after
  SteamStub has decrypted .text (hook installed from coop_init).
- Gob name: inline char[] at gob+0x4 (found by searching the player gob for "bob").
- Observed through the hook: Kurt's gun = 2 damage, type 0x1, part -1 (or 2 on some scenery); Doganboy hits
  Kurt for 2-8, type 0x2. Shots that miss damage level geometry gobs (l1_r2plate, l1_r2pillars, l1r3_wall, l1_r2).
- settings.lua is read into an 8 KB stack buffer (function around 0x42c4b0, sub esp 0x2004): a settings.lua
  over 8192 bytes crashes the game at boot with 0xc0000409 (stack cookie). Mod Lua now runs from
  coop\coop.lua via lua_dobuffer (0x478de0, status 0 on success); lua_pushstring 0x476c40 confirmed working.
- Two instances, host + client, both spawn "coop_dog1": the client shot it 50 times; host log
  "event 1 on coop_dog1 ... hp 98 ... hp 0" and client log "event 2 on coop_dog1 ... hp 98 ... hp 0" match on
  every hit; it died in both games.
- The host's Doganboy did not attack the client's puppet in this test (no HURT events), and the client's own
  Doganboy AI still attacked the client (each game runs its own enemy AI; host-driven enemies are next).
Unknown: whether enemy AI ever targets a non-player OBJ_KURT; unnamed (spawner) gobs have no network id yet.

## Session 1h: enemies (2026-10-20)
Checked:
- Gob type: *(short *)(*(gob+0x84)) (native mdkGetGobType = 0x40cf20). The class index used by the damage
  code is *(game object + 0xb4) & 0x3f where game object = *(gob+0x84) (NOT gob+0xb4: reading that
  gave garbage indices and crashed). Class descriptors: 0x4f64a8[class] -> 14 function pointers.
- Engine gob table at 0x524e10: 0x800 entries {gob *, u16 generation, u16}; handle = gen<<16 | slot
  (0x449560 = mdkGetGob(handle)). Level 1 cp2: 425 gobs with game objects.
- OBJ type numbers (logged from Lua globals): 200-254 creatures (200 BIF, 202 GRUNT, 207 DOGANBOY,
  217 POOPSY, 230 TURRET, 240 SPHEREOFDOOM, 250-254 CONEHEADCIV), 300s items/weapons/pickups,
  400s shots (403 DOGANBOYSHOT, 455-459 TURSHOT), 500s level devices (540 SPAWNER), 600s explosions,
  700s doors, 800 SCENERY, 802 STATICLIGHT, 803 ROOM, 1101 AMBIENTSOUND.
- omGobEnterStasis hides the gob entirely (not drawn, no AI, not hittable): not usable for puppets.
- Profiling class 4 (Doganboy family) by wrapping descriptor slots in counting thunks: slots +0x4 and +0x1c
  run once per frame. Replacing +0x4 with "xor eax,eax; ret" stops the AI (no attacks); the gob stays
  drawn and hittable. Same slot exists in classes 13 (sphere of doom) and 19 (turret).
- Natives: set position 0x40dda0(gob, float x, y, z) (behind mdkGobSetPositionXYZ);
  delete 0x449710(gob) (behind omGobDelete); lua_pushusertag 0x476ce0(ptr, tolua_tag("OMGOB")) pushes a gob.
- Two-instance test (host + client, level 1 cp2): host announced 24 placed actors; the client bound all
  24 by name (same handles on both sides). A host-spawned Doganboy appeared on the client at the same
  spot. The host's Doganboy AI attacks the client's puppet (non-player OBJ_KURT) as well as the host's
  player. The mirrored Doganboy on the client still deals damage through its copied attack animation;
  the client now ignores damage to its player from actors/projectiles and takes only the host's HURT
  (client hp 91 after one 9-point HURT). Client fire killed the host's Doganboy, hp matched on every hit,
  and its DESPAWN removed the client's copy.
Unknown / next:
- Projectiles are not mirrored (the client does not see the host's enemy shots); client weapons whose
  damage comes from a projectile gob (grenades, sniper rounds) are not forwarded because `from` is the shot.
- Pickups (300s), doors (700s), level scripts/triggers, checkpoints and level changes run independently.

## Session 1i: shots (2026-10-20)
Checked:
- Per-gob update (around 0x42b3f0) calls the class think as think(gob): slot +0x4 normally, slot +0x0 when
  the gob is the player-controlled one (compared with [0x5494c8]).
- Creating a shot with mdkCreateObjectLua("", OBJ_DOGANBOYSHOT, ...) on the client made the game exit
  silently (no WER event): the Lua "exit(1). Unable to recover" path.
- Doganboy fire: 0x434e5c checks type 0xcf (207), 0x44db50(gob, "slot name") picks the muzzle,
  0x42e030(shooter, slot, shot type) computes muzzle position/direction (0x44e580) and the target
  (handle at shooter+0x88 -> 0x449560) and calls create_object.
- create_object = 0x42d360(name, type, scene, parent, slot, &pos, &quat, model, a8, a9, a10, a11), cdecl,
  31 call sites; prologue 8b 44 24 04 83 ec 0c (7 bytes, no relative addressing). For shots parent = target,
  a8 = shooter's gob handle (damage is credited to it). Main scene pointer at [0x549500].
  Kurt's sniper rounds (406) are created with parent NULL and a8 = 0.
- Sniper round hits are credited to Kurt (bob, 25 damage, type 0x9), not to the projectile.
- Replaying FIRE through the original create_object works both ways: host Doganboy fireballs (403) show
  on the client, client sniper rounds (406) show on the host; damage counted once (HIT path).

## Session 1j: pickups (2026-10-20)
Checked:
- Lua event indices: table at 0x4f6370, 16 bytes each: 0 OnUpdate, 1 OnCollision, 2 OnDestroy, 3 OnDamage,
  4 OnHear, 5 OnAnimLoop, 6 OnCustomKey, 7 OnTimer, 8 OnCreate, 9 OnSpawn, 10 OnDie, 11 OnEnterRoom,
  12 OnShotLanded, 13 OnShotExploded, 14 OnOutOfAmmo, 15 OnPickedUp, 16 OnItemUsed, 17 OnModeSwitch,
  18 OnItemEquiped.
- has_lua_event(gob, 15) is asked only on pickup paths: 0x408d70 (Kurt, after inventory add 0x414e50 on
  +0x6c), 0x419071/0x4190ca (Max, +0x94), 0x4222ef (Doc, +0x64), 0x4157c4 (item used on the spot, after
  deleting it with 0x449710). Prologue 53 56 8b 74 24 0c. One path also passes the player gob ("bob").
- Level 1 items (type 300-399, class 6) are named: l1_r3health01-03 (313 apple), r3_ham01 (314), l1_r3grenade
  (317), l1_r3dummy (319 decoy) at (140, 46, -42), ...
- Two instances: client stood on l1_r3dummy -> PICKUP -> host deleted its copy; the host standing there later
  picked nothing up.
Unknown: items dropped at runtime (unnamed) are not synced; both players can pick up the same item if they
touch it within one round trip.

## Session 1k: doors and rooms (2026-10-20)
Checked:
- Proximity door (OBJ_PROXDOOR1 700, class 17) think 0x4251a0: player = gob_by_handle([0x5494c8]); state =
  *(game object + 0x40) -> { open, hold, ?, radius (float), locked }; opens (anim 0x42) within radius when not
  locked, closes (anim 0x43) otherwise. Only the local player counts. dr1_01 radius 20, dr1_03 radius 15.
  dr1_02 is locked in Level.Init and unlocked by r2_dog01.OnDie (level1.lua).
- Gob flags word at gob+0xa6: 0x8 = stasis (0x449470 sets, 0x4494a0 clears); the scene update
  (0x457xxx -> 0x44b620 per gob, recursive over children) skips stasis gobs and their subtrees;
  0x2 = has been updated once. A gob with a Lua OnUpdate gets that instead of its class think (0x42b390).
- Room manager 0x4302a0 (called from 0x40262a): marks every room inactive (+0x24 = 1), finds the camera gob
  (handle [0x5494b4], 0x42ae90) and calls mark_rooms(arg, camera, 1) = 0x430020, which activates the room whose
  box (+0x2c / +0x38, point_in_box 0x453fc0) holds it plus its visible rooms (+0x4 count, +0xc array), fires
  OnEnterRoom (event 11), music (0x42f0b0), sound env (0x42bc90), sets current room [0x524de0]; then inactive
  rooms enter stasis, active ones exit. Room list: [0x524380] array of [0x524378] room structs, [0] = room gob.
- Observed: host rooms near the client's puppet had flag 0x8 and the door there had flags 0000 (never updated).
  After also activating the puppet's rooms, l1_r4 shows 0002 and dr1_03 opens for the puppet.
- Calling mark_rooms with the puppet gob crashed both games (it reads camera-only fields).

## Session 1l: levels and going down (2026-10-20)
Checked:
- Main loop around 0x4025ff..0x402638: switch play mode (0x42af80, writes the player handle 0x5494c8),
  0x42fa40, then the room manager (0x4302a0) unless 0x40e860() == 4, then update_scene(scene) 0x457520
  (unconditional). Running Lua with lua_dobuffer right after update_scene works every frame, title included.
- Every level load ends in the Lua function level(l, cp, sec) (mdk2.lua); wrapping it in coop.lua catches
  menu, script (mdkNewGame in level1.lua: 2,12 / 2,1 / 1,10 / 1,12), debug and save loads. The player gob does
  not exist yet when level() returns (CreateKurt runs, but mdkGetPlayerGob() is nil); it does a tick later.
  The title screen is level 0.
- Level 1 script drivers: OnDamage/OnDie 41 handlers (synced through damage), OnAnimLoop 25, OnUpdate 11,
  OnTimer 9, OnEnterRoom 4 (l1_con2a02 locks l1_con2adr01; l1_r7 hint timer; l1_r1 empty; l1_tun13swt only in
  the pc demo build).
- Two instances: host F7 -> LEVEL(1, 2) -> client followed (log "peer started level 1 checkpoint 2:
  following"), 24 enemies bound. F3 (200 damage, 100 hp) revived each side at the partner with 100 hp.
Unknown: OnUpdate proximity triggers (mdkGobDistancePoint(mdkGetPlayerGob(), ...)) only see the local player;
OnEnterRoom fires per game; deaths that do not go through the damage function (pits, scripted kills).

## QA (2026-10-20)
`tools/qa.sh` in mdk2-coop: 17 checks with two real instances driven by key/mouse input, asserting on both
bridge.logs. Results: 17/17 localhost; 17/17 with 120 ms latency / 40 ms jitter / 5% loss on both sides
(Doganboy attack and shot checks were 0/0 in the lossy run: the enemy did not act in the window).
QA level for the write-up: automated (partial scope: level 1 start area only, no full playthrough).

## First manual session (2026-10-20)
- Playing the host window, the client crashed. The game writes its own minidump
  (<game dir>\mdk2hd_crash_<date>.mdmp, no WER event). Exception 0xc0000005 at 0x40cf66 (has_lua_event,
  reading gob+0x84 with gob NULL), stack: coop tick -> Lua -> coop_apply -> 0x42d9fc -> 0x4169da -> 0x403b67.
  Cause: APPLY damage applied with attacker NULL; the Poopsy's damage path asks the attacker for Lua events.
- Spawners name all spawns "<spawner>_spawn"; level 1's r3dogangen spawns Poopsies (217) in room l1_r3.
- New Game: level0.lua mdkNewGame(1,9) (intro), then level1 scripts mdkNewGame(1,12) (skydive),
  Level.IntroMovieBDone -> mdkNewGame(1,1). Runs on both games.
- Pressing Esc on the title screen opens "quit?" with Yes selected; automation must not press Enter after it.

## Second manual session (2026-10-20)
- Observed: after the skydiving section the player is pushed all the way to the left wall.
  Measured with no input: host Kurt 80 -> 105.41 with a partner, stays at 80 alone. Cause: both players spawn
  on the same point; the puppet is solid and re-placed on the partner's position every frame, overlapping the
  player. omGobGMSetSolid(puppet, slot, 0) on every slot did not help (character collision is elsewhere).
  Fix: never place the puppet within 3 units (horizontal) of the local player. Both stay at 80.
- OBJ_DOGANBOYGRENADE (417) is created through create_object with pos/quat NULL and a non-main scene: placed
  and thrown afterwards, so it is not replicated yet.

## Third manual session (2026-10-20)
- Going up the big fan shaft (l1_r7 area), the player died in lava (r7_lava -> bob 500, damage type 0x40). The
  revive then applied hp-1 of the hit; lava damage still triggered the death sequence and the host reloaded with
  level(1, -1) (instant load from save\auto.sav). The client followed mdkNewGame(1, -1) and crashed:
  0xc0000005 at 0x45b942 under mdkGobCreateInstant (Lua wrapper 0x42a6a0). Fix: apply none of a lethal hit;
  translate cp -1 to mdkGetCurrentCheckPoint(); client ignores negative checkpoints.
- Mark 1: a texture that can only be seen from one side and can be walked through, at (263, -77, -169) in
  l1_r5 just after picking up l1_r5_cloak (OBJ 320). Cause unknown (cloak effect, one-sided original geometry,
  or the extra active rooms); F2 now saves a screenshot.
- Poopsy damage type seen: 0x422.

## Fourth manual session (2026-10-20)
- Jumping off the top of level 1 (dome area), the player fell forever and never died (host last at z -764).
  Checked: host alone (bridge loaded, no partner, co-op logic idle), Kurt dropped 400 units below the level at
  1-6 kept falling for 30+ s at 83 hp. Derived: no kill plane there in the game (not checked with the bridge
  removed). Fix: with a partner, z < lowest room bmin.z - 30 (level 1: -236.8) moves the player to the partner.
- mdk2.str: u32 count, entries of {u32 id, u32 byte offset, i32} from 0x18, UTF-16LE text from 0x20c4.
  605 "Use the Sniper Mortar" (l1_r7 hint every 75 s), 712-717 tutorial tooltips (default keys: right mouse
  jumps, space bar enters sniper mode, mouse wheel selects sniper ammo, e throws grenades).
- l1_r7 (top of the fan shaft): three domed platforms (platportal1-3, Level.InitPlatform), each with guards,
  a fan (blower, disabled at start) and two with Doganboy spawners; sniper mortar ammo is spawned on the
  player's platform (l1_r7.SpawnMeSomeMortars) until the domes are all dead.
- Revives seen this session: enemy hits (type 0x2) x3, lava (0x40) x1; no reload followed.

## Fifth manual session (2026-10-20)
- Using the checkpoint keys, then F8 to level 2, both games exited (no dumps that time).
- Reproduced with MDK2_START="2 1": a game died within ~16 s in 2 of 3 runs. Minidumps (host and client
  identical): 0xc0000005 at 0x449715 (gob_delete, gob NULL), caller 0x4219a0 = Max class anim-loop handler,
  dispatched from 0x42b532 (class slot +0x2c, called when an animation loops, after the Lua OnAnimLoop check,
  event 5). It deletes the gob a Max arm holds (arm state at game object+0x40, 36 bytes per arm, handle at +0x88).
  The puppet Max holds nothing; animations copied from the partner make it loop. Fix: mirrored gobs and the
  puppet skip slot +0x2c too. 4/4 runs survived after.
- The game binds quicksave to F6 and quickload to F7 (mdk2.lua COM_SAVEINSTANT / COM_LOADINSTANT; instant load
  shows up as level(l, -1) with mdkLoadLevelIsInstant() 1). The mod's checkpoint keys collided; quicksave and
  quickload are unbound in co-op now.
- mdkNewGame(level, cp[, x]) -> 0x42c2a0(level, cp, x): x (default 1) only gates a music/sound stop
  (0x470e20/0x470e30); the load itself is 0x42ea50(level, cp).

## Session 2a: background testing and state checks (2026-10-20)
Checked:
- Background test mode (`[test] background=1` in coop.ini): the bridge feeds scripted DirectInput presses
  (Lua `coop_press(code, ms)`, written by the harness to `<game dir>\coop\command.lua`) instead of the real
  keyboard and mouse, keeps the window unfocused behind others, mutes, caps at 60 fps.
  Other programs stayed usable on the same PC while full sweeps ran.
- `chScreenShot` called from a script gives a black frame and ignores its file name (writes screenNNNN.tga to
  `%LOCALAPPDATA%\MDK2HD`). Screenshots are taken from the back buffer in the SwapBuffers hook instead.
- `coop_state(tag)` dumps each game's world (players, puppet, synced enemies with hp, actors, items, doors);
  `tools/statecheck.py` diffs host against client. It found desyncs that the per-feature checks never saw.
- The client finishes a level load about 1 s after the host: dumps taken right after a load show transient
  host-only items. Only dumps a few seconds after loading are trusted.
- `tail | grep -q` under `set -o pipefail` gives false "no match" (SIGPIPE) once logs grow.

## Session 2b: checkpoint sweep and the bugs it found (2026-10-20 / 21)
`tools/sweep.sh` visits all 96 checkpoints of the debug menu's list with host and client, walks, fires and
lands a client hit at each, and checks: client followed, both alive, puppets, every announced enemy bound,
orphans, Lua errors, enemy hp equal, state diff. About 55 min, in the background.
Bugs found and fixed (each confirmed by a re-run of the failing checkpoints):
- Stale puppet after a level or checkpoint load: the host drew the partner at its old-level position for
  about a second; the room manager keeps the puppet's rooms running, so that spot of the new level woke and its
  OnCreate made items on the host only (Max's guns in 2-3, chute 1-5, cloak 4-2). Player packets now carry the
  sender's level and load count; states from before the partner's load are ignored.
- Item spawners (OBJ types 300-399 made by spawners 511/540) were switched off on the client with the enemy
  spawners; health and ammo respawns existed on the host only.
- The debug checkpoint list has 3-16 ("3-6.5") between 3-6 and 3-7; a prefix match sent F6 from 3-6 to 4-x.
- 9-7: a script deletes `ch1`/`ch2` and makes new gobs with the same names. The host announced the new spawn
  before the old despawn, and `gob_delete` (0x449710) only marks a gob: it stays in the gob table and its Lua
  global stays valid until the scene's cleanup, so the client's name lookup bound the new id to the dying
  gob. Fix: despawns first, and gobs we deleted count as gone for the rest of the frame.
- 8-5: a spawned soldier (`l8r5_sod01_spawn`, type 240) has two gobs without object info (GOB_INFO NULL)
  carrying its name. Name uniqueness counted them, the host fell back to a numeric network name, and the client
  created a bare type-240 copy, which has 0 hp. Info-less gobs no longer count.
- 12-6: the client woke rooms at the host's body position, the host by its camera. One step to the left at the
  start put the host's body inside the next room's bounding box but not its camera, so that room's OnCreate ran
  on the client only (7 items). Each game now also wakes the rooms at its own player.
Derived: "went down" at 1-7, 3-9, 4-3, 6-2 and elsewhere were real deaths (lava `r7_lava` 500 damage, enemy fire)
caught by the co-op revive, not bugs.
Result: 96/96 checkpoints clean on commit 129b7e3 (one one-off item-at-origin dump at 10-4 did not repeat in two
re-runs). This sweep ran before the N-player change below and has not been re-run since.

## Session 2c: N players, co-op saves, reconnect (2026-10-21)
From the mod's commits 5e48dd6 and ba9cc77 (developed in parallel):
- Players 1..16: the host is player 1 and gives clients a slot from their first packet; clients talk only to
  the host, which relays states, shots, pickups and markers. Players travel as `@p<n>` in events, puppets are
  `coop_p<n>`; rooms, doors and revives use the nearest player. Level epochs replace the load-count guess.
- Silent players leave after 10 s; a joining or restarted client gets the level, live enemies with hp and
  taken items.
- Co-op saves: the host calls them (10 s into a level, after each checkpoint, every 2 min) and every game makes
  a full instant save of its own world under the same number. A crashed client comes back on its copy of the
  newest save; a crashed host resumes from Load Game > "Resume co-op".
- Tests: `qa.sh` 30/30 (2 players), `qa_n.sh` 38/38 (3) and 53/53 (8), `qa_save.sh` 31/31 (3) and 41/41 (8),
  as reported in those commits.
- Several test copies on one PC: each copy gets its own folder and ports; every copy shares
  `%LOCALAPPDATA%\MDK2HD` (settings.lua, and save\, where Auto0-3.sav are written by all of them).
Findings from that session's runs:
- Save paths are cut to 16 characters (`save\coop_t_c.sav` became ".sa"); co-op saves use `save\c<4 hex id><a|b>.sav`.
- `mdkStartInstantSave(file, desc)` / `mdkStartInstantLoad(file)` are full-state saves and work mid co-op with
  puppets and mirrored gobs present. Catches: the save brings back old puppets (duplicate `coop_p*`), so purge them
  after any instant load; `mdkGetCurrentCheckPoint()` returns -1 after an instant load, so track the checkpoint yourself.
- Level loads finish a few frames after `mdkNewGame` returns. SPAWN events read in between were dropped and the
  client then deleted those enemies as orphans; clients hold events until the load completes.
- A client that follows a LEVEL event during its intro movies crashes (the intro's own title load lands on top of
  the level); LEVEL is held until the game has reached the title.
- Windows UDP: without `SIO_UDP_CONNRESET` off, an ICMP port-unreachable from a dead peer makes the next recv fail
  with WSAECONNRESET; with several clients that cut every frame's reading short.
- The mirrored / foreign gob pointer sets must be cleared on every level start: a freed puppet's address reused by
  a new enemy made that enemy skip its AI.
- About 370 MB of memory per game. 8 players were no less smooth than 3. Host upload is roughly 20+ Mbit/s with 8 players
  in a busy level, mostly full enemy animation snapshots to each client (fine on LAN, needs trimming for internet).
- Lua: `bob.orientation` is userdata whose fields cannot be read; `mdkGobSetOrientation(gob, yaw, 0, 0)` works,
  yaw 0 faces +y.
- Max's tutorial at the start of level 2 blocks saving (`Save.intutorial = 1`; the game's own quicksave is blocked
  too). The host therefore also records its progress at each level start and checkpoint, and Resume picks that
  checkpoint when it is newer than the last co-op save (mod commit 9ba18c7; `qa_save.sh` 38/38 with 3 players).

## Parallel work on one mod (2026-10-21)
- Two streams of work edited the same working tree at once; a half-done header change in one broke the
  other's build. Fixed by giving each its own git worktree and branch, its own game copies and ports, and
  making the test kill step close only processes whose exe path is in its own copies (it used
  `taskkill /IM`, which would have killed the other stream's games).

## Session 2d: player installer and release (2026-10-21)
Checked:
- Steam installs the game in `steamapps\common\MDK 2 HD` (appmanifest_213350.acf `installdir`, `buildid` 3902);
  libraries are listed in `<Steam>\steamapps\libraryfolders.vdf` (`"path"` entries). The HKCU `SteamPath` value is
  lower-cased; HKLM `WOW6432Node\Valve\Steam\InstallPath` keeps the case.
- The game rewrites `%LOCALAPPDATA%\MDK2HD\settings.lua` from scratch when options are saved
  (`mdkSaveControlSettings`, called by the controls menu): the mod's loader line was gone and the next start had
  no co-op. It also wrote the mod's key rebinds into the player's settings. Fix: the bridge (loaded with the exe,
  before the game reads the file) re-adds the line at startup; verified by saving options, restarting, and
  seeing coop_init run.
- The game boots normally to the title with a settings.lua that holds only the loader line (defaults for the
  rest), so the bridge creates one when there is none.
- Installer: PowerShell 5.1 script + .cmd wrappers, no dev tools. Finds the game, checks the exe SHA-256,
  writes only the `[net]` keys of coop.ini through WritePrivateProfileString (keeps other keys), backs up
  settings.lua once. Tested on test copies: join, switch to host, a wrong exe refused with nothing changed,
  uninstall, and an installer-made host and client connecting over UDP.
- Released as a GitHub pre-release zip (v0.1.0-alpha). Known gap: the shipped Lua still has the developer F-key
  shortcuts.
- Sweep with 3 games (`MDK2_N=3`): 1-1 .. 1-3 and 8-4 .. 8-5 clean.

## Session 2e: Options > Co-op menu (2026-10-21)
Checked:
- The game's menus are Lua (`menu.lua`, `options.lua`): `mdkCreateMenu`, `mdkCreateMenuItem` (raw text) /
  `mdkCreateMenuItemW` (string id), `mdkMenuItemAddCheckBox` (Select(item, n)), `mdkMenuItemAddComboBox` +
  `mdkMenuItemAddComboString` (read with `mdkMenuItemGetWidgitValue` in Inc/Dec), `mdkMenuItemEnable`. Text entry
  is the save menu's way: `mdkMessageBoxCreate` + `mdkEditString`, then poll `mdkGetEditResult()` (1 accept,
  2 cancel) and `mdkGetEditString()` from the menu's `Update`, with `conscheme = "poo"` while editing.
- Redefining `BuildOptionsMenu` (from coop.lua) adds an item to Options on the title screen and in the pause
  menu. Disabled items are drawn too faint to read on a busy background; a combo box in a JUSTIFY_CENTER menu is
  drawn over its own label; long labels in a JUSTIFY_RIGHT menu run off the left edge.
- Menu keys from a script: the pause menu fades in for about a second, and key presses before that are lost.
  Down arrow (DirectInput 208) and Enter (28) navigate.
- Friendly fire: a player character hitting the player does nothing, whoever applies it: the engine ignores a
  player-class attacker on the player, and the player's damage filter (game object +0x10) drops the player gun's
  damage type 0x1. Passed with no attacker and as type 0x2 (an enemy gun), it lands (host 100 -> 93,
  client 100 -> 94).
- `mdkGobGetHealth` returned -2147483648 for the player from a command; game object +0x12 is the real hp.
- A client that accepts datagrams from any address keeps playing with the old host after being pointed at a new
  one (the old host still sends to it). Clients now take packets only from the address they are set to.
Derived:
- Settings the host decides (friendly fire, enemy toughness) are read only on the host: clients report every
  hit on another player, the host drops or applies it.
Evidence: mod commits 1c7c6af, 3c15d21; `tools/qa_menu.sh` 26/26 with 3 players (menu reached with key presses,
names relayed, ping, friendly fire both ways, enemy hp 100 -> 200 at +50% x 2 extra players, live host switch,
controls, developer keys, settings kept over a restart); qa.sh 30/30, qa_n.sh 38/38, qa_save.sh 38/38.

## Session 2f: players on screen (2026-10-21)
Checked:
- The game makes its GL context with plain `wglCreateContext` (exe imports no `wglCreateContextAttribsARB`), so
  it is a compatibility context: on an NVIDIA GPU (GL 4.6), the fixed-function calls work
  next to the game's shaders and framebuffer objects. Drawing in the SwapBuffers hook: save the current program,
  framebuffer and active texture unit, `glPushAttrib(GL_ALL_ATTRIB_BITS)`, push the three matrix stacks, bind
  framebuffer 0, draw, put everything back. No visible change to the game's own frame or menus afterwards.
- Text: `wglUseFontBitmapsA` (looked up in opengl32.dll at run time) from a Windows font. After the game switched
  to 1920x1080 it failed on every frame; called twice in a row it works (no failure logged since).
- The game's camera is a gob (`mdkGetCameraGob()`, named "camera"): position + quaternion, looking along its local
  +y with +z up and +x right. At the start of 1-2: (80, 190, -28.5), quaternion (0, 0, 0, 1). Its vertical field of
  view is about 64 degrees: Kurts placed 3.4 units to the side at depths 8.5 and 12.5 appeared 180 and 120 px off
  centre in a 561 px high view.
- Kurt is about 1.45 units tall (feet at the gob's position, head 1.45 above, from the same shot).
Evidence: mod commit d68742b; `tools/qa_hud.sh` 11/11 (panel rows from the bridge's own log of what it drew, the
panel changing the frame's pixels, hidden when switched off and under a menu, Max in level 2); qa.sh 30/30,
qa_n.sh 38/38, qa_menu.sh 26/26, qa_save.sh 38/38; an 8-player screenshot with all seven tags and rows.

## Session 2e: network traffic and a faster sweep (2026-10-21)
Measured (host upload, 8 games at 2-1 with 71 enemies, everyone moving; netstats.c counts UDP/IP headers):
- Before: 27.0 Mbit/s: enemy snapshots 21.7, relayed player states 4.6. Each snapshot was a fixed ~390-byte
  struct, nearly all of it 8 animation slots; 1 of 24 enemies at 1-2 had any animation running (the rest are in
  rooms in stasis), and every enemy went to every client 15 times a second.
- An animation state block is 10 DWORDs: [0] flags 1, [1] mode 1, [3]/[4] normalised play position (equal),
  [5] a constant per animation (0.405, 0.3125), [6] weight 1.0, [7] 4.0, [9] 1. Only [3]/[4] change while it plays.
- After: compact encoding (non-zero values only, rotation as int16, play positions as u16), enemies sent only
  when they change (plus two more, then 1 s keepalives), relayed states bundled per client and tick:
  1.3 Mbit/s (enemies 0.1, relays 0.9, own state 0.3); a client uploads ~38 kbit/s.
- All suites pass after it, also with 80 ms latency / 30 ms jitter / 3% loss; full sweep 96/96.
Derived: for an engine that runs most of the world in stasis, "send on change" is worth far more than better
encoding; the encoding mattered for the players' own states, which change every frame.

The checkpoint sweep went from 35 min to 4 min (236 s) with the same rows: it spent ~18 of 20.5 s per checkpoint
in fixed sleeps and in games acting one after another. Now it waits on conditions (every client bound every
announced enemy; hits and revives stopped), all games act at once, screenshots only on failures, and four sets
of games run side by side on separate copies and ports. A difference that is gone 1.5 s later is reported as
transient (a revive teleport caught mid-way), not as a failure.

## Session 2g: AI-upscaled textures (2026-10-21)
Checked:
- `mdk2.tar` holds 1808 DDS: 770 uncompressed 32-bit diffuse (BGRA), 511 `ATI2`/BC5 normal maps (`_n`), 511
  `ATI1`/BC4 specular maps (`_s`), a few DXT3/DXT5. Only 17 have a new HD diffuse (`_d`, the main characters,
  2048); the level textures with `_n`/`_s` keep their original diffuse, mostly 64-512 pixels.
- In HD mode the game draws Kurt with `kurt_d.dds` (tinted red in a patched archive: red Kurt), not `kurt.dds`.
- Loose textures are ignored: a tinted `kurt_d.dds` in `<game>\`, `<game>\override\`, `<game>\data\`,
  `<game>\textures\`, `%LOCALAPPDATA%\MDK2HD\override\` and `\data\` changed nothing. The exe's search-path strings
  (`data\`, `override\`, `override\<language>\`, `scripts\`, `save\`) sit next to `\MDK2HD\`.
- The game loads DXT5 (BC3) in place of uncompressed diffuse textures, with a rebuilt archive (members in the same
  order, new sizes); no other change needed.
- Memory: one game, first checkpoint of each level, before / after upscaling 441 textures to at most 1024 BC3:
  peak virtual 1166 / 1183 MB of the 2 GB a non-large-address-aware 32-bit exe gets; level 1 start 770 / 984 MB.
- Real-ESRGAN `realesrgan-x4plus` (ncnn/Vulkan): about 0.5 s per 512 texture on a desktop GPU. Up close it adds
  plausible detail (a crisp crack and grain where the original is blotches); from the default camera distance
  mipmapping hides most of it, and fine noise turns into smoother, painted-looking surfaces. texconv converts
  PNG input as sRGB unless `--ignore-srgb` (otherwise every texture shifts in brightness).
Derived:
- BC3 at 1024 with mips is about the size of the uncompressed 512 originals, so 4x costs little memory.
Evidence: mod commit bec16c6; sweep 96/96 with the upscaled archive in every game; `-Restore` byte-identical.

## Session 2f: going down, reviving, spectating (2026-10-21)
Checked:
- The player's death is ANIM_DIE (17): play once (style 3), about 0.93 s, then the pose holds. State block
  {1, 1, 3, phase, phase, 1.0714, 1.0, 4.0, 0, 1}.
- `mdkDeathCancel()` does not just stop the death sequence: it instant-loads the save ("checkpoint -1").
  During the death sequence `mdkGobGetHealth` of the player returns NaN/INF; `mdkGobGetHealth` of a living
  player also reads INF, so read the hp from the game object instead.
- `omAnimPlay(player, ANIM_DIE, 0)` on a living player does nothing visible: the player's own control code
  puts its animation back every frame. `mdkDisablePlayerControl()` stops movement (W does nothing) and
  `mdkEnablePlayerControl()` gives it back.
- What works for a downed pose: mark the player like a mirrored gob (its class think and anim-loop slots
  skipped) and apply ANIM_DIE's state block every frame, after the engine's update and before the
  network snapshot is taken (else the other games get the idle animation).
- The default camera (OBJ_DEFAULTCAMERA 110) is class 0 with no think of its own, so it cannot be
  redirected by wrapping a class think like the proximity doors. Spectating instead: the dead player's
  hidden (`omGobGMSetVisible(gob, 0)`), untargetable, think-less body rides 3 units behind the watched
  teammate, and the normal camera follows it. Its think being skipped also keeps it from picking items up.
- Damage types (Lua constants): GOODGUY 1, BADGUY 2, LAVA 4, SNIPER 8, FALLING 16, AREA 32, LAVAFALL 64,
  LAVAFLOAT 128, BLACKHOLE 256, LAVAELECTRIC 512, KNOCKDOWN 1024, LAVADEATH 2048, PUNCH 4096.
- `mdkShowToolTipMessage` takes a string id from mdk2.str, not text; own text needs the overlay (hud.c).
- A load of the checkpoint a client is already at was treated as "already here" (to avoid double loads
  from the client's own scripts); a team-wipe reload needs a forced flag on the LEVEL event.
Design: down 30 s (a teammate holds R beside you 3 s; the clock stops meanwhile),
then dead and spectating until no enemy is left in the rooms around your body (host-checked) or a
checkpoint; lava, black holes and the void go straight to dead; everyone down reloads the checkpoint.
Tests: tools/qa_revive.sh 33/33 (3 games); all other suites and the full sweep pass.


## Recorded-route progression bot (2026-10-21)
Observations:
- Added in-bridge keyboard/mouse recording and timed playback, using the normal game input path.
  Routes contain observed transforms and checkpoint transitions as assertions; no replay teleport
  or checkpoint advancement. Complete versioned recordings are validated before playback.
- Added a background host/client runner, progression-boundary and endpoint world comparisons,
  strict participant/life checks, bounded waits and failure artifacts. No campaign routes bundled.
- Bridge builds with mingw-w64. Portable parser/player tests, ASan/UBSan checks, Windows tests of
  the actual DirectInput wrappers (axes/buttons, buffered reads, peek, key release and foreground
  recording), four participant scorer tests with failure mutations, and CLI invalid-version
  rejection all passed. Evidence: mod worktree `local/replay-validation.txt`.
- Live testing in separate disposable copies could not start the bridge: both the new bridge and
  an unchanged baseline build were bypassed, and the system DINPUT8.dll was loaded instead.
  Process ImageLoad PreferSystem32 was observed ON. A process-creation override and a .local
  marker did not change the outcome. Stopped only the test game copies; no persistent
  mitigation settings changed.
Derived:
- A successful parser/input test establishes the replay machinery, not campaign progression.
  Missing intermediate milestones must fail even when the final checkpoint equals one already
  visited earlier in the recording; a regression test covers that case.
Unknowns:
- Cause of the DLL launch behavior, and whether it is transient or environment-specific.
- Actual two-client recorded-route progression, timing tolerances under combat, and campaign
  route coverage remain unvalidated. Existing checkpoint sweep claims were not extended.


## Projectile launch capture (2026-10-21)
Observations:
- Verified the test executable as Steam build 3902, SHA-256 bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43.
- In the existing decrypted-image disassembly, 0x4305f7 creates the Doganboy grenade (417)
  attached to its shooter/model slot, with NULL position and quaternion. The animation callback
  detaches it at 0x434d6d and launches it at 0x434d78 through 0x403b00(shot, shooter, target, direction).
- Class 5's constructor (0x403830) sets up the model and physics without necessarily launching.
  Its state at info+0x40 has active at +4, lifetime at +0x10, shooter/target handles at +0x18/+0x1c.
  The launcher computes the aim spread/ballistic arc and writes velocity at gob->physics(+0x68)+4.
- Added launch capture/replay in the mod worktree: FIRE carries the finished transform, velocity,
  lifetime and translated owner; constructor launches are captured once after creation returns;
  delayed launches are captured by the launcher hook. Replayed shots remain foreign. Launches
  from mirrored actors/puppets are discarded to avoid duplicate shots from copied animation keys.
- Build and fake-engine capture/replay tests passed. The existing two-game qa.sh completed
  34 passed, 0 failed (evidence: mod local/qa/20261021-183817).
- The Windows child processes had image-load policy flags 4 (PreferSystem32Images),
  loading system DirectInput instead of the proxy. A local Win32_Process.Create launcher gave
  flags 0 and loaded the unmodified game's adjacent proxy. No system settings were changed.
Derived:
- A create-only projectile hook misses hand-attached shots and can replay a different trajectory
  when the actual launcher runs later or adds random aim spread. Capture the finished launch state.
Unknown:
- Deterministic type-417 flight/damage checks with three games are still running; results follow.

Projectile validation follow-up (2026-10-21):
Observations:
- The first deterministic three-game test failed correctly: it saw type-417 replays created, but
  their sampled positions were stationary. The sender moved. Root cause: descriptor flag 0x8
  selects a bouncing rigid body; 0x403e0c writes its velocity to gob->body(+0x80)+0xb4, whereas
  non-bouncing shots use gob->physics(+0x68)+4. Updated capture/replay and the fake-engine fixture
  to cover both layouts.
- After the correction, tools/qa_shots.sh passed with three real games: host and client each
  launched a type-417 grenade; all six source/peer combinations had exactly one OnSpawn callback
  and changed position between samples, and all four foreign copies suppressed injected damage.
  Evidence: mod local/qa-shots/20261021-184551/game1.log .. game3.log.
- The source's type-417 launch velocity was (0, -14.14, 14.14), matching its 20-unit speed;
  peer position samples followed the same arc. The pre-fix capture read a gravity vector instead.
Derived:
- A replay-created log is not a projectile flight test. Capture the replayed gob and sample its
  actual movement; a fake engine must model each physics backend rather than assume one layout.
Unknown:
- Full-campaign projectile coverage and internet play remain untested by this focused check.


## Projectile launch replication (2026-10-21)
Observations:
- Steam build 3902, executable SHA-256 bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43.
- The Doganboy grenade path at 0x4305a0 creates type 417 attached to the shooter and hand slot,
  without position or orientation arguments. Animation release at 0x434d6d reparents it into the
  main scene, then calls launch_shot at 0x403b00. Creation is not a launch.
- Class-5 constructor branch 0x42da26 initializes 406 (sniper round) and 417 without launching.
  Other class-5 branches call 0x403b00 inside create_object. Capturing both hooks would duplicate those.
- 0x403b00 computes the final orientation and velocity, records owner/target handles and activates
  the projectile. Velocity is at gob->physics (+0x68) +4, or gob->movement (+0x80) +0xb4 when the
  shot definition's +0x54 has flag 0x8. Shot state is gob->info (+0x84) -> +0x40; +4..+0x14 is
  five pointer-free values (active, age, flags, timers); +0x18/+0x1c are owner/target handles.
- The bridge now sends class-5 FIRE from the launch hook, captures that result, and restores it on
  replay. Automatic constructor launches are suppressed during replay: native launch calls the
  shooter's OnSpawn and uses local targeting/randomness. Other projectile classes retain creation replay.
- FIRE uses a union over its unused attacker-name bytes for launch state, retaining 128-byte events.
  Player magic is MDKC so an old bridge cannot silently create inert replays. Replays remain foreign;
  owner and target handles are translated, and mirrored/foreign shooters cannot announce another shot.
- tools/test_shots.sh passes with the real shots.c and an engine double. Removing the launched flag
  makes its assertions fail. Covers delayed and immediate launch, no duplicate FIRE, missing owner,
  target/player translation, alternate velocity storage, foreign/mirrored suppression, failed creation,
  cleanup and the background-only native sniper QA entry point.
- Three real games, 80 ms latency / 30 ms jitter / 3% loss: tools/qa_shots.sh 26 passed, 0 failed.
  Both clients showed active, foreign, moving Doganboy grenades; every launch position/velocity matched,
  all grenades expired, and a client's native sniper launch was relayed exactly once to each peer.
  Evidence: mod local/qa-shots/20261021-184701 (logs and two flight samples per client).
Derived:
- Replaying creation arguments alone cannot replicate a projectile whose constructor leaves it inactive
  or whose later launch overwrites its transform. Capture the final launch result instead.
Unknowns / validation limits:
- This machine's fresh test processes selected the system DirectInput DLL; an isolated test launcher
  loaded the bridge explicitly. Scripted input did not reach those processes. The projectile suite uses
  native engine actions, and the checkpoint sweep uses native checkpoint callbacks and actions instead
  of input. No inference about input controls or the normal release startup follows from these runs.
- A complete campaign playthrough and every uncommon projectile variant remain unverified.

Final checkpoint validation for the projectile change:
- 96/96 native-action load/state checks, 168 s, two games in each of four isolated sets;
  all followed and survived, zero Lua errors, enemy hp mismatches or persistent world-state
  failures. Client-only spawns were purged normally; state-check warnings are retained in the report.
  Evidence: mod local/sweeps/20261021-184731-x4/results.md. Uses the explicit loader and native-action
  substitution described above; the original input-driven suites remain unverified.


## Steam transport merge into master (2026-10-21)
Observations:
- Committed the Steam transport, lobby menus, installer updates and offline fixtures as 032274a.
- Merged into master as cfe06c2, preserving recorded-input replay commit 27cfc6f.
- Resolved the bridge Makefile conflict by retaining steam.c, route.c and bot.c,
  with dependencies on both steam_abi.h and route.h.
- The combined Win32 DLL built successfully. tools/test_network.sh passed 234 checks;
  tools/test_replay.sh passed route playback/progression checks, Windows input checks
  and four Python tests. Shell syntax and git diff checks passed.
Unknowns / validation limits:
- This merge ran no game clients or live Steam account tests; two-account
  invites, relay connectivity and gameplay remain unvalidated. No commits were pushed.


## Consolidated projectile change (2026-10-21)
Observations:
- Consolidated two overlapping implementations into one. The kept one remains the base, including the optional MDK2_PROCESS_SERVICE=1
  launcher workaround; the other's protocol guard, replay safeguards and additional tests are included.
- Canonical FIRE layout: existing 128-byte event, part = 1, extra[0] = translated shooter;
  the unused attacker-name union carries velocity[3], then five pointer-free state DWORDs.
  Player magic is MDKC, preventing old creation-only bridges from accepting the new payload.
- Replay suppresses constructor launches and restores recorded state directly, without invoking
  shooter OnSpawn scripts or needing a live shooter. It translates owner/target handles and restores
  the native owner-collision exclusion when an owner is available. Mirrored/foreign shooters cannot
  announce authoritative children. Reused allocation addresses have stale shot flags pruned first.
- Consolidated fake-engine tests passed, including both velocity layouts, missing shooters,
  suppressed constructor scripts, owner/target translation, same-frame handle-generation reuse,
  background-only sniper automation and ownership cleanup. The payload size and offsets are asserted.
- Final build passed with mingw-w64. The merged normal-input tools/qa.sh passed 34/34 checks,
  including world diffs and level transitions (mod local/qa/20261021-190351).
- Three games with simulated 120 ms latency, 40 ms jitter and 5% loss: tools/qa_shots.sh passed
  all six source/peer flight cases and four replay-damage checks, with no shooter callbacks on peers
  (mod local/qa-shots/20261021-190105). The imported tools/qa_shots_native.sh passed 26/26 checks:
  real held Doganboy grenade release, matching launch transforms, active foreign flight, expiry,
  exactly one client sniper replay on every peer, surviving processes and no Lua errors
  (mod local/qa-shots/20261021-190148). Launchers used the unmodified Steam build 3902 executables.
Derived:
- Final launch restoration should bypass shooter scripts and retain the shooter's collision exclusion;
  invoking the launcher again merely to activate a replay introduces extra effects and requires a live owner.
Unknown:
- The complete campaign and every uncommon projectile variant were not played through by these checks.

## 2026-10-21: merged parallel branches, shots QA on the merged tree

Observed:
- Two branches had independently implemented launched-projectile replication (same FIRE event
  extension, same packet magic bump). They cannot both be merged; the one with a unit-test double,
  copied-attack suppression and a native QA script was kept, the other was archived as a patch and dropped.
- On the merged master: unit test passes; `qa_shots.sh` 6/6 and `qa_shots_native.sh` 26/26 with 3 games.
- `qa_shots*.sh` with `MDK2_N=3` exits silently with no output directory when `mdk2-test3` does not exist
  (install output is sent to /dev/null). Fix: set `MDK2_DIR_BASE` to a prefix with numbered copies.
Derived:
- Parallel work streams on the same task will duplicate work; check `git worktree list` for overlapping
  topics before starting.

## 2026-10-21: AI texture upscaler removed

Observed:
- Side by side at 1440p, same spot (one game with the upscaled archive, one with the original): the
  upscaled one was much darker and flatter; the warm wall lighting and the lit fridge were gone. Diffuse
  detail (wood grain, ceiling speckle) was a little sharper.
- Fullscreen on a chosen display works only through `chVideoChangeMode(<index into the resolution list>)`
  (list entries are per display driver); `chSetVideoMode` gives a framed window. `chScreenShot` from a
  command gave black frames in fullscreen; grabbing the display from Windows worked.
Guesses (untested):
- Alpha of the originals carrying lighting/glow lost in Real-ESRGAN, or a colour-space shift in the BC3 step.
Decision: the result was judged worse than the original; the tool, its two launchers and the docs were removed from the mod
(session 2g above describes the removed feature).

## 2026-10-21: couch (split-screen) co-op by multi-instance

Observed:
- Two copies started with `[couch]` (borderless 1280x720 windows stacked on one screen,
  `chSetVideoMode('', 0, 0, w, h, 165, 0)` from Lua at start) both rendered at that size and joined over
  localhost; the HUD and name tags worked in each window.
- Faked XInput state (left stick forward 3 s) moved the player ~30 units in the host's world; the right
  stick turned the camera.
Derived:
- The engine has one player gob, one camera and one render pass, so in-process split-screen was not tried;
  N processes tiled on one screen reuse all the co-op sync. Cost is N times the CPU/GPU/RAM.
- Gamepad -> the synthetic keyboard/mouse the background-test mode already feeds DirectInput works without
  window focus.
Unknown: real pads, 3-4 windows, whether a game's saved options (shared settings.lua) leak couch settings
(volume 0, 1280x720) into normal play.

## 2026-10-21: pre-launch launcher inside the proxy DLL

Observed:
- Startup order in bridge.log: DllMain (bridge attached) -> the exe's first `CreateWindowExA` -> settings.lua
  -> `coop_init`. So a modal UI run inside the first `CreateWindowExA` (IAT hook, game thread, loader lock
  long released) comes before the game reads settings.lua or the mod reads coop.ini.
- `mdk2hd.exe` statically imports `DINPUT8.dll` and `CreateWindowExA` / `RegisterClassA` from user32.
- `Titlescreen_PC.dds` (1024x1024) and `font.dds` (512x512) in mdk2.tar are uncompressed 32-bit DDS (pixel
  format flags 0x41, masks R 0xff0000 G 0xff00 B 0xff A 0xff000000) with rows stored bottom up.
  `font.lua` holds `dim`, 16 rows of 16 advance widths in cell units, indexed `dim[col][row]` for
  character `row * 16 + col`; 32x32 cells, glyphs left-aligned, plain ASCII layout.
- settings.lua's video line is `chSetVideoMode('<s>', a, b, width, height, hz, fullscreen)`; the game
  default (defaultconfig.lua) is `( "", 2147483648, -1, 640, 480, -1, 1 )`. The launcher rewrites width,
  height and the last argument there before the game reads the file; the game then started and ran
  (window size not measured).
- `tools/qa_launcher.sh` (mdk2-coop, launcher hidden and driven by a command file): 23/23, covering a UDP
  join at a typed address (host logged "player 2 joined from 127.0.0.1"), solo leaving coop_init inert
  with the game still running, Esc exiting before the game window, `[launcher] show=0`, and
  `+connect_lobby` skipping it.
Derived:
- A pre-launch screen needs no separate exe or Steam launch option: the proxy DLL already loads first.
- Settings that only apply at startup (transport, role, ports) can be changed there and take effect at once.
Unknown:
- The visible (non-test) window path, real mouse handling and DPI scaling were not exercised by the
  background QA (it never shows windows); left for a hand test.
- Arguments a and b of chSetVideoMode; whether the rewritten resolution / fullscreen flag is honoured
  at that start (expected, since the file is read after the hook, but not measured).

## 2026-10-21: players through walls (HUD outline)

Observations:
- Name tags were already drawn in 2D with the depth test off, so they already show through walls.
- Added a per-slot coloured capsule outline (flat when downed) drawn the same way; `[hud] outline`, default on.
- Screenshot from a 2-instance `tools/qa_hud.sh` run shows the outline around a partner at the screen edge.

Derived facts:
- `MAX_PLAYERS` is 16; a colour table sized 8 and indexed by slot read past its end and drew black for slot 8.
- The outline is a fixed-size capsule from the projected feet and head points, not the mesh silhouette.

Unknown:
- A true mesh silhouette (re-drawing the puppet with depth test off) was not tried; needs a hook into the game's renderer.
- Outline sizes for Max and Doc (taller/shorter than Kurt) were not measured.

## 2026-10-21: controls presets and a drawn keyboard view

Observed:
- The game's menus live in the middle 4:3 of the window (menu x 0..1 = that part, labels end at x, value
  widgets run on to the right of it); on a 16:9 window a strip each side is free, so a bridge-drawn overlay
  can sit beside the game's menu. The overlay is drawn after the menu, so it must leave the menu's column
  alone; a full-screen dim box hid the menu.
- Rebinding reuses the game's own capture: `conscheme = "poo"`, `mdkShowMouse(0,0)`, then `omGetKeyPress()` each frame
  (-1 nothing, 1 Esc cancels). A key joins an action's two slots and the older drops out.
- Mouse codes (game's own names): 1000 / 1001 buttons, 1002 "Mouse B3", 1003 "Mouse B4", 1008 / 1009 wheel.
- Several processes editing the shared settings.lua at once: `sed -i` fails ("cannot rename ... Permission
  denied") while any game has it open. A scratch file via `MDK2_SETTINGS` works when the loader line is already there.
- `tools/qa_controls.sh` 30/30, `tools/qa_menu.sh` 26/26 with the change.
Derived:
- Original and Modern are tables over a list of actions with the game commands each one's keys are bound to;
  a "set" list says which actions are in use together, for taking a key away from the one that had it.

## 2026-10-21: difficulty presets and rules sync

Observed:
- A scripted hit on the host's own player (`mdkDealDamage(enemy, player, ...)`) is not a clean HP check in a
  test: a spawned Doganboy is alive and shoots the host while the script runs, so HP deltas are noise. The
  hook now logs `difficulty: <from> -> <to> damage a -> b` whenever it rescales a hit, and the test greps for a hit
  size the enemy never deals.
- The game's Lua is 3.1: no `for` loops (use `while`), no `getn`. Rebuilding a menu page from inside one of its own
  item handlers is avoided; the rows are updated in place instead.
- `tools/qa_menu.sh` 38/38 and `tools/qa_revive.sh` 33/33 with the change.
Derived:
- All difficulty goes through the one host-side damage hook (world hits on players, players' hits on the world),
  the enemy spawn hook (hp) and two constants in revive.c; the host sends its values to clients as a reliable
  event on join and on every `[game]` change, so the down and revive timers agree on every side.
- A preset is just a set of `[game]` values; "Custom" is whatever matches none, so there is no preset key to keep in sync.
Unknown:
- Damage from projectiles a client launched (grenades) may count as the world's rather than a player's for
  `dmg_dealt`; not tested.

## 2026-10-21: controls page merged; keys of other features folded in

Observed:
- The chat / ping (T, V, middle mouse) and the revive key (R) were hardcoded in the bridge and
  Modern's Doc inventory used R: a clash found only by reading the merge. They are now set from Lua
  (`coop_game_keys`), shown on the Controls page's Co-op tab, rebindable, and in the conflict sets.
- `tools/qa_chat.sh` failed on master before the merge: its host-message regex ended in `$` and the
  Windows log line ends in a carriage return (fixed with `[[:space:]]*$`).
- Suites on the merged tree: qa_controls 36/36, qa_menu 26/26, qa_chat 8/8, qa_revive 33/33.
Derived:
- Any feature that reads keys itself should take them from the controls table, or two features will
  quietly share a key; the conflict check only knows the actions in `coop.actions`.

## 2026-10-22: merging voice, emotes, outlines, difficulty, couch and the launcher (master 43a34c4)

Observed (from the mod's commit message; the combined result was not re-run by suite here):
- Voice push to talk was a hardcoded key that clashed with chat and ping; it is now an action on the Controls
  page's Co-op tab (B in both presets) and is conflict-checked like the others. The keys the bridge reads
  itself (chat, ping, revive, push to talk) are ignored while a chat line is open, so typing "r" or "b" no
  longer revives or talks.
- Emotes ended on fixed movement keys; they now end on whatever is bound to move, jump, fire and sniper mode
  in the active preset, and not while typing.
- Couch games use the Modern preset (the keys the gamepad presses), skip the launcher, and have no voice chat
  (one room, one microphone).
- The launcher's difficulty and controls entries were writing keys the Gameplay page had replaced (a Hard or
  Custom 75% enemy health became 50% on every Play). It now offers the same presets and writes `[game]` only
  when something changed.
- HUD: the MIC badge moved to the bottom right (chat is bottom left); a chat bubble sits above a talking
  player's speaker icon.
Derived:
- Each feature built on its own branch worked alone; the clashes were all shared state (keys, `[game]` /
  `[controls]` keys, screen corners). Merging needed a pass over every table the features share.
Unknowns:
- The 96-checkpoint sweep and the earlier suites have not been re-run on this merged tree.

## 2026-10-22: six parallel feature branches merged (outlines, emotes, difficulty, voice, couch, launcher)

Observed:
- All six branches started before the Controls page and chat were on master. Each merged with only small
  textual conflicts (the Makefile source list, menu tables, event enum), but three breakages merged silently:
  - Difficulty defined `coop.SetPreset(i)` / `coop.presets` / `coop.PresetName`, and the Controls page already had
    `coop.SetPreset(name)` for key presets. Lua globals: whichever loaded last won, with no error. Renamed to
    `coop.SetDiffPreset` etc.
  - The launcher returned early from the `CreateWindowExA` hook when `!test_background`. Couch co-op had replaced
    that test with `synth` everywhere else, so couch windows would have kept their border and size.
  - The launcher's "Tougher enemies" row rewrote `enemy_scale` from its own four values on every Play (a Hard
    preset's 75% became 50%), and its "Modern controls" row wrote the replaced `[controls] modern` key.
- Key clashes: voice push-to-talk defaulted to V (Ping) and offered T (Chat). It is now a Controls-page action
  (B, `coop_game_keys` 3). Emotes ended on fixed WASD and arrow keys, and now on the bound move, jump, fire and
  sniper keys (`coop_emote_keys`). Co-op keys are ignored while a chat line is open.
- Couch games: play with the Modern preset (the keys the gamepad presses), skip the launcher, and have no voice.
- Test environment: games started with `Start-Process` from this WSL shell loaded the system `dinput8.dll`. No mod
  code ran, in any copy or at any commit, including copies that had worked an hour earlier. `mdk_cmd` reported
  "did not pick up" for every command, and `bridge.log` kept its old date. `MDK2_PROCESS_SERVICE=1`
  (`Win32_Process.Create`) loads the proxy. qa_launcher's own `+connect_lobby` start now honours it too.
- With it, on master 606132e: qa_controls 37/37, qa_voice 14/14, qa_menu 38/38, qa_chat 8/8, qa_launcher 23/23,
  qa_hud 11/11, qa_revive 33/33, and the full sweep at 96/96 checkpoints with no Lua errors, HP mismatches or state
  failures (`MDK2_SHARDS=4`, 236 s).
Derived:
- A clean merge says nothing about two features' shared inputs (keys), settings keys, screen space and global
  names. Each pair has to be read for those.
Unknown:
- Why the launch path started preferring system DLLs between 23:46 and 23:58 on 2026-10-21.
- Couch co-op with real gamepads, and voice with a real microphone, are still untested by hand.

## 2026-10-22: automated regression ladder and oracle checks

Observations:
- Added a sequential regression runner in the mod worktree: offline, smoke, features,
  campaign and full profiles; per-gate timeouts, lab leasing/cleanup, terminal result
  validation, JSON/Markdown summaries and optional recorded progression routes.
- Existing input and network tests had stale dependencies and failed to link. The
  input fixture also selected the old background flag instead of the new synthetic
  input mode. Updated fixture dependencies and mode selection; input tests pass.
- Host welcome now queues EV_RULES before gameplay events. Updated the network
  test's reliable-event sequence/ack assertions and verified welcome after nonce reset.
- Offline profile passes all five gates: DLL build, route/input/oracle checks,
  projectile core, pad mapping and fake-Steam/loopback network tests.
- The two-client 1-1 through 1-3 sweep produced matching worlds (zero state warnings
  at each checkpoint), matching enemy counts and HP, and no Lua errors. The first
  strict scorer incorrectly rejected three client orphan-deletion messages at 1-3.
  Source inspection confirmed these record successful removal of local spawner
  duplicates; removed actors are counted as evidence, not surviving world mismatches.
- The default process launcher did not load the proxy in this agent environment;
  MDK2_PROCESS_SERVICE=1 loaded it. Both attempts were stopped and leases released.

Derived facts:
- Sweep process success previously did not establish suite success: the script
  returned zero even for state failures and missing shard rows. The new scorer
  explicitly checks requested checkpoint rows and fails absent/incomplete evidence.
- NaN coordinates can bypass threshold comparisons. State parsing must validate
  finite positions before calculating distances; empty/missing-header dumps fail.

Unknown:
- Feature profile validation is in progress. Full 96-checkpoint campaign and actual
  recorded progression routes have not been rerun by this session yet.

Further observations:
- Feature ladder: three-player qa_n passed 38/38. Two-player qa.sh passed 30 and
  failed four death/revive timing checks; retained as failures, no relaxed asserts.
- Added a disposable-installer gate staging only mod code and reading an executable
  from the lease. The first fixture attempt under WSL UNC failed at the Win32 INI
  API; fixtures now use the lease's NTFS scratch folder, with their location recorded
  in the ignored evidence directory. No original install or owner saves are used.
- Python oracle suite now has 20 tests, including artifact-collection failure with
  guaranteed lease release. Mutants cover the production state/sweep/runner scorers.

Validation update:
- qa_save passed 38/38; qa_revive passed 33/33; installer fixtures passed 22/22
  after moving them to NTFS. The remaining menu/UI feature reruns were interrupted
  to prioritize the new level sampler; the feature profile's report is FAIL with
  an INTERRUPTED runner row, not a claimed full pass.
- Checkpoint smoke 1-1 through 1-3 passed end-to-end through the regression runner
  with all game logs/configurations captured before the next gate.
- Added --levels to the sweep: first checkpoint of each of the twelve levels,
  using F8 between fixtures, with the same gameplay actions and state comparisons.
  Its first run reached all twelve with matching state but failed while writing
  the summary because the shell source was edited during execution. Scoring the
  saved rows directly passed 12/12. A clean end-to-end rerun is in progress.
- Sweep now rejects invalid endpoints, invalid instance/shard counts and leases
  too small for the requested shards. Sharded ports follow actual leased slots,
  including noncontiguous slot sets, rather than overlapping fixed offsets.
- Final offline profile passed all five gates, including 235 network assertions;
  Python tests now total 21. The manifest test requires every qa*.sh game suite
  to be registered in the full profile.

Lesson:
- Do not rewrite a running shell harness. Bash may read the next top-level command
  from the modified file at its old byte offset. Finish or stop the run first, or
  execute an immutable copy. The observed failure here was runner syntax, not a
  game-state mismatch; keep that distinction in the evidence and rerun the gate.

Clean sampler result:
- The unmodified --levels rerun passed all twelve checkpoints end-to-end in
  143.4 seconds, with both processes surviving and no scored world failures.
  Evidence: local/regression/20261022-144137-501292 and
  local/sweeps/20261022-144139 in the mod worktree. The lease was released.
- Full 96-checkpoint and the remaining feature/menu suites were not rerun;
  no complete campaign traversal or recorded route is claimed.

Sharded validation:
- Two shards (two real clients per shard) passed checkpoint fixtures 1-1 and 1-2
  in 39 seconds using noncontiguous leased slots 0,1,4,5. This exercised child
  summaries, all-child exit checks, merged scoring and port assignment across a
  slot gap. Evidence: local/sweeps/20261022-144447-x2. All four slots released;
  the other game's two-client lease remained running.

Final gate validation:
- Installer also passed end-to-end through the new runner (22 checks, 7.4 seconds),
  including diagnostic collection, process cleanup and lease release. Evidence:
  local/regression/20261022-144550-480965 in the mod worktree.
- Documentation links, shell syntax checks and git diff --check passed. Changes
  remain in the worktree; this session did not publish or push them.

Continuation: revive reliability and broader validation

Observations:
- The unchanged two-player QA reran successfully, 34/34; the prior four failures
  are intermittent, not a consistently reproduced revive defect. Evidence:
  local/regression/20261022-145655-357320 in the mod worktree.
- Added a game-free fixture around the actual bridge/revive.c implementation:
  69 assertions covering damage/lava/void, bleed-out and pause clamping, a teammate
  holding revive, range checks, health restoration, spectating/cycling, checkpoint
  authority, zone-clear delay, one-shot wipe/reload and solo fallback.
- Ten deliberately broken source variants were rejected by those state assertions:
  unreachable damage, missing spectating, excessive revive range, missing wipe,
  bleeding while revived, full-health respawn, wrong checkpoint authority,
  repeated held-key revive, pause-induced bleed-out and intercepting solo death.
- Reworked two-player death phase to load a fresh checkpoint after combat; it now
  checks native local/received life state, survivor preconditions and replication
  before requesting respawns. This change is under real-game validation.

Derived facts:
- The old fixture's assumptions were not asserted: the survivor could still be
  fighting previously spawned enemies, and a fixed sleep could expire before the
  peer's dead/alive state was received. The new fixture exposes these as explicit
  conditions rather than treating an old log line as current life state.

Unknown:
- The exact cause of the earlier four failures is unknown: complete bridge logs
  from that initial run were overwritten before diagnostic collection was added.

Epoch regression discovered while hardening the death fixture:

Observations:
- A host reset to the same checkpoint generated a new epoch but the client logged
  "already here", retained old actor IDs and kept the already collected item gone.
  This is a world-reset defect as well as a test-fixture precondition problem.
- Added qa_epoch.sh: mutate both worlds through a host spawn and client pickup,
  reset the host at exactly the same checkpoint, require both games to really load
  and restore the fixture, then cut the client network for 12 seconds and require
  a same-epoch continuation without another reload.
- Against the old build it failed 5 of 20 checks: client reload, reset world match,
  fixture restoration, and the post-reconnect world/fixture. Evidence:
  local/regression/20261022-150740-647152 and local/qa-epoch/20261022-150745.
- A new network assertion failed against the old code because net_level_event
  adopted the pending epoch before the load completed. After the fix, all 243
  network assertions pass, including spawn deferral and 16-bit epoch wrap.
- Fixed net_level_event to defer epoch adoption to net_level_reset and removed
  FollowLevel's checkpoint-number-only reuse path. Same-epoch reuse remains the
  explicit same==2 case. Real-client validation of the fix is in progress.
- The revised two-player QA fixture hops through 1-1 before returning to 1-2,
  explicitly verifies real loads and reads local/received life state. It passed
  50/50 before the epoch fix; evidence local/regression/20261022-150457-829600.

Derived facts:
- Equal checkpoint numbers do not establish equal worlds. The epoch must match
  before a reconnect can safely retain actors/items and apply newly received events.

Epoch fix validation:
- The same immutable qa_epoch.sh passed 20/20 against the fixed build, including
  actual same-checkpoint reloads, restored pickup/spawn state, and no reload on a
  same-epoch reconnect. Evidence local/regression/20261022-150907-236075.
- Full feature validation is now running with the fixed native/Lua build. Added a
  lossy epoch gate and an optional --campaign-shards setting on the runner so the
  complete checkpoint fixtures can run in bounded parallel sets with proper leases.

Game-free and motion coverage expansion:
- Network regression now includes four compiled broken variants, all rejected by
  assertions: eager epoch adoption, applying spawns while loading, blocking a
  same-epoch reconnect and ignoring epoch zero. Baseline passes 243 checks.
- Added test-only per-frame Lua motion telemetry and qa_motion.sh for real walking,
  strafing and jump/jet behavior on Kurt, Max and Doc in both games. It never writes
  player transforms, requires a stable idle control, opposite motion directions,
  sufficient frames, live players, actual movement/rise and matching worlds.
  Real-game fixture validation is pending the ongoing feature lease.
- Motion scoring has mutation tests for missing/duplicate probes, wrong character,
  zero samples, bad life/menu/loading state, unstable idle, wrong direction,
  stationary controls, missing jump rise, NaN/Inf and malformed numbers. Total
  Python tests: 26, including resource/diagnostic checks for sharded runner gates.

## 2026-10-22 mdk-save: lab trial
- Slots 2,3,4 via `lab acquire --slots 3` (no wait), `eval "$(lab env mdk2)"`, ports 27932+.
- tools/qa_revive.sh: 33 passed, 0 failed. tools/qa_save.sh: 38 passed, 0 failed.
- Both scripts already lab-aware (MDK2_DIR*, MDK2_PORT, MDK2_PIDS from env); no changes.
- No collisions seen; the other agents' Witness games ran concurrently, mdk_kill is path-scoped.

## 2026-10-22 mdk-sweep: lab trial, tools/qa.sh
- Observed: `lab acquire --slots 2` from the worktree gave slots 0,1 (owner mdk-sweep); an acquire run from another cwd takes that folder's name as owner (released it). Port 27900, dirs C:\coop\lab\mdk2\s0/s1, pids under run\mdk-sweep.
- Observed: `tools/qa.sh` with lease env: 34 passed, 0 failed (about 4 min), while mdk-save held slots 2-4. No collision seen.
- Observed: check "client hurt only via host" passed vacuously (0 HURT, 0 host hits).
- Not run: qa_n.sh, sweep.sh. No code changed.

## 2026-10-23: cutscenes together (shared go, skip vote, end barrier, skydive ghosts, host-triggered movies)
Observations:
- Every game runs a level's movies itself: `StartMovie(gob, { tasks..., Done = fn })` (script.lua) puts a task list on a gob and
  `ScriptUpdate` steps it; a movie of the level load starts inside the level script's Init, i.e. inside the `level()` call
  (the log line of the first movie comes before "level N checkpoint M started"). A movie's Done usually loads the next level
  (`mdkNewGame`), or starts the next movie (1-9's lounge scene -> corridor movie -> 1-12 skydive -> 1-1).
- The abort key is COM_ABORTMOVIE (Backspace, `comfuncs.game`); `StopScript` runs the Done, or, with a minigame later in the
  list (levels 1, 2, 5, 9), jumps to the minigame instead; `Save.inminigame` blocks the key during it.
- With the dev keys on, the emote browser bound Backspace to its stop command; the engine gives a key to the command bound last and the
  game saved that into settings.lua, so abort-movie then read -1 (key dead) on every later start, test copies included
  (settings.lua is shared by all copies). Rebinding the command at start (omBindCommandI) fixed it.
- Lua 3.1 has no `true` (an undefined global, nil): a mutant "if (true) then" never ran.
- Events a client sends while its level is still loading carry the old epoch and are dropped by the host: send from the next
  frame's tick, after the load call returned.
- 2-player run of tools/qa_cutscene.sh: 34 of 34 (lounge scene and corridor movie skipped by vote, skydive skipped up to the
  minigame, minigame not skippable, the host waits in the end hold until the client has landed, both load 1-1, a stand-in
  host-triggered movie played by the client, ghost of the other player in the sky, screenshot shows both skydivers);
  mutants "no end barrier" and "one-player vote" are caught by the checks that guard them.
Derived:
- Movies need keeping together, not copying: the same scripts run everywhere, so the host only needs to say when to start, whether the
  vote passed and when everyone has reached the end. A paused script (`stack.paused`) is a clean start barrier; wrapping `sc.Done`
  is a clean end barrier (the Done runs from the host's tick when every game has reported).
- Triggered movies (boss entrance, fan cutscene) only play in the game whose player triggered them; the functions that start them
  (`Level.PlayBossMovie`, `l1_r6.StartCutScene`, ...) can be wrapped so the host announces them and the others call their own copy.
Unknown:
- Triggered movies in levels other than the stand-in on level 1 (wrapped by name from the scripts, not run): the clients' copies
  of the function may rely on state the host's game has (boss gobs, items); they are called protected.
- With three or more real players: counts follow the majority rule, only two were run.

## 2026-10-23 (later): ghosts in the other three minigames
Observations:
- Levels 2 (checkpoint 13), 5 (11) and 9 (13) each have one torpedo-style minigame: a ship (`torpgob`, `podgob`, `jdgob`) steered on one or two axes
  (z only; x and z; x and z) at a fixed depth, dodging meteors; a hit sets `.dead`, the movie script retries on a negative result.
- Ghosts of the other players (their own ship model, placed from the two numbers they send) show in levels 2 and 9 screenshots
  (qa_minigames 15/15 by log lines). In level 5's screenshot the host's own pod had just exploded, so no ghost is visible there.
Unknown: how the ghosts look while moving with several players; level 5 ghost by eye.

## 2026-10-23: Controls page in the game's colours, gamepad tab, pad in normal games
Observations:
- The Controls picture now uses black and amber with notched panels (cap, tab and mouse panels share one `panel()`
  helper in ctrlview.c); qa_controls 40/40 with a new Gamepad page and a check that the pad follows each preset.
Derived:
- The pad's action keys come from the preset through `coop_pad_keys`; the arrow keys stay pressed beside them so menus work.
- In a normal game the pad is merged into the real keyboard/mouse reads (state and buffered), only while the window is focused.
Unknown:
- The non-couch pad path with a real pad: only the mapping (pad_test) and the Controls page were run. Real pads, by hand, untested.

## 2026-10-23 character wheel (switch hero at any time)
Observations:
- Each level script calls exactly one of CreateKurt / CreateMax / CreateDoctor (levels 6 and 12: CreateDoctor + CreateHyde).
- The debug menu's "Switch Player Character" (debug.lua) deletes the camera, calls CreateX(), moves `bob` to the old
  position and deletes the old player gob. Doing the same from Lua in a running level worked for Kurt <-> Max <-> Doc in
  level 1 (checkpoint 2) although that level normally only has Kurt: player type read back as 100 / 101 / 102.
Derived: the puppet must follow the peer's player_type (it already travels in every packet); the bridge replaces it on a change.
Unknown: switching in Hyde levels (left out), health carried over (not done: the new hero starts fresh), levels other than 1-2.
Evidence: tools/qa_char.sh 9/9, tools/qa_char2.sh 7/7 (2 games), qa.sh 50/50, qa_emote 19/19, qa_controls 37/37.

## 2026-10-23: softer player outlines, HUD text in the game's menu font
Observations:
- Outlines are now a person-shaped curve (Catmull-Rom through ~35 points: head, shoulders, arms, legs), 1.5 px with a faint
  dark under-line, alpha 0.6 at most, faded to nothing between 11 and 3 game units from the camera; lying down when downed;
  wider for Max and Doc. Seen in a 3-game qa_hud screenshot: thin, readable, roughly the puppet's size.
- The game's `font.dds` has upper and lower case; the HUD (tags, panel, chat, headings) draws it as textured quads (cell 32 px,
  tinted), the launcher's loader shared. Small controls-page text stays on the Windows font (the sheet is too soft at ~10 px).
Unknown:
- qa_hud's panel-row checks (3 of 11) fail on slot numbers in the row text (rows read 5 and 6, the script expects 2 and the
  client slot); not looked into, not caused by drawing.
- Not a mesh silhouette: the shape is generic, the puppet's pose is not followed.

## 2026-10-23: save browser (sessions) and level picker
Observations:
- The game cuts the whole save path to 15 characters (`save\` + 6 + `.sav`), so a co-op save file name is `c` + 2 base-36 digits of the
  copy's player id + 2 of the session + a/b. My first try (`c` + 4 hex + 4 hex + a/b) was never written: "co-op save N: ... was not written".
- Lua 3.1 has no upvalues: a menu item's `Select` function that used a local of the building function was a load error
  (`running coop.lua ...: 1`, nothing else in bridge.log). Keep what a closure needs on the item or look it up again.
- Sessions: each new game from the title screen (or Start at level) makes a session id (1..1295); the save event carries it, so each client
  files its own save under the same session. saves.ini keeps who played (names), the most players at once, last played and progress, per
  session, in every game's copy. The launcher's Saves tab lists them (cards: level-checkpoint, CO-OP n PLAYERS badge, names, host, age, progress bar),
  picks "Continue" for a session this copy hosted, and has a level/checkpoint picker for a new game (the debug menu's 96-checkpoint list).
  The choice goes to coop.ini `[start] action=new|resume` and coop_dev.lua uses it once from the title screen.
Derived: a client can only continue a session through its host (the host resumes, clients follow), so friends' sessions are shown, not selectable.
Evidence: tools/qa_launcher.sh 29/29 (saves page with seeded sessions, Continue, new game at 2-3 started by itself); tools/qa_save.sh with the
session checks (same session in every game, all players recorded, resume keeps one session, Start at level makes a second) - see the final run in the
session summary. Level names: unknown (the page shows numbers only).
Unknown: whether a session's old files survive the game's own save-slot clean-up (nothing in the game deletes them that I saw); the in-game
"Start at level..." menu was only built and driven from Lua (`coop.BuildStart()`, `coop.StartAt`), not clicked by hand.

## 2026-10-23: level captions and numbering
Observations:
- `mdk2.str` (in mdk2.tar) holds the loading screen captions as UTF-16 text, "<place>, <town>: 9:05AM". Level scripts show them with
  `HideNextLoadingScreen(strings.l<N>chk1)` (ids 207-209 for levels 1-3, 509-515 for 4-10); there is none for 11 and 12, and a second
  one for level 1 (`l1chk2`, the intro movie). The first ten time-stamped strings in file order are levels 1-10 in order.
- Characters per level come from the one `CreateKurt/CreateMax/CreateDoctor` call in each level script (6 and 12 also CreateHyde):
  Kurt 1,4,7,10; Max 2,5,8,11; Doc 3,6,9,12.
- The launcher reads the captions at run time (nothing of the text is kept in the repo) and numbers checkpoints in play order.
Unknown: the .str index format (not decoded; strings found by scanning); names for levels 11 and 12.
Evidence: tools/qa_launcher.sh 30/30, tools/qa_save.sh 52/52, saves page screenshot in the qa output folder.

## 2026-10-23: skydive ghosts face the right way, homing missile ghost, cutscene countdown
Observations (from a manual two-window playtest, then scripts):
- The skydive ghost was upside down in both games. The game registers `ktgame_kurt` with a tilted orientation
  (quaternion 0.5228, 0.1249, -0.0978, -0.8376); the ghost was created with the identity orientation. It now copies
  the player's own Kurt's orientation, read from the gob (new bridge call `coop_pose(gob)` -> position + quaternion,
  OMGOB +0x18 / +0x24).
- Each game runs its own skydive: lasers (`ktgame_laser0`, aimed at random points) and every fifth shot a homing
  bullet (`OBJ_TURSHOT07`, `mdkShootBullet` at your Kurt, kept in `kurtgame.lastmis`). The engine moves that bullet, so
  its Lua fields hold no position; `coop_pose` reads it. It is sent 8 times a second (`MV_FALLMIS`) and the others draw
  a harmless `ktgame_missl0` ghost, extrapolated along its last heading (at most 0.5 s ahead).
- This game's Lua is 3.1: `getn` does not exist ("call expression not a function" at movie start, the host retried
  autostart in a loop and the client stayed on the title screen).
- Cutscene banner: the skip vote has no time limit any more (it lasts until the movie ends, Backspace takes it back);
  the banner counts down the time the movie has left. A movie's length is not in its script (it follows animation
  lengths and dialog timers), so it is learned: the play time (waits and the minigame left out) when a movie ends
  unskipped, kept in `coop.ini [movies]` under level_gobname_stepcount.
Derived: a learned length is only as good as the play-through; a skipped movie never records one.
Unknown: lengths of all the game's movies (the seed table `coop.mvlen` is empty; a fresh install shows no countdown
until a movie has been watched to its end once); whether `ktgame_missl0` is the model of the real homing bullet (it is
the level's unused missile model; looks plausible, not compared side by side).
Evidence: tools/qa_cutscene.sh 40/41 then the failing check's pattern fixed (log had the line, the pattern required
end of line before `\r`), checked against the run's log; MUTANT=upright caught by "client's ghost Kurt faces like its
own" (orientation dot 1.000 with the fix); tools/qa_minigames.sh 15/15; skydive screenshot shows both Kurts in the same pose.

## 2026-10-23: two client crashes in movies (level 8 intro, end of the torpedo minigame), measuring movie lengths
Observations:
- Level 8's intro (8-8) crashed the client a few seconds after go, every time (two of two), host fine. Crash
  in gob_delete (0x449715, write to 0xa6 = null gob + the flags word). The movie deletes the level's coneheads
  by name (`Level.ForAllConeheads(omGobDelete)`, level8.lua); a client has already removed its copies of the
  host's enemies. Fix: coop.lua wraps omGobDelete to skip a gob coop_alive says is gone (coop.lua loads before
  the level scripts, so the movie tables capture the wrapper).
- End of the level 2 torpedo minigame (won through the game's own tpgame_timer.OnCustomKey): the client crashed
  3 s later (when torpgob.OnTimer swaps the scenes back) in every unguarded co-op run (more than six); once the host
  as well. A single game did not crash. Not caused by the minigame ghost (fallspec off), the puppet (removed) or
  stasis calls on gone gobs (guarded, none logged).
- Crash site 0x48b00d: SSE memcpy (0x483cb0) past the end of a heap block, called from 0x447bf2 in the update of
  a gob part of kind 1 (gob +0x70 -> part, dword 0 = kind; dispatcher 0x448930, kind 1 -> 0x447ab0). That part is
  a particle emitter: realloc (0x473550) of an array of 32-byte entries at +0x7c, max +0x0c (capped 256), and a
  memmove of [+0x34] * 32 bytes. On the client, `ml2b_cameragob` (level 2's movie camera) has an emitter whose
  +0x34 is -15 from the level load on (the host's is 0), so the move size is about 4 GB.
- The game writes -15 back every frame while that camera is held. The bridge now resets a negative +0x34 before
  each scene update (frame.c, fix_emitters; `coop_emitter_guard(0)` turns it off): the client survives (two of two).
- Code read from a running game's memory (ReadProcessMemory, 0x400000..0x4e0000): the exe on disk is packed.
- tools/qa_launcher.sh left `player.ini` with id 0a0b0c0d in the lab copies it used; a client sharing an id with
  an earlier player got player 3 instead of 2, and the cutscene test failed on that. It now restores the files.
Derived: +0x34 is an index into that emitter's ring of particles (other writers at 0x447802 / 0x44845c set it
from a running index), not a plain count.
Unknown: why the client's camera emitter gets -15 (only on the client, already at load); whether levels 5 and 9
hit the same crash (qa_minigames now checks all three).
Measuring movies: tools/measure_movies.sh boots two games at each movie checkpoint (the cp == N branches of the
level scripts' Level.Init) and wins minigames through the same hooks; lengths repeat within 0.2 s between runs.
Movie names: the playing gob's name, step count and a sum of its animation ids and waits; the level is not in it
(a movie of a level load starts before coop.level is set, so it held the previous level).
Results (later the same day):
- 31 movies measured (every Level.Init movie checkpoint and what their Done chains led to, credits 329 s); the table is
  coop.mvlen in lua/coop_dev.lua (the longest measure of each). The same movie measured twice differs by 0.2 s or less,
  except the shared boss.lua boss movie (75.5 / 77.0 / 77.5 s over levels 10, 11, 12).
- Trigger movies (coop.moviehooks: room cutscenes, death movies, bridge, KillZizzy): calling the function at each play
  checkpoint did not play any in 45 minutes (l1_r6 started at 1-1 and did not end in 10 minutes; l3_bathroom03 at none).
  They get a countdown once a game has watched one to its end (learned, coop.ini [movies]).
- The torpedo-minigame crash guard: tools/qa_minigames.sh 24/24 (all three minigames won, both games live on);
  MUTANT=noguard (coop.ini [test] emitter_guard=0 from the start) fails level 2 only. Turning the guard off after the
  level has loaded does not crash: the bad index appears during the load and is repaired by then.
- Level 8 intro: tools/qa_level8_intro.sh 6/6; MUTANT=gonegob crashes the client. Deleting a fresh gob twice does not
  crash, so that was no test of this.
- The host keeps each player id's slot ([slots] in player.ini) and gives a new id a slot nobody has had: lab copies that
  hosted earlier runs made their client player 3, 4, ... and tests that expect player 2 failed. tools/install.sh now
  drops [slots] / [steam_slots] from the copies it sets up.
- Two installs at once can collide on the shared settings.lua (`sed: cannot rename ... Permission denied`).
Evidence: tools/qa_cutscene.sh 42/42 with the table (banner "Cutscene 1:27 | Skip cutscene: 1 of 2 (Backspace)" on the
first viewing of the lounge movie); measuring output in local/movies/.

## 2026-10-23: every option in the launcher, an Interface page, the skip key as an action, the missile ghost's nose
Observations:
- mdkGobPointToXYZ(g, dx, dy, dz) takes a direction (not a point) and turns the model's +y that way: for
  (0, 0, 10) it gave the quaternion (0.707, 0.707, 0, 0). The skydive's missile model (ktgame_missl0) has its
  nose on +z, so a ghost turned that way flew on its side. Three still test missiles (no rotation; +x; +y,
  screenshot) showed it: the unrotated one is a dot seen from above, nose up. The ghost now gets the rotation
  from +z to its heading, written with a new bridge call (coop_set_quat; the engine's setter copies the
  quaternion at gob +0x24 the same way).
- The game has no master volume: three levels in settings.lua (chSndSetEffectsVolume, chSndMusicVolume,
  chSndSetVoiceVolume, 0..1, default 0.25), and the game saves them from its own state, so scaling them in
  Lua would compound on every save. The launcher edits those three lines (10% steps).
- "sed: cannot rename ... Permission denied" when installs ran beside starting games: inject.sh's lock only
  serialises installs; a game that is starting has settings.lua open and Windows refuses the rename. A copy
  that failed there kept its old coop.lua and failed tests on code it did not have. inject.sh now retries.
- The cutscene test's host died in the skydive once (a laser), which starts the death camera as new movies and
  broke the end-of-skydive checks; the test now swaps the game's missile update for one that never hits.
Derived: a [features] switch that takes an action's keys away (coop.BindAction) needs no change in the bridge.
Evidence: qa_launcher 32/32 (Interface switch and music level written), qa_menu 38/38 (Interface page
screenshot), qa_emote 21/21 (MUTANT=nofeat caught), qa_cutscene 48/48 (cutscene box, timer and ghost switches,
missile nose; MUTANT=sideways caught), qa_controls 40/40 (Skip cutscene on the Co-op page).

## 2026-10-23: Esc skipped a co-op cutscene for one player; character collision distance
Observations:
- In a manual session the 112 s opening comic ended after 3 to 10 s with no skip vote logged. A key sweep
  (W, E, F, Tab, Enter, Space, 0, keypad 0, both mouse buttons, Esc, Backspace, one at a time on the client,
  StopScript wrapped to log) showed Esc: the engine handles COM_PAUSE itself (no comfuncs entry) and during a
  movie it calls StopScript(Save.movietocancel). The script runner also calls StopScript when a script runs past
  its last step, so the wrapper only turns a stop in the middle of the co-op movie, within 1 s of a key press,
  into a skip vote.
- Character collision: with the partner's puppet held d units from the standing host at 1-1 (moving the host
  there each time), d >= 1 moved nothing; d <= 0.75 moved the host out to 0.88 apart, smoothly. The puppet was
  kept 3 away, which players saw as the partner sliding off. Now 1. A host simply standing next to a puppet held
  0.5 away was not moved at all, so only moving into an overlap triggers the push.
- The skydive lands by itself when ktgame_timer's animation reaches its custom key: a test that waits must hold
  that back.
Evidence: qa_cutscene 49/49 (MUTANT=escskip caught), qa.sh 52/52 (MUTANT=overlap and magnet caught by the
"partner right beside" check).

## 2026-10-23: a hero stuck in a wall after a switch; emote wheels per hero
Observations:
- Doc walked into the wall left of 1-1's start (x -39), then the switch to Kurt put Kurt on Doc's spot: Kurt could
  not move in any direction. Placed 0.2 or more away from the wall he moved. Kurt (and Max) are wider than Doc.
- Tried as a test of a spot: mdkAILineOfSight between the player and a probe character (reported clear even 2 units
  into the wall, and differed by direction: facing, not walls); dropping the hero from 0.4 above the spot (a stuck
  hero still falls). Neither tells a stuck spot. Going back 0.5 along the recent path did not help when Doc had slid
  along the wall (0.17 away). What works: after a switch, while a move key is held and the hero has not moved, try
  the spots around in turn (8 directions, 0.3 then 0.6); the first worked here.
- Animations each hero has (om_anim_state on the player, ids 0-159): Kurt 61, Max 61, Doc 70. Max has no THROW (57),
  Doc no ACTION05 (82): the shared wheel's slots logged "does not exist for this character".
Evidence: qa_char 12/12 (MUTANT=nounstick caught), qa_emote 25/25.

## 2026-10-23: regression takeover, private configuration and bounded QA lanes

Observations:
- The inherited sequential full ladder completed 31 gates: 28 passed. Controls failed
  22 checks, the lossy projectile gate failed one movement check, and campaign
  fixtures 3-10 and 3-11 reported client Lua arithmetic errors. Evidence:
  `local/regression/20261023-163734-876744`, sweep `local/sweeps/20261023-171411-x4`.
- The controls screenshot shows Interface where the test expected Controls: adding
  Interface shifted the navigation by one item. The restart PID write was also
  accidentally inside a shell comment. Both test defects have been corrected.
- The lossy projectile's first client-3 snapshot contained an active grenade with
  velocity; its second snapshot contained no grenade. The test failed because it
  requires two positions, not because the captured positions were identical.
- Five subprocess-based runner tests verify actual suite overlap, exclusive gate
  barriers, busy-allocation rollback, malformed-allocation cleanup, conflicting
  resource rejection and cancellation. All 31 Python tests passed after integration.
- Workstation instructions and allocation commands have moved to ignored private
  configuration. Public tooling uses documented environment variables and an
  optional command/JSON resource interface; no particular local allocator is required.

Derived facts:
- An early movie GO path sets state to play without initialising tick_at, while
  MovieTick subtracts tick_at from the clock. The affected campaign logs show the
  early-GO message immediately before arithmetic errors. This is a strong candidate
  for the campaign failures; a targeted reproducer/fix is still pending.

Unknown:
- Whether the lossy projectile movement check is consistently sampling too late.
- Full parity and runtime of the new three-lane ladder; validation is running.

Follow-up observations:
- The three-lane full ladder passed 30/31 gates, including all 96 checkpoints, in
  about 21 minutes. The sole failure was character-motion: later-level probes had
  only 11-17 frames in a one-second sample (the oracle requires 20), and one jump
  was missed. Sequential baseline evidence had passed. Character-motion is now an
  exclusive gate; no motion assertion was lowered.
- A subsequent three-lane selection passed character-motion (44 checks, 74 s),
  HUD, native projectiles and lossy native projectiles. Motion ran alone before
  the other three overlapped. The two projectile variants started in the same
  second and produced distinct evidence directories after adding PID suffixes.
- Native projectile snapshots are now captured as a short pair per client, with
  clients sampled concurrently, so another client's shell round trips do not
  consume the grenade's lifetime before the second snapshot. The same active,
  foreign-projectile and position-change assertions remain in place.
Evidence: `local/regression/20261023-171852-379664` (full),
`local/regression/20261023-174024-204153` (exclusive motion plus simultaneous
projectile variants).

Deterministic movie-race validation:
- Receiving MV_GO before MovieStarted formerly entered play with nil tick_at,
  producing the same arithmetic errors as the earlier campaign fixtures. The
  branch now calls the ordinary MovieGo initialiser. The focused two-game check
  observes a positive elapsed clock, normal completion and no Lua errors.
- The earlygo mutant restores the original branch and is caught by the clock
  assertion, with the arithmetic errors also observed. An initial test assertion
  accidentally matched the echoed command source; anchoring it to the actual
  `lua:` log prefix fixed the false positive before accepting the mutant result.
Evidence: `local/regression/movie-clock-20261023-174546` (positive 4/4; the mutant
must fail the clock and no-error checks, and the negative harness reports caught).

Fresh-resource sweep observation:
- A campaign-only run with a newly created helper folder hit concurrent NTFS file
  creation errors while shards copied PowerShell scripts. Install failure was then
  ignored and the sweep booted stale configuration. The invalid run was cancelled;
  all eight owned games and the lease were released.
- Helper copying now takes a short lock, and a failed install stops a shard before
  boot. A real-shell concurrency reproducer fails before the lock and passes after;
  another shell test verifies that failed installation never boots a game. All 33
  Python tests pass. A new fresh-folder campaign retest is running.
Invalid-run evidence: `local/regression/20261023-174743-655439`.

Final fresh-folder validation:
- Campaign profile passed all seven gates, including all 96 checkpoints after
  the movie-clock and helper-install fixes. All 16 resource slots were free after
  cleanup. The final 33-test Python run, shell syntax, Markdown target checks and
  tracked-file workstation audit pass.
Evidence: `local/regression/20261023-175357-180407`,
`local/sweeps/20261023-175423-761414-x4`.

## 2026-10-23: separate private workstation configuration from public requirements

Observations:
- Workstation instructions, environment integration, allocation adapter and private
  handover history now live in a separate private workspace. Ignored links keep the
  existing checkout's generic configuration entry points working. A filesystem-only
  setup helper attaches new worktrees and refuses to overwrite different configuration.
- The public guides retain generic configuration, allocator protocol and executable
  dependencies. Unnecessary workstation-specific setup prose has been removed.
- Validation: all 33 repository Python tests and three private setup tests pass;
  environment mapping, adapter permissions, symlinks, shell syntax, Markdown targets
  and whitespace checks pass. No game tests were run for this configuration move.

## 2026-10-23: full ladder before the public alpha, a hero stuck when the player waits

Observations:
- Full ladder with three lanes and four campaign shards on the release candidate: 30 of 31 gates passed, the
  campaign (96 checkpoints) in 240 s. Controls and character-motion, which failed in earlier runs, passed.
- character-wheel failed one check: after switching from Doc to Kurt at the wall in 1-1, Kurt did not move
  (-39.09 124.64 before and after W, S and D). In the two earlier full runs he moved about 10 units, and the game
  logged `character: unstuck at spot 1`; this time that line is missing.
- The unstick time limit (4 s) started at the switch. The test switched, waited 3 s, read the position over a
  command round trip, then pressed keys; with the other lanes running, the keys arrived after the limit.

Derived facts:
- A player who switches hero at a wall and waits more than 4 s before walking stays stuck. Fix: the limit starts
  with the first held move key. The test now waits 6 s after the switch, past the old limit.

Evidence:
- Fixed code: character-wheel gate 12/12, Kurt -39.09 124.64 -> -29.42 126.11. `MUTANT=nounstick`: caught. The
  previous code with the new test: the wall check fails. Mod commit 75998a7, released as v0.2.0-alpha.
