# Overgrowth (2017)

| | |
|---|---|
| Engine | Phoenix (Wolfire's own C++ engine with AngelScript game logic), open source since 2022-04-21, Apache-2.0 |
| Versions tested | Steam, `public` branch, build id 3450449 (data); Overgrowth Online fork release 0.40.0 (`Version e2168f3_build-2001 2026-10-21`), Windows 11 |
| Projects | [Overgrowth Online (CoreFX)](https://github.com/crxhvrd/overgrowth-online) (base), [Wolfire's 2021 online prototype](https://www.wolfire.com/blog/2021/12/Overgrowth-Online-Multiplayer-Prototype/), [Overgrowth Story Multiplayer](https://steamcommunity.com/sharedfiles/filedetails/?id=3496994351), [Full Coop (split screen)](https://steamcommunity.com/sharedfiles/filedetails/?id=2637639111), [OGMP](https://github.com/ogmp/ogmp_client); all five in the directory |
| QA level | automated (two scenarios with two real game clients on one PC, see QA) |
| Last updated | 2026-10-25 |

## Where things stand

Two people can play the story campaign together today on the Overgrowth Online engine fork: a host starts a story level, a friend joins through Steam friends or by IP, each player's character runs on their own machine, and both follow the host across level changes. This was proven by scripts driving two real game clients on one PC for the first two story levels. Dialogue choices, checkpoints, deaths and revives, enemies and items in co-op are not yet checked, the fork is nine days old with one author, and its current installer breaks joining for anyone with Steam Workshop subscriptions unless those are disabled on every machine.

## Existing work

- **Wolfire Online Multiplayer Prototype** (official, 2021-12-15, Steam beta branch `internal_multiplayer_testing`, abandoned after the 2022 open-sourcing). The code is `Source/Online/` of [WolfireGames/overgrowth](https://github.com/WolfireGames/overgrowth): host-simulated thin clients over Steam networking sockets. Clients send input and receive poses, so every move waits a round trip; players reported heavy input lag (Steam thread, 2022-06/07). The campaign is not exposed in its multiplayer menu because the stock campaign data never declares online support.
- **Overgrowth Online (CoreFX)**, [crxhvrd/overgrowth-online](https://github.com/crxhvrd/overgrowth-online), Apache-2.0, created 2026-10-16, daily builds, release 0.40.0 on 2026-10-23. A fork of the engine that replaces the movement replication (local player simulation, character snapshots, clock sync, GameNetworkingSockets transport) and adds a story co-op mode, teammate revive, a server browser, Steam friend join and direct IP, plus a test bridge for automated tests. Syncs: players (verified), level changes (verified), everything else per the author only.
- **Overgrowth Story Multiplayer** (Workshop, StealthDrifter, 2025-06-11). Exposes the campaign on Wolfire's prototype. Its author's bug list (rough latency, the client moving during dialogue, kill barriers killing the host, the guest waiting for the host's load) is the regression list for the fork. No source found.
- **Full Coop (Campaign, arena...)** (Workshop, Catahrai, 2021-10-26; MIT source by [Denperidge](https://github.com/Denperidge/Overgrowth-Full-Local-Coop)). Split screen only (F9 adds a player, up to 4); online only through Steam Remote Play Together. Players report it working as of 2023-12.
- **OGMP** (2015 era, GPL-2.0): presence, chat and positions through a Linux server. Dead.

## How the game works (for co-op)

Full engine source plus every shipped script (`Data/Scripts/*.as`). The engine already has a host/client layer: 50 message types, host authority over objects, `Online_RegisterState` / `Online_SendState` for script state (dialogue steps, damage flashes and kills are already synced in the stock scripts), Steam networking sockets with a basic socket fallback. Campaigns are mods (`Data/Mods/<name>/mod.xml`), and a campaign or level is offered for online play only if its `mod.xml` says `supports_online="true"`. The user data folder (`%USERPROFILE%\Documents\Wolfire\Overgrowth`) holds config, saves, logs and installed mods; the engine accepts `--write-dir`, `--level`, `--no-dialogues`, `--disable-rendering` and `--clear-log`, and the fork adds `--game-data <install>` and `--test-bridge <port>`. Mod lists must match between host and guest or the join is refused.

## What we changed

A fix sent to the fork as [crxhvrd/overgrowth-online#1](https://github.com/crxhvrd/overgrowth-online/pull/1) (branch `fix/mod-list-order` on the owner's fork), verified with the harness: the shipped build fails the join only when the same mods are found in a different order, the fixed build passes both ways. The report:

- **Join refused with "not using the same mods" between identical installs.** The host compares the active-mod list as a string built in mod discovery order; Workshop mods are discovered in callback order, which differs per process. Also, the fork's installer writes a mod config without the player's Workshop subscriptions, so the engine activates all of them on first start. Workaround: set every Workshop mod to deactivated on every machine. Fix: sorted comparison, and an installer that carries over activation states.

## QA

Harness: PowerShell scripts in `tools/` driving the fork's test bridge (JSON lines on a localhost port). `make_test_copy.ps1` builds a hard-linked test copy of the Steam install with its own write dir (the real install is only read and is checked against a size+mtime manifest after runs); the fork's `install.ps1 -GamePath <copy> -WriteDir <copy>\write` is applied to each copy; `bridge.ps1` holds the helpers.

- `tools/coop_smoke.ps1`: starts host and client, hosts `og_story/01_Slaver_Camp_Landing.xml` in the Overgrowth campaign, joins by IP, starts the session, skips the opening dialogue, and asserts that each machine controls exactly one non-remote avatar and sees the other's as remote, then walks the guest forward for 2 s and asserts the host saw it move and agrees on its position. Last run 2026-10-25: `RESULT: PASS` (guest moved 15.7 m as seen by the host, position gap 0.00 m, ping 12 ms).
- `tools/coop_level_change.ps1`: the same join, then the host loads `og_story/02_Slaver_Camp.xml`; asserts the client followed, the session survived (host peers 1, players 2 on both) and both avatars are back in the scene. Last run 2026-10-25: `RESULT: PASS`.
- Negative control: with Workshop mods active on the host the first scenario fails at the join step (observed in the run that found the bug). Mutation twins for each assertion are not written yet.

Measured on one PC: the host's copy of the guest's avatar trailed the guest's own position by 0.42 m while moving (proxy delay 47 to 57 ms) and converged to 0 within a second of stopping.

## How to play

Until the fork's installer bug is fixed, every player: buy Overgrowth on Steam, download `OvergrowthOnline_0.40.0.zip` from the fork's [releases](https://github.com/crxhvrd/overgrowth-online/releases) (SHA-256 `002FF5F7B65AD9103EF96F21DEC317DCFC9678B7235016AEB749E1D4323F697D`), close the game, run `install.bat`, start Overgrowth from Steam (the installed launcher starts Overgrowth Online; hold Shift for the stock game), open the Mods menu and disable every Workshop mod. The host picks Multiplayer, then a story campaign; friends join from the Steam friends list ("Join Game") or by IP. The Overhaul mod the installer enables must be on for everyone or off for everyone.

## Known issues

- Anyone with Workshop subscriptions is refused at join until those mods are disabled on every machine (see above).
- The guest's character is hidden during the host's dialogue scenes and placed beside the host afterwards; the guest cannot act during a cutscene.
- The guest's log fills with "no state registered for player id" lines while joining; harmless.
- The in-game screenshot taken from the guest's minimized window came back black once; not yet checked with a visible window.

## Next steps

- Scenarios for the known campaign bugs: dialogue choices, a kill barrier hit by the guest, checkpoints, death and revive, enemies hitting both players, items.
- Follow up on the fix PR with the fork's author.
- Mutation twins for each assertion; a run with `net_sim` latency and loss (the fork's bridge has it) and the hub's chaos relay.
- A two-PC session over Steam friends with a second account to confirm the relay path.
