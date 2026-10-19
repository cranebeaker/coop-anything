# No Rest for the Wicked: notes

## Identity (observed 2026-10-19)

- Store: Steam, appid 1371980, buildid 22928553
- `NoRestForTheWicked.exe` SHA-256: `FE6EC10D47D183C872ED575114EBCA29F6EBD99B47E0AE66C6316A62A6044CF6`
- Engine: Unity 6000.1.15 (exe FileVersion), IL2CPP (`GameAssembly.dll`, `global-metadata.dat`)
- OS: Windows
- Plugins: `steam_api64.dll` (Steamworks.NET), Wwise (`AkSoundEngine.dll`), Backtrace crashpad

## Observations

- `global-metadata.dat` contains the strings "Photon" (328 hits), "Steamworks" (376),
  "Multiplayer" (78), "COOP" (19). Plain ASCII string scan, no decompilation.

## Sources

- Official "Together" co-op update, up to 4 players, shared persistent Realms that do not
  need the host online, PC on 2026-01-22:
  - https://www.gematsu.com/2025/12/no-rest-for-the-wicked-early-access-together-update-launches-january-22-2026-open-beta-test-now-available
  - https://www.rpgsite.net/news/19232-no-rest-for-the-wicked-multiplayer-co-op-update-release-date-gameplay
  - https://mp1st.com/news/no-rest-for-the-wicked-together-co-op-update-out-next-week

## Connection failure diagnosis (2026-10-19, build 29466 / Steam buildid 22928553)

Symptom from the owner: official co-op, owner and friend cannot connect.

Observed in `%USERPROFILE%\AppData\LocalLow\Moon Studios\NoRestForTheWicked\Player.log`:

- `GetAuthSessionTicket returned Invalid handle`, then `Failed to get Steam auth ticket`,
  then `Failed to connect to backend using Steam.` about 3 s after boot.
- `Could not resolve endpoint from gatekeeper` on
  `GET https://prod.gatekeeper.moonbackend.net//v3/staging?buildId=29466&modern=True`,
  inner exception `NameResolutionFailure`.

Checked on the network side:

- `prod.gatekeeper.moonbackend.net` returns NXDOMAIN from the authoritative Route 53
  server (`ns-161.awsdns-20.com`), from 1.1.1.1 and 8.8.8.8, from Cloudflare and Google
  DNS-over-HTTPS, and from Google DoH with EDNS client subnets for US, UK, DE, FR, JP, BR.
  Every `*.moonbackend.net` name found in `global-metadata.dat` (`earlyaccess.api`,
  `earlyaccess.gatekeeper`, `earlyaccess.queue`) is NXDOMAIN as well. The zone apex
  `moonbackend.net` exists.
- No `moonbackend` entry in the hosts file. Steam client running, not in offline mode.
  `steam_api64.dll` (`NoRestForTheWicked_Data\Plugins\x86_64`) is Authenticode-signed by
  Valve Corp., status Valid.
- Steam public branch buildid is 22928553 (api.steamcmd.net), same as installed.
- Steam announcements up to 2026-10-16 mention no shutdown. 1.0 is set for March 2027.

Sources:

- Official forum, same log lines, unfixed bug thread with workarounds:
  https://forum.norestforthewicked.com/t/ongoing-permanently-offline/22173
  (posts 12, 20, 94, 104: VPN or Cloudflare WARP got the game online; 17, 48, 59: switch
  to the `crucible_legacy` beta and back; 31, 33, 35: a firewall or security suite blocked it; 82:
  an internet filter blocked it; 63: phone hotspot; 90: same `Failed to get Steam auth
  ticket` line; 102: the problem followed the Steam account, not the PC or network.)
- Steam news, outage 2026-02-16:
  https://store.steampowered.com/news/app/1371980/view/524242197281767745

Derived:

- The gatekeeper NXDOMAIN happens from every region, yet the forum shows players online the
  same week. So that lookup failing is probably not what blocks the owner. It may be an
  optional lookup with a fallback. Not verified.
- The blocking step is the Steam auth ticket. Without it the game never logs in to Moon's
  backend, stays offline and cannot share or join Realms.

Update after decompiling (see "Local backend server + redirect" below):

- Confirmed: the dead `prod.gatekeeper.moonbackend.net` lookup belongs to the older
  `AwsBackend` path. The login itself (`MoonBackendNew.LoginWithSteam`) uses
  `prod.gatekeeper.norestforthewickedbackend.net`, which resolves.
- Confirmed: the ticket failure alone stops the login. The test copy (no OptiScaler) logs
  the same `GetAuthSessionTicket returned Invalid handle`; with a placeholder ticket the
  login goes on to the gatekeeper, token and queue calls.

Unknown:

- Whether a working client also logs the gatekeeper NXDOMAIN. Needs a log from an online player.
- The friend's log. Not collected yet.
- Whether OptiScaler / fakenvapi (`dxgi.dll` proxy) plays a part. No source links it to
  the offline bug.

## Network stack, from a plain string scan of global-metadata.dat (2026-10-19, build 29466)

Observed (assembly and type names only, no decompilation yet):

- Netcode: Photon Quantum (`Quantum.Deterministic.dll`, `Quantum.Engine.dll`,
  `DeterministicSessionConfigAsset`, `CreateDeterministicSession`) over Photon Realtime
  (`Photon.Realtime.dll`, `PhotonClient.dll`, `LoadBalancingClient`, `AppIdQuantum`).
  No Fusion or PUN.
- Photon uses custom authentication (`AuthenticationValues`, `OnCustomAuthenticationResponse`).
- A configurable Photon server list: `moon.photonUtils|PhotonServerConfig`,
  `PhotonServerRegistry` (`Assets\scripts\core\network\photonUtils\PhotonServerRegistry.cs`).
- Moon REST backend in `moon.backend.dll`: gatekeeper, auth and queue endpoints
  ("Authenticate via the gatekeeper endpoint", "... auth endpoint", "... queue endpoint"),
  JWT bearer tokens (`GetBearerToken`, `GetJwtSecurityTokenFromToken`), `/api/status`.
  Account calls: `GetAccountRealms`, `GetAccountCharacters`, `GetAccountRealmInvites`,
  `CreateSharedRealm`, `CreatePrivateRealm`, `CreateEphemeralRealm`, `DeleteBackendRealm`,
  `DeleteBackendCharacter`. Steam login through `GetAuthSessionTicket` and `GetEncryptedAppTicket`.
- Local, offline paths exist: `CreateNewLocalRealm`, `CreateNewLocalCharacter`,
  `CreateOfflineRealmMembers`, `QuantumLocalInputSource`, `GetLocalPlayers`,
  `GetLocalPlayerSlots`, "Use LocalPlayers".
- No anti-cheat files in the install.

Derived (guesses until decompiled):

- A private server must stand in for three things: the Moon REST backend, a Photon
  master/game server that accepts the custom auth, and the Quantum server that orders
  inputs each tick. The third is the hard part, since Photon only offers a self-hosted
  Quantum server under an Enterprise licence.
- "Use LocalPlayers" and the local input source hint that Quantum's local multiplayer
  path is compiled in, so a split-screen route may exist. Not checked.

## Second gatekeeper host (2026-10-19)

Observed:

- A read-only string scan of the running game's memory (owner's PC, game open) and of
  `global-metadata.dat` finds a list of hard-coded backend URLs, in this order:
  `https://patagonia-int.maelstrom.gameservices.xboxlive.com/tenants/patagonia/routes/steam/`,
  `https://patagoniabackend-dev.azurewebsites.net`, `https://prod.gatekeeper.moonbackend.net/`,
  `https://prod.gatekeeper.norestforthewickedbackend.net`.
- `prod.gatekeeper.norestforthewickedbackend.net` resolves (three AWS A records) and
  answers `GET /v3/staging?buildId=29466&modern=True` with HTTP 403 `{"message":"Forbidden"}`
  when sent without an API key. Metadata has the strings "Explicitly set the gatekeeper API key."
  and "Explicitly set the auth server API key.".
- The client used the dead `moonbackend.net` host in both runs today.
- The memory also shows Photon region hosts (`GCAMS1090.exitgames.com`, `GCASH1301`,
  `GCHK1023`, `GCSYD1000`, `ns.photonengine.io`, `ns-us.photonengine.io`), so the client
  reaches Photon's name server for region pings.
- `NoRestForTheWicked_Data\StreamingAssets\backend_config.json` holds only
  `{"Experimental": false}`. Metadata has `EnvironmentUtils`, `CURRENT_ENVIRONMENT`,
  `BackendFactory` with `Offline` and `Aws`.

Tested:

- Setting `Experimental` to `true` did not change the host: the next run still called
  `prod.gatekeeper.moonbackend.net`. The file is restored.

Derived:

- Which host is picked is decided in code, not by `backend_config.json`.

## Local backend server + redirect (2026-10-19, build 29466 / Steam buildid 22928553)

Goal: run Moon's REST backend locally so the game logs in, lists realms and saves
without Moon's servers. Work done on a test copy of the install; the owner's install and
saves were not changed (saves backed up first; the plugin redirects
`Application.persistentDataPath`, the game log goes elsewhere via `-logFile`).

Tools, all pinned by SHA-256:

- Cpp2IL 2022.1.0-pre-release.21 (GitHub release, Windows exe)
  `663fb432433b4371fd1ee0ebc321a8fff2a9aac5ac4230c843f9e03ddee4e04c` (matches GitHub digest).
- BepInEx 6.0.0-pre.2 IL2CPP (newest GitHub release) does NOT work:
  `Unsupported metadata version found! We support 23-29, got 31`, then the game crashes.
- BepInEx 6.0.0-be.788+5b766a3 IL2CPP win-x64 from builds.bepinex.dev (owner approved)
  `f4cc496bd098a0df4164b81e3737297707f13a47c2478dba2f60eefab784817a` (no published digest).
  Interop generation and chainloader work on Unity 6000.1.15.

Observed (from the Cpp2IL dump, runtime dumps of the game's own key tables, and test runs):

- Login chain, `InitializeMoonBackend.TrySignIntoBackend` -> `MoonBackendNew.LoginWithSteam`:
  1. `GET https://prod.gatekeeper.norestforthewickedbackend.net/v3/prod?buildId=29466&platform=FINAL&modern=True`,
     header `x-api-key` = constant `InitializeMoonBackend.c_gatekeeperApiKey`. Answer is the
     `Response` envelope `{"errors":[],"data":[GatekeeperEndpointsFromId]}` with
     `{"endpoints":{"auth","queue","rest","main","wss"},"authApiKey"}`.
  2. `POST {auth}/v3/token` (then `/v3/signup` if that fails), body
     `{"entitlementTicket":"<hex Steam ticket>","entitlementPlatform":"steam"}`, answer
     `BackendAuth {accountId, email, queueApiKey, token, emailValidationMessageId}`. A HS256
     JWT made by the local server is accepted.
  3. `POST {queue}/join`, answer `LoginQueueInfo {mainApiKey, accessToken, buildId}`.
  The SDK picks the API key by comparing base URLs, so auth, queue and main need distinct
  URLs (same URL for all gives "API key missing ... Authenticate via the queue endpoint").
- Save backend: `POST {main}/save/<accountId>/<Operation>` with plain BackendJson bodies and
  answers (no envelope). Operations: ListRealms, ListCharacters, CreateCharacter,
  CreateRealm, CreateEphemeralRealm, LoadRealm, LoadCharacter, SaveData, DeleteCharacter,
  QueryRealmParticipation, PlayRealm, LeaveRealm, Invite, AcceptInvite, Kick,
  SetRealmParticipantOwner, GetAccountIdsFromSteamIds, UpsertAccount. Keys are PascalCase
  (`BackendJson.s_keysFor*`), `byte[]` is base64, Guid is the usual dashed string, enums
  answered as ints were accepted. Header `Current-Epoch-Ms` gives server time.
- The older `AwsBackend` also asks `https://prod.gatekeeper.moonbackend.net//v3/staging`.
  Answering it the same way removes the "Could not resolve endpoint from gatekeeper" error.
- `GetAuthSessionTicket returned Invalid handle` also happens on the test copy, so the
  login never starts. The plugin hands the game a placeholder ticket instead.

How the redirect works (BepInEx plugin, inside the test copy only):

- Harmony prefix on the game's `IMoonBackendHttpClient.SendAsync` implementations
  (`ForsakenBackendClient`, `MoonBackendHttpClient`) rewrites any URL whose host ends in
  `moonbackend.net`, `norestforthewickedbackend.net` (and two dev hosts) to
  `http://127.0.0.1:5800/_h/<host>/<path>`. Plain http, so no certificate check is involved;
  no hosts file or root certificate change.
- A patch on `Il2CppSystem.Net.Http.HttpClient.SendAsyncWorker` never fired: IL2CPP
  inlined it into its callers. The same happened to the async stub
  `InitializeMoonBackend.GetSteamAuthTicket`, so the ticket bypass is a native detour
  (`INativeDetour`) on its state machine `<GetSteamAuthTicket>d__4.MoveNext`: on the first
  step it stores a completed `Task<string>` in the builder's `m_task` (+0x18) and sets the
  state to -2.
- Il2CppInterop mis-marshals `Nullable<Guid>` reads, and `Nullable<SaveEntry>` /
  `SaveEntry[]` arguments (the game crashed). The bot test builds those in IL2CPP memory
  with runtime field offsets.

Tested (automated, bot test inside the game, two bot accounts, 2026-10-19 07:01):

```
selftest done: 26 passed, 0 failed
```

Steps: both bots log in through the game's own `MoonBackendNew` (gatekeeper, token, queue);
A creates a character and a realm, lists both, loads the realm, saves realm + character
(SaveData), reloads both with the new bytes; A invites B, B sees the invite, accepts,
queries participation (Citizen, A as Owner), loads the shared realm; A and B PlayRealm,
A sees B playing; B leaves; A deletes the character.

Also observed in the real UI (manual runs, not tests):

- The main menu shows ONLINE. Character select lists the character stored on the local server.
- A character and realm created through the menus at 06:53 went: local save files first,
  `<steamid>_Character_<local guid>.cerimal` and `<steamid>_Realm_<local guid>.cerimal`
  under `DataStore\29466\` in the redirected profile folder, then `CreateCharacter` and
  `CreateRealm` (the server's ids differ from the local file ids), then
  `QueryRealmParticipation`. The game then loaded into the prologue (ship deck).
- That run's log has no `|E|` lines but two `|X|` lines:
  `FileNotFoundException: Could not find file ...\DataStore\29466\<steamid>_Character_<guid>.cerimal`
  (and the same for the Realm file), thrown in `Quantum.SaveFileIO.PerformBackupSequenceFor`
  while it backs up the previous save before the first write. The files exist afterwards, so
  this looks like the normal first-save path, not a backend problem. Derived, not verified.
- At 07:10, CONTINUE on that character sent `LoadRealm` (200) and loaded the realm.
- No `SaveData` from the game itself in about 35 minutes in the world (07:08 to 07:43), but
  the character stood still or sat in the settings menu most of that time, and the local
  `.cerimal` files were not rewritten either (last write 06:53). So this says nothing yet
  about autosave.

Server limits found while testing:

- `playing` (set by `PlayRealm`) is never cleared; the server has no "stopped playing" call
  yet. `ExitRealm` belongs to the server-plugin API and the client never calls it.
- `server-data/requests.log` is reset by the test script on every run, so older request
  logs are gone; the times above come from the session transcript and the game logs.

Unknown / not tested:

- Whether the game's own autosave reaches SaveData in a real session (only the bot called it).
- Co-op play itself: see the next section.
- The `wss` endpoint (unused so far), news, configs, telemetry.
- A second PC: the server binds 127.0.0.1 by default; it has a `--bind` option, untested.

## Co-op join on the local backend (2026-10-19, build 29466)

Question: can two clients logged in to the local backend play the same realm together?
Answer: no, not with this approach. Online realms run on a server-side plugin that only
Moon has. No live join was attempted (reason below).

Observed:

- Solo play on the local backend works: CONTINUE loaded character and realm from the local
  server (`LoadRealm` 200) and played them as a local Quantum session. No `PlayRealm` call.
  The plugin's guard logged two Photon connects in that run, both to Photon's name server
  (`useNameServer=True`, no fixed region, UDP, `authType` unset, no user id, no auth
  parameters, no post data, no token, appVersion 29466): region pinging, no login.
- So Photon is confirmed as the transport at runtime (`Photon.Realtime.RealtimeClient.ConnectUsingSettings`),
  not only by strings.
- The online session code talks to a server plugin, not to a host client:
  `SessionHandlerOnline` has `m_serverPluginAsTarget`, `SendMessageToServer`,
  `SendMessageToServerUnreliable` and `NotifyServerPluginOfFullyBoundView`.
  `SessionDefinition.IsHeadlessSession` and `SessionHandlerAPI.RequestHeadlessOnlineSession`
  exist as well.
- The backend contract for that plugin is in the client only as an interface:
  `IServerPluginBackendAPI { EnterRealm(Guid realmId, string[] promotionJwts);
  ExitRealm(string promotionJwt); ServerPluginSave(SaveEntry? realm, SaveEntry[] characters) }`,
  with `EnterRealmResult { ServerPluginJwt }`. A text search of the Cpp2IL dump (151
  assemblies) finds no class that implements it and no class named like a Quantum server
  or plugin (`DeterministicServer`, `QuantumPlugin`, `ServerPlugin`). A plugin with other
  names could still be missed.
- `PhotonServerType` has Local, Custom and Cloud, and `PhotonServerRegistry` can load
  server lists from CSV files. The game ships no such CSV.
- The game's Photon settings carry an AppId for Moon's own Photon app (`appIdRealtime` set,
  `appIdQuantum` empty, appVersion 29466). The AppId value is left out of these notes.

Derived:

- An online realm works like this: `PlayRealm` returns a `PlayJwt`; the client joins a room
  on Moon's Photon app; Moon's server plugin presents the players' JWTs to Moon's backend
  (`EnterRealm`) and then loads and saves the realm itself (`ServerPluginSave`). A JWT made
  by the local server, and a realm stored only on the local server, are unknown to Moon's
  backend, so this has to fail. The flow is read from type and method names, not from
  decompiled method bodies.
- Photon custom authentication is configured per app in the Photon dashboard, on Photon's
  side, so the client cannot point it at another URL. Source ("You can setup all
  authentication providers from your Photon Applications' Dashboard"):
  https://doc.photonengine.com/realtime/v5/connection-and-authentication/authentication/custom-authentication
- To get co-op on a private backend, someone would have to stand up their own Photon app
  (their own AppId) and write a replacement for Moon's server plugin from the message
  protocol in `SessionHandlerOnline`. That is a large project, not a redirect.

Guess (not checked):

- A Quantum server plugin like Moon's probably needs a Photon Enterprise or self-hosted
  licence. No pricing or licence page was read for this.

Not done:

- connect to Moon's Photon app with a token from the local server. The plugin now
  includes `PhotonGuard`: it logs every Photon connect (server, region, auth type, which
  auth fields are set) and refuses it when the auth data carries a JWT from the local
  server (`[Safety] BlockPhotonWithLocalJwt = true`, default).
- Reaching an online realm through the menus: that needs progress past the prologue on
  the test character. Not done, so the guard has not yet seen a `PlayRealm` connect.

Unknown:

- Whether the plugin's message protocol is small enough to reimplement.
- Whether the Local/Custom Photon server types still work in this build (no CSV ships).

## Related tools (not game-specific)

- Engine-agnostic Steam auth-ticket stub (native DLL hooking the `steam_api64.dll` flat
  API, `version.dll` proxy, playbook page, reference token server), built in a separate
  session from the NRFTW ticket bypass. Not merged and not in a PR yet; the build and its
  offline tests pass, but it was not run in any game (QA: none).

## Derived

- The game ships official online co-op ("Together", up to 4 players), so for players
  whose login works, triage ends at "official".
- A private backend gets solo online features
  (login, realm and save storage) but not co-op (see "Co-op join on the local backend").
- Photon is the network transport: confirmed at runtime (see above), no longer only strings.

## Unknown

- How to invite friends in the UI. The backend API has `Invite(realmId, inviteeAccountId)`
  and `GetAccountIdsFromSteamIds`, which suggests invites go to Steam friends via their
  Moon account ids; not seen in the UI.

Resolved since first written:

- The installed build has the co-op UI: the title screen says "Together" and the main menu
  has REALMS (test copy, 2026-10-19).

## Netcode seam for co-op without Moon's servers (2026-10-19, build 29466)

Question: can co-op run with no Moon backend and no Moon server plugin, with one player's
game acting as the server? Read from the Cpp2IL type dump only (field and method
signatures, no method bodies, no test runs).

Observed (type dump):

- Quantum 3. `Photon.Deterministic.Protocol.Join` is marked obsolete with "See the new
  start protocol in the Quantum 3 documentation".
- The deterministic session talks to the network only through `Photon.Deterministic.ICommunicator`:
  `ActorNumber`, `IsConnected`, `RoundTripTime`, `AddEventListener(OnEventReceived)`,
  `RaiseEvent(byte eventCode, byte[] message, int length, bool reliable, int[] toPlayers)`,
  `Service()`, `DisposeEventObject`, `OnDestroy`, `OnDestroyAsync`. Event codes
  (`MessageTypes`): 100 PROTOCOL, 101 SIMULATION, 102 INPUT, 103 INPUT_DELTA.
- Moon extends it: `IMoonQuantumServerCommunicator : ICommunicator` adds `IsFirstClient` and
  `SendMessageToServer(Message)`. `SessionHandlerOnline` implements `ICommunicator` on top
  of Photon Realtime (`RealtimeClient`) and sends to `m_serverPluginAsTarget`.
  `MoonQuantumGameCreateParameters.ServerCommunicator` is where the communicator is handed in.
- `QuantumSessionConfig(DeterministicGameMode gameMode = Local, ..., ICommunicator communicator, ...)`.
  `DeterministicGameMode`: Multiplayer 0, Local 1, Replay 2, Spectating 3.
- Quantum protocol messages (`Photon.Deterministic.Protocol`, the `Serializer` class packs
  them): `StartRequest {Id, ProtocolVersion, InitialTick, SessionConfig, RuntimeConfig}`,
  `SimulationStart {Reconnect, ServerTime, WaitingForSnapshot, RuntimeConfig, SessionConfig}`,
  `SimulationStop {FinalFrame}`, `AddPlayer {PlayerSlot, Data}`, `AddPlayerFailed`,
  `RemovePlayer`, `RemovePlayerFailed`, `Command {PlayerSlot, PredictedTick, Data}`,
  `ClockCorrect {ServerSeconds, ServerTimeScale}`, `RttUpdate {Rtt}`,
  `FrameSnapshot {Tick, TotalSize, Last, Data}` (49152-byte chunks),
  `FrameSnapshotRequest {ReferenceTick}`, `TickChecksum {Tick, Checksum}`, `TickChecksumError`,
  `Disconnect {Reason}`, `GameResult`, `RuntimeConfig`, `SessionConfig`.
  Input encoding classes are in the same DLL: `DeterministicTickInputSet`,
  `DeterministicTickInputEncodeHeader`, `DeterministicTickInputDecoder`, `DeltaCompressorDefault`.
- Moon's own server messages (`Quantum.Moon.Protocol`, `MessageSerializer`, 49152-byte max
  per message, 460800 per pack): `StartupInfoMessage {SessionId, RealmID, ServerId, AppId}`,
  `RequestMapChangeMessage {SpawnPointId}` / `MapChangeMessage : FrameMessage {ChangeDenied}`,
  `RequestResyncMessage {FrameNumber}` / `ResyncMessage : FrameMessage {Forced}`,
  `FrameMessage {FromPlayer, FrameNumber, TotalSize, Last, Data, RuntimeConfig}`,
  `RequestGameSaveDataMessage` / `GameSaveDataMessage : SegmentedMessage {SaveRequested}`,
  `PlayerDataMessage {RuntimePlayer}`, `BeforePlayerCharacterInsertedMessage {SpawnPosition, CharacterId}`,
  `DestroyPlayerMessage {ActorId}`, `ClientNotificationMessage` (SaveReady, LoadingHeartbeat,
  ViewReady), `ChecksumMessage`, `ChecksumErrorMessage`, `DesyncInfoMessage`,
  `RequestDesyncInfoMessage`, `PlayerAreaMessage`, `PlayerPingMessage`, `RequestPlayerPingMessage`,
  `PlayerKickedMessage`, `VoteKickMessage`, `EventActivityUpdateMessage`, `PDTokenMessage`,
  `DebugMessage`, `ServerDebugMessage`, `BugReportRequestMessage`.
- `SessionDefinition` has `Type` (Quickplay, PrivateRealm, SharedRealm, PrologueRealm,
  CinematicReplayRealm, Reconnection), `Multiplayer {RemoteIds, RoomName, Region, ...}`,
  `IsHeadlessSession`. `CreateQuickplay(..., bool online = false)` exists (dev launcher).
- `SessionViewBinding` holds one `QuantumLocalInputSource`. `QuantumGame.GetLocalPlayers()`
  and `GetLocalPlayerSlots()` exist (Quantum standard API).

Derived (from names and fields, not from method bodies):

- Moon's server plugin is authoritative for more than input ordering: it sends whole
  frames (`ResyncMessage`, `MapChangeMessage` carry frame data), decides map changes,
  asks clients for save data (`RequestGameSaveDataMessage`) and saves it to the backend.
  So it most likely runs its own copy of the simulation.
- `IsFirstClient` suggests the first client starts the simulation from its own state and the
  server takes it from there.
- A Moon-free design that fits this seam: a BepInEx plugin gives the game its own
  `IMoonQuantumServerCommunicator` (direct UDP or Steam networking instead of Photon), and
  the host's game also runs a small server that (1) answers the Quantum start protocol and
  relays inputs each tick, using the game's own input-set and serializer classes so no wire
  format has to be rewritten, (2) answers Moon's messages: resync and map-change frames taken
  from the host's verified frame, save requests written to the host's local saves, player
  data and character insertion for the guest. No Photon account, no Moon backend.
- Split screen is a worse route: the view side has one local input source, so camera, UI
  and input would all need doubling.

Unknown:

- Message bodies and ordering: needs ISIL or a decompiler on `SessionHandlerOnline`,
  `Session.CreateDeterministicSession`, `DeterministicNetwork` and the Moon message handlers.
- Whether `IsFirstClient` really means "the first client sends the initial snapshot".
- Whether the Multiplayer game mode works when the "server" is in the same process as one client.

## Research: existing co-op work (2026-10-19)

- `gh search repos "no rest for the wicked"` and `gh search repos nrftw`: tools, maps, save
  editor (https://github.com/suidpit/nrftw-save-editor), camera mod
  (https://github.com/ZishuNing/NoRestForTheWicked-ThirdPerson), localization kits. No co-op,
  multiplayer or private-server project.
- Web search for private server, offline co-op and LAN co-op mods: nothing; results are about
  the official offline mode and the Together update.
- Not checked: Nexus, Discord.

## Method bodies: session start and Quantum wire format (2026-10-19, build 29466)

Tools: IDA Pro 9.3 (already installed on the owner's PC) through idalib, headless, without
full auto-analysis: each target method is disassembled over its Cpp2IL address range and
decompiled with Hex-Rays (cache off). Calls are named from the Cpp2IL address map.
Metadata-usage slots (`qword_18C...`) are decoded from `global-metadata.dat` v31 (slot value
`v`: kind = `v >> 29`, index = `(v & 0x1FFFFFFE) >> 1`; kind 1/2 type via
`Il2CppMetadataRegistration.types`, 3 method, 5 string literal, 6 method spec). Metadata
registration at `0x18B0EBE90` (from the Cpp2IL log), typeDefinition record 88 bytes, method
record 36 bytes. Scripts: `ida_decomp.py`, `il2meta.py` (kept in the owner's local tools for now).

Observed (decompiled code):

- `Session.Tick` state machine: state 0, if online, constructs `SessionHandlerOnline(session)`,
  stores it at `Session+0x380` and goes to 1. State 1 calls `SetupConnectionUpdate()` every tick
  and waits until `RealtimeClient.State` (`client+0x7C`) is 9 (Joined). Then state 2 calls
  `CreateDeterministicSession()`, state 3 waits for the game to start and gives up after 15 s
  ("Initialization timeout during WaitingForGameStart").
- `CreateDeterministicSession`: game mode is Multiplayer when the byte at `Session+0x330` is set,
  else Local. The communicator is `Session+0x380` (the `SessionHandlerOnline`). Builds
  `QuantumSessionConfig` (frame data null, initial tick 0), the `DeterministicSession`, then calls
  `DeterministicSession.Join(id)`, where id is a Guid from the session or "LocalSession".
  RuntimeConfig must stay under 51200 bytes.
- `SessionHandlerOnline` is a thin transport. `RaiseEvent` and `SendMessageToServer` both go to
  `RealtimeClient.OpRaiseEvent` (vtable slot +0x2B8). Server messages target the actor list
  `m_serverPluginAsTarget`. `OnEvent` switches on the Photon event code: 2, 3, 4, 5 (frame dump
  link), 6 (server log line, printed as "[SERVER ...]"), 7 (chat), 8 (BackendSaveResult),
  13 (SaveCycleFinishResponse); everything else goes to `HandleQuantumEvent`, which passes the
  `ByteArraySlice` payload to the `OnEventReceived` listener (`m_onEventReceived`, +0xD0).
- `DeterministicNetwork`: accepts event codes 100..103 only. 100 PROTOCOL and 101 SIMULATION are
  `Serializer.PackMessages` batches (100 sent reliable, 101 unreliable). 102 INPUT from the server
  is a full input set; 103 INPUT_DELTA is `uint32 tick, uint16 size` plus a delta against the
  previous raw buffer, then decoded like 102. The client sends its own inputs as event 102,
  unreliable, with redundancy.
- `DeterministicTickInputDecoder.Decode` (server to client, event 102):
  `DeterministicTickInputEncodeHeader.Serialize` (MaxPing, PlayerCount, InputFixedSize,
  ServerTime, ServerTimeScale), then while at least 32 bits remain: byte align, `uint32 tick`,
  1 bit flag, the player bit mask (`PlayerCount` bits; two extra uint32 above 64 players), then
  `DeterministicTickInput.SerializeForDecoder(serializer, stream, header)` for each player in the
  mask, then each player's RPC bytes (skipped when Flags == 3 with no data and no RPC). All
  these `Serialize` methods read or write depending on the stream mode, so the same game
  classes can encode on a server.
- `DeterministicSession` has `OnProtocolFrameSnapshotRequest` and `SendSnapshot(referenceTick)`:
  a client can supply the snapshot for a late joiner, as in stock Quantum 3.
- Moon's own messages (`Quantum.Moon.Protocol`, `MessageSerializer`): no direct caller of
  `MessageSerializer.ReadNext` or `MoonQuantumGame.set_ServerId` in the client (direct-call
  search over the ISIL dump; misses virtual calls). `MoonQuantumGame.HandleResync` is only
  called from `LockstepAPI.ResyncToFrame`.
  `DeterministicInputFlags`: Repeatable 1, PlayerNotPresent 2, ReplacedByServer 4, Command 8.

Derived:

- The client side of an online session is stock Quantum 3 plus a handful of Moon events (log,
  chat, save results). The Moon message set is most likely used inside Moon's server plugin
  (it runs the same `moon.quantum.forsaken` code), not on the wire to the client. So a
  replacement server needs the Quantum 3 start and input relay, snapshot relay, and answers to
  the save-cycle messages, not the whole Moon message set.
- Spike plan: force a solo realm session into online mode (byte `Session+0x330`), skip
  `SetupConnectionUpdate`, mark the Photon client Joined, catch every send at
  `RealtimeClient.OpRaiseEvent`, and feed replies to the stored `OnEventReceived` listener from
  an in-process server.

Unknown:

- How the realm save reaches the simulation in online mode (initial frame is tick 0 with no
  frame data; possibly through RuntimeConfig, possibly from the server).
- Format of the client's own input packets (event 102 upstream); `DeterministicTickInput.DeserializeMultiple`
  is the likely server-side reader.

## Loopback server spike: a SharedRealm session without Photon or Moon's plugin (2026-10-19, build 29466)

Goal: one client runs the game's real online session (Quantum Multiplayer mode) against a
server that lives inside the same process. No network connection is opened: the Photon
connect step is skipped and every send is caught before Photon. Test copy only, local backend.

Result (observed in the test copy, screenshots and trace, not yet an automated assertion):
the character spawned on the prologue beach with the HUD up, and holding D moved it. The
session ran over 2 minutes (more than 8,000 ticks) without errors, client inputs reached the
server in time (1 tick replaced, the join tick).

How the session is reached (bot, no menu clicks): a realm only takes the shared path when it
is not solo (`SaveDataManager.Realm.IsSoloRealm`: false once another account participates).
The local backend realm got a second participant (a bot account as citizen). The bot hooks
`SessionHandler.Tick(ViewFrame)` and calls
`vf.ClientFlow.Item2.RequestSessionJoin(new CharacterIndex(character), new RealmIndex(realm), false, false)`.
The game then uploads saves, deletes its local realm copy (`DeleteLocalRealmSave`, the test
profile's realm file is gone, backed up first), calls `LoadRealm` and `PlayRealm`, builds
`SessionDefinition.CreateSharedRealm` and starts a SharedRealm session.

Server protocol as implemented and confirmed working (event codes on `ICommunicator`):

1. Client `StartRequest` (event 100). Reply `SimulationStart {ServerTime 0, WaitingForSnapshot false,
   RuntimeConfig = the client's, SessionConfig = the client's with InputDeltaCompression false}`.
   With delta compression on, the client expects event 103 frames; full 102 sets then hit
   "RawInputCache unexpected frame N request. Cache is empty".
2. Hold confirmed ticks until the client's `AddPlayer` arrives. Confirming ticks with no player
   killed the process.
3. Join: the player's RPC (no Command flag) = `int32 actor, int32 slot, AddPlayer.Data`
   (`Frame.UpdatePlayerData` → `DeterministicSession.OnPlayerAdded(frame, slot, actor, player)`,
   local when actor == `ICommunicator.ActorNumber`). The client only reads player RPCs from
   about frame 120 on (first `UpdatePlayerData` call was frame 120), so the join goes on tick 130.
4. Confirmed input stream (event 102): see the decoder layout above; per player
   `1 bit empty, 1 bit has-data, InputFixedSize bytes, 4 bits flags`, then RPCs.
   Absent players: Flags 3 (Repeatable | PlayerNotPresent), empty.
5. Client inputs (event 102 upstream, unreliable, InputRedundancy 3): records of
   `u8 player, u32 tick, InputFixedSize bytes (47 here), RPC (BitStream byte[] serialize, 1 bit when null), u8 flags`.
6. Tick pacing: confirm tick T when every present player's input for T is in, or 250 ms late;
   header `ServerTime` = T / UpdateFPS (the newest confirmed tick, not the wall clock).
7. Client `Command {PlayerSlot, PredictedTick, Data}` (event 100): deliver as that player's RPC
   with Flags |= Command (8), one per tick. Without this the world never finished loading
   (stuck on the loading screen; the first commands are 42 and 114 bytes).
8. The client also sends Moon events to the "server plugin": event 0 every 30 ticks
   (`u32 tick, u64 checksum`), event 9 once (12 bytes). Not answered yet; nothing broke.

Session config the client sent: 60 fps, 4 players, InputFixedSize 47, rollback 120,
InputRedundancy 3, InputDelayMin 0 / Max 7, TimeCorrectionRate 4.

Pitfalls (each cost a crash, with the evidence):

- Il2CppInterop binding bugs in this build: `new DeterministicTickInput()` invoked the wrong
  native method (it re-entered `ICommunicator.Service`); `SessionHandlerOnline.m_onEventReceived`
  read back through the interop property returned a garbage object after the join (delegate
  `method_ptr` = 1). Capture the delegate in an `AddEventListener` postfix instead. A Harmony
  postfix on the `SessionHandlerOnline(Session)` constructor never ran.
- Passing the `DeterministicTickInputEncodeHeader` struct by value into
  `DeterministicTickInput.SerializeForDecoder` through interop did not work. The encoder is
  written in C# from the decompiled bit layout.
- `ICommunicator.Service` is called on the main thread too; only pump on the Quantum thread.
- Silent process exits were coreclr fail-fasts (`EEPolicy::HandleFatalError`, event log
  ".NET Runtime ... internal error"): a .NET exception escaping a native-to-managed hook. A
  minidump reader (exception, module, heuristic stack scan named from the Cpp2IL map) plus
  UTF-16 strings from the crashing thread's stack pointed at it. Every managed callback the game
  can reach must catch everything.
- A probe that logs every IL2CPP throw: native detour on `il2cpp_codegen_raise_exception`
  (GameAssembly RVA 0x90FAA0 in this build). Normal background throws: "Not supported as
  contextless director" (several director types), and `SocketException: Connection refused` from
  `TcpClient.Connect` on another thread (target unknown).

Not done / unknown:

- A second client. Next: two game copies on one PC, the server in the host, UDP between them.
- Save cycle: the client never sent save chunks in this run (`ProcessSaveCycleQueue`,
  `SendCustomMessages`); events 8 and 13 back are unanswered.
- Moon events 0 and 9 (checksum reports) are ignored; desync detection is off.
- Late join (snapshot request to an existing client) is not implemented.

## Two-player co-op on one PC (2026-10-19, build 29466)

Setup: two copies of the install (`nrftw-test`, `nrftw-test2`, asset folders as junctions),
each with its own account on the local backend (configurable placeholder ticket), its own
profile folder and its own plugin config. Host copy: `[Coop] Role = Host` runs the room and
listens on UDP 5810. Guest copy: `Role = Guest`, actor 2. Both bots join the same shared realm.

Guest's character: a copy of the host's test character on the local backend. The character
metadata names its owning account (16-byte .NET GUID, once), checked on load ("Remote
character does not reference the owning account"). Cerimal documents carry an XxHash64
(seed 0) of bytes 28..end at offset 20 ("Document's content does not match its checksum").
Swapping the GUID and re-hashing made it load. Both characters keep the same local character id.

Late join, as implemented and working: the guest's `StartRequest` gets `SimulationStart` with
`WaitingForSnapshot = true` ("Waiting for snapshot. Clock paused.") and the host's session and
runtime config; the room sends `FrameSnapshotRequest {ReferenceTick}` to the host's client, which
answers with `FrameSnapshot` chunks of its verified frame (`SendSnapshot` serializes
`DeterministicSimulator._stateVerified`; about 230 KB, 5 chunks of 49152 bytes); the room
forwards them and streams confirmed inputs from snapshot tick + 1. Then the guest's `AddPlayer`
becomes a join RPC with actor 2. Upstream input records and `Command.PlayerSlot` carry the
client's local slot; the room maps (actor, slot) to the player index.

Tick pacing that works: the room ticks on its own clock (tick T due at T/fps), confirms T once
every present player's input is in, and at the latest 100 ms after it is due; clients report
100 ms RTT so they run about 3 ticks ahead and their inputs arrive in time (0.0 % replaced).
Two rules that failed: clock only with 1 ms RTT (every input replaced, key presses lost) and
waiting for inputs without a clock (deadlock: the client paces itself from `ServerTime`).
With the waiting rule the checksums also diverged after the guest joined (tick 870); not seen
with the clock rule. Root cause not investigated.

Automated test (`coop-test.sh` + `tools/coop_check.py`, owner's local tooling): 15 checks.

```
coop test: 15 passed, 0 failed
```

Checks: both players' AddPlayer and joins, snapshot forwarded, OnPlayerAdded for actor 2 on both
clients, checksums (Moon event 0, both clients, every 30 ticks) equal on 234 common ticks and on
213 ticks after both players moved, under 5 % inputs replaced (0.0 %), each player's camera moved
over 1 m while a key was held in its window (host 2.05 m, guest 7.16 m), both processes alive.
Runs: 15/15, 14/15 (guest camera 0.37 m: the key press probably did not reach the guest window;
checksums and inputs were fine), 15/15. Screenshots show both characters side by side in both
windows and a second party portrait in each HUD.

Not done: save cycle, leaving and rejoining, desync recovery (checksums are logged, not acted
on), retransmission on UDP (loopback only so far), more than one guest, two PCs.
