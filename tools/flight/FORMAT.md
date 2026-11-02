# Co-op flight record format (.cofr, version 1)

A **co-op flight record** is the black box of one machine in a co-op session: what it sent and received, what it decided,
what it applied to its own world, digests of its state, and notes. One file per machine per dump. Put the files of 2 to 4
machines side by side and the analyzer finds where their worlds first disagreed and why.

The format knows nothing about any game. Everything a game chooses (which digest categories exist, what a message type
is called, which extra record kinds it writes) lives in a JSON **schema** that is stored inside the file.

* Writer: `tools/flight/cpp/` (standalone C++17). The C++ side is the source of truth for the byte layout.
* Reader, writer, analyzer, shrinker: `tools/flight/*.py` (Python, no dependencies). `cofr.py` also writes files, for tests and for tools that add records.
* Golden samples: `tools/flight/golden/`. `run_tests.py` reads them on every run, and a C++ test fails if the writer's output drifts from them.

## 1. Versioning

The magic is `COFR`. The header carries `version` (a u16, currently 1).

* A reader refuses a file whose magic or version it does not know.
* These changes keep version 1: new record types in the reserved range 10 to 31, new bits in `flags`, new keys in the schema.
  Readers keep unknown record types as raw payload and ignore unknown schema keys.
* A change to the header, the record header or the layout of a record type that exists today needs version 2.

## 2. File layout

One file per machine per dump. All integers are little endian. `str16` is a `u16` byte length followed by UTF-8 bytes.

```
HEADER (64 bytes)
  "COFR" (4)   u16 version (=1)   u16 headerSize (=64)
  u8 slot   u8 role (0 host, 1 client, 2 sim)   u8 reason (0 manual, 1 desync, 2 crash, 3 shutdown, 4 test)   u8 reserved
  u32 protoVersion      your protocol or schema version, free to use
  u64 wallStartMs       unix time in ms when the recorder started
  u64 dumpTUs           recorder time of the dump, in microseconds
  u64 recordsDropped    records the ring overwrote before the dump
  u64 recordCount
  u32 sessionId   u32 epoch   u64 reserved
RECORD (16-byte header + payload), repeated to the end of the file
  u8 type   u8 flags   u16 payloadLen   u32 frame   u64 tUs      then payloadLen bytes
```

* `slot` is the machine's number (the host is usually 0). `0xFF` means unknown.
* `tUs` is microseconds since the recorder started, monotonic, and only comparable inside one file. In a simulator all
  machines can share one virtual clock. The analyzer estimates the real offset between machines from paired messages.
* `frame` is the game's frame or tick counter when the record was made.
* A half-written last record (a crash dump) is fine: readers keep every complete record before it and report the cut.
* A payload is at most 65535 bytes. Big things (a long digest, a big datagram) are split or cut by the writer.
* `headerSize` lets later versions add header fields: a reader jumps to `headerSize` to find the first record.

## 3. Record types

| Type | Name | Payload |
|---|---|---|
| 1 | Meta | repeated `str16 key`, `str16 value`. The first pair is `label`, the machine's name in reports |
| 2 / 3 | NetOut / NetIn | 36-byte prefix, then the datagram bytes (see below) |
| 4 | TxnCaptured | `u8 kind` (0 commit the machine decided, 1 intent it asked for), `u32 intentId`, `u64 seq` (0 for an intent with no number yet), `u32 effectCount`, `u64 hash` (a hash over the effect hashes, computed by the game), `u8 cause`, `u32 epoch` |
| 5 | TxnApplied | one record per effect applied to the local world: `u64 seq`, `u32 epoch`, `u8 origin`, `u32 index`, `u8 flags`, `u8 cause`, `u8 effectTag`, `u64 hash` |
| 6 | Ownership | `u8 op`, `u64 actor`, `u32 epoch`, `u8 fromOwner` (255 none), `u8 toOwner` (255 none), `u8 reason`, `u64 txnId` |
| 7 | Digest | `u8 category`, `u32 epoch`, `u64 seq`, `u64 hash`, `u32 itemCount`, `u16 nShards` then `u32` each, `u32 nKeys` then (`u64 key`, `u32 valueHash`) each |
| 8 | Marker | `u16 kind`, `str16 text` |
| 9 | Schema | UTF-8 JSON, see section 5. The writer emits it once, right after Meta |
| 10 to 31 | reserved | for later versions |
| 32 to 255 | game-defined | layout given by the schema's `custom_records` |

The words "txn" and "effect" are the format's: a **txn** is one committed batch of changes numbered by `seq`; an **effect**
is one change inside it, numbered by `index`. A game with another shape (for example lockstep turns) maps a turn to a seq
and an input to an effect, or leaves the Txn records out and uses digests and markers only.

### Record flags

| Bit | Name | Meaning |
|---|---|---|
| 1 | truncated | Net: the datagram was cut at the writer's `maxRawBytes` |
| 2 | derived | Digest: built from a received message, not from the local state |
| 4 | detail | Digest: carries only per-key entries (no buckets) |
| 8 | bucketed | Digest: the shards are key buckets, bucket = key mod number of shards |

### Net prefix (NetOut and NetIn)

```
u32 peer          transport peer id of the other side
u8 channel        send channel; 0xFF on NetIn
u8 mux            first byte of the datagram
u8 msgType        message type, filled by the decoder (0 without one)
u8 decode         0 decoded, 1 the decoder refused or the datagram was cut, 2 no decoder
u32 session   u32 epoch   u64 seqLo   u64 seqHi     filled by the decoder: the range of txn numbers the datagram carries
u32 rawLen        length of the datagram on the wire
then the datagram bytes, at most `maxRawBytes` of them
```

The hot path stores only `peer`, `channel`, `mux`, `rawLen` and the bytes. The writer runs your decoder when it dumps, not when
the datagram is recorded, so recording never decodes anything. The analyzer pairs a send with its receive by a hash of
the datagram bytes (the sender is the NetOut machine, the receiver the NetIn machine).

### Peer ids

A `peer` is a number from your transport. The analyzer treats it as the slot of the other machine. When that is not
true on a machine (a host whose transport numbers its connections differently), the recorder writes Meta entries
`peer.<id>` = `<slot>` and the analyzer uses them. `Recorder::mapPeer(id, slot)` does it.

### Ownership

An actor is anything a machine can own (an NPC, a door, a vehicle). The schema says which ops give an actor an owner
(`ownership_owner_ops`, default grant and transfer). The analyzer reports two different owners at the same
`(actor, epoch)`, and an owner op with an epoch lower than one already seen on that machine. Other ops (release,
rejected stale, rejected echo, rejected non-owner) are the system doing its job and are never reported.

### TxnApplied flags

`flags` has bits the schema names (default 1 snapshot, 2 repair, 4 baseline, 8 catch-up). The schema's `restate_mask`
(default 7) says which bits mark an apply that **restates state** instead of replaying a txn. The analyzer ignores
those applies when it compares effect content, looks for double applies and looks for missing ones.

### Markers

`kind` is a number the schema names (defaults: 0 note, 1 desync, 2 join, 3 leave, 4 test step, 5 save, 6 load, 7 rebase).
**Kind 1 (desync) and kind 4 (test step) keep their meaning in every schema**: other tools write kind 4 summaries and
kind 1 breaches, and the analyzer reports a machine's own desync marker as a finding. A desync marker whose text contains
`ref=#<hex id>` links it to the actor with that id: the causal chain then shows that actor's ownership history.

Entry text lines: a marker (any kind) whose text is `entry <key as 16 hex digits> <value hash in decimal> <text>`
describes one entry of the state a digest was taken from, for example
`entry c8800ff437102235 10 stage.q1 = 10`. The analyzer shows the text of both machines next to each other.
`Recorder::entryText(key, valueHash, text)` writes one.

## 4. Digests

A digest is a hash of part of the shared state. `category` is a number the schema names. Digests with the same
`(category, epoch, seq, derived)` on different machines are compared. A **derived** digest (flag 2) is one a decoder pulled out of a
received message; it exists on both the sender and the receiver and is identical there by construction, so derived and
real digests are grouped apart and never compared with each other.

### Bucketed digests

The reference recorder builds bucketed digests (`bucketedDigest`, `BucketedState`), and `cofr.py` computes the same bytes.

```
mix(key, valueHash) = low 32 bits of splitmix64(key XOR splitmix64(valueHash))
bucket b            = key mod nBuckets                    (default 256)
shard[b]            = sum of mix(key, valueHash) over the entries of bucket b, modulo 2^32
hash                = FNV-1a 64 over the shards (each as 4 little-endian bytes), starting from the offset basis + itemCount
```

`splitmix64(x)`: `x += 0x9E3779B97F4A7C15; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9; x = (x ^ (x >> 27)) * 0x94D049BB133111EB; return x ^ (x >> 31)`
(Sebastiano Vigna, public domain), all in 64 bits. FNV-1a 64: offset basis `14695981039346656037`, prime `1099511628211`.

Because the shards are sums, the digest does not depend on the order of the entries, and one changed value changes exactly one
shard. Set flag 8 on a digest whose shards are key buckets; the analyzer maps a differing shard to the keys that fall into it.
Any other kind of shard (a digest of 128 map regions, say) is compared shard by shard but never mapped to keys.

### Narrowing: digest, bucket, key, value

1. **Digest:** the hashes of two machines differ.
2. **Bucket:** the shards that differ.
3. **Key:** the entries whose value hash differs, or that exist on one machine only. They come from detail records:
   Digest records with flag 4 that carry up to 4000 `(key, valueHash)` pairs each, several records for one digest (same category,
   epoch and seq; `itemCount` is the total). A desync handler writes the main digest plus its detail records on every machine.
   For a digest that has buckets only, the analyzer takes the keys from the newest other digest that has detail and whose differing
   keys fall into the differing buckets.
4. **Value:** the text of the entry lines (section 3, Markers) on each machine.

A game that records a bucketed digest every few seconds and writes the detail only when it detects a desync pays almost
nothing in quiet times and still gets down to the key.

## 5. The schema

JSON text, written into every file as record type 9. Every key is optional. **A key that is present replaces the default
for that key as a whole; a key that is missing keeps the default.** A game gives only what it changes.

```json
{
 "game": "toy",
 "digest_categories": {"0": "world", "1": "npcs"},
 "marker_kinds": {"0": "note", "1": "desync", "4": "test-step", "9": "checkpoint"},
 "ownership_ops": {"0": "grant", "1": "transfer", "2": "release"},
 "ownership_owner_ops": [0, 1],
 "message_types": {"4": "batch", "5": "ack"},
 "txn_messages": [4, 5],
 "apply_flags": {"1": "snapshot", "2": "repair", "4": "baseline", "8": "catch-up"},
 "restate_mask": 7,
 "custom_records": {
  "40": {"name": "ScriptWrite", "fields": [
   {"name": "seq", "type": "u64"}, {"name": "key", "type": "str16"}, {"name": "value", "type": "i32"}]}
 }
}
```

| Key | Meaning |
|---|---|
| `game` | free text |
| `digest_categories` | names of the digest categories your game uses. An id without a name is shown as `cat<N>` |
| `marker_kinds` | names of marker kinds |
| `ownership_ops` | names of ownership ops |
| `ownership_owner_ops` | the ops that give an actor an owner |
| `message_types` | names of the message types your decoder reports |
| `txn_messages` | the message types whose seq range is a txn range. In a causal chain, a datagram is "directly about" a txn when it is one of these and its range contains the seq. Omitted: every decoded message counts |
| `apply_flags`, `restate_mask` | see TxnApplied flags |
| `custom_records` | game-defined record types 32 to 255 |

`schemas/default.json` is the default (the example set story, actors, containers, objects, other). `schemas/example_game.json`
is a small custom one. The default is also built into the C++ recorder and the Python reader, and a test checks that all three agree.

### Game-defined records

A record type from 32 to 255 is yours. Describe its layout with fields in order. Field types: `u8 u16 u32 u64 i8 i16 i32 i64 f32 f64`
(little endian), `str16` (u16 length, UTF-8), `bytes16` (u16 length, raw bytes), and `bytes` (the rest of the payload; last field only).
The analyzer prints these records by name, shows them in the causal chain, and treats one as directly about a txn when it has a `seq` field equal to
the txn's seq (or a `ref` field equal to the actor). Write one with `Recorder::custom(type, bytes)`. Without a schema entry
the record is kept as raw bytes and shown as `Unknown<type>`.

An external schema (`flight.py ... --schema FILE`) overrides the schema in the files. That is how you read old files after
you renamed things.

## 6. What the analyzer reports

`flight.py diverge a.cofr b.cofr [c.cofr] [d.cofr]` finds the earliest of:

* `txn-hash`: the same `(seq, index)` was applied with different effect hashes. The odd machine is named when there are three or four.
* `txn-double`: one machine applied an effect twice.
* `txn-missing`: applied elsewhere and never here. Only reported when this machine applied a later seq, or kept recording more than 2 seconds past it.
* `ownership-conflict`, `ownership-stale`.
* `digest`: a hash, a bucket or a key differs. With narrowing as in section 4.
* `desync-marker`: a machine wrote its own desync marker (lowest priority).

It prints the key, the suspect machine or machines, and a **causal chain**: the records about that key (marked `*`) plus the
txns, ownership events, game-defined records and messages between the involved machines, back to the last digest of that
category that everyone agreed on, plus any message lost or delivered twice in that window. "Lost" means sent and never seen
arriving; when identical bytes are retransmitted, which copy was lost is a guess (the count is right).

## 7. Replay fixtures and the replay hook

The shrinker (`shrink.py`) reduces a list of independent steps and keeps a smaller list only if the replay still fails with the
**same failure signature**. The part that knows your game is the **replay hook**, either a command or a Python function.

* **Fixture:** JSON, `{"version": 1, "config": {...}, "steps": [ ... ]}`. The list may be called `ops` (or any name you give with `--list-key`).
  Every step must be removable on its own, with its own absolute time and explicit arguments. A step with `"pin": true` is never removed
  (use it for the final check, such as `{"op": "quiesce", "pin": true}`).
* **Fixture from traces** (`flight.py fixture`): one `{"op": "deliver", "to": SLOT, "from": PEER, "hex": "<datagram>", "t": ms}` step per received
  datagram that was not cut, in aligned-time order, plus the pinned quiesce step. A trace holds hashes of host-side decisions, not the decisions, so such a fixture replays
  what each machine received, not what the host decided locally.
* **Command hook** (`--replay-cmd "program args {fixture}"`): the token `{fixture}` becomes a temp file. The program exits 0 when the replay converged,
  1 when it diverged, 2 for a bad fixture. It may print one JSON line `{"diverged": true, "violations": ["..."], "first": {...}}` or one line per broken
  rule, `VIOLATION <rule-name> <detail>`. The signature is the first rule name, or the first violation with numbers and hashes removed.
* **Python hook** (`--replay-py module:function`, with `--replay-path DIR` for the import): the function takes the fixture dict and returns
  `{"diverged": bool, "violations": [...]}`. It may raise on a bad fixture.
* `toy_replay.py` implements both as a complete example.

* **Trace fixture** (`"format": "cofr-trace-1"`, written by `from_failure.py`, replayed in process by `trace_replay.py`): the raw records
  of every machine's `.cofr` file, so no replay program is needed. Each step is one whole record and is independent:
  `{"op": "rec", "slot": 0, "ty": "TxnApplied", "at_ms": 123.4, "hex": "<the 16-byte record header and its payload>"}`. `ty` and `at_ms`
  (aligned time) are for people and for the shrinker. `"headers": {"<slot>": "<hex of that machine's 64-byte header>"}` gives each machine's
  header (its record count is rewritten to the steps kept), and the optional `"schema"` is schema JSON text that replaces the one stored in
  the records. The replay aligns the machines and classifies what the analyzer finds: `digest-mismatch`, `divergence`, `stuck-sync`
  (a txn applied on one machine and never on another, with nothing restating that machine's state afterwards), `desync-marker`, and
  `transient-gap` (not a failure). It reports the earliest finding that is not a machine's own desync marker.

`gentest` writes the shrunk fixture as `<name>.json` with `"expect": {"converges": true}` and `<name>.cmake` with one `add_test`
line for your replay program. `regress.py DIR` runs every fixture in a folder through the hook and prints ALL OK when each matches its
expectation (`"converges": false` marks a known open bug). A trace fixture gets no `.cmake` file and needs no hook: `regress.py` replays it itself.
