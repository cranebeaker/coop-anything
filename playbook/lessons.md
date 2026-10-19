# Hard-learned lessons

Drawn from six co-op projects (STALKER, Fallout New Vegas, Dungeon Keeper 1, Ocarina of Time, Animal Crossing, Deus Ex). Each one wasted time or produced a false result at least once.

1. **"No co-op exists" is usually wrong.** Searches on the default branch of a repo missed a complete multiplayer fork that lived on another branch, and a "no working Deus Ex co-op" claim was wrong because an older mod existed. Before telling the owner nothing exists, search forks, branches, release pages, forums and archive.org, state what you searched, and be ready to correct yourself in the open. Finding the existing work early changed the plan from "build" to "revive and extend" every time.

2. **Prove a fix before you claim it.** A "corpse cap fixes the frame rate" claim shipped before any test, and an A/B run with one variable changed showed it did nothing. Every fix needs a before and after run that changes only that one thing, and the claim in the notes should quote the run.

3. **Check the measuring instrument.** A 60 FPS engine cap hid real drops across several runs; a desync log printed turn 0 for every mismatch; a `-level` flag silently fell back to single player so the run "passed" while testing nothing. At the start of each run assert the mode you think you are in, and compare frame time, not capped FPS.

4. **A test that checks once is a flaky test.** One false FAIL came from checking ten seconds after a trigger when the flag arrived a few seconds later. Poll with a timeout for everything, and leave generous slack for late answers under load (client probes arrived late when many processes ran).

5. **Exit code zero is not a pass.** A pipeline through `tail` hid failing builds, and a test runner printed "is not recognized as an internal or external command" while exiting cleanly. A suite counts only if its output proves tests ran; a missing executable is a FAIL, not a skip.

6. **Put the test bot inside the game.** Simulated mouse and keyboard are slow, fragile and take over the owner's PC. A bot in the game's own scripting or code (writing packets, calling script functions, a JSON-RPC bridge plugin) is deterministic, invisible, and can assert against real state. "Needs a human playtest" is a gap to close, not a step in the plan.

7. **Test co-op with co-op.** Single client against a simulated partner finds protocol bugs but not the real ones. Two real clients, comparing shared state (and a lockstep checksum where the engine has one), found the camera desync while possessing, the wrong campaign progress file, and the stats screen hang. Most real bugs were in transitions (map, briefing, level, stats, loss, retry, disconnect, late join), not in plain in-level play.

8. **Mutation twins make tests trustworthy.** Ship a deliberately broken variant with every feature and require its test to fail on it. Add self-checks too: two dumps of one save must diff equal, and a known change must show up in the diff exactly. A test that cannot fail proves nothing.

9. **Never rebuild what you cannot rebuild; add a package next to it.** The original Deus Ex co-op package could not be recompiled (the compiler asserts on a class without script text, and the native DLL has no source), so fixes went into a separate package of subclasses and mutator modules selected by config. The pattern avoids binary compatibility traps and keeps the original intact for comparison.

10. **Single-instance checks come in layers.** Renaming the named mutex was necessary but not sufficient: the second game copy exited silently because of a window-class lookup in the start-up code. Search for every instance check (mutexes, events, window lookups, shared document folders, Steam checks) and fix them inside your own process, not by closing handles in other processes.

11. **Never touch the owner's install, config or saves.** Copy to a test folder, use instance-local INIs and saves, back up before editing, and never set attributes on the owner's files. One experiment set an owner INI read-only and had to be repaired. Also never use reserved save names and never use an old save that is known to crash.

12. **Never take desktop screenshots.** A screen recorder captures whatever else is on the screen, including a browser or private apps. Use the game's own screenshot command, in-engine capture or logs, tell the owner if anything else was captured, and delete the frames.

13. **Do not make the owner's PC unusable.** Full-screen launches by test agents were the loudest complaint. Always windowed, minimized without activation, "always active" set so the game does not pause, and a game-slot lease so agents do not launch more than one at a time. Memory pressure from many launches caused the host to kill a suite mid-run; run fewer at a time.

14. **Clean up after tests or you ship your test files.** A test archive left in an add-on made the server checksum differ, and other players saw "different versions". Remove test files, delete screenshots, and compare the installed patch byte for byte with the shipped one before handing anything over.

15. **Delete the log before each run.** A boot test of many levels showed the next level failing with the previous level's error because the log still held it. Other logs only flush on clean exit or rarely; for data you must have, write your own file and flush it.

16. **Find a hook that actually fires.** In STALKER the obvious per-tick hook (the actor binder update) never ran on a dedicated server; an effect attached to an always-online restrictor did. In another game, client-side binders did not fire while server ones did. Probe each hook with a trivial log line on the exact setup (dedicated, listen, client) before building on it.

17. **Textbook algorithms differ from the game's variant.** The standard LZHUF frequency limit corrupted decompressed output after about 97 KB because the game used a different maximum. When a decoder works for small files and breaks later, suspect a constant, and round-trip test every archive tool (pack, extract, diff).

18. **Do not trust claims in chat or by agents.** A moderator's "new version" was the old files under an unchanged number; a forum "re-upload" was a repost; an agent said the host could not use computers when a test showed it could. Verify by hash, date, version string, or a test, and label what is inference.

19. **Treat tokens and third-party data as radioactive.** A Discord token was pasted into the chat once; that means rotate it immediately. Account tokens in third-party tools also break the platform's terms, so prefer admin exports, bot accounts or manual reading. Never copy handles, IDs, invite links or personal paths into public docs, and do not argue with a safety classifier that blocks bulk collection: hand the step to the owner or find an in-process alternative.

20. **Never overclaim.** Bot-versus-bot runs, a mock Steam, and fixes reasoned from code with no two-player test are all useful, but none is a human or two-account proof. Set the QA level honestly, write only what was actually tested, and keep the open items in your notes or GitHub issues so the next agent picks them up.

21. **Several copies of a single-instance game for automated tests.** Besides the named mutex, check WinMain for a FindWindow(own window class) early exit. Neutralize both inside your own process via IAT hooks, and launch the copies from a long-lived process so they are not killed along with a short-lived launcher's job object. Example: Fallout: New Vegas (fnv-coop).

22. **A copied install is not enough when settings live in Documents.** The Witcher 3 saves its settings from the main menu, so test flags like `-width 960 -height 540` ended up in the owner's real settings file and its backup. Give every test instance its own user folder: hook `SHGetKnownFolderPath(FOLDERID_Documents)` and `SHGetFolderPath(CSIDL_PERSONAL)` in your own process, seed the sandbox with copies of the settings and the one save the test loads, and make the test plugin refuse to start without the sandbox.

23. **Faking focus has side effects.** To keep an alt-tabbed test instance running, The Witcher 3 needed `GetForegroundWindow` faked and `WM_ACTIVATE`/`WM_ACTIVATEAPP`/`WM_KILLFOCUS` filtered. A game that believes it is focused also clips the cursor, warps the mouse, steals focus and reads the keyboard, so in test mode neutralize `ClipCursor`, `SetCursorPos`, `SetForegroundWindow`, `GetAsyncKeyState` and raw-input `INPUTSINK` too, and mute the instance's own audio session.

24. **Two modern DX12 instances can take down the GPU driver.** Two Witcher 3 Remastered instances with ray tracing ended in a device-removed crash and a driver reset the owner could notice. Apply the game's own lowest preset to test instances (expand it from the game's options XML rather than guessing keys), turn ray tracing off, cap the frame rate, and check free VRAM first: desktop apps can hold most of the card.

25. **First-run screens are not yours to answer.** Account-link and telemetry consent screens appear on a fresh launch. Close them in test mode without signing in or recording a choice, so the owner still sees them and decides. System dialogs (for example a one-time "skill points reset" notice after loading an old save) do pause the game, so log and answer those.

26. **Record test video from inside the game, never from the screen.** Agents judge camera and animation work much better from video than from screenshots, but a screen-region capture of a test window on the owner's second monitor also recorded the owner's chat window lying on top of it. Capture frames in your own process instead: hook `IDirect3DDevice9::Present` (vtable slot 17), resolve the multisampled back buffer with `StretchRect`, copy it with `GetRenderTargetData` and write raw frames that ffmpeg turns into MP4 and contact sheets. Example: Clive Barker's Jericho (`native/capture.c` in jericho-coop).

27. **A pawn pinned by per-frame writes can die, and a spawned puppet can end the mission.** In Far Cry 2 the peer's avatar was created from the player's own archetype and moved by writing its physics body every tick at a fixed height. The pinned body took damage and both copies showed "Game over", although every entity check still said valid. Make a puppet invincible right after the spawn (the engine's own event, here `CFCXCountersComponent_SetInvincible`) and keep the writes per frame (about 140 per second) with smoothing; four writes per second looked like teleports and let gravity act between them.

28. **A scan of a live game's memory must not be able to fault.** Walking every committed region with direct reads crashed one Far Cry 2 copy when another thread released a page between the region query and the read. Copy in chunks with `ReadProcessMemory` (a failed read skips the chunk), and install a vectored exception handler that logs access violations, so a crash leaves a record. Many access violations at start-up are the OS or the game handling its own exceptions; log module and offset before you worry.

29. **Find objects by class, not by a pointer chain.** A breadth-first chain from the entity to the physics body (found in one session) broke in the next one, because it ran through lists whose order changes. The body was found in every session by its class (a static vtable) plus the nearest position to the entity. Also dry-run any loop that writes to memory: a helper named `Dir` hit the PowerShell alias, the test measured nothing and left its test value in 244 places because the restore step never ran. Print the first measurement and the first restore before looping.


30. **Do not copy a huge mod archive for every test run.** Each Shadow of COOP test packed a small test archive into a copy of the 4.2 GB mod add-on: 30 s to 2 min per run, and the copies filled the disk until runs failed to start. Loading the test files from elsewhere did not work: xrMPE mounts the add-on last, so a `patches` folder or loose files lose to it, and it reads only the first `-addon=`. What works: keep one permanent copy per test lane, and before each run cut it back to the end of the original archive data and append the new test archive. The build went from minutes to 3 to 4 seconds and a level boot check to about 70 s. Also run offline checks first (seconds): files shipped by two patches, broken escapes in script string paths (`"misc\coop.ltx"` in Lua reads `\c` as `c`), and config references to missing sections. Example: Shadow of COOP (`tools/stalker_xrmpe`, `qa_build.py --lane`).
