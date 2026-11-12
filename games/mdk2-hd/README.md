# MDK2 HD (2011)

| | |
|---|---|
| Engine | BioWare in-house (MDK2), OpenGL 3.0 renderer, Lua 3.1 scripting with tolua bindings |
| Versions tested | Steam app 213350, build id 3902, `mdk2hd.exe` SHA-256 `bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43`; the Overhaul Games (Beamdog client) build, `mdk2hd.exe` SHA-256 `2e4ae928...39f9` (the mod's `docs/DEVELOPMENT.md` gives it in this short form) |
| Projects | [mdk2-coop](https://github.com/TommoDotCommo/mdk2-coop) (directory entry "MDK2 HD") |
| QA level | automated |
| Last updated | 2026-11-11 |

## Where things stand

MDK2 HD has no multiplayer. mdk2-coop adds drop-in co-op: two or more players move through the campaign
together, with the host's world (enemies, shots, pickups, doors, checkpoints, level loads, cutscenes) mirrored on
every client. The first public beta, [v0.4.0-beta.1](https://github.com/TommoDotCommo/mdk2-coop/releases/tag/v0.4.0-beta.1)
(2026-11-11, a pre-release with a one-file Setup and a launcher), supports the Steam build and the Overhaul Games
build. Its automated ladder has 109 gates, including all 96 checkpoints with host and client; one complete run
passed 105 of them (details under QA). Nobody has finished the whole campaign in co-op yet, and the automated
tests do not solve puzzles or beat bosses.

## Existing work

None found before this project (searched DIRECTORY.md, this repo's issues and PRs, and the web on 2026-10-20).

## How the game works (for co-op)

- Everything is in `mdk2.tar`, a flat ustar archive (5842 entries) with the 101 game scripts as plain Lua source.
  Loose-file overrides do not apply to scripts, and `dofile()` only reads from the archive.
- `%LOCALAPPDATA%\MDK2HD\settings.lua` is written by the game and run at boot after `mdk2.lua`, so one appended
  line can load a mod. Keep it small: the game reads it into an 8 KB stack buffer and crashes when it is larger.
- Lua has no io library, so networking needs native code. The exe imports DINPUT8.dll, so a dinput8.dll proxy
  in the game folder is the native side. It hooks the frame, damage, room and door functions and talks UDP.
- `debug.lua` ships in the archive: `dofile('debug')` enables BioWare's debug menu (F9), and its checkpoint
  list doubles as the test plan.
- The engine runs several hero gobs at once (the final boss spawns Kurt, Max and Doc with AI), so a partner
  can be a second `OBJ_KURT` puppet created from Lua.
- Rooms: each frame the room manager deactivates every room, then wakes the one holding the camera plus the rooms
  visible from it; everything else is in stasis. Co-op has to wake the rooms around every player, by the same
  rule on every machine, or a room's OnCreate runs on one side only.
- `gob_delete` only marks a gob; it stays in the gob table (and its Lua global stays valid) until the scene's
  cleanup. Name lookups in the same frame still find it.
- Spawners name every spawn `<spawner>_spawn`, and some spawned actors have info-less sub-gobs carrying the same
  name, so name matching between machines needs a numeric fallback and has to ignore those parts.
- Full-state saves (`mdkStartInstantSave` / `mdkStartInstantLoad`) work mid co-op; save paths are cut to 16
  characters, and every copy of the game on one PC shares `%LOCALAPPDATA%\MDK2HD`.

Details, addresses and evidence: [NOTES.md](NOTES.md).

## What we changed

Each fix below came from a failing automated or manual check and was re-tested; NOTES.md has the evidence.

- Partner puppet with position, orientation and animation sync; never placed inside the local player.
- Host-authoritative enemies (spawn, despawn, snapshots); client spawners off except item spawners.
- Shots replayed on every machine; damage routed through the host; revive at a partner instead of game over.
- Delayed projectile launches, including Doganboy grenades, carry their finished trajectory and fuse;
  bouncing shots use their rigid body's velocity. Copied attack animations cannot duplicate launches.
- Pickups, proximity doors (open for the nearest player), checkpoints and level loads followed by every client.
- Crash fixes: NULL attacker in damage events; Max's anim-loop handler deleting a gob the puppet never held.
- Desync fixes found by the state diff: stale puppet positions after loads waking rooms early; a despawn and
  respawn under the same name binding to the dying gob; info-less name twins producing 0-hp copies; rooms woken by
  camera on one side and by body on the other.
- Up to 16 players (host relays), rejoin mid-level, co-op saves and resume after a crash of client or host.
- Going down: a hit that would kill you puts you down; a teammate holding R stands you up; bleed out and you
  spectate until the zone is clear or a checkpoint is reached; everyone down reloads the checkpoint.
- Two transports: direct UDP, and Steam friends-only lobbies with invites and a lobby ID (Steam P2P messages,
  no port forwarding).
- On screen: players panel, name tags, person-shaped outlines through walls (off by default), chat bubbles and pings.
- Text chat (T) and pings (V or middle mouse); proximity voice (push to talk, host-set range).
- Options > Co-op menus in the game's own style: players, session, gameplay, difficulty (Story, Normal, Hard,
  Nightmare, Custom) and a Controls page with Original, Modern and Custom key presets.
- A launcher before the game (host, join or solo, transport, name, settings, screen mode), an installer that
  checks the Steam build hash, and a release script.
- Couch co-op: 2-4 local players, one window and gamepad each, by running several game copies.
- Cutscenes: everyone starts together, skipping is a vote with a countdown (Esc), and the next part waits until
  everyone has reached the end. In the skydives and torpedo minigames the other players are shown as ghosts.
  An early "go" from the host (received before the local cutscene had started) entered playback without its clock
  and raised Lua errors on clients in two campaign fixtures; it now uses the normal start path.
- A character wheel (hold J: Kurt, Max or Doc at any time) and an emote wheel per hero (hold H). Kurt and Max are
  wider than Doc, so a hero switched in at a wall can be stuck in it; held move keys put it on nearby spots until
  it moves. The time limit for that started at the switch, so a player who waited 4 s before walking stayed stuck;
  it now starts with the first held move key.
- A save browser and a level and checkpoint picker in the launcher.
- Removed: an AI texture upscaler (it made the game darker and flatter than the original).

Since the alpha (v0.3.x and v0.4.0-beta.1; the mod's release notes in `docs/` have the full list):

- A second game build. The Overhaul Games build has the same code at other addresses; every address sits in one
  table per build, chosen at start by a signature test shared with the installer (`bridge/build_sigs.h`). An
  unknown build keeps co-op off and says so in a message box. A crash guard written against one build's address
  was silently off on the other until a gate ran on both.
- What one player does happens for everyone: a room's scripts run for all players when anyone enters, a boss
  starts when anyone gets near, and a player left behind a closing door is brought along. Pickups give their
  item, ammo or healing to every connected player.
- Item uses reach every player. A room's `OnItemUsed` script ran only in the user's game and the script state it
  changes is never synced, so the level 3 leak puzzle diverged for good. The game that handles the use now sends
  a reliable message (kept by the host for late joiners) and the others run the room's handler with the user's
  puppet as the player.
- Enemy hp that the game changes without a hit (a downed grunt standing up again, a script topping up a turret)
  is sent as an hp-only message; before, a client could see an enemy the host was still fighting as a corpse.
- Flying enemies attached to a bone of an animated scenery model (the level 2 mantas) are taken off the bone on
  clients and placed by the host's snapshots; the engine otherwise moved them back onto the bone every scene
  update, up to 230 units from the host's.
- Doc's leaf blower from a client pushes the host's real enemies through the game's own push volume. Puppets get
  the things a character's own code builds: Max's jetpack and its flame, Doc's blower, Kurt's open parachute.
- Client fixes at the level 3 boss (a blast script with no target, a Lua error at every blast) and at the start
  of level 8 (a client loading later than the host called into a creature the host had already removed).
- Joining without typing an address: a list of hosts found on the local networks, on Tailscale and among recent
  hosts, one paste box for an invite code, link, Steam lobby number or address, and an invite that carries every
  route that works. The lobby says how friends can reach the host and asks the router to open the port (UPnP,
  else NAT-PMP).
- The launcher drawn in the game's own look from the player's `mdk2.tar` (nothing from the game is shipped),
  usable with a pad. In game: a player list (hold Tab), shared co-op keys, a co-op layer on the pad's View button,
  an on-screen keyboard for chat, and a quick guide (hold F1) written from the current bindings.
- One-file `MDK2-Coop-Setup.exe` (unsigned), an in-launcher update check, a drop-in zip for manual installs, a
  Vortex game extension, a Mod Organizer 2 game plugin and `install-linux.sh` for Proton (untested).

## QA

- Background test mode: the games run unfocused, muted, driven by scripted input inside the game, with
  back-buffer screenshots, so the PC stays usable while tests run.
- `tools/qa_all.py full` runs the whole ladder: offline tests, every feature suite with real games,
  lossy-network variants, and the campaign (every checkpoint, 96, with host and client: walk, fire, a client hit,
  the client follows, both alive, enemies bound, hp equal, no Lua errors, state diff).
- Suites check deliberately broken variants (`MUTANT=...`) where practical, so a gate that cannot fail is caught.

Result for v0.4.0-beta.1 (release commit `ecb208f`, 2026-11-10, one Windows 11 PC with both supported builds;
from the release notes' "Verified and not verified" section):

- 109 gates: 41 offline, 68 with running games. One complete run passed 105. Of the other four, the campaign
  sweep, `two-player` and `updater-live` passed when run again alone, and `first-enemy` is intermittent (below).
- Checkpoint sweep: two players at all 96 checkpoints, 96 of 96 followed and matching, with levels 1 to 3 (to
  checkpoint 3-6) on the Steam build and the rest on the Overhaul build.
- Of the 68 gates with games, 49 ran on Overhaul copies only, 15 on Steam copies only and 4 on both. Two commits
  before the release commit every gate with games ran on Steam copies in one pass.
- Intermittent: `first-enemy` failed 3 of 5 runs on the release commit, always the same check (the level 1 dog's
  first bite on the host took longer than the test's 20 seconds). Not diagnosed. On the Steam build
  `remote-events` and `projectiles` each failed one run in three two commits earlier and passed in the final
  run. Not diagnosed.
- With 15 or more games running at once, tests with timing limits fail in scattered places; the numbers above
  are from an otherwise idle PC.

Not covered:

- A full campaign played through in co-op, and boss fights and puzzles beyond the checkpoint starts.
- A clean install on a PC that never had the mod, and play between two homes on this build.
- Steam invites between two separate accounts and long internet sessions.
- Steam Deck, Proton and Linux (the install script is tested against fixture folders only); Mod Organizer 2
  under its virtual file system.
- The campaign sweep with more than two players.
- Real gamepads, microphones and 3-4 couch players (tested with simulated input).

## How to play

Download `MDK2-Coop-Setup.exe` from the mod's [Releases](https://github.com/TommoDotCommo/mdk2-coop/releases) and
double-click it. It finds the game (Steam libraries or the Overhaul Games folder), says which build it found, and
installs or repairs; an unknown build installs only after a warning that co-op will stay off. Setup is not
code-signed, so Windows SmartScreen warns before the first run. A launcher opens before the game for hosting,
joining, playing solo and every setting. Host over Steam from Options > Co-op > Session (invite friends or share
the lobby ID), or over UDP from the launcher's lobby, which reports how friends can reach the host and gives one
invite to copy. Everyone needs the same version. The zip on the release page is for a manual install (two files
into the game folder) and for mod managers. To build from source, see the mod's `docs/DEVELOPMENT.md`.

## Known issues

- Steam invites between two separate accounts and long internet sessions have not been tested yet.
- No host migration: if the host leaves, the session ends for everyone.
- The host uploads about 1.3 Mbit/s with 8 players in a busy level, less with fewer.
- Some cutscenes started by events inside a level have no countdown until they have been watched once.
- Voice has no echo cancellation and no voice in couch games (one room, one microphone).
- Items made by item spawners run independently on each machine and are matched by name.
- The level 3 airlock's item handler stays local to the player who uses the item.
- Steam Deck and Linux (Proton) are untested.
- Setup is not code-signed.

## Next steps

- A full co-op playthrough by real players, with the problems found turned into automated checks.
- Test Steam invites between two real accounts, a clean install on another PC and a long session over the internet.
- Run down the intermittent `first-enemy` gate.
- Search the other level scripts for the level 8 shape (a client script touching an object the host already
  removed).
