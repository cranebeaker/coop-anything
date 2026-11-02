# Automated QA: prove co-op works without anyone playing

The QA level is the most important field in the directory. "It works" means a script
drove real game clients, checked the shared state, and wrote PASS. This page is the
policy and the techniques that made it work on four very different engines.

## Policy

- **Don't ask a human to playtest.** The game drives itself, checks the result and logs
  PASS/FAIL. People playing it is a bonus, not the test plan.
- **"Needs a human playtest" is a gap, not a step.** If something can't be automated
  yet, track it as an open item in your notes or an issue and build the missing piece: an in-game bot, a
  second client, a mock service.
- **The bot lives in the game's own scripting or code**, not in fake mouse or keyboard
  input. It does what a player would do (story flags, dialog, level changes, inventory)
  and asserts the outcome.
- **Test co-op with co-op.** Run 2 or more real game clients and compare shared state
  between them.
- **Use the game's own logs and screenshots**, never desktop capture. Desktop capture
  grabs whatever else is on the owner's screen.

## The harness shape that keeps working

1. **Isolated test installs.** Copy the store install (or hardlink it) once per test
   slot. Never touch the owner's install, INIs or saves. Configure windowed, no intro,
   no mouse capture, software renderer if two windows fight over the GPU.
2. **A launcher script** starts a host plus N clients on one PC, waits for the test to
   finish, collects every log and prints one line: `RESULT: PASS` or `RESULT: FAIL`.
3. **A test bot inside the game** with steps: wait until N players are in, do a thing
   (through the same RPC a player's input would use), wait, assert, log.
4. **A suite list** (one line per test: name, map, clients, timeout) and a runner that
   writes a summary. Rerun the whole suite after every merge.
5. **One port and one install per parallel agent**, so several agents can test at once
   without killing each other's games.

Ready-made pieces for this shape are in [tools/](../tools/README.md): a game slot lock and
warm pool for the copies, JSON scenarios, a bot runner with co-op playbooks, a chaos relay
for lag and loss, a flight recorder for desyncs, a fuzzer for the sync core and an
animation checker.

### Examples

| Game | Engine | How the bot works | Oracle |
|---|---|---|---|
| Deus Ex (HX) | Unreal Engine 1 | A test actor selected by URL option (`?HXFixTest=Class`) runs steps on the listen server; a per-player driver actor makes real clients run console commands so client to server RPCs are exercised | Server state after the RPC, client reports sent back over RPC |
| Dungeon Keeper (KeeperFX) | Custom C, lockstep | `-archontest` flag: a bot inside the exe writes input packets like a human | Per-turn checksums on both sides, 0 mismatches, plus gameplay asserts |
| Fallout New Vegas | Gamebryo | xNVSE plugin serving JSON-RPC on a named pipe (`exec`, `waitFor`, `state`) | World snapshots split into shared and per-player state, diffed between clients |
| Gothic 3 (G3 Together) | Genome | G3 Together: the bot is compiled into the mod and active only when a flag file exists; it reads numbered commands from a file and calls the game's own scripts (quest manager, hit judgement, AI tasks, save). Menus driven by focus plus Enter posted to its own window. Two real clients per test pair, two pairs for two agents. | State read on both clients (quests, NPC lists, hit points, inventories, motions), invariant monitors at the end of each suite, the owner's real saves hashed before and after |
| Graveyard Keeper (Back From The Grave) | Unity 2020.3 (Mono) | A test-only BepInEx plugin, inert without `-coopqa`, fakes Steamworks.NET with Harmony and runs step lists that call the same methods as the menu buttons and the hotbar; a Python launcher starts two copies on one PC | Both bots write PASS; state is exchanged between the bots (positions, clocks) and checked on both sides; the host is the control for player actions |
| STALKER (xrMPE) | X-Ray | Test-only script archive hooked from a space restrictor's logic | In-engine checks written to its own log file |
| The Witcher 3 (Witcher Online port) | REDengine 3 | Test-only script mod plus a test ASI that registers natives (env, log, quit) through WitcherNativeBridge; it skips movies, loads a save and reports each second | Each client compares the remote player's ghost with the networked position; every bot must log PASS |
| Outlast (OpenOL) | Unreal Engine 3 | No bot yet. Real clients started straight on a map by URL; a checkpoint is loaded by a script through the mod's debug overlay; a scripted third peer joins the relay room and injects packets; walking only when unavoidable | A logging UDP proxy between clients and relay decodes every packet and writes the shared state (positions, hp, enemy mode, doors, triggers) to a JSON file; client logs show what each side applied |
| The Doors of Trithius | Java (JVM) | A text-verb driver inside each game JVM (the javaagent tails a command file and acks every verb with ok, no-effect, partial or fail). About 65 gate scripts start a real host and a real guest JVM on one PC over TCP. AI player agents then play whole evenings with keyboard and mouse through a relay with delay, outage and drop modes, while an observer agent scores a checklist | Host-authoritative state dumps from both seats, per-seat logs, and the host doing the same action as the solo control |

## Rules learned the hard way

- **Check that the test can fail (mutation twins).** Turn the fix off and run the test:
  it must fail. Several "passing" tests were testing nothing until this check.
- **Hollow passes.** A test that ends early, or a mode that silently fell back to single
  player, still prints PASS. Assert the mode you think you're in at the start (number of
  players, flags, map), and assert that the step you care about actually happened.
- **A crash at exit is still a crash.** Unreal Engine 1 games show a "Critical Error"
  box and never flush their log when they crash, so "no error in the log" proves nothing.
  The launcher must check whether each game window closes when asked, and read the
  dialog text when it doesn't (Win32 `EnumWindows` plus `WM_GETTEXT` on the children).
- **Check too early, fail falsely.** Poll with a timeout instead of checking once.
- **Instruments lie.** A capped FPS counter hid real drops; a desync log printed turn 0
  for every mismatch. Fix the instrument before trusting it.
- **Teleports fail silently.** A test put a player inside a wall and then "found" a bug.
  Assert the precondition (player really is inside the trigger) before asserting the
  result.
- **A death in a test changes the test world.** In Shadow of War a test copy whose player
  died autosaved a different world: a large enemy came to the test start and its fire hurt
  the player and the stand-in in later checks. A health invariant (no player or stand-in may
  lose health during a check) is what showed it. Before a suite, assert that no living
  stranger is near the test start, and restore the test copy's save from a backup when one
  is ([NOTES](../games/shadow-of-war/NOTES.md), session as).
- **Alternate the fix with the control when the symptom varies.** In Shadow of War an
  animation glitch ran at one of two speeds at random (5 or 56 restarts in 6 s with nothing
  changed). Two single before/after comparisons looked like fixes and were chance. Run each
  setting several times, interleaved with the control, and look at the spread before
  believing a difference ([NOTES](../games/shadow-of-war/NOTES.md), session bb).
- **Check streamed-object ownership.** Validate current-area identity before reusing a trigger; stale registrations can survive a load ([Friend or Foe](../games/spider-man-friend-or-foe/NOTES.md)).
- **Low memory kills long runs.** Run tests one at a time on a busy PC, in short
  batches, and clean up any game process you started.
- **Clean up.** Remove test-only files from anything you ship and compare the shipped
  files byte for byte afterwards. Servers that checksum packages will reject players
  with leftovers.
- **Test the scorer, not only the test.** Make the first gate of the suite a scorer self-test: it feeds known bad output (a missing result line, a timeout, a crash text) to the real scorer function and must see FAIL. Score a gate that printed no result as a failure, and a gate that ran out of time as TIMEOUT, not FAIL (The Doors of Trithius).
- **Measure the control before you call it a mod bug.** Let the host do the same action alone. Several co-op "bugs" were the single-player game's own rules: a downed player loses gold in vanilla, and a world-map rest never lowers weariness (The Doors of Trithius).
- **Bots prove mechanics; agents that play like people find the rest.** Gates drive verbs, so they never exercise the join window, focus, mouse paths or a menu a person opens. An evening where AI agents play both seats with keyboard and mouse found the join-click, dialogue-pane and shop-window defects that every gate missed. Keep the host as the control and have an observer agent score a written checklist (The Doors of Trithius).
- **Invariant monitors next to the asserts.** At the end of every two-player suite compare the shared objects on both machines a few times, seconds apart, and report what holds in every sample: simulated on both machines, dead on one only, far apart, different hit points, a mirror running its own AI, a body with no motion. Report only at first, and give the monitors a self-test that breaks sync on purpose and must be caught (Gothic 3).
- **A flake from game behaviour is fixed in the action, not the assert.** A hit test failed because the NPC parried. The test now swings again, up to three times, as a player would; the assert stays strict (Gothic 3).
- **Check what the other player sees.** A motion name set on a body is not a motion running; measure "is it running". Check also that enemies pick a remote player by themselves; forced attacks hid that they never did (Gothic 3, first human look).
- **Count dwell and timeouts in real time when the game can fast-forward.** A crawl that skipped cutscenes by raising the engine timescale measured its per-map dwell in game time, so the joiner got about 80 frames on some maps and they still passed. Time every test step with the wall clock, and end a cutscene that stays stuck (Jedi Outcast).
- **A connected client is not a live client.** A crashed joiner stays "connected" on the host until the server times it out, and a joiner carried over from the last map counted as joined while still loading the new one. Count a client only while a value it alone advances (its last command time) keeps moving (Jedi Outcast).
- **Match errors exactly.** A scorer that killed the host on "ERROR" also matched a harmless coloured map warning (`^1ERROR: ref_tag ... invalid target`) and a GL extension name. Strip timestamps and colour codes and match the engine's real error prefix at line start, case-sensitive (Jedi Outcast).
- **Own processes positively.** Stop only a process your run started (its PID, or a marker on its command line). A blanket kill by image name once destroyed a live two-player session (The Doors of Trithius).

- **A zero is an answer.** A poll helper that returns the measured value treats `0.0` as "not yet" in most languages, so a perfect match (gap 0.0) timed out. Return a tuple or a flag, not the value (Skyward Sword).
- **Startup is a gate.** If the games never come up, the runner must still print `RESULT: FAIL`; one run crashed with a traceback instead, which a scorer may read as no result at all (Skyward Sword).
- **Savestates pin the build.** On emulators a savestate holds the mod's code as it was loaded. Write a build number where the bot can read it and fail the boot gate when it does not match, or you test yesterday's code (Skyward Sword).
- **Give the bot dev commands for states it cannot reach.** With no button input and an early save, the bot could not make an enemy switch animation; a one-shot "play animation N on enemy X" field in the mod's mailbox, ignored in normal play, made the check possible. Keep such commands out of player-facing paths (Skyward Sword).
- **Validate the checksum itself.** Exclude padding and inactive history only with native control-flow evidence; retain mutation checks for active fields ([Friend or Foe](../games/spider-man-friend-or-foe/NOTES.md)).
- **Respect fixture boundaries.** Inject controls before the measured binding stage and end special assertions on schedule; keep general invariants running ([Friend or Foe](../games/spider-man-friend-or-foe/NOTES.md)).
- **Let a bot play the story and compare quests.** Per-feature tours only cover what you thought of. A story autopilot on the host (answer choices, press whatever the story has armed, walk into and look at story triggers) with one check, "every client holds the host's active quests, every 30 s", found a desync in a chapter that every feature tour had passed. Report how far the bot got in each chapter: a chapter where it took 0 to 2 actions proves little ([MiSide](../games/miside/README.md)).
- **Measure leaks with memory held after a full collection.** .NET's `GCMemoryInfo.HeapSizeBytes` includes garbage not yet collected, so it rises with the allocation rate and flagged long two-player runs as leaking. `GC.GetTotalMemory(true)` once a minute showed a flat 11 MB ([MiSide](../games/miside/README.md)).

## Testing online features on one PC

- **Second Steam account:** you can't run two on one PC. Use a mock `steam_api.dll`,
  see [networking-steam-spacewar.md](networking-steam-spacewar.md). In Unity Mono
  games that use Steamworks.NET, patch the managed wrapper with Harmony instead
  ([same page](networking-steam-spacewar.md#unity-mono-games-mock-steamworksnet-not-the-dll); Graveyard Keeper).
- **Internet conditions:** use the engine's packet emulation if it has one (Unreal
  Engine 1: `PktLag=150`, `PktLoss=5`, `PktOrder=1`, `PktDup=1` console commands on host
  and clients) and rerun the suite with it on.
- **A friend without the mod:** keep a second install without your package and check
  the game downloads it from the host (Unreal Engine 1 `ServerPackages`).
- **Several copies of single-instance games:** see the Nucleus Co-op techniques in
  [methods.md](methods.md) (renamed mutexes, FindWindow hooks, hardlinked installs).

## What to write in your game's QA section

- QA level reached (see [data/SCHEMA.md](../data/SCHEMA.md)).
- The command that runs the suite and its last summary (pass counts).
- **Put a decoding proxy between clients and the relay.** It needs no change to the mod, shows what each side really sent and received (one line per state change), and gives bots a state file to read instead of screen scraping. Make the bot stop when the state file is stale (> 3 s) so a crashed proxy does not turn into a blind walk (Outlast, OpenOL).
- **Inject, don't walk.** A scripted peer that joins the room and sends the protocol's own trigger packets (a sweep over `SeqEvent_Touch_0..N`) tested story sync in minutes and needed no one in the game. Map the receiver's log lines back to each send by timestamp (Outlast, OpenOL).
- **Start from the state you want.** Launch clients straight on a map from the command line and load a checkpoint with one command; survey the checkpoints once (spawn point and nearby enemies) and pick the one that already puts both players next to what you test. Walking to it is the slowest and flakiest step (Outlast, OpenOL).
- **Force log flushing.** Some engines write the log in large delayed chunks, so a test that reads the log right after an action sees nothing and reports "no effect". Check the engine's flush switch (Unreal Engine 3: `-FORCELOGFLUSH`) before trusting an empty log (Outlast, OpenOL).
- **A walking bot needs a progress rule, not a movement rule.** "Moved less than N" does not catch bouncing between two obstacles. Track the best distance to the target; no new best for two bursts means stuck, then escalate detours and give up with a message. Stop on damage or a special move instead of treating a grab as a wall (Outlast, OpenOL).
- **No desktop capture, even for your own debugging.** A full-screen grab recorded a Steam window and friend list. Capture only the game window, or use the game's screenshot command, and don't commit pictures unless you checked every pixel (Outlast).
- **A twin catches test bugs too.** The first far-peer trigger test passed on the unpatched relay as well, because an earlier case had used up a one-shot trigger. Running the same test against the unfixed build is what showed it. Use repeatable triggers/objects for before/after comparisons and order the cases so one cannot consume another's precondition (Outlast, OpenOL).
- **Give a check its own precondition check.** "The other player takes no damage" proves nothing if the enemy never attacked; add a separate test that asserts the attack happened (a line in the game log) and score the pair together (Outlast, OpenOL).
- **Watch for the engine's crash box inside every assertion.** A hung or dead client shows up as a Windows error dialog owned by the game process (`EnumWindows`, class `#32770`, text of the children); turn it into an ERROR with the dialog text and stop the run, instead of letting the following checks time out (Outlast, OpenOL).
- **Edit-and-restore config in a context manager.** Tests that need a different ini value (a relay's room code, a sync switch) change it in `with IniOverride(...)` that always restores the file, and compare the file with a backup at the end of the run (Outlast, OpenOL).
- **Say SKIP, not PASS, when the case never happened.** If a test needs an event that may not occur in a run (the other player dying first), print `SKIP` with the reason and do not fail the run; a silent pass would hide the gap (Outlast, OpenOL).
- **The mod's own debug overlay can be the control channel.** A Dear ImGui overlay with a checkpoint list, spawn panel and relay start button let a script drive the game with a few keypresses and clicks at fixed window offsets instead of walking and menus; keep the overlay layout assumptions in one place and re-check them after a mod update (Outlast, OpenOL).
- **Test what a late joiner receives, without the game, and look for dead code.** A relay can have a complete snapshot feature (store, queue, replay) whose last step is never called from the loop the shipped binary runs. Outlast's standalone relay sent late joiners nothing for pickups and other players' matinee and hero state because the CLI main loop skipped the drain call; the game hid it by asking for doors and enemies itself. A fake peer that sends state, a second peer that joins afterwards and lists what it receives found it in minutes; a debug build with two log lines in the queue and drain functions showed which step was missing. Add a blind-mirror twin so the check is known to fail.
