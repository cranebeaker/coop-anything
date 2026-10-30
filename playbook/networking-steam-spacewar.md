# Steam "Spacewar" (AppID 480) networking: no IPs, no port forwarding

How to give any co-op mod Steam lobbies, friend joins and relayed P2P traffic, so
players never type an IP address or touch their router. Two working examples:

- **Built into the game:** the KeeperFX (Dungeon Keeper) co-op build has a Steam
  network layer, `src/net_steam.cpp`, next to its other network providers.
- **A relay next to the game, no engine changes:** `HXCoop.exe` for Deus Ex co-op
  (see [games/deus-ex](../games/deus-ex/README.md)). It creates the lobby, starts
  the game, and moves the game's own UDP packets over Steam. Use this pattern when
  the engine's netcode is closed or hard to change.

## The idea

Every Steam user can run AppID **480 "Spacewar"**, Valve's free sample app. If a
game loads `steam_api.dll` with a `steam_appid.txt` containing `480` next to the
exe, Steam treats it as Spacewar and gives it the full Steamworks API:

- **Lobbies** (ISteamMatchmaking). The host creates a friends-only lobby and the
  friend joins by invite, by right-clicking the host → Join Game, or by lobby id.
- **P2P messaging** (ISteamNetworkingMessages). Traffic goes by SteamID through
  Valve's relay network, so there are no IPs, no port forwarding and no NAT
  problems.
- **Overlay invite dialog and rich presence** (ISteamFriends).

The catch: both players show as "playing Spacewar", lobbies are visible to every
480 user worldwide unless you filter with lobby data, and you can't ship on the
Steam store this way. For a mod shared between friends, that's fine.

Both players need Steam running and logged in. Nobody needs to own anything.

## Getting a 32-bit steam_api.dll

Use the Steamworks.NET "Standalone" release (github.com/rlabrecque/Steamworks.NET/releases).
Both examples use `Windows-x86/steam_api.dll` from release 2025.164.1:

- SHA-256 `fdafb7c65ec7d4182b42bf92cc8671c68e4dc547cc6e613899d2618a47eb65da`
- exports `SteamAPI_InitFlat` and the `..._v002` networking-messages interface.

Copies bundled with old games (e.g. Fallout New Vegas) are too old to export the
flat API, so don't use them.

`CreateLobby(self, eLobbyType, maxMembers)`: lobby type 1 = friends-only.
`SteamNetworkingMessages` must be the `..._SteamAPI_v002` getter, not the
game-server one.

**Join Game when the game isn't running** probably launches Spacewar itself
(AppID 480), not your mod. Tell the friend to start the mod first, then accept
the invite or click Join Game.

**Check the game's own copy first.** A recent Steam game may already ship a
`steam_api64.dll` with the flat API: The Witcher 3 Remastered's (`bin\x64_dx12`, version
10.87.46.89) exports every function and interface version listed below. The relay can load
it from the player's own install, so there is nothing to download or redistribute.

For a 64-bit game, use `Windows-x64/steam_api64.dll` from the same release:

- SHA-256 `eb17909a76668cf9ae0b92a618a34a50f6c73d3a6787cb4dd8ce36a8b10bfb75`

Check the architecture before using either: a 32-bit exe needs the PE32 i386 DLL, a
64-bit exe needs the x64 one. Several struct offsets differ between them (below).

### The game's copy may not have it; and what to do with no compiler

The check has to actually run, because the answer is often no: the `steam_api64.dll`
shipped inside Poppy Playtime (Steamworks 1.51, in the install's `Engine\...\Steamv151\Win64`
and `PlaytimeLauncher` folders) has **no `SteamAPI_InitFlat`**, so a plan that assumed the
game's own copy would fail late. A few lines of Python settle it — `ctypes.WinDLL(path)` then
`getattr(dll, name)` for each symbol — with no toolchain needed.

That same trick is the fallback when the machine has no C/C++ compiler at all. The flat C
API exists so a language without Steamworks headers can call it: `ctypes` can drive init, the
`SteamAPI_ISteam*` getters, `ManualDispatch_*`, lobbies and `NetworkingMessages` directly,
using the struct offsets on this page. One program with a `--transport udp|steam` switch then
gives both a scriptable test stand-in (UDP over localhost, no second account, two clients on
one PC) and the real Steam path from the same code. Evidence: Poppy Playtime,
[games/poppy-playtime/NOTES.md](../games/poppy-playtime/NOTES.md) (Phase 6).

## Design: no SDK headers, dynamic loading only

`LoadLibrary("steam_api.dll")`, then `GetProcAddress` for each **flat C API**
function. The flat API is plain `__cdecl` C functions that take the interface
pointer as their first argument. No SDK headers or import libs are needed, so the
GPL repo stays clean and it works with mingw. If the DLL is missing, show a clear
error and fall back to the normal netcode.

### Functions used (exact export names)

```
SteamAPI_IsSteamRunning           SteamAPI_InitFlat           SteamAPI_GetHSteamPipe
SteamAPI_ManualDispatch_Init      SteamAPI_ManualDispatch_RunFrame
SteamAPI_ManualDispatch_GetNextCallback  SteamAPI_ManualDispatch_FreeLastCallback
SteamAPI_ManualDispatch_GetAPICallResult

Interface getters (each returns the interface pointer):
SteamAPI_SteamUser_v023  SteamAPI_SteamMatchmaking_v009  SteamAPI_SteamFriends_v018
SteamAPI_SteamNetworkingMessages_SteamAPI_v002  SteamAPI_SteamNetworkingUtils_SteamAPI_v004

SteamAPI_ISteamUser_GetSteamID  SteamAPI_ISteamUser_BLoggedOn
SteamAPI_ISteamMatchmaking_CreateLobby / JoinLobby / LeaveLobby / SetLobbyData /
    SetLobbyJoinable / GetLobbyOwner
SteamAPI_ISteamFriends_SetRichPresence  SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog
SteamAPI_SteamNetworkingIdentity_SetSteamID64 / GetSteamID64
SteamAPI_ISteamNetworkingMessages_SendMessageToUser / ReceiveMessagesOnChannel /
    AcceptSessionWithUser / CloseSessionWithUser
SteamAPI_SteamNetworkingMessage_t_Release
SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32
```

`SteamAPI_InitFlat(char errMsg[1024])` returns 0 on OK, 1 for a generic failure,
2 when Steam isn't running, and 3 for a version mismatch. Write
`steam_appid.txt` = `480` yourself if it's missing.

### Callbacks: manual dispatch (no C++ CCallback classes)

Call `ManualDispatch_Init()` once. Then call this every frame:

```
pipe = SteamAPI_GetHSteamPipe();
ManualDispatch_RunFrame(pipe);
while (ManualDispatch_GetNextCallback(pipe, &msg)) {
    if (msg.callback == 703 /* SteamAPICallCompleted_t */) {
        // async call result (e.g. LobbyCreated, LobbyEnter from CreateLobby/JoinLobby)
        buf = malloc(done->param_size);
        if (ManualDispatch_GetAPICallResult(pipe, done->call, buf, done->param_size, done->callback, &failed) && !failed)
            handle(done->callback, buf);
    } else handle(msg.callback, msg.param);
    ManualDispatch_FreeLastCallback(pipe);
}
```

Callback ids:

| Callback | Id |
|---|---|
| GameLobbyJoinRequested | 333 |
| GameRichPresenceJoinRequested | 337 |
| LobbyEnter | 504 |
| LobbyChatUpdate | 506 |
| LobbyCreated | 513 |
| SteamAPICallCompleted | 703 |
| SteamNetworkingMessagesSessionRequest | 1251 |
| SteamNetworkingMessagesSessionFailed | 1252 |

### Struct layouts (Windows, `#pragma pack(push, 8)`)

```
CallbackMsg_t            { int32 hSteamUser; int32 callback; uint8 *param; int32 param_size; }
SteamAPICallCompleted_t  { uint64 call; int32 callback; uint32 param_size; }
LobbyCreated_t           { int32 result; uint64 lobby; }                // 8-byte aligned
LobbyEnter_t             { uint64 lobby; uint32 perms; bool locked; uint32 response; } // 1 = success
LobbyChatUpdate_t        { uint64 lobby; uint64 user_changed; uint64 making_change; uint32 state_change; }
                          // state flags: left 0x2, disconnected 0x4, kicked 0x8, banned 0x10
GameLobbyJoinRequested_t { uint64 lobby; uint64 friend; }
GameRichPresenceJoinRequested_t { uint64 friend; char connect[256]; }
```

- **SteamNetworkingIdentity** is 136 bytes. Treat it as an opaque buffer (KeeperFX
  allocates 160) and fill or read it only through `SetSteamID64` / `GetSteamID64`.
  The session request/failed callbacks start with that identity.
- **SteamNetworkingMessage_t** (32-bit, MSVC build of the DLL): `data` at offset 0,
  `size` at 4, and the sender identity at **12** in the real 32-bit `steam_api.dll`
  (Steamworks.NET Standalone 2025.164.1; confirmed between two real accounts in
  Clive Barker's Jericho, 2026-10-21). 16, which the 8-byte packing of the headers
  suggests, reads sender 0: every message is then dropped as unknown and nothing
  says why, while the session request (whose identity sits at 0) still looks fine.
  Read 16 and 12 and keep whichever decodes to a non-zero id (KeeperFX does the
  same), and log which one won. A mock that writes the identity at 16 passes either
  way, so only a two-account test catches this. Always call
  `SteamNetworkingMessage_t_Release`.
- **SteamNetworkingMessage_t on 64-bit** (`steam_api64.dll`): `data` at 0 (an
  8-byte pointer), `size` at **8**, sender identity at 16. Reading `size` at 4 on
  x64 gives garbage. `FriendGameInfo_t` and the callback structs above keep the
  same layout on x64.

### Messaging rules learned the hard way

- **Session accept:** the first message from an unknown SteamID raises
  SessionRequest (1251). Call `AcceptSessionWithUser` there, or messages are held
  or dropped. The host accepts strangers; clients accept only the host.
- **No disconnect event:** ISteamNetworkingMessages doesn't tell you when a peer
  closes. KeeperFX handles this in three ways:
  - a control channel (channel 1) carries goodbye, kick-with-reason and a 1 s keep-alive;
  - a peer silent for 20 s is dropped;
  - LobbyChatUpdate left/disconnected flags and SessionFailed also drop the peer.
- **Answer keep-alives before game data starts.** A friend's relay usually joins the lobby
  while their game is still loading, so for a minute only keep-alives flow. If the host
  only tracks peers that sent game data, the joiner sees silence and gives up. Track every
  lobby member that talks on the control channel, and have the joiner rejoin instead of
  exiting when the host goes quiet (The Witcher 3 relay, found by the two-client test).
- **LobbyCreated arrives twice on real Steam:** once as the `CreateLobby` call result
  (703) and once as a broadcast callback (513). Handle it idempotently; a mock that sends
  it once won't show this (The Witcher 3 relay's real-Steam smoke test).
- **Channels:** game data is reliable and ordered on channel 0. Unsequenced or
  unreliable data is sent with `Unreliable` on the same channel. All sends add
  `NoNagle`. Send flags are Unreliable 0, NoNagle 1, Reliable 8.
- **Large messages:** big messages (save/resync snapshots) are split into
  60000-byte pieces with a 1-byte frame header (whole/part/last) and reassembled.
  Raise `SendBufferSize`, `RecvBufferSize` and `RecvMaxMessageSize` to 32 MB through
  `SetGlobalConfigValueInt32`. If a reliable send returns an error because the
  buffer is full, wait about 5 ms and retry.
- **Invites:**
  - after LobbyCreated, the host calls `SetLobbyData` and
    `SetRichPresence("connect", "+connect_lobby <id>")`, then opens
    `ActivateGameOverlayInviteDialog(lobby)`;
  - the overlay may not open in an old D3D9 game (it didn't in Clive Barker's
    Jericho, with the overlay loaded): list the friends online in your own menu
    (`GetFriendCount(4)`, `GetFriendByIndex`, `GetFriendPersonaState` != 0) and send
    `ISteamMatchmaking::InviteUserToLobby(lobby, friend)` for the one picked. The
    friend gets a normal Steam invite;
  - a friend accepting while the game is running gives callback 333;
  - Join Game from the friends list gives 337 with the connect string;
  - when the game isn't running, Steam launches it with `+connect_lobby <id>`
    on the command line, so parse that too.
- **Lobby id:** show the id on screen and copy it to the clipboard as a manual
  fallback (`-steamjoin <id>`).

## Testing without two Steam accounts: the mock DLL

One PC can only run one Steam account, so you can't test two players locally with
real Steam. Build a mock: about 400 lines of C compiled as a DLL with the same
name and the same flat exports (the Deus Ex co-op repo ships one in
`steamlink/mock/`). It:

- it fakes the SteamID from the `-nick` argument;
- it passes messages between the two local processes over localhost UDP;
- it fakes lobby create/join with deterministic lobby ids;
- like real Steam, it holds the first message from a stranger until
  AcceptSession;
- it can fake an accepted invite or a Join Game click (env `KFX_MOCK_STEAM_INVITE`).

Swap it in for the real `steam_api.dll` in the two test installs and run the
normal two-instance tests. **Copy the mock and extend it with whatever extra
exports your game uses** (HXCoop added the friend-list calls it uses to find a
hosting friend).

### Unity Mono games: mock Steamworks.NET, not the DLL

When the game or the mod talks to Steam through Steamworks.NET (the managed C#
wrapper), you don't need a native mock. A test-only BepInEx plugin can patch the
wrapper with Harmony and leave `steam_api64.dll` alone. Graveyard Keeper's
co-op mod (Back From The Grave) was tested this way: about 1,100 lines of C# for
the mock and 300 for its transport.

- **Find the calls:** grep the mod for `Steam(User|Friends|Matchmaking|Networking|Utils)\.`
  and give each one a Harmony prefix that returns fake data and skips the
  original. Fail loudly if a method isn't found.
- **Callbacks:** a `Callback<T>` registers with Steam's native dispatcher. Prefix
  `Callback<T>.Register` and `Unregister` for each callback type the mod uses,
  keep the instance and its delegate (field `m_Func`) in your own table, and invoke
  it yourself from the plugin's `Update` on the main thread. For `CallResult<T>.Set`,
  take over only the call handles your mock issued (start them at a high fixed
  value) and pass real handles on to Steam.
- **Load order:** the patches must be in place before the mod registers its
  callbacks, so the plugin must load first. A GUID that sorts first (`aaa.coopqa`)
  was enough there. Make it do nothing unless a command-line flag (`-coopqa`) is set.
- **Keep real Steam for the rest:** the Steam client stays signed in, and the game
  still checks DLC and achievements against it. Only the calls the mod uses for
  multiplayer are faked.
- **Ids and transport:** fake individual SteamIDs from a reserved account range
  (900000000 + player number) and lobby ids as chat-type ids. Lobby data and P2P
  packets go between the copies over localhost TCP, one port per player.
- **Copy the Steam rules the mod relies on:** unreliable sends over 1200 bytes and
  any send over 1 MB fail; the first packet from a stranger raises
  `P2PSessionRequest_t` and is held until `AcceptP2PSessionWithUser`; create fires
  `LobbyCreated_t` then `LobbyEnter_t`; a join fires `LobbyDataUpdate_t` then
  `LobbyEnter_t` on the joiner and `LobbyChatUpdate_t` on the members; every data
  change fires `LobbyDataUpdate_t` on every member; when the owner leaves, the
  next member becomes the owner. Add switches for extra latency and unreliable
  packet loss.
- **A setup error must fail the run.** `AccessTools.Method` without parameter
  types throws on an overloaded method. The plugin then stopped without starting
  the bot, and the test only timed out. Print `RESULT: FAIL` for setup errors, and
  have the launcher fail a copy that writes no status file.
- **A mock picks one order of events.** Ours always delivered lobby data before the
  first P2P packet. That made a race in the mod fail on every join: the joiner's
  late compatibility hello locked the mod's transport to the wrong channel. The
  other order would have hidden it. Where real Steam doesn't promise an order, add
  a switch for each order (we haven't added one yet).

### Unity IL2CPP games: call steam_api64 flat, poll everything

In an IL2CPP game the game's own Steamworks.NET is compiled to native code, and
IL2CPP strips every method the game never calls, so the matchmaking and
networking calls a mod needs are often missing from the interop assemblies.
Harmony can't help either: there is no managed body to patch. Dave the Diver
(Unity 6000.0, BepInEx 6 be.790) did it this way:

- **Call the flat API yourself.** `[DllImport("steam_api64")]` on the
  `SteamAPI_*` exports (`SteamAPI_SteamMatchmaking_v009`,
  `SteamAPI_SteamNetworkingMessages_SteamAPI_v002` and so on). The game has
  already run `SteamAPI_Init` with its real app id, so the interfaces work.
  The DLL is in `<game>_Data\Plugins\x86_64`, outside the default search path:
  set `NativeLibrary.SetDllImportResolver` to load it from there.
- **No callbacks.** A plugin can't register `CCallback`s here. Poll instead:
  `ISteamUtils::IsAPICallCompleted` and `GetAPICallResult` for lobby create and
  join, `GetNumLobbyMembers` and `GetLobbyOwner` for membership, and
  `GetFriendGamePlayed` to list friends who are hosting.
- **Accept sessions by polling.** With no `SteamNetworkingMessagesSessionRequest_t`
  callback, the host calls `AcceptSessionWithUser` every 0.5 s for each lobby
  member it hasn't accepted yet. It accepts only members of its own lobby.
- **Tune the send buffer for big transfers.** The default rate and buffer choked
  on a save transfer: every unacked chunk was re-sent after 0.1 s, the relay
  link clogged, and even keepalives stopped. A retransmit timeout from the
  measured round trip, at most 32 chunks in flight, and a larger
  `SetGlobalConfigValueInt32` send buffer and rate fixed it.
- **Mock it in the mod, not the DLL.** One Steam account can't P2P to itself.
  A lab switch (`-dtmp-steam-mock <slot>`) sends the same calls to an in-mod
  stand-in: fake account `900000000 + slot`, lobbies as files in a folder both
  lab copies share, and datagrams over localhost UDP. Like Steam, it holds a
  peer's datagrams until `Accept` or until we have sent to that peer first.

## Reusing it for Deus Ex (Unreal Engine 1) co-op

KeeperFX had a swappable network layer (`struct NetSP`), so Steam slots in as one
more implementation. **UE1 doesn't have that.** Its networking is `UTcpNetDriver`
(IpDrv), a UDP NetDriver/NetConnection set in `DeusEx.ini` under
`[Engine.GameEngine] NetworkDevice=` / `[IpDrv.TcpNetDriver]`. Options:

1. **A custom NetDriver (best fit):** write a native C++ package with
   `USteamNetDriver` / `USteamNetConnection` subclassing `UNetDriver` /
   `UNetConnection`. `LowLevelSend` becomes `SendMessageToUser` (unreliable; UE1
   does its own reliability), and `TickDispatch` drains `ReceiveMessagesOnChannel`
   into `ReceivedRawPacket`. Point `NetworkDevice` at it in the ini. Map "URL host"
   to a SteamID, e.g. `open steam.76561198xxxxxxxxx` or the lobby id. This needs
   the UE1 public headers (the Deus Ex SDK / UT432 headers) and an MSVC 6-compatible
   build, because the engine is 32-bit MSVC.
2. **A local UDP proxy (no engine code):** a small helper exe or DLL loaded with
   the game. It binds 127.0.0.1:7790 on the client and talks to the host over Steam
   P2P. The host side forwards to the real server port. The game just does
   `open 127.0.0.1`. This is the quickest way to get something working; it's the
   same idea as the mock DLL turned inside out.

Either way, reuse the parts from this document as-is: the dynamic loading, the
manual callback dispatch, the lobby and invite flow, the session-accept rule, the
keep-alive and timeout, and the mock DLL. Because UE1 handles its own
reliability, sending everything Unreliable is usually right. The 60000-byte
splitting probably isn't needed, since UE1 packets are about 512 bytes.

## Finding the host without codes

HXCoop's join side needs no lobby id: it walks the friend list
(`GetFriendCount` / `GetFriendByIndex` with flag 4 = immediate friends) and calls
`GetFriendGamePlayed`. A friend whose `FriendGameInfo_t.m_gameID` has AppID 480 in
its low 24 bits and a non-zero `m_steamIDLobby` (offset 16) is hosting. Join that
lobby, then check your own lobby data key (`hxcoop=1`) so you don't join someone
playing the real Spacewar.

## A relay instead of engine code (HXCoop pattern)

- Host: one local UDP socket per remote player, `connect()`ed to the game's port on
  127.0.0.1. Steam messages from that player go out of their socket, and whatever
  the game answers on it goes back to that player. The game sees each friend as a
  separate 127.0.0.1 client.
- Client: bind 127.0.0.1 on a free port, start the game connecting to it, forward
  everything to the host's SteamID.
- Send game packets unreliable with NoNagle (the engine has its own reliability).
  Keep-alive and goodbye on channel 1 as reliable messages.
- **Only lobby members get through.** Before forwarding a new peer's traffic into the
  local server, check that their SteamID is in your lobby
  (`GetNumLobbyMembers` / `GetLobbyMemberByIndex`) and drop anyone else. Add
  those two exports to the mock too.
- Exit when the game process exits, and leave the lobby and clear rich presence.
- The engine's own packet lag and loss emulation (Unreal Engine 1 has `PktLag=`,
  `PktLoss=`, `PktOrder=`, `PktDup=` console commands) is a cheap stand-in for
  relay latency in automated tests.

## TCP games: a stream relay

The pattern above is for UDP games. When the game talks TCP (many source ports
and emulator netplay do), relay streams instead of packets:

- Give each TCP connection a **stream id**, and send small frames over Steam:
  `open(id)`, `data(id, bytes)`, `close(id)`.
- Send them **reliable and ordered** (TCP expects that), split into chunks of
  about 16000 bytes.
- When a reliable send fails because the send buffer is full, wait a few ms and
  retry; never drop stream data.
- Propagate close both ways: a local socket closing sends `close(id)`, and a
  `close(id)` from the peer shuts the local socket. Otherwise one side hangs on a
  half-open connection.
- The host opens one local TCP connection to the game's server per remote stream
  id, the same way the UDP relay opens one socket per player.

## Writing the helper exe: gotchas

- If another tool starts the helper with redirected output, call
  `setvbuf(stdout, NULL, _IONBF, 0)` or its log arrives late or never.
- Check that stdin is a console before using `_kbhit` / `_getch` for a menu; with
  redirected stdin they misbehave.
- When the helper starts a local game server as a child process, start it with
  `CREATE_NEW_PROCESS_GROUP` and stop it with Ctrl+Break
  (`GenerateConsoleCtrlEvent`) so it shuts down cleanly, and put it in a job object
  with kill-on-close so it never outlives the helper.
- Export macros: `__declspec(dllexport)` and `__cdecl` in the order mingw accepts
  can fail under MSVC. Test the mock DLL with both compilers if you support both.

## Packaging and status

- Ship `steam_api.dll` (or `steam_api64.dll`) next to the helper exe, and have the
  helper write `steam_appid.txt` (`480`) itself, so a missing file can't break it.
- Make the status line honest and specific: say whether real game traffic has
  passed through the relay yet (not just "connected to Steam"), and when someone
  joins a lobby id that isn't one of yours (the lobby data key is missing), say so
  plainly ("that lobby is not a <game> co-op game").

## Status / unknowns

- All tests on the mock pass with 0 desyncs: in-level co-op, a 3-level campaign
  flow, campaign progression, clean and hard-kill disconnects, and both invite
  paths. The ENet regression also passes.
- The real-Steam single-instance test passed:
  - `InitFlat` succeeded and lobby creation returned a lobby id;
  - the lobby id showed on screen and was copied to the clipboard.

  `ActivateGameOverlayInviteDialog` returns void, so there's no success value,
  and nobody saw the overlay on screen.
- Real two-account play (Clive Barker's Jericho co-op, 2026-10-21, two PCs on one LAN,
  two Steam accounts that are friends):
  - the message sender is at offset **12**, not 16 (above): with 16 nobody could join;
  - with that fixed, a friend joins from the in-game friends' games list and the
    session plays (lobby, level load, world sync), round trip 6-19 ms;
  - the overlay invite dialog did not open in that D3D9 game: invites go through
    `InviteUserToLobby` from the game's own list (above);
  - a scripted two-account test (host on one PC reads its lobby id from its log, the
    other PC's game joins it by id) now runs with every regression there.
  Still open: Join Game auto-launch when the game is not running, and two PCs on
  different networks (relay rather than LAN).
- HXCoop: all tests pass over the mock with real game clients relayed through it.
  Real two-account play: check the sender offset first.
