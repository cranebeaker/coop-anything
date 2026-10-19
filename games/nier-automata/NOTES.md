# NieR:Automata: working notes

Running notes, newest last. Observations, derived facts, guesses and unknowns are kept apart.

## Game identity

- Store: Steam, app 524220, build id 7020666
- `NieRAutomata.exe` SHA-256: `5171bed09e6fec7b21bf0ea479dbd2e1b228695c67d1f0b478549a9be2f5726a`, PE timestamp 2021-06-23
- Engine: PlatinumGames in-house engine (name: unknown). Native code only; game logic partly in the `hap` script layer
  (class names like `hap::BunkerAction`, `script_export`), story scripts compiled for mruby (source: AutomataMP's `shared/sdk/mruby.*`).
- OS tested: Windows 11

## Existing work

- AutomataMP by praydog: https://github.com/praydog/AutomataMP. Players, animations, Pod and enemies over ENet with a Go server.
- nier-coop, fork of AutomataMP aimed at two-player story co-op: https://github.com/egoriyNovikov/nier-coop.
  Design: `docs/COOP_CONCEPT.md` there. Story sync not done yet.

## 2026-10-18: isolated test copies

Observed:
- The game finds Documents through `SHGetSpecialFolderPathA(CSIDL_PERSONAL)` (only shell import) and
  `%APPDATA%\PlatinumGames\NierAutomata\config.ini` through `getenv("APPDATA")` (string `APPDATA` in .rdata, `getenv` import).
- The first run of each new test copy rewrote `SystemData.dat` in the owner's Documents (one byte, offset 16: 1 -> 0;
  restored from the copy made before the run). Process Monitor showed the writer: **NVIDIA App**, not the game.
  `nvcontainer.exe` starts `NvBackend\ApplicationOntology\OAWrapper.exe -a nier_automata -p <new install dir> -s`, which
  "optimizes" the newly seen install by writing the settings file in the real Documents. Redirects inside the game
  process can't stop another process. Fix: turn off automatic optimization of new games in NVIDIA App, or reuse test folders.
- Steam Auto-Cloud uploads the owner's save folder when a test copy (same app id) exits, so a damaged file also reaches the cloud.
  Restore before the test copy exits. Steam does not run the launch sync for exes started outside Steam.
- With an `NtCreateFile` write guard (refuses writes to the owner's Documents, `%APPDATA%\PlatinumGames` and the store
  install) plus both redirects, 3 runs (title screen, save loaded) ended with 0 blocked writes and the owner's files unchanged.
- Remaining writes outside the test copy: NVIDIA driver caches, D3D shader cache, Steam overlay log.
- Two copies can run at once (the mod renames the `NieR:AutomataEvent` single-instance event).
- `data\*.cpk` can be hardlinked into the test copy; the game only reads them.

## 2026-10-18: test bot

- A virtual XInput pad 0 (hook of `XInputGetState` in `xinput1_4.dll`) is enough to get from the title screen to the loaded
  world: any button, Continue, choose the save, Yes, Start Game. The save menu offers Chapter Select on a finished save.
- In-game screenshots by copying the D3D11 back buffer in the present hook (no desktop capture).

## 2026-10-18: story phase

Observed (build 7020666):
- `.text` on disk is encrypted (SteamStub, `.bind` section). Search references in memory of the running game.
- The current phase is a string like `16_AB_bunker_Order` (Bunker, early route A save) or `60_05_AB_Forest_Castle`
  (finished save at the Resistance Camp). Same name in four globals: exe base + `0x13ecc34`, `0x13f8548`, `0x145b6a8`, `0x1495548`.
- `+0x1495544` is an int32 right before the name (256 on the first save, 512 on the second). The game compares it with `0x100`.
- Signature: `8b 3d ? ? ? ? 81 ff 00 01 00 00 75 ? 48 8d 15 ? ? ? ? 48 8d 0d` (mov edi,[id]; cmp edi,100h; jnz; lea rdx; lea rcx,[name]).

Derived:
- Phase names follow the LiveSplit autosplitter list for v1.01 (`00_85_A_RobotM_Pro_2ndArea`, ...):
  https://github.com/Henny022/NierAutomataSplitter. That autosplitter's addresses do not apply to this build.

Unknown:
- Meaning of the int32 before the name. Which of the four copies the game treats as the source.
- How to switch the phase (what Chapter Select calls).

## 2026-10-18: automated co-op smoke test

- Two test copies on one PC (one Steam account): both load the same save through the bot, one presses Host Game
  (starts the Go server), the other Join Game to 127.0.0.1. Checks: both in session, roles, each sees the other player
  within 3 m of where the other game says it is, same story phase, host plays 2B and client took over 9S. Result: PASS
  (0.12 m and 0.03 m).
- Blind timed menu presses failed twice (a lost press on the title screen shifted the rest: once into License, once
  Exit Game -> Yes). Recognising the menu screen from a screenshot before each press fixed it. Whole-frame comparison
  is not enough (title vs main menu differ by ~5/255 on average); compare the header and the menu centre regions.
- The bot must not run inside the present hook: the game stopped presenting (window not responding) on the new-game
  name entry screen reached by a wrong press, and a present-driven bot went silent.
- After the owner turned off "Automatically optimize newly added games" in NVIDIA App, a first launch from a new folder
  (slot4) still ran `OAWrapper.exe ... -g c` (read) but no `-s` (write); the owner's SystemData.dat stayed unchanged.

## 2026-10-18: story flags (scene states)

Observed (build 7020666):
- Story scripts set and reset named flags through the script functions `SceneStateSystem.set` / `.reset`
  (script function table entries found by CRC32 of "Class.method", AutomataMP `sdk::ScriptFunctions`).
  The argument `hap::SceneStateName const&` points to data with the CRC32 of the name at +0x20 and the name text
  at +0x38 for short names: `q410/REQ`, `hacking_state/last_9S`, `p300_PV_block_off`, `em2100/DIE`, `Unlock_RCtoAP`.
- Scripts call `set` on the same flags every frame (~4000 calls a minute), and a chapter load resets ~1500 flags.
  Log only real changes (remember the last value per CRC).
- Chapter Select (finished save) -> Ch. 02-01 as 2B: phase became `30_AB_Ruined_City_ForcedLanding`, id 256.
  The chapter menu asks 2B or 9S, which is how route A or B is picked for shared chapters.

## 2026-10-18: cutscenes (story events)

Observed (build 7020666):
- Pre-rendered movies are `movie/ev%04x.usm` (format string in .rdata); story events also build sound event names
  `ev%04x_Start` and `ev%04x_End` from the event id. The code is `mov r8d, [rdi+4]; lea rdx, <format>;
  lea rcx, [rsp+20h]; call sprintf`, the same bytes for Start and End: hook both and tell them apart by the
  format string the lea points to.
- Chapter Select 03-03 (Battle with Adam): `Event start ev0250` (Adam's birth, ~57 s, "Hold to Skip"), `Event end
  ev0250`, then `ev0251` starts and ends at once. The movie format hook did not fire for this scene.
- A Cheat Engine style search (snapshots in and out of two cutscenes, writable sections of the exe) left 23
  candidate bytes without a clear winner; hooking the event start/end code was simpler and gives the id.

## 2026-10-18: route and Chapter Select

Observed (build 7020666):
- The route and the route's hero are scene state flags: `Route/A`, `Route/B`, `Route/C` and `PL/2B`, `PL/9S`, `PL/A2`.
  `SceneStateSystem.has` on them gives the route. Chapter Select of a shared chapter as 2B -> `Route/A` + `PL/2B`,
  as 9S -> `Route/B` + `PL/9S`; loading both test saves (route A) -> `Route/A` + `PL/2B`.
- `this` for `SceneStateSystem.has` must be the object the game's own `SceneStateSystem.set` calls get. AutomataMP's
  `SceneStateSystem::get()` (from the `@SceneState` token category) is 8 bytes off on this build and `has` crashed with it.
- Chapter Select confirm handler (exe + `0x95cb30`): reads the segment from a table of 0x54-byte entries (phase id as
  int16 at +0xc, phase name at +0xe, hero or -1 at +0x4), sets the hero and route flags, calls exe + `0x9c5a00(id, phase
  name, n)` which writes the phase global and resets story data, then sets a byte the main loop reads to load the world.
  The arguments are the same for 2B and 9S: the hero choice only goes into the flags.
- Loading a save fills the phase global by copying it from the save data, not through `0x9c5a00`.
- Found with hardware breakpoints set from inside the game (debug registers on every thread through
  `SetThreadContext`, a vectored exception handler records the writing instruction, the call stack unwound with
  `RtlVirtualUnwind` over the exe's `.pdata`, and for execute breakpoints rcx/rdx/r8/r9). No debugger attached;
  SteamStub did not interfere. One run of the bot gives the whole call chain of whoever writes a global.

Derived:
- Chapter Select can only jump to segment starts and only exists on a finished save, so it is not a general way to
  move one player's game to the other's story point.

Unknown:
- How the phase changes during normal play in the world (no bot run reached a natural phase change yet).
- Whether `0x9c5a00` plus the load byte works when called from inside the world.
- Meaning of `n` (0xf for the segment the bot picked, phase `51_AB_MainDesert`) and of the phase id (256, 512).

## 2026-10-18: talking to the Commander (story step inside a phase)

Observed (build 7020666, "Test" save, phase `16_AB_bunker_Order`, objective "Reboot: Speak with the Commander"):
- The bot reaches NPCs by teleporting next to them (entity list position + `Pl0000.setPosRotResetHap`, facing them);
  the Commander is the only entity with model `0x11020`. Holding the gamepad's B next to her starts the conversation.
- The conversation ends with a new objective marker, but the phase name, the scene state flags set through
  `SceneStateSystem.set` and the story event hooks show no change. Story steps inside a phase are kept somewhere else.
- With the window out of focus the game paused the conversation ("PAUSED") and the lines did not advance on their own;
  pressing A advanced them.
- Teleporting to a guessed point (the hangar) dropped 2B out of the Bunker into space: teleport only to known positions.

Unknown:
- Where the current quest and its step are stored.

## 2026-10-18: where the story step is stored; does the client's game follow the host's conversation

Observed (build 7020666, "Test" save, phase `16_AB_bunker_Order`; offsets from the exe base):
- Snapshot diff of the exe's writable sections (bot `snap`/`filter`): two idle snapshots before the talk, one during it,
  two after, kept only bytes that were stable before, changed across the talk and stable after. Two runs, about
  1,600 bytes in both; almost all of it camera and matrix floats, a table of heap pointers and the id of the last
  voiced line (`1010_S0030_G0200_001_a2b`).
- One int32 changed the same way in both runs: `+0x14952c8`, 130 before, 140 after the conversation. It sits in a
  block of int32s that are -1 (0xFFFFFFFF).
- A hardware write breakpoint (bot `watch`) on it caught two writes during the next conversation, by the instruction at
  `+0x9480b6` (`mov [rip+...], eax`, bytes `89 81 98 00 00 00 89 05 ? ? ? ? 89 af 54 06 00 00`). The value comes from a
  pending field of an object (`[obj+0x650]`, with a "pending" flag at `[obj+0x654]` that the code clears); the same value
  is also written to `[[obj+0x130]+0x98]`.
- Code that references the global: the "start the story from a phase" function used by Chapter Select (`+0x9c5a00`)
  writes -1 into it (`+0x9c5c0c`); `+0x8dd2b7` writes -1 too; `+0x921f03` reads it.
- The exe has RTTI for a `QuestManager` with `StaticArray<sQuestState, 30>` and `StaticArray<sQuestMsgInfo, 100>`.
  Instances are in `.data` (found by their vtables with a new bot command `find`), but none of their bytes changed
  during the conversation.
- Two test copies in one co-op session (host and client both on the "Test" save, both at step 130): the host talks to
  the Commander, its step becomes 140 and the objective becomes "Gathering Intel". On the client the step stays 130,
  no conversation starts and the objective does not change. Automated check:
  `tools/qa/coop_smoke.py --scenario commander` in the owner's fork, RESULT: PASS (host 130 -> 140, client 130 -> 130).

Derived:
- The global is the story progress inside the current phase ("step"): a phase start resets it, a story conversation
  moves it.
- A story conversation of the host does not reach the client's game by itself. The mod has to carry its result over.

Guesses (not checked):
- Steps go up in tens, the way PlatinumGames scripts number points of a phase.

Unknown:
- What the object with the pending step is, and which function sets the pending value (the one to call on the client).
- Which quest the step belongs to, and whether setting the step alone gives the client the new objective.
- Whether the step changes in a natural phase change (no bot run reached one yet).

Pitfalls:
- `bot.load` sometimes picks the wrong save in the Load Game list (a lost key press?): in 2 of 9 co-op runs today a copy
  loaded another save (once `60_05_AB_Forest_Castle` instead of the Bunker). The phase checks catch it; reruns passed.
- In a fresh co-op session the first "hold B" next to the Commander sometimes comes before the talk prompt shows;
  the scenario tries twice. Pressing A after the talk ends makes 2B jump, so it stops as soon as the step changes.
