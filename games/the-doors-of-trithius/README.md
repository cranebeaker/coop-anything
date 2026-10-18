# The Doors of Trithius (Early Access)

| | |
|---|---|
| Engine | Java (JVM): a custom Java 2D engine in one `DOT.jar` with a bundled JDK 22 |
| Versions tested | Steam 0.6.3b (Steam build 25513945), `DOT.jar` SHA-256 `E933BD53726362D5DC83B85E718CCC6BC728B6B971315E7BFAEE80132D9A540A`, `DOT.exe` SHA-256 `239b396392152b2b72ceb0fe1520561c43675a0d6436bdc4f027d45b31f9ca4e`, Windows 11 |
| Projects | Trithius co-op, a two-player javaagent mod. Not public yet: the release repo follows a clean live test. No directory entry until it has a public source. |
| QA level | automated |
| Last updated | 2026-10-18 |

## Where things stand

Two players can play the same world together over the internet: both explore, fight, loot,
trade, take jobs, rent rooms, craft at stations and walk the world map. The host's game is the
authority and the guest plays with keyboard and mouse like in single-player. The last live
test evening (2026-10-15) ended "ready with friction": six open findings, the worst one a
guest gold loss after being downed. The next build is in its test suite now.

## Existing work

None in this directory (checked 2026-10-18). A wider search (Steam forums, Nexus, the
developer's own channels) is still owed. The game is single-player only and has no netcode.

## How the game works (for co-op)

- **Code and data:** one jar. Game data (`modules/*.dat`) are serialized Java objects inside
  the jar. `javap` from the game's own JDK answers every API question, and a decompiler reads
  the logic.
- **Hook point:** the game runs its rules through an event bus with public event classes, and
  most services hang off one context object (`context.player`, the player's bag, skills,
  abilities, the turn service). A `-javaagent` can subscribe to the bus and call the services
  without changing any game file.
- **Turns:** a turn machine with states (waiting, moving, attacking). Everything else acts
  when the one player ends a turn. On the world map one step runs the start-of-turn procedures
  once per unit of terrain cost (40 to 120), then one more start of turn.
- **Places and saves:** each place (town, dungeon, the world map) loads as a zone in prepare
  steps (load reputation, refresh stats, pass time). A save holds the zone files and the one
  player. Autosaves rename zone files, which can fail when another process holds them.
- **UI:** the game's panes (dialogue, trade, storage, skills) read the one player's state
  directly, so a second player needs its own copy of each pane's inputs.

## What we changed

The mod runs two full games: the host and the guest, each with the mod as a javaagent.

- **Host authority with an impersonation window.** The guest sends commands (move, attack,
  use, talk, trade). The host runs each one with the vanilla code for a single player: it
  swaps the context's player fields (entity, bag, skills, recipes, abilities, hunger,
  weariness, field of view) to the guest's, runs the command, and restores them in a
  `finally`. A static test checks that nothing runs between the swap and the `try`. This
  reuses every game rule (damage, loot, crafting, prices) for the second player.
- **Seat attribution.** Events queued during a guest command are drained inside its window,
  and kill, hit and wear events are credited by the event's own attacker or killer. Without
  this, XP, perks and feed lines landed on the host.
- **A replica, not a second simulation.** The guest's world is a replica built from host
  snapshots. It never simulates. Snapshots use interest management in big places (one place
  held 2448 entities against a 2048 limit).
- **A split clock for players far apart.** In a turn-based game one player's turns must not
  run the other player's monsters. Each creature has one owning clock (the player it targets,
  else the nearer player within 30 tiles, else the host). Close together, the clocks merge.
- **Truthful acknowledgements.** Every command answers applied, no effect, partial or
  refused, with the reason the game would show a solo player.
- **Identity gate.** The handshake carries the game build, the jar hash and the mod build
  id. A mismatched peer is refused, and the message names both ids.
- **Characters travel.** The guest's character is saved on the host per name and can be
  exported to the guest's own game. Files written to disk never carry host-world statuses
  (for example the reputation status the guest wears for shop prices).

## QA

- A gate suite of about 65 PowerShell scripts. Each starts a real host JVM and a real guest
  JVM on one PC, drives both through an in-JVM text-verb driver and reads state dumps from
  both seats. The first gates are a harness self-test and a scorer self-test. One runner
  scores a missing result line as a failure and an overrun as TIMEOUT.
- Live test evenings: AI agents play the host and the guest for 3 to 4 hours with keyboard
  and mouse through a relay with delay, outage and drop modes. An observer agent scores a
  written checklist, with the host doing the same action as the solo control.
- Last full suite: 63 of 63 PASS on build r16 (2026-10-15). The latest live evening (V31,
  build r17, 2026-10-15) is summarized above. Live evenings have run since September 2026.

## How to play

Not released yet. The guest kit is two jars and a join script that the friend puts into
their own Steam install. The host starts the game with the host jar and gives the friend an
address and a secret. Direct IP (port forward or a VPN) for now.

## Known issues

- After the guest is downed it can wake with less gold. The game itself takes gold from a
  downed solo player, and the fix in test removed the mod part of it (a lost feed line).
- While the host has a talk or trade window open, a guest talk or trade waits (fix in test).
- A host restart can leave the guest's On Guard toggle off.

## Next steps

- Run the next live evening on the build now in test.
- A Steam relay sidecar (Spacewar) instead of direct IP: a feasibility study only so far.
- Open the release repo and the directory entry after a clean evening.
