# Alien: Isolation (2014)

| | |
|---|---|
| Engine | Creative Assembly proprietary (CATHODE) |
| Versions tested | Steam, AI.exe SHA-256 recorded in the test rig; guard bytes at file offset 0x6534D0 |
| Projects | AI co-op (build from zero, this fork) |
| QA level | automated (two-instance harness, see below) |
| Last updated | 2026-10-23 |

## Where things stand

Presence co-op only: two players each run their own instance of the game and see
the other as a body-shaped avatar in the world, at the right place, size and
facing. Positions sync over UDP at 20 Hz with snapshot interpolation and a 1.5 s
peer timeout. The two worlds are **not** shared — the alien, doors, items and
story are per-instance, and both players must load the same mission save to be
in the same place. Occlusion does not work (the avatar draws through walls) and
there is no join/host UI beyond scripts.

## Existing work

- **Alien Isolation co-op (Dropper7894)** — announced prototype aimed at
  Survivor Mode; no campaign support confirmed. Listed in `data/directory.yaml`.
- No usable campaign co-op mod exists; this project builds from zero.

## How the game works (for co-op)

- Engine namespace `CATHODE`; retail netcode compiled out. No existing co-op to
  revive.
- The player record sits at `*(uint32_t*)0x016F0C88 -> +0x58`, with position
  floats at `blk+0x79C/0x7C4/0x7EC` and a facing field at `blk+0x790` that is
  roughly the camera's left axis (not the view direction).
- The camera matrix is uploaded as two blocks in one 576-byte constant buffer
  bound to the vertex shader: **view at 0x040, projection at 0x080**, camera
  position verbatim at 0x140 row 2. The view matrix is column-vector form
  (right/up/forward rows, translation in the last column, t = -R*cam). Near
  0.05, far 1000, vertical FOV 47 deg.
- The camera sits on the player record (first person, distance ~0), so the
  distance gate is 0.75 m and any look-angle gate is wrong.
- Saves go through Steam Remote Storage; the mock redirects them to
  `steam_remote/<profile>/`.
- Never call the entity-property getter (`FUN_008d63c0`): its byte counter
  overflows at 256 and kills the engine.

## What we changed

- **Mock `steam_api.dll`** provides the Steam interfaces the game dereferences,
  enables per-profile save directories, and loads `coopnet.dll` in-process via
  `AIC_PROBE_DLL` (gated by `AIC_PROBE_DELAY_MS`).
- **Narrow exe gate patch** (`patch_gate.py`): two conditional jumps in the
  Steam boot gate become unconditional, so a second instance can boot without
  skipping `SteamAPI_Init()` (skipping it crashes at RVA 0x664C21).
- **Constant-buffer camera detector** (`camera_detect.h`): 36 unit tests in
  `convtest.exe`, finds exactly one buffer in 4096 on real dumps via
  `camfind.exe`. The store publishes both blocks into a double buffer on every
  fill, so the overlay is lock-free (an earlier seqlock retry silently dropped
  most frames).
- **Avatar**: per-frame position and camera-forward yaw over UDP (`AvatarWire`,
  18 bytes), snapshot interpolation one send interval back, 1.5 s peer timeout,
  and a screen-space body billboard sized from real body proportions. An edge
  arrow with the range is drawn when the peer is off screen or behind the
  camera.
- **Tooling**: overlay-safe window resolution (`wingame.ps1`), client-area
  screenshots, pixel counting, OCR-verified menu navigation, and a two-instance
  driver (`drive_level.ps1`) that keeps both windows side by side on one display.

## QA

- `convtest.exe` — 36 checks on the camera detector (all pass).
- `stage5/test/fakepeer.ps1` — scripted peer for projection and peer-learning
  tests.
- Two-instance harness on a 3440x1440 rig: avatar projection checked against
  hand arithmetic (971 px at 2.16 m vs 972; 230 px wide vs 229; 1040 px at
  2.02 m), interpolation (drawn point trails the newest packet by 0.013 m at
  32 ms), peer timeout (paints climb while drawn stays fixed), and peer
  address learning (both transitions logged, self-heals).
- `inetntoa_test.c` proves the `inet_ntoa` static-buffer bug and its fix.

## How to play

See `stage5/pack/README.md` for install, host, join and status commands.
Summary: `install.ps1` equips a copy; `local2p.ps1` runs two instances side by
side; `host.ps1` / `join.ps1 -PeerHost <ip>` connect two PCs. Both players must
load the same mission save.

## Known issues

- The two players are in separate worlds (no shared enemies, doors, items or
  story progression).
- The avatar draws through walls (depth buffer is cleared before Present; a
  mid-frame hook or map-collision ray cast would be needed).
- No occlusion, no in-game join/host UI, no animation states beyond standing.
- Steam overlay, achievements and cloud saves do not work while the mock
  `steam_api.dll` is installed; saves go to `steam_remote/<profile>/`.

## Next steps

- Find a hook point between scene rendering and the depth clear, or ray-cast
  against the map collision data, to get real occlusion.
- Shared-world gameplay: spawn the remote player as an in-world entity so the
  alien and doors can react to them.
- Animation state (walk/run/crouch) from the player record.
- A join/host flow in the game's own UI rather than scripts.
