# Warhammer 40,000: Dawn of War - Soulstorm (2008)

| | |
|---|---|
| Engine | Relic Essence Engine (EE), the same codebase ModDB files under "Impossible Creatures Engine"; x86 only (`PE machine 0x014C`) |
| Versions tested | Retail non-Steam **patch v1.2** (Oct 2008, from `Release_Notes.txt`), `Soulstorm.exe` file version 1.4.0.0, SHA-256 `9d8f57251dbc10198bbb8de337df42e7cdb4b5b77e3bd8c1f0776b4aee741b2d`, Russian localization. Steam build 1.3 was **not** tested. |
| Projects | [DowOnline](https://dowonline.ru/) (directory entry `dowonline`); no campaign co-op project exists |
| QA level | community-reports |
| Last updated | 2026-10-23 |

## Where things stand

**There is no story or campaign co-op for Soulstorm, and no project is trying to add one.** The
campaign is a single-player conquest metamap and the engine has no co-op path for it: the executable
contains no `coop`/`cooperative` string and no campaign-in-multiplayer handler.

What people call "co-op" in this game is **skirmish against AI** (comp-stomp) in the multiplayer
lobby: humans on one team, computer players filling the rest. That mode is **native, needs no mod and
no server, and works on a LAN today**. It is also what the live DowOnline games show: matches on
their public state endpoint contain both human accounts and slots named `Компьютер 1`..`Компьютер 6`.

DowOnline exists for the case a LAN cannot serve: playing **over the internet**, and matches larger
than the stock lobby allows. It emulates a LAN adapter and relays the game's own UDP traffic, so it
does not change what is synced, only where the packets go.

## Existing work

Searched 2026-10-23: this repo, GitHub (repos and code), ModDB, the DowOnline site, DowOnline's own
public API. Not checked: Nexus Mods, the DowOnline Discord, Russian forums, archive.org.

- **[DowOnline](https://dowonline.ru/)** - the only co-op-adjacent project for Soulstorm. A closed
  source .NET client that injects a hook DLL into `Soulstorm.exe`, hooks the socket layer and the
  UI, and connects the game to a VLAN rendezvous server so its LAN lobby spans the internet. It also
  ships a map pack with 2-8 player maps and an MMR/stats service. Client 3.3.0. The main source is
  **not public**; the only public artefact is the maps repo,
  [Drobor/dowonline-maps](https://github.com/Drobor/dowonline-maps). Authors named on the site:
  Drobor, SpeedRay, Кунжутина, Elamaunt, Anibus, TranspersonalPsy, Willweeverwin.
- **No ModDB co-op mod.** The 29 entries under `dawn-of-war-soulstorm/mods` are single-player
  campaigns, overhauls, UI fixes and total conversions (Ultimate Apocalypse, Doomsday Unlimited,
  Unification, Dark Prophecy, ...). None adds co-op.
- **No GitHub co-op project.** Repository and code searches for `dowonline`, `soulstorm coop`,
  `"dawn of war" multiplayer` return only `Drobor/dowonline-maps`.

## How the game works (for co-op)

- **Install layout:** `Soulstorm.exe` plus Relic module files (`W40k.module`, `DXP2.module`), data in
  `Engine/Engine.sga` and `W40k/*.sga`. Scripts are Lua (`LuaConfig.dll`, `luabind.dll`).
  Networking lives in `NetConfig.dll`, `Platform.dll` (the socket layer DowOnline hooks) and
  `GSLobby.dll` (GameSpy).
- **LAN is a separate code path from GameSpy and it still works.** The running game binds
  **UDP 6112 on `0.0.0.0`** (verified with `Get-NetUDPEndpoint` while the process was alive). LAN
  discovery is broadcast; no master server is involved.
- **Vanilla internet play is permanently dead.** `W40k.ini` configures GameSpy only, and every
  endpoint it points at is gone: `thq.vo.llnw.net` (Limelight) and the GameSpy master servers, which
  shut down in 2014. That is the hole DowOnline fills.
- **The lobby rejects mismatched peers itself.** The executable contains
  `LOBBY -- Rejecting connection (version)`, `(net version) (%d) (%d)`, `(crc)`, `(dev)`, `(online)`
  and `(sizeof)`. So a host and a guest whose builds or data differ are refused at connect time,
  which a player experiences as "the lobby is there but the join fails". This is the single most
  likely failure for two people with differently-sourced copies.
- **UI screens present:** `MultiplayerMenu`, `LanGameSetup`, `LanJoinMenu`, `SkirmishGameSetup`.
- **Tuning** is in `W40k.ini` `[global]`: `connectTimeout = 30000`,
  `waitingForResponseTimeout = 20000`, `keepAliveSendRate = 1000`, `pingRate = 15000`,
  `lagBeforeShow = 2`, `lagBeforeKick = 45`.
- **Profile / identity:** `Local.ini` names the profile and the current mod (`dxp2` = Soulstorm);
  `Profiles/<name>/` holds `name.dat` and `playercfg.lua`. The retail build logs
  `AppRegistry::GetValue - Failed to read valueName CDKEY`, i.e. no CD key is registered, and LAN
  does not ask for one.

## What we changed

Nothing in the game yet. This entry is research plus a directory record, which [AGENTS.md](../../AGENTS.md)
allows as a standalone contribution.

What was actually established and worth keeping:

- **The LAN port is UDP 6112**, measured, not guessed. A firewall rule for `Soulstorm.exe` on UDP
  6112 (inbound) is the only network change a LAN game needs.
- **The LAN path needs no mod.** For two players on one network, DowOnline is not involved, so its
  rendezvous server cannot break their game. Advice to "install the mod to play together" is wrong
  for this case.
- **Mismatched copies are rejected by the engine**, not by the network. Both installs must be the
  same build and the same content; a differently-sourced copy changes `Soulstorm.exe` and/or the
  `.sga` data and is refused with `(version)` or `(crc)`.
- **DowOnline is alive, and its advertised relay is not stale.** Its public API
  (`GET /api/VLANAddress?gameId=1`) returns `161.35.29.36:11168`, the same value the client caches,
  and `GET /api/onlinestate` lists concurrent matches and roughly 50 online accounts. So the earlier
  "the service is down" reading of a local failure was wrong: the client's 2026-10-20 log shows
  `SocketException 10013` (access denied on an outbound connect), `11001` (DNS) and `10054`
  (connection reset), which are local blocking or local network faults, not a dead server.
- **DowOnline's install detection is fragile.** On this machine the client threw
  `IndexOutOfRangeException` while "getting supported dow directory" and the install path had to be
  typed into `options.txt` by hand. The path here contains a non-ASCII dash (U+2013) and sits in a
  non-default root, so either can be the trigger; the cause is not proven. Worth reporting to the
  authors, who have no public issue tracker.

## QA

Honest level: **community-reports**. Nothing here was verified by two humans playing, and no
automated two-client test was run.

What was checked, and how:

| Claim | How it was checked |
|---|---|
| The game binds UDP 6112 | Launched `Soulstorm.exe` from an isolated copy, then `Get-NetUDPEndpoint` filtered by its PID: one endpoint, `0.0.0.0:6112` |
| LAN UI exists | String scan of `Soulstorm.exe`: `LanGameSetup`, `LanJoinMenu`, `MultiplayerMenu`, `SkirmishGameSetup` |
| The engine rejects mismatched peers | String scan: `LOBBY -- Rejecting connection (version)`, `(crc)`, `(net version)`, `(dev)`, `(online)`, `(sizeof)` |
| Vanilla internet play is dead | `W40k.ini` points only at `thq.vo.llnwd.net` and GameSpy; both are gone |
| No campaign co-op | No `coop`/`cooperative`/campaign-multiplayer string in the executable; the campaign is a single-player metamap |
| DowOnline serves live games | `GET https://dowonline.ru/api/onlinestate` returned 200 with 4 concurrent matches and ~50 `onlinePlayers` |
| DowOnline's relay address is current | `GET https://dowonline.ru/api/VLANAddress?gameId=1` returned `161.35.29.36:11168`, equal to the value in the client's `options.txt` |
| The relay is not refusing us | UDP datagram to `161.35.29.36:11168` timed out rather than returning ICMP port-unreachable, consistent with an open or filtered port (not proof) |
| The hook really injects | `DowOnline.HookContent20261020.log`: injected into two PIDs, hooked `NativeUI`/`WinApi`/`LuaConfig`/`Platform.dll`, intercepted socket creation |

Not done: **no two-client test.** A single PC cannot prove a LAN match joins and stays in sync, and
the repo's `tools/gameslot`/`gamepool` harness was not wired up for this game. Treat "it will work
for two players tonight" as untested until a real pair of machines tries it.

## How to play

**Two players, one LAN, no mod (the case that matters here).**

1. Both installs must be **the same build and the same content**. Compare the SHA-256 of
   `Soulstorm.exe` on both PCs; if they differ, fix that before anything else. Remove or match any
   installed mods, because the lobby also rejects on a data `crc`.
2. Allow `Soulstorm.exe` through Windows Firewall on **UDP 6112 inbound** on the host (Windows
   prompts on first launch if it is not already allowed).
3. Host: Multiplayer -> LAN -> create a game, pick a map and teams, add computer players to the
   empty slots. Guest: Multiplayer -> LAN -> the host's game appears in the list -> join.
4. Same subnet, and the network profile must be Private; Windows blocks discovery on Public.

**Internet, or larger matches:** install [DowOnline](https://dowonline.ru/) on every PC, launch
the game through it, and use the game's LAN lobby as above. The client points the game at the
project's relay (`161.35.29.36:11168` as of 2026-10-23) and adds its 2-8 player map pack. Every
player needs their own legitimate copy of the game.

## Known issues

- **No campaign co-op, and no project adding it.** The metamap campaign is single-player only.
- **Version/crc mismatch is rejected at connect time** with a generic failure, so "lobby visible but
  cannot join" almost always means the two installs are not identical.
- **Vanilla internet multiplayer does not work** and cannot be made to work; the GameSpy backend is
  gone. Only LAN and DowOnline's relay are live.
- **DowOnline is closed source and single-operator.** Its relay address comes from its own API, so
  if the operator moves or stops it, internet play stops. There is no fallback.
- **DowOnline's install auto-detection can throw** instead of asking for the folder, and its log
  cannot be filed anywhere public.
- The retail build logs a missing `CDKEY` registry value. LAN does not need it; whether any
  DowOnline path does is unknown.
- **Two copies cannot run on one PC.** The game refuses a second instance (`PLATFORM -- Duplicate
  instance of app`, exits in about 0.2 s), and it holds UDP 6112 exclusively, so a second client
  could not open its LAN socket even if the check were bypassed. Test with two machines; on a
  single PC only a virtual machine with its own network stack can act as the second player.

## Next steps

- Run a real two-client LAN test **on two machines** and raise the QA level. The repo's
  `gameslot`/`gamepool` harness cannot stand in for the second machine here: this game refuses a
  second instance and binds UDP 6112 exclusively, so the guest has to be a VM or a second PC.
  Assert: guest appears in the host's lobby, the match starts on both, AI slots are present, and
  the game survives five minutes without a disconnect.
- Capture what the `(version)` and `(crc)` checks actually hash, so the error can be turned into a
  clear message ("your build differs") instead of a silent refusal.
- If campaign co-op is ever wanted, this is a from-zero job on the Essence Engine: the metamap
  campaign and the skirmish lobby are different code paths, and only the skirmish one is
  network-aware.
- Report the `ssloc` auto-detection `IndexOutOfRangeException` to the DowOnline authors.
