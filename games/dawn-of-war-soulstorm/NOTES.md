# Dawn of War: Soulstorm - research notes

Date: 2026-10-23. Owner's private state (logs, dumps, install paths) is in `local/`, which is
git-ignored.

Goal as stated by the owner: play Soulstorm in co-op with one friend, both on the same LAN, on
non-Steam copies.

## Observations

### The game

- **Warhammer 40,000: Dawn of War - Soulstorm**, Relic, published by THQ, 2008. Russian localization.
- `Soulstorm.exe`: file version **1.4.0.0**, `CompanyName = THQ Canada Inc.`,
  `ProductName = Warhammer 40 000: Dawn of War`, SHA-256
  `9d8f57251dbc10198bbb8de337df42e7cdb4b5b77e3bd8c1f0776b4aee741b2d`.
- **PE machine `0x014C` (x86)**, so any injected code must be 32-bit.
- **Game patch level v1.2, October 2008**, read from `Release_Notes.txt` in the install root. This is
  not a Steam install: no DoW app id in either Steam library, and the folder carries Inno Setup
  uninstaller files (`unins000.exe`, `unins000.dat`).
- **Engine:** Relic Essence Engine. Evidence: Relic module files `W40k.module` and `DXP2.module`,
  `Engine/Engine.sga`, `SimEngine.dll`, `UserInterface.dll`, `LuaConfig.dll` + `luabind.dll`,
  `Platform.dll`, `seInterface.dll`, `WXPMod.dll`. ModDB files this codebase under "Impossible
  Creatures Engine", which is the same engine under its earlier marketing name.
- Install size on this machine: 210 files, 5,613,177,954 bytes.

### Native networking

- **The running game binds UDP 6112 on `0.0.0.0`.** Measured: launched the game from an isolated
  copy, waited 25 s, then `Get-NetUDPEndpoint | Where OwningProcess -eq <pid>` returned exactly one
  endpoint, `0.0.0.0:6112`. No TCP endpoint at all.
- **LAN UI screens exist in the executable:** `MultiplayerMenu`, `LanGameSetup`, `LanJoinMenu`,
  `SkirmishGameSetup`.
- **The lobby validates peers itself.** Strings in `Soulstorm.exe`:
  `LOBBY -- Rejecting connection (version)`, `LOBBY -- Rejecting connection (net version) (%d) (%d)`,
  `LOBBY -- Rejecting connection (crc)`, `(dev)`, `(online)`, `(sizeof)`,
  `Lobby -- hosting on port %d`, `Lobby - Join success %x`, `Lobby -- Host migrated`.
  So build and content mismatches are refused at connect time by the engine.
- **Vanilla internet play is dead.** `W40k.ini` `[global]` configures GameSpy only:
  `automatchUrl = http://thq.vo.llnwd.net/o10/DoW/automatch/AutomatchDefaultsSS.lua`,
  `roomsurlDXP2 = http://thq.vo.llnwd.net/o10/DoW/lobby/LobbyRooms.lua`,
  `newsunicodeurlDXP2 = http://thq.vo.llnwd.net/o10/DoW/news/dow_news.txt`,
  `statsurlDXP2 = http://www.dawnofwargame.com/SS_StatsPage/homepage.php.htm`.
  Limelight (`llnw.net`) is gone and the GameSpy master servers closed in 2014. `GSLobby.dll` is the
  only online backend the game knows.
- Network tuning keys in `W40k.ini`: `connectTimeout = 30000`, `waitingForResponseTimeout = 20000`,
  `keepAliveSendRate = 1000`, `pingRate = 15000`, `enumerationUpdateTime = 1200`,
  `lagBeforeShow = 2`, `lagBeforeKick = 45`.
- `Local.ini` names the profile and the active mod (`currentmoddc = dxp2`).
  `Profiles/<name>/` holds `name.dat`, `playercfg.lua` and `dxp2/KEYDEFAULTS.LUA`.
- The retail build logs `AppRegistry::GetValue - Failed to read valueName CDKEY` twice at startup.
  LAN does not ask for a key.

### No campaign co-op

- No `coop`, `co-operative` or campaign-in-multiplayer identifier in `Soulstorm.exe`.
- The campaign is the single-player metamap (`METAMAP.gfx`, `CAMPAIGNNUGGET.gfx`,
  `COMMANDERSCREEN.gfx` are campaign screens; the multiplayer path uses `SkirmishGameSetup`).
- Every live match visible on DowOnline's public state is a skirmish: humans plus slots named
  `Компьютер 1`..`Компьютер 6`.

### DowOnline (the only co-op-adjacent project)

- Site: <https://dowonline.ru/> (EN `/en/`, RU `/ru/`) and <https://dowonline.org/> (same app).
  Discord: <https://discord.gg/n72AvaTAfN>. Downloads:
  `/clientBin/DowOnline.Installer.exe`, `/clientBin/DowOnlineLatest.zip`.
- Stated design: "connectivity with other players anywhere in the world by **local network
  emulation**". The in-game step is "create - join game in **LAN lobby**".
- **Client version 3.3.0.** `DowOnline.exe` ProductVersion
  `1.0.0+34e0fcdece3f4b36cdbe140496d6b4d434772103` (a git SHA, so built from a private repo).
- Implementation: .NET, self-contained (`coreclr.dll`, `hostfxr.dll`), Avalonia UI,
  `Reloaded.Injector` + `Reloaded.Hooks` (native x86 hooks), `DotnetInject.Injector` (CLR injection),
  FASM (asm patching), `MessagePack` + `Communication.dll` (RPC), `DowOnline.Api.Server.dll` +
  `DowOnline.Api.VLANServer.dll`, Serilog. Log shipping to `http://log.dowonline.org:5341`.
- It installs beside the game (`<root>\DowOnline`) and leaves `DllTie.dll` plus
  `dowonlineSettings.json` in the game folder.
- **Source:** only the maps are public, <https://github.com/Drobor/dowonline-maps> (2p..8p map
  folders, `tags.json`, `DowOnline.Maps.DescriptionEditor.exe`). The client and server are closed.

### DowOnline's public API (read directly, 2026-10-23)

Derived from UTF-16 string extraction over `DowOnline.Api.Server.dll`:

| Route | Note |
|---|---|
| `api/VLANAddress?gameId=` | Returns the relay address as plain text |
| `api/OnlineState` | Live matches and players |
| `api/Mods/list?`, `api/Mods/GetModDescription` | Mod catalogue |
| `api/Maps/list?` | Map catalogue |
| `api/Stats`, `api/Stats/list?` | MMR / statistics |
| `api/Account/LoginByHwid` | Account login |
| `api/chat/GetLatestMessages`, `api/chat/SendMessage` | Chat |
| `api/Ban?` | Bans |
| `api/GetStorageFiles` | POST-only bulk file fetch; 405 on GET, 500 on an empty body |

Measured responses:

- `GET /api/VLANAddress?gameId=1` (also `20`, `0`) -> `200`, body `161.35.29.36:11168`. The client's
  cached `vlanserveripep=161.35.29.36:11168` is therefore **current, not stale**.
- `GET /api/onlinestate` -> `200`, with `gamesInProcess` (4 concurrent matches, 6-8 players each,
  mods `dxp2`, `doomsday_unlimited`, `unification_unlimited`), ~50 `onlinePlayers`, and
  `lastMapsUpdateTime = 639273345297401456`. The client's `dowonlineSettings.json`
  `LastInstalledMapsUpdateTime` holds the same number.
- `GET /api/mods/modfamilies` -> 182 families, id 1 `Dawn of War - Soulstorm`, id 20 `dowonline`,
  id 49 `ultimateapocalypse`, id 141 `unification`.
- `GET /api/onlinestate` also shows `"version":3148800`, which is `3.3.0` packed - matching the client.

### What the 2026-10-20 client log actually says

The client was installed and pointed at the game by hand on 2026-10-20. Its log contains:

| Error | Code | Reading |
|---|---|---|
| `HttpRequestException ... (dowonline.ru:443)` | `SocketException 10013` | WSAEACCES: outbound connect denied, i.e. local block |
| same host, later | `SocketException 11001` | WSAHOST_NOT_FOUND: DNS failure |
| same host | `SocketException 10054` | WSAECONNRESET |
| chat long-poll | `SocketException 10053` | WSAECONNABORTED |
| `Failed to acquire server udp address` | (no inner stack) | the `api/VLANAddress` fetch failed for the same reason |
| `DEFAULT MAPS UPDATE: ERROR - retries count exceeded` at `ModificationFilesDownloader.DownloadFilesFromServer` | - | map pack download gave up after the network failures |

Also from that session, in `DowOnline.Client20261020.log`:
`Error while getting supported dow directory - IndexOutOfRangeException`. The install path had to be
typed into `%APPDATA%\DowOnline\options.txt` as `ssloc=` before the hook worked. The path contains
U+2013 (en dash) instead of a hyphen and lives in a non-default root, not under Program Files.

`DowOnline.HookContent20261020.log` proves the hook works once the path is right: it injected into
two game PIDs, initialised `NativeUI`/`WinApi`/`LuaConfig`/`Platform.dll` hooks, intercepted socket
creation, and connected to `161.35.29.36:11168`.

## Derived

- **For two players on one LAN, DowOnline is unnecessary.** Their game needs no server, no mod and
  no internet; the native LAN lobby does it. The relay only matters for internet play, matches
  larger than the stock lobby, and the custom map pack.
- **The failure they will actually hit is a build/content mismatch**, refused by
  `LOBBY -- Rejecting connection (version)` or `(crc)`. That is why "the lobby is visible but the
  join fails" is the classic symptom.
- **DowOnline's earlier "outage" was local.** Its API serves live games and returns the same relay
  address the client caches; the client's own errors were `10013`/`11001`/`10054`, which are local
  blocking and DNS faults. The relay's UDP port also did not answer with ICMP port-unreachable,
  which is what a closed port would do.
- **The en dash cannot break LAN play**, only DowOnline's install detection.

## Guesses (explicitly unverified)

- That the `IndexOutOfRangeException` in install detection is caused by the U+2013 in the folder
  name rather than by the non-default install root. The exception type points at index access,
  not at a failed string compare, so the root cause may be different.
- That `(crc)` compares the loaded `.sga` content and `(version)`/`(net version)` compares the exe
  build and the protocol version. The strings exist; which input goes to which check was not
  traced.
- That the 4-player cap in the retail LAN lobby is what DowOnline's 2p-8p map pack works around.
  The maps repo implies up to 8, but the cap itself was not measured.

## Unknowns

- Whether two differently-sourced but same-patch copies actually pass each other's `(crc)` check.
  Not tested.
- What the maximum player count in a stock LAN lobby is.
- Whether DowOnline's relay is reachable from a network that is not this one, and whether its UDP
  port is open or merely filtered here.
- Whether any DowOnline path needs the missing `CDKEY` registry value.
- Whether the map pack is needed for stock maps (it should not be, but was not tested).
- Whether the Steam 1.3 build interoperates with retail 1.2. Untested and unlikely.

## Searched

- This repo: `data/directory.yaml` (7730 lines) and `games/` - no Dawn of War, Soulstorm or
  Warhammer entry. Public issue and PR search for the same terms - nothing.
- GitHub repository and code search: `dowonline`, `soulstorm coop`, `"dawn of war" multiplayer` -
  only `Drobor/dowonline-maps`.
- GitHub user `Drobor`: 15 public repos, only the maps repo plus forks of Reloaded/CoreRPC.
- ModDB `dawn-of-war-soulstorm/mods`: all 29 entries are single-player content or total conversions.
- `dowonline.ru` (EN and RU), `dowonline.org`, and its API routes listed above.
- Local binaries: ASCII and UTF-16 string extraction over `Soulstorm.exe`, `NetConfig.dll`,
  `Platform.dll`, `GSLobby.dll`, `DowOnline.ClientCore.dll`, `DowOnline.Api.Server.dll`,
  `DowOnline.Api.VLANServer.dll`.
- Not checked: Nexus Mods, the DowOnline Discord (the owner declined to contact the authors), Russian
  forums, archive.org, the `dowstats.ru` rating server, the maps repo's `tags.json` in detail.
