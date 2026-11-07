# Space Rangers HD: A War Apart — Co-op Mod

Co-op planetary battles for Space Rangers HD: A War Apart (Steam appid 214730, build 20648864).

## What works

Two-player cooperative planetary battles in the MatrixGame engine. Host-authoritative: the host runs the full battle simulation; the guest sends orders and receives pose replication.

- **Channel A (reliable, channel 0):** handshake (HELLO/WELCOME), map verification, guest orders → host
- **Channel B (unreliable sequenced, channel 1):** per-tick pose snapshots from host → guest
- Systems: ENet over UDP (localhost:25333 for tests), /Zp1 struct packing for ABI parity

## What does NOT work yet

- Full campaign / galaxy map (only planet battles, MVP)
- Checkpoint sync (triggers fire for both players regardless of position)
- Map enemy ownership (each client simulates its own copy; use SyncEnemies=False to avoid duplicates)
- Pushables, cutscenes, pickups
- Two PCs over the internet (tested on one machine, localhost only)

## How to run

### Build

```cmd
cd local\MatrixGame
cmake -S MatrixGame -B build -A Win32 -DMATRIXGAME_BUILD_DLL=ON
cmake --build build --config Release
```

Copy `build\MatrixGame\Release\MatrixGame.dll` to `local\test-install\MatrixGame.dll` (overwrite the stock DLL).

### Run (one PC, two windows)

**Window 1 — Host:**

```cmd
cd local\test-install
set SR_COOP=host
set SR_COOP_PORT=25333
set SR_LOG_PREFIX=m2h_
set SR_SMOKE_HEADLESS=1
set SR_SMOKE_BATTLE=1
Rangers.exe Matrix\Map\Boarding.cmap
```

**Window 2 — Guest:**

```cmd
cd local\test-install
set SR_COOP=guest
set SR_COOP_SIDE=2
set SR_COOP_ADDR=127.0.0.1
set SR_LOG_PREFIX=m2g_
set SR_SMOKE_HEADLESS=1
set SR_SMOKE_BATTLE=1
Rangers.exe Matrix\Map\Boarding.cmap
```

Wait 5–8 seconds after starting the host before launching the guest.

## Tests

Run all three smoke gates from `local\analysis\`:

```powershell
powershell -ExecutionPolicy Bypass -File smoke_matrixgame.ps1
powershell -ExecutionPolicy Bypass -File smoke_coop_m1.ps1
powershell -ExecutionPolicy Bypass -File smoke_coop_m2.ps1
```

All must PASS.

## Project status

Alpha. Core net layer (handshake + poses + orders) verified on one PC with two client instances. Next: real battle via Rangers.exe + two PCs over LAN/internet, then upstream-PR to murgesku/MatrixGame.

## Sources

- Steam: https://store.steampowered.com/app/214730/
- Engine source: https://github.com/murgesku/MatrixGame
- Build docs: games/space-rangers-hd-a-war-apart/HANDOFF.md
- Full notes: games/space-rangers-hd-a-war-apart/NOTES.md