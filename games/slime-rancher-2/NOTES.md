# Slime Rancher 2 — notes

## Identity (tested build)
- Steam, buildid 24131940, game version 1.3.0, Unity 6000.0.73f1 IL2CPP, Windows 11
- SHA-256 SlimeRancher2.exe: fb186f4b9d02a47817a0c1d8e7581d7b9a6b77d65e034355f39dae138e61fc28
- SHA-256 GameAssembly.dll: 786006ede0d198f6e15a2c01f90ef9108afe694745fa0ad894e1c3ed342385b7

## Triage 2026-10-23
### Observations (sourced)
- No entry in DIRECTORY.md / data/directory.yaml / games/ for Slime Rancher 1 or 2; no hub issues found via GitHub search API.
- **Ranching Together (SR2MP)** — https://github.com/pyeight/SlimeRancher2Multiplayer — GPL-3.0, active
  (last push 2026-09-26), latest release Beta_0.3.9 (2026-09-24), asset SR2MP.dll
  sha256 9cf2823c635e0d4e515828b14a50e8b42442f45bfb31e175f6a80984f437ea28 (GitHub digest). 3 open issues (cosmetic).
  - `SR2MP/BuildInfoConstants.cs`: MinimumGameVersion = ExactGameVersion = "1.3.0" → matches this install.
  - `SR2MP/AssemblyInfo.cs`: VerifyLoaderVersion(0,7,3) → MelonLoader 0.7.3. README: Starlight 4.0.3 required.
  - Networking: direct UDP, default port 1919. Join code = encoded IP:port (`Shared/Utils/JoinCode.cs`).
    UPnP auto-host via Open.NAT (`Shared/Utils/AutoHost.cs`). No Steam relay.
  - README feature table: most systems synced; partial: Gray Labyrinth, Sanctuary, some slime behaviours;
    not synced: player inventory. Joining OVERWRITES the client's save → join with an empty save slot.
- Older/dead: Saty's SR2MP (itch.io, discontinued, SR2 0.4.0 only); Egor935/SR2MP (last release v0.1.7, old game versions).
  NewSR2MP by PinkTarrLargo (Nexus 90) — status unknown; Ranching Together appears to be its successor (same Discord invite a7wfBw5feU) — derived, unverified.

### Derived
- Path 1 (existing working mod). No co-op needs to be built; the job is install + verify + maybe Steam-relay/no-port-forward improvement.

### Unknown
- Whether owner's router supports UPnP.

## Workspace + QA 2026-10-23
### Observations
- MelonLoader log: `Game Version: 1.3.0 [20260704030000-113492]`, Unity 6000.0.73f1. ML 0.7.3 x64 zip
  (sha256 5b2b2f3d…a416) installed manually; generates interop assemblies on first run (~30 s).
- SR2MP.dll 0.3.9 contains the string "4.0.4" (MinimumStarlightVersion = Starlight.BuildInfo.CodeVersion at build time).
  Starlight 4.0.4 (released 2026-09-24, same hour as SR2MP 0.3.9) loads fine. README still says 4.0.3 → README outdated.
  Not tested whether 4.0.3 is rejected.
- Saves: `Application.persistentDataPath` = `%USERPROFILE%\AppData\LocalLow\MonomiPark\SlimeRancher2\Steam\<steamid64>\`,
  which is a Steam Auto-Cloud folder (`steam_autocloud.vdf`). A test copy would write the owner's saves and cloud.
  Fix used: test-only ML plugin `CoopQA.SaveIsolation` patches the `SHGetKnownFolderPath` import of UnityPlayer.dll and
  GameAssembly.dll (both import it) and answers FOLDERID_LocalAppDataLow with `<instance>\LocalLow`. Runs in
  OnPreInitialization; that is early enough (Unity resolved LocalLow once, after the patch: `redirected=1`).
  Player.log redirected with `-logFile`. Owner's save folder + install hashed before/after every run: unchanged.
- `steam_appid.txt` (1657630) in the copy is enough: no relaunch through Steam, SteamAPI works, a second copy starts
  (no single-instance check). Main menu ~11 s; RAM ~2.4 GB at menu, ~3.3 GB in world per copy.
- Game has CLI options class `Il2CppMonomiPark.SlimeRancher.CliOptions` (`SaveFileToLoad`, `QATestFilter`,
  `ProfileScenario*`). Setting `CliOptions.SaveFileToLoad = <full path to .sav>` (postfix on `CliOptions.Initialize`)
  makes the game load that save at boot, skipping the menu. Used by the test bot.
- SR2MP player id = SHA-256(MachineName + UserName)[..9] (`Shared/Utils/PlayerIdGenerator.cs`): two copies on one PC
  get the same id. Bot sets `PlayerIdGenerator.cachedPlayerId` per copy (test only).
- SR2MP client refuses 127.0.0.1 unless DevMode (`Client/Client.cs:117`); tests use the LAN IP.
- **Upstream bug (does not affect players):** `Server.Start(port, enableIPv6: false)` → IPv4 socket, but
  `Server/Managers/NetworkManager.cs:76` always uses an IPv6Any EndPoint for `ReceiveFrom` → ArgumentException in a
  tight loop without backoff (6.1 M log entries, 4.4 GB Latest.log in 6 min). All in-mod callers pass `true`
  (`Commands.cs:26`, `MultiplayerUI.Action.cs:14`). Found because the first bot version passed `false`.
- **Upstream bug (affects players, impact unknown):** on join the client logged once
  `Packet handler for InitialLandPlotsPacket threw: IndexOutOfRangeException at AmmoModel.Reset ← SiloStorage.SetModel
  ← InitialLandPlotsHandler.Handle (InitialLandPlots.cs:127)`. Host save had silos; client save was an early game.
  Whether later land plots in that packet were skipped: unknown (not checked).

### QA run 2026-10-23 (local/qa/run_coop_test.py, two real clients on one PC, LAN IP)
- Run 1: FAIL 8/14 (bot bug above: server never answered) → shows the connect/sync checks can fail.
- Run 2: **PASS 16/16**: both copies boot and load their save via CliOptions; saves differ before connect
  (3183 vs 250 newbucks); server running; client connected; host counts 1 client; each side sees the other's player;
  client money = host money after join (3183); host +777 → client 3960; client +111 → host 4071;
  remote player positions within 2 m (d=0.00, players standing still: weak check); owner saves + install unchanged.
- Not tested: internet / NAT / UPnP, movement, slimes, plots, gadgets, saving on the client, more than 2 players,
  lag/loss.
