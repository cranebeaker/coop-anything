# Dragon Age: Origins (2009)

| | |
|---|---|
| Engine | Eclipse Engine (BioWare's upgraded Aurora — the Neverwinter Nights engine), 32-bit x86, RTTI intact, no netcode |
| Versions tested | **1.05.13263.0**, GOG build's `DAOrigins.exe` inside a Steam install (Steam app **47810**, depot 47811, build id 21245176). GOG, EA and Steam ship different executables of the same version; DAOLib's address library maps all three. |
| Projects | [dao-coop](https://github.com/vmosenkov/dao-coop) — work in progress, directory entry below |
| QA level | automated (two real game clients, the scripted suite) |
| Last updated | 2026-10-23 |

## Where things stand

**Two players can now join one world, control two different party members,
fight together, die and be brought back, and read a real in-game event log** —
a working co-op prototype, not a finished game. Verified in the game: DAOLib
loads native plugins; a guest **joins over the network** (the host saves and
serves the slot folder, the guest writes it into its own profile and loads it,
ending up in the host's world with the same object ids); with a companion hired
before the join save, **the host controls one party member while the guest
controls another**, each game mirroring the other's creature; the host's
hostile creature's position, health and death state are mirrored.

On top of that: **combat arbitration** (the guest's attack orders become
request packets; the host validates them against the live world, tells its own
game to attack, and sends the result back; the host repeats the order each two
seconds so the swing completes), **death and revival** (a fallen host player
keeps the world running while a party member is alive; after the fight the
fallen party members are set back to full health and control returns), **guest
rejoin after the host loads another save** (host re-saves and serves the world,
guest receives it, reloads and rejoins), and an **in-game Event Log panel**:
a plugin-owned SWF 8 screen in DAO's HUD, built from ActionScript 2 sources with
the game's own fonts, filled from the session's event log (multiplayer, combat
and system categories, filter tabs, creature names, folding by click). Missing:
effect/loot/death-animation sync, area transitions, a host/join menu, an
internet transport and a second physical PC. The copy's attack swing is mirrored
already; the rest of the animation and effects are not. The only interface is
`daocoop.ini` (written by the release's `install.ps1`).

The log and the movement were fixed the same day (2026-10-23), and the LAN path was re-verified on 0.7.3 with `independence-test -PeerIp <this machine's LAN address>` (both peers bound `0.0.0.0`, `dropped 0`, both logs shared): the session
**event log is now shared** (a new event packet carries each line to the other
game, and a single receive loop dispatches every packet kind, which also fixed a
transport buffer overflow), so both players read the same lines; and a guest
that configured no party member now **takes the first companion automatically**,
so the two players no longer control the same Warden and cannot be dragged back
to each other's position. Automated: independence-test and walk-test pass on
the 0.7.1 release build.
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
| M7 | **Combat arbitration with real damage in the host's game** | `tools\attack-test.ps1`: the guest's controlled creature attacked the host's ogre; the guest turned its attack command into a request, the host validated attacker/target (known guest, monotonic request id, both alive, same area, hostile target) and gave its own game the attack order, answering with the result; the guest's creature's attack reached the host (`attack: request ... accepted`), the ogre's health went down in the host's game and the mirrored copy followed; the guest's positions stayed in sync during the fight. `tools\demo-fight.ps1` shows the two games fighting the same ogre (host + guest attacking, mirrored) |
| M8 | **Death and revival of a player** | `tools\death-test.ps1` **PASS**: the host player is killed on purpose (test hook), the host's world keeps running while the guest/party is alive (state lines continue), the hostile is then defeated through the test hook, the revive path sets fallen party members back to their maximum health and returns control to the host's avatar (avatar above zero health afterwards). Fix found by the test: a corpse left in the hostile table no longer counts as an ongoing fight |
| M9 | **Guest rejoin after the host loads another save** | `tools\reload-test.ps1` **PASS**: the host loaded a second save mid-session; the host re-saved the transfer slot and served it again, the guest saw the world-reloaded flag, reset its join state, received and loaded the new slot (loading screen) and re-selected its party member — no restart |
| M10 | **In-game Event Log panel from the plugin's own SWF** | `tools\ui-shot.ps1` and `tools\ui-join-shot.ps1`: an ActionScript 2 panel (`ui/eventlog/frame_1/DoAction.as`) is compiled into `ui/coop_eventlog.swf` (SWF 8, the game's `$TitleFont`/`$BodyFont` imported from `gfxfontlib.swf`) with FFDec, added as a HUD screen through `dao::gui::AddScreen`, and filled from `EventLog` through `Movie::SetArray`/`SetVariable` on a revision counter; screenshots show the sample lines, a real "Another player joined the game." event, combat lines with creature names, and the ALL/COMBAT/MULTI/SYSTEM filter tabs |
| M11 | **Release package 0.7.0 with the event log screen, re-tested on the release build** | `tools\package.ps1 -Version 0.7.0` built `dao-coop-0.7.0.zip` (7.0 MB: loader, address files, plugin, the SWF screen beside it, install notes, `daocoop.ini.example`); on that release build `tools\attack-test.ps1` (guest attack request accepted and resolved by the host) and `tools\reload-test.ps1` (host loads another save, guest reloads and rejoins) both **PASS** |
| M12 | **Shared Event Log and independent players (0.7.1)** | Protocol 8's event packet shares each log line (both logs hold both join lines: `independence-test`); the receive loop is one `ReceiveAny` dispatch (the per-kind receives starved each other and the host stopped seeing the guest). A guest with no party index takes index 1 (`party: the guest will take party index 1 by default`), the mirror no longer repositions it before the switch (`applied` 0 after), it walks 2.14 m away from the standing host and the host's copy follows at 1.12 m; `walk-test` also passes on the release build |
| M13 | **One-command install and LAN re-verification (0.7.4)** | The release carries `install.ps1`: `-GameDir <game folder> -Role host|guest [-PeerIp <host address>] -Apply` extracts the release and writes `daocoop.ini` for that role (backing up an old one; dry run without `-Apply`); tested on a scratch folder with a dummy `daorigins.exe` (plan, apply, guest ini plus `.bak`). LAN path re-verified on 0.7.3: `independence-test -PeerIp 192.168.56.1` (both peers bound 0.0.0.0, `dropped 0`, both logs hold both join lines) |

## QA

- Build: `tools\build.ps1 -Preset debug` (VS 2022 C++, x86, vcpkg:
  detours/spdlog/directxtex, `x86-windows-static-md`). Deploy to an isolated
  copy only: `tools\deploy.ps1` (refuses any path but the test install).
- Run: the test scripts start the host, wait for its world, start the guest,
  wait for its world, drive a scenario for 60-180 s and read both plugin logs.
  `sync-test`, `join-test`, `two-avatar-test`, `combat-test`, `attack-test`,
  `death-test`, `reload-test`, `pause-test`, `ui-shot` and `ui-join-shot` are
  the suite; each asserts its own evidence in both logs.
- **Last result 2026-10-23: PASS** for `independence-test` (loopback and the LAN address), `walk-test`, `attack-test`, `death-test`,
  `reload-test`, `ui-shot` and `ui-join-shot`; `demo-fight` shows the shared
  fight on screen. One scenario per test, two real game clients, on one PC over
  UDP loopback or its non-loopback LAN address — hence QA level `automated`,
  not `verified-playthrough`.

## How to play

Not a finished game yet, but two players can join and move in one world today:

1. Both players: run the release's `install.ps1` (it extracts the release and writes `daocoop.ini` for that PC's role: `-GameDir "<game dir>" -Role host -Apply` or `-Role guest -PeerIp <host LAN address> -Apply`), or extract the `dao-coop-0.7.4.zip` release over their game
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
- Combat sync covers the creatures' positions, health, targets, attack orders
  and death state, not the animations and effects themselves: a swing can play
  a different phase in the two games, and the log records attack starts and
  deaths, not every hit or damage number. Enemy AI is the host's; the guest's
  copy only follows the mirrored state.
- A player's fall is handled (world keeps running, party revived after the
  fight), but there is no game-over/load screen integration for a guest whose
  own game ends, and no quest/cutscene sync.
- No inventory, quest, dialogue, effect or area-transition sync yet.
- Only GOG-build executables have been tested (inside a Steam install); the
  address library supports EA/Steam wrappers but that is untested here.
- Most tests ran on the debug build; `attack-test`, `reload-test`,
  `independence-test` (loopback and LAN), `walk-test`, `ui-menu-test` and
  `ui-shot` were also run on release builds. `combat-test` is known to fail on
  the Fort Drakon save (an inactive `CreateObject` ogre is not returned by the
  hostility query there); the fight path is covered by `attack-test`,
  `walk-test` and `demo-fight`.

## Next steps

1. Replicate the rest of the combat animations/effects (hits, deaths, spells;
   the copy's attack swing already plays) between the games, so the two screens
   look like one fight; then loot and bodybags.
2. A second physical PC over the LAN (the address path is exercised on one
   machine), then the internet transport (Steam relay).
3. Area transitions with object-id alignment (`OnAreasBuild`); a host/join
   flow beyond the ini file; the remaining UI work (scrolling history, event
   details, player names in multiplayer lines).



1. Real combat arbitration — guest attacks sent to the host and resolved there
   (the hostile health channel is proven; damage is still applied by the host
   plugin in the test), then effect and loot sync.
2. A second physical PC over the LAN (the address path is exercised on one
   machine), then the internet transport (Steam relay).
3. Area transitions with object-id alignment (`OnAreasBuild`); a host/join
   flow beyond the ini file; the automated suite extended to the combat and
   transition scenarios.








