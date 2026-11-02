# Hotline Miami 2: Wrong Number (2015)

| | |
|---|---|
| Engine | GameBaker 2.0 + SilverWare (Abstraction Games): the original GML compiled to native C++ |
| Versions tested | Steam, Windows, HotlineMiami2.exe 32-bit, linked 2017-10-20, SHA-256 `56fde3b1e4bbb650e1afdfcafaa5585ee93330f035b907c0ef5528781b88fb80` |
| Projects | none yet; build from zero in progress |
| QA level | none |
| Last updated | 2026-10-20 |

## Where things stand

Nobody can play the HM2 campaign in co-op today. Reverse engineering has started; nothing is playable.

## Existing work

- [HotlineMultiplayer](https://github.com/SomewhatLiz/HotlineMultiplayer): a separate Godot remake, not a mod of the retail game.
- [Hotline Miami MULTIPLAYER mod (video)](https://www.youtube.com/watch?v=z87i6u27RSU): first game, author was looking for netcode help; build link not checked.
- [hlm2-wad-extract](https://git.foosoft.net/alex/hlm2-wad-extract): 2015 tool for the `.wad` archives.

## How the game works (for co-op)

- No GameMaker runner: UndertaleModTool-style tools do not apply. Native 32-bit x86 exe with D3D9 or OpenGL (SDL2), FMOD audio, Steamworks for achievements, leaderboards and Workshop only. No netcode.
- Game tables are in `hlm2_data_desktop.wad` ("AGAR" archive, format in NOTES.md): `GL/hlm2_objects.bin` (2414 objects), `GL/hlm2_rooms.bin` (321 rooms), sprites, collision events.
- At startup the exe registers each object id into a 16-byte global handle (`push id; call lookup; mov ecx,<handle>; call store`). Searching for a handle address finds the code for that object; searching for small ids directly mostly hits MSVC SEH state writes.
- Best first co-op target: the Alex & Ash (Swan) levels, which already run two player objects (`objPlayerSwanChainsaw`, `objPlayerSwanGun`), one driven by AI.

## What we changed

Nothing yet.

## QA

None yet.

## How to play

Not playable yet.

## Known issues

n/a

## Next steps

See NOTES.md "Next steps": label the Swan objects' events with a debugger, find the instance struct, take over the AI partner with a second local controller through a proxy DLL, then add Steam relay networking.
