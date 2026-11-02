# Balatro (2024)

| | |
|---|---|
| Engine | LÖVE 11 (fused exe: `love.exe` + a zip of Lua sources), LuaJIT, luasteam |
| Versions tested | Steam, appid 2379780, build 17459173, version 1.0.1o-FULL, `Balatro.exe` SHA-256 `0d75fe164accf3312734d4b37ac98788dd15f0b8e4f9bb8b7f90c4e59de93f47`, Windows 10 x64 |
| Projects | [Balatro Duo](https://github.com/dobrodeetel/balatro-duo) (local co-op on one PC, release 0.6.1), directory entry "Balatro Duo"; [BalatroCoop](https://thunderstore.io/c/balatro/p/GUANZHUTAFEI/BalatroCoop/) (LAN, same run), directory entry "BalatroCoop" |
| QA level | automated (bot clients, 9 scenarios incl. crashes and restarts) plus playtests to ante 8 |
| Last updated | 2026-10-30 |

## Where things stand

Two people can play Balatro together on one PC with Balatro Duo: two windows, each player
with their own run, one shared blind score, the host picking blinds. Continuing after a crash
or restart of either window works. Playtesters played several sessions to ante 8 with a mouse
and a DualSense; a gamepad in the window without focus is not covered by tests.

## Existing work

- [BalatroCoop](https://thunderstore.io/c/balatro/p/GUANZHUTAFEI/BalatroCoop/) 0.2.2 alpha
  (2026-10-13): two players in the same run, combined blind score, shared shop, separate
  decks, LAN over TCP 22122 (or a tunnel). Needs Steamodded and Lovely. No source repo or
  license listed.
- [BalatroMultiplayer](https://github.com/Balatro-Multiplayer/BalatroMultiplayer) (GPL-3): PvP
  (attrition, showdown, survival), not co-op. Needs Steamodded and Lovely.
- Balatro Duo (this write-up) is a different design: each player keeps their own run and only
  the blind is shared.

## How the game works (for co-op)

- `Balatro.exe` is a fused LÖVE exe; the Lua sources open with any zip reader.
  [Lovely](https://github.com/ethangreen-dev/lovely-injector) (`version.dll`) patches them at load
  time with pattern patches and can append whole files to `main.lua`.
- Score: `G.FUNCS.evaluate_play` computes `hand_chips*mult` at once and queues a non-blocking
  ease of `G.GAME.chips` behind the card-by-card animation (`state_events.lua` ~1044).
  Win check: `G.GAME.chips - G.GAME.blind.chips >= 0` in `update_hand_played` and
  `end_round`.
- Blind screen: `Game:update_blind_select` builds `G.blind_select` from
  `G.GAME.round_resets.blind_states`; `create_UIBox_blind_select` recomputes `blind_on_deck`
  from the states. The select and skip buttons are `one_press` (one click disables them).
- The next ante's boss is rolled at cash out (`reset_blinds`), so it is known from the shop on.
- Tags: pack tags fire on `new_blind_choice` in a loop that stops at the first tag that fires
  and runs again when the pack closes (`end_consumeable`). Two packs at once crash on close.
- Saves: `save_run` serialises `G.GAME` (plain tables kept) at nearly every state change;
  Continue calls `Game:start_run({savetext = ...})`, the same function as a new run.
- Secret hands become visible when played; their planets are "softlock" and only in the pool
  once the hand was played (`common_events.lua` ~2009).
- No netcode. The game uses SDL2 for gamepads; LÖVE follows `APPDATA` for the save folder,
  Lovely does not (pass `--mod-dir`).

## What we changed

All in [Balatro Duo](https://github.com/dobrodeetel/balatro-duo): `lovely.toml` (6 pattern
patches) and `duo.lua` appended to `main.lua`. Details, root causes and every bug the tests
found: [NOTES.md](NOTES.md).

- Two windows, TCP on 127.0.0.1:22123, one text line per message. The second window has its
  own save identity (`BalatroDuoP2`). Gamepads are split between the windows (SDL background
  events), and the unfocused window ignores the mouse.
- Shared blind score: own points go to the partner only after they counted into the own
  total. A won round waits for a partner hand that is still counting. Out of hands: wait for
  the partner without locking input.
- Host decides blinds. The guest follows skips by state and the host's boss from the shop on.
  Guest select/skip presses show on the host's screen as a wish.
- Options: shared hand levels (planets or all upgrades), both skip tags to both, shared
  secret hands (their planets enter the partner's pool).
- Continue after a crash: the co-op state is in `G.GAME.duo` (run id, current round score
  split, partner's secret hands). Each side sends its whole run position (`sy`). The side
  further along wins: the one behind catches up on its blind screen or joins the blind the
  partner is already in. A dead guest window is reopened by the host.
- Text in all 15 game languages (`duo_lang.lua`), using the game's own terms.

## QA

- `tools/run_all.ps1` in the mod repo: 9 scenarios, each two bot clients playing real games to
  the end of ante 2, logs compared by `tools/check_logs.ps1`. It checks selections, skips,
  targets, equal totals in every round, score sent only after counting, tags, levels and
  wishes.
- Scenarios: shop lag + planets, host shop lag + secret hands, tag stack (Double + Charm on
  both), guest wishes + slow guest, own decks, host killed at 110 s, guest killed at 45 s,
  both killed at 130 s, guest killed and reopened by the host.
- Last full run 2026-10-27 (0.6.0): 9/9 passed (46, 46, 41, 44, 39, 20, 25, 17, 25 checks).
- 0.6.1 (2026-10-29): scenarios 1-2 (46/46 each); a wish run (47/47); language screenshots
  (`tools/lang_test.ps1`, 15/15).

## How to play

Download the zip from the [releases](https://github.com/dobrodeetel/balatro-duo/releases), run
`Install.cmd`, start Balatro from Steam, main menu -> CO-OP -> Play together (2 windows).
Host: mouse and keyboard; guest: gamepad #1.

## Known issues

- Gamepad presses in the window without focus are not covered by tests.
- Boss reroll vouchers and Boss Tag on the guest are not covered by tests.
- Continue and catch-up were bot-tested on antes 1-2 only.
- A side that continued an older save replays that round alone (the partner already finished it).
- Starting a second window from Steam while one runs opens it with the host's profile.

## Next steps

- Bot scenarios for boss reroll vouchers, Boss Tag and higher antes.
- Online play (the protocol is plain TCP; the address is in `duo_config.txt`, no UI yet).
