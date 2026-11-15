# Step 6 — Gate 5: transport between two machines

This file belongs to one agent step. Do not edit `NOTES.md`, `README.md`, `PLAN.md`, `prompts/`
or `data/` — the integrator folds this file into them once no other agent is running
(see `../PARALLEL.md`).

- Prompt: `prompts/06-transport-and-relay.md`
- Agent: repository-hygiene subagent (backfill), 2026-11-15
- Branch / worktree: **the shared branch `game/south-park-the-stick-of-truth`, in the main
  checkout — not a step-06 worktree.** There is no `sp-step06` worktree and no
  `game/south-park-the-stick-of-truth-step06-transport` branch. The transport was written on the
  shared branch in commit `61fb97d`, before this backfill, by the lead session. This file records
  that; it does not move the code.
- Launch lock held: none — no game launch. The only run below is a loopback self-test with no game.

## Observed (ran and saw)

- Code: `mod/src/net.h`, `mod/src/net.cpp`, `mod/src/net_selftest.cpp` on the shared branch, commit
  `61fb97d` ("South Park mod: UDP snapshot transport (direct address, 20 Hz, newer-wins) with no
  game code in it yet"). `git branch --contains 61fb97d` lists only the shared branch and branches
  cut from it; no step-06 branch contains it.
- One UDP socket does both roles. `Net::Listen(port)` binds `INADDR_ANY:port`; `Net::Connect(host,
  port)` binds port 0 and sends to the peer; the socket is non-blocking (`ioctlsocket(FIONBIO)`).
- Snapshot packet: magic `PSC1`, sequence, `x`/`y`/`z` floats, `yaw`, `animation`, `body_id`;
  32 bytes. `Publish` sends at `kSendHz = 20` (one packet per 50 ms) using a caller-supplied
  millisecond clock.
- Ordering: `Poll` accepts a packet only when `sequence > sequence_peer_` (newer wins). There is no
  acknowledgement, retransmission or reliability layer.
- Peer address: a guest that is not yet `connected_` takes the sender's address and port from the
  first valid packet and starts sending there. `net.h` states this is deliberate.
- `PeerAgeMs` returns the time since the last accepted packet; `SentCount` and `ReceivedCount` are
  exposed through the bridge command `net` as `sent=… received=…`.
- The bridge starts it: `exec "host <port>"` calls `Listen`, `exec "join <host> <port>"` calls
  `Connect` (`mod/src/mod.cpp`, `Exec`). The mod tick calls `Poll` then `Publish` on every
  iteration (`mod/src/mod.cpp`, worker loop).
- Self-test: `net_selftest.cpp` starts two endpoints on loopback and runs 7 checks (host listens,
  guest connects, first snapshot arrives, position matches, yaw and ids match, newer snapshot wins,
  peer age grows). `NOTES.md` records the built `net_selftest.exe` as **7/7 checks, `ALL OK`**.
  This backfill did not rebuild or re-run it.

## Derived (concluded from evidence, with the evidence named)

- The transport is a snapshot channel, not a session: it has no handshake, no build or version
  check, no join/leave message, no level identity and no ownership. Evidence: the packet struct and
  the `Net` API contain none of those fields. Confidence: high for the code; what a mixed build
  does is unknown.
- It is not a relay and does not traverse NAT. It sends UDP to one address and learns the peer from
  the first packet, which is why the first release assumes a VPN with a direct address. Evidence:
  the `net.h` header comment and `sendto`/`recvfrom` in `net.cpp`. Confidence: high.
- The 20 Hz / newer-wins choice fits a position stream whose late packets are worthless; it does
  not fit anything that must not be lost (spawn, join, level change), which the `net.h` comment
  itself marks as future work. Confidence: high.

## Unknown

- Whether two real game clients exchange snapshots at all: the only test is loopback between two
  `Net` objects, not two game processes. Needs two clients under the launch lock.
- Whether a VPN (ZeroTier, Radmin VPN or Tailscale) carries this between two PCs, and whether
  Windows Firewall prompts or blocks the UDP port. No two-machine run exists.
- What happens on packet loss, reorder, duplication, jitter or a changed peer address. No chaos
  pass (`playbook/network-chaos-testing.md`); the only loss case is the self-test's "peer age grows".
- Mixed builds: there is no version or handshake field, so the prompt's "refuse mixed builds"
  requirement is not met.
- Whether the peer body is drawn at the received position in a real session: `remote.*`
  interpolation is separate (26/26 self-test recorded in `NOTES.md`), and the two have not been
  shown together in a game.
- `NETWORK.md` and `tools/relay.py` do not exist. The transport was built in `mod/` instead, on the
  shared branch, so the step-6 deliverables in `prompts/06-transport-and-relay.md` are only partly
  met.

## Deliverable

- Files that exist: `mod/src/net.h`, `mod/src/net.cpp`, `mod/src/net_selftest.cpp`; CMake targets
  `net_selftest` and `remote_selftest`; bridge commands `host`, `join` and `net` in
  `mod/src/mod.cpp`.
- Reproduce the self-test (from `mod/`): `cmake -G "MinGW Makefiles" -B build`, then
  `mingw32-make -C build`, then run `build\net_selftest.exe`; expected last line `ALL OK`.
- Not delivered: `NETWORK.md`, `tools/relay.py`, a two-machine run, a chaos pass, a handshake.

## Handoff notes for the integrator

- `README.md` / `NOTES.md`: "Transport exists on the shared branch (`mod/src/net.*`): UDP
  snapshots at 20 Hz, newer-wins by sequence, peer learned from the first packet, self-test 7/7
  `ALL OK`. No two-machine run, no handshake and no chaos pass yet."
- `PLAN.md` step 6 stays "waits on 5"; do not mark Gate 5 done from the self-test.
- The file name says "transport-and-relay"; there is no relay. Say "transport" in the write-up.
