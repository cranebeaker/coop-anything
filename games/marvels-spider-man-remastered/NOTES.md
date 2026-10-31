# Notes: Marvel's Spider-Man Remastered

## 2026-10-19: triage

Observed:
- Steam app 1817070, build id 23986256 (from the app manifest).
- `Spider-Man.exe` SHA-256 e297d4d94f1ffe4febf289745e79e7b6fa233a788e7a00f480fc77c55db81ad1, no version resource.
- Install root holds the exe, middleware DLLs and `asset_archive\` (37 entries).
- Patreon page https://www.patreon.com/hbgda: newest post "SMT Progress Update and Recent Issues", about 4 days old, locked.
- Nexus mods/5696: HTTP 403 to WebFetch.

From sources (not checked first hand):
- $10 "Web-Head" tier closed beta; free roam, hideouts and challenges work; story is rough (ComicBook 2025-08-26).
- Up to 7 players (Kotaku 2025-08-27; Insider Gaming).

Unknown: transport, synced state, supported game builds, licence.

Correction: a web-search summary said the 80.lv article was "11 days" old. The article itself is dated 2025-08-28.

## 2026-10-20: research, workspace, first look at the exe

Correction to the 2026-10-19 write-up: it says the game "ships no multiplayer mode, so there
is no stock netcode". That was inferred from the file listing and is wrong at the string level.

Observed (ASCII strings of `Spider-Man.exe`, build 23986256, read-only):
- Networking: `Net::Session`, `Net::SessionLan::Update() | Disconnect due to server timeout`,
  `Net::ConnectionManagerLan | Received out-of-order UDP packet, sequence diff: %d`,
  "Got udp punch through ack with the wrong unqiue id!", "Send connect info udp punch through error",
  `GameNet::DisconnectFromSession`, `Net::RooomList` (sic), `NET_E*` socket error names.
- Switches: `LanEnable`, `LanMode`, `LanTimeoutEnable`, `UnlockMultiplayer`, `AutoMatchmake`, `UseLobby`.
- Co-op gameplay: RTTI `.?AVUITitleScreenOptionHostCoopGame@@`, `.?AVUITitleScreenOptionJoinCoopGame@@`,
  `.?AVHeroCoopSpawnPoint@@`, `.?AVCoopWarpAction@@`, `.?AVCoopWarpLocation@@`; `kRemotePlayerGroup`
  ("All actors controlled by remote players."), `MaxHPCoop`, `InjuredHPCoop`, `RegenDelayCoop`,
  `kCoopOnly`, `kSingleAndCoop`, `AllowInCoop`, "If set and in co-op, Character2 will see this text",
  "Picking random hero for late-joining client %d", `DamagePacket` ("Send damage packets at this interval").
- Leftover PSN party code: "HandlePartyJoinSession: multiplayer not unlocked".
- RTTI type names are present, so classes can be named in Ghidra.

From sources (not checked first hand): one news summary says hbgda's mod started from leftover
multiplayer code in the PC files. DSOGaming (2025-08-28) does not say this; source unconfirmed.

Derived: the engine still has a LAN session layer and co-op data fields. Whether any of it can
run in this build (or is stubbed, like the dev requests in lessons.md item 32) is unknown.

Unknown: whether the LAN code is reachable, its ports and protocol, what it syncs.

Research, existing tools (2026-10-20): Overstrike (open-source mod manager for Insomniac ports),
SMPC Tool and its open-source version (asset archive and `toc` editing), SpiderManTextureTool.
No script hook or runtime SDK for this game was found.
- https://github.com/leonyarov/SpiderManTextureTool
- https://www.nexusmods.com/marvelsspidermanremastered/mods/4199
- https://openmods.net/games/marvels-spider-man-remastered
- https://www.dsogaming.com/mods/a-multiplayer-mod-for-marvels-spider-man-is-available-in-early-access/

Workspace: a test copy of the install (61 files, 65.96 GB, exe byte-identical to the store
copy). Tools: Ghidra 12.1.4 (release SHA-256 checked), Temurin JDK 21, x64dbg, VS 2022 Build
Tools. Ghidra headless analysis started.

## 2026-10-20 (later): is the LAN code compiled in?

Method: two small dependency-free Python scripts over the exe on disk (no Ghidra yet).
`xref_scan.py` finds RIP-relative `lea`/`mov` refs and absolute pointers to strings and maps
each hit to its function through `.pdata`. `rtti_vtables.py` walks MSVC RTTI
(TypeDescriptor, CompleteObjectLocator, vtable).

Observed:
- Imports: WS2_32 (`WSASocketW`, `WSAPoll`, `getaddrinfo`, `inet_pton`, `inet_ntop` and ordinals
  covering socket/bind/sendto/recvfrom/setsockopt/select), IPHLPAPI `GetAdaptersAddresses`,
  `GetIpForwardTable`. Steam imports are only init and callbacks (no SteamNetworking).
- vtables with real slots: `Net::SessionLan` (39, 0x143d068f0), `Net::ClientLan` (2),
  `Net::LocalClient`, `Net::Party` (22), `GameNetManager` (20), `GameNetManagerBase` (20),
  `GameManagerMultiplayer` (42), `UITitleScreenOptionHostCoopGame` / `JoinCoopGame` (13 each),
  `HeroCoopSpawnPoint`, `CoopWarpAction`. `Net::Session` (36 slots) is all `_purecall`: an
  abstract base whose only concrete subclass seen is `SessionLan`.
- 192 `*Remote` state classes (`HeroStateJumpLandRemote`, `BotStateFlungRemote`, ...) have vtables.
- `ConnectionManagerLan` code sits at 0x1417e6000-0x1417e9d00 and references its own log
  strings: TCP connect, UDP punch-through, unique-id handshake, encrypt/decrypt, reliable
  and unreliable send, keep-alive and connect timeouts. `RoomListLan` (0x1417e9d00-0x1417eac00)
  creates a UDP socket, `SO_REUSEADDR`, binds, and joins a multicast group (`IP_ADD_MEMBERSHIP`).
  This is LAN game discovery.
- `VoiceChatSystem` with `PlayerId0`..`PlayerId7` (eight player slots).
- `NetSettings` config type (registered next to `NavSettings`), fields: `LanTimeoutEnable`,
  `VersionCheckEnable`, `PingPort`, `PingPortExternal`, `RoomListRequestPort`,
  `RoomListResponsePort`, `DefaultBrowsePool`, `RestrictBrowseGamesToFriends`,
  `RestoreRoomAvailableWhenLoseClients`, `IntrospectionEnable`, `TitleId`, `VersionId`,
  `LocalAddress`, `Enable`, `ProfilingEnable`, `LanEnable`, `P2PEnable`, `VoiceChatEnable`.
  Its name table is at 0x144f0b150. No loose `.config` path names it, so it is probably in
  a packed system config.
- `Net::SessionLan` methods are small (largest 561 bytes, `Update`); the socket work is in
  `ConnectionManagerLan`.

Derived: a complete LAN transport (TCP plus UDP, encrypted, with discovery) and the remote-player
state machines are compiled into the PC build. It is not just leftover strings. The plan
changes from "write netcode" to "turn on `NetSettings.LanEnable` and reach the hidden title-screen
Host/Join options".

Unknown: NetSettings field offsets and its default values, who reads `LanEnable`, the ports,
whether gameplay sync (hero, bots, damage) still works or only the transport.

Next: decompile the list in `decomp_targets.txt` once Ghidra finishes analysis.

## 2026-10-20 (evening): Ghidra pass, how host/join is wired

Ghidra 12.1.4 headless, 12 GB heap, Decompiler Parameter ID and Aggressive Instruction
Finder off: analysis took 12293 s. Reference queries use `FindRefs.java` on the saved project
(read-only). Addresses are for build 23986256.

Observed (decompiled):
- Title menu handler `0x1407ac600`: Host Co-op option (type hash 0x36e53d84) sets byte
  `0x1473cf1a1`, Join Co-op (0xd3bc7ad5) sets `0x1473cf1a2`, then closes the title. These
  bytes sit inside the `DebugSettings` global and nothing else reads them; one reset at `0x140c57ac0`.
- LAN gate: byte `0x147a88daa` (NetSettings, almost certainly `LanEnable`). Read by
  `0x1417e5570` (creates `Net::RooomList` at init) and the SessionLan factory `0x1417e5ab0`.
  No code writes it, so it is filled from config data. `0x147a88da0` (a NetSettings dword) is
  compared against incoming room-list packets (probably `VersionId`/`TitleId`).
- `GameNetManager` singleton pointer `0x147c2b1c0`. Session start `0x141f012c0` calls the factory.
  - host: `0x141f007b0` -> `0x141f00db0` -> SessionLan slot +0xb0 (`0x1417eda70`, allocates
    client slots, marks local players). **No callers anywhere.**
  - join: `0x141eff2a0` -> `0x141f00c00` -> SessionLan slot +0xb8 (`0x1417eb490`, copies the
    host's connect info, calls ConnectionManagerLan connect). Callers: a LAN lobby list UI
    (`0x1407a5200`, `0x1407a5bd0`, entries 0x108 bytes at +0x158), and an auto-join state
    machine `0x140c5f510` that browses rooms and joins the one whose name matches the
    DebugSettings string at `0x1473cf1a8`.
- Connect state machine in `GameNetManagerBase` (`0x141eff7a0`, `0x141effa70`): creates local
  clients, then game-state sync objects; state 10 = `kConnectError`.
- Debug overlay `0x140c5f960` prints "Local Party", `GameServerHost: 226.0.0.1`, `SessionType: %s`
  (byte `0x1473cf005`, also put in room-browse requests).

Derived: the join side is still wired to a LAN lobby UI and a debug auto-join. The host side is
complete code that nothing calls. Plan for the proxy DLL:
1. force the LAN gate (patch the two reads of `0x147a88daa`, or set it before net init),
2. call the host API `0x141f007b0` with a room/session descriptor (layout still unknown; the
   join entries are 0x108 bytes with the session type at +0xbc),
3. on the second machine, reach the lobby UI or the auto-join with a matching name.

Unknown: the descriptor layout for hosting, what calls the lobby UI, whether gameplay sync
(remote hero, bots) works after the connection is up.

## 2026-10-20 (night): first in-game test, two instances on one PC

Setup: proxy `winmm.dll` (source now in `mod/src/coop.cpp`) in the test
copy, with `winmm_sys.dll` (a copy of the system DLL that the exports forward to) and
`steam_appid.txt` (1817070, so the exe does not relaunch through Steam). Saves backed up before
the run; afterwards a hash comparison showed no save file changed.

Patches applied by the DLL (all check the original bytes first):
| VA | original | purpose |
|---|---|---|
| 0x1417e557b | `74 26` | create `Net::RooomList` even though `LanEnable` is 0 |
| 0x1417e5b4a | `0f 84 1d 01 00 00` | SessionLan factory: skip the `LanEnable` check |
| 0x141d2ec4a | `0f 84 0b 04 00 00` | `<title>_GameInitMutex` exists -> quit; skipped so a 2nd instance runs |
| 0x142aa30e0 | `48 89 5c 24 08` | PSN account-linking HTTP worker returns at once (it crashed the 2nd instance, null queue entry) |
| 0x141bbb4e0 | stub `... c3` | retail engine printf is a stub; jump to our logger instead |
Runtime write: game-mode table at 0x1463c8c40 (0x18-byte entries, max players at +0x10):
ids -1 and 0 have max 1, id 1 has max 2. `setmax 2` raises all to 2.

Observed:
- Host (`0x141f007b0` with a 0x108-byte descriptor from `0x1417e53a0`, +0xbc = 1): RoomList UDP
  socket bound to 59003, joined multicast 226.0.0.1, TCP listen socket opened, netmgr state 0 -> 9.
- Browse (`0x1417e54f0`) from the same and from a second process both found room `SpideyCoop`.
- Join (`0x141eff2a0`) first run: "Server connect success!", host "Client index 1 connected!",
  then client `kConnectError GameNetManagerBase::HandleInitialSlotRequest` and disconnect.
  Cause: the host's slot handler (`0x141efdd10`) accepts only if requested + used <= max players
  for the current game mode, and the story-mode entries allow 1.
- Join after `setmax 2` on the host: client goes through client sync, game-state sync, load-state
  sync and the slot request to state 9 (connected); host logs "Client index 1 connected!" and
  stays connected. Neither process crashed.
- The test copy shows the PC launcher (Play/Settings/Quit) on start; Play is at client (50,335)
  of a 790x445 window and accepts a posted click.
- The Sync Host server (`EskimoSync::SyncHost`) is compiled into the exe and runs in-process.

Not verified: whether either game shows the other player. Both windows rendered black to
capture (probably still on the intro/title, which needs focus and input), and a Windows
Security prompt owned by another app held the foreground, so the windows could not be focused.

Next: with the user present, go past the title in both instances (load a save or free roam),
then check for a remote hero (`kRemotePlayerGroup`, `*Remote` hero states).

## 2026-10-21: into the city, client spawn crashes

Tools: `mod/tools/coop_test.ps1` runs the whole sequence (host instance, then a client
that late-joins). Saves backed up to `E:\coop\backup\20261021-0925` first and restored after
(host Continue rewrote `slot0-s.save` and `-userprefs.save`; restored copies hash-verified).

Order that matters:
- The game renders nothing while unfocused during the intro, and an instance whose Boot game mode
  starts while unfocused gets stuck: render thread spins on a GPU query that never resolves
  (`0x14188f1d0`, vtable +0xe8 returns 1 forever), main thread waits on it (`0x141896a70`).
- Starting single player (title -> Continue) creates its own local session: netmgr goes to 9 and
  `Host` (`0x141f007b0`) refuses (it requires state 0). So host at the intro, before Continue.
- Working sequence: host instance `setmax 2` + `host` at the intro, then intro -> Continue into
  the city (its session survives). Client instance: focus it so it boots by itself (it makes a
  local session, state 9), `leave`, `browse`, `join 0`.
- Joining before the host loads: the client follows the host's mode change into Boot while
  unfocused and hangs as above. Late join into a running host works better.

Late join: client goes 1 -> 2 -> 3 -> 4 -> 5 -> 7 -> 8 -> 9 (all connect states), GameMode Lobby,
loads the host's checkpoint (`CHK_FISK_BASE_INTRO_02_L13`), host logs "Picking random hero for
late-joining client 1", client logs "Spawning hero for client 1, (character_id: 0)". Then the
client crashes while activating/updating its own hero. The host keeps running and logs the
disconnect. Each failure is a component that resolves a handle on the hero actor and reads
through null; the hero spawned on the client is missing parts single player always has:

| site | class | what |
|---|---|---|
| 0x1408c7852 (activate 0x1408c7720) | `WAM::HeroWAMTargetPicker` | owner handle +0x58 -> null, reads +0x180/+0x184 |
| 0x14065b309 (activate 0x14065b290) | `EmergentVOManagerHero` | component lookups on own actor -> null, reads +0x14 |
| 0x14084468f (update 0x1408445b0) | `Hero::Attitude` | handle +0x60 -> null, reads +0x174 |
| 0x1409571f3 (from 0x14065c8ad) | EmergentVO update (job thread) | probably follows from skipping its activate |
| 0x1408ffba9 (from 0x1408bd771) | ? | not looked at yet |

Mod changes (`mod/src/coop.cpp`): null-checked tail for the WAM picker;
`GuardCallSite<I>` wraps a rel32 call in SEH (logs RTTI class, returns 0) used on batch activate
(`0x14191989a` -> `0x141676010`) and the Attitude update (`0x140844d86`); a vectored handler logs
first-chance access violations with a stack (the last crash left no dump). Guarding is a probe,
not a fix: skipped components fail later.

Next: find out why the client's hero lacks those components. Look at what "Spawning hero for
client %d" does differently for a non-zero client index / non-local machine (which actor/config
it spawns, `character_id`, remote vs local hero setup), and at "Picking random hero" on the host.

## 2026-10-21 evening: why the client's heroes break (Ghidra + probe)

Spawn path (all addresses build 23986256):
- `HeroSystem` (RTTI vtable `0x1438a9858`, instance `DAT_147c307d8`, static at `0x145d9dd00`).
  Spawn-all at level start `0x14083dad0` (vtable +0x78) -> `0x141f22710` loops over this
  machine's *local* clients only; single respawn `0x141f253b0` -> `0x141f22510`. Both log
  "Spawning hero for client %d" and queue a 0x60-byte request (client index at +0x4c) through
  `0x141f65320` with callback `0x141f23710`, which calls create (vtable +0xc0, `0x14083f430`).
- Roster: vtable +0xa8 = hero count (`0x14083e060`, from config `0x1408407b0` +0x30), entries
  have a name (+8) and actor (+0x80). Create always spawns roster[0]'s actor and names it after
  roster[character_id]. "Picking random hero" (`0x141efd560`) avoids heroes other clients use.
- Local or remote: create asks `0x141efc100(ClientManager 0x1463c8f40, client handle)`. It
  compares the handle with the 4 local-player slots (`[mgr+0x118]+0x104`, stride 0x10c); the
  client table is `[mgr+0x110]+0x18 + index*4`. Local -> `HeroLocal` component (+ local player
  index, `DAT_145d9de58[]`); remote -> `HeroRemote`.
- Probe result (call at `0x14083f481` redirected): on the client the tables are right (table[1]
  = own client = local slot 0; table[0] = host). The client creates the host's hero as remote
  as soon as it joins, and crashes ~9 s later activating components of that *remote* hero
  (EmergentVOManagerHero `0x14065b309`). The morning crashes were the same thing.
- The missing parts are local-only components: the WAM picker needs `HeroEnvironmentManagerLocal`
  (type `0x146dd6750`), EmergentVO needs `HeroAimManagerLocal` (`0x146dd5a40`). Remote heroes
  never get them.
- Size of the gap: `HeroRemote` is referenced by 3 functions (type init, create, one more);
  `HeroLocal` by 119, `HeroAimManagerLocal` by 89, `HeroEnvironmentManagerLocal` by 39. Of ~231
  `Hero*Local` classes, 151 have a `*Remote` twin; the ~80 without are Spider-Man traversal
  (wall crawl, zips, web-yanks, perches, wall run...) plus HeroEnvironmentManager. So the engine
  has a remote-hero framework, but Spider-Man's hero only partly supports it. Not a one-line fix.

Host crash found on the way: since a reboot (exe base changed) a freshly started host crashed
every time on the first frame after its hero spawned: WAM target query `0x1408c9f80` resolves
picker +0x5c before the picker's activate has set it and passes null to `0x141f24ad0`. Fixed in
the mod by redirecting the call at `0x1408ca153` to a null-safe wrapper. The two SEH guards from
the morning are now off (not the cause, but no longer needed as probes). Earlier hosts survived
only because they were started before the reboot.

Mod changes (uncommitted): `RedirectCall` helper; `hero-local-probe` (logs when create decides
a hero is remote, plus both client tables); `wam-query-null`; `clients` command. coop_test.ps1:
dumps `clients` before leave and after join; Send-Cmd gives up after 30 s (it hung on a dead
client). Saves backed up `E:\coop\backup\20261021-1651`, restored, hash-verified; crash dumps
moved to `E:\coop\re\crash\20261021-evening`.

Options next:
1. Puppet partner (recommended): don't create the engine's remote hero; spawn a visual-only
   Spider-Man actor and drive its transform/animation from our own small sync over the existing
   LAN session (or our own socket). Both players keep a normal local hero. Most work is ours, but
   it avoids the unfinished remote-hero code.
2. Keep making remote-hero components null-safe. Bounded by the ref counts above, but even when
   it stops crashing, ~80 traversal states have no remote version, so the partner would not
   swing/crawl correctly.

## 2026-10-21 night: puppet partner, step 1 (no engine remote hero)

User chose the puppet-partner route. Step 1 is to never let the engine spawn a HeroRemote.
- Both spawn paths queue a 0x60-byte request via `0x141f65320`: all clients at call `0x141f229ef`, respawn at `0x141f226b9`. The client index is at request +0x4c.
- The mod redirects both calls to `SpawnRequestFilter`. It resolves the client handle (`0x141efbe40`), checks it is local (`0x141efc100`), and drops the request if not, logging "spawn: client N is remote".
- Actor spawn inside HeroSystem create (`0x14083f430`), to reuse for the puppet:
  - asset ref from roster +0x80 → `FUN_14159ec90(asset, params)`
  - `FUN_14190cfa0(&DAT_147a42f80, &handle, params)` → `FUN_1415a0560(handle)` = actor
  - `FUN_14159db20(actor)` finishes it
  - `FUN_14159cb80(actor, type, init)` adds the HeroLocal or HeroRemote component
  - Decompiles queued in `re/refs_targets16.txt`.
- First test run: the patches applied, the host reached the city and the client booted. Then Claude Code stopped the test script for low system memory: 1.7 GB free of 32 GB with both games at about 15 GB. The run ended before `join`, so the filter has not been exercised yet. Saves were restored and verified (backup `E:\coop\backup\20261021-1854`).

### Step 1 working (test 4, 2026-10-21 ~20:21)
- Test 2: the filter alone was not enough. The client still created the host's hero through the spawn-request callback `0x141f23710`, which the network code runs when the host's spawn arrives, and crashed at `0x14065b309`.
- Test 3: gating HeroSystem create (vtable `0x1438a9858` +0xc0) to return null for remote clients moved the crash into the callback (`0x141f237cf`).
  - The callback compares the created hero with the local player's hero from the slot table. On a freshly joined client that one doesn't exist yet, so both are null, they match, and it stores through null to +0x168.
- Fix: the callback is replaced by `SpawnCallback` (entry patched with an absolute jmp), the same logic plus a null check.
- Test 4: both instances in the city, each with its own Spider-Man, client joined ("Client index 1 connected!").
  - Both sides log "hero-create: client N is remote, skipped".
  - 100 s after join: no exceptions, no disconnect, both responsive.
  - Screenshots confirm both are in free roam.
  - Claude Code stopped the test script for low memory again (3.5 GB free), but the games kept running.
- The client wrote a new `slot5-s.save`. It was moved to `E:\coop\backup\test-modified-20261021\run4`, and the saves were restored and verified identical.
- Actor spawn API, for the puppet (decompiled in `re/refs16`):
  - `FUN_14159ec90(asset, params)` fills a 0x28-byte block with the asset id.
  - `FUN_14190cfa0(&ActorManager 0x147a42f80, &handle, params)` allocates the actor.
  - `FUN_1415a0560(&handle)` gives the actor pointer.
  - `FUN_14159db20(actor)` activates it.
- Next: choose a puppet asset (Spider-Man model without hero components) and sync transforms.

## 2026-10-22 morning: puppet partner, step 2a (a visible second Spider-Man)

Tested with one game instance only (`mod/tools/puppet_test.ps1`, commands `puppet`, `pdress`, `pinfo`, `poff x y z`).

What it took:
1. **Game-thread tick.**
   - The call at `0x140844d86` → `0x1408445b0` (per-frame hero update) is redirected; `PuppetTick` runs after it.
   - Commands set flags, and the tick acts on them.
2. **Spawn.** Same calls as HeroSystem create:
   - roster (`0x1408407b0`) entry 0, +0x80 → `0x141bb25d0` → `0x14159f3c0(0x1460421e0, ...)` = asset
   - `0x14159ec90` params
   - `0x14190cfa0(ActorManager 0x147a42f80, &handle, params)`
   - then `0x14159e4a0` / `0x14159e3f0` (name) / `0x14159e590` (level), using the local hero's spawn context (create's third argument, captured in `HeroCreateGate`).
3. **Activate with the batch pass** `0x1419194e0(ActorManager, {handles, count}[], groups)`.
   - `0x14159db20` alone only begins activation, and the actor stays at flags 0x20a.
   - The batch pass sets 0x2001, runs the pre-activate job, adds the node to the scene and activates the components.
   - The hero gets this pass from level load; a late-spawned actor needs it explicitly.
4. **Transform.** Actor +0 → node, a 4x4 row-major matrix with the position at +0x30. Set with `0x14191c0e0(node, m, node + 0x70)`.
5. **Dress it.** The body is not part of the actor asset. Spider-Man is built from SkinManager parts:
   - parts bitset at +0x58, 65 part entries of 0x6b8 bytes at +0x50
   - the hero has parts 0 and 30 (`0x40000001`); a fresh actor has none, so it draws nothing (the node bbox is about 0.2 tall).
   - A hardware write-watchpoint on the hero's bitset (DR0 via `ArmWatchThread`, logged by `WatchHandler`) found the setter: **AddPart `0x1420c8d50(skin, index, desc)`**, called from the part-load path `0x1420c6890`.
   - desc layout:
     - +0 model asset
     - +8 owner node id (node +0x64)
     - +0xc and +0x14 (u64 each)
     - +0x20 → SkinItemPrius
     - +0x30 flag byte
   - `DressPuppet` rebuilds the desc from each of the hero's part entries (+0x3c, +0x44, +0x4c, +0x70, +0x6b2) and calls AddPart, then SkinManager vtable +0x58 with `0x140bcc0e0(0x146050d30, asset)`.
   - **Result: a full Spider-Man puppet renders next to the hero and follows it** (`re/shots/20261022-first-puppet.png`). It is in T-pose because nothing animates it yet.

Dead ends:
- HeroCharacterManager "look" `0x140917100` / `0x1409189e0` are ability and gadget flags.
- Hero actor +0x20 children = a swing-setup volume.
- AttachmentList = gadgets (WebShooter, ShockerBlast, ...).
- Copying the bbox / node flags changed nothing.

Next:
- Pose: copy the hero's bone pose onto the puppet (local test), then send it.
- Own UDP transform sync between instances (puppet = the other player's hero).
- Hide the puppet when the partner is not connected.

### Step 2b: partner sync over our own UDP (2026-10-22 ~09:00)
- Commands: `sync host` (binds UDP 59010) and `sync <host-ip>` (client).
  - Each side sends its hero's 4x4 matrix at up to 30 Hz (packet "SPCP" + seq + 16 floats).
  - The host replies to the last sender.
  - On the first packet a game spawns and dresses the puppet; dressing runs 30 ticks after spawn.
  - Each tick it sets the puppet's transform to the newest partner matrix. A matrix older than 2 s counts as stale.
- Two-instance test on one PC: both connected through LAN co-op, and both spawned puppets.
  - pinfo: the host's puppet was at the client hero's position (3.2 49.5 -1614.0) and the client's puppet at the host hero's position (6.2 49.1 -1653.5).
  - The heroes were about 40 m apart, so neither puppet was in view in screenshots. A visual check needs both heroes in one place.
- Claude Code stopped the test script for low memory again, after `join`; `sync 127.0.0.1` was then sent by hand with `tools/cmd.sh`.
- The client's hero wears a different part 30 asset than the host's (`9b54a708...` vs `bab656a0...`), probably a different suit state in the save slot it loaded.

Remaining for a playable partner:
- Animation pose (the puppet is in T-pose).
- Send the suit parts over sync instead of copying the local hero's suit.
- Hide or despawn the puppet when the partner leaves.
- LAN test on two PCs.
- A spawn-near-partner option so both players start together.

### Step 2c: animation pose (2026-10-22 ~10:10)
- The pose is found by diffing memory reachable from the hero across frames (`adiff <root>`; root 1 = scene node).
- **Scene node +0xd8 → per-instance joint matrices:**
  - 64-byte 4x4 matrices, one per skeleton joint.
  - Joint count: node +0x88 (model resource) → +0x200 (skeleton) → u16 at +2, which is 237 for Spider-Man.
- Copying the hero's 237×64 bytes into the puppet's buffer every tick, after the hero update, makes the puppet take the hero's pose. The T-pose is gone.
- Copying 32-byte entries (half the data) did nothing visible.
- Sync now carries the pose:
  - header + matrix + `joints` × 64 bytes ≈ 15.3 KB per packet at up to 30 Hz (~450 KB/s); socket buffers set to 1 MB.
  - The receiver writes the pose into the puppet's buffer each tick.
- `sync loop` (single instance, sends to itself, the puppet keeps its offset) proves the full serialize → UDP → apply path. Walking the hero with the drive tool shows the puppet following and posed mid-motion (`re/shots/20261022-puppet-loopback-moving.png`).
- Not yet checked:
  - whether the copy happens before or after the puppet's own anim/skinning each frame (possible one-frame lag or flicker);
  - bandwidth over a real LAN.
  - Possible compression later: 3×4 matrices or quaternions.
- Research tools still in the DLL: `adiff`, `ndiff`, `pinfo`, `ppoke`, `pbbox`, and the watchpoint (off). Remove before release.

### Step 3: warp to partner, first real co-op view (2026-10-22 ~10:22)
- The game has a `CoopWarpAction` (`0x1409ac210`) that builds a **PerformWarpEvent**:
  - ctor `0x141b037f0`, init `0x141b0aa00`
  - flags +8/+0x18 |= 0x6c000
  - +0x22c = 1, +0x22d = 1
  - target 4x4 set with `0x141b57e40(ev + 0x138, m, 1)`
  - posted with `0x141676a50(component, ev)`
- HeroWarpProcessor registers handler `0x140867520` for it. The handler stores a pending warp: transform at +0x48, flags at +0x98, pending flag **+0xa8 = 1**. The hero state then performs it.
- Posting via `0x141676a50` never reached the processor (+0xa8 stayed 0). Calling the handler directly with the hero's HeroWarpProcessor works.
  - The sender is stamped at +0x2c as `0x141676a50` does.
  - The pending flag is consumed within a frame, and the hero is at the new spot.
- Commands: `warp dx dy dz` (relative test) and `warpp` (to the partner's last synced position + 1.5 m x).
- Two-instance test:
  - host `sync host`, client `sync 127.0.0.1`, then `warpp` on the client.
  - **Both games show both Spider-Men side by side on the rooftop** (`re/shots/20261022-coop-host-view.png`, `...-client-view.png`).
  - The client's save wears a different suit, and its puppet wears that suit too: dressing copies the local hero (to fix).
  - Claude Code stopped the test script for low memory after all steps had run; both games survived.

### Step 4: partner suit (shelved), leave handling, one-key start, cleanup (2026-10-22 ~11:00)

**Partner suit (shelved):**
- Packets now carry the sender's enabled skin parts: index, asset, desc fields, and the Prius look hash (+0x98) and +0xa8.
- The receiver's AddPart with those values, using a SkinItemPrius cloned from our own entry and patched, gives an invisible puppet. The asset info is loaded on both sides.
- Part dumps show the Prius also holds per-instance values (u32s at +0xdc, +0x11c, ...).
- The right fix is the game's own part-load request path, the callers of `0x1420c6890`: `0x1420c6aa2` ← `0x1420d0d07` ← `0x1420cfe79`.
- Kept as research (`psuit`). The puppet wears the local suit. (Solved in step 5.)

**Leave:**
- After 2 s without packets the puppet is parked at y = -5000 and shown again when packets resume.
- Tested by killing the client: the host logged "partner silent, puppet hidden" before the game's own "Client index 1 disconnected!".
- A real actor destroy is still TODO. The disconnect handlers `0x141f010e0` / `0x141f01550` only forward to the network layer.

**One-key start:**
- `coophost` = F7 at the title screen: setmax 2 + host + sync host.
- `coopjoin` = F9 in the city, run on a thread: setmax 2, leave, browse, join 0, sync with no address, then auto-warp 90 ticks after the first partner packet.
- With no address the client broadcasts (SO_BROADCAST, 255.255.255.255:59010) and learns the host from its reply. On one PC it found the host at the PC's LAN address, not loopback.
- F8 = warpp, F6 = status. Hotkeys only act in the focused game window.
- `tools/player_test.ps1` drives exactly this. Passed on the release build: both games show both players on the rooftop.

**Cleanup:**
- Research code lives in `mod/src/research.inc` and is built only with `build.bat research` (/DCOOP_RESEARCH).
- Release `winmm.dll` is 210 KB.
- `mod/README.md` has install and play steps.

### Step 5: partner suit through the game's item loader (2026-10-22 ~17:30)

Why step 4's suit was invisible: AddPart alone skips streaming. The game loads a skin item in two stages:
- `0x1420bf2a0(SkinManager*, SkinItemPrius*)` is the entry. It copies the Prius into a pooled load request (request +0 = skin handle, +8 = Prius copy, +0x520 = asset) and queues the asset load with callback `0x1420c5db0`.
- `0x1420c5db0` resolves the part's sub-assets (`0x1420ce7b0` with skin +0xb4 / +0x180) and queues them with callback `0x1420c6890`.
- `0x1420c6890` computes the slot from the asset info (`0x1420c1920`, info +0x74), clears it (`0x1420ca930`), then AddPart (`0x1420c8d50`) + vtable +0x58.
- `0x1420c1ed0` + `0x1420cd900` skip an item identical to the one already worn (asset + property arrays compared).

Callers that build a fresh Prius: the suit menu paper doll `0x140822390` (suit id -> suit def `0x140c92b70(0x146050d30, id)+0x30`, one item per entry) and the remote-player path `0x141f5e8c0`. Both build a default SkinItemPrius:
- vtable (read at runtime from our hero's part entry +0x70), element ctors `0x1407a6090` (+0x40), `0x1407a60f0` x4 (+0x70, stride 0x40), `0x1407a60b0` x6 (+0x170, 0x48), `0x1407a6060` x4 (+0x320, 0x70), `0x1407a60d0` (+0x4e0); size 0x518;
- init `0x142273f20`, flags +8 / +0x18 |= 2, asset at +0x30, then `0x1420bf2a0`, then release `0x142274040`.

The mod does the same (`LoadSkinItem`) for each partner part whose asset differs (`ApplyPartnerSuit`). The part asset (entry +0x3c) equals the item asset (Prius +0x30). Vtable +0x88 on SkinManager is not a plain RemovePart: calling it changed nothing, so parts the partner doesn't wear are still left on.

Tests:
- One instance, `sync loop` (the puppet mirrors our own parts): equipped Spider Armor MK II in the game's suit menu. The log showed `suit: loading partner part 30 asset 8e7aed6ee3a9ffde` and the puppet switched to the armor.
- Two instances, release build, `player_test.ps1`: the client's hero had a different part 30 (`9b54a708...`, classic suit) from the host (`bab656a0...`, Advanced Suit). Each side loaded the other's part 30 and the screenshots show the partner in the partner's suit.
- Puppet part entries loaded this way have zeroed property arrays (the hero's carry per-item values), so suit options set through those properties may not match.

Saves restored and hash-verified after each run.
