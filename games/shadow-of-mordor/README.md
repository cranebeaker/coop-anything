# Middle-earth: Shadow of Mordor (2014)

| | |
|---|---|
| Engine | Firebird, Monolith's LithTech-derived engine (the exe's module name is `Firebird_C.exe`) |
| Versions tested | Steam app 241930, build 7348332, `x64\ShadowOfMordor.exe` 1.0.1951.29, SHA-256 `f5d72578f7e5fcf05e6d15d8bd231f763078620c8545f8464bb186e842279455` |
| Projects | [danudns/shadow-of-mordor-coop](https://github.com/danudns/shadow-of-mordor-coop) (MIT, [v0.1.0](https://github.com/danudns/shadow-of-mordor-coop/releases/tag/v0.1.0), built here; claim: cranebeaker/coop-anything#418); [JustAddMP/ShadowOfMordor](https://github.com/JustAddMP/ShadowOfMordor) is a placeholder |
| QA level | none: lab runs (two game copies on one PC) and sessions between the author and one friend over the internet, plus a 70-case offline suite; no automated in-game suite, no reports from other players |
| Last updated | 2026-11-23 |

## Where things stand

Two players can play together over the internet in an early, experimental mod:
[shadow-of-mordor-coop v0.1.0](https://github.com/danudns/shadow-of-mordor-coop). Each player runs
their own game; the friend joins through a Steam invite and plays in a copy of the host's save.

- **Shared:** each player sees the other as a **second Talion**, moving smoothly and animated by the
  game's own animation system (idle, walk, run, turns, attacks, executions, sword draw and sheathe,
  stealth). The host's roaming enemies are shown in the friend's world (the friend's game spawns
  none of its own); when the host kills one, it disappears for the friend.
- **Not shared yet:** hits that do not kill, orcs reacting to the friend, death falls, captains and
  Nemesis, missions and progression. The friend's kills counting for the host and joining next to
  the host are in v0.1.0 but not yet seen working in a session.

Raw notes with every run: [NOTES.md](NOTES.md). The mod's
[how-it-works](https://github.com/danudns/shadow-of-mordor-coop/blob/main/docs/how-it-works.md)
lists its messages and the game functions it calls.

## Existing work

- [danudns/shadow-of-mordor-coop](https://github.com/danudns/shadow-of-mordor-coop): the mod
  built from this research (MIT, v0.1.0 on 2026-11-23, source, offline tests, installer).
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
- **Client controllers.** `3f0080` builds the 0xDA0-byte client player controller for a
  local-player slot (0–3) from one of 6 preallocated local-player entries; the proxy keeps it at
  `+0x118`. The client game mode reads a camera for every controller in the slot table
  (`50ce20`) and passes the table's count to the renderer as a view count.
- **What animates a player.** Each frame `3f0380` runs the controller update `4009a0` for every
  controller in the slot table. That update calls `81b950(component+0x3968)` and
  `428970(sub)` → `7a0940(component)`, the animation component update; `sub` is the controller's
  sub-component `+0x2D8`. The pose is built only if `component+0x8D8` holds a pose output, which
  the sub-component's bind `42ead0` sets. A second `CPlayerObj` created with the object API gets
  none of this until it has a bound controller and someone runs the update for it.
- **Locomotion inputs are named graph parameters.** `785bc0(component, name)` → handle,
  `77b1c0(component, handle, value, 1)` sets and `77b250` reads. The player graph has
  `RawMoveMag` (stick 0..1; keyboard gives 0 or 1), `Game_SmoothedVelocity` (units/s: run about
  226, sprint about 607), `DeltaYaw`, `CameraDeltaYaw`, `CameraYawMinusPlayerYaw` and
  `LastSpike` (radians for the angles). It has no `Game_Velocity`.
- **Combat moves are named requests.** Attacks, counters, finishers and stealth kills go through
  the combat model `457120` → `79f150(component, &request, 0)`; `77af10(&req, name)` builds a
  request and `7a16b0(component)` ends the previous combat move. Clips such as the sheathe, sprint
  and jumps go through `79f9b0` ("play by action"). Graph switches (`InCombat`, `Sheathed`,
  `Stealth`, `Daggers`, …) are set through `785bb0` → `785a70`.
- **Enemies.** `aae540` registers every NPC in a table at `exe+19835C0` (0x30-byte entries,
  pooled objects, so ids need a generation). Roaming population spawns go through one call at
  `exe+AE2B53`; skipping it is treated as a failed spawn. An NPC's animation component is
  `*(npc+0xC38)`, same class as the player's; its locomotion is clip-driven.
- **Steam.** The game's `steam_api64.dll` offers `SteamNetworking005` and `SteamMatchMaking009`,
  which the exe itself does not use for networking; lobbies, invites and P2P work from a mod.
- **Single-instance lock.** Engine main (`e043e0`) creates
  `Global\{43570706-DA86-477B-9C2D-2F3D4F95FDE4}` and, if it exists, focuses the first window and
  exits with **code 0** (skipped only under a developer option, `RunningUnderToolControl`).

## What we changed

Everything is one `bink2w64.dll` proxy. It started as a test-copy guard (below); the same DLL in
"mod mode" is the released mod. Each feature is off unless its switch in `somguard.ini` is set.

**In the released mod (v0.1.0)**, on top of the second Talion and animation work below:

- **Saves.** Alone or hosting: the real save is used as without the mod, with a dated backup at
  every start (newest 10). Invited: only the cloud save is redirected, to a co-op slot that
  receives the host's save over Steam (size and SHA-256 checked). Settings and language are never
  touched.
- **Steam link.** Friends-only lobby, invite through the overlay, peer-to-peer with relay;
  positions at 30/s unreliable, everything else reliable. A restarted partner and failed lobby
  enters are recovered.
- **Combat moves and states.** The partner's named animation requests are replayed on their
  Talion; paired moves end after 3.5 s or when the partner moves. Sword, graph switches and clips
  (sheathe, sprint, jumps, ledges, climbing) are sent too; clips are replayed from a kept copy of
  the action data, checked by asset name.
- **Enemies, host-authoritative.** The friend's game skips its own population spawns and keeps
  their arguments per template. The host streams the 40 nearest NPCs within 60 m of either player
  (type, position, facing, movement inputs, dead flag) and their named animations; the friend's
  game spawns puppets of the same template, senses off, and removes them when they die or leave.
  A puppet the friend kills is reported to the host, which kills the real NPC with the game's own
  damage record (untested in a session).
- **Session check.** Every 5 s both games compare where they show both Talions and the enemies,
  and log the differences in metres.

**Test-copy guard** (lab only):

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
  - send the local Talion's transform and his own graph inputs (`RawMoveMag`, `DeltaYaw`,
    `Game_SmoothedVelocity`) 20 times a second, in 48-byte packets (36-byte position-only packets
    are still accepted);
  - place the puppet at the newest received transform every frame;
  - drop packets that are malformed, old, non-finite, too far, all-zero, carry a non-unit
    rotation, or carry graph inputs out of range.
- **Two copies on one PC.**
  - A profile (`SOMGUARD_PROFILE=B`) gives the second copy its own sandbox and swapped ports.
  - The guard renames the single-instance mutex for that profile only.
  - Steam accepted the second copy launched this way.
- **Animating the second Talion**, after the animation set-up (`79f950`, `784dc0`):
  1. `ctrlslot`: a client controller in local-player slot 1 (`3f0080`), linked at
     `proxy+0x118`, then taken back out of the slot table so the camera pass and the renderer
     never see it;
  2. `ctrlbind`: the controller sub-component's bind `42ead0`, called once;
  3. `animtick`: `81b950` and `7a0940` on his component every frame;
  4. `animparams`: his locomotion inputs set by name every frame, from the other player's packets
     or, without them, from his own speed (`RawMoveMag` = speed / 230).
- **Diagnostics** (logging only):
  - per-object call counting on vtables, with the caller of each call (`animprobe`);
  - dumps of the local-player and controller state (`ctrlinfo`);
  - a message logger (`msglog`);
  - a field diff of both Talions' animation components (`compdiff`);
  - hardware write breakpoints that log the writer's stack (`dwatch`);
  - per-frame graph inputs of the real Talion (`animcal`) and a freeze detector;
  - a crash logger with unwound stacks (`crashlog`).
- Also prototyped and logged in the notes (not part of the current path):
  - an NPC puppet: spawn, brand, per-frame placement;
  - NPC walking through `GotoNode` with a runtime `AINodeGoto` (no effect);
  - hand-built Attract stimuli (accepted, but did not move the puppet);
  - record swaps for the look (NPC accepted, still an orc).

## QA

- Offline suite (in the mod repo: `build.bat`, then `build\tests\somguard_tests.exe` with the
  guard, the negative control and the game's original video DLL): runs the mod against fakes of
  the game's data. Last run for v0.1.0, 2026-11-23: **70 passed, 0 failed, 0 skipped; negative
  control (protection compiled out) failed 14 of 14 protection cases, as required.** Besides the list below it covers the
  Steam lobby and save transfer, message validation for every message kind, smoothing, ending
  paired moves, the enemy snapshot, type and kill messages, and the read cache.
- Earlier, at 50 cases (2026-11-21: 50 passed, 0 failed; negative control 14 of 14), it covered:
  - protected writes, including path variants;
  - cloud-save sandboxing and network limits;
  - DLL forwarders;
  - each hook installer (refuses a mismatched target, passes calls through);
  - packet validation;
  - record reading, cloning and in-place swaps;
  - the profile sandbox and the mutex rename;
  - the call-counting stubs (int, float and stack arguments pass through; counts, callers and
    restore are right), the slot controller's refusals, the crash logger and the field diff;
  - the data watch, the animation tick and bind (including an exception in each), the
    locomotion inputs, the calibration logger and packet v2.
- Game runs (Steam build 7348332, one PC, launched by a script that checks the real save and
  settings against hashed backups before and after every run; 0 changes in every run):
  - **one copy and a loopback bot** sending a circle: 1,809 packets received, 0 rejected, the
    puppet placed every frame and following Talion;
  - **two copies**: each placed its puppet exactly at the other copy's Talion position, packet
    age 15–63 ms;
  - **second Talion**: created (`players=2`, the real one first), moved by the bot, and reported
    visible as Talion in the A-pose;
  - **diagnostic runs 33–44**: call counts, controller dumps, a slot-1 controller, component
    diffs, message-handler counts and contents, and write breakpoints, as listed in the notes.
    Every probe restored its vtable slots with 0 mismatches;
  - **animation runs 45–49** with the bot:
    - launch 46: idle pose (owner);
    - launch 47: walking (owner and a screenshot);
    - launch 49: walk, jog, sprint and stand phases (owner);
  - **two copies, animated (launch 50)**: 777 and 984 packets accepted, 0 rejected; each copy's
    second Talion driven by the other copy's own graph inputs. Owner: the other Talion moved like
    the one being steered.
  - **combat moves (launches 54–65)**: attacks replayed in order between two copies over loopback
    (launch 56); sword, sheathe clip and stealth switches mirrored (launches 58–63, owner checks);
    over the network both ways with 0 bad packets (launch 65).
  - **enemies (launches 66–78)**: the friend-side copy spawned no NPCs of its own; 18–28 puppets
    at the host's NPC positions with 0 rejected snapshots; deaths removed the puppet (launch 74,
    owner: "fully gone"); orc idle animations replayed (launch 78).
- Sessions over the internet (author and one friend, real installs, 2026-11-21 to 11-22, several
  builds): invite, save transfer, seeing each other, attacks, executions and shared enemies worked
  in the host's logs and the author's reports. Each session's problems are in the notes and were
  fixed in later builds; the last two fixes (friend's kills, joining next to the host) have not been
  seen in a session yet.
- These checks come from logs and the author's reports. There is no automated assertion suite
  for in-game state and no reports from other players, so the QA level stays `none`.

## How to play

Install and play instructions are in the mod's
[README](https://github.com/danudns/shadow-of-mordor-coop#install). In short: both players
extract the [release zip](https://github.com/danudns/shadow-of-mordor-coop/releases) and run
`Install.cmd`; the host loads their save and invites through the Steam overlay; the friend closes
the game and accepts the invite. Needs the current Steam build (the mod stays off on any other).

## Known issues

- The friend's hits that do not kill are not sent; orcs in the host's world ignore the friend.
- Orcs the host kills vanish for the friend instead of falling.
- Enemy puppets on the friend's side move and idle but do not play every walk clip or attack.
- Captains, Nemesis, missions, collectibles and progression are separate for each player.
- If the host disconnects, the friend's world has no roaming enemies until a normal restart.
- Quitting the game can crash it (a reference-count assert at exit, `exe+D59E56`, with the second
  Talion's controller in place). The save is already written by then.

## Next steps

From the mod's roadmap:
- **Hits (phase 2):** send the friend's hits to the host and apply them with the game's own damage
  code; send staggers, knockdowns and deaths back so orcs fall on both screens.
- **Orcs react to the friend (phase 3):** in the host's world, orcs notice, target and attack the
  friend's Talion.
- **Full orc animations**, then **captains and Nemesis**, then **missions and progression**.
- **Research, one shared world:** feed the friend's client from the host's server through the
  engine's own client/server message queue instead of syncing two worlds.
- **Exit crash:** release the second Talion's controller cleanly before the game quits.
