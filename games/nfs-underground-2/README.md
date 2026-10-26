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
cars needs a render hook that is not written yet. The game could not be played on this
machine (it crashes at `0x3DBBBA` with a clean install too), so nothing in-game is
claimed beyond the plugin loading, the PE being parsed, 10/10 documented anchors
resolving and the HUD hook coming up.

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

In-game: `.asi` loaded by `dinput8.dll`, `speed2.exe` parsed, 10/10 anchors resolved,
`EndScene` hooked, ImGui initialised. The game then crashed at `0x3DBBBA` — the same
crash happens with the mod removed, so it is an environment problem (no verified
playthrough possible here).

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

