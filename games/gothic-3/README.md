# Gothic 3 (2006)

| | |
|---|---|
| Engine | Genome; see the unofficial [Gothic 3 SDK](https://github.com/georgeto/gothic3sdk) |
| Versions tested | Steam build 1037023, `Gothic3.exe` 1.75.20108.14 (Community Patch 1.75.14), x86, with Parallel Universe Patch 1.1.1 |
| Projects | [G3 Together](https://github.com/GGM1k3y23/g3-together) (own mod, built from zero on the SDK, GPL-3.0-or-later; public beta, latest [v0.6.0-beta](https://github.com/GGM1k3y23/g3-together/releases/tag/v0.6.0-beta) of 2026-10-26). Earlier projects: [OTHER-PROJECTS.md](OTHER-PROJECTS.md) |
| QA level | automated (bot suites with two real game clients on one PC, each stage 3 passes plus a failing negative control), plus a human co-op evening |
| Last updated | 2026-10-26 |

## Where things stand

G3 Together makes the whole Gothic 3 campaign playable for two players. It is a public
beta ([releases](https://github.com/GGM1k3y23/g3-together/releases), latest v0.6.0-beta of
2026-10-26) with a one-click setup in English, German and Polish. Players connect over
direct IP or a virtual LAN such as Hamachi. They see each other on screen, map and
compass, fight the same enemies, share quests, dialogs and story moves, freed towns, loot,
town reputation, the clock and the host's saves, trade with each other and revive a downed
partner. Every feature is covered by bot suites that drive two real game clients; for
0.6.0 that was 187 runs, and the bots played the main story to Innos' ending. People have
played it together for an evening. Still to be played by people: the Adanos and Beliar
endings and sessions with more than two players.

## Existing work

- **G3 Together** (this project, since 2026-10-17): a native script DLL built on
  the Gothic 3 SDK, with [ENet](https://github.com/lsalzman/enet) 1.3.18 (MIT) for direct
  IP. Host plus clients, regional NPC ownership, host-kept ledger for story, items and
  the campaign save. GPL-3.0-or-later. Source: [GGM1k3y23/g3-together](https://github.com/GGM1k3y23/g3-together)
  (public releases since 2026-10-22, beta since 2026-10-25, latest v0.6.0-beta). Details below and in
  [NOTES.md](NOTES.md).
- Earlier projects: Myrtana Accord (tested: only quest state is shared; players do not
  see each other, no NPC or combat sync, rejoining is refused), Gothic 3 Heroes and
  Sative's Gothic 3 Multiplayer (no download available to test). None of them gave usable
  story co-op, so G3 Together was started from zero, to avoid repeating their limits; it
  uses none of their code or files. Details: [OTHER-PROJECTS.md](OTHER-PROJECTS.md).

## How the game works (for co-op)

Observed in the test copies unless marked "static analysis" (Ghidra or SDK reading,
not confirmed at runtime). Addresses and decompiled code stay private.

**Loading code and running two copies**

- Script plugins are native DLLs in `scripts/`, loaded through a `ScriptInit` export.
  This build's loader needs the decorated name `_ScriptInit@0`.
- A subclass of `eCEngineComponentBase` registered with `eCModuleAdmin::RegisterModule`
  gets `Process()` about 100 times per second in the campaign world, plus
  `WorldActivate`, `WorldDeactivate` and `PreShutdown`. No vtable patching needed.
- Game logic runs through named scripts held by the script admin. A mod can swap a
  script's function pointer (`gSScript::m_funcScript`) and call the original. AI states
  and AI functions (441 in this build) can be wrapped through the name maps of
  `gCScriptAdminExt`. Some scripts are also called directly, not through the admin
  (`GetAttitude` has 29 direct callers; `GiveXP` is called directly from the death
  script), so a pointer swap alone misses those calls.
- `Gothic3.exe` exits with code 0 while the named mutex `GenomeMutex` exists. A second
  copy starts once the first closes its own handle to it.
- The new-game intro `G3_Intro.bik` (214 s) blocks the session loop; no per-frame
  callback runs. It starts only after the world is active; one Escape posted to the game
  window then skips it.
- Menus read the window's message queue; gameplay keys are read through `DINPUT8.dll`.
  Keys posted to the window work in menus, not in play.
- Saves, options and logs go through `SHGetFolderPathA` from six engine and patch DLLs.
  Redirecting that import per copy gives each test copy its own profile.

**Who simulates what**

- The engine only processes entities inside a sphere around the active camera
  (`[Engine.Setup] Entity.ROI` in `ge3.ini`, 4000 = 40 m in this install; measured
  about 45 m). `Entity::GetNPCs()` lists only those. Entities further away exist and
  resolve by GUID, but are frozen: no task, and a kill written to them does not hold.
  When they are processed again the engine places them by their routine. So a single
  host cannot simulate the world once players split up.
- Entities from the world data have the same GUID on every copy. Entities created at
  runtime (summons, arrows, drops) get random IDs, so they need network IDs.

**Damage, death and experience**

- Weapon touch damage, the AI combat loop and `gCEntity::DoDamage` all end in the
  `AddHitPoints` script. In a 45 s raid probe, 225 of 236 calls were weapon touch damage;
  the swapped `DoLogicalDamage` entry was never called. Wrapping `AddHitPoints`
  (negative amounts) is where weapon and spell hits are decided, but it is not the only
  way to damage or death (static analysis): `OnTick` applies poison and burn damage by
  calling `AddHitPoints` directly, past the script table; `AssessHit` can schedule a death
  before `AddHitPoints` runs; `RagdollDead` sets hit points to 0 itself.
- The hit judgement (`OnDamage` / `AssessHit`) records the damage, and hit points drop
  afterwards in `AddHitPoints`. It caps the damage of any attacker but the player, so only
  the player's hits reach the death branches. Calling `gCEntity::DoDamage` from a mod
  raised an exception. Other outcomes that `Script_Game.dll` applies by calling
  `AddHitPoints` directly (parries, perks) also bypass the admin: unknown whether that
  matters.
- A remote player's body must have collision on, or no weapon ever touches it.
- The `Kill` script does the bookkeeping of a death: quest `OnNPCKilled`, experience
  through `GiveXP` (paid only while `gCNPC_PS::DefeatedByPlayer` is unset, then set),
  plunder and party clean-up. The death tasks `ZS_Dead` / `ZS_RagDollDead` run it; the
  console kill `OnCheatKill` only sets those tasks (static analysis, matches runtime).
  A mirrored NPC must not run `Kill`: in G3 Together it doubled quest events and paid
  experience for other players' kills. Setting hit points 0 and AIMode 9 (dead) first
  makes `ZS_Dead` only lay the body down.
- `Entity::Kill()` destroys the entity. Killing NPCs from outside in mid-fight crashed
  the game several times; a lethal hit judged by the game did not.
- When the hero dies, `Kill` shows the game-over message. The knock-out task
  `ZS_LieKnockDown` is the game's own non-lethal fall; its loop heals the hero and stands
  it up after 5 s (static analysis, timing observed).

**The player is one entity**

- NPCs pick targets by asking the script `GetAttitude(self, other)` for every NPC they
  process. The player is special only by identity with `Entity::GetPlayer()`; a second
  `PC_Hero` NPC is judged by faction tables, and orcs then never attack it. Wrapping the
  native `GetAttitude` so a remote player's body is judged like the local hero made
  enemies attack remote players. One case must be left alone: an NPC fleeing from that
  body (Panic).
- 201 of 847 registered scripts compare something with the player (static analysis).
  Many only matter when the player itself runs them.
- A transformation spell makes the creature `Entity::GetPlayer()` while
  `gCSession::GetHero()` stays the hidden human body.
- The script `Shoot` aims at the camera's screen centre only for the player; for any
  other shooter the arrow flies to its target (static analysis).

**Story, dialogs, clock and saves**

- `gCQuestManager_PS` holds 570 quests. `RunQuest`, `SucceedQuest` and `FailQuest`
  change status with the game's own side effects (journal, experience).
  `gCInfoManager_PS` holds 4591 dialog options; `InfoGiven` marks one as told.
- Kill quests count kills by the victim's name in `gCQuest_PS::OnNPCKilled`, with no
  killer check and no guard against a second call (static analysis).
- Each dialog line is a string command (69 kinds) run by `IS_Running_ProcessInfo` on the
  machine that holds the dialog. Setting `InfoGiven` elsewhere replays nothing (static
  analysis), so the results of a dialog have to be sent, not the flag.
- The clock lives in the `World_MCP` entity's clock property set; its setters keep the
  clock consistent. A sleep jumps the clock.
- `gCHUDFileManager::SaveGame` / `LoadGame` in Game.dll are the game's save and load
  (found with Ghidra, not exported). A save is `<name>.g3savcpx` plus
  `<name>.g3savcpxdat`. A save taken while a second `PC_Hero` exists cannot be loaded
  ("current world has more than one hero!"). `LoadGame` activates the new world without
  a `WorldDeactivate` first. Loading a slot that does not exist hangs the game.

**Animation**

- Motion slot 0 of `eCVisualAnimation_PS` is what a body visibly plays. A motion started
  with `Entity::StartPlayAni` is soon replaced by the stance's idle loop. Replaying
  attacks, parries and staggers as the game's combat move (`sAICombatMoveInstr`), and
  gestures as a play-ani instruction on the body's script unit, made them run on the
  other machine. Walking is played by the movement controller
  (`PSCharacterMovement::SetMovementMode` plus a goal), not by replaying the motion.

## What we changed

Nothing in the game files. G3 Together is a new mod, written from zero.

**Architecture (one DLL, `Script_G3Together.dll`, in `scripts/`)**

- Transport: ENet direct IP, UDP port 34567, the host listens. Player state at 20 Hz.
  Protocol version 36 at the tested `master` (commit `dff5cfa`).
- Remote players are puppets spawned from the hero's template (or the creature's when
  transformed), with no AI of their own, collision on, moved by the game's movement
  controller and showing the player's weapons, stance and swings.
- Every NPC has one owner, the machine that simulates it: near the host the host, far
  from the host the client that processed it first. Others mirror it; all AI states of a
  mirror are wrapped so it only runs an idle task. `OnTick`, which is a callback and not an
  AI state, is skipped on mirrors and puppets separately (commit `2a9e983`).
- One hit rule on every machine, enforced in `AddHitPoints`: a hit is decided by the
  owner of its target; hits caused by puppets or mirrors are dropped; a hit on another
  player's puppet goes to that player and is judged against the real hero there. Paths
  that bypass `AddHitPoints` need their own guard: copies of area spells owned by a
  puppet run no area damage at all (`5628884`, after a Hailstorm copy had judged a death
  before the filter).
- The host keeps the ledger: quests, told dialogs, story events, dialog results, item
  claims, chest/corpse/trade transactions, kill-quest counts, the clock and a journal of
  deaths. The journal is saved next to the host's save as `<save>.g3t`.
- Campaign: the host's native save is the campaign. A joining client gets it over ENet
  (16 KB chunks with CRC-32), loads it itself and puts its own hero record on top.
- The host refuses a client whose game DLLs, script DLLs, rules archives or game options
  differ, and says which.
- The test bot is compiled in and only active when the copy contains `qa-enable.txt`.

**Stages** (mod stage table at commit `dff5cfa`; each passed 3 times with two game
clients plus a negative control that fails)

| Area | Stages | What the suites show | Limits written in the stage table |
|---|---|---|---|
| Base | 1, P | Engine module, hero report, clean shutdown; install parity at join | Loose files in `Data` subfolders and custom difficulty sliders not checked |
| Players | 2, V3, V4, T, L | Puppets walk and run with the game's motions, hold the player's weapons, show the player's swings (14 to 15 of 15), show a transformed player as the creature; a key jumps to the partner | Fights and saves while transformed not checked; the key press itself is not bot-driven |
| NPC ownership | 3a, E (E1 to E5), B, C | Host NPCs mirrored on clients; NPCs far from the host owned by the client, mirrored by the host when it arrives and taken back when the client leaves; companions follow their recruiter, also across a save; distance notice at 35 m; deaths far away applied when the other machine gets there | Tested with 2 players only; journal holds deaths only |
| Combat | 3b, 3c, A, A2, V1, V2, V5, H3, P3a, P1 | Hits decided by the target's owner; experience to the killer only; no game over while a partner can help, help costs one healing potion, death when nobody can help; enemies attack remote players by themselves; mirrored fights replay as combat moves; dialog duels attack the right player; companion and summon kills pay their player; a meteor summon acts once | Bot hits in 3b go through `OnDamage`; monsters hitting a downed hero and robbery not checked; 37 to 55 % of the host's sampled attacks seen running on the client |
| Story | 4, 4b, 4c, H, Q | Shared quest status, told dialog options and story events; a client can talk to host NPCs; dialog results that change an NPC reach every machine; kills far from the host count once in kill quests | Dialog Attack/Spawn/inventory/party commands partly; teaching and trading dialogs not checked |
| Items | 5, 5b, 5c | Ground items, chests, corpses and trade are host transactions; the first claim wins; a refused take is undone | Bot trades through script calls, not the trade screen |
| Saves and time | D1, D2, D3, G, G2 | Saves remove puppets first; per-player hero record; joining gets the host's campaign over the network; one clock; a sleep moves time on for everyone | Menu save path assumed to use the hooked function; real bed dialog not bot-driven |

Since then the mod's stage table (at `master` `d0cdb38`, 2026-10-21) also lists as
passing: P1 (meteor summon), P2 (arrows and bolts flown as copies on the other machine;
"an arrow that hits counts once"), P2f (fireball) and HO (heal the other player). The
runs behind P2 and P2f are in [RUNTIME-QA-G3TOGETHER.md](RUNTIME-QA-G3TOGETHER.md); for P2
the bot's arrows rarely reach the target, so exactly-once has 3 green runs of 5, not three
in a row, and none for bolts. Further additions on `master` (NPCs fight back against the
client, shared doors and locks, NPC spells, summoned creatures, town reputation) are in
[NOTES.md](NOTES.md) and the mod's commit history, not yet in this table.

**Test harness** (part of the mod repository, `qa/`)

- `qa/together.py --suite <name> [--negative-control]` starts a host and a client copy,
  sends the in-game bot numbered commands through a file, reads its answers from the
  mod log and prints `RESULT: PASS` or `FAIL`.
- Before every run it checks the copies' isolation; before and after it hashes the
  owner's real save folder. It stops only the processes it started.
- Two full copies of the install per test pair, each with its own redirected profile.
  A second pair (own copies, port 34568, own QA lock and run folder) lets a second agent
  test at the same time.
- Each game releases its own `GenomeMutex` handle as soon as its bot answers; a start
  that exits early is retried.
- Screenshots come from the game windows only (`PrintWindow`), never the desktop.

## QA

How the suites work: each stage has a suite. A stage counts as done after 3 passes in a
row, a negative control (the feature switched off by a QA command) that fails, and the
earlier suites still passing. Every run also asserts a clean shutdown of both games and
unchanged real saves. At the end of every two-player suite, invariant monitors compare
the NPCs near both heroes three times, 3 s apart (report only, they do not fail a
suite).

Latest (2026-10-26, v0.6.0-beta, mod commit `ef449a3`): full regression of 187 runs on
three PC pairs, 181 passed in 121 min. The six reds were rechecked: `stageA` and
`stageTreasure` passed on rerun; `stageP2hit` (the test shooter rarely reaches its target)
and `stageObserveSuspect` (measured too late) are known test-side reds; `stageUpset` is also
red with the 0.5.1 build, and the measurement `friendlydiag` passed 2 of 3 times.

Earlier results (2026-10-19, details and run IDs in
[RUNTIME-QA-G3TOGETHER.md](RUNTIME-QA-G3TOGETHER.md)):

| Series | Result |
|---|---|
| Full regression with invariant monitors (38 suites) | 37 of 38 passed. `stage3c` failed on a test assumption, was adjusted, then passed; negative control failed as required. Monitors: no lasting violation in any suite |
| Pre-package regression (9 suites) | 8 of 9 passed. `stage3b` failed because the orc parried the test hit; the test now swings again; rerun passed, negative control failed |
| After the runtime trace was added (7 suites) | 7 of 7 passed |
| Merged `master` `dff5cfa` | `stageP1`, `stageP3a`, `stageL` passed; a full regression on it had started and its result is unknown here |
| Soak, 2026-10-18 (older build) | 22 min, 320 random actions; then 55 min, 738 actions incl. 75 loads: no failed action, no crash, memory levels off |

The 2026-10-20 runs (P2 arrows, P2f fireball, the puppet-turn fix, summoned creature
lifetime) are in [RUNTIME-QA-G3TOGETHER.md](RUNTIME-QA-G3TOGETHER.md#results-2026-10-20).

## How to play

Download the mod package from the [releases](https://github.com/GGM1k3y23/g3-together/releases)
(not GitHub's source ZIP) and run `Setup.cmd` (`Einrichten.cmd` in German, `Instalacja.cmd`
in Polish) on both PCs. Both need the same Steam version with Community Patch 1.75.14,
Parallel Universe Patch 1.1.1 and the same mod release (the mod checks at join; 0.6.0 uses
network protocol 56 and does not connect to 0.5.x). The host allows Gothic 3 through the
firewall on UDP port 34567; over the internet the guides describe Hamachi, Tailscale or
ZeroTier, or a forwarded port. Player guides in English, German and Polish are linked from
the repository README. Do not mix it with another co-op DLL in `scripts/`.

## Known issues

Current list: the mod's [known limitations](https://github.com/GGM1k3y23/g3-together/blob/main/docs/PLAYING.md#known-limitations)
and the [v0.6.0-beta release notes](https://github.com/GGM1k3y23/g3-together/releases/tag/v0.6.0-beta).
Not yet played by people or not set up by the bots: the Adanos and Beliar endings, Xardas'
plan, the fifth temple, the Cape Dun uprising, freeing a town mid-game, drive-away quests
and plundering, swimming forward, the trade window's mouse controls. Telekinesis and the ice
and fire spells that put out burning or freezing act on the other player's NPCs only on the
caster's PC.

The list below is the state of the mod's `master` `d0cdb38` (2026-10-21): its player
guide's known problems plus the static findings in [NOTES.md](NOTES.md); some items have
been fixed since. Static findings are marked; they were
read from code and archived logs, not reproduced at runtime.

- Crashes: the game can still crash in a fight or shortly after an enemy dies. The known
  crash in the orc raid (`Entity::AttachTo`, an invalid vtable read) has a guard since
  `5bea542`; others are not ruled out. One host crash executed data inside `Game.dll`
  right after a test-bot teleport (seen once, cause unknown).
- Bows and spells: the other player now sees arrows, bolts and fireballs fly as copies,
  and a fireball that hits counts once. The test bot's arrows often do not reach the
  target in the village, so "an arrow that hits counts once" has 3 green runs of 5, not
  three in a row, and is not shown for bolts.
- Summoned creatures freeze on the other machine when the players move more than about
  40 m apart, because the creature's end is only sent with movement. Its suite passed
  anyway: it does not fail on a frozen copy (fix planned, see NOTES).
- A controlled or tamed NPC (Word of Control, taming) does not follow reliably. Static:
  a host takeover leaves the client's old claim, and a failed takeover leaves a delayed
  Follow task.
- Stun and hypnosis cast by the client go through the host and may not take effect.
- Poison and disease are not fully visible on the other machine, stamina is not sent at
  all, and the experience for a poison kill can be missing.
- Town reputation is shared, but a guard can still blame or chase the wrong player.
  Static: a dialog hold can be overwritten by a second player and released by the wrong
  one.
- Static: restoring a player's own hero adds its skills to those already in the loaded
  campaign instead of replacing them, so a joining client's hero can keep skills the
  host's hero learned.
- The partner's puppet can trail briefly under hits or play a motion imprecisely. One
  case is open: a client running under orc hits, the puppet walks 1 to 2 m behind. The
  rare puppet turn (`ZS_HangAround` after a stagger) is fixed since `a4da3aa`.
- After a join, enemies or companions can act differently on the two machines.
- Using the same campfire, seat or other workplace at the same time can hold a player.
- Sideways combat steps sometimes show as standing on the other machine; mirrored NPC
  positions are off by about 60 cm (median), up to about 3 m.
- How an NPC treats a remote player follows the hero of the machine that simulates it
  (its crimes and fame).
- Companions are matched to players by name after a load; changing the name loses them.
  If the host loads another save mid-game, clients must leave and join again.
- Fixed on 2026-10-19 (`9acdaf5`): orcs the host killed stood upright on the client (the
  mirror got the death task but no death motion). Now 12 of 12 lie; only orcs were tested,
  other species are unknown. The first P2 arrow runs still showed standing orcs because
  their commit `ce6c5c7` predates the fix.
- Not checked: fights, saves and death while transformed; monsters hitting a downed
  hero; robbing a downed hero; the real trade screen and bed dialog; more than two
  players; real internet play.

## Next steps

- More human co-op evenings over a virtual LAN; bugs from them take priority.
- Summoned creatures: send their end reliably, and make `stageCreatureLife` fail on a
  frozen copy.
- Arrows: a test shooter that reaches its target, then three green `stageP2hit` runs in a
  row, also with bolts.
- Identity checks: guards and crimes per player, dialog holds, Word of Control and
  taming, the exact hero restore.
- Check whether `ContinueRoutine` and the other wrapped scripts have direct native
  callers that the script-table wrappers miss; hooking them in place, as for `Shoot`,
  covers both.
- Internet play, then a longer human playthrough, then 1.0.
