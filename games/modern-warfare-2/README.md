# Call of Duty: Modern Warfare 2 (2009)

| | |
|---|---|
| Engine | IW 4.0 (closed; id Tech 3 derived) |
| Versions tested | Steam app 10180, build 25374375 (the 64-bit update of September 2026): `iw4sp.exe` x64, PE timestamp `0x6aaaa257`, SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`, `version` dvar `IW4 2.0 build 14 Fri Oct 03 16:56:21 2025 win64`, Russian language set |
| Projects | [mw2_coop](https://github.com/stxlvn/mw2_coop) (campaign co-op, prototype, MIT); COD-MOD 3.223 campaign co-op pack (32-bit, 2020, abandoned) |
| QA level | automated (two game copies on one PC, in-game scripted checks of the reworked set pieces and a spawn and kit check per level; the campaign has not been played through end to end) |
| Last updated | 2026-10-22 |

## Where things stand

**Work in progress (prototype).** On the current 64-bit Steam build, two players play the
campaign together through the game's own Spec Ops co-op netcode, hosted and joined from the
game's menus (lobby, list of games on the network, address, Steam invite). Reworked for two
players: S.S.D.D. (each takes his own weapons), Team Player's convoy, the Humvees of
Wolverines!, Takedown's car, The Only Easy Day's swim and stealth kill, Cliffhanger's ice climb
and snowmobiles, No Russian's elevator, cover and ending, the crash site and ISS of Second Sun,
Endgame's boat chase and the final knife fight. Enemies fight both players. Other set pieces are
still written for one player, and nobody has played the whole campaign through together yet.
The older COD-MOD campaign co-op pack only runs on the 32-bit build that the 2026 update
replaced, and players who tried it found it rough.

## Existing work

- **COD-MOD 3.223 campaign co-op pack** (2020). A `iw4sp.hk` DLL ("Call of Duty -
  Singleplayer Modification", built 2020-09-16) loaded by a renamed 1.0.159 exe
  (`iw4sp_codmod.exe`), plus `zone/custom/x_coop.ff` (campaign script changes),
  `a_coop_models.ff`, `z_customanim.ff` and `spdata/x_core.iwd` (menus). Distributed through a
  Steam guide with a mega.nz pack and Hamachi, host + 1 client
  ([Steam thread](https://steamcommunity.com/app/10180/discussions/0/3087772148451672664),
  2021-10, comments to 2025-11; a 2023 comment reports a mission that freezes while loading).
  The DLL source is [FreeTheTech101/cod-mod](https://github.com/FreeTheTech101/cod-mod)
  (momo5502, 223 commits, last 2014-12, no license file): `code/PatchMW2_Coop.cpp` only
  re-enables the Spec Ops co-op netcode over direct IP. The campaign logic is in `x_coop.ff`,
  author unknown. Its `trainer.gsc` differs from the game's in about 40 lines: player 2 is
  teleported to a fixed spot and gets weapons at the pit; nothing for cutscenes, restarts or
  mission end. Logs from a 2026-07 run with this pack: a crash loading The Hornet's Nest,
  whole-level restarts, missing Spec Ops end screens, lobby time-outs. It cannot run on the
  64-bit build (32-bit exe, 32-bit zones).
- **iw4x-sp** (AlterWare): singleplayer client for the 1.0.159 build; no co-op
  ([install guide](https://forum.alterware.dev/t/iw4x-singleplayer-install-guide/383)).
- **IW4x** ([iw4x/iw4x-client](https://github.com/iw4x/iw4x-client)): open-source multiplayer
  client, not the campaign.
- **mw2_coop** ([stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop), MIT, v0.3.1-alpha):
  this project, for the 64-bit build.

## How the game works (for co-op)

- **The 2026 update made the game 64-bit**, content included: every zone in `zone/<language>`
  keeps the `IWffu100` header and version `0x114`, but its asset list holds 8-byte pointers.
  32-bit mods and tools (COD-MOD, iw4x-sp, widescreen fixes) stopped working. The exe is
  wrapped in SteamStub (`.bind` section holds the entry point); `.text` is not encrypted.
- **Scripts** (GSC) are plain text stored as zlib rawfiles inside the zones: scanning an
  inflated zone for `name\0` followed by a zlib stream extracts them without a full asset
  parser, on 32-bit and 64-bit zones alike.
- **The singleplayer exe carries a 2-player co-op netcode** (Spec Ops): `listen`, `connect`,
  `coopLaunch`, Steam lobby invites, `coop_*` dvars. It is **lockstep**: the client also
  compiles and runs the level scripts (`sv_running 1` on both), so every script change must be
  identical on both machines. Handshake seen on the wire: `coopConnect` → `coopAccept`
  (carries the map) → `coopAcceptAck` → bandwidth test → `coopLaunch <map>` → `coopLoaded`;
  `fast_restart` on the host sends `coopRestart` to the client.
- A Spec Ops level is a campaign map with a different level script. Loading a campaign map
  as the co-op map (`coop_mapName`) works; `maps\_load` then sets up `level.player2`, and
  `maps\_coop` brings the downed/revive system. The lobby sets `specialops 1`, which sends
  campaign scripts down their Spec Ops branches, so a mod has to make `is_specialop()` false
  for campaign maps.
- **What closes the door without a code mod:** command-line `+set`/`+exec` are ignored,
  `config.cfg` only applies `set`/`seta`/`bind`, the console does not open, console output and
  file logging are compiled out, loose GSC files are not read, a second instance quits
  (PID file `<game dir>\__iw4sp`), and `listen`/`connect` need Steam logged in plus the
  DemonWare online state. A `winmm.dll` proxy is loaded from the game folder and opens all of
  it.
- **Co-op scripts see few dvars:** in a co-op session GSC `getdvar` goes through
  `SV_GetDvarCoop`, which returns the real value only for dvars flagged with one of
  `0x20c4` and folds name and value into the co-op validation checksum. Dvars made with
  `set` or by script read as "" — including `start`, so level start points do nothing in
  co-op unless the flag is set on both machines.
- **Retail `Scr_Error` drops its message** and only longjmps out of the failing script
  thread; runtime script errors are invisible unless the function is replaced. A failing
  script can be found anyway: the VM's live code position and the callers' saved positions
  are in memory, and `Scr_GetFunctionHandle` maps function names to offsets in the program
  buffer until `Scr_EndLoadScripts` frees its table (mw2_coop hooks it and logs
  `file::function +offset` stacks; RVAs in NOTES.md).
- **The revive system (`maps\_coop`) assumes Spec Ops:** it reads the flag
  `special_op_terminated`, which only `maps\_specialops` creates, and kills a player who
  bleeds out (Spec Ops then fails the mission). In the campaign the flag has to exist and be
  set when the level ends.
- **Local-player builtins break lockstep:** some builtins only work on the machine's own
  player (seen: `GetPlayerIntelIsFound`); on the client the host's player is not local, so
  the script fails on one machine only. (`GetLocalPlayerProfileData( "autoAim" )` in
  S.S.D.D. ran on both machines without an error.)
- **Saves and progress:** in a co-op session the game writes no save; campaign records are
  local-player profile data. A level start point (`start` dvar) is the progress unit that both
  lockstep machines can share.
- **Next mission:** `ChangeLevel()` does nothing in co-op (its exit handler skips gameMode
  "so"), the party is inactive in a level and the client ignores a second `coopLaunch`; each
  machine has to start the next map itself the way the lobby launch does.
- **Dvars a level sets and reads back** read as "" in co-op (same getdvar filter): Team
  Player's `newintro` turned an SP-only intro back on that unlinked the player from the opening
  scene. Values a script sets are equal on both machines, so they can be made readable.
- **Turrets:** a second `turret UseBy( player )` after `EnableTurretDismount()` takes a player
  off; handing a turret from one player to the other in a single server frame crashed both
  machines, over three frames it works.
- Function addresses (RVAs) with what they do are in [NOTES.md](NOTES.md) and in
  `src/game.h` of mw2_coop.

## What we changed

All in [mw2_coop](https://github.com/stxlvn/mw2_coop) (latest release v0.3.1-alpha of 2026-10-22; since v0.3.0-alpha an install archive with `winmm.dll` and the `mw2coop` folder, hosting and joining in the game's menus, no `.bat` launchers):
- `winmm.dll` proxy that checks the build, runs console commands from a file, watches dvars
  and memory, logs co-op packets and script runtime errors.
- Script loader replacement (`Scr_AddSourceBuffer`): own scripts plus load-time patches of the
  game's script text, so the mod never ships game scripts.
- Co-op campaign framework (`_mw2coop.gsc`): campaign mode in a co-op session, player 2
  regrouped with player 1, weapon and first-person hand sync, checkpoints as level start
  points, restart at the checkpoint when both players are down or one bleeds out, mission
  end. S.S.D.D. patch: checkpoints, ammo and ending for both players. Patches to
  `maps\_coop` (revive) and `maps\_intelligence` (no intel in co-op) for the campaign.
- Saved progress: the host keeps mission and checkpoint in `mw2coop\progress.txt`;
  `mw2coop_continue` resumes there, and the host sends the client the start point in an
  out-of-band packet next to `coopLaunch`, so both machines start at the same point.
- Script errors logged with GSC call stacks; crashes logged with the faulting RVA; builtin
  method tracing for QA.
- Next mission: each DLL launches it (`mw2coop_launch_map`); test god mode (`mw2coop_god`).
- Vehicles: player 2 takes a free seat of player 1's vehicle (`_mw2coop::seat_partner_in_vehicle`,
  a named squadmate can give up his seat); Team Player turret for whoever boards first, seat
  swap by holding Use, ride end and crash for both, Shepherd's scene for both; Wolverines!
  opening and closing Humvee rides.
- Joining by address: `mw2coop_party_join` stands in for the Steam lobby step that activates
  the client's party (QA and console joins run it before `connect`; the in-game join does the same).
- QA-only helper to pair two copies on one Steam account: a shifted XUID (`!xuid_add`; the host
  ignores a `coopConnect` from its own XUID). No shipped launcher uses it.
- In-game co-op menus (the `.bat` launchers are gone): host lobby with mission select and access
  (open, Steam friends, password), a list of games on the network, join by address or Steam
  invite, an in-game Windows Firewall popup and UPnP port forwarding for the host.
- Third-person bodies: the game only has `coop_body_*` player models in the Spec Ops zones, so the
  DLL loads a level's Spec Ops zone (or one with the same uniform) with it in a co-op session.
  Loose Ends' loadout still has an unfinished Infinity Ward co-op branch that asks for ghillie
  suits no zone has (red placeholder bodies); patched to the forest uniform.
- Asset pools: Just Like Old Times plus a body zone overflowed the 1350 `loaded_sound` pool. Each
  pool is a free list (`{ head; entries[n] }`, a free entry points to the next), so the DLL links
  650 more entries in front of it before the first co-op level loads (pool sizes rva `0x3bf930`,
  pool pointers rva `0x3bfe00`, the asset alloc that raises "Exceeded limit" rva `0xeee30`).
- Set pieces since the last write-up: Takedown's car (each ducks on his own key), The Only Easy
  Day's SDVs, surfacing and stealth kill, Cliffhanger's climb (per-player icepicks and catch) and
  snowmobiles, No Russian's elevator and ending (Makarov shoots both), Second Sun's crash, ISS and
  falling helicopters, Endgame's knife fight (whoever strikes first plays it, the other is
  stunned and watches), Endgame's boat (first aboard drives, the other lies at the bow across from
  Price on a body with Price's boat animations and shoots where he looks; Price keeps his side).
- Loose Ends, Shepherd's betrayal (v0.3.1-alpha): both players go through
  the ending as Roach, held on his view with no bodies or names of their own (Ghost's drag while
  fighting on his back, the LZ, the body toss, the fire). Shepherd's shot hits both; player 2's
  screen then goes black until the bodies are thrown, and both go in the one throw (only Roach's
  and Ghost's bodies are seen). COD-MOD left player 2 out of this scene.
- v0.3.1-alpha also: every mission intro (the title on black) holds both players, not just player
  1; UPnP finds the router on a PC with several network adapters (the search leaves from the
  adapter of the internet route).
- S.S.D.D.: both start inside the range's sandbag bay, where COD-MOD puts its two players (v0.3.0-alpha put
  player 1 inside the bay's back wall, where he could not walk); each takes his own rifle and pistol off the
  table (another lies there until both have one; holders near it keep full ammo so they cannot
  pick it up as ammo) and his own frags. COD-MOD let only one player take the rifle.
- Enemies: the engine already lets AI pick either player (QA `targets` on Loose Ends: 151 and 129
  of 918 enemy samples on players 1 and 2). Level scripts that send enemies at `level.player`
  (spawner favourite enemy "player", player seekers, attack triggers, 16 level scripts) now pick
  the closest player still up; scenes that hide the player from enemies (`ignoreme`) and a
  level's `attackeraccuracy` / `threatbias` for the player cover both, and `setthreatbiasgroup`
  is mirrored to player 2. COD-MOD had no fix here (only `ignoreme` and health hacks). No
  Russian: straying and grenades count for either player; a blown cover kills both.

## QA

- `tools/qa_pair.sh <scenario>` in mw2_coop starts a host and a client copy, connects them,
  launches the mission and runs a scenario from `_mw2coop_qa.gsc` on both machines.
- Runs on 2026-10-20, S.S.D.D., host and client, no script errors on either machine in any
  of these:
  - `intro`: two players, campaign mode, player 2 next to player 1, same weapons, player 2
    gets the rifle with player 1 (6/6 PASS).
  - `restart`: checkpoint reached, player 2 down (not dead), both down, `coopRestart`, level
    back at the checkpoint, players up (all PASS).
  - `end` (start point `end`): mission end reached, next mission `roadkill` found and saved
    as progress.
  - `resume @continue`: host starts with `mw2coop_continue`, client with no start point;
    both start at the saved checkpoint `bullet_penetration`.
  - `end` after the transition fix: both machines load Team Player and exchange `coopLoaded`.
  - `god`: heavy damage downs neither player.
  - Wolverines! `ride`: player 2 seated in the opening Humvee, together while driving, both
    out together.
  - Team Player `rkintro`, `board`, `rideend`, `turret`: see the list above (both in Shepherd's
    scene; first to board is the gunner, the convoy waits, swap and swap back; ride end and
    crash for both; turret hand-over).
- 2026-10-21, host and client, no script errors on either machine:
  - S.S.D.D. `pickups`: rifle (player 2 takes it, another lies there, player 1 takes his, the
    drill goes on), `frag` (both take frags from the box), `pit` (pistol the same way), both in
    the sandbag bay on the floor: all PASS.
  - Endgame `boat`: 14 PASS (boarding, driver, passenger across from Price, his body's gun
    follows his view, Price keeps his side through the level's 4 position triggers, 30 s ride).
  - `spawn` with weapon and threat checks on S.S.D.D., Team Player, Cliffhanger, No Russian,
    Takedown, Wolverines!: 9/9 PASS each; Just Like Old Times loads and spawns (PASS). The other
    levels were not rerun on this build.
  - `look` on Loose Ends: both bodies in the forest uniform on both screens (screenshots).
- Bots press real buttons (since v0.3.1-alpha): a scenario asks for a press with
  `press( player, "activate", ms )` and that player's own machine runs the key binding
  (`mw2coop_bind +activate`), so Use or Forward reaches the server in his input like a key press.
  `+activate` and `+forward` are no console commands in this build, which is why queuing them
  did nothing before. Runs on 2026-10-21, host and client, identical results on both:
  - `buttons` on S.S.D.D. and Whiskey Hotel: each player walks forward on a Forward press, the
    server sees his Use button and not the other player's: all PASS.
  - S.S.D.D. `pickups` with real input: both take the rifle and the pistol off the tables with
    one Use press each, both walk up to the frag box with Forward and get 4 frags: all PASS.
  - Found with it: player 1's place in S.S.D.D.'s sandbag bay was inside the bay's back wall
    (Forward moved him 0 units), and the level intro (`maps\_introscreen`) held player 1 only:
    player 2 walked and took a pistol under the title. Fixed: both stand where COD-MOD puts its
    two players (player 1 on the left place by the bags, player 2 on the level's start), and
    every intro holds both players.
  - 2026-10-22: `bay` on every range start (default, `timed_ads`, `bullet_penetration`, `frag`):
    both inside the sandbags, not stuck; `introfreeze`: under the title a real Forward press moves
    neither player and both walk afterwards on S.S.D.D., Whiskey Hotel, The Hornet's Nest and
    Exodus (on Takedown, No Russian and The Only Easy Day both ride something under the title:
    not checked); `pickups` (all three starts), `buttons`, `takedown`, `oilrig`, `dcemp`, `intro`:
    all PASS.
  - 2026-10-22, Loose Ends `betrayal` (start `ending`, god mode off): the ending runs to the
    mission's end (113 s), both players at Roach's place and held all through, nobody down,
    Shepherd's shot hits both (health down to 2): all PASS; screenshots of player 2 in the drag,
    at the LZ and at the body toss.
  - Older scenarios still drive players through script (positions, angles, notifies); real input
    on those set pieces is checked by hand.
- 2026-10-19: `intro` ended the session at the rifle hand-out ("no viewmodel set for player
  0"); fixed by setting player 2's first-person hands.
- Earlier run (no QA script): both copies in S.S.D.D. in co-op for about 110 s, the client
  sees the host's player model with its name tag; the host plays the intro scene, the client
  does not.

## How to play

Alpha: see the mw2_coop README. Both players copy `winmm.dll` and the `mw2coop` folder from the
[v0.3.1-alpha release](https://github.com/stxlvn/mw2_coop/releases) archive into the game folder; the host picks Campaign -> co-op, the partner joins from
the list, by address or by Steam invite.

## Known issues

- Set pieces not reworked yet are scripted for player 1 (scenes, some triggers); player 2 is kept
  next to him, but some scenes show only to player 1.
- Global screen effects (slow motion, field of view, vision sets) show on both screens.
- A slow scripted move of player 1 can leave player 2 behind; on Wolverines! start point `yards`
  the players ended 2255 units apart (not looked into yet).
- Campaign records (missions unlocked in the menu) are not updated in co-op.
- Loose Ends start point `forest_fight` logs two script errors from the level's own start code.
- Joining through a Steam invite with two accounts is untested.

## Next steps

- Rerun the spawn and kit check on the levels not covered by this build.
- Survey every remaining set piece for two players (gulag, contingency, the rest of estate,
  boneyard, af_caves, the Washington levels).
- Enemy scaling for two players; later, support the 32-bit build.
- Check whether MW3 (2011) `iw5sp.exe` carries the same co-op netcode.
