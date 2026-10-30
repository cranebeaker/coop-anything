# Zoo Tycoon (2001)

| | |
|---|---|
| Engine | Blue Fang's own engine (C++, DirectDraw), built on with [openzt](https://github.com/openzt1/openzt) |
| Versions tested | Zoo Tycoon Complete Collection, `zoo.exe` 12.4.0.9, SHA-256 `9cec15e08b8c0bac1a7418422452ffb22e6d98f397fb8c4e69ec13faa05a43ef` |
| Projects | [ZooTogether](https://github.com/multiplayereverything/zoo-tycoon-coop) (directory entry "Zoo Tycoon / ZooTogether") |
| QA level | automated |
| Last updated | 2026-10-30 |

## Where things stand

Two players can run one zoo together over direct TCP. The host's zoo is the real one; a friend joins by
IP, receives the zoo, and then budget, date, pause, admission price, research, everything either
player builds, sells, fences in, paints or terraforms, adopts or hires, and where the animals and staff
are stay shared. Each player sees the other's map cursor. Guests are not synced (on the joining copy
they are decoration; all money is the host's). Early prototype (0.11.0-dev), tested in a two-copy lab,
not yet by players over the internet.

## Existing work

None found before this project: no Zoo Tycoon (2001) co-op or multiplayer mod on GitHub, Nexus, ModDB
or forums (web search, 2026-10-30). The 2013 Xbox *Zoo Tycoon* has its own official co-op; it is a
different game. [openzt](https://github.com/openzt1/openzt) (Finn Hartshorn, Eric Galvan) is an
open-source framework for the 2001 game (Rust DLL loaded as `res-openzt.dll`, a generated table of
thousands of vanilla function addresses, Rust ports of many managers, a Lua console) with no networking.
ZooTogether is a patch on openzt.

## How the game works (for co-op)

- No netcode and no multiplayer mode to reuse. No Steamworks. The game refuses a second instance
  (`FindWindowA` at 0x5337e6); the lab patches that check in memory after starting suspended.
- The game loads `res*.dll` from its folder, which is how openzt gets in without patching the exe.
  openzt's console runs Lua on the game thread from its `ZTApp::updateSim` detour; ZooTogether's tick
  runs there too.
- The simulation is not deterministic across machines, so this is host-authoritative state sync, not
  lockstep.
- Building: a purchase puts an entity on the cursor (`ZTMapView::setTempEntity` 0x4f189a) and the map
  click calls `ZTMapView::placeEntityOnMap(entity, cost, 0)` (0x4d9088). Fence drags, paths, staff
  hires and animal adoptions are placements too. Objects are named on the wire by the type's
  `zt_type`/`zt_sub_type` strings (e.g. `building/vendfood`) plus world position and rotation.
- Selling/firing ends in `ZTMapView::removeEntityFromMap` (0x50a0b0), which asks "are you sure?" for
  buildings through a modal dialog unless the map view's byte `+0x456` is clear.
- Only animals and staff can be picked up (`pickUpSelectedEntity` 0x46ab75); buildings can't be moved.
- Terrain lives in tile bytes `+0x80..+0x84` (`BFTile`, 0x8c bytes); each tile's graphics are
  pointers recomputed from its shape (`BFTile::validatePositions`, `BFTile::calculateShape`).
- Research: openzt's `ztresearch.rs` port (branch funding level, selected program chosen at random,
  progress).

Full addresses and evidence: `handoff.md` in the ZooTogether repo.

## What we changed

All in openzt's `coop` feature (`openzt/src/coop.rs` plus small call-outs), shipped as a patch on
openzt `df01b96`. Highlights, each with its lab check:

- **Join in progress:** the host saves through the game's own `SAVE_GAME` (save dialog detoured),
  streams the file, the client loads it like the main menu does. (`zoo_transfer`, `layout`)
- **Budget:** host streams cash; client purchases through vanilla `subtractCash` become sequenced
  spend intents the host applies once. (`cash_join`, `host_change`, `client_spend`, `no_double`)
- **Date, pause, admission price, research:** the host's values ride in its state; the client's
  changes are intents it keeps until the host confirms. Research also forces the host's randomly
  picked next program. (`date`, `host_pause`, `client_pause`, `admission_*`, `research_*`)
- **Cursors:** world-space mouse position, drawn on the other screen from a `GXVideoManager::flip`
  detour. (`cursor_*`)
- **Building, selling, moving, exhibits:** replay of placements and removals. A replayed entity needed
  `BFEntity::snapToGrid` to be drawn at all; fences then needed their exact position restored (snap
  moves edge objects half a tile); the replaying copy skips the "New Exhibit Name" prompt and turns
  its own auto-placed gate back into fence so only the builder's gate counts. (`build_*`, `remove_*`,
  `move_c2h`, `fire_c2h`, `exhibit_*`, `path_c2h`, `hire_c2h`, `animal_c2h`)
- **Animals and staff:** once a second the host sends every animal and staff member; the client
  pairs them by type and nearest position, replaces drifted ones, creates births and removes deaths.
  Only the player's own sell/fire is sent the other way. (`living_*`)
- **Terrain:** tile-byte diff twice a second each way, then the receiver recomputes the tile's
  graphics. (`terrain_c2h`)
- **Crashes found and fixed on the way:** stale pointers in the undo history and selection after a
  replayed object was removed (crashed the next build); a nested modal loop re-entering the per-frame
  hook (deadlock, fixed with `try_lock`); writing an entity's position directly (left it on its old
  tile; a later path build crashed).

## QA

`python scripts/lab_all.py` in the ZooTogether repo starts two hardlinked copies of the game (host and
client), hidden off-screen at below-normal priority, drives them through openzt's Lua console and posted
mouse messages, and runs 35 checks plus 11 mutants (each mutant breaks one feature on purpose and the run
must fail exactly the checks that cover it). Drags (fences, paths, terrain) work in parked windows via a
lab-only `GetCursorPos` detour (`ZT_LAB_FAKE_MOUSE=1`).

Last suites (2026-10-30, research build, 0.11.0-dev):

| Run | Result |
|---|---|
| full suite 15:23-16:01, 2 normal + 11 mutants | one normal run 35/35, the other failed only `date` (the host's clock stood still for the 4 s window); all 11 mutants caught |
| rerun 16:04-16:10, 2 normal + `nojoin` | 35/35, 35/35, `nojoin` caught |

The earlier full suites (terrain build 14:41-15:07, living-state build 13:44-14:08) passed every run.

## How to play

Build the DLL from the ZooTogether repo (`scripts/get-openzt.ps1`, `scripts/build.cmd`) and copy it
into the game folder as `res-openzt.dll`. Host: set `ZT_COOP_HOST=27150` and load a zoo; friend: set
`ZT_COOP_JOIN=<host ip>` and stay on the main menu. Forward TCP 27150 or use a VPN. No release yet.

## Known issues

- Guests aren't synced; on the joining copy they come and go on their own.
- Animals' needs, happiness and health aren't synced, and pop-ups ("... has escaped") come from each
  copy's own simulation. An animal that drifts is replaced on the joining copy, so its name and age
  restart there.
- A player loses their own undo history whenever the other player builds, moves or removes something.
- The exhibit's name stays "Exhibit N" on the copy that didn't build it.
- One client per host; no reconnect, relay or in-game host/join menu.

## Next steps

Guests (coarse count or positions), animal needs, awards, an in-game host/join UI, relay support, and
a real two-PC playtest.
