# Saints Row: The Third Remastered — research notes

## Session 2026-11-27

Owner wants 3+ player campaign co-op on PC/Steam. Native campaign co-op is 2-player
(owner-provided); the goal is to extend it. Triage reported no known existing 3+
player campaign co-op project.

### Setup and claim

- Cloned https://github.com/cranebeaker/coop-anything into the project workspace; `git pull` clean.
- Installed GitHub CLI 2.106.0 via winget (official GitHub release, pinned SHA-256 in winget manifest), with owner permission; logged in to GitHub as the owner's account with owner permission.
- Local work is on branch `game/saints-row-the-third-remastered`.
- Ran the required checks: `gh issue list --search "Saints Row" --state all` and `gh pr list --search "Saints Row" --state all` in cranebeaker/coop-anything. Both returned no results before the claim.
- Opened claim issue: https://github.com/cranebeaker/coop-anything/issues/440 (title: "Claim: Saints Row: The Third Remastered (Steam) - 3+ player campaign co-op").
- Note: the issue template declares label `in-progress`, but that label does not exist in the repo; the issue was created without it. Possible playbook/template fix to send back later.
- Installed PyYAML 6.0.2 (pinned, owner-approved) and ran `python tools/build_directory.py`; the builder reported "wrote DIRECTORY.md: 590 entries".
- Forked the hub to the owner's GitHub account and opened draft PR https://github.com/cranebeaker/coop-anything/pull/441 with the research notes, write-up and directory entries. The owner-review checkbox is left unchecked.
- Installed official Ghidra 12.2 (`Ghidra_12.2_build`, published 2026-10-22) under `local/tools/` from the pinned release asset `ghidra_12.2_PUBLIC_20261016.zip`; verified SHA-256 `2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67` against the GitHub release `digest`. No other RE tools installed.
- Playbook warns `analyzeHeadless.bat` mangles paths with spaces. The Ghidra project, scripts and a 21 MB copy of `SRTTR.exe` are staged under the environment-approved no-space temp path, not under the workspace path (which contains "Default Project"). Only findings and hashes go back into the repo.
- The isolated install copy finished on 2026-11-27: robocopy exit 1 (normal "files copied" status), 0 mismatches, 0 failed, 0 extras; the copied `SRTTR.exe` SHA-256 matches the original.
- First Ghidra 12.2 headless run failed before analysis: it requires JDK 25+ and rejected the installed Java 21 (`JDK 25+ (64-bit) could not be found`). Owner approved installing pinned Microsoft OpenJDK 25.0.5.102 via winget; winget manifest installer SHA-256 `138be7ddf67d2c8cee75fe3e616f3e9a8289ef2552cbfbf78a7d35de37c440fa`, from https://aka.ms/download-jdk/microsoft-jdk-25.0.5.1-windows-x64.msi#winget . Winget verified the installer hash and installed it to `C:\Program Files\Microsoft\jdk-25.0.5.102-hotspot`; the headless xref scan was rerun with `JAVA_HOME` pointed there. No results have been reviewed yet.

### Identity (private details in local/identity.md)

- Store: Steam (owner confirmed 2026-11-27). Steam app id 978300 confirmed from the local `appmanifest_978300.acf`.
- Steam build id: 7026541 (`TargetBuildID` 7026541 in that manifest).
- Main exe: `SRTTR.exe`, SHA-256 `F23BE8B32759EC708A896F2F9C8EA35B99213ED3EF1DE9F75B99BEBFEEE7996A`; FileVersion/ProductVersion `1, 0, 0, 1`; ProductName `Saints Rowr: The ThirdT Remastered`; CompanyName `Koch Media GmbH, Austria`. All read read-only from the owner's install on 2026-11-27.
- Engine family: proprietary "Engine Core Technology Group" engine with Havok physics; SR3R's README says this for the Epic build, SR3MP's forum says the Remastered build is 64-bit. Neither has been verified against the owner's Steam exe.
- An isolated copy of the install is being made at an owner-specified path outside the repo; private path recorded in `local/identity.md`. The original install and saves are untouched. The first robocopy run was killed by a 5-minute command timeout, then resumed with a 30-minute timeout; robocopy skips files already copied.
  Source: https://github.com/MrReekoFTWxD/SR3R , https://www.saintsrowmods.com/forum/threads/sr3mp-multiplayer-mod.19447/

## Research 2026-11-27

### Observations (directly read from the linked source)

**SR3MP (https://github.com/SR3-MP, author @K3rhos):**

- "This project is now longer active, and is officially abandonned" in the org README. Project started May 2021.
- Closed source: "only repositories related with documentation or servers resources are available publicly". No netcode source was found.
- Custom netcode, **not** the game's original netcode; dedicated servers. FAQ: "Does your mod use the original netcode of the game? No, it's a custom netcode, using dedicated servers".
- Not campaign co-op. FAQ: "Can we play coop missions? No, SR3MP is comparable to FiveM, Garry's Mod or Roblox ... our goal is not to recreate a 'coop' mod with more players!"
- Scale: FAQ claims up to 127 players. Supports Steam, Epic and GOG versions of the Remastered game. Runs as an independent install and does not alter existing game files (author's claim).
- Public libraries listed by the project: ENet, ImGui, CEF, DirectXTK, MinHook, toml11, nlohmann JSON, Discord RPC, httplib, Zipper, SHA-1, base64, Lua, LuaBridge. This names the transport family (ENet) but is not SR3MP source.
- Latest public docs commit found: 2024-08-11 (`SR3-MP/.gitbook`, GITBOOK-92). Alpha changelog stops at 0.1.4. Forum thread last edited 2024-07-22.
- Changelog 0.1.4: "Reworked the network packets sender system, every packets get synchronized with the network thread before being sent over the network (50Hz thread loop) ... When opening the chatbox ... LUA handlers are now handled in the main thread." That is a concrete threading constraint for any in-process mod on this engine.
- Scripting docs show client/server event routing (`trigger_on_server`, `trigger_on_client`, `register`, `add_handler`) and game-UI helpers (objective waypoints, markers). Sources: https://github.com/SR3-MP/.gitbook/blob/main/docs/SUMMARY.md , https://raw.githubusercontent.com/SR3-MP/.gitbook/main/docs/home/changelogs/alpha/0.1.4.md , https://raw.githubusercontent.com/SR3-MP/SR3MP-Docs/main/docs/functions.md
- Forum thread: https://www.saintsrowmods.com/forum/threads/sr3mp-multiplayer-mod.19447/ (started 2022-02-24 by @K3rhos).
- `https://sr3mp.net` did not resolve from this environment on 2026-11-27 (browser `ERR_NAME_NOT_RESOLVED`, webfetch transport error). Do not treat the site's current content as verified; the GitHub and forum mirrors were used instead.

**Related public repositories:**

| Project | Last confirmed activity | License | What it is |
|---|---|---|---|
| https://github.com/RootKiller/sr3mp-Abandoned | last commit 2018-12-29 ("Characters and vehicles spawning demo") | none stated | Older abandoned SR3 MP prototype; `src/Client`, `src/Shared`, `sdk/SteamAPI`. Credited by SR3MP for reversed NPC/vehicle spawn functions. |
| https://github.com/MrReekoFTWxD/SR3R | last push 2021-12-30 | none stated | x64 Detours reverse-engineering project, Epic build; entity/ESP work, engine and Havok named in README. Credited by SR3MP for reversed UI functions. |
| https://github.com/MrReekoFTWxD/Saints-Row-3-Research | last push 2023-11-21 | none stated | Docs-only research dumps for SR3 Remastered and PS3 SR3. |
| https://github.com/SR3-MP/SR3-Helpers | last push 2024-05-17 | none stated | C helpers (Audio, HUD, Miscellaneous, World). |
| https://github.com/SR3-MP/SR3MP-IntelliSense | last push 2024-06-10 | MIT | Editor IntelliSense definitions for the SR3MP scripting API. |

License facts came from the GitHub repository API (`license` field) on 2026-11-27.

### Native Steam executable string scan (read-only, 2026-11-27)

I extracted printable ASCII strings from the owner's `SRTTR.exe` (SHA-256 above) without
modifying the install. Observations:

- Embedded original source paths include `sr3\src\game\multi\multi_core\multi_core_matchmaking.cpp`, `sr3\src\game\multi\multi_core\multi_core_global_sync.cpp`, `sr3\src\game\multi\multi_core\delta_compression\dc_objects.cpp` and `sr3\src\game\multi\multi_game\multi_game.cpp`. They also include `vendor\dsonline\include\memory\utilities\*.h` and `sr3\src\game\gamesave\gamesave_steam.cpp`. This is direct evidence that the shipped executable contains named native multiplayer/matchmaking/global-sync modules and a vendored `dsonline` library; it is not yet proof of how they work.
- Co-op/lobby strings include `coop_mode`, `Player slots`, `game_lobby`, `game_lobby_init_done`, `game_lobby_joined`, `game_lobby_disconnect`, `game_lobby_server_ready`, `game_lobby_client_ready`, `game_lobby_finished_loading`, `game_lobby_coop_finished`, `COOP_LOBBY_START_GAME`, `COOP_LOBBY_RECONNECT`, `game_lobby_update_ready`, `game_lobby_countdown_update`, `game_lobby_get_remote_player_name`, `game_lobby_server_char`, `game_lobby_client_char`, `MISSION_PARTNER_CONNECTED`, `MISSION_PARTNER_DISCONNECTED`, `HUD_WAIT_FOR_PARTNER_TO_JOIN`, `You must wait for your partner to finish joining.`, `DIALOG_PAUSE_DISCONNECT_PROMPT`, `Pause_menu_save_for_server_drop`, `COMPLETION_COOP_DISCONNECT`, `Completion_should_wait_for_coop`, `main_menu_set_coop_menu_type`, `coop_list_responder`.
- Steam-related strings include `+connect_lobby`, `SteamUser021`, `STEAMREMOTESTORAGE_INTERFACE_VERSION014`, `STEAMAPPS_INTERFACE_VERSION008`, `STEAMUSERSTATS_INTERFACE_VERSION012`, and a source path for `gamesave_steam.cpp`. The local install also contains `steam_api64.dll`.
- Other sync-related strings include `Disable_Synced_Attacks`, `Synced_Animation`, `Synced_Melee_Move`, `Synced_State`, `Synced_Partner_Flinch`, `trail_blazing_sync_data`, `tank_mayhem_sync_data`, `Allow_Update_By_Server`, `stop_when_host_destroyed`, and mission/activity strings such as `MISSION_COOP_CONFIRM_MISSION`, `MISSION_COOP_START_MSG`, `ACTIVITY_COOP_START_MSG`, `ACT_RUNNING_MAN_WAITING_ON_HOST`, `DIVERSION_COOP_SUCCESS_HOST_WIN` and `DIVERSION_COOP_SUCCESS_CLIENT_WIN`.
- The `+connect_lobby` string is a Steam command-line/connect hint; no runtime test has confirmed which lobby API is used.

I also parsed the PE import table of the same exe (script kept in `local/analysis/pe_imports.py`; ordinal names resolved from `C:\Windows\System32\wsock32.dll` and `ws2_32.dll` with `pe_exports.py`). Direct imports include:

- `WSOCK32.dll`: `WSAStartup`, `WSACleanup`, `socket`, `bind`, `sendto`, `recvfrom`, `select`, `setsockopt`, `getsockopt`, `ioctlsocket`, `htons`/`ntohs`, `inet_ntoa`, `gethostbyname`/`gethostname`, `__WSAFDIsSet`, `WSAGetLastError`.
- `WS2_32.dll`: `getaddrinfo`, `freeaddrinfo`, `WSASocketW`, `connect`, `listen`, `accept`, `send`, `recv`, `inet_pton`/`inet_ntop`, `getsockname`, `WSAIoctl`.
- `steam_api64.dll`: `SteamAPI_Init`, `SteamAPI_RunCallbacks`, `SteamAPI_RegisterCallback`/`RegisterCallResult`, `SteamInternal_CreateInterface`, `SteamInternal_FindOrCreateUserInterface`, `SteamAPI_GetHSteamUser`, `SteamAPI_RestartAppIfNecessary`, `SteamAPI_Shutdown`.

Derived hypotheses (not facts yet): the native implementation has distinct matchmaking and global-sync subsystems; the lobby appears to have explicit server/client roles, ready states and a loading handshake; `dsonline` may be the underlying online library; the executable has both Steam interface-based APIs and direct Winsock sockets, so transport may not be Steam-only. These are targets for Ghidra xref analysis, not conclusions about player-slot limits.

More string observations from the same scan, adjacent to the `multi_core_matchmaking.cpp` path: `SteamMatchMaking009`, `SteamClient020`, `SteamUtils010`, `SteamNetworking006`, `campaign`, `game_mode`, `horde`, `dlc_mask`, `voice`, `voice_info`, `join_rel`, `join_unrel`, `upd_conn`, `conn_left`, `badcon`, `dc_hello`, `dc_rupdate`, `dc_update`, `dc_dirty`, `dc_detach`, `dc_own`, `clock`, `bw_info`, `snd_lmt`, `down_lmt`, `migrate`, `post_migration`, `modify_session`, `ext_data`, `upd_state`, `synced_sys`, `ack_reply`, `hrtbeat`, `no_connect`, `bulltaco`, `findserver_pool`, `private`, `joined`, `MULTI_ERROR_SESSION_FULL`, `MULTI_ERROR_HOST_LEFT`, `MP_JOIN_ACCEPT`, `MP_JOIN_DENY`, `COOP_HOST_BUSY`, `COOP_JOIN_IS_WAITING`, `COOP_SERVER_SAVEGAME_LOADING`, `COOP_DLC_MISMATCH`, `Remote Command from %ls: %s`, and RTTI such as `multi_session_op_start`, `dc_human_steering_compressor`, `dc_blocking_animation_compressor_partial`, `dc_blocking_animation_compressor_full`, `dc_byte_data_compressor`, `dc_detour_reg`, `dc_hud_healthbar_reg`, `llp_allocator<player_respawn_data>`, `llp_allocator<player_morph_event_data>`, `llp_allocator<player_component_event_data>`, `llp_allocator<player_send_morph_event_data>`, `multi_game_sim_event`, `variant_update_data`, and `explosion_event_data`. These give concrete function/data names to hunt in Ghidra; their semantics are not yet decoded.

RTTI strings additionally identify `steam_platform` callbacks `P2PSessionRequest_t`, `P2PSessionConnectFail_t`, `GameLobbyJoinRequested_t`, `LobbyInvite_t`, `LobbyDataUpdate_t`, and `ui_game_lobby`; session operations `multi_session_operation`, `multi_session_op_create`, `multi_session_op_delete`, `multi_session_op_join`, `multi_session_op_modify`, `multi_session_op_start`, `multi_session_op_end`; and typed results `CCallResult<multi_session_op_create, LobbyCreated_t>` and `CCallback<multi_session_op_join, LobbyEnter_t>`. That is direct evidence the native session layer uses Steam lobby creation/join callbacks plus Steam P2P session callbacks; it still does not reveal the player cap. Delta-compression RTTI names cover players, humans, vehicles, props, script objects, triggers, cameras, ragdolls, hit points, killed data, component/rig/mover data and more, which suggests a general object-registration system worth inspecting rather than only two hardcoded player slots.

### Broader public-source research (2026-11-27)

A separate read-only research pass found these observations (sources below):

- Steam's store page says the game can be played solo or "online with a friend"; PCGamingWiki and Co-Optimus list 2 online players. TechRaptor and GameRevolution describe full campaign co-op with one friend, drop-in/drop-out, no split-screen, and friends/private/open session options. Source: https://store.steampowered.com/app/978300/Saints_Row_The_Third_Remastered/ , https://www.pcgamingwiki.com/wiki/Saints_Row:_The_Third_Remastered , https://www.co-optimus.com/game/7247/pc/saints-row-the-third-remastered.html , https://techraptor.net/gaming/guides/saints-row-3-remastered-split-screen-online-multiplayer , https://www.gamerevolution.com/guides/647255-saints-row-the-third-co-op-split-screen
- Community reports say in-game matchmaking often finds "no connectable games", while accepting a Steam friend invite works; one user says the Remastered version has only internet co-op, not LAN/network co-op, contradicting Co-Optimus's LAN cell. Unresolved without a test. Source: https://steamcommunity.com/app/978300/discussions/0/4290314417248660346 , https://steamcommunity.com/app/978300/discussions/0/3185740658302144752
- Portforward lists TCP/UDP 4200 for SR3R. A 2026 Wireshark article about the **original** SR3 reports UDP broadcasts and gameplay unicast on port 4200. Neither is a capture of build 7026541. Source: https://portforward.com/saints-row-the-third-remastered , https://chris-besch.com/articles/saints_row_3_wireguard/
- Prima's official guide for **original** SR3 says co-op happens in the host's city, the guest cannot buy cribs/upgrades or do gang customization there, the host's voice is primarily used, missions have host/guest role differences, and success/failure is shared with a revive requirement. Not verified for the remaster. Source: https://primagames.com/eguides/saints-row-the-third-eguide/multiplayer/multiplayer/co-op
- Volition Senior Programmer Russell wrote in 2014: "Adding additional players to SR3 & 4 is basically a non-starter. Just on the technical side there are various limitations based on how we built many of the gameplay systems to only expect to have to deal with two players." This was about the original games, not the 2020 remaster, but it is a direct warning that netcode slots alone may not be enough. Source: https://www.saintsrowmods.com/forum/threads/more-co-op-players-or-any-type-of-multiplayer-support.6196
- SteamDB/appinfo list build 7026541 as 64-bit with engine tag `CTG_OR_Geo-Mod Engine`. This matches the `ctg\src` paths in the executable strings. Source: https://steamdb.info/app/978300/depots , https://steamdb.info/app/978300/info , https://api.steamcmd.net/v1/info/978300
- Russian-language pages describe the same co-op UI path (campaign co-op → online → new game/invite friend), but those sites distribute unlicensed builds/fixes; only the UI description was noted, nothing was downloaded or copied. Source: https://online-fix.me/games/shooter/16359-saints-row-the-third-remastered-po-seti.html , https://freetp.org/po-seti/4292-saints-row-the-third-igra-po-seti-besplatno-v-onlayn.html

### Conclusions derived from those observations (not measured in-game)

1. **No SR3MP netcode can be copied**: the implementation is closed source and the project is abandoned. Its public docs describe architecture and behaviour, not code.
2. **Reusable findings, not reusable code**: dedicated-server topology, ENet reliable UDP as a transport choice, a 50 Hz network thread that owns packet sending, main-thread requirement for game/Lua calls, client/server event routing, and the names of reversed engine functions in the public repos. These are leads to verify against the Steam build, not proof that anything works there.
3. **Code from the no-license repos must not be copied** under this project's rules. The MIT IntelliSense repo can be used under MIT if needed. Asking the authors for a license or permission is an option, but that means contacting them — owner permission required first.
4. **Most promising path remains the game's native 2-player co-op**: SR3MP explicitly does not sync missions/story, so it does not solve the campaign problem. Whether the native co-op replication can accept a third client has to be answered by reverse engineering the Steam build on an isolated copy.
5. **SR3MP's scale claim (127 players) proves nothing about campaign co-op**; it refers to a sandbox server, not story/mission state.

### Unknowns (do not claim otherwise)

- Exact engine version for the owner's install (the reported "Engine Core Technology Group + Havok" description has not been verified against this exe).
- Whether the native 2-player co-op uses a general replication layer or a hardcoded two-slot session.
- Whether story, mission triggers, checkpoints and saves are host-authoritative or client-side.
- Whether SR3MP authors would license or share any internal netcode/findings on request.
- No in-game test has been run; QA level is `none` for this game.

### Searched (2026-11-27)

- Co-op Anything `DIRECTORY.md`, `games/`, and a repo-wide grep for `Saints Row|SR3MP|sr3mp`: no matches.
- `gh issue list --search "Saints Row" --state all` and `gh pr list --search "Saints Row" --state all` in cranebeaker/coop-anything: no results before the claim.
- `gh search repos` for `sr3mp`, `saints row 3 multiplayer`, `saints row remastered`, `saints row multiplayer`: found SR3-MP org, RootKiller/sr3mp-Abandoned, SR3R and Saints-Row-3-Research only.
- Web search phrasings: "SR3MP Saints Row The Third multiplayer mod netcode source code remastered", "Saints Row The Third Remastered 3 player 4 player campaign co-op mod", "SR3MP Saints Row The Third Remastered co-op multiplayer 3 players", "Saints Row 3 Remastered кооператив 3 игрока мод multiplayer", "site:github.com Saints Row co-op remastered mod multiplayer", `"Saints Row: The Third" "3 players" co-op campaign mod remastered".
- Read: SR3MP org README, SR3MP forum thread, SR3MP GitBook FAQ/getting-started/changelog 0.1.4, SR3MP-Docs functions.md, SR3R README.
- Not checked: Discord (needs owner permission/account), Steam discussions body (page rendered without post content through the text fetch), Nexus/ModDB individual file pages (fetches often blocked; only search result text seen), archive.org, YouTube transcripts.

### Owner question

Research playbook asks the owner who they have heard of working on the game. On 2026-11-27 the owner did not name a lead, and authorized contacting the SR3MP authors only if a specific finding later proves necessary. No contact has been made.
