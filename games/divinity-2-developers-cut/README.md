# Divinity II: Developer's Cut (2012)

| | |
|---|---|
| Engine | Gamebryo-derived ("Divinity Application", Larian Studios); DX9, PhysX, embedded Lua 5.1 + Osiris scripting |
| Versions tested | Developer's Cut v1.4.700.49, unofficial repack (not Steam/GOG); exe SHA-256 `633B8EC683FD7CACE512F40E1943731FA9DA1342DB081E0A5657B6FEEEB9C8BB`. Steam/GOG builds untested and likely need address re-finding. |
| Projects | Split-screen foundation for 2 players on one PC (this folder; no separate repo yet) |
| QA level | automated (per-instance profile isolation layer; see QA for exact scope) |
| Last updated | 2026-10-18 |

## Where things stand

Nobody can play Divinity II together today. The exe has zero networking code (no
socket/lobby imports or strings) and no prior co-op attempt was found anywhere. This
project is at the foundation stage: a `dinput8.dll` proxy now injects into the game, and
two instances get fully separated profiles and saves, verified with two real game
instances. There is no co-op gameplay yet. Two instances can run on one PC, but
simultaneous startup is flaky (timing race in the engine's main-menu movie player, a
pre-existing bug, not ours).

## Existing work

None found. Research on 2026-10-18: no entry in this repo's directory; GitHub API
searches for divinity2 co-op / dv2 unpack returned 0 results; the Larian forum's
Divinity 2 technical board has no co-op, multiplayer or mod-loader threads (scene
essentially dead since 2012); a binary scan found no multiplayer strings or imports.
NexusMods returned HTTP 403 from the research environment, so it was not verified by
hand; if a mod exists there it was not seen here. Verdict: build from zero.

## How the game works (for co-op)

- Engine: Gamebryo-derived "Divinity Application", 32-bit PE (WOW64 on modern Windows),
  image base `0x400000`, DX9 + PhysX, VS2008 runtime.
- Scripting: statically linked Lua 5.1 VM (C API registration table near VA
  `0x014C6448`) plus Osiris, Larian's behavior scripting (it debuted in this game).
  Quest/world logic lives in XML triggers and compiled scripts inside `.dv2` packs; no
  public mod loader.
- Netcode: none at all. No `ws2_32`/winsock imports. All co-op netcode must be added.
- Single-instance guard: named semaphore `"Div2Lock"` (string at VA `0x1476B04`, guard
  function retries ~15 s then exits). A second copy can run if the string is renamed in
  its exe (same length, one occurrence).
- Profile/saves: built via `SHGetFolderPathA` with `CSIDL_LOCAL_APPDATA` (three call
  sites total; `0x1c` is the only one that matters) into the machine-wide
  `%LOCALAPPDATA%\Divinity 2\` (Profile + Savegames). All copies share it unless
  redirected. The `0x23` site builds a regenerable shader cache under ProgramData; the
  `0x05` site is an existence probe that never writes.
- Injection surface: the exe imports exactly one function from `dinput8.dll`
  (`DirectInput8Create`) -> a `dinput8` proxy DLL is the cleanest in-process vector.
- Debug build (`Divinity2-debug.exe`) exposes an F11 console with teleport / god /
  lua-script-assign commands; useful for future co-op puppet tests.

## What we changed

1. `dinput8` proxy injection (Stage 1, `stage1/`).
   - Need: in-process code execution; the game has no mod loader.
   - Fix: 32-bit `dinput8.dll` that loads the real system `dinput8.dll` and forwards
     `DirectInput8Create`. Export recipe matters: the implementation is a stdcall
     function and a `.def` alias (`DirectInput8Create = DefDirectInput8Create@20`)
     produces the undecorated name the game imports, with a native `ret 20`. Built with
     llvm-mingw (clang i686); TinyCC was tried and fails at this (stdcall decoration or
     a broken stack), Zig was unusable from the test network.
   - Test: module enumeration showed both the proxy and system `dinput8` loaded in the
     running game; the proxy log recorded attach -> load real -> forward; the game
     survived past the 60 s crash window with no crash event; an isolated
     non-DirectInput caller got the expected `E_NOINTERFACE` and lived, proving stack
     cleanup is correct.

2. Per-instance profile and savegame redirection (Stage 2, `stage2/`).
   - Bug: all copies share one machine-wide profile; settings clobber each other and
     autosaves would overwrite each other.
   - Root cause: the profile root comes from `SHGetFolderPathA(CSIDL_LOCAL_APPDATA)`;
     nothing in the engine supports per-instance paths.
   - Fix: the Stage 1 proxy gains a MinHook hook on `SHGetFolderPathA` (hooked by name,
     so it survives other exe builds), enabled by the `DIV2_INSTANCE` environment
     variable: set to a short token, the token becomes an instance root segment and
     every copy gets its own `...\Divinity 2\Profile\` and `Savegames\`. Unset or
     invalid -> no redirection at all, so normal single-instance play is untouched. The
     token is validated (length and path characters) so a malformed value cannot escape
     the base folder.
   - Test: `test-hook.ps1` launches both copies with tokens, hashes the real profile
     before and after, and scans exit-writes and crash events. Across five game-launch
     runs the real profile was untouched every time (0 files changed) and exit-writes
     landed in the per-token redirected profiles. See QA for the coexistence caveat.

3. Second-copy test harness (not a player feature).
   - Need: run two copies of a single-instance game for the tests above.
   - Fix: a second folder whose `Data`/`images` are junctions to the shared data (no
     7 GB duplicate), with the `Div2Lock` string renamed in its exe.
   - Test: both instances reached stable main menus together (owner confirmed two
     windows); the second copy alone also runs and exits cleanly, proving the copy is
     healthy.

4. Known game bug documented, not fixed: the main-menu movie player passes a NULL movie
   object into a setter (fault RVA `0xB1024A`, caller is the main-menu movie player).
   Under dual-instance contention (two DX9 apps + two WMV decoders on one GPU) it
   either crashes with an unhandled access violation or exits early with code 0. This
   predates our work and is the reason dual-instance startup is flaky.

## QA

- How to run: `stage1/build.ps1` and `stage2/build.ps1` build the proxies (PE32 /
  export / `ret 20` checks, SHA-256 printed); `stage2/test-hook.ps1` runs the dual
  instance test and asserts: hook target is the real `shell32!SHGetFolderPathA`,
  redirection fires for the relevant CSIDL values, per-token profile trees are created,
  the real profile has zero file changes vs. a pre-run SHA-256 snapshot, and no new
  crash events appear.
- Last suite result (2026-10-18, five game-launch runs): isolation assertions passed in
  every run (real profile untouched; exit-writes in per-token profiles). Dual-instance
  coexistence is flaky: 1 simultaneous run hit the known movie crash (WER 1000);
  2 staggered runs had the second instance exit early with code 0; 1 staggered run had
  both instances alive 40 s+; 1 second-copy-alone control run was healthy (exit 0 after
  75 s). So: isolation layer = deterministic pass; two healthy simultaneous instances =
  timing-dependent, not yet reliable.

## How to play

Not yet: there is no co-op release. The DLLs and scripts here are test-harness
artifacts, not a player mod. Anyone who wants to reproduce the isolation layer can
build it from source with the `build.ps1` scripts (llvm-mingw, i686 target) and the
bundled MinHook source.

## Known issues

- Running two instances at once is unreliable: the second instance sometimes exits
  immediately or hits the main-menu movie crash (see above). Each copy is healthy on
  its own; the isolation layer itself is deterministic.
- Everything here is tied to one exe build (the addresses above are for SHA-256
  `633B8EC6...EB9C8BB`). A Steam/GOG copy will need the addresses re-found by string,
  though the `SHGetFolderPathA` hook itself is build-independent by design.
- The tested copy is an unofficial repack, not Steam/GOG. Friends with a store copy may
  have a different build; version match must be verified before any play-together plan.

## Next steps

- Stage 6b: a launch-retry loop and/or a main-menu movie disable/skip to make
  dual-instance startup reliable.
- Use the Lua C API registration table (near VA `0x014C6448`) as a control channel for
  driving a second character from the injected DLL.
- Then the actual co-op sync core, per this repo's `playbook/how-coop-mods-work.md`,
  and window/layout/input routing per `playbook/split-screen.md`.
