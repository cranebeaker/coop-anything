# Dark Messiah of Might and Magic: working notes

## Game identity (observed 2026-10-23)
- Store: Steam, appid 2100 ("Dark Messiah of Might & Magic Single Player"), buildid 49752
- mm.exe SHA-256 (vanilla): f8faaee53a2c9ed72b8c7b83b75608f96a77ab07f374314950701901bda7e34c, 3727360 bytes
- Engine: Source (2006 branch, Arkane fork). OS: Windows binary, here run under Proton on Linux.

## Triage (observed)
- Existing co-op: wiltOS "Dark Messiah Co-op" by KingDavidW, version A2-03.23 (info.json),
  repo https://github.com/KingDavidW/DarkMessiah-Coop-Mod-Files, last push 2026-03-23, GPL-3.0,
  ships prebuilt bin/dmcoop_server.dll + bin/dmcoop_client.dll (no source in repo).
- Depends on: wiltOS Mod Launcher (https://github.com/KingDavidW/DarkMessiah-ModLauncher-Files,
  last push 2026-03-11) and Advanced SDK base module
  (https://github.com/KingDavidW/DarkMessiah-AdvancedSDK-Mod-Files, last push 2026-03-23).
- Launch params from info.json: -insecure, +setmaster add master.wiltostech.com:27010,
  gameinfo +type singleplayer_and_multiplayer.
- Launcher's mm.exe (SHA-256 74bb913ef6fc259d8eaa18eae317d9cf1b36c6be905a8f82a065b049e709ec69)
  differs from vanilla buildid 49752 mm.exe in 9 bytes (cmp -l). Same size. Purpose of patch: unknown
  (derived guess: loader hook for bin/wos_dmmm.dll).
- Older predecessor: https://github.com/Balimbanana/DarkMessiahCoopSetup (last commit 2023-03-16),
  lump edits via -tempcontent; credited in wiltOS README as the initial co-op changes.

## Known bugs (from GitHub issues, open as of 2026-10-23)
- #13 [Linux/Proton] "Platform Error: module failed to initialize" with co-op module enabled; no reply.
- #8 Portcullis wheel out of sync, can softlock; workaround: release Use before full rotation.
- #16 high latency over ZeroTier; #1 client input lag at ~150 ping; suggested cvars
  cl_predict 0, cl_interp 0.01, cl_interp_npcs 0.6 (maintainer comment 2025-02-14).
- HUD: #9 cooldown timer, #10 mana capped at 29 display, #11 no underwater stamina bar;
  #12 chat names missing; #3 head bob after bow; #5 stealth UI; #6 conflicts with Unlimited Edition modpack.

## Unknown
- Whether #13 still reproduces with A2-03.23 on current Proton.
- Max players, enemy/world/item sync scope.

## Install + Proton test (2026-10-23, observed)
- Pinned commits: ModLauncher 66aca607caa9af05f4e8ca21a2bbc967579d8cdd, AdvancedSDK
  c5a6fb8f84bf9e6bc4f463b45655d812d1d59e15, Coop 04f3c881cc1a6dc96cbffdffbe23147df1abaec0.
- SHA-256: wos_dmmm.dll 4b3c5bc9...8c83a, client_sdk.dll a15b4746...54bc3, server_sdk.dll cec5327a...0b9d,
  dmcoop_client.dll 713cd676...1f385, dmcoop_server.dll 4d94721a...3209 (full hashes in local/).
- Launcher overwrites 6 game-root files: mm.exe, steam.dll, steam.inf, steamclient.dll, tier0_s.dll,
  vstdlib_s.dll. The launcher repo ships `Steam.dll` (capital S); the game has `steam.dll`. On a
  case-sensitive Linux filesystem a plain extract leaves BOTH files. We copied it over `steam.dll`.
- Mod order can be set without the GUI: `_mods/core/user_settings.json` -> "load_order": ["advanced_sdk","dm_coop"].
- Proton Experimental, SteamLinuxRuntime_4, KDE Wayland: launcher GUI appears; "Launch with Mods" loads
  server_sdk.dll, client_sdk.dll, dmcoop_server.dll, dmcoop_client.dll (checked in /proc/<pid>/maps);
  main menu shows Find Servers / Create Server. Issue #13 "Platform Error" did NOT reproduce.
- `steam -applaunch 2100` from a shell did nothing (no GameAction in console_log); `xdg-open steam://rungameid/2100` worked.
- Not yet tested: hosting a session, a client joining, any gameplay sync.

## Tester playtest (2026-10-24, reported by a tester, not measured)
- Linux host (Proton), Windows clients via public IP + forwarded UDP 27015. Session worked.
- "sync was a bit off", "climbing on chains a bit laggy". Unknown: ping, player count, cvars used.
