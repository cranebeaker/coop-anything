# Running several game copies for tests

Co-op tests need 2 or more real game clients on one PC, started by a script, often by several agents at
the same time, on a machine where the owner may be playing. This page is how to do that without breaking
the owner's game, the owner's saves, or each other.

The tools in this repo implement everything below for any Windows game:

- [`tools/gameslot`](../tools/gameslot/README.md): a lease lock (slots, RAM guard, pid-scoped kills).
- [`tools/gamepool`](../tools/gamepool/README.md): hard-linked isolated installs, launch helpers, a warm pool,
  a low-cost profile, crash evidence. One JSON file per game holds everything game specific.

They were ported from the Fallout: New Vegas co-op project and need nothing but Python. For finished launchers
that give each copy a gamepad and a screen region (split screen), see [split-screen.md](split-screen.md) and the
Nucleus Co-op row in [tools.md](tools.md). The harness shape these tools serve is in [automated-qa.md](automated-qa.md).

## The short version

1. Take a slot first. Never start a game without one.
2. Give every copy its own install folder made of hard links, with private settings, saves and logs.
3. If the second copy exits after a moment, the game has a single-instance check. Make the check pass inside the
   game process, or run the copies in separate sessions or sandboxes.
4. Start copies from a detached process, minimized, one after the other, and wait until each is ready.
5. Record every pid you start (with its creation time). Kill only those.
6. Never write to the owner's install, settings or saves, and prove it by hashing them before and after a test run.

## 1. One isolated install per copy (hard links)

A 50 GB game does not need 50 GB per copy. On one NTFS volume, a hard link is a second name for the same file,
and a folder of hard links appears in about a second.

- Mirror the install: hard link every file, create real folders. Fall back to symlinks, then to plain copies
  when links are not possible (different volume, no symlink right). Check free disk first: a copy fallback
  needs the full size per instance.
- A hard link is the **same file**. Anything the game or a tool writes through a link changes the owner's
  install. So every file the game writes must be a real private file in the instance:
  settings files, rendered config, shader caches, the runtime log, crash dumps, plugin folders you fill, and the
  save folder (create it empty).
- When you rewrite a private file, delete it first, then write. That way an old link from an earlier run is
  never written through. Refuse to edit any file with more than one hard link, any link, and anything outside
  the instances folder.
- Many games read settings and write saves under `Documents` or `AppData` by default. Look for the game's own
  switch to use its install folder instead (a "portable" or "use install directory" setting), set it in the
  private settings file, and add the owner's real folders to a protected list that no tool may write to.
- Start the private settings from a copy of the owner's file (read only), then apply your edits. The owner's file
  usually has the DLC and mod list the game needs.
- Record the SHA-256 of the files that must not change (the main executable and the main data file) when you
  build the instance, and check them before a test. A hash that changed means a link was written through or
  an update replaced the install.
- Windows `mklink /H` and `mklink /J` do the same for a hand-made setup; Nucleus Co-op can create symlinked
  copies for you ([tools.md](tools.md) section 3).

`gamepool provision` does all of this from the config (`link`, `config_files`, `protected_paths`).

**Unity games with no "portable" switch** keep saves in `Application.persistentDataPath`
(`%USERPROFILE%\AppData\LocalLow\<company>\<product>`), often a Steam Auto-Cloud folder. Unity resolves it with
`SHGetKnownFolderPath(FOLDERID_LocalAppDataLow)`, imported by `UnityPlayer.dll` (and by `GameAssembly.dll` in IL2CPP
builds). A test-only loader plugin that patches that import in the game's own modules, early (MelonLoader
`OnPreInitialization`), and answers LocalLow with `<instance>\LocalLow` gives every copy private saves, prefs and
analytics files; add `-logFile <instance>\Player.log` for the log. Verified on Slime Rancher 2 (Unity 6000.0, IL2CPP),
owner folder hashed unchanged across runs: [games/slime-rancher-2/qa/SaveIsolation](../games/slime-rancher-2/qa/SaveIsolation).
A `steam_appid.txt` in the copy stops the Steam relaunch of the store install.

## 2. Single-instance checks: named mutex and FindWindow

Many games refuse to start a second copy. Two mechanisms are common:

| Check | How it works | Symptom |
|---|---|---|
| Named mutex | The game calls `CreateMutex` with a fixed name at start up and exits if the name already exists (`ERROR_ALREADY_EXISTS`) | The second copy shows an error box or closes within a second or two, with a non-zero exit code |
| Window class or title | The game calls `FindWindow` for its own class or title and exits or activates the old window when it finds one | Same, or the first copy's window jumps to the front |
| Unity player setting | `<Game>_Data/boot.config` has a `single-instance=` line, and the Unity player exits when a copy is already running. Remove the line in the test copies only; it is a text file, not the exe (Graveyard Keeper, Unity 2020.3) | The second copy closes at once |

How to tell which one: watch the second copy with Process Monitor or API Monitor for `CreateMutex` and
`FindWindow`, or read what the game logs on the way out. The launcher in `gamepool` reports a copy that exits
within `watch_s` seconds together with its exit code, so you notice at once instead of waiting for a timeout.
Also check the image path of each process: a launcher that starts a second copy of the original game from the
store folder can take the mutex, and then it is not your copy that is in the way.

Ways around it, best first:

1. **Fix it inside the game process.** A mod, plugin or proxy DLL that loads before the game's own start-up code
   wraps `CreateMutexA/W` (and `FindWindowA/W`) in its own process. For the mutex, make the name unique per
   process; for the window check, make the call report "not found" or give each copy a unique class. Only
   the game's own process is touched. The shape of the fix:

   ```c
   // Runs inside the game process. Route the game's CreateMutexA calls here (patch the import table or use a hook library).
   HANDLE WINAPI MyCreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL own, LPCSTR name) {
       char unique[256];
       if (name && strcmp(name, "NameTheGameUses") == 0) {
           snprintf(unique, sizeof unique, "%s-%lu", name, GetCurrentProcessId());
           name = unique;
       }
       return CreateMutexA(sa, own, name);
   }
   ```

   Gate it behind a config switch that only the test instances turn on, so players never run it.
2. **Separate sessions or sandboxes.** Mutex and window namespaces are per Windows session and per sandbox. Run
   each copy under its own Windows user, in a Sandboxie-Plus box, or in a VM. Heavier, but needs no code.
   Nucleus Co-op's handler options cover this family of tricks ([tools.md](tools.md) section 3).
3. **Close the mutex handle from outside.** Technically possible (`DuplicateHandle` with `DUPLICATE_CLOSE_SOURCE`
   on the first copy's handle), but it reaches into another process, and an agent permission system will rightly
   refuse it. The New Vegas project dropped this route. Do not choose it.

Do not patch the game's executable to remove the check, and do not touch store DRM. This page is about running
your own copies of a game you own.

`gamepool/tests/fake_game.py` is a stand-in that has both checks (`--mutex`, `--window-class`) and the
per-process-name fix (`--rename-mutex`, `--unique-window-class`). The test suite uses it to show that the
second copy is stopped, that the early exit is reported with its exit code, and that two copies run once the
names are unique.

## 3. Launch from a long-lived process

A game started by a short command (an agent's shell call, a script that ends) often dies when that command ends,
because the parent's job object closes and takes its children with it.

- Start copies with `DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB`. If the job forbids
  breakaway the call fails with access denied; retry without that one flag.
- Better still, let a daemon (the warm pool) own the games. The command line only talks to the daemon.
- Some games start through a loader that starts the real game and exits. The pid you launched is then
  the wrong one. Find the real process **below the loader in the process tree**, not by "a new process with that
  name appeared": another agent, or the owner, may have started the game at the same moment. A process keeps the
  pid of its parent after the parent exits, so the lookup still works after the loader is gone.
- Start copies one after the other. Copy k must be ready (probe answers) before copy k+1 starts, because the
  first copy's start-up may create the lock the second one trips over, and two starts at once fight for the disk.
- Define "ready" per game: a window exists, a ready file appears, a log line is written, a port or named pipe
  answers. Poll with a timeout; never check once.

## 4. Minimized, windowed, cheap

Copies the owner never looks at should cost little and never take the screen:

- Start with `STARTUPINFO.wShowWindow = SW_SHOWMINNOACTIVE` (7). The window opens minimized and does not take
  the foreground, so the owner's active window keeps focus.
- Many games create a second window after the splash screen. Keep minimizing new windows for a few seconds after
  the last one appeared (`ShowWindowAsync`, again without activation). Never send input to find or move a window.
- Settings that cost the most: small window, windowed mode (so it can stay minimized), no vsync, a frame cap,
  no shadows, no anti-aliasing, skip intro movies. Keep ticking while unfocused (most engines have an "always
  active" or "run in background" setting).
- Some savings change behaviour: fewer loaded world cells can make a save made at another setting crash or
  corrupt on load; no audio can stall a test that waits for a voice line to end. Keep those opt-in.
- Use the game's own logs and in-game screenshots as evidence, not desktop capture, which grabs whatever else is
  on the owner's screen.

`gamepool lowcost` applies a profile from the config to the private settings only.

## 5. RAM guard

Two or three big games can push a PC into swapping, which also ruins the owner's session.

- Keep a free-RAM floor (3 GB is a start) and refuse to start another copy when free RAM is below it. The first
  copy and lease renewals are never refused. `gameslot` does this with `min_free_gb`.
- 32-bit games hit their own limit (about 3 GB of working set) well before the PC is full. A warm pool restarts a
  copy when its working set grows by a set amount over the size at start-up, or passes a fixed limit.
- Measure instead of guessing: sample the working set and CPU of each copy while idle, with and without the
  low-cost profile (`gamepool measure`).

## 6. Pid-scoped kills

The owner may be playing the game on the same PC while your tests run. A kill by image name
(`taskkill /IM Game.exe`) ends their session.

- Record every pid you start, with its image name and its creation time. Kill only recorded processes, and only
  while pid, name and creation time still match. A pid can be reused by an unrelated process after yours ends.
- Register each pid with the slot lock too. When a crashed owner's lease expires and someone takes it over, the
  new owner kills only the pids that lease registered.
- Refuse to start when a game process that is not yours is already running, unless the caller says it is safe.
  It may be the owner's own game.
- Use `taskkill /T` (the process tree) for a recorded copy: a loader or helper below it should go with it.
- Test this with real processes: start a decoy with the same image name, run your stop code, and require that the
  decoy is still alive.

## 7. Never touch the owner's game or saves

- Work in copies. The install is read only; the owner's settings and save folders are on a protected list.
- Hash the owner's save folder and settings files before and after a test run. A changed hash is a bug in the
  harness, not in the game.
- A test that loads a save loads a copy of it into the instance's own save folder.
- Do not run your copy against online services or anti-cheat. Copies are for your own LAN or loopback tests.

## 8. Cold start or warm pool

| | Cold start (`up`, `job`) | Warm pool (`pool up`, `lease`) |
|---|---|---|
| Start-up cost per test | Minutes (the game boots) | Milliseconds (a copy is already running) |
| State between tests | Fresh | Whatever the last test left, so reset or mark dirty |
| Use it for | Few tests, tests that need a clean boot | Many short tests, bots that load a save and check state |

A pool hands out whole instances, restarts dead or unresponsive ones before a test sees them, recycles a copy
after K leases, on RAM growth, when the test marks it dirty, or when its lease expired, and gives every instance
a generation number. A test checks that number so it never talks to a freshly restarted game that lost its state.

## 9. When a copy crashes

- Exit code first: a copy that exits within seconds of launch is a start-up failure (single-instance check, missing
  file, bad setting). A copy that dies later has a crash record.
- Collect the game's logs, the marker lines in them, and the Windows Application log entry (event id 1000)
  for the process. A game that crashes at exit, or shows its own error box, may never flush its log, so
  "no error in the log" proves nothing.
- Pass log patterns through the environment, not into a command string, when you query the event log.

`gamepool crashlog` does this from the `crash` block of the config.

## 10. Test the harness itself

A harness that cannot fail is worth nothing. Build a stand-in game (a script that can hold a mutex, look for a
window, write a ready file, crash) and test the tools against it:

- Real processes for the things that must be real: pids, kills, mutexes, window classes, ports, pipes.
- A decoy with the same image name next to every kill test.
- Mutation twins: break one guard on purpose (kill by name, link everything, never expire a lease) and require
  the same check to fail. A check that still passes against the broken code tests nothing.

Both tools do this: `python -I tools/gameslot/run_tests.py` and `python -I tools/gamepool/run_tests.py` print
`ALL OK` only when every check passes and every twin is caught.

## Checklist for an agent

- [ ] A slot is held before any launch, and released after every launch is killed.
- [ ] Each copy runs from its own hard-linked folder; settings, saves and logs are private files.
- [ ] The owner's install, settings and saves are protected and hashed before and after.
- [ ] A single-instance check is solved inside the game process or with a separate session or sandbox.
- [ ] Copies start detached, minimized, one at a time, each waited on until ready.
- [ ] Every pid is recorded with its creation time; nothing else is ever killed.
- [ ] Free RAM is checked before another copy starts.
- [ ] The test fails when the thing it guards is broken (a twin).
