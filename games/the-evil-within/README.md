# The Evil Within (2014)

| | |
|---|---|
| Engine | id Tech 5 fork (Tango "STEM"), x64, SteamStub-wrapped |
| Versions tested | Steam build 2708258, EvilWithin.exe SHA-256 743df7a126a66470dbfc27b8c30348eea7d29bf948b82705da6e7babc6e67437 |
| Projects | [evil-within-coop](https://github.com/kennedypoopson/evil-within-coop) (directory entry: EvilWithThem) |
| QA level | none (hand-tested, two game copies on one PC) |
| Last updated | 2026-11-09 |

## Where things stand

Two players can see each other move, aim, sneak and switch weapons, each in their own copy of the level. Enemy
damage is replayed between games (first version). Enemy AI, world state and story progression are not shared yet.

## Existing work

None found before this project (GitHub, directory, issues/PRs searched 2026-11-09). OAleex/The-Evil-Within-Tools is
an asset/file-format tool, not co-op.

## How the game works (for co-op)

- The exe still contains id's BFG-era netcode (idLobby, idSession, Steam lobbies, snapshot deltas, usercmds), but
  game logic is hard-wired to one player: GetPlayer(i) returns null for i != 0, SetupPlayer clamps the index to 0,
  idPlayer::Spawn errors on entityNum >= MAX_PLAYERS (1).
- Code is encrypted on disk (SteamStub); analysis used a memory dump of the running game. The console `spawn`
  command is stubbed in retail, but idGameLocal::SpawnEntityDef works.
- A player's position authority is the actor motion object (player+1720); physics is overwritten from it each frame.
- Input: idUserCmdMgr (cmdBuffer[128]); RunFrame feeds player 0 only via 0x88F440. idPlayer::IsLocalPlayer (vtable
  slot 228) gates the controller input step (weapons, sneak). Many systems call GetPlayer(0) instead of `this`.
- Third-person body facing is separate from the camera (motion object quaternion at +352).
- Keyboard/mouse via DirectInput in foreground mode; weapon quick-select acts on the LOCAL player.
- Characters take damage through vtable slot 80 (168 actor classes); entity names (entity+8) match across copies.
- Full address table: include/game.h in the project repo.

## What we changed

- Spawn a second idPlayer with the MAX_PLAYERS check and per-player record write patched only during that spawn.
- Place it through the motion object; feed it usercmds; IsLocalPlayer true for it; no friendly fire.
- Netcode v3: pos/vel, view angles, body facing, input, weapon name; 100 ms snapshot interpolation.
- Weapon-select buttons stripped from remote input (they switched the local player's weapon).
- Two copies on one PC: release the "The Evil Within" mutex, launch with SteamAppId/SteamGameId env vars; swallow
  focus-loss messages (no pause) and block DirectInput reads in the unfocused copy.
- Enemy damage replay: send target name, damage decl, dmgInfo, dir, location (pointers scrubbed); receiver calls
  Damage with player 2 as attacker.

## QA

No automated suite yet. Manual: two copies on one PC, host/join over 127.0.0.1, movement/facing/weapons verified,
damage replay seen applied on the other copy.

## How to play

See the project README and HOW_TO_PLAY.txt (inject.exe, Ctrl+Shift+K host, Ctrl+Shift+O join).

## Known issues

Enemies move independently in each game; doors/items/checkpoints not shared; one unexplained R6025 pure virtual call
(handler now logs its stack); player 2 looks like Sebastian.

## Next steps

Host-authoritative enemy AI sync, reliable event channel with acks, world state and checkpoint/chapter sync,
automated two-copy QA, Joseph model for player 2.
