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

## 2026-10-19: the story step comes from the phase's action graph; a natural phase change

Observed (build 7020666, "Test" save, phase `16_AB_bunker_Order`; offsets from the exe base):
- The pending step lives in a static object at `+0xf68af0` (its first member is a
  `StaticArray<UIPlHudManager::sUICtrl, ...>`, so most likely the HUD manager): value `[+0x650]`, flag `[+0x654]`.
  The setter is the function at `+0x8fae70(int step)`: under a lock, if the step is not 0 and differs from the pending
  value, it stores it and sets the flag; the HUD update (`+0x947f80`) copies it into the step global the next frame.
  A phase start calls the setter with -1 (`+0x3d59a7`).
- Calling the setter from the mod (`setstep 140` on step 130) changes nothing: the game itself calls the setter every
  frame with 130 (from `+0x3e37f4`, `ecx = [rbx+0xb4]`), so 130 comes back at once.
- `rbx` there is a `hap::HapGoalLocationAction` (RTTI name; read through the vtable with a new bot command `rtti`): an
  action of the phase's script graph that keeps the objective marker on a target (the Commander, an `NPC` object) and
  carries the step for that objective at `+0xb4`. The same function sets the marker target (`+0x8141a0`, a manager at
  `+0x125df80`; the marker position is the float triplet at `+0x1268920`).
- The graph is an array of 24 `hap::Action*` (`DynamicArray<Action*>` at `+0x28` of the graph object): condition blocks,
  `SendCommand`, `MemorySaveAction`, `HapGoalLocationAction`, `AreaAction`, `CameraAction`, `DelayAction` and others.
  Each action has an id at `+0x18` (a 32-bit hash, the same in every run), flags at `+0x1c` (byte `+0x1e`: 0x20 started,
  0xa0 finished, 0 not reached) and a state hash at `+0x20` (`0x26b80d1f` = finished).
- During the conversation: the `ConditionBlock` with id `0x4c4e6dc1` finishes, then the old goal action (`0x883b60bd`,
  step 130) and the next actions; the next goal action (`0x39a20c1d`, step 140, marker on the hangar) and an
  `AreaAction` (`0xa9864814`) start. The state change is written by `+0x3ec550(action, state)` (stores the state hash,
  sets the "finished" flag for `0x26b80d1f`, then notifies by the action id); `+0x356080` wraps it.
- Walking on: the `AreaAction` finishes at the far end of the hangar floor; next to the host's flight unit (model
  `0x10010`) the launch cutscene `ev0200` starts, the step goes 140 -> -1, and after loading the phase is
  `20_AB_buker_Aerial_battle` (spelled so by the game). The phase is applied from the main loop: `+0x3aaa30` calls
  `+0x9c5d20()`, which copies a new story state block (0x399c0 bytes, it holds the phase name and the step) into place.
- Two test copies in one co-op session, the host walks this path and the client stands still: the host's phase changes,
  the client's game stays at `16_AB_bunker_Order`, step 130, no cutscene. The client showed `loading=1` for a few
  seconds at the same moment. Automated check: `tools/qa/coop_smoke.py --scenario hangar` in the owner's fork,
  RESULT: PASS.

Derived:
- The step global is only a mirror of the current goal action. To give the client the host's progress, the mod has to
  move the client's phase graph (finish the same actions, found by their ids), not write the step.
- A natural phase change of the host does not happen in the client's game either.

Guesses (not checked):
- Finishing the `ConditionBlock` `0x4c4e6dc1` on the client with `+0x3ec550` (state `0x26b80d1f`) lets the client's
  graph go on the same way as the host's.
- The client's short `loading=1` is the mod reacting to the host's phase change, not the game.

Unknown:
- What the `ConditionBlock` checks (the talk being over, a flag) and what the second argument of `+0x3ec550` holds
  besides the state hash (`+0x8`, `+0x10`).
- Whether finishing actions from outside is safe for the graph.
- Why the write breakpoint on the phase name caught nothing (threads created after the watch are not armed?).

Pitfalls:
- The Bunker turns: positions of its parts change with time, a saved coordinate can be open space a minute later.
  Teleport next to an entity at its current position (the bot reads it right before `goto`).
- `bot.load` picking the wrong save: once the main menu cursor was on New Game and the bot reached
  "Delete the current save data and create a new file?" (cursor on No); once the save pointer was on "Test" when A
  was pressed and the other save loaded. Most likely cause (not proven): the owner's mouse pointer resting over a
  test window, the game highlights the item under it. Test copies now get no mouse (mouse messages dropped,
  `GetCursorPos` far away), the bot checks the highlighted main menu item and the save pointer before pressing A, and
  backs out of New Game. 9 loads in co-op runs after that were right.
- `find` took over 30 s because it read write-combined GPU memory; skipping `PAGE_NOCACHE`/`PAGE_WRITECOMBINE` made it
  about 2 s.

## 2026-10-19 (evening): moving the client's story graph; the player character asks for the phase change

Observed (build 7020666, "Test" save, phase `16_AB_bunker_Order`; offsets from the exe base):
- The exe is encrypted on disk (Steam DRM); code was read from the memory of a running test copy.
- `+0x356080(action, state hash, param)` is the game's own way to change an action's state. It builds the state
  description `{ +0x0 state hash; +0x8 a ref-counted state object looked up by the hash (+0x3eddd0); +0x10 param }`
  (`+0x3cfeb0`) and passes it to `+0x3ec550`. That function returns false if hash and param are unchanged; otherwise
  it stores the hash at `[action+0x20]`, the object at `+0x28`, the param at `+0x30`, sets `0x800000` in the flags
  (`+0x1c`) for `0x26b80d1f` and notifies listeners with the action id. It runs tens of thousands of times in a few
  minutes, almost always without a change.
- The graph belongs to a script node named after the quest step (`M1010_S0030_GM`). Graph: `+0x18` hash
  (`0x312961f6` in every run), `+0x30` action array, `+0x38` count. Each action points back to its graph at `+0x8`.
- One copy, no conversation: finishing the `ConditionBlock` `0x4c4e6dc1` with `+0x356080` moves the step 130 -> 140,
  the objective "Gathering Intel" appears and 9S says his next line. The graph goes on by itself, as after the talk.
- "Finished" is not one state: most actions end with `0x26b80d1f`, the `AreaAction` after the Commander ended with
  `0x4a063532` (one copy) or `0xcc7564cb` (co-op run), and its finished flag is set outside `+0x3ec550`.
  `+0x3aaaf0` compares the state with `cee3b8c9, 26b80d1f, a09a3421, b649891d, ecbccdf8, ee3e0a30`.
- That `AreaAction` covers the spot where the "Test" save starts (the hangar entrance), not the far end of the
  hangar floor. The launch cutscene at the flight unit starts only after it. Yesterday's scenario walked the floor and
  reached the area by chance; this morning it failed 2 of 2 runs.
- The phase change is asked for by the player character, not by a `SendCommand` of the graph: about 40 s into the
  flight after the cutscene `ev0200`, a state handler of `Pl0000` (`+0x50d6b9`, after a float comparison and a state
  change to `0x6bff8489`) calls `+0x81a440`, which sets `[+0x11ea898] = 1` (the main loop's state). The main loop
  (`+0x83c8f0`) then calls `+0x3aaa30` -> `+0x9c5d20`, which takes the new story block from the buffer at
  `[+0x1421f08]` (it starts with "TY"). Call stack of the request: `+0x50d6be <- +0x503960 <- +0x42c574 <- +0x4a048f
  <- +0x270cae`. `+0x81a440` has three more callers (`+0x47cff1`, `+0x4ab623`, `+0x691315`), not looked at.
- The mod now moves the client's graphs (owner's fork, `StoryActionSync`, packet `ID_STORY_ACTION`): a hook on
  `+0x3ec550` learns the graphs, the host scans them every frame and sends each action whose finished flag appears
  (phase, graph hash, action id, state hash); the client sets the same state once its own graph has started that
  action (0.5 s grace, so commands that finish by themselves still do their work). Automated checks with two test
  copies (`tools/qa/coop_smoke.py` in the owner's fork): `--scenario commander` (the client's step follows to 140) and
  `--scenario hangar` (the client's game plays `ev0200` too and changes to `20_AB_buker_Aerial_battle`) -
  RESULT: PASS; the other three scenarios PASS too. In the hangar run the host sent 47 actions of about 20 graphs
  (the flight scene has its own graphs).
- With the transfer off, the client's loading flag was not written at all while the host changed phase (hardware
  write breakpoint, 0 hits). The short `loading=1` seen this morning did not happen again.

Derived:
- Moving the graph by action ids works without knowing what each action checks: the client's game then plays its
  own lines, the cutscene and the phase change.

Guesses (not checked):
- Intercepting `+0x81a440` in the client's game can hold back the client's own phase changes until the host's.

Unknown:
- What `M1010` / `S0030` mean (quest and step numbers?) and who fills the "TY" buffer with the next phase.
- What the other three callers of `+0x81a440` are.
- What the transfer does to side quest graphs, and when the client joins in the middle of a phase.

Pitfalls:
- A hardware breakpoint on `+0x3ec550` fills the bot's hit buffer at once (it is that hot); watch the caller instead.
- Change graph state only on the game thread (the bot runs `hapset` there through the mod's per-frame hook).

## 2026-10-20: holding the client's phase change; graph names; personal graphs

Observed (build 7020666, offsets from the exe base):
- `+0x81a440` is two stores and a `ret`: `[+0x11ea89c] = 1`, `[+0x11ea8a0] = 0` (the main loop's state). It can be
  found through the call in the `Pl0000` handler: `ba 89 84 ff 6b 48 8d 4c 24 60 e8 ?? ?? ?? ?? 89 74 24 70 48 8d 54 24
  60 48 8b cf e8 ?? ?? ?? ?? 90 48 8d 4c 24 68 e8 ?? ?? ?? ?? 90 e8` (the last `e8` is the call, return address
  `+0x50d6be`).
- Its other callers: `+0x691314` comes right after the game plays `BGM_Player_Die` (string next to `BGM_Sleep_In`) in a
  state machine at `+0x690d20`; `+0x47cff0` is in `+0x47a9c0`, a state function called by 12 character handlers, on the
  branch where the state is `0xc5` and `[+0x10668] <= 0`; `+0x4ab622` is in `+0x4aaf10` (called from a `Pl0000`
  handler), which checks the hero's model (`0x10200`, `0x10203`).
- Setting the hero's health to 0 does not start the death sequence, and falling below the Bunker never ends, so the
  death caller was not seen running.
- In the owner's fork a hook on `+0x81a440` holds the client's request from `+0x50d6be` while in a session and lets it
  through when the host's phase changes. Hangar run with two test copies: the client's game asked for the next phase
  4.9 s before the host's, the request was held and let through 0.04 s after the host's new phase arrived, both games
  loaded `20_AB_buker_Aerial_battle` (`coop_smoke.py --scenario hangar`, RESULT: PASS).
- Graphs have names: graph `[+0x0]` -> description, description `[+0x28]` -> zero-terminated Shift-JIS name. In the
  Bunker: `M1010_S0030_GM` (the quest step graph), イベント再生 (event playback), フェーズイベント再生 (phase event
  playback), 敵セット (enemy set), メール (mail), 武器屋/道具屋 (weapon/item shop), アイテム (item) and others.
- In the open world (finished save, `60_05_AB_Forest_Castle`) about 100 graphs are live, most of them world objects:
  宝箱 / 共通宝箱 (chests), 採取ポイント (gathering points), 釣りエリア (fishing spots), 自販機 (vending machines),
  ロボショップ (robot shop), 実績 (achievements), ファストトラベル (fast travel), ベッド (bed), and 依頼者_q210 (the
  giver of side quest q210). Graph `+0x1c` is 0 for most, `0xffffffff` for 共有スクリプト (shared script), 3, 0xa and
  0x3e8...0x3ec for a few (meaning unknown).
- The first pointer of a description is not a parent: following it gives names like `q150/RUN_13`, `q340_1_1/REQ`,
  `DLC_plato/3` (they look like scene state names).
- The host now keeps graphs whose name has one of those words or `q` + 3 digits to itself; in the open world run it
  sent 49 actions and kept 14 (shops, vending machines, a gathering point, an achievement); `--scenario world` PASS.
- A client that joins after the host talked to the Commander gets every action finished in the phase so far and its
  step goes 130 -> 140 without the talk (`--scenario midjoin`, PASS).

Derived:
- Holding the request function is enough to keep the client's natural phase change in step with the host's for
  this transition; the request is a flag, so letting it through later from the mod's per-frame hook works.
- The story is not only the `M####` graph: the launch cutscene and the phase change come from event and enemy set
  graphs, so filtering by "main quest graph name" would break the flight. Filtering out known personal words keeps it.

Guesses (not checked):
- `+0x47cff0` is the death of a character in another form (flight unit, hacking), `[+0x10668]` its health.
- Side quest graphs are named after the quest (`q###`) throughout, as the giver is.

Unknown:
- Which object owns a graph (the quest or the layout file), so that story graphs can be told apart without a word list.
- Whether the story changes phase anywhere without `+0x81a440` (a `SendCommand` of a graph).

Pitfalls:
- The Commander's fixed coordinate failed one run (the bot talked to a YoRHa soldier next to her): teleport next to
  the entity with model `0x11020` at its current position instead.
- While a third test copy was running, the second one quit at start in both tries ("AutomataMP shutting down" right
  after hooking DX12); with two copies it started. Cause not checked.
- Graph names in logs are Shift-JIS: read logs as cp932, and a Windows console in cp1251 can't print them.

## 2026-10-21: the Bunker's ring

Observed (build 7020666, "Test" save, phase `16_AB_bunker_Order`, two test copies in one session):
- The Bunker is a ring around the x axis (center y 188.55, z -0.68). While the hero walks in a corridor, the ring's
  controller (`+0x83bb60`) turns the whole ring under the hero and the hero stays at the bottom. Every room, NPC and the
  hangar's flight units turn with it. Teleporting the hero does not turn the ring.
- The ring object is the behavior of the entity with model `0xf1200` (class `Bunker`). Its matrix rows are
  (0, cos a, -sin a), (0, sin a, cos a), (1, 0, 0); the euler angle a is also at `+0x94`.
- In the client's game the hero is the host's puppet, so the puppet turned the client's ring. The same world
  coordinates were different places of the Bunker in the two games: in the client's game the Commander rode around
  the ring while in the host's she stood still.
- Moving the puppet more than 5 m by writing its position only: the physics did not take it, the puppet ran toward
  the target for about 10 s and ended 2-5 m below the floor. The game's `Pl0000.setPosRotResetHap` moves it at once.
- The controller stops while `[ctrl+0xe8]` is set. In the egoriyNovikov/nier-coop fork the client's controller only remembers the
  hero's position, the client sets its ring to the host's angle (sent over the network), and positions go over the
  network in the ring's frame. `coop_smoke.py --scenario commander`: ring angles equal, the host's puppet 0.04 m from
  the host (PASS).

Derived:
- Any shared position in the Bunker has to be relative to the ring, and only one game may drive the ring.

Pitfalls:
- A "2B clone" seen on screenshots of the client was a YoRHa android with a quest marker, not a second puppet.

## 2026-10-22: flying from the Bunker together

Observed (phase `20_AB_buker_Aerial_battle`, two test copies; the host flew off from the hangar, the client followed):
- Each game flies two flight units (`BehaviorFlightUnit`, model `0x10010`): the hero's, which the player steers, and
  the story buddy's, which the game's AI flies in formation. The client's game gives the hero's unit to its own hero.
- Both games fly in the same coordinates (near the origin), so the other player's position can be used as it is.
- With the other player's puppet put in the buddy's place, the flight never turned into the battle seen from above.
  Alone, each block of the flight starts with the camera 20 m away and moves it to about 46 m after about 12 s; in the
  session it stayed at 13-16 m, and the buddy's unit stayed a mech and fired down. Without taking over the buddy the
  flight was as alone.
- The AI pulls the buddy's unit back to its formation spot by about 0.3 m per frame: smoothing the unit toward the
  other player left it 2 m off. Setting its position every frame keeps it 0.01 m (median) off.
- A player who does not steer is shot down; the game then repeats the block from a checkpoint about every 50 s
  (alone too).

Derived:
- The flight script waits for the buddy's AI. In a flight the buddy stays the game's, and only its unit follows the
  other player (egoriyNovikov/nier-coop fork, `coop_smoke.py --scenario hangar` asserts both the unit position and the camera going
  past 35 m, PASS).

Unknown:
- How to carry the other unit's rotation and banking, the mech transform and the shots over (not synced yet).

## 2026-10-22: the next block can have the same phase name

Observed:
- During the flight from the Bunker the game asks for the next story block (`+0x81a440`) again and again, and every
  block it loads is called `20_AB_buker_Aerial_battle`.
- The egoriyNovikov/nier-coop fork held the client's request until the host's phase name changed (2026-10-20). Here it never changed:
  the client's game waited in the dark for 1.5 min.

Derived:
- "The host loaded the next block" is a counter, not a name change. The host now sends how many blocks it has loaded,
  and the client's held request goes through when that number grows, also when the host's load came a few seconds
  before the client's request. `--scenario hangar` asserts that no request stays held.

## 2026-10-22: talking to an NPC; the client's story talks

Observed (build 7020666, offsets from the exe base):
- In the client's game the client's 9S could hold B next to the Commander and have the whole conversation; the client's
  story step went 130 -> 140 while the host stayed at 130 (the action graphs then differ).
- The player's interaction code calls `+0x7cfbb0` every frame with the target (and `+0x7cfdb0`, a variant with a
  hold-to-fill meter); when the button is pressed it calls virtual slot 125 (`[vtable+0x3e8]`) of the target's
  behavior. The `BehaviorAppBase` one (`+0x41e860`, signature `40 53 48 83 ec 40 48 c7 44 24 20 fe ff ff ff 48 8b d9
  ba fb 0e cb 28`) posts the signal `ON_TALK` (hash `0x28cb0efb`) to the object; `NPC` (`+0x6ff040`) checks one state
  and jumps to it. Exec breakpoints: slot 125 runs once per press, the poll runs every frame.
- The talk-disable flag that the script functions `BehaviorAppBase.setTalkDisable` / `setTalkEnable` write is
  `[behavior+0xbcc]`; the poll reads it.
- The story scripts are mruby code with a class `EntityPuid` (methods `talk?`, `signal?`, `startTalk`, `endTalk`,
  `setTalkMark`, ... registered at `+0x2fec90`). `talk?` (`+0x337640`) checks for `ON_TALK` and marks it handled.
  `startTalk` (`+0x336b30`) sets the conversation partner in `hap::TalkMan` (a static object at `+0xfbf630`: `[+0x668]`
  talking, `[+0x66c]` partner handle). In the runs watched, exec breakpoints on `talk?` and `signal?` did not fire for
  the Commander's talk; a write watch on `TalkMan` caught `startTalk` called from a script fiber resumed from a
  `hap::SceneStateObj`.
- `hap::TalkMan` keeps the handles of NPCs with a talk mark at `[+0x688]` (array), `[+0x690]` (count). It was empty in
  the Bunker at step 130 and in the open world on the finished save.
- `[behavior+0x66c]` is the id of the entity layout that placed the object (`0x6aae9bcb` for the Commander). The
  objective "Speak with the Commander" is a `hap::HapGoalLocationAction` of `M1010_S0030_GM`, and its `[+0xc0]` holds
  that same id; an objective that marks a place holds `0xffffffff`.
- In the egoriyNovikov/nier-coop fork, in the client's game, slot 125 is skipped when a started, not finished goal action of a
  non-personal graph points at the target's layout id; the screen shows a hint that the host does it. New scenario
  `coop_smoke.py --scenario client_talk`: the client holds B at the Commander, nothing starts, its step stays 130; then
  the host talks and the client's step follows to 140 (PASS).

Derived:
- "Story NPC" can be told from the objective that points at it, without a list of NPCs. Merchants and chatter have no
  objective pointing at them and stay usable.

Unknown:
- A side quest's giver before the quest is taken: no objective points at it yet. Not seen, no save with an open
  quest giver in the test copies.
- Where the dialog choice is made (a talk that the gate lets through can still have choices).

## 2026-10-22: phase changes outside the request

Observed:
- Chapter Select starts the story through `+0x9c5a00` (called from `+0x95cf46`), not through `+0x81a440` or
  `+0x9c5d20` (exec breakpoints on all three during a Chapter Select from the title screen).
- The script function `SaveState.loadFromMemory` is a jump to `+0x9c5d20` (`+0x38e9a0`), the function the main loop
  applies the next story block with (`bool()`, it checks for the `TY` block at `[+0x1421f08]`). A story script can
  change the phase with it without the request.
- The other code that writes the main loop's state (`[+0x11ea89c]`) only resets it to 0 or is the main loop itself.
- Setting the hero's health (`[behavior+0x858]`, what `getHp` returns) to 0 does not start the death sequence in the
  Bunker or in the open world either: the bar stays full. Death needs damage.

Derived:
- Besides `+0x81a440`, a story script's `SaveState.loadFromMemory` is the one known way to change the phase in the
  world. The egoriyNovikov/nier-coop fork now logs its calls (swapped in the script function table, the code stays as it is).

Unknown:
- Which scripts call `SaveState.loadFromMemory`, and whether `+0x691314` really is the hero's death (needs a death in
  a fight).

## 2026-10-23: a death in co-op, the real health, quest givers, Flight Unit angles

Observed:
- In a session in the desert (`51_AB_MainDesert`, two test copies on one PC): 9S of the client died to
  an enemy, and the host's game went to the death screen too, though 2B of the host was far away and alive. Both
  games asked for the next phase from the same call, return address `+0x4ab627`; the host's 24 ms first.
- The real health of a character is `[behavior+0x10668]`. The game copies it into `+0x858` every frame
  (`+0x428b96`), which is why writing `+0x858` never sticks.
- An NPC that gives a side quest before the quest is taken is placed by an action of a story graph
  (`EntityLayoutArea` / `EntityLayoutAction`). The layout entry (lookup by id: `+0x3d8240`) points at
  `EntityLayoutsMixin` at `[+0x38]`, which sits in the action at `+0x50`. Checked on 23 NPCs of the Resistance
  Camp: all come from graphs named `オブジェ配置` ("object placement"); the giver of q210 has its own graph,
  `依頼者_q210` ("requester_q210").
- A `BehaviorFlightUnit` in flight keeps its euler angles (`+0x90`) and its quaternion (`+0x60`) at zero while it
  turns. Its orientation is stored somewhere else (a candidate is the model's root bone).

Derived:
- The client's death reached the host's game through the puppet (see 2026-10-29).
- A side quest's giver can be found by the name of the graph that placed it (`qNNN`). There is no need for a list
  of NPCs.

## 2026-10-29: the hero's death; shared death in co-op

Observed:
- The state of a character is `[Pl0000+0x670]`. The Pl0000 update switches on it (`+0x503968`, 0xd4 states).
  `start_animation` (vfunc `+0x90`) sets it, so the "animations" AutomataMP sends are these states.
- `0x33`, `0x34` and `0x35` are the ways to die. Each goes on to `0x36` (`+0x4aaf10`). `0x36` builds the Body
  Report and, if vfunc `+0x350` returns true (player controlled), asks for the next phase at `+0x4ab622`: that is
  the Game Over. The check of the 9S models (`0x10200`/`0x10203`) there only picks the report's text.
- The game kills a character itself like this (`+0x47bf1a`, `+0x42a5a4`): `+0x10668` and `+0x858` = 0, then
  `start_animation(0x35, 0, 0, 0 or 1)`. Doing the same from the mod gives the full death: the fall, the Body
  Report, Continue and the reload. In the Bunker the Game Over loads at once, without the Body Report.
- `[Pl0000+0x10364]` (float) is an invulnerability timer in frames. `setEventMuteki` (`+0x4eca50`) sets it to 300,
  `changePlayer` to 60. The characters' damage handler (`+0x47a9c0`) compares it.

Derived:
- The 2026-10-23 double death: the client's `Die` state (`0x35`) went over as an animation, and the host's puppet
  (marked player controlled for its input) died with it and called the host's Game Over.
- The egoriyNovikov/nier-coop fork now sends a death as its own packet. The puppet doesn't get the death states and keeps the
  invulnerability timer up. The other game kills its own hero the game's way, and the client's Game Over waits for
  the host's. The bot checks this in two copies (`coop_smoke.py --scenario death|death_world|death_host`).

Unknown:
- How enemy hits on a puppet with the timer up look (no damage, but a hit reaction?). Needs a fight.
- `+0x691314`, the other caller of the phase request near `BGM_Player_Die`, is still not seen.
