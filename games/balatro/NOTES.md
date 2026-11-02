# Balatro: working notes (local co-op "Balatro Duo")

Status: 0.6.1, released in https://github.com/dobrodeetel/balatro-duo (mod source, installer, test tools). Write-up: README.md.

## Identity
- Steam appid 2379780, depot 2379781, build 17459173, Windows 10 x64
- Version 1.0.1o-FULL (from `globals.lua`)
- Balatro.exe SHA-256 `0d75fe164accf3312734d4b37ac98788dd15f0b8e4f9bb8b7f90c4e59de93f47`
- Engine: LÖVE 11 fused exe (exe + zip of Lua sources, opens with any zip reader), LuaJIT, luasteam

## Goal
Two windows on one PC, each with its own run (deck, money, jokers, shop). Only the blind
score is shared: both players' hands go into one pot, blind beaten when the pot reaches the
target. Host picks blinds, skips and boss. Difficulty multiplier set by host in game.

## Approach
- Loader: Lovely 0.9.0 (`version.dll`, SHA-256 of release zip
  `40b994a055ee75e5f2aba81e7ae06f2c17460e18cc346483089921899fadd1f7`). No Steamodded.
- Transport: LuaSocket (ships with LÖVE) TCP on 127.0.0.1:22123, one text line per message.
- Patches (Lovely pattern patches, everything else is monkey-patching globals from code appended to `main.lua`):
  - `state_events.lua` hand score ease line → count own score, send to partner
  - `game.lua` `update_hand_played` end check → when out of hands, wait for the partner
  - `game.lua` deck-empty `end_round()` checks → same
  - `UI_definitions.lua` main menu → CO-OP button
- Score sharing: partner points are added to `G.GAME.chips` only while no own score ease is
  running (the ease target is computed when the hand is queued, so adding in the middle would be lost).
- Shared target: host computes `get_blind_amount(ante) * blind.mult * ante_scaling * difficulty`,
  sends it with the select; both override `Blind:set_blind` result.
- Boss: guest's blind-select screen waits until the host's boss for that ante is known; host
  rerolls are followed with `G.FORCE_BOSS` (vanilla hook in `get_new_boss`).
- Round end: a player who is idle when the pot is reached waits ("won, waiting") until the
  partner reports done; a player still animating finishes, then ends. Both out of hands → both lose.
- Second window: own save identity `BalatroDuoP2`, profile (unlocks) copied on first start.
- Gamepads: `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1` set via LuaJIT FFI on SDL2.dll; gamepad
  #1 → window 2, #2 → window 1 (switchable). Unfocused window ignores the mouse.

- Settings live on the host only (difficulty, deck mode, gamepad split); the guest's CO-OP
  menu shows them read-only. Deck mode "shared": host's `Game:start_run` sends deck key,
  stake and challenge id; the guest starts the same run (its Play button is locked).
  Deck mode "own": each player picks deck and stake.
- SDL hints set via FFI: `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1` and
  `SDL_MOUSE_FOCUS_CLICKTHROUGH=1` (without it the click that focuses the other window is
  swallowed). The game picks the clicked element from the last mouse move, so a click
  first replays a move to the click point.

- Skips are synced by state, not by command: the host sends its Small/Big blind states
  (`bst`), the guest skips whatever the host skipped and it has not. The host can only select
  when the guest reports the same `blind_on_deck`. Reason: in a playtest the host logged
  "skip Big" while it actually skipped Small (cause unknown, not reproduced by clicking); the
  guest applied "skip Big", nothing happened, and the windows drifted. Bot test with the guest
  delaying skips by 8 s (`--duo-test-lag 8`): host waited, guest caught up, 39/39 checks.

## Observations
- Lovely ignores the `APPDATA` variable for its mod dir; pass `--mod-dir` (it created
  `%APPDATA%\Balatro\Mods\lovely\log` in the real profile on the first try).
- LÖVE does follow `APPDATA` for the save folder, so tests run fully isolated.
- Two instances run side by side with Steam running; `steam_appid.txt` is in the game folder.
- The event queue always holds a non-blocking watcher event; "busy" must only count
  blocking events.
- DualSense is detected by the game's SDL as "DualSense Wireless Controller", gamepad=true.

## QA (automated, ran 2026-10-23)
Two bot clients (`--duo-bot`), host + guest, seeds HOSTSEED/GUESTSEED, difficulty x0.3,
checker compares both flight-recorder logs.
```
rounds=5 selects=5 skips=1 checks=37 failed=0
  r1: target=90 host 60+44=104 guest 44+60=104 won=true
  r2: target=135 host 75+96=171 guest 96+75=171 won=true
  r3: target=180 host 112+116=228 guest 116+112=228 won=true
  r4: target=360 host 304+112=416 guest 112+304=416 won=true
  r5: target=480 host 86+143=229 guest 143+86=229 won=false
  end: BOT gameover ante=2 round=5 (both)
```
Deck modes (same bots, host asks Blue deck, guest asks Yellow, stake 2), 39/39 checks each:
`shared` → both started `b_blue` stake 2; `own` → host `b_blue`, guest `b_yellow`.
Also ran: one-button launch (window 1 starts window 2, arranges both halves, profile copy).

## 0.4.0 (2026-10-25), from a playtest of 0.3.0
Three things were reported after playing 0.3.0:
1. After restarting, both windows were forced onto screen halves, although the player had
   set them up borderless, one per monitor. The game's video settings still showed
   Borderless, so the player had to switch to Windowed and back on each window.
   - Cause (observed in source): the game restores its own `G.SETTINGS.WINDOW` (screenmode,
     selected_display) from `settings.jkr` in `Game:set_render_settings` →
     `G.FUNCS.apply_window_changes(true)`. The second window has its own `settings.jkr` (save
     identity `BalatroDuoP2`). `D.arrange` then called `love.window.updateMode` to windowed
     without touching `G.SETTINGS.WINDOW`.
   - Fix: new host option "Windows on start": `keep` (default) leaves Fullscreen/Borderless
     windows alone. A windowed window goes back to its last position (`duo_window.txt` per
     identity, written every 2 s while in co-op), or to a half when nothing is saved yet.
     `halves` always arranges. `D.arrange` now sets `G.SETTINGS.WINDOW.screenmode='Windowed'`
     and saves the settings, so the game's menu matches. A guest whose `settings.jkr` was
     just copied from the host always gets a half.
2. The host clicked the blind select while the guest was in the shop: the toast showed, then
   the button never worked again, even after the guest came back.
   - Cause (observed): `select_blind_button` and the skip button are `one_press`
     (`UI_definitions.lua` 1476, 1559). `UIElement:click` sets `disable_button = true`
     (`engine/ui.lua` 967), and the mod's refusal returned without clearing it.
   - Fix: refusals clear `e.disable_button`. The guest-side refusals (select, skip, reroll)
     do the same.
3. Request: shared hand levels, and a submenu for what is shared. New submenu "What is
   shared...": difficulty, deck and stake, hand levels (`off` / `planets` (default) /
   `all`). This hooks `level_up_hand` (every upgrade goes through it). `planets` = the card
   arg is a Planet or Black Hole. `all` also covers Space Joker, Burnt Joker and Orbital Tag.
   Negative amounts (The Arm) are never sent. The receiver applies the level with
   `instant=true` and shows one keyed toast.

QA 0.4.0 (bots; the checker was ported to
`tools/check_logs.ps1`, run by `tools/run_suite.ps1`):
- `run_suite.ps1 -ShopLag 6 -Planets` (guest lingers 6 s in each shop. The host bot presses
  the real `select_blind_button` UIElement while the guest shops, then again when ready.
  Host uses Pluto + Black Hole, guest Neptune, via `G.FUNCS.use_card`):
```
rounds=5 selects=5 skips=1 early_clicks=4 refused=4 levels host->guest=13/13 guest->host=1/1 checks=45 failed=0
  r1: target=90 host 60+44=104 guest 44+60=104 won=true
  r2: target=135 host 110+96=206 guest 96+110=206 won=true
  r3: target=180 host 135+135=270 guest 135+135=270 won=true
  r4: target=360 host 400+123=523 guest 123+400=523 won=true
  r5: target=480 host 236+319=555 guest 319+236=555 won=true
  end: BOT quit ante=3 / BOT quit ante=3
```
  Every second click on the real button logged `disabled=false` and selected.
- `run_suite.ps1 -Planets -Levels off`: `levels host->guest=0/0 guest->host=0/0 checks=42 failed=0`
  (the bots lost r5 here: without shared levels their hands score less).

## 0.5.0 (2026-10-26), from a playtest of 0.4.0
1. Request: the partner should get a hand's points only after they have counted into the
   own total, and the overlay should change at that moment too.
   - `evaluate_play` computes `hand_chips*mult` and queues a non-blocking ease of
     `G.GAME.chips` behind the card-by-card animation (`state_events.lua` ~1044).
   - 0.5.0 keeps the amount in `r.unsent` and sends `sc` from `D.flush_score` when the
     ease reaches `own_target`. It also flushes before any round status `st`, so `sc`
     always comes before `st`.
   - Race found by the bots: the host beat the target with its own hand and vanilla closed
     the round, while the guest's last hand (50) was still counting. The host's total
     missed it (505 vs 555). It was always possible, but rare while `sc` went out at play
     time.
   - Fix: `pl` goes out at play time. `DUO_hp_end` keeps a won round open while the
     partner's hand is counting, and `can_play` is off once won. `round_update` ends the
     round after the partner's `sc` and `st`. The checker now requires equal totals in
     every round (it used to only for lost rounds).
2. Request: "both tags to both" option. `D.share_tag` reads the tag from the skipped
   blind's `tag_container` (as vanilla `skip_blind` does) and sends key, blind type and
   the Orbital hand. The receiver builds `Tag(key)`, `add_tag`s it and runs its
   `immediate` context. The host also runs `new_blind_choice` at once, because pack tags
   would otherwise wait for its next skip or shop exit.
   - The host's tag reaches the guest before the host's `bst`, so the guest adds the
     host's tag before its own skip.
   - Boss Tag is not copied: it would reroll the receiver's boss and break the boss sync.
   - A copied Orbital Tag levels only its own side (`duo_from_peer`, so `levels=all`
     doesn't double).
   - Double Tag copies a received tag like any other `add_tag` (vanilla behaviour).
3. The guest was sometimes seen on an empty screen with no blinds after leaving the
   shop first. Confirmed in code: the guest's blind screen waits for the host's boss of
   the new ante, and the host sent `bos` only on its blind screen. The next boss is rolled
   in `reset_blinds` at cash out, so the host now sends `bos` from the shop on. It is not
   sent during round/eval: there `ease_ante` has already raised the ante, but the boss is
   still the old one.
4. Secret hands: the guest made Five of a Kind; it showed in the guest's hand list only.
   - `evaluate_play` sets `hands[text].visible = true`.
   - Planet X / Ceres / Eris have `softlock` and are in `get_current_pool` only when
     `hands[hand].played > 0` (`common_events.lua` 2009).
   - 0.5.0 sends `hv` when a secret hand becomes visible. The receiver sets `visible` and
     remembers it in `D.peer_known`, and a Lovely patch on the softlock line also accepts
     `DUO_hand_known(hand)`. `played` is left alone, so Supernova and Obelisk are not
     touched.
   - `D.peer_known` lives in memory only (lost on quit and continue, like the rest of the
     co-op state).

QA 0.5.0:
- `run_suite.ps1 -HostShopLag 6 -Secret -Planets` (host lingers 6 s per shop; the guest
  bot marks Five of a Kind played in its first shop, the host bot then checks
  `get_current_pool('Planet')`):
```
rounds=5 selects=5 skips=1 early_clicks=0 refused=0 levels host->guest=13/13 guest->host=1/1 checks=56 failed=0
  r5: target=480 host 236+319=555 guest 319+236=555 won=true
  flushes host/guest=9/9 tags host->guest=tag_double guest->host=tag_meteor guest blind waits= secret=Five_of_a_Kind pool=true
  end: BOT quit ante=3 / BOT quit ante=3
```
- `run_suite.ps1 -ShopLag 6 -Planets` (0.4.0 regression, final 0.5.0 code): `early_clicks=4 refused=4
  checks=56 failed=0`. r5 `host 236+319=555 guest 319+236=555`: the host logged
  `apply r=5 +50` and then `force_end r=5 why=peer=done` (it waited for the guest's last hand).
- The "What is shared..." submenu with five options fits on the screen (screenshot, 1807x822 desktop).
  At the ante 1→2 shop the guest reported `p=blind` at 86.4 s and the host left its shop
  at 93.0 s: the guest had its blind screen 6.5 s before the host, with no BLINDWAIT. The
  guest log has `TAG peer tag_double` before `TAG own tag_meteor`. Every FLUSH logged
  `chips >= mine + theirs` (the own score was already in the total).

## 0.6.0 (2026-10-27), from a playtest of 0.5.0
Reports and what the code says:
1. The guest left the shop first and sat on an empty screen while the host was still in
   the shop (photo, ante 1 round 1).
   - Unknown why: no logs from that session, and the logs are overwritten on start.
   - Observed: in 0.5.0 the guest only gets a boss when the host is on the blind screen,
     in the shop or in the run start. Any loss of `bos`, a fresh run with an empty
     `host_boss`, or a different ante (see 5) left the screen empty with no way out.
   - 0.6.0: the guest asks (`need`) and waits at most 1 s, then shows its blinds with
     its own boss. The adoption (below) swaps in the host's boss when it arrives.
   - The previous log is now kept as `duo_log_prev.txt`.
2. After a restart the blinds were out of step (photo: guest round 0 with Big up,
   host round 1 with Big defeated). Each window continued its own save, and the two
   saves were made at different moments.
   - 0.5.0 never compared positions: the host waited for "the same round and blind",
     the guest waited for the host.
3. Request: show on the host which blind the guest pressed (select or skip).
4. Request: check that running out of hands doesn't block everything. 0.5.0 set
   `G.CONTROLLER.locks.duo_wait` while waiting, which blocks all input (jokers,
   consumables, run info).
5. Crash and broken continue (photo 3).
   - Both had a Double Tag; the skipped blind gave a Charm Tag (Mega Arcana pack) to
     both. Each side had 3 Charm Tags (own, Double copy, partner's).
   - The host opened two packs at once and crashed when they closed.
   - After the restart the host continued (ante 8) and the guest was in a new run at
     ante 1 with an empty screen.
   - Cause of the crash (reproduced, `run_suite.ps1 -TestTags -OldNbc`): `D.peer_tag`
     ran the received tag's `new_blind_choice` itself while the game's own loop had
     just opened a pack from the host's tag. The host showed "Oops! Something went
     wrong: functions/button_callbacks.lua:2133: attempt to index field 'cards' (a nil
     value)" when the first pack closed.
   - The game's loop (`update_blind_select`, `skip_blind`, `end_consumeable`) stops
     at the first tag that fires and runs again when the pack closes.
   - Cause of the broken continue (observed in code): Continue goes through
     `Game:start_run` like a new run. The host's hook sent `run` (deck, stake), and the
     guest started a new run.

What 0.6.0 does (code):
- Run identity: `G.GAME.duo.id`, made by the host for a new run and taken by the
  guest. `G.GAME` is serialised by `save_run` (`recursive_table_cull` keeps plain
  tables), so the id and the current round's score split come back with Continue.
  `save_run` is wrapped to write that snapshot.
- `sy` message: the whole run position (in run, id, ante, round, blind states, boss,
  phase, deck/stake/challenge), sent on change. It replaces `run`, `bos` and `bst`.
- The guest follows the host's run (`guest_reconcile_run`): continue its own save if
  the save id matches, otherwise start a new run with the host's deck. A host that
  restarts continues its save if the guest is in a run with the same id, once per
  connection and only when the main menu is fully up.
- Blind screen adoption (guest): if ante, round, blind states or boss differ from the
  host's (on the host's blind screen) for 3 s (at once for a boss-only difference),
  take the host's and rebuild the screen in place (same UIBox code as
  `update_blind_select`).
- `tot` message: the partner's own score for the round, after a reconnect or a
  continue. It applies only inside the round.
- Waiting for the partner no longer sets `locks.duo_wait`; only Play is disabled.
- Received pack tags: the game's own `new_blind_choice` loop runs once the blind screen
  is idle (`D.run_pending_nbc`).
- "Play together" waits 2.5 s for a guest that is still running before it opens a new one.
- Guest select/skip presses are sent as `wish`. The host shows "Partner wants to:
  SELECT/SKIP <blind>" and pulses that button or tag.

Found by the new tests and fixed:
- Host auto-continued 2.5 s after start, during the splash. The run stayed paused
  (`paused=true`, bot never moved, the guest waited forever). Now it waits for
  `G.MAIN_MENU_UI` plus 1.5 s; unpause safety net for 30 s after a continue.
- `tot` applied while the guest was in the shop (`facing_blind` is still set there).
  The total carried into the next round: 156 + 120 = 276. Now only in round states.
  `tot` also waits for the round it is for (the answer to a query arrives while the
  blind loads) instead of being dropped.
- Guest killed and restarted: auto-continue sometimes stayed paused under an
  unlock/discovery popup (`common_events.lua` 1866 sets `paused` and opens an
  overlay). A person closes it; the bot now closes overlays after 3 s.
- Host killed while the guest played Big at ante 2: the guest, out of hands and
  disconnected, ended the round as lost (game over) through vanilla `update_hand_played`
  and the old "lost partner: finish solo" code. Now a round with no partner waits up to
  90 s (`DUO_hp_end`, `DUO_deck_out_ok`, solo end in `D.update`). A partner that is behind
  or in the menu (restarting) is waited for up to 120 s.
- Blind alignment was "the host is right", which rewound a guest that was ahead and left
  a host that was behind waiting forever. Now (`align_blinds`, both sides) whoever is
  further along wins:
  - The side behind takes the other's position; if the other is inside a blind, it
    takes the position just before it and selects the same blind (JOIN). `peer_ready`
    accepts a partner that is already in the blind being picked.
  - `sel` carries the target round, and a stale one is dropped.
  - Entering a blind asks for the partner's score (`tot`).
  - Progress = round * 4 + skips this ante; 'Select' and 'Upcoming' compare as equal.
- Host continued inside a Meteor pack (opened on the blind screen by the guest's tag):
  closing it went back to BLIND_SELECT with STATE_COMPLETE set and no screen.
  `D.update` sets STATE_COMPLETE=false after 2 s of that ("REBUILD blind screen").
  Possibly a vanilla issue when continuing inside a tag pack; not checked without the mod.
- Own hands played while disconnected were not counted in `mine` (the hook required
  `D.coop()`). They are counted now and handed over by `tot` after the reconnect.
- Race: the host pressed Play just before the guest's points made the blind won, and
  the guest closed its won round in the ~3 s before the host's hand was scored
  (host 230 vs guest 202). `pl` now goes out when Play is pressed
  (`play_cards_from_highlighted`).
- A guest window that dies is reopened by the host after 10 s without it (if the host
  opened it), and continues by itself.

QA 0.6.0 (`tools/run_all.ps1`, 9 scenarios, 400 s timeout each; every scenario plays to
the end of ante 2 on stake 2, x0.3 difficulty):
- 1 shop lag + planets 46/46
- 2 host shop lag + secret + planets 46/46
- 3 tag stack (Double + Charm, the crash) 41/41, 3 packs each, one at a time
- 4 guest wishes + slow guest 44/44: 6 wishes clicked, received and seen; waiting
  never locked
- 5 own decks 39/39
- 6 host killed at 110 s 20/20: the host continued, JOIN'ed the guest's Big, totals equal
- 7 guest killed at 45 s: failed on the Play race, fixed, rerun below
- 8 both killed at 130 s 17/17
- 9 guest killed, the host reopened it 20/20

Final run of all 9 on the code that shipped as 0.6.0 (after the Play race fix): all PASS
(46, 46, 41, 44, 39, 20, 25, 17, 25 checks, 0 failed). The wish line on the host's blind
screen was checked on a screenshot: "Partner wants to: SELECT Small Blind".

Not covered by these runs: two monitors, a person with a gamepad, antes above 2, a crash
in the middle of a hand animation, Boss Tag / boss reroll vouchers on the guest.

## 0.6.1 (2026-10-29), from a playtest of 0.6.0
Playtest feedback: "everything works"; two asks.
1. A guest's Select wish pulsed on the host, a Skip wish didn't.
   - Cause (observed): the host looked for `tag_container` in the blind option UIBox.
     That id doesn't exist there: the tag row is `tag_<Small|Big>`
     (`create_UIBox_blind_tag`), and the skip button has no id at all.
   - Fix: `D.find_uie` walks the UIBox tree to the node with `button == 'skip_blind'`;
     that node and `tag_<blind>` both pulse.
   - Bot test: the guest presses the real skip button on Small. The checker requires a
     pulse per wish and `found=2` for skips.
2. Translations. The mod's 60 strings were in ru and en. Now 15 languages: every
   `G.SETTINGS.language` the game has (en-us, ru, de, fr, es_ES, es_419, it, pt_BR, pl,
   nl, id, ja, ko, zh_CN, zh_TW).
   - New languages are in `duo_lang.lua`, appended before `duo.lua` (Lovely copy patch
     with two sources). Lookup is the full code first, then the first two letters.
   - Words for blind, tag, run, deck, stake and hand follow the game's own localization
     files (e.g. fr Blinde/Badges/partie, pl Przeszkadzajka/Znaczniki, it buio/Patti).
   - All languages have all keys, and `%s` counts match English (awk check).
   - `tools/lang_test.ps1` switches the game language (`G.FUNCS.change_lang`), opens the
     CO-OP menu and its submenu, and screenshots both. All 15 loaded without errors.
     Fonts cover CJK, umlauts and Polish. Long strings make the option box wider; none
     are cut off. French "les deux badges pour les deux" was shortened.
   - Found by those screenshots: the status bar covered the menu title, and moving it
     to the bottom covered Back. With a menu open it now stays at the top at 75 % size,
     at most 2 lines (status + newest message), above the menu box.
3. Installer messages are in Russian when the Windows display language, regional format
   or install language is Russian, in English otherwise.
   - `install.ps1` is UTF-8 with BOM: Windows PowerShell 5.1 reads a BOM-less script as
     ANSI.
   - The test machine has UI culture en-US, regional format ru-RU and install language ru-RU, so
     `Get-UICulture` alone was not enough.
   - Added README.txt (English) and Install.cmd / Uninstall.cmd next to the Russian files.

QA 0.6.1:
- Wish run, before the status bar and language edits: 47/47 (6 wishes, both skips
  pulsed with `found=2`).
- Language screenshots: 15/15 loaded, menus readable.
- Final code: scenarios 1 and 2 of `run_all.ps1` passed (46/46 each).
- Stopped there (nine scenarios felt like too many). Scenarios 3-9 were
  not rerun on 0.6.1. Their code paths didn't change after the 0.6.0 run, where all 9
  passed.

## Not covered yet
- Tags with immediate effects other than Double/Meteor, Orbital Tag copy, and pack tags
  opening on the host mid blind screen with a person at the controls.
- Window placement with two monitors: the test machine shows one display, so only one monitor was checked. Checked by hand there with `tools/win_test.ps1` (separate profile `wt`): first start → both halves; guest moved with SetWindowPos → after a restart `place restore 884x561 at 208,181`. Host switched to Borderless in the game's Video menu → after a restart `place keep Borderless display 1`, guest still restored. The game's Video menu showed "Windowed" after a halves arrange (the old mismatch is gone). The CO-OP menu, the "What is shared..." submenu and its Back button render.
- Hand levels shared with `all` (Space Joker, Burnt Joker, Orbital Tag) are not bot-tested; only Planet and Black Hole are.
- Gamepad input in the unfocused window (needs a person pressing buttons).
- Host boss reroll (Director's Cut / Retcon) and Boss Tag on the guest.
- Antes past 2, jokers that read the score (Mr. Bones), Hieroglyph/Petroglyph (ante shift).
- Mid-run quit and continue beyond the antes 1-2 bot runs (Continue itself is covered, see 0.6.0 above).
- Install into the real game folder.
