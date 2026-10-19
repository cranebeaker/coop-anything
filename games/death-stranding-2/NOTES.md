# DEATH STRANDING 2: ON THE BEACH — research notes

Updated: 2026-10-19

## Game identity

The private identity record is in ignored `local/identity.md`. This investigation covers Steam PC `DS2.exe` 1.10.89.0 on Windows 11. The executable hash and install path are kept out of this tracked write-up. The analysis was performed on an isolated copy; no game files or saves were modified.

## Observed facts

- The Co-op Anything directory previously listed the original DEATH STRANDING as a Decima gap, but had no DEATH STRANDING 2 entry or write-up. The PlayStation developer interview identifies Decima as the engine used for the series.
- DEATH STRANDING 2's official PC FAQ describes the Social Strand System as asynchronous multiplayer: other players' routes and structures can appear and be used as areas are connected. The PC and PS5 versions share servers. Official Patch 1.1.53 notes add Steam friends to the SSS friends list. A Strand Contract is set through Ring Terminal → Social Strand Service → Bridge Links.
- The SSS connects separate playthroughs; it is not a shared live game session.
- The executable imports Steam API, WinHTTP, and Windows socket functions. Embedded strings include `MultiplayerGroup`, `MatchmakingGroup`, `ReplicationMode`, `UseSimpleNetworkReplication`, `NetReplicatedFactManager`, `MsgEntityUpdate`, and `MsgMissionPlayerNotify`. These names alone do not prove that DS2 synchronizes live player gameplay.
- Targeted Ghidra references and decompilation in the inspected executable confirm registration code for a `MultiplayerGroup` API with `GetNetworkTime` and `IsGameServer`, a `MultiplayerGameLibGroup` API with `IsDedicatedServer`, and a `MatchmakingGroup` API with matchmaking enter/cancel, game start/end, party ID, and team/faction switching callbacks. The matchmaking enter callback updates state and calls internal helpers. These findings establish generic multiplayer and matchmaking machinery in the binary; the runtime purpose and relation to playable co-op remain unknown.
- Steam interface lookup wrappers contain direct code references to `SteamClient020`, `SteamUser023`, `SteamFriends017`, and `SteamUtils010`. The targeted scan found no direct code references to `SteamNetworkingMessages002`, `SteamNetworkingSockets012`, `SteamNetworkingUtils004`, `SteamMatchMaking009`, `SteamMatchGameSearch001`, or `SteamParties002`. This was a targeted code-reference scan, not proof that no networking interface can be reached dynamically or through another transport.
- `SequenceNetworkEntity` is present in the executable, and validated code references use it in a name/CRC lookup pattern alongside sequence/cinematic type names. Treat it as a likely sequence/reflection type, not as evidence of player replication.
- `MsgEntityUpdate` and `MsgMissionPlayerNotify` are present as message/class names, but the direct MOV/LEA reference scan found no code references to those exact strings. A message-name string is not proof of a packet or live network message.

## Research performed

- Pulled the public repository and checked its directory, game write-ups, issues, and pull requests. The GitHub public API exposed 35 issues and 16 PRs; none matched Death Stranding, Kojima, or Decima. The `gh` CLI was unavailable.
- The repository MCP `search_directory` search for `Death Stranding 2` returned no entry. Its read-only engine scan covered 184 copied files and detected `steam_api64.dll`, but could not identify the engine from file signatures. The official developer interview identifies Decima, so the signature scan is inconclusive rather than contrary evidence.
- Searched GitHub's Death Stranding 2 topic, web-indexed GitHub repositories, Nexus Mods, ModDB, Steam Community guides/discussions, Reddit, Archive.org, and English, Japanese, and Simplified Chinese terms for a live co-op project. No DS2 live co-op attempt surfaced in this bounded search. Discord was not searched; this is not proof no private, unindexed, or newly released project exists.
- Odradek is an actively maintained GPL-3.0 asset viewer/extractor that lists support for DS2 1.10.89.0; it is not a multiplayer base. The DS2 Mod Suite and other ASI-loader mods show a possible native plugin entry path, but none found in this search synchronizes players or gameplay state.
- Copied the game into ignored `local/death-stranding-2-test` (112.456 GB, 184 files). Its `DS2.exe` SHA-256 matched the private identity record. No game executable or save was changed.
- Ran read-only PE/import and string scans, then a targeted RIP-relative reference scan and Ghidra decompilation on selected functions in the isolated copy. Ghidra 12.1.4 and Temurin JDK 21.0.12.1 were downloaded from official release sources, SHA-256 checked, and extracted under ignored `local/tools`; no system PATH or game files were changed. Archive SHA-256: Ghidra `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`; Temurin `f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e`.
- Full automatic analysis did not finish. The first run spent excessive time in switch analysis and emitted invalid-reference warnings, so it was stopped and replaced with targeted, no-analysis imports and function decompilation. No whole-program decompilation, runtime tracing, game launch, or co-op test was completed.
- Posted the required claim as [issue #38](https://github.com/cranebeaker/coop-anything/issues/38). The local issue text remains in ignored `local/claim-issue-draft.md`.
- Set up the repository MCP with an isolated Python 3.12 environment and PyYAML 6.0.3, then added its server entry to the user's Codex configuration. TOML parsing and an MCP `initialize` request succeeded; restart Codex to load the server.

## Assessment

- Current supported way to connect with friends: use SSS and create Strand Contracts. On Steam, use a build with the Patch 1.1.53 change so Steam friends appear in the SSS list.
- Inference: a true simultaneous co-op mod likely starts from zero unless further research finds an existing project. The repository has no reusable Decima co-op base, and the bounded public search found no DS2 live co-op project.
- The inspected binary has generic multiplayer/matchmaking API registration and replication-related identifiers, but the evidence does not show an active DS2 live-session path, two-player simulation, or a usable gameplay hook. Steam's direct interface references found in the targeted scan are friends/user/client/utilities interfaces; this does not rule out custom or dynamically selected networking.
- ASI loading is a candidate mod entry path based on existing DS2 mods, not a verified hook into gameplay or networking.
- No feasibility verdict, engine hook point, multiplayer state model, save behavior, or co-op implementation has been verified. No game was run.

## Unknowns / next gates

- Whether an unindexed, private, or newly released DS2 co-op project exists; community Discord channels were not searched.
- Which matchmaking callbacks are used in a normal play session, what online service owns them, and whether any state can represent a remote player.
- Whether the generic replication metadata is used for gameplay networking, and how levels, cutscenes, enemies, missions, and saves could work with a second player.
- A suitable hook and safe test plan for build 1.10.89.0. Work remains static and limited to the isolated copy.

## Sources

- Co-op Anything [AGENTS.md](https://github.com/cranebeaker/coop-anything/blob/main/AGENTS.md), [directory](https://github.com/cranebeaker/coop-anything/blob/main/DIRECTORY.md), [engine-family guide](https://github.com/cranebeaker/coop-anything/blob/main/playbook/engine-families.md), [research guide](https://github.com/cranebeaker/coop-anything/blob/main/playbook/research.md), and [reverse-engineering guide](https://github.com/cranebeaker/coop-anything/blob/main/playbook/reverse-engineering.md).
- KOJIMA PRODUCTIONS [PC announcement](https://kojimaproductions.jp/index.php/en/deathstranding2_feb_announce).
- PlayStation [developer interview](https://blog.playstation.com/?p=407261).
- Official Steam [PC FAQ](https://store.steampowered.com/news/posts/?enddate=1772732685&feed=steam_community_announcements).
- Nixxes [PC Patch 1.1 notes](https://support.nixxes.com/hc/en-us/articles/50286010105363-DEATH-STRANDING-2-ON-THE-BEACH-PC-Patch-1-1).
- KOJIMA PRODUCTIONS [Social Strand System and Bridge Links guide](https://kojimaproductions.jp/index.php/en/deathstranding2-SupportRookiePorters).
- GitHub [Death Stranding 2 topic](https://github.com/topics/death-stranding-2) and ShadelessFox [Odradek asset viewer/extractor](https://github.com/ShadelessFox/odradek).
- Nexus Mods [DEATH STRANDING 2 mod list](https://www.nexusmods.com/games/deathstranding2onthebeach) and [DS2 Mod Suite](https://www.nexusmods.com/deathstranding2onthebeach/mods/72).
- Steam Community [PC FAQ](https://steamcommunity.com/app/3280350/discussions/1/758429196899977361/) and [guide listing](https://steamcommunity.com/app/3280350/guides/).
- Official [Ghidra 12.1.4 release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.4_build), versioned [Getting Started requirements](https://github.com/NationalSecurityAgency/ghidra/blob/Ghidra_12.1.4_build/GhidraDocs/GettingStarted.md), and [Eclipse Temurin 21 release](https://github.com/adoptium/temurin21-binaries/releases).
