# Forager: engineering notes

Notes behind [README.md](README.md), for the next agent working on this game or on another GameMaker YYC game.
Everything here is for Steam build 6160665 (`Forager.exe` PE timestamp `0x60148428`); addresses are VAs for image
base `0x400000` (the exe has ASLR: address in the process = VA - `0x400000` + module base). Source of the mod:
https://github.com/melogo5/forager-coop. The decompiled game code is not published anywhere; it is regenerated from
your own copy of the game with the steps below.

## Getting readable code out of a YYC exe

1. `data.win` metadata: UndertaleModTool CLI 0.9.2.0 with C# scripts dumps objects (parent, sprite, mask, events),
   rooms and their instances, sprites, sounds, strings and extensions. A YYC `data.win` has no code, but object and
   room data are complete, which was enough to rebuild interiors from a room dump.
2. Function names: every YYC function pushes a pointer to its name string (`gml_Script_PlayerFind`,
   `gml_Object_objPlayer_Step_0`) onto the YYC stack trace (top of stack at `01B77490`). Find the string, find the one
   function that references it: that is the function. A Python script builds the name map from the exe's strings, a
   Ghidra script renames the functions and exports pseudo-C per object event and script, with GML-to-GML calls
   renamed too.
3. Ghidra 12.1.4 headless: of about 4080 names, 4015 bound to functions (66 not found, for example
   `objPlayer/KeyPress_17`), 3969 decompiled. 46 failed: 300 s timeout or more than 14 GB RAM. A retry one function
   at a time with a 1 h timeout and a 2000 MB response buffer did not help (`objPlayer/Step_0` times out;
   `WorldControl/Step_2` and `objPlayer/Step_2` exceed the response buffer, and with a bigger buffer the decompiler
   hangs; 4096 MB does not start at all). For those, export the full listing plus a summary of called functions and
   referenced strings, and trace the running game.
4. Annotate the pseudo-C: the YYVAR table (`{const char* name; int id}` in `.data`, ids filled in by the runtime at
   start) maps `DAT_xxxxxxxx` to GML builtins and variables, and variable slot numbers map to instance variable names
   (2349 of them). After that, `GETVAR(self, inventory)` reads close to GML.
5. A tracer in the DLL hooks any function listed in a text file next to the exe (`name<TAB>VA`) and logs the GML
   call chain. This answered most questions the 46 undecompiled functions left open.
6. A probe DLL confirmed the basics before any co-op code: the `version.dll` proxy loads, MinHook hooks on
   `gml_Script_PlayerFind` and `gml_Object_objPlayer_Step_0` work, reading `x`/`y`/`id`, a global by slot and calling
   a builtin by name (`ds_list_size`) work.

## Runtime (GameMaker 2.2.5.378, x86, cdecl)

| What | Signature |
|---|---|
| Script `gml_Script_X` | `RValue* (CInstance* self, CInstance* other, RValue* result, int argc, RValue** args)` |
| Object event `gml_Object_X_Event` | `void (CInstance* self, CInstance* other)` |
| Builtin (table entry) | `void (RValue* result, CInstance* self, CInstance* other, int argc, RValue* args)` |

- **RValue** (16 bytes): `+0` value (double, int32, int64 or pointer), `+8` flags, `+0xC` kind. Kind `& 0xFFFFFF`:
  0 real, 1 string, 2 array, 3 ptr, 5 undefined, 6 object, 7 int32, 10 int64, 13 bool, 14 iterator. Only kinds 1 to 3
  own memory (`FREE_RValue`). Pre-2.3 runtime: no structs or methods.
- **CInstance**: `+0` vtable (`[1]` = `InternalGetYYVarRef(slot)`, thiscall), `+4` `RValue* yyvars` (by slot, 16-byte
  stride, may be NULL; then go through the vtable), `+0x74` flags (bit 0 destroyed, bit 1 deactivated), `+0x7C`
  object_index, `+0x130` next active instance.
- **Object** (resource): `+0x44` linked list of its instances `{next, ?, CInstance*}`.
- **RFunction** table of builtins (`01B8C014`), 0x4C bytes per entry: `char name[64]`, `+0x40` fn, `+0x44` argc.
  2502 entries in this build; builtins past that (`layer_*`, `tilemap_*`) are found by name, not by index.

| VA | Function |
|---|---|
| `016AB490` | `YYGML_CallBuiltin(self, other, &ret, argc, fnIndex, RValue** args)` |
| `016CAD50` / `016CAF50` | `Variable_GetValue` / `Variable_SetValue(int instId, int slot, int arrayIndex, RValue*)` (instId >= 100000 instance, < 100000 object, -3 all) |
| `016CA140` / `016CACD0` | `Variable_GetBuiltin` / `Variable_SetBuiltin(CInstance*, varId, arrayIndex, RValue*)` (`x`, `y`, `sprite_index`; index `0x80000000` = none) |
| `004012B0` | `FREE_RValue` |
| `00409120` | `RValue::operator=` (thiscall, this = destination) |
| `016A52A0` | instance lookup by id |
| `016ABC10` | `YYSetString` |
| `016AADE0` | `YYError` (fatal GML error) |
| `016F2D00` | `event_user(self, other, n)` = `Perform_Event(self, other, 7, 10 + n)` at `017DA800` |
| `016E6BA0` | `instance_destroy` core `(self, other, id, executeEvent)`: every destroy ends here |
| `01B7CA98` | `global` instance |
| `01B8E0D0` | current room; `+0x80` first active instance |

YYC calls some builtins directly instead of through the table (`YYGML_instance_destroy` at `016EC110`), so hook the
core function, not the table entry.

## Facts about the game that shaped the design

- `PlayerFind(n)` = `ds_list_find_value(global.players, n)`, always called with 0. It is the single choke point for
  "the player", which made the actor swap possible: hook it and return the puppet while performing a remote player's
  action. It runs about 350 times per frame, so the hook must be cheap.
- State on the player instance: `inventory`, `inventorySize`, `coins`, `hp`, `invulFrames`, `energy`, `level`,
  `xpToGain`, `gear`, `tool`, `toolSelected`, `buff`, `cosmetic`, `bomb`. Global: `skillUnlocked`, `featProgress`,
  `droids`, `assignedDroids`, `ownedLands`, `zoneCurrent`, `deathCount`.
- Why a plain second `objPlayer` does not work: it reacts to the same global input, and its own Step and Draw call
  `PlayerFind(0)` and read the first player's state. Skipping its events and drawing it is what makes it a puppet.
- A hit on a resource: `objPlayer_Step_0 -> UseItem -> UsePickaxe`, which does
  `with (target) { killedByPlayer = room_speed; event_user(0) }`. A resource dies in its Step at `hp <= 0`;
  `Destroy_0` calls `GainXp` and `DropItem`.
- Death: `objPlayer/Step_1` calls `SaveGame` and creates `objGameOver` at `hp <= 0`.
- Cell sleep lists hold `[id, box]` arrays. A destroyed or woken id left on them crashed the host later, when
  `ProcessCulls` touched the stale id. Take ids off every list on destroy and on wake.
- The blue dungeon chest is the same `objTreasureChest` with another sprite and opens without a key; treating every
  `objTreasureChest` as a key chest spent the host's key and left it waiting for the guest's.
- `global.droidNames` is a ds_grid, not an array (one row per droid: name, look, colour).
- Looped sounds (`sndBombFuse`, the `objWatcher` laser) are stopped only in the object's `Destroy_0`; a replica
  removed silently keeps the sound playing forever. Stop an object's looped sounds when removing its replica.
- The archery crosshair is drawn by `objPlayer Draw_0`, so a puppet draws the remote player's crosshair unless that
  sprite is skipped while a puppet draws.
- `COOP_DEBUG_GODMODE` turns on the game's own god mode, which also lets the player walk on water: tests that move a
  player with it on put buildings over water.

## Official Modding API: what stopped the first prototype

Mod API 1.0.1, a GML interpreter inside the game. Learned while building the prototype:

- Returning to the main menu unloads all mods and wipes their `global.*`. Anything that must survive goes into the
  game's own globals (`variable_global_set`).
- A network event while mods unload crashes the game (`ref_map_wrap` in `WorldControl`). Close the session before the
  room change and delay the change.
- Never wrap the game's data structures (`variable_global_get(..., "grid")`): unloading destroys them with the mod.
- `OnStep` does not run while a menu is open; `OnDrawGUI` does.
- `instance_activate_object` wakes instances only on the next frame, so a world snapshot takes two frames.
- `instance_destroy()` without an argument is rejected; `-1` means `self`, use `noone` for an empty reference.
- `OnResourceSpawn`/`OnMobSpawn` fire before the object is fully set up (ore type); send a frame later.
- Sockets are TCP only; no `buffer_seek`, UDP or `json_encode`.
- No file access, so the host's world could not be handed to a guest: the guest had to copy the host's save by hand
  before every session and after every island purchase.

## Bugs from playtests worth knowing about

Each was found in a two-PC playtest, root-caused from both logs, fixed and given a scripted check. Full tables are in
`docs/coop/PLAYTEST*.md` (Russian) in the mod repo.

| Seen | Cause | Fix |
|---|---|---|
| Guest's sword, arrows and staffs did no damage to enemies | they hit the guest's replica; the next hp from the host overwrote it | the hp drop since the last host write is sent as a hit |
| Guest's skill points wrong | written to a variable the game never reads; `objPlayer.skills` is level - 1 - learned | write the real field |
| Museum shelves empty in co-op | interiors rebuilt from `data.win` lacked the per-instance room creation code (`gml_RoomCC_rmMuseum_*`), which YYC keeps only in the exe | find those functions by their name strings in Ghidra and call them after creating each instance |
| Host crash after many destroys | stale id on a cell sleep list | remove ids from the lists |
| Broken world saves | a zone file with a short `Entities` list | repair zone files before the world loads, writing atomically and keeping the first `.broken` copy |
| Guest's drone gone after rejoin | the guest plays in the host's world, so its own droids were saved nowhere | the host stores each guest's familiars next to its character file and restores them on join |
| Infinite fuse sound at the guest | looped sound stopped only in `Destroy_0` | see above |

## Code review findings (2026-10-25)

A review of all 13 400 lines of the DLL at the time found, among others: handlers left armed after a guest left (a
later connection could rewrite the host's slot 99), an array length taken from the network unchecked, NaN
coordinates reaching the world, non-atomic writes of the guest character file, a vectored exception handler that
could loop on a fault inside the GML stack reader, and guests that vanished without closing TCP staying in the
session forever. All were fixed; the second pass added a writer thread and queue per connection, a hello frame
before a connection counts, and validation of every value a guest sends.
