# Hogwarts Legacy (2023)

| | |
|---|---|
| Engine | Unreal Engine 4 (project `Phoenix`; exact 4.x not confirmed from the binary), Denuvo-protected exe, UE4SS 3.0.1 Lua mod + a separate network helper |
| Versions tested | Epic Games Store build 1613387 (exe dated 2025-11), Windows 11; two game copies on one PC, and one real session between two PCs over Tailscale |
| Projects | HL Co-op (this write-up; code repo link to come), directory entry "HL Co-op"; other projects below |
| QA level | automated |
| Last updated | 2026-10-28 |

## Where things stand

Two to eight players can roam one world, see each other, fight the same enemies and, after an invite, play the
host's story together: a joiner takes the host's quests and world in about 5 s with no load screen, keeps their own
character, and gets their own world back when they leave. Only the host moves the story. It is an alpha: tested with
bots on two game copies on one PC and in one real two-PC session; cutscenes and dialogue are not shared.

## Existing work

Found on 2026-10-24, re-checked 2026-10-28. None syncs the story.

- **Wandrealm** ([Nexus 2588](https://www.nexusmods.com/hogwartslegacy/mods/2588)): playable free-roam multiplayer on
  its own servers, Steam and Epic, closed source. No story.
- **HogwartsMP** by MafiaHub ([docs](https://github.com/hogwarts-mp/docs),
  [foundations](https://github.com/hogwarts-mp/foundations)): server-scripted roleplay (players, NPCs, encounters, PvP,
  mounts, Quidditch), closed-source mod with public scripting docs, very active (pushed 2026-10-27). Its docs say
  places built by "a story script a multiplayer session never runs": no story. Server config default 32 players.
- **HogWarp** by the Skyrim Together team ([Nexus 1378](https://www.nexusmods.com/hogwartslegacy/mods/1378)):
  free-roam, abandoned 2023. Its directory entry linked `github.com/hogwarts-mp/mod`, which is MafiaHub's (a different
  project) and now a 404.
- **HogwartsLegacyMP** by Kheartz ([GitHub](https://github.com/Kheartz/HogwartsLegacyMP)): "early development, not
  yet playable", position sync only, last push 2026-06, no license file. Same stack idea as ours (UE4SS + ENet).

## How the game works (for co-op)

- **No usable netcode.** The engine's net drivers are compiled in, but the game is single-player throughout.
  Everything here is a UE4SS Lua mod: game functions are hooked and called by name, nothing is patched in the exe.
- **Lua cannot open sockets** in UE4SS, so networking lives in a small helper process (ENet, UDP) that the mod talks
  to over named pipes: 20 Hz player state, reliable events, outfits, enemy batches, compressed world pushes.
- **All progress is one live SQLite database** (`DbGateway`; the `.sav` holds the same image, Oodle-wrapped). The
  game writes through to it at once but reads it only at load time. The pause menu's `ExecuteLoad(slot)` reloads a
  slot in place in about 1 s with no load screen and the player staying put. That is the story sync: the host
  exports every *world* table as SQL (about 3 MB), the joiner applies it to its own database, writes it over its own
  save slot and loads in place. Personal tables (inventory, gear, talents, appearance, beasts, Room of Requirement,
  XP) stay the joiner's. The in-place load drops some mission rows and zeroes a few bytes of one table, so the world
  rows are re-applied after it; a store hash of all world tables is compared on both sides.
- **Players are puppets**: a `SkeletalMeshActor` dressed from the remote player's skeletal mesh parts and materials
  (dynamic material parameters included), animated with the game's own locomotion blend spaces. A puppet is no spell
  target; for PvP an invisible enemy NPC frozen with `CustomTimeDilation = 0` sits on it (a spawned NPC never stays
  AI-suspended and fights back otherwise).
- **Enemies**: each one is run by the nearest player; the others mirror position, health and death. The host keeps
  enemies near other players awake (the game shuts AI down far from the player, and a shut-down enemy takes no damage).
- **Mounts**: on a broom the player stays the pawn, its mesh attached to the broom's mesh; on a creature (Hippogriff,
  Graphorn, Thestral) the controller possesses the creature and the player is its rider
  (`GetMountComponent():GetMountHandler().RiderCharacter`).
- **Cutscenes** are SceneRigs (`ASceneRigActor`); missions live in `MissionManager` and the database.
- **Traps**: Lua holding actors across a map load crashed copies (drop every reference in a LoadMap pre-hook); UE4SS's
  async thread must never run game calls; `IsValid()` says true on a freed address another object reused.

## What we changed

Each item has an exit test that drives real game copies with bots (key messages to the window, a file bridge for
Lua calls) and prints PASS only if every step ran.

- **Puppets** with the other player's outfit and gait; 3 players measured within 75 cm (`run_suite.py --game`).
- **Shared enemies**: ownership by distance, handoffs, hit and kill claims, deaths, loot per player.
- **Story by checkpoint** (above): join sync and every host quest change; natural quest step on the host reaches the
  joiner (`quest_check.py`); store hashes equal (`store_hash.py`); a late third player (`late_join_check.py`).
- **Free roam by default, story by invite** (host F6, joiner F7, F7 again leaves and restores the joiner's own world;
  `free_roam_check.py`). Time of day follows the host.
- **Level transitions**: joiners follow the host's fast travel, dungeons and the Room of Requirement
  (`transition_check.py`, 15 trips with equal hashes).
- **Unreliable links**: 400 ms, 2 % loss relay (`chaos_check.py`); a killed client is dropped within 5 s.
- **PvP**, off by default, F9 toggles it for everyone; hits on the stand-in are applied to the victim with the game's
  own damage call (`pvp_check.py`).
- **Brooms and creature mounts** shown on puppets at the right height with the game's riding animations
  (`flying_check.py`, partial: see Known issues).
- **Only the host moves the story**: a joiner's own main-mission step inside the host's story is undone by asking the
  host's world back; cutscenes play where they are set off (`cutscene_check.py`).
- **Saves are safe**: co-op runs on its own save folder; a guard refuses to load synced worlds anywhere else.

## QA

- Two game copies on one PC from fresh seeds of a real save, each test starting the session, autoloading, driving
  bots and comparing logs, database rows and store hashes; owner saves are hashed before and after every run.
- Last results (2026-10-28): suite `run_suite.py --game` 26/26, `free_roam_check.py` 23/23, `transition_check.py`
  25/25, `pvp_check.py` 17/17, `cutscene_check.py` 12/12; `flying_check.py` every check passed in some run, best
  single run 14/15.
- One real session between two PCs (Tailscale, 2026-10-27): the friend's join worked after fixes for other outfits
  and the time of day; one sync result from the friend's side was never confirmed.

## How to play

A friend kit (install script, the helper, a vetted UE4SS loader, the mod) exists; release link to come. Each player
installs it into their game folder; the host starts a hosting launcher, friends join by the host's address (LAN or a
VPN such as Tailscale). Epic players need the game's `-saveddirsuffix` launch option for the co-op save folder.

## Known issues

- Cutscenes and dialogue play only for the player who starts them; the others get the result a few seconds later.
- Your friends' spells are not shown on your screen; enemies do not flinch on the other screens; kill XP is per player.
- A mount appears and disappears without mount or landing animations; Thestral and Graphorn untested.
- A player you target in PvP shows an enemy health bar named "Ashwinder Scout".
- Joiners follow the host's fast travel, not travel from the map screen or through doors.
- Moving objects with spells and peaceful animals are not shared yet.

## Next steps

A second two-PC session with the newest kit; then remote spell visuals and hit reactions, objects moved by spells,
peaceful animals; a story cutscene a bot can reach for the cutscene test; a full PASS of `flying_check.py` at a quiet
spot where mounts work.
