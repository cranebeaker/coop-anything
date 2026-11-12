# Abo Khashem (2018)

| | |
|---|---|
| Engine | Unity (per SteamDB and ModDB; not yet confirmed from a store build's files) |
| Versions tested | none. Latest public patch is 1.0.7.5 (2018-03-27, Steam, PS4, Xbox One) |
| Projects | none found; gap entry in `data/directory.yaml` |
| QA level | none |
| Last updated | 2026-11-12 |

## Where things stand

Nobody can play the story together today. Abo Khashem is single-player on Steam, PS4 and Xbox
One, and the searches in [NOTES.md](NOTES.md) (web in English and Arabic, GitHub, ModDB, Nexus,
the Steam forum, the official site and its archived presskit) found no co-op or multiplayer mod
and no community hub beyond the Steam forum. The path is build from zero.

## Existing work

None found. Where we looked, with dates and results, is in the research log in
[NOTES.md](NOTES.md). Reddit and SteamDB's patch notes were not reachable with our tools.

## How the game works (for co-op)

From public sources only, nothing read from game files yet:

- Unity engine, so the Unity row of [engine-families.md](../../playbook/engine-families.md)
  applies: BepInEx or MelonLoader load C# mods, and Nitrox, HKMP and QSB show how Unity co-op
  mods are built. Whether this build is Mono or IL2CPP is the first thing to check.
- Open-world comedic RPG: a city to explore, dungeons, vehicles, minigames, a business and
  interior-design layer, and a phone texting system.
- Shukman the lizard is a playable companion who follows the hero (presskit: "Fun playable
  companion"). A player 2 could take over Shukman rather than a second hero.

## What we changed

Nothing yet; research only.

## QA

No mod, no tests yet.

## How to play

There is no co-op build to play.

## Known issues

None; there is no mod yet.

## Next steps

1. Install from Steam (app 662840) into a Steam library and record the identity:
   `buildid` from `steamapps/appmanifest_662840.acf`, SHA-256 of the exe, Unity version from
   `globalgamemanagers`, `Managed/Assembly-CSharp.dll` (Mono) or `GameAssembly.dll` (IL2CPP),
   and whether `steam_api64.dll` is present.
2. Copy the install to a test folder and decompile `Assembly-CSharp.dll`: find the player
   controller, Shukman's controller, the save system and scene loading.
3. Prototype player 2 as a Shukman puppet with a BepInEx plugin, then pick a networking model
   ([networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md)).
