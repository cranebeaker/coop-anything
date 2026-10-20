# Call of Duty: Modern Warfare 2 (2009)

| | |
|---|---|
| Engine | IW 4.0 (closed; id Tech 3 derived) |
| Versions tested | Steam app 10180, build 25374375 (the 64-bit update of September 2026): `iw4sp.exe` x64, PE timestamp `0x6aaaa257`, SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`, `version` dvar `IW4 2.0 build 14 Fri Oct 03 16:56:21 2025 win64`, Russian language set |
| Projects | [mw2_coop](https://github.com/stxlvn/mw2_coop) (campaign co-op, prototype, MIT); COD-MOD 3.223 campaign co-op pack (32-bit, 2020, abandoned) |
| QA level | automated (two game copies on one PC, in-game scripted checks on S.S.D.D., Team Player and Wolverines! set pieces; the campaign does not run through yet) |
| Last updated | 2026-10-20 |

## Where things stand

**Work in progress.** The campaign cannot be played through together yet. On the current
64-bit Steam build, two copies of the game connect through the game's own Spec Ops co-op
netcode and play the first mission (S.S.D.D.) together: player 2 is placed and armed with
player 1, checkpoints work, both players down restarts the level at the last checkpoint, the
host saves progress to continue later, and both move on to the next mission. In Team Player
both share the convoy Humvee (first to board takes the turret, the other a seat, they can
swap); in Wolverines! both ride in the Humvees. The other missions have no co-op fixes yet.
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
- **mw2_coop** ([stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop), MIT, v0.2.0-alpha):
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

All in [mw2_coop](https://github.com/stxlvn/mw2_coop) (v0.2.0-alpha and later commits on main; v0.2.0-alpha ships an install archive with `.bat` launchers for host, continue and join):
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
  the client's party (the v0.2.0-alpha join launcher runs it before `connect`).
- QA-only helper to pair two copies on one Steam account: a shifted XUID (`!xuid_add`; the host
  ignores a `coopConnect` from its own XUID). No shipped launcher uses it.

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
- 2026-10-19: `intro` ended the session at the rifle hand-out ("no viewmodel set for player
  0"); fixed by setting player 2's first-person hands.
- Earlier run (no QA script): both copies in S.S.D.D. in co-op for about 110 s, the client
  sees the host's player model with its name tag; the host plays the intro scene, the client
  does not.

## How to play

Not playable through yet (prototype). Testers: the v0.2.0-alpha install archive of mw2_coop
has `.bat` launchers to host, continue and join; see the mw2_coop README.

## Known issues

- Vehicles in the other missions (Cliffhanger snowmobile, Endgame boat, the underwater start of
  The Only Easy Day...) are not done.
- A slow scripted move of player 1 can leave player 2 behind (no distance leash yet).
- Campaign records (missions unlocked in the menu) are not updated in co-op.
- Only S.S.D.D. has level fixes; in-engine scenes elsewhere are scripted for player 1 only.
- Joining through a Steam invite with two accounts is untested.

## Next steps

- The other vehicle missions: a second snowmobile in Cliffhanger and an end trigger that
  waits for both players; a distance leash; a hint for the seat swap.
- Vehicle sections for two players: survey all missions' scripts.
- Check whether MW3 (2011) `iw5sp.exe` carries the same co-op netcode; its write-up says the
  singleplayer engine has none, which this MW2 finding questions.
