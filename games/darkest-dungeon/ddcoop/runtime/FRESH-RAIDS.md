# Test58: start a new raid after resuming a saved one

Observed in the test57 two-PC report: both games agreed on the next mission,
party and supplies. The host generated the shared map seed, but the guest did
not call the map generator and resumed an old battle instead. Its native log
also reported restoring saved entries. The guest runs -nosave and still has the
initial session checkpoint on disk after returning to the Hamlet.

The exact executable shows Darkest::RaidStart optionally calling Raid::Restore
at60eee0 when app+120d is set. A true result skips the new app+1720 parameters;
a false result follows the game's normal fresh-start branch. Restore reads
persist.raid.json and persist.map.json from the mounted campaign storage.
tests/test_fresh_raid_layout.py proves that branch, arguments and13-byte
instruction-aligned trampoline against the pinned executable.

The runtime now scopes a fresh-departure flag to a shared RaidStart with valid
new parameters and map/Rules seeds. The guest must also match the host mission
and persistent party IDs. During that call only, optional old-checkpoint Restore
returns false so native code generates the new mission. Initial saved-raid resume,
other campaigns and standalone play retain native Restore behavior. The flag is
cleared after native RaidStart; no disk save, app resume flag or original backup
is edited. A fresh departure that produces no map stops both peers instead of
silently accepting an old raid. Protocol27 is unchanged;120 signatures are checked.

The regression models the reported lifecycle: resume the original battle, then
enter two subsequent missions while the guest disk checkpoint never changes.
Both new missions must use the host seeds and party, without calling old Restore.
It also covers scope cleanup, wrong mission/party and passive restore behavior.
Other runtime and launcher suites remain required. Native game effects in these
fixtures are faked; the repaired two-PC lifecycle still needs a live playtest.

Reconnect refusal in the same report was intentional: after a game connection is
lost, both games must restart and the host creates a new session/snapshot. This
change does not add live rejoin or weaken that protection. English notices and
README explain that workflow. Initial guest backup/recovery and host save
preference remain unchanged.

Written by Codex,9 Nov2026; prior Claude Code and Codex work retained.
