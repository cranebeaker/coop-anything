# MDK2 HD (2011)

| | |
|---|---|
| Engine | BioWare in-house (MDK2), OpenGL 3.0 renderer, Lua 3.1 scripting with tolua bindings |
| Versions tested | Steam app 213350, build id 3902, `mdk2hd.exe` SHA-256 `bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43` |
| Projects | [mdk2-coop](https://github.com/TommoDotCommo/mdk2-coop) (directory entry "MDK2 HD") |
| QA level | automated |
| Last updated | 2026-10-23 |

## Where things stand

MDK2 HD has no multiplayer. mdk2-coop adds drop-in co-op: two or more players move through the campaign
together, with the host's world (enemies, shots, pickups, doors, checkpoints, level loads, cutscenes) mirrored on
every client. The public alpha [v0.2.0-alpha](https://github.com/TommoDotCommo/mdk2-coop/releases/tag/v0.2.0-alpha)
(2026-10-23, a pre-release with an installer and a launcher) passed the full automated ladder first: 31 gates,
including all 96 checkpoints with host and client. Nobody has finished the whole campaign in co-op yet, and the
automated tests do not solve puzzles or beat bosses.

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

## QA

- Background test mode: the games run unfocused, muted, driven by scripted input inside the game, with
  back-buffer screenshots, so the PC stays usable while tests run.
- `tools/qa_all.py full` runs the whole ladder: offline tests (build, oracles, projectile, controller, revive and
  network cores), every feature suite with real games, lossy-network variants, and the campaign (every checkpoint,
  96, with host and client: walk, fire, a client hit, the client follows, both alive, enemies bound, hp equal, no
  Lua errors, state diff). `--lanes 3` runs feature suites side by side on separate game copies;
  `--campaign-shards 4` splits the campaign over four host/client pairs.
- Before v0.2.0-alpha (2026-10-23): 30 of 31 gates passed, the campaign in 4 minutes. The failure was the stuck
  hero above, a real bug that showed up only when the PC was busy; after the fix the character-wheel gate passed
  12/12, and both the old code and a mutant without unsticking fail it.
- Suites check deliberately broken variants (`MUTANT=...`) where practical, so a gate that cannot fail is caught.
- Not covered: a full campaign played through in co-op, boss fights and puzzles beyond the checkpoint starts,
  long sessions over the internet, Steam invites between two separate accounts, and real gamepads, microphones and
  3-4 couch players (tested with simulated input).

## How to play

Download the zip from the mod's [Releases](https://github.com/TommoDotCommo/mdk2-coop/releases), unzip it and run
`Install.cmd`. It finds the Steam install, refuses any build other than Steam 3902, and asks Steam or UDP and host
or join; `Uninstall.cmd` removes it. Host over Steam from Options > Co-op > Session (invite friends or share the
lobby ID); over UDP the host forwards port 27900. A launcher opens before the game for hosting,
joining, playing solo and every setting; `Couch.cmd` starts couch co-op. To build from source, see the mod's
`docs/DEVELOPMENT.md` (`bridge/` with mingw i686, `tools/install.sh`, `tools/release.sh`).

## Known issues

- Steam invites between two separate accounts and long internet sessions have not been tested yet.
- No host migration: if the host leaves, the session ends for everyone.
- The host uploads about 1.3 Mbit/s with 8 players in a busy level, less with fewer.
- Some cutscenes started by events inside a level have no countdown until they have been watched once.
- Voice has no echo cancellation and no voice in couch games (one room, one microphone).
- Items made by item spawners run independently on each machine and are matched by name.

## Next steps

- A full co-op playthrough by real players, with the problems found turned into automated checks.
- Test Steam invites between two real accounts and a long session over the internet.
- Record routes for the replay bot (the machinery exists; no campaign routes are bundled) to cover what the
  checkpoint sweep cannot: puzzles and boss fights.
