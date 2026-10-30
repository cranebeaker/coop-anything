# BLACK SOULS II (2018)

| | |
|---|---|
| Engine | RPG Maker VX Ace (RGSS3, Ruby 1.9.2) |
| Versions tested | Steam 1.0 ([app 3855540](https://store.steampowered.com/app/3855540/), build 25767755; censored release, 2025, 18 maps); Steam + official full-version patch 1.4 + the Spanish fan translation (411 maps): translated by the Hispachan community, adapted to the Steam version by Yotichi ([Steam guide](https://steamcommunity.com/sharedfiles/filedetails/?id=3614515422)) |
| Projects | BS2 Coop: private build, code not public (directory entry "BS2 Coop") |
| QA level | automated |
| Last updated | 2026-10-30 |

## Where things stand

A private mod lets 2-4 players play together over direct TCP (a gaming VPN or a forwarded port):
everyone sees the others walking on the same map, story and world flags are shared, and any player
can join another's battle, where enemy damage and deaths are shared. Real-client bot suites pass on
both the Steam 1.0 build and the patched, translated build. Coverage is the scripted scenarios listed
under QA, run with two clients on one PC over localhost.
The code is not public; this write-up records how it works so the next agent can rebuild it.

## Existing work

None found for this game (web search for BLACK SOULS II co-op / multiplayer, 2026-10-30; every
result was about Dark Souls II). The closest relative is Fear & Hunger Together (RPG Maker MV), but
MV runs JavaScript in a browser engine, so nothing carries over directly except ideas (census of
switches from game data, separate characters, bots driving real clients).

## How the game works (for co-op)

- **Data and scripts.** Everything is in `Game.rgss3a`: an XOR archive (`RGSSAD\0\3`, key
  `k*9+3`, per-file key `k*7+3` per 4 bytes) holding `Data/*.rvdata2` (Ruby Marshal) and
  `Data/Scripts.rvdata2` (an array of `[id, name, zlib(code)]`). A 60-line C# reader unpacks it;
  Windows ships `csc.exe` (.NET Framework), so no SDK is needed.
- **The archive wins.** While `Game.rgss3a` exists, RGSS3 loads scripts and `load_data` from the
  archive only; a `Scripts=` path on disk fails with "Failed to load script". To inject a mod without
  repacking: extract the archive next to `Game.exe`, rename it, and point `Game.ini` `Scripts=` at a
  one-section file that evals `Data/Scripts.rvdata2` section by section and the mod's own `.rb` files
  just before `Main`. That works for any build or translation of the game.
- **Unfocused windows freeze.** RGSS3 stops inside `Graphics.update` after `WM_ACTIVATEAPP(FALSE)`
  (posting that message to a running copy froze it; posting `TRUE` resumed it). A session would stall
  whenever anyone alt-tabs. Fix from Ruby alone: write a 60-byte x86 window procedure into
  `VirtualAlloc`'d memory that drops `WM_ACTIVATEAPP(FALSE)` and `WM_ACTIVATE(WA_INACTIVE)` and forwards
  everything else with `CallWindowProc`, install it with `SetWindowLong(GWL_WNDPROC)`, then post
  `WM_ACTIVATEAPP(TRUE)` to yourself in case another window took focus before the shim existed. Ignore
  keyboard input while `GetForegroundWindow` is not the game.
- **Networking.** No netcode at all. `Win32API` against `ws2_32` gives non-blocking TCP (`ioctlsocket
  FIONBIO`, `select` for connect completion) with no extra DLL. Receive buffers must be binary
  (`force_encoding("ASCII-8BIT")`): a UTF-8 buffer sliced by characters corrupted frames as soon as a
  Japanese sprite file name was sent.
- **Switches and variables.** Names are stripped in the shipped data, so the mod classifies them by
  how event lists use them: operands that are random, game data or script, transfer and picture
  coordinates, writes inside battle (troop) events and parallel events, flags set ON and OFF in one
  event, and anything the game's Ruby scripts reference stay per-player; everything else written by
  events is world state, plus all self switches. On the full game this takes 0.74 s for 410 maps.
  Examples from this game: variable 4 is the random roll that picks a symbol enemy's troop; variables
  21-23 hold the soul-drop map and position and 29-31 the last bonfire.
- **Battles.** Symbol encounters on the map run `Battle Processing` with "can lose"; the lose branch
  calls common event 11 (soul drop, "YOU DIED", respawn at the bonfire). The battle system is a custom
  ATB: between actions `Scene_Battle#start_party_command_selection` spins a wait loop calling
  `battlers_frame_update`, which is the safe point to apply another client's damage. If a remote kill
  ends the fight inside that loop, `BattleManager.judge_win_loss` runs there and a private exception
  unwinds to `Scene_Battle#update` / `post_start`.
- **Translated builds.** The Spanish fan translation adds Fullscreen++ (Zeus81; F5/F6, writes a
  `[Fullscreen++]` section into `Game.ini`, grabs the first "RGSS Player" window on the PC) and Steam
  achievements via `steam_api.dll`. An installer must change only the `Scripts=` line of `Game.ini`.
- **Test traps.** Starting a new game without the intro leaves placeholder actor 1 with ~27x stats
  (270k HP, 16k ATK): fights one-shot and enemies "aren't affected". The intro (map 7) swaps in one
  of the origin characters (actors 2-6). The player also starts transparent until the intro shows them.

## What we changed

Built from zero:

- Loader as above, and an installer that unpacks whatever archive the player has, backs up every file
  it overwrites, and uninstalls cleanly.
- Host/client sessions over TCP (star topology, host relays), handshake with mod version and a CRC of
  `Data/*.rvdata2` so mismatched game data refuses to connect, safe custom serializer (no `Marshal`
  over the network).
- Shared world: census, switch/variable/self-switch hooks, host snapshot on join, remote changes held
  until the local map interpreter is idle.
- Presence: other players drawn as interpolated characters with name tags; gesture balloons; travel to
  a partner.
- Joinable battles: the joiner starts the same troop with their own party; enemy HP loss and deaths are
  broadcast; a joiner who loses runs the same death event the starting player's battle event would.
- UI in Spanish with keyboard text entry (`ToUnicode`, live modifier state, Ctrl+V paste) and a list of
  the host's IPv4 addresses (LAN or VPN) to give to the other players.

Bugs the bot suites found and fixed: the unfocused freeze; the focus race at startup; the UTF-8 receive
buffer; `Graphics.frame_count` being rewritten on load (breaking timeouts; the mod keeps its own
counter); Shift lost because `GetKeyboardState` lags behind injected or fast key presses.

## QA

- A bot runs inside each real game client (only when `COOP_BOT` is set), drives a scenario through the
  game's own objects and writes PASS/FAIL lines; a PowerShell runner starts 2 clients side by side
  (visible, so a tester can watch them) and captures the game windows with `PrintWindow`.
- Scenarios: handshake and names; unicode round trip; world snapshot on join, switch/variable/self-switch
  sync both ways, a per-player variable staying local, presence and interpolation; Knight + Deprived
  (level 5) vs Headless Soldier with the F7 join path; three Star Spirits with kills from both clients;
  a joiner's defeat running the death event and respawning at the hub; menus and travel; both players
  choosing Attack through the real actor command window; real keyboard entry (only when the window has
  focus, otherwise SKIP so keys never go to another app).
- Suite summary, 2026-10-30 (`run-all.ps1`, one run per build, two clients on one PC over localhost;
  counts are checks passed / checks run, per client):

  | Scenario | What it asserts | Steam 1.0 (host / guest) | 1.4 + Spanish (host / guest) |
  |---|---|---|---|
  | net | handshake (mod version, data CRC), names, host IP list | 4/4 / 3/3 | 4/4 / 3/3 |
  | unicode | Japanese and Spanish text, 70 KB frame round trip | 3/3 / 3/3 | 3/3 / 3/3 |
  | world | snapshot on join, switch/variable/self-switch sync both ways, per-player variable stays local, presence | 7/7 / 7/7 | 7/7 / 7/7 |
  | battle | Knight + Deprived (level 5) vs Headless Soldier, join path, shared damage, win, troop dead | 8/8 / 8/8 | 8/8 / 8/8 |
  | battle3 | three Star Spirits at level 30, kills from both clients | 8/8 / 8/8 | 8/8 / 8/8 |
  | death | level 5 vs three Star Spirits; the guest's defeat runs the death event and respawns at the hub | 4/4 / 5/5 | 4/4 / 5/5 |
  | ui | main menu entry, co-op menu, address box, travel to the partner | 3/3 / 3/3 | 3/3 / 3/3 |
  | manual2 | both players pick Attack through the real battle command window (no auto-battle) | 7/7 / 7/7 | 7/7 / 7/7 |
  | typing | real keyboard entry with Shift, Backspace, Enter (one client) | 2/2 / - | 2/2 / - |
  | **Total** | | **46/46 / 44/44** | **46/46 / 44/44** |

  In an earlier run the guest of the death scenario reported no result on Steam 1.0: the host's client
  closed while the guest was still fighting, which ended the session before the guest's battle finished.
  The mod now records the battle result even when the session ends mid-fight, and each bot waits for its
  partner to leave battle before closing; the scenario then passed twice in a row on each build before
  the run above.

## How to play

Private build, not distributed. Its installer finds the Steam folder, unpacks the archive and adds the
mod; in game: menu > Cooperativo > host or join, F7 joins a partner's battle, F8 shows a gesture.
Internet play needs Radmin VPN, ZeroTier or Tailscale, or TCP port 47800 forwarded to the host. All
players must use byte-identical `Game.rgss3a` files (same patch and translation).

## Known issues

- Wandering symbol enemies move independently on each screen; only their defeated state is shared.
- In shared battles only enemy HP loss and death are synced, not enemy heals or states.
- A cutscene might fire for a partner when a shared flag flips. The suites cover flag sync and the death
  event, not story cutscenes.
- Joining adopts the host's world progress; players should use a separate save slot for co-op.
- The host leaving ends the session.

## Next steps

- A co-op playthrough on the 1.4 build to confirm the census choices and cutscene behaviour.
- Host-authoritative positions for symbol enemies per map.
- Enemy states and heals in shared battles; partner actions in the battle log.
- Run the suites across two PCs and through a VPN (all runs so far used one PC over localhost).
