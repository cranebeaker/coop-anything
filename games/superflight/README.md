# Superflight (2017)

| | |
|---|---|
| Engine | Unity 2017.2.0, Mono scripting backend |
| Versions tested | Steam build 2304714 (2025-07-13), identity recorded; no co-op test run yet |
| Projects | [SFMF Multiplayer by Phlarfl](https://github.com/Phlarfl/SFMF/tree/master/Multiplayer/Multiplayer) (directory entry: Superflight / SFMF Multiplayer) |
| QA level | none |
| Last updated | 2026-10-29 |

## Where things stand

Nobody can play Superflight with a friend today. One multiplayer mod exists, written in
2018 and published as a DLL in January 2022; its author said in January 2025 they doubt
it still works, and no player report of a working session was found. The source is public
and small, so reviving it is the plan.

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

Nothing yet. Audit of the 2018 source (details in NOTES.md) found: position serialisation
truncates to whole units and breaks on negative coordinates; every remote player gets the
local player's name; each update is broadcast N-1 times; dropped connections leave ghost
players; a null dereference window between connect and the server's reply; no rotation
sync; direct IP only.

## QA

No tests run yet.

## How to play

Not playable yet.

## Known issues

See "What we changed" for the bugs in the original mod.

## Next steps

1. Owner's go-ahead: copy the install to a test folder, pin and download BepInEx 5
   (win_x64), create a release repo for the plugin code.
2. Confirm the class and method names above still exist in build 2304714 (dnSpyEx or a
   reflection dump from a BepInEx plugin).
3. Port the mod to a BepInEx plugin: puppet per remote player (position plus rotation),
   host-chosen seed, fix the bugs above, Steam lobby + P2P via the shipped Steamworks.NET
   with direct IP as fallback, score submission off while connected.
4. Two real clients on one PC with `tools/gameslot` and the bots harness; chaos runs.
