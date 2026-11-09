DDCoop town-context probe v9b (read-only, test copy only)
=======================================================

Purpose
  This probe reads the town-controller registry and active Darkest Dungeon town-controller objects so Codex can map safe, local
  contexts for replicated town actions. It does not change game memory or save files.

Safety and scope
  - It checks that this is the supported Steam Darkest.exe build before logging controller data.
  - It runs the existing test copy at %USERPROFILE%\DD-coop-test\DarkestDungeon with -nosave.
  - It only replaces SDL2.dll inside that test copy. SDL2_orig.dll is left in place.
  - Do not install this probe in the real Steam game folder.

Steps
  1. Extract this folder anywhere.
  2. With Steam running, double-click Run probe.bat. It invokes the helper with PowerShell's
     per-process Bypass setting, so no script signing or system policy change is needed.
  3. In the test copy, load the test campaign and visit each screen that is available:
     Stagecoach, roster, Blacksmith/Guild, Nomad Wagon, heirloom exchange, provisions, and Tavern/Abbey.
     Let each screen stay open for about 5 seconds. Do not buy, recruit, dismiss, exchange, or assign heroes.
  4. Quit the game normally. The script writes coop-probe-town-context.log next to Darkest.exe.
  5. Send that log to Codex. This diagnostic build also records whether it could read the controller registry.

To restore the previous test-copy proxy afterwards, copy
  %USERPROFILE%\DD-coop-test\DarkestDungeon\_windows\win64\SDL2-before-town-context-probe.dll
back to SDL2.dll in that same folder. The real Steam game is never modified.

This is a read-only probe. The separate game process is still launched with -nosave.
