# Desync hunting with traces and shrinking

A desync is two machines disagreeing about the shared world. Staring at two log files is the slow way to find the first moment
they disagreed. This page is a workflow that turns it into a short, repeatable loop: **record, find the first divergence, narrow it to a
key and a value, shrink the failing run, keep it as a test.** The tools are in [tools/flight](../tools/flight/README.md) and work for any game; the
ideas apply even if you write your own.

## The loop

1. **Record** on every machine, all the time, into a ring buffer that costs about a microsecond per event.
2. **Dump** when your own check sees a desync (and on a crash, and on request).
3. **Find the first divergence** across the dumps of 2 to 4 machines.
4. **Narrow it** from the digest to the bucket, the key and the value, with the messages that led there.
5. **Reproduce** it in your test rig and **shrink** the run to the few steps that matter.
6. **Keep it** as a regression test that fails until the bug is fixed.
7. **Check the tools can fail**: break something on purpose and see the tests notice.

## 1. Record the right things

Record what a person debugging would ask for, and nothing that costs frames:

- **Every datagram in and out**, as bytes, with the peer and channel. Decode it later, at dump time, never in the hot path.
- **Every effect applied to the local world**, with a hash of its content, its txn number (`seq`) and its index. The same effect must have the same hash on every machine.
- **Every decision the host made** (its commits), with a hash.
- **Ownership changes** (who owns which actor, at which epoch).
- **Digests** of the shared state every few seconds, at a quiet point, with an epoch and a seq so machines can be compared at the same moment.
- **Markers** for joins, saves, loads and anything else you will want to see in the chain.

Make the hashes **stable**: hash the content of the effect, not pointers or addresses; iterate maps in sorted order; hash floats by their bits. A hash that differs
between two healthy machines produces false desyncs, and every one of them costs an hour.

Put a **budget on the overhead and test it.** The benchmark in the recorder's tests fails if a frame costs more than 100 microseconds.

## 2. Digests that point at the bug

A single hash of the whole state tells you that you have a desync, not where. A **bucketed digest** is better: hash each entry as (key, value hash), add the entries up in
256 buckets by `key mod 256`, and store the 256 sums. Addition does not care about order, one changed value changes exactly one bucket, and the digest
can be kept up to date one entry at a time (`BucketedState`) so you never walk the whole world.

- Record the bucketed digest every few seconds. It costs almost nothing.
- **When you detect a desync, also write the detail**: every (key, value hash) pair of the state you compared, plus a readable line for each entry
  (`entry <key> <value hash> quest_stage intro = 77`). Now the analyzer can go from the differing bucket to the key and show the value on each machine.
- Compare digests taken at the same `(category, epoch, seq)`. A digest taken while messages are still in flight is not a desync.

## 3. Read the first divergence

```
python -I tools/flight/flight.py diverge host.cofr client1.cofr client2.cofr
```

On the sample pair in `tools/flight/golden` (a script on the client quietly overwrote one value) it prints:

```
FIRST DIVERGENCE (digest) at 680.000ms: world digest (epoch 1, seq 5) differs: ...
  key: world:0xA21A963AA6B76AAB   suspect slot(s): [0, 1]   (2 candidate(s) in total)
  narrowing: bucket(s) 171 of 256
    key 0xA21A963AA6B76AAB (bucket 171)  [from this digest]
      slot 0: value hash 10  stage.q1 = 10
      slot 1: value hash 99  stage.q1 = 99
  causal chain, window 290.000ms .. 680.000ms:
        ...
        325.000ms slot 1 TxnApplied   seq=4[0] origin=0 epoch=1 tag=1 hash=8d0f343480ecf481
        325.000ms slot 1 ScriptWrite  seq=4 key=stage.q1 value=99
```

Read it from the top: **what** differs (the key and both values), **who** is the odd one out (with three or four machines the analyzer names the minority),
and **how it got there** (the chain from the last digest everyone agreed on to the one that differs; lines marked `*` are directly about the key).
Only the first finding matters. Later ones are usually its consequences.

What each kind of finding usually means:

| Finding | Usually |
|---|---|
| `txn-hash`: the same effect has different hashes | The effect was computed differently on two machines: something nondeterministic (iteration order, a float, a local clock, a random number) went into it. |
| `txn-double`: applied twice | A duplicate datagram or a retransmit was not de-duplicated. The chain shows "message delivered 2 times". |
| `txn-missing`: applied elsewhere, never here | A datagram was lost and never resent, or was dropped by a filter. The chain shows "message lost in transit". |
| `ownership-conflict` | Two machines both think they own an actor at the same epoch: a handover race. |
| `ownership-stale` | A machine accepted an owner change with an older epoch: a missing epoch check. |
| `digest` with every effect hash equal | Something outside the sync path changed the state on one machine: a local script, an AI routine, a cleanup, a load. In the sample, the `ScriptWrite` record shows it. |
| `desync-marker` | Your own check reported something that is not a visible state difference, for example a stale update that was accepted. |

## 4. Reproduce and shrink

A trace shows you what happened once. To fix it and keep it fixed you need a smaller run you can replay on demand.

- Your test rig (a fuzzer, a simulator, a scripted bot) replays a **fixture**: a JSON list of steps. Give the shrinker a **replay hook**, a command or a Python function that
  runs a fixture and says whether the bug still shows. The contract is a few lines ([tools/flight/FORMAT.md](../tools/flight/FORMAT.md) section 7), and `toy_replay.py` is a working example.
- `flight.py fixture` can build a fixture from the traces (one step per received datagram), which replays what each machine received. Decisions the host made locally are in the trace as hashes only, so
  for those, use the scenario your rig produced the failure with.
- `flight.py shrink` runs delta debugging (ddmin): it removes chunks of steps and keeps a smaller list only if the replay **still fails the same way**. Matching the failure
  signature matters: without it a shrink happily turns your bug into a different, easier one. In the tool's tests a 31-step toy fixture shrinks to the 3 steps that matter.
- Make steps **independent**: each has its own absolute time and explicit arguments, so removing any subset leaves a valid run. Mark the final check step `"pin": true` so it is never removed.

```
python -I tools/flight/flight.py shrink fixture.json --replay-cmd "build/my_replay {fixture}" -o shrunk.json
python -I tools/flight/flight.py gentest shrunk.json --name stale_echo --target my_replay
python -I tools/flight/flight.py report host.cofr client.cofr -o report.html --shrink shrunk.json
```

## 5. Keep it as a test

`gentest` writes the shrunk fixture with `"expect": {"converges": true}` and a CMake `add_test` line for your replay program. The test **fails while the bug exists and passes once it is fixed**, so it
goes in before the fix. `regress.py DIR` runs every fixture in a folder and prints `ALL OK` when each matches its expectation; mark a fixture `"converges": false` while the bug is a known open one.

## 6. Prove the tools can fail

A desync tool that says "no divergence" is only worth something if it would have said otherwise. The flight tools carry **mutation twins**: each breaks one thing on purpose (a ring that drops the newest record
instead of the oldest, a digest that ignores values, a shrinker that accepts a different failure) and the suite must fail. Do the same for your own
checks, as described in [automated QA](automated-qa.md): turn the fix off and the test must go red.

## Pitfalls

- **Clocks differ.** The analyzer estimates the offset between machines from paired messages; it needs messages in both directions. With one-way traffic it falls back to the wall clocks and says so.
- **Peer ids are not slots.** The analyzer assumes the transport's peer id is the other machine's slot. If your host numbers connections differently, record the mapping (`mapPeer`) or nothing will pair up.
- **A small ring loses the start.** The alignment notes say "lost N old records". Use a bigger ring or dump earlier; a desync that began before the oldest record cannot be traced to its cause.
- **Lag is not loss.** A txn that has not arrived yet is not "missing" until the machine applied a later one or kept recording for 2 more seconds.
- **Cut datagrams cannot be decoded or replayed.** Keep the size limit above your biggest message, or accept the gap.
- **Traces hold raw datagrams.** They can contain player names and chat. Look before you attach one to a public issue.

## Where to go next

- [tools/flight/README.md](../tools/flight/README.md): how to add the recorder to your mod, every command, how it is tested.
- [tools/flight/FORMAT.md](../tools/flight/FORMAT.md): the file format and the schema, so you can write records from any language.
- [Automated QA](automated-qa.md): the test harness the traces plug into. [Networking](networking.md): the transport underneath.
