# Metal Gear Solid V: The Phantom Pain (2015)

| | |
|---|---|
| Engine | Fox Engine (Konami, closed/proprietary; Lua 5.1 scripting) |
| Versions tested | none by us (research only); mods target the Steam release, AppID 287700 |
| Projects | [Dynamite](https://github.com/unknown321/dynamite) (2p), [Joint Operations](https://github.com/Hound-Protocol/Joint-Operations) (planned 16p), [alex-ppg mod](https://github.com/alex-ppg/mgs-v-multiplayer-mod) (placeholder) |
| QA level | none (no testing done by us) |
| Last updated | 2026-10-28 |

## Where things stand

Two players can play The Phantom Pain together today using **Dynamite**, an early but
functioning co-op mod (PC Gamer called it "a functioning Metal Gear Solid 5 co-op mod",
with full mission playthroughs on video). It is explicitly early-development and buggy, needs
an unmodified Steam copy (not the Japanese version), and requires rolling the game back one
patch before installing. There is **no playable route for 3+ players**: Joint Operations aims
for 16 but "nothing is playable yet," and alex-ppg's repo is a one-line placeholder. We did
research only this pass; nothing here was installed, reverse-engineered, or tested by us.

## Existing work

- **[Dynamite](https://github.com/unknown321/dynamite)** by unknown321, 2-player co-op, "early
  development stage." 476 stars, 55 open issues; latest release v0.6.0 (2025-10-31), master ahead to
  2026-09-29 (created 2025-07-20). Windows 10+ and Linux/SteamOS (Proton via a `dinput8.dll` override).
  Requires an unmodified Steam copy (JP unsupported), a Steam connection, ~1 GB free, and a friend; a
  100% save is recommended to reach all working missions. You **must roll the game back past the
  2026-06-09 Steam patch**, which breaks launch (issue
  [#136](https://github.com/unknown321/dynamite/issues/136)). License: **GPL-3.0 with the Commons
  Clause**. Single Go binary; bundles its own master server ([fuse](https://github.com/unknown321/fuse))
  and save-safety tool ([squib](https://github.com/unknown321/squib)). Docs: `docs/MAKING_OF.md` (a
  detailed build diary) and `webui/docs/docs.md`. This is the one build players and press report
  actually playing. See NOTES.md for the full architecture + bug breakdown.
- **[Joint Operations](https://github.com/Hound-Protocol/Joint-Operations)** by Hound Protocol,
  aims for up to 16 players (1 host + 15 clients) in the open world, Windows + Linux/Proton, using
  the game's own online functionality and the Steamworks SDK. As of 2026-10-25 it is organization,
  documentation, website and desk research only; the README states **"Nothing is playable yet"** and
  the first (2-player) build, 0.1, is planned. GPL-3.0-only (docs CC BY-SA 4.0); clean-room IP policy
  (will not copy Dynamite's code).
- **[alex-ppg/mgs-v-multiplayer-mod](https://github.com/alex-ppg/mgs-v-multiplayer-mod)**, a
  placeholder repo created 2026-10-25, one-line README, co-op named as the first target, no code yet.
  CC0-1.0.

## How the game works (for co-op)

- **Engine:** Fox Engine is closed and proprietary; there is no decompilation or source port, so
  any co-op work is native hooking plus scripting, not code changes to an open engine.
- **Scripting:** most game logic is **Lua 5.1**, embedded in the executable, loaded from
  `data1.dat`/patch `.dat` files and from inside `fpk` archives. This is the lever the modding scene
  pulls on.
- **Modding stack that makes co-op feasible:**
  [Infinite Heaven](https://www.nexusmods.com/metalgearsolidvtpp/mods/1226) (large Lua mission
  framework), [V_Framework](https://mgsvmoddingwiki.github.io/V_Framework/) (a Lua layer exposing
  UI, sound, soldiers, bosses and vehicles that the game didn't expose; needs IH and game 1.0.15.3 or
  1.0.15.4), and [IHHook](https://mgsvmoddingwiki.github.io/IHHook) (a `dinput8.dll` proxy DLL that
  adds a Lua C API to the statically-compiled game Lua). [MockFox](https://github.com/TinManTex/MockFox)
  can run the game's Lua and IH scripts outside the game for offline analysis.
- **Netcode (how Dynamite actually does it, from its `MAKING_OF.md`):** the shipped game has no co-op
  *campaign* netcode, but a Fox Engine session is natively multiplayer (up to 16 players); single-player
  missions just run a 1-player session, and **FOB** missions run the same session with a second player
  allowed to join. Dynamite hijacks the FOB "start mission" connection path: a **Steam P2P** session
  connects host and client **by SteamID** (a Steam-network connection is required to establish it), then
  the mission id/geometry is swapped to a single-player mission and a second player instance is allocated
  via a `.fox2` edit. It is **host-authoritative**, player 0 is the host; enemy AI and targeting are
  driven by the host (targeting patched to "nearest player," since FOB enemies normally ignore the
  joiner/"defender"). Markers are synced through an in-game network-message channel (originally a
  damage-packet hack). Native patches are done with an **IHHook**-based `dinput8.dll` proxy plus direct
  exe byte-patching in Go; it does **not** build on Infinite Heaven.
- **Master server:** co-op records would otherwise go to Konami's master server and risk an online ban,
  so Dynamite bundles **fuse** (a Go/SQLite re-implementation) running locally on port 6667.
- **Save safety:** Dynamite copies your save and runs co-op on the copy (tool: **squib**); your
  single-player data is left untouched, and online funds/resources are stubbed, not transferred.

## What we changed

Nothing. This pass is research and directory/write-up only.

## QA

No tests were run by us. QA level: `none`. Reported play (Dynamite) is from the author's gameplay
videos and press coverage, not structured testing.

## How to play

For 2 players today: install **Dynamite** per its own documentation
(https://github.com/unknown321/dynamite), unmodified Steam copy (not JP), roll the game back one
patch first (issue #136), then follow the shipped user docs. Players run these steps on their own
machine; coop-anything does not distribute the game or the mod.

## Known issues

What a player will actually notice in Dynamite (categorised from its 55 open issues + `MAKING_OF.md`):

- **Scope:** 2 players only (3+ fails to start); mission-based from a curated list, **no free roam**;
  various missions are unplayable or crash-prone.
- **Players:** your partner's avatar often only renders in FOB mode, not in regular missions (so they
  can look invisible); missing animations when carrying bodies or hiding in toilets.
- **Enemies:** positions and field-of-view are not synced between the two PCs (vitals are); each side
  runs its own AI, so a soldier can be in different places for each player.
- **Objectives/markers:** secondary objectives and objective markers are not shared; some objectives
  update for the host only.
- **Vehicles:** tanks/cars have spawn and position-sync bugs (host picking a tank can crash the game);
  Walker Gear is invisible to the other player.
- **Buddies:** Quiet/D-Dog are local (not seen by the partner); D-Horse causes teleport desync.
- **Flow:** death → respawn after ~10 s (no mission-fail); restarting from the pause menu hangs on an
  infinite loading screen; fultoning another player fails the mission.
- **Setup:** requires a game-version rollback (undone whenever Steam updates the game); JP version
  unsupported; co-op won't connect during Steam weekly maintenance.

## Next steps

Networking, supported version, and what-syncs are now mapped (see above and NOTES.md). Remaining:

- **Highest-value fixes by the author's own priority:** `>2 player` support (session fails to start)
  and **enemy position/FOV sync** (#112, #113), the two that most change the "is this really co-op"
  feeling. The marker channel already shows a working custom-sync path to copy.
- Build from source (`docs/BUILD.md`, Go) and set up **isolated test installs + bot-driven QA**
  (`playbook/automated-qa.md`, `tools/`) so fixes are verified without a human playtest.
- Diff vanilla vs. Dynamite scripts with the author's own method (unpack game, StyLua-format, `meld`)
  to see exactly where each patch lives before changing it.
- Confirm the exact pre-2026-06-09 build number to roll back to (issue #136 only gives the date).
- Watch Joint Operations' 0.1 (2-player) build and update the directory when it ships.

