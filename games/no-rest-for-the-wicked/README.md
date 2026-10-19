# No Rest for the Wicked (2024, Early Access)

| | |
|---|---|
| Engine | Unity 6000.1.15, IL2CPP; Photon Quantum 3 (deterministic lockstep) over Photon Realtime |
| Versions tested | Steam appid 1371980, buildid 22928553 (game build 29466), Windows. `NoRestForTheWicked.exe` SHA-256 `FE6EC10D47D183C872ED575114EBCA29F6EBD99B47E0AE66C6316A62A6044CF6` |
| Projects | Official "Together" update (online co-op, up to 4 players). No community co-op project found. |
| QA level | automated: two game copies on one PC, 15-check co-op test (join, snapshot, movement, checksums); not tested across two PCs |
| Last updated | 2026-10-19 |

## Where things stand

The game has official online co-op since the "Together" update (PC, 2026-01-22): up to
four players share persistent Realms. It needs Moon Studios' backend and Moon's Photon
server plugin. When a player's client cannot log in to that backend, co-op is gone
entirely, and nothing on the player's side can stand in for Moon's servers today.

Research so far covers how the online stack works, a local stand-in for the REST backend
(solo login, realm and save storage), and a spike that runs the game's real online session
(a SharedRealm, Quantum Multiplayer mode) against a small server inside the same game
process, now with a second game joining over UDP: two players in the same realm, both moving,
both games in sync (automated two-copy test on one PC, 15 checks). No Photon and no Moon server
involved. Not yet tested across two PCs; saves during co-op and rejoining are not handled.

## Existing work

- Official "Together" update: [Gematsu](https://www.gematsu.com/2025/12/no-rest-for-the-wicked-early-access-together-update-launches-january-22-2026-open-beta-test-now-available),
  [RPG Site](https://www.rpgsite.net/news/19232-no-rest-for-the-wicked-multiplayer-co-op-update-release-date-gameplay),
  [MP1st](https://mp1st.com/news/no-rest-for-the-wicked-together-co-op-update-out-next-week).
- Official forum thread on clients that stay offline, with player workarounds:
  https://forum.norestforthewicked.com/t/ongoing-permanently-offline/22173
- No community co-op mod or private server was found (GitHub repo search for "no rest for
  the wicked" and "nrftw", web search, 2026-10-19; Nexus and Discord not checked). Other
  community mods exist, for example a third-person camera mod and a save editor:
  https://github.com/ZishuNing/NoRestForTheWicked-ThirdPerson,
  https://github.com/suidpit/nrftw-save-editor.

## How the game works (for co-op)

Read from a Cpp2IL type dump (signatures only) and test runs on a copy of the install.
Details, with what was observed versus derived, are in [NOTES.md](NOTES.md).

- **Modding:** BepInEx 6 IL2CPP bleeding-edge build be.788 works on Unity 6000.1.15.
  BepInEx 6.0.0-pre.2 does not (`Unsupported metadata version found! We support 23-29, got 31`).
  IL2CPP inlines some small methods into their callers, so some Harmony patches never fire;
  native detours on the caller or its async state machine work.
- **Simulation:** the whole game runs as a Photon Quantum 3 deterministic simulation, also
  in single player (`DeterministicGameMode.Local`).
- **Online:** `SessionHandlerOnline` implements Quantum's `ICommunicator` over Photon Realtime.
  Inputs go through Quantum's standard protocol (event codes 100 to 103). On top of that, Moon's
  own message set (`Quantum.Moon.Protocol`, about 30 messages) talks to a server plugin
  that resyncs whole frames, decides map changes and collects save data. The plugin is not
  in the client. Photon login uses custom authentication configured on Moon's Photon app.
- **Backend:** REST login chain gatekeeper, `/v3/token`, queue `/join`, then
  `POST {main}/save/<accountId>/<Operation>` for realms and characters. A local stand-in
  answers all of it for solo play.
- **Saves:** Cerimal files in `DataStore\<build>\` under the game's persistent data path,
  mirrored to the backend.

## What we changed

Nothing a player can use yet. Research tooling, kept outside this repo:

- Loopback server spike (BepInEx plugin): skips the Photon connect, catches
  `SessionHandlerOnline.RaiseEvent`, and answers as a Quantum 3 server: `SimulationStart`,
  the join RPC, confirmed input sets, client commands. Protocol details and the pitfalls hit
  on the way are in [NOTES.md](NOTES.md) ("Loopback server spike").

- A BepInEx plugin that redirects the backend hosts to a local Python server and hands the
  game a placeholder Steam ticket. Purpose: study the login and save protocol without Moon's
  servers. It refuses any Photon connect that carries a token made by the local server, so
  it never reaches Moon's Photon app with forged credentials.

## QA

- Local backend bot test (two bot accounts inside one game client, through the game's own
  `MoonBackendNew`): `selftest done: 26 passed, 0 failed` (2026-10-19). Covers login, character
  and realm create, list, load, save, invite, accept, participation, play and leave. It does
  not cover co-op play.

## How to play

Official co-op: Realms menu in the game. Private co-op (this project, experimental): BepInEx 6
be.788 plus two plugins and a small Python server run by the host; the package and its
how-to are not published yet (owner's decision).

## Known issues

- Some clients never log in and stay offline (`GetAuthSessionTicket returned Invalid handle`,
  then `Failed to connect to backend using Steam.` in `Player.log`). Without login there is no
  co-op. See the forum thread above for player workarounds.

## Next steps

Moon-free co-op, host's game as the server. Order:

1. Done (spike): one client in a SharedRealm session against an in-process server, joined
   by a bot through `ClientFlowAPI.RequestSessionJoin`, spawned and moved.
2. Done: two game copies on one PC, guest over UDP, late join via snapshot, automated test.
3. Two PCs; UDP retransmission (or TCP) for the internet.
4. Save cycle (client save chunks, events 8 and 13), desync recovery from the logged checksums,
   leave and rejoin, three and four players.
