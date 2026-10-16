# Original Linux loopback transport helper (Task 5 portion)

This C11/POSIX helper is **not a playable co-op build**. It contains no
native game, ROM, assets, copied engine/decompilation source, rendering codec,
world model, save logic or game adapter. Task 3 and full Task 5 remain separate
gates. `NET_CAP_TWO_PLAYERS` is a claim supplied by the future native adapter;
these tests cannot prove that claim true.

## Build and verify

Using the installed compiler, make, and Python 3 (negative controls only):

```sh
make test
python3 tests/negative_controls.py
make clean
```

Build flags: `-std=c11 -Wall -Wextra -Werror -pedantic -O2 -g` with
`_POSIX_C_SOURCE=200809L`. No dependency downloads or sanitizer runtime required.
Every test listener binds exactly `127.0.0.1` using port zero, then explicitly
closes and rebinds that allocated port to check listener cleanup. Tests fork real
processes for input/state, compatibility, fragmentation and malformed exchanges.
The process suite is bounded by a 20-second alarm. The short-write suite has a
10-second alarm and 5000-iteration progress guard; each mutation invocation has a
25-second supervisor timeout. Where a filesystem sandbox blocks socket creation, these loopback-only tests
need an execution environment that permits local TCP sockets.

`tests/red_stub.c` preserves the buildable missing-functionality baseline.
`negative_controls.py` compiles that baseline and six copies with one deliberate
bug each. Compile failure, timeout or signal death does not count as detection.
It expects a normal test assertion failure and removes the temporary sources and
binaries. Logs remain in `tests/`. A second test uses the existing GNU linker's
`--wrap=send` to call real TCP sends with at most seven bytes per call plus
controlled EINTR/EAGAIN returns. This verifies syscall handling, not simulated
network latency or TCP packet loss. Its max-payload roundtrip catches a mutation
that discards the unsent tail of a short write.

## Application contract

Include `net.h` and link `net.c`. All calls run on one application thread. Objects
are caller-owned, stack-allocatable, and must not be copied while active.
`net_listen` and `net_connect` accept exactly the literal `127.0.0.1`; even a DNS
name or wildcard bind is refused. Host setup uses an application-supplied protocol
number, 32-byte build tag, 32-byte ROM hash, actual capability bits, and nonzero
session epoch. The guest supplies its own matching identity and capabilities.
The helper does not calculate hashes or provide cryptographic verification.

- `net_listen`: 0 success, -1 invalid/OS failure. `host.port` reports the bound port.
- `net_accept`: 1 accepted, 0 would block/interrupted, -1 invalid/OS failure,
  -2 another connection was closed because P2 is already reserved. Reservation
  begins on accept, before handshake. Failed handshakes free the slot.
- `net_connect`: 0 nonblocking connection started, -1 invalid/immediate OS failure.
- `net_pump`: flushes at most one send attempt and reads at most one header/body
  segment; 1 yields one event, 0 pending/dropped replay, -1 terminal with
  `NET_CLOSED` or `NET_ERROR`. Continue pumping even when no input is pending.
  Nonblocking I/O handles EAGAIN/EWOULDBLOCK/EINTR, short reads and short writes.
- `NET_READY` assigns this sole guest to P2 and supplies the host epoch. P1 is
  always host. Host receives only `NET_INPUT` from P2; guest receives only
  `NET_VIEW`/`NET_EVENT` from P1. These role IDs are not authenticated identities.
- `net_send`: 1 copied into the one-frame output slot, 0 busy (retry later),
  -1 invalid/not ready. A successful queue does not mean delivery. Each sender
  uses strictly increasing nonzero sequences across its application messages.
  Host view/event messages share one sequence stream. No buffer overwrite and
  no unbounded output queue occur under backpressure.
- Receive drops sequences at or below the previously delivered value. The host
  retains its P2 input floor across reconnects with the same epoch, while the
  listener object lives. A different supplied host epoch resets that floor.
  Guest reconnect must continue its input sequence above that floor. READY
  does not currently communicate the floor; the future adapter must retain it
  or supply an authoritative resync before sending. A new listener/process does
  not persist it: the future game adapter/save layer must restore its own
  application transaction deduplication state and choose a fresh epoch where
  appropriate. This helper guarantees event emission at most once within that
  floor, not successful/durable application of a game transaction.
- `net_peer_close` releases its accepted slot and discards queued unsent output.
  Close peers before `net_host_close`, and keep host storage alive until peers
  close. `net_host_close` closes only its listener. Setup objects only once before
  closing/reinitializing; passing an already-live object to setup is unsupported.

There is no handshake timer inside the helper. The application must bound idle
handshakes, pump scheduling and reconnect attempts. One stalled peer can occupy
the sole slot until the application closes it. There is no authentication,
encryption, discovery, DNS, LAN/internet binding, firewall modification, overlay,
relay, host migration or remote input authorization. Session/security and friend
reachability review are required before any non-loopback transport is enabled.

## Wire format, version 1

All integer fields use unsigned network byte order. Byte offsets below are within
each header/body; no native C struct is transmitted.

The 12-byte header is `OOC1` at 0..3, kind at 4, three reserved zero bytes at 5..7,
then body byte length (u32) at 8..11. The header version marker and the negotiated
application protocol must both match. A body never exceeds 4116 bytes; payload
capacity is 4096. Invalid magic, reserved bytes, lengths, direction, player ID,
epoch or zero sequence closes the peer with `NET_ERROR`. Unknown kinds also close.
EOF on a partial frame or before READY is `NET_ERROR`; empty frame-boundary EOF
after READY is `NET_CLOSED`.

| Kind | Direction | Body |
| --- | --- | --- |
| 1 HELLO | Guest to host, before READY | 84 bytes: protocol u32 at 0; zero epoch u64 at 4; build[32] at 12; ROM hash[32] at 44; capability u32 at 76; P2 byte at 80; three zero bytes at 81 |
| 5 WELCOME | Host to guest, before READY | Same 84 bytes, with actual host epoch instead of zero; P2 is the assigned guest slot |
| 2 INPUT | Guest P2 to host | epoch u64 at 0; sequence u64 at 8; P2 byte at 16; three zeros at 17; opaque bytes at 20 |
| 3 VIEW | Host P1 to guest | Same metadata, with P1 at 16; opaque view bytes at 20 |
| 4 EVENT | Host P1 to guest | Same metadata, with P1 at 16; opaque event bytes at 20 |

The application protocol, exact build tag, exact ROM hash and the two-player bit
must match both directions. Other capability bits are reserved for future adapter
use and currently ignored. The host may queue WELCOME before its application sees
READY; callers should retry a busy output slot after pumping.

There is no raw GB/RAM merge and no shared-world interpretation. Payloads are
opaque to leave snapshot, event and rendering decisions to the native room proof.
No simulated application-message faults or claims of tested TCP packet loss,
latency/jitter, blackout recovery, real game actions, or campaign coverage are made.
