# Middle-earth: Shadow of Mordor (2014)

| | |
|---|---|
| Engine | Firebird, Monolith's LithTech-derived engine (the exe's module name is `Firebird_C.exe`) |
| Versions tested | Steam app 241930, build 7348332, `x64\ShadowOfMordor.exe` 1.0.1951.29, SHA-256 `f5d72578f7e5fcf05e6d15d8bd231f763078620c8545f8464bb186e842279455` |
| Projects | none released; [JustAddMP/ShadowOfMordor](https://github.com/JustAddMP/ShadowOfMordor) is a placeholder; research here (claim: cranebeaker/coop-anything#418) |
| QA level | none: no shared play yet; prototype checks ran with one and two game copies on one PC, with an offline guard suite |
| Last updated | 2026-11-21 |

## Where things stand

Nobody can play Shadow of Mordor together today. This folder holds a from-scratch start, built and
tested on one PC:
- a safe way to run an instrumented test copy;
- a map of how the engine splits client and server;
- the engine's object-spawn API;
- a prototype that shows the other player as a **second Talion** in your world, moved by
  position packets from another game copy over loopback UDP.

The second Talion slides and stays in his rest pose (A-pose): animation is not synced yet, and
nothing goes over the internet yet. What is known: his animation component is never updated,
and giving him a client controller does not change that (see [Next steps](#next-steps)). Raw
notes with every run: [NOTES.md](NOTES.md).

## Existing work

- [JustAddMP/ShadowOfMordor](https://github.com/JustAddMP/ShadowOfMordor): README only,
  "planned" (created 2026-10-17, no code as of 2026-11-19).
- No co-op or multiplayer mod found (web search, GitHub repository search, Steam forum threads
  via search results; 2026-11-19). The game shipped single-player only; its online Vendetta
  features ended in 2021 ([Worthplaying](https://worthplaying.com/article/2021/1/16/news/123693)).
- The sister game [Shadow of War](../shadow-of-war/README.md) (same engine family) has an alpha
  co-op mod. None of its function signatures match this build (0 of 9), but its 64-bit LCG
  constants do, so design findings carry over and addresses do not.

## How the game works (for co-op)

Everything below is for build 7348332. RVAs are relative to image base `0x140000000`. Details
and the reasoning are in [NOTES.md](NOTES.md).

- **No DRM wrapper.** No SteamStub `.bind` section and no Denuvo-style section; plain compiler
  output, so static analysis works on the file directly. A `bink2w64.dll` proxy loads into the
  game (17 imports from it).
- **Saves are cloud-only.** The only save, `GameData.sav` (1 MiB), goes through
  `ISteamRemoteStorage` (`FileWrite`, `FileRead`, `FileDelete`, `GetFileSize`). A test copy has
  to serve those calls from a sandbox; redirecting file paths is not enough. Settings go through
  `SHGetFolderPathA` (Documents).
- **One process runs a local server and a client.** `CGameServerShell` (vtable `exe+1462960`,
  12 slots) exists once a world is loaded, next to the client shell and `ClientMgr`. The engine
  object steps both in the same frame. They talk through **two in-memory message queues**:
  client→server with a client-id byte, server→client without one. There is no transport between
  them.
- **Client proxies read server memory directly.** One engine object record links both sides:
  - `+0x38` server object, `+0x48` its class;
  - `+0x40` client proxy, `+0x50` its class;
  - `+0x08` transform.

  `CCharacterProxy` reads its server `CCharacter`'s fields each frame. So a "remote client" of
  one game is not possible: **each player runs a full game, and the other player is shown as a
  puppet.**
- **Players.** Every `CPlayerObj` adds itself to a growable list at `RVA 1988A90` in its
  constructor. 158 functions only ever use the first entry (`c47c10`); radius queries loop over
  all players. The client id at `+0xB68` is always 0 in this build.
- **Object API** (`*RVA 19D80D0`):
  - `+0x70` get class by name, `+0x48` create from a 0x198-byte `CObjectCreateParams`,
    `+0x50` remove;
  - `+0x98` / `+0x90` / `+0xB0` set position / rotation / both.

  The block layout (flags, transform, spawn data, class, name, required object set) is in the
  notes. Talion is spawned by `50aa00`; NPCs by AI spawners through `b03640`.
- **NPC records.** An NPC comes from an `AI.Template` record:
  - its CharacterModel link → an 88-field CharacterModel record;
  - which links → a 65-field character record (`npc+0x2A8`) holding behaviour: `AIGoalSet`
    (required by the NPC set-up `ab1fa0`), `AIActionSet`, `TargetSelectSet`, `AIMovementSet`,
    `SensorSet`.

  Swapping these for Talion's records changes behaviour, **not** the look. An orc's appearance
  comes from elsewhere (probably the Nemesis appearance system).
- **The game's own brand** (`npc->vtable[0x170]`, `bf2820`) makes a spawned NPC passive toward
  Talion.
- **Animation events are messages.** The engine's in-process message queue (`e55000` →
  `e53fd0`) delivers each record to the server object (`object+0x38`) or the client proxy
  (`object+0x40`) through `vt+0x68`. For the player these are the keys a playing animation fires
  (`HeelStrike`, `ToeStrike`, `snd`, `ComplexAnim`, …), about 11 a second each way. They are the
  animation's output, not what drives it.
- **A second `CPlayerObj` does not animate.** Created with the object API, its animation component
  (`player+0xB60`) is never updated: 0 of 0x4000 bytes changed in 30 s, against 96 fields on the
  real one. What updates the real one is not found yet.
- **Client controllers.** `3f0080` builds the 0xDA0-byte client player controller for a
  local-player slot (0–3) from one of 6 preallocated local-player entries; the proxy keeps it at
  `+0x118`. The client game mode reads a camera for every controller in the slot table
  (`50ce20`) and passes the table's count to the renderer as a view count.
- **Single-instance lock.** Engine main (`e043e0`) creates
  `Global\{43570706-DA86-477B-9C2D-2F3D4F95FDE4}` and, if it exists, focuses the first window and
  exits with **code 0** (skipped only under a developer option, `RunningUnderToolControl`).

## What we changed

All of it is a test-copy guard (a `bink2w64.dll` proxy), not released. Each feature is off unless
its switch is set.

- **Isolation.** Refuses to run outside the lab folder, on another exe build or without
  `steam_appid.txt`. It:
  - serves the Steam cloud save from a sandbox;
  - redirects Documents;
  - limits the game's sockets to loopback;
  - hooks `NtCreateFile`/`NtOpenFile`/`NtDeleteFile`/`NtSetInformationFile` and ends the process
    on any write to the real install, the real saves or the backups.
- **A second Talion as the remote player.**
  1. On the first NPC spawn, create a second `CPlayerObj` with the object API's create, filling
     the block the way `50aa00` does.
  2. Skip `50aa00`'s client, controller and camera binding.
  3. The second Talion shows Talion's real model, and the owner keeps control of their own.
- **Network link.** Loopback UDP on the game thread (server-shell update slot 9 wrapped):
  - send the local Talion's transform 20 times a second;
  - place the puppet at the newest received transform every frame;
  - 36-byte packets; malformed, old, non-finite, too-far, non-unit-rotation and all-zero packets
    are dropped.
- **Two copies on one PC.**
  - A profile (`SOMGUARD_PROFILE=B`) gives the second copy its own sandbox and swapped ports.
  - The guard renames the single-instance mutex for that profile only.
  - Steam accepted the second copy launched this way.
- **A client controller for the second Talion** (`ctrlslot`). Built in local-player slot 1 with
  `3f0080`, linked at `proxy+0x118`, then taken back out of the slot table so the camera pass and
  the renderer never see it. Some controller-driven calls then run for the second Talion, but he
  still does not animate.
- **Diagnostics** (logging only): per-object call counting on vtables with the caller of each call
  (`animprobe`), dumps of the local-player and controller state (`ctrlinfo`), a message logger (`msglog`), a field diff of both
  Talions' animation components (`compdiff`) and a crash logger with unwound stacks (`crashlog`).
- Also prototyped and logged in the notes (not part of the current path):
  - an NPC puppet: spawn, brand, per-frame placement;
  - NPC walking through `GotoNode` with a runtime `AINodeGoto` (no effect);
  - hand-built Attract stimuli (accepted, but did not move the puppet);
  - record swaps for the look (NPC accepted, still an orc).

## QA

- Offline guard suite: 44 cases plus a negative control (the guard built with protection
  compiled out must fail all 14 protection cases). Last run 2026-11-21: **44 passed, 0 failed;
  negative control 14 of 14.** It covers:
  - protected writes, including path variants;
  - cloud-save sandboxing and network limits;
  - DLL forwarders;
  - each hook installer (refuses a mismatched target, passes calls through);
  - packet validation;
  - record reading, cloning and in-place swaps;
  - the profile sandbox and the mutex rename;
  - the call-counting stubs (int, float and stack arguments pass through; counts, callers and
    restore are right), the slot controller's refusals, the crash logger and the field diff.
- Game runs (Steam build 7348332, one PC, launched by a script that checks the real save and
  settings against hashed backups before and after every run; 0 changes in every run):
  - **one copy and a loopback bot** sending a circle: 1,809 packets received, 0 rejected, the
    puppet placed every frame and following Talion;
  - **two copies**: each placed its puppet exactly at the other copy's Talion position, packet
    age 15–63 ms;
  - **second Talion**: created (`players=2`, the real one first), moved by the bot, and reported
    visible as Talion in the A-pose;
  - **diagnostic runs 33–43**: call counts, controller dumps, a slot-1 controller, component
    diffs, message-handler counts and message contents, as listed in the notes. Every probe restored its vtable
    slots with 0 mismatches.
- These are prototype checks, observed in logs and owner reports. There is no automated
  assertion suite for in-game state yet, so the QA level stays `none`.

## How to play

Not playable yet.

## Known issues

- The remote Talion slides along and stands in the A-pose: no animation sync.
- Nothing goes over the internet yet: loopback on one PC only.
- No enemies, combat, story or Nemesis state is shared.
- With the slot-1 controller attached, the game crashes when it quits (a reference-count assert
  in the exit-time destructors, `exe+D59E56`). Play itself is not affected, and without the
  controller the game exits normally.

## Next steps

- **Animation sync:** find the code that updates the real Talion's animation component each
  frame (for example from the call stack where its animation event messages are produced), then
  run it for the second Talion and feed it the friend's animation state. Ruled out so far: the
  animation component set-up alone (`79f950`, `784dc0`), a client controller alone, `50aa00`'s
  two post-spawn engine calls, and replaying the event messages (they are the animation's
  output).
- **Internet transport:** the guard currently allows only loopback. The Steam P2P/lobby code in
  `INetOnlineService` (vtable `exe+148F050`) is a candidate.
- **Shared enemies and Nemesis:** host-authoritative spawning through `b03640`; the guest mirrors
  the host's NPCs.
