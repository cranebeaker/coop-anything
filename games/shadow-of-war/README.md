# Middle-earth: Shadow of War (2017)

| | |
|---|---|
| Engine | Firebird, Monolith's LithTech-derived engine (internal name "Kraken") |
| Versions tested | Steam app 356190, build 3095001 (last update 2018-09-05), `x64\ShadowOfWar.exe` 1.0.8874.0, SHA-256 `ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef` |
| Projects | [Santonus-not-bot/shadow-of-war-coop](https://github.com/Santonus-not-bot/shadow-of-war-coop) (alpha, MIT); [JustAddMP/ShadowOfWar](https://github.com/JustAddMP/ShadowOfWar) is a placeholder; research here (claim: cranebeaker/coop-anything#93) |
| QA level | automated: scripted checks with two game copies on one PC and an offline suite; never tried on two PCs |
| Last updated | 2026-11-08 |

## Where things stand

The co-op mod is in its own repo, [shadow-of-war-coop](https://github.com/Santonus-not-bot/shadow-of-war-coop): one host and one
client, the client joins the host's save, sees and fights the host's enemies, follows the host's fast travel, and runs the
host's Nemesis events (captain fights, kills, promotions) on its own copy of the Nemesis world, and shows each
captain as himself (built from its own copy of his Nemesis record; a captain the host makes after the join is an
ordinary orc there). Hits go both ways with the host's grunts, archers, Olog-hai, berserkers, spearmen, shield orcs and wild
wargs; the client's arrows and gear effects reach the host's enemies exactly; counters protect the client. Both players' deaths,
bad networks (a simulated 330-380 ms lossy link), a 10-second outage mid-fight and runs of up to 90 minutes were checked with
bots, without a crash. Alpha,
tested only on one PC. This folder holds the research and reverse-engineering behind it,
for a from-scratch, host-authoritative co-op: a safe way to run and instrument a test copy,
a map of the engine's console commands, the Nemesis system's live log output, and the
Nemesis event, death, promotion and RNG functions with byte signatures for the Steam
build above. Raw notes with sources: [NOTES.md](NOTES.md).

## Existing work

- [JustAddMP/ShadowOfWar](https://github.com/JustAddMP/ShadowOfWar): README only, "planned"
  (created 2026-10-17), part of a group with a shared framework,
  [JustAddMP/core](https://github.com/JustAddMP/core).
- No other co-op or multiplayer mod found (web, GitHub, Nexus via search results, Steam
  forums; 2026-10-20). The game's online features (Vendettas, Online Conquest) are
  asynchronous.
- Modding tools that work on this build (all hook the running game):
  [ReaperAnon/Middle-Earth-Mod-Loader](https://github.com/ReaperAnon/Middle-Earth-Mod-Loader)
  (a `bink2w64.dll` proxy) and its [mods](https://github.com/ReaperAnon/Shadow-Of-War-Mods);
  SystematicSkid's [save editor](https://github.com/SystematicSkid/shadow-of-war-save-editor),
  [freecam](https://github.com/SystematicSkid/sow-freecam) and
  [backend server rebuild](https://github.com/SystematicSkid/shadow-of-war-server);
  survivalizeed's [resource loader](https://github.com/survivalizeed/ShadowOfWarPacketLoader).
  Their byte patterns all match this build exactly once; the save editor's fixed
  *function* addresses do not (its data addresses do).

## How the game works (for co-op)

- **Code on disk.** SteamStub is present and Denuvo likely (its usual section layout, no
  marker string; NOTES.md session b), but the game's code section `.arch` (31 MB), the
  `.pdata` function table and `.sdata` are byte-identical on disk and in memory while
  the game runs, so the exe file can be read statically. Image base
  0x140000000.
- **Engine layout.** LithTech's client/server split survives in the source paths
  (`Game\ClientShellDLL`, `Game\ObjectDLL`, `Game\Shared`), with a "game NetMesh" hosting
  port and a host-vote net state machine. Whether a real client/server split exists at
  runtime is not known yet.
- **Console registry.** A linked list of 680 named entries (RVA `2A58BE0`): 449 console
  programs (handler pointer at entry `+0x38`) and variables, with description, source file
  and line. Relevant programs: `Faction.Save` / `Faction.Load` / `Faction.Dump` (the whole
  Nemesis world), `FactionMember.Save` / `.Load`, `Faction.SimEvent.*`,
  `Faction.AdvanceTime`, `QuestMgr.Save` / `.Load`, `Teleport`, `SlowMO.*`, `Replay.*`.
- **Debug log.** A compiled-in `debug_print(category, fmt, ...)` (RVA `19C83AC`) is a 12-byte
  stub in this build; 1,100 call sites, most gated by one-byte channel flags (table around
  `2A34D9E`-`2A34DBF`). Hooked, with the Nemesis channels on, it logs the system live.
- **Nemesis system.** Internally "Faction". All its logic runs on one game thread. Every
  event goes through `HandleFactionEventAction(Event, Subject, Other)` (RVA `40C304`), which
  also applies promotions; deaths go through a resurrection check and a random roll
  (`RollResurrection`, `6DBD4C`, against `RandomFloat`). Regions are factions
  (`Faction_MtnPass`, `Faction_desert`, ...); captains hold (row, col) positions.
- **Randomness.** One global 64-bit LCG (`state = state * 0x0BB38435 + 0x3619636B`, upper
  32 bits returned), state at RVA `2A25C88`, reached through `RandomFloat` and an inlined
  copy.
- **Saves.** `%LOCALAPPDATA%\WB Games\Shadow of War\<prefix>_ShadowOfWar.sav` plus numbered
  slots, rotated once per session. Every save is also sent through
  `ISteamRemoteStorage::FileWrite`, which `steam.exe` writes into
  `Steam\userdata\<id>\356190\remote` even with Steam Cloud off for the game. Autosave is
  frequent (16 saves in ~5 minutes of play).
- **Online.** With outbound connections failed as on an unplugged PC, the game reaches the
  main menu and plays normally; it retries two HTTPS hosts periodically. Refusing socket
  creation instead makes it show a server error dialog.

Function table and signatures: NOTES.md, sessions r and s.

| RVA | function | unique signature (verified in the running game) |
|---|---|---|
| `40C304` | HandleFactionEventAction | `48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 70 4C 8B FA` |
| `62E670` | its caller (also adds event replays) | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 81 EC 80 00 00 00` |
| `6DB95C` | on captain killed: evaluate resurrection | `48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 4C 89 60 20 55 41 56 41 57 48 8D 68 A1 48 81 EC A0 00 00 00 4C 8B F2` |
| `6DBD4C` | RollResurrection | `48 89 5C 24 10 48 89 74 24 20 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 60 48 8B 99 28 02 00 00` |
| `622A84` | SimEvent::Simulate | `48 89 5C 24 08 57 48 83 EC 20 80 3D ? ? ? ? 00 48 8B D9 74 2B` |
| `741284` | AddFactionEventReplay | `48 8B C4 48 89 58 08 48 89 70 10 48 89 78 20 4C 89 40 18 55 41 56 41 57 48 8D 68 B9` |
| `1B3C6C` | RandomFloat | `48 83 EC 28 0F 28 C8 E8 ? ? ? ?` |
| `1807908` | Rng (call target of RandomFloat) | resolve from RandomFloat (the LCG step also occurs inlined at `FC0E93`) |
| `18DDCC` | LookupFactionMember | `8B 52 08 E9 ? ? ? ? 48 8B 49 20` |
| `18BDB0` | RecordName | `48 85 C9 74 18 F6 41 01 02 74 12 48 8B 41 20` |

## What we changed

The co-op mod itself is in its own repo (see "Where things stand"); it grew out of this tooling for working on the game safely:

- **sowguard**, a `bink2w64.dll` proxy for a copied test install: refuses to run outside
  the test folder or on another build; hooks ntdll file calls in the game process to
  redirect the save folders into a sandbox and stop the game on any write to the real
  install or saves; fails outbound connections like an offline PC; blocks
  `ISteamRemoteStorage` writes in the game process (the Steam client would otherwise write
  the test copy's saves into the real Steam folder).
- **sowprobe**, a named-pipe probe inside the game: read memory, scan patterns, follow
  pointers, read the console registry; with write mode on, capture the debug print.

## QA

- The tooling has an offline test suite (no game process) and a negative-control build
  with the protections compiled out; both live in an unpublished repo, so no results are
  claimed here.
- Live: seven guarded launches on the Steam build. The after-run check against a hashed
  backup of the real saves and Steam folder caught two writes from outside the game
  process: the NVIDIA App rewriting the real `render.cfg`, and `steam.exe` writing the
  test copy's save into the real Steam cloud folder (restored from the backup). The
  second led to the remote-storage block; with it, a session with 16 autosaves left the
  real saves and Steam folder unchanged. One launch crashed because of the first
  debug-print hook (see the playbook lesson on hooking leaf stubs); fixed and tested.
- Co-op QA (session cm, 2026-11-07, two copies on one PC, bot-driven: `tools/roam_fight.py` in the mod's repo moves the client
  around an arena while the host keeps orcs on it): two 25-minute runs and a 15-minute run on the current build completed with
  no crash, 258-509 enemy hits on the client per 25 minutes, all applied by the client; the client's freeze (the game's reference
  list destructor looping, caused by the mod's own write to a captain's member link) and two host crash causes (a removed
  decoy's AI running on, raw target writes) were found, fixed and checked with negative controls (`tools/jump_cycles.py`,
  `tools/captain_unlink.py`, unit tests with a looping-list control). Offline: unit tests 93/93 and 36/36, guard suite 52/52.
  Each player's death was checked with bots too (`tools/host_death.py`, `tools/death_standin.py`): the client follows the host's
  respawn, or is taken back to the host after its own, and both pictures follow again; no crash.
  `tools/regress.py` runs the live checks in one go (a fight, duplicate damage, exact losses, both deaths, kill-and-respawn): 6 of 6
  passed on the latest build (2026-11-08). Fights also ran through the hub's chaos relay on bad Wi-Fi, transatlantic and the worst profile (8% loss in
  bursts, 330-380 ms round trip): hits went both ways, the link never dropped, no crash; a 10-second outage in a fight recovered by
  itself. Soaks of 25, 30, 45, 75 and 90 minutes completed without a crash.
  Details and every run in [NOTES.md](NOTES.md).

## How to play

See the player guide in the mod's repo (`release/README-PLAYERS.md`): a co-op copy of the game folder per player, an overlay VPN,
host starts first, client joins and gets the host's save.

## Known issues

- Never tried on two PCs over a real network.
- The host's archers' arrows land on the host's picture of the client (it stands where the decoy does); since session cm that
  damage is sent to the client. Archers shoot less at the client than at the host's player.
- A 256-entry table in the game's AI fills for characters whose AI the mod switches off (the client's enemy pictures, both
  stand-ins) and crashed the client three times; the mod now makes such a character again before its table fills (session cm).
  Not yet run for hours. The table is the animation state machine's (its entries come from the start-action function); after
  the client's death its picture of the host kept a growing list the game never counted, so the mod now checks the list too.
- The host's picture of the client can lag behind on rough ground (the enemies follow the client's sent place since session cm).
- When the host dies, the client stays in its fight through the host's death scene, screens and the killer's presentation and is
  taken to the respawn point once the host walks again (session cm; it was pulled away at once before).
- A swing that reached both the host's picture of the client and the decoy was sent twice; since session cm it is sent once,
  also when two enemies land the same attack at once.
- The host's tamed wargs hurt the client when their running lunge passed through its picture; since session cm hits by the
  host's tamed wargs and followers are not sent (seen once in a fight; the drop was checked with a staged hit).
- Counters protect the client: a swing it counters does not take its health (checked in session cm), so the experimental
  give-back (`coop_counter_heal`) stays off.
- When a captain ambushes the host, the client applies the host's Nemesis events (the presentation, the showdown, a kill of the
  host's player) and also stands on a screen until the player presses on (seen once with bots).
- After a client death the host's picture of the client once stood still 1,600 units off (the game no longer moved it); since
  session cm the mod moves such a picture by direct writes until the game moves it again. Why it happens is not known.
- A picture whose AI update the game has switched off slides and jumps (moved only by the mod's put-backs) and its AI list is
  never emptied; since session cm such a picture is made again after 6 s. What switches it off in play is not known.
- Two rare host crashes in the game's AI (seen once and twice) both read the host's player as an AI character. The mod no longer
  points enemies at the host's player, and both crash sites have a narrow guard; neither crash came back, so the fix is not
  proven.
- Caragors, graugs and other beasts were not tried (they cannot be spawned in the test region).
- In bot tests a captain's appearance or the host's death leaves a copy on a screen until a press; the test tools press through
  (players do so anyway).

## Next steps

- A test between two PCs over a real network (needs a second player).
- Find what switches a picture's AI update off in play (the byte at +0x285 of its agent: set by the agent's binding `193704`,
  cleared by its shutdown `415D5C`; session cm), so pictures need not be made again.
- Fights with beasts (a region whose spawn table has caragors and graugs).
- Run `tools/regress.py` (in the mod's repo) on every change; it covers a fight, duplicate damage, exact losses, both deaths and
  kill-and-respawn.
