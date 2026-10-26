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
