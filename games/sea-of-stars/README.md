# Sea of Stars (2023)

| | |
|---|---|
| Engine | Unity **2021.3.25f1**, IL2CPP (metadata v29), x64; BepInEx **6.0.0-pre.2** IL2CPP plugin |
| Versions tested | build **2.0.58728** (Windows). `SeaOfStars.exe` SHA-256 `8009b8c8f842e02c12f8576f3b3aefcaddfc76048fe4d5233f5b5055bf08e427`. Both players must run this build |
| Projects | **Sea of Stars Online** — private repo, source not published yet (see *Where the code is*) |
| QA level | **none** — automated two-client gates cover the transport, the peer's co-op slot and movement in both directions; combat, the actors' kill path, story, items, transitions and saves have no passing run (see *QA*) |
| Last updated | 2026-11-20 |

## Where things stand

Sea of Stars ships local co-op, so this project does not build co-op from scratch: it puts the
second controller on the internet. On each machine the peer's party character is given to a real
`SinglePlayerPlusPlayer` slot and that slot's input comes from the network instead of a gamepad; the
game itself moves, animates, casts shadows for and drives both bodies. Two real game clients on two
Steam accounts connect, load the same map, play different characters, and each machine's copy of the
peer's body ends exactly on the position the peer reports.

Everything outside those two bodies is still two separate worlds. Enemies are streamed but nothing
kills one; encounters, dialogue, chests, items, map transitions and saves are not replicated. Nobody
has played the story through together.

| Layer | Status | Evidence |
|---|---|---|
| Both copies report the same plugin protocol (2) and build | works | `plugin: protocol=2 build=1.0.0.0` in both logs of `C2-guest-20261120-211415` |
| BepInEx 6 pre.2 hook loads; Harmony patch set binds | works | `harmony PatchAll ok` in every passing log; a missing patch target shows as `harmony PatchAll FAILED` and costs the whole patch set (`A-20261120-210616`, 18/19; the cause is in its `.guest.log`) |
| Session over the mock transport, two copies on one PC | works | `A-20261120-220613.qa.out` — 19/19 |
| Same, with 10% packet loss and 80 ms delay | works | `loss-20261120-184929.qa.out` — 19/19 |
| Real Steam: two accounts, friends-only lobby, join by lobby id | works | `C2-guest-20261120-211415.qa.out` — 17/17; `C2-guest-20261120-211415.lobby.txt` |
| Real Steam: public lobby (control arm) | works | `C-guest-20261120-211920.qa.out` — 17/17 |
| Guest's character moves on the host (real Steam) | works | `C-guest-20261120-190636.qa.out` — 13/13; `C2-guest-20261120-211415.qa.out` |
| Host's character moves on the guest (real Steam) | works — **public lobby arm**; friends-only not re-run | `C-host-20261120-190919.qa.out` — 13/13; the friends-only host-walks run is `C2-host-20261120-201843.qa.out`, `FAIL (16/17)` on the guest-persona assertion only |
| One enemy actor streamed, position agrees on both sides | works, one actor only | `actorsync: role=…` status lines in `C2-guest-20261120-211415.host.log` / `.guest.log` |
| Actor kill → despawn on the peer | unproven | `tools/qa-actor-kill.py` scored **3/16** on `C-guest-20261120-190636.{host,guest}.log` (`docs/actor-kill.md`) |
| Combat: entry, participants, turn order, one mirrored hit, guest apply | **works** (one controlled fight) | `local/qa/battle-full-20261121-010232.qa.out` — `RESULT: PASS (19/19)`. Contact entry into `ENC_01 (TutoMana)`, identical `participants=[P:ZALE,P:VALERE,P:GARL,E:0:TERN]` and turn order, `epoch=1`, guest `apply done`, host `combat: hit seq=36 dmg=1 hp=4/5` mirrored as guest `combat: hp seq=36`, duplicate ignored; both probes `loaded=22 inScene=22 active=10`. Scope: one tutorial fight, one hit — a general combat claim still needs a longer fight and a second encounter |
| Story state: GPI + blackboard snapshot, host → peer | partial — snapshot only | `local/qa/story-chest-r3-20261120-223200.guest.log:60` — `story: snapshot apply seq=5 gpi=0 bb=2 changed=2`, final `state=` identical on both sides, `diff=0`, `misses=0`, `unsupported=0`; checker `local/qa/story-chest-r3-20261120-223200.strict.story.out` passes every state assertion |
| Story: dialogue mirroring (presentation only) | proven once — one `show` effect | `local/qa/story-archivist-20261120-231310.strict.story.out` — `RESULT: PASS (24/24)`; host `story: dialog out seq=1 op=show data='f6fe0400…' char='THE_ARCHIVIST'`, guest `story: dialog in seq=1 peerBox=1073741824` with a box remap and no guest graph advance. `line`, `close` and `choice` were not in that dialog, so those assertions passed vacuously |
| Story: chest loot grant, items | unproven | `tools/qa-story.py` fails those assertions on every run so far (no dialog and no loot has occurred); the one chest found (`98940bbb…`, `looted=false`) is behind a scene boundary this save cannot cross |
| Map transitions | written, runtime unproven | `LevelTravel.cs` (msgs 52-55, host captures door/switch/load, peer applies, landing on `FinishLevelInitialization`); 35/35 unit tests, but the lab level has **no `LevelLoadingZone`** so no run has exercised it |
| Save transfer | written, runtime unproven | `SaveTransfer.cs` (msgs 56-58, 28-byte header, 4 MiB bound); guest takes the host's blob at the title screen into a **memory-only** slot with saving disabled (`saveDisabled` is checked before any write, proved from the binary); 25/25 unit tests, no run |
| Three players | out of scope | owner decision on 2026-11-20: two players only; `docs/roadmap-sync-everything.md` still describes the 3-player design |

## Existing work

- **Official local co-op** ("Single Player+"): a second player joins on the same screen and drives
  another party member. It is the base this project builds on, not a competitor.
- **No online co-op mod was found.** Searches run on 2026-11-20: web search for
  `"Sea of Stars" online co-op mod multiplayer`, this hub's `DIRECTORY.md`, and `games/` in this
  repo. Not checked: GitHub code search, ModDB, Discord, Steam forums. Treat "none exists" as
  unproven rather than as a finding.
- **Nexus has a modding scene**: a BepInEx 6 setup guide and a local co-op combat-control quality-of-
  life mod (Nexus mod ids 11 and 18, recorded in the working notes). No networking work was seen
  there.
- **Remote Play Together** was considered and rejected: zero mod work, but it streams a screen and
  needs a Steam copy of the game on both sides; the request was a co-op mod.
- One earlier cut of this mod drew the peer as a sprite clone and hid the local copy of their
  character. It was removed: it duplicates a body the game already simulates, with the wrong shadow,
  particles and combat state. It is not in the current history.

## How the game works (for co-op)

- **There is no network code anywhere in the assembly.** Steam is reached only through
  Facepunch.Steamworks, for platform services. There is nothing to revive and nothing to hook; the
  netcode is new code in the plugin.
- **Local co-op is the hook point.** `SinglePlayerPlusManager` (a `Manager<T>` singleton) holds the
  `SinglePlayerPlusPlayer` slots; each slot wraps a **Rewired** `Player` and has `SetPlayer()`,
  `Join()`, `Leave()`, `BeginPlaying()`, `UnassignCharacter()`, `AssignCharacter()` and
  `EnterPlayableState()`. `PlayerInputs` / `SinglePlayerPlusInputs` read that Rewired player.
- **Party and leader.** `PlayerPartyManager` exposes `Leader`, `LeaderAndFollowers`,
  `CurrentPartyCharacters` and `SetLeader(CharacterDefinitionId)`; `PlayerPartyCharacter` exposes the
  `Transform`, `playerSpriteRenderer` and `playerInputs`. `CharacterDefinitionId` carries a string id
  (`ZALE`, `VALERE`, `GARL`, …).
- **Combat slots.** `EncounterPlayersManager` with `AbstractEncounterPlayerSlotAssignator` decides
  which character fills which battle slot. Not replicated.
- **Scenes and saves.** Scenes are Unity scenes; the lab save loads `Mooncradle_Art`, with
  `Mooncradle_Gameplay` additively loaded behind it and holding every `EncounterTrigger` and
  `Encounter`. Saves are
  `%USERPROFILE%\AppData\LocalLow\Sabotage Studio\Sea of Stars\SOS_SaveSlot*.sos`; both copies read
  and write that folder.
- **Reading it.** Il2CppDumper (dump + dummy DLLs) plus the interop assemblies BepInEx generates from
  `GameAssembly.dll`. `ClassInjector` accepts an injected MonoBehaviour on this build — every gate log
  has `AddComponent<Ticker> ok` (e.g. `C2-guest-20261120-211415.host.log`) — so the plugin gets a
  normal `Update` loop; the Harmony tick is only the fallback.
- **Action names** come from Rewired's own mapping and were dumped from a running copy:
  `local/action-list.txt`, 59 actions (`Interact`, `UI_Confirm`, `AdvanceTime`, `RewindTime`,
  `Join`, `Leave`, `HighFive`, …). The same dump is what makes scripted input possible without
  guessing names.
- **Addressables bundles are readable offline.** The Mooncradle scene tree (trigger and encounter
  positions, hitbox sizes) was read straight out of
  `StreamingAssets/aa/StandaloneWindows64/abf1f4ca3fae9b611b75814026b5eb0a.bundle` without launching
  the game; `docs/battle-entry.md` and `docs/lab-map.md` are built from that.

## What we changed

Design rule: **one authority per simulated thing**. The lobby owner is the host, and each machine
owns the character it plays — the session is symmetric, the host has no authority over the peer's
body. The layers still to be built (encounters, story, transitions) are designed host-authoritative:
the host decides, the peer applies.

1. **Loader.** BepInEx 6.0.0-pre.2 (IL2CPP, zip SHA-256 `616EC7EB…93CEB`), plugin in
   `BepInEx\plugins`, `UnityLogListening = false` (otherwise the IL2CPP chainloader dies inside
   Il2CppInterop), `steam_appid.txt` = 480. *Proof:* `plugin: protocol=2 build=1.0.0.0` in both logs
   of `C2-guest-20261120-211415`.
2. **Pause bug.** A copy stopped ticking when it lost window focus, so it never answered the peer.
   Root cause: Unity stops calling `Update` for a background window. Fix: `Application.runInBackground
   = true` at load. *Proof:* every gate below runs with both windows minimized and both copies keep
   logging status lines (`A-20261120-220613.qa.out`).
3. **Native co-op slot.** The peer's character is handed to a real slot:
   `ReInput.players.GetPlayer(1)` → `SetPlayer` → `Join` → `UnassignCharacter` →
   `AssignCharacter(peer character)` → `EnterPlayableState(true)`. Two probe findings made this work:
   `Join()` sets `Playing`/`Index` immediately but assigns a *default* character a frame or two later
   (so the wanted character must be assigned explicitly), and `SetLeader`/`AssignCharacter` leave the
   body in `FollowerCopyTargetState`, where the game never reads movement input — only
   `EnterPlayableState(true)` makes it playable. *Proof:* "host's co-op slot is the client's
   character: VALERE vs VALERE", "client's co-op slot is the host's character: ZALE vs ZALE",
   `joins=1` on both sides (`A-20261120-220613.qa.out`).
4. **Input replication.** 30 Hz axes plus a held-button mask; press edges are sent separately and
   **latched on the receiver until consumed**, so a 30 Hz stream cannot swallow a single-frame tap.
   The override sits at the controller source — `Rewired.Player.GetAxis` / `GetButton` /
   `GetButtonDown` / `GetButtonUp` (string and int overloads), plus the `PlayerInputs` category
   methods — because the character controller reads Rewired directly on some paths; patching only
   `PlayerInputs` accepted the value and the body never moved. The remote slot deliberately gets a
   Rewired player id different from the local human's, or the two overrides mask each other.
   *Proof:* "guest's own body walked left: x delta -1.55" (`A-20261120-220613.qa.out`) and the
   movement assertions in `C2-guest-20261120-211415.qa.out`.
5. **Pose correction.** Each machine sends its own body position at 5 Hz; the same body on the other
   machine is eased toward it (snapped when more than 3 m out, and right after the slot is created).
   x/z only — y stays with each machine's own simulation. *Proof:* "host copy ends on the guest: axis
   delta 0.00" (`C2-guest-20261120-211415.qa.out`).
6. **Steam transport.** AppID 480 (Spacewar), so there is no separate app to obtain and no port
   forwarding.
   `ISteamNetworkingMessages` through the flat C API with every entry point resolved by
   `GetProcAddress`; Valve's `steam_api64.dll` (Steamworks.NET standalone 2025.164.1, SHA-256
   `eb17909a…fb75`) ships in the plugin folder and is loaded **by absolute path**, so the DLL in the
   game folder is never the one bound. **Polling, not callbacks**: the game runs its own Steamworks
   dispatcher, so the plugin uses `IsAPICallCompleted`/`GetAPICallResult` and membership queries, and
   accepts sessions per lobby member. Friends-only (`k_ELobbyTypeFriendsOnly`) is the default; a
   public lobby is the explicit opt-out used as a lab control. Only lobby members' datagrams are
   accepted. *Proof:* `C2-guest-20261120-211415.lobby.txt` (`lobby.created.type=friends-only`,
   `EResult=1`, `join.EResponse=1`, `members=2`) and the matching 17/17 assertion file.
7. **Scripted input** (lab tool, not player-facing). A JSON scenario drives the local player at the
   Rewired layer — stick, held actions, taps, arbitrary duration — so a gate can walk somewhere and
   press something with no keyboard, no focus and a minimized window. Steps are evaluated
   analytically, so a tap between two frames is still delivered. The older 4 s single-direction walk
   is kept byte-identical for the 19-assertion gate. *Proof:* the action dump in
   `local/action-list.txt` came from a running copy; `docs/scripted-input.md` has the schema.
8. **Story state, effects not scripts** (`StorySync.cs`). Host-authoritative and idempotent by
   `(epoch, seq)`: a chunked GPI + blackboard snapshot at a 5 s heartbeat and on change, the guest
   applying it and logging drift rather than hard-blocking its own writes; dialogue mirrored as
   presentation (show/line/close/choice) so the guest shows the host's lines without executing its own
   graph; chest loot mirrored by reconstructing the host's concrete `lootsToGive` entries and calling
   `Loot.GiveLoot(character, showNotification: false)` on the peer, never the chest's own
   `OnLootSuccess`. *Proof:* snapshot replication only — see the layer table; the wire format has an
   offline suite (`local\qa\storysync-tests-20261120.unit.out`, `PASS 28/28`).

### Three bugs the harnesses caught (each cost a whole layer)



A Harmony patch whose **target method does not exist** makes `PatchAll` throw, and every patch in the
assembly is silently lost while the plugin still loads. Gate A caught it as the same single
assertion four times — `guest's own body walked left: x delta 0.00`, 18/19 — with two different
culprits: `Player.GetAxisRaw(string)` (does not exist on this Rewired build; `A-20261120-205458`,
`A-20261120-205655`) and the 3-arg `PlayDialog` overload, which lives on `NewDialogBoxInstance` and
not on `NewDialogBox` where the patch looked (`A-20261120-210433`, `A-20261120-210616`). Both fixed;
the suite is green again from `A-20261120-210753.qa.out`. Every gate log must therefore be checked
for `harmony PatchAll ok`.

The third was not a patch: **every story message was applied twice** because `StorySync.Attach`
subscribed the transport's `Received` while `Session.OnEnvelope` already forwarded the same frames.
Found by `tools/qa-story.py` before any player saw a doubled chest, fixed by keeping the Session path
only: the log then shows one `story: snapshot recv` per `seq` and one `apply`
(`local/qa/story-chest-r3-20261120-223200.guest.log`). The pattern that pays for itself: one strict
checker per subsystem, a synthetic dry run that must pass and a real run that must fail.

### How a real battle was reached (and what it took)

No gate could produce a fight on the game's own saves: the tutorial encounter starts but never builds a
party, and the forced path makes the game's own `InitiateEncounter` throw. The fight came from two
things instead. First, **a crafted lab save**: a `.sos` file is a ULEB128 length prefix plus UTF-8 JSON
(the "checksum" is just that length), so `tools/lab-save.py` can point a copy at any known level and
checkpoint, recomputing the prefix. Second, `savepointId = 0` — "no anchor" — makes the game honour the
saved position, so the party spawns at the encounter trigger and contact fires during the load.

With that, `ENC_01 (TutoMana)` in `ElderMistTrials` produced a real battle: `participants=[P:ZALE,
P:VALERE,P:GARL,E:0:TERN]` and the same turn order on host and guest, `epoch=1`, no forcing anywhere.
It also proved the input layer was genuinely dead until a Harmony prefix was fixed (a `Task<bool>`
declared against `Il2CppSystem.Threading.Tasks.Task<bool>` aborted `PatchAll` and silently dropped
every later patch — see lesson 99).

### Written, not proven

`ActorSync.cs` (actor stream,
ownership epochs, despawn path), `StorySync.cs` (GPI/chest state, blackboard flags, dialogue
presentation, mirrored loot), `BattleEntry.cs` (walk-to-trigger driver) and `TriggerDump.cs` exist and
are wired into the plugin's tick, and `tools/qa-actor-kill.py` / `tools/qa-battle.py` assert on them.
None of them has a passing run; see *Known issues*.

Two of those do have unit coverage, and it is honest to say what it covers: the scripted-input
parser/timeline and the story wire format have offline suites that run without the game —
`local\qa\scriptedinput-tests-20261120.unit.out` (`RESULT: PASS (42/42)`) and
`local\qa\storysync-tests-20261120.unit.out` (`PASS 28/28`). A unit suite proves the format and the
timeline logic, not that a battle or a chest syncs.

## QA

**The level is `none`.** The template's `automated` means the mod's claims are checked by a test
that ran, and `verified-playthrough` means a human finished the game in co-op; neither is true here.
What exists is narrower: automated two-client gates for the transport, the peer's co-op slot, peer
input and movement in both directions, the friends-only lobby and the public-lobby control, plus one
real dialogue mirrored. Combat, the actors' kill path, chest loot and items, transitions and saves
have code, unit suites and harnesses, and no passing runtime run.

**One command for the whole verdict:** `tools/acceptance.ps1` runs the gates in order (Gate A clean,
Gate A at 10% loss + 80 ms, Gate C2 real Steam friends-only, Gate D forced encounter, the story
checker), writes `local/qa/acceptance-<stamp>/` with every artifact plus `SUMMARY.md`, and prints a
table where each row carries its own `RESULT:` line and a one-line honesty note. A skipped gate prints
`SKIPPED` and the summary says **SKIPPED is not a pass**; a missing prerequisite fails that gate by
name instead of hiding. It never launches anything itself — each row calls the existing script, so the
gate scripts stay the single source of truth.

**One runner per machine:** `tools/game-lease.ps1` holds a lease (holder, pid, start time, commit) that
every launch path takes before starting a copy and releases in `finally`. A second launcher exits
immediately with `lease: held by <holder>` and never waits; a lease whose pid is dead is reclaimed and
logged. That turns "nobody else launches" from a promise into something the machine enforces.

**Strict per-subsystem checkers, each proven red before green:** `qa-story.py` (24 assertions; PASS on
synthetic fixtures, FAIL on every real run that lacked dialog or loot), `qa-battle.py` (with a
`--forced` mode that puts the caveat in the RESULT line itself), `qa-travel.py` (29 assertions, 33
strict — PASS 29/29 on a synthetic three-transition run, and every deliberately broken arm goes red:
a missing apply 28/29, a 3.5 m spawn shift 27/29, no door dump 27/29, a second landing 2 s later
28/29, an idle pair 16/29), `qa-actor-kill.py` (16 checks, 3/16 on today's real logs), and `qa-save.py` (17 assertions on the
save-transfer contract: PASS 17/17 on the good pair, FAIL 8/17 on a bad one, and it compares
runner-captured md5 manifests so the guest save folder is proven byte-identical while the checker
itself only reads). Missing
lines are always FAIL, never skipped — the rule that caught the double-dispatch bug.

**Unit suites (no game, no Unity):** `ScriptedInput` 42/42, `StorySync` 28/28, `LevelTravel` 35/35,
`SaveTransfer` 25/25 — each compiles one production file with `SOSO_TESTS` and asserts the pure half.
A unit suite proves formats and timelines, never that a fight or a chest syncs.

### How to run the tests

All of these run from the lab repo, start the game copies **minimized**, send no keystrokes and
restore the player's saves afterwards. Artifacts land in `local\qa\<label>-<stamp>.*`.

```powershell
cd <lab dir>
powershell -ExecutionPolicy Bypass -File tools\gate.ps1 -Gate A                          # mock pair
powershell -ExecutionPolicy Bypass -File tools\gate.ps1 -Gate loss -Loss 10 -DelayMs 80  # same + loss
```

`gate.ps1` runs a frozen copy of the two-copy harness (`local\pre-s6-original\`) and tees its stdout,
so a `RESULT:` line exists as a file. `qa-two-client.py` asserts 19 things on the two plugin logs: both copies in gameplay, each
sees one peer, same map, different characters (`ZALE` vs `VALERE`), each machine's co-op slot holds
the peer's character, input frames in both directions, exactly one slot join per side, the guest body
walked under the scripted stick, and the host's copy of that body ends on it.

```powershell
# real Steam: host PC + one Hyper-V VM, two Steam accounts, elevated PowerShell
powershell -ExecutionPolicy Bypass -File tools\gate-c.ps1 -Mover guest                 # friends-only (C2-*)
powershell -ExecutionPolicy Bypass -File tools\gate-c.ps1 -Mover host
powershell -ExecutionPolicy Bypass -File tools\gate-c.ps1 -Mover guest -PublicLobby    # control (C-*)
```

One direction per invocation, because each direction is a separate quotable result.
`qa-realsteam.py` asserts 17 things, including the lobby type actually requested, both sides' persona
state, the join path and the character the peer's slot holds. Every run also writes
`<label>-<stamp>.lobby.txt` with the requested vs created lobby type, `EResult`, `join.EResponse`,
membership and the Steam account each client bound.

```powershell
# whole-lab drivers, for reproducing without remembering the pieces
powershell -ExecutionPolicy Bypass -File tools\run-lab.ps1 -Players 2 mock
powershell -ExecutionPolicy Bypass -File tools\run-lab.ps1 -Players 2 real -Deploy

# assertion scripts on collected logs
python -I tools\qa-two-client.py    --host <host plugin.log> --client <guest plugin.log>
python -I tools\qa-realsteam.py     --host <host plugin.log> --guest <guest plugin.log> --mover guest
python -I tools\qa-actor-kill.py    --host <host plugin.log> --guest <guest plugin.log>
python -I tools\qa-battle.py        --host <host plugin.log> --guest <guest plugin.log> --require-probe
```

Two lab traps that produced false failures: **mock gates need the lab VM off** (with the VM's Steam
client signed in both copies freeze right after `TitleScreen.OnContinue()`; three runs were lost to
`0/8` and `2/8` before it was found — `gate.ps1` now refuses to run while the VM is flagged up), and
a copy needs `SOSO_TEST_WINDOW=1` (its saved screen state is native 1920x1080 and covers the screen)
plus `SOSO_LOAD_SLOT=0` (Continue is pressed from inside the plugin via `TitleScreen.OnContinue()`,
so no key is sent to a background window).

### Last results (all 2026-11-20)

| Run | Artifact | Result |
|---|---|---|
| Gate A, mock pair, clean | `A-20261120-220613.qa.out` | PASS 19/19 |
| Gate A, 10% loss + 80 ms | `loss-20261120-184929.qa.out` | PASS 19/19 |
| Gate C, guest walks, public lobby | `C-guest-20261120-190636.qa.out` | PASS 13/13 |
| Gate C, host walks, public lobby | `C-host-20261120-190919.qa.out` | PASS 13/13 |
| Gate C, guest walks, friends-only | `C2-guest-20261120-210044`, `-211011`, `-211415` | PASS 17/17 each |
| Gate C, host walks, friends-only | `C2-host-20261120-201843.qa.out` | FAIL 16/17 — the one failure is the guest persona log line, added after that run; all movement assertions passed |
| Gate C, guest walks, public control | `C-guest-20261120-211920.qa.out` | PASS 17/17 |
| Battle assertions | `C-host-20261120-191531.qa.out` | FAIL 1/5 |
| Actor-kill assertions | `C-guest-20261120-190636.{host,guest}.log` | FAIL 3/16 |

Gate A was run 24 times that day: 17 at 19/19, three at 0–2/8 during the VM trap, four at 18/19 from
the aborted `PatchAll` described above.

**What the movement result does and does not prove.** The peer's body is driven by the replicated
input *and* eased toward the 5 Hz position its owner reports, so the assertion "the copy ends on the
peer's body" cannot separate the two contributions; it proves that each machine's body ends where the
peer says it is, not that input alone carried it there. The loss run's parameters (10%, 80 ms) come
from the gate command line — the artifact is named `loss-` and records the result, not the settings.

**The last green Gate A predates the two most recent commits** (`7353eb9` changes scripted input and
encounter detection; `173b697` is its docs follow-up); re-run it before quoting 19/19 for `HEAD`.

### Where the code is

Private repo at `<lab dir>` (not published):

- `src/SeaOfStarsOnline/` — `Plugin.cs`, `Tick.cs`, `Session.cs`, `GameSync.cs`, `RemoteSlot.cs`,
  `InputSync.cs`, `RewiredInput.cs`, `ActorSync.cs`, `EncounterSync.cs`, `StorySync.cs`,
  `ScriptedInput.cs`, `BattleEntry.cs`, `TriggerDump.cs`, `Net/` (`ITransport`, `SteamTransport`,
  `SteamApi`, `MockTransport`).
- `docs/` — `architecture.md`, `lab-map.md`, `battle-entry.md`, `actor-kill.md`, `scripted-input.md`,
  `lobby-friends.md`, `headless-lab.md`, `slice-s4-story.md`, `roadmap-sync-everything.md`.
- `tools/` — the gates, the QA scripts, `run-lab.ps1`, `bot-scenarios/`, `ScriptedInput.Tests`,
  `StorySync.Tests`, VM helpers.
- Source is MIT. No game files or assets are included.

## How to play

There is no release yet, and this is not a playable co-op campaign: expect two player bodies and
nothing else.

Requirements: Sea of Stars build **2.0.58728** on both machines (exe SHA-256 above), Windows 10/11
x64, and Steam running and logged in on both machines. No Spacewar ownership is needed; both players
appear as Spacewar because the mod uses AppID 480.

Install, from the repo:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -GameDir "<game dir>"     # .NET 6; stages plugin\
powershell -ExecutionPolicy Bypass -File install.ps1 -GameDir "<game dir>"
```

`build.ps1` takes `-Dotnet <path>` if `dotnet` is not where it expects it.

`install.ps1` installs BepInEx 6 pre.2 (hash-pinned), the plugin, Valve's `steam_api64.dll` from
Steamworks.NET standalone 2025.164.1 (hash-pinned) in the plugin folder, writes `steam_appid.txt` =
480 and sets the one BepInEx logging option this IL2CPP build needs. It modifies no game file.
Uninstall: `-Uninstall`.

Then:

1. Both players load the **same save** and stand in the same area.
2. Host presses **F9**. The lobby is friends-only by default; its id is in the log and in
   `BepInEx\config\seaofstars-online\endpoint.txt`.
3. Guest presses **F10**. The lobby id is read from `%TEMP%\seaofstars-online\join.txt` or the
   clipboard. (A second F10 invites friends, once both are in a session.)
4. **F11** leaves.

Once both copies are on the same map, each machine binds the peer's character to a co-op slot and
the two players drive different party members; the guest swaps leader after the handshake so the two
humans never hold the same body. Nothing happens outside that map gate, and nothing carries the
host's save to the guest. Lobby creation, tag and join are the ones in
`C2-guest-20261120-211415.lobby.txt`; the friends-only default is in `docs/lobby-friends.md`.

Environment switches the harness uses (not needed to play): `SOSO_TRANSPORT=mock`, `SOSO_SLOT=1|2`,
`SOSO_HOST=1`, `SOSO_JOIN=<id>`, `SOSO_LOBBY_PUBLIC=1`, `SOSO_TEST_WINDOW=1`, `SOSO_LOAD_SLOT=<n>`,
`SOSO_SCRIPT_FILE=<json>`, `SOSO_NO_OVERLAY=1`. Log:
`BepInEx\config\seaofstars-online\plugin.log`.

## Known issues

What a player will notice:

- **Two separate worlds.** Enemies, encounters, dialogue and story flags, chests, item and currency
  contents, map transitions and saves are per copy. Only the two player bodies cross the wire. A
  remote player who looks like they are fighting, opening a chest or leaving the map is not doing it
  on the other machine.
- **Both players must load the same save themselves.** There is no save transfer, and the only
  compatibility check is the map: two copies on the same map from *different* saves will bind, then
  diverge in every story flag.
- **The co-op slot binds only while both copies are on the same map.** Until they are, the guest's
  character is not handed over and nothing syncs; the log says `co-op slot not bound until the maps
  match`, and the bind retries on every tick once they match. The guest's leader swap is attempted up
  to five times, 3 s apart, while both machines still hold the same character.
- **No invite, no lobby list.** Join by lobby id (`join.txt` or clipboard) is the only implemented
  path; the mod never binds `GameLobbyJoinRequested_t`, and Steam never lists friends-only lobbies,
  so "my friend cannot see the lobby" is expected.
- **Combat timing input is not replicated.** `AdvanceTime` / `RewindTime` are not overridden, so even
  once a battle is synced a remote player cannot hit timed attack or block windows.
- **Two players only.** The game's local co-op has a slot per party character; this mod fills one of
  them remotely. Three players are out of scope by owner decision (2026-11-20);
  `docs/roadmap-sync-everything.md` still holds the 3-player design.
- **Pose correction moves x/z only.** Height stays with each machine's own simulation, so stairs and
  other height changes are untested.

Open issues (no known cause, listed so they are not rediscovered as new):

- **`C2-guest-20261120-203257` — 10/17, `connected=False`.** One friends-only run has no explanation.
  Both clients joined the lobby (`members=2`, `EResult=1`, `join.EResponse=1`), the session connected
  for a few seconds, then went silent in both directions: the guest logged `session: peer … timed out`
  and neither side reconnected, while both clients stayed in the lobby. The guest's own body walked
  (x span 60.33) and the host's copy of it never moved (x span 0.00, end delta 74.50); the guest's game
  had not loaded the save yet (`scene=''`) when the session dropped. Three later friends-only runs and
  the public control do not reproduce it, and nothing in either log names a cause.
- **Mock gates under a running lab VM** freeze both copies right after `TitleScreen.OnContinue()`
  (`0/8`, `2/8`, `2/8` artifacts exist). Cause is recorded in `docs/headless-lab.md`; the gate now
  refuses to run in that state.
- **`C2-host-20261120-201843` scores 16/17** only because the guest persona log line did not exist
  when it ran; the assertion is green in every later friends-only run. Quote it for movement, not for
  17/17.
- **Persona evidence is split.** The lobby files show both accounts set to Invisible in the Steam
  client at launch, while the plugin's own `GetPersonaState()` read `Online`; the friends-only result
  proves the lobby type and the join, not that an Invisible friend stays joinable.
- **The Steam path has only been run on one LAN** (host PC plus a VM on a bridged switch). Different
  networks, Steam's relay path and a NAT on either side are untested.
- **`ActorSync` streams from the host only** (`SetOwner` is never called), and a death during a battle
  is deliberately ignored until the actor has been inert for 2 s afterwards. Any kill test must avoid
  a map transition, which clears actor state.

## Next steps

1. **Combat sync.** Needs a save past the Mooncradle tutorial: the training encounter starts
   (`Encounter_KidsFight`) but never builds a party on this save (`players=0`, `participants=[]`), and
   `Mooncradle_Art` holds no reachable touch trigger (`docs/battle-entry.md`; that forced-entry
   observation has no `.qa.out` of its own). Then override `GetAdvanceTime` / `GetRewindTime` and give
   `EncounterPlayersManager` host authority.
2. **Actor kill.** Move the host to the village plaza at `(66.8, 1.0, 79.9)` so two actors are inside
   the 32 m sample radius, produce one death, and drive `tools/qa-actor-kill.py` from 3/16 to 16/16.
   The despawn path already exists (`CheckLocalDeaths` → `MsgDespawn` → `ApplyRemoteDeath` with
   duplicate-epoch guards); the missing piece was always a host-side kill to observe.
3. **Story by effects** (`docs/slice-s4-story.md`): host-side GPI/chest/lever capture, dialogue
   presentation on the peer, guest-side interaction suppression, and the canonical state hash that
   makes a desync visible.
4. **Map transitions and save transfer**, so a guest can follow the host through a door and join an
   existing run without the host's save file.
5. **Publish**: source and a release (one install file per OS), then invite-via-lobby support so the
   guest does not have to copy a lobby id by hand.
