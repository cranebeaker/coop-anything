# NFSU2 co-op — working notes

## Observations (verified here)

- `speed2.exe` is PE32, machine `0x014C`, image base `0x400000`, entry `0x0035BCC7`,
  `.text` `0x1000`–`0x383000`, size 4,800,512 bytes, SHA-256
  `bc34932b36bb19194000690ec26edf964176856c31fb0978e01e61ee814eee9d`.
- All ten function anchors published in the Crashing Foreign Cars README resolve inside
  `.text` at the documented VAs on this build:
  `0x620AE0`, `0x60D7F0`, `0x637D70`, `0x5FFE70`, `0x55B5C0`, `0x54E6E0`, `0x558790`,
  `0x5289F0`, `0x55C230`, `0x4B0140`. `coop-probe` verifies them and finds
  `Player::ClearZone` by signature (`53 56 8B F1 8B 46 58 33 DB 3B C3`).
- The game ships LAN quick race: strings `UI_PC_LAN_ServerSelect.fng`, `LANServerSelect`,
  `MMLAN`, `UI_PC_LAN.fng`, plus `server.dll` and `server.cfg` (`ADDR=%%bind(...)`).
  Online strings: `nfs.2005`, `EAN08`.
- D3D9: our `EndScene` hook installs and ImGui initialises in the real process.
- `speed2.exe` crashes with `0xC0000005` at `0x3DBBBA` on this machine **without any mod**
  (also with `/s`). Faulting module is `speed2.exe` itself (Windows Error Reporting).
- Existing install already has an ASI loader: `dinput8.dll` (Ultimate ASI Loader from the
  Widescreen Fix) and `scripts\NFSUnderground2.WidescreenFix.asi`.

## In-game (second session, 2026-10-27)

- Owner fixed the missing files; two installs run: `<...>\game` (host, slot 0) and
  `<...>\game test player` (guest, slot 1).
- Both load `NFSU2Coop.asi`; logs (one per PID now) show 10/10 anchors, built-in
  bindings, EndScene hook, host listening, guest `welcome slot=1`.
- Guest HUD: 2 players, rtt 15–16 ms, `puppets 1` — the host car position arrives and a
  puppet is created.
- Car array found by hooking `CarRenderInfo::Render` with an injected probe (`tap.dll`):
  `this` → `[this]` = car object; `[0x89CCF8]` in .data holds four Car* in free roam;
  position `+0x50` (second, near-identical triple at `+0x60`), 3x3 basis at `+0x90`.
  Host and guest report `(913.8, -1213.1, 13.4)` and `(919.5, -1218.4, 13.9)`.
- Not found: traffic/AI list, speed/gear, checkpoint progress. Race completion still
  needs a checkpoint binding or a finish-screen hook.

## Derived

- The documented anchors belong to this exact build; offsets from the same mod's docs
  (`CarRenderInfo::Render` etc.) can be used without re-deriving addresses.
- Career is not part of the LAN/online path: the LAN code covers quick race only, so
  career co-op needs its own session layer (what this project does).
- Traffic and AI cars are engine-owned in one process; a deterministic all-peers
  simulation would need engine spawn access, so mirroring from the host is the cheaper
  correct design.

## Guesses (not verified)

- `0x3DBBBA` is probably an early render/device path failing in this VM (no confirmed GPU
  profile). It happens before any input.
- The player car pointer is reachable from `Player::ClearZone`'s `this`; not yet tried on
  this machine because the game does not stay alive.
- Event completion is observable in `MenuScreen::BaseNotify`; unverified.

## Unknowns

- Which object owns traffic cars, and whether the traffic array is `struct[]` or
  `ptr[]` (the adapter assumes a flat array with `TrafficStride`).
- Whether `StartEvent` can be called safely outside the menu's own flow.
- Save-file layout for cash/rep (not touched on purpose).

## Attempts

- Launch `speed2.exe` clean: crash `0xC0000005` @ `0x3DBBBA` after a few seconds.
- Launch with `/s`: same crash.
- Launch with `NFSU2Coop.asi`: same crash, but the log proves module parse, 10/10
  anchors, `EndScene` hook, ImGui init, and the client connecting.
- Two-process test (`coopd` + 2 `coop-bot`): race ran, both finished, career applied once,
  identical digests.

## Next commands for the next agent

```powershell
cd nfsu2-coop
.\build\coop-probe.exe --exe "<game>\speed2.exe" --json probe-report.json
.\tests\run_all.ps1
```

Then, on a machine where the game runs: attach x64dbg, break at `0x5FFE70`, and fill
`[Bindings]` (see `nfsu2-coop/docs/GAME-BINDINGS.ru.md`).


## Remote car model hunt (2026-10-27, later session)

### Observations (verified)

- Borrowed render-list car is drawn at the remote position, but keeps its own traffic
  model (`SKYLINE`/Civic), never the player''s `CELICA`.
- Car object header: `+0x0C` = pointer to the model name string; `+0x04` = per-model
  instance-ish pointer; `+0x08` = car index used by the registry.
- Registry writer: `0x5F0DCD` `mov [0x89CCF8+edi*4],eax` (index from `car->+0x08`), then
  the same index fills `0x89CD50`, `0x89CDA8`, `0x89CE00`, `0x89CE58`, `0x89CEB0` with
  `car+0x28/+0x34/+0x24/+0x2C` pointers. Despawn/clear path at `0x5F4545`.
- `CarRenderInfo::Render` caller at `0x6311xx` passes per-car object fields `+0x900`
  (object), `+0x960` (`lea [esp+0x54]`), `+0xa60` (`lea [esp+0xa0]`) — these are the
  per-car render/model arguments, not the header fields we were swapping.
- Class-name strings exist and are referenced from code: `StreamingTrafficCar`
  (`0x784750`, ref `0x40BD47`), `StreamingTrafficCarManager` (`0x785080`, ref `0x412728`),
  `CarController` (`0x78445C`, refs `0x404827/0x40483C`), `TrafficController` (`0x785B98`),
  `TrafficMover` (`0x78458C`).

### Attempts (all failed to change the model)

- Swap car `+0x00` (model object) permanently: crash at `0x1C2C9A`; transient swap in the
  render hook: also crashes.
- Swap car `+0x04`, `+0x08`, `+0x0C`, `+0x10`, `+0x14`, `+0x18`, `+0x1C`, `+0x20`, `+0x2C`
  for the frame (restored at `EndScene`): no visual change, stable.
- Swap `CarRenderInfo` `+0x04`/`+0x08` (also as a default for a while): no visual change;
  it was also what crashed the guest instance after ~3 s, so it is off by default now.
- Point `[CarRenderInfo]` at a clone of the local car object (copied with a
  `VirtualQuery`-sized copy after fixing a short-read bug): no visual change.
- `x64dbg headless -pid` + `-cf` script with `bp 0x5F0DCD` /
  `bpc "[0x89CCF8+edi*4]==0"`: script runs (`GUI_SCRIPT_*` messages), but headless does
  not emit `log` output (GUI callbacks are `[TODO] Not implemented`), and stopping the
  debugger takes the game process down with it.
- In-process hardware watchpoints (DR0–DR3 on the four slots + VEH): armed
  (`dr7=DDDD0055`), the game crashed before the first log line.

### Derived

- The model is bound when the car is created, not read from the car header at draw time;
  render-time writes to that header cannot change it.
- Headless x64dbg is unusable for this loop: no log output plus an unsafe detach.
- `0x89CCF8` is the render registry, not the spawn list; the spawn path is still unknown.

### Unknowns

- The function that creates a car and selects its model (candidate: around the
  `StreamingTrafficCar` / `StreamingTrafficCarManager` class code).
- Whether a render-copy object exists that holds the model the renderer actually uses.
- Whether `+0x900/+0x960/+0xA60` of the render object are the model switch to pursue.

### Next

- Build the in-game bridge from `playbook/in-game-bridge.md` (loopback TCP, main-thread
  queue) so fields can be written and observed without restarts or attach/detach.
- Use the bridge to write the `+0x900/+0x960/+0xA60` candidates and see which one the
  renderer reads back.
- If that fails, Ghidra headless decompile-all around the `StreamingTrafficCar` class
  references and find the spawn/model-selection function; then call it with the
  player''s model id.

## In-game bridge (2026-10-27, end of session)

- The plugin embeds `coop-anything/tools/bridge/include/coopbridge/server.h` and serves
  the standard protocol plus `mem.read`, `mem.write`, `car.slots`, `car.name`
  (ini `[Debug] BridgePort=27971`, pumped from the EndScene hook). Verified with
  `tools/bridge/bridge_client.py`: `ping` (frames, mainLoopAgeMs), `version`, `methods`,
  `state` (pid, four cars, localPos, localModel), `car.slots`, `mem.read`.
- Bridge pauses with the game when the window loses focus: `mainLoopAgeMs` ~70 s means
  paused, requests answer `-32001` until the game pumps again.
- Object graph read live: car `+0x00` and `+0x04` point at objects whose `+0x0C` string is
  a traffic model name (`4DR_SEDAN02`, `PICKUP`, `PARCELVAN`, `TAXI`), not the car''s own
  model (`CELICA`, `SUPRA`, `GTO`, `RSX`). So those two pointers are not the model
  switch to flip.
- Next: use the bridge to walk the graph for a pointer whose `+0x0C` equals the car''s own
  model name, and swap that one live (bridge write, screenshot, revert) instead of
  restarting.
