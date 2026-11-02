# Co-op sync bug classes

Every co-op sync layer meets the same handful of bugs, on every game. This page names them, says how each one looks
in a real game, which oracle catches it, and which mutation twin in the example target switches it on.

A **mutation twin** is a copy of a correct piece of code with one bug put back in on purpose. The oracles are only
trustworthy if they fail on every twin. `python -I tools/fuzz/run_tests.py` checks that all of them are caught.
Run one yourself: `python -I tools/fuzz/run.py --bug two_owners --count 100`.

## The oracles

| Oracle | What must hold | Needs from your target |
|---|---|---|
| `converge` | After the link heals and traffic stops, every connected machine has the same things with the same values as the host, and the per-category digests are equal | `view`, `digest`, `connected`, `quiescent` |
| `double_apply` | The same message (same id) is never accepted twice on one machine | an `idem` on every apply log entry |
| `multi_owner` | Each actor has one owner per ownership epoch, and epochs never go backwards | ownership log (`logOwn`) |
| `stale_applied` | Nothing out of date is applied: an older ownership epoch, an update sequence that did not advance, an update from someone who is not the owner, a request made before the last host load | apply log with `epoch`, `seq`, `origin`, `worldEpoch` |
| `echo_adopted` | A machine never applies the echo of its own update as if it were new news | apply log with `origin` |
| `dup_thing` | No machine has two things with the same net id, or two copies of the same identity (spawn site slot, original item) | `view` with `ident` |
| `repairs_after_quiescence` | When everything is quiet, the number of repair or resync actions stays at zero | `repairs` |
| `not_quiescent` | After the link heals, traffic really stops within a time limit (nothing resends forever) | `quiescent` |

`setup` is not an oracle: it is reported when a scenario names a target or a bug the build does not have.

## The catalogue

| # | Class | Twin (`--bug`) | Caught by |
|---|---|---|---|
| 1 | Double apply | `no_idempotency` | `double_apply`, `converge` |
| 2 | Lost update | `no_retransmit` | `converge`, `not_quiescent` |
| 3 | Stale echo re-applies an old assignment, then duplicates | `stale_assignment_echo` | `dup_thing`, `converge` |
| 4 | Double ownership | `two_owners` | `multi_owner` |
| 5 | Old owner still writes | `stale_epoch_accepted` | `stale_applied` |
| 6 | Update order not enforced | `stale_seq_accepted` | `stale_applied` |
| 7 | Echo of your own update adopted | `echo_adopted` | `echo_adopted` |
| 8 | Duplicate spawn identity | `spawn_dup` | `dup_thing` |
| 9 | Loot race served twice | `pickup_dup` | `dup_thing` |
| 10 | Handoff re-creates the actor | `handoff_respawn` | `dup_thing` |
| 11 | Repairs never settle | `digest_unstable` | `repairs_after_quiescence` |
| 12 | Request from before a load is accepted | `no_world_epoch` | `stale_applied` |

### 1. Double apply (`no_idempotency`)

A packet arrives twice (the network duplicated it, or your resend logic did) and the second copy is applied too.
In a game: a door toggles twice and ends closed, an item is added to a container twice, damage is dealt twice.
Fix: give every message an id (sender plus sequence) and drop ids you already applied. Make the check cover every
path a message can take, including the resend and the repair path.

### 2. Lost update (`no_retransmit`)

A message is lost on a lossy link and nothing ever sends it again, so one machine keeps an old value forever. The
twin sends each stream item once. In a game: a quest flag never reaches the guest, a kill is not seen, a door stays
shut on one screen. Fix: acknowledge what you received and resend what was not acknowledged, or send periodic
digests and repair on mismatch. The oracle shows it as machines that never converge, or as traffic that never stops.

### 3. Stale echo re-applies an old assignment (`stale_assignment_echo`)

The class behind a real bug: after an owner asks for an assignment, the answer it gets back is only an echo of the
state the request carried. If the owner applies that snapshot, it can undo what it did while the request was in
flight. In the real bug, an actor that died while its assignment was on the wire was brought back to life, the owner
then treated it as a new actor, handed it on, and the loop repeated until hundreds of copies filled the area and the
game crashed. This was found and fixed in Skyrim Together Reborn's code base, pull request #906 in
[tiltedphoques/TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution/pull/906). The fix keeps the local
state for an actor the machine already owns and lets the normal change tracking report any difference.

In the twin: clients predict their own hits, and the host's ownership message carries a snapshot (hit points, dead
flag). With the bug on, the new owner takes the snapshot, an actor it had already killed comes back, and the owner
sends a "rediscovered as new" request, so the host creates a second copy with the same identity. The oracle sees
`dup_thing`. It needs a small coincidence (a kill and an assignment crossing on the wire), so the twin test uses
more seeds for it than for the others.

Fix: an answer to your own request is not news. Take only what you did not already know (the owner, the epoch),
never state that you changed after you sent the request.

### 4. Double ownership (`two_owners`)

An ownership transfer does not bump the epoch, so two machines can both believe they own the actor in the same
epoch and both simulate it. In a game: an enemy rubber-bands, fights two players at once, or is killed on one screen
and alive on the other. Fix: every grant, transfer and release gets a new epoch, and only updates of the current
epoch from the current owner are accepted.

### 5. Old owner still writes (`stale_epoch_accepted`)

The authority accepts a position update from a machine that stopped owning the actor, or at an older epoch, so the
actor jumps back to an old place. Fix: check owner and epoch on every update before applying it.

### 6. Update order not enforced (`stale_seq_accepted`)

Within one epoch, updates are numbered. If an older update arrives after a newer one (reordering) and is applied,
the actor moves backwards. Fix: keep the last sequence per (actor, epoch) and drop anything not newer.

### 7. Echo of your own update adopted (`echo_adopted`)

The host rebroadcasts your move to everyone, including you, and you apply it as if someone else moved you. With
latency this pulls you back to where you were a moment ago. Fix: tag broadcasts with their origin and ignore the
ones that come from yourself.

### 8. Duplicate spawn identity (`spawn_dup`)

Two players trigger the same encounter at once and both spawn it, or the host spawns it twice. In a game: two
copies of the same guard, or a quest NPC that exists twice. Fix: spawns are decided by one authority and keyed by
identity (the spawn site, not the net id). A second request for the same identity is rejected.

### 9. Loot race served twice (`pickup_dup`)

Two players pick up the same item in the same moment and both get it. The twin makes the host grant the loser a
copy. Fix: the host picks one winner per item and answers the loser with "contested".

### 10. Handoff re-creates the actor (`handoff_respawn`)

When ownership moves, the new owner (or the host) re-creates the actor under a new net id and the old one stays
behind. In a game: a ghost body or a double enemy after every area change. Fix: a handoff changes the owner field of
the same thing. It never creates or destroys.

### 11. Repairs never settle (`digest_unstable`)

The digest each side computes is not the same function of the same state (one side rounds a position, one hashes a
field the other ignores). Every digest check then "fails" and triggers a full resync, forever. In a game: periodic
hitches or a flood of resync traffic. Fix: hash exactly the fields both sides agree are shared, and test that two
equal worlds give equal digests. The oracle needs the repair count to stay at zero once everything is quiet.

### 12. Request from before a load is accepted (`no_world_epoch`)

The host loads a save. Requests that were made against the old world arrive afterwards and get applied to the new
one, so an item picked up before the load is applied after it. Fix: stamp requests with a world epoch that the host
bumps on load, and reject mismatches (clients re-send what still makes sense).

## Add your own twin

1. Find a bug you fixed (or a classic one from this list) in your own sync core.
2. Put a switch around the fix: a runtime flag set from `Scenario::bugs`, or a compile-time `#ifdef` for code that
   cannot take a flag. The switch must be off by default.
3. Register the bug name in your target's `TargetInfo::bugs` and read it in the target factory.
4. Run `python -I tools/fuzz/run.py --bug <name> --count 200`. It must fail with the oracle you expect. If it
   passes, the oracles cannot see that class on your target: add the missing log entry, ops or digest category,
   and try again.
5. Save a minimized repro with `--fixture <name>`, and keep a test that sweeps seeds and requires the twin to be
   caught (see `tests/tests.cpp`, `twin(...)`).
