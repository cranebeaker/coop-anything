# Fable Anniversary (2014)

| | |
|---|---|
| Engine | Hybrid: the original Fable C++ engine (simulation, AI, quests) inside Unreal Engine 3 (rendering, UI) |
| Versions tested | Steam, AppID 288470, build id 434572, exe FileVersion 1.0.8767.0, SHA-256 `2a95eea3c2cce9b47ca0f454a605b6952216f5d25158efd12ba48b70130989f2` |
| Projects | fable-coop (dinput8.dll proxy, MIT), https://github.com/LenoreSlate/fable-coop; claim issue [#280](https://github.com/cranebeaker/coop-anything/issues/280) |
| QA level | none (two copies on one PC, checked by the owner's eyes and by memory reads through the bridge; no bot harness yet) |
| Last updated | 2026-11-02 |

## Where things stand

Not playable together yet. Two copies of the game, linked over UDP, each show the other player's hero
walking, running, fighting and turning in the same region, with matching animations and a normal face, and
share the time of day. Nothing else is shared: no enemies, quests, items or day count. Only tested with both
copies on one PC; the planned online transport (Steam relay) is not written yet.

## Existing work

None found on 2026-10-31: no directory entry, no GitHub issue or PR for "fable", nothing on the
[Steam discussions](https://steamcommunity.com/app/288470/discussions/0/613936039452164692/) or
[ModDB](https://www.moddb.com/games/fable-anniversary/mods). The official modding kit covers art and
animation only ([PC Gamer](https://www.pcgamer.com/uk/fable-anniversary-will-support-modding-with-included-unreal-engine-3-editor)).

## How the game works (for co-op)

- **Two engines in one exe.** The original Fable engine runs the world (`CThing`, `CThingPlayerCreature`,
  `CWorld`, quest scripts as `NScript::CQ_*Script` coroutines); UE3 renders it through `HeroPawn` /
  `AIPawn` actors. `WellingtonGame.u` is small: game logic is native, so UE3 replication does not help.
- **No netcode.** `CNetworkClient` / `CNetworkServer` are an in-process per-tick message pipe, not sockets.
- **Hook point.** The exe imports one function from `DINPUT8.dll`; a 32-bit proxy loads at startup.
  ASLR is on, so the mod works from offsets to the exe base.
- **Scripting interface.** Quest scripts call a 942-slot `CGameScriptInterface` (global at 0x35bbc34,
  preferred base 0x400000): create creature, take scripted control, move to, remove a thing. The mod calls the
  same slots from the per-frame update (`CMainGameComponent` vftable slot 5, patched).
- **Positions.** `CTCPhysicsControlled` (hero) and `CTCPhysicsNavigator` (NPCs): position at +0x0c, facing
  at +0x5c. The navigator's teleport slot (0x90) places a creature without fighting its own movement.
- **Region.** `CWorldMap` (global 0x3630374) +0xbc = current map index (Oakvale 4, Heroes' Guild 44); a region
  load removes scripted creatures.
- **Pause in the background.** Fable pauses the world when its window is not in front (its own code, not the
  UE3 `bPauseOnLossOfFocus`); two jumps in 0x1f9f0a0 are patched.
- **Saves.** One file under `%USERPROFILE%\Documents\My Games\FableHD\Saves`, synced by Steam Cloud. The
  proxy redirects Documents to `<game dir>\CoopProfiles\<profile>` so test copies never touch the real save.
- **Menus are Scaleform.** The main menu is a GFx 4 movie (FrontEnd, in `Region_GameFrontEnd.umap`); the exe
  supports AS2 and AS3, `attachMovie` and loading SWF files, so a Coop entry can be added at runtime.
- **UE3 reflection works over a memory bridge.** GNames at 0x35071e8; UObject +0x2c name, +0x34 class;
  UProperty +0x60 offset. Enough to read anim trees and nodes by name from a script.

Every address, the method and the dead ends are in [NOTES.md](NOTES.md).

## What we changed

- **Two copies on one PC.** The game quits if mutex `UnrealEngine3_8` exists; the proxy renames it per process.
- **Separate saves.** `SHGetFolderPathW(CSIDL_PERSONAL)` answers a per-profile folder.
- **Runs in the background.** The focus pause is patched out, so the copy that is not in front keeps simulating.
- **Player 2 puppet.** On the first packet the proxy spawns `CREATURE_YOUNG_HERO` (the cutscene double of the
  child hero; a second `CREATURE_HERO_CHILD` freezes the game) under scripted control, empties its action
  queue and places it each frame where the remote hero was 0.1 s ago (interpolated). It is removed when the
  regions differ or the peer is silent for 5 s, and respawned when both are back in one region.
- **Face.** The puppet played adult villager anims, which pose the child mesh's face bones (wide-eyed,
  open mouth). A hook on `UAnimNodeSequence::SetAnim` (vtable slot) maps them to hero anims. Owner: normal face.
- **Animation sync.** Each packet carries the UE3 name and FName number of the sender's heaviest
  `AnimNodeSequence`; the receiver sets it on every puppet sequence node. Without the number the combat stance
  matched no sequence and showed a T-pose.
- **Strikes and sharp turns.** Earlier the puppet was walked with the game's move-to orders; it glided on strikes
  (the engine moves the hero 2.4 units per 0.77 s strike while the anim stays in place) and stopped before
  sharp turns. Per-frame placement fixed both.
- **Time of day.** The guest copies the host's `CGameTimeManager` time when it differs.

## QA

No automated suite yet. What ran, all on one PC with two copies (profiles p1 host, p2 guest):

- Anim sync, 2026-11-02: a script read both games through the bridge every 1.5 s for 90 s while the owner played
  p2; 60/60 samples the puppet's nodes played p2's main anim (idle, run, jog, combat idle, strike, kicks, idles).
- Region, 2026-11-01: p2 entered the Heroes' Guild, p1 removed the puppet; p2 reloaded Oakvale, both respawned it.
- Time of day, 2026-11-01: 0.95 written into p1 reached p2 within 1 s.
- Owner's visual checks, 2026-11-02: normal face, combat stance, no slide on strikes, fluid sharp turns.

## How to play

Not ready for players. To try the prototype: build the proxy (MSVC, 32-bit, `proxy/build.bat`), copy
`dinput8.dll` into `<game dir>\Binaries\Win32` of a **copy** of the game, and start each copy with
`FABLE_COOP_PROFILE`, `FABLE_COOP_LISTEN`, `FABLE_COOP_PEER` and `FABLE_COOP_HOST` set (see the repo README).

## Known issues

- Same region only; the puppet looks like the child hero, whatever the other player's age or clothes.
- One anim per packet: an upper-body move over walking legs shows only one of the two. One-shot anims are not
  time-aligned.
- Nothing else is shared yet: enemies, quests, items, money, day count.
- A copy sometimes crashes about 10 s after launch, before the menu (exe 0x62f361); this was seen before any
  co-op code. Relaunching works.

## Next steps

- Online transport: `ISteamNetworking::SendP2PPacket` (SteamNetworking005, in the game's own `steam_api.dll`)
  under the game's AppID, then lobbies (SteamMatchMaking009) for a Coop entry in the main menu.
- Owner's design: free co-op (own camera, roam anywhere). Gold (story) quests done together, silver and bronze
  solo; all quest progress shared. The host's save is the world; the guest keeps their hero, gold and items.
- Quest sync (the card colour's location in the game data is unknown), enemies with per-region ownership,
  an adult look-alike puppet, a bot harness for the QA level.
