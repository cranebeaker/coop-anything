# Clive Barker's Jericho (2007)

| | |
|---|---|
| Engine | MercurySteam ScrapEngine (the Scrapland engine), Lua 5.1.1, D3D9, PhysX 2.7 |
| Versions tested | Retail DVD, `Jericho.exe` 1.0.0.1, SHA-256 `7fb5b759823283a83b685adc4848289a149f520ddbab6dbc7f9e6131b15d2a62`, Windows 10 and 11; also with SMOD 1.0.3 |
| Projects | [Jericho Co-op](https://github.com/dolbaras/jericho-coop) (directory entry "Jericho Co-op"); fork [MAK-PIVO/jericho-coop](https://github.com/MAK-PIVO/jericho-coop), merged back in 0.2.1 |
| QA level | automated (whole campaign over a LAN between two PCs, menus driven like a player with virtual gamepads; human playthroughs just started) |
| Last updated | 2026-10-21 |

## Where things stand

Two to four players can play the whole campaign together, each controlling one squad member
while the AI keeps the rest, over a local network, direct IP or Steam, and two of them can
share one PC in split screen. Test builds are released with an installer
([releases](https://github.com/dolbaras/jericho-coop/releases), 0.2.1 at the time of writing).
Every level, power, story part and cutscene runs in automated tests; real people have only
just started playing it, so expect rough edges.

## Existing work

- Before this project: no co-op or multiplayer mod found (GitHub, Nexus, ModDB, Steam forums
  via web search, 2026-10-18). Players asking for one:
  [ModDB](https://www.moddb.com/forum/thread/clive-barkers-jericho-multiplayer),
  [Steam](https://steamcommunity.com/app/11420/discussions/0/846963710897727939/),
  [GameFAQs](https://gamefaqs.gamespot.com/boards/934444-clive-barkers-jericho/41226885).
- [MAK-PIVO/jericho-coop](https://github.com/MAK-PIVO/jericho-coop) (MIT, last push
  2026-10-21), a fork of a pre-0.2.0 build that later merged 0.2.0 back: Steam lobbies,
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
  back after 12 s; a client left without a body after a skipped scene gets it back.
- **Installer:** finds the game, backs up what it replaces, updates and uninstalls. It
  carries Python's embeddable build, so it can build the cutscene animations and a fixed
  Ross model from the player's own game files, and it marks the retail exe large address
  aware (original kept and restored).
- **Bug reports:** F11 saves the last 30 seconds as a Motion-JPEG AVI (frames scaled on the
  GPU, JPEG through Windows' WIC on a worker thread, kept in memory).
- **Development:** a Lua console into the running test game (UDP on 127.0.0.1, test runs
  only), reached on the second PC over ssh.

## QA

Test runners in the mod repo's `tools/` (`run_pair.ps1`, `run_bot.ps1`, `run_menu_pair.ps1`,
`level_smoke.ps1`, `storylines.ps1`, `puppet_jitter.py`) plus the workspace's LAN runners.
Every check is a scripted run with log assertions; test windows are muted and on the second
monitor, two game instances are never run on the same PC with D3D.

Results for 0.2.1 (2026-10-22, final package):

- Whole campaign over a LAN between two PCs (host on a second PC, client here with a split
  screen player 2): 33/33 levels to the ending, 0 Lua errors, 0 engine errors, no crash.
- Split screen with two virtual gamepads (ViGEm), 9 scripted runs: astral projection by
  either player, gamepads plugged in and out, every character's buttons for player 2:
  9/9, 0 errors.
- Network checks of the 0.2.1 sync work: a running remote player's visible jumps 23/min ->
  0/min (snapshot interpolation); doors and destroyed objects identical on the client
  (0 digest misses); Black's telekinesis, Church's runes, fire, ghost bullets, Ababanili and
  Jones's astral pyrokinesis/telekinesis reach the host; shared QTE passes when one player
  succeeds and fails when both fail; a stuck client control lock is released after 12 s.
- Steam between two real accounts on two PCs: lobby, join, a level played (also as an
  automated test, round trip 6-19 ms); earlier, 6 levels through a local Steam stand-in.
- Split screen visibility: both players turned to face each squad member in turn, recorded
  and checked frame by frame on the retail game and with SMOD (no floating arms, no body
  shown as a weapon alone).
- Every in-engine cutscene (27 starts) recorded next to the unmodded game, with the original
  camera and with "own eyes", and reviewed.
- Act 1's Ross for the other machines, vanilla and SMOD: one textured model (recorded).
- Menus driven like a player with virtual gamepads on a clean install made by the installer.

## How to play

Download a release from https://github.com/dolbaras/jericho-coop/releases, run
`Install Co-op.bat`, start the game, choose Co-op. Host: Host game, pick the network (local
network or Steam) and a chapter, Create session. Others: Join (local network) or Join through
Steam. Over the internet without Steam the host forwards UDP 28100 or everyone uses a
virtual LAN. PhysX legacy 9.12.x or 9.13.x is needed on modern Windows
([Steam thread](https://steamcommunity.com/app/11420/discussions/0/846945955298387706/)).

## Known issues

- Human playthroughs have only just started; the full campaign has been run by scripts, not
  finished by people.
- Steam is tested between two real accounts on one LAN; across networks (Steam relay) and
  Join Game auto-launch (game not running) are not tested yet.
- Split screen draws the two views on alternate frames, so each runs at half the frame rate.
- The "own eyes" cutscene view is experimental: the sets were built for one camera, and it
  falls back to the original camera where a character cannot see the scene.

## Next steps

- Fix what human playthroughs report (real gamepads, Steam between real accounts); open items
  are tracked in the mod repo's issues.
- A full co-op playthrough by people, written up, would move this to `verified-playthrough`.
