# Forager (2019)

| | |
|---|---|
| Engine | GameMaker Studio 2, compiled with **YYC** (GML compiled to x86 machine code; `data.win` has no CODE/VARI/FUNC chunks). GameMaker runtime 2.2.5.378 |
| Versions tested | Steam build 6160665 (in-game version 4.1.9, GEN8 2.2.2.302, built 2021-01-29), Windows. `Forager.exe` SHA-256 `ad2b90f164320f646c340b0be60af966851dfdc74dce366580f3dcd531853ae6` (PE timestamp `0x60148428`), `data.win` `0f5ccbfbbd80dd7eb5f1c4fde676c4c1f8f1dc77e24bcce9086d8ec2b38144e8`, `steam_api.dll` `af17df745250d1814eaa274fff7b0faeb43381e6762e026267e5859778477abd` |
| Projects | [Forager Co-op Mod](https://github.com/melogo5/forager-coop) (MIT; releases v0.1.0 and v0.1.1) |
| QA level | automated: scripted sessions drive 2 to 4 real game clients on one PC and print PASS/FAIL per check, with mutation twins. Also nine two-PC playtests, eight of them over Steam |
| Last updated | 2026-10-30 |

## Where things stand

Up to four players can play one Classic world online: a host and up to three guests, joined by a Steam invite
(F2 in the world) or over LAN/VPN. The world, enemies, buildings, chests, islands, time and weather, level and XP,
coins, the skill tree, the museum, NPC quests and dungeons are shared; each guest keeps their own inventory, gear,
familiars and costume. The mod is a single `version.dll` next to `Forager.exe`; no game files are changed. It is
released as a beta: the Void trial does not work in co-op, and a guest occasionally loads a slightly different world,
which the mod corrects within seconds.

## Existing work

- **Forager Co-op Mod** (https://github.com/melogo5/forager-coop): started 2026-10-16 as a prototype on the game's
  official Modding API (`mods/CoopTest` in the repo, frozen), rewritten as a native `version.dll` patch on
  2026-10-17. 149 commits up to 2026-10-29, about 16 500 lines of C++. Public since 2026-10-28.

## How the game works (for co-op)

- **No scripts to edit.** YYC compiles every GML script and object event to a native function inside `Forager.exe`.
  The names survive: each compiled function writes a pointer to its own name (`gml_Object_objPlayer_Step_0`) into
  the YYC stack trace, so the function that references that string is the one you want. Ghidra 12.1.4 (headless)
  named and decompiled 3969 of about 4080 functions this way. 46 of them (among them `objPlayer/Step_0`, `Step_2`,
  `WorldControl/Step_2`, `SaveGame`) time out or run out of memory in the decompiler; those were read from listings
  and by tracing the running game. UndertaleModTool CLI dumps objects, rooms, sprites and strings from `data.win`.
- **One player, behind one function.** `objPlayer` adds itself to `global.players`, and 365 functions get the
  player through `PlayerFind(n)`. A probe hooked on it saw only `n = 0` (about 1.4 million calls in 4 minutes, about
  350 per frame), and no code loops over the list: the abstraction is there, nothing uses it.
- **Input is global** (`InputCheck*` read `global.inputStatus`), so a second `objPlayer` reacts to the same keys.
- **No pause.** Open menus do not pause the game; about 30 objects check `IsMenuOpen` themselves and stop.
- **Culling.** `WorldControl/Step_1` calls `CullObject` (deactivates everything outside `global.camera*`) and
  `ProcessCulls` (wakes what came back into view). Objects culled on the host are invisible to all its code, so a
  guest far from the host plays in a frozen world unless the host keeps it awake.
- **The world is random and timed.** `ResourceSpawn` puts resources and enemies on random owned land; drops come from
  `DropItem` in dozens of `Destroy_0` events. Only one machine can run this.
- **Saves** hold one player (`SaveGame`, `SerializeCharacterChunk`); the world is in `saveData<N>/Zones/*.cin`.
- **Steam.** The game already loads and initialises `steam_api.dll` (Steamworks SDK about 1.41) and calls
  `SteamAPI_RunCallbacks` every frame, so a DLL can use lobbies, invites and `ISteamNetworking` P2P through the flat
  API, with callbacks arriving on the game thread. Lobbies and P2P work under Forager's own app id 751780.
- **The official Modding API** (1.0.1) has TCP sockets but no `buffer_seek`, UDP or JSON. Mods are unloaded on every
  return to the main menu (their `global.*` and any game structure they wrapped are destroyed, and a network event
  during unloading crashes the game), `OnStep` does not run while a menu is open, and mods cannot write files, so the
  host's save could not be handed to a guest. These limits are why the first prototype was frozen and the mod moved
  to a native DLL.

Runtime structures, the addresses the DLL uses and the hook list are in [NOTES.md](NOTES.md).

## What we changed

The mod: https://github.com/melogo5/forager-coop (MIT). Its developer docs, playtest analyses and code review are in
Russian, in `docs/coop/`.

**Model.** `version.dll` proxies the Windows library of the same name and hooks game functions with MinHook. It
checks the exe's PE timestamp and only forwards calls on any other build. The host is authoritative: the world lives
and is saved only on the host; a guest gets the host's save in a temporary slot (99, deleted afterwards) and then a
stream of changes. Every other player is a second `objPlayer` whose own events (Step, Alarm, Mouse) are skipped and
whose Draw runs (a puppet). Because `objPlayer` inherits `parAlly`, enemies target puppets with no extra code.

**Actor swap.** The `PlayerFind` hook returns the current actor: normally the local player, but while the host
performs a guest's action, that guest's puppet. The game's own code then reads the guest's tool damage, skills, buffs
and gear from the puppet, and outputs aimed at the player (`GainXp`, `GainItem`, coins, `DmgPlayer`, toasts) go to
the guest instead of being applied on the host. A guest's hit travels as `hit(hostInstanceId, dmg)`; the host runs the
object's `event_user(0)` with the puppet as actor.

Main pieces, with the problem each one solved:

| Area | Problem | Fix |
|---|---|---|
| Culling | the world around a guest far from the host was asleep on the host | hooks on `instance_deactivate_object` and `ds_list_add` keep anything inside a camera-sized box around each guest awake; `ProcessCulls` runs again per guest box |
| Destroys | objects the guest had culled survived the host's destroy (20 trees at the host against 71 at the guest) | destroyed ids are taken off every cell sleep list; culled replicas are destroyed too |
| Reconcile | guests sometimes loaded extra or missing trees | every 5 s the host sends the objects around each guest; a difference seen twice in a row is fixed |
| Ghost enemies | guest hits on enemies missed | enemies at the guest are exactly the host's replicas; local spawns and stale replicas are removed |
| Ranged and sword damage | arrows, staffs and sword swings hit the guest's replica, and the host's hp overwrote the damage | before the host's hp is written into a replica, any drop since the last write is sent to the host as a hit |
| Drops | items doubled or taken by the wrong player | drops go straight into the inventory of whoever broke or killed the object, sent only to that guest |
| Chests, stations | two players in one chest or furnace | the host arbitrates; the later opener is told it is busy and its menu closes |
| Skills | the guest's points were written to an unused variable; trees diverged | one tree for everyone: a skill bought by anyone is repeated for all through the game's own `ItemEdit`/`StructureEdit` calls; points stay personal |
| Shared progress | level, XP, coins, structure unlocks | kept by the host and broadcast; unlocks copied with `StructureEdit(s, 11, true)` |
| Museum, quests | lost donations, "Quest not found" | donations merge as a grid (max per cell); NPC quest state is polled every second and applied with `NPCSetQuest`/`NPCSetState` once the NPC exists |
| Dungeons | first blocked, then players were split | everyone enters and leaves together; puzzle pieces, lanterns, keys, the boss key's ice and chests are synced; death in a dungeon respawns at its entrance and resets that player's minecarts |
| Interiors | entering the wizard tower or museum stopped the world for the others | interiors are rebuilt inside the world room from a room dump of `data.win`; the museum's per-instance creation code is called by address |
| Death | game over ended the session | respawn on the starting island, no save, no game over |
| Players | 2 only | a star through the host, up to 3 guests, a peer id per connection that is never reused within a session |
| Transport | IP, Hamachi or Radmin needed | Steam friends-only lobby, F2 invite, `+connect_lobby`; frames cut into 512 KB P2P packets; TCP and Steam guests can share one session |
| Robustness | a slow guest froze the host; a vanished guest stayed forever | a writer thread and queue per connection, TCP keepalive, a hello frame before a connection counts, the host validates every value a guest sends |

## QA

- **Several copies on one PC.** With `steam_appid.txt` (`751780`) next to the exe, `Forager.exe` starts without
  relaunching through Steam, and two to four copies run side by side.
- `tools/gametest/coopflow.ps1` starts a host and a guest and drives the menus with window messages (no real mouse);
  `regress.ps1` runs one fresh-world session with debug switches (`COOP_DEBUG_AUTOHIT`, `_BUILD`, `_STORAGE`,
  `_COINS`, ...) and prints PASS/FAIL per check from both logs and the game's `error.log`. About 40 scenario scripts
  cover single features (`sharedskills.ps1`, `museum.ps1`, `traps.ps1`, `relay.ps1` for three players, `four.ps1`
  for four).
- `COOP_DEBUG_PORT=1` opens a query port into the running game: read objects and variables, call scripts
  (`query.ps1`).
- `mutants.ps1` runs each check plain (must PASS) and with its fix turned off through `COOP_DEBUG_MUTANT` (must FAIL).
- Last recorded results (from the mod's docs, not re-run for this write-up): `regress.ps1` 9/9 PASS after the code
  review (2026-10-25). After playtest 9 (2026-10-27, `regress.ps1 -HostSlot 5`) all PASS except "shared chest": on that
  save the debug helper's `instance_nearest` could not see the chest because it was culled; the storage code had not
  changed. Four players (`four.ps1`, 2026-10-20): every copy sees the three others; drops reached only their owner
  (guest 1: 10 of 10, guest 2: 35 of 35, guest 3: 0).
- Nine playtests on two PCs from 2026-10-19 to 2026-10-27, eight of them through Steam invites, the longest
  3 h 15 min. Both players' logs were analysed after each; the analyses are `docs/coop/PLAYTEST*.md` in the mod repo.
  Three and four players were tested only as copies on one PC.

## How to play

Every player needs Forager on Steam at the build above. Download `version.dll` from
https://github.com/melogo5/forager-coop/releases/latest and put it next to `Forager.exe` in `<game dir>`.
Host: main menu, COOP, CREATE, pick a save or a new Classic world, then F2 in the world to invite. Guest: accept the
Steam invite, or COOP, type a name, pick the host's LAN server, JOIN. LAN and VPN play use TCP 7790 and UDP 7791.
To uninstall, delete `version.dll`.

## Known issues

- The Void trial does not work in co-op (respawn outside the arena, timer and enemy count stuck at the guest).
- A guest occasionally loads a slightly different world (extra or missing trees); it is corrected within seconds.
- Only Classic worlds. Forager's own Mod API mods are not supported.
- Only the host buys islands and gets Steam achievements.

## Next steps

- Void: find how the game builds and removes the arena (callers, world and global changes), the way interiors and the
  land map after an island purchase were handled.
- Crafting from a chest that another player holds open: the closer's contents overwrite what the craft used. It needs
  a merge, not only a lock.
- Root cause of the occasional different world at the guest. The mod logs `world: WARNING the world came incomplete`
  when a snapshot is short.
- Test three and four players on separate PCs.
