# State of Decay (2013)

| | |
|---|---|
| Engine | CryEngine 3 |
| Versions identified | Steam app 241540, buildid 305948, original DX9 release with Breakdown and Lifeline. Isolated test copies launched repeatedly for RE; Steam install never modified. Not Year-One Survival Edition. |
| Projects | none shipped; in-progress investigation of the build's dormant CryEngine netcode (this repo) |
| QA level | none |
| Last updated | 2026-10-23 |

## Where things stand

Friends cannot play this copy together yet. The game shipped single-player. Undead Labs cancelled the co-op they had planned, and no later mod was found that adds it.

Investigation is underway on the dormant CryEngine netcode in this build. On isolated test copies (never the Steam install), a second process connects over UDP and the host spawns a second player character for it; with `host_boot_coop` the host also renders gameplay. As of 2026-10-21 the joining copy can **walk its own character**: with two runtime changes on the joining copy (a UI-gate bypass inside `CPlayerInput::OnAction` and loading a character model onto its player, which it never gets), forward/back/strafe input — including a real keyboard key sent to the client window — moves the player there, and the host's copy follows through the engine's own client→host input replication. As of the same evening the joining copy also **shows the game**: its window was drawing the renderer's load-time Flash movie (the title art); stopping that playback, skipping the front-end update and popping the title menu gives a third-person view of its own player, with mouse look working. Loading a character model on each machine's copy of the other player makes both players visible to each other. Later fixes (NOTES Phases 3r–3s) gave survivors their models on the joining copy, fixed its turning and strafing (its character had been spawned under the game's AI control), and brought up its minimap and health/stamina HUD. As of 2026-10-23 the two players can **fight zombies together** (NOTES Phase 4a): both machines see the same zombies (clothed bodies on the joining copy), zombies target and damage either player, the joining player's melee hits and kills register on the host, each player keeps their own kill count, the joining player has a melee weapon (starter hammer, cycled from community storage with the middle mouse button) and the host player's own weapon works as in single-player. Zombies stay alive, chase and spawn around the joining player even when the host's player is several hundred metres away. Host-to-client data (banners, minimap icons, zombie models, weapon and pause state) goes over the game's own network (script RMIs and chat messages). It is still not playable with a friend as a product: missions, community and other single-player state run only on the host, the joining player has no survivor record of her own, a long stress test still hung the host once (likely cause removed, see Known issues), and everything is applied at runtime by helpers under gitignored `local/`, so it is not reproducible from this write-up alone. Detail is in [NOTES.md](NOTES.md) (Phases 3p–4a).

State of Decay 2 is a different game and already has official co-op. It is not this install.

## Existing work

- Official co-op was dropped in July 2013. Jeff Strain (Undead Labs) said it would mean redesigning core systems, not only adding netcode. [GamesRadar](https://www.gamesradar.com/state-decay-scraps-plans-co-op/).
- Year-One Survival Edition also shipped without co-op. A Microsoft producer said the studio would have had to rewrite a large part of the code. [Eurogamer](https://www.eurogamer.net/heres-whats-new-in-state-of-decay-year-one-survival-edition).
- A 2013 Steam thread proposes a LAN mod and does not link a build. [Discussion](https://steamcommunity.com/app/241540/discussions/0/810939351105827815).
- ModDB lists three mods for this game (a skin mod, a gameplay-number mod, and a 2016 "Lifeline" upload that describes itself as the full game). None is multiplayer. [ModDB](https://www.moddb.com/games/state-of-decay/mods).
- Nexus mods under `stateofdecay` that matched a multiplayer/co-op search are single-player (followers, ally AI, a lone-survivor mode). The full catalog was not paged.
- A 2013 playground.ru "split screen" upload was not downloaded. Its description is two windows on one PC. Shared-world play is unconfirmed.

Searches and the file-level notes are in [NOTES.md](NOTES.md).

## How the game works (for co-op)

CryEngine 3, codename Class3. Content is in zip-format `.pak` files. Every entry in `gamedata.pak` uses zip method 13, which is Xbox LZX (`XMemDecompress`), not deflate. A test-copy reader with LZX window bits 17 produced the scripts below. Details and the check are in [NOTES.md](NOTES.md).

The campaign rules are `scripts/gamerules/singleplayer.lua`. `IsMultiplayer()` returns false, and `config/singleplayer.cfg` sets `sv_gamerules="SinglePlayer"`. The same script still has CryEngine's server and client. `Server:OnClientConnect` spawns a `Player` actor for the connecting channel. The `Player` class is `scripts/entities/actor/player.lua`, with the game's own animation graphs. Use prompts and several other rules ignore any actor that is not `g_localActorId`.

A `multiplayer.cfg`, an empty InstantAction level rotation, Crysis level names, and `scripts/entities/multiplayer/` are shipped too. Those files are CryEngine samples. There is no multiplayer rules script.

World objects the game added (searchable containers, homes, infestations, facilities) are Lua under `scripts/entities/ul/`. Missions and the community simulation were not in those rules. They are in the exe and in XML inside the paks.

The campaign level is `class3` (`Game/levels/Class3`, level info name `game\levels\class3`). Putting `map class3` in `user.cfg` loads it. The same command on the exe command line does not. Once the level is up, `OnClientConnect` runs for the local player on channel 1 and spawns the `Player` actor.

`map class3 server`, with `sv_port` and `sv_bind`, keeps that local player and also binds a UDP port. `map class3` alone does not bind a port. `map class3 dedicated` binds the port and does not spawn a local player.

A second test-copy process running `connect <host> 64087` is told to prepare `class3`, finishes level load, and connects. The early `cause 12 Unreachable address` disconnect turned out to be a side effect, not the cause: a logging UDP relay showed the second copy *stopped sending* about ten seconds in, and its minidump has an access violation on a NULL object during the join. A small runtime memory patch on the joining copy (a NULL check, written into the running process, exe file unchanged) gets it past that. The host then reaches `OnClientConnect` for the second peer on channel 2.

The host's own code then aborts with the assert `Buffer overflow in entity db` (`UL\EntityDb\EntityDb.cpp`, `EntityDbAdd`): the single-player code keeps the player in a database named `Dude` with room for one entry. Naming the second actor anything else (the loose `singleplayer.lua` names it `CoopPeer2`) avoids the assert, and the host spawns and network-registers a second player character two metres from the first. Both processes stay alive. No one is playable yet: the host window is black and the joining copy sits on the title menu.

The black host is the next wall, and it is not the network. Loading the level with the `map` command never starts State of Decay's front-end "Play" flow (community sim, survivor control, HUD, camera), so nothing is drawn even with no client and even with daytime forced. The menu itself renders fine. "Play State of Decay" is `fscommand("DLCSelected","SOD")` in the front-end flash, handled in native code, and it starts a *single-player* session with no socket. So the menu path renders a real game but has no network, and the `map ... server` path has the network but no rendered game. Co-op needs one path that does both, and the junction is native C++.

Saves for `sys_user_folder = USER` go to `%USERPROFILE%\Saved Games\USER` (junctioned at the test copy). The Steam install was not modified.

## What we changed

No shippable co-op build yet. On the test copies only: loose probe lines in `main.lua` and `singleplayer.lua`; `singleplayer.lua` names a channel > 1 actor `CoopPeer<channel>` instead of `Dude`; cfg files that load `class3`; runtime NULL-check patches written into the joining copy's memory (exe file unchanged); and session/RE helpers (UDP relay, `host_boot_coop` / `session_join_leave`, CPI bridges, etc.). Since then `coop_setup.py` also installs the combat and zombie layer: input and state-machine replication both ways, the joining player's hits and kill counts, zombie targeting, focus, cull and population for her, weapon relay, shared pause, and crash guards; extra RMIs live in the loose `singleplayer.lua`. **All of that lives under gitignored `local/`**, not in this repo — the spawn/join results above cannot be reproduced from the published files alone.

## QA

No automated suite. The checks are manual launches of the two test copies, windowed, then closed, plus a UDP relay and a debugger/trap to read crash sites. QA level stays `none`.

## How to play

There is no co-op build to install.

## Known issues

There is no installable build: a session is the two test copies booted by `local/restart_session.sh` and patched at runtime by `local/coop_setup.py` (with `local/hud_relay.py` running next to the host). `Unable to bind object to network` on the joining copy is expected for a client and harmless.

- Missions, community, journal data and use prompts are single-player state on the host; the joining player has no survivor record.
- New zombies spawn around the joining player only while she is within 300 m of the host's camera (the host streams the level around its own camera only; spawning 550 m out killed the host once). Zombies already near her are kept at any distance.
- A 30-minute stress test (constant teleports and spawns) hung the host once in the game's own lookup of the radar HUD element: the UI element map had a node linked to itself. The likely cause, the kill-count banner running UI script from inside native hooks, is now off (the count is still kept, just not shown). Not yet re-verified by a full soak.
- Windows sleep (Modern Standby) during a session throttles both games and crashes both renderers on wake. Keep the PC awake while testing.
- Spawn density follows the map: some areas (near the home base, open fields) have no zombies for either player.

## Next steps

Re-run the 30-minute soak (`local/zm_soak.py`) without the kill banners to confirm the host hang is gone, then give the joining player a survivor record of her own (journal, prompts, community), and move the remaining runtime patches toward something installable. Do not launch the Steam install. See [NOTES.md](NOTES.md) (Phase 4a) for addresses and failed attempts.
