# Outlast 2 — Co-op Research and Development Notes

Game slug: `outlast-2`  
Session: 2026-10-21  
Target: Campaign co-op for Outlast 2  

---

## 1. Game Identity (Rule 5)

| Field | Value | Source |
|---|---|---|
| Title | Outlast 2 | Steam Store / AppManifest |
| Store | Steam | `appmanifest_414700.acf` |
| Steam AppID | `414700` | `appmanifest_414700.acf` |
| Steam Build ID | `2601375` | `appmanifest_414700.acf` |
| Depot ID | `414701` (Manifest: `4849578644945068180`) | `appmanifest_414700.acf` |
| Internal Build Info | `timestampForBVT=2018-03-12_14.00.00`, `changelistBuiltFrom=72352`, `engineVersion=17517` | `Binaries\build.properties` |
| Main Executable | `<game dir>\Binaries\Win64\Outlast2.exe` (36,192,256 bytes) | File system check |
| Main Executable SHA-256 | `F7993204F4A865AAE46473A7B5AF59D6D0F1F8DD134158157CD95AA726554107` | PowerShell `Get-FileHash` |
| Engine | Unreal Engine 3 (custom UE3.5, 64-bit DirectX 11) | Engine config & headers |
| OS | Windows 64-bit | Platform binaries |

---

## 2. Phase 0 — Triage

### Answers to Triage Questions (`playbook/triage.md`)

1. **Which exact game?**  
   Outlast 2 (2017), developed and published by Red Barrels. Tested build: Steam 64-bit release build 2601375 (March 12, 2018 update).

2. **Is it already done?**  
   **No.**  
   - `DIRECTORY.md`: No entry for Outlast or Outlast 2.
   - `games/`: No folder for `outlast` or `outlast-2`.
   - Issue / PR tracker in `cranebeaker/coop-anything`: No mentions of Outlast 2.
   - Community modding ecosystem (Nexus Mods, GitHub, ModDB): No public co-op mod exists for Outlast 2.

3. **Which engine?**  
   Unreal Engine 3 (specifically custom UE3.5). The game compiles 64-bit code using PhysX, APEX, Wwise, and DirectX 11. Assets are stored in cooked packages (`.upk`) and texture cache files (`.tfc`) located in `OLGame\CookedPCConsole`.

4. **How much source is available?**  
   Closed binary only. Script source is not shipped in plain text, but compiled UnrealScript bytecode is embedded in the cooked packages (`Core.upk`, `Engine.upk`, `OLGame.upk`, `GameFramework.upk`, `IpDrv.upk`). Packages can be inspected using UE Explorer / UPK Explorer / UModel.

5. **Does the engine have multiplayer?**  
   The shipping game is strictly single-player. However, because it runs on Unreal Engine 3:
   - Underlying UE3 replication classes (`UNetDriver`, `UNetConnection`, `UPackageMap`, `UChannel`, `IpDrv.TcpNetDriver`) are compiled into `Outlast2.exe` and present in `IpDrv.upk`.
   - `OnlineSubsystemSteamworks.upk` and `steam_api64.dll` are included.
   - However, gameplay levels (`Crash_Persistent.udk`, `School_Persistent.udk`, etc.) and Kismet logic were authored without replication properties. A native listen-server connection does not replicate single-player sequence events, Matinee cutscenes, or door/hiding spots out of the box.

6. **Shipping and Anti-Cheat Check:**  
   Outlast 2 has no anti-cheat software (no EAC, BattlEye, or Denuvo on the latest Steam build). The mod can ship completely separately as an injectable DLL or proxy DLL without redistributing any copyrighted game assets or executables.

### Path Decision

- Since no existing co-op mod exists for Outlast 2, this is a **Build from Zero** project (`coop-build-from-zero`) catalogued as a directory gap.
- We leverage the proven architecture established by the Outlast 1 community mod (`OpenOL` / `OpenOL-Relay`), adapting it to Outlast 2's 64-bit UE3.5 engine.

---

## 3. Phase 1 — Research & Evidence

### Search Log (`searched:`)

- **Nexus Mods (`nexusmods.com/outlast2`):** Examined all 40+ available mods. Only found graphical reshades, camera mods, and a debug trainer menu (`O2menu` by Pyraciste using `O2Inject.exe`). No multiplayer or co-op mods.
- **GitHub (`site:github.com`):**
  - `"Outlast 2" coop OR multiplayer`: 0 functional co-op projects. Found personal configuration notes and an experimental vision controller.
  - Sibling game research (`"Outlast" coop OR multiplayer`): Found **`ShyKiss/OpenOL`** and **`ShyKiss/OpenOL-Relay`**, as well as speedrun tracking tools by Heawikk (`OutlastMP`).
- **Reddit & Steam Community:** Community threads repeatedly confirm Outlast 2 has never received a functional co-op mod due to the complexity of synchronizing single-player UnrealScript events and cutscenes.
- **Official Multiplayer:** Red Barrels subsequently developed ***The Outlast Trials*** (Unreal Engine 4) specifically for co-op horror.

### Reference Architecture: ShyKiss/OpenOL (Outlast 1)

`OpenOL` achieved multiplayer for the first Outlast using a two-tier approach:
1. **In-Game Client Hooks / UnrealScript Injection:** Injected custom script hooks into `OLGame.upk` / `Engine.upk` to read player position/orientation, spawn remote puppets, intercept door activations, enemy attacks, and trigger volumes.
2. **Dedicated UDP Relay (`OpenOL-Relay`):** A lightweight server maintaining rooms and state snapshots (`relay.db`). It relays high-frequency player movement packets and low-frequency reliable state events (doors opened, pickups collected, enemy states).

---

## 4. Technical Architecture for Outlast 2 Co-op

### 4.1 Injection & Hook Point
- **Hook Method:** Proxy DLL using a native system dependency in `Binaries\Win64` (such as `d3dcompiler_46.dll`, `dxgi.dll`, or `xinput1_3.dll`).
- **Engine Detour:** Hook `UObject::ProcessEvent` using MinHook.
  - Standard UE3 signature:
    ```cpp
    void __fastcall hkProcessEvent(UObject* pCallingObject, UFunction* pFunction, void* pParms, void* pResult);
    ```
  - Intercepting `ProcessEvent` provides full visibility into engine ticks (`PlayerTick`, `Tick`), movement execution, user interactions (`Use`, `Interact`), weapon/camcorder usage, and damage events.
  - Find `GObjects` and `GNames` via pattern scan or RVA offset to query all active actors in the level.

### 4.2 Remote Player Representation (Puppets)
- In Outlast 2, the protagonist is Blake Langermann (`OLHero` subclass of `OLPawn` / `APawn`).
- **Spawning:** Remote players are spawned as puppet `OLHero` or custom pawn actors with AI controller and local input bindings stripped.
- **Mesh & Animations:** Replicate skeleton, mesh, head pitch, movement animations, and camcorder holding state.
- **Camera / Night Vision / Mic:** Sync whether the remote player has night vision active or the directional microphone extended (visual and audio cues).

### 4.3 Authority Model
- **Host Authoritative:** The host instance simulates:
  - Enemy AI (`OLBot` / `OLEnemyPawn`) and pathfinding.
  - Enemy chase music, aggression, and attack targets.
  - Door states (open, closed, locked, barricaded).
  - Batteries and bandage pickups.
- **Client Autonomous:** Each client simulates their own local physics, stamina, and camera movement, broadcasting their transform, crouch/slide/swim state to the host at 20–30 Hz.
- **Damage / Death:** Host determines enemy attacks and hit registration. If a client is attacked, the host synchronizes the struggle/execution sequence.

### 4.4 Story & Level Streaming
- Outlast 2 relies heavily on streaming levels (e.g. `VSlice1_Persistent`, `Crash_Persistent`, `School_Persistent`) and Kismet trigger volumes.
- Checkpoints and streaming transitions (`X_Checkpoints.upk`) must be coordinated by the host:
  - Host triggers a transition -> broadcasts barrier packet to clients -> clients load streaming sublevel -> resumes when all clients acknowledge ready.

---

## 5. Directory and Documentation Updates
- Add entry for Outlast 2 to `data/directory.yaml` as an documented gap (`kind: gap`, `status: none`) with full search history.
- Add entry for Outlast 1 (`ShyKiss/OpenOL`) as an existing reference mod for the engine family.
- Regenerate `DIRECTORY.md` using `python tools/build_directory.py`.
- Create `games/outlast-2/README.md` following `templates/game.md`.

---

## 6. Runtime Hook Validation (2026-10-24, isolated install)

### 6.1 Test setup
- Isolated install under test: `local/installs/instance1` (Outlast 2 build 2601375, exe SHA-256 `F7993204...54107`).
- Proxy DLL source: `games/outlast-2/src/dinput8proxy.c` (+ `win_min.h`, `dinput8proxy.def`, vendored `minhook/`), built by `games/outlast-2/src/build.bat` into the isolated install's `Binaries\Win64\dinput8.dll`.
- Live launchers: `local/run_test_client.ps1` (25s), `local/run_long_test.ps1` (was 90s -> 180s -> 300s -> 420s), `local/diag_test.ps1` (60s + process liveness check).

### 6.2 Findings (2026-10-24)

- **Hook install: 3/3 live runs OK.** Log sequence on every run:
  - `[INIT] Outlast 2 Co-op Proxy DLL (dinput8.dll) attached.`
  - `[HOOK] Target function address: 0x...4DA80`
  - `[HOOK] Prologue bytes verified: 48 8B C4 48 89.`
  - `[HOOK SUCCESS] ProcessEvent hooked with 14-byte 64-bit absolute jump!`
- **Hook target validated statically** via `local/analyze_pdata.py` against the exe:
  - `.pdata` RUNTIME_FUNCTION containing RVA `0x4DA80`: Begin=`0x4DA80`, End=`0x4DDAC`, Size=812 bytes.
  - Prologue bytes at target: `48 8B C4 48 89 50 10 56 57 41 54 41 56 41 57 ...` (`mov rax,rsp; mov [rax+10h],rdx; push r15...`).
  - Stolen-region boundary lands cleanly after `push r15` (byte +15), so 15-byte trampoline copy + `jmp [rip+0]` back to +15 is instruction-aligned.
- **`ProcessEvent` interception: 0 calls observed** across live windows of 90 s, 180 s, 300 s and 420 s after hook install. The first-call log line (`[HOOK] *** FIRST ProcessEvent call intercepted successfully! ***`) never fired.
- **Process liveness (60 s diag run):** process RUNNING, 96 threads, ~30 s CPU / 60 s, working set ~1.7 GB -> engine was alive and busy (loading/streaming), not crashed, not hung at a hard exit.

### 6.3 Interpretation / open questions
- `UObject::ProcessEvent` dispatch may simply not occur until gameplay phase (post menu / post intro), or only for UFUNCTION-bound calls.
- Engine busy-ness (CPU + 1.7 GB working set) is consistent with cooked-package streaming, not with a dead hook.
- Next step: extend live window past menu/intro, and/or instrument with a longer baseline before concluding the hook is wrong.

### 6.4 Next steps
- Re-run isolated test client with a longer window (e.g. 600 s) to try to catch first dispatch.
- If still no dispatch: hook earlier in boot (e.g. `UEngine` tick) or verify entry path.
- Once first call is captured, proceed to `GObjects`/`GNames` world-actor dump and puppet spawning (`games/outlast-2/src/dinput8proxy.c` hook now logs object/function pointers + 32-byte headers on first call; rebuilt and redeployed 2026-10-24).

### 6.5 Capture enhancements (2026-10-24)
- Added `log_hex_bytes()` helper and extended first-call logging to dump object + function header bytes.
- Rebuilt via `games/outlast-2/src/build.bat`; redeployed to `local/installs/instance1\Binaries\Win64\dinput8.dll`.
- Re-validated in live run: hook install still clean (prologue OK, jump OK, no crash).

### 6.6 Phase 3 prep (2026-10-24)
- Added world actor dump / puppet spawn scaffolding to `games/outlast-2/src/dinput8proxy.c`:
  - Co-op phase state (`g_coopPhase`: solo/host/client), hero object, 8 remote puppet slots.
  - `LogActorContext()` helper (object pointer + 32-byte header dump).
  - `DumpWorldActorsNow()` and `RequestRemotePuppetSpawn()` stubs; global UObject array
    accessor (GObject/GNames) and engine spawn path still need runtime resolution.
- Rebuilt and re-validated in isolated install; still no ProcessEvent capture.

### 6.7 Capture attempts log (2026-10-24)
- Live runs with progressively extended windows (25s / 60s / 90s / 180s / 300s / 420s) and two
  build revisions (context-capture build, phase-3 build): hook install clean every time,
  first-call capture never fired.
- Process liveness: alive, CPU-active (33-34s CPU/60s), ~96 threads, ~1.7GB working set —
  engine running/loading, not crashed.

---

## 7. Sync Design (Phase 3 — capture-gated wiring)

Written 2026-10-24 while first ProcessEvent capture is pending. Design is based on the
OpenOL (Outlast 1) host-authoritative puppet model. Packet definitions live in
`games/outlast-2/src/coop_protocol.h` (compiled into the proxy DLL).

### 7.1 Authority flow

- **Host authoritative:** enemy AI (`OLBot` / `OLEnemyPawn`), door states, item/battery
  pickups, checkpoints & streaming level barriers, story/Kismet events, damage and
  struggle/execution sequences. Host relays each client's state to the others.
- **Client autonomous:** local pawn physics, camera, stamina; broadcasts own state at
  20–30 Hz (`COOP_TICK_RATE = 25 Hz`) over unreliable UDP; receives relayed state for
  remote puppets and host-authored world state.
- **Reliable channels:** `WORLD_EVENT`, `LEVEL_BARRIER`, `ENEMY_STATE`, `JOIN` are
  retransmitted until acknowledged; player state and heartbeat are fire-and-forget with
  sequence numbers for interpolation.

### 7.2 Puppet state schema (`CoopPlayerState`)

| Field | Type | Notes |
|---|---|---|
| `slot` | u8 | remote player id 0–7, host-assigned on JOIN |
| `flags` | u8 | crouch / prone / slide / swim / dive / sprint / dead / camcorder raised |
| `misc` | u8 | night vision, mic extended, bleeding, bandage, interacting |
| `stamina` | u8 | 0–100 |
| `pos[3]` | float | world position (UE3 coords) |
| `vel[3]` | float | velocity for interpolation / prediction |
| `yaw`,`pitch` | float | rotation + head/camera pitch |
| `seq` | u32 | monotonic per sender, for ordering and interpolation |

Interpolation: hold 100–150 ms of buffered state, interpolate pos/vel, snap rotation.

### 7.3 Relay packet design

- Header: `magic ('OL' 0x4F4C)` + `version` + `type` + `seq` (all little-endian, packed).
- Types: `0x01 PLAYER_STATE` (unreliable, 25 Hz), `0x02 WORLD_EVENT` (reliable),
  `0x03 LEVEL_BARRIER` (host → clients, ack; level streaming gate), `0x04 ENEMY_STATE`
  (reliable, host → clients), `0x05 JOIN` (late-join room snapshot), `0x06 HEARTBEAT`
  (1 Hz).
- Transport: single UDP socket to relay (OpenOL-Relay pattern) or direct Steam
  P2P/Spacewar relay (per `playbook/networking-steam-spacewar.md`).

### 7.4 Gating items (pending capture)

- Resolve `GObject`/`GNames` accessors from first captured `ProcessEvent` context.
- Wire `DumpWorldActorsNow()` to the real actor array; resolve engine spawn path for
  `RequestRemotePuppetSpawn()`.
- Bind `NextLocalState()` fill to the local `OLHero` pawn state.

### 7.5 Offline protocol tests (2026-10-24)

- Command: `games/outlast-2/tests/run.bat` (freestanding build, `/NODEFAULTLIB`, same
  environment as the proxy DLL; no CRT headers available in this VS install).
- Asserts: packed wire sizes (`8/44/16/12/12` bytes), field offsets, big-endian-free
  round-trip (player state + world event), single-bit flag/misc invariants, distinct
  packet type ids, protocol constants.
- Result: **PASS — 5 check groups** (2026-10-24).

### 7.6 UDP transport + loopback tests (2026-10-24)

- `games/outlast-2/src/coop_net.h/.c`: freestanding UDP transport over ws2_32 imports
  (no CRT/SDK headers), API: `coop_net_init/shutdown`, `coop_net_open` (bind, ephemeral
  port support), `coop_net_send`, `coop_net_recv` (timeout), `coop_net_local_port`,
  `coop_net_close`. `ws2_32.def` generates the import lib.
- `games/outlast-2/tests/net_loopback_test.c`: proves packet flow without the game —
  (1) self send/receive of a `CoopPlayerState`, (2) two-socket client→host delivery with
  source-address check, (3) receive timeout behavior.
- Result: **PASS — 3 check groups** (2026-10-24).
- Gotcha found by a hang: Windows `SO_RCVTIMEO` takes a `DWORD` in milliseconds, not a
  POSIX `struct timeval`; fixed in `coop_net_recv`.
- Proxy DLL build now compiles `coop_net.c` and links `ws2_32.lib` (inert until wired).

### 7.7 Relay session layer + unit tests (2026-10-24)

- `games/outlast-2/src/coop_session.h/.c`: pure relay logic, no sockets and no game code.
  - join/leave: `coop_session_add` (returns existing slot for a known address), `find`,
    `remove`; slots `0..COOP_MAX_PLAYERS-1`, freed slots are reused.
  - liveness: `touch` + `expire(now, timeout)` with wraparound-safe `now - last_seen`
    arithmetic (time is injected, so tests are deterministic).
  - fan-out: `relay_targets(sender_slot, ...)` never returns the sender's own address.
  - late join: `update_state` stores each player's last `CoopPlayerState`;
    `snapshot(exclude_slot, ...)` returns them for the joining client.
- `games/outlast-2/tests/session_test.c`: 5 groups — join/find/duplicate join,
  capacity + slot reuse, touch/expire, relay fan-out targets, late-join snapshot
  (including the output cap and "joined but no state yet" case).
- Result: **PASS — 5 check groups** (2026-10-24), alongside the existing protocol (5)
  and UDP loopback (3) groups: `games/outlast-2/tests/run.bat`.
- Proxy DLL build now compiles `coop_session.c` as well (inert until wired).

### 7.8 Relay pump + end-to-end UDP tests (2026-10-24)

- `games/outlast-2/src/coop_relay.h/.c`: ties `coop_net` to `coop_session`.
  - host: `pump(now_ms)` receives up to 32 packets per call, validates framing, tracks
    players, relays state/events to the others, answers `JOIN` with a late-join snapshot,
    and expires players silent for `COOP_RELAY_TIMEOUT_MS` (5 s).
  - client: `join`, `send_state`, `send_event`, `pump`; every accepted packet goes to a
    sink callback (`CoopRelaySink`, gets the raw packet, source address and slot).
  - authority detail: the host rewrites a `PLAYER_STATE` sender's `slot` byte to the
    host-assigned slot, so a client cannot impersonate another player.
  - stats counters (`rx_packets`, `rx_invalid`, `rx_unknown`, `rx_full`, `tx_relay`,
    `tx_snapshot`, `expired`) for QA assertions and in-game logging.
- `coop_protocol.h`: added `coop_packet_size()` and `coop_packet_validate()`
  (exact-length, magic/version/type checks).
- `games/outlast-2/tests/relay_test.c`: real UDP loopback, real host + two clients —
  (1) join, state relay, late-join snapshot of earlier state, (2) host-controlled slot
  assignment against a lying sender, (3) packets from an address that never joined are
  ignored, (4) malformed packets (magic / version / type / length) rejected,
  (5) silent player expiry at the timeout boundary.
- Result: **PASS — 5 check groups** (2026-10-24). All suites: protocol 5 + UDP loopback 3
  + session 5 + relay 5 = **18 groups** via `games/outlast-2/tests/run.bat`.
- Freestanding build note: test frames over 4 KB pull in `__chkstk` from the CRT, so the
  relay test keeps its `TestNet` fixture `static` (`.bss`) instead of on the stack.
- Proxy DLL build now compiles `coop_relay.c` as well (inert until wired).
- Live validation (2026-10-24): the rebuilt DLL with the transport + session + relay
  linked in installs cleanly in the isolated install — prologue verified, 14-byte jump
  applied, process alive (~31 s CPU / 95 threads over the run), no crash. First
  `ProcessEvent` capture is still the open gate (see §6).

### 7.9 Puppet interpolation + fake-network tests (2026-10-24)

- `games/outlast-2/src/coop_interp.h/.c`: remote-puppet playback, no game code.
  - per-slot jitter buffer of 8 snapshots, sorted by arrival time; playback runs
    `COOP_INTERP_DELAY_MS` (100 ms) behind the newest arrival so one late packet does
    not stutter the puppet;
  - position/velocity lerped between the two bracketing snapshots, yaw/pitch along the
    shortest arc (`wrap180`), discrete flags/stamina taken from the older snapshot so
    they do not flicker mid-blend;
  - before the oldest snapshot it holds the oldest; after the newest it extrapolates on
    the last velocity for at most `COOP_INTERP_MAX_EXTRAP_MS` (150 ms) and then holds;
  - a step larger than `COOP_INTERP_TELEPORT_DIST` (800 UE3 units ≈ 8 m) snaps to the
    newer snapshot (streaming load, scripted move) instead of sliding through geometry;
  - stale/duplicate/out-of-order seq are rejected, so playback never rewinds.
- `games/outlast-2/tests/interp_test.c`: 6 groups — lerp + playback delay, stale/reorder
  rejection, loss gaps + extrapolation cap, teleport snapping, shortest-arc angles, and a
  scripted lossy/reordered fake network (25 Hz sender, every 3rd packet dropped, pair
  swaps, duplicates, 0-10 ms jitter) asserting no NaN, no rewind, bounded per-frame step
  and that the puppet tracks the sender to the end of the run.
- Result: **PASS — 6 check groups** (2026-10-24). All suites: protocol 5 + UDP loopback 3
  + session 5 + relay 5 + interp 6 = **24 groups** via `games/outlast-2/tests/run.bat`.
- Test-data bug found on the first run: the fake network timestamped packets by their
  index in the *surviving* array instead of their 25 Hz send time, which made the
  simulated sender appear to move at 1.5x speed and tripped the per-frame step bound.
  Recv times now come from the tick's send time.
- `coop_interp.c` defines `_fltused` (float use in a `/NODEFAULTLIB` DLL).
- Proxy DLL build now compiles `coop_interp.c` as well (inert until wired).
- Live validation (2026-10-24): the rebuilt DLL with transport + session + relay +
  interpolation linked in installs cleanly — prologue verified, jump applied, process
  alive (~33 s CPU / 95 threads over the run), no crash.

