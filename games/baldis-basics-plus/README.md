# Baldi's Basics Plus (2020)

| | |
|---|---|
| Engine | Unity 2020.3.49f1, Mono, Windows x64 (BepInEx 5.4.23.5 or MelonLoader 0.7.3, Harmony) |
| Versions tested | Steam (app 1275890), game version 0.14.5, buildid 24261910. `BALDI.exe` SHA-256 `b9bd86d050c90811d037a978ab246b7c06c9370bf25d0e8f83b35d3a1a120026`, `Assembly-CSharp.dll` SHA-256 `635b0d164a7050942077f3a7509ad2e4dc49a4d860e476ebe6906c7508dfa367` |
| Projects | Baldi Together ([baldi-plus-coop](https://github.com/multiplayereverything/baldi-plus-coop), [v0.9.2 pre-release](https://github.com/multiplayereverything/baldi-plus-coop/releases/tag/v0.9.2)); directory entry "Baldi Together" |
| QA level | automated |
| Last updated | 2026-10-26 |

## Where things stand

Baldi Together lets up to four players run Hide & Seek floors together, over LAN or online through Steam. The host runs the real game; NPCs, doors, pickups, the shop and results follow the host. Each player can be caught on their own and then spectates teammates. It is an alpha. The main mode is covered by automated tests with real game processes, and the author's own hands-on testing (mostly on their own, not yet with friends) found it works well for the most part. Random events follow the host (0.9.2): fog, the Mystery Room, the broken ruler and the time-out are mirrored in every player's game, and the rest run on the host only. Field trips pick the same Pitstop trip on every copy. Challenges and Endless have not been validated.

## Existing work

A search on 2026-10-26 found no other co-op or multiplayer project for Baldi's Basics Plus. It covered this directory, web searches (only single-player mods and mod menus on itch.io and GameBanana turned up) and the [Steam discussions](https://steamcommunity.com/app/1275890/discussions/0/2963893984920858040), where co-op is player talk, not a project. The [multiplayer project on GameBanana](https://gamebanana.com/mods/668785) is for Baldi's Basics Classic, not Plus. Not searched: the BB+ modding Discord and non-English sites. The full list is in the mod's [`docs/IDEAS.md`](https://github.com/multiplayereverything/baldi-plus-coop/blob/main/docs/IDEAS.md). The game is single-player only.

## How the game works (for co-op)

- **Engine and scripts:** Unity Mono, so all game logic is in `BALDI_Data/Managed/Assembly-CSharp.dll` and can be hooked with Harmony. Levels are generated from a seed. The same seed and the same game build give the same layout, but random free-item placement is not stable across machines, so the host reconciles the item list before a client enters.
- **No netcode.** Everything above the mod's transport is the mod's own protocol: TCP frames with a 4-byte length prefix, an exact-version and layout-hash handshake, then host-authoritative state.
- **Steam is already there.** The game ships Steamworks.NET (`com.rlabrecque.steamworks.net.dll`, networking sockets v012, matchmaking v009) and `steam_api64.dll`. Its own `SteamManager` calls `SteamAPI.Init` and `SteamAPI.RunCallbacks` every frame, and pauses a running game when the Steam overlay opens.
- **Saves:** multiplayer runs make no resumable run save. Lab mode (`-bt-lab`) redirects the game's save paths into the mod workspace, so tests never touch the owner's saves.

## What we changed

The full feature list is in the mod's README. This section lists the lessons that apply to other games.

### Link liveness must not depend on the game loop

- **Bug:** a player's log showed a client migrating to a new host while the host was still running. Before 0.9.0 the only regular traffic was a 2 s heartbeat sent from `Update`, and the socket timed out after 15 s of silence. So a host whose game loop stopped for 15 s looked dead, and its clients migrated. A load, a hitch, or possibly Alt+Tab can stop the loop that long. A stalled client was dropped by the host the same way.
- **Second cause:** the receive queue dropped a peer at 512 queued packets, and the send queue at 128. With one client's poses arriving during a host stall, that was about 25 s, or about 9 s with three clients. A bad connection that held TCP for a few seconds could also fill the send queue.
- **Fix (protocol 14):**
  - The socket's writer thread sends a 1-byte keepalive frame (`0x00`) after 2 s without traffic, and the reader drops it. A stalled game loop keeps the link. A dead process or a dead network still times out after 15 s.
  - Keepalives start only after the peer's first frame. An older build therefore still gets the version-mismatch message instead of an unknown frame.
  - The client measures the host's silence itself: "The host isn't responding" after 5 s, and migrate (LAN) or leave (Steam) after 60 s.
  - The queues are capped by size (8 MB waiting for the game loop, 4 MB unsent per link), not by packet count.
- **Lesson:** if your heartbeat comes from the game's update loop, a long load looks like a disconnect. Send keepalives from the network thread, and decide "the other side is gone" separately from "the other side is busy".

### A TCP chaos proxy

The hub's chaos box (`tools/chaos`) impairs UDP datagrams. On TCP the kernel repairs loss, reordering and duplicates, so what a player feels is delay, head-of-line stalls, a bandwidth ceiling, outages and cut connections. [`tests/tcp-chaos.py`](https://github.com/multiplayereverything/baldi-plus-coop/blob/main/tests/tcp-chaos.py) is a seeded loopback proxy that models those, per direction, and reads the hub's `tools/chaos/profiles.json` (with an embedded copy as a fallback):

- delay and jitter, with bytes always leaving in order;
- Gilbert-Elliott loss, turned into a retransmission stall: a lost segment is held for one retransmission timeout, and everything behind it waits;
- bandwidth pacing with backpressure (the proxy stops reading once its buffer is full);
- a control port taking one JSON command per line: `set_profile`, `stall` (an outage TCP survives), `cut` (close every proxied connection), `stats`, `shutdown`.

```
python -I tests/tcp-chaos.py --listen 7801 --target 127.0.0.1:27015 --profile bad_wifi --control 7802
python -I tests/tcp-chaos.py call 7802 '{"cmd":"stall","dir":"both","seconds":8}'
python -I tests/tcp-chaos.py selftest
```

It is a single Python file with no dependencies, so any TCP game link can use it.

### Use the game's own Steamworks, not Spacewar 480

When a Steam game already ships Steamworks.NET and initializes it, a mod can use that copy:

- Steam lobbies (`bt=1` plus the exact mod version and protocol in lobby data);
- `SteamNetworkingSockets.CreateListenSocketP2P` and `ConnectP2P` through Valve's relay;
- rich presence `connect` = `+connect_lobby <id>`, so friends get **Join Game** on the host's profile;
- overlay invites (`GameLobbyJoinRequested_t`, `GameRichPresenceJoinRequested_t`), and the `+connect_lobby` launch argument.

Nothing is downloaded or bundled, Steam shows the players as playing the real game, and no `steam_appid.txt` is written. The mod's existing protocol needed one change: an `IPacketLink` interface, so one Steam connection can be a Transport link, with one Transport frame per reliable Steam message. The handshake and everything above it is the same as on LAN. Check first that the game's `SteamManager` really calls `RunCallbacks` every frame, and look for side effects. Here, opening the overlay pauses the game, so INVITE FRIENDS is only on the lobby screen. Steam rooms have no host migration, because migration needs addresses.

### Random events: one schedule, the host's

Each copy of the game builds the same seeded list of random events for a floor. The start order, the gaps and each event's length come from `UnityEngine.Random` at run time, so two copies running their own timers drift apart. In Hide & Seek only the host starts the timers. A challenge scene starts them directly as it loads, so the mod blocks `EnvironmentController.StartEventTimers` on client worlds.

The host sends each event's announcement, begin and end by its index in the shared list. Clients play the jingle and the TV intro. For events whose effects are each player's own (fog, the Mystery Room, the broken ruler, and the time-out's rule break, lights and music), a client runs the event's own `Begin`, with the event's end timer disabled so only the host's End stops it. Baldi's anger from the time-out stays on the host. The test runs a fog event on the host and checks the client's copy turns on and off with it. A mutant that sends nothing must fail it, and it does.

The Pitstop picked its field-trip room with `UnityEngine.Random` too, which could give two copies different Pitstops and fail the world match. The pick is now seeded from the run seed and the floor.

### Only lobby members may connect

The host's Steam listen socket used to accept any SteamID that called it, up to three. The playbook's rule from [networking-steam-spacewar](../../playbook/networking-steam-spacewar.md) applies to connection-based sockets too: only members of the host's lobby get in. Steam's member list can reach the host a moment after the joiner's call, so a caller who isn't listed yet is held for up to 5 s before being refused.

### Testing Steam with one account

One Steam account cannot open a P2P connection to itself, but most of the Steam path can still be tested in one process:

- **`SteamAppId` instead of a file:** the lab launcher sets the environment variables `SteamAppId=1275890` and `SteamGameId=1275890` before starting the exe, which is what Steam itself does. `SteamAPI.Init` then succeeds without Steam starting the game and without writing `steam_appid.txt` into the game folder.
- **`SteamNetworkingSockets.CreateSocketPair`:** this gives two connected connections inside one process, through the real Steam networking API. Attach one to a host Transport and the other to a client Transport, and run the real handshake and frames over them.
- **Real lobby without exposure:** HOST ON STEAM opens a real friends-only lobby marked `bt=lab`, which the public filter never lists. The test checks its lobby data and Join Game presence, then leaves and checks that the lobby closed and the presence was cleared.

This found two bugs before any real session:

- a closing side never ended its own peer, because Steam posts no callback for a local close, which left a ghost player in the room;
- the menu fell through to the options page after HOST ON STEAM.

What it cannot cover is Valve's relay between two accounts.

## QA

- `scripts/test.ps1`: 26 transport checks: an idle link survives past the read timeout, a silent raw peer still times out, and 1,500 queued packets wait for a stalled loop. Its mutation twin (`BT_MUTANT=no-keepalive`) must fail the idle-link check, and it does.
- `scripts/link-test.ps1` (BepInEx host, MelonLoader client, through `tcp-chaos.py` on `bad_wifi`): 7/7. The client waited through a 25 s host stall (`-bt-lab-stall-host 25`) without dropping or migrating, and the host kept the client. Both sides survived a 10 s two-way outage and stayed in sync. A cut link was still detected. With `-bt-lab-no-keepalive` the client drops at 14,998 ms and migrates, which is the reported bug, so the test can fail.
- `run-tour.ps1 -Steam` (one process, Steam signed in): 17/17 on 0.9.2, including the lobby gate; its no-gate mutant fails it. Steam's socket pair under two real Transports (connect, both directions, a 64 KB frame, 200 frames in order, final frame before close, closing side ends its peer), the Steam pages, a real `bt=lab` lobby with Join Game presence, and leaving.
- `launch-lab.ps1 -EventSync` with `tests/verify-events.py` (0.9.2): 7/7; the no-event-sync mutant fails (2/7). `client_events_host_only` in the core test: a client asked to start its own event timers must refuse, and a mutant that lets it fails.
- `scripts/lab-suite.ps1` on 0.9.2 (second PC, quiet): every tour, two-process LAN, NPC sync, core, encounter 45/45, soak 12/12 (mod cost about 0.2 ms per frame), link and shop tests, with no retries. Lab copies run as small minimized windows at low priority. Earlier, on the 0.9.0 build: the transport checks and their mutant; feature tours (lobby 32, spectator 16, hide-and-seek audio 7, spectator UI 18, items 18, Beans 14, Arts and Crafters 16, Mrs. Pomp 17, The Test 15, Dr. Reflex 16, Chalkles 15, Hall Monitor 25, compat-fail 3); two-process tests (LAN 15/15, NPC sync 15/15, core co-op 14/14, soak 12/12, link 7/7). Every game launch uses `-bt-lab -bt-diagnostics` and test-owned processes only.
- The two-process encounter test passed 45/45 on the final build after an intermittent failure in an earlier suite run. Some two-process launches stall at startup and are retried once.
- **Hands-on:** the author has played it mostly on their own and reports it works well for the most part. It has not been played with friends yet.
- Not covered yet: a Steam session between two accounts over Valve's relay.

## How to play

Install BepInEx 5.4.23.5 or MelonLoader 0.7.3 (one loader per game copy; the two builds play together), then extract the matching ZIP from the [releases page](https://github.com/multiplayereverything/baldi-plus-coop/releases) into `<game dir>`. Select **MULTIPLAYER** on the title screen.

- **LAN:** HOST GAME, then friends use JOIN GAME or FIND LAN GAMES.
- **Steam:** start the game from Steam, then STEAM GAME → HOST ON STEAM. Friends join with an invite, with Join Game on the host's profile, or from FIND STEAM GAMES.
- Everyone needs the same game version (0.14.5) and mod version.

## Known issues

- A Steam room ends when its host leaves; only LAN games migrate to a new host.
- Opening the Steam overlay pauses the game during a run.
- Flood, party, gravity, Balder Dash and student shuffle run only in the host's game; their NPCs and doors appear for everyone, but each player's own effects (water, balloons, the flipped view) do not.
- Challenges, Endless and other mods have not been tested with co-op. A challenge win ends the host's game, which disconnects everyone.
- Join before the run starts; joining a run already in progress is not supported.

## Next steps

- Run a real two-account Steam session over Valve's relay.
- Validate challenges and Endless (win and results for everyone, repeating notebooks), and mirror the remaining events' local effects.
- Play a full run in co-op and write it up for `verified-playthrough`.
