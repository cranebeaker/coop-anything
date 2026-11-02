# Call of Duty: Modern Warfare 2 (2009)

| | |
|---|---|
| Engine | IW 4.0 (closed; id Tech 3 derived) |
| Versions tested | Steam app 10180, build 25374375 (the 64-bit update of September 2026): `iw4sp.exe` x64, PE timestamp `0x6aaaa257`, SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`, `version` dvar `IW4 2.0 build 14 Fri Oct 03 16:56:21 2025 win64`, Russian language set |
| Projects | [mw2_coop](https://github.com/stxlvn/mw2_coop) (campaign co-op, beta, MIT); COD-MOD 3.223 campaign co-op pack (32-bit, 2020, abandoned) |
| QA level | automated (two game copies on one PC, in-game scripted checks of the reworked set pieces, a spawn and kit check on six levels, a bot tour of the start points of all 19 levels; the campaign has not been played through end to end) |
| Last updated | 2026-10-25 |

## Where things stand

**Beta, playable (v0.5.0-beta,** also on [Nexus Mods](https://www.nexusmods.com/codmw2/mods/99)**).** On the current 64-bit Steam build, two players play the
campaign together through the game's own Spec Ops co-op netcode, hosted and joined from the
game's menus (lobby, list of games on the network, address, Steam invite). Reworked for two
players: S.S.D.D. (each takes his own weapons), Team Player's convoy, the Humvees of
Wolverines!, the rope and rappel scenes (the Gulag's cellblock rope and evacuation, Just Like Old Times' opening
rappel, Escape from Favela's big jump and chopper ladder), the Only Easy Day's escape little bird, Takedown's car, The Only Easy Day's swim and stealth kill, Cliffhanger's ice climb
and snowmobiles, No Russian's elevator, cover and ending, the crash site and ISS of Second Sun,
Endgame's boat chase and the final knife fight. Enemies fight both players; both play the
difficulty the host picks, with a lobby switch for friendly fire and checkpoints in every
level. Bots toured the start points of all 19 levels on two copies (results in
[NOTES.md](NOTES.md); the Museum epilogue still fails for player 2, see Known issues). Two people
have played parts of the story on two PCs over the internet, from the start up to and including
No Russian (Cliffhanger through, then into No Russian); the missions after it were checked
by bots only, and nobody has played the whole campaign through together yet.
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
- **mw2_coop** ([stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop), MIT, v0.5.0-beta, [Nexus Mods](https://www.nexusmods.com/codmw2/mods/99)):
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

All in [mw2_coop](https://github.com/stxlvn/mw2_coop) (latest release v0.5.0-beta of 2026-10-25; since v0.3.0-alpha an install archive with `winmm.dll` and the `mw2coop` folder, hosting and joining in the game's menus, no `.bat` launchers):
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
- v0.4.0-beta: the test god mode was on in every lobby session since v0.3.0-alpha (it was on
  unless a dvar said "0", which only the removed `.bat` launchers set); now only when the host
  sets it. Both players play the difficulty the host picks: the popup only latches
  `g_gameskill` and `g_gameskill_player_1/2` (`devmap` applies the latch, `coopLaunch` does not),
  and the level start set each player's from his own profile (party member data), so the DLL
  hooks `profile_difficultySave` (each popup button runs it), sends the pick with `coopLaunch`,
  and `_gameskill`'s `updateGameSkill()` takes it in co-op. Recruit had no time between downs:
  the first down broke `maps\_coop`'s down handling. Downs and revives no longer reset a scene's
  threatbias and attackeraccuracy. Mission tour fixes: No Russian crew look (crew mesh on the
  co-op skeleton) and line of fire for both, Cliffhanger's ledge death, an outright death fails
  the mission, partner seats and spots (Gulag, Blackhawk, UAZ, surfacing, Second Sun starts,
  Burger Town), Exodus' laser designator to one random player, intros only on a mission's first
  start.
- v0.4.5-beta: checkpoints in every level (the section a start point begins is reached: the
  level's section functions in `handle_starts`, else the last new flag of a start point's start
  function or its section handler; forward only, never a developers' start); a lobby switch for
  friendly fire (on by default; off gives the partner's damage back in full, since
  `g_friendlyfireDamageScale 0` still leaves 1 per hit); each player's campaign records written
  to his own profile; the Predator's zoom only on the pilot's screen (a DLL thread restores
  `cg_fov` on the other machine: `setClientDvar` drops the co-op session); Team Player's seat swap
  hint; player 2's own flare in Whiskey Hotel, as COD-MOD gave him.
- v0.4.7-beta (from a two-player playtest on two PCs, see NOTES.md "Playtest 2026-10-23"): saves
  per playthrough (`mw2coop\progress\save_<n>.txt`, its players by an id in `mw2coop\player.ini`
  and their Steam IDs; a save picker in the host menu; starting with someone new asks: continue,
  add him to the playthrough, or a new game). Whoever reaches a checkpoint first, the other is
  brought to him; a checkpoint keeps where it was reached (Cliffhanger's `start_c4()` places
  nobody, a restart there put both back at the cave). Five level endings went on to the next
  mission only from the default start ("DEVELOPER: END OF SCRIPTED LEVEL"), and a co-op checkpoint
  is a start point: No Russian, The Enemy of My Enemy, Second Sun (two) and Whiskey Hotel now go
  on. Cliffhanger: the weapon sync took and gave the climbers' ice picks (player 2 fell off the
  wall); the climb plays the easy rules in co-op; any fall from the first wall goes back to the
  bottom; player 2's own spots across the gap and at the bottom were inside the rock, he uses
  player 1's now; one Soap for both catches; the helicopter ending starts when either player gets
  there (it waited for player 2 to get off his snowmobile, which nobody does); 250 health on the
  snowmobiles. A session recorder writes both players' positions, buttons, weapons, damage and
  the allies to `mw2coop.log`.
- v0.5.0-beta (see NOTES.md "v0.5.0-beta (2026-10-25)"): the scenes that were scripted for the level's player
  alone now play for two: the Gulag's cellblock rope (both rappel in turn) and the evacuation rope to the Pavelow
  (each clips on with his own use key), Just Like Old Times' opening rappel (whoever hooks up first rappels and
  kills the guard, the other rappels after him), Escape from Favela's big jump over the gap and the chopper's rope
  ladder (the first to leap starts it, the other catches the same rope), the escape little bird of The Only Easy Day.
  The playtest's reports: Cliffhanger's stealth for both, the snowmobile seen whole by the partner and the rider
  in his own body (the co-op body's mesh on Soap's skeleton, so the level's AI driving animation fits; the DLL
  makes that model in memory), the partner seeing the climber's body instead of four arms, upright ice picks in
  both hands; S.S.D.D.'s gates close once both are through (`GetEntArray` by `script_flag` finds nothing in this
  build: the triggers are now picked by class); Team Player's seat swap by either player; a Password row in the
  lobby. The Russian game gets Tektur and Fira Sans Condensed instead of one generic font. The host's packets are
  accepted only from the partner, the QA command file only if present at start, the log rotates at 5 MB.
  A 32-bit build for the older game ships as a separate experimental archive (`mw2coop-v0.5.0-beta-x86.zip`).
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
    levels were not rerun on this build (the 2026-10-22 tour below covers their start points).
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
  - Real key input (through the key bindings) runs in `buttons`, `pickups`, `introfreeze`, `bay`,
    `look` and the tour's walk check. The older set-piece scenarios (`ride`, `takedown`, `oilrig`,
    `dcemp`, `boat`, `climb` and others) still move players by script (positions, angles,
    notifies, velocity pushes); nobody has checked those set pieces with real input.
- 2026-10-22, level tour (QA `tour`, god mode on): every start point of a level in turn, both
  players up, together, not stuck, a real-input walk, player 1 sent to the two nearest triggers,
  frames from both screens. All 19 levels, 192 sections (17 levels and 179 sections in the first run,
  Whiskey Hotel and the Museum later that day); per-level results and what each FAIL
  was in [NOTES.md](NOTES.md). Run on the mod's main of that day, before v0.4.0-beta, not rerun
  on the release DLL.
- 2026-10-22, v0.4.0-beta (god mode off where it matters): `god` (default on in QA, off without
  the dvar: both downed at 2 health), `downs` (Recruit: before the fix 4 FAIL and the script
  error on both machines, after all PASS), `sceneskill` (threatbias 777 and attackeraccuracy
  0.123 kept through a down and a revive: before 4 FAIL, after all PASS), `skill` (host picks
  through `profile_difficultySave`, client profile on Regular: picks 3, 0 and 2 give the level
  and both players 3, 0 and 2 on both machines). The release DLL from the archive: `skill` with
  3 and `god` with god mode off, 0 script errors.
- 2026-10-19: `intro` ended the session at the rifle hand-out ("no viewmodel set for player
  0"); fixed by setting player 2's first-person hands.
- Earlier run (no QA script): both copies in S.S.D.D. in co-op for about 110 s, the client
  sees the host's player model with its name tag; the host plays the intro scene, the client
  does not.

## How to play

Beta: see the mw2_coop README. Both players copy `winmm.dll` and the `mw2coop` folder from the
[v0.5.0-beta release](https://github.com/stxlvn/mw2_coop/releases) archive (or from [Nexus Mods](https://www.nexusmods.com/codmw2/mods/99)) into the game folder; the host picks Campaign -> co-op, the partner joins from
the list, by address or by Steam invite.

## Known issues

- Slow motion shows on both screens, whoever triggers it (one game clock for both PCs).
- Checkpoints: a restart from one was tested by bots in Team Player and S.S.D.D. and played in
  Cliffhanger and No Russian; in the other levels bots only checked that each checkpoint's flag
  exists, not when a full run sets it.
- The scenes added in v0.5.0-beta (ropes and rappels, the chopper ladder, the Cliffhanger snowmobile and climb
  views, S.S.D.D.'s gates, Team Player's swap) were checked by bots on two copies only; no two people have played
  them. The level-ending fix (endings after a checkpoint restart) has not been played yet either.
- Museum (the epilogue, map `ending`): in the bot tour player 2 cannot walk or ends up far from
  player 1 at the exhibit start points (9 FAIL); not fixed. Its civilians also flood the log
  with `_anim::anim_weight` errors and some stand in a T-pose, in single player too (checked
  with the co-op scripts moved aside).
- No Russian's far ending (nobody at the van door) logs one script error (`AnimScripted` refuses
  the van's driver); the scene plays on.
- Loose Ends start point `forest_fight` logs two script errors from the level's own start code.
- A mouse click on the ice climb sometimes needed repeating; one likely cause is fixed in v0.5.0-beta (a swing
  clicked during the last one counted for nothing), not confirmed by a test: report if it still happens.

## Next steps

- The Museum epilogue for player 2.
- Play the campaign through together end to end, and the new scenes with two people.
- Enemy scaling for two players; finish the 32-bit build (an experimental archive now), split screen.
- Check whether MW3 (2011) `iw5sp.exe` carries the same co-op netcode.
