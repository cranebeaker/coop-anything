# Spooky's Jump Scare Mansion: HD Renovation (2017)

| | |
|---|---|
| Engine | Unity 2021.3, Mono, Windows x64 (BepInEx 5.4.23.5 or MelonLoader 0.7, Harmony) |
| Versions tested | Steam (app 577690), buildid 12368455. `SpookyUnity.exe` SHA-256 `ed13875fc3eab0f0afe677c3eee936e1885789b032c78ccab3a59a4c8b79b74a` |
| Projects | Spooky Together ([spooky-together](https://github.com/multiplayereverything/spooky-together), [v0.2.11 pre-release](https://github.com/multiplayereverything/spooky-together/releases/tag/v0.2.11)); directory entry "Spooky Together" |
| QA level | automated |
| Last updated | 2026-10-26 |

## Where things stand

Spooky Together lets 2 to 4 players run the mansion together: the base mansion, Endless, the hospital and the Dollhouse. Everyone is in the same room and is chased by the same specimens, and the team shares keys, items and the axe. Endings take the whole team. A lethal hit downs a player instead of ending the run while a teammate is still standing. It is an alpha. The author has played it with friends in real sessions, and it worked very well. They played four players on separate PCs and Steam accounts. Every fix is also covered by scripted tests with up to four game copies. Nobody has written up a full playthrough yet.

## Existing work

No other co-op project for this game was found in this directory or in the author's research notes ([`docs/research.md`](https://github.com/multiplayereverything/spooky-together/tree/main/docs)). The game is single-player only.

## How the game works (for co-op)

- **Engine:** Unity Mono, so the game logic is in `SpookyUnity_Data/Managed/Assembly-CSharp.dll` and Harmony can hook it. Rooms are drawn from a sequence. Each room's randomness (locked doors, lights, flips, rotations, which Dollhouse local-room specimens appear) comes from Unity's random generator, so the mod seeds it per room from the host.
- **Specimens:** most find the player through `SpookyCharacterController` reads. Some keep the player in a local or a field, including coroutine state: Spec6, Spec8, the Hooked Doll, Frenzy and Nightmare.
- **Run state:** puzzle flags, notes, `violence` and ending values live in a key store that room scripts read and write. Endings branch on those keys and on the specimen and jump-scare options.
- **Steam:** `SteamPlatformInstance` calls `SteamAPI.RestartAppIfNecessary(577690)`. Starting `SpookyUnity.exe` directly quits right after "Discord: init", with no error in the log. Lab copies need `steam_appid.txt`; real installs launch through Steam.

## What we changed

The full feature list is in the mod's README and MODLOG. This section lists the lessons that apply to other games.

### Retarget enemies with a transpiler, including the player kept in a variable

- **Approach:** the host runs every specimen. A transpiler rewrites each specimen's reads of the player's `.transform` into `Target(this)`, which returns the nearest standing player. Uses of the player for health, the camera and sounds stay local. A specimen's hits land on the player it is chasing.
- **Bug (fixed in 0.2.9):** five specimens kept the player in a local or a field, so they measured the distance to the host and ignored teammates. The transpiler now also rewrites `.transform` reads of a `SpookyCharacterController` held in a local or field, coroutine state included.
- **Proof:** the `enemies` tour counts the rewritten reads for each specimen: Spec6 8, Spec8 12, Hooked Doll 1, Frenzy 2, Nightmare 3.

### Pair the client's own copy of a spawned enemy with the host's

- **Bug:** the host's spawn usually reaches a client before the client's own room script spawns its copy. The client made a stand-in at once and removed its own copy 6 s later, so room scripts holding that copy lost it, and `Spawned()` ran twice.
- **Fix:**
  - a stand-in waits until 1.5 s after the client's room is ready;
  - the client's own copy binds to a host specimen that is still waiting for one;
  - an own copy arriving after a stand-in (within 10 s) replaces it.

### Shared keys: one copy, a checksum repair, and no double counting

- **Shared copy:** the run's keys and the inventory are one shared copy, and anyone's change reaches everyone. Every 10 s the host compares checksums with each client and repairs any difference.
- **Bug:** a puzzle segment met by a teammate's change ran its events on every machine, so its `AddKey` counted once per player.
- **Fix:** a segment met by a remote change runs its events for show only, and its key and inventory writes are skipped.
- **Contested actions:** the Dollhouse furnace re-reads the shared values before every action and refuses actions that no longer apply. Clients' actions run on the host's furnace, in order. Three players taking the key at the same moment give one key.

### Turn quit-the-game scares into knock-downs

Two scares close the game outright: Unknown Specimen 1's second pursuit and the scare chair. In co-op they become normal knock-downs, so one player's scare doesn't remove them from the session.

### A mutation check for every fix

Each fix has a lab-only switch (`-st-mutate <name>`) that turns it off. `scripts/mutants.ps1` runs the tour that guards each fix with its switch on, and every such run must fail. That proves the test can actually see the bug. There are 16 switches: retargeting (three kinds), hit routing, shared keys, weapon hits, elevator presses, stale locks, spawn pairing, the furnace, puzzle replays, missed endings, key order for late joiners, room re-skins, early specimen spawns, endings during a room load and endings held for a load that never came. `tests/rules-test.py` checks the source in seconds, with no game: every message op has a handler and is built somewhere, every mutable static is reset or marked, every empty `catch` says why, and the version numbers agree.

## QA

- `scripts/run-tour.ps1 -Tour <name> [-Players 4] [-Lag <ms>] [-Steam]` starts up to four isolated lab copies, minimized and small (480x270, lowest quality, 30 fps, below-normal priority) so they never take focus: a BepInEx host and MelonLoader or BepInEx clients. It runs one scripted tour and judges the run with `tests/verify-lab.py`, which checks:
  - the tour's checks;
  - the error count;
  - managed heap and object-count trends;
  - entity desync;
  - the room sequence;
  - the mod's CPU time per frame (budget 2 ms).
- Lab players act like real players. They open real doors, pick up the axe and chop boards. Forced transfers or fake item ids would break the game and give false failures.
- **v0.2.11, 4 players:**
  - The full suite passed 29/29. It includes 150 ms lag, a soak over the LAN address with 120 ms lag, and full EM, KH and dollhouse mansions.
  - The 7 switches touched in 0.2.10-0.2.11 were rerun, and all were caught.
  - Lesson: a short tour gave the heap-slope rule only three samples, inside one garbage-collector sawtooth. The rule now needs five; the soak judges long runs.
- **v0.2.9, 4 players:**
  - `rules-test.py` passes.
  - The new `pairing`, `furnace`, `puzzles` and `lateending` tours pass, along with `enemies`, `dollprops`, `ending` and `clientending`.
  - The quick suite passed 13 of 15 at 97% CPU. `axe` and `elevator` failed under load and passed on reruns.
  - The mutation check: all 5 new switches were caught.
  - `test-transport.ps1`: 5/5.
- **Earlier results:** 0.2.8 passed the quick suite 11/11 and its mutation checks 6/6. 0.2.2 passed the full suite 17/17, including a 10-minute 4-player soak.
- **Real sessions:** the author has played it with three friends on separate PCs and Steam accounts (four players), and it worked very well. No structured notes were kept, so it stays at `automated` rather than `verified-playthrough`.
- The Steam path runs real Steam lobbies and Steam Networking Sockets between lab copies on one account, over 127.0.0.1.
- Not covered:
  - specimens chasing a teammate in real play (the retarget check counts the rewritten reads);
  - real puzzles beyond the test segment;
  - Steam between two accounts.

## How to play

1. Install BepInEx 5.4.23 (copy `SpookyTogether.BepInEx.dll` to `BepInEx/plugins/SpookyTogether/`) or MelonLoader 0.7 (copy `SpookyTogether.MelonLoader.dll` to `Mods/`). Downloads are on the [releases page](https://github.com/multiplayereverything/spooky-together/releases).
2. Start the game through Steam and press **F5**.
3. Connect:
   - **Online (Steam):** Host on Steam. Friends join from the Steam games list, from Join Game on the host's profile, or with an invite.
   - **LAN / direct:** the host needs TCP 27020 reachable, and others join from the LAN server list or by address.
4. The host starts the run.

## Known issues

- Taker boss attacks are per player. The host decides the boss's health, when it is open to hits, and the ending.
- Health, stamina, the flashlight and notes on screen are per player.
- Some puzzle props update only when you next enter the room.
- The Dollhouse furnace reacts a moment late for clients on a slow link.
- If the host disconnects, clients keep playing alone; there is no host migration.

## Next steps

- Run a real two-account Steam session.
- Play to an ending in co-op and write it up for `verified-playthrough`.
