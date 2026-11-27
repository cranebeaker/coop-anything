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

### Read-only RIP-relative xref scan (2026-11-27)

Ghidra's first full analysis attempt ran out of Java heap in `AddressTableAnalyzer`; a retry with a 6 GB heap was started but had not produced reviewed results at the time of this note. As a fallback, `local/analysis/find_rip_refs.py` scans the isolated staged exe for RIP-relative `LEA` references to string addresses. It reads the exe only and does not disassemble instructions or prove function boundaries.

Observed direct xrefs (VA = image base `0x140000000` plus section RVA):

- Source paths `multi_core_matchmaking.cpp` and `multi_core_global_sync.cpp` have no direct RIP-relative xrefs found; they may be debug-only strings or referenced through other instruction forms.
- `multi_game` at `0x140e0c438` is referenced from `0x140018db4`.
- `SteamMatchMaking009` at `0x140e07b68` is referenced from `0x14045a1e1`; `SteamNetworking006` at `0x140e08120` from `0x14045c661`.
- `game_lobby` strings are referenced heavily around `0x1403a7ed0`-`0x1403a9800`, and `COOP_LOBBY` from `0x1403a8e76`, `0x1403a8f5c` and `0x1403a8f79`. That cluster is the next place to inspect for lobby state and slot handling.
- `MULTI_ERROR_SESSION_FULL` (`0x140e0a1e8`) and `MULTI_ERROR_HOST_LEFT` (`0x140e0a208`) are referenced together from `0x140475a70` and `0x140475a84`; `MP_JOIN_ACCEPT` from `0x140472928`; `COOP_SERVER_SAVEGAME_LOADING` from `0x14046ef22` and `0x14046f4ac`.
- `Player slots` at `0x140daebd0` is referenced from `0x14016b1c8`, but nearby strings are memory-pool labels (`weapon high res gpu`, `cloth sim runtime cpu`, `coop_player cpu`, `coop_player gpu`, `Vehicle slots`, `Character slots`). It is a memory-budget label, not evidence of a player-count cap.
- The network names `ping`, `pong`, `voice`, `voice_info`, `join_rel`, `join_unrel`, `upd_conn`, `conn_left`, `badcon`, `dc_hello`, `dc_rupdate`, `dc_update`, `dc_dirty`, `dc_detach`, `dc_own`, `clock`, `bw_info`, `snd_lmt`, `down_lmt`, `migrate`, `post_migration`, `modify_session`, `ext_data`, `upd_state`, `synced_sys`, `ack_reply`, `hrtbeat`, `no_connect`, `bulltaco` and `acks` form one contiguous string table from `0x140e07cd0` to `0x140e07e58`. Their xrefs are concentrated from `0x14045b9d5` to `0x14045bfcd`, which looks like a message-name registration routine. Decompiling that routine is the highest-value next step because it should expose packet IDs and their handlers.
- `DEFAULT_PLAYER_NAME` is referenced from `0x14045bfcd`, at the end of that same cluster.

These are observations about code addresses in build 7026541; no function boundaries, control flow, player limit or story authority have been decoded yet.

A follow-up raw-byte decoder (`local/analysis/parse_msg_reg.py`, output in `local/analysis/srttr_msg_ids.txt`) decoded the registration function at `0x14045b9c0`. It makes 33 calls to helper `0x14045b0a0`, one per message ID 0-32, with these names in order:

`ping`, `pong`, `voice`, `voice_info`, `join_rel`, `join_unrel`, `upd_conn`, `conn_left`, `badcon`, `dc_hello`, `dc_rupdate`, `dc_update`, `dc_dirty`, `dc_create`, `dc_detach`, `dc_own`, `qos`, `clock`, `bw_info`, `snd_lmt`, `down_lmt`, `migrate`, `post_migration`, `modify_session`, `ext_data`, `upd_state`, `synced_sys`, `ack_reply`, `hrtbeat`, `no_connect`, `bulltaco`, `acks`, `global`.

Ghidra 12.2 decompilation of the registration function and helper (`local/analysis/ghidra_scripts/DecompileTargets.java`, output `decompiled_targets.c`) confirmed and corrected the earlier raw-byte decoding. The table is at `0x141d6f610` (the earlier `0x140d6f610` was off by `0x10000000` because an ad-hoc file-offset decoder confused RVA with VA), with a 40-byte stride indexed by the unsigned-byte message ID. Each entry stores: ID at +0, name pointer at +8, handler pointer at +0x10 (also used as the occupancy check), a second pointer/parameter at +0x18, a small channel-like value at +0x20 (`0`, `1` or `2`), a flag byte at +0x24 (`0`, `1`, `2`, `6` or `7`) and a byte at +0x25 (`0` or `0xff`). Registration only proceeds if the slot handler is null and the handler argument is nonzero, and if `param_7 == 0xff` or `param_6 & 1` is set. The meanings of the channel/flag bytes are **unknown**. Confirmed message-to-handler map: `ping`->`0x14045ea70`, `pong`->`0x14045eb60`, `voice`->`0x140466a90`, `voice_info`->`0x1404668c0`, `join_rel`/`join_unrel`->`0x1404628a0`, `upd_conn`->`0x140455900`, `conn_left`->`0x1404637a0`, `badcon`->`0x1404557d0`, `dc_hello`->`0x140451cf0`, `dc_rupdate`->`0x140451f00`, `dc_update`->`0x140451ff0`, `dc_dirty`->`0x140451c30`, `dc_create`->`0x14044e010`, `dc_detach`->`0x14044e220`, `dc_own`->`0x14044c430`, `qos`->`0x14045df40`, `clock`->`0x14046a790`, `bw_info`->`0x14045d4b0`, `snd_lmt`->`0x14045dcb0`, `down_lmt`->`0x14045dc50`, `post_migration`->`0x14045ab10`, `upd_state`->`0x140456ee0`, `synced_sys`->`0x1404653f0`, `ack_reply`->`0x140456ad0`, `no_connect`->`0x140453b20`, `acks`->`0x140455820`, `global`->`0x140458d50`. Five handlers (`migrate`, `modify_session`, `ext_data`, `hrtbeat`, `bulltaco`) were mislabelled `_guard_check_icall` by Ghidra and still need raw decoding. This gives a concrete packet-ID map for later tracing but still says nothing about the player cap.

A second raw-byte decoder (`local/analysis/decode_error_switch.py`) decoded a 15-case error-string switch at `0x140475a20`, with jump table at `0x140475b78`. Input codes 1-15 map, in order, to `MULTI_ERROR_INTERNAL_ERROR`, `MULTI_ERROR_NO_JOIN_INFO`, `MULTI_ERROR_SESSION_ERROR`, `MULTI_ERROR_SESSION_FULL`, `MULTI_ERROR_CANT_FIND_MATCH`, `MULTI_ERROR_HOST_LEFT`, `COOP_KICKED`, `MULTI_ERROR_JOIN_REJECTED`, `MULTI_ERROR_NETWORK_TROUBLE`, `MULTI_NO_ETHERNET`, `MULTI_LOST_CONNECTION`, `MAINMENU_LOGIN_CHANGE_EXPOSITION`, `MAINMENU_DOWNLOADABLE_CONTENT_REMOVED_EXPOSITION`, `PLT_TITLE_COMPATIBILITY_PC` and `NO_NETWORK_CONNECTION`. Direct calls to the switch found so far are at `0x140475cb2`, `0x140475da4` and `0x140475e76`; the first passes a computed code in `ebx`, so the decision that raises `SESSION_FULL` is not yet isolated.

The `game_lobby_*` strings around `0x1403a8008`-`0x1403a8f79` are Lua/script binding names rather than game logic themselves. Decoding the binding descriptors gave these implementation leads:

- `game_lobby_get_local_player_name` -> `0x1403a84a0`; calls common getter (Ghidra name `FUN_140a11740`) then accessor `FUN_140573690` and copies `0xc0` bytes from that object at `+0x2134`.
- `game_lobby_get_remote_player_name` -> `0x1403a8510`; calls common getter (`FUN_140a11740`) then accessor `FUN_1404547c0` and copies `0xc0` bytes from that object at `+0xce`.

Correction: the earlier note that `0x1403a8510` calls `0x140453bc0` and walks a linked list was wrong — `0x453bc0` is the **file offset** of the call target whose VA is `0x1404547c0`. The linked-list bytes I described were a different function. Both accessors have now been decompiled (see below); this is still **not** evidence for or against a 3+ player cap.

#### Full Ghidra analysis and batch decompilations (2026-11-27)

The full headless Ghidra 12.2 analysis completed successfully with `-Xmx6G` (825 s; log in `%TEMP%\opencode\sr3r\headless.log`), and `FindNetworkStrings.java` logged 91 string/xref matches. Larger decompilation batches are in `decompiled_targets.c`, `decompiled_batch2.c` and (pending) `decompiled_batch3.c` under the same temp folder; scripts are in `local/analysis/ghidra_scripts/`.

Verified from Ghidra decompilation (not runtime tests):

- `FUN_1404547c0` (remote-player accessor) gets the multiplayer session from `FUN_14046a740`, reads a circular linked list head from session `+0x58`, walks entries via `+0xb2c0`, and returns the first entry whose first byte is zero (free slot). `FUN_140573690` just returns the single local-player global `0x141dd5b60`. This shows remote players are list-backed, but says nothing about a cap on list length.
- `FUN_140459cf0` (matchmaking) allocates an array of `0x70`-byte entries. Its count is `DAT_141d6f5b0 + FUN_14045a7f0()`, clamped by `*(uint *)(DAT_141d6f598 + 4)`. Those globals are a **prime suspect** for a hard player/session cap; where they are written is being traced.
- `FUN_1404628a0` (join handlers) only dispatches join cases 1-5 when a nested object at `(session context -> +0x160 -> +0x280 -> +0x80)` has `*(int *)(obj + 8) == 2`; another `== 2` check appears in the lobby start path. `2` is likely a session/mode enum (coop), not yet proven to be a player count.
- `FUN_1404637a0` (connection-left handler) maps reason `3` to `MULTI_ERROR_HOST_LEFT`, `0xf` to `COOP_KICKED`, and anything else to `MULTI_ERROR_INTERNAL_ERROR` by calling `FUN_140475c30` with error codes `6`, `7` and `1`.
- `FUN_14044c430` (`dc_own`) reads/writes byte-sized player IDs at player-object `+0x168`, using `0xff` for "no owner"; ownership payloads carry that byte. `FUN_140458d50` (`global`) syncs named global objects with byte owner IDs, so neither is visibly limited to two IDs in the decompiled code.
- `FUN_1403a8d50` (lobby UI) uses `FUN_1401277b0() == 2` to identify coop mode and drives the `game_lobby_*` script variables; this is a mode check, not a player count.
- `Player slots` xrefs resolve to `FUN_14016b010`, a very large memory-budget/pool initialiser that also registers `coop_player cpu/gpu`, `Vehicle slots` and `Character slots`. This confirms it is not a gameplay player-cap check.
- `FUN_1403a8510` copies `0xc0` bytes from remote entry `+0xce`; `FUN_1403a84a0` copies `0xc0` bytes from the local player at `+0x2134`.

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

### Ghidra batches 7-9 (2026-11-27)

Scripts `AnalyzeBatch7.java`-`AnalyzeBatch9.java`; outputs `decompiled_batch7.c`-`decompiled_batch9.c`
and `xrefs_batch7.txt`-`xrefs_batch9.txt` in `%TEMP%\opencode\sr3r\`. All headless runs
exited 0. Findings from decompilation, not runtime tests:

- `FUN_14045e900` allocates the pool named `findserver_pool` (`0x11a00` bytes) and builds
  `0x40` free entries of stride `0x468` in the free list at `DAT_141d71f40`. `FUN_14045eb60`
  (the `pong` handler) only adds an entry while `DAT_141d71f30 < 0x40`, parses a pong payload
  with `FUN_14045b380(...,0x100)`, de-duplicates with `FUN_14046c4c0`, writes the label
  `MP_TYPE` / `#Multiplayer` into the new entry, and increments `DAT_141d71f30`.
  **Correction:** `DAT_141d71f30` / `DAT_141d71f38` / `DAT_141d71f40` are therefore the
  *find-server / server-browser* list, not the in-game co-op player list. The 64-entry bound
  is a server-browser cap. `FUN_14045ea60` returns that list's count; `FUN_14045f390`
  returns an entry by index by walking `+0x458` pointers. Earlier notes that called these
  "player count/player by index" were a mislabelling, not evidence about player slots.
- `FUN_14045d5f0` iterates a candidate pool at `DAT_141d73c90` with stride `0x2b0` and
  gates sends on `*(uint *)(player + 0x16c) < 2` and on `FUN_1402e0cc0()` returning 5 or 6.
  `FUN_140456de0` frees global remote-player entries whose `+0x16c` is also `< 2`, so
  `+0x16c` looks like a connection-state field; its exact enum is still unknown.
- `FUN_140452c30` allocates a `0xb2d8`-byte entry from the global pool `DAT_141d6ef60`
  using a `ushort` free-index list, then initialises it; no two-entry limit appears in the
  allocator. `FUN_140453a80` uses it to create a player, sets the name `UNKNOWN` at `+0xce`
  and calls `FUN_140454c30`. `FUN_1404546f0` finds a free global-pool entry (first byte `1`);
  `FUN_1404547c0` finds a free entry in the session's circular list at `+0x58` (first byte
  `0`, stride `0xb2c0`).
- `FUN_14084b330` is only an rwlock/critical-section initialiser for `DAT_141d6ef60`; it is
  not a fixed-size pool setup. `FUN_140469ec0(obj)` returns `*(int *)(obj + 0x70)`; it is
  used both in a join gate (`1 < value`) and in latency-bucket selection (2/0xb/0xd). Its
  meaning is still unknown.

No hard two-player cap has been found in the allocators, join path, message dispatch or
server-browser list inspected so far. This does **not** contradict the 2014 developer
warning that gameplay systems assume two players; it only narrows where to look next.

### Ghidra batch 10 and raw message-handler decode (2026-11-27)

- The message table at `0x141d6f610` is in uninitialised static storage: reading it from
  the file/Ghidra memory gives all zeros until the registration function runs. The stored
  handler pointers had to be recovered from the registration calls themselves.
- New raw decoder `local/analysis/decode_msg_handlers.py` reads the `lea r9,[rip+disp]`
  handler argument before each call to `FUN_14045b0a0`. It confirms all 33 handler
  addresses from the earlier decompilation **and** resolves the five that Ghidra labelled
  `_guard_check_icall`: `migrate`, `modify_session`, `ext_data`, `hrtbeat` and `bulltaco`
  all pass `0x1400486d0`. Bytes at that address are `c2 00 00 cc cc ...`, i.e. `ret`
  followed by padding. In this build those five message types are registered to a no-op
  stub; there is no implemented runtime `modify_session` or host-migration handler to
  reuse for adding slots.
- `FUN_140475c30` writes the error code to global `DAT_141d7433c` and shows the warning
  dialog through `FUN_140475a20` (which maps code `4` to `MULTI_ERROR_SESSION_FULL`).
  A raw scan of all direct calls to `FUN_140475c30` found 12 sites, with immediate codes
  `1,2,6,7,10,11,12,16,17,18` — no direct call with `4`. The decision that raises
  `SESSION_FULL` is still not isolated.
- `FUN_140456de0` frees a per-player array at `+0x181 + i*0x10`, bounded by a count at
  `+0x1578`. Player initialisation `FUN_140454c30` sets the state byte at `+0x16c` to
  `3`; the `< 2` tests seen in the join and latency paths are consistent with a state
  check, not a player-count limit, but the enum is still unverified.

### Mission/lobby and join-request strings (batches 11-12, 2026-11-27)

- `MISSION_PARTNER_CONNECTED` is used by `FUN_140471a60`: when its `param_2 == 0` it
  shows the connected message, otherwise it calls `FUN_140462290`.
  `MISSION_PARTNER_DISCONNECTED` is used by `FUN_14027dd50` when an 8-byte script value
  differs from a reference string and a script state equals `5`. These are script/UI
  notifications, not slot allocation.
- `FUN_1403a86a0` registers the `game_lobby_*` Lua bindings, including a single-index
  `game_lobby_get_remote_player_name` (`FUN_1403a8510`), `game_lobby_finished_loading`
  and `game_lobby_coop_finished`. `FUN_1403a8d50` is the lobby UI script driver: its
  `*piVar9 != 2` test is still consistent with a coop **mode** enum, not a player count.
- The join-request dialog `FUN_14046ec40` branches on a state byte `2`, `3` or `4`
  (`COOP_JOIN_IS_WAITING` is one of its messages). `FUN_1404728d0` builds the
  `MP_JOIN_DENY` / `MP_JOIN_ACCEPT` prompt; `FUN_140474410` shows `COOP_HOST_BUSY`.
  No explicit "two players already present" rejection was found in these paths.
- The error-code global `DAT_141d7433c` is written by `FUN_140475bd0`, `FUN_140475c30`
  and `FUN_140475d80`/`FUN_140475e70` wrappers; `FUN_140475a20` maps code `4` to
  `MULTI_ERROR_SESSION_FULL`. The code-4 decision is still not isolated.

Static analysis has now covered allocators, message dispatch, message handlers,
matchmaking, server browser, lobby UI, join request and mission-partner notifications
without finding a two-player slot limit in the replication layer itself. The remaining
likely blockers are gameplay systems that assume a single partner (the 2014 developer
warning), which can only be settled by running the game. No runtime test has been performed.

### Hard lobby cap found (batch 14, 2026-11-27)

- `FUN_14046cd00` calls SteamMatchmaking vtable offset `0x68` with arguments
  `(lobby type, 2)`. This matches `ISteamMatchmaking::CreateLobby(ELobbyType, int
  cMaxMembers)`, so the host creates the campaign/horde lobby with a **maximum of two
  members**. This is the first concrete hard two-player limit found.
- The same function selects lobby type from a config field (`+0x6c`): `0 -> 2`,
  `1 -> 1`, otherwise `0`. The `2` passed to `CreateLobby` is separate and is the
  member cap.
- `FUN_14046ca20` clamps session field `+0x13c` to `1` when its `param_2 == 2`
  (a matchmaking callback/leave path). It was first read as a player-count reset;
  batch 20 reclassified `+0x13c` as an expected-state field, so this is a state
  reset.
- **SUPERSEDED by the batch 20 correction below.** Earlier this section read session
  fields `+0x138` and `+0x13c` as current/expected player counts and treated
  `2 < value` as a latent 3+ player path. That interpretation was wrong. The hard
  `CreateLobby` cap in this section remains valid; the state interpretation does not.
- This does **not** prove 3+ campaign co-op works. It gives a precise first patch
  candidate: raise the `CreateLobby` member cap, then observe whether extra clients can
  join and whether gameplay scripts recognise them.

### Exact patch candidate (batch 15, 2026-11-27)

- Disassembly confirms the cap is an immediate operand in
  `FUN_14046cd00` at VA `0x14046cdf8`: bytes `41 b8 02 00 00 00 8b d7`
  (`mov r8d,2; mov edx,edi`). The `2` is at VA `0x14046cdfa`, file offset
  `0x46c1fa` in `SRTTR.exe` (image base `0x140000000`, `.text` raw pointer
  `0x400`, RVA `0x46cdfa`).
- Candidate 3-player total patch: change the single byte at file offset
  `0x46c1fa` from `02` to `03`. Candidate 4-player total patch: change it to
  `04`. No other bytes in this instruction are believed to need changing.
- This is a static byte candidate only. It has not been applied to the owner's
  install, the isolated copy, or any test copy, and it has not been run.

### Join and script bindings (batches 13, 16, 2026-11-27)

- `FUN_14041b240` registers the game Lua bindings. Relevant mappings:
  `game_get_is_coop_client` -> `FUN_140415b60`,
  `game_get_is_host` -> `FUN_140415bc0`,
  `game_kick_coop_player` -> `FUN_1404163b0`,
  `game_coop_kick_player` -> `FUN_1404156a0`,
  `game_get_party_member_count` -> `FUN_140415450`,
  `game_get_coop_ping` -> `FUN_140415ab0`,
  `game_show_coop_partner_gamercard` -> `FUN_140416d00`,
  and `coop_is_active` -> `FUN_14063e390`.
- `FUN_140415b60` returns true only when `FUN_140452750()` is true and session
  `+0x68 != +0x60`; `FUN_140415bc0` returns true when `+0x68 == +0x60`.
  These fields behave as remote/local player pointers in the session object.
  `FUN_14063e390` (`coop_is_active`) simply returns `FUN_140452750()`.
- `FUN_140452750` gates on session `+0x138`, `+0x13c`, `+0x141` and
  `FUN_140469ec0`, then walks the remote-player list by byte ID. This is a
  coop-presence check, not an explicit player-count cap, but its dependence on
  one `+0x68` pointer is consistent with a single-partner design.
- `FUN_140415450` (`game_get_party_member_count`) always returns `0`; scripts
  cannot learn a 3rd player's presence through that binding.
- `FUN_140469b30` finds a player by byte ID at player `+0x168` in the circular
  list at session `+0x58` (stride `0xb2c0`). `FUN_140464ad0` maps a byte ID
  into a packet table at packet `+0x18`, stride 5, bounded by `+0x20`. The
  replication layer therefore has generic multi-ID machinery, but gameplay use
  of all IDs is unverified.
- `FUN_14046b2f0` resets `+0x138` to `1` when `+0x68 == +0x60`; in the coop
  branch (`1 < +0x138` and `1 < +0x13c`) it walks every `+0x58` list entry and
  raises error `10` if a non-privileged player has an unexpected state. This is
  the clearest single-partner assumption seen in session management so far.
- The join handler `FUN_1404628a0` has cases `0`-`5`; case `5` creates a new
  remote player from a byte ID with `FUN_140453a80(...,local_38[0],...)` and no
  explicit two-player rejection. Steam `GetNumLobbyMembers` (`+0xa8`) is used
  in `FUN_14045a280` only to iterate lobby keys for `dlc_mask`, not to cap
  joins.
- `FUN_14028d4d0` loads `mission_checkpoints.xtbl` into a 166-entry
  `mission_checkpoint_sp` table under `sr3_city`. A co-op-specific checkpoint
  data path has not been identified. `save_system_cancel_coop_load` is bound
  to `FUN_140412360` (not yet decompiled). `mission_set_checkpoint` is bound to
  `FUN_14064e2c0` (not yet decompiled).

### Patch-site verification and state-3 packet path (batches 17-18, 2026-11-27)

- `local/analysis/verify_patch_site.py` verifies the isolated exe hash and the
  bytes at file offset `0x46c1f8` without writing any file. The pattern
  `41 b8 02 00 00 00 8b d7` occurs **twice** in the binary (the other offset is
  `0x7a620b`), so the patch must be applied by exact file offset, never by
  blind find/replace. No file was modified.
- In-memory patched hashes (derived, not applied): cap `3` -> SHA-256
  `044815A3901F36EF876481CAE385E3B0E953AD567A055F0977DA061838354BCE`; cap `4`
  -> SHA-256 `7E0E31A261C323781D53C45A99FC440433B64EF59C65B08042C7925811724838`.
- `FUN_140474f20` is just a getter for global packet pointer `DAT_141d74010`.
  The `2 < value` branches in `FUN_1404750f0` / `FUN_140475140` therefore use
  the current join packet, not an unimplemented stub. They require the packet
  to match the current session (`FUN_14046a770`) and all mission-state flags in
  `FUN_1402e17d0` to be zero. Batch 20 later showed `2 < value` is a session-state
  check (state 3), not a player-count check.
- `FUN_1402e17d0` checks eight globals `DAT_1415b3cb1`, `cb2`, `cb3`, `cb4`,
  `cb5`, `cb6`, `cb7` and `cba`; their xrefs include mission/coop state
  functions such as `FUN_1402e2dd0`, `FUN_1402e2ec0`, `FUN_1403fee90` and
  `FUN_1404637a0`. Exact meanings are unknown, but this gate means the state-3
  packet path is skipped while those states are active.
- `FUN_140464aa0(packet,player)` returns `0xff` when the packet's ID table is
  empty or the player's byte ID is not marked valid, otherwise the table byte.
  `FUN_14047e9c0` only serialises/sends a join packet when that result is `>1`.
  This is generic player-ID addressing, not a two-player check.
- `FUN_140412360` (`save_system_cancel_coop_load`) only sets global
  `DAT_141adfd64 = 5` if it is not already `5`; it has no player-count logic.
- `FUN_14064e2c0` (`mission_set_checkpoint`) calls `FUN_140283bf0` and exposes
  no player-count check. `FUN_140283bf0` replicates the checkpoint only when
  session `+0x68 == +0x60` (host/no remote partner), using `FUN_14045b590`; no
  single-partner target address is visible in that path. Runtime authority is
  still unverified.
- `FUN_14046ca20` resets session field `+0x13c` to `1` on one callback path
  (`param_2 == 2`) and leaves the lobby. Batch 20 reclassified `+0x13c` as an
  expected-state field, so this is a state reset, not a player-count reset. No
  patch is proposed until a 3-client run shows whether it causes a failure.

### CORRECTION: `+0x138` / `+0x13c` are session states, not player counts (batch 20, 2026-11-27)

Earlier entries in this file, and the first draft of the write-up, called
`+0x138`/`+0x13c` "current/expected total player counts" and read `2 < value` as a
latent 3+ player path. That was wrong and is superseded by this section.

- The only writes in the network cluster are the literals `1`, `2` and `3`:
  `+0x138 = 1` in the reset path (`FUN_14046b2f0`), `+0x138 = 2` when join
  completes (`FUN_14046cad0`), `+0x138 = 3` when the session starts
  (`FUN_14046dcf0`); `+0x13c = 2` in `FUN_14046d980` / `FUN_140469ed0`, and
  `+0x13c = 3` in `FUN_14046ba00`. `FUN_140468530` restores `+0x13c` from a
  saved value at `+0x144`.
- The pervasive `+0x138 == +0x13c` checks are therefore current-state equals
  expected-state, not player-count equality. `FUN_14046ce90` explicitly returns
  `+0x138 == 2`. A plausible enum is `1` idle, `2` joined/loading, `3` started;
  the exact names are unverified.
- Consequently `2 < value` means "state 3 / started", not "more than two
  players". `FUN_140474f20` and the `FUN_1404750f0` / `FUN_140475140` branches
  are state-3 join-packet paths, not latent 3+ support.
- The hard `CreateLobby(..., 2)` cap remains a valid static finding and the
  one-byte patch candidate stands. What is **not** supported is the earlier
  claim that the native session layer already contains a 3+ player path.
  Deeper changes are likely needed for the single `+0x68` partner pointer, the
  state machine and partner-oriented script bindings. No runtime test has run.

### Checkpoint send is broadcast to the whole remote list (batches 21-22, 2026-11-27)

This refines the earlier checkpoint note; it does not restore the retracted
"latent 3+ player path" claim.

- `FUN_140283bf0` (`mission_set_checkpoint` path) builds the checkpoint packet
  only when `FUN_14046a740()->+0x68 == FUN_14046a740()->+0x60`, i.e. on the
  host/no-privileged-peer path. It then calls
  `FUN_14045b590(&packet, session, 0, 0)`.
- With the third argument null, `FUN_14045b590` calls `FUN_140460740(session,
  packet)` or `FUN_140460820()` according to the packet channel flag. Both
  helpers walk every entry in the remote-player circular list at session `+0x58`
  (stride `0xb2c0`) and send to each entry that passes its state checks. There
  is no single-partner target address in that path.
- Therefore the earlier wording "replicates the checkpoint only to the single
  partner" was too strong. Authority is host-only, but delivery is to all
  remote players registered in the list. Whether a third client actually gets
  registered and accepts the packet is still untested.
- `FUN_1404696c0(session, player)` increments session `+0x70` and inserts the
  player into that same circular list. `FUN_140452750` (used by
  `coop_is_active` and the `game_get_is_*` bindings) requires `+0x70 >= 2`, so
  `+0x70` appears to be a session member counter rather than a state field.
  Its exact baseline and maximum are still being traced.
- Send helpers use `session->+0x68` as the privileged peer pointer for
  channel/connection checks even while iterating other players. That remains
  the clearest single-peer assumption in replication.

### Member counter and send-path checks (batches 23-24, 2026-11-27)

- `FUN_140467d80` initialises a session by creating one player with byte ID `0`,
  setting `session->+0x68 = session->+0x60 = local_player`, zeroing the 32-bit
  counter at `+0x70`, then incrementing `+0x70` to `1` and inserting that local
  player into the `+0x58` circular list.
- `FUN_1404696c0` increments `+0x70` for every remote player inserted into the
  list; `FUN_14046b690` decrements it when a player is removed;
  `FUN_140469ec0` is just a getter. `FUN_140452750` requires `+0x70 >= 2`
  (local plus at least one remote) for `coop_is_active` and the `game_get_is_*`
  bindings. No hard limit of `2` was seen in this counter; it is a 32-bit
  generic member count.
- `FUN_1404535a0(player)` returns true only when that player equals
  `session->+0x60` (the local player). `FUN_140460a80(channel,
  session->+0x68)` checks channel readiness against the privileged peer
  pointer, while the actual sends in `FUN_1404535b0` and `FUN_1404539d0`
  operate on the individual player object passed in. The broadcast helpers
  therefore can target every remote list entry even though the readiness
  check consults one privileged peer.
- Reliable send rejects player state `>1`; unreliable send rejects state `>2`.
  State values `0`, `1`, `3` and `0xc` were already observed; these checks are
  per-player gates, not player-count limits.
- Net effect: replication plumbing looks capable of more than one remote
  player. The unresolved risks are the privileged `+0x68` pointer, the
  single-partner script bindings and the 2014 developer warning about
  two-player gameplay assumptions. No runtime test has run.

### Privileged pointer semantics and a two-player reconciliation loop (batch 26, 2026-11-27)

- Correction to the batch 16 label: `FUN_14046a5a0` is not a peer-setter. It is
  the session teardown path; it removes players, clears `+0x68` to `0`, clears
  `+0x138`, and returns. The only non-zero initialisation of `+0x68` seen so far
  is in `FUN_140467d80`.
- Steam callback `FUN_14046d180` case `2` (lobby-enter/join path) creates a
  player with `FUN_140453a80(..., *(session + 0x3c), ...)`, assigns it to
  `session->+0x60`, and inserts it with `FUN_1404696c0`. Case `1`
  (lobby-created/host path) does not rewrite `+0x60`. Therefore the best
  current reading is: `+0x60` is the local player and `+0x68` is the
  host/authority player. They are equal on the host and differ on a client.
  That fits `FUN_140415bc0` (`game_get_is_host`: equal) and
  `FUN_140415b60` (`game_get_is_coop_client`: not equal). Earlier wording
  calling `+0x68` a "remote partner" pointer was imprecise and is superseded.
- `FUN_140469d00` / `FUN_140469c30` choose identity data from `+0x68` or
  `+0x60` respectively and return `2` for unsupported mode paths. This
  supports the host/local role interpretation; it is not a member-count check.
- New concrete two-player assumption: `FUN_140468dc0` (called from the session
  reset path) reconciles player state using an explicit loop bounded by
  `bVar11 < 2` over bit positions in the `+0x48` masks of the `+0x68` and
  `+0x60` players. It resolves each bit to a player ID by scanning the
  `+0x58` list. This loop currently considers only two role slots. Raising
  `CreateLobby` to 3 may therefore not be sufficient; no runtime test has
  shown whether a third client is excluded here.
- `FUN_140465210` allocates packet-ID tables as `param_2 * param_3 * 5`; its
  caller passes `10` and a global, not a literal `2`. No player-cap evidence
  was found there.
- Net effect after batch 26: the one-byte lobby cap patch is still the clean
  first test, but a second two-slot assumption now exists in session state
  reconciliation. Keep QA `none` until a real multi-client run.

