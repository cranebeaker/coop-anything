# The Godfather II (2009)

| | |
|---|---|
| Engine | Visceral Engine (per PCGamingWiki; not confirmed from the binary) |
| Versions tested | `1.0.789.0` as shipped inside the SE v0.56 Nexus mod bundle - an **unverified third-party executable**, not a build to follow: run only in a disposable copy. Boots to the campaign there, menu and gameplay confirmed. Clean retail `1.0.764.0` inspected statically only |
| Projects | Multiplayer Revival (match multiplayer, not campaign) <https://www.nexusmods.com/thegodfather2/mods/95>; The Godfather II SE script extender (modding hook, not co-op) <https://www.nexusmods.com/thegodfather2/mods/40>; VisceralToolkit (asset/stream tooling, WIP) <https://github.com/Greavesy1899/VisceralToolkit> |
| QA level | none (no automated assertion suite yet; a repeatable manual boot path and the tooling to build one are in place) |
| Last updated | 2026-10-21 |

## Where things stand

Still no campaign co-op project exists for this game, and none was found in the
public sources. What changed in this round of work is the ground under such a
project: the game is now **runnable and observable** in an isolated copy, and a
small own-code plugin loads inside it through the SE extender's ASI loader, which
gives a co-op mod a supported place to hook.

Concretely: the `1.0.789.0` executable bundled with SE v0.56 (again: an
unverified third-party exe, usable only as a disposable test target) starts from
a disposable copy, reaches the main menu, loads an existing campaign profile and
**drops straight into the campaign - no Cuba intro** - and runs the mission
gameplay with HUD, NPCs and the world map. The player's world transform is read
from the engine at 5 Hz by our own plugin, and character objects and their
transform rows can be located in the live process with an external inspector.
The single open end is the authoritative transform slot of a second character,
which is what a "player 2" puppet would have to write.

Research log with every address, crash cause and dead end: [NOTES.md](NOTES.md).

## Existing work

- **Godfather II Multiplayer Revival** (<https://www.nexusmods.com/thegodfather2/mods/95>,
  by `paindubreak`, last upload 2026-08-06): restores the original online match
  modes through the community FESL/matchmaking server; play is peer-to-peer once
  a match starts. Requires build `1.0.789.0` plus an ASI loader. This is the
  game's original competitive multiplayer, not a shared story campaign, and it
  was not installed or launched here. The original EA service is shut down
  (PCGamingWiki); the revival is community reports only.
- **The Godfather II SE** (<https://www.nexusmods.com/thegodfather2/mods/40>,
  by `Greavesy`/`Greavesy899`, v0.56 last updated 2026-08-26): script
  extender/mod menu with loose-file loading, NPC/item/vehicle spawning,
  checkpoint loading, and SimGroup overrides. It ships an **unverified
  third-party game executable** of unknown build and no public source or
  license - do not treat it as a target build and do not redistribute it. Its
  bug list includes API/startup crash reports. **Its loader is the hook used
  here**: the `dinput8.dll` in the bundle is the Ultimate ASI Loader and it
  loads third-party `scripts\*.asi` plugins, so a co-op mod can ship as an ASI.
- **VisceralToolkit** (<https://github.com/Greavesy1899/VisceralToolkit>, no
  license published): inspection/editing tools for Visceral engine games. The
  wiki names Godfather II Version 12 `.str` stream files and lists resource
  types (navigation graphs, MetaModels, `VariableDictionary`), with most formats
  marked unknown. Useful as an asset reader lead, not a co-op framework.
- **Dead Space 3 co-op** (<https://www.pcgamingwiki.com/wiki/Dead_Space_3>) is
  the same named engine and shipped a two-player campaign co-op. That is an
  engine-family reference for what the engine can do; no code or tools were
  found or reused from it.

## How the game works (for co-op)

- **Assets:** three `GODFATH*.viv` container archives (magic `BIGF`) plus about
  2,300 loose version-12 `.str` stream files (magic `3slo`) covering mission
  and objective paths; no Lua/script files shipped.
- **Engine objects:** the engine keeps the player's world matrix in a global
  (`0x0112E210` in the `1.0.789.0` build) and the camera look-at/eye transforms
  in `0x0112A840` / `0x0112A850`. Characters are objects with a class vtable
  (`0x00D6B350`); their position is read through a virtual getter
  (`vtable+0x48` -> `this+0x3C0`). The engine rewrites every character's
  transform from its own kinematics each frame, so a puppet can only be steered
  in-process and after that update.
- **Scripting:** the engine exposes a script/behavior layer to the game data
  (`GetPlayerPosition`, `SetPositionValues`, `CreatePosition`, `GetPlatform`,
  movement commands such as `moveToPos`) - a sanctioned way to move a character
  that does not exist yet in any public mod.
- **DRM:** the clean retail exe is SecuROM-protected (`.securom` sections and
  `paul.dll`; PCGamingWiki lists 7.38.0015). Ghidra static decompilation of the
  SecuROM-protected exe fails (invalid flow/overlap errors); do not unpack it.
  The unverified third-party `1.0.789.0` exe from the SE bundle decompiles
  cleanly, which is why the memory addresses in the notes come from it; it is a
  disposable test target only, not a build to follow.

## What we changed

Nothing in any installation, and nothing published. Inside a **disposable copy**
only:

- three byte patches in the copy's `godfather2.exe` (each recorded in the private
  identity notes): the under-spec capability gate, the `Window.Fullscreen`
  default, and the GPU-vendor detector that dereferences a NULL `strstr` result;
- our own ASI plugin (`scripts\coopcore.asi`, own code) with five engine hooks, a
  5 Hz player-state log, a path redirect that keeps the machine's real EA Games
  profile untouched, a puppet writer and a vectored exception handler as a
  safety net;
- own tooling: an external inspector/writer for the running game, a position
  finder, a container dumper, an input/screenshot driver, an exe byte patcher and
  a D3D9 environment probe.

The machine's pre-existing profile, settings and campaign saves were verified
byte-identical against a pre-work backup, and the original installation's
executable hash is unchanged.

## QA

No automated assertions yet. What exists is a repeatable procedure and the
tooling to automate it:

1. launch the disposable copy on the host; the plugin redirects the profile
   paths into the copy, so the real ones are never written;
2. menu path: any key on the splash -> select the existing profile with a real
   mouse click -> resume the saved game -> a mission; the campaign loads directly,
   no intro;
3. `coopcore_player.txt` next to the executable then shows the live player
   position, and the external inspector can dump and compare engine objects.

The same boot was reproduced in Windows Sandbox (vGPU, networking disabled),
which is where the display-mode limitations were measured. Note that synthetic
mouse input does not reach the game's UI there (the engine reads the pointer
through Raw Input), which is why the host is the faster lane for iteration.

## How to play

Nothing to play: there is no co-op release. The internal test procedure is the
menu path above.

## Known issues

- Build identity is a moving target: the clean copy reports `1.0.764.0` while
  both the Multiplayer Revival archive and the SE extender bundle their own
  different `godfather2.exe` builds. Any patching must be re-derived per build -
  every address in the notes belongs to the SE `1.0.789.0` build only.
- The game's UI takes the mouse through Raw Input; keyboard navigation works in
  the menus, but the profile screen needs a real click.
- The engine's own in-game mouse handling is unchanged by our hooks, so a
  scripted test bot cannot drive the character with synthetic keys yet.

## Next steps

1. Find the authoritative transform of a second character. Established: it is
   not a plain float triple (a whole-heap delta search finds none), the
   `.data`/heap pairs such as `0x01163640`/`0x04303640` are render scratch
   written by the matrix multiply `FUN_00417560` (callers `FUN_00419b80`,
   `FUN_0041b9d0`, source data in TLS), and a PRESENT-time matrix write is one
   frame too late for the skeleton. The engine's script VM is the right path:
   it was located live, `GetPlayerPosition` resolves to type 6 (a function) via
   `FUN_00625ca0` + `FUN_00633cb0`, and the game's own call wrappers are
   `FUN_00468ea0`/`FUN_00468f40` with stack cleanup in `FUN_006255e0`.
   Next: call the wrapper (not the raw `FUN_00636570`) while the game's script
   update is active, and move the companion with `SetPositionValues`.
2. With a writable slot, drive a second character from the player's position -
   the first visible "player 2" puppet - and check that the engine does not
   overwrite it after the write.
3. Turn the manual boot path into an automated one (script the menu with
   injected keys, add a watchdog that asserts the campaign map is reached).
4. Then pick the co-op hook and the networking model per
   `playbook/how-coop-mods-work.md` and
   `playbook/networking-steam-spacewar.md`.
