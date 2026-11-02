# Actor ownership with epochs

If your co-op mod shares NPCs and creatures between players, you need a rule for **who runs
each one**. Without it you get actors that fight themselves, jitter, vanish, or multiply. This page
is the rule that worked, the one bug class it prevents, and the small library that implements it.

Code and tests: [tools/ownership](../tools/ownership/README.md) (the library) and
[tools/fakeworld](../tools/fakeworld/README.md) (a fake world that uses it). Related:
[effect-capture-story-sync.md](effect-capture-story-sync.md), [fake-world-testing.md](fake-world-testing.md),
[fuzzing-coop-sync.md](fuzzing-coop-sync.md), [automated-qa.md](automated-qa.md).

## Why

A single player game runs every actor's AI on your machine. In co-op, if every machine runs the AI
of every actor, the copies drift apart, and each one decides to attack, wander, die or loot on its
own. If no machine runs it, actors freeze. The fix every working mod converged on:

- **Exactly one machine simulates each actor.** That machine is the **owner**. Its AI runs.
- Everyone else shows the actor from the owner's updates, with their own AI for it switched off.
- The owner publishes position, animation, health and death. Nobody else is allowed to.

That answer is easy. Keeping it true while players walk around, join, leave and lose packets is the
hard part, and it is where the bugs are.

## The rules

1. **One owner per actor, decided by one machine.** The host decides. Clients never pick owners.
2. **Prefer the host, then the nearest player.** The host usually has the most stable state. Among
   guests, the nearest one wins; a player is a candidate if they are in the actor's cell or in a
   neighbouring loaded cell.
3. **Every change raises an epoch.** Epoch is a counter stored with the owner: grant, transfer
   and release each bump it. "Owner" alone is not enough, you need "owner at epoch 7".
4. **An update is valid only from the owner at the current epoch.** Everything else is dropped:
   an older epoch (a past owner still talking), a newer epoch (our view is behind: ask the host),
   a sender who is not the owner, and our own update coming back.
5. **Ownership messages are idempotent and order independent.** Applying the same message twice, or
   an old message after a newer one, changes nothing. The epoch is the only thing that orders them.
6. **Handoff is a handshake, not a switch.** The old owner stays the authority until the new owner
   has the old owner's last state:
   `Transfer` (host) then old owner sends its final state, new owner applies it and sends `Ack`, then
   `Commit` (host). Only after the Commit does the new owner run the AI. A timeout lets the host
   force the outcome so a lost Ack cannot hang an actor.
7. **Do not flap.** Two players standing about equally near would swap the actor back
   and forth every frame. Keep an owner for a minimum time and only move it when a challenger is
   clearly closer (the library uses 1.5 seconds and 25 percent).
8. **People leave.** When an owner leaves, its actors are released and given to someone else at a
   higher epoch. A handoff to a player who left is cancelled. A handoff from a player who left is
   finished at once.
9. **Joiners get the table.** A joining machine receives the current owner and epoch of every actor,
   including released ones, so late messages from an old owner are still recognized as stale.

## The follower exception

Companions break rule 2. A follower must be simulated by the player who recruited it, wherever that
player goes. If it were owned by the nearest player, it would be handed to whoever is closest every
time the recruiter walks away, and it would lose its follow behaviour at each handoff. So:

- While an actor has a recruiter, the recruiter owns it. No hold time, no distance test.
- When the recruiter disconnects or dismisses the actor, the normal rules apply again.

The library has this as `setRecruiter`. The fake world has a test where a companion follows its
recruiter into a far cell and keeps the same owner.

## Two players, pose-only state

Skyward Sword ([write-up](../games/skyward-sword/README.md)) needed only part of the library, so it uses the same rules
in a few lines of Python:

- With two players the host decides every owner (nearer Link, hold 1.5 s, challenger 25 percent nearer), and one epoch
  counter that never goes down, also across area changes, stamps every grant. The joiner keeps an assignment only if its
  epoch is newer; motion is used only from the owner at exactly the known epoch. A test replays an old grant and a past
  owner's motion, and a mutation twin without the epoch check must fail it.
- When the only synced state is pose (position, facing, animation frame), the handoff handshake can be dropped: the new
  owner has been showing the old owner's pose until the switch, so it already holds the last state. State that is not
  pose (health, timers, AI memory) still needs the handshake, or a merge that does not care about order.
- Health that only goes down merges without any owner: both machines take the lower value, so hits from either player
  count and nothing is ever undone.

## The bug class: stale and echoed assignments

Every ownership system eventually meets this one. The shape:

> A machine receives an **older** statement about who owns an actor, or an old copy of the actor's
> state, and treats it as the truth.

It happens through echoes (the host reflects a request back), retransmits and replays, a client
answering from a cached copy, or a response to a question that was asked before the world changed.
Because nothing in the message says how old it is, the receiver cannot tell.

A real example, publicly documented: **Skyrim Together Reborn, pull request
[#906](https://github.com/tiltedphoques/TiltedEvolution/pull/906)** ("Fix rapid NPC
duplication caused by owners applying stale assignment state"). As that PR describes it, NPCs could
multiply quickly in some areas, sometimes until the game crashed. An owner reconciled a newly
assigned actor against server data that only repeated the original request, so it could be out of
date. An NPC that had died while the request was pending was respawned, which reset its reference
and reloaded its model. The owner then released the entity and treated the actor as new, and this
repeated on each round trip. Other players received the released entities and created copies, so
the count kept growing. Credit to the author for finding and fixing it, and for writing it up
clearly.

What we took from it:

- **Stamp ownership with an epoch** and compare epochs on every message. A statement about an older
  epoch is dropped, whatever it says. Then a replayed old grant cannot overwrite a newer owner.
- **Never rebuild an actor during a handoff.** No respawn, no reset of its reference or model. The
  new owner takes over the existing actor from the old owner's last state.
- **Make the handoff idempotent**, so a repeated Transfer or Commit does nothing the second time.
- **Test it on purpose.** Replay an old assignment at a view and check that it does not win; replay
  the previous owner's updates and check that they are rejected. The library has this as a test and
  as a mutation twin (the twin turns the ordering off and the test must go red).

## How to use the library

The library never touches your network or clock. You give it facts, it gives you messages.

On the host:

```cpp
#include "coop/ownership.h"
using namespace coop;

OwnershipService svc;
OwnershipView hostView(kHostPlayer);
svc.playerJoined(kHostPlayer);                 // and for every guest that connects

OwnershipService::Out out;
svc.track(actorId, whoIsNear(actor), nowMs, out);        // when the actor appears
svc.evaluate(actorId, whoIsNear(actor), nowMs, out);     // every few hundred ms per actor
svc.setRecruiter(actorId, recruiterSlot, whoIsNear(actor), nowMs, out);   // companions
svc.tick(nowMs, out);                                    // every host tick
for (const OwnMsg& m : out) { hostView.apply(m); broadcast(m); }
```

`whoIsNear` fills one `Presence` per player: connected, in the actor's cell, in a neighbouring cell,
and the distance. On a guest joining, send `svc.snapshot(...)`. On a guest leaving call
`svc.playerLeft(slot, nowMs, out)` and `evaluate` the returned orphans again.

On every machine, including the host:

```cpp
// receiving ownership messages
view.apply(msg);

// before publishing an actor's state
OwnerEpoch epoch;
if (view.mayPublish(id, epoch)) send(stateUpdate, epoch);

// on receiving a state update
switch (view.check(id, fromSlot, updateEpoch)) {
    case UpdateVerdict::Accept: applyState(); break;
    case UpdateVerdict::FutureEpoch: askHostForOwner(id); break;
    default: break;      // stale, not the owner, our own echo, not yet active, unknown
}

// during a handoff
OwnerEpoch e;
if (view.handoffIncoming(id, e)) { applyLastStateYouHave(); sendAckToHost(id, e); }
```

And the two game side duties the library cannot do for you:

- **Old owner**, when a `Transfer` names you as the previous owner: stop the AI, send your last state
  marked "final", keep it authoritative until the `Commit`.
- **New owner**, only after the `Commit`: switch the actor's AI on and start publishing at the new epoch.

If your host can load a saved world, put a session counter on every message and drop the old
session's messages: epochs restart from 1 in a new service. See
[DESIGN.md](../tools/ownership/DESIGN.md) for the details and the edge cases.

## Proving it

```
python -I tools/ownership/run_tests.py
python -I tools/fakeworld/run_tests.py
```

The first runs the rule, handoff and view tests, a random churn with lost acks and reordered, duplicated
messages, the stale echo test and seven mutation twins. The second runs the same ideas inside a whole
fake world with lossy links, where the ownership rules decide who simulates what. Both print `ALL OK`
only when every twin is caught. After you wire the library into your game, add these checks to your own
tests: no actor is driven by two machines, no epoch goes backwards, and nothing from an older epoch
is applied. [fake-world-testing.md](fake-world-testing.md) shows how.
