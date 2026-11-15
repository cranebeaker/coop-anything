# South Park: The Stick of Truth (2014)

| | |
|---|---|
| Engine | Onyx (Obsidian), inherited from Dungeon Siege III; evidence: `engine.ini` `project_name = Onyx`, `onyx` and `c_00101_ds3_logo_00` strings, and Onyx multiplayer replication templates in the exe (their arguments name player classes; no bare player class in RTTI). Signature-based engine detection returns `unknown`. |
| Versions tested | Steam app 213670, build 228353, Windows x86; exe 11,468,656 bytes, SHA-256 `54a83d05…56a0d6`, `build_version.txt` `1380--2014-03-30_08-32PM` |
| Projects | None found (see "Existing work") |
| QA level | none |
| Last updated | 2026-11-15 |

## Where things stand

There is no playable co-op mod, and nobody has shown a second player in this game. Two
menu-only guarded sessions were run (2026-11-04, 2026-11-15); they established how to test
safely and what the engine still contains, not co-op.

Target, so no later reader relaxes it: **two players on two PCs, each with their own screen,
over the network, without port forwarding**. A second character on one machine is a development
milestone, not co-op.

## Existing work

Searches on 2026-11-04 and 2026-11-15 found no co-op project: GitHub repository search for
"stick of truth" returns trainers, a localization patch and archive extractors; GitHub issue
search for `"stick of truth" coop` returns zero. Nexus, ModDB, Reddit and the Steam forums
were **not** checked. This is not proof that no attempt exists.

Prior work on this machine, by an earlier agent session:

- a test copy of the install, an extractor for the `.oaf` archives, 316 extracted AMX scripts
  and a read-only AMX API index;
- a Frida file guard that redirects the profile and save paths into a per-run folder, denies
  unknown writes, and passes 8 x64 + 7 x86 synthetic checks (these check the guard, not co-op);
- a guarded launcher that reaches the main menu with the original profile untouched;
- a read-only Winsock/Steamworks probe (added 2026-11-15). Tooling and its limits are in
  `tools/README.md`.

## How the game works (for co-op)

The shipped executable still contains Obsidian's Onyx multiplayer stack: Steam session and
P2P managers, a LAN session manager, chat and voice managers, host/ghost replication classes
for `Player_Object`, `Creature`, `Item`, `Door`, `Level_State`, `Secondary_Player_Object` and
`Remote_Camera_Object`, the `LOCAL_WITH_NETWORK` mode, a `connect IP<:port>` console command
with friends (`disconnect`, `restart_server`, `server`, `status`, `kick`, `warp_to_player`),
and inherited tuning cvars (`multiplayer_local_enabled`, `multiplayer_remote_enabled`,
`multiplayer_tether_range`, `multiplayer_tether_screen`, `Multiplayer camera ownership`).
That is Dungeon Siege III's co-op engine, linked in and unused.

In a menu-only run the engine initialises Winsock but never binds or listens, never dials out,
never creates a Steam lobby or game server, and only keeps polling `SteamNetworking()`. So the
code is present and idle; whether it can be started is the open question.

The scripting layer cannot help here: the 314 native imports in the 316 AMX scripts include no
session, server or network entry point, and the archived bytecode is marked `CRYPT`. The
network code is native engine code.

The game reads a loose `data/` directory layer (movies ship loose there, `data/global` is empty
in the retail install) over the 1.9 GB `data_archive.oaf`. That is the modding seam. The engine
also looks at `data/global/console_scripts` at startup, but no `.txt` in it was ever opened,
so console-script autoload is unproven.

The profile lives in `%USERPROFILE%\Documents\My Games\South Park - The Stick of Truth`
(options plus saves). The test harness redirects it; the owner disabled Steam Cloud for the
title.

## What we changed

No game files were changed and no freight was shipped. This session added:

- a read-only network probe and a guarded runner (`tools/`), and four recorded menu runs;
- a correction of the earlier notes: `startup_verified` was hardcoded `false`, the native
  "layout verified" claim is weaker than stated, and the previous next gate (AMX loader) cannot
  reach a network session;
- the evidence table in `NOTES.md` and a six-part plan with one prompt per agent in `PLAN.md`
  and `prompts/`.

## QA

- 4 guarded launches (`py -3.14 -B tools/run_net_probe.py`, ~50 s each, isolated copy):
  `original_profile_unchanged`, `guard_integrity` and `original_exe_hash_still_matches` were
  `true` in every run.
- Guard unit tests from 2026-11-04: 8 x64 and 7 x86 synthetic checks passed.
- None of this tests co-op. QA level stays `none`.

## How to play

No release exists. The game cannot currently be played in co-op through this work.

## Known issues

- Nothing is known about reaching gameplay under the harness: the profile had no saves, and
  `PrintWindow` returns black frames (only the foreground `BitBlt` fallback produced a readable
  menu screenshot).
- Console access, the multiplayer console command table, the actor/object model and the input
  path for a second player are all unverified.
- `.text` is Steam-CEG protected, so static disassembly of the game's functions is not
  trustworthy; conclusions must come from runtime evidence.

## Next steps

Product target: two players, two PCs, two screens, over the network, no port forwarding.

Gate 0: prove whether Onyx netcode can be started (host, then client, then two instances in one
PC with `-multiple_instance`). Gate 1: get console/dev access. Gate 2: a gameplay-state harness.
Gate 3: the actor/object model. Gate 4: a second controllable body and input (local development
milestone, not co-op). Gate 5: transport between two machines. Gate 6: the remote player, the
guest's own screen, join and reconnect — the point at which it is actually co-op. Gate 7: sync
scope (turns, story, saves, transitions). Details and one prompt per agent: `PLAN.md`, `prompts/`.

## Release shape

Co-op ships as a **launcher plus an in-game mod**: the mod does the work, the launcher does host,
join, version checks, install and uninstall. The launcher alone cannot do co-op. Details, first
release scope, transport choice and what each player must own: `SHAPE.md`.
