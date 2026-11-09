# DDCoop launcher

`DDCoop.exe` is the co-op window for Darkest Dungeon. It handles Host/Join and saves before launch, and publishes the in-game F9 panel state. Players
install it with `Install DDCoop.bat`, open it, click **Host een spel** or **Join deze vriend**, and the
launcher does the rest:

- the host's launcher writes `mods\DDCoop\ddcoop.ini` (role=host, a fresh 32-character password, a free
  port on 127.0.0.1) and starts the game;
- the friend's launcher joins the host's friends-only Steam lobby, receives the password and the host's
  campaign, in the slot number the host chose (after backing up its own slot of that number in `%USERPROFILE%\DDCoop-backups`), writes its ini
  (role=guest, host=127.0.0.1, its local relay port) and starts the game with `-nosave`;
- the host chooses the campaign in a dropdown: slots 1-9 with their estate names (`lr_list_campaigns`, base_root.estatename), remembered in `ddcoop-slot.txt`; profile 9 is never offered and `lr_slot_folder_ok` refuses it everywhere;
- checks on the campaign's `persist.game.json` (`lr_game_parse`): the host's launcher refuses a campaign
  that is still at the start of the tutorial (never saved; the friend's game would show an empty slot), and the
  friend's launcher refuses a campaign whose DLC it lacks (folders under `<game>\dlc`; the game would call the
  campaign corrupted and start a new one) before its own save is touched, naming the DLC;
- the co-op runtime's TCP connection travels between the PCs over Steam (SteamNetworkingMessages, AppID
  480): no IP address, port forward, firewall rule or password to type;
- F9 shows a compact panel rendered inside the game; when the game closes the ini goes back to `role=off`, so a
  normal start from Steam stays single player;
- test9 prepares the exact host snapshot through a separate process using the installed game's
  `ISteamRemoteStorage` API, before starting `Darkest.exe`. It verifies AppID 262060 and the expected Steam
  account, backs up the API-visible original, imports and reads everything back byte for byte. Both the
  slot root and `profile_2/backup` are prepared; the latter is mirrored from the current host snapshot;
- the Windows guest coordinator leaves its original raw files intact while receiving. It first preserves
  the original root and backup files in a verified, flushed `ddcoop-guest-original.bin`, then delegates the
  replacement to the preparation child. After game exit the same child restores the exact original tree.
  A pending recovery file survives launcher/game failure and is retried at the next launcher start;
- host games save progress when "Voortgang opslaan als ik host" is ticked (remembered in `mods\DDCoop\ddcoop-save.txt`;
  without that file the build decides: on, or off in a `DDC_TEST_NOSAVE=1` build). Before each session DDCoop keeps
  the slot as it was in `%USERPROFILE%\DDCoop-backups\Steam-prepare-*.bin`; "Back-up van dit slot terugzetten" puts
  the newest one of the chosen slot back through the same Steam helper, after saving the current state as
  `Steam-undo-*.bin`. Guests always use `-nosave`; because that flag does not prevent every
  game write, verified restoration is still required. Preparation/recovery never targets another slot;
- a pinned installed `SDL2-bootstrap.dll` repairs the known Steam-restored-original case automatically.
  The log bundle includes preparation, restoration and `app.log`; guest exit/failure collects it automatically.
- the hero list on the right shows the heroes of the co-op campaign; each player ticks the heroes they command
  in the dungeon, the host's launcher decides, and both write `mods\DDCoop\ddcoop-heroes.ini` for the runtime
  (design and file format: `OWNERSHIP.md`).
- test11 groups the host's saved current party (status 0, up to four heroes) first, with a gold background,
  bold names and a dot marker. Owners/checkboxes remain keyed by hero ID. Within each group, the order is
  stable hero ID, not dungeon rank. The existing five-second roster refresh shares saved changes with guests;
- the left checklist is a wrapping, read-only text control with a vertical scrollbar and keyboard access.
  Status refresh preserves its selection and scroll position.

The test12 runtime attempts all eight owner-reported gaps: estate trinket sale; drag-based equip,
unequip and inventory order; dismissal; renaming; active learned combat/camping skills; normal heirloom
exchange; party composition/rank. Each command is host-authoritative and uses local native game operations
after bounded identity/state checks. No live pointers travel over the network. The portable Hamlet model,
actual runtime hooks/apply fixtures and AddressSanitizer checks pass; the first two-PC test is pending.
Protocol 3 carries quoted sale/move/name/skill/party state so concurrent stale actions are refused.
Bulk auto-sort and unequip-all buttons are guarded; use individual drags. Keep both expedition screens
open for the first party test. Base-game exchange ratios only. Dungeon/raid transitions remain future work.

The runtime renders F9 with OpenGL just before SDL swaps. The launcher publishes a versioned read-only
snapshot per game PID; stable-ID checkbox commands return to the existing lr_core_claim handler through
a local registered window message. No passwords, Steam IDs or save data are published in that snapshot.
The panel includes connection/runtime status, sorted/highlighted heroes, scrolling checks, logs, copy code
and open-launcher buttons. Host/Join and invites stay in the prelaunch window. The old global F9 hotkey is
unregistered while the game runs. Outside mouse clicks still belong to the game. Native/GL fixtures verify
input filtering, graphics-state restoration, stale/torn snapshots and actual launcher publication. The owner
must still confirm behavior above the real game. Hero ownership is a table; dungeon control is not built yet.

Test11 added normal Stagecoach
recruit requests. Guests request before their roster mutates; the host decides and sends ordered applies.
Remote apply calls the game's Roster::AddEntry followed by the normal source completion callback, using a
validated, genuine live recruit display. A closed Stagecoach defers the ordered queue and asks the player
to open it. Roster capacity and source identity are checked; special roster-limit candidates and shard
recruits are outside this first test. No manual roster writes or PvP/analytics calls are introduced.
The owner confirmed host and guest recruits on 3 Nov; logs confirm single additions/source consumption
and verified guest restoration. Survivalist purchases were also reported successful, with ordered upgrade
applies in the logs. Screenshot capture was unavailable for Codex's native UI fixture, so the highlight
colors were not visually inspected by Codex.

Test10a's normal Nomad purchases are preserved, including the 9999-slot bound and gold/item/stock checks.
The owner confirmed host and guest purchases on both copies on 2 Nov at 23:16, followed by verified guest
restoration. Debug currency and raid transitions remain unsynchronized.
Launcher protocol remains 2; both players must install test12's matching runtime hash.
Test12 changes the bootstrap: two local SDL exports (PollEvent and GL_SwapWindow), 843 verified forwarders.
The installer recognizes the pinned predecessor only with an intact Steam original. Repair never saves a
proxy as SDL2_orig.dll. Both payload/bootstrap remain SHA-256 pinned (`src/lr_hashes.h`).

## Layout

| Path | What |
|---|---|
| `src/lr_wire.c/.h` | Messages between two launchers; strict decoder (platform independent) |
| `src/lr_relay.c/.h` | Carries the runtime's TCP streams over peer messages; host retries while the game loads |
| `src/lr_core.c/.h` | Session: HELLO, password hand-over, campaign transfer with backup, ddcoop.ini, keep-alive, hero ownership |
| `src/lr_roster.c/.h` | Reads the hero list from the game's binary save `persist.roster.json` (platform independent) |
| `src/lr_owners.c/.h` | Hero ownership table: rules, `ddcoop-heroes.ini`, the `LR_ROSTER` body (platform independent) |
| `src/lr_files.c/.h`, `src/lr_sock.c/.h` | UTF-8 file helpers (wide APIs on Windows), non-blocking loopback TCP |
| `src/lr_steam.c/.h` | Windows: the game's own `steam_api64.dll` through the flat API (lobby, invites, friends, messages) |
| `src/lr_install.c/.h` | Windows: `--install`, `--repair`, `--uninstall`, `--start` (what the .bat files call) |
| `src/lr_snapshot.c/.h` | Bounded snapshot format, complete storage comparison, backup-before-write and verified rollback |
| `src/lr_prepare_win.c/.h` | Separate AppID-262060 process; imports/restores only the snapshot's one slot via the existing Steam API |
| `src/main.c` | Windows: the window, checklist, hero list, F9, starting the game |
| `kit/` | The .bat files and LEESMIJ.txt that go into the zip |
| `tests/` | `test_lr_wire` (round trips + 1M fuzz), `test_lr_core` (two launchers end to end), `test_lr_roster` (save reader, truncations, 200k mutations), `test_lr_owners` (rules, ini, codec, 1M fuzz); `dson_writer.h` builds rosters for the tests |

## Build and test

From a Visual Studio 2022 x64 Native Tools Command Prompt:

    launcher\build-windows-msvc.cmd
    launcher\test-windows-msvc.cmd

The second command exercises the preparation child only against a test Steam ABI DLL in an isolated
build folder. It checks import, exact original restoration (including a different backup tree), wrong
AppID/account refusal and isolation from other slots. The owner must still test the real Steam client
and game load. API byte equality does not prove that game content can parse a campaign.

    sh launcher/build-linux.sh                 # Linux / WSL tests with ASan/UBSan
    CROSS= DDC_VERSION=1.2.0 sh launcher/build-windows.sh   # DDCoop.exe natively in MSYS2 UCRT64
    sh launcher/remote-build.sh 1.0.0          # DDCoop.exe via MinGW in CT116 -> build/win/DDCoop.exe
    python3 -I tools/make_dropkit.py <codex kit dir> <output dir>/DDCoop-install.zip 1.0.0

`test_lr_core` runs two launcher cores over an in-memory transport that refuses every third send (a full
Steam buffer) against fake games on real loopback TCP: campaign transfer and backup, 2 MB both ways through
the relay while the host's game is still "loading", close propagation, a stranger outside the lobby, a
second friend (refused: the runtime takes one guest), a corrupted chunk, a different game build, a game
already open, a silent host, and the ini going back to role=off. Hero ownership: the table built from the
host's campaign reaches the guest, tick/untick rules, refused ticks, a roster change while playing, choices
restored on the next host session, and a heroes file this PC got as a guest never restored.

## Historical pre-test9 questions (normal prepare/load/restore subsequently passed)

- The new preparation and automatic restoration flow has passed Windows automated tests with the fake
  API. It has not been exercised against the real Steam client or the game; no claim that the prior assert is fixed.
- Whether a hosting friend still shows as "HOST DDCOOP" once their game (AppID 262060) runs; invites and the
  code box are the fallback.
- The folder name Steam uses for slot 3 under `userdata\<id>\262060\remote` (`profile_2` or
  `steam_profile_2`; both are tried, and the launcher log lists what is there).
- Whether the guest loads the correct week/roster, can join, and recovers its original slot after game exit
  and after restarting a launcher that was stopped during the borrowed session.

## Placement test log review (2026-10-31)

The host's v5 log shows the first campaign transfer completed (19 files, 282,231 bytes) and the guest sent
`SAVE_RESULT` successfully. This confirms file transfer completion only; it does not confirm that the game can
load the received campaign. The host runtime became active, but there is no `direct TCP session ready` entry
after this launch, so the guest game did not reach the runtime handshake before leaving the lobby. The guest's
`HierarchicalFile::Reader::GetStringData` assertion is therefore a save-load failure that precedes in-game sync.

The same host launcher log later enumerates its own `profile_2` as 7 files / 29,010 bytes, down from the initial
19 / 282,231. No filenames or content checksums were logged, so the cause of that change is unknown. Preserve both
players' saves and compare the host's exact file list, sizes, and CRCs before another transfer. The launcher has no
game-level save validation and must not treat `SAVE_RESULT` as proof of loadability.

The host now snapshots its campaign when hosting starts, before launching the game, and logs each file's name,
size, and CRC. On the guest, each written file is read back and checked against the received size and CRC before
the launcher reports campaign transfer success. This verifies byte integrity, not whether the game can parse the
save; guest-load validation still requires a second PC. Before replacing an existing guest campaign, the
launcher now compares every backup file byte-for-byte with the source and checks that the file counts match.
If the backup is unreadable, incomplete, or differs, the transfer stops before it writes over the guest save.
