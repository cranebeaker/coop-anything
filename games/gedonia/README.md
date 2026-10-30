# Gedonia (2022)

| | |
|---|---|
| Engine | Unity 2019.4.20, Mono backend (Assembly-CSharp.dll), NGUI, Enviro sky, A* Pathfinding, DunGen |
| Versions tested | Steam, app 1114220, build 16727998 (Gedonia.exe SHA-256 B375205374C5355991FE1C91577CEB729AD9FBC2FC6534FE4EF897288A5DF91B), Windows 11 x64 |
| Projects | [Gedonia Co-op](https://github.com/Zerbith/gedonia-coop) (directory entry "Gedonia / Gedonia Co-op"), claim [#214](https://github.com/cranebeaker/coop-anything/issues/214) |
| QA level | automated |
| Last updated | 2026-10-30 |

This is the original Gedonia, not Gedonia 2 (which has official co-op).

## Where things stand

Two or more players can play the open world and the story together over Steam (friends, in-game invites,
relay) or in the home network. Players, enemies, story, conversations, loot spots, world events,
expeditions with their generated dungeons, time and weather are shared. Every feature has an automated
two-client scenario; real sessions with friends over Steam went fine, and the points they
found (0.6.x) are fixed in 0.7.x. 0.8 adds groups, enemy scaling and trading, so far tested only by the
automated scenarios. Status: alpha (0.8.1).

## Existing work

None found before this project (web search, Nexus, this repo's issues and pull requests, 2026-10-24).

## How the game works (for co-op)

- **No netcode.** UNet only appears in the bundled Enviro sky samples. The game ships Steamworks.NET 12
  (SDK 1.42) in Assembly-CSharp-firstpass: classic SteamNetworking P2P with relay fallback, lobbies,
  friends, rich presence; no SteamNetworkingMessages. It runs under its own app id.
- **Singletons everywhere.** `PlayerScript.instance` is referenced about 11,400 times. So each machine
  keeps one local player and other players are puppets: a clone of the player root (`PlayerFinal`) made
  under an inactive holder (so no Awake runs), stripped to Transform/Animator/renderers/
  PlayerModelCustomization, then activated with root motion and animation events off.
- **Appearance** is applied with `PlayerModelCustomization.RefreshPlayerModel(...)`, but it also reads
  the LOCAL player (helmet/cloak visibility from PlayerPrefs, quiver by the local weapon, torch light by
  local stealth). These must be sent and set on the puppet before calling it.
- **Enemies:** `FighterNPCModule` AI runs from `NPC.Update` via `npc.updateFunction`; all damage goes
  through `NPC.Damage(7 args)`. Target search wants the "Player" tag, and an attacking enemy drops a
  target that has neither that tag nor an NPC component, so puppets carry a disabled NPC component.
  Enemies in streamed cells (World Streamer, scenes `MediumStr__x..._z...`) load and unload by distance.
- **Story:** about 980 public methods on `GameManager`, dispatched by name with
  `GameManager.instance.SendMessage(name)` from dialogue answers, mystery confirmations, switch triggers,
  timelines and world events. Quests: `QuestsData.AddQuest/ObjectiveCompleted/ReturnQuest`.
  Dialogue: `DialogueNPCModule.Talk` -> `InterfaceManager.RefreshDialogue` (buttons named by answer
  index, requirements checked on the local player) -> `OnDialogueButton`.
- **World state:** `SaveLoadManager.WorldDatabase` (Dictionary<int,string>) with `SaveableItem`
  ids; containers, ore and herbs are `Container` with loot taken in `InterfaceManager.TakeLootItem`.
- **Saves:** Easy Save 3 files in `%USERPROFILE%\AppData\LocalLow\Oleg Kazakov\Gedonia`, PlayerPrefs in
  `HKCU\Software\Oleg Kazakov\Gedonia`, both named after the Unity product name (see the playbook lesson).
- **Time and weather:** Enviro (`EnviroSky.GameTime`, per-zone random weather rolls).
- **Expeditions:** rolled with UnityEngine.Random per machine; dungeon phases use DunGen with
  `ShouldRandomizeSeed = true` (the stored seed was ignored).
- **Travellers:** `TravellersManager` keeps a virtual position per traveller (46 of them: merchants,
  bards, sages, citizens, rabbits) and spawns the NPC when the local player comes near; each save had
  them in different places.
- Tools: ilspycmd 9.1 for decompiling, BepInEx 5.4.23.5 + HarmonyX, the plugin is compiled with the
  .NET Framework csc that ships with Windows (C# 5).

## What we changed

All in the [Gedonia Co-op](https://github.com/Zerbith/gedonia-coop) plugin; each item has a test scenario
(`scripts/test-local.ps1 -Scenario <name>`).

- **Transport:** `ITransport` with Steam P2P (friends-only lobby, invites from an in-game friend list,
  join requests) and UDP with its own reliable channel (numbered, acked, resent). A mock of the Steam API
  over loopback UDP follows Steam's rules (sessions held until accepted, size limits, loss), so the Steam
  code path is tested with two clients on one PC.
- **Players** (walk, looks, bow): puppet model, animator mirror (dominant layer-0 clip match about 98%,
  Speed parameter error 0), look signature hash checked on both sides, gear/helmet/cloak/mod weapons,
  Steam name, world map/minimap/compass markers.
- **Enemies** (fight, apart, lossy, death): presence reports, host picks one owner per enemy with epochs
  and a hold time; the owner publishes, others mirror and send damage requests; deaths run everywhere,
  so every player gets a lootable body of their own. `scripts/check-ownership.ps1` fails if an enemy is
  owned twice in a row. Bug found: an exception on an enemy whose cell had unloaded aborted the rest of an
  ownership message. Bug found by players: looting writes "0" for the body into the world database, and
  the host's world-state sync deleted the body at everyone else; now a dead body stays until looted there.
- **Groups** (party): the host keeps the groups (the inviter leads; invite, answer, leave, remove are
  requests to the host, which sends the whole table). A group limits combat XP sharing and the map
  markers to its members; without a group neither is shared.
- **Enemy scaling** (scaling): the host counts, per shared enemy, the members of one group within 150 m
  (from the enemy presence reports). The count goes up at once and down only after 10 s and when the
  enemy is unhurt and has no target, so a fight keeps its numbers. Every machine applies the host's
  settings to `maxHealth` (health in proportion); damage is scaled once, where the player is hit
  (`PlayerScript.Damage`, or by enemy id for a hit forwarded by the owner, whose attacker may not be
  loaded there).
- **Trading** (trade): offers carry versions and a confirmation names both, so any change takes it back.
  Two-phase commit with the host as coordinator: both confirmed -> each trader re-checks, writes an
  escrow file and takes its goods out -> the host writes its decision to a file before anyone hears it
  -> deliver, or abort and give back (a failure, a trader gone, or 20 s of silence). A trader that
  missed the outcome asks the host again from its escrow file. The game has no quest-item flag;
  `Item.cantbesold` serves as one (weapons and armour with it stay tradable).
- **Shared XP** (effects): `PlayerScript.AddExperience` multiplies by intellect, rings, sleep and dungeon
  blessings, so equal kills gave different XP and levels drifted apart. Copied deaths give no XP; the
  machine where the enemy really dies sends the amount it gained, and group members within 100 m add
  exactly that (level-ups through the game's own loop).
- **Story** (story, latejoin): a Harmony transpiler swaps the `SendMessage` in the answer/trigger sites for
  a dispatcher; the host numbers story calls and every machine runs the same function in that order;
  quest digests repair anyone who fell behind; the world database changes are sent to late joiners.
- **Shared conversations** (dialogue): conversations are announced; players near the NPC join with F;
  clicks are votes; agreement decides at once, otherwise the starter's vote after a configurable time
  (or only agreement); an answer's stat requirement is met if any member meets it; money/item costs
  need one member who has them and only that member pays (the story function runs everywhere, so on
  all other machines item removals are skipped and gold is given back for that call); only the leader
  dispatches the story function.
- **Effects and transformations** (effects): `Object.Instantiate` of `PlayerScript.effectsGos` prefabs
  and `ParticleSystem.Play` on the player are sent; receivers build the prefab under an inactive holder
  and strip every script except visual ones and every collider before it wakes, so copies never deal
  damage. Werewolf, blood demon and elemental forms are separate bodies from the same prefab list; the
  transformed player streams body, position and every Animator state, others show a copy instead of
  the puppet.
- **Cutscenes**: `TimelineCutsceneController.StartCutscene` is held in co-op; every player holds the
  interaction key until a ring fills, then the host starts it for all. Others are hidden while it plays.
- **Loot** (loot, farloot, chestgold, chestlid): a lock per container from the host, takes broadcast and
  applied by item; containers that are not loaded apply them when they load.
- **World events and expeditions** (event, eventhandover, expedition, expfull): the event owner spawns,
  others build the same enemies with the same ids; expedition lists come from the host, generation is
  seeded with the stored id (also DunGen), every expedition phase type was played through.
- **Companions and summons** (companions, summons): each player owns their followers; others see
  mirrors built from the same Resources prefab.
- **Clock/weather** (clock), **worlds and dungeons** (worlds, dungeon), **travellers** (travellers),
  **sounds** (Harmony postfix on `AudioSource.PlayOneShot/Play`: clip name and path sent, played on the
  puppet or mirror), **pause** (the pause menu no longer stops time in co-op), **respawn crystals**
  (respawn, respawnnorth: the game only has a reload on death, which would turn back the world; in
  co-op the fallen player gets up at a crystal bound with F; crystals stand at measured spots, because
  searching a dry spot on each PC put one into a pond whose water streamed in late on a slower PC).

## QA

- `scripts/test-local.ps1 -Transport udp|steam-mock -Scenario <name>` starts two copies of the game
  (the second is hard links; each has its own Unity product name, so saves and settings are separate)
  with a fixed test save, runs timed test steps and bots in both, and logs what each side observed
  (positions, animation, look signatures, ownership, story events, votes) plus screenshots.
- 38 scenarios. Runs on the 0.8.x code (2026-10-27 to 2026-10-30): party (XP not shared before the group,
  shared in it, not after leaving; invite, remove, decline), scaling (same max health on both PCs, Bandit
  100 -> 150; hits 35 -> 42; an attacking unhurt enemy keeps its numbers after the group ends), trade (a
  normal exchange, an offered item used up, no answer to the commit, a missed outcome; the sum of both
  inventories per stack stays the same), a fight without the private German files; 0 errors.
- Runs on 2026-10-26 on the 0.7.x code: dialogue (gold answer paid by the friend only,
  host's gold unchanged, mercenary joins the payer), effects (UDP and Steam mock: friend's slashes and
  host's spells shown on the other side, both players 33 -> 183 XP with intellect 2 vs 8, a body looted
  by the host still lootable at the friend), transformations (all six forms shown, puppet hidden),
  cutscene (held until both held the key, started on both within 10 ms; letting go early resets the
  ring), respawn at the measured crystal, story; 0 errors in both BepInEx logs.
  Earlier milestones (enemies with 20 % packet loss, expedition phases, late join) are listed in the
  mod repository's history.
- After every run the real saves and registry values were compared with a backup (unchanged).
- Real mouse clicks in the test windows were not automated (desktop control was not used). The tester
  and friends used the menus and shared conversations by hand in real sessions up to 0.6.x; the 0.7
  additions (hold-to-confirm ring, transformations) and the 0.8 windows (group, trade) are only tested by
  the automated scenarios so far; those call the same functions as the buttons.

## How to play

Download the ZIP from the [releases](https://github.com/Zerbith/gedonia-coop/releases), run `Install.cmd`
(finds Gedonia via Steam, backs up saves, downloads BepInEx 5.4.23.5 with SHA-256 check). Main menu:
"Host co-op" (Steam or home network) and "Join co-op"; in game the co-op button in the top bar or F8.
Everyone needs the same mod version.

## Known issues

- A game closed without saving goes back to its last save, so a crash right after a trade can undo that
  player's side.
- Scaling also applies to neutral animals, which are shared enemies too.
- Story teleports move every player.
- Steam's overlay invite may not open when the game is started outside Steam; the in-game friend list
  works.
- Arena, Survival and Dungeon mode (separate main-menu modes) are not tuned for co-op.
- Other players' companions stay visible during a cutscene.
- The "Uncharted Islands" cannot be reached in this game version (no travel route).

## Next steps

- Real-network sessions with more than two players.
- In-game chat (the co-op menu has a placeholder).
- A real-network test of groups, scaling and trading.
