# RE7 split-screen notes (coop-re7 + Couchlink, one PC)

Working notes, 2026-11-04. Goal: two players on one PC, one copy of RE7 each, through Couchlink.

## Observed
- coop-re7 v0.25 (zip SHA-256 640df7dc...88b0, matches the GitHub release digest) has a `udp` mode
  (`reframework/data/coopnet_config.json`: bindHost/localPort/peerHost/peerPort). CoopNet.cs sends both movement
  and events over UDP in that mode (CoopNet.cs ~line 782).
- The Lua side only enables several features for `steam` or `bot` mode, so `udp` silently loses them:
  held weapon and weapon animation bank (coop_dummy.lua), lobby look, friend's chapter line, enemy hand-offs and
  pause hand-off (coop_enemies.lua `check_handoffs`, `watch_pause`), catch-up list, health, heal-my-friend
  (coop_talk.lua).
- Enemy host is "the lower SteamID" (coop_enemies.lua `decide_role`); in `udp` mode it returns "host" on both
  sides. Two copies on one PC under one Steam account have the same SteamID anyway.
- Couchlink 0.1.16 (zip SHA-256 35abfd35...18dc): its proxy picker skips a DLL name that already exists next
  to the exe (proxy.rs `pick`), so REFramework's dinput8.dll is not replaced.
- Couchlink accepts a user recipe with `files.per_seat` entries limited by `seats` (list_games: no errors) and
  finds a non-Steam game folder through `<state>/games.json`.

## Changed (local patch, not yet sent upstream)
- `udp` treated like `steam` in those gates; in `udp` mode the copy with the lower `localPort` hosts enemies
  (read once per 5 s from coopnet_config.json). 37 changed lines in coop_dummy.lua, coop_enemies.lua, coop_talk.lua.
- Couchlink recipe `re7-coop`: steam `shared`, 2 players, per-seat coopnet_config.json (27777<->27778),
  borderless windows, fps_limit 30.

## Unknown (needs a run)
- Whether Steam lets two RE7 copies run under one account (`steam: shared`), and whether RE7 has a
  single-instance mutex (`locks` empty for now).
- Performance of two copies on a 4 GB laptop GPU.
- Whether the patched Lua loads without errors (no Lua interpreter on this PC to syntax-check).

## First Couchlink runs (2026-11-04, Steam buildid 22773795, re7.exe SHA-256 D5502600...7321)

### Observed
- One copy of the co-op folder, started alone: REFramework, the C# plugin and all coop Lua scripts load; no
  Lua errors from the patched files (JSON errors only for coop_settings.json / coop_story_played.json that are
  empty on a first run).
- Couchlink session on an exFAT drive fails at once: `cannot link ... Fonction incorrecte (os error 1)`.
  Hardlinked copies need NTFS. Moving the co-op folder to an NTFS drive fixed it; the copies then live in
  `%LOCALAPPDATA%\Couchlink\instances\re7-coop\<seat>`.
- With `steam: shared`, Steam let two RE7 copies run under one account (both processes up, windows placed
  top/bottom).
- Files written during a solo test run (coopnet_status.txt and friends) get copied/linked into every seat and
  look like live state. Delete them from the base folder before a Couchlink run.
- A recipe rewritten by Windows PowerShell 5.1 `Set-Content -Encoding UTF8` gets a BOM, and Couchlink then refuses
  it (`expected value at line 1 column 1`). Write recipes as UTF-8 without BOM.
- Run 1 (seat 2 started 18 s after seat 1): seat 1 reached its first frame and the mod ran in `udp` mode waiting
  on 127.0.0.1:27778. Seat 2 never rendered: its REFramework log stopped right before D3D12 init
  ("Render frame: 1" never logged) for 9+ minutes while using about one CPU core. Laptop RTX 3050 4 GB: 3.5 GB
  VRAM in use; RAM 12 GB of 15.6 GB for the two copies (6.7 + 5.3 GB private).
- Run 2: seat 2 delayed by `window.settle_ms` 120000: stuck at the same log line again, so it is not load
  contention.
- Run 3: recipe `locks: ["*"]` (Couchlink renames every named mutex per copy), settle_ms 60000: seat 2 reached
  its first frame at once. Both copies then ran the mod in `udp` mode pointed at each other (27777 <-> 27778),
  status updating on both. So a second RE7 copy blocks before D3D12 init on some named mutex; which one is
  unknown (only the `*` rule was tried).
- Memory with both at the title screen: 8.7 GB private each, 2.0 GB physical RAM free of 15.6 GB.
- Not yet checked: packets flowing (sent/recv stay 0 until a player exists in both games), input per seat,
  frame rate.
- Run 4 (locks `*`, settle 60 s): seat 2 stalled again at the same line, after seat 1 had started but before
  seat 1 finished D3D12 init. Both copies then exited (cause unknown; nothing in couchlinkd.log).
- Run 5 (locks `*`, seat 2 held back by a per-seat helper until seat 1's UDP port 27777 was open, i.e. seat 1
  fully initialised): seat 2 stalled again at the same line and later exited. Seat 1 played on its own
  (sent 7412 packets, recv 0). So the ordering theory is wrong; only run 3 of 5 got seat 2 to render.
- The stall is always right after `amsi.dll` / `MpOav.dll` (Windows Defender's AMSI provider) load in the
  REFramework log, before `Render frame: 1`. Cause unknown; next step would be the waiting threads of a stuck
  seat 2 (thread wait reasons or a dump) and, with the owner's OK, a Defender exclusion test for the copies folder.
- Run 6: seat 2 exited (no WER crash event) ~1 min after reaching the same log line. Run 7: seat 2 stalled;
  thread snapshot (scratchpad tool `threadsnap.ps1`: suspend each thread, GetThreadContext, map rip and a
  heuristic stack scan to modules; no symbols), taken while stalled:
  - the only busy thread (100% of one core) loops in REFramework (`DINPUT8.dll+0x47032`) through
    KERNELBASE sleep/yield;
  - two re7.exe threads are blocked in USER32/win32u calls that go through Couchlink's hook (loaded as
    `XINPUT1_3.dll`) and Steam's `GameOverlayRenderer64.dll` (Couchlink's input log: "preloaded
    ...\Steam\GameOverlayRenderer64.dll");
  - the NVIDIA driver threads are idle.
  Derived (not proven): seat 2's main thread is stuck in a window-message call (overlay or hook), and REFramework
  spins waiting for it. Next test: Steam overlay off for RE7 (Steam > RE7 > Properties), then a new session.
- Couchlink's `set_game_overlay` only applies to `goldberg` recipes, not `steam: shared`.

## Root cause and workaround (2026-11-04, evening)
- Bisect with seat 2 started by hand (Couchlink's stand-in `xinput1_3.dll` in the folder, config file through
  `COUCHLINK_HOOK_CONFIG`, nothing touching the window): no hook -> first frame in 9 s; `pad` -> 7 s;
  `pad`+`focus` -> 4 s; `pad`+`focus`+`locks ["*"]` -> 4 s. Started through couchlinkd it stalled every time but once.
- Own launcher run 1: it restyled and moved player 1's window right after REFramework logged `Render frame: 1`;
  player 1 froze (not responding, log ends at `[Thread.cpp:98] Suspending <tid>`), and the launcher itself hung in
  SetWindowLong/SetWindowPos on that window. Derived: changing a copy's window while REFramework is suspending
  the game's threads to install its hooks deadlocks the game; Couchlink places windows in that same window of time.
  Not proven for Couchlink (its window code not traced).
- Launcher run 2 (touch windows only after the mod's coopnet_status.txt exists, SWP_ASYNCWINDOWPOS, skip hung
  windows, start player 2 only once player 1's mod runs): player 1 ready in 19 s, player 2 ready 13 s after, both
  at the title screen with the mod in `udp` mode. Repeated once more (22:54): same result.
- Player folders: `C:\RE7-coop-seats\p1|p2`, files over 50 MB hardlinked from the co-op folder, the rest copied.
  XInput 0 and 1 are DS4Windows' virtual Xbox 360 pads for the two DualSense.
- Not yet checked: both in game together (recv > 0 both ways), each pad driving only its own copy, frame rate.
- 22:56, both players in a new game (one DualSense each): traffic both ways within a minute, no bad packets.
  J1 sent 363 / recv 431, events 36 out / 30 in, enemy snapshots 1403 out / 917 in; J2 the mirror image.
  Both responding; 7.9 + 8.0 GB private, 0.6 GB physical RAM free of 15.6 GB.
- Still to confirm by the players: each sees the other's ghost, each pad drives only its own copy, frame rate.
- Owner wants one player per screen, borderless. RE7 is in `WindowMode=Normal` and puts its border back when it
  reapplies display settings, so the launcher now keeps checking every 2 s (style without caption/thickframe,
  exact rect) until both games close. `-Layout screens` (default): player 1 on the main screen, player 2 on the
  biggest other one (Couchlink's 1000x750 "VDD by MTT" virtual display is left out by size). Checked 23:08:
  p1 0,0 1920x1080 no border, p2 -3200,0 3200x1800 no border, both still connected.
- The installer's desktop shortcut starts one copy in `steam` mode, so starting it twice never connects the two
  copies on one PC. The 2-screen launcher now lives in its own folder with a copy of Couchlink's stand-in (and its
  MIT licence) and a desktop shortcut (hidden PowerShell, errors in a message box).
- Saving: both copies share one Steam account and so one save folder. The launcher sets coop-re7's
  `allow_saving` true for player 1 and false for player 2 (coop_settings.json, read live by coop_saveguard.lua);
  next session both pick Continue and load player 1's save. Not yet tested in play.
- One pad drove both copies (owner, 2026-11-05 00:0x). Both copies load HID.DLL, SETUPAPI and the system
  dinput8.dll besides XInput, so RE7 also reads the DualSense directly; the hook's `pad` only filters XInput.
  Launcher now adds `"own_pads": []` (hide every real pad from DirectInput, Raw Input and HID opens), so each
  copy should only see its own XInput pad (DS4Windows' virtual Xbox 360 pads). Not yet tested.
  (Why the earlier session looked right per pad is unknown.)
- 2026-11-05, owner: player 1 saved, both games closed, restart with the 2-screen launcher, Continue on both:
  both players back where they were, player 2 next to player 1. Player 2 cannot save (blocked by design).
