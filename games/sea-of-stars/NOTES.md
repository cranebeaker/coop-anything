# Sea of Stars — online co-op (work in progress)

Working notes for the coop-anything agent. Written as the work happened; observations, derived
facts, guesses and unknowns are kept apart.

## Identity (observed 2026-11-20)

| Field | Value |
|---|---|
| Game | Sea of Stars (Sabotage Studio), build **2.0.58728**, Copyright 2023-2025 |
| Engine | Unity **2021.3.25f1**, IL2CPP, metadata v29 |
| Exe | `SeaOfStars.exe`, SHA-256 `8009b8c8f842e02c12f8576f3b3aefcaddfc76048fe4d5233f5b5055bf08e427` |
| Test install | `<lab host install>` (root copy + junction to `<game dir>\SeaOfStars_Data`), second copy `<lab guest install>` |
| Loader | BepInEx **6.0.0-pre.2** (Unity IL2CPP win-x64), zip SHA-256 `616EC7EB...93CEB` |
| Steam | real `steam_api64.dll` from Steamworks.NET standalone 2025.164.1, SHA-256 `eb17909a...b75` (playbook pin) |
| Local co-op | shipped ("Single Player+"): up to 3 local players |

The install this agent was pointed at is a repack: `tenoke.ini`, `steam_settings/`,
`steam_api64.tenoke.bak`, plus a **Goldberg emulator** `steam_api64.dll` (11.4 MB, `{::EmuSteamInstall::}`,
`Spacewar/SteamworksExample`) and `steam_appid.txt` = 480 placed by an earlier session. There is no
Sea of Stars appmanifest in the local Steam library, so the owner does not appear to own it on Steam.
That is why the mod uses **Spacewar AppID 480 with the real Steamworks DLL loaded by path**, never the
install's emulator.

## Phase 0/1 findings

- Directory: no `sea-of-stars` entry in `DIRECTORY.md`, no `games/sea-of-stars` in the checkout.
- Web: only local co-op exists officially. Nexus has a modding scene (BepInEx 6 for this game,
  Nexus mod 11; a co-op combat-control QoL mod, Nexus mod 18). **No online co-op mod found** by
  search on 2026-11-20 (`"Sea of Stars" online co-op mod multiplayer`). Still open: GitHub code
  search, ModDB, Discord.
- Prior session in this install: `host_probe.out`, `lobby.txt` and the Goldberg DLL. The probe
  proves the *flat Steam API* and lobby create/join/ping work — against Goldberg, i.e. a local
  emulator. It does **not** prove real Steam relay. Treated as a hint, not a result.

## Phase 3: how local co-op is built (from Il2CppDumper)

Dump: `local/dump` of the work repo (not committed): `dump.cs`, `il2cpp.h`, `DummyDll`.

- `SinglePlayerPlusManager : Manager<SinglePlayerPlusManager>` holds `allPlayers` (3
  `SinglePlayerPlusPlayer`), `allActivePlayers`, `activeAdditionalPlayers`,
  `playerControlledCharacters`, high-five data, a safe teleporter.
- `SinglePlayerPlusPlayer : MonoBehaviour` wraps a **Rewired `Player`** (`Player` property), has
  `Index`, `Playing`, `AssignedCharacter`, `SinglePlayerPlusInputs`, `Join()`, `Leave()`,
  `BeginPlaying()`, `AssignCharacter()`, `EnterFollowerState()`, `EnterPlayableState()`.
- Input per human: `PlayerInputs : InputCategory` (`GetDirectionalInput`, `GetHorizontalInput`,
  `GetInteractDown`, `GetHighFive`, …) constructed from a Rewired `Player`; co-op extras in
  `SinglePlayerPlusInputs` (`GetJoinDown`, `GetLeave`, `GetBubble`).
- Party: `PlayerPartyManager : Manager<PlayerPartyManager>` — `Leader`, `LeaderAndFollowers`,
  `CurrentPartyCharacters`, `SetLeader(CharacterDefinitionId)`, `SwapPartyLeader()`,
  `ShowOnlyLeaderAndFollowers()`, `onLeaderSwapped`.
- `PlayerPartyCharacter : Character` — `playerSpriteRenderer` (SpriteRenderer at 0x100),
  `controller`, `followComponent`, `playerCombatActor`, `Transform` property. `Character` carries
  `characterDefinitionId` (struct with a `string characterId`; statics `Zale`, `Valere`, `Garl`, …).
- Combat turn ownership: `EncounterPlayersManager` + `AbstractEncounterPlayerSlotAssignator`
  (`EncounterPlayerSlotDefaultAssignator`, `...NearestCharacterAssignator`).
- No network code of any kind in the assembly. Steam is only Facepunch.Steamworks
  (`Facepunch.Steamworks.Win64.dll` is in the interop set) for platform services.

### Observed at runtime (probe plugin)

- Title screen → main menu → Continue: scene `TitleScreen` → `Mooncradle_Art`.
- Leader `PlayableCharacter_KidSunboy(Clone)` at `(-74.50,-11.00,31.70)`, sprite
  `SunboyKid_Idle_D5_F00`, sortingOrder 100, layer Default.
- `CurrentPartyCharacters` = `[PlayableCharacter_KidSunboy(Clone), PlayableCharacter_KidMoongirl(Clone),
  PlayableCharacter_KidGarl(Clone)]`.
- `SinglePlayerPlusManager.allPlayers` = 3 slots; slot 0 `index=0 playing=True`, 1 and 2 idle.
- `ClassInjector` **works** on this build (an injected MonoBehaviour ticks), unlike the Prince of
  Persia IL2CPP target in this workspace. The Harmony fallback is kept.

## Design (chosen, after the owner's correction)

The first cut rendered the peer as a sprite puppet. The owner rejected that: the game already has
real co-op bodies, so the network should feed the *native* co-op slot, not draw a clone. Final
design:

1. Both players run the game (same build) and load the same save.
2. **Host keeps the leader; the guest switches leader** to another party character
   (`PlayerPartyManager.SetLeader`), so the two humans own different bodies.
3. On each machine, the **peer's character is given to a real `SinglePlayerPlusPlayer` slot**:
   `ReInput.players.GetPlayer(1)` → `slot.SetPlayer` → `slot.Join()` → `slot.AssignCharacter(...)`.
   The game then drives that body itself: animation, shadows, followers, combat turns.
4. The peer's input is streamed at 30 Hz (axes + held buttons + latched press edges) and returned
   from Harmony prefixes on that slot's `PlayerInputs`. Nothing in this mod moves or draws a body.
5. Transport: Steam Spacewar (AppID 480) lobbies + `ISteamNetworkingMessages`, flat C API loaded by
   absolute path, polling only. Lab path: mock transport over localhost UDP with fake SteamIDs.

Probe evidence for step 3 (build 2.0.58728, one machine):

```
coop-probe: rewired playerCount=3 all=4
coop-probe: slot[1] index=-1 playing=False player=Player1 char=none
coop-probe: SetPlayer ok
coop-probe: Join ok -> playing=True index=1 char=none
coop-probe: assigned character is null          # filled on a later frame
... 4 s later: slot[1] playing=True char=assigned (GARL by default)
```

So `Join()` takes effect immediately, the game assigns a default character a frame or two later, and
the wanted character has to be assigned explicitly (`UnassignCharacter` + `AssignCharacter`).

**The missing step, found the hard way:** assigning a character is not enough to control it. Both
`PlayerPartyManager.SetLeader` and `SinglePlayerPlusPlayer.AssignCharacter` leave the body in
`FollowerCopyTargetState`; the game only reads movement input once
`SinglePlayerPlusPlayer.EnterPlayableState(true)` has been called (state then reads
`PlayerDefaultState/Grounded`). Without it the mod's input reached the game — `gameLocalReads`
climbed — but the character never moved. On the guest, the same call is needed after the leader
swap: `allActivePlayers[0].UnassignCharacter(); AssignCharacter(newLeader); EnterPlayableState(true)`.

Input-path evidence from the counters in the status line (`xz`, `dir`, `gameLocalReads`): the
character reads `PlayerInputs.GetHorizontalInput()` for locomotion (xz/dir stayed 0), which is the
read the override must answer.

### Why not the obvious alternatives

- **Remote Play Together** would be zero mod work but needs a Steam copy and a streamed screen; the
  request is a mod, and this install is not a Steam copy.
- **Input streaming into one host world** with no client instance would need the client to see the
  host's screen; that is a video stream, not co-op.
- **Puppet clone** (the first implementation, still in the git history): works and was tested 16/16,
  but it duplicates a body the game can already simulate, with wrong shadow/particles/combat state.
  The native slot is strictly better and is what ships.

## Build

Work repo `<lab dir>` (not the game install). Build with the workspace-local .NET 8 SDK:

```
<dotnet path> build -c Release
```

Deploys to `<lab host install>\BepInEx\plugins\SeaOfStarsOnline\` (and `native\steam_api64.dll`).

## Status / QA level

**automated** for transport, native co-op slot and input layer. Two real game clients, mock
transport, both windows minimized, no keyboard input and no focus:

| Run | Result |
|---|---|
| clean | 19/19 PASS: same map, `ZALE` vs `VALERE`, each slot holds the peer's character, input both ways, one join per copy, guest body walked -1.55 x, host copy ends exactly on it (0.00) |
| 10% loss + 80 ms delay | 19/19 PASS on the same assertions |

Real Steam relay with two accounts: **not tested** (one account cannot P2P to itself).
Real Steam host with one account: **verified** — real `steam_api64.dll` from the plugin folder,
`SteamAPI_InitFlat` ok, friends-only lobby created (`109775245114291199`), lobby tag `soso=soso1`
written (`set=True`), lobby id published to `endpoint.txt`. Invite overlay suppressed in the lab with
`SOSO_NO_OVERLAY=1` so nothing pops up on the owner's desktop.
Nothing beyond the two player bodies is synced (enemies, encounters, story, items, transitions).

Test-path lessons worth keeping:
- lab copies must be launched with `SOSO_TEST_WINDOW=1` (the game's saved screen state is native
  1920x1080 and covers the screen) and loaded with `SOSO_LOAD_SLOT=0`
  (`TitleScreen.OnContinue()`), so the window can stay minimized with no keystrokes;
- `Application.runInBackground = true` is required or a background copy stops ticking;
- a Harmony patch whose target method does not exist (`GetAxisRaw(string)` here) aborts `PatchAll`
  for the whole assembly while the plugin still loads — check the `harmony PatchAll ok` log line.

## Open questions / unknowns

- Does the game's save system permit two copies to use the same slot without clobbering? Both copies
  in the lab share `%USERPROFILE%\AppData\LocalLow\Sabotage Studio\Sea of Stars`; saves are backed up
  and restored around every run.
- Whether `SetLeader` mid-session is stable in all states (cinematics, encounters) — only tested from
  free movement so far.
- Which other subsystems need host authority first: encounters (`EncounterPlayerSlotAssignator`),
  story/dialogue advance, chests/levers, map transitions (`SinglePlayerPlusTeleporterHandler`).

## 2026-11-20 — takeover by Co-op man; scope set to 2 players; first integrated gate

Owner instruction: **ship 2 players only.** No third Steam account, no third player, no second guest
VM. S1 (multi-peer slots) was cancelled mid-write for that reason.

Repo hygiene: `<lab dir>` is now a git repo. Baseline `e96d427` on `main` (a restore
point — it contains S1's partial edits), integration on `feat/2p-sync`, green build `1513c09`.

Gate A (the original two-client harness, both copies minimized, `RESULT: PASS (19/19)`, run again
with 10% loss + 80 ms): 19/19 both times. Assertions: both copies in gameplay on the same map, each
sees one peer, each owns a different character, each renders the other's body at the other's
position, input frames flow both ways, bot walk markers, one co-op slot join per side.

Movement, read from `plugin.log` directly rather than from a summary: the guest's own body ends
`(-76.05,-11.00,31.21)` and the host's `remotePos` equals it exactly, with only two distinct
`remotePos` values in the whole run. **Guest moves on host: verified. Host moves on guest: not
exercised** (the host's body never walked, so the guest's `remotePos` stayed constant).

Not verified, and nothing claims otherwise: S2 encounters (`EncounterSync.cs` compiles and is
integrated, no battle run), S3 world actors (`ActorSync.cs` compiles, `sent=0 msgs=0 bytes=0` on one
side — the wire has not carried an actor yet), combat timing input, story/items/transitions/saves,
real-Steam movement (`remotePos` constant, input frames carried zero axes — the run proved session
and input-frame exchange only), 3 players (out of scope).

Lesson worth keeping: the harness's `RESULT:` lines were only ever in an agent's report, not on disk.
Every gate must write its stdout to `<lab dir>\local\qa\<gate>-<timestamp>.out`, or the
claim cannot be checked later and cannot be quoted in the write-up.

## 2026-11-20 — Gate C: movement sync verified both directions on real Steam

Build `161d16c` on `feat/2p-sync`, 0 errors. Host game on the desktop, second game inside the
Hyper-V VM, both minimized; two real Steam accounts; **public Spacewar lobby** (`SOSO_LOBBY_PUBLIC=1`)
because the two lab accounts are not friends — friends-only was not tested.

- guest walks, host still: `local\qa\C-guest-20261120-190636.qa.out` — `RESULT: PASS (13/13)`.
  Host `remotePos` x span `76.05` against the guest's own `61.88`, copy lands with `0.00` axis delta.
- host walks, guest still: `local\qa\C-host-20261120-190919.qa.out` — `RESULT: PASS (13/13)`, mirrored.
- Gate A regression after the same build: `local\qa\A-20261120-193001.qa.out` — `PASS (19/19)`.

So the peer's body is the game's own co-op slot, driven by replicated input, and it moves on the other
machine in both directions. That is the transport + per-character layer finished.

Actor streaming, same runs: host `role=host sent=942 msgs=942 bytes=40506 tracked=46 view=1`,
guest `role=peer applied=642 dropped=335 missed=4 tracked=1`, identical sample
`B49A037DA2999B74@-66.08,-11.00,30.78:e1:o0` on both. Positions agree; `actors=1`, `despawnIn=0` —
no multi-actor scene and no death, so kill/despawn and handoff stay unverified.

Battle entry blocked: `Mooncradle_Art` contains no `Encounter` until a player enters an
`EncounterTrigger`'s range; a debug scan found `Encounter objects=0` and the env flag could not force
one. The debug hook was reverted instead of shipping an unverified path. Next: dump trigger positions
and drive the bot to one.

**Environment trap worth remembering: run mock gates with the VM off.** With the VM running and its
Steam client signed in, both mock copies froze right after `TitleScreen.OnContinue()` (`TitleScreen` →
`Empty`, then no ticks) — `0/8`, `2/8`, `2/8`. Reverting the only source diff did not fix it;
stopping the VM did. Three runs were lost to this before it was found.

Correction (owner, 2026-11-20): **the two lab accounts are Steam friends** — `:3`
(76561199163564981) and `a.ashraf.yehia` (76561199521689184) — and the owner runs both as Invisible.
So the public-lobby run above was chosen on a false premise: friends-only is the shipping default and
is testable here. Gate C2 re-runs the same assertions on a friends-only lobby with both accounts
Invisible; a failure there is a shipping bug (invisible peer not joinable), not a lab limitation.

## 2026-11-20 — tooling that unblocked the remaining gates, and two owner decisions

Owner decisions this session: **chest loot is mirrored** to the second player's character (open state
alone is not enough), and dialogue mirrors **presentation only** — the guest shows the host's lines but
its own graph never advances, because replaying the script is out.

Built and integrated, each verified by a build (`0 error CS`) and, where it has pure logic, by a test
suite that ran:
- **scene-driven bot** (`ScriptedInput.cs` + `Scripted/`): strict JSON scenario, arbitrary-duration
  moves, buttons by Rewired action name, `SOSO_LOG_ACTIONS=1` to dump the local player's real action
  names. `SOSO_SCRIPT_MOVE` kept byte-identical. Suite `RESULT: PASS (42/42)`. This is the replacement
  for "hold one direction for 4 s": before it, no gate could cross more than a few metres or press
  anything, which is why battles, kills and chests were all blocked by the same missing tool.
- **trigger/route probe** (`BattleEntry.cs`, `SOSO_DUMP_TRIGGERS`): dumps every `EncounterTrigger` and
  `Encounter` including inactive. Verdict: `Mooncradle_Art` has none; all 10 live in the additively
  loaded `Mooncradle_Gameplay` and are inactive; nearest is the Garl training dummy 8.47 units east of
  spawn with `manualTrigger=1`.
- **actor-kill QA** (`tools/qa-actor-kill.py`, 16 checks): fails 3/16 on today's real logs, passes
  16/16 on synthetic. `ActorSync` already has the full despawn path (`CheckLocalDeaths` →
  `MsgDespawn` → `ApplyRemoteDeath` with duplicate-epoch guards); the missing piece was a host-side
  kill to observe, not code.
- **lobby evidence** (`gate-c.ps1` `.lobby.txt`): lobby type requested vs created, `LobbyCreated`/
  `LobbyEnter` results, enumerate/join path, both accounts' persona at launch, bound SteamIDs.

Friends-only lobby verified with an **Invisible peer**: host created `k_ELobbyTypeFriendsOnly`
(`k_EResultOK`), invisible guest joined by id (`k_EChatRoomEnterResponseSuccess`), `members=2`, bodies
bound to each other's slots, movement 0.00 end delta. Two runner traps: the lab harness used to force
`SOSO_LOBBY_PUBLIC=1` (so no run could be friends-only), and the VM needs dynamic memory 1–6 GB plus
guest Steam started and confirmed after boot.

## 2026-11-21 — save transfer, dialogue close, and four ways a gate lied

The day the remaining layers got runs, and the day four separate "green" mechanisms turned out to be
unable to fail. Each is recorded here because each cost real time and each will recur.

**Save transfer works, and writes nothing on the guest.** Shared root: `save-transfer-shared-20261121-131905.qa.out`
`PASS (18/18)` — `requests=1 sends=1 duplicate_seqs=[]`, every blob applied with matching wire hash,
level guid, party and GPI, `final story state is identical`, guest folder `before=4 after=4 identical`.
Across machines: `gate-save-20261121-112920/` shows the VM guest's own LocalLow byte-identical before
and after. The guest's slot is memory-only; the mechanism is worth remembering for any mod that hands a
save to a peer: take the blob at the title screen, apply into a slot flagged unsavable, and check the
flag before every write.

**Dialogue in this build is a sequence of `ShowDialog` boxes, not `PlayDialog`/`CloseDialogBox`.**
Three lines produced three `show` events with three different data GUIDs and zero `PlayDialog` calls;
a box that vanished was the only observable close. So the mod detects close host-side and emits it.
`choice` never occurs in the Archivist conversation — `SelectChoice` is never called — so the `op=choice`
path has no run and no hook. Assertions: `each mirrored show carries the same data id` (text source, not
event count), `peer mirrors arrive in order`, `every dialog close is mirrored host=2 peer=2 missing=[]`.

**Trap 1 — a gate detector matched the previous session's log.** `gate-c.ps1` cleared the host plugin
log at deploy but not the guest's, and its save detector was `$gTail -match 'save: applied'` over a
200-line tail. The tick that printed `applied=True` ran one second *before* the new guest session's
first line, so it matched the prior run's bytes. The run had transferred nothing. Fix: clear both logs
at deploy, latch `applied` once true, and print the exact line that matched.

**Trap 2 — an undeclared PowerShell switch is silently ignored.** `gate-c.ps1` was called with
`-StopVmAfterGateC` but never declared `[switch]$StopVmAfterGateC`; for a non-advanced script the
unknown switch lands in `$args` and the variable is null, so the VM-stop branch never ran and three
artefacts implied the fix was untested when it had not executed once. A switch a caller passes is not a
switch the script received — same family as a patch target that does not exist.

**Trap 3 — a hidden parameter prompt looks like a permissions wall.** `Join-Path` with one argument in
an elevated run blocks on `Supply values for the following parameters: ChildPath:`; the console is
invisible, so the gate simply sat at `lease: acquired` with no log and no flag, which was read as
"elevation is blocked". It cost a day and one wrong conclusion about Hyper-V permissions.

**Trap 4 — the mock transport delivered every reliable frame three times.** `MockTransport.Send` sent a
frame plus two delayed copies, and the copies were stamped with the transport's *current* `_seq`, so a
copy arriving after another send passed the receiver's dedup. Story snapshots showed it (`recv seq=20`
twice, both skipped) and a save applied then a duplicate dropped 124 ms later — which a blunt "no
message dropped" assertion called data loss. Fix: copies carry the original sequence; the drop assertion
now separates "duplicate of an applied seq" (benign) from anything else.

**The VM lab guest stopped loading.** Two warm launches, the second on a fresh VM at 5 GiB, hung before
any scene: six plugin lines, process alive, no transition, no Unity `Player.log`, and Windows reports
`Winsrv 10002 … terminated because it was hung`. Recorded as a lab limitation. Owner decision the same
day: **local first** — exhaust what two local copies can prove before spending time on the VM again.

**Builds of the same source do not hash the same.** Three builds, three PE hashes
(`A4083D50…`, `24ECF0B5…`, `98B2EA7F…`); cause unidentified. The release is packaged from the exact
tested DLL with `package.ps1 -SkipBuild`: zip `A17EA030…`, plugin `9B7E7329…`, identical to the copy
installed in the lab. Any "shipped" claim has to name the binary the gates ran, not the commit alone.
