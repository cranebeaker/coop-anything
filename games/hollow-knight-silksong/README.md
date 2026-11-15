# Hollow Knight: Silksong (2025)

| | |
|---|---|
| Engine | Unity 6000.0.50f1 (Mono), BepInEx 5 mods |
| Versions tested | Steam build `22479045`, game version 1.0.30000, Windows 11 (exe SHA-256 `5957b04989de6e242938f5ee0fc745a5b1af42da8cca44b0e6f8daf30d836496`) |
| Projects | [Silksong Co-op (Nirok85)](https://github.com/Nirok85/silksong-coop-mod), [Multiple Hornets](https://github.com/SiroxCW/Multiple-Hornets), [Silksong Together](https://github.com/Ceplargo/SilksongTogether), [SSMP](https://github.com/Extremelyd1/SSMP), [HK2SS Two Player](https://github.com/gjt-x/silksong-knight-coop), SilklessCoop (Nek5); all in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated, for the couch setup below (shared wallet, players in different rooms, input per player) on one PC |
| Last updated | 2026-11-15 |

## Where things stand

Several online co-op mods exist, and two Hornets can play side by side on **one PC** today: one copy of the game
per player, joined by the Silksong Co-op mod over UDP on the same PC, with
[Couchlink](https://github.com/cranebeaker/couchlink) placing each copy on its own monitor (or half a screen) and
giving each copy only its own controller. Automated runs with two real game clients confirmed a shared wallet in
both directions and players in different rooms staying connected. Story, enemies, bosses and world objects in this
setup were not tested here.

## Existing work

Checked 2026-11-15 from each project's README, releases and (where named) source.

| Project | Model | Shared | Own per player | Notes |
|---|---|---|---|---|
| [Silksong Co-op](https://github.com/Nirok85/silksong-coop-mod) (Nirok85, MIT) | One game per player, Steam P2P or UDP room code | Rosaries and shell shards (one wallet, `Hooks/CurrencyHooks.cs`), upgrades, unlocks | Health, silk, equipped gear | v3.3.7 stable, v3.3.9 betas (2026-11). No auto host or join. |
| [Multiple Hornets](https://github.com/SiroxCW/Multiple-Hornets) (SiroxCW, GPL-3.0) | One game per player, direct IP (LiteNetLib) | Progression, world, bosses, wishes, maps | Enemy drops and repeatable shop purchases (so rosaries) | Host decides what syncs. |
| [Silksong Together](https://github.com/Ceplargo/SilksongTogether) (Ceplargo, SSMP fork) | One shared save, Steam invites only | Story, world, quests | Health, silk, crest, tools, rosaries | Act 1 tested per its README; claim issue #300. |
| [SSMP](https://github.com/Extremelyd1/SSMP) (Extremelyd1, LGPL-2.1) | Each player in their own world | Little | Save | Steam, matchmaking with hole punching, direct IP 26960. Base for add-ons. |
| [HK2SS Two Player](https://github.com/gjt-x/silksong-knight-coop) (gjt-x, MIT) | Same game, one camera | not read | not read | Player 2 is the Knight from Hollow Knight (assets built from the player's own HK install). |
| SilklessCoop (Nek5) | One game per player, Steam | Nothing beyond seeing each other | Save | Already in the directory, not rechecked. |

Not opened: [ArtichaTM/SilksongCoop](https://github.com/ArtichaTM/SilksongCoop) (no license),
[SSMPEnemySync](https://github.com/faarisaahmed/SSMPEnemySync), [CoopMinimap](https://github.com/Brizna-Soft/CoopMinimap),
[KIS_X_SSMP](https://github.com/BlackDiamond1503/KIS_X_SSMP), [Silksong-Steam-multiplayer-mod](https://github.com/233XvX233/Silksong-Steam-multiplayer-mod).

## How the game works (for co-op)

Read from `Assembly-CSharp.dll` with Mono.Cecil (IL only, nothing decompiled to disk):

- **Save folder.** `DesktopPlatform.Awake` sets `saveDirPath = Application.persistentDataPath/<platform user id>` and
  builds `roamingSharedData = new JsonSharedData(saveDirPath, "shared.dat", true)` and
  `DesktopSaveRestoreHandler(saveDirPath)` (restore points). Slot files come from `DesktopPlatform.GetSaveSlotPath`
  (`userN.dat`, plus versioned backups `userN_<version>.dat`). A Harmony postfix on `DesktopPlatform.Awake` that
  rewrites those three fields moves every save of a copy to a folder of your choice. Two copies started under one
  Steam account otherwise write the same folder, which Steam Cloud also syncs.
- **Options live in the registry.** `localSharedData` is `PlayerPrefsSharedData`, so options and the "language was
  chosen" flag (`GameLangSet`, read by `StartManager.CheckIsLanguageSet`) go to
  `HKCU\Software\Team Cherry\Hollow Knight Silksong`, shared by every copy and by the normal game. Without that flag
  a copy stops at the language screen. Replacing `localSharedData` with a `JsonSharedData` in the copy's save folder
  keeps each copy's options to itself. Unity's own `Screenmanager ...` window keys still land in that registry key.
- **Loading a slot from code:** `GameManager.LoadGameFromUI(int slot)` from `Menu_Title` (the first scenes are
  `Pre_Menu_Loader`, `Pre_Menu_Intro`). The loaded hero sits on a bench and has `acceptingInput == false` until a
  button is pressed, so do not wait for input readiness after a load.
- **Room exits:** `TransitionPoint` (`targetScene`, `entryPoint`, `isADoor`); calling its private
  `DoSceneTransition(true)` moves the hero through the exit the way walking into it does. A bare
  `GameManager.BeginSceneTransition` without `HeroLeaveDirection` leaves an idle hero on the return gate, and it
  bounces between the two rooms.
- **Currency:** `CurrencyManager.AddCurrency / TakeCurrency(int, CurrencyType.Money|Shard, bool)` queue a change;
  `PlayerData.geo` and `PlayerData.ShellShards` hold the totals. Shell shards are capped (700 on the save tested), so
  a test that adds shards to a full pouch sees no change.
- **Input:** InControl (inside `Assembly-CSharp`), with `SharpDX.DirectInput.dll` next to it; a Couchlink recipe
  should filter XInput and DirectInput. Copies keep running unfocused with `Application.runInBackground = true`.
- Unity's `-monitor N` argument did not move the window to the second monitor (both copies opened on the primary).

## What we changed

**SilkCouch**, a small BepInEx 5 plugin (C# 5, compiled with the .NET Framework `csc.exe` that ships with Windows,
against the game's own assemblies) next to Silksong Co-op 3.3.7. Command-line driven, so a Couchlink recipe can set it
per seat:

- seat 0 calls the mod's own `CoopMenuUI.HostUdpSession()` and writes the room code (with its 80-bit secret) to a
  file; seat 1 reads it and calls `JoinUdpSession(code)` (ignores a code older than its own start);
- per-copy save and options folder (above), filled once from a copy of the player's saves; player 2 copies player 1's
  slot the first time, so player 2 starts as a copy of player 1;
- loads the chosen slot by itself, keeps running unfocused;
- test bot: changes the wallet, leaves the room through an exit, logs JSON lines per second (scene, wallet, position,
  connection, controllers the copy sees, button presses).

Its source is not in a public repository yet; the owner decides whether to publish it.

The couch setup is a Couchlink recipe: `players: [2, 2]`, `steam: shared`, `seat_args` for host and client,
`input: {xinput: true, dinput: true}`, the modded folder as the game folder (`games.json`). No change to the co-op
mod itself.

## QA

Two real game clients on one PC, driven by bots, no people. Tests:

- a pair runner that starts two copies directly, and a Couchlink session driver (stdlib WebSocket client to
  `couchlinkd --dev --virtual-pads 2`, `COUCHLINK_STATE_DIR` inside the workspace) that seats two virtual pads, sets the
  screens, starts the session, waits for the bots and stops it;
- `check_pair.py` asserts: both copies reach gameplay and connect; host +37 rosaries reaches the client; client +23
  reaches the host; after the client walked into another room, host +5 rosaries and -3 shell shards reach it; final
  wallet equal on both and equal to start + all changes, never above it (no doubling); client stays 10 s in a room the
  host never entered, still connected;
- `check_presses.py`: seat 1's pad pressed 3 times, seat 0's pad 2 times; each copy must see exactly its own presses.

Results, 2026-11-15, Steam build 22479045, Silksong Co-op 3.3.7, Couchlink 0.1.21:

| Run | Setup | Result |
|---|---|---|
| pair3 | two copies started directly | 13/14 (shards unchanged on a full pouch: test fault, bot then spends shards) |
| cl1 | Couchlink, both players split on the second monitor | 14/14 |
| cl2 | Couchlink, one player per monitor | 14/14, presses 2/2 and 3/3, each copy sees one "Xbox Controller" |

```
PASS P1 wallet = start + all three additions  (16875 vs 16875)
PASS P2 wallet = start + all three additions  (16875 vs 16875)
PASS shell shards shared (P1 spent 3)  (P1 697, P2 697, expected 697)
PASS P2 left for another room and stayed there 10 s  (Peak_01)
PASS P1 stayed in its own room meanwhile  (P1 rooms ['Slab_06'], P2 room Peak_01)
PASS still connected in different rooms
14/14 checks passed
PASS player 1's copy got exactly player 1's 2 presses
PASS player 2's copy got exactly player 2's 3 presses
```

The wallet change showed on the other copy within about one second (each copy logs its own clock, so this is derived).
Mod log on this build: `Compatibility: game 1.0.30000, Unity 6000.0.50f1-uum-100966-branch1, mod 3.3.7, protocol v10`,
`Harmony hooks applied: 7 class(es), 0 failed`; "Steamworks is not initialized" for copies started outside Steam (UDP
still works).

Not covered: real controllers (virtual pads go through the same Couchlink hook), keyboard and mouse players, enemies,
bosses, world objects, deaths and revives, long sessions.

## How to play

1. Copy the game folder and install Silksong Co-op 3.3.7 (`SilksongCoop_v3.3.7.zip`, SHA-256
   `c3f78a569d5ab1b63804bf4ef1b2d39b55078f15817002ca57559db7132c0ff9`, includes BepInEx 5.4.21) into the copy; put
   `steam_appid.txt` (`1030300`) next to the exe.
2. Couchlink 0.1.21 (`Couchlink-0.1.21-windows-x64.zip`, SHA-256
   `ccaab9d3feb0a6c49a4533fa1f68cb5563bc33192daa0bedae7f9f4e3f372ee0`): add the recipe, point it at the modded copy.
3. Each player presses A, drag players to monitors, Play. Without SilkCouch, host and join from the mod's F10 menu in
   each copy (UDP room code).

## Known issues

- Without a per-copy options folder, the copies write the game's options to the registry key the normal game uses;
  undo it after playing.
- From the co-op mod's README: some scripted doors and boss phases can differ between players; 3 or more players over
  Steam are experimental, UDP takes two.

## Next steps

- Run the couch setup with real controllers and a keyboard player.
- Bot checks for shared upgrades and unlocks, enemies and a boss fight in the couch setup.
- Publish SilkCouch (or upstream an auto-join option to Silksong Co-op) and send the recipe to Couchlink.
