# Chaos box: a bad network between two copies of any game

A small tool that sits between a host and a client of any UDP game or mod and makes the
link behave like a bad one: delay, jitter, loss, bursts of loss, reordering, duplicates,
bit flips, a bandwidth cap, one-way outages and full disconnects. You change the profile
while the game runs, from a script or from the command line.

The same impairment model also exists as a header-only C++ file, so a game-free
simulator or fuzzer can use the exact same network. A cross-check proves that the Python
and the C++ versions give identical results for the same seed.

Why you want it: co-op sync that works on a LAN often breaks on real internet. Loss
drops an update, reordering applies two updates in the wrong order, a duplicate applies
one twice, a long outage leaves a client behind. Test for these on purpose, with the same
seed every time, so a failure can be repeated and fixed.

Python 3.8 or newer, standard library only. The C++ mirror needs cmake and a C++17 compiler
(optional, only for the cross-check).

## Quick start: put the relay between a host and a client

The relay listens on a UDP port and forwards every datagram to the real server through
the impairment model. Each client address gets its own upstream socket, so the host sees
separate peers, like a NAT would show.

1. Start your game host as usual. Say it listens on UDP port 27970 on this PC.
2. Start the relay in front of it:

   ```
   python -I tools/chaos/chaos.py relay --target 127.0.0.1:27970 --profile bad_wifi --log out/chaos/run.jsonl
   ```

   It prints one line, for example `{"listen": 53211, "rpc": 53212, "log": "out/chaos/run.jsonl"}`.
   `listen` is the port the client must connect to. `rpc` is the control port (section below).
3. Point the client at `127.0.0.1:<listen>` instead of the host's port. The client now
   talks to the host through the bad network.
4. Change the network while the game runs:

   ```
   python -I tools/chaos/chaos.py call --rpc-port 53212 set_profile '{"profile":"hell"}'
   python -I tools/chaos/chaos.py call --rpc-port 53212 partition '{"dir":"s2c","on":true,"duration_ms":4000}'
   python -I tools/chaos/chaos.py call --rpc-port 53212 stats
   ```

Options for `relay`:

| Option | Meaning |
|---|---|
| `--target host:port` | The real server. Required. Can be another PC; the relay then sends from any local interface. |
| `--listen N` | UDP port for clients (0 picks a free one, the default). |
| `--bind ADDR` | Address to listen on. Default `127.0.0.1`. Use `0.0.0.0` so other PCs can connect to the relay. |
| `--profile NAME` | Starting profile, default `lan`. |
| `--seed N` | Override the profile seed, to get a different but repeatable sequence. |
| `--rpc-port N` | Control port (0 picks one). The control port always stays on loopback. |
| `--log FILE` | Write a packet log (JSON Lines, format below). |
| `--info-file FILE` | Write `{listen, rpc, log}` as JSON once the relay is up. Handy for scripts. |
| `--seconds N` | Stop after N seconds. Default: run until the `shutdown` call or Ctrl+C. |

Two things to know:

- The relay is one hop on one PC. Latency you ask for is added on top of the real latency
  of the machine, and it is the one-way delay for each direction, so a profile with 35 ms
  adds about 70 ms round trip.
- A client that changes its source port looks like a new peer, as with any NAT.

If the game cannot be pointed at a different port (the port is fixed in the game), run the
host on another port and let the relay listen on the port the client expects, or use a
system level tool such as [clumsy](https://github.com/jagt/clumsy) (Windows, WinDivert based)
for the first quick experiments and come back to this relay when you need repeatable seeds,
live control from a script and a packet log.

## Profiles

All delays are one way, per direction. `loss` is the base loss. `burst_*` drive a
Gilbert-Elliott two-state model (the link flips between a good and a bad state, and loss is
much higher in the bad state, so drops come in clusters like real wifi and mobile links).
The table is `profiles.json`, the single source of truth for both languages.

| Profile | Latency +/- jitter | Base loss | Bursts | Reorder | Duplicate | Bandwidth cap | Use it for |
|---|---|---|---|---|---|---|---|
| `lan` | 0.5 +/- 0.2 ms | 0 | none | 0 | 0 | none | Baseline. Nearly perfect. |
| `good_wifi` | 12 +/- 4 ms | 0.5 % | none | 0 | 0 | none | Normal home wifi. |
| `bad_wifi` | 35 +/- 25 ms | 2 % | enter 1 %, exit 25 %, 60 % loss while bad | 2 % | 0.5 % | none | A busy or distant wifi. The workhorse for sync bugs. |
| `mobile_tether` | 70 +/- 40 ms | 1 % | enter 1 %, exit 20 %, 50 % loss while bad | 1 % | 0 | 2 Mbit/s, 300 ms queue | A phone hotspot. |
| `transatlantic` | 90 +/- 8 ms | 0.3 % | none | 0 | 0 | none | Players on two continents. Clean but slow. |
| `hell` | 200 +/- 150 ms | 8 % | enter 3 %, exit 10 %, 90 % loss while bad | 10 % | 5 % | 512 kbit/s, 500 ms queue | Stress. If your sync survives this, it is solid. |

Every profile has a fixed seed. Run the same profile twice with the same packet sequence
and you get the same drops and delays. Directions use separate random streams, labelled
`c2s` (client to server) and `s2c` (server to client).

Parameters you can override with `set_params`:

| Key | Meaning |
|---|---|
| `latency_ms`, `jitter_ms` | Delay is latency plus or minus jitter, uniform, never below 0. |
| `loss` | Chance to drop a packet in the good state. |
| `burst_enter`, `burst_exit`, `burst_loss` | Per packet chance good to bad, bad to good, and the loss chance while bad. |
| `reorder` | Chance a packet skips the latency and overtakes earlier ones. |
| `duplicate` | Chance a packet is delivered twice. |
| `corrupt` | Chance one bit of the packet is flipped. |
| `bandwidth_bps`, `buffer_ms` | Rate cap with a tail-drop queue. 0 means unlimited. |
| `seed` | Random seed. |

Besides the profile, you can switch these on and off at any time: a one-way partition, and
a timed disconnect of both directions.

## Live control (JSON-RPC)

The relay serves newline-delimited JSON-RPC 2.0 over TCP on the `rpc` port (loopback only).
From a shell use `chaos.py call --rpc-port P <method> '<json params>'`. From Python:

```python
import sys
sys.path.insert(0, "tools/chaos")
import client                      # thin functions, the rpc argument is a port, "host:port" or the --info-file path
client.set_profile("out/info.json", "bad_wifi")
client.partition("out/info.json", "s2c", seconds=3)   # host to client blackout for 3 s, then it heals itself
client.disconnect("out/info.json", 5)                  # both directions down for 5 s
print(client.stats("out/info.json"))
```

or with the lower level `relay.ChaosClient(port).call(method, **params)`.

| Method | Params | Effect |
|---|---|---|
| `status` | | Target, profile, client count, per direction params, partition and down flags. |
| `stats`, `reset_stats` | | Per direction counters: seen, forwarded, dropped, duplicated, bytes, drop reasons. |
| `profiles` | | The profile table. |
| `set_profile` | `profile` (or `name`), `seed?`, `dir?` | Switch profile. `dir` is `c2s`, `s2c` or both. Reseeds that direction. |
| `set_params` | `params`, `dir?` | Change single values without reseeding. |
| `partition` | `dir`, `on`, `duration_ms?` | One way or two way blackout. With `duration_ms` it heals itself. |
| `disconnect` | `duration_ms` | Drop everything in both directions for a while. |
| `reconnect` | | End a disconnect early. |
| `log_path` | | Path of the packet log. |
| `shutdown` | | Ask the relay process to exit. |

### Packet log

JSON Lines. The first line is a header: `{"format":"coopchaos-packetlog","version":1,"profile","seed","start_wall"}`.
Then one line per datagram: `t` (ms on the relay clock), `dir`, `peer`, `n` (per direction
counter), `len`, `head` (first 16 bytes as hex), `verdict` (`fwd` or `drop`), `reason`
(`loss`, `burst`, `partition`, `disconnect`, `bandwidth`), `copies` (delivery delays in ms, two
entries means a duplicate), `corrupt`. Event lines carry an `event` field (`profile`, `params`,
`partition`, `disconnect`, `reconnect`). To line a log up with your own game traces, match
on `head`, `len` and order, not on frame numbers.

## Use the same network in a simulator (C++)

`cpp/impair.h` is header-only, no sockets and no clocks, virtual time:

```cpp
#include "impair.h"
auto p = coopchaos::profile("bad_wifi");
coopchaos::Impairment c2s(p, "c2s"), s2c(p, "s2c");   // one per direction, same labels as the relay
auto v = c2s.decide(nowMs, sizeBytes);                // v.dropped(), v.reason, v.delaysMs, v.corrupt
for (double d : v.delaysMs) deliverAt(nowMs + d);     // two entries = duplicate
c2s.partition(true); c2s.disconnectUntil(nowMs + 3000); c2s.reconnect();
```

`cpp/profiles_gen.h` is generated from `profiles.json` by `gen_header.py --write`. The tests fail
if it is stale. Both languages use the same SplitMix64 random stream, seeded from an FNV-1a hash of
`"<seed>:<label>"`, and the same call order and arithmetic, so a profile, a seed and a packet
sequence give identical outcomes. Keep the two files in step: change `impair.py`, mirror it in
`impair.h`, and the cross-check tells you if they drifted.

## How it is tested

One command, from the repository root:

```
python -I tools/chaos/run_tests.py
```

It prints `ALL OK` when everything passes and every deliberately broken copy is caught. It takes
about three minutes the first time (the C++ part is built once) and about two after that. It runs:

1. **Python tests** (`test_chaos.py`, 20 tests). Model statistics in virtual time for every profile
   (loss rate within tolerance of the theory, mean latency, burst clustering, reorder and duplicate
   rates, the bandwidth cap, one-way partition, timed disconnect, determinism per seed, independent
   directions). Then real UDP round trips on loopback through the relay (profiles, mid-run changes,
   packet log, two clients seen as two peers), the RPC client module, and the command line.
2. **Python mutation twins** (11). The `COOPCHAOS_MUTANT` environment variable breaks one behaviour
   on purpose (no loss, no bursts, half the latency, no reorder, no duplicates, no bandwidth cap, no
   partition, and a client helper that always partitions both directions). A named test must fail for
   each one. A test suite that cannot tell a broken box from a working one is worthless.
3. **C++ cross-check.** `run_tests.py` builds `cpp/` with cmake, one compile job at a time to keep memory
   low, runs about 36,000 decisions through the C++ mirror and the Python model (6 profiles, 2 directions,
   with corruption, partition, disconnect, bandwidth, duplicate and reorder changes in the middle of the run)
   and requires identical results. Then 4 mutant builds of the C++ header (`cpp/CMakeLists.txt`) and a stale
   generated header must each be caught.
   If cmake or a C++ compiler is missing, the script prints a `SKIP:` line with the reason and the final line
   says the cross-check was skipped. It never claims it ran when it did not.

Flags: `--no-twins` runs only the Python tests, `--no-cpp` skips the C++ part.
A `CMAKE_TOOLCHAIN_FILE` left in your environment for another target (for example an Android NDK) is ignored for
this build. The build directory is made under the system temp folder, not in the repository.

## Files

| File | What |
|---|---|
| `impair.py` | The impairment model. Pure, seeded, virtual time. |
| `relay.py` | UDP relay, JSON-RPC server, `ChaosClient`, packet log. |
| `chaos.py` | Command line: `profiles`, `relay`, `call`. |
| `client.py` | Thin functions for test scripts. |
| `profiles.json` | The profile table, source of truth. |
| `gen_header.py` | Generates `cpp/profiles_gen.h` from `profiles.json`. |
| `xcheck.py`, `cpp/` | The Python versus C++ cross-check and the C++ mirror. |
| `test_chaos.py`, `run_tests.py` | Tests and the twin runner. |
| `examples/fnv/` | How the box was used on a Fallout: New Vegas co-op project. |

## Limits

- The relay is written in Python. Tests use under 1000 packets per second. Its throughput was not
  benchmarked beyond that; for a game that sends more, check the numbers in `stats`.
- On Windows the relay asks for a 1 ms timer while it runs, so delayed packets are not rounded up
  to the default 15.6 ms tick. The added error is covered by the test tolerance (mean round trip within
  14 ms plus 12 percent, LAN round trip under 15 ms), and was not measured more finely.
- UDP only. A TCP game needs a different tool.
- It sees whole datagrams. If your game encrypts or signs packets, `corrupt` makes the receiver drop
  them, which is itself a useful test.

## Credits and licence

MIT, like the rest of this repository. The ideas come from public sources: the networking articles on
Gaffer on Games, the Gilbert-Elliott two-state loss model, and tools such as clumsy and Linux netem that do the same
job at system level. No code was taken from them.

Ported from fnv-coop commit f57aa78 (https://github.com/cranebeaker/fnv-coop).
