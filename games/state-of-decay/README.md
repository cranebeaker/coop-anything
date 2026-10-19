# State of Decay (2013)

| | |
|---|---|
| Engine | CryEngine 3 |
| Versions identified | Steam app 241540, buildid 305948, original DX9 release with Breakdown and Lifeline. Not launched. Not Year-One Survival Edition. |
| Projects | none shipped; in-progress investigation of the build's dormant CryEngine netcode (this repo) |
| QA level | none |
| Last updated | 2026-10-19 |

## Where things stand

Friends cannot play this copy together yet. The game shipped single-player. Undead Labs cancelled the co-op they had planned, and no later mod was found that adds it.

Investigation is underway on the dormant CryEngine netcode in this build. So far, on test copies, a second game process connects to a host over the network and the host spawns a second player character for it without crashing. It is not playable: the host does not render the game, and the joining copy does not control its character. The remaining work to bridge that is native code. Detail is below and in [NOTES.md](NOTES.md).

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

No shippable co-op build yet. On the test copies only: loose probe lines in `main.lua` and `singleplayer.lua`; `singleplayer.lua` names a channel > 1 actor `CoopPeer<channel>` instead of `Dude`; cfg files that load `class3`; a runtime NULL-check patch written into the joining copy's memory (exe file unchanged); and small tools (UDP relay, exit-call trap, memory patcher). All of this lives under `local/`, not in this repo.

## QA

No automated suite. The checks are manual launches of the two test copies, windowed, then closed, plus a UDP relay and a debugger/trap to read crash sites. QA level stays `none`.

## How to play

There is no co-op build to install.

## Known issues

A second player cannot actually play yet. The second character spawns on the host but the host does not render gameplay (see above), and the joining copy does not take control of its character ("we are not a server", "Unable to bind object to network"). The joining copy also relies on a runtime memory patch to survive the join.

## Next steps

The networking connects and a second character spawns. The blocker is native: make one path both render the front-end game flow and host over the network (the `DLCSelected` -> `StartGame` handler, read from live process memory since the exe is SteamStub-wrapped). Then make the joining copy take control of its character and start replicating the `UL` world objects. Do not launch the Steam install.
