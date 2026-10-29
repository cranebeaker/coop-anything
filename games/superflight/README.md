# Superflight (2017)

| | |
|---|---|
| Engine | Unity 2017.2.0, Mono scripting backend |
| Versions tested | Steam build 2304714 (2025-07-13), identity recorded; no co-op test run yet |
| Projects | [SFMF Multiplayer by Phlarfl](https://github.com/Phlarfl/SFMF/tree/master/Multiplayer/Multiplayer) (directory entry: Superflight / SFMF Multiplayer) |
| QA level | automated (two real copies on one PC, direct IP; see QA) |
| Last updated | 2026-10-29 |

## Where things stand

A new BepInEx plugin (superflight-coop, alpha) puts players in the same world over direct
IP, each visible as a tinted copy of the wingsuit with name and score; an automated
two-copy test passes. Steam friend joins without port forwarding are the next step. The
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
- Direct IP only: still true in this version (Steam lobby next).
- New: leaderboard uploads are blocked while in a session; a client's portal keeps it in
  the host's world (Harmony prefix on `WorldManager.NewWorld`); the host's portal sends the
  new seed to everyone (postfix on `LevelGenerator.GenerateLevel`).
- Found while testing: a Unity player pauses completely without window focus, so two copies
  on one PC need `Application.runInBackground = true` (the 2018 mod set it too).

## QA

- `python -I tests\run_two.py --instances <copy0> <copy1> --coop-anything <clone>` in the
  plugin repo: starts two private copies, host and client, both with the in-game bot, and
  asserts through the plugin's bridge: both started, client joined, both see 2 players,
  same world seed, both players move, both draw the other's wingsuit meshes, state keeps
  flowing, the host's copy of the client position is close, screenshots written, both
  still running.
- Last result: `RESULT: PASS`, 2026-10-29, Steam build 2304714, build d9c7bbb, 20 s hold,
  377 and 402 state updates received, positions 6.3 units apart, 10 puppet renderers drawn
  on each side. The run without `runInBackground` failed (host paused), the mutation check
  for that fix.

## How to play

Install BepInEx 5.4.23.6 (win_x64) into the game folder, run the game once, put
`SuperflightCoop.dll` into `BepInEx\plugins\SuperflightCoop\`. Host: pick a port, click
Host, forward that UDP port or use a VPN. Join: type `host:port`. Release repo and
download: not published yet (see Next steps).

## Known issues

- Direct IP only; friends outside the LAN need a forwarded UDP port or a VPN.
- Remote wingsuits do not animate (still mesh copy).
- No shared scoring mode; each player keeps their own run.

## Next steps

1. Steam transport: friends-only lobby plus `SteamNetworking` P2P with relay through the
   game's shipped Steamworks.NET and the game's own app id, invite from the overlay. Needs
   two Steam accounts or two PCs to test.
2. Chaos runs with `tools/chaos` (lag, loss) and a three-player run.
3. Publish the plugin repo and a release; add it to the directory entry.
