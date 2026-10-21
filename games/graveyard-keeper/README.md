# Graveyard Keeper (2018)

| | |
|---|---|
| Engine | Unity 2020.3.17, Mono (BepInEx 5 + Harmony) |
| Versions tested | Steam, Windows, build 22583570, base game without DLC |
| Projects | [Back From The Grave](https://github.com/ZlordHUN/GYK-Back-From-The-Grave), [graveyard-keeper-multiplayer](https://github.com/Zonda001/graveyard-keeper-multiplayer); both in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated (Back From The Grave 1.2.0 Beta plus three fixes; graveyard-keeper-multiplayer not tested here) |
| Last updated | 2026-10-20 |

## Where things stand

Two players can share one Graveyard Keeper world with Back From The Grave (BFTG): one host, one client, over Steam. Our two-client suite (join, shared intro cutscene, movement, sleep, eating from a quick slot, ping) passes 9/9, but only with three fixes we sent to the mod as pull requests. The released 1.2.0 Beta still has those bugs. The one players notice most: a client can't eat or drink anything from their own inventory ([issue #22](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/issues/22)).

## Existing work

Searched on 2026-10-20: web search (English and Russian phrases), GitHub repository search, this hub's directory, issues and PRs. Not searched yet: the Nexus mod pages and their bug tabs, the Steam guide, Reddit, the authors' Discords.

- **[Back From The Grave](https://github.com/ZlordHUN/GYK-Back-From-The-Grave)** (ZlordHUN, MIT). Latest release 1.2.0 Beta (2026-09-04), last commit 2026-09-19. About 330 C# files. Host-authoritative over Steam lobbies and P2P, plus LAN discovery. Its README lists shared time, weather, quests, technology, crafting, world objects, combat, drops, zones, workers, dungeons, fishing, dialogue and cutscenes, plus the Stranger Sins, Game of Crone and Better Save Soul DLCs. Each player keeps their own inventory. A `MaxPlayers` setting exists, but the README says only 2 players were tested. Open issues on 2026-10-20: #24 (~400 ms ping), #23 (inviting a player who is already in a lobby breaks the UI), #22 (client can't use food), #21 (client energy after sleep), #20 (morgue counter), #11 (cutscene bug).
- **[graveyard-keeper-multiplayer](https://github.com/Zonda001/graveyard-keeper-multiplayer)** (Zonda001, MIT). Latest build v0.1.5-test2 (2026-08-30, pre-release), last commit 2026-09-11. Mostly two large C# files. A different model: the second player has a personal save and their own quest journal over the host's world ("Stardew model"). Its README lists movement, time, chopping, foraging, garden beds, graves, corpse carrying, building, crafting, shared chests and weather. NPC villagers and dungeon mobs are not synced yet. Open issues: a client desync with NullReferenceException spam, a host soft lock, and large-item duplication. The repo ships `Steamworks.NET.dll` and a 14 KB `Stitch.dll` in `Multiplayer/lib`. Zonda001 also wrote the reliable transport (PR #9) and a spawn-offset fix (PR #10) in BFTG.
- Not co-op projects: `maxcraft2222/Graveyard-Keeper-Multiplayer` (a README and a licence, no code or releases), `edfariamaia/GraveyardKeeper-multiplayer` (9 commits, no licence, last push 2026-06). GitHub search also returns "trainer free download" repos; treat those as malware bait.
- Graveyard Keeper 2 has its own co-op projects. They are a different game and are not covered here.

## How the game works (for co-op)

- **Engine:** Unity 2020.3.17 with the Mono backend, so the usual Unity route works: BepInEx 5 x64, Harmony, and decompiling `Assembly-CSharp.dll` with ILSpy. The game uses Steamworks.NET 11.0.0 (Steamworks SDK 1.42), Rewired for input, an NGUI-style GUI (`BaseGUI`, `GUIElements.me`), and `MainGame.me` for the running game. There is no anti-cheat and no netcode.
- **Saves:** `PlatformSpecific.GetSaveFolder()`, which resolves to `%USERPROFILE%\AppData\LocalLow\Lazy Bear Games\Graveyard Keeper`. Patching that one method gives each test copy its own save folder.
- **Second copy on one PC:** `Graveyard Keeper_Data\boot.config` has a `single-instance=` line, and with it a second copy exits at once. Remove the line in test copies only.
- **Start-up:** the input layer (Rewired/`LazyInput`) and the main menu GUI are not ready while the preloader is up. Mod UI built before the main menu has been shown throws `NullReferenceException`. That is bug B1 below.
- **After the intro** the bed refuses with "Can't do that right now." The single-player game does the same, so it is not a mod bug. Tests that need sleep open `SleepGUI` directly, the way `Flow_CharSleep` does.
- **BFTG source layout:** the csproj compiles the files in the `src/**` subfolders. The flat copies in `src/*/*.cs` are excluded from the build, so a fix there does nothing. The repo also has a committed `bin/Debug` DLL; build from source instead. `GamePath` in the csproj defaults to the Steam install. Pass `-p:GamePath=<test copy>` so the build reads a test copy instead.
- **BFTG networking:** Steam lobby metadata (`mod_version`, `protocol_revision`) plus a compatibility hello (`Op.Hello`) sent on native Steam reliable decide whether a joiner is admitted. After that, traffic moves to RNET, the mod's own reliable layer over P2P. Inventories: each client sends the host a snapshot that includes its own player (`player:local`), and the host approves the client's use of personal consumables against that copy.

## What we changed

Three fixes to BFTG 1.2.0 (commit 432b1d0), each with a test that fails without it. They are sent upstream as pull requests: B1 [#25](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/25), B2 [#26](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/26), B3 [#27](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/27).

1. **B1: an invite received while the game is loading is lost.** If the client gets an invite before it reaches the main menu, building or opening `JoinGameGUI` throws a `NullReferenceException`, and the P2P invite sent with it ends with "Error parsing invite". Neither is retried. Fix: `SteamLobbyManager.IsInviteUiReady()` (a game is running, or the main menu has been shown at least once and no loading screen is up). Until then the Steam invite stays pending and the P2P invite is deferred. The first version checked "main menu visible now". That blocked normal invites, because the join and lobby screens hide the main menu, and `move_sync` caught it. Test: `invite_during_load`. PR [#25](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/25).
2. **B2: the client's Ready never reaches the host.** The joiner sends its compatibility hello on native Steam reliable. If its lobby metadata validates it first, the host treats the late hello as application traffic, `ReliableTransport.AcceptNativeInbound` locks the route to native, and the joiner, already on RNET, drops everything the host sends. Fix: the host always consumes `Op.Hello` from a lobby member in `SteamP2PManager.HandleIncomingPacket` (the check is idempotent). Our mock always delivers lobby data first, so it failed on every join. Whether real Steam hits it depends on timing; not observed there. Test: `join_newgame`. PR [#26](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/26).
3. **B3: a client can't use food or potions from their own inventory (issue #22).** The host approves a client's use against the `player:local` entry of the client's inventory snapshot, but `InventorySync.ShouldSyncWgo` rejected every player object, so that entry was never sent. Fix: `ShouldSyncWgo` lets the local player through on clients only. Test: `quickslot_food`. PR [#27](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/27).

Bug reports we could not reproduce:

- **#21 (client energy after sleep):** `sleep_energy` and `sleep_energy_client_first` pass. Energy starts rising 1.4 to 1.7 s after both players lie down, both wake at 100/100, and the clocks agree to within 0.0004 of a day. These runs had no added lag or loss.
- **#24 (~400 ms ping):** the mod's ping is 12 to 27 ms on localhost (one or two frames) and 213 ms with 100 ms added each way, so the mod adds about one frame over the round trip. The cause is probably outside the mod or depends on load; not confirmed.

## QA

The harness ("CoopQA") is a test-only BepInEx plugin plus two Python scripts. It runs a host and a client as two real copies of the game on one PC, with one Steam account.

- **Steam mock:** Harmony prefixes replace the Steamworks.NET calls the mod makes (`SteamUser`, `SteamFriends`, `SteamMatchmaking`, `SteamNetworking` P2P, avatars) and `Callback<T>`/`CallResult<T>` registration. Lobbies and P2P go over localhost TCP. How it works and what it copies from real Steam: [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md#unity-mono-games-mock-steamworksnet-not-the-dll).
- **Isolation:** the plugin loads before the mod (GUID `aaa.coopqa`) and does nothing without `-coopqa` on the command line. Each copy gets its own test install, its own save folder (`GetSaveFolder` patch) and its own result and status files. The launcher stops only the processes it started.
- **Bot:** step lists inside the game call the same methods the menu buttons and the hotbar call, wait with a timeout, assert, and write `RESULT: PASS` or `RESULT: FAIL`. A QA-only side channel lets each bot publish a state word ("menu", "ate") for the others. Each test asserts its precondition, for example that the client is really not in the menu yet when the invite is sent.
- **Fixture:** `python run_qa.py make_fixture --save-fixture after_intro` saves the host's game after the intro; tests that start from it take about 35 s to set up instead of about 140 s.
- **Commands:** `python run_qa.py <test> [--lag ms] [--drop rate] [--fixture after_intro]` for one test, `python run_suite.py` for the suite.

The suite and what it asserts:

| Test | Asserts |
|---|---|
| `join_newgame` | lobby, invite, Ready, New Game, save transfer, both players in game and connected for 20 s |
| `invite_during_load` | the client starts 8 s later; the host invites before the client reaches the menu; the client still joins |
| `intro` | new game, shared intro cutscene, both players get control back |
| `move_sync` | the players take turns walking; the other side sees them at the position they report |
| `sleep_energy`, `sleep_energy_client_first` | both go to bed (host first, or client first); energy rises within 3 s of both lying down, both wake fully rested, same day and time on both |
| `quickslot_food` | each player eats from quick slot 1; one item gone and energy up on both (the host is the control) |
| `ping`, `ping --lag 100` | at least 5 of the mod's ping results in 60 s, median under a limit; without and with 100 ms added each way |

Last suite run, 2026-10-20, BFTG 432b1d0 plus B1, B2 and B3:

```
RESULT: PASS   join_newgame  (79s)
RESULT: PASS   invite_during_load --stagger 8  (67s)
RESULT: PASS   intro  (146s)
RESULT: PASS   move_sync --fixture after_intro  (54s)
RESULT: PASS   sleep_energy --fixture after_intro  (65s)
RESULT: PASS   sleep_energy_client_first --fixture after_intro  (65s)
RESULT: PASS   quickslot_food --fixture after_intro  (50s)
RESULT: PASS   ping --fixture after_intro  (100s)
RESULT: PASS   ping --fixture after_intro --lag 100  (107s)

SUITE: 9/9 passed in 12.2 min
```

Mutation check: with B1 removed, `invite_during_load` fails (the client never joins). With B2 removed, `join_newgame` fails at "P2 ready" in 4 of 4 runs. With B3 removed, the client fails `quickslot_food` in 3 of 3 runs while the host passes.

Limits: everything runs on localhost through the mock, so real Steam (relay, overlay invites, the real order of lobby data and P2P) is not covered. Movement uses `GoTo` from script, not the keyboard. Sleep opens `SleepGUI` directly. Two players only.

## How to play

Install BepInEx 5.4.x x64 and the mod DLL from the [BFTG releases](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/releases) on both PCs, as the mod's README describes, and use the same mod version on both. If the client doesn't own a DLC, turn that DLC off in the host's settings (`EnableDLCStories`, `EnableDLCRefugees`, `EnableDLCSouls`) before creating the lobby. Back up your saves first. The fixes above are not in a release until the author merges them.

## Known issues

- In 1.2.0 a client can't use food or potions from their own inventory (#22; fixed by B3, [PR #27](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/27), not released yet).
- An invite that arrives while the game is still loading is lost; invite again once the friend is in the main menu (fixed by B1, [PR #25](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/25), not released yet).
- Inviting a player who is already in a lobby breaks the UI (#23).
- Players report about 400 ms ping (#24), the morgue counter showing 1 with no corpse (#20) and a cutscene bug (#11).
- More than two players is untested.

## Next steps

- Get B1, B2 and B3 reviewed and merged upstream ([#25](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/25), [#26](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/26), [#27](https://github.com/ZlordHUN/GYK-Back-From-The-Grave/pull/27)).
- Test on real Steam with two accounts on two PCs: relay, overlay invites, and whether B2 happens there.
- Rerun the sleep and ping tests with `--lag` and `--drop`. One lead for #24: on real Steam, in the main menu with no connection, the mod's P2P receive took 12 to 15 ms per frame (one log, not measured by a test).
- Add tests for #23 (invite to a player already in a lobby), #20 and #11.
- Try graveyard-keeper-multiplayer under the same harness. It also uses Steamworks.NET, so the mock may carry over (not tried).
