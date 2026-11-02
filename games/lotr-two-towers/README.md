# The Lord of the Rings: The Two Towers (2002)

| | |
|---|---|
| Engine | Stormfront "Fusion" (Xbox), played on PC through the static recompilation [The Two Towers Recompiled](https://github.com/Gumpolas/TooTawersRecomp) v1.9.4 (xboxrecomp 0.12.0) |
| Versions tested | US Xbox retail, `Default.xbe` SHA-256 `ae2d1b5304b3e9572339b693a6a45ad0869bdaeec89aae374b5c14feebbb8d30` (title 0x45410010, version 1), with the port's source at v1.9.4 (also v1.9.2) |
| Projects | Couch co-op on top of the port: [Volverine-Dude/TooTawersRecomp-coop](https://github.com/Volverine-Dude/TooTawersRecomp-coop) (a fork of the port, MIT; see its `COOP.md`) |
| QA level | automated (a 14-scenario bot suite on the real game, 73 checks; see QA) |
| Last updated | 2026-10-21 |

## Where things stand

Local couch co-op: two players on one PC, two controllers, one shared camera, playing the story levels together. No
networking: one game runs, with a second hero in it that has its own controller, character, weapons, health, ammo,
experience and upgrades. It is an alpha; the known issues are below. Source only: each player builds it from their
own copy of the game.

## Existing work

- No co-op mod, patch or hack for The Two Towers was found (Co-op Anything directory, web search, 2026-10-17). The
  game has no multiplayer and no netcode.
- Two PC ports exist, neither with co-op: [The Two Towers Recompiled](https://github.com/Gumpolas/TooTawersRecomp)
  (MIT for its own code, LGPL parts from xemu) and [Athelas](https://github.com/Shagg88/Athelas) (MIT; its headless
  Linux harness is a good model for bot tests).
- Return of the King has built-in 2-player co-op, but on a different engine (EA's own); it is a design reference only.

## How the game works (for co-op)

One process, one simulation, two heroes. Everything below is for the US retail build; addresses are the port's (xbe
virtual addresses).

- **"The player" is a set of globals**, `[0x43B15C]` the hero, `[0x43B160]` its controller, `[0x1CB8B4]` the pad
  port, plus several blocks around `0x43B1xx` and `0x1B0Cxx`. Player 2 is a second hero (a twin of player 1, built
  from player 1's placement record while the level loads) whose updates run with those globals swapped to its own
  copy. Most of the game then works for it unchanged.
- **Level scripts** set the hero up by name (`@AttachToBone`, `@SetEnemyID`, `@SetWeaponID`...). They are replayed
  for the twin with object lookups (0xB4690) answering it.
- **Hero records**: `0x4AB598 + hero byte × 0x348` (hero byte `[0x4AB69D]`: 1 Aragorn, 2 Legolas, 3 Gimli,
  4 Isildur). Experience at +0x6C, the level's running experience at +0x90, the combat rating meter at +4. Player 2
  gets its own copy, swapped in with its context. Upgrades: 80 bytes per hero at `0x4AC5B0 + hero × 0x50`.
- **Character type** (+0x2F0) sets health and ammo (+0x304, max +0x306): Aragorn 0 (30), Legolas 7 (60), Gimli 8
  (25).
- **Weapons** are objects (class 0x174980, owner +0x11C, weapon id +0x2F4 → reach and data).
- **Things that ask "is it the player?"** and needed player 2 handled: the hit handler (0x849F0, rating meter), the
  kill award (0x99D40), pickups (0x8FC60 → 0x8F960), the enemies' target choice (0x5A920) and brain scripts, the
  camera (0x68DC0), the move picks (the hero getter 0xD2310).
- **Checkpoints**: `@SetRespawnId` (0xB0FB0) keeps the hero's health, ammo and record +0x90..+0xB3 at 0x1BB354; a
  game over's Replay puts them back.
- **Front end**: screen object at 0x4AD050 (+4 the next screen; 9 stats, 10 shop, 7 save, 1 map, 6 play). End of
  mission 0xF1230(code).

Findings, with the evidence and the wrong leads: [NOTES.md](NOTES.md).

## What we changed

Each item was checked with bot runs (frame dumps and logs); details and evidence in NOTES.md.

- **Second player**: a Co-op menu (New Game / Load Game, co-op options), player 2's turn in the hero select, its own
  controller, HUD (health, ammo, rings, gauge) and head markers.
- **Placement**: player 2 starts beside player 1 on a walkable cell of the level's pathmap, follows cutscenes, and is
  put back beside player 1 after a fall into the void.
- **Shared camera**: frames the midpoint of both heroes and zooms out (field of view) as they separate; caps Normal
  1.3, Wide 1.5.
- **Combat**: enemies target either hero (threat per player); player 2's hits, kills and experience count for player
  2; downed / respawn / game over for two.
- **Player 2's own moves**: its move picks read its own hero's bought upgrades (not player 1's), and its lock-on is
  the game's own target pick.
- **Checkpoints**: after a game over's Replay, player 2's level experience and per-level stats go back to its values
  at the checkpoint, as the game does for player 1.
- **Player 2's own weapons**: copies of its hero's weapons with its hero's weapon ids, so Gimli swings with his axe's
  reach and never takes player 1's bow; player 1's weapons it has no copy of are never touched by its events.
- **Ranged weapons**: player 2 throws or shoots from its own copy of a ranged weapon (Legolas' bow, Gimli's throwing
  axe, Aragorn's bow; Isildur, who has none, uses Aragorn's), never from player 1's.
- **Ammo and pickups**: player 2 has its own hero's ammo; its pickups refill it.
- **After a level**: player 1's stats, player 2's stats, player 1's shop, player 2's shop, then the save. Player 2's
  experience and purchases are saved (in its hero's record) and come back after loading. The shops page through the
  level tabs with left and right; Replay is left out where the screens follow each other (both shops, player 2's
  stats).
- **Head markers**: P1 / P2 over the heroes, projected through the camera's own frustum, and dimmed when something is
  drawn in front of the head (a depth probe in the Direct3D 11 renderer).
- **Difficulty** (co-op option): enemy damage, the heroes' health, the respawn wait and extra enemies from the
  spawners.
- **P2 Bot** (co-op option): a stopgap AI for player 2. It keeps within a circle around player 1 and the companions,
  melees enemies near it (walking up and swinging only beside them), shoots from inside the circle at enemies further
  off, blocks now and then, roams while player 1 is down, and at the Watcher parries by a tentacle and attacks one
  after a parry lands. To be rewritten with proper controller settings for player 2.

## QA

A bot suite runs the real game (a test copy of the install) fourteen times, with both heroes driven by scripted pads,
steering bots or the P2 Bot, and test switches that set up each situation, then checks the game's log for pass/fail.
The game's own frame dumps are kept for each run. It is in the fork, `tools/coop-bot/`.

| Scenario | What it checks |
|---|---|
| start_place | player 2 loads as Gimli from the save's record, with Gimli's own character type, placed beside player 1 on the level's pathmap |
| weapons | player 2 refuses player 1's weapons, its axe gets Gimli's weapon id, its throws are axes from its own copy, its own ammo goes down while player 1's stays, its move picks read its own hero's upgrades |
| weapons_bow | player 2 Isildur shoots from its own copy, never from player 1 Legolas' bow |
| kills_post | a kill by player 2 pays it experience, every award for its kills runs in its context, player 1 (idle) gets none, player 2's stats screen gets its level's experience, upgrades bought for its hero open its moves and player 1's hero's never do |
| levelup_shops | each player's shop gets that player's level-up and points |
| pickups | player 2's quiver touches refill player 2, never player 1 |
| down_respawn | player 1 down while player 2 stands: no game over, player 1 respawns beside player 2 |
| down_gameover | both down: game over |
| menus | the Co-op menu's background, the hero select giving each player a hero, the level starting with player 2's hero |
| camera | the camera zooms out to the Normal cap as the heroes part, both head markers placed on screen, the markers' depth probe reads a point under the ground as hidden |
| checkpoint | from the Watcher's checkpoint: after a game over and Replay, player 2's level experience and per-level stats are back at its checkpoint values with player 1's; the P2 Bot goes near a tentacle and parries, and never shoots there |
| shop_left | left pages the level tabs in both shops and is held back where it would reach the hidden Replay; player 2's stats screen has no Replay |
| save_buy | player 2's shop closes with its experience committed, the game saves (player 2 buys an upgrade) |
| save_reload | player 2's experience and its whole saved record (checksum, the bought upgrade included) come back after loading |

Every scenario also checks for no crash and a median frame rate of 50 or more. The test copy's settings and save are
restored after each run.

Last full run (port source 1.9.4 with the co-op changes as published, Direct3D 11, 2026-10-21):

```
Co-op bot suite, 2026-10-21 20:19, 24 min: 73 passed, 0 failed of 73 checks in 14 scenarios
```

Not covered by the suite: a second physical controller (the suite drives pad 2 by script), Vulkan, 4:3 screens, and
the frames themselves (checked by eye).

## How to play

Build the fork from your own copy of the game, as the port: see its
[COOP.md](https://github.com/Volverine-Dude/TooTawersRecomp-coop/blob/coop/COOP.md). The port's `generate.py` needs the US
retail `Default.xbe` above. Never share built executables or generated code: they contain the game's code.

## Known issues

- The Watcher's tentacles take no hits from either hero in bot runs, so how the P2 Bot should win that fight is
  still open.
- The P2 Bot can get stuck wading at the lake before the Gates of Moria boss; no bot test reaches that spot (the boss
  checkpoint starts past it).
- Head markers are dimmed, not hidden, behind walls (depth can't tell a wall from an orc in front); the depth probe is
  Direct3D 11 only. Markers drifting outward when zoomed out was seen once and not reproduced.
- Big enemies (the cave troll): whether the P2 Bot gets close enough to hit them is untested.
- Hobbit guests' marker height is a guess; 4:3 screens are unchecked.
- The bot tests drive player 2 with scripted pads; a second physical controller is not covered by them.

## Credits

- **[The Two Towers Recompiled](https://github.com/Gumpolas/TooTawersRecomp)** by Gumpolas: the native PC port this
  co-op is built on. Its static recompilation, renderer, menus, extra heroes and settings make all of this possible;
  the co-op is a set of changes on top of it.
- **[xboxrecomp](https://github.com/sp00nznet/xboxrecomp)** by sp00nznet: the static recompilation toolkit the port
  uses (with LGPL parts from xemu).
- **[Co-op Anything](https://github.com/cranebeaker/coop-anything)** by cranebeaker and contributors: the playbook
  this work follows (triage, bot QA, the write-up and directory).
- **[Athelas](https://github.com/Shagg88/Athelas)** by Shagg88: its Two Towers notes and headless test harness were
  used as references.
- The Lord of the Rings: The Two Towers belongs to its rights holders; nothing from the game is included here.

## Next steps

- Controller settings for player 2, then rewrite the P2 Bot.
- The Watcher's tentacles: find what lets a hero's weapon hit them.
- A ready-to-play download, if one can be made without shipping the game's code.
