# Superflight: working notes

Dated log. Observations, derived facts, guesses and unknowns are kept apart. Owner's
private details (install path, hashes of the owner's copy) live in `local/`, not here.

## 2026-10-29: triage and research

### Game identity (observed)

- Superflight, GrizzlyGames, released 2017-11-08, Steam app id 732430, Windows only
  ([Steam store](https://store.steampowered.com/app/732430/SUPERFLIGHT/)).
- Steam build id 2304714 (appmanifest, last updated 2025-07-13). `superflight.exe` is a
  PE32+ x64, 652,288 bytes, SHA-256
  `1d9a942b064781d5345f5c8fb99d0c9673fbeacd346312090a3292b7f45275ea`
  (coop_mcp `game_identity`).
- Engine: Unity 2017.2.0 (UnityPlayer.dll file version 2017.2.0.4644257), Mono scripting
  backend: `<game dir>\superflight_Data\Managed\Assembly-CSharp.dll` (333,824 bytes) and
  `Assembly-CSharp-firstpass.dll` (283,136 bytes), no GameAssembly.dll. coop_mcp
  `detect_engine` says "Unity (Mono)", hooking tools BepInEx or MelonLoader, dnSpyEx, Harmony.
- Shipped managed assemblies that matter for co-op: `UnityEngine.Networking.dll` (UNet
  HLAPI, 254,464 bytes, contains `NetworkServer`/`NetworkClient`) and
  `UnityEngine.UNETModule.dll`. Steamworks.NET is compiled into
  `Assembly-CSharp-firstpass.dll` (type names `SteamNetworking`, `SteamMatchmaking`,
  `SteamFriends`, `SteamUser`, `SteamUserStats` present as strings); native side is
  `superflight_Data\Plugins\CSteamworks.dll` + `steam_api64.dll`. The game's own code
  (`Assembly-CSharp.dll`) references `SteamFriends`, `SteamUser`, `SteamUserStats` only.
- No anti-cheat seen. Online features: Steam leaderboards and achievements only; the store
  page lists "Online multiplayer" under what the game doesn't have.

### Prior work in this repo (observed)

- `DIRECTORY.md`, `games/`: nothing for Superflight. GitHub search of issues and PRs in
  cranebeaker/coop-anything for "superflight": 0 results (2026-10-29, via the GitHub
  search API, `gh` not installed on this PC).

### Existing attempts found (observed, with sources)

1. **SFMF Multiplayer by Phlarfl** (the only co-op/multiplayer attempt found).
   - Source: https://github.com/Phlarfl/SFMF/tree/master/Multiplayer/Multiplayer
     (no license file in the repo). Commit "Multiplayer mod" 2018-12-29; the only later
     commits are "Fixed Launcher" and "Update Launcher" on 2022-01-27.
   - Release: `Multiplayer.dll` (18,944 bytes) tagged `multiplayer.1.0.0`, published
     2022-01-27, 120 downloads
     (https://github.com/Phlarfl/SFMF/releases/tag/multiplayer.1.0.0). Download page
     https://phlarfl.github.io/SFMF/ links only `SFMFLauncher.v1.2.0.zip` (2017-12-24).
   - Steam thread "Multiplayer Mod Progress"
     (https://steamcommunity.com/app/732430/discussions/0/1489992080516642412/):
     progress posts 2017-11-27 (host, join, shared world, all players shown) and
     2017-12-02 (menu, IP:port, smoothed movement); "Not currently, but I'll be back some
     day to finish this" 2018-09-18; source released 2018-12-29 so others can finish it;
     "Released BETA multiplayer" 2022-01-27; one user (drak) got the launcher to run and
     the mod to install on 2022-01-27 after a re-download, no play report; asked on
     2024-12-23 whether it still works, Phlarfl replied 2025-01-05 "I don't think it
     would still work."
   - What it syncs (from reading the source, see audit below): remote players as
     coloured spheres with name and score tags, host's world seed so everyone flies the
     same map. Nothing else (the game has no enemies, items or story).
   - Networking: Unity UNet HLAPI (`NetworkServer.Listen(port)`, `NetworkClient.Connect(ip,
     port)`), direct IP, default port 4444, host plays too (`ClientScene.ConnectLocalServer`).
   - Forks of Phlarfl/SFMF: one (xxx-lgm/SFMF, pushed 2018-12-29, same day as upstream,
     no sign of extra work).
2. **SFMF by vicjohnson** (the maintained mod loader, no multiplayer).
   - https://github.com/vicjohnson/SFMF, MIT, releases v1.0.0 (2018-03-12) to v2.0.3
     (2025-10-12, "support for custom install directories"), last push 2026-03-07.
     Mods: https://github.com/vicjohnson/Superflight-Mods (no license file, last push
     2026-03-07). Manifest of 14 mods
     (https://raw.githubusercontent.com/vicjohnson/SFMF/manifest/manifest.json): none is
     multiplayer. Open request "Multiplayer Mod!!" since 2018-08-28
     (https://github.com/vicjohnson/Superflight-Mods/issues/21) and "share trails as
     pseudo-multiplayer" (issue 22).
   - Steam thread "New SFMF (Mods for Superflight)"
     (https://steamcommunity.com/app/732430/discussions/0/1697169163410538309/):
     2018-04-01 takeover announcement; 2020-05-06 the author warns a recent game update may
     have broken some mods.
3. **BepInEx on this game, proof it loads**: Superflight VR mod by doofy-dev uses BepInEx
   5.4.22 (Steam guide https://steamcommunity.com/sharedfiles/filedetails/?id=3130254629,
   2024-01-03; repo https://github.com/doofy-dev/superflight_vr) and SuperflightRL
   (https://github.com/dmitrymdd/SuperflightRL, MIT, pushed 2026-10-25) is "a BepInEx
   plugin" that exposes the game over a local socket. Neither states the game build.
4. Not multiplayer, listed so nobody re-checks them: fan level editor
   (https://rgc-exists.itch.io/superflight-level-editor-mod), MamaMonke/SuperFlightCheat,
   squidward5/superflight-webport, Iemand005/SuperFlight (C++, no description).

Searched (2026-10-29): web search "Superflight co-op mod multiplayer", "Superflight
GrizzlyGames mod BepInEx github", "Superflight multiplayer steam discussions"; GitHub
repository search "superflight" (182 hits, first 19 read), "SFMF superflight",
"superflight multiplayer OR coop OR netplay OR online" (157 hits, first 19 read),
Phlarfl/SFMF forks; Steam discussions threads 1489992080516642412 (all 3 pages),
1697169163410538309, 3183345176711352555; Steam store page. Not checked: Reddit (fetch
blocked), ModDB, Nexus, archive.org, Discord, GitHub code search (needs auth).

### How SFMF hooks the game (observed from source)

- Both SFMF versions patch `Assembly-CSharp.dll` in place with Mono.Cecil: ILRepack merges
  `SFMF.dll` into it and a `call SFMF.Core.Init` is inserted at instruction index 7 of
  `LocalGameManager.Start` (vicjohnson `SFMFManager/Injection/Injector.cs`, Phlarfl
  `SFMFLauncher/MainWindow.xaml.cs`). Optionally `SteamIntegration.SubmitHighscore` and
  `SubmitCombo` get an early `ret` so modded runs don't post to leaderboards.
- `Core.Init` adds a `Core` MonoBehaviour to `LocalGameManager.Singleton`, opens a Win32
  console, loads mod DLLs with `Assembly.Load(bytes)` and adds every exported type deriving
  from the marker base class as a component.
- The two frameworks are **not plugin-compatible**: Phlarfl's loader looks for
  `SFMF.IPlugin` in `.\plugins\*.dll`; vicjohnson's looks for `SFMF.IMod` listed in
  `.\SFMF\installedMods.txt`. Phlarfl's `Multiplayer.dll` therefore only loads under
  Phlarfl's launcher 1.2.0, not under the maintained SFMF v2.
- Game classes used by mods (from the sources): `LocalGameManager.Singleton`
  (`ScoreThisRun`, `ScoreThisCombo`), `PlayerMovement.Singleton` (`transform`,
  `currentSpeed`), `WorldManager.currentWorld.seed`, `LevelGenerator.Singleton.GenerateLevel(new World(seed))`,
  `CameraManager.singelton.mainCamera.mainCameraReference`, `SteamManager.Initialized`,
  `Steamworks.SteamFriends.GetPersonaName()`, `PlayerMaterials.Singleton.playerBodyParts`,
  `Utility.SetTriplanarColors`, `SteamIntegration.SubmitHighscore/SubmitCombo`.

### Audit of Phlarfl's Multiplayer mod (derived from reading the source, not yet run)

Files: `Plugin.cs`, `Player.cs`, `Nametags.cs`, `OverlayMainMenu.cs`,
`Network/{Client,Server,PacketType,PlayerData}.cs`, `Util/{MathUtil,StringUtil}.cs`.

Design: one `PlayerData` message (id, name, position, score, HSV colour, world seed).
Server (`NetworkServer`) assigns id = connectionId, spawn position and colour, sends the
host's `WorldManager.currentWorld.seed`; the client regenerates the level from that seed
on connect. Every client sends its `PlayerData` every `Update` on the default (reliable)
channel; the server rebroadcasts. Remote players are 10-unit spheres lerped 1/5 of the
way per frame; `OnGUI` draws name tags and a host/connect window (IP, port, username).

Bugs found by reading (each needs a live test to confirm):

1. `PlayerData.Serialize` does `(uint)((int)Position.x * 10000f)`: the cast to `int`
   happens before the scale, so precision is 1 world unit, and negative coordinates
   (the spawn is `(0, 1310, -1220)`) go through `(uint)` of a negative value. Remote
   positions will be wrong on at least one axis. Same for colour.
2. `Client.OnPlayerData` sets `otherData.PlayerName = Plugin.Username` before storing it,
   so every remote player is labelled with the local player's name.
3. `Server.OnPlayerData` calls `NetworkServer.SendToAll` inside a `foreach` over all
   players, so each update is broadcast N-1 times.
4. `MsgType.Disconnect` handler is commented out on the server; a client that crashes or
   loses connection is never removed, its sphere stays.
5. `Client.Update` sends a reliable message every frame (no rate limit, no unreliable
   channel) and runs before `OnClientConnect` sets `Player`, so `Player.Data` is a null
   dereference in the frames between `Connect` and the server's reply (`Plugin.Update`
   calls `Network.Update()` whenever `Network != null`).
6. No rotation or animation is sent, so remote players can only ever be spheres.
7. Score of the remote player is shown, but there is no shared or compared run
   (Phlarfl's planned "everyone ready, shared world, scoreboard at the end" from the
   2017-12-09 Steam post was never written).
8. Direct IP only, port 4444, no Steam lobby or relay, so friends need port forwarding
   or a VPN.

Guesses (unverified): the 2022 `Multiplayer.dll` is this 2018 source compiled unchanged
(no source commits between 2018-12-29 and 2022-01-27). UNet HLAPI still works at runtime
in Unity 2017.2 since the DLL ships with the game; it is deprecated, not removed.

Unknowns: whether the Steam build 2304714 `Assembly-CSharp.dll` still has the class and
method names above (the 2020 update may have changed them); whether Phlarfl's launcher
1.2.0 still injects into this build; the exact Steamworks.NET version compiled into
`Assembly-CSharp-firstpass.dll` (no version string found by a byte scan).

### Triage decision

Path 1, revive: the sync design (puppet per remote player, host-chosen seed) is right for
this game and small enough to port. Rather than patching `Assembly-CSharp.dll` in place
the way SFMF does (touches the Steam install, breaks on verify-integrity, disables
leaderboards by patching), the plan is a BepInEx 5 plugin with Harmony, which the VR mod
and SuperflightRL already prove loads on this game. Networking: keep host-authoritative
state sync; replace direct-IP UNet with Steam lobbies plus P2P by SteamID through the
Steamworks.NET the game already ships (every player owns the game, so the game's own app
id can be used and nobody shows as "playing Spacewar"); keep direct IP as a fallback.
Decision pending the owner's go-ahead (test copy of the install, BepInEx download,
release repo).

Playbook contradictions: none yet. The MCP server's test suite fails under
`python -I` when PyYAML is only in the user site-packages (2 of 13 tests); the server
itself runs fine without `-I`. Worth a note in `playbook/mcp.md`.

## 2026-10-29: workspace, port to BepInEx, first automated two-copy test

### Workspace (observed)

- Two private copies of the install (plain copies, 130 MB each, not hard links: the game writes
  `saveGame.gd` and `output_log.txt` under `%USERPROFILE%\AppData\LocalLow\Grizzly Games\<product>`,
  and PlayerPrefs under `HKCU\Software\Grizzly Games\<product>`). Each copy got its own product
  name by patching the one 11-byte `SUPERFLIGHT` string in `superflight_Data\globalgamemanagers`
  (4-byte length prefix; the company string `Grizzly Games` sits 20 bytes earlier, 4-byte aligned)
  and `app.info`, plus `steam_appid.txt` = 732430 so `SteamAPI.RestartAppIfNecessary` does not
  relaunch the Steam install. Checked after runs: the real `SUPERFLIGHT` registry key's export hash
  unchanged; the copies' keys and LocalLow folders created separately. Playbook lesson 51 applied as is.
- `boot.config` is empty (no single-instance flag); two copies run side by side.
- BepInEx 5.4.23.6 win_x64 (official release zip, SHA-256
  `6c57a714a892efb4e9e1fef63d608a3391f30ec984031c465404e4ee381a0755`) loads: "CLR runtime
  version 2.0.50727.1433", so plugins target .NET 3.5. `Assembly-CSharp.dll` references mscorlib
  2.0 and System.Core 3.5.
- Build: Roslyn `csc.exe` from Visual Studio Build Tools against the game's own `Managed\*.dll`
  plus BepInEx's `BepInEx.dll` and `0Harmony.dll` (`/nostdlib+`, C# 7.3). No NuGet, no SDK
  project. A reflection dump of all types and members plus a Mono.Cecil IL dump (Cecil taken from
  BepInEx's core folder) of the world-loading methods were enough; no decompiler was installed.

### What the game does (derived from IL, Steam build 2304714)

- `LocalGameManager.Start` = `LoadCurrentWorld()` + `LoadMenu()`. `LoadMenu` destroys the player
  object; `LoadFlight` calls `ResetPlayer`, which instantiates it. In the menu
  `PlayerMovement.Singleton` is null.
- `LoadCurrentWorld()` (private): `LevelGenerator.GenerateLevel(WorldManager.currentWorld)`, or
  `GenerateLevel(WorldManager.NewWorld())` when null; resets the combo.
- `OnSeedEnter(World)` (public): sets `WorldManager.currentWorld`, `LoadCurrentWorld()`, moves the
  player and camera to the spawn, resets rotation, combo, colour and particles. A client uses it to
  enter the host's world while flying.
- `OnPortalEnter`: `GenerateLevel(WorldManager.NewWorld())` plus the same reset. A prefix on
  `WorldManager.NewWorld` returning `new World(hostSeed)` keeps a client in the host's world
  through its own portals; a postfix on `LevelGenerator.GenerateLevel(World)` lets the host
  announce every new seed.
- `World(int customSeed)` is public; `World.seed` is the int the generator uses.
- `SteamManager.Awake` is the stock Steamworks.NET sample (Packsize and DllCheck tests,
  `RestartAppIfNecessary`, `SteamAPI.Init`). Steamworks.NET 10.0.0, SDK 1.40: old-style
  `SteamNetworking.SendP2PPacket/ReadP2PPacket` with relay, `SteamMatchmaking` lobbies,
  `Callback<T>.Create`, `CallResult<T>`; no `SteamNetworkingMessages`.
- `PlayerMovement.GetInput()` (private) fills `horizontalInput` and `verticalInput`; a postfix
  overwrites them, which is how the test bot steers. `LocalGameManager.QuickReset()` (private)
  respawns after a death. `LocalGameManager.PlayerState`: Flying, Paused, Dead, Completed, Menu.

### The plugin (what was built)

Repo: superflight-coop (local for now, MIT). BepInEx 5 plugin with Harmony patches, no game file
modified. Design kept from Phlarfl's mod: host-authoritative, the host's seed, one puppet per
remote player. Changes: own UDP transport with a small reliable channel (sequence, ack, 250 ms
resend, in-order delivery, 10 s timeout) instead of UNet; float position and quaternion rotation
(fixes the int truncation and negative-coordinate bug); names from the host's roster (fixes the
name mix-up); one relay per update (fixes the N-1 broadcast); timeouts remove ghosts; no send
before Welcome (fixes the null-dereference window); puppets are a clone of the local wingsuit
meshes (scripts, physics, particles stripped) tinted with the player's hue, interpolated with a
100 ms delay and short extrapolation; leaderboard uploads blocked in a session; an in-game bridge
(coop-anything `tools/bridge` protocol v1: ping, version, methods, events, state, exec, eval,
waitFor, screenshot) and a test bot (`--coop-bot`).

### QA (ran)

`tests/run_two.py`: two private copies, host (`--coop-host 4444 --coop-autoflight --coop-bot`)
and client (`--coop-join 127.0.0.1:4444 ...`), bridges over loopback TCP, 20 s hold.

- Run 1, plugin without `Application.runInBackground`: FAIL. The host copy lost focus when the
  client window opened and Unity paused it entirely; the host never hosted, the client timed out
  ("lost the host"). This is also the mutation check for that line.
- Run 2, with runInBackground, no bot: join, seed and state flow PASS, but both players were
  `Dead` at the spawn after a few seconds (no input), so a movement check would have failed.
- Run 3, with bot: PASS. Seed 654102576 on both; host moved 547 units, client 1142 in 20 s;
  376 and 398 states received (about 19/s each way); host's copy of the client position 5.1 units
  from the client's own; screenshots show the other player's name tag and score in the world.
- Run 4, with mesh checks: PASS, build d9c7bbb. Seed 120652260; host moved 472, client 1041
  units; each copy drew 10 renderers of the other's wingsuit clone in a frame (not a sphere);
  377 and 402 states; positions 6.3 units apart; no error or warning in either BepInEx log.

Not yet tested: more than two players, lag and loss (`tools/chaos`), the host taking a portal
mid-session, Steam transport (not written yet), two different PCs.

Playbook note: the harness must warn the owner not to touch the game windows; the bot overrides
steering, but Escape, Enter or the mod's own hotkeys reach the focused copy.

## 2026-10-29 (later): level changes, the choice screen, and a patching bug

### What the game does at a portal and after a crash (derived from IL)

- `Portal.OnTriggerEnter` calls `LocalGameManager.OnPortalEnter` at once: no choice screen in the
  game. `OnPortalEnter` = stats and achievement, `GenerateLevel(WorldManager.NewWorld())`, portal
  sound, camera reset, player moved to the spawn, `ComboComplete`. `LevelBottomTrigger` does the
  same through `OnEndPlaneEnter` when the player falls out of the level.
- A crash: `PlayerMovement.OnCollisionEnter` → `Die` → `LocalGameManager.OnPlayerDeath` (state
  Dead, crash sound, death animation, `RunEnded` → run-end canvas after 0.5 s). `QuickReset`
  (private) = `quickResetFlag`, `CollectPoints`, `ResetPlayer` (new player object at the spawn,
  scores zeroed, state Flying). `GetInput` only reacts to the reset and menu buttons while Flying.
  The scene's serialized UI references only `LoadFlight`, `Quit` and `OpenGallery`.

### Owner's design for level changes

The host decides. A portal (or fall-out) opens a choice: proceed to the next level, or respawn in
the current one, with a scoreboard. The host's proceed sends the new seed; a client keeps it
pending and gets the same choice at its own next portal, fall-out or crash. Nobody is pulled out of
a run. Built as `LevelChoice.cs`: Harmony prefixes on `OnPortalEnter` and `OnEndPlaneEnter` show
the screen and pause (`Time.timeScale = 0`); a postfix on `OnPlayerDeath` shows it after a crash
when a seed is pending (no pause); a prefix on `QuickReset` blocks the game's own respawn while the
screen is up. Proceed on the host calls the real `OnPortalEnter` with a pass-through flag; on a
client it calls `OnSeedEnter(new World(seed))`, or after a crash sets `WorldManager.currentWorld`,
`LoadCurrentWorld()` and `QuickReset()`. Respawn = `OnSeedEnter(currentWorld)` (regenerate in
place) or `QuickReset()` after a crash. The `WorldManager.NewWorld` prefix stays as the safety net
(a client never gets a world the host has not been in). Protocol v2 adds each player's seed to the
state message so the scoreboard can say "host's level" or "previous level".

### The patching bug (observed, fixed)

Until this point no Harmony patch had ever been live: `harmony.PatchAll(typeof(Patches))` patches
only the type it is given, and the patches were nested classes inside it. The plugin logged
"patches applied" and the earlier runs passed because none of them needed a patch (the seed was
applied by a direct call on join, the bot reset by a direct call). The first portal test failed on
every patched behaviour at once, which is what exposed it. Fix: `PatchAll(typeof(Plugin).Assembly)`
and log `GetPatchedMethods()` count at start (9 methods), error when it is 0. The leaderboard block
logged its first "upload skipped" only after this fix. Playbook lesson added.

### QA (ran)

- Run 5, `tests/run_portal.py` (first attempt): FAIL on every choice and seed check: the patches
  were not live (see above). The test did its job.
- Run 6, `tests/run_portal.py` after the fix: PASS, 40 checks, 17:11 local time, build 22fd796+.
  Host portal → choice open, reason Portal, timeScale 0, seed unchanged, scoreboard 2 rows;
  respawn → same level, timeScale 1, flying; portal again, proceed → new seed 487603592 →
  1438637820, client got it as pending and stayed in its level, both still 2 players; client
  crash → choice, reason Crash, not paused; respawn → same level, pending kept; client portal →
  proceed → client in the host's level, pending cleared, session seed matches; client portal with
  nothing pending → proceed acts as respawn, cannot get ahead of the host; both processes alive.
  Screenshots of both choice screens taken through the bridge.
- Run 7, `tests/run_two.py` regression with the hooks live: PASS; the bot now really steers (2898
  and 3256 units in 20 s), 377 and 399 states, 10 renderers each side.

## 2026-10-29 (evening): Steam friend invites

### Why Steam
The owner asked which path needs no port forwarding. Direct IP needs the host to forward UDP
4444 or a VPN; Steam's `ISteamNetworking` P2P is relayed by Steam when no direct path exists.
Owner chose Steam.

### What the game ships (from the reflection dump, `local/dumps/Assembly-CSharp-firstpass.txt`)
Steamworks.NET 10.0 (SDK 1.40): `SteamNetworking.SendP2PPacket/IsP2PPacketAvailable/
ReadP2PPacket/AcceptP2PSessionWithUser/CloseP2PSessionWithUser/AllowP2PPacketRelay`,
`SteamMatchmaking.CreateLobby/JoinLobby/LeaveLobby/GetLobbyOwner/GetNumLobbyMembers/
GetLobbyMemberByIndex/SetLobbyData/GetLobbyData`, `SteamFriends.ActivateGameOverlayInviteDialog/
SetRichPresence/ClearRichPresence`, `Callback<T>.Create`, `CallResult<T>.Create/Set`, structs
`LobbyCreated_t`, `LobbyEnter_t`, `GameLobbyJoinRequested_t`, `LobbyChatUpdate_t`,
`P2PSessionRequest_t`, `P2PSessionConnectFail_t`. No `SteamApps.GetLaunchCommandLine` (added in a
later SDK), so `+connect_lobby` is read from `Environment.GetCommandLineArgs`. The game's own
`SteamManager` (the stock Steamworks.NET one, IL checked: `Packsize.Test`, `DllCheck.Test`,
`RestartAppIfNecessary`, `SteamAPI.Init` in Awake, `RunCallbacks` in Update) dispatches our
callbacks; nothing needs a second init.

### Design (plugin commit d459576, version 0.2.0)
- `SteamTransport`: `ITransport` over P2P channel 0. Steam's reliable send type carries the
  reliable messages (Hello, Welcome, Seed, joins), `UnreliableNoDelay` the 20 Hz state. One kind
  byte (0 payload, 3 keep-alive), keep-alive every second, 15 s silence drops a peer,
  `P2PSessionConnectFail_t` drops at once. The host accepts a `P2PSessionRequest_t` only from a
  current lobby member; a client only talks to the lobby owner. Peer key = SteamID string.
- `SteamLobby`: friends-only lobby of 8 with lobby data `game=superflight-coop`,
  `protocol=<Protocol.Version>`, `plugin=<version>`; a joiner that finds another game or protocol
  leaves again with a clear message. Rich presence `connect` = `+connect_lobby <id>`. Overlay
  invite dialog. `GameLobbyJoinRequested_t` (accept while running) and `+connect_lobby` (Steam
  starts the game) both queue a join that runs once `LocalGameManager` exists. `LobbyChatUpdate_t`
  left/disconnected/kicked/banned drops that member from the session; if the owner goes, the
  client leaves. Fallback for a disabled overlay: the lobby id is shown as a code to paste.
- `Session` changes shared by both transports: connect timeout per transport (UDP 10 s, Steam
  30 s), Hello resent every 2 s until Welcome (the host already answered duplicates), `Ended`
  callback so the lobby is left whenever a session ends, `DropPeer` for lobby departures.
- Steam identity: the plugin never sends SteamIDs in its own protocol; names still come from the
  host's roster.

### QA (ran)
- Run 8, `tests/run_two.py` and `tests/run_portal.py` with the Steam code in, direct IP: both
  `RESULT: PASS`, 19:24 local time; 9 patches live; 0 warnings or errors in both BepInEx logs;
  both logs show "steam: lobby callbacks registered", so `SteamManager.Initialized` is true in the
  test copies (Steam client running on this PC).
- `tests/run_steam.py` (one copy: Steam ready, `steamhost`, lobby created, hosting over the Steam
  transport, one member, screenshot, leave tears it down; `--join CODE` mode for a second PC and
  account): written, NOT yet run. It creates a lobby on the owner's Steam account, so it waits for
  the owner's go-ahead (AGENTS.md: ask before using their accounts).
- A full two-player Steam run is impossible on one PC with one account (an account is in a lobby
  once); it needs the owner and a friend, or a second PC with a second account: `unknown` until
  then. Rich-presence "Join Game" in the friends list: `unknown`.
- Run 9, `tests/run_steam.py` host mode with the owner's OK, 19:28 local time: `RESULT: PASS`, 15
  checks. Steam API ready in the copy, `steamhost` created a friends-only lobby (id logged), the
  session hosted over the Steam transport with one member and no error, the window showed
  "Hosting for Steam friends", the invite button and the lobby code; `leave` tore the lobby down
  and closed the session; the copy kept running. Screenshot `tests/shot-steam-host.png`. Still
  `unknown`: an actual join and P2P traffic between two accounts.

### Release package (prepared, not published)
`SuperflightCoop-0.2.0.zip` (DLL, README, LICENSE, INSTALL.txt) built from plugin tag v0.2.0
(commit 795e01c) with `/pathmap` so neither the DLL nor the PDB carries a local folder path; the
first package had the full PDB path inside the DLL's debug directory, caught by a byte scan of the
zip for the username, `C:\Users` and `OneDrive` (now 0 hits; the PDB is not shipped). SHA-256 in
the zip's `.sha256` file next to it. Publishing (GitHub repo, release, directory entry) is the
owner's step: their GitHub account.

### Pre-publish history scrub
Before the owner pushes, every revision was scanned for the username, `C:\Users`, `OneDrive` and
the owner's e-mail: a tracked `tests/__pycache__/*.pyc` (compiled path inside) and the old
`build.ps1` default game folder were in earlier commits. Both were rewritten out with
`git filter-branch` (index filter for the cache, tree filter for the path line), tag v0.2.0 moved
to the new head 014e00a, old objects pruned, scan now clean. The release zip was rebuilt from that
head (embedded commit matches) and `tests/run_two.py` passed on it: `RESULT: PASS`, build 014e00a,
19:36 local. Final SHA-256 of `SuperflightCoop-0.2.0.zip`:
2e39331a727521211f2b128f864a92968bc40fff458f70aa1592d48fcfc40654. Commit author identity is the
owner's own git config; flagged to them (GitHub's "block pushes that expose my email" setting would
reject the push if it is on).

## 2026-10-30: plugin repo published

With the owner's OK and their empty repo URL, the plugin repo was pushed from the scrubbed
history: `git remote add origin https://github.com/carlgoodtoseeyu/superflightmp.git`,
`git push -u origin main --tags`. Verified after the push: `git ls-remote origin` shows `main` and
`v0.2.0` at the same objects as local (head 014e00a, tag 2d59f1c), 26 tracked files (source,
tests, build script, README, INSTALL, RELEASING, LICENSE; no DLL, PDB, logs or caches), the repo
and the tag page answer HTTP 200 without sign-in, and `git grep` over every pushed revision finds
no username, local path or e-mail. The GitHub release (tag v0.2.0, `SuperflightCoop-0.2.0.zip`,
SHA-256 2e39331a...0654) is still the owner's step; the directory entry gets the release link once
it exists.
