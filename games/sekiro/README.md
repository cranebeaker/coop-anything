# Sekiro: Shadows Die Twice (2019)

| | |
|---|---|
| Engine | FromSoftware proprietary (Dantelion-lineage: `.bdt`/`.bhd` archives, Oodle, no official co-op) |
| Versions tested | Steam, buildid 5794815 (game v1.06), Windows 11 26200; `sekiro.exe` SHA-256 `637aca52…98b856` |
| Projects | [Sekiro Online v2.05](https://www.nexusmods.com/sekiro/mods/577), [sekiro-coop (Rust)](https://github.com/mstampfli/sekiro-coop) |
| QA level | community-reports (players report co-op; no automated or verified playthrough here) |
| Last updated | 2026-10-20 |

## Where things stand

Sekiro has no official online mode, so co-op means modding. The only released co-op mod,
**Sekiro Online v2.05**, still installs and loads on today's Steam build, but its author
discontinued it in March 2025, the promised rewrite has never been published, and the
matchmaking server it used for summon signs no longer answers. Friend co-op through a
Steam invite is the part that might still work; it has not been verified yet.

## Existing work

- **Sekiro Online v2.05** (LukeYui, Nexus 577, June 2021). Closed source: a 425 KB
  `dinput8.dll` proxy plus a font. Advertises PvP, invasions, co-op for 1 host + 5
  phantoms, enemy and world sync, item-driven session control (Cursed Emblem to host,
  Coward's Bell to leave, Demon Eyes to invade, Vengeful/Serendipitous Spirit to invite).
  Nexus now carries the notice "THIS MOD IS NO LONGER BEING DEVELOPED. A NEW VERSION IS
  BEING MADE AS OF 09/03/2025"; no build of that rewrite exists publicly. The author's
  repository holds only `VERSION`, `DATA` and a license, not the mod's code, and the Nexus
  FAQ explains he is unwilling to publish it because of cheating in Souls games.
  Players reported crashes on start with 1.06 + 2.05 and with the Cursed Emblem in
  2023-24; Lady Butterfly's door stayed closed for phantoms and Guardian Ape failed to die
  in 2024 reports.
- **sekiro-coop** (Rust, AGPL-3.0, me3-injected). Open source, active in 2026, replicates
  player snapshots, flags, effects, items, XP, warps and host-authoritative enemy HP. Its
  own README says it is not playable yet: the remote player is not rendered.

Neither project has a working, documented enemy + story co-op today that a normal player
can install and play with a friend without caveats.

## How the game works (for co-op)

- The mod is loaded as a **DLL proxy**: `dinput8.dll` exports `DirectInput8Create` and
  takes over the game at load. It hooks engine structures (WorldChrMan for players,
  EventFlagMan for world flags, GameDataMan for saves, MapItemMan for rewards) and draws
  an ImGui overlay over D3D11.
- Networking is **Steam lobbies plus Steam P2P** (`SteamMatchMaking009`,
  `SteamNetworkingSockets009`, `SteamFriends017`, auth tickets, `LobbyEnter_t`,
  `GameLobbyJoinRequested`), not a game server. A hardcoded endpoint
  `74.91.125.31:54000` (raw TCP via `WS2_32`) is only used for summon signs.
- `WINHTTP` calls `raw.githubusercontent.com/LukeYui/Sekiro-Online/master/VERSION` and
  `.../DATA` to gate versions and fetch the online item id table.
- Saves are client-side (`%APPDATA%\Sekiro\<steamid>\S0000.sl2`); the mod adds its online
  items to them, so back saves up before installing or removing it.
- Sessions end when a player changes area, and the online items are locked until the
  tutorial has been finished once. Nothing here is server-authoritative, so each client
  simulates its own world.

## What we changed

No gameplay fix is claimed yet. This work establishes whether the last build is still
usable and gives players a safe way to install and test it:

- Verified the shipped archive and DLL by SHA-256 and recorded the game build they were
  tested against.
- Static audit of `dinput8.dll` v2.05: proxy export, engine class hooks, Steam lobby/P2P
  networking, the summon-sign endpoint, the GitHub version gate, log file names.
- Live checks: the version gate and item table still answer; the summon-sign server times
  out; the mod loads and reaches the title screen on build 5794815 without crashing.
- `games/sekiro/revive-package/` installer, uninstaller, diagnostics and a live monitor. The
  installer takes the player's own Nexus archive, verifies it, moves any existing
  `dinput8.dll` aside, backs up `S0000.sl2` and writes an install manifest.

## QA

What actually ran (2026-10-20), on an isolated copy of the game made with hardlinks:

- Installer round trip: `install.ps1` -> `uninstall.ps1` -> `install.ps1`; hashes checked,
  identity gate passes, restore path works.
- Boot test x2: title screen reached, `DINPUT8.dll` loaded from the game folder, 3.0-3.3 GB
  working set, no crash. One launch immediately after a forced kill died at 131 MB with
  the mod's "Sekiro - Fatal Error" dialog; a clean relaunch worked. Cause unknown.
- Service probe: `VERSION` = `v2.05`, `DATA` = 16 item ids, TCP to `74.91.125.31:54000`
  times out.
- Co-op test kit (`games/sekiro/revive-package/coop-test/`): a single-client run read all
  four HUD fields in 9/9 samples and the report returned `NO CO-OP` for an offline client;
  the verdict logic passed synthetic pass/partial/none/unreadable cases. A real two-PC
  session has not been recorded yet.
- Local two-client testing is blocked: `Global\SekiroMutex` stops a second launch
  (exit code 51), patching it trips Arxan (also 51), and the me3 plus mutex-escape route
  still ends with the guest exiting. A join would also need a second Steam identity.
- Not run: host, join, enemy/story sync, desync, area transitions. Menu automation is
  blocked because the game reads raw input and ignores synthetic keys.

## How to play

1. Own Sekiro on Steam and install Sekiro Online v2.05 from Nexus (or use the installer in
   `games/sekiro/revive-package/`).
2. Both players launch through Steam, load a save, and be past the tutorial once.
3. Host uses the **Cursed Emblem**, then invites the friend through the Steam friends list.
4. Friend accepts the invite and appears in the host's world.

## Known issues

- Public summon signs and random invasions are dead with the matchmaking server gone;
  direct Steam invites are the intended workaround.
- Sessions drop on area change.
- The online items are added to your save; back it up before installing or removing the mod.
- Upstream reports remain open: startup crashes on 1.06 + 2.05, a Cursed Emblem crash,
  Lady Butterfly's door closed for phantoms, Guardian Ape not dying or not dropping the
  Finger Whistle, Floating Passage showing as sold out.

## Next steps

- Prove or disprove Steam-invite co-op with a real second client: run `coop-test/run.ps1`
  on each PC, host with the Cursed Emblem, invite, then `report.py host.jsonl guest.jsonl`.
  That single test decides whether this revive is playable.
- Capture the summon-sign protocol and either reimplement the server or redirect the
  hardcoded endpoint to a community one; the address is a plain ASCII string in the DLL.
- Reproduce the post-force-kill startup crash in a controlled loop.
- Turn the boot and join checks into a bot-driven suite once input can be delivered (raw
  input needs a virtual gamepad or an interception driver, not `keybd_event`).
