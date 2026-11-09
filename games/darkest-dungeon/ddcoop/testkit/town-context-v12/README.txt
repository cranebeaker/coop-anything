DDCoop ProvisionSelect probe v12 (read-only, test copy only)
===========================================================

Purpose
  Watches the game's ProvisionSelect panel pre-show method directly and logs the panel's local address
  and vtable. It also logs the separate TownController state for comparison. This distinguishes the
  visible expedition provisions panel from unrelated listener and controller state.

Safety and scope
  - Checks the supported Steam Darkest.exe SHA-256 before installing the probe.
  - Runs only %USERPROFILE%\DD-coop-test\DarkestDungeon with -nosave.
  - Changes only SDL2.dll in that test copy; it backs up the previous DLL once as
    SDL2-before-town-context-probe.dll. The real Steam game is not modified.
  - The probe only reads memory and function arguments; it does not buy supplies or alter saves.

Steps
  1. Extract this folder anywhere and double-click Run probe.bat with Steam running.
  2. Load the test campaign. Open Embark, select a mission, fill the four hero slots, then open Provision.
  3. Stop on the provisions screen. Do not buy anything or start the expedition. Quit the game normally.
  4. Send coop-probe-town-context.log from the test copy's _windows\win64 folder.

The helper verifies the game and probe DLL hashes before replacing SDL2.dll. On a mismatch it exits
without changing the test copy.
