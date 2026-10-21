# MDK2 HD (2011)

| | |
|---|---|
| Engine | BioWare in-house (MDK2), OpenGL 3.0 renderer, Lua 3.1 scripting with tolua bindings |
| Versions tested | Steam app 213350, build id 3902, `mdk2hd.exe` SHA-256 `bf5faa81b18837b4c07f35941b107e5c23caa8b105536ccd9c8e5a8e37357f43` |
| Projects | [mdk2-coop](https://github.com/TommoDotCommo/mdk2-coop) (directory entry "MDK2 HD") |
| QA level | automated |
| Last updated | 2026-10-22 |

## Where things stand

MDK2 HD has no multiplayer. mdk2-coop adds drop-in co-op: two or more players move through the campaign
together, with the host's world (enemies, shots, pickups, doors, checkpoints, level loads) mirrored on every
client. All 96 checkpoints passed an automated two-client sweep on commit 129b7e3; that sweep ran before the N-player
change and has not been re-run since. It is alpha: a player build exists
([v0.1.0-alpha](https://github.com/TommoDotCommo/mdk2-coop/releases/tag/v0.1.0-alpha), a pre-release with an
installer), but nobody has finished the whole campaign in co-op, and cutscenes and boss fights are only partly
checked. Since that release, master has gained text chat and pings, proximity voice, a controls page with
presets, difficulty presets, player outlines through walls, an emote hook, couch (split-screen) co-op and a
launcher that opens before the game. These were tested feature by feature (below), not by a new full sweep.

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
- On screen: players panel, name tags and capsule outlines through walls, chat bubbles and pings.
- Text chat (T) and pings (V or middle mouse); proximity voice (push to talk, host-set range).
- Options > Co-op menus in the game's own style: players, session, gameplay, difficulty (Story, Normal, Hard,
  Nightmare, Custom) and a Controls page with Original, Modern and Custom key presets.
- A launcher before the game (host, join or solo, transport, name, settings, screen mode), an installer that
  checks the Steam build hash, and a release script.
- Couch co-op: 2-4 local players, one window and gamepad each, by running several game copies.
- Removed: an AI texture upscaler (it made the game darker and flatter than the original).

## QA

- Background test mode: the games run unfocused, muted, driven by scripted input inside the game, with
  back-buffer screenshots, so the PC stays usable while tests run.
- `tools/qa.sh`: two real clients, feature checks on both logs plus host/client state diffs. 30/30.
- Projectile checks: `make -C bridge test-shots` passed capture/replay cases against a fake engine;
  `MDK2_N=3 tools/qa_shots.sh` passed all six source/peer flight checks for type 417, including
  client-to-client relay, exactly one launch each and no local damage from foreign copies (2026-10-21).
- `tools/qa_n.sh`: 38/38 with 3 players, 53/53 with 8. `tools/qa_save.sh`: 31/31 with 3, 41/41 with 8.
- `tools/sweep.sh`: every checkpoint (96) with host and client: walk, fire, a client hit, then client followed,
  both alive, enemies bound, hp equal, no Lua errors, state diff. 96/96 clean on 2026-10-21 (commit 129b7e3,
  before the N-player change; not re-run since).
- Feature suites added since (`tools/qa_revive.sh`, `qa_hud.sh`, `qa_chat.sh`, `qa_controls.sh`, `qa_menu.sh`,
  `qa_voice.sh`, `qa_launcher.sh` with 23 checks, `tools/test_network.sh` and the replay bot's tests) ran during
  development with screenshots of each page. No pass counts for the merged master are recorded here: the
  combined suite list and the 96-checkpoint sweep have not been re-run on it.
- Not covered: a full campaign played through in co-op, boss fights and cutscenes beyond the checkpoint starts,
  play over the internet, and Steam invites between two real accounts (the Steam path was checked against a
  simulated Steam and by creating and leaving real lobbies; same-PC tests use UDP).

## How to play

Download the zip from the mod's [Releases](https://github.com/TommoDotCommo/mdk2-coop/releases), unzip it and run
`Install.cmd`. It finds the Steam install, refuses any build other than Steam 3902, and asks Steam or UDP and host
or join; `Uninstall.cmd` removes it. Host over Steam from Options > Co-op > Session (invite friends or share the
lobby ID); over UDP the host forwards port 27900. To build from source, see the mod's README (`bridge/` with
mingw i686, `tools/install.sh`, `tools/release.sh`). Note that the v0.1.0-alpha zip predates the newer features
listed above; they are on master only.

## Known issues

- The only release (v0.1.0-alpha) predates chat, voice, couch co-op, the launcher and the controls page; build master to get them.
- Host upload is high with many players (about 20 Mbit/s with 8 in a busy level): fine on a LAN, not yet trimmed
  for internet play.
- Voice has no echo cancellation and no voice in couch games (one room, one microphone).
- Outlines are a fixed-size capsule, not the character's real silhouette.
- Chat and the Controls page were only exercised by the harness, not played by hand in a focused game.
- Items made by item spawners run independently on each machine and are matched by name.

## Next steps

- Re-run the full sweep and all suites on current master, cut a new release, then a full co-op playthrough with
  known issues written up.
- Trim enemy snapshots for internet play; test Steam invites between two real accounts.
- Record routes for the replay bot (the machinery exists; no campaign routes are bundled) to cover what the
  checkpoint sweep cannot.
