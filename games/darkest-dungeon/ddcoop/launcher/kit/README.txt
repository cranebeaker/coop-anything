DDCoop - development test kit {VERSION}
======================================

New in test58 / duo41
- A new expedition now uses a fresh dungeon on both PCs. The guest no longer
  resumes the initial saved raid when departing again with -nosave.
- Launcher, in-game DDCoop notices and these instructions are in English.
- The animated mascot is built into the launcher. No extra GIF file is needed.
- Default party-order button/shortcut, inventory checks and earlier fixes remain.

Install on BOTH PCs
1. Close both games. Let DDCoop finish restoring the guest's own campaign, then
   close the launcher. Extract this entire ZIP.
2. Open DDCoop-install and double-click Install DDCoop.bat. It finds the supported
   Steam installation and installs into mods\DDCoop. No manual DLL copying.
3. Open DDCoop. Host: choose the campaign slot and click Host a game.
   Guest: select the hosting friend and click Join friend, or use the host code.
4. Let both games load the selected slot automatically. Wait for the connection
   status before shared actions. F9 opens DDCoop during gameplay.

Progress and recovery
The host follows Save progress when hosting. The guest always uses -nosave.
Only the selected campaign slot is borrowed; it is backed up, replaced with the
host snapshot and restored automatically after the guest game closes.
Close both games after testing and let recovery finish. Repair DDCoop.bat is not
needed for normal save recovery. Backups are in DDCoop-backups under your user folder.

If a player disconnects
This version cannot resume a running session. BOTH players close the game. The
host clicks Host again to create a new session; the guest then joins that session.
Rejoining the old running host is intentionally refused to prevent loading an
outdated snapshot. Do not delete saves or manually copy a different campaign.

Test plan
See TESTSTAPPEN-test58.txt. Resume a dungeon, complete it, return to the Hamlet,
then depart on a second mission with a changed party. Repeat if possible.
Compare the map, room, party, supplies and first fight on both PCs.
Use Copy logs on both launchers afterwards; send both bundles and the last action
if anything differs. New changes have automated coverage but still need live tests.

Scope and limits
Two players; supported Steam Windows x64 build28385. No automatic combat resync.
Use the same kit on both PCs. Arbitrary manual dungeon formation drags are not
newly synchronized; the default-order button and shortcut are synchronized.
DLC groundwork remains, but full Color of Madness/shards/waves support and DLC
hero gameplay still need validation. Steam game assets are not included.

Files (SHA-256)
{HASHES}
