# Sally Face co-op research notes

Date: 2026-11-01

Claim: https://github.com/cranebeaker/coop-anything/issues/294

## Observations

- Target copy: Steam on Windows. The installed game's `changeLog.txt` reports version 1.5.53.
- The Steam app manifest reports build ID 24952397.
- `Sally Face.exe` reports Unity 2022.3.62f3. `GameAssembly.dll` and `Sally Face_Data/il2cpp_data/Metadata` are present; no `Managed` directory was found. This is an IL2CPP build.
- The installed `global-metadata.dat` header reports metadata version 31.
- Cpp2IL `2022.1.0-pre-release.21` was run against an isolated copy. It detected Unity 2022.3.62f3, parsed metadata version 31.1, processed 46,804 method definitions, and generated dummy assemblies and Diffable C# output successfully. This confirms the tool can parse this build; it does not establish that the output is complete or that a mod can run.
- BepInEx Unity IL2CPP Windows x64 build `6.0.0-be.791+b782da4` was extracted into the isolated copy. The game was not launched with it, so startup compatibility is unknown.
- The game appears to use Adventure Creator, based on generated type names including `AC.Player`, `AC.PlayerInput`, `AC.PlayerData` and `AC.SaveSystem`. Only a small number of game-specific managed script types appeared in `Assembly-CSharp`, including `CustomMainMenuHandler`, `SavingUIManager` and `ScriptedVariableApplier`. This is a static inspection of generated output; gameplay logic may also be in serialized assets.
- Steam describes the game as single-player: https://store.steampowered.com/app/541570/Sally_Face__Episode_One/
- No Sally Face entry was present in Co-op Anything's directory when checked: https://github.com/cranebeaker/coop-anything/blob/main/DIRECTORY.md
- Sally Face searches of Co-op Anything issues and pull requests were checked. The current claim is issue #294; no earlier game implementation was found in those searches.
- Web searches for Sally Face co-op/multiplayer mods did not surface a Sally Face project. One generic co-op guide result was unrelated (it describes jobs and vehicles): https://steamcommunity.com/sharedfiles/filedetails/?id=3690823294
- A search of GameBanana, ModDB and Nexus found no co-op project, but listing coverage was incomplete (some pages are JavaScript-rendered): https://gamebanana.com/games/10512 · https://www.moddb.com/games/sallyface/mods · https://www.nexusmods.com/sallyfaceepisodeone/mods/top
- The publisher's itch.io page labels Episode One version 1.5.52: https://portablemoose.itch.io/sallyface
- The publisher's version 1.5.46 update says that build used Unity 2021.3.45f2: https://portablemoose.itch.io/sallyface/devlog/1069775/version-1546-update
- BepInEx's Unity IL2CPP installation guide says its Windows x64 IL2CPP builds are available through Bleeding Edge, and first launch generates support files: https://github.com/BepInEx/bepinex-docs/blob/master/articles/user_guide/installation/unity_il2cpp.md
- Il2CppDumper's README lists support through Unity 2022.2, below this game's reported 2022.3.62f3: https://github.com/Perfare/Il2CppDumper
- Cpp2IL release notes mention metadata v31 support for some late Unity 2022.3 patch versions: https://github.com/SamboyCoding/Cpp2IL/releases
- Co-op Anything's Dave the Diver write-up describes a same-screen second-player clone in one Unity IL2CPP instance; Crime Scene Cleaner describes IL2CPP co-op using two clients. These are technique references, not evidence the code transfers: https://github.com/cranebeaker/coop-anything/tree/main/games/dave-the-diver · https://github.com/cranebeaker/coop-anything/tree/main/games/crime-scene-cleaner

## Inferences

- The starting route appears to be building from zero, based on the searches above. Searches are not exhaustive.
- Same-PC split screen is a later layer: the split-screen playbook says a single-player game needs co-op support first: https://github.com/cranebeaker/coop-anything/blob/main/playbook/split-screen.md
- Use the installed Steam build as the target identity; the older publisher release note does not establish the engine version of this newer Steam build.
- Adventure Creator may provide reusable player and save systems, but the second-player and story-state design cannot be chosen from type names alone.

## Guesses

- None.

## Unknowns

- Whether generated Cpp2IL output contains the methods and types needed to implement a second player.
- Feasibility of adding a second player, and how story flags, dialogue, puzzle interaction and saves should behave.
- Whether multiple Steam instances can run on this PC; this would not itself add co-op to the game.
- BepInEx startup compatibility with this exact build; it has not been launched.
- Whether searches of community listings and discussion sites missed an existing attempt.
- No co-op mod has been built or tested.
