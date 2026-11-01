# Dead Cells (2018)

| | |
|---|---|
| Engine | Haxe on HashLink (bytecode embedded in the exe), Heaps, SDL2 |
| Versions tested | Steam, game version 35 ("The End is Near"), build 2023-10-01, `deadcells.exe` SHA-256 `6778a6cf685a0f5962dd880af7e7ab6dbd848b5cd32daab6a8a8a6078534a7a1`; Windows x64; DCCM 35.14.1 |
| Projects | [Dead Cells Local Co-op](https://github.com/Osman-Mujic/DeadCellsLocalCoop) (this write-up, local couch co-op); [DeadCellsMultiplayerMod](https://github.com/vaiserYT/DeadCellsMultiplayerMod) (online, separate project) |
| QA level | none (manual playtests by the owner, no automated harness yet) |
| Last updated | 2026-10-27 |

## Where things stand

Two players on one PC with two controllers can play a run together: two real heroes with their own health, flasks,
gear, mutations, scrolls and HUD, a shared camera, down/revive, and loot doubled for the second player. The owner
played version 0.6.0 from the start of a run to the Bank and called it "fully playable from beginning to end with
minor issues". Versions 0.7.x and 0.8.0 fix the bugs from that run but were not played yet.

## Existing work

- **DeadCellsMultiplayerMod** (vaiserYT, MIT, github.com/vaiserYT/DeadCellsMultiplayerMod, last commit seen 2026-10-19):
  online host/client over TCP or Steam P2P on DCCM. The remote player is a "ghost" entity (`GhostKing : KingSkin`), not a
  real hero; no local play. Its Steam Workshop item shows "removed ... violates Steam Community & Content Guidelines"
  (checked 2026-10-23, reason unknown), as does DCCM's own Workshop item.
- **Dead Cells Core Modding (DCCM)** (github.com/dead-cells-core-modding/core, MIT): the framework both mods use. C# on
  .NET 10, hooks any HashLink function (`Hook_<Class>.<method> += (orig, self, ...) => ...`), generated proxies for every
  game class.
- The official ModTools (data paks, CastleDB, room/level scripts) cannot add a player.

## How the game works (for co-op)

- **One hero everywhere.** `Game.ME.hero` is read by menus, NPCs, pickups, traps, doors, the HUD and many items. The mod
  spawns a second `en.hero.Beheaded` and, around code that only knows "the hero", temporarily points `Game.hero` at
  player 2 ("P2's turn"). `Game.hud` / `HUD.ME` are never swapped globally: the game reaches the single minimap
  through them (swapping crashed v0.2.0 and v0.3.x).
- **Input**: `tool.Controller` reads one `GamePad` when `gc` is set, otherwise merges all pads. Player 1 keeps the boot
  controller pinned to pad 1; player 2 gets a new `Controller` pinned to pad 2. Menus take an *exclusive*
  `ControllerAccess` on the boot controller: handing a menu to player 2 must dispose that access, or player 1 stays
  frozen.
- **Haxe enum singletons**: argument-less enum values are compared by pointer in the game
  (`Controller.updateMainMode`, `ControlIcon.setAction`). Creating `new MainMode.MM_Gamepad()` from C# makes a third value
  that matches nothing and froze the button prompts. Take the real values from the game. (DCCM's C# `==` on enums
  compares by index, which hides this.)
- **Levels**: `Hero.clean()` before leaving a level, `Game.initHero` after; player 2 is cleaned and re-placed in hooks.
- **Camera**: `Viewport.update` aims at `tracked.spr`; player 1's sprite is parked at the players' midpoint during the call.
- **HUD**: a second `ui.HUD` built under P2's turn and mirrored to the right. 39 hero/skill/weapon functions write to
  `HUD.ME` (found by a bytecode scan); each runs with `HUD.ME` pointing at its owner's HUD. The minimap's fog of war is
  revealed while it draws, so it can be moved but must not be hidden.
- **Profile-wide state**: outfit and head live in `User.heroSkin` / `User.heroHeadSkin` and every hero is drawn from them;
  some counters (Diverse Deck "Watcher" charge, kill count) live in `game.data`. These need per-player handling.
- **Keys**: doors, teleporters, graves and trigger zones count keys in `game.hero.inventory` only. The mod treats keys
  as a shared key ring (`Inventory.countItemKind/hasItem/removeOne` also see the partner's keys).
- **Tools**: crashlink (bundled with DCCM) disassembles `hlboot.dat`; a full text dump of every function with named call
  targets (about 100 MB) made each bug a grep instead of a decompile. Function indexes (`f@NNNN`) are for v35.

Details, with the function index of every hooked game function, are in [NOTES.md](NOTES.md).

## What we changed

The mod: https://github.com/Osman-Mujic/DeadCellsLocalCoop (MIT). Main pieces, with the bug each one fixed:

| Area | Bug seen in playtests | Root cause | Fix |
|---|---|---|---|
| Menus for P2 | P1 frozen after P2 swapped an item | exclusive ControllerAccess never disposed | dispose, give the menu an exclusive P2 access |
| HUD | P2's damage on P1's bar, cooldown icons flickering, sound loop | hero code writes to `HUD.ME` | per-owner HUD routing |
| Deaths | run ended with P2 alive | death via `Hero.onDie` without `kill` | down/revive on both paths |
| Loot | swapped/declined items duplicated, shop items copied endlessly | copy made before the pick was decided | copy only once the pick completes; never for shop buys or dropped items |
| Reward rooms | one choice for two players | `ItemAltar.disable` turns off the whole row | other altars kept for the partner |
| Keys | P2's keycard opened nothing | key checks read `game.hero.inventory` | shared key ring |
| Doors/bosses | players split by doors and boss arenas | doors close behind "the hero" | partner brought through; L3 regroup |
| Prompts | button icons stuck on keyboard | C#-made enum value matched nothing | use the game's enum singletons |
| Minimap | explored areas never shown | fog drawn only while visible | keep drawing, move it |
| Diverse Deck, scythe | P2's skill did nothing, ghosts followed P1 | `game.hero` reads | run as the owner |
| Kills | counted twice | every living hero is notified of each death | count each mob once |
| Outfits | (found by reading code) P2 choosing an outfit would change P1's | one profile-wide outfit | per-player outfit/head saved in the mod config |

## QA

No automated test yet: `CoopTestBot.cs` is an empty stub. Everything above was checked by the owner in nine manual
playtests (two players, two controllers), with the DCCM log read after each one. That breaks this playbook's rule
"test with bots, not with your owner"; the in-game bot is the first next step.

## How to play

Install DCCM 35.14.1, then unzip the mod's release into `<game dir>\coremod\mods\`. Full steps and controls in the
mod's README. Not on Steam Workshop (DCCM code mods are installed by hand; DCCM's Workshop items were removed).

## Known issues

- Player 2 has no visible head.
- Player 2 sometimes floats after a double jump until a down-slam (no trigger found).
- Skills and items not yet checked may still act on player 1 when player 2 uses them.
- Kill-streak and no-hit challenges are shared.

## Next steps

- Build the in-game bot (`LOCALCOOP_SELFTEST=1` hook already exists): spawn both heroes, walk through doors and
  teleporters, open each menu as P2, assert P1 is never frozen and both heroes stay in one level.
- Scan the bytecode for every skill/power that reads `game.hero` (like Diverse Deck) and run them as their owner.
- Camera v2 (owner idea): centre on the midpoint and drag by each player's offset instead of a hard screen edge.
- Find the double-jump float.
