# Superflight (2017)

| | |
|---|---|
| Engine | Unity 2017.2.0, Mono scripting backend |
| Versions tested | Steam build 2304714 (2025-07-13); direct IP tested automatically, Steam lobby code built, two-account Steam run pending |
| Projects | [SFMF Multiplayer by Phlarfl](https://github.com/Phlarfl/SFMF/tree/master/Multiplayer/Multiplayer) (directory entry: Superflight / SFMF Multiplayer) |
| QA level | automated (two real copies on one PC, direct IP; see QA) |
| Last updated | 2026-10-29 |

## Where things stand

A new BepInEx plugin (superflight-coop, alpha) puts players in the same world over direct
IP, each visible as a tinted copy of the wingsuit with name and score; two automated
two-copy suites pass. Steam friend invites without port forwarding are built on the game's
own Steamworks.NET (friends-only lobby, P2P with relay, overlay invite, `+connect_lobby`);
what one PC and one account can test is scripted, the two-account run is pending. The
2018 SFMF Multiplayer mod it replaces is abandoned (its author doubts it still works).

## Existing work

- **SFMF Multiplayer** (Phlarfl, no license stated). Source committed 2018-12-29,
  `Multiplayer.dll` released 2022-01-27 (120 downloads). Unity UNet over direct IP, port
  4444, host also plays. Syncs: remote players as coloured spheres with name and score
  tags, and the host's world seed so everyone flies the same map. Loads only under
  Phlarfl's own SFMF launcher 1.2.0 (2017-12-24), which patches `Assembly-CSharp.dll` in
  place. Steam thread: [Multiplayer Mod Progress](https://steamcommunity.com/app/732430/discussions/0/1489992080516642412/).
  Last author statement, 2025-01-05: "I don't think it would still work."
- **SFMF v2** (vicjohnson, MIT, [repo](https://github.com/vicjohnson/SFMF)): the
  maintained mod loader, v2.0.3 released 2025-10-12, last push 2026-03-07. Its
  [mod manifest](https://raw.githubusercontent.com/vicjohnson/SFMF/manifest/manifest.json)
  has 14 mods and no multiplayer; a request has been open since 2018
  ([issue 21](https://github.com/vicjohnson/Superflight-Mods/issues/21)). Its plugin
  interface (`SFMF.IMod`) differs from Phlarfl's (`SFMF.IPlugin`), so the two are not
  compatible.
- **BepInEx 5 loads on this game**: the
  [Superflight VR mod](https://github.com/doofy-dev/superflight_vr) (BepInEx 5.4.22,
  [Steam guide](https://steamcommunity.com/sharedfiles/filedetails/?id=3130254629)) and
  [SuperflightRL](https://github.com/dmitrymdd/SuperflightRL) (pushed 2026-10-25).

Dead ends and where they were searched are in [NOTES.md](NOTES.md).

## How the game works (for co-op)

- Unity 2017.2 Mono: all game logic is in `superflight_Data\Managed\Assembly-CSharp.dll`,
  readable with dnSpyEx and patchable at runtime with Harmony through BepInEx. Known
  classes: `LocalGameManager` (scores), `PlayerMovement` (position, speed),
  `WorldManager.currentWorld.seed`, `LevelGenerator.GenerateLevel(World)`,
  `CameraManager`, `SteamIntegration.SubmitHighscore/SubmitCombo`.
- No netcode of its own, but the game ships `UnityEngine.Networking.dll` (UNet HLAPI,
  which the 2018 mod uses) and Steamworks.NET compiled into
  `Assembly-CSharp-firstpass.dll` with `SteamMatchmaking` and `SteamNetworking` types, so
  Steam lobbies and P2P by SteamID are reachable from a plugin without extra DLLs.
- No save system to sync: runs are scored per world seed; leaderboards are per player.
  Level generation is seeded, so sharing the seed gives everyone the same map.
- Online features: Steam leaderboards and achievements. SFMF disables score submission
  while mods are installed; a co-op plugin should do the same for modded runs.
- No anti-cheat.

## What we changed

The 2018 mod was ported to a BepInEx 5 plugin instead of SFMF's in-place patch of
`Assembly-CSharp.dll`, keeping its design (host's seed, puppet per player) and fixing what
the source audit found (details in NOTES.md):

- Position serialisation truncated to whole units and broke on negative coordinates: now
  floats plus a quaternion for rotation. Test: the host's copy of the client position stays
  within a few units of the client's own while both fly.
- Every remote player got the local player's name: names come from the host's roster.
  Test: the host lists the client as "Bot1".
- Each update was broadcast N-1 times, dropped connections left ghost players, and a send
  could run before the host had answered: one relay per update, 10 s timeouts, no send
  before Welcome. Test: about 19 states/s each way over 20 s, no exception in the logs.
- No rotation sync and sphere puppets: rotation is sent, puppets are mesh clones. Test:
  each copy draws 10 renderers of the other's wingsuit in a frame.
- Steam friends: `SteamTransport` over `ISteamNetworking` P2P (relay allowed, Steam's reliable
  send type for the reliable messages) and `SteamLobby` (friends-only lobby of 8, lobby data
  with game and protocol, overlay invite dialog, rich presence connect string, join from
  `GameLobbyJoinRequested_t` or `+connect_lobby`, members who leave the lobby are dropped).
  No extra DLLs: the game ships Steamworks.NET 10.0 and runs the callbacks. Test: see QA;
  the two-account run is still open.
- New: leaderboard uploads are blocked while in a session.
- New: level changes are a choice, not a yank. A portal or fall-out opens "Proceed to next
  level / Respawn in this level" with a scoreboard (name, score, where everyone is). The
  host's Proceed runs the game's own portal code and sends the seed; a client keeps it
  pending and gets the same screen at its next portal, fall-out or crash. Nobody can get
  ahead of the host. Test: `tests/run_portal.py`, 40 checks through the bridge.
- Found while testing: Harmony's `PatchAll(Type)` does not patch nested patch classes, so
  the first builds ran with no hook live. The plugin now patches the assembly and logs the
  patched-method count (9) at start.
- Found while testing: a Unity player pauses completely without window focus, so two copies
  on one PC need `Application.runInBackground = true` (the 2018 mod set it too).

## QA

- `python -I tests\run_two.py --instances <copy0> <copy1> --coop-anything <clone>` in the
  plugin repo: starts two private copies, host and client, both with the in-game bot, and
  asserts through the plugin's bridge: both started, client joined, both see 2 players,
  same world seed, both players move, both draw the other's wingsuit meshes, state keeps
  flowing, the host's copy of the client position is close, screenshots written, both
  still running.
- `python -I tests\run_portal.py ...`: host portal choice (paused, seed unchanged, scoreboard
  rows), host respawn, host proceed (new seed sent, client only pending, not pulled out),
  client crash choice (not paused), client respawn (pending kept), client portal proceed
  (now in the host's level), client portal with nothing pending (cannot get ahead).
- `python -I tests\run_steam.py ...`: one copy, Steam client signed in: Steam API ready,
  lobby created, session hosting over the Steam transport, one member, leave tears it down.
  `--join CODE` on a second PC and account: joins, gets the host's world, state and puppet
  arrive. Host mode ran on the owner's PC with their OK: `RESULT: PASS`, 15 checks (lobby
  created, hosting over the Steam transport, teardown). The join mode has not run yet.
- Last results, 2026-10-29, Steam build 2304714: run_two and run_portal `RESULT: PASS`
  (run_two: 377 and 399 states in 20 s, 10 puppet renderers each side; run_portal: 40
  checks), rerun after the Steam code landed with 0 warnings or errors in the logs. The run
  without `runInBackground` failed (host paused) and the first run_portal failed on every
  patched behaviour (patches not live): both are the mutation checks for those fixes.

## How to play

Install BepInEx 5.4.23.6 (win_x64) into the game folder, run the game once, put
`SuperflightCoop.dll` into `BepInEx\plugins\SuperflightCoop\`. Steam: click "Host for Steam
friends", then "Invite friends" in the co-op window; friends accept in the overlay (Steam
starts the game for them if needed), or paste the lobby code the host sees. Direct IP: pick
a port, click Host, forward that UDP port or use a VPN; friends type `host:port`. Release
repo and download: not published yet (see Next steps).

## Known issues

- The Steam path has not been run between two accounts yet; direct IP (forwarded UDP port
  or VPN) is the tested fallback. Friends-list "Join Game" via rich presence: unverified.
- Remote wingsuits do not animate (still mesh copy).
- No shared scoring mode; each player keeps their own run. The choice screen's scoreboard
  shows everyone's current run score.
- The choice screen shows the level as a number, not the game's share code.

## Next steps

1. Steam: a real two-account run over the internet (owner plus a friend through the overlay
   invite, or `tests/run_steam.py --join CODE` on a second PC with another account).
2. Chaos runs with `tools/chaos` (lag, loss) and a three-player run.
3. Publish: the plugin repo is tagged v0.2.0 and `SuperflightCoop-0.2.0.zip` with its SHA-256
   is packaged; the owner pushes the repo, uploads the release, then the directory entry gets
   the link.
