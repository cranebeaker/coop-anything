# A Difficult Game About Climbing: notes

Raw working notes, 2026-10-24 to 2026-10-25. Paths use `<game dir>`. Final write-up: [README.md](README.md).

## Identity (observed)

- Steam app 2497920, build id 20993968, Windows x64.
- `<game dir>/A Difficult Game About Climbing.exe`, SHA-256 `9FDC9F180393A7109BEF27279985A81DBC1E399BFE0B3FE3EEBEB58030ADAB39`, file version 2021.3.21.1774945.
- Unity 2021.3.21, Mono (`<game>_Data/Managed/Assembly-CSharp.dll` present, no `GameAssembly.dll`).
- Steamworks.NET shipped in `Managed/com.rlabrecque.steamworks.net.dll`; `SteamManager` initialises with the real app id.

## Observations

- `Assembly-CSharp.dll` decompiled with ilspycmd 9.1.0.7988: 271 classes, mostly game scripts.
- Player prefab clone is named `Climber5(Clone)`; 10 Rigidbody2D, 51 skeleton transforms under `HeroCharacter`.
- Arms: `ArmScript_v2` (mouse/stick driven hands, FixedJoint2D/FrictionJoint2D grab). Pose: `IKControl.OnAnimatorIK` +
  `AnimControl` layer weights; `IKControl.LateUpdate` shifts forearm and hand bones.
- `SaveSystemJ` writes `Application.persistentDataPath` every 20 s (rotating data.pain / data1 / data2, current.slot)
  and on first water entry (datacp.pain).
- BepInEx 5.4.23.5 (SHA-256 `82f9878551030f54657792c0740d9d51a09500eeae1fba21106b0c441e6732c4`) loads in the copy via
  winhttp.dll doorstop; plugin loads, finds the climber, builds ghosts.
- Two clients on one PC and one Steam account both start (UDP link used for tests). Unity stops simulating an
  unfocused window unless `Application.runInBackground = true`.
- The only exception seen in `Player.log` is the game's own `LocalizationScript.SelectLanguage` NullReferenceException.
- A Steam friends-only lobby was created from a game copy with `--coop-host`.

## Mistake worth recording

First test runs wrote to the real save folder. Editing `<game>_Data/app.info` (company/product name) moved
`Player.log` only; `persistentDataPath` stayed `LocalLow/Pontypants/...`. The files had been backed up beforehand, were
restored (hashes identical), and isolation was done with a Harmony postfix on `Application.get_persistentDataPath`,
checked by hashing the real folder before and after a two-instance run. Playbook suggestion: back up saves before the
first launch of any copy and verify isolation with a hash before trusting it (see `playbook/methods.md`).

## Derived

- Ghost co-op is enough to see friends climb; the physics climber is local so no input latency.
- A rope can be approximated by a spring force toward the other ghost's torso (each side applies its own half).

## Guesses

- Rope will feel stretchy above roughly 100 ms ping.
- Host-authoritative sync of moving objects (balloons, pendulums, falling pieces) will be needed for puzzles that depend on timing.

## Unknown

- Steam path with two accounts: lobby join, `SteamNetworkingMessages` session accept, overlay invite from a copy started outside Steam.
- Behaviour on game builds other than 20993968.
- Whether Steam Cloud uploaded the temporarily changed save files before they were restored (Steam was running; the real app id was in use).
- Existing co-op work on Steam Workshop, Nexus, Thunderstore, Reddit, Discord (not searched).

## Sources

- https://store.steampowered.com/app/2497920/
- GitHub search results listing single-player BepInEx mods, e.g. https://github.com/ITR13/A-Difficult-Game-About-Climbing-Mods
- https://github.com/BepInEx/BepInEx/releases/tag/v5.4.23.5
