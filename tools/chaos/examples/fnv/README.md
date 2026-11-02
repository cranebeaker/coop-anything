# Example: the chaos box on a Fallout: New Vegas co-op project

This box was written for [fnv-coop](https://github.com/cranebeaker/fnv-coop), a shared-story co-op
mod for Fallout: New Vegas. This page lists the parts that were specific to that project, so you
can see how a game plugs in. Nothing here is needed to use the box with another game.

## How it plugged in

- The mod's session layer talks over ENet (UDP) in a star: one host, up to a few clients. The relay
  sits between a client and the host exactly as in the main README: the host listens on its port, the
  relay listens on another, and the client is told to connect to the relay's port.
- The mod's game-free test simulator has its own simulated link. The simulator's link parameters
  had latency range, loss, duplicate and corrupt only. The mapping used to give the simulator a
  profile from `profiles.json`:

  | Simulator field | From the profile |
  |---|---|
  | minimum latency | `latency_ms - jitter_ms` (not below 0) |
  | maximum latency | `latency_ms + jitter_ms` |
  | loss, duplicate, corrupt | same names |

  Burst loss, reorder and bandwidth caps were not in that link type. That is why the header-only
  C++ mirror (`cpp/impair.h`) exists: a simulator can call `Impairment::decide` for every
  datagram and get all of them, with the same results as the real relay for the same seed.
- Test scripts for two game copies are meant to start the relay with `--info-file`, read the control port
  from that file with `client.py`, and change the profile or cut one direction at chosen steps of a
  scenario (for example `client.partition(info, "s2c", seconds=3)` in the middle of a quest sync
  check, then assert that both copies converge after the outage heals).
- The packet log format (`head`, `len`, order) was designed so it can be lined up with the mod's own
  flight recorder traces, by matching bytes and order rather than frame numbers.

## What the session layer had to survive

The profiles were chosen to hit the failure modes of a story-sync protocol:

| Failure | Profile or call | What it checks |
|---|---|---|
| Lost updates | `bad_wifi`, `hell` | Retransmit and ack of the effect log. |
| Out of order updates | `bad_wifi` (reorder 2 percent), `hell` (10 percent) | Gaps are buffered, applied in order. |
| Duplicated updates | `bad_wifi`, `hell` | Nothing applies twice (no duplicate rewards). |
| Slow link with a small pipe | `mobile_tether`, `hell` | Catch-up after a gap without flooding the pipe. |
| Host to client blackout | `partition {"dir":"s2c","on":true,"duration_ms":N}` | Client falls behind, then catches up. |
| Client to host blackout | `partition {"dir":"c2s", ...}` | Client requests are durable and retried, executed once. |
| Both directions down | `disconnect {"duration_ms":N}` | Reconnect and resume, or rebase. |

## What is not covered

- Real-game behaviour through the relay (ENet connect timing, timeouts with a given profile) needs a run
  with two game copies. The box does not launch games; that belongs in the game's own test runner.
- The relay and the simulator share the impairment model but not the clock: the relay uses wall time,
  the simulator uses virtual time.
