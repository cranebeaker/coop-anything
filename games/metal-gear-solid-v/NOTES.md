# Metal Gear Solid V: The Phantom Pain: co-op notes

Research-only pass (no code written, no game installed, no reverse engineering done by us).
All findings are from public pages, the GitHub API, and web search. Dates are when verified.

## Game identity

| | Value |
|---|---|
| Game | Metal Gear Solid V: The Phantom Pain |
| Release | 2015 |
| Engine | Fox Engine (Konami, closed/proprietary) |
| Scripting | Lua 5.1, embedded; most game logic is scripted |
| Store tested | none by us; mods target the Steam release (AppID 287700) |
| Exe hash | unknown (not computed; no install touched) |

Not yet in coop-anything's DIRECTORY.md before this pass. Related game not covered here:
MGSV: Ground Zeroes (separate game, same engine); no co-op project found for it, not searched
in depth.

## Goal

Play The Phantom Pain in co-op with friends. Only a 2-player route exists today (Dynamite).
Nothing supports 3+ players in a playable build yet.

## Research log

### 2026-10-28 Phase 0-1: triage + research (web only)

#### Found (with sources)

| Find | What it really is | License | Last activity | Players | Use for us |
|---|---|---|---|---|---|
| [unknown321/dynamite](https://github.com/unknown321/dynamite) | The real one. 2-player co-op mod, "early development stage". Real mission playthroughs on video (missions 5, 15). PC Gamer: "a functioning Metal Gear Solid 5 co-op mod". Ships a `masterserver/` and a `hook/`. Requires unmodified Steam copy (JP not supported) and a game-version **rollback** before install (issue #136). | GPL-3.0 **+ Commons Clause** (no selling) | pushed 2026-09-29; created 2025-07-20; v0.6.0 video 2025-10-31; 476 stars, 55 open issues | 2 | Path 1 target: revive/finish. Bug reports in its GitHub issues. |
| [Hound-Protocol/Joint-Operations](https://github.com/Hound-Protocol/Joint-Operations) | Aims for up to 16 players (1 host + 15 clients) in the open world, Windows + Linux/Proton. Heavy org/docs/research scaffolding; README states **"Nothing is playable yet."** First build 0.1 (2 players) is *planned*. Plans to use the game's own online functionality + Steamworks SDK. | GPL-3.0-only (docs CC BY-SA 4.0) | pushed 2026-10-25; created 2026-10-21; 0 stars, 0 issues | up to 16 (planned) | Watch. Public research/decision notes may be useful. Clean-room IP policy (won't copy Dynamite code). |
| [alex-ppg/mgs-v-multiplayer-mod](https://github.com/alex-ppg/mgs-v-multiplayer-mod) | Placeholder repo. One-line README; co-op stated as the first target. No code yet. | CC0-1.0 | created + pushed 2026-10-25; 0 stars, 0 issues | unknown | Watch; nothing usable yet. |
| MGSV Modding Wiki: [Infinite Heaven](https://www.nexusmods.com/metalgearsolidvtpp/mods/1226), [V_Framework](https://mgsvmoddingwiki.github.io/V_Framework/), [IHHook](https://mgsvmoddingwiki.github.io/IHHook), [Zeta](https://mgsvmoddingwiki.github.io/Zeta) | The modding stack, not co-op itself. Infinite Heaven = big Lua mission framework. V_Framework = Lua layer exposing UI/sound/soldiers/bosses/vehicles ("breaks the wall between Lua and the exe"), needs IH + game 1.0.15.3/1.0.15.4. IHHook = `dinput8.dll` proxy adding Lua C API. Zeta = in-game mod manager/framework. | varies | current | The reason co-op is feasible here at all: native hook (IHHook) + rich Lua control (IH/V_Framework). |
| [TinManTex/MockFox](https://github.com/TinManTex/MockFox) | Runs the game's Lua + IH scripts outside the game in a Lua interpreter; used to generate IH docs. | see repo | - | Offline Lua analysis without launching the game. |

#### Observed (facts from sources)

- Fox Engine is closed/proprietary (Konami); no decomp or source port exists.
- MGSV is heavily Lua 5.1 scripted; Lua lives in `data1.dat`/patch `.dat` and inside `fpk` files.
  This is what the IH/V_Framework stack hooks.
- FOB (Forward Operating Base) is asynchronous PvP infiltration; Metal Gear Online (MGO) is a
  separate, unrelated multiplayer mode. Neither is co-op campaign.
- Dynamite is the only build anyone reports actually playing co-op in.

#### Derived (not verified by us)

- This is a **Path 1 (revive/finish)** situation, not build-from-zero: a functional 2-player
  mod exists. The honest play-now answer for a player is Dynamite.
- Dynamite's `masterserver/` + "online connection" requirement implies hosted matchmaking and/or
  relay, with the game instances synced host-authoritatively. The exact transport is **not
  confirmed** from public docs read so far.
- A co-op campaign on the Fox Engine almost certainly rides on the Lua layer (IH/V_Framework) +
  a native hook (IHHook-style `dinput8.dll` proxy) for state/entity sync, since there is no
  usable built-in co-op netcode and no source.

#### Unknown (next to check)

- Dynamite's exact networking model (direct-IP? relay? P2P + master server for matchmaking?) and
  its supported game build number / the exact version to roll back to (issue #136).
- What Dynamite actually syncs beyond "two players in the same mission": world state, Mother Base,
  items, save/progression ownership. Needs reading its docs (`webui/docs/docs.md`, `docs/MAKING_OF.md`)
  and/or a test install.
- Whether Dynamite builds on Infinite Heaven or hooks the game independently.
- License practicality: GPL-3.0 + Commons Clause allows forking/derivatives (non-commercial);
  fine for reviving, not for selling.
- Ground Zeroes: any co-op attempt (not searched in depth).

### 2026-10-28 Phase 1b: Dynamite deep-dive (its docs + 55 open issues)

Read `docs/MAKING_OF.md` (a long build diary) and `webui/docs/docs.md` (user docs), and listed the
open issues via the GitHub API. No install, no code. This resolves the networking/version/what-syncs
unknowns above.

#### Observed (from the author's own docs)

- **Origin:** author (unknown321) built a MGSV master-server reimplementation (`fuse`, Go+SQLite) to
  connect two players on a FOB map, then swapped the map/mission to single-player ones -> dynamite.
  First two-player contact 2025-04-07.
- **Why it is possible:** a Fox Engine session is natively multiplayer (up to 16). Single-player
  missions run a 1-player session; FOB missions run the same session but let a second player join.
- **Connection:** Steam P2P session, host accepts incoming connections, client supplies the host's
  **SteamID** (via UI target or hardcoded). A Steam-network connection is required to broker it; after
  connecting, the client knows the host's IP (so there is a griefing surface -> black/whitelists in
  `dynamite.log.txt`). Client must press Master+UP (`Dynamite.CreateClientSession`) after the host
  loads, every time.
- **Two player instances:** single-player missions allocate one player; a `.fox2` definition edit adds
  the second instance so both render.
- **Host authority:** enemies are hardcoded to attack player 0 (host) and ignore the joiner
  ("defender"). Enemy AI is host-controlled; movement commands are sent to the client. Dynamite patches
  targeting to "nearest player entity."
- **Native layer:** started as pure Go exe byte-patching (no DLL) for portability; targeting logic
  forced a move to C++ via an **IHHook**-based `dinput8.dll` proxy (IHHook is part of Infinite Heaven,
  but dynamite is NOT IH-based and rejected building on IH for readability/compat reasons).
- **Markers:** first hacked over an `AddLocalDamage` packet (6 floats), later replaced by the game's
  in-game network messaging. Only ~11 of the game's entity sync instances are implemented.
- **Save safety:** save location is defined in Lua, so dynamite backs up + copies the save and runs on
  the copy (tool `squib`); single-player data is untouched, online funds/resources stubbed.
- **Packaging:** single cross-platform Go binary; own repack tool `datfpk` (rejected SnakeBite/Makebite
  and GzsTool as unworkable on Linux / for rapid iteration); config is an `.ini` edited through a local
  web UI; local `fuse` on port 6667 avoids sending co-op records to Konami (ban risk).
- **Platforms:** Windows 10+ and Linux/SteamOS (Proton needs `WINEDLLOVERRIDES="dinput8.dll=n,b"`).
- **Version:** latest release v0.6.0 (2025-10-31); master pushed to 2026-09-29. Issue #136: the
  2026-06-09 Steam update breaks launch, hence the mandatory rollback (exact pre-patch build id not
  stated).

#### What it really syncs (observed)

| Area | State |
|---|---|
| Players visible | Buggy. Partner avatar often renders only in FOB mode, not regular missions (#109, #59). |
| Enemies | Partial. Vitals synced; positions and FOV NOT synced (#112, #113); AI host-driven. |
| Story/objectives | Partial. Secondary objectives + objective markers not shared (#115, #117); some update host-only (#118). |
| World state | Partial. Weather host-only; gimmicks/physics desynced; outpost capture not announced to client (#98). |
| Items/loadout | Per-player and partial. Loadout drop invisible to partner; ammo drop can be the wrong player's; weapon pickup "not allowed" (#87, #45). |
| Vehicles | Broken. One instance only, position desync, driver frozen; tank-as-host can crash (#128, #35, #42, #47). |
| Buddies | Quiet/D-Dog local (unseen by partner); D-Horse teleport desync (#43, #90, #122). |
| Markers (user + enemy) | Working (custom sync). |

#### Bug clusters (from the 55 open issues)

- **Connection/installer:** #136 (2026-06-09 patch breaks launch), #129/#127 connect fails, #92/#76/#102 installer, #85 (new save can't connect until missions 1&2 unlocked).
- **Enemy sync:** #112 positions, #113 FOV, #36 revenge, #55 skulls.
- **Player sync:** #109/#59 invisible partner, #29/#31 animations, #87 weapon-not-allowed, #94 cargo fulton.
- **Objectives:** #115 markers, #117 secondary objectives, #118/#95/#83 per-mission.
- **Vehicles/Walker Gear:** #128/#35/#42/#47/#19, #130/#116/#21.
- **Per-mission crashes:** #3 (#13 Pitch Dark), #16 (#43), #103 (#30), #15 (#28), #124 (#10).
- **Enhancements/roadmap:** #58 free roam, #123 invite-based join, #4 FOB items; `>2 players` (author's stated priority, currently fails to start).

#### Resolved from the earlier "Unknown" list

- Networking model: **confirmed** (Steam P2P by SteamID + local fuse master server; host-authoritative).
- What it syncs: **mapped** (table above).
- Builds on Infinite Heaven? **No**, uses IHHook only.
- Supported version: rollback target is the build **before the 2026-06-09 Steam patch** (exact id still unknown).
- Still open: exact pre-patch build number; whether unreleased master fixes change any of the above.

## Sources

- https://github.com/unknown321/dynamite
- https://github.com/unknown321/dynamite/blob/master/docs/MAKING_OF.md
- https://github.com/unknown321/dynamite/blob/master/webui/docs/docs.md
- https://github.com/unknown321/dynamite/issues
- https://www.pcgamer.com/games/action/after-10-years-theres-a-functioning-metal-gear-solid-5-co-op-mod-so-you-and-a-friend-can-be-played-like-a-damn-fiddle-together/
- https://github.com/Hound-Protocol/Joint-Operations
- https://github.com/alex-ppg/mgs-v-multiplayer-mod
- https://mgsvmoddingwiki.github.io/V_Framework/
- https://mgsvmoddingwiki.github.io/IHHook
- https://www.nexusmods.com/metalgearsolidvtpp/mods/1226
- https://github.com/TinManTex/MockFox
