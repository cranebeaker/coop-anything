# Call of Duty: Modern Warfare 2 (2009)

| | |
|---|---|
| Engine | IW 4.0 (closed; id Tech 3 derived) |
| Versions tested | Steam app 10180, build 25374375 (the 64-bit update of September 2026): `iw4sp.exe` x64, PE timestamp `0x6aaaa257`, SHA-256 `2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805`, `version` dvar `IW4 2.0 build 14 Fri Oct 03 16:56:21 2025 win64`, Russian language set |
| Projects | [mw2_coop](https://github.com/stxlvn/mw2_coop) (campaign co-op, alpha, MIT); COD-MOD 3.223 campaign co-op pack (32-bit, 2020, abandoned) |
| QA level | automated (two game copies on one PC, in-game scripted checks; a mission does not run through yet) |
| Last updated | 2026-10-19 |

## Where things stand

The campaign cannot be played together yet. On the current 64-bit Steam build, two copies of
the game connect through the game's own Spec Ops co-op netcode and load a campaign mission
(S.S.D.D.) together, with both players in the level. The level scripts still assume one
player: the first scripted weapon hand-out ends the session, and only S.S.D.D. has fixes.
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
- **mw2_coop** ([stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop), MIT, v0.1.0-alpha):
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
  thread; runtime script errors are invisible unless the function is replaced.
- Function addresses (RVAs) with what they do are in [NOTES.md](NOTES.md) and in
  `src/game.h` of mw2_coop.

## What we changed

All in [mw2_coop](https://github.com/stxlvn/mw2_coop) v0.1.0-alpha:
- `winmm.dll` proxy that checks the build, runs console commands from a file, watches dvars
  and memory, logs co-op packets and script runtime errors.
- Script loader replacement (`Scr_AddSourceBuffer`): own scripts plus load-time patches of the
  game's script text, so the mod never ships game scripts.
- Co-op campaign framework (`_mw2coop.gsc`): campaign mode in a co-op session, player 2
  regrouped with player 1, weapon sync, checkpoints as level start points, restart when both
  players are down, mission end request. S.S.D.D. patch: checkpoints, ammo and ending for
  both players.
- QA-only helpers to pair two copies on one Steam account: a shifted XUID (the host ignores
  a `coopConnect` from its own XUID) and `mw2coop_party_join`, which stands in for the Steam
  lobby step that activates the client's party.

## QA

- `tools/qa_pair.sh <scenario>` in mw2_coop starts a host and a client copy, connects them,
  launches the mission and runs a scenario from `_mw2coop_qa.gsc` on both machines.
- Last run, 2026-10-19, scenario `intro` (S.S.D.D. start): 5 PASS on host and client (two
  players, campaign mode, player 2 placed with player 1, same weapons); the 6th step (give
  player 1 the rifle) ended the session with the engine error "no viewmodel set for player 0
  for 'm4_grunt'". Scenarios `restart` and `end` not run yet.
- Earlier run (no QA script): both copies in S.S.D.D. in co-op for about 110 s, the client
  sees the host's player model with its name tag; the host plays the intro scene, the client
  does not.

## How to play

Not playable yet. Testers: see the mw2_coop README (a copy of the game folder, hosting and
joining through console commands).

## Known issues

- Giving player 1 a weapon at the start of S.S.D.D. ends the co-op session.
- In-engine scenes are scripted for player 1 only.
- Restart, mission end and the move to the next mission are untested.
- Joining through a Steam invite with two accounts is untested.

## Next steps

- Set the first-person hands on both players in co-op (`maps\_loadout` sets them only on
  `level.player` in its campaign branch), rerun `intro`, then `restart` and `end`.
- Next-mission transition: find out what `missionSuccess` does in a co-op session.
- Vehicle sections for two players (the owner asked for them): survey all missions' scripts.
- Check whether MW3 (2011) `iw5sp.exe` carries the same co-op netcode; its write-up says the
  singleplayer engine has none, which this MW2 finding questions.
