# IRON NEST: Heavy Turret Simulator (2026)

| | |
|---|---|
| Engine | Unity 6 (6000.3.21f1), IL2CPP x64, Windows |
| Versions tested | Steam build 25664742; BepInEx 6.0.0-be.785 and be.788 (IL2CPP) |
| Projects | [Nestmates](https://github.com/skythebro/Nestmates) (MIT, [Nexus](https://www.nexusmods.com/ironnest/mods/90)); [Iron Nest Co-Op Mod](https://github.com/Jager2251/Iron-Nest-Co-Op-Mod); [IronNestCoop](https://github.com/Xeltor/IronNestCoop). All three are in `data/directory.yaml` |
| QA level | automated |
| Last updated | 2026-10-30 (1.0.2) |

## Where things stand

Two to five players can crew one turret in the host's missions over Steam lobbies with Nestmates (1.0.2). The
systems the bot suite covers (see QA) are shared, and players can join or rejoin mid-mission. A full campaign
mission was played to the end by two real players, and a bot soak test ran five campaign missions with two guests
(three players). It isn't a verified campaign playthrough: the mod isn't hardened against a player who has already
joined and sends bad data, the `events` scenario sometimes fails, and map area effects aren't synced or tested
(see Known issues).

## Existing work

- **Nestmates** (this write-up): <https://github.com/skythebro/Nestmates>, MIT, 1.0.0 released 2026-10-29 and
  1.0.1 on 2026-10-30. Download on [Nexus](https://www.nexusmods.com/ironnest/mods/90). Built with this playbook.
- **Iron Nest Co-Op Mod** by Jager2251: [Nexus 20](https://www.nexusmods.com/ironnest/mods/20), issue tracker at
  <https://github.com/Jager2251/Iron-Nest-Co-Op-Mod> (no source there). It uses a Steam lobby code and takes up to 5
  players. Its README lists missions, aim, loading and firing, doors and hatches, cards, map drawings, the supply
  console and avatars as shared. Its 2.5.0 changelog adds units (damage, behaviours, movement), map area effects, a
  shared random seed per mission, end-screen statistics and the mission clock. Music and calculator inputs stay local on purpose, and joining a running mission is
  unsupported. CC BY-NC-ND 4.0, binaries only, latest 2.5.0 (2026-10-28, about 16k unique downloads on Nexus). Players
  file bug reports in its issues.
- **IronNestCoop** by Xeltor: <https://github.com/Xeltor/IronNestCoop>. A host-authoritative plugin over a Steam
  star topology, built with AI agents (stated in its README). The README lists mission flow, avatars, map,
  calculator, requisition, guns, turret, operations stations and the cat as synchronized, with ownership leases and
  late-join snapshots. It targets 5 players. Source visible but All Rights Reserved, latest v1.1.0 (2026-08-14).
- **Open Nest Co-op** by 1499501762: <https://github.com/1499501762/OPEN_NEST_CO-OP>,
  [Nexus 36](https://www.nexusmods.com/ironnest/mods/36). It runs on BepInEx 6 or MelonLoader, uses Steam P2P
  lobbies with a password, and added LAN in its latest alpha. Its README marks turret, firing, reload, avatars,
  record player, objectives, impacts, counter-battery, engine and lights as done ("main paths"), plus mid-join
  snapshots and a custom mission framework. AGPL-3.0, latest v0.2.1-Alpha-2 (2026-09-12).
- **VR. Co-op. PvP.** (Iron Nest VR & Co-Op) by Blah64: [Nexus 17](https://www.nexusmods.com/ironnest/mods/17). A VR
  mod (OpenXR) with flatscreen crossplay: up to 4 players in one nest for co-op, or two nests for PvP, with a lobby
  browser on F7. Its page says testing never completed a full co-op mission and that more than two players is
  untested. Version 2.1, made for the game's demo, last updated 2026-07-02 (before the full release). Source isn't
  linked.
- Nestmates is its own clean-room code. Jager2251's and Xeltor's licenses don't allow reuse, so those two were used
  only to learn *what* players expect to be shared, never *how*. Open Nest Co-op and the VR mod weren't consulted.
  None of their code was read or decompiled.

## How the game works (for co-op)

- **No netcode at all.** It is a single-player Unity 6 IL2CPP game. BepInEx 6 bleeding edge (be.785) loads fine,
  and Il2CppInterop exposes private members publicly on the generated proxies, which makes hooking easy.
- **The turret is a room full of physical controls.** Dials, sliders, levers and buttons all go through one
  `DynamicCursorManager`, which has hover, press and drag hooks (`BeginDrag`, `_currentHover`, `_clickPressed`).
  Dials read the local pointer through `DialInteractable.GetPointerWorldPointOnDialPlane`, so a remote player's
  drag must freeze or override that, or the local pointer will steal the control.
- **The mission is a graph** plus timers, enemies, teleprinter orders and a shell simulation. Impacts restore
  through `ImpactVisualCorrections.RestoreMissionState`, which is handy for late joiners.
- **Save:** `MissionSaveManager`. The Esc menu's Save & Exit only exits after the save returns true, so a guest
  must be handled there or the button looks broken.
- **Identical-code folding (ICF)** is common in IL2CPP. Many tiny methods share one native body, so a Harmony
  detour on one fires for all of them. Nestmates ships `tools/foldcheck.py`, which reads the Cpp2IL dump and fails
  the build check if a patch target is folded or too short to detour.

## What we changed

The full list is in the Nestmates README. The approach:

- **Host-authoritative.** Guests' copies of the mission logic are switched off with Harmony prefixes, and they
  mirror the host's results.
- **Guest actions are forwarded.** A guest's press or drag is replayed on the host through the game's own
  methods, then echoed to everyone else.
- **One module per game system.** Each has a join snapshot for late join and rejoin.

Fixes that might help other Unity IL2CPP games:

- **Remote drags vs. the local pointer.** A dial someone else is holding followed the guest's hover. Fix: a
  postfix on the pointer-on-plane method returns the frozen remote value, presses on a held control are refused,
  and `BeginDrag` refuses controls being replayed.
- **Pause.** The game pauses on Esc. In a session it only pauses when every player has the menu open.
- **Late join of visual state.** Impact parts, correction arrows and off-map edge markers had to be re-created
  from prefabs on the guest, not just from the save data.

## QA

- `python -I tools/coopqa.py run <scenario>` starts two sandboxed copies of the game (never the Steam install)
  connected over loopback. In-game bots play both sides and compare the two worlds. The harness restores
  PlayerPrefs afterwards.
- Fixes come with a **mutant twin**: a launch flag (`-coop-mutant=<name>`) switches the fix off, and the scenario
  must then fail. That proves the test checks the fix. The notes below say which 1.0.1 twins were run.
- Unit tests: `dotnet run --project tests/IronNestCoop.Net.Tests` gives **PASS 29/29** (protocol and transports).
  `tools/foldcheck.py` gives PASS (0 unsafe patch targets).

**Full bot suite, 2026-10-30, Nestmates 1.0.1** (zip SHA-256 `a9a03e5e…`), Steam build 25664742, two instances
over loopback. Result: **37 of 38 scenarios pass.** Steps are host / guest bot checks.

| Scenario | Result | Host / guest steps |
|---|---|---|
| connect | PASS | 12 steps, 0 fail / 9 steps, 0 fail |
| mission | PASS | 12 steps, 0 fail / 15 steps, 0 fail |
| entities | PASS | 15 steps, 0 fail / 14 steps, 0 fail |
| world | PASS | 11 steps, 0 fail / 11 steps, 0 fail |
| avatars | PASS | 25 steps, 0 fail / 16 steps, 0 fail |
| controls | PASS | 33 steps, 0 fail / 34 steps, 0 fail |
| gunnery | PASS | 74 steps, 0 fail / 61 steps, 0 fail |
| engine | PASS | 15 steps, 0 fail / 17 steps, 0 fail |
| music | PASS | 29 steps, 0 fail / 40 steps, 0 fail |
| cats | PASS | 27 steps, 0 fail / 17 steps, 0 fail |
| items | PASS | 25 steps, 0 fail / 24 steps, 0 fail |
| markers | PASS | 18 steps, 0 fail / 14 steps, 0 fail |
| unlocks | PASS | 12 steps, 0 fail / 7 steps, 0 fail |
| pause | PASS | 16 steps, 0 fail / 13 steps, 0 fail |
| latejoin | PASS | 18 steps, 0 fail / 13 steps, 0 fail |
| rejoin | PASS | 12 steps, 0 fail / 9 steps, 0 fail |
| requisition | PASS | 19 steps, 0 fail / 15 steps, 0 fail |
| toggles | PASS | 16 steps, 0 fail / 15 steps, 0 fail |
| espresso | PASS | 22 steps, 0 fail / 17 steps, 0 fail |
| cups | PASS | 14 steps, 0 fail / 15 steps, 0 fail |
| maprejoin | PASS | 48 steps, 0 fail / 19 steps, 0 fail |
| emotes | PASS | 53 steps, 0 fail / 30 steps, 0 fail |
| saveexit | PASS | 11 steps, 0 fail / 8 steps, 0 fail |
| fixes101 | PASS | 14 steps, 0 fail / 11 steps, 0 fail |
| language | PASS | 13 steps, 0 fail / 13 steps, 0 fail |
| progress | PASS | 18 steps, 0 fail / 14 steps, 0 fail |
| cursor | PASS | 14 steps, 0 fail / 12 steps, 0 fail |
| pickup | PASS | 15 steps, 0 fail / 10 steps, 0 fail |
| rules | PASS | (rerun after a test fix) |
| magazine | PASS | 15 steps, 0 fail / 19 steps, 0 fail |
| paint | PASS | 10 steps, 0 fail / 8 steps, 0 fail |
| scout | PASS | 16 steps, 0 fail / 12 steps, 0 fail |
| dice | PASS | 28 steps, 0 fail / 27 steps, 0 fail |
| events | FAIL | 52 steps, 0 fail / 33 steps, 2 fail |
| gametab | PASS | 45 steps, 0 fail / 45 steps, 0 fail |
| devtools | PASS | 27 steps, 0 fail / 19 steps, 0 fail |
| sweep | PASS | 737 steps, 0 fail / 1347 steps, 0 fail |
| lamps | PASS | 22 steps, 0 fail / 51 steps, 0 fail |

Notes on this run:
- `connect`, `mission`, `entities` and `requisition` first stopped before reporting: the game instance didn't
  finish starting, in one case a crash inside the game's own settings code at boot. They passed when rerun the
  same day; the table shows the rerun.
- `rules` failed twice on one check because the bot's own dial drag lands near its target value, not on it. The
  check now compares against where the drag landed, and the scenario passes. That's a test-only change after the
  1.0.1 zip was built.
- `events` fails: after the field-report and high-command alarms, the guest shows the ceiling-hatch spotlight lit
  while the host doesn't. It's a known, intermittent visual mismatch that is still open.
- Mutant twins of the 1.0.1 fixes, run 2026-10-29: `noreveal` fails as it should. `noscrollsync` still passes, so
  the teleprinter-paper fix isn't proven by a bot test (see Known issues).

**Nestmates 1.0.2, 2026-10-30** (zip SHA-256 `1785c96e…`), Steam build 25664742. Release regression on BepInEx
be.785: connect, mission, gunnery, requisition, items, markers, events, latejoin, rejoin, maprejoin, fixes101,
mech1705, news, measurenotes, rules and gametab all PASS. New in 1.0.2:
- `campaign`: the host plays five campaign missions (Hospital False Flag, ChallangeFDC, SiegeOfCartagena,
  DeadReckoning, PhantomBattery) for two minutes each, and every guest compares its world with the host's every 15
  s (scene state, units, area effects, statistics, clock, counter-battery). With one guest: PASS. With two guests
  (`--guests=2`, three players): one PASS, then two runs where one guest's blinking engine warning lamp in
  PhantomBattery differed (every other check passed); that's still being looked at.
- `mech1705`: mission clock, counter-battery timer after added time, mission statistics and the medal records
  (kills, shells, custom values) match on the guest. Twins `nostatsync` and `nomedalsync` fail.
- `news`: the guest's newspaper article queue becomes the host's (the mission's front page). Twin `nonewsqueue` fails.
- `measurenotes`: with the new host rule on, a guest's measurement line writes its note on the host's notepad.
  Twin `nomeasurenotes` fails.

**BepInEx be.788** (6.0.0-be.788+5b766a3), 2026-10-30, Nestmates 1.0.2 unchanged: 150/150 patch classes apply;
connect, mission, controls, gunnery, items, markers, requisition, events, latejoin, rejoin, maprejoin, fixes101,
mech1705, news and measurenotes PASS. `campaign` with two guests: everything matches except the same engine-lamp check.

**Manual tests with real players over Steam:**
- 2026-10-28 and 2026-10-29: the owner's two PCs on two different Steam accounts. Hosting and joining through a
  friends-only Steam lobby, a mission played together, and Discord's "Join" button on the host's status (needs the
  game and Nestmates on the joining side).
- 2026-10-29, Nestmates 1.0.0: two players on two PCs and two Steam accounts. The campaign
  mission Iron Road was played to the end over a Steam lobby, about 65 minutes, after one restart. That session
  found seven issues. Five are fixed in 1.0.1 (map spotting, stopwatch, teleprinter paper after a pause, a map line
  drawn during an erase, deleting a moved card copy). Two are open (the guest's newspaper issue and end-of-mission
  stats).

## How to play

Download the release zip from [Nexus Mods](https://www.nexusmods.com/ironnest/mods/90) and follow its `README.txt`:

1. Install BepInEx 6.0.0-be.785 (IL2CPP x64).
2. Copy the `BepInEx` folder from the zip into `<game dir>`.
3. In game, press F7, or open Settings → Multiplayer, and host or join a Steam lobby.

Back up `%USERPROFILE%\AppData\LocalLow\Iron Nest\Iron Nest_ Heavy Turret Simulator\` first.

## Known issues

- Map area effects (the game's `WorldEffector`, made by mission steps) exist only on the host. Guests don't run unit
  behaviours, so this matters only if an effect is visible; none of the eight missions the bots played made one.
- A blinking red engine warning lamp in PhantomBattery differed on one guest in two of three two-guest soak runs.
  It's unclear yet whether that's sampling at different blink phases or a lamp that doesn't blink for guests.
- The `events` scenario sometimes fails on a ceiling-hatch spotlight after an alarm.
- The teleprinter-paper fix (host scrolls while a guest is paused) is confirmed by reasoning only.
- Things printed as finished text in the host's language can stay in that language for guests (teleprinters are
  re-rendered and fine).
- The direct-connect (TCP) option is for trusted networks only; Steam lobbies are the normal way.

## Next steps

- Hardening against a hostile peer that has already joined: check the sender's readiness on the host, rate-limit
  marker and item messages, set per-frame network budgets, and stop fragment replay. This is tracked for 1.1.
- Re-check `foldcheck` and the hook list after every game update (build ids are in the Nestmates README).
