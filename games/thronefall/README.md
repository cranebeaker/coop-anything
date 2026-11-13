# Thronefall (2023)

| | |
|---|---|
| Engine | Unity 6000.3.18f1, Mono scripting backend, URP (since the Craaghelm update of 2026-11-11; Unity 2022.3 before) |
| Versions tested | Steam, buildid 26487600 (2026-11-11), Windows 11 |
| Projects | Thronefall Multiplayer: BadWolf original (archived), Bwarhness / BiggestBlackest continuation, spencatro GitLab fork, Bemno fork (current) |
| QA level | automated (base game, Nordfels; see QA) |
| Last updated | 2026-11-13 |

## Where things stand

Two players can play Thronefall together over Steam with Bemno's fork of Thronefall Multiplayer (1.2.7), which runs on
the current Unity 6 build. Automated tests with two real game copies on two Steam accounts, plus the owner's first real
session with a friend, found six bugs in 1.2.7. The worst break everyday play: joining a friend a second time hangs,
Continue Run gives the joining player an empty map, a friend without the level unlocked cannot join it, and the end
screen can stay on screen for good. All six have fixes that pass the same tests. On the owner's request, optional
co-op settings were added: enemy difficulty, boss difficulty, a night vote, a map ping and rebindable mod keys, all
changeable in a menu on the map without a new lobby and all off by default (local build 1.2.7.6, not published). The
Craaghelm DLC maps were not tested.

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
  are collected. On buildid 26487600 an older progress file was not found ("Main save file not found").
- The game reads the match save into memory only when a level's first menu page opens
  (`LevelSelectUIFrameHelper.OnShow` -> `MatchSaveLoadHandler.TryLoadRun`, the only caller).
- Calling the night needs `PlayerInteraction.focussedInteractor == null` (not standing at a building).
- Level flags: the interaction collider sits about 15 m from the flag, at `LevelInteractor.PlayerTeleportPosition`.
- Input is Rewired. The mod samples it into `PlayerNetworkData.SharedData` for movement and night call, but
  `RunInteraction` reads Interact from Rewired directly.
- The mod's lobby is joinable only while the host is in a level select map (`Network.OnSceneChanged`).
- End-of-match screens (score, Level Up Frame, After Match Frame) live in the persistent `_UI` scene; Back from any
  player's end screen takes everyone to the map.
- Enemies: the host decides every spawn and sends an `EnemySpawn` packet to all machines, including itself. The game's
  own mutators scale `Hp.maxHp`, `AutoAttack.DamageMultiplyer`, `AutoAttack.cooldownDuration` and
  `PathfindMovementEnemy.movementSpeed`. Waves do not scale with the number of players.

## What we changed

Six fixes and a set of optional co-op settings, as seven patches against Bemno 1.2.7 (local build 1.2.7.6). The code
is not published yet.

1. **Joining hangs from the second session on.** `RecentPartners` (since 1.2.5) pings remembered partners while
   offline. When the ping times out (the partner's game is not ready), the `SteamNetworkingMessages` session to that peer
   stays failed, and every later send to it, including the join handshake, returns `k_EResultConnectFailed`; nothing
   closes it. Fix: close a session when a send returns `k_EResultConnectFailed`, and a failed one before the handshake.
   Test: two accounts that played together before: 1.2.7 FAIL 7/7, fixed PASS 5/5 and 3/3; first meeting PASS on both.
2. **Continue Run gives the client an empty map.** The client loads the match save when the host opens the flag, before
   the host sends its save; the save also travels on its own channel and can arrive after the level-sync packet. Mirror
   case: for New Run the host sent its old save. Fix: the client applies the host's save as soon as it arrives in the
   map and enters the level only after it arrived (max 5 s); New Run sends "start fresh".
   Test: Continue Run after a full restart of both games: unfixed FAIL 3/3, fixed PASS 3/3; New Run with an old host
   save: PASS.
3. **A rejoining player can end up as a "ghost".** Found while testing a crash and rejoin with an early version of fix 1:
   closing a working session before the handshake made the new connection time out while the host was loading the map,
   and the mod's `OnSessionFailed` then switched the joining client offline while its join request still reached the
   host, which added a player whose game never listened. Fix: close only failed sessions; while joining, a failed
   session to the host is retried. Test: `rejoin` PASS 12/12.
4. **A friend cannot join a level they have not unlocked.** Reported from the owner's first session: the friend's fresh
   profile had only the tutorial, and co-op in the tutorial does not work. `HandleLoadoutOpen` ignored the host's open
   for a level locked on the client ("LoadoutOpen for locked level ..., ignoring"), so the client got no loadout popup and
   no weapon pick, and the host could not start. Fix: a client follows opens that come from the host; the host's
   progress decides. Test: `locked_endscreen` with the level locked on the client only: 1.2.7.3 FAIL at the loadout step,
   1.2.7.4 PASS.
5. **The end screen stays on screen after a match.** Reported by the owner after a victory: the friend pressed Back
   first, the host was pulled to the map with its Level Up Frame open, and the screen stayed, even in the main menu.
   `CloseAllFrames` alone closes only the active frame; the After Match Frame below it on the stack stayed visible.
   Fix: before a networked transition out of a level, close the active frame and every frame on the stack. Test:
   `locked_endscreen` (client clicks through and presses Back first): host has no frame open in the map, PASS twice.
6. **Chat messages over about 60 characters are never sent.** `Buffer.EnsureSize` doubled the buffer once, which is not
   enough for a string longer than the buffer ("Destination array was not long enough"). Fix: grow until it fits.
   Found through the co-op difficulty chat message.

### New: optional co-op settings

Nothing changes unless a player turns it on. Session settings belong to the host; keys belong to each player.

| Setting | Who decides | Default | Choices |
|---|---|---|---|
| Night vote | host | Off | Off, On |
| Map ping | host | Off | Off, On |
| Count & HP | host | Normal | 10 presets, the last one Custom |
| Enemy type | host | Off | 10 presets, the last one Custom |
| Scale as players | host | Off | Off, Actual, 2 to 8 players; per player Light, Medium, Strong |
| Bosses | host | Normal | Normal, Sturdy, Tough, Brutal, Legendary, Custom |
| Ping key | each player | middle mouse button | any key or mouse button; None |
| Ping at | each player | Cursor | Cursor, Player |
| Chat key | each player | Enter | any key |
| Player list key | each player | the key left of 1 | any key |

**Co-op Settings menu (1.2.7.5).** Esc on the map opens the pause menu, which gets a "Co-op Settings" entry next to
"Copy Invite". The host changes values with < and >; they apply from the next level start, so the difficulty can change
between levels without a new lobby. The host publishes its values in the lobby data (`tfmp_coop`), and the other
players see them read only. The same values are in the BepInEx configuration window (F1). Below the session settings,
"Your keys (only for you)" lets every player rebind the mod's keys: click the key, press the new one (Esc cancels, Del
clears).

**Night vote (1.2.7.5).** When on, a player whose night-call bar fills is marked ready instead of starting the night;
the chat shows "X is ready for the night (1/2)". The host starts the night when every player is ready. A ready mark
lasts 30 seconds.

**Boss difficulty (1.2.7.5).** A survey of every level found the bosses spawn through the normal wave path: Frostsee
wave 12 (15000 HP), Moorweg wave 11 Ghostqueen (15000), Freifort wave 12 Iron Castle (66000), Totend wave 14 Corrupt
King (66666) with a second stage (70000). The host detects bosses by the game's Boss tag and gives them only the boss
setting: Sturdy 150% HP, Tough 200%, Brutal 300% and +25% damage, Legendary 400% and +50%, Custom. Every Hp of a boss
is scaled, so second stages too. Correction to 1.2.7.4: there the enemy difficulty also scaled bosses.

**Map ping (1.2.7.6).** When on, the ping key marks a spot for every player: a diamond in the player's colour with their
name on a dark plate, for 5 seconds, kept at the screen edge while the spot is off screen. "Cursor" puts it on the
ground under the mouse (the plane at the player's height, or a static collider within 8 units of it; characters and
units are skipped), "Player" where the character stands. A `MapPing` packet goes client to host to the others; the host
stamps the real sender. The marker is screen-space UI, so it needs no shaders.

**Co-op difficulty (1.2.7.4).** Waves are the single-player waves, so every extra player makes the game easier. The
host picks one entry from each list; the default is the base game.

- CountAndHp: Normal, Sturdy (125% HP), Tough (150%), VeryTough (200%), MoreEnemies (+50%), Horde (+100% at 85% HP),
  Veterans (+25% at 130% HP), Hard (+50% at 150% HP), Brutal (+100% at 200% HP), Custom (free HP and count).
- EnemyType: Off, SomeElites (20% elites), ManyElites (40%), Aggressive (+30% damage), Swift (+25% speed), Frenzied
  (attack 30% faster), Brutes (+50% damage, 15% slower), EliteAssault (40% elites, +25% damage), Nightmare (60% elites,
  +30% damage, +20% speed, attack 15% faster), Custom (free damage, speed, attack rate, elite share).
- ScaleAsPlayers (off by default): Actual, or a fixed count from 2 to 8, independent of the real number (two friends
  with the scaling of eight, or eight with the scaling of two); ScalingPerPlayer Light/Medium/Strong adds 15/25/35% HP
  and enemies per player beyond the first.

The host sends HP, damage, speed and attack-rate factors, the elite roll and the extra spawns in its `EnemySpawn`
packets, so every machine spawns the same enemies. Extra enemies drop no coins and do not count toward the wave, so
waves still end and the economy stays as in the base game. Bosses get only the boss setting (1.2.7.5 on). When a setting
is not at its default, the choice is shown in chat at level start. The packet format changed, so all players need the
same build (the mod's version check enforces it).

## QA

Two isolated copies of the game on one PC: copy A on the owner's Steam account, copy B in a Sandboxie-Plus box on a
second account (`StartService=Steam Client Service` in the box). Both run as Spacewar (`steam_appid.txt` = 480) with a
patched product name in `globalgamemanagers`, so the owner's saves, PlayerPrefs and Steam Cloud stay untouched (hashed
before and after every run). A test-only plugin (`qa/CoopQaDriver`, inert without `-coopqa`) is a virtual gamepad at the
Rewired level plus a JSON control port; `qa/tf_bots.py` runs Thronefall playbooks on the hub runner (`tools/bots`), which
settles and compares the shared state after every step. Game errors count as `apply_failures` and must not rise.

```
powershell -File games/thronefall/qa/run-connect.ps1 -Unlock            # or -UnlockHostOnly: the client keeps its own progress
python -I games/thronefall/qa/tf_bots.py --playbook day_night --level Nordfels --nights 0
python -I games/thronefall/qa/tf_bots.py --playbook locked_endscreen --level Nordfels
python -I games/thronefall/qa/tf_bots.py --playbook difficulty --level Nordfels
python -I games/thronefall/qa/tf_bots.py --playbook coop_settings --level Frostsee
python -I games/thronefall/qa/tf_bots.py --playbook map_ping --level Nordfels
```

Results, 2026-11-12 and 2026-11-13, buildid 26487600, mod 1.2.7 plus fixes:

| Playbook | Result |
|---|---|
| connect (two accounts, Steam lobby + P2P) | PASS; with remembered partners 1.2.7 FAIL 7/7, fixed PASS |
| `day_night` Nordfels 2 nights | PASS 10/10 steps: host and client build in turns, each calls a night, enemy count difference 0, building HP equal at dawn |
| `day_night` Nordfels to the end | PASS 19/19 steps: defeat on night 5 on both at once, same end screen, Back returns both to the map |
| `save_continue` after a full restart | unfixed FAIL 3/3, fixed PASS 3/3, PASS again on later builds |
| `rejoin`: client crash, host plays on, client rejoins in the map, Continue Run | PASS 12/12 (1.2.7.3) |
| `locked_endscreen`: level locked on the client; client presses Back first after the match | 1.2.7.3 FAIL (client ignores the open), 1.2.7.4 PASS 15/15 twice |
| `difficulty`, every enemy compared host vs client (max HP, speed, elite, damage) | Normal: 3 enemies, HP 25, speed 5, damage 1.0. Brutal + Nightmare: 6 enemies, HP 50 (elites 200), speed 6, damage 1.3 (elites 3.9). Custom 150%/+50%, damage 200%, scaled as 8 players (Light): 7 enemies, HP 63.8 (25 x 2.55), damage 2.0. All PASS, identical on both machines, 0 game errors |
| `coop_settings` Frostsee: menu on the map, night vote, boss difficulty changed in the same lobby | PASS 15/15 (1.2.7.5 and 1.2.7.6): client read only; host alone ready stays Day, both ready gives Night on both; boss 15000 HP (Normal), then 30000 on both after switching to Tough in the menu |
| `map_ping` Nordfels | PASS 9/9 (1.2.7.6), twice: off by default sends nothing; host turns it on, client sees it read only; both players rebind their own keys; Ping at Player lands on the pinging player; two cursor points land under the cursor (screen round trip within 3%) and on the same spot on both |
| `locked_endscreen` regression | PASS 15/15 on 1.2.7.5 |

## How to play

Both players install BepInExPack_Thronefall 5.4.2100 and the same build of the mod (1.2.7 from
[Bemno's releases](https://github.com/Bemno/Thronefall_Multiplayer/releases); the fixed build is not published yet).
Host: Multiplayer, Host, invite. Friend: Multiplayer, Direct Connect with the invite. Always let the same player host a
run; the host's save is sent to the others automatically. After a match, both click through the end screens before one
presses Back. With the fixed build, the optional settings are in Esc > Co-op Settings on the map.

## Known issues

- With 1.2.7 as released:
  - joining the same friend a second time can hang on "Connecting". Workaround: the joining player deletes
    `BepInEx\config\thronefall_mp_recent.txt` before starting the game;
  - Continue Run gives the joining player an empty map. Workaround: start a New Run;
  - a friend can only join levels they have unlocked themselves. Workaround: play the tutorial alone once;
  - if one player presses Back on the end screen first, the other's end screen can stay on screen. Workaround:
    restart the game;
  - chat messages over about 60 characters are not sent.
- Co-op in the tutorial level does not work (players die in a loop).
- A joining player who crashes cannot rejoin while the host is in a level: the host goes back to the map (pause menu),
  the friend joins again (a second click may be needed right after the host's map loads), the host continues the run
  from the last dawn. Wait a few seconds after the friend appears before opening a level flag.
- Reported once, not reproduced: the joining player did not see the perks the host selected (in the tutorial). The
  owner later confirmed the friend does see them outside the tutorial.
- Not tested in 1.2.7.6: real key presses for the ping and the key capture (the bots call the same code the key calls,
  with minimized windows the mouse position is not meaningful), controller buttons as keys, pinging on the overworld
  map, the Co-op Settings menu with a controller, Totend's second boss stage, a victory run.
- Leaving a level right after a defeat logs a short burst of `NullReferenceException` in `PathfindMovementEnemy.Update`
  (game code; about 700 lines on the host, then quiet). No visible effect.

## Next steps

- Publish the fixes (offer them to Bemno/Thronefall_Multiplayer).
- Shared perk selection with different unlocks: 26 equippables are not in the mod's equipment table and get runtime
  ids, which may differ between machines (suspicion).
- A victory run (the bot needs upgrade choice dialogs to survive 13 waves), Craaghelm maps (need the DLC), army
  commands, a lossy link.
- Lead: a host with no weapon equipped (fresh profile) who presses Play on the level's first page waits forever for its
  own weapon pick (`WeaponRequest` sent to itself, "Received unknown packet").
