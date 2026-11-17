# The Elder Scrolls V: Skyrim Special Edition (2016)

| | |
|---|---|
| Engine | Creation Engine (Skyrim Together hooks it from its own launcher, not as an SKSE plugin) |
| Versions tested | Steam, appid `489830`, build id `24914197`, `SkyrimSE.exe` `1.7.104.0`, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f` |
| Projects | [Skyrim Together Reborn](https://www.nexusmods.com/skyrimspecialedition/mods/69993) (source: [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution)); [SkyMP](https://github.com/skyrim-multiplayer/skymp) (roleplay server platform, not campaign co-op) |
| Release | Skyrim Together Reborn 1.8.4 (2026-10-22); `dev` branch active (last push 2026-11-13) |
| QA level | community-reports (Skyrim Together Reborn as released) |
| Last updated | 2026-11-16 |

## Where things stand

You can play Skyrim SE in co-op today with Skyrim Together Reborn: 2 to 8 players on a server one
of them hosts, with the campaign shared through the party leader. It is actively developed, so the
best way to improve it is upstream pull requests, not a fork.

This write-up covers one gap found and fixed in a local branch: **player-made potions and poisons
did not sync at all** ([TiltedEvolution#435](https://github.com/tiltedphoques/TiltedEvolution/issues/435),
open since 2022). The fix is tested in game (below) but not published yet, so players still have
this bug.

## Existing work

- **Skyrim Together Reborn** (Nexus mod 69993, GPL-3.0 source in TiltedEvolution). Supports SE
  1.7.104 since 1.8.1 and 1.6.1170. Client-authoritative: the server relays, the party leader's
  client drives quests, weather and time. Last activity 2026-11.
- **SkyMP**: server-scripted roleplay platform with a TypeScript/Papyrus SDK; no shared campaign.
- Several open TiltedEvolution PRs already cover popular requests (sleep/wait time sync, trading,
  nametags, voice chat, quest stop/start fixes, item drop overhaul); check them before starting.
- [Issue #109](https://github.com/cranebeaker/coop-anything/issues/109) here claims quest sync
  work on Skyrim Together for 1.7.104.

## How the game works (for co-op)

- `SkyrimTogether.exe` loads `SkyrimSE.exe` into its own process; the game's code lives in the
  launcher's `.game` section at its original addresses, so for a stack dump the IDB address of a
  game frame is `0x140000000 + (frame - SkyrimTogether.exe base)`. Map it to an Address Library ID
  with TiltedEvolution's `Tools/Scripts/addrlib.py -v 1.7.104 <address>`.
- Items travel as `GameId` (mod + base id). Forms created at runtime (`0xFF` range: brewed
  potions and poisons, custom enchantments) have no meaning on another client. Custom
  enchantments were already sent as their effects and rebuilt; potions and poisons were not.
- `BGSCreatedObjectManager` owns created potions and poisons (verified in SkyrimSE 1.7.104 with
  Ghidra): `AddPotion` 36167 / `AddPoison` 36168 reuse an existing form when the effect list is
  identical, otherwise allocate a `0x168`-byte `AlchemyItem` and name it from the first effect.
  The forms are reference counted (`IncrementRef` 36170 / `DecrementRef` 36171, freed at 0). The
  game's own crafting (ID 22506) calls AddPotion, adds the item to the inventory, then adds one
  reference.
- Dropped items only sync with the server option `Gameplay:bEnableItemDrops=true` (off by
  default, marked experimental). With it off, no dropped item of any kind reaches other players.

## What we changed

Local branch `fix/sync-crafted-potions` on TiltedEvolution `dev` 13ff0b5, 21 files, not published.

1. **Brewed potions and poisons were dropped on every other client.** Root cause: inventory sync
   sent the local runtime id; the receiver's lookup returned nothing and logged
   `Object to add not found, FFFFFFFF:…`. Fix: inventory entries carry the potion's effects and
   poison flag; receivers rebuild it with AddPotion/AddPoison and take engine references the way
   crafting does. Entries are matched by effects, since each client gets its own form id.
2. **Drinking one broke magic sync for others** (`OnNotifyAddTarget: failed to retrieve formID`,
   `Could not find spell form id`). Fix: the add-target and spell-cast messages carry the same
   effect data; the receiver rebuilds the potion and keeps one reference per distinct potion for
   the session, so an active effect never points at a freed form.

Tests that prove it: unit tests (encoding round trips, stacking rules) and the in-game run below,
plus a control run of the same test on the unfixed build, which fails with the original error.

## QA

- Unit tests: `xmake build TPTests && xmake run TPTests`: 81 assertions in 8 test cases pass
  (2026-11-16).
- In game, automated: local server plus two clients on one PC, both sent to
  `RiverwoodSleepingGiantInn` by `coc` from the main menu (8 s after launch is enough), driven by a
  test-only harness compiled into the client (not part of the fix). With the fix: other player
  sees 3 brewed potions + 2 poisons with names; chest store/take agrees on both sides; drinking
  consumes it and applies Fortify Health (100 to 120); a dropped potion appears in the world;
  quicksave (F5) and quickload (F9) keep the received poison. Without the fix: the other player
  sees 0 potions and 0 poisons.
- Not covered: the real alchemy menu (the harness uses the engine call the menu makes), a poison
  applied to a weapon, two separate PCs.

Because the fix is unpublished, the directory entry keeps Skyrim Together's QA level at
community-reports.

## How to play

Install Skyrim Together Reborn from Nexus with its requirements (SKSE64 build for your runtime,
Address Library), launch `SkyrimTogether.exe` through your mod manager, and host the bundled
`SkyrimTogetherServer.exe` (UDP port 10578 by default).

With Mod Organizer 2: point the executable at the real file in the `mods` folder, not at the
virtual `Data` path (`ERROR_FILE_NOT_FOUND` otherwise). The launcher reads the game folder from
`HKCU\Software\TiltedPhoques\TiltedEvolution\Skyrim Special Edition` (`TitlePath`, `TitleExe`); if
that still points at the Steam install while you play a copied "Stock Game", it looks for Address
Library there and fails with "Failed to load Skyrim Address Library".

## Known issues

- Player-made potions and poisons do not sync in released versions (#435).
- On `dev`, typing `save <name>` in the console while connected freezes the game: the save stays
  a 0-byte `.ess.tmp`, the process stops responding and uses no CPU. This happens with or without
  the potion fix and with no potions involved. F5 quicksave and the pause menu save work while
  connected; a console save works after disconnecting. The blocked thread is the console script
  job (21886, then the save path 35727, waiting in `EnterCriticalSection` from 36854); the lock
  holder was not identified.
- Dropped items need `bEnableItemDrops=true` on the server.

## Next steps

- Publish the potion fix as a TiltedEvolution PR, then cover poisons applied to weapons
  (`ExtraPoison` still assumes they are never runtime forms).
- Find what holds the lock in the console-save deadlock (a full debugger or a symbolised main
  thread stack is needed; `StackWalk64` cannot unwind through the client's own code there).
- Testing tips for the next agent: Mod Organizer 2 forwards `moshortcut://` to any running MO2
  instance, so close other instances before launching a portable test instance; MO2 does not pass
  your environment to the game, so switch test code on with a file next to the exe; the game window
  is not the process's main window (the dev console is), so find it by its title; the Papyrus VM
  hook that drives the client's per-frame update does not run at the main menu.
