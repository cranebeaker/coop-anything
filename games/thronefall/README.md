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
session with a friend, found seven bugs in 1.2.7. The worst break everyday play: joining a friend a second time hangs,
Continue Run gives the joining player an empty map, a friend without the level unlocked cannot join it, and the end
screen can stay on screen for good. All seven have fixes that pass automated tests (the Esc fix with real key
presses). On the owner's request, optional co-op settings were added: enemy and boss difficulty with per-player
scaling, a night vote, map pings with a ping wheel, a ready check, match and night summaries, joining a running level,
teammate revive and respawn rules, planning with coloured blueprints and build votes, destroyed buildings that stay
destroyed (with gold back and rebuilds), an endless mode, player colours per part, shared wardrobes, team HP, a kill
counter and extra game keys; all changeable in a tabbed menu on the map without a new lobby and all off by default
(local build 1.2.8.0, not published). The Craaghelm DLC maps were not tested.

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

Seven fixes and a set of optional co-op settings, as thirteen patches against Bemno 1.2.7 (local build 1.2.8.0). The code
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
7. **Esc stops closing the mod's menus.** Reported by the owner (Co-op Settings did not close with Esc). Already in
   1.2.7: `UIManager.ExitHandled` ("Esc used this frame") was a static field that the first panel to use Esc set to
   true and nothing ever reset, so every later Esc was ignored by every mod panel. Fix: it is true only in the frame it
   was set. Not tested with a real key press (the bots cannot press Esc).

### New: optional co-op settings

Nothing changes unless a player turns it on. Session settings belong to the host; keys belong to each player.

| Setting | Who decides | Default | Choices |
|---|---|---|---|
| Night vote | host | Off | Off, On |
| Map ping | host | Off | Off, On |
| Ready check | host | Off | Off, On |
| Match stats | host | Off | Off, On |
| Join during a level | host | Off | Off, On |
| Plan buildings first | host | Off | Off, On |
| Build vote | host | Off | Off, AnyOne, Majority, All |
| Night summary | host | Off | Off, On |
| Destroyed stay destroyed / Gold back | host | Off / 0% | Off, On / 0-100% |
| Endless / Harder per night | host | Off / Steady | Off, On / Gentle 5%, Steady 10%, Hard 15%, Brutal 25%, Insane 40%, Custom 1-100% |
| Teammate revive / Revive time | host | Off / 3 s | Off, On / 1-10 s |
| Respawn wait / Longer per death | host | game / 0 s | game or 5-120 s / 0-60 s |
| Gold from extra enemies | host | 0% | 0-200% |
| Enemy damage, boss HP, boss damage per player | host | Off | Off, Light, Medium, Strong, Custom |
| Profiles | host | - | 5 slots, Save / Load |
| Color mode / part colours | each player | One color / Auto | One color, Individual: clothes, cape, trim, crown, breastplate, horse |
| Show wardrobes, Team HP bars, Down alerts, Kill counter, Watch friend when down | each player | Off | Off, On |
| Chat time | each player | 3 s | 3-30 s |
| Confirm plans (hold), Vote yes, Vote no | each player | C, Y, N | any key |
| Sprint mode | each player | Hold | Hold, Toggle |
| Extra game keys (move, interact, sprint, call night, command, select 1-4, zoom, pause) | each player | none | any key |
| Count & HP | host | Normal | 10 presets, the last one Custom |
| Enemy type | host | Off | 10 presets, the last one Custom |
| Scale as players | host | Off | Off, Actual, 2 to 8 players; per player Light, Medium, Strong |
| Bosses | host | Normal | Normal, Sturdy, Tough, Brutal, Legendary, Custom |
| Your color | each player | Auto | Auto, Gold, Cyan, Pink, Green, Orange, Blue, Red, White |
| Friend arrow | each player | Off | Off, On |
| Ping key | each player | middle mouse button | any key or mouse button; None |
| Ping at | each player | Cursor | Cursor, Player |
| Chat key | each player | Enter | any key |
| Player list key | each player | the key left of 1 | any key |

**Co-op Settings menu (1.2.7.5).** Esc on the map opens the pause menu, which gets a "Co-op Settings" entry next to
"Copy Invite". The host changes values with < and >; they apply from the next level start, so the difficulty can change
between levels without a new lobby. The host publishes its values in the lobby data (`tfmp_coop`), and the other
players see them read only. The same values are in the BepInEx configuration window (F1). Since 1.2.7.7 the menu has
two columns: the session on the left, "Your settings (only for you)" on the right (colour, friend arrow and the mod's
keys: click the key, press the new one; Esc cancels, Del clears). Inactive rows (a Custom value while its preset is
not Custom) are soft green.

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

**Ping wheel (1.2.7.7).** Holding the ping key opens a wheel: Attack here, Defend here, Build here, Upgrade to 1-4
(the number shows in the marker, e.g. "build the 4th upgrade here"; 1-4 on the keyboard pick it directly), Wait,
Danger (flashes red, 7 s). A tap is the plain ping. The game font has none of the tried symbols (swords, shield, pause),
so the signs are X, D, +, 1-4, II and !.

**Player colours (1.2.7.7).** Each player picks a colour (Auto by default). It travels as Steam lobby member data, no
packet. It paints the character's clothes and cape on every machine, on the map and in levels, and colours that
player's pings, arrow and stats row. The cape is painted the way `WardrobePlayer.SetCapeMaterial` does it
(`_Color2`/`_Color1` on the cape material) but without calling it, because that also stores the choice in the player's
wardrobe prefs. The clothes are the body mesh `Humanoid Model Base/Mesh` (material "Player", `_BaseColor` 5879DB,
`_ColorDim` 003BEC); armour from the game's wardrobe is not painted.

**Friend arrow (1.2.7.7).** Per player: an arrow in the friend's colour at the screen edge, with name and distance,
while the friend is off screen.

**Ready check (1.2.7.7).** When on, the level start screen gets a bar with a Ready button for every player but the host;
the host's Play (New Run and Continue Run) waits for all of them, with a "Waiting for X" dialog that closes by itself.
The same bar lists every co-op setting that is not at its default, so both players see what they are starting.

**Match stats (1.2.7.7).** The host counts, per player, enemy kills and damage dealt by the player's character (in
`Hp.TakeDamage`, HP before and after, attacker under the player's object) and buildings built and upgraded (in the
build confirmation). At victory or defeat it sends the table to everyone. Gold is not counted: the host holds the
shared purse.

**Join during a level (1.2.7.9).** The lobby stays open in a level. A late joiner (or a player rejoining after a
crash) is sent to the map first, gets the host's dawn save of the level, loads the level from it, and the host then
replays the builds made since that dawn (the game's `SaveRun` in the middle of a day writes the dawn state again, so a
day snapshot is not possible). By day this happens at once; at night the joiner waits for the next dawn. The host's own
save file is not written. This needed the level sync per peer and a wait for a running scene transition on the client.

**Revive and respawn (1.2.7.9).** Teammate revive: standing next to a fallen player for the revive time finishes the
game's own AutoRevive wait, so revive perks still apply. Respawn wait and "longer per death" patch the game's
AutoRevive wait; every machine counts the deaths from the synced knock-outs, so all show the same countdown. "Watch
friend when down" points the camera (`CameraRig.overrideCameraTarget`) at a teammate who is up; arrow keys switch.

**Planning, build votes, rebuilds (1.2.8.0).** Every build request passes the host first. With "Plan buildings first"
the first build at a slot becomes a plan: the game's own building preview is drawn there in the planner's colour, with a
plate naming the building, the upgrade and its cost, and the paid coins go back. Building there again, or holding the
Confirm plans key (near a plan: that one; elsewhere: all), builds it. With a build vote the others vote yes / no; the
builder counts as yes; AnyOne, Majority or All decide. "Destroyed stay destroyed" switches on the game's own rule that
skips the dawn repair (`PerkManager.DestructionGodActive`); because the game then leaves the slot unusable, every
destroyed building is offered as an orange "Rebuild" plan for everything paid into it, and "Gold back" returns a share
of that at dawn, so destruction never makes money.

**Endless (1.2.8.0).** The game ends a level when the last wave is reached and done. With Endless on, every machine
appends waves ahead of the current one (the level's last up to four waves without a boss or a placed unit, in a fixed
order, so the lists match), and each extra night adds the chosen percentage to enemy HP and count. The game's own
"Eternal Trial" mode uses a random seed per machine and is not used.

**Looks and HUD (1.2.7.9-1.2.8.0).** Colours per part (clothes, cape, trim, crown, breastplate, horse) or one colour;
the main colour stays the player's colour for pings, blueprints, arrows, stats and chat names. Shared wardrobes: each
player publishes their wardrobe items and materials, and the other kings are dressed with the game's own wardrobe
functions (with the wardrobe manager taken off, so nothing is stored in anyone's wardrobe). Team HP bars, down alerts
with a danger ping, a live kill counter, a night summary at dawn, chat names in player colours and a scrollable chat
history.

**Game keys (1.2.8.0).** An extra key for any game action, read next to the game's input (Rewired), so the game's own
keys and controllers keep working and its control settings stay untouched. The game already toggles sprint with Tab
(Left Shift holds); the mod's Sprint mode Toggle turns the hold key into a toggle too.

**Co-op difficulty (1.2.7.4).** Waves are the single-player waves, so every extra player makes the game easier. The
host picks one entry from each list; the default is the base game. Since 1.2.7.8 the menu shows next to every value
what it does, and a line "In total: ..." under the session settings with what all of it adds up to.

*Count & HP* (how many enemies and how much HP each has; "more enemies" are extra copies of the enemies the wave
spawns):

| Preset | Enemy HP | More enemies |
|---|---|---|
| Normal | 100% | none (base game) |
| Sturdy | 125% | none |
| Tough | 150% | none |
| VeryTough | 200% | none |
| MoreEnemies | 100% | +50% (1 extra for every 2) |
| Horde | 85% | +100% (twice as many) |
| Veterans | 130% | +25% (1 extra for every 4) |
| Hard | 150% | +50% |
| Brutal | 200% | +100% |
| Custom | Custom HP (50-500%) | Custom more enemies (0-300%) |

*Enemy type* (how dangerous each enemy is; "elites" are the game's own elite version of an enemy):

| Preset | Elites | Damage | Speed | Attack speed |
|---|---|---|---|---|
| Off | 0% | 100% | 100% | 100% (base game) |
| SomeElites | 20% | 100% | 100% | 100% |
| ManyElites | 40% | 100% | 100% | 100% |
| Aggressive | 0% | +30% | 100% | 100% |
| Swift | 0% | 100% | +25% | 100% |
| Frenzied | 0% | 100% | 100% | 30% faster |
| Brutes | 0% | +50% | -15% | 100% |
| EliteAssault | 40% | +25% | 100% | 100% |
| Nightmare | 60% | +30% | +20% | 15% faster |
| Custom | 0-100% | 50-300% | 50-200% | 50-200% |

*Bosses* (only bosses, separate from the two lists above): Normal; Sturdy 150% HP; Tough 200% HP; Brutal 300% HP and
125% damage; Legendary 400% HP and 150% damage; Custom (boss HP 50-1000%, boss damage 50-300%).

*Scale as players* (Off by default). It adds a bonus for every player after the first, to enemy HP and to the number of
enemies alike: Light +15%, Medium +25%, Strong +35% per player. The player count is either the real one ("Actual") or
a fixed one from 2 to 8 ("Players4" = as if four played, whoever is really there). The bonus is added to the Count &
HP preset, not multiplied:

- enemy HP = preset HP + (players - 1) x step
- more enemies = preset more enemies + (players - 1) x step

| Example | Enemy HP | More enemies |
|---|---|---|
| Normal, Players2, Medium | 100% + 25% = 125% | 0% + 25% = +25% |
| Hard, Players4, Medium | 150% + 75% = 225% | 50% + 75% = +125% |
| VeryTough, Players4, Medium | 200% + 75% = 275% | 0% + 75% = +75% |
| Normal, Players8, Strong | 100% + 245% = 345% | 0% + 245% = +245% |

Enemy type and bosses are not scaled by players. "+75% more enemies" means exactly 3 extra for every 4: the host adds
0.75 to a counter for each enemy the wave spawns and spawns one extra copy each time the counter reaches 1.

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
python -I games/thronefall/qa/tf_bots.py --playbook coop_extras --level Nordfels
python -I games/thronefall/qa/tf_bots.py --playbook coop_more --level Nordfels
python -I games/thronefall/qa/tf_bots.py --playbook late_join --level Nordfels
python -I games/thronefall/qa/tf_bots.py --playbook coop_big --level Nordfels
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
| `coop_extras` Nordfels (1.2.7.7) | PASS 21/21: everything off by default (game's blue clothes); colours Red and Cyan on clothes and capes on both machines, map and level; friend arrow when 150 m away, gone when near; start-screen bar with the active settings on both; Play blocked until the client's Ready, dialog closes by itself; wheel directions; Attack, Upgrade 4, Danger and Wait pings with sign and label; three nights to defeat, then the same stats table on both (9 kills / 322 damage / 1 built and 1 / 118 / 1). `map_ping` 9/9 and `coop_settings` 15/15 again on this build |
| `coop_more` Nordfels (1.2.7.9) | PASS 14/14: profile saved and loaded; same-spot ping replaced, nearby plates spread; respawn wait 20 s then 30 s on both; revived by the host after 3.1 s twice, the client saw the bar fill; camera watching while down, switch keys; kill counter equal on both and to the host's counts; extra enemies carried gold |
| `late_join` Nordfels (1.2.7.9) | PASS 15/15: client crash after dawn, host builds a tower alone, client restarts and joins straight into the running level, 1 build replayed, shared state equal, host save file hash unchanged, one more day and night together |
| `coop_big` Nordfels (1.2.8.0) | PASS 15/15: part colours and wardrobe seen by the other player; coloured chat names; team HP; plan, vote yes, built; plan, vote no (still planned), vote yes, built; down alert; tower destroyed, summary equal on both, gold back 2 of 3 at 50%, rebuild plan, vote, rebuilt; endless: no match end at the last wave, wave lists equal, +10% on the first extra night |
| Regression on 1.2.8.0 | `coop_more`, `coop_extras`, `coop_settings`, `map_ping`, `late_join`, `rejoin`, `locked_endscreen` all PASS; owner files untouched |
| Real keys and mouse on a visible copy (owner allowed) | 9/10: Esc closes Co-op Settings twice; tap ping; ping wheel up = Attack; wheel + 4 = Upgrade to 4; hold C next to a plan builds it; extra key J calls the night; sprint toggle with an extra key. Injected Left Shift did not reach the game's input, so the toggle on the game's own Shift is unverified |

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
  - chat messages over about 60 characters are not sent;
  - after the first Esc in a mod menu, Esc no longer closes the mod's menus. Workaround: use the Close buttons.
- Co-op in the tutorial level does not work (players die in a loop).
- Without "Join during a level", a joining player who crashes cannot rejoin while the host is in a level: the host goes
  back to the map (pause menu), the friend joins again (a second click may be needed right after the host's map
  loads), the host continues the run from the last dawn. Wait a few seconds after the friend appears before opening a
  level flag.
- Reported once, not reproduced: the joining player did not see the perks the host selected (in the tutorial). The
  owner later confirmed the friend does see them outside the tutorial.
- Not tested in 1.2.8.0: the sprint toggle on the game's own Shift key, controller buttons as keys, the Co-op Settings
  menu with a controller, match stats after a victory and with upgrades, three or more players (watch switching, votes
  with more than two), joining at night (wait for dawn), an endless run played for many nights, Totend's second boss
  stage, a victory run.
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
