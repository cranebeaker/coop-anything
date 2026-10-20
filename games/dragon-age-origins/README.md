# Dragon Age: Origins (2009)

| | |
|---|---|
| Engine | Eclipse Engine (BioWare's upgraded Aurora — the Neverwinter Nights engine), 32-bit x86, RTTI intact, no netcode |
| Versions tested | **1.05.13263.0**, GOG build's `DAOrigins.exe` inside a Steam install (Steam app **47810**, depot 47811, build id 21245176). GOG, EA and Steam ship different executables of the same version; DAOLib's address library maps all three. |
| Projects | [dao-coop](https://github.com/vmosenkov/dao-coop) — work in progress, directory entry below |
| QA level | automated (two real game clients, one scenario) |
| Last updated | 2026-10-20 |

## Where things stand

**Two players can now move two different avatars in the same world, and the
host's hostile creature's health and death state reach the guest** — not yet a
finished co-op game, but the core is in place and verified in the game: DAOLib
loads native plugins; a guest **joins over the network** (the host saves and
serves the slot folder, the guest writes it into its own profile and loads it,
ending up in the host's world with the same object ids); with a companion
hired before the join save, **the host controls one party member while the
guest controls another**, each game mirroring the other's creature (4,815 /
5,274 position applications, mirror distances 0.21–0.26 m); and a host-spawned
ogre's health (220 → 0, death included) is mirrored into the guest. Missing:
real combat arbitration (the damage in that test is applied by the host
plugin), AI/effects/loot sync, area transitions, a player-facing host/join UI
and an internet transport. A **0.6.0 release package** (loader + address
files + plugin + install notes + `daocoop.ini.example`) and a player-facing
`daocoop.ini` exist; the release build passed the join test, and the same test
passed with both peers on a non-loopback LAN address (sockets bound `0.0.0.0`).
A second physical PC has not been tried and there is no in-game menu yet.

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
| M3 | **Network join with no manual file copying** | `tools\join-test.ps1`: host wrote manual slot 91 and served its folder over TCP; guest received 4 files, wrote them into its own profile, loaded them (`game loaded: slot 91`) and reached the host's area; state sync continued afterwards (569 positions applied, last distance 0.15 m) |
| M4 | **Two separate avatars**: host controls the Warden, guest controls a companion | `tools\two-avatar-test.ps1`: host created and hired a mabari with the engine's `AddToParty` before the join save (party list 2); guest joined, switched control to party index 1 with `SelectPartyMemberIndexMessage`; host controlled `0x82` mirroring `0x363` (4,815 applies, 0.26 m) and guest the reverse (5,274 applies, 0.21 m), same area |
| M5 | **Hostile health/death channel** | `tools\combat-test.ps1`: host spawned an ogre with AI off (the scripts' own flag) before the join save, then damaged it after the guest connected; guest received the same hostile id and health (`220 → 0`, death state included), applying it 13 times |
| M6 | **Release package, config file and LAN path** | `dao-coop-0.6.0.zip` (loader + address files + plugin + install notes + `daocoop.ini.example`, 6.1 MB) built from the release preset; a host instance with only `DAOLIB_INSTANCE` set read its role, autoload and handoff from `daocoop.ini`; the join + sync test **passed on the release build** and again with both peers on a non-loopback LAN address (`192.168.56.1`, sockets bound `0.0.0.0`) |

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

Not a finished game yet, but two players can join and move in one world today:

1. Both players: extract the `dao-coop-0.6.0.zip` release over their game
   folder (with the game closed). It carries the DAOLib loader, the address
   files and the plugin, so nothing else is installed.
2. Copy `daocoop.ini.example` to
   `bin_ship\daolib\plugins\daocoop\daocoop.ini`.
3. Host: `role = host`, `handoff = 1`, `add_companion = 1`; load a save; wait
   a few seconds. Guest: `role = guest`, `peer = <host LAN address>`,
   `join = 1`, `select_party = 1`; wait at the main menu.
4. Allow inbound UDP 45830 and TCP 45832 on the host, UDP 45831 on the guest.

It is LAN-only for now and there is no in-game menu: the ini file is the
interface. Source and install notes: https://github.com/vmosenkov/dao-coop.

## Known issues

- Work in progress: hosting/joining is configured through `daocoop.ini`, not
  an in-game menu.
- LAN only: the LAN address path was exercised on one machine through its
  non-loopback address; two physical PCs have not been tried, and there is no
  internet transport yet.
- No combat arbitration yet: the health channel is proven, but damage is
  applied by the host plugin in the test, guest attacks do not reach the host,
  and enemy AI, effects, animations and loot are not synced.
- No quest, inventory, pause, dialogue, effect or area-transition sync yet.
- Only GOG-build executables have been tested (inside a Steam install); the
  address library supports EA/Steam wrappers but that is untested here.
- Tests run the debug build.

## Next steps

1. Real combat arbitration — guest attacks sent to the host and resolved there
   (the hostile health channel is proven; damage is still applied by the host
   plugin in the test), then effect and loot sync.
2. A second physical PC over the LAN (the address path is exercised on one
   machine), then the internet transport (Steam relay).
3. Area transitions with object-id alignment (`OnAreasBuild`); a host/join
   flow beyond the ini file; the automated suite extended to the combat and
   transition scenarios.
