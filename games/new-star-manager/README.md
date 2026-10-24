# New Star Manager (2018)

| | |
|---|---|
| Engine | Monkey X, Java target (Android), no native libraries |
| Versions tested | Android 1.7.6 (`uk.fiveaces.nsfc`, versionCode 51) in an Android 14 x86_64 emulator; 1.7.7 read only |
| Projects | Shared online league, in progress (claim: cranebeaker/coop-anything#175) |
| QA level | none (co-op not built yet; an automated single-player match harness exists, see QA) |
| Last updated | 2026-10-24 |

## Where things stand

Nobody can play together yet. The game is single-player only, and there was no earlier attempt.
This write-up covers triage, how the game works for multiplayer, and an in-game QA bot that plays
matches unattended. The bot is the base for testing two clients later.

## Existing work

- No multiplayer or co-op mod was found, including the "mod APK" sites, which only unlock currency.
  Steam lists the game as single-player.
  Search: web, 2026-10-24.

## How the game works (for co-op)

Details and code references are in [NOTES.md](NOTES.md). In short:

- **Code:** all of it is Monkey X translated to Java. Read it with jadx and patch it as smali with apktool.
  Most tuning is data in `assets/monkey/Tweaks/TweakValues.txt`.
- **No multiplayer netcode,** but there is a TCP stack (`c_Socket`, `c_TcpStream`), used today for remote config.
- **One human per career:** the global `bb_.g_player` is read in about 2500 places. The world is built around
  the human's nation.
- **Saves** are 16 text parts. Parts 3 and 4 are the world (clubs, competitions and fixtures); the rest belongs to
  the human.
- **The day loop** (`c_TPlayer.p_PlayToDate` → `c_TCompetition.m_PlayFixtures`) simulates every fixture with
  `m_result == 0`. The human fixture is played first and marked −1. This is the hook for a matchday gate.
- **No club select:** the game converts the median club of the starting league into "New Star FC".
  `m_forceLeague` and a seed already exist in career creation.
- **Interactive chances freeze the engine** until the first touch: `m_gamespeed = −25`, acceleration 0, and a
  freeze on every human reception.

## What we changed

Patches live in the local mod project (not public, it is built on a personal mod). Each change is listed with
its hook in NOTES.md.

- Firebase consent no longer crashes when the `INTERNET` permission is removed, and the consent screen is skipped.
- A career in England can start in the Premier League with a real club instead of the converted New Star FC.
- A QA bot plays the human chances with the game's own ball-carrier AI. It is switched by a system property and
  unfreezes chances the same way a first touch does.

## QA

- `scripts/test_bot.sh`: PC unit test of the bot's shot-angle logic, plus a mutant twin that must fail
  (641 failures caught).
- `scripts/qa_bot.sh`, on the device:
  - twin with the bot off: `FAIL: stuck on a chance waiting for the human (91 s)`, which is the expected outcome;
  - bot on: `PASS`, a full home friendly ended 0-0 with 8 unfreezes, 2324 bot-controlled frames, no crash, and
    income in the economy log.
- Menus between chances are still driven by adb taps on the green/red button. It never taps while a human has the ball.

## How to play

Not playable together yet.

## Known issues

- The bot rarely shoots: the game's AI is conservative. It is good enough to finish matches, not to stand in for a player.

## Next steps

- Matchday gate and host-owned world over TCP between two emulators.
- Give each friend a different club. Each phone currently converts the same median club.
