# A Difficult Game About Climbing (2024)

| | |
|---|---|
| Engine | Unity 2021.3.21, Mono scripting backend, 2D physics (Rigidbody2D) for the climber |
| Versions tested | Steam app 2497920, build 20993968, exe SHA-256 `9FDC9F180393A7109BEF27279985A81DBC1E399BFE0B3FE3EEBEB58030ADAB39`, Windows x64 |
| Projects | ADGAC Co-op 0.1.0 (BepInEx plugin, source not yet published) |
| QA level | none (manual two-client smoke test only, see QA) |
| Last updated | 2026-10-25 |

## Where things stand

No co-op project existed in this repo or in the searches below. A BepInEx plugin now gives "ghost co-op": every player
climbs their own physics climber and sees the others as tinted, name-tagged copies, with an optional rope that pulls
players together. Steam lobby code is written and a lobby was created from a game copy; messaging between two real Steam
accounts is **unverified**.

## Existing work

- This repo (`DIRECTORY.md`, `games/`): nothing, 2026-10-24.
- GitHub repository search "A Difficult Game About Climbing", 10 of 13 results read: only single-player BepInEx mods
  (giant mode, mod manager, hand indicators, sound swap, VRM importer, skins). None is multiplayer.
- Web search, two phrasings: no co-op or multiplayer mod found.
- Not checked: Steam Workshop/community, Nexus, Thunderstore, Reddit, Discord, page 2 of the GitHub results.
- One GitHub repo offers the game "free" in a password-protected archive. Do not download it.

## How the game works (for co-op)

- `Assembly-CSharp.dll` is small (about 270 classes) and decompiles cleanly with ILSpy 9.1.
- The player is one prefab, `ClimberMain`, spawned by `PlayerSpawn` and referenced by `StaticGameReference.climberMain`.
  Respawn and teleport go through `PlayerSpawn.Respawn(Vector2)`.
- Physics bodies (10 Rigidbody2D): torso (`Body`), two arm roots, two hands, two cursors, legs connector, two legs.
  Hands grab with FixedJoint2D/FrictionJoint2D. The torso hangs from the hands.
- The visible skeleton (51 transforms under `HeroCharacter`) is posed by an Animator with layer weights plus IK
  (`IKControl.OnAnimatorIK`, `AnimControl`). It is not a physics ragdoll. IK also shifts the forearm and hand bones in
  `LateUpdate`.
- Saves: `Application.persistentDataPath` (`data.pain`, `data1/2.pain`, slot file) plus PlayerPrefs. The game saves
  every 20 s and on first entering water. **Editing `<game>_Data/app.info` in a game copy only moves the log file, not
  the saves**: our first test runs overwrote the real saves this way (restored from a backup taken first).
  Isolation that works: a Harmony postfix on `Application.get_persistentDataPath` (verified by hashing the real save
  folder before and after a two-instance run). Always back up saves before the first launch of any copy.
- The game already loads Steamworks.NET (`SteamManager`) with its real app id, so lobbies and
  `SteamNetworkingMessages` can be used directly with no extra DLLs.
- Scenes: 0 init, 1 level one, 2 new game plus.

## What we changed

New plugin, no game files modified:

- Pose sync, about 30 Hz, about 570 bytes: world position and angle of the 10 bodies, local rotation of 51 bones
  (smallest-three quaternion), local position of 4 IK-shifted bones.
- Ghost = clone of the local climber built under an inactive parent (so no game Awake/Start runs), then all
  MonoBehaviours, Animator, joints, colliders, rigidbodies and audio removed and bones driven from the network.
- Rope: each client adds a spring force toward the other player's ghost beyond a slack length, plus a line renderer.
- Transport: Steam friends-only lobby plus SteamNetworkingMessages (channel 7); localhost UDP for single-PC testing.
- `Application.runInBackground = true`, otherwise a second window stops simulating when unfocused.

## QA

- No automated suite. Run `run-test.ps1` (launches two copies with separate save folders, linked over UDP 7001/7002,
  ghosts shown next to the local climber with `--coop-ghost-near`).
- 2026-10-25: two real game clients, UDP link, each showed the other as a named ghost with its own arm pose; rope line
  rendered; no exceptions from the plugin. The only exception in `Player.log` is the game's own
  `LocalizationScript.SelectLanguage`.
- A Steam lobby was created from a game copy (`--coop-host`). No second-account Steam session yet.

## How to play

See the plugin's `README.md`: run `install.ps1` once (makes a separate game copy with its own saves and installs
BepInEx 5.4.23.5 verified by SHA-256), start `Play Co-op.bat`, F8 to host or join.

## Known issues

- World objects (balloons, pendulums, falling pieces, water) are simulated locally, so they differ between players.
- Players do not collide with each other, and there is no shared progress or checkpoints.
- Rope uses forces from delayed ghost positions, so it feels stretchy at high ping. Untuned.
- Only this game build was checked.

## Next steps

1. Test with two Steam accounts (join by friend list and by lobby ID).
2. Playtest rope tuning on real climbs; consider sharing checkpoints or resetting both players together.
3. Sync moving world objects from the host (balloons, pendulums, falling pieces).
4. Automated test: a bot that drives `ArmScript_v2` and asserts both clients agree on ghost positions.
5. Publish the plugin source in its own repo and add the directory entry.
