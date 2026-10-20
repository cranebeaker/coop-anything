# Engine families: reuse co-op work across games

Before designing anything, check what has been done on the same engine. A framework built
for one game is often most of the work for its siblings, and its bug list tells you where
the engine fights back. Projects named here are in [DIRECTORY.md](../DIRECTORY.md) with
sources; this page is about what carries over.

## Families with a reusable base

| Engine family | Games with co-op work | What carries over |
|---|---|---|
| **Creation Engine / Gamebryo** (Bethesda) | Skyrim (Skyrim Together Reborn, SkyMP), Fallout 4 (Fallout Together, unfinished), Fallout New Vegas (NV:MP), Oblivion (old attempts), Starfield (Creation Engine 2: Starfield Together, PULSAR) | Tilted Online's architecture (TiltedEvolution) was built to span games; Fallout Together lives on a branch of it. Script extenders (SKSE, F4SE, xNVSE) give a supported hook point. Quest state is the common missing piece: most projects sync players and NPCs, not quests. |
| **Unreal Engine 1** | Deus Ex (HX), Unreal (coop, Return to Na Pali coop) | The engine has replication and most games shipped UnrealScript, so co-op is script work. Fixes can ship as a separate package that subclasses and swaps classes. Packet emulation (`PktLag`, `PktLoss`) is built in for testing. Candidates: other UE1 single-player games. |
| **Unreal Engine 2 / 2.5** | Splinter Cell Chaos Theory (official), SWAT 4 (official), Republic Commando (mods), BioShock (BioShock Together) | Same replication model as UE1. |
| **Unreal Engine 4 / 5** | script-modding base UE4SS (used across UE4 games, and for the Poppy Playtime attempt in [games/poppy-playtime](../games/poppy-playtime/NOTES.md)) | UE4SS gives Lua/C++ hooks and console access without a compiler, so co-op is an in-process mod, not a fork. Three traps that each cost test runs on Poppy Playtime: (a) a `summon`-ed actor starts at scale 1 while the player's mesh component can carry a large scale (14.37 there), so a spawned puppet looks absent rather than small — copy the source component's scale; (b) Blueprint animation graphs are often procedural and cast to the owning pawn, so they cannot go on a bare puppet — use a `UAnimSingleNodeInstance` sequence on the same skeleton; (c) every UObject read/write must run on the game thread (`ExecuteInGameThread`) — off-thread mutation crashes the game, and a blocking call there freezes the mod's tick loop with no error logged at all. |
| **X-Ray** (STALKER) | Shadow of Chernobyl (Shadow of COOP), Clear Sky (Clear COOP), Call of Pripyat, Anomaly | The xrMPE multiplayer extension hosts all three games' co-op as add-ons. Scripts are Lua plus LTX configs inside `.db` archives; `.dscript` files extend base scripts without forking them. |
| **Source / GoldSrc** | Half-Life (Sven Co-op), Half-Life 2 (Synergy, Obsidian Conflict), Dark Messiah, Vampire Bloodlines (VTMBMP) | Server/client split and the SDK make single-player maps playable in multiplayer; the work is mostly entity and map scripting fixes. |
| **RenderWare** (GTA III era) | GTA San Andreas (CoopAndreas, SACoop, GTA SA Open Co-Op, Player 2 CLEO scripts; SA-MP and MTA are free-roam only), GTA Vice City (VC-MP, free-roam only) | Mods load as ASI plugins and CLEO scripts, and [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) covers GTA III, Vice City and San Andreas. CoopAndreas is the reference for story co-op on this engine (players, weapons, vehicles, mission markers). `detect_engine` recognises the family from the `.txd` header (also seen in Manhunt). |
| **id Tech 2 to 4** | Quake, Quake II, Doom 3 (OpenCoop, LibreCoop on dhewm3), Quake 4 | Open source releases (id Tech up to 4) and source ports (dhewm3) put co-op in reach. |
| **CryEngine** | Far Cry (Assault Coop), Crysis (two projects), Prey 2017, Kingdom Come Deliverance 1 and 2; Far Cry 2 (Dunia, a CryEngine 1 derivative: research prototype in [games/far-cry-2](../games/far-cry-2/README.md)) | Mod SDKs and Lua game logic. Dunia has no SDK: its Lua 4.0 runtime, game modes (`gamemodesconfig.xml`) and pawn replication classes can be reached from a `dinput8.dll` proxy; the archives unpack with Gibbed.Dunia. |
| **RE Engine** | Resident Evil 2, 4 remakes, Dragon's Dogma 2 | Shared tooling (REFramework) across the family; RE Village is still a gap. |
| **REDengine 3 / 4** | The Witcher 3 (Witcher Online), Cyberpunk 2077 (CyberpunkMP) | The Witcher 3 ships its script source; annotations (`@wrapMethod`, `@addMethod`) and WitcherNativeBridge (C++ natives callable from script) cover most hooking. Patches change script APIs and native string encoding (5.0 Remastered: UTF-8 strings, CName areas, new Gwent natives), so check a mod against the exact build. |
| **FromSoftware** | Elden Ring, Dark Souls Remastered, Dark Souls III, Nightreign (Seamless Co-op), Sekiro | Seamless Co-op covers several games of the family; its internals are not public, see [how co-op mods work](how-coop-mods-work.md). |
| **Genome** (Piranha Bytes) | Gothic 3 (G3 Together, Myrtana Accord, Gothic 3 Heroes, Sative's Gothic 3 Multiplayer) | The unofficial [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) (GPL) gives C++ headers and import libraries; plugins are native DLLs in `scripts/` with a `ScriptInit` export. Game logic runs through named scripts whose function pointers a plugin can wrap (`AddHitPoints` for hit authority, `GetAttitude` so NPCs treat a remote player like the hero). The engine only processes NPCs near the camera, so split players need regional ownership. G3 Together (built from zero, ENet direct IP, source: [g3-together](https://github.com/GGM1k3y23/g3-together)) syncs players, NPCs, combat, story, items and saves; the earlier projects share quest state at most or could not be tested. See [games/gothic-3](../games/gothic-3/README.md). |
| **Unity / XNA** | Subnautica (Nitrox), Hollow Knight (HKMP), Outer Wilds (QSB), Celeste (CelesteNet) | C# mod loaders (BepInEx, MelonLoader, Everest) make Unity games the easiest modern targets. |
| **SAGE** (EA Los Angeles RTS) | C&C3 Tiberium Wars (campaign co-op prototype, games/command-and-conquer-3); Generals Zero Hour source is public (GPL, EA) | Lockstep with a lobby; campaign maps can run inside a lobby game if the side stripping (`prepareForMP_or_Skirmish`) and slot sides are skipped. The Generals source names most C&C3/KW/RA3 seams. Watch the per-slot anti-spoofing check when two humans share one side. |
| **ScrapEngine** (MercurySteam) | Clive Barker's Jericho (Jericho Co-op, [games/clive-barkers-jericho](../games/clive-barkers-jericho/README.md)); Scrapland had official multiplayer | Gameplay is Lua 5.1 (`.lc` = u32 header + zlib bytecode, decompiles with unluac); `-InitLuaLine:` runs your Lua at startup without touching game files, and `OnRenderDebug` gives a per-frame hook. The exe keeps Scrapland's UDP client/server and a Lua `Net` library, but its join packet carries Scrapland ship data; a proxy DLL can repoint unused `Net` entries to your own transport. |
| **IW engine** (Call of Duty, id Tech 3 derived) | Modern Warfare 2 (2009): campaign co-op alpha [mw2_coop](https://github.com/stxlvn/mw2_coop), [games/modern-warfare-2](../games/modern-warfare-2/README.md); Modern Warfare 3 (2011): from-zero attempt, [games/modern-warfare-3](../games/modern-warfare-3/README.md); Spec Ops (MW2, MW3) is official 2-player co-op | The MW2 singleplayer exe carries a lockstep 2-player co-op netcode (Spec Ops), and a campaign map loads as a co-op map, so campaign co-op is script work on top: turn off the Spec Ops branches (`is_specialop()`), give player 2 the scripted moments. Scripts are zlib text inside the zones; a `winmm.dll` proxy can replace the script loader to patch them. In co-op, script `getdvar` only sees flagged dvars and retail script errors are silent. The 2026 Steam updates made MW2 and MW3 64-bit (64-bit zones), which broke every 32-bit mod and tool. Whether MW3's `iw5sp.exe` has the same co-op netcode is unchecked. |
| **Java (JVM)** | The Doors of Trithius (two-player co-op mod, [games/the-doors-of-trithius](../games/the-doors-of-trithius/README.md)) | A `-javaagent` (`java.lang.instrument`) loads the mod into the unmodified game JVM, so no game file changes. The game's public classes and its event bus are the hook points; `javap` from the bundled JDK gives every API fact, and a decompiler (CFR, Vineflower) reads the logic. For a single-player game with no netcode, an impersonation window reuses every rule: the host swaps the game's current-player fields (entity, bag, skills, abilities, field of view) to the guest's, runs the vanilla code for the guest's command, then restores them. Turn-based games also need a clock rule for two players far apart. |

## Rebuilt engines and decompilations

A source port or decompilation turns co-op from reverse engineering into normal
programming. Check for one first.

| Base | Co-op examples | Notes |
|---|---|---|
| OpenMW (Morrowind) | TES3MP | Full quest co-op exists because the engine is open. |
| KeeperFX (Dungeon Keeper) | Archon mode, shared keeper co-op | Lockstep; checksums make a perfect test oracle. |
| Arx Libertatis (Arx Fatalis) | two co-op forks | GPL rewrite. |
| fallout1-ce / fallout2-ce | Fallout 1 and 2 co-op projects | Turn-based combat sync is the hard part. |
| OpenJK (Jedi Academy) | JACoop | |
| DevilutionX (Diablo) | built-in multiplayer | |
| reone (KOTOR) | multiplayer demo | Engine still early. |
| Ship of Harkinian (Ocarina of Time) | Anchor, Shipwright-Online, True Co-op | Host authority for enemies and bosses is the frontier. |
| sm64 decomp | sm64coopdx | The model most decomp co-op projects copy: puppet players plus synced objects. |
| ac-decomp (Animal Crossing) | ACGC-PC-Multiplayer | Built on a native PC port of the decomp. |

## Big gaps on engines with no co-op base yet

From the directory's gap list: 4A Engine (Metro 2033, Exodus), Decima (Horizon, Death
Stranding), id Tech 5 (Wolfenstein The New Order), Frostbite (Dead Space remake),
Northlight (Alan Wake 2, Control), Visceral (Dead Space), Glacier (Hitman Blood Money),
and console-exclusive engines ported to PC (God of War, Ghost of Tsushima, The Last of Us).
These need a team; start with research and a design doc, and claim the game with an issue.
