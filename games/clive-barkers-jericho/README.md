# Clive Barker's Jericho (2007)

| | |
|---|---|
| Engine | MercurySteam ScrapEngine (the Scrapland engine), Lua 5.1.1, D3D9, PhysX 2.7 |
| Versions tested | Retail DVD, `Jericho.exe` 1.0.0.1, SHA-256 `7fb5b759823283a83b685adc4848289a149f520ddbab6dbc7f9e6131b15d2a62`, Windows 10 and 11; also with SMOD 1.0.3 |
| Projects | [Jericho Co-op+](https://github.com/dolbaras/jericho-coop) (directory entry "Jericho Co-op+", called Jericho Co-op before 0.2.4); fork MAK-PIVO/jericho-coop (no longer public since 2026-10-22), merged back in 0.2.1 |
| QA level | automated (whole campaign over a LAN between two PCs, menus driven like a player with virtual gamepads, every story split with a real host and client); a first human session over Steam (missions 1-8) |
| Last updated | 2026-10-24 (0.2.7) |

## Where things stand

Two to four players can play the whole campaign together, each controlling one squad member
while the AI keeps the rest, over a local network, direct IP or Steam, and two of them can
share one PC in split screen. Builds are released with an installer
([releases](https://github.com/dolbaras/jericho-coop/releases), 0.2.7 at the time of writing),
in English, Russian, German, French, Spanish and Italian; from 0.2.3 the installer's shortcut
installs a newer release from GitHub before each start. Every level, power, story part and
cutscene runs in automated tests. A first session between two people over Steam (missions
1-8) found enemies that ignored joining players, walls only they met, scripts that took their
controls and actions the host did not see; 0.2.2 fixes those. 0.2.3 gives each fire team its
own player to follow, syncs every breakable and movable object and keeps a downed player on
their side of a story split. 0.2.4 gives the players their
place in the story (the host is Ross's soul, the second player Jones's astral projection,
players 3-4 the squad members themselves), adds a display mode and refresh rate to the game's
Video settings, renames the mod Jericho Co-op+ (it does more than co-op) and fixes what a full
code review and the release tests found. 0.2.5 completes 0.2.4: the items
0.2.4 left open (a split screen player waiting for a body, the last boss's rune, game folders
with any name), a game start without a mouse and an installer record fix. 0.2.6 fixes the
game's exit, which stopped on a D3D warning box, and lets the installer find and update every
earlier version, the first test build (no installer) included. 0.2.7 keeps the split screen HUD in
shape on wide screens, shows each split screen player their own hints, stops an engine halt
when switching squad members in split screen, caps the frame rate at the screen's (twice it in
split screen), makes Rawlings come for a downed player 3-4 every time, stops a joining PC's
split screen player from falling through the map at a level's start and fixes a rare crash on
a joining PC when the psychic commander disappears. A whole campaign played through by people
is still to come.

## Existing work

- Before this project: no co-op or multiplayer mod found (GitHub, Nexus, ModDB, Steam forums
  via web search, 2026-10-18). Players asking for one:
  [ModDB](https://www.moddb.com/forum/thread/clive-barkers-jericho-multiplayer),
  [Steam](https://steamcommunity.com/app/11420/discussions/0/846963710897727939/),
  [GameFAQs](https://gamefaqs.gamespot.com/boards/934444-clive-barkers-jericho/41226885).
- MAK-PIVO/jericho-coop (MIT, last push 2026-10-21; the repository is no longer public
  since 2026-10-22, its work lives on in Jericho Co-op 0.2.1+), a fork of a pre-0.2.0 build that later merged 0.2.0 back: Steam lobbies,
  shared QTEs, snapshot interpolation, door/object and power sync, a control watchdog, a
  large-address-aware exe flag, boss hits and Rawlings' powers from clients, the melee
  slow-motion fix. These were adapted into 0.2.1 with credit (the LAA flag set by the
  installer on the retail exe only, with the original kept). Not taken: its in-memory
  single-instance patch (the test proxy already handles that through an environment
  variable) and, for now, its redesigned online-only lobby (it drops split screen).
- SMOD, a modding toolkit and rules pack: https://www.nexusmods.com/clivebarkersjericho/mods/2.
  The co-op mod detects it and plays by the host's SMOD rules when every player has it.

## How the game works (for co-op)

- **Scripts:** the gameplay is Lua 5.1. Archives `*.packed` (BFPK) hold `*.lc` files, which
  are a u32 header plus zlib-compressed stock Lua 5.1 bytecode with debug info. All 1533
  files decompile with unluac. Each level has its own `scripts/` (map, triggers, enemies,
  scenes).
- **Command line:** `-Name:value` sets any `game.cfg` variable. `-InitLuaLine:"..."` runs Lua
  after `init.lua` once and is then cleared. The mod re-arms it from its own tick, so it
  runs on every level. `dofile` works with absolute paths to loose files.
- **Engine netcode:** the exe still has Scrapland's UDP client/server and a Lua `Net`
  library, but the join packet expects Scrapland ship data and the server crashes
  (`newpos<=size`, FileSys\FileMem.cpp:46). The mod does not use it. It carries its own
  transport in a `dinput8.dll` proxy and rewrites unused `Net` Lua entries to point at it.
- **Squad:** characters are `WalkChar` entities. `UsrControl` 0 is the local player and -1
  is AI; `oAI.bEnable` toggles the AI. With the AI off, `InCtrl_Walk`/`InCtrl_Str` and
  `Ang` move a body with its own animations, which is how remote players are shown. Actions
  with root motion (`UseDisp`) must not move a remote body, or it drifts from the snapshots.
- **Frame hook:** the `OnRenderDebug` variable names a Lua function called every rendered
  frame. It is independent of game time, unlike `Game.AddScheduledFunc`, so it keeps
  running under slow motion.
- **Cutscenes:** there are 40 in-engine scenes. First-person scenes animate first-person arms
  for one character (`FP_<Char>`) and full bodies for the others (`<Scene><Char>`). Video
  intros are Bink files. QTEs are Lua callbacks of the scene, so they can be routed to one
  player, auto-passed or offered to everyone.
- **Split screen leftovers:** the engine still has Scrapland's split screen code. With a
  second player entity and per-view render calls, each frame draws one player's view in turn
  (each view at half the frame rate). Jones's astral projection holds the game's one set of
  controls, so the other player's body is driven from its own gamepad by the mod meanwhile.
- **Camera units:** `FCam.Ang` is set in radians, but every camera's `camAng` reads in
  degrees. Hide a model with `bObjectVisible`. Entity property names sit as strings in the
  exe next to `TimeSpeed` and `EntityLink`.
- **Single instance:** the game refuses a second copy with `FindWindowA("MercuryGameClass")`
  inside SecuROM code. For tests, the proxy DLL makes that call return NULL when an
  environment variable is set. The exe's code is never patched.
- **Unfocused window:** the game pauses when its window loses focus. Test mode fakes the
  foreground and swallows deactivation. It also stops reading keyboard, mouse and DirectInput
  while unfocused, so the owner's typing never reaches the game.
- **Hiding a body:** while a character's `bCheckFPSVisible` is set, the engine decides its
  visibility itself and ignores `bObjectVisible`: a squad member is drawn, the player's own
  body is hidden in every view (recorded). With `bCheckFPSVisible` off, `bObjectVisible`
  works. Scenes hide characters with `Active = 0` (which also stops them).
- **First-person models:** `0FP_<char>` / `1FP_<char>` follow the camera of the user in their
  hidden `id` field and are drawn in every view; the engine resets their `bObjectVisible` and
  `Active` every frame, but keeps `id`. Split screen hides each player's arms from the other
  half by giving the model an unused user (3) there. Game scripts read `Game.UsrEntity(fp.id)`,
  so that call has to map it back. Flags and ids set in Lua take effect in the frame after
  next, i.e. for the other split screen view.
- **Archives and loose files:** `Pack1.packed` next to `Data0*.packed` is mounted by the
  game and can carry animations and models, but not textures (a level then hangs while
  loading). SMOD moves the archives to `Packed\` and reads loose files; that layout does
  not read Pack1, so built files must also go next to the game's own there. SM3 strings
  are length-prefixed and the chunk size fields are 0, so a texture path in a model can
  change length in place (TrueRoss's rifle textures pointed back at Jones' folder).
- **game.log** lists every texture the engine could not find ("Texture not found"), which
  shows as checkered or flat-coloured surfaces: the mod's test runs compare it against the
  game's own missing ones.
- **Steam:** the game is not on Steam with networking; the mod runs as Valve's sample app
  Spacewar (AppID 480) through a pinned `steam_api.dll` only when a player picks Steam. Steam's
  overlay then answers the game's XInput with empty pads (Spacewar is a Steam Input app); the
  proxy reads pads through a private copy of `xinput1_4.dll` while the overlay is loaded.

Detailed session notes: [NOTES.md](NOTES.md).

## What we changed

All of it lives in the mod repo. On the player's PC the installer only adds files, sets the
large-address-aware flag in the retail exe's header and builds `Pack1.packed` (and, with
SMOD, loose files) from the player's own game files; everything it replaces is backed up and
restored on uninstall.

- **Transport:** ENet on a background thread (LAN discovery, direct IP), or Steam lobbies and
  P2P messages, chosen by the host when creating the game. Lua sees the same `Net.Coop*`
  functions either way.
- **Session:** "Co-op" main menu entry, host/join screens per network, lobby (character,
  nickname, ready, chapter or co-op save, split screen player 2), session settings
  (difficulty, enemy toughness, character-only actions, QTE mode, AI squad damage, cutscene
  view), version check between players, separate co-op saves.
- **Host authority:** the host runs enemies, story triggers, scenes, saves and level changes;
  clients get snapshots (20 Hz, timestamped and interpolated), events and a catch-up when
  they join late. Client hits, uses, revives, powers (telekinesis, runes, fire, ghost bullets,
  Ababanili...), doors and destroyed objects reach the host's world.
- **Story:** character-only actions are offered to the player who owns that character, with an
  AI fallback; story parts written for the AI stand-in were extended so a player can hold them
  (Brandenburg gate, the bunker restoration, Act 1 with Ross). Players beyond the living squad
  become "spirits" that can take a free body.
- **Cutscenes:** everyone watches at once with the original camera; skipped only when every
  player skips. An experimental "own eyes" view plays each scene first-person from the
  player's character, with directed gaze and the scene's character retargeted from its
  first-person animation.
- **Split screen:** a second local player with its own view, HUD and gamepad, in hosted and
  joined games, hot-plug of gamepads, astral projection both ways.
- **Bosses and powers over the network** (adapted from the fork): a client's hit on a boss
  that only takes damage on a weak part carries the part's bone; Rawlings' curse, calm and
  heals act on the machine that controls the target.
- **Robustness:** a client whose controls stay taken outside a scene or story part gets them
  back after 12 s; a client left without a body after a skipped scene gets it back; a player
  cut off by a door or passage the story shut behind the others is offered to rejoin.
- **Story steps per machine (0.2.2):** level scripts written for "the player" run on every
  machine for that machine's own body. Steps that would lock, turn or walk a remote player's
  body are left to that player (a turn and a hint, with the AI as fallback); invisible
  blockers that levels switch off at the end of a talk follow the host.
- **Enemies (0.2.2):** the engine gives enemies targets only among AI-driven squad members;
  the host now offers them the nearest human too. Enemy generators run on the host only.
- **Squad orders per player (0.2.3):** the engine's area manager and the squad's "follow me"
  follow only local user 0. Each fire team (Alpha, Omega) now has a leader: in Act 1 Ross's
  player leads everyone, after that the first player whose character is in a team leads it,
  and a team without one goes to a player who leads nothing yet; AI members follow their
  leader with the game's own chase behaviour, and "hold" pins them. In a story split the AI
  follows the player it is with.
- **Objects (0.2.3):** every prop the map places (whatever its name) breaks for everyone;
  one pushed, knocked over or thrown comes to rest where its mover's machine says, through
  the host. A player joining later gets both.
- **Story split sides (0.2.3):** while a story split is in progress the host tells everyone
  which characters are on which side; a downed player moves only into a free body on their
  own side, else waits for Rawlings' revive.
- **Updates (0.2.3):** the installer's shortcut runs a small PowerShell launcher: it reads
  the latest release from the GitHub API, downloads its zip only from the project's release
  URLs, checks it against the asset's SHA-256 digest, runs the new installer unattended and
  starts the game (through Steam when the copy is a Steam install); offline it just starts.
- **Languages (0.2.2):** the mod's texts and SMOD's menu entries in the game's six
  languages, converted to the game's code page (cp1252, or cp1251 for the Russian release);
  the game's own aspect ratio option extended rather than a new one added.
- **Installer:** a window with its steps, a progress bar (also on the taskbar button) and the
  minutes left, in six languages; finds the game, backs up what it replaces, updates and
  uninstalls. It
  carries Python's embeddable build, so it can build the cutscene animations and a fixed
  Ross model from the player's own game files, and it marks the retail exe large address
  aware (original kept and restored).
- **Bug reports:** F11 saves the last 30 seconds as a Motion-JPEG AVI (frames scaled on the
  GPU, JPEG through Windows' WIC on a worker thread, kept in memory).
- **Player souls (0.2.4):** the host is Ross's soul and the second player Jones's astral
  projection (the game's own lore: Jones projects into others and uses their abilities); both
  switch bodies freely as in the original, the projection is cut while Jones is held (lost,
  possessed, caged). Players 3-4 keep their character; the other bodies are greyed out in
  their squad menu. The menu's fields are write-only from Lua (reading one is an engine error
  that returns nil), so the mod repeats the game's own test for a selectable body.
- **Display settings (0.2.4):** display mode (full screen, window, borderless window) and the
  refresh rate as rows of the game's own Video settings, applied on its Accept and dropped on
  Cancel; a borderless window is a frameless window over the monitor, set through hooks on
  window creation and the D3D9 present parameters.
- **One engine fix (0.2.4):** the last boss's fire spirit (Ababanili, steering at
  `0x4fc9b0`) takes a new prey with no check; with every player near it down or bound by the
  rune (which the single-player game never reaches) it read NULL and the game crashed. A
  vectored exception handler resumes that one read on the function's own "no target" path,
  only when the bytes there match the retail exe. The exe file is not changed.
- **No mouse needed (0.2.5):** the game itself stops with "Mouse not found" when DirectInput
  cannot create the system mouse (`CreateDevice(GUID_SysMouse)` fails on a PC with gamepads
  only). The proxy's DirectInput hook then returns a device of its own that reports a mouse
  that never moves, so a TV split screen setup starts.
- **Game folders with any name (0.2.5):** the engine opens files through the ANSI code page,
  so the mod's folder is passed to Lua as the lossless code page form, else the 8.3 short
  name, else a path relative to the working folder (the shortcut and Steam start the game in
  the game folder; some drives have 8.3 names turned off).
- **A clean exit (0.2.6):** the engine's last `Release` of the D3D9 device shows a modal
  "A D3D object has a non-zero reference count" box when anything still holds the device. The
  proxy hooks the device's `Release` (vtable slot 2) and, once the count is down to the
  engine's own last few references, frees its F11 recorder, capture and split screen surfaces
  first.
- **Split screen HUD on wide screens (0.2.7):** the shape-keeping HUD layout (each user's HUD
  viewport shrunk to the whole screen's shape, its corners moved to the screen's edges, and
  the 2D batcher's last batch drawn in the viewport it was made for) is used in the wide view
  too, where it had been left out: there the HUD was twice as wide as high and each player's
  message line blinked into the other half.
- **Frame cap and the switch flight (0.2.7):** with the game's own cap off and no vertical sync,
  the proxy waits before `Present` to hold the screen's refresh rate (twice it in split screen,
  where each view gets every other frame). The squad switch's soul flight ended in the frame it
  started (co-op does not slow the world) and the game deleted the camera the engine was
  drawing with ("Check failed: GetActiveCamera() != this" in split screen); its end now waits
  0.1 s.
- **Body states across a level change (0.2.7):** states a client sent in the last level reach
  the host after its load, and the host relays every player's state to everyone; the client
  drove its own split screen player's body, a puppet for the first second until the split
  screen takes it, to the last level's place, outside the new map. The host now takes a peer's
  states only after its "level loaded" message, a client none in its first second and no
  relayed copy of its own bodies.
- **Enemy events that call the AI (0.2.7):** clients mirror the host's enemy actions, so the
  animation events run there too; four classes end appear/disappear with a call into their AI
  (`oAI:OnAfterDisappear()`), which with the client's AI off reached a stale pointer (a crash
  once in about ten runs of one story split). On a client those events skip the AI call and
  keep their effects.
- **Updating any earlier version (0.2.6):** installs without an install record (0.1.0 was
  a zip extracted into the game folder) are recognised by the proxy's own marker string and
  their files; the installer then updates or removes them like a recorded install and never
  keeps an older mod's file as the game's original.
- **Development:** a Lua console into the running test game (UDP on 127.0.0.1, test runs
  only), reached on the second PC over ssh.

## QA

Test runners in the mod repo's `tools/` (`run_pair.ps1`, `run_bot.ps1`, `run_menu_pair.ps1`,
`level_smoke.ps1`, `storylines.ps1`, `puppet_jitter.py`) plus the workspace's LAN runners.
Every check is a scripted run with log assertions; test windows are muted and on the second
monitor, two game instances are never run on the same PC with D3D.

Results for 0.2.6 (2026-10-24):

- The release regression as for 0.2.5 (whole campaign 33/33 levels over a LAN, split screen
  with virtual gamepads, a real Steam session, nine story splits, player souls, player 2
  waiting, no mouse), with the same results.
- The exit through the game's own menu call on the installed package: ends in 1 s in the main
  menu, after an F11 clip and in a split screen level (the 0.2.5 DLL stops on the box).
- Every published version installed as players had it (0.1.0 extracted, 0.1.1+ by their own
  installers), then updated to 0.2.6 and removed: one by one and all nine in a row, the game
  folder equal to the clean game afterwards; each also removed directly by the new installer.
- The update from 0.2.5 through its shortcut, from a local feed and from the real GitHub
  release (checksum matched).
- The player souls test showed a downed third player not revived within 12 s in some runs;
  an A/B with the 0.2.5 DLL showed the same (Rawlings' AI on the host does not always come):
  fixed in 0.2.7.

Results for 0.2.7 (2026-10-24):

- Whole campaign by two players over a LAN between two PCs with a split screen player on the
  joining PC: 33/33 levels, no crash, on the final code (version 0.2.7); gamepads, a real
  Steam session, all nine story splits, the player souls test twice, player 2 waiting,
  starting without a mouse.
- The split screen player's fall at Rom_Coloseum (owner, campaign test): the client log showed
  bodies moved 6000-87000 units at 10 of 33 level starts; after the fix the host dropped the
  last level's states at every level start and no body moved across levels in the campaign.
- A client crash in one story split (WWII_PillBoxes, `AccessViolation at 04A0400B`, the
  psychic commander's AI call): after the fix the commander's disappear/appear events were
  forced six times on a joining PC and the last boss split ran, no crash.
- Rawlings for a downed player 3-4: the host has the AI Rawlings heal a body he left waiting;
  every souls run revived (one of the first four by that fallback).
- The squad switch in split screen: the engine stop reproduced within 19-37 switches with the
  released 0.2.6, none in 3 x 60 and the final 2 x 60 after the fix (lab/splitswitch.ps1).
- Frame cap measured: 120 frames a second in split screen on a 60 Hz screen, 60 without it.
- Split screen HUD in the wide view (wider than 1.85): before and after pictures of a level
  in a 1912x1001 window (lab/splithud.ps1), a hint for one player in their half only, a screen
  fade over the whole half; then every split screen test again with the new DLL.

Results for 0.2.5 (2026-10-23):

- Whole campaign by two players over a LAN between two PCs: 33/33 levels to the ending, no
  crash; split screen with virtual gamepads; a real Steam session; all nine story splits and
  the player souls test as for 0.2.4.
- Each fix checked in the game: player 2's gamepad not moving player 1 while player 2 waits
  for a body (before and after); player 2 moving on once Rawlings is held by the last boss's
  rune while a tutorial's switch lock changes nothing; a body left after a wait acting on its
  own again once revived; split screen starting when a body frees up; a start with
  DirectInput's mouse failing (the pad still plays); a game folder with a Greek letter on both
  PCs (short name on one, relative path on the other, whose drive has no 8.3 names); Auto
  refresh back to 75 Hz and a borderless window kept until restart.
- A clean install, removal (exe back to the retail hash), and the update from the published
  0.2.4 through its shortcut, from the real GitHub release. That test found the install
  record's shortcuts written as one joined line since 0.2.3 (removal left them, the next
  update could stop with an error); fixed and read back from old records.
- Re-published the same day: the main menu's co-op label covered the game's "Press [ENTER] or
  START" line, and in a 4:3 window on a 16:9 monitor it was laid out for 16:9. Checked at
  16:10 and 4:3, with and without SMOD, in background test runs (no keys, window captures);
  the update from 0.2.4 again, from the real GitHub release.

Results for 0.2.4 (2026-10-23, final code):

- Whole campaign by two players over a LAN between two PCs: 33/33 levels from Alk_Desert to
  the ending, no crash; split screen with virtual gamepads; a real Steam session between two
  accounts.
- All nine story splits with a real host and client. One crashed the host at the last boss
  (the engine read above); after the fix, the same split and a forced test of that read
  (every Ababanili losing its prey each frame for two minutes) ran without a crash.
- Player souls on two PCs: the roles, a third player's other bodies greyed out and the aim
  switch refused, the second player moving freely, the projection cut and freed, a downed
  third player waiting and revived.
- Display settings: in full screen on the second PC a 60 Hz rate and Auto back to the
  monitor's 75 Hz; in the main and pause menus the restart notice for a mode change, and
  Cancel applying nothing.
- A clean install from the release zip, and the update from the published 0.2.3 through its
  shortcut (checksum matched, shortcut renamed).

Results for 0.2.3 (2026-10-23, final code):

- Whole campaign by two players over a LAN between two PCs: 34 level loads to the ending,
  no crash; nine combinations of graphics options with no engine error or missing texture.
- All nine story splits played with the second PC hosting the character who goes on (the
  story line driven step by step) and this PC's game joined as the one left behind: each
  split's waves, sides and notices started and ended on both, a fallen player never moved to
  the other side. The test found one split that set its sides only from a trigger players can
  bypass.
- Both branches of the Lost Patrol by playthrough bots, the merge and its scene skip included.
- Breaking and moving objects both ways; squad leaders with a host and a joining player.
- The update path: the installer's shortcut from 0.2.2 to the published 0.2.3 (checksum
  matched), a bad checksum (the game starts on the old version), offline (3 s).

Results for 0.2.2 (2026-10-22, final code):

- Whole campaign over a LAN between two PCs (host on a second PC, client here with a split
  screen player 2): 33/33 levels to the ending, 0 Lua errors, 0 engine errors, no crash.
- Split screen with two virtual gamepads (ViGEm): astral projection by either player,
  gamepads plugged in and out, every character's buttons for player 2. This suite had been
  passing while the game crashed after its checks (a melee hit, 4 runs in 6, also in 0.2.1);
  runs are now also checked for their full length, and the crash is fixed (6/6 full runs).
- Steam between two real accounts on two PCs: lobby, join, a level played.
- SMOD, the co-op and Clive Upscaler's textures (with DXVK) on both machines: jumps seen at
  full height, the host's crouch and stand-up on the client, Act 1's Ross; no fault.
- Remote bodies' height against their owner's: 20-42 units off before (in or above the
  floor), 0-2 after (45 samples per body).
- Each fix from the first human session checked between the two PCs, most with virtual
  gamepads and recordings; the new menu choices, hints and the installer window gone through
  like a player and checked on screen (installer in all six languages, install, update,
  removal, closing during an install).

## How to play

Download a release from https://github.com/dolbaras/jericho-coop/releases, run
`Install Co-op.bat`, start the game, choose Co-op. Host: Host game, pick the network (local
network or Steam) and a chapter, Create session. Others: Join (local network) or Join through
Steam. Over the internet without Steam the host forwards UDP 28100 or everyone uses a
virtual LAN. PhysX legacy 9.12.x or 9.13.x is needed on modern Windows
([Steam thread](https://steamcommunity.com/app/11420/discussions/0/846945955298387706/)).

## Known issues

- One human session so far (missions 1-8 over Steam); the full campaign has been run by
  scripts, not finished by people. A report from that session (a cutscene taking a client's
  controls while the host walked) could not be reproduced.
- Steam is tested between two real accounts on one LAN; across networks (Steam relay) and
  Join Game auto-launch (game not running) are not tested yet.
- Split screen draws the two views on alternate frames, so each runs at half the frame rate.
- The "own eyes" cutscene view is experimental: the sets were built for one camera, and it
  falls back to the original camera where a character cannot see the scene.

## Next steps

- Fix what human playthroughs report (real gamepads, Steam between real accounts); open items
  are tracked in the mod repo's issues.
- A full co-op playthrough by people, written up, would move this to `verified-playthrough`.
