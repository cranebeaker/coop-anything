Darkest Dungeon co-op - test kit 8 (save slots; read-only probe v8)
===================================================================

What this run is for
  We decided: real co-op play uses the bottom campaign slot, and test sessions run with -nosave.
  Before the mod relies on that, this run checks how the game names the save slots: which storage it
  mounts when you load a campaign, and whether it ever removes a save folder by itself. The game is
  started with -nosave, so it saves nothing. The probe only reads memory. Source: probe.c.

Steps
  1. Unzip this kit, open PowerShell in that folder and run:
         powershell -ExecutionPolicy Bypass -File .\update-probe.ps1
  2. Make sure Steam is running, then run:
         powershell -ExecutionPolicy Bypass -File .\run-savecheck.ps1
  3. At the campaign list, WRITE DOWN which positions (1 = top ... 9 = bottom) hold a campaign, and at
     which position the TEST campaign is.
  4. Load the TEST campaign (only that one). Wait until you are in the Hamlet.
  5. Quit to the main menu, then quit the game.
  6. Put coop-probe-saves.log (in %USERPROFILE%\DD-coop-test\DarkestDungeon\_windows\win64) in
     <project dir> on the development machine, together with your notes from step 3.
