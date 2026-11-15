# South Park: The Stick of Truth — investigation notes

## 2026-11-04: initial evidence

QA: no multiplayer implementation verified; no in-game test in this session.

- Read repository AGENTS.md and Co-op Anything MCP skills coop-triage, coop-research and coop-build-from-zero; read reverse-engineering playbook through MCP.
- Repository update: `git pull --ff-only` returned Already up to date.
- MCP directory search for South Park returned zero entries. This does not establish that no mod exists elsewhere.
- Exact private build identity and paths are in ignored local/identity.md.
- Existing work contains a test install, extracted data and exploratory analysis, not an identified working co-op implementation. Preserve it.
- New read-only AMX definition-table inspector parsed all 316 extracted scripts, finding 314 distinct native imports with zero parse errors. Its eight synthetic tests pass. These are parser tests, not co-op QA.
- Parsed AMX format observed: magic 0xF1E1, file/VM version 10, definition stride 12. Inspector deliberately rejects unsupported variants; it does not execute bytecode.
- Script library companion.amx imports get_party_companion, set_party_companion and remove_party_companion and exports named companion selection functions. This provides an investigation target, NOT evidence of controllable second players.
- player_get_by_index is imported by only one observed script, demo/demo_cutscene.amx. No native import name containing multiplayer was found in the 314-name set. Neither fact proves a feature is absent or usable.
- Library names Onyx and DS3 occur in AMX definition tables. The test engine.ini also says project_name = Onyx. Treat this as engine-family evidence, not proof of inherited network functionality.
- Old test_startlevel_full.ps1 kills all processes matching South and manipulates foreground input. Do not reuse it unchanged; it was inspected but not run.
- User approved Steam-backed test launches after save isolation and approved official Frida download. User manually disabled Steam Cloud for this title. Steam config was then observed to have the global `cloudenabled` value `0`; the local app-specific setting's absence alone was not treated as proof.
- Frida 17.25.2 was downloaded from PyPI only after approval, pinned to SHA-256 `0b4417264041e227364df268c77b8d4d8a9920beffd5ac55cec2a06d74e0839e`, and installed without dependencies under ignored `local/runtime-libs`. Its local-device import smoke test passed.
- Built a test-only Frida file guard under `local/`. It redirects only explicitly configured profile/runtime paths into ignored test run folders and denies non-allowlisted writes. Synthetic tests passed: 8 x64 checks and 7 x86 checks, covering redirected read/write/delete, outside-write and rename denial, source preservation and no hook errors.
- In game startup probes, all original profile file hashes and test-copy exe hash stayed unchanged. A first guard version blocked the Steam child launch; after setting the legitimate Steam app environment, accepting existing named pipes and redirecting Steam log/NVIDIA runtime paths, the test game remained alive for 25 seconds with its profile folder and every observed save file redirected to its test profile. The game frame captured at that time was black with an FPS counter, so a playable menu or save load is not verified.
- A later 55-second probe with read-only native-call tracing failed *before game resume* because Frida could not attach to a short native function. The tracer was changed to record individual unavailable hooks rather than aborting startup; its subsequent run status must be read from the run report before relying on it.
- Static inspection of `companion.amx` records 16 companion-related public entry points and 16 native imports. Its AMX header has magic `0xf1e1` (64-bit-cell Pawn family) and flags `0x14`: NOCHECKS plus CRYPT. Therefore the archived bytecode is encrypted; entry offsets and names can be mapped, but its code must not be treated as readable source. This also explains why a direct on-disk bytecode decompile is not yet available.
- Bounded bot experiments sent either Enter or the previously recorded `Esc -> Space -> Enter -> Enter` sequence to the verified test window. The keystrokes were accepted, but the later PrintWindow frame remained black and there were no traced companion-native calls. No claim is made that a save loaded; both results are negative/inconclusive.
- Menu-only guarded memory scan searched 499,601,040 readable bytes for exact 32-byte headers of `companion.amx` and `demo_cutscene.amx`; zero matches. At that screen these scripts are either not loaded or have been transformed. This does not establish that the scripts or companion system are absent.
- The `.text` bytes at all five candidate handler RVAs are high-entropy and do not form valid static x86 functions; only the live process briefly exposed meaningful instructions. The function pointers therefore sit in a protected/encrypted code region. No protection was removed or modified; native reasoning must be based on runtime evidence only.
- Previous game-test stdout/stderr files mostly empty. Nonempty launch output shows archive mounting and Steam initialization, but not successful multiplayer.

### Searched (public read-only)
- https://api.github.com/search/issues?q=%22stick+of+truth%22+repo%3Acranebeaker%2Fcoop-anything : zero results.
- https://api.github.com/search/repositories?q=%22stick+of+truth%22+coop : zero results.
- https://api.github.com/search/repositories?q=south+park+stick+truth : 12 results, including an archive extractor, dialogue browser and trainers; no co-op project identified in returned descriptions. No trainer binaries downloaded or executed.
- https://github.com/ohegba/ObsidianArchiveExtractor : potential archive-format reference, not yet audited.
- https://github.com/QwePek/SouthParkTheStickOfTruth-DialogueBrowser : potential data-format reference, not yet audited.

### Next gate
A guarded process remained live 25 seconds with save paths redirected, but the observed frame was still black and no playable state has been verified. Confirm a legitimate Steam-backed path to the menu/load screen without touching the original profile, then observe the five mapped native candidates during a known save load. Do not invoke native handlers until argument conventions are traced. The later 55-second tracer probe must be checked for startup status before it is interpreted.

Private API reports and local scripts remain ignored under local/. No publishing, account access, game launch or software installation in this session.

## 2026-11-15: review of the 2026-11-04 session, and the gate it missed

QA: co-op is still not implemented and not verified. What ran: 4 menu-only guarded
launches of the isolated copy (~50 s each) with the save guard plus a new read-only
Winsock/Steamworks probe. Original profile hashes were identical before and after every
run, `guard_integrity` was `true`, and the test-copy exe hash still matched.

### Verdict on the previous session

The method was sound and nothing was thrown away: the file guard, its synthetic tests,
the read-only AMX report and the guarded launcher are all reused below. Three things were
wrong or overstated, and one was expensive.

1. **`startup_verified` was a hardcoded `false`.** `local/probe_game_startup.py` line 94
   sets `result['startup_verified'] = False` unconditionally, so the field never measured
   anything. That is why the README says the harness "reached the title menu" while the
   notes say a playable menu was "not verified". The evidence resolves it in favour of the
   menu: the private-workspace frame `screens/console_tilde.png` is a readable 1280x720 main menu
   (Continue / New Game / Load / DLC / Settings / Quit), and the runs stayed alive for the
   full 25-55 s. *(Correction: this is a menu, not gameplay. There is no save in the
   profile, so no save load was ever possible with `-startsavegame`.)*
2. **`runtime_name_pointer_layout_verified` over-claims.** What the probe actually checks is
   that the record's name field reads back the expected string and that the handler field
   equals the static candidate address. The run reports show *identical* 12-instruction
   disassembly for `set_party_companion` (rva `0x4a88d0`) and `player_get_by_index`
   (rva `0x4a69b0`), and `object_get_by_tag` fails to disassemble at all. The `.text` region
   is Steam-CEG protected, so the bytes read there are not a reliable statement about what
   the function does. Keep the record layout, drop the behavioural claim.
3. **The chosen next gate was the wrong gate.** The 2026-11-04 notes made "instrument the
   encrypted AMX loader to trace companion natives" the gate. The AMX API index has **no**
   `session`, `server` or network native at all (`find_api session` = 0, `find_api server` = 0;
   the only `net` hits are the in-game social-network UI). Scripts therefore cannot start or
   join a session, and the companion library is not on the path to a second player. The
   network layer is native engine code, and that is where the cheap answer was hiding.

### New evidence: Onyx's own co-op netcode is compiled into this exe

File offsets are from the retail test-copy executable (`South Park - The Stick of Truth.exe`,
11,468,656 bytes, SHA-256 `54a83d05…56a0d6`); all found as plain ASCII in the image:

| String | Offset | Meaning |
|---|---|---|
| `steam_online_session_manager.cpp` | 7976716 | Onyx Steam session manager is linked |
| `steam_online_p2p_communicator.cpp`, `steam p2p hub`, `steam p2p channel` | 7976xxx / 7812936 | Steam P2P transport is linked |
| `steam_online_voice_manager.cpp`, `network_chat_manager.cpp` | 7976316 / — | voice and chat managers are linked |
| `lan_online_session_manager.cpp`, `lan session manager` | 7975800 | LAN session manager is linked |
| `.?AV?$Host_Object_Controller@…`, `.?AV?$Ghost_Object_Controller@UPlayer_Object_Net_Data…` | RTTI, `.data` | Onyx host/ghost replication template instances whose arguments name Player, Creature, Item, Door, Level_State (no bare class of those names exists in RTTI; checked 2026-11-15) |
| `usage: connect IP<:port>`, `Connects to a server at the given address` | 8341252 / 8341780 | console command `connect` |
| `restart_server`, `server`, `status`, `kick <player_index>`, `warp_to_player`, `disconnect` | 8341xxx | the rest of the multiplayer console command set, with help text |
| `LOCAL_WITH_NETWORK`, `localWithNetwork` | 8413656 | a local game can run through the network stack |
| `chat_entry.gfx`, `GUI_command_parser_chat_entry`, `SEND_CHAT` | 8145124 | a chat UI resource and its GUI parser are still in the build |
| cvars `multiplayer_local_enabled`, `multiplayer_remote_enabled`, `multiplayer_tether_range`, `multiplayer_tether_screen`, `multiplayer_stuck_warp_delay`, `Multiplayer camera ownership`, `inv_show_remote_players` | `.rdata` | inherited Onyx/Dungeon Siege III co-op tuning |

This is engine-level inheritance, not a feature: the strings prove the code was linked, not
that any of it is reachable. It does change the plan, because reusing Onyx replication
would be far cheaper than writing netcode from scratch.

### What the runtime probe showed (4 runs, menu state only)

- Winsock is initialised: 2x `WSAStartup`, 3x `socket`, 2x `closesocket`.
- **No** `bind`, `listen`, `accept` or `connect` happened: nothing is listening and nothing dials out.
- `SteamAPI_Init` runs once; `SteamNetworking()` is called **~886 times in 50 s** and the fuzzy
  backtrace places the caller inside `South Park - The Stick of Truth.exe`, i.e. the engine's
  Steam p2p communicator keeps polling. `SteamMatchmaking`, `SteamGameServer_Init` and
  `SteamFriends` were never called: no lobby, no game server, no session.
- Console-script autoload is **not** demonstrated. The engine opens the directory
  `data\global\console_scripts` once (`NtOpenFile`) but never opens any `.txt` inside it,
  including the four candidate names written for the probe; `-exec probe_console` produced
  no `write_file` dump and no error. `write_file <name>` (documented in the exe as "saves all
  non-default var values") is therefore the wrong oracle until execution is proven another way.
- `data\global` is empty in the retail install and `data\movies` ships loose, so the `data/`
  directory layer is read and is the natural modding seam to test next.
- Test-copy leftovers from these runs: `global\console_scripts\{platform_win,init,console,autoexec,probe_console}.txt`,
  the same set under `data\global\console_scripts\`, and `probe_console.txt` in the game root.
  They are inert and stay only in the test copy.
- Useful command-line flags found in the image: `-connect`, `-exec`, `-load`, `-journal`,
  `-record`, `-multiple_instance` (the build otherwise refuses a second instance),
  `-startlevel`, `-startsavegame`, `-godmode`, `-noplayer`, `-enumerate`, `-forcepack`,
  `-disablecaching`, `-no_timeout`.
- `-connect 127.0.0.1:2005` as a **command-line** parameter produced no `ws2_32!connect` and no
  error (run `879854ab…`, 50 s, guard clean, profile unchanged). So `-connect` is not a working
  launch parameter in this build, or it needs another form: the client path is still unproven and
  the string is probably a leftover from Dungeon Siege III.

### Still unknown

- Whether the session/host path can be reached at all in this build, and what gates it.
- Whether the in-game console can be opened by a key or by `console_toggle`, and whether the
  multiplayer commands are registered in the command table.
- Which actor class the game uses for the New Kid at runtime, and whether a second
  `Player_Object` can exist (replication templates name `Secondary_Player_Object` and
  `Remote_Camera_Object`; no bare class of those names is in RTTI, so this is open).
- Turn-based combat: whether a second controller can share a fight, and what happens to the
  companion slot the game already fills.

## 2026-11-15 (second run): two instances, and the command-line client path is dead

QA: still no co-op. Ran one guarded **two-instance** session (`local/guard_multi.py`, 45 s, instances
A and B alive at the same time, each with its own redirected profile, launch lock held). Original
profile hashes unchanged, exe hash matched, no guard errors.

- `-multiple_instance` works: two copies of the game ran side by side on one PC. That is the local
  test bed for everything up to Gate 5, and it is the only way to test a client without a second PC.
- **Neither instance called `ws2_32!connect`, and neither called `bind`/`listen`.** Instance B was
  started with `-connect 127.0.0.1:2005`; instance A ran plain. Both only did `WSAStartup`, 3
  sockets, 2 closes and the usual `SteamNetworking` polling (742 and 1286 calls in 45 s).
- So `-connect` is **not** a working launch parameter in this build (it is a leftover string, same
  family as the `-journal`/`-record` flags), and the earlier single-instance `-connect` run said the
  same. The client path now has to come from the console command `connect`, or from calling the
  session manager directly — both are Gate 1 work, not Gate 0 evidence.
- What remains for Gate 0: (a) does a host path exist at all (session manager reachable, or
  `restart_server`), (b) can the console `connect` be executed. Command-line and file-based
  autoexec routes are closed.

## 2026-11-15 (third run): scheduling

Steps 2 (gameplay harness) and 3 (console) were started as parallel agents under `PARALLEL.md`:
own worktree and branch, own notes file, machine-wide launch lock. The integrator (me) keeps
`NOTES.md`, `README.md`, `PLAN.md`, `data/` and merges their notes when they report.

## 2026-11-15 (step 4, static): RTTI index merged

Source: `notes/04-object-model.md` (branch `game/south-park-the-stick-of-truth-step04-objects`, commit
`e560f6f`), merged by the integrator. Static only; no game launched, no lock used.

- 937 classes recovered from the test copy (hash checked); 878 polymorphic, backed by 929 verified
  MSVC COLs and vtables. Tool: `tools/rtti_index.py`, output `tools/rtti_index.json`.
- **Correction (verified by the integrator with a raw byte search):** `Player_Object`,
  `Secondary_Player_Object` and `Remote_Camera_Object` have **no bare RTTI class**. They occur only
  as template arguments of `Host_Object_Controller<…>` / `Ghost_Object_Controller<…>` (Net-data
  types). The same holds for `Creature`, `Companion` (which has AI-state classes such as
  `Companion_Basic`, `Companion_Follow`, `Companion_Command`), `Actor` and `Level_State` (which has
  `Component_Level_State`). `NetConnection` has no RTTI name at all.
- Component classes that matter for the object model: `Component_Player` `0x7af064`,
  `Component_RPG_Player` `0x7a1c0c`, `Component_Party_Member` `0x7b89ec`, `Component_Remote_Camera`
  `0x7b8434`, `Component_Network` `0x7b2f3c`, `Game::State::Game_State` `0x7b9a3c`,
  `Net::Session_Func` `0x7d6720`, `Net::Client_Node` `0x7bb448`. RVAs relative to ImageBase `0x400000`.
- Still needs runtime work (lock + guarded launch): find the module base, scan for vtable pointers,
  validate by moving the character, and count live instances. That is step 4's runtime half, not done.

## 2026-11-15 (step 1, Gate 0): partial at the time, closed as "no" later that day

Source: `notes/01-net-reachability.md` (branch `game/south-park-the-stick-of-truth-step01-net`, commit
`ebbea21`), merged by the integrator. Two launch series, both under the lock, all validity flags true.
The table below is what was known before the console work; the final verdicts follow it.

| Question | Verdict | Evidence |
|---|---|---|
| Host path | **No at the menu.** After a console `restart_server`: **unknown** (not triggerable yet). | `bind`, `listen`, `accept` = 0 in both runs; `SteamMatchmaking` never requested; `SteamGameServer()` returned NULL about 1,780 times; `SteamGameServer_Init` never called. |
| Client path | **Unknown.** Blocked on console `connect` (step 3). | `ws2_32!connect` and `WSAConnect` = 0 in all instances; no console command could run on this branch. |
| Two instances run | **Yes.** | Pair run: both alive at 80 s, guard clean. |
| Two instances join | **Unknown.** No join attempted. | No connect, bind, listen, P2P or lobby call in the pair run. |

- The menu polls `SteamNetworking` about 22 times a second; only vtable slot 2 runs (its name is unknown).
- Pair instance A's screenshot was fully black while A stayed alive and the guard was clean. Cause unknown
  (capture order or focus is the first suspect).
- Implication: the thin-mod versus thick-mod decision is **not made**. Both the host and the client
  trigger are console commands (step 3). If `restart_server` and `connect` reach `bind` and
  `ws2_32!connect`, the engine's own netcode is the thin path. If they do not, the puppet path is the fallback.

**Final verdicts (closed the same day, after the console work below).**

| Question | Verdict | Evidence |
|---|---|---|
| Host path | **No.** | Console `restart_server` survives at the menu but opens nothing; in a level it kills the process; a direct call to the session-manager callee faults on a non-executable address. `bind`, `listen`, `accept` = 0 in every run. |
| Client path | **No.** | Console `connect` kills the process both at the menu and in a level, with 0 net events; `ws2_32!connect` was never reached. |
| Two instances run | **Yes.** | Pair run: both alive at 80 s, guard clean. |
| Two instances join | **No.** | Nothing to join: no host ever came up. |

The shipped Onyx multiplayer is dead code in this build and cannot be started from outside. The plan is
the thick path (own transport and replication), MVP in `PLAN.md`, release shape in `SHAPE.md`.

## 2026-11-15 (lead): `-startsavegame` does not load a save in this harness

Two guarded launches of `tools/in_game.py --once --startsavegame …` under the lock, both
`run_valid: true` (profile unchanged, guard clean, exe hash matched):

| Run | Argument | in_game | Last frame | Notes |
|---|---|---|---|---|
| `in-game-6ae6719d…` | `Save Slot 001` | false at 84 s | front end, correlation 0.995 | no new saves, 14 profile files unchanged |
| `in-game-1857544d…` | `Save Slot 001.sav` | false at 84 s | front end, correlation 0.996 | same |

Observed in both: the level manifest `2_hub` is resident (held 9 times), 0 AMX script headers, working
set about 455 MB, and the screen never leaves the front end. So the save name is not the cause (the
`.sav` form was tried after the bare form), and command-line `-startsavegame` does not reach gameplay
in this build. The reliable way into a level is still the menu: click Continue, as in the step 2 runs.

Also: the step 3 launch of 2026-11-15 wrote its `exec` file but recorded **0 executed commands**,
so console execution is **not proven** on this branch.

## 2026-11-15 (lead, step 4 runtime): the player lives only in real gameplay

Every earlier scan ran in the intro bedroom, reached by `-startsavegame` or by the menu's
"Продолжить", because that loads the newest save `Твой дом`. In that scene the engine is not
in gameplay: no player component exists, `GetKeyState` is never called by the game, and
`PeekMessageA` is hammered about 830k times a second — by `gameoverlayrenderer.dll` (the Steam
overlay), not by the game. That is why the first runtime scan reported zero instances of every
focus class.

A save from the middle of the game is reached with four mouse clicks (menu "Загрузить игру",
a row, the bottom "Загрузить игру", then the confirmation dialog at 0.43,0.572). With that
route the scan runs in real gameplay and finds `tools/object_model.py --load-save fort`
(run `object-model-b1c39528`, all validity flags true):

| Class | Menu | Clyde's Fort |
|---|---|---|
| `OEI::Game_Component::DS_Component_Player` | 0 | **1** |
| `OEI::Game_Component::DS_Component_RPG_Player` | 0 | 2 |
| `OEI::Game_Component::NY_Component_Actor` | 1 | 29 |
| `OEI::AI::State::Package::Follower::Player_Basic` | 0 | 1 |
| `OEI::AI::State::Package::Follower::Companion_Follow` | 0 | 1 |
| `bfx::MoverImpl` | 0 | 3 |
| `NY_Component_Flash_Camera` | 0 | 5 |

The bare focus names (`Component_Player`, `Component_Actor`, …) still read 0 because their own
vtable is never instantiated — the live objects use the `DS_`/`NY_` derived vtables, which is
what the family logic was for. So the object model's focal point is now
`DS_Component_Player`, and the open question is its position offset, not its existence.

Input remains a **test-harness** limitation, not a product one: a human keyboard is not
affected. Synthetic keyboard, posted `WM_KEYDOWN`, the XInput pad and DirectInput state were all
tried; the game never polls `GetKeyState`/`GetAsyncKeyState` itself (only the Steam overlay and
`MSCTF` do), so the key path inside the engine is still unknown. The next probe is a one-off
position write to the `DS_Component_Player` neighbourhood, which validates the body by motion
and is the same write the puppet will need.

## 2026-11-15 (lead, input): the harness reaches a scene, not controllable gameplay

The step 3 notes already said "controllability is not shown". Nine guarded launch series on the
isolated copy (all three validity flags true in every run) now pin down what that means.

| Test | Result | Run |
|---|---|---|
| Keyboard at the main menu (Down/Up) | **works**: the highlight moves, frames differ | `input-check4-report.json` |
| Synthetic pad (`XInputGetState` hook) at the menu | **works**, but only when the hook is installed right after resume and a neutral state is held from the first frame; the selection moves | `input-check6-report.json` |
| Keyboard in the bedroom scene, then in the world after a click | **no change**: W/A/S/D, Enter, Space, Escape, clicks, pad A/B/Start/Back all leave the frame identical | `input-check2/3/4/8` |
| Engine's own input polling in the bedroom | `GetKeyState` 60 calls at the menu, **0 more** in the bedroom; `GetRawInputData` 12 → 16; `XInputGetState` still polled about 480/s (probably the Steam overlay) | `input-check4`, `input-check8` |
| Window activation | not the cause: `GetGUIThreadInfo` says active == focus == foreground == game window; `AttachThreadInput` + `SetForegroundWindow` + `WM_ACTIVATE`/`WM_SETFOCUS` change nothing | `input-check3` |
| A dialog on top of the game | none: 260 top-level windows listed, the game owns only its window and two hidden IME windows; a full-screen capture shows no overlay | `input-check8` |
| `-startlevel world\levels\1_southpark\2_hub.opf 0×7` | **does not load**: black frame, luma 2.2, no level | `input-check5` |
| "Continue" on the Russian menu | loads the bedroom save `Твой дом` (14.07.2025 22:35) — the top of the load list, a scripted intro | `load-screen-report.json` |
| Loading a mid-game save | the load list needs three clicks: row, bottom "Загрузить игру", then the **confirmation dialog** "все автосохранения будут стёрты" (button at 0.43,0.572). Without the third click the dialog just sits there | `fort-load-report.json` |

So the single blocker for every later step is not input delivery: the engine is in a state where it
does not poll the keyboard at all. The hub already has the technique for exactly this:
`playbook/methods.md` §7, "Inject input inside the process so a bot needs no focus" (The Witness) —
hook the input imports, find the input-manager bytes that gate the game, hold them, and post the
messages the OS would. Next step is that, on this engine.

Also recorded: the synthetic pad (`tools/input_probe.js`) is a working, focus-free input path for
the menu and is reusable for the mod's tests; `XInputGetCapabilities` was never called, so what
mattered was installing the hook before the engine's input init, not faking capabilities.

## 2026-11-15 (step 3, console): pipeline works, no command is registered

Source: `notes/02-console-and-devtools.md` (branch `game/south-park-the-stick-of-truth-step03-console`),
merged by the integrator. Four launch series; the 4th is the one that settled it.

- **Executed:** a script file in `data\global\console_scripts`, run through the engine's own `exec`
  handler, was opened and split into lines. Each line was echoed and dispatched. The engine answered
  `unknown var "<name>"` for `find`, `status` and `write_file`. Output is readable (the console print
  hook is at RVA `0x107c60`).
- **Not executed:** any real console command. `find`, `status` and `write_file` are **not registered
  in this build**, so the engine has no command for them. `connect` and `restart_server` cannot be
  reached by name. This is the same negative as "0 executed commands" in the earlier run.
- **Found:** in the live process `.text` is decrypted, and all 15 console strings are referenced
  from registration thunks (`push handler; push help; push flags; push name; mov ecx, <command
  object>; call Console::add_command @ 0x51b6a0`). Registration is apparently not taking effect at
  runtime. Cause unknown.
- **Tool:** `tools/console_exec.py` (`run_commands`, `probe`, `call_handler`). Caveat: a direct call to
  the handler without the wrapper faulted at address 0 in the agent's tests, so the wrapper matters.
- **Still unknown:** whether a direct handler call (`status`, `restart_server`, `connect`) reaches the
  network code. Follow-up is running on the same branch, under the lock.

Consequence for Gate 0: host and client verdicts stay **unknown**, not "no". The thin-versus-thick
decision waits for the follow-up.

## 2026-11-15 (step 3, direct handler calls): console route is a no

Source: `notes/02-console-and-devtools.md`, branch `game/south-park-the-stick-of-truth-step03-console`
(commits `38d1ab1`, `8b71b93`), merged by the integrator.

| Call | Where | Result |
|---|---|---|
| `status` (0x4d22f0) | menu and level | survives; prints labels (`hostname`, `version`, `client`, `server`, `os`, `level`, `cell`, `players`); values not yet readable |
| `restart_server` (0x4d1bf0) | menu | survives, prints nothing; no bind, listen, connect, Steam server or matchmaking |
| `restart_server` | inside a loaded level | **process dies** within 3 s; 0 net events |
| `connect 127.0.0.1:2005` (0x4d34b0) | menu | **process dies**, 0 net events |
| `connect 127.0.0.1:2005` | inside a loaded level | **process dies**, 0 net events |

- The level was entered by the same route as step 2 (menu, Continue, level verdict true).
- So neither multiplayer entry point reaches a socket or a Steam interface from the console. Cause unknown
  (no session object to restart, a missing singleton, or a deliberate abort).
- Decision rule (PLAN.md): one timeboxed attempt through the session manager's callees, capped at two launch series;
  if it fails, the thin path is closed and the thick path is the plan.

### Tools considered and rejected (2026-11-15)

- `vercel-labs/agent-desktop` (Apache-2.0, Rust, CLI + MCP, Windows 10 1809+): computer-use через
  UI Automation. **Not adopted for the game.** The game draws its UI in DX9/Scaleform, so UIA
  exposes no elements and ref-based actions have nothing to click; only `--headed` coordinate
  clicks remain, which is what our own Win32 `SendInput` already does, and the capture problem is
  ours to solve anyway. It also does not touch any hard gate (netcode, console, object model,
  second body, transport). Risks if adopted: unsigned binary, AV/SmartScreen and Smart App Control
  blocking, screenshots/clipboard can capture private desktop content, `--headed` steals the
  foreground from a capture in progress, and it would need the same machine-wide launch lock.
  **Possible later use:** Steam/crash-reporter dialogs during Gate 2, and installer QA for Gate 8
  (install → run → uninstall with UI text checks) where the UI is ordinary Win32/WPF/Chromium and
  UIA does work. If used: pin the version and SHA-256, install locally (not globally), with the
  owner's approval, under `tools/launch_lock.py`. Sources: the repo README and
  `agent-browser.dev/desktop/platforms/windows` (read 2026-11-15; Windows behaviour "not run for
  these docs" by its own authors, so unproven on this title).

### Next gate (replaces the AMX-loader gate)

Answer, with a test, whether Onyx netcode can be started in this build — host first, client
second, two instances on one PC third. Only if that is answered "no" drop to the
build-from-zero puppet path. `PLAN.md` and `prompts/` split this into seven agent-sized jobs, with
two PCs and two screens over the network as the product target and local two-player only as a
development milestone.
