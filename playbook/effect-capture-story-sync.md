# Effect capture: sync a story by recording what scripts do, not by running them twice

A design write-up for co-op on single-player games that tell a story with scripts: quests,
dialogue results, triggers, scripted events. It describes one way to keep every player's
story identical, and how to find and fix the cases where it drifts.

The idea in one line: **only the host runs story scripts. While a script runs, the host records every
primitive change it makes to the world. That list of changes (an "effect list") is sent to the clients,
who apply the changes directly and never run the script.**

The worked example is a Fallout: New Vegas co-op project,
[fnv-coop](https://github.com/cranebeaker/fnv-coop). The design itself is game neutral. Where this page says
"quest stage" you can read "story flag" for your game.

Related pages: [how-coop-mods-work.md](how-coop-mods-work.md) (the general shape of co-op mods),
[networking.md](networking.md) (sync models and transports), [network-chaos-testing.md](network-chaos-testing.md)
(testing under a bad link), [actor-ownership.md](actor-ownership.md) (who simulates which actor),
[desync-hunting.md](desync-hunting.md) (finding where copies diverge),
[fuzzing-coop-sync.md](fuzzing-coop-sync.md) (game-free testing of a sync core).

## 1. The problem

A single-player story game keeps its progress in a few kinds of state: quest or chapter stages, objectives,
global variables, "this character is dead or disabled", items given, reputation, and many small script variables.
Scripts change that state when something happens: the player finishes a conversation, walks into a trigger,
picks up an item.

When you add players, every copy of the game has its own scripts. If you do nothing, each copy runs
them on its own and the stories drift apart. The two obvious fixes both fail.

| Obvious fix | What goes wrong |
|---|---|
| **Replay the trigger on every client.** When the host advances a quest, tell each client to advance it too, so its own scripts run. | The result scripts run N times. Items and rewards are duplicated. Characters spawn twice. Scripts that read "the player" read the wrong player. The random rolls inside a script come out different on each machine. |
| **Send only the new state number.** Tell clients "quest is now at stage 40". | The side effects of reaching stage 40 never happen on the client: the door is not unlocked, the character is not enabled, the item is not added, the next quest is not started. The number matches but the world does not. |

Skyrim Together Reborn and TES3MP meet the same problem and solve parts of it. TES3MP keeps the journal, topics and
factions on the server and lets scripts run locally. Skyrim Together lets the party leader advance quests. Both found
that a story needs one authority and a shared log.

## 2. The idea

```
 host machine                                        client machine
 ------------                                        --------------
 story script runs  --> capture hooks record        apply effects directly
 (the only place)       each primitive change       (no script is run)
                        |                                  ^
                        v                                  |
                 one ordered, numbered transaction ---- network ----> client applies in order
                 (the effect list)                     acknowledge, resend, repair
```

What this buys you:

- **No duplication.** The script ran once. Rewards were created once, then described as "add item X, count 1" to each
  recipient.
- **No missing side effects.** Every change the script made is in the list.
- **Random rolls are made once.** If a script picked a random item, the list says which one. Clients never roll.
- **A perfect test fixture.** Every transaction is data. You can record it, replay it into a fake world, compare it with
  single-player, or shrink it into a regression test.

## 3. Three kinds of authority

Decide who is the source of truth for each kind of state, and write it down.

| Kind | Authority | Examples |
|---|---|---|
| **Story** | The host, always | Quest stages, objectives, dialogue results, globals, shared reputation, endings, scripted world changes |
| **Actors** | One owner per actor or area, with an **epoch** (a counter that grows on every ownership change) | Live NPC movement, AI, combat. Only the current owner at the current epoch may publish an actor's state. Everyone else shows a remote-driven copy. See [actor-ownership.md](actor-ownership.md) |
| **Player** | Each player owns their avatar | Position, animation, own inventory, own hits as attacker |

This page is about the first row. The other two follow the same rules: one authority, numbered messages, nothing applied twice,
nothing stale wins.

## 4. The effect vocabulary

An effect is one **primitive** change to the world, small enough to apply directly without a script. Pick a vocabulary that covers what
your game's scripts can do. The New Vegas project has about fifty effect kinds, added as it surveyed what the scripts do.
Group them like this:

| Group | Examples (game neutral names) |
|---|---|
| Progress | set stage, set objective flags, complete quest, fail quest, start quest, stop quest |
| Variables | set global, set a script variable on an owner (player or object) |
| World objects | enable or disable a reference, move a reference, lock or unlock, open or close, destroyed, set texture or angle |
| Actors | kill, resurrect, spawn, despawn, change faction rank, add or remove a topic or note |
| Items | add or remove item for a recipient, add or remove in a container, remove all |
| Player numbers | change a stat by a delta, change reputation by a delta, change karma by a delta, add or remove a perk |
| Environment | set weather, set time scale, advance time |
| Session | party tether, barrier arrive or release, suppress saving, request a save |
| Events (not state) | forced dialogue, slideshow end, save request |

Design rules that paid off:

- **Deltas for numbers, not absolute values.** "Add 100 experience" is exact, integer deltas keep every copy bit identical, and the
  sequence numbers of section 5 make sure each delta is applied once. Floats appear only for values that really are floats (globals, positions)
  and must be finite.
- **A `before` value on changes that can be undone.** When an effect carries what the value was before, the system can
  revert it, and the desync healer can tell a real difference from noise.
- **Each effect names a recipient.** "Default" means the player who caused it, "all" means everyone, or a mask of
  players. Rewards, quest items and per-player numbers are routed by policy, not by whoever happens to be the host.
- **Each transaction carries a cause.** Quest result, dialogue result, console, loot, combat, script, intent. It is
  invaluable when you are asking "why did the copies diverge?".
- **Identify things by name, not by load-order number.** Send (plugin name, local id), never a raw numeric id that depends on
  the other player's mod list. Intern the names once per message so they stay small.
- **Adding or reordering effects is an incompatible change.** Append new kinds at the end and bump a version (see section 12).

## 5. Transactions, the story log and acknowledgements

All the effects captured while one script runs are sent as **one atomic, ordered transaction**.

- The host gives each committed transaction a dense, growing sequence number and appends it to a **story log**.
- Clients apply strictly in sequence order, buffer a gap, ignore duplicates, and acknowledge the highest contiguous number they applied.
- The host resends unacknowledged transactions with exponential backoff. A live send must never refresh the retransmit timer, or
  steady commits starve the resends.
- A client that is far behind gets catch-up paced by its own acknowledgements, not a flood of live sends.
- The log is also how late join and reconnect work: replay it, or send a snapshot if the log has been compacted.

This layer is reliable on top of an unreliable transport. That is deliberate. It means the protocol survives loss, duplication,
reordering and corruption by itself, and you can prove it with [the chaos box](network-chaos-testing.md).

## 6. Epochs: stopping old messages from winning

An epoch is a counter that changes when a world is replaced. Messages from another epoch are stale and dropped. Three are useful:

| Epoch | Changes when | Protects against |
|---|---|---|
| **World epoch** | The host loads an older save (history rewinds) | Old transactions being applied into the new timeline |
| **Intent epoch** | A client's own world is replaced (load or rejoin) | A client's old request number being confused with a new one |
| **Ownership epoch** | An actor or area changes owner | A previous owner's late update overwriting the new owner's state |

Also use a **session id** chosen by the client on every connect and echoed in every later message, so stale datagrams from a previous
connection are ignored. Skyrim Together Reborn hit a mass-duplication bug from stale ownership state, which is the strongest argument for
checking epochs on every apply.

## 7. Clients: never run the script, never apply your own echo

On a client:

- **Gate the story scripts.** The local copy of the game still contains the quest scripts. Suppress them (hook the point where
  the script engine would start them) so only the host authors story changes.
- **Echo guard.** While applying a remote effect, set a flag so your own change hooks do not see the change and send it back.
- **Do not run the effect's side scripts again.** Applying "set stage 40" must set the number without firing the script that would
  normally run on a stage change. Some engines have a "set without running result scripts" path; others need a hook.
- **Per-machine scripts** (achievements, fades, rumble, UI) should still run locally. Use an allow-list of script names that are
  allowed to run on every machine, and make sure anything they change locally is not sent as shared state.

## 8. When a client causes the story to move: intents

A client player walks into a trigger, activates an object or picks something up. The script that reacts is bound to the
**host's** player, and it only runs on the host. So:

- The client **detects** the interaction and sends an **intent**: "I activated object X". It applies nothing locally except the
  user interface part (the menu opens).
- The host runs the interaction **on the client's behalf**, in an "acting player" context: while that block runs, every reference to
  "the player" in the script resolves to the client's avatar. The captured effects are tagged with the client as the origin, so
  player-targeted effects (items, XP) go to the right person.
- The result comes back as a normal transaction.
- Intents are numbered inside an intent epoch. The host runs each number exactly once, in order, and acknowledges it. The client resends until
  acknowledged and after a reconnect. A resend never runs the action twice.

Kinds that proved necessary: applied-effects (the client already did it and tells the host), activate, trigger enter, pick up, scene pass, and
the client's cell and proximity reports.

## 9. Merge rules and policy

When two sources can write the same thing, pick a rule by type and write it down:

| Type | Rule |
|---|---|
| Stage numbers | Maximum wins (a quest never goes backwards by accident) |
| Flags | OR them (once true, stays true) |
| Kills | Union |
| Globals | Host wins |
| Mutually exclusive outcomes | First committed wins (a rule kind for "these stages exclude each other") |
| Random picks (leveled lists, rolls) | Made once on the host, then sent as the real result |

Decide the **share policy** with the owner of the project before you write code, and keep it as data with one flag per category
(journal, topics, faction rank, reputation, karma, kills, globals, quest rewards to all). The New Vegas project decided:

| Question | Decision |
|---|---|
| Karma and faction reputation | Shared. The party is one character in the story |
| Quest items and quest rewards | Every player receives them |
| Dialogue | The initiator chooses. Everyone sees the lines and the result |
| Endings and cinematics | The host decides. Everyone is locked and moved together |
| One-way transitions (a point of no return) | A party barrier first: everyone is present and agrees. Nobody is left behind |
| Companions | Owned by the recruiter. Only the owner commands them |
| Casino winnings, own storage | Per player |

These are examples of the kind of decision to make, not rules for your game.

## 10. Join, rejoin and baselines

A player who joins late or reconnects must end up with the same world.

- **Fresh join:** the host sends a snapshot of the shared state, then the log from that point.
- **Reconnect in the same session:** the client says "I have everything up to number N". The host resumes from N. Missed transactions are
  replayed once, never twice.
- **Join with the client's own save:** the client's world came from a different file, so it differs from the host in unknown ways. Use a
  **baseline transfer**:
  1. The client sends a summary: a (key, hash) for every tracked world entry, in chunks.
  2. The host compares with its own state and streams back a diff: effects to force-set, and keys to revert that the client has and the
     host does not.
  3. The client applies the diff with "force" semantics and acknowledges each chunk. Chunks are safe to repeat.
  4. Hash a canonical form of floats (convert to 32 bit and round) so harmless float noise between machines does not create fake differences.
- **Host loads an older save:** the world epoch changes. The host tells clients to **rebase**: dump the world again and rejoin with a baseline.
  Some flags can never be un-set by a rewind. Document these.
- **Per-player rewards** (items and XP earned so far) are not part of a shared world diff. Keep a ledger per player on the host and
  send it on join, and decide how a restarted client whose own save already holds those items avoids getting them twice.

## 11. Detect and heal desync

Prevention (one authority, epochs, idempotent ids, no script reruns, shared random picks) removes most drift, but not all. Add a safety net:

1. **Digest.** Each client periodically hashes its real world state into a fixed number of **shards** (the New Vegas design used 128: 8
   entry classes times 16 key buckets) at a known sequence number. Also send a digest after events that often cause drift
   (barriers, scenes) or when the host asks.
2. **Compare.** The host compares with the same sequence number. For each mismatching shard it asks the client for the entries.
3. **Repair.** The host answers with force-set effects and revert keys, skipping any key touched since the digest (so a repair never
   undoes a newer change). A repair applies only if the epoch matches.
4. **Log.** Record which values differed and on whose side. A repeated mismatch on the same key is a bug, not noise.
5. **Count.** A dashboard of repairs per minute by category. The target in scripted tests is zero.

The healer is a safety net, not the plan. If the repair count is high in your tests, your first line of defence is broken. See
[desync-hunting.md](desync-hunting.md) for tracing a divergence to its first cause.

## 12. The wire format, briefly

The details are game specific. The rules that matter:

- A magic number, a version byte, a message type, a body, and a checksum at the end. Drop anything that does not decode.
- The hello and reject messages use a version-independent envelope and only ever have fields appended, so two incompatible builds can still
  tell each other why they cannot play together.
- Bump the wire version for any incompatible byte change, including a new or reordered effect kind. Bump the protocol version too, and also
  when behaviour changes without bytes changing (a merge rule, a hash canonicalisation).
- Compare at join: protocol version, build, a hash of the rules file, a hash of the mod list and, for a baseline join, a hash of the content.
  A mismatch gets a readable rejection, not a corrupt session.
- Treat all input as hostile: bound every count, reject non-finite floats, never trust a slot number from the sender.
- One connection, several channels: put a channel byte first, so story, dialogue and other adapters share the transport.

## 13. How to test it

Develop **game-free first**. The sync core is plain code that does not know the engine:

1. A **simulator** with N fake clients, a virtual clock and a lossy link (use the model from [tools/chaos](../tools/chaos/README.md) so the simulator
   and real-copy tests share the same bad network).
2. **Round trip and fuzz** tests on the wire format (truncation, bit flips, hostile counts).
3. **Invariants** checked in every run: copies converge, nothing applied twice, one owner per epoch, no duplicate spawn, no stale write wins,
   no story script ran on a client.
4. **Mutation twins.** For each protection, a build with it turned off must fail a test. Otherwise a passing test may test nothing.
5. **Single-player differential.** Play the same scenario in single-player and in co-op. The shared state in co-op must follow the single-player
   trajectory at each checkpoint.
6. **Real copies** last, through the relay, to confirm what the simulator predicted.
7. **Protected expectations.** Keep golden files that agents cannot loosen without the owner reviewing the change.

For the loop, see [automated-qa.md](automated-qa.md) and [fuzzing-coop-sync.md](fuzzing-coop-sync.md).

## 14. Worked example: New Vegas

The New Vegas project applies this design to a closed engine through a script-extender plugin. A few concrete points:

- **What the host captures.** While a quest stage result script, a dialogue result script or an object script runs, hooks record the
  stage and objective changes, global writes, enable and disable of references, item add and remove, actor value, reputation and karma
  changes, quest start and stop, moves, kills, spawns and script variable writes. The list is committed as one transaction.
- **Why not just replay `SetStage`.** A stage result script that adds items and enables a character would run again on every client. With
  capture, the host runs it once, and each client gets "set stage", "add item to every player", "enable character".
- **Per-player routing.** Quest XP and caps go to every player (the full reward to each). Leveled-list rewards are rolled once on the host and the
  real items are sent. Karma and reputation are shared.
- **Client interactions.** A guest who activates a terminal or walks into a trigger sends an intent. The host runs the interaction with the guest's
  avatar as the acting player.
- **Session machinery.** About fifty effect kinds, a story log, intents with epochs, baseline transfer for a client that loads its own save, a
  digest of 128 shards for desync repair, and a dialogue channel next to the story channel in the same connection.
- **Policy.** Karma and reputation shared, quest items and rewards to everyone, the dialogue initiator picks, endings and one-way transitions need
  the whole party, companions belong to their recruiter.

What was left out of the shared story on purpose: per-machine scripts (achievements, fades), which run locally from an allow-list.

## 15. Limits to expect

- **Sticky one-way flags.** Some engine flags (a completed or failed quest) cannot be un-set by a rewind. A host loading an older save cannot
  fully rewind a client that already completed a quest. Document it.
- **Rewards after a restart.** A client restarted with its own save that already contains earlier rewards may receive them again, unless the
  client can tell and treats ledger effects from a snapshot as "give only if missing".
- **Cells that only one machine has loaded.** A host simulates only the areas loaded around it. Players in other areas need a tether, an area
  owner or a stand-in. See [actor-ownership.md](actor-ownership.md).
- **Engine thread safety.** Apply effects on the engine's main thread.
- **Coverage.** Capture only sees what you hooked. Survey the scripts (dump every command the story scripts use) and add effect kinds until the
  survey is covered. Unknown commands should be logged loudly, not ignored.

## 16. Checklist for a new game

1. Find out how the story state is stored and which script commands change it. Dump the scripts or use a decomp.
2. Choose the effect vocabulary from the commands that the story scripts actually use. Survey, do not guess.
3. Find the hook points: where a script starts, where each primitive command writes. Record effects at the write, not at the script call.
4. Find a "set without running side scripts" path for applying on clients, or hook the side scripts and suppress them.
5. Decide the share policy with your owner. Write it as data.
6. Write the sync core without the engine: effects, transactions, log, epochs, intents, baselines, digests, wire format.
7. Test it in a simulator with a lossy link, with invariants and mutation twins.
8. Hook it into the game. Test with two real copies through [the chaos relay](network-chaos-testing.md).
9. Run the single-player differential and the digest repair counts, and write down the numbers.
10. Write the limits in section 15 for your game in your game's README.

## Sources

- [Skyrim Together Reborn](https://github.com/tiltedphoques/TiltedEvolution): open source. Party leader advances quests, owner per actor, ownership epochs.
- [TES3MP](https://github.com/TES3MP/TES3MP): open source. Journal, topics and factions on the server, scripts run locally, cell authority.
- [VaultMP](https://github.com/foxtacles/vaultmp): open source player puppets for the Fallout games.
- [Gaffer on Games](https://gafferongames.com/): snapshot interpolation, state synchronisation, reliability on UDP.
- [fnv-coop](https://github.com/cranebeaker/fnv-coop): the New Vegas project this design comes from.
