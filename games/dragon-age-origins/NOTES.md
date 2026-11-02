# Agent notes: Dragon Age: Origins

Working notes for adding co-op to **Dragon Age: Origins – Ultimate Edition**
(Steam app 47810, 1.05.13263.0). Plan and evidence live in the game's own
repository `dao-coop`; this file is the cooperator's log for the hub, so the
next agent starts from here. Started 2026-10-20.

## Triage (phase 0)

| | |
|---|---|
| Game | Dragon Age: Origins – Ultimate Edition, 2009 |
| Store / version | Steam, app 47810, build id 21245176, game **1.05.13263.0** (GOG, EA and Steam builds differ; all three are 1.05.13263.0) |
| Engine | **Eclipse** (BioWare in-house, an upgraded Aurora — Neverwinter Nights' engine), 32-bit x86, RTTI intact |
| Existing co-op | **none public, ever**; directory entry is a gap |
| Source available | no; a closed 2026 decompilation project is ~60% done and plans net/co-op (not an artifact) |
| Modding surface | native plugin loader exists (DAOLib, SKSE-style); script VM with 827 commands; engine message bus; Flash UI |
| Path | 2 (build from zero), but **not from zero RE**: DAOLib already maps the engine. |

## What exists (phase 1, checked 2026-10-20)

- **No public DAO co-op mod.** Searched GitHub (repo/code/topics), Nexus,
  ModDB, GameBanana, Steam Community, Reddit, Russian/German/Polish forums,
  archive.org, YouTube, Nucleus Co-op's handler hub. Negative evidence and the
  search ledger are in the research notes of the working session; the summary
  below is what matters.
- **DAOLib** — https://github.com/Whocares66-Dev/DAOLib — GPLv3, SKSE/CommonLibSSE
  style loader (`dinput8.dll`) + C++ library for DAO 1.05.13263.0 on GOG/EA/Steam.
  Address library (100k+ stable IDs) at
  https://github.com/Whocares66-Dev/DAOAddressLibrary. Public releases v0.1.0
  (2026-10-10) → v0.5.0 (2026-10-18). Exposes: all 827 script commands
  callable/interceptable, engine messages, per-frame callback on the script
  thread, object/creature state, orders that look like player clicks, input
  injection, custom Scaleform screens, save hooks + co-save files, game
  modes/pause.
- **DAOKadanMP** — `Whocares66-Dev/DAOKadanMP`, **private** (GitHub 404
  unauthenticated, absent from the public repo list). The only live multiplayer
  attempt; evidence survives only in public DAOLib/DAOEluvianMCP issue text:
  two game processes on one save, clipboard-address join, host/guest object-id
  matching (partial), shared chest contents, in-session saves loading. Known
  gaps from the same issues: kill bodybags/loot not synced (DAOLib #103), blows
  >60 m from the host's camera not landing (DAOLib #88). No release, no
  license, no download — a lead to contact, not a dependency.
- **DAOEluvianMCP** — https://github.com/Whocares66-Dev/DAOEluvianMCP — GPLv3 MCP
  test bench plugin: serves a live game over MCP/HTTP, drives it, supports
  multiple instances on their own ports. Intended automated QA rig for the
  two-client tests.
- **DAO Turn Based** (Nexus 7053), **DAO Photo Cam** (7060), **DAO Elgara
  Shaders** (7061), **Extended Item Variations** (7069) — public DAOLib plugins;
  none networked. The turn-based mod's engine findings (pause, command queue,
  clock) are in DAOLib's `dev/TURN_BASED.md`.
- **2026 decompilation project** — Reddit user `Shot_Let_1344`, 6 devs, ~3
  years, 60% decompiled/55% linked of 34,415 functions, release only at ~90%
  (targeted 2027), **plans** 64-bit, controller, per-player inventories, pause
  rework, "potential net support and co-op campaigns". No repo, site or build
  yet. Treat as a future enabler; no public link to DAOLib's author found.
- **xoreos** (GPLv3 Aurora reimplementation) has DAO as a target but only
  partial spectator gameplay, no netcode. **OpenDAO / Nikami Aurora** is a
  separate MIT Godot compatibility runtime; no multiplayer.

## Engine facts that shape the design

- No netcode, no client/server split; one controlled creature at a time, one
  party inventory, global real-time-with-pause, one dialogue/cutscene camera.
- Game logic runs in an in-engine script VM on one thread; script commands must
  be called from that thread (DAOLib gives a per-frame callback on it).
- A multiplayer mode existed and was cut in development (2021 interviews).
- Two copies can run on one PC as DAOLib **named instances**
  (`DAOLIB_INSTANCE=<name>`): own mutex + own Documents profile, seeded once
  from the default profile's saves.
- A save is a folder; a guest can be sent the host's save, write it into its
  own profile and load it — both games then agree on every object id (DAOLib
  `dev/SAVES.md`, seen in play 2026-10-12 by the private mod's session: guest
  in the host's world 9–19 s later).

## Chosen approach

Two processes, host-authoritative: guest joins by **save transfer**, then state
sync (player state, orders, hostiles, effects, transitions) with host-resolved
damage. Two players share the same party (no puppet spawning needed — both
worlds hold the same objects). LAN UDP first (the two players share a local
network), Steam/Spacewar relay later. Full design and milestones in
`dao-coop/docs/DESIGN.md`.

## Session log (2026-10-20)

- Claimed the game: cranebeaker/coop-anything#79. Directory gap entry updated
  with the DAOLib / DAOAddressLibrary / DAOEluvianMCP leads, the private
  KadanMP mod, the decompilation plan and the claim issue.
- Game identity recorded: exe `bin_ship\daorigins.exe` 9,674,752 bytes,
  FileVersion 1.05.13263.0, SHA-256
  `A79621C2D2020C6F838096150E30F8E51FA263DBC9121E04E3D8C633975892F8`; the copy
  in place is the **GOG build** (no `.bind`, link stamp 0x4eb19d53) with the
  stock Steam-wrapped exe kept beside it as a backup; Steam depot 47811, build
  id 21245176.
- Environment verified: DAOLib 0.5.0 loader working in the install
  (`loader.log`: 103293 address IDs, entry point detoured, game reached the
  main menu); two **named instances** (`DAOLIB_INSTANCE=host` and `=guest`)
  ran side by side from an isolated full copy of the install, each with its
  own Documents profile seeded from the player's saves, no writes to the real
  profile.
- Plugin skeleton (`dao-coop`) builds against DAOLib with VS 2022 C++ and
  vcpkg (detours/spdlog/directxtex, triplet `x86-windows-static-md`).
- Milestones 0 and 1 verified in the game (2026-10-20): the built plugin loads
  (`daocoop.dll: loaded` in the loader log) and reads live state
  (`mode explore`, party bar, controlled creature id/area/position/health);
  two named instances then ran side by side, each in its own profile, both in
  the world, and after loading the same save **both agreed on the controlled
  creature's object id and the area id** — the object-id alignment a
  save-transfer join depends on. A full two-instance test rig was run; no
  networking exists yet (milestone 2 next).


- BioWare no co-op in 2009: https://www.eurogamer.net/dragon-age-has-no-co-op-multiplayer
- Multiplayer mode cut (2021 interviews): https://www.thegamer.com/dragon-age-origins-multiplayer-cut/
- DAOLib: https://github.com/Whocares66-Dev/DAOLib , docs https://whocares66-dev.github.io/DAOLib/
- DAOAddressLibrary: https://github.com/Whocares66-Dev/DAOAddressLibrary
- DAOEluvianMCP: https://github.com/Whocares66-Dev/DAOEluvianMCP
- DAO Turn Based (Nexus): https://www.nexusmods.com/dragonage/mods/7053
- Decompilation project press: https://www.pcgamesn.com/dragon-age-origins/modders-decompiling-source-code
- DAO engine lineage discussion / wiki: https://en.wikipedia.org/wiki/Dragon_Age:_Origins
- xoreos: https://github.com/xoreos/xoreos
- Steam app: https://store.steampowered.com/app/47810/Dragon_Age_Origins__Ultimate_Edition/


## 2026-10-23 — combat arbitration, death/revive, rejoin, in-game Event Log

All verified in the game on the isolated test install; the scripts named are in
the `dao-coop` repository.

- **Combat arbitration** (`tools\attack-test.ps1`, PASS). The guest's
  `dao::commands::OnStart` sees its controlled creature's attack order; the
  plugin turns it into an AttackRequestPacket (request id, attacker, target)
  instead of letting the local game attack. The host validates (known guest,
  monotonic request id, both objects exist, target alive, same area, hostile),
  calls `dao::orders::Attack` in its own game and answers with a
  AttackResultPacket; the guest retries the same id twice on a bad-attacker
  answer. The host repeats the order every ~120 frames so the swing completes;
  position mirroring is skipped only during the first 30 frames of a swing.
  `tools\demo-fight.ps1` shows both games fighting the same ogre.
- **Death and revival** (`tools\death-test.ps1`, PASS). A fallen host player
  does not end the world while a party member is alive; after the fight
  `RevivePartyAfterFight` sets fallen party members from zero to maximum health
  and returns control to the host's avatar. A corpse left in the hostile table
  is not "a fight in progress" any more (this kept revive from firing).
- **Guest rejoin** (`tools\reload-test.ps1`, PASS). When the host loads
  another save mid-session, its `OnGameLoaded` (second and later loads) marks
  the world reloaded, re-saves the transfer slot and serves it again; the guest
  sees the flag, resets its join state, receives and loads the new slot
  (loading screen) and re-selects its party member.
- **In-game Event Log** (`tools\ui-shot.ps1`, `tools\ui-join-shot.ps1`, PASS).
  A plugin-owned SWF 8 HUD screen: ActionScript 2 source
  (`ui/eventlog/frame_1/DoAction.as`) is compiled by FFDec into
  `ui/coop_eventlog.swf` with the game's `$TitleFont`/`$BodyFont` imported from
  `gfxfontlib.swf`; the pipeline and the shell generator are adapted from
  DAOTurnBased's `tools/movie.py` (GPLv3, the same author as DAOLib). The
  plugin adds the screen with `dao::gui::AddScreen(L"CoopEventLog",
  L"coop_eventlog", dao::gui::kLayerHud)` and `ShowScreen`, then pushes
  `EventLog`'s last eight lines plus their categories and a revision through
  `Movie::SetArray`/`SetVariable`; the movie redraws only on a new revision.
  The panel lays itself out from `dao::world::ScreenSize()` in screen pixels,
  has ALL/COMBAT/MULTI/SYSTEM filter tabs and folds to its latest line on a
  click. Screenshots in `dao-coop/docs/images/`.
- **Creature names in the log**: `dao::script::api::GetName` (script command
  275) gives "Айдан", "Морриган", "Огр"; the log is UTF-8 and the UI bridge
  widens it with `MultiByteToWideChar` for AS2.
- Also verified earlier in the same session: damage/death channel, release
  package on the LAN address, pause agreement. `docs/STATUS.md` in the project
  repository has the evidence for each round.

## 2026-10-23 (later) — shared Event Log and independent players

The tester reported the two panels showing different events and the players being
dragged into one position. Root causes and fixes:

- **Log desync.** Each game only wrote the events it generated itself (the host
  the combat lines, the guest its join line). Protocol version 8 adds an
  `EventPacket` (232 bytes, category + UTF-8 text): `Notify` sends every line,
  the receiver adds it to its own `EventLog` (`event(remote): ...`).
- **A transport bug this exposed.** The pending-packet buffer was sized to the
  state packet (176) while the event packet is 232, and each packet kind had its
  own `Receive` call that scanned the socket and kept one pending datagram; with
  a fourth kind the scans starved each other (the host reported `recv 0` and
  the guest dropped hundreds). Fixed by `sizeof(EventPacket)` and one
  `Transport::ReceiveAny` dispatch loop in the plugin.
- **Players bound to one position.** A guest that set no `select_party`
  controlled the host's own Warden; the mirror then rewrote its position to the
  host's every packet ("teleported back"). A guest now takes party index 1 by
  default, the mirror is skipped until it has switched, and the apply counters
  start from there.
- Automated: `independence-test.ps1` (different ids, guest `applied` 0,
  furthest distance 2.14 m, puppet 1.12 m, both join lines in both logs) and
  `walk-test.ps1` PASS on the 0.7.1 release build. The artificial-ogre
  `combat-test` does not pass on the Fort Drakon save: `CreateObject`'s ogre
  is not returned by the hostility query there; the fight path is covered by
  `attack-test` and `demo-fight`.
- **LAN path re-verified on 0.7.3** (2026-10-23, this machine's address
  `192.168.56.1`): `independence-test -PeerIp 192.168.56.1` PASS — both peers
  bound `0.0.0.0` and sent to the LAN address, `dropped 0` on both sides,
  the event packet carried both join lines to both logs. Two physical PCs are
  still untested.
