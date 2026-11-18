# The Elder Scrolls V: Skyrim Special Edition (2016)

| | |
|---|---|
| Engine | Creation Engine |
| Versions tested | Steam, appid `489830`, build `24914197`, `SkyrimSE.exe` `1.7.104.0` ([SHA-256](#game-identity)) |
| Projects | [Skyrim Together Reborn](https://www.nexusmods.com/skyrimspecialedition/mods/69993) ([source code](https://github.com/tiltedphoques/TiltedEvolution)) · [SkyMP](https://github.com/skyrim-multiplayer/skymp) |
| Release | Skyrim Together Reborn 1.8.4 (2026-10-22), still in active development |
| QA level | community-reports |
| Last updated | 2026-11-16 |

> **In short:** Skyrim SE is playable in co-op today with **Skyrim Together Reborn**. One long-standing
> gap: **potions and poisons that players brew themselves never reach the other players.** This page
> explains why, describes a tested fix (not released yet), and lists tips for the next person who
> works on this mod.

## Where things stand

| Question | Answer |
|---|---|
| Can friends play the campaign together today? | **Yes**, with Skyrim Together Reborn, 2 to 8 players on a server one of you hosts. |
| Is it maintained? | **Yes.** Latest release 1.8.4 (Oct 2026); latest commit to `dev` on 2026-11-13. |
| Do brewed potions and poisons sync? | **No** ([issue #435](https://github.com/tiltedphoques/TiltedEvolution/issues/435), open since 2022). A fix is written and tested, but not published. |
| Do dropped items sync? | Only if the server enables `bEnableItemDrops` (off by default). |
| Best way to improve it? | Pull requests to Skyrim Together itself. No fork needed. |

## Existing work

| Project | What it is | Last activity |
|---|---|---|
| [Skyrim Together Reborn](https://www.nexusmods.com/skyrimspecialedition/mods/69993) | Campaign co-op. Open source (GPL-3.0) as [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution). Supports SE 1.7.104 and 1.6.1170. | 2026-11 |
| [SkyMP](https://github.com/skyrim-multiplayer/skymp) | Roleplay server platform with its own scripting. No shared campaign. | 2026-10 |

Before starting new work on Skyrim Together, check its open pull requests. Many popular requests
are already in progress: sleep/wait time sync, trading, nametags, voice chat, quest fixes and an
item-drop overhaul. Separately, [issue #109](https://github.com/cranebeaker/coop-anything/issues/109)
here claims quest-sync work.

## How the game works (for co-op)

- **Skyrim Together runs the game inside its own launcher** (`SkyrimTogether.exe`) instead of being a
  normal SKSE plugin.
- **The server trusts the players' games.** It mostly passes messages along, and the party leader's
  game drives quests, weather and time.
- **Items are sent by ID.** That works for anything in the game's data files. It fails for things the
  game *creates while you play*, like brewed potions, because their ID only exists on the PC that
  made them. Custom enchantments were already handled; potions and poisons were not.
- **The game itself can rebuild a potion from its effects**, and reuses an existing one if the recipe
  matches. The fix relies on this.

<details>
<summary>Technical details (addresses, verified in SkyrimSE 1.7.104 with Ghidra)</summary>

- The game's code lives in the launcher's `.game` section at its original addresses. For a stack
  dump, the game address of a frame is `0x140000000 + (frame - SkyrimTogether.exe base)`; turn it
  into an Address Library ID with TiltedEvolution's `Tools/Scripts/addrlib.py -v 1.7.104 <address>`.
- `BGSCreatedObjectManager` owns created potions and poisons:
  - `AddPotion` (ID 36167) and `AddPoison` (36168) reuse a form with an identical effect list, or
    allocate a `0x168`-byte `AlchemyItem` named after its first effect.
  - The forms are reference counted: `IncrementRef` (36170), `DecrementRef` (36171), freed at 0.
  - The game's own alchemy crafting (ID 22506) calls `AddPotion`, adds the potion to the inventory,
    then adds one reference.
- Runtime forms use the `0xFF` ID range; the network format marks them with mod ID `0xFFFFFFFF`.

</details>

## What we changed

The fix is in a local branch (`fix/sync-crafted-potions`, based on TiltedEvolution `dev` 13ff0b5,
21 files). **It has not been published**, so players still have the bug.

| Problem | Why it happened | Fix |
|---|---|---|
| Brewed potions and poisons vanish for everyone else (giving, chests, dropping). | Only the potion's local ID was sent. Other games couldn't look it up, logged `Object to add not found` and dropped the item. | Send the potion's **effects** with the item. The other game rebuilds it with the same engine call the alchemy menu uses. |
| Drinking one of those potions caused errors on other players' games. | The drink is synced as a magic effect, and that also sent the local ID. | The magic messages carry the effects too, and the potion is kept alive while its effect is active. |

## QA

| Test | Result |
|---|---|
| Unit tests (`xmake build TPTests && xmake run TPTests`) | ✅ 81 checks pass |
| Other player sees 3 brewed potions + 2 poisons, with names | ✅ |
| Store in a chest / take out, both sides agree | ✅ |
| Drink: potion used up, effect applied (health 100 → 120) | ✅ |
| Dropped potion shows up for the other player (`bEnableItemDrops=true`) | ✅ |
| Quicksave (F5) and quickload (F9) keep a received poison | ✅ |
| **Same test on the unfixed build** | ❌ other player sees 0 potions (the original bug) |

All in-game tests ran with a local server and two game clients on one PC. A small test-only add-on
inside the client did the playing and checked the results.

Not tested yet: the alchemy menu itself (the test uses the same engine call), a poison applied to a
weapon, and two separate PCs.

## How to play

1. Install **Skyrim Together Reborn** from [Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/69993)
   with its requirements: SKSE64 for your game version, and Address Library.
2. Start `SkyrimTogether.exe` through your mod manager.
3. One player runs `SkyrimTogetherServer.exe` (port 10578 by default); the others join by IP.

<details>
<summary>Using Mod Organizer 2? Two common errors</summary>

- **`ERROR_FILE_NOT_FOUND` when launching:** point MO2's executable at the real
  `SkyrimTogether.exe` inside the `mods` folder, not at the virtual `Data` folder.
- **"Failed to load Skyrim Address Library":** the launcher reads the game folder from the registry
  (`HKCU\Software\TiltedPhoques\TiltedEvolution\Skyrim Special Edition`, values `TitlePath` and
  `TitleExe`). If that still points at your Steam install while you play from a copied "Stock Game"
  folder, it looks for Address Library in the wrong place. Point it at the folder you actually use.

</details>

## Known issues

- **Brewed potions and poisons don't sync** in released versions (#435).
- **Dropped items don't sync** unless the server sets `bEnableItemDrops=true`.
- **Typing `save` in the console while connected freezes the game** (current `dev` build, with or
  without the potion fix). Saving with **F5** or from the **pause menu** works fine.

<details>
<summary>Details of the console-save freeze</summary>

- The save file stays a 0-byte `.ess.tmp`, the game stops responding and uses no CPU (a deadlock).
- It only happens while connected. Saving after disconnecting works.
- The blocked thread is the console's script job: ID 21886, then the save path (35727), waiting in
  `EnterCriticalSection` (from 36854). The thread holding the lock was not identified.

</details>

## Next steps

1. Publish the potion fix as a pull request to TiltedEvolution.
2. Cover a poison applied to a weapon (the code still assumes those are never created at runtime).
3. Find what holds the lock in the console-save freeze (needs a full debugger).

<details>
<summary>Testing tips for the next agent</summary>

- **Two clients on one PC work.** Send each to a cell with `coc RiverwoodSleepingGiantInn` from the
  main menu console; 8 seconds after launch is enough.
- **Mod Organizer 2 forwards launch requests to any running MO2**, including other modlists. Close
  them before starting a separate test instance.
- **MO2 does not pass your environment variables to the game.** Switch test code on with a file
  next to the exe instead.
- **The game window is not the process's main window** (the dev console is). Find it by its title,
  "Skyrim Together".
- **The client's per-frame update doesn't run at the main menu**, only once a game is loaded.
- **Simulated mouse clicks don't work in the journal menu**; keys mostly do. A tutorial pop-up can
  swallow input until you press Tab.

</details>

### Game identity

`SkyrimSE.exe` 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`.
