# Star Wars: Knights of the Old Republic (2003)

| | |
|---|---|
| Engine | reone (open-source engine reimplementation), forked |
| Versions tested | Steam (app 32370), `swkotor.exe` v1.0.3.0, buildid 4143502, on Windows x64 |
| Projects | [KotOR co-op](https://github.com/McChaffeeFAF/kotor-coop) (reone fork) - directory entry |
| QA level | automated |
| Last updated | 2026-10-21 |

## Where things stand

Two players can play the KotOR 1 campaign together today, online, by IP:port. The host
runs the story and the guests take the party's companions; the campaign, enemies and
world state are shared. It is an alpha - it is the only working route, it needs a build
from source, and whole systems are still unfinished (see Known issues).

This is a fork of [reone](https://github.com/seedhartha/reone), the free engine
reimplementation for KotOR and KotOR II. The upstream reone is single-player; the fork
adds the co-op layer. The fork is source-only: no GitHub releases, no CI, and it has to
be built with Visual Studio and vcpkg.

**Whose fork it is.** [McChaffeeFAF/kotor-coop](https://github.com/McChaffeeFAF/kotor-coop)
is **not** ours — it is McChaffeeFAF's project, which we found and tested. We only fixed
two bugs in its packaging scripts and sent those fixes back to it; see
[What we changed](#what-we-changed). The engine and the co-op layer are McChaffeeFAF's
work, themselves descended from seedhartha's reone.

## Existing work

- [reone](https://github.com/seedhartha/reone) - the engine reimplementation this forks.
  GPL-3.0. Upstream is single-player. A 2020 GameStar
  [article](https://www.gamestar.de/artikel/sw-kotor-koop,3360443.html) describes a
  multiplayer demo with two human-controlled party members aiming at three-player co-op,
  where only basic gameplay worked in v0.5.1. That is the demo this fork's work descends
  from, and the claim the old directory entry was based on.
- [KotOR co-op (McChaffeeFAF fork)](https://github.com/McChaffeeFAF/kotor-coop) - the
  fork with the online co-op layer. GPL-3.0, alpha-0.4.0 (2026-10-15). The only viable
  open-source route found; there are no usable prebuilt binaries.

A prebuilt-binary hunt found nothing runnable: no releases on the fork, 0 CI runs, 0
issues. A build from source is required.

## How the game works (for co-op)

- **Engine.** reone reimplements BioWare's Odyssey engine; it reads the retail game's
  own data files (`chitin.key`, BIFs, ERFs, 2DAs, GFFs) and never modifies them. KotOR
  II's install is *used for its interface resources and systems* but is not required -
  the engine runs KotOR 1 with the KotOR HUD without it (verified: `k2:"absent"` in the
  session state, `--hud k1`, no `--k2game`).
- **Scripting.** The game logic is BioWare's own script VM (NWScript); the engine runs
  the retail `.ncs` bytecode. The mid-game co-op work is largely about deciding which
  scripted screens make sense for more than one player.
- **Netcode.** The fork adds an authoritative host. The host owns the simulation, the
  campaign state and the conversation flow; guests send *intent* (move, open, attack,
  use) and the host decides and replicates the result. A client's local replica of host
  objects is keyed by the host's object ids, and the state reports replica health
  (`replicas`, `bound`, `unmatched`, `violations`) so drift is visible rather than silent.
- **Sessions.** Story Mode (host runs the story, guests take companions) and Free Mode
  (guests bring their own characters). Networking is direct UDP: host on a port, guests
  `--join ip:port`. A join-code service (`tools/coopserver`, Python/Docker) is optional
  and swaps the shared IP:port for a short code (no port forwarding).
- **Automation.** The engine has a **console control port** (`--control-port N`) for
  scripted testing: plain-text, newline-delimited commands on 127.0.0.1, each answered
  with one JSON line. This is what makes the QA below possible without a human.

## What we changed

Two bugs in the fork's packaging scripts, fixed, **pushed to a personal fork of McChaffeeFAF's repo and
opened as a PR to their project**: [McChaffeeFAF/kotor-coop#1](https://github.com/McChaffeeFAF/kotor-coop/pull/1)
(commits `4ec25f9` and `5d45c38`, rebased on `c56a05e` = alpha-0.4.4). The PR carries the
branch and the diff; until it is merged, a fresh clone of the fork's `main` still shows
both bugs:

1. **The package shipped the wrong VC++ runtime and crashed on startup.** Root cause: the
   packaging script searched Visual Studio installs for a `Microsoft.VC*.CRT` redist
   folder and sorted by *name*, so `2019\BuildTools` beat `18\BuildTools`. It copied
   **VC142 (14.29)** next to an engine built with **VC145 (14.51)**; the app-local
   `msvcp140.dll` shadowed the newer system runtime and the engine died with `0xC0000005`,
   no log, no output. Fix: read the toolset from the engine's own imports
   (`boost_program_options-vc145-...`), select that CRT, warn on a mismatch. Test: the
   packaged engine now runs; the zip's fresh-extracted engine hosts a session.
2. **The launcher refused to start without KotOR II.** Root cause: readiness required
   *both* games, so a KotOR-1-only player got all of Host / Join / solo **disabled** -
   even Play solo - though KotOR-1-only works at the engine level. Fix (Windows
   `launcher.ps1`/`setup.ps1`, Linux `setup.sh`): KotOR II is optional; readiness keys on
   KotOR 1; the HUD follows what is installed. Test: a synthetic first-run with no KotOR
   II now writes `hud=k1` and no `k2game`.

Nothing in the retail install or its saves was touched; all testing ran against a copy.

## QA

- **Two-client suite:** `games/kotor/test/harness.ps1` (published with this write-up). It
  starts two real engine clients on one PC (host and guest, both headless), connects them
  over localhost UDP, drives both through their TCP control ports and asserts the shared
  state on **both** seats. Run it with
  `powershell -ExecutionPolicy Bypass -File harness.ps1` (parameters: `-GamePath`,
  `-BinDir`, ports). **77 gates**, `RESULT: PASS` on two consecutive runs (2026-10-20), on alpha-0.4.0 with both clients on one PC over localhost. Not rerun on the current alpha-0.4.4.
  It covers join/module/movement, dialogue, wallet, containers, level-up, combat, area
  transitions, pause propagation, a guest opening a door in reach, and the area-exit
  ready-check.
- **Unit tests:** 3,650 tests / 526 suites - 3,639 passed, 0 failed, 11 skipped.
- **Packaging:** the Linux launcher harness 32/32; a full **host->join from the built
  package: 9/9** checks - both packaged clients in `end_m01aa`, replication clean
  (`unmatched: 0`, `violations: 0`), traffic both ways, `k2:"absent"`.
- **Not gated (honest gaps):** store, workbench/lab, swoop/turret minigames and Free Mode
  / character generation are not reachable from the control port without a scripted
  merchant/bench or a different session mode. They are recorded, not claimed.

## How to play

1. Build the fork from source (Visual Studio + vcpkg; `-G "Visual Studio 18 2026"`).
   There are no releases - a build is required.
2. Package it with `tools/package/make_tester_package.ps1`; the player runs
   **Play KotOR Co-op.cmd** and picks Host or Join. The launcher finds the installs
   itself.
3. Host: hosts from the launcher and shares their IP:port; the friend joins with
   `--join ip:port`. Or set up the join-code service for short codes.
4. KotOR II is optional; without it the game plays KotOR 1 with the KotOR HUD.

## Known issues

- **Alpha.** Expect unfinished screens and rough edges.
- **Source-only.** No binaries; a developer toolchain is needed to play.
- **One machine.** Both clients ran on one PC over localhost. There is no cross-network test.
- **Suite version.** The 77 gates ran on alpha-0.4.0, not on the current alpha-0.4.4.
- **Unreleased fixes.** Two packaging bugs are fixed on
  [PR #1 to the fork](https://github.com/McChaffeeFAF/kotor-coop/pull/1) but not merged
  yet; until they land, a fresh clone of the fork still crashes on startup and still
  demands KotOR II.
- **Companion-gated screens.** During a conversation the host answers guest commands
  `busy` - by design (the host leads dialogue), but it means a guest cannot act mid-scene.
- **A guest's action is accepted even when it cannot be carried out** - e.g. a door out of
  the companion's reach. The host performs it only when the companion is close enough.
- **Ungated systems** - store, workbench, minigames, Free Mode - may misbehave in co-op;
  nobody has automated them yet.

## Next steps

- Gate the store, workbench and minigame flows (need a scripted merchant/bench or a
  Free Mode session).
- A verified co-op **playthrough** would move QA from `automated` to
  `verified-playthrough`; nobody has finished the campaign in co-op.
- Get [the two packaging fixes](https://github.com/McChaffeeFAF/kotor-coop/pull/1)
  reviewed and merged in the fork; they are an open PR, not on the fork's `main`.
