DDCoop town-context probe v10 (read-only, test copy only)
=========================================================

Purpose
  Captures town-controller states at the exact moment the game registers them. The earlier periodic
  scan sampled every five seconds and could miss short-lived screen states. This probe logs the
  registered state's subobject and base vtables plus the caller's game-code address.

Safety and scope
  - It checks the supported Steam Darkest.exe SHA-256 before installing the probe.
  - It runs only %USERPROFILE%\DD-coop-test\DarkestDungeon with -nosave.
  - It changes only SDL2.dll in that test copy; the previous file is backed up once as
    SDL2-before-town-context-probe.dll. The real Steam game is not modified.
  - The probe only reads arguments and memory. It does not perform game actions or change saves.

Steps
  1. Extract this folder anywhere.
  2. With Steam running, double-click Run probe.bat.
  3. Load the test campaign. Visit Stagecoach, roster, Blacksmith/Guild, Nomad Wagon, heirloom exchange,
     provisions, Tavern, and Abbey. Leave each screen open briefly. Do not buy, recruit, dismiss,
     exchange, or assign heroes.
  4. Quit the game normally. Send coop-probe-town-context.log from the test copy's _windows\win64 folder.

The console verifies the Darkest.exe and probe DLL hashes before replacing SDL2.dll. If it reports an
unsupported build or hash mismatch, it exits without changing the test copy.
