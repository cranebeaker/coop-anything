# Clive Barker's Jericho (2007)

| | |
|---|---|
| Engine | MercurySteam ScrapEngine (the Scrapland engine), Lua 5.1.1, D3D9, PhysX 2.7 |
| Versions tested | Retail DVD, `Jericho.exe` 1.0.0.1, SHA-256 `7fb5b759823283a83b685adc4848289a149f520ddbab6dbc7f9e6131b15d2a62`, Windows 11 |
| Projects | [Jericho Co-op](https://github.com/dolbaras/jericho-coop) (directory entry "Jericho Co-op") |
| QA level | automated (two real game instances on one PC, plus a headless ENet bot; no full playthrough) |
| Last updated | 2026-10-18 |

## Where things stand

Two to four players can start a level together from a "Co-op" main menu entry, each
controlling one squad member while the AI keeps the rest. Enemies, story triggers, scenes
and level changes follow the host, and first-person cutscenes are shown from each player's
own character. It is a prototype: the campaign has not been played through, and QTEs,
Act 1 seats, saves and difficulty scaling are still open.

## Existing work

- No co-op or multiplayer mod found (GitHub, Nexus, ModDB, Steam forums via web search,
  2026-10-18). Players asking for one:
  [ModDB](https://www.moddb.com/forum/thread/clive-barkers-jericho-multiplayer),
  [Steam](https://steamcommunity.com/app/11420/discussions/0/846963710897727939/),
  [GameFAQs](https://gamefaqs.gamespot.com/boards/934444-clive-barkers-jericho/41226885).
- SMOD, a modding toolkit: https://www.nexusmods.com/clivebarkersjericho/mods/2 (not used).

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
  (`newpos<=size`, FileSys\FileMem.cpp:46). The mod does not use it. It carries its own ENet
  transport in a `dinput8.dll` proxy and rewrites 8 unused `Net` Lua entries to point at it.
- **Squad:** characters are `WalkChar` entities. `UsrControl` 0 is the local player and -1
  is AI; `oAI.bEnable` toggles the AI. With the AI off, `InCtrl_Walk`/`InCtrl_Str` and
  `Ang` move a body with its own animations, which is how remote players are shown.
- **Frame hook:** the `OnRenderDebug` variable names a Lua function called every rendered
  frame. It is independent of game time, unlike `Game.AddScheduledFunc`, so it keeps
  running under slow motion.
- **Cutscenes:** there are 40 in-engine scenes. First-person scenes animate first-person arms
  for one character (`FP_<Char>`) and full bodies for the others (`<Scene><Char>`). Video
  intros are Bink files.
- **Camera units:** `FCam.Ang` is set in radians, but every camera's `camAng` reads in
  degrees. Hide a model with `bObjectVisible`. Entity property names sit as strings in the
  exe next to `TimeSpeed` and `EntityLink`.
- **Single instance:** the game refuses a second copy with `FindWindowA("MercuryGameClass")`
  inside SecuROM code. For tests, the proxy DLL makes that call return NULL when an
  environment variable is set. The exe is never modified.
- **Unfocused window:** the game pauses when its window loses focus. Test mode fakes the
  foreground and swallows deactivation. It also stops reading keyboard, mouse and DirectInput
  while unfocused, so the owner's typing never reaches the game.

Detailed session notes: [NOTES.md](NOTES.md).

## What we changed

All of it lives in the mod repo. The game files are not changed.

- **Transport and Lua bridge:** ENet on a background thread, exposed as `Net.CoopHost`,
  `CoopJoin`, `CoopSend`, `CoopRecv` and related functions.
- **Session and players:**
  - "Co-op" main menu entry with host, join by IP, and a lobby (character, nickname, ready,
    chapter).
  - The menu text follows the game language.
  - Player state is sent at 20 Hz.
  - Remote players are shown as AI-off puppets driven through `InCtrl`.
  - Nameplates can be turned off.
- **Host authority:**
  - The host runs enemies and sends snapshots and spawn/death events, with a catch-up for
    late joiners.
  - Client hits are forwarded to the host.
  - On clients, map callbacks, NPC/enemy generators and level saves/loads are stubbed out.
    Story triggers, scenes and level changes come from the host.
- **Squad rules:**
  - A downed player moves to a free squad member.
  - Rawlings' revive works across players.
  - The host decides game over.
  - Cole's and Black's slow motion is shared, and human bodies get compensating
    `TimeSpeed`.
- **Cutscenes:** players who do not own the first-person scene watch from their own actor's
  head, with their own body hidden. They look at the point the original scene camera looks
  at: the hit point of its view ray, smoothed.

## QA

Test runners in the mod repo's `tools/`:

- `run_pair.ps1` runs two real game instances. The client renders on the second GPU, both
  windows open on the secondary monitor, both run unfocused at a 30 fps cap and below-normal
  priority.
- `run_bot.ps1` runs one instance and a headless ENet bot (`tools/bot/coop_bot.py`).
- `run_single.ps1` runs one instance with a dev experiment.
- `record.ps1` and `sheet.ps1` turn in-game frame captures into MP4 videos and contact
  sheets.

Results from 2026-10-18 runs:

- Puppet error 11-24 units on average, with 0-1 snaps per minute, while a bot replays a
  recorded path.
- Enemies driven on the client from host snapshots; hits from the client reach the host.
- A scene trigger on the host plays the scene on the client. The level change from
  rom_palace_02 to the colosseum follows the host.
- Downed, revive and game-over flows checked with a dev experiment.
- Slow motion checked with shared time messages and compensation.
- The cutscene camera was checked frame by frame on video against the original scene camera
  in rom_palace_02 ("TheTrap").

There is no single pass/fail suite yet; each check is a dev experiment plus log assertions.

## How to play

Not released. Build with `native/build.sh` (Zig), install with `tools/install.ps1 -Game
<game dir>`, then start the game normally. The main menu has a "Co-op" entry. PhysX legacy
9.12.x or 9.13.x is needed on modern Windows
([Steam thread](https://steamcommunity.com/app/11420/discussions/0/846945955298387706/)).

## Known issues

- The campaign has not been played through in co-op.
- QTEs, Act 1 (Ross), the separate co-op save and difficulty scaling are not done.
- Enemy AI may still pick targets as if only the host were human.
- Only one cutscene was checked by video.

## Next steps

- QTE routing: only the character's owner sees the QTE; when nobody owns the character,
  the host plays it and clients watch from a side camera.
- Act 1 seats, the co-op save slot, difficulty scaling, lobby portraits.
- A single scripted suite over all levels that prints pass counts.
