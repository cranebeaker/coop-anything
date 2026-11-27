# Saints Row: The Third Remastered (2020)

| | |
|---|---|
| Engine | Proprietary CTG / Geo-Mod engine + Havok (SteamDB tag `CTG_OR_Geo-Mod Engine`; `ctg\src` and `vendor\havok` paths in `SRTTR.exe`) |
| Versions tested | None yet. Target: Steam build 7026541; `SRTTR.exe` SHA-256 `F23BE8B32759EC708A896F2F9C8EA35B99213ED3EF1DE9F75B99BEBFEEE7996A` |
| Projects | Claim: https://github.com/cranebeaker/coop-anything/issues/440 ; no 3+ player campaign co-op project found |
| QA level | none |
| Last updated | 2026-11-27 |

## Where things stand

The Remastered game ships 2-player campaign co-op. No 3+ player campaign co-op mod was
found in this research pass. The obvious sandbox multiplayer mod, SR3MP, is closed
source, abandoned, and explicitly not a campaign co-op project. A read-only scan of the
Steam exe found native matchmaking/global-sync modules, so the next step is to map them
on an isolated copy of the Steam build.

## Existing work

- **Official 2-player co-op**: Steam describes playing solo or "online with a friend"; PCGamingWiki and Co-Optimus list 2 online players. Guides describe full campaign co-op with one friend, drop-in/drop-out, no split-screen, and friends/private/open sessions. Sources: https://store.steampowered.com/app/978300/Saints_Row_The_Third_Remastered/ , https://www.pcgamingwiki.com/wiki/Saints_Row_The_Third_Remastered , https://www.co-optimus.com/game/7247/pc/saints-row-the-third-remastered.html , https://techraptor.net/gaming/guides/saints-row-3-remastered-split-screen-online-multiplayer , https://www.gamerevolution.com/guides/647255-saints-row-the-third-co-op-split-screen
- **SR3MP** (https://github.com/SR3-MP, forum: https://www.saintsrowmods.com/forum/threads/sr3mp-multiplayer-mod.19447/): standalone custom multiplayer client for the Remastered game (Steam/Epic/GOG), dedicated servers, ENet, Lua client/server scripting, author-claimed up to 127 players. The org README says the project is officially abandoned and closed source. FAQ explicitly says it does not use the game's original netcode and that making co-op missions with more players is not its goal. Latest public docs commit 2024-08-11; alpha changelog stops at 0.1.4. QA level for campaign co-op: none (it does not do campaign co-op).
- **RootKiller/sr3mp-Abandoned** (https://github.com/RootKiller/sr3mp-Abandoned): older prototype, last commit 2018-12-29, no license stated. Credited by SR3MP for reversed NPC/vehicle spawn functions. Code cannot be copied without permission.
- **SR3R** (https://github.com/MrReekoFTWxD/SR3R): x64 Detours reverse-engineering project for the Epic build, last push 2021-12-30, no license stated. Names the engine and Havok and provides reversed entity/UI leads; verify addresses against the Steam build.
- **SR3-MP/SR3MP-IntelliSense** (https://github.com/SR3-MP/SR3MP-IntelliSense): MIT-licensed scripting API definitions, last push 2024-06-10.
- No project was found that extends campaign co-op to 3+ players.

## How the game works (for co-op)

Findings from a read-only scan of the Steam build, not yet runtime-verified:

- `SRTTR.exe` contains original source paths for `multi_core_matchmaking.cpp`, `multi_core_global_sync.cpp`, `delta_compression\dc_objects.cpp` and `multi_game.cpp`, plus `vendor\dsonline` container headers. Nearby strings name `SteamMatchMaking009`, `SteamClient020`, `SteamUtils010`, `SteamNetworking006`, `campaign`, `game_mode`, `horde`, `dlc_mask`, `voice`, `join_rel`, `join_unrel`, `conn_left`, `dc_hello`, `dc_update`, `dc_own`, `migrate`, `modify_session`, `synced_sys`, `hrtbeat`, `MULTI_ERROR_SESSION_FULL`, `MULTI_ERROR_HOST_LEFT`, `MP_JOIN_ACCEPT` and `COOP_SERVER_SAVEGAME_LOADING`.
- RTTI strings identify `steam_platform` callbacks for `P2PSessionRequest_t`, `P2PSessionConnectFail_t`, `GameLobbyJoinRequested_t`, `LobbyInvite_t` and `LobbyDataUpdate_t`, plus `multi_session_op_create/join/modify/start/end` and typed `LobbyCreated_t`/`LobbyEnter_t` results. The native session layer therefore uses Steam lobbies and Steam P2P session callbacks; the player cap is still unknown.
- The exe imports both Steam interface APIs and direct Winsock functions (`socket`, `bind`, `sendto`, `recvfrom`, `connect`, `listen`, `accept`, `send`, `recv`, `getaddrinfo`, `WSASocketW`). Transport is therefore not proven to be Steam-only.
- Delta-compression RTTI covers players, humans, vehicles, props, script objects, triggers, cameras, ragdolls and more, suggesting a general object-registration system rather than only a two-player special case.
- Lobby UI strings show server/client roles, ready states, countdowns, loading handshakes and disconnect/save handling (`game_lobby_*`, `COOP_LOBBY_START_GAME`, `COOP_LOBBY_RECONNECT`, `Pause_menu_save_for_server_drop`, `Completion_should_wait_for_coop`).
- Community reports say Steam friend invites work while in-game matchmaking often finds no joinable games; LAN support is contradictory between sources. Portforward lists TCP/UDP 4200 for SR3R, and a packet-capture article for the **original** SR3 reports UDP 4200, but build 7026541 has not been captured. Sources: https://steamcommunity.com/app/978300/discussions/0/4290314417248660346 , https://portforward.com/saints-row-the-third-remastered , https://chris-besch.com/articles/saints_row_3_wireguard/
- For the **original** SR3, Prima documents host-centric rules: co-op occurs in the host's city, the guest cannot buy cribs/upgrades or change gang customization there, missions have host/guest role differences, and progress/saves are host-controlled. Not verified for the remaster. Source: https://primagames.com/eguides/saints-row-the-third-eguide/multiplayer/multiplayer/co-op
- Volition Senior Programmer Russell wrote in 2014 that adding players to original SR3/SR4 was "basically a non-starter" because gameplay systems were built for two players. This is about the original games, but it warns that slots alone may not be enough. Source: https://www.saintsrowmods.com/forum/threads/more-co-op-players-or-any-type-of-multiplayer-support.6196
- SR3MP's public docs say game-state calls must run on the main thread and that its 0.1.4 changelog describes a 50 Hz network thread. Those are SR3MP findings, not native netcode findings.

Native co-op's player-slot limit, replication details, mission triggers and save authority are still unknown.

## What we changed

Nothing in the game yet. Research, claim issue, directory entries and read-only binary inspection only.

## QA

No tests run. QA level: `none`.

## How to play

Not yet possible with 3+ players. Native 2-player co-op remains the only known option.

## Known issues

- SR3MP's old site `sr3mp.net` did not resolve from the research environment on 2026-11-27; use the GitHub mirrors listed above.
- No 3+ player build exists to test.
- Public sources disagree about LAN/network co-op on the Remastered edition.

## Next steps

1. Finish the isolated copy of the Steam install (the original install and saves are untouched).
2. Use Ghidra to find xrefs to the native matchmaking/global-sync strings and map the lobby, session, replication and mission-state code.
3. Measure the player-slot limit and whether a third client can join before designing a patch.
4. Test with bots in multiple isolated game copies; QA level stays `none` until a test actually runs.
5. Ask the SR3MP authors about licensing only if a specific finding turns out to be necessary (owner permission already given for that limited case).
