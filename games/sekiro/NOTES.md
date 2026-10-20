# Sekiro Online (Nexus 577) — revive notes

Started 2026-10-20. Target game identity is in the owner's `local/identity.md`:
Steam appid 814380, buildid 5794815, `sekiro.exe` SHA-256
`637aca52…98b856`, Windows 11 26200.

## Why this game, this path

Triage (AGENTS.md phase 0): the only released Sekiro co-op mod is **Sekiro Online
v2.05**, and it is abandoned. Path 1 (revive), skill `coop-revive`.

The second directory entry, `mstampfli/sekiro-coop` (Rust, AGPL), is a from-zero
prototype whose own README says it is not playable (remote player not rendered);
it is a different project, not a revival target.

## Build under revival (verified)

- Archive: Nexus mod 577, file `Sekiro-Online-v2.05-577-2-05-1623599819.zip`
  (downloaded 2026-10-03, present twice, identical).
  SHA-256 `be6ef9f61b73d7faa8c2e78ecf35808fde45ebaf1fc2a5240198dbf99ad311cc`.
- Contents: `dinput8.dll` (424,960 B, SHA-256 `b40dd9c3…7dad1`), `SekiroOnlineFont.ttf`,
  `readme.txt` (dated 2020-12-30). No license file. PDB path embedded in the DLL:
  `H:\Development\workspace\soulsmodding\Sekiro-Online-Release\x64\Release\dinput8.pdb`.
- Source: **not public.** The author's repo `github.com/yuiamoroll/Sekiro-Online`
  (also served at `LukeYui/Sekiro-Online`) contains only `VERSION`, `DATA`, `README.md`,
  `LICENSE`. The Nexus FAQ says source may be released "one day" but not now.
  => any revival must be a **separate package that patches the shipped DLL**, not a rebuild.

## Static audit of `dinput8.dll` v2.05 (x64, image base 0x180000000)

Proxy DLL: exports only `DirectInput8Create` (loaded by the game as a dinput8 proxy).

Imports that matter:
- `steam_api64.dll` + `steamclient64.dll`: `SteamMatchMaking009`, `SteamFriends017`,
  `SteamNetworking005`, `SteamNetworkingSockets009`, `SteamUser020`, `SteamApps008`,
  `SteamNetworkingUtils003`. Classes in the binary: `CProtoServer`, `CSummonSignManager`,
  `CNetworking` with `LobbyCreated_t`, `LobbyEnter_t`, `LobbyMatchList_t`,
  `GameLobbyJoinRequested`, `LobbyChatUpdate`, `ValidateAuthTicketResponse`,
  `InitRelayAccessCallback`, `ConnectionStatusChanged`.
  => connections are **Steam lobbies + Steam P2P**, not raw sockets to a game server.
  `YuiKeyNetworking` appears to be the author's own Steam networking layer.
- `WS2_32.dll`: raw TCP, used with the hardcoded endpoint below.
- `WINHTTP.dll`: `WinHttpOpenRequest`/`WinHttpReadData` — update/version check.
- `d3d11.dll` + `D3DCOMPILER_47.dll` + imgui shaders: in-game overlay/menu (F1).

Hardcoded endpoints and paths (ASCII strings in `.rdata`):
- `74.91.125.31` and `54000` — the central "summon sign" server (readme: "There is a
  server, however this is only used for the summon signs").
- `raw.githubusercontent.com`, `LukeYui/Sekiro-Online/master/VERSION`, `.../DATA`
  — version gate and online-item id table.
- Logs: `SekiroOnlineLog.txt`, `SekiroOnlineErrorLog.txt`, `imgui.ini` (game folder).
- UI strings confirm the item-driven flow: Cursed Emblem (host), Demon Eyes (invade),
  Coward's Bell (leave), Vengeful Spirit / Serendipitous Spirit (invite), beacons.

## Live checks actually run (2026-10-20)

1. `GET https://raw.githubusercontent.com/LukeYui/Sekiro-Online/master/VERSION`
   -> HTTP 200, body `v2.05`. `.../DATA` -> HTTP 200, 16 item ids `01100001…`.
   So the **version gate still passes**: the mod sees itself as current.
2. TCP connect to `74.91.125.31:54000` -> **timeout after 5 s** (no SYN-ACK).
   One host, one attempt; a firewall could also drop it. => the matchmaking/summon-sign
   server is not answering; treat public summon signs as **broken** until proven otherwise.
3. Game not running; Steam client is running. No live game test attempted yet.

### Live boot test (isolated copy, 2026-10-20 01:29–01:40)

Set-up: `C:\SekiroOnlineTest` built from the owner's install with `cp -al` (hardlinks;
~70 MB real), ReShade/DLSS/other mods removed from the copy, v2.05 `dinput8.dll`
installed, `steam_appid.txt` = 814380. Owner's saves backed up first.

- Launch 1: reached the title screen (`Sekiro`, 3.3 GB working set). Module list shows
  `DINPUT8.dll` loaded from `C:\SekiroOnlineTest` (1.5 MB mapped) next to the system
  `dinput8.dll` — i.e. the mod is loaded and the game did not crash on build 5794815.
- Launch 2 (immediately after `taskkill` of launch 1): process died at 131 MB with the
  mod's own "Sekiro - Fatal Error" dialog. Relaunching a few seconds later worked
  normally (3.0 GB title screen). **Cause unknown** — possible stale Steam/session state
  after a forced kill. Needs a controlled retry to call it a bug.
- The mod writes `SekiroOnlineLog.txt` **into the process working directory**. Launching
  `sekiro.exe` from a shell made that the shell's cwd, not the game folder, which is why
  the first launch left no log next to the exe. Launch through Steam and it lands in the
  game folder.
- That log is a **crash log**. The failed launch produced:
  `Fault Offset: 0x1419DCA99`, `Mod version: v2.05`, `YuiKey module: 0x9186181B2F2D2EA4`,
  `RCX/RDX/RAX = 0`, return address `0x1419DC8B8` at `[RSP+78]`. With the game image base
  0x140000000 that is roughly `sekiro.exe+0x19DCA99`, i.e. a null-register access inside
  the game, caught by the mod's handler. Saved at
  `local/evidence/SekiroOnlineLog-crash-20261020-0136.txt` (not committed).
- No `imgui.ini` appeared: the overlay was never toggled because synthetic keys are ignored.
- No connection to `74.91.125.31:54000` was attempted at the title screen; the only
  established connection was Steam's (`35.166.24.62:10901`).
- Entering the world could not be automated: `keybd_event`/`SendKeys` input is ignored
  (the game reads raw input), so in-world/host testing currently needs a human.

### Package built and tested (this workspace)

`sekiro-online-revive/`: `install.ps1`, `uninstall.ps1`, `verify.ps1`, `watch.ps1`,
`README.md`. Installer verifies the pinned archive/DLL/exe hashes, moves an existing
`dinput8.dll` aside, writes a manifest, backs up `S0000.sl2`. Ran install → uninstall →
install on the isolated copy; identity gate passes and the restore path works.

## Derived (not yet proven)

- Friend-invite co-op probably does **not** need the dead server: invites go through
  Steam lobbies (`GameLobbyJoinRequested`, `LobbyEnter_t`), and the readme's own
  instructions for playing with a friend are "host creates a session, then invite through
  the Steam friends list". If that path still works, the mod is playable today for a
  group of friends even with the summon-sign server gone.
- If the lobby path is also broken, the hardcoded `74.91.125.31` / `54000` strings can be
  **binary-patched to any host**, so a community matchmaker is technically feasible
  (needs the wire protocol captured/reimplemented — not started).

## Unknowns

- Whether v2.05 boots without crashing on build 5794815 today (the 2023-24 bug tracker has
  "Crash on start? 1.06 and 2.05" and "Cursed Emblem crash" reports).
- Whether the summon-sign server is globally dead or just unreachable from this network.
- Whether two clients on one Steam account can host+join (needed for automated QA).
- Whether the author's promised rewrite ("Sekiro Seamless Co-op") will ever ship
  (Patreon Feb 2026 reportedly says development "ground to a halt"; page returned 403,
  so this is **unverified**).

## Co-op test kit and the two-instance question (2026-10-20)

Goal: assert a real co-op session, not just that the game boots. Findings:

- The mod's HUD is readable by OCR and is the only session oracle it gives us:
  `[Sekiro Online v2.05 ...] [Steam: Ready] [Server: Unavailable] [Connection: Offline]`.
  `Connection:` changing off `Offline` is the pass signal. Verified 9/9 samples on build
  5794815 with full-frame 2x OCR; a 4x crop of the top strip reads less, not more.
- A second local client is blocked at three layers:
  1. `sekiro.exe` creates `Global\SekiroMutex` (UTF-16 string at file offset 0x2bdb3f6).
     A second plain launch exits in 0.6 s with code 51 and no log or dialog.
  2. Patching that string in a copy also exits with code 51 (Arxan / code-tamper check).
  3. me3 (`-e <copy> -n sekiro_mutex_escape.dll --disable-arxan`) attaches, then the
     guest process still disappears. Early CreateProcess-suspended injection of the same
     DLL crashed the guest too.
- Even with two processes, a lobby join needs a second Steam identity; only one account
  is signed in on this PC, and Steam multi-client is unsupported.

Delivered instead: `revive-package/coop-test/` records both PCs (HUD OCR + TCP) and
`report.py` returns PASS / NO CO-OP / INCONCLUSIVE. The verdict logic was checked against
synthetic pass/partial/none/unreadable logs; a single-client run correctly returned
NO CO-OP. Input automation is deliberately not part of it (raw input ignores synthetic
keys; virtual-pad automation was dropped by request), so humans drive the two item uses.

## Can a bot stand in for the second client? (2026-10-20)

Checked the Co-op Anything MCP server directly (`tools/mcp/coop_mcp.py`, `tools/list`):
its tools are `search_directory`, `detect_engine`, `game_identity`, `list_docs`,
`read_playbook`, `get_skill`, `get_game`, `validate_entry`. It is a knowledge and
analysis server. It cannot launch a game, inject a mod, or play.

An in-process bot in the host's world would not be a co-op peer here: Sekiro Online
admits players through Steam lobbies and auth tickets, and the remote player is a
networked entity driven by that peer's client. A locally spawned phantom would prove
nothing about the join path, the mod's sync, or the dead matchmaker. That is exactly the
"hollow pass" the QA playbook warns about, so it is not a substitute for a second client.

Two routes remain for a real session: a second PC with its own Steam login (cheap, needs
the friend), or the playbook's one-PC Steam stand-in (a mock `steam_api64.dll` with
lobbies and NetworkingSockets plus a working mutex bypass). The second is a project in
itself and still needs the mutex problem solved, since a patched exe trips Arxan.

## Plan (coop-revive)

1. ~~Isolated test install, boot test of v2.05 alone.~~ done: boots on build 5794815.
2. Host a session in-world; read `SekiroOnlineLog.txt` for lobby creation vs. server errors.
   Blocked on human input (raw-input menus) and on the mod's logging option.
3. Two-client host+client test (owner has a second Steam account; friend available) —
   the real question, and the only thing that decides whether this revive is playable.
4. Fix as a separate package: DLL shim that loads the original `dinput8.dll`, applies
   binary patches (e.g. endpoint redirect, crash guards) and ships an installer +
   one-click host/join. Nothing redistributed from the original without permission.
5. QA: scripted launch/boot/lobby checks with real clients; then a directory entry update
   with the honest QA level.

## Sources

- Nexus mod 577 description/logs/bugs: https://www.nexusmods.com/sekiro/mods/577
- Author repo (VERSION/DATA only): https://github.com/yuiamoroll/Sekiro-Online
- Steam discussion, Jul 2024 connection trouble:
  https://steamcommunity.com/app/814380/discussions/0/4521135162292556345/
- Eurogamer (co-op, region transitions, save backup):
  https://www.eurogamer.net/this-sekiro-mod-lets-you-play-co-op-and-pvp-with-your-friends
- Siliconera (beta, sliding phantom workaround):
  https://www.siliconera.com/sekiro-online-mod-adds-co-op-and-pvp-on-pcs/
- Rust prototype (not this project): https://github.com/mstampfli/sekiro-coop
