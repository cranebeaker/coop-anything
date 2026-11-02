# Star Wars: Knights of the Old Republic (2003) — session notes

Agent session: 2026-10-18. Repo: cranebeaker/coop-anything (cloned to `<workspace>/coop-anything`).

## 0. Triage — game identity (rule 5)

Observed on the test machine (read-only, install not modified):

- Store: Steam. App id **32370** (verified via Steam store API: appdetails?appids=32370
  returns "STAR WARS™ Knights of the Old Republic™"; note 32350 is Starfighter, not KOTOR).
- Install dir: `<Steam library>\steamapps\common\swkotor`
- `appmanifest_32370.acf`: StateFlags=4 (installed), buildid **4143502**, installdir `swkotor`.
- Main exe: `swkotor.exe`, size 4,395,008 bytes, FileVersion/ProductVersion **1.0.3.0**,
  ProductName "Star Wars: Knights of the Old Republic".
- SHA-256 of swkotor.exe: `34E6D971C034222A417995D8E1E8FDD9F8781795C9C289BD86C499A439F34C88`
- Engine: BioWare **Odyssey** (Aurora derivative), 32-bit, closed binary, no netcode, no script extender.
- OS family: Windows x64.
- Goal of this session: **online co-op campaign with friends** (not splitscreen).

Observations about the install (may matter later):

- `binkw32.dll` (220,672 B) AND `binkw32_real.dll` (375,808 B) both present, plus
  `hires_patcher.exe/.pl`, `patch.erf`. This binkw32/binkw32_real pairing usually means a
  bink proxy DLL was installed at some point (fix/mod loader). Unknown which, unknown when.
- `32370_install.vdf` sits inside the game folder (Steam install script artifact; content = registry install script incl. SecuROM strings — Steam's own, no SecuROM on the Steam build).
- `Override/`, `logs/` folders exist → game has been run and possibly modded.
- KOTOR II (Steam app **208580**) is **not installed** on this PC (no acf, no folder).

## 1. Research — existing co-op attempts (phase 1)

Repo directory (data/directory.yaml, checked 2026-10-16) had exactly one entry:

1. **reone multiplayer** — https://github.com/seedhartha/reone — decomp-port, prototype,
   3 players, untested, last activity 2025-04. Engine reimplementation (clean-room, GPL-3.0,
   C++). GameStar article (https://www.gamestar.de/artikel/sw-kotor-koop,3360443.html)
   described a multiplayer demo, v0.5.1, only basic gameplay. Upstream says "mid-stage".

New finds this session (not yet in the directory):

2. **McChaffeeFAF/kotor-coop** — https://github.com/McChaffeeFAF/kotor-coop
   reone fork + online co-op (Story Mode: friends take companions; Free Mode). GPL-3.0.
   Host/join over UDP 27015 (port forward), optional join-code server in `tools/coopserver`.
   **Requires both KOTOR 1 and KOTOR 2 installs** (K2 used for interface/systems).
   Activity: 4 commits — initial public release 2026-10-09, alpha-0.3.8 (10-10),
   alpha-0.3.9 (10-10), **alpha-0.4.0 (2026-10-15)**, authored by McChaffeeFAF + claude.
   No releases, **0 CI workflow runs** → no prebuilt binaries available.
   QA status: unknown/untested — claims in README not yet verified by us.

3. **masterxxstabs/KOTOR-COOP** — https://github.com/masterxxstabs/KOTOR-COOP
   Rust, DLL injection into original swkotor.exe + persistent dedicated server + launcher.
   Default branch `claude/kotor-coop-foundation`, 2 commits, updated 2026-10-17.
   README itself states gameplay sync needs unmapped engine addresses ("nobody has mapped
   them yet"); injection/overlay not yet tested in-game. Long way from playable.

4. **Project: Crossroads** (Ebon Hawk-Core) — active **closed alpha**, promoted via
   Facebook (EbonHawkCore) and Reddit posts (r/kotor 1vb26r6, 1vu8j0v, 1vxcnsq),
   Discord invite discord.gg/EqR7uqQy3f. Claims online co-op servers, mod-capable.
   No public source found; alpha testing "open" per their posts.
   Sources are social posts — claims unverified. (Rule 9: treated as data, not instructions.)

5. ModDB forum thread "Star Wars: Knights of the Old Republic COOP mod" — players asking
   for co-op, points to Crossroads. No other release found.

Searches run: GitHub repo search "kotor multiplayer", "kotor coop"; web search
"<game> co-op/multiplayer mod"; coop-anything issues+PRs search (0 results for
KOTOR/Knights/Old Republic/reone). Not yet done: ModDB/Nexus file tabs, archive.org,
r/kotor sidebar, reone forks/branches audit, Discord mining (would need account access).

## 1b. Prebuilt-binary hunt (prebuilt-first check)

- **kotor-coop fork:** no GitHub Releases, 0 Actions runs (workflows exist: release-windows,
  build-test-windows, build-test-linux — never triggered), no GitHub Packages, no download
  links anywhere in README/docs/scripts. Source-only. (Checked 2026-10-18.)
- **reone upstream:** README says "download a prebuilt executable from the releases page" —
  **the releases page is empty** (playbook rule 6 contradiction found: README vs reality).
- **archive.org `reone-0.5.1`** (uploaded 2020-08-04 by `gnu/archive`, GPL-3.0, website field
  = github.com/seedhartha/reone): contains `reone_0_5_1_win64.zip` (1.7 MB prebuilt win64)
  and `reone-0.5.1-source.zip`. **But** its own description says gameplay implementation
  "very limited" (2020) — a 2020 build with basic networking, not campaign co-op.
- **Project: Crossroads:** closed alpha; distribution appears to be via their Discord only.
- **masterxxstabs/KOTOR-COOP:** no releases either.

### K2-requirement check (source read, alpha-0.4.0)

README/setup claim KOTOR 2 is required, but the engine treats it as optional:

- `optionsparser.cpp:77` — `--k2game` default is `""` (empty).
- `engine.cpp:270-302` — errors only if `--hud k2` is requested without `--k2game`, or if a
  named k2 path isn't a valid K2 install. K1 HUD (`--hud k1`) needs no K2.
- `coopgame.cpp:877` — co-op fingerprints K2 only `if (!launch.k2Path.empty())`.
- BUT `tools/package/files/scripts/setup.ps1:37` hard-prompts for the K2 folder, and
  `launcher.ps1:64` defaults `hud=k2` when unset (would throw at startup without K2).
- **Derived:** K1-only co-op likely runs if `hud=k1` and `k2game` unset; setup scripts
  would need bypassing/config editing. Unverified — needs a build to test.

## 2. Derived / guesses (labelled)

- Derived: kotor-coop is currently the only *open-source, campaign-co-op, recent* route
  that fits "online co-op with friends" without building netcode from scratch.
- Guess: alpha-0.4.0 likely builds via `tools/package/make_tester_package.ps1`; no CI
  artifacts exist, so a source build (VS2022 + vcpkg) is required.
- Guess: binkw32_real.dll suggests an earlier third-party fix (e.g. bink proxy mod);
  must identify before copying install to a test folder, or the test copy inherits it.

## 3. Unknowns

- Whether kotor-coop actually plays the campaign co-op end-to-end (needs bot/human test).
- What syncs (players/quests/enemies/world/items) and how well; networking model details.
- Whether upstream reone can already play KOTOR 1 to completion (fork inherits limits).
- Crossroads: code, license, sync scope — all unknown without their Discord.
- Whether the test install is modded (Override/ contents not yet inventoried).

## 4. Blockers and decisions

- KOTOR 2 (app 208580) not installed — required by kotor-coop per README, but see 1b:
  the engine treats K2 as optional and `--hud k1` runs without it (now verified, see 5).
- Build toolchain installed and the build is complete (see 5).
- GitHub account actions (claim-a-game issue, fork, draft PR) opened in a later session.

## 5. Build + first run (severity: verified 2026-10-19)

Built `McChaffeeFAF/kotor-coop` alpha-0.4.0 from source on the test machine.

Toolchain:

- Visual Studio Build Tools 18 (2026), MSVC 14.51. Generator: `-G "Visual Studio 18 2026"`.
- vcpkg at `<vcpkg dir>` (had to move out of a spaced path: ffmpeg refuses to build with
  spaces in the path). All 69 dependencies installed, exit 0 (~44 min).
- CMake found GameNetworkingSockets (the co-op transport).

Build result (Release, `--parallel 8`, exit 0, zero errors):

- `build/bin/engine.exe` (8.77 MB) — the game engine.
- `launcher.exe` (0.35 MB) — upstream reone's graphics/audio settings GUI. **NOT the
  co-op launcher** (it calls bare `engine`, no host/join flags); see §8. The co-op
  launcher is `tools/package/files/scripts/launcher.ps1` via `Play KotOR Co-op.cmd`.
- `texgen.exe`, `toolkit.exe`, `dataminer.exe`, `gff2json.exe`, `nsscomp`.
- Runtime DLLs staged beside them (avcodec, GameNetworkingSockets, libcrypto, protobuf…).

Unit tests: `build/bin/tests.exe` → **3650 tests from 526 suites, 3639 passed, 0 failed,
11 skipped** (skips are K2-dependent or opt-in). Exit 0.

### First real run — K1-only, no KOTOR 2 (verified)

Run against the isolated copy of the install, `--hud k1`, no `--k2game`:

```
engine.exe --game <copy> --hud k1 --headless 1 --control-port <port>
```

Two harness facts learned (both cost time):

- **Console output does NOT go to stdout.** The console keeps an in-memory text buffer
  rendered on screen (`apps/engine/console.cpp`). Redirecting stdout captures nothing.
- **The automation interface is the TCP control port** (`--control-port N`, 127.0.0.1,
  newline-delimited commands, one JSON reply line each:
  `{"id":n,"cmd":"...","ok":true,"output":[...],"frame":n,"time":s,"tick":n}`). This is
  the path a bot driver should use. The `--commands-file` path exists too, but has no
  output sink and no implicit exit — add `quit` as the last line or the engine runs forever.
- **`Start-Process -ArgumentList` splits paths on spaces.** Quote any arg containing a
  space, or the engine fails with `directory_iterator: cannot find the path "…\Default"`.

What the run proved (real K1 data, not a mock):

- Module `end_m01aa`, area `m01aa` loaded — the **Endar Spire** opening level.
- Player creature spawned: name rolled "Galon Baillen", pos (15.4, 20.1, -1.3), HP 6/6,
  level 1, `faction:0`, `commandable:true`.
- **Trask** present (id 7), HP 36/36, level 3, clothes + blaster pistol equipped,
  `dialog:end_trask01` — the tutorial companion.
- Real level objects enumerated: `end_locker01` (Footlocker, 6 items), `end_door01`
  (dialog `end_door01`, keyRequired), `end_trig02`, waypoints, chairs.
- Sim ticking at 30 Hz; `tickstatus` → `tick=7 rate=30 gate=0 … rng.gameplay.seed=1792471790
  rng.violations=0`. The deterministic sim reports **0 violations**.
- `quit` over the control port exits cleanly.

Conclusion: the fork plays KOTOR 1 with no KOTOR 2 install, headless, and is drivable by a
bot over the control channel.

## 6. Two-client co-op QA (severity: verified 2026-10-20)

Wrote `test/harness.ps1`: starts **two real engine clients** on one PC (a host
and a guest, both headless), connects them over localhost UDP through GameNetworkingSockets,
drives both through their TCP control ports, and asserts the shared state. It prints one
`[PASS]/[FAIL]` per gate and ends with `RESULT: PASS|FAIL`.

Result: **23/23 gates PASS, three consecutive clean runs.** `powershell -File test/harness.ps1`.

What the suite proves (all observed, not inferred):

- Host auto-hosts (`--host 27125`), guest `coopjoin 127.0.0.1:27125` → `state:"joined"`.
- Host sees `players:2`; **0 dropped / 0 out-of-order packets** on the client route.
- **Fingerprints: `k1:"key=ebe0162bc593f19e …"`, `k2:"absent"`** — the session runs with
  KOTOR 2 genuinely not installed, which is the whole point of the K1-only route.
- Story Mode auto-assigns the guest a companion: `slot.status:"assigned"`,
  `slot.tag:"end_trask"`, `slot.id:330`.
- Guest loads the host's module: `module:"end_m01aa"`, `loaded:true`, `appliedTick:302`,
  **`replicas:103` world objects replicated and bound** (`bound:103`, `spawned:1`).
- Guest-driven movement: `coopmove` → host `accepted:true`; the companion moves
  (15.42,20.12) → (17.46,20.15) and the guest's own copy tracks the host exactly
  (`pos == localPos`, diff 0.000). Host is authoritative; the guest sends intent only.
- `quit` on both seats exits cleanly.

### Three harness facts that cost the most time (all now verified)

1. **`newgame` starts the intro movie, and a movie HOLDS the simulation.** While a movie is
   on screen the engine returns early from its frame update (`src/libs/game/game.cpp`, the
   `if (_movie) … return;` path), so `_coop->afterTick()` never runs. The host then never
   detects its module (`generation` stays 0), never sends `ModuleInfo`, and the guest never
   loads anything. A player's click clears it; over the harness, send **`skipmovie`**.
2. **The opening conversation makes the host answer `busy` to every guest command.**
   `coopcommand.cpp` rejects with `CommandReject::Busy` when
   `!acceptsInput(game.currentScreen())` — during a conversation. This is correct
   authoritative design (the host leads conversations). Clear it with the built-in
   auto-skip: `autoskipenable 1` + `autoskipentries 1 …` + `autoskipreplies 1 …`.
3. **The host reports `in_game` briefly *before* the intro starts**, so a single
   screen check returns too early. Require the state to hold across several checks with the
   tick count advancing before calling it "in normal gameplay".

Also: the client's assigned companion is `slot.id` in `netstatus` (a host id), not the
host-side `controlled` map. And a `weak_ptr` to the host module plus an `==` compare means
"the module changed" is only seen once per visit — fine in practice, but worth knowing when
reading the replication code.

This reaches **QA level: automated** for the two-client story-mode path. Open items: store
and minigame flows are not yet covered by gates.

## 7. Extended co-op QA: gameplay gates (severity: verified 2026-10-20)

Extended `test/harness.ps1` from 23 to **55 gates**, adding six gameplay flows on top of
the join/movement/shutdown gates. Result: **55/55 PASS, two consecutive clean runs**
(`coop2-suite7.log`, `coop2-suite8.log`).

New gates (each asserts both seats, not one):

- **G9 dialogue replication.** The host is in the opening Trask conversation
  (`end_trask01`); the client shows a replica of the same scene — `conversationReplica` =
  the same dialog, same entry index, same entry text and speaker, and the full reply/camera
  shot (`entry:51`, camera eye/target). A guest `coopmove` during the dialogue makes the host
  answer **`busy`** (`rejects:{"busy":1}`) — Story Mode hosts lead conversations, by design.
- **G10 shared party wallet.** Host `givegold 1234` → the client's shared inventory shows
  `credits:1234`, equal to the host's `partyGold`.
- **G11 containers.** The guest clicks the footlocker `end_locker01` (id 85, 6 items):
  the companion walks there, the host opens the client's own container screen
  (`viewing:85`, 6 items with tags), the guest `cooptake all`, and the loot lands in the
  shared inventory (`containerItems:0`, `inventory.items:6`). Host `commandsAccepted`
  rises with **no new rejects**.
- **G12 level-up replication.** A companion may only level up to the host's own level, so
  the gate first raises the host player (xp + `levelup` to 13), then gives the guest's
  companion enough xp to be `isLevelUpPending`. The client sees `sheet.levelUp:true`; the
  guest `cooplevelup` through the same screens its HUD button opens, and the host applies
  it: **level 3 → 4**, no new rejects.
- **G13 combat.** A spawned creature from the `crsghost10` template is placed on the
  guest's companion, selected, and made hostile (`setfaction 1` → `hostile:true`,
  dist 3.4). The client replicates it, `coopattack` is accepted, and the host's
  `dumpstate.combat` gains a round with **`attacker == companion, target == creature`**;
  the companion reports `inCombat:true` with that `attackTarget`. (HP loss is dice- and
  weapon-dependent, so the gate asserts the combat *round*, which is deterministic.)
- **G14 area transition.** Host `warp end_m01ab` → the client follows: `module` goes
  `end_m01aa` → `end_m01ab`, `generation` 1 → 2, `loaded:true`, **34 replicas rebuilt and
  bound** for the new deck; both seats agree on the module name.

### Two facts the new gates depend on (verified)

1. **A companion's level-up is gated by the host's own level.** `hostSendSheets`
   (`coopinventory.cpp`) sets `allowed = companionLevel < hostLevel`, and
   `validateCoopLevelUp` (`coopprogress.cpp`) rejects with `LevelUpUnavailable` when
   `level + 1 > hostLevel`. A fresh game has the player at level 1 and spawned companions
   at level 3, so **no guest level-up is possible until the host player is levelled**. Any
   level-up test must raise the host first.
2. **The host's reject counters are cumulative.** G9 deliberately provokes one `busy`, so
   later gates must judge acceptance by the *delta* in `commandsAccepted` / per-reason
   `rejects`, not by "rejects is empty". Asserting against the absolute map fails with a
   stale `busy:1` from an earlier gate.

### Reusable console commands these gates use

`givegold`, `givexp`, `levelup`, `spawncreature`, `selectobjectbyid`, `setposition`,
`setfaction`, `warp` (host side); `coopuse`, `cooptake`, `coopclose`, `cooplevelup`,
`coopattack` (guest side). The Endar Spire's footlocker tag is `end_locker01` and the
stock hostile creature template is `crsghost10` (faction must be set to 1 to fight it).

## 7b. Extended co-op QA, round 2: pause, doors, exits (severity: verified 2026-10-20)

Grew the suite from 55 to **77 gates**. Result: **77/77 PASS, two consecutive clean runs**
(`coop2-suite9.log`, `coop2-suite10.log`). Three new flows, each asserting both seats:

- **G16 pause propagation.** Anyone may pause the shared world. The guest sends
  `cooppause 1`; the host owns the pause and applies it to both seats, naming the asker:
  host and client both report `paused:true`, `pausedBy:"GuestPlayer"`. `cooppause 0`
  resumes both (`paused:false` both sides). The client's pause is a *request*
  (`CommandType::Pause`) the host carries out, not a local pause.
- **G17 the guest opens a door.** Doors are replicated, so the client has a copy of the
  host's door. `coopdoor <host id>` walks the companion to the door and sends the open;
  the host opens the real door and the client's replica shows `open:true`. **The companion
  must be in reach of the door**, so the gate walks it onto the door first (as a player
  walking up and clicking would) and waits until it is within 2.5 units. Test door:
  `droidshop` in `tar_m02ab` (unlocked). Host `commandsAccepted` rises.
- **G18 area-exit ready check.** A guest companion that walks into a **module-linked exit
  trigger** asks the party to leave. The host starts a ready check naming the destination
  and the initiator (`destination:"tar_m03ab"`, `initiator:"GuestPlayer"`, `players:2`);
  both seats answer with `coopready 1`, and **both seats move to `tar_m03ab`** (client
  `generation` 3 → 5 across the two warps). Test trigger: `tar03_npcext` in `tar_m03aa`.

### Two flow facts these gates depend on (verified)

1. **A guest action needs the companion in reach.** `coopdoor` is *accepted* (bound to the
   guest's actor, `seq` assigned) even when the companion is 50 units away, but the host
   never opens the door. The client's click path walks the actor there first; the harness
   must do the same (`coopmove` onto the object, then act). Accepted ≠ carried out — the
   gate asserts the world change on the host, not the acceptance.
2. **Only module-linked triggers start a ready check.** `hostCheckClientExits`
   (`coopflows.cpp`) skips triggers whose `linkedToModule()` is empty or that are inactive,
   so most Endar Spire triggers never fire one. `tar03_npcext` does. A scripted
   conversation can also pull the host out of gameplay mid-walk (the host then answers
   `busy`), so the gate re-asserts gameplay and retries the walk a few times.

### Flows that are NOT reachable from the control port (recorded, not gated)

- **Store.** `coopbuy`/`coopsell`/`coopoffer` only act while a store screen is open, and a
  store screen opens only when the game's own script calls `OpenStore` (routine 378)
  during a conversation (`coopcommand.cpp`, `coopfree.cpp`). There is no console fixture to
  conjure a store, and conversations cannot be started from the console. Not gated.
- **Workbench / lab.** Same shape: the guest bench screen needs a scripted
  `ShowUpgradeScreen`. `coopbench state` and `coopmg` answer cleanly with no screen open
  (`{"screen":false,"open":false}` / `{"active":false}`), so only the "nothing open"
  contract is observable, which is not worth a gate.
- **Minigame (swoop/turret).** `startMiniGame` requires the area's `MINIGAME` field
  (`hasMinigame()`); the swoop tracks are set up by **scripts** at race time, not by static
  area data. `showgallerymode swoop` fails in every module probed (`end_m01aa`,
  `tar_m02aa/ab/03aa`, `ebo_m12aa/40aa`, `danm14aa`, `kas_m22aa`). The co-op minigame
  logic is unit-tested directly (`test/game/coopminigame.cpp` calls `game.begin(...)`), but
  that seam is not on the console path. Not gated.
- **Free Mode / chargen.** `coopnewguest` needs `clientNeedsGuest()` (a Free Mode client
  with no character), a different session mode from Story Mode. Out of scope for this
  Story-Mode suite.

### Still-open coverage (honest gaps)

Class/chargen, store, workbench/lab, minigame and Free Mode remain ungated — see above.
The reachable next additions would need either a Free Mode session or a module with a
scripted merchant/bench the harness can walk a guest into.

## 8. Packaging: build + verify the shipped package (severity: verified 2026-10-20)

Phase 7 is already implemented in the fork under `tools/package/`; the work here was to
**build it, test it, and fix what broke**, not to write a launcher. Two real bugs found,
both fixed, then a full host->join run from the package.

### What packaging already is (source read)

| Piece | Job |
|---|---|
| `tools/package/make_tester_package.ps1` | Portable zip: `engine.exe`, the runtime DLLs, app-local VC++ runtime, launch scripts, `texgen.exe`; refuses to ship if a game file sneaks in |
| `tools/package/files/scripts/launcher.ps1` | WinForms one-click launcher: auto-detects installs, writes `reone.cfg`, Host / Join / Play solo |
| `Host.cmd` / `Join.cmd` / `Play KotOR Co-op.cmd` / `Setup.cmd` | Command wrappers |
| `tools/package/linux/` | The same in bash, with `test_launcher.sh` (32 synthetic-tree checks) |
| `tools/coopserver/` | Optional join-code + TURN relay service (Python/Docker) |

**`launcher.exe` is NOT the co-op launcher.** It is upstream reone's graphics/audio
settings GUI (calls bare `engine`, no host/join flags). The co-op launcher is
`launcher.ps1`, reached through `Play KotOR Co-op.cmd`. Easy to confuse; do not ship or
document `launcher.exe` as the co-op front end.

### The engine reads `reone.cfg` from its working directory (verified)

`--game`, `--hud`, `--coop-name` and so on can live in `reone.cfg` (written by the
launcher) with the rest on the command line; command-line flags override the file.
**`reone.cfg` handles paths with spaces correctly** (Boost's config parser keeps the whole
line value) - verified with a real install reached through a spaced directory. Only
arguments passed on the *command line* need quoting, which is `Start-Process`'s problem,
not the engine's.

### Bug 1 (fixed): the package shipped the wrong VC++ runtime

`make_tester_package.ps1` searched the Visual Studio installs for a `Microsoft.VC*.CRT`
redist folder and sorted by **name**, descending. `2019\BuildTools` sorts after
`18\BuildTools`, so it copied **VC142 (14.29)** next to an engine built with **VC145
(14.51)**. The app-local `msvcp140.dll` then shadowed the newer system runtime and the
engine died on startup with `0xC0000005` (access violation), no log line, no output.

- Evidence: with the 9 bundled CRT DLLs moved aside, the same `engine.exe` ran fine
  (`--help` exit 0). With them present, exit `-1073741819`.
- Fix: read the toolset from the engine's own imports (`boost_program_options-vc145-...`
  -> `VC145`), look for that CRT first, fall back to the newest, and warn on a mismatch.
  Also re-copy the redists over anything of the same name that came from `build\bin`.
- After the fix the package's CRT is all `14.51.36247.0` and the engine runs.

### Bug 2 (fixed): the launcher refused to start without KOTOR 2

The launcher required **both** games (`Update-Rows` gated Host/Join/solo on K1 *and* K2),
and `hud` defaulted to `k2`. A K1-only player got "Both games are needed" and **all three
buttons disabled** - even Play solo - though K1-only is fully verified at the engine
level (`--hud k1`, no `--k2game`, `k2:"absent"`).

- Fix (Windows `launcher.ps1` + `setup.ps1`, Linux `setup.sh`): KotOR II is optional.
  Readiness depends on K1 only; `hud` follows what is installed; the HUD picker locks to
  the KotOR look with no K2; `Setup` skips the K2 prompt with Enter; the READMEs say K2 is
  optional. Added a `cfg_remove` helper for the Linux side and a K1-only case to
  `test_launcher.sh`.
- Before/after self-test (`KOTOR_LAUNCHER_SELFTEST=1`), K2 absent:
  `host enabled: False` + "Both games are needed" -> `host enabled: True` + the
  port-forward/join hint, with the K2 row reading "Optional".

### Verified end to end (severity: verified 2026-10-20)

1. `test_launcher.sh`: **32/32** synthetic-tree checks pass (was 31 before the new case;
   an early edit broke the join-by-address check by reassigning `$root` - fixed by
   restoring test 8's package for test 9).
2. Package built: 24.8 MB zip, no game files (guard passed), VC145 runtime, all scripts.
3. **Fresh-extracted zip** runs: control port up, "Co-op: host session, mode story",
   K1 keys present, no K2 needed.
4. **Host->join from the package: 9/9 checks** (`package-e2e.log`). The packaged
   `engine.exe` hosted, a second packaged `engine.exe` joined by `127.0.0.1:<port>`,
   both landed in `end_m01aa`, replication clean (`unmatched:0`, `violations:0`,
   `replicas:102 bound:102`), traffic both ways, `k2:"absent"`.

Harness note: the control port takes **plain-text, newline-delimited commands** and
answers each with one JSON line. Sending a JSON request object gets `Unrecognized
command` - the reply is JSON, the request is not.

Local run: `powershell -File pkgtest.ps1` (kept out of the repo; points at the isolated
install and the built package).

## 9. Publishing the packaging fixes (severity: verified 2026-10-21)

The two packaging fixes are **published**: branch `fix/packaging` pushed to our fork, and
opened as a pull request to the upstream repo,
**[McChaffeeFAF/kotor-coop#1](https://github.com/McChaffeeFAF/kotor-coop/pull/1)**.

- Commits `4ec25f9` (VC++ runtime) and `5d45c38` (KotOR II optional); 8 files, +149/-48.
- Rebased onto upstream's current `main`, **alpha-0.4.4** (`c56a05e`): upstream had moved
  four releases since the fixes were written against alpha-0.4.0.
- Rebase conflicts: upstream had already changed the `hud` default from `k2` to `k1`
  (`launcher.ps1`, `setup.sh`) and added Steam Deck handling and richer install detection
  (`linux/`). Kept our conditional-HUD logic on top of theirs and merged their detection
  wording with our KotOR-II-optional wording.
- **alpha-0.4.4 still hard-requires KotOR II**: `Update-Rows` still loops over
  `@("k1","k2")` and disables Host/Join/solo. The HUD-default change alone did not fix a
  KotOR-1-only player's problem, so this fix is still wanted.

Verified after the rebase:

- Launcher self-test with KotOR II absent: **`host enabled: True`** with the join hint and
  the KotOR II row marked optional (was "Both games are needed" with everything disabled).
- `linux/test_launcher.sh`: **all launcher checks pass** (run via Git Bash; WSL has no
  distro on this machine, so it could not be run through WSL).
- The C++ engine is untouched by this branch, so the 77-gate suite result still stands and
  the built package is unchanged.

The suite is published with the write-up at `games/kotor/test/harness.ps1` in the hub
repo, so the 77 gates are reproducible rather than an unpublished claim. It is
parameterised (`-GamePath`, `-BinDir`, ports) and takes no private paths; SHA-256 of the
published copy is `EB6DFAEB9CAE68C36149F62E036485EB93D3C0DDD2F64E8DB6E0E16E5B0D6D7E`.
Re-run on 2026-10-21 from the published copy: **77 passed, 0 failed, RESULT: PASS**.

Note: pass `-GamePath` and `-BinDir` as **absolute** paths. The harness hands them to
`Start-Process`, and a relative path resolved against the harness's own working directory
fails with the control ports never opening (`RESULT: FAIL`), which reads as a real
failure but is only a bad path.

### Two traps hit while publishing (worth knowing for any repo)

**`>` in PowerShell 5.1 writes UTF-16.** Piping a command into a file
(`git cat-file blob <sha> > f.ps1`) re-encodes the bytes as UTF-16LE with a BOM. The file
looks doubled in size and fails any hash check. It cost real time here: it made a
correctly-stored blob look like a UTF-16 blob in the git object database, when the index
and the committed object were both fine all along. To capture a command's real bytes use
`cmd /c "tool > file"`, or read and write with Python. Never trust `>` when a hash is
involved.

**The source file was double-encoded, not just displayed wrongly.** This note file had 37
em dashes, arrows, ellipses, the trademark sign and the section sign written as mojibake
from the moment it was created - a UTF-8 file decoded as cp1252 and re-encoded once. The
tell is byte-level, not visual: an em dash should be `E2 80 94`, and the mojibake form is
`C3 A2 E2 82 AC E2 80 9D`; check that the whole file decodes as strict UTF-8 with no
`U+FFFD`. Git itself is happy to store this; only a reader notices. The harness was never
affected (it is ASCII plus a few box-drawing comments).

Also worth recording: pushing to the hub needs the **fork** remote
(`git push fork game/kotor`), not `origin` - `origin` is `cranebeaker/coop-anything`, where
this account has read-only access, so a push there fails with HTTP 403.
