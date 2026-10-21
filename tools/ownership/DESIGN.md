# Ownership design

This is the full rule set of `coop_ownership`, in the order you need it when you wire it into a mod.
The short version is in the [README](README.md) and the story behind it in
[playbook/actor-ownership.md](../../playbook/actor-ownership.md).

Ported from fnv-coop commit f57aa78 ([github.com/cranebeaker/fnv-coop](https://github.com/cranebeaker/fnv-coop)).

## The model

- Every live actor has **one owner**: the machine that runs its AI and publishes its state.
  Everyone else shows a copy driven by the owner's updates.
- Every ownership change raises the actor's **epoch**. An update is only valid if it carries the
  current epoch and comes from the current owner.
- The **host** (slot 0) is the only place that decides. It runs `OwnershipService`. Every machine,
  the host included, runs an `OwnershipView` that applies the host's messages and judges updates.
- The library never sends anything. It returns `OwnMsg` values, you broadcast them. It never
  reads a clock. You pass `nowMs` in, so tests can use a virtual clock.

## Messages

| Kind | Direction | Meaning |
|---|---|---|
| `Grant` | host to all | `owner` owns the actor at `epoch`. Used for a first owner, a take over from a player who left, and to cancel a handoff |
| `Transfer` | host to all | Start moving the actor from `prevOwner` to `owner`. Carries the epoch the new owner will have. The epoch only takes effect at `Commit` |
| `Ack` | new owner to host | "I applied the old owner's last state, I am ready". Carries the Transfer's epoch |
| `Commit` | host to all | Handoff done: `owner` is authoritative at `epoch` |
| `Release` | host to all | Nobody owns the actor at `epoch` (cell unloaded for everyone, owner left) |

`encode` / `decode` give a compact byte form (kind, varint id, varint epoch, owner, previous owner).
`decode` refuses unknown kinds, overlong or oversized varints, truncated input and trailing bytes.
You can use your own serializer instead.

## Choosing an owner (`pickOwner`)

Input: a `Presence` per player (`connected`, `inCell`, `adjacent`, `distance`), the current owner,
and whether the actor is still inside its hold time.

1. **Eligible** means connected and (in the actor's cell or in a neighbouring loaded cell).
2. Among eligible players the **host wins** if it is eligible. Otherwise the nearest wins. Equal
   distance goes to the lowest slot.
3. No current owner, or the current owner disconnected: take the best player, or nobody.
4. Nobody eligible, or the best is already the owner: keep the owner. An actor whose owner walked
   away but nobody else is around stays dormant with its owner.
5. The current owner is no longer eligible (it left the cell area): switch to the best at once.
6. Otherwise **hysteresis** applies, so ownership does not flap:
   - while `minHoldMs` (default 1500) has not passed since the last change, keep the owner;
   - after that the host takes the actor as soon as it is eligible;
   - another player takes it only if they are at least `switchMargin` (default 25 percent) closer.

`distance` can be any unit, it is only compared.

## The follower exception

`setRecruiter(actor, player, ...)` makes a companion owned by its recruiter wherever it goes, with no
hold time and no eligibility test. The recruiter's machine must simulate its follower because the
actor leaves every other player's area when the recruiter does. `setRecruiter(actor, kNoPlayer, ...)`
or the recruiter leaving (`playerLeft`) ends it, and the actor goes back to the normal rules.

## Handoff

```
host                old owner            new owner
 | Transfer(e+1) ---->|------------------->|
 |                    | keeps publishing   | applies the last state it has seen
 |                    | at epoch e; sends  |
 |                    | its final state    |
 |<------------------------------ Ack(e+1) |
 | Commit(e+1) ------>|------------------->|
                      | stops              | starts its AI, publishes at e+1
```

- While a handoff is pending the old owner is still the authority: updates at the old epoch from the
  old owner are accepted. The new owner's updates at the new epoch get `NotYetActive`.
- Only one handoff per actor at a time. `evaluate` does nothing for an actor with a pending handoff.
- `onAck` accepts an Ack only from the pending new owner with the pending epoch. Duplicates and stale
  Acks return false and change nothing.
- `tick` forces the outcome after `handoffTimeoutMs` (default 1000): if the new owner is still
  connected the host sends `Commit` anyway (counted as a forced commit), otherwise it sends a `Grant`
  back to the old owner at a higher epoch (counted as an aborted handoff).
- The new owner must not start its AI before it sees the `Commit`. The old owner must stop its AI
  when it sees the `Transfer` is about it, send its final state, and stop publishing at `Commit`.
  The library gives you the signals (`handoffIncoming`, the `OwnMsg` kinds). Doing it is your job.

## Players leaving and joining

`playerLeft(p, ...)`:

- Actors owned by `p` get a `Release` (new epoch) and are returned, so you can `evaluate` them again
  for someone else.
- A pending handoff **to** `p` is cancelled. The old owner gets a `Grant` at a higher epoch, or a
  `Release` if the old owner is gone too.
- A pending handoff **from** `p` is committed at once, so the new owner gets the actor.
- Recruiter links to `p` are cleared.

`snapshot(out)` produces the current assignment as `Grant` and `Release` messages for a joining or
rejoining machine. Apply them to a fresh view (`view.clear()` first). Released actors are included
with their epoch, so late messages from their old owner stay stale after the rejoin.

`current(id, out)` answers one actor, for example when a machine sends a `FutureEpoch` update and the
host wants to correct it.

## The view

`apply(msg)` returns `Applied`, `Duplicate` or `Stale`. Rules:

- Grant, Commit, Release: older epoch is `Stale`. Same epoch is `Duplicate` if the owner is the same
  and `Stale` if it differs. Newer epoch applies.
- Transfer: only recorded as pending (the owner does not change yet). Replays of a Transfer, and
  Transfers older than the current state, are `Duplicate` or `Stale`.
- Ack is a host side message and is ignored by views.
- A Commit that arrives without its Transfer (lost) still applies.

So messages can be duplicated, reordered or replayed without harm. This is what defeats the stale
echo bug in the playbook page.

`check(id, from, epoch)` returns one verdict per update:

| Verdict | Meaning | What to do |
|---|---|---|
| `Accept` | from the current owner at the current epoch | apply it |
| `NotOwner` | right epoch, wrong sender | drop |
| `StaleEpoch` | older epoch: a past owner is still talking | drop |
| `FutureEpoch` | newer epoch than we know: our view is behind | drop, ask the host (`current`) |
| `NotYetActive` | sender is the new owner of an uncommitted handoff | drop, it will be valid after the Commit |
| `EchoOfSelf` | the sender is this machine: our own publish came back | drop |
| `UnknownActor` | no record | drop or ask the host |

`mayPublish(id, epochOut)` is true only while this machine is the owner. Stamp `epochOut` on the
update. `digest()` hashes `(id, owner, epoch)` of owned actors: equal views give equal digests, so a
test or a periodic check can compare machines cheaply.

## Things to know

- **Host save and load.** A new `OwnershipService` starts its epochs from 1 again. If you can load a
  saved world on the host, stamp a session counter on every message and drop anything from an older
  session, the way the fake world does (see [../fakeworld](../fakeworld/README.md)). Epochs only order
  messages inside one session.
- **Slots.** One byte of connected flags, so 8 slots. `PlayerId` 255 means nobody.
- **Dead and disabled actors.** Call `untrack` when an actor is gone for good (a `Release` is emitted
  if it had an owner). Dormant actors with no eligible player keep their owner.
- **Digest byte order.** `digest()` hashes the native bytes of 32 bit ids, so compare digests between
  machines of the same endianness (all common PC and console targets are little endian).

## Test faults

`OwnershipFaults` switches (process global, set them with `OwnershipFaultScope`, never in production)
re-create known bugs: `noEpochBump`, `acceptStaleEpoch`, `adoptEcho`, `transferNoAck`, `noHysteresis`,
`followerIgnored`, `acceptNonOwner`. Each has a scenario in `tests/` that must fail when the fault
is on. That is the mutation twin idea: a test you have never seen fail is a test you cannot trust.
