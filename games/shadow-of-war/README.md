# Middle-earth: Shadow of War (2017)

| | |
|---|---|
| Engine | Firebird, Monolith's LithTech-derived engine (internal name "Kraken") |
| Versions tested | Steam app 356190, build 3095001 (last update 2018-09-05), `x64\ShadowOfWar.exe` 1.0.8874.0, SHA-256 `ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef` |
| Projects | none playable; [JustAddMP/ShadowOfWar](https://github.com/JustAddMP/ShadowOfWar) is a placeholder; research here (claim: cranebeaker/coop-anything#93) |
| QA level | none (no co-op exists yet) |
| Last updated | 2026-10-21 |

## Where things stand

There is no co-op for Shadow of War. This folder holds research and reverse-engineering
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

No co-op changes yet. Tooling for working on the game safely (not published yet):

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
- Co-op QA: none (nothing to test yet).

## How to play

Nothing to play yet.

## Known issues

None (no release).

## Next steps

- Decide the sync model: host-side event stream from `HandleFactionEventAction` and the
  death/promotion functions, or host world state through `Faction.Save` / `Faction.Load`.
- Check whether the LithTech client/server split is real at runtime (`GameServerShell`,
  `clientmgr`, NetMesh code).
- Map the player character from `GameClient` (RVA `26FFA98`).
