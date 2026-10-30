# Slime Rancher 2 (2022)

| | |
|---|---|
| Engine | Unity 6000.0.73f1, IL2CPP (MelonLoader mods) |
| Versions tested | Steam, game 1.3.0 (`1.3.0 [20260704030000-113492]`), build id 24131940, Windows 11 |
| Projects | [Ranching Together (SR2MP)](https://github.com/pyeight/SlimeRancher2Multiplayer), directory entry "Slime Rancher 2 / Ranching Together" |
| QA level | automated (smoke + money/player sync, 2 clients on one PC; see QA) |
| Last updated | 2026-10-23 |

## Where things stand

Yes, people can play together today. Ranching Together is an active open-source (GPL-3.0) MelonLoader mod. In its
own feature table almost every system is synced; the exceptions are the player inventory and parts of the Gray
Labyrinth and the Sanctuary. On 1.3.0 it boots, hosts and joins, and money and player sync passed an automated test
with two real game clients. The internet path (port forwarding, UPnP) has not been tested here.

## Existing work

| Project | Last activity | Status | Notes |
|---|---|---|---|
| [Ranching Together / SR2MP by pyeight (Shark)](https://github.com/pyeight/SlimeRancher2Multiplayer), also [Nexus 118](https://www.nexusmods.com/slimerancher2/mods/118) | release Beta_0.3.9, 2026-09-24; push 2026-09-26 | active | Needs game version exactly 1.3.0 (`BuildInfoConstants.cs`), MelonLoader 0.7.3, Starlight. Contributors list PinkTarr (author of the older NewSR2MP, Nexus 90). |
| [SR2MP by Saty](https://satypardus.itch.io/multiplayer-mod-slime-rancher-2) | update 521, 2024-02-10 | discontinued | Works only on SR2 0.4.0 per its itch page; the author dropped it because of IL2CPP. |
| [Egor935/SR2MP](https://github.com/Egor935/SR2MP) | v0.1.6 2025-02-10 (v0.1.7 listed) | unknown | Targets old game versions per its release notes. Not tested. |

## How the game works (for co-op)

- No netcode in the game. All sync is in the mod: UDP, host authority, default port 1919, IPv6 dual-mode socket.
  The "join code" is the host's IP and port encoded (`Shared/Utils/JoinCode.cs`). Auto-host tries UPnP through
  Open.NAT (`Shared/Utils/AutoHost.cs`). There is no Steam relay, so the host needs UPnP, a forwarded UDP port, or a VPN.
- Saves: `Application.persistentDataPath\Steam\<steamid64>\*.sav`, under `%USERPROFILE%\AppData\LocalLow\MonomiPark\SlimeRancher2`,
  synced by Steam Auto-Cloud. Joining replaces the client's save with the host's world. The mod backs up saves to
  `UserData/SR2MP/SaveBackups/`.
- The game ships CLI options (`Il2CppMonomiPark.SlimeRancher.CliOptions`: `SaveFileToLoad`, `QATestFilter`,
  `ProfileScenario*`). Setting `SaveFileToLoad` to a full `.sav` path makes the game load it at boot, which is handy for bots.
- SR2MP player id = SHA-256(machine name + Windows user name), so two copies on one PC share an id. The test bot overrides it.
- The SR2MP client refuses `127.0.0.1` unless the mod's DevMode is on. Tests on one PC use the LAN IP.

## What we changed

Nothing in the mod. Test-only tools were added in `qa/` (inert without their flag files):

- `qa/SaveIsolation`: MelonLoader plugin. It patches the `SHGetKnownFolderPath` import of `UnityPlayer.dll` and
  `GameAssembly.dll` so that FOLDERID_LocalAppDataLow points to `<instance>\LocalLow`. Test copies then never touch the
  the real saves or the Steam Cloud folder. It works for any Unity game on Windows that keeps saves in
  `persistentDataPath`, see [multi-instance-testing](../../playbook/multi-instance-testing.md).
- `qa/Bot`: MelonLoader mod. It loads a save at boot through `CliOptions.SaveFileToLoad` and gives each copy its own
  SR2MP player id. Every 0.5 s it writes `UserData/CoopQA/state.json` (money, position, server and client state,
  remote players) and runs numbered commands from `UserData/CoopQA/cmd.txt` (`host`, `connect`, `addcurrency`,
  `save`, `quit`).
- `qa/run_coop_test.py`: runner built on `tools/gamepool`. It starts host and client copies, checks state, prints
  `RESULT: PASS/FAIL`, and hashes the real install and save folder before and after.

Bugs found upstream (to report to the mod):
1. `Server.Start(port, enableIPv6: false)` creates an IPv4 socket, but `Server/Managers/NetworkManager.cs:76`
   always passes an IPv6 EndPoint to `ReceiveFrom`. The result is an exception in a tight loop with no backoff: 6.1 M
   log entries, 4.4 GB of log in 6 minutes. In-mod callers all pass `true`, so players are not affected today.
2. On join the client logged `InitialLandPlotsPacket threw: IndexOutOfRangeException` at
   `AmmoModel.Reset ← SiloStorage.SetModel ← InitialLandPlotsHandler.Handle` (`InitialLandPlots.cs:127`). The host save
   had silos and the client save was an early game. Whether the remaining plots in that packet were skipped: unknown.

## QA

- Setup: copy the install, build hard-linked instances with `tools/gamepool` (config like `qa/sr2.example.json`), and
  install MelonLoader 0.7.3, Starlight 4.0.4, SR2MP 0.3.9, `CoopQA.SaveIsolation.dll` (into `Plugins/`) and
  `CoopQA.Bot.dll` (into `Mods/`) into each instance. Then:
  `python -I games/slime-rancher-2/qa/run_coop_test.py --host-ip <LAN IPv4>` (private paths, Steam id and save names
  go in `local/sr2-qa.json`).
- Asserts: both copies load their own save; the saves differ before connect; the server runs; the client connects;
  the host counts 1 client; each side sees the other's player; the client's money equals the host's after join; host
  +777 reaches the client; client +111 reaches the host; each side's view of the other player is within 2 m (players
  standing still, so a weak check); real install and saves unchanged.
- 2026-10-23, game 1.3.0, SR2MP 0.3.9, Starlight 4.0.4, MelonLoader 0.7.3, one PC, 2 clients:

  ```
  RESULT: PASS  (16/16 checks)
  ```

  An earlier run with a bot bug (server started with `enableIPv6=false`, which never answers) failed 8/14, so the
  connect and sync checks can fail.
- Not covered: internet, NAT or UPnP, movement, slimes, plots, gadgets, drones, client saving, 3+ players, lag and loss.

## How to play

1. Every player: install [MelonLoader 0.7.3](https://github.com/LavaGang/MelonLoader/releases/tag/v0.7.3) on Slime
   Rancher 2. Put [Starlight 4.0.4](https://github.com/ThatFinn/Starlight/releases/tag/v4.0.4) `Starlight.dll` and
   [SR2MP 0.3.9](https://github.com/pyeight/SlimeRancher2Multiplayer/releases/tag/Beta_0.3.9) `SR2MP.dll` into `Mods`.
   The mod README says Starlight 4.0.3, but SR2MP 0.3.9 was built against 4.0.4. Everyone needs the same versions.
2. Host: load the world you want to share and open the multiplayer menu. Auto-host (UPnP) or forward UDP 1919, then
   send the join code. Without UPnP or port forwarding, use a VPN such as Tailscale, ZeroTier or Radmin VPN.
3. Friends: load an empty or new save slot (joining overwrites it), then enter the code.

## Known issues

- The player inventory is not synced; rejoin using the same save (mod README).
- Gray Labyrinth, the Sanctuary and some slime behaviours are only partly synced (mod README).
- One error on join while copying silo contents (see above). Some silos may not match; not verified.
- No Steam relay: the host needs UPnP, a forwarded port or a VPN.

## Next steps

- Report the two upstream bugs with the stack traces above.
- Extend the bot: move a player, vacuum and shoot slimes, plant a garden, check silo contents on both sides (bug 2),
  save on the client, run lag and loss through `tools/chaos`.
- A Steam relay transport (Spacewar, see [networking-steam-spacewar](../../playbook/networking-steam-spacewar.md))
  would remove port forwarding; it would have to be proposed to the mod author.
