# Resident Evil 2 (2019 remake) — agent notes

Game: Resident Evil 2 (2019), Steam app 883710, build 11636119, standard edition.
Engine: RE Engine. Identity recorded in the fork's `local/` (private; not for PR).

## Goal

Owner wants **online co-op with friends on separate PCs**. The directory only shows
local-only mods for this game, so this is a "build the missing online layer" project
(path 2, but standing on path-1 work).

## What exists (researched 2026-10-23)

### Local co-op mods (both REFramework Lua based)

- **Local Couch Co-op / "HUNK's Friend" (re2coop)** by Shopcreeper01
  (https://www.nexusmods.com/residentevil22019/mods/2938) — status `playable`, 2 players.
  Architecture confirmed from the installed copy on this PC:
  - `reframework/autorun/re2coop_main.lua` — 29,325 lines, v1.5.0.
  - `reframework/plugins/re2coop_pads.dll` — native input plugin.
  - Single process: spawns a second real pawn (prefab `ObjectRoot/Prefab/Character/Player/pl4000.pfb`),
    routes a second controller to it via hooks on InputSystem update methods.
  - Split-screen cameras (side-by-side / top-bottom), per-player HUD, shared world,
    separate inventories, revive/downed handling, AI-partner mode, coins a shared world so
    no serialization layer exists.
  - Hard requirements: REFramework **v1.5.9.1 RE2 build from GitHub** (REFramework installed
    on this PC: version v1.5.9.1, commit d4611aef1d63effd3a736bdf9404ea702428017d).
  - Game data versions: Non-RT build runs TDB 66, RT build TDB 70; mod has `st.NON_RT` gates
    (`sdk.get_tdb_version()`).
  - Source not published on GitHub (searched "re2coop", 2026-10-23: 0 repos). Tagged
    "AI Assisted" on Nexus.
- **RE2 Party Mode** by secretspook (https://www.nexusmods.com/residentevil22019/mods/3024)
  — v0.5.0, 18 Oct 2026, `alpha`, **local 2–4 player split-screen**, fork of the above
  (author: "I used his Local Couch Co-op as a basis"). Same REFramework Lua + `REPM_pads.dll`
  pattern. Requires REFramework v1.5.9.1. No online plans voiced by the author (checked
  description + posts, 2026-10-23). Not installed on this PC.

### Online co-op on the same engine (proves it is doable)

- **RE4 Remake Multiplayer Mod** by claybtv (https://www.nexusmods.com/residentevil42023/mods/6539)
  — 2-player **online** campaign co-op on RE Engine: enemies, story events, doors, switches
  synced over central relay servers + direct connect (UDP 39000). Closed source; ~70 open
  bugs; known issues: gimmick-area scans cause frame drops, remote enemies choppy
  (interpolation), pause breaks sessions. Proves the path; its pain list is our risk list.
- **DD2gether** for Dragon's Dogma 2 (https://github.com/But-Frog/dd2-together-release,
  binaries only) — up to 4 players, REFramework Lua (`reframework/autorun/coop/`) +
  LuaSocket transport + Steam lobbies with invite flow ("no port forwarding"). Syncs
  attacks/hitboxes/damage, enemies, NPC/quest state. Closest architectural blueprint:
  **REF Lua + small transport + game's own state machines**.
- **Steam Spacewar relay recipe** (this repo, playbook/networking-steam-spacewar.md):
  Steamworks flat API + `steam_appid.txt = 480`, ISteamMatchmaking lobbies +
  ISteamNetworkingMessages over Valve relay. No port forwarding, no extra purchases.
  Tested recipe from other games in this repo (Jericho 2026-10-21).

### Mod downloads need Nexus auth

Nexus files (Party Mode zip, anything new) require the owner's Nexus account; the agent
cannot fetch mod zips anonymously. `re2coop` mod files are readable locally — treat as
installed, don't modify them (never touch the owner's install).

## Online architecture (proposed, not yet built)

Layered on the proven local architecture:

1. **Game-side (Lua, REFramework):** reuse re2coop/Party-Mode style pawn spawn, per-player
   camera/room loading, inventory surgery. In online mode the local injector feeds P2..Pn
   pawns not from pads but from **network input buffers**, and streams our pawn's pose +
   actions out.
2. **Net layer (C++ REFramework plugin — reframework API in praydog/REFramework, docs
   refdocs.praydog.com):** transport = Steam Networking Sockets (Spacewar relay recipe) so
   friends host/join via Steam invites without port forwarding.
3. **Authority:** host = the game's own P1 process, like the AI-partner mode but driven by
   remote players' inputs. Enemies/doors/story stay in the host process — one authoritative
   world, avoiding the two-locked-worlds story desync class first. Interpolation for remote
   pawns (see claybtv choppy-enemy lesson: budget for interpolation early).
4. **QA:** two game copies + loopback first (per playbook multi-instance tests), then chaos
   proxy (`tools/chaos`), then the rest of the sync tool suite.

## Observations (this PC, verified)

- RE2 install lives at `E:\SteamLibrary\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2`
  (installdir per appmanifest); `E:\SteamLibrary\common` is missing/hidden — the folder
  oddity doesn't affect the game.
- re2coop has been used on this install (data logs exist: `re2coop_p2_log.txt`,
  `re2coop_hunk_config.json`, pads log); REFramework log shows a run at 2026-10-23 01:40.
- Non-RT re2.exe hashes: 6CAAA815BF9E95F8A841BC81FDF29FEC55E836C87BAB8A76D55C69BEF9ADA941
  (recorded 2026-10-23).

## Derived / guesses (not verified - labelled, per AGENTS.md rule 4)

- Guess: the re2coop "AI partner" mode (input-driven local pawn) is the correct injection
  point for remote players — that is inferred from the inline comments about input
  routing, not proven by tests yet.
- Unknown: how re2coop keeps the second pawn's room loaded ("keep_p2_area") works
  internally; Party Mode advertises per-player rooms, RE2 may allow several active rooms.
- Unknown: re2coop license — source is on Nexus as a packaged lua; distribution/derivative
  terms of the mod need checking before we build on its code.

## Workspace build environment (phase 2, done 2026-10-23)

Verified with `python tools/test_all.py`: **all 15 sync tools now print all green**.
On the first pass the following three failures appeared on this machine; all are
tool-vs-newer-environment issues, which we are going to submit via PR:

- **flight** hard-coded `["-G", "Visual Studio 17 2022", "-A", "x64"]` (`run_tests.py`).
  When VS 2022 is absent, it falls back to no generator → CMake chooses NMake →
  `nmake --parallel 1` → U1065. On this machine there is only BuildTools 18 (VS 2026).
  Fix: probe `Visual Studio 18 2026` → `Visual Studio 17 2022` → `Ninja`, and detect
  the generator via the `CMAKE_GENERATOR` line in the build cache (the compiler path
  in the cache also contains "Visual Studio" and confuses the old check).
- **flight (CommandOracle)** parsed via `shlex.split(posix=False)` on Windows, which
  does not remove MSVC quoting if the path contains a space → `python -I "<path>"`.
  Fix: our own `_msvcrt_split` parser (MS CRT rules) when `os.name == 'nt'`.
- **mutants (coop_mut.h)**: the test asserted that the literal like `add_wrong_operator`
  is in the test binary in contiguous bytes. MSVC 2026 folds the literales of
  `is("name")` comparisons into the instruction as 64-bit immediates
  (`add_wron`+`g_operat`+`or` interleaved), leaving nothing to find. Fix: `COOP_MUT_NOINLINE`
  (`__declspec(noinline)` / `__attribute__((noinline))`) in `coop_mut::is()` so the constant
  survives contiguous in .rdata (and additionally smaller binaries).

Environment steps (PowerShell) that are required for running it, recorded in `local/owner-state.md`:

1. `winget install GitHub.cli Kitware.CMake Ninja-build.Ninja` (already installed here).
2. Prefix PATH with `C:\Program Files\CMake\bin` + winget `Links` of Ninja (session-specific PATH).
3. Wrap the whole session with
   `cmd /c "chcp 65001 > NUL && <VsDevCmd> -arch=x64 && python tools\test_all.py"`.
   Important: it must be the banner codepage **before** Python starts (UTF-8).
   The repo calls the tools with `-I`; `-I` discards the `PYTHONIOENCODING` environment variable
   and then `print()` under cp1252 crashes on Unicode characters in test notes. Therefore we
   launched here without `-I` (only for the encoding issue, not for isolation reasons).
4. A deleted build directory (`${env:TEMP}\cofr-flight-build`, `$env:TEMP\coop-mutants-build-*`)
   forces the new generator.

## Sync protocol prototype (phase 5 started, 2026-10-23)

`tools/fuzz/re2_target.cpp` (in the fork; temporary home until the mod has its own repo) wraps
the protocol MVP we plan to transcribe into the RE2 mod, as a coopfuzz target (test runs without
the game, verified with the framework's own oracles):

- Model: 2-4 machines, machine 0 = host (P1/campaign). Each machine simulates its own pawn and
  sends it to the host with a `seq` + ack/retransmit; the host replies with a versioned snapshot
  (pawns + zombies + doors + item holders), applied only if `version > myVersion`
  (per-thing `idem`), retransmitted until the client's ack, which allows it to stop.
- Interactions as requests to the host with SNAPSHOT replication: doors (interact), items
  (pick up → machine buys it; drop releases it), spawning a zombie (one-time per site, double
  request = clean rejection), damage to a zombie (host; last hit releases the corpse via
  logOwn `Release`). Requests are a serial stream per client (`seq`, deduped by `(client, seq)`,
  only acked/accepted once), each carrying the world epoch it was issued in.
- Extended beyond the MVP with everything from example_target.cpp: `Join/Leave` (presence flags
  travel inside the snapshot; a leaver's held items are released to host via logOwn `Transfer`;
  sequence numbers are never reused, the leaver self-acks its queues), `Save/Load`
  (`worldEpoch` bumps on load; requests carry their epoch and stale ones are rejected and
  consumed so streams unstick; clients re-assert their pawn on new epochs), an ownership log
  (`logOwn`: Grant/Release/Transfer for zombies and items) fed to the oracles, a host AI tick
  that roams zombies, and stale-op raw replay.

Test runs (real tools, executed here):

- Clean: `--minutes 1 --players 2`: **12,265 sessions ALL OK**; `--minutes 3 --players 4`:
  **14,428 ALL OK**; `--minutes 2 --players 3`: **16,858 ALL OK** (join/leave/save/load every
  few seconds, stales, link chaos; totals above are the coverage counts).
- `--minutes 2 --players 4` (previous round) caught 2 real bugs, shrunk to 1 op each, fixed:
  1. snapshot reading zombies/doors read flags 1 byte with a 4-byte read → desynchronization
     (pos bled with adjacent bytes, `(-1,-1)` in the shrunk report).
  2. On moving the HOST's own pawn, `version` was not incremented → the new position of P1
     never left the host.
- `--minutes 1 --players 2` (this round, join/leave active) caught 5 more real bugs, all
  shrunk + fixed, each found from the minimized report:
  1. snapshot size assumed 4 players → with 2 players every snapshot was dropped forever
     (session never quiescent).
  2. brace-init `std::vector<uint8_t> door_{kDoors}` makes ONE element of value 4 → doors
     desynchronized and out-of-bounds writes (replaced with constructor init lists).
  3. request stream desynced across a leave/join: a request lost in flight was never acked,
     and the host's strict in-order gate lagged the client's numbering forever (`gap request`
     storm). Fix: leaver self-acks its own streams; window-1 retransmit; on rejoin the client
     sends a `hello` (its frozen `reqHigh`) that the host adoptively applies to its gate.
  4. requests replayed in flight after a counter reset executed twice (`double_apply`) —
     fixed by never reusing sequence numbers (host retains per-peer counters).
  5. hp-only snapshot changes were classified "no change" (change predicate compared alive/
     pos but not hp) → damaged zombies healed back on clients after a while.
- Injected twins, each caught (3/3 shrunk sessions):
  `no_snapshot_dedupe` → `double_apply`; `no_request_dedupe` → `double_apply`;
  `no_world_epoch` → `stale_applied`. The fuzzer fails against known bugs, not greenwash.

## Phase 6 in progress: the mod (REFramework plugin transport)

Decision (2026-10-23): the sync core does NOT move to Lua. REFramework's Lua has no
sockets/multithreading, and the embedded Lua is vanilla 5.4 built into REFramework privately
(no exported Lua C API for plugins to call). So: a C++ REFramework plugin
(`local/mod/RE2Online/`) for transport + the fuzz-validated core, with re2coop's file-bridge
pattern as precedent.

- REFramework plugin API verified against the pinned source
  (commit `d4611aef1d63effd3a736bdf9404ea702428017d`): exports
  `reframework_plugin_required_version` (fills major/minor/patch from `API.h` macros 1.15.0)
  and `reframework_plugin_initialize(param)`; `param->functions` gives `log_info` +
  `on_present(cb)` (the frame pump), via `<reframework/API.h>` (the versioned C header; the
  `API.hpp` wrapper needs C++20 and is avoided).
- The mod reuses `tools/fuzz/re2_target.cpp` verbatim (single source of truth): the fuzz
  harness (`Re2Target` + registrar) is compiled out with `COOPNET_MOD_CORE_ONLY` (verified:
  fuzz still passes, 12,288 sessions ALL OK); the plugin compiles that file and drives
  `Re2Core` exactly like the fuzzer's adapter (50 ms `tick()`, 200 ms `hostWander`).
- `local/mod/RE2Online/plugin/plugin.cpp` (~440 lines, builds with cl.exe /std:c++17 /LD,
  ws2_32): UDP loopback (host binds the pair port and learns the client's address from its
  first datagram; client binds an ephemeral port and targets `127.0.0.1:<port>`), rx thread →
  present-thread pump, config from env (`GAMEPOOL_ROLE`/`GAMEPOOL_PAIR`/`GAMEPOOL_PORT`/
  `GAMEPOOL_INSTANCE`, injected by `tools/gamepool` launcher.py:174-177) with an INI fallback
  (`reframework/data/re2coopnet.ini`, what `pairs.edits` can write per instance).
- Evidence, written by the plugin: `reframework/data/re2coopnet/apply_<role>_<pid>.jsonl`
  (every accepted/rejected apply with idem/seq/why) and `own_<role>_<pid>.jsonl` (Grant/
  Transfer/Release/WorldEpoch), plus a 1 Hz `reframework/data/re2coopnet_status_<role>_<pid>.txt`
  JSON status; periodic `COOPNET:` lines land in `re2_framework_log.txt`.
- Synthetic "world" for milestone A: local pawn patrol (moves every 700 ms), host spawns
  zombies/roams/toggles doors/hits (via the real request stream), client picks up / drops
  items — full protocol paths, no RE SDK calls yet.

**First integration evidence, no game needed** (25 s loopback pair test,
`local/mod/RE2Online/run_pair.ps1`, two `coopnet_standalone.exe` copies of the plugin driven
by a fake REFramework param, host binds 27960 / client env `GAMEPOOL_ROLE=client`):

- host 27304: `progVer=3448 own=542 applied=2643 tx=1494 rx=1493 txErr=0`,
  peer learned at `127.0.0.1:62473`; client 12432: `myVer=3437 applied=2252 moves=804
  mreqs=804`, rejects are the expected dedupe lines (`no change`).
- JSONL sanity: host own-log shows Grant/epoch chains (refs 512+ = kEnemyBase, epochs
  incrementing); client apply-log shows `playerpos` ref 256 (kPawnBase) + `spawn` applies;
  idem keys shaped `0x2…` (snapshot)/`0x4…` (pawn) as in the fuzz report format.

**Pump-rate bug found and fixed via the flight recorder** (this is the automation doing its
job): the first pair runs synced but at absurd pacing (host `progVer` ~700 versions/s, own
grant→release pairs 3 ms apart). Instrumenting the pump (`dbg pumps/tickIt/...` lines) showed
`tickIt=1,569,958` on the first pump: the pump clock is `steady_clock` ms since boot (~78M)
but every period timer was initialized at 1 — the 50/200 ms tick timers had catch-up `while`
loops (1.5M-iteration first-pump flood), while the driver timers (`if` + `+= step`) lagged
forever by ~78M ms and fired every pump. Fix: timers are reset to `t0_` in `prepare()` and a
single `every(next, period, maxCatchUp, fn)` helper ticks with a bounded catch-up and jumps
forward after a stall (window minimized/no frames), so no history replay flood. Verified:
518 pumps → pawn 22 (≈1/700ms), spawn 5 (≈1/3.5s), door 2 (≈1/9s), hit 4 (≈1/4s).

**Two real game copies sync (milestone A)** (2026-10-23, `D:\coop-test\re2`, both copies
launched from the same dir, host first, client ~25 s later, ~40 s observation):

- host: `progVer=640 own=11 tx=169 rx=169 txErr=0`, dbg `pumps=12839 pawn=65 spawn=13
  door=6 hit=12` (game presents ~115 Hz ≈ on_present per-frame pump);
- client: `myVer=636` — within 4 in-flight versions of the host's current world version,
  `tx=166 rx=166 txErr=0`, `applied=280`, rejects = expected dedupes;
- the client joins late and converges (no explicit join handshake needed for the
  presence-always-up milestone; versions dedupe by idem).
- Deploy notes: the game copies must be dead before replacing the plugin DLL (Windows file
  lock); `re2coop_main.lua` renamed `.disabled` (backup `.bak` next to it) so re2coop's own
  Lua doesn't interfere.

Next: gamepool 2-instance pair (`local/re2.json`, `D:\coop-test\re2pool`, per-instance
`re2coopnet.ini` via `pairs.edits`), then `tools/chaos` bad_wifi relay over that pair, then
real pawn data through the RE SDK (the synthetic world stays as the health check).

### Milestone A(b): the same plugin driven by the gamepool launcher (2026-10-23)

`local/re2.json` (gamepool config, gitignored tree): `game_dir` = test copy,
`instances_root` = its own subdir, `launch {window: normal, stagger_s: 20}`,
`ready {type: file, path: reframework/data/re2coopnet_ready.txt}` (the plugin writes this
one-liner on start; the launcher deletes it fresh per launch), `pairs {port_base: 27960,
copy_gb: 3.0, reserve_gb: 4.0, edits}` templating a per-instance
`reframework/data/re2coopnet.ini` with `role={role} port={port} pair={pair}` — the plugin's
env vars are a fallback, the INI is what gamepool-controlled runs actually use.

Config-schema kills learned while doing it (candidates for `tools/gamepool` docs/PR
feedback):

- `link.per_instance_dirs` is not a default: without `["reframework/data"]` the link
  stage **hard-linked our mod's own evidence (status txt, apply/own jsonl, lua leftovers)
  from the test install into the fresh instances** — invisible until the inode check
  (`os.stat(...).st_ino == source.ino`), because per-instance renaming looked fine.
- a `config_files` key and the `pairs.edits` key must be the same string
  (`reframework/data/re2coopnet.ini`, not `re2coopnet.ini`), and a file absent from the
  install only needs the `edits` (empty base is legal); a file present anywhere in
  `per_instance_files` mirrors it private.
- `up` didn't block on the ready file with 0 bytes — the file probe passed on
  *existence*, but our `re2coopnet_ready.txt` was empty because `JsonlLog` relied on
  process-exit flush: **flush now runs in the 1 Hz timer** so live evidence collects.

`provision --pairs 1` + `up --pairs 1` result — two real RE2 copies from
`D:\coop-test\re2pool\{0,1}`, plugin loaded in both (their own `re2_framework_log.txt`,
pump-rate lines sane), pair synced on loopback port 27960:

- host: `progVer=3146 own=51 tx=rx=1383 txErr=0 txDrop=703` (pre-peer-learn drops),
  `peer=127.0.0.1:61779` — learned the client's ephemeral port from the wire;
- client: `myVer=3158 applied=3260 tx=rx=1388 txErr=0`, `peer=127.0.0.1:27960` (the
  templated port), client/host versions within one snapshot cycle;
- `apply_host_36840.jsonl` grows while the game runs (flush fix), instance data dirs now
  contain only current-pid files.

Plugin deps: `Session::openLogs` also writes `re2coopnet_ready.txt` (probe target);
`JsonlLog` gained `flush()`. Known leftovers: dbg counters (`pumps/pawn/spawn/door/hit`)
intentionally kept until QA ship; the wide `%s` bug in the start reLog replaced with a
narrow summary.

### Milestone A(c): chaos relay between the two real copies (2026-10-23)

Setup: gamepool pair `down`ed after provisioning; `tools/chaos/chaos.py relay --target
127.0.0.1:27960 --listen 27961 --profile lan` (packet log + info file under
`local/mod/RE2Online/out/chaos/`); the client instance's private
`reframework/data/re2coopnet.ini` edited by hand to `port=27961` (the client targets
`port`; the host's stays 27960), both copies launched manually from their instance dirs so
the launcher's `GAMEPOOL_*` env doesn't override the ini. Lesson: gamepool's env injection
always wins over the ini, so a relay in front of a pool pair needs either a templated
`peer`/port trick in `pairs.edits` per experiment or manual launches — noted for the
`tools/gamepool` feedback PR.

Results (client `myVer` vs host `progVer`, all statuses from per-instance files):

- `lan` baseline: 1072 vs 1079 (in-flight ≈ latency window), tx=rx line-to-line.
- `bad_wifi` 60 s: 4164 vs 4170, host tx 2542 / rx 2475 (delayed + burst-dropped), rejects
  up with `lastWhy=old snapshot` (reorder dedupe working).
- `partition` s2c 4 s: client world freezes ~120 versions behind, then fully heals in
  under 20 s (6112 vs 6099, zero in-flight) — the hello-window resync covers a blackout.
- `hell` 60 s: 6652 vs 7602 (~950 behind under 8% loss + bursts + 200 ms + 512 kbit cap) —
  bent, never broke: no crash, no runaway, reject dedupes keep arriving. tx=7038 vs
  rx=4684 on the client ≈ the c2s rate cap queue doing its job.
- relay self-stopped at 420 s (`--seconds`), and the pair kept running silently against a
  dead relay for the remaining window: worlds froze, no spam, `txErr=0` — clean outage.

Relay packet log: 9,824 datagrams, 9,355 fwd / 469 dropped, event lines: profile, partition
on/off, profile. RPC via PowerShell CLI mangles quoted params — use
`tools/chaos/client.py` from Python (README note worth adding to the chaos page).

Next: real pawn data through the RE SDK (the synthetic world stays as the health check).

## Unknowns to next steps

- Two-copy loopback pawn sync test (needs a test copy of the game on another drive).
- Party Mode needs installing from Nexus (needs owner's account login).
- Fork + branch + claim issue needs `gh` (not installed) and the owner's GitHub account.
