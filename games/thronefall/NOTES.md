# Thronefall: working notes

Running log. Observations, derivations, guesses and unknowns are kept apart.

## 2026-11-12: triage

### Game identity (tested copy)
- Steam appid 2239150, buildid 26487600 (update of 2026-11-11, Craaghelm DLC release day)
- thronefall.exe SHA-256 `5645d89e2e5c46896e9ee5a082ee7b3af12333c54c24e0e8ce0b4fa364b5b38d`
- Assembly-CSharp.dll SHA-256 `ea6dab2be5da49ca35d917ea520387edef0b2f2d2023864365e476d98257d9d9`
- Engine: Unity 6000.3.18f1, Mono scripting backend, URP. Windows.
- Bundles Steamworks.NET + steam_api64.dll (so an in-process plugin can use the game's real Steam session, no Spacewar needed). No networking library (no Mirror/Netcode/Photon).

### Observations
- No Thronefall entry in DIRECTORY.md / data/directory.yaml / games/ at commit 1dedd6d.
- Co-op mod lineage (all BepInEx 5 + MonoMod HookGen, Steam P2P via Steamworks.NET, plugin GUID `com.badwolf.thronefall_mp`):
  1. MunWolf/thronefall_multiplayer (BadWolf): original, archived 2024-02-10, 6 open issues. https://github.com/MunWolf/thronefall_multiplayer
     Thunderstore: https://thunderstore.io/c/thronefall/p/Mods_for_the_Throne/ThronefallMultiplayer/
  2. Bwarhness/thronefall_multiplayer: GitHub fork, 1.0.6 to 1.2.2, last push 2026-06-15, issues disabled.
     Thunderstore "ThronefallMultiplayerContinued" by BiggestBlackest, latest 1.2.2.
     https://github.com/Bwarhness/thronefall_multiplayer
     https://thunderstore.io/c/thronefall/p/BiggestBlackest/ThronefallMultiplayerContinued/
  3. Bemno/Thronefall_Multiplayer: standalone repo (not a GitHub fork) carrying the Bwarhness history, created 2026-11-11.
     1.2.4 (v2.13 support, separate MP saves, host save sync, in-lobby updater), 1.2.5/1.2.6 (invites), 1.2.7 on 2026-11-12
     "experimental Craaghelm DLC support". README: Craaghelm tested single-player only; boss phases/randomness, energy
     objects, tunnel ends may not sync. https://github.com/Bemno/Thronefall_Multiplayer
  4. Other forks of MunWolf with no newer work: Finidi-Schoonhoven (2025-10-07), Lewpo, IpsumDominum, thuskey.
- No repo in the lineage has a LICENSE file. BadWolf's archive note invites forks "as long as they give credit" (per Bemno and Thunderstore READMEs).
- Steam announcements: v2.13 QoL update 2026-06-29; Craaghelm DLC (8 new maps) released 2026-11-11.
  https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=2239150
- A local build self-reporting "Thronefall Multiplayer 2.1.1" (origin unknown) logged, on the pre-DLC Unity 2022.3.62 game,
  `MissingFieldException: SceneTransitionManager.levelSelectSceneCraaghelm` on every Play click, plus a HookGen
  return-type mismatch on `RespawnAKnockedOutUnit`. It also references `levelSelectSceneFangmore`, which does not exist in the
  2026-11-11 Assembly-CSharp.
- 2026-11-11 Assembly-CSharp contains `levelSelectScene`, `levelSelectSceneCraaghelm`, `RespawnAKnockedOutUnit`.

### Derived
- Unity jumped 2022.3 -> 6000.3 in the DLC update. HookGen MMHOOK caches made before 2026-11-11 are stale and must be regenerated.
- Path: revive / extend (Bemno 1.2.7 is the newest build and targets this game generation). Not build-from-zero.

### More observations (same day)
- spencatro's GitLab fork https://gitlab.com/catro-creative/thronefall-multiplayer-thirdfork (1.3.4 to 2.1.1, last activity
  2026-08-04; adds veto cards, endless/challenge modes, Steam Deck installer, CI test builds). Its 2.1.1 release DLL
  (SHA-256 79cfeb34...8dbe) references `SceneTransitionManager.levelSelectSceneFangmore`, absent from buildid 26487600, so it
  cannot JIT `IsLevelSelectScene` on this build. README: no license, same "fork with credit" terms.
- Bemno 1.2.7 release: DLL sha256 7a959a4a...35ad, zip 62694bab...62be (match GitHub's published digests). Zip bundles the same
  BepInExPack_Thronefall 5.4.2100 (BepInEx 5.4.21). Release notes: "If you want something known to work online, stay on 1.2.6."
  1.2.7 renumbers build slots, so it cannot play with older builds.
- Mod transport (1.2.7 source): Steam lobbies (SteamMatchmaking) + SteamNetworkingMessages, ~35 Steamworks.NET calls,
  9 callback types (LobbyCreated/Enter/MatchList CallResults; GameLobbyJoinRequested, LobbyChatMsg, LobbyChatUpdate,
  LobbyDataUpdate, SteamNetworkingMessagesSessionRequest/Failed). Host-authoritative. No test framework (its AGENTS.md:
  "clean build is the test suite").
- Game saves: `[SaveServices] Using SteamSaveServices`: saves go through SteamRemoteStorage.FileWrite/FileRead (Steam cloud
  of the running app id), not only files. Match saves via `MatchSaveLoadHandler.SavePath`.
- `thronefall_Data/app.info` product rename does NOT change persistentDataPath or the PlayerPrefs key on Unity 6000.3
  (test run 1 wrote analytics into the real LocalLow folder and PlayerPrefs into the real registry key; both restored from backup).
- Renaming productName inside `globalgamemanagers` (byte offset 1452 in this build, 10 chars, same length) DOES move
  LocalLow and the registry key. Combined with `steam_appid.txt` = 480, the owner's saves, PlayerPrefs and Steam cloud
  manifest hashed unchanged across runs 2 to 5. The game boots under app id 480 (no ownership gate seen up to the main menu).
- BepInEx 5.4.21 loads on Unity 6000.3.18f1. HookGenPatcher needs MonoMod.exe + MonoMod.RuntimeDetour.HookGen.exe in the game root.
- Mod 1.2.6 on buildid 26487600: `MissingFieldException: UnityEngine.Material TMPro.TMP_Asset.material` in `BaseUI.Create`,
  so its multiplayer UI does not build (Unity 6 moved TMP into UGUI).
- Mod 1.2.7 on buildid 26487600: loads to main menu with no errors (only "DontDestroyOnLoad" and "only one Event System" warnings).
- Two copies (A plain copy, B hard links + private BepInEx/globalgamemanagers/steam_appid) run at the same time to the
  main menu with 1.2.7. `boot.config` has `single-instance=` (empty) and does not block the second copy.

### Two Steam accounts on one PC (Sandboxie-Plus 1.18.8)
- Box `Steam2`: `OpenFilePath=D:\coop-test\thronefall\B\*` (so copy B's logs land in the real folder) and
  `StartService=Steam Client Service`. Without StartService the boxed Steam logs `Failed to connect to Steam Service (GLE 183)`,
  shows "Steam Service requires some maintenance", and exits on Cancel. With it, the box runs its own SteamService and the
  boxed Steam stays up. Harmless leftovers: "Failed to add firewall exception" console, port 27036 bind failures (LAN discovery).
- Owner signs the second account in once inside the box ("Remember me"); no password passes through the agent.
- Copy A (normal Steam) and copy B (started with `Start.exe /box:Steam2`) both run as app 480 on two different accounts
  (Steam gameprocess_log.txt of each client lists its own PID).

### Connect test: PASS (2026-11-12 19:35, mod 1.2.7, buildid 26487600)
- `games/thronefall/qa/run-connect.ps1` + test-only plugin `games/thronefall/qa/CoopQaDriver` (`-coopqa host|join`).
- Host clicks the mod's Host button via the ButtonControl.OnClick backing field, writes the lobby id to a file; joiner calls
  `Network.ConnectLobby`. Both reached `_LevelSelect` with players=2, peers=1, identical player positions, errors=0, in ~10 s.

### Bug: rejoin hangs after the first session (RecentPartners poisons the Steam session), FIXED in local build 1.2.7.1
- Symptom: first-ever connect between two accounts passes; every later connect fails. Client logs
  `SendMessage to <host> returned error 'k_EResultConnectFailed'` every 2 s forever; host never sees the approval.
- Repro (2026-11-12, mod 1.2.7, two accounts, run-connect.ps1): `BepInEx/config/thronefall_mp_recent.txt` present on
  both copies: FAIL 7/7. File removed: PASS 2/2 (and the run recreates the file).
- Root cause (from Steam's own networking debug log, enabled by the QA driver): `RecentPartners` (added in Bemno 1.2.5)
  sends "TFMPASK1" to remembered partners on channel 92 every 15 s while offline, starting during `_Boot`. Both copies open
  a SteamNetworkingMessages session to each other at startup; it ends "closed by peer (5003): Timed out attempting to
  connect". A messages session is per peer and shared by all channels, so the join approval (channel 0) later goes into
  the failed session and returns k_EResultConnectFailed; nothing ever calls CloseSessionWithUser for it.
- Player impact (derived): two friends who played once, one starts the game before the other hosts, then joins: stuck on
  "Connecting" until restart.
- Fix (27-line patch, local only: `local/patches/0001-...patch`, working copy D:\coop-test\src\tfmp): `Network.Connect`
  closes any session to the host before the approval handshake; `Network.ResetIfConnectFailed` closes the session when
  any send (Network, RecentPartners, SaveSync, ModUpdater) returns k_EResultConnectFailed.
- Verified: same condition (partner files present), fixed build 1.2.7.1 (sha256 ed8fe450...f878): PASS 5/5, client
  ConnectFailed=0, one session reset per run.
- Side finding, unverified impact: `Network.OnSessionRequest` has `if (_peers.Count < MaxPlayers) { "Server Full"; return; }`
  (inverted). MaxPlayers appears to stay 0, so the branch never runs today. Compiler also flags Harmony003 in
  ManualAttackPatch (`__state` fields assigned on a non-ref parameter, assignments have no effect).

### Gameplay bot (2026-11-12 evening)
- `games/thronefall/qa/CoopQaDriver` (test-only BepInEx plugin, inert without `-coopqa`): JSON control port, virtual
  controller as Harmony prefixes on `Rewired.Player.GetButton/GetButtonDown/GetButtonUp/GetAxis(string)` ("Interact",
  "Call Night", "Move Horizontal/Vertical"), so the game and the mod read the bot like a gamepad. Tasks: open level flag,
  build slot (hold Interact until the level rises), call night. UI by name: `ui`, `ui_apply`, `pick_weapon`, `press_play`.
- Why Rewired level: the mod's `PlayerInteractionPatch.RunInteraction` reads Interact from Rewired directly, not from
  `PlayerNetworkData.SharedData`, so overriding SharedData alone does not interact.
- Bot shortcuts (not what is tested): teleport to a level flag's interaction collider (it sits ~15 m from the flag, at the
  game's `PlayerTeleportPosition`) and to the edge of a far build slot's interaction zone; `-coopqa-unlock` patches
  `LevelInteractor.CanBePlayed` to true because fresh test profiles only have the tutorial.
- Observed game rules: night call needs `PlayerInteraction.focussedInteractor == null` (step off buildings first); the level
  flag's first page shows Play, or New Run / Continue Run when the host has `mp_<level>.json`.
- Saves (observed in the isolated test folder): host writes `ThroneSave.sav` and `mp_Nordfels.json` into
  `LocalLow\Grizzly Games\<product>`; the client writes no match save. Nothing appeared under Steam userdata for app 480.
- `tf_bots.py` runs a Thronefall playbook on the hub runner (`tools/bots/coopbots/core.run_playbook`), settle + shared
  snapshot compare after every step, game error count as `apply_failures`.
- RESULT 2026-11-12 20:37, build 26487600, mod 1.2.7.1, two Steam accounts: `day_night` Nordfels 2 nights PASS 10/10
  steps (host and client build in turns, host calls night 1, client calls night 2, enemy count difference 0 during both
  nights, building HP equal at both dawns, no game errors). Evidence: D:\coop-test\thronefall\runs\bots-20261112-203659.

### Bug: Continue Run gives the client an empty map (save sync race), FIXED in local build 1.2.7.2
- Symptom: host continues a saved run (castle + 5 houses, wave 1); client enters the same level empty (no buildings,
  wave -1). Shared gold still syncs, the world does not; the runner's settle fails ("machines did not converge").
- Root cause (logs + IL): the game reads the match save only in `LevelSelectUIFrameHelper.OnShow` -> `TryLoadRun`
  (the only caller). On a client that page opens when the host opens the flag, before the host sends its save (the host
  sends at level request). The client logged "no host save received ... using local file" and then, after
  "Transitioning", "received Nordfels (save)": too late, and the save travels on its own channel (93) so the level-sync
  packet can overtake it.
- Mirror problem found while fixing: for New Run the host still sent its old file (the presser's OverwriteSave was not used
  for normal entry), so a client could load the old run while the host started fresh.
- Fix (local commit 912aa1d, `local/patches/0001-Clients-load-the-host-s-match-save-...patch`): `SaveSync.ApplyHostSave`
  runs `TryLoadRun` with the host's save as soon as it arrives in a level select map; `LevelDataSync.HandleSyncPacket`
  enters a level only after the host save for it arrived (at most 5 s); `SendSave(map, fresh)` sends "no save" for New
  Run; client drops saves from earlier visits when back in a level select map.
- A/B, same saved run, full restart of both games before each run (2026-11-12 ~20:50):
  1.2.7.1 save_continue FAIL 3/3 (client empty map); 1.2.7.2 save_continue PASS 3/3 (buildings, wave, time equal to
  the saved dawn on both; gold equal on both). New Run with an existing host save on 1.2.7.2: day_night 1 night PASS
  7/7, client logged "applied host fresh start".
- Observation, not a co-op issue as far as known: the dawn save holds the balance before the morning coins are collected
  (host logs "Restoring dawn-save balance 6"); whether vanilla gives the uncollected coins back on continue: unknown.

### Level to the end (2026-11-12 ~21:00, 1.2.7.2)
- `day_night --nights 0` Nordfels (13 waves): PASS 19/19 steps; the bot's economy ran dry and the castle fell on night 5:
  AfterMatchDefeat on both at once, same "After Match Frame". Clicking through (Level Up Frame "Continue Reward Button",
  After Match Frame "Back Button") returned both to `_LevelSelect`, still connected (2 players).
- Leaving the level after the defeat: burst of `NullReferenceException` in `PathfindMovementEnemy.Update` (game code, no
  mod frame in the stack): ~700 on the host, 13 on the client, then none in the next 10 s. Effect: none seen.
- Victory: not reached (needs upgrade choice dialogs).

### Crash and rejoin (2026-11-12 ~21:55, 1.2.7.3): `rejoin` PASS 12/12
- Client killed mid-match: host keeps the match (players 1, InMatch, errors 0).
- Restarted client cannot join while the host is in a level: lobby not joinable outside level select
  (`Network.OnSceneChanged` -> `SetLobbyJoinable`), client hangs in join. By design of the mod.
- Host back to the map (`InMatchResignHelper.TryBackToMap`), client joins (Steam refused the first click right after the
  host's map loaded: "Failed to join lobby with code 3"; second click worked), host Continue Run: both at the saved dawn.
- Bug found on the way, in my fix 1 (1.2.7.1/1.2.7.2) together with a mod weakness: `Connect` closed a working session
  to the host; the host, busy loading the map ("SteamNetworkingSockets lock held for 904ms", "Ignoring P2P signal ...
  unknown remote connection"), let the new connection time out; `OnSessionFailed` then dropped the host and switched the
  client offline while `GetApproval` kept resending, so the host authenticated a player ("ghost", players=2 on the host)
  whose client sat in the main menu with "join timed out". Fix (local commit d8ceb10, patch 0003): close only
  ClosedByPeer/ProblemDetectedLocally sessions in `Connect`; while awaiting approval a failed session to the host is
  closed and retried. After the fix: rejoin PASS, connect with partner files PASS 3/3 (ConnectFailed 0), save_continue PASS.
- Observed timing races in the rejoin flow (bot opened the level flag in the same second the client finished joining):
  once the client's loadout popup closed right after opening and the close propagated to the host; once (1.2.7.3 before
  the pause) the client stayed in `_LevelSelect` while the host loaded the level. With a 5 s pause after the join (like a
  person) the flow passed. Root cause of the second one: unknown (not reproduced with the scenestate capture).
- Hub tool fix: `tools/bots` `Ctx._tick` polled machines in `ctx.absent`; with a killed real game copy every wait failed.
  Now polls `ctx.present`; test `tests/test_absent.py`, twin `pollabsent`; bots suite ALL OK, 16/16 twins.

### Owner's first real session (2026-11-12 evening, 1.2.7.3) and fixes 4 to 6 (1.2.7.4)
- Reported by the owner: the friend's game forced the tutorial; co-op in the tutorial did not work ("Hoppla, du bist
  gestorben" loop). Owner log: "Main save file not found" (this build finds no progress, only the tutorial unlocked).
- Fix 4: `PacketHandler.HandleLoadoutOpen` ignored the host's open for a level locked on the client ("LoadoutOpen for
  locked level ..., ignoring"), so the client had no popup and no weapon pick. A client now follows opens from the host.
  `locked_endscreen` with host-only unlock (`run-connect.ps1 -UnlockHostOnly`): 1.2.7.3 FAIL at step 2 (log line above),
  1.2.7.4 PASS (client log "locked here but opened by the host: following").
- Reported by the owner: after a victory the score screen stayed on screen, even in the main menu. Owner log at the
  transition: `activeFrame=Level Up Frame`: the friend pressed Back first and pulled the host out mid end screen.
  Fix 5: `SceneTransitionManagerPatch.Transition` closes the active frame and the frames below it on the stack
  (`CloseAllFrames` alone left the After Match Frame visible: seen in a test run). `locked_endscreen` PASS 15/15 twice
  (host: `open []` after being pulled to the map; log "Closing open UI frame 'Level Up Frame' (+1 stacked)").
- Fix 6: `Buffer.EnsureSize` doubled once; strings longer than the buffer threw "Destination array was not long
  enough" (found through a ~90 character server chat message; player chat over ~60 characters had the same problem).
- Perks: the owner reported the friend could not see the selected perks (in the tutorial). Not investigated with a test;
  26 equippables in the owner's log are "not in the equipment table" and get runtime ids, which may differ between
  machines with different unlocks (suspicion, unverified).

### Co-op difficulty (1.2.7.4, owner's request)
- Host-only settings (F1, BepInEx ConfigurationManager): CountAndHp (Normal, Sturdy, Tough, VeryTough, MoreEnemies,
  Horde, Veterans, Hard, Brutal, Custom), EnemyType (Off, SomeElites, ManyElites, Aggressive, Swift, Frenzied, Brutes,
  EliteAssault, Nightmare, Custom), ScaleAsPlayers (Off default, Actual, Players2..8) with ScalingPerPlayer.
- The host sends HP, damage, speed and attack-rate factors, the elite roll and extra spawns in `EnemySpawnPacket`
  (wire format changed); extra enemies have Coins 0 and do not advance `spawnedUnits`.
- `difficulty` playbook, Nordfels night 1, every enemy compared host vs client (maxHp|speed|elite|damage):
  Normal: 3 enemies, HP 25, speed 5, damage 1.0. Brutal+Nightmare: 6 enemies, HP 50 (elites 200), speed 6.0, damage
  1.3 (elites 3.9), 2 elites. Custom 150%/+50% + Custom damage 200% + ScaleAsPlayers 8 Light (+105%): 7 enemies, HP 63.8
  (= 25 x 2.55), damage 2.00. All PASS, host and client identical, 0 game errors.

### Co-op Settings menu, night vote, boss difficulty (1.2.7.5, 2026-11-13)
- Boss survey (`qa/boss_survey.py`, every level's waves): all bosses come through the normal wave spawn path. Frostsee
  wave 12 "BOSS FIGHT" 15000 HP; Moorweg wave 11 Ghostqueen 15000; Freifort wave 12 Iron Castle 66000; Totend wave 14
  Corrupt King 66666 + second stage 70000. **Correction:** before 1.2.7.5 the CountAndHp/EnemyType factors were also
  applied to bosses (the write-up said bosses were unaffected). 1.2.7.5 detects bosses (Boss tag in the prefab) and
  gives them only the BossDifficulty factors.
- Menu: Esc on the map > "Co-op Settings" (a clone of "Copy Invite"). Host edits, clients read the lobby data
  `tfmp_coop` directly (a copy refreshed every second was stale in the first run).
- `coop_settings` playbook PASS 15/15: client read only; night vote (host alone stays Day, both ready -> Night on
  both); Frostsee boss 15000 (Normal) -> 30000 (Tough) on host and client after a change in the same lobby.

### Map ping and key bindings (1.2.7.6, 2026-11-13, owner's request)
- Host setting "Map ping" (Off default) in Co-op Settings; clients read it from the lobby data (key `ping`).
- Ping key per player (default middle mouse), "Ping at" Cursor or Player, plus rebindable chat (Enter) and player list
  (`) keys: "Your keys" in Co-op Settings (click, press a key; Esc cancels, Del clears) or F1.
- `MapPingPacket` (new PacketId at the end of the enum), client -> host -> others (ShouldPropagate); the host stamps
  the sender's player id. Marker: screen-space UI (no shaders), clamped to the screen edge, 5 s, max 3 per player.
- Cursor ping: ground plane at the player's height, or the first static collider hit within 8 units of that height
  (CharacterController/Rigidbody hits skipped). Seen: one cursor point landed at y 6.1 (a collider above ground).
- With minimized test windows `Input.mousePosition` is not meaningful (the first run's "cursor" ping fell back to the
  player's feet), so the test pings at given screen points and checks the round trip (WorldToScreenPoint within 0.03).
- First screenshot: name label unreadable (thin font, outline) and the ping hidden under the host's Invite dialog.
  Fixed: bold 28 px name on a dark plate, bigger marker, ping layer moved to the top when a ping arrives.
- `map_ping` playbook PASS 9/9 (off by default sends nothing; host turns it on, client read only; both rebind keys;
  Ping at Player lands on the pinging player; two cursor points land under the cursor and on the same spot on both).
  `coop_settings` regression PASS 15/15 on the same build. Owner files untouched.
- Not tested: real key presses (bots call the same TryPing the key calls; the capture loop uses Input.GetKeyDown),
  controller buttons as ping key, the ping on the overworld map.

### Ping kinds, friend arrow, player colours, ready check, match stats (1.2.7.7, 2026-11-13, owner's request)
- Player colour: Steam lobby member data `tfmp_color` (0 Auto, 1-8 palette), no packet. The cape is painted the way
  `WardrobePlayer.SetCapeMaterial` does it (`capeSMR.material` `_Color2` main, `_Color1` accent, also
  `capeMR_DeadStatic`), but without calling it: `SetCapeMaterial` also calls `PlayerWardrobeManager.SetMatIndex`,
  which would store the choice in the player's wardrobe prefs. Observed: the map character and the level character
  both have a `WardrobePlayer`; colours equal on both machines, capes painted on both.
- Friend arrow (per player): screen-edge chevron towards each off-screen player. On the overworld the camera is zoomed
  out: a player 60 m away is still on screen; 300 m east of the start is outside the map and the game puts the player
  back. 150 m east gave an arrow with direction (0.78, 0.62).
- Ping kinds: `MapPingPacket` gained a kind byte (Look, Attack, Defend, Build, Upgrade1-4, Wait, Danger). Hold the ping
  key 0.25 s for the wheel (9 choices clockwise from the top, dead zone 45 px, 1-4 pick an upgrade). The game font has
  none of the tried symbols (crossed swords, multiplication sign, shield, pause), so the signs are X, D, +, 1-4, II, !.
- Ready check: `ReadyPacket` client -> host -> others (host stamps the sender); the host's gate in
  `LevelStartGatePatch` now also covers Continue Run (any popup frame), the weapon check still only the grid. Marks are
  cleared when the start popup opens or closes (debounced 1 s, page changes leave no active frame for a moment). The
  host's "Waiting for X" dialog stayed open for a whole match in the first run; it now closes once nobody waits.
- Match stats: counted on the host in `HpPatch.TakeDamage` (HP before/after, kill = HP crossed 0, target tagged
  EnemyOwned, attacker = a TaggedObject under a player's character) and `HandleConfirmBuild` (level 0 built, else
  upgrade). Sent once when `LocalGamestate` reaches AfterMatch*. No "unknown source" player damage was logged in the
  test, so arrows/weapon hits resolve to the character. The host keeps counting a moment after the end (the panel
  showed 322 damage, the host's live count later 332); the table is the snapshot at the end.
- Start-screen bar: `CoopDifficulty.Summary` lists rows that differ from their default (sub-rows only when their parent
  differs), from the host's own values or the lobby data.
- `coop_extras` playbook PASS 21/21 (defaults off; colours and capes on map and level; arrow; bar text on both;
  Play blocked until Ready, dialog closes; wheel directions; four ping kinds with sign and label; build, play three
  nights to defeat; stats table equal on both: 9 kills/322 damage/1 built and 1/118/1). Regressions `map_ping` 9/9 and
  `coop_settings` 15/15 on the same build. Owner files untouched.
- Not tested: real key hold for the wheel (bots call the same functions), the stats after a victory, upgrades in the
  stats (bots only built), colours with three or more players.
- Owner feedback: the character itself did not change colour (only the cape). Found by dumping every renderer under
  the player object: the default wardrobe has no armour (`Armour` inactive); the king's clothes are one mesh,
  `Alive Visuals/Horse_LOD1/Rig/root/body/Humanoid Model Base/Mesh`, material "Player" (FlatKit/Stylized Surface,
  `_BaseColor` 5879DB, `_ColorDim` 003BEC, same hue, full saturation). The death poses have their own copies. Painted
  with the chosen colour and a dim of the same hue (saturation x1.7, value x0.85). `coop_extras` PASS 21/21 with body
  checks (blue by default, the chosen colour on map and level on both machines).
- Owner report: Esc did not close Co-op Settings. Cause already in Bemno 1.2.7 (the baseline has the field and no reset): `UIManager.ExitHandled` was a static
  field set to true by any panel that used Esc and never reset, so every later Esc was ignored. Now it is true only
  in the frame it was set. Not tested with a real key press (the bots cannot press Esc).

### Explanations in the menu (1.2.7.8, 2026-11-13, owner's request)
- Every session row can carry a `Detail` (value + all values -> text), computed from the same `Values(...)` tables the
  spawns use, so the text cannot drift from the effect; clients compute it from the lobby data. Shown right of the
  arrows. A line "In total: ..." under the session column adds presets and player scaling exactly like
  `HpScale = (hp + bonus) / 100` and `ExtraPerSpawn = (extra + bonus) / 100`, bonus = (players - 1) x step; enemy type
  and bosses are not scaled (bosses take only `BossHpScale`, no extra copies).
- "+X% more enemies" is exact on average: `EnemySpawnerPatch` adds ExtraPerSpawn to a carry for every enemy the wave
  spawns and spawns one copy each time the carry reaches 1 (+75% = 3 extra for every 4).
- Checked by cycling every preset on the host copy: e.g. VeryTough "enemy HP 200%"; Hard + Players4 Medium "In total:
  enemy HP 225% (150% + 75% scaling), 125% more enemies (50% + 75% scaling)"; VeryTough + Nightmare + Players4 + Brutal
  "enemy HP 275% (200% + 75% scaling), 75% more enemies (0% + 75% scaling), 60% elites, damage +30%, speed +20%,
  attack +15% faster. bosses HP 300%, damage 125%". `coop_extras` now also checks that host and client show the same
  explanation and total line.

### Join during a level, revive, respawn wait, profiles, kill counter (1.2.7.9, 2026-11-13, owner's request)
- **Join during a level** (host rule): the lobby stays joinable in a level. Level sync is now per peer
  (`LevelDataSync.CaresAboutPeer`), so a late joiner is first told "_LevelSelect", reports in from the map
  (`LateJoinPacket`), gets the host's dawn save of the level (`SaveSync.SendSaveTo`), then the level name.
  Findings on the way:
  - `MatchSaveLoadHandler.SaveRun()` in the middle of a day writes the dawn state again (4106 chars, same as the
    dawn file), so a day snapshot is not possible that way. Instead the host records every confirmed build since the
    last dawn save and replays them to the late joiner once it reports the level loaded; the client applies a build
    only if the slot is still at that level (`BuildSlot.Level == packet level`), so duplicates do nothing.
  - With one state per peer, `BaseSync.Handle` stores RequestLevel/WeaponResponse packets as the peers' last packet,
    and `LevelDataSync.Compare` cast them to `SyncLevelDataPacket` (InvalidCastException every frame, nobody entered
    levels). Compare now treats a foreign packet as "changed".
  - A client that has just loaded the map still has `SceneTransitionIsRunning`; `Transition` then only queues the
    level, and the queue runs at the next transition (never). `TransitionAfterHostSave` waits for the running one.
  - The late-joiner set must hold everybody who is with the host on the map (updated every frame there), or players
    who came in normally are taken for late joiners during the host's transition.
  - At night the joiner waits; it is admitted 3 s into the next day (after the dawn save).
  - `late_join` PASS 15/15: client crash after dawn, host builds a tower alone, client restarts and joins into the
    running level, 1 build replayed, shared state equal, host save hash unchanged, one more day and night together.
- **Respawn wait / longer per death** (host): Harmony postfix on the private getter
  `AutoRevive.ReviveAfterBeingKnockedOutFor` (must sit in a class marked `[HarmonyPatch]`: CreateAndPatchAll ignores
  method-level attributes in unmarked classes; the first run showed the game's 11 s). Deaths per player per level are
  counted on every machine from the synced knock-outs, so host and client compute the same wait: 20 s then 30 s with
  20 s + 10 s.
- **Teammate revive** (host): a teammate within 3.5 m of a downed player for the revive time; the host sets
  `AutoRevive.hasBeenKnockedOutFor` to the wait, so the game's own AutoRevive revives (perks included). Revived after
  3.1 s with 3 s set, twice; the client saw the bar fill (0.07 ... 0.88). Note: AutoRevive revives at once by day, so
  downs only matter at night.
- **Watch friend when down** (per player): `CameraRig.overrideCameraTarget` = a teammate who is up, only if nothing
  else uses the override; cleared on revive.
- **Kill counter** (per player): host sends kills per player once a second when they change; equal on both and to
  the host's counts.
- **Gold from extra enemies** (host, 0-200 %, default 0): a share of the copied enemy's coins with the remainder carried
  over; 2 extra enemies carried 2 coins at 100 %.
- **Separate per-player scaling** for enemy damage, boss HP and boss damage (Off/Light/Medium/Strong/Custom), added like
  enemy HP; the "In total" line shows it ("damage +20% (0% + 20% scaling)").
- **Profiles**: 5 slots in `BepInEx/config/thronefall_mp_profiles.txt`; save Tough, change, load: Tough again.
- **Pings**: a ping within 2 m replaces the old one; overlapping name plates move down 42 px; older pings shrink and fade.
- Menu: difficulty left (with profiles and the total line), rules and own settings right; fits at 1440p.
- Regression on 1.2.7.9: `coop_extras` 21/21, `coop_settings` 15/15, `map_ping` 9/9, `rejoin` 12/12,
  `locked_endscreen` 15/15. Owner files untouched.
- Infrastructure: after a PC/Steam restart the sandboxed second Steam is not running and copy B logs
  `SteamAPI_Init() failed` (connect fails with "no peer joined"); start it again with `Start.exe /box:Steam2 steam.exe`.

### 1.2.8.0 (2026-11-13, owner's request): planning, votes, rebuilds, endless, wardrobes, game keys
- **Destroyed stay destroyed**: `PerkManager.DestructionGodActive` (the game's own rule, read only in
  `Hp.ReviveAllKnockedOutPlayerUnitsAndBuildings` at dawn) forced on. Experiment: the slot then stays `Built` with a dead
  building and cannot be focused again, so it is not rebuildable by itself. Resetting a slot to level 0 mid-match is
  unsafe (other slots depend on it: activators, walls), so destroyed buildings are listed as "Rebuild" plans that
  revive the building (`Hp.Revive`, remove `AUTO_NoReviveNextMorning`, clear `knockedOutTonight`) for everything paid
  into it (`sum upgrades[0..Level-1].cost`). Gold back at dawn = share of that sum; with rebuild costing the full sum
  there is no gold loop.
- **Planning / build vote**: every build passes `BuildPlanner.OnBuildRequest` on the host (client: in
  `HandleBuildOrUpgrade`, refused requests get `CancelBuild`; host: in the `OnUpgradeChoiceComplete` hook). Finding: the
  game's cancel (`OnUpgradeChoiceComplete(null)` -> `OnUpgradeCancel`) does not give the host back what it paid while
  holding (8 -> 5 gold for a 3 gold castle), so the hook refunds `upgradeSelected.cost`. Confirmed plans are built by
  the host with `ConfirmBuildPacket` and an explicit gold charge. Vote results were overwritten by the host's next
  "no vote" broadcast in the same frame; results now stay 3 s. Blueprints: the game's own preview
  (`Graphics.DrawMesh(UpgradePreviewMesh, MainMesh matrix, previewMaterial)`) with a per-player tinted material copy.
- **Endless**: `EnemySpawner.MatchOver` is `wavenumber >= waves.Count - 1 && !SpawningInProgress`; every machine
  appends waves ahead (the last up to four waves without a boss or placed unit, fixed order) so the last wave never
  comes; host and client lists match (checked: 13 waves, at wave 12 count 16, MatchOver false on both). Each extra
  night adds the ramp to HP and count (first extra night +10% at Steady). The game also has an "EternalTrial" mode with
  a seeded wave generator (`EternalWaveGenerator`, random seed per machine), not used.
- **Wardrobes**: `WardrobePlayer.Set*` store the choice through `PlayerWardrobeManager` only if that field is set; for
  other players' kings it is nulled during the call. Each player publishes item and material indices as lobby member
  data. Colours per part: clothes ("Player" material), cape `_Color2`, trim `_Color1`, crown / armour objects of the
  WardrobePlayer, horse ("Horse" material).
- **Game keys**: the game's Rewired keyboard map (read with the driver's `bindings`): Sprint = Left Shift, **Sprint
  Toggle = Tab (already in the game)**, Interact / Call Night = Space or left mouse, Command Units = R / Left Ctrl /
  mouse 3, Select All Army/Melee/Ranged/Heroes = 1-4, Smart Command = F / Q, Zoom = Z, Lock Target = Left Alt. Extra
  keys are added in postfixes on `Rewired.Player.GetButton/GetButtonDown/GetButtonUp/GetAxis(string)`; nothing in the
  game's control maps is changed.
- QA: `coop_big` PASS 15/15 (part colours and wardrobe seen by the other player, coloured chat names, team HP, plan ->
  vote yes -> built, plan -> vote no -> still planned -> vote yes -> built, client down alert, tower destroyed ->
  summary on both + gold back 2 of 3 at 50% -> rebuild plan -> vote -> rebuilt, endless). Regression on the same build:
  `coop_more`, `coop_extras`, `coop_settings`, `map_ping`, `late_join`, `rejoin`, `locked_endscreen` all PASS.
- Real keys and mouse (owner allowed), visible host copy: Esc closes Co-op Settings twice (the game then shows its
  pause menu again, one more Esc closes it), tap middle mouse = ping, hold + mouse up = Attack, wheel + key 4 =
  Upgrade to 4, hold C next to a plan builds it, extra key J calls the night, sprint toggle with an extra key: PASS.
  Synthetic Left Shift (keybd_event, VK 0xA0 and 0x10) did not reach Rewired, so the toggle via the game's own Shift
  is unverified with injected input.

### Lead (not proven player-facing): host with no weapon equipped hangs the level start
- Seen with a fresh profile: host on the level's first page (Level Select Frame) presses Play/Start directly; host's pick is
  `Invalid` (nothing equipped), `LevelDataSync` falls back to `WeaponRequestPacket` and also sends it to the host itself;
  the host logs `Received unknown packet (type: 'WeaponRequest')` every resend and never starts the level.
- Unknown: whether a real player reaches this (players with an equipped weapon get a pick when the popup opens).

### Unknown
- Game version number of buildid 26487600 ("2.15" string seen in globalgamemanagers, unconfirmed).
- Whether BepInExPack_Thronefall 5.4.2100 (BepInEx 5.4.21) loads on Unity 6000.3.18f1.
- Whether Bemno 1.2.7 was built against buildid 26487600.
- Whether base-game maps work online with 1.2.7; whether Craaghelm maps work online at all.
