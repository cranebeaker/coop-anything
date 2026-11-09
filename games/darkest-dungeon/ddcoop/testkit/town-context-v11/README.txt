DDCoop town-state probe v11 (read-only, test copy only)
======================================================

Purpose
  Reads the active state ID and state object from the game's TownController, and logs every call to its
  state-switch function. State IDs are decoded to names such as Stagecoach, Provisions, and HeroFacility.
  This replaces the previous probe's event-listener registry scan, which did not identify the open screen.

Safety and scope
  - It checks the supported Steam Darkest.exe SHA-256 before installing the probe.
  - It runs only %USERPROFILE%\DD-coop-test\DarkestDungeon with -nosave.
  - It changes only SDL2.dll in that test copy; the previous file is backed up once as
    SDL2-before-town-context-probe.dll. The real Steam game is not modified.
  - The probe only reads game memory and function arguments. It does not perform game actions or change saves.

Steps
  1. Extract this folder anywhere.
  2. With Steam running, double-click Run probe.bat.
  3. Load the test campaign and visit Provisions, Stagecoach, roster, Blacksmith/Guild, Nomad Wagon,
     heirloom exchange, Tavern, and Abbey. Leave each screen open briefly. Do not buy, recruit, dismiss,
     exchange, or assign heroes.
  4. Quit the game normally. Send coop-probe-town-context.log from the test copy's _windows\win64 folder.

The helper verifies the game and probe DLL hashes before replacing SDL2.dll. If either hash differs, it
exits without changing the test copy.
