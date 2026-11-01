# How existing co-op mods work

Goal: give an agent with no prior mod to lean on enough background to build co-op for a
single-player game from scratch. This page covers the problems every project solves, how
the notable projects solved them, the tooling per engine type, and a build order.

Sourcing rules for this page: a claim about a project has a link. Where the fetched
sources did not say, the page says `unknown` instead of guessing. "Observed" means a
fact from a README or docs page; "inferred" means a reading of file names or layout.
Nothing here replaces reading the project's own source before copying its design.

Related pages: [engine-families.md](engine-families.md),
[reverse-engineering.md](reverse-engineering.md), [methods.md](methods.md),
[automated-qa.md](automated-qa.md), [reviving-abandoned-mods.md](reviving-abandoned-mods.md).

---

## 1. The problems every project solves

Every project below answers the same eight questions. Decide your answer to each before
writing netcode.

### 1.1 Remote players on screen (puppets)

A single-player game has exactly one player object wired to input, camera and UI. A
remote player is a second object that looks like a player but takes its state from the
network instead of from input.

- sm64coopdx indexes players locally: `gMarioStates[0]` is always the local player, the
  order of `gMarioStates`, `gNetworkPlayers` and `gPlayerSyncTable` differs on every
  client, and a global index is the stable identifier
  ([Lua globals docs mirror](https://bytes.keithhacks.cyou/etc/sm64coopdx/raw/tag/v1.0.4/docs/lua/globals.md)).
- Nitrox lists "player movement, basic animations, and a HUD for remote players" as
  completed work ([Nitrox README](https://github.com/SubnauticaNitrox/Nitrox)).
- HKMP is described by community guides as putting "a copy of each player into each
  other player's world" ([HKMP community guide](https://geroyuni.notion.site/hkmp)).

Pattern: spawn one puppet per remote player, drive it from received position, rotation,
animation and equipment state, interpolate between updates, and never let a puppet read
local input.

### 1.2 Who owns each entity (authority)

Someone must decide what an NPC does. Options seen in the wild:

| Model | Example | Source |
|---|---|---|
| One host runs all AI, others mirror | OOT True Co-op: one instance "runs enemy AI. The other instance displays the same enemies"; hits from the other player are checked by the authority | [OOT True Co-op](https://github.com/bghill95/OOT-True-Co-op) |
| Per-area authority held by one player | TES3MP `Cell` stores an authority as a RakNet GUID with `setAuthority` and `hasLocalAuthority` | [Cell.hpp](https://git.cijber.net/mirror/openmw-tes3mp/src/branch/master/apps/openmw/mwmp/Cell.hpp), [search summary of Cell.cpp commits](https://git.cijber.net/mirror/openmw-tes3mp/src/commit/060ebe3d4ac2e78d4acf094a4979de1e29cee7aa/apps/openmw-mp/Cell.cpp) |
| Server holds canonical state, clients send actions | Nitrox, per a third-party docs mirror (secondary source, verify upstream) | [mirror](https://www.mintlify.com/Papela/Nitrox-Cracked-Mod/development/networking) |
| Party leader's world is the source of truth | Skyrim Together Reborn parties | [Wabbanode party guide](https://wabbanode.com/help/skyrim-together/understanding-parties-in-skyrim-together-reborn) |
| Nobody owns it: each client simulates its own copy | HKMP per community guides: "Enemies have separate health" | [HKMP guide](https://geroyuni.notion.site/hkmp) |
| Engine's own replication decides | Unreal Engine 1 (HX) | see 2.9 |

Expect to need an authority hand-off rule (what happens when the owner leaves a cell or
disconnects). OOT True Co-op documents one: if one game crashes the other's enemies
pause for about 1.5 seconds, then resume under local AI
([README](https://github.com/bghill95/OOT-True-Co-op)).

Copy results, not events. When an NPC dies on the owner's machine, show it dead elsewhere as a *state*, without
running the game's own death logic again: in Gothic 3 the death script also advanced quests, paid experience
and rolled loot, so every mirrored death did those twice on the client until the copy set only the dead state
(G3 Together, [games/gothic-3/NOTES.md](../games/gothic-3/NOTES.md), stage 3c and C). The same holds for
dialog side effects: passing on an NPC's resulting state is easier than interpreting each command.

### 1.3 World, quest and story state

This is the part most projects skip, and it is where single-player games are hardest.

- TES3MP claims "very extensive player, NPC, world and quest synchronization" and
  needs a sync whitelist for client-side script variables "to avoid packet spam"
  ([README](https://github.com/TES3MP/openmw-tes3mp)). Possible because the engine is
  open source.
- Skyrim Together Reborn syncs quests through the party leader and tells members to
  wait for the leader before advancing quests
  ([Wabbanode](https://wabbanode.com/help/skyrim-together/understanding-parties-in-skyrim-together-reborn)).
- Ship of Harkinian Anchor syncs "flag sets and item gives" to all players in a room or
  team ([Anchor README](https://raw.githubusercontent.com/garrettjoecox/Anchor/main/README.md)).
- Archipelago syncs items only; it is a randomizer framework, not a world-state mod
  ([Archipelago](https://github.com/ArchipelagoMW/Archipelago)).
- Observed in our own NV:MP analysis (closed source, so a local finding, not a public
  source): the story server has no quest sync and quest actors are deliberately left
  unsynced. Treat as `unverified` for anyone else's build.

Practical rule: find the game's flag/variable store (quest stages, globals, event
flags). If everything story-related goes through that store, replicate writes to it from
one authority and you have most of story co-op.

### 1.4 Saves and level transitions

- Skyrim Together Reborn: saves live on the party leader's PC, not the server
  ([search summary of guides](https://wabbanode.com/help/skyrim-together/understanding-parties-in-skyrim-together-reborn)
  covers quicksave advice; the "saves on leader PC" claim comes from secondary guides,
  verify).
- DK2 co-op: "Campaign progress and unlocks are saved on the host's PC only"
  ([DK2 co-op](https://ciphercom.github.io/dk2-coop)).
- Anchor loads a save state from a remote player when you join their session
  ([Anchor](https://raw.githubusercontent.com/garrettjoecox/Anchor/main/README.md)).
- Seamless Co-op's selling point is that sessions persist across the whole world instead
  of ending after each boss ([Gamewatcher, secondary](https://www.gamewatcher.com/elden-ring-seamless-co-op-mod)).

Design question to settle early: whose save is canonical, and what do other players
persist (usually nothing, or inventory only).

A save can also be the join mechanism for a single mission. BattleTech co-op does not
regenerate a campaign contract on the guest (procedural content and story scripts read the
whole campaign state); the host builds a combat save in memory at the mission briefing and
sends it, and the guest loads it as a "restart mission", so both launch the same mission
fresh and both see its opening (a loaded save differs from the live state, and a restart
re-rolls random picks, see [lessons.md](lessons.md) 74 and 79). The guest never saves, because it is running the
host's campaign and would write into its own save slots
([games/battletech](../games/battletech/README.md)).

Check what a save captures while remote players are present. In Gothic 3 the remote player's body is a copy
of the hero; a save taken with it in the world aborts on load ("more than one hero"), so the save function
itself has to be hooked to remove those bodies first, also for saves from the game's menu. A small sidecar
file next to each save can carry the mod's own state, and each player can keep their own character record per
host save; a joining player then loads the host's save (sent over the mod's own connection) and puts their own
character back on top (G3 Together, stage D in [games/gothic-3/NOTES.md](../games/gothic-3/NOTES.md)).

### 1.5 Cutscenes and dialog

No source fetched gave a general solution. Observed advice and constraints:

- DK2 co-op says "Cutscenes should be allowed to finish"
  ([DK2 co-op](https://ciphercom.github.io/dk2-coop)).
- Skyrim Together Reborn tells only the leader to talk to quest NPCs and make dialogue
  choices; others see the choices as text notifications
  ([Wabbanode](https://wabbanode.com/help/skyrim-together/understanding-parties-in-skyrim-together-reborn)).
- Our own STALKER coop work found dialogue triggers use "whoever talks" as speaker while
  quest flags are server-global (local finding from reading the addon scripts).

Typical options: initiator picks, leader picks, or a vote. Cutscenes are either played
on all clients with a barrier, or skipped for non-hosts. Mark this `unknown` for any new
game until you have tested it.

### 1.6 Lockstep versus state sync

| | Deterministic lockstep | Host-authoritative state sync |
|---|---|---|
| Sends | player inputs only | entity state (positions, health, flags) |
| Bandwidth | tiny, independent of object count | grows with entity count |
| Requires | bit-exact determinism, same fixed timestep, same initial state | no determinism; a way to read/write entity state |
| Fails as | desync (silent divergence) | rubber-banding, duplicated or missing entities |
| Good for | RTS-like games, decompiled or rewritten engines you can make deterministic | retrofitting onto an unmodified closed game |

Lockstep facts from [Gaffer on Games](https://gafferongames.com/post/deterministic_lockstep/):
send only inputs; both sides need the same initial state, inputs and fixed timestep;
frame n cannot run until input n arrives; determinism must hold "down to the bit-level"
and may break across compilers, OSes and debug versus release floating point;
checksum the whole state each frame to detect desync.

KeeperFX is a candidate for lockstep, but the fetched README only says "modern
multiplayer protocol" and does not state the model
([KeeperFX](https://github.com/dkfans/keeperfx)). `unknown` from public docs; check the
source (`net` and packet code) before assuming.

State sync, bandwidth and priority: [Gaffer on Games, state
synchronization](https://gafferongames.com/post/state_synchronization/) describes a
per-object priority accumulator, sending the highest accumulated first within a
bandwidth budget, and delta compression against acknowledged baselines.

Retrofits onto closed single-player games are almost always host-authoritative state
sync, because you cannot make the original engine deterministic.

Exception: a game that shipped its own versus mode may already be deterministic where it
counts. BattleTech (2018) routes every combat action as an `InvocationMessage` through one
handler that reseeds the logic RNG from the message's own seed, for its skirmish PvP. The
single-player campaign runs the same path, so co-op combat there is lockstep on those
messages with the host ordering them; only the AI is not deterministic (wall-clock think
budget), so only the host's AI thinks and its orders go out as ordinary messages
([games/battletech](../games/battletech/README.md)). Check for such a layer before choosing
state sync.

### 1.7 Interest management

Send each client only what is near it or matters. Evidence in sources:

- The Gaffer priority accumulator above is the simplest form (priority by importance,
  not distance).
- Nitrox lists "long-range world syncing" as a feature, and a docs mirror says exact
  creature positions are not synced while spawns, health and deaths are
  ([Nitrox README](https://github.com/SubnauticaNitrox/Nitrox),
  [mirror, secondary](https://www.mintlify.com/Papela/Nitrox-Cracked-Mod/features/synchronization)).
- TES3MP scopes authority and actor updates to cells (see 1.2).
- Unreal's relevancy rules and Source's PVS are the engine-native versions. The Valve
  and Unreal docs pages returned HTTP 403 to our fetcher, so no quote is given here.

Start without interest management. Add it when profiling shows bandwidth or CPU cost.

Measure the engine's own processing range early: it limits host authority more than bandwidth does. Gothic 3
only simulates entities within about 40 m of the camera (`Entity.ROI`); far entities exist but are frozen,
and state written into them (a kill) was reset when they were processed again. Results from far regions
therefore go into a journal that each machine applies when it starts processing that entity
(G3 Together, explorations 1 and 5 and stage C in [games/gothic-3/NOTES.md](../games/gothic-3/NOTES.md)).

### 1.8 Late join

A joining client must be brought to the current state. Seen approaches:

- Anchor: load a save state from a remote player
  ([Anchor](https://raw.githubusercontent.com/garrettjoecox/Anchor/main/README.md)).
- TES3MP server-side Lua scripts "save and load the state of most of the synchronized
  data" ([search summary](https://git.cijber.net/mirror/openmw-tes3mp/src/commit/060ebe3d4ac2e78d4acf094a4979de1e29cee7aa/apps/openmw-mp/Cell.cpp);
  secondary snippet, verify).
- Skyrim Together Reborn: joiner follows the leader's world
  ([Wabbanode](https://wabbanode.com/help/skyrim-together/understanding-parties-in-skyrim-together-reborn)).

Build "snapshot on join" (full state dump, then deltas) early. It also doubles as your
reconnect path and your test oracle.

---

## 2. Projects

Each entry: hook method, authority/transport, what is synced, what is unknown.

### 2.1 Tilted Online and Skyrim Together Reborn (TiltedEvolution)

- Repo: [tiltedphoques/TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution).
  A framework for multiplayer in Bethesda games, currently Skyrim Special Edition.
- Layout (observed): `Code/client` holds "engine hooks, reverse-engineered game types
  and the sync logic"; `Code/server` builds `STServer.dll`, hosted by `server_runner`;
  `Code/encoding` has network messages shared by client and server;
  `Code/immersive_launcher` starts the game with the client loaded; `Code/skyrim_ui`
  is an Angular UI in a Chromium Embedded Framework overlay; build is xmake
  ([README](https://github.com/tiltedphoques/TiltedEvolution)).
- Submodules: TiltedReverse, TiltedUI, TiltedConnect, TiltedHooks, TiltedScript,
  cpp-httplib ([.gitmodules](https://raw.githubusercontent.com/tiltedphoques/TiltedEvolution/master/.gitmodules)).
  TiltedConnect is "the top layer of the network stack" over a network library, and its
  submodules include a fork of Valve's GameNetworkingSockets
  ([TiltedConnect](https://github.com/tiltedphoques/TiltedConnect),
  [its .gitmodules](https://raw.githubusercontent.com/tiltedphoques/TiltedConnect/master/.gitmodules)).
- Authority: dedicated server plus party leader hosting the world (see 1.2).
- Unknown from fetched sources: exact hook technique (TiltedHooks exists, contents not
  read), per-entity ownership rules, full sync list.
- Takeaway: the layered design (hooks lib, reverse-engineered game types, shared message
  definitions, server DLL) is a good template for a multi-game framework.

### 2.2 Seamless Co-op (Elden Ring, Dark Souls)

- Author: LukeYui. The GitHub repo is only a mirror of the Nexus page
  ([repo](https://github.com/LukeYui/EldenRingSeamlessCoopRelease)); no source
  or technical write-up was found. Source is closed, so the architecture is `unknown`.
- Observed behaviour from secondary reports: it uses its own launcher and does not use
  the vanilla matchmaking or FromSoftware online services, replaces summon/leave flow
  with item-based session start, keeps sessions persistent across the world, and
  disables invasions
  ([Gamewatcher](https://www.gamewatcher.com/elden-ring-seamless-co-op-mod),
  [Prima Games](https://primagames.com/tips/how-to-download-and-use-the-elden-ring-seamless-co-op-mod)).
- Correction to a common assumption: the sources found do not say it extends the
  game's summoning. Do not describe it that way without reading the binary. The
  reportedly useful lesson is product-level: take a game that already has co-op
  plumbing and change session rules (persistence, no boss-ends-session).

### 2.3 sm64coopdx (Super Mario 64 decomp port)

- Repo: [coop-deluxe/sm64coopdx](https://github.com/coop-deluxe/sm64coopdx), successor
  of [djoslin0/sm64ex-coop](https://github.com/djoslin0/sm64ex-coop) (archived), a fork
  of the sm64ex PC port. It "synchronizes all entities and every level" and, unlike
  earlier projects, syncs enemies and events.
- Hook method: none needed; it modifies decompiled C source directly (a decomp port is
  the game's source). Contributor docs live in `docs/c/c.md` and `docs/lua/lua.md`.
- Sync API (Lua): per-player `gPlayerSyncTable[]` fields "automatically synchronized with
  all other clients"; objects registered with `network_init_object` and pushed with
  `network_send_object`
  ([Lua globals](https://bytes.keithhacks.cyou/etc/sm64coopdx/raw/tag/v1.0.4/docs/lua/globals.md),
  [manual.lua](https://bytes.keithhacks.cyou/etc/sm64coopdx/src/commit/96e21e13a1231e8e0268e0f01df8eca5243246b3/autogen/lua_definitions/manual.lua);
  mirror of an older tag, API may have moved).
- Unknown from fetched sources: transport library, topology details.
- Takeaway: the model most decomp co-op projects copy is puppet players plus
  per-object sync with explicit registration.

### 2.4 Ship of Harkinian Anchor, OOT True Co-op (Ocarina of Time)

- [Anchor](https://github.com/garrettjoecox/Anchor): a generic Go relay server (run with
  `go run .` or Docker, default port 43383); the game client "handles all game state".
  Syncs save state on join, flag sets and item gives within a room or team.
  ([README](https://raw.githubusercontent.com/garrettjoecox/Anchor/main/README.md)).
- [OOT True Co-op](https://github.com/bghill95/OOT-True-Co-op): fork of Ship of
  Harkinian plus Anchor for two players sharing one world on one PC. Adds shared enemy
  HP, mirrored enemy position and animation, and bosses. One "authority" instance runs
  enemy AI; the other displays. New bosses need an `ActorSyncAdapter`, not new sync
  code. Known gaps: enemies aim at the authority's Link, AI sounds and particles only
  on the authority, `EnemySyncMirroring` kill switch falls back to shared HP.
- Takeaway: flags/inventory sync gets you a long way cheaply. Enemy authority is the
  frontier and benefits from an adapter interface per actor type plus a kill switch.

### 2.5 xrMPE and STALKER co-op

- xrMPE is an X-Ray multiplayer extension; co-op ships as add-ons for the launcher
  ([xrMPE launcher releases](https://github.com/M4dMurloc/xrMPE_Launcher_Release); README
  has no technical content). A Japanese news item calls version 0.9 a Call of Pripyat
  multiplayer mod with Steam integration and four-player co-op
  ([Game*Spark](https://www.gamespark.jp/article/2025/11/29/159983.html)).
- Whether it is a full engine fork: a deleted ModDB page described an "X-Ray
  Multiplayer Engine-fork"; no repo found. Treat as `unconfirmed`.
- Local finding from unpacking one co-op addon (not a public source): game logic is
  X-Ray Lua scripts and LTX configs in `.db` archives, quest flags are server-global and
  broadcast to clients, and the server checksums the addon, so every player needs
  identical copies. Reproduce before relying on it.
- Takeaway: a game with its own multiplayer engine and scriptable game logic needs
  script-level porting, not netcode.

### 2.6 TES3MP (Morrowind on OpenMW)

- [openmw-tes3mp](https://github.com/TES3MP/openmw-tes3mp): a multiplayer fork of the
  OpenMW engine; server behaviour is customised with Lua scripts
  (CoreScripts repo); remaining problems mostly concern AI.
- Authority: `Cell` holds an authority identified by a RakNet GUID; local versus
  dedicated actors are separate concepts handled by `CellController`
  ([Cell.hpp](https://git.cijber.net/mirror/openmw-tes3mp/src/branch/master/apps/openmw/mwmp/Cell.hpp),
  [CellController.hpp](https://git.cijber.net/mirror/openmw-tes3mp/src/commit/73aa83aa03416ed748d108dd26b675d9846f8173/apps/openmw/mwmp/CellController.hpp)).
- Takeaway: with an open engine, you get the strongest result (full quest sync) and a
  clean split: engine fork for sync hooks, Lua on the server for policy.

### 2.7 Nitrox (Subnautica, Unity)

- [Nitrox](https://github.com/SubnauticaNitrox/Nitrox): projects `NitroxPatcher`,
  `NitroxClient`, `Nitrox.Server.Subnautica`, `Nitrox.Launcher`, `Nitrox.Model`.
  Lists "Integration with Harmony" as done; the patcher patches the game at runtime.
- Docs mirror (secondary): Harmony-based patcher with `Dynamic/` (multiplayer-only) and
  `Persistent/` patches; launcher starts the game pointing at Nitrox libraries; 130+
  typed packets; LiteNetLib-based UDP; server-authoritative
  ([architecture](https://www.mintlify.com/Papela/Nitrox-Cracked-Mod/development/architecture),
  [networking](https://www.mintlify.com/Papela/Nitrox-Cracked-Mod/development/networking)).
  Verify against the upstream wiki before copying.
- Synced per README: movement, animations, item pickup/drop, base building, power,
  vehicles, chat.
- Takeaway: a Unity game with a mod loader and Harmony is the easiest modern target.
  Patch the methods that spawn or mutate things, serialize them as packets, apply on
  the server.

### 2.8 HKMP (Hollow Knight, Unity)

- [HKMP](https://github.com/Extremelyd1/HKMP) is a Hollow Knight mod. Addons are
  assemblies loaded like Hollow Knight mods, in separate client and server halves; the
  standalone server "has no notion of the game code"; packets carry IDs from an enum,
  with reliability flags such as `DropReliableDataIfNewerExists`
  ([wiki](https://github.com/Extremelyd1/HKMP/wiki)).
- Sync scope per guides: player positions mostly; enemies, items and breakables are
  per-player ([guide](https://geroyuni.notion.site/hkmp)).
- Unknown from fetched sources: hook method details and transport internals.
- Takeaway: a game-agnostic server plus game-specific client addons keeps the server
  simple and lets the community extend sync per feature.

### 2.9 HX for Deus Ex (Unreal Engine 1)

- HX plays the single-player campaign in co-op; the ModDB page is deleted
  ([ModDB](https://www.moddb.com/mods/hx)). A maintained successor stack with
  fixes is [cranebeaker/deus-ex-coop](https://github.com/cranebeaker/deus-ex-coop), and a
  Docker dedicated server is [MisterCalvin/dxhx-coop-server](https://github.com/MisterCalvin/dxhx-coop-server).
- Local finding: `HX.u` embeds its UnrealScript source (hundreds of classes), a native
  `HX.dll` has no source, and fixes ship as a separate package that subclasses HX
  classes instead of rebuilding `HX.u`. See the repo's notes for method.
- Takeaway: on Unreal Engine 1 and 2 games, co-op is mostly script work because the
  engine already replicates actors. See also
  [engine-families.md](engine-families.md).

### 2.10 NV:MP (Fallout New Vegas)

- Co-op mod with a dedicated story server and a client DLL; `partial` story state per
  [Co-Optimus](https://www.co-optimus.com/article/13884/fallout-new-vegas-co-op-mod-needs-beta-testers.html)
  (page returned HTTP 403 to our fetcher; claim carried from the directory entry).
- Netcode is closed source; findings from our own reverse engineering are local and
  version-specific (see 1.3). Public lesson: a closed mod that does not sync quests
  can be complemented by a separate script-extender plugin that syncs the quest store.

### 2.11 RAGECOOP (GTA V story mode)

- [RAGECOOP-V](https://github.com/RAGECOOP/RAGECOOP-V): requires ScriptHookV and
  ScriptHookVDotNet 3.6.0+, uses a modified Lidgren network library, syncs bullets,
  vehicles, players, NPCs, projectiles and simple ragdoll states, supports story mode,
  and has a scripting and resource API
  ([README](https://raw.githubusercontent.com/RAGECOOP/RAGECOOP-V/main/README.md)).
- Unknown from fetched sources: server architecture, tick rate, conflict handling.

### 2.12 KeeperFX and DK2 co-op

- [KeeperFX](https://github.com/dkfans/keeperfx): open source (GPL-2.0), began as a
  decompilation, now a rewritten C/C++ codebase with "modern multiplayer protocol".
  Sync model not stated in the README (see 1.6).
- [DK2 co-op](https://ciphercom.github.io/dk2-coop): built on
  [Flame](https://github.com/DiaLight/Flame); two players share one Keeper faction;
  UDP port 7575; both need the same game version and package version; progress saved
  on the host only. Sync model not stated.

### 2.13 Nucleus Co-op (split screen)

- [Nucleus Co-op](https://github.com/SplitScreen-Me/splitscreenme-nucleus) runs several
  instances of one game, binds each to a gamepad, and arranges windows. It symlinks game
  files, sometimes kills the single-instance mutex, uses XInput replacement libraries,
  and relies on LAN or online-emulation (Goldberg, Nemirtingas) to connect instances
  ([README](https://raw.githubusercontent.com/SplitScreen-Me/splitscreenme-nucleus/master/README.md)).
- It does not add co-op to a single-player game: the game must already have a way to
  connect instances. Useful for your two-client test rig, not as a co-op solution.

### 2.14 Archipelago (item sync)

- [Archipelago](https://github.com/ArchipelagoMW/Archipelago): multiworld randomizer
  framework with a central server (`MultiServer.py`, inferred from file name) and
  per-game clients (`*Client.py`, inferred). It syncs items between worlds; players do
  not share a space. Cheapest form of "co-op": a trade or shared-pool layer.

### 2.15 Other approaches to note

- Shared-keeper "Archon mode" in KeeperFX is upstream work in progress
  ([KeeperFX PRs](https://github.com/dkfans/keeperfx/pulls)); check before forking.
- ACGC-PC-Multiplayer (Animal Crossing, built on a decomp PC port) is host-authoritative
  with puppet players, per its repository
  ([ACGC-PC-Multiplayer](https://github.com/vsimochi/ACGC-PC-Multiplayer), `multiplayer`
  branch).

---

## 3. Tooling by engine type

### 3.1 Hooking and injection

| Tool | Use | Source |
|---|---|---|
| Script extenders (SKSE, F4SE, xNVSE) | Supported DLL plugin mechanism for Bethesda engines; SKSE loads plugins from `Data/SKSE/Plugins` and supports specific game builds only | [SKSE](http://skse.silverlock.org/) |
| BepInEx | Plugin framework for Unity Mono, IL2CPP and .NET (XNA, FNA, MonoGame); ships HarmonyX; uses UnityDoorstop | [BepInEx](https://github.com/BepInEx/BepInEx) |
| MelonLoader | Mod loader for Unity Il2Cpp and Mono; uses a proxy DLL (`version.dll` by default, also `winhttp.dll`, `winmm.dll`, `dinput8.dll`, `d3d11.dll`) | [MelonLoader](https://github.com/LavaGang/MelonLoader) |
| Harmony | Runtime patching of .NET/Mono methods: run code before/after, or modify IL; touches no files | [Harmony](https://github.com/pardeike/Harmony) |
| UE4SS | Injectable modding for UE 4.7 to 5.8: Lua, C++ and Blueprint mods, SDK/UHT dumping, live property editing; may need AOB signature updates per game | [RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) |
| REFramework | Mod loader and Lua scripting for RE Engine (RE2/3/4/7/Village, DMC5, SF6, MH Rise/Wilds, Dragon's Dogma 2, more) with a plugin system | [REFramework](https://github.com/praydog/REFramework) |
| MinHook | Minimal x86/x64 inline hooking with trampolines; queued enable for many hooks | [MinHook](https://github.com/TsudaKageyu/minhook) |
| Detours | Microsoft API hooking library (MIT, Windows) | [Detours](https://github.com/Microsoft/Detours) |
| Source SDK | Server/client split and map/entity scripting; see [engine-families.md](engine-families.md) | `unknown` (no page fetched) |
| UnrealScript (UE1/UE2) | Game logic is script; engine has built-in replication; ship a subclassing package | see 2.9 |
| Decomp + recompilers | Decomps give you the source; N64Recomp statically recompiles N64 binaries to C, hooks/function patches are listed as not yet working in its README; Zelda64Recomp supports mods | [N64Recomp](https://github.com/N64Recomp/N64Recomp), [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) |

Rule of thumb: prefer the highest-level supported hook the game offers (script, mod
loader, extender) and drop to inline hooks (MinHook, Detours) only for functions that
level cannot reach.

### 3.2 Networking libraries seen in these mods

| Library | Seen in | Properties | Source |
|---|---|---|---|
| GameNetworkingSockets | Tilted (via submodule) | Reliable and unreliable messages, fragmentation, AES-GCM encryption, lanes, P2P with NAT traversal; no Steam needed; no entity serialization or delta encoding included | [GNS](https://github.com/ValveSoftware/GameNetworkingSockets), [TiltedConnect](https://raw.githubusercontent.com/tiltedphoques/TiltedConnect/master/.gitmodules) |
| LiteNetLib | Nitrox (per docs mirror) | .NET reliable UDP: reliable ordered, unordered, sequenced; channels; NAT punch; loss simulation | [LiteNetLib](https://github.com/RevenantX/LiteNetLib) |
| Lidgren | RAGECOOP (modified) | .NET UDP library; no longer actively developed | [Lidgren](https://github.com/lidgren/lidgren-network-gen3), [RAGECOOP](https://raw.githubusercontent.com/RAGECOOP/RAGECOOP-V/main/README.md) |
| ENet | Used in our own host-to-client quest sync test | Reliable UDP in C, MIT | [ENet](https://github.com/lsalzman/enet) |
| RakNet | TES3MP (GUIDs in Cell authority; library use inferred) | `unknown` from fetched pages | [Cell.hpp](https://git.cijber.net/mirror/openmw-tes3mp/src/branch/master/apps/openmw/mwmp/Cell.hpp) |
| Custom UDP | HKMP (addon packet system), DK2 co-op (UDP 7575), Anchor (TCP-style relay, details unknown) | Own packet IDs and reliability flags | [HKMP wiki](https://github.com/Extremelyd1/HKMP/wiki), [DK2](https://ciphercom.github.io/dk2-coop) |

For tests on one machine, pick a library with built-in loss and latency simulation
(GNS and LiteNetLib both list it). See
[networking-steam-spacewar.md](networking-steam-spacewar.md) for the Steam path.

### 3.3 Reverse engineering for agents

| Tool | What it gives an agent | Source |
|---|---|---|
| Ghidra headless | Batch decompile of whole binaries; script it and grep the output | [reverse-engineering.md](reverse-engineering.md) |
| GhidraMCP | Java plugin runs an HTTP server inside Ghidra (default `localhost:8080`); Python MCP bridge; decompile, rename, list methods/classes/imports/exports; headless mode through PyGhidra | [GhidraMCP](https://github.com/LaurieWired/GhidraMCP) |
| ida-pro-mcp | IDA tools: decompile, disasm, xrefs, callgraph, rename, set type, patch, byte search; headless `idalib-mcp`; its README now recommends Hex-Rays' official IDA MCP server; needs paid IDA | [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp) |
| REA | One MCP over Ghidra, IDA, Hopper (plus objdump, Rizin); needs Node; `npx rea-agents setup` edits agent config, so the owner approves it | [REA](https://github.com/morluto/rea) |

---

## 4. From nothing to co-op: build order for an agent

Work in this order. Each stage ends in a test that two scripted clients pass without a
human. See [automated-qa.md](automated-qa.md).

### Stage 0: recon (hours)

1. Search for existing work first ([DIRECTORY.md](../DIRECTORY.md),
   [engine-families.md](engine-families.md)). Reviving or extending is far cheaper.
2. Record game identity (build, exe SHA-256, engine). Look for source: SDK, decomp,
   script archives, open engine rewrite.
3. Classify the engine: has replication (Unreal, Source)? scriptable (Lua, UnrealScript,
   Papyrus-like)? Unity? decomp? Choose the tools in section 3.

### Stage 1: pick the hook point

Order of preference: (1) game source or decomp; (2) engine with built-in multiplayer plus
scripts; (3) official or community mod loader (script extender, BepInEx/Harmony,
REFramework, UE4SS); (4) inline hooks in a DLL you inject.

Exit test: your code runs inside the game, logs a line each frame, and can read the
local player's position and world id. Difficulty: easy on Unity/mod-loader games
(hours), moderate on script-extender games (days), hard on unknown native engines
(weeks, mostly reverse engineering and finding stable addresses per build).

### Stage 2: make a second player visible (puppet)

1. Write a minimal transport: host plus one client, a message for "player state"
   (id, world id, position, rotation, animation, equipment), 10 to 20 Hz.
2. Spawn a puppet from the game's own NPC or player model. Disable its AI and input.
   Drive it from received state with interpolation. Without a spawn API, an existing NPC
   can be taken over: skip its decision call and feed the game's own move request so its
   walk animation plays, or spawn a cutscene copy of the player character and give it the
   player's own animation graph (Shadow of War; see
   [reverse-engineering.md](reverse-engineering.md#reading-and-driving-live-state-from-inside-the-game)).
   With the player's graph, send the graph's own inputs (stick, speed, turn) and let them
   move the puppet; pinning it to the received position every frame breaks its stride.
3. Handle disconnect (despawn) and cell/level mismatch (hide puppets in other
   levels).

Exit test: client A's script walks in a circle; host observes a puppet doing the same
within a position tolerance. Difficulty: this is the first hard milestone. Spawning a
controllable-looking actor safely and keeping the engine from "cleaning it up" often
takes the longest. Moderate to hard.

### Stage 3: authority model

Decide per entity class (section 1.2). Default: the host runs all AI and physics, clients
send inputs and "I hit X" claims, host validates and broadcasts results. Start with only
what is needed to fight together: NPC position, health, death. Add an authority hand-off
rule and a kill switch (like `EnemySyncMirroring`) early.

Exit test: both clients damage the same enemy; both see the same health and death; drops
appear once. Difficulty: hard. AI is the long tail (TES3MP still lists AI as its main
open problem; OOT True Co-op lists aggro, sounds and particles as gaps).

### Stage 4: world and quest state

1. Locate the story variable store (quest stages, global flags, event flags). Intercept
   writes at the lowest common function.
2. Replicate writes from the authority; apply on others with a re-entrancy guard
   (applying a received write must not re-broadcast).
3. Build snapshot-on-join: dump the store, send, apply, then stream deltas.
4. Decide dialog and cutscene policy (section 1.5).
5. Items: shared world pickups once, containers by authority, per-player inventory.

Exit test: host advances a quest, client reads the same stage; join a third client late
and verify identical store hash. Difficulty: easy if the engine has one flag store,
very hard if state is scattered across scripts and objects (Fallout, Skyrim). Moderate
typical.

### Stage 5: transitions and saves

1. Map each way the game changes level (doors, fast travel, scripted warps) and make
   them host-driven, with clients following and a barrier before resume.
2. Choose canonical save (host's). Joining clients never overwrite theirs, or run a
   per-session profile.
3. Re-sync the full snapshot after every load; assume all entity ids change.

Exit test: 15 consecutive scripted level transitions with checksums matching after each.
(Our own Deus Ex soak ran 15 travels with two local clients.) Difficulty: hard and
bug-dense, because it touches everything above.

### Stage 6: automated two-client tests

1. Run two real instances on one machine (see mutex notes in [methods.md](methods.md)),
   or run one real instance plus a headless bot speaking your protocol.
2. Add a state hash command and compare after every scripted step; this is also your
   desync detector, mirroring lockstep checksum practice.
3. Add loss, latency and jitter simulation from the transport.
4. Wire scenarios (walk, fight, quest step, transition, reconnect) into a CI-like
   script. A human playtest is not a test plan.

### Realistic scope

| Game type | Players visible | Combat | Quest sync | Full campaign |
|---|---|---|---|---|
| Decomp or open-source engine | days | days to weeks | weeks | weeks to months |
| Unity with mod loader | days | weeks | weeks | months |
| Unreal 1/2 with script | days | already there | already there | weeks (bug fixing) |
| Bethesda with extender | weeks | weeks to months | months | not achieved by public projects without a leader-centric workaround |
| Closed native engine, no loader | weeks (RE) | months | months | likely out of scope |

These are estimates for an agent working with a human reviewer, not measured data.

---

## 5. Unknowns and gaps in this page

- Tilted: exact hook method, per-entity ownership.
- Seamless Co-op: any technical architecture (closed source, no write-up found).
- HKMP, sm64coopdx, TES3MP: transport internals not confirmed from fetched pages.
- KeeperFX and DK2: sync model not stated in fetched pages.
- xrMPE: whether it is a full engine fork; engine repo not found.
- NV:MP: source pages returned 403; internals come only from local reverse engineering.
- Nitrox architecture details are from a third-party docs mirror; verify upstream.
- Source and Unreal engine networking docs (Valve, Epic) returned 403 or 404; read them
  directly before relying on engine-level replication details.

If you fill one of these in, add the source link and open a pull request.
