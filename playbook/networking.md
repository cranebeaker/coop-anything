# Networking for co-op mods

How to pick a sync model, a transport and a way to connect players when you add
co-op to a single-player game. Read it before you design anything, then use
[networking-steam-spacewar.md](networking-steam-spacewar.md) for the Steam
implementation details.

Conventions: every concrete claim has a link. **(unverified)** means no source
confirmed it, or only a weak one did. **(judgment)** means design advice from this
playbook's authors, not from a cited source. Check licenses yourself before you
ship; the license column is what each project's repository page said.

## 0. Quick decisions

| Question | Default | Fallback |
|---|---|---|
| Sync model | Host-authoritative state sync, with a few client-owned things (own player, own camera) | Lockstep only if the engine is already deterministic (section 1) |
| Transport, your own netcode | ENet (C/C++) or LiteNetLib (C#) for UDP | GameNetworkingSockets if you want encryption, ICE and Steam relay in one library |
| Reaching a friend | Steam relay (ISteamNetworkingMessages), see the Spacewar page | Tailscale or ZeroTier overlay, then port forwarding or UPnP |
| Game netcode already exists, closed | Local UDP proxy tunnelled over Steam (section 4) | Custom NetDriver or hook in the engine |
| Testing | Mock Steam DLL on one PC plus clumsy or netem for bad links | Engine emulation (PktLag, PktLoss) |

## 1. Sync models

### 1.1 The models

| Model | What crosses the wire | Fits a mod when | Trade-offs for mods |
|---|---|---|---|
| Deterministic lockstep | Inputs only | The engine is already bit-exact deterministic (own sim, fixed timestep, seeded RNG) | Bandwidth is tiny and scales with input size, not object count, but every peer must produce identical results, and floating point can differ across compilers, OS and instruction sets. [Gaffer On Games](https://gafferongames.com/post/deterministic_lockstep/) calls guaranteeing this "nearly impossible" and says even the author's fixed example only held on one machine, binary and OS. A mod cannot change engine timing, so lockstep is rarely realistic. |
| Host-authoritative state sync | Object state, with a priority list | Most single-player games, because the host's copy of the game is the truth | Both sides run the sim and the host sends corrections; the result is approximate and you will chase extrapolation divergence and pops ([Gaffer On Games: state synchronization](https://gafferongames.com/post/state_synchronization/)). Needs a read/write hook for every synced field. |
| Snapshot interpolation | Full state snapshots, clients only render | Non-deterministic sims, simple physics, remote players and NPC puppets | Higher bandwidth; costs interpolation delay: about 350 ms at 10 snapshots/s, 150 ms at 30/s, 85 ms at 60/s per [Gaffer On Games](https://gafferongames.com/post/snapshot_interpolation/). Good for remote avatars even when the rest is event-based. |
| Client-authoritative with host arbitration | Client sends its own state; host accepts or vetoes | Co-op between friends where cheating is not a concern | Easiest for a mod: each player's own character, camera and combat hits are simulated locally, so no prediction code. Host arbitrates shared things (quest flags, loot, doors). **(judgment)** |
| Rollback (GGPO style) | Inputs, with prediction and re-simulation | Fighting games and tiny deterministic sims | Needs the whole game state to be saveable and loadable every frame plus determinism; [GGPO](https://github.com/pond3r/ggpo) (MIT) is the reference SDK. Unrealistic for a big modern game you do not control. |

Ready-made proof that "sync only the players" is a valid scope: the HKMP wiki says it
does not sync anything except players and their movement, while enemies, items and
breakables stay separate per player
(summary via [search result](https://github.com/Extremelyd1/HKMP), see also the
[community wiki](https://geroyuni.notion.site/hkmp)). **(unverified: wiki text was only seen
through a search summary.)**

### 1.2 Techniques that apply to any model

| Technique | What it is | Source / note |
|---|---|---|
| Quantization | Send fewer bits: smallest-three quaternions (29 bits instead of 128), positions to about 2 mm (50 bits instead of 96) | [Snapshot compression](https://gafferongames.com/post/snapshot_compression/) |
| Delta compression | Encode state against a baseline the receiver has acknowledged; unchanged objects cost one bit | [Snapshot compression](https://gafferongames.com/post/snapshot_compression/) |
| Priority accumulator | Each frame add every object's priority to its accumulator, send the top ones that fit the budget, reset what was sent | [State synchronization](https://gafferongames.com/post/state_synchronization/) |
| Interest management | Only send objects near a player or in the same cell/level | Standard practice **(judgment)**; for a 2-4 player co-op mod it is usually unnecessary except for large worlds with hundreds of NPCs. |
| Reliable vs unreliable | Positions unreliable (stale data is useless), events reliable | [Steam Messages docs](https://partner.steamgames.com/doc/api/ISteamNetworkingMessages) describe both modes; Nitrox-style guidance says use reliable only for rare critical events and never reliable-ordered for movement ([third-party Nitrox doc](https://www.mintlify.com/Papela/Nitrox-Cracked-Mod/development/networking), **unverified fork docs**) |
| Send-time stamps and an adaptive playback delay | Stamp each state packet with the sender's high-resolution clock. The receiver keeps ~32 snapshots and shows "sender time - delay", blending position and facing between the two snapshots around it. Delay = 90th percentile of packet lateness (vs the fastest packet of the last ~2 s) + one send interval, clamped 20..200 ms, drifting slowly. Snap instead of blend on big jumps (teleports, doors) | Obscure (games/obscure): snapping each character to the newest packet looked smooth on one PC and stuttered over Steam (RTT 62..78 ms, spikes to 500 ms); fixed in build 1.1 for players and 1.7 for enemies (checked by playing over Steam, no packet capture) |
| Fixed send tick, not per frame | Send on a 60 Hz clock; per-frame rates scale with an uncapped frame rate | Obscure: ~275 fps uncapped meant ~600 packets/s over Steam with "every 2 frames" rates |

### 1.3 What to sync for story co-op

This is the part that decides whether a mod feels like co-op. Priority order, from
cheap to hard. **(judgment)**, except where a link is given.

| Layer | Approach | Notes |
|---|---|---|
| Player avatar | Client-authoritative, snapshot-interpolated on peers | Position, rotation, animation state, equipment visuals |
| NPC / enemy authority | One owner per NPC, the host or the nearest player, and ownership transfer when it changes | TiltedEvolution's release notes list "NPC ownership transfer" and synchronizing leveled NPCs ([release v1.8.1](https://newreleases.io/project/github/tiltedphoques/TiltedEvolution/release/v1.8.1)) |
| Combat and damage | Attacker's client reports a hit, owner of the target applies it | Avoid double application: key each hit with an id |
| World objects | Doors, containers, pickups: host decides, broadcast result | Use stable ids (form ids, level+index), never memory addresses |
| Quest and story flags | Host owns them; mirror every flag/variable write to clients | Hook the engine's single "set variable/flag" function if one exists; this is the highest-value hook. Unsynced quests are the main failure of existing mods |
| Dialog and cutscenes | One player drives, the others get a "watch" state; gate on all players being present | Hardest area; decide per game whether cutscenes pause for everyone or run for the triggerer only |
| Inventory, gold, XP | Per player by default; shared pool only if the design asks | Shared state needs host arbitration |
| Time, weather, RNG | Host broadcasts; clients adopt | |

Simple cases are worth shipping alone. The Ship of Harkinian "Anchor" client's code shows a
room that carries an owner id plus options to sync items and flags
([source excerpt](https://code.moparisthebest.com/mirror/Shipwright/commit/060878fb2e75c5c2dbbea325169671cf3f9f122f)),
i.e. a flag-sync mode is a switch, not a rewrite.

### 1.4 Late join, level changes and saves

All **(judgment)** unless linked.

- **Late join:** the host sends a full snapshot (world flags, quest state, entity list), then switches to deltas. Anchor requests the team state right after load when a save is loaded ([source excerpt](https://code.moparisthebest.com/mirror/Shipwright/commit/060878fb2e75c5c2dbbea325169671cf3f9f122f)).
- **Level / map transitions:** one player changing level must not tear down state for the others. Either keep everyone in the same level (a "follow the leader" rule) or track a level id per entity and only apply updates for the loaded level.
- **Loading a save mid-session:** find the one place the game requests a load (Obscure: the load menu reads the slot, then sets the state machine to "load"; that write exists once in the exe). The host watches for it and sends "load" naming the save transfer it belongs to; the guest waits until that file is on disk, then runs the same two steps. Joining guests can use the same path, so nobody picks a save by hand (games/obscure).
- **Saves:** treat the host's save as the source of truth; joiners receive its state at connect. Saving on a client is a no-op or a separate local slot. Test save, quit, load while connected.
- **Reconnects after a map change:** a game that reconnects on every map change needs a connection id so the host recognizes the same player (section 4).
- **Large messages:** big snapshots must be fragmented; Steam Messages fragments for you but with limits (see the Spacewar page, which splits at 60000 bytes and raises buffer sizes).

## 2. Transports and libraries

| Library | Language | License | Reliability / extras | Mods that use it where sources say |
|---|---|---|---|---|
| [ENet](https://github.com/lsalzman/enet) | C | MIT | Reliable UDP, channels | KeeperFX regression-tests ENet alongside Steam (see [Spacewar page](networking-steam-spacewar.md)); other users **unverified** |
| [GameNetworkingSockets](https://github.com/valvesoftware/GameNetworkingSockets) | C++ (C API, bindings for C#, Go, Rust) | BSD-3-Clause | Reliable + unreliable, fragmentation, AES-GCM-256 encryption, ICE-based P2P with your own signaling, SDR only on Steam | TiltedPhoques' TiltedConnect wraps "the network library" without naming it ([README](https://raw.githubusercontent.com/TiltedPhoques/TiltedConnect/master/README.md)); **GNS use by Skyrim Together is unverified** |
| Steam Networking Messages / Sockets | C API via `steam_api.dll` | Steamworks SDK terms | Session-less P2P by SteamID, relayed over Valve's backbone (SDR) | KeeperFX net_steam, this repo's HXCoop ([Spacewar page](networking-steam-spacewar.md)); SSMP (Silksong) "fully supports Steam networking" ([SSMP](https://github.com/Extremelyd1/SSMP)); Elden Ring Seamless Co-op relies on Steam matchmaking and blocks FromSoftware's service ([guide via search](https://www.exitlag.com/blog/elden-ring-seamless-co-op/), **unverified at source level**) |
| [Epic Online Services P2P](https://dev.epicgames.com/docs/) | C SDK | EOS SDK terms | NAT punch with relay fallback, addressed by account id, free; dedicated servers cannot listen on P2P because they do not log in ([Redpoint docs](https://docs.redpoint.games/docs/setup/networking/p2p_vs_ip), [Edgegap](https://edgegap.com/blog/can-epic-online-services-eos-relays-allow-for-dedicated-server-or-authoritative-server)) | No known mod **(unverified)**; use when the game ships on Epic or you want a store-neutral path |
| [LiteNetLib](https://github.com/RevenantX/LiteNetLib) | C# | MIT | Several reliable modes, fragmentation, UDP NAT hole punching, loss/latency simulation, works in Unity, Godot, MonoGame | Nitrox lists it as a dependency per [NuGet](https://www.nuget.org/packages/LiteNetLib/) (Nitrox is GPL-3.0, [repo](https://github.com/SubnauticaNitrox/Nitrox)) |
| Lidgren.Network | C# | MIT per its repo **(unverified, not fetched)** | Reliable UDP | Older Unity/XNA games; no source checked |
| [SLikeNet](https://github.com/SLikeSoft/SLikeNet) (RakNet fork) | C++ | RakNet-derived open source, per-component terms, see repo | NAT punchthrough server and client plugins, UPnP sample; RakNet itself is discontinued | RakNet use in game mods **(unverified)** |
| [yojimbo](https://github.com/mas-bandwidth/yojimbo) | C++ | BSD-3-Clause | Client/server, connect tokens, encrypted UDP, reliable-ordered and unreliable messages, 100 players or fewer | No mod use found; meant for dedicated servers, not P2P |
| [Riptide](https://github.com/RiptideNetworking/Riptide) | C# | MIT | UDP and TCP transports, separate Steam transport | None found |
| [Mirror](https://github.com/MirrorNetworking/Mirror) | C# (Unity) | MIT | KCP default; Telepathy TCP, WebSockets, community Steam transports (FizzySteam, FizzyFacepunch) | Games built on it, not typically mods |
| [FishNet](https://github.com/FirstGearGames/FishNet) | C# (Unity) | See repo | Unity framework | None found |
| [KCP](https://github.com/skywind3000/kcp) | C, two files | MIT | ARQ algorithm only (you supply UDP); claims 10-20% more bandwidth for 30-40% lower average latency than TCP, author's figures | Used inside Mirror |
| QUIC | n/a | n/a | Not covered: no mod example found **(unverified)** | |
| [libdatachannel](https://github.com/paullouisageneau/libdatachannel) | C/C++ | MPL-2.0 | WebRTC data channels over SCTP/DTLS, ICE with STUN and TURN, libjuice backend | None found; good if you need browser clients |
| [libjuice](https://github.com/paullouisageneau/libjuice) | C | MPL-2.0 | Lightweight ICE agent, STUN and TURN, UDP, no dependencies | sm64coopdx's build docs mention compiling libjuice ([search summary](https://github.com/coop-deluxe/sm64coopdx), **purpose unverified**) |
| Plain UDP | any | n/a | You write reliability, fragmentation, handshakes | Many small mods; the UE1 and Source netcodes a proxy tunnels are plain UDP |

Notes on mods, as far as sources go:

| Mod | Known networking facts | Link |
|---|---|---|
| TiltedEvolution / Skyrim Together Reborn | GPLv3; submodules include TiltedConnect (serialization plus a layer over a network library), cpp-httplib; server image needs UDP 10578 forwarded | [repo](https://github.com/tiltedphoques/TiltedEvolution), [.gitmodules](https://raw.githubusercontent.com/tiltedphoques/TiltedEvolution/master/.gitmodules), [Docker Hub](https://hub.docker.com/r/tiltedphoques/st-game-server) |
| Nitrox (Subnautica) | Client/server design, GPL-3.0, LiteNetLib per NuGet dependents | [repo](https://github.com/SubnauticaNitrox/Nitrox), [NuGet](https://www.nuget.org/packages/LiteNetLib/) |
| HKMP (Hollow Knight) | Host a port, forward it, or use Hamachi; LGPL-2.1; transport not stated on the repo page | [repo](https://github.com/Extremelyd1/HKMP) |
| SSMP (Silksong) | Steam networking, matchmaking service doing NAT hole punching, direct IP on port 26960 | [repo](https://github.com/Extremelyd1/SSMP) |
| Ship of Harkinian Anchor | Client connects to a configurable host, default `anchor.hm64.org` port 43383; transport protocol unverified | [source excerpt](https://code.moparisthebest.com/mirror/Shipwright/commit/060878fb2e75c5c2dbbea325169671cf3f9f122f) |
| sm64coopdx | Modes: local, direct IP (default 7777), CoopNet server, Discord integration | [launcher docs](https://mintlify.wiki/retired64/sm64coopdx_launcher/features/network), [repo](https://github.com/coop-deluxe/sm64coopdx) |
| NV:MP (New Vegas) | Host-based sessions with a synchronized host (press descriptions); transport not stated | [GameSpot](https://www.gamespot.com/articles/fallout-new-vegas-is-getting-a-multiplayer-mod/1100-6434268/), [Prima](https://primagames.com/gaming/fallout-new-vegas-multiplayer-mod-how-it-works-max-players-installation-and-more) |
| KeeperFX co-op | Built-in swappable `NetSP` layer with ENet and a Steam provider | [Spacewar page](networking-steam-spacewar.md) |
| xrMPE (STALKER) | Not researched here | see [games/stalker](../games/stalker/README.md) |

### Picking a library

- **C# / Unity / Mono game:** LiteNetLib (MIT, hole punching built in), plus a Steam transport if you want relay.
- **C++ native hook:** ENet for simplest; GameNetworkingSockets if you want one library with encryption and P2P; or call the Steamworks flat API directly with no SDK headers (the Spacewar page shows how).
- **Unity with an existing framework:** Mirror (KCP default, community Steam transports).
- **Game already has netcode:** do not replace it, tunnel it (section 4).
- **Avoid:** RakNet itself (discontinued per the [SLikeNet readme](https://github.com/SLikeSoft/SLikeNet)); pick SLikeNet only to match an existing RakNet game.

## 3. Connecting players without port forwarding

| Option | How it works | Pros | Cons |
|---|---|---|---|
| **Steam relay, AppID 480 Spacewar** | Lobbies + ISteamNetworkingMessages by SteamID, relayed over Valve's backbone ([Steam networking docs](https://partner.steamgames.com/doc/features/multiplayer/networking)) | No IPs, no ports, invite and Join Game through the overlay, free | Both players show "playing Spacewar"; lobbies visible to all 480 users unless filtered; cannot ship on Steam; docs describe 480 as a sample app only ([Steamworks example](https://partner.steamgames.com/doc/sdk/api/example)); ToS stance **unverified**: no Valve source found either allowing or forbidding mods using 480, and pirated games also use 480 ([mein-mmo](https://mein-mmo.de/spacewar-raubkopierer-steam/), secondhand) |
| Your own Steam AppID | Same API, your id ($100 Steam Direct fee **unverified**) | Proper name and Workshop; no 480 collisions | Needs a store entry; a mod of someone else's game cannot claim it **(judgment)** |
| EOS P2P | NAT punch with relay fallback, account-id addressing, free ([Redpoint](https://docs.redpoint.games/docs/setup/networking/p2p_vs_ip)) | Store-neutral, works for Epic and non-Steam owners | Each player needs an Epic account; no control over routing; extra relay hop; packet size limits **unverified**; relay "not intended" as a dedicated server ([Edgegap](https://edgegap.com/blog/can-epic-online-services-eos-relays-allow-for-dedicated-server-or-authoritative-server)) |
| NAT punch with rendezvous server | Both peers send to each other via a known server's observed addresses (ICE/STUN, libjuice, LiteNetLib, SLikeNet plugin) | You control it; SSMP's matchmaking service does exactly this ([SSMP](https://github.com/Extremelyd1/SSMP)) | You host a server; symmetric NAT defeats it, so you need a relay too |
| Hosted relay (TURN or self-built) | UDP forwarder on a public VPS | Always works | You pay for bandwidth and run it; libjuice includes a light STUN/TURN server ([libjuice](https://github.com/paullouisageneau/libjuice)) |
| Community dedicated servers | Players rent a server; Skyrim Together's server runs from a Docker image on UDP 10578 ([Docker Hub](https://hub.docker.com/r/tiltedphoques/st-game-server)) | Best for 24/7 and many players | Cost; not zero-setup |
| VPN overlay | Virtual LAN across the internet | Works for every game with an IP field, no code | Everyone installs and joins it |
| UPnP / NAT-PMP | Ask the router to open a port ([MiniUPnP](https://github.com/miniupnp/miniupnp), BSD-3-Clause) | Automatic when it works | Many routers disable it; no help with CGNAT |
| Port forwarding | Manual | Zero overhead | Hardest for non-technical friends |

VPN overlays in detail:

| Overlay | Facts | Source |
|---|---|---|
| Tailscale | Direct UDP when possible, DERP relay fallback (slower); Personal plan is free for non-commercial use, up to 6 users | [connection types](https://tailscale.com/kb/1257/connection-types), [pricing](https://tailscale.com/pricing/) via [search](https://tailscale.com/kb/1251/pricing-faq) |
| ZeroTier | MPL-2.0 for most of the repo; relays traffic when peer-to-peer fails, "free (but slow)" | [ZeroTierOne](https://github.com/zerotier/ZeroTierOne) |
| Hamachi | Free plan: 5 members per network | [GoTo support](https://support.goto.com/hamachi/help/logmein-hamachi-subscription-types-hamachi-c-hamachi-subscriptions) |
| Radmin VPN | Claimed unlimited peers on free tier by a third-party listing only **(unverified)** | [agentdeals](https://agentdeals.dev/vendor/radmin-vpn) |

### Recommendation

- **Default for small friend groups (2-4):** Steam relay through ISteamNetworkingMessages, with a lobby plus friend-list discovery, as in the [Spacewar page](networking-steam-spacewar.md). No port, no IP, no server cost. If the mod is a standalone product with its own Steam App, use your own AppID instead of 480.
- **Fallback:** Tailscale (or ZeroTier) overlay with direct IP:port, because it needs no code in the mod. Keep a manual IP field working in the mod and offer UPnP as a convenience.
- **Non-Steam audience:** EOS P2P, or an NAT-punch matchmaking server as SSMP does.
- Always keep the LAN/direct-IP path working; every other option reduces to it and it is the easiest to test.

## 4. Wrapping a game's existing UDP netcode (local proxy)

Use this when the engine's multiplayer is closed, hard to change, or already
works on a LAN (Unreal Engine 1 in Deus Ex, Source-like games, many older shooters).
The mod does not touch the engine: a helper process sits next to the game and moves its
packets over a better transport. This repo's HXCoop does this for Deus Ex; the
step-by-step relay rules are on the [Spacewar page](networking-steam-spacewar.md#a-relay-instead-of-engine-code-hxcoop-pattern).

```
Client PC                                   Host PC
game --UDP--> 127.0.0.1:P  [proxy] ==Steam/EOS/other== [proxy] --UDP--> game server 127.0.0.1:S
```

Rules that make it work (all from the HXCoop pattern unless noted):

| Rule | Why |
|---|---|
| Host proxy opens one UDP socket per remote player, bound to a distinct local port and `connect()`ed to the server port | The game server identifies clients by source address and port; sharing one socket makes all players look like one client |
| Client proxy binds 127.0.0.1 on a free port and the game is told to join that address | The game believes it talks to a LAN server |
| Give every tunnelled player a connection id | Games that disconnect and reconnect on map change must resume the same mapping, not create a new player slot |
| Send game datagrams unreliable; tunnel control on a reliable side channel | The game has its own reliability; double reliability stalls. Keep-alive, goodbye and kick on the reliable channel |
| Keep-alive plus idle timeout | Relays such as ISteamNetworkingMessages do not report peer-initiated closes ([docs](https://partner.steamgames.com/doc/api/ISteamNetworkingMessages)) |
| Exit with the game process; clear lobby and rich presence | No orphaned proxies |
| Respect the MTU: UDP datagrams over about 1200 bytes may fragment | Game packets are usually small (UE1 about 512 bytes per the Spacewar page); check yours before skipping fragmentation |
| Test with the engine's own lag emulation | UE1 has `PktLag`, `PktLoss`, `PktOrder`, `PktDup`; modern UE uses `NetEmulation.` ([Unreal docs](https://dev.epicgames.com/documentation/unreal-engine/using-network-emulation-in-unreal-engine)) |

When not to proxy: if the engine cannot host a listen server with your content, or
the game's netcode does not carry quest state at all. Then the proxy only moves bytes
and you still need a sync layer on top (section 1).

## 5. Testing netcode

### 5.1 Emulating bad networks

| Tool | Platform | Use | Source |
|---|---|---|---|
| clumsy | Windows | Lag, drop, tamper with packets, system-wide, no code changes, works for localhost-to-localhost; MIT; built on WinDivert | [clumsy](https://github.com/jagt/clumsy) |
| tc netem | Linux | `tc qdisc add dev eth0 root netem delay 100ms 10ms loss 0.3% duplicate 1%`; reordering needs a delay; add a pfifo child to stop jitter reordering | [man page](https://man7.org/linux/man-pages/man8/tc-netem.8.html) |
| Unreal `NetEmulation.PktLag/PktLoss/PktDup/PktOrder` | Unreal 4/5 | Runtime console, command line `SETTING=VALUE`, or `[PacketSimulationSettings]` ini; incoming settings too | [Unreal docs](https://dev.epicgames.com/documentation/unreal-engine/using-network-emulation-in-unreal-engine) |
| LiteNetLib built-in | C# | Packet loss and latency simulation in the library | [LiteNetLib](https://github.com/RevenantX/LiteNetLib) |
| Loopback + clumsy | Windows | Two game instances on one PC | [clumsy](https://github.com/jagt/clumsy) |

Suggested profiles **(judgment)**: 50 ms RTT clean; 150 ms RTT with 30 ms jitter and
2% loss; 300 ms RTT with 5% loss and 1% duplicates; one 3 s total blackout.

### 5.2 One PC, two players

- **Mock Steam DLL:** build a fake `steam_api.dll` with the same flat exports, fake SteamIDs from a `-nick` argument, localhost UDP between the two processes, deterministic lobby ids, and the real "hold first message until AcceptSession" behavior. Details and what to extend are in the [Spacewar page](networking-steam-spacewar.md#testing-without-two-steam-accounts-the-mock-dll).
- Real Steam allows one account per PC; Steam Networking Messages need real accounts for a genuine relay test. A real two-account test is a separate, human step and should be listed as untested until done.
- Run each client in its own copy of the install (see [AGENTS.md](../AGENTS.md) rule 2).

### 5.3 What to assert

| Test | Pass condition |
|---|---|
| Join, leave, rejoin | Slot reuse correct, no ghost entities |
| Kill -9 a client | Host drops it within the keep-alive timeout |
| Kill the host | Clients exit cleanly or return to menu, no hang |
| Map change on host | Clients follow or hold, no desync |
| Late join mid-quest | Same quest flags as the host's |
| Save and reload during a session | State matches after reload |
| Loss and jitter profiles from 5.1 | No crash, state converges within N seconds |
| Soak | 30-60 minutes of scripted play (bot input) with periodic state hash comparisons; report mismatches and memory growth **(judgment)** |

State hashes: have both ends hash a canonical list (flags, inventory, positions
rounded) every few seconds and log it; a diff of two logs is a cheap desync detector.
For bot-driven runs see [automated-qa.md](automated-qa.md).

## 6. Pitfalls checklist

- Pin the game build (SHA-256 of the exe) before reusing offsets or hooks from someone else's mod.
- Do not use reliable-ordered for movement; one lost packet stalls everything behind it ([Gaffer](https://gafferongames.com/post/deterministic_lockstep/) on TCP stalls).
- Do not rely on pointers or memory addresses in the protocol; use ids stable across sessions and machines.
- Do not trust "peer disconnected" callbacks: add keep-alives (ISteamNetworkingMessages does not report peer closes).
- Do not assume the relay's addressing equals IP:port; wrap addresses in your own player id.
- Anti-cheat: modding an online anti-cheat game for co-op is out of scope here (see AGENTS.md rule 10); Seamless Co-op for Elden Ring blocks the game's online service to stay on a separate path ([guide](https://www.exitlag.com/blog/elden-ring-seamless-co-op/), secondhand).
- Licensing: GPL libraries force your mod to GPL if you distribute it linked; prefer MIT or BSD (ENet, LiteNetLib, GameNetworkingSockets, yojimbo) unless the mod is GPL already.
- Write down unknowns instead of guessing: every "(unverified)" above is an invitation to test and send a PR.

## Sources not fetched or thin

- Epic's official EOS P2P reference pages returned 404 for every URL tried; EOS facts above come from third-party docs.
- Valve's Source multiplayer networking wiki returned 403; Source-style lag compensation is not cited.
- Skyrim Together, NV:MP, Seamless Co-op, sm64coopdx and Anchor internals: only repository pages and search summaries were available. Read their source before copying a design.
