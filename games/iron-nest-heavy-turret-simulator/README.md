# IRON NEST: Heavy Turret Simulator (2026)

| | |
|---|---|
| Engine | Unity 6 (6000.3.21f1), IL2CPP x64, Windows |
| Versions tested | Steam build 25664742, BepInEx 6.0.0-be.785 IL2CPP |
| Projects | [Nestmates](https://github.com/skythebro/Nestmates) (MIT), see `data/directory.yaml` |
| QA level | automated |
| Last updated | 2026-10-29 |

## Where things stand

Yes, you can play the full game together. With Nestmates 1.0.0, two to five players crew one turret in the host's
mission over Steam lobbies. Everything the mission depends on is shared, players can join or rejoin mid-mission,
and a two-instance bot suite covers each synced system.

## Existing work

- **Nestmates** (this write-up): source on GitHub, MIT, 1.0.0 released 2026-10. Built with this playbook.
- Two earlier co-op mods exist. Their licenses don't allow reuse (one is CC BY-NC-ND and ships binaries only, the
  other is All Rights Reserved), so they were used only to learn *what* players expect to be shared, never *how*.
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
- There are 30 scenarios: connect, mission, controls, gunnery, latejoin, rejoin, maprejoin, emotes and more.
- Every fix has a **mutant twin**: a launch flag (`-coop-mutant=<name>`) switches the fix off, and the scenario
  must then fail. That proves the test checks the fix.
- Unit tests: `dotnet run --project tests/IronNestCoop.Net.Tests` (29/29, protocol and transports).
- Last results (2026-10-29, on 1.0.0): unit tests 29/29; latejoin, items and emotes pass. Every scenario passed
  during development; `events` has a known intermittent failure (a spotlight after an alarm). Two real players over Steam on two machines and two accounts tested by hand, including
  Discord Join.

## How to play

Download the release zip and follow its `README.txt`:

1. Install BepInEx 6.0.0-be.785 (IL2CPP x64).
2. Copy the `BepInEx` folder from the zip into `<game dir>`.
3. In game, press F7, or open Settings → Multiplayer, and host or join a Steam lobby.

Back up `%USERPROFILE%\AppData\LocalLow\Iron Nest\Iron Nest_ Heavy Turret Simulator\` first.

## Known issues

- Things printed as finished text in the host's language can stay in that language for guests (teleprinters are
  re-rendered and fine).
- The direct-connect (TCP) option is for trusted networks only; Steam lobbies are the normal way.

## Next steps

- Hardening against a hostile peer that has already joined: check the sender's readiness on the host, rate-limit
  marker and item messages, set per-frame network budgets, and stop fragment replay. This is tracked for 1.1.
- Re-check `foldcheck` and the hook list after every game update (build ids are in the Nestmates README).
