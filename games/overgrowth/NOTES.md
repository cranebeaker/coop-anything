# Overgrowth: research notes (phases 0 and 1)

Started 2026-10-25. Layout follows coop-anything AGENTS.md. Private identity data is in `local/identity.md` (git-ignored).

## Phase 0: triage answers

1. **Which game.** Overgrowth, Wolfire Games, Steam release 2017-10-16 (early access from 2008). Steam app 25000. The owner's copy is Steam; the last run on this PC was build `1.5.0_indev-1588-g2f10f7ec5_build-470` (2022-03-01). The game is not installed at the moment (see `local/identity.md`). Also sold on Humble and itch.io; only the Steam build has beta branches.
2. **Already done?** Not in coop-anything: no `games/overgrowth`, no directory entry, 0 issues and 0 PRs matching "overgrowth" (checked 2026-10-25 via the GitHub web search pages). Outside the hub: yes, several attempts, see Phase 1.
3. **Engine.** Phoenix, Wolfire's own C++ engine with AngelScript game logic. Open source since 2022-04-21 under Apache-2.0: https://github.com/WolfireGames/overgrowth (code only; art, levels and audio stay with the purchased game). Upstream last commit 2025-03-20; 21 open PRs, none about networking.
4. **Source.** Full engine source plus all shipped scripts (`Data/Scripts/*.as`). Best case in the triage table.
5. **Engine multiplayer.** Yes. The engine contains a complete host/client layer (`Source/Online/`, ~5.5k lines, 50 message types) over Steam networking sockets (`Source/Network/Steam/steam_net_framework.cpp`, `isteamnetworkingsockets.h`), with a Basic socket fallback. Wolfire shipped it as the "Online Multiplayer Prototype" on 2021-12-15 (Steam beta branch `internal_multiplayer_testing`). The AngelScript API is `Online_Host`, `Online_Connect(ip)`, `Online_IsHosting`, `Online_IsClient`, `Online_RegisterState` / `Online_SendState` (script state sync), `Online_ActivateInviteDialog` (Steam overlay invites).
6. **Shipping check.** Code is Apache-2.0, so a modified engine build can be shipped as a zip the player drops next to their own purchased data. No anti-cheat. The only online service is Steam itself. Workshop mods must declare `<SupportsOnline>true</SupportsOnline>` in `mod.xml` or the engine refuses to host (`online_utility.cpp`).

**Difficulty row that applies:** "Decomp or open source port exists" plus "Engine has MP plus script source". Expected path: build on the existing netcode, not from zero.

## Phase 1: existing work (every attempt found)

### A. Wolfire's official online prototype (2021)
- Source: https://www.wolfire.com/blog/2021/12/Overgrowth-Online-Multiplayer-Prototype/ (2021-12-15); Steam news lists the same post and no multiplayer post since (newest news 2023-02-10).
- How to get it: Steam > Overgrowth > Properties > Betas > `internal_multiplayer_testing` (branch name from the Steam news post; the exact current branch list is **unknown**, SteamDB blocked the fetch).
- What it is, from the code (observation, `Source/Online/online.cpp`, `movementobject.cpp`): the host simulates everything. Clients send raw input (`PlayerInputMessage`) to the host and receive `MovementObjectUpdate` poses back, interpolated by `TimeInterpolator`. A client never runs its own character's physics (`ASSetCharAnimation` returns early on clients). So every move waits a full round trip, which matches the player reports of "serious lag and input delay" (Steam thread "Multiplayer coop campaign", 2022-06/07).
- Campaign support: the multiplayer menu (`Data/Scripts/multiplayer_menu.as`) only lists campaigns whose `mod.xml` says `supports_online="true"`. The stock campaign mods do not (the fork's comment: "The 1.4 data marks none as online"), so stock players only see arena/versus levels. Dialogue, damage flashes, kills and level loading are already synced in the scripts (`dialogue.as`, `aschar.as`, `overgrowth_level.as`), i.e. the campaign was partly wired up but never exposed.
- Status: `official`, `alpha`, abandoned upstream (no commits touching `Source/Online` after the 2022 open-sourcing; Steam thread 2022-07-09: "the devs haven't really done much with it").
- Bug reports: Steam discussions, the Wolfire Discord (`discord.gg/Wolfire`, not read: needs an account, see discord-export.md rules).
- Known player-reported problems: client lag/input delay (2022), arena crash at loading screen for the client (2022-06-27), campaign co-op only through a mod (below).

### B. crxhvrd/overgrowth-online, "Overgrowth Online" by the CoreFX modding team (2026)
- https://github.com/crxhvrd/overgrowth-online, fork of WolfireGames/overgrowth, Apache-2.0. Created 2026-10-16, 216 commits (23 ahead of upstream `main` at merge-base `245fe48`, 2025-03-20), last push 2026-10-24, one author (`crxhvrd`). 0 stars, 0 forks, 0 issues. No Discord or site found by search (2026-10-25).
- Release: tag `Release`, "Build 0.40.0" (2026-10-23), asset `OvergrowthOnline_0.40.0.zip`, 131,255,364 bytes, 3 downloads, no release notes, no checksum published. Commits since then go to 0.41.3. An earlier tag `0.38.2` was a "Public Test" release.
- Claims (README, not verified by us): local simulation of the player's own character (no input lag), ~35 kbit/s per player, defender decides hits, host validates messages, weapons and footsteps synced, Steam friends joining, server browser, direct IP, story co-op and arena PvP with bots. Windows only; renderer rebuilt (OpenGL 4.6 / Vulkan 1.3, DLSS/FSR).
- What the diff shows (observation, `git diff 245fe48..fork/main`): `Source/Online` + `Source/Network`: 60 files, +5,645/-582; `MovementObjectUpdate` deleted, replaced by `character_snapshots`, `char_snapshot`, `clock_sync`, `combat_messages`; a new `GNS` net framework on open-source GameNetworkingSockets 1.4.0 (`USE_GNS_NETWORK_FRAMEWORK`) beside the Steam one. `Data/Scripts`: 59 files changed, +3,738/-577, including `aschar.as`, `enemycontrol.as`, `dialogue.as`, `level.as`, 30 hotspot scripts, new `coop_story.as` (host gives guests the host's starting weapons and knockout shield), `coop_revive.as` (drag a downed teammate 3 s to revive), `multiplayer_browser.as`, `multiplayer_join_friend.as`, `multiplayer_character_menu.as`, `online_arena.as`.
- Story co-op selection: `multiplayer_menu.as` `CoopCampaign()` lists every linear campaign (Overgrowth, Lugaru, Drika's Story, Therium-2) whether or not its `mod.xml` says online; the engine side has an `Online::CoopCampaign` rule set.
- Ships a bundled "Overhaul" gameplay mod (`mod/overgrowth-online/mod.xml`, replaces `aschar.as`, `enemycontrol.as`, `situationawareness.as`; optional but must match across players) and a test-only mod `test/mods/mptest` whose level hook hosts or connects from config keys `mpt_role` / `mpt_ip` and logs `[MPT]` lines (session state, players, positions). That is the start of an in-game bot in the hub's sense.
- Status: `fork`, `alpha` (daily builds, 9 days old). QA: `none` (author claims only).
- Unknowns: whether any level transition, dialogue or checkpoint works for a guest in a real session; whether the GNS framework lets two copies on one PC host/join by IP without Steam; whether the release zip is a full engine build or a patch; whether the author wants help (no CONTRIBUTING of their own, upstream's copied).

### C. "Full Coop (Campaign, arena...)" by Catahrai, source by Denperidge (2021)
- Steam Workshop id 2637639111 (posted 2021-10-26, 6,087 subscribers, 28 comments); source https://github.com/Denperidge/Overgrowth-Full-Local-Coop (MIT, 6 commits, 4 open issues).
- What it does (from `startCoop.as`): a level hook that duplicates the player's character (F9 adds, F10 removes, up to 4) for **local** split-screen; "online" only through Steam Remote Play Together. Nothing network-related in the script.
- Players report (Workshop comments): works as of 2023-12; over Remote Play both players sometimes control the same character; player 2 respawns at level start on death.
- Status: `mod`, `playable` for split screen, `community-reports`.
- Note: the anonymous Workshop fetch showed a "removed from the community" banner on this page and on D. **Unverified**; check while logged in to Steam before listing it as dead.

### D. "Overgrowth Story Multiplayer" by StealthDrifter (2025)
- Steam Workshop id 3496994351 (posted 2025-06-11, 646 subscribers, 33 MB). Adds the campaign to the official prototype's multiplayer menu (needs the multiplayer beta branch; host from Multiplayer > Campaign, then pause and press the invite button, per the author's comment).
- Author's known bugs: latency "rough" as the client; the client can move during dialogue; a kill barrier hit by the client kills the host instead; the client cannot load the next level until the host finished loading. Comments: black screen / crash when joining, frozen camera for the joiner, fixed camera in "passenger" sections.
- No source repo found (searched GitHub and web, 2026-10-25). License none stated.
- Status: `mod`, `partial`, `community-reports`. These bugs are the best existing bug list for the official prototype's campaign path.

### E. OGMP (2015-2018)
- https://github.com/ogmp/ogmp_client (GPL-2.0, 105 commits) + ogmp_server, for pre-1.0 alphas: presence, chat and positions over a Linux server, no shared enemies or story. Dead (server gone per a 2020 Steam post). Status `abandoned`.

### F. Lugaru campaign co-op mod by DoctorGester (2017)
- Wolfire blog "Mod Highlight: Lugaru Campaign Co-op mod" (2017-07). Split screen for the Lugaru campaign inside Overgrowth; a 2023-02 Steam post says a pair finished the campaign that way with Remote Play. Local only.

### searched:
GitHub repo and code search (overgrowth multiplayer / coop / netcode, forks of WolfireGames/overgrowth), Steam Workshop pages, Steam discussions (4 threads), Wolfire blog and Steam news, Wolfire wiki FAQ, Wolfire forums (403, not readable), SteamDB (403), ModDB (not checked), Nexus (not checked), Reddit (not checked), Wolfire Discord (not checked, needs account), all 2026-10-25. Non-English searches not done (fan base is English-speaking).

## Derived (from reading code, not yet run)

- The upstream prototype's lag is architectural (thin client), so "fix the official prototype" means rewriting its movement replication. The fork already did that rewrite. Reviving the fork's work is therefore the shortest path; building from zero would repeat it.
- The hub's QA shape fits well: the engine is scriptable (AngelScript level hooks run inside the game), logs to `%USERPROFILE%\Documents\Wolfire\Overgrowth\logfile.txt`, takes config keys from `config.txt`, and the fork's `mptest` hook already hosts/connects from config. A launcher that starts two copies with separate user-data folders and reads both logs is the next piece.
- One-PC testing: the Steam framework ties a copy to the logged-in Steam account; the fork's GNS framework (plain UDP) should allow host + client on one PC by IP, if the build flag is on in the release. Must be verified by running it.
- Two copies of the game on one PC need separate user-data dirs. The engine reads `%USERPROFILE%\Documents\Wolfire\Overgrowth` by default (observed in the log), and `Source/Main/main.cpp` (upstream, TCLAP) accepts `--write-dir <dir>`, `--working-dir <dir>`, `--level <file>`, `--config <string>`, `--no-dialogues` (automatic dialogue responses), `--disable-rendering`, `--quit-after-load`, `--clear-log`, `--run-unit-tests`. So a test launcher can give each copy its own write dir, config, log and start level without touching the owner's folder. Whether the fork kept these switches: not yet checked (its `main.cpp` diff is pending).

## Guesses
- The owner's last-used Steam branch was the multiplayer beta (the log lists `com-wolfire-versus-online`, which only ships there). Unconfirmed.
- The "removed from the community" banners on both Workshop pages are an artifact of anonymous fetching, not real removals (6k subscribers, comments as late as 2023-12). Unconfirmed.

## Unknowns to close next
- Game identity hash (needs a reinstall).
- Does the fork's 0.40.0 zip run against the current Steam `Data/`? Which branch's data does it expect (its COMPILING.md says `internal_testing`)?
- Does story co-op in the fork survive a level transition, a dialogue and a checkpoint for the guest?
- Max players in story co-op (engine reads the level's "Player Limit" param; default **unknown**).
- Whether the fork's author accepts PRs or wants the work upstreamed.

## Phase 2/5 log: first runs of the fork build (2026-10-25)

Setup: two hard-linked test copies of the Steam install (`tools/make_test_copy.ps1`: Data/ linked, root files real copies), the fork's `install.ps1 -GamePath <copy> -WriteDir <copy>\write` applied to each, windowed 960x540. Launch: `OvergrowthOnline\OvergrowthOnline.exe --write-dir <write> --test-bridge <port> --clear-log`. Helpers: `tools/bridge.ps1`; scenario: `tools/coop_smoke.ps1`.

Observations:
- Fork release 0.40.0 runs as `Version e2168f3_build-2001 2026-10-21`, OpenGL 4.6, Steam API connects. Two instances run side by side on one PC under one Steam account; both Steam inits succeed.
- The release build's transport is the fork's GNS framework (log: `gns_net_framework.cpp`), and it tries Valve's SDR config fetch (`HTTP 504`, retried). Host listens on `mp_port` (bridge `host {port}`), client `connect {ip, port}` to 127.0.0.1 works: `GNS: connection 1 -> Connected`.
- The test bridge works as documented (ping, status, state, host, connect, start_session, quit verified). `status.online` carries peers, players, byte and message counters, hit and takeover counters; `state` carries every character with `controlled`, `remote`, `proxy`, `is_player`, position, health.
- Host side: `host {campaign, level}` loaded `og_story/01_Slaver_Camp_Landing.xml` in 18 s, `waiting_for_input` true until `start_session`; afterwards the host owns avatar 0 (`controlled`, not `remote`).
- Log noise on every start: machine-translations mod fails its thumbnail check (also in the stock game), `campaign_common.as` include unresolved, missing tree prefabs, no navmesh for the level in the write dir. None stopped the level.
- After one host run, the real Steam install's 21,411 Data files matched the size+mtime manifest and no new root files appeared.

**Bug 1 (confirmed by logs, root cause from code): join refused with "You are not using the same mods the host is" although both copies had identical mod configs.** `StateHandshake::OnUpdate` (`Source/Online/online_connection_states.cpp`) compares `actor->active_mods_string` with `OnlineUtility::GetActiveModsString()` as plain strings; that string is built in `ModLoading::GetAllMods()` order, and Steam Workshop mods are appended in the order the UGC callbacks arrive (host log: steamworks mods 2,1,6,5,7,8,3,4; client log: 4,6,7,1,3,2,8,5). Same set, different order, rejected. Fix: sort the ids (both sides) before comparing. Second cause of the same symptom: the fork's installer writes a `modconfig.xml` without the player's Workshop subscriptions, so the engine activates every subscribed Workshop mod on first start (the host's rewritten modconfig showed five `activated="true"` steamworks rows), against the README's advice to disable them; the installer should carry over the stock `modconfig.xml` activation states, or write them as deactivated. Workaround used for testing: explicit `activated="false"` rows for all eight subscriptions in both write dirs.

## Decision (proposed to the owner)
Path 1, "finish what exists", on crxhvrd/overgrowth-online: it is the only project with locally simulated players and a story co-op mode, it is Apache-2.0, and it is active. Treat Wolfire's prototype as the reference and StealthDrifter's bug list as the first regression suite. Use the hub tools (`gameslot`, `gamepool`, `chaos`, `bots`) around the fork's `mptest` hook.

**Run 2 (Workshop mods deactivated on both sides): the join works.** `connect` -> `GNS: connection 1 -> Connected`, client loaded the host's level in 6 s, `start_session` on the host, and both machines list both players (host avatar 0, guest avatar 1309). Host log: `co-op story rules on`, then `cutscene, player character 1309 hidden`; 32 s later `cutscene over, player character 1309 beside the host's 0` (the fork's `CoopCutsceneUpdate`: guests are hidden during the host's dialogue scene and stood up beside the host afterwards). Link line on the host: `ping 0 ms, quality 1/-1, out 509 B/s, in 621 B/s` with one idle guest. The bridge's `input {hold: {skip_dialogue: 1}}` on the host advanced the dialogue; `level_messages` shows the fork's own `mp_role host #0 2 #0 <name> #1309 <name>` and `ohl ...` (Overhaul) messages.

Observation worth a note in the write-up: on the guest's machine its own avatar reports `controlled=true, remote=false, proxy=false` but `is_player=false`; on the host the same avatar is `is_player=true, remote=true, proxy=true`. Tests must judge ownership by `controlled && !remote`, not `is_player`.

Also: the client log repeats `Tried to get own players state, but there is no state registered for player id: 0` (GetOwnPlayerState before the host assigned the player id) dozens of times per second during the join. Harmless but noisy; a candidate cleanup for the fork.

**Movement sync measured (run 2, both copies on one PC, loopback):** the guest walked forward for 2 s on injected input (`input {hold:{up:1}, steps:240}`). Sampled every 0.5 s from both bridges, the host's copy of the guest's avatar trailed the guest's own position by 0.42 m while moving (the proxy's `net_delay_ms` was 47 to 57 ms) and converged to 0.00 m within a second of stopping. Peer ping 9.8 ms, clock offset 46 ms. Traffic after about 4 minutes with one guest: host out 3.26 MB / in 0.62 MB, 6,423 snapshot records out, 1,729 in, 0 takeovers. In-game screenshots via the bridge's `screenshot {path}` work (the game's own capture, not the desktop).

**Level change (tools/coop_level_change.ps1, 2026-10-25): PASS.** Host `load_level og_story/02_Slaver_Camp.xml` while hosting level 1 with one guest: host in level 2 after 5 s (`waiting_for_input` again, `start_session` needed), the guest followed within the same poll, host log `Link peer 1` continues, both machines list 2 players, both avatars back in the scene (new ids: host 21, guest 915) after the level-2 cutscene. First run of the script failed on my own assertion (read `online` from a `state` reply instead of `status`); fixed and rerun. Also the `coop_smoke.ps1` second run: PASS (guest moved 15.7 m as seen by the host, gap 0.00 m, ping 12.4 ms).

**Hub tools (Python 3.12 via winget, 2026-10-25):** `python -I tools/test_all.py`: PASS scenarios, bots, animcheck, inspect, buildlock; bridge passes its Python tests; chaos, flight, ownership, fakeworld, fuzz, hotreload, mutants and the C++ parts need cmake, which is not installed (not needed yet). `tools/build_directory.py` needs PyYAML (installed with pip). Directory: five Overgrowth entries added, `--check` passes (316 entries).
