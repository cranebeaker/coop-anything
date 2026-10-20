# Dragon Age: Origins (2009)

| | |
|---|---|
| Engine | Eclipse Engine (BioWare's upgraded Aurora — the Neverwinter Nights engine), 32-bit x86, RTTI intact, no netcode |
| Versions tested | **1.05.13263.0**, GOG build's `DAOrigins.exe` inside a Steam install (Steam app **47810**, depot 47811, build id 21245176). GOG, EA and Steam ship different executables of the same version; DAOLib's address library maps all three. |
| Projects | [dao-coop](https://github.com/vmosenkov/dao-coop) — work in progress, directory entry below |
| QA level | automated (two real game clients, one scenario) |
| Last updated | 2026-10-20 |

## Where things stand

**No playable co-op yet.** The platform underneath it is proven in the game:
DAOLib loads native plugins; two game processes run side by side as DAOLib
**named instances** with separate profiles; after both load the same save they
agree on every object id; and a UDP loopback channel mirrors the host's
controlled creature into the guest (830 positions applied, last measured
distance 0.00 m, in a 90 s automated two-client run). A join handshake, a
second avatar, combat sync and area transitions are still ahead.

## Existing work

- **DAOKadanMP** — `Whocares66-Dev/DAOKadanMP`: **private** repository, no
  release, no license. The only live multiplayer attempt; evidence survives
  only in public issue text of DAOLib/DAOEluvianMCP (Oct 2026): two games on
  one save, clipboard-address joining, partial object-id matching, shared
  chest contents, in-session saves loading. Known gaps from the same issues:
  kill bodybags/loot not synced (DAOLib #103), blows more than ~60 m from the
  host's camera not landing (DAOLib #88). Treat as a lead to contact, not a
  dependency.
- **2026 decompilation project** (Reddit user `Shot_Let_1344`): six developers,
  ~3 years, ~60% decompiled/~55% linked of 34,415 functions, public release
  only at ~90% (targeted 2027). It **plans** 64-bit, engine fixes, controller
  support and "potential net support and co-op campaigns"; no repo, site or
  build exists yet. Not a current artifact.
- **xoreos** (GPLv3, Aurora-engine reimplementation) has DAO as a target but
  only partial spectator gameplay and no netcode. **OpenDAO / Nikami Aurora**
  is an MIT Godot compatibility runtime; no multiplayer either.
- **No public co-op mod has ever shipped.** Steam Community, Nexus, ModDB,
  Reddit and Russian/German forum threads going back to 2009 all answer
  "single-player only"; Nucleus Co-op has no handler because there is no
  multiplayer to layer split-screen on.

## How the game works (for co-op)

- Eclipse is a single-player engine: **no netcode**, one controlled creature,
  one party inventory, one global real-time-with-pause, one dialogue/cutscene
  camera. A multiplayer mode existed and was cut during development (2021
  developer interviews).
- Game logic is **827 numbered script commands** run by an in-engine VM on one
  thread, with 314 engine message handlers. Scripts drive story, AI, triggers
  and level changes.
- **[DAOLib](https://github.com/Whocares66-Dev/DAOLib)** (GPLv3) is an
  SKSE/CommonLibSSE-style loader (`dinput8.dll`) and C++ library for
  1.05.13263.0 on GOG/EA/Steam, with a 100k+ ID
  [address library](https://github.com/Whocares66-Dev/DAOAddressLibrary). It
  gives a plugin: every script command (callable, interceptable, extensible),
  the engine's message bus, a per-frame callback on the script thread,
  object/creature state (read, and creature properties written), orders that
  are indistinguishable from player clicks, input, Flash UI, save hooks
  (co-save files, `Save`/`Load`/`List`) and the game modes/pause.
- **Saves are folders** (`Characters\<name>\Saves\<slot>` with `.das`,
  `.das.met`, `screen.dds`). A save folder copied into another profile loads
  as it is, and a load numbers the new objects from the save's own counter, so
  two games that load the same save agree on object ids — the foundation of the
  save-transfer join this project uses.
- **Named instances**: starting the game with `DAOLIB_INSTANCE=<name>` gives it
  its own process mutex and its own Documents profile
  (`%LOCALAPPDATA%\DAOLib\instances\<name>`), so two copies run side by side
  without touching the player's profile.
- **[DAOEluvianMCP](https://github.com/Whocares66-Dev/DAOEluvianMCP)** is a
  public MCP test bench plugin that serves a live game over HTTP/MCP and
  supports multiple instances — the intended automated test rig.

## What we changed

The project lives in its own repository
([dao-coop](https://github.com/vmosenkov/dao-coop), GPLv3, no game files). Its
`docs/STATUS.md` carries the evidence for each step; `docs/DESIGN.md` the
design; `docs/TESTING.md` the test plan.

| Milestone | What it proves | Test that proves it |
|---|---|---|
| M0 | DAOLib loads our plugin and the plugin reads live engine state on the game thread | `loader.log` `daocoop.dll: loaded`; `daocoop.log` `mode explore`, party bar, controlled creature id/area/position/health |
| M1 | Two game processes run side by side as named instances with separate profiles | two live `daorigins` processes; both logs show their own seeded profile |
| M1 | Both games agree on object ids after loading the same save | both logs: controlled `0x00000082`, area `0x000000ca` |
| M2 | Host-authoritative state channel inside two running games | `tools\sync-test.ps1`: host 34 drive orders; host 3,404 sent / 2,185 received; guest 2,169 sent / 3,346 received; **0 dropped**; guest applied **830** positions via the engine's `SetPosition`; last host→guest distance **0.00 m** |

## QA

- Build: `tools\build.ps1 -Preset debug` (VS 2022 C++, x86, vcpkg:
  detours/spdlog/directxtex, `x86-windows-static-md`). Deploy to an isolated
  copy only: `tools\deploy.ps1` (refuses any path but the test install).
- Run: `tools\sync-test.ps1` starts the host, waits for its world, starts the
  guest, waits for its world, runs 90 s and reads both plugin logs. It asserts:
  both sides received packets, the guest applied ≥ 10 positions, its last
  reported distance to the host ≤ 2.0 m, and the host issued drive orders.
- **Last result 2026-10-20: PASS** (numbers above). One scenario, two real game
  clients, on one PC over UDP loopback — hence QA level `automated`, not
  `verified-playthrough`.

## How to play

Not yet — there is no join flow and no second avatar. When there is, a release
will be: DAOLib release zip + the `daocoop` plugin folder, installed by both
players from https://github.com/vmosenkov/dao-coop. Both players need the same
game build (1.05.13263.0) and the same mod build; the plugin refuses anything
else.

## Known issues

- Work in progress: hosting/joining is done by the test rig, not by an in-game
  UI; the two characters are the **same creature mirrored**, not two avatars.
- No combat, quest, inventory, pause, dialogue or area-transition sync yet.
- Only GOG-build executables have been tested (inside a Steam install); the
  address library supports EA/Steam wrappers but that is untested here.
- Tests run the debug build.

## Next steps

1. M3: in-game join — host writes a save, hands the slot folder to the guest
   over the transport, guest writes it into its profile and loads it, with no
   manual copying.
2. M4: combat — host-authoritative damage, guest attack intents resolved
   host-side, shared death/loot.
3. Two-avatar model (guest controls a second party member), transitions with
   object-id alignment (`OnAreasBuild`), then LAN polish and the Steam relay.
