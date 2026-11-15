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

### Tools considered and rejected (2026-11-15)

- `vercel-labs/agent-desktop` (Apache-2.0, Rust, CLI + MCP, Windows 10 1809+): computer-use через
  UI Automation. **Not adopted for the game.** The game draws its UI in D3D11/Scaleform, so UIA
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
