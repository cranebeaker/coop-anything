# Playing Pitfall: The Lost Expedition "ghost co-op" on two PCs (experimental)

What you get: each of you plays your own game, and the other player's Harry runs around in it
as a second Harry, with the game's own run animation, following where they really are. What you
do not get yet: shared enemies, items, puzzles or progress (each world is separate), any level
other than Crash Site, or a camera that follows both. Tested on one Windows PC with two PCSX2
copies and a simulated bad network; two real PCs over the internet are untested.

## What each PC needs

- Windows, [PCSX2 v2.8.2](https://github.com/PCSX2/pcsx2/releases/tag/v2.8.2).
- Your own dump of Pitfall: The Lost Expedition, PS2 NTSC-U, serial `SLUS-20408`, CRC `394F402E`
  (PCSX2 shows both in the game's Properties), and your own PS2 BIOS. Nothing here includes or
  links game files.
- [Python 3.13](https://www.python.org/downloads/) (no extra packages).
- The files from `tools/` in this folder: `ptle_peer.py`, `ptle_bridge.py`, `pcsx2_keys.py`,
  `pine_check.py`, `spawn_patch.py`, `mips.py`.
- A way for the two PCs to reach each other: [Tailscale](https://tailscale.com/) (free for
  personal use) on both, or a forwarded UDP port on the host's router.
- A memory-card save in the Crash Site level (the first jungle area after the prologue).

## One-time PCSX2 setup (both PCs)

1. Settings > Advanced: enable PINE, slot `28011`.
2. Settings > Controllers > Controller Port 2: DualShock 2, bound to keyboard keys
   Left stick up/down/left/right = `U` / `N` / `B` / `M`, Cross = `Z`, Square = `X`,
   Circle = `C`, Triangle = `V`, L1 = `Y`, R1 = `R`. Leave Start and Select unbound.
   The peer presses these keys in the PCSX2 window to move the other player's Harry, so do not
   use these keys yourself.
3. Make the patch file and put it in PCSX2's `patches` folder (in a normal install,
   `%USERPROFILE%\Documents\PCSX2\patches`):

   ```
   python -I spawn_patch.py pnach "%USERPROFILE%\Documents\PCSX2\patches\SLUS-20408_394F402E.pnach"
   ```

   Patches must be enabled (Settings > Emulation, default on). With this file PCSX2 turns off
   its own bundled patches for this game and says so in its log.

## Each time you play

1. Both: start the game, LOAD GAME, load the Crash Site save. A second Harry appears next to
   yours. He ignores your controller.
2. Both: connect Tailscale and note the host's Tailscale IP (`100.x.y.z`).
3. Host (one of you), in the folder with the tools:

   ```
   python -I ptle_peer.py --code jungle --listen 47100
   ```

   Windows may ask whether Python may use the network; allow it on private networks.
4. The other player:

   ```
   python -I ptle_peer.py --code jungle --connect 100.x.y.z:47100
   ```

   Use the same `--code` word on both sides; packets with another code are ignored.
5. Each window prints `steering` once a second when connected. Walk around: your Harry shows up
   in the other game. Stop with Ctrl+C (it lets go of the puppet's keys).

## Known problems

- Only Crash Site: the second Harry is spawned at a fixed spot in that level. Loading another
  level spawns him there too, which is wrong (he may fall and the level may restart).
- Each world is separate: enemies, items, doors and progress are not shared.
- The second Harry copies Cross/Square/Circle/Triangle/L1/R1 presses, but jumps and attacks were
  not checked on screen.
- The other player's Harry trails behind by a few units while running and stops about one unit
  away. If he gets stuck or more than 6 units off, he is moved into place.
- If one of you pauses, changes level or quits, the second Harry stands still until both are in
  the same level again.
