# Shipping to players: one file per OS

The mod works on your PC. Now your friend has to install it, and every manual step (find the game folder, pick
the right loader build, unzip in the right place, set a launch option) is a place where they give up or get it
subtly wrong. Ship **one file per operating system** that does the whole install, and test it the way a player
runs it.

Learned on Planetbase ([games/planetbase](../games/planetbase/README.md)): explaining the manual install to a
friend took longer than the session. A `.bat` for Windows and a `.sh` for Linux that do everything got a second
player into the game on the first try. MDK2 HD ([games/mdk2-hd](../games/mdk2-hd/NOTES.md)) ships the same idea as
a PowerShell script with `.cmd` wrappers.

## What the file does, in order

1. **Find the game without asking.** Read Steam's install path (Windows registry `HKCU\Software\Valve\Steam`
   `SteamPath`, then `HKLM\SOFTWARE\WOW6432Node\Valve\Steam` `InstallPath`; on Linux `~/.steam/steam`,
   `~/.local/share/Steam`, Flatpak and Snap paths), parse every library in `steamapps/libraryfolders.vdf`, and
   read `installdir` from `appmanifest_<appid>.acf`. Offer `--game-dir` for everything else.
2. **Check the game build** before writing anything: hash the file your mod depends on (main exe, or
   `Assembly-CSharp.dll` for Unity) against the build you tested. On a mismatch, say how to restore it (Steam >
   Verify integrity) and stop unless the player insists.
3. **Install the loader** (BepInEx, UE4SS, script extender) from its official release URL, pinned to one version,
   checked by SHA-256. Pick the right architecture: Planetbase is 32-bit and the x64 BepInEx does not load. If a
   loader is already there, check it is the right build rather than reinstalling over it.
4. **Install the mod** the same way: pinned file, SHA-256 checked, the previous version backed up.
5. **Set what the OS needs.** Under Proton, BepInEx needs the launch option
   `WINEDLLOVERRIDES="winhttp=n,b" %command%`. Steam keeps launch options in
   `userdata/<id>/config/localconfig.vdf` and rewrites that file when it quits, so close Steam first (ask), back
   the file up, change only that one value, and merge into an existing `WINEDLLOVERRIDES` instead of replacing it.
6. **Offer to start the game** (`steam://rungameid/<appid>`) and print the two lines a player needs: how to host,
   how to join.
7. **Uninstall from the same file**, and **update by running it again**.

## What it must not do

- Run anything it downloaded without the player's consent. A script that replaces itself is "running a downloaded
  script unread" (AGENTS.md rule 8): ask first, and never do it in a non-interactive or `--yes` run; just report
  that a newer version exists.
- Trust a hash fetched from the same place as the file. A manifest next to the mod catches corruption, not a
  compromised repo; pin the loader's hash in the script itself.
- Write outside the game folder, except the one platform file it must change (backed up first).
- Ask for admin rights up front. Try, and if the folder is protected, offer to restart elevated.
- Hide what it did. Keep a log, show a summary, and point to it in every error message.

## Make it usable by someone who never opened a terminal

- Windows: a `.bat` the player double-clicks. A `.bat` can carry its PowerShell body after a `<# : ... #>` header,
  so it is still one file. Expect SmartScreen's "Windows protected your PC"; say in the README which buttons to
  press.
- Linux: `bash file.sh`. Check for the tools it needs (`curl` or `wget`, `sha256sum`, `unzip`) and print the
  install command for the main distributions when one is missing.
- Every failure ends with a "what to do" box in plain words, not a stack trace.
- A menu by default (install or update, uninstall, quit); `install`, `uninstall`, `--yes` and `--game-dir` on the
  command line so tests can drive it.

## Test it like a player

Run it against test copies, never the owner's install (AGENTS.md rule 2). Planetbase's scripts read environment
overrides (`COOP_STEAM_ROOT`, `COOP_FAKE_STEAM_RUNNING`, a local `COOP_SOURCE`) so a test can point them at a fake
Steam folder and a local release. Cover: first install, update over the previous release, a wrong game build
refused with nothing changed, uninstall, and a second run that finds everything already done. Install the
previous release the way a player did before testing an update: Jericho Co-op+'s update broke for players with
two shortcuts because every test used one ([lessons.md](lessons.md), lesson 45).

Then send the file to someone who has never seen the mod, and watch where they get stuck.
