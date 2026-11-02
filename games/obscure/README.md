# Obscure (2004)

| | |
|---|---|
| Engine | Hydravision in-house (`.hvp` archives, `.zwo` models, `.hoe` scripts); not RenderWare |
| Versions tested | Steam app 254460, build 215286, `Obscure.exe` SHA-256 `7107439eb6ee1b20cad32e87f7a10a3a95b71c0b5ee26b1ec9139b263577a0f2` |
| Projects | [obscure-coop](https://github.com/Gyurooo/obscure-coop) (MIT; proxy `xinput1_3.dll`, source and bot tests); directory entry "Obscure / obscure-coop" |
| QA level | automated for builds up to 1.0 (bot tests with two real game copies on one PC); builds 1.1 to 1.8 were not bot-tested, only checked by screenshots, measurements and playtests over Steam (see QA) |
| Last updated | 2026-10-25 |

## Where things stand

Two players on two PCs play the story together over Steam with build 1.11. Playtests over Steam
reported joining, moving, fighting, doors, items, weapons, deaths and loading a save working.
Over the internet, characters and enemies are smoothed. Optional graphics: in-game supersampling,
16x filtering, a tuned ReShade setup, and 16:9 widescreen (no side bars, wider view).

## Existing work

- No online co-op mod was found before this one (search list in `NOTES.md`, section 2).
- The game has 2-player **local** co-op on one screen: player 2 presses Start on a second controller.
- Steam Remote Play Together and Parsec work by player reports (they stream the host's screen; the
  friend needs no copy of the game). Links in `NOTES.md`.

## How the game works (for co-op)

- No netcode in the exe. One global game object `*(0x6ab8e4)` holds:
  - two real-player slots (`+0x4fc`, `+0x500`)
  - the story flags (161 bytes at `*(+0xa24)`)
  - the level manager (`+0x510`: humans, enemies and items lists)
  - the state machine: `+4` = 2 in play; `+8` = 5 change level (name `+0x590`, entry `+0x5d0`), 3 load a save (slot `+0x838`)
- Characters (`CPlayerHuman`, vtable `0x6449e4`): position `+0x20`, direction `+0x2c`, health `+0x54`,
  state `+0x60`, requested state `+0x230`, weapons in a `CPlayerInventory` at `+0x128`.
- **Deaths:** when player 1 dies with no free character left, `FUN_0044c2b0` swaps the two real
  players, so the survivor becomes player 1. Only player 1's device opens the pause menu.
- **Saves:** `SteamRemoteStorage` files. The load menu calls `FUN_004524d0(slot)` on the save manager
  `*(game+0xa20)`, then sets `+0x838` and `+8 = 3`. That is the only place `+8 = 3` is written.
- **Rendering:** Direct3D 9, renderer `*(0x6ab890)`. The screen rect comes from `GetClientRect` when
  windowed, else from the mode size. Bitmap fonts are drawn 1:1 in pixels by `FUN_0045ba60`. The mip
  filter is NONE, so only mip 0 is ever sampled.
- **Input:** XInput through `xinput1_3.dll` (pads), DirectInput 8 (keyboard, menus).
- **Startup checks:** mutex `obscure_lock`, `IsDebuggerPresent`, Steam must run. The SteamStub
  wrapper is left untouched.
- Offsets are for the build above only. Full list with evidence: `NOTES.md`.

## What we changed

A proxy `xinput1_3.dll` next to `Obscure.exe` (our code only; no game files changed or shipped).
Source: [obscure-coop](https://github.com/Gyurooo/obscure-coop).

- **Transport:** Steam P2P (the game's own app id, friends only), with a UDP mode for one-PC tests.
  The friend's controller becomes the host's virtual pad 2, so the game's own local co-op drives
  player 2.
- **Host-authoritative mirroring** to the friend's copy: characters, enemies, items, doors (with
  their animation), story flags, level changes, weapons, the team bag, the Give action, inventory
  menus, pickup pictures and messages.
- **Attacks:** one-frame requests are recorded as events on the host and replayed in order on the guest.
- **Deaths:** the host follows the real-player object that joined as player 2, not the slot (the
  0.7 crash: after the death swap the guest was switched onto the dead character).
- **Loading a save:** the host notices `+8 == 3` and sends a load packet naming the save transfer.
  The guest waits for that file in its co-op slot, then runs the same two steps for slot 0. A guest
  joining also loads the host's save by itself.
- **Pause after a death:** a dead host is player 2 after the swap, and its START asks "Player 2
  quits the party". The host gets the pause menu instead, opened the way the main loop opens it,
  3 s after a death or with F1.
- **Smoothing (playtest report: micro-stutters over the internet):**
  - Packets carry the host's send time, and the guest shows characters and enemies at host time
    minus an adaptive delay (p90 jitter + one send interval), blended between snapshots.
  - Sends run on a fixed 60 Hz tick; at about 275 fps the old per-frame rates meant about 600
    packets/s.
- **Door loop:** the guest forced a door state every frame, which logged 41,509 swallowed level
  changes in 2 minutes and probably hid a character. It is now forced once per door use.
- **Supersampling (`SSAA`):**
  - The back buffer is 2x or 4x bigger; `Present` shrinks it with `StretchRect` LINEAR into an
    extra window-sized swap chain.
  - The game's `GetClientRect` answers bigger, so its layout follows. Fullscreen becomes a
    borderless window on the windowed path.
- **Text size with SSAA (`TEXT_SCALE`):** while `FUN_0045ba60` draws a string, its glyph and box
  quads (renderer `+0xb8`/`+0xc0`) are scaled about the string's anchor.
- **Filtering (`ANISO`):** `MINFILTER` LINEAR becomes ANISOTROPIC; about +4% detail on angled
  ground (measured).
- **16:9 widescreen (`WIDESCREEN`, 1.8):** in fullscreen the game drew a pillarboxed 4:3 picture
  (viewport 1920x1440 at x = 320 of 2560x1440). The renderer's keep-4:3 default (`mov [esi+0x298], 1` at
  `0x5ac1db`) is cleared, and the camera's set-FOV function (`FUN_00590230`) is hooked to keep the 4:3
  vertical view and widen the horizontal one, so the game's culling follows (no pop-in at the edges). The
  main menu logo and background are stretched wide at 16:9.
- **Movable objects (1.10)** and **breakable windows and vending machines (1.11):** the host's physics decides the
  position of chairs, wheeled objects and basketballs; the host sends which windows and machines are broken and the
  friend's game runs the same break function (flying pieces are not synced). Tests in `tests/`.
- **Watchdog:** if the main loop stops for 5 s, the log records where.

Tried and dropped: a mip LOD bias (no effect: mip filter NONE), and 2x font textures (the swap keyed
by texture address hit a reused address and drew letters on the ground).

## QA

Tests drive two real copies on one PC (host + guest over UDP) through a named pipe in the DLL
(virtual pads, keys, test commands) and read both processes' memory; they assert on game state.
Scripts: `tests/` in [obscure-coop](https://github.com/Gyurooo/obscure-coop); run with
`sh tests/suite.sh`. Every run fingerprints the player's real install and saves before and after.

Last results:

| Test | Build | Result |
|---|---|---|
| `load_test.py --menu` (death, the pause menu appears on the host, LOAD > 01 > YES with the host's pad, the guest reloads, dead character alive on both, P2 back, remote pad moves it; twice) | 1.0 | 21/21 |
| `load_test.py` (the same with the `loadslot` test command; the guest also loaded the save by itself at join) | 1.0 | 19/19 |
| `death_test.py` (P1 dies, the host swaps players, the guest keeps its character, no crash) | 1.0 | 7/7 |
| `camera_test.py` (the host's camera stays on the host's player) | 1.0 | 3/3 |
| `give`, `bag`, `door`, `p2door`, `stall` | 0.5 | 7/7, 9/9, 5/5, 7/7, 4/4 |

Earlier builds: mirroring, levels, enemies, items, combat, join, per-player inventory, view, message
and attack tests, all recorded in `NOTES.md`.

**Builds 1.1 to 1.8 were not bot-tested in co-op;** 1.10 and 1.11 added `props`, `break` and `sbreak` tests. 1.1 to 1.7 (smoothing, door loop, graphics, text size,
enemy smoothing) were checked by screenshots and measurements in test copies and by playtests over Steam.
1.8 (widescreen) was checked by back-buffer screenshots at fullscreen 2560x1440 and a playtest; `death` and
`camera` passed on its first widescreen build only.

## How to play

Windows download: [obscure-coop 1.11](https://github.com/Gyurooo/obscure-coop/releases/tag/v1.11). See the
[obscure-coop README](https://github.com/Gyurooo/obscure-coop#play). In short:

- **Friend:** drops `xinput1_3.dll` and `obscoop.ini` next to `Obscure.exe` and starts the game from
  Steam.
- **Host:** starts a copy of the game folder with `start_host.bat`.
- The host's save loads on both, and the friend presses START to join.
- Both need the Steam version (build above) and the same mod version.

## Known issues

- On the friend's screen, a character using a ladder, rack or lock does not animate (the result is
  still mirrored).
- In-game controller settings must be Player 1 = Game controller 1, Player 2 = Game controller 2,
  or the controls swap.
- With ReShade and `SSAA`, ReShade needs `ReShade2.ini` (the mod presents through an extra swap
  chain) and `DepthCopyBeforeClears=1` for ambient occlusion. Both are in the repo's `graphics/`.
- Two players only (the game has two player slots).

## Next steps

- Mirror ladder, rack and lock interactions by sending the target object's (type, id) with the event
  (states and classes in `NOTES.md`).
- Run the lag and loss test (`tests/chaos_run.sh`, `playbook/network-chaos-testing.md`).
- Run the full bot suite on the current build.
- Obscure II (Steam app 254480) is a separate target; the engine is related, but none of it is checked.
