# Thronefall (2023)

| | |
|---|---|
| Engine | Unity 6000.3.18f1, Mono scripting backend, URP (since the Craaghelm update of 2026-11-11; Unity 2022.3 before) |
| Versions tested | Steam, buildid 26487600 (2026-11-11), Windows 11 |
| Projects | Thronefall Multiplayer: BadWolf original (archived), Bwarhness / BiggestBlackest continuation, spencatro GitLab fork, Bemno fork (current) |
| QA level | automated (base game, Nordfels; see QA) |
| Last updated | 2026-11-12 |

## Where things stand

Two players can play Thronefall together over Steam with Bemno's fork of Thronefall Multiplayer (1.2.7), which runs on
the current Unity 6 build. Automated tests with two real game copies on two Steam accounts found two bugs that break
everyday play (joining a friend a second time hangs; Continue Run gives the joining player an empty map); both have
fixes that pass the same tests. The Craaghelm DLC maps were not tested.

## Existing work

All projects descend from BadWolf's mod and share the plugin GUID `com.badwolf.thronefall_mp`. BepInEx 5 + MonoMod
HookGen + Harmony, Steam lobbies and `SteamNetworkingMessages`, host authoritative. None has a license file; BadWolf's
archive note invites forks "as long as they give credit".

| Project | Last activity | State on buildid 26487600 |
|---|---|---|
| [MunWolf/thronefall_multiplayer](https://github.com/MunWolf/thronefall_multiplayer) (BadWolf) | archived 2024-02-10 | not tested |
| [Bwarhness/thronefall_multiplayer](https://github.com/Bwarhness/thronefall_multiplayer), Thunderstore [ThronefallMultiplayerContinued](https://thunderstore.io/c/thronefall/p/BiggestBlackest/ThronefallMultiplayerContinued/) 1.2.2 | 2026-06-15 | not tested |
| [catro-creative/thronefall-multiplayer-thirdfork](https://gitlab.com/catro-creative/thronefall-multiplayer-thirdfork) (GitLab) 2.1.1 | 2026-08-04 | references `SceneTransitionManager.levelSelectSceneFangmore`, which this build does not have (not run) |
| [Bemno/Thronefall_Multiplayer](https://github.com/Bemno/Thronefall_Multiplayer) 1.2.6 | 2026-11-11 | loads, but its multiplayer menu throws `MissingFieldException: TMPro.TMP_Asset.material` (Unity 6) |
| Bemno 1.2.7 | 2026-11-12 | loads cleanly; tested below |

## How the game works (for co-op)

- Single-player game, no netcode of its own. Bundles Steamworks.NET and `steam_api64.dll`; an in-process plugin uses the
  game's Steam session (real app id) for lobbies and P2P.
- Saves: `%USERPROFILE%\AppData\LocalLow\Grizzly Games\Thronefall\` (`ThroneSave.sav` progress, `mp_<level>.json` the
  mod's multiplayer match saves), synced by Steam Auto-Cloud. A match is saved at every dawn, before the morning coins
  are collected.
- The game reads the match save into memory only when a level's first menu page opens
  (`LevelSelectUIFrameHelper.OnShow` -> `MatchSaveLoadHandler.TryLoadRun`, the only caller).
- Calling the night needs `PlayerInteraction.focussedInteractor == null` (not standing at a building).
- Level flags: the interaction collider sits about 15 m from the flag, at `LevelInteractor.PlayerTeleportPosition`.
- Input is Rewired. The mod samples it into `PlayerNetworkData.SharedData` for movement and night call, but
  `RunInteraction` reads Interact from Rewired directly.
- The mod's lobby is joinable only while the host is in a level select map (`Network.OnSceneChanged`).

## What we changed

Fixes live as three patches against Bemno 1.2.7 (local build 1.2.7.3); they are not published yet.

1. **Joining hangs from the second session on.** `RecentPartners` (since 1.2.5) pings remembered partners while
   offline. When the ping times out (the partner's game is not ready), the `SteamNetworkingMessages` session to that peer
   stays failed, and every later send to it, including the join handshake, returns `k_EResultConnectFailed`; nothing
   closes it. Fix: close the session before the handshake and whenever a send returns `k_EResultConnectFailed`.
   Test: two accounts that played together before: 1.2.7 FAIL 7/7, fixed PASS 5/5; first meeting PASS on both.
2. **Continue Run gives the client an empty map.** The client loads the match save when the host opens the flag, before
   the host sends its save; the save also travels on its own channel and can arrive after the level-sync packet. Mirror
   case: for New Run the host sent its old save. Fix: the client applies the host's save as soon as it arrives in the
   map and enters the level only after it arrived (max 5 s); New Run sends "start fresh".
   Test: Continue Run after a full restart of both games: unfixed FAIL 3/3, fixed PASS 3/3; New Run with an old host
   save: PASS.

3. **A rejoining player can end up as a "ghost".** Found while testing a crash and rejoin with fix 1: closing a working
   session before the handshake made the new connection time out while the host was loading the map, and the mod's
   `OnSessionFailed` then switched the joining client offline while its join request still reached the host, which added
   a player whose game never listened. Fix: close only failed sessions; while joining, a failed session to the host is
   retried. Test: `rejoin` PASS 12/12 (client crash, host plays on, client rejoins in the map, Continue Run: both at the
   saved dawn); connect with remembered partners PASS 3/3.

## QA

Two isolated copies of the game on one PC: copy A on the owner's Steam account, copy B in a Sandboxie-Plus box on a
second account (`StartService=Steam Client Service` in the box). Both run as Spacewar (`steam_appid.txt` = 480) with a
patched product name in `globalgamemanagers`, so the owner's saves, PlayerPrefs and Steam Cloud stay untouched (hashed
before and after every run). A test-only plugin (`qa/CoopQaDriver`, inert without `-coopqa`) is a virtual gamepad at the
Rewired level plus a JSON control port; `qa/tf_bots.py` runs Thronefall playbooks on the hub runner (`tools/bots`), which
settles and compares the shared state after every step. Game errors count as `apply_failures` and must not rise.

```
powershell -File games/thronefall/qa/run-connect.ps1 -Unlock
python -I games/thronefall/qa/tf_bots.py --playbook day_night --level Nordfels --nights 0
```

Results, 2026-11-12, buildid 26487600, mod 1.2.7 plus fixes (1.2.7.2):

| Playbook | Result |
|---|---|
| connect (two accounts, Steam lobby + P2P) | PASS |
| `day_night` Nordfels 2 nights | PASS 10/10 steps: host and client build in turns, host and client each call a night, enemy count difference 0, building HP equal at dawn |
| `day_night` Nordfels to the end | PASS 19/19 steps: 5 nights, defeat on night 5 on both machines at once, same end screen; Back from the end screen returns both to the map, still connected |
| `save_continue` after a full restart | PASS 3/3 (fixed build), PASS again on the final build |
| `rejoin`: client crash mid-match, host plays on, client rejoins in the map, Continue Run | PASS 12/12 (final build 1.2.7.3) |

## How to play

Both players install BepInExPack_Thronefall 5.4.2100 and the same build of the mod (1.2.7 from
[Bemno's releases](https://github.com/Bemno/Thronefall_Multiplayer/releases)). Host: Multiplayer, Host, invite. Friend:
Multiplayer, Direct Connect with the invite. Always let the same player host a run; the host's save is sent to the
others automatically.

## Known issues

- With 1.2.7 as released: joining the same friend a second time can hang on "Connecting". Workaround: the joining player
  deletes `BepInEx\config\thronefall_mp_recent.txt` before starting the game.
- With 1.2.7 as released: Continue Run gives the joining player an empty map. Workaround: start a New Run.
- A joining player who crashes cannot rejoin while the host is in a level: the host goes back to the map (pause menu),
  the friend joins again (a second click may be needed right after the host's map loads), the host continues the run
  from the last dawn.
- Right after a rejoin, wait a few seconds before opening a level flag: in tests, opening it in the same second the
  friend appeared could close the menu again or leave the friend on the map.
- Leaving a level right after a defeat logs a short burst of `NullReferenceException` in `PathfindMovementEnemy.Update`
  (game code; about 700 lines on the host, then quiet). No visible effect.

## Next steps

- Offer both fixes to Bemno/Thronefall_Multiplayer.
- A victory run (the bot needs to handle upgrade choice dialogs to survive 13 waves), Craaghelm maps (need the DLC),
  army commands, the tutorial level, a lossy link.
- Lead: a host with no weapon equipped (fresh profile) who presses Play on the level's first page waits forever for its
  own weapon pick (`WeaponRequest` sent to itself, "Received unknown packet").
