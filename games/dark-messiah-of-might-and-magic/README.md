# Dark Messiah of Might and Magic (2006)

| | |
|---|---|
| Engine | Source (Arkane's 2006 branch) |
| Versions tested | Steam appid 2100, buildid 49752, `mm.exe` SHA-256 `f8faaee53a2c9ed72b8c7b83b75608f96a77ab07f374314950701901bda7e34c` |
| Projects | [wiltOS Dark Messiah Co-op](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files) (directory entry "Dark Messiah Co-op Mod (wiltOS)") |
| QA level | community-reports |
| Last updated | 2026-10-23 |

## Where things stand

You can play the campaign co-op today with the wiltOS mod (alpha, version A2-03.23). Players
report that it is playable, with lag at high ping, HUD glitches and a portcullis bug that can
softlock a level. On Linux under Proton the mod loads and reaches the co-op menu. A session
with a joining client has not been tested yet.

## Existing work

- [KingDavidW/DarkMessiah-Coop-Mod-Files](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files):
  the current mod, GPL-3.0, prebuilt `dmcoop_server.dll` / `dmcoop_client.dll`, no source in the repo.
  Last push 2026-03-23. Uses a community master server (`master.wiltostech.com:27010`).
- Depends on the [wiltOS Mod Launcher](https://github.com/KingDavidW/DarkMessiah-ModLauncher-Files)
  and the [Advanced SDK base module](https://github.com/KingDavidW/DarkMessiah-AdvancedSDK-Mod-Files).
- [Balimbanana/DarkMessiahCoopSetup](https://github.com/Balimbanana/DarkMessiahCoopSetup): the
  2023 predecessor (lump edits loaded with `-tempcontent`). The wiltOS README credits it as the first co-op changes.

## How the game works (for co-op)

The Source engine already has client/server netcode, and Dark Messiah shipped a separate
multiplayer game. The mod runs the single-player maps on a listen server, with extra game
modules loaded by the launcher (`info.json` → `modules`) and map fixes as MapAdds. It launches
with `-insecure` and `+type singleplayer_and_multiplayer`.

## What we changed

Nothing in the mod. Install notes for Linux:

- The launcher ships `Steam.dll`; the game has `steam.dll`. On a case-sensitive filesystem,
  extracting the launcher as-is leaves both files. Copy `Steam.dll` over `steam.dll` instead.
- The mod load order can be set without the GUI in `_mods/core/user_settings.json`:
  `"load_order": ["advanced_sdk", "dm_coop"]`.

## QA

Checks that actually ran (2026-10-23, Linux, KDE Wayland, Proton Experimental, SteamLinuxRuntime_4):

- The launcher GUI opens under Proton.
- "Launch with Mods" loads `server_sdk.dll`, `client_sdk.dll`, `dmcoop_server.dll` and
  `dmcoop_client.dll` (checked in `/proc/<pid>/maps`).
- The main menu shows Find Servers / Create Server. The "Platform Error: module failed to
  initialize" from [issue #13](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files/issues/13) did not appear.

Tester playtest (2026-10-24, not a structured test): Linux host under Proton, Windows clients
joining over the internet through a port-forwarded listen server. The session worked. Players
reported that sync is "a bit off", and that climbing chains feels laggy. Not recorded: ping,
player count, and which `cl_*` cvars were set.

Not tested: what exactly syncs for enemies, doors, items or story.

## How to play

1. Back up the game folder files the launcher replaces: `mm.exe`, `steam.dll`, `steam.inf`,
   `steamclient.dll`, `tier0_s.dll`, `vstdlib_s.dll`.
2. Extract the Mod Launcher into `<game dir>`, then the Advanced SDK into `<game dir>/_mods/advanced_sdk`
   and the co-op files into `<game dir>/_mods/dm_coop`. The Advanced SDK must be first in the mod order.
3. Start from Steam, then click **Launch with Mods**.
4. The host clicks Create Server. Others use Find Servers, or type `connect <host ip>:27015` in the console
   (UDP 27015 must be reachable on the host).
5. Suggested client cvars from the maintainer ([#1](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files/issues/1)):
   `cl_predict 0`, `cl_interp 0.01`, `cl_interp_npcs 0.6`.

To uninstall, restore the backed-up files or use Steam's "Verify integrity of game files", then delete `_mods`.

## Known issues

- Portcullis wheels: holding Use until the wheel finishes leaves the gate shut (softlock).
  Let go before the wheel finishes turning ([#8](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files/issues/8)).
- Sync feels slightly off in normal play, and climbing chains is laggy (tester playtest 2026-10-24).
- Clients notice input lag at about 150 ms ping; VPN overlays like ZeroTier made it worse for one user ([#16](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files/issues/16)).
- HUD: wrong cooldown timer, mana shown capped at 29, no underwater stamina bar; chat sometimes drops names.
- Conflicts with the Unlimited Edition 1.4.1 modpack ([#6](https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files/issues/6)).

## Next steps

- Measure chain-climbing lag: compare `cl_predict 1` with `cl_predict 0`, recording ping and `net_graph`.
  Guess, not verified: chains are a movement mode the client may not predict, so with `cl_predict 0` every
  climb waits a full round trip.
- Record what syncs per subsystem (enemies, doors, items, story), ideally with a bot client (playbook/automated-qa.md).
- The source for the co-op DLLs is not public, so fixes go upstream through the wiltOS Discord or GitHub issues.
