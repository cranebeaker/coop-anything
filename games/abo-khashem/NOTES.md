# Abo Khashem (2018): working notes

Session 1, 2026-11-12. Phase 0 (triage) and phase 1 (research), done without the game files.
Session 2, 2026-11-12. Phase 1 finished: the sources session 1 left unchecked (Steam community,
ModDB and Nexus tabs, official site, archived presskit). Phase 2 is on hold, see "Open question".

## Observations (sourced)

- Comedic open-world RPG by Moving Dimensions (Saudi studio), released 2018-02-22/23.
  Steam app 662840, $29.99, still purchasable on 2026-11-12. https://store.steampowered.com/app/662840
- Steam lists it as single-player only, no multiplayer or co-op category. Store categories on
  2026-11-12: Single-player, Steam Achievements, Remote Play on TV, Family Sharing (no Remote Play
  Together). Same store page.
- Steam minimum OS on the store page: "Windows XP SP2+". Only DLC: the soundtrack.
- Also released on PS4 and Xbox One, 1 player, no online. https://www.moddb.com/games/abo-khashem,
  https://vandal.elespanol.com/juegos/pc/abo-khashem/57030
- ModDB lists engine Unity and platforms Windows, Mac, XONE, PS4. https://www.moddb.com/games/abo-khashem
- SteamDB detects Unity Engine. Last record update 2025-10-19. https://steamdb.info/app/662840
  (page blocked for fetch; read from search result text)
- SteamDB depots: 662841 Windows 32-bit, 662842 macOS, 662843 Windows 64-bit, about 3.1 GiB each.
  https://steamdb.info/app/662840/depots (search result text)
- Languages: English and Arabic (interface, full audio, subtitles).
- Last patch: 1.0.7.5, dated 3/27/18 on the official patch notes page and posted on Steam on
  2018-04-03, live on PS4, Xbox One and Steam. The one before was 1.0.6.2 (2018-02-27/28).
  https://www.abokhashem.com/patchnotes, https://steamcommunity.com/app/662840/allnews/
- The developer's presskit (archived 2018-09-28) calls it a "Comedic multi-genre single player
  RPG" and lists "Fun playable companion" among the features: Shukman the lizard, who travels with
  the hero. Patch 1.0.6.2 mentions "Improved jumping with Abo Khashem and Shukman".
  https://web.archive.org/web/20180928225312/http://moving-dimensions.com/press/
- The official site links Facebook, Twitter, YouTube and the presskit; no Discord or modding
  links. The live presskit URL (moving-dimensions.com/press/) returns 404. https://www.abokhashem.com/home

## Derived

- Steam minimum spec text ("SSE2", "DX9 shader model 3.0 or DX11 feature level 9.3") is Unity's
  stock requirement wording, which agrees with SteamDB's and ModDB's Unity listing.
- A 2017/2018 Unity build is likely 5.x or 2017.x. If it is a Mono build, Assembly-CSharp.dll
  decompiles to readable C# and BepInEx can load a mod, which makes it a "build from zero" target
  in the easiest engine family (playbook/engine-families.md, Unity row).
- Shukman is already a second, playable character that follows the hero. Giving player 2 control
  of Shukman (instead of spawning a second hero) may avoid most "only one hero" assumptions in
  quests and dialogue. A design idea to test in phase 3, nothing more.

## Guesses (unverified)

- Mono scripting backend rather than IL2CPP.

## Unknowns (need a store install)

- Exact build id / version string, SHA-256 of the exe, Unity version, Mono or IL2CPP
  (look for `<game>_Data/Managed/Assembly-CSharp.dll` vs `GameAssembly.dll`).
- Whether the exe calls Steamworks (steam_api.dll / steam_api64.dll present?).
- Save format and location.
- Store page lists Windows only while SteamDB shows a macOS depot and ModDB lists Mac. Unresolved.

## Open question (phase 2 on hold)

The owner's copy could not be matched to a store yet, so no identity is recorded here. Phase 2
waits for a copy installed from a store (Steam app 662840), whose build id can be read from
`steamapps/appmanifest_662840.acf`.

## Research log

| Where | Query | Result |
|---|---|---|
| This repo | DIRECTORY.md, games/, grep "khashem" | nothing |
| This repo issues and PRs | REST search "khashem", "abo khashem", "moving dimensions", Arabic "خشم" (session 2) | 0 issues, 0 PRs |
| Web (EN) | "Abo Khashem multiplayer co-op mod", "... nexusmods OR bepinex OR online mod" | no co-op or multiplayer mod found |
| Web (AR) | "لعبة أبو خشم", "ابو خشم مود اونلاين مع الاصدقاء" | no mod found |
| GitHub repo search | "abo khashem", "khashem", "abokhashem", "shukman" (session 2) | 1 hit: MAF5YT/Abo-Khashem, single commit 2026-09-24, README is only a title, 0 forks. Not a mod. |
| GitHub code search | "abo khashem" multiplayer, khashem bepinex, khashem harmony patch (session 2) | game databases, sales datasets and store lists only; no mod code |
| Steam community | all 20 discussion topics read by title; forum search "multiplayer" and "coop"; guides tab (session 2) | no topic about multiplayer, co-op or mods; both searches 0 results; 0 guides |
| ModDB | mods tab, files tab (session 2) | 0 mods; 1 file, an "Installer Tool" from 2018-03-11 (not a mod, not downloaded) |
| Nexus Mods | site search "abo khashem" (session 2) | no results, no game page |
| Official site, presskit | abokhashem.com, archived presskit on archive.org (session 2) | no Discord, no modding or multiplayer mention |
| Not checked | Reddit (blocked for my tools), SteamDB patch notes (bot check page, not passed) | |

Conclusion: no co-op project found by the searches above, and no community hub beyond the Steam
forum. Path: build from zero (playbook/how-coop-mods-work.md), pending the identity checks above.
