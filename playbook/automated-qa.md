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

### Examples

| Game | Engine | How the bot works | Oracle |
|---|---|---|---|
| Deus Ex (HX) | Unreal Engine 1 | A test actor selected by URL option (`?HXFixTest=Class`) runs steps on the listen server; a per-player driver actor makes real clients run console commands so client to server RPCs are exercised | Server state after the RPC, client reports sent back over RPC |
| Dungeon Keeper (KeeperFX) | Custom C, lockstep | `-archontest` flag: a bot inside the exe writes input packets like a human | Per-turn checksums on both sides, 0 mismatches, plus gameplay asserts |
| Fallout New Vegas | Gamebryo | xNVSE plugin serving JSON-RPC on a named pipe (`exec`, `waitFor`, `state`) | World snapshots split into shared and per-player state, diffed between clients |
| STALKER (xrMPE) | X-Ray | Test-only script archive hooked from a space restrictor's logic | In-engine checks written to its own log file |
| The Witcher 3 (Witcher Online port) | REDengine 3 | Test-only script mod plus a test ASI that registers natives (env, log, quit) through WitcherNativeBridge; it skips movies, loads a save and reports each second | Each client compares the remote player's ghost with the networked position; every bot must log PASS |
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
- **Low memory kills long runs.** Run tests one at a time on a busy PC, in short
  batches, and clean up any game process you started.
- **Clean up.** Remove test-only files from anything you ship and compare the shipped
  files byte for byte afterwards. Servers that checksum packages will reject players
  with leftovers.
- **Test the scorer, not only the test.** Make the first gate of the suite a scorer self-test: it feeds known bad output (a missing result line, a timeout, a crash text) to the real scorer function and must see FAIL. Score a gate that printed no result as a failure, and a gate that ran out of time as TIMEOUT, not FAIL (The Doors of Trithius).
- **Measure the control before you call it a mod bug.** Let the host do the same action alone. Several co-op "bugs" were the single-player game's own rules: a downed player loses gold in vanilla, and a world-map rest never lowers weariness (The Doors of Trithius).
- **Bots prove mechanics; agents that play like people find the rest.** Gates drive verbs, so they never exercise the join window, focus, mouse paths or a menu a person opens. An evening where AI agents play both seats with keyboard and mouse found the join-click, dialogue-pane and shop-window defects that every gate missed. Keep the host as the control and have an observer agent score a written checklist (The Doors of Trithius).
- **Own processes positively.** Stop only a process your run started (its PID, or a marker on its command line). A blanket kill by image name once destroyed a live two-player session (The Doors of Trithius).

## Testing online features on one PC

- **Second Steam account:** you can't run two on one PC. Use a mock `steam_api.dll`,
  see [networking-steam-spacewar.md](networking-steam-spacewar.md).
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
