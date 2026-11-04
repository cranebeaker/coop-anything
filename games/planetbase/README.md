# Planetbase (2015)

| | |
|---|---|
| Engine | Unity, Mono backend (BepInEx 5 x86 + Harmony); exact Unity version not recorded |
| Versions tested | Steam, Windows build 1.4.2 (Steam build id 20301361), run on Linux through Proton 9.0; `Assembly-CSharp.dll` SHA-256 `245441e4892d10551823334b7171ec712e88e7218913c20d8b44154ca98af045` |
| Projects | [Planetbase Co-op](https://github.com/Elwynn18/Planetbase-coop); in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated |
| Last updated | 2026-10-25 |

## Where things stand

Two players can run one colony together: one hosts, the other joins and plays in the host's colony. Building,
linking, trading, priorities, game speed and settings all go through the host. An automated suite with two copies
of the game under Proton passes 16/16, including mutation twins. Only two players have been tested, and the game has no
player avatars, so "seeing the other player" means seeing their placement previews with their name.

## Existing work

Searched on 2026-10-25: web search (two queries), the Planetbase Steam discussions found by them, and this hub's
directory, issues and games. No other co-op or multiplayer project for Planetbase was found. Not searched: Nexus
mod pages one by one, Reddit, Discord servers.

- **[Planetbase Co-op](https://github.com/Elwynn18/Planetbase-coop)** (Elwynn18, MIT), version 0.2.0, first commit
  2026-10-24. Built with this hub's playbook.

## How the game works (for co-op)

- **Engine:** Unity with the Mono backend: game code in `Planetbase_Data/Managed/Assembly-CSharp.dll`, readable
  with ILSpy, patchable with Harmony. The game is **32-bit**: BepInEx must be the `win_x86` build
  (`BepInEx_win_x86_5.4.23.5.zip`); the x64 build does not load.
- **No netcode** of its own. The game ships Steamworks.NET (20.2.0) and its Steam app id is 403190; the mod reuses
  both for its Steam transport, so nothing extra is installed for Steam play.
- **Saves:** the game's own serializers turn a colony into XML. The mod uses them to build a save in memory on the
  host and load it on the joiner with the game's own loader.
- **Linux:** the Windows build runs under Proton. Wine loads its own `winhttp.dll` unless the launch option
  `WINEDLLOVERRIDES="winhttp=n,b" %command%` is set, so BepInEx never starts without it.
- **Version check:** the mod compares the game's `Assembly-CSharp.dll` byte for byte between players, so any mod
  that patches that file on disk shows up as a different game build.

## What we changed

Planetbase Co-op is a new mod (path 2, built from zero). Design: host-authoritative mirror.

- **Join:** the host's colony is sent as a save built in memory and loaded through the game's own loader; the
  joiner's simulation is then frozen.
- **Sync:** object lifecycle (created/destroyed, as the game's own XML), small state records (positions,
  construction, resource holders, ships, indicators), and a census every 5 seconds that repairs drift.
- **Actions:** player actions are caught at the game's commit points (placing a module, linking, the trade
  window's Trade button) and replayed on the host by object id.
- **Hazards:** meteors and lightning are replayed on joiners from the host's seed and target; sandstorms,
  blizzards and solar flares are sent once a second. Damage is worked out on the host only.
- **Fixes from the first playtest (0.2.0):** accumulator and water tank levels; the joiner's colonist counter
  showed `int.MaxValue`; a failed joiner trade could lose the base's coins; a joiner's settings edit could be
  overwritten by the host's 5 s re-send; build progress of a paused build did not reach joiners.
- **Error guard:** during a session, a game error no longer opens the fatal "ERROR ... Quit" page; it is logged,
  and a joiner sends it to the host's log.

## QA

`test/lab.py` provisions hard-linked copies of the game, each with its own Proton prefix, BepInEx and the mod, and
runs each copy inside its own headless gamescope, so no window appears on the tester's desktop. An in-game bot
drives each copy. Copies have no `steamclient.dll` and the mod blocks `SteamAPI.Init` in test mode, so tests never
reach a real Steam client. Xvfb does not work for this game (software GL never draws a second frame).

`python3 -I test/suite.py` runs every case. Each mutation twin turns one protection off and must fail on the named
check:

| Case | Asserts |
|---|---|
| `boot` | one copy starts with BepInEx and the mod loaded |
| `join` | the joiner loads the host's colony; buildings, colonists and resources match |
| `refuse` | a joiner with a different mod version is refused |
| `ships` | landing ships, notifications and settings reach the other side |
| `hazards` | meteors, sandstorms and placement previews show on both sides |
| `twin-no-lifecycle` | fails: host-built module appears on the joiner |
| `twin-no-commands` | fails: joiner-placed module built by the host and mirrored back |
| `twin-no-char-states` | fails: colonists are where the host has them |
| `twin-no-res-states` | fails: resources have the same holder and place |
| `twin-no-census` | fails: census restores the resource the joiner lost |
| `twin-no-build-states` | fails: construction state and progress match |
| `twin-no-ship-states` | fails: landing ship position on the joiner |
| `twin-no-notices` | fails: host notification shown on the joiner |
| `twin-no-settings` | fails: host's landing permission change reaches the joiner |
| `twin-no-edit-flush` | fails: joiner's landing permission change reaches the host |
| `twin-no-error-guard` | fails: joiner error caught and reported |

Last suite run, 2026-10-26, mod 0.2.0:

```
SUITE: 16/16 passed
  PASS boot (19 s)
  PASS join (59 s)
  PASS refuse (72 s)
  PASS twin-no-commands (90 s) twin caught: FAIL joiner-placed module built by the host and mirrored back (host side) (timeout after 25 s)
  PASS twin-no-lifecycle (123 s) twin caught: FAIL host-built module appears on the joiner (Module: host 4 joiner 3)
  PASS twin-no-res-states (66 s) twin caught: FAIL resources have the same holder and place (4 of 114 off, e.g. 119 host 0:-1:-1:1021.7:925.8 joiner 0:-1:-1:1017.7:922.8)
  PASS twin-no-char-states (117 s) twin caught: FAIL colonists are where the host has them (within 5 m, one walked 8 m+) (4 of 9 off, 3 moved, worst 48.0 m)
  PASS twin-no-census (77 s) twin caught: FAIL census restores the resource the joiner lost (id 119) (timeout after 20 s)
  PASS twin-no-build-states (77 s) twin caught: FAIL construction state and progress match (2 of 5 off, e.g. 132 host ModuleTypeSolarPanel:1:1065.8:943.3:0.50 joiner ModuleTypeSolarPanel:1:1065.8:943.3:0.00)
  PASS ships (90 s)
  PASS twin-no-ship-states (111 s) twin caught: FAIL landing ship position on the joiner (timeout after 60 s)
  PASS twin-no-notices (102 s) twin caught: FAIL host notification shown on the joiner (timeout after 10 s)
  PASS twin-no-settings (99 s) twin caught: FAIL host's landing permission change reaches the joiner (timeout after 10 s)
  PASS twin-no-edit-flush (100 s) twin caught: FAIL joiner's landing permission change reaches the host (timeout after 10 s)
  PASS hazards (146 s)
  PASS twin-no-error-guard (158 s) twin caught: FAIL joiner error caught and reported (received 0, fatal 'coop test: deliberate error to check the error guard')
```

Limits: the suite runs on one PC over direct IP. Two people played together on two PCs with two Steam accounts,
joining by Steam invite (reported by the author, no test harness): the first session (0.1.0) found the bugs fixed in 0.2.0,
and a later one used the one-file installer on the second player's PC. Lightning, blizzards and solar flares share the meteor and sandstorm code but are not covered by tests.

## How to play

Everyone needs Planetbase 1.4.2 from Steam and the same mod version.

- **Windows:** download [`install/Planetbase-Coop.bat`](https://github.com/Elwynn18/Planetbase-coop/blob/main/install/Planetbase-Coop.bat)
  and double-click it.
- **Linux (Steam + Proton):** download [`install/planetbase-coop.sh`](https://github.com/Elwynn18/Planetbase-coop/blob/main/install/planetbase-coop.sh)
  and run `bash planetbase-coop.sh`. It also sets the Proton launch option (Steam closed, `localconfig.vdf` backed
  up first).

Each installer finds the game in every Steam library, checks the game build, installs BepInEx 5.4.23.5 x86 and the
latest mod (both SHA-256 checked), and offers to start the game. Run it again to update; the same menu uninstalls.
Manual steps are in the [mod's README](https://github.com/Elwynn18/Planetbase-coop#readme).

In the game, press **F8**. Host: load a colony, F8 > Host with Steam > Invite. Joiner: accept the Steam invite from
the main menu. Direct IP also works (TCP 47610; forward it on the host's router or use a VPN).

## Known issues

- Only the host can save. If the host quits, joiners return to the menu.
- A joiner's action takes one round trip to the host.
- Joiners don't see the anti-meteor laser's beam, only the meteor vanishing.
- A building right at the edge of the allowed area near the colony ship can show as blocked for a joiner.
- More than two players: not covered by tests.

## Next steps

- Play a session over the internet with two Steam accounts and record what breaks.
- Test three or more players.
- Add tests for lightning, blizzards, solar flares and intruders.
- Add a lag and loss run (`tools/chaos`).
