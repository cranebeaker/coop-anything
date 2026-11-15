# Step 3 — Gate 1: console and dev access

This file belongs to one agent step. Do not edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`
or `data/` — the integrator folds this file into them once no other agent is running
(see `../PARALLEL.md`).

- Prompt: `prompts/02-console-and-devtools.md`
- Agent: step 3 (console), worktree `D:\sp-step03`, branch
  `game/south-park-the-stick-of-truth-step03-console`
- Build tested: Steam app 213670, build 228353, exe 11,468,656 bytes, SHA-256
  `54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6`, checked before and
  after every launch. Isolated copy `D:\coop-work\game-test` (`SP_GAME_DIR`); the retail
  install and the real profile were never opened for writing.
- Launch lock: owner `step03`, one series at a time, released after each. Three series had to
  queue behind steps 1 and 2 (35-45 min each); no other agent's process was killed and no two
  launches overlapped.
- **Launch budget: 4 series, not 3.** Series 3 produced no answer because of a units bug of
  mine (absolute VAs scanned as RVAs); series 4 (25 s of game time) was run to settle it rather
  than hand over a step with no result. All four are listed below, including the two failures.
- **Second experiment: 3 launches, not 2**, for the same reason (run `78b687a1…` was a wasted
  discovery pass after an RVA/VA mix-up). The other two produced the verdicts.

## Verdict

**First experiment (console pipeline):** the console is **reachable and readable, but empty** —
the engine runs a console script, splits it into lines, echoes each line with its own `"] %s"`
prefix and dispatches the name — and then answers `unknown var "<name>"` for `find`, `status` and
`write_file`, because **no console command is registered in this build**.

**Second experiment (direct handler calls, `status` / `restart_server` / `connect`):**
- **Host path: no.** `restart_server` (handler rva `0x4d1bf0`, `void` signature) returns normally
  without a single `bind`, `listen`, `socket`, `SteamGameServer_Init` or `SteamMatchmaking` call —
  only the engine's ordinary `SteamNetworking` polling continued. Run `6052be13c272438082349985f8be5ad5`.
- **Client path: no (as a console command), and unknown as a capability.** `connect 127.0.0.1:2005`
  (handler rva `0x4d34b0`, verified `argc`/`argv` signature) **kills the process**, so nothing can
  be read back and no network call was observed. Run `6ebdc2a277c14ca3a74c42c87eaf68f9`.
- **Direct handler calls are not safe in general**: two of three calls (status, restart_server)
  survived; `connect` terminated the game. Nothing was retried.

## Observed (ran and saw)

### Second experiment — direct handler calls (runs `78b687a1…`, `6ebdc2a2…`, `6052be13…`)

Three guarded launches, all with `tools/net_probe.js` loaded as a second Frida script, all with
the three framework flags `true` and no guard errors.

1. **`78b687a179fd4982ba631b251f6693c1`** (30 s): discovery returned no candidates — my
   `scan_text` RPC was handed RVAs while the scan compares absolute VAs, the same units mistake
   as before. No call was made. Fixed in the tool (the RPC now takes an RVA).
2. **`6ebdc2a277c14ca3a74c42c87eaf68f9`** (~35 s): handlers resolved by scanning live `.text` for
   the command-name strings and reading the registration thunk's first `push`:
   `status` → thunk `0x714de0` → handler `0x4d22f0`; `restart_server` → thunk `0x714c90` →
   handler `0x4d1bf0`; `connect` → thunk `0x714d50` → handler `0x4d34b0`. Signatures read before
   any call:
   - `status` `0x4d22f0`: `push ebp / mov ebp, esp / sub esp, 0x104`, then straight into
     `push <string>; push "%8s: %s"; call 0x507c60` — **no argument reads**, i.e. `void(void)`.
   - `restart_server` `0x4d1bf0`: `push ebp / mov ebp, esp / sub esp, 0x460`, then
     `call 0x7e0e90; test eax,eax; je …; call 0x723290 …` — **no argument reads**, i.e. `void(void)`.
   - `connect` `0x4d34b0`: `cmp dword ptr [ebp + 8], 2` and `push "usage: connect IP<:port>"` —
     the same `(argc, argv)` convention as `exec`/`find`.
   Only `connect` passed my first (too strict) check, so it was the only one called:
   `call_handler(0x4d34b0, ['connect', '127.0.0.1:2005'])` **killed the process**
   (`alive_after: false`, `script has been destroyed`). No output, no net telemetry could be read
   afterwards. Recorded, **not retried**.
3. **`6052be13c272438082349985f8be5ad5`** (~40 s): `status` and `restart_server` called with
   **zero arguments** (their signature) via `Game.call_void`, plus live network hooks that
   `send()` each `bind`/`listen`/`connect`/`WSAConnect`/`SteamGameServer_Init`/`SteamMatchmaking`
   the moment it happens, so a crash cannot destroy the record.
   - `status`: process survived. The console print hook fired **8 times**, with the labels
     `hostname`, `version`, `client` ("not connected"), `server` ("remote"), `os` ("Windows"),
     `level`, `cell`, `players` — the status block really was produced. My printf renderer did not
     substitute that call's varargs (the captured text starts *at* each label and runs into the
     neighbouring `.rdata`), so the **values** (host name, build, level, player count) are not
     readable yet. Net delta: no `bind`/`listen`/`connect`/`socket`, only
     `SteamNetworking` 46 (the normal per-frame poll).
   - `restart_server`: process survived, **no console line at all**, and **no** `bind` (0),
     `listen` (0), `connect` (0), `socket` (0), `SteamGameServer_Init` (0), `SteamMatchmaking` (0);
     only `SteamNetworking` 46. Nothing was listening and nothing dialled after the call.

### First experiment — the console pipeline (4 series)

### Series 1 — read-only reconnaissance (run `b294e9229544431d8cd0d6b643a182f0`)

`py -3.14 -B tools/console_exec.py probe --duration 40`; 41 s at the main menu. The script only
read memory (no hooks, no calls). `original_profile_unchanged`, `guard_integrity`,
`original_exe_hash_still_matches` all `true`, no guard errors.

- **`.text` is decrypted in the live process.** The file has no 4-byte reference to any console
  string; scanning `0x400000+0x1000 … +0x7368d1` in the process for the address of 15 console
  strings found code for **all 15**: `"find: no matches for '%s'"` 1, `"usage: exec filename"` 1,
  `"exec"` 3, `"write_file"` 2, `"] %s"` 1, `'unknown var "%s"'` 1, `"global/console_scripts"` 2,
  `"con_echo_command 1"` 1, `"CONSOLE_EXEC"` 1, `"console_open"` 1, `"console_toggle"` 1,
  `"quit"` 1, `"connect"` 2, `"status"` 4.
- Every command-name hit sits in a **registration thunk** (live listing, `03-listings.json` in
  the run folder):

  ```
  6cdcd0  push 0x507e50        ; handler      (rva 0x107e50)
  6cdcd5  push 0xb524c4        ; help string  "executes commands from a text file"
  6cdcda  push 0               ; flags
  6cdcdc  push 0xb524bc        ; name         "exec"
  6cdce1  mov ecx, 0xf35a74    ; static command object (an absolute VA, .data rva 0xb35a74)
  6cdce6  call 0x51b6a0        ; Console::add_command(name, flags, help, handler)
  ```

  Same shape for `find` (`0x6cdc40`, object `0xf35a2c`, handler `0x107d00`), `revert_all`
  (`0x6cdc70`, `0xf35a44`, `0x107d60`), `revert` (`0x6cdca0`, `0xf35a5c`, `0x107df0`),
  `write_file` (`0x6cdd00`, `0xf35a8c`, `0x107e80`), `con_queue_disable` (`0x6cdd30`,
  `0xf35aa4`, `0x107eb0`), `if` (`0x6cdd60`, `0xf35abc`, `0x108090`) and `console_open`
  (`0x6f1ee0`, object `0x20a5c54`, handler `0x100a10`). Objects are 24 bytes apart.
- Those thunks are referenced from static-init arrays in `.rdata`
  (`0x7388d4`, `0x7388d8`, …, `0x73c568`, `0x73b0a8`, `0x73efec`, `0x73eff8`), i.e. registration
  is compiled into startup, not editor-only dead code.
- `exec` handler body (`0x107e50`): `argc` at `[ebp+8]`, argv at `[ebp+0xc]` as **0x80-byte
  NUL-terminated slots** (`argc == 2` → `argv + 0x80`); otherwise prints `"usage: exec filename"`
  through `0x107c60`. `find` (`0x107d00`) and `write_file` (`0x107e80`) use the same convention.
- `0x107f50` is the **console line executor**: splits on `\n` and `;` (quote-aware), handles
  `con_echo_command 1|0` itself, echoes each line with `"] %s"` when echo is on, and calls
  `0x107ec0` per line. `0x107ec0` tokenises the first word, looks it up (`0x514a50`), prints
  `unknown var "%s"` when it is not found, and otherwise dispatches.
- `0x107c60` is the console **print**: `push <arg>; push <fmt>; call 0x507c60; add esp, 8`
  (cdecl varargs).
- Script paths are `global/console_scripts/<name>.txt` (`"%s/%s"` + `"%s.txt"`, rva `0x114b10`);
  `write_file` rejects names containing `: \ / .` and defaults to `con_vars.txt` (rva `0x114ce0`).

### Series 2 — first execution attempt, aborted (run `481c25a6c29641cfa2a60d2c0ff49cad`)

Guarded, 5 s, framework flags all `true`. It **failed, and the failure was informative**:

- The session was armed 4 s after resume. `.text` pages were still CEG-encrypted at that moment
  (`0x107c60` disassembled as a garbage `jmp` to an address outside the image).
- `exec step03_script` was still resolved by the engine and the file was opened
  (`NtCreateFile D:\coop-work\game-test\data\global\console_scripts\step03_script.txt`, from the
  guard), but nothing was printed and nothing ran.
- Calling the line executor directly at that point killed the process (`script has been
  destroyed`). Conclusion: gate every call on the game having started, not on "it looks idle".

### Series 3 — readiness gated on the command table (run `c1ebf8ef02a04251b6587c8a24dec9e4`)

45 s, framework flags all `true`, **no execution**: my readiness gate waited for a populated
command table and never saw one, so it refused to call anything. The gate was right about the
table and wrong about what it means (see series 4). The scan itself was also wrong: I had
hardcoded the command-object addresses as RVAs although the thunks use absolute VAs.

### Series 4 — the engine executes the script and answers (run `235566a1423345e297beca06ca0090e5`)

25 s after resume, then `exec_file('step03_script')`. Framework flags all `true`, no guard
errors. Script written for the test copy:

```
con_echo_command 1
find multiplayer
status
write_file coop_step03_exec
```

- The guard logged `NtOpenFile …\data\global\console_scripts` and
  `NtCreateFile …\data\global\console_scripts\step03_script.txt` — the engine opened **my** file.
- The print hook fired **6 times** and returned the engine's own text, one entry per line, in
  this order (full text in `03-exec-file.json`, `result.json`):

  | # | engine text |
  |---|---|
  | 1 | `] find multiplayer` (the console echo prefix) |
  | 2 | `unknown var "find"` |
  | 3 | `] status` |
  | 4 | `unknown var "status"` |
  | 5 | `] write_file coop_step03_exec` |
  | 6 | `unknown var "write_file coop_step03_exec"` |

  (each captured string also carries trailing bytes from the engine's buffer; the meaningful
  prefix is quoted here, the rest is unreachable stack/`.rdata` text after the format argument.)
- No file was created by `write_file`, and `new_game_files` is `{}` — consistent with the
  command never being dispatched.
- Calling the line executor **directly** from the debugger thread still faults
  (`access violation accessing 0x0`) even 25 s in, while the same code executes normally when
  reached through the engine's own `exec` handler. So the function I found is the right body but
  not a safe entry point on its own; the wrapper that prepares it is not identified.

## Derived (concluded from evidence, with the evidence named)

- **Execution through the engine's console path works.** `exec` opened the script in the test
  copy, the line executor consumed its lines, the first line (`con_echo_command 1`) took effect
  (proven by the `"] "` echo that appears only when that variable is on, and which was absent for
  the first line itself), and each following line was echoed and dispatched. Evidence: series 4
  guard events + captured lines 1-6. Confidence: high.
- **Console output is readable as data.** Hooking `0x107c60` yields the engine's own formatted
  text (`"] %s"` and `'unknown var "%s"'` both rendered from live format strings). Evidence:
  series 4 captured lines; series 1 handler listings show every handler printing through
  `0x507c60`. Confidence: high.
- **The console command registry is empty at runtime in this build.** Every registered name the
  script used came back `unknown var`, although the registration thunks are wired into static-init
  arrays. Evidence: series 4 lines 2, 4, 6 plus series 3's empty command-object scan of all of
  `.data`. Confidence: high for "lookup fails", medium for "nothing is ever registered" (see
  Unknown).
- **This single fact explains the three earlier negatives**: `-exec <file>` (its lines would all
  answer `unknown var`), the inert `data/global/console_scripts/*.txt` autoload (same path), and
  the dead `-connect`/`-startlevel` command-line switches (they are console commands; the help
  block even lists `-connect` and `-startlevel` as command names). Evidence: series 4 behaviour +
  the command-name string block at `.rdata` `0x7c98f8` (`-connect`, `-startlevel`, `quit`). The
  earlier sessions could not tell "wrong trigger" from "empty registry"; now it is the registry.
- **Direct handler calls reach real engine code**: `status` and `restart_server` are plain
  `void(void)` functions, both ran, both returned, and `status` produced its 8-line block.
  Evidence: run `6052be13…`, survival + 8 print-hook hits. Confidence: high.
- **`restart_server` does not start anything in this build at the main menu**: zero `bind`,
  `listen`, `socket`, `connect`, `SteamGameServer_Init`, `SteamMatchmaking` in the 1.5 s after the
  call, while the same probe counted 46 `SteamNetworking` polls in the same window (so the hooks
  were alive and the session was running). The command's own help text says "Restarts the server
  at the current position", and there is no position at the menu, so this is *not* proof that the
  host path is impossible — it is proof that the console cannot open it from the menu.
  Confidence: high for the observation, medium for the reason.
- **`connect` as a console command is not usable**: the handler is the same `(argc, argv)` shape
  as `exec`, and calling it with a valid argv block killed the process. A working client path
  would not do that, so the console route to `connect` is closed; whether the session manager
  itself can dial remains open (step 1's problem). Confidence: high for "this call kills", medium
  for "the client path is impossible".
- **The print hook is not a general printf.** It renders `"] %s"` and `'unknown var "%s"'`
  correctly (series 4), but for the `status` block it captured the label pointer instead of the
  formatted line, so numeric/string arguments are not reliable yet. That limits "read the
  result" to commands whose output is a plain format at argument 0.
- **Static analysis alone cannot find any of this**, because `.text` is CEG-encrypted at rest and
  decrypted lazily in the live process: the file contains zero references to the console strings
  while the process contains 15 for 15. Any later step needing an engine function address in this
  build should start from `console_exec.py probe`.
- **`write_file <name>` is a valid oracle once a command can run at all** (it writes
  `con_vars.txt`-style dumps into `global/console_scripts`), but it cannot be used to prove
  execution while the registry is empty — which is why the earlier session's "write_file wrote
  nothing" was not evidence either way.

## Unknown

- Why registration does not take effect: whether `Console::add_command` is skipped behind a
  retail/developer check, runs against a different console instance than the one `0x514a50`
  looks up, or is defeated by the CEG-decrypted layout. Answer: hook `0x51b6a0` and log its
  arguments at startup, or read the singleton that `0x514a40` returns.
- Whether the `console.gfx` UI can be opened with a key. `gui/console.gfx`, `gui/console.scn`,
  the `console_open`/`console_toggle`/`console_close` commands and the `CONTEXT_CONSOLE` input
  context all exist, but no keybinding file ships in `data_archive.oaf` and no `console.*`
  resource was opened at startup in any run. Needs a foreground-keypress + BitBlt capture test.
- The exact wrapper that prepares the line executor (direct calls fault at `0x0` while the
  engine's own path works). This is the main loose end of this step.
- Whether calling a handler directly (`call_handler`) or registering our own command objects
  makes the useful commands work. **Partially answered:** direct calls work for `status` and
  `restart_server` (both survived) and crash for `connect`; registration was not attempted.
- Why a console handler that only calls `bind`/`listen` would need anything the menu lacks:
  whether `restart_server` would start a server in a **loaded level**. Answer: rerun
  `Game.call_void(0x4d1bf0)` after step 2's harness has a level loaded, with the same live net
  hooks. This is the single cheapest follow-up and it decides whether the host path is reachable
  from the console at all.
- Whether the `status` values are readable: the print function's argument order for multi-argument
  calls is unresolved (labels captured, values not). Answer: hook `0x107c60` and log `args[0..4]`
  raw for one `status` call, then fix the renderer.
- What `status`, `getpos` or the net commands print in a loaded level — moot until a command can
  run at all, and a Gate 2 dependency anyway.

## Deliverable

- `tools/console_exec.py`
  - `run_commands(['status', 'find multiplayer']) -> {'ok', 'output': [str], 'evidence': {...}}`
    Writes the lines to `<game>/data/global/console_scripts/<name>.txt` in the isolated copy,
    launches the guarded copy, waits for the menu, hooks the console print (`0x107c60`), calls
    the engine's `exec` handler (`0x107e50`) with `(argc, argv-in-0x80-byte-slots)`, collects
    what the engine printed, and reports the files the commands created plus the three framework
    flags. `ok` means "the engine ran the script and answered", not "the command succeeded";
    the engine's answer is in `output` (`unknown var "<name>"` in this build).
  - `Game(...)` session object with `arm()`, `exec_file(name)`, `exec_line(text)`,
    `exec_line_main(text)` (queues for the game's message pump), `call_handler(rva, argv)`,
    `call_void(rva)` (zero-argument handlers), `scan_text(rva)` (live `.text` xref scan),
    `net_events()` (crash-tolerant network record), `extra_scripts=['net_probe.js']`,
    `command_table()`, `disasm(rva, n)`, `dwords(rva, n)`, `output()`.
  - `probe()` / `py -3.14 -B tools/console_exec.py probe` — the live-`.text` xref scan that found
    every address above; reuse it for any other engine string.
  - `py -3.14 -B tools/console_exec.py run "status"` — takes the launch lock itself (unless
    `--no-lock`), prints the JSON result.
- Known-good addresses (RVAs, base `0x400000`, all from the live process):
  print `0x107c60`; line executor `0x107f50` (eax argument, unsafe entry); add_command
  `0x51b6a0`; handlers `exec 0x107e50`, `find 0x107d00`, `write_file 0x107e80`,
  `revert 0x107df0`, `revert_all 0x107d60`, `con_queue_disable 0x107eb0`, `if 0x108090`,
  `console_open 0x100a10`, `status 0x4d22f0` (void), `restart_server 0x4d1bf0` (void),
  `connect 0x4d34b0` (argc/argv, **crashes**), `quit 0x3e1f20` (not called).
- Run folders under `tools/game-runs/`: `b294e9229544431d8cd0d6b643a182f0` (reconnaissance),
  `481c25a6c29641cfa2a60d2c0ff49cad` (armed too early, crashed), `c1ebf8ef02a04251b6587c8a24dec9e4`
  (readiness gate refused to call), `235566a1423345e297beca06ca0090e5` (executed the script,
  captured `] `/`unknown var` output), `78b687a179fd4982ba631b251f6693c1` (discovery units bug),
  `6ebdc2a277c14ca3a74c42c87eaf68f9` (`connect` call, process killed),
  `6052be13c272438082349985f8be5ad5` (`status` + `restart_server`, both survived, no network).
- Files this step created in the test copy `D:\coop-work\game-test` (all inert, listed so nobody
  mistakes them for game files): `data\global\console_scripts\step03_script.txt` and
  `global\console_scripts\step03_script.txt` (75 bytes each). Earlier sessions' `platform_win`,
  `init`, `console`, `autoexec`, `probe_console`, `quit` scripts are untouched. The handler
  experiments wrote no files (no `write_file` was reachable).
- **Honesty note:** `run_commands()` is a repackaging of the exact call sequence the series-4
  driver used (`arm` → `exec_file` → drain `output`). The *CLI* form was not re-run after that
  refactor, because the launch budget was already exceeded at four series.
- **Launch count for the second experiment: 3, not 2.** The first was wasted on the RVA/VA units
  bug in `scan_text` (run `78b687a1…`, no call made). All three are listed above.

## Handoff notes for the integrator

- `README.md`: replace "no agent can execute a console command" with "the console pipeline runs
  and its output can be read (`tools/console_exec.py`), but this build registers **no** console
  commands, so console text answers `unknown var`. Handlers can be called directly instead:
  `status` and `restart_server` run and return; calling `connect` kills the process."
- `NOTES.md`, Gate 0 section: add "`restart_server` called directly at the main menu does **not**
  start a server: no `bind`, `listen`, `socket`, `SteamGameServer_Init` or `SteamMatchmaking`
  (run `6052be13…`). Its help text says it restarts the server *at the current position*, so it
  must be retried in a loaded level before the host path is called impossible."
- `NOTES.md`, under "Still unknown": drop "whether the multiplayer commands are registered in the
  command table" and replace with "the command table is empty at runtime; why registration does
  not take effect is open (hook `0x51b6a0`)".
- `PLAN.md` Gate 1: mark it **partially answered** — execution and read-back: yes; useful commands
  through the console: no (empty registry), but direct handler calls work for `status` /
  `restart_server` and are the practical route from here.
- For step 1 (net reachability): the `-connect` command-line switch, the console `connect` command
  and a direct `connect` handler call are all dead ends at the menu (two did nothing, one crashed).
  Gate 0's client path must go through the session manager, not the console.
- For step 2 (gameplay harness): once a level is loaded, `Game.call_void(0x4d1bf0)`
  (`restart_server`) with `extra_scripts=['net_probe.js']` is the cheapest host-path test in the
  project. The live `net_events()` record survives a crash.
- For step 4 (object model): `tools/console_exec.py probe` is the working way to find engine code
  in this build (static xrefs do not exist because `.text` is encrypted at rest), and
  `Game.disasm`/`Game.dwords` read live `.text`/`.data` through the same guarded session.
- If you publish this, keep the failures in the story: "the console works but is empty, and
  `restart_server` at the menu does nothing" is a different claim from "the console/netcode does
  not work", and only the run folders support the first one.
