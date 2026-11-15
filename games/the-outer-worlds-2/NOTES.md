# The Outer Worlds 2 (2025): co-op notes

Triage (phase 0) and a first research pass (phase 1), done from public sources on 2026-11-15
without the game installed. Everything that needs the install is listed under Unknowns.

## Observations (sourced)

- Steam app 1449110, released 2025-10-29, Obsidian Entertainment / Xbox Game Studios. Windows only
  (no macOS or Linux build), DirectX 12, 110 GB, SSD required, 16 GB RAM minimum. Steam categories:
  single-player only, Steam Cloud. No third-party DRM notice on the store page.
  [Steam appdetails](https://store.steampowered.com/api/appdetails?appids=1449110)
- Engine Unreal Engine 5, build 5.4.4 (ref-checked 2026-01-22). Internal project name `Arkansas`:
  config in `%LOCALAPPDATA%\Arkansas\Saved\Config\Windows\`. Steam saves in
  `%USERPROFILE%\Saved Games\TheOuterWorlds2\`, Steam Cloud on. Anti-cheat field empty. Also sold on
  Battle.net and the Microsoft Store / Game Pass (other builds).
  [PCGamingWiki](https://www.pcgamingwiki.com/wiki/The_Outer_Worlds_2)
- Single-player mods exist, including a console enabler + Blueprint mod loader and an MO2 plugin.
  [Nexus: Console Enabler and BP ModLoader](https://www.nexusmods.com/theouterworlds2/mods/1),
  [Nexus: MO2 plugin](https://www.nexusmods.com/theouterworlds2/mods/45)
- A UEVR (VR injection) mod exists: [letmein-vr/OUTERWORLDS2_UEVR](https://github.com/letmein-vr/OUTERWORLDS2_UEVR).
- No official multiplayer and none announced.
  [Insider Gaming](https://insider-gaming.com/does-the-outer-worlds-2-have-multiplayer/)

## Existing co-op work

None found, 2026-11-15, by: this repo's DIRECTORY.md, `games/`, issues and PRs ("outer worlds");
web search for "The Outer Worlds 2" co-op or multiplayer mod; Nexus via web search; GitHub repo
search ("outer worlds coop", "outer worlds multiplayer", "outer worlds 2 mod", "outerworlds2",
"outer worlds ue4ss"). Not checked: Reddit, Discord servers, Nexus browsed directly, GitHub code
search, forks and branches.

Closest same-engine projects in DIRECTORY.md (both open source, both qa `none`):

- [Gothic 1 Remake Coop](https://github.com/CreativeNa/gothic1-remake-coop): UE 5.4, injected
  native DLL, puppet players over direct IP, prototype.
- [NOLA-Stalker2-Coop](https://github.com/Joaovitor404silva/NOLA-Stalker2-Coop): UE5, claims
  story, enemy and world sync, partial.

## Derived (not yet checked against the install)

- Path 2, build from zero: there is no co-op mod to revive.
- Hook point: UE4SS (Lua/C++) or an injected DLL, with a Dumper-7 SDK (playbook/tools.md).
  With the stock UE layout UE4SS goes in `<game dir>\Arkansas\Binaries\Win64\`.
- The UEVR mod and the BP mod loader suggest DLL injection and pak loading work on this build.
- Test copies must keep saves and Steam Cloud isolated (methods.md, Shadow of War lesson): back up
  and hash `%USERPROFILE%\Saved Games\TheOuterWorlds2\` before the first test launch.

## Guesses

- No Denuvo (based only on the missing DRM notice).
- The exe may still ship UE's netcode (`IpNetDriver`, `open <map>?listen`) as Genesis Alpha One
  did; the single-player Blueprints won't be written for replication either way.
- Two copies at once for bot QA need well over the 16 GB RAM minimum.

## Unknowns (record on the game PC, triage step 1)

- Steam build id, main exe name and SHA-256, exact UE version from the exe's version info.
- SteamStub or other protection on the exe; whether UE4SS loads at start-up or needs late injection
  (Crash Bandicoot 4 lesson in engine-families.md).
- Single-instance checks: can two copies run on one PC?
- Whether a working NetDriver is configured or can be.
- Where the game writes besides the save folder (settings, cloud, logs).

## Playbook gap

The README quick start doesn't say the agent must run on the PC that runs the game. An agent on a
machine that can't run the game can only do research. Proposed: one line in the quick start.
