# Draft: "claim a game" issue for cranebeaker/coop-anything

Template: `.github/ISSUE_TEMPLATE/claim-a-game.yml` (label `in-progress`). To be opened under the owner's GitHub account after they approve it. Nothing private below.

**Game:** Overgrowth (Wolfire Games, 2017, Steam)

**Plan:**
Reviving an existing attempt. The engine (Phoenix) is open source (Apache-2.0, github.com/WolfireGames/overgrowth) and already contains Wolfire's 2021 online multiplayer prototype (host-simulated thin clients, Steam networking sockets, Steam beta branch `internal_multiplayer_testing`), abandoned upstream since the 2022 open-sourcing. A new engine fork, github.com/crxhvrd/overgrowth-online (Apache-2.0, active October 2026), rewrote the movement replication (local player simulation, snapshots, clock sync), added a story co-op mode, revive, server browser and Steam friend join, and ships a test-only level-hook mod that hosts or connects from config and logs session state.

Approach: treat Wolfire's prototype as the reference and the fork as the base. Verify the fork's story co-op with two real clients on one PC (its GameNetworkingSockets build, no Steam) using the hub's gameslot/gamepool/bots tools around the fork's test hook; turn the known campaign bugs from the "Overgrowth Story Multiplayer" Workshop mod (client moves during dialogue, kill barriers kill the host, guest waits for the host's level load, join crashes) into a regression suite; fix what fails as a separate mod package or as PRs to the fork; package host/join for Steam friends. Directory entries for five Overgrowth projects (Wolfire prototype, overgrowth-online, Story Multiplayer, Full Coop split screen, OGMP) are drafted.

Notes will be in `games/overgrowth/NOTES.md` on the branch `game/overgrowth` of the owner's fork.

**Your repo or branch (optional):** https://github.com/dekstrus/coop-anything/tree/game/overgrowth

Status 2026-10-25: fork created at https://github.com/dekstrus/coop-anything (via the owner's GitHub session in the built-in browser). The issue form was prefilled in the browser for the owner to submit; not yet submitted.
