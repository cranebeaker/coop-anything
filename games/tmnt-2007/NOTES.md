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
- Turtle id: obj → [+0x18] → [+4] → [+0x48] → +0x6A4 (1-4). Turtle AI struct: IO port +0x708 (0 controlled,
  -1 idle), present state +0x938 (2 active, 0 absent), health 0-100.
- `Switch_SwitchMainActor` 0x00575AE0 only moves main actor 0 and the port; the old turtle stays alive.
- `Turtle_Spawn` 0x005C1F70 (id, spawn point, offset, port): teleports and makes present; port set via 0x004E5190.
  Spawning an idle brother with port 0 puts a second turtle on screen.
- Setting a turtle's port to 1 does not stop pad/keyboard from moving it: the port is a gate, not a controller index.
  Input comes from the shared IO object 0x014F2F60 (reads via 0x00565D20, in protected/virtualized code).
- Pad manager 0x0063DD40, four XInput pad objects at mgr+0x30..+0x3C; `GamePadX360` read ~0x00646B30 calls
  `XInputGetState([this+0xEC], this+0xD8)`. Processed left stick 0x00824BE4/0x00824BE8. Pad buttons for moves go
  through the DirectInput path.
- Action ids seen on the action path: 1 jump, 8 unarmed, 6 armed, 4 team move. Dodge is not on that path.
- The game asks each button four ways (pressed now, held, a held variant, held duration). Answering only "pressed now"
  from player 2's pad left most moves broken; answering all four still leaves history-based moves (double jump,
  combos, dodge, run-up) broken.
- Exposing a second physical pad to the game: the game merges all its pads into one input state (last one wins).
- Camera target actor pointer at 0x0150A798 is rewritten by the game every frame; overwriting it made things worse.
  Shifting player 1's position to the midpoint only during the camera update and restoring it after works.
- Real `IDirect3DDevice9*` global at 0x014F5BEC (hooking a dummy device's EndScene drew nothing).
- Super Family Attack does use player 2 as partner, then the move script (obfuscated) flies it away and sets it
  absent; re-placing it and camera fixes did not help.

## Derived

- Player 2 needs its own input history, not only its own button answers: the move logic reads the shared IO object's
  history and timers. A per-player copy of that object, filled by the game's own update and swapped in while player 2
  updates, should give parity without per-move work (same shape as The Two Towers' second pad record).

## Unknown

- Store/edition of the tested copy; whether other builds match.
- How the brother bond bar is stored (unlocking all turtles from level start is not solved).
- Behaviour of player 2 across level loads and checkpoint reloads.

## Checks that actually ran

Manual runs of each mod build during development (2026-10-12 to 2026-10-14) and mod logs. No bot or scripted test yet.
