# Linux Steam lobby/message adapter: mock-tested helper only

Original C11/POSIX Linux x86-64 code, no SDK implementation bodies or Valve binary.
This helper is not integrated with the native game and does not establish Internet
play, a visible overlay, real friend invites or native two-player capability.
Do not initialize the real Steam library until the concrete frontend and live
account/overlay/lobby actions receive the required review/authorization.

## Build/test

```sh
make test
python3 tests/negative_controls.py
```

Installed compiler/make/libdl only; Python 3 runs mutation controls. Strict flags:
`-std=c11 -Wall -Wextra -Werror -pedantic -O2 -g`. Every test loads exactly
`./tests/libmocksteam.so`, an original mock. No real Steam library is loaded or
called. No AppID/config/installation/account/friend-list changes occur.

The process test forks two real processes and passes 4096-byte payloads in each
direction through mock Steam message calls over actual UDP. The only sockets are
127.0.0.1 ephemeral ports; both are explicitly closed and rebound after shutdown.
The suite has a 20-second alarm and needs an environment that permits local
loopback sockets. It is not a network impairment, relay, Internet or real Steam
test.

The suite also independently checks nondefault published metadata, delivery to
the intended peer of the original copied binary payload exactly once, and send
deadline expiry while valid inbound traffic keeps the receive deadline live.

The negative runner preserves the buildable RED stub and eight previous bugs:
public lobby type, accepting an outsider's session, delivering an outsider's
message, bypassing silence timeout with outsider traffic, skipping metadata,
falsely accepting a busy output slot, leaking a received message, and reading
LobbyCreated using a Windows-like offset. Five additional mutations publish a
wrong key or value, send to the wrong peer, overwrite a busy payload, or remove
the send deadline. All thirteen mutations compile successfully. Only normal
test assertion failures count; compile failure, signal or timeout does not. Individual
logs remain; temporary mutation sources/binaries are removed. Use
`--log-dir <new directory>` to keep separate runs' evidence.

## API and bounds

`sa_open(context, explicit_library_path, config, now_ms)` resolves all required
symbols before initialization, then obtains versioned client interfaces, requires
logged-on status/nonzero local SteamID, and enables manual dispatch. Missing
library/symbols and initialization/interface/account failure return -1. This API
can call real Steam if a future reviewed frontend explicitly supplies that library;
there is no automatic library search or fallback. One active context per process,
one application thread. Storage cannot move/copy while active. Close before reuse.

Config contains bounded nonempty NUL-terminated game/build/protocol strings and a
nonzero timeout <=600000ms. The caller supplies a monotonic millisecond clock and
must pump regularly. Calls do not sleep or change networking configuration.

- `sa_host`: creates friends-only lobby type 1 with exactly two member slots.
  On asynchronous success, makes it unjoinable while setting `game`, `build`,
  `protocol`, then enables joins. `SA_LOBBY_READY` assigns host P1. Steam's
  friends-only type allows friends/invitees and is not the private type 0.
- `sa_invite`: explicit host-only request while waiting for P2. Calls the void
  overlay invite function; return 0 means dispatched, not rendered/sent/accepted.
- `sa_poll` exposes GameLobbyJoinRequested (333) as `SA_JOIN_REQUEST`; the app
  decides whether to call `sa_join` with that lobby ID. No automatic invite joins.
  Direct join also requires explicit app action. Rich-presence callback 337,
  cold-start command-line parsing and friend-list discovery are not implemented.
- On guest entry, exact game/build/protocol metadata must match. Owner, member
  count/limit and own membership are checked. Guest P2 pins owner P1; host P1 pins
  the sole other member P2. Subsequent polls verify owner/partner stability,
  self membership, max-two limit and metadata. Owner migration, member loss,
  third member or peer substitution terminates the context instead of adopting
  a different player. P2 reconnect/session restoration remains game-adapter work.
- Session requests are accepted only for the current joined partner with current
  membership verified. Others are closed. Received messages from other identities
  are dropped, closed and released without refreshing partner liveness; the
  silence deadline is still enforced before returning. Trusted messages must be
  channel 0, size 0..4096, with valid data storage. Every received Steam message is released.
- `sa_send` copies one opaque <=4096-byte message: 1 queued, 0 busy, -1 invalid.
  Only joined partner sends are allowed. Poll sends Reliable|NoNagle (9) on
  channel 0. LimitExceeded (25) retains the one pending copy for bounded retries;
  other errors fail. Successful queue/send does not prove delivery/application.
- `sa_poll`: 1 event, 0 pending, -1 `SA_FAILURE`. At most eight callbacks and one
  inbound message are handled per call. Recognized callback sizes and async
  call/result identity are checked; callback parameters are bounded at 1024
  bytes and always freed after GetNextCallback. No size-driven allocation.
  SessionFailed reads its verified leading 136-byte identity only.
- Create/join, queued sends and partner receive silence use the configured
  deadline. Idle waiting host/idle menu have no timer. There is no heartbeat
  generator: a future protocol must supply one or choose suitable deadlines.
  Failure events are currently generic; frontend-friendly reason text is deferred.
- `sa_close` closes partner session, leaves known lobby, shuts down the initialized
  Steam API, unloads the library, and clears local state. It is idempotent.

This layer transports opaque application messages. It does not implement the
common version/ROM/native-mode handshake, epoch/input sequence rules, shared-world
transactions, snapshots, rendering, saves or campaign semantics. Those must be
connected above it after the room/game adapter proof. Metadata is a compatibility
filter; Steam identity and lobby APIs are trusted here, but only mocked identities
were exercised. Neither mock IDs nor metadata flags prove actual authentication,
capability, secrecy or Internet reachability. No raw RAM merging is used.

## Verified Linux ABI and provenance

Pinned primary source: Steamworks.NET 2025.164.1 / SDK 1.64, commit
`c21a8f0e31c56ae8707130967faf491f7dd7c0d8`. These are original minimal C declarations
of ABI facts; no generated wrapper or SDK implementation bodies were copied.

- [Packsize.cs](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/com.rlabrecque.steamworks.net/Runtime/Packsize.cs)
  selects small packing for Linux, value 4.
- [Native flat declarations](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/CodeGen/steam/steam_api_flat.h),
  [manual dispatch declarations](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/CodeGen/steam/steam_api.h),
  [callback envelope](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/CodeGen/steam/steam_api_internal.h)
  verify exact function signatures and versioned getter names.
- [Native network types](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/CodeGen/steam/steamnetworkingtypes.h)
  put Identity inside pack1, while Message is outside callback packing.
- [Native matchmaking declarations](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/CodeGen/steam/isteammatchmaking.h)
  and [callback declarations](https://github.com/rlabrecque/Steamworks.NET/blob/c21a8f0e31c56ae8707130967faf491f7dd7c0d8/com.rlabrecque.steamworks.net/Runtime/autogen/SteamCallbacks.cs)
  verify lobby type and callback fields/IDs.

Compile-time assertions enforce Linux x86-64: CallbackMsg size20/data offset8;
LobbyCreated size12/lobby offset4; LobbyEnter size20/response offset16;
CallCompleted size16; Identity size136; native Message size216/peer offset16/
channel offset192. Callback result fixtures independently encode numeric offsets.
Interface versions are User_v023, Friends_v018, Matchmaking_v009 and
NetworkingMessages_SteamAPI_v002. Loader function signatures use C/POSIX dlsym;
no C++ vtables or Windows pointer-offset guesses.

Read-only `nm -D` inspection found all 32 required exports in the Linux redistributable
from the pinned release, SHA256
`ec4797f76a206eb0af627af0f0788eb5a2eabf5dee38ee0a6affbaa44a645f4e`.
That library was not loaded, initialized or called, and is not in this folder.
Its redistribution/license and AppID/provider setup remain packaging decisions.
