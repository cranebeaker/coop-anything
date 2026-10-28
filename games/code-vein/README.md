# Code Vein (2019)

| | |
|---|---|
| Engine | Unreal Engine 4.18 (`++UE4+Release-4.18`), Denuvo still present |
| Versions tested | Steam (app 678960), ProductVersion 1.53.93354, game exe SHA-256 `8fa550fe00a8c28c8947a9511128c21924860be0bb4ebf5a09b8a6cf50e0e172` |
| Projects | work in progress (claim: cranebeaker/coop-anything#207); Nexus mod 192 (older, partial) |
| QA level | none (manual two-PC sessions only) |
| Last updated | 2026-10-27 |

## Where things stand

Code Vein ships online co-op for a host and **one** guest, matched by distress signals. Vanilla ends the session on host
death, on a guest's final death in a boss room and after every boss kill, and it forbids co-op in areas whose boss is dead.
This project keeps the vanilla session and works around those ends. A UE4SS Lua mod now re-hosts and re-joins automatically:
in two-PC sessions the guest was back in the host's world without menu input after a field death (27 s), a host death (27 s),
a boss-room death followed by a wipe, and a boss kill (55 s after the kick). A native hook DLL (C, MinHook) reaches the parts
that Lua cannot; two native experiments (resting at a Mistle in co-op, keeping a dead guest in the session) are in progress.
Seamless story co-op for 3+ players is the goal; it is not playable as a release today.

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

**Save system.** Story state is string-flag based (`UAppSaveManager::SetGameProgress`). Saves live in the per-user local app data
folder, under `CodeVein\Saved\SaveGames\<account>\`. `-userdir` is ignored, so isolate test runs by swapping the folder. Copying
`.sav` files between Steam accounts works with Steam Cloud off.

**One-PC testing.** A second copy exits at once on a single-instance mutex; only one copy gets the Steam identity, and a client
without it crashed in native code on `open 127.0.0.1`. A mock `steam_api` is not an option because Denuvo checks ownership through
Steam. Two-player tests use two PCs.

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
- **Native experiments (CoopNative, off by default, switched in an ini):**
  - `rest_in_coop`: answers "no" to the multiplayer checks only for calls coming from the Mistle's prompt and input functions. With the
    prompt checks covered, "Rest at Mistle" became active with a guest connected; pressing E did nothing, and the input-function check
    was found afterwards. Not tested since.
  - `veto_endcoop`: skips `EndCooperativeMode` while a guest is dead in a boss fight and someone is alive, to keep the session for
    spectating. A first attempt on the "on enter state" handler fired but did not hold (the state machine had already switched and
    moved on 11 ms later), which is how the real decision point was found. The new target is not tested yet; it is expected to be
    needed on both PCs.
  - A spectate camera works: `PlayerController:SetViewTargetWithBlend` on another character holds while alive and during the death
    sequence; only the respawn takes it back.

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

## How to play

Not released. Requires UE4SS v3.0.1-1181 with `HookLoadMap = 0` on host and guests, and the same matchmaking password (the
automatic search joins the first matching signal).

## Known issues

- Without a matchmaking password a guest's automatic search can join a stranger's signal.
- A guest whose host stopped on purpose keeps searching every 25 s.
- Resting at a Mistle and vestige restoration are blocked in co-op (vanilla).
- If a distress signal "fails to go through", Steam lobby creation is timing out (`k_EResultTimeout` in the game log); restarting the
  Steam client fixed it once.

## Next steps

1. Two-PC test of `veto_endcoop` (both PCs) and `rest_in_coop`; check that wipes and boss kills still end or reset properly.
2. Spectating in boss fights on top of the veto (the camera part works), then a group respawn instead of the rejoin fallback
   (`RestartPlayer`-style, with the game's own `PlayerSpawner` actor in mind).
3. Vestige restoration and memory sequences in co-op (the restoration UI asks for the other players and can end the session).
4. Group teleport with a vote, a 3-player cap (native: the Steam session's player count or join acceptance), Home Base together.
