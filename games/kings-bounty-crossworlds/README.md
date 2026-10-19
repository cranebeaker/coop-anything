# King's Bounty: Crossworlds (2010)

| | |
|---|---|
| Engine | TheEngine |
| Versions tested | Steam App 63910, manifest build ID 7254336; executable version string unknown |
| Projects | None found in the searches listed below |
| QA level | none |
| Last updated | 2026-10-20 |

## Where things stand

There is no ready-to-use co-op campaign project in the sources searched so far, and the Steam listing describes Crossworlds as single-player. A full co-op mod is **not implemented yet**; this write-up is research and reverse engineering, not a playable build. Friends cannot currently join a shared campaign through the stock game or a co-op mod from this research. The executable contains TCP and Lua network APIs, but the only configured endpoint found is named `debug_server`; this is not evidence of a playable multiplayer mode.

## Existing work

- No Crossworlds co-op project surfaced in bounded searches of GitHub, ModDB, Nexus, Steam, the Co-op Anything directory, and the official Fulqrum forum index.
- [nPwka Engine Extension](https://www.nexusmods.com/kingsbountycrossworlds/mods/38) adds a leaderboard and engine/modder features, but does not advertise co-op or campaign synchronization. Its author requires permission for public mods that depend on or build on its features.
- [Fantasy Wars](https://www.pcgamingwiki.com/wiki/Fantasy_Wars) and [Elven Legacy](https://www.pcgamingwiki.com/wiki/Elven_Legacy_Collection) are same-engine multiplayer comparison targets, not Crossworlds co-op mods.
- The open claim and research task is [issue #61](https://github.com/cranebeaker/coop-anything/issues/61).

## How the game works (for co-op)

Crossworlds combines real-time overworld exploration with turn-based tactical battles. The installed Steam copy has script files inside ZIP-readable `.kfs` session archives and includes `LuaIDE.dll`. A bounded search of text-like session scripts did not find network or multiplayer terms.

Static inspection of `kb.exe` found Winsock imports and calls, Lua globals named `NetListen`, `NetConnect`, `NetSend`, and `NetConnected`, and engine server/client symbols. Targeted decompilation also confirmed native TCP `listen`/`accept`, `connect`, `send`, and `recv` paths. The Lua table still exposes no receive callback, and the receive-to-Lua/game-command path has not been traced. The exposed `Logic` hero APIs resolve the current hero from active game state and do not accept a hero/client ID. A scenario action named `SPAWN_CLIENT` reaches an object factory with a `hero` type whose atom is `atom_hero_c`; if no state is supplied, that factory constructs and links a separate `client_hero_c` state object with per-instance movement fields. This is a plausible route to an independently progressing second hero, but independent user input, active-hero selection, UI binding, army ownership, save/load, and network synchronization are unproven. The inspected `data/app.ini` sets `~debug_server` to `TCP:0.0.0.0:51515`. A 30-second hidden launch of the isolated copy with that setting changed to loopback produced no observable listener; the original config was restored after the check. There is still no evidence that the native network layer lets a second player join the campaign or replicates campaign state.

The release uses TheEngine, also used by Fantasy Wars and Elven Legacy. Contemporary coverage describes LAN, Internet and hot-seat multiplayer in Elven Legacy, and Fantasy Wars is documented with direct-IP multiplayer. These are useful comparison targets, but no source or matching local multiplayer build has been examined yet. They do not establish that Crossworlds can use the same session protocol.

## What we changed

Reverse-engineering notes and a private smoke test of a minimal `dinput8.dll` proxy in the isolated test copy. The proxy loaded and forwarded DirectInput to Windows, but it does not hook game state or implement networking. No co-op code or game assets were committed.

## QA

No co-op client or gameplay test exists yet. The QA level is `none`.

## Temporary shared-control workaround (not co-op)

There is no co-op package to install, host, or join. If friends want to take turns in one running copy while development continues, Parsec can share keyboard and mouse input:

1. The host starts King's Bounty: Crossworlds on a Windows PC and shares the computer in Parsec.
2. Friends connect to the host. By default, Parsec guests get gamepad access; the host must explicitly grant keyboard and mouse control for this mouse-driven game.
3. Everyone plays the same running copy, hero, and save. Take turns moving and making battle decisions; this does not add separate heroes, networked game state, or independent saves.

This workaround is not the requested co-op: everyone shares one hero, one save and one game process; players cannot join as separate network participants. It has not been tested as part of this project. Parsec's current [Hosting and Permissions guide](https://support.parsec.app/hc/en-us/articles/32381747079572-Hosting-and-Permissions) describes hosting a Windows PC, inviting guests, and granting keyboard/mouse permissions. Do not expose the configured `debug_server` port as a multiplayer workaround; its protocol and purpose have not been audited.

## Known issues

The game has no confirmed shared campaign, player joining, synchronized battles, or shared save support. The presence of TCP symbols alone does not provide those features. There is no playable co-op mod or automated co-op QA result yet.

## Next steps

Invoke `SPAWN_CLIENT` in an isolated scripted scenario and test whether its `atom_hero_c`/`client_hero_c` pair can be independently selected and controlled; trace the native receive callback into engine events and game commands; map campaign and battle state plus save/load; then implement a host-authoritative shared campaign and a two-client bot test before calling any co-op feature playable. Track findings in [NOTES.md](NOTES.md) and [issue #61](https://github.com/cranebeaker/coop-anything/issues/61).
