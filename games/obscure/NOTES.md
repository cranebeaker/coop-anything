# Obscure (2004): research notes

Technical log for the online co-op mod in [obscure-coop](https://github.com/Gyurooo/obscure-coop) (MIT). The
summary, QA table and known issues are in [README.md](README.md). This file keeps the evidence: addresses,
what each test showed, and what was tried and dropped.

Labels used below: **observed** (seen in the exe, a log, memory or a screenshot), **derived** (inferred from
observations), **unknown** (not found). Addresses are virtual addresses in `Obscure.exe` (image base `0x400000`,
no ASLR) and apply to build 215286 only.

## 1. Game identity

- Obscure (Hydravision Entertainment, 2004), Steam app 254460, build id 215286. Obscure II (app 254480) is a
  separate game and is not covered.
- `Obscure.exe` SHA-256 `7107439eb6ee1b20cad32e87f7a10a3a95b71c0b5ee26b1ec9139b263577a0f2`, 3,662,336 bytes, PE32
  x86, PE timestamp 2014-03-18, FileVersion 1.0.1.0, CompanyName "Mighty Rocket Studio".
- `steam_api.dll` SHA-256 `6756b377865f55e1c8c8a21526f87559741f204036ce33e5be594d3e13b53bce`, `binkw32.dll`
  `3627edef5383ad43ffc14ab14f8d81171b6c08fdcb9f52a657f24301d3fd7b00`.
- Files in the install: `Obscure.exe`, `steam_api.dll`, `binkw32.dll`, `cachpack.hvp`, `datapack.hvp`,
  `kinepack.hvp`, `strmpack.hvp`, `mrs.bik`, `logoload.bmp`, `video_cfg.dat`, `Deluxe Content/`.
- The exe is SteamStub-wrapped (`.bind` section) but `.text` is not
  encrypted (entropy 6.57), so it can be analysed directly; it was never unpacked.
- Engine: **Hydravision in-house, not RenderWare** (the Wikipedia infobox says RenderWare). There are no
  RenderWare strings or `.txd`/`.dff` files, and 722 RTTI class names show the layers: `CZoe*` (renderer, scene,
  DX9), `CNoe*` (physics, entities), `CToe*`/`CHoe*` (behaviour, AI), `CHV*` (input, sound, streams), `CZPackage*`
  (archives), `CZMemoryCardSteam` (saves), `Cslh_L*` (script language) and about 300 `CTM_*` script natives.
- Imports: VERSION, WINMM, AVIFIL32, MSVFW32, d3d9 (`Direct3DCreate9`), DINPUT8, DSOUND, SHLWAPI, binkw32,
  XINPUT1_3 (by ordinal), KERNEL32, USER32, GDI32, COMDLG32, ADVAPI32, SHELL32, ole32, steam_api (UserStats,
  Utils, Friends, User, Apps, RemoteStorage, callbacks). **No network code**: no winsock, dplay or Steam
  networking or matchmaking imports.

## 2. Prior art

- No online co-op mod was found. Searched: the co-op hub's directory, games and issues; web searches in English,
  French and Russian; GitHub repository and code search; ModDB and Nexus (search results only, Nexus returns 403
  to automated requests); the Steam discussions for app 254460.
- The game has 2-player **local** co-op on one screen: the second player presses START on a second controller
  once the second character is on screen (about 10 minutes in).
- Players report that Steam Remote Play Together works for the whole campaign (it streams the host's screen and
  needs a controller for at least one player, because both PCs look like one keyboard) and that Parsec works:
  [thread 1](https://steamcommunity.com/app/254460/discussions/0/540735426727130309),
  [thread 2](https://steamcommunity.com/app/254460/discussions/0/142261027580957991).
- Asset tools: a Blender add-on for `.zwo` files
  ([ObscureZWOimporter](https://github.com/studiosleafmore-maker/ObscureZWOimporter), alpha, no license stated) and
  HVP unpack/repack tools for Obscure II on Nexus (not tried on this game).
- Other pages: [Steam store](https://store.steampowered.com/app/254460/),
  [Wikipedia](https://en.wikipedia.org/wiki/Obscure_(video_game)). The Co-Optimus page for
  `Obscure__The_Aftermath` is about Obscure II.

## 3. Files and engine facts

**HVP archives** (observed, reader checked on all four): big-endian. Header `0x28` bytes: magic `HV PackFile\0`,
u16 3, u16 1, then u32 root entries, total entries, file entries, TOC size, 8 unknown bytes. TOC entries (depth
first) start with u32 entry size and u8 type: 0 = directory (u32 0, u32 child count, u32 name length, name),
1 = file (u32 compressed, stored size, size, crc, absolute offset, name length, name). Data is zlib (`78 da`);
all 847 checked files inflate to the listed size. The `crc` field is not standard CRC-32 (unknown; only matters
for repacking). File counts: cachpack 732, datapack 1708, kinepack 40 (`.bik` cutscenes), strmpack 6447 (`.ogg`).
Content: 202 compiled `.hoe` scripts (named globals such as `gPlayerDetected`, 179 reference `TM_*` natives), 150
`.tm` per-level binaries (meaning unknown), `.noe`, `.dip`, `.coa`, `gdatas*.ini` (plain-text weapon and
difficulty tables), `default.ipt` and `default_pad.ipt` (input maps), `chapters/chaptr0..9.sav` and `default*.sav`
(save templates).

**Script natives** (observed): 255, each a class whose constructor calls `FUN_005b8860(name, flag, nargs)`; slot 3
of the vtable is the body; the by-name factory is `FUN_004a3140`.

**Startup checks** (observed): `CreateMutexA(NULL, 0, "obscure_lock")` (a second instance exits); the main function
`FUN_00547290` only runs when `IsDebuggerPresent()` is false; `SteamAPI_Init` failure shows "Steam must be
running" and exits.

**Tooling pitfalls** (observed): Ghidra 12.2 needs Java 25 (set `JAVA_HOME` and PATH for `analyzeHeadless`); its
auto-analysis does not label MSVC vtables, so functions reached only through a vtable are missing until created
from the constructor's `*this = &PTR_...` address; RTTI (type descriptor to complete object locator to vtable)
finds them by class name.

## 4. Game state and addresses

**Global game object** `*(0x6ab8e4)`:

| Field | Meaning |
|---|---|
| `+4` | state, 2 = in play |
| `+8` | state-machine request: 5 = change level (name `+0x590`, entry `+0x5d0`), 3 = load a save (slot `+0x838`) |
| `+0x4fc`, `+0x500` | the two real-player slots (`CRealPlayer*`); natives reject index 2 and up |
| `+0x510` | level manager (humans list `+0x1c`, items `+0x30`, enemies `+0x58`; level id byte at `+8`) |
| `+0xa20` | save manager (per-player device table at `+0x2c0`) |
| `+0xa24` | story flags: 161 bytes, written by `TM_SetGameEventState` (`0x4d8c70`) |
| `+0x7bc` | the team's shared bag (categories 8..17 at `+0x14 * category`) |

**Real player** (`CRealPlayer`): `+4` role (100 or 200), `+0xc` controlled character, `+0x10` its `CInventory`
menu, `+0x84` controller object (its `+4` is the device type: 1 keyboard, 3..6 XInput users 0..3). `FUN_0044b7e0`
switches the controlled character; `FUN_0044c2b0` runs every frame for both slots.

**Character** (`CPlayerHuman`, vtable `0x6449e4`): `+0x20..+0x28` position, `+0x2c..+0x34` facing, `+0x54` health,
`+0x58`/`+0x5c` dead flags, `+0x60` state (10 idle, 20 walk, 30 run, 101 aim, 103 swing, 50/52 door), `+0x68`
physics body, `+0x78` id, `+0x88` controlling real player, `+0x128` embedded `CPlayerInventory`, `+0x224` door
in use, `+0x230` requested state, `+0x238..+0x254` move block. Slot 1 of the vtable is `Update` (`FUN_0043f6e0`),
which runs the state machine `FUN_0043bf60`; in idle it switches on the requested state the controller wrote
earlier in the frame. Setting `+0x60` directly does not stick; the requested state is what drives animation.
Requested states are one-frame values (a swing is `103` for a single frame).

**Enemies** (`CPlayerPNJ*`, e.g. `CPlayerPNJMord` vtable `0x64cafc`): level manager list `+0x58`, same node layout
`{item, next}`. Position and facing come from the physics body each frame. `TM_SetLife` uses vtable slot 6
(`AddLife`); `TM_KillPlayer` sets `+0x54 = 0`, `+0x58 = 1`, `+0x5c = 1`.

**Items and doors** (list `+0x30`, keyed by type `+4` and id `+8`, position `+0xc..+0x14`): `CItDoor` type 1
(`+0x88` open request, `+0xfc` open time up to 2000), `CItPlayerPos` type 2 (spawn markers), collectables with
`+0xfc` = collected. Collecting is `FUN_00448a00` (AddItem). Ids are unique only within a type. All collectables
are in a registry at game `+0x648`, searched by `FUN_004ebdd0`. Weapons in an inventory are not in the level list.

**Inventory:** `CPlayerInventory` has 8 lists `{head, tail, iter, count, flag}` from `+0x10` (stride `0x14`,
category 1 bare hands, 2..7 weapons), weapon in hand `+0xd8`, active item `+0xdc`. Item fields: `+4` type, `+8` id,
`+0x3c` count, `+0xfc` collected, `+0x12c` weapon energy (weapons only). The inventory menu `CInventory` has an
items section (`CInventorySectionItems`, `0x1a8` bytes, ctor `FUN_00512890`) that the constructor `FUN_0041f6a0`
creates only once (global flag `DAT_006a6868`), so player 2 shared player 1's menu. **Give** is inventory action 6
in `FUN_005111a0`: `FUN_00449410(item, count)` on the giver then `FUN_00448a00(item, 0, 0)` on the receiver.

**Levels:** 111 names in a table (`FUN_0042c150`: ids at `0x695df8` as bytes, name pointers at `0x695e68`), for
example A000 = 1, B000 = 17, C000 = 49, D000 = 65, M000 = 193. Ids 225..234 (`WLOCK`...`WHOMM`) are developer
levels that are not in the shipped archives; requesting one sends the game back to the menu (`FUN_00416a70`).
`TM_LoadLevel` (`0x4ddb80`) and `TM_EventChangeLevel` (`0x4d92a0`) write the three change-level fields.

**Camera:** each camera follows a real player (`camera+0x180`); the two-player camera manager is `game+0x1c`
(`+8` = followed slot; toggled by `FUN_0040bf10`). When a character dies and no free living character exists,
`FUN_0044c2b0` **swaps the real players**: the survivor becomes player 1 (slot `+0x4fc`, role 100), the dead one
player 2. Only player 1's device opens the pause menu.

**Input:** controller polling is `FUN_005868e0`. Type 1 keyboard, 3..6 XInput users 0..3 (`XInputGetState`,
about 3,000 polls/s, all four users every frame). Button order from the pad reader: 01 A, 02 B, 03 X, 04 Y,
05 BACK, 06 START, 07 LB, 08 RB, 09 and 10 stick clicks, 11 dpad up, 12 left, 13 right, 14 down, 15 LT, 16 RT.
Player 1 moves with W/A/S/D; RETURN is the action key; **holding Q** (LB on a pad) opens the inventory; **LT** aims
and **LT+A** swings. START on the second pad raises "Player 2 joins the game", confirmed with Enter or A on
player 1. Per-player device assignment is stored in `tm_input.ipt` (a raw image of the binding tables, `0xcd`/`0xcc`
filler; changing the device table in memory does not rebind). XInput is read whatever window has focus.

**Saves:** Steam remote-storage files (`game0..9.sav`, `recover.sav`, `options.sav`, `tm_input.ipt`). Saves are
22,528 bytes, version byte 6 for `options.sav`, with a standard CRC-32 over bytes `[4:]` stored little-endian in
bytes `[0:4]`. The chapter templates are 30,720 bytes and are rejected as "Damaged game" (the loader checks the
expected size). The save writer is `FUN_00416640`; the save menu `FUN_0053fa60` is its only direct caller. The
load menu calls `FUN_004524d0(slot)` on the save manager, then sets `game+0x838 = slot` and `game+8 = 3`; that is
the only place `+8 = 3` is written. `video_cfg.dat` is read and written through `ISteamApps::GetAppInstallDir`
(app id `0x3e1fc`), so a copy of the install still reads and writes the store install's file.

## 5. Co-op design

- **Remote input:** a proxy `xinput1_3.dll` next to `Obscure.exe` reports a virtual pad on one XInput index whose
  state comes from the remote player, so the game treats them as its own local player 2. Real pads can be hidden
  from a test copy (`HIDE_REAL_PADS`). A virtual DirectInput keyboard (`GetDeviceState` slot 9, every keyboard
  device the game creates) drives menus in tests; the game polls no mouse.
- **Authority:** the host's game is the real one; the guest's copy mirrors it. The mod patches `CPlayerHuman::Update`
  on the guest so each mirrored character gets the host's requested state and move block before the game's own
  state machine runs, and the host's position and facing after it.
- **Per-frame hook:** `steam_api!SteamAPI_RunCallbacks`, called once per main-loop pass.
- **Transport:** Steam P2P through the game's own 2014 `steam_api.dll`, called through C++ vtables (`SteamUser017`,
  `SteamFriends013`, `SteamNetworking005`); friends only, one guest. The guest says hello to a configured host
  Steam id or to every friend whose game is app 254460. All packets are unreliable and under 1200 bytes; the
  protocol handles loss. A UDP mode backs the one-PC tests.
- **What is mirrored:** characters (position, facing, health, state, requested-state events), enemies, the 161
  story flags (every 15 frames), level changes, collected items, doors (with the door in use), weapon lists and
  the shared bag, inventory menu state (hold buttons, scroll, action menu), picture viewer, on-screen messages,
  save transfer and loads. Not mirrored: ladders, racks and locks (the result is mirrored, not the animation),
  readable documents' page text (untested), cutscene-driven objects.
- **One-frame events:** the host hooks `Update` and records every change of a character's requested state before the
  game consumes it (a sequence number plus the last 4 values per character); the guest replays missed changes in
  order. Only a safe list of states is mirrored (10, 11, 20, 30, 31, 100, 101, 103, and 50/52 with the door
  object); mirroring the door state without its context hung the guest's main thread in `FUN_005907d0`.
- **Smoothing:** state packets carry the host's send time. The host sends on a fixed 60 Hz tick (per-frame rates
  gave about 600 packets/s at 275 fps). The guest keeps 32 snapshots and shows host time minus an adaptive delay
  (90th percentile of lateness over 128 packets plus one send interval, clamped 20..200 ms), blending position
  and facing, extrapolating up to 50 ms and snapping on jumps over 150 units. Enemies use the same buffer.
- **Per-player inventory:** the host creates the missing items section for player 2 each frame (game allocator
  `0x5e3fda`, ctor `FUN_00512890`) and patches the hand-back on close (`75 19` to `EB 19` at `0x41fb1f`).
- **Movement frame of the guest's player:** `FUN_0044c2b0` (real player update) turns the stick into a world direction
  relative to a forward vector: a locked reference (real player `+0xa8/+0xac/+0xb0`, valid while `+0xa0` is 1 and
  the stick stays close to the direction it was captured with, `+0xb4/+0xb8`) or the level's current camera
  (`*(*(game+0x514)+0x54)`, axis at `+0x40/+0x44/+0x48`). The references are copied from camera objects by
  `FUN_0040bf10` and `FUN_0040c9f0` (zone triggers, view changes). On the host these are the host's cameras, so the
  friend's stick moved him in the host's frame while his own screen showed another camera angle (playtest report).
  The guest sends the camera axis of its own level camera (packet 19, about 20 per second); on the host a hook on
  `FUN_0044c2b0` (entry `55 8b ec 8b 45 08`, trampoline) swaps that axis into the camera and the locked reference for
  the duration of the guest's real player update only and restores both after the call. Measured: with the guest
  sending a chosen axis, player 2 walked at headings 81.6, 8.6, -81.5 and -179.5 degrees for axes +y, +x, -y, -x
  (`tests/moveref_test.py`, 4/4). Camera yaw from the view matrix showed fixed angles of 0 and 90 degrees in the
  hall level (C000) and a single 90 degree angle in the courtyard. Not tested: two real camera zones at once.
- **Movable props (1.10):** chairs, wheeled objects (TV stands) and basketballs are Noe entities (`CNoeChair` vtable
  `0x6539a4`, `CNoeCasterObject` `0x65354c`, `CNoeBasketBall` `0x652f14`, `CNoeMobileRack` `0x653dc4`) wrapped by Toe
  entities (`CToeEntityChair` `0x64732c`, `...CasterObject` `0x647318`, `...BasketBall` `0x647354`, `...MobileRack`
  `0x6472d4`) in `CToeWorld` (`*(game+0x588)`), whose list at `+0xc` has nodes `{item, next}`; a Toe entity points to
  its Noe entity at `+4`. The list also holds the characters' bone entities, so props are picked by class. A Noe
  entity keeps its position at `+0xb0` (3 floats) and its rotation quaternion at `+0xbc`; vtable slots `0x4c`/`0x50`
  read and `0x54`/`0x58` set them (they also update the matrix and the physics body), slot `0x30(1)` freezes the
  velocity. The engine also has its own 9-byte serialisation for them (slot 3, tag, identity hash and x/y/heading packed
  into 4 bytes inside given bounds; slot 4 reads it back); it was not used because of the coarse quantisation. Found
  per level (class counts): chairs in most school levels (D000 1, level 19 5, 21 9, 25 13, 53 15, 71 19), wheeled objects
  in levels 52 (1), 54 (4), 68 (1), 79 (3), a basketball in level 177; the courtyard (193) and C000/B000 have none
  (they have closet doors, breakables and `CNoeEntityObject`). The host sends the poses that changed at 30 Hz plus every
  prop about once a second (packet 20); both copies build the list in the same order, so a prop is its class and its
  index among that class; the guest sets position and rotation with the entity's own setters. Test (level 54, 10 props,
  `tests/props_test.py`, 6/6): same list on both copies, a prop moved by the host's setter follows, a caster shoved
  225 to 324 units by a character in contact (teleported next to it) is within 2 units on the guest. Not tested:
  pushing by walking (the keyboard input did not move player 1 right after a level change in that setup). Not synced:
  closet doors (`CNoeClosetDoor`), `CItPushable`/`CItMobile*` items (none found in the levels scanned).
- **Breakables (1.11):** two kinds, both synced as "which ones are broken", the flying pieces are not.
  - Items of the level manager's list (`+0x30`): `CItBreakable` (vtable `0x64dc14`), `...DoorWindow` (`0x64dcf4`),
    `...Wall` (`0x64dde4`), `...Wall2` (`0x64de54`), `...Window` (`0x64e02c`). `+0x2c` is 1 once broken, `+0x30`
    counts the break animation to 1000; vtable slot `0x60` breaks with sound and effects (what `TM_EventBreakIt`
    calls), `0x64` at once. They ride in the items packet (kind 3); the guest calls slot `0x60`, or `0x64` when the
    host's animation is over. Test `tests/break_test.py` (level 71, 5 windows): 4/4.
  - Scene objects (windows in walls, vending machines = `distrib_PT`, which release item type `0xab`): a
    `CNoeBreakableGroup` (vtable `0x65307c`) holds members tagged at `+0x30` (`0xe` unbroken, `0xf` broken, `0x11`
    effects, `0x10` unbreakable); its broken flag is at `+0x7c` and `FUN_0052a820` (this = group) sets it and
    switches the members over. The unbroken member (`CNoeBreakableUnbroken`, `0x653114`) keeps the group at
    `+0x2f4`; the Toe world's list (`+0x588`, `+0xc`) points to it at `+4`. The game's own entity serializer
    (slots 3 and 4 of that member) sends the same flag as byte 6 plus a name hash. The host sends the broken indices
    (index among the unbroken members, packet 21, about every 2 s and on change); the guest calls `FUN_0052a820`.
    Test `tests/sbreak_test.py`: level 17 (8 members, two start broken, three have no group) and level 49 (10
    members): same state on both copies, one broken on the host is broken on the guest, the rest unchanged; 3/3
    each. The drink is an ordinary item, so the item sync should carry it (not checked). Breaks were triggered
    with a test command, not by shooting a window.
- **Deaths:** the host follows the real-player object that joined as player 2 (it keeps its controller through the
  swap) rather than the slot; the guest never switches onto a dead character (access violation at `0x44ba63`,
  null body, otherwise). A dead host is player 2 after the swap and its START asks "Player 2 quits the party", so
  the host opens the pause menu itself (3 s after a death or on F1: `game+0x60c = 1`, `FUN_005353d0` on
  `game+0x628`, node 400).
- **Loading a save:** the host sends a load packet when `game+8 == 3` names the transfer of that file; the guest
  waits for the file in its co-op slot, then runs the same two steps for slot 0. A joining guest loads the host's
  save by itself.
- **Isolation of the mod's own state:** the guest's saves go to a `coop_profile` folder (or a per-user folder when the
  game is under Program Files, where Windows file virtualization can redirect writes), never the player's cloud
  saves; remote-storage write calls are blocked or proxied; the window title shows the mod's status; a watchdog
  logs the main thread's address if the loop stops for 5 s.

## 6. Test evidence

Bot tests drive two real copies on one PC (host and guest over UDP) through a named pipe in the DLL (virtual pads
and keys, test commands such as `level`, `teleport`, `hurt`, `give`, `loadslot`) and assert on both processes'
memory. They live in `tests/` of the mod repo.

| Area | Test | Result | First passed on |
|---|---|---|---|
| Remote pad joins as player 2 | `net_join_test` | 3/3 | 0.1 |
| Both characters mirrored | `mirror_test` | 7/7 (8/8 after the Steam rewrite) | 0.1 |
| Animation by requested state | run log | 47 of 48 samples agree | 0.1 |
| Level change and story flags | `level_test` (id 194) | 7/7 | 0.1 |
| Enemies and health | `enemy_test` | 6/6 | 0.1 |
| Item pickup | `item_test` | 5/5 | 0.1 |
| Enemy death | `combat_test` | 6/6 | 0.1 |
| Join receives the host's save | `join_test` | 5/5 | 0.2 |
| Per-player inventory | `items_split_test` | 3/3 | 0.3 |
| Attack events | `attack_test` | host 3 swings, guest 3 | 0.3 |
| Weapon give | `give_test` | 7/7 | 0.4 |
| Shared bag | `bag_test` | 9/9 | 0.5 |
| Door and player 2 through a door | `door_test`, `p2door_test` | 7/7, 9/9 | 0.7 |
| Watchdog | `stall_test` | 4/4 | 0.5 |
| Inventory menu on the guest | `ui_test` | 7/7 | 0.6 |
| Picture viewer, messages | `view_test`, `msg_test` | 7/7, 2/2 | 0.6 |
| Death swap | `death_test` | 7/7 (9/9 on 1.8's first widescreen build) | 0.8 |
| Camera hold | `camera_test` | 3/3 | 0.9 |
| Load after death, pause menu on the host | `load_test --menu`, `load_test` | 21/21, 19/19 | 1.0 |

**Not bot-tested:** builds 1.1 to 1.8. Smoothing (1.1, 1.7), supersampling and text size (1.2 to 1.4), filtering
and ReShade (1.3 to 1.5), the font swap and its revert (1.5, 1.6) and widescreen (1.8) were checked by screenshots,
measurements and playtests over Steam (two PCs), not by the bot suite in co-op. The lag and packet-loss test
(`chaos_run.sh`) is prepared and has not been run.

Playtests over Steam (two PCs, builds 0.2 to 1.7) reported: no connection at first (the guest's log was missing;
later builds show the mod's status in the window title), a camera problem that could not be reproduced, a door
freeze (fixed), a crash when a player died (fixed), per-player inventory control (fixed), missing inventory bar,
pictures and messages on the guest (fixed) and micro-stutter over the internet (smoothing, 1.1 and 1.7).

## 7. Graphics

- **Direct3D 9, fixed function:** a draw census in the original game (in-level, windowed) showed about 520 draws
  per frame with no vertex or pixel shader bound (`SetVertexShader(NULL)` every time), all with one UV set and
  FVF `0x144`/`0x152`. The game submits no Direct3D lights (0 `SetLight` calls); it lights with baked vertex
  colours plus an ambient value.
- **Supersampling (`SSAA`, 1.2):** the game renders into a back buffer 2x or 4x wider and taller; each `Present`
  shrinks it into an extra window-sized swap chain with `StretchRect` LINEAR (exactly 2:1 is a box filter). The
  game's `GetClientRect` answers bigger so its layout follows, and fullscreen becomes a borderless window with the
  renderer on its windowed path (`renderer+0x124 = 0`). First attempt drew into the top-left quarter.
- **Text size (`TEXT_SCALE`, 1.4):** fonts are bitmaps drawn 1:1 in pixels by `FUN_0045ba60` through renderer vtable
  slots `+0xb8`/`+0xc0`. While a string is drawn its quads are scaled about the string's anchor.
- **Filtering (`ANISO`, 1.3):** the game sets `MINFILTER` LINEAR and `MIPFILTER` NONE. A mip LOD bias changed
  nothing; 16x anisotropic filtering added about 4% detail on far angled ground (mean gradient, A/B).
- **Sharper fonts (1.5), reverted in 1.6:** a texture swap keyed by the Direct3D texture address drew letters on the
  ground after the game freed a font texture and reused the address.
- **ReShade 6.8.0:** runs on the game's D3D9; with `SSAA` it sees the extra swap chain as a second runtime and
  needs `ReShade2.ini`; `DepthCopyBeforeClears=1` makes depth work for MXAO. Presets: MXAO, SMAA, Deband, CAS,
  Vibrance, and a second one with colour grading, grain and vignette.
- **16:9 widescreen (1.8):**
  - At fullscreen 2560x1440 the game set its viewport to 1920x1440 at x = 320 (pillarboxed 4:3) and built the
    camera from that. `FUN_005ab420` makes the screen rectangle and letterboxes only when `renderer+0x298` is
    non-zero; the renderer constructor `FUN_005ac170` sets it with `mov dword [esi+0x298], 1` at `0x5ac1db`.
    Patching that `1` to `0` gives the full rectangle. Windowed runs and the supersampling window already used
    the full client rectangle.
  - The game keeps the horizontal FOV fixed (projection `_11` = 2.4142, tan of 67.5 degrees) and derives the vertical
    one from the aspect (`_22` 3.219 at 4:3, 4.292 at 16:9), so 16:9 alone cropped top and bottom. The camera's
    set-FOV function `FUN_00590230` (floats at `+0xc` horizontal, `+0x10` vertical; every caller passes values for
    4:3) is replaced by a hook that keeps the 4:3 vertical view and sets the horizontal angle from the real aspect.
    Scaling the projection matrix instead showed pop-in at the edges because the game culls with its own camera
    values; the camera hook fixes that.
  - The set-FOV hook must be idempotent. Camera views set their angles from the level's data (horizontal angle is the
    base, the vertical one is derived from the aspect), and the game also feeds a camera's current angle back in when
    views change (the vertical angle is recomputed, so only the horizontal one matches a stored value). Widening that
    again made the view grow by 0.75 x aspect on every camera change (logged: base 0.6 gave 0.7824, then 1.0056 on the
    next set). The hook now remembers the horizontal angles it produced and passes matching ones through; a base
    value always gives the same result (0.6 -> 0.7824, 0.7854 -> 1.0092, 1.1427 -> 1.4173).
  - Checked by back-buffer dumps at fullscreen 2560x1440 (courtyard, staircase, hall, menu). Open: the main menu
    logo and background are stretched wide; videos and inventory screens were not checked at 16:9.

## 8. RTX Remix experiment (parked)

NVIDIA's RTX Remix targets fixed-function Direct3D 9 games, which Obscure is. Remix 1.6.1 (x86 release) was run
in a test copy.

- **Startup crash (cause found, workaround written):** `FUN_005ac610` walks a table of device types and calls
  `GetDeviceCaps` and `CheckDeviceType`; one entry is garbage (`0x658d2c`: the table slot is overwritten with the
  adapter count). Native Direct3D 9 rejects it; Remix's D3D9 accepts any type, so the game formatted a bad name and
  crashed in a loop. Rejecting device types outside 1..3 like Windows does fixes the start.
- **Lights:** the game's lights are objects, not Direct3D lights: `CLightingElementLightOmni` (vtable `0x64472c`,
  type 10), `...LightSpot` (`0x644708`, type 11, `0x104` bytes), `...DarkOmni`, `...Ambient`, `...AmbientBox`.
  Omni layout: `+0x10` enabled, `+0x20..+0x28` position (equal to Direct3D world coordinates), `+0x2c..+0x34`
  direction, `+0x38` colour ARGB, `+0x3c` range. Indoor levels have them (B000 8, C000 6, D000 up to 4; range 160
  to 1566); the courtyard has none. Handing them to Remix as legacy Direct3D lights worked (real shadows indoors).
  One disabled spot light with default range 100000 exists in C004, likely the flashlight's; not verified.
- **Open:** in the hall (C000) every wall, floor and locker showed one swirl texture under Remix while doors, props
  and characters were right. Ruled out: lighting, texture format (the courtyard uses the same R5G6B5 textures),
  UV sets, blend state, and the Remix options `recomputeTextureHashOnWrite`, `alwaysWaitForAsyncTextures`,
  `ignoreLastTextureStage`, bridge `sendAllServerResponses`, and keeping freed textures alive. A Remix capture
  would show which textures it holds. The code for this was archived and is not in the shipped builds.

## 9. Open items

- Ladders, racks and locks do not animate on the guest (requesters of states 85..87, 122..142 are mapped; the
  object's type and id would have to be sent with the event).
- Run the lag and loss test and the full bot suite on the current build.
- Widescreen: main menu logo, videos and inventory screens at 16:9.
- Meaning of the `.tm` files, the `.hoe` bytecode format, the HVP crc, the layout of `tm_input.ipt`.
- Obscure II (app 254480) is a separate target; nothing there was checked.
