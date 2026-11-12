# Death Stranding Director's Cut (2022)

| | |
|---|---|
| Engine | Decima |
| Versions tested | Steam Director's Cut, app `1850570`, build `13300582`, `ds.exe` SHA-256 `9d5306d7eb2f5d3135c63accc3e74b8f9b029f9d76112a6aae4a38fdbb4edd2b`, Windows. The mod refuses other builds. The 2020 PC release and the Epic release are untested |
| Projects | [death-stranding-coop](https://github.com/TommoDotCommo/death-stranding-coop) (MIT, prototype, no release; directory entry "Death Stranding") |
| QA level | automated (two game copies on one PC; see QA for what it covers) |
| Last updated | 2026-11-10 |

## Where things stand

People cannot play the story together yet, and there is no release. [death-stranding-coop](https://github.com/TommoDotCommo/death-stranding-coop)
is a prototype built from zero that works on one PC with two copies of the game: each player sees the other as a
second Sam who walks, runs, crouches and jumps beside them, in the other's gear, seated on a bike or truck when the
other rides one, with a name label and a map pin. Cargo one player puts down or picks up appears or disappears in the
other's world, and a structure one builds or dismantles (run with ladders) is built or removed in the other's.

Nothing about a shared story exists: no BTs, MULEs, deaths, orders, deliveries or cutscenes, and a session between two
PCs has not been tried.

## Existing work

No real-time co-op or multiplayer mod was found (searches and their limits are in [NOTES.md](NOTES.md) and the
directory entry's `searched:` list). The game's own
[Social Strand System](https://www.kojimaproductions.jp/en/death-stranding-directors-cut-beginners-guide) shares
structures between players' worlds asynchronously; it does not show players to each other.

Tooling that reaches into the game (public metadata as of 2026-10-25; none of it was downloaded, built or run here):

| Project | Evidence and relevance | Confirmed version / activity | License / reports |
|---|---|---|---|
| [DeathStrandingModLoader](https://github.com/NRTnarathip/DeathStrandingModLoader) | Director's Cut loader. Source adds entity tracking, reflection, Lua bindings and render-callback ticks beyond the README's emulator focus. Native spawning and co-op not demonstrated. | `master` at [`8e5396d`](https://github.com/NRTnarathip/DeathStrandingModLoader/commit/8e5396d222d72e9f231d439def3ddd81f66f6eac), 2025-09-24; latest release metadata [`v0.0.4`](https://github.com/NRTnarathip/DeathStrandingModLoader/releases/tag/v0.0.4), 2025-08-07; release contents not inspected | Root MIT; vendored decima-native has GPL-3.0 text. Review component licenses before reuse. [Issues](https://github.com/NRTnarathip/DeathStrandingModLoader/issues) |
| [decima-native, dsdc-injector branch](https://github.com/ShadelessFox/decima-native/tree/dsdc-injector) | Relevant non-default branch: RTTI/exported-symbol dumping, core load/unload listeners, signature-based address discovery and resource-edit examples. No demonstrated player replication. | [`3a9211a`](https://github.com/ShadelessFox/decima-native/commit/3a9211abd48a46a348361f9fbbc3453f6773d601), 2026-04-11; no release listed | GPL-3.0; [issues](https://github.com/ShadelessFox/decima-native/issues) |
| [Decima Workshop](https://github.com/ShadelessFox/decima-workshop) | Typed asset/core-object viewer/editor and archive repacker; explicitly supports both PC editions. Useful for locating actor resources and references. | Latest release metadata [`v0.1.27`](https://github.com/ShadelessFox/decima-workshop/releases/tag/v0.1.27), 2025-04-26; default-branch tip `59c6ea8`, 2026-05-10 | GPL-3.0; [issues](https://github.com/ShadelessFox/decima-workshop/issues) |
| [Decima Explorer](https://github.com/Jayveer/Decima-Explorer) | Archive listing/unpack/pack tool; exact Director's Cut compatibility unverified. README warns that blind core swapping can prevent loading. | `master` tip `f22ef4c`, 2020-09-10 | MIT outside Ooz; Ooz GPL; [issues](https://github.com/Jayveer/Decima-Explorer/issues) |
| [Odradek RGB Sync](https://github.com/advanced-bencoding/odradek-rgb-sync) | Read-only named-animation discovery. Author's evidence is Epic Director's Cut under Proton; Steam/native Windows compatibility unverified. | `main` at [`8a88b3f`](https://github.com/advanced-bencoding/odradek-rgb-sync/commit/8a88b3f56cc3fbd0f5a5552076d04b6b5ec2724d), 2026-10-23 | MIT; [research and limitations](https://github.com/advanced-bencoding/odradek-rgb-sync/blob/main/RESEARCH.md) |

## How the game works (for co-op)

Observed on build 13300582; addresses and the evidence for each are in the mod's
[mod/game_offsets.md](https://github.com/TommoDotCommo/death-stranding-coop/blob/main/mod/game_offsets.md), a summary
in [NOTES.md](NOTES.md).

- 64-bit exe with no anti-cheat files in the install. It calls `SteamAPI_RestartAppIfNecessary`, so a copy needs a
  `steam_appid.txt` or Steam may relaunch the original install.
- Decima's reflection names (types, exported functions) are plain strings in the exe, and RTTI names the classes.
  Short masked signatures plus a RIP-relative resolve find the getters.
- Saves are under Local AppData, found through the known-folder API, so an environment variable does not move them;
  Steam Auto-Cloud syncs a second path. A single-instance mutex stops a second copy.
- The local player's position is three float64 and its rotation a 3x3 float32 matrix.
- Creating a second Sam from the player's resource crashes: the player's creation path attaches a
  `DSPlayerController` that an independent body cannot have.
- The game keeps a spare Sam in memory, `DSPlayerAKukanEntity`, the private-room body: asleep, hidden, with no AI or
  controller. The game's own sleep, visibility, transform, mover and animation-variable calls drive it.
- The engine's common entity creator, given that body's resource, makes a further Sam; the game's in-place art-parts
  swap dresses it.
- Cargo items are records in a table (`CreateBaggage`, `DeleteBaggage`). Structures are records in the construction
  manager's table (`CreateConstruction`), and the game has a "Net" variant for other players' structures with its own
  id range.
- A seated porter is a child entity of the vehicle. Vehicles come from a pooled creator and a mover can play back
  transforms.
- The game reads keyboard and mouse as raw input and pads through Windows.Gaming.Input; it never polls XInput.

Not known: how story progress, orders, enemies, death and cutscenes are stored and driven; whether the 2020 release or
other builds share any of this.

## What we changed

Nothing in the game's files. In disposable copies only:

- **A loader proxy that isolates a copy.** It redirects saves and settings into the copy by patching the import tables
  of every loaded module, makes the single-instance mutex per-copy, and refuses the game's online connections so a
  modded copy never talks to the game's service. Each protection has a mutant its test must fail. Two threads patching
  import tables at once crashed the game once; one lock fixed it.
- **A game-free presence core**: versioned handshake, quantised snapshots, clock and round-trip estimates,
  interpolation, reconnect, and an exactly-once channel for world events. A 15.6 ms Windows timer and relative sleeps
  made a localhost test drift by 0.3 to 0.4 m; a monotonic clock and fixed deadlines brought it to about 1 cm.
- **The partner's body.** First the borrowed private-room Sam, handed back whenever the game wants it; then a body of
  the mod's own from the engine's entity creator. Applying a second outfit left two outfits overlapping; the game's
  own art-parts swap, after the porter is dressed, fixed it.
- **Shared cargo and structures**, with bugs found by fuzzing two simulated worlds against each other: a quick
  put-down and pick-up left a stray copy, identical items close together were matched wrongly, and shared ids were lost
  when either mod restarted (a removal now also names the item and its place).
- **The partner's vehicle, gear, carried cargo, name, map pin, pings, rain where the host stands, and likes** (off by
  default, because the game saves and counts them).
- **Cost per frame** cut from about 105 ms to about 5 ms by caching region checks and reading each pointer chain once
  per tick.
- **Tests without the desktop**: scripted keyboard and mouse fed to the game as raw input on its own thread, and a
  stand-in partner program so one copy can be tested alone.

## QA

- Game-free: `make test` in the mod repository runs the simulator, the hub's fuzzer, the two-world cargo and
  structure fuzzers and every mutant.
- Live: two real copies (or one copy and the stand-in partner), driven without the window focus, with state read from
  the game processes. Asserted: the partner body's distance from the real porter, cargo and structures appearing and
  disappearing on the other side, the real saves unchanged after every run.

Last game-free run (2026-11-10, mod commit `bb7fcfc`):

```
make test
exit 0, about 7 minutes; 345 mutants reported killed, none surviving
```

Live regressions recorded in the mod's STATUS.md for 2026-11-10 (not re-run for this write-up): both porters walking at
once in two copies, each copy's partner within a median 5 mm and at most 9 cm of the other's track over 15 to 18 m;
the partner quitting and starting again, both seeing each other 3 mm apart afterwards, no fault in any process.

Not tested: two PCs, more than two players, anything with enemies or story, structures other than ladders, the game's
own build menus.

## How to play

Not playable yet. There is no release and the mod is only ever installed into disposable test copies. Build and test
instructions are in the [mod's README](https://github.com/TommoDotCommo/death-stranding-coop#readme).

## Known issues

- Two copies on one PC only; a second PC is built but untried.
- The partner's vehicle is display-only: a porter walks through it and its wheels do not turn. No passengers.
- The partner's carried cargo is drawn as plain cases, not the game's packing.
- Identical items stacked on one spot can be matched the wrong way round after a restart (6 in 2000 fuzzed sessions).
- Only ladders are confirmed for shared structures.
- The game's licence terms forbid reverse engineering. The project stays offline-only, bypasses no protection and
  ships no game files; anyone building on it should read the terms and decide for themselves.

## Next steps

1. A session between two PCs over a LAN or VPN.
2. Structures built through the game's own menus, then the other structure types.
3. Collision and passengers for the partner's vehicle.
4. The first piece of shared story: one order accepted and delivered by two players, credited once.
5. BTs and MULEs: who owns an enemy, and what a death or voidout means for two players.
