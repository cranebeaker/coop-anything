# Tale of Immortal (鬼谷八荒, 2021)

| | |
|---|---|
| Engine | Unity 2020.3.9f1, IL2CPP, x64. The game ships MelonLoader 0.5.4 as its official code-mod loader |
| Versions tested | Steam build 21758240, `guigubahuang.exe` SHA-256 `038789f47291a3a9dfd4c6f6897e24757630d95139d6b12af3987d649a25feac`, Windows 11, Tale of Immortal Multiplayer 1.1.10 with two real clients on one PC |
| Projects | [Tale of Immortal Multiplayer](https://steamcommunity.com/sharedfiles/filedetails/?id=3811631543), [Immortal Co-op](https://github.com/JosephCarmichael/ImmortalCoop), [诸天万界](https://steamcommunity.com/sharedfiles/filedetails/?id=2958503186) (abandoned) |
| QA level | automated for Tale of Immortal Multiplayer, 24 of 25 checks (see QA). None for Immortal Co-op |
| Last updated | 2026-11-02 |

## Where things stand

Yes, people can play together today, with rough edges. In automated runs with two real game clients, Tale of Immortal
Multiplayer hosted, joined, merged two characters into one world, kept NPCs and player positions matched, passed months
and survived a client crash. One check still fails: a few NPCs differ after a month change, until the next month. It needs a VPN
LAN, and these runs used one PC. Immortal Co-op connects through Steam with no VPN, but each player keeps their own world
after a shared start, and its guide says it has not been tested across separate computers.

## Existing work

| Project | Last activity | Status | What it shares |
|---|---|---|---|
| [Tale of Immortal Multiplayer](https://steamcommunity.com/sharedfiles/filedetails/?id=3811631543) by Akulia, also [Nexus 87](https://www.nexusmods.com/taleofimmortal/mods/87) | v1.1.10, 2026-10-27 | active | One world and save: NPC positions, deaths, grades, relations, sects. Group fights in dungeons, PvP, trade, marriage. 2+ players, author tested 5. VPN LAN or real LAN, port 47813. Closed source |
| [Immortal Co-op](https://github.com/JosephCarmichael/ImmortalCoop) by JosephCarmichael, [site](https://josephcarmichael.github.io/ImmortalCoop/) | v0.5.1, 2026-10-18 | early test build | Shared start, then separate worlds. Player-caused NPC deaths, hatred, joinable battles. 2 to 4 players, Steam invites. Closed source, unsigned installer |
| [诸天万界（联机mod）](https://steamcommunity.com/sharedfiles/filedetails/?id=2958503186) by SolidStateLight | 2023-06-01 | abandoned | Chat across games, assist copies, invasions and duels. No shared world |

An AI-made prototype by 每周五更新的银行保安 appears in [a bilibili video](https://www.bilibili.com/video/BV1vv4X6VEYd/) from August 2026. No download was found. The
Steam store page lists no multiplayer feature, and no developer statement on co-op was found in the threads read.

## How the game works (for co-op)

- No netcode. The only network-ish classes are `SteamManager` and a remote-config fetch.
- Code mods run under the MelonLoader 0.5.4 the game ships, with Il2CppAssemblyUnhollower 0.4.18 proxies and .NET Framework 4.7.2.
  Guides for MelonLoader 0.6+ and Il2CppInterop don't match this install, though one community project does target that newer stack.
- `ModMain.Init()` runs on entering the game and `Destroy()` on return to the menu, then `Init()` again.
- The game ships `steam_api64.dll` with the flat API, lobbies and `SteamNetworkingMessages`.
- Time passes by month. Akulia's mod holds the month until every player is ready.
- Saves are in `%USERPROFILE%\AppData\LocalLow\guigugame\guigubahuang\`.

## What we changed

Nothing in either mod. This write-up records research, static review and one load test.

## QA

The suite lives in [qa/](qa/). It starts two real clients on one PC with `tools/gamepool` and `tools/gameslot`. `CoopQA.Bot` is a
test-only MelonLoader mod, inert without its config file. It calls the handlers behind the mod's buttons, loads a prepared save,
answers dialogs and reports state to a file. `run_coop_test.py` scores the run and prints `RESULT: PASS` or `RESULT: FAIL`.

```
py -I games/tale-of-immortal/qa/setup_lab.py <repo> config      # after copying the install to local/lab/base
py -I tools/gamepool/gamepool.py --config local/toi.json provision --n 2
py -I games/tale-of-immortal/qa/setup_lab.py <repo> 2
py -I games/tale-of-immortal/qa/build_bot.py local/lab/base local/lab/build/CoopQA.Bot.dll
py -I games/tale-of-immortal/qa/run_coop_test.py --config local/toi-qa.json
py -I games/tale-of-immortal/qa/run_coop_test.py --config local/toi-qa.json --mutant drop_npc_death
```

Checks, in order: two clients boot with the bot and mod loaded; roles are as set; the host session starts and the client joins;
both load a world; the merged world is active on both, in the same folder, with different characters; both dumps hold both players
and the same living NPCs (1,557 units); the client moves and the host sees the new tile; an NPC killed on the host dies on the
client; two month changes finish on both; the client is killed and the host notices and keeps running; the owner's real data is unchanged.

Results on 2026-11-02, build 21758240, mod 1.1.10, one PC, 2 clients:

```
r8  RESULT: FAIL  (24/25 checks) failed=['same_living_npcs_after_month_1']   1 NPC differs after month 1, 0 after month 2
r9  RESULT: FAIL  (24/25 checks) failed=['same_living_npcs_after_month_1']   6 NPCs differ after month 1, 0 after month 2
n1  RESULT: FAIL  (16/17 checks) failed=['npc_death_reaches_client']         negative control, failing as required
```

The negative control makes the client drop the host's NPC death messages, and only the NPC death check fails. Every run also hashed
the real saves, PlayerPrefs, install manifest and Steam userdata before and after: `ISOLATION: PASS`, and a tampered snapshot makes that check
print FAIL. Copies still read the real PlayerPrefs once, through the game's `FixPatch139.RetrieveSavedPrefs`, so isolation means no writes.

For the tests only, the bot skips the mod's firewall step and binds its sockets to loopback, so no administrator prompt appears.

Not covered: new character creation (runs start from a prepared save), three or more players, separate PCs, a real VPN, rejoining after a
disconnect, battles together, lag and loss. Immortal Co-op was not run.

Static checks with no code executed:

- Tale of Immortal Multiplayer 1.1.10 (`TOI_MP_MenuMod.dll`, SHA-256 `5f78b6285db1015574edf7f3d368c68d6401e44a0e4239a78c14b1ba997fbfcd`, not signed): no native imports
  and no web address. Network use is TCP and UDP on port 47813, and the host listens on all interfaces. It starts only `netsh`, directly to read
  rules and through an elevated `cmd.exe` to add them, with `profile=any`.
- Immortal Co-op 0.5.1: all release files match their published SHA-256 values. Network use found is Steam sockets, TCP sessions, the update
  check and download, and problem reports sent from the menu or hotkey.
- Both were targeted traces, not full audits.

## How to play

Tale of Immortal Multiplayer:

1. Back up `%USERPROFILE%\AppData\LocalLow\guigugame\guigubahuang` and close the game.
2. Every player subscribes to the Workshop item and uses the same mod and game version. Other code mods are best switched off.
3. Every player joins one network in a VPN such as Radmin VPN or ZeroTier.
4. Host: title screen, MULTIPLAYER, Create Game. The mod asks for administrator approval to add inbound rules for port 47813 on
   all network types. Remove them after play, or add narrower rules yourself first.
5. Friends: MULTIPLAYER, Join Game. Everyone clicks their banner, and the shared world starts when all are ready.

Immortal Co-op: follow [the site](https://josephcarmichael.github.io/ImmortalCoop/), and start a new campaign. Each player needs their own Steam account.

## Known issues

- Tale of Immortal Multiplayer:
  - Each machine rolls some NPC deaths in its own month simulation. 1 to 6 of about 1,555 NPCs differed after a month change in three runs. In the two
    runs that went on, the next month cleared them all.
  - Secret realms and mining spots differ between players on purpose, per the author.
  - Everyone must be ready before the month changes, and a month-end story dialog on any machine holds everyone until it is answered.
  - A world that needs a missing mod can break hosting. One test save threw an exception on load, after which saving and loading never finished.
    If the game shows a lost mod warning, try another world.
  - Hosting adds inbound firewall rules for port 47813 on all network types, and nothing found removes them.
- Immortal Co-op: quests, shops and NPC lives differ between players. Trials and story battles are solo. Death penalties are real, including permanent death.

## Next steps

- Report the month-end NPC death divergence to the mod's author, with the run logs.
- Extend the bot: create a new character, fight together, rejoin after a disconnect, three clients, and lag and loss.
- Run one session across two PCs over a VPN, and check Tailscale, which the mod's pages do not name.
