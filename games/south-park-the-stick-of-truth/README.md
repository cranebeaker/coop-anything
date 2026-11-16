# South Park: The Stick of Truth (2014)

| | |
|---|---|
| Engine | Onyx (Obsidian), inherited from Dungeon Siege III; evidence: `engine.ini` `project_name = Onyx`, `onyx` and `c_00101_ds3_logo_00` strings, and Onyx multiplayer replication templates in the exe (their arguments name player classes; no bare player class in RTTI). Signature-based engine detection returns `unknown`. `engine.ini` says `use_directx_11 = false`, but the process loads `d3d9.dll`, `d3d11.dll`, `dxgi.dll` and `d3dx11_43.dll`, and resolves the D3D11 entry points through `GetProcAddress`; which path presents is **not yet proven** (the Steam overlay loads D3D11 too). |
| Versions tested | Steam app 213670, build 228353, Windows x86; exe 11,468,656 bytes, SHA-256 `54a83d05…56a0d6`, `build_version.txt` `1380--2014-03-30_08-32PM` |
| Projects | None found (see "Existing work") |
| QA level | none |
| Last updated | 2026-11-15 |

## Where things stand

There is no playable co-op mod, and nobody has shown a second player in this game. What exists is
a tested way to drive the game safely and a settled answer to the one question that decides how
co-op gets built here: **the engine's own multiplayer cannot be started** (see below), so co-op is
built as our own transport and replication driven from inside the process.

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
code is present and idle.

**It cannot be woken up from outside (settled 2026-11-15).** Three routes were tried, all on the
isolated copy under the guard:

- **Console commands.** The engine's own `exec` handler runs a script file, echoes every line and
  dispatches each name — then answers `unknown var` for `find`, `status` and `write_file`. No
  console command is registered in this build, so `connect` and `restart_server` cannot be reached
  by name.
- **Direct handler calls.** `status` works and prints its labels. `restart_server` and `connect`
  kill the process, at the menu and inside a loaded level, without reaching `bind`, `listen`,
  `ws2_32!connect`, `SteamGameServer_Init` or `SteamMatchmaking`.
- **The session manager behind `restart_server`.** The handler jumps to a non-executable address
  before any session function runs; zero network events.

So the multiplayer classes in the exe are dead code in a single-player build, and the plan is the
thick path: our own transport and replication, built on the runtime object model. Release shape and
scope: `SHAPE.md`; decision and MVP: `PLAN.md`.

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

- Guarded launches on an isolated copy, `original_profile_unchanged`, `guard_integrity` and
  `original_exe_hash_still_matches` true in every run: 4 menu-only, 2 two-instance, 3 reaching a
  loaded level, 2 `-startsavegame`, and the console/handler experiments that settled the netcode
  question.
- Guard unit tests from 2026-11-04: 8 x64 and 7 x86 synthetic checks passed.
- None of this tests co-op. QA level stays `none`.

## How to play

No release exists. The game cannot currently be played in co-op through this work.

## Known issues

- Reaching gameplay is shown only for a loaded level that idles; controllability is not shown. The
  profile does contain saves (corrected 2026-11-15), but `-startsavegame` does not load one in this
  build (two runs, with and without `.sav`), and `PrintWindow` returns black frames (only the
  foreground `BitBlt` fallback produced a readable menu screenshot).
- The multiplayer console commands cannot be executed and their handlers fault, so the actor/object
  model must come from memory scanning, not from game commands.
- `.text` is Steam-CEG protected and decrypted page by page, so live disassembly of `call` targets is
  not trustworthy; conclusions must come from behaviour and memory values.

## Next steps

Product target: two players, two PCs, two screens, over the network, no port forwarding. Release
shape: a launcher plus an in-game mod (`SHAPE.md`).

Gate 0 is **closed with a no**: the shipped Onyx netcode cannot be started (see "How the game
works"). From here the plan is the thick path, in this order:

1. Gate 2 (done): a way into a loaded level that proves itself.
2. Gate 3 (static done, runtime in progress): the live actor/object model, which values to
   replicate, and whether a second player body can exist.
3. Gate 4: a second controllable body driven by real input (local development milestone, not co-op).
4. Gate 5: transport between two machines — a direct address inside a VPN, no port forwarding.
5. Gate 6: the remote player and the guest's own screen, join after load, leave, reconnect. This is
   where it becomes co-op.
6. Gate 7, next milestone: sync scope (combat turns, story, saves, transitions).

MVP scope and stop rules: `PLAN.md`; one prompt per agent: `prompts/`.

## Release shape

Co-op ships as a **launcher plus an in-game mod**: the mod does the work, the launcher does host,
join, version checks, install and uninstall. The launcher alone cannot do co-op. Details, first
release scope, transport choice and what each player must own: `SHAPE.md`.
