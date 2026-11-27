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

### Identity (private details in local/identity.md)

- Store: Steam (owner confirmed 2026-11-27). Steam app id 978300 appears in SR3MP's own docs/forum links but has not been checked against the owner's install.
- Exact build, exe SHA-256, engine version: unknown — the install has not been inspected.
- Engine family: proprietary "Engine Core Technology Group" engine with Havok physics; SR3R's README says this for the Epic build, SR3MP's forum says the Remastered build is 64-bit. Neither has been verified against the owner's Steam exe.
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

### Conclusions derived from those observations (not measured in-game)

1. **No SR3MP netcode can be copied**: the implementation is closed source and the project is abandoned. Its public docs describe architecture and behaviour, not code.
2. **Reusable findings, not reusable code**: dedicated-server topology, ENet reliable UDP as a transport choice, a 50 Hz network thread that owns packet sending, main-thread requirement for game/Lua calls, client/server event routing, and the names of reversed engine functions in the public repos. These are leads to verify against the Steam build, not proof that anything works there.
3. **Code from the no-license repos must not be copied** under this project's rules. The MIT IntelliSense repo can be used under MIT if needed. Asking the authors for a license or permission is an option, but that means contacting them — owner permission required first.
4. **Most promising path remains the game's native 2-player co-op**: SR3MP explicitly does not sync missions/story, so it does not solve the campaign problem. Whether the native co-op replication can accept a third client has to be answered by reverse engineering the Steam build on an isolated copy.
5. **SR3MP's scale claim (127 players) proves nothing about campaign co-op**; it refers to a sandbox server, not story/mission state.

### Unknowns (do not claim otherwise)

- Steam app id, build id, main exe SHA-256 and exact engine version for the owner's install.
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

### Owner question still pending

Research playbook asks the owner who they have heard of working on the game. Ask before closing research as complete.
