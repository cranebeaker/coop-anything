# Hydroneer (2020)

| | |
|---|---|
| Engine | Unreal Engine 4.26 (`4.26.2-15973114+++UE4+Release-4.26`), PhysX 3, Voxel Plugin Pro, EasyMultiSave |
| Versions tested | Steam app 1106840, build 23218978; `Mining-Win64-Shipping.exe` SHA-256 `DF27B86E4564100FA792464EA625F64C96C4D155C6C911486FE3769C8D5570B1`; Windows 11 and Linux (Proton) |
| Projects | Official split-screen / Remote Play co-op; [CoopNet](https://github.com/Zane11332/hydroneer-coopnet), host/join over Steam (this write-up). Both in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | official mode: community-reports; CoopNet: automated (two real game clients on one PC, bot-driven), plus five two-PC sessions (Windows host, Linux/Proton friend) whose logs drove the fixes |
| Last updated | 2026-10-23 |

## Where things stand

Hydroneer ships co-op only as split screen, played online by streaming the host's screen with Steam
Remote Play Together. CoopNet adds real host/join co-op: each player runs their own copy of the game
on their own PC and screen, the host's world decides, and the connection goes through Steam. The
host plays any world, new or saved; the friend joins at any time and receives the host's world.
Together they can walk, carry, dig and use tools. Money, shops, machines, building, vehicles and
quests are not synced yet. Release 0.3 is an early test build.

## Existing work

| Project | What it is | Source |
|---|---|---|
| Official multiplayer (2.0 update; existed by 2022-08) | Split screen; online via Steam Remote Play Together (the remote player sees a stream of the host's split screen, needs no copy of the game). Developers say they "tried all avenues" for true multiplayer and blame the physics load | [TechRaptor guide](https://techraptor.net/gaming/guides/hydroneer-multiplayer-guide), [2.0 update](https://techraptor.net/gaming/news/hydroneer-20-multiplayer-update-is-out-now), [Steam thread](https://steamcommunity.com/app/1106840/discussions/0/3458220484817654192/) |
| [CoopNet](https://github.com/Zane11332/hydroneer-coopnet) | Host/join co-op mod (UE4SS Lua + Steam relay), this write-up. MIT | GitHub |
| [Hydroneer-P2-Keyboard](https://github.com/codeopen0-cpu/Hydroneer-P2-Keyboard) | Keyboard and mouse for split-screen player 2 (pushed 2026-06-07). Not host/join | GitHub |
| [HydroneerAPMod](https://github.com/Thedragon005/HydroneerAPMod) | Archipelago randomizer: item sync only (pushed 2026-09-26). Not host/join | GitHub |
| Modding base | HML (Hydroneer Mod Loader) for Workshop mods; [Hydroneer-Modkit](https://github.com/Buckminsterfullerene02/Hydroneer-Modkit) and [Hydroneer-Template](https://github.com/Buckminsterfullerene02/Hydroneer-Template) (UE 4.26 editor projects) | GitHub, [Steam](https://steamcommunity.com/app/1106840/discussions/0/3729575504107129027) |

Players report up to 2 or up to 4 local players on PC; sources disagree
([Steam thread](https://steamcommunity.com/app/1106840/discussions/0/6756004301608937453)).

## How the game works (for co-op)

- **Framework:** `GM_Hydroneer_C` (game mode), `BP_GameController_C`, `BP_Character_HydroGuy_C`,
  game C++ module `/Script/Mining`. The main menu runs inside the world level `L_GameWorld`.
- **Netcode:** `GameNetDriver` = `IpNetDriver` is configured, but no game Blueprint has a replicated
  property or RPC. Engine replication alone syncs nothing.
- **Players:** `GM_Hydroneer_C:AddLocalPlayer()` adds a full second player.
  `GameplayStatics:SetForceDisableSplitscreen(true)` hides its view on the host; it keeps working.
  This is the slot the remote player drives.
- **Input:** movement is `BP_GameCharacter_C:InpAxisEvt_MoveForward_*` / `MoveRight_*`; actions are
  generated `InpActEvt_<Action>_*` pairs on the controller (lower index = key down: Pickup 12/13,
  LMB 5/6). Calling them runs the game's own logic for that player, no screen needed.
- **Items:** `BP_ParentItem_C` with `BeingCarriedBy` and `ProxyItem` (the held copy). Tool use is
  `PlayPrimaryUseAnim` / `PlaySecondaryUseAnim`, which call `AC_Animator:PlayAnimation(seconds,
  mesh, move curve, rotation curve)`. Item state lives in plain fields and mesh visibility (bucket:
  `HasLiquidType?`, `DirtLoads`, `IceLoads`, `WaterMesh`, `DirtMesh`; shovel: `Dirt`).
- **Digging:** `BP_GameController_C:MoveDirt(PickUp?, Radius)` ends in Voxel plugin edits: a scoop is
  `VoxelSphereTools:RemoveSphere(world, pos, 30, true x4)`, a dump `AddSphere(world, pos, 28, ...)`,
  on voxel worlds such as `BP_HydroVoxelWorldEmberCradle`.
- **Item names:** a new game spawns its starter tools at runtime with game-made names
  (`BP_ShovelT1_C_2147479685`) that differ between two PCs; level-placed items have fixed names.
- **Sounds:** blueprints play them through `GameplayStatics` (`PlaySoundAtLocation`, `PlaySound2D`,
  `SpawnSoundAttached`, ...); footsteps come from the walk animation.
- **Saves and settings:** EasyMultiSave slots (`Level.sav` ~21 MB, mostly voxel data, plus
  `Player.sav`, `SaveData.sav`). `-userdir=` moves saves and engine configs.
- **Trap for testers:** the first launch of every new copy of the game rewrites the real
  `%LOCALAPPDATA%\Mining\Saved\Config\WindowsNoEditor\Settings.ini` once, even with `-userdir`. The
  writer is unknown. Hash the real folder before and after every run.
- **Tools used:** UE4SS experimental build `v3.0.1-1181-g83330635` (SHA-256 of the zip
  `b9a166a756605b21058a64cde93bcaab1a855dba3553e5c7e6ce0fa8fdfad7b9`, matches the GitHub digest).
  It adds `UWorld:SpawnActor` and game-thread timers; the stable 3.0.1 cannot spawn actors from Lua.

## What we changed

CoopNet is a UE4SS Lua mod in both games plus a Python relay per player. UE4SS Lua has no sockets,
so the game and the relay exchange lines through two files; the relay sends them through Steam's
`SteamNetworkingMessages` as Spacewar (app 480) using Steamworks.NET's flat API, found through the
host's Steam lobby (see [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md)).

- **Host authority:** the client's world-changing key mappings are blanked; keys are polled and only
  sent. The host replays them on its hidden player 2, so pickups, drops and digs run only there.
- **Movement:** the client sends move axes and look rotation; the host replays the axes every frame
  and acks the last input line, and the client blends towards host P2 at that line.
- **Host player on the client:** the client's own second local player as a puppet, with the host's
  velocity for the walk animation and the host's yaw set on the puppet's controller (setting it on
  the pawn alone was overwritten every frame).
- **Items:** moved items every tick, all items every ~4.5 s; held proxies skipped. Items made after
  F7 get net ids and are spawned on the client. Items that existed at F7 with game-made names are
  announced and **paired** with the client's own item of that class (or spawned).
- **Item state:** per class, reflection finds number/bool fields and mesh components; the host sends
  changes, the client applies them (also to its held proxy).
- **Terrain:** the host hooks the Voxel sphere tools and sends each call; the client makes the same
  call on the same voxel world, whoever dug.
- **Animations and sounds:** the host sends `AC_Animator:PlayAnimation` calls and player-related
  `GameplayStatics` sounds (attributed by object or by a recent nearby action, footsteps and the
  client's own jump excluded, repeats within 0.4 s dropped); the client plays them.
- **Session and version:** inboxes skip what was written before F7, a session id per host start,
  and each side checks the other's CoopNet version.
- **Late join (0.3):** the friend presses F7 at the main menu; the host autosaves and its relay
  sends the slot's files (~21 MB, acked 48 KB chunks, SHA-256); the friend's game loads them as
  slot `CoopNetHost` through the save manager's own calls. Terrain edits after the save are
  repeated; items keep the host's names (net ids map onto them).
- **Players' packaging:** one launcher window on Windows, a zenity or text menu on Linux,
  install/uninstall that back up saves and remove exactly what they added.

Each fix, its evidence and the failed attempts are in [NOTES.md](NOTES.md).

## QA

Harness: two copies on one PC, saves redirected, slots from [tools/gameslot](../../tools/gameslot/README.md),
the owner's real save folder hashed before and after every run (and the run refuses to start while
the owner's own game is open). Bots in both games walk, pick up, carry, dig, dump, drop and carry a
runtime item; the host spawns the starter items and destroys one. The client's starter items are
remade under new names (`-RenameClientItems`) so pairing is tested. Real key presses go to the
client window only after a foreground check. At the end both games dump every item and a script
compares them.

Last run, 2026-10-22 (fix 5, Steam build 23218978, UE4SS v3.0.1-1181, mock Steam transport):

| Check | Result |
|---|---|
| Items, host vs client | 564 vs 564, all matched, max error 0.0 cm |
| Starter items with different names | 6 of 6 paired, 0 cm apart |
| Digging and dumping | ground equal on both sides (145.7 / 194.5), replayed `RemoveSphere` / `AddSphere` |
| Tool animations | client ran `PlayAnimation` with the dig and empty curves |
| Sounds | 12 sent, 11 played (the skipped one is the host's own 2D sound) |
| Version check | fix5 / fix5 on both sides |
| Client player vs host P2 | worst correction 29 cm (bot walk) |
| Transport | 0 lines lost |
| Host work per tick | avg 1.69 ms, max 14 ms, 114 of 2,747 ticks over 4 ms |

Late join, 2026-10-23 (0.3, `-LateJoin`: the host spawns items and digs a hole with the game's own
shovel before the friend joins):

| Check | Result |
|---|---|
| Save transfer (relay only, real 21.9 MB save) | 3.3 s, byte-identical; with 10% of messages dropped 16.7 to 19.8 s, byte-identical |
| Items after the late join | 572 vs 572, all matched, max 7.3 cm (a shovel just dropped) |
| Hole dug before the save | ground 147.1 on both sides |
| Digs after the join | equal (145.7 / 194.5) |
| Normal co-op after the change | 572/572 within 0.0 cm, 6/6 starter items paired |

Real sessions (Windows host, Linux friend on Proton, Steam relay; latency not measured, the relay's
one-way figure compares two PCs' clocks): five so far.
Each one's logs led to a fix (Wine read bug, starter items, terrain, turning, item state,
animations, sounds). Not yet confirmed in a real session: the 0.2 animations and sounds on the
friend's side.

## How to play

Download the player package from the
[CoopNet releases](https://github.com/Zane11332/hydroneer-coopnet/releases/latest). Windows:
`Start Hydroneer Co-op.bat`, Install, then Host or Join. Linux: `bash hydroneer-coop.sh`, plus the
Steam launch option it prints. The host loads a world and presses F7 in it; the friend presses F7
at the main menu and is sent the host's world.

## Known issues

- Not synced: money, shops, buying and selling, machines, building, vehicles, quests, saves, late
  join. Play a new, throwaway world.
- On the friend's side, tool animations and sounds are new in 0.2 and not yet confirmed in a real
  session; whether a bucket's water mesh shows is not known.
- Late join is tested with two copies on one PC only, not yet between two PCs or under Proton.
- Progress is saved in the host's world only.
- The host's tick work rose with item-state checks and the action log (max 14 ms in the last run).
- Needs the experimental UE4SS build.
- The first launch of a new game copy rewrites the real `Settings.ini` (see the trap above).

## Next steps

1. Confirm 0.2 animations and sounds on Linux; trim the host tick cost (state checks spread over
   more ticks, action log off by default).
2. Confirm late join in a real Windows/Linux session.
3. Money, shops and machines (host-side state the client must mirror).
4. Network chaos tests ([playbook/network-chaos-testing.md](../../playbook/network-chaos-testing.md)).
5. Find the first-launch `Settings.ini` writer.
