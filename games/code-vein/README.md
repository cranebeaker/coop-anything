# Code Vein (2019)

| | |
|---|---|
| Engine | Unreal Engine 4.18 (`++UE4+Release-4.18`), Denuvo still present |
| Versions tested | Steam (app 678960), ProductVersion 1.53.93354, game exe SHA-256 `8fa550fe00a8c28c8947a9511128c21924860be0bb4ebf5a09b8a6cf50e0e172` |
| Projects | [eight8pac/codevein-coop](https://github.com/eight8pac/codevein-coop) (code, MIT; claim: cranebeaker/coop-anything#207); Nexus mod 192 (older, partial) |
| QA level | none (manual two-PC sessions only) |
| Last updated | 2026-10-29 |

## Where things stand

Code Vein ships online co-op for a host and **one** guest, matched by distress signals. Vanilla ends the session on host
death, on a guest's final death in a boss room and after every boss kill, and it forbids co-op in areas whose boss is dead.
This project keeps the vanilla session and works around those ends. A UE4SS Lua mod re-hosts and re-joins automatically after
field deaths, host deaths, boss-room deaths and boss kills. A native hook DLL (C, MinHook) reaches the parts that Lua cannot. In
two-PC sessions: a guest who dies for good stays in the host's world, spectates the host and respawns next to the host at the
host's next Mistle rest (the partner cannot revive him after his revive window); a guest held in a boss fight lets the host's
outro play after the kill; the host can rest at a Mistle with a guest connected; and the guest gets his own Mistle menu in the
host's world and can level up there (both PCs show the new level and max HP); and a Mistle teleport by the host takes the guest
along, also into another stage. Seamless story co-op for 3+ players is the goal;
it is not playable as a release today. Code: [eight8pac/codevein-coop](https://github.com/eight8pac/codevein-coop).

## Existing work

- **Nexus mod 192** (2020), described as a co-op mod. According to a forum reply it only lets the host send signals
  in areas whose boss is dead; the guest is still removed on boss death and fog walls remain. The mod page was not
  fetched (blocked), so this is unverified.
  Source: <https://steamcommunity.com/app/678960/discussions/0/7098294290812770549>
- A 2023 forum reply says no mod raises the guest count or adds seamless co-op:
  <https://steamcommunity.com/app/678960/discussions/0/3875968032471215769>
- An unverified claim of a paid seamless mod exists in the same thread. Treat it as unknown.
- **UE4SS works on this game**: "Ultra Plus for Code Vein" ships it (<https://www.nexusmods.com/codevein/mods/808>).
- Fan petition for a 3-player limit: <https://www.change.org/p/bandai-namco-expand-code-vein-s-player-limit-to-3>
- Vanilla co-op rules: <https://www.shacknews.com/article/114202/how-to-play-co-op-in-code-vein>

## How the game works (for co-op)

**Packaging.** Pak version 4, no encryption, zlib blocks; cooked Blueprints are split into `.uasset` (name table)
and `.uexp` (bytecode). A minimal read-only pak reader and a Kismet call finder answer most "who calls what with which
argument" questions without extracting anything.

**Exe and Denuvo.** The exe has **no ASLR** (`DYNAMIC_BASE` off), so addresses are fixed and the UFunction exec-thunk
addresses in a UE4SS object dump are directly usable for native hooks. Denuvo mostly **relocated** function bodies: the
original address holds a 5-byte `jmp` into the large `.code` section, where the code is plain, readable compiler output.
Targeted Ghidra decompiles (headless, no full analysis) take a minute each. There is no RTTI; class vtables can be read from a
live object (or from the exe, since vtables are static data).

**UE4SS.** v3.0.1-1181 (`experimental-latest`) injects fine despite Denuvo. **`HookLoadMap = 1` crashes** at the first map
travel (`UGameEngine::Tick`+0x398); found by bisecting all 14 hooks. On this build UE4SS can hook **native UFunctions only**:
hooks on Blueprint-defined functions and on BlueprintImplementableEvents fail to register, while calling BP events from Lua works.
**A Lua hook on `FieldTriggerBase:IsAuthorization` froze the game** the first time a Mistle overlap called it; avoid it. Keybinds
do not fire from injected key presses, so drive mods by polling a trigger file.

**Online layer** (module `/Script/Extensions`):
- `UAppOnlineWrapper` wraps the Steam OnlineSubsystem (`SteamNetDriver`, P2P relay on). `UAppSessionManager` adds the
  distress-signal layer. **`ERescueSignalState` (Idle 0, Sending 1, Receiving 2, CooperativeMode 3, EndOfCooperativeMode 4) is the
  byte at `AppSessionManager + 0xE08`**. State 3 is set as soon as a signal is out, before anyone joins.
- **The session is open only while all four channels of `ESessionOpenness` are open: Field = 1, BossArea = 2, Cinematics = 4,
  EnemyLevelResetByCheckPoint = 8.** `ExpectsBeOpenRaw(bit)` reports one channel. Cutscenes close Cinematics, a Mistle rest closes
  EnemyReset for ~2 s, and in an area whose boss is dead Field stays closed.
- The session flow lives in the game instance Blueprint (`/Game/Modes/App_GameInstance`): `Event_CreateLobby(PublicConnections,
  UseLAN, Caller)` → native `CreateOnlineSessionLatent`, and `Event_FindAndJoinSession(Caller)` → native
  `FindAndJoinOnlineSessionsLatent` (a strict search, then a relaxed retry). The only caller of both is
  `UserInterface/FieldMenu/BP_FieldMenuUIComponent`, whose bytecode passes **`PublicConnections = 3`**; the number never reaches the
  session settings (derived from the name tables). **The 1-guest cap is native.**
- **Session state machine (native):** an internal state integer at `AppSessionManager + 0xD90` (codes 0x00 to 0x22) drives the visible
  state through an "on enter state" handler (body `0x146EA3AC0`). Codes 0x1F and 0x20 set EndOfCooperativeMode. **`EndCooperativeMode`
  (body `0x146E92EF0`)** either switches to 0x1F at once (no other players) or sets a request flag at `+0xDC0` that the state machine
  turns into 0x1F shortly after. Its only direct caller is a player-death handler. A guest's final death in the boss room leads there
  ~7.5 s after the death, on both PCs at the same moment, and the host then destroys its session.

**Teardowns seen in vanilla two-player sessions** (host logs):

| Event | What happens |
|---|---|
| Player downed | Session closed to new joiners for ~26 s, guest stays |
| Guest dies in the field | Guest removed, session stays open |
| Host dies | Session destroyed ~3 s after death, natively |
| Guest's final death in a boss room | Both sides switch to EndOfCooperativeMode ~7.5 s later; host destroys the session, guest travels home 4 s after |
| Enter boss mist | Operation lock on, session closed, `BossAreaState` 0→1→2 |
| Boss killed | `MulticastGainRewards` → `MulticastFinishBossBattle(true)` → the **guest's own** `ServerRequestNetworkDisconnect` |
| Area whose boss is dead | `IsSendRescueSignal` false (menu greyed out), Field channel closed |
| Menu "cancel signal" / "leave" | `DestroyOnlineSessionLatent` (the only teardown that calls it, on host and guest) |
| Mistle rest with a guest connected | Prompt greyed out (native checks, see below) |

**Mistle rest.** The Mistle's `IsActionButtonActive` override (vtable `+0x660`) greys the prompt out when `IsOnlineMultiplay()` is true
or a "more than one player" check (`0x140828970`) is true (also through a wrapper at its start), and when enemies are near. Its
`ProcessInputButtonEvent` (vtable `+0x618`) starts with the same "more than one player" check before resting.

**Deaths.** A player first goes down into a revivable "dying" state (`CharacterHelper:IsDying`; a partner or another player can
revive him). When the revive window runs out, the death is decided on two paths: `CharacterHelper:RequestTryExecuteDeath` ->
`0x140508540(world, character, ...)` (falls and similar, decided by the host) and a component's death processing
`0x14056AC60` (component `+0xE8` = character; an expired window on the guest is decided by the guest's own game). Both lead to
the death fade (`FadeUIComponentIF:RequestDeathFade`, which also sets a temporary input mask on the controller; only that fade
component's own `RequestUnfade` releases it), the dissolve (`bIsDisappearExpression`, a material effect the visibility flags do
not undo) and, on a guest, the leave (`SwitchToEndCoop` from `0x14633CFA0`, ~3 s later). Vetoing both death paths for a guest
while the host lives keeps him "dying" with no death sequence at all; patching the death sequence after the fact (unfade, input
masks, input mode) did not give a guest his controls back after a respawn. `CharacterHelper:RequestPlayerSpawn(ctx, controller,
location, rotation, controller rotation, IsByRespawn, SkipAppear)` on the host spawns a fresh pawn for a guest's controller,
including the appear effect. `CharacterHelper:IsNeverRevive` is not asked by the partner's revive.

**Mistle internals.** `ProcessInputButtonEvent` (body `0x146A1CB70`) places the player, asks the session to close the
EnemyLevelResetByCheckPoint channel (`0x1408176A0` = CloseOnlineSession(8), session event 0x16), spawns an actor, sets the
Mistle's own state and continues in a callback after the sit-down animation, which opens the menu. On a guest the session part
stalls (0x16 -> 0x19 only) and changing the Mistle's state asserts (`CheckPointBase.cpp:850`, "WITH_EDITOR"). The menu itself opens
anywhere with `BP_CheckpointUIComponent:Open(None, None, 0, None, WDT_CheckpointMenu_C)`. Its entries are widgets
(`WDT_CommonListTextPiece_C_*`; in screen order level up, gifts, storage, teleport, switch partner, presents, tutorial, quit).

**Who owns what for a guest.** His haze lives on his PC (`HumanContextComponent:Server_AddExp` on the host reaches him and leaves
the host's copy at 0). His level as the host sees it (and his max HP, sent back with `Multicast_SyncTotalMaxHealth`) lives in the
host's copy of him. The level getter (`HumanContextIF` base `+0x90`) returns the stored level only when the character is "in
charge" (`0x140456A00`, authority-like); otherwise it derives it from the player data, and the setter writes that derived value
back, so a plain level-up on a guest changes nothing. Level, gifts and equipment are synced once per component at the join
(`ServerInitialDataSync`, sent by `0x140465CA0`, guarded by `bIsInitialSynced`); a guest cannot resend it later. The menu's
level-up message (`0x2c` to `HumanContextIF` components) did not reach the real level-up `0x1405A2E20` in co-op.

**Mistle teleport.** The world is one persistent map (`/Game/Maps/MainGame`) with streamed stage sublevels, but a Mistle teleport
is not a move: it is `CheckPointUIComponent:RequestPlayerSpawnToCheckPoint(FieldID, PointIndex)`, a fresh spawn of the player at the
target Mistle. A solo trace of two teleports saw the characters re-spawn (initial data sync and gift equip multicasts) and no call
to the deferred-teleport functions (`LIBDeferredTeleportManager:BeginDeferredTeleport`, `ScenarioActor:WaitForDeferredTeleportFinished`).
`CharacterHelper:RequestPlayerTeleport(target, location, rotation, controller rotation, bSkipAnimation)` moves a player in place
(used by another modder's test tool), and `CharacterHelper:RequestPlayerSpawn` spawns a fresh pawn for a remote controller. The
native implementation `0x1409A1800(component, FieldID, PointIndex)` gives the destination. A teleport with a guest connected ends
co-op twice: `SwitchToEndCoop` from `0x146678D02` right at the teleport (after an "ending co-op / distress signal" message,
`0x14083A630`), and, on arriving in a stage, the area-entry code (`SwitchToEndCoop` from `0x146D627CF`, then `0x1409F1BA0`, which
shows "multiplayer is not possible in this area" and ends co-op through a Blueprint path). Home Base is a separate map: vanilla ends
co-op and travels there.

**Story cutscenes and memories.** Field cutscenes and boss intros already play with a guest present (both see them). The post-boss
outro (e.g. `Cinematics/Scenario/St09_Mt_Snow/.../BP_ScenarioFlowDiagram_e06700_e06800`) is a chain of native latent waits on
`ScenarioActor`: `WaitForPreparePolyFinished`, `WaitForLargePolyFinished`, `WaitForDeferredTeleportFinished`, then
`ScenarioManager:PlayMemoryResidue` and `WaitForMemoryResidueFinished`, with `bSpawnPlayerOnExit`. With the boss-end kick removed the
host hung on a loading screen at the outro start, before any memory object existed; which wait it hangs on is not known yet. The
vestige restoration screen (`BP_BCRestorationUIComponent`) asks `AppSessionManager:IsCreateSession` and
`AppOnlineWrapper:GetOnlineOtherPlayerNum`, shows "ending the distress signal / co-op" (`MultiplayText_0470`/`0480`), destroys the
session and then jumps into the memory (`JumpToMemory`, `MemoryResidueId`). With a distress signal out, the player cannot try a
memory alone, so this needs a guest to test.
With the kick removed (two-PC tests, 2026-10-29) the boss intro and the outro start normally, the outro's deferred-teleport
driver (`DeferredTeleportDriver`, state `Done`, no pause tokens) finishes, and then the host waits on a loading screen before
`PlayMemoryResidue` until the guest leaves; at that moment the outro plays at once. The wait is in the large-poly loading steps.
The boss's blood code is granted only after the memory. Tracing the `ScenarioActor` wait UFunctions with exec-thunk detours made
the boss intro hang, so trace them one at a time.

**Save system.** Story state is string-flag based (`UAppSaveManager::SetGameProgress`). Saves live in the per-user local app data
folder, under `CodeVein\Saved\SaveGames\<account>\`. `-userdir` is ignored, so isolate test runs by swapping the folder. Copying
`.sav` files between Steam accounts works with Steam Cloud off.

**One-PC testing.** A second copy exits at once on a single-instance mutex; only one copy gets the Steam identity, and a client
without it crashed in native code on `open 127.0.0.1`. A mock `steam_api` is not an option because Denuvo checks ownership through
Steam. Two-player tests use two PCs (one person can drive both with a remote desktop tool).

**Pitfalls.** Lua hooks (UE4SS) froze or crashed this game on `FieldTriggerBase:IsAuthorization`, `AppSessionManager:
GetRescueSignalState` (when a distress signal was sent) and when ten or more death/fade UFunctions were hooked at once (title
screen crash in UE4SS.dll); native exec-thunk detours of the same functions were stable. Accepting a Steam invite or "Join game"
made the test copy quit cleanly and Steam start the real install, which then played on the real save: join test sessions only
through the in-game distress signal.

## What we changed

Three parts: `CoopSeamless` (Lua, host and guests), `CoopProbe` (Lua, read-only state logging) and `CoopNative` (C DLL loaded by
Ultimate ASI Loader as `winmm.dll`, MinHook detours, verifies 16 bytes at each target before patching).

- **Co-op in areas whose boss is dead.** `IsSendRescueSignal` overridden to true, then `OpenOnlineSession(Field)`. Tested with two PCs.
- **Boss-end kick (understood, deliberately kept).** The vanilla kick is the guest's reaction to `MulticastFinishBossBattle(true)`;
  flipping the flag on the guest keeps the guest connected but stalls the host at the outro, which leads into a single-player memory
  sequence. So the kick stays and the guest rejoins after it.
- **Automatic step out and rejoin (CoopSeamless).** A reconciler runs every second and acts only after 8 s of calm (online, not busy,
  no lock, no boss fight, own character alive and not downed, no level sequence playing, the other three channels open; a respawn
  restarts the 8 s). A host without a session calls `Event_CreateLobby(3, false, nil)`; with a session but Field closed it calls
  `OpenOnlineSession(Field)`; a guest who is not connected calls `Event_FindAndJoinSession(nil)` every 25 s. Host and guest mode
  turn on only when a session or connection *starts* by hand; a menu cancel or leave (`DestroyOnlineSessionLatent`) turns them off.
  Two-PC results: see QA.
- **Native features (CoopNative + CoopProbe, off by default, switched in `plugins\CoopNative.ini`):**
  - `rest_in_coop`: answers "no" to the multiplayer checks only for calls from the Mistle's prompt and input functions; the host
    rests with a guest connected and the guest stays.
  - `veto_endcoop` + `spectate_cam`: keeps the session when a player dies for good in a boss fight while another lives. Two veto
    points are needed, `EndCooperativeMode` and the small "switch to end of co-op" function `0x14081BDA0` that a Blueprint-driven
    death path calls directly. While held, the dead player's camera follows a living player (`SetViewTargetWithBlend`) and the death
    overlays (`WDT_FadeScreen`, `WDT_DeathParts`) are hidden. On a boss kill the held guest replays his vetoed leave so the host's
    outro can finish.
  - `keep_dying` (group respawn): vetoes both death paths for the guest's character (host: his pawn; guest: his own pawn) while the
    host lives. Once his revive window has run out, the host removes his body (unpossess, destroy) so the partner cannot revive
    him, he spectates, and the host's next Mistle rest spawns him next to the host with `RequestPlayerSpawn` and the appear effect.
  - `group_teleport`: vetoes the two end-of-co-op calls of a host teleport (only those callers, for 60 s), skips their messages,
    then spawns the guest next to the host once CoopNative sees the host's characters re-spawn (`AppCharacter:MulticastInitialDataSync`).
    `teleport_skip_fields` keeps vanilla behaviour for listed destinations (meant for Home Base).
  - Falls: a guest whose fall the host decides (`TryExecuteDeath` while falling fast) goes straight to spectating: dissolve, body
    removed, back at the host's next rest.
  - `guest_rest`: on the guest, E at an opened Mistle skips the vanilla rest and opens his own Mistle menu; switch partner is disabled
    (it left him on a loading screen). Level-up: the menu's level-up becomes a direct `HumanContextIF` level-up on his own component
    with `0x140456A00` forced true for his pawn during the call (spends his haze, sets his level), then he sends
    `Server_AddExp(0)` once per level; the host (`levelup_relay`) sets the level of its copy with the setter under the same force and
    recomputes the stats, so his max HP updates.

## QA

No automated suite; the QA level is none. Tests were manual sessions on two PCs (host on a full copy of the install with a
save swap and SHA-256 checks of the real saves before and after each run), with logs from both sides:

| Case | Result |
|---|---|
| Guest dies in the field | rejoined automatically after 27 s |
| Host dies | session ended, host signal back 8 s after respawn, guest rejoined 27 s after the teardown |
| Guest's final death in a boss room, then a wipe | rejoined after the fight reset |
| Boss kill | outro and memory played normally for the host; guest rejoined 55 s after the kick |
| Host ends the session through the menu | host mode off, no re-signal |
| Guest leaves through the menu | pulled back (bug); fix written, not yet tested with two players |
| Host rests at a Mistle with a guest connected (`rest_in_coop`) | rest works, guest stays; enemies reset once per session only |
| Guest's final death in a boss room (`veto_endcoop`, `spectate_cam`) | guest stays and watches the host; a wipe still ends normally |
| Boss kill while the guest is held | host outro plays, guest goes home and rejoins |
| Guest dies for good in the field (`keep_dying`) | body removed after his revive window, he spectates, host rest respawns him next to the host, visible, with input |
| Guest at a Mistle (`guest_rest`) | his menu opens; heal sticks; level-up shows on both PCs incl. max HP; switch partner disabled |
| Gifts on the guest | unlock and equip work (vanilla) once he switches to the gift's blood code |
| Host Mistle teleport (`group_teleport`) | guest stays and appears next to the host, within a stage and into another stage; guest sees the world unload for a few seconds |
| Guest falls deep (`keep_dying`) | straight to spectating, back at the host's rest |
| Boss kill with the kick removed | intro, fight and outro start; host waits before the memory until the guest leaves |

## How to play

Not released. Install steps and settings are in the code repo's README
([eight8pac/codevein-coop](https://github.com/eight8pac/codevein-coop)): UE4SS v3.0.1 with `HookLoadMap = 0` on host and guests,
the Lua mods, optionally Ultimate ASI Loader + CoopNative, and the same matchmaking password (the automatic search joins the first
matching signal).

## Known issues

- Without a matchmaking password a guest's automatic search can join a stranger's signal.
- A guest whose host stopped on purpose keeps searching every 25 s.
- Vestige restoration is blocked in co-op (the Mistle menu entries can be enabled; the restoration screen still refuses).
- Enemies reset only on the first co-op rest of a session; a guest's rest does not reset the host's world.
- A guest cannot open chests and does not see ground items in the host's world. Story NPCs follow each player's own story
  progress (one appeared twice on the guest's side).
- While spectating, the low-health screen filter and heartbeat keep playing; a respawned guest flashes once before the appear effect.
- A guest opening his Mistle menu while the host rests crashes; resting together crashes; enemies no longer reset after the first rest.
- Teleporting the group to Home Base drops the host into a pit (separate map; its FieldID is to be added to `teleport_skip_fields`).
- A guest joining while the host sits at a Mistle crashes.
- If a distress signal "fails to go through", Steam lobby creation is timing out (`k_EResultTimeout` in the game log); restarting the
  Steam client fixed it once.

## Next steps

1. Guest progression and shared boss rewards (CoopProgress by Rikka230, eight8pac/codevein-coop#1): two-PC test.
2. Host death in a boss fight while the guest lives (the host spectates until everyone is dead).
3. Gifts equip and chests for the guest (likely the same host-side handling as the level-up), enemy reset on every rest.
4. Vestige restoration and memory sequences in co-op (one player goes through the memory, the other watches): answer "no session"
   to the restoration screen only; find the outro wait that hangs with a guest present.
5. Group teleport: a fade for the guest, Home Base, a vote. Then a 3-player cap (native: the Steam session's player count or join acceptance), Home Base together.
