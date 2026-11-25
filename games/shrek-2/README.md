# Shrek 2 (2004)

| | |
|---|---|
| Engine | Unreal Engine 2 (KnowWonder build; exact revision unknown) |
| Versions tested | Russian 1C disc edition, version 1.00: `Game.exe` SHA-256 `4900AA131CF35D4951943F2D6EE831841745AF8E5D657FFDCFD7FB012CBAA2EF`, `Engine.dll` SHA-256 `79EB368BA102740644EB6537E1CABFE7BA58DF7BBA47DC4045653CD30A2385E3`, Windows 11 |
| Projects | [Shrek 2 Enhanced](https://github.com/stxlvn/shrek2-enhanced) (MIT, this write-up); [Shrek2-Multiplayer](https://github.com/Master-64/Shrek2-Multiplayer) and its fork [Michiocre/Shrek2-Multiplayer](https://github.com/Michiocre/Shrek2-Multiplayer) (prototype). Directory entries: "Shrek 2" in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | automated |
| Last updated | 2026-11-20 |

## Where things stand

Up to four players can play the story levels of the 2004 PC game together in the host's world, over LAN, a direct
address or UPnP. Every check so far used scripted bots and several game copies on one PC. Nobody has played the whole
story through together, and play over the Internet between real people has had little testing. There is no public
release yet: the sources are on GitHub and the setup is built locally (commit `88ba1ed`, 2026-11-20).

Shrek 2 Enhanced works only on the Russian 1C edition: its setup checks the hash of `Engine.dll`, and the 1C script
packages differ from the English ones.

## Existing work

| Project | What it is | Co-op | Last activity | Source |
|---|---|---|---|---|
| Shrek 2 Enhanced | script package `Enhanced.u` plus an engine layer (`d3d8.dll` proxy) and a setup | 2-4 players, story levels, host's world | 2026-11 | https://github.com/stxlvn/shrek2-enhanced |
| Shrek2-Multiplayer (Master_64) | a chat and menu skeleton on the MPak library: lobby page, T-key chat, the local hero's position sent as text every tick to ghost pawns | players visible only; its header lists shared inventory, level transfer, saves and enemy targeting as open tasks | 2025-02 | https://github.com/Master-64/Shrek2-Multiplayer |
| Michiocre/Shrek2-Multiplayer (branch `rewrite`) | the same with the author's own TCP text link and a Node.js relay with room codes (`RoomServer`) | as above, no port forwarding | 2026-08 | https://github.com/Michiocre/Shrek2-Multiplayer |
| Shrek 2.2 (Master_64) | full-game mod on the English edition; MPak library with `MPatcher` level fixes | "Online P2P Multiplayer" listed as a future feature only | 2025-07 | https://github.com/Master-64/Shrek-2.2 , https://github.com/Master-64/MPak |
| Shrek 2 Remastered (Aspyr) | official remaster with couch and online play | official, a different game | announced for 2026-11-10 | see [NOTES.md](NOTES.md), "Existing work" |

Details of the review (what MPak and the forks contain, what was worth taking) are in [NOTES.md](NOTES.md),
"Shrek 2.2, MPak and the Shrek2-Multiplayer forks".

## How the game works (for co-op)

- Unreal Engine 2 with its replication and `IpDrv.dll` shipped, but no multiplayer mode or menu. A listen server starts
  with `<map>?listen` once a three-place patch of `Engine.dll`'s network start is applied ([NOTES.md](NOTES.md), "Level
  change with a client").
- Gameplay is UnrealScript in `KWGame.u` and `SHGame.u`. UELib decompiles every class (2,192 classes in the five script
  packages). The Shrek 2 Level Editor supplies `UCC.exe` and stub packages that a mod compiles against. Its classes are older
  than the game's: `PlayerController.ClientHearSound` has 6 parameters there and 7 in the game, `Actor.PlayMusic` differs,
  and `Actor` lacks `PreSaveGame`/`PostLoadGame`. A mod must not call `Super` on, or override, what the stubs declare differently.
- **One hero everywhere.** The scripts read `LevelInfo.PlayerHeroActor` 1,399 times in 235 classes. Enemies, traps,
  pickups, save fairies, bosses, the HUD and cutscenes all see only the host's hero.
- **Per-level values on placed actors.** A map sets values on the hero it places (read from the retail maps with UELib):
  - `7_Prison_Donkey` Steed: double jump 128 (class default 96);
  - `4_FGM_PIB` Puss: dies from falls over 1000 (default 768);
  - `6_Hamlet` Shrek: walk speed 200;
  - `10_Castle_Siege`: run speeds 300 / 240 / 157.5;
  - `1_Shreks_Swamp`: no jump attack.

  A hero spawned from the class does not get these values.
- **Cutscenes** are `KWCutScene` scripts that possess the cast with `KWCutController` and drive the host's camera. A level's
  opening starts from the `PlayerStart` event.
- **Saves:** save fairies call the stock `SaveGame`. A stock load loses each hero's health (it comes back as 100) and the
  link of placed projectors to the renderer (`RenderInfo` is transient), as MPatcher found.
- **Loading:** a network client that opens a local map crashes while painting the loading screen of the old level
  (`ATerrainInfo::UpdateVertices` from `UGameEngine::PaintProgress`).

## What we changed

All in [Shrek 2 Enhanced](https://github.com/stxlvn/shrek2-enhanced). Earlier work (lobby in the book menu, LAN search,
UPnP, co-op checkpoints and saves, gates before cutscenes, leash, death and revive, enemy scaling, Hor+ field of view, HUD,
animation sync, setup with rollback) is described in [NOTES.md](NOTES.md). Added 2026-11-20:

| Problem | Cause | Fix | Check |
|---|---|---|---|
| Enemies, traps, pickups and bosses ignored the guests | one global hero (above) | engine-layer hook of the VM opcode that reads object variables (`GNatives[0x01]`) answers each reader. Owners get their player's hero, a hero's own code gets itself. An event about an actor (`Touch`, `Bump`, `HitSHPawn`, ...) gets that actor for 3 s. Other actors get the nearest usable hero (1.5 s hysteresis). Host-only functions and classes get the host. | 4-player swamp run: 3 guests spawned and moved, about 1,900 swapped reads per 20 s, no deaths |
| Guests saw the opening wrongly | the host's cutscene was mirrored | each guest plays the level's opening alone as a single-player game (`?EnhIntroOnly`), then joins with `?EnhIntroDone`. He appears where the host's hero stands after the opening. | run `lc8`: opening 115 s on the guest, joined, spawned 125 units from the host |
| Guest client crashed after its opening | the `PaintProgress` crash (above) | the engine layer skips `PaintProgress` while the process is a network client | `lc8`, `lc10`, `hv1`, `sw1`: no crash |
| Host played on while a guest watched | the owner's rule | the host pauses the world (`Level.Pauser`) with a note until every guest sent to an opening is back, or 4 minutes pass | `lc10` (24 s), `hv1` (14 s), `sw1` (11 s) |
| Guest heroes missed the level's values | spawned from the class | the guest copies them from the host's hero (same class); the guest's machine gets the jump heights for prediction | `hv1`: guest Puss got fall distance 1000 |
| Music, boss bar and timers only on the host | played locally | music changes are relayed (ALAudio hooks); boss bar, game and potion timers are drawn on the guest HUD | swamp and boss runs |
| MPatcher fixes | see [NOTES.md](NOTES.md) | Steed double jump 128, Puss falls up to 1000, swinging balls hit with a 100 x 100 cylinder, health and projectors restored after a stock load (credited to Master_64) | `sv1`: health 37 came back as 100 and was restored to 37; projector `RenderInfo` was 0 after the load and is attached again. `tw1`, `tw2`, `hv1` logged the changed values. |
| 794 `Accessed None` lines per run on a guest | Puss's sword exists only on the server | the sword notifies skip when there is no sword | `sw1`: 0 |

## QA

The rig runs host and guests as separate game copies of a sandbox install on one PC:
- quiet windows on a second monitor that never take focus;
- My Documents redirected by a proxy DLL, with a check that the real `Documents\Shrek 2` did not change;
- scripted input on the guests;
- the game's own screenshots;
- a log check per run: who joined, who got a pawn, deaths, leash, mod warnings, crashes, T-pose repairs, cutscenes.

The rig scripts live in the owner's workspace, not in a public repository. The installer test is public:
`tests/test_installer.ps1` in the mod's repository.

Runs of 2026-11-20 (sandbox, Russian 1C edition):

| Run | Players | Level | Result |
|---|---|---|---|
| `lv1` | 4 | 1_Shreks_Swamp | 3 guests joined and moved, hook active, only stock `Accessed None` warnings |
| `lc8` | 2 | 1_Shreks_Swamp | guest's own opening, rejoin at the obstacle course, no crash |
| `lc9` | 2 | 1_Shreks_Swamp | **failed**: the guest's own opening stopped in its 3rd second (cause not proven), the host waited the full 240 s |
| `lc10` | 2 | 1_Shreks_Swamp | pause 24 s with the note, guest next to the host |
| `sv1` | 1 | 1_Shreks_Swamp | stock save and load: both stock bugs shown and fixed |
| `tw1`, `tw2` | 1 | 8_Prison_PIB, 3_The_Hunt_Part2 | Steed, Puss and swing-stone values changed |
| `hv1` | 2 | 7_Prison_Donkey | level values copied to the guest, pause 14 s |
| `sw1` | 2 | 4_FGM_PIB | 0 sword warnings (794 before) |
| installer test | - | scratch game copy | PASSED, 58 checks (without the HD pack) |

The earlier 4-player sweep of all 20 maps (`runlevels.ps1`) has not been repeated since these changes.

## How to play

No public release yet. Build from the sources ([README of the mod](https://github.com/stxlvn/shrek2-enhanced#building)).
The setup installs into the 1C edition and backs up every file it changes. Host and join from the book menu.

## Known issues

- Guests cannot use potions, the shops or the wanted posters: in the stock code they belong to the host's hero controller.
- Mid-level cutscenes are seen through the host's camera. The swamp's in-level movie plays only on the host.
- In one test run a guest's own opening stopped and the host waited 4 minutes before going on (`lc9`).
- Play over the Internet and long sessions are barely tested.

## Next steps

- Repeat the 4-player sweep of all 20 maps with these changes.
- Potions, shop and posters for guests (needs a hero controller for guests).
- A guest playing the Steed (client-side jump sync) has not been exercised.
- A relay with room codes for players without UPnP (idea from the Michiocre fork; needs a hosted server).
