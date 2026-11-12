# Abo Khashem (2018): working notes

Session 1, 2026-11-12. Phase 0 (triage) and phase 1 (research), done without the game files.

## Observations (sourced)

- Comedic open-world RPG by Moving Dimensions (Saudi studio), released 2018-02-22/23.
  Steam app 662840, $29.99, still purchasable on 2026-11-12. https://store.steampowered.com/app/662840
- Steam lists it as single-player only, no multiplayer or co-op category. Same store page.
- Also released on PS4 and Xbox One, 1 player, no online. https://www.moddb.com/games/abo-khashem,
  https://vandal.elespanol.com/juegos/pc/abo-khashem/57030
- SteamDB detects Unity Engine. Last record update 2025-10-19. https://steamdb.info/app/662840
  (page blocked for fetch; read from search result text)
- SteamDB depots: 662841 Windows 32-bit, 662842 macOS, 662843 Windows 64-bit, about 3.1 GiB each.
  https://steamdb.info/app/662840/depots (search result text)
- Languages: English and Arabic.

## Derived

- Steam minimum spec text ("SSE2", "DX9 shader model 3.0 or DX11 feature level 9.3") is Unity's
  stock requirement wording, which agrees with SteamDB's Unity detection.
- A 2017/2018 Unity build is likely 5.x or 2017.x. If it is a Mono build, Assembly-CSharp.dll
  decompiles to readable C# and BepInEx can load a mod, which makes it a "build from zero" target
  in the easiest engine family (playbook/engine-families.md, Unity row).

## Guesses (unverified)

- Mono scripting backend rather than IL2CPP.

## Unknowns (need the owner's install)

- Exact build id / version string, SHA-256 of the exe, Unity version, Mono or IL2CPP
  (look for `<game>_Data/Managed/Assembly-CSharp.dll` vs `GameAssembly.dll`).
- Whether the exe calls Steamworks (steam_api.dll / steam_api64.dll present?).
- Save format and location.
- Store page lists Windows only while SteamDB shows a macOS depot. Unresolved.

## Research log

| Where | Query | Result |
|---|---|---|
| This repo | DIRECTORY.md, games/, grep "khashem" | nothing |
| This repo issues and PRs | REST search "khashem" | 0 issues, 0 PRs |
| Web (EN) | "Abo Khashem multiplayer co-op mod", "... nexusmods OR bepinex OR online mod" | no co-op or multiplayer mod found |
| Web (AR) | "لعبة أبو خشم", "ابو خشم مود اونلاين مع الاصدقاء" | no mod found |
| GitHub repo search | "abo khashem" | 1 hit: MAF5YT/Abo-Khashem, single commit 2026-09-24, README is only a title. Not a mod. |
| Not checked | ModDB/Nexus file tabs directly, Steam community discussions, Reddit, archive.org, developer Discord | |

Conclusion so far: no co-op project found by the searches above. Path: build from zero
(playbook/how-coop-mods-work.md), pending the identity checks above.
