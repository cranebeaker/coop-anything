# Persona 5 Royal – co-op notes

## 2026-10-30 Triage

### Observations (checked)
- Store: Steam (app 1687950; `install.vdf`, `steam_api64.dll` present).
- `P5R.exe` FileVersion 1.0.4.0, PE timestamp 2024-08-23, SHA-256
  `5D9E18C383700302D029C947DDF8C811BA6AFA47E60EC1AFA0554D452BDC0CBF`. Size 395,876,288 bytes.
- PE sections include `.xcode`, `.arch`, `.xtext` alongside `.text` (consistent with an anti-tamper wrapper).
- Data: `CPK/BASE.CPK` (~25 GB), `FR.CPK`, `MOVIE_JE.CPK`, `SOUND_E.CPK`, `SOUND_J.CPK`. 12 files, 39.4 GB total.
- OS: Windows 11. Engine: Atlus in-house (GFD), closed native x64 binary.
- coop-anything directory, issues and PRs: no prior Persona 5 Royal entry (searched 2026-10-30).

### Sources
- Denuvo on P5R PC, inlining makes patching hard: https://shrinefox.com/blog/2022/10/22/modding-persona-5-royal-on-pc-switch/
- Mod loader (Reloaded-II, "Persona Essentials"): https://github.com/Sewer56/p5rpc.modloader
- No co-op/multiplayer mod found in a web search (2026-10-30).

### Derived
- Path: build from zero (nothing to revive). Engine family: closed binary → Reloaded-II C# mod with
  signature-scanned hooks; never patch or strip the DRM.
- Single-player calendar/confidant state makes two-instance lockstep very costly. A single-host model
  (host runs the only game copy, guests stream video + send input) avoids world sync entirely.

### Unknowns
- Where the battle "current acting unit" lives in memory: unknown.
- Whether a second controllable field avatar can be spawned (field scripts / flowscript): unknown.

## 2026-10-30 Step 1: battle turn ownership

### Observations (checked, static, against the 1.0.4.0 exe above)
- Input imports: `xinput1_4.dll` ordinals #2 (XInputGetState) and #3 (XInputSetState); `dinput8!DirectInput8Create`;
  Steam Input interface string `SteamInput006`. No SDL, no raw input.
- Existing input mod (AnimatedSwine37/p5rpc.inputhook, GPL-3.0) hooks after the game merges all devices into one
  key list, so device identity is already lost there. Per-player routing must happen at the device layer.
- Flowscript AI/BTL function table: 461 entries of `{fn, argc, name}` at VA 0x14192F9C0, plaintext on disk.
  `BTL_GET_CURRENT_CHARAID` = 0x140B295A0, `AI_GET_MY_ID` = 0x1407B5580.
- Both read the global script context at 0x14293D008, then `[ctx+0x260]` (battle/AI unit context). `AI_GET_MY_ID`
  resolves `[ctx+0x260]+8` through 0x14002CC80 and reads the unit id at `+0x1D8`.
- p5rpc.lib (GPL-3.0) calls flowscript functions with its own context; ctx+0x260 is then unset, so calling
  BTL_GET_CURRENT_CHARAID from a mod would dereference null. Not used.

### Built (repo `p5r-coop`, local for now)
- `P5RCoop.Core/TurnOwnership`: pure rule for who owns input (alternate by party slot, per-character pins,
  field player or shared field). 19/19 offline checks pass (`dotnet run --project P5RCoop.Tests`).
- `p5rpc.coop`: Reloaded-II mod that hooks `XInputGetState` in xinput1_4 (system DLL, not the protected exe)
  and shows the game only the tester's pad as pad 0. Builds; **not run in game yet**.

### Unknowns (need the game running)
- Whether P5R reads pads through XInput, Steam Input or DirectInput when Steam Input is on/off, and how
  Remote Play Together guest pads appear (XInput index vs Steam Input handle).
- Where the acting party member lives while a player picks a command (Direct Commands tactic). Candidate:
  the unit object behind ctx+0x260 in battle; needs a runtime probe.

QA level: untested (no in-game run).

## 2026-10-30 First in-game run
- Saves backed up before launch (folder copy, 19 files, sizes verified). Work copy made with robocopy; P5R.exe hash matches.
- Reloaded-II 1.31.0 (SHA-256 matches the GitHub release digest) + reloaded.sharedlib.hooks 1.16.3 (no published digest;
  size matches the release, local SHA-256 2b7c2e6118a3f1eb00a2e1e9105397b0d17a118a84596308c3a6a9ff3cb14b1b).
- Launched via `Reloaded-II.exe --launch`. A `steam_appid.txt` (1687950) in the work copy was needed: R-II SteamHook
  reported the app was not in any Steam library.
- **Observed:** p5rpc.coop loaded; XInputGetState called ~470/s at the title/menus (2880 calls in the first 10 s).
  So P5R polls XInput, and the gate sits on a live path. Game exited normally ~90 s later (Reloaded "ExitProcess Hook").
- Not yet observed: Steam Input path, Remote Play guest pads, battle actor.
- Next: `p5r-coop/tools/turnprobe.py` (read-only memory snapshots on F9) during one battle with Direct Commands.

## 2026-10-30 Battle actor found (probe run 1)
- Probe: 4 snapshots in one battle, tester-reported actors Joker, Morgana, Ann, Ryuji (Direct Commands).
- **Observed:** static `P5R.exe+0x293EEF8` (u64) -> battle object (0x115994E90 in that run, same in all 4 snapshots).
  - `+0x592` u16 = acting chara id: 1, 3, 4, 2 (matches Joker=1, Morgana=3, Ann=4, Ryuji=2).
  - `+0x2B0`, `+0x2BC`, `+0x2C4` u32 = 0, 1, 2, 3 (turn or party slot index; unknown which).
  - `+0x590` u8 = 0, 1, 1, 1 (unknown).
  - Global script ctx 0x14293D008 was null during the command menus.
- Only one battle in one process: the pointer chain is **not yet confirmed across launches**, and its value
  outside battle is unknown.
- Mod v0.2.0: `BattleActorSource` polls that chain every 30 ms via ReadProcessMemory (safe on stale pointers),
  logs every change, and feeds `ActorTracker` (party order = order of first action in each battle).
  Offline checks: 28/28 pass. In-game routing: not yet tested.

## 2026-10-30 Tester test of v0.2.0 (3 battles, 2 launches): FAILED
- Tester report: first battle "felt" like it worked, then only pad 1 worked.
- **Observed in the mod log:** `P5R.exe+0x293EEF8` read 0 for the whole of both launches, so the mod never saw a
  battle and pad 1 owned input throughout. The "first battle worked" impression is not supported by the log.
- **Derived:** 0x14293EEF8 has no RIP-relative code references (scanned every section), so it is not a real
  battle global. 0x1423F3E70 is a generic intrusive list head (nodes linked at +8/+0x10/+0x18) — not usable.
- Lesson for the playbook: a pointer seen in one session is not a pointer path. Confirm across launches first.
- `tools/ptrscan.py`: depth-3 static -> offset chains to the actor field. Session 1 alone: 481 chains stable over
  its 4 snapshots. Session 2 probe pending to filter them.

## 2026-10-30 Probe run 2 (new launch): actor chain confirmed across launches
- Tester-reported actors (8 F9 presses): Joker, Joker (One More), Morgana, Ann, Ryuji, Morgana, Ann, Ann (One More).
- ptrscan: 481 chains from session 1 -> **23 valid in both sessions**.
- **Observed:** these 6 chains (statics with code refs) give the right id in all 12 snapshots, both launches:
  `+0x286C2A8 ->+0x88 ->+0x48 ->+0x592` (142 code refs), `+0x2900BF8 ->+0x98 ->+0x48`, `+0x2530FB0 ->+0x90 ->+0x48`,
  `+0x293DB58 ->+0x148 ->+0x48`, `+0x2901F58 ->+0x380 ->+0x108`, `+0x2536F78 ->+0x18 ->+0x18` (all end at u16 +0x592).
- **Derived:** `[X+0x48]` is the battle object for several manager objects X. "One More" keeps the same actor id.
- Mod v0.3.0 reads all 6 and needs 2 to agree. Offline: 28/28 checks; vote replay matches both sessions.
- Unknown: what these chains read outside battle (no field snapshot yet). The mod logs every change to find out.

## 2026-10-30 Tester test of v0.3.0: FAILED (pad 2 never got control) — root cause found
- **Observed in the mod log:** all 6 chains read 0 for the whole session, battles included.
- **Root cause (checked in a scratch app):** the mod kept `Process.GetCurrentProcess().Handle` as a raw handle; when
  the Process object is finalized the handle is closed, and ReadProcessMemory fails with error 6 from then on.
  The pseudo-handle (-1) keeps working. This also explains v0.2.0 reading 0, so the 0x293EEF8 verdict above is
  not proven either way (still unused: no code refs).
- v0.3.1: pseudo-handle, logs failed reads of statics, logs the first static at startup.
- Playbook lesson: a read helper that returns 0 on failure hides bugs as "not in battle". Log failures.

## 2026-10-30 Tester test of v0.3.1: battle detection WORKS, pad 2 not usable
- **Observed in the mod log:** actor sequence Joker, Morgana, Ann, Ryuji, Joker, Morgana with 6/6 votes; back to
  `field` when the battle ended (chains read 0 or junk, votes < 2). The first static reads 0 at the title screen.
  Chain `+0x2536F78 ->+0x18 ->+0x18` is noisy at battle start (junk for ~1 s); the vote absorbs it.
- Tester report: on Player 2 turns the game said "controller disconnected"; pad 1 was blocked as designed; the keyboard
  still worked (not gated).
- **Observed (Device Manager):** one pad was in XInput mode (listed as "Xbox 360 controller"); the other was in
  plain HID / DirectInput mode, so it has no XInput index.
- **Derived:** P2 was mapped to XInput index 1, which was empty, so the gate passed a disconnected pad as pad 0.
- v0.4.0: pads auto-assigned from connected XInput indices (Player N = Nth connected pad); if the tester has no
  connected pad, every pad passes (never a lock-out). Logs the connected pad list. Offline: 32/32 checks.
- Open: DirectInput pads are not gated or mapped yet. Remote Play guests should appear as XInput (unverified).

## 2026-10-30 Tester test of v0.4.0: battle turn rotation WORKS on one PC with two XInput pads
- Setup: both pads switched to XInput mode. Tester report: "everything worked".
- **Observed in the mod log:** pads connected [0,1]; Joker (P1, pad 0) -> Morgana (P2, pad 1, logged `owner=P2 pad=1`)
  -> Ann (P1) -> back to field (P1, pad 0) after the battle.
- v0.4.1 logs actor decisions only (v0.4.0 logged junk reads from one chain hundreds of times per load screen).
- QA level: manual tester playtest on one PC, 1 battle in the log. No automated test yet; Remote Play Together untested;
  keyboard and DirectInput pads not gated.

## 2026-10-30 Remote route
- **Observed:** Steam store API categories for 1687950: Single-player, Achievements, Full controller support,
  Trading Cards, Cloud, Family Sharing. **No "Remote Play Together"**, so Steam will not offer the RPT invite.
  (Also the work copy is launched by Reloaded, not by Steam.)
- **Derived:** use a general streaming host that turns guest pads into virtual XInput pads on the host (e.g. Parsec).
  The mod already maps "Nth connected XInput pad" to Player N, so a guest pad should become Player 2. Unverified.

## 2026-10-30 Tester decision: two game instances (own screen each), linked over Tailscale
- No streaming (Parsec / Remote Play) wanted. This changes the project from input routing on one PC
  (done, v0.4.1) to real netcode between two P5R instances.
- **Derived plan (playbook networking.md defaults):** host-authoritative; transport = plain UDP/TCP to the host's
  Tailscale IP (no NAT work needed); each player needs their own P5R 1.0.4.0 copy (Denuvo, per-account license).
- Candidate battle model: command lockstep with host ordering + RNG reseed per command, like BattleTech
  (how-coop-mods-work.md 1.6). Needs: the battle-command function, the battle RNG, a way to start the same
  encounter on the guest. All `unknown` so far.
- Field / story: host owns calendar, flags, confidants; guest follows. Seeing each other's avatar = puppet spawning
  (step 3), `unknown`.
- Tester: the friend does not own P5R. Two-instance co-op is blocked until they do. Steam Family Sharing (listed
  for 1687950) lets one person play a shared game at a time, so it cannot run two copies at once. Not pursuing
  any licence or DRM workaround (AGENTS.md rule 10).
