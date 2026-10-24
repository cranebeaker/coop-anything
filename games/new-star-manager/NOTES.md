# New Star Manager — working notes

Phase 0 (triage) + start of phase 1 (research), 2026-10-23.

## Identity

- New Star Manager (Five Aces / New Star Games), Android, package `uk.fiveaces.nsfc`.
- Version 1.7.7 (versionCode 57), Google Play split install: `base.apk` SHA-256
  `222593838caf0ef11b71d46fa324f428df29e7e0c844ba4009de711c7b9cb4fb`.
- Also seen: 1.7.6 (versionCode 51), single APK, SHA-256
  `8908f910b2e2732b59578e54404213813adb5e67acc5f5e518bc1e6e1650a0c0`.
- A PC version exists on Steam (app 883130), not examined.
- Target mode wanted by owner: head-to-head, each friend manages a different club, on phones.

## Observations (checked in the 1.7.6 jadx decompilation — the economy mod base; 1.7.7 only decoded to smali, code in classes4)

- Engine is Monkey X with the Java target. Runtime classes `BBGame`, `BBAndroidGame` and
  `BBDataBuffer` sit in `uk.fiveaces.nsfc`. The APK has no native `.so` libs. The game logic is
  translated Monkey code (`c_*` / `bb_*` classes), readable with jadx and patchable with
  apktool/smali.
- Data-driven tuning lives in `assets/monkey/Tweaks/TweakValues.txt`.
- **No multiplayer netcode.** `bb_netphysics` is goal-net cloth physics tweak values, and
  `bb_networkstate` is a single online-status tweak.
- **There is a TCP stack:** `BBSocket`, `c_Socket`, `c_TcpStream`, `c_AsyncQueue` and
  `c_NetLink` (Monkey `brl.socket`). Today it is used for downloads and the remote config
  (`c_Cloud`).
- **Exactly one human per career.** A global `bb_.g_player` (`c_TPlayer`) holds one
  `m_myclub` / `m_clubid`.
- Saves go through Monkey `LoadState`/`SaveState`, with the career split into text blobs
  `career<slot>_<n>.monkeystate` (`c_GameSave.m_LoadCareerDataFile`). The parts are joined
  with the separator `]-P-[`, and each part has a `_TS` timestamp.
- The match sim steps once per drawn frame, with physics scaled ×60 per frame
  (`c_TMatchPlayer`). The engine's update-rate limiter is forced off in
  `BBAndroidGame.onDrawFrame`.
- The match uses touch control classes (`bb_class_matchtouch`, `bb_class_matchcontrol`)
  and scripted "chances".

## Derived

- The socket classes mean a mod can add networking in Monkey/Java without bringing in a
  new library.
- A shared world with two human clubs would need a second `c_TPlayer`. Everything that reads
  `bb_.g_player` assumes there is one human, so that is a deep refactor.
- A single online match between two clubs (each phone brings its own club) avoids touching
  the career/save code. That makes it the smallest useful milestone.
- The frame-locked sim means devices at different refresh rates drift unless the sim is
  host-authoritative or driven by a fixed tick.

## Guesses (unverified)

- In a match, the human only acts during their own team's "chances", while the opponent's
  chances are simulated. If so, a 1v1 needs each phone to play its own chances and the host
  to own the result.

## Unknowns

- Whether the match sim is deterministic across devices (float math, RNG seeding).
- How a club and its squad serialize (needed to send "my club" to the other phone).
- Whether the PC (Steam) build shares this code (same engine, different target?).

## Research log

- Web search "New Star Manager multiplayer/online mod" found nothing (2026-10-23). Steam
  lists the game as single-player only.
- coop-anything: no directory entry, write-up, issue or PR for New Star Manager or Soccer.

# Phase 3 — understanding, for a shared league (2026-10-24)

Owner chose: **shared league**, one career where each friend runs a different club.

## Observations

- `bb_.g_player` (the one human) is read in 2541 places across 243 files. Rewriting the game
  for N humans is not viable. Swapping which `c_TPlayer` the global points at is viable.
- Human-club special cases (`== bb_.g_player.m_myclub/m_clubid`) appear in 60 files. The
  heaviest are `c_TMatch` (14), `c_TPlayer` (13), `c_TCompetition` (11) and
  `c_Person_Player` (10). The rest are mostly UI.
- **The world is centred on the human's nation.** `c_TCompetition` builds and simulates
  competitions where `m_based == g_player.m_myclub.m_nationid`, and continental qualifiers
  are filtered by the same nation. => Every human club has to be in the same nation.
- **Day loop:** `c_TPlayer.p_PlayToDate` runs `m_date.p_AddDays(1)` and then
  `c_TCompetition.m_PlayFixtures()`. That auto-simulates (`c_TFixture.p_PlayFixture`) every
  fixture with `m_result == 0` and `m_sdate <= today`. The human's fixture is played
  interactively first and marked `m_result = -1` (`c_TMatch`:1990), and `m_PlayFixtures`
  then finalises it to `1`.
- **The calendar lives on the human:** `g_player.m_date`.
- **The career save has 16 text parts** (`c_GameSave.m_currentCareerData[i]`, written by
  `c_TPlayer.p_SaveCommon`):
  - World: 3 = all clubs (`c_TClub.m_glist`: league, strength, ...), 4 = competitions and
    fixtures.
  - Human: 0 player/manager, 1 staff (`m_coach[]`), 2 squad (`m_list_squad`, the human's
    squad only), 5 finances, 6–8 coaching/manager/staff-boost cards, 9 trophy log,
    10 messages, 11 board targets, 12 squad archive, 15 shortlist.
  - Unclear: 14 = saved tweaks (`c_Tweaks.m_SaveString(2)`), 13 = not seen being written.
- **AI clubs have no real squads in the save**, only club rows with `m_strength`.

## Derived design (proposal, not built)

1. **Context swap.** Keep one active `c_TPlayer`. Store one bundle of human parts
   (0,1,2,5–12,15) per human. To switch to human B, write A's parts into A's bundle, load B's
   parts and repoint `bb_.g_player`. Keep the world parts (3,4) and the date shared.
2. **Matchday gate.** In `m_PlayFixtures`, a fixture whose club is in the human set and that
   still has `m_result == 0` blocks the day advance until that human has played it, instead
   of being auto-simulated.
3. **Joining with an AI club.** That club has no squad, so the code path that builds the
   squad at career start (club select) has to run for each extra human.
4. **Human vs human fixture.** The match engine knows one real squad. First version: simulate
   it (`p_PlayFixture`) with each club's `m_strength` refreshed from the human squad rating.
5. **Milestones:**
   - M1: hot-seat on one phone (two humans, swap), no network. This proves 1–3.
   - M2: async by passing the shared save file between phones.
   - M3 (optional): real time over TCP using the game's own `c_Socket`/`c_TcpStream`.

## Unknowns

- How `c_TClub.m_strength` is computed for the human club (needed for point 4).
- Which load function reads each part (needed to reload only the human parts).
- Contents of part 14 (tweaks) and part 13.
- Whether achievements, notifications and analytics singletons hold human state outside
  `c_TPlayer`.

## Owner decisions (2026-10-24)

- Sync: **real time** (everyone online at once), not async file passing.
- Human vs human fixture: **the home human plays it** interactively against an AI-built side
  using the away club's strength. The away phone receives the result.
- Base: build **on top of the owner's personal economy mod** (no ads, packs folded into
  progression, no internet).

## Contradiction found (stop, owner informed)

- The economy mod removes the `INTERNET` permission and the ads SDK (its change #13) and forces
  the "offline" remote-config path. Real-time play needs `INTERNET` back. Proposal: restore
  only the permission, and keep remote config and ads disabled.

## Design revised for real time (derived)

One human per phone, so no context swap is needed:
- **Host phone owns the world:** parts 3 (clubs) and 4 (competitions/fixtures) plus the date.
- **Each phone runs its own `g_player` normally.** Other humans' clubs look like AI club rows
  locally.
- **Matchday:** each phone plays its own fixture and sends the result plus its club strength
  to the host. The host blocks the day advance (gate in `m_PlayFixtures`) until every human
  fixture has been reported, then advances and broadcasts parts 3 and 4. Clients swap those
  parts in and reload.
- **Clients must not auto-simulate** another human's fixture. Its result comes from the host.
- **Joining:** the guest gets the world from the host, picks a club in the host's nation, and
  the club-select path builds the guest's squad locally.
- **Transport:** the game's own `c_Socket`/`c_TcpStream`. For friends on different networks,
  something like Tailscale on each phone (to be decided with the owner).
- **v1 limits:** no transfers between human clubs (the other human's players don't exist on
  your phone).

## Correction (2026-10-24)

- The jadx sources read above are from **1.7.6** (versionCode 51, game code in classes3), not
  1.7.7. The owner's economy mod (`1.7.6-mod`) is built on 1.7.6, so 1.7.6 is the co-op
  target. Re-check every smali offset before porting to 1.7.7 (code moved to classes4).

## Owner decisions, economy for the league (P46–P49, 2026-10-24)

- P46: **even start.** Every human gets the same Bux and the same stadium, and all clubs come
  from the same division.
- P47: **one top division, no promotion or relegation.** Hook (observed): every
  domestic-league move goes through `c_TCompetition.p_PromoteToMe`. Branch `m_level == 0 &&
  m_locale == 0 && m_comptype == 0` sets `club.m_leagueid`. Skipping it freezes the leagues,
  while continental qualification takes a different branch.
- P48: in human-vs-human fixtures, turn off **dynamic difficulty** (`StreakDifficulty`) only.
  Bought retries and energy offers stay.
- P49: owner asked whether Tailscale counts as "local network". Answer given: Android needs
  the `INTERNET` permission for any socket, LAN or Tailscale. Proposal: restore the
  permission, but only allow league connections to private ranges (192.168/16, 10/8,
  172.16/12, Tailscale 100.64/10). Ads and remote config stay off. Pending owner OK.

## Economy impact (derived)

- With no division changes, the per-division price multiplier (owner's P14/#10) is a
  constant: about ×196 in the English top flight per the owner's DISENO.md. Income and prices
  have to be recalibrated around top-flight values from day 1.
- The old progression axis ("promote → everything scales") is gone. Progression has to come
  from the stadium and facilities, squad building, titles, cups and continental places.
- P50: country **England** (Premier League as the only human division).
- P51: **same squad level for all.** Each human picks any Premier club and its squad is
  regenerated to a common strength. Name, badge and kit are kept.
- P52: league mod in a **new folder** (sibling of the hub clone, never inside it: game files
  must not reach the public repo). The owner's economy-mod project stays untouched.
- P53: P49 proposal accepted. `INTERNET` comes back, league sockets are limited to private
  ranges and Tailscale, ads and remote config stay off.

# Phase 5–6 — QA bot and economy measurement (2026-10-24)

Work happens in a local mod project (owner's machine, not public), branch `liga`, built on the owner's 1.7.6 economy mod.

## Observations (code: 1.7.6 jadx; behaviour: Android 14 x86_64 emulator)

- **There is no club select.** At career start the game takes the median-strength club of the starting league
  (`size/2` after sorting by strength) and rebrands it with `c_TClub.p_NewStarFC()`, which is cosmetic only
  (name, TLA, stadium name, kits). Without a forced league it uses the lowest domestic league. `m_forceLeague`
  (`c_Transition_Delayed_CreateCareer_OutAction`) and a seed (`c_TScreen_SetUp.m_inp_Seed` -> `g_player.m_seedOffset`)
  already exist. With `Debug.Enabled` the setup wheel shows one entry per league and the seed box.
- **AI clubs do have squads.** `p_GenerateSquadsForClubs` builds them for every club in the human's nation at career
  creation. They are not stored in the save (only the human squad is, in part 2). Not verified: whether they are
  regenerated from the seed on load.
- **Interactive chances freeze the engine.** `c_MatchEngine.m_CreateIncident` sets `m_gamespeed = -25`. During matches
  `g_CGAMESPEED_ACCEL = 0`, and `c_TMatchPlayer.p_FinishedTrap` calls `m_FreezeGameplay()` on every human reception.
  Only a touch (`m_UnFreezeGameplay(1.0)`) restarts the clock. While frozen, `p_Update2` is not called at all.
  `m_UpdateGameSpeed` also holds the clock for human headers without a target and for human set-piece kickers
  (state 14 + `m_swapkicker`).
- **The AI ball-carrier logic** `p_DoControlledByComAI` is used for the AI team and for the human keeper. Its shot test
  `rawdir > 10 && rawdir < 170` assumes attacking towards +y, but the human attacks towards −y
  (`p_GetShootingDirection`), so handing the human team to that AI unchanged makes it never shoot.

## QA bot (in game code, no fake input for chances)

- A switch `adb shell setprop debug.nsm.bot 1`, read through reflection on `android.os.SystemProperties`. It does not
  depend on the package name, does not touch saves, and is never set on a normal phone.
- Hooks: in `p_Update2`, the human ball carrier also goes through `p_DoControlledByComAI` when the bot is on. The shot
  angle test is mirrored for the human. In `m_UpdateGameSpeed`, when the bot is on and a human has the ball, it calls
  `m_UnFreezeGameplay(1.0)` and skips the human waits.
- Bot logic is in Java, compiled to smali at build time (`javac --release 8` -> `d8 --min-api 22` -> apktool baksmali,
  with `invoke-custom` rejected). It has a PC unit test plus a mutant twin.
- Device QA: the twin (bot off) must stall on the first chance. Result: **passed** (stalled after 91 s with no progress).
  The bot-on run must end the match with income in the mod's economy log. Result: pending.
- Menus between chances are still driven by adb taps on the green/red button (bounded gap; it never taps while a human
  has the ball).
- Lessons:
  - A static-screen watchdog is useless here, because menus and frozen chances keep animating.
  - A frozen chance writes no more log lines, so "no recent log" does not mean "no chance".
  - Sampling a button's colour must avoid its label: one sample point hit a letter of "CONFIRM".

## Economy measurement (owner's economy mod, Premier League, facilities 0)

- Home friendly: gate 3,956, sponsor win bonus 37,500, goal bonus 2 × 12,500, TV 0 (friendly), shareholder cut 1%.
  Total 65,792, against 987 for the same kind of match in the 5th tier.
- Shirt sponsor up front: 1,200,000. Starting cash: 34,000.
