# Need for Speed: Underground 2 (2004)

| | |
|---|---|
| Engine | EAGL (EA Black Box, in-house); 32-bit x86, D3D9 |
| Versions tested | PC 1.2 build, `speed2.exe` 4,800,512 bytes, SHA-256 `bc34932b36bb19194000690ec26edf964176856c31fb0978e01e61ee814eee9d` |
| Projects | NFSU2 Co-op (this project; source in `nfsu2-coop/`, not published as its own repo yet) |
| QA level | none in-game / automated protocol + session suite |
| Last updated | 2026-10-27 |

## Where things stand

Career co-op does not exist for this game and nothing was there to revive, so this is
path 2: build from zero. The result today is a working host/join stack and race director
with a shared career ledger, a mirrored traffic/AI feed, a D3D9 HUD, and an automated
suite that drives a real host and real clients over UDP (including 25 % loss, duplicates
and a 1.2 s partition). The in-game side is deliberately split: networking, races,
standings and career work with no addresses at all; reading the player car and the
traffic list needs a `[Bindings]` table for the target build, and drawing other players'
cars needs a render hook that is not written yet. The game crashed at `0x3DBBBA` on the first machine (it did so with a clean install
too), but the owner fixed the missing files and two instances now run. With both
instances in game: host + guest connect over UDP, the HUD shows 2 players and 15 ms RTT,
and the guest receives the host car's position and spawns a puppet (`puppets 1`). The
local car's position is now read on the verified build (car array `0x89CCF8`, index 0,
position `+0x50`); traffic has no binding yet, and remote cars are not drawn.

## Existing work

- No career or free-roam co-op mod for NFSU2 exists in the directory, on GitHub or on
  the usual NFS mod sites at the time of writing.
- The game itself ships **LAN quick race** (`server.dll`, `server.cfg`, `UI_PC_LAN_*`
  menus, `MMLAN`) and EA Nation online (`nfs.2005`, `EAN08` strings). Career is
  single-player: it never goes through that netcode.
- Modding ecosystem worth building on: **Widescreen Fix** (also the ASI loader here,
  `dinput8.dll`), **Crashing Foreign Cars** by Chrystian Farias (FRS Mods) — its docs
  publish the function anchors used by the probe — and the usual NFS tools
  (NFS-VltEd, Binary, nfs2tools) for `.bin`/`.bun` data.

## How the game works (for co-op)

- `speed2.exe`: PE32, image base `0x400000`, `.text` at `0x1000`, entry `0x0035BCC7`.
  Single main thread; D3D9 rendering; a 32-bit address space, so the mod is 32-bit.
- Career: profile save, event list with stage gating, money/reputation, sponsors,
  magazines, DVDs, outruns, unique parts. Events are started from the world map or the
  garage; results are applied in the front end.
- World: the engine streams districts; traffic and AI racers are engine-owned objects
  with their own spawn rules. There is no scripting layer to hook.
- Saves are opaque profiles beside the install; the mod does not touch them.

Because traffic and AI cars are owned by one engine instance, the co-op design mirrors
them from the host instead of re-simulating on every peer. That removes the classic
"two different traffic streams" desync at the cost of clients not being able to take
over a car's physics.

## What we changed

Built a complete session layer (C++17, portable, no engine types) plus a thin game
adapter:

- **Host authority**: one host decides the race director state, the career ledger and the
  traffic/AI set; each player owns their own car only.
- **Race director**: lobby → countdown → racing → finished, checkpoint validation
  (one step forward, never past the end), places computed from finish *time* so packet
  reordering cannot change the podium, disconnect mid-race = DNF.
- **Shared career ledger**: events are idempotent keys; cash/rep are a pure function of
  the key, so a duplicated packet or a second F7 press cannot pay twice. Late joiners get
  a snapshot.
- **Traffic/AI mirror**: the host reads its own cars (handle + model + transform), assigns
  net ids, and streams them with distance culling; clients spawn puppets and hide their
  local traffic.
- **Digest + repair**: race and career hashes every 2 s; a mismatch requests a full
  snapshot instead of drifting silently.
- **In-game plumbing**: PE scanner (`coop-probe` verified 10/10 anchors and found
  `Player::ClearZone` by signature), safe memory reads via `VirtualQuery`, a game-thread
  command queue, D3D9 `EndScene` hook via MinHook, ImGui HUD, `.ini` config, log.

Tests that prove it:

| Suite | What it proves |
|---|---|
| `core-tests` (14 tests, 120 checks) | wire format survives truncation, race rules, DNF, idempotent career, digests, packet framing/CRC |
| `sync-tests` (6, 70) | two/three real clients on loopback: visibility, traffic mirror, full race, career once, late join, host stop |
| `net-tests` (2, 24) | same cluster through a chaos proxy: 25 % loss + 10 % duplicates converges; 1.2 s partition recovers |
| `run_all.ps1` e2e | `coopd.exe` + two `coop-bot.exe`: race finished, career applied once, identical digests |

## QA

```powershell
cd nfsu2-coop
.\toolchain\install.ps1
.\build.ps1
.\tests\run_all.ps1        # prints ALL OK
```

Last run 2026-10-27: 22 tests / 214 checks, ALL OK; e2e career digest identical on both
bots; probe 10/10.

In-game (two instances, 2026-10-27): `.asi` loaded, `speed2.exe` parsed, 10/10 anchors
resolved, `EndScene` hooked, host listening on udp/27970, guest welcomed as slot 1,
HUD 2 players / 15 ms RTT, guest `puppets 1` (host car position received). Car position
read from `[0x89CCF8]` entry 0 at `+0x50`; two instances report positions ~8 m apart.
After the owner moved the cars, the host HUD read `Test 30 m E` and the guest
`Player 30 m W` (same distance, opposite bearing, `puppets 1` on both), and a third
headless client received both cars with coordinates matching `[0x89CCF8]+0x50` in each
process: `(944.70, -1190.17, 14.12)` and `(939.47, -1266.38, 13.98)`.
Remote cars are now drawn by borrowing one of the game''s own render-list cars
(`[0x89CCF8]` entries 1..3): at `CarRenderInfo::Render` the borrowed car''s position and
basis are replaced with the remote player''s, and `EndScene` restores them. Model swapping was attempted and does not work: swapping the car object pointer (`+0x00`)
crashes the game, and swapping `CarRenderInfo` `+0x04`/`+0x08` (or pointing `[RI]` at a
clone of the local car) leaves the borrowed car with its own traffic model (Skyline,
Civic). The model is bound when the car is spawned, not read from these fields at draw
time, so a real fix needs the spawn function.
Not verified in game: traffic mirror (no binding), automatic race completion (no
checkpoint binding; the director aborts after 20 min).

## How to play

1. Build and install: `nfsu2-coop\install.ps1 -GameDir "<game dir>"`.
2. Host: `Host=1` in `scripts\nfsu2coop.ini`; guests: `Host=0`, `Server=<host ip>`.
3. In game: F6 opens the event (host), F5 readies (guests), race and standings sync;
   F7 reports a career event done.
4. Full reference: `nfsu2-coop/docs/INSTALL.ru.md` (Russian; code comments are English).

## Known issues

- No memory bindings by default: car/traffic sync off, HUD says so.
- Remote cars are counted but not drawn (needs `CarRenderInfo::Render` work).
- The race is started by each player from the game's own menu; `StartEvent` is optional.
- Career lives in the session, not in the profile save file.
- No Steam relay / NAT traversal yet; direct IP only.
- The game crashes in this test environment even unmodded.

## Next steps

Bindings for car and traffic on a working install → render-hook puppets → `StartEvent`
and event-completion capture → relay → profile persistence. Details in
`nfsu2-coop/docs/KNOWN-ISSUES.ru.md`.







