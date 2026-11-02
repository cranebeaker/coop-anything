# Dave the Diver (2023)

| | |
|---|---|
| Engine | Unity 6000.0.52f1, IL2CPP (BepInEx 6 + HarmonyX: be.788 for local co-op, be.790 for online) |
| Versions tested | Steam (app 1868140), Windows. Local co-op: `GameAssembly.dll` SHA-256 `8544e01549c9c9329c2c18283f7e35b652b174f59834cfc0279cf8732637be59`, `DaveTheDiver.exe` SHA-256 `d5e61b65ba86c35f92c2fcf749b62c02f2a7fe1b6bd13b0ff8d6a1d3ea2976c3`. Online: Steam build 26209260 (v1.0.6.2118), `GameAssembly.dll` SHA-256 `1f88db7f819a6cdf56908c7b5c775d7acaaea0b339390c29ec4f810d3c543d4f`, `DaveTheDiver.exe` SHA-256 `dcb1257fd0d9525953712154ec2de0a68c06c06f80da00c6813e0ef659e60410` |
| Projects | [Daves the Divers](https://github.com/masonan777/Daves-the-Diver-Coop-Mod) (local co-op), [Coop-Divers](https://github.com/S1ii/Coop-Divers) (online, original), [Dave the Diver Multiplayer](https://github.com/multiplayereverything/Dave-The-Divers-Coop) (online, maintained fork of Coop-Divers) |
| QA level | automated: creature grabs in dives (local co-op); a two-copy lab suite with mutants (online fork) |
| Last updated | 2026-10-30 |

## Where things stand

Two people can dive together on one PC today. Player 2 is a second, purple Dave on the right side of the keyboard or an Xbox controller, with their own oxygen, HUD and loadout. The sushi bar has a second restaurant Dave who can serve, clean and play the mini-games. Story scenes, the farm and other side activities are single-player as before.

Online, two players on two PCs can share one campaign through a friends-only Steam lobby (or LAN): the host's save is copied to the friend, both dive into the same sea with the same fish, and story, dialogue, day and the sushi bar follow the host. Each player still runs their own diver's oxygen, HP and weapons. See [Online: the Coop-Divers fork](#online-the-coop-divers-fork).

## Existing work

- **[Coop-Divers / DaveTheDiverMP](https://github.com/S1ii/Coop-Divers)** (MIT, last commit 2026-07). Online, host-authoritative co-op over direct UDP. Its README lists synced fish, pickups, bosses, story progress and the sushi bar. No GitHub releases. It started this project, but **Daves the Divers shares no code with it**.
- **[Dave the Diver Multiplayer](https://github.com/multiplayereverything/Dave-The-Divers-Coop)** (MIT, 2026-10). A fork of Coop-Divers with Steam online play, fixes for the current game build and a two-copy lab. Pre-releases with the BepInEx loader as its own release. Covered in [Online: the Coop-Divers fork](#online-the-coop-divers-fork).
- **[Daves the Divers](https://github.com/masonan777/Daves-the-Diver-Coop-Mod)** (MIT, 2026-10). Same-screen co-op in a single game instance. The sections from "How the game works" to "Next steps" are about this one.

## How the game works (for co-op)

- **No netcode, one player everywhere.**
  - Gameplay code finds "the player" through a singleton field: `InGameManager.playerCharacter`, which is `Singleton.Instance + 0xA8` in the native code.
  - Restaurant code does the same through `SushiBarStaffManagerImpl.m_Dave`.
  - Grabs, bites, the button-mash bar, HUD updates, the camera and many animation events all act on that one field.
- **Input:**
  - Dave reads his controls in `PlayerCharacter.OnDirectHandler(input)` (`StaffDave.OnDirectHandler` in the restaurant) through the game's `InputSystemWrapper`. Each input map (`Dave`, `InGame`, `QTE`, `MiniGame` …) has its own direct handler.
  - Mini-games take over the input, so the Dave map stops getting actions.
- **Creature grabs** are spread over several parts: skill runners (`QTEReadyToEnterSkillRunner`, `BiteHoldSkillRunner`, `OpenQTEUISkillRunner`), the `DR.AI.Biteable` ability, and creature animation events (`AniClipEvents.OpenQTEUI`, `PlayDaveAnimation`, `DoDamageEvent`). All of them target "the player".
- **Reading the code:**
  - Il2CppInterop (BepInEx `interop/`) gives signatures.
  - For method bodies, run Cpp2IL `2022.1.0-pre-release.21` with `--output-as diffable-cs` for layouts and field offsets (~16 s), and `--output-as isil` for bodies (~1 min, ~800 MB).
  - Then name the call targets with an RVA map. [tools/annotate_isil.py](https://github.com/masonan777/Daves-the-Diver-Coop-Mod/blob/main/tools/annotate_isil.py) in the mod repo does this.

## What we changed

The core trick is **player 2's turn**. Player 2 is a runtime clone of player 1's `PlayerCharacter`. When the game needs to run code for player 2, the mod points `InGameManager.playerCharacter` at player 2, calls the game's own method, and points it back. That reuses the game's bite, grab, mash-bar, helper-item and HUD code instead of rewriting it. The rest is the clone's shared state, split per player:

| Bug | Root cause | Fix |
|---|---|---|
| Player 1's mouse aim froze after player 2 joined | Each input action has one callback slot, and the clone overwrote player 1's | Snapshot every action callback before cloning and restore it (`ActionCallbacks.cs`) |
| Player 2's keys also moved player 1 | Both read the same keyboard | Player 2's keys are read from Win32 (`GetAsyncKeyState`) or XInput. A virtual keyboard that carries only player 1's keys can replace the real one (`KeyboardFilter.cs`) |
| Player 2's knife worked once | A shared "melee command" asset never got player 2's reset | Give player 2 its own command copies (`OwnCommands.cs`) |
| A shark grab on player 2 froze player 1 and hid the HUD | Grab sequences only work on "the player" | Skip the game's grab on player 2 and run a mod-side hold with a copy of the game's mash bar (`P2Grab.cs`, `P2MashUI.cs`); the sawtooth's grab is off for both divers |
| Whitetip stuck in its charge animation after player 2 broke free | The game's mash result sends the fish `SetTrigger(UserQTESucces0 / UserQTEFail0)` before `BiteSkillEnd`; the mod only called `BiteSkillEnd` | Send the same trigger, then end the bite during player 2's turn |
| Player 1 panicked when player 2 was grabbed | Animation event `PlayDaveAnimation` starts the battle-QTE animation on "the player" | Run it during player 2's turn when the creature is really on player 2 |
| A tiger shark holding player 2 also grabbed player 1 | Nothing stopped a second grab | Skip grabs on player 1 by the creature holding player 2 |
| Player 1 locked while player 2 played a sushi mini-game | The mini-game owns the input; player 1's Dave only gets input through the game | The mod drives player 1's Dave directly from player 1's own key bindings, and gives a keyboard player 2 a virtual gamepad for the mini-game |

The repo's `src/` has more, and each file starts with the problem it solves.

## QA

- **Test bot:** an in-game bot (`CoopTest.cs`, following [automated-qa.md](../../playbook/automated-qa.md)). Set `[Test] AutoRun = true` and launch the game. It goes title → Continue → normal dive, joins player 2, then for each creature in `Saw,Whitetip,Tiger_Shark` and each diver it spawns the creature and makes it attack.
- **What it asserts:**
  - The right diver is hit or held.
  - The other diver is never locked.
  - Neither diver is still locked, and the HUD is not still hidden, 4 s after the creature leaves.
- **Mutation pass:** the bot then runs the suite again with the mod's grab rules off, and that pass **must fail**. This proves the tests catch the bugs.
- **Results:** `BepInEx\coop_tests.log`.
- **Last result (2026-10-19):** normal pass PASS, 6 of 6. Mutation pass FAIL as required: 5 pass, 1 fail ("P2 still locked 4 s after the creature left").
- **Not covered by the bot:** the sushi bar, and the grab fixes made after that run (tiger double grab, sawtooth animation). Those were only hand-tested or not tested yet.

## How to play

Install BepInEx 6 IL2CPP (`6.0.0-be.788`), start the game once, then build the mod or drop `DaveTheDiverLocalCoop.dll` into `BepInEx\plugins\DaveTheDiverLocalCoop\`. In game, F9 (or LB + RB on a controller) adds player 2. The [mod's README](https://github.com/masonan777/Daves-the-Diver-Coop-Mod#readme) has the controls.

## Known issues

- Player 2 sometimes flashes as if hit when only player 1 was attacked (sawtooth). The mod logs each flash with its cause to track this down.
- Only one mini-game or conversation screen can be open at a time.
- Story scenes, the farm, bosses and other side activities are single-player or untested.

## Next steps

- Read the flash logs and stop player 2's stray flash.
- Extend the test bot to the sushi bar: serve, clean and mini-games with both players.
- Try the "player 2's turn" approach on boss fights.

## Online: the Coop-Divers fork

[Dave the Diver Multiplayer](https://github.com/multiplayereverything/Dave-The-Divers-Coop) is S1ii's Coop-Divers (MIT, upstream commit `421bbac`) maintained by Multiplayer Everything. Two players, two PCs, two game copies.

### How it works

- **Authority:** the host owns the world. Client actions that change shared state (hitting a fish, a pickup, a dialogue choice, a sushi serve, a purchase) are requests; the host applies them natively and sends the result.
- **Transport:** a friends-only Steam lobby says who is hosting, and datagrams go through `ISteamNetworkingMessages` over Valve's relay, so there are no IPs or ports. LAN UDP is the fallback. The game's IL2CPP Steamworks.NET lacks the calls, so the mod calls `steam_api64.dll` directly and polls every async result; see [networking-steam-spacewar.md](../../playbook/networking-steam-spacewar.md#unity-il2cpp-games-call-steam_api64-flat-poll-everything).
- **Protocol:** its own reliability layer (acks, retries, a 1024-sequence receive window) over unreliable datagrams of at most 1200 bytes. A build id (game build GUID, game version, Unity version, mod id) is checked at connect.
- **Saves:** the host sends its campaign in chunks; the client loads it from manual slot 7 and gets its own profile back when it leaves.
- **Dives:** both hold Start at the dive point. The host sends the sub-scene ids it picked (`B01_01_01` and so on) and the client rebuilds the same layout with the game's `SceneContext.BuildMapLayerDataByCheat`. Seeding Unity's `Random` was not enough: each side builds the layout from its own missions and layer cache.
- **World and story:** fish (seeded spawn plus a host manifest), pickups, puzzle flags, three boss families; story, day, time, dialogue, missions and the sushi bar go as host "manager event" lanes ordered by revision. Shared dialogue advances when both players vote.

### What we changed

The full list with how each fix was checked is in the fork's [BUGS.md](https://github.com/multiplayereverything/Dave-The-Divers-Coop/blob/master/BUGS.md). The ones another agent is most likely to hit:

| Bug | Root cause | Fix |
|---|---|---|
| The mod's `Update` threw 2,163 times in one run and nothing after it ran | Reading a game `Singleton<T>.Instance` on the boot scene creates the manager, which throws there | Read managers only when `hasInstance` (53 call sites); record the scene first |
| Boss families never recognised; pickup ids differed between host and client | `GetType().Name` on an Il2CppInterop wrapper gives the type asked for, not the real class | Use the IL2CPP runtime type and `TryCast` |
| The two divers dove into different seas | Each side picks the dive's sub-scenes from its own state | The host sends its scene ids; the client rebuilds from them (mutant `layout`) |
| Shared story stopped at "1/2 votes" | `StartDialogueV2` throws a native exception on the client for tutorial bundles, and Il2CppInterop swallows it inside the patched method | A client that couldn't show the dialogue sends a spectator vote (mutant `dialogue-spectator`) |
| Both divers dead: soft-lock in spectator mode | The exit waited for a confirmation a dead diver can't send | Both dead ends the dive (mutant `party-wipe`) |
| Session lost during loading or saving | 5 s peer timeout | 20 s timeout, longer reliable expiry (lab test `stall`, mutant `short-timeout`) |
| Online join stuck on "Receiving host save" | Every unacked chunk re-sent after 0.1 s clogged Steam's relay | Retransmit from the measured round trip, at most 32 chunks in flight (mutant `fixed-retry`) |
| Second session refused: "slot 7 changed outside DTMP" | The game itself saves the shared campaign into slot 7 | Reclaim a DTMP-marked slot after backing it up (mutant `slot-reclaim`) |
| Friend joined but never saw Ready | Esc (also when closing Steam's invite overlay) ended the host's running session | Esc only leaves when no session runs (mutant `escape-leaves`) |
| Online needed an open UDP port | Direct UDP only | Steam lobby plus SteamNetworkingMessages |

The IL2CPP lessons are in [reverse-engineering.md](../../playbook/reverse-engineering.md#unity-il2cpp-il2cppinterop-traps).

### QA

- `scripts\make-lab.ps1` makes two isolated game copies with their own BepInEx and save folder. `tests\lab-suite.ps1` starts a host and a client with `-dtmp-lab` and a scripted tour, then compares what each logged. `-Steam` runs the Steam code path through an in-mod SteamMock (one Steam account can't P2P to itself), `-Chaos` adds 120 ms lag and 5% loss both ways. `tests\mutation-suite.ps1` runs each mutant, which must fail the check it targets.
- Checks: connect, save fingerprint, puppet position, dive, dive exit, second dive, sea layout, fish binding, host fish kill, client harpoon catch, party death, a 12 s client stall, travel to the sushi bar, story, day, sushi start, sushi menu, shared dialogue, sushi bar open.
- Last results, Steam build 26209260:
  - **0.19.3, Steam path (2026-10-30):** 19/19 PASS, 0 errors logged.
  - **0.19.0, LAN and with lag and loss (2026-10-27):** 19/19 PASS each.
  - **0.19.0, Steam path (2026-10-27):** 18/19. "Host fish kill" failed once: the host's kill didn't show on the client.
  - **Mutants (0.19.0):** each of the 10 mutation runs failed its target check, as it should.
- Not covered: two different Steam accounts (the lab uses one, through the mock), real menu input, sushi nights beyond opening, bosses and missions.

### How to play

Download the BepInEx loader from the [`bepinex-6.0.0-be.790` release](https://github.com/multiplayereverything/Dave-The-Divers-Coop/releases/tag/bepinex-6.0.0-be.790) and the mod from the [latest release](https://github.com/multiplayereverything/Dave-The-Divers-Coop/releases), unzip both into the game folder, and start the game once. On the title screen pick **Online**: the host picks **Host on Steam**, the friend picks **Join: <host>** or accepts the Steam invite, then Ready and Start. Both need the same game build and mod version.

### Known issues

- Diver HP, oxygen and weapons are not synced: each player runs their own diver.
- Clients can't damage bosses.
- Some sushi-night cases, mission resets and stacked time stops can drift; they are listed as open in the fork's BUGS.md.
- One client per host.

### Next steps

- A full session on two Steam accounts, then promote the release out of pre-release.
- Lab tours for a sushi night, a mission reset and a boss fight, to reproduce the open bugs.
- Wire up the upstream `DiverVitalReplicator` and `DiverWeapon*` classes so oxygen, HP and weapons are shared.
