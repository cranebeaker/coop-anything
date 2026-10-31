# Obscure II / The Aftermath (2007)

| | |
|---|---|
| Engine | Hydravision in-house (Zoe / Noe / Hoe / TM layers) |
| Versions tested | Steam, app 254480, build 215294; `Obscure2.exe` SHA-256 `003f195b0e5bff91682f7e436cfe8213b6ca53803e17e98cdb5acd2103769354`, 3,733,504 bytes |
| Projects | [obscure2-coop](https://github.com/Gyurooo/obscure2-coop) (MIT, code only), release [v2.0](https://github.com/Gyurooo/obscure2-coop/releases/tag/v2.0) |
| QA level | automated (two real game copies on one PC, bot-driven, over a Steam-shaped test link) |
| Last updated | 2026-10-30 |

## Where things stand

Two players can start the campaign from a save and play it together online over Steam, each on
their own PC: both characters move and animate on both screens, and doors, level changes, menus,
item pickups, item use, breakable objects and cutscenes are shared. Enemies and combat against
them are not covered by tests yet, so this is a prototype, not a finished campaign co-op.

## Existing work

- [obscure2-coop](https://github.com/Gyurooo/obscure2-coop): this project, release 2.0
  (protocol 22). Research notes: [NOTES.md](NOTES.md).
- [obscure-coop](https://github.com/Gyurooo/obscure-coop): the sister project for Obscure 1. Its
  remote-character method (the game's own update driven by the partner's move intent, then the
  position put back with the game's setters) is the model 2.0 follows.
- No other Obscure II online co-op project was found (claim issue
  [#203](https://github.com/cranebeaker/coop-anything/issues/203); the searches are listed in
  NOTES.md). The retail game has no online netcode; it has local 2-player co-op.

## How the game works (for co-op)

- Hydravision engine with Zoe/Noe/Hoe/TM layers. The Steam build is wrapped by SteamStub and checks
  ownership, so the game's own app id cannot be changed.
- The game reads XInput (users 0-3). Local co-op gives a pad to each player; player directions are
  camera-relative.
- A human character's position exists three times: the position (`+0x23c`), the move target
  (`+0x27c`) that the human update (`FUN_004de4f0`) walks towards, and the collision body
  (`human+0x30c`, position at `+0xcc`) that only SetPosition (vtable slot 4, `FUN_004c6880`) moves.
- The move intent the input controller writes before the update is the direction `+0x288` (vtable
  slot 0x60) and the speed `+0x278`; the movement machine (`FUN_004e76c0`) picks the gait from the
  speed and plays two animation layers (`+0x220` upper body, `+0x224` legs).
- A one-player save stores the partner as the AI teammate (`MTHC`); patching it to `2CHC` makes the
  loaded game attach player 2. In such a world the door use runs the game's partner sweep
  (`FUN_0042ad70`), which must not find player 2 on the teammate controller.
- World state `world+0x11c`: 1 normal play, 2 inventory, 5 pause, 6 cinematic, 7 scripted event,
  8 the "found item" presentation after a pickup. `world+0x12c` is a request queue (0xc =
  ChangeLevel), not a state.
- Saves use SteamRemoteStorage; the proxy redirects them to the mod's own folder.

## What we changed

- **Transport:** UDP and Steam P2P share one network thread (receive and send every 2 ms). Before,
  Steam was pumped once per drawn frame and stopped during loads, so the partner's input arrived in
  bursts and gaps.
- **Each copy simulates only its own player.** The partner's character runs the game's own update
  with the partner's streamed move intent (Obscure 1 method), then its pose is put back with
  SetPosition so the collision body follows. Before, both copies simulated both players from pads,
  which drifted over Steam and crashed when the copies disagreed (`Obscure2.exe+0xe812b`: a swing
  with a NULL attack target on the copy whose controller never ran). Test: `check_anim_sync.py`
  (both animation layers' clip and clock, the player's own copy vs the other copy: 100 % on walk, turn, run, aim,
  aim + walk/strafe/run and swing), `check_movement.py`, `check_visible.py` (the partner is in the
  render world at its streamed pose, collision body within 8 units).
- **Doors:** a door use pulls the partner to the door and waits for its walk-through; the partner
  copy now takes part, and level changes are host-authoritative and ordered, with a self-heal if the
  two copies ever end in different levels. Test: `check_doors.py`, the soak's door/load races.
- **Freeze fixes:** the teammate controller is unlinked in normal play (door freeze); entity tables
  are keyed on the world object and re-checked before every call into the game (freezes right after
  a load); the teammate is re-linked where the game's scripted-event code needs it.
- **Found-item presentation:** leaving it on one copy leaves it on both (before, the partner's game
  stayed paused). Test: `check_pickup.py`.
- **Shared menus:** the pause menu and inventory open on both screens and either player can drive
  them. Test: `check_menus.py`.
- **Installer:** an Inno Setup installer checks the exe, backs up what it replaces, imports the
  player's Steam saves into the mod's save folder and patches them for two players. Test:
  `test_installer.sh` on fake folders.

## QA

- Tests: [obscure2-coop/tests](https://github.com/Gyurooo/obscure2-coop/tree/main/tests), described
  in [tests/README.md](https://github.com/Gyurooo/obscure2-coop/blob/main/tests/README.md). Two real
  game copies on one PC, driven through a named pipe (virtual pads, teleports, memory reads, window
  captures). `OBSCOOP_LINK=steam sh tests/run_soak.sh <dll>` runs the full soak over a link shaped
  like measured Steam traffic (75 ms round trip, jitter, bursts, short dropouts).
- Tests ran on the 2.0 source (mod repo commit `2f10469`); the packaged 2.0 DLL passed a party and
  visibility smoke run, and the installer test passed on the 2.0 setup.
- Last suite (2026-10-30): steam-link soak PASS (30 location changes plus door/load races, animation
  sync on every movement phase, both characters visible on both copies, players attacking each other
  3 times each way, doors, 0 crashes, 0 game-thread stalls); default-link soak PASS; breakables, item
  use, pickup, menus, party, items, reconnect and cutscene skip PASS.
- Not covered by tests: real enemies and combat against them (the test levels have none active),
  climbing down (no ladder in the test level), abilities, the map and documents screens, the
  installer's interactive pages, a real two-PC Steam session of 2.0.

## How to play

- Download [v2.0](https://github.com/Gyurooo/obscure2-coop/releases/tag/v2.0) and run
  `obscoop2-2.0-setup.exe` on both PCs; both players need 2.0 (older builds do not connect). The
  zip is the manual route (`install.ps1`).
- Both players must be Steam friends. Start with `start_host_steam.bat` / `start_guest_steam.bat`
  (or the Start-menu shortcuts). Both start at the main menu; the host loads a save and the guest
  follows.
- No game files are shipped; MIT.

## Known issues

- Not covered by tests: enemies, enemy damage and deaths in combat online.
- The installer refuses any exe other than Steam build 215294 (widescreen patches change it).
- Not covered by tests: `d3d9.dll` add-ons (DXVK, ReShade); the installer warns about them.

## Next steps

Enemy and combat sync in a level with active enemies, climbing and abilities through the new
movement path, the map and documents screens, a long mixed-play soak, and a real two-PC Steam
playtest of 2.0.
