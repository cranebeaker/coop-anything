# Resident Evil 7: two players on one PC (local co-op)

| | |
|---|---|
| Game | Resident Evil 7 biohazard, Steam app 418370, buildid 22773795 (current RT/DX12 build), `re7.exe` 1.0.0.7, SHA-256 `D550260029B51A580E206B9A12A4EB7F4A1B4DD230BBA5DEFCCA81DA40597321` |
| Engine / OS | RE Engine, Windows 11 |
| Mod | [coop-re7](https://github.com/xXCroissantXx/coop-re7) v0.25 + [local split-screen PR](https://github.com/xXCroissantXx/coop-re7/pull/1) (draft) |
| Input hook | [Couchlink](https://github.com/cranebeaker/couchlink) 0.1.16 stand-in `xinput1_3.dll` (MIT), started by the PR's launcher, not by the Couchlink service |
| QA level | none (one owner session with two players on one PC, plus scripted start-up checks; no automated suite) |
| Last updated | 2026-11-05 |

coop-re7 is online co-op: one PC per player, over Steam relay. This page is the local version: two copies of RE7 on
one PC, one controller and one screen each (or top and bottom of one screen), talking over 127.0.0.1. Working
notes with every run and its numbers: [NOTES.md](NOTES.md).

## Where things stand

Works on the test PC: two players started a new game together and played about 10 minutes, each copy sending and
receiving the whole time, then again with one player per screen. Each controller drives only its own copy. Saving
by player 1 and Continue on both is not tested yet.

## How to play

Install coop-re7 (`Install.cmd`), apply the PR (or use a release that has it), double-click
`Split screen setup.cmd` once, then start with the **RE7 Co-op split screen** desktop shortcut. Steam must be
running; PlayStation pads need DS4Windows or Steam Input so Windows sees them as XInput pads 0 and 1. Details in
the PR's `split-screen/README.md`.

## What it took

1. **The mod's `udp` mode was a test mode.** Several features were gated on `steam` or `bot` mode, and the enemy
   host was "the lower SteamID", which can't decide when both copies run under one Steam account. The PR hosts
   enemies on the copy with the lower local port and treats `udp` like `steam`.
2. **Copies:** one folder per player with the big data files hardlinked from the co-op folder. Hardlinks need NTFS:
   on an exFAT drive Couchlink failed at once with `cannot link ... (os error 1)`.
3. **Two copies under one Steam account** ran fine with `SteamAppId` set and `steam_appid.txt` in the folder.
4. **The second copy stalled before its first frame** when the Couchlink service started it (6 of 7 runs), with
   or without mutex renaming, with or without a 2-minute delay, with or without Steam's overlay. Started by hand
   with the same hook and settings it started every time. Then a launcher that restyled player 1's window right
   after REFramework's `Render frame: 1` froze player 1 (log ends at `[Thread.cpp:98] Suspending <tid>`), and the
   launcher hung in `SetWindowLong` on it. The launcher now touches a window only after that copy's mod writes its
   status file, and uses `SWP_ASYNCWINDOWPOS`. Derived, not traced in Couchlink's code: its window placement lands
   in the same moment.
5. **One pad drove both copies** with the hook's `pad` setting alone: RE7 loads `HID.DLL`, SetupAPI and the
   system `dinput8.dll` and reads PlayStation pads directly. `own_pads: []` (hide every real pad from DirectInput,
   Raw Input and HID) fixed it; each copy then sees only its XInput pad (DS4Windows' virtual Xbox 360 pads).
6. **Borderless:** RE7 in `WindowMode=Normal` puts its border back when it reapplies display settings, so the
   launcher re-checks both windows every 2 s while the games run.
7. **Saves:** both copies share one Steam save folder, so only player 1 may save (coop-re7's `allow_saving` in
   `coop_settings.json`); next session both pick Continue.

## Known issues

- Memory: each copy took about 8 GB of RAM (16 GB PC, 0.6 GB left with both in game). Keep graphics low.
- Saving with player 1 then Continue on both: untested.
- `-Layout split` untested since the `own_pads` change; Xbox pads untested.
- Couchlink's own launcher does not work for this game yet (stall above); reported as
  [cranebeaker/couchlink#11](https://github.com/cranebeaker/couchlink/issues/11).

## Sources

- coop-re7 source and v0.25 release: https://github.com/xXCroissantXx/coop-re7
- The PR with the patch and launcher: https://github.com/xXCroissantXx/coop-re7/pull/1
- Couchlink hook config fields (`pad`, `focus`, `locks`, `own_pads`) and the stand-in's `COUCHLINK_HOOK_CONFIG`:
  `core/crates/couchlink-hook/src/config.rs` and `proxy.rs` in https://github.com/cranebeaker/couchlink
- Runs, logs and thread snapshots: [NOTES.md](NOTES.md)
